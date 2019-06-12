#ifndef __ZOS_H_
#define __ZOS_H_
//-------------------------------------------------------------------------------------
//
// external interface:
//
#include <_Nascii.h>
#include <stdarg.h>
#define __ZOS_CC
#ifdef __cplusplus
extern "C" {
#endif
#ifndef __size_t
typedef unsigned long size_t;
#define __size_t 1
#endif

extern void *_convert_e2a(void *dst, const void *src, size_t size);
extern void *_convert_a2e(void *dst, const void *src, size_t size);
extern char **__get_environ_np(void);
extern void __xfer_env(void);
extern int __chgfdccsid(int fd, unsigned short ccsid);
extern size_t __e2a_l(char *bufptr, size_t szLen);
extern size_t __a2e_l(char *bufptr, size_t szLen);
extern size_t __e2a_s(char *string);
extern size_t __a2e_s(char *string);
extern int dprintf(int fd, const char *, ...);
extern int vdprintf(int fd, const char *, va_list ap);
extern void __xfer_env(void);
extern int __chgfdccsid(int fd, unsigned short ccsid);
extern void __dump(int fd, const void *addr, size_t len, size_t bw);
extern void __dump_title(int fd, const void *addr, size_t len, size_t bw,
                         const char *, ...);
extern int backtrace(void **buffer, int size);
extern char **backtrace_symbols(void *const *buffer, int size);
extern void backtrace_symbols_fd(void *const *buffer, int size, int fd);
extern void __abend(int comp_code, unsigned reason_code, int flat_byte,
                    void *plist);
extern int strncasecmp_ignorecp(const char *a, const char *b, size_t n);
extern int strcasecmp_ignorecp(const char *a, const char *b);
extern int __guess_ae(const void *src, size_t size);
extern int conv_utf8_utf16(char *, size_t, const char *, size_t);
extern int conv_utf16_utf8(char *, size_t, const char *, size_t);
extern int __console_printf(const char *fmt, ...);
extern int __indebug(void);
extern void __setdebug(int);
extern char **__getargv(void);
extern char **__getargv_a(void);
extern int __getargc(void);

extern void *__dlcb_next(void *last);
extern int __dlcb_entry_name(char *buf, int size, void *dlcb);
extern void *__dlcb_entry_addr(void *dlcb);
extern int __find_file_in_path(char *out, int size, const char *envvar,
                               const char *file);
extern void __listdll(int fd);
extern int __file_needs_conversion(int fd);
extern int __file_needs_conversion_init(const char *name, int fd);
extern void __fd_close(int fd);

#ifdef __cplusplus
}
#endif

#define _str_e2a(_str)                                                         \
  ({                                                                           \
    const char *src = (const char *)(_str);                                    \
    int len = strlen(src) + 1;                                                 \
    char *tgt = (char *)alloca(len);                                           \
    (char *)_convert_e2a(tgt, src, len);                                       \
  })

#define AEWRAP(_rc, _x)                                                        \
  (__isASCII() ? ((_rc) = (_x), 0)                                             \
               : (__ae_thread_swapmode(__AE_ASCII_MODE), ((_rc) = (_x)),       \
                  __ae_thread_swapmode(__AE_EBCDIC_MODE), 1))

#define AEWRAP_VOID(_x)                                                        \
  (__isASCII() ? ((_x), 0)                                                     \
               : (__ae_thread_swapmode(__AE_ASCII_MODE), (_x),                 \
                  __ae_thread_swapmode(__AE_EBCDIC_MODE), 1))

#ifdef __cplusplus
class __auto_ascii {
  int ascii_mode;

public:
  __auto_ascii();
  ~__auto_ascii();
};

#endif

#endif
