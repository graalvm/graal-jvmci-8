/*
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

#ifndef SHARE_VM_JVMCI_JVMCI_OPTIONS_HPP
#define SHARE_VM_JVMCI_JVMCI_OPTIONS_HPP

#include "memory/allocation.hpp"
#include "utilities/exceptions.hpp"
#include "jvmci/jvmciHashtable.hpp"

#define PRINT_FLAGS_ARG "PrintFlags"
#define PRINT_FLAGS_HELP "Prints all JVMCI flags (similar to XX's PrintFlagsFinal)"

enum OptionType {
  _string,
  _int,
  _long,
  _float,
  _double,
  _boolean
};

struct OptionDesc {
  const char* name;
  const char* help;
  OptionType type;
  const char* declaringClass;
  const char* fieldClass;
};

inline unsigned int compute_string_hash(const char *s, int n) {
  unsigned int val = 0;
  while (--n >= 0) {
    val = *s++ + 31 * val;
  }
  return val;
}

class OptionDescsTable : public JVMCIHashtable<const char*, OptionDesc> {
protected:
  unsigned int compute_hash(const char* key) { return compute_string_hash(key, (int)strlen(key)); }
  bool key_equals(const char* k1, const char* k2) { return strcmp(k1, k2) == 0; }
  const char* get_key(OptionDesc value) { return value.name; } ;
  const char* get_key(OptionDesc* value) { return value->name; } ;
public:
  OptionDescsTable() : JVMCIHashtable<const char*, OptionDesc>(100) {}
  ~OptionDescsTable();
  using JVMCIHashtable<const char*, OptionDesc>::get;
  OptionDesc* get(const char* name, size_t arglen);
  OptionDesc * fuzzy_match(const char* name, size_t length);

  static OptionDescsTable* load_options();
};

struct OptionValue {
  OptionDesc desc;
  union {
    const char* string_value;
    jint int_value;
    jlong long_value;
    jfloat float_value;
    jdouble double_value;
    jboolean boolean_value;
  };
};

class OptionValuesTable : public JVMCIHashtable<const char*, OptionValue> {
  OptionDescsTable* _table;
protected:
  unsigned int compute_hash(const char* key) { return compute_string_hash(key, (int)strlen(key)); }
  bool key_equals(const char* k1, const char* k2) { return strcmp(k1, k2) == 0; }
  const char* get_key(OptionValue value) { return value.desc.name; } ;
  const char* get_key(OptionValue* value) { return value->desc.name; } ;
public:
  OptionValuesTable(OptionDescsTable* table) : _table(table), JVMCIHashtable<const char*, OptionValue>(100) {}
  ~OptionValuesTable();
  using JVMCIHashtable<const char*, OptionValue>::get;
  OptionValue* get(const char* name, size_t arglen);
  OptionDescsTable* options_table() { return _table; }
};


#endif // SHARE_VM_JVMCI_JVMCI_OPTIONS_HPP
