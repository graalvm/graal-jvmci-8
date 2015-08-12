/*
 * Copyright (c) 2011, Oracle and/or its affiliates. All rights reserved.
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

import java.lang.reflect.*;

import jdk.internal.jvmci.code.*;
import jdk.internal.jvmci.hotspotvmconfig.*;
import jdk.internal.jvmci.meta.*;
import sun.misc.*;

/**
 * Calls from Java into HotSpot. The behavior of all the methods in this class that take a metaspace
 * pointer as an argument (e.g., {@link #getExceptionTableStart(long)}) is undefined if the argument
 * does not denote a valid metaspace object.
 */
public interface CompilerToVM {

    /**
     * Copies the original bytecode of {@code metaspaceMethod} into a new byte array and returns it.
     *
     * @return a new byte array containing the original bytecode of {@code metaspaceMethod}
     */
    byte[] getBytecode(long metaspaceMethod);

    /**
     * Gets the number of entries in {@code metaspaceMethod}'s exception handler table or 0 if it
     * has not exception handler table.
     */
    int getExceptionTableLength(long metaspaceMethod);

    /**
     * Gets the address of the first entry in {@code metaspaceMethod}'s exception handler table.
     *
     * Each entry is a native object described by these fields:
     *
     * <ul>
     * <li>{@link HotSpotVMConfig#exceptionTableElementSize}</li>
     * <li>{@link HotSpotVMConfig#exceptionTableElementStartPcOffset}</li>
     * <li>{@link HotSpotVMConfig#exceptionTableElementEndPcOffset}</li>
     * <li>{@link HotSpotVMConfig#exceptionTableElementHandlerPcOffset}</li>
     * <li>{@link HotSpotVMConfig#exceptionTableElementCatchTypeIndexOffset}
     * </ul>
     *
     * @return 0 if {@code metaspaceMethod} has no exception handlers (i.e.
     *         {@code getExceptionTableLength(metaspaceMethod) == 0})
     */
    long getExceptionTableStart(long metaspaceMethod);

    /**
     * Determines if {@code metaspaceMethod} has balanced monitors.
     */
    boolean hasBalancedMonitors(long metaspaceMethod);

    /**
     * Determines if {@code metaspaceMethod} can be inlined. A method may not be inlinable for a
     * number of reasons such as:
     * <ul>
     * <li>a CompileOracle directive may prevent inlining or compilation of methods</li>
     * <li>the method may have a bytecode breakpoint set</li>
     * <li>the method may have other bytecode features that require special handling by the VM</li>
     * </ul>
     */
    boolean canInlineMethod(long metaspaceMethod);

    /**
     * Determines if {@code metaspaceMethod} should be inlined at any cost. This could be because:
     * <ul>
     * <li>a CompileOracle directive may forces inlining of this methods</li>
     * <li>an annotation forces inlining of this method</li>
     * </ul>
     */
    boolean shouldInlineMethod(long metaspaceMethod);

    /**
     * Used to implement {@link ResolvedJavaType#findUniqueConcreteMethod(ResolvedJavaMethod)}.
     *
     * @param metaspaceMethod the metaspace Method on which to base the search
     * @param actualHolderMetaspaceKlass the best known type of receiver
     * @return the metaspace Method result or 0 is there is no unique concrete method for
     *         {@code metaspaceMethod}
     */
    long findUniqueConcreteMethod(long actualHolderMetaspaceKlass, long metaspaceMethod);

    /**
     * Gets the implementor for the interface class {@code metaspaceKlass}.
     *
     * @return the implementor if there is a single implementor, 0 if there is no implementor, or
     *         {@code metaspaceKlass} itself if there is more than one implementor
     */
    long getKlassImplementor(long metaspaceKlass);

    /**
     * Determines if {@code metaspaceMethod} is ignored by security stack walks.
     */
    boolean methodIsIgnoredBySecurityStackWalk(long metaspaceMethod);

