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
package jdk.internal.jvmci.options;

import static jdk.internal.jvmci.inittimer.InitTimer.*;

import java.io.*;
import java.util.*;

import jdk.internal.jvmci.inittimer.*;

/**
 * This class contains methods for parsing JVMCI options and matching them against a set of
 * {@link OptionDescriptors}. The {@link OptionDescriptors} are loaded from JVMCI jars, either
 * {@linkplain JVMCIJarsOptionDescriptorsProvider directly} or via a {@link ServiceLoader}.
 */
public class OptionsParser {

    private static final OptionValue<Boolean> PrintFlags = new OptionValue<>(false);

    /**
     * A service for looking up {@link OptionDescriptor}s.
     */
    public interface OptionDescriptorsProvider {
        /**
         * Gets the {@link OptionDescriptor} matching a given option {@linkplain Option#name() name}
         * or null if no option of that name is provided by this object.
         */
        OptionDescriptor get(String name);
    }

    public interface OptionConsumer {
        void set(OptionDescriptor desc, Object value);
    }

    /**
     * Finds the index of the next character in {@code s} starting at {@code from} that is a space
     * iff {@code spaces == true}.
     */
    private static int skip(String s, int from, boolean spaces) {
        for (int i = from; i < s.length(); i++) {
            if ((s.charAt(i) != ' ') == spaces) {
                return i;
            }
        }
        return s.length();
    }

    /**
     * Parses the set of space separated JVMCI options in {@code options}.
     *
     * Called from VM. This method has an object return type to allow it to be called with a VM
     * utility function used to call other static initialization methods.
     *
     * @param options space separated set of JVMCI options to parse
     */
    public static Boolean parseOptionsFromVM(String options) {
        try (InitTimer t = timer("ParseOptions")) {
            JVMCIJarsOptionDescriptorsProvider odp = new JVMCIJarsOptionDescriptorsProvider();
            assert options != null;
            int index = skip(options, 0, true);
            while (index < options.length()) {
                int end = skip(options, index, false);
                String option = options.substring(index, end);
                parseOption(option, null, odp);
                index = skip(options, end, true);
            }
        }
        return Boolean.TRUE;
    }

    public static void parseOption(String option, OptionConsumer setter, OptionDescriptorsProvider odp) {
        parseOption(OptionsLoader.options, option, setter, odp);
    }

    /**
     * Parses a given option value specification.
     *
     * @param option the specification of an option and its value
     * @param setter the object to notify of the parsed option and value
     * @throws IllegalArgumentException if there's a problem parsing {@code option}
     */
    public static void parseOption(SortedMap<String, OptionDescriptor> options, String option, OptionConsumer setter, OptionDescriptorsProvider odp) {
        if (option.length() == 0) {
            return;
        }

        Object value = null;
        String optionName = null;
        String valueString = null;

        char first = option.charAt(0);
        if (first == '+' || first == '-') {
            optionName = option.substring(1);
            value = (first == '+');
        } else {
            int index = option.indexOf('=');
            if (index == -1) {
                optionName = option;
                valueString = null;
            } else {
                optionName = option.substring(0, index);
                valueString = option.substring(index + 1);
            }
        }

        OptionDescriptor desc = odp == null ? options.get(optionName) : odp.get(optionName);
        if (desc == null && value != null) {
            int index = option.indexOf('=');
            if (index != -1) {
                optionName = option.substring(1, index);
                desc = odp == null ? options.get(optionName) : odp.get(optionName);
            }
            if (desc == null && optionName.equals("PrintFlags")) {
                desc = OptionDescriptor.create("PrintFlags", Boolean.class, "Prints all JVMCI flags and exits", OptionsParser.class, "PrintFlags", PrintFlags);
            }
        }
        if (desc == null) {
            List<OptionDescriptor> matches = fuzzyMatch(options, optionName);
            Formatter msg = new Formatter();
            msg.format("Could not find option %s", optionName);
            if (!matches.isEmpty()) {
                msg.format("%nDid you mean one of the following?");
                for (OptionDescriptor match : matches) {
                    boolean isBoolean = match.getType() == Boolean.class;
                    msg.format("%n    %s%s%s", isBoolean ? "(+/-)" : "", match.getName(), isBoolean ? "" : "=<value>");
                }
            }
            throw new IllegalArgumentException(msg.toString());
        }

        Class<?> optionType = desc.getType();

        if (value == null) {
            if (optionType == Boolean.TYPE || optionType == Boolean.class) {
                throw new IllegalArgumentException("Boolean option '" + optionName + "' must use +/- prefix");
            }

            if (valueString == null) {
                throw new IllegalArgumentException("Missing value for non-boolean option '" + optionName + "' must use " + optionName + "=<value> format");
            }

            if (optionType == Float.class) {
                value = Float.parseFloat(valueString);
            } else if (optionType == Double.class) {
                value = Double.parseDouble(valueString);
            } else if (optionType == Integer.class) {
                value = Integer.valueOf((int) parseLong(valueString));
            } else if (optionType == Long.class) {
                value = Long.valueOf(parseLong(valueString));
            } else if (optionType == String.class) {
                value = valueString;
            } else {
                throw new IllegalArgumentException("Wrong value for option '" + optionName + "'");
            }
        } else {
            if (optionType != Boolean.class) {
                throw new IllegalArgumentException("Non-boolean option '" + optionName + "' can not use +/- prefix. Use " + optionName + "=<value> format");
            }
        }
        if (setter == null) {
            desc.getOptionValue().setValue(value);
        } else {
            setter.set(desc, value);
        }

        if (PrintFlags.getValue()) {
            printFlags(options, "JVMCI", System.out);
            System.exit(0);
        }
    }

