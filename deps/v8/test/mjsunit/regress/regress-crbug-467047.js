// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On z/OS, this test needs a stack size of at least 204 kBytes.
// Flags: --stack-size=220

function captureMatch(re) {
  var local_variable = 0;
  "abcd".replace(re, function() { });
  assertEquals("abcd", RegExp.input);
  assertEquals("a", RegExp.leftContext);
  assertEquals("bc", RegExp.lastMatch);
  assertEquals("d", RegExp.rightContext);
  assertEquals("foo", captureMatch(/^bar/));
}

assertThrows(function() { captureMatch(/(bc)/) }, RangeError);
