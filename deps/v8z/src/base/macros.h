// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_MACROS_H_
#define V8_BASE_MACROS_H_

#include "include/v8stdint.h"
#include "src/base/build_config.h"
#include "src/base/logging.h"


// The expression OFFSET_OF(type, field) computes the byte-offset
// of the specified field relative to the containing type. This
// corresponds to 'offsetof' (in stddef.h), except that it doesn't
// use 0 or NULL, which causes a problem with the compiler warnings
// we have enabled (which is also why 'offsetof' doesn't seem to work).
// Here we simply use the non-zero value 4, which seems to work.
#define OFFSET_OF(type, field)                                          \
  (reinterpret_cast<intptr_t>(&(reinterpret_cast<type*>(4)->field)) - 4)


// The expression ARRAY_SIZE(a) is a compile-time constant of type
// size_t which represents the number of elements of the given
// array. You should only use ARRAY_SIZE on statically allocated
// arrays.
#define ARRAY_SIZE(a)                                   \
  ((sizeof(a) / sizeof(*(a))) /                         \
  static_cast<size_t>(!(sizeof(a) % sizeof(*(a)))))


// A macro to disallow the evil copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define DISALLOW_COPY_AND_ASSIGN(TypeName)  \
  TypeName(const TypeName&) V8_DELETE;      \
  void operator=(const TypeName&) V8_DELETE


// A macro to disallow all the implicit constructors, namely the
// default constructor, copy constructor and operator= functions.
//
// This should be used in the private: declarations for a class
// that wants to prevent anyone from instantiating it. This is
// especially useful for classes containing only static methods.
#define DISALLOW_IMPLICIT_CONSTRUCTORS(TypeName)  \
  TypeName() V8_DELETE;                           \
  DISALLOW_COPY_AND_ASSIGN(TypeName)


// Newly written code should use V8_INLINE and V8_NOINLINE directly.
#define INLINE(declarator)    V8_INLINE declarator
#define NO_INLINE(declarator) V8_NOINLINE declarator


// Newly written code should use V8_WARN_UNUSED_RESULT.
#define MUST_USE_RESULT V8_WARN_UNUSED_RESULT


// Define V8_USE_ADDRESS_SANITIZER macros.
#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define V8_USE_ADDRESS_SANITIZER 1
#endif
#endif

// Define DISABLE_ASAN macros.
#ifdef V8_USE_ADDRESS_SANITIZER
#define DISABLE_ASAN __attribute__((no_sanitize_address))
#else
#define DISABLE_ASAN
#endif


#if V8_CC_GNU
#define V8_IMMEDIATE_CRASH() __builtin_trap()
#else
#define V8_IMMEDIATE_CRASH() ((void(*)())0)()
#endif


// Use C++11 static_assert if possible, which gives error
// messages that are easier to understand on first sight.
#if V8_HAS_CXX11_STATIC_ASSERT
#define STATIC_ASSERT(test) static_assert(test, #test)
#else
// This is inspired by the static assertion facility in boost.  This
// is pretty magical.  If it causes you trouble on a platform you may
// find a fix in the boost code.
template <bool> class StaticAssertion;
template <> class StaticAssertion<true> { };
// This macro joins two tokens.  If one of the tokens is a macro the
// helper call causes it to be resolved before joining.
#define SEMI_STATIC_JOIN(a, b) SEMI_STATIC_JOIN_HELPER(a, b)
#define SEMI_STATIC_JOIN_HELPER(a, b) a##b
// Causes an error during compilation of the condition is not
// statically known to be true.  It is formulated as a typedef so that
// it can be used wherever a typedef can be used.  Beware that this
// actually causes each use to introduce a new defined type with a
// name depending on the source line.
template <int> class StaticAssertionHelper { };
#define STATIC_ASSERT(test)                                                    \
  typedef                                                                     \
    StaticAssertionHelper<sizeof(StaticAssertion<static_cast<bool>((test))>)> \
    SEMI_STATIC_JOIN(__StaticAssertTypedef__, __LINE__) V8_UNUSED

#endif


// The USE(x) template is used to silence C++ compiler warnings
// issued for (yet) unused variables (typically parameters).
template <typename T>
inline void USE(T) { }


#define IS_POWER_OF_TWO(x) ((x) != 0 && (((x) & ((x) - 1)) == 0))


