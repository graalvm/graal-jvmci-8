/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
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
package jdk.vm.ci.hotspot;

import static jdk.vm.ci.hotspot.HotSpotJVMCIRuntime.runtime;
import static jdk.vm.ci.hotspot.UnsafeAccess.PLATFORM_UNSAFE;

import java.lang.annotation.Annotation;
import java.lang.reflect.Array;
import java.lang.reflect.Constructor;
import java.lang.reflect.Executable;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Type;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;
import java.util.function.Predicate;

import jdk.vm.ci.common.JVMCIError;
import jdk.vm.ci.meta.JavaConstant;
import jdk.vm.ci.meta.JavaKind;
import jdk.vm.ci.meta.MetaAccessProvider;
import jdk.vm.ci.meta.ResolvedJavaField;
import jdk.vm.ci.meta.ResolvedJavaMethod;
import jdk.vm.ci.meta.ResolvedJavaType;
import jdk.vm.ci.meta.SpeculationLog;

/**
 * Implementation of {@link HotSpotJVMCIReflection} in terms of standard JDK reflection API. This is
 * only available when running in the HotSpot heap.
 */
final class HotSpotJDKReflection extends HotSpotJVMCIReflection {

    @Override
    Object resolveObject(HotSpotObjectConstantImpl object) {
        if (object == null) {
            return null;
        }
        return ((DirectHotSpotObjectConstantImpl) object).object;
    }

    @Override
    boolean isInstance(HotSpotResolvedObjectTypeImpl holder, HotSpotObjectConstantImpl obj) {
        Class<?> javaMirror = getMirror(holder);
        Object value = resolveObject(obj);
        return javaMirror.isInstance(value);
    }

    @Override
    boolean isAssignableFrom(HotSpotResolvedObjectTypeImpl holder, HotSpotResolvedObjectTypeImpl otherType) {
        Class<?> javaMirror = getMirror(holder);
        return javaMirror.isAssignableFrom(getMirror(otherType));

    }

    @Override
    Annotation[] getAnnotations(HotSpotResolvedObjectTypeImpl holder) {
        Class<?> javaMirror = getMirror(holder);
        return javaMirror.getAnnotations();
    }

    @Override
    Annotation[] getDeclaredAnnotations(HotSpotResolvedObjectTypeImpl holder) {
        Class<?> javaMirror = getMirror(holder);
        return javaMirror.getDeclaredAnnotations();
    }

    @Override
    <T extends Annotation> T getAnnotation(HotSpotResolvedObjectTypeImpl holder, Class<T> annotationClass) {
        Class<?> javaMirror = getMirror(holder);
        return javaMirror.getAnnotation(annotationClass);
    }

    @Override
    boolean isLocalClass(HotSpotResolvedObjectTypeImpl holder) {
        Class<?> javaMirror = getMirror(holder);
        return javaMirror.isLocalClass();
    }

    @Override
    boolean isMemberClass(HotSpotResolvedObjectTypeImpl holder) {
        Class<?> javaMirror = getMirror(holder);
        return javaMirror.isMemberClass();
    }

    @Override
    HotSpotResolvedObjectType getEnclosingClass(HotSpotResolvedObjectTypeImpl holder) {
        Class<?> javaMirror = getMirror(holder);
        return (HotSpotResolvedObjectType) HotSpotJVMCIMetaAccessContext.fromClass(javaMirror.getEnclosingClass());
    }

    @Override
    ResolvedJavaMethod[] getDeclaredConstructors(HotSpotResolvedObjectTypeImpl holder) {
        Class<?> javaMirror = getMirror(holder);

        Constructor<?>[] constructors = javaMirror.getDeclaredConstructors();
        ResolvedJavaMethod[] result = new ResolvedJavaMethod[constructors.length];
        for (int i = 0; i < constructors.length; i++) {
            result[i] = runtime().getHostJVMCIBackend().getMetaAccess().lookupJavaMethod(constructors[i]);
            assert result[i].isConstructor();
        }
        return result;
    }

    @Override
    ResolvedJavaMethod[] getDeclaredMethods(HotSpotResolvedObjectTypeImpl holder) {
        Class<?> javaMirror = getMirror(holder);

        Method[] methods = javaMirror.getDeclaredMethods();
        ResolvedJavaMethod[] result = new ResolvedJavaMethod[methods.length];
        for (int i = 0; i < methods.length; i++) {
            result[i] = runtime().getHostJVMCIBackend().getMetaAccess().lookupJavaMethod(methods[i]);
            assert !result[i].isConstructor();
        }
        return result;
    }

