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
import static jdk.vm.ci.services.Services.IS_IN_NATIVE_IMAGE;

import java.lang.ref.WeakReference;
import java.util.HashMap;

import jdk.vm.ci.common.JVMCIError;
import jdk.vm.ci.common.NativeImageReinitialize;
import jdk.vm.ci.meta.JavaKind;
import jdk.vm.ci.meta.ResolvedJavaType;

/**
 * This class manages the set of {@code Metadata*} values that must be scanned during garbage
 * collection. Because of class redefinition Method* and ConstantPool* can be freed if they don't
 * appear to be in use so they must be tracked when there are live references to them from Java.
 *
 * The general theory of operation is that all {@link MetaspaceHandleObject}s are created by calling
 * into the VM which calls back out to actually create the wrapper instance. During the call the VM
 * keeps the metadata reference alive through the use of metadata handles. Once the call completes
 * the wrapper object is registered here and will be scanned during metadata scanning. The weakness
 * of the reference to the wrapper object allows them to be reclaimed when they are no longer used.
 *
 */
final class HotSpotJVMCIMetaAccessContext {

    HotSpotJVMCIMetaAccessContext() {
    }

    /**
     * Add a {@link MetaspaceHandleObject} to be tracked by the GC. It's assumed that the caller is
     * responsible for keeping the reference alive for the duration of the call. Once registration
     * is complete then the VM will ensure it's kept alive.
     *
     * @param metaspaceObject
     */

    static void add(MetaspaceHandleObject metaspaceObject) {
        HandleCleaner.create(metaspaceObject, metaspaceObject.getMetadataHandle());
    }

    static void add(IndirectHotSpotObjectConstantImpl constantObject) {
        HandleCleaner.create(constantObject, constantObject.objectHandle);
    }

    @NativeImageReinitialize private static HashMap<Long, WeakReference<ResolvedJavaType>> resolvedJavaTypes;

    static HotSpotResolvedJavaType createClass(Class<?> javaClass) {
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

    /**
     * Cache for speeding up {@link #fromClass(Class)}.
     */
    @NativeImageReinitialize private volatile ClassValue<WeakReference<HotSpotResolvedJavaType>> resolvedJavaType;

    private HotSpotResolvedJavaType fromClass0(Class<?> javaClass) {
        if (resolvedJavaType == null) {
            synchronized (this) {
                if (resolvedJavaType == null) {
                    resolvedJavaType = new ClassValue<WeakReference<HotSpotResolvedJavaType>>() {
                        @Override
                        protected WeakReference<HotSpotResolvedJavaType> computeValue(Class<?> type) {
                            return new WeakReference<>(createClass(type));
                        }
                    };
                }
            }
        }
        HotSpotResolvedJavaType javaType = null;
        while (javaType == null) {
            WeakReference<HotSpotResolvedJavaType> type = resolvedJavaType.get(javaClass);
            javaType = type.get();
            if (javaType == null) {
                /*
                 * If the referent has become null, clear out the current value and let computeValue
                 * above create a new value. Reload the value in a loop because in theory the
                 * WeakReference referent can be reclaimed at any point.
                 */
                resolvedJavaType.remove(javaClass);
            }
        }
        return javaType;
    }

    /**
     * Gets the JVMCI mirror for a {@link Class} object.
     *
     * @return the {@link ResolvedJavaType} corresponding to {@code javaClass}
     */
    static HotSpotResolvedJavaType fromClass(Class<?> javaClass) {
        if (javaClass == null) {
            return null;
        }
        return runtime().metaAccessContext.fromClass0(javaClass);
    }

    static synchronized HotSpotResolvedObjectTypeImpl fromMetaspace(long klassPointer, String signature) {
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