    /**
     * Converts a name to a metaspace Klass.
     *
     * @param name a well formed Java type in {@linkplain JavaType#getName() internal} format
     * @param accessingClass the context of resolution (must not be null)
     * @param resolve force resolution to a {@link ResolvedJavaType}. If true, this method will
     *            either return a {@link ResolvedJavaType} or throw an exception
     * @return the metaspace Klass for {@code name} or 0 if resolution failed and
     *         {@code resolve == false}
     * @throws LinkageError if {@code resolve == true} and the resolution failed
     */
    long lookupType(String name, Class<?> accessingClass, boolean resolve);

    /**
     * Resolves the entry at index {@code cpi} in {@code metaspaceConstantPool} to an object.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry that can be
     * resolved to an object.
     */
    Object resolveConstantInPool(long metaspaceConstantPool, int cpi);

    /**
     * Resolves the entry at index {@code cpi} in {@code metaspaceConstantPool} to an object,
     * looking in the constant pool cache first.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry that can be
     * resolved to an object.
     */
    Object resolvePossiblyCachedConstantInPool(long metaspaceConstantPool, int cpi);

    /**
     * Gets the {@code JVM_CONSTANT_NameAndType} index from the entry at index {@code cpi} in
     * {@code metaspaceConstantPool}.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry containing a
     * {@code JVM_CONSTANT_NameAndType} index.
     */
    int lookupNameAndTypeRefIndexInPool(long metaspaceConstantPool, int cpi);

    /**
     * Gets the name of the {@code JVM_CONSTANT_NameAndType} entry at index {@code cpi} in
     * {@code metaspaceConstantPool}.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote a
     * {@code JVM_CONSTANT_NameAndType} entry.
     */
    String lookupNameRefInPool(long metaspaceConstantPool, int cpi);

    /**
     * Gets the signature of the {@code JVM_CONSTANT_NameAndType} entry at index {@code cpi} in
     * {@code metaspaceConstantPool}.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote a
     * {@code JVM_CONSTANT_NameAndType} entry.
     */
    String lookupSignatureRefInPool(long metaspaceConstantPool, int cpi);

    /**
     * Gets the {@code JVM_CONSTANT_Class} index from the entry at index {@code cpi} in
     * {@code metaspaceConstantPool}.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry containing a
     * {@code JVM_CONSTANT_Class} index.
     */
    int lookupKlassRefIndexInPool(long metaspaceConstantPool, int cpi);

    /**
     * Looks up a class denoted by the {@code JVM_CONSTANT_Class} entry at index {@code cpi} in
     * {@code metaspaceConstantPool}. This method does not perform any resolution.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote a
     * {@code JVM_CONSTANT_Class} entry.
     *
     * @return a metaspace Klass for a resolved class entry (tagged by
     *         {@link HotSpotVMConfig#compilerToVMKlassTag}) or a metaspace Symbol otherwise (tagged
     *         by {@link HotSpotVMConfig#compilerToVMSymbolTag})
     */
    long lookupKlassInPool(long metaspaceConstantPool, int cpi);

    /**
     * Looks up a method denoted by the entry at index {@code cpi} in {@code metaspaceConstantPool}.
     * This method does not perform any resolution.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry representing
     * a method.
     *
     * @param opcode the opcode of the instruction for which the lookup is being performed or
     *            {@code -1}. If non-negative, then resolution checks specific to the bytecode it
     *            denotes are performed if the method is already resolved. Should any of these
     *            checks fail, 0 is returned.
     * @return a metaspace Method for a resolved method entry, 0 otherwise
     */
    long lookupMethodInPool(long metaspaceConstantPool, int cpi, byte opcode);

    /**
     * Ensures that the type referenced by the specified {@code JVM_CONSTANT_InvokeDynamic} entry at
     * index {@code cpi} in {@code metaspaceConstantPool} is loaded and initialized.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote a
     * {@code JVM_CONSTANT_InvokeDynamic} entry.
     */
    void resolveInvokeDynamicInPool(long metaspaceConstantPool, int cpi);

