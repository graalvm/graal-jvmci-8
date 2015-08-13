/*
 * Copyright (c) 2015, 2015, Oracle and/or its affiliates. All rights reserved.
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
package jdk.internal.jvmci.meta;

import java.util.*;
import java.util.function.*;

/**
 * A per thread cache of {@link ResolvedJavaType}s. The per thread cache can be flushed by using
 * {@link #flush()} but there is no way to globally flush all caches.
 */
public class JVMCIThreadLocalMetaAccessContext implements JVMCIMetaAccessContext {

    protected final ThreadLocal<Map<Class<?>, ResolvedJavaType>> threadLocalMap = new ThreadLocal<Map<Class<?>, ResolvedJavaType>>() {
        @Override
        protected Map<Class<?>, ResolvedJavaType> initialValue() {
            return new HashMap<>();
        }
    };

    protected final Function<Class<?>, ResolvedJavaType> factory;

    public JVMCIThreadLocalMetaAccessContext(Function<Class<?>, ResolvedJavaType> factory) {
        this.factory = factory;
    }

    public ResolvedJavaType fromClass(Class<?> javaClass) {
        Map<Class<?>, ResolvedJavaType> map = threadLocalMap.get();
        ResolvedJavaType type = map.get(javaClass);
        if (type == null) {
            type = factory.apply(javaClass);
            map.put(javaClass, type);
        }
        return type;
    }

    /**
     * Drop all current cached state.
     */
    public boolean flush() {
        threadLocalMap.remove();
        return true;
    }
}
