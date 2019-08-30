#define _AE_BIMODAL 1
#undef _ENHANCED_ASCII_EXT
#define _ENHANCED_ASCII_EXT 0xFFFFFFFF
#define _XOPEN_SOURCE 600
#define _OPEN_SYS_FILE_EXT 1
#define __ZOS_CC
#include <_Ccsid.h>
#include <_Nascii.h>
#include <__le_api.h>
#include <builtins.h>
#include <ctest.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <iconv.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/__getipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <unistd.h>
#include <mutex>
#include <unordered_map>
#include <vector>
#include "zos.h"
static int __debug_mode = 0;
#if ' ' != 0x20
#error not build with correct codeset
#endif

int __argc = 1;
char** __argv;
extern void __settimelimit(int secs);
static int shmid_value(void);

static inline void* __convert_one_to_one(const void* table,
                                         void* dst,
                                         size_t size,
                                         const void* src) {
  void* rst = dst;
  __asm(" troo 2,%2,b'0001' \n jo *-4 \n"
        : "+NR:r3"(size), "+NR:r2"(dst), "+r"(src)
        : "NR:r1"(table)
        : "r0", "r1", "r2", "r3");
  return rst;
}
static inline unsigned strlen_ae(const unsigned char* str,
                                 int* code_page,
                                 int max_len,
                                 int* ambiguous) {
  static int last_ccsid = 819;
  static const unsigned char _tab_a[256] __attribute__((aligned(8))) = {
      1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  };
  static const unsigned char _tab_e[256] __attribute__((aligned(8))) = {
      1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 1,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0,
      1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1,
      1, 1, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
      0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
      1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
      1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0,
      0, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1,
  };
  unsigned long bytes;
  unsigned long code_out;
  const unsigned char* start;

  bytes = max_len;
  code_out = 0;
  start = str;
  __asm(" trte %1,%3,b'0000'\n"
        " jo *-4\n"
        : "+NR:r3"(bytes), "+NR:r2"(str), "+r"(bytes), "+r"(code_out)
        : "NR:r1"(_tab_a)
        : "r1", "r2", "r3");
  unsigned a_len = str - start;

  bytes = max_len;
  code_out = 0;
  str = start;
  __asm(" trte %1,%3,b'0000'\n"
        " jo *-4\n"
        : "+NR:r3"(bytes), "+NR:r2"(str), "+r"(bytes), "+r"(code_out)
        : "NR:r1"(_tab_e)
        : "r1", "r2", "r3");
  unsigned e_len = str - start;
  if (a_len > e_len) {
    *code_page = 819;
    last_ccsid = 819;
    *ambiguous = 0;
    return a_len;
  } else if (e_len > a_len) {
    *code_page = 1047;
    last_ccsid = 1047;
    *ambiguous = 0;
    return e_len;
  }
  *code_page = last_ccsid;
  *ambiguous = 1;
  return a_len;
}

static const unsigned char __ibm1047_iso88591[256]
    __attribute__((aligned(8))) = {
        0x00, 0x01, 0x02, 0x03, 0x9c, 0x09, 0x86, 0x7f, 0x97, 0x8d, 0x8e, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x9d, 0x0a, 0x08, 0x87,
        0x18, 0x19, 0x92, 0x8f, 0x1c, 0x1d, 0x1e, 0x1f, 0x80, 0x81, 0x82, 0x83,
        0x84, 0x85, 0x17, 0x1b, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x05, 0x06, 0x07,
        0x90, 0x91, 0x16, 0x93, 0x94, 0x95, 0x96, 0x04, 0x98, 0x99, 0x9a, 0x9b,
        0x14, 0x15, 0x9e, 0x1a, 0x20, 0xa0, 0xe2, 0xe4, 0xe0, 0xe1, 0xe3, 0xe5,
        0xe7, 0xf1, 0xa2, 0x2e, 0x3c, 0x28, 0x2b, 0x7c, 0x26, 0xe9, 0xea, 0xeb,
        0xe8, 0xed, 0xee, 0xef, 0xec, 0xdf, 0x21, 0x24, 0x2a, 0x29, 0x3b, 0x5e,
        0x2d, 0x2f, 0xc2, 0xc4, 0xc0, 0xc1, 0xc3, 0xc5, 0xc7, 0xd1, 0xa6, 0x2c,
        0x25, 0x5f, 0x3e, 0x3f, 0xf8, 0xc9, 0xca, 0xcb, 0xc8, 0xcd, 0xce, 0xcf,
        0xcc, 0x60, 0x3a, 0x23, 0x40, 0x27, 0x3d, 0x22, 0xd8, 0x61, 0x62, 0x63,
        0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0xab, 0xbb, 0xf0, 0xfd, 0xfe, 0xb1,
        0xb0, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0xaa, 0xba,
        0xe6, 0xb8, 0xc6, 0xa4, 0xb5, 0x7e, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
        0x79, 0x7a, 0xa1, 0xbf, 0xd0, 0x5b, 0xde, 0xae, 0xac, 0xa3, 0xa5, 0xb7,
        0xa9, 0xa7, 0xb6, 0xbc, 0xbd, 0xbe, 0xdd, 0xa8, 0xaf, 0x5d, 0xb4, 0xd7,
        0x7b, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0xad, 0xf4,
        0xf6, 0xf2, 0xf3, 0xf5, 0x7d, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
        0x51, 0x52, 0xb9, 0xfb, 0xfc, 0xf9, 0xfa, 0xff, 0x5c, 0xf7, 0x53, 0x54,
        0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0xb2, 0xd4, 0xd6, 0xd2, 0xd3, 0xd5,
        0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0xb3, 0xdb,
        0xdc, 0xd9, 0xda, 0x9f};

static const unsigned char __iso88591_ibm1047[256]
    __attribute__((aligned(8))) = {
        0x00, 0x01, 0x02, 0x03, 0x37, 0x2d, 0x2e, 0x2f, 0x16, 0x05, 0x15, 0x0b,
        0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x3c, 0x3d, 0x32, 0x26,
        0x18, 0x19, 0x3f, 0x27, 0x1c, 0x1d, 0x1e, 0x1f, 0x40, 0x5a, 0x7f, 0x7b,
        0x5b, 0x6c, 0x50, 0x7d, 0x4d, 0x5d, 0x5c, 0x4e, 0x6b, 0x60, 0x4b, 0x61,
        0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0x7a, 0x5e,
        0x4c, 0x7e, 0x6e, 0x6f, 0x7c, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
        0xc8, 0xc9, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8, 0xd9, 0xe2,
        0xe3, 0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xad, 0xe0, 0xbd, 0x5f, 0x6d,
        0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x91, 0x92,
        0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6,
        0xa7, 0xa8, 0xa9, 0xc0, 0x4f, 0xd0, 0xa1, 0x07, 0x20, 0x21, 0x22, 0x23,
        0x24, 0x25, 0x06, 0x17, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x09, 0x0a, 0x1b,
        0x30, 0x31, 0x1a, 0x33, 0x34, 0x35, 0x36, 0x08, 0x38, 0x39, 0x3a, 0x3b,
        0x04, 0x14, 0x3e, 0xff, 0x41, 0xaa, 0x4a, 0xb1, 0x9f, 0xb2, 0x6a, 0xb5,
        0xbb, 0xb4, 0x9a, 0x8a, 0xb0, 0xca, 0xaf, 0xbc, 0x90, 0x8f, 0xea, 0xfa,
        0xbe, 0xa0, 0xb6, 0xb3, 0x9d, 0xda, 0x9b, 0x8b, 0xb7, 0xb8, 0xb9, 0xab,
        0x64, 0x65, 0x62, 0x66, 0x63, 0x67, 0x9e, 0x68, 0x74, 0x71, 0x72, 0x73,
        0x78, 0x75, 0x76, 0x77, 0xac, 0x69, 0xed, 0xee, 0xeb, 0xef, 0xec, 0xbf,
        0x80, 0xfd, 0xfe, 0xfb, 0xfc, 0xba, 0xae, 0x59, 0x44, 0x45, 0x42, 0x46,
        0x43, 0x47, 0x9c, 0x48, 0x54, 0x51, 0x52, 0x53, 0x58, 0x55, 0x56, 0x57,
        0x8c, 0x49, 0xcd, 0xce, 0xcb, 0xcf, 0xcc, 0xe1, 0x70, 0xdd, 0xde, 0xdb,
        0xdc, 0x8d, 0x8e, 0xdf};

extern "C" void* _convert_e2a(void* dst, const void* src, size_t size) {
  int ccsid;
  int am;
  unsigned len = strlen_ae((unsigned char*)src, &ccsid, size, &am);
  if (ccsid == 819) {
    memcpy(dst, src, size);
    return dst;
  }
  return __convert_one_to_one(__ibm1047_iso88591, dst, size, src);
}
extern "C" void* _convert_a2e(void* dst, const void* src, size_t size) {
  int ccsid;
  int am;
  unsigned len = strlen_ae((unsigned char*)src, &ccsid, size, &am);
  if (ccsid == 1047) {
    memcpy(dst, src, size);
    return dst;
  }
  return __convert_one_to_one(__iso88591_ibm1047, dst, size, src);
}
extern "C" int __guess_ae(const void* src, size_t size) {
  int ccsid;
  int am;
  unsigned len = strlen_ae((unsigned char*)src, &ccsid, size, &am);
  return ccsid;
}

extern char** environ;  // this would be the ebcdic one

