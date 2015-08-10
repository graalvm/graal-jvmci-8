/*
 * Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.
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

import static jdk.internal.jvmci.inittimer.InitTimer.*;
import jdk.internal.jvmci.code.*;
import jdk.internal.jvmci.common.*;
import jdk.internal.jvmci.inittimer.*;
import jdk.internal.jvmci.meta.*;

/**
 * Entries into the HotSpot VM from Java code.
 */
public class CompilerToVMImpl implements CompilerToVM {

    /**
     * Initializes the native part of the JVMCI runtime.
     */
    private static native void init();

    static {
        try (InitTimer t = timer("CompilerToVMImpl.init")) {
            init();
        }
    }

    @Override
    public native int installCode(HotSpotCompiledCode compiledCode, InstalledCode code, SpeculationLog speculationLog);

    @Override
    public native long getMetaspaceMethod(Class<?> holder, int slot);

    @Override
    public native byte[] getBytecode(long metaspaceMethod);

    @Override
    public native int getExceptionTableLength(long metaspaceMethod);

    @Override
    public native long getExceptionTableStart(long metaspaceMethod);

    @Override
    public native boolean hasBalancedMonitors(long metaspaceMethod);

    @Override
    public native long findUniqueConcreteMethod(long actualHolderMetaspaceKlass, long metaspaceMethod);

    @Override
    public native long getKlassImplementor(long metaspaceKlass);

    @Override
    public native long lookupType(String name, Class<?> accessingClass, boolean eagerResolve);

    public native Object resolveConstantInPool(long metaspaceConstantPool, int cpi);

    public Object resolvePossiblyCachedConstantInPool(long metaspaceConstantPool, int cpi) {
        JVMCIError.guarantee(!HotSpotConstantPool.Options.UseConstantPoolCacheJavaCode.getValue(), "");
        return resolvePossiblyCachedConstantInPool0(metaspaceConstantPool, cpi);
    }

    private native Object resolvePossiblyCachedConstantInPool0(long metaspaceConstantPool, int cpi);

    @Override
    public native int lookupNameAndTypeRefIndexInPool(long metaspaceConstantPool, int cpi);

    @Override
    public String lookupNameRefInPool(long metaspaceConstantPool, int cpi) {
        JVMCIError.guarantee(!HotSpotConstantPool.Options.UseConstantPoolCacheJavaCode.getValue(), "");
        return lookupNameRefInPool0(metaspaceConstantPool, cpi);
    }

    private native String lookupNameRefInPool0(long metaspaceConstantPool, int cpi);

    @Override
    public String lookupSignatureRefInPool(long metaspaceConstantPool, int cpi) {
        JVMCIError.guarantee(!HotSpotConstantPool.Options.UseConstantPoolCacheJavaCode.getValue(), "");
        return lookupSignatureRefInPool0(metaspaceConstantPool, cpi);
    }

    private native String lookupSignatureRefInPool0(long metaspaceConstantPool, int cpi);

    @Override
    public int lookupKlassRefIndexInPool(long metaspaceConstantPool, int cpi) {
        JVMCIError.guarantee(!HotSpotConstantPool.Options.UseConstantPoolCacheJavaCode.getValue(), "");
        return lookupKlassRefIndexInPool0(metaspaceConstantPool, cpi);
    }

    private native int lookupKlassRefIndexInPool0(long metaspaceConstantPool, int cpi);

    public native long constantPoolKlassAt(long metaspaceConstantPool, int cpi);

    @Override
    public native long lookupKlassInPool(long metaspaceConstantPool, int cpi);

    @Override
    public native long lookupMethodInPool(long metaspaceConstantPool, int cpi, byte opcode);

    @Override
    public native long resolveField(long metaspaceConstantPool, int cpi, byte opcode, long[] info);

    public int constantPoolRemapInstructionOperandFromCache(long metaspaceConstantPool, int cpi) {
        JVMCIError.guarantee(!HotSpotConstantPool.Options.UseConstantPoolCacheJavaCode.getValue(), "");
        return constantPoolRemapInstructionOperandFromCache0(metaspaceConstantPool, cpi);
    }

