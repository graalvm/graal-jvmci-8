/*
 * Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.
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
#include "asm/codeBuffer.hpp"
#include "code/codeCache.hpp"
#include "compiler/compileBroker.hpp"
#include "compiler/disassembler.hpp"
#include "jvmci/jvmciRuntime.hpp"
#include "jvmci/jvmciCompilerToVM.hpp"
#include "jvmci/jvmciCompiler.hpp"
#include "jvmci/jvmciJavaClasses.hpp"
#include "jvmci/jvmciEnv.hpp"
#include "memory/oopFactory.hpp"
#include "oops/oop.inline.hpp"
#include "oops/instanceMirrorKlass.hpp"
#include "prims/jvm.h"
#include "runtime/biasedLocking.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/reflection.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/sweeper.hpp"
#include "utilities/debug.hpp"
#include "utilities/defaultStream.hpp"

#if defined(_MSC_VER)
#define strtoll _strtoi64
#endif

#ifdef ASSERT
#define METADATA_TRACK_NAMES
#endif

struct _jmetadata {
 private:
  Metadata* _handle;
#ifdef METADATA_TRACK_NAMES
  // Debug data for tracking stale metadata
  const char* _name;
#endif

 public:
  Metadata* handle() { return _handle; }

#ifdef METADATA_TRACK_NAMES
  void initialize() {
    _handle = NULL;
    _name = NULL;
  }
#endif

  void set_handle(Metadata* value) {
    _handle = value;
  }

#ifdef METADATA_TRACK_NAMES
  const char* name() { return _name; }
  void set_name(const char* name) {
    if (_name != NULL) {
      os::free((void*) _name);
      _name = NULL;
    }
    if (name != NULL) {
      _name = os::strdup(name);
    }
  }
#endif
};
typedef struct _jmetadata HandleRecord;

// JVMCI maintains direct references to metadata. To make these references safe in the face of
// class redefinition, they are held in handles so they can be scanned during GC. They are
// managed in a cooperative way between the Java code and HotSpot. A handle is filled in and
// passed back to the Java code which is responsible for setting the handle to NULL when it
// is no longer in use. This is done by jdk.vm.ci.hotspot.HandleCleaner. The
// rebuild_free_list function notices when the handle is clear and reclaims it for re-use.
class MetadataHandleBlock : public CHeapObj<mtInternal> {
 private:
  enum SomeConstants {
    block_size_in_handles  = 32,      // Number of handles per handle block
    ptr_tag = 1,
    ptr_mask = ~((intptr_t)ptr_tag)
  };

  // Free handles always have their low bit set so those pointers can
  // be distinguished from handles which are in use.  The last handle
  // on the free list has a NULL pointer with the tag bit set, so it's
  // clear that the handle has been reclaimed.  The _free_list is
  // always a real pointer to a handle.

  HandleRecord    _handles[block_size_in_handles]; // The handles
  int             _top;                         // Index of next unused handle
  MetadataHandleBlock* _next;                   // Link to next block

  // The following instance variables are only used by the first block in a chain.
  // Having two types of blocks complicates the code and the space overhead is negligible.
  MetadataHandleBlock* _last;                   // Last block in use
  intptr_t        _free_list;                   // Handle free list
  int             _allocate_before_rebuild;     // Number of blocks to allocate before rebuilding free list

  jmetadata allocate_metadata_handle(Metadata* metadata);
  void rebuild_free_list();

  MetadataHandleBlock() {
    _top = 0;
    _next = NULL;
    _last = this;
    _free_list = 0;
    _allocate_before_rebuild = 0;
#ifdef METADATA_TRACK_NAMES
    for (int i = 0; i < block_size_in_handles; i++) {
      _handles[i].initialize();
    }
#endif
  }

  const char* get_name(int index) {
#ifdef METADATA_TRACK_NAMES
    return _handles[index].name();
#else
    return "<missing>";
#endif
  }

 public:
  jmetadata allocate_handle(methodHandle handle)       { return allocate_metadata_handle(handle()); }
  jmetadata allocate_handle(constantPoolHandle handle) { return allocate_metadata_handle(handle()); }

  static MetadataHandleBlock* allocate_block();

  // Adds `handle` to the free list in this block
  void chain_free_list(HandleRecord* handle) {
    handle->set_handle((Metadata*) (ptr_tag | _free_list));
#ifdef METADATA_TRACK_NAMES
    handle->set_name(NULL);
#endif
    _free_list = (intptr_t) handle;
  }

  HandleRecord* get_free_handle() {
    assert(_free_list != 0, "should check before calling");
    HandleRecord* handle = (HandleRecord*) (_free_list & ptr_mask);
    _free_list = (ptr_mask & (intptr_t) (handle->handle()));
    assert(_free_list != ptr_tag, "should be null");
    handle->set_handle(NULL);
    return handle;
  }

  void metadata_do(void f(Metadata*));

  void do_unloading(BoolObjectClosure* is_alive);
};

jmetadata MetadataHandleBlock::allocate_metadata_handle(Metadata* obj) {
  assert(obj->is_valid() && obj->is_metadata(), "must be");

  // Try last block
  HandleRecord* handle = NULL;
  if (_last->_top < block_size_in_handles) {
    handle = &(_last->_handles)[_last->_top++];
  } else if (_free_list != 0) {
    // Try free list
    handle = get_free_handle();
  }

  if (handle != NULL) {
    handle->set_handle(obj);
#ifdef METADATA_TRACK_NAMES
    handle->set_name(obj->print_value_string());
#endif
    return (jmetadata) handle;
  }

  // Check if unused block follow last
  if (_last->_next != NULL) {
    // update last and retry
    _last = _last->_next;
    return allocate_metadata_handle(obj);
  }

  // No space available, we have to rebuild free list or expand
  if (_allocate_before_rebuild == 0) {
    rebuild_free_list();        // updates _allocate_before_rebuild counter
  } else {
    // Append new block
    // This can block, but the caller has a metadata handle around this object.
    _last->_next = allocate_block();
    _last = _last->_next;
    _allocate_before_rebuild--;
  }
  return allocate_metadata_handle(obj);  // retry
}

void MetadataHandleBlock::rebuild_free_list() {
  assert(_allocate_before_rebuild == 0 && _free_list == 0, "just checking");
  int free = 0;
  int blocks = 0;
  for (MetadataHandleBlock* current = this; current != NULL; current = current->_next) {
    for (int index = 0; index < current->_top; index++) {
      HandleRecord* handle = &(current->_handles)[index];
      if (handle->handle() == NULL) {
        // this handle was cleared out by a delete call, reuse it
        chain_free_list(handle);
        free++;
      }
    }
    // we should not rebuild free list if there are unused handles at the end
    assert(current->_top == block_size_in_handles, "just checking");
    blocks++;
  }
  // Heuristic: if more than half of the handles are free we rebuild next time
  // as well, otherwise we append a corresponding number of new blocks before
  // attempting a free list rebuild again.
  int total = blocks * block_size_in_handles;
  int extra = total - 2*free;
  if (extra > 0) {
    // Not as many free handles as we would like - compute number of new blocks to append
    _allocate_before_rebuild = (extra + block_size_in_handles - 1) / block_size_in_handles;
  }
  if (TraceJNIHandleAllocation) {
    tty->print_cr("Rebuild free list MetadataHandleBlock " PTR_FORMAT " blocks=%d used=%d free=%d add=%d",
                  p2i(this), blocks, total-free, free, _allocate_before_rebuild);
  }
}

MetadataHandleBlock* MetadataHandleBlock::allocate_block() {
  return new MetadataHandleBlock();
}

void MetadataHandleBlock::metadata_do(void f(Metadata*)) {
  for (MetadataHandleBlock* current = this; current != NULL; current = current->_next) {
    for (int index = 0; index < current->_top; index++) {
      HandleRecord* root = &(current->_handles)[index];
      Metadata* value = root->handle();
      // traverse heap pointers only, not deleted handles or free list
      // pointers
      if (value != NULL && ((intptr_t) value & ptr_tag) == 0) {
        assert(value->is_valid(), err_msg("invalid metadata %s", get_name(index)));
        f(value);
      }
    }
    // the next handle block is valid only if current block is full
    if (current->_top < block_size_in_handles) {
      break;
    }
  }
}

// Visit any live metadata handles and clean them up.  Since clearing of these handles is driven by
// weak references they will be cleared at some point in the future when the reference cleaning logic is run.
void MetadataHandleBlock::do_unloading(BoolObjectClosure* is_alive) {
  for (MetadataHandleBlock* current = this; current != NULL; current = current->_next) {
    for (int index = 0; index < current->_top; index++) {
      HandleRecord* handle = &(current->_handles)[index];
      Metadata* value = handle->handle();
      // traverse heap pointers only, not deleted handles or free list
      // pointers
      if (value != NULL && ((intptr_t) value & ptr_tag) == 0) {
        Klass* klass = NULL;
        if (value->is_klass()) {
          klass = (Klass*)value;
        } else if (value->is_method()) {
          Method* m = (Method*)value;
          klass = m->method_holder();
        } else if (value->is_constantPool()) {
          ConstantPool* cp = (ConstantPool*)value;
          klass = cp->pool_holder();
        } else {
          ShouldNotReachHere();
        }
        if (klass->class_loader_data()->is_unloading()) {
          // This needs to be marked so that it's no longer scanned
          // but can't be put on the free list yet. The
          // ReferenceCleaner will set this to NULL and
          // put it on the free list.
          jlong old_value = Atomic::cmpxchg((jlong) (ptr_tag), (jlong*)handle, (jlong) value);
          if (old_value == (jlong) value) {
            // Success
          } else {
            guarantee(old_value == 0, "only other possible value");
          }
        }
      }
    }
    // the next handle block is valid only if current block is full
    if (current->_top < block_size_in_handles) {
      break;
    }
  }
}

JNIHandleBlock* JVMCI::_object_handles = NULL;
MetadataHandleBlock* JVMCI::_metadata_handles = NULL;
JVMCIRuntime* JVMCI::_compiler_runtime = NULL;
JVMCIRuntime* JVMCI::_java_runtime = NULL;

// Simple helper to see if the caller of a runtime stub which
// entered the VM has been deoptimized

static bool caller_is_deopted() {
  JavaThread* thread = JavaThread::current();
  RegisterMap reg_map(thread, false);
  frame runtime_frame = thread->last_frame();
  frame caller_frame = runtime_frame.sender(&reg_map);
  assert(caller_frame.is_compiled_frame(), "must be compiled");
  return caller_frame.is_deoptimized_frame();
}

// Stress deoptimization
static void deopt_caller() {
  if ( !caller_is_deopted()) {
    JavaThread* thread = JavaThread::current();
    RegisterMap reg_map(thread, false);
    frame runtime_frame = thread->last_frame();
    frame caller_frame = runtime_frame.sender(&reg_map);
    Deoptimization::deoptimize_frame(thread, caller_frame.id(), Deoptimization::Reason_constraint);
    assert(caller_is_deopted(), "Must be deoptimized");
  }
}

// Manages a scope for a JVMCI runtime call that attempts a heap allocation.
// If there is a pending exception upon closing the scope and the runtime
// call is of the variety where allocation failure returns NULL without an
// exception, the following action is taken:
//   1. The pending exception is cleared
//   2. NULL is written to JavaThread::_vm_result
//   3. Checks that an OutOfMemoryError is Universe::out_of_memory_error_retry().
class RetryableAllocationMark: public StackObj {
 private:
  JavaThread* _thread;
 public:
  RetryableAllocationMark(JavaThread* thread, bool activate) {
    if (activate) {
      assert(!thread->in_retryable_allocation(), "retryable allocation scope is non-reentrant");
      _thread = thread;
      _thread->set_in_retryable_allocation(true);
    } else {
      _thread = NULL;
    }
  }
  ~RetryableAllocationMark() {
    if (_thread != NULL) {
      _thread->set_in_retryable_allocation(false);
      JavaThread* THREAD = _thread;
      if (HAS_PENDING_EXCEPTION) {
        oop ex = PENDING_EXCEPTION;
        CLEAR_PENDING_EXCEPTION;
        oop retry_oome = Universe::out_of_memory_error_retry();
        if (ex->is_a(retry_oome->klass()) && retry_oome != ex) {
          ResourceMark rm;
          fatal(err_msg("Unexpected exception in scope of retryable allocation: " INTPTR_FORMAT " of type %s", p2i(ex), ex->klass()->external_name()));
        }
        _thread->set_vm_result(NULL);
      }
    }
  }
};

JRT_BLOCK_ENTRY(void, JVMCIRuntime::new_instance_common(JavaThread* thread, Klass* klass, bool null_on_fail))
  JRT_BLOCK;
  assert(klass->is_klass(), "not a class");
  Handle holder(THREAD, klass->klass_holder()); // keep the klass alive
  InstanceKlass* h = InstanceKlass::cast(klass);
  {
    RetryableAllocationMark ram(thread, null_on_fail);
    h->check_valid_for_instantiation(true, CHECK);
    oop obj;
    if (null_on_fail) {
      if (!h->is_initialized()) {
        // Cannot re-execute class initialization without side effects
        // so return without attempting the initialization
        return;
      }
    } else {
      // make sure klass is initialized
      h->initialize(CHECK);
    }
    // allocate instance and return via TLS
    obj = h->allocate_instance(CHECK);
    thread->set_vm_result(obj);
  }
  JRT_BLOCK_END;

  if (ReduceInitialCardMarks) {
    new_store_pre_barrier(thread);
  }
JRT_END

JRT_BLOCK_ENTRY(void, JVMCIRuntime::new_array_common(JavaThread* thread, Klass* array_klass, jint length, bool null_on_fail))
  JRT_BLOCK;
  // Note: no handle for klass needed since they are not used
  //       anymore after new_objArray() and no GC can happen before.
  //       (This may have to change if this code changes!)
  assert(array_klass->is_klass(), "not a class");
  oop obj;
  if (array_klass->oop_is_typeArray()) {
    BasicType elt_type = TypeArrayKlass::cast(array_klass)->element_type();
    RetryableAllocationMark ram(thread, null_on_fail);
    obj = oopFactory::new_typeArray(elt_type, length, CHECK);
  } else {
    Handle holder(THREAD, array_klass->klass_holder()); // keep the klass alive
    Klass* elem_klass = ObjArrayKlass::cast(array_klass)->element_klass();
    RetryableAllocationMark ram(thread, null_on_fail);
    obj = oopFactory::new_objArray(elem_klass, length, CHECK);
  }
  thread->set_vm_result(obj);
  // This is pretty rare but this runtime patch is stressful to deoptimization
  // if we deoptimize here so force a deopt to stress the path.
  if (DeoptimizeALot) {
    static int deopts = 0;
    // Alternate between deoptimizing and raising an error (which will also cause a deopt)
    if (deopts++ % 2 == 0) {
      if (null_on_fail) {
        return;
      } else {
        ResourceMark rm(THREAD);
        THROW(vmSymbols::java_lang_OutOfMemoryError());
      }
    } else {
      deopt_caller();
    }
  }
  JRT_BLOCK_END;

  if (ReduceInitialCardMarks) {
    new_store_pre_barrier(thread);
  }
JRT_END

void JVMCIRuntime::new_store_pre_barrier(JavaThread* thread) {
  // After any safepoint, just before going back to compiled code,
  // we inform the GC that we will be doing initializing writes to
  // this object in the future without emitting card-marks, so
  // GC may take any compensating steps.
  // NOTE: Keep this code consistent with GraphKit::store_barrier.

  oop new_obj = thread->vm_result();
  if (new_obj == NULL)  return;

  assert(Universe::heap()->can_elide_tlab_store_barriers(),
         "compiler must check this first");
  // GC may decide to give back a safer copy of new_obj.
  new_obj = Universe::heap()->new_store_pre_barrier(thread, new_obj);
  thread->set_vm_result(new_obj);
}

JRT_ENTRY(void, JVMCIRuntime::new_multi_array_common(JavaThread* thread, Klass* klass, int rank, jint* dims, bool null_on_fail))
  assert(klass->is_klass(), "not a class");
  assert(rank >= 1, "rank must be nonzero");
  Handle holder(THREAD, klass->klass_holder()); // keep the klass alive
  RetryableAllocationMark ram(thread, null_on_fail);
  oop obj = ArrayKlass::cast(klass)->multi_allocate(rank, dims, CHECK);
  thread->set_vm_result(obj);
JRT_END

JRT_ENTRY(void, JVMCIRuntime::dynamic_new_array_common(JavaThread* thread, oopDesc* element_mirror, jint length, bool null_on_fail))
  RetryableAllocationMark ram(thread, null_on_fail);
  oop obj = Reflection::reflect_new_array(element_mirror, length, CHECK);
  thread->set_vm_result(obj);
JRT_END

JRT_ENTRY(void, JVMCIRuntime::dynamic_new_instance_common(JavaThread* thread, oopDesc* type_mirror, bool null_on_fail))
  InstanceKlass* klass = InstanceKlass::cast(java_lang_Class::as_Klass(type_mirror));

  if (klass == NULL) {
    ResourceMark rm(THREAD);
    THROW(vmSymbols::java_lang_InstantiationException());
  }
  RetryableAllocationMark ram(thread, null_on_fail);

  // Create new instance (the receiver)
  klass->check_valid_for_instantiation(false, CHECK);

  if (null_on_fail) {
    if (!klass->is_initialized()) {
      // Cannot re-execute class initialization without side effects
      // so return without attempting the initialization
      return;
    }
  } else {
    // Make sure klass gets initialized
    klass->initialize(CHECK);
  }

  oop obj = klass->allocate_instance(CHECK);
  thread->set_vm_result(obj);
JRT_END

extern void vm_exit(int code);

// Enter this method from compiled code handler below. This is where we transition
// to VM mode. This is done as a helper routine so that the method called directly
// from compiled code does not have to transition to VM. This allows the entry
// method to see if the nmethod that we have just looked up a handler for has
// been deoptimized while we were in the vm. This simplifies the assembly code
// cpu directories.
//
// We are entering here from exception stub (via the entry method below)
// If there is a compiled exception handler in this method, we will continue there;
// otherwise we will unwind the stack and continue at the caller of top frame method
// Note: we enter in Java using a special JRT wrapper. This wrapper allows us to
// control the area where we can allow a safepoint. After we exit the safepoint area we can
// check to see if the handler we are going to return is now in a nmethod that has
// been deoptimized. If that is the case we return the deopt blob
// unpack_with_exception entry instead. This makes life for the exception blob easier
// because making that same check and diverting is painful from assembly language.
JRT_ENTRY_NO_ASYNC(static address, exception_handler_for_pc_helper(JavaThread* thread, oopDesc* ex, address pc, nmethod*& nm))
  // Reset method handle flag.
  thread->set_is_method_handle_return(false);

  Handle exception(thread, ex);
  nm = CodeCache::find_nmethod(pc);
  assert(nm != NULL, "this is not a compiled method");
  // Adjust the pc as needed/
  if (nm->is_deopt_pc(pc)) {
    RegisterMap map(thread, false);
    frame exception_frame = thread->last_frame().sender(&map);
    // if the frame isn't deopted then pc must not correspond to the caller of last_frame
    assert(exception_frame.is_deoptimized_frame(), "must be deopted");
    pc = exception_frame.pc();
  }
#ifdef ASSERT
  assert(exception.not_null(), "NULL exceptions should be handled by throw_exception");
  assert(exception->is_oop(), "just checking");
  // Check that exception is a subclass of Throwable, otherwise we have a VerifyError
  if (!(exception->is_a(SystemDictionary::Throwable_klass()))) {
    if (ExitVMOnVerifyError) vm_exit(-1);
    ShouldNotReachHere();
  }
#endif

  // Check the stack guard pages and reenable them if necessary and there is
  // enough space on the stack to do so.  Use fast exceptions only if the guard
  // pages are enabled.
  bool guard_pages_enabled = thread->stack_yellow_zone_enabled();
  if (!guard_pages_enabled) guard_pages_enabled = thread->reguard_stack();

  if (JvmtiExport::can_post_on_exceptions()) {
    // To ensure correct notification of exception catches and throws
    // we have to deoptimize here.  If we attempted to notify the
    // catches and throws during this exception lookup it's possible
    // we could deoptimize on the way out of the VM and end back in
    // the interpreter at the throw site.  This would result in double
    // notifications since the interpreter would also notify about
    // these same catches and throws as it unwound the frame.

    RegisterMap reg_map(thread);
    frame stub_frame = thread->last_frame();
    frame caller_frame = stub_frame.sender(&reg_map);

    // We don't really want to deoptimize the nmethod itself since we
    // can actually continue in the exception handler ourselves but I
    // don't see an easy way to have the desired effect.
    Deoptimization::deoptimize_frame(thread, caller_frame.id(), Deoptimization::Reason_constraint);
    assert(caller_is_deopted(), "Must be deoptimized");

    return SharedRuntime::deopt_blob()->unpack_with_exception_in_tls();
  }

  // ExceptionCache is used only for exceptions at call sites and not for implicit exceptions
  if (guard_pages_enabled) {
    address fast_continuation = nm->handler_for_exception_and_pc(exception, pc);
    if (fast_continuation != NULL) {
      // Set flag if return address is a method handle call site.
      thread->set_is_method_handle_return(nm->is_method_handle_return(pc));
      return fast_continuation;
    }
  }

  // If the stack guard pages are enabled, check whether there is a handler in
  // the current method.  Otherwise (guard pages disabled), force an unwind and
  // skip the exception cache update (i.e., just leave continuation==NULL).
  address continuation = NULL;
  if (guard_pages_enabled) {

    // New exception handling mechanism can support inlined methods
    // with exception handlers since the mappings are from PC to PC

    // debugging support
    // tracing
    if (TraceExceptions) {
      ttyLocker ttyl;
      ResourceMark rm;
      tty->print_cr("Exception <%s> (" INTPTR_FORMAT ") thrown in compiled method <%s> at PC " INTPTR_FORMAT " for thread " INTPTR_FORMAT "",
                    exception->print_value_string(), p2i((address)exception()), nm->method()->print_value_string(), p2i(pc), p2i(thread));
    }
    // for AbortVMOnException flag
    NOT_PRODUCT(Exceptions::debug_check_abort(exception));

    // Clear out the exception oop and pc since looking up an
    // exception handler can cause class loading, which might throw an
    // exception and those fields are expected to be clear during
    // normal bytecode execution.
    thread->clear_exception_oop_and_pc();

    bool recursive_exception = false;
    continuation = SharedRuntime::compute_compiled_exc_handler(nm, pc, exception, false, false, recursive_exception);
    // If an exception was thrown during exception dispatch, the exception oop may have changed
    thread->set_exception_oop(exception());
    thread->set_exception_pc(pc);

    // The exception cache is used only for non-implicit exceptions
    // Update the exception cache only when another exception did
    // occur during the computation of the compiled exception handler
    // (e.g., when loading the class of the catch type).
    // Checking for exception oop equality is not
    // sufficient because some exceptions are pre-allocated and reused.
    if (continuation != NULL && !recursive_exception && !SharedRuntime::deopt_blob()->contains(continuation)) {
      nm->add_handler_for_exception_and_pc(exception, pc, continuation);
    }
  }

  // Set flag if return address is a method handle call site.
  thread->set_is_method_handle_return(nm->is_method_handle_return(pc));

  if (TraceExceptions) {
    ttyLocker ttyl;
    ResourceMark rm;
    tty->print_cr("Thread " PTR_FORMAT " continuing at PC " PTR_FORMAT " for exception thrown at PC " PTR_FORMAT,
                  p2i(thread), p2i(continuation), p2i(pc));
  }

  return continuation;
JRT_END

// Enter this method from compiled code only if there is a Java exception handler
// in the method handling the exception.
// We are entering here from exception stub. We don't do a normal VM transition here.
// We do it in a helper. This is so we can check to see if the nmethod we have just
// searched for an exception handler has been deoptimized in the meantime.
address JVMCIRuntime::exception_handler_for_pc(JavaThread* thread) {
  oop exception = thread->exception_oop();
  address pc = thread->exception_pc();
  // Still in Java mode
  DEBUG_ONLY(ResetNoHandleMark rnhm);
  nmethod* nm = NULL;
  address continuation = NULL;
  {
    // Enter VM mode by calling the helper
    ResetNoHandleMark rnhm;
    continuation = exception_handler_for_pc_helper(thread, exception, pc, nm);
  }
  // Back in JAVA, use no oops DON'T safepoint

  // Now check to see if the compiled method we were called from is now deoptimized.
  // If so we must return to the deopt blob and deoptimize the nmethod
  if (nm != NULL && caller_is_deopted()) {
    continuation = SharedRuntime::deopt_blob()->unpack_with_exception_in_tls();
  }

  assert(continuation != NULL, "no handler found");
  return continuation;
}

JRT_ENTRY_NO_ASYNC(void, JVMCIRuntime::monitorenter(JavaThread* thread, oopDesc* obj, BasicLock* lock))
  IF_TRACE_jvmci_3 {
    char type[O_BUFLEN];
    obj->klass()->name()->as_C_string(type, O_BUFLEN);
    markOop mark = obj->mark();
    TRACE_jvmci_3("%s: entered locking slow case with obj=" INTPTR_FORMAT ", type=%s, mark=" INTPTR_FORMAT ", lock=" INTPTR_FORMAT, thread->name(), p2i(obj), type, p2i(mark), p2i(lock));
    tty->flush();
  }
  if (PrintBiasedLockingStatistics) {
    Atomic::inc(BiasedLocking::slow_path_entry_count_addr());
  }
  Handle h_obj(thread, obj);
  assert(h_obj()->is_oop(), "must be NULL or an object");
  if (UseBiasedLocking) {
    // Retry fast entry if bias is revoked to avoid unnecessary inflation
    ObjectSynchronizer::fast_enter(h_obj, lock, true, CHECK);
  } else {
    if (JVMCIUseFastLocking) {
      // When using fast locking, the compiled code has already tried the fast case
      ObjectSynchronizer::slow_enter(h_obj, lock, THREAD);
    } else {
      ObjectSynchronizer::fast_enter(h_obj, lock, false, THREAD);
    }
  }
  TRACE_jvmci_3("%s: exiting locking slow with obj=" INTPTR_FORMAT, thread->name(), p2i(obj));
JRT_END

JRT_LEAF(void, JVMCIRuntime::monitorexit(JavaThread* thread, oopDesc* obj, BasicLock* lock))
  assert(thread == JavaThread::current(), "threads must correspond");
  assert(thread->last_Java_sp(), "last_Java_sp must be set");
  // monitorexit is non-blocking (leaf routine) => no exceptions can be thrown
  EXCEPTION_MARK;

#ifdef ASSERT
  if (!obj->is_oop()) {
    ResetNoHandleMark rhm;
    nmethod* method = thread->last_frame().cb()->as_nmethod_or_null();
    if (method != NULL) {
      tty->print_cr("ERROR in monitorexit in method %s wrong obj " INTPTR_FORMAT, method->name(), p2i(obj));
    }
    thread->print_stack_on(tty);
    assert(false, "invalid lock object pointer dected");
  }
#endif

  if (JVMCIUseFastLocking) {
    // When using fast locking, the compiled code has already tried the fast case
    ObjectSynchronizer::slow_exit(obj, lock, THREAD);
  } else {
    ObjectSynchronizer::fast_exit(obj, lock, THREAD);
  }
  IF_TRACE_jvmci_3 {
    char type[O_BUFLEN];
    obj->klass()->name()->as_C_string(type, O_BUFLEN);
    TRACE_jvmci_3("%s: exited locking slow case with obj=" INTPTR_FORMAT ", type=%s, mark=" INTPTR_FORMAT ", lock=" INTPTR_FORMAT, thread->name(), p2i(obj), type, p2i(obj->mark()), p2i(lock));
    tty->flush();
  }
JRT_END

JRT_ENTRY(void, JVMCIRuntime::throw_and_post_jvmti_exception(JavaThread* thread, const char* exception, const char* message))
  TempNewSymbol symbol = SymbolTable::new_symbol(exception, CHECK);
  SharedRuntime::throw_and_post_jvmti_exception(thread, symbol, message);
JRT_END

JRT_ENTRY(void, JVMCIRuntime::throw_klass_external_name_exception(JavaThread* thread, const char* exception, Klass* klass))
  ResourceMark rm(thread);
  TempNewSymbol symbol = SymbolTable::new_symbol(exception, CHECK);
  SharedRuntime::throw_and_post_jvmti_exception(thread, symbol, klass->external_name());
JRT_END

JRT_ENTRY(void, JVMCIRuntime::throw_class_cast_exception(JavaThread* thread, const char* exception, Klass* caster_klass, Klass* target_klass))
  ResourceMark rm(thread);
  const char* message = SharedRuntime::generate_class_cast_message(caster_klass->external_name(), target_klass->external_name());
  TempNewSymbol symbol = SymbolTable::new_symbol(exception, CHECK);
  SharedRuntime::throw_and_post_jvmti_exception(thread, symbol, message);
JRT_END

JRT_LEAF(void, JVMCIRuntime::log_object(JavaThread* thread, oopDesc* obj, bool as_string, bool newline))
  ttyLocker ttyl;

  if (obj == NULL) {
    tty->print("NULL");
  } else if (obj->is_oop_or_null(true) && (!as_string || !java_lang_String::is_instance(obj))) {
    if (obj->is_oop_or_null(true)) {
      char buf[O_BUFLEN];
      tty->print("%s@" INTPTR_FORMAT, obj->klass()->name()->as_C_string(buf, O_BUFLEN), p2i(obj));
    } else {
      tty->print(INTPTR_FORMAT, p2i(obj));
    }
  } else {
    ResourceMark rm;
    assert(obj != NULL && java_lang_String::is_instance(obj), "must be");
    char *buf = java_lang_String::as_utf8_string(obj);
    tty->print_raw(buf);
  }
  if (newline) {
    tty->cr();
  }
JRT_END

JRT_LEAF(void, JVMCIRuntime::write_barrier_pre(JavaThread* thread, oopDesc* obj))
  thread->satb_mark_queue().enqueue(obj);
JRT_END

JRT_LEAF(void, JVMCIRuntime::write_barrier_post(JavaThread* thread, void* card_addr))
  thread->dirty_card_queue().enqueue(card_addr);
JRT_END

JRT_LEAF(jboolean, JVMCIRuntime::validate_object(JavaThread* thread, oopDesc* parent, oopDesc* child))
  bool ret = true;
  if(!Universe::heap()->is_in_closed_subset(parent)) {
    tty->print_cr("Parent Object " INTPTR_FORMAT " not in heap", p2i(parent));
    parent->print();
    ret=false;
  }
  if(!Universe::heap()->is_in_closed_subset(child)) {
    tty->print_cr("Child Object " INTPTR_FORMAT " not in heap", p2i(child));
    child->print();
    ret=false;
  }
  return (jint)ret;
JRT_END

JRT_ENTRY(void, JVMCIRuntime::vm_error(JavaThread* thread, jlong where, jlong format, jlong value))
  ResourceMark rm;
  const char *error_msg = where == 0L ? "<internal JVMCI error>" : (char*) (address) where;
  char *detail_msg = NULL;
  if (format != 0L) {
    const char* buf = (char*) (address) format;
    size_t detail_msg_length = strlen(buf) * 2;
    detail_msg = (char *) NEW_RESOURCE_ARRAY(u_char, detail_msg_length);
    jio_snprintf(detail_msg, detail_msg_length, buf, value);
  }
  report_vm_error(__FILE__, __LINE__, error_msg, detail_msg);
JRT_END

JRT_LEAF(oopDesc*, JVMCIRuntime::load_and_clear_exception(JavaThread* thread))
  oop exception = thread->exception_oop();
  assert(exception != NULL, "npe");
  thread->set_exception_oop(NULL);
  thread->set_exception_pc(0);
  return exception;
JRT_END

PRAGMA_DIAG_PUSH
PRAGMA_FORMAT_NONLITERAL_IGNORED
JRT_LEAF(void, JVMCIRuntime::log_printf(JavaThread* thread, const char* format, jlong v1, jlong v2, jlong v3))
  ResourceMark rm;
  tty->print(format, v1, v2, v3);
JRT_END
PRAGMA_DIAG_POP

static void decipher(jlong v, bool ignoreZero) {
  if (v != 0 || !ignoreZero) {
    void* p = (void *)(address) v;
    CodeBlob* cb = CodeCache::find_blob(p);
    if (cb) {
      if (cb->is_nmethod()) {
        char buf[O_BUFLEN];
        tty->print("%s [" INTPTR_FORMAT "+" JLONG_FORMAT "]", cb->as_nmethod_or_null()->method()->name_and_sig_as_C_string(buf, O_BUFLEN), p2i(cb->code_begin()), (jlong)((address)v - cb->code_begin()));
        return;
      }
      cb->print_value_on(tty);
      return;
    }
    if (Universe::heap()->is_in(p)) {
      oop obj = oop(p);
      obj->print_value_on(tty);
      return;
    }
    tty->print(INTPTR_FORMAT " [long: " JLONG_FORMAT ", double %lf, char %c]",p2i((void *)v), (jlong)v, (jdouble)v, (char)v);
  }
}

PRAGMA_DIAG_PUSH
PRAGMA_FORMAT_NONLITERAL_IGNORED
JRT_LEAF(void, JVMCIRuntime::vm_message(jboolean vmError, jlong format, jlong v1, jlong v2, jlong v3))
  ResourceMark rm;
  const char *buf = (const char*) (address) format;
  if (vmError) {
    if (buf != NULL) {
      fatal(err_msg(buf, v1, v2, v3));
    } else {
      fatal("<anonymous error>");
    }
  } else if (buf != NULL) {
    tty->print(buf, v1, v2, v3);
  } else {
    assert(v2 == 0, "v2 != 0");
    assert(v3 == 0, "v3 != 0");
    decipher(v1, false);
  }
JRT_END
PRAGMA_DIAG_POP

JRT_LEAF(void, JVMCIRuntime::log_primitive(JavaThread* thread, jchar typeChar, jlong value, jboolean newline))
  union {
      jlong l;
      jdouble d;
      jfloat f;
  } uu;
  uu.l = value;
  switch (typeChar) {
    case 'Z': tty->print(value == 0 ? "false" : "true"); break;
    case 'B': tty->print("%d", (jbyte) value); break;
    case 'C': tty->print("%c", (jchar) value); break;
    case 'S': tty->print("%d", (jshort) value); break;
    case 'I': tty->print("%d", (jint) value); break;
    case 'F': tty->print("%f", uu.f); break;
    case 'J': tty->print(JLONG_FORMAT, value); break;
    case 'D': tty->print("%lf", uu.d); break;
    default: assert(false, "unknown typeChar"); break;
  }
  if (newline) {
    tty->cr();
  }
JRT_END

JRT_ENTRY(jint, JVMCIRuntime::identity_hash_code(JavaThread* thread, oopDesc* obj))
  return (jint) obj->identity_hash();
JRT_END

JRT_ENTRY(jboolean, JVMCIRuntime::thread_is_interrupted(JavaThread* thread, oopDesc* receiver, jboolean clear_interrupted))
  // Ensure that the C++ Thread and OSThread structures aren't freed before we operate.
  // This locking requires thread_in_vm which is why this method cannot be JRT_LEAF.
  Handle receiverHandle(thread, receiver);
  MutexLockerEx ml(thread->threadObj() == (void*)receiver ? NULL : Threads_lock);
  JavaThread* receiverThread = java_lang_Thread::thread(receiverHandle());
  if (receiverThread == NULL) {
    // The other thread may exit during this process, which is ok so return false.
    return JNI_FALSE;
  } else {
    return (jint) Thread::is_interrupted(receiverThread, clear_interrupted != 0);
  }
JRT_END

JRT_ENTRY(jint, JVMCIRuntime::test_deoptimize_call_int(JavaThread* thread, int value))
  deopt_caller();
  return value;
JRT_END

// These entry points can be called from Java code executing in either the JVMCI shared library
// JavaVM or on the HotSpot heap.  In the shared library case the JNIEnv is associated with a
// non-HotSpot runtime so use JVM_ENTRY_NO_ENV instead of the standard JVM_ENTRY.

// private static void JVMCIClassLoaderFactory.init(ClassLoader loader)
JVM_ENTRY_NO_ENV(void, JVM_InitJVMCIClassLoader(JNIEnv *env, jclass c, jobject loader_handle))
  SystemDictionary::init_jvmci_loader(JNIHandles::resolve(loader_handle));
JVM_END

// private static JVMCIRuntime JVMCI.initializeRuntime()
JVM_ENTRY_NO_ENV(jobject, JVM_GetJVMCIRuntime(JNIEnv *env, jclass c))
  JNI_JVMCIENV(env);
  if (!EnableJVMCI) {
    JVMCIENV->throw_InternalError("JVMCI is not enabled");
  }
  JVMCIENV->runtime()->initialize_HotSpotJVMCIRuntime(JVMCI_CHECK_NULL);
  JVMCIObject runtime = JVMCIENV->runtime()->get_HotSpotJVMCIRuntime(JVMCI_CHECK_NULL);
  return JVMCIENV->get_jobject(runtime);
JVM_END

// private static ClassLoader Services.getJVMCIClassLoader()
JVM_ENTRY_NO_ENV(jobject, JVM_GetJVMCIClassLoader(JNIEnv *env, jclass c))
  JNI_JVMCIENV(env);
  if (!EnableJVMCI) {
    // This message must not change - it is used by the Java code to
    // distinguish an InternalError due to -EnableJVMCI from other
    // InternalErrors that may be raised below.
    JVMCIENV->throw_InternalError("JVMCI is not enabled");
    return NULL;
  }
  JVMCIENV->runtime()->ensure_jvmci_class_loader_is_initialized(JVMCIENV);
  return JNIHandles::make_local(THREAD, SystemDictionary::jvmci_loader());
JVM_END

void JVMCIRuntime::call_getCompiler(TRAPS) {
  THREAD_JVMCIENV(JavaThread::current());
  JVMCIObject jvmciRuntime = JVMCIRuntime::get_HotSpotJVMCIRuntime(JVMCI_CHECK);
  ensure_jvmci_class_loader_is_initialized(JVMCIENV);
  JVMCIENV->call_HotSpotJVMCIRuntime_getCompiler(jvmciRuntime, JVMCI_CHECK);
}

JVMCINMethodData* volatile JVMCINMethodData::_for_release = NULL;

JVMCINMethodData::JVMCINMethodData(JVMCIEnv* jvmciEnv, JVMCIObject nmethod_mirror, JVMCIObject speculation_log, bool triggers_invalidation) {
  _next = NULL;
  _triggers_invalidation = triggers_invalidation;
  _speculation_log = jvmciEnv->make_weak(speculation_log);
  if (jvmciEnv->is_hotspot()) {
    _nmethod_mirror = jvmciEnv->make_weak(nmethod_mirror);
  } else {
    _nmethod_mirror = JVMCIObject();
  }
  _nmethod_mirror_name = NULL;

  if (jvmciEnv->isa_InstalledCode(nmethod_mirror)) {
    JVMCIObject nmethod_mirror_name = jvmciEnv->get_InstalledCode_name(nmethod_mirror);
    if (!nmethod_mirror_name.is_null()) {
      const char* name = jvmciEnv->as_utf8_string(nmethod_mirror_name);
      char* name_copy = NEW_C_HEAP_ARRAY(char, strlen(name) + 1, mtCompiler);
      strcpy(name_copy, name);
      _nmethod_mirror_name = name_copy;
    }
  }
}

JVMCINMethodData::~JVMCINMethodData() {
  clear_nmethod_mirror();
  guarantee(_nmethod_mirror.is_null(), "must be clear now");
  clear_speculation_log(true);
  guarantee(_speculation_log.is_null(), "must be clear now");
  if (_nmethod_mirror_name != NULL) {
    FREE_C_HEAP_ARRAY(char, _nmethod_mirror_name, mtCompiler);
    _nmethod_mirror_name = NULL;
  }
}

void JVMCINMethodData::release(JVMCINMethodData* data) {
  if (data->_speculation_log.is_null() || data->_speculation_log.is_hotspot()) {
    delete data;
  } else {
    // Queue the data for release.
    MutexLocker locker(JVMCI_lock);
    data->_next = _for_release;
    _for_release = data;
  }
}

void JVMCINMethodData::cleanup() {
  if (_for_release == NULL) {
    return;
  }
  JVMCINMethodData* current = NULL;
  {
    MutexLocker locker(JVMCI_lock);
    current = _for_release;
    _for_release = NULL;
  }
  while (current != NULL) {
    JVMCINMethodData* next = current->_next;
    delete current;
    current = next;
  }
}

JVMCIObject JVMCINMethodData::get_nmethod_mirror() {
  return _nmethod_mirror;
}

void JVMCINMethodData::add_nmethod_mirror(JVMCIEnv* jvmciEnv, JVMCIObject nmethod_mirror, JVMCI_TRAPS) {
  // Only HotSpotNmethod instances are tracking directly by the runtime.  HotSpotNMethodHandle
  // instances are updated cooperatively.
  if (!jvmciEnv->is_hotspot()) {
    return;
  }

  if (_nmethod_mirror.is_non_null()) {
    JVMCI_THROW_MSG(IllegalArgumentException, "Cannot overwrite existing HotSpotNmethod object for nmethod");
  }
  _nmethod_mirror = jvmciEnv->make_weak(nmethod_mirror);
}

void JVMCINMethodData::update_nmethod_mirror_in_gc(nmethod* nm, BoolObjectClosure* is_alive) {
  if (_nmethod_mirror.is_null()) {
    return;
  }
  oop mirror_obj = HotSpotJVMCI::resolve(_nmethod_mirror);
  if (mirror_obj == NULL || !is_alive->do_object_b(mirror_obj)) {
    clear_nmethod_mirror();
  }
  if (_triggers_invalidation) {
    if (_nmethod_mirror.is_null()) {
      // The references to the mirror have been dropped so invalidate
      // the nmethod and allow the sweeper to reclaim it.
      nm->make_not_entrant();
    }
  }
}

void JVMCINMethodData::invalidate_mirror(nmethod* nm) {
  if (_nmethod_mirror.is_null()) {
    return;
  }
  assert(_nmethod_mirror.is_hotspot(), "only HotSpot reference is supported");
  JVMCIEnv jvmciEnv(_nmethod_mirror, __FILE__, __LINE__);
  if (!_nmethod_mirror.is_null()) {
    // Check weak reference for null
    if (jvmciEnv.equals(_nmethod_mirror, JVMCIObject())) {
      // The referent is null so delete weak reference
      jvmciEnv.destroy_weak(_nmethod_mirror);
      _nmethod_mirror = JVMCIObject();
      return;
    }

    // Update the values in the HotSpotNmethod object if it still refers to this nmethod
    nmethod* current = (nmethod*) jvmciEnv.get_InstalledCode_address(_nmethod_mirror);
    if (nm == current) {
      if (!nm->is_alive()) {
        // Break the link from HotSpotNmethod to nmethod such that
        // future invocations via the HotSpotNmethod will result in
        // an InvalidInstalledCodeException.
        jvmciEnv.set_InstalledCode_address(_nmethod_mirror, 0);
        jvmciEnv.set_InstalledCode_entryPoint(_nmethod_mirror, 0);
      } else if (nm->is_not_entrant()) {
        // Zero the entry point so any new invocation will fail but keep
        // the address link around that so that existing activations can
        // be invalidated (i.e. JVMCIEnv::invalidate_installed_code).
        jvmciEnv.set_InstalledCode_entryPoint(_nmethod_mirror, 0);
      }
    }
  }
  if (!nm->is_alive()) {
    // Clear these out after the nmethod is dead and all
    // relevant fields in the HotSpotNmethod have been zeroed.
    clear_nmethod_mirror();
  }
  if (!nm->is_alive()) {
    clear_speculation_log();
  }
}

void JVMCINMethodData::update_speculation(JavaThread* thread, nmethod* nm) {
  long speculation = thread->pending_failed_speculation();
  if (speculation != 0) {
    if (!_speculation_log.is_null()) {
      JVMCIEnv jvmciEnv(_speculation_log, __FILE__, __LINE__);
      if (jvmciEnv.equals(_speculation_log, JVMCIObject())) {
        // The weak reference has been cleared
        jvmciEnv.destroy_weak(_speculation_log);
        _speculation_log = JVMCIObject();
        return;
      }
      if (TraceDeoptimization || TraceUncollectedSpeculations) {
        if (!jvmciEnv.get_HotSpotSpeculationLog_lastFailed(_speculation_log) != 0) {
          tty->print_cr("A speculation that was not collected by the compiler is being overwritten");
        }
      }
      if (TraceDeoptimization) {
        tty->print_cr("Saving speculation to speculation log");
      }
      jvmciEnv.set_HotSpotSpeculationLog_lastFailed(_speculation_log, speculation);
    } else {
      if (TraceDeoptimization) {
        tty->print_cr("Speculation present but no speculation log");
      }
    }
    thread->set_pending_failed_speculation(0);
  }
}

void JVMCINMethodData::clear_nmethod_mirror() {
  if (!_nmethod_mirror.is_null()) {
    JVMCIEnv jvmciEnv(_nmethod_mirror, __FILE__, __LINE__);
    jvmciEnv.destroy_weak(_nmethod_mirror);
    _nmethod_mirror = JVMCIObject();
  }
}

void JVMCINMethodData::clear_speculation_log(bool force) {
  if (!_speculation_log.is_null()) {
    // Non HotSpot speculations have to be cleaned up more carefully so
    // they shouldn't be done by default.
    if (!force && !_speculation_log.is_hotspot()) {
      return;
    }
    JVMCIEnv jvmciEnv(_speculation_log, __FILE__, __LINE__);
    jvmciEnv.destroy_weak(_speculation_log);
    _speculation_log = JVMCIObject();
  }
}

void JVMCIRuntime::initialize_HotSpotJVMCIRuntime(JVMCI_TRAPS) {
  if (!_HotSpotJVMCIRuntime_instance.is_null()) {
    if (JVMCIENV->is_hotspot() && JVMCIGlobals::java_mode() == JVMCIGlobals::SharedLibrary) {
      JVMCI_THROW_MSG(InternalError, "JVMCI has already been enabled in the JVMCI shared library");
    }
  }

  ensure_jvmci_class_loader_is_initialized(JVMCIENV);

  // This should only be called in the context of the JVMCI class being initialized
  JVMCIObject result = JVMCIENV->call_HotSpotJVMCIRuntime_runtime(JVMCI_CHECK);
  int adjustment = JVMCIENV->get_HotSpotJVMCIRuntime_compilationLevelAdjustment(result);
  assert(adjustment >= JVMCIRuntime::none &&
         adjustment <= JVMCIRuntime::by_full_signature,
         "compilation level adjustment out of bounds");
  _comp_level_adjustment = (CompLevelAdjustment) adjustment;

  _HotSpotJVMCIRuntime_instance = JVMCIENV->make_global(result);
}


void JVMCI::initialize_compiler(TRAPS) {
  if (JVMCILibDumpJNIConfig) {
    JNIJVMCI::initialize_ids(NULL);
    ShouldNotReachHere();
  }

  JVMCI::compiler_runtime()->call_getCompiler(CHECK);
}

void JVMCI::initialize_globals() {
  _object_handles = JNIHandleBlock::allocate_block();
  _metadata_handles = MetadataHandleBlock::allocate_block();
  if (JVMCIGlobals::java_mode() == JVMCIGlobals::SharedLibrary) {
    // There are two runtimes.
    _compiler_runtime = new JVMCIRuntime();
    _java_runtime = new JVMCIRuntime();
  } else {
    // There is only a single runtime
    _java_runtime = _compiler_runtime = new JVMCIRuntime();
  }
}


void JVMCIRuntime::initialize(JVMCIEnv* JVMCIENV) {
  // Check first without JVMCI_lock
  if (_initialized) {
    return;
  }

  MutexLocker locker(JVMCI_lock);
  // Check again under JVMCI_lock
  if (_initialized) {
    return;
  }

  while (_being_initialized) {
    JVMCI_lock->wait();
    if (_initialized) {
      return;
    }
  }

  _being_initialized = true;

  {
    MutexUnlocker unlock(JVMCI_lock);

    HandleMark hm;
    ResourceMark rm;
    JavaThread* THREAD = JavaThread::current();
    if (JVMCIENV->mode() == JVMCIGlobals::HotSpot) {
      HotSpotJVMCI::compute_offsets(CHECK_EXIT);
    } else {
      JNIAccessMark jni(JVMCIENV);

      JNIJVMCI::initialize_ids(jni.env());
      if (jni()->ExceptionCheck()) {
        jni()->ExceptionDescribe();
        fatal("JNI exception during init");
      }
    }
    create_jvmci_primitive_type(T_BOOLEAN, JVMCI_CHECK_EXIT_((void)0));
    create_jvmci_primitive_type(T_BYTE, JVMCI_CHECK_EXIT_((void)0));
    create_jvmci_primitive_type(T_CHAR, JVMCI_CHECK_EXIT_((void)0));
    create_jvmci_primitive_type(T_SHORT, JVMCI_CHECK_EXIT_((void)0));
    create_jvmci_primitive_type(T_INT, JVMCI_CHECK_EXIT_((void)0));
    create_jvmci_primitive_type(T_LONG, JVMCI_CHECK_EXIT_((void)0));
    create_jvmci_primitive_type(T_FLOAT, JVMCI_CHECK_EXIT_((void)0));
    create_jvmci_primitive_type(T_DOUBLE, JVMCI_CHECK_EXIT_((void)0));
    create_jvmci_primitive_type(T_VOID, JVMCI_CHECK_EXIT_((void)0));
  }
  _initialized = true;
  _being_initialized = false;
  JVMCI_lock->notify_all();
}

JVMCIObject JVMCIRuntime::create_jvmci_primitive_type(BasicType type, JVMCI_TRAPS) {
  Thread* THREAD = Thread::current();
  // These primitive types are long lived and are created before the runtime is fully set up
  // so skip registering them for scanning.
  JVMCIObject mirror = JVMCIENV->get_object_constant(java_lang_Class::primitive_mirror(type), false, true);
  if (JVMCIENV->is_hotspot()) {
    JavaValue result(T_OBJECT);
    JavaCallArguments args;
    args.push_oop(HotSpotJVMCI::resolve(mirror));
    args.push_int(type2char(type));
    JavaCalls::call_static(&result, HotSpotJVMCI::HotSpotResolvedPrimitiveType::klass(), vmSymbols::fromMetaspace_name(), vmSymbols::primitive_fromMetaspace_signature(), &args, CHECK_(JVMCIObject()));

    return JVMCIENV->wrap(JNIHandles::make_local((oop)result.get_jobject()));
  } else {
    JNIAccessMark jni(JVMCIENV);
    jobject result = jni()->CallStaticObjectMethod(JNIJVMCI::HotSpotResolvedPrimitiveType::clazz(),
                                           JNIJVMCI::HotSpotResolvedPrimitiveType_fromMetaspace_method(),
                                           mirror.as_jobject(), type2char(type));
    if (jni()->ExceptionCheck()) {
      return JVMCIObject();
    }
    return JVMCIENV->wrap(result);
  }
}

void JVMCIRuntime::initialize_JVMCI(JVMCI_TRAPS) {
  if (_HotSpotJVMCIRuntime_instance.is_null()) {
    initialize(JVMCI_CHECK);
    JVMCIENV->call_JVMCI_getRuntime(JVMCI_CHECK);
  }
  assert(_HotSpotJVMCIRuntime_instance.is_non_null(), "what?");
}

JVMCIObject JVMCIRuntime::get_HotSpotJVMCIRuntime(JVMCI_TRAPS) {
  ensure_jvmci_class_loader_is_initialized(JVMCIENV);
  initialize_JVMCI(JVMCI_CHECK_(JVMCIObject()));
  return _HotSpotJVMCIRuntime_instance;
}

jobject JVMCI::make_global(Handle obj) {
  assert(_object_handles != NULL, "uninitialized");
  MutexLocker ml(JVMCI_lock);
  return _object_handles->allocate_handle(obj());
}

bool JVMCI::is_global_handle(jobject handle) {
  return _object_handles->chain_contains(handle);
}

jmetadata JVMCI::allocate_handle(const methodHandle& handle) {
  assert(_metadata_handles != NULL, "uninitialized");
  MutexLocker ml(JVMCI_lock);
  return _metadata_handles->allocate_handle(handle);
}

jmetadata JVMCI::allocate_handle(const constantPoolHandle& handle) {
  assert(_metadata_handles != NULL, "uninitialized");
  MutexLocker ml(JVMCI_lock);
  return _metadata_handles->allocate_handle(handle);
}

void JVMCI::release_handle(jmetadata handle) {
  MutexLocker ml(JVMCI_lock);
  _metadata_handles->chain_free_list(handle);
}

void JVMCI::oops_do(OopClosure* f) {
  if (_object_handles != NULL) {
    _object_handles->oops_do(f);
  }
}

void JVMCI::metadata_do(void f(Metadata*)) {
  if (_metadata_handles != NULL) {
    _metadata_handles->metadata_do(f);
  }
}

void JVMCI::do_unloading(BoolObjectClosure* is_alive, bool unloading_occurred) {
  if (_metadata_handles != NULL && unloading_occurred) {
    _metadata_handles->do_unloading(is_alive);
  }
}

CompLevel JVMCI::adjust_comp_level(methodHandle method, bool is_osr, CompLevel level, JavaThread* thread) {
  return compiler_runtime()->adjust_comp_level(method, is_osr, level, thread);
}


// private static void CompilerToVM.registerNatives()
JVM_ENTRY_NO_ENV(void, JVM_RegisterJVMCINatives(JNIEnv *env, jclass c2vmClass))

#ifdef _LP64
#ifndef TARGET_ARCH_sparc
  uintptr_t heap_end = (uintptr_t) Universe::heap()->reserved_region().end();
  uintptr_t allocation_end = heap_end + ((uintptr_t)16) * 1024 * 1024 * 1024;
  guarantee(heap_end < allocation_end, "heap end too close to end of address space (might lead to erroneous TLAB allocations)");
#endif // TARGET_ARCH_sparc
#else
  fatal("check TLAB allocation code for address space conflicts");
#endif

  JNI_JVMCIENV(env);

  if (!EnableJVMCI) {
    JVMCIENV->throw_InternalError("JVMCI is not enabled");
  }

  JVMCIENV->runtime()->ensure_jvmci_class_loader_is_initialized(JVMCIENV);

  {
    ResourceMark rm;
    HandleMark hm(thread);
    ThreadToNativeFromVM trans(thread);

    // Ensure _non_oop_bits is initialized
    Universe::non_oop_word();

    if (JNI_OK != env->RegisterNatives(c2vmClass, CompilerToVM::methods, CompilerToVM::methods_count())) {
      if (!env->ExceptionCheck()) {
        for (int i = 0; i < CompilerToVM::methods_count(); i++) {
          if (JNI_OK != env->RegisterNatives(c2vmClass, CompilerToVM::methods + i, 1)) {
            guarantee(false, err_msg("Error registering JNI method %s%s", CompilerToVM::methods[i].name, CompilerToVM::methods[i].signature));
            break;
          }
        }
      } else {
        env->ExceptionDescribe();
      }
      guarantee(false, "Failed registering CompilerToVM native methods");
    }
  }
JVM_END

void JVMCIRuntime::ensure_jvmci_class_loader_is_initialized(JVMCIEnv* JVMCIENV) {
  if (UseJVMCIClassLoader) {
    // This initialization code is guarded by a static pointer to the Factory class.
    // Once it is non-null, the JVMCI class loader is guaranteed to have been
    // initialized. By going through the static initializer of Factory, we can rely
    // on class initialization semantics to synchronize racing threads.
    static Klass* _FactoryKlass = NULL;
    if (_FactoryKlass == NULL) {
      JavaThread* THREAD = JavaThread::current();
      TempNewSymbol name = SymbolTable::new_symbol("jdk/vm/ci/services/JVMCIClassLoaderFactory", CHECK_EXIT);
      Klass* klass = SystemDictionary::resolve_or_fail(name, true, CHECK_EXIT);
      klass->initialize(CHECK_EXIT);
      _FactoryKlass = klass;
      assert(SystemDictionary::jvmci_loader() != NULL, "JVMCI classloader should have been initialized");
    }
  }
  initialize(JVMCIENV);
}


bool JVMCI::is_compiler_initialized() {
  return compiler_runtime()->is_HotSpotJVMCIRuntime_initialized();
}


void JVMCI::shutdown() {
  if (compiler_runtime() != NULL) {
    compiler_runtime()->shutdown();
  }
}


bool JVMCI::shutdown_called() {
  if (compiler_runtime() != NULL) {
    return compiler_runtime()->shutdown_called();
  }
  return false;
}


void JVMCIRuntime::shutdown() {
  if (_HotSpotJVMCIRuntime_instance.is_non_null()) {
    _shutdown_called = true;

    THREAD_JVMCIENV(JavaThread::current());
    JVMCIENV->call_HotSpotJVMCIRuntime_shutdown(_HotSpotJVMCIRuntime_instance);
  }
}

void JVMCIRuntime::bootstrap_finished(TRAPS) {
  if (_HotSpotJVMCIRuntime_instance.is_non_null()) {
    THREAD_JVMCIENV(JavaThread::current());
    JVMCIENV->call_HotSpotJVMCIRuntime_bootstrapFinished(_HotSpotJVMCIRuntime_instance, JVMCIENV);
  }
}
CompLevel JVMCIRuntime::adjust_comp_level(methodHandle method, bool is_osr, CompLevel level, JavaThread* thread) {
  if (!thread->adjusting_comp_level()) {
    thread->set_adjusting_comp_level(true);
    level = adjust_comp_level_inner(method, is_osr, level, thread);
    thread->set_adjusting_comp_level(false);
  }
  return level;
}

CompLevel JVMCIRuntime::adjust_comp_level_inner(methodHandle method, bool is_osr, CompLevel level, JavaThread* thread) {
  JVMCICompiler* compiler = JVMCICompiler::instance(false, thread);
  if (compiler != NULL && compiler->is_bootstrapping()) {
    return level;
  }
  if (!is_HotSpotJVMCIRuntime_initialized() || _comp_level_adjustment == JVMCIRuntime::none) {
    // JVMCI cannot participate in compilation scheduling until
    // JVMCI is initialized and indicates it wants to participate.
    return level;
  }

  JavaThread* THREAD = JavaThread::current();
  ResourceMark rm;
  HandleMark hm;

#define CHECK_RETURN JVMCIENV); \
  if (HAS_PENDING_EXCEPTION) { \
    Handle exception(THREAD, PENDING_EXCEPTION); \
    CLEAR_PENDING_EXCEPTION; \
  \
    if (exception->is_a(SystemDictionary::ThreadDeath_klass())) { \
      /* In the special case of ThreadDeath, we need to reset the */ \
      /* pending async exception so that it is propagated.         */ \
      thread->set_pending_async_exception(exception()); \
      return level; \
    } \
    tty->print("Uncaught exception while adjusting compilation level: "); \
    java_lang_Throwable::print(exception(), tty); \
    tty->cr(); \
    java_lang_Throwable::print_stack_trace(exception(), tty); \
    if (HAS_PENDING_EXCEPTION) { \
      CLEAR_PENDING_EXCEPTION; \
    } \
    return level; \
  } \
  (void)(0


  THREAD_JVMCIENV(thread);
  JVMCIObject receiver = _HotSpotJVMCIRuntime_instance;
  JVMCIObject name;
  JVMCIObject sig;
  if (_comp_level_adjustment == JVMCIRuntime::by_full_signature) {
    name = JVMCIENV->create_string(method->name(), CHECK_RETURN);
    sig = JVMCIENV->create_string(method->signature(), CHECK_RETURN);
  }

  int comp_level = JVMCIENV->call_HotSpotJVMCIRuntime_adjustCompilationLevel(receiver, method->method_holder(), name, sig, is_osr, level, JVMCI_CHECK_EXIT_(level));
  if (comp_level < CompLevel_none || comp_level > CompLevel_full_optimization) {
    assert(false, "compilation level out of bounds");
    return level;
  }
  return (CompLevel) comp_level;
#undef CHECK_RETURN
}

