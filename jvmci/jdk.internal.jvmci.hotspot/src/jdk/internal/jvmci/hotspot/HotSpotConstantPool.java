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

import static jdk.internal.jvmci.common.UnsafeAccess.*;
import static jdk.internal.jvmci.hotspot.HotSpotJVMCIRuntime.*;

import java.lang.invoke.*;
import java.util.*;

import jdk.internal.jvmci.common.*;
import jdk.internal.jvmci.meta.*;
import jdk.internal.jvmci.options.*;

/**
 * Implementation of {@link ConstantPool} for HotSpot.
 */
public class HotSpotConstantPool implements ConstantPool, HotSpotProxified {

    public static class Options {
        // @formatter:off
        @Option(help = "Use Java code to access the constant pool cache and resolved references array", type = OptionType.Expert)
        public static final OptionValue<Boolean> UseConstantPoolCacheJavaCode = new OptionValue<>(false);
        // @formatter:on
    }

    /**
     * Subset of JVM bytecode opcodes used by {@link HotSpotConstantPool}.
     */
    public static class Bytecodes {
        public static final int LDC = 18; // 0x12
        public static final int LDC_W = 19; // 0x13
        public static final int LDC2_W = 20; // 0x14
        public static final int GETSTATIC = 178; // 0xB2
        public static final int PUTSTATIC = 179; // 0xB3
        public static final int GETFIELD = 180; // 0xB4
        public static final int PUTFIELD = 181; // 0xB5
        public static final int INVOKEVIRTUAL = 182; // 0xB6
        public static final int INVOKESPECIAL = 183; // 0xB7
        public static final int INVOKESTATIC = 184; // 0xB8
        public static final int INVOKEINTERFACE = 185; // 0xB9
        public static final int INVOKEDYNAMIC = 186; // 0xBA
        public static final int NEW = 187; // 0xBB
        public static final int NEWARRAY = 188; // 0xBC
        public static final int ANEWARRAY = 189; // 0xBD
        public static final int CHECKCAST = 192; // 0xC0
        public static final int INSTANCEOF = 193; // 0xC1
        public static final int MULTIANEWARRAY = 197; // 0xC5

        static boolean isInvoke(int opcode) {
            switch (opcode) {
                case INVOKEVIRTUAL:
                case INVOKESPECIAL:
                case INVOKESTATIC:
                case INVOKEINTERFACE:
                case INVOKEDYNAMIC:
                    return true;
                default:
                    return false;
            }
        }

        /**
         * See: {@code Rewriter::maybe_rewrite_invokehandle}.
         */
        static boolean isInvokeHandleAlias(int opcode) {
            switch (opcode) {
                case INVOKEVIRTUAL:
                case INVOKESPECIAL:
                    return true;
                default:
                    return false;
            }
        }
    }

    /**
     * Enum of all {@code JVM_CONSTANT} constants used in the VM. This includes the public and
     * internal ones.
     */
    private enum JVM_CONSTANT {
        // @formatter:off
        Utf8(config().jvmConstantUtf8),
        Integer(config().jvmConstantInteger),
        Long(config().jvmConstantLong),
        Float(config().jvmConstantFloat),
        Double(config().jvmConstantDouble),
        Class(config().jvmConstantClass),
        UnresolvedClass(config().jvmConstantUnresolvedClass),
        UnresolvedClassInError(config().jvmConstantUnresolvedClassInError),
        String(config().jvmConstantString),
        Fieldref(config().jvmConstantFieldref),
        MethodRef(config().jvmConstantMethodref),
        InterfaceMethodref(config().jvmConstantInterfaceMethodref),
        NameAndType(config().jvmConstantNameAndType),
        MethodHandle(config().jvmConstantMethodHandle),
        MethodHandleInError(config().jvmConstantMethodHandleInError),
        MethodType(config().jvmConstantMethodType),
        MethodTypeInError(config().jvmConstantMethodTypeInError),
        InvokeDynamic(config().jvmConstantInvokeDynamic);
        // @formatter:on

        private final int tag;

        private static final int ExternalMax = config().jvmConstantExternalMax;
        private static final int InternalMin = config().jvmConstantInternalMin;
        private static final int InternalMax = config().jvmConstantInternalMax;

        private JVM_CONSTANT(int tag) {
            this.tag = tag;
        }

        private static HotSpotVMConfig config() {
            return runtime().getConfig();
        }

        /**
         * Maps JVM_CONSTANT tags to {@link JVM_CONSTANT} values. Using a separate class for lazy
         * initialization.
         */
        static class TagValueMap {
            private static final JVM_CONSTANT[] table = new JVM_CONSTANT[ExternalMax + 1 + (InternalMax - InternalMin) + 1];

            static {
                assert InternalMin > ExternalMax;
                for (JVM_CONSTANT e : values()) {
                    table[indexOf(e.tag)] = e;
                }
            }

