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

JVMCI_FLAGS(MATERIALIZE_DEVELOPER_FLAG, MATERIALIZE_PD_DEVELOPER_FLAG, MATERIALIZE_PRODUCT_FLAG, MATERIALIZE_PD_PRODUCT_FLAG, MATERIALIZE_NOTPRODUCT_FLAG)

bool JVMCIGlobals::check_jvmci_flags_are_consistent() {
  bool status = true;
  if (!EnableJVMCI) {
#define JVMCI_CHECK3(type, name, doc)        JVMCI_CHECK_FLAG(name)
#define JVMCI_CHECK4(type, name, value, doc) JVMCI_CHECK_FLAG(name)
#define JVMCI_CHECK_FLAG(FLAG)                         \
    if (strcmp(#FLAG, "EnableJVMCI") && !FLAG_IS_DEFAULT(FLAG)) {                                   \
      jio_fprintf(defaultStream::error_stream(), "EnableJVMCI must be enabled to use VM option '%s'\n", #FLAG); \
      status = false; \
    }
    JVMCI_FLAGS(JVMCI_CHECK4, JVMCI_CHECK3, JVMCI_CHECK4, JVMCI_CHECK3, JVMCI_CHECK4)
#undef JVMCI_CHECK3
#undef JVMCI_CHECK4
#undef JVMCI_CHECK_FLAG
  } else {
#ifndef TIERED
    // JVMCI is only usable as a jit compiler if the VM supports tiered compilation.
#define JVMCI_CHECK_FLAG(FLAG)                         \
    if (!FLAG_IS_DEFAULT(FLAG)) {                                   \
      jio_fprintf(defaultStream::error_stream(), "VM option '%s' cannot be set in non-tiered VM\n", #FLAG); \
      status = false; \
    }
    JVMCI_CHECK_FLAG(UseJVMCICompiler)
    JVMCI_CHECK_FLAG(BootstrapJVMCI)
    JVMCI_CHECK_FLAG(PrintBootstrap)
    JVMCI_CHECK_FLAG(JVMCIThreads)
    JVMCI_CHECK_FLAG(JVMCIHostThreads)
    JVMCI_CHECK_FLAG(JVMCICountersExcludeCompiler)
#undef JVMCI_CHECK_FLAG
#endif
    if (BootstrapJVMCI && !UseJVMCICompiler) {
      warning("BootstrapJVMCI has no effect if UseJVMCICompiler is disabled");
    }
  }
  if (UseJVMCICompiler) {
    if(JVMCIThreads < 1) {
      // Check the minimum number of JVMCI compiler threads
      jio_fprintf(defaultStream::error_stream(), "JVMCIThreads of " INTX_FORMAT " is invalid; must be at least 1\n", JVMCIThreads);
      status = false;
    }
  }
  return status;
}

void JVMCIGlobals::set_jvmci_specific_flags() {
  if (UseJVMCICompiler) {
    if (FLAG_IS_DEFAULT(TypeProfileWidth)) {
      FLAG_SET_DEFAULT(TypeProfileWidth, 8);
    }
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