    @Override
    JavaConstant readFieldValue(HotSpotResolvedObjectTypeImpl holder, HotSpotResolvedJavaField field, boolean isVolatile) {
        Class<?> javaMirror = getMirror(holder);
        return readFieldValue(field, javaMirror, isVolatile);
    }

    @Override
    JavaConstant readFieldValue(HotSpotObjectConstantImpl object, HotSpotResolvedJavaField field, boolean isVolatile) {
        Object value = resolveObject(object);
        return readFieldValue(field, value, isVolatile);
    }

    /**
     * The {@link SpeculationLog} for methods compiled by JVMCI hang off this per-declaring-type
     * {@link ClassValue}. The raw Method* value is safe to use as a key in the map as a) it is
     * never moves and b) we never read from it.
     * <p>
     * One implication is that we will preserve {@link SpeculationLog}s for methods that have been
     * redefined via class redefinition. It's tempting to periodically flush such logs but we cannot
     * read the JVM_ACC_IS_OBSOLETE bit (or anything else) via the raw pointer as obsoleted methods
     * are subject to clean up and deletion (see InstanceKlass::purge_previous_versions_internal).
     */
    private static final ClassValue<Map<Long, SpeculationLog>> SpeculationLogs = new ClassValue<Map<Long, SpeculationLog>>() {
        @Override
        protected Map<Long, SpeculationLog> computeValue(java.lang.Class<?> type) {
            return new HashMap<>(4);
        }
    };

    @Override
    Map<Long, SpeculationLog> getSpeculationLogs(HotSpotResolvedObjectTypeImpl holder) {
        Class<?> javaMirror = getMirror(holder);
        return SpeculationLogs.get(javaMirror);
    }

    @Override
    boolean equals(HotSpotObjectConstantImpl a, HotSpotObjectConstantImpl b) {
        return resolveObject(a) == resolveObject(b) && a.isCompressed() == b.isCompressed();
    }

    @Override
    JavaConstant getJavaMirror(HotSpotResolvedPrimitiveType holder) {
        return holder.mirror;
    }

    @Override
    JavaConstant getJavaMirror(HotSpotResolvedObjectTypeImpl holder) {
        return DirectHotSpotObjectConstantImpl.forNonNullObject(getMirror(holder), false);
    }

    @Override
    ResolvedJavaMethod.Parameter[] getParameters(HotSpotResolvedJavaMethodImpl method) {
        java.lang.reflect.Parameter[] javaParameters = getMethod(method).getParameters();
        ResolvedJavaMethod.Parameter[] res = new ResolvedJavaMethod.Parameter[javaParameters.length];
        for (int i = 0; i < res.length; i++) {
            java.lang.reflect.Parameter src = javaParameters[i];
            String paramName = src.isNamePresent() ? src.getName() : null;
            res[i] = new ResolvedJavaMethod.Parameter(paramName, src.getModifiers(), method, i);
        }
        return res;
    }

    @Override
    Annotation[][] getParameterAnnotations(HotSpotResolvedJavaMethodImpl javaMethod) {
        return getMethod(javaMethod).getParameterAnnotations();
    }

    @Override
    Type[] getGenericParameterTypes(HotSpotResolvedJavaMethodImpl javaMethod) {
        return getMethod(javaMethod).getGenericParameterTypes();
    }

    static final Annotation[] NO_ANNOTATIONS = new Annotation[0];

    @Override
    Annotation[] getFieldAnnotations(HotSpotResolvedJavaFieldImpl javaField) {
        if (javaField.isInternal()) {
            return NO_ANNOTATIONS;
        }
        Field field = getField(javaField);
        if (field == null) {
            return NO_ANNOTATIONS;
        }
        return field.getAnnotations();
    }

    @Override
    Annotation[] getMethodAnnotations(HotSpotResolvedJavaMethodImpl javaMethod) {
        Executable method = getMethod(javaMethod);
        if (method == null) {
            return NO_ANNOTATIONS;
        }
        return method.getAnnotations();
    }

    @Override
    Annotation[] getMethodDeclaredAnnotations(HotSpotResolvedJavaMethodImpl javaMethod) {
        Executable method = getMethod(javaMethod);
        if (method == null) {
            return NO_ANNOTATIONS;
        }
        return method.getDeclaredAnnotations();
    }

    @Override
    Annotation[] getFieldDeclaredAnnotations(HotSpotResolvedJavaFieldImpl javaField) {
        Field field = getField(javaField);
        if (field == null) {
            return NO_ANNOTATIONS;
        }
        return field.getDeclaredAnnotations();
    }

