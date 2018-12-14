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

    @NativeImageReinitialize private static HandleCleaner first = null;

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

    private void clearHandle() {
        remove(this);
        if (isJObject) {
            CompilerToVM.compilerToVM().deleteGlobalHandle(handle);
        } else {
            long value = UNSAFE.getLong(null, handle);
            UNSAFE.compareAndSwapLong(null, handle, value, 0);
        }
    }

    /**
     * Periodically trim the list of tracked metadata. A new list is created to replace the old to
     * avoid concurrent scanning issues.
     */
    private static synchronized void clean() {
        HandleCleaner ref = (HandleCleaner) queue.poll();
        if (ref == null) {
            return;
        }
        while (ref != null) {
            ref.clearHandle();
            ref = (HandleCleaner) queue.poll();
        }
    }

    /**
     * The {@link ReferenceQueue} tracking the weak references created by this context.
     */
    private static final ReferenceQueue<Object> queue = new ReferenceQueue<>();

    static void create(Object wrapper, long handle) {
        clean();
        assert wrapper instanceof IndirectHotSpotObjectConstantImpl || wrapper instanceof MetaspaceHandleObject;
        add(new HandleCleaner(wrapper, handle, wrapper instanceof IndirectHotSpotObjectConstantImpl));
    }

}
