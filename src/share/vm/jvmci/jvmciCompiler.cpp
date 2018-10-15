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
  _bootstrapping = false;
  _bootstrap_compilation_request_handled = false;
  _methods_compiled = 0;
  assert(_instance == NULL, "only one instance allowed");
  _instance = this;
}

// Initialization
void JVMCICompiler::initialize() {
  if (!UseCompiler || !UseJVMCICompiler || !should_perform_init()) {
    return;
  }

  set_state(initialized);

  // JVMCI is considered as application code so we need to
  // stop the VM deferring compilation now.
  CompilationPolicy::completed_vm_startup();
}

void JVMCICompiler::bootstrap(TRAPS) {
  if (Arguments::mode() == Arguments::_int) {
    // Nothing to do in -Xint mode
    return;
  }
#ifndef PRODUCT
  // We turn off CompileTheWorld so that compilation requests are not
  // ignored during bootstrap or that JVMCI can be compiled by C1/C2.
  FlagSetting ctwOff(CompileTheWorld, false);
#endif

  _bootstrapping = true;
  // Allow bootstrap to perform JVMCI compilations of itself
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
  int z = 0;
  do {
    // Loop until there is something in the queue.
    do {
      os::sleep(THREAD, 100, true);
      qsize = CompileBroker::queue_size(CompLevel_full_optimization);
    } while (!_bootstrap_compilation_request_handled);
    if (PrintBootstrap) {
      while (z < (_methods_compiled / 100)) {
        ++z;
        tty->print_raw(".");
      }
    }
  } while (qsize != 0);

  if (PrintBootstrap) {
    tty->print_cr(" in " JLONG_FORMAT " ms (compiled %d methods)", os::javaTimeMillis() - start, _methods_compiled);
  }
  _bootstrapping = false;
  JVMCI::compiler_runtime()->bootstrap_finished(CHECK);
}

// Compilation entry point for methods
void JVMCICompiler::compile_method(ciEnv* env, ciMethod* target, int entry_bci) {
  ShouldNotReachHere();
}

// Print compilation timers and statistics
void JVMCICompiler::print_timers() {
  print_compilation_timers();
}

// Print compilation timers and statistics
void JVMCICompiler::print_compilation_timers() {
  TRACE_jvmci_1("JVMCICompiler::print_timers");
  tty->print_cr("       JVMCI code install time:        %6.3f s",    _codeInstallTimer.seconds());
}
