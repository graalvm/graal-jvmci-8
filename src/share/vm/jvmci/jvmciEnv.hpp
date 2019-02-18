/*
 * Copyright (c) 1999, 2017, Oracle and/or its affiliates. All rights reserved.
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
 *
 */

#ifndef SHARE_VM_JVMCI_JVMCIENV_HPP
#define SHARE_VM_JVMCI_JVMCIENV_HPP

#include "classfile/systemDictionary.hpp"
#include "code/debugInfoRec.hpp"
#include "code/dependencies.hpp"
#include "code/exceptionHandlerTable.hpp"
#include "compiler/oopMap.hpp"
#include "jvmci/jvmciJavaClasses.hpp"
#include "jvmci/jvmciExceptions.hpp"
#include "runtime/thread.hpp"
#include "memory/oopFactory.hpp"

class CompileTask;
class JVMCIObject;
class JVMCIObjectArray;
class JVMCIPrimitiveArray;
class JVMCICompiler;
class JVMCIRuntime;

// Bring the JVMCI compiler thread into the VM state.
#define JVMCI_VM_ENTRY_MARK                       \
  JavaThread* thread = JavaThread::current(); \
  ThreadInVMfromNative __tiv(thread);       \
  ResetNoHandleMark rnhm;                   \
  HandleMarkCleaner __hm(thread);           \
  Thread* THREAD = thread;                  \
  debug_only(VMNativeEntryWrapper __vew;)

#define JVMCI_EXCEPTION_CONTEXT \
  JavaThread* thread=JavaThread::current(); \
  Thread* THREAD = thread;


class JVMCICompileState;

// Helper to log more context on a JNI exception
#define JVMCI_EXCEPTION_CHECK(env, ...) \
  do { \
    if (env->ExceptionCheck()) { \
      if (env != JavaThread::current()->jni_environment() && JVMCIEnv::get_shared_library_path() != NULL) { \
        tty->print_cr("In JVMCI shared library (%s):", JVMCIEnv::get_shared_library_path()); \
      } \
      tty->print_cr(__VA_ARGS__); \
      return; \
    } \
  } while(0)

// Helper class to ensure that references to Klass* are kept alive for G1
class JVMCIKlassHandle : public StackObj {
 private:
  Klass*     _klass;
  Handle     _holder;
  Thread*    _thread;

  Klass*        klass() const                     { return _klass; }
  Klass*        non_null_klass() const            { assert(_klass != NULL, "resolving NULL _klass"); return _klass; }

 public:
  /* Constructors */
  JVMCIKlassHandle (Thread* thread) : _klass(NULL), _thread(thread) {}
  JVMCIKlassHandle (Thread* thread, Klass* klass);

  JVMCIKlassHandle (const JVMCIKlassHandle &h): _klass(h._klass), _holder(h._holder), _thread(h._thread) {}
  JVMCIKlassHandle& operator=(const JVMCIKlassHandle &s);
  JVMCIKlassHandle& operator=(Klass* klass);

  /* Operators for ease of use */
  Klass*        operator () () const            { return klass(); }
  Klass*        operator -> () const            { return non_null_klass(); }

  bool    operator == (Klass* o) const          { return klass() == o; }
  bool    operator == (const JVMCIKlassHandle& h) const  { return klass() == h.klass(); }

  /* Null checks */
  bool    is_null() const                      { return _klass == NULL; }
  bool    not_null() const                     { return _klass != NULL; }
};

// Wrapper for a JNI call into the JVMCI shared library.
// This performs a ThreadToNativeFromVM transition so that the VM
// will not be blocked if the call takes a long time (e.g., due
// to a GC in the shared library).
class JNIAccessMark : private ThreadToNativeFromVM, private HandleMark {
 private:
  JNIEnv* _env;
 public:
  JNIAccessMark(JVMCIEnv* jvmci_env);
  JNIEnv* env() const { return _env; }
  JNIEnv* operator () () const { return _env; }
};

// This class is a top level wrapper around interactions between HotSpot
// and the JVMCI Java code.  It supports both a HotSpot heap based
// runtime with HotSpot oop based accessors as well as a shared library
// based runtime that is accessed through JNI. It abstracts away all
// interactions with JVMCI objects so that a single version of the
// HotSpot C++ code can can work with either runtime.
class JVMCIEnv : public ResourceObj {
  friend class JNIAccessMark;

