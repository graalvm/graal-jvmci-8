/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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

import static jdk.vm.ci.hotspot.UnsafeAccess.UNSAFE;

import java.lang.ref.ReferenceQueue;
import java.lang.ref.WeakReference;

import jdk.vm.ci.common.NativeImageReinitialize;
import jdk.vm.ci.meta.ResolvedJavaType;

/**
 * This class manages a set of {@code jobject} and {@code jmetadata} handles whose lifetimes are
 * dependent on associated {@link IndirectHotSpotObjectConstantImpl} and
 * {@link MetaspaceHandleObject} wrapper objects respectively.
 *
 * The general theory of operation is that all wrappers are created by calling into the VM which
 * calls back out to actually create the wrapper instance. During the call the VM keeps the object
 * or metadata reference alive through the use of handles. Once the call completes the wrapper
 * object is registered here and will be scanned during metadata scanning. The weakness of the
 * reference to the wrapper object allows the handles to be reclaimed when they are no longer used.
 *
 * This is like {@link sun.misc.Cleaner} but with weak semantics instead of phantom. Objects
 * referenced by this might be referenced by {@link ResolvedJavaType} which is kept alive by a
 * {@link WeakReference} so we need equivalent reference strength.
 */
final class HandleCleaner extends WeakReference<Object> {

    /**
     * Head of linked list of cleaners.
     */
    @NativeImageReinitialize private static HandleCleaner first = null;

    /**
     * Linked list pointers.
     */
    private HandleCleaner next = null;
    private HandleCleaner prev = null;

    /**
     * A {@code jmetadata} or {@code jobject} handle.
     */
    private final long handle;

    /**
     * Specifies if {@link #handle} is a {@code jobject} or {@code jmetadata}.
     */
    private final boolean isJObject;

    private static synchronized HandleCleaner add(HandleCleaner cl) {
        if (first != null) {
            cl.next = first;
            first.prev = cl;
        }
        first = cl;
        return cl;
    }

    /**
     * Removes {@code cl} from the linked list of cleaners.
     */
    private static synchronized boolean remove(HandleCleaner cl) {
        // If already removed, do nothing
        if (cl.next == cl) {
            return false;
        }

        // Update list
        if (first == cl) {
            if (cl.next != null) {
                first = cl.next;
            } else {
                first = cl.prev;
            }
        }
        if (cl.next != null) {
            cl.next.prev = cl.prev;
        }
        if (cl.prev != null) {
            cl.prev.next = cl.next;
        }

        // Indicate removal by pointing the cleaner to itself
        cl.next = cl;
        cl.prev = cl;
        return true;
    }

    private HandleCleaner(Object wrapper, long handle, boolean isJObject) {
        super(wrapper, queue);
        this.handle = handle;
        this.isJObject = isJObject;
    }

    /**
     * Releases the resource associated with {@code this.handle}.
     */
    private void deleteHandle() {
        remove(this);
        if (isJObject) {
            // The sentinel value used to denote a free handle is
            // an object on the HotSpot heap so we call into the
            // VM to set the target of an object handle to this value.
            CompilerToVM.compilerToVM().deleteGlobalHandle(handle);
        } else {
            // Setting the target of a jmetadata handle to 0 enables
            // the handle to be reused. See MetadataHandleBlock in
            // jvmciRuntime.cpp for more info.
            long value = UNSAFE.getLong(null, handle);
            UNSAFE.compareAndSwapLong(null, handle, value, 0);
        }
    }

    /**
     * Cleans the handles who wrappers have been garbage collected.
     */
    private static void clean() {
        HandleCleaner ref = (HandleCleaner) queue.poll();
        while (ref != null) {
            ref.deleteHandle();
            ref = (HandleCleaner) queue.poll();
        }
    }

    /**
     * The {@link ReferenceQueue} to which handle wrappers are enqueued once they become
     * unreachable.
     */
    private static final ReferenceQueue<Object> queue = new ReferenceQueue<>();

    /**
     * Registers a cleaner for {@code handle}. The cleaner will release the handle some time after
     * {@code wrapper} is detected as unreachable by the garbage collector.
     */
    static void create(Object wrapper, long handle) {
        clean();
        assert wrapper instanceof IndirectHotSpotObjectConstantImpl || wrapper instanceof MetaspaceHandleObject;
        add(new HandleCleaner(wrapper, handle, wrapper instanceof IndirectHotSpotObjectConstantImpl));
    }
}
