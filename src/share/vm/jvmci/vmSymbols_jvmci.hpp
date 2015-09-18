/*
 * Copyright (c) 2012, 2015, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_JVMCI_VMSYMBOLS_JVMCI_HPP
#define SHARE_VM_JVMCI_VMSYMBOLS_JVMCI_HPP


#if !INCLUDE_JVMCI
#define JVMCI_VM_SYMBOLS_DO(template, do_alias)
#else
#define JVMCI_VM_SYMBOLS_DO(template, do_alias)                                                                                           \
  template(jdk_internal_jvmci_hotspot_HotSpotCompiledCode,             "jdk/internal/jvmci/hotspot/HotSpotCompiledCode")                  \
  template(jdk_internal_jvmci_hotspot_HotSpotCompiledCode_Comment,     "jdk/internal/jvmci/hotspot/HotSpotCompiledCode$Comment")          \
  template(jdk_internal_jvmci_hotspot_HotSpotCompiledNmethod,          "jdk/internal/jvmci/hotspot/HotSpotCompiledNmethod")               \
  template(jdk_internal_jvmci_hotspot_HotSpotForeignCallTarget,        "jdk/internal/jvmci/hotspot/HotSpotForeignCallTarget")             \
  template(jdk_internal_jvmci_hotspot_HotSpotReferenceMap,             "jdk/internal/jvmci/hotspot/HotSpotReferenceMap")                  \
  template(jdk_internal_jvmci_hotspot_CompilerToVM,                    "jdk/internal/jvmci/hotspot/CompilerToVM")                         \
  template(jdk_internal_jvmci_hotspot_HotSpotInstalledCode,            "jdk/internal/jvmci/hotspot/HotSpotInstalledCode")                 \
  template(jdk_internal_jvmci_hotspot_HotSpotNmethod,                  "jdk/internal/jvmci/hotspot/HotSpotNmethod")                       \
  template(jdk_internal_jvmci_hotspot_HotSpotResolvedJavaMethodImpl,   "jdk/internal/jvmci/hotspot/HotSpotResolvedJavaMethodImpl")        \
  template(jdk_internal_jvmci_hotspot_HotSpotResolvedObjectTypeImpl,   "jdk/internal/jvmci/hotspot/HotSpotResolvedObjectTypeImpl")        \
  template(jdk_internal_jvmci_hotspot_HotSpotCompressedNullConstant,   "jdk/internal/jvmci/hotspot/HotSpotCompressedNullConstant")        \
  template(jdk_internal_jvmci_hotspot_HotSpotObjectConstantImpl,       "jdk/internal/jvmci/hotspot/HotSpotObjectConstantImpl")            \
  template(jdk_internal_jvmci_hotspot_HotSpotMetaspaceConstantImpl,    "jdk/internal/jvmci/hotspot/HotSpotMetaspaceConstantImpl")         \
  template(jdk_internal_jvmci_hotspot_HotSpotStackFrameReference,      "jdk/internal/jvmci/hotspot/HotSpotStackFrameReference")           \
  template(jdk_internal_jvmci_hotspot_HotSpotConstantPool,             "jdk/internal/jvmci/hotspot/HotSpotConstantPool")                  \
  template(jdk_internal_jvmci_hotspot_HotSpotJVMCIMetaAccessContext,   "jdk/internal/jvmci/hotspot/HotSpotJVMCIMetaAccessContext")        \
  template(jdk_internal_jvmci_hotspot_HotSpotJVMCIRuntime,             "jdk/internal/jvmci/hotspot/HotSpotJVMCIRuntime")                  \
  template(jdk_internal_jvmci_meta_JavaConstant,                       "jdk/internal/jvmci/meta/JavaConstant")                            \
  template(jdk_internal_jvmci_meta_PrimitiveConstant,                  "jdk/internal/jvmci/meta/PrimitiveConstant")                       \
  template(jdk_internal_jvmci_meta_RawConstant,                        "jdk/internal/jvmci/meta/RawConstant")                             \
  template(jdk_internal_jvmci_meta_NullConstant,                       "jdk/internal/jvmci/meta/NullConstant")                            \
  template(jdk_internal_jvmci_meta_ExceptionHandler,                   "jdk/internal/jvmci/meta/ExceptionHandler")                        \
  template(jdk_internal_jvmci_meta_JavaKind,                           "jdk/internal/jvmci/meta/JavaKind")                                \
  template(jdk_internal_jvmci_meta_LIRKind,                            "jdk/internal/jvmci/meta/LIRKind")                                 \
  template(jdk_internal_jvmci_meta_Value,                              "jdk/internal/jvmci/meta/Value")                                   \
  template(jdk_internal_jvmci_meta_Assumptions_ConcreteSubtype,        "jdk/internal/jvmci/meta/Assumptions$ConcreteSubtype")             \
  template(jdk_internal_jvmci_meta_Assumptions_LeafType,               "jdk/internal/jvmci/meta/Assumptions$LeafType")                    \
  template(jdk_internal_jvmci_meta_Assumptions_NoFinalizableSubclass,  "jdk/internal/jvmci/meta/Assumptions$NoFinalizableSubclass")       \
  template(jdk_internal_jvmci_meta_Assumptions_ConcreteMethod,         "jdk/internal/jvmci/meta/Assumptions$ConcreteMethod")              \
  template(jdk_internal_jvmci_meta_Assumptions_CallSiteTargetValue,    "jdk/internal/jvmci/meta/Assumptions$CallSiteTargetValue")         \
  template(jdk_internal_jvmci_meta_SpeculationLog,                     "jdk/internal/jvmci/meta/SpeculationLog")                          \
  template(jdk_internal_jvmci_code_Architecture,                       "jdk/internal/jvmci/code/Architecture")                            \
  template(jdk_internal_jvmci_code_TargetDescription,                  "jdk/internal/jvmci/code/TargetDescription")                       \
  template(jdk_internal_jvmci_code_CompilationResult_Call,             "jdk/internal/jvmci/code/CompilationResult$Call")                  \
  template(jdk_internal_jvmci_code_CompilationResult_ConstantReference, "jdk/internal/jvmci/code/CompilationResult$ConstantReference")    \
  template(jdk_internal_jvmci_code_CompilationResult_DataPatch,        "jdk/internal/jvmci/code/CompilationResult$DataPatch")             \
  template(jdk_internal_jvmci_code_CompilationResult_DataSectionReference, "jdk/internal/jvmci/code/CompilationResult$DataSectionReference") \
  template(jdk_internal_jvmci_code_CompilationResult_ExceptionHandler, "jdk/internal/jvmci/code/CompilationResult$ExceptionHandler")      \
  template(jdk_internal_jvmci_code_CompilationResult_Mark,             "jdk/internal/jvmci/code/CompilationResult$Mark")                  \
  template(jdk_internal_jvmci_code_CompilationResult_Infopoint,        "jdk/internal/jvmci/code/CompilationResult$Infopoint")             \
  template(jdk_internal_jvmci_code_CompilationResult_Site,             "jdk/internal/jvmci/code/CompilationResult$Site")                  \
  template(jdk_internal_jvmci_code_InfopointReason,                    "jdk/internal/jvmci/code/InfopointReason")                         \
  template(jdk_internal_jvmci_code_InstalledCode,                      "jdk/internal/jvmci/code/InstalledCode")                           \
  template(jdk_internal_jvmci_code_BytecodeFrame,                      "jdk/internal/jvmci/code/BytecodeFrame")                           \
  template(jdk_internal_jvmci_code_BytecodePosition,                   "jdk/internal/jvmci/code/BytecodePosition")                        \
  template(jdk_internal_jvmci_code_DebugInfo,                          "jdk/internal/jvmci/code/DebugInfo")                               \
  template(jdk_internal_jvmci_code_Location,                           "jdk/internal/jvmci/code/Location")                                \
  template(jdk_internal_jvmci_code_Register,                           "jdk/internal/jvmci/code/Register")                                \
  template(jdk_internal_jvmci_code_RegisterValue,                      "jdk/internal/jvmci/code/RegisterValue")                           \
  template(jdk_internal_jvmci_code_StackSlot,                          "jdk/internal/jvmci/code/StackSlot")                               \
  template(jdk_internal_jvmci_code_StackLockValue,                     "jdk/internal/jvmci/code/StackLockValue")                          \
  template(jdk_internal_jvmci_code_VirtualObject,                      "jdk/internal/jvmci/code/VirtualObject")                           \
  template(jdk_internal_jvmci_code_RegisterSaveLayout,                 "jdk/internal/jvmci/code/RegisterSaveLayout")                      \
  template(jdk_internal_jvmci_code_InvalidInstalledCodeException,      "jdk/internal/jvmci/code/InvalidInstalledCodeException")           \
  template(compileMethod_name,                                         "compileMethod")                                                   \
  template(compileMethod_signature,                                    "(Ljdk/internal/jvmci/hotspot/HotSpotResolvedJavaMethod;IJI)V")    \
  template(fromMetaspace_name,                                         "fromMetaspace")                                                   \
  template(method_fromMetaspace_signature,                             "(J)Ljdk/internal/jvmci/hotspot/HotSpotResolvedJavaMethod;")       \
  template(constantPool_fromMetaspace_signature,                       "(J)Ljdk/internal/jvmci/hotspot/HotSpotConstantPool;")             \
  template(klass_fromMetaspace_signature,                              "(Ljava/lang/Class;)Ljdk/internal/jvmci/hotspot/HotSpotResolvedObjectTypeImpl;")   \
  template(jdk_internal_jvmci_hotspot_Stable_signature,                "Ljdk/internal/jvmci/hotspot/Stable;")
#endif

#endif // SHARE_VM_JVMCI_VMSYMBOLS_JVMCI_HPP