extern "C" char** __get_environ_np(void) {
  static char** __environ = 0;
  static long __environ_size = 0;
  char** start = environ;
  int cnt = 0;
  int size = 0;
  int len = 0;
  int arysize = 0;
  while (*start) {
    size += (strlen(*start) + 1);
    ++start;
    ++cnt;
  }
  arysize = (cnt + 1) * sizeof(void*);
  size += arysize;
  if (__environ) {
    if (__environ_size < size) {
      free(__environ);
      __environ_size = size;
      __environ = (char**)malloc(__environ_size);
    }
  } else {
    __environ_size = size;
    __environ = (char**)malloc(__environ_size);
  }
  char* p = (char*)__environ;
  p += arysize;
  int i;
  start = environ;
  for (i = 0; i < cnt; ++i) {
    __environ[i] = p;
    len = strlen(*start) + 1;
    __convert_one_to_one(__ibm1047_iso88591, p, len, *start);
    p += len;
    ++start;
  }
  __environ[i] = 0;
  return __environ;
}

int __setenv_a(const char*, const char*, int);
#pragma map(__setenv_a, "\174\174A00188")
extern "C" void __xfer_env(void) {
  char** start = __get_environ_np();
  int i;
  int len;
  char* str;
  char* a_str;
  while (*start) {
    str = *start;
    len = strlen(str);
    a_str = (char*)alloca(len + 1);
    memcpy(a_str, str, len);
    a_str[len] = 0;
    for (i = 0; i < len; ++i) {
      if (a_str[i] == u'=') {
        a_str[i] = 0;
        break;
      }
    }
    if (i < len) {
      int rc = __setenv_a(a_str, a_str + i + 1, 1);
      if (rc != 0) {
        __auto_ascii _a;
        __printf_a("__setenv_a %s=%s failed rc=%d\n", a_str, a_str + i + 1, rc);
      }
    }
    ++start;
  }
}

extern "C" int __chgfdccsid(int fd, unsigned short ccsid) {
  attrib_t attr;
  memset(&attr, 0, sizeof(attr));
  attr.att_filetagchg = 1;
  attr.att_filetag.ft_ccsid = ccsid;
  if (ccsid != FT_BINARY) {
    attr.att_filetag.ft_txtflag = 1;
  }
  return __fchattr(fd, &attr, sizeof(attr));
}

extern "C" int __getfdccsid(int fd) {
  struct stat st;
  int rc;
  rc = fstat(fd, &st);
  if (rc != 0) return -1;
  unsigned short ccsid = st.st_tag.ft_ccsid;
  if (st.st_tag.ft_txtflag) {
    return 65536 + ccsid;
  }
  return ccsid;
}

static void ledump(const char* title) {
  __auto_ascii _a;
  __cdump_a((char*)title);
}
#if DEBUG_ONLY
extern "C" size_t __e2a_l(char* bufptr, size_t szLen) {
  int ccsid;
  int am;
  if (0 == bufptr) {
    errno = EINVAL;
    return -1;
  }
  unsigned len = strlen_ae((const unsigned char*)bufptr, &ccsid, szLen, &am);

  if (ccsid == 819) {
    if (__debug_mode && !am) {
      /*
      __dump_title(2, bufptr, szLen, 16,
                   "Attempt convert from ASCII to ASCII \n");
      ledump((char *)"Attempt convert from ASCII to ASCII");
      */
      return szLen;
    }
    // return szLen; restore to convert
  }

  __convert_one_to_one(__ibm1047_iso88591, bufptr, szLen, bufptr);
  return szLen;
}
extern "C" size_t __a2e_l(char* bufptr, size_t szLen) {
  int ccsid;
  int am;
  if (0 == bufptr) {
    errno = EINVAL;
    return -1;
  }
  unsigned len = strlen_ae((const unsigned char*)bufptr, &ccsid, szLen, &am);

  if (ccsid == 1047) {
    if (__debug_mode && !am) {
      /*
     __dump_title(2, bufptr, szLen, 16,
                  "Attempt convert from EBCDIC to EBCDIC\n");
     ledump((char *)"Attempt convert from EBCDIC to EBCDIC");
     */
      return szLen;
    }
    // return szLen; restore to convert
  }
  __convert_one_to_one(__iso88591_ibm1047, bufptr, szLen, bufptr);
  return szLen;
}
extern "C" size_t __e2a_s(char* string) {
  if (0 == string) {
    errno = EINVAL;
    return -1;
  }
  return __e2a_l(string, strlen(string));
}
extern "C" size_t __a2e_s(char* string) {
  if (0 == string) {
    errno = EINVAL;
    return -1;
  }
  return __a2e_l(string, strlen(string));
}
#endif

static void __console(const void* p_in, int len_i) {
  const unsigned char* p = (const unsigned char*)p_in;
  int len = len_i;
  while (p[len] == 0x15 && len > 0) {
    --len;
  }
  typedef struct wtob {
    unsigned short sz;
    unsigned short flags;
    unsigned char msgarea[130];
  } wtob_t;
  wtob_t* m = (wtob_t*)__malloc31(134);
  while (len > 126) {
    m->sz = 130;
    m->flags = 0x8000;
    memcpy(m->msgarea, p, 126);
    memcpy(m->msgarea + 126, "\x20\x00\x00\x20", 4);
    __asm(" la  0,0 \n"
          " lr  1,%0 \n"
          " svc 35 \n"
          :
          : "r"(m)
          : "r0", "r1", "r15");
    p += 126;
    len -= 126;
  }
  if (len > 0) {
    m->sz = len + 4;
    m->flags = 0x8000;
    memcpy(m->msgarea, p, len);
    memcpy(m->msgarea + len, "\x20\x00\x00\x20", 4);
    __asm(" la  0,0 \n"
          " lr  1,%0 \n"
          " svc 35 \n"
          :
          : "r"(m)
          : "r0", "r1", "r15");
  }
  free(m);
}
extern "C" int __console_printf(const char* fmt, ...) {
  va_list ap;
  char* buf;
  int len;
  va_start(ap, fmt);
  va_list ap1;
  va_list ap2;
  va_copy(ap1, ap);
  va_copy(ap2, ap);
  int bytes;
  int ccsid;
  int am;
  strlen_ae((const unsigned char*)fmt, &ccsid, strlen(fmt) + 1, &am);
  int mode;
  if (ccsid == 819) {
    mode = __ae_thread_swapmode(__AE_ASCII_MODE);
    bytes = __vsnprintf_a(0, 0, fmt, ap1);
    buf = (char*)alloca(bytes + 1);
    len = __vsnprintf_a(buf, bytes + 1, fmt, ap2);
    __a2e_l(buf, len);
  } else {
    mode = __ae_thread_swapmode(__AE_EBCDIC_MODE);
    bytes = __vsnprintf_e(0, 0, fmt, ap1);
    buf = (char*)alloca(bytes + 1);
    len = __vsnprintf_e(buf, bytes + 1, fmt, ap2);
  }
  va_end(ap2);
  va_end(ap1);
  va_end(ap);
  if (len < 0) goto quit;
  __console(buf, len);
quit:
  __ae_thread_swapmode(mode);
  return len;
}

extern "C" int vdprintf(int fd, const char* fmt, va_list ap) {
  int ccsid;
  int am;
  strlen_ae((const unsigned char*)fmt, &ccsid, strlen(fmt) + 1, &am);
  int mode;
  int len;
  int bytes;
  char* buf;
  va_list ap1;
  va_list ap2;
  va_copy(ap1, ap);
  va_copy(ap2, ap);
  if (ccsid == 819) {
    mode = __ae_thread_swapmode(__AE_ASCII_MODE);
    bytes = __vsnprintf_a(0, 0, fmt, ap1);
    buf = (char*)alloca(bytes + 1);
    len = __vsnprintf_a(buf, bytes + 1, fmt, ap2);
  } else {
    mode = __ae_thread_swapmode(__AE_EBCDIC_MODE);
    bytes = __vsnprintf_e(0, 0, fmt, ap1);
    buf = (char*)alloca(bytes + 1);
    len = __vsnprintf_e(buf, bytes + 1, fmt, ap2);
  }
  if (len == -1) goto quit;
  len = write(fd, buf, len);
quit:
  __ae_thread_swapmode(mode);
  return len;
}
extern "C" int dprintf(int fd, const char* fmt, ...) {
  va_list ap;
  char* buf;
  int len;
  va_start(ap, fmt);
  va_list ap1;
  va_list ap2;
  va_copy(ap1, ap);
  va_copy(ap2, ap);
  int bytes;
  int ccsid;
  int am;
  strlen_ae((const unsigned char*)fmt, &ccsid, strlen(fmt) + 1, &am);
  int mode;
  if (ccsid == 819) {
    mode = __ae_thread_swapmode(__AE_ASCII_MODE);
    bytes = __vsnprintf_a(0, 0, fmt, ap1);
    buf = (char*)alloca(bytes + 1);
    len = __vsnprintf_a(buf, bytes + 1, fmt, ap2);
  } else {
    mode = __ae_thread_swapmode(__AE_EBCDIC_MODE);
    bytes = __vsnprintf_e(0, 0, fmt, ap1);
    buf = (char*)alloca(bytes + 1);
    len = __vsnprintf_e(buf, bytes + 1, fmt, ap2);
  }
  va_end(ap2);
  va_end(ap1);
  va_end(ap);
  if (len == -1) goto quit;
  len = write(fd, buf, len);
quit:
  __ae_thread_swapmode(mode);
  return len;
}

extern void __dump_title(
    int fd, const void* addr, size_t len, size_t bw, const char* format, ...);

extern void __dump(int fd, const void* addr, size_t len, size_t bw) {
  __dump_title(fd, addr, len, bw, 0);
}