  static char*   _shared_library_path;   // argument to os:dll_load
  static void*   _shared_library_handle; // result of os::dll_load
  static JavaVM* _shared_library_javavm; // result of calling JNI_CreateJavaVM in shared library

  // Attaches the current thread to the JavaVM in the shared library,
  // initializing the shared library VM first if necessary.
  // Returns the JNI interface pointer of the current thread.
  // The _shared_library_* fields are initialized by the first
  // call to this method.
  static JNIEnv* attach_shared_library();

  // Initializes the _env, _mode and _runtime fields.
  void init_env_mode_runtime(JNIEnv* parent_env);

  void init(bool is_hotspot, const char* file, int line);

  JNIEnv*                _env;     // JNI env for calling into shared library
  JVMCIRuntime*          _runtime; // Access to a HotSpotJVMCIRuntime
  JVMCIGlobals::JavaMode _mode;    // Which heap is the HotSpotJVMCIRuntime in
  bool        _throw_to_caller;    // Propagate an exception raised in this env to the caller?
  const char*            _file;    // The file and ...
  int                    _line;    // ... line where this JNIEnv was created

  // Translates an exception on the HotSpot heap to an exception on
  // the shared library heap. The translation includes the stack and
  // causes of `throwable`. The translated exception is pending in the
  // shared library thread upon returning.
  void translate_hotspot_exception_to_jni_exception(JavaThread* THREAD, Handle throwable);

public:
  // Opens a JVMCIEnv scope for a Java to VM call (e.g., via CompilerToVM).
  // An exception occurring within the scope is left pending when the
  // scope closes so that it will be propagated back to Java.
  // The JVMCIEnv destructor translates the exception object for the
  // Java runtime if necessary.
  JVMCIEnv(JNIEnv* env, const char* file, int line);

  // Opens a JVMCIEnv scope for a compilation scheduled by the CompileBroker.
  // An exception occurring within the scope must not be propagated back to
  // the CompileBroker.
  JVMCIEnv(JVMCICompileState* compile_state, const char* file, int line);

  // Opens a JNIEnv scope for a call from within the VM. An exception occurring
  // within the scope must not be propagated back to the caller.
  JVMCIEnv(JavaThread* env, const char* file, int line);

  // Opens a JNIEnv scope for accessing `for_object`. An exception occurring
  // within the scope must not be propagated back to the caller.
  JVMCIEnv(JVMCIObject for_object, const char* file, int line) {
    // A JNI call to access an object in the shared library heap
    // can block or take a long time so do not allow such access
    // on the VM thread.
    assert(for_object.is_hotspot() || !Thread::current()->is_VM_thread(),
        "cannot open JVMCIEnv scope when in the VM thread for accessing a shared library heap object");
    init(for_object.is_hotspot(), file, line);
  }

  // Opens a JNIEnv scope for the HotSpot runtime if `is_hotspot` is true
  // otherwise for the shared library runtime. An exception occurring
  // within the scope must not be propagated back to the caller.
  JVMCIEnv(bool is_hotspot, const char* file, int line) {
    init(is_hotspot, file, line);
  }

  ~JVMCIEnv();

  JVMCIRuntime* runtime() {
    return _runtime;
  }

  jboolean has_pending_exception() {
    if (!is_hotspot()) {
      JNIAccessMark jni(this);
      return jni()->ExceptionCheck();
    } else {
      Thread* THREAD = Thread::current();
      return HAS_PENDING_EXCEPTION;
    }
  }

  jboolean clear_pending_exception();

  // Prints an exception and stack trace of a pending exception.
  void describe_pending_exception(bool clear);

  int get_length(JVMCIArray array) {
    if (is_hotspot()) {
      return HotSpotJVMCI::resolve(array)->length();
    } else {
      JNIAccessMark jni(this);
      return jni()->GetArrayLength(get_jarray(array));
    }
  }
  JVMCIObject get_object_at(JVMCIObjectArray array, int index) {
    if (is_hotspot()) {
      oop result = HotSpotJVMCI::resolve(array)->obj_at(index);
      return wrap(result);
    } else {
      JNIAccessMark jni(this);
      jobject result = jni()->GetObjectArrayElement(get_jobjectArray(array), index);
      return wrap(result);
    }
  }

