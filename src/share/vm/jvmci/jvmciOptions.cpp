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

#include "precompiled.hpp"
#include "jvmci/jvmciOptions.hpp"
#include "jvmci/jvmciRuntime.hpp"
#include "runtime/arguments.hpp"
#include "utilities/hashtable.inline.hpp"

class OptionsParseClosure : public ParseClosure {
  OptionsTable* _table;
public:
  OptionsParseClosure(OptionsTable* table) : _table(table) {}
  void do_line(char* line) {
    char* idx = strchr(line, '\t');
    if (idx == NULL) {
      warn_and_abort("invalid format: could not find first tab");
      return;
    }
    *idx = '\0';
    char* name = line;
    line = idx + 1;
    idx = strchr(line, '\t');
    if (idx == NULL) {
      warn_and_abort("invalid format: could not find second tab");
      return;
    }
    *idx = '\0';
    if (strlen(line) != 1) {
      warn_and_abort("invalid format: type should be 1 char long");
      return;
    }
    char typeChar = *line;
    line = idx + 1;
    idx = strchr(line, '\t');
    if (idx == NULL) {
      warn_and_abort("invalid format: could not find third tab");
      return;
    }
    *idx = '\0';
    char* help = line;
    line = idx + 1;
    idx = strchr(line, '\t');
    if (idx == NULL) {
      warn_and_abort("invalid format: could not find fourth tab");
      return;
    }
    *idx = '\0';
    char* declaringClass = line;
    line = idx + 1;
    char* fieldClass = line;
    OptionType type;
    switch(typeChar) {
      case 's':
        type = _string;
        break;
      case 'i':
        type = _int;
        break;
      case 'j':
        type = _long;
        break;
      case 'f':
        type = _float;
        break;
      case 'd':
        type = _double;
        break;
      case 'z':
        type = _boolean;
        break;
      default:
        warn_and_abort("unknown type");
        return;
    }
    char* name2 = NEW_C_HEAP_ARRAY(char, (strlen(name) + 1 + strlen(help) + 1 + strlen(declaringClass) + 1 + strlen(fieldClass) + 1), mtCompiler);
    char* help2 = name2 + strlen(name) + 1;
    char* declaringClass2 = help2 + strlen(help) + 1;
    char* fieldClass2 = declaringClass2 + strlen(declaringClass) + 1;
    strcpy(name2, name);
    strcpy(help2, help);
    strcpy(declaringClass2, declaringClass);
    strcpy(fieldClass2, fieldClass);
    OptionDesc desc = {name2, help2, type, declaringClass2, fieldClass2};
    if (!_table->add(desc)) {
      warn_and_abort("duplicate option");
      return;
    }
  }
};

class FreeNamesClosure : public ValueClosure<OptionDesc> {
  void do_value(OptionDesc* desc) {
    if (desc->declaringClass == NULL) {
      return; //skip pseudo-options whose name is not allocated with malloc
    }
    FREE_C_HEAP_ARRAY(char, desc->name, mtCompiler);
  }
};

OptionsTable::~OptionsTable() {
  FreeNamesClosure closure;
  for_each(&closure);
}

OptionsTable* OptionsTable::load_options() {
  OptionsTable* table = new OptionsTable();
  // Add PrintFlags option manually
  OptionDesc printFlagsDesc;
  printFlagsDesc.name = PRINT_FLAGS_ARG;
  printFlagsDesc.type = _boolean;
  printFlagsDesc.help = PRINT_FLAGS_HELP;
  printFlagsDesc.declaringClass = NULL;
  printFlagsDesc.fieldClass = NULL;
  table->add(printFlagsDesc);

  char optionsDir[JVM_MAXPATHLEN];
  const char* fileSep = os::file_separator();
  jio_snprintf(optionsDir, sizeof(optionsDir), "%s%slib%sjvmci%soptions",
               Arguments::get_java_home(), fileSep, fileSep, fileSep);
  DIR* dir = os::opendir(optionsDir);
  if (dir != NULL) {
    struct dirent *entry;
    char *dbuf = NEW_C_HEAP_ARRAY(char, os::readdir_buf_size(optionsDir), mtInternal);
    OptionsParseClosure closure(table);
    while ((entry = os::readdir(dir, (dirent *) dbuf)) != NULL && !closure.is_aborted()) {
      const char* name = entry->d_name;
      char optionFilePath[JVM_MAXPATHLEN];
      jio_snprintf(optionFilePath, sizeof(optionFilePath), "%s%s%s",optionsDir, fileSep, name);
      JVMCIRuntime::parse_lines(optionFilePath, &closure, false);
    }
    FREE_C_HEAP_ARRAY(char, dbuf, mtInternal);
    os::closedir(dir);
    if (closure.is_aborted()) {
      delete table;
      return NULL;
    }
    return table;
  }
  // TODO(gd) should this be silent?
  warning("Could not open jvmci options directory (%s)",optionsDir);
  return table;
}

OptionDesc* OptionsTable::get(const char* name, size_t arglen) {
  char nameOnly[256];
  guarantee(arglen < 256, "Max supported option name len is 256");
  strncpy(nameOnly, name, arglen);
  nameOnly[arglen] = '\0';
  return JVMCIHashtable<const char*, OptionDesc>::get(nameOnly);
}

// Compute string similarity based on Dice's coefficient
static float str_similar(const char* str1, const char* str2) {
  size_t len1 = strlen(str1);
  size_t len2 = strlen(str2);

  if (len1 == 0 || len2 == 0) {
    return 0;
  }

  int hits = 0;
  for (size_t i = 0; i < len1 - 1; ++i) {
    for (size_t j = 0; j < len2 -1; ++j) {
      if ((str1[i] == str2[j]) && (str1[i+1] == str2[j+1])) {
        ++hits;
        break;
      }
    }
  }

  size_t total = len1 + len2;
  return 2.0f * (float) hits / (float) total;
}

float VMOptionsFuzzyMatchSimilarity = 0.7f;

class FuzzyMatchClosure : public ValueClosure<OptionDesc> {
  OptionDesc* _match;
  float _max_score;
  const char* _name;
public:
  FuzzyMatchClosure(const char* name) : _name(name), _match(NULL), _max_score(-1) {}
  void do_value(OptionDesc* value) {
    float score = str_similar(value->name, _name);
    if (score > VMOptionsFuzzyMatchSimilarity && score > _max_score) {
      _max_score = score;
      _match = value;
    }
  }
  OptionDesc* get_match() {
    return _match;
  }
};

OptionDesc * OptionsTable::fuzzy_match(const char* name, size_t length) {
  FuzzyMatchClosure closure(name);
  for_each(&closure);
  return closure.get_match();
}

class FreeStringsClosure : public ValueClosure<OptionValue> {
  void do_value(OptionValue* value) {
    if (value->desc.type == _string) {
      FREE_C_HEAP_ARRAY(char, value->string_value, mtCompiler);
    }
  }
};

OptionsValueTable::~OptionsValueTable() {
  FreeStringsClosure closure;
  for_each(&closure);
  delete _table;
}



OptionValue* OptionsValueTable::get(const char* name, size_t arglen) {
  char nameOnly[256];
  guarantee(arglen < 256, "Max supported option name len is 256");
  strncpy(nameOnly, name, arglen);
  nameOnly[arglen] = '\0';
  return JVMCIHashtable<const char*, OptionValue>::get(nameOnly);
}
