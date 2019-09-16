// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On z/OS, this test needs a stack size of at least 204 kBytes.
// Flags: --allow-natives-syntax --stack-size=220 --stress-inline

var source = "return 1" + new Array(2048).join(' + a') + "";
eval("function g(a) {" + source + "}");

function f(a) {
  return g(a);
};
%PrepareFunctionForOptimization(f);
%OptimizeFunctionOnNextCall(f);
try {
  f(0);
} catch (e) {
}
