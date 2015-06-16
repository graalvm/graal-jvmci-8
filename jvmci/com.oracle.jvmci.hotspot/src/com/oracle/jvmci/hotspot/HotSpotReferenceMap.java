/*
 * Copyright (c) 2009, 2015, Oracle and/or its affiliates. All rights reserved.
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
import com.oracle.jvmci.common.*;
import com.oracle.jvmci.meta.*;

public final class HotSpotReferenceMap extends ReferenceMap {

    private Location[] objects;
    private Location[] derivedBase;
    private int[] sizeInBytes;
    private int maxRegisterSize;

    private ArrayList<Value> objectValues;
    private int objectCount;

    private final TargetDescription target;

    public HotSpotReferenceMap(TargetDescription target) {
        this.target = target;
        this.objectCount = 0;
    }

    @Override
    public void reset() {
        objects = null;
        derivedBase = null;
        sizeInBytes = null;
        maxRegisterSize = 0;

        objectValues = new ArrayList<>();
        objectCount = 0;
    }

    @Override
    public void addLiveValue(Value v) {
        if (isConstant(v)) {
            return;
        }
        LIRKind lirKind = v.getLIRKind();
        if (!lirKind.isValue()) {
            objectValues.add(v);
            if (lirKind.isDerivedReference()) {
                objectCount++;
            } else {
                objectCount += lirKind.getReferenceCount();
            }
        }
        if (isRegister(v)) {
            int size = target.getSizeInBytes(lirKind.getPlatformKind());
            if (size > maxRegisterSize) {
                maxRegisterSize = size;
            }
        }
    }

    @Override
    public void finish() {
        objects = new Location[objectCount];
        derivedBase = new Location[objectCount];
        sizeInBytes = new int[objectCount];

        int idx = 0;
        for (Value obj : objectValues) {
            LIRKind kind = obj.getLIRKind();
            int bytes = bytesPerElement(kind);
            if (kind.isDerivedReference()) {
                throw JVMCIError.unimplemented("derived references not yet implemented");
            } else {
                for (int i = 0; i < kind.getPlatformKind().getVectorLength(); i++) {
                    if (kind.isReference(i)) {
                        objects[idx] = toLocation(obj, i * bytes);
                        derivedBase[idx] = null;
                        sizeInBytes[idx] = bytes;
                        idx++;
                    }
                }
            }
        }

        assert idx == objectCount;
        objectValues = null;
        objectCount = 0;
    }

    private int bytesPerElement(LIRKind kind) {
        PlatformKind platformKind = kind.getPlatformKind();
        return target.getSizeInBytes(platformKind) / platformKind.getVectorLength();
    }

    private static Location toLocation(Value v, int offset) {
        if (isRegister(v)) {
            return Location.subregister(asRegister(v), offset);
        } else {
            StackSlot s = asStackSlot(v);
            return Location.stack(s.getRawOffset() + offset, s.getRawAddFrameSize());
        }
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