void JVMCIRuntime::describe_pending_hotspot_exception(JavaThread* THREAD, bool clear) {
  if (HAS_PENDING_EXCEPTION) {
    Handle exception(THREAD, PENDING_EXCEPTION);
    const char* exception_file = THREAD->exception_file();
    int exception_line = THREAD->exception_line();
    CLEAR_PENDING_EXCEPTION;
    if (exception->is_a(SystemDictionary::ThreadDeath_klass())) {
      // Don't print anything if we are being killed.
    } else {
      java_lang_Throwable::print(exception(), tty);
      tty->cr();
      java_lang_Throwable::print_stack_trace(exception(), tty);

      // Clear and ignore any exceptions raised during printing
      CLEAR_PENDING_EXCEPTION;
    }
    if (!clear) {
      THREAD->set_pending_exception(exception(), exception_file, exception_line);
    }
  }
}


void JVMCIRuntime::exit_on_pending_exception(JVMCIEnv* JVMCIENV, const char* message) {
  JavaThread* THREAD = JavaThread::current();

  static volatile int report_error = 0;
  if (!report_error && Atomic::cmpxchg(1, &report_error, 0) == 0) {
    // Only report an error once
    tty->print_raw_cr(message);
    if (JVMCIENV != NULL) {
      JVMCIENV->describe_pending_exception(true);
    } else {
      describe_pending_hotspot_exception(THREAD, true);
    }
  } else {
    // Allow error reporting thread to print the stack trace.  Windows
    // doesn't allow uninterruptible wait for JavaThreads
    const bool interruptible = true;
    os::sleep(THREAD, 200, interruptible);
  }

  before_exit(THREAD);
  vm_exit(-1);
}