            private static int indexOf(int tag) {
                if (tag >= InternalMin) {
                    return tag - InternalMin + ExternalMax + 1;
                } else {
                    assert tag <= ExternalMax;
                }
                return tag;
            }

            static JVM_CONSTANT get(int tag) {
                JVM_CONSTANT res = table[indexOf(tag)];
                if (res != null) {
                    return res;
                }
                throw new JVMCIError("Unknown JVM_CONSTANT tag %s", tag);
            }
        }

        public static JVM_CONSTANT getEnum(int tag) {
            return TagValueMap.get(tag);
        }
    }

    private static class LookupTypeCacheElement {
        int lastCpi = Integer.MIN_VALUE;
        JavaType javaType;

        public LookupTypeCacheElement(int lastCpi, JavaType javaType) {
            super();
            this.lastCpi = lastCpi;
            this.javaType = javaType;
        }
    }

    /**
     * Reference to the C++ ConstantPool object.
     */
    private final long metaspaceConstantPool;
    private volatile LookupTypeCacheElement lastLookupType;

    /**
     * The constant pool cache of this constant pool.
     */
    private final Cache cache;

    /**
     * Represents a {@code ConstantPoolCache}. The cache needs to be lazy since the constant pool
     * cache is created when the methods of this class are rewritten and rewriting happens when the
     * class is linked.
     */
    private final class Cache {

        private long address;

        public Cache() {
            // Maybe the constant pool cache already exists...
            queryAddress();
        }

        /**
         * Queries the current value of {@code ConstantPool::_cache} if the current address is null.
         */
        private void queryAddress() {
            if (address == 0) {
                address = unsafe.getAddress(metaspaceConstantPool + runtime().getConfig().constantPoolCacheOffset);
            }
        }

        /**
         * Returns whether a constant pool cache for this constant pool exists.
         *
         * @return true if it exists, false otherwise
         */
        public boolean exists() {
            queryAddress();
            return address != 0;
        }

        /**
         * Represents a {@code ConstantPoolCacheEntry}.
         */
        private final class Entry {

            private final long address;

            public Entry(final long address) {
                this.address = address;
            }

            /**
             * {@code ConstantPoolCacheEntry::_indices} is volatile of type {@code intx}.
             *
             * @return value of field {@code _indices}
             */
            private long getIndices() {
                assert runtime().getHostJVMCIBackend().getTarget().wordSize == 8 : "port to non-64-bit platform";
                return unsafe.getLongVolatile(null, address + runtime().getConfig().constantPoolCacheEntryIndicesOffset);
            }

            /**
             * {@code ConstantPoolCacheEntry::_f1} is volatile of type {@code Metadata*}.
             *
             * @return value of field {@code _f1}
             */
            private long getF1() {
                assert runtime().getHostJVMCIBackend().getTarget().wordSize == 8 : "port to non-64-bit platform";
                return unsafe.getLongVolatile(null, address + runtime().getConfig().constantPoolCacheEntryF1Offset);
            }

            /**
             * {@code ConstantPoolCacheEntry::_f2} is volatile of type {@code intx}.
             *
             * @return value of field {@code _f2}
             */
            private long getF2() {
                assert runtime().getHostJVMCIBackend().getTarget().wordSize == 8 : "port to non-64-bit platform";
                return unsafe.getLongVolatile(null, address + runtime().getConfig().constantPoolCacheEntryF2Offset);
            }

            /**
             * {@code ConstantPoolCacheEntry::_flags} is volatile of type {@code intx}.
             *
             * @return flag bits
             */
            private long flags() {
                assert runtime().getHostJVMCIBackend().getTarget().wordSize == 8 : "port to non-64-bit platform";
                return unsafe.getLongVolatile(null, address + runtime().getConfig().constantPoolCacheEntryFlagsOffset);
            }

            private boolean isF1Null() {
                final long f1 = getF1();
                return f1 == 0;
            }

            /**
             * Returns the constant pool index for this entry. See
             * {@code ConstantPoolCacheEntry::constant_pool_index()}
             *
             * @return the constant pool index for this entry
             */
            public int getConstantPoolIndex() {
                return ((int) getIndices()) & runtime().getConfig().constantPoolCacheEntryCpIndexMask;
            }

            /**
             * See {@code ConstantPoolCache::has_appendix()}.
             *
             * @return true if there is an appendix, false otherwise
             */
            private boolean hasAppendix() {
                return (!isF1Null()) && (flags() & (1 << runtime().getConfig().constantPoolCacheEntryHasAppendixShift)) != 0;
            }

            /**
             * See {@code ConstantPoolCache::appendix_if_resolved()}.
             */
            public Object getAppendixIfResolved() {
                if (!hasAppendix()) {
                    return null;
                }
                final int index = ((int) getF2()) + runtime().getConfig().constantPoolCacheEntryIndyResolvedReferencesAppendixOffset;
                return resolvedReferences.getArray()[index];
            }
        }

