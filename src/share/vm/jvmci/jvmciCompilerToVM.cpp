/*
 * Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "code/codeCache.hpp"
#include "code/scopeDesc.hpp"
#include "interpreter/linkResolver.hpp"
#include "memory/oopFactory.hpp"
#include "oops/generateOopMap.hpp"
#include "oops/fieldStreams.hpp"
#include "oops/oop.inline.hpp"
#include "prims/nativeLookup.hpp"
#include "runtime/fieldDescriptor.hpp"
#include "runtime/javaCalls.hpp"
#include "jvmci/jvmciRuntime.hpp"
#include "compiler/abstractCompiler.hpp"
#include "compiler/compileBroker.hpp"
#include "compiler/compileLog.hpp"
#include "compiler/compilerOracle.hpp"
#include "compiler/disassembler.hpp"
#include "jvmci/jvmciCompilerToVM.hpp"
#include "jvmci/jvmciCompiler.hpp"
#include "jvmci/jvmciEnv.hpp"
#include "jvmci/jvmciJavaClasses.hpp"
#include "jvmci/jvmciCodeInstaller.hpp"
#include "gc_implementation/g1/heapRegion.hpp"
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#include "runtime/javaCalls.hpp"
#include "runtime/deoptimization.hpp"
#include "runtime/vframe.hpp"
#include "runtime/vframe_hp.hpp"
#include "runtime/vmStructs.hpp"
#include "utilities/resourceHash.hpp"

class JVMCITraceMark : public StackObj {
  const char* _msg;
 public:
  JVMCITraceMark(const char* msg) {
    _msg = msg;
    if (JVMCITraceLevel >= 1) {
      tty->print_cr(PTR_FORMAT " JVMCITrace-1: Enter %s", p2i(JavaThread::current()), _msg);
    }
  }
  ~JVMCITraceMark() {
    if (JVMCITraceLevel >= 1) {
      tty->print_cr(PTR_FORMAT " JVMCITrace-1: Exit %s", p2i(JavaThread::current()), _msg);
    }
  }
};

// Entry to native method implementation that transitions current thread to '_thread_in_vm'.
#define C2V_VMENTRY(result_type, name, signature)        \
  JNIEXPORT result_type JNICALL c2v_ ## name signature { \
  JVMCITraceMark jtm("CompilerToVM::" #name);            \
  TRACE_CALL(result_type, jvmci_ ## name signature)      \
  JVMCI_VM_ENTRY_MARK;                                   \
  ResourceMark rm;                                       \
  JNI_JVMCIENV(env, true);


#define C2V_END }

int CompilerToVM::Data::Klass_vtable_start_offset;
int CompilerToVM::Data::Klass_vtable_length_offset;

int CompilerToVM::Data::Method_extra_stack_entries;

address CompilerToVM::Data::SharedRuntime_ic_miss_stub;
address CompilerToVM::Data::SharedRuntime_handle_wrong_method_stub;
address CompilerToVM::Data::SharedRuntime_deopt_blob_unpack;
address CompilerToVM::Data::SharedRuntime_deopt_blob_uncommon_trap;

size_t CompilerToVM::Data::ThreadLocalAllocBuffer_alignment_reserve;

CollectedHeap* CompilerToVM::Data::Universe_collectedHeap;
int CompilerToVM::Data::Universe_base_vtable_size;
address CompilerToVM::Data::Universe_narrow_oop_base;
int CompilerToVM::Data::Universe_narrow_oop_shift;
address CompilerToVM::Data::Universe_narrow_klass_base;
int CompilerToVM::Data::Universe_narrow_klass_shift;
void* CompilerToVM::Data::Universe_non_oop_bits;
uintptr_t CompilerToVM::Data::Universe_verify_oop_mask;
uintptr_t CompilerToVM::Data::Universe_verify_oop_bits;

bool       CompilerToVM::Data::_supports_inline_contig_alloc;
HeapWord** CompilerToVM::Data::_heap_end_addr;
HeapWord** CompilerToVM::Data::_heap_top_addr;
int CompilerToVM::Data::_max_oop_map_stack_offset;

jbyte* CompilerToVM::Data::cardtable_start_address;
int CompilerToVM::Data::cardtable_shift;
int CompilerToVM::Data::g1_young_card;
int CompilerToVM::Data::dirty_card;

int CompilerToVM::Data::vm_page_size;

int CompilerToVM::Data::sizeof_vtableEntry = sizeof(vtableEntry);
int CompilerToVM::Data::sizeof_ExceptionTableElement = sizeof(ExceptionTableElement);
int CompilerToVM::Data::sizeof_LocalVariableTableElement = sizeof(LocalVariableTableElement);
int CompilerToVM::Data::sizeof_ConstantPool = sizeof(ConstantPool);
int CompilerToVM::Data::sizeof_SymbolPointer = sizeof(Symbol*);
int CompilerToVM::Data::sizeof_narrowKlass = sizeof(narrowKlass);
int CompilerToVM::Data::sizeof_arrayOopDesc = sizeof(arrayOopDesc);
int CompilerToVM::Data::sizeof_BasicLock = sizeof(BasicLock);

address CompilerToVM::Data::CodeCache_low_bound;
address CompilerToVM::Data::CodeCache_high_bound;

address CompilerToVM::Data::dsin;
address CompilerToVM::Data::dcos;
address CompilerToVM::Data::dtan;
address CompilerToVM::Data::dexp;
address CompilerToVM::Data::dlog;
address CompilerToVM::Data::dlog10;
address CompilerToVM::Data::dpow;

address CompilerToVM::Data::symbol_init;
address CompilerToVM::Data::symbol_clinit;

void CompilerToVM::Data::initialize(JVMCI_TRAPS) {
  Klass_vtable_start_offset = InstanceKlass::vtable_start_offset() * HeapWordSize;
  Klass_vtable_length_offset = InstanceKlass::vtable_length_offset() * HeapWordSize;
  Method_extra_stack_entries = Method::extra_stack_entries();

  SharedRuntime_ic_miss_stub = SharedRuntime::get_ic_miss_stub();
  SharedRuntime_handle_wrong_method_stub = SharedRuntime::get_handle_wrong_method_stub();
  SharedRuntime_deopt_blob_unpack = SharedRuntime::deopt_blob()->unpack();
  SharedRuntime_deopt_blob_uncommon_trap = SharedRuntime::deopt_blob()->uncommon_trap();

  ThreadLocalAllocBuffer_alignment_reserve = ThreadLocalAllocBuffer::alignment_reserve();

  Universe_collectedHeap = Universe::heap();
  Universe_base_vtable_size = Universe::base_vtable_size();
  Universe_narrow_oop_base = Universe::narrow_oop_base();
  Universe_narrow_oop_shift = Universe::narrow_oop_shift();
  Universe_narrow_klass_base = Universe::narrow_klass_base();
  Universe_narrow_klass_shift = Universe::narrow_klass_shift();
  Universe_non_oop_bits = Universe::non_oop_word();
  Universe_verify_oop_mask = Universe::verify_oop_mask();
  Universe_verify_oop_bits = Universe::verify_oop_bits();

  _supports_inline_contig_alloc = Universe::heap()->supports_inline_contig_alloc();
  _heap_end_addr = _supports_inline_contig_alloc ? Universe::heap()->end_addr() : (HeapWord**) -1;
  _heap_top_addr = _supports_inline_contig_alloc ? Universe::heap()->top_addr() : (HeapWord**) -1;

  _max_oop_map_stack_offset = (OopMapValue::register_mask - VMRegImpl::stack2reg(0)->value()) * VMRegImpl::stack_slot_size;
  int max_oop_map_stack_index = _max_oop_map_stack_offset / VMRegImpl::stack_slot_size;
  assert(OopMapValue::legal_vm_reg_name(VMRegImpl::stack2reg(max_oop_map_stack_index)), "should be valid");
  assert(!OopMapValue::legal_vm_reg_name(VMRegImpl::stack2reg(max_oop_map_stack_index + 1)), "should be invalid");

  g1_young_card = G1SATBCardTableModRefBS::g1_young_card_val();
  dirty_card = CardTableModRefBS::dirty_card_val();

  CodeCache_low_bound = CodeCache::low_bound();
  CodeCache_high_bound = CodeCache::high_bound();

  symbol_init = (address) vmSymbols::object_initializer_name();
  symbol_clinit = (address) vmSymbols::class_initializer_name();

  BarrierSet* bs = Universe::heap()->barrier_set();
  switch (bs->kind()) {
  case BarrierSet::CardTableModRef:
  case BarrierSet::CardTableExtension:
  case BarrierSet::G1SATBCT:
  case BarrierSet::G1SATBCTLogging: {
    jbyte* base = ((CardTableModRefBS*) bs)->byte_map_base;
    assert(base != 0, "unexpected byte_map_base");
    cardtable_start_address = base;
    cardtable_shift = CardTableModRefBS::card_shift;
    break;
  }
  case BarrierSet::ModRef:
    cardtable_start_address = 0;
    cardtable_shift = 0;
    // No post barriers
    break;
  default:
    JVMCI_ERROR("Unsupported BarrierSet kind %d", bs->kind());
    break;
  }

  vm_page_size = os::vm_page_size();

#define SET_TRIGFUNC(name)                                 \
  name = StubRoutines::name();                             \
  if (name == NULL) {                                      \
	  name = CAST_FROM_FN_PTR(address, SharedRuntime::name); \
  }                                                        \
  assert(name != NULL, "could not initialize " #name)

  SET_TRIGFUNC(dsin);
  SET_TRIGFUNC(dcos);
  SET_TRIGFUNC(dtan);
  SET_TRIGFUNC(dexp);
  SET_TRIGFUNC(dlog10);
  SET_TRIGFUNC(dlog);
  SET_TRIGFUNC(dpow);
#undef SET_TRIGFUNC
}

JVMCIObjectArray CompilerToVM::initialize_intrinsics(JVMCI_TRAPS) {
  JVMCIObjectArray vmIntrinsics = JVMCIENV->new_VMIntrinsicMethod_array(vmIntrinsics::ID_LIMIT - 1, JVMCI_CHECK_NULL);
  int index = 0;
  // The intrinsics for a class are usually adjacent to each other.
  // When they are, the string for the class name can be reused.
  vmSymbols::SID kls_sid = vmSymbols::NO_SID;
  JVMCIObject kls_str;
#define SID_ENUM(n) vmSymbols::VM_SYMBOL_ENUM_NAME(n)
#define VM_SYMBOL_TO_STRING(s) \
  JVMCIENV->create_string(vmSymbols::symbol_at(SID_ENUM(s)), JVMCI_CHECK_NULL);
#define VM_INTRINSIC_INFO(id, kls, name, sig, ignore_fcode) {             \
    if (kls_sid != SID_ENUM(kls)) {                                       \
      kls_str = VM_SYMBOL_TO_STRING(kls);                                 \
      kls_sid = SID_ENUM(kls);                                            \
    }                                                                     \
    JVMCIObject name_str = VM_SYMBOL_TO_STRING(name);                    \
    JVMCIObject sig_str = VM_SYMBOL_TO_STRING(sig);                      \
    JVMCIObject vmIntrinsicMethod = JVMCIENV->new_VMIntrinsicMethod(kls_str, name_str, sig_str, (jint) vmIntrinsics::id, JVMCI_CHECK_NULL); \
    JVMCIENV->put_object_at(vmIntrinsics, index++, vmIntrinsicMethod);   \
  }

  VM_INTRINSICS_DO(VM_INTRINSIC_INFO, VM_SYMBOL_IGNORE, VM_SYMBOL_IGNORE, VM_SYMBOL_IGNORE, VM_ALIAS_IGNORE)
#undef SID_ENUM
#undef VM_SYMBOL_TO_STRING
#undef VM_INTRINSIC_INFO
  assert(index == vmIntrinsics::ID_LIMIT - 1, "must be");

  return vmIntrinsics;
}

/**
 * The set of VM flags known to be used.
 */
#define PREDEFINED_CONFIG_FLAGS(do_bool_flag, do_intx_flag, do_uintx_flag) \
  do_intx_flag(AllocateInstancePrefetchLines)                              \
  do_intx_flag(AllocatePrefetchDistance)                                   \
  do_intx_flag(AllocatePrefetchInstr)                                      \
  do_intx_flag(AllocatePrefetchLines)                                      \
  do_intx_flag(AllocatePrefetchStepSize)                                   \
  do_intx_flag(AllocatePrefetchStyle)                                      \
  do_intx_flag(BciProfileWidth)                                            \
  do_bool_flag(BootstrapJVMCI)                                             \
  do_bool_flag(CITime)                                                     \
  do_bool_flag(CITimeEach)                                                 \
  do_uintx_flag(CodeCacheSegmentSize)                                      \
  do_intx_flag(CodeEntryAlignment)                                         \
  do_bool_flag(CompactFields)                                              \
  NOT_PRODUCT(do_intx_flag(CompileTheWorldStartAt))                        \
  NOT_PRODUCT(do_intx_flag(CompileTheWorldStopAt))                         \
  do_intx_flag(ContendedPaddingWidth)                                      \
  do_bool_flag(DontCompileHugeMethods)                                     \
  do_bool_flag(EnableContended)                                            \
  do_intx_flag(FieldsAllocationStyle)                                      \
  do_bool_flag(FoldStableValues)                                           \
  do_bool_flag(ForceUnreachable)                                           \
  do_intx_flag(HugeMethodLimit)                                            \
  do_bool_flag(Inline)                                                     \
  do_intx_flag(JVMCICounterSize)                                           \
  do_bool_flag(EagerJVMCI)                                                 \
  do_bool_flag(JVMCIPrintProperties)                                       \
  do_bool_flag(JVMCIUseFastLocking)                                        \
  do_intx_flag(MethodProfileWidth)                                         \
  do_intx_flag(ObjectAlignmentInBytes)                                     \
  do_bool_flag(PrintInlining)                                              \
  do_bool_flag(ReduceInitialCardMarks)                                     \
  do_bool_flag(RestrictContended)                                          \
  /* do_intx_flag(StackReservedPages) JDK 9 */                             \
  do_intx_flag(StackShadowPages)                                           \
  do_bool_flag(TLABStats)                                                  \
  do_uintx_flag(TLABWasteIncrement)                                        \
  do_intx_flag(TypeProfileWidth)                                           \
  do_bool_flag(UseAESIntrinsics)                                           \
  X86_ONLY(do_intx_flag(UseAVX))                                           \
  do_bool_flag(UseBiasedLocking)                                           \
  do_bool_flag(UseCRC32Intrinsics)                                         \
  do_bool_flag(UseCompressedClassPointers)                                 \
  do_bool_flag(UseCompressedOops)                                          \
  X86_ONLY(do_bool_flag(UseCountLeadingZerosInstruction))                  \
  X86_ONLY(do_bool_flag(UseCountTrailingZerosInstruction))                 \
  do_bool_flag(UseConcMarkSweepGC)                                         \
  do_bool_flag(UseG1GC)                                                    \
  do_bool_flag(UseParallelGC)                                              \
  do_bool_flag(UseParallelOldGC)                                           \
  do_bool_flag(UseParNewGC)                                                \
  do_bool_flag(UseSerialGC)                                                \
  COMPILER2_PRESENT(do_bool_flag(UseMontgomeryMultiplyIntrinsic))          \
  COMPILER2_PRESENT(do_bool_flag(UseMontgomerySquareIntrinsic))            \
  COMPILER2_PRESENT(do_bool_flag(UseMulAddIntrinsic))                      \
  COMPILER2_PRESENT(do_bool_flag(UseMultiplyToLenIntrinsic))               \
  do_bool_flag(UsePopCountInstruction)                                     \
  do_bool_flag(UseSHA1Intrinsics)                                          \
  do_bool_flag(UseSHA256Intrinsics)                                        \
  do_bool_flag(UseSHA512Intrinsics)                                        \
  do_intx_flag(UseSSE)                                                     \
  COMPILER2_PRESENT(do_bool_flag(UseSquareToLenIntrinsic))                 \
  do_bool_flag(UseStackBanging)                                            \
  do_bool_flag(UseTLAB)                                                    \
  do_bool_flag(VerifyOops)                                                 \

#define BOXED_BOOLEAN(name, value) name = ((jboolean)(value) ? boxedTrue : boxedFalse)
#define BOXED_DOUBLE(name, value) do { jvalue p; p.d = (jdouble) (value); name = JVMCIENV->create_box(T_DOUBLE, &p, JVMCI_CHECK_NULL);} while(0)
#define BOXED_LONG(name, value) \
  do { \
    jvalue p; p.j = (jlong) (value); \
    JVMCIObject* e = longs.get(p.j); \
    if (e == NULL) { \
      JVMCIObject h = JVMCIENV->create_box(T_LONG, &p, JVMCI_CHECK_NULL); \
      longs.put(p.j, h); \
      name = h; \
    } else { \
      name = (*e); \
    } \
  } while (0)