Klass* JVMCIRuntime::resolve_or_null(Symbol* name, TRAPS) {
  assert(!UseJVMCIClassLoader || SystemDictionary::jvmci_loader() != NULL, "JVMCI classloader should have been initialized");
  return SystemDictionary::resolve_or_null(name, SystemDictionary::jvmci_loader(), Handle(), CHECK_NULL);
}

Klass* JVMCIRuntime::resolve_or_fail(Symbol* name, TRAPS) {
  assert(!UseJVMCIClassLoader || SystemDictionary::jvmci_loader() != NULL, "JVMCI classloader should have been initialized");
  return SystemDictionary::resolve_or_fail(name, SystemDictionary::jvmci_loader(), Handle(), true, CHECK_NULL);
}


// ------------------------------------------------------------------
// Note: the logic of this method should mirror the logic of
// constantPoolOopDesc::verify_constant_pool_resolve.
bool JVMCIRuntime::check_klass_accessibility(Klass* accessing_klass, Klass* resolved_klass) {
  if (accessing_klass->oop_is_objArray()) {
    accessing_klass = ObjArrayKlass::cast(accessing_klass)->bottom_klass();
  }
  if (!accessing_klass->oop_is_instance()) {
    return true;
  }

  if (resolved_klass->oop_is_objArray()) {
    // Find the element klass, if this is an array.
    resolved_klass = ObjArrayKlass::cast(resolved_klass)->bottom_klass();
  }
  if (resolved_klass->oop_is_instance()) {
    return Reflection::verify_class_access(accessing_klass, resolved_klass, true);
  }
  return true;
}

