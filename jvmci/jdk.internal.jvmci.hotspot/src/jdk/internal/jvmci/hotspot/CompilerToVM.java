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
 * pointer as an argument (e.g., {@link #getExceptionTableStart(HotSpotResolvedJavaMethodImpl)}) is
 * undefined if the argument does not denote a valid metaspace object.
 */
public interface CompilerToVM {

    /**
     * Copies the original bytecode of {@code method} into a new byte array and returns it.
     *
     * @return a new byte array containing the original bytecode of {@code method}
     */
    byte[] getBytecode(HotSpotResolvedJavaMethodImpl method);

    /**
     * Gets the number of entries in {@code method}'s exception handler table or 0 if it has not
     * exception handler table.
     */
    int getExceptionTableLength(HotSpotResolvedJavaMethodImpl method);

    /**
     * Gets the address of the first entry in {@code method}'s exception handler table.
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
     * @return 0 if {@code method} has no exception handlers (i.e.
     *         {@code getExceptionTableLength(method) == 0})
     */
    long getExceptionTableStart(HotSpotResolvedJavaMethodImpl method);

    /**
     * Determines if {@code method} has balanced monitors.
     */
    boolean hasBalancedMonitors(HotSpotResolvedJavaMethodImpl method);

    /**
     * Determines if {@code method} can be inlined. A method may not be inlinable for a number of
     * reasons such as:
     * <ul>
     * <li>a CompileOracle directive may prevent inlining or compilation of methods</li>
     * <li>the method may have a bytecode breakpoint set</li>
     * <li>the method may have other bytecode features that require special handling by the VM</li>
     * </ul>
     */
    boolean canInlineMethod(HotSpotResolvedJavaMethodImpl method);

    /**
     * Determines if {@code method} should be inlined at any cost. This could be because:
     * <ul>
     * <li>a CompileOracle directive may forces inlining of this methods</li>
     * <li>an annotation forces inlining of this method</li>
     * </ul>
     */
    boolean shouldInlineMethod(HotSpotResolvedJavaMethodImpl method);

    /**
     * Used to implement {@link ResolvedJavaType#findUniqueConcreteMethod(ResolvedJavaMethod)}.
     *
     * @param method the method on which to base the search
     * @param actualHolderKlass the best known type of receiver
     * @return the method result or 0 is there is no unique concrete method for {@code method}
     */
    HotSpotResolvedJavaMethodImpl findUniqueConcreteMethod(HotSpotResolvedObjectTypeImpl actualHolderKlass, HotSpotResolvedJavaMethodImpl method);

    /**
     * Gets the implementor for the interface class {@code metaspaceKlass}.
     *
     * @return the implementor if there is a single implementor, 0 if there is no implementor, or
     *         {@code metaspaceKlass} itself if there is more than one implementor
     */
    HotSpotResolvedObjectTypeImpl getKlassImplementor(HotSpotResolvedObjectTypeImpl klass);

    /**
     * Determines if {@code method} is ignored by security stack walks.
     */
    boolean methodIsIgnoredBySecurityStackWalk(HotSpotResolvedJavaMethodImpl method);

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
    HotSpotResolvedObjectTypeImpl lookupType(String name, Class<?> accessingClass, boolean resolve);

    /**
     * Resolves the entry at index {@code cpi} in {@code constantPool} to an object.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry that can be
     * resolved to an object.
     */
    Object resolveConstantInPool(HotSpotConstantPool constantPool, int cpi);

    /**
     * Resolves the entry at index {@code cpi} in {@code constantPool} to an object, looking in the
     * constant pool cache first.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry that can be
     * resolved to an object.
     */
    Object resolvePossiblyCachedConstantInPool(HotSpotConstantPool constantPool, int cpi);

    /**
     * Gets the {@code JVM_CONSTANT_NameAndType} index from the entry at index {@code cpi} in
     * {@code constantPool}.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry containing a
     * {@code JVM_CONSTANT_NameAndType} index.
     */
    int lookupNameAndTypeRefIndexInPool(HotSpotConstantPool constantPool, int cpi);

    /**
     * Gets the name of the {@code JVM_CONSTANT_NameAndType} entry at index {@code cpi} in
     * {@code constantPool}.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote a
     * {@code JVM_CONSTANT_NameAndType} entry.
     */
    String lookupNameRefInPool(HotSpotConstantPool constantPool, int cpi);

    /**
     * Gets the signature of the {@code JVM_CONSTANT_NameAndType} entry at index {@code cpi} in
     * {@code constantPool}.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote a
     * {@code JVM_CONSTANT_NameAndType} entry.
     */
    String lookupSignatureRefInPool(HotSpotConstantPool constantPool, int cpi);

    /**
     * Gets the {@code JVM_CONSTANT_Class} index from the entry at index {@code cpi} in
     * {@code constantPool}.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry containing a
     * {@code JVM_CONSTANT_Class} index.
     */
    int lookupKlassRefIndexInPool(HotSpotConstantPool constantPool, int cpi);

    /**
     * Looks up a class denoted by the {@code JVM_CONSTANT_Class} entry at index {@code cpi} in
     * {@code constantPool}. This method does not perform any resolution.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote a
     * {@code JVM_CONSTANT_Class} entry.
     *
     * @return a HotSpotResolvedObjectTypeImpl for a resolved class entry or a String otherwise
     */
    Object lookupKlassInPool(HotSpotConstantPool constantPool, int cpi);

    /**
     * Looks up a method denoted by the entry at index {@code cpi} in {@code constantPool}. This
     * method does not perform any resolution.
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
    HotSpotResolvedJavaMethodImpl lookupMethodInPool(HotSpotConstantPool constantPool, int cpi, byte opcode);

    /**
     * Ensures that the type referenced by the specified {@code JVM_CONSTANT_InvokeDynamic} entry at
     * index {@code cpi} in {@code constantPool} is loaded and initialized.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote a
     * {@code JVM_CONSTANT_InvokeDynamic} entry.
     */
    void resolveInvokeDynamicInPool(HotSpotConstantPool constantPool, int cpi);

    /**
     * Ensures that the type referenced by the entry for a <a
     * href="https://docs.oracle.com/javase/specs/jvms/se8/html/jvms-2.html#jvms-2.9">signature
     * polymorphic</a> method at index {@code cpi} in {@code constantPool} is loaded and
     * initialized.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry representing
     * a signature polymorphic method.
     */
    void resolveInvokeHandleInPool(HotSpotConstantPool constantPool, int cpi);

    /**
     * Gets the resolved metaspace Klass denoted by the entry at index {@code cpi} in
     * {@code constantPool}.
     *
     * The behavior of this method is undefined if {@code cpi} does not denote an entry representing
     * a class.
     *
     * @throws LinkageError if resolution failed
     */
    HotSpotResolvedObjectTypeImpl resolveKlassInPool(HotSpotConstantPool constantPool, int cpi) throws LinkageError;

    /**
     * Looks up and attempts to resolve the {@code JVM_CONSTANT_Field} entry at index {@code cpi} in
     * {@code constantPool}. The values returned in {@code info} are:
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
    HotSpotResolvedObjectTypeImpl resolveFieldInPool(HotSpotConstantPool constantPool, int cpi, byte opcode, long[] info);

    /**
     * Converts {@code cpci} from an index into the cache for {@code constantPool} to an index
     * directly into {@code constantPool}.
     *
     * The behavior of this method is undefined if {@code ccpi} is an invalid constant pool cache
     * index.
     */
    int constantPoolRemapInstructionOperandFromCache(HotSpotConstantPool constantPool, int cpci);

    /**
     * Gets the appendix object (if any) associated with the entry at index {@code cpi} in
     * {@code constantPool}.
     */
    Object lookupAppendixInPool(HotSpotConstantPool constantPool, int cpi);

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
    void notifyCompilationStatistics(int id, HotSpotResolvedJavaMethodImpl method, boolean osr, int processedBytecodes, long time, long timeUnitsPerSecond, InstalledCode installedCode);

    /**
     * Resets all compilation statistics.
     */
    void resetCompilationStatistics();

    /**
     * Initializes the fields of {@code config}.
     */
    void initializeConfiguration(HotSpotVMConfig config);

    /**
     * Resolves the implementation of {@code method} for virtual dispatches on objects of dynamic
     * type {@code metaspaceKlassExactReceiver}. This resolution process only searches "up" the
     * class hierarchy of {@code metaspaceKlassExactReceiver}.
     *
     * @param klassCaller the caller or context type used to perform access checks
     * @return the link-time resolved method (might be abstract) or {@code 0} if it can not be
     *         linked
     */
    HotSpotResolvedJavaMethodImpl resolveMethod(HotSpotResolvedObjectTypeImpl klassExactReceiver, HotSpotResolvedJavaMethodImpl method, HotSpotResolvedObjectTypeImpl klassCaller);

    /**
     * Gets the static initializer of {@code metaspaceKlass}.
     *
     * @return 0 if {@code metaspaceKlass} has no static initialize
     */
    HotSpotResolvedJavaMethodImpl getClassInitializer(HotSpotResolvedObjectTypeImpl klass);

    /**
     * Determines if {@code metaspaceKlass} or any of its currently loaded subclasses overrides
     * {@code Object.finalize()}.
     */
    boolean hasFinalizableSubclass(HotSpotResolvedObjectTypeImpl klass);

    /**
     * Gets the method corresponding to {@code holder} and slot number {@code slot} (i.e.
     * {@link Method#slot} or {@link Constructor#slot}).
     */
    @SuppressWarnings("javadoc")
    HotSpotResolvedJavaMethodImpl getResolvedJavaMethodAtSlot(Class<?> holder, int slot);

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
     * Gets a stack trace element for {@code method} at bytecode index {@code bci}.
     */
    StackTraceElement getStackTraceElement(HotSpotResolvedJavaMethodImpl method, int bci);

    /**
     * Executes some {@code installedCode} with arguments {@code args}.
     *
     * @return the result of executing {@code installedCode}
     * @throws InvalidInstalledCodeException if {@code installedCode} has been invalidated
     */
    Object executeInstalledCode(Object[] args, InstalledCode installedCode) throws InvalidInstalledCodeException;

    /**
     * Gets the line number table for {@code method}. The line number table is encoded as (bci,
     * source line number) pairs.
     *
     * @return the line number table for {@code method} or null if it doesn't have one
     */
    long[] getLineNumberTable(HotSpotResolvedJavaMethodImpl method);

    /**
     * Gets the number of entries in the local variable table for {@code method}.
     *
     * @return the number of entries in the local variable table for {@code method}
     */
    int getLocalVariableTableLength(HotSpotResolvedJavaMethodImpl method);

    /**
     * Gets the address of the first entry in the local variable table for {@code method}.
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
     * @return 0 if {@code method} does not have a local variable table
     */
    long getLocalVariableTableStart(HotSpotResolvedJavaMethodImpl method);

    /**
     * Gets the {@link Class} mirror associated with {@code metaspaceKlass}.
     */
    Class<?> getJavaMirror(HotSpotResolvedObjectTypeImpl klass);

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
     * Determines if {@code method} should not be inlined or compiled.
     */
    void doNotInlineOrCompile(HotSpotResolvedJavaMethodImpl method);

    /**
     * Invalidates the profiling information for {@code method} and (re)initializes it such that
     * profiling restarts upon its next invocation.
     */
    void reprofile(HotSpotResolvedJavaMethodImpl method);

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
    int allocateCompileId(HotSpotResolvedJavaMethodImpl method, int entryBCI);

    /**
     * Gets the names of the supported GPU architectures.
     *
     * @return a comma separated list of names
     */
    String getGPUs();

    /**
     * Determines if {@code method} has OSR compiled code identified by {@code entryBCI} for
     * compilation level {@code level}.
     */
    boolean hasCompiledCodeForOSR(HotSpotResolvedJavaMethodImpl method, int entryBCI, int level);

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
    HotSpotStackFrameReference getNextStackFrame(HotSpotStackFrameReference frame, HotSpotResolvedJavaMethodImpl[] methods, int initialSkip);

    /**
     * Materializes all virtual objects within {@code stackFrame} updates its locals.
     *
     * @param invalidate if {@code true}, the compiled method for the stack frame will be
     *            invalidated.
     */
    void materializeVirtualObjects(HotSpotStackFrameReference stackFrame, boolean invalidate);

    /**
     * Gets the v-table index for interface method {@code method} in the receiver type
     * {@code metaspaceKlass} or {@link HotSpotVMConfig#invalidVtableIndex} if {@code method} is not
     * in {@code metaspaceKlass}'s v-table.
     */
    int getVtableIndexForInterface(HotSpotResolvedObjectTypeImpl klass, HotSpotResolvedJavaMethodImpl method);

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

    /**
     * Read a value representing a metaspace Method* and return the
     * {@link HotSpotResolvedJavaMethodImpl} wrapping it. This method does no checking that the
     * location actually contains a valid Method*. If the {@code base} object is a
     * {@link MetaspaceWrapperObject} then the metaspace pointer is fetched from that object and
     * used as the base. Otherwise the object itself is used as the base.
     *
     * @param base an object to read from or null
     * @param displacement
     * @return null or the resolved method for this location
     */
    HotSpotResolvedJavaMethodImpl getResolvedJavaMethod(Object base, long displacement);

    /**
     * Read a value representing a metaspace ConstantPool* and return the
     * {@link HotSpotConstantPool} wrapping it. This method does no checking that the location
     * actually contains a valid ConstantPool*. If the {@code base} object is a
     * {@link MetaspaceWrapperObject} then the metaspace pointer is fetched from that object and
     * used as the base. Otherwise the object itself is used as the base.
     *
     * @param base an object to read from or null
     * @param displacement
     * @return null or the resolved method for this location
     */
    HotSpotConstantPool getConstantPool(Object base, long displacement);

    /**
     * Read a value representing a metaspace Klass* and return the
     * {@link HotSpotResolvedObjectTypeImpl} wrapping it. The method does no checking that the
     * location actually contains a valid Klass*. If the {@code base} object is a
     * {@link MetaspaceWrapperObject} then the metaspace pointer is fetched from that object and
     * used as the base. Otherwise the object itself is used as the base.
     *
     * @param base an object to read from or null
     * @param displacement
     * @param compressed true if the location contains a compressed Klass*
     * @return null or the resolved method for this location
     */
    HotSpotResolvedObjectTypeImpl getResolvedJavaType(Object base, long displacement, boolean compressed);
}