extern void __dump_title(
    int fd, const void* addr, size_t len, size_t bw, const char* format, ...) {
  static const unsigned char* atbl = (unsigned char*)"................"
                                                     "................"
                                                     " !\"#$%&'()*+,-./"
                                                     "0123456789:;<=>?"
                                                     "@ABCDEFGHIJKLMNO"
                                                     "PQRSTUVWXYZ[\\]^_"
                                                     "`abcdefghijklmno"
                                                     "pqrstuvwxyz{|}~."
                                                     "................"
                                                     "................"
                                                     "................"
                                                     "................"
                                                     "................"
                                                     "................"
                                                     "................"
                                                     "................";
  static const unsigned char* etbl = (unsigned char*)"................"
                                                     "................"
                                                     "................"
                                                     "................"
                                                     " ...........<(+|"
                                                     "&.........!$*);^"
                                                     "-/.........,%_>?"
                                                     ".........`:#@'=\""
                                                     ".abcdefghi......"
                                                     ".jklmnopqr......"
                                                     ".~stuvwxyz...[.."
                                                     ".............].."
                                                     "{ABCDEFGHI......"
                                                     "}JKLMNOPQR......"
                                                     "\\.STUVWXYZ......"
                                                     "0123456789......";
  const unsigned char* p = (const unsigned char*)addr;
  if (format) {
    va_list ap;
    va_start(ap, format);
    vdprintf(fd, format, ap);
    va_end(ap);
  } else {
    dprintf(fd, "Dump: \"Address: Content in Hexdecimal, ASCII, EBCDIC\"\n");
  }
  if (bw < 16 && bw > 64) {
    bw = 16;
  }
  unsigned char line[2048];
  const unsigned char* buffer;
  long offset = 0;
  long sz = 0;
  long b = 0;
  long i, j;
  int c;
  __auto_ascii _a;
  while (len > 0) {
    sz = (len > (bw - 1)) ? bw : len;
    buffer = p + offset;
    b = 0;
    b += __snprintf_a((char*)line + b, 2048 - b, "%*p:", 16, buffer);
    for (i = 0; i < sz; ++i) {
      if ((i & 3) == 0) line[b++] = ' ';
      c = buffer[i];
      line[b++] = "0123456789abcdef"[(0xf0 & c) >> 4];
      line[b++] = "0123456789abcdef"[(0x0f & c)];
    }
    for (; i < bw; ++i) {
      if ((i & 3) == 0) line[b++] = ' ';
      line[b++] = ' ';
      line[b++] = ' ';
    }
    line[b++] = ' ';
    line[b++] = '|';
    for (i = 0; i < sz; ++i) {
      c = buffer[i];
      if (c == -1) {
        line[b++] = '*';
      } else {
        line[b++] = atbl[c];
      }
    }
    for (; i < bw; ++i) {
      line[b++] = ' ';
    }
    line[b++] = '|';
    line[b++] = ' ';
    line[b++] = '|';
    for (i = 0; i < sz; ++i) {
      c = buffer[i];
      if (c == -1) {
        line[b++] = '*';
      } else {
        line[b++] = etbl[c];
      }
    }
    for (; i < bw; ++i) {
      line[b++] = ' ';
    }
    line[b++] = '|';
    line[b++] = 0;
    dprintf(fd, "%-.*s\n", b, line);
    offset += sz;
    len -= sz;
  }
}

__auto_ascii::__auto_ascii(void) {
  ascii_mode = __isASCII();
  if (ascii_mode == 0) __ae_thread_swapmode(__AE_ASCII_MODE);
}
__auto_ascii::~__auto_ascii(void) {
  if (ascii_mode == 0) __ae_thread_swapmode(__AE_EBCDIC_MODE);
}

static void init_tf_parms_t(__tf_parms_t* parm,
                            char* pu_name_buf,
                            size_t len1,
                            char* entry_name_buf,
                            size_t len2,
                            char* stmt_id_buf,
                            size_t len3) {
  _FEEDBACK fc;
  parm->__tf_pu_name.__tf_buff = pu_name_buf;
  parm->__tf_pu_name.__tf_bufflen = len1;
  parm->__tf_entry_name.__tf_buff = entry_name_buf;
  parm->__tf_entry_name.__tf_bufflen = len2;
  parm->__tf_statement_id.__tf_buff = stmt_id_buf;
  parm->__tf_statement_id.__tf_bufflen = len3;
  parm->__tf_dsa_addr = 0;
  parm->__tf_caa_addr = 0;
  parm->__tf_call_instruction = 0;
  int skip = 2;
  while (skip > 0 && !parm->__tf_is_main) {
    ____le_traceback_a(__TRACEBACK_FIELDS, parm, &fc);
    parm->__tf_dsa_addr = parm->__tf_caller_dsa_addr;
    parm->__tf_call_instruction = parm->__tf_caller_call_instruction;
    --skip;
  }
}

static int backtrace_w(void** buffer, int size);

extern "C" int backtrace(void** buffer, int size) {
  int mode;
  int result;
  mode = __ae_thread_swapmode(__AE_ASCII_MODE);
  result = backtrace_w(buffer, size);
  __ae_thread_swapmode(mode);
  return result;
}

int backtrace_w(void** input_buffer, int size) {
  void** buffer = input_buffer;
  __tf_parms_t tbck_parms;
  _FEEDBACK fc;
  int rc = 0;
  init_tf_parms_t(&tbck_parms, 0, 0, 0, 0, 0, 0);
  while (size > 0 && !tbck_parms.__tf_is_main) {
    ____le_traceback_a(__TRACEBACK_FIELDS, &tbck_parms, &fc);
    if (fc.tok_sev >= 2) {
      dprintf(2, "____le_traceback_a() service failed\n");
      return 0;
    }
    *buffer = tbck_parms.__tf_dsa_addr;
    tbck_parms.__tf_dsa_addr = tbck_parms.__tf_caller_dsa_addr;
    tbck_parms.__tf_call_instruction = tbck_parms.__tf_caller_call_instruction;
    --size;
    if (rc > 0) {
      if (input_buffer[rc - 1] >= input_buffer[rc]) {
        // xplink stack address is not increasing as we go up, could be stack
        // corruption
        input_buffer[rc] = 0;
        --rc;
        return rc;
      }
    }
    ++buffer;
    ++rc;
  }
  return rc;
}

static void backtrace_symbols_w(void* const* buffer,
                                int size,
                                int fd,
                                char*** return_string);

extern "C" char** backtrace_symbols(void* const* buffer, int size) {
  int mode;
  char** result;
  mode = __ae_thread_swapmode(__AE_ASCII_MODE);
  backtrace_symbols_w(buffer, size, -1, &result);
  __ae_thread_swapmode(mode);
  return result;
}
void backtrace_symbols_w(void* const* buffer,
                         int size,
                         int fd,
                         char*** return_string) {
  int sz;
  char* return_buff;
  char** table;
  char* stringpool;
  char* buff_end;
  __tf_parms_t tbck_parms;
  char pu_name[256];
  char entry_name[256];
  char stmt_id[256];
  char* return_addr;
  _FEEDBACK fc;
  int rc = 0;
  int i;
  int cnt;
  int inst;
  void* caller_dsa = 0;
  void* caller_inst = 0;

  sz = ((size + 1) * 300);  // estimate
  if (fd == -1) {
    return_buff = (char*)malloc(sz);
  }
  while (return_buff != 0 || (return_buff == 0 && fd != -1)) {
    if (fd == -1) {
      table = (char**)return_buff;
      stringpool = return_buff + ((size + 1) * sizeof(void*));
      buff_end = return_buff + sz;
    }
    init_tf_parms_t(&tbck_parms, pu_name, 256, entry_name, 256, stmt_id, 256);
    for (i = 0; i < size; ++i) {
      if (i > 0) {
        tbck_parms.__tf_dsa_addr = buffer[i];
      }
      if (tbck_parms.__tf_dsa_addr == caller_dsa) {
        tbck_parms.__tf_call_instruction = caller_inst;
      } else {
        tbck_parms.__tf_call_instruction = 0;
      }
      ____le_traceback_a(__TRACEBACK_FIELDS, &tbck_parms, &fc);
      if (fc.tok_sev >= 2) {
        dprintf(2, "____le_traceback_a() service failed\n");
        free(return_buff);
        *return_string = 0;
        return;
      }
      caller_dsa = tbck_parms.__tf_caller_dsa_addr;
      caller_inst = tbck_parms.__tf_caller_call_instruction;
      inst = *(char*)(tbck_parms.__tf_caller_call_instruction);
      if (inst == 0xa7) {
        // BRAS
        return_addr = 6 + (char*)tbck_parms.__tf_caller_call_instruction;
      } else {
        // BASR
        return_addr = 4 + (char*)tbck_parms.__tf_caller_call_instruction;
      }
      if (tbck_parms.__tf_call_instruction) {
        if (pu_name[0]) {
          if (fd == -1)
            cnt = __snprintf_a(stringpool,
                               buff_end - stringpool,
                               "%s:%s (%s+0x%lx) [0x%p]",
                               pu_name,
                               stmt_id,
                               entry_name,
                               (char*)tbck_parms.__tf_call_instruction -
                                   (char*)tbck_parms.__tf_entry_addr,
                               return_addr);
          else
            dprintf(fd,
                    "%s:%s (%s+0x%lx) [0x%p]\n",
                    pu_name,
                    stmt_id,
                    entry_name,
                    (char*)tbck_parms.__tf_call_instruction -
                        (char*)tbck_parms.__tf_entry_addr,
                    return_addr);

        } else {
          if (fd == -1)
            cnt = __snprintf_a(stringpool,
                               buff_end - stringpool,
                               "(%s+0x%lx) [0x%p]",
                               entry_name,
                               (char*)tbck_parms.__tf_call_instruction -
                                   (char*)tbck_parms.__tf_entry_addr,
                               return_addr);
          else
            dprintf(fd,
                    "(%s+0x%lx) [0x%p]\n",
                    entry_name,
                    (char*)tbck_parms.__tf_call_instruction -
                        (char*)tbck_parms.__tf_entry_addr,
                    return_addr);
        }
      } else {
        if (pu_name[0]) {
          if (fd == -1)
            cnt = __snprintf_a(stringpool,
                               buff_end - stringpool,
                               "%s:%s (%s) [0x%p]",
                               pu_name,
                               stmt_id,
                               entry_name,
                               return_addr);
          else
            dprintf(fd,
                    "%s:%s (%s) [0x%p]\n",
                    pu_name,
                    stmt_id,
                    entry_name,
                    return_addr);
        } else {
          if (fd == -1)
            cnt = __snprintf_a(stringpool,
                               buff_end - stringpool,
                               "(%s) [0x%p]",
                               entry_name,
                               return_addr);
          else
            dprintf(fd, "(%s) [0x%p]\n", entry_name, return_addr);
        }
      }
      if (fd == -1) {
        if (cnt < 0 || cnt >= (buff_end - stringpool)) {
          // out of space
          break;
        }
        table[i] = stringpool;
        stringpool += (cnt + 1);
      }
    }
    if (fd == -1) {
      if (i == size) {
        // return &table[0];
        table[i] = 0;
        *return_string = &table[0];
        return;
      }
      free(return_buff);
      sz += (size * 300);
      return_buff = (char*)malloc(sz);
    } else
      return;
  }
}