// ------------------------------------------------------------------
Klass* JVMCIRuntime::get_klass_by_name_impl(Klass*& accessing_klass,
                                          const constantPoolHandle& cpool,
                                          Symbol* sym,
                                          bool require_local) {
  JVMCI_EXCEPTION_CONTEXT;

  // Now we need to check the SystemDictionary
  if (sym->byte_at(0) == 'L' &&
    sym->byte_at(sym->utf8_length()-1) == ';') {
    // This is a name from a signature.  Strip off the trimmings.
    // Call recursive to keep scope of strippedsym.
    TempNewSymbol strippedsym = SymbolTable::new_symbol(sym->as_utf8()+1,
                    sym->utf8_length()-2,
                    CHECK_NULL);
    return get_klass_by_name_impl(accessing_klass, cpool, strippedsym, require_local);
  }

  Handle loader(THREAD, (oop)NULL);
  Handle domain(THREAD, (oop)NULL);
  if (accessing_klass != NULL) {
    loader = Handle(THREAD, accessing_klass->class_loader());
    domain = Handle(THREAD, accessing_klass->protection_domain());
  }

  Klass* found_klass;
  {
    ttyUnlocker ttyul;  // release tty lock to avoid ordering problems
    MutexLocker ml(Compile_lock);
    if (!require_local) {
      found_klass = SystemDictionary::find_constrained_instance_or_array_klass(sym, loader, CHECK_NULL);
    } else {
      found_klass = SystemDictionary::find_instance_or_array_klass(sym, loader, domain, CHECK_NULL);
    }
  }

  // If we fail to find an array klass, look again for its element type.
  // The element type may be available either locally or via constraints.
  // In either case, if we can find the element type in the system dictionary,
  // we must build an array type around it.  The CI requires array klasses
  // to be loaded if their element klasses are loaded, except when memory
  // is exhausted.
  if (sym->byte_at(0) == '[' &&
      (sym->byte_at(1) == '[' || sym->byte_at(1) == 'L')) {
    // We have an unloaded array.
    // Build it on the fly if the element class exists.
    TempNewSymbol elem_sym = SymbolTable::new_symbol(sym->as_utf8()+1,
                                                 sym->utf8_length()-1,
                                                 CHECK_NULL);

    // Get element Klass recursively.
    Klass* elem_klass =
      get_klass_by_name_impl(accessing_klass,
                             cpool,
                             elem_sym,
                             require_local);
    if (elem_klass != NULL) {
      // Now make an array for it
      return elem_klass->array_klass(CHECK_NULL);
    }
  }

  if (found_klass == NULL && !cpool.is_null() && cpool->has_preresolution()) {
    // Look inside the constant pool for pre-resolved class entries.
    for (int i = cpool->length() - 1; i >= 1; i--) {
      if (cpool->tag_at(i).is_klass()) {
        Klass*  kls = cpool->resolved_klass_at(i);
        if (kls->name() == sym) {
          return kls;
        }
      }
    }
  }

  return found_klass;
}

