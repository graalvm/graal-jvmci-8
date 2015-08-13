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

import java.util.function.*;

/**
 * Caches the {@link ResolvedJavaType} for a class in a {@link ClassValue}. The instances live
 * forever with no way to clean them up once they are no longer in use.
 */

public class JVMCIGlobalMetaAccessContext implements JVMCIMetaAccessContext {

    /**
     * JVMCI mirrors are stored as a {@link ClassValue} associated with the {@link Class} of the
     * type.
     */
    protected final ClassValue<ResolvedJavaType> jvmciMirrors = new ClassValue<ResolvedJavaType>() {
        @Override
        protected ResolvedJavaType computeValue(Class<?> javaClass) {
            return factory.apply(javaClass);
        }

    };
    protected final Function<Class<?>, ResolvedJavaType> factory;

    public JVMCIGlobalMetaAccessContext(Function<Class<?>, ResolvedJavaType> factory) {
        this.factory = factory;
    }

    public ResolvedJavaType fromClass(Class<?> javaClass) {
        return jvmciMirrors.get(javaClass);
    }

    public boolean flush() {
        // There is no mechanism to flush all data.
        return false;
    }
}
