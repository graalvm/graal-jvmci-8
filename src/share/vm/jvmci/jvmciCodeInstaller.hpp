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

#ifndef SHARE_VM_JVMCI_JVMCI_CODE_INSTALLER_HPP
#define SHARE_VM_JVMCI_JVMCI_CODE_INSTALLER_HPP

#include "jvmci/jvmci.hpp"
#include "jvmci/jvmciCompiler.hpp"
#include "jvmci/jvmciEnv.hpp"
#include "jvmci/jvmciJavaClasses.hpp"

/*
 * This class handles the conversion from a InstalledCode to a CodeBlob or an nmethod.
 */
class CodeInstaller : public StackObj {
  friend class VMStructs;
private:
  enum MarkId {
    VERIFIED_ENTRY             = 1,
    UNVERIFIED_ENTRY           = 2,
    OSR_ENTRY                  = 3,
    EXCEPTION_HANDLER_ENTRY    = 4,
    DEOPT_HANDLER_ENTRY        = 5,
    INVOKEINTERFACE            = 6,
    INVOKEVIRTUAL              = 7,
    INVOKESTATIC               = 8,
    INVOKESPECIAL              = 9,
    INLINE_INVOKE              = 10,
    POLL_NEAR                  = 11,
    POLL_RETURN_NEAR           = 12,
    POLL_FAR                   = 13,
    POLL_RETURN_FAR            = 14,
    CARD_TABLE_ADDRESS         = 15,
    CARD_TABLE_SHIFT           = 16,
    INVOKE_INVALID             = -1
  };

  Arena         _arena;
  JVMCIEnv*     _jvmci_env;

  JVMCIPrimitiveArray    _data_section_handle;
  JVMCIObjectArray       _data_section_patches_handle;
  JVMCIObjectArray       _sites_handle;
#ifndef PRODUCT
  JVMCIObjectArray       _comments_handle;
#endif
  JVMCIPrimitiveArray    _code_handle;
  JVMCIObject            _word_kind_handle;

  CodeOffsets   _offsets;

  jint          _code_size;
  jint          _total_frame_size;
  jint          _orig_pc_offset;
  jint          _parameter_count;
  jint          _constants_size;

  bool          _has_wide_vector;

  MarkId        _next_call_type;
  address       _invoke_mark_pc;

  CodeSection*  _instructions;
  CodeSection*  _constants;

  OopRecorder*              _oop_recorder;
  DebugInformationRecorder* _debug_recorder;
  Dependencies*             _dependencies;
  ExceptionHandlerTable     _exception_handler_table;
  ImplicitExceptionTable    _implicit_exception_table;

  static ConstantOopWriteValue* _oop_null_scope_value;
  static ConstantIntValue*    _int_m1_scope_value;
  static ConstantIntValue*    _int_0_scope_value;
  static ConstantIntValue*    _int_1_scope_value;
  static ConstantIntValue*    _int_2_scope_value;
  static LocationValue*       _illegal_value;

  jint pd_next_offset(NativeInstruction* inst, jint pc_offset, JVMCIObject method, JVMCI_TRAPS);
  void pd_patch_OopConstant(int pc_offset, JVMCIObject constant, JVMCI_TRAPS);
  void pd_patch_MetaspaceConstant(int pc_offset, JVMCIObject constant, JVMCI_TRAPS);
  void pd_patch_DataSectionReference(int pc_offset, int data_offset, JVMCI_TRAPS);
  void pd_relocate_ForeignCall(NativeInstruction* inst, jlong foreign_call_destination, JVMCI_TRAPS);
  void pd_relocate_JavaMethod(JVMCIObject method, jint pc_offset, JVMCI_TRAPS);
  void pd_relocate_poll(address pc, jint mark, JVMCI_TRAPS);

  JVMCIObjectArray sites()                { return _sites_handle; }
  JVMCIPrimitiveArray code()              { return _code_handle; }
  JVMCIPrimitiveArray  data_section()     { return _data_section_handle; }
  JVMCIObjectArray data_section_patches() { return _data_section_patches_handle; }
#ifndef PRODUCT
  JVMCIObjectArray comments()             { return _comments_handle; }
#endif
  JVMCIObject word_kind()                 { return _word_kind_handle; }

public:

  CodeInstaller(JVMCIEnv* jvmci_env) : _arena(mtCompiler), _jvmci_env(jvmci_env) {}