// ------------------------------------------------------------------
Klass* JVMCIRuntime::get_klass_by_name(Klass* accessing_klass,
                                  Symbol* klass_name,
                                  bool require_local) {
  ResourceMark rm;
  constantPoolHandle cpool;
  return get_klass_by_name_impl(accessing_klass,
                                                 cpool,
                                                 klass_name,
                                                 require_local);
}

// ------------------------------------------------------------------
// Implementation of get_klass_by_index.
Klass* JVMCIRuntime::get_klass_by_index_impl(const constantPoolHandle& cpool,
                                        int index,
                                        bool& is_accessible,
                                        Klass* accessor) {
  JVMCI_EXCEPTION_CONTEXT;
  Klass* klass = ConstantPool::klass_at_if_loaded(cpool, index);
  Symbol* klass_name = NULL;
  if (klass == NULL) {
    klass_name = cpool->klass_name_at(index);
  }

  if (klass == NULL) {
    // Not found in constant pool.  Use the name to do the lookup.
    Klass* k = get_klass_by_name_impl(accessor,
                                        cpool,
                                        klass_name,
                                        false);
    // Calculate accessibility the hard way.
    if (k == NULL) {
      is_accessible = false;
    } else if (k->class_loader() != accessor->class_loader() &&
               get_klass_by_name_impl(accessor, cpool, k->name(), true) == NULL) {
      // Loaded only remotely.  Not linked yet.
      is_accessible = false;
    } else {
      // Linked locally, and we must also check public/private, etc.
      is_accessible = check_klass_accessibility(accessor, k);
    }
    if (!is_accessible) {
      return NULL;
    }
    return k;
  }

  // It is known to be accessible, since it was found in the constant pool.
  is_accessible = true;
  return klass;
}

