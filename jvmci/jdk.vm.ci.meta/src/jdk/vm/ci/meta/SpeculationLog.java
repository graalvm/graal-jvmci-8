/*
 * Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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
package jdk.vm.ci.meta;

import java.util.Map;
import java.util.function.Supplier;

/**
 * Manages unique {@link SpeculationReason} objects that denote why a deoptimization occurred.
 * Reasons are embedded in compiled code for a method. If the compiled code deoptimizes at a
 * position associated with a {@link SpeculationReason}, the reason is added to a set of failed
 * speculations associated with the method. A subsequent compilation of the method can query the
 * failed speculations via a {@link SpeculationLog} to avoid making a speculation based on
 * invalidated reasons. This avoids repeated deoptimizations.
 *
 * All methods of this interface are called by the compiler. There is no need for API to register
 * failed speculations during deoptimization, since every VM has different needs there.
 */
public interface SpeculationLog {
    /**
     * The specific attributes of a speculation that a compiler uses to denote a speculation in a
     * compiled method. Typical attributes of a speculation can include a bytecode position, type
     * information about a variable being speculated, an enum denoting the type of operation to
     * which the speculation applies. A {@link SpeculationLog} a {@link SpeculationReason} can be
     * used as a key in a {@link Map} and so a {@link SpeculationReason} must implement
     * {@link Object#equals(Object)} and {@link Object#hashCode()} in terms of its attributes.
     *
     * A JVMCI implementation may want to serialize speculations for storage off heap (e.g. in
     * native memory associated with an nmethod). For this reason, the attributes of a
     * {@link SpeculationReason} are restricted to those supported by the {@code add...} methods of
     * {@link SpeculationReasonEncoding}.
     */
    public interface SpeculationReason {

        /**
         * Encodes the attributes of this reason using a {@link SpeculationReasonEncoding}. A
         * {@link SpeculationReason} implementation should cache the returned value and return it
         * for all subsequent calls to this method. Upon returning, the caller close the returned
         * {@link SpeculationReasonEncoding} object such that all subsequent calls to its
         * {@code add...} method will result in an error.
         *
         * @param encodingSupplier source of a {@link SpeculationReasonEncoding}. The same supplier
         *            will be passed to all invocations of this method.
         * @return a {@link SpeculationReasonEncoding} that encodes all the attributes that uniquely
         *         identify this reason
         */
        default SpeculationReasonEncoding encode(Supplier<SpeculationReasonEncoding> encodingSupplier) {
            return null;
        }
    }

    /**
     * Provides a facility for encoding the attributes of a {@link SpeculationReason}.
     * <p>
     * A {@link SpeculationReasonEncoding} object starts out empty and attribute data is added to it
     * using the {@code add...} methods.
     */
    public interface SpeculationReasonEncoding {
        void addMethod(ResolvedJavaMethod method);

        void addType(ResolvedJavaType type);

        void addInt(int value);

        void addLong(long value);

        void addEnum(Enum<?> e);

        void addString(String value);
    }

    /**
     * Marker class that indicates that a speculation has no reason.
     */
    final class NoSpeculationReason implements SpeculationReason {
    }

    class Speculation {
        private SpeculationReason reason;

        public Speculation(SpeculationReason reason) {
            this.reason = reason;
        }

        public SpeculationReason getReason() {
            return reason;
        }

        @Override
        public String toString() {
            return reason.toString();
        }

        @Override
        public boolean equals(Object obj) {
            if (obj instanceof Speculation) {
                Speculation other = (Speculation) obj;
                return reason.equals(other.reason);
            }
            return false;
        }

        @Override
        public int hashCode() {
            return getReason().hashCode();
        }
    }

    Speculation NO_SPECULATION = new Speculation(new NoSpeculationReason());

    /**
     * Must be called before compilation, i.e., before a compiler calls {@link #maySpeculate}.
     */
    void collectFailedSpeculations();

    /**
     * If this method returns true, the compiler is allowed to {@link #speculate} with the given
     * reason.
     */
    boolean maySpeculate(SpeculationReason reason);

    /**
     * Registers a speculation performed by the compiler. The compiler must guard every call to this
     * method for a specific reason with a call to {@link #maySpeculate(SpeculationReason)}.
     *
     * This API is subject to a benign race where a during the course of a compilation another
     * thread might fail a speculation such that {@link #maySpeculate(SpeculationReason)} will
     * return false but an earlier call returned true. This method will still return a working
     * {@link Speculation} in that case but the compile will eventually be invalidated and the
     * compile attempted again without the now invalid speculation.
     *
     * @param reason an object representing the reason for the speculation
     * @return a compiler constant encapsulating the provided reason. It is usually passed as an
     *         argument to the deoptimization function.
     */
    Speculation speculate(SpeculationReason reason);

    /**
     * Returns if this log has speculations.
     *
     * @return true if there are speculations, false otherwise
     */
    boolean hasSpeculations();

    /**
     * Given a {@link JavaConstant} previously returned from
     * {@link MetaAccessProvider#encodeSpeculation(Speculation)} return the original
     * {@link Speculation} object.
     */
    Speculation lookupSpeculation(JavaConstant constant);
}
