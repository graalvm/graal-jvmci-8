/*
 * Copyright (c) 2017, 2019, Oracle and/or its affiliates. All rights reserved.
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

#include "utilities/exceptions.hpp"

class BoolObjectClosure;
class constantPoolHandle;
class JavaThread;
class JNIHandleBlock;
class JVMCIEnv;
class JVMCIRuntime;
class Metadata;
class OopClosure;


struct _jmetadata;
typedef struct _jmetadata *jmetadata;

class JVMCI : public AllStatic {
  friend class JVMCIRuntime;
  friend class JVMCIEnv;

 private:
  // Access to the HotSpotJVMCIRuntime used by the CompileBroker.
  static JVMCIRuntime* _compiler_runtime;

  // True when JVMCIRuntime::initialize_HotSpotJVMCIRuntime()
  // has completed successfully.
  static volatile bool _is_initialized;

  // Handle created when loading the JVMCI shared library with os::dll_load.
  // Must hold JVMCI_lock when initializing.
  static void* _shared_library_handle;

  // Argument to os::dll_load when loading JVMCI shared library
  static char* _shared_library_path;

  // Records whether JVMCI::shutdown has been called.
  static volatile bool _in_shutdown;

  // Access to the HotSpot heap based JVMCIRuntime
  static JVMCIRuntime* _java_runtime;

  // JVMCI event log (shows up in hs_err crash logs).
  static StringEventLog* _events;
  static StringEventLog* _verbose_events;
  enum {
    max_EventLog_level = 4
  };

 public:
  enum CodeInstallResult {
     ok,
     dependencies_failed,
     dependencies_invalid,
     cache_full,
     code_too_large
  };

  // Gets the handle to the loaded JVMCI shared library, loading it
  // first if not yet loaded and `load` is true. The path from
  // which the library is loaded is returned in `path`. If
  // `load` is true then JVMCI_lock must be locked.
  static void* get_shared_library(char*& path, bool load);

  static void do_unloading(bool unloading_occurred);

  static void metadata_do(void f(Metadata*));

  static void oops_do(OopClosure* f);

  static void shutdown();

  // Returns whether JVMCI::shutdown has been called.
  static bool in_shutdown();

  static bool is_compiler_initialized();

  static void initialize_globals();

  static void initialize_compiler(TRAPS);

  static JVMCIRuntime* compiler_runtime() { return _compiler_runtime; }
  // Gets the single runtime for JVMCI on the Java heap. This is the only
  // JVMCI runtime available when !UseJVMCINativeLibrary.
  static JVMCIRuntime* java_runtime()     { return _java_runtime; }

  // The `obj` value might have come from a weak location so enqueue
  // it to make sure it's noticed by G1.
#ifdef INCLUDE_ALL_GCS
  static oop ensure_oop_alive(oop obj);
#else
  static oop ensure_oop_alive(oop obj) { return obj; };
#endif

  // Appends an event to the JVMCI event log if JVMCIEventLogLevel >= `level`
  static void vlog(int level, const char* format, va_list ap);

  // Traces an event to tty if JVMCITraceLevel >= `level`
  static void vtrace(int level, const char* format, va_list ap);

 public:
  // Gets the Thread* value for the current thread or NULL if it's not available.
  static Thread* current_thread_or_null();

  // Log/trace a JVMCI event
  static void event(int level, const char* format, ...);
  static void event1(const char* format, ...);
  static void event2(const char* format, ...);
  static void event3(const char* format, ...);
  static void event4(const char* format, ...);
};

// JVMCI event macros.
#define JVMCI_event_1 if (JVMCITraceLevel < 1 && JVMCIEventLogLevel < 1) ; else ::JVMCI::event1
#define JVMCI_event_2 if (JVMCITraceLevel < 2 && JVMCIEventLogLevel < 2) ; else ::JVMCI::event2
#define JVMCI_event_3 if (JVMCITraceLevel < 3 && JVMCIEventLogLevel < 3) ; else ::JVMCI::event3
#define JVMCI_event_4 if (JVMCITraceLevel < 4 && JVMCIEventLogLevel < 4) ; else ::JVMCI::event4

#endif // SHARE_VM_JVMCI_JVMCI_HPP