extern "C" void backtrace_symbols_fd(void* const* buffer, int size, int fd) {
  int mode;
  mode = __ae_thread_swapmode(__AE_ASCII_MODE);
  backtrace_symbols_w(buffer, size, fd, 0);
  __ae_thread_swapmode(mode);
}

extern "C" void __display_backtrace(int fd) {
  void* buffer[4096];
  int nptrs = backtrace(buffer, 4096);
  backtrace_symbols_fd(buffer, nptrs, fd);
}

void __abend(int comp_code, unsigned reason_code, int flat_byte, void* plist) {
  unsigned long r15 = reason_code;
  unsigned long r1;
  void* __ptr32 r0 = plist;
  if (flat_byte == -1) flat_byte = 0x84;
  r1 = (flat_byte << 24) + (0x00ffffff & comp_code);
  __asm(" SVC 13\n" : : "NR:r0"(r0), "NR:r1"(r1), "NR:r15"(r15) :);
}

static const unsigned char ascii_to_lower[256] __attribute__((aligned(8))) = {
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b,
    0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20, 0x21, 0x22, 0x23,
    0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0x3e, 0x3f, 0x40, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73,
    0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f,
    0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b,
    0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77,
    0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81, 0x82, 0x83,
    0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b,
    0x9c, 0x9d, 0x9e, 0x9f, 0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7,
    0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3,
    0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf,
    0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc9, 0xca, 0xcb,
    0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7,
    0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3,
    0xe4, 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef,
    0xf0, 0xf1, 0xf2, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb,
    0xfc, 0xfd, 0xfe, 0xff};

int strcasecmp_ignorecp(const char* a, const char* b) {
  int len_a = strlen(a);
  int len_b = strlen(b);

  if (len_a != len_b) return len_a - len_b;
  if (!memcmp(a, b, len_a)) return 0;
  char* a_new = (char*)_convert_e2a(alloca(len_a + 1), a, len_a + 1);
  char* b_new = (char*)_convert_e2a(alloca(len_b + 1), b, len_b + 1);
  __convert_one_to_one(ascii_to_lower, a_new, len_a, a_new);
  __convert_one_to_one(ascii_to_lower, b_new, len_b, a_new);
  return strcmp(a_new, b_new);
}

int strncasecmp_ignorecp(const char* a, const char* b, size_t n) {
  int ccsid_a, ccsid_b;
  int am_a, am_b;
  unsigned len_a = strlen_ae((unsigned char*)a, &ccsid_a, n, &am_a);
  unsigned len_b = strlen_ae((unsigned char*)b, &ccsid_b, n, &am_b);
  char* a_new;
  char* b_new;
  if (len_a != len_b) return len_a - len_b;

  if (ccsid_a != 819) {
    a_new = (char*)__convert_one_to_one(
        __ibm1047_iso88591, alloca(len_a + 1), len_a, a);
    a_new[len_a] = 0;
    a_new = (char*)__convert_one_to_one(ascii_to_lower, a_new, len_a, a_new);
  } else {
    a_new = (char*)__convert_one_to_one(
        ascii_to_lower, alloca(len_a + 1), len_a, a);
    a_new[len_a] = 0;
  }

  if (ccsid_b != 819) {
    b_new = (char*)__convert_one_to_one(
        __ibm1047_iso88591, alloca(len_b + 1), len_b, b);
    b_new[len_b] = 0;
    b_new = (char*)__convert_one_to_one(ascii_to_lower, b_new, len_b, b_new);
  } else {
    b_new = (char*)__convert_one_to_one(
        ascii_to_lower, alloca(len_b + 1), len_b, b);
    b_new[len_b] = 0;
  }

  return strcmp(a_new, b_new);
}

class __csConverter {
  int fr_id;
  int to_id;
  char fr_name[_CSNAME_LEN_MAX + 1];
  char to_name[_CSNAME_LEN_MAX + 1];
  iconv_t cv;
  int valid;

 public:
  __csConverter(int fr_ccsid, int to_ccsid) : fr_id(fr_ccsid), to_id(to_ccsid) {
    valid = 0;
    if (0 != __toCSName(fr_id, fr_name)) {
      return;
    }
    if (0 != __toCSName(to_id, to_name)) {
      return;
    }
    if (fr_id != -1 && to_id != -1) {
      cv = iconv_open(fr_name, to_name);
      if (cv != (iconv_t)-1) {
        valid = 1;
      }
    }
  }
  int is_valid(void) { return valid; }
  ~__csConverter(void) {
    if (valid) iconv_close(cv);
  }
  size_t iconv(char** inbuf,
               size_t* inbytesleft,
               char** outbuf,
               size_t* outbytesleft) {
    return ::iconv(cv, inbuf, inbytesleft, outbuf, outbytesleft);
  }
  int conv(char* out, size_t outsize, const char* in, size_t insize) {
    size_t o_len = outsize;
    size_t i_len = insize;
    char* p = (char*)in;
    char* q = out;
    if (i_len == 0) return 0;
    int converted = ::iconv(cv, &p, &i_len, &q, &o_len);
    if (converted == -1) return -1;
    if (i_len == 0) {
      return outsize - o_len;
    }
    return -1;
  }
};

static void cleanupipc(int others) {
  IPCQPROC buf;
  int rc;
  int uid = getuid();
  int pid = getpid();
  int stop = -1;
  rc = __getipc(0, &buf, sizeof(buf), IPCQMSG);
  while (rc != -1 && stop != buf.msg.ipcqmid) {
    if (stop == -1) stop = buf.msg.ipcqmid;
    if (buf.msg.ipcqpcp.uid == uid) {
      if (buf.msg.ipcqkey == 0) {
        if (buf.msg.ipcqlrpid == pid) {
          msgctl(buf.msg.ipcqmid, IPC_RMID, 0);
        } else if (others && kill(buf.msg.ipcqlrpid, 0) == -1 &&
                   kill(buf.msg.ipcqlspid, 0) == -1) {
          msgctl(buf.msg.ipcqmid, IPC_RMID, 0);
        }
      }
    }
    rc = __getipc(rc, &buf, sizeof(buf), IPCQMSG);
  }
  if (others) {
    stop = -1;
    rc = __getipc(0, &buf, sizeof(buf), IPCQSHM);
    while (rc != -1 && stop != buf.shm.ipcqmid) {
      if (stop == -1) stop = buf.shm.ipcqmid;
      if (buf.shm.ipcqpcp.uid == uid) {
        if (buf.shm.ipcqcpid == pid) {
          shmctl(buf.shm.ipcqmid, IPC_RMID, 0);
        } else if (kill(buf.shm.ipcqcpid, 0) == -1) {
          shmctl(buf.shm.ipcqmid, IPC_RMID, 0);
        }
      }
      rc = __getipc(rc, &buf, sizeof(buf), IPCQSHM);
    }
  }
}
class __init {
  int mode;
  int cvstate;
  int forkmax;
  int* forkcurr;
  int shmid;

 public:
  __init() : forkmax(0), shmid(0) {
    // initialization
    unsigned long* x =
        (unsigned long*)((char***** __ptr32*)1208)[0][11][1][113][149];
    __argv = (char**)x[1];
    if ((unsigned long)(__argv) < 0x00000000FFFFFFFFUL) {
      __argc = 0;
    } else {
      if (0 == (x[0] & 0x00000000ffffffffUL)) {
        __argc = x[0] >> 32;
      } else {
        __argc = x[0];
      }
    }
    mode = __ae_thread_swapmode(__AE_ASCII_MODE);
    cvstate = __ae_autoconvert_state(_CVTSTATE_QUERY);
    if (_CVTSTATE_OFF == cvstate) {
      __ae_autoconvert_state(_CVTSTATE_ON);
    }
    char* cu = __getenv_a("__IPC_CLEANUP");
    if (cu && !memcmp(cu, "1", 2)) {
      cleanupipc(1);
    }
    char* dbg = __getenv_a("__NODERUNDEBUG");
    if (dbg && !memcmp(dbg, "1", 2)) {
      __debug_mode = 1;
    }
    char* tl = __getenv_a("__NODERUNTIMELIMIT");
    if (tl) {
      int sec = __atoi_a(tl);
      if (sec > 0) {
        __settimelimit(sec);
      }
    }
    char* fm = __getenv_a("__NODEFORKMAX");
    if (fm) {
      int v = __atoi_a(fm);
      if (v > 0) {
        forkmax = v;
        char path[1024];
        if (0 == getcwd(path, sizeof(path))) strcpy(path, "./");
        key_t key = ftok(path, 9021);
        shmid = shmget(key, 1024, 0666 | IPC_CREAT);
        forkcurr = (int*)shmat(shmid, (void*)0, 0);
        *forkcurr = 0;
      }
    }
  }
  int get_forkmax(void) { return forkmax; }
  int inc_forkcount(void) {
    if (0 == forkmax || 0 == shmid) return 0;
    int original;
    int new_value;

    do {
      original = *forkcurr;
      new_value = original + 1;
      __asm(" cs %0,%2,%1 \n "
            : "+r"(original), "+m"(*forkcurr)
            : "r"(new_value)
            :);
    } while (original != (new_value - 1));
    return new_value;
  }
  int shmid_value(void) { return shmid; }
  ~__init() {
    if (_CVTSTATE_OFF == cvstate) {
      __ae_autoconvert_state(cvstate);
    }
    __ae_thread_swapmode(mode);
    if (shmid != 0) {
      shmdt(forkcurr);
      shmctl(shmid, IPC_RMID, 0);
    }
    cleanupipc(0);
  }
};
static __init __a;
static __csConverter utf16_to_8(1208, 1200);
static __csConverter utf8_to_16(1200, 1208);

