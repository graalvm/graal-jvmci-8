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
package jdk.vm.ci.hotspot.sparc;

import jdk.vm.ci.hotspot.HotSpotVMConfigAccess;
import jdk.vm.ci.hotspot.HotSpotVMConfigStore;

/**
 * Used to access native configuration details.
 *
 * All non-static, public fields in this class are so that they can be compiled as constants.
 */
public class SPARCHotSpotVMConfig extends HotSpotVMConfigAccess {

    public SPARCHotSpotVMConfig(HotSpotVMConfigStore config) {
        super(config);
    }

    public final boolean linuxOs = System.getProperty("os.name", "").startsWith("Linux");

    public final boolean useCompressedOops = getFlag("UseCompressedOops", Boolean.class);

    // CPU capabilities
    public final int vmVersionFeatures = getFieldValue("VM_Version::_features", Integer.class, "int");

    // SPARC specific values
    public final int sparcVis3Instructions = getConstant("VM_Version::vis3_instructions_m", Integer.class);
    public final int sparcVis2Instructions = getConstant("VM_Version::vis2_instructions_m", Integer.class);
    public final int sparcVis1Instructions = getConstant("VM_Version::vis1_instructions_m", Integer.class);
    public final int sparcCbcondInstructions = getConstant("VM_Version::cbcond_instructions_m", Integer.class);
    public final int sparcV8Instructions = getConstant("VM_Version::v8_instructions_m", Integer.class);
    public final int sparcHardwareMul32 = getConstant("VM_Version::hardware_mul32_m", Integer.class);
    public final int sparcHardwareDiv32 = getConstant("VM_Version::hardware_div32_m", Integer.class);
    public final int sparcHardwareFsmuld = getConstant("VM_Version::hardware_fsmuld_m", Integer.class);
    public final int sparcHardwarePopc = getConstant("VM_Version::hardware_popc_m", Integer.class);
    public final int sparcV9Instructions = getConstant("VM_Version::v9_instructions_m", Integer.class);
    public final int sparcSun4v = getConstant("VM_Version::sun4v_m", Integer.class);
    public final int sparcBlkInitInstructions = getConstant("VM_Version::blk_init_instructions_m", Integer.class);
    public final int sparcFmafInstructions = getConstant("VM_Version::fmaf_instructions_m", Integer.class);
    public final int sparcFmauInstructions = getConstant("VM_Version::fmau_instructions_m", Integer.class);
    public final int sparcSparc64Family = getConstant("VM_Version::sparc64_family_m", Integer.class);
    public final int sparcMFamily = getConstant("VM_Version::M_family_m", Integer.class);
    public final int sparcTFamily = getConstant("VM_Version::T_family_m", Integer.class);
    public final int sparcT1Model = getConstant("VM_Version::T1_model_m", Integer.class);
    public final int sparcSparc5Instructions = getConstant("VM_Version::sparc5_instructions_m", Integer.class);
    public final int sparcAesInstructions = getConstant("VM_Version::aes_instructions_m", Integer.class);
    public final int sparcSha1Instruction = getConstant("VM_Version::sha1_instruction_m", Integer.class);
    public final int sparcSha256Instruction = getConstant("VM_Version::sha256_instruction_m", Integer.class);
    public final int sparcSha512Instruction = getConstant("VM_Version::sha512_instruction_m", Integer.class);

    public final boolean useBlockZeroing = getFlag("UseBlockZeroing", Boolean.class);
    public final int blockZeroingLowLimit = getFlag("BlockZeroingLowLimit", Integer.class);
}
