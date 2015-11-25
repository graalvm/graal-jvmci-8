/*
 * Copyright (c) 2011, 2015, Oracle and/or its affiliates. All rights reserved.
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

import java.nio.ByteBuffer;
import java.nio.ByteOrder;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.EnumMap;
import java.util.List;
import java.util.Map;
import java.util.stream.Stream;
import java.util.stream.Stream.Builder;

import jdk.vm.ci.code.BytecodeFrame;
import jdk.vm.ci.code.CompilationResult;
import jdk.vm.ci.code.CompilationResult.CodeAnnotation;
import jdk.vm.ci.code.CompilationResult.CodeComment;
import jdk.vm.ci.code.CompilationResult.DataPatch;
import jdk.vm.ci.code.CompilationResult.ExceptionHandler;
import jdk.vm.ci.code.CompilationResult.Infopoint;
import jdk.vm.ci.code.CompilationResult.JumpTable;
import jdk.vm.ci.code.CompilationResult.Mark;
import jdk.vm.ci.code.CompilationResult.Site;
import jdk.vm.ci.code.DataSection;
import jdk.vm.ci.code.InfopointReason;
import jdk.vm.ci.meta.Assumptions.Assumption;
import jdk.vm.ci.meta.ResolvedJavaMethod;

/**
 * A {@link CompilationResult} with additional HotSpot-specific information required for installing
 * the code in HotSpot's code cache.
 */
public class HotSpotCompiledCode {

    public final String name;
    public final Site[] sites;
    public final ExceptionHandler[] exceptionHandlers;
    public final Comment[] comments;
    public final Assumption[] assumptions;

    public final byte[] targetCode;
    public final int targetCodeSize;

    public final byte[] dataSection;
    public final int dataSectionAlignment;
    public final DataPatch[] dataSectionPatches;

    public final int totalFrameSize;
    public final int customStackAreaOffset;

    /**
     * The list of the methods whose bytecodes were used as input to the compilation. If
     * {@code null}, then the compilation did not record method dependencies. Otherwise, the first
     * element of this array is the root method of the compilation.
     */
    public final ResolvedJavaMethod[] methods;

    public static class Comment {

        public final String text;
        public final int pcOffset;

        public Comment(int pcOffset, String text) {
            this.text = text;
            this.pcOffset = pcOffset;
        }
    }

    public HotSpotCompiledCode(CompilationResult compResult) {
        name = compResult.getName();
        sites = getSortedSites(compResult);
        if (compResult.getExceptionHandlers().isEmpty()) {
            exceptionHandlers = null;
        } else {
            exceptionHandlers = compResult.getExceptionHandlers().toArray(new ExceptionHandler[compResult.getExceptionHandlers().size()]);
        }
        List<CodeAnnotation> annotations = compResult.getAnnotations();
        comments = new Comment[annotations.size()];
        if (!annotations.isEmpty()) {
            for (int i = 0; i < comments.length; i++) {
                CodeAnnotation annotation = annotations.get(i);
                String text;
                if (annotation instanceof CodeComment) {
                    CodeComment codeComment = (CodeComment) annotation;
                    text = codeComment.value;
                } else if (annotation instanceof JumpTable) {
                    JumpTable jumpTable = (JumpTable) annotation;
                    text = "JumpTable [" + jumpTable.low + " .. " + jumpTable.high + "]";
                } else {
                    text = annotation.toString();
                }
                comments[i] = new Comment(annotation.position, text);
            }
        }
        assumptions = compResult.getAssumptions();
        assert validateFrames();

        targetCode = compResult.getTargetCode();
        targetCodeSize = compResult.getTargetCodeSize();

        DataSection data = compResult.getDataSection();
        assert data.isFinalized() : "data section needs to be finalized before code installation";
        dataSection = new byte[data.getSectionSize()];

        ByteBuffer buffer = ByteBuffer.wrap(dataSection).order(ByteOrder.nativeOrder());
        Builder<DataPatch> patchBuilder = Stream.builder();
        data.buildDataSection(buffer, patchBuilder);

        dataSectionAlignment = data.getSectionAlignment();
        dataSectionPatches = patchBuilder.build().toArray(len -> new DataPatch[len]);

        totalFrameSize = compResult.getTotalFrameSize();
        customStackAreaOffset = compResult.getCustomStackAreaOffset();

        methods = compResult.getMethods();
    }