        /**
         * Get the constant pool cache entry at index {@code index}.
         *
         * @param index index of entry to return
         * @return constant pool cache entry at given index
         */
        public Entry getEntryAt(int index) {
            queryAddress();
            assert exists();
            HotSpotVMConfig config = runtime().getConfig();
            return new Entry(address + config.constantPoolCacheSize + config.constantPoolCacheEntrySize * index);
        }

        /**
         * Maps the constant pool cache index back to a constant pool index. See
         * {@code ConstantPool::remap_instruction_operand_from_cache}.
         *
         * @param index the constant pool cache index
         * @return constant pool index
         */
        public int constantPoolCacheIndexToConstantPoolIndex(int index) {
            final int cacheIndex = index - runtime().getConfig().constantPoolCpCacheIndexTag;
            return getEntryAt(cacheIndex).getConstantPoolIndex();
        }

    }

    /**
     * Resolved references of this constant pool.
     */
    private final ResolvedReferences resolvedReferences = new ResolvedReferences();

    /**
     * Hide the resolved references array in a private class so it cannot be accessed directly. The
     * reason is the resolved references array is created when the constant pool cache is created.
     *
     * @see Cache
     */
    private final class ResolvedReferences {

        /**
         * Pointer to the {@code ConstantPool::_resolved_references} array.
         */
        private Object[] resolvedReferences;

        /**
         * Map of constant pool indexes to {@code ConstantPool::_resolved_references} indexes.
         */
        private final HashMap<Integer, Integer> referenceMap = new HashMap<>();

        /**
         * Returns the {@code ConstantPool::_resolved_references} array for this constant pool.
         *
         * @return resolved references array if exists, null otherwise
         */
        public Object[] getArray() {
            if (resolvedReferences == null) {
                final long handle = unsafe.getAddress(metaspaceConstantPool + runtime().getConfig().constantPoolResolvedReferencesOffset);
                if (handle != 0) {
                    resolvedReferences = (Object[]) runtime().getCompilerToVM().readUncompressedOop(handle + runtime().getConfig().handleHandleOffset);
                    fillReferenceMap();
                }
            }
            return resolvedReferences;
        }

        /**
         * Fills the {@link #referenceMap} with all the values from
         * {@code ConstantPool::_reference_map} for faster lookup.
         */
        private void fillReferenceMap() {
            // It is possible there is a resolved references array but no reference map.
            final long address = unsafe.getAddress(metaspaceConstantPool + runtime().getConfig().constantPoolReferenceMapOffset);
            if (address != 0) {
                final int length = unsafe.getInt(null, address + runtime().getConfig().arrayU1LengthOffset);
                for (int i = 0; i < length; i++) {
                    final int value = unsafe.getShort(address + runtime().getConfig().arrayU2DataOffset + i * Short.BYTES);
                    referenceMap.put(value, i);
                }
            }
        }

        /**
         * See {@code ConstantPool::cp_to_object_index}.
         *
         * @param cpi constant pool index
         * @return resolved references array index
         */
        public int constantPoolIndexToResolvedReferencesIndex(int cpi) {
            final Integer index = referenceMap.get(cpi);
            // We might not find the index for jsr292 call.
            return (index == null) ? -1 : index;
        }

    }

    public HotSpotConstantPool(long metaspaceConstantPool) {
        this.metaspaceConstantPool = metaspaceConstantPool;

        // Cache constructor needs metaspaceConstantPool.
        cache = new Cache();
    }

    /**
     * Gets the holder for this constant pool as {@link HotSpotResolvedObjectTypeImpl}.
     *
     * @return holder for this constant pool
     */
    private HotSpotResolvedObjectType getHolder() {
        final long metaspaceKlass = unsafe.getAddress(metaspaceConstantPool + runtime().getConfig().constantPoolHolderOffset);
        return HotSpotResolvedObjectTypeImpl.fromMetaspaceKlass(metaspaceKlass);
    }

    /**
     * Converts a raw index from the bytecodes to a constant pool index by adding a
     * {@link HotSpotVMConfig#constantPoolCpCacheIndexTag constant}.
     *
     * @param rawIndex index from the bytecode
     * @param opcode bytecode to convert the index for
     * @return constant pool index
     */
    private static int rawIndexToConstantPoolIndex(int rawIndex, int opcode) {
        int index;
        if (opcode == Bytecodes.INVOKEDYNAMIC) {
            index = rawIndex;
            // See: ConstantPool::is_invokedynamic_index
            assert index < 0 : "not an invokedynamic constant pool index " + index;
        } else {
            assert opcode == Bytecodes.GETFIELD || opcode == Bytecodes.PUTFIELD || opcode == Bytecodes.GETSTATIC || opcode == Bytecodes.PUTSTATIC || opcode == Bytecodes.INVOKEINTERFACE ||
                            opcode == Bytecodes.INVOKEVIRTUAL || opcode == Bytecodes.INVOKESPECIAL || opcode == Bytecodes.INVOKESTATIC : "unexpected invoke opcode " + opcode;
            index = rawIndex + runtime().getConfig().constantPoolCpCacheIndexTag;
        }
        return index;
    }

