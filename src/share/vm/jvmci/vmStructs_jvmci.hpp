/*
 * Copyright (c) 2013, 2020, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

#ifndef SHARE_VM_JVMCI_VMSTRUCTS_JVMCI_HPP
#define SHARE_VM_JVMCI_VMSTRUCTS_JVMCI_HPP

#include "compiler/abstractCompiler.hpp"
#include "jvmci/jvmciCodeInstaller.hpp"
#include "jvmci/jvmciCompilerToVM.hpp"
#include "jvmci/jvmciEnv.hpp"

#define VM_STRUCTS_JVMCI(nonstatic_field, static_field, volatile_nonstatic_field)                                                    \
  static_field(CompilerToVM::Data,             Klass_vtable_start_offset,              int)                                          \
  static_field(CompilerToVM::Data,             Klass_vtable_length_offset,             int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             Method_extra_stack_entries,             int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             SharedRuntime_ic_miss_stub,             address)                                      \
  static_field(CompilerToVM::Data,             SharedRuntime_handle_wrong_method_stub, address)                                      \
  static_field(CompilerToVM::Data,             SharedRuntime_deopt_blob_unpack,        address)                                      \
  static_field(CompilerToVM::Data,             SharedRuntime_deopt_blob_unpack_with_exception_in_tls, address)                              \
  static_field(CompilerToVM::Data,             SharedRuntime_deopt_blob_uncommon_trap, address)                                      \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             ThreadLocalAllocBuffer_alignment_reserve, size_t)                                     \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             Universe_collectedHeap,                 CollectedHeap*)                               \
  static_field(CompilerToVM::Data,             Universe_base_vtable_size,              int)                                          \
  static_field(CompilerToVM::Data,             Universe_narrow_oop_base,               address)                                      \
  static_field(CompilerToVM::Data,             Universe_narrow_oop_shift,              int)                                          \
  static_field(CompilerToVM::Data,             Universe_narrow_klass_base,             address)                                      \
  static_field(CompilerToVM::Data,             Universe_narrow_klass_shift,            int)                                          \
  static_field(CompilerToVM::Data,             Universe_non_oop_bits,                  void*)                                        \
  static_field(CompilerToVM::Data,             Universe_verify_oop_mask,               uintptr_t)                                    \
  static_field(CompilerToVM::Data,             Universe_verify_oop_bits,               uintptr_t)                                    \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             _supports_inline_contig_alloc,          bool)                                         \
  static_field(CompilerToVM::Data,             _heap_end_addr,                         HeapWord**)                                   \
  static_field(CompilerToVM::Data,             _heap_top_addr,                         HeapWord**)                                   \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             _max_oop_map_stack_offset,              int)                                          \
  static_field(CompilerToVM::Data,             _fields_annotations_base_offset,        int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             cardtable_start_address,                jbyte*)                                       \
  static_field(CompilerToVM::Data,             cardtable_shift,                        int)                                          \
  static_field(CompilerToVM::Data,             g1_young_card,                          int)                                          \
  static_field(CompilerToVM::Data,             dirty_card,                             int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             vm_page_size,                           int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             sizeof_vtableEntry,                     int)                                          \
  static_field(CompilerToVM::Data,             sizeof_ExceptionTableElement,           int)                                          \
  static_field(CompilerToVM::Data,             sizeof_LocalVariableTableElement,       int)                                          \
  static_field(CompilerToVM::Data,             sizeof_ConstantPool,                    int)                                          \
  static_field(CompilerToVM::Data,             sizeof_narrowKlass,                     int)                                          \
  static_field(CompilerToVM::Data,             sizeof_arrayOopDesc,                    int)                                          \
  static_field(CompilerToVM::Data,             sizeof_BasicLock,                       int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             CodeCache_low_bound,                    address)                                      \
  static_field(CompilerToVM::Data,             CodeCache_high_bound,                   address)                                      \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             dsin,                                   address)                                      \
  static_field(CompilerToVM::Data,             dcos,                                   address)                                      \
  static_field(CompilerToVM::Data,             dtan,                                   address)                                      \
  static_field(CompilerToVM::Data,             dexp,                                   address)                                      \
  static_field(CompilerToVM::Data,             dlog,                                   address)                                      \
  static_field(CompilerToVM::Data,             dlog10,                                 address)                                      \
  static_field(CompilerToVM::Data,             dpow,                                   address)                                      \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             symbol_init,                            address)                                      \
  static_field(CompilerToVM::Data,             symbol_clinit,                          address)                                      \
                                                                                                                                     \
  static_field(StubRoutines,                   _sha1_implCompress,                     address)                                      \
  static_field(StubRoutines,                   _sha1_implCompressMB,                   address)                                      \
  static_field(StubRoutines,                   _sha256_implCompress,                   address)                                      \
  static_field(StubRoutines,                   _sha256_implCompressMB,                 address)                                      \
  static_field(StubRoutines,                   _sha512_implCompress,                   address)                                      \
  static_field(StubRoutines,                   _sha512_implCompressMB,                 address)                                      \
  static_field(StubRoutines,                   _montgomeryMultiply,                    address)                                      \
  static_field(StubRoutines,                   _montgomerySquare,                      address)                                      \
                                                                                                                                     \
  volatile_nonstatic_field(ObjectMonitor,      _cxq,                                   ObjectWaiter*)                                \
  volatile_nonstatic_field(ObjectMonitor,      _EntryList,                             ObjectWaiter*)                                \
  volatile_nonstatic_field(ObjectMonitor,      _succ,                                  Thread*)                                      \
                                                                                                                                     \
  nonstatic_field(Klass,                       _class_loader_data,                     ClassLoaderData*)                             \
                                                                                                                                     \
  nonstatic_field(JVMCICompileState,           _jvmti_can_hotswap_or_post_breakpoint,  jbyte)                                        \
  nonstatic_field(JVMCICompileState,           _jvmti_can_access_local_variables,      jbyte)                                        \
  nonstatic_field(JVMCICompileState,           _jvmti_can_post_on_exceptions,          jbyte)                                        \
  nonstatic_field(JVMCICompileState,           _jvmti_can_pop_frame,                   jbyte)                                        \
  nonstatic_field(JVMCICompileState,           _compilation_ticks,                     jint)                                         \
                                                                                                                                     \
  volatile_nonstatic_field(JavaThread,         _doing_unsafe_access,                   bool)                                         \
  nonstatic_field(JavaThread,                  _pending_deoptimization,                int)                                          \
  nonstatic_field(JavaThread,                  _pending_failed_speculation,            jlong)                                        \
  nonstatic_field(JavaThread,                  _pending_transfer_to_interpreter,       bool)                                         \
  nonstatic_field(JavaThread,                  _jvmci_counters,                        jlong*)                                       \
  nonstatic_field(JavaThread,                  _jvmci_reserved0,                       intptr_t*)                                    \
  nonstatic_field(JavaThread,                  _jvmci_reserved1,                       intptr_t*)                                    \
  nonstatic_field(JavaThread,                  _jvmci_reserved_oop0,                   oop)                                          \
  nonstatic_field(JavaThread,                  _should_post_on_exceptions_flag,        int)                                          \
  nonstatic_field(JavaThread,                  _jni_environment,                       JNIEnv)                                       \
  nonstatic_field(MethodData,                  _jvmci_ir_size,                         int)                                          \
  nonstatic_field(ConstantPool,                _flags,                                 int)                                          \
  nonstatic_field(Annotations,                 _fields_annotations,                    Array<AnnotationArray*>*)                     \

#define VM_TYPES_JVMCI(declare_type, declare_toplevel_type)                   \
  declare_toplevel_type(narrowKlass)                                          \
  declare_toplevel_type(JVMCIEnv)                                             \
  declare_toplevel_type(CompilerToVM::Data)                                   \
  declare_toplevel_type(ObjectWaiter)                                         \
  declare_toplevel_type(JVMCICompileState)                                    \
  declare_toplevel_type(Annotations)                                          \
  declare_toplevel_type(Array<AnnotationArray*>*)                             \
  declare_toplevel_type(JNIEnv)

#define VM_INT_CONSTANTS_JVMCI(declare_constant, declare_preprocessor_constant)                   \
  declare_constant(Deoptimization::Reason_unreached0)                                             \
  declare_constant(Deoptimization::Reason_type_checked_inlining)                                  \
  declare_constant(Deoptimization::Reason_optimized_type_check)                                   \
  declare_constant(Deoptimization::Reason_aliasing)                                               \
  declare_constant(Deoptimization::Reason_transfer_to_interpreter)                                \
  declare_constant(Deoptimization::Reason_not_compiled_exception_handler)                         \
  declare_constant(Deoptimization::Reason_unresolved)                                             \
  declare_constant(Deoptimization::Reason_jsr_mismatch)                                           \
  declare_constant(Deoptimization::_support_large_access_byte_array_virtualization)               \
  declare_preprocessor_constant("JVMCIEnv::ok",                   JVMCI::ok)                      \
  declare_preprocessor_constant("JVMCIEnv::dependencies_failed",  JVMCI::dependencies_failed)     \
  declare_preprocessor_constant("JVMCIEnv::dependencies_invalid", JVMCI::dependencies_invalid)    \
  declare_preprocessor_constant("JVMCIEnv::cache_full",           JVMCI::cache_full)              \
  declare_preprocessor_constant("JVMCIEnv::code_too_large",       JVMCI::code_too_large)          \
  declare_constant(JVMCIRuntime::none)                                                            \
  declare_constant(JVMCIRuntime::by_holder)                                                       \
  declare_constant(JVMCIRuntime::by_full_signature)                                               \
                                                                                                  \
  declare_preprocessor_constant("JVM_ACC_VARARGS", JVM_ACC_VARARGS)                               \
  declare_preprocessor_constant("JVM_ACC_BRIDGE", JVM_ACC_BRIDGE)                                 \
  declare_preprocessor_constant("JVM_ACC_ANNOTATION", JVM_ACC_ANNOTATION)                         \
  declare_preprocessor_constant("JVM_ACC_ENUM", JVM_ACC_ENUM)                                     \
  declare_preprocessor_constant("JVM_ACC_SYNTHETIC", JVM_ACC_SYNTHETIC)                           \
  declare_preprocessor_constant("JVM_ACC_INTERFACE", JVM_ACC_INTERFACE)                           \
  declare_preprocessor_constant("JVM_ACC_FIELD_INITIALIZED_FINAL_UPDATE", JVM_ACC_FIELD_INITIALIZED_FINAL_UPDATE) \
                                                                                                  \
  declare_constant(BitData::exception_seen_flag)                                                  \
  declare_constant(BitData::null_seen_flag)                                                       \
  declare_constant(CounterData::count_off)                                                        \
  declare_constant(JumpData::taken_off_set)                                                       \
  declare_constant(JumpData::displacement_off_set)                                                \
  declare_constant(ReceiverTypeData::nonprofiled_count_off_set)                                   \
  declare_constant(ReceiverTypeData::receiver_type_row_cell_count)                                \
  declare_constant(ReceiverTypeData::receiver0_offset)                                            \
  declare_constant(ReceiverTypeData::count0_offset)                                               \
  declare_constant(BranchData::not_taken_off_set)                                                 \
  declare_constant(ArrayData::array_len_off_set)                                                  \
  declare_constant(ArrayData::array_start_off_set)                                                \
  declare_constant(MultiBranchData::per_case_cell_count)                                          \
  declare_constant(JVMCINMethodData::SPECULATION_LENGTH_BITS)                                     \
                                                                                                  \
  declare_constant(CodeInstaller::VERIFIED_ENTRY)                                                 \
  declare_constant(CodeInstaller::UNVERIFIED_ENTRY)                                               \
  declare_constant(CodeInstaller::OSR_ENTRY)                                                      \
  declare_constant(CodeInstaller::EXCEPTION_HANDLER_ENTRY)                                        \
  declare_constant(CodeInstaller::DEOPT_HANDLER_ENTRY)                                            \
  declare_constant(CodeInstaller::FRAME_COMPLETE)                                                 \
  declare_constant(CodeInstaller::INVOKEINTERFACE)                                                \
  declare_constant(CodeInstaller::INVOKEVIRTUAL)                                                  \
  declare_constant(CodeInstaller::INVOKESTATIC)                                                   \
  declare_constant(CodeInstaller::INVOKESPECIAL)                                                  \
  declare_constant(CodeInstaller::INLINE_INVOKE)                                                  \
  declare_constant(CodeInstaller::POLL_NEAR)                                                      \
  declare_constant(CodeInstaller::POLL_RETURN_NEAR)                                               \
  declare_constant(CodeInstaller::POLL_FAR)                                                       \
  declare_constant(CodeInstaller::POLL_RETURN_FAR)                                                \
  declare_constant(CodeInstaller::CARD_TABLE_SHIFT)                                               \
  declare_constant(CodeInstaller::CARD_TABLE_ADDRESS)                                             \
  declare_constant(CodeInstaller::DEOPT_MH_HANDLER_ENTRY)                                         \
  declare_constant(CodeInstaller::INVOKE_INVALID)                                                 \
                                                                                                  \
  declare_constant(vmIntrinsics::FIRST_MH_SIG_POLY)                                               \
  declare_constant(vmIntrinsics::LAST_MH_SIG_POLY)                                                \
  declare_constant(vmIntrinsics::_invokeGeneric)                                                  \
  declare_constant(vmIntrinsics::_compiledLambdaForm)                                             \
                                                                                                  \
  declare_constant(Method::invalid_vtable_index)                                                  \

#define VM_ADDRESSES_JVMCI(declare_address, declare_preprocessor_address, declare_function) \
  declare_function(SharedRuntime::register_finalizer)                     \
  declare_function(SharedRuntime::exception_handler_for_return_address)   \
  declare_function(SharedRuntime::OSR_migration_end)                      \
  declare_function(SharedRuntime::frem)                                   \
  declare_function(SharedRuntime::drem)                                   \
                                                                          \
  declare_function(os::dll_load)                                          \
  declare_function(os::dll_lookup)                                        \
  declare_function(os::javaTimeMillis)                                    \
  declare_function(os::javaTimeNanos)                                     \
                                                                          \
  declare_function(Deoptimization::fetch_unroll_info)                     \
  declare_function(Deoptimization::uncommon_trap)                         \
  declare_function(Deoptimization::unpack_frames)                         \
                                                                          \
  declare_function(JVMCIRuntime::new_instance) \
  declare_function(JVMCIRuntime::new_array) \
  declare_function(JVMCIRuntime::new_multi_array) \
  declare_function(JVMCIRuntime::dynamic_new_array) \
  declare_function(JVMCIRuntime::dynamic_new_instance) \
  \
  declare_function(JVMCIRuntime::new_instance_or_null) \
  declare_function(JVMCIRuntime::new_array_or_null) \
  declare_function(JVMCIRuntime::new_multi_array_or_null) \
  declare_function(JVMCIRuntime::dynamic_new_array_or_null) \
  declare_function(JVMCIRuntime::dynamic_new_instance_or_null) \
  \
  declare_function(JVMCIRuntime::invoke_static_method_one_arg) \
  \
  declare_function(JVMCIRuntime::thread_is_interrupted) \
  declare_function(JVMCIRuntime::vm_message) \
  declare_function(JVMCIRuntime::identity_hash_code) \
  declare_function(JVMCIRuntime::exception_handler_for_pc) \
  declare_function(JVMCIRuntime::monitorenter) \
  declare_function(JVMCIRuntime::monitorexit) \
  declare_function(JVMCIRuntime::throw_and_post_jvmti_exception) \
  declare_function(JVMCIRuntime::throw_klass_external_name_exception) \
  declare_function(JVMCIRuntime::throw_class_cast_exception) \
  declare_function(JVMCIRuntime::log_primitive) \
  declare_function(JVMCIRuntime::log_object) \
  declare_function(JVMCIRuntime::log_printf) \
  declare_function(JVMCIRuntime::vm_error) \
  declare_function(JVMCIRuntime::load_and_clear_exception) \
  declare_function(JVMCIRuntime::write_barrier_pre) \
  declare_function(JVMCIRuntime::write_barrier_post) \
  declare_function(JVMCIRuntime::validate_object) \
  \
  declare_function(JVMCIRuntime::test_deoptimize_call_int)

#ifdef TARGET_OS_FAMILY_linux

#define VM_ADDRESSES_JVMCI_OS(declare_address, declare_preprocessor_address, declare_function) \
  declare_preprocessor_address("RTLD_DEFAULT", RTLD_DEFAULT)

#endif // TARGET_OS_FAMILY_linux


#ifdef TARGET_OS_FAMILY_bsd

#define VM_ADDRESSES_JVMCI_OS(declare_address, declare_preprocessor_address, declare_function) \
  declare_preprocessor_address("RTLD_DEFAULT", RTLD_DEFAULT)

#endif // TARGET_OS_FAMILY_bsd

#ifndef VM_ADDRESSES_JVMCI_OS
#define VM_ADDRESSES_JVMCI_OS(declare_address, declare_preprocessor_address, declare_function)
#endif

#endif // SHARE_VM_JVMCI_VMSTRUCTS_JVMCI_HPP