#define CSTRING_TO_JSTRING(name, value) \
  JVMCIObject name; \
  do { \
    if (value != NULL) { \
      JVMCIObject* e = strings.get(value); \
      if (e == NULL) { \
        JVMCIObject h = JVMCIENV->create_string(value, JVMCI_CHECK_NULL); \
        strings.put(value, h); \
        name = h; \
      } else { \
        name = (*e); \
      } \
    } \
  } while (0)

C2V_VMENTRY(jobjectArray, readConfiguration, (JNIEnv *env))
  HandleMark hm;

  // Used to canonicalize Long and String values.
  ResourceHashtable<jlong, JVMCIObject> longs;
  ResourceHashtable<const char*, JVMCIObject, &CompilerToVM::cstring_hash, &CompilerToVM::cstring_equals> strings;

  jvalue prim;
  prim.z = true;  JVMCIObject boxedTrue =  JVMCIENV->create_box(T_BOOLEAN, &prim, JVMCI_CHECK_NULL);
  prim.z = false; JVMCIObject boxedFalse = JVMCIENV->create_box(T_BOOLEAN, &prim, JVMCI_CHECK_NULL);

  CompilerToVM::Data::initialize(JVMCI_CHECK_NULL);

  JVMCIENV->VMField_initialize(JVMCI_CHECK_NULL);
  JVMCIENV->VMFlag_initialize(JVMCI_CHECK_NULL);
  JVMCIENV->VMIntrinsicMethod_initialize(JVMCI_CHECK_NULL);

  int len = VMStructs::localHotSpotVMStructs_count();
  JVMCIObjectArray vmFields = JVMCIENV->new_VMField_array(len, JVMCI_CHECK_NULL);
  for (int i = 0; i < len ; i++) {
    VMStructEntry vmField = VMStructs::localHotSpotVMStructs[i];
    size_t name_buf_len = strlen(vmField.typeName) + strlen(vmField.fieldName) + 2 /* "::" */;
    char* name_buf = NEW_RESOURCE_ARRAY_IN_THREAD(THREAD, char, name_buf_len + 1);
    sprintf(name_buf, "%s::%s", vmField.typeName, vmField.fieldName);
    CSTRING_TO_JSTRING(name, name_buf);
    CSTRING_TO_JSTRING(type, vmField.typeString);
    JVMCIObject box;
    if (vmField.isStatic && vmField.typeString != NULL) {
      if (strcmp(vmField.typeString, "bool") == 0) {
        BOXED_BOOLEAN(box, *(jbyte*) vmField.address);
        assert(box.is_non_null(), "must have a box");
      } else if (strcmp(vmField.typeString, "int") == 0 ||
                 strcmp(vmField.typeString, "jint") == 0) {
        BOXED_LONG(box, *(jint*) vmField.address);
        assert(box.is_non_null(), "must have a box");
      } else if (strcmp(vmField.typeString, "uint64_t") == 0) {
        BOXED_LONG(box, *(uint64_t*) vmField.address);
        assert(box.is_non_null(), "must have a box");
      } else if (strcmp(vmField.typeString, "address") == 0 ||
                 strcmp(vmField.typeString, "intptr_t") == 0 ||
                 strcmp(vmField.typeString, "uintptr_t") == 0 ||
                 strcmp(vmField.typeString, "size_t") == 0 ||
                 // All foo* types are addresses.
                 vmField.typeString[strlen(vmField.typeString) - 1] == '*') {
        BOXED_LONG(box, *((address*) vmField.address));
        assert(box.is_non_null(), "must have a box");
      }
    }
    JVMCIObject vmFieldObj = JVMCIENV->new_VMField(name, type, vmField.offset, (jlong) vmField.address, box, JVMCI_CHECK_NULL);
    JVMCIENV->put_object_at(vmFields, i, vmFieldObj);
  }

  int ints_len = VMStructs::localHotSpotVMIntConstants_count();
  int longs_len = VMStructs::localHotSpotVMLongConstants_count();
  len = ints_len + longs_len;
  JVMCIObjectArray vmConstants = JVMCIENV->new_Object_array(len * 2, JVMCI_CHECK_NULL);
  int insert = 0;
  for (int i = 0; i < ints_len ; i++) {
    VMIntConstantEntry c = VMStructs::localHotSpotVMIntConstants[i];
    CSTRING_TO_JSTRING(name, c.name);
    JVMCIObject value;
    BOXED_LONG(value, c.value);
    JVMCIENV->put_object_at(vmConstants, insert++, name);
    JVMCIENV->put_object_at(vmConstants, insert++, value);
  }
  for (int i = 0; i < longs_len ; i++) {
    VMLongConstantEntry c = VMStructs::localHotSpotVMLongConstants[i];
    CSTRING_TO_JSTRING(name, c.name);
    JVMCIObject value;
    BOXED_LONG(value, c.value);
    JVMCIENV->put_object_at(vmConstants, insert++, name);
    JVMCIENV->put_object_at(vmConstants, insert++, value);
  }
  assert(insert == len * 2, "must be");

  len = VMStructs::localHotSpotVMAddresses_count();
  JVMCIObjectArray vmAddresses = JVMCIENV->new_Object_array(len * 2, JVMCI_CHECK_NULL);
  for (int i = 0; i < len ; i++) {
    VMAddressEntry a = VMStructs::localHotSpotVMAddresses[i];
    CSTRING_TO_JSTRING(name, a.name);
    JVMCIObject value;
    BOXED_LONG(value, a.value);
    JVMCIENV->put_object_at(vmAddresses, i * 2, name);
    JVMCIENV->put_object_at(vmAddresses, i * 2 + 1, value);
  }