  void put_object_at(JVMCIObjectArray array, int index, JVMCIObject value) {
    if (is_hotspot()) {
      HotSpotJVMCI::resolve(array)->obj_at_put(index, HotSpotJVMCI::resolve(value));
    } else {
      JNIAccessMark jni(this);
      jni()->SetObjectArrayElement(get_jobjectArray(array), index, get_jobject(value));
    }
  }

  jboolean get_bool_at(JVMCIPrimitiveArray array, int index) {
    if (is_hotspot()) {
      return HotSpotJVMCI::resolve(array)->bool_at(index);
    } else {
      JNIAccessMark jni(this);
      jboolean result;
      jni()->GetBooleanArrayRegion(array.as_jbooleanArray(), index, 1, &result);
      return result;
    }
  }
  void put_bool_at(JVMCIPrimitiveArray array, int index, jboolean value) {
    if (is_hotspot()) {
      HotSpotJVMCI::resolve(array)->bool_at_put(index, value);
    } else {
      JNIAccessMark jni(this);
      jni()->SetBooleanArrayRegion(array.as_jbooleanArray(), index, 1, &value);
    }
  }

  jbyte get_byte_at(JVMCIPrimitiveArray array, int index) {
    if (is_hotspot()) {
      return HotSpotJVMCI::resolve(array)->byte_at(index);
    } else {
      JNIAccessMark jni(this);
      jbyte result;
      jni()->GetByteArrayRegion(array.as_jbyteArray(), index, 1, &result);
      return result;
    }
  }
  void put_byte_at(JVMCIPrimitiveArray array, int index, jbyte value) {
    if (is_hotspot()) {
      HotSpotJVMCI::resolve(array)->byte_at_put(index, value);
    } else {
      JNIAccessMark jni(this);
      jni()->SetByteArrayRegion(array.as_jbyteArray(), index, 1, &value);
    }
  }

  int get_int_at(JVMCIPrimitiveArray array, int index) {
    if (is_hotspot()) {
      return HotSpotJVMCI::resolve(array)->int_at(index);
    } else {
      JNIAccessMark jni(this);
      int result;
      jni()->GetIntArrayRegion(array.as_jintArray(), index, 1, &result);
      return result;
    }
  }
  void put_int_at(JVMCIPrimitiveArray array, int index, int value) {
    if (is_hotspot()) {
      HotSpotJVMCI::resolve(array)->int_at_put(index, value);
    } else {
      JNIAccessMark jni(this);
      jni()->SetIntArrayRegion(array.as_jintArray(), index, 1, &value);
    }
  }

  long get_long_at(JVMCIPrimitiveArray array, int index) {
    if (is_hotspot()) {
      return HotSpotJVMCI::resolve(array)->long_at(index);
    } else {
      JNIAccessMark jni(this);
      jlong result;
      jni()->GetLongArrayRegion(array.as_jlongArray(), index, 1, &result);
      return result;
    }
  }
  void put_long_at(JVMCIPrimitiveArray array, int index, jlong value) {
    if (is_hotspot()) {
      HotSpotJVMCI::resolve(array)->long_at_put(index, value);
    } else {
      JNIAccessMark jni(this);
      jni()->SetLongArrayRegion(array.as_jlongArray(), index, 1, &value);
    }
  }

  void copy_bytes_to(JVMCIPrimitiveArray src, jbyte* dest, int offset, int size_in_bytes) {
    if (size_in_bytes == 0) {
      return;
    }
    if (is_hotspot()) {
      memcpy(dest, HotSpotJVMCI::resolve(src)->byte_at_addr(offset), size_in_bytes);
    } else {
      JNIAccessMark jni(this);
      jni()->GetByteArrayRegion(src.as_jbyteArray(), offset, size_in_bytes, dest);
    }
  }
  void copy_bytes_from(jbyte* src, JVMCIPrimitiveArray dest, int offset, int size_in_bytes) {
    if (size_in_bytes == 0) {
      return;
    }
    if (is_hotspot()) {
      memcpy(HotSpotJVMCI::resolve(dest)->byte_at_addr(offset), src, size_in_bytes);
    } else {
      JNIAccessMark jni(this);
      jni()->SetByteArrayRegion(dest.as_jbyteArray(), offset, size_in_bytes, src);
    }
  }

  JVMCIObjectArray initialize_intrinsics(JVMCI_TRAPS);

