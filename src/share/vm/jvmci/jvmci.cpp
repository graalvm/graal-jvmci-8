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
#include "runtime/arguments.hpp"
#include "jvmci/jvmci.hpp"
#include "jvmci/jvmci_globals.hpp"
#include "jvmci/jvmciJavaClasses.hpp"
#include "jvmci/jvmciRuntime.hpp"
#include "jvmci/metadataHandles.hpp"
#ifdef INCLUDE_ALL_GCS
#include "gc_implementation/g1/g1SATBCardTableModRefBS.hpp"
#endif

JVMCIRuntime* JVMCI::_compiler_runtime = NULL;
JVMCIRuntime* JVMCI::_java_runtime = NULL;
volatile bool JVMCI::_is_initialized = false;
void* JVMCI::_shared_library_handle = NULL;
char* JVMCI::_shared_library_path = NULL;
volatile bool JVMCI::_in_shutdown = false;
StringEventLog* JVMCI::_events = NULL;

void* JVMCI::get_shared_library(char*& path, bool load) {
  void* sl_handle = _shared_library_handle;
  if (sl_handle != NULL || !load) {
    path = _shared_library_path;
    return sl_handle;
  }
  assert(JVMCI_lock->owner() == Thread::current(), "must be");
  path = NULL;
  if (_shared_library_handle == NULL) {
    char path[JVM_MAXPATHLEN];
    char ebuf[1024];
    if (JVMCILibPath != NULL) {
      if (!os::dll_build_name(path, sizeof(path), JVMCILibPath, JVMCI_SHARED_LIBRARY_NAME)) {
        vm_exit_during_initialization("Unable to locate JVMCI shared library in path specified by -XX:JVMCILibPath value", JVMCILibPath);
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

    JVMCI::log("loaded JVMCI shared library from %s", path);
  }
  path = _shared_library_path;
  return _shared_library_handle;
}

void JVMCI::initialize_compiler(TRAPS) {
  if (JVMCILibDumpJNIConfig) {
    JNIJVMCI::initialize_ids(NULL);
    ShouldNotReachHere();
  }

  JVMCI::compiler_runtime()->call_getCompiler(CHECK);
}

void JVMCI::initialize_globals() {
  if (LogEvents) {
    _events = new StringEventLog("JVMCI Events");
  }
  if (UseJVMCINativeLibrary) {
    // There are two runtimes.
    _compiler_runtime = new JVMCIRuntime(0);
    _java_runtime = new JVMCIRuntime(-1);
  } else {
    // There is only a single runtime
    _java_runtime = _compiler_runtime = new JVMCIRuntime(0);
  }
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
  if (_java_runtime != NULL) {
    _java_runtime->_object_handles->oops_do(f);
  }
  if (_compiler_runtime != NULL && _compiler_runtime != _java_runtime) {
    _compiler_runtime->_object_handles->oops_do(f);
  }
}

void JVMCI::metadata_do(void f(Metadata*)) {
  if (_java_runtime != NULL) {
    _java_runtime->_metadata_handles->metadata_do(f);
  }
  if (_compiler_runtime != NULL && _compiler_runtime != _java_runtime) {
    _compiler_runtime->_metadata_handles->metadata_do(f);
  }
}

void JVMCI::do_unloading(bool unloading_occurred) {
  if (unloading_occurred) {
    if (_java_runtime != NULL) {
      _java_runtime->_metadata_handles->do_unloading();
    }
    if (_compiler_runtime != NULL && _compiler_runtime != _java_runtime) {
      _compiler_runtime->_metadata_handles->do_unloading();
    }
  }
}

bool JVMCI::is_compiler_initialized() {
  return _is_initialized;
}

void JVMCI::shutdown() {
  ResourceMark rm;
  {
    MutexLocker locker(JVMCI_lock);
    _in_shutdown = true;
    JVMCI::log("shutting down JVMCI");
  }
  JVMCIRuntime* java_runtime = _java_runtime;
  if (java_runtime != compiler_runtime()) {
    java_runtime->shutdown();
  }
  if (compiler_runtime() != NULL) {
    compiler_runtime()->shutdown();
  }
}

bool JVMCI::in_shutdown() {
  return _in_shutdown;
}

Thread* JVMCI::current_thread_or_null() {
  Thread* thread = ThreadLocalStorage::thread();
  return thread == NULL ? ThreadLocalStorage::get_thread_slow() : thread;
}

bool JVMCI::trace_prefix(int level) {
  Thread* thread = current_thread_or_null();
  if (thread != NULL) {
    ResourceMark rm;
    tty->print("JVMCITrace-%d[%s]:%*c", level, thread->name(), level, ' ');
  } else {
    tty->print("JVMCITrace-%d[?]:%*c", level, level, ' ');
  }
  return true;
}

void JVMCI::log(const char* format, ...) {
  if (LogEvents) {
    guarantee(_events != NULL, "JVMCI event log not yet initialized");
    va_list ap;
    va_start(ap, format);
    _events->logv(current_thread_or_null(), format, ap);
    va_end(ap);
  }
  if (JVMCITraceLevel >= 1) {
    trace_prefix(1);
    va_list ap;
    va_start(ap, format);
    tty->vprint_cr(format, ap);
    va_end(ap);
  }
}
