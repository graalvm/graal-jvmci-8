/*
 * Copyright (c) 2012, Oracle and/or its affiliates. All rights reserved.
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
#include "compiler/compileBroker.hpp"
#include "compiler/disassembler.hpp"
#include "graal/graalRuntime.hpp"
#include "graal/graalCompilerToVM.hpp"
#include "graal/graalCompiler.hpp"
#include "graal/graalJavaAccess.hpp"
#include "graal/graalEnv.hpp"
#include "memory/oopFactory.hpp"
#include "prims/jvm.h"
#include "runtime/biasedLocking.hpp"
#include "runtime/interfaceSupport.hpp"
#include "runtime/arguments.hpp"
#include "runtime/reflection.hpp"
#include "utilities/debug.hpp"
#include "utilities/defaultStream.hpp"

jobject GraalRuntime::_HotSpotGraalRuntime_instance = NULL;
bool GraalRuntime::_HotSpotGraalRuntime_initialized = false;
bool GraalRuntime::_shutdown_called = false;

void GraalRuntime::initialize_natives(JNIEnv *env, jclass c2vmClass) {
  uintptr_t heap_end = (uintptr_t) Universe::heap()->reserved_region().end();
  uintptr_t allocation_end = heap_end + ((uintptr_t)16) * 1024 * 1024 * 1024;
  AMD64_ONLY(guarantee(heap_end < allocation_end, "heap end too close to end of address space (might lead to erroneous TLAB allocations)"));
  NOT_LP64(error("check TLAB allocation code for address space conflicts"));

  ensure_graal_class_loader_is_initialized();

  JavaThread* THREAD = JavaThread::current();
  {
    ThreadToNativeFromVM trans(THREAD);

    ResourceMark rm;
    HandleMark hm;

    graal_compute_offsets();

    // Ensure _non_oop_bits is initialized
    Universe::non_oop_word();

    env->RegisterNatives(c2vmClass, CompilerToVM_methods, CompilerToVM_methods_count());
  }
  if (HAS_PENDING_EXCEPTION) {
    abort_on_pending_exception(PENDING_EXCEPTION, "Could not register natives");
  }
}

BufferBlob* GraalRuntime::initialize_buffer_blob() {
  JavaThread* THREAD = JavaThread::current();
  BufferBlob* buffer_blob = THREAD->get_buffer_blob();
  if (buffer_blob == NULL) {
    buffer_blob = BufferBlob::create("Graal thread-local CodeBuffer", GraalNMethodSizeLimit);
    if (buffer_blob != NULL) {
      THREAD->set_buffer_blob(buffer_blob);
    }
  }
  return buffer_blob;
}

BasicType GraalRuntime::kindToBasicType(jchar ch) {
  switch(ch) {
    case 'z': return T_BOOLEAN;
    case 'b': return T_BYTE;
    case 's': return T_SHORT;
    case 'c': return T_CHAR;
    case 'i': return T_INT;
    case 'f': return T_FLOAT;
    case 'j': return T_LONG;
    case 'd': return T_DOUBLE;
    case 'a': return T_OBJECT;
    case '-': return T_ILLEGAL;
    default:
      fatal(err_msg("unexpected Kind: %c", ch));
      break;
  }
  return T_ILLEGAL;
}

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

JRT_BLOCK_ENTRY(void, GraalRuntime::new_instance(JavaThread* thread, Klass* klass))
  JRT_BLOCK;
  assert(klass->is_klass(), "not a class");
  instanceKlassHandle h(thread, klass);
  h->check_valid_for_instantiation(true, CHECK);
  // make sure klass is initialized
  h->initialize(CHECK);
  // allocate instance and return via TLS
  oop obj = h->allocate_instance(CHECK);
  thread->set_vm_result(obj);
  JRT_BLOCK_END;

  if (GraalDeferredInitBarriers) {
    new_store_pre_barrier(thread);
  }
JRT_END

JRT_BLOCK_ENTRY(void, GraalRuntime::new_array(JavaThread* thread, Klass* array_klass, jint length))
  JRT_BLOCK;
  // Note: no handle for klass needed since they are not used
  //       anymore after new_objArray() and no GC can happen before.
  //       (This may have to change if this code changes!)
  assert(array_klass->is_klass(), "not a class");
  oop obj;
  if (array_klass->oop_is_typeArray()) {
    BasicType elt_type = TypeArrayKlass::cast(array_klass)->element_type();
    obj = oopFactory::new_typeArray(elt_type, length, CHECK);
  } else {
    Klass* elem_klass = ObjArrayKlass::cast(array_klass)->element_klass();
    obj = oopFactory::new_objArray(elem_klass, length, CHECK);
  }
  thread->set_vm_result(obj);
  // This is pretty rare but this runtime patch is stressful to deoptimization
  // if we deoptimize here so force a deopt to stress the path.
  if (DeoptimizeALot) {
    static int deopts = 0;
    // Alternate between deoptimizing and raising an error (which will also cause a deopt)
    if (deopts++ % 2 == 0) {
      ResourceMark rm(THREAD);
      THROW(vmSymbols::java_lang_OutOfMemoryError());
    } else {
      deopt_caller();
    }
  }
  JRT_BLOCK_END;

  if (GraalDeferredInitBarriers) {
    new_store_pre_barrier(thread);
  }
JRT_END

void GraalRuntime::new_store_pre_barrier(JavaThread* thread) {
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

JRT_ENTRY(void, GraalRuntime::new_multi_array(JavaThread* thread, Klass* klass, int rank, jint* dims))
  assert(klass->is_klass(), "not a class");
  assert(rank >= 1, "rank must be nonzero");
  oop obj = ArrayKlass::cast(klass)->multi_allocate(rank, dims, CHECK);
  thread->set_vm_result(obj);
JRT_END

JRT_ENTRY(void, GraalRuntime::dynamic_new_array(JavaThread* thread, oopDesc* element_mirror, jint length))
  oop obj = Reflection::reflect_new_array(element_mirror, length, CHECK);
  thread->set_vm_result(obj);
JRT_END

JRT_ENTRY(void, GraalRuntime::dynamic_new_instance(JavaThread* thread, oopDesc* type_mirror))
  instanceKlassHandle klass(THREAD, java_lang_Class::as_Klass(type_mirror));

  if (klass == NULL) {
    ResourceMark rm(THREAD);
    THROW(vmSymbols::java_lang_InstantiationException());
  }

  // Create new instance (the receiver)
  klass->check_valid_for_instantiation(false, CHECK);

  // Make sure klass gets initialized
  klass->initialize(CHECK);

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
  assert(nm != NULL, "this is not an nmethod");
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

    continuation = SharedRuntime::compute_compiled_exc_handler(nm, pc, exception, false, false);
    // If an exception was thrown during exception dispatch, the exception oop may have changed
    thread->set_exception_oop(exception());
    thread->set_exception_pc(pc);

    // the exception cache is used only by non-implicit exceptions
    if (continuation != NULL && !SharedRuntime::deopt_blob()->contains(continuation)) {
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
address GraalRuntime::exception_handler_for_pc(JavaThread* thread) {
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

  // Now check to see if the nmethod we were called from is now deoptimized.
  // If so we must return to the deopt blob and deoptimize the nmethod
  if (nm != NULL && caller_is_deopted()) {
    continuation = SharedRuntime::deopt_blob()->unpack_with_exception_in_tls();
  }

  assert(continuation != NULL, "no handler found");
  return continuation;
}

JRT_ENTRY(void, GraalRuntime::create_null_exception(JavaThread* thread))
  SharedRuntime::throw_and_post_jvmti_exception(thread, vmSymbols::java_lang_NullPointerException());
  thread->set_vm_result(PENDING_EXCEPTION);
  CLEAR_PENDING_EXCEPTION;
JRT_END

JRT_ENTRY(void, GraalRuntime::create_out_of_bounds_exception(JavaThread* thread, jint index))
  char message[jintAsStringSize];
  sprintf(message, "%d", index);
  SharedRuntime::throw_and_post_jvmti_exception(thread, vmSymbols::java_lang_ArrayIndexOutOfBoundsException(), message);
  thread->set_vm_result(PENDING_EXCEPTION);
  CLEAR_PENDING_EXCEPTION;
JRT_END

JRT_ENTRY_NO_ASYNC(void, GraalRuntime::monitorenter(JavaThread* thread, oopDesc* obj, BasicLock* lock))
  if (TraceGraal >= 3) {
    char type[O_BUFLEN];
    obj->klass()->name()->as_C_string(type, O_BUFLEN);
    markOop mark = obj->mark();
    tty->print_cr("%s: entered locking slow case with obj=" INTPTR_FORMAT ", type=%s, mark=" INTPTR_FORMAT ", lock=" INTPTR_FORMAT, thread->name(), p2i(obj), type, p2i(mark), p2i(lock));
    tty->flush();
  }
#ifdef ASSERT
  if (PrintBiasedLockingStatistics) {
    Atomic::inc(BiasedLocking::slow_path_entry_count_addr());
  }
#endif
  Handle h_obj(thread, obj);
  assert(h_obj()->is_oop(), "must be NULL or an object");
  if (UseBiasedLocking) {
    // Retry fast entry if bias is revoked to avoid unnecessary inflation
    ObjectSynchronizer::fast_enter(h_obj, lock, true, CHECK);
  } else {
    if (GraalUseFastLocking) {
      // When using fast locking, the compiled code has already tried the fast case
      ObjectSynchronizer::slow_enter(h_obj, lock, THREAD);
    } else {
      ObjectSynchronizer::fast_enter(h_obj, lock, false, THREAD);
    }
  }
  if (TraceGraal >= 3) {
    tty->print_cr("%s: exiting locking slow with obj=" INTPTR_FORMAT, thread->name(), p2i(obj));
  }
JRT_END

JRT_LEAF(void, GraalRuntime::monitorexit(JavaThread* thread, oopDesc* obj, BasicLock* lock))
  assert(thread == JavaThread::current(), "threads must correspond");
  assert(thread->last_Java_sp(), "last_Java_sp must be set");
  // monitorexit is non-blocking (leaf routine) => no exceptions can be thrown
  EXCEPTION_MARK;

#ifdef DEBUG
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

  if (GraalUseFastLocking) {
    // When using fast locking, the compiled code has already tried the fast case
    ObjectSynchronizer::slow_exit(obj, lock, THREAD);
  } else {
    ObjectSynchronizer::fast_exit(obj, lock, THREAD);
  }
  if (TraceGraal >= 3) {
    char type[O_BUFLEN];
    obj->klass()->name()->as_C_string(type, O_BUFLEN);
    tty->print_cr("%s: exited locking slow case with obj=" INTPTR_FORMAT ", type=%s, mark=" INTPTR_FORMAT ", lock=" INTPTR_FORMAT, thread->name(), p2i(obj), type, p2i(obj->mark()), p2i(lock));
    tty->flush();
  }
JRT_END

JRT_LEAF(void, GraalRuntime::log_object(JavaThread* thread, oopDesc* obj, jint flags))
  bool string =  mask_bits_are_true(flags, LOG_OBJECT_STRING);
  bool addr = mask_bits_are_true(flags, LOG_OBJECT_ADDRESS);
  bool newline = mask_bits_are_true(flags, LOG_OBJECT_NEWLINE);
  if (!string) {
    if (!addr && obj->is_oop_or_null(true)) {
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

JRT_LEAF(void, GraalRuntime::write_barrier_pre(JavaThread* thread, oopDesc* obj))
  thread->satb_mark_queue().enqueue(obj);
JRT_END

JRT_LEAF(void, GraalRuntime::write_barrier_post(JavaThread* thread, void* card_addr))
  thread->dirty_card_queue().enqueue(card_addr);
JRT_END

JRT_LEAF(jboolean, GraalRuntime::validate_object(JavaThread* thread, oopDesc* parent, oopDesc* child))
  bool ret = true;
  if(!Universe::heap()->is_in_closed_subset(parent)) {
    tty->print_cr("Parent Object "INTPTR_FORMAT" not in heap", p2i(parent));
    parent->print();
    ret=false;
  }
  if(!Universe::heap()->is_in_closed_subset(child)) {
    tty->print_cr("Child Object "INTPTR_FORMAT" not in heap", p2i(child));
    child->print();
    ret=false;
  }
  return (jint)ret;
JRT_END

JRT_ENTRY(void, GraalRuntime::vm_error(JavaThread* thread, jlong where, jlong format, jlong value))
  ResourceMark rm;
  const char *error_msg = where == 0L ? "<internal Graal error>" : (char*) (address) where;
  char *detail_msg = NULL;
  if (format != 0L) {
    const char* buf = (char*) (address) format;
    size_t detail_msg_length = strlen(buf) * 2;
    detail_msg = (char *) NEW_RESOURCE_ARRAY(u_char, detail_msg_length);
    jio_snprintf(detail_msg, detail_msg_length, buf, value);
  }
  report_vm_error(__FILE__, __LINE__, error_msg, detail_msg);
JRT_END

JRT_LEAF(oopDesc*, GraalRuntime::load_and_clear_exception(JavaThread* thread))
  oop exception = thread->exception_oop();
  assert(exception != NULL, "npe");
  thread->set_exception_oop(NULL);
  thread->set_exception_pc(0);
  return exception;
JRT_END

JRT_LEAF(void, GraalRuntime::log_printf(JavaThread* thread, oopDesc* format, jlong v1, jlong v2, jlong v3))
  ResourceMark rm;
  assert(format != NULL && java_lang_String::is_instance(format), "must be");
  char *buf = java_lang_String::as_utf8_string(format);
  tty->print(buf, v1, v2, v3);
JRT_END

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

JRT_LEAF(void, GraalRuntime::vm_message(jboolean vmError, jlong format, jlong v1, jlong v2, jlong v3))
  ResourceMark rm;
  char *buf = (char*) (address) format;
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

JRT_LEAF(void, GraalRuntime::log_primitive(JavaThread* thread, jchar typeChar, jlong value, jboolean newline))
  union {
      jlong l;
      jdouble d;
      jfloat f;
  } uu;
  uu.l = value;
  switch (typeChar) {
    case 'z': tty->print(value == 0 ? "false" : "true"); break;
    case 'b': tty->print("%d", (jbyte) value); break;
    case 'c': tty->print("%c", (jchar) value); break;
    case 's': tty->print("%d", (jshort) value); break;
    case 'i': tty->print("%d", (jint) value); break;
    case 'f': tty->print("%f", uu.f); break;
    case 'j': tty->print(JLONG_FORMAT, value); break;
    case 'd': tty->print("%lf", uu.d); break;
    default: assert(false, "unknown typeChar"); break;
  }
  if (newline) {
    tty->cr();
  }
JRT_END

JRT_ENTRY(jint, GraalRuntime::identity_hash_code(JavaThread* thread, oopDesc* obj))
  return (jint) obj->identity_hash();
JRT_END

JRT_ENTRY(jboolean, GraalRuntime::thread_is_interrupted(JavaThread* thread, oopDesc* receiver, jboolean clear_interrupted))
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

JRT_ENTRY(jint, GraalRuntime::test_deoptimize_call_int(JavaThread* thread, int value))
  deopt_caller();
  return value;
JRT_END

// private static void Factory.init()
JVM_ENTRY(void, JVM_InitGraalClassLoader(JNIEnv *env, jclass c, jobject loader_handle))
  SystemDictionary::init_graal_loader(JNIHandles::resolve(loader_handle));
  SystemDictionary::WKID scan = SystemDictionary::FIRST_GRAAL_WKID;
  SystemDictionary::initialize_wk_klasses_through(SystemDictionary::LAST_GRAAL_WKID, scan, CHECK);
JVM_END

// boolean com.oracle.graal.hotspot.HotSpotOptions.isCITimingEnabled()
JVM_ENTRY(jboolean, JVM_IsCITimingEnabled(JNIEnv *env, jclass c))
  return CITime || CITimeEach;
JVM_END

// private static GraalRuntime Graal.initializeRuntime()
JVM_ENTRY(jobject, JVM_GetGraalRuntime(JNIEnv *env, jclass c))
  GraalRuntime::initialize_HotSpotGraalRuntime();
  return GraalRuntime::get_HotSpotGraalRuntime_jobject();
JVM_END

// private static String[] Services.getServiceImpls(Class service)
JVM_ENTRY(jobject, JVM_GetGraalServiceImpls(JNIEnv *env, jclass c, jclass serviceClass))
  HandleMark hm;
  ResourceMark rm;
  KlassHandle serviceKlass(THREAD, java_lang_Class::as_Klass(JNIHandles::resolve_non_null(serviceClass)));
  return JNIHandles::make_local(THREAD, GraalRuntime::get_service_impls(serviceKlass, THREAD)());
JVM_END

// private static TruffleRuntime Truffle.createRuntime()
JVM_ENTRY(jobject, JVM_CreateTruffleRuntime(JNIEnv *env, jclass c))
  GraalRuntime::ensure_graal_class_loader_is_initialized();
  TempNewSymbol name = SymbolTable::new_symbol("com/oracle/graal/truffle/hotspot/HotSpotTruffleRuntime", CHECK_NULL);
  KlassHandle klass = GraalRuntime::resolve_or_fail(name, CHECK_NULL);

  TempNewSymbol makeInstance = SymbolTable::new_symbol("makeInstance", CHECK_NULL);
  TempNewSymbol sig = SymbolTable::new_symbol("()Lcom/oracle/truffle/api/TruffleRuntime;", CHECK_NULL);
  JavaValue result(T_OBJECT);
  JavaCalls::call_static(&result, klass, makeInstance, sig, CHECK_NULL);
  return JNIHandles::make_local(THREAD, (oop) result.get_jobject());
JVM_END

// private static NativeFunctionInterfaceRuntime.createInterface()
JVM_ENTRY(jobject, JVM_CreateNativeFunctionInterface(JNIEnv *env, jclass c))
  GraalRuntime::ensure_graal_class_loader_is_initialized();
  TempNewSymbol name = SymbolTable::new_symbol("com/oracle/graal/truffle/hotspot/HotSpotTruffleRuntime", CHECK_NULL);
  KlassHandle klass = GraalRuntime::resolve_or_fail(name, CHECK_NULL);

  TempNewSymbol makeInstance = SymbolTable::new_symbol("createNativeFunctionInterface", CHECK_NULL);
  TempNewSymbol sig = SymbolTable::new_symbol("()Lcom/oracle/nfi/api/NativeFunctionInterface;", CHECK_NULL);
  JavaValue result(T_OBJECT);
  JavaCalls::call_static(&result, klass, makeInstance, sig, CHECK_NULL);
  return JNIHandles::make_local(THREAD, (oop) result.get_jobject());
JVM_END

Handle GraalRuntime::callInitializer(const char* className, const char* methodName, const char* returnType) {
  guarantee(!_HotSpotGraalRuntime_initialized, "cannot reinitialize HotSpotGraalRuntime");
  Thread* THREAD = Thread::current();

  TempNewSymbol name = SymbolTable::new_symbol(className, CHECK_ABORT_(Handle()));
  KlassHandle klass = load_required_class(name);
  TempNewSymbol runtime = SymbolTable::new_symbol(methodName, CHECK_ABORT_(Handle()));
  TempNewSymbol sig = SymbolTable::new_symbol(returnType, CHECK_ABORT_(Handle()));
  JavaValue result(T_OBJECT);
  JavaCalls::call_static(&result, klass, runtime, sig, CHECK_ABORT_(Handle()));
  return Handle((oop)result.get_jobject());
}

void GraalRuntime::initialize_HotSpotGraalRuntime() {
  if (JNIHandles::resolve(_HotSpotGraalRuntime_instance) == NULL) {
#ifdef ASSERT
    // This should only be called in the context of the Graal class being initialized
    Thread* THREAD = Thread::current();
    TempNewSymbol name = SymbolTable::new_symbol("com/oracle/graal/api/runtime/Graal", CHECK_ABORT);
    instanceKlassHandle klass = InstanceKlass::cast(load_required_class(name));
    assert(klass->is_being_initialized() && klass->is_reentrant_initialization(THREAD),
           "HotSpotGraalRuntime initialization should only be triggered through Graal initialization");
#endif

    Handle result = callInitializer("com/oracle/graal/hotspot/HotSpotGraalRuntime", "runtime",
                                    "()Lcom/oracle/graal/hotspot/HotSpotGraalRuntime;");
    _HotSpotGraalRuntime_initialized = true;
    _HotSpotGraalRuntime_instance = JNIHandles::make_global(result());
  }
}

void GraalRuntime::initialize_Graal() {
  if (JNIHandles::resolve(_HotSpotGraalRuntime_instance) == NULL) {
    callInitializer("com/oracle/graal/api/runtime/Graal",     "getRuntime",      "()Lcom/oracle/graal/api/runtime/GraalRuntime;");
  }
  assert(_HotSpotGraalRuntime_initialized == true, "what?");
}

// private static void CompilerToVMImpl.init()
JVM_ENTRY(void, JVM_InitializeGraalNatives(JNIEnv *env, jclass c2vmClass))
  GraalRuntime::initialize_natives(env, c2vmClass);
JVM_END

void GraalRuntime::ensure_graal_class_loader_is_initialized() {
  // This initialization code is guarded by a static pointer to the Factory class.
  // Once it is non-null, the Graal class loader and well known Graal classes are
  // guaranteed to have been initialized. By going through the static
  // initializer of Factory, we can rely on class initialization semantics to
  // synchronize threads racing to do the initialization.
  static Klass* _FactoryKlass = NULL;
  if (_FactoryKlass == NULL) {
    Thread* THREAD = Thread::current();
    TempNewSymbol name = SymbolTable::new_symbol("com/oracle/graal/hotspot/loader/Factory", CHECK_ABORT);
    KlassHandle klass = SystemDictionary::resolve_or_fail(name, true, THREAD);
    if (HAS_PENDING_EXCEPTION) {
      static volatile int seen_error = 0;
      if (!seen_error && Atomic::cmpxchg(1, &seen_error, 0) == 0) {
        // Only report the failure on the first thread that hits it
        abort_on_pending_exception(PENDING_EXCEPTION, "Graal classes are not available");
      } else {
        CLEAR_PENDING_EXCEPTION;
        // Give first thread time to report the error.
        os::sleep(THREAD, 100, false);
        vm_abort(false);
      }
    }

    // We cannot use graalJavaAccess for this because we are currently in the
    // process of initializing that mechanism.
    TempNewSymbol field_name = SymbolTable::new_symbol("useGraalClassLoader", CHECK_ABORT);
    fieldDescriptor field_desc;
    if (klass->find_field(field_name, vmSymbols::bool_signature(), &field_desc) == NULL) {
      ResourceMark rm;
      fatal(err_msg("Invalid layout of %s at %s", field_name->as_C_string(), klass->external_name()));
    }

    InstanceKlass* ik = InstanceKlass::cast(klass());
    address addr = ik->static_field_addr(field_desc.offset() - InstanceMirrorKlass::offset_of_static_fields());
    *((jboolean *) addr) = (jboolean) UseGraalClassLoader;
    klass->initialize(CHECK_ABORT);
    _FactoryKlass = klass();
    assert(!UseGraalClassLoader || SystemDictionary::graal_loader() != NULL, "Graal classloader should have been initialized");
  }
}

OptionsValueTable* GraalRuntime::parse_arguments() {
  OptionsTable* table = OptionsTable::load_options();
  if (table == NULL) {
    return NULL;
  }

  OptionsValueTable* options = new OptionsValueTable(table);

  // Process option overrides from graal.options first
  parse_graal_options_file(options);

  // Now process options on the command line
  int numOptions = Arguments::num_graal_args();
  for (int i = 0; i < numOptions; i++) {
    char* arg = Arguments::graal_args_array()[i];
    if (!parse_argument(options, arg)) {
      delete options;
      return NULL;
    }
  }
  return options;
}

void not_found(OptionsTable* table, const char* argname, size_t namelen) {
  jio_fprintf(defaultStream::error_stream(),"Unrecognized VM option '%.*s'\n", namelen, argname);
  OptionDesc* fuzzy_matched = table->fuzzy_match(argname, strlen(argname));
  if (fuzzy_matched != NULL) {
    jio_fprintf(defaultStream::error_stream(),
                "Did you mean '%s%s%s'?\n",
                (fuzzy_matched->type == _boolean) ? "(+/-)" : "",
                fuzzy_matched->name,
                (fuzzy_matched->type == _boolean) ? "" : "=<value>");
  }
}

bool GraalRuntime::parse_argument(OptionsValueTable* options, const char* arg) {
  OptionsTable* table = options->options_table();
  char first = arg[0];
  const char* name;
  size_t name_len;
  if (first == '+' || first == '-') {
    name = arg + 1;
    OptionDesc* optionDesc = table->get(name);
    if (optionDesc == NULL) {
      not_found(table, name, strlen(name));
      return false;
    }
    if (optionDesc->type != _boolean) {
      jio_fprintf(defaultStream::error_stream(), "Unexpected +/- setting in VM option '%s'\n", name);
      return false;
    }
    OptionValue value;
    value.desc = *optionDesc;
    value.boolean_value = first == '+';
    options->put(value);
    return true;
  } else {
    const char* sep = strchr(arg, '=');
    name = arg;
    const char* value = NULL;
    if (sep != NULL) {
      name_len = sep - name;
      value = sep + 1;
    } else {
      name_len = strlen(name);
    }
    OptionDesc* optionDesc = table->get(name, name_len);
    if (optionDesc == NULL) {
      not_found(table, name, name_len);
      return false;
    }
    if (optionDesc->type == _boolean) {
      jio_fprintf(defaultStream::error_stream(), "Missing +/- setting for VM option '%s'\n", name);
      return false;
    }
    if (value == NULL) {
      jio_fprintf(defaultStream::error_stream(), "Must use '-G:%.*s=<value>' format for %.*s option", name_len, name, name_len, name);
      return false;
    }
    OptionValue optionValue;
    optionValue.desc = *optionDesc;
    char* check;
    errno = 0;
    switch(optionDesc->type) {
      case _int: {
        long int int_value = ::strtol(value, &check, 10);
        if (*check != '\0' || errno == ERANGE || int_value > max_jint || int_value < min_jint) {
          jio_fprintf(defaultStream::error_stream(), "Expected int value for VM option '%s'\n", name);
          return false;
        }
        optionValue.int_value = int_value;
        break;
      }
      case _long: {
        long long int long_value = ::strtoll(value, &check, 10);
        if (*check != '\0' || errno == ERANGE || long_value > max_jlong || long_value < min_jlong) {
          jio_fprintf(defaultStream::error_stream(), "Expected long value for VM option '%s'\n", name);
          return false;
        }
        optionValue.long_value = long_value;
        break;
      }
      case _float: {
        optionValue.float_value = ::strtof(value, &check);
        if (*check != '\0' || errno == ERANGE) {
          jio_fprintf(defaultStream::error_stream(), "Expected float value for VM option '%s'\n", name);
          return false;
        }
        break;
      }
      case _double: {
        optionValue.double_value = ::strtod(value, &check);
        if (*check != '\0' || errno == ERANGE) {
          jio_fprintf(defaultStream::error_stream(), "Expected double value for VM option '%s'\n", name);
          return false;
        }
        break;
      }
      case _string: {
        char* copy = NEW_C_HEAP_ARRAY(char, strlen(value) + 1, mtCompiler);
        strcpy(copy, value);
        optionValue.string_value = copy;
        break;
      }
      default:
        ShouldNotReachHere();
    }
    options->put(optionValue);
    return true;
  }
}

class GraalOptionParseClosure : public ParseClosure {
  OptionsValueTable* _options;
public:
  GraalOptionParseClosure(OptionsValueTable* options) : _options(options) {}
  void do_line(char* line) {
    if (!GraalRuntime::parse_argument(_options, line)) {
      warn("There was an error parsing an argument. Skipping it.");
    }
  }
};

void GraalRuntime::parse_graal_options_file(OptionsValueTable* options) {
  const char* home = Arguments::get_java_home();
  size_t path_len = strlen(home) + strlen("/lib/graal.options") + 1;
  char path[JVM_MAXPATHLEN];
  char sep = os::file_separator()[0];
  jio_snprintf(path, JVM_MAXPATHLEN, "%s%clib%cgraal.options", home, sep, sep);
  GraalOptionParseClosure closure(options);
  parse_lines(path, &closure, false);
}

#define CHECK_WARN_ABORT_(message) THREAD); \
  if (HAS_PENDING_EXCEPTION) { \
    warning(message); \
    char buf[512]; \
    jio_snprintf(buf, 512, "Uncaught exception at %s:%d", __FILE__, __LINE__); \
    GraalRuntime::abort_on_pending_exception(PENDING_EXCEPTION, buf); \
    return; \
  } \
  (void)(0

class SetOptionClosure : public ValueClosure<OptionValue> {
  Thread* _thread;
public:
  SetOptionClosure(TRAPS) : _thread(THREAD) {}
  void do_value(OptionValue* optionValue) {
    TRAPS = _thread;
    const char* declaringClass = optionValue->desc.declaringClass;
    if (declaringClass == NULL) {
      // skip PrintFlags pseudo-option
      return;
    }
    const char* fieldName = optionValue->desc.name;
    const char* fieldClass = optionValue->desc.fieldClass;

    size_t fieldSigLen = 2 + strlen(fieldClass);
    char* fieldSig = NEW_RESOURCE_ARRAY_IN_THREAD(THREAD, char, fieldSigLen + 1);
    jio_snprintf(fieldSig, fieldSigLen + 1, "L%s;", fieldClass);
    for (size_t i = 0; i < fieldSigLen; ++i) {
      if (fieldSig[i] == '.') {
        fieldSig[i] = '/';
      }
    }
    fieldSig[fieldSigLen] = '\0';
    size_t declaringClassLen = strlen(declaringClass);
    char* declaringClassBinary = NEW_RESOURCE_ARRAY_IN_THREAD(THREAD, char, declaringClassLen + 1);
    for (size_t i = 0; i < declaringClassLen; ++i) {
      if (declaringClass[i] == '.') {
        declaringClassBinary[i] = '/';
      } else {
        declaringClassBinary[i] = declaringClass[i];
      }
    }
    declaringClassBinary[declaringClassLen] = '\0';

    TempNewSymbol name = SymbolTable::new_symbol(declaringClassBinary, CHECK_WARN_ABORT_("Declaring class could not be found"));
    Klass* klass = GraalRuntime::resolve_or_null(name, CHECK_WARN_ABORT_("Declaring class could not be resolved"));

    if (klass == NULL) {
      warning("Declaring class for option %s could not be resolved", declaringClass);
      abort();
      return;
    }

    // The class has been loaded so the field and signature should already be in the symbol
    // table.  If they're not there, the field doesn't exist.
    TempNewSymbol fieldname = SymbolTable::probe(fieldName, (int)strlen(fieldName));
    TempNewSymbol signame = SymbolTable::probe(fieldSig, (int)fieldSigLen);
    if (fieldname == NULL || signame == NULL) {
      warning("Symbols for field for option %s not found (in %s)", fieldName, declaringClass);
      abort();
      return;
    }
    // Make sure class is initialized before handing id's out to fields
    klass->initialize(CHECK_WARN_ABORT_("Error while initializing declaring class for option"));

    fieldDescriptor fd;
    if (!InstanceKlass::cast(klass)->find_field(fieldname, signame, true, &fd)) {
      warning("Field for option %s not found (in %s)", fieldName, declaringClass);
      abort();
      return;
    }
    oop value;
    switch(optionValue->desc.type) {
    case _boolean: {
      jvalue jv;
      jv.z = optionValue->boolean_value;
      value = java_lang_boxing_object::create(T_BOOLEAN, &jv, THREAD);
      break;
    }
    case _int: {
      jvalue jv;
      jv.i = optionValue->int_value;
      value = java_lang_boxing_object::create(T_INT, &jv, THREAD);
      break;
    }
    case _long: {
      jvalue jv;
      jv.j = optionValue->long_value;
      value = java_lang_boxing_object::create(T_LONG, &jv, THREAD);
      break;
    }
    case _float: {
      jvalue jv;
      jv.f = optionValue->float_value;
      value = java_lang_boxing_object::create(T_FLOAT, &jv, THREAD);
      break;
    }
    case _double: {
      jvalue jv;
      jv.d = optionValue->double_value;
      value = java_lang_boxing_object::create(T_DOUBLE, &jv, THREAD);
      break;
    }
    case _string:
      value = java_lang_String::create_from_str(optionValue->string_value, THREAD)();
      break;
    default:
      ShouldNotReachHere();
    }

    oop optionValueOop = klass->java_mirror()->obj_field(fd.offset());

    if (optionValueOop == NULL) {
      warning("Option field was null, can not set %s", fieldName);
      abort();
      return;
    }

    if (!InstanceKlass::cast(optionValueOop->klass())->find_field(vmSymbols::value_name(), vmSymbols::object_signature(), false, &fd)) {
      warning("'Object value' field not found in option class %s, can not set option %s", fieldClass, fieldName);
      abort();
      return;
    }

    optionValueOop->obj_field_put(fd.offset(), value);
  }
};

void GraalRuntime::set_options(OptionsValueTable* options, TRAPS) {
  ensure_graal_class_loader_is_initialized();
  {
    ResourceMark rm;
    SetOptionClosure closure(THREAD);
    options->for_each(&closure);
    if (closure.is_aborted()) {
      vm_abort(false);
    }
  }
  OptionValue* printFlags = options->get(PRINT_FLAGS_ARG);
  if (printFlags != NULL && printFlags->boolean_value) {
    print_flags_helper(CHECK_ABORT);
  }
}

void GraalRuntime::print_flags_helper(TRAPS) {
  // TODO(gd) write this in C++?
  HandleMark hm(THREAD);
  TempNewSymbol name = SymbolTable::new_symbol("com/oracle/graal/hotspot/HotSpotOptions", CHECK_ABORT);
  KlassHandle hotSpotOptionsClass = load_required_class(name);
  TempNewSymbol setOption = SymbolTable::new_symbol("printFlags", CHECK);
  JavaValue result(T_VOID);
  JavaCallArguments args;
  JavaCalls::call_static(&result, hotSpotOptionsClass, setOption, vmSymbols::void_method_signature(), &args, CHECK);
}

Handle GraalRuntime::create_Service(const char* name, TRAPS) {
  TempNewSymbol kname = SymbolTable::new_symbol(name, CHECK_NH);
  Klass* k = resolve_or_fail(kname, CHECK_NH);
  instanceKlassHandle klass(THREAD, k);
  klass->initialize(CHECK_NH);
  klass->check_valid_for_instantiation(true, CHECK_NH);
  JavaValue result(T_VOID);
  instanceHandle service = klass->allocate_instance_handle(CHECK_NH);
  JavaCalls::call_special(&result, service, klass, vmSymbols::object_initializer_name(), vmSymbols::void_method_signature(), THREAD);
  return service;
}

void GraalRuntime::shutdown() {
  if (_HotSpotGraalRuntime_instance != NULL) {
    _shutdown_called = true;
    JavaThread* THREAD = JavaThread::current();
    HandleMark hm(THREAD);
    TempNewSymbol name = SymbolTable::new_symbol("com/oracle/graal/hotspot/HotSpotGraalRuntime", CHECK_ABORT);
    KlassHandle klass = load_required_class(name);
    JavaValue result(T_VOID);
    JavaCallArguments args;
    args.push_oop(get_HotSpotGraalRuntime());
    JavaCalls::call_special(&result, klass, vmSymbols::shutdown_method_name(), vmSymbols::void_method_signature(), &args, CHECK_ABORT);

    JNIHandles::destroy_global(_HotSpotGraalRuntime_instance);
    _HotSpotGraalRuntime_instance = NULL;
  }
}

void GraalRuntime::call_printStackTrace(Handle exception, Thread* thread) {
  assert(exception->is_a(SystemDictionary::Throwable_klass()), "Throwable instance expected");
  JavaValue result(T_VOID);
  JavaCalls::call_virtual(&result,
                          exception,
                          KlassHandle(thread,
                          SystemDictionary::Throwable_klass()),
                          vmSymbols::printStackTrace_name(),
                          vmSymbols::void_method_signature(),
                          thread);
}

void GraalRuntime::abort_on_pending_exception(Handle exception, const char* message, bool dump_core) {
  Thread* THREAD = Thread::current();
  CLEAR_PENDING_EXCEPTION;
  tty->print_raw_cr(message);
  call_printStackTrace(exception, THREAD);

  // Give other aborting threads to also print their stack traces.
  // This can be very useful when debugging class initialization
  // failures.
  os::sleep(THREAD, 200, false);

  vm_abort(dump_core);
}

Klass* GraalRuntime::resolve_or_null(Symbol* name, TRAPS) {
  return SystemDictionary::resolve_or_null(name, SystemDictionary::graal_loader(), Handle(), CHECK_NULL);
}

Klass* GraalRuntime::resolve_or_fail(Symbol* name, TRAPS) {
  return SystemDictionary::resolve_or_fail(name, SystemDictionary::graal_loader(), Handle(), true, CHECK_NULL);
}

Klass* GraalRuntime::load_required_class(Symbol* name) {
  Klass* klass = resolve_or_null(name, Thread::current());
  if (klass == NULL) {
    tty->print_cr("Could not load class %s", name->as_C_string());
    vm_abort(false);
  }
  return klass;
}

void GraalRuntime::parse_lines(char* path, ParseClosure* closure, bool warnStatFailure) {
  struct stat st;
  if (os::stat(path, &st) == 0 && (st.st_mode & S_IFREG) == S_IFREG) { // exists & is regular file
    int file_handle = os::open(path, 0, 0);
    if (file_handle != -1) {
      char* buffer = NEW_C_HEAP_ARRAY(char, st.st_size + 1, mtInternal);
      int num_read = (int) os::read(file_handle, (char*) buffer, st.st_size);
      if (num_read == -1) {
        warning("Error reading file %s due to %s", path, strerror(errno));
      } else if (num_read != st.st_size) {
        warning("Only read %d of " SIZE_FORMAT " bytes from %s", num_read, (size_t) st.st_size, path);
      }
      os::close(file_handle);
      closure->set_filename(path);
      if (num_read == st.st_size) {
        buffer[num_read] = '\0';

        char* line = buffer;
        while (line - buffer < num_read && !closure->is_aborted()) {
          // find line end (\r, \n or \r\n)
          char* nextline = NULL;
          char* cr = strchr(line, '\r');
          char* lf = strchr(line, '\n');
          if (cr != NULL && lf != NULL) {
            char* min = MIN2(cr, lf);
            *min = '\0';
            if (lf == cr + 1) {
              nextline = lf + 1;
            } else {
              nextline = min + 1;
            }
          } else if (cr != NULL) {
            *cr = '\0';
            nextline = cr + 1;
          } else if (lf != NULL) {
            *lf = '\0';
            nextline = lf + 1;
          }
          // trim left
          while (*line == ' ' || *line == '\t') line++;
          char* end = line + strlen(line);
          // trim right
          while (end > line && (*(end -1) == ' ' || *(end -1) == '\t')) end--;
          *end = '\0';
          // skip comments and empty lines
          if (*line != '#' && strlen(line) > 0) {
            closure->parse_line(line);
          }
          if (nextline != NULL) {
            line = nextline;
          } else {
            // File without newline at the end
            break;
          }
        }
      }
      FREE_C_HEAP_ARRAY(char, buffer, mtInternal);
    } else {
      warning("Error opening file %s due to %s", path, strerror(errno));
    }
  } else if (warnStatFailure) {
    warning("Could not stat file %s due to %s", path, strerror(errno));
  }
}

class ServiceParseClosure : public ParseClosure {
  GrowableArray<char*> _implNames;
public:
  ServiceParseClosure() : _implNames() {}
  void do_line(char* line) {
    size_t lineLen = strlen(line);
    char* implName = NEW_C_HEAP_ARRAY(char, lineLen + 1, mtCompiler); // TODO (gd) i'm leaking
    // Turn all '.'s into '/'s
    for (size_t index = 0; index < lineLen; ++index) {
      if (line[index] == '.') {
        implName[index] = '/';
      } else {
        implName[index] = line[index];
      }
    }
    implName[lineLen] = '\0';
    _implNames.append(implName);
  }
  GrowableArray<char*>* implNames() {return &_implNames;}
};


Handle GraalRuntime::get_service_impls(KlassHandle serviceKlass, TRAPS) {
  const char* home = Arguments::get_java_home();
  const char* serviceName = serviceKlass->external_name();
  size_t path_len = strlen(home) + strlen("/lib/graal/services/") + strlen(serviceName) + 1;
  char* path = NEW_RESOURCE_ARRAY_IN_THREAD(THREAD, char, path_len);
  char sep = os::file_separator()[0];
  sprintf(path, "%s%clib%cgraal%cservices%c%s", home, sep, sep, sep, sep, serviceName);
  ServiceParseClosure closure;
  parse_lines(path, &closure, true); // TODO(gd) cache parsing results?

  GrowableArray<char*>* implNames = closure.implNames();
  objArrayOop servicesOop = oopFactory::new_objArray(serviceKlass(), implNames->length(), CHECK_NH);
  objArrayHandle services(THREAD, servicesOop);
  for (int i = 0; i < implNames->length(); ++i) {
    char* implName = implNames->at(i);
    Handle service = create_Service(implName, CHECK_NH);
    services->obj_at_put(i, service());
  }
  return services;
}