  jboolean is_boxing_object(BasicType type, JVMCIObject object) {
    if (is_hotspot()) {
      return java_lang_boxing_object::is_instance(HotSpotJVMCI::resolve(object), type);
    } else {
      JNIAccessMark jni(this);
      return jni()->IsInstanceOf(get_jobject(object), JNIJVMCI::box_class(type));
    }
  }

  jvalue get_boxed_value(BasicType type, JVMCIObject object) {
    jvalue result;
    if (is_hotspot()) {
      java_lang_boxing_object::get_value(HotSpotJVMCI::resolve(object), &result);
    } else {
      JNIAccessMark jni(this);
      jfieldID field = JNIJVMCI::box_field(type);
      switch (type) {
        case T_BOOLEAN: result.z = jni()->GetBooleanField(get_jobject(object), field); break;
        case T_BYTE:    result.b = jni()->GetByteField(get_jobject(object), field); break;
        case T_SHORT:   result.s = jni()->GetShortField(get_jobject(object), field); break;
        case T_CHAR:    result.c = jni()->GetCharField(get_jobject(object), field); break;
        case T_INT:     result.i = jni()->GetIntField(get_jobject(object), field); break;
        case T_LONG:    result.j = jni()->GetLongField(get_jobject(object), field); break;
        case T_FLOAT:   result.f = jni()->GetFloatField(get_jobject(object), field); break;
        case T_DOUBLE:  result.d = jni()->GetDoubleField(get_jobject(object), field); break;
        default:
          ShouldNotReachHere();
      }
    }
    return result;
  }

  BasicType get_box_type(JVMCIObject object) {
    if (is_hotspot()) {
      return java_lang_boxing_object::basic_type(HotSpotJVMCI::resolve(object));
    } else {
      JNIAccessMark jni(this);
      jclass clazz = jni()->GetObjectClass(get_jobject(object));
      if (jni()->IsSameObject(clazz, JNIJVMCI::box_class(T_BOOLEAN))) return T_BOOLEAN;
      if (jni()->IsSameObject(clazz, JNIJVMCI::box_class(T_BYTE))) return T_BYTE;
      if (jni()->IsSameObject(clazz, JNIJVMCI::box_class(T_SHORT))) return T_SHORT;
      if (jni()->IsSameObject(clazz, JNIJVMCI::box_class(T_CHAR))) return T_CHAR;
      if (jni()->IsSameObject(clazz, JNIJVMCI::box_class(T_INT))) return T_INT;
      if (jni()->IsSameObject(clazz, JNIJVMCI::box_class(T_LONG))) return T_LONG;
      if (jni()->IsSameObject(clazz, JNIJVMCI::box_class(T_FLOAT))) return T_FLOAT;
      if (jni()->IsSameObject(clazz, JNIJVMCI::box_class(T_DOUBLE))) return T_DOUBLE;
      ShouldNotReachHere();
      return T_CONFLICT;
    }
  }

  JVMCIObject create_box(BasicType type, jvalue* value, JVMCI_TRAPS) {
    if (is_hotspot()) {
      JavaThread* THREAD = JavaThread::current();
      oop box = java_lang_boxing_object::create(type, value, CHECK_(JVMCIObject()));
      return HotSpotJVMCI::wrap(box);
    } else {
      JNIAccessMark jni(this);
      jobject box = jni()->NewObjectA(JNIJVMCI::box_class(type), JNIJVMCI::box_constructor(type), value);
      assert(box != NULL, "");
      return wrap(box);
    }
  }

  const char* as_utf8_string(JVMCIObject str) {
    if (is_hotspot()) {
      return java_lang_String::as_utf8_string(HotSpotJVMCI::resolve(str));
    } else {
      JNIAccessMark jni(this);
      int length = jni()->GetStringLength(str.as_jstring());
      char* result = NEW_RESOURCE_ARRAY(char, length + 1);
      jni()->GetStringUTFRegion(str.as_jstring(), 0, length, result);
      return result;
    }
  }

  char* as_utf8_string(JVMCIObject str, char* buf, int buflen) {
    if (is_hotspot()) {
      return java_lang_String::as_utf8_string(HotSpotJVMCI::resolve(str), buf, buflen);
    } else {
      JNIAccessMark jni(this);
      int length = jni()->GetStringLength(str.as_jstring());
      if (length >= buflen) {
        length = buflen;
      }
      jni()->GetStringUTFRegion(str.as_jstring(), 0, length, buf);
      return buf;
    }
  }

