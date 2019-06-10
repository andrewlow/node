'use strict';
const { mustCall, spawnCat } = require('../common');
const assert = require('assert');

const cat = spawnCat();

assert.ok(process.kill(cat.pid, 0));

cat.on('exit', mustCall(function() {
  assert.throws(function() {
    process.kill(cat.pid, 0);
  }, Error);
}));

cat.stdout.on('data', mustCall(function() {
  process.kill(cat.pid, 'SIGKILL');
}));

// EPIPE when null sig fails
cat.stdin.write('test');
