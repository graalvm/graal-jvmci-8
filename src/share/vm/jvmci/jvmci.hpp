/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_JVMCI_JVMCI_HPP
#define SHARE_VM_JVMCI_JVMCI_HPP

#include "interpreter/interpreter.hpp"
#include "jvmci/jvmciExceptions.hpp"
#include "jvmci/jvmciJavaClasses.hpp"
#include "memory/allocation.hpp"
#include "runtime/arguments.hpp"
#include "runtime/deoptimization.hpp"

class MetadataHandleBlock;
class JVMCIRuntime;

struct _jmetadata;
typedef struct _jmetadata *jmetadata;

class JVMCI : public AllStatic {
  friend class JVMCIRuntime;
  friend class JVMCIEnv;

 private:
  // Handles to objects in the HotSpot heap.
  static JNIHandleBlock* _object_handles;

  // Handles to Metadata objects.
  static MetadataHandleBlock* _metadata_handles;

  // Access to the HotSpotJVMCIRuntime used by the CompileBroker.
  static JVMCIRuntime* _compiler_runtime;

  // Access to the HotSpotJVMCIRuntime used by Java code running on the
  // HotSpot heap. It will be the same as _compiler_runtime if
  // JVMCIGlobals::java_mode() == JVMCIGlobals::HotSpot.
  static JVMCIRuntime* _java_runtime;

 public:
  enum CodeInstallResult {
     ok,
     dependencies_failed,
     dependencies_invalid,
     cache_full,
     code_too_large
  };

  static void do_unloading(BoolObjectClosure* is_alive, bool unloading_occurred);

  static void metadata_do(void f(Metadata*));

  static void oops_do(OopClosure* f);

  static void shutdown();

  static bool shutdown_called();

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
  static CompLevel adjust_comp_level(methodHandle method, bool is_osr, CompLevel level, JavaThread* thread);

  static bool is_compiler_initialized();

  static void initialize_globals();

  static void initialize_compiler(TRAPS);
  // JVMCIRuntime::call_getCompiler(CHECK);

  static jobject make_global(Handle obj);
  static bool is_global_handle(jobject handle);

  static jmetadata allocate_handle(const methodHandle& handle);
  static jmetadata allocate_handle(const constantPoolHandle& handle);

  static void release_handle(jmetadata handle);

  static JVMCIRuntime* compiler_runtime() { return _compiler_runtime; }
  static JVMCIRuntime* java_runtime()     { return _java_runtime; }
};

#endif // SHARE_VM_JVMCI_JVMCI_HPP
