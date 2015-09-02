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
        timedInit();
    }

    @SuppressWarnings("try")
    private static void timedInit() {
        try (InitTimer t = timer("CompilerToVMImpl.init")) {
            init();
        }
    }

    @Override
    public native int installCode(TargetDescription target, HotSpotCompiledCode compiledCode, InstalledCode code, SpeculationLog speculationLog);

    @Override
    public native HotSpotResolvedJavaMethodImpl getResolvedJavaMethodAtSlot(Class<?> holder, int slot);

    @Override
    public native byte[] getBytecode(HotSpotResolvedJavaMethodImpl method);

    @Override
    public native int getExceptionTableLength(HotSpotResolvedJavaMethodImpl method);

    @Override
    public native long getExceptionTableStart(HotSpotResolvedJavaMethodImpl method);

    @Override
    public native boolean hasBalancedMonitors(HotSpotResolvedJavaMethodImpl method);

    @Override
    public native HotSpotResolvedJavaMethodImpl findUniqueConcreteMethod(HotSpotResolvedObjectTypeImpl actualHolderType, HotSpotResolvedJavaMethodImpl method);

    @Override
    public native HotSpotResolvedObjectTypeImpl getImplementor(HotSpotResolvedObjectTypeImpl type);

    @Override
    public native HotSpotResolvedObjectTypeImpl lookupType(String name, Class<?> accessingClass, boolean eagerResolve);

    public native Object resolveConstantInPool(HotSpotConstantPool constantPool, int cpi);

    public native Object resolvePossiblyCachedConstantInPool(HotSpotConstantPool constantPool, int cpi);

    @Override
    public native int lookupNameAndTypeRefIndexInPool(HotSpotConstantPool constantPool, int cpi);

    @Override
    public native String lookupNameRefInPool(HotSpotConstantPool constantPool, int cpi);

    @Override
    public native String lookupSignatureRefInPool(HotSpotConstantPool constantPool, int cpi);

    @Override
    public native int lookupKlassRefIndexInPool(HotSpotConstantPool constantPool, int cpi);

    public native HotSpotResolvedObjectTypeImpl resolveTypeInPool(HotSpotConstantPool constantPool, int cpi);

    @Override
    public native Object lookupKlassInPool(HotSpotConstantPool constantPool, int cpi);

    @Override
    public native HotSpotResolvedJavaMethodImpl lookupMethodInPool(HotSpotConstantPool constantPool, int cpi, byte opcode);

    @Override
    public native HotSpotResolvedObjectTypeImpl resolveFieldInPool(HotSpotConstantPool constantPool, int cpi, byte opcode, long[] info);

    public native int constantPoolRemapInstructionOperandFromCache(HotSpotConstantPool constantPool, int cpi);

    @Override
    public native Object lookupAppendixInPool(HotSpotConstantPool constantPool, int cpi);

    @Override
    public native void initializeConfiguration(HotSpotVMConfig config);

    @Override
    public native HotSpotResolvedJavaMethodImpl resolveMethod(HotSpotResolvedObjectTypeImpl klassExactReceiver, HotSpotResolvedJavaMethodImpl method, HotSpotResolvedObjectTypeImpl klassCaller);

    @Override
    public native boolean hasFinalizableSubclass(HotSpotResolvedObjectTypeImpl type);

    public native boolean methodIsIgnoredBySecurityStackWalk(HotSpotResolvedJavaMethodImpl method);

    @Override
    public native HotSpotResolvedJavaMethodImpl getClassInitializer(HotSpotResolvedObjectTypeImpl type);

    @Override
    public native long getMaxCallTargetOffset(long address);

    // The HotSpot disassembler seems not to be thread safe so it's better to synchronize its usage
    @Override
    public synchronized native String disassembleCodeBlob(long codeBlob);

    @Override
    public native StackTraceElement getStackTraceElement(HotSpotResolvedJavaMethodImpl method, int bci);

    @Override
    public native Object executeInstalledCode(Object[] args, InstalledCode hotspotInstalledCode);

    @Override
    public native long[] getLineNumberTable(HotSpotResolvedJavaMethodImpl method);

    @Override
    public native long getLocalVariableTableStart(HotSpotResolvedJavaMethodImpl method);

    @Override
    public native int getLocalVariableTableLength(HotSpotResolvedJavaMethodImpl method);

    @Override
    public native void reprofile(HotSpotResolvedJavaMethodImpl method);

    @Override
    public native void invalidateInstalledCode(InstalledCode hotspotInstalledCode);

    @Override
    public native Object readUncompressedOop(long address);

    @Override
    public native void doNotInlineOrCompile(HotSpotResolvedJavaMethodImpl method);

    public synchronized native void notifyCompilationStatistics(int id, HotSpotResolvedJavaMethodImpl method, boolean osr, int processedBytecodes, long time, long timeUnitsPerSecond,
                    InstalledCode installedCode);

    public native void resetCompilationStatistics();

    public native long[] collectCounters();

    public native boolean isMature(long metaspaceMethodData);

    public native int allocateCompileId(HotSpotResolvedJavaMethodImpl method, int entryBCI);

    public String getGPUs() {
        return "";
    }

    public native boolean canInlineMethod(HotSpotResolvedJavaMethodImpl method);

    public native boolean shouldInlineMethod(HotSpotResolvedJavaMethodImpl method);

    public native boolean hasCompiledCodeForOSR(HotSpotResolvedJavaMethodImpl method, int entryBCI, int level);

    public native HotSpotStackFrameReference getNextStackFrame(HotSpotStackFrameReference frame, HotSpotResolvedJavaMethodImpl[] methods, int initialSkip);

    public native void materializeVirtualObjects(HotSpotStackFrameReference stackFrame, boolean invalidate);

    public native long getTimeStamp();

    public native String getSymbol(long metaspaceSymbol);

    public native void resolveInvokeDynamicInPool(HotSpotConstantPool constantPool, int index);

    public native void resolveInvokeHandleInPool(HotSpotConstantPool constantPool, int index);

    public native int getVtableIndexForInterface(HotSpotResolvedObjectTypeImpl type, HotSpotResolvedJavaMethodImpl method);

    public native boolean shouldDebugNonSafepoints();

    public native void writeDebugOutput(byte[] bytes, int offset, int length);

    public native void flushDebugOutput();

    public native HotSpotResolvedJavaMethodImpl getResolvedJavaMethod(Object base, long displacement);

    public native HotSpotConstantPool getConstantPool(Object base, long displacement);

    public native HotSpotResolvedObjectTypeImpl getResolvedJavaType(Object base, long displacement, boolean compressed);
}
