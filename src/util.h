#ifndef SRC_UTIL_H_
#define SRC_UTIL_H_

#if defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#include "v8.h"

#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// OSX 10.9 defaults to libc++ which provides a C++11 <type_traits> header.
#if defined(__APPLE__) && __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ < 1090
#define USE_TR1_TYPE_TRAITS
#endif

#ifdef USE_TR1_TYPE_TRAITS
#include <tr1/type_traits>  // NOLINT(build/c++tr1)
#else
#include <type_traits>  // std::remove_reference
#endif

namespace node {

// These should be used in our code as opposed to the native
// versions as they abstract out some platform and or
// compiler version specific functionality
// malloc(0) and realloc(ptr, 0) have implementation-defined behavior in
// that the standard allows them to either return a unique pointer or a
// nullptr for zero-sized allocation requests.  Normalize by always using
// a nullptr.
template <typename T>
inline T* UncheckedRealloc(T* pointer, size_t n);
template <typename T>
inline T* UncheckedMalloc(size_t n);
template <typename T>
inline T* UncheckedCalloc(size_t n);

// Same things, but aborts immediately instead of returning nullptr when
// no memory is available.
template <typename T>
inline T* Realloc(T* pointer, size_t n);
template <typename T>
inline T* Malloc(size_t n);
template <typename T>
inline T* Calloc(size_t n);

// Shortcuts for char*.
inline char* Malloc(size_t n) { return Malloc<char>(n); }
inline char* Calloc(size_t n) { return Calloc<char>(n); }
inline char* UncheckedMalloc(size_t n) { return UncheckedMalloc<char>(n); }
inline char* UncheckedCalloc(size_t n) { return UncheckedCalloc<char>(n); }

// Used by the allocation functions when allocation fails.
// Thin wrapper around v8::Isolate::LowMemoryNotification() that checks
// whether V8 is initialized.
void LowMemoryNotification();

#ifdef __GNUC__
#define NO_RETURN __attribute__((noreturn))
#else
#define NO_RETURN
#endif

// The slightly odd function signature for Assert() is to ease
// instruction cache pressure in calls from ASSERT and CHECK.
NO_RETURN void Abort();
NO_RETURN void Assert(const char* const (*args)[4]);
void DumpBacktrace(FILE* fp);

#ifdef USE_TR1_TYPE_TRAITS
template <typename T> using remove_reference = std::tr1::remove_reference<T>;
#else
template <typename T> using remove_reference = std::remove_reference<T>;
#endif

#define FIXED_ONE_BYTE_STRING(isolate, string)                                \
  (node::OneByteString((isolate), (string), sizeof(string) - 1))

#define DISALLOW_COPY_AND_ASSIGN(TypeName)                                    \
  void operator=(const TypeName&) = delete;                                   \
  void operator=(TypeName&&) = delete;                                        \
  TypeName(const TypeName&) = delete;                                         \
  TypeName(TypeName&&) = delete

// Windows 8+ does not like abort() in Release mode
#ifdef _WIN32
#define ABORT_NO_BACKTRACE() raise(SIGABRT)
#else
#define ABORT_NO_BACKTRACE() abort()
#endif

#define ABORT() node::Abort()

#ifdef __GNUC__
#define LIKELY(expr) __builtin_expect(!!(expr), 1)
#define UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#define PRETTY_FUNCTION_NAME __PRETTY_FUNCTION__
#else
#define LIKELY(expr) expr
#define UNLIKELY(expr) expr
#define PRETTY_FUNCTION_NAME ""
#endif

#define STRINGIFY_(x) #x
#define STRINGIFY(x) STRINGIFY_(x)

#define CHECK(expr)                                                           \
  do {                                                                        \
    if (UNLIKELY(!(expr))) {                                                  \
      static const char* const args[] = { __FILE__, STRINGIFY(__LINE__),      \
                                          #expr, PRETTY_FUNCTION_NAME };      \
      node::Assert(&args);                                                    \
    }                                                                         \
  } while (0)

#ifdef NDEBUG
#define ASSERT(expr)
#else
#define ASSERT(expr) CHECK(expr)
#endif

#define ASSERT_EQ(a, b) ASSERT((a) == (b))
#define ASSERT_GE(a, b) ASSERT((a) >= (b))
#define ASSERT_GT(a, b) ASSERT((a) > (b))
#define ASSERT_LE(a, b) ASSERT((a) <= (b))
#define ASSERT_LT(a, b) ASSERT((a) < (b))
#define ASSERT_NE(a, b) ASSERT((a) != (b))

#define CHECK_EQ(a, b) CHECK((a) == (b))
#define CHECK_GE(a, b) CHECK((a) >= (b))
#define CHECK_GT(a, b) CHECK((a) > (b))
#define CHECK_LE(a, b) CHECK((a) <= (b))
#define CHECK_LT(a, b) CHECK((a) < (b))
#define CHECK_NE(a, b) CHECK((a) != (b))

#define UNREACHABLE() ABORT()

#define ASSIGN_OR_RETURN_UNWRAP(ptr, obj, ...)                                \
  do {                                                                        \
    *ptr =                                                                    \
        Unwrap<typename node::remove_reference<decltype(**ptr)>::type>(obj);  \
    if (*ptr == nullptr)                                                      \
      return __VA_ARGS__;                                                     \
  } while (0)

// TAILQ-style intrusive list node.
template <typename T>
class ListNode;

// TAILQ-style intrusive list head.
template <typename T, ListNode<T> (T::*M)>
class ListHead;

template <typename T>
class ListNode {
 public:
  inline ListNode();
  inline ~ListNode();
  inline void Remove();
  inline bool IsEmpty() const;

 private:
  template <typename U, ListNode<U> (U::*M)> friend class ListHead;
  ListNode* prev_;
  ListNode* next_;
  DISALLOW_COPY_AND_ASSIGN(ListNode);
};

template <typename T, ListNode<T> (T::*M)>
class ListHead {
 public:
  class Iterator {
   public:
    inline T* operator*() const;
    inline const Iterator& operator++();
    inline bool operator!=(const Iterator& that) const;

   private:
    friend class ListHead;
    inline explicit Iterator(ListNode<T>* node);
    ListNode<T>* node_;
  };

  inline ListHead() = default;
  inline ~ListHead();
  inline void MoveBack(ListHead* that);
  inline void PushBack(T* element);
  inline void PushFront(T* element);
  inline bool IsEmpty() const;
  inline T* PopFront();
  inline Iterator begin() const;
  inline Iterator end() const;

 private:
  ListNode<T> head_;
  DISALLOW_COPY_AND_ASSIGN(ListHead);
};

// The helper is for doing safe downcasts from base types to derived types.
template <typename Inner, typename Outer>
class ContainerOfHelper {
 public:
  inline ContainerOfHelper(Inner Outer::*field, Inner* pointer);
  template <typename TypeName>
  inline operator TypeName*() const;
 private:
  Outer* const pointer_;
};

// Calculate the address of the outer (i.e. embedding) struct from
// the interior pointer to a data member.
template <typename Inner, typename Outer>
inline ContainerOfHelper<Inner, Outer> ContainerOf(Inner Outer::*field,
                                                   Inner* pointer);

// If persistent.IsWeak() == false, then do not call persistent.Reset()
// while the returned Local<T> is still in scope, it will destroy the
// reference to the object.
template <class TypeName>
inline v8::Local<TypeName> PersistentToLocal(
    v8::Isolate* isolate,
    const v8::Persistent<TypeName>& persistent);

// Unchecked conversion from a non-weak Persistent<T> to Local<TLocal<T>,
// use with care!
//
// Do not call persistent.Reset() while the returned Local<T> is still in
// scope, it will destroy the reference to the object.
template <class TypeName>
inline v8::Local<TypeName> StrongPersistentToLocal(
    const v8::Persistent<TypeName>& persistent);

template <class TypeName>
inline v8::Local<TypeName> WeakPersistentToLocal(
    v8::Isolate* isolate,
    const v8::Persistent<TypeName>& persistent);

// Convenience wrapper around v8::String::NewFromOneByte().
inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const char* data,
                                           int length = -1);

// For the people that compile with -funsigned-char.
inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const signed char* data,
                                           int length = -1);