extern "C" int conv_utf8_utf16(char* out,
                               size_t outsize,
                               const char* in,
                               size_t insize) {
  return utf8_to_16.conv(out, outsize, in, insize);
}
extern "C" int conv_utf16_utf8(char* out,
                               size_t outsize,
                               const char* in,
                               size_t insize) {
  return utf16_to_8.conv(out, outsize, in, insize);
}

typedef struct timer_parm {
  int secs;
  pthread_t tid;
} timer_parm_t;

unsigned long __clock(void) {
  unsigned long long value, sec, nsec;
  __stckf(&value);
  return ((value / 512UL) * 125UL) - 2208988800000000000UL;
}
static void* _timer(void* parm) {
  timer_parm_t* tp = (timer_parm_t*)parm;
  unsigned long t0 = __clock();
  unsigned long t1 = t0;
  while ((t1 - t0) < ((tp->secs) * 1000000000)) {
    sleep(tp->secs);
    t1 = __clock();
  }
  if (__debug_mode) {
    dprintf(2, "Sent abort: __NODERUNTIMELIMIT was set to %d\n", tp->secs);
    raise(SIGABRT);
  }
  return 0;
}

extern void __settimelimit(int secs) {
  pthread_t tid;
  pthread_attr_t attr;
  int rc;
  timer_parm_t* tp = (timer_parm_t*)malloc(sizeof(timer_parm_t));
  tp->secs = secs;
  tp->tid = pthread_self();
  rc = pthread_attr_init(&attr);
  if (rc) {
    perror("timer:pthread_create");
    return;
  }
  rc = pthread_create(&tid, &attr, _timer, tp);
  if (rc) {
    perror("timer:pthread_create");
    return;
  }
  pthread_attr_destroy(&attr);
}
extern "C" void __setdebug(int v) {
  __debug_mode = v;
}
extern "C" int __indebug(void) {
  return __debug_mode;
}
extern "C" char** __getargv(void) {
  return __argv;
}
extern "C" int __getargc(void) {
  return __argc;
}

extern "C" void* __dlcb_next(void* last) {
  if (last == 0) {
    return ((char***** __ptr32*)1208)[0][11][1][113][193];
  }
  return ((char**)last)[0];
}
extern "C" int __dlcb_entry_name(char* buf, int size, void* dlcb) {
  unsigned short n;
  char* name;
  if (dlcb == 0) return 0;
  n = ((unsigned short*)dlcb)[44];
  name = ((char**)dlcb)[12];
  return __snprintf_a(
      buf,
      size,
      "%-.*s",
      n,
      __convert_one_to_one(__ibm1047_iso88591, alloca(n + 1), n, name));
}
extern "C" void* __dlcb_entry_addr(void* dlcb) {
  if (dlcb == 0) return 0;
  char* addr = ((char**)dlcb)[2];
  return addr;
}

static int return_abspath(char* out, int size, const char* path_file) {
  char buffer[1025];
  char* res = 0;
  if (path_file[0] != '/') res = __realpath_a(path_file, buffer);
  return __snprintf_a(out, size, "%s", res ? buffer : path_file);
}

extern "C" int __find_file_in_path(char* out,
                                   int size,
                                   const char* envvar,
                                   const char* file) {
  char* start = (char*)envvar;
  char path[1025];
  char real_path[1025];
  char path_file[1025];
  char* p = path;
  int len = 0;
  struct stat st;
  while (*start && (p < (path + 1024))) {
    if (*start == ':') {
      p = path;
      ++start;
      if (len > 0) {
        for (; len > 0 && path[len - 1] == '/'; --len)
          ;
        __snprintf_a(path_file, 1025, "%-.*s/%s", len, path, file);
        if (0 == __stat_a(path_file, &st)) {
          return return_abspath(out, size, path_file);
        }
        len = 0;
      }
    } else {
      ++len;
      *p++ = *start++;
    }
  }
  if (len > 0) {
    for (; len > 0 && path[len - 1] == '/'; --len)
      ;
    __snprintf_a(path_file, 1025, "%-.*s/%s", len, path, file);
    if (0 == __stat_a(path_file, &st)) {
      return return_abspath(out, size, path_file);
    }
  }
  return 0;
}

static char* __ptr32* __ptr32 __base(void) {
  static char* __ptr32* __ptr32 res = 0;
  if (res == 0) {
    res = ((char* __ptr32* __ptr32* __ptr32* __ptr32*)0)[4][136][6];
  }
  return res;
}
static void __bpx4kil(int pid,
                      int signal,
                      void* signal_options,
                      int* return_value,
                      int* return_code,
                      int* reason_code) {
  void* reg15 = __base()[308 / 4];  // BPX4KIL offset is 308
  void* argv[] = {&pid,
                  &signal,
                  signal_options,
                  return_value,
                  return_code,
                  reason_code};  // os style parm list
  __asm(" basr 14,%0\n" : "+NR:r15"(reg15) : "NR:r1"(&argv) : "r0", "r14");
}
static void __bpx4frk(int* pid, int* return_code, int* reason_code) {
  void* reg15 = __base()[240 / 4];                 // BPX4FRK offset is 240
  void* argv[] = {pid, return_code, reason_code};  // os style parm list
  __asm(" basr 14,%0\n" : "+NR:r15"(reg15) : "NR:r1"(&argv) : "r0", "r14");
}

extern "C" void abort(void) {
  __display_backtrace(STDERR_FILENO);
  kill(getpid(), SIGABRT);
}

// overriding LE's kill when linked statically
extern "C" int kill(int pid, int sig) {
  int rv, rc, rn;
  __bpx4kil(pid, sig, 0, &rv, &rc, &rn);
  if (rv != 0) errno = rc;
  return rv;
}
// overriding LE's fork when linked statically
extern "C" int __fork(void) {
  int cnt = __a.inc_forkcount();
  int max = __a.get_forkmax();
  if (cnt > max) {
    dprintf(2,
            "fork(): current count %d is greater than "
            "__NODEFORKMAX value %d, fork failed\n",
            cnt,
            max);
    errno = EPROCLIM;
    return -1;
  }
#if 0
  int rc, rn, pid;
  __bpx4frk(&pid, &rc, &rn);
  if (-1 == pid) {
    errno = rc;
  }
  return pid;
#else
  return fork();
#endif
}

struct IntHash {
  size_t operator()(const int& n) const { return n * 0x54edcfac64d7d667L; }
};

typedef unsigned long fd_attribute;

typedef std::unordered_map<int, fd_attribute, IntHash>::const_iterator cursor_t;

class fdAttributeCache {
  std::unordered_map<int, fd_attribute, IntHash> cache;
  std::mutex access_lock;

 public:
  fd_attribute get_attribute(int fd) {
    std::lock_guard<std::mutex> guard(access_lock);
    cursor_t c = cache.find(fd);
    if (c != cache.end()) {
      return c->second;
    }
    return 0;
  }
  void set_attribute(int fd, fd_attribute attr) {
    std::lock_guard<std::mutex> guard(access_lock);
    cache[fd] = attr;
  }
  void unset_attribute(int fd) {
    std::lock_guard<std::mutex> guard(access_lock);
    cache.erase(fd);
  }
  void clear(void) {
    std::lock_guard<std::mutex> guard(access_lock);
    cache.clear();
  }
};

fdAttributeCache fdcache;

enum notagread {
  __NO_TAG_READ_DEFAULT = 0,
  __NO_TAG_READ_DEFAULT_WITHWARNING = 1,
  __NO_TAG_READ_V6 = 2,
  __NO_TAG_READ_STRICT = 3
} notagread;

static enum notagread get_no_tag_read_behaviour(void) {
  char* ntr = __getenv_a("__UNTAGGED_READ_MODE");
  if (ntr && !strcmp(ntr, "DEFAULT")) {
    return __NO_TAG_READ_DEFAULT;
  } else if (ntr && !strcmp(ntr, "WARN")) {
    return __NO_TAG_READ_DEFAULT_WITHWARNING;
  } else if (ntr && !strcmp(ntr, "V6")) {
    return __NO_TAG_READ_V6;
  } else if (ntr && !strcmp(ntr, "STRICT")) {
    return __NO_TAG_READ_STRICT;
  }
  return __NO_TAG_READ_DEFAULT;  // defualt
}
static int no_tag_read_behaviour = get_no_tag_read_behaviour();

