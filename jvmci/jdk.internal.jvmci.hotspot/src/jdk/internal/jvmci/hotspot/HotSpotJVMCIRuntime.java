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

import static jdk.internal.jvmci.inittimer.InitTimer.timer;

import java.lang.reflect.Array;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;
import java.util.TreeMap;

import jdk.internal.jvmci.code.Architecture;
import jdk.internal.jvmci.code.CompilationResult;
import jdk.internal.jvmci.code.InstalledCode;
import jdk.internal.jvmci.common.JVMCIError;
import jdk.internal.jvmci.compiler.Compiler;
import jdk.internal.jvmci.compiler.CompilerFactory;
import jdk.internal.jvmci.compiler.StartupEventListener;
import jdk.internal.jvmci.inittimer.InitTimer;
import jdk.internal.jvmci.meta.JVMCIMetaAccessContext;
import jdk.internal.jvmci.meta.JavaKind;
import jdk.internal.jvmci.meta.JavaType;
import jdk.internal.jvmci.meta.ResolvedJavaType;
import jdk.internal.jvmci.runtime.JVMCI;
import jdk.internal.jvmci.runtime.JVMCIBackend;
import jdk.internal.jvmci.service.Services;

//JaCoCo Exclude

/**
 * HotSpot implementation of a JVMCI runtime.
 */
public final class HotSpotJVMCIRuntime implements HotSpotJVMCIRuntimeProvider, HotSpotProxified {

    /**
     * The proper initialization of this class is complex because it's tangled up with the
     * initialization of the JVMCI and really should only ever be triggered through
     * {@link JVMCI#getRuntime}. However since {@link #runtime} can also be called directly it
     * should also trigger proper initialization. To ensure proper ordering, the static initializer
     * of this class initializes {@link JVMCI} and then access to
     * {@link HotSpotJVMCIRuntime.DelayedInit#instance} triggers the final initialization of the
     * {@link HotSpotJVMCIRuntime}.
     */
    private static void initializeJVMCI() {
        JVMCI.initialize();
    }

    static {
        initializeJVMCI();
    }

    @SuppressWarnings("try")
    static class DelayedInit {
        private static final HotSpotJVMCIRuntime instance;

        static {
            try (InitTimer t0 = timer("HotSpotJVMCIRuntime.<clinit>")) {
                try (InitTimer t = timer("StartupEventListener.beforeJVMCIStartup")) {
                    for (StartupEventListener l : Services.load(StartupEventListener.class)) {
                        l.beforeJVMCIStartup();
                    }
                }

                try (InitTimer t = timer("HotSpotJVMCIRuntime.<init>")) {
                    instance = new HotSpotJVMCIRuntime();
                }

                try (InitTimer t = timer("HotSpotJVMCIRuntime.completeInitialization")) {
                    instance.completeInitialization();
                }
            }
        }
    }

    /**
     * Gets the singleton {@link HotSpotJVMCIRuntime} object.
     */
    public static HotSpotJVMCIRuntime runtime() {
        assert DelayedInit.instance != null;
        return DelayedInit.instance;
    }

    /**
     * Do deferred initialization.
     */
    public void completeInitialization() {
        compiler = HotSpotJVMCICompilerConfig.getCompilerFactory().createCompiler(this);
        trivialPrefixes = HotSpotJVMCICompilerConfig.getCompilerFactory().getTrivialPrefixes();
    }

    public static HotSpotJVMCIBackendFactory findFactory(String architecture) {
        for (HotSpotJVMCIBackendFactory factory : Services.load(HotSpotJVMCIBackendFactory.class)) {
            if (factory.getArchitecture().equalsIgnoreCase(architecture)) {
                return factory;
            }
        }

        throw new JVMCIError("No JVMCI runtime available for the %s architecture", architecture);
    }

    /**
     * Gets the kind of a word value on the {@linkplain #getHostJVMCIBackend() host} backend.
     */
    public static JavaKind getHostWordKind() {
        return runtime().getHostJVMCIBackend().getCodeCache().getTarget().wordJavaKind;
    }

    protected final CompilerToVM compilerToVm;

    protected final HotSpotVMConfig config;
    private final JVMCIBackend hostBackend;

    private Compiler compiler;
    protected final JVMCIMetaAccessContext metaAccessContext;

    private final Map<Class<? extends Architecture>, JVMCIBackend> backends = new HashMap<>();

    private final Iterable<HotSpotVMEventListener> vmEventListeners;

    @SuppressWarnings("unused") private String[] trivialPrefixes;

    @SuppressWarnings("try")
    private HotSpotJVMCIRuntime() {
        compilerToVm = new CompilerToVM();

        try (InitTimer t = timer("HotSpotVMConfig<init>")) {
            config = new HotSpotVMConfig(compilerToVm);
        }

        String hostArchitecture = config.getHostArchitectureName();

        HotSpotJVMCIBackendFactory factory;
        try (InitTimer t = timer("find factory:", hostArchitecture)) {
            factory = findFactory(hostArchitecture);
        }

        CompilerFactory compilerFactory = HotSpotJVMCICompilerConfig.getCompilerFactory();

        try (InitTimer t = timer("create JVMCI backend:", hostArchitecture)) {
            hostBackend = registerBackend(factory.createJVMCIBackend(this, compilerFactory, null));
        }

        vmEventListeners = Services.load(HotSpotVMEventListener.class);

        JVMCIMetaAccessContext context = null;
        for (HotSpotVMEventListener vmEventListener : vmEventListeners) {
            context = vmEventListener.createMetaAccessContext(this);
            if (context != null) {
                break;
            }
        }
        if (context == null) {
            context = new HotSpotJVMCIMetaAccessContext();
        }
        metaAccessContext = context;

        if (Boolean.valueOf(System.getProperty("jvmci.printconfig"))) {
            printConfig(config, compilerToVm);
        }
    }