inline v8::Local<v8::String> OneByteString(v8::Isolate* isolate,
                                           const unsigned char* data,
                                           int length = -1);

inline void Wrap(v8::Local<v8::Object> object, void* pointer);

inline void ClearWrap(v8::Local<v8::Object> object);

template <typename TypeName>
inline TypeName* Unwrap(v8::Local<v8::Object> object);

// Swaps bytes in place. nbytes is the number of bytes to swap and must be a
// multiple of the word size (checked by function).
inline void SwapBytes16(char* data, size_t nbytes);
inline void SwapBytes32(char* data, size_t nbytes);
inline void SwapBytes64(char* data, size_t nbytes);

// tolower() is locale-sensitive.  Use ToLower() instead.
inline char ToLower(char c);

// strcasecmp() is locale-sensitive.  Use StringEqualNoCase() instead.
inline bool StringEqualNoCase(const char* a, const char* b);

// strncasecmp() is locale-sensitive.  Use StringEqualNoCaseN() instead.
inline bool StringEqualNoCaseN(const char* a, const char* b, size_t length);

// Allocates an array of member type T. For up to kStackStorageSize items,
// the stack is used, otherwise malloc().
template <typename T, size_t kStackStorageSize = 1024>
class MaybeStackBuffer {
 public:
  const T* out() const {
    return buf_;
  }