#define COUNT_FLAG(ignore) +1
#ifdef ASSERT
#define CHECK_FLAG(type, name) { \
  Flag* flag = Flag::find_flag(#name, strlen(#name), /*allow_locked*/ true, /* return_flag */ true); \
  assert(flag != NULL, "No such flag named " #name); \
  assert(flag->is_##type(), "Flag " #name " is not of type " #type); \
}
#else
#define CHECK_FLAG(type, name)
#endif

#define ADD_FLAG(type, name, convert) {                                                \
  CHECK_FLAG(type, name)                                                               \
  CSTRING_TO_JSTRING(fname, #name);                                                    \
  CSTRING_TO_JSTRING(ftype, #type);                                                    \
  convert(value, name);                                                                \
  JVMCIObject vmFlagObj = JVMCIENV->new_VMFlag(fname, ftype, value, JVMCI_CHECK_NULL); \
  JVMCIENV->put_object_at(vmFlags, i++, vmFlagObj);                                    \
}
#define ADD_BOOL_FLAG(name)  ADD_FLAG(bool, name, BOXED_BOOLEAN)
#define ADD_INTX_FLAG(name)  ADD_FLAG(intx, name, BOXED_LONG)
#define ADD_UINTX_FLAG(name) ADD_FLAG(uintx, name, BOXED_LONG)

  len = 0 + PREDEFINED_CONFIG_FLAGS(COUNT_FLAG, COUNT_FLAG, COUNT_FLAG);
  JVMCIObjectArray vmFlags = JVMCIENV->new_VMFlag_array(len, JVMCI_CHECK_NULL);
  int i = 0;
  JVMCIObject value;
  PREDEFINED_CONFIG_FLAGS(ADD_BOOL_FLAG, ADD_INTX_FLAG, ADD_UINTX_FLAG)

  JVMCIObjectArray vmIntrinsics = CompilerToVM::initialize_intrinsics(JVMCI_CHECK_NULL);

  JVMCIObjectArray data = JVMCIENV->new_Object_array(5, JVMCI_CHECK_NULL);
  JVMCIENV->put_object_at(data, 0, vmFields);
  JVMCIENV->put_object_at(data, 1, vmConstants);
  JVMCIENV->put_object_at(data, 2, vmAddresses);
  JVMCIENV->put_object_at(data, 3, vmFlags);
  JVMCIENV->put_object_at(data, 4, vmIntrinsics);

  return JVMCIENV->get_jobjectArray(data);

#undef COUNT_FLAG
#undef ADD_FLAG
#undef ADD_BOOL_FLAG
#undef ADD_INTX_FLAG
#undef ADD_UINTX_FLAG
#undef CHECK_FLAG
C2V_END

C2V_VMENTRY(jobject, getFlagValue, (JNIEnv* env, jobject c2vm, jobject name_handle))
#define RETURN_BOXED_LONG(value) jvalue p; p.j = (jlong) (value); JVMCIObject box = JVMCIENV->create_box(T_LONG, &p, JVMCI_CHECK_NULL); return box.as_jobject();
#define RETURN_BOXED_DOUBLE(value) jvalue p; p.d = (jdouble) (value); JVMCIObject box = JVMCIENV->create_box(T_DOUBLE, &p, JVMCI_CHECK_NULL); return box.as_jobject();
  JVMCIObject name = JVMCIENV->wrap(name_handle);
  if (name.is_null()) {
    JVMCI_THROW_NULL(NullPointerException);
  }
  const char* cstring = JVMCIENV->as_utf8_string(name);
  Flag* flag = Flag::find_flag(cstring, strlen(cstring), /* allow_locked */ true, /* return_flag */ true);
  if (flag == NULL) {
    return c2vm;
  }
  if (flag->is_bool()) {
    jvalue prim;
    prim.z = flag->get_bool();
    JVMCIObject box = JVMCIENV->create_box(T_BOOLEAN, &prim, JVMCI_CHECK_NULL);
    return JVMCIENV->get_jobject(box);
  } else if (flag->is_ccstr()) {
    JVMCIObject value = JVMCIENV->create_string(flag->get_ccstr(), JVMCI_CHECK_NULL);
    return JVMCIENV->get_jobject(value);
  } else if (flag->is_intx()) {
    RETURN_BOXED_LONG(flag->get_intx());
/*} else if (flag->is_int()) {
    RETURN_BOXED_LONG(flag->get_int());
  } else if (flag->is_uint()) {
    RETURN_BOXED_LONG(flag->get_uint());*/
  } else if (flag->is_uint64_t()) {
    RETURN_BOXED_LONG(flag->get_uint64_t());
/*} else if (flag->is_size_t()) {
    RETURN_BOXED_LONG(flag->get_size_t());*/
  } else if (flag->is_uintx()) {
    RETURN_BOXED_LONG(flag->get_uintx());
  } else if (flag->is_double()) {
    RETURN_BOXED_DOUBLE(flag->get_double());
  } else {
    JVMCI_ERROR_NULL("VM flag %s has unsupported type %s", flag->_name, flag->_type);
  }
C2V_END

#undef BOXED_LONG
#undef BOXED_DOUBLE
#undef CSTRING_TO_JSTRING

C2V_VMENTRY(jobject, getObjectAtAddress, (JNIEnv* env, jobject c2vm, jlong oop_address))
  if (env != JavaThread::current()->jni_environment()) {
    JVMCI_THROW_MSG_NULL(InternalError, "Only supported when running in HotSpot");
  }
  if (oop_address == 0) {
    JVMCI_THROW_MSG_NULL(InternalError, "Handle must be non-zero");
  }
  oop obj = *((oopDesc**) oop_address);
  if (obj != NULL) {
    obj->verify();
  }
  return JNIHandles::make_local(obj);
C2V_END

C2V_VMENTRY(jbyteArray, getBytecode, (JNIEnv* env, jobject, jobject jvmci_method))
  methodHandle method = JVMCIENV->asMethod(jvmci_method);

  int code_size = method->code_size();
  jbyte* reconstituted_code = NEW_RESOURCE_ARRAY(jbyte, code_size);

  guarantee(method->method_holder()->is_rewritten(), "Method's holder should be rewritten");
  // iterate over all bytecodes and replace non-Java bytecodes

  for (BytecodeStream s(method); s.next() != Bytecodes::_illegal; ) {
    Bytecodes::Code code = s.code();
    Bytecodes::Code raw_code = s.raw_code();
    int bci = s.bci();
    int len = s.instruction_size();

    // Restore original byte code.
    reconstituted_code[bci] =  (jbyte) (s.is_wide()? Bytecodes::_wide : code);
    if (len > 1) {
      memcpy(reconstituted_code + (bci + 1), s.bcp()+1, len-1);
    }

    if (len > 1) {
      // Restore the big-endian constant pool indexes.
      // Cf. Rewriter::scan_method
      switch (code) {
        case Bytecodes::_getstatic:
        case Bytecodes::_putstatic:
        case Bytecodes::_getfield:
        case Bytecodes::_putfield:
        case Bytecodes::_invokevirtual:
        case Bytecodes::_invokespecial:
        case Bytecodes::_invokestatic:
        case Bytecodes::_invokeinterface:
        case Bytecodes::_invokehandle: {
          int cp_index = Bytes::get_native_u2((address) reconstituted_code + (bci + 1));
          Bytes::put_Java_u2((address) reconstituted_code + (bci + 1), (u2) cp_index);
          break;
        }

        case Bytecodes::_invokedynamic:
          int cp_index = Bytes::get_native_u4((address) reconstituted_code + (bci + 1));
          Bytes::put_Java_u4((address) reconstituted_code + (bci + 1), (u4) cp_index);
          break;
      }

      // Not all ldc byte code are rewritten.
      switch (raw_code) {
        case Bytecodes::_fast_aldc: {
          int cpc_index = reconstituted_code[bci + 1] & 0xff;
          int cp_index = method->constants()->object_to_cp_index(cpc_index);
          assert(cp_index < method->constants()->length(), "sanity check");
          reconstituted_code[bci + 1] = (jbyte) cp_index;
          break;
        }

        case Bytecodes::_fast_aldc_w: {
          int cpc_index = Bytes::get_native_u2((address) reconstituted_code + (bci + 1));
          int cp_index = method->constants()->object_to_cp_index(cpc_index);
          assert(cp_index < method->constants()->length(), "sanity check");
          Bytes::put_Java_u2((address) reconstituted_code + (bci + 1), (u2) cp_index);
          break;
        }
      }
    }
  }

  JVMCIPrimitiveArray result = JVMCIENV->new_byteArray(code_size, JVMCI_CHECK_NULL);
  JVMCIENV->copy_bytes_from(reconstituted_code, result, 0, code_size);
  return JVMCIENV->get_jbyteArray(result);
C2V_END

C2V_VMENTRY(jint, getExceptionTableLength, (JNIEnv* env, jobject, jobject jvmci_method))
  methodHandle method = JVMCIENV->asMethod(jvmci_method);
  return method->exception_table_length();
C2V_END

C2V_VMENTRY(jlong, getExceptionTableStart, (JNIEnv* env, jobject, jobject jvmci_method))
  methodHandle method = JVMCIENV->asMethod(jvmci_method);
  if (method->exception_table_length() == 0) {
    return 0L;
  }
  return (jlong) (address) method->exception_table_start();
C2V_END

C2V_VMENTRY(jobject, asResolvedJavaMethod, (JNIEnv* env, jobject, jobject executable_handle))
  if (env != JavaThread::current()->jni_environment()) {
    JVMCI_THROW_MSG_NULL(InternalError, "Only supported when running in HotSpot");
  }

  oop executable = JNIHandles::resolve(executable_handle);
  oop mirror = NULL;
  int slot = 0;

  if (executable->klass() == SystemDictionary::reflect_Constructor_klass()) {
    mirror = java_lang_reflect_Constructor::clazz(executable);
    slot = java_lang_reflect_Constructor::slot(executable);
  } else {
    assert(executable->klass() == SystemDictionary::reflect_Method_klass(), "wrong type");
    mirror = java_lang_reflect_Method::clazz(executable);
    slot = java_lang_reflect_Method::slot(executable);
  }
  Klass* holder = java_lang_Class::as_Klass(mirror);
  methodHandle method = InstanceKlass::cast(holder)->method_with_idnum(slot);
  JVMCIObject result = JVMCIENV->get_jvmci_method(method, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
}

C2V_VMENTRY(jobject, getResolvedJavaMethod, (JNIEnv* env, jobject, jobject base, jlong offset))
  methodHandle method;
  JVMCIObject base_object = JVMCIENV->wrap(base);
  if (base_object.is_null()) {
    method = *((Method**)(offset));
  } else if (JVMCIENV->isa_HotSpotObjectConstantImpl(base_object)) {
    oop obj = JVMCIENV->asConstant(base_object, JVMCI_CHECK_NULL);
    if (obj->is_a(SystemDictionary::MemberName_klass())) {
      method = (Method*) (intptr_t) obj->long_field(offset);
    }
  } else if (JVMCIENV->isa_HotSpotResolvedJavaMethodImpl(base_object)) {
    method = JVMCIENV->asMethod(base_object);
  }
  if (method.is_null()) {
    JVMCI_THROW_MSG_NULL(IllegalArgumentException, err_msg("Unexpected type: %s", JVMCIENV->klass_name(base_object)));
  }
  assert (method.is_null() || method->is_method(), "invalid read");
  JVMCIObject result = JVMCIENV->get_jvmci_method(method, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
}

C2V_VMENTRY(jobject, getConstantPool, (JNIEnv* env, jobject, jobject object_handle))
  constantPoolHandle cp;
  JVMCIObject object = JVMCIENV->wrap(object_handle);
  if (object.is_null()) {
    JVMCI_THROW_NULL(NullPointerException);
  }
  if (JVMCIENV->isa_HotSpotResolvedJavaMethodImpl(object)) {
    cp = JVMCIENV->asMethod(object)->constMethod()->constants();
  } else if (JVMCIENV->isa_HotSpotResolvedObjectTypeImpl(object)) {
    cp = InstanceKlass::cast(JVMCIENV->asKlass(object))->constants();
  } else {
    JVMCI_THROW_MSG_NULL(IllegalArgumentException,
                err_msg("Unexpected type: %s", JVMCIENV->klass_name(object)));
  }
  assert(!cp.is_null(), "npe");

  JVMCIObject result = JVMCIENV->get_jvmci_constant_pool(cp, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
}

C2V_VMENTRY(jobject, getResolvedJavaType0, (JNIEnv* env, jobject, jobject base, jlong offset, jboolean compressed))
  KlassHandle klass;
  JVMCIObject base_object = JVMCIENV->wrap(base);
  jlong base_address = 0;
  if (base_object.is_non_null() && offset == oopDesc::klass_offset_in_bytes()) {
    // klass = JVMCIENV->unhandle(base_object)->klass();
    if (JVMCIENV->isa_HotSpotObjectConstantImpl(base_object)) {
      oop base_oop = JVMCIENV->asConstant(base_object, JVMCI_CHECK_NULL);
      klass = base_oop->klass();
    } else {
      assert(false, "What types are we actually expecting here?");
    }
  } else if (!compressed) {
    if (base_object.is_non_null()) {
      if (JVMCIENV->isa_HotSpotResolvedJavaMethodImpl(base_object)) {
        base_address = (intptr_t) JVMCIENV->asMethod(base_object);
      } else if (JVMCIENV->isa_HotSpotConstantPool(base_object)) {
        base_address = (intptr_t) JVMCIENV->asConstantPool(base_object);
      } else if (JVMCIENV->isa_HotSpotResolvedObjectTypeImpl(base_object)) {
        base_address = (intptr_t) JVMCIENV->asKlass(base_object);
      } else if (JVMCIENV->isa_HotSpotObjectConstantImpl(base_object)) {
        oop base_oop = JVMCIENV->asConstant(base_object, JVMCI_CHECK_NULL);
        if (base_oop->is_a(SystemDictionary::Class_klass())) {
          base_address = (jlong) (address) base_oop;
        }
      }
      if (base_address == 0) {
        JVMCI_THROW_MSG_NULL(IllegalArgumentException,
                    err_msg("Unexpected arguments: %s " JLONG_FORMAT " %s", JVMCIENV->klass_name(base_object), offset, compressed ? "true" : "false"));
      }
    }
    klass = *((Klass**) (intptr_t) (base_address + offset));
  } else {
    JVMCI_THROW_MSG_NULL(IllegalArgumentException,
                err_msg("Unexpected arguments: %s " JLONG_FORMAT " %s",
                        base_object.is_non_null() ? JVMCIENV->klass_name(base_object) : "null",
                        offset, compressed ? "true" : "false"));
  }
  assert (klass.is_null() || klass->is_klass(), "invalid read");
  JVMCIObject result = JVMCIENV->get_jvmci_type(klass, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
}

C2V_VMENTRY(jobject, findUniqueConcreteMethod, (JNIEnv* env, jobject, jobject jvmci_type, jobject jvmci_method))
  methodHandle method = JVMCIENV->asMethod(jvmci_method);
  KlassHandle holder = JVMCIENV->asKlass(jvmci_type);
  if (holder->is_interface()) {
    JVMCI_THROW_MSG_NULL(InternalError, err_msg("Interface %s should be handled in Java code", holder->external_name()));
  }

  methodHandle ucm;
  {
    MutexLocker locker(Compile_lock);
    ucm = Dependencies::find_unique_concrete_method(holder(), method());
  }
  JVMCIObject result = JVMCIENV->get_jvmci_method(ucm, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
C2V_END

C2V_VMENTRY(jobject, getImplementor, (JNIEnv* env, jobject, jobject jvmci_type))
  Klass* klass = JVMCIENV->asKlass(jvmci_type);
  if (!klass->is_interface()) {
    THROW_MSG_0(vmSymbols::java_lang_IllegalArgumentException(),
        err_msg("Expected interface type, got %s", klass->external_name()));
  }
  InstanceKlass* iklass = InstanceKlass::cast(klass);
  JVMCIObject implementor = JVMCIENV->get_jvmci_type(iklass->implementor(), JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(implementor);
C2V_END

C2V_VMENTRY(jboolean, methodIsIgnoredBySecurityStackWalk,(JNIEnv* env, jobject, jobject jvmci_method))
  methodHandle method = JVMCIENV->asMethod(jvmci_method);
  return method->is_ignored_by_security_stack_walk();
C2V_END

C2V_VMENTRY(jboolean, isCompilable,(JNIEnv* env, jobject, jobject jvmci_method))
  methodHandle method = JVMCIENV->asMethod(jvmci_method);
  // Skip redefined methods
  if (method->is_old()) {
    return false;
  }
  return !method->is_not_compilable(CompLevel_full_optimization);
C2V_END

C2V_VMENTRY(jboolean, hasNeverInlineDirective,(JNIEnv* env, jobject, jobject jvmci_method))
  methodHandle method = JVMCIENV->asMethod(jvmci_method);
  return !Inline || CompilerOracle::should_not_inline(method) || method->dont_inline();
C2V_END

C2V_VMENTRY(jboolean, shouldInlineMethod,(JNIEnv* env, jobject, jobject jvmci_method))
  methodHandle method = JVMCIENV->asMethod(jvmci_method);
  return CompilerOracle::should_inline(method) || method->force_inline();
C2V_END

C2V_VMENTRY(jobject, lookupType, (JNIEnv* env, jobject, jstring jname, jclass accessing_class, jboolean resolve))
  JVMCIObject name = JVMCIENV->wrap(jname);
  const char* str = JVMCIENV->as_utf8_string(name);
  TempNewSymbol class_name = SymbolTable::new_symbol(str, CHECK_NULL);

  if (class_name->utf8_length() <= 1) {
    JVMCI_THROW_MSG_0(InternalError, err_msg("Primitive type %s should be handled in Java code", class_name->as_C_string()));
  }

  Klass* resolved_klass = NULL;
  Handle class_loader;
  Handle protection_domain;
  Klass* accessing_klass = NULL;
  if (accessing_class != NULL) {
    accessing_klass = JVMCIENV->asKlass(accessing_class);
    class_loader = accessing_klass->class_loader();
    protection_domain = accessing_klass->protection_domain();
  }

  if (resolve) {
    resolved_klass = SystemDictionary::resolve_or_null(class_name, class_loader, protection_domain, CHECK_0);
  } else {
    if (class_name->byte_at(0) == 'L' &&
      class_name->byte_at(class_name->utf8_length()-1) == ';') {
      // This is a name from a signature.  Strip off the trimmings.
      // Call recursive to keep scope of strippedsym.
      TempNewSymbol strippedsym = SymbolTable::new_symbol(class_name->as_utf8()+1,
                                                          class_name->utf8_length()-2,
                                                          CHECK_0);
      resolved_klass = SystemDictionary::find(strippedsym, class_loader, protection_domain, CHECK_0);
    } else if (FieldType::is_array(class_name)) {
      FieldArrayInfo fd;
      // dimension and object_key in FieldArrayInfo are assigned as a side-effect
      // of this call
      BasicType t = FieldType::get_array_info(class_name, fd, CHECK_0);
      if (t == T_OBJECT) {
        TempNewSymbol strippedsym = SymbolTable::new_symbol(class_name->as_utf8()+1+fd.dimension(),
                                                            class_name->utf8_length()-2-fd.dimension(),
                                                            CHECK_0);
        // naked oop "k" is OK here -- we assign back into it
        resolved_klass = SystemDictionary::find(strippedsym,
                                                             class_loader,
                                                             protection_domain,
                                                             CHECK_0);
        if (resolved_klass != NULL) {
          resolved_klass = resolved_klass->array_klass(fd.dimension(), CHECK_0);
        }
      } else {
        resolved_klass = Universe::typeArrayKlassObj(t);
        resolved_klass = TypeArrayKlass::cast(resolved_klass)->array_klass(fd.dimension(), CHECK_0);
      }
    } else {
      resolved_klass = SystemDictionary::find(class_name, class_loader, protection_domain, CHECK_0);
    }
  }
  JVMCIObject result = JVMCIENV->get_jvmci_type(resolved_klass, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
C2V_END

C2V_VMENTRY(jobject, lookupClass, (JNIEnv* env, jobject, jclass mirror))
  if (env != JavaThread::current()->jni_environment()) {
    JVMCI_THROW_MSG_NULL(InternalError, "Only supported when running in HotSpot");
  }
  if (mirror == NULL) {
    return NULL;
  }
  Klass* klass = java_lang_Class::as_Klass(JNIHandles::resolve(mirror));
  if (klass == NULL) {
    JVMCI_THROW_MSG_NULL(IllegalArgumentException, "Primitive classes are unsupported");
  }
  JVMCIObject result = JVMCIENV->get_jvmci_type(klass, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
}

C2V_VMENTRY(jobject, resolveConstantInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint index))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  oop result = cp->resolve_constant_at(index, THREAD);
  if (HAS_PENDING_EXCEPTION) {
    JVMCIENV->throw_pending_hotspot_exception(CHECK_NULL);
  }
  return JVMCIENV->get_jobject(JVMCIENV->get_object_constant(result));
C2V_END

C2V_VMENTRY(jobject, resolvePossiblyCachedConstantInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint index))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  oop result = cp->resolve_possibly_cached_constant_at(index, JVMCI_RETHROW_CHECK_NULL);
  return JVMCIENV->get_jobject(JVMCIENV->get_object_constant(result));
C2V_END

C2V_VMENTRY(jint, lookupNameAndTypeRefIndexInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint index))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  return cp->name_and_type_ref_index_at(index);
C2V_END

C2V_VMENTRY(jobject, lookupNameInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint which))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  JVMCIObject sym = JVMCIENV->create_string(cp->name_ref_at(which), JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(sym);
C2V_END

C2V_VMENTRY(jobject, lookupSignatureInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint which))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  JVMCIObject sym = JVMCIENV->create_string(cp->signature_ref_at(which), JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(sym);
C2V_END

C2V_VMENTRY(jint, lookupKlassRefIndexInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint index))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  return cp->klass_ref_index_at(index);
C2V_END

C2V_VMENTRY(jobject, resolveTypeInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint index))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  Klass* resolved_klass = cp->klass_at(index, JVMCI_RETHROW_CHECK_NULL);
  if (resolved_klass->oop_is_instance()) {
    InstanceKlass::cast(resolved_klass)->link_class_or_fail(THREAD);
  }
  JVMCIObject klass = JVMCIENV->get_jvmci_type(resolved_klass, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(klass);
C2V_END

C2V_VMENTRY(jobject, lookupKlassInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint index, jbyte opcode))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  KlassHandle loading_klass(cp->pool_holder());
  bool is_accessible = false;
  KlassHandle klass = JVMCIRuntime::get_klass_by_index(cp, index, is_accessible, loading_klass);
  Symbol* symbol = NULL;
  if (klass.is_null()) {
    // We have to lock the cpool to keep the oop from being resolved
    // while we are accessing it.
    MonitorLockerEx ml(cp->lock());
    constantTag tag = cp->tag_at(index);
    if (tag.is_klass()) {
      // The klass has been inserted into the constant pool
      // very recently.
      klass = cp->resolved_klass_at(index);
    } else if (tag.is_symbol()) {
      symbol = cp->symbol_at(index);
    } else {
      assert(cp->tag_at(index).is_unresolved_klass(), "wrong tag");
      symbol = cp->unresolved_klass_at(index);
    }
  }
  JVMCIObject result;
  if (!klass.is_null()) {
    result = JVMCIENV->get_jvmci_type(klass, JVMCI_CHECK_NULL);
  } else {
    result = JVMCIENV->create_string(symbol, JVMCI_CHECK_NULL);
  }
  return JVMCIENV->get_jobject(result);
C2V_END

C2V_VMENTRY(jobject, lookupAppendixInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint index))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  oop appendix_oop = ConstantPool::appendix_at_if_loaded(cp, index);
  return JVMCIENV->get_jobject(JVMCIENV->get_object_constant(appendix_oop));
C2V_END

C2V_VMENTRY(jobject, lookupMethodInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint index, jbyte opcode))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  instanceKlassHandle pool_holder(cp->pool_holder());
  Bytecodes::Code bc = (Bytecodes::Code) (((int) opcode) & 0xFF);
  methodHandle method = JVMCIRuntime::get_method_by_index(cp, index, bc, pool_holder);
  JVMCIObject result = JVMCIENV->get_jvmci_method(method, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
C2V_END

C2V_VMENTRY(jint, constantPoolRemapInstructionOperandFromCache, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint index))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  return cp->remap_instruction_operand_from_cache(index);
C2V_END

