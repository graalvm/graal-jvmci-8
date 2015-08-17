/*
 * Copyright (c) 2015, 2015, Oracle and/or its affiliates. All rights reserved.
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

import jdk.internal.jvmci.code.*;
import jdk.internal.jvmci.common.*;
import jdk.internal.jvmci.compiler.*;
import jdk.internal.jvmci.compiler.Compiler;
import jdk.internal.jvmci.meta.*;
import jdk.internal.jvmci.runtime.*;
import jdk.internal.jvmci.service.*;

final class HotSpotJVMCICompilerConfig {

    private static class DummyCompilerFactory implements CompilerFactory, Compiler {

        public void compileMethod(ResolvedJavaMethod method, int entryBCI, long jvmciEnv, int id) {
            throw new JVMCIError("no JVMCI compiler selected");
        }

        public void compileTheWorld() throws Throwable {
            throw new JVMCIError("no JVMCI compiler selected");
        }

        public String getCompilerName() {
            return "<none>";
        }

        public Architecture initializeArchitecture(Architecture arch) {
            return arch;
        }

        public Compiler createCompiler(JVMCIRuntime runtime) {
            return this;
        }
    }

    private static CompilerFactory compilerFactory;

    /**
     * Called from the VM.
     */
    static Boolean selectCompiler(String compilerName) {
        for (CompilerFactory factory : Services.load(CompilerFactory.class)) {
            if (factory.getCompilerName().equals(compilerName)) {
                compilerFactory = factory;
                return Boolean.TRUE;
            }
        }

        throw new JVMCIError("JVMCI compiler '%s' not found", compilerName);
    }

    static CompilerFactory getCompilerFactory() {
        if (compilerFactory == null) {
            compilerFactory = new DummyCompilerFactory();
        }
        return compilerFactory;
    }
}
