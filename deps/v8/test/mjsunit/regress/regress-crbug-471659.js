// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On z/OS, this test needs a stack size of at least 204 kBytes.
// Flags: --stack-size=220

var s = "0123456789ABCDEF";
for (var i = 0; i < 16; i++) s += s;

var count = 0;
function f() {
  try {
    f();
    if (count < 10) {
      f();
    }
  } catch(e) {
      s.replace("+", "-");
  }
  count++;
}
f();
