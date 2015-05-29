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

#include "jvmci/jvmciHashtable.hpp"

template<class K, class V> bool JVMCIHashtable<K,V>::add(V value, bool replace) {
  K key = get_key(value);
  unsigned int hash = compute_hash(key);
  unsigned int index = hash_to_index(hash);
  for (JVMCIHashtableEntry<V>* e = bucket(index); e != NULL; e = e->next()) {
    if (key_equals(get_key(e->literal_addr()), key)) {
      if (replace) {
        e->set_literal(value);
      }
      return false;
    }
  }
  JVMCIHashtableEntry<V>* e = new JVMCIHashtableEntry<V>(value);
  e->set_next(_buckets[index]);
  _buckets[index] = e;
  ++_number_of_entries;
  return true;
}

template<class K, class V> V* JVMCIHashtable<K,V>::get(K key) {
  unsigned int index = index_for(key);
  for (JVMCIHashtableEntry<V>* e = bucket(index); e != NULL; e = e->next()) {
    if (key_equals(get_key(e->literal_addr()), key)) {
      return e->literal_addr();
    }
  }
  return NULL;
}

template<class K, class V> void JVMCIHashtable<K, V>::for_each(ValueClosure<V>* closure) {
  for (size_t i = 0; i < table_size(); ++i) {
    for (JVMCIHashtableEntry<V>* e = bucket(i); e != NULL && !closure->is_aborted(); e = e->next()) {
      closure->do_value(e->literal_addr());
    }
  }
}

template<class K, class V> JVMCIHashtable<K,V>::~JVMCIHashtable() {
  for (size_t i = 0; i < table_size(); ++i) {
    JVMCIHashtableEntry<V>* e = bucket(i);
    while (e != NULL) {
      JVMCIHashtableEntry<V>* current = e;
      e = e->next();
      delete current;
    }
  }
  FREE_C_HEAP_ARRAY(JVMCIHashtableEntry*, _buckets, mtCompiler);
}

// Instantiation
#include "jvmci/jvmciOptions.hpp"
template class JVMCIHashtable<const char*, OptionDesc>;
template class JVMCIHashtable<const char*, OptionValue>;