    @Override
    <T extends Annotation> T getMethodAnnotation(HotSpotResolvedJavaMethodImpl javaMethod, Class<T> annotationClass) {
        Executable method = getMethod(javaMethod);
        if (method == null) {
            return null;
        }
        return method.getAnnotation(annotationClass);
    }

    @Override
    <T extends Annotation> T getFieldAnnotation(HotSpotResolvedJavaFieldImpl javaField, Class<T> annotationClass) {
        Field field = getField(javaField);
        if (field == null) {
            return null;
        }
        return field.getAnnotation(annotationClass);
    }

    @Override
    HotSpotResolvedObjectTypeImpl getType(HotSpotObjectConstantImpl object) {
        Object value = resolveObject(object);
        Class<?> theClass = value.getClass();
        return (HotSpotResolvedObjectTypeImpl) HotSpotJVMCIMetaAccessContext.fromClass(theClass);
    }

    @Override
    String asString(HotSpotObjectConstantImpl object) {
        Object value = resolveObject(object);
        if (value instanceof String) {
            return (String) value;
        }
        return null;
    }

    @Override
    ResolvedJavaType asJavaType(HotSpotObjectConstantImpl object) {
        Object value = resolveObject(object);
        if (value instanceof Class) {
            Class<?> javaClass = (Class<?>) value;
            return HotSpotJVMCIMetaAccessContext.fromClass(javaClass);
        }
        if (value instanceof ResolvedJavaType) {
            return (ResolvedJavaType) value;
        }
        return null;
    }

    @SuppressWarnings("unchecked")
    @Override
    <T> T asObject(HotSpotObjectConstantImpl object, Class<T> type) {
        Object value = resolveObject(object);
        if (type.isInstance(value)) {
            return (T) value;
        }
        return null;
    }

    @Override
    Object asObject(HotSpotObjectConstantImpl object, HotSpotResolvedJavaType type) {
        Object value = resolveObject(object);
        if (getMirror(type).isInstance(value)) {
            return value;
        }
        return null;
    }

    @Override
    String formatString(HotSpotObjectConstantImpl object) {
        return JavaKind.Object.format(resolveObject(object));
    }

    @Override
    Integer getLength(HotSpotObjectConstantImpl arrayObject) {
        Object object = resolveObject(arrayObject);
        if (object.getClass().isArray()) {
            return Array.getLength(object);
        }
        return null;
    }

    @Override
    JavaConstant readArrayElement(HotSpotObjectConstantImpl arrayObject, int index) {
        Object a = resolveObject(arrayObject);
        if (!a.getClass().isArray() || index < 0 || index >= Array.getLength(a)) {
            return null;
        }
        if (a instanceof Object[]) {
            Object element = ((Object[]) a)[index];
            return forObject(element);
        } else {
            if (a instanceof int[]) {
                return JavaConstant.forInt(((int[]) a)[index]);
            } else if (a instanceof char[]) {
                return JavaConstant.forChar(((char[]) a)[index]);
            } else if (a instanceof byte[]) {
                return JavaConstant.forByte(((byte[]) a)[index]);
            } else if (a instanceof long[]) {
                return JavaConstant.forLong(((long[]) a)[index]);
            } else if (a instanceof short[]) {
                return JavaConstant.forShort(((short[]) a)[index]);
            } else if (a instanceof float[]) {
                return JavaConstant.forFloat(((float[]) a)[index]);
            } else if (a instanceof double[]) {
                return JavaConstant.forDouble(((double[]) a)[index]);
            } else if (a instanceof boolean[]) {
                return JavaConstant.forBoolean(((boolean[]) a)[index]);
            } else {
                throw new JVMCIError("Should not reach here");
            }
        }
    }

    @Override
    JavaConstant unboxPrimitive(HotSpotObjectConstantImpl source) {
        return JavaConstant.forBoxedPrimitive(resolveObject(source));
    }

    @Override
    JavaConstant forObject(Object value) {
        if (value == null) {
            return JavaConstant.NULL_POINTER;
        }
        return forNonNullObject(value);
    }

    private static HotSpotObjectConstantImpl forNonNullObject(Object value) {
        return DirectHotSpotObjectConstantImpl.forNonNullObject(value, false);
    }

    @Override
    JavaConstant boxPrimitive(JavaConstant source) {
        return forNonNullObject(source.asBoxedPrimitive());
    }

    @Override
    int getInt(HotSpotObjectConstantImpl object, long displacement) {
        return PLATFORM_UNSAFE.getInt((resolveObject(object)), displacement);
    }

