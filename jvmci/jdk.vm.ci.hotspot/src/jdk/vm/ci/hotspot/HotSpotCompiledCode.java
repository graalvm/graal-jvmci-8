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

import jdk.vm.ci.code.CompiledCode;
import jdk.vm.ci.code.site.DataPatch;
import jdk.vm.ci.code.site.Site;
import jdk.vm.ci.meta.Assumptions.Assumption;
import jdk.vm.ci.meta.ResolvedJavaMethod;

/**
 * A {@link CompiledCode} with additional HotSpot-specific information required for installing the
 * code in HotSpot's code cache.
 */
public class HotSpotCompiledCode extends CompiledCode {

    protected final Comment[] comments;

    protected final byte[] dataSection;
    protected final int dataSectionAlignment;
    protected final DataPatch[] dataSectionPatches;

    protected final int totalFrameSize;
    protected final int customStackAreaOffset;

    public static class Comment {

        public final String text;
        public final int pcOffset;

        public Comment(int pcOffset, String text) {
            this.text = text;
            this.pcOffset = pcOffset;
        }
    }

    public HotSpotCompiledCode(String name, byte[] targetCode, int targetCodeSize, Site[] sites, Assumption[] assumptions, ResolvedJavaMethod[] methods, Comment[] comments, byte[] dataSection,
                    int dataSectionAlignment, DataPatch[] dataSectionPatches, int totalFrameSize, int customStackAreaOffset) {
        super(name, targetCode, targetCodeSize, sites, assumptions, methods);
        this.comments = comments;
        this.dataSection = dataSection;
        this.dataSectionAlignment = dataSectionAlignment;
        this.dataSectionPatches = dataSectionPatches;
        this.totalFrameSize = totalFrameSize;
        this.customStackAreaOffset = customStackAreaOffset;
    }
}