// Returns true iff x is a power of 2. Cannot be used with the maximally
// negative value of the type T (the -1 overflows).
template <typename T>
inline bool IsPowerOf2(T x) {
  return IS_POWER_OF_TWO(x);
}


// Define our own macros for writing 64-bit constants.  This is less fragile
// than defining __STDC_CONSTANT_MACROS before including <stdint.h>, and it
// works on compilers that don't have it (like MSVC).
#if V8_CC_MSVC
# define V8_UINT64_C(x)   (x ## UI64)
# define V8_INT64_C(x)    (x ## I64)
# if V8_HOST_ARCH_64_BIT
#  define V8_INTPTR_C(x)  (x ## I64)
#  define V8_PTR_PREFIX   "ll"
# else
#  define V8_INTPTR_C(x)  (x)
#  define V8_PTR_PREFIX   ""
# endif  // V8_HOST_ARCH_64_BIT
#elif V8_CC_MINGW64
# define V8_UINT64_C(x)   (x ## ULL)
# define V8_INT64_C(x)    (x ## LL)
# define V8_INTPTR_C(x)   (x ## LL)
# define V8_PTR_PREFIX    "I64"
#elif V8_HOST_ARCH_64_BIT
# if V8_OS_MACOSX
#  define V8_UINT64_C(x)   (x ## ULL)
#  define V8_INT64_C(x)    (x ## LL)
# else
#  define V8_UINT64_C(x)   (x ## UL)
#  define V8_INT64_C(x)    (x ## L)
# endif
# define V8_INTPTR_C(x)   (x ## L)
# define V8_PTR_PREFIX    "l"
#else
# define V8_UINT64_C(x)   (x ## ULL)
# define V8_INT64_C(x)    (x ## LL)
# define V8_INTPTR_C(x)   (x)
# if V8_OS_AIX
# define V8_PTR_PREFIX    "l"
# else
# define V8_PTR_PREFIX    ""
#endif
#endif

#define V8PRIxPTR V8_PTR_PREFIX "x"
#define V8PRIdPTR V8_PTR_PREFIX "d"
#define V8PRIuPTR V8_PTR_PREFIX "u"

// Fix for Mac OS X defining uintptr_t as "unsigned long":
#if V8_OS_MACOSX
#undef V8PRIxPTR
#define V8PRIxPTR "lx"
#endif

// The following macro works on both 32 and 64-bit platforms.
// Usage: instead of writing 0x1234567890123456
//      write V8_2PART_UINT64_C(0x12345678,90123456);
#define V8_2PART_UINT64_C(a, b) (((static_cast<uint64_t>(a) << 32) + 0x##b##u))


// Compute the 0-relative offset of some absolute value x of type T.
// This allows conversion of Addresses and integral types into
// 0-relative int offsets.
template <typename T>
inline intptr_t OffsetFrom(T x) {
  return x - static_cast<T>(0);
}


// Compute the absolute value of type T for some 0-relative offset x.
// This allows conversion of 0-relative int offsets into Addresses and
// integral types.
template <typename T>
inline T AddressFrom(intptr_t x) {
  return static_cast<T>(static_cast<T>(0) + x);
}


// Return the largest multiple of m which is <= x.
template <typename T>
inline T RoundDown(T x, intptr_t m) {
  DCHECK(IsPowerOf2(m));
  return AddressFrom<T>(OffsetFrom(x) & -m);
}


// Return the smallest multiple of m which is >= x.
template <typename T>
inline T RoundUp(T x, intptr_t m) {
  return RoundDown<T>(static_cast<T>(x + m - 1), m);
}


// Increment a pointer until it has the specified alignment.
// This works like RoundUp, but it works correctly on pointer types where
// sizeof(*pointer) might not be 1.
template<class T>
T AlignUp(T pointer, size_t alignment) {
  DCHECK(sizeof(pointer) == sizeof(uintptr_t));
  uintptr_t pointer_raw = reinterpret_cast<uintptr_t>(pointer);
  return reinterpret_cast<T>(RoundUp(pointer_raw, alignment));
}


template <typename T, typename U>
inline bool IsAligned(T value, U alignment) {
  return (value & (alignment - 1)) == 0;
}


