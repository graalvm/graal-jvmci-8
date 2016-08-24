/*
 * Copyright (c) 2013, 2014, Oracle and/or its affiliates. All rights reserved.
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

#define VM_STRUCTS_JVMCI(nonstatic_field, static_field)                       \
  static_field(CompilerToVM::Data,             Klass_vtable_start_offset,              int)                                          \
  static_field(CompilerToVM::Data,             Klass_vtable_length_offset,             int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             Method_extra_stack_entries,             int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             SharedRuntime_ic_miss_stub,             address)                                      \
  static_field(CompilerToVM::Data,             SharedRuntime_handle_wrong_method_stub, address)                                      \
  static_field(CompilerToVM::Data,             SharedRuntime_deopt_blob_unpack,        address)                                      \
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
                                                                                                                                     \
  static_field(CompilerToVM::Data,             cardtable_start_address,                jbyte*)                                       \
  static_field(CompilerToVM::Data,             cardtable_shift,                        int)                                          \
  static_field(CompilerToVM::Data,             g1_young_card,                          int)                                          \
  static_field(CompilerToVM::Data,             dirty_card,                             int)                                          \
                                                                                                                                     \
  static_field(CompilerToVM::Data,             vm_page_size,                           int)                                          \
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
  nonstatic_field(JavaThread,                  _pending_deoptimization,                int)                                          \
  nonstatic_field(JavaThread,                  _pending_failed_speculation,            oop)                                          \
  nonstatic_field(JavaThread,                  _pending_transfer_to_interpreter,       bool)                                         \
  nonstatic_field(JavaThread,                  _jvmci_counters,                        jlong*)                                       \
  nonstatic_field(MethodData,                  _jvmci_ir_size,                         int)                                          \
  nonstatic_field(JVMCIEnv,                    _task,                                  CompileTask*)                                 \
  nonstatic_field(JVMCIEnv,                    _jvmti_can_hotswap_or_post_breakpoint,  bool)                                         \

#define VM_TYPES_JVMCI(declare_type, declare_toplevel_type)                   \
  declare_toplevel_type(narrowKlass)                                          \
  declare_toplevel_type(JVMCIEnv)                                             \
  declare_toplevel_type(CompilerToVM::Data)                                   \

#define VM_INT_CONSTANTS_JVMCI(declare_constant, declare_preprocessor_constant)                   \
  declare_constant(Deoptimization::Reason_unreached0)                                             \
  declare_constant(Deoptimization::Reason_type_checked_inlining)                                  \
  declare_constant(Deoptimization::Reason_optimized_type_check)                                   \
  declare_constant(Deoptimization::Reason_aliasing)                                               \
  declare_constant(Deoptimization::Reason_transfer_to_interpreter)                                \
  declare_constant(Deoptimization::Reason_not_compiled_exception_handler)                         \
  declare_constant(Deoptimization::Reason_unresolved)                                             \
  declare_constant(Deoptimization::Reason_jsr_mismatch)                                           \
  declare_constant(JVMCIEnv::ok)                                                                  \
  declare_constant(JVMCIEnv::dependencies_failed)                                                 \
  declare_constant(JVMCIEnv::dependencies_invalid)                                                \
  declare_constant(JVMCIEnv::cache_full)                                                          \
  declare_constant(JVMCIEnv::code_too_large)                                                      \
  declare_constant(JVMCIRuntime::none)                                                            \
  declare_constant(JVMCIRuntime::by_holder)                                                       \
  declare_constant(JVMCIRuntime::by_full_signature)                                               \
                                                                                                  \
  declare_preprocessor_constant("JVM_ACC_SYNTHETIC", JVM_ACC_SYNTHETIC)                           \
  declare_preprocessor_constant("JVM_RECOGNIZED_FIELD_MODIFIERS", JVM_RECOGNIZED_FIELD_MODIFIERS) \
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
                                                                                                  \
  declare_constant(CodeInstaller::VERIFIED_ENTRY)                                                 \
  declare_constant(CodeInstaller::UNVERIFIED_ENTRY)                                               \
  declare_constant(CodeInstaller::OSR_ENTRY)                                                      \
  declare_constant(CodeInstaller::EXCEPTION_HANDLER_ENTRY)                                        \
  declare_constant(CodeInstaller::DEOPT_HANDLER_ENTRY)                                            \
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
  declare_constant(CodeInstaller::INVOKE_INVALID)                                                 \
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
