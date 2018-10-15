/*
 * Copyright (c) 2012, 2012, Oracle and/or its affiliates. All rights reserved.
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

import java.lang.reflect.Field;

import sun.misc.Unsafe;

/**
 * Package private access to the {@link Unsafe} capability.
 */
class UnsafeAccess {

    static final UnsafeAccess UNSAFE;
    static final Unsafe PLATFORM_UNSAFE;

    static {
        Unsafe unsafe;
        try {
            // Fast path when we are trusted.
            unsafe = Unsafe.getUnsafe();
        } catch (SecurityException se) {
            // Slow path when we are not trusted.
            try {
                Field theUnsafe = Unsafe.class.getDeclaredField("theUnsafe");
                theUnsafe.setAccessible(true);
                unsafe = (Unsafe) theUnsafe.get(Unsafe.class);
            } catch (Exception e) {
                throw new RuntimeException("exception while trying to get Unsafe", e);
            }
        }
        PLATFORM_UNSAFE = unsafe;
        UNSAFE = new UnsafeAccess();
    }

    UnsafeAccess() {
    }

    long getAddress(long l) {
        return PLATFORM_UNSAFE.getAddress(l);
    }

    int getByteVolatile(Object o, long l) {
        return PLATFORM_UNSAFE.getByteVolatile(o, l);
    }

    int getInt(long l) {
        return PLATFORM_UNSAFE.getInt(l);
    }

    long getLong(long l) {
        return PLATFORM_UNSAFE.getLong(l);
    }

    double getDouble(long l) {
        return PLATFORM_UNSAFE.getDouble(l);
    }

    float getFloat(long l) {
        return PLATFORM_UNSAFE.getFloat(l);
    }

    int getByte(long l) {
        return PLATFORM_UNSAFE.getByte(l);
    }

    int getShort(long l) {
        return PLATFORM_UNSAFE.getShort(l);
    }

    void putInt(long l, int size) {
        PLATFORM_UNSAFE.putInt(l, size);
    }

    int getChar(long l) {
        return PLATFORM_UNSAFE.getChar(l);
    }

    long staticFieldOffset(Field reflectionField) {
        return PLATFORM_UNSAFE.staticFieldOffset(reflectionField);
    }

    long objectFieldOffset(Field reflectionField) {
        return PLATFORM_UNSAFE.objectFieldOffset(reflectionField);
    }

    void copyMemory(Object srcBase, long srcOffset, Object destBase, long destOffset, long bytes) {
        PLATFORM_UNSAFE.copyMemory(srcBase, srcOffset, destBase, destOffset, bytes);
    }
}