    private static long parseLong(String v) {
        String valueString = v.toLowerCase();
        long scale = 1;
        if (valueString.endsWith("k")) {
            scale = 1024L;
        } else if (valueString.endsWith("m")) {
            scale = 1024L * 1024L;
        } else if (valueString.endsWith("g")) {
            scale = 1024L * 1024L * 1024L;
        } else if (valueString.endsWith("t")) {
            scale = 1024L * 1024L * 1024L * 1024L;
        }

        if (scale != 1) {
            /* Remove trailing scale character. */
            valueString = valueString.substring(0, valueString.length() - 1);
        }

        return Long.parseLong(valueString) * scale;
    }

    /**
     * Wraps some given text to one or more lines of a given maximum width.
     *
     * @param text text to wrap
     * @param width maximum width of an output line, exception for words in {@code text} longer than
     *            this value
     * @return {@code text} broken into lines
     */
    private static List<String> wrap(String text, int width) {
        List<String> lines = Collections.singletonList(text);
        if (text.length() > width) {
            String[] chunks = text.split("\\s+");
            lines = new ArrayList<>();
            StringBuilder line = new StringBuilder();
            for (String chunk : chunks) {
                if (line.length() + chunk.length() > width) {
                    lines.add(line.toString());
                    line.setLength(0);
                }
                if (line.length() != 0) {
                    line.append(' ');
                }
                String[] embeddedLines = chunk.split("%n", -2);
                if (embeddedLines.length == 1) {
                    line.append(chunk);
                } else {
                    for (int i = 0; i < embeddedLines.length; i++) {
                        line.append(embeddedLines[i]);
                        if (i < embeddedLines.length - 1) {
                            lines.add(line.toString());
                            line.setLength(0);
                        }
                    }
                }
            }
            if (line.length() != 0) {
                lines.add(line.toString());
            }
        }
        return lines;
    }

    public static void printFlags(SortedMap<String, OptionDescriptor> sortedOptions, String prefix, PrintStream out) {
        out.println("[List of " + prefix + " options]");
        for (Map.Entry<String, OptionDescriptor> e : sortedOptions.entrySet()) {
            e.getKey();
            OptionDescriptor desc = e.getValue();
            Object value = desc.getOptionValue().getValue();
            List<String> helpLines = wrap(desc.getHelp(), 70);
            out.println(String.format("%9s %-40s = %-14s %s", desc.getType().getSimpleName(), e.getKey(), value, helpLines.get(0)));
            for (int i = 1; i < helpLines.size(); i++) {
                out.println(String.format("%67s %s", " ", helpLines.get(i)));
            }
        }
    }

    /**
     * Compute string similarity based on Dice's coefficient.
     *
     * Ported from str_similar() in globals.cpp.
     */
    static float stringSimiliarity(String str1, String str2) {
        int hit = 0;
        for (int i = 0; i < str1.length() - 1; ++i) {
            for (int j = 0; j < str2.length() - 1; ++j) {
                if ((str1.charAt(i) == str2.charAt(j)) && (str1.charAt(i + 1) == str2.charAt(j + 1))) {
                    ++hit;
                    break;
                }
            }
        }
        return 2.0f * hit / (str1.length() + str2.length());
    }

    private static final float FUZZY_MATCH_THRESHOLD = 0.7F;

    /**
     * Returns the set of options that fuzzy match a given option name.
     */
    private static List<OptionDescriptor> fuzzyMatch(SortedMap<String, OptionDescriptor> options, String optionName) {
        List<OptionDescriptor> matches = new ArrayList<>();
        for (Map.Entry<String, OptionDescriptor> e : options.entrySet()) {
            float score = stringSimiliarity(e.getKey(), optionName);
            if (score >= FUZZY_MATCH_THRESHOLD) {
                matches.add(e.getValue());
            }
        }
        return matches;
    }
}