    @Override
    byte getByte(HotSpotObjectConstantImpl object, long displacement) {
        return PLATFORM_UNSAFE.getByte(resolveObject(object), displacement);
    }

    @Override
    short getShort(HotSpotObjectConstantImpl object, long displacement) {
        return PLATFORM_UNSAFE.getShort(resolveObject(object), displacement);
    }

    @Override
    long getLong(HotSpotObjectConstantImpl object, long displacement) {
        return PLATFORM_UNSAFE.getLong(resolveObject(object), displacement);
    }

    @Override
    void checkRead(HotSpotObjectConstantImpl constant, JavaKind kind, long displacement, HotSpotResolvedObjectType type) {
        checkRead(kind, displacement, type, resolveObject(constant));
    }

    /**
     * Offset of injected {@code java.lang.Class::oop_size} field. No need to make {@code volatile}
     * as initialization is idempotent.
     */
    private long oopSizeOffset;

    private static int computeOopSizeOffset(HotSpotJVMCIRuntime runtime) {
        MetaAccessProvider metaAccess = runtime.getHostJVMCIBackend().getMetaAccess();
        ResolvedJavaType staticType = metaAccess.lookupJavaType(Class.class);
        for (ResolvedJavaField f : staticType.getInstanceFields(false)) {
            if (f.getName().equals("oop_size")) {
                int offset = f.getOffset();
                assert offset != 0 : "not expecting offset of java.lang.Class::oop_size to be 0";
                return offset;
            }
        }
        throw new JVMCIError("Could not find injected java.lang.Class::oop_size field");
    }

    long oopSizeOffset() {
        if (oopSizeOffset == 0) {
            oopSizeOffset = computeOopSizeOffset(runtime());
        }
        return oopSizeOffset;
    }

    private boolean checkRead(JavaKind kind, long displacement, HotSpotResolvedObjectType type, Object object) {
        if (type.isArray()) {
            ResolvedJavaType componentType = type.getComponentType();
            JavaKind componentKind = componentType.getJavaKind();
            final int headerSize = runtime().getArrayBaseOffset(componentKind);
            int sizeOfElement = runtime().getArrayIndexScale(componentKind);
            int length = Array.getLength(object);
            long arrayEnd = headerSize + (sizeOfElement * length);
            boolean aligned = ((displacement - headerSize) % sizeOfElement) == 0;
            if (displacement < 0 || displacement > (arrayEnd - sizeOfElement) || (kind == JavaKind.Object && !aligned)) {
                int index = (int) ((displacement - headerSize) / sizeOfElement);
                throw new IllegalArgumentException("Unsafe array access: reading element of kind " + kind +
                                " at offset " + displacement + " (index ~ " + index + ") in " +
                                type.toJavaName() + " object of length " + length);
            }
        } else if (kind != JavaKind.Object) {
            long size;
            if (object instanceof Class) {
                int wordSize = runtime().getHostJVMCIBackend().getCodeCache().getTarget().wordSize;
                size = PLATFORM_UNSAFE.getInt(object, oopSizeOffset()) * wordSize;
            } else {
                size = Math.abs(type.instanceSize());
            }
            int bytesToRead = kind.getByteCount();
            if (displacement + bytesToRead > size || displacement < 0) {
                throw new IllegalArgumentException("Unsafe access: reading " + bytesToRead + " bytes at offset " + displacement + " in " +
                                type.toJavaName() + " object of size " + size);
            }
        } else {
            ResolvedJavaField field = null;
            if (object instanceof Class) {
                // Read of a static field
                HotSpotResolvedJavaType hotSpotResolvedJavaType = HotSpotJVMCIMetaAccessContext.fromClass((Class<?>) object);
                if (hotSpotResolvedJavaType instanceof HotSpotResolvedObjectTypeImpl) {
                    HotSpotResolvedObjectTypeImpl staticFieldsHolder = (HotSpotResolvedObjectTypeImpl) hotSpotResolvedJavaType;
                    field = staticFieldsHolder.findStaticFieldWithOffset(displacement, JavaKind.Object);
                }
            }
            if (field == null) {
                field = type.findInstanceFieldWithOffset(displacement, JavaKind.Object);
            }
            if (field == null) {
                throw new IllegalArgumentException("Unsafe object access: field not found for read of kind Object" +
                                " at offset " + displacement + " in " + type.toJavaName() + " object");
            }
            if (field.getJavaKind() != JavaKind.Object) {
                throw new IllegalArgumentException("Unsafe object access: field " + field.format("%H.%n:%T") + " not of expected kind Object" +
                                " at offset " + displacement + " in " + type.toJavaName() + " object");
            }
        }
        return true;
    }