  JVMCIObject create_string(Symbol* str, JVMCI_TRAPS) {
    return create_string(str->as_C_string(), JVMCI_CHECK_(JVMCIObject()));
  }

  JVMCIObject create_string(const char* str, JVMCI_TRAPS);

  bool equals(JVMCIObject a, JVMCIObject b);

  // Convert into a JNI handle for the appropriate runtime
  jobject get_jobject(JVMCIObject object)                       { assert(object.as_jobject() == NULL || is_hotspot() == object.is_hotspot(), "mismatch"); return object.as_jobject(); }
  jarray get_jarray(JVMCIArray array)                           { assert(array.as_jobject() == NULL || is_hotspot() == array.is_hotspot(), "mismatch"); return array.as_jobject(); }
  jobjectArray get_jobjectArray(JVMCIObjectArray objectArray)   { assert(objectArray.as_jobject() == NULL || is_hotspot() == objectArray.is_hotspot(), "mismatch"); return objectArray.as_jobject(); }
  jbyteArray get_jbyteArray(JVMCIPrimitiveArray primitiveArray) { assert(primitiveArray.as_jobject() == NULL || is_hotspot() == primitiveArray.is_hotspot(), "mismatch"); return primitiveArray.as_jbyteArray(); }

  JVMCIObject         wrap(jobject obj);
  JVMCIObjectArray    wrap(jobjectArray obj)  { return (JVMCIObjectArray)    wrap((jobject) obj); }
  JVMCIPrimitiveArray wrap(jintArray obj)     { return (JVMCIPrimitiveArray) wrap((jobject) obj); }
  JVMCIPrimitiveArray wrap(jbooleanArray obj) { return (JVMCIPrimitiveArray) wrap((jobject) obj); }
  JVMCIPrimitiveArray wrap(jbyteArray obj)    { return (JVMCIPrimitiveArray) wrap((jobject) obj); }
  JVMCIPrimitiveArray wrap(jlongArray obj)    { return (JVMCIPrimitiveArray) wrap((jobject) obj); }

 private:
  JVMCIObject wrap(oop obj)                  { assert(is_hotspot(), "must be"); return wrap(JNIHandles::make_local(obj)); }
  JVMCIObjectArray wrap(objArrayOop obj)     { assert(is_hotspot(), "must be"); return (JVMCIObjectArray) wrap(JNIHandles::make_local(obj)); }
  JVMCIPrimitiveArray wrap(typeArrayOop obj) { assert(is_hotspot(), "must be"); return (JVMCIPrimitiveArray) wrap(JNIHandles::make_local(obj)); }

 public:
  // Compiles a method with the JVMIC compiler.
  // Caller must handle pending exception.
  JVMCIObject call_HotSpotJVMCIRuntime_compileMethod(JVMCIObject runtime, JVMCIObject method, int entry_bci,
                                                     jlong compile_state, int id);

  int call_HotSpotJVMCIRuntime_adjustCompilationLevel(JVMCIObject runtime, InstanceKlass* declaringClass,
                                                      JVMCIObject name, JVMCIObject signature, bool is_osr, int level, JVMCI_TRAPS);
  void call_HotSpotJVMCIRuntime_bootstrapFinished(JVMCIObject runtime, JVMCI_TRAPS);
  void call_HotSpotJVMCIRuntime_shutdown(JVMCIObject runtime);
  JVMCIObject call_HotSpotJVMCIRuntime_runtime(JVMCI_TRAPS);
  JVMCIObject call_JVMCI_getRuntime(JVMCI_TRAPS);
  JVMCIObject call_HotSpotJVMCIRuntime_getCompiler(JVMCIObject runtime, JVMCI_TRAPS);

  JVMCIObject call_HotSpotJVMCIRuntime_callToString(JVMCIObject object, JVMCI_TRAPS);

  JVMCIObject call_PrimitiveConstant_forTypeChar(jchar kind, jlong value, JVMCI_TRAPS);
  JVMCIObject call_JavaConstant_forFloat(float value, JVMCI_TRAPS);
  JVMCIObject call_JavaConstant_forDouble(double value, JVMCI_TRAPS);

