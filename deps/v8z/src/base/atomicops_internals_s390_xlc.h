// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is an internal atomic implementation, use atomicops.h instead.
//

#ifndef V8_ATOMICOPS_INTERNALS_S390_XLC_H_
#define V8_ATOMICOPS_INTERNALS_S390_XLC_H_

#include <stdlib.h>

namespace v8 {
namespace base {

inline bool __sync_bool_compare_and_swap(volatile Atomic32 *ptr,
                                         Atomic32 oldval,
                                         Atomic32 newval) {
  return !__cs1(reinterpret_cast<void*>(&oldval),
                const_cast<void*>(reinterpret_cast<volatile void*>(ptr)),
                reinterpret_cast<void*>(&newval));
}

inline Atomic32 __sync_val_compare_and_swap(volatile Atomic32 *ptr,
                                            Atomic32 oldval,
                                            Atomic32 newval) {
  __cs1(reinterpret_cast<void*>(&oldval),
        const_cast<void*>(reinterpret_cast<volatile void*>(ptr)),
        reinterpret_cast<void*>(&newval));
  return oldval;
}

inline Atomic32 __sync_add_and_fetch(volatile Atomic32 *ptr, Atomic32 value) {
  Atomic32 tmp, old;
  do {
    old = *ptr;
    tmp = old + value;
  } while (__cs1(reinterpret_cast<void*>(&old),
                const_cast<void*>(reinterpret_cast<volatile void*>(ptr)),
                 reinterpret_cast<void*>(&tmp)));
  return tmp;
}

#ifdef V8_TARGET_ARCH_S390X
inline bool __sync_bool_compare_and_swap(volatile Atomic64 *ptr,
                                         Atomic64 oldval,
                                         Atomic64 newval) {
  return !__csg(reinterpret_cast<void*>(&oldval),
                const_cast<void*>(reinterpret_cast<volatile void*>(ptr)),
                reinterpret_cast<void*>(&newval));
}

inline Atomic64 __sync_val_compare_and_swap(volatile Atomic64 *ptr,
                                            Atomic64 oldval,
                                            Atomic64 newval) {
  __csg(reinterpret_cast<void*>(&oldval),
        const_cast<void*>(reinterpret_cast<volatile void*>(ptr)),
        reinterpret_cast<void*>(&newval));
  return oldval;
}

inline bool __sync_add_and_fetch(volatile Atomic64 *ptr, Atomic64 value) {
  Atomic64 tmp, old;
  do {
    old = *ptr;
    tmp = old + value;
  } while (__csg(reinterpret_cast<void*>(&old),
                 const_cast<void*>(reinterpret_cast<volatile void*>(ptr)),
                 reinterpret_cast<void*>(&tmp)));
  return tmp;
}
#endif

inline bool __sync_synchronize() {
  // Use this pragma around the asm string to counteract -qascii option to xlc.
  #pragma convert("ibm-1047")
  // Emit a "bcr 14,0" instruction to acheive serialization of the CPU.
  __asm__ __volatile__(" bcr 14,0");
  #pragma convert(pop)
  return true;
}

} }  // namespace v8::base

#endif  // V8_ATOMICOPS_INTERNALS_S390_XLC_H_