    /**
     * Decode a constant pool cache index to a constant pool index.
     *
     * See {@code ConstantPool::decode_cpcache_index}.
     *
     * @param index constant pool cache index
     * @return decoded index
     */
    private static int decodeConstantPoolCacheIndex(int index) {
        if (isInvokedynamicIndex(index)) {
            return decodeInvokedynamicIndex(index);
        } else {
            return index - runtime().getConfig().constantPoolCpCacheIndexTag;
        }
    }

    /**
     * See {@code ConstantPool::is_invokedynamic_index}.
     */
    private static boolean isInvokedynamicIndex(int index) {
        return index < 0;
    }

    /**
     * See {@code ConstantPool::decode_invokedynamic_index}.
     */
    private static int decodeInvokedynamicIndex(int i) {
        assert isInvokedynamicIndex(i) : i;
        return ~i;
    }

    /**
     * Gets the constant pool tag at index {@code index}.
     *
     * @param index constant pool index
     * @return constant pool tag
     */
    private JVM_CONSTANT getTagAt(int index) {
        assertBounds(index);
        HotSpotVMConfig config = runtime().getConfig();
        final long metaspaceConstantPoolTags = unsafe.getAddress(metaspaceConstantPool + config.constantPoolTagsOffset);
        final int tag = unsafe.getByteVolatile(null, metaspaceConstantPoolTags + config.arrayU1DataOffset + index);
        if (tag == 0) {
            return null;
        }
        return JVM_CONSTANT.getEnum(tag);
    }

    /**
     * Gets the constant pool entry at index {@code index}.
     *
     * @param index constant pool index
     * @return constant pool entry
     */
    private long getEntryAt(int index) {
        assertBounds(index);
        return unsafe.getAddress(metaspaceConstantPool + runtime().getConfig().constantPoolSize + index * runtime().getHostJVMCIBackend().getTarget().wordSize);
    }

    /**
     * Gets the integer constant pool entry at index {@code index}.
     *
     * @param index constant pool index
     * @return integer constant pool entry at index
     */
    private int getIntAt(int index) {
        assertTag(index, JVM_CONSTANT.Integer);
        return unsafe.getInt(metaspaceConstantPool + runtime().getConfig().constantPoolSize + index * runtime().getHostJVMCIBackend().getTarget().wordSize);
    }

    /**
     * Gets the long constant pool entry at index {@code index}.
     *
     * @param index constant pool index
     * @return long constant pool entry
     */
    private long getLongAt(int index) {
        assertTag(index, JVM_CONSTANT.Long);
        return unsafe.getLong(metaspaceConstantPool + runtime().getConfig().constantPoolSize + index * runtime().getHostJVMCIBackend().getTarget().wordSize);
    }

    /**
     * Gets the float constant pool entry at index {@code index}.
     *
     * @param index constant pool index
     * @return float constant pool entry
     */
    private float getFloatAt(int index) {
        assertTag(index, JVM_CONSTANT.Float);
        return unsafe.getFloat(metaspaceConstantPool + runtime().getConfig().constantPoolSize + index * runtime().getHostJVMCIBackend().getTarget().wordSize);
    }

    /**
     * Gets the double constant pool entry at index {@code index}.
     *
     * @param index constant pool index
     * @return float constant pool entry
     */
    private double getDoubleAt(int index) {
        assertTag(index, JVM_CONSTANT.Double);
        return unsafe.getDouble(metaspaceConstantPool + runtime().getConfig().constantPoolSize + index * runtime().getHostJVMCIBackend().getTarget().wordSize);
    }

    /**
     * Gets the {@code JVM_CONSTANT_NameAndType} constant pool entry at index {@code index}.
     *
     * @param index constant pool index
     * @return {@code JVM_CONSTANT_NameAndType} constant pool entry
     */
    private int getNameAndTypeAt(int index) {
        assertTag(index, JVM_CONSTANT.NameAndType);
        return unsafe.getInt(metaspaceConstantPool + runtime().getConfig().constantPoolSize + index * runtime().getHostJVMCIBackend().getTarget().wordSize);
    }

    /**
     * Gets the {@code JVM_CONSTANT_NameAndType} reference index constant pool entry at index
     * {@code index}.
     *
     * @param index constant pool index
     * @return {@code JVM_CONSTANT_NameAndType} reference constant pool entry
     */
    private int getNameAndTypeRefIndexAt(int index) {
        return runtime().getCompilerToVM().lookupNameAndTypeRefIndexInPool(metaspaceConstantPool, index);
    }

    /**
     * Gets the name of a {@code JVM_CONSTANT_NameAndType} constant pool entry at index
     * {@code index}.
     *
     * @param index constant pool index
     * @return name as {@link String}
     */
    private String getNameRefAt(int index) {
        if (Options.UseConstantPoolCacheJavaCode.getValue()) {
            final int nameRefIndex = getNameRefIndexAt(getNameAndTypeRefIndexAt(index));
            return new HotSpotSymbol(getEntryAt(nameRefIndex)).asString();
        } else {
            return runtime().getCompilerToVM().lookupNameRefInPool(metaspaceConstantPool, index);
        }
    }