    /**
     * Ensures that the type referenced by the entry for a <a
     * href="https://docs.oracle.com/javase/specs/jvms/se8/html/jvms-2.html#jvms-2.9">signature
     * polymorphic</a> method at index {@code cpi} in {@code metaspaceConstantPool} is loaded and
     * initialized.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry representing
     * a signature polymorphic method.
     */
    void resolveInvokeHandleInPool(long metaspaceConstantPool, int cpi);

    /**
     * Gets the resolved metaspace Klass denoted by the entry at index {@code cpi} in
     * {@code metaspaceConstantPool}.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry representing
     * a class.
     *
     * @throws LinkageError if resolution failed
     */
    long resolveKlassInPool(long metaspaceConstantPool, int cpi) throws LinkageError;

    /**
     * Looks up and attempts to resolve the {@code JVM_CONSTANT_Field} entry at index {@code cpi} in
     * {@code metaspaceConstantPool}. The values returned in {@code info} are:
     *
     * <pre>
     *     [(int) flags,   // only valid if field is resolved
     *      (int) offset]  // only valid if field is resolved
     * </pre>
     *
     * The behavior of this method is undefined if {@code cpi} does not denote a
     * {@code JVM_CONSTANT_Field} entry.
     *
     * @param info an array in which the details of the field are returned
     * @return the metaspace Klass defining the field if resolution is successful, 0 otherwise
     */
    long resolveFieldInPool(long metaspaceConstantPool, int cpi, byte opcode, long[] info);

    /**
     * Converts {@code cpci} from an index into the cache for {@code metaspaceConstantPool} to an
     * index directly into {@code metaspaceConstantPool}.
     *
     * The behavior of this method is undefined if {@code ccpi} is an invalid constant pool cache
     * index.
     */
    int constantPoolRemapInstructionOperandFromCache(long metaspaceConstantPool, int cpci);

    /**
     * Gets the appendix object (if any) associated with the entry at index {@code cpi} in
     * {@code metaspaceConstantPool}.
     */
    Object lookupAppendixInPool(long metaspaceConstantPool, int cpi);

    /**
     * Installs the result of a compilation into the code cache.
     *
     * @param compiledCode the result of a compilation
     * @param code the details of the installed CodeBlob are written to this object
     * @return the outcome of the installation which will be one of
     *         {@link HotSpotVMConfig#codeInstallResultOk},
     *         {@link HotSpotVMConfig#codeInstallResultCacheFull},
     *         {@link HotSpotVMConfig#codeInstallResultCodeTooLarge},
     *         {@link HotSpotVMConfig#codeInstallResultDependenciesFailed} or
     *         {@link HotSpotVMConfig#codeInstallResultDependenciesInvalid}.
     */
    int installCode(HotSpotCompiledCode compiledCode, InstalledCode code, SpeculationLog speculationLog);

    /**
     * Notifies the VM of statistics for a completed compilation.
     *
     * @param id the identifier of the compilation
     * @param method the method compiled
     * @param osr specifies if the compilation was for on-stack-replacement
     * @param processedBytecodes the number of bytecodes processed during the compilation, including
     *            the bytecodes of all inlined methods
     * @param time the amount time spent compiling {@code method}
     * @param timeUnitsPerSecond the granularity of the units for the {@code time} value
     * @param installedCode the nmethod installed as a result of the compilation
     */
    void notifyCompilationStatistics(int id, HotSpotResolvedJavaMethod method, boolean osr, int processedBytecodes, long time, long timeUnitsPerSecond, InstalledCode installedCode);

    /**
     * Resets all compilation statistics.
     */
    void resetCompilationStatistics();

    /**
     * Initializes the fields of {@code config}.
     */
    void initializeConfiguration(HotSpotVMConfig config);

