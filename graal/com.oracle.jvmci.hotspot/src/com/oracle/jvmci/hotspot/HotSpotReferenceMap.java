/*
 * Copyright (c) 2009, 2014, Oracle and/or its affiliates. All rights reserved.
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

import static com.oracle.jvmci.code.ValueUtil.*;

import java.util.*;

import com.oracle.jvmci.code.*;
import com.oracle.jvmci.meta.*;

public final class HotSpotReferenceMap extends ReferenceMap {

    private Value[] objects;
    private int[] bytesPerElement;
    private ArrayList<Value> objectValues;

    private final TargetDescription target;

    public HotSpotReferenceMap(TargetDescription target) {
        this.target = target;
        this.objects = Value.NO_VALUES;
    }

    @Override
    public void reset() {
        objectValues = new ArrayList<>();
        objects = Value.NO_VALUES;
        bytesPerElement = null;
    }

    @Override
    public void addLiveValue(Value v) {
        if (isConstant(v)) {
            return;
        }
        LIRKind lirKind = v.getLIRKind();
        if (!lirKind.isValue()) {
            objectValues.add(v);
        }
    }

    @Override
    public void finish() {
        objects = objectValues.toArray(new Value[objectValues.size()]);
        this.bytesPerElement = new int[objects.length];
        for (int i = 0; i < objects.length; i++) {
            bytesPerElement[i] = bytesPerElement(objects[i].getLIRKind());
        }
        objectValues = null;
    }

    private int bytesPerElement(LIRKind kind) {
        PlatformKind platformKind = kind.getPlatformKind();
        return target.getSizeInBytes(platformKind) / platformKind.getVectorLength();
    }

    @Override
    public int hashCode() {
        throw new UnsupportedOperationException();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) {
            return true;
        }
        if (obj instanceof HotSpotReferenceMap) {
            HotSpotReferenceMap that = (HotSpotReferenceMap) obj;
            if (Arrays.equals(objects, that.objects) && this.target.equals(that.target)) {
                return true;
            }
        }
        return false;
    }

    @Override
    public String toString() {
        return Arrays.toString(objects);
    }
}
