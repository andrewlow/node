// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On z/OS, this test needs a stack size of at least 204 kBytes.
// Flags: --stack-size=220

function f() {
  try {
    f();
  } catch(e) {
    try {
      Realm.create();
    } catch (e) {
      quit();
    }
  }
}
f();
