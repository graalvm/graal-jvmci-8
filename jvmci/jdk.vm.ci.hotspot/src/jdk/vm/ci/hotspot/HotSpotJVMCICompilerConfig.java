/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
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

import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;

import jdk.vm.ci.code.CompilationRequest;
import jdk.vm.ci.code.CompilationRequestResult;
import jdk.vm.ci.common.JVMCIError;
import jdk.vm.ci.hotspot.HotSpotJVMCIRuntime.Option;
import jdk.vm.ci.runtime.JVMCICompiler;
import jdk.vm.ci.runtime.JVMCIRuntime;
import jdk.vm.ci.runtime.services.JVMCICompilerFactory;
import jdk.vm.ci.runtime.services.JVMCICompilerFactory.AutoSelectionPrecedence;
import jdk.vm.ci.services.Services;

final class HotSpotJVMCICompilerConfig {

    private static class DummyCompilerFactory extends JVMCICompilerFactory implements JVMCICompiler {

        public CompilationRequestResult compileMethod(CompilationRequest request) {
            throw new JVMCIError("no JVMCI compiler selected");
        }

        @Override
        public String getCompilerName() {
            return "<none>";
        }

        @Override
        public JVMCICompiler createCompiler(JVMCIRuntime runtime) {
            return this;
        }
    }

    /**
     * Factory of the selected system compiler.
     */
    private static JVMCICompilerFactory compilerFactory;

    /**
     * Comparator that sorts available {@link JVMCICompilerFactory} objects according to their
     * {@link JVMCICompilerFactory#getAutoSelectionRelationTo(JVMCICompilerFactory) relative}
     * auto-selection preferences. Factories with higher preferences are sorted earlier. If a
     */
    static class FactoryComparator implements Comparator<JVMCICompilerFactory> {

        /**
         * Compares two compiler factories and returns -1 if {@code o1} should be auto-selected over
         * {@code o2}, -1 if {@code o1} should be auto-selected over {@code o2} or 0 if
         * {@code o1 == o2 || o1.getClass() == o2.getClass()}.
         *
         * @throws JVMCIError there is no auto-selection preference relation between {@code o1} and
         *             {@code o2}
         */
        public int compare(JVMCICompilerFactory o1, JVMCICompilerFactory o2) {
            if (o1 == o2 || o1.getClass() == o2.getClass()) {
                return 0;
            }
            AutoSelectionPrecedence o1Precedence = o1.getAutoSelectionRelationTo(o2);
            AutoSelectionPrecedence o2Precedence = o2.getAutoSelectionRelationTo(o1);
            switch (o1Precedence) {
                case HIGHER: {
                    assert o2Precedence != o1Precedence : "auto selection precedence of " + o1 + " and " + o2 + " cannot both be " + o1Precedence;
                    return -1;
                }
                case LOWER: {
                    assert o2Precedence != o1Precedence : "auto selection precedence of " + o1 + " and " + o2 + " cannot both be " + o1Precedence;
                    return 1;
                }
                case UNRELATED: {
                    switch (o2Precedence) {
                        case HIGHER: {
                            return 1;
                        }
                        case LOWER: {
                            return -1;
                        }
                        default:
                            break;
                    }
                }
            }
            // No auto-selection preference relation between o1 and o2
            throw new JVMCIError("JVMCI compiler must be specified with the '%s' system property", Option.JVMCI_OPTION_PROPERTY_PREFIX + Option.Compiler);
        }
    }

    /**
     * Gets the selected system compiler factory.
     *
     * @return the selected system compiler factory
     */
    static JVMCICompilerFactory getCompilerFactory() {
        if (compilerFactory == null) {
            JVMCICompilerFactory factory = null;
            String compilerName = Option.Compiler.getString();
            if (compilerName != null) {
                for (JVMCICompilerFactory f : Services.load(JVMCICompilerFactory.class)) {
                    if (f.getCompilerName().equals(compilerName)) {
                        factory = f;
                    }
                }
                if (factory == null) {
                    throw new JVMCIError("JVMCI compiler '%s' not found", compilerName);
                }
            } else {
                // Auto selection
                ArrayList<JVMCICompilerFactory> factories = new ArrayList<>();
                for (JVMCICompilerFactory f : Services.load(JVMCICompilerFactory.class)) {
                    factories.add(f);
                }
                if (!factories.isEmpty()) {
                    if (factories.size() == 1) {
                        factory = factories.get(0);
                    } else {
                        Collections.sort(factories, new FactoryComparator());
                        factory = factories.get(0);
                    }
                } else {
                    factory = new DummyCompilerFactory();
                }
                factory.onSelection();
            }
            assert factory != null;
            compilerFactory = factory;
        }
        return compilerFactory;
    }
}
