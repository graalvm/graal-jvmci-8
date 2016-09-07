/*
 * Copyright (c) 2011, 2016, Oracle and/or its affiliates. All rights reserved.
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

import static java.lang.reflect.Modifier.ABSTRACT;
import static java.lang.reflect.Modifier.FINAL;
import static java.lang.reflect.Modifier.INTERFACE;
import static java.lang.reflect.Modifier.NATIVE;
import static java.lang.reflect.Modifier.PRIVATE;
import static java.lang.reflect.Modifier.PROTECTED;
import static java.lang.reflect.Modifier.PUBLIC;
import static java.lang.reflect.Modifier.STATIC;
import static java.lang.reflect.Modifier.STRICT;
import static java.lang.reflect.Modifier.SYNCHRONIZED;
import static java.lang.reflect.Modifier.TRANSIENT;
import static java.lang.reflect.Modifier.VOLATILE;
import static jdk.vm.ci.hotspot.HotSpotVMConfig.config;

import java.lang.reflect.Modifier;

/**
 * The modifiers not public in {@link Modifier} that need to be retrieved from
 * {@link HotSpotVMConfig}.
 */
public interface HotSpotModifiers {

    // @formatter:off
    int ANNOTATION = config().jvmAccAnnotation;
    int ENUM       = config().jvmAccEnum;
    int VARARGS    = config().jvmAccVarargs;
    int BRIDGE     = config().jvmAccBridge;
    int SYNTHETIC  = config().jvmAccSynthetic;
    // @formatter:on

    static int jvmClassModifiers() {
        return PUBLIC | FINAL | INTERFACE | ABSTRACT | ANNOTATION | ENUM | SYNTHETIC;
    }

    static int jvmMethodModifiers() {
        return PUBLIC | PRIVATE | PROTECTED | STATIC | FINAL | SYNCHRONIZED | BRIDGE | VARARGS | NATIVE | ABSTRACT | STRICT | SYNTHETIC;
    }

    static int jvmFieldModifiers() {
        return PUBLIC | PRIVATE | PROTECTED | STATIC | FINAL | VOLATILE | TRANSIENT | ENUM | SYNTHETIC;
    }
}