extern "C" void __fd_close(int fd) {
  fdcache.unset_attribute(fd);
}
extern "C" int __file_needs_conversion(int fd) {
  if (no_tag_read_behaviour == __NO_TAG_READ_STRICT) return 0;
  if (no_tag_read_behaviour == __NO_TAG_READ_V6) return 1;
  unsigned long attr = fdcache.get_attribute(fd);
  if (attr == 0x0000000000020000UL) {
    return 1;
  }
  return 0;
}
extern "C" int __file_needs_conversion_init(const char* name, int fd) {
  char buf[4096];
  off_t off;
  int cnt;
  if (no_tag_read_behaviour == __NO_TAG_READ_STRICT) return 0;
  if (no_tag_read_behaviour == __NO_TAG_READ_V6) {
    fdcache.set_attribute(fd, 0x0000000000020000UL);
    return 1;
  }
  if (lseek(fd, 1, SEEK_SET) == 1 && lseek(fd, 0, SEEK_SET) == 0) {
    // seekable file (real file)
    cnt = read(fd, buf, 4096);
    off = lseek(fd, 0, SEEK_SET);
    if (off != 0) {
      // introduce an error, because of the offset is no longer valide
      close(fd);
      return 0;
    }
    if (cnt > 8) {
      int ccsid;
      int am;
      unsigned len = strlen_ae((unsigned char*)buf, &ccsid, cnt, &am);
      if (ccsid == 1047) {
        if (no_tag_read_behaviour == __NO_TAG_READ_DEFAULT_WITHWARNING) {
          const char* filename = "(null)";
          if (name) {
            int len = strlen(name);
            filename =
                (const char*)_convert_e2a(alloca(len + 1), name, len + 1);
          }
          dprintf(2,
                  "Warning: File \"%s\"is untagged and seems to contain EBCDIC "
                  "characters\n",
                  filename);
        }
        fdcache.set_attribute(fd, 0x0000000000020000UL);
        return 1;
      }
    }        // seekable files
  }          // seekable files
  return 0;  // not seekable
}
extern "C" unsigned long __mach_absolute_time(void) {
  unsigned long long value, sec, nsec;
  __stckf(&value);
  return ((value / 512UL) * 125UL) - 2208988800000000000UL;
}

//------------------------------------------accounting for memory allocation
// begin

static const int kMegaByte = 1024 * 1024;
#define MAP_FAILED ((void*)-1L)

static int mem_account(void) {
  static int res = -1;
  if (-1 == res) {
    res = 0;
    char* ma = getenv("__MEM_ACCOUNT");
    if (ma && 0 == strcmp("1", ma)) {
      res = 1;
    }
  }
  return res;
}

static int gettcbtoken(char* out, int type) {
  typedef struct token_parm {
    char token[16];
    char* __ptr32 ascb;
    char type;
    char reserved[3];
  } token_parm_t;
  token_parm_t* tt = (token_parm_t*)__malloc31(sizeof(token_parm_t));
  memset(tt, 0, sizeof(token_parm_t));
  tt->type = type;
  long workreg;
  __asm(" LLGF %0,16(0,0) \n"
        " L %0,772(%0,0) \n"
        " L %0,212(%0,0) \n"
        " PC 0(%0) \n"
        : "=NR:r15"(workreg)  // also return code
        : "NR:r1"(tt)
        :);
  memcpy(out, (char*)tt, 16);
  free(tt);
  return workreg;
}

struct iarv64parm {
  unsigned char xversion __attribute__((__aligned__(16)));  //    0
  unsigned char xrequest;                                   //    1
  unsigned xmotknsource_system : 1;                         //    2
  unsigned xmotkncreator_system : 1;                        //    2(1)
  unsigned xmatch_motoken : 1;                              //    2(2)
  unsigned xflags0_rsvd1 : 5;                               //    2(3)
  unsigned char xkey;                                       //    3
  unsigned keyused_key : 1;                                 //    4
  unsigned keyused_usertkn : 1;                             //    4(1)
  unsigned keyused_ttoken : 1;                              //    4(2)
  unsigned keyused_convertstart : 1;                        //    4(3)
  unsigned keyused_guardsize64 : 1;                         //    4(4)
  unsigned keyused_convertsize64 : 1;                       //    4(5)
  unsigned keyused_motkn : 1;                               //    4(6)
  unsigned keyused_ownerjobname : 1;                        //    4(7)
  unsigned xcond_yes : 1;                                   //    5
  unsigned xfprot_no : 1;                                   //    5(1)
  unsigned xcontrol_auth : 1;                               //    5(2)
  unsigned xguardloc_high : 1;                              //    5(3)
  unsigned xchangeaccess_global : 1;                        //    5(4)
  unsigned xpageframesize_1meg : 1;                         //    5(5)
  unsigned xpageframesize_max : 1;                          //    5(6)
  unsigned xpageframesize_all : 1;                          //    5(7)
  unsigned xmatch_usertoken : 1;                            //    6
  unsigned xaffinity_system : 1;                            //    6(1)
  unsigned xuse2gto32g_yes : 1;                             //    6(2)
  unsigned xowner_no : 1;                                   //    6(3)
  unsigned xv64select_no : 1;                               //    6(4)
  unsigned xsvcdumprgn_no : 1;                              //    6(5)
  unsigned xv64shared_no : 1;                               //    6(6)
  unsigned xsvcdumprgn_all : 1;                             //    6(7)
  unsigned xlong_no : 1;                                    //    7
  unsigned xclear_no : 1;                                   //    7(1)
  unsigned xview_readonly : 1;                              //    7(2)
  unsigned xview_sharedwrite : 1;                           //    7(3)
  unsigned xview_hidden : 1;                                //    7(4)
  unsigned xconvert_toguard : 1;                            //    7(5)
  unsigned xconvert_fromguard : 1;                          //    7(6)
  unsigned xkeepreal_no : 1;                                //    7(7)
  unsigned long long xsegments;                             //    8
  unsigned char xttoken[16];                                //   16
  unsigned long long xusertkn;                              //   32
  void* xorigin;                                            //   40
  void* xranglist;                                          //   48
  void* xmemobjstart;                                       //   56
  unsigned xguardsize;                                      //   64
  unsigned xconvertsize;                                    //   68
  unsigned xaletvalue;                                      //   72
  int xnumrange;                                            //   76
  void* __ptr32 xv64listptr;                                //   80
  unsigned xv64listlength;                                  //   84
  unsigned long long xconvertstart;                         //   88
  unsigned long long xconvertsize64;                        //   96
  unsigned long long xguardsize64;                          //  104
  char xusertoken[8];                                       //  112
  unsigned char xdumppriority;                              //  120
  unsigned xdumpprotocol_yes : 1;                           //  121
  unsigned xorder_dumppriority : 1;                         //  121(1)
  unsigned xtype_pageable : 1;                              //  121(2)
  unsigned xtype_dref : 1;                                  //  121(3)
  unsigned xownercom_home : 1;                              //  121(4)
  unsigned xownercom_primary : 1;                           //  121(5)
  unsigned xownercom_system : 1;                            //  121(6)
  unsigned xownercom_byasid : 1;                            //  121(7)
  unsigned xv64common_no : 1;                               //  122
  unsigned xmemlimit_no : 1;                                //  122(1)
  unsigned xdetachfixed_yes : 1;                            //  122(2)
  unsigned xdoauthchecks_yes : 1;                           //  122(3)
  unsigned xlocalsysarea_yes : 1;                           //  122(4)
  unsigned xamountsize_4k : 1;                              //  122(5)
  unsigned xamountsize_1meg : 1;                            //  122(6)
  unsigned xmemlimit_cond : 1;                              //  122(7)
  unsigned keyused_dump : 1;                                //  123
  unsigned keyused_optionvalue : 1;                         //  123(1)
  unsigned keyused_svcdumprgn : 1;                          //  123(2)
  unsigned xattribute_defs : 1;                             //  123(3)
  unsigned xattribute_ownergone : 1;                        //  123(4)
  unsigned xattribute_notownergone : 1;                     //  123(5)
  unsigned xtrackinfo_yes : 1;                              //  123(6)
  unsigned xunlocked_yes : 1;                               //  123(7)
  unsigned char xdump;                                      //  124
  unsigned xpageframesize_pageable1meg : 1;                 //  125
  unsigned xpageframesize_dref1meg : 1;                     //  125(1)
  unsigned xsadmp_yes : 1;                                  //  125(2)
  unsigned xsadmp_no : 1;                                   //  125(3)
  unsigned xuse2gto64g_yes : 1;                             //  125(4)
  unsigned xdiscardpages_yes : 1;                           //  125(5)
  unsigned xexecutable_yes : 1;                             //  125(6)
  unsigned xexecutable_no : 1;                              //  125(7)
  unsigned short xownerasid;                                //  126
  unsigned char xoptionvalue;                               //  128
  unsigned char xrsv0001[8];                                //  129
  unsigned char xownerjobname[8];                           //  137
  unsigned char xrsv0004[7];                                //  145
  void* xdmapagetable;                                      //  152
  unsigned long long xunits;                                //  160
  unsigned keyused_units : 1;                               //  168
  unsigned xunitsize_1m : 1;                                //  168(1)
  unsigned xunitsize_2g : 1;                                //  168(2)
  unsigned xpageframesize_1m : 1;                           //  168(3)
  unsigned xpageframesize_2g : 1;                           //  168(4)
  unsigned xtype_fixed : 1;                                 //  168(5)
  unsigned xflags9_rsvd1 : 2;                               //  168(6)
  unsigned char xrsv0005[7];                                //  169
};
static long long __iarv64(void* parm, void** ptr, long long* reason_code_ptr) {
  long long rc;
  long long reason;
  __asm volatile(" lgr 1,%3 \n"
                 " llgtr 14,14 \n"
                 " l 14,16(0,0) \n"
                 " l 14,772(14,0) \n"
                 " l 14,208(14,0) \n"
                 " la 15,14 \n"
                 " or 14,15 \n"
                 " pc 0(14) \n"
                 " stg 1,%0 \n"
                 " stg 15,%1 \n"
                 " stg 0,%2 \n"
                 : "=m"(*ptr), "=m"(rc), "=m"(reason)
                 : "r"(parm)
                 : "r0", "r1", "r14", "r15");
  if (rc != 0 && reason_code_ptr != 0) {
    *reason_code_ptr = reason;
  }
  return rc;
}

