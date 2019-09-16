// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On z/OS, this test needs a stack size of at least 204 kBytes.
// Flags: --stack-size=220

function Module(stdlib) {
  "use asm";

  var fround = stdlib.Math.fround;

  // f: double -> float
  function f(a) {
    a = +a;
    return fround(a);
  }

  return { f: f };
}

var f = Module({ Math: Math }).f;

function runNearStackLimit()  {
  function g() { try { g(); } catch(e) { f(); } };
  g();
}

(function () {
  function g() {}

  runNearStackLimit(g);
})();
