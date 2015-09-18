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
package jdk.internal.jvmci.hotspot.jfr.events;

import static jdk.internal.jvmci.hotspot.HotSpotVMConfig.config;

import java.net.URISyntaxException;

import jdk.internal.jvmci.hotspot.events.EmptyEventProvider.EmptyCompilationEvent;
import jdk.internal.jvmci.hotspot.events.EmptyEventProvider.EmptyCompilerFailureEvent;
import jdk.internal.jvmci.hotspot.events.EventProvider;
import jdk.internal.jvmci.service.ServiceProvider;

/**
 * A JFR implementation for {@link EventProvider}. This implementation is used when Flight Recorder
 * is turned on.
 *
 * Note: The use of fully qualified names for deprecated types is a workaround for <a
 * href="https://bugs.openjdk.java.net/browse/JDK-8032211">JDK-8032211</a>.
 */
@ServiceProvider(EventProvider.class)
@SuppressWarnings("deprecation")
public final class JFREventProvider implements EventProvider {

    private final boolean enabled;

    /**
     * Need to store the producer in a field so that it doesn't disappear.
     */
    @SuppressWarnings("unused") private final com.oracle.jrockit.jfr.Producer producer;

    public JFREventProvider() {
        enabled = config().flightRecorder;
        com.oracle.jrockit.jfr.Producer p = null;
        if (enabled) {
            try {
                /*
                 * The "HotSpot JVM" producer is a native producer and we cannot use it. So we
                 * create our own. This has the downside that Mission Control is confused and
                 * doesn't show JVMCI events in the "Code" tab. There are plans to revise the JFR
                 * code for JDK 9.
                 */
                p = new com.oracle.jrockit.jfr.Producer("HotSpot JVM", "Oracle Hotspot JVM", "http://www.oracle.com/hotspot/jvm/");
                p.register();
                // Register event classes with Producer.
                for (Class<?> c : JFREventProvider.class.getDeclaredClasses()) {
                    if (c.isAnnotationPresent(com.oracle.jrockit.jfr.EventDefinition.class)) {
                        assert com.oracle.jrockit.jfr.InstantEvent.class.isAssignableFrom(c) : c;
                        registerEvent(p, c);
                    }
                }
            } catch (URISyntaxException e) {
                throw new InternalError(e);
            }
        }
        this.producer = p;
    }

    /**
     * Register an event class with the {@link com.oracle.jrockit.jfr.Producer}.
     *
     * @param c event class
     * @return the {@link EventToken event token}
     */
    @SuppressWarnings({"javadoc", "unchecked"})
    private static com.oracle.jrockit.jfr.EventToken registerEvent(com.oracle.jrockit.jfr.Producer producer, Class<?> c) {
        try {
            return producer.addEvent((Class<? extends com.oracle.jrockit.jfr.InstantEvent>) c);
        } catch (com.oracle.jrockit.jfr.InvalidEventDefinitionException | com.oracle.jrockit.jfr.InvalidValueException e) {
            throw new InternalError(e);
        }
    }

    public CompilationEvent newCompilationEvent() {
        if (enabled) {
            return new JFRCompilationEvent();
        }
        return new EmptyCompilationEvent();
    }

    /**
     * A JFR compilation event.
     *
     * <p>
     * See: event {@code Compilation} in {@code src/share/vm/trace/trace.xml}
     */
    @com.oracle.jrockit.jfr.EventDefinition(name = "Compilation", path = "vm/compiler/compilation")
    public static class JFRCompilationEvent extends com.oracle.jrockit.jfr.DurationEvent implements CompilationEvent {

        /*
         * FIXME method should be a Method* but we can't express that in Java.
         */
        @com.oracle.jrockit.jfr.ValueDefinition(name = "Java Method") public String method;
        @com.oracle.jrockit.jfr.ValueDefinition(name = "Compilation ID", relationKey = "COMP_ID") public int compileId;
        @com.oracle.jrockit.jfr.ValueDefinition(name = "Compilation Level") public short compileLevel;
        @com.oracle.jrockit.jfr.ValueDefinition(name = "Succeeded") public boolean succeeded;
        @com.oracle.jrockit.jfr.ValueDefinition(name = "On Stack Replacement") public boolean isOsr;
        @com.oracle.jrockit.jfr.ValueDefinition(name = "Compiled Code Size", contentType = com.oracle.jrockit.jfr.ContentType.Bytes) public int codeSize;
        @com.oracle.jrockit.jfr.ValueDefinition(name = "Inlined Code Size", contentType = com.oracle.jrockit.jfr.ContentType.Bytes) public int inlinedBytes;

        public void setMethod(String method) {
            this.method = method;
        }

        public void setCompileId(int id) {
            this.compileId = id;
        }

        public void setCompileLevel(int compileLevel) {
            this.compileLevel = (short) compileLevel;
        }

        public void setSucceeded(boolean succeeded) {
            this.succeeded = succeeded;
        }

        public void setIsOsr(boolean isOsr) {
            this.isOsr = isOsr;
        }

        public void setCodeSize(int codeSize) {
            this.codeSize = codeSize;
        }

        public void setInlinedBytes(int inlinedBytes) {
            this.inlinedBytes = inlinedBytes;
        }
    }

    public CompilerFailureEvent newCompilerFailureEvent() {
        if (enabled) {
            return new JFRCompilerFailureEvent();
        }
        return new EmptyCompilerFailureEvent();
    }

    /**
     * A JFR compiler failure event.
     *
     * <p>
     * See: event {@code CompilerFailure} in {@code src/share/vm/trace/trace.xml}
     */
    @com.oracle.jrockit.jfr.EventDefinition(name = "Compilation Failure", path = "vm/compiler/failure")
    public static class JFRCompilerFailureEvent extends com.oracle.jrockit.jfr.InstantEvent implements CompilerFailureEvent {

        @com.oracle.jrockit.jfr.ValueDefinition(name = "Compilation ID", relationKey = "COMP_ID") public int compileId;
        @com.oracle.jrockit.jfr.ValueDefinition(name = "Message", description = "The failure message") public String failure;

        public void setCompileId(int id) {
            this.compileId = id;
        }

        public void setMessage(String message) {
            this.failure = message;
        }
    }

}
