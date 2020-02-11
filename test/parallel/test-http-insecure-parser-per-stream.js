'use strict';
const common = require('../common');
const assert = require('assert');
const http = require('http');
const net = require('net');

// Test that setting the `insecureHTTPParser` option works on a per-stream-basis.

// Test 1: The server sends an invalid header.
{
  const server = net.createServer(common.mustCall((socket) => {
    socket.end('HTTP/1.1 200 OK\r\n' +
            'Hello: foo\x08foo\r\n' +
            'Content-Length: 0\r\n' +
            '\r\n\r\n');
    server.close();
  }));

  server.listen(0, common.mustCall(function() {
    const client = http.request({
      port: this.address().port,
      insecureHTTPParser: true
    }, common.mustCall((res) => {
      assert.strictEqual(res.headers.hello, 'foo\x08foo');
      res.resume(); // We don't actually care about contents.
      res.on('end', common.mustCall());
    }));
    client.end();
  }));
}

// Test 2: The same as Test 1 except without the option, to make sure it fails.
{
  const server = net.createServer(common.mustCall((socket) => {
    socket.end('HTTP/1.1 200 OK\r\n' +
            'Hello: foo\x08foo\r\n' +
            'Content-Length: 0\r\n' +
            '\r\n\r\n');
    server.close();
  }));

  server.listen(0, common.mustCall(function() {
    const client = http.request({
      port: this.address().port
    }, common.mustNotCall());
    client.end();
    client.on('error', common.mustCall());
  }));
}

// Test 3: The client sends an invalid header.
{
  const testData = 'Hello, World!\n';
  const server = http.createServer(
    { insecureHTTPParser: true },
    common.mustCall((req, res) => {
      res.statusCode = 200;
      res.setHeader('Content-Type', 'text/plain');
      res.end(testData);
      server.close();
    }));

  server.on('clientError', common.mustNotCall());

  server.listen(0, common.mustCall(function() {
    const client = net.createConnection({
      port: this.address().port
    }, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\n' +
                   'Hello: foo\x08foo\r\n' +
                   '\r\n\r\n');
      client.end();
    }));
  }));
}

// Test 4: The same as Test 3 except without the option, to make sure it fails.
{
  const server = http.createServer(common.mustNotCall());

  server.on('clientError', common.mustCall(() => {
    server.close();
  }));

  server.listen(0, common.mustCall(function() {
    const client = net.createConnection({
      port: this.address().port
    }, common.mustCall(() => {
      client.write('GET / HTTP/1.1\r\n' +
                   'Hello: foo\x08foo\r\n' +
                   '\r\n\r\n');
      client.end();
    }));
  }));
}
