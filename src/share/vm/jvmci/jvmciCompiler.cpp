/*
 * Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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
#include "memory/oopFactory.hpp"
#include "runtime/javaCalls.hpp"
#include "jvmci/jvmciCompiler.hpp"
#include "jvmci/jvmciEnv.hpp"
#include "jvmci/jvmciRuntime.hpp"
#include "runtime/compilationPolicy.hpp"
#include "runtime/globals_extension.hpp"

JVMCICompiler* JVMCICompiler::_instance = NULL;
elapsedTimer JVMCICompiler::_codeInstallTimer;

JVMCICompiler::JVMCICompiler() : AbstractCompiler(jvmci) {
#ifdef COMPILERJVMCI
  _bootstrapping = false;
  _methodsCompiled = 0;
#endif
  assert(_instance == NULL, "only one instance allowed");
  _instance = this;
}

// Initialization
void JVMCICompiler::initialize() {
#ifdef COMPILERJVMCI
  if (!UseCompiler || !should_perform_init()) {
    return;
  }

  BufferBlob* buffer_blob = JVMCIRuntime::initialize_buffer_blob();
  if (buffer_blob == NULL) {
    set_state(failed);
  } else {
    set_state(initialized);
  }
  // JVMCI is considered as application code so we need to
  // stop the VM deferring compilation now.
  CompilationPolicy::completed_vm_startup();
#endif // COMPILERJVMCI
}

#ifdef COMPILERJVMCI
void JVMCICompiler::bootstrap() {
  JavaThread* THREAD = JavaThread::current();
  _bootstrapping = true;
  // Allow bootstrap to perform JVMCI compilations of itself
  bool c1only = JVMCICompileWithC1Only;
  JVMCICompileWithC1Only = false;
  ResourceMark rm;
  HandleMark hm;
  if (PrintBootstrap) {
    tty->print("Bootstrapping JVMCI");
  }
  jlong start = os::javaTimeMillis();

  Array<Method*>* objectMethods = InstanceKlass::cast(SystemDictionary::Object_klass())->methods();
  // Initialize compile queue with a selected set of methods.
  int len = objectMethods->length();
  for (int i = 0; i < len; i++) {
    methodHandle mh = objectMethods->at(i);
    if (!mh->is_native() && !mh->is_static() && !mh->is_initializer()) {
      ResourceMark rm;
      int hot_count = 10; // TODO: what's the appropriate value?
      CompileBroker::compile_method(mh, InvocationEntryBci, CompLevel_full_optimization, mh, hot_count, "bootstrap", THREAD);
    }
  }

  int qsize;
  bool first_round = true;
  int z = 0;
  do {
    // Loop until there is something in the queue.
    do {
      os::sleep(THREAD, 100, true);
      qsize = CompileBroker::queue_size(CompLevel_full_optimization);
    } while (first_round && qsize == 0);
    first_round = false;
    if (PrintBootstrap) {
      while (z < (_methodsCompiled / 100)) {
        ++z;
        tty->print_raw(".");
      }
    }
  } while (qsize != 0);

  if (PrintBootstrap) {
    tty->print_cr(" in " JLONG_FORMAT " ms (compiled %d methods)", os::javaTimeMillis() - start, _methodsCompiled);
  }
  JVMCICompileWithC1Only = c1only;
  _bootstrapping = false;
}

void JVMCICompiler::compile_method(methodHandle method, int entry_bci, JVMCIEnv* env) {
  JVMCI_EXCEPTION_CONTEXT

  bool is_osr = entry_bci != InvocationEntryBci;
  if (_bootstrapping && is_osr) {
      // no OSR compilations during bootstrap - the compiler is just too slow at this point,
      // and we know that there are no endless loops
      return;
  }

  JVMCIRuntime::ensure_jvmci_class_loader_is_initialized();
  HandleMark hm;
  ResourceMark rm;
  JavaValue result(T_VOID);
  JavaCallArguments args;
  Handle receiver = JVMCIRuntime::get_HotSpotJVMCIRuntime();
  args.push_oop(receiver);
  args.push_long((jlong) (address) method());
  args.push_int(entry_bci);
  args.push_long((jlong) (address) env);
  args.push_int(env->task()->compile_id());
  JavaCalls::call_special(&result, receiver->klass(), vmSymbols::compileMetaspaceMethod_name(), vmSymbols::compileMetaspaceMethod_signature(), &args, CHECK_ABORT);

  _methodsCompiled++;
}


// Compilation entry point for methods
void JVMCICompiler::compile_method(ciEnv* env, ciMethod* target, int entry_bci) {
  ShouldNotReachHere();
}

// Print compilation timers and statistics
void JVMCICompiler::print_timers() {
  print_compilation_timers();
}

#endif // COMPILERJVMCI

// Print compilation timers and statistics
void JVMCICompiler::print_compilation_timers() {
  TRACE_jvmci_1("JVMCICompiler::print_timers");
  tty->print_cr("       JVMCI code install time:        %6.3f s",    _codeInstallTimer.seconds());
}

void JVMCICompiler::compile_the_world() {
  HandleMark hm;
  ResourceMark rm;
  JVMCIRuntime::ensure_jvmci_class_loader_is_initialized();
  JavaThread* THREAD = JavaThread::current();
  Handle receiver = JVMCIRuntime::get_HotSpotJVMCIRuntime();
  TempNewSymbol compileTheWorld = SymbolTable::new_symbol("compileTheWorld", CHECK_ABORT);
  JavaValue result(T_VOID);
  JavaCallArguments args;
  args.push_oop(receiver);
  JavaCalls::call_special(&result, receiver->klass(), compileTheWorld, vmSymbols::void_method_signature(), &args, CHECK_ABORT);
}
