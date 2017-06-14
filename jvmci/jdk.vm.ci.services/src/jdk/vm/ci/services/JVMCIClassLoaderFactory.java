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
package jdk.vm.ci.services;

import java.io.File;
import java.io.IOException;
import java.net.MalformedURLException;
import java.net.URL;
import java.net.URLClassLoader;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.ArrayList;
import java.util.List;

import sun.misc.VM;

/**
 * Utility called from the VM to create and register a separate class loader for loading JVMCI
 * classes. The core JVMCI classes are in {@code lib/jvmci/jvmci-api.jar} and
 * {@code lib/jvmci/jvmci-hotspot.jar}. The JVMCI compiler classes are in {@code lib/jvmci/*.jar)}
 * and the class path specified by the {@code "jvmci.class.path.append"} system property. The JVMCI
 * class loader can optionally be given a parent other than the boot class loader as specified by
 * {@link #getJVMCIParentClassLoader(Path)}.
 */
class JVMCIClassLoaderFactory {

    /**
     * Copy of the {@code UseJVMCIClassLoader} VM option. Set by the VM before the static
     * initializer is called.
     */
    private static boolean useJVMCIClassLoader;

    /**
     * Registers the JVMCI class loader in the VM.
     */
    private static native void init(ClassLoader loader);

    static {
        init(useJVMCIClassLoader ? newClassLoader() : null);
    }

    /**
     * Creates a new class loader for loading JVMCI classes.
     */
    private static ClassLoader newClassLoader() {
        Path jvmciDir = Paths.get(VM.getSavedProperty("java.home"), "lib", "jvmci");
        if (!Files.isDirectory(jvmciDir)) {
            throw new InternalError(jvmciDir + " does not exist or is not a directory");
        }
        URL[] urls = getJVMCIJarsUrls(jvmciDir);
        ClassLoader parent = getJVMCIParentClassLoader(jvmciDir);
        return URLClassLoader.newInstance(urls, parent);
    }

    /**
     * Creates a parent class loader for the JVMCI class loader. The class path for the loader is
     * specified in the optional file {@code <java.home>/lib/jvmci/parentClassLoader.classpath}. If
     * the file exists, its contents must be a well formed class path. Relative entries in the class
     * path are resolved against {@code <java.home>/lib/jvmci}.
     *
     * @return {@code null} if {@code <java.home>/lib/jvmci/parentClassLoader.classpath} does not
     *         exist otherwise a {@link URLClassLoader} constructed from the class path in the file
     */
    private static ClassLoader getJVMCIParentClassLoader(Path jvmciDir) {
        Path parentClassPathFile = jvmciDir.resolve("parentClassLoader.classpath");
        if (Files.exists(parentClassPathFile)) {
            String[] entries;
            try {
                entries = new String(Files.readAllBytes(parentClassPathFile)).trim().split(File.pathSeparator);
            } catch (IOException e) {
                throw new InternalError("Error reading " + parentClassPathFile.toAbsolutePath(), e);
            }
            URL[] urls = new URL[entries.length];
            for (int i = 0; i < entries.length; i++) {
                try {
                    if (entries[i].isEmpty()) {
                        throw new InternalError("Class path entry " + i + " in " + parentClassPathFile + " is empty");
                    }
                    // If entries[i] is already absolute, it will remain unchanged
                    Path path = jvmciDir.resolve(entries[i]);
                    if (!Files.exists(path)) {
                        // Unlike the user class path, be strict about this class
                        // path only referring to existing locations.
                        String errorMsg = "Class path entry " + i + " in " + parentClassPathFile + " refers to a file or directory that does not exist: \"" + entries[i] + "\"";
                        if (!path.toString().equals(entries[i])) {
                            errorMsg += " (resolved: \"" + path + "\")";
                        }
                        throw new InternalError(errorMsg);
                    }
                    urls[i] = path.toUri().toURL();
                } catch (MalformedURLException e) {
                    throw new InternalError(e);
                }
            }
            ClassLoader parent = null;
            return URLClassLoader.newInstance(urls, parent);
        }
        return null;
    }

    private static Path ensureExists(Path path) {
        if (!Files.exists(path)) {
            throw new InternalError("Required JVMCI jar is missing: " + path);
        }
        return path;
    }

    /**
     * Gets the URLs for the required JVMCI jars, all the other jar files in the {@code jvmciDir}
     * directory and the entries specified by the {@code "jvmci.class.path.append"} system property.
     */
    private static URL[] getJVMCIJarsUrls(Path jvmciDir) {
        String[] dirEntries = jvmciDir.toFile().list();
        String append = VM.getSavedProperty("jvmci.class.path.append");
        String[] appendEntries = append != null ? append.split(File.pathSeparator) : new String[0];
        List<URL> urls = new ArrayList<>(dirEntries.length + appendEntries.length);

        try {
            urls.add(ensureExists(jvmciDir.resolve("jvmci-api.jar")).toUri().toURL());
            urls.add(ensureExists(jvmciDir.resolve("jvmci-hotspot.jar")).toUri().toURL());

            for (String e : dirEntries) {
                if (e.endsWith(".jar")) {
                    if (!e.equals("jvmci-api.jar") && !e.equals("jvmci-hotspot.jar")) {
                        urls.add(jvmciDir.resolve(e).toUri().toURL());
                    }
                }
            }
            for (int i = 0; i < appendEntries.length; ++i) {
                Path path = Paths.get(appendEntries[i]);
                if (!Files.exists(path)) {
                    // Unlike the user class path, be strict about this class
                    // path only referring to existing locations.
                    String errorMsg = "Entry " + i + " of class path specified by jvmci.class.path.append " +
                                    "system property refers to a file or directory that does not exist: \"" +
                                    appendEntries[i] + "\"";
                    throw new InternalError(errorMsg);
                }
                urls.add(path.toUri().toURL());
            }
        } catch (MalformedURLException e) {
            throw new InternalError(e);
        }

        return urls.toArray(new URL[urls.size()]);
    }
}