// ------------------------------------------------------------------
// Get a klass from the constant pool.
Klass* JVMCIRuntime::get_klass_by_index(const constantPoolHandle& cpool,
                                   int index,
                                   bool& is_accessible,
                                   Klass* accessor) {
  ResourceMark rm;
  Klass* result = get_klass_by_index_impl(cpool, index, is_accessible, accessor);
  return result;
}

// ------------------------------------------------------------------
// Implementation of get_field_by_index.
//
// Implementation note: the results of field lookups are cached
// in the accessor klass.
void JVMCIRuntime::get_field_by_index_impl(InstanceKlass* klass, fieldDescriptor& field_desc,
                                        int index) {
  JVMCI_EXCEPTION_CONTEXT;

  assert(klass->is_linked(), "must be linked before using its constant-pool");

  constantPoolHandle cpool(thread, klass->constants());

  // Get the field's name, signature, and type.
  Symbol* name  = cpool->name_ref_at(index);

  int nt_index = cpool->name_and_type_ref_index_at(index);
  int sig_index = cpool->signature_ref_index_at(nt_index);
  Symbol* signature = cpool->symbol_at(sig_index);

  // Get the field's declared holder.
  int holder_index = cpool->klass_ref_index_at(index);
  bool holder_is_accessible;
  Klass* declared_holder = get_klass_by_index(cpool, holder_index,
                                               holder_is_accessible,
                                               klass);

  // The declared holder of this field may not have been loaded.
  // Bail out with partial field information.
  if (!holder_is_accessible) {
    return;
  }


  // Perform the field lookup.
  Klass*  canonical_holder =
    InstanceKlass::cast(declared_holder)->find_field(name, signature, &field_desc);
  if (canonical_holder == NULL) {
    return;
  }

  assert(canonical_holder == field_desc.field_holder(), "just checking");
}