    /**
     * Gets the name reference index of a {@code JVM_CONSTANT_NameAndType} constant pool entry at
     * index {@code index}.
     *
     * @param index constant pool index
     * @return name reference index
     */
    private int getNameRefIndexAt(int index) {
        final int refIndex = getNameAndTypeAt(index);
        // name ref index is in the low 16-bits.
        return refIndex & 0xFFFF;
    }

    /**
     * Gets the signature of a {@code JVM_CONSTANT_NameAndType} constant pool entry at index
     * {@code index}.
     *
     * @param index constant pool index
     * @return signature as {@link String}
     */
    private String getSignatureRefAt(int index) {
        if (Options.UseConstantPoolCacheJavaCode.getValue()) {
            final int signatureRefIndex = getSignatureRefIndexAt(getNameAndTypeRefIndexAt(index));
            return new HotSpotSymbol(getEntryAt(signatureRefIndex)).asString();
        } else {
            return runtime().getCompilerToVM().lookupSignatureRefInPool(metaspaceConstantPool, index);
        }
    }

    /**
     * Gets the signature reference index of a {@code JVM_CONSTANT_NameAndType} constant pool entry
     * at index {@code index}.
     *
     * @param index constant pool index
     * @return signature reference index
     */
    private int getSignatureRefIndexAt(int index) {
        final int refIndex = getNameAndTypeAt(index);
        // signature ref index is in the high 16-bits.
        return refIndex >>> 16;
    }

    /**
     * Gets the klass reference index constant pool entry at index {@code index}. See
     * {@code ConstantPool::klass_ref_index_at}.
     *
     * @param index constant pool index
     * @param cached whether to go through the constant pool cache
     * @return klass reference index
     */
    private int getKlassRefIndexAt(int index, boolean cached) {
        int cpi = index;
        if (cached && cache.exists()) {
            // change byte-ordering and go via cache
            cpi = cache.constantPoolCacheIndexToConstantPoolIndex(index);
        }
        assertTagIsFieldOrMethod(cpi);
        final int refIndex = unsafe.getInt(metaspaceConstantPool + runtime().getConfig().constantPoolSize + cpi * runtime().getHostJVMCIBackend().getTarget().wordSize);
        // klass ref index is in the low 16-bits.
        return refIndex & 0xFFFF;
    }

    /**
     * Gets the klass reference index constant pool entry at index {@code index}. See
     * {@code ConstantPool::klass_ref_index_at}.
     *
     * @param index constant pool index
     * @return klass reference index
     */
    private int getKlassRefIndexAt(int index) {
        if (Options.UseConstantPoolCacheJavaCode.getValue()) {
            return getKlassRefIndexAt(index, true);
        } else {
            return runtime().getCompilerToVM().lookupKlassRefIndexInPool(metaspaceConstantPool, index);
        }
    }

    /**
     * Gets the uncached klass reference index constant pool entry at index {@code index}. See:
     * {@code ConstantPool::uncached_klass_ref_index_at}.
     *
     * @param index constant pool index
     * @return klass reference index
     */
    private int getUncachedKlassRefIndexAt(int index) {
        if (Options.UseConstantPoolCacheJavaCode.getValue()) {
            return getKlassRefIndexAt(index, false);
        } else {
            assertTagIsFieldOrMethod(index);
            final int refIndex = unsafe.getInt(metaspaceConstantPool + runtime().getConfig().constantPoolSize + index * runtime().getHostJVMCIBackend().getTarget().wordSize);
            // klass ref index is in the low 16-bits.
            return refIndex & 0xFFFF;
        }
    }

    /**
     * Asserts that the constant pool index {@code index} is in the bounds of the constant pool.
     *
     * @param index constant pool index
     */
    private void assertBounds(int index) {
        assert 0 <= index && index < length() : "index " + index + " not between 0 and " + length();
    }

    /**
     * Asserts that the constant pool tag at index {@code index} is equal to {@code tag}.
     *
     * @param index constant pool index
     * @param tag expected tag
     */
    private void assertTag(int index, JVM_CONSTANT tag) {
        final JVM_CONSTANT tagAt = getTagAt(index);
        assert tagAt == tag : "constant pool tag at index " + index + " is " + tagAt + " but expected " + tag;
    }

    /**
     * Asserts that the constant pool tag at index {@code index} is a {@link JVM_CONSTANT#Fieldref},
     * or a {@link JVM_CONSTANT#MethodRef}, or a {@link JVM_CONSTANT#InterfaceMethodref}.
     *
     * @param index constant pool index
     */
    private void assertTagIsFieldOrMethod(int index) {
        final JVM_CONSTANT tagAt = getTagAt(index);
        assert tagAt == JVM_CONSTANT.Fieldref || tagAt == JVM_CONSTANT.MethodRef || tagAt == JVM_CONSTANT.InterfaceMethodref : tagAt;
    }

