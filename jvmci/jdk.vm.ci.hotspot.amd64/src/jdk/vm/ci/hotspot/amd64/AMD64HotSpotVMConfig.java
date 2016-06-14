/*
 * Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.
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
package jdk.vm.ci.hotspot.amd64;

import jdk.vm.ci.hotspot.HotSpotVMConfigAccess;
import jdk.vm.ci.hotspot.HotSpotVMConfigStore;

public class AMD64HotSpotVMConfig extends HotSpotVMConfigAccess {

    public AMD64HotSpotVMConfig(HotSpotVMConfigStore config) {
        super(config);
    }

    /**
     * Maximum allowed size of allocated area for a frame.
     */
    public final int maxFrameSize = 16 * 1024;

    public final boolean windowsOs = System.getProperty("os.name", "").startsWith("Windows");

    public final boolean useCountLeadingZerosInstruction = getFlag("UseCountLeadingZerosInstruction", Boolean.class);
    public final boolean useCountTrailingZerosInstruction = getFlag("UseCountTrailingZerosInstruction", Boolean.class);
    public final boolean useCompressedOops = getFlag("UseCompressedOops", Boolean.class);

    // CPU capabilities
    public final int useSSE = getFlag("UseSSE", Integer.class);
    public final int useAVX = getFlag("UseAVX", Integer.class);

    public final long vmVersionFeatures = getFieldValue("VM_Version::_cpuFeatures", Long.class, "int");

    // CPU feature flags
    public final long amd64CX8 = getConstant("VM_Version::CPU_CX8", Long.class);
    public final long amd64CMOV = getConstant("VM_Version::CPU_CMOV", Long.class);
    public final long amd64FXSR = getConstant("VM_Version::CPU_FXSR", Long.class);
    public final long amd64HT = getConstant("VM_Version::CPU_HT", Long.class);
    public final long amd64MMX = getConstant("VM_Version::CPU_MMX", Long.class);
    public final long amd643DNOWPREFETCH = getConstant("VM_Version::CPU_3DNOW_PREFETCH", Long.class);
    public final long amd64SSE = getConstant("VM_Version::CPU_SSE", Long.class);
    public final long amd64SSE2 = getConstant("VM_Version::CPU_SSE2", Long.class);
    public final long amd64SSE3 = getConstant("VM_Version::CPU_SSE3", Long.class);
    public final long amd64SSSE3 = getConstant("VM_Version::CPU_SSSE3", Long.class);
    public final long amd64SSE4A = getConstant("VM_Version::CPU_SSE4A", Long.class);
    public final long amd64SSE41 = getConstant("VM_Version::CPU_SSE4_1", Long.class);
    public final long amd64SSE42 = getConstant("VM_Version::CPU_SSE4_2", Long.class);
    public final long amd64POPCNT = getConstant("VM_Version::CPU_POPCNT", Long.class);
    public final long amd64LZCNT = getConstant("VM_Version::CPU_LZCNT", Long.class);
    public final long amd64TSC = getConstant("VM_Version::CPU_TSC", Long.class);
    public final long amd64TSCINV = getConstant("VM_Version::CPU_TSCINV", Long.class);
    public final long amd64AVX = getConstant("VM_Version::CPU_AVX", Long.class);
    public final long amd64AVX2 = getConstant("VM_Version::CPU_AVX2", Long.class);
    public final long amd64AES = getConstant("VM_Version::CPU_AES", Long.class);
    public final long amd64ERMS = getConstant("VM_Version::CPU_ERMS", Long.class);
    public final long amd64CLMUL = getConstant("VM_Version::CPU_CLMUL", Long.class);
    public final long amd64BMI1 = getConstant("VM_Version::CPU_BMI1", Long.class);
    public final long amd64BMI2 = getConstant("VM_Version::CPU_BMI2", Long.class);
    public final long amd64RTM = getConstant("VM_Version::CPU_RTM", Long.class);
    public final long amd64ADX = getConstant("VM_Version::CPU_ADX", Long.class);
}
