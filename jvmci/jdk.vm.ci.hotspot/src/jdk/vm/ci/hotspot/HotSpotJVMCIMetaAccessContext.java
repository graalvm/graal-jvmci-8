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

final class HotSpotJVMCIMetaAccessContext {

    HotSpotJVMCIMetaAccessContext() {
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