    @Override
    public int length() {
        return unsafe.getInt(metaspaceConstantPool + runtime().getConfig().constantPoolLengthOffset);
    }

    @Override
    public Object lookupConstant(int cpi) {
        assert cpi != 0;
        final JVM_CONSTANT tag = getTagAt(cpi);
        switch (tag) {
            case Integer:
                return JavaConstant.forInt(getIntAt(cpi));
            case Long:
                return JavaConstant.forLong(getLongAt(cpi));
            case Float:
                return JavaConstant.forFloat(getFloatAt(cpi));
            case Double:
                return JavaConstant.forDouble(getDoubleAt(cpi));
            case Class:
            case UnresolvedClass:
            case UnresolvedClassInError:
                final int opcode = -1;  // opcode is not used
                return lookupType(cpi, opcode);
            case String:
                String string;
                if (Options.UseConstantPoolCacheJavaCode.getValue()) {
                    // See: ConstantPool::resolve_constant_at_impl
                    /*
                     * Note: Call getArray() before constantPoolIndexToResolvedReferencesIndex()
                     * because it fills the map if the array exists.
                     */
                    Object[] localResolvedReferences = resolvedReferences.getArray();
                    final int index = resolvedReferences.constantPoolIndexToResolvedReferencesIndex(cpi);
                    if (index >= 0) {
                        string = (String) localResolvedReferences[index];
                        if (string != null) {
                            return HotSpotObjectConstantImpl.forObject(string);
                        }
                    }
                    assert index != -1;
                    // See: ConstantPool::string_at_impl
                    string = (String) localResolvedReferences[index];
                    if (string == null) {
                        final long metaspaceSymbol = getEntryAt(cpi);
                        HotSpotSymbol symbol = new HotSpotSymbol(metaspaceSymbol);
                        string = symbol.asString().intern();
                        // See: ConstantPool::string_at_put
                        localResolvedReferences[index] = string;
                    }
                } else {
                    string = (String) runtime().getCompilerToVM().resolvePossiblyCachedConstantInPool(metaspaceConstantPool, cpi);
                }
                return HotSpotObjectConstantImpl.forObject(string);
            case MethodHandle:
            case MethodHandleInError:
            case MethodType:
            case MethodTypeInError:
                Object obj = runtime().getCompilerToVM().resolveConstantInPool(metaspaceConstantPool, cpi);
                return HotSpotObjectConstantImpl.forObject(obj);
            default:
                throw new JVMCIError("Unknown constant pool tag %s", tag);
        }
    }

    @Override
    public String lookupUtf8(int cpi) {
        assertTag(cpi, JVM_CONSTANT.Utf8);
        String s;
        if (Options.UseConstantPoolCacheJavaCode.getValue()) {
            HotSpotSymbol symbol = new HotSpotSymbol(getEntryAt(cpi));
            s = symbol.asString();
            // It shouldn't but just in case something went wrong...
            if (s == null) {
                throw JVMCIError.shouldNotReachHere("malformed UTF-8 string in constant pool");
            }
        } else {
            s = runtime().getCompilerToVM().getSymbol(getEntryAt(cpi));
        }
        return s;
    }

    @Override
    public Signature lookupSignature(int cpi) {
        return new HotSpotSignature(runtime(), lookupUtf8(cpi));
    }

    @Override
    public JavaConstant lookupAppendix(int cpi, int opcode) {
        assert Bytecodes.isInvoke(opcode);
        final int index = rawIndexToConstantPoolIndex(cpi, opcode);

        Object appendix = null;

        if (Options.UseConstantPoolCacheJavaCode.getValue()) {
            if (!cache.exists()) {
                // Nothing to load yet.
                return null;
            }
            final int cacheIndex = decodeConstantPoolCacheIndex(index);
            Cache.Entry entry = cache.getEntryAt(cacheIndex);
            appendix = entry.getAppendixIfResolved();
        } else {
            appendix = runtime().getCompilerToVM().lookupAppendixInPool(metaspaceConstantPool, index);
        }

        if (appendix == null) {
            return null;
        } else {
            return HotSpotObjectConstantImpl.forObject(appendix);
        }
    }

