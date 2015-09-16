/*
 * Copyright (c) 2014, 2015, Oracle and/or its affiliates. All rights reserved.
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
package jdk.internal.jvmci.hotspot;

import java.util.Objects;

import jdk.internal.jvmci.hotspot.HotSpotVMConfig.CompressEncoding;
import jdk.internal.jvmci.meta.Constant;
import jdk.internal.jvmci.meta.VMConstant;

public final class HotSpotMetaspaceConstantImpl implements HotSpotMetaspaceConstant, VMConstant, HotSpotProxified {

    static HotSpotMetaspaceConstantImpl forMetaspaceObject(long rawValue, Object metaspaceObject, boolean compressed) {
        return new HotSpotMetaspaceConstantImpl(rawValue, metaspaceObject, compressed);
    }

    static Object getMetaspaceObject(Constant constant) {
        return ((HotSpotMetaspaceConstantImpl) constant).metaspaceObject;
    }

    private final Object metaspaceObject;
    private final long rawValue;
    private final boolean compressed;

    private HotSpotMetaspaceConstantImpl(long rawValue, Object metaspaceObject, boolean compressed) {
        this.metaspaceObject = metaspaceObject;
        this.rawValue = rawValue;
        this.compressed = compressed;
    }

    @Override
    public int hashCode() {
        return System.identityHashCode(metaspaceObject) ^ (int) (rawValue ^ (rawValue >>> 32)) ^ (compressed ? 1 : 2);
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) {
            return true;
        }
        if (!(o instanceof HotSpotMetaspaceConstantImpl)) {
            return false;
        }

        HotSpotMetaspaceConstantImpl other = (HotSpotMetaspaceConstantImpl) o;
        return Objects.equals(this.metaspaceObject, other.metaspaceObject) && this.rawValue == other.rawValue && this.compressed == other.compressed;
    }

    @Override
    public String toValueString() {
        return String.format("meta[%08x]{%s%s}", rawValue, metaspaceObject, compressed ? ";compressed" : "");
    }

    @Override
    public String toString() {
        return toValueString();
    }

    public boolean isDefaultForKind() {
        return rawValue == 0;
    }

    public boolean isCompressed() {
        return compressed;
    }

    public Constant compress(CompressEncoding encoding) {
        assert !isCompressed();
        HotSpotMetaspaceConstantImpl res = HotSpotMetaspaceConstantImpl.forMetaspaceObject(encoding.compress(rawValue), metaspaceObject, true);
        assert res.isCompressed();
        return res;
    }

    public Constant uncompress(CompressEncoding encoding) {
        assert isCompressed();
        HotSpotMetaspaceConstantImpl res = HotSpotMetaspaceConstantImpl.forMetaspaceObject(encoding.uncompress((int) rawValue), metaspaceObject, false);
        assert !res.isCompressed();
        return res;
    }

    public HotSpotResolvedObjectType asResolvedJavaType() {
        if (metaspaceObject instanceof HotSpotResolvedObjectType) {
            return (HotSpotResolvedObjectType) metaspaceObject;
        }
        return null;
    }

    public HotSpotResolvedJavaMethod asResolvedJavaMethod() {
        if (metaspaceObject instanceof HotSpotResolvedJavaMethod) {
            return (HotSpotResolvedJavaMethod) metaspaceObject;
        }
        return null;
    }

    public long rawValue() {
        return rawValue;
    }
}