  JVMCI::CodeInstallResult install(JVMCICompiler* compiler, JVMCIObject target, JVMCIObject compiled_code,
                                   CodeBlob*& cb, JVMCIObject installed_code, JVMCIObject speculation_log, JVMCI_TRAPS);

  JVMCIEnv* jvmci_env() { return _jvmci_env; }
  JVMCIRuntime* runtime() { return _jvmci_env->runtime(); }

  static address runtime_call_target_address(oop runtime_call);
  static VMReg get_hotspot_reg(jint jvmciRegisterNumber, JVMCI_TRAPS);
  static bool is_general_purpose_reg(VMReg hotspotRegister);

private:
  Location::Type get_oop_type(JVMCIObject value);
  ScopeValue* get_scope_value(JVMCIObject value, BasicType type, GrowableArray<ScopeValue*>* objects, ScopeValue* &second, JVMCI_TRAPS);
  MonitorValue* get_monitor_value(JVMCIObject value, GrowableArray<ScopeValue*>* objects, JVMCI_TRAPS);

  void* record_metadata_reference(CodeSection* section, address dest, JVMCIObject constant, JVMCI_TRAPS);
#ifdef _LP64
  narrowKlass record_narrow_metadata_reference(CodeSection* section, address dest, JVMCIObject constant, JVMCI_TRAPS);
#endif

  // extract the fields of the HotSpotCompiledCode
  void initialize_fields(JVMCIObject target, JVMCIObject compiled_code, JVMCI_TRAPS);
  void initialize_dependencies(JVMCIObject compiled_code, JVMCI_TRAPS);
  
  int estimate_stubs_size(JVMCI_TRAPS);
  
  // perform data and call relocation on the CodeBuffer
  JVMCI::CodeInstallResult initialize_buffer(CodeBuffer& buffer, JVMCI_TRAPS);

  void assumption_NoFinalizableSubclass(JVMCIObject assumption);
  void assumption_ConcreteSubtype(JVMCIObject assumption);
  void assumption_LeafType(JVMCIObject assumption);
  void assumption_ConcreteMethod(JVMCIObject assumption);
  void assumption_CallSiteTargetValue(JVMCIObject assumption, JVMCI_TRAPS);

  void site_Safepoint(CodeBuffer& buffer, jint pc_offset, JVMCIObject site, JVMCI_TRAPS);
  void site_Infopoint(CodeBuffer& buffer, jint pc_offset, JVMCIObject site, JVMCI_TRAPS);
  void site_Call(CodeBuffer& buffer, jint pc_offset, JVMCIObject site, JVMCI_TRAPS);
  void site_DataPatch(CodeBuffer& buffer, jint pc_offset, JVMCIObject site, JVMCI_TRAPS);
  void site_Mark(CodeBuffer& buffer, jint pc_offset, JVMCIObject site, JVMCI_TRAPS);
  void site_ExceptionHandler(jint pc_offset, JVMCIObject site);

  OopMap* create_oop_map(JVMCIObject debug_info, JVMCI_TRAPS);

  VMReg getVMRegFromLocation(JVMCIObject location, int total_frame_size, JVMCI_TRAPS);

  /**
   * Specifies the level of detail to record for a scope.
   */
  enum ScopeMode {
    // Only record a method and BCI
    BytecodePosition,
    // Record a method, bci and JVM frame state
    FullFrame
  };

  int map_jvmci_bci(int bci);
  
  void record_scope(jint pc_offset, JVMCIObject debug_info, ScopeMode scope_mode, bool return_oop, JVMCI_TRAPS);
  void record_scope(jint pc_offset, JVMCIObject debug_info, ScopeMode scope_mode, JVMCI_TRAPS) {
    record_scope(pc_offset, debug_info, scope_mode, false /* return_oop */, JVMCIENV);
  }
  void record_scope(jint pc_offset, JVMCIObject position, ScopeMode scope_mode, GrowableArray<ScopeValue*>* objects, bool return_oop, JVMCI_TRAPS);
  void record_object_value(ObjectValue* sv, JVMCIObject value, GrowableArray<ScopeValue*>* objects, JVMCI_TRAPS);

  GrowableArray<ScopeValue*>* record_virtual_objects(JVMCIObject debug_info, JVMCI_TRAPS);

  int estimateStubSpace(int static_call_stubs);
};

#endif // SHARE_VM_JVMCI_JVMCI_CODE_INSTALLER_HPP
