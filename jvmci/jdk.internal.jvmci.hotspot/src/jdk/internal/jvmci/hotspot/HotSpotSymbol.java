/*
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved.
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

import static jdk.internal.jvmci.common.UnsafeAccess.*;
import static jdk.internal.jvmci.hotspot.HotSpotJVMCIRuntime.*;

import java.io.*;
import java.util.*;

/**
 * Represents a metaspace {@code Symbol}.
 */
public class HotSpotSymbol {

    private final long metaspaceSymbol;

    public HotSpotSymbol(long metaspaceSymbol) {
        assert metaspaceSymbol != 0;
        this.metaspaceSymbol = metaspaceSymbol;
    }

    /**
     * Decodes this {@code Symbol} and returns the symbol string as {@link java.lang.String}.
     *
     * @return the decoded string, or null if there was a decoding error
     */
    public String asString() {
        return readModifiedUTF8(asByteArray());
    }

    /**
     * Reads the modified UTF-8 string in {@code buf} and converts it to a {@link String}. The
     * implementation is taken from {@link java.io.DataInputStream#readUTF(DataInput)} and adapted
     * to operate on a {@code byte} array directly for performance reasons.
     *
     * @see java.io.DataInputStream#readUTF(DataInput)
     */
    private static String readModifiedUTF8(byte[] buf) {
        final int utflen = buf.length;
        byte[] bytearr = null;
        char[] chararr = new char[utflen];

        int c;
        int char2;
        int char3;
        int count = 0;
        int chararrCount = 0;

        bytearr = buf;

        while (count < utflen) {
            c = bytearr[count] & 0xff;
            if (c > 127) {
                break;
            }
            count++;
            chararr[chararrCount++] = (char) c;
        }

        while (count < utflen) {
            c = bytearr[count] & 0xff;
            switch (c >> 4) {
                case 0:
                case 1:
                case 2:
                case 3:
                case 4:
                case 5:
                case 6:
                case 7:
                    /* 0xxxxxxx */
                    count++;
                    chararr[chararrCount++] = (char) c;
                    break;
                case 12:
                case 13:
                    /* 110x xxxx 10xx xxxx */
                    count += 2;
                    if (count > utflen) {
                        // malformed input: partial character at end
                        return null;
                    }
                    char2 = bytearr[count - 1];
                    if ((char2 & 0xC0) != 0x80) {
                        // malformed input around byte
                        return null;
                    }
                    chararr[chararrCount++] = (char) (((c & 0x1F) << 6) | (char2 & 0x3F));
                    break;
                case 14:
                    /* 1110 xxxx 10xx xxxx 10xx xxxx */
                    count += 3;
                    if (count > utflen) {
                        // malformed input: partial character at end
                        return null;
                    }
                    char2 = bytearr[count - 2];
                    char3 = bytearr[count - 1];
                    if (((char2 & 0xC0) != 0x80) || ((char3 & 0xC0) != 0x80)) {
                        // malformed input around byte
                        return null;
                    }
                    chararr[chararrCount++] = (char) (((c & 0x0F) << 12) | ((char2 & 0x3F) << 6) | ((char3 & 0x3F) << 0));
                    break;
                default:
                    /* 10xx xxxx, 1111 xxxx */
                    // malformed input around byte
                    return null;
            }
        }
        // The number of chars produced may be less than utflen
        char[] value = Arrays.copyOf(chararr, chararrCount);
        return new String(value);
    }

    private byte[] asByteArray() {
        final int length = getLength();
        byte[] result = new byte[length];
        for (int index = 0; index < length; index++) {
            result[index] = getByteAt(index);
        }
        return result;
    }

    private int getLength() {
        return unsafe.getShort(metaspaceSymbol + runtime().getConfig().symbolLengthOffset);
    }

    private byte getByteAt(int index) {
        return unsafe.getByte(metaspaceSymbol + runtime().getConfig().symbolBodyOffset + index);
    }
}
