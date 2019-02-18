/*
 * Copyright (c) 2012, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_JVMCI_JVMCI_RUNTIME_HPP
#define SHARE_VM_JVMCI_JVMCI_RUNTIME_HPP

#include "interpreter/interpreter.hpp"
#include "jvmci/jvmci.hpp"
#include "jvmci/jvmciExceptions.hpp"
#include "jvmci/jvmciJavaClasses.hpp"
#include "memory/allocation.hpp"
#include "runtime/arguments.hpp"
#include "runtime/deoptimization.hpp"

class JVMCIObject;
class JVMCIEnv;
class JVMCICompileState;

// Encapsulates the JVMCI metadata associated with an nmethod.
class JVMCINMethodData: public CHeapObj<mtCompiler> {
  // Value of HotSpotNmethod.name converted to a C string.
  const char* _nmethod_mirror_name;

  // Weak reference to the HotSpotNmethod mirror in the HotSpot heap.
  JVMCIObject _nmethod_mirror;

  // Address of the failed speculations list potentially appended
  // to when deoptimizing the nmethod.
  FailedSpeculation** _failed_speculations;

  // Determines whether the associated nmethod is invalidated when the
  // referent in _nmethod_mirror is cleared.  This will be false if
  // the referent is initialized to a HotSpotNmethod object whose
  // isDefault field is true.  That is, a mirror other than a
  // "default" HotSpotNmethod causes nmethod invalidation.  See
  // HotSpotNmethod.isDefault for more detail.
  bool _triggers_invalidation;

  // Used to maintain the linked list held by the _for_release field
  JVMCINMethodData* _next;

  // Maintains a list of JVMCINMethodDatas that require cleanup on the
  // next call to installCode. This field must be updated under
  // the JVMCI_lock.
  static JVMCINMethodData* volatile _for_release;

 public:
  JVMCINMethodData(JVMCIEnv* jvmciEnv, JVMCIObject nmethod_mirror, bool triggers_invalidation, FailedSpeculation** failed_speculations);

  // Releases all resources held by this object.
  ~JVMCINMethodData();

  // Adds `speculation` to the failed speculations list.
  void add_failed_speculation(nmethod* nm, jlong speculation);

  // Release the data object or queue it for lazy cleanup.
  // If the data object contains non-null references to objects
  // in the shared library heap, cleanup is deferred since
  // processing these references can block on the ThreadToNativeFromVM
  // transition calling into the shared library.
  static void release(JVMCINMethodData* data);

  // Release any instances which require lazy cleanup.
  static void cleanup();

  // Gets the value of HotSpotNmethod.name converted to a C string (which may be NULL).
  const char* nmethod_mirror_name() { return _nmethod_mirror_name; }

  // Clears the `address` field in the HotSpotNmethod mirror. If the nmethod
  // is no longer alive, the `entryPoint` field is also cleared and the weak
  // reference to the mirror is released (e.g., JNI DeleteWeakGlobalRef).
  void invalidate_mirror(nmethod* nm);

  // Process the HotSpotNmethod mirror during the nmethod unloading
  // phase of a HotSpot GC. If the weak reference to the mirror is
  // null and _triggers_invalidation is true, then the nmethod is made non-entrant.
  void update_nmethod_mirror_in_gc(nmethod* nm, BoolObjectClosure* is_alive);

  // Gets the HotSpotNmethod mirror in the HotSpot heap
  JVMCIObject get_nmethod_mirror();

  // Adds a HotSpotNmethod mirror.
  void add_nmethod_mirror(JVMCIEnv* jvmciEnv, JVMCIObject mirror, JVMCI_TRAPS);

  // Deletes the weak reference (if any) to the HotSpotNmethod object
  // associated with this nmethod.
  void clear_nmethod_mirror();
};


// A top level class that represents an initialized JVMCI runtime.
// There is one instance of this class per HotSpotJVMCIRuntime object.
class JVMCIRuntime: public CHeapObj<mtCompiler> {
 public:
  // Constants describing whether JVMCI wants to be able to adjust the compilation
  // level selected for a method by the VM compilation policy and if so, based on
  // what information about the method being schedule for compilation.
  enum CompLevelAdjustment {
     none = 0,             // no adjustment
     by_holder = 1,        // adjust based on declaring class of method
     by_full_signature = 2 // adjust based on declaring class, name and signature of method
  };

 private:
  volatile bool _being_initialized;
  volatile bool _initialized;

  JVMCIObject _HotSpotJVMCIRuntime_instance;

  CompLevelAdjustment _comp_level_adjustment;

  bool _shutdown_called;

  CompLevel adjust_comp_level_inner(methodHandle method, bool is_osr, CompLevel level, JavaThread* thread);

  JVMCIObject create_jvmci_primitive_type(BasicType type, JVMCI_TRAPS);

  // Implementation methods for loading and constant pool access.
  static Klass* get_klass_by_name_impl(Klass*& accessing_klass,
                                       const constantPoolHandle& cpool,
                                       Symbol* klass_name,
                                       bool require_local);
  static Klass*   get_klass_by_index_impl(const constantPoolHandle& cpool,
                                          int klass_index,
                                          bool& is_accessible,
                                          Klass* loading_klass);
  static void   get_field_by_index_impl(InstanceKlass* loading_klass, fieldDescriptor& fd,
                                        int field_index);
  static methodHandle  get_method_by_index_impl(const constantPoolHandle& cpool,
                                                int method_index, Bytecodes::Code bc,
                                                InstanceKlass* loading_klass);

  // Helper methods
  static bool       check_klass_accessibility(Klass* accessing_klass, Klass* resolved_klass);
  static methodHandle  lookup_method(InstanceKlass*  accessor,
                                     InstanceKlass*  holder,
                                     Symbol*         name,
                                     Symbol*         sig,
                                     Bytecodes::Code bc);

 public:
  JVMCIRuntime() {
    _comp_level_adjustment = JVMCIRuntime::none;
    _initialized = false;
    _being_initialized = false;
    _shutdown_called = false;
  }

  /**
   * Compute offsets and construct any state required before executing JVMCI code.
   */
  void initialize(JVMCIEnv* jvmciEnv);

  /**
   * Ensures that the JVMCI class loader is initialized and the well known JVMCI classes are loaded.
   */
  void ensure_jvmci_class_loader_is_initialized(JVMCIEnv* jvmciEnv);

  /**
   * Gets the singleton HotSpotJVMCIRuntime instance, initializing it if necessary
   */
  JVMCIObject get_HotSpotJVMCIRuntime(JVMCI_TRAPS);

  bool is_HotSpotJVMCIRuntime_initialized() {
    return _HotSpotJVMCIRuntime_instance.is_non_null();
  }

  /**
   * Trigger initialization of HotSpotJVMCIRuntime through JVMCI.getRuntime()
   */
  void initialize_JVMCI(JVMCI_TRAPS);

  /**
   * Explicitly initialize HotSpotJVMCIRuntime itself
   */
  void initialize_HotSpotJVMCIRuntime(JVMCI_TRAPS);

  void call_getCompiler(TRAPS);

  /**
   * Lets JVMCI modify the compilation level currently selected for a method by
   * the VM compilation policy.
   *
   * @param method the method being scheduled for compilation
   * @param is_osr specifies if the compilation is an OSR compilation
   * @param level the compilation level currently selected by the VM compilation policy
   * @param thread the current thread
   * @return the compilation level to use for the compilation
   */
  CompLevel adjust_comp_level(methodHandle method, bool is_osr, CompLevel level, JavaThread* thread);

  void shutdown();

  bool shutdown_called() {
    return _shutdown_called;
  }

  void bootstrap_finished(TRAPS);

  // Look up a klass by name from a particular class loader (the accessor's).
  // If require_local, result must be defined in that class loader, or NULL.
  // If !require_local, a result from remote class loader may be reported,
  // if sufficient class loader constraints exist such that initiating
  // a class loading request from the given loader is bound to return
  // the class defined in the remote loader (or throw an error).
  //
  // Return an unloaded klass if !require_local and no class at all is found.
  //
  // The CI treats a klass as loaded if it is consistently defined in
  // another loader, even if it hasn't yet been loaded in all loaders
  // that could potentially see it via delegation.
  static Klass* get_klass_by_name(Klass* accessing_klass,
                                  Symbol* klass_name,
                                  bool require_local);

  // Constant pool access.
  static Klass*   get_klass_by_index(const constantPoolHandle& cpool,
                                     int klass_index,
                                     bool& is_accessible,
                                     Klass* loading_klass);
  static void   get_field_by_index(InstanceKlass* loading_klass, fieldDescriptor& fd,
                                   int field_index);
  static methodHandle  get_method_by_index(const constantPoolHandle& cpool,
                                           int method_index, Bytecodes::Code bc,
                                           InstanceKlass* loading_klass);

  // converts the Klass* representing the holder of a method into a
  // InstanceKlass*.  This is needed since the holder of a method in
  // the bytecodes could be an array type.  Basically this converts
  // array types into java/lang/Object and other types stay as they are.
  static InstanceKlass* get_instance_klass_for_declared_method_holder(Klass* klass);

  // Helper routine for determining the validity of a compilation
  // with respect to concurrent class loading.
  static JVMCI::CodeInstallResult validate_compile_task_dependencies(Dependencies* target, JVMCICompileState* task, char** failure_detail);

  // Compiles `target` with the JVMCI compiler.
  void compile_method(JVMCIEnv* JVMCIENV, JVMCICompiler* compiler, const methodHandle& target, int entry_bci);

  // Register the result of a compilation.
  JVMCI::CodeInstallResult register_method(JVMCIEnv* JVMCIENV,
                       const methodHandle&       target,
                       nmethod*&                 nm,
                       int                       entry_bci,
                       CodeOffsets*              offsets,
                       int                       orig_pc_offset,
                       CodeBuffer*               code_buffer,
                       int                       frame_words,
                       OopMapSet*                oop_map_set,
                       ExceptionHandlerTable*    handler_table,
                       ImplicitExceptionTable*   implicit_exception_table,
                       AbstractCompiler*         compiler,
                       DebugInformationRecorder* debug_info,
                       Dependencies*             dependencies,
                       int                       compile_id,
                       bool                      has_unsafe_access,
                       bool                      has_wide_vector,
                       JVMCIObject               compiled_code,
                       JVMCIObject               nmethod_mirror,
                       FailedSpeculation**       failed_speculations,
                       char*                     speculations,
                       int                       speculations_len);

  /**
   * Exits the VM due to an unexpected exception.
   */
  static void exit_on_pending_exception(JVMCIEnv* JVMCIENV, const char* message);

  static void describe_pending_hotspot_exception(JavaThread* THREAD, bool clear);

