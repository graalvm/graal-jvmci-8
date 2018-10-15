/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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
package jdk.vm.ci.services;

import java.lang.reflect.Field;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Formatter;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Properties;
import java.util.ServiceLoader;

import sun.misc.VM;
import sun.reflect.Reflection;

/**
 * An mechanism for accessing service providers via JVMCI. These providers are loaded via a JVMCI
 * class loader that is hidden from application code. Hence the {@link SecurityManager} checks in
 * {@link #load(Class)} and {@link #loadSingle(Class, boolean)}.
 */
public final class Services {

    /**
     * Guards code that should be run when building an JVMCI shared library but should be excluded
     * from (being compiled into) the library. Such code must be directly guarded by an {@code if}
     * statement on this field - the guard cannot be behind a method call.
     */
    public static final boolean IS_BUILDING_NATIVE_IMAGE = Boolean.getBoolean("jdk.vm.ci.services.aot");

    /**
     * Guards code that should only be run in a JVMCI shared library. Such code must be directly
     * guarded by an {@code if} statement on this field - the guard cannot be behind a method call.
     *
     * The value of this field in a JVMCI shared library runtime must be {@code true}.
     */
    public static final boolean IS_IN_NATIVE_IMAGE;
    static {
        /*
         * Prevents javac from constant folding use of this field. It is set to true by the process
         * that builds the shared library.
         */
        IS_IN_NATIVE_IMAGE = false;
    }

    private Services() {
    }

