/*
 * Copyright (c) 1999, 2018, Oracle and/or its affiliates. All rights reserved.
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

#include "precompiled.hpp"
#include "jvmci/jvmciEnv.hpp"
#include "classfile/javaAssertions.hpp"
#include "classfile/systemDictionary.hpp"
#include "code/codeCache.hpp"
#include "code/scopeDesc.hpp"
#include "compiler/compileBroker.hpp"
#include "compiler/compileLog.hpp"
#include "compiler/compilerOracle.hpp"
#ifdef INCLUDE_ALL_GCS
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#endif
#include "interpreter/linkResolver.hpp"
#include "memory/allocation.inline.hpp"
#include "memory/oopFactory.hpp"
#include "memory/universe.inline.hpp"
#include "oops/methodData.hpp"
#include "oops/objArrayKlass.hpp"
#include "oops/oop.inline.hpp"
#include "prims/jvmtiExport.hpp"
#include "runtime/init.hpp"
#include "runtime/reflection.hpp"
#include "runtime/sharedRuntime.hpp"
#include "runtime/javaCalls.hpp"
#include "utilities/dtrace.hpp"
#include "jvmci/jvmciRuntime.hpp"
#include "jvmci/jvmciJavaClasses.hpp"

#include <string.h>

JVMCICompileState::JVMCICompileState(CompileTask* task, int system_dictionary_modification_counter):
  _task(task),
  _system_dictionary_modification_counter(system_dictionary_modification_counter),
  _failure_reason(NULL),
  _failure_reason_on_C_heap(false),
  _retryable(true) {
  // Get Jvmti capabilities under lock to get consistent values.
  MutexLocker mu(JvmtiThreadState_lock);
  _jvmti_can_hotswap_or_post_breakpoint = JvmtiExport::can_hotswap_or_post_breakpoint() ? 1 : 0;
  _jvmti_can_access_local_variables     = JvmtiExport::can_access_local_variables() ? 1 : 0;
  _jvmti_can_post_on_exceptions         = JvmtiExport::can_post_on_exceptions() ? 1 : 0;
  _jvmti_can_pop_frame                  = JvmtiExport::can_pop_frame() ? 1 : 0;
}

bool JVMCICompileState::jvmti_state_changed() const {
  if (!jvmti_can_access_local_variables() &&
      JvmtiExport::can_access_local_variables()) {
    return true;
  }
  if (!jvmti_can_hotswap_or_post_breakpoint() &&
      JvmtiExport::can_hotswap_or_post_breakpoint()) {
    return true;
  }
  if (!jvmti_can_post_on_exceptions() &&
      JvmtiExport::can_post_on_exceptions()) {
    return true;
  }
  if (!jvmti_can_pop_frame() &&
      JvmtiExport::can_pop_frame()) {
    return true;
  }
  return false;
}

JavaVM* JVMCIEnv::_shared_library_javavm = NULL;
void* JVMCIEnv::_shared_library_handle = NULL;
char* JVMCIEnv::_shared_library_path = NULL;

static void init_shared_library_options(JavaVMInitArgs& vm_args) {
  int n_options = 0;
  int args_len = 0;
  const size_t len = JVMCILibArgs != NULL ? strlen(JVMCILibArgs) : 0;
  char sep = JVMCILibArgsSep[0];
  const char* p = JVMCILibArgs;
  const char* end = JVMCILibArgs + len;
  while (p < end) {
    if (*p != sep) {
      args_len++;
      if (p == JVMCILibArgs || *(p - 1) == sep) {
        n_options++;
      }
    }
    p++;
  }

  JavaVMOption* options = NEW_RESOURCE_ARRAY(JavaVMOption, n_options);
  int option_index = 0;

  if (args_len != 0) {
    int i = 0;
    char* args = NEW_RESOURCE_ARRAY(char, args_len);
    char* a = args;
    int args_index = 0;
    p = JVMCILibArgs;
    while (p < end) {
      // Skip separator chars
      while (*p == sep) {
        p++;
      }

      char* arg = a;
      while (*p != sep && *p != '\0') {
        *a++ = *p++;
      }
      p++;
      if (arg != a) {
        *a++ = '\0';
        options[option_index++].optionString = arg;
      }
    }
  }
  assert(option_index == n_options, "must be");
  vm_args.options = options;
  vm_args.nOptions = n_options;
}

JNIEnv* JVMCIEnv::attach_shared_library() {
  if (_shared_library_javavm == NULL) {
    MutexLocker locker(JVMCI_lock);
    if (_shared_library_javavm == NULL) {
      char path[JVM_MAXPATHLEN];
      char ebuf[1024];
      if (JVMCILibPath != NULL) {
        if (!os::dll_build_name(path, sizeof(path), JVMCILibPath, JVMCI_SHARED_LIBRARY_NAME)) {
          vm_exit_during_initialization("Unable to create JVMCI shared library path from -XX:JVMCILibPath value", JVMCILibPath);
        }
      } else {
        if (!os::dll_build_name(path, sizeof(path), Arguments::get_dll_dir(), JVMCI_SHARED_LIBRARY_NAME)) {
          vm_exit_during_initialization("Unable to create path to JVMCI shared library");
        }
      }

      void* handle = os::dll_load(path, ebuf, sizeof ebuf);
      if (handle == NULL) {
        vm_exit_during_initialization("Unable to load JVMCI shared library", ebuf);
      }
      _shared_library_handle = handle;
      _shared_library_path = strdup(path);
      jint (*JNI_CreateJavaVM)(JavaVM **pvm, void **penv, void *args);
      typedef jint (*JNI_CreateJavaVM_t)(JavaVM **pvm, void **penv, void *args);

      JNI_CreateJavaVM = CAST_TO_FN_PTR(JNI_CreateJavaVM_t, os::dll_lookup(handle, "JNI_CreateJavaVM"));
      JNIEnv* env;
      if (JNI_CreateJavaVM == NULL) {
        vm_exit_during_initialization("Unable to find JNI_CreateJavaVM", path);
      }

      ResourceMark rm;
      JavaVMInitArgs vm_args;
      vm_args.version = JNI_VERSION_1_2;
      vm_args.ignoreUnrecognized = JNI_TRUE;
      init_shared_library_options(vm_args);

      JavaVM* the_javavm = NULL;
      int result = (*JNI_CreateJavaVM)(&the_javavm, (void**) &env, &vm_args);
      if (result == JNI_OK) {
        guarantee(env != NULL, "missing env");
        _shared_library_javavm = the_javavm;
        return env;
      } else {
        vm_exit_during_initialization(err_msg("JNI_CreateJavaVM failed with return value %d", result), path);
      }
    }
  }
  JNIEnv* env;
  if (_shared_library_javavm->AttachCurrentThread((void**)&env, NULL) == JNI_OK) {
    guarantee(env != NULL, "missing env");
    return env;
  }
  fatal("Error attaching current thread to JVMCI shared library JNI interface");
  return NULL;
}

void JVMCIEnv::init_env_mode_runtime(JNIEnv* parent_env) {
  // By default there is only one runtime which is the compiler runtime.
  _runtime = JVMCI::compiler_runtime();
  if (JVMCIGlobals::java_mode() == JVMCIGlobals::HotSpot) {
    // In HotSpot mode, JNI isn't used at all.
    _mode = JVMCIGlobals::HotSpot;
    _env = NULL;
    return;
  }

  if (parent_env != NULL) {
    // If the parent JNI environment is non-null then figure out whether it
    // is a HotSpot or shared library JNIEnv and set the state appropriately.
    JavaThread* thread = JavaThread::current();
    if (thread->jni_environment() == parent_env) {
      // Select the Java runtime
      _runtime = JVMCI::java_runtime();
      _mode = JVMCIGlobals::HotSpot;
      _env = NULL;
      return;
    }
  }

  // Running in JVMCI shared library mode so get a shared library JNIEnv
  _mode = JVMCIGlobals::SharedLibrary;
  _env = attach_shared_library();
  assert(parent_env == NULL || _env == parent_env, "must be");

  if (parent_env == NULL) {
    // There is no parent shared library JNI env so push
    // a JNI local frame to release all local handles in
    // this JVMCIEnv scope when it's closed.
    assert(_throw_to_caller == false, "must be");
    JNIAccessMark jni(this);
    jint result = _env->PushLocalFrame(32);
    if (result != JNI_OK) {
      char message[256];
      jio_snprintf(message, 256, "Uncaught exception pushing local frame for JVMCIEnv scope entered at %s:%d", _file, _line);
      JVMCIRuntime::exit_on_pending_exception(this, message);
    }
  }
}

JVMCIEnv::JVMCIEnv(JVMCICompileState* compile_state, const char* file, int line):
    _throw_to_caller(false), _file(file), _line(line), _compile_state(compile_state) {
  init_env_mode_runtime(NULL);
}

JVMCIEnv::JVMCIEnv(JavaThread* thread, const char* file, int line):
    _throw_to_caller(false), _file(file), _line(line), _compile_state(NULL) {
  init_env_mode_runtime(NULL);
}

JVMCIEnv::JVMCIEnv(JNIEnv* parent_env, const char* file, int line):
    _throw_to_caller(true), _file(file), _line(line), _compile_state(NULL) {
  init_env_mode_runtime(parent_env);
  assert(_env == NULL || parent_env == _env, "mismatched JNIEnvironment");
}

void JVMCIEnv::init(bool is_hotspot, const char* file, int line) {
  _compile_state = NULL;
  _throw_to_caller = false;
  _file = file;
  _line = line;
  if (is_hotspot) {
    _env = NULL;
    _mode = JVMCIGlobals::HotSpot;
    _runtime = JVMCI::java_runtime();
  } else {
    init_env_mode_runtime(NULL);
  }
}

// Prints a pending exception (if any) and its stack trace.
void JVMCIEnv::describe_pending_exception(bool clear) {
  if (!is_hotspot()) {
    JNIAccessMark jni(this);
    if (jni()->ExceptionCheck()) {
      jthrowable ex = !clear ? jni()->ExceptionOccurred() : NULL;
      jni()->ExceptionDescribe();
      if (ex != NULL) {
        jni()->Throw(ex);
      }
    }
  } else {
    Thread* THREAD = Thread::current();
    if (HAS_PENDING_EXCEPTION) {
      JVMCIRuntime::describe_pending_hotspot_exception((JavaThread*) THREAD, clear);
    }
  }
}

void JVMCIEnv::translate_hotspot_exception_to_jni_exception(JavaThread* THREAD, Handle throwable) {
  assert(!is_hotspot(), "must_be");
  // Resolve HotSpotJVMCIRuntime class explicitly as HotSpotJVMCI::compute_offsets
  // may not have been called.
  Klass* runtimeKlass = SystemDictionary::resolve_or_fail(vmSymbols::jdk_vm_ci_hotspot_HotSpotJVMCIRuntime(),
      SystemDictionary::jvmci_loader(), Handle(), true, CHECK);
  JavaCallArguments jargs;
  jargs.push_oop(throwable());
  JavaValue result(T_OBJECT);
  JavaCalls::call_static(&result,
                          runtimeKlass,
                          vmSymbols::encodeThrowable_name(),
                          vmSymbols::encodeThrowable_signature(), &jargs, THREAD);
  if (HAS_PENDING_EXCEPTION) {
    JVMCIRuntime::exit_on_pending_exception(this, "HotSpotJVMCIRuntime.encodeThrowable should not throw an exception");
  }

  oop encoded_throwable_string = (oop) result.get_jobject();

  ResourceMark rm;
  const char* encoded_throwable_chars = java_lang_String::as_utf8_string(encoded_throwable_string);

  JNIAccessMark jni(this);
  jobject jni_encoded_throwable_string = jni()->NewStringUTF(encoded_throwable_chars);
  jthrowable jni_throwable = (jthrowable) jni()->CallStaticObjectMethod(JNIJVMCI::HotSpotJVMCIRuntime::clazz(),
                                JNIJVMCI::HotSpotJVMCIRuntime::decodeThrowable_method(),
                                jni_encoded_throwable_string);
  jni()->Throw(jni_throwable);
}

JVMCIEnv::~JVMCIEnv() {
  if (_throw_to_caller) {
    if (is_hotspot()) {
      // Nothing to do
    } else {
      if (Thread::current()->is_Java_thread()) {
        JavaThread* THREAD = JavaThread::current();
        if (HAS_PENDING_EXCEPTION) {
          Handle throwable = PENDING_EXCEPTION;
          CLEAR_PENDING_EXCEPTION;
          translate_hotspot_exception_to_jni_exception(THREAD, throwable);
        }
      }
    }
  } else {
    if (!is_hotspot()) {
      // Pop the JNI local frame that was pushed when entering this JVMCIEnv scope.
      JNIAccessMark jni(this);
      jni()->PopLocalFrame(NULL);
    }

    if (has_pending_exception()) {
      char message[256];
      jio_snprintf(message, 256, "Uncaught exception exiting JVMCIEnv scope entered at %s:%d", _file, _line);
      JVMCIRuntime::exit_on_pending_exception(this, message);
    }
  }
}

JVMCIObject JVMCIEnv::call_HotSpotJVMCIRuntime_compileMethod (JVMCIObject runtime, JVMCIObject method, int entry_bci,
                                                              jlong compile_state, int id) {
  if (is_hotspot()) {
    Thread* THREAD = Thread::current();
    JavaCallArguments jargs;
    jargs.push_oop(HotSpotJVMCI::resolve(runtime));
    jargs.push_oop(HotSpotJVMCI::resolve(method));
    jargs.push_int(entry_bci);
    jargs.push_long(compile_state);
    jargs.push_int(id);
    JavaValue result(T_OBJECT);
    JavaCalls::call_special(&result,
                            HotSpotJVMCI::HotSpotJVMCIRuntime::klass(),
                            vmSymbols::compileMethod_name(),
                            vmSymbols::compileMethod_signature(), &jargs, CHECK_(JVMCIObject()));
    return wrap((oop) result.get_jobject());
  } else {
    JNIAccessMark jni(this);
    jobject result = jni()->CallNonvirtualObjectMethod(runtime.as_jobject(),
                                                     JNIJVMCI::HotSpotJVMCIRuntime::clazz(),
                                                     JNIJVMCI::HotSpotJVMCIRuntime::compileMethod_method(),
                                                     method.as_jobject(), entry_bci, compile_state, id);
    if (jni()->ExceptionCheck()) {
      return JVMCIObject();
    }
    return wrap(result);
  }
}

int JVMCIEnv::call_HotSpotJVMCIRuntime_adjustCompilationLevel (JVMCIObject runtime, InstanceKlass* declaringClass,
                                                               JVMCIObject name, JVMCIObject signature, bool is_osr, int level, JVMCI_TRAPS) {
  if (is_hotspot()) {
    Thread* THREAD = Thread::current();
    JavaCallArguments jargs;
    jargs.push_oop(HotSpotJVMCI::resolve(runtime));
    jargs.push_oop(declaringClass->java_mirror());
    jargs.push_oop(HotSpotJVMCI::resolve(name));
    jargs.push_oop(HotSpotJVMCI::resolve(signature));
    jargs.push_int(is_osr);
    jargs.push_int(level);
    JavaValue result(T_INT);
    JavaCalls::call_special(&result, HotSpotJVMCI::HotSpotJVMCIRuntime::klass(), vmSymbols::adjustCompilationLevel_name(), vmSymbols::adjustCompilationLevel_signature(), &jargs, THREAD);
    return result.get_jint();
  } else {
    JVMCIObject declaringClassName = create_string(declaringClass->external_name(), JVMCI_CHECK_(0));
    int result;
    {
      JNIAccessMark jni(this);
      result = jni()->CallNonvirtualIntMethod(runtime.as_jobject(),
                                              JNIJVMCI::HotSpotJVMCIRuntime::clazz(),
                                              JNIJVMCI::HotSpotJVMCIRuntime::adjustCompilationLevel_method(),
                                              get_jobject(declaringClassName), get_jobject(name), get_jobject(signature), is_osr, level);
    }
    return result;
  }
}

void JVMCIEnv::call_HotSpotJVMCIRuntime_bootstrapFinished (JVMCIObject runtime, JVMCIEnv* JVMCIENV) {
  if (is_hotspot()) {
    Thread* THREAD = Thread::current();
    JavaCallArguments jargs;
    jargs.push_oop(HotSpotJVMCI::resolve(runtime));
    JavaValue result(T_VOID);
    JavaCalls::call_special(&result, HotSpotJVMCI::HotSpotJVMCIRuntime::klass(), vmSymbols::bootstrapFinished_name(), vmSymbols::void_method_signature(), &jargs, CHECK);
  } else {
    JNIAccessMark jni(this);
    jni()->CallNonvirtualVoidMethod(runtime.as_jobject(), JNIJVMCI::HotSpotJVMCIRuntime::clazz(), JNIJVMCI::HotSpotJVMCIRuntime::bootstrapFinished_method());

  }
}

void JVMCIEnv::call_HotSpotJVMCIRuntime_shutdown (JVMCIObject runtime) {
  HandleMark hm;
  JavaThread* THREAD = JavaThread::current();
  if (is_hotspot()) {
    JavaCallArguments jargs;
    jargs.push_oop(HotSpotJVMCI::resolve(runtime));
    JavaValue result(T_VOID);
    JavaCalls::call_special(&result, HotSpotJVMCI::HotSpotJVMCIRuntime::klass(), vmSymbols::shutdown_name(), vmSymbols::void_method_signature(), &jargs, THREAD);
  } else {
    JNIAccessMark jni(this);
    jni()->CallNonvirtualVoidMethod(runtime.as_jobject(), JNIJVMCI::HotSpotJVMCIRuntime::clazz(), JNIJVMCI::HotSpotJVMCIRuntime::shutdown_method());
  }
  if (has_pending_exception()) {
    // This should never happen as HotSpotJVMCIRuntime.shutdown() should
    // handle all exceptions.
    describe_pending_exception(true);
  }
}

JVMCIObject JVMCIEnv::call_HotSpotJVMCIRuntime_runtime (JVMCIEnv* JVMCIENV) {
  JavaThread* THREAD = JavaThread::current();
  if (is_hotspot()) {
    JavaCallArguments jargs;
    JavaValue result(T_OBJECT);
    JavaCalls::call_static(&result, HotSpotJVMCI::HotSpotJVMCIRuntime::klass(), vmSymbols::runtime_name(), vmSymbols::runtime_signature(), &jargs, CHECK_(JVMCIObject()));
    return wrap((oop) result.get_jobject());
  } else {
    JNIAccessMark jni(this);
    jobject result = jni()->CallStaticObjectMethod(JNIJVMCI::HotSpotJVMCIRuntime::clazz(), JNIJVMCI::HotSpotJVMCIRuntime::runtime_method());
    if (jni()->ExceptionCheck()) {
      return JVMCIObject();
    }
    return wrap(result);
  }
}

JVMCIObject JVMCIEnv::call_JVMCI_getRuntime (JVMCIEnv* JVMCIENV) {
  JavaThread* THREAD = JavaThread::current();
  if (is_hotspot()) {
    JavaCallArguments jargs;
    JavaValue result(T_OBJECT);
    JavaCalls::call_static(&result, HotSpotJVMCI::JVMCI::klass(), vmSymbols::getRuntime_name(), vmSymbols::getRuntime_signature(), &jargs, CHECK_(JVMCIObject()));
    return wrap((oop) result.get_jobject());
  } else {
    JNIAccessMark jni(this);
    jobject result = jni()->CallStaticObjectMethod(JNIJVMCI::JVMCI::clazz(), JNIJVMCI::JVMCI::getRuntime_method());
    if (jni()->ExceptionCheck()) {
      return JVMCIObject();
    }
    return wrap(result);
  }
}

JVMCIObject JVMCIEnv::call_HotSpotJVMCIRuntime_getCompiler (JVMCIObject runtime, JVMCIEnv* JVMCIENV) {
  JavaThread* THREAD = JavaThread::current();
  if (is_hotspot()) {
    JavaCallArguments jargs;
    jargs.push_oop(HotSpotJVMCI::resolve(runtime));
    JavaValue result(T_OBJECT);
    JavaCalls::call_virtual(&result, HotSpotJVMCI::HotSpotJVMCIRuntime::klass(), vmSymbols::getCompiler_name(), vmSymbols::getCompiler_signature(), &jargs, CHECK_(JVMCIObject()));
    return wrap((oop) result.get_jobject());
  } else {
    JNIAccessMark jni(this);
    jobject result = jni()->CallObjectMethod(runtime.as_jobject(), JNIJVMCI::HotSpotJVMCIRuntime::getCompiler_method());
    if (jni()->ExceptionCheck()) {
      return JVMCIObject();
    }
    return wrap(result);
  }
}


JVMCIObject JVMCIEnv::call_HotSpotJVMCIRuntime_callToString(JVMCIObject object, JVMCIEnv* JVMCIENV) {
  JavaThread* THREAD = JavaThread::current();
  if (is_hotspot()) {
    JavaCallArguments jargs;
    jargs.push_oop(HotSpotJVMCI::resolve(object));
    JavaValue result(T_OBJECT);
    JavaCalls::call_static(&result,
                           HotSpotJVMCI::HotSpotJVMCIRuntime::klass(),
                           vmSymbols::callToString_name(),
                           vmSymbols::callToString_signature(), &jargs, CHECK_(JVMCIObject()));
    return wrap((oop) result.get_jobject());
  } else {
    JNIAccessMark jni(this);
    jobject result = (jstring) jni()->CallStaticObjectMethod(JNIJVMCI::HotSpotJVMCIRuntime::clazz(),
                                                     JNIJVMCI::HotSpotJVMCIRuntime::callToString_method(),
                                                     object.as_jobject());
    if (jni()->ExceptionCheck()) {
      return JVMCIObject();
    }
    return wrap(result);
  }
}


JVMCIObject JVMCIEnv::call_PrimitiveConstant_forTypeChar(jchar kind, jlong value, JVMCI_TRAPS) {
  JavaThread* THREAD = JavaThread::current();
  if (is_hotspot()) {
    JavaCallArguments jargs;
    jargs.push_int(kind);
    jargs.push_long(value);
    JavaValue result(T_OBJECT);
    JavaCalls::call_static(&result,
                           HotSpotJVMCI::PrimitiveConstant::klass(),
                           vmSymbols::forTypeChar_name(),
                           vmSymbols::forTypeChar_signature(), &jargs, CHECK_(JVMCIObject()));
    return wrap((oop) result.get_jobject());
  } else {
    JNIAccessMark jni(this);
    jobject result = (jstring) jni()->CallStaticObjectMethod(JNIJVMCI::PrimitiveConstant::clazz(),
                                                     JNIJVMCI::PrimitiveConstant::forTypeChar_method(),
                                                     kind, value);
    if (jni()->ExceptionCheck()) {
      return JVMCIObject();
    }
    return wrap(result);
  }
}

JVMCIObject JVMCIEnv::call_JavaConstant_forFloat(float value, JVMCI_TRAPS) {
  JavaThread* THREAD = JavaThread::current();
  if (is_hotspot()) {
    JavaCallArguments jargs;
    jargs.push_float(value);
    JavaValue result(T_OBJECT);
    JavaCalls::call_static(&result,
                           HotSpotJVMCI::JavaConstant::klass(),
                           vmSymbols::forFloat_name(),
                           vmSymbols::forFloat_signature(), &jargs, CHECK_(JVMCIObject()));
    return wrap((oop) result.get_jobject());
  } else {
    JNIAccessMark jni(this);
    jobject result = (jstring) jni()->CallStaticObjectMethod(JNIJVMCI::JavaConstant::clazz(),
                                                     JNIJVMCI::JavaConstant::forFloat_method(),
                                                     value);
    if (jni()->ExceptionCheck()) {
      return JVMCIObject();
    }
    return wrap(result);
  }
}

JVMCIObject JVMCIEnv::call_JavaConstant_forDouble(double value, JVMCI_TRAPS) {
  JavaThread* THREAD = JavaThread::current();
  if (is_hotspot()) {
    JavaCallArguments jargs;
    jargs.push_double(value);
    JavaValue result(T_OBJECT);
    JavaCalls::call_static(&result,
                           HotSpotJVMCI::JavaConstant::klass(),
                           vmSymbols::forDouble_name(),
                           vmSymbols::forDouble_signature(), &jargs, CHECK_(JVMCIObject()));
    return wrap((oop) result.get_jobject());
  } else {
    JNIAccessMark jni(this);
    jobject result = (jstring) jni()->CallStaticObjectMethod(JNIJVMCI::JavaConstant::clazz(),
                                                     JNIJVMCI::JavaConstant::forDouble_method(),
                                                     value);
    if (jni()->ExceptionCheck()) {
      return JVMCIObject();
    }
    return wrap(result);
  }
}

JVMCIObject JVMCIEnv::get_jvmci_primitive_type(BasicType type) {
  JVMCIObjectArray primitives = get_HotSpotResolvedPrimitiveType_primitives();
  JVMCIObject result = get_object_at(primitives, type);
  return result;
}

JVMCIObject JVMCIEnv::new_StackTraceElement(methodHandle method, int bci, JVMCI_TRAPS) {
  JavaThread* THREAD = JavaThread::current();
  Symbol* method_name_sym;
  Symbol* file_name_sym;
  int line_number;
  Handle mirror (THREAD, method->method_holder()->java_mirror());
  java_lang_StackTraceElement::decode(mirror, method, bci, method_name_sym, file_name_sym, line_number);

  InstanceKlass* holder = method->method_holder();
  const char* declaring_class_str = holder->external_name();

  if (is_hotspot()) {
    HotSpotJVMCI::StackTraceElement::klass()->initialize(CHECK_(JVMCIObject()));
    Handle obj = HotSpotJVMCI::StackTraceElement::klass()->allocate_instance(CHECK_(JVMCIObject()));

    oop declaring_class = StringTable::intern((char*) declaring_class_str, CHECK_(JVMCIObject()));
    HotSpotJVMCI::StackTraceElement::set_declaringClass(this, obj(), declaring_class);

    oop method_name = StringTable::intern(method_name_sym, CHECK_(JVMCIObject()));
    HotSpotJVMCI::StackTraceElement::set_methodName(this, obj(), method_name);

    if (file_name_sym != NULL) {
      oop file_name = StringTable::intern(file_name_sym, CHECK_(JVMCIObject()));
      HotSpotJVMCI::StackTraceElement::set_fileName(this, obj(), file_name);
    }
    HotSpotJVMCI::StackTraceElement::set_lineNumber(this, obj(), line_number);
    return wrap(obj());
  } else {
    JNIAccessMark jni(this);
    jobject declaring_class = jni()->NewStringUTF(declaring_class_str);
    if (jni()->ExceptionCheck()) {
      return JVMCIObject();
    }
    jobject method_name = jni()->NewStringUTF(method_name_sym->as_C_string());
    if (jni()->ExceptionCheck()) {
      return JVMCIObject();
    }
    jobject file_name = NULL;
    if (file_name != NULL) {
      file_name = jni()->NewStringUTF(file_name_sym->as_C_string());
      if (jni()->ExceptionCheck()) {
        return JVMCIObject();
      }
    }

    jobject result = jni()->NewObject(JNIJVMCI::StackTraceElement::clazz(),
                                      JNIJVMCI::StackTraceElement::constructor(),
                                      declaring_class, method_name, file_name, line_number);
    return wrap(result);
  }
}

JVMCIObject JVMCIEnv::new_HotSpotNmethod(methodHandle method, const char* name, jboolean isDefault, jlong compileId, JVMCI_TRAPS) {
  JavaThread* THREAD = JavaThread::current();

  JVMCIObject methodObject = get_jvmci_method(method(), JVMCI_CHECK_(JVMCIObject()));

  if (is_hotspot()) {
    instanceKlassHandle ik(THREAD, HotSpotJVMCI::HotSpotNmethod::klass());
    if (ik->should_be_initialized()) {
      ik->initialize(CHECK_(JVMCIObject()));
    }
    oop obj = ik->allocate_instance(CHECK_(JVMCIObject()));
    Handle obj_h(THREAD, obj);
    Handle nameStr = java_lang_String::create_from_str(name, CHECK_(JVMCIObject()));

    // Call constructor
    JavaCallArguments jargs;
    jargs.push_oop(obj_h());
    jargs.push_oop(HotSpotJVMCI::resolve(methodObject));
    jargs.push_oop(nameStr());
    jargs.push_int(isDefault);
    jargs.push_long(compileId);
    JavaValue result(T_VOID);
    JavaCalls::call_special(&result, ik,
                            vmSymbols::object_initializer_name(),
                            vmSymbols::method_string_bool_long_signature(),
                            &jargs, CHECK_(JVMCIObject()));
    return wrap(obj_h());
  } else {
    JNIAccessMark jni(this);
    jobject nameStr = name == NULL ? NULL : jni()->NewStringUTF(name);
    if (jni()->ExceptionCheck()) {
      return JVMCIObject();
    }

    jobject result = jni()->NewObject(JNIJVMCI::HotSpotNmethod::clazz(),
                                      JNIJVMCI::HotSpotNmethod::constructor(),
                                      methodObject.as_jobject(), nameStr, isDefault);
    return wrap(result);
  }
}

JVMCIObject JVMCIEnv::make_local(JVMCIObject object) {
  if (object.is_null()) {
    return JVMCIObject();
  }
  if (is_hotspot()) {
    return wrap(JNIHandles::make_local(HotSpotJVMCI::resolve(object)));
  } else {
    JNIAccessMark jni(this);
    return wrap(jni()->NewLocalRef(object.as_jobject()));
  }
}

JVMCIObject JVMCIEnv::make_global(JVMCIObject object) {
  if (object.is_null()) {
    return JVMCIObject();
  }
  if (is_hotspot()) {
    return wrap(JNIHandles::make_global(HotSpotJVMCI::resolve(object)));
  } else {
    JNIAccessMark jni(this);
    return wrap(jni()->NewGlobalRef(object.as_jobject()));
  }
}

JVMCIObject JVMCIEnv::make_weak(JVMCIObject object) {
  if (object.is_null()) {
    return JVMCIObject();
  }
  if (is_hotspot()) {
    return wrap(JNIHandles::make_weak_global(HotSpotJVMCI::resolve(object)));
  } else {
    JNIAccessMark jni(this);
    return wrap(jni()->NewWeakGlobalRef(object.as_jobject()));
  }
}

void JVMCIEnv::destroy_local(JVMCIObject object) {
  if (is_hotspot()) {
    JNIHandles::destroy_local(object.as_jobject());
  } else {
    JNIAccessMark jni(this);
    jni()->DeleteLocalRef(object.as_jobject());
  }
}

void JVMCIEnv::destroy_global(JVMCIObject object) {
  if (is_hotspot()) {
    JNIHandles::destroy_global(object.as_jobject());
  } else {
    JNIAccessMark jni(this);
    jni()->DeleteGlobalRef(object.as_jobject());
  }
}

void JVMCIEnv::destroy_weak(JVMCIObject object) {
  if (is_hotspot()) {
    JNIHandles::destroy_weak_global(object.as_jweak());
  } else {
    JNIAccessMark jni(this);
    jni()->DeleteWeakGlobalRef(object.as_jweak());
  }
}

const char* JVMCIEnv::klass_name(JVMCIObject object) {
  if (is_hotspot()) {
    return HotSpotJVMCI::resolve(object)->klass()->signature_name();
  } else {
    JVMCIObject name;
    {
      JNIAccessMark jni(this);
      jclass jcl = jni()->GetObjectClass(object.as_jobject());
      jobject result = jni()->CallObjectMethod(jcl, JNIJVMCI::Class_getName_method());
      name = JVMCIObject::create(result, is_hotspot());
    }
    return as_utf8_string(name);
  }
}

JVMCIObject JVMCIEnv::get_jvmci_method(const methodHandle& method, JVMCI_TRAPS) {
  JVMCIObject method_object;
  if (method() == NULL) {
    return method_object;
  }

  Thread* THREAD = Thread::current();
  jmetadata handle = JVMCI::allocate_handle(method);
  jboolean exception = false;
  if (is_hotspot()) {
    JavaValue result(T_OBJECT);
    JavaCallArguments args;
    args.push_long((jlong) handle);
    JavaCalls::call_static(&result, HotSpotJVMCI::HotSpotResolvedJavaMethodImpl::klass(),
                           vmSymbols::fromMetaspace_name(),
                           vmSymbols::method_fromMetaspace_signature(), &args, THREAD);
    if (HAS_PENDING_EXCEPTION) {
      exception = true;
    } else {
      method_object = wrap((oop)result.get_jobject());
    }
  } else {
    JNIAccessMark jni(this);
    method_object = JNIJVMCI::wrap(jni()->CallStaticObjectMethod(JNIJVMCI::HotSpotResolvedJavaMethodImpl::clazz(),
                                                                  JNIJVMCI::HotSpotResolvedJavaMethodImpl_fromMetaspace_method(),
                                                                  (jlong) handle));
    exception = jni()->ExceptionCheck();
  }

  if (exception) {
    JVMCI::release_handle(handle);
    return JVMCIObject();
  }

  assert(asMethod(method_object) == method(), "must be");
  if (get_HotSpotResolvedJavaMethodImpl_metadataHandle(method_object) != (jlong) handle) {
    JVMCI::release_handle(handle);
  }
  assert(!method_object.is_null(), "must be");
  return method_object;
}

JVMCIObject JVMCIEnv::get_jvmci_type(JVMCIKlassHandle& klass, JVMCI_TRAPS) {
  JVMCIObject type;
  if (klass.is_null()) {
    return type;
  }
#ifdef INCLUDE_ALL_GCS
    if (UseG1GC) {
      // The klass might have come from a weak location so enqueue
      // the Class to make sure it's noticed by G1
      G1SATBCardTableModRefBS::enqueue(klass()->java_mirror());
    }
#endif  // Klass* don't require tracking as Metadata*

  jlong pointer = (jlong) klass();
  JavaThread* THREAD = JavaThread::current();
  JVMCIObject signature = create_string(klass->signature_name(), JVMCI_CHECK_(JVMCIObject()));
  jboolean exception = false;
  if (is_hotspot()) {
    JavaValue result(T_OBJECT);
    JavaCallArguments args;
    args.push_long(pointer);
    args.push_oop(HotSpotJVMCI::resolve(signature));
    JavaCalls::call_static(&result,
                           HotSpotJVMCI::HotSpotResolvedObjectTypeImpl::klass(),
                           vmSymbols::fromMetaspace_name(),
                           vmSymbols::klass_fromMetaspace_signature(), &args, THREAD);

    if (HAS_PENDING_EXCEPTION) {
      exception = true;
    } else {
      type = wrap((oop)result.get_jobject());
    }
  } else {
    JNIAccessMark jni(this);

    HandleMark hm(THREAD);
    type = JNIJVMCI::wrap(jni()->CallStaticObjectMethod(JNIJVMCI::HotSpotResolvedObjectTypeImpl::clazz(),
                                                        JNIJVMCI::HotSpotResolvedObjectTypeImpl_fromMetaspace_method(),
                                                        pointer, signature.as_jstring()));
    exception = jni()->ExceptionCheck();
  }
  if (exception) {
    return JVMCIObject();
  }

  assert(type.is_non_null(), "must have result");
  return type;
}

JVMCIObject JVMCIEnv::get_jvmci_constant_pool(constantPoolHandle cp, JVMCI_TRAPS) {
  JVMCIObject cp_object;
  jmetadata handle = JVMCI::allocate_handle(cp);
  jboolean exception = false;
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    JavaValue result(T_OBJECT);
    JavaCallArguments args;
    args.push_long((jlong) handle);
    JavaCalls::call_static(&result,
                           HotSpotJVMCI::HotSpotConstantPool::klass(),
                           vmSymbols::fromMetaspace_name(),
                           vmSymbols::constantPool_fromMetaspace_signature(), &args, THREAD);
    if (HAS_PENDING_EXCEPTION) {
      exception = true;
    } else {
      cp_object = wrap((oop)result.get_jobject());
    }
  } else {
    JNIAccessMark jni(this);
    cp_object = JNIJVMCI::wrap(jni()->CallStaticObjectMethod(JNIJVMCI::HotSpotConstantPool::clazz(),
                                                             JNIJVMCI::HotSpotConstantPool_fromMetaspace_method(),
                                                             (jlong) handle));
    exception = jni()->ExceptionCheck();
  }

  if (exception) {
    JVMCI::release_handle(handle);
    return JVMCIObject();
  }

  assert(!cp_object.is_null(), "must be");
  // Constant pools aren't cached so this is always a newly created object using the handle
  assert(get_HotSpotConstantPool_metadataHandle(cp_object) == (jlong) handle, "must use same handle");
  return cp_object;
}

JVMCIPrimitiveArray JVMCIEnv::new_booleanArray(int length, JVMCI_TRAPS) {
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    typeArrayOop result = oopFactory::new_boolArray(length, CHECK_(JVMCIObject()));
    return wrap(result);
  } else {
    JNIAccessMark jni(this);
    jbooleanArray result = jni()->NewBooleanArray(length);
    return wrap(result);
  }
}

JVMCIPrimitiveArray JVMCIEnv::new_byteArray(int length, JVMCI_TRAPS) {
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    typeArrayOop result = oopFactory::new_byteArray(length, CHECK_(JVMCIObject()));
    return wrap(result);
  } else {
    JNIAccessMark jni(this);
    jbyteArray result = jni()->NewByteArray(length);
    return wrap(result);
  }
}

JVMCIObjectArray JVMCIEnv::new_byte_array_array(int length, JVMCI_TRAPS) {
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    Klass* byteArrayArrayKlass = TypeArrayKlass::cast(Universe::byteArrayKlassObj  ())->array_klass(CHECK_(JVMCIObject()));
    objArrayOop result = ObjArrayKlass::cast(byteArrayArrayKlass) ->allocate(length, CHECK_(JVMCIObject()));
    return wrap(result);
  } else {
    JNIAccessMark jni(this);
    jobjectArray result = jni()->NewObjectArray(length, JNIJVMCI::byte_array(), NULL);
    return wrap(result);
  }
}

JVMCIPrimitiveArray JVMCIEnv::new_intArray(int length, JVMCI_TRAPS) {
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    typeArrayOop result = oopFactory::new_intArray(length, CHECK_(JVMCIObject()));
    return wrap(result);
  } else {
    JNIAccessMark jni(this);
    jintArray result = jni()->NewIntArray(length);
    return wrap(result);
  }
}

JVMCIPrimitiveArray JVMCIEnv::new_longArray(int length, JVMCI_TRAPS) {
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    typeArrayOop result = oopFactory::new_longArray(length, CHECK_(JVMCIObject()));
    return wrap(result);
  } else {
    JNIAccessMark jni(this);
    jlongArray result = jni()->NewLongArray(length);
    return wrap(result);
  }
}

JVMCIObject JVMCIEnv::new_VMField(JVMCIObject name, JVMCIObject type, jlong offset, jlong address, JVMCIObject value, JVMCI_TRAPS) {
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    HotSpotJVMCI::VMField::klass()->initialize(CHECK_(JVMCIObject()));
    oop obj = HotSpotJVMCI::VMField::klass()->allocate_instance(CHECK_(JVMCIObject()));
    HotSpotJVMCI::VMField::set_name(this, obj, HotSpotJVMCI::resolve(name));
    HotSpotJVMCI::VMField::set_type(this, obj, HotSpotJVMCI::resolve(type));
    HotSpotJVMCI::VMField::set_offset(this, obj, offset);
    HotSpotJVMCI::VMField::set_address(this, obj, address);
    HotSpotJVMCI::VMField::set_value(this, obj, HotSpotJVMCI::resolve(value));
    return wrap(obj);
  } else {
    JNIAccessMark jni(this);
    jobject result = jni()->NewObject(JNIJVMCI::VMField::clazz(),
                                    JNIJVMCI::VMField::constructor(),
                                    get_jobject(name), get_jobject(type), offset, address, get_jobject(value));
    return wrap(result);
  }
}

JVMCIObject JVMCIEnv::new_VMFlag(JVMCIObject name, JVMCIObject type, JVMCIObject value, JVMCI_TRAPS) {
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    HotSpotJVMCI::VMFlag::klass()->initialize(CHECK_(JVMCIObject()));
    oop obj = HotSpotJVMCI::VMFlag::klass()->allocate_instance(CHECK_(JVMCIObject()));
    HotSpotJVMCI::VMFlag::set_name(this, obj, HotSpotJVMCI::resolve(name));
    HotSpotJVMCI::VMFlag::set_type(this, obj, HotSpotJVMCI::resolve(type));
    HotSpotJVMCI::VMFlag::set_value(this, obj, HotSpotJVMCI::resolve(value));
    return wrap(obj);
  } else {
    JNIAccessMark jni(this);
    jobject result = jni()->NewObject(JNIJVMCI::VMFlag::clazz(),
                                    JNIJVMCI::VMFlag::constructor(),
                                    get_jobject(name), get_jobject(type), get_jobject(value));
    return wrap(result);
  }
}

JVMCIObject JVMCIEnv::new_VMIntrinsicMethod(JVMCIObject declaringClass, JVMCIObject name, JVMCIObject descriptor, int id, JVMCI_TRAPS) {
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    HotSpotJVMCI::VMIntrinsicMethod::klass()->initialize(CHECK_(JVMCIObject()));
    oop obj = HotSpotJVMCI::VMIntrinsicMethod::klass()->allocate_instance(CHECK_(JVMCIObject()));
    HotSpotJVMCI::VMIntrinsicMethod::set_declaringClass(this, obj, HotSpotJVMCI::resolve(declaringClass));
    HotSpotJVMCI::VMIntrinsicMethod::set_name(this, obj, HotSpotJVMCI::resolve(name));
    HotSpotJVMCI::VMIntrinsicMethod::set_descriptor(this, obj, HotSpotJVMCI::resolve(descriptor));
    return wrap(obj);
  } else {
    JNIAccessMark jni(this);
    jobject result = jni()->NewObject(JNIJVMCI::VMIntrinsicMethod::clazz(),
                                    JNIJVMCI::VMIntrinsicMethod::constructor(),
                                    get_jobject(declaringClass), get_jobject(name), get_jobject(descriptor), id);
    return wrap(result);
  }
}

JVMCIObject JVMCIEnv::new_HotSpotStackFrameReference(JVMCI_TRAPS) {
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    HotSpotJVMCI::HotSpotStackFrameReference::klass()->initialize(CHECK_(JVMCIObject()));
    oop obj = HotSpotJVMCI::HotSpotStackFrameReference::klass()->allocate_instance(CHECK_(JVMCIObject()));
    return wrap(obj);
  } else {
    ShouldNotReachHere();
    return JVMCIObject();
  }
}
JVMCIObject JVMCIEnv::new_JVMCIError(JVMCI_TRAPS) {
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    HotSpotJVMCI::JVMCIError::klass()->initialize(CHECK_(JVMCIObject()));
    oop obj = HotSpotJVMCI::JVMCIError::klass()->allocate_instance(CHECK_(JVMCIObject()));
    return wrap(obj);
  } else {
    ShouldNotReachHere();
    return JVMCIObject();
  }
}


JVMCIObject JVMCIEnv::get_object_constant(Handle obj, bool compressed, bool dont_register) {
  if (obj.is_null()) {
    return JVMCIObject();
  }
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    HotSpotJVMCI::DirectHotSpotObjectConstantImpl::klass()->initialize(CHECK_(JVMCIObject()));
    oop constant = HotSpotJVMCI::DirectHotSpotObjectConstantImpl::klass()->allocate_instance(CHECK_(JVMCIObject()));
    HotSpotJVMCI::DirectHotSpotObjectConstantImpl::set_object(this, constant, obj());
    HotSpotJVMCI::HotSpotObjectConstantImpl::set_compressed(this, constant, compressed);
    return wrap(constant);
  } else {
    jlong handle = make_handle(obj);
    JNIAccessMark jni(this);
    jobject result = jni()->NewObject(JNIJVMCI::IndirectHotSpotObjectConstantImpl::clazz(),
                                      JNIJVMCI::IndirectHotSpotObjectConstantImpl::constructor(),
                                      handle, compressed, dont_register);
    return wrap(result);
  }
}


Handle JVMCIEnv::asConstant(JVMCIObject constant, JVMCI_TRAPS) {
  if (constant.is_null()) {
    return Handle();
  }
  if (is_hotspot()) {
    assert(HotSpotJVMCI::DirectHotSpotObjectConstantImpl::is_instance(this, constant), "wrong type");
    return HotSpotJVMCI::DirectHotSpotObjectConstantImpl::object(this, HotSpotJVMCI::resolve(constant));
  } else {
    assert(isa_IndirectHotSpotObjectConstantImpl(constant), "wrong type");
    jlong object_handle = get_IndirectHotSpotObjectConstantImpl_objectHandle(constant);
    Handle result = resolve_handle(object_handle);
    if (result == NULL) {
      JVMCI_THROW_MSG_(InternalError, "Constant was unexpectedly NULL", Handle());
    }
    return result;
  }
}

JVMCIObject JVMCIEnv::wrap(jobject object) {
  return JVMCIObject::create(object, is_hotspot());
}

jlong JVMCIEnv::make_handle(Handle obj) {
  assert(!obj.is_null(), "should only create handle for non-NULL oops");
  jobject handle = JVMCI::make_global(obj);
  return (jlong) handle;
}

oop JVMCIEnv::resolve_handle(jlong objectHandle) {
  assert(objectHandle != 0, "should be a valid handle");
  oop obj = *((oopDesc**)objectHandle);
  if (obj != NULL) {
    obj->verify();
  }
  return obj;
}

JVMCIObject JVMCIEnv::create_string(const char* str, JVMCI_TRAPS) {
  if (is_hotspot()) {
    JavaThread* THREAD = JavaThread::current();
    Handle result = java_lang_String::create_from_str(str, CHECK_(JVMCIObject()));
    return HotSpotJVMCI::wrap(result());
  } else {
    jobject result;
    jboolean exception = false;
    {
      JNIAccessMark jni(this);
      result = jni()->NewStringUTF(str);
      exception = jni()->ExceptionCheck();
    }
    return wrap(result);
  }
}

bool JVMCIEnv::equals(JVMCIObject a, JVMCIObject b) {
  if (is_hotspot()) {
    return HotSpotJVMCI::resolve(a) == HotSpotJVMCI::resolve(b);
  } else {
    JNIAccessMark jni(this);
    return jni()->IsSameObject(a.as_jobject(), b.as_jobject()) != 0;
  }
}

BasicType JVMCIEnv::kindToBasicType(JVMCIObject kind, JVMCI_TRAPS) {
  if (kind.is_null()) {
    JVMCI_THROW_(NullPointerException, T_ILLEGAL);
  }
  jchar ch = get_JavaKind_typeChar(kind);
  switch(ch) {
    case 'Z': return T_BOOLEAN;
    case 'B': return T_BYTE;
    case 'S': return T_SHORT;
    case 'C': return T_CHAR;
    case 'I': return T_INT;
    case 'F': return T_FLOAT;
    case 'J': return T_LONG;
    case 'D': return T_DOUBLE;
    case 'A': return T_OBJECT;
    case '-': return T_ILLEGAL;
    default:
      JVMCI_ERROR_(T_ILLEGAL, "unexpected Kind: %c", ch);
  }
}

void JVMCIEnv::initialize_installed_code(JVMCIObject installed_code, CodeBlob* cb, JVMCI_TRAPS) {
  // Ensure that all updates to the InstalledCode fields are consistent.
  if (get_InstalledCode_address(installed_code) != 0) {
    JVMCI_THROW_MSG(InternalError, "InstalledCode instance already in use");
  }
  if (!isa_HotSpotInstalledCode(installed_code)) {
    JVMCI_THROW_MSG(InternalError, "InstalledCode instance must be a subclass of HotSpotInstalledCode");
  }

  // Ignore the version which can stay at 0
  if (cb->is_nmethod()) {
    nmethod* nm = cb->as_nmethod_or_null();
    if (!nm->is_alive()) {
      JVMCI_THROW_MSG(InternalError, "nmethod has been reclaimed");
    }
    if (nm->is_in_use()) {
      set_InstalledCode_entryPoint(installed_code, (jlong) nm->verified_entry_point());
    }
  } else {
    set_InstalledCode_entryPoint(installed_code, (jlong) cb->code_begin());
  }
  set_InstalledCode_address(installed_code, (jlong) cb);
  set_HotSpotInstalledCode_size(installed_code, cb->size());
  set_HotSpotInstalledCode_codeStart(installed_code, (jlong) cb->code_begin());
  set_HotSpotInstalledCode_codeSize(installed_code, cb->code_size());
}


void JVMCIEnv::invalidate_nmethod_mirror(JVMCIObject mirror, JVMCI_TRAPS) {
  if (mirror.is_null()) {
    JVMCI_THROW(NullPointerException);
  }

  jlong nativeMethod = get_InstalledCode_address(mirror);
  nmethod* nm = JVMCIENV->asNmethod(mirror);
  if (nm == NULL) {
    // Nothing to do
    return;
  }

  Thread* THREAD = Thread::current();
  if (!mirror.is_hotspot() && !THREAD->is_Java_thread()) {
    // Calling back into native might cause the execution to block, so only allow this when calling
    // from a JavaThread, which is the normal case anyway.
    JVMCI_THROW_MSG(IllegalArgumentException,
                    "Cannot invalidate HotSpotNmethod object in shared library VM heap from non-JavaThread");
  }

  nmethodLocker nml(nm);
  if (nm->is_alive()) {
    // Invalidating the HotSpotNmethod means we want the nmethod
    // to be deoptimized.
    nm->mark_for_deoptimization();
    VM_Deoptimize op;
    VMThread::execute(&op);
  }

  // A HotSpotNmethod instance can only reference a single nmethod
  // during its lifetime so simply clear it here.
  set_InstalledCode_address(mirror, 0);
}

Klass* JVMCIEnv::asKlass(JVMCIObject obj) {
  return (Klass*) get_HotSpotResolvedObjectTypeImpl_metadataPointer(obj);
}

Method* JVMCIEnv::asMethod(JVMCIObject obj) {
  Method** metadataHandle = (Method**) get_HotSpotResolvedJavaMethodImpl_metadataHandle(obj);
  return *metadataHandle;
}

ConstantPool* JVMCIEnv::asConstantPool(JVMCIObject obj) {
  ConstantPool** metadataHandle = (ConstantPool**) get_HotSpotConstantPool_metadataHandle(obj);
  return *metadataHandle;
}


CodeBlob* JVMCIEnv::asCodeBlob(JVMCIObject obj) {
  address code = (address) get_InstalledCode_address(obj);
  if (code == NULL) {
    return NULL;
  }
  if (isa_HotSpotNmethod(obj)) {
    jlong compile_id_snapshot = get_HotSpotNmethod_compileIdSnapshot(obj);
    if (compile_id_snapshot != 0L) {
      // A HotSpotNMethod not in an nmethod's oops table so look up
      // the nmethod and then update the fields based on its state.
      CodeBlob* cb = CodeCache::find_blob_unsafe(code);
      if (cb == (CodeBlob*) code) {
        // Found a live CodeBlob with the same address, make sure it's the same nmethod
        nmethod* nm = cb->as_nmethod_or_null();
        if (nm != NULL && nm->compile_id() == compile_id_snapshot) {
          if (!nm->is_alive()) {
            // Break the links from the mirror to the nmethod
            set_InstalledCode_address(obj, 0);
            set_InstalledCode_entryPoint(obj, 0);
          } else if (nm->is_not_entrant()) {
            // Zero the entry point so that the nmethod
            // cannot be invoked by the mirror but can
            // still be deoptimized.
            set_InstalledCode_entryPoint(obj, 0);
          }
          return cb;
        }
      }
      // Clear the InstalledCode fields of this HotSpotNmethod
      // that no longer refers to an nmethod in the code cache.
      set_InstalledCode_address(obj, 0);
      set_InstalledCode_entryPoint(obj, 0);
      return NULL;
    }
  }
  return (CodeBlob*) code;
}


// Generate implementations for the initialize, new, isa, get and set methods for all the types and
// fields declared in the JVMCI_CLASSES_DO macro.

#define START_CLASS(className, fullClassName)                                                                        \
  void JVMCIEnv::className##_initialize(JVMCI_TRAPS) {                                                               \
    if (is_hotspot()) {                                                                                              \
      HotSpotJVMCI::className::initialize(JVMCI_CHECK);                                                              \
    } else {                                                                                                         \
      JNIJVMCI::className::initialize(JVMCI_CHECK);                                                                  \
    }                                                                                                                \
  }                                                                                                                  \
  JVMCIObjectArray JVMCIEnv::new_##className##_array(int length, JVMCI_TRAPS) {                                      \
    if (is_hotspot()) {                                                                                              \
      Thread* THREAD = Thread::current();                                                                            \
      objArrayOop array = oopFactory::new_objArray(HotSpotJVMCI::className::klass(), length, CHECK_(JVMCIObject())); \
      return (JVMCIObjectArray) wrap(array);                                                                         \
    } else {                                                                                                         \
      JNIAccessMark jni(this);                                                                                       \
      jobjectArray result = jni()->NewObjectArray(length, JNIJVMCI::className::clazz(), NULL);                       \
      return wrap(result);                                                                                           \
    }                                                                                                                \
  }                                                                                                                  \
  bool JVMCIEnv::isa_##className(JVMCIObject object) {                                                               \
    if (is_hotspot()) {                                                                                              \
      return HotSpotJVMCI::className::is_instance(this, object);                                                     \
    } else {                                                                                                         \
      return JNIJVMCI::className::is_instance(this, object);                                                         \
    }                                                                                                                \
  }

#define END_CLASS

#define FIELD(className, name, type, accessor, cast)                 \
  type JVMCIEnv::get_##className##_##name(JVMCIObject obj) {         \
    if (is_hotspot()) {                                              \
      return HotSpotJVMCI::className::get_##name(this, obj);         \
    } else {                                                         \
      return JNIJVMCI::className::get_##name(this, obj);             \
    }                                                                \
  }                                                                  \
  void JVMCIEnv::set_##className##_##name(JVMCIObject obj, type x) { \
    if (is_hotspot()) {                                              \
      HotSpotJVMCI::className::set_##name(this, obj, x);             \
    } else {                                                         \
      JNIJVMCI::className::set_##name(this, obj, x);                 \
    }                                                                \
  }

#define EMPTY_CAST
#define CHAR_FIELD(className, name)                    FIELD(className, name, jchar, Char, EMPTY_CAST)
#define INT_FIELD(className, name)                     FIELD(className, name, jint, Int, EMPTY_CAST)
#define BOOLEAN_FIELD(className, name)                 FIELD(className, name, jboolean, Boolean, EMPTY_CAST)
#define LONG_FIELD(className, name)                    FIELD(className, name, jlong, Long, EMPTY_CAST)
#define FLOAT_FIELD(className, name)                   FIELD(className, name, jfloat, Float, EMPTY_CAST)

#define OBJECT_FIELD(className, name, signature)              OOPISH_FIELD(className, name, JVMCIObject, Object, EMPTY_CAST)
#define OBJECTARRAY_FIELD(className, name, signature)         OOPISH_FIELD(className, name, JVMCIObjectArray, Object, (JVMCIObjectArray))
#define PRIMARRAY_FIELD(className, name, signature)           OOPISH_FIELD(className, name, JVMCIPrimitiveArray, Object, (JVMCIPrimitiveArray))

#define STATIC_OBJECT_FIELD(className, name, signature)       STATIC_OOPISH_FIELD(className, name, JVMCIObject, Object, (JVMCIObject))
#define STATIC_OBJECTARRAY_FIELD(className, name, signature)  STATIC_OOPISH_FIELD(className, name, JVMCIObjectArray, Object, (JVMCIObjectArray))

#define OOPISH_FIELD(className, name, type, accessor, cast)           \
  type JVMCIEnv::get_##className##_##name(JVMCIObject obj) {          \
    if (is_hotspot()) {                                               \
      return HotSpotJVMCI::className::get_##name(this, obj);          \
    } else {                                                          \
      return JNIJVMCI::className::get_##name(this, obj);              \
    }                                                                 \
  }                                                                   \
  void JVMCIEnv::set_##className##_##name(JVMCIObject obj, type x) {  \
    if (is_hotspot()) {                                               \
      HotSpotJVMCI::className::set_##name(this, obj, x);              \
    } else {                                                          \
      JNIJVMCI::className::set_##name(this, obj, x);                  \
    }                                                                 \
  }

#define STATIC_OOPISH_FIELD(className, name, type, accessor, cast)    \
  type JVMCIEnv::get_##className##_##name() {                         \
    if (is_hotspot()) {                                               \
      return HotSpotJVMCI::className::get_##name(this);               \
    } else {                                                          \
      return JNIJVMCI::className::get_##name(this);                   \
    }                                                                 \
  }                                                                   \
  void JVMCIEnv::set_##className##_##name(type x) {                   \
    if (is_hotspot()) {                                               \
      HotSpotJVMCI::className::set_##name(this, x);                   \
    } else {                                                          \
      JNIJVMCI::className::set_##name(this, x);                       \
    }                                                                 \
  }

#define STATIC_PRIMITIVE_FIELD(className, name, type, accessor, cast) \
  type JVMCIEnv::get_##className##_##name() {                         \
    if (is_hotspot()) {                                               \
      return HotSpotJVMCI::className::get_##name(this);               \
    } else {                                                          \
      return JNIJVMCI::className::get_##name(this);                   \
    }                                                                 \
  }                                                                   \
  void JVMCIEnv::set_##className##_##name(type x) {                   \
    if (is_hotspot()) {                                               \
      HotSpotJVMCI::className::set_##name(this, x);                   \
    } else {                                                          \
      JNIJVMCI::className::set_##name(this, x);                       \
    }                                                                 \
  }
#define STATIC_INT_FIELD(className, name) STATIC_PRIMITIVE_FIELD(className, name, jint, Int, EMPTY_CAST)
#define STATIC_BOOLEAN_FIELD(className, name) STATIC_PRIMITIVE_FIELD(className, name, jboolean, Boolean, EMPTY_CAST)
#define METHOD(jniCallType, jniGetMethod, hsCallType, returnType, className, methodName, signatureSymbolName, args)
#define CONSTRUCTOR(className, signature)

JVMCI_CLASSES_DO(START_CLASS, END_CLASS, CHAR_FIELD, INT_FIELD, BOOLEAN_FIELD, LONG_FIELD, FLOAT_FIELD, OBJECT_FIELD, PRIMARRAY_FIELD, OBJECTARRAY_FIELD, STATIC_OBJECT_FIELD, STATIC_OBJECTARRAY_FIELD, STATIC_INT_FIELD, STATIC_BOOLEAN_FIELD, METHOD, CONSTRUCTOR)

#undef START_CLASS
#undef END_CLASS
#undef METHOD
#undef CONSTRUCTOR
#undef FIELD
#undef CHAR_FIELD
#undef INT_FIELD
#undef BOOLEAN_FIELD
#undef LONG_FIELD
#undef FLOAT_FIELD
#undef OBJECT_FIELD
#undef PRIMARRAY_FIELD
#undef OBJECTARRAY_FIELD
#undef STATIC_OOPISH_FIELD
#undef STATIC_OBJECT_FIELD
#undef STATIC_OBJECTARRAY_FIELD
#undef STATIC_INT_FIELD
#undef STATIC_BOOLEAN_FIELD
#undef EMPTY_CAST
