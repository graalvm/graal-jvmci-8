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

#ifndef SHARE_VM_GRAAL_GRAAL_HASHTABLE_HPP
#define SHARE_VM_GRAAL_GRAAL_HASHTABLE_HPP

#include "memory/allocation.hpp"
#include "memory/allocation.inline.hpp"

// based on hashtable.hpp

template <class T> class GraalHashtableEntry : public CHeapObj<mtCompiler> {
  friend class VMStructs;
private:
  T               _literal;       // ref to item in table.
  GraalHashtableEntry*  _next;          // Link to next element in the linked list for this bucket

public:
  GraalHashtableEntry(T literal) :  _literal(literal), _next(NULL) {}

  T literal() {
    return _literal;
  }

  void set_literal(T value) {
    _literal = value;
  }

  T* literal_addr() {
    return &_literal;
  }

  GraalHashtableEntry* next() const {
    return _next;
  }

  void set_next(GraalHashtableEntry* next) {
    _next = next;
  }
};

template <class V>
class ValueClosure : public StackObj {
  bool _abort;
protected:
  void abort() { _abort = true; }
public:
  ValueClosure() : _abort(false) {}
  virtual void do_value(V* value) = 0;
  bool is_aborted() { return _abort; }
};

template <class K, class V> class GraalHashtable : public CHeapObj<mtCompiler> {
  friend class VMStructs;
private:
  // Instance variables
  unsigned int             _table_size;
  GraalHashtableEntry<V>** _buckets;
  unsigned int             _number_of_entries;

public:
  GraalHashtable(size_t size) : _table_size(size), _number_of_entries(0) {
    _buckets = NEW_C_HEAP_ARRAY(GraalHashtableEntry<V>*, table_size(), mtCompiler);
    for (size_t i = 0; i < table_size(); ++i) {
      _buckets[i] = NULL;
    }
  }
  virtual ~GraalHashtable();

private:
  // Bucket handling
  unsigned int hash_to_index(unsigned int full_hash) {
    unsigned int h = full_hash % _table_size;
    assert(h >= 0 && h < _table_size, "Illegal hash value");
    return h;
  }

  unsigned  int index_for(K key) {
    return hash_to_index(compute_hash(key));
  }

  size_t entry_size() {
    return sizeof(V);
  }

  size_t table_size() { return _table_size; }

  GraalHashtableEntry<V>* bucket(unsigned int index) {
    return _buckets[index];
  }

  bool add(V v, bool replace);

protected:
  virtual unsigned int compute_hash(K key) = 0;
  virtual bool key_equals(K k1, K k2) = 0;
  virtual K get_key(V value) = 0;
  virtual K get_key(V* value) = 0;

public:
  /**
   * Tries to insert the value in the hash table. Returns false if an entry with the same key already exists.
   * In this case it does *not* replace the existing entry.
   */
  bool add(V v) { return add(v, false); }
  /**
   * Inserts the value in the hash table. Returns false if an entry with the same key already exists.
   * In this case it replaces the existing entry.
   */
  bool put(V v) { return add(v, true); }
  V* get(K k);
  void for_each(ValueClosure<V>* closure);
  int number_of_entries() { return _number_of_entries; }

};

#endif // SHARE_VM_GRAAL_GRAAL_HASHTABLE_HPP