    /**
     * Gets a {@link JavaType} corresponding a given metaspace Klass or a metaspace Symbol depending
     * on the {@link HotSpotVMConfig#compilerToVMKlassTag tag}.
     *
     * @param metaspacePointer either a metaspace Klass or a metaspace Symbol
     */
    private static JavaType getJavaType(final long metaspacePointer) {
        HotSpotJVMCIRuntime runtime = runtime();
        HotSpotVMConfig config = runtime.getConfig();
        if ((metaspacePointer & config.compilerToVMSymbolTag) != 0) {
            final long metaspaceSymbol = metaspacePointer & ~config.compilerToVMSymbolTag;
            String name;
            if (Options.UseConstantPoolCacheJavaCode.getValue()) {
                HotSpotSymbol symbol = new HotSpotSymbol(metaspaceSymbol);
                name = symbol.asString();
                // It shouldn't but just in case something went wrong...
                if (name == null) {
                    throw JVMCIError.shouldNotReachHere("malformed UTF-8 string in constant pool");
                }
            } else {
                name = runtime.getCompilerToVM().getSymbol(metaspaceSymbol);
            }
            return HotSpotUnresolvedJavaType.create(runtime(), "L" + name + ";");
        } else {
            assert (metaspacePointer & config.compilerToVMKlassTag) == 0;
            return HotSpotResolvedObjectTypeImpl.fromMetaspaceKlass(metaspacePointer);
        }
    }

    @Override
    public JavaMethod lookupMethod(int cpi, int opcode) {
        final int index = rawIndexToConstantPoolIndex(cpi, opcode);
        final long metaspaceMethod = runtime().getCompilerToVM().lookupMethodInPool(metaspaceConstantPool, index, (byte) opcode);
        if (metaspaceMethod != 0L) {
            HotSpotResolvedJavaMethod result = HotSpotResolvedJavaMethodImpl.fromMetaspace(metaspaceMethod);
            return result;
        } else {
            // Get the method's name and signature.
            String name = getNameRefAt(index);
            HotSpotSignature signature = new HotSpotSignature(runtime(), getSignatureRefAt(index));
            if (opcode == Bytecodes.INVOKEDYNAMIC) {
                HotSpotResolvedObjectType holder = HotSpotResolvedObjectTypeImpl.fromObjectClass(MethodHandle.class);
                return new HotSpotMethodUnresolved(name, signature, holder);
            } else {
                final int klassIndex = getKlassRefIndexAt(index);
                final long metaspacePointer = runtime().getCompilerToVM().lookupKlassInPool(metaspaceConstantPool, klassIndex);
                JavaType holder = getJavaType(metaspacePointer);
                return new HotSpotMethodUnresolved(name, signature, holder);
            }
        }
    }

    @Override
    public JavaType lookupType(int cpi, int opcode) {
        final LookupTypeCacheElement elem = this.lastLookupType;
        if (elem != null && elem.lastCpi == cpi) {
            return elem.javaType;
        } else {
            final long metaspacePointer = runtime().getCompilerToVM().lookupKlassInPool(metaspaceConstantPool, cpi);
            JavaType result = getJavaType(metaspacePointer);
            if (result instanceof ResolvedJavaType) {
                this.lastLookupType = new LookupTypeCacheElement(cpi, result);
            }
            return result;
        }
    }

    @Override
    public JavaField lookupField(int cpi, int opcode) {
        final int index = rawIndexToConstantPoolIndex(cpi, opcode);
        final int nameAndTypeIndex = getNameAndTypeRefIndexAt(index);
        final int nameIndex = getNameRefIndexAt(nameAndTypeIndex);
        String name = lookupUtf8(nameIndex);
        final int typeIndex = getSignatureRefIndexAt(nameAndTypeIndex);
        String typeName = lookupUtf8(typeIndex);
        JavaType type = runtime().lookupType(typeName, getHolder(), false);

        final int holderIndex = getKlassRefIndexAt(index);
        JavaType holder = lookupType(holderIndex, opcode);

        if (holder instanceof HotSpotResolvedObjectTypeImpl) {
            long[] info = new long[2];
            long metaspaceKlass;
            try {
                metaspaceKlass = runtime().getCompilerToVM().resolveField(metaspaceConstantPool, index, (byte) opcode, info);
            } catch (Throwable t) {
                /*
                 * If there was an exception resolving the field we give up and return an unresolved
                 * field.
                 */
                return new HotSpotUnresolvedField(holder, name, type);
            }
            HotSpotResolvedObjectTypeImpl resolvedHolder = HotSpotResolvedObjectTypeImpl.fromMetaspaceKlass(metaspaceKlass);
            final int flags = (int) info[0];
            final long offset = info[1];
            HotSpotResolvedJavaField result = resolvedHolder.createField(name, type, offset, flags);
            return result;
        } else {
            return new HotSpotUnresolvedField(holder, name, type);
        }
    }