  BasicType kindToBasicType(JVMCIObject kind, JVMCI_TRAPS);

#define DO_THROW(name)                              \
  void throw_##name(const char* msg = NULL) {       \
    if (is_hotspot()) {                             \
      JavaThread* THREAD = JavaThread::current();   \
      THROW_MSG(HotSpotJVMCI::name::symbol(), msg); \
    } else {                                        \
      JNIAccessMark jni(this);                      \
      jni()->ThrowNew(JNIJVMCI::name::clazz(), msg);  \
    }                                               \
  }

  DO_THROW(InternalError)
  DO_THROW(ArrayIndexOutOfBoundsException)
  DO_THROW(IllegalStateException)
  DO_THROW(NullPointerException)
  DO_THROW(IllegalArgumentException)
  DO_THROW(InvalidInstalledCodeException)
  DO_THROW(UnsatisfiedLinkError)

#undef DO_THROW

  void fthrow_error(const char* file, int line, const char* format, ...) ATTRIBUTE_PRINTF(4, 5) {
    const int max_msg_size = 1024;
    va_list ap;
    va_start(ap, format);
    char msg[max_msg_size];
    vsnprintf(msg, max_msg_size, format, ap);
    msg[max_msg_size-1] = '\0';
    va_end(ap);
    if (is_hotspot()) {
      JavaThread* THREAD = JavaThread::current();
      Handle h_loader = Handle(THREAD, SystemDictionary::jvmci_loader());
      Handle h_protection_domain = Handle();
      Exceptions::_throw_msg(THREAD, file, line, vmSymbols::jdk_vm_ci_common_JVMCIError(), msg, h_loader, h_protection_domain);
    } else {
      JNIAccessMark jni(this);
      jni()->ThrowNew(JNIJVMCI::JVMCIError::clazz(), msg);
    }
  }

  // Given an instance of HotSpotInstalledCode return the corresponding CodeBlob*
  CodeBlob* asCodeBlob(JVMCIObject code);

  nmethod* asNmethod(JVMCIObject code) {
    CodeBlob* cb = asCodeBlob(code);
    if (cb == NULL) {
      return NULL;
    }
    nmethod* nm = cb->as_nmethod_or_null();
    guarantee(nm != NULL, "not an nmethod");
    return nm;
  }

  MethodData* asMethodData(jlong metaspaceMethodData) {
    return (MethodData*) (address) metaspaceMethodData;
  }

  const char* klass_name(JVMCIObject object);

  // Unpack an instance of HotSpotResolvedJavaMethodImpl into the original Method*
  Method* asMethod(JVMCIObject jvmci_method);
  Method* asMethod(jobject jvmci_method) { return asMethod(wrap(jvmci_method)); }

  // Unpack an instance of HotSpotResolvedObjectTypeImpl into the original Klass*
  Klass* asKlass(JVMCIObject jvmci_type);
  Klass* asKlass(jobject jvmci_type)  { return asKlass(wrap(jvmci_type)); }

  JVMCIObject get_jvmci_method(const methodHandle& method, JVMCI_TRAPS);

  JVMCIObject get_jvmci_type(JVMCIKlassHandle& klass, JVMCI_TRAPS);

  // Unpack an instance of HotSpotConstantPool into the original ConstantPool*
  ConstantPool* asConstantPool(JVMCIObject constant_pool);
  ConstantPool* asConstantPool(jobject constant_pool)  { return asConstantPool(wrap(constant_pool)); }

  JVMCIObject get_jvmci_constant_pool(constantPoolHandle cp, JVMCI_TRAPS);
  JVMCIObject get_jvmci_primitive_type(BasicType type);

  oop asConstant(JVMCIObject object, JVMCI_TRAPS);
  JVMCIObject get_object_constant(Handle obj, bool compressed = false, bool dont_register = false);

  JVMCIPrimitiveArray new_booleanArray(int length, JVMCI_TRAPS);
  JVMCIPrimitiveArray new_byteArray(int length, JVMCI_TRAPS);
  JVMCIPrimitiveArray new_intArray(int length, JVMCI_TRAPS);
  JVMCIPrimitiveArray new_longArray(int length, JVMCI_TRAPS);

  JVMCIObject new_StackTraceElement(methodHandle method, int bci, JVMCI_TRAPS);
  JVMCIObject new_HotSpotNmethod(methodHandle method, const char* name, jboolean isDefault, JVMCI_TRAPS);
  JVMCIObject new_VMField(JVMCIObject name, JVMCIObject type, jlong offset, jlong address, JVMCIObject value, JVMCI_TRAPS);
  JVMCIObject new_VMFlag(JVMCIObject name, JVMCIObject type, JVMCIObject value, JVMCI_TRAPS);
  JVMCIObject new_VMIntrinsicMethod(JVMCIObject declaringClass, JVMCIObject name, JVMCIObject descriptor, int id, JVMCI_TRAPS);
  JVMCIObject new_HotSpotStackFrameReference(JVMCI_TRAPS);
  JVMCIObject new_JVMCIError(JVMCI_TRAPS);

  jlong make_handle(Handle obj);
  oop resolve_handle(jlong objectHandle);

  // These are analagous to the JNI routines
  JVMCIObject make_local(JVMCIObject object);
  JVMCIObject make_global(JVMCIObject object);
  JVMCIObject make_weak(JVMCIObject object);
  void destroy_local(JVMCIObject object);
  void destroy_global(JVMCIObject object);
  void destroy_weak(JVMCIObject object);

  // Deoptimizes the nmethod (if any) in the address field of a given
  // HotSpotNmethod object. The address field is also zeroed.
  void invalidate_nmethod_mirror(JVMCIObject nmethod_mirror, JVMCI_TRAPS);

  void initialize_installed_code(JVMCIObject installed_code, CodeBlob* cb, JVMCI_TRAPS);

 private:
  JVMCICompileState* _compile_state;

 public:
  static JavaVM* get_shared_library_javavm() { return _shared_library_javavm; }
  static void* get_shared_library_handle()   { return _shared_library_handle; }
  static char* get_shared_library_path()     { return _shared_library_path; }

  // Determines if this is for the JVMCI runtime in the HotSpot
  // heap (true) or the shared library heap (false).
  bool is_hotspot() { return _mode == JVMCIGlobals::HotSpot; }

  JVMCIGlobals::JavaMode mode()              { return _mode; }

  JVMCICompileState* compile_state() { return _compile_state; }
  void set_compile_state(JVMCICompileState* compile_state) {
    assert(_compile_state == NULL, "set only once");
    _compile_state = compile_state;
  }
  // Generate declarations for the initialize, new, isa, get and set methods for all the types and
  // fields declared in the JVMCI_CLASSES_DO macro.

