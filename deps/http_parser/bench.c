/* Copyright Fedor Indutny. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */
#include "http_parser.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

/* 8 gb */
static const int64_t kBytes = 8LL << 30;

static const char data[] =
    "\x50\x4f\x53\x54\x20\x2f\x6a\x6f\x79\x65\x6e\x74\x2f\x68\x74\x74\x70\x2d\x70\x61\x72\x73\x65\x72\x20\x48\x54\x54\x50\x2f\x31\x2e\x31\xd\xa"
    "\x48\x6f\x73\x74\x3a\x20\x67\x69\x74\x68\x75\x62\x2e\x63\x6f\x6d\xd\xa"
    "\x44\x4e\x54\x3a\x20\x31\xd\xa"
    "\x41\x63\x63\x65\x70\x74\x2d\x45\x6e\x63\x6f\x64\x69\x6e\x67\x3a\x20\x67\x7a\x69\x70\x2c\x20\x64\x65\x66\x6c\x61\x74\x65\x2c\x20\x73\x64\x63\x68\xd\xa"
    "\x41\x63\x63\x65\x70\x74\x2d\x4c\x61\x6e\x67\x75\x61\x67\x65\x3a\x20\x72\x75\x2d\x52\x55\x2c\x72\x75\x3b\x71\x3d\x30\x2e\x38\x2c\x65\x6e\x2d\x55\x53\x3b\x71\x3d\x30\x2e\x36\x2c\x65\x6e\x3b\x71\x3d\x30\x2e\x34\xd\xa"
    "\x55\x73\x65\x72\x2d\x41\x67\x65\x6e\x74\x3a\x20\x4d\x6f\x7a\x69\x6c\x6c\x61\x2f\x35\x2e\x30\x20\x28\x4d\x61\x63\x69\x6e\x74\x6f\x73\x68\x3b\x20\x49\x6e\x74\x65\x6c\x20\x4d\x61\x63\x20\x4f\x53\x20\x58\x20\x31\x30\x5f\x31\x30\x5f\x31\x29\x20"
        "\x41\x70\x70\x6c\x65\x57\x65\x62\x4b\x69\x74\x2f\x35\x33\x37\x2e\x33\x36\x20\x28\x4b\x48\x54\x4d\x4c\x2c\x20\x6c\x69\x6b\x65\x20\x47\x65\x63\x6b\x6f\x29\x20"
        "\x43\x68\x72\x6f\x6d\x65\x2f\x33\x39\x2e\x30\x2e\x32\x31\x37\x31\x2e\x36\x35\x20\x53\x61\x66\x61\x72\x69\x2f\x35\x33\x37\x2e\x33\x36\xd\xa"
    "\x41\x63\x63\x65\x70\x74\x3a\x20\x74\x65\x78\x74\x2f\x68\x74\x6d\x6c\x2c\x61\x70\x70\x6c\x69\x63\x61\x74\x69\x6f\x6e\x2f\x78\x68\x74\x6d\x6c\x2b\x78\x6d\x6c\x2c\x61\x70\x70\x6c\x69\x63\x61\x74\x69\x6f\x6e\x2f\x78\x6d\x6c\x3b\x71\x3d\x30\x2e\x39\x2c"
        "\x69\x6d\x61\x67\x65\x2f\x77\x65\x62\x70\x2c\x2a\x2f\x2a\x3b\x71\x3d\x30\x2e\x38\xd\xa"
    "\x52\x65\x66\x65\x72\x65\x72\x3a\x20\x68\x74\x74\x70\x73\x3a\x2f\x2f\x67\x69\x74\x68\x75\x62\x2e\x63\x6f\x6d\x2f\x6a\x6f\x79\x65\x6e\x74\x2f\x68\x74\x74\x70\x2d\x70\x61\x72\x73\x65\x72\xd\xa"
    "\x43\x6f\x6e\x6e\x65\x63\x74\x69\x6f\x6e\x3a\x20\x6b\x65\x65\x70\x2d\x61\x6c\x69\x76\x65\xd\xa"
    "\x54\x72\x61\x6e\x73\x66\x65\x72\x2d\x45\x6e\x63\x6f\x64\x69\x6e\x67\x3a\x20\x63\x68\x75\x6e\x6b\x65\x64\xd\xa"
    "\x43\x61\x63\x68\x65\x2d\x43\x6f\x6e\x74\x72\x6f\x6c\x3a\x20\x6d\x61\x78\x2d\x61\x67\x65\x3d\x30\xd\xa\xd\xa\x62\xd\xa\x68\x65\x6c\x6c\x6f\x20\x77\x6f\x72\x6c\x64\xd\xa\x30\xd\xa";
static const size_t data_len = sizeof(data) - 1;

static int on_info(http_parser* p) {
  return 0;
}


static int on_data(http_parser* p, const char *at, size_t length) {
  return 0;
}

static http_parser_settings settings = {
  .on_message_begin = on_info,
  .on_headers_complete = on_info,
  .on_message_complete = on_info,
  .on_header_field = on_data,
  .on_header_value = on_data,
  .on_url = on_data,
  .on_status = on_data,
  .on_body = on_data
};

int bench(int iter_count, int silent) {
  struct http_parser parser;
  int i;
  int err;
  struct timeval start;
  struct timeval end;

  if (!silent) {
    err = gettimeofday(&start, NULL);
    assert(err == 0);
  }

  fprintf(stderr, "req_len=%d\n", (int) data_len);
  for (i = 0; i < iter_count; i++) {
    size_t parsed;
    http_parser_init(&parser, HTTP_REQUEST);

    parsed = http_parser_execute(&parser, &settings, data, data_len);
    assert(parsed == data_len);
  }

  if (!silent) {
    double elapsed;
    double bw;
    double total;

    err = gettimeofday(&end, NULL);
    assert(err == 0);

    fprintf(stdout, "Benchmark result:\n");

    elapsed = (double) (end.tv_sec - start.tv_sec) +
              (end.tv_usec - start.tv_usec) * 1e-6f;

    total = (double) iter_count * data_len;
    bw = (double) total / elapsed;

    fprintf(stdout, "%.2f mb | %.2f mb/s | %.2f req/sec | %.2f s\n",
        (double) total / (1024 * 1024),
        bw / (1024 * 1024),
        (double) iter_count / elapsed,
        elapsed);

    fflush(stdout);
  }

  return 0;
}

int main(int argc, char** argv) {
  int64_t iterations;

  iterations = kBytes / (int64_t) data_len;
  if (argc == 2 && strcmp(argv[1], "\x69\x6e\x66\x69\x6e\x69\x74\x65") == 0) {
    for (;;)
      bench(iterations, 1);
    return 0;
  } else {
    return bench(iterations, 0);
  }
}
