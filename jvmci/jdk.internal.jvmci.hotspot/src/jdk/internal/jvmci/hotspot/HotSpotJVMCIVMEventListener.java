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

import static jdk.internal.jvmci.hotspot.CompileTheWorld.Options.*;
import jdk.internal.jvmci.debug.*;
import jdk.internal.jvmci.hotspot.CompileTheWorld.*;
import jdk.internal.jvmci.service.*;

@ServiceProvider(HotSpotVMEventListener.class)
public class HotSpotJVMCIVMEventListener implements HotSpotVMEventListener {

    @Override
    public void notifyCompileTheWorld() throws Throwable {
        CompilerToVM compilerToVM = HotSpotJVMCIRuntime.runtime().getCompilerToVM();
        int iterations = CompileTheWorld.Options.CompileTheWorldIterations.getValue();
        for (int i = 0; i < iterations; i++) {
            compilerToVM.resetCompilationStatistics();
            TTY.println("CompileTheWorld : iteration " + i);
            CompileTheWorld ctw = new CompileTheWorld(CompileTheWorldClasspath.getValue(), new Config(CompileTheWorldConfig.getValue()), CompileTheWorldStartAt.getValue(),
                            CompileTheWorldStopAt.getValue(), CompileTheWorldMethodFilter.getValue(), CompileTheWorldExcludeMethodFilter.getValue(), CompileTheWorldVerbose.getValue());
            ctw.compile();
        }
        System.exit(0);
    }

    @Override
    public void notifyShutdown() {
        // nothing to do
    }

    @Override
    public void compileMetaspaceMethod(long metaspaceMethod, int entryBCI, long jvmciEnv, int id) {
        HotSpotResolvedJavaMethod method = HotSpotResolvedJavaMethodImpl.fromMetaspace(metaspaceMethod);
        CompilationTask.compileMethod(method, entryBCI, jvmciEnv, id);
    }
}