#define START_CLASS(className, fullClassName)                           \
  void className##_initialize(JVMCI_TRAPS); \
  JVMCIObjectArray new_##className##_array(int length, JVMCI_TRAPS); \
  bool isa_##className(JVMCIObject object);

#define END_CLASS

#define FIELD(className, name, type, accessor)                                                                                                                         \
  type get_ ## className ## _ ## name(JVMCIObject obj); \
  void set_ ## className ## _ ## name(JVMCIObject obj, type x);

#define OOPISH_FIELD(className, name, type, hstype, accessor) \
  FIELD(className, name, type, accessor)

#define STATIC_FIELD(className, name, type) \
  type get_ ## className ## _ ## name(); \
  void set_ ## className ## _ ## name(type x);

#define STATIC_OOPISH_FIELD(className, name, type, hstype) \
  STATIC_FIELD(className, name, type)

#define EMPTY_CAST
#define CHAR_FIELD(className,  name) FIELD(className, name, jchar, char_field)
#define INT_FIELD(className,  name) FIELD(className, name, jint, int_field)
#define BOOLEAN_FIELD(className,  name) FIELD(className, name, jboolean, bool_field)
#define LONG_FIELD(className,  name) FIELD(className, name, jlong, long_field)
#define FLOAT_FIELD(className,  name) FIELD(className, name, jfloat, float_field)
#define OBJECT_FIELD(className,  name, signature) OOPISH_FIELD(className, name, JVMCIObject, oop, obj_field)
#define OBJECTARRAY_FIELD(className,  name, signature) OOPISH_FIELD(className, name, JVMCIObjectArray, objArrayOop, obj_field)
#define PRIMARRAY_FIELD(className,  name, signature) OOPISH_FIELD(className, name, JVMCIPrimitiveArray, typeArrayOop, obj_field)