// ------------------------------------------------------------------
// Get a field by index from a klass's constant pool.
void JVMCIRuntime::get_field_by_index(InstanceKlass* accessor, fieldDescriptor& fd, int index) {
  ResourceMark rm;
  return get_field_by_index_impl(accessor, fd, index);
}

// ------------------------------------------------------------------
// Perform an appropriate method lookup based on accessor, holder,
// name, signature, and bytecode.
methodHandle JVMCIRuntime::lookup_method(InstanceKlass* h_accessor,
                               InstanceKlass* h_holder,
                               Symbol*       name,
                               Symbol*       sig,
                               Bytecodes::Code bc) {
  JVMCI_EXCEPTION_CONTEXT;
  LinkResolver::check_klass_accessability(h_accessor, h_holder, KILL_COMPILE_ON_FATAL_(NULL));
  methodHandle dest_method;
  switch (bc) {
  case Bytecodes::_invokestatic:
    dest_method =
      LinkResolver::resolve_static_call_or_null(h_holder, name, sig, h_accessor);
    break;
  case Bytecodes::_invokespecial:
    dest_method =
      LinkResolver::resolve_special_call_or_null(h_holder, name, sig, h_accessor);
    break;
  case Bytecodes::_invokeinterface:
    dest_method =
      LinkResolver::linktime_resolve_interface_method_or_null(h_holder, name, sig,
                                                              h_accessor, true);
    break;
  case Bytecodes::_invokevirtual:
    dest_method =
      LinkResolver::linktime_resolve_virtual_method_or_null(h_holder, name, sig,
                                                            h_accessor, true);
    break;
  default: ShouldNotReachHere();
  }

  return dest_method;
}


// ------------------------------------------------------------------
methodHandle JVMCIRuntime::get_method_by_index_impl(const constantPoolHandle& cpool,
                                          int index, Bytecodes::Code bc,
                                          InstanceKlass* accessor) {
  if (bc == Bytecodes::_invokedynamic) {
    ConstantPoolCacheEntry* cpce = cpool->invokedynamic_cp_cache_entry_at(index);
    bool is_resolved = !cpce->is_f1_null();
    if (is_resolved) {
      // Get the invoker Method* from the constant pool.
      // (The appendix argument, if any, will be noted in the method's signature.)
      Method* adapter = cpce->f1_as_method();
      return methodHandle(adapter);
    }

    return NULL;
  }

  int holder_index = cpool->klass_ref_index_at(index);
  bool holder_is_accessible;
  Klass* holder = get_klass_by_index_impl(cpool, holder_index, holder_is_accessible, accessor);

  // Get the method's name and signature.
  Symbol* name_sym = cpool->name_ref_at(index);
  Symbol* sig_sym  = cpool->signature_ref_at(index);

  if (cpool->has_preresolution()
      || (holder == SystemDictionary::MethodHandle_klass() &&
          MethodHandles::is_signature_polymorphic_name(holder, name_sym))) {
    // Short-circuit lookups for JSR 292-related call sites.
    // That is, do not rely only on name-based lookups, because they may fail
    // if the names are not resolvable in the boot class loader (7056328).
    switch (bc) {
    case Bytecodes::_invokevirtual:
    case Bytecodes::_invokeinterface:
    case Bytecodes::_invokespecial:
    case Bytecodes::_invokestatic:
      {
        Method* m = ConstantPool::method_at_if_loaded(cpool, index);
        if (m != NULL) {
          return m;
        }
      }
      break;
    }
  }

  if (holder_is_accessible) { // Our declared holder is loaded.
    InstanceKlass* lookup = get_instance_klass_for_declared_method_holder(holder);
    methodHandle m = lookup_method(accessor, lookup, name_sym, sig_sym, bc);
    if (!m.is_null()) {
      // We found the method.
      return m;
    }
  }

  // Either the declared holder was not loaded, or the method could
  // not be found.

  return NULL;
}

