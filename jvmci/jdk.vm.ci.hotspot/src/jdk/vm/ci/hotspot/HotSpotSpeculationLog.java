/*
 * Copyright (c) 2014, 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
package jdk.vm.ci.hotspot;

import static jdk.vm.ci.hotspot.CompilerToVM.compilerToVM;

import java.lang.ref.ReferenceQueue;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import jdk.vm.ci.meta.JavaConstant;
import jdk.vm.ci.meta.SpeculationLog;

/**
 * Implements a {@link SpeculationLog} that can be used to:
 * <ul>
 * <li>Query failed speculations recorded in a native linked list of {@code FailedSpeculation}s (see
 * methodData.hpp).</li>
 * <li>Make speculations during compilation and record them in compiled code. This must only be done
 * on compilation-local {@link HotSpotSpeculationLog} objects.</li>
 * </ul>
 *
 * The choice of constructor determines whether the native failed speculations list is
 * {@linkplain #managesFailedSpeculations() managed} by a {@link HotSpotSpeculationLog} object.
 */
public class HotSpotSpeculationLog implements SpeculationLog {

    private static final byte[][] NO_SPECULATIONS = {};
    private static final byte[] NO_FLATTENED_SPECULATIONS = {};

    /**
     * Creates a speculation log that manages a failed speculation list. That is, when this object
     * dies, the native resources of the list are freed.
     *
     * @see #managesFailedSpeculations()
     * @see #getFailedSpeculationsAddress()
     */
    public HotSpotSpeculationLog() {
        failedSpeculationsAddress = UnsafeAccess.UNSAFE.allocateMemory(HotSpotJVMCIRuntime.getHostWordKind().getByteCount());
        Cleaner c = new Cleaner(this, failedSpeculationsAddress);
        assert c.address == failedSpeculationsAddress;
        failedSpeculations = NO_SPECULATIONS;
        managesFailedSpeculations = true;
    }

    /**
     * Creates a speculation log that reads from an externally managed failed speculation list. That
     * is, the lifetime of the list is independent of this object.
     *
     * @param failedSpeculationsAddress an address in native memory at which the pointer to the
     *            externally managed sailed speculation list resides
     */
    public HotSpotSpeculationLog(long failedSpeculationsAddress) {
        if (failedSpeculationsAddress == 0) {
            throw new IllegalArgumentException("failedSpeculationsAddress cannot be 0");
        }
        this.failedSpeculationsAddress = failedSpeculationsAddress;
        if (UnsafeAccess.UNSAFE.getLong(failedSpeculationsAddress) == 0) {
            failedSpeculations = new byte[0][];
        } else {
            failedSpeculations = compilerToVM().getFailedSpeculations(failedSpeculationsAddress);
        }
        managesFailedSpeculations = false;
    }

    /**
     * Gets the address of the pointer to the native failed speculations list.
     *
     * @see #managesFailedSpeculations()
     */
    public long getFailedSpeculationsAddress() {
        return failedSpeculationsAddress;
    }

    /**
     * Returns {@code true} if the value returned by {@link #getFailedSpeculationsAddress()} is only
     * valid only as long as this object is alive, {@code false} otherwise.
     */
    public boolean managesFailedSpeculations() {
        return managesFailedSpeculations;
    }

    static final class HotSpotSpeculation extends Speculation {

        /**
         * A speculation id is a long encoding an offset (high 32 bits) and a length (low 32 bts).
         * Combined, the index and length denote where the
         * {@linkplain HotSpotSpeculationEncoding#toByteArray() encoded speculation} is in a
         * {@linkplain HotSpotSpeculationLog#getFlattenedSpeculations() flattened} speculations
         * array.
         */
        private JavaConstant offsetAndLength;

        HotSpotSpeculation(SpeculationReason reason, JavaConstant offsetAndLength) {
            super(reason);
            this.offsetAndLength = offsetAndLength;
        }

        JavaConstant getOffsetAndLength() {
            return offsetAndLength;
        }
    }

    /**
     * Address of a pointer to a set of failed speculations. The address is recorded in the nmethod
     * compiled with this speculation log such that when it fails a speculation, the speculation is
     * added to the list.
     */
    private final long failedSpeculationsAddress;

