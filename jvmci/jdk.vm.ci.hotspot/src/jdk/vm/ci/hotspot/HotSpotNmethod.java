/*
 * Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.
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
import static jdk.vm.ci.services.Services.IS_IN_NATIVE_IMAGE;

import jdk.vm.ci.code.InstalledCode;
import jdk.vm.ci.code.InvalidInstalledCodeException;
import jdk.vm.ci.meta.JavaKind;
import jdk.vm.ci.meta.JavaType;
import jdk.vm.ci.meta.ResolvedJavaMethod;

/**
 * Implementation of {@link InstalledCode} for code installed as an {@code nmethod}. The address of
 * the {@code nmethod} is stored in {@link InstalledCode#address} and the value of
 * {@code nmethod::verified_entry_point()} is in {@link InstalledCode#entryPoint}.
 * <p>
 * When a {@link HotSpotNmethod} dies, it triggers unloading of the {@code nmethod} unless
 * {@link #isDefault() == true}.
 */
public class HotSpotNmethod extends HotSpotInstalledCode {

    /**
     * This (indirect) {@code Method*} reference is safe since class redefinition preserves all
     * methods associated with nmethods in the code cache.
     */
    private final HotSpotResolvedJavaMethodImpl method;

    /**
     * Specifies whether the {@code nmethod} associated with this object is the code executed by
     * default HotSpot linkage when a normal Java call to {@link #method} is made. That is, does
     * {@code this.method.metadataHandle->_code} == {@code this.address}. If not, then the
     * {@code nmethod} can only be invoked via a reference to this object. An example of this is the
     * trampoline mechanism used by Truffle: https://goo.gl/LX88rZ.
     *
     * HotSpot will unload the {@code nmethod} once this object dies if {@code isDefault == false}.
     */
    private final boolean isDefault;

    /**
     * A snapshot of the nmethod's compile identifier when this object was created. This is non-zero
     * iff a reference to this object is not in the oops table of the nmethod. In that case, every
     * VM call for this object resolves the nmethod in the code cache based on {@link #getAddress()}
     * and {@link #compileIdSnapshot}. If this field is 0, the {@code address} and {@code} fields of
     * this object are updated by the VM if the nmethod dies or is made non-entrant.
     */
    private final long compileIdSnapshot;

    HotSpotNmethod(HotSpotResolvedJavaMethodImpl method, String name, boolean isDefault, long compileId) {
        super(name);
        this.method = method;
        this.isDefault = isDefault;
        this.compileIdSnapshot = isDefault ? compileId : 0L;
    }

    /**
     * Determines if the nmethod associated with this object is the compiled entry point for
     * {@link #getMethod()}. If {@code false}, then the nmethod is unloaded when the VM determines
     * this object has died.
     */
    public boolean isDefault() {
        return isDefault;
    }

    @Override
    public boolean isValid() {
        if (compileIdSnapshot != 0L) {
            compilerToVM().updateHotSpotNmethod(this);
        }
        return super.isValid();
    }

    public ResolvedJavaMethod getMethod() {
        return method;
    }

    @Override
    public void invalidate() {
        compilerToVM().invalidateHotSpotNmethod(this);
    }

    @Override
    public long getAddress() {
        if (compileIdSnapshot != 0L) {
            compilerToVM().updateHotSpotNmethod(this);
        }
        return super.getAddress();
    }

    @Override
    public long getEntryPoint() {
        if (compileIdSnapshot != 0L) {
            return 0;
        }
        return super.getEntryPoint();
    }

    @Override
    public String toString() {
        return String.format("HotSpotNmethod[method=%s, codeBlob=0x%x, isDefault=%b, name=%s]", method, getAddress(), isDefault, name);
    }

    private boolean checkArgs(Object... args) {
        JavaType[] sig = method.toParameterTypes();
        assert args.length == sig.length : method.format("%H.%n(%p): expected ") + sig.length + " args, got " + args.length;
        for (int i = 0; i < sig.length; i++) {
            Object arg = args[i];
            if (arg == null) {
                assert sig[i].getJavaKind() == JavaKind.Object : method.format("%H.%n(%p): expected arg ") + i + " to be Object, not " + sig[i];
            } else if (sig[i].getJavaKind() != JavaKind.Object) {
                assert sig[i].getJavaKind().toBoxedJavaClass() == arg.getClass() : method.format("%H.%n(%p): expected arg ") + i + " to be " + sig[i] + ", not " + arg.getClass();
            }
        }
        return true;
    }

    @Override
    public Object executeVarargs(Object... args) throws InvalidInstalledCodeException {
        if (IS_IN_NATIVE_IMAGE) {
            throw new HotSpotJVMCIUnsupportedOperationError("Cannot execute nmethod via mirror in native image");
        }
        assert checkArgs(args);
        return compilerToVM().executeHotSpotNmethod(args, this);
    }

    @Override
    public long getStart() {
        return isValid() ? super.getStart() : 0;
    }
}
