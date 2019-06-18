/*
 * Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
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
#include "classfile/systemDictionary.hpp"
#include "jvmci/jvmci.hpp"
#include "jvmci/jvmci_globals.hpp"
#include "jvmci/jvmciJavaClasses.hpp"
#include "jvmci/jvmciRuntime.hpp"
#include "jvmci/metadataHandleBlock.hpp"
#ifdef INCLUDE_ALL_GCS
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#endif

JNIHandleBlock* JVMCI::_object_handles = NULL;
MetadataHandleBlock* JVMCI::_metadata_handles = NULL;
JVMCIRuntime* JVMCI::_compiler_runtime = NULL;
JVMCIRuntime* JVMCI::_java_runtime = NULL;

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
  if (UseJVMCINativeLibrary) {
    // There are two runtimes.
    _compiler_runtime = new JVMCIRuntime();
    _java_runtime = new JVMCIRuntime();
  } else {
    // There is only a single runtime
    _java_runtime = _compiler_runtime = new JVMCIRuntime();
  }
}

jobject JVMCI::make_global(const Handle& obj) {
  assert(_object_handles != NULL, "uninitialized");
  MutexLocker ml(JVMCI_lock);
  return _object_handles->allocate_handle(obj());
}

bool JVMCI::is_global_handle(jobject handle) {
  assert(_object_handles != NULL, "uninitialized");
  MutexLocker ml(JVMCI_lock);
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

#ifdef INCLUDE_ALL_GCS
oop JVMCI::ensure_oop_alive(oop obj) {
    if (UseG1GC && obj != NULL) {
      G1SATBCardTableModRefBS::enqueue(obj);
    }
    return obj;
}
#endif // INCLUDE_ALL_GCS

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

void JVMCI::do_unloading(bool unloading_occurred) {
  if (_metadata_handles != NULL && unloading_occurred) {
    _metadata_handles->do_unloading();
  }
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