    @Override
    public void loadReferencedType(int cpi, int opcode) {
        int index;
        switch (opcode) {
            case Bytecodes.CHECKCAST:
            case Bytecodes.INSTANCEOF:
            case Bytecodes.NEW:
            case Bytecodes.ANEWARRAY:
            case Bytecodes.MULTIANEWARRAY:
            case Bytecodes.LDC:
            case Bytecodes.LDC_W:
            case Bytecodes.LDC2_W:
                index = cpi;
                break;
            case Bytecodes.INVOKEDYNAMIC: {
                // invokedynamic instructions point to a constant pool cache entry.
                if (Options.UseConstantPoolCacheJavaCode.getValue()) {
                    // index = decodeConstantPoolCacheIndex(cpi) +
                    // runtime().getConfig().constantPoolCpCacheIndexTag;
                    // index = cache.constantPoolCacheIndexToConstantPoolIndex(index);
                    final int cacheIndex = cpi;
                    index = cache.getEntryAt(decodeInvokedynamicIndex(cacheIndex)).getConstantPoolIndex();
                    // JVMCIError.guarantee(index == x, index + " != " + x);
                } else {
                    index = decodeConstantPoolCacheIndex(cpi) + runtime().getConfig().constantPoolCpCacheIndexTag;
                    index = runtime().getCompilerToVM().constantPoolRemapInstructionOperandFromCache(metaspaceConstantPool, index);
                }
                break;
            }
            case Bytecodes.GETSTATIC:
            case Bytecodes.PUTSTATIC:
            case Bytecodes.GETFIELD:
            case Bytecodes.PUTFIELD:
            case Bytecodes.INVOKEVIRTUAL:
            case Bytecodes.INVOKESPECIAL:
            case Bytecodes.INVOKESTATIC:
            case Bytecodes.INVOKEINTERFACE: {
                // invoke and field instructions point to a constant pool cache entry.
                if (Options.UseConstantPoolCacheJavaCode.getValue()) {
                    // index = rawIndexToConstantPoolIndex(cpi, opcode);
                    // index = cache.constantPoolCacheIndexToConstantPoolIndex(index);
                    final int cacheIndex = cpi;
                    index = cache.getEntryAt(cacheIndex).getConstantPoolIndex();
                    // JVMCIError.guarantee(index == x, index + " != " + x);
                } else {
                    index = rawIndexToConstantPoolIndex(cpi, opcode);
                    index = runtime().getCompilerToVM().constantPoolRemapInstructionOperandFromCache(metaspaceConstantPool, index);
                }
                break;
            }
            default:
                throw JVMCIError.shouldNotReachHere("Unexpected opcode " + opcode);
        }

        final JVM_CONSTANT tag = getTagAt(index);
        if (tag == null) {
            assert getTagAt(index - 1) == JVM_CONSTANT.Double || getTagAt(index - 1) == JVM_CONSTANT.Long;
            return;
        }
        switch (tag) {
            case MethodRef:
            case Fieldref:
            case InterfaceMethodref:
                index = getUncachedKlassRefIndexAt(index);
                // Read the tag only once because it could change between multiple reads.
                final JVM_CONSTANT klassTag = getTagAt(index);
                assert klassTag == JVM_CONSTANT.Class || klassTag == JVM_CONSTANT.UnresolvedClass || klassTag == JVM_CONSTANT.UnresolvedClassInError : klassTag;
                // fall through
            case Class:
            case UnresolvedClass:
            case UnresolvedClassInError:
                final long metaspaceKlass = runtime().getCompilerToVM().constantPoolKlassAt(metaspaceConstantPool, index);
                HotSpotResolvedObjectTypeImpl type = HotSpotResolvedObjectTypeImpl.fromMetaspaceKlass(metaspaceKlass);
                Class<?> klass = type.mirror();
                if (!klass.isPrimitive() && !klass.isArray()) {
                    unsafe.ensureClassInitialized(klass);
                }
                switch (tag) {
                    case MethodRef:
                        if (Bytecodes.isInvokeHandleAlias(opcode)) {
                            final int methodRefCacheIndex = rawIndexToConstantPoolIndex(cpi, opcode);
                            if (isInvokeHandle(methodRefCacheIndex, type)) {
                                runtime().getCompilerToVM().resolveInvokeHandle(metaspaceConstantPool, methodRefCacheIndex);
                            }
                        }
                }
                break;
            case InvokeDynamic:
                if (isInvokedynamicIndex(cpi)) {
                    runtime().getCompilerToVM().resolveInvokeDynamic(metaspaceConstantPool, cpi);
                }
                break;
            default:
                // nothing
                break;
        }
    }

    private boolean isInvokeHandle(int methodRefCacheIndex, HotSpotResolvedObjectTypeImpl klass) {
        int index;
        if (Options.UseConstantPoolCacheJavaCode.getValue()) {
            index = cache.constantPoolCacheIndexToConstantPoolIndex(methodRefCacheIndex);
        } else {
            index = runtime().getCompilerToVM().constantPoolRemapInstructionOperandFromCache(metaspaceConstantPool, methodRefCacheIndex);
        }
        assertTag(index, JVM_CONSTANT.MethodRef);
        return ResolvedJavaMethod.isSignaturePolymorphic(klass, getNameRefAt(methodRefCacheIndex), runtime().getHostJVMCIBackend().getMetaAccess());
    }

    @Override
    public String toString() {
        HotSpotResolvedObjectType holder = getHolder();
        return "HotSpotConstantPool<" + holder.toJavaName() + ">";
    }
}