    /**
     * Resolves the implementation of {@code metaspaceMethod} for virtual dispatches on objects of
     * dynamic type {@code metaspaceKlassExactReceiver}. This resolution process only searches "up"
     * the class hierarchy of {@code metaspaceKlassExactReceiver}.
     *
     * @param metaspaceKlassCaller the caller or context type used to perform access checks
     * @return the link-time resolved method (might be abstract) or {@code 0} if it can not be
     *         linked
     */
    long resolveMethod(long metaspaceKlassExactReceiver, long metaspaceMethod, long metaspaceKlassCaller);

    /**
     * Gets the static initializer of {@code metaspaceKlass}.
     *
     * @return 0 if {@code metaspaceKlass} has no static initialize
     */
    long getClassInitializer(long metaspaceKlass);

    /**
     * Determines if {@code metaspaceKlass} or any of its currently loaded subclasses overrides
     * {@code Object.finalize()}.
     */
    boolean hasFinalizableSubclass(long metaspaceKlass);

    /**
     * Gets the metaspace Method corresponding to {@code holder} and slot number {@code slot} (i.e.
     * {@link Method#slot} or {@link Constructor#slot}).
     */
    @SuppressWarnings("javadoc")
    long getMetaspaceMethod(Class<?> holder, int slot);

    /**
     * Gets the maximum absolute offset of a PC relative call to {@code address} from any position
     * in the code cache.
     *
     * @param address an address that may be called from any code in the code cache
     * @return -1 if {@code address == 0}
     */
    long getMaxCallTargetOffset(long address);

    /**
     * Gets a textual disassembly of {@code codeBlob}.
     *
     * @return a non-zero length string containing a disassembly of {@code codeBlob} or null if
     *         {@code codeBlob} could not be disassembled for some reason
     */
    String disassembleCodeBlob(long codeBlob);

    /**
     * Gets a stack trace element for {@code metaspaceMethod} at bytecode index {@code bci}.
     */
    StackTraceElement getStackTraceElement(long metaspaceMethod, int bci);

    /**
     * Executes some {@code installedCode} with arguments {@code args}.
     *
     * @return the result of executing {@code installedCode}
     * @throws InvalidInstalledCodeException if {@code installedCode} has been invalidated
     */
    Object executeInstalledCode(Object[] args, InstalledCode installedCode) throws InvalidInstalledCodeException;

    /**
     * Gets the line number table for {@code metaspaceMethod}. The line number table is encoded as
     * (bci, source line number) pairs.
     *
     * @return the line number table for {@code metaspaceMethod} or null if it doesn't have one
     */
    long[] getLineNumberTable(long metaspaceMethod);

    /**
     * Gets the number of entries in the local variable table for {@code metaspaceMethod}.
     *
     * @return the number of entries in the local variable table for {@code metaspaceMethod}
     */
    int getLocalVariableTableLength(long metaspaceMethod);

    /**
     * Gets the address of the first entry in the local variable table for {@code metaspaceMethod}.
     *
     * Each entry is a native object described by these fields:
     *
     * <ul>
     * <li>{@link HotSpotVMConfig#localVariableTableElementSize}</li>
     * <li>{@link HotSpotVMConfig#localVariableTableElementLengthOffset}</li>
     * <li>{@link HotSpotVMConfig#localVariableTableElementNameCpIndexOffset}</li>
     * <li>{@link HotSpotVMConfig#localVariableTableElementDescriptorCpIndexOffset}</li>
     * <li>{@link HotSpotVMConfig#localVariableTableElementSignatureCpIndexOffset}
     * <li>{@link HotSpotVMConfig#localVariableTableElementSlotOffset}
     * <li>{@link HotSpotVMConfig#localVariableTableElementStartBciOffset}
     * </ul>
     *
     * @return 0 if {@code metaspaceMethod} does not have a local variable table
     */
    long getLocalVariableTableStart(long metaspaceMethod);

    /**
     * Gets the {@link Class} mirror associated with {@code metaspaceKlass}.
     */
    Class<?> getJavaMirror(long metaspaceKlass);