  T* out() {
    return buf_;
  }

  // operator* for compatibility with `v8::String::(Utf8)Value`
  T* operator*() {
    return buf_;
  }

  const T* operator*() const {
    return buf_;
  }

  T& operator[](size_t index) {
    CHECK_LT(index, length());
    return buf_[index];
  }

  const T& operator[](size_t index) const {
    CHECK_LT(index, length());
    return buf_[index];
  }

  size_t length() const {
    return length_;
  }

  // Current maximum capacity of the buffer with which SetLength() can be used
  // without first calling AllocateSufficientStorage().
  size_t capacity() const {
    return IsAllocated() ? capacity_ :
                           IsInvalidated() ? 0 : kStackStorageSize;
  }

  // Make sure enough space for `storage` entries is available.
  // This method can be called multiple times throughout the lifetime of the
  // buffer, but once this has been called Invalidate() cannot be used.
  // Content of the buffer in the range [0, length()) is preserved.
  void AllocateSufficientStorage(size_t storage) {
    CHECK(!IsInvalidated());
    if (storage > capacity()) {
      bool was_allocated = IsAllocated();
      T* allocated_ptr = was_allocated ? buf_ : nullptr;
      buf_ = Realloc(allocated_ptr, storage);
      capacity_ = storage;
      if (!was_allocated && length_ > 0)
        memcpy(buf_, buf_st_, length_ * sizeof(buf_[0]));
    }

    length_ = storage;
  }

  void SetLength(size_t length) {
    // capacity() returns how much memory is actually available.
    CHECK_LE(length, capacity());
    length_ = length;
  }

  void SetLengthAndZeroTerminate(size_t length) {
    // capacity() returns how much memory is actually available.
    CHECK_LE(length + 1, capacity());
    SetLength(length);

    // T() is 0 for integer types, nullptr for pointers, etc.
    buf_[length] = T();
  }

  // Make derefencing this object return nullptr.
  // This method can be called multiple times throughout the lifetime of the
  // buffer, but once this has been called AllocateSufficientStorage() cannot
  // be used.
  void Invalidate() {
    CHECK(!IsAllocated());
    length_ = 0;
    buf_ = nullptr;
  }

  // If the buffer is stored in the heap rather than on the stack.
  bool IsAllocated() const {
    return !IsInvalidated() && buf_ != buf_st_;
  }

  // If Invalidate() has been called.
  bool IsInvalidated() const {
    return buf_ == nullptr;
  }

  // Release ownership of the malloc'd buffer.
  // Note: This does not free the buffer.
  void Release() {
    CHECK(IsAllocated());
    buf_ = buf_st_;
    length_ = 0;
    capacity_ = 0;
  }

  MaybeStackBuffer() : length_(0), capacity_(0), buf_(buf_st_) {
    // Default to a zero-length, null-terminated buffer.
    buf_[0] = T();
  }

  explicit MaybeStackBuffer(size_t storage) : MaybeStackBuffer() {
    AllocateSufficientStorage(storage);
  }

  ~MaybeStackBuffer() {
    if (IsAllocated())
      free(buf_);
  }

 private:
  size_t length_;
  // capacity of the malloc'ed buf_
  size_t capacity_;
  T* buf_;
  T buf_st_[kStackStorageSize];
};

class Utf8Value : public MaybeStackBuffer<char> {
 public:
  explicit Utf8Value(v8::Isolate* isolate, v8::Local<v8::Value> value);
};

class TwoByteValue : public MaybeStackBuffer<uint16_t> {
 public:
  explicit TwoByteValue(v8::Isolate* isolate, v8::Local<v8::Value> value);
};

class BufferValue : public MaybeStackBuffer<char> {
 public:
  explicit BufferValue(v8::Isolate* isolate, v8::Local<v8::Value> value);
};

class NativeEncodingValue : public MaybeStackBuffer<char> {
 public:
  explicit NativeEncodingValue(v8::Isolate* isolate, v8::Local<v8::Value> value);
};


class E2A {
  public:
    explicit E2A(const char* val);
    explicit E2A(const char* val, unsigned len);
    explicit E2A(const char* prefix, const char* val);

    ~E2A() {
        free(str_);
    }

    char* operator*() {
      return str_;
    };

    const char* operator*() const {
      return str_;
    };

    size_t length() const {
      return length_;
    };

  private:
    size_t length_;
    char* str_;
};

}  // namespace node

#endif  // defined(NODE_WANT_INTERNALS) && NODE_WANT_INTERNALS

#endif  // SRC_UTIL_H_
