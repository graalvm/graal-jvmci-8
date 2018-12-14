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
package jdk.vm.ci.hotspot.test;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.function.Predicate;

import org.junit.Assert;
import org.junit.Test;

import jdk.vm.ci.hotspot.HotSpotJVMCIRuntime;
import jdk.vm.ci.meta.MetaAccessProvider;
import jdk.vm.ci.meta.ResolvedJavaType;

public class TestHotSpotJVMCIRuntime {

    @Test
    public void getIntrinsificationTrustPredicateTest() throws Exception {
        HotSpotJVMCIRuntime runtime = HotSpotJVMCIRuntime.runtime();
        MetaAccessProvider metaAccess = runtime.getHostJVMCIBackend().getMetaAccess();
        Predicate<ResolvedJavaType> predicate = runtime.getIntrinsificationTrustPredicate(HotSpotJVMCIRuntime.class);
        List<Class<?>> classes = new ArrayList<>(Arrays.asList(
                        Object.class,
                        String.class,
                        Class.class,
                        HotSpotJVMCIRuntime.class,
                        VirtualObjectLayoutTest.class,
                        TestHotSpotJVMCIRuntime.class));
        try {
            classes.add(Class.forName("com.sun.crypto.provider.AESCrypt"));
            classes.add(Class.forName("com.sun.crypto.provider.CipherBlockChaining"));
        } catch (ClassNotFoundException e) {
            // Extension classes not available
        }
        ClassLoader jvmciLoader = HotSpotJVMCIRuntime.class.getClassLoader();
        ClassLoader extLoader = getExtensionLoader();
        for (Class<?> c : classes) {
            ClassLoader cl = c.getClassLoader();
            boolean expected = cl == null || cl == jvmciLoader || cl == extLoader;
            boolean actual = predicate.test(metaAccess.lookupJavaType(c));
            Assert.assertEquals(c + ": cl=" + cl, expected, actual);
        }
    }

    private static ClassLoader getExtensionLoader() throws Exception {
        Object launcher = Class.forName("sun.misc.Launcher").getMethod("getLauncher").invoke(null);
        ClassLoader appLoader = (ClassLoader) launcher.getClass().getMethod("getClassLoader").invoke(launcher);
        ClassLoader extLoader = appLoader.getParent();
        assert extLoader.getClass().getName().equals("sun.misc.Launcher$ExtClassLoader") : extLoader;
        return extLoader;
    }
}