// ------------------------------------------------------------------
InstanceKlass* JVMCIRuntime::get_instance_klass_for_declared_method_holder(Klass* method_holder) {
  // For the case of <array>.clone(), the method holder can be an ArrayKlass*
  // instead of an InstanceKlass*.  For that case simply pretend that the
  // declared holder is Object.clone since that's where the call will bottom out.
  if (method_holder->oop_is_instance()) {
    return InstanceKlass::cast(method_holder);
  } else if (method_holder->oop_is_array()) {
    return InstanceKlass::cast(SystemDictionary::Object_klass());
  } else {
    ShouldNotReachHere();
  }
  return NULL;
}


// ------------------------------------------------------------------
methodHandle JVMCIRuntime::get_method_by_index(const constantPoolHandle& cpool,
                                     int index, Bytecodes::Code bc,
                                     InstanceKlass* accessor) {
  ResourceMark rm;
  return get_method_by_index_impl(cpool, index, bc, accessor);
}

// ------------------------------------------------------------------
// Check for changes to the system dictionary during compilation
// class loads, evolution, breakpoints
JVMCI::CodeInstallResult JVMCIRuntime::validate_compile_task_dependencies(Dependencies* dependencies, JVMCICompileState* compile_state, char** failure_detail) {
  // If JVMTI capabilities were enabled during compile, the compilation is invalidated.
  if (compile_state != NULL && compile_state->jvmti_state_changed()) {
    *failure_detail = (char*) "Jvmti state change during compilation invalidated dependencies";
    return JVMCI::dependencies_failed;
  }

  // Dependencies must be checked when the system dictionary changes
  // or if we don't know whether it has changed (i.e., compile_state == NULL).
  bool counter_changed = compile_state == NULL || compile_state->system_dictionary_modification_counter() != SystemDictionary::number_of_modifications();
  CompileTask* task = compile_state == NULL ? NULL : compile_state->task();
  Dependencies::DepType result = dependencies->validate_dependencies(task, counter_changed, failure_detail);
  if (result == Dependencies::end_marker) {
    return JVMCI::ok;
  }

  if (!Dependencies::is_klass_type(result) || counter_changed) {
    return JVMCI::dependencies_failed;
  }
  // The dependencies were invalid at the time of installation
  // without any intervening modification of the system
  // dictionary.  That means they were invalidly constructed.
  return JVMCI::dependencies_invalid;
}


void JVMCIRuntime::compile_method(JVMCIEnv* JVMCIENV, JVMCICompiler* compiler, const methodHandle& method, int entry_bci) {
  JVMCI_EXCEPTION_CONTEXT

  JVMCICompileState* compile_state = JVMCIENV->compile_state();

  bool is_osr = entry_bci != InvocationEntryBci;
  if (compiler->is_bootstrapping() && is_osr) {
    // no OSR compilations during bootstrap - the compiler is just too slow at this point,
    // and we know that there are no endless loops
    compile_state->set_failure("No OSR during boostrap", true);
    return;
  }

  HandleMark hm;
  JVMCIObject receiver = get_HotSpotJVMCIRuntime(JVMCI_CHECK_EXIT);
  JVMCIObject jvmci_method = JVMCIENV->get_jvmci_method(method, JVMCI_CHECK);
  JVMCIObject result_object = JVMCIENV->call_HotSpotJVMCIRuntime_compileMethod(receiver, jvmci_method, entry_bci,
                                                                     (jlong) compile_state, compile_state->task()->compile_id());
  if (!JVMCIENV->has_pending_exception()) {
    if (result_object.is_non_null()) {
      JVMCIObject failure_message = JVMCIENV->get_HotSpotCompilationRequestResult_failureMessage(result_object);
      if (failure_message.is_non_null()) {
        const char* failure_reason = JVMCIENV->as_utf8_string(failure_message);
        compile_state->set_failure(failure_reason, JVMCIENV->get_HotSpotCompilationRequestResult_retry(result_object) != 0);
      } else {
        if (compile_state->task()->code() == NULL) {
          compile_state->set_failure("no nmethod produced", true);
        } else {
          compile_state->task()->set_num_inlined_bytecodes(JVMCIENV->get_HotSpotCompilationRequestResult_inlinedBytecodes(result_object));
          compiler->inc_methods_compiled();
        }
      }
    } else {
      assert(false, "JVMCICompiler.compileMethod should always return non-null");
    }
  } else {
    // An uncaught exception was thrown during compilation. Generally these
    // should be handled by the Java code in some useful way but if they leak
    // through to here report them instead of dying or silently ignoring them.
    JVMCIENV->describe_pending_exception(true);
    compile_state->set_failure("unexpected exception thrown", false);
  }
  if (compiler->is_bootstrapping()) {
    compiler->set_bootstrap_compilation_request_handled();
  }
}


// ------------------------------------------------------------------
JVMCI::CodeInstallResult JVMCIRuntime::register_method(JVMCIEnv* JVMCIENV,
                                const methodHandle& method,
                                nmethod*& nm,
                                int entry_bci,
                                CodeOffsets* offsets,
                                int orig_pc_offset,
                                CodeBuffer* code_buffer,
                                int frame_words,
                                OopMapSet* oop_map_set,
                                ExceptionHandlerTable* handler_table,
                                ImplicitExceptionTable* implicit_exception_table,
                                AbstractCompiler* compiler,
                                DebugInformationRecorder* debug_info,
                                Dependencies* dependencies,
                                int compile_id,
                                bool has_unsafe_access,
                                bool has_wide_vector,
                                JVMCIObject compiled_code,
                                JVMCIObject nmethod_mirror,
                                JVMCIObject speculation_log) {
  JVMCI_EXCEPTION_CONTEXT;
  NMethodSweeper::possibly_sweep();
  nm = NULL;
  int comp_level = CompLevel_full_optimization;
  char* failure_detail = NULL;

  assert(JVMCIENV->isa_HotSpotNmethod(nmethod_mirror), "must be");
  bool install_default = JVMCIENV->get_HotSpotNmethod_isDefault(nmethod_mirror) != 0;
  bool triggers_invalidation = !install_default;

  JVMCINMethodData* data = new JVMCINMethodData(JVMCIENV, nmethod_mirror, speculation_log, triggers_invalidation);

  JVMCI::CodeInstallResult result;
  {
    // To prevent compile queue updates.
    MutexLocker locker(MethodCompileQueue_lock, THREAD);

    // Prevent SystemDictionary::add_to_hierarchy from running
    // and invalidating our dependencies until we install this method.
    MutexLocker ml(Compile_lock);

    // Encode the dependencies now, so we can check them right away.
    dependencies->encode_content_bytes();

    // Record the dependencies for the current compile in the log
    if (LogCompilation) {
      for (Dependencies::DepStream deps(dependencies); deps.next(); ) {
        deps.log_dependency();
      }
    }

    // Check for {class loads, evolution, breakpoints} during compilation
    result = validate_compile_task_dependencies(dependencies, JVMCIENV->compile_state(), &failure_detail);
    if (result != JVMCI::ok) {
      // While not a true deoptimization, it is a preemptive decompile.
      MethodData* mdp = method()->method_data();
      if (mdp != NULL) {
        mdp->inc_decompile_count();
#ifdef ASSERT
        if (mdp->decompile_count() > (uint)PerMethodRecompilationCutoff) {
          ResourceMark m;
          tty->print_cr("WARN: endless recompilation of %s. Method was set to not compilable.", method()->name_and_sig_as_C_string());
        }
#endif
      }

      // All buffers in the CodeBuffer are allocated in the CodeCache.
      // If the code buffer is created on each compile attempt
      // as in C2, then it must be freed.
      //code_buffer->free_blob();
    } else {
      nm =  nmethod::new_nmethod(method,
                                 compile_id,
                                 entry_bci,
                                 offsets,
                                 orig_pc_offset,
                                 debug_info, dependencies, code_buffer,
                                 frame_words, oop_map_set,
                                 handler_table, implicit_exception_table,
                                 compiler, comp_level,
                                 data);

      // Free codeBlobs
      //code_buffer->free_blob();
      if (nm == NULL) {
        // The CodeCache is full.  Print out warning and disable compilation.
        {
          MutexUnlocker ml(Compile_lock);
          MutexUnlocker locker(MethodCompileQueue_lock);
          CompileBroker::handle_full_code_cache();
        }
      } else {
        nm->set_has_unsafe_access(has_unsafe_access);
        nm->set_has_wide_vectors(has_wide_vector);

        // Record successful registration.
        // (Put nm into the task handle *before* publishing to the Java heap.)
        if (JVMCIENV->compile_state() != NULL) {
          JVMCIENV->compile_state()->task()->set_code(nm);
        }

        if (install_default) {
          if (entry_bci == InvocationEntryBci) {
            if (TieredCompilation) {
              // If there is an old version we're done with it
              nmethod* old = method->code();
              if (TraceMethodReplacement && old != NULL) {
                ResourceMark rm;
                char *method_name = method->name_and_sig_as_C_string();
                tty->print_cr("Replacing method %s", method_name);
              }
              if (old != NULL ) {
                old->make_not_entrant();
              }
            }
            if (TraceNMethodInstalls) {
              ResourceMark rm;
              char *method_name = method->name_and_sig_as_C_string();
              ttyLocker ttyl;
              tty->print_cr("Installing method (%d) %s [entry point: %p]",
                            comp_level,
                            method_name, nm->entry_point());
            }
            // Allow the code to be executed
            method->set_code(method, nm);
          } else {
            if (TraceNMethodInstalls ) {
              ResourceMark rm;
              char *method_name = method->name_and_sig_as_C_string();
              ttyLocker ttyl;
              tty->print_cr("Installing osr method (%d) %s @ %d",
                            comp_level,
                            method_name,
                            entry_bci);
            }
            InstanceKlass::cast(method->method_holder())->add_osr_nmethod(nm);
          }
        }
      }
      result = nm != NULL ? JVMCI::ok :JVMCI::cache_full;
    }
  }

  if (result != JVMCI::ok) {
    delete data;
    data = NULL;
  }

  // String creation must be done outside lock
  if (failure_detail != NULL) {
    // A failure to allocate the string is silently ignored.
    JVMCIObject message = JVMCIENV->create_string(failure_detail, JVMCIENV);
    JVMCIENV->set_HotSpotCompiledNmethod_installationFailureMessage(compiled_code, message);
  }

  // JVMTI -- compiled method notification (must be done outside lock)
  if (nm != NULL) {
    nm->post_compiled_method_load_event();
  }

  return result;
}
