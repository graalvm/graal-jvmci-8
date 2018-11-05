/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

import static jdk.vm.ci.hotspot.HotSpotJVMCIRuntime.runtime;
import static jdk.vm.ci.hotspot.UnsafeAccess.UNSAFE;
import static jdk.vm.ci.services.Services.IS_IN_NATIVE_IMAGE;

import java.lang.ref.ReferenceQueue;
import java.lang.ref.WeakReference;
import java.util.HashMap;

import jdk.vm.ci.common.JVMCIError;
import jdk.vm.ci.common.NativeImageReinitialize;
import jdk.vm.ci.meta.JavaKind;
import jdk.vm.ci.meta.ResolvedJavaType;

/**
 * This class manages the set of metadata roots that must be scanned during garbage collection.
 * Because of class redefinition Method* and ConstantPool* can be freed if they don't appear to be
 * in use so they must be tracked when there are live references to them from Java.
 *
 * The general theory of operation is that all {@link MetaspaceHandleObject}s are created by calling
 * into the VM which calls back out to actually create the wrapper instance. During the call the VM
 * keeps the metadata reference alive through the use of metadata handles. Once the call completes
 * the wrapper object is registered here and will be scanned during metadata scanning. The weakness
 * of the reference to the wrapper object allows them to be reclaimed when they are no longer used.
 *
 */
class HotSpotJVMCIMetaAccessContext {

    /**
     * This is like {@link sun.misc.Cleaner} but with weak semantics instead of phantom. Objects
     * referenced by this might be referenced by {@link ResolvedJavaType} which is kept alive by a
     * {@link WeakReference} so we need equivalent reference strength.
     */
    static final class ReferenceCleaner extends WeakReference<Object> {

        @NativeImageReinitialize private static ReferenceCleaner first = null;

        private ReferenceCleaner next = null;
        private ReferenceCleaner prev = null;

        private final long handle;

        private static synchronized ReferenceCleaner add(ReferenceCleaner cl) {
            if (first != null) {
                cl.next = first;
                first.prev = cl;
            }
            first = cl;
            return cl;
        }

        private static synchronized boolean remove(ReferenceCleaner cl) {
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

        ReferenceCleaner(Object o, long handle) {
            super(o, queue);
            this.handle = handle;
        }

        void clearHandle() {
            remove(this);
            long value = UNSAFE.getLong(null, handle);
            UNSAFE.compareAndSwapLong(null, handle, value, 0);
        }

        /**
         * Periodically trim the list of tracked metadata. A new list is created to replace the old
         * to avoid concurrent scanning issues.
         */
        private static synchronized void clean() {
            ReferenceCleaner ref = (ReferenceCleaner) queue.poll();
            if (ref == null) {
                return;
            }
            while (ref != null) {
                ref.clearHandle();
                ref = (ReferenceCleaner) queue.poll();
            }
        }

        /**
         * The {@link ReferenceQueue} tracking the weak references created by this context.
         */
        private static final ReferenceQueue<Object> queue = new ReferenceQueue<>();

        private static void create(Object object, long handle) {
            clean();
            add(new ReferenceCleaner(object, handle));
        }

    }

    HotSpotJVMCIMetaAccessContext() {
    }

    /**
     * Add a {@link MetaspaceHandleObject} to be tracked by the GC. It's assumed that the caller is
     * responsible for keeping the reference alive for the duration of the call. Once registration
     * is complete then the VM will ensure it's kept alive.
     *
     * @param metaspaceObject
     */

    void add(MetaspaceHandleObject metaspaceObject) {
        ReferenceCleaner.create(metaspaceObject, metaspaceObject.getMetadataHandle());
    }

    void add(IndirectHotSpotObjectConstantImpl constantObject) {
        ReferenceCleaner.create(constantObject, constantObject.objectHandle);
    }

    @NativeImageReinitialize private static HashMap<Long, WeakReference<ResolvedJavaType>> resolvedJavaTypes;

    /**
     * Gets the JVMCI mirror for a {@link Class} object.
     *
     * @return the {@link ResolvedJavaType} corresponding to {@code javaClass}
     */
    static HotSpotResolvedJavaType fromClass(Class<?> javaClass) {
        if (javaClass == null) {
            /*
             * If the referent has become null, clear out the current value and let computeValue
             * above create a new value. Reload the value in a loop because in theory the
             * WeakReference referent can be reclaimed at any point.
             */
            return null;
        }
        if (javaClass.isPrimitive()) {
            return HotSpotResolvedPrimitiveType.forKind(JavaKind.fromJavaClass(javaClass));
        }
        if (IS_IN_NATIVE_IMAGE) {
            try {
                return runtime().compilerToVm.lookupType(javaClass.getName().replace('.', '/'), null, true);
            } catch (ClassNotFoundException e) {
                throw new JVMCIError(e);
            }
        }
        return runtime().compilerToVm.lookupClass(javaClass);
    }

    synchronized HotSpotResolvedObjectTypeImpl fromMetaspace(long klassPointer, String signature) {
        if (resolvedJavaTypes == null) {
            resolvedJavaTypes = new HashMap<>();
        }
        assert klassPointer != 0;
        WeakReference<ResolvedJavaType> klassReference = resolvedJavaTypes.get(klassPointer);
        HotSpotResolvedObjectTypeImpl javaType = null;
        if (klassReference != null) {
            javaType = (HotSpotResolvedObjectTypeImpl) klassReference.get();
        }
        if (javaType == null) {
            javaType = new HotSpotResolvedObjectTypeImpl(klassPointer, signature);
            resolvedJavaTypes.put(klassPointer, new WeakReference<>(javaType));
        }
        return javaType;
    }
}
