/*
 * Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.
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
#include "jvmci/jvmci_globals.hpp"
#include "utilities/defaultStream.hpp"
#include "runtime/globals_extension.hpp"
#include "runtime/arguments.hpp"
#ifdef TARGET_OS_FAMILY_linux
# include "os_linux.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_solaris
# include "os_solaris.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_windows
# include "os_windows.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_aix
# include "os_aix.inline.hpp"
#endif
#ifdef TARGET_OS_FAMILY_bsd
# include "os_bsd.inline.hpp"
#endif

JVMCI_FLAGS(MATERIALIZE_DEVELOPER_FLAG, MATERIALIZE_PD_DEVELOPER_FLAG, MATERIALIZE_PRODUCT_FLAG, MATERIALIZE_PD_PRODUCT_FLAG, MATERIALIZE_NOTPRODUCT_FLAG)

bool JVMCIGlobals::check_jvmci_flags_are_consistent() {
#ifndef PRODUCT
#define APPLY_JVMCI_FLAGS(params3, params4) \
  JVMCI_FLAGS(params4, params3, params4, params3, params4)
#define JVMCI_DECLARE_CHECK4(type, name, value, doc) bool name##checked = false;
#define JVMCI_DECLARE_CHECK3(type, name, doc)        bool name##checked = false;
#define JVMCI_FLAG_CHECKED(name)                          name##checked = true;
  APPLY_JVMCI_FLAGS(JVMCI_DECLARE_CHECK3, JVMCI_DECLARE_CHECK4)
#else
#define JVMCI_FLAG_CHECKED(name)
#endif

  // Checks that a given flag is not set if a given guard flag is false.
#define CHECK_NOT_SET(FLAG, GUARD)                     \
  JVMCI_FLAG_CHECKED(FLAG)                             \
  if (!GUARD && !FLAG_IS_DEFAULT(FLAG)) {              \
    jio_fprintf(defaultStream::error_stream(),         \
        "Improperly specified VM option '%s': '%s' must be enabled\n", #FLAG, #GUARD); \
    return false;                                      \
  }

  JVMCI_FLAG_CHECKED(UseJVMCICompiler)
  JVMCI_FLAG_CHECKED(EnableJVMCI)

  CHECK_NOT_SET(BootstrapJVMCI,   UseJVMCICompiler)
  CHECK_NOT_SET(PrintBootstrap,   UseJVMCICompiler)
  CHECK_NOT_SET(JVMCIThreads,     UseJVMCICompiler)
  CHECK_NOT_SET(JVMCIHostThreads, UseJVMCICompiler)

  if (UseJVMCICompiler) {
    if (!FLAG_IS_DEFAULT(EnableJVMCI) && !EnableJVMCI) {
      jio_fprintf(defaultStream::error_stream(),
          "Improperly specified VM option UseJVMCICompiler: EnableJVMCI cannot be disabled\n");
      return false;
    }
    FLAG_SET_DEFAULT(EnableJVMCI, true);
  }
  CHECK_NOT_SET(UseJVMCIClassLoader,          EnableJVMCI)
  CHECK_NOT_SET(CodeInstallSafepointChecks,   EnableJVMCI)
  CHECK_NOT_SET(JVMCITraceLevel,              EnableJVMCI)
  CHECK_NOT_SET(JVMCICounterSize,             EnableJVMCI)
  CHECK_NOT_SET(JVMCICountersExcludeCompiler, EnableJVMCI)
  CHECK_NOT_SET(JVMCIUseFastLocking,          EnableJVMCI)
  CHECK_NOT_SET(JVMCINMethodSizeLimit,        EnableJVMCI)
  CHECK_NOT_SET(MethodProfileWidth,           EnableJVMCI)
  CHECK_NOT_SET(JVMCIPrintProperties,         EnableJVMCI)
  CHECK_NOT_SET(TraceUncollectedSpeculations, EnableJVMCI)

#ifndef PRODUCT
#define JVMCI_CHECK4(type, name, value, doc) assert(name##checked, #name " flag not checked");
#define JVMCI_CHECK3(type, name, doc)        assert(name##checked, #name " flag not checked");
  // Ensures that all JVMCI flags are checked by this method.
  APPLY_JVMCI_FLAGS(JVMCI_CHECK3, JVMCI_CHECK4)
#undef APPLY_JVMCI_FLAGS
#undef JVMCI_DECLARE_CHECK3
#undef JVMCI_DECLARE_CHECK4
#undef JVMCI_CHECK3
#undef JVMCI_CHECK4
#undef JVMCI_FLAG_CHECKED
#endif
#undef CHECK_NOT_SET
  if (UseJVMCICompiler) {
    if(JVMCIThreads < 1) {
      // Check the minimum number of JVMCI compiler threads
      jio_fprintf(defaultStream::error_stream(), "JVMCIThreads of " INTX_FORMAT " is invalid; must be at least 1\n", JVMCIThreads);
      return false;
    }
  }
  return true;
}

void JVMCIGlobals::set_jvmci_specific_flags() {
  if (FLAG_IS_DEFAULT(UseJVMCICompiler) && !UseJVMCICompiler) {
    if (Arguments::get_property("jvmci.class.path.append") != NULL) {
      // If jvmci.class.path.append is defined then
      // assume there is a JVMCI compiler.
      FLAG_SET_ERGO(bool, UseJVMCICompiler, true);
    } else {
      // If lib/jvmci contains at least one jar apart from
      // jvmci-api.jar and jvmci-hotspot.jar, then
      // assume there is a JVMCI compiler.
      char jvmciDir[JVM_MAXPATHLEN];
      const char* fileSep = os::file_separator();
      jio_snprintf(jvmciDir, sizeof(jvmciDir), "%s%slib%sjvmci", Arguments::get_java_home(), fileSep, fileSep);
      DIR* dir = os::opendir(jvmciDir);
      if (dir != NULL) {
        /* Scan the directory for jars */
        struct dirent *entry;
        char *dbuf = NEW_C_HEAP_ARRAY(char, os::readdir_buf_size(jvmciDir), mtInternal);
        while ((entry = os::readdir(dir, (dirent *) dbuf)) != NULL) {
          const char* name = entry->d_name;
          const char* ext = name + strlen(name) - 4;
          if (ext > name && os::file_name_strcmp(ext, ".jar") == 0) {
            if (strcmp(name, "jvmci-api.jar") != 0 && strcmp(name, "jvmci-hotspot.jar") != 0) {
              FLAG_SET_ERGO(bool, UseJVMCICompiler, true);
              break;
            }
          }
        }
        FREE_C_HEAP_ARRAY(char, dbuf, mtInternal);
        os::closedir(dir);
      }
    }
  }

  if (UseJVMCICompiler) {
    if (FLAG_IS_DEFAULT(TypeProfileWidth)) {
      FLAG_SET_DEFAULT(TypeProfileWidth, 8);
    }
    // Adjust the on stack replacement percentage to avoid early
    // OSR compilations while JVMCI itself is warming up
    if (FLAG_IS_DEFAULT(OnStackReplacePercentage)) {
      FLAG_SET_DEFAULT(OnStackReplacePercentage, 933);
    }
    if (FLAG_IS_DEFAULT(ReservedCodeCacheSize)) {
      FLAG_SET_DEFAULT(ReservedCodeCacheSize, 64*M);
    }
    if (FLAG_IS_DEFAULT(InitialCodeCacheSize)) {
      FLAG_SET_DEFAULT(InitialCodeCacheSize, 16*M);
    }
    if (FLAG_IS_DEFAULT(MetaspaceSize)) {
      FLAG_SET_DEFAULT(MetaspaceSize, 12*M);
    }
    if (FLAG_IS_DEFAULT(NewSizeThreadIncrease)) {
      FLAG_SET_DEFAULT(NewSizeThreadIncrease, 4*K);
    }
  }
  if (!ScavengeRootsInCode) {
    warning("forcing ScavengeRootsInCode non-zero because JVMCI is enabled");
    ScavengeRootsInCode = 1;
  }
}