    JavaConstant readFieldValue(HotSpotResolvedJavaField field, Object obj, boolean isVolatile) {
        assert obj != null;
        assert !field.isStatic() || obj instanceof Class;
        long displacement = field.getOffset();

        assert checkRead(field.getJavaKind(), displacement,
                        (HotSpotResolvedObjectType) runtime().getHostJVMCIBackend().getMetaAccess().lookupJavaType(field.isStatic() ? (Class<?>) obj : obj.getClass()),
                        obj);
        JavaKind kind = field.getJavaKind();
        switch (kind) {
            case Boolean:
                return JavaConstant.forBoolean(isVolatile ? PLATFORM_UNSAFE.getBooleanVolatile(obj, displacement) : PLATFORM_UNSAFE.getBoolean(obj, displacement));
            case Byte:
                return JavaConstant.forByte(isVolatile ? PLATFORM_UNSAFE.getByteVolatile(obj, displacement) : PLATFORM_UNSAFE.getByte(obj, displacement));
            case Char:
                return JavaConstant.forChar(isVolatile ? PLATFORM_UNSAFE.getCharVolatile(obj, displacement) : PLATFORM_UNSAFE.getChar(obj, displacement));
            case Short:
                return JavaConstant.forShort(isVolatile ? PLATFORM_UNSAFE.getShortVolatile(obj, displacement) : PLATFORM_UNSAFE.getShort(obj, displacement));
            case Int:
                return JavaConstant.forInt(isVolatile ? PLATFORM_UNSAFE.getIntVolatile(obj, displacement) : PLATFORM_UNSAFE.getInt(obj, displacement));
            case Long:
                return JavaConstant.forLong(isVolatile ? PLATFORM_UNSAFE.getLongVolatile(obj, displacement) : PLATFORM_UNSAFE.getLong(obj, displacement));
            case Float:
                return JavaConstant.forFloat(isVolatile ? PLATFORM_UNSAFE.getFloatVolatile(obj, displacement) : PLATFORM_UNSAFE.getFloat(obj, displacement));
            case Double:
                return JavaConstant.forDouble(isVolatile ? PLATFORM_UNSAFE.getDoubleVolatile(obj, displacement) : PLATFORM_UNSAFE.getDouble(obj, displacement));
            case Object:
                return forObject(isVolatile ? PLATFORM_UNSAFE.getObjectVolatile(obj, displacement) : PLATFORM_UNSAFE.getObject(obj, displacement));
            default:
                throw new IllegalArgumentException("Unsupported kind: " + kind);

        }
    }

    // Non-volatile since multi-initialization is harmless
    private Predicate<ResolvedJavaType> intrinsificationTrustPredicate;

    @Override
    Predicate<ResolvedJavaType> getIntrinsificationTrustPredicate(Class<?>... compilerLeafClasses) {
        if (intrinsificationTrustPredicate == null) {
            intrinsificationTrustPredicate = new Predicate<ResolvedJavaType>() {
                @Override
                public boolean test(ResolvedJavaType type) {
                    if (type instanceof HotSpotResolvedJavaType) {
                        Class<?> mirror = getMirror((HotSpotResolvedJavaType) type);
                        ClassLoader cl = mirror.getClassLoader();
                        return cl == null || getTrustedLoaders().contains(cl);
                    } else {
                        return false;
                    }
                }

                // Non-volatile since initialization is idempotent
                private Set<ClassLoader> trustedLoaders;

                private Set<ClassLoader> getTrustedLoaders() {
                    Set<ClassLoader> loaders = trustedLoaders;
                    if (loaders == null) {
                        loaders = new HashSet<>();
                        try {
                            Object launcher = Class.forName("sun.misc.Launcher").getMethod("getLauncher").invoke(null);
                            ClassLoader appLoader = (ClassLoader) launcher.getClass().getMethod("getClassLoader").invoke(launcher);
                            ClassLoader extLoader = appLoader.getParent();
                            assert extLoader.getClass().getName().equals("sun.misc.Launcher$ExtClassLoader") : extLoader;
                            loaders.add(extLoader);
                        } catch (Exception e) {
                            throw new JVMCIError(e);
                        }
                        for (Class<?> compilerLeafClass : compilerLeafClasses) {
                            ClassLoader cl = compilerLeafClass.getClassLoader();
                            while (cl != null) {
                                loaders.add(cl);
                                cl = cl.getParent();
                            }
                        }
                        trustedLoaders = loaders;
                    }
                    return loaders;
                }
            };
        }
        return intrinsificationTrustPredicate;
    }
}
