/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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

import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;

import jdk.vm.ci.meta.ResolvedJavaMethod;
import jdk.vm.ci.meta.ResolvedJavaType;
import jdk.vm.ci.meta.SpeculationLog.SpeculationReasonEncoding;

final class HotSpotSpeculationEncoding implements SpeculationReasonEncoding {

    private ByteArrayOutputStream baos = new ByteArrayOutputStream();
    private DataOutputStream dos = new DataOutputStream(baos);
    private byte[] result;

    private void checkOpen() {
        if (result != null) {
            throw new IllegalArgumentException("Cannot update closed speculation encoding");
        }
    }

    private static final int NULL_METHOD = -1;
    private static final int NULL_TYPE = -2;
    private static final int NULL_STRING = -3;

    @Override
    public void addMethod(ResolvedJavaMethod method) {
        if (!addNull(method, NULL_METHOD)) {
            checkOpen();
            if (method instanceof HotSpotResolvedJavaMethodImpl) {
                try {
                    dos.writeLong(((HotSpotResolvedJavaMethodImpl) method).getMetaspaceMethod());
                } catch (IOException e) {
                    throw new InternalError(e);
                }
            } else {
                throw new IllegalArgumentException("Cannot encode unsupported type " + method.getClass().getName() + ": " + method.format("%H.%n(%p)"));
            }
        }
    }

    @Override
    public void addType(ResolvedJavaType type) {
        if (!addNull(type, NULL_TYPE)) {
            checkOpen();
            if (type instanceof HotSpotResolvedObjectTypeImpl) {
                try {
                    dos.writeLong(((HotSpotResolvedObjectTypeImpl) type).getMetaspaceKlass());
                } catch (IOException e) {
                    throw new InternalError(e);
                }
            } else {
                throw new IllegalArgumentException("Cannot encode unsupported type " + type.getClass().getName() + ": " + type.toClassName());
            }
        }
    }

    @Override
    public void addString(String value) {
        if (!addNull(value, NULL_STRING)) {
            checkOpen();
            try {
                dos.writeChars(value);
            } catch (IOException e) {
                throw new InternalError(e);
            }
        }
    }

    @Override
    public void addInt(int value) {
        checkOpen();
        try {
            dos.writeInt(value);
        } catch (IOException e) {
            throw new InternalError(e);
        }
    }

    @Override
    public void addLong(long value) {
        checkOpen();
        try {
            dos.writeLong(value);
        } catch (IOException e) {
            throw new InternalError(e);
        }
    }

    private boolean addNull(Object o, int nullValue) {
        if (o == null) {
            addInt(nullValue);
            return true;
        }
        return false;
    }

    byte[] toByteArray() {
        if (result == null) {
            result = baos.toByteArray();
            dos = null;
            baos = null;
        }
        return result;
    }
}