#define STATIC_INT_FIELD(className, name) STATIC_FIELD(className, name, jint)
#define STATIC_BOOLEAN_FIELD(className, name) STATIC_FIELD(className, name, jboolean)
#define STATIC_OBJECT_FIELD(className, name, signature) STATIC_OOPISH_FIELD(className, name, JVMCIObject, oop)
#define STATIC_OBJECTARRAY_FIELD(className, name, signature) STATIC_OOPISH_FIELD(className, name, JVMCIObjectArray, objArrayOop)
#define METHOD(jniCallType, jniGetMethod, hsCallType, returnType, className, methodName, signatureSymbolName, args)
#define CONSTRUCTOR(className, signature)

  JVMCI_CLASSES_DO(START_CLASS, END_CLASS, CHAR_FIELD, INT_FIELD, BOOLEAN_FIELD, LONG_FIELD, FLOAT_FIELD, OBJECT_FIELD, PRIMARRAY_FIELD, OBJECTARRAY_FIELD, STATIC_OBJECT_FIELD, STATIC_OBJECTARRAY_FIELD, STATIC_INT_FIELD, STATIC_BOOLEAN_FIELD, METHOD, CONSTRUCTOR)

#undef JNI_START_CLASS
#undef START_CLASS
#undef END_CLASS
#undef METHOD
#undef CONSTRUCTOR
#undef FIELD
#undef CHAR_FIELD
#undef INT_FIELD
#undef BOOLEAN_FIELD
#undef LONG_FIELD
#undef FLOAT_FIELD
#undef OBJECT_FIELD
#undef PRIMARRAY_FIELD
#undef OBJECTARRAY_FIELD
#undef FIELD
#undef OOPISH_FIELD
#undef STATIC_FIELD
#undef STATIC_OOPISH_FIELD
#undef STATIC_FIELD
#undef STATIC_OBJECT_FIELD
#undef STATIC_OBJECTARRAY_FIELD
#undef STATIC_INT_FIELD
#undef STATIC_BOOLEAN_FIELD
#undef EMPTY_CAST

  // End of JVMCIEnv
};


inline JNIAccessMark::JNIAccessMark(JVMCIEnv* jvmci_env): ThreadToNativeFromVM(JavaThread::current()), HandleMark(JavaThread::current()) {
  _env = jvmci_env->_env;
}


// A class that maintains the state needed for compilations requested
// by the CompileBroker.  It is created in the broker and passed through
// into the code installation step.
class JVMCICompileState : public ResourceObj {
  friend class VMStructs;
 private:
  CompileTask*     _task;
  int              _system_dictionary_modification_counter;

  // Cache JVMTI state. Defined as bytes so that reading them from Java
  // via Unsafe is well defined (the C++ type for bool is implementation
  // defined and may not be the same as a Java boolean).
  jbyte  _jvmti_can_hotswap_or_post_breakpoint;
  jbyte  _jvmti_can_access_local_variables;
  jbyte  _jvmti_can_post_on_exceptions;
  jbyte  _jvmti_can_pop_frame;

  // Compilation result values.
  bool             _retryable;
  const char*      _failure_reason;

  // Specifies if _failure_reason is on the C heap. If so, it is allocated
  // with the mtCompiler NMT flag.
  bool             _failure_reason_on_C_heap;

 public:
  JVMCICompileState(CompileTask* task, int system_dictionary_modification_counter);

  CompileTask* task() { return _task; }

  int system_dictionary_modification_counter() { return _system_dictionary_modification_counter; }
  bool  jvmti_state_changed() const;
  bool  jvmti_can_hotswap_or_post_breakpoint() const { return  _jvmti_can_hotswap_or_post_breakpoint != 0; }
  bool  jvmti_can_access_local_variables() const     { return  _jvmti_can_access_local_variables != 0; }
  bool  jvmti_can_post_on_exceptions() const         { return  _jvmti_can_post_on_exceptions != 0; }
  bool  jvmti_can_pop_frame() const                  { return  _jvmti_can_pop_frame != 0; }

  const char* failure_reason() { return _failure_reason; }
  bool failure_reason_on_C_heap() { return _failure_reason_on_C_heap; }
  bool retryable() { return _retryable; }

  void set_failure(bool retryable, const char* reason, bool reason_on_C_heap = false) {
    _failure_reason = reason;
    _failure_reason_on_C_heap = reason_on_C_heap;
    _retryable = retryable;
  }
};

#endif // SHARE_VM_JVMCI_JVMCIENV_HPP