    private final boolean managesFailedSpeculations;

    /**
     * The list of failed speculations (i.e. cause a deoptimization) that were read from native
     * memory with {@link CompilerToVM#getFailedSpeculations(long)} when this log was initialized.
     */
    private final byte[][] failedSpeculations;

    /**
     * Speculations made during the compilation associated with this log.
     */
    private List<byte[]> speculations;
    private List<SpeculationReason> speculationReasons;

    @Override
    public void collectFailedSpeculations() {
    }

    byte[] getFlattenedSpeculations() {
        if (speculations == null) {
            return NO_FLATTENED_SPECULATIONS;
        }
        int size = 0;
        for (byte[] s : speculations) {
            size += s.length;
        }
        byte[] result = new byte[size];
        size = 0;
        for (byte[] s : speculations) {
            System.arraycopy(s, 0, result, size, s.length);
            size += s.length;
        }
        return result;
    }

    @Override
    public boolean maySpeculate(SpeculationReason reason) {
        if (failedSpeculations != null) {
            byte[] encoding = encode(reason);
            for (byte[] fs : failedSpeculations) {
                if (Arrays.equals(fs, encoding)) {
                    return false;
                }
            }
        }
        return true;
    }

    private static long encodeIndexAndLength(int index, int length) {
        return ((long) index) << 32 | length;
    }

    private static int decodeIndex(long indexAndLength) {
        return (int) (indexAndLength >>> 32);
    }

    @Override
    public Speculation speculate(SpeculationReason reason) {
        byte[] encoding = encode(reason);
        JavaConstant id;
        if (speculations == null) {
            speculations = new ArrayList<>();
            id = JavaConstant.forLong(encodeIndexAndLength(0, encoding.length));
            speculations.add(encoding);
        } else {
            id = null;
            int index = 0;
            for (byte[] fs : speculations) {
                if (Arrays.equals(fs, encoding)) {
                    id = JavaConstant.forLong(encodeIndexAndLength(index, fs.length));
                    break;
                }
                index++;
            }
            if (id == null) {
                id = JavaConstant.forLong(encodeIndexAndLength(index, encoding.length));
                speculations.add(encoding);
            }
        }

        return new HotSpotSpeculation(reason, id);
    }

    private static byte[] encode(SpeculationReason reason) {
        HotSpotSpeculationEncoding encoding = (HotSpotSpeculationEncoding) reason.encode(HotSpotSpeculationEncoding::new);
        byte[] result = encoding == null ? null : encoding.toByteArray();
        if (result == null) {
            throw new IllegalArgumentException(HotSpotSpeculationLog.class.getName() + " expects " + reason.getClass().getName() + ".encode() to return a non-empty encoding");
        }
        return result;
    }

    @Override
    public boolean hasSpeculations() {
        return speculations != null;
    }

    @Override
    public Speculation lookupSpeculation(JavaConstant constant) {
        if (constant.isDefaultForKind()) {
            return NO_SPECULATION;
        }
        int index = decodeIndex(constant.asLong());
        if (index >= 0 && index < speculationReasons.size()) {
            SpeculationReason reason = speculationReasons.get(index);
            return new HotSpotSpeculation(reason, constant);
        }
        throw new IllegalArgumentException("Unknown encoded speculation: " + constant);
    }

    /**
     * Frees the native memory resources associated with {@link HotSpotSpeculationLog}s once they
     * become reclaimable.
     */
    private static final class Cleaner extends WeakReference<HotSpotSpeculationLog> {

        private static final ReferenceQueue<HotSpotSpeculationLog> queue = new ReferenceQueue<>();

        Cleaner(HotSpotSpeculationLog referent, long address) {
            super(referent, cleanedQueue());
            this.address = address;
        }

        /**
         * Frees resources with all {@link HotSpotSpeculationLog}s that have been reclaimed.
         */
        private static ReferenceQueue<HotSpotSpeculationLog> cleanedQueue() {
            Cleaner c = (Cleaner) queue.poll();
            while (c != null) {
                compilerToVM().releaseFailedSpeculations(c.address);
                UnsafeAccess.UNSAFE.freeMemory(c.address);
                c = (Cleaner) queue.poll();
            }
            return queue;
        }

        final long address;
    }
}
