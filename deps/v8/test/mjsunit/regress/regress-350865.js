// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On z/OS, this test needs a stack size of at least 204 kBytes.
// Flags: --stress-compaction --stack-size=220

/\2/.test("1");

function rec() {
  try {
    rec();
  } catch(e) {
    /\2/.test("1");
  }
}

rec();
