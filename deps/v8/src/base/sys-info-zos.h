// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Contains z/OS Control Block Structures for retreiving system information.

#ifndef V8_BASE_SYS_INFO_ZOS_H_
#define V8_BASE_SYS_INFO_ZOS_H_

namespace v8 {
namespace base {

// CPU Management Control Table (CCT).
typedef struct ZOSCCT {
  uint8_t filler [110];
  uint16_t cpuCount;  // Number of online CPUs.
} ZOSCCT_t;


// System Resources Manager Control Table (RMCT).
typedef struct ZOSRMCT {
  uint8_t name[4];
  ZOSCCT* __ptr32 cct;
} ZOSRMCT_t;


// RSM Control and Enumeration Area (RCE).
typedef struct ZOSRCE {
  uint8_t id[4];
  uint32_t pool;  // Number of frames currently available to system.
} ZOSRCE_t;


// Communications Vector Table (CVT).
typedef struct ZOSCVT {
  uint8_t filler[604];
  ZOSRMCT* __ptr32 rmct;
  uint8_t filler1[560];
  ZOSRCE* __ptr32 rce;
  uint8_t filler2[92];
  uint8_t cvtoslvl[16];
} ZOSCVT_t;


// Prefixed Save Area (PSA).
// Maps storage starting at location 0.
typedef struct ZOSPSA {
  uint8_t filler[16];  // Ignore 16 bytes before CVT pointer.
  ZOSCVT* __ptr32 cvt;
} ZOSPSA_t;

} }  // namespace v8::base

#endif  // V8_BASE_SYS_INFO_ZOS_H_
