/*
 * Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.
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
package jdk.vm.ci.runtime.test;

import org.junit.Assert;
import org.junit.Test;

import jdk.vm.ci.meta.JavaMethod;
import jdk.vm.ci.meta.MetaAccessProvider;
import jdk.vm.ci.meta.ResolvedJavaMethod;
import jdk.vm.ci.meta.ResolvedJavaType;
import jdk.vm.ci.runtime.JVMCI;

public class ConstantPoolTest {

    static Object cloneByteArray(byte[] arr) {
        return arr.clone();
    }

    static Object cloneCharArray(char[] arr) {
        return arr.clone();
    }

    static Object cloneShortArray(short[] arr) {
        return arr.clone();
    }

    static Object cloneIntArray(int[] arr) {
        return arr.clone();
    }

    static Object cloneFloatArray(float[] arr) {
        return arr.clone();
    }

    static Object cloneLongArray(long[] arr) {
        return arr.clone();
    }

    static Object cloneDoubleArray(double[] arr) {
        return arr.clone();
    }

    static Object cloneObjectArray(Object[] arr) {
        return arr.clone();
    }

    public static final int ALOAD_0 = 42; // 0x2A
    public static final int INVOKEVIRTUAL = 182; // 0xB6

    public static int beU2(byte[] data, int bci) {
        return ((data[bci] & 0xff) << 8) | (data[bci + 1] & 0xff);
    }

    public static int beU1(byte[] data, int bci) {
        return data[bci] & 0xff;
    }

    @Test
    public void lookupArrayCloneMethodTest() throws Exception {
        MetaAccessProvider metaAccess = JVMCI.getRuntime().getHostJVMCIBackend().getMetaAccess();
        ResolvedJavaType type = metaAccess.lookupJavaType(ConstantPoolTest.class);
        for (ResolvedJavaMethod m : type.getDeclaredMethods()) {
            if (m.getName().startsWith("clone")) {
                byte[] bytecode = m.getCode();
                Assert.assertNotNull(m.toString(), bytecode);
                Assert.assertEquals(m.toString(), 5, bytecode.length);
                Assert.assertEquals(m.toString(), ALOAD_0, beU1(bytecode, 0));
                Assert.assertEquals(m.toString(), INVOKEVIRTUAL, beU1(bytecode, 1));
                int cpi = beU2(bytecode, 2);
                JavaMethod callee = m.getConstantPool().lookupMethod(cpi, INVOKEVIRTUAL);
                Assert.assertTrue(callee.toString(), callee instanceof ResolvedJavaMethod);
            }
        }
    }
}