#define CHECK_EXIT THREAD); \
  if (HAS_PENDING_EXCEPTION) { \
    char buf[256]; \
    jio_snprintf(buf, 256, "Uncaught exception at %s:%d", __FILE__, __LINE__); \
    JVMCIRuntime::exit_on_pending_exception(NULL, buf); \
    return; \
  } \
  (void)(0

#define CHECK_EXIT_(v) THREAD);                 \
  if (HAS_PENDING_EXCEPTION) { \
    char buf[256]; \
    jio_snprintf(buf, 256, "Uncaught exception at %s:%d", __FILE__, __LINE__); \
    JVMCIRuntime::exit_on_pending_exception(NULL, buf); \
    return v; \
  } \
  (void)(0

#define JVMCI_CHECK_EXIT JVMCIENV); \
  if (JVMCIENV->has_pending_exception()) {      \
    char buf[256]; \
    jio_snprintf(buf, 256, "Uncaught exception at %s:%d", __FILE__, __LINE__); \
    JVMCIRuntime::exit_on_pending_exception(JVMCIENV, buf); \
    return; \
  } \
  (void)(0

#define JVMCI_CHECK_EXIT_(result) JVMCIENV); \
  if (JVMCIENV->has_pending_exception()) {      \
    char buf[256]; \
    jio_snprintf(buf, 256, "Uncaught exception at %s:%d", __FILE__, __LINE__); \
    JVMCIRuntime::exit_on_pending_exception(JVMCIENV, buf); \
    return result; \
  } \
  (void)(0

  /**
   * Same as SystemDictionary::resolve_or_null but uses the JVMCI loader.
   */
  static Klass* resolve_or_null(Symbol* name, TRAPS);

  /**
   * Same as SystemDictionary::resolve_or_fail but uses the JVMCI loader.
   */
  static Klass* resolve_or_fail(Symbol* name, TRAPS);

  static BasicType kindToBasicType(Handle kind, TRAPS);

  static void new_instance_common(JavaThread* thread, Klass* klass, bool null_on_fail);
  static void new_array_common(JavaThread* thread, Klass* klass, jint length, bool null_on_fail);
  static void new_multi_array_common(JavaThread* thread, Klass* klass, int rank, jint* dims, bool null_on_fail);
  static void dynamic_new_array_common(JavaThread* thread, oopDesc* element_mirror, jint length, bool null_on_fail);
  static void dynamic_new_instance_common(JavaThread* thread, oopDesc* type_mirror, bool null_on_fail);

  // The following routines are called from compiled JVMCI code

  // When allocation fails, these stubs:
  // 1. Exercise -XX:+HeapDumpOnOutOfMemoryError and -XX:OnOutOfMemoryError handling and also
  //    post a JVMTI_EVENT_RESOURCE_EXHAUSTED event if the failure is an OutOfMemroyError
  // 2. Return NULL with a pending exception.
  // Compiled code must ensure these stubs are not called twice for the same allocation
  // site due to the non-repeatable side effects in the case of OOME.
  static void new_instance(JavaThread* thread, Klass* klass) { new_instance_common(thread, klass, false); }
  static void new_array(JavaThread* thread, Klass* klass, jint length) { new_array_common(thread, klass, length, false); }
  static void new_multi_array(JavaThread* thread, Klass* klass, int rank, jint* dims) { new_multi_array_common(thread, klass, rank, dims, false); }
  static void dynamic_new_array(JavaThread* thread, oopDesc* element_mirror, jint length) { dynamic_new_array_common(thread, element_mirror, length, false); }
  static void dynamic_new_instance(JavaThread* thread, oopDesc* type_mirror) { dynamic_new_instance_common(thread, type_mirror, false); }

  // When allocation fails, these stubs return NULL and have no pending exception. Compiled code
  // can use these stubs if a failed allocation will be retried (e.g., by deoptimizing and
  // re-executing in the interpreter).
  static void new_instance_or_null(JavaThread* thread, Klass* klass) { new_instance_common(thread, klass, true); }
  static void new_array_or_null(JavaThread* thread, Klass* klass, jint length) { new_array_common(thread, klass, length, true); }
  static void new_multi_array_or_null(JavaThread* thread, Klass* klass, int rank, jint* dims) { new_multi_array_common(thread, klass, rank, dims, true); }
  static void dynamic_new_array_or_null(JavaThread* thread, oopDesc* element_mirror, jint length) { dynamic_new_array_common(thread, element_mirror, length, true); }
  static void dynamic_new_instance_or_null(JavaThread* thread, oopDesc* type_mirror) { dynamic_new_instance_common(thread, type_mirror, true); }

  static jboolean thread_is_interrupted(JavaThread* thread, oopDesc* obj, jboolean clear_interrupted);
  static void vm_message(jboolean vmError, jlong format, jlong v1, jlong v2, jlong v3);
  static jint identity_hash_code(JavaThread* thread, oopDesc* obj);
  static address exception_handler_for_pc(JavaThread* thread);
  static void monitorenter(JavaThread* thread, oopDesc* obj, BasicLock* lock);
  static void monitorexit (JavaThread* thread, oopDesc* obj, BasicLock* lock);
  static void vm_error(JavaThread* thread, jlong where, jlong format, jlong value);
  static oopDesc* load_and_clear_exception(JavaThread* thread);
  static void log_printf(JavaThread* thread, const char* format, jlong v1, jlong v2, jlong v3);
  static void log_primitive(JavaThread* thread, jchar typeChar, jlong value, jboolean newline);
  // Print the passed in object, optionally followed by a newline.  If
  // as_string is true and the object is a java.lang.String then it
  // printed as a string, otherwise the type of the object is printed
  // followed by its address.
  static void log_object(JavaThread* thread, oopDesc* object, bool as_string, bool newline);
  static void write_barrier_pre(JavaThread* thread, oopDesc* obj);
  static void write_barrier_post(JavaThread* thread, void* card);
  static jboolean validate_object(JavaThread* thread, oopDesc* parent, oopDesc* child);
  static void new_store_pre_barrier(JavaThread* thread);

  // used to throw exceptions from compiled JVMCI code
  static void throw_and_post_jvmti_exception(JavaThread* thread, const char* exception, const char* message);
  // helper methods to throw exception with complex messages
  static void throw_klass_external_name_exception(JavaThread* thread, const char* exception, Klass* klass);
  static void throw_class_cast_exception(JavaThread* thread, const char* exception, Klass* caster_klass, Klass* target_klass);

  // Test only function
  static int test_deoptimize_call_int(JavaThread* thread, int value);
};

// Tracing macros.

#define IF_TRACE_jvmci_1 if (!(JVMCITraceLevel >= 1)) ; else
#define IF_TRACE_jvmci_2 if (!(JVMCITraceLevel >= 2)) ; else
#define IF_TRACE_jvmci_3 if (!(JVMCITraceLevel >= 3)) ; else
#define IF_TRACE_jvmci_4 if (!(JVMCITraceLevel >= 4)) ; else
#define IF_TRACE_jvmci_5 if (!(JVMCITraceLevel >= 5)) ; else

#define TRACE_jvmci_1 if (!(JVMCITraceLevel >= 1 && (tty->print(PTR_FORMAT " JVMCITrace-1: ", p2i(JavaThread::current())), true))) ; else tty->print_cr
#define TRACE_jvmci_2 if (!(JVMCITraceLevel >= 2 && (tty->print(PTR_FORMAT "    JVMCITrace-2: ", p2i(JavaThread::current())), true))) ; else tty->print_cr
#define TRACE_jvmci_3 if (!(JVMCITraceLevel >= 3 && (tty->print(PTR_FORMAT "       JVMCITrace-3: ", p2i(JavaThread::current())), true))) ; else tty->print_cr
#define TRACE_jvmci_4 if (!(JVMCITraceLevel >= 4 && (tty->print(PTR_FORMAT "          JVMCITrace-4: ", p2i(JavaThread::current())), true))) ; else tty->print_cr
#define TRACE_jvmci_5 if (!(JVMCITraceLevel >= 5 && (tty->print(PTR_FORMAT "             JVMCITrace-5: ", p2i(JavaThread::current())), true))) ; else tty->print_cr

#endif // SHARE_VM_JVMCI_JVMCI_RUNTIME_HPP