    /**
     * Ensure that all the frames passed into HotSpot are properly formatted with an empty or
     * illegal slot following double word slots.
     */
    private boolean validateFrames() {
        for (Site site : sites) {
            if (site instanceof Infopoint) {
                Infopoint info = (Infopoint) site;
                if (info.debugInfo != null) {
                    BytecodeFrame frame = info.debugInfo.frame();
                    assert frame == null || frame.validateFormat();
                }
            }
        }
        return true;
    }

    static class SiteComparator implements Comparator<Site> {

        /**
         * Defines an order for sorting {@link Infopoint}s based on their
         * {@linkplain Infopoint#reason reasons}. This is used to choose which infopoint to preserve
         * when multiple infopoints collide on the same PC offset.
         */
        static final Map<InfopointReason, Integer> HOTSPOT_INFOPOINT_SORT_ORDER = new EnumMap<>(InfopointReason.class);
        static {
            int order = 0;
            HOTSPOT_INFOPOINT_SORT_ORDER.put(InfopointReason.SAFEPOINT, ++order);
            HOTSPOT_INFOPOINT_SORT_ORDER.put(InfopointReason.CALL, ++order);
            HOTSPOT_INFOPOINT_SORT_ORDER.put(InfopointReason.IMPLICIT_EXCEPTION, ++order);
            HOTSPOT_INFOPOINT_SORT_ORDER.put(InfopointReason.METHOD_START, ++order);
            HOTSPOT_INFOPOINT_SORT_ORDER.put(InfopointReason.METHOD_END, ++order);
            HOTSPOT_INFOPOINT_SORT_ORDER.put(InfopointReason.BYTECODE_POSITION, ++order);
            HOTSPOT_INFOPOINT_SORT_ORDER.put(InfopointReason.SAFEPOINT, ++order);
        }

        /**
         * Records whether any two {@link Infopoint}s had the same {@link Infopoint#pcOffset}.
         */
        boolean sawCollidingInfopoints;

        public int compare(Site s1, Site s2) {
            if (s1.pcOffset == s2.pcOffset) {
                // Marks must come first since patching a call site
                // may need to know the mark denoting the call type
                // (see uses of CodeInstaller::_next_call_type).
                boolean s1IsMark = s1 instanceof Mark;
                boolean s2IsMark = s2 instanceof Mark;
                if (s1IsMark != s2IsMark) {
                    return s1IsMark ? -1 : 1;
                }

                // Infopoints must group together so put them after
                // other Site types.
                boolean s1IsInfopoint = s1 instanceof Infopoint;
                boolean s2IsInfopoint = s2 instanceof Infopoint;
                if (s1IsInfopoint != s2IsInfopoint) {
                    return s1IsInfopoint ? 1 : -1;
                }

                if (s1IsInfopoint) {
                    assert s2IsInfopoint;
                    Infopoint s1Info = (Infopoint) s1;
                    Infopoint s2Info = (Infopoint) s2;
                    sawCollidingInfopoints = true;
                    return HOTSPOT_INFOPOINT_SORT_ORDER.get(s1Info.reason) - HOTSPOT_INFOPOINT_SORT_ORDER.get(s2Info.reason);
                }
            }
            return s1.pcOffset - s2.pcOffset;
        }
    }

    /**
     * HotSpot expects sites to be presented in ascending order of PC (see
     * {@code DebugInformationRecorder::add_new_pc_offset}). In addition, it expects
     * {@link Infopoint} PCs to be unique.
     */
    private static Site[] getSortedSites(CompilationResult target) {
        List<?>[] lists = new List<?>[]{target.getInfopoints(), target.getDataPatches(), target.getMarks()};
        int count = 0;
        for (List<?> list : lists) {
            count += list.size();
        }
        Site[] result = new Site[count];
        int pos = 0;
        for (List<?> list : lists) {
            for (Object elem : list) {
                result[pos++] = (Site) elem;
            }
        }
        SiteComparator c = new SiteComparator();
        Arrays.sort(result, c);
        if (c.sawCollidingInfopoints) {
            Infopoint lastInfopoint = null;
            List<Site> copy = new ArrayList<>(count);
            for (int i = 0; i < count; i++) {
                if (result[i] instanceof Infopoint) {
                    Infopoint info = (Infopoint) result[i];
                    if (lastInfopoint == null || lastInfopoint.pcOffset != info.pcOffset) {
                        lastInfopoint = info;
                        copy.add(info);
                    } else {
                        // Omit this colliding infopoint
                        assert lastInfopoint.reason.compareTo(info.reason) < 0;
                    }
                } else {
                    copy.add(result[i]);
                }
            }
            result = copy.toArray(new Site[copy.size()]);
        }
        return result;
    }

    @Override
    public String toString() {
        return name;
    }
}