// Returns the smallest power of two which is >= x. If you pass in a
// number that is already a power of two, it is returned as is.
// Implementation is from "Hacker's Delight" by Henry S. Warren, Jr.,
// figure 3-3, page 48, where the function is called clp2.
inline uint32_t RoundUpToPowerOf2(uint32_t x) {
  DCHECK(x <= 0x80000000u);
  x = x - 1;
  x = x | (x >> 1);
  x = x | (x >> 2);
  x = x | (x >> 4);
  x = x | (x >> 8);
  x = x | (x >> 16);
  return x + 1;
}


inline uint32_t RoundDownToPowerOf2(uint32_t x) {
  uint32_t rounded_up = RoundUpToPowerOf2(x);
  if (rounded_up > x) return rounded_up >> 1;
  return rounded_up;
}


// Returns current value of top of the stack. Works correctly with ASAN.
DISABLE_ASAN
inline uintptr_t GetCurrentStackPosition() {
  // Takes the address of the limit variable in order to find out where
  // the top of stack is right now.
  uintptr_t limit = reinterpret_cast<uintptr_t>(&limit);
  return limit;
}

#ifdef V8_OS_ZOS
inline const uint8_t& Ascii2Ebcdic(const char letter) {
  static unsigned char a2e[256] = {
  0,1,2,3,55,45,46,47,22,5,37,11,12,13,14,15,
  16,17,18,19,60,61,50,38,24,25,63,39,28,29,30,31,
  64,79,127,123,91,108,80,125,77,93,92,78,107,96,75,97,
  240,241,242,243,244,245,246,247,248,249,122,94,76,126,110,111,
  124,193,194,195,196,197,198,199,200,201,209,210,211,212,213,214,
  215,216,217,226,227,228,229,230,231,232,233,74,224,90,95,109,
  121,129,130,131,132,133,134,135,136,137,145,146,147,148,149,150,
  151,152,153,162,163,164,165,166,167,168,169,192,106,208,161,7,
  32,33,34,35,36,21,6,23,40,41,42,43,44,9,10,27,
  48,49,26,51,52,53,54,8,56,57,58,59,4,20,62,225,
  65,66,67,68,69,70,71,72,73,81,82,83,84,85,86,87,
  88,89,98,99,100,101,102,103,104,105,112,113,114,115,116,117,
  118,119,120,128,138,139,140,141,142,143,144,154,155,156,157,158,
  159,160,170,171,172,173,174,175,176,177,178,179,180,181,182,183,
  184,185,186,187,188,189,190,191,202,203,204,205,206,207,218,219,
  220,221,222,223,234,235,236,237,238,239,250,251,252,253,254,255
  };
  return a2e[letter];
}

inline const uint8_t& Ebcdic2Ascii(const char letter) {
  static const uint8_t e2a[256] = {
  0,1,2,3,156,9,134,127,151,141,142,11,12,13,14,15,
  16,17,18,19,157,133,8,135,24,25,146,143,28,29,30,31,
  128,129,130,131,132,10,23,27,136,137,138,139,140,5,6,7,
  144,145,22,147,148,149,150,4,152,153,154,155,20,21,158,26,
  32,160,161,162,163,164,165,166,167,168,91,46,60,40,43,33,
  38,169,170,171,172,173,174,175,176,177,93,36,42,41,59,94,
  45,47,178,179,180,181,182,183,184,185,124,44,37,95,62,63,
  186,187,188,189,190,191,192,193,194,96,58,35,64,39,61,34,
  195,97,98,99,100,101,102,103,104,105,196,197,198,199,200,201,
  202,106,107,108,109,110,111,112,113,114,203,204,205,206,207,208,
  209,126,115,116,117,118,119,120,121,122,210,211,212,213,214,215,
  216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,
  123,65,66,67,68,69,70,71,72,73,232,233,234,235,236,237,
  125,74,75,76,77,78,79,80,81,82,238,239,240,241,242,243,
  92,159,83,84,85,86,87,88,89,90,244,245,246,247,248,249,
  48,49,50,51,52,53,54,55,56,57,250,251,252,253,254,255
  };
  return e2a[letter];
}

#define GET_ASCII_CODE(byte_char) Ebcdic2Ascii(byte_char)
#else
#define GET_ASCII_CODE(byte_char) byte_char
#endif

#endif   // V8_BASE_MACROS_H_
