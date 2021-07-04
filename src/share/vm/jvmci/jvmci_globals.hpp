/*
 * Copyright (c) 2000, 2012, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_JVMCI_JVMCI_GLOBALS_HPP
#define SHARE_VM_JVMCI_JVMCI_GLOBALS_HPP

#include "runtime/globals.hpp"
#ifdef TARGET_ARCH_x86
# include "jvmci_globals_x86.hpp"
#endif
#ifdef TARGET_ARCH_sparc
# include "jvmci_globals_sparc.hpp"
#endif
#ifdef TARGET_ARCH_arm
# include "jvmci_globals_arm.hpp"
#endif
#ifdef TARGET_ARCH_ppc
# include "jvmci_globals_ppc.hpp"
#endif

#define LIBJVMCI_ERR_FILE "hs_err_pid%p_libjvmci.log"

//
// Defines all global flags used by the JVMCI compiler. Only flags that need
// to be accessible to the JVMCI C++ code should be defined here. All other
// JVMCI flags should be defined in JVMCIOptions.java.
//
#define JVMCI_FLAGS(develop, develop_pd, product, product_pd, notproduct)   \
                                                                            \
  product(bool, EnableJVMCI, true,                                          \
          "Enable JVMCI")                                                   \
                                                                            \
  product(bool, UseJVMCICompiler, false,                                    \
          "Use JVMCI as the default compiler. Will be true by default "     \
          "if jvmci.Compiler property is set (either on command line or "   \
          "from contents of <java.home>/lib/jvmci/compiler-name")           \
                                                                            \
  product(bool, JVMCIPrintProperties, false,                                \
          "Prints properties used by the JVMCI compiler and exits")         \
                                                                            \
  product(bool, UseJVMCIClassLoader, true,                                  \
          "Load JVMCI classes with separate class loader")                  \
                                                                            \
  product(bool, BootstrapJVMCI, false,                                      \
          "Bootstrap JVMCI before running Java main method. This "          \
          "initializes the compile queue with a small set of methods "      \
          "and processes the queue until it is empty. Combining this with " \
          "-XX:-TieredCompilation makes JVMCI compile more of itself.")     \
                                                                            \
  product(bool, PrintBootstrap, true,                                       \
          "Print JVMCI bootstrap progress and summary")                     \
                                                                            \
  product(bool, EagerJVMCI, false,                                          \
          "Force eager initialization of the JVMCI compiler")               \
                                                                            \
  product(intx, JVMCIThreads, 1,                                            \
          "Force number of JVMCI compiler threads to use. Ignored if "      \
          "UseJVMCICompiler is false.")                                     \
                                                                            \
  product(intx, JVMCIHostThreads, 1,                                        \
          "Force number of C1 compiler threads. Ignored if "                \
          "UseJVMCICompiler is false.")                                     \
                                                                            \
  product(bool, CodeInstallSafepointChecks, true,                           \
          "Perform explicit safepoint checks while installing code")        \
                                                                            \
  NOT_COMPILER2(product_pd(intx, MaxVectorSize,                             \
          "Max vector size in bytes, "                                      \
          "actual size could be less depending on elements type"))          \
                                                                            \
  NOT_COMPILER2(product(bool, ReduceInitialCardMarks, true,                 \
          "Defer write barriers of young objects"))                         \
                                                                            \
  product(intx, JVMCITraceLevel, 0,                                         \
          "Trace level for JVMCI")                                          \
                                                                            \
  product(intx, JVMCIEventLogLevel, 1,                                      \
          "Event log level for JVMCI")                                      \
                                                                            \
  product(intx, JVMCICounterSize, 0,                                        \
          "Reserved size for benchmark counters")                           \
                                                                            \
  product(bool, JVMCICountersExcludeCompiler, true,                         \
          "Exclude JVMCI compiler threads from benchmark counters")         \
                                                                            \
  develop(bool, JVMCIUseFastLocking, true,                                  \
          "Use fast inlined locking code")                                  \
                                                                            \
  product(intx, JVMCINMethodSizeLimit, (80*K)*wordSize,                     \
          "Maximum size of a compiled method.")                             \
                                                                            \
  product(intx, MethodProfileWidth, 0,                                      \
          "Number of methods to record in call profile")                    \
                                                                            \
  product(bool, ZapTLAB, false,                                             \
             "Ensure newly allocated TLABs are filled with 0xBAADBABE")     \
                                                                            \
  product(ccstr, JVMCILibPath, NULL,                                        \
          "LD path for loading the JVMCI shared library")                   \
                                                                            \
  product(ccstr, JVMCILibDumpJNIConfig, NULL,                               \
          "Dumps to the given file a description of the classes, fields "   \
          "and methods the JVMCI shared library must provide")              \
                                                                            \
  product(bool, UseJVMCINativeLibrary, false,                               \
          "Execute JVMCI Java code from a shared library "                  \
          "instead of loading it from class files and executing it "        \
          "on the HotSpot heap")                                            \
                                                                            \
  product(ccstr, JVMCINativeLibraryErrorFile, NULL,                         \
          "If an error in the JVMCI native library occurs, save the "       \
          "error data to this file"                                         \
          "[default: ./" LIBJVMCI_ERR_FILE "] (%p replaced with pid)")      \
                                                                            \
  product(ccstr, TraceClassLoadingCause, NULL,                              \
          "Print Java stack trace when loading a class whose fully"         \
          "qualified name contains this string (\"*\" matches any class).") \

// Read default values for JVMCI globals

JVMCI_FLAGS(DECLARE_DEVELOPER_FLAG, DECLARE_PD_DEVELOPER_FLAG, DECLARE_PRODUCT_FLAG, DECLARE_PD_PRODUCT_FLAG, DECLARE_NOTPRODUCT_FLAG)

// The base name for the shared library containing the JVMCI based compiler
#define JVMCI_SHARED_LIBRARY_NAME "jvmcicompiler"

class JVMCIGlobals {
 private:
  static fileStream* _jni_config_file;
 public:

  static void set_jvmci_specific_flags();

  // Returns true if jvmci flags are consistent. If not consistent,
  // an error message describing the inconsistency is printed before
  // returning false.
  static bool check_jvmci_flags_are_consistent();

  static fileStream* get_jni_config_file() { return _jni_config_file; }
};

#endif // SHARE_VM_JVMCI_JVMCI_GLOBALS_HPP