C2V_VMENTRY(jobject, resolveFieldInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint index, jbyte opcode, jintArray info_handle))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  Bytecodes::Code code = (Bytecodes::Code)(((int) opcode) & 0xFF);
  fieldDescriptor fd;
  LinkResolver::resolve_field_access(fd, cp, index, Bytecodes::java_code(code), true, false, CHECK_0);
  JVMCIPrimitiveArray info = JVMCIENV->wrap(info_handle);
  if (info.is_null() || JVMCIENV->get_length(info) != 3) {
    JVMCI_ERROR_NULL("info must not be null and have a length of 3");
  }
  JVMCIENV->put_int_at(info, 0, fd.access_flags().as_int());
  JVMCIENV->put_int_at(info, 1, fd.offset());
  JVMCIENV->put_int_at(info, 2, fd.index());
  JVMCIObject field_holder = JVMCIENV->get_jvmci_type(fd.field_holder(), JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(field_holder);
C2V_END

C2V_VMENTRY(jint, getVtableIndexForInterfaceMethod, (JNIEnv* env, jobject, jobject jvmci_type, jobject jvmci_method))
  Klass* klass = JVMCIENV->asKlass(jvmci_type);
  Method* method = JVMCIENV->asMethod(jvmci_method);
  if (klass->is_interface()) {
    JVMCI_THROW_MSG_0(InternalError, err_msg("Interface %s should be handled in Java code", klass->external_name()));
  }
  if (!method->method_holder()->is_interface()) {
    JVMCI_THROW_MSG_0(InternalError, err_msg("Method %s is not held by an interface, this case should be handled in Java code", method->name_and_sig_as_C_string()));
  }
  if (!klass->oop_is_instance()) {
    JVMCI_THROW_MSG_0(InternalError, err_msg("Class %s must be instance klass", klass->external_name()));
  }
  if (!InstanceKlass::cast(klass)->is_linked()) {
    JVMCI_THROW_MSG_0(InternalError, err_msg("Class %s must be linked", klass->external_name()));
  }
  return LinkResolver::vtable_index_of_interface_method(klass, method);
C2V_END

C2V_VMENTRY(jobject, resolveMethod, (JNIEnv* env, jobject, jobject receiver_jvmci_type, jobject jvmci_method, jobject caller_jvmci_type))
  KlassHandle recv_klass = JVMCIENV->asKlass(receiver_jvmci_type);
  KlassHandle caller_klass = JVMCIENV->asKlass(caller_jvmci_type);
  methodHandle method = JVMCIENV->asMethod(jvmci_method);

  KlassHandle h_resolved   (THREAD, method->method_holder());
  Symbol* h_name      = method->name();
  Symbol* h_signature = method->signature();

  vmIntrinsics::ID iid = method()->intrinsic_id();
  if (MethodHandles::is_signature_polymorphic(iid) && MethodHandles::is_signature_polymorphic_intrinsic(iid)) {
      // Signature polymorphic methods are already resolved, JVMCI just returns NULL in this case.
      return NULL;
  }
  methodHandle m;
  // Only do exact lookup if receiver klass has been linked.  Otherwise,
  // the vtable has not been setup, and the LinkResolver will fail.
  if (recv_klass->oop_is_array() ||
      InstanceKlass::cast(recv_klass())->is_linked() && !recv_klass->is_interface()) {
    bool check_access = true;
    if (h_resolved->is_interface()) {
      m = LinkResolver::resolve_interface_call_or_null(recv_klass, h_resolved, h_name, h_signature, caller_klass, check_access);
    } else {
      m = LinkResolver::resolve_virtual_call_or_null(recv_klass, h_resolved, h_name, h_signature, caller_klass, check_access);
    }
  }

  if (m.is_null()) {
    // Return NULL only if there was a problem with lookup (uninitialized class, etc.)
    return NULL;
  }

  JVMCIObject result = JVMCIENV->get_jvmci_method(m, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
C2V_END

C2V_VMENTRY(jboolean, hasFinalizableSubclass,(JNIEnv* env, jobject, jobject jvmci_type))
  Klass* klass = JVMCIENV->asKlass(jvmci_type);
  assert(klass != NULL, "method must not be called for primitive types");
  return Dependencies::find_finalizable_subclass(klass) != NULL;
C2V_END

C2V_VMENTRY(jobject, getClassInitializer, (JNIEnv* env, jobject, jobject jvmci_type))
  Klass* klass = JVMCIENV->asKlass(jvmci_type);
  if (!klass->oop_is_instance()) {
    return NULL;
  }
  InstanceKlass* iklass = InstanceKlass::cast(klass);
  JVMCIObject result = JVMCIENV->get_jvmci_method(iklass->class_initializer(), JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
C2V_END

C2V_VMENTRY(jlong, getMaxCallTargetOffset, (JNIEnv* env, jobject, jlong addr))
  address target_addr = (address) addr;
  if (target_addr != 0x0) {
    int64_t off_low = (int64_t)target_addr - ((int64_t)CodeCache::low_bound() + sizeof(int));
    int64_t off_high = (int64_t)target_addr - ((int64_t)CodeCache::high_bound() + sizeof(int));
    return MAX2(ABS(off_low), ABS(off_high));
  }
  return -1;
C2V_END

C2V_VMENTRY(void, setNotInlinableOrCompilable,(JNIEnv* env, jobject,  jobject jvmci_method))
  methodHandle method = JVMCIENV->asMethod(jvmci_method);
  method->set_not_c1_compilable();
  method->set_not_c2_compilable();
  method->set_dont_inline(true);
C2V_END

C2V_VMENTRY(jint, installCode, (JNIEnv *env, jobject, jobject target, jobject compiled_code, jobject installed_code, jobject speculation_log))
  HandleMark hm;
  JVMCIObject target_handle = JVMCIENV->wrap(target);
  JVMCIObject compiled_code_handle = JVMCIENV->wrap(compiled_code);
  CodeBlob* cb = NULL;
  JVMCIObject installed_code_handle = JVMCIENV->wrap(installed_code);
  JVMCIObject speculation_log_handle = JVMCIENV->wrap(speculation_log);

  JVMCICompiler* compiler = JVMCICompiler::instance(true, CHECK_(JNI_ERR));

  TraceTime install_time("installCode", JVMCICompiler::codeInstallTimer());

  JVMCINMethodData::cleanup();

  CodeInstaller installer(JVMCIENV);
  JVMCI::CodeInstallResult result = installer.install(compiler, target_handle, compiled_code_handle, cb, installed_code_handle, speculation_log_handle, JVMCI_CHECK_0);

  if (PrintCodeCacheOnCompilation) {
    stringStream s;
    // Dump code cache into a buffer before locking the tty,
    {
      MutexLockerEx mu(CodeCache_lock, Mutex::_no_safepoint_check_flag);
      CodeCache::print_summary(&s, false);
    }
    ttyLocker ttyl;
    tty->print_raw_cr(s.as_string());
  }

  if (result != JVMCI::ok) {
    assert(cb == NULL, "should be");
  } else {
    if (installed_code_handle.is_non_null()) {
      if (cb->is_nmethod()) {
        assert(JVMCIENV->isa_HotSpotNmethod(installed_code_handle), "wrong type");
        // Clear the link to an old nmethod first
        JVMCIObject nmethod_mirror = installed_code_handle;
        JVMCIENV->invalidate_nmethod_mirror(nmethod_mirror, JVMCI_CHECK_0);
      } else {
        assert(JVMCIENV->isa_InstalledCode(installed_code_handle), "wrong type");
      }
      // Initialize the link to the new code blob
      JVMCIENV->initialize_installed_code(installed_code_handle, cb, JVMCI_CHECK_0);
    }
  }
  return result;
C2V_END

C2V_VMENTRY(void, resetCompilationStatistics, (JNIEnv* env, jobject))
  JVMCICompiler* compiler = JVMCICompiler::instance(true, CHECK);
  CompilerStatistics* stats = compiler->stats();
  stats->_standard.reset();
  stats->_osr.reset();
C2V_END

C2V_VMENTRY(jobject, disassembleCodeBlob, (JNIEnv* env, jobject, jobject installedCode))
  HandleMark hm;

  if (installedCode == NULL) {
    JVMCI_THROW_MSG_NULL(NullPointerException, "installedCode is null");
  }

  JVMCIObject installedCodeObject = JVMCIENV->wrap(installedCode);
  CodeBlob* cb = JVMCIENV->asCodeBlob(installedCodeObject);
  if (cb == NULL) {
    return NULL;
  }

  // We don't want the stringStream buffer to resize during disassembly as it
  // uses scoped resource memory. If a nested function called during disassembly uses
  // a ResourceMark and the buffer expands within the scope of the mark,
  // the buffer becomes garbage when that scope is exited. Experience shows that
  // the disassembled code is typically about 10x the code size so a fixed buffer
  // sized to 20x code size plus a fixed amount for header info should be sufficient.
  int bufferSize = cb->code_size() * 20 + 1024;
  char* buffer = NEW_RESOURCE_ARRAY(char, bufferSize);
  stringStream st(buffer, bufferSize);
  if (cb->is_nmethod()) {
    nmethod* nm = (nmethod*) cb;
    if (!nm->is_alive()) {
      return NULL;
    }
    Disassembler::decode(nm, &st);
  } else {
    Disassembler::decode(cb, &st);
  }
  if (st.size() <= 0) {
    return NULL;
  }

  JVMCIObject result = JVMCIENV->create_string(st.as_string(), JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
C2V_END

C2V_VMENTRY(jobject, getStackTraceElement, (JNIEnv* env, jobject, jobject jvmci_method, int bci))
  HandleMark hm;

  methodHandle method = JVMCIENV->asMethod(jvmci_method);
  JVMCIObject element = JVMCIENV->new_StackTraceElement(method, bci, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(element);
C2V_END

C2V_VMENTRY(jobject, executeHotSpotNmethod, (JNIEnv* env, jobject, jobject args, jobject hs_nmethod))
  bool wrap_objects = false;
  if (env != JavaThread::current()->jni_environment()) {
    wrap_objects = true;
  }

  if (wrap_objects) {
    // The incoming arguments array would have to contain JavaConstants instead of regular objects
    // and the return value would have to be wrapped as a JavaConstant.
    JVMCI_THROW_MSG_NULL(InternalError, "Wrapping of arguments is currently unsupported");
  }

  HandleMark hm;

  JVMCIObject nmethod_mirror = JVMCIENV->wrap(hs_nmethod);
  nmethod* nm = JVMCIENV->asNmethod(nmethod_mirror);
  if (nm == NULL) {
    JVMCI_THROW_NULL(InvalidInstalledCodeException);
  }
  methodHandle mh = nm->method();
  Symbol* signature = mh->signature();
  JavaCallArguments jca(mh->size_of_parameters());

  JavaArgumentUnboxer jap(signature, &jca, (arrayOop) JNIHandles::resolve(args), mh->is_static());
  JavaValue result(jap.get_ret_type());
  jca.set_alternative_target(nm);
  JavaCalls::call(&result, mh, &jca, CHECK_NULL);

  if (jap.get_ret_type() == T_VOID) {
    return NULL;
  } else if (jap.get_ret_type() == T_OBJECT || jap.get_ret_type() == T_ARRAY) {
    return JNIHandles::make_local((oop) result.get_jobject());
  } else {
    jvalue *value = (jvalue *) result.get_value_addr();
    // Narrow the value down if required (Important on big endian machines)
    switch (jap.get_ret_type()) {
      case T_BOOLEAN:
       value->z = (jboolean) value->i;
       break;
      case T_BYTE:
       value->b = (jbyte) value->i;
       break;
      case T_CHAR:
       value->c = (jchar) value->i;
       break;
      case T_SHORT:
       value->s = (jshort) value->i;
       break;
     }
    JVMCIObject o = JVMCIENV->create_box(jap.get_ret_type(), value, JVMCI_CHECK_NULL);
    return JVMCIENV->get_jobject(o);
  }
C2V_END

C2V_VMENTRY(jlongArray, getLineNumberTable, (JNIEnv* env, jobject, jobject jvmci_method))
  Method* method = JVMCIENV->asMethod(jvmci_method);
  if (!method->has_linenumber_table()) {
    return NULL;
  }
  u2 num_entries = 0;
  CompressedLineNumberReadStream streamForSize(method->compressed_linenumber_table());
  while (streamForSize.read_pair()) {
    num_entries++;
  }

  CompressedLineNumberReadStream stream(method->compressed_linenumber_table());
  JVMCIPrimitiveArray result = JVMCIENV->new_longArray(2 * num_entries, JVMCI_CHECK_NULL);

  int i = 0;
  jlong value;
  while (stream.read_pair()) {
    value = ((long) stream.bci());
    JVMCIENV->put_long_at(result, i, value);
    value = ((long) stream.line());
    JVMCIENV->put_long_at(result, i + 1, value);
    i += 2;
  }

  return (jlongArray) JVMCIENV->get_jobject(result);
C2V_END

C2V_VMENTRY(jlong, getLocalVariableTableStart, (JNIEnv* env, jobject, jobject jvmci_method))
  Method* method = JVMCIENV->asMethod(jvmci_method);
  if (!method->has_localvariable_table()) {
    return 0;
  }
  return (jlong) (address) method->localvariable_table_start();
C2V_END

C2V_VMENTRY(jint, getLocalVariableTableLength, (JNIEnv* env, jobject, jobject jvmci_method))
  Method* method = JVMCIENV->asMethod(jvmci_method);
  return method->localvariable_table_length();
C2V_END

C2V_VMENTRY(void, reprofile, (JNIEnv* env, jobject, jobject jvmci_method))
  Method* method = JVMCIENV->asMethod(jvmci_method);
  MethodCounters* mcs = method->method_counters();
  if (mcs != NULL) {
    mcs->clear_counters();
  }
  NOT_PRODUCT(method->set_compiled_invocation_count(0));

  nmethod* code = method->code();
  if (code != NULL) {
    code->make_not_entrant();
  }

  MethodData* method_data = method->method_data();
  if (method_data == NULL) {
    ClassLoaderData* loader_data = method->method_holder()->class_loader_data();
    method_data = MethodData::allocate(loader_data, method, CHECK);
    method->set_method_data(method_data);
  } else {
    method_data->initialize();
  }
C2V_END


C2V_VMENTRY(void, invalidateHotSpotNmethod, (JNIEnv* env, jobject, jobject hs_nmethod))
  JVMCIObject nmethod_mirror = JVMCIENV->wrap(hs_nmethod);
  JVMCIENV->invalidate_nmethod_mirror(nmethod_mirror, JVMCI_CHECK);
C2V_END

C2V_VMENTRY(jobject, readUncompressedOop, (JNIEnv* env, jobject, jlong addr))
  oop ret = oopDesc::load_decode_heap_oop((oop*)(address)addr);
  return JVMCIENV->get_jobject(JVMCIENV->get_object_constant(ret));
C2V_END

C2V_VMENTRY(jlongArray, collectCounters, (JNIEnv* env, jobject))
  JVMCIPrimitiveArray array = JVMCIENV->new_longArray(JVMCICounterSize, JVMCI_CHECK_NULL);
  JavaThread::collect_counters(JVMCIENV, array);
  return (jlongArray) JVMCIENV->get_jobject(array);
C2V_END

C2V_VMENTRY(int, allocateCompileId, (JNIEnv* env, jobject, jobject jvmci_method, int entry_bci))
  HandleMark hm;
  if (jvmci_method == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Method* method = JVMCIENV->asMethod(jvmci_method);
  if (entry_bci >= method->code_size() || entry_bci < -1) {
    JVMCI_THROW_MSG_0(IllegalArgumentException, err_msg("Unexpected bci %d", entry_bci));
  }
  return CompileBroker::assign_compile_id_unlocked(THREAD, method, entry_bci);
C2V_END


C2V_VMENTRY(jboolean, isMature, (JNIEnv* env, jobject, jlong metaspace_method_data))
  MethodData* mdo = JVMCIENV->asMethodData(metaspace_method_data);
  return mdo != NULL && mdo->is_mature();
C2V_END

C2V_VMENTRY(jboolean, hasCompiledCodeForOSR, (JNIEnv* env, jobject, jobject jvmci_method, int entry_bci, int comp_level))
  Method* method = JVMCIENV->asMethod(jvmci_method);
  return method->lookup_osr_nmethod_for(entry_bci, comp_level, true) != NULL;
C2V_END

C2V_VMENTRY(jobject, getSymbol, (JNIEnv* env, jobject, jlong symbol))
  JVMCIObject sym = JVMCIENV->create_string((Symbol*)(address)symbol, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(sym);
C2V_END

bool matches(jobjectArray methods, Method* method, GrowableArray<Method*>** resolved_methods_ref, JVMCIEnv* JVMCIENV) {
  GrowableArray<Method*>* resolved_methods = *resolved_methods_ref;
  if (resolved_methods == NULL) {
    objArrayOop methods_oop = (objArrayOop) JNIHandles::resolve(methods);
    resolved_methods = new GrowableArray<Method*>(methods_oop->length());
    for (int i = 0; i < methods_oop->length(); i++) {
      oop resolved = methods_oop->obj_at(i);
      assert(HotSpotJVMCI::HotSpotResolvedJavaMethodImpl::klass()->is_leaf_class(), "must be leaf to perform direct comparison");
      Method* resolved_method = NULL;
      if (resolved->klass() == HotSpotJVMCI::HotSpotResolvedJavaMethodImpl::klass()) {
        resolved_method = HotSpotJVMCI::asMethod(JVMCIENV, resolved);
      }
      resolved_methods->append(resolved_method);
    }
    *resolved_methods_ref = resolved_methods;
  }
  assert(method != NULL, "method should not be NULL");
  for (int i = 0; i < resolved_methods->length(); i++) {
    Method* m = resolved_methods->at(i);
    if (m == method) {
      return true;
    }
  }
  return false;
}

C2V_VMENTRY(jobject, iterateFrames, (JNIEnv* env, jobject compilerToVM, jobjectArray initial_methods, jobjectArray match_methods, jint initialSkip, jobject visitor))

  if (!thread->has_last_Java_frame()) {
    return NULL;
  }

  if (env != JavaThread::current()->jni_environment()) {
    JVMCI_THROW_MSG_NULL(InternalError, "getNextStackFrame is only supported for HotSpot stack walking");
  }

  HotSpotJVMCI::HotSpotStackFrameReference::klass()->initialize(CHECK_NULL);
  Handle frame_reference = Handle();

  StackFrameStream fst(thread);
  jobjectArray methods = initial_methods;
  methodHandle visitor_method = NULL;
  GrowableArray<Method*>* resolved_methods = NULL;

  int frame_number = 0;
  vframe* vf = vframe::new_vframe(fst.current(), fst.register_map(), thread);

  while (true) {
    // look for the given method
    bool realloc_called = false;
    while (true) {
      StackValueCollection* locals = NULL;
      if (vf->is_compiled_frame()) {
        // compiled method frame
        compiledVFrame* cvf = compiledVFrame::cast(vf);
        if (methods == NULL || matches(methods, cvf->method(), &resolved_methods, JVMCIENV)) {
          if (initialSkip > 0) {
            initialSkip --;
          } else {
            frame_reference = HotSpotJVMCI::HotSpotStackFrameReference::klass()->allocate_instance(CHECK_NULL);
            ScopeDesc* scope = cvf->scope();
            // native wrappers do not have a scope
            if (scope != NULL && scope->objects() != NULL) {
              GrowableArray<ScopeValue*>* objects;
              if (!realloc_called) {
                objects = scope->objects();
              } else {
                // some object might already have been re-allocated, only reallocate the non-allocated ones
                objects = new GrowableArray<ScopeValue*>(scope->objects()->length());
                for (int i = 0; i < scope->objects()->length(); i++) {
                  ObjectValue* sv = (ObjectValue*) scope->objects()->at(i);
                  if (sv->value().is_null()) {
                    objects->append(sv);
                  }
                }
              }
              bool realloc_failures = Deoptimization::realloc_objects(thread, fst.current(), objects, CHECK_NULL);
              Deoptimization::reassign_fields(fst.current(), fst.register_map(), objects, realloc_failures, false);
              realloc_called = true;

              GrowableArray<ScopeValue*>* local_values = scope->locals();
              typeArrayHandle array = oopFactory::new_boolArray(local_values->length(), CHECK_NULL);
              for (int i = 0; i < local_values->length(); i++) {
                ScopeValue* value = local_values->at(i);
                if (value->is_object()) {
                  array->bool_at_put(i, true);
                }
              }
              HotSpotJVMCI::HotSpotStackFrameReference::set_localIsVirtual(JVMCIENV, frame_reference(), array());
            } else {
              HotSpotJVMCI::HotSpotStackFrameReference::set_localIsVirtual(JVMCIENV, frame_reference(), NULL);
            }

            locals = cvf->locals();
            HotSpotJVMCI::HotSpotStackFrameReference::set_bci(JVMCIENV, frame_reference(), cvf->bci());
            JVMCIObject method = JVMCIENV->get_jvmci_method(cvf->method(), JVMCI_CHECK_NULL);
            HotSpotJVMCI::HotSpotStackFrameReference::set_method(JVMCIENV, frame_reference(), JNIHandles::resolve(method.as_jobject()));
          }
        }
      } else if (vf->is_interpreted_frame()) {
        // interpreted method frame
        interpretedVFrame* ivf = interpretedVFrame::cast(vf);
        if (methods == NULL || matches(methods, ivf->method(), &resolved_methods, JVMCIENV)) {
          if (initialSkip > 0) {
            initialSkip --;
          } else {
            frame_reference = HotSpotJVMCI::HotSpotStackFrameReference::klass()->allocate_instance(CHECK_NULL);
            locals = ivf->locals_no_oop_map_cache();
            HotSpotJVMCI::HotSpotStackFrameReference::set_bci(JVMCIENV, frame_reference(), ivf->bci());
            JVMCIObject method = JVMCIENV->get_jvmci_method(ivf->method(), JVMCI_CHECK_NULL);
            HotSpotJVMCI::HotSpotStackFrameReference::set_method(JVMCIENV, frame_reference(), JNIHandles::resolve(method.as_jobject()));
            HotSpotJVMCI::HotSpotStackFrameReference::set_localIsVirtual(JVMCIENV, frame_reference(), NULL);
          }
        }
      }

      assert((locals == NULL) == (frame_reference.is_null()), "should be synchronized");

      // locals != NULL means that we found a matching frame and result is already partially initialized
      if (locals != NULL) {
        HotSpotJVMCI::HotSpotStackFrameReference::set_compilerToVM(JVMCIENV, frame_reference(), JNIHandles::resolve(compilerToVM));
        HotSpotJVMCI::HotSpotStackFrameReference::set_stackPointer(JVMCIENV, frame_reference(), (jlong) fst.current()->sp());
        HotSpotJVMCI::HotSpotStackFrameReference::set_frameNumber(JVMCIENV, frame_reference(), frame_number);

        // initialize the locals array
        objArrayHandle array = oopFactory::new_objectArray(locals->size(), CHECK_NULL);
        for (int i = 0; i < locals->size(); i++) {
          StackValue* var = locals->at(i);
          if (var->type() == T_OBJECT) {
            array->obj_at_put(i, locals->at(i)->get_obj()());
          }
        }
        HotSpotJVMCI::HotSpotStackFrameReference::set_locals(JVMCIENV, frame_reference(), array());
        HotSpotJVMCI::HotSpotStackFrameReference::set_objectsMaterialized(JVMCIENV, frame_reference(), JNI_FALSE);

        JavaValue result(T_OBJECT);
        JavaCallArguments args(JNIHandles::resolve_non_null(visitor));
        if (visitor_method.is_null()) {
          CallInfo callinfo;
          Handle receiver = args.receiver();
          KlassHandle recvrKlass(THREAD, receiver.is_null() ? (Klass*)NULL : receiver->klass());
          LinkResolver::resolve_interface_call(
                  callinfo, receiver, recvrKlass, HotSpotJVMCI::InspectedFrameVisitor::klass(), vmSymbols::visitFrame_name(), vmSymbols::visitFrame_signature(),
                  KlassHandle(), false, true, CHECK_NULL);
          visitor_method = callinfo.selected_method();
          assert(visitor_method.not_null(), "should have thrown exception");
        }

        args.push_oop(frame_reference);
        JavaCalls::call(&result, visitor_method, &args, CHECK_NULL);
        if (result.get_jobject() != NULL) {
          return JNIHandles::make_local(thread, (oop) result.get_jobject());
        }
        if (methods == initial_methods) {
          methods = match_methods;
          if (resolved_methods != NULL && JNIHandles::resolve(match_methods) != JNIHandles::resolve(initial_methods)) {
            resolved_methods = NULL;
          }
        }
        assert(initialSkip == 0, "There should be no match before initialSkip == 0");
        if (HotSpotJVMCI::HotSpotStackFrameReference::objectsMaterialized(JVMCIENV, frame_reference()) == JNI_TRUE) {
          // the frame has been deoptimized, we need to re-synchronize the frame and vframe
          intptr_t* stack_pointer = (intptr_t*) HotSpotJVMCI::HotSpotStackFrameReference::stackPointer(JVMCIENV, frame_reference());
          fst = StackFrameStream(thread);
          while (fst.current()->sp() != stack_pointer && !fst.is_done()) {
            fst.next();
          }
          if (fst.current()->sp() != stack_pointer) {
            THROW_MSG_NULL(vmSymbols::java_lang_IllegalStateException(), "stack frame not found after deopt")
          }
          vf = vframe::new_vframe(fst.current(), fst.register_map(), thread);
          if (!vf->is_compiled_frame()) {
            THROW_MSG_NULL(vmSymbols::java_lang_IllegalStateException(), "compiled stack frame expected")
          }
          for (int i = 0; i < frame_number; i++) {
            if (vf->is_top()) {
              THROW_MSG_NULL(vmSymbols::java_lang_IllegalStateException(), "vframe not found after deopt")
            }
            vf = vf->sender();
            assert(vf->is_compiled_frame(), "Wrong frame type");
          }
        }
        frame_reference = Handle();
      }

      if (vf->is_top()) {
        break;
      }
      frame_number++;
      vf = vf->sender();
    } // end of vframe loop

    if (fst.is_done()) {
      break;
    }
    fst.next();
    vf = vframe::new_vframe(fst.current(), fst.register_map(), thread);
    frame_number = 0;
  } // end of frame loop

  // the end was reached without finding a matching method
  return NULL;
C2V_END

C2V_VMENTRY(void, resolveInvokeDynamicInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint index))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  CallInfo callInfo;
  LinkResolver::resolve_invokedynamic(callInfo, cp, index, CHECK);
  ConstantPoolCacheEntry* cp_cache_entry = cp->invokedynamic_cp_cache_entry_at(index);
  cp_cache_entry->set_dynamic_call(cp, callInfo);
C2V_END

C2V_VMENTRY(void, resolveInvokeHandleInPool, (JNIEnv* env, jobject, jobject jvmci_constant_pool, jint index))
  constantPoolHandle cp = JVMCIENV->asConstantPool(jvmci_constant_pool);
  KlassHandle holder = cp->klass_ref_at(index, CHECK);
  Symbol* name = cp->name_ref_at(index);
  if (MethodHandles::is_signature_polymorphic_name(holder(), name)) {
    CallInfo callInfo;
    LinkResolver::resolve_invokehandle(callInfo, cp, index, CHECK);
    ConstantPoolCacheEntry* cp_cache_entry = cp_cache_entry = cp->cache()->entry_at(cp->decode_cpcache_index(index));
    cp_cache_entry->set_method_handle(cp, callInfo);
  }
C2V_END

C2V_VMENTRY(jobject, getSignaturePolymorphicHolders, (JNIEnv* env, jobject))
  JVMCIObjectArray holders = JVMCIENV->new_String_array(2, JVMCI_CHECK_NULL);
  JVMCIObject mh = JVMCIENV->create_string("Ljava/lang/invoke/MethodHandle;", JVMCI_CHECK_NULL);
  JVMCIObject vh = JVMCIENV->create_string("Ljava/lang/invoke/VarHandle;", JVMCI_CHECK_NULL);
  JVMCIENV->put_object_at(holders, 0, mh);
  JVMCIENV->put_object_at(holders, 1, vh);
  return JVMCIENV->get_jobject(holders);
C2V_END

C2V_VMENTRY(jboolean, shouldDebugNonSafepoints, (JNIEnv* env, jobject))
  //see compute_recording_non_safepoints in debugInfroRec.cpp
  if (JvmtiExport::should_post_compiled_method_load() && FLAG_IS_DEFAULT(DebugNonSafepoints)) {
    return true;
  }
  return DebugNonSafepoints;
C2V_END

// public native void materializeVirtualObjects(HotSpotStackFrameReference stackFrame, boolean invalidate);
C2V_VMENTRY(void, materializeVirtualObjects, (JNIEnv* env, jobject, jobject _hs_frame, bool invalidate))
  JVMCIObject hs_frame = JVMCIENV->wrap(_hs_frame);
  if (hs_frame.is_null()) {
    JVMCI_THROW_MSG(NullPointerException, "stack frame is null");
  }

  if (env != JavaThread::current()->jni_environment()) {
    JVMCI_THROW_MSG(InternalError, "getNextStackFrame is only supported for HotSpot stack walking");
  }

  JVMCIENV->HotSpotStackFrameReference_initialize(JVMCI_CHECK);

  // look for the given stack frame
  StackFrameStream fst(thread);
  intptr_t* stack_pointer = (intptr_t*) JVMCIENV->get_HotSpotStackFrameReference_stackPointer(hs_frame);
  while (fst.current()->sp() != stack_pointer && !fst.is_done()) {
    fst.next();
  }
  if (fst.current()->sp() != stack_pointer) {
    JVMCI_THROW_MSG(IllegalStateException, "stack frame not found");
  }

  if (invalidate) {
    if (!fst.current()->is_compiled_frame()) {
      JVMCI_THROW_MSG(IllegalStateException, "compiled stack frame expected");
    }
    assert(fst.current()->cb()->is_nmethod(), "nmethod expected");
    ((nmethod*) fst.current()->cb())->make_not_entrant();
  }
  Deoptimization::deoptimize(thread, *fst.current(), fst.register_map(), Deoptimization::Reason_none);
  // look for the frame again as it has been updated by deopt (pc, deopt state...)
  StackFrameStream fstAfterDeopt(thread);
  while (fstAfterDeopt.current()->sp() != stack_pointer && !fstAfterDeopt.is_done()) {
    fstAfterDeopt.next();
  }
  if (fstAfterDeopt.current()->sp() != stack_pointer) {
    JVMCI_THROW_MSG(IllegalStateException, "stack frame not found after deopt");
  }

  vframe* vf = vframe::new_vframe(fstAfterDeopt.current(), fstAfterDeopt.register_map(), thread);
  if (!vf->is_compiled_frame()) {
    JVMCI_THROW_MSG(IllegalStateException, "compiled stack frame expected");
  }

  GrowableArray<compiledVFrame*>* virtualFrames = new GrowableArray<compiledVFrame*>(10);
  while (true) {
    assert(vf->is_compiled_frame(), "Wrong frame type");
    virtualFrames->push(compiledVFrame::cast(vf));
    if (vf->is_top()) {
      break;
    }
    vf = vf->sender();
  }

  int last_frame_number = JVMCIENV->get_HotSpotStackFrameReference_frameNumber(hs_frame);
  if (last_frame_number >= virtualFrames->length()) {
    JVMCI_THROW_MSG(IllegalStateException, "invalid frame number");
  }

  // Reallocate the non-escaping objects and restore their fields.
  assert (virtualFrames->at(last_frame_number)->scope() != NULL,"invalid scope");
  GrowableArray<ScopeValue*>* objects = virtualFrames->at(last_frame_number)->scope()->objects();

  if (objects == NULL) {
    // no objects to materialize
    return;
  }

  bool realloc_failures = Deoptimization::realloc_objects(thread, fstAfterDeopt.current(), objects, CHECK);
  Deoptimization::reassign_fields(fstAfterDeopt.current(), fstAfterDeopt.register_map(), objects, realloc_failures, false);

  for (int frame_index = 0; frame_index < virtualFrames->length(); frame_index++) {
    compiledVFrame* cvf = virtualFrames->at(frame_index);

    GrowableArray<ScopeValue*>* scopeLocals = cvf->scope()->locals();
    StackValueCollection* locals = cvf->locals();
    if (locals != NULL) {
      for (int i2 = 0; i2 < locals->size(); i2++) {
        StackValue* var = locals->at(i2);
        if (var->type() == T_OBJECT && scopeLocals->at(i2)->is_object()) {
          jvalue val;
          val.l = (jobject) locals->at(i2)->get_obj()();
          cvf->update_local(T_OBJECT, i2, val);
        }
      }
    }

    GrowableArray<ScopeValue*>* scopeExpressions = cvf->scope()->expressions();
    StackValueCollection* expressions = cvf->expressions();
    if (expressions != NULL) {
      for (int i2 = 0; i2 < expressions->size(); i2++) {
        StackValue* var = expressions->at(i2);
        if (var->type() == T_OBJECT && scopeExpressions->at(i2)->is_object()) {
          jvalue val;
          val.l = (jobject) expressions->at(i2)->get_obj()();
          cvf->update_stack(T_OBJECT, i2, val);
        }
      }
    }

    GrowableArray<MonitorValue*>* scopeMonitors = cvf->scope()->monitors();
    GrowableArray<MonitorInfo*>* monitors = cvf->monitors();
    if (monitors != NULL) {
      for (int i2 = 0; i2 < monitors->length(); i2++) {
        cvf->update_monitor(i2, monitors->at(i2));
      }
    }
  }

  // all locals are materialized by now
  JVMCIENV->set_HotSpotStackFrameReference_localIsVirtual(hs_frame, NULL);
  // update the locals array
  JVMCIObjectArray array = JVMCIENV->get_HotSpotStackFrameReference_locals(hs_frame);
  StackValueCollection* locals = virtualFrames->at(last_frame_number)->locals();
  for (int i = 0; i < locals->size(); i++) {
    StackValue* var = locals->at(i);
    if (var->type() == T_OBJECT) {
      JVMCIENV->put_object_at(array, i, HotSpotJVMCI::wrap(locals->at(i)->get_obj()()));
    }
  }
  HotSpotJVMCI::HotSpotStackFrameReference::set_objectsMaterialized(JVMCIENV, hs_frame, JNI_TRUE);
C2V_END

C2V_VMENTRY(void, writeDebugOutput, (JNIEnv* env, jobject, jbyteArray bytes, jint offset, jint length))
  if (bytes == NULL) {
    JVMCI_THROW(NullPointerException);
  }
  JVMCIPrimitiveArray array = JVMCIENV->wrap(bytes);

  // Check if offset and length are non negative.
  if (offset < 0 || length < 0) {
    JVMCI_THROW(ArrayIndexOutOfBoundsException);
  }
  // Check if the range is valid.
  int array_length = JVMCIENV->get_length(array);
  if ((((unsigned int) length + (unsigned int) offset) > (unsigned int) array_length)) {
    JVMCI_THROW(ArrayIndexOutOfBoundsException);
  }
  jbyte buffer[O_BUFLEN];
  while (length > 0) {
    int copy_len = MIN2(length, O_BUFLEN);
    JVMCIENV->copy_bytes_to(array, buffer, offset, copy_len);
    tty->write((char*) buffer, copy_len);
    length -= O_BUFLEN;
    offset += O_BUFLEN;
  }
C2V_END

C2V_VMENTRY(void, flushDebugOutput, (JNIEnv* env, jobject))
  tty->flush();
C2V_END

C2V_VMENTRY(void, writeCompileLogOutput, (JNIEnv* env, jobject, jbyteArray bytes, jint offset, jint length))
  CompileLog*     log = NULL;
  if (THREAD->is_Compiler_thread()) {
    log = ((CompilerThread*)THREAD)->log();
  }
  if (log == NULL) {
    JVMCI_THROW_MSG(IllegalArgumentException, "No CompileLog available");
  }
  if (bytes == NULL) {
    JVMCI_THROW(NullPointerException);
  }
  JVMCIPrimitiveArray array = JVMCIENV->wrap(bytes);

  // Check if offset and length are non negative.
  if (offset < 0 || length < 0) {
    JVMCI_THROW(ArrayIndexOutOfBoundsException);
  }
  // Check if the range is valid.
  int array_length = JVMCIENV->get_length(array);
  if ((((unsigned int) length + (unsigned int) offset) > (unsigned int) array_length)) {
    JVMCI_THROW(ArrayIndexOutOfBoundsException);
  }
  jbyte buffer[O_BUFLEN];
  while (length > 0) {
    int copy_len = MIN2(length, O_BUFLEN);
    JVMCIENV->copy_bytes_to(array, buffer, offset, copy_len);
    log->write((char*) buffer, copy_len);
    length -= O_BUFLEN;
    offset += O_BUFLEN;
  }
C2V_END

C2V_VMENTRY(void, flushCompileLogOutput, (JNIEnv* env, jobject))
  CompileLog*     log = NULL;
  if (THREAD->is_Compiler_thread()) {
    log = ((CompilerThread*)THREAD)->log();
  }
  if (log == NULL) {
    THROW_MSG(vmSymbols::java_lang_IllegalArgumentException(), "No CompileLog available");
  }
  log->flush();
C2V_END

C2V_VMENTRY(int, methodDataProfileDataSize, (JNIEnv* env, jobject, jlong metaspace_method_data, jint position))
  MethodData* mdo = JVMCIENV->asMethodData(metaspace_method_data);
  ProfileData* profile_data = mdo->data_at(position);
  if (mdo->is_valid(profile_data)) {
    return profile_data->size_in_bytes();
  }
  DataLayout* data    = mdo->extra_data_base();
  DataLayout* end   = mdo->extra_data_limit();
  for (;; data = mdo->next_extra(data)) {
    assert(data < end, "moved past end of extra data");
    profile_data = data->data_in();
    if (mdo->dp_to_di(profile_data->dp()) == position) {
      return profile_data->size_in_bytes();
    }
  }
  JVMCI_THROW_MSG_0(IllegalArgumentException, err_msg("Invalid profile data position %d", position));
C2V_END

C2V_VMENTRY(jobject, getHostClass, (JNIEnv* env, jobject, jobject jvmci_type))
  InstanceKlass* k = InstanceKlass::cast(JVMCIENV->asKlass(jvmci_type));
  Klass* host = k->host_klass();
  JVMCIObject result = JVMCIENV->get_jvmci_type(host, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
C2V_END

C2V_VMENTRY(jobject, getInterfaces, (JNIEnv* env, jobject, jobject jvmci_type))
  if (jvmci_type == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }

  Klass* klass = JVMCIENV->asKlass(jvmci_type);
  if (klass == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  if (!klass->oop_is_instance()) {
    JVMCI_THROW_MSG_0(InternalError, err_msg("Class %s must be instance klass", klass->external_name()));
  }
  InstanceKlass* iklass = InstanceKlass::cast(klass);

  // Regular instance klass, fill in all local interfaces
  int size = iklass->local_interfaces()->length();
  JVMCIObjectArray interfaces = JVMCIENV->new_HotSpotResolvedObjectTypeImpl_array(size, JVMCI_CHECK_NULL);
  for (int index = 0; index < size; index++) {
    Klass* k = iklass->local_interfaces()->at(index);
    JVMCIObject type = JVMCIENV->get_jvmci_type(k, JVMCI_CHECK_NULL);
    JVMCIENV->put_object_at(interfaces, index, type);
  }
  return JVMCIENV->get_jobject(interfaces);
C2V_END

C2V_VMENTRY(jobject, getComponentType, (JNIEnv* env, jobject, jobject jvmci_type))
  if (jvmci_type == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }

  Klass* klass = JVMCIENV->asKlass(jvmci_type);
  oop component_mirror = Reflection::array_component_type(klass->java_mirror(), CHECK_NULL);
  if (component_mirror == NULL) {
    return NULL;
  }
  Klass* component_klass = java_lang_Class::as_Klass(component_mirror);
  if (component_klass != NULL) {
    JVMCIObject result = JVMCIENV->get_jvmci_type(component_klass, JVMCI_CHECK_NULL);
    return JVMCIENV->get_jobject(result);
  }
  BasicType type = java_lang_Class::primitive_type(component_mirror);
  JVMCIObject result = JVMCIENV->get_jvmci_primitive_type(type);
  return JVMCIENV->get_jobject(result);
C2V_END

C2V_VMENTRY(void, ensureInitialized, (JNIEnv* env, jobject, jobject jvmci_type))
  if (jvmci_type == NULL) {
    JVMCI_THROW(NullPointerException);
  }

  Klass* klass = JVMCIENV->asKlass(jvmci_type);
  if (klass != NULL && klass->should_be_initialized()) {
    InstanceKlass* k = InstanceKlass::cast(klass);
    k->initialize(CHECK);
  }
C2V_END

C2V_VMENTRY(int, interpreterFrameSize, (JNIEnv* env, jobject, jobject bytecode_frame_handle))
  if (bytecode_frame_handle == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }

  JVMCIObject top_bytecode_frame = JVMCIENV->wrap(bytecode_frame_handle);
  JVMCIObject bytecode_frame = top_bytecode_frame;
  int size = 0;
  int callee_parameters = 0;
  int callee_locals = 0;
  Method* method = JVMCIENV->asMethod(JVMCIENV->get_BytecodePosition_method(bytecode_frame));
  int extra_args = method->max_stack() - JVMCIENV->get_BytecodeFrame_numStack(bytecode_frame);

  while (bytecode_frame.is_non_null()) {
    int locks = JVMCIENV->get_BytecodeFrame_numLocks(bytecode_frame);
    int temps = JVMCIENV->get_BytecodeFrame_numStack(bytecode_frame);
    bool is_top_frame = (JVMCIENV->equals(bytecode_frame, top_bytecode_frame));
    Method* method = JVMCIENV->asMethod(JVMCIENV->get_BytecodePosition_method(bytecode_frame));

    int frame_size = BytesPerWord * Interpreter::size_activation(method->max_stack(),
                                                                 temps + callee_parameters,
                                                                 extra_args,
                                                                 locks,
                                                                 callee_parameters,
                                                                 callee_locals,
                                                                 is_top_frame);
    size += frame_size;

    callee_parameters = method->size_of_parameters();
    callee_locals = method->max_locals();
    extra_args = 0;
    bytecode_frame = JVMCIENV->get_BytecodePosition_caller(bytecode_frame);
  }
  return size + Deoptimization::last_frame_adjust(0, callee_locals) * BytesPerWord;
C2V_END

C2V_VMENTRY(void, compileToBytecode, (JNIEnv* env, jobject, jobject lambda_form_handle))
  Handle lambda_form = JVMCIENV->asConstant(JVMCIENV->wrap(lambda_form_handle), JVMCI_CHECK);
  if (lambda_form->is_a(SystemDictionary::LambdaForm_klass())) {
    TempNewSymbol compileToBytecode = SymbolTable::new_symbol("compileToBytecode", CHECK);
    JavaValue result(T_VOID);
    JavaCalls::call_special(&result, lambda_form, SystemDictionary::LambdaForm_klass(), compileToBytecode, vmSymbols::void_method_signature(), CHECK);
  } else {
    JVMCI_THROW_MSG(IllegalArgumentException,
                    err_msg("Unexpected type: %s", lambda_form->klass()->external_name()));
  }
C2V_END

C2V_VMENTRY(int, getIdentityHashCode, (JNIEnv* env, jobject, jobject object))
  oop obj = JVMCIENV->asConstant(JVMCIENV->wrap(object), JVMCI_CHECK_0);
  return obj->identity_hash();
C2V_END

C2V_VMENTRY(jboolean, isInternedString, (JNIEnv* env, jobject, jobject object))
  Handle str = JVMCIENV->asConstant(JVMCIENV->wrap(object), JVMCI_CHECK_0);
  int len;
  jchar* name = java_lang_String::as_unicode_string(str(), len, CHECK_0);
  return (StringTable::lookup(name, len) != NULL);
C2V_END


C2V_VMENTRY(jobject, unboxPrimitive, (JNIEnv* env, jobject, jobject object))
  if (object == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Handle box = JVMCIENV->asConstant(JVMCIENV->wrap(object), JVMCI_CHECK_NULL);
  BasicType type = java_lang_boxing_object::basic_type(box());
  jvalue result;
  java_lang_boxing_object::get_value(box(), &result);
  JVMCIObject boxResult = JVMCIENV->create_box(type, &result, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(boxResult);
C2V_END

C2V_VMENTRY(jobject, boxPrimitive, (JNIEnv* env, jobject, jobject object))
  if (object == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  JVMCIObject box = JVMCIENV->wrap(object);
  BasicType type = JVMCIENV->get_box_type(box);
  jvalue value = JVMCIENV->get_boxed_value(type, box);
  JavaValue box_result(T_OBJECT);
  JavaCallArguments jargs;
  Klass* box_klass = NULL;
  Symbol* box_signature = NULL;
#define BOX_CASE(bt, v, argtype, name)           \
  case bt: \
    jargs.push_##argtype(value.v); \
    box_klass = SystemDictionary::name##_klass(); \
    box_signature = vmSymbols::name##_valueOf_signature(); \
    break

  switch (type) {
    BOX_CASE(T_BOOLEAN, z, int, Boolean);
    BOX_CASE(T_BYTE, b, int, Byte);
    BOX_CASE(T_CHAR, c, int, Character);
    BOX_CASE(T_SHORT, s, int, Short);
    BOX_CASE(T_INT, i, int, Integer);
    BOX_CASE(T_LONG, j, long, Long);
    BOX_CASE(T_FLOAT, f, float, Float);
    BOX_CASE(T_DOUBLE, d, double, Double);
    default:
      ShouldNotReachHere();
  }
#undef BOX_CASE

  JavaCalls::call_static(&box_result,
                         box_klass,
                         vmSymbols::valueOf_name(),
                         box_signature, &jargs, CHECK_NULL);
  oop hotspot_box = (oop) box_result.get_jobject();
  JVMCIObject result = JVMCIENV->get_object_constant(hotspot_box, false);
  return JVMCIENV->get_jobject(result);
C2V_END

C2V_VMENTRY(jobjectArray, getDeclaredConstructors, (JNIEnv* env, jobject, jobject holder))
  if (holder == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Klass* klass = JVMCIENV->asKlass(holder);
  if (!klass->oop_is_instance()) {
    JVMCI_THROW_MSG_0(InternalError, err_msg("Class %s must be instance klass", klass->external_name()));
  }
  InstanceKlass* iklass = InstanceKlass::cast(klass);
  GrowableArray<Method*> constructors_array;
  for (int i = 0; i < iklass->methods()->length(); i++) {
    Method* m = iklass->methods()->at(i);
    if (m->name() == vmSymbols::object_initializer_name()) {
      constructors_array.append(m);
    }
  }
  JVMCIObjectArray methods = JVMCIENV->new_ResolvedJavaMethod_array(constructors_array.length(), JVMCI_CHECK_NULL);
  for (int i = 0; i < constructors_array.length(); i++) {
    JVMCIObject method = JVMCIENV->get_jvmci_method(constructors_array.at(i), JVMCI_CHECK_NULL);
    JVMCIENV->put_object_at(methods, i, method);
  }
  return JVMCIENV->get_jobjectArray(methods);
C2V_END

C2V_VMENTRY(jobjectArray, getDeclaredMethods, (JNIEnv* env, jobject, jobject holder))
  if (holder == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Klass* klass = JVMCIENV->asKlass(holder);
  if (!klass->oop_is_instance()) {
    JVMCI_THROW_MSG_0(InternalError, err_msg("Class %s must be instance klass", klass->external_name()));
  }
  InstanceKlass* iklass = InstanceKlass::cast(klass);
  GrowableArray<Method*> constructors;
  for (int i = 0; i < iklass->methods()->length(); i++) {
    Method* m = iklass->methods()->at(i);
    if (m->name() != vmSymbols::object_initializer_name()) {
      constructors.append(m);
    }
  }
  JVMCIObjectArray methods = JVMCIENV->new_ResolvedJavaMethod_array(constructors.length(), JVMCI_CHECK_NULL);
  for (int i = 0; i < constructors.length(); i++) {
    JVMCIObject method = JVMCIENV->get_jvmci_method(constructors.at(i), JVMCI_CHECK_NULL);
    JVMCIENV->put_object_at(methods, i, method);
  }
  return JVMCIENV->get_jobjectArray(methods);
C2V_END

C2V_VMENTRY(jobject, readFieldValue, (JNIEnv* env, jobject, jobject object, jobject field, jboolean is_volatile))
  if (object == NULL || field == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  JVMCIObject field_object = JVMCIENV->wrap(field);
  JVMCIObject java_type = JVMCIENV->get_HotSpotResolvedJavaFieldImpl_type(field_object);
  int modifiers = JVMCIENV->get_HotSpotResolvedJavaFieldImpl_modifiers(field_object);
  Klass* holder = JVMCIENV->asKlass(JVMCIENV->get_HotSpotResolvedJavaFieldImpl_holder(field_object));
  if (!holder->oop_is_instance()) {
    JVMCI_THROW_MSG_0(InternalError, err_msg("Holder %s must be instance klass", holder->external_name()));
  }
  InstanceKlass* ik = InstanceKlass::cast(holder);
  BasicType constant_type;
  if (JVMCIENV->isa_HotSpotResolvedPrimitiveType(java_type)) {
    constant_type = JVMCIENV->kindToBasicType(JVMCIENV->get_HotSpotResolvedPrimitiveType_kind(java_type), JVMCI_CHECK_NULL);
  } else {
    constant_type = T_OBJECT;
  }
  int displacement = JVMCIENV->get_HotSpotResolvedJavaFieldImpl_offset(field_object);
  fieldDescriptor fd;
  if (!ik->find_local_field_from_offset(displacement, modifiers & JVM_ACC_STATIC, &fd)) {
    JVMCI_THROW_MSG_0(InternalError, err_msg("Can't find field with displacement %d", displacement));
  }
  JVMCIObject base = JVMCIENV->wrap(object);
  Handle obj;
  if (JVMCIENV->isa_HotSpotObjectConstantImpl(base)) {
    obj = JVMCIENV->asConstant(base, JVMCI_CHECK_NULL);
  } else if (JVMCIENV->isa_HotSpotResolvedObjectTypeImpl(base)) {
    Klass* klass = JVMCIENV->asKlass(base);
    obj = klass->java_mirror();
  } else {
    JVMCI_THROW_MSG_NULL(IllegalArgumentException,
                         err_msg("Unexpected type: %s", JVMCIENV->klass_name(base)));
  }
  jlong value = 0;
  JVMCIObject kind;
  switch (constant_type) {
    case T_OBJECT: {
      oop object = is_volatile ? obj->obj_field_acquire(displacement) : obj->obj_field(displacement);
      JVMCIObject result = JVMCIENV->get_object_constant(object);
      if (result.is_null()) {
        return JVMCIENV->get_jobject(JVMCIENV->get_JavaConstant_NULL_POINTER());
      }
      return JVMCIENV->get_jobject(result);
    }
    case T_FLOAT: {
      float f = is_volatile ? obj->float_field_acquire(displacement) : obj->float_field(displacement);
      JVMCIObject result = JVMCIENV->call_JavaConstant_forFloat(f, JVMCI_CHECK_NULL);
      return JVMCIENV->get_jobject(result);
    }
    case T_DOUBLE: {
      double f = is_volatile ? obj->double_field_acquire(displacement) : obj->double_field(displacement);
      JVMCIObject result = JVMCIENV->call_JavaConstant_forDouble(f, JVMCI_CHECK_NULL);
      return JVMCIENV->get_jobject(result);
    }
    case T_BOOLEAN: value = is_volatile ? obj->bool_field_acquire(displacement) : obj->bool_field(displacement); break;
    case T_BYTE: value = is_volatile ? obj->byte_field_acquire(displacement) : obj->byte_field(displacement); break;
    case T_SHORT: value = is_volatile ? obj->short_field_acquire(displacement) : obj->short_field(displacement); break;
    case T_CHAR: value = is_volatile ? obj->char_field_acquire(displacement) : obj->char_field(displacement); break;
    case T_INT: value = is_volatile ? obj->int_field_acquire(displacement) : obj->int_field(displacement); break;
    case T_LONG: value = is_volatile ? obj->long_field_acquire(displacement) : obj->long_field(displacement); break;
    default:
      ShouldNotReachHere();
  }
  JVMCIObject result = JVMCIENV->call_PrimitiveConstant_forTypeChar(type2char(constant_type), value, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
C2V_END

C2V_VMENTRY(jboolean, isInstance, (JNIEnv* env, jobject, jobject holder, jobject object))
  if (object == NULL || holder == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Handle obj = JVMCIENV->asConstant(JVMCIENV->wrap(object), JVMCI_CHECK_0);
  Klass* klass = JVMCIENV->asKlass(JVMCIENV->wrap(holder));
  return obj->is_a(klass);
C2V_END

C2V_VMENTRY(jboolean, isAssignableFrom, (JNIEnv* env, jobject, jobject holder, jobject otherHolder))
  if (holder == NULL || otherHolder == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Klass* klass = JVMCIENV->asKlass(JVMCIENV->wrap(holder));
  Klass* otherKlass = JVMCIENV->asKlass(JVMCIENV->wrap(otherHolder));
  return otherKlass->is_subtype_of(klass);
C2V_END


C2V_VMENTRY(jobject, asJavaType, (JNIEnv* env, jobject, jobject object))
  if (object == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Handle obj = JVMCIENV->asConstant(JVMCIENV->wrap(object), JVMCI_CHECK_NULL);
  if (java_lang_Class::is_instance(obj())) {
    if (java_lang_Class::is_primitive(obj())) {
      JVMCIObject type = JVMCIENV->get_jvmci_primitive_type(java_lang_Class::primitive_type(obj()));
      return JVMCIENV->get_jobject(type);
    }
    Klass* klass = java_lang_Class::as_Klass(obj());
    JVMCIObject type = JVMCIENV->get_jvmci_type(klass, JVMCI_CHECK_NULL);
    return JVMCIENV->get_jobject(type);
  }
  return NULL;
C2V_END


C2V_VMENTRY(jobject, asString, (JNIEnv* env, jobject, jobject object))
  if (object == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Handle obj = JVMCIENV->asConstant(JVMCIENV->wrap(object), JVMCI_CHECK_NULL);
  const char* str = java_lang_String::as_utf8_string(obj());
  JVMCIObject result = JVMCIENV->create_string(str, JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(result);
C2V_END


C2V_VMENTRY(jboolean, equals, (JNIEnv* env, jobject, jobject x, jlong xHandle, jobject y, jlong yHandle))
  if (x == NULL || y == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  return JVMCIENV->resolve_handle(xHandle) == JVMCIENV->resolve_handle(yHandle);
C2V_END

C2V_VMENTRY(jobject, getJavaMirror, (JNIEnv* env, jobject, jobject object))
  if (object == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  JVMCIObject base_object = JVMCIENV->wrap(object);
  Handle mirror;
  if (JVMCIENV->isa_HotSpotResolvedObjectTypeImpl(base_object)) {
    mirror = JVMCIENV->asKlass(base_object)->java_mirror();
  } else if (JVMCIENV->isa_HotSpotResolvedPrimitiveType(base_object)) {
    mirror = JVMCIENV->asConstant(JVMCIENV->get_HotSpotResolvedPrimitiveType_mirror(base_object), JVMCI_CHECK_NULL);
  } else {
    JVMCI_THROW_MSG_NULL(IllegalArgumentException,
                         err_msg("Unexpected type: %s", JVMCIENV->klass_name(base_object)));
 }
  JVMCIObject result = JVMCIENV->get_object_constant(mirror());
  return JVMCIENV->get_jobject(result);
C2V_END


C2V_VMENTRY(jint, getArrayLength, (JNIEnv* env, jobject, jobject x))
  if (x == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Handle xobj = JVMCIENV->asConstant(JVMCIENV->wrap(x), JVMCI_CHECK_0);
  if (xobj->klass()->oop_is_array()) {
    return arrayOop(xobj())->length();
  }
  return -1;
C2V_END


C2V_VMENTRY(jobject, readArrayElement, (JNIEnv* env, jobject, jobject x, int index))
  if (x == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Handle xobj = JVMCIENV->asConstant(JVMCIENV->wrap(x), JVMCI_CHECK_NULL);
  if (xobj->klass()->oop_is_array()) {
    arrayOop array = arrayOop(xobj());
    BasicType element_type = ArrayKlass::cast(array->klass())->element_type();
    if (index < 0 || index >= array->length()) {
      return NULL;
    }
    JVMCIObject result;

    if (element_type == T_OBJECT) {
      result = JVMCIENV->get_object_constant(objArrayOop(xobj())->obj_at(index));
      if (result.is_null()) {
        result = JVMCIENV->get_JavaConstant_NULL_POINTER();
      }
    } else {
      jvalue value;
      switch (element_type) {
        case T_DOUBLE:        value.d = typeArrayOop(xobj())->double_at(index);        break;
        case T_FLOAT:         value.f = typeArrayOop(xobj())->float_at(index);         break;
        case T_LONG:          value.j = typeArrayOop(xobj())->long_at(index);          break;
        case T_INT:           value.i = typeArrayOop(xobj())->int_at(index);            break;
        case T_SHORT:         value.s = typeArrayOop(xobj())->short_at(index);          break;
        case T_CHAR:          value.c = typeArrayOop(xobj())->char_at(index);           break;
        case T_BYTE:          value.b = typeArrayOop(xobj())->byte_at(index);           break;
        case T_BOOLEAN:       value.z = typeArrayOop(xobj())->byte_at(index) & 1;       break;
        default:              ShouldNotReachHere();
      }
      result = JVMCIENV->create_box(element_type, &value, JVMCI_CHECK_NULL);
    }
    assert(!result.is_null(), "must have a value");
    return JVMCIENV->get_jobject(result);
  }
  return NULL;;
C2V_END


C2V_VMENTRY(jint, arrayBaseOffset, (JNIEnv* env, jobject, jobject kind))
  if (kind == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  BasicType type = JVMCIENV->kindToBasicType(JVMCIENV->wrap(kind), JVMCI_CHECK_0);
  return arrayOopDesc::header_size(type) * HeapWordSize;
C2V_END

C2V_VMENTRY(jint, arrayIndexScale, (JNIEnv* env, jobject, jobject kind))
  if (kind == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  BasicType type = JVMCIENV->kindToBasicType(JVMCIENV->wrap(kind), JVMCI_CHECK_0);
  return type2aelembytes(type);
C2V_END

C2V_VMENTRY(jbyte, getByte, (JNIEnv* env, jobject, jobject x, long displacement))
  if (x == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Handle xobj = JVMCIENV->asConstant(JVMCIENV->wrap(x), JVMCI_CHECK_0);
  return xobj->byte_field(displacement);
}

C2V_VMENTRY(jshort, getShort, (JNIEnv* env, jobject, jobject x, long displacement))
  if (x == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Handle xobj = JVMCIENV->asConstant(JVMCIENV->wrap(x), JVMCI_CHECK_0);
  return xobj->short_field(displacement);
}

C2V_VMENTRY(jint, getInt, (JNIEnv* env, jobject, jobject x, long displacement))
  if (x == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Handle xobj = JVMCIENV->asConstant(JVMCIENV->wrap(x), JVMCI_CHECK_0);
  return xobj->int_field(displacement);
}

C2V_VMENTRY(jlong, getLong, (JNIEnv* env, jobject, jobject x, long displacement))
  if (x == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Handle xobj = JVMCIENV->asConstant(JVMCIENV->wrap(x), JVMCI_CHECK_0);
  return xobj->long_field(displacement);
}

C2V_VMENTRY(jobject, getObject, (JNIEnv* env, jobject, jobject x, long displacement))
  if (x == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Handle xobj = JVMCIENV->asConstant(JVMCIENV->wrap(x), JVMCI_CHECK_0);
  oop res = xobj->obj_field(displacement);
  JVMCIObject result = JVMCIENV->get_object_constant(res);
  return JVMCIENV->get_jobject(result);
}

C2V_VMENTRY(jlong, registerNativeMethods, (JNIEnv* env, jobject, jclass mirror))
  void* shared_library = JVMCIEnv::get_shared_library_handle();
  if (shared_library == NULL) {
    JVMCI_THROW_MSG_0(UnsatisfiedLinkError, "JVMCI shared library is unavailable");
  }
  if (mirror == NULL) {
    JVMCI_THROW_0(NullPointerException);
  }
  Klass* klass = java_lang_Class::as_Klass(JNIHandles::resolve(mirror));
  if (klass == NULL || !klass->oop_is_instance()) {
    JVMCI_THROW_MSG_0(IllegalArgumentException, "clazz is for primitive type");
  }

  InstanceKlass* iklass = InstanceKlass::cast(klass);
  for (int i = 0; i < iklass->methods()->length(); i++) {
    Method* method = iklass->methods()->at(i);
    if (method->is_native()) {
      if (method->has_native_function()) {
        JVMCI_THROW_MSG_0(UnsatisfiedLinkError, err_msg("Cannot overwrite existing native implementation for %s",
            method->name_and_sig_as_C_string()));
      }

      // Compute argument size
      int args_size = 1                             // JNIEnv
                    + (method->is_static() ? 1 : 0) // class for static methods
                    + method->size_of_parameters(); // actual parameters

      // 1) Try JNI short style
      stringStream st;
      char* pure_name = NativeLookup::pure_jni_name(method);
      os::print_jni_name_prefix_on(&st, args_size);
      st.print_raw(pure_name);
      os::print_jni_name_suffix_on(&st, args_size);
      char* jni_name = st.as_string();

      address entry = (address) os::dll_lookup(shared_library, jni_name);
      if (entry == NULL) {
        // 2) Try JNI long style
        st.reset();
        char* long_name = NativeLookup::long_jni_name(method);
        os::print_jni_name_prefix_on(&st, args_size);
        st.print_raw(pure_name);
        st.print_raw(long_name);
        os::print_jni_name_suffix_on(&st, args_size);
        jni_name = st.as_string();
      }
      if (entry == NULL) {
        JVMCI_THROW_MSG_0(UnsatisfiedLinkError, method->name_and_sig_as_C_string());
      }
      method->set_native_function(entry, Method::native_bind_event_is_interesting);
      if (PrintJNIResolving) {
        tty->print_cr("[Dynamic-linking native method %s.%s ... JNI]",
          method->method_holder()->external_name(),
          method->name()->as_C_string());
      }
    }
  }
  return (jlong) (address) JVMCIEnv::get_shared_library_javavm();
}

C2V_VMENTRY(jlong, translate, (JNIEnv* env, jobject, jobject obj_handle))
  if (obj_handle == NULL) {
    return 0L;
  }
  JVMCIEnv __peer_jvmci_env__(!JVMCIENV->is_hotspot());
  JVMCIEnv* peerEnv = &__peer_jvmci_env__;
  JVMCIEnv* thisEnv = JVMCIENV;

  JVMCIObject obj = thisEnv->wrap(obj_handle);
  JVMCIObject result;
  if (thisEnv->isa_HotSpotResolvedJavaMethodImpl(obj)) {
    Method* method = thisEnv->asMethod(obj);
    result = peerEnv->get_jvmci_method(method, JVMCI_CHECK_0);
  } else if (thisEnv->isa_HotSpotResolvedObjectTypeImpl(obj)) {
    Klass* klass = thisEnv->asKlass(obj);
    result = peerEnv->get_jvmci_type(klass, JVMCI_CHECK_0);
  } else if (thisEnv->isa_HotSpotResolvedPrimitiveType(obj)) {
    BasicType type = JVMCIENV->kindToBasicType(JVMCIENV->get_HotSpotResolvedPrimitiveType_kind(obj), JVMCI_CHECK_0);
    result = peerEnv->get_jvmci_primitive_type(type);
  } else if (thisEnv->isa_IndirectHotSpotObjectConstantImpl(obj)) {
    oop constant = thisEnv->asConstant(obj, JVMCI_CHECK_0);
    result = peerEnv->get_object_constant(constant);
  } else if (thisEnv->isa_DirectHotSpotObjectConstantImpl(obj)) {
    oop constant = thisEnv->asConstant(obj, JVMCI_CHECK_0);
    result = peerEnv->get_object_constant(constant);
  } else if (thisEnv->isa_HotSpotNmethod(obj)) {
    nmethod* nm = thisEnv->asNmethod(obj);
    if (nm != NULL) {
      JVMCINMethodData* data = nm->jvmci_nmethod_data();
      if (data != NULL) {
        // First check if an InstalledCode instance already exists in the appropriate runtime
        JVMCIObject peer_installed_code = data->get_nmethod_mirror();
        if (!peer_installed_code.is_null() && peer_installed_code.is_hotspot() != obj.is_hotspot()) {
          nmethod* peer_nm = peerEnv->asNmethod(peer_installed_code);
          if (peer_nm == nm) {
            result = peer_installed_code;
          }
        }
      }
    }
    if (result.is_null()) {
      JVMCIObject methodObject = thisEnv->get_HotSpotNmethod_method(obj);
      methodHandle mh = thisEnv->asMethod(methodObject);
      jboolean isDefault = thisEnv->get_HotSpotNmethod_isDefault(obj);
      JVMCIObject name_string = thisEnv->get_InstalledCode_name(obj);
      const char* cstring = name_string.is_null() ? NULL : thisEnv->as_utf8_string(name_string);
      // Create a new HotSpotNmethod instance in the peer runtime
      result = peerEnv->new_HotSpotNmethod(mh(), cstring, isDefault, JVMCI_CHECK_0);
      if (nm == NULL) {
        // nmethod must have been unloaded
      } else {
        // Link the new HotSpotNmethod to the nmethod
        peerEnv->initialize_installed_code(result, nm, JVMCI_CHECK_0);
        JVMCINMethodData* data = nm->jvmci_nmethod_data();
        data->add_nmethod_mirror(peerEnv, result, JVMCI_CHECK_0);
      }
    }
  } else {
    JVMCI_THROW_MSG_0(IllegalArgumentException,
                err_msg("Cannot translate object of type: %s", thisEnv->klass_name(obj)));
  }
  return (jlong) peerEnv->make_global(result).as_jobject();
}

C2V_VMENTRY(jobject, unhand, (JNIEnv* env, jobject, jlong obj_handle))
  if (obj_handle == 0L) {
    return NULL;
  }
  jobject global_handle = (jobject) obj_handle;
  JVMCIObject global_handle_obj = JVMCIENV->wrap((jobject) obj_handle);
  jobject result = JVMCIENV->make_local(global_handle_obj).as_jobject();
  JVMCIENV->destroy_global(global_handle_obj);
  return result;
}

C2V_VMENTRY(void, updateHotSpotNmethodHandle, (JNIEnv* env, jobject, jobject code_handle))
  JVMCIObject code = JVMCIENV->wrap(code_handle);
  // Execute this operation for the side effect of updating the InstalledCode state
  JVMCIENV->asNmethod(code);
}

C2V_VMENTRY(jbyteArray, getCode, (JNIEnv* env, jobject, jobject code_handle))
  JVMCIObject code = JVMCIENV->wrap(code_handle);
  CodeBlob* cb = JVMCIENV->asCodeBlob(code);
  if (cb == NULL) {
    return NULL;
  }
  int code_size = cb->code_size();
  JVMCIPrimitiveArray result = JVMCIENV->new_byteArray(code_size, JVMCI_CHECK_NULL);
  JVMCIENV->copy_bytes_from((jbyte*) cb->code_begin(), result, 0, code_size);
  return JVMCIENV->get_jbyteArray(result);
}

#define CC (char*)  /*cast a literal from (const char*)*/
#define FN_PTR(f) CAST_FROM_FN_PTR(void*, &(c2v_ ## f))

#define STRING                  "Ljava/lang/String;"
#define OBJECT                  "Ljava/lang/Object;"
#define CLASS                   "Ljava/lang/Class;"
#define OBJECTCONSTANT          "Ljdk/vm/ci/hotspot/HotSpotObjectConstantImpl;"
#define HANDLECONSTANT          "Ljdk/vm/ci/hotspot/IndirectHotSpotObjectConstantImpl;"
#define EXECUTABLE              "Ljava/lang/reflect/Executable;"
#define STACK_TRACE_ELEMENT     "Ljava/lang/StackTraceElement;"
#define INSTALLED_CODE          "Ljdk/vm/ci/code/InstalledCode;"
#define TARGET_DESCRIPTION      "Ljdk/vm/ci/code/TargetDescription;"
#define BYTECODE_FRAME          "Ljdk/vm/ci/code/BytecodeFrame;"
#define JAVACONSTANT            "Ljdk/vm/ci/meta/JavaConstant;"
#define INSPECTED_FRAME_VISITOR "Ljdk/vm/ci/code/stack/InspectedFrameVisitor;"
#define RESOLVED_METHOD         "Ljdk/vm/ci/meta/ResolvedJavaMethod;"
#define HS_RESOLVED_METHOD      "Ljdk/vm/ci/hotspot/HotSpotResolvedJavaMethodImpl;"
#define HS_RESOLVED_KLASS       "Ljdk/vm/ci/hotspot/HotSpotResolvedObjectTypeImpl;"
#define HS_RESOLVED_TYPE        "Ljdk/vm/ci/hotspot/HotSpotResolvedJavaType;"
#define HS_RESOLVED_FIELD       "Ljdk/vm/ci/hotspot/HotSpotResolvedJavaField;"
#define HS_INSTALLED_CODE       "Ljdk/vm/ci/hotspot/HotSpotInstalledCode;"
#define HS_NMETHOD              "Ljdk/vm/ci/hotspot/HotSpotNmethod;"
#define HS_NMETHOD_HANDLE       "Ljdk/vm/ci/hotspot/HotSpotNmethodHandle;"
#define HS_CONSTANT_POOL        "Ljdk/vm/ci/hotspot/HotSpotConstantPool;"
#define HS_COMPILED_CODE        "Ljdk/vm/ci/hotspot/HotSpotCompiledCode;"
#define HS_STACK_FRAME_REF      "Ljdk/vm/ci/hotspot/HotSpotStackFrameReference;"
#define HS_SPECULATION_LOG      "Ljdk/vm/ci/hotspot/HotSpotSpeculationLog;"
#define METASPACE_OBJECT        "Ljdk/vm/ci/hotspot/MetaspaceObject;"
#define METASPACE_METHOD_DATA   "J"

JNINativeMethod CompilerToVM::methods[] = {
  {CC"getBytecode",                                  CC"("HS_RESOLVED_METHOD")[B",                                                     FN_PTR(getBytecode)},
  {CC"getExceptionTableStart",                       CC"("HS_RESOLVED_METHOD")J",                                                      FN_PTR(getExceptionTableStart)},
  {CC"getExceptionTableLength",                      CC"("HS_RESOLVED_METHOD")I",                                                      FN_PTR(getExceptionTableLength)},
  {CC"findUniqueConcreteMethod",                     CC"("HS_RESOLVED_KLASS HS_RESOLVED_METHOD")"HS_RESOLVED_METHOD,                   FN_PTR(findUniqueConcreteMethod)},
  {CC"getImplementor",                               CC"("HS_RESOLVED_KLASS")"HS_RESOLVED_KLASS,                                       FN_PTR(getImplementor)},
  {CC"getStackTraceElement",                         CC"("HS_RESOLVED_METHOD"I)"STACK_TRACE_ELEMENT,                                   FN_PTR(getStackTraceElement)},
  {CC"methodIsIgnoredBySecurityStackWalk",           CC"("HS_RESOLVED_METHOD")Z",                                                      FN_PTR(methodIsIgnoredBySecurityStackWalk)},
  {CC"setNotInlinableOrCompilable",                  CC"("HS_RESOLVED_METHOD")V",                                                      FN_PTR(setNotInlinableOrCompilable)},
  {CC"isCompilable",                                 CC"("HS_RESOLVED_METHOD")Z",                                                      FN_PTR(isCompilable)},
  {CC"hasNeverInlineDirective",                      CC"("HS_RESOLVED_METHOD")Z",                                                      FN_PTR(hasNeverInlineDirective)},
  {CC"shouldInlineMethod",                           CC"("HS_RESOLVED_METHOD")Z",                                                      FN_PTR(shouldInlineMethod)},
  {CC"lookupType",                                   CC"("STRING HS_RESOLVED_KLASS"Z)"HS_RESOLVED_TYPE,                                FN_PTR(lookupType)},
  {CC"lookupClass",                                  CC"("CLASS")"HS_RESOLVED_TYPE,                                                    FN_PTR(lookupClass)},
  {CC"lookupNameInPool",                             CC"("HS_CONSTANT_POOL"I)"STRING,                                                  FN_PTR(lookupNameInPool)},
  {CC"lookupNameAndTypeRefIndexInPool",              CC"("HS_CONSTANT_POOL"I)I",                                                       FN_PTR(lookupNameAndTypeRefIndexInPool)},
  {CC"lookupSignatureInPool",                        CC"("HS_CONSTANT_POOL"I)"STRING,                                                  FN_PTR(lookupSignatureInPool)},
  {CC"lookupKlassRefIndexInPool",                    CC"("HS_CONSTANT_POOL"I)I",                                                       FN_PTR(lookupKlassRefIndexInPool)},
  {CC"lookupKlassInPool",                            CC"("HS_CONSTANT_POOL"I)Ljava/lang/Object;",                                      FN_PTR(lookupKlassInPool)},
  {CC"lookupAppendixInPool",                         CC"("HS_CONSTANT_POOL"I)"OBJECTCONSTANT,                                          FN_PTR(lookupAppendixInPool)},
  {CC"lookupMethodInPool",                           CC"("HS_CONSTANT_POOL"IB)"HS_RESOLVED_METHOD,                                     FN_PTR(lookupMethodInPool)},
  {CC"constantPoolRemapInstructionOperandFromCache", CC"("HS_CONSTANT_POOL"I)I",                                                       FN_PTR(constantPoolRemapInstructionOperandFromCache)},
  {CC"resolveConstantInPool",                        CC"("HS_CONSTANT_POOL"I)"OBJECTCONSTANT,                                          FN_PTR(resolveConstantInPool)},
  {CC"resolvePossiblyCachedConstantInPool",          CC"("HS_CONSTANT_POOL"I)"OBJECTCONSTANT,                                          FN_PTR(resolvePossiblyCachedConstantInPool)},
  {CC"resolveTypeInPool",                            CC"("HS_CONSTANT_POOL"I)"HS_RESOLVED_KLASS,                                       FN_PTR(resolveTypeInPool)},
  {CC"resolveFieldInPool",                           CC"("HS_CONSTANT_POOL"IB[I)"HS_RESOLVED_KLASS,                                    FN_PTR(resolveFieldInPool)},
  {CC"resolveInvokeDynamicInPool",                   CC"("HS_CONSTANT_POOL"I)V",                                                       FN_PTR(resolveInvokeDynamicInPool)},
  {CC"resolveInvokeHandleInPool",                    CC"("HS_CONSTANT_POOL"I)V",                                                       FN_PTR(resolveInvokeHandleInPool)},
  {CC"resolveMethod",                                CC"("HS_RESOLVED_KLASS HS_RESOLVED_METHOD HS_RESOLVED_KLASS")"HS_RESOLVED_METHOD, FN_PTR(resolveMethod)},
  {CC"getSignaturePolymorphicHolders",               CC"()[" STRING,                                                                   FN_PTR(getSignaturePolymorphicHolders)},
  {CC"getVtableIndexForInterfaceMethod",             CC"("HS_RESOLVED_KLASS HS_RESOLVED_METHOD")I",                                    FN_PTR(getVtableIndexForInterfaceMethod)},
  {CC"getClassInitializer",                          CC"("HS_RESOLVED_KLASS")"HS_RESOLVED_METHOD,                                      FN_PTR(getClassInitializer)},
  {CC"hasFinalizableSubclass",                       CC"("HS_RESOLVED_KLASS")Z",                                                       FN_PTR(hasFinalizableSubclass)},
  {CC"getMaxCallTargetOffset",                       CC"(J)J",                                                                         FN_PTR(getMaxCallTargetOffset)},
  {CC"asResolvedJavaMethod",                         CC"(" EXECUTABLE ")" HS_RESOLVED_METHOD,                                          FN_PTR(asResolvedJavaMethod)},
  {CC"getResolvedJavaMethod",                        CC"("OBJECTCONSTANT"J)"HS_RESOLVED_METHOD,                                        FN_PTR(getResolvedJavaMethod)},
  {CC"getConstantPool",                              CC"("METASPACE_OBJECT")"HS_CONSTANT_POOL,                                         FN_PTR(getConstantPool)},
  {CC"getResolvedJavaType0",                         CC"(Ljava/lang/Object;JZ)"HS_RESOLVED_KLASS,                                      FN_PTR(getResolvedJavaType0)},
  {CC"readConfiguration",                            CC"()[Ljava/lang/Object;",                                                        FN_PTR(readConfiguration)},
  {CC"installCode",                                  CC"("TARGET_DESCRIPTION HS_COMPILED_CODE INSTALLED_CODE HS_SPECULATION_LOG")I",   FN_PTR(installCode)},
  {CC"resetCompilationStatistics",                   CC"()V",                                                                          FN_PTR(resetCompilationStatistics)},
  {CC"disassembleCodeBlob",                          CC"("INSTALLED_CODE")"STRING,                                                     FN_PTR(disassembleCodeBlob)},
  {CC"executeHotSpotNmethod",                        CC"(["OBJECT HS_NMETHOD")"OBJECT,                                                 FN_PTR(executeHotSpotNmethod)},
  {CC"getLineNumberTable",                           CC"("HS_RESOLVED_METHOD")[J",                                                     FN_PTR(getLineNumberTable)},
  {CC"getLocalVariableTableStart",                   CC"("HS_RESOLVED_METHOD")J",                                                      FN_PTR(getLocalVariableTableStart)},
  {CC"getLocalVariableTableLength",                  CC"("HS_RESOLVED_METHOD")I",                                                      FN_PTR(getLocalVariableTableLength)},
  {CC"reprofile",                                    CC"("HS_RESOLVED_METHOD")V",                                                      FN_PTR(reprofile)},
  {CC"invalidateHotSpotNmethod",                     CC"("HS_NMETHOD")V",                                                              FN_PTR(invalidateHotSpotNmethod)},
  {CC"readUncompressedOop",                          CC"(J)"OBJECTCONSTANT,                                                            FN_PTR(readUncompressedOop)},
  {CC"collectCounters",                              CC"()[J",                                                                         FN_PTR(collectCounters)},
  {CC"allocateCompileId",                            CC"("HS_RESOLVED_METHOD"I)I",                                                     FN_PTR(allocateCompileId)},
  {CC"isMature",                                     CC"("METASPACE_METHOD_DATA")Z",                                                   FN_PTR(isMature)},
  {CC"hasCompiledCodeForOSR",                        CC"("HS_RESOLVED_METHOD"II)Z",                                                    FN_PTR(hasCompiledCodeForOSR)},
  {CC"getSymbol",                                    CC"(J)"STRING,                                                                    FN_PTR(getSymbol)},
  {CC"iterateFrames",                                CC"(["RESOLVED_METHOD "["RESOLVED_METHOD "I" INSPECTED_FRAME_VISITOR ")"OBJECT,   FN_PTR(iterateFrames)},
  {CC"materializeVirtualObjects",                    CC"("HS_STACK_FRAME_REF"Z)V",                                                     FN_PTR(materializeVirtualObjects)},
  {CC"shouldDebugNonSafepoints",                     CC"()Z",                                                                          FN_PTR(shouldDebugNonSafepoints)},
  {CC"writeDebugOutput",                             CC"([BII)V",                                                                      FN_PTR(writeDebugOutput)},
  {CC"flushDebugOutput",                             CC"()V",                                                                          FN_PTR(flushDebugOutput)},
  {CC"writeCompileLogOutput",                        CC"([BII)V",                                                                      FN_PTR(writeCompileLogOutput)},
  {CC"flushCompileLogOutput",                        CC"()V",                                                                          FN_PTR(flushCompileLogOutput)},
  {CC"methodDataProfileDataSize",                    CC"(JI)I",                                                                        FN_PTR(methodDataProfileDataSize)},
  {CC"getHostClass",                                 CC"("HS_RESOLVED_KLASS")"HS_RESOLVED_KLASS,                                       FN_PTR(getHostClass)},
  {CC"interpreterFrameSize",                         CC"("BYTECODE_FRAME")I",                                                          FN_PTR(interpreterFrameSize)},
  {CC"compileToBytecode",                            CC"("OBJECTCONSTANT")V",                                                          FN_PTR(compileToBytecode)},
  {CC"getFlagValue",                                 CC"("STRING")"OBJECT,                                                             FN_PTR(getFlagValue)},
  {CC"getObjectAtAddress",                           CC"(J)"OBJECT,                                                                    FN_PTR(getObjectAtAddress)},
  {CC"getInterfaces",                                CC"("HS_RESOLVED_KLASS ")["HS_RESOLVED_KLASS,                                     FN_PTR(getInterfaces)},
  {CC"getComponentType",                             CC"("HS_RESOLVED_KLASS ")"HS_RESOLVED_TYPE,                                       FN_PTR(getComponentType)},
  {CC"ensureInitialized",                            CC"("HS_RESOLVED_KLASS ")V",                                                      FN_PTR(ensureInitialized)},
  {CC"getIdentityHashCode",                          CC"("OBJECTCONSTANT")I",                                                          FN_PTR(getIdentityHashCode)},
  {CC"isInternedString",                             CC"("OBJECTCONSTANT")Z",                                                          FN_PTR(isInternedString)},
  {CC"unboxPrimitive",                               CC"("OBJECTCONSTANT")" OBJECT,                                                    FN_PTR(unboxPrimitive)},
  {CC"boxPrimitive",                                 CC"("OBJECT")" OBJECTCONSTANT,                                                    FN_PTR(boxPrimitive)},
  {CC"getDeclaredConstructors",                      CC"("HS_RESOLVED_KLASS")[" RESOLVED_METHOD,                                       FN_PTR(getDeclaredConstructors)},
  {CC"getDeclaredMethods",                           CC"("HS_RESOLVED_KLASS")[" RESOLVED_METHOD,                                       FN_PTR(getDeclaredMethods)},
  {CC"readFieldValue",                               CC"("HS_RESOLVED_KLASS HS_RESOLVED_FIELD"Z)" JAVACONSTANT,                        FN_PTR(readFieldValue)},
  {CC"readFieldValue",                               CC"("OBJECTCONSTANT HS_RESOLVED_FIELD"Z)" JAVACONSTANT,                           FN_PTR(readFieldValue)},
  {CC"isInstance",                                   CC"("HS_RESOLVED_KLASS OBJECTCONSTANT")Z",                                        FN_PTR(isInstance)},
  {CC"isAssignableFrom",                             CC"("HS_RESOLVED_KLASS HS_RESOLVED_KLASS")Z",                                     FN_PTR(isAssignableFrom)},
  {CC"asJavaType",                                   CC"("OBJECTCONSTANT")"HS_RESOLVED_TYPE,                                           FN_PTR(asJavaType)},
  {CC"asString",                                     CC"("OBJECTCONSTANT")"STRING,                                                     FN_PTR(asString)},
  {CC"equals",                                       CC"("OBJECTCONSTANT"J"OBJECTCONSTANT"J)Z",                                        FN_PTR(equals)},
  {CC"getJavaMirror",                                CC"("HS_RESOLVED_TYPE")" OBJECTCONSTANT,                                          FN_PTR(getJavaMirror)},
  {CC"getArrayLength",                               CC"("OBJECTCONSTANT")I",                                                          FN_PTR(getArrayLength)},
  {CC"readArrayElement",                             CC"("OBJECTCONSTANT"I)Ljava/lang/Object;",                                        FN_PTR(readArrayElement)},
  {CC"arrayBaseOffset",                              CC"(Ljdk/vm/ci/meta/JavaKind;)I",                                                 FN_PTR(arrayBaseOffset)},
  {CC"arrayIndexScale",                              CC"(Ljdk/vm/ci/meta/JavaKind;)I",                                                 FN_PTR(arrayIndexScale)},
  {CC"getByte",                                      CC"("OBJECTCONSTANT"J)B",                                                         FN_PTR(getByte)},
  {CC"getShort",                                     CC"("OBJECTCONSTANT"J)S",                                                         FN_PTR(getShort)},
  {CC"getInt",                                       CC"("OBJECTCONSTANT"J)I",                                                         FN_PTR(getInt)},
  {CC"getLong",                                      CC"("OBJECTCONSTANT"J)J",                                                         FN_PTR(getLong)},
  {CC"getObject",                                    CC"("OBJECTCONSTANT"J)"OBJECTCONSTANT,                                            FN_PTR(getObject)},
  {CC"registerNativeMethods",                        CC"("CLASS")J",                                                                   FN_PTR(registerNativeMethods)},
  {CC"translate",                                    CC"("OBJECT")J",                                                                  FN_PTR(translate)},
  {CC"unhand",                                       CC"(J)"OBJECT,                                                                    FN_PTR(unhand)},
  {CC"updateHotSpotNmethodHandle",                   CC"("HS_NMETHOD_HANDLE")V",                                                       FN_PTR(updateHotSpotNmethodHandle)},
  {CC"getCode",                                      CC"("HS_INSTALLED_CODE")[B",                                                      FN_PTR(getCode)},
};

int CompilerToVM::methods_count() {
  return sizeof(methods) / sizeof(JNINativeMethod);
}