static void* __iarv64_alloc(int segs, const char* token) {
  void* ptr = 0;
  long long rc, reason;
  struct iarv64parm parm __attribute__((__aligned__(16)));
  memset(&parm, 0, sizeof(parm));
  parm.xversion = 5;
  parm.xrequest = 1;
  parm.xcond_yes = 1;
  parm.xsegments = segs;
  parm.xorigin = 0;
  parm.xdumppriority = 99;
  parm.xtype_pageable = 1;
  parm.xdump = 32;
  parm.xsadmp_no = 1;
  parm.xpageframesize_pageable1meg = 1;
  parm.xuse2gto64g_yes = 1;
  parm.xexecutable_yes = 1;
  parm.keyused_ttoken = 1;
  memcpy(&parm.xttoken, token, 16);
  rc = __iarv64(&parm, &ptr, &reason);
  if (mem_account())
    dprintf(2,
            "__iarv64_alloc: pid %d tid %d ptr=%p size=%lu rc=%lld\n",
            getpid(),
            (int)(pthread_self().__ & 0x7fffffff),
            parm.xorigin,
            (unsigned long)(segs * 1024 * 1024),
            rc);
  if (rc == 0) {
    ptr = parm.xorigin;
  }
  return ptr;
}

#define __USE_IARV64 1
static int __iarv64_free(void* ptr, const char* token) {
  long long rc, reason;
  void* org = ptr;
  struct iarv64parm parm __attribute__((__aligned__(16)));
  memset(&parm, 0, sizeof(parm));
  parm.xversion = 5;
  parm.xrequest = 3;
  parm.xcond_yes = 1;
  parm.xsadmp_no = 1;
  parm.xmemobjstart = ptr;
  parm.keyused_ttoken = 1;
  memcpy(&parm.xttoken, token, 16);
  rc = __iarv64(&parm, &ptr, &reason);
  if (mem_account())
    dprintf(2,
            "__iarv64_free pid %d tid %d ptr=%p rc=%lld\n",
            getpid(),
            (int)(pthread_self().__ & 0x7fffffff),
            org,
            rc);
  return rc;
}

static void* __mo_alloc(int segs) {
  __mopl_t moparm;
  void* p = 0;
  memset(&moparm, 0, sizeof(moparm));
  moparm.__mopldumppriority = __MO_DUMP_PRIORITY_STACK + 5;
  moparm.__moplrequestsize = segs;
  moparm.__moplgetstorflags = __MOPL_PAGEFRAMESIZE_PAGEABLE1MEG;
  int rc = __moservices(__MO_GETSTOR, sizeof(moparm), &moparm, &p);
  if (rc == 0 && moparm.__mopl_iarv64_rc == 0) {
    return p;
  }
  perror("__moservices GETSTOR");
  return 0;
}

static int __mo_free(void* ptr) {
  int rc = __moservices(__MO_DETACH, 0, NULL, &ptr);
  if (rc) {
    perror("__moservices DETACH");
  }
  return rc;
}

typedef unsigned long value_type;
typedef unsigned long key_type;

struct __hash_func {
  size_t operator()(const key_type& k) const {
    int s = 0;
    key_type n = k;
    while (0 == (n & 1) && s < (sizeof(key_type) - 1)) {
      n = n >> 1;
      ++s;
    }
    return s + (n * 0x744dcf5364d7d667UL);
  }
};

typedef std::unordered_map<key_type, value_type, __hash_func>::const_iterator
    mem_cursor_t;

class __Cache {
  std::unordered_map<key_type, value_type, __hash_func> cache;
  std::mutex access_lock;
  char tcbtoken[16];
  unsigned short asid;
  int oktouse;

 public:
  __Cache() {
#if defined(__USE_IARV64)
    gettcbtoken(tcbtoken, 3);
    asid = ((unsigned short*)(*(char* __ptr32*)(0x224)))[18];
#endif
    oktouse =
        (*(int*)(80 + ((char**** __ptr32*)1208)[0][11][1][123]) > 0x040202FF);
    // LE level is 230 or above
  }
  void addptr(const void* ptr, size_t v) {
    unsigned long k = (unsigned long)ptr;
    std::lock_guard<std::mutex> guard(access_lock);
    cache[k] = v;
    if (mem_account()) dprintf(2, "ADDED: @%lx size %lu\n", k, v);
  }
  // normal case:  bool elligible() { return oktouse; }
  bool elligible() { return true; }  // always true for now
#if defined(__USE_IARV64)
  void* alloc_seg(int segs) {
    std::lock_guard<std::mutex> guard(access_lock);
    unsigned short this_asid =
        ((unsigned short*)(*(char* __ptr32*)(0x224)))[18];
    if (asid != this_asid) {
      // a fork occurred
      asid = this_asid;
      gettcbtoken(tcbtoken, 3);
    }
    void* p = __iarv64_alloc(segs, tcbtoken);
    if (p) {
      unsigned long k = (unsigned long)p;
      cache[k] = segs * 1024 * 1024;
      if (mem_account())
        dprintf(2,
                "ADDED:@%lx size %lu RMODE64\n",
                k,
                (size_t)(segs * 1024 * 1024));
    }
    return p;
  }
  int free_seg(void* ptr) {
    unsigned long k = (unsigned long)ptr;
    std::lock_guard<std::mutex> guard(access_lock);
    unsigned short this_asid =
        ((unsigned short*)(*(char* __ptr32*)(0x224)))[18];
    if (asid != this_asid) {
      // a fork occurred
      asid = this_asid;
      gettcbtoken(tcbtoken, 3);
    }
    int rc = __iarv64_free(ptr, tcbtoken);
    if (rc == 0) {
      mem_cursor_t c = cache.find(k);
      if (c != cache.end()) {
        cache.erase(c);
      }
    }
    return rc;
  }
#else
  void* alloc_seg(int segs) {
    void* p = __mo_alloc(segs);
    std::lock_guard<std::mutex> guard(access_lock);
    if (p) {
      unsigned long k = (unsigned long)p;
      cache[k] = segs * 1024 * 1024;
      if (mem_account())
        dprintf(2,
                "ADDED:@%lx size %lu RMODE64\n",
                k,
                (size_t)(segs * 1024 * 1024));
    }
    return p;
  }
  int free_seg(void* ptr) {
    unsigned long k = (unsigned long)ptr;
    int rc = __mo_free(ptr);
    std::lock_guard<std::mutex> guard(access_lock);
    if (rc == 0) {
      mem_cursor_t c = cache.find(k);
      if (c != cache.end()) {
        cache.erase(c);
      }
    }
    return rc;
  }
#endif
  int is_exist_ptr(const void* ptr) {
    unsigned long k = (unsigned long)ptr;
    std::lock_guard<std::mutex> guard(access_lock);
    mem_cursor_t c = cache.find(k);
    if (c != cache.end()) {
      return 1;
    }
    return 0;
  }
  int is_rmode64(const void* ptr) {
    unsigned long k = (unsigned long)ptr;
    std::lock_guard<std::mutex> guard(access_lock);
    mem_cursor_t c = cache.find(k);
    if (c != cache.end()) {
      if (0 != (k & 0xffffffff80000000UL))
        return 1;
      else
        return 0;
    }
    return 0;
  }
  void show(void) {
    std::lock_guard<std::mutex> guard(access_lock);
    if (mem_account())
      for (mem_cursor_t it = cache.begin(); it != cache.end(); ++it) {
        dprintf(2, "LIST: @%lx size %lu\n", it->first, it->second);
      }
  }
  void freeptr(const void* ptr) {
    unsigned long k = (unsigned long)ptr;
    std::lock_guard<std::mutex> guard(access_lock);
    mem_cursor_t c = cache.find(k);
    if (c != cache.end()) {
      cache.erase(c);
    }
  }
  ~__Cache() {
    std::lock_guard<std::mutex> guard(access_lock);
    if (mem_account())
      for (mem_cursor_t it = cache.begin(); it != cache.end(); ++it) {
        dprintf(2,
                "Error: DEBRIS (allocated but never free'd): @%lx size %lu\n",
                it->first,
                it->second);
      }
  }
};

static __Cache alloc_info;

static void* anon_mmap_inner(void* addr, size_t len) {
  int retcode;
  if (alloc_info.elligible() && len % kMegaByte == 0) {
    size_t request_size = len / kMegaByte;
    void* p = alloc_info.alloc_seg(request_size);
    if (p)
      return p;
    else
      return MAP_FAILED;
  } else {
    char* p;
#if defined(__64BIT__)
    __asm(" SYSSTATE ARCHLVL=2,AMODE64=YES\n"
          " STORAGE OBTAIN,LENGTH=(%2),BNDRY=PAGE,COND=YES,ADDR=(%0),RTCD=(%1),"
          "LOC=(31,64)\n"
#if defined(__clang__)
          : "=NR:r1"(p), "=NR:r15"(retcode)
          : "NR:r0"(len)
          : "r0", "r1", "r14", "r15");
#else
          : "=r"(p), "=r"(retcode)
          : "r"(len)
          : "r0", "r1", "r14", "r15");
#endif
#else
    __asm(" SYSSTATE ARCHLVL=2\n"
          " STORAGE "
          "OBTAIN,LENGTH=(%2),BNDRY=PAGE,COND=YES,ADDR=(%0),RTCD=(%1)\n"
#if defined(__clang__)
          : "=NR:r1"(p), "=NR:r15"(retcode)
          : "NR:r0"(len)
          : "r0", "r1", "r14", "r15");
#else
          : "=r"(p), "=r"(retcode)
          : "r"(len)
          : "r0", "r1", "r14", "r15");
