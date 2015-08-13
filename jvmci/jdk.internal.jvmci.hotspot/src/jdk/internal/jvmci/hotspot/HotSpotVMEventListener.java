/*
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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
package jdk.internal.jvmci.hotspot;

import java.util.function.*;

import jdk.internal.jvmci.code.*;
import jdk.internal.jvmci.meta.*;

public interface HotSpotVMEventListener {

    /**
     * Compiles a method to machine code and installs it in the code cache if the compilation is
     * successful.
     *
     * @param metaspaceMethod the address of a Method metaspace object
     * @param entryBCI the BCI at which to start compiling where -1 denotes a non-OSR compilation
     *            request and all other values denote an OSR compilation request
     * @param jvmciEnv pointer to native {@code JVMCIEnv} object
     * @param id a unique identifier for this compilation
     */
    default void compileMetaspaceMethod(long metaspaceMethod, int entryBCI, long jvmciEnv, int id) {
    }

    /**
     * Notifies this client that HotSpot is running in CompileTheWorld mode and the JVMCI compiler
     * should now perform its version of CompileTheWorld.
     */
    default void notifyCompileTheWorld() throws Throwable {
    }

    /**
     * Notifies this client that the VM is shutting down.
     */
    default void notifyShutdown() {
    }

    /**
     * Notify on successful install into the CodeCache.
     *
     * @param hotSpotCodeCacheProvider
     * @param installedCode
     * @param compResult
     */
    default void notifyInstall(HotSpotCodeCacheProvider hotSpotCodeCacheProvider, InstalledCode installedCode, CompilationResult compResult) {
    }

    /**
     * Perform any extra initialization required.
     *
     * @param hotSpotJVMCIRuntime
     * @param compilerToVM the current {@link CompilerToVM instance}
     * @return the original compilerToVM instance or a proxied version.
     */
    default CompilerToVM completeInitialization(HotSpotJVMCIRuntime hotSpotJVMCIRuntime, CompilerToVM compilerToVM) {
        return compilerToVM;
    }

    /**
     * Create a custom {@link JVMCIMetaAccessContext} to be used for managing the lifetime of loaded
     * metadata. It a custom one isn't created then the default implementation will be a single
     * context with globally shared instances of {@link ResolvedJavaType} that are never released.
     *
     * @param hotSpotJVMCIRuntime
     * @param factory the factory function to create new ResolvedJavaTypes
     * @return a custom context or null
     */
    default JVMCIMetaAccessContext createMetaAccessContext(HotSpotJVMCIRuntime hotSpotJVMCIRuntime, Function<Class<?>, ResolvedJavaType> factory) {
        return null;
    }
}