    /**
     * Reads an object pointer within a VM data structure. That is, any {@link HotSpotVMField} whose
     * {@link HotSpotVMField#type() type} is {@code "oop"} (e.g.,
     * {@code ArrayKlass::_component_mirror}, {@code Klass::_java_mirror},
     * {@code JavaThread::_threadObj}).
     *
     * Note that {@link Unsafe#getObject(Object, long)} cannot be used for this since it does a
     * {@code narrowOop} read if the VM is using compressed oops whereas oops within VM data
     * structures are (currently) always uncompressed.
     *
     * @param address address of an oop field within a VM data structure
     */
    Object readUncompressedOop(long address);

    /**
     * Determines if {@code metaspaceMethod} should not be inlined or compiled.
     */
    void doNotInlineOrCompile(long metaspaceMethod);

    /**
     * Invalidates the profiling information for {@code metaspaceMethod} and (re)initializes it such
     * that profiling restarts upon its next invocation.
     */
    void reprofile(long metaspaceMethod);

    /**
     * Invalidates {@code installedCode} such that {@link InvalidInstalledCodeException} will be
     * raised the next time {@code installedCode} is executed.
     */
    void invalidateInstalledCode(InstalledCode installedCode);

    /**
     * Collects the current values of all JVMCI benchmark counters, summed up over all threads.
     */
    long[] collectCounters();

    /**
     * Determines if {@code metaspaceMethodData} is mature.
     */
    boolean isMature(long metaspaceMethodData);

    /**
     * Generate a unique id to identify the result of the compile.
     */
    int allocateCompileId(long metaspaceMethod, int entryBCI);

    /**
     * Gets the names of the supported GPU architectures.
     *
     * @return a comma separated list of names
     */
    String getGPUs();

    /**
     * Determines if {@code metaspaceMethod} has OSR compiled code identified by {@code entryBCI}
     * for compilation level {@code level}.
     */
    boolean hasCompiledCodeForOSR(long metaspaceMethod, int entryBCI, int level);

    /**
     * Fetch the time stamp used for printing inside hotspot. It's relative to VM start so that all
     * events can be ordered.
     *
     * @return milliseconds since VM start
     */
    long getTimeStamp();

    /**
     * Gets the value of {@code metaspaceSymbol} as a String.
     */
    String getSymbol(long metaspaceSymbol);

    /**
     * Looks for the next Java stack frame matching an entry in {@code methods}.
     *
     * @param frame the starting point of the search, where {@code null} refers to the topmost frame
     * @param methods the metaspace methods to look for, where {@code null} means that any frame is
     *            returned
     * @return the frame, or {@code null} if the end of the stack was reached during the search
     */
    HotSpotStackFrameReference getNextStackFrame(HotSpotStackFrameReference frame, long[] methods, int initialSkip);

    /**
     * Materializes all virtual objects within {@code stackFrame} updates its locals.
     *
     * @param invalidate if {@code true}, the compiled method for the stack frame will be
     *            invalidated.
     */
    void materializeVirtualObjects(HotSpotStackFrameReference stackFrame, boolean invalidate);

    /**
     * Gets the v-table index for interface method {@code metaspaceMethod} in the receiver type
     * {@code metaspaceKlass} or {@link HotSpotVMConfig#invalidVtableIndex} if
     * {@code metaspaceMethod} is not in {@code metaspaceKlass}'s v-table.
     */
    int getVtableIndexForInterface(long metaspaceKlass, long metaspaceMethod);

    /**
     * Determines if debug info should also be emitted at non-safepoint locations.
     */
    boolean shouldDebugNonSafepoints();

    /**
     * Writes {@code length} bytes from {@code buf} starting at offset {@code offset} to the
     * HotSpot's log stream. No range checking is performed.
     */
    void writeDebugOutput(byte[] bytes, int offset, int length);

    /**
     * Flush HotSpot's log stream.
     */
    void flushDebugOutput();
}
