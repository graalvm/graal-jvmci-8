/*
 * Copyright (c) 2018, 2018, Oracle and/or its affiliates. All rights reserved.
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

import static jdk.vm.ci.hotspot.CompilerToVM.compilerToVM;

/**
 * A subclass of {@link HotSpotNmethod} with cooperative update semantics. It cannot be used for
 * invocation but will properly answer all other queries. It doesn't support the
 * {@link HotSpotNmethod#isDefault} semantics of triggering unloading of the corresponding nmethod
 * when the instance is reclaimed.
 */
class HotSpotNmethodHandle extends HotSpotNmethod {
    @SuppressFBWarnings(value = "UWF_UNWRITTEN_FIELD", justification = "field is set by the native part") private long compileId;

    HotSpotNmethodHandle(HotSpotResolvedJavaMethodImpl method, String name, boolean isDefault) {
        super(method, name, isDefault);
    }

    @Override
    public Object executeVarargs(Object... args) {
        throw new HotSpotJVMCIUnsupportedOperationError("unsupported from native image");
    }

    @Override
    public boolean isValid() {
        compilerToVM().updateHotSpotNmethodHandle(this);
        return super.isValid();
    }

    @Override
    public boolean isAlive() {
        compilerToVM().updateHotSpotNmethodHandle(this);
        return super.isAlive();
    }

    @Override
    public long getAddress() {
        compilerToVM().updateHotSpotNmethodHandle(this);
        return super.getAddress();
    }

    @Override
    public long getEntryPoint() {
        return 0;
    }
}