    /**
     * Gets the system properties saved when {@link System} is initialized. The caller must not
     * modify the returned value.
     */
    public static Map<String, String> getSavedProperties() {
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkPermission(new JVMCIPermission());
        }
        try {
            Field savedPropsField = VM.class.getDeclaredField("savedProps");
            savedPropsField.setAccessible(true);
            Properties props = (Properties) savedPropsField.get(null);
            Map<String, String> res = new HashMap<>(props.size());
            for (Map.Entry<Object, Object> e : props.entrySet()) {
                res.put((String) e.getKey(), (String) e.getValue());
            }
            return res;
        } catch (Exception e) {
            throw new InternalError(e);
        }
    }

    /**
     * Causes the JVMCI subsystem to be initialized if it isn't already initialized.
     */
    public static void initializeJVMCI() {
        try {
            Class.forName("jdk.vm.ci.runtime.JVMCI", true, Services.getJVMCIClassLoader());
        } catch (ClassNotFoundException e) {
            throw new InternalError(e);
        }
    }

    private static boolean jvmciEnabled = true;

    /**
     * When {@code -XX:-UseJVMCIClassLoader} is in use, JVMCI classes are loaded via the boot class
     * loader. When {@code null} is the second argument to
     * {@link ServiceLoader#load(Class, ClassLoader)}, service lookup will use the system class
     * loader and thus find application classes which violates the API of {@link #load} and
     * {@link #loadSingle}. To avoid this, a class loader that simply delegates to the boot class
     * loader is used.
     */
    static class LazyBootClassPath {
        static final ClassLoader bootClassPath = new ClassLoader(null) {
        };
    }

    private static ClassLoader findBootClassLoaderChild(ClassLoader start) {
        ClassLoader cl = start;
        while (cl.getParent() != null) {
            cl = cl.getParent();
        }
        return cl;
    }

    private static final Map<Class<?>, List<?>> servicesCache = IS_BUILDING_NATIVE_IMAGE ? new HashMap<>() : null;

    @SuppressWarnings("unchecked")
    private static <S> Iterable<S> load0(Class<S> service) {
        if (IS_IN_NATIVE_IMAGE || IS_BUILDING_NATIVE_IMAGE) {
            List<?> list = servicesCache.get(service);
            if (list != null) {
                return (Iterable<S>) list;
            }
            if (IS_IN_NATIVE_IMAGE) {
                throw new InternalError(String.format("No %s providers found when building native image", service.getName()));
            }
        }

        Iterable<S> providers = Collections.emptyList();
        if (jvmciEnabled) {
            ClassLoader cl = null;
            try {
                cl = getJVMCIClassLoader();
                if (cl == null) {
                    cl = LazyBootClassPath.bootClassPath;
                    // JVMCI classes are loaded via the boot class loader.
                    // If we use null as the second argument to ServiceLoader.load,
                    // service loading will use the system class loader
                    // and find classes on the application class path. Since we
                    // don't want this, we use a loader that is as close to the
                    // boot class loader as possible (since it is impossible
                    // to force service loading to use only the boot class loader).
                    cl = findBootClassLoaderChild(ClassLoader.getSystemClassLoader());
                }
                providers = ServiceLoader.load(service, cl);
            } catch (UnsatisfiedLinkError e) {
                jvmciEnabled = false;
            } catch (InternalError e) {
                if (e.getMessage().equals("JVMCI is not enabled")) {
                    jvmciEnabled = false;
                } else {
                    throw e;
                }
            }
        }
        if (IS_BUILDING_NATIVE_IMAGE) {
            synchronized (servicesCache) {
                ArrayList<S> providersList = new ArrayList<>();
                for (S provider : providers) {
                    providersList.add(provider);
                }
                servicesCache.put(service, providersList);
                providers = providersList;
            }
        }
        return providers;
    }

    /**
     * Performs any required security checks and dynamic reconfiguration to allow the module of a
     * given class to access the classes in the JVMCI module.
     *
     * Note: This API exists to provide backwards compatibility for JVMCI clients compiled against a
     * JDK release earlier than 9.
     *
     * @param requestor a class requesting access to the JVMCI module for its module
     * @throws SecurityException if a security manager is present and it denies
     *             {@link JVMCIPermission}
     */
    public static void exportJVMCITo(Class<?> requestor) {
        // There are no modules in JVMCI-8.
    }

    /**
     * Gets an {@link Iterable} of the JVMCI providers available for a given service.
     *
     * @throws SecurityException if a security manager is present and it denies <tt>
     *             {@link RuntimePermission}("jvmci")</tt>
     */
    public static <S> Iterable<S> load(Class<S> service) {
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkPermission(new JVMCIPermission());
        }
        return load0(service);
    }

    /**
     * Gets the JVMCI provider for a given service for which at most one provider must be available.
     *
     * @param service the service whose provider is being requested
     * @param required specifies if an {@link InternalError} should be thrown if no provider of
     *            {@code service} is available
     * @throws SecurityException if a security manager is present and it denies <tt>
     *             {@link RuntimePermission}("jvmci")</tt>
     */
    public static <S> S loadSingle(Class<S> service, boolean required) {
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
            sm.checkPermission(new JVMCIPermission());
        }
        Iterable<S> providers = load0(service);

        S singleProvider = null;
        for (S provider : providers) {
            if (singleProvider != null) {
                throw new InternalError(String.format("Multiple %s providers found: %s, %s", service.getName(), singleProvider.getClass().getName(), provider.getClass().getName()));
            }
            singleProvider = provider;
        }
        if (singleProvider == null && required) {
            String javaHome = System.getProperty("java.home");
            String vmName = System.getProperty("java.vm.name");
            Formatter errorMessage = new Formatter();
            errorMessage.format("The VM does not expose required service %s.%n", service.getName());
            errorMessage.format("Currently used Java home directory is %s.%n", javaHome);
            errorMessage.format("Currently used VM configuration is: %s", vmName);
            throw new UnsupportedOperationException(errorMessage.toString());
        }
        return singleProvider;
    }

    static {
        Reflection.registerMethodsToFilter(Services.class, "getJVMCIClassLoader");
    }

    /**
     * Gets the JVMCI class loader.
     *
     * @throws InternalError with the {@linkplain Throwable#getMessage() message}
     *             {@code "JVMCI is not enabled"} iff JVMCI is not enabled
     */
    private static ClassLoader getJVMCIClassLoader() {
        if (IS_IN_NATIVE_IMAGE) {
            return null;
        }
        return getJVMCIClassLoader0();
    }

    private static native ClassLoader getJVMCIClassLoader0();

}
