// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// On z/OS, this test needs a stack size of at least 204 kBytes.
// Flags: --stack-size=220

function call_replace_close_to_stack_overflow() {
  try {
    call_replace_close_to_stack_overflow();
  } catch {
    "b".replace(/(b)/g);
  }
}

call_replace_close_to_stack_overflow();
