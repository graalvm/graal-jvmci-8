/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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
package com.oracle.jvmci.service;

import java.util.*;

import sun.reflect.*;

/**
 * A mechanism on top of the standard {@link ServiceLoader} that enables JVMCI enabled runtime to
 * efficiently load services marked by {@link Service}. This is important to avoid the performance
 * overhead of the standard service loader mechanism for services loaded in the runtime
 * initialization process.
 */
public class Services {

    private static final ClassValue<List<Service>> cache = new ClassValue<List<Service>>() {
        @Override
        protected List<Service> computeValue(Class<?> type) {
            return Arrays.asList(getServiceImpls(type));
        }
    };

    /**
     * Gets an {@link Iterable} of the implementations available for a given service.
     */
    @SuppressWarnings("unchecked")
    @CallerSensitive
    public static <S> Iterable<S> load(Class<S> service) {
        // TODO(ds): add SecurityManager checks
        if (Service.class.isAssignableFrom(service)) {
            try {
                return (Iterable<S>) cache.get(service);
            } catch (UnsatisfiedLinkError e) {
                // Fall back to standard ServiceLoader
            }
        }

        // Need to use the ClassLoader of the caller
        ClassLoader cl = Reflection.getCallerClass().getClassLoader();
        return ServiceLoader.load(service, cl);
    }

    /**
     * Gets the implementation for a given service for which at most one implementation must be
     * available.
     *
     * @param service the service whose implementation is being requested
     * @param required specifies if an {@link InternalError} should be thrown if no implementation
     *            of {@code service} is available
     */
    @SuppressWarnings("unchecked")
    @CallerSensitive
    public static <S> S loadSingle(Class<S> service, boolean required) {
        // TODO(ds): add SecurityManager checks
        Iterable<S> impls = null;
        if (Service.class.isAssignableFrom(service)) {
            try {
                impls = (Iterable<S>) cache.get(service);
            } catch (UnsatisfiedLinkError e) {
                // Fall back to standard ServiceLoader
            }
        }

        if (impls == null) {
            // Need to use the ClassLoader of the caller
            ClassLoader cl = Reflection.getCallerClass().getClassLoader();
            impls = ServiceLoader.load(service, cl);
        }
        S singleImpl = null;
        for (S impl : impls) {
            if (singleImpl != null) {
                throw new InternalError(String.format("Multiple %s implementations found: %s, %s", service.getName(), singleImpl.getClass().getName(), impl.getClass().getName()));
            }
            singleImpl = impl;
        }
        if (singleImpl == null && required) {
            String javaHome = System.getProperty("java.home");
            String vmName = System.getProperty("java.vm.name");
            Formatter errorMessage = new Formatter();
            errorMessage.format("The VM does not expose required service %s.%n", service.getName());
            errorMessage.format("Currently used Java home directory is %s.%n", javaHome);
            errorMessage.format("Currently used VM configuration is: %s", vmName);
            throw new UnsupportedOperationException(errorMessage.toString());
        }
        return singleImpl;
    }

    private static native <S> S[] getServiceImpls(Class<?> service);
}
