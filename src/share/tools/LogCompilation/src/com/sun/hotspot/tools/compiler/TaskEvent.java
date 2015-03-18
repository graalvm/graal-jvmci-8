/*
 * Copyright (c) 2009, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

package com.sun.hotspot.tools.compiler;

import java.io.PrintStream;

public class TaskEvent extends BasicLogEvent {
    private int level;
    private String comment;
    private Kind kind;

    enum Kind {
        Enqueue,
        Dequeue,
        Finish
    }
    
    public TaskEvent(double start, String id, int level, Kind kind) {
        super(start, id);
        this.level = level;
        this.kind = kind;
    }

    void setComment(String comment) {
        this.comment = comment;
    }

    String getComment() {
        return comment;
    }

    int getLevel() {
        return level;
    }

    Kind getKind() {
        return kind;
    }

    public void print(PrintStream stream) {
        stream.printf("%s id='%s' level='%s' %s%n", start, id, level, kind);
    }
}