    private JVMCIBackend registerBackend(JVMCIBackend backend) {
        Class<? extends Architecture> arch = backend.getCodeCache().getTarget().arch.getClass();
        JVMCIBackend oldValue = backends.put(arch, backend);
        assert oldValue == null : "cannot overwrite existing backend for architecture " + arch.getSimpleName();
        return backend;
    }

    public ResolvedJavaType fromClass(Class<?> javaClass) {
        return metaAccessContext.fromClass(javaClass);
    }

    public HotSpotVMConfig getConfig() {
        return config;
    }

    public CompilerToVM getCompilerToVM() {
        return compilerToVm;
    }

    public JVMCIMetaAccessContext getMetaAccessContext() {
        return metaAccessContext;
    }

    public Compiler getCompiler() {
        return compiler;
    }

    public JavaType lookupType(String name, HotSpotResolvedObjectType accessingType, boolean resolve) {
        Objects.requireNonNull(accessingType, "cannot resolve type without an accessing class");
        // If the name represents a primitive type we can short-circuit the lookup.
        if (name.length() == 1) {
            JavaKind kind = JavaKind.fromPrimitiveOrVoidTypeChar(name.charAt(0));
            return fromClass(kind.toJavaClass());
        }

        // Resolve non-primitive types in the VM.
        HotSpotResolvedObjectTypeImpl hsAccessingType = (HotSpotResolvedObjectTypeImpl) accessingType;
        final HotSpotResolvedObjectTypeImpl klass = compilerToVm.lookupType(name, hsAccessingType.mirror(), resolve);

        if (klass == null) {
            assert resolve == false;
            return HotSpotUnresolvedJavaType.create(this, name);
        }
        return klass;
    }

    public JVMCIBackend getHostJVMCIBackend() {
        return hostBackend;
    }

    public <T extends Architecture> JVMCIBackend getJVMCIBackend(Class<T> arch) {
        assert arch != Architecture.class;
        return backends.get(arch);
    }

    public Map<Class<? extends Architecture>, JVMCIBackend> getJVMCIBackends() {
        return Collections.unmodifiableMap(backends);
    }

    /**
     * Called from the VM.
     */
    @SuppressWarnings({"unused"})
    private void compileMethod(HotSpotResolvedJavaMethod method, int entryBCI, long jvmciEnv, int id) {
        compiler.compileMethod(method, entryBCI, jvmciEnv, id);
    }

    /**
     * Shuts down the runtime.
     *
     * Called from the VM.
     */
    @SuppressWarnings({"unused"})
    private void shutdown() throws Exception {
        for (HotSpotVMEventListener vmEventListener : vmEventListeners) {
            vmEventListener.notifyShutdown();
        }
    }

    void notifyInstall(HotSpotCodeCacheProvider hotSpotCodeCacheProvider, InstalledCode installedCode, CompilationResult compResult) {
        for (HotSpotVMEventListener vmEventListener : vmEventListeners) {
            vmEventListener.notifyInstall(hotSpotCodeCacheProvider, installedCode, compResult);
        }
    }

    private static void printConfig(HotSpotVMConfig config, CompilerToVM vm) {
        Field[] fields = config.getClass().getDeclaredFields();
        Map<String, Field> sortedFields = new TreeMap<>();
        for (Field f : fields) {
            if (!f.isSynthetic() && !Modifier.isStatic(f.getModifiers())) {
                f.setAccessible(true);
                sortedFields.put(f.getName(), f);
            }
        }
        for (Field f : sortedFields.values()) {
            try {
                String line = String.format("%9s %-40s = %s%n", f.getType().getSimpleName(), f.getName(), pretty(f.get(config)));
                byte[] lineBytes = line.getBytes();
                vm.writeDebugOutput(lineBytes, 0, lineBytes.length);
                vm.flushDebugOutput();
            } catch (Exception e) {
            }
        }
    }

    private static String pretty(Object value) {
        if (value == null) {
            return "null";
        }

        Class<?> klass = value.getClass();
        if (value instanceof String) {
            return "\"" + value + "\"";
        } else if (value instanceof Method) {
            return "method \"" + ((Method) value).getName() + "\"";
        } else if (value instanceof Class<?>) {
            return "class \"" + ((Class<?>) value).getSimpleName() + "\"";
        } else if (value instanceof Integer) {
            if ((Integer) value < 10) {
                return value.toString();
            }
            return value + " (0x" + Integer.toHexString((Integer) value) + ")";
        } else if (value instanceof Long) {
            if ((Long) value < 10 && (Long) value > -10) {
                return value + "l";
            }
            return value + "l (0x" + Long.toHexString((Long) value) + "l)";
        } else if (klass.isArray()) {
            StringBuilder str = new StringBuilder();
            int dimensions = 0;
            while (klass.isArray()) {
                dimensions++;
                klass = klass.getComponentType();
            }
            int length = Array.getLength(value);
            str.append(klass.getSimpleName()).append('[').append(length).append(']');
            for (int i = 1; i < dimensions; i++) {
                str.append("[]");
            }
            str.append(" {");
            for (int i = 0; i < length; i++) {
                str.append(pretty(Array.get(value, i)));
                if (i < length - 1) {
                    str.append(", ");
                }
            }
            str.append('}');
            return str.toString();
        }
        return value.toString();
    }
}
