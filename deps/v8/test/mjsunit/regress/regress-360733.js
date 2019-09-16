// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On z/OS, this test needs a stack size of at least 204 kBytes.
// Flags: --stack-size=220

function f(a) {
  f(a + 1);
}

Error.__defineGetter__('stackTraceLimit', function() { });
try {
  f(0);
} catch (e) { }
