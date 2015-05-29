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
package com.oracle.jvmci.hotspot;

import java.util.*;

import com.oracle.jvmci.options.*;
import com.oracle.jvmci.service.*;

//JaCoCo Exclude

/**
 * Sets JVMCI options from the HotSpot command line. Such options are distinguished by the
 * {@link #JVMCI_OPTION_PREFIX} prefix.
 */
public class HotSpotOptions {

    private static final String JVMCI_OPTION_PREFIX = "-G:";

    /**
     * Called from VM.
     */
    static void printFlags() {
        SortedMap<String, OptionDescriptor> options = new TreeMap<>();
        for (Options opts : Services.load(Options.class)) {
            for (OptionDescriptor desc : opts) {
                if (isHotSpotOption(desc)) {
                    String name = desc.getName();
                    OptionDescriptor existing = options.put(name, desc);
                    assert existing == null : "Option named \"" + name + "\" has multiple definitions: " + existing.getLocation() + " and " + desc.getLocation();
                }
            }
        }

        OptionUtils.printFlags(options, JVMCI_OPTION_PREFIX);
    }

    /**
     * Determines if a given option is a HotSpot command line option.
     */
    private static boolean isHotSpotOption(OptionDescriptor desc) {
        return desc.getDeclaringClass().getName().startsWith("com.oracle.graal");
    }
}