    private native int constantPoolRemapInstructionOperandFromCache0(long metaspaceConstantPool, int cpi);

    @Override
    public Object lookupAppendixInPool(long metaspaceConstantPool, int cpi) {
        JVMCIError.guarantee(!HotSpotConstantPool.Options.UseConstantPoolCacheJavaCode.getValue(), "");
        return lookupAppendixInPool0(metaspaceConstantPool, cpi);
    }

    private native Object lookupAppendixInPool0(long metaspaceConstantPool, int cpi);

    @Override
    public native void initializeConfiguration(HotSpotVMConfig config);

    @Override
    public native long resolveMethod(long metaspaceKlassExactReceiver, long metaspaceMethod, long metaspaceKlassCaller);

    @Override
    public native boolean hasFinalizableSubclass(long metaspaceKlass);

    public native boolean methodIsIgnoredBySecurityStackWalk(long metaspaceMethod);

    @Override
    public native long getClassInitializer(long metaspaceKlass);

    @Override
    public native long getMaxCallTargetOffset(long address);

    // The HotSpot disassembler seems not to be thread safe so it's better to synchronize its usage
    @Override
    public synchronized native String disassembleCodeBlob(long codeBlob);

    @Override
    public native StackTraceElement getStackTraceElement(long metaspaceMethod, int bci);

    @Override
    public native Object executeCompiledMethodVarargs(Object[] args, InstalledCode hotspotInstalledCode);

    @Override
    public native long[] getLineNumberTable(long metaspaceMethod);

    @Override
    public native long getLocalVariableTableStart(long metaspaceMethod);

    @Override
    public native int getLocalVariableTableLength(long metaspaceMethod);

    @Override
    public native String getFileName(HotSpotResolvedJavaType method);

    @Override
    public native void reprofile(long metaspaceMethod);

    @Override
    public native void invalidateInstalledCode(InstalledCode hotspotInstalledCode);

    @Override
    public native Class<?> getJavaMirror(long metaspaceKlass);

    @Override
    public native long readUnsafeKlassPointer(Object o);

    @Override
    public native Object readUncompressedOop(long address);

    @Override
    public native void doNotInlineOrCompile(long metaspaceMethod);

    @Override
    public Object executeCompiledMethod(Object arg1, Object arg2, Object arg3, InstalledCode hotspotInstalledCode) throws InvalidInstalledCodeException {
        return executeCompiledMethodVarargs(new Object[]{arg1, arg2, arg3}, hotspotInstalledCode);
    }

    public synchronized native void notifyCompilationStatistics(int id, HotSpotResolvedJavaMethod method, boolean osr, int processedBytecodes, long time, long timeUnitsPerSecond,
                    InstalledCode installedCode);

    public native void resetCompilationStatistics();

    public native long[] collectCounters();

    public native boolean isMature(long method);

    public native int allocateCompileId(long metaspaceMethod, int entryBCI);

    public String getGPUs() {
        return "";
    }

    public native boolean canInlineMethod(long metaspaceMethod);

    public native boolean shouldInlineMethod(long metaspaceMethod);

    public native boolean hasCompiledCodeForOSR(long metaspaceMethod, int entryBCI, int level);

    public native HotSpotStackFrameReference getNextStackFrame(HotSpotStackFrameReference frame, long[] methods, int initialSkip);

    public native void materializeVirtualObjects(HotSpotStackFrameReference stackFrame, boolean invalidate);

    public native long getTimeStamp();

    public String getSymbol(long metaspaceSymbol) {
        JVMCIError.guarantee(!HotSpotConstantPool.Options.UseConstantPoolCacheJavaCode.getValue(), "");
        return getSymbol0(metaspaceSymbol);
    }

    private native String getSymbol0(long metaspaceSymbol);

    public native void resolveInvokeDynamic(long metaspaceConstantPool, int index);

    public native void resolveInvokeHandle(long metaspaceConstantPool, int index);

    public native int getVtableIndexForInterface(long metaspaceKlass, long metaspaceMethod);

    public native boolean shouldDebugNonSafepoints();

    public native void writeDebugOutput(byte[] bytes, int offset, int length);

    public native void flushDebugOutput();
}
