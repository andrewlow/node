// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.

'use strict';
const common = require('../common');
const assert = require('assert');
const spawn = require('child_process').spawn;
var cat;
if (process.platform === 'zos') {
  const teststr = "cat is running";
  cat = spawn('cat', ['-u']);
  cat.stdin.write(teststr);
  cat.stdout.on('data', (data) => {
    if (data.toString() !== teststr) {
      console.error("error: cat emitted '" + data.toString() + "', expected '" + teststr + "'");
      process.exit(1);
    }
  });
} else {
  cat = spawn(common.isWindows ? 'cmd' : 'cat');
}

cat.stdout.on('end', common.mustCall());
if (process.platform === 'zos') {
  cat.stderr.on('data',(data) => {
    if (!data.toString().match(/^\s*CEE5205S The signal SIGTERM was received.\s*/)) {
      console.error('error: found unexpected output in stderr: "',data.toString(),'"');
      process.exit(1);
    }
  });
} else {
  cat.stderr.on('data', common.mustNotCall());
}
cat.stderr.on('end', common.mustCall());
  
cat.on('exit', common.mustCall((code, signal) => {
  assert.strictEqual(code, null);
  assert.strictEqual(signal, 'SIGTERM');
}));
  
assert.strictEqual(cat.killed, false);
cat.kill();
assert.strictEqual(cat.killed, true);