#endif

#endif
    if (retcode == 0) {
      alloc_info.addptr(p, len);
      return p;
    }
    return MAP_FAILED;
  }
}

static int anon_munmap_inner(void* addr, size_t len, bool is_above_bar) {
  int retcode;
  if (is_above_bar) {
    return alloc_info.free_seg(addr);
  } else {
#if defined(__64BIT__)
    __asm(" SYSSTATE ARCHLVL=2,AMODE64=YES\n"
          " STORAGE RELEASE,LENGTH=(%2),ADDR=(%1),RTCD=(%0),COND=YES\n"
#if defined(__clang__)
          : "=NR:r15"(retcode)
          : "NR:r1"(addr), "NR:r0"(len)
          : "r0", "r1", "r14", "r15");
#else
          : "=r"(retcode)
          : "r"(addr), "r"(len)
          : "r0", "r1", "r14", "r15");
#endif
#else
    __asm("SYSSTATE ARCHLVL=2"
          "STORAGE RELEASE,LENGTH=(%2),ADDR=(%1),RTCD=(%0),COND=YES"
#if defined(__clang__)
          : "=NR:r15"(retcode)
          : "NR:r1"(addr), "NR:r0"(len)
          : "r0", "r1", "r14", "r15");
#else
          : "=r"(retcode)
          : "r"(addr), "r"(len)
          : "r0", "r1", "r14", "r15");
#endif

#endif
    if (0 == retcode) alloc_info.freeptr(addr);
  }
  return retcode;
}

extern "C" void* anon_mmap(void* _, size_t len) {
  void* ret = anon_mmap_inner(_, len);
  if (ret == MAP_FAILED) {
    if (mem_account())
      dprintf(2, "Error: anon_mmap request size %zu failed\n", len);
    return ret;
  }
  return ret;
}

extern "C" int anon_munmap(void* addr, size_t len) {
  if (alloc_info.is_exist_ptr(addr)) {
    if (mem_account())
      dprintf(
          2, "Address found, attempt to free @%p size %d\n", addr, (int)len);
    int rc = anon_munmap_inner(addr, len, alloc_info.is_rmode64(addr));
    if (rc != 0) {
      if (mem_account())
        dprintf(2, "Error: anon_munmap @%p size %zu failed\n", addr, len);
      return rc;
    }
    return 0;
  } else {
    if (mem_account())
      dprintf(2,
              "Error: attempt to free %p size %d (not allocated)\n",
              addr,
              (int)len);
    return 0;
  }
}

extern "C" int execvpe(const char* name,
                       char* const argv[],
                       char* const envp[]) {
  int lp, ln;
  const char* p;

  int eacces = 0, etxtbsy = 0;
  char *bp, *cur, *path, *buf = 0;

  // Absolute or Relative Path Name
  if (strchr(name, '/')) {
    return execve(name, argv, envp);
  }

  // Get the path we're searching
  if (!(path = getenv("PATH"))) {
    if ((cur = path = (char*)alloca(2)) != NULL) {
      path[0] = ':';
      path[1] = '\0';
    }
  } else {
    char* n = (char*)alloca(strlen(path) + 1);
    strcpy(n, path);
    cur = path = n;
  }

  if (path == NULL ||
      (bp = buf = (char*)alloca(strlen(path) + strlen(name) + 2)) == NULL)
    goto done;

  while (cur != NULL) {
    p = cur;
    if ((cur = strchr(cur, ':')) != NULL) *cur++ = '\0';

    if (!*p) {
      p = ".";
      lp = 1;
    } else
      lp = strlen(p);
    ln = strlen(name);

    memcpy(buf, p, lp);
    buf[lp] = '/';
    memcpy(buf + lp + 1, name, ln);
    buf[lp + ln + 1] = '\0';

  retry:
    (void)execve(bp, argv, envp);
    switch (errno) {
      case EACCES:
        eacces = 1;
        break;
      case ENOTDIR:
      case ENOENT:
        break;
      case ENOEXEC: {
        size_t cnt;
        char** ap;

        for (cnt = 0, ap = (char**)argv; *ap; ++ap, ++cnt)
          ;
        if ((ap = (char**)alloca((cnt + 2) * sizeof(char*))) != NULL) {
          memcpy(ap + 2, argv + 1, cnt * sizeof(char*));

          ap[0] = (char*)"sh";
          ap[1] = bp;
          (void)execve("/bin/sh", ap, envp);
        }
        goto done;
      }
      case ETXTBSY:
        if (etxtbsy < 3) (void)sleep(++etxtbsy);
        goto retry;
      default:
        goto done;
    }
  }
  if (eacces)
    errno = EACCES;
  else if (!errno)
    errno = ENOENT;
done:
  return (-1);
}
//------------------------------------------accounting for memory allocation end
//--tls simulation begin
extern "C" {
static void _cleanup(void* p) {
  pthread_key_t key = *((pthread_key_t*)p);
  free(p);
  pthread_setspecific(key, 0);
}

static void* __tlsPtrAlloc(size_t sz,
                           pthread_key_t* k,
                           pthread_once_t* o,
                           const void* initvalue) {
  unsigned int initv = 0;
  unsigned int expv;
  unsigned int newv = 1;
  expv = 0;
  newv = 1;
  __asm(" cs  %0,%2,%1 \n" : "+r"(expv), "+m"(*o) : "r"(newv) :);
  initv = expv;
  if (initv == 2) {
    // proceed
  } else if (initv == 0) {
    // create
    pthread_key_create(k, _cleanup);
    expv = 1;
    newv = 2;
    __asm(" cs  %0,%2,%1 \n" : "+r"(expv), "+m"(*o) : "r"(newv) :);
    initv = expv;
  } else {
    // wait and poll for completion
    while (initv != 2) {
      expv = 0;
      newv = 1;
      __asm(" la 15,0\n"
            " svc 137\n"
            " cs  %0,%2,%1 \n"
            : "+r"(expv), "+m"(*o)
            : "r"(newv)
            : "r15");
      initv = expv;
    }
  }
  void* p = pthread_getspecific(*k);
  if (!p) {
    // first call in thread allocate
    p = malloc(sz + sizeof(pthread_key_t));
    memcpy(p, k, sizeof(pthread_key_t));
    pthread_setspecific(*k, p);
    memcpy((char*)p + sizeof(pthread_key_t), initvalue, sz);
  }
  return (char*)p + sizeof(pthread_key_t);
}
static void* __tlsPtr(pthread_key_t* key) {
  return pthread_getspecific(*key);
}
static void __tlsDelete(pthread_key_t* key) {
  pthread_key_delete(*key);
}

struct __tlsanchor {
  pthread_once_t once;
  pthread_key_t key;
  size_t sz;
};
extern struct __tlsanchor* __tlsvaranchor_create(size_t sz) {
  struct __tlsanchor* a =
      (struct __tlsanchor*)calloc(1, sizeof(struct __tlsanchor));
  a->once = PTHREAD_ONCE_INIT;
  a->sz = sz;
  return a;
}
extern void __tlsvaranchor_destroy(struct __tlsanchor* anchor) {
  pthread_key_delete(anchor->key);
  free(anchor);
}

extern void* __tlsPtrFromAnchor(struct __tlsanchor* anchor,
                                const void* initvalue) {
  return __tlsPtrAlloc(anchor->sz, &(anchor->key), &(anchor->once), initvalue);
}
}
//--tls simulation end

// --- start __atomic_store
#define CSG(_op1, _op2, _op3)                                                  \
  __asm(" csg %0,%2,%1 \n " : "+r"(_op1), "+m"(_op2) : "r"(_op3) :)

#define CS(_op1, _op2, _op3)                                                   \
  __asm(" cs %0,%2,%1 \n " : "+r"(_op1), "+m"(_op2) : "r"(_op3) :)

extern "C" void __atomic_store_real(int size,
                                    void* ptr,
                                    void* val,
                                    int memorder) asm("__atomic_store");
void __atomic_store_real(int size, void* ptr, void* val, int memorder) {
  if (size == 4) {
    unsigned int new_val = *(unsigned int*)val;
    unsigned int* stor = (unsigned int*)ptr;
    unsigned int org;
    unsigned int old_val;
    do {
      org = *(unsigned int*)ptr;
      old_val = org;
      CS(old_val, *stor, new_val);
    } while (old_val != org);
  } else if (size == 8) {
    unsigned long new_val = *(unsigned long*)val;
    unsigned long* stor = (unsigned long*)ptr;
    unsigned long org;
    unsigned long old_val;
    do {
      org = *(unsigned long*)ptr;
      old_val = org;
      CSG(old_val, *stor, new_val);
    } while (old_val != org);
  } else if (0x40 & *(const char*)209) {
    long cc;
    int retry = 10000;
    while (retry--) {
      __asm(" TBEGIN 0,X'FF00'\n"
            " IPM      %0\n"
            " LLGTR    %0,%0\n"
            " SRLG     %0,%0,28\n"
            : "=r"(cc)::);
      if (0 == cc) {
        memcpy(ptr, val, size);
        __asm(" TEND\n"
              " IPM      %0\n"
              " LLGTR    %0,%0\n"
              " SRLG     %0,%0,28\n"
              : "=r"(cc)::);
        if (0 == cc) break;
      }
    }
    if (retry < 1) {
      dprintf(2,
              "%s:%s:%d size=%d target=%p source=%p store failed\n",
              __FILE__,
              __FUNCTION__,
              __LINE__,
              size,
              ptr,
              val);
      abort();
    }
  } else {
    dprintf(2,
            "%s:%s:%d size=%d target=%p source=%p not implimented\n",
            __FILE__,
            __FUNCTION__,
            __LINE__,
            size,
            ptr,
            val);
    abort();
  }
}
// --- end __atomic_store
