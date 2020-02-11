#ifdef __MVS__
#define _AE_BIMODAL
#endif
#include "node.h"
#include "node_buffer.h"
#include "node_constants.h"
#include "node_file.h"
#include "node_http_parser.h"
#include "node_javascript.h"
#include "node_version.h"
#include "node_internals.h"
#include "node_revert.h"
#include "node_watchdog.h"

#if defined HAVE_PERFCTR
#include "node_counters.h"
#endif

#if HAVE_OPENSSL
#include "node_crypto.h"
#endif

#if defined(NODE_HAVE_I18N_SUPPORT)
#include "node_i18n.h"
#endif

#if defined HAVE_DTRACE || defined HAVE_ETW
#include "node_dtrace.h"
#endif

#if defined HAVE_LTTNG
#include "node_lttng.h"
#endif

#include "ares.h"
#include "async-wrap-inl.h"
#include "env.h"
#include "env-inl.h"
#include "handle_wrap.h"
#include "req-wrap-inl.h"
#include "string_bytes.h"
#include "util.h"
#include "uv.h"
#if NODE_USE_V8_PLATFORM
#include "libplatform/libplatform.h"
#endif  // NODE_USE_V8_PLATFORM
#include "v8-debug.h"
#include "v8-profiler.h"
#include "zlib.h"

#ifdef NODE_ENABLE_VTUNE_PROFILING
#include "../deps/v8/src/third_party/vtune/v8-vtune.h"
#endif

#include <errno.h>
#include <fcntl.h>  // _O_RDWR
#include <limits.h>  // PATH_MAX
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <string>
#include <vector>
#include <list>

#if defined(NODE_HAVE_I18N_SUPPORT)
#include <unicode/uvernum.h>
#endif

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

#if defined(_MSC_VER)
#include <direct.h>
#include <io.h>
#define getpid GetCurrentProcessId
#define umask _umask
typedef int mode_t;
#else
#include <pthread.h>
#include <sys/resource.h>  // getrlimit, setrlimit
#include <unistd.h>  // setuid, getuid
#endif

#if defined(__POSIX__) && !defined(__ANDROID__)
#include <pwd.h>  // getpwnam()
#include <grp.h>  // getgrnam()
#endif

#ifdef __APPLE__
#include <crt_externs.h>
#define environ (*_NSGetEnviron())
#elif !defined(_MSC_VER)
extern char **environ;
#endif

#ifdef __MVS__
#include <strings.h>
#include <unistd.h> // e2a
#include <setjmp.h> // e2a
#include <sys/__getipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#endif


namespace node {

using v8::Array;
using v8::ArrayBuffer;
using v8::Boolean;
using v8::Context;
using v8::EscapableHandleScope;
using v8::Exception;
using v8::Float64Array;
using v8::Function;
using v8::FunctionCallbackInfo;
using v8::FunctionTemplate;
using v8::HandleScope;
using v8::HeapStatistics;
using v8::Integer;
using v8::Isolate;
using v8::Local;
using v8::Locker;
using v8::MaybeLocal;
using v8::Message;
using v8::Name;
using v8::NamedPropertyHandlerConfiguration;
using v8::Null;
using v8::Number;
using v8::Object;
using v8::ObjectTemplate;
using v8::Promise;
using v8::PromiseRejectMessage;
using v8::PropertyCallbackInfo;
using v8::PropertyHandlerFlags;
using v8::ScriptOrigin;
using v8::SealHandleScope;
using v8::String;
using v8::TryCatch;
using v8::Uint32Array;
using v8::V8;
using v8::Value;

static bool print_eval = false;
static bool force_repl = false;
static bool syntax_check_only = false;
static bool trace_deprecation = false;
static bool throw_deprecation = false;
static bool trace_sync_io = false;
static bool track_heap_objects = false;
static const char* eval_string = nullptr;
static std::vector<std::string> preload_modules;
#if HAVE_INSPECTOR
static bool use_inspector = false;
#else
static const bool use_inspector = false;
#endif
static bool use_debug_agent = false;
static bool debug_wait_connect = false;
static std::string* debug_host;  // coverity[leaked_storage]
static int debug_port = 5858;
static std::string* inspector_host;  // coverity[leaked_storage]
static int inspector_port = 9229;
static const int v8_default_thread_pool_size = 4;
static int v8_thread_pool_size = v8_default_thread_pool_size;
static bool prof_process = false;
static bool v8_is_profiling = false;
static bool node_is_initialized = false;
static node_module* modpending;
static node_module* modlist_builtin;
static node_module* modlist_linked;
static node_module* modlist_addon;

// Bit flag used to track security reverts (see node_revert.h)
unsigned int reverted = 0;

#if defined(NODE_HAVE_I18N_SUPPORT)
// Path to ICU data (for i18n / Intl)
static std::string icu_data_dir;  // NOLINT(runtime/string)
#endif

uint64_t max_http_header_size = 8 * 1024;

// Set in node.cc by ParseArgs when --insecure-http-parser is used.
bool insecure_http_parser = false;

// used by C++ modules as well
bool no_deprecation = false;

#if HAVE_OPENSSL
// use OpenSSL's cert store instead of bundled certs
bool ssl_openssl_cert_store =
#if defined(NODE_OPENSSL_CERT_STORE)
        true;
#else
        false;
#endif

# if NODE_FIPS_MODE
// used by crypto module
bool enable_fips_crypto = false;
bool force_fips_crypto = false;
# endif  // NODE_FIPS_MODE
std::string openssl_config;  // NOLINT(runtime/string)
#endif  // HAVE_OPENSSL

// true if process warnings should be suppressed
bool no_process_warnings = false;
bool trace_warnings = false;

// Set in node.cc by ParseArgs when --preserve-symlinks is used.
// Used in node_config.cc to set a constant on process.binding('config')
// that is used by lib/module.js
bool config_preserve_symlinks = false;

bool v8_initialized = false;

// Set in node.cc by ParseArgs when --expose-internals or --expose_internals is
// used.
// Used in node_config.cc to set a constant on process.binding('config')
// that is used by lib/internal/bootstrap_node.js
bool config_expose_internals = false;

// Set in node.cc by ParseArgs when --redirect-warnings= is used.
std::string config_warning_file;  // NOLINT(runtime/string)

// process-relative uptime base, initialized at start-up
static double prog_start_time;
static bool debugger_running;
static uv_async_t dispatch_debug_messages_async;

static Mutex node_isolate_mutex;
static v8::Isolate* node_isolate;

static struct {
#if NODE_USE_V8_PLATFORM
  void Initialize(int thread_pool_size) {
    platform_ = v8::platform::CreateDefaultPlatform(thread_pool_size);
    V8::InitializePlatform(platform_);
  }

  void PumpMessageLoop(Isolate* isolate) {
    v8::platform::PumpMessageLoop(platform_, isolate);
  }

  void Dispose() {
    delete platform_;
    platform_ = nullptr;
  }

  bool StartInspector(Environment *env, const char* script_path,
                      const char* host, int port, bool wait) {
#if HAVE_INSPECTOR
    return env->inspector_agent()->Start(platform_, script_path,
                                         host, port, wait);
#else
    return true;
#endif  // HAVE_INSPECTOR
  }

  v8::Platform* platform_;
#else  // !NODE_USE_V8_PLATFORM
  void Initialize(int thread_pool_size) {}
  void PumpMessageLoop(Isolate* isolate) {}
  void Dispose() {}
  bool StartInspector(Environment *env, const char* script_path,
                      const char* host, int port, bool wait) {
    env->ThrowError("\x4e\x6f\x64\x65\x20\x63\x6f\x6d\x70\x69\x6c\x65\x64\x20\x77\x69\x74\x68\x20\x4e\x4f\x44\x45\x5f\x55\x53\x45\x5f\x56\x38\x5f\x50\x4c\x41\x54\x46\x4f\x52\x4d\x3d\x30");
    return false;  // make compiler happy
  }
#endif  // !NODE_USE_V8_PLATFORM
} v8_platform;

#ifdef __POSIX__
static uv_sem_t debug_semaphore;
static bool debug_semaphore_active = false;
static void StopDebugSignalHandler();
static const unsigned kMaxSignal = 32;
#endif

#ifdef __MVS__
static uv_thread_t signalHandlerThread;
static int signalHandlerExit;
class OS390ThreadManager {
public:
  static void Destroy(int sig) {
    uv_queue_work(NULL, NULL, NULL, NULL);
    StopDebugSignalHandler();
    SigintWatchdogHelper::GetInstance()->StopThread();
    msgctl(uv_backend_fd(uv_default_loop()), IPC_RMID, NULL);
  }
  ~OS390ThreadManager() {
    Destroy(0);
  }
};
#endif

static void PrintString(FILE* out, const char* format, va_list ap) {
#ifdef _WIN32
  HANDLE stderr_handle = GetStdHandle(STD_ERROR_HANDLE);

  // Check if stderr is something other than a tty/console
  if (stderr_handle == INVALID_HANDLE_VALUE ||
      stderr_handle == nullptr ||
      uv_guess_handle(_fileno(stderr)) != UV_TTY) {
    vfprintf(out, format, ap);
    va_end(ap);
    return;
  }

  // Fill in any placeholders
  int n = _vscprintf(format, ap);
  std::vector<char> out(n + 1);
  vsprintf(out.data(), format, ap);

  // Get required wide buffer size
  n = MultiByteToWideChar(CP_UTF8, 0, out.data(), -1, nullptr, 0);

  std::vector<wchar_t> wbuf(n);
  MultiByteToWideChar(CP_UTF8, 0, out.data(), -1, wbuf.data(), n);

  // Don't include the null character in the output
  CHECK_GT(n, 0);
  WriteConsoleW(stderr_handle, wbuf.data(), n - 1, nullptr, nullptr);
#elif defined(__MVS__)
  int size = __vsnprintf_a(NULL, 0, format, ap);
  char buf[size+1];
  __vsnprintf_a(buf, size + 1, format, ap);
  __a2e_s(buf);
  fprintf(out, "%s", buf);
#else
  vfprintf(out, format, ap);
#endif
}

static void PrintOutString(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  PrintString(stdout, format, ap);
  va_end(ap);
}

static void PrintErrorString(const char* format, ...) {
  va_list ap;
  va_start(ap, format);
  PrintString(stderr, format, ap);
  va_end(ap);
}

static void CheckImmediate(uv_check_t* handle) {
  Environment* env = Environment::from_immediate_check_handle(handle);
  HandleScope scope(env->isolate());
  Context::Scope context_scope(env->context());
  MakeCallback(env, env->process_object(), env->immediate_callback_string());
}


static void IdleImmediateDummy(uv_idle_t* handle) {
  // Do nothing. Only for maintaining event loop.
  // TODO(bnoordhuis) Maybe make libuv accept nullptr idle callbacks.
}

inline const uint8_t& Ascii2Ebcdic(const char letter) {
  static unsigned char a2e[256] = {
  0,1,2,3,55,45,46,47,22,5,21,11,12,13,14,15,
  16,17,18,19,60,61,50,38,24,25,63,39,28,29,30,31,
  64,79,127,123,91,108,80,125,77,93,92,78,107,96,75,97,
  240,241,242,243,244,245,246,247,248,249,122,94,76,126,110,111,
  124,193,194,195,196,197,198,199,200,201,209,210,211,212,213,214,
  215,216,217,226,227,228,229,230,231,232,233,74,224,90,95,109,
  121,129,130,131,132,133,134,135,136,137,145,146,147,148,149,150,
  151,152,153,162,163,164,165,166,167,168,169,192,106,208,161,7,
  32,33,34,35,36,21,6,23,40,41,42,43,44,9,10,27,
  48,49,26,51,52,53,54,8,56,57,58,59,4,20,62,225,
  65,66,67,68,69,70,71,72,73,81,82,83,84,85,86,87,
  88,89,98,99,100,101,102,103,104,105,112,113,114,115,116,117,
  118,119,120,128,138,139,140,141,142,143,144,154,155,156,157,158,
  159,160,170,171,172,173,174,175,176,177,178,179,180,181,182,183,
  184,185,186,187,188,189,190,191,202,203,204,205,206,207,218,219,
  220,221,222,223,234,235,236,237,238,239,250,251,252,253,254,255
  };
  return a2e[letter];
}


inline int GetFirstFlagFrom(const char* format_e, int start = 0) {
  int flag_pos = start;
  for (; format_e[flag_pos] != '\x0' && format_e[flag_pos] != '\x25'; flag_pos++); // find the first flag
  return flag_pos;
}


#ifdef __MVS__
int VSNPrintFASCII(char* out, int length, const char* format_a, ...) {
  va_list args;
  va_start(args, format_a);

  int bytes_written = 0, bytes_remain = length;
  size_t format_len = strlen(format_a);
  char buffer_e[format_len + 1];
  char * format_e = buffer_e;
  memcpy(format_e, format_a, format_len + 1);
  __a2e_s(format_e);
  int first_flag = GetFirstFlagFrom(format_e);
  if (first_flag > 0) {
    int size = snprintf(out, length, "%.*s", first_flag, format_e);
    CHECK(size >= 0);
    bytes_written += size;
    bytes_remain = length - bytes_written;
  }
  format_e += first_flag;
  if (format_e[0] == '\x0') return bytes_written;

  do {
    int next_flag = GetFirstFlagFrom(format_e, 2);
    char tmp = format_e[next_flag];
    int ret = 0;
    format_e[next_flag] = '\x0';
    char flag = format_e[1];
    if (flag == '\x73') {
      // convert arg
      char * str = va_arg(args, char *);
      size_t str_len = strlen(str);
      char str_e[str_len + 1];
      memcpy(str_e, str, str_len + 1);
      __a2e_s(str_e);
      ret = snprintf(out + bytes_written, bytes_remain, format_e, str_e);
    } else if (flag == '\x63') {
      ret = snprintf(out + bytes_written, bytes_remain, format_e, Ascii2Ebcdic(va_arg(args, char)));
    } else {
      ret = snprintf(out + bytes_written, bytes_remain, format_e, args);
    }
    CHECK(ret >= 0);
    bytes_written += ret;
    bytes_remain = length - bytes_written;
    format_e[next_flag] = tmp;
    format_e += next_flag;
    bytes_remain = length - bytes_written;
  } while (format_e[0] != '\x0' || bytes_remain <= 0);

  __e2a_s(out);
  return bytes_written;
}


int SNPrintFASCII(char * out, int length, const char* format_a, ...) {
  va_list args;
  va_start(args, format_a);
  int ret = VSNPrintFASCII(out, length, format_a, args);
  va_end(args);
  return ret;
}
#endif

static inline const char *errno_string(int errorno) {
#define ERRNO_CASE(e)  case e: return USTR(#e;)
  switch (errorno) {
#ifdef EACCES
  ERRNO_CASE(EACCES);
#endif

#ifdef EADDRINUSE
  ERRNO_CASE(EADDRINUSE);
#endif

#ifdef EADDRNOTAVAIL
  ERRNO_CASE(EADDRNOTAVAIL);
#endif

#ifdef EAFNOSUPPORT
  ERRNO_CASE(EAFNOSUPPORT);
#endif

#ifdef EAGAIN
  ERRNO_CASE(EAGAIN);
#endif

#ifdef EWOULDBLOCK
# if EAGAIN != EWOULDBLOCK
  ERRNO_CASE(EWOULDBLOCK);
# endif
#endif

#ifdef EALREADY
  ERRNO_CASE(EALREADY);
#endif

#ifdef EBADF
  ERRNO_CASE(EBADF);
#endif

#ifdef EBADMSG
  ERRNO_CASE(EBADMSG);
#endif

#ifdef EBUSY
  ERRNO_CASE(EBUSY);
#endif

#ifdef ECANCELED
  ERRNO_CASE(ECANCELED);
#endif

#ifdef ECHILD
  ERRNO_CASE(ECHILD);
#endif

#ifdef ECONNABORTED
  ERRNO_CASE(ECONNABORTED);
#endif

#ifdef ECONNREFUSED
  ERRNO_CASE(ECONNREFUSED);
#endif

#ifdef ECONNRESET
  ERRNO_CASE(ECONNRESET);
#endif

#ifdef EDEADLK
  ERRNO_CASE(EDEADLK);
#endif

#ifdef EDESTADDRREQ
  ERRNO_CASE(EDESTADDRREQ);
#endif

#ifdef EDOM
  ERRNO_CASE(EDOM);
#endif

#ifdef EDQUOT
  ERRNO_CASE(EDQUOT);
#endif

#ifdef EEXIST
  ERRNO_CASE(EEXIST);
#endif

#ifdef EFAULT
  ERRNO_CASE(EFAULT);
#endif

#ifdef EFBIG
  ERRNO_CASE(EFBIG);
#endif

#ifdef EHOSTUNREACH
  ERRNO_CASE(EHOSTUNREACH);
#endif

#ifdef EIDRM
  ERRNO_CASE(EIDRM);
#endif

#ifdef EILSEQ
  ERRNO_CASE(EILSEQ);
#endif

#ifdef EINPROGRESS
  ERRNO_CASE(EINPROGRESS);
#endif

#ifdef EINTR
  ERRNO_CASE(EINTR);
#endif

#ifdef EINVAL
  ERRNO_CASE(EINVAL);
#endif

#ifdef EIO
  ERRNO_CASE(EIO);
#endif

#ifdef EISCONN
  ERRNO_CASE(EISCONN);
#endif

#ifdef EISDIR
  ERRNO_CASE(EISDIR);
#endif

#ifdef ELOOP
  ERRNO_CASE(ELOOP);
#endif

#ifdef EMFILE
  ERRNO_CASE(EMFILE);
#endif

#ifdef EMLINK
  ERRNO_CASE(EMLINK);
#endif

#ifdef EMSGSIZE
  ERRNO_CASE(EMSGSIZE);
#endif

#ifdef EMULTIHOP
  ERRNO_CASE(EMULTIHOP);
#endif

#ifdef ENAMETOOLONG
  ERRNO_CASE(ENAMETOOLONG);
#endif

#ifdef ENETDOWN
  ERRNO_CASE(ENETDOWN);
#endif

#ifdef ENETRESET
  ERRNO_CASE(ENETRESET);
#endif

#ifdef ENETUNREACH
  ERRNO_CASE(ENETUNREACH);
#endif

#ifdef ENFILE
  ERRNO_CASE(ENFILE);
#endif

#ifdef ENOBUFS
  ERRNO_CASE(ENOBUFS);
#endif

#ifdef ENODATA
  ERRNO_CASE(ENODATA);
#endif

#ifdef ENODEV
  ERRNO_CASE(ENODEV);
#endif

#ifdef ENOENT
  ERRNO_CASE(ENOENT);
#endif

#ifdef ENOEXEC
  ERRNO_CASE(ENOEXEC);
#endif

#ifdef ENOLINK
  ERRNO_CASE(ENOLINK);
#endif

#ifdef ENOLCK
# if ENOLINK != ENOLCK
  ERRNO_CASE(ENOLCK);
# endif
#endif

#ifdef ENOMEM
  ERRNO_CASE(ENOMEM);
#endif

#ifdef ENOMSG
  ERRNO_CASE(ENOMSG);
#endif

#ifdef ENOPROTOOPT
  ERRNO_CASE(ENOPROTOOPT);
#endif

#ifdef ENOSPC
  ERRNO_CASE(ENOSPC);
#endif

#ifdef ENOSR
  ERRNO_CASE(ENOSR);
#endif

#ifdef ENOSTR
  ERRNO_CASE(ENOSTR);
#endif

#ifdef ENOSYS
  ERRNO_CASE(ENOSYS);
#endif

#ifdef ENOTCONN
  ERRNO_CASE(ENOTCONN);
#endif

#ifdef ENOTDIR
  ERRNO_CASE(ENOTDIR);
#endif

#ifdef ENOTEMPTY
# if ENOTEMPTY != EEXIST
  ERRNO_CASE(ENOTEMPTY);
# endif
#endif

#ifdef ENOTSOCK
  ERRNO_CASE(ENOTSOCK);
#endif

#ifdef ENOTSUP
  ERRNO_CASE(ENOTSUP);
#else
# ifdef EOPNOTSUPP
  ERRNO_CASE(EOPNOTSUPP);
# endif
#endif

#ifdef ENOTTY
  ERRNO_CASE(ENOTTY);
#endif

#ifdef ENXIO
  ERRNO_CASE(ENXIO);
#endif


#ifdef EOVERFLOW
  ERRNO_CASE(EOVERFLOW);
#endif

#ifdef EPERM
  ERRNO_CASE(EPERM);
#endif

#ifdef EPIPE
  ERRNO_CASE(EPIPE);
#endif

#ifdef EPROTO
  ERRNO_CASE(EPROTO);
#endif

#ifdef EPROTONOSUPPORT
  ERRNO_CASE(EPROTONOSUPPORT);
#endif

#ifdef EPROTOTYPE
  ERRNO_CASE(EPROTOTYPE);
#endif

#ifdef ERANGE
  ERRNO_CASE(ERANGE);
#endif

#ifdef EROFS
  ERRNO_CASE(EROFS);
#endif

#ifdef ESPIPE
  ERRNO_CASE(ESPIPE);
#endif

#ifdef ESRCH
  ERRNO_CASE(ESRCH);
#endif

#ifdef ESTALE
  ERRNO_CASE(ESTALE);
#endif

#ifdef ETIME
  ERRNO_CASE(ETIME);
#endif

#ifdef ETIMEDOUT
  ERRNO_CASE(ETIMEDOUT);
#endif

#ifdef ETXTBSY
  ERRNO_CASE(ETXTBSY);
#endif

#ifdef EXDEV
  ERRNO_CASE(EXDEV);
#endif

  default: return "";
  }
}

const char *signo_string(int signo) {
#define SIGNO_CASE(e)  case e: return USTR(#e;)
  switch (signo) {
#ifdef SIGHUP
  SIGNO_CASE(SIGHUP);
#endif

#ifdef SIGINT
  SIGNO_CASE(SIGINT);
#endif

#ifdef SIGQUIT
  SIGNO_CASE(SIGQUIT);
#endif

#ifdef SIGILL
  SIGNO_CASE(SIGILL);
#endif

#ifdef SIGTRAP
  SIGNO_CASE(SIGTRAP);
#endif

#ifdef SIGABRT
  SIGNO_CASE(SIGABRT);
#endif

#ifdef SIGIOT
# if SIGABRT != SIGIOT
  SIGNO_CASE(SIGIOT);
# endif
#endif

#ifdef SIGBUS
  SIGNO_CASE(SIGBUS);
#endif

#ifdef SIGFPE
  SIGNO_CASE(SIGFPE);
#endif

#ifdef SIGKILL
  SIGNO_CASE(SIGKILL);
#endif

#ifdef SIGUSR1
  SIGNO_CASE(SIGUSR1);
#endif

#ifdef SIGSEGV
  SIGNO_CASE(SIGSEGV);
#endif

#ifdef SIGUSR2
  SIGNO_CASE(SIGUSR2);
#endif

#ifdef SIGPIPE
  SIGNO_CASE(SIGPIPE);
#endif

#ifdef SIGALRM
  SIGNO_CASE(SIGALRM);
#endif

  SIGNO_CASE(SIGTERM);

#ifdef SIGCHLD
  SIGNO_CASE(SIGCHLD);
#endif

#ifdef SIGSTKFLT
  SIGNO_CASE(SIGSTKFLT);
#endif


#ifdef SIGCONT
  SIGNO_CASE(SIGCONT);
#endif

#ifdef SIGSTOP
  SIGNO_CASE(SIGSTOP);
#endif

#ifdef SIGTSTP
  SIGNO_CASE(SIGTSTP);
#endif

#ifdef SIGBREAK
  SIGNO_CASE(SIGBREAK);
#endif

#ifdef SIGTTIN
  SIGNO_CASE(SIGTTIN);
#endif

#ifdef SIGTTOU
  SIGNO_CASE(SIGTTOU);
#endif

#ifdef SIGURG
  SIGNO_CASE(SIGURG);
#endif

#ifdef SIGXCPU
  SIGNO_CASE(SIGXCPU);
#endif

#ifdef SIGXFSZ
  SIGNO_CASE(SIGXFSZ);
#endif

#ifdef SIGVTALRM
  SIGNO_CASE(SIGVTALRM);
#endif

#ifdef SIGPROF
  SIGNO_CASE(SIGPROF);
#endif

#ifdef SIGWINCH
  SIGNO_CASE(SIGWINCH);
#endif

#ifdef SIGIO
  SIGNO_CASE(SIGIO);
#endif

#ifdef SIGPOLL
# if SIGPOLL != SIGIO
  SIGNO_CASE(SIGPOLL);
# endif
#endif

#ifdef SIGLOST
# if SIGLOST != SIGABRT
  SIGNO_CASE(SIGLOST);
# endif
#endif

#ifdef SIGPWR
# if SIGPWR != SIGLOST
  SIGNO_CASE(SIGPWR);
# endif
#endif

#ifdef SIGINFO
# if !defined(SIGPWR) || SIGINFO != SIGPWR
  SIGNO_CASE(SIGINFO);
# endif
#endif

#ifdef SIGSYS
  SIGNO_CASE(SIGSYS);
#endif

  default: return "";
  }
}


// Convenience methods

static void platform_exit(int code) {
#ifndef __MVS__
  exit(code);
#endif

  OS390ThreadManager::Destroy(0);
  exit(code);
}


void ThrowError(v8::Isolate* isolate, const char* errmsg) {
  Environment::GetCurrent(isolate)->ThrowError(errmsg);
}


void ThrowTypeError(v8::Isolate* isolate, const char* errmsg) {
  Environment::GetCurrent(isolate)->ThrowTypeError(errmsg);
}


void ThrowRangeError(v8::Isolate* isolate, const char* errmsg) {
  Environment::GetCurrent(isolate)->ThrowRangeError(errmsg);
}


void ThrowErrnoException(v8::Isolate* isolate,
                         int errorno,
                         const char* syscall,
                         const char* message,
                         const char* path) {
  Environment::GetCurrent(isolate)->ThrowErrnoException(errorno,
                                                        syscall,
                                                        message,
                                                        path);
}


void ThrowUVException(v8::Isolate* isolate,
                      int errorno,
                      const char* syscall,
                      const char* message,
                      const char* path,
                      const char* dest) {
  Environment::GetCurrent(isolate)
      ->ThrowUVException(errorno, syscall, message, path, dest);
}


Local<Value> ErrnoException(Isolate* isolate,
                            int errorno,
                            const char *syscall,
                            const char *msg,
                            const char *path) {
  Environment* env = Environment::GetCurrent(isolate);

  Local<Value> e;
  Local<String> estring = OneByteString(env->isolate(), errno_string(errorno));
  if (msg == nullptr || msg[0] == '\x0') {
    msg = strerror(errorno);
  }
  Local<String> message = OneByteString(env->isolate(), msg);

  Local<String> cons =
      String::Concat(estring, FIXED_ONE_BYTE_STRING(env->isolate(), "\x2c\x20"));
  cons = String::Concat(cons, message);

  Local<String> path_string;
  if (path != nullptr) {
    // FIXME(bnoordhuis) It's questionable to interpret the file path as UTF-8.
    path_string = String::NewFromUtf8(env->isolate(), path);
  }

  if (path_string.IsEmpty() == false) {
    cons = String::Concat(cons, FIXED_ONE_BYTE_STRING(env->isolate(), "\x20\x27"));
    cons = String::Concat(cons, path_string);
    cons = String::Concat(cons, FIXED_ONE_BYTE_STRING(env->isolate(), "\x27"));
  }
  e = Exception::Error(cons);

  Local<Object> obj = e->ToObject(env->isolate());
  obj->Set(env->errno_string(), Integer::New(env->isolate(), errorno));
  obj->Set(env->code_string(), estring);

  if (path_string.IsEmpty() == false) {
    obj->Set(env->path_string(), path_string);
  }

  if (syscall != nullptr) {
    obj->Set(env->syscall_string(), OneByteString(env->isolate(), syscall));
  }

  return e;
}


static Local<String> StringFromPath(Isolate* isolate, const char* path) {
#ifdef _WIN32
  if (strncmp(path, "\\\\?\\UNC\\", 8) == 0) {
    return String::Concat(FIXED_ONE_BYTE_STRING(isolate, "\\\\"),
                          String::NewFromUtf8(isolate, path + 8));
  } else if (strncmp(path, "\\\\?\\", 4) == 0) {
    return String::NewFromUtf8(isolate, path + 4);
  }
#endif

  return String::NewFromUtf8(isolate, *E2A(path));
}


Local<Value> UVException(Isolate* isolate,
                         int errorno,
                         const char* syscall,
                         const char* msg,
                         const char* path) {
  return UVException(isolate, errorno, syscall, msg, path, nullptr);
}


Local<Value> UVException(Isolate* isolate,
                         int errorno,
                         const char* syscall,
                         const char* msg,
                         const char* path,
                         const char* dest) {
  Environment* env = Environment::GetCurrent(isolate);

  if (!msg || !msg[0])
    msg = uv_strerror(errorno);

#ifdef __MVS__
  Local<String> js_code = OneByteString(isolate, *E2A(uv_err_name(errorno)));
#else
  Local<String> js_code = OneByteString(isolate, uv_err_name(errorno));
#endif
  Local<String> js_syscall = OneByteString(isolate, syscall);
  Local<String> js_path;
  Local<String> js_dest;

  Local<String> js_msg = js_code;
  js_msg = String::Concat(js_msg, FIXED_ONE_BYTE_STRING(isolate, "\x3a\x20"));
#ifdef __MVS__
  js_msg = String::Concat(js_msg, OneByteString(isolate, *E2A(msg)));
#else
  js_msg = String::Concat(js_msg, OneByteString(isolate, msg));
#endif
  js_msg = String::Concat(js_msg, FIXED_ONE_BYTE_STRING(isolate, "\x2c\x20"));
  js_msg = String::Concat(js_msg, js_syscall);

  if (path != nullptr) {
    js_path = StringFromPath(isolate, path);

    js_msg = String::Concat(js_msg, FIXED_ONE_BYTE_STRING(isolate, "\x20\x27"));
    js_msg = String::Concat(js_msg, js_path);
    js_msg = String::Concat(js_msg, FIXED_ONE_BYTE_STRING(isolate, "\x27"));
  }

  if (dest != nullptr) {
    js_dest = StringFromPath(isolate, dest);

    js_msg = String::Concat(js_msg, FIXED_ONE_BYTE_STRING(isolate, "\x20\x2d\x3e\x20\x27"));
    js_msg = String::Concat(js_msg, js_dest);
    js_msg = String::Concat(js_msg, FIXED_ONE_BYTE_STRING(isolate, "\x27"));
  }

  Local<Object> e = Exception::Error(js_msg)->ToObject(isolate);

  e->Set(env->errno_string(), Integer::New(isolate, errorno));
  e->Set(env->code_string(), js_code);
  e->Set(env->syscall_string(), js_syscall);
  if (!js_path.IsEmpty())
    e->Set(env->path_string(), js_path);
  if (!js_dest.IsEmpty())
    e->Set(env->dest_string(), js_dest);

  return e;
}


// Look up environment variable unless running as setuid root.
bool SafeGetenv(const char* key, std::string* text) {
#ifndef _WIN32
  // TODO(bnoordhuis) Should perhaps also check whether getauxval(AT_SECURE)
  // is non-zero on Linux.
  if (getuid() != geteuid() || getgid() != getegid()) {
    text->clear();
    return false;
  }
#endif
  if (const char* value = getenv(key)) {
    *text = value;
    return true;
  }
  text->clear();
  return false;
}


#ifdef _WIN32
// Does about the same as strerror(),
// but supports all windows error messages
static const char *winapi_strerror(const int errorno, bool* must_free) {
  char *errmsg = nullptr;

  FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
      FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, errorno,
      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&errmsg, 0, nullptr);

  if (errmsg) {
    *must_free = true;

    // Remove trailing newlines
    for (int i = strlen(errmsg) - 1;
        i >= 0 && (errmsg[i] == '\xa' || errmsg[i] == '\xd'); i--) {
      errmsg[i] = '\x0';
    }

    return errmsg;
  } else {
    // FormatMessage failed
    *must_free = false;
    return "\x55\x6e\x6b\x6e\x6f\x77\x6e\x20\x65\x72\x72\x6f\x72";
  }
}


Local<Value> WinapiErrnoException(Isolate* isolate,
                                  int errorno,
                                  const char* syscall,
                                  const char* msg,
                                  const char* path) {
  Environment* env = Environment::GetCurrent(isolate);
  Local<Value> e;
  bool must_free = false;
  if (!msg || !msg[0]) {
    msg = winapi_strerror(errorno, &must_free);
  }
  Local<String> message = OneByteString(env->isolate(), msg);

  if (path) {
    Local<String> cons1 =
        String::Concat(message, FIXED_ONE_BYTE_STRING(isolate, "\x20\x27"));
    Local<String> cons2 =
        String::Concat(cons1, String::NewFromUtf8(isolate, path));
    Local<String> cons3 =
        String::Concat(cons2, FIXED_ONE_BYTE_STRING(isolate, "\x27"));
    e = Exception::Error(cons3);
  } else {
    e = Exception::Error(message);
  }

  Local<Object> obj = e->ToObject(env->isolate());
  obj->Set(env->errno_string(), Integer::New(isolate, errorno));

  if (path != nullptr) {
    obj->Set(env->path_string(), String::NewFromUtf8(isolate, path));
  }

  if (syscall != nullptr) {
    obj->Set(env->syscall_string(), OneByteString(isolate, syscall));
  }

  if (must_free)
    LocalFree((HLOCAL)msg);

  return e;
}
#endif

void* ArrayBufferAllocator::Allocate(size_t size) {
  if (env_ == nullptr ||
      !env_->array_buffer_allocator_info()->no_zero_fill() ||
      zero_fill_all_buffers)
    return node::UncheckedCalloc(size);
  env_->array_buffer_allocator_info()->reset_fill_flag();
  return node::UncheckedMalloc(size);
}

static bool DomainHasErrorHandler(const Environment* env,
                                  const Local<Object>& domain) {
  HandleScope scope(env->isolate());

  Local<Value> domain_event_listeners_v = domain->Get(env->events_string());
  if (!domain_event_listeners_v->IsObject())
    return false;

  Local<Object> domain_event_listeners_o =
      domain_event_listeners_v.As<Object>();

  Local<Value> domain_error_listeners_v =
      domain_event_listeners_o->Get(env->error_string());

  if (domain_error_listeners_v->IsFunction() ||
      (domain_error_listeners_v->IsArray() &&
      domain_error_listeners_v.As<Array>()->Length() > 0))
    return true;

  return false;
}

static bool DomainsStackHasErrorHandler(const Environment* env) {
  HandleScope scope(env->isolate());

  if (!env->using_domains())
    return false;

  Local<Array> domains_stack_array = env->domains_stack_array().As<Array>();
  if (domains_stack_array->Length() == 0)
    return false;

  uint32_t domains_stack_length = domains_stack_array->Length();
  for (uint32_t i = domains_stack_length; i > 0; --i) {
    Local<Value> domain_v = domains_stack_array->Get(i - 1);
    if (!domain_v->IsObject())
      return false;

    Local<Object> domain = domain_v.As<Object>();
    if (DomainHasErrorHandler(env, domain))
      return true;
  }

  return false;
}


static bool ShouldAbortOnUncaughtException(Isolate* isolate) {
  HandleScope scope(isolate);

  Environment* env = Environment::GetCurrent(isolate);
  Local<Object> process_object = env->process_object();
  Local<String> emitting_top_level_domain_error_key =
    env->emitting_top_level_domain_error_string();
  bool isEmittingTopLevelDomainError =
      process_object->Get(emitting_top_level_domain_error_key)->BooleanValue();

  return isEmittingTopLevelDomainError || !DomainsStackHasErrorHandler(env);
}


void SetupDomainUse(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (env->using_domains())
    return;
  env->set_using_domains(true);

  HandleScope scope(env->isolate());
  Local<Object> process_object = env->process_object();

  Local<String> tick_callback_function_key = env->tick_domain_cb_string();
  Local<Function> tick_callback_function =
      process_object->Get(tick_callback_function_key).As<Function>();

  if (!tick_callback_function->IsFunction()) {
    PrintErrorString(u8"process._tickDomainCallback assigned to non-function\n");
    ABORT();
  }

  process_object->Set(env->tick_callback_string(), tick_callback_function);
  env->set_tick_callback_function(tick_callback_function);

  CHECK(args[0]->IsArray());
  env->set_domain_array(args[0].As<Array>());

  CHECK(args[1]->IsArray());
  env->set_domains_stack_array(args[1].As<Array>());

  // Do a little housekeeping.
  env->process_object()->Delete(
      env->context(),
      FIXED_ONE_BYTE_STRING(args.GetIsolate(), "\x5f\x73\x65\x74\x75\x70\x44\x6f\x6d\x61\x69\x6e\x55\x73\x65")).FromJust();

  uint32_t* const fields = env->domain_flag()->fields();
  uint32_t const fields_count = env->domain_flag()->fields_count();

  Local<ArrayBuffer> array_buffer =
      ArrayBuffer::New(env->isolate(), fields, sizeof(*fields) * fields_count);

  args.GetReturnValue().Set(Uint32Array::New(array_buffer, 0, fields_count));
}

void RunMicrotasks(const FunctionCallbackInfo<Value>& args) {
  args.GetIsolate()->RunMicrotasks();
}


void SetupProcessObject(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsFunction());

  env->set_push_values_to_array_function(args[0].As<Function>());
  env->process_object()->Delete(
      env->context(),
      FIXED_ONE_BYTE_STRING(env->isolate(), "\x5f\x73\x65\x74\x75\x70\x50\x72\x6f\x63\x65\x73\x73\x4f\x62\x6a\x65\x63\x74")).FromJust();
}


void SetupNextTick(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  CHECK(args[0]->IsFunction());
  CHECK(args[1]->IsObject());

  env->set_tick_callback_function(args[0].As<Function>());

  env->SetMethod(args[1].As<Object>(), "\x72\x75\x6e\x4d\x69\x63\x72\x6f\x74\x61\x73\x6b\x73", RunMicrotasks);

  // Do a little housekeeping.
  env->process_object()->Delete(
      env->context(),
      FIXED_ONE_BYTE_STRING(args.GetIsolate(), "\x5f\x73\x65\x74\x75\x70\x4e\x65\x78\x74\x54\x69\x63\x6b")).FromJust();

  // Values use to cross communicate with processNextTick.
  uint32_t* const fields = env->tick_info()->fields();
  uint32_t const fields_count = env->tick_info()->fields_count();

  Local<ArrayBuffer> array_buffer =
      ArrayBuffer::New(env->isolate(), fields, sizeof(*fields) * fields_count);

  args.GetReturnValue().Set(Uint32Array::New(array_buffer, 0, fields_count));
}

void PromiseRejectCallback(PromiseRejectMessage message) {
  Local<Promise> promise = message.GetPromise();
  Isolate* isolate = promise->GetIsolate();
  Local<Value> value = message.GetValue();
  Local<Integer> event = Integer::New(isolate, message.GetEvent());

  Environment* env = Environment::GetCurrent(isolate);
  Local<Function> callback = env->promise_reject_function();

  if (value.IsEmpty())
    value = Undefined(isolate);

  Local<Value> args[] = { event, promise, value };
  Local<Object> process = env->process_object();

  callback->Call(process, arraysize(args), args);
}

void SetupPromises(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = env->isolate();

  CHECK(args[0]->IsFunction());

  isolate->SetPromiseRejectCallback(PromiseRejectCallback);
  env->set_promise_reject_function(args[0].As<Function>());

  env->process_object()->Delete(
      env->context(),
      FIXED_ONE_BYTE_STRING(isolate, "\x5f\x73\x65\x74\x75\x70\x50\x72\x6f\x6d\x69\x73\x65\x73")).FromJust();
}


Local<Value> MakeCallback(Environment* env,
                          Local<Value> recv,
                          const Local<Function> callback,
                          int argc,
                          Local<Value> argv[]) {
  // If you hit this assertion, you forgot to enter the v8::Context first.
  CHECK_EQ(env->context(), env->isolate()->GetCurrentContext());

  Local<Function> pre_fn = env->async_hooks_pre_function();
  Local<Function> post_fn = env->async_hooks_post_function();
  Local<Object> object, domain;
  bool ran_init_callback = false;
  bool has_domain = false;

  Environment::AsyncCallbackScope callback_scope(env);

  // TODO(trevnorris): Adding "_asyncQueue" to the "this" in the init callback
  // is a horrible way to detect usage. Rethink how detection should happen.
  if (recv->IsObject()) {
    object = recv.As<Object>();
    Local<Value> async_queue_v = object->Get(env->async_queue_string());
    if (async_queue_v->IsObject())
      ran_init_callback = true;
  }

  if (env->using_domains()) {
    CHECK(recv->IsObject());
    Local<Value> domain_v = object->Get(env->domain_string());
    has_domain = domain_v->IsObject();
    if (has_domain) {
      domain = domain_v.As<Object>();
      if (domain->Get(env->disposed_string())->IsTrue())
        return Undefined(env->isolate());
    }
  }

  if (has_domain) {
    Local<Value> enter_v = domain->Get(env->enter_string());
    if (enter_v->IsFunction()) {
      if (enter_v.As<Function>()->Call(domain, 0, nullptr).IsEmpty()) {
        FatalError("\x6e\x6f\x64\x65\x3a\x3a\x4d\x61\x6b\x65\x43\x61\x6c\x6c\x62\x61\x63\x6b",
                   "\x64\x6f\x6d\x61\x69\x6e\x20\x65\x6e\x74\x65\x72\x20\x63\x61\x6c\x6c\x62\x61\x63\x6b\x20\x74\x68\x72\x65\x77\x2c\x20\x70\x6c\x65\x61\x73\x65\x20\x72\x65\x70\x6f\x72\x74\x20\x74\x68\x69\x73");
      }
    }
  }

  if (ran_init_callback && !pre_fn.IsEmpty()) {
    TryCatch try_catch(env->isolate());
    MaybeLocal<Value> ar = pre_fn->Call(env->context(), object, 0, nullptr);
    if (ar.IsEmpty()) {
      ClearFatalExceptionHandlers(env);
      FatalException(env->isolate(), try_catch);
      return Local<Value>();
    }
  }

  Local<Value> ret = callback->Call(recv, argc, argv);

  if (ran_init_callback && !post_fn.IsEmpty()) {
    Local<Value> did_throw = Boolean::New(env->isolate(), ret.IsEmpty());
    // Currently there's no way to retrieve an uid from node::MakeCallback().
    // This needs to be fixed.
    Local<Value> vals[] =
        { Undefined(env->isolate()).As<Value>(), did_throw };
    TryCatch try_catch(env->isolate());
    MaybeLocal<Value> ar =
        post_fn->Call(env->context(), object, arraysize(vals), vals);
    if (ar.IsEmpty()) {
      ClearFatalExceptionHandlers(env);
      FatalException(env->isolate(), try_catch);
      return Local<Value>();
    }
  }

  if (ret.IsEmpty()) {
    // NOTE: For backwards compatibility with public API we return Undefined()
    // if the top level call threw.
    return callback_scope.in_makecallback() ?
        ret : Undefined(env->isolate()).As<Value>();
  }

  if (has_domain) {
    Local<Value> exit_v = domain->Get(env->exit_string());
    if (exit_v->IsFunction()) {
      if (exit_v.As<Function>()->Call(domain, 0, nullptr).IsEmpty()) {
        FatalError("\x6e\x6f\x64\x65\x3a\x3a\x4d\x61\x6b\x65\x43\x61\x6c\x6c\x62\x61\x63\x6b",
                   "\x64\x6f\x6d\x61\x69\x6e\x20\x65\x78\x69\x74\x20\x63\x61\x6c\x6c\x62\x61\x63\x6b\x20\x74\x68\x72\x65\x77\x2c\x20\x70\x6c\x65\x61\x73\x65\x20\x72\x65\x70\x6f\x72\x74\x20\x74\x68\x69\x73");
      }
    }
  }

  if (callback_scope.in_makecallback()) {
    return ret;
  }

  Environment::TickInfo* tick_info = env->tick_info();

  if (tick_info->length() == 0) {
    env->isolate()->RunMicrotasks();
  }

  Local<Object> process = env->process_object();

  if (tick_info->length() == 0) {
    tick_info->set_index(0);
    return ret;
  }

  if (env->tick_callback_function()->Call(process, 0, nullptr).IsEmpty()) {
    return Undefined(env->isolate());
  }

  return ret;
}


// Internal only.
Local<Value> MakeCallback(Environment* env,
                           Local<Object> recv,
                           uint32_t index,
                           int argc,
                           Local<Value> argv[]) {
  Local<Value> cb_v = recv->Get(index);
  CHECK(cb_v->IsFunction());
  return MakeCallback(env, recv.As<Value>(), cb_v.As<Function>(), argc, argv);
}


Local<Value> MakeCallback(Environment* env,
                           Local<Object> recv,
                           Local<String> symbol,
                           int argc,
                           Local<Value> argv[]) {
  Local<Value> cb_v = recv->Get(symbol);
  CHECK(cb_v->IsFunction());
  return MakeCallback(env, recv.As<Value>(), cb_v.As<Function>(), argc, argv);
}


Local<Value> MakeCallback(Environment* env,
                           Local<Object> recv,
                           const char* method,
                           int argc,
                           Local<Value> argv[]) {
  Local<String> method_string = OneByteString(env->isolate(), method);
  return MakeCallback(env, recv, method_string, argc, argv);
}


Local<Value> MakeCallback(Isolate* isolate,
                          Local<Object> recv,
                          const char* method,
                          int argc,
                          Local<Value> argv[]) {
  EscapableHandleScope handle_scope(isolate);
  Local<String> method_string = OneByteString(isolate, method);
  return handle_scope.Escape(
      MakeCallback(isolate, recv, method_string, argc, argv));
}


Local<Value> MakeCallback(Isolate* isolate,
                          Local<Object> recv,
                          Local<String> symbol,
                          int argc,
                          Local<Value> argv[]) {
  EscapableHandleScope handle_scope(isolate);
  Local<Value> callback_v = recv->Get(symbol);
  if (callback_v.IsEmpty()) return Local<Value>();
  if (!callback_v->IsFunction()) return Local<Value>();
  Local<Function> callback = callback_v.As<Function>();
  return handle_scope.Escape(MakeCallback(isolate, recv, callback, argc, argv));
}


Local<Value> MakeCallback(Isolate* isolate,
                          Local<Object> recv,
                          Local<Function> callback,
                          int argc,
                          Local<Value> argv[]) {
  // Observe the following two subtleties:
  //
  // 1. The environment is retrieved from the callback function's context.
  // 2. The context to enter is retrieved from the environment.
  //
  // Because of the AssignToContext() call in src/node_contextify.cc,
  // the two contexts need not be the same.
  EscapableHandleScope handle_scope(isolate);
  Environment* env = Environment::GetCurrent(callback->CreationContext());
  Context::Scope context_scope(env->context());
  return handle_scope.Escape(
      MakeCallback(env, recv.As<Value>(), callback, argc, argv));
}


enum encoding ParseEncoding(const char* encoding,
                            enum encoding default_encoding) {
  switch (encoding[0]) {
    case '\x75':
      // utf8, utf16le
      if (encoding[1] == '\x74' && encoding[2] == '\x66') {
        // Skip `-`
        encoding += encoding[3] == '\x2d' ? 4 : 3;
        if (encoding[0] == '\x38' && encoding[1] == '\x0')
          return UTF8;
        if (strncmp(encoding, "\x31\x36\x6c\x65", 4) == 0)
          return UCS2;

      // ucs2
      } else if (encoding[1] == '\x63' && encoding[2] == '\x73') {
        encoding += encoding[3] == '\x2d' ? 4 : 3;
        if (encoding[0] == '\x32' && encoding[1] == '\x0')
          return UCS2;
      }
      break;
    case '\x6c':
      // latin1
      if (encoding[1] == '\x61') {
        if (strncmp(encoding + 2, "\x74\x69\x6e\x31", 4) == 0)
          return LATIN1;
      }
      break;
    case '\x62':
      // binary
      if (encoding[1] == '\x69') {
        if (strncmp(encoding + 2, "\x6e\x61\x72\x79", 4) == 0)
          return LATIN1;

      // buffer
      } else if (encoding[1] == '\x75') {
        if (strncmp(encoding + 2, "\x66\x66\x65\x72", 4) == 0)
          return BUFFER;
      }
      break;
    case '\x0':
      return default_encoding;
    default:
      break;
  }

  if (StringEqualNoCase(encoding, "\x75\x74\x66\x38")) {
    return UTF8;
  } else if (StringEqualNoCase(encoding, "\x75\x74\x66\x2d\x38")) {
    return UTF8;
  } else if (StringEqualNoCase(encoding, "\x61\x73\x63\x69\x69")) {
    return ASCII;
  } else if (StringEqualNoCase(encoding, "\x62\x61\x73\x65\x36\x34")) {
    return BASE64;
  } else if (StringEqualNoCase(encoding, "\x75\x63\x73\x32")) {
    return UCS2;
  } else if (StringEqualNoCase(encoding, "\x75\x63\x73\x2d\x32")) {
    return UCS2;
  } else if (StringEqualNoCase(encoding, "\x75\x74\x66\x31\x36\x6c\x65")) {
    return UCS2;
  } else if (StringEqualNoCase(encoding, "\x75\x74\x66\x2d\x31\x36\x6c\x65")) {
    return UCS2;
  } else if (StringEqualNoCase(encoding, "\x6c\x61\x74\x69\x6e\x31")) {
    return LATIN1;
  } else if (StringEqualNoCase(encoding, "\x62\x69\x6e\x61\x72\x79")) {
    return LATIN1;  // BINARY is a deprecated alias of LATIN1.
  } else if (StringEqualNoCase(encoding, "\x62\x75\x66\x66\x65\x72")) {
    return BUFFER;
  } else if (StringEqualNoCase(encoding, "\x68\x65\x78")) {
    return HEX;
  } else {
    return default_encoding;
  }
}


enum encoding ParseEncoding(Isolate* isolate,
                            Local<Value> encoding_v,
                            enum encoding default_encoding) {
  CHECK(!encoding_v.IsEmpty());

  if (!encoding_v->IsString())
    return default_encoding;

  node::Utf8Value encoding(isolate, encoding_v);

  return ParseEncoding(*encoding, default_encoding);
}

Local<Value> Encode(Isolate* isolate,
                    const char* buf,
                    size_t len,
                    enum encoding encoding) {
  CHECK_NE(encoding, UCS2);
  return StringBytes::Encode(isolate, buf, len, encoding);
}

Local<Value> Encode(Isolate* isolate, const uint16_t* buf, size_t len) {
  return StringBytes::Encode(isolate, buf, len);
}

// Returns -1 if the handle was not valid for decoding
ssize_t DecodeBytes(Isolate* isolate,
                    Local<Value> val,
                    enum encoding encoding) {
  HandleScope scope(isolate);

  return StringBytes::Size(isolate, val, encoding);
}

// Returns number of bytes written.
ssize_t DecodeWrite(Isolate* isolate,
                    char* buf,
                    size_t buflen,
                    Local<Value> val,
                    enum encoding encoding) {
  return StringBytes::Write(isolate, buf, buflen, val, encoding, nullptr);
}

bool IsExceptionDecorated(Environment* env, Local<Value> er) {
  if (!er.IsEmpty() && er->IsObject()) {
    Local<Object> err_obj = er.As<Object>();
    auto maybe_value =
        err_obj->GetPrivate(env->context(), env->decorated_private_symbol());
    Local<Value> decorated;
    return maybe_value.ToLocal(&decorated) && decorated->IsTrue();
  }
  return false;
}

void AppendExceptionLine(Environment* env,
                         Local<Value> er,
                         Local<Message> message,
                         enum ErrorHandlingMode mode) {
  if (message.IsEmpty())
    return;

  HandleScope scope(env->isolate());
  Local<Object> err_obj;
  if (!er.IsEmpty() && er->IsObject()) {
    err_obj = er.As<Object>();

    auto context = env->context();
    auto processed_private_symbol = env->processed_private_symbol();
    // Do it only once per message
    if (err_obj->HasPrivate(context, processed_private_symbol).FromJust())
      return;
    err_obj->SetPrivate(
        context,
        processed_private_symbol,
        True(env->isolate()));
  }

  // Print (filename):(line number): (message).
  ScriptOrigin origin = message->GetScriptOrigin();
  node::NativeEncodingValue filename(env->isolate(), message->GetScriptResourceName());
  const char* filename_string = *filename;
  int linenum = message->GetLineNumber();
  // Print line of source code.
  node::NativeEncodingValue sourceline(env->isolate(), message->GetSourceLine());
  const char* sourceline_string = *sourceline;

  // Because of how node modules work, all scripts are wrapped with a
  // "function (module, exports, __filename, ...) {"
  // to provide script local variables.
  //
  // When reporting errors on the first line of a script, this wrapper
  // function is leaked to the user. There used to be a hack here to
  // truncate off the first 62 characters, but it caused numerous other
  // problems when vm.runIn*Context() methods were used for non-module
  // code.
  //
  // If we ever decide to re-instate such a hack, the following steps
  // must be taken:
  //
  // 1. Pass a flag around to say "this code was wrapped"
  // 2. Update the stack frame output so that it is also correct.
  //
  // It would probably be simpler to add a line rather than add some
  // number of characters to the first line, since V8 truncates the
  // sourceline to 78 characters, and we end up not providing very much
  // useful debugging info to the user if we remove 62 characters.

  int script_start =
      (linenum - origin.ResourceLineOffset()->Value()) == 1 ?
          origin.ResourceColumnOffset()->Value() : 0;
  int start = message->GetStartColumn(env->context()).FromMaybe(0);
  int end = message->GetEndColumn(env->context()).FromMaybe(0);
  if (start >= script_start) {
    CHECK_GE(end, start);
    start -= script_start;
    end -= script_start;
  }

  char arrow[1024];
  int max_off = sizeof(arrow) - 2;

  int off = snprintf(arrow,
                     sizeof(arrow),
                     "%s:%i\n%s\n",
                     filename_string,
                     linenum,
                     sourceline_string);
  CHECK_GE(off, 0);
  if (off > max_off) {
    off = max_off;
  }

  // Print wavy underline (GetUnderline is deprecated).
  for (int i = 0; i < start; i++) {
    if (sourceline_string[i] == '\0' || off >= max_off) {
      break;
    }
    CHECK_LT(off, max_off);
    arrow[off++] = (sourceline_string[i] == '\t') ? '\t' : ' ';
  }
  for (int i = start; i < end; i++) {
    if (sourceline_string[i] == '\0' || off >= max_off) {
      break;
    }
    CHECK_LT(off, max_off);
    arrow[off++] = '^';
  }
  CHECK_LE(off, max_off);
  arrow[off] = '\n';
  arrow[off + 1] = '\0';

#ifdef __MVS__
  __e2a_s(arrow);
#endif
  
  Local<String> arrow_str = String::NewFromUtf8(env->isolate(), arrow);

  const bool can_set_arrow = !arrow_str.IsEmpty() && !err_obj.IsEmpty();
  // If allocating arrow_str failed, print it out. There's not much else to do.
  // If it's not an error, but something needs to be printed out because
  // it's a fatal exception, also print it out from here.
  // Otherwise, the arrow property will be attached to the object and handled
  // by the caller.
  if (!can_set_arrow || (mode == FATAL_ERROR && !err_obj->IsNativeError())) {
    if (env->printed_error())
      return;
    env->set_printed_error(true);

    uv_tty_reset_mode();
    PrintErrorString(u8"\n%s", arrow);
    return;
  }

  CHECK(err_obj->SetPrivate(
            env->context(),
            env->arrow_message_private_symbol(),
            arrow_str).FromMaybe(false));
}


static void ReportException(Environment* env,
                            Local<Value> er,
                            Local<Message> message) {
  HandleScope scope(env->isolate());

  AppendExceptionLine(env, er, message, FATAL_ERROR);

  Local<Value> trace_value;
  Local<Value> arrow;
  const bool decorated = IsExceptionDecorated(env, er);

  if (er->IsUndefined() || er->IsNull()) {
    trace_value = Undefined(env->isolate());
  } else {
    Local<Object> err_obj = er->ToObject(env->isolate());

    trace_value = err_obj->Get(env->stack_string());
    arrow =
        err_obj->GetPrivate(
            env->context(),
            env->arrow_message_private_symbol()).ToLocalChecked();
  }

  node::Utf8Value trace(env->isolate(), trace_value);

  // range errors have a trace member set to undefined
  if (trace.length() > 0 && !trace_value->IsUndefined()) {
    if (arrow.IsEmpty() || !arrow->IsString() || decorated) {
      PrintErrorString(u8"%s\n", *trace);
    } else {
      node::Utf8Value arrow_string(env->isolate(), arrow);
      PrintErrorString(u8"%s\n%s\n", *arrow_string, *trace);
    }
  } else {
    // this really only happens for RangeErrors, since they're the only
    // kind that won't have all this info in the trace, or when non-Error
    // objects are thrown manually.
    Local<Value> message;
    Local<Value> name;

    if (er->IsObject()) {
      Local<Object> err_obj = er.As<Object>();
      message = err_obj->Get(env->message_string());
      name = err_obj->Get(FIXED_ONE_BYTE_STRING(env->isolate(), "\x6e\x61\x6d\x65"));
    }

    if (message.IsEmpty() ||
        message->IsUndefined() ||
        name.IsEmpty() ||
        name->IsUndefined()) {
      // Not an error object. Just print as-is.
      String::Utf8Value message(er);

      PrintErrorString(u8"%s\n", *message ? *message :
                                          u8"<toString() threw exception>");
    } else {
      node::Utf8Value name_string(env->isolate(), name);
      node::Utf8Value message_string(env->isolate(), message);

      if (arrow.IsEmpty() || !arrow->IsString() || decorated) {
        PrintErrorString(u8"%s: %s\n", *name_string, *message_string);
      } else {
        node::Utf8Value arrow_string(env->isolate(), arrow);
        PrintErrorString(u8"%s\n%s: %s\n",
                         *arrow_string,
                         *name_string,
                         *message_string);
      }
    }
  }

  fflush(stderr);
}


static void ReportException(Environment* env, const TryCatch& try_catch) {
  ReportException(env, try_catch.Exception(), try_catch.Message());
}


// Executes a str within the current v8 context.
static Local<Value> ExecuteString(Environment* env,
                                  Local<String> source,
                                  Local<String> filename) {
  EscapableHandleScope scope(env->isolate());
  TryCatch try_catch(env->isolate());

  // try_catch must be nonverbose to disable FatalException() handler,
  // we will handle exceptions ourself.
  try_catch.SetVerbose(false);

  ScriptOrigin origin(filename);
  MaybeLocal<v8::Script> script =
      v8::Script::Compile(env->context(), source, &origin);
  if (script.IsEmpty()) {
    ReportException(env, try_catch);
    platform_exit(3);
  }

  Local<Value> result = script.ToLocalChecked()->Run();
  if (result.IsEmpty()) {
    ReportException(env, try_catch);
    platform_exit(4);
  }

  return scope.Escape(result);
}


static void GetActiveRequests(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Local<Array> ary = Array::New(args.GetIsolate());
  Local<Context> ctx = env->context();
  Local<Function> fn = env->push_values_to_array_function();
  Local<Value> argv[NODE_PUSH_VAL_TO_ARRAY_MAX];
  size_t idx = 0;

  for (auto w : *env->req_wrap_queue()) {
    if (w->persistent().IsEmpty())
      continue;
    argv[idx] = w->object();
    if (++idx >= arraysize(argv)) {
      fn->Call(ctx, ary, idx, argv).ToLocalChecked();
      idx = 0;
    }
  }

  if (idx > 0) {
    fn->Call(ctx, ary, idx, argv).ToLocalChecked();
  }

  args.GetReturnValue().Set(ary);
}


// Non-static, friend of HandleWrap. Could have been a HandleWrap method but
// implemented here for consistency with GetActiveRequests().
void GetActiveHandles(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Local<Array> ary = Array::New(env->isolate());
  Local<Context> ctx = env->context();
  Local<Function> fn = env->push_values_to_array_function();
  Local<Value> argv[NODE_PUSH_VAL_TO_ARRAY_MAX];
  size_t idx = 0;

  Local<String> owner_sym = env->owner_string();

  for (auto w : *env->handle_wrap_queue()) {
    if (w->persistent().IsEmpty() || !HandleWrap::HasRef(w))
      continue;
    Local<Object> object = w->object();
    Local<Value> owner = object->Get(owner_sym);
    if (owner->IsUndefined())
      owner = object;
    argv[idx] = owner;
    if (++idx >= arraysize(argv)) {
      fn->Call(ctx, ary, idx, argv).ToLocalChecked();
      idx = 0;
    }
  }
  if (idx > 0) {
    fn->Call(ctx, ary, idx, argv).ToLocalChecked();
  }

  args.GetReturnValue().Set(ary);
}


NO_RETURN void Abort() {
  DumpBacktrace(stderr);
  fflush(stderr);
  ABORT_NO_BACKTRACE();
}


NO_RETURN void Assert(const char* const (*args)[4]) {
  auto filename = (*args)[0];
  auto linenum = (*args)[1];
  auto message = (*args)[2];
  auto function = (*args)[3];

  char exepath[256];
  size_t exepath_size = sizeof(exepath);
  if (uv_exepath(exepath, &exepath_size))
    snprintf(exepath, sizeof(exepath), "node");

  char pid[12] = {0};
#ifndef _WIN32
  snprintf(pid, sizeof(pid), "[%u]", getpid());
#endif

  fprintf(stderr, "%s%s: %s:%s:%s%s Assertion `%s' failed.\n",
          exepath, pid, filename, linenum,
          function, *function ? ":" : "", message);
  fflush(stderr);

  Abort();
}


static void Abort(const FunctionCallbackInfo<Value>& args) {
  Abort();
}


static void Chdir(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() != 1 || !args[0]->IsString()) {
    return env->ThrowTypeError("\x42\x61\x64\x20\x61\x72\x67\x75\x6d\x65\x6e\x74\x2e");
  }

  node::NativeEncodingValue path(args.GetIsolate(), args[0]);
  int err = uv_chdir(*path);
  if (err) {
    return env->ThrowUVException(err, "\x75\x76\x5f\x63\x68\x64\x69\x72");
  }
}


static void Cwd(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
#ifdef _WIN32
  /* MAX_PATH is in characters, not bytes. Make sure we have enough headroom. */
  char buf[MAX_PATH * 4];
#else
  char buf[PATH_MAX];
#endif

  size_t cwd_len = sizeof(buf);
  int err = uv_cwd(buf, &cwd_len);
  if (err) {
    return env->ThrowUVException(err, "\x75\x76\x5f\x63\x77\x64");
  }

  Local<String> cwd = String::NewFromUtf8(env->isolate(),
#ifdef __MVS__
                                          *E2A(buf, cwd_len),
#else
                                          buf,
#endif
                                          String::kNormalString,
                                          cwd_len);
  args.GetReturnValue().Set(cwd);
}


static void Umask(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  uint32_t old;

  if (args.Length() < 1 || args[0]->IsUndefined()) {
    old = umask(0);
    umask(static_cast<mode_t>(old));
  } else if (!args[0]->IsInt32() && !args[0]->IsString()) {
    return env->ThrowTypeError("\x61\x72\x67\x75\x6d\x65\x6e\x74\x20\x6d\x75\x73\x74\x20\x62\x65\x20\x61\x6e\x20\x69\x6e\x74\x65\x67\x65\x72\x20\x6f\x72\x20\x6f\x63\x74\x61\x6c\x20\x73\x74\x72\x69\x6e\x67\x2e");
  } else {
    int oct;
    if (args[0]->IsInt32()) {
      oct = args[0]->Uint32Value();
    } else {
      oct = 0;
      node::Utf8Value str(env->isolate(), args[0]);

      // Parse the octal string.
      for (size_t i = 0; i < str.length(); i++) {
        char c = (*str)[i];
        if (c > '\x37' || c < '\x30') {
          return env->ThrowTypeError("\x69\x6e\x76\x61\x6c\x69\x64\x20\x6f\x63\x74\x61\x6c\x20\x73\x74\x72\x69\x6e\x67");
        }
        oct *= 8;
        oct += c - '\x30';
      }
    }
    old = umask(static_cast<mode_t>(oct));
  }

  args.GetReturnValue().Set(old);
}


#if defined(__POSIX__) && !defined(__ANDROID__)

static const uid_t uid_not_found = static_cast<uid_t>(-1);
static const gid_t gid_not_found = static_cast<gid_t>(-1);


static uid_t uid_by_name(const char* name) {
  struct passwd pwd;
  struct passwd* pp;
  char buf[8192];

  errno = 0;
  pp = nullptr;

  if (getpwnam_r(name, &pwd, buf, sizeof(buf), &pp) == 0 && pp != nullptr) {
    return pp->pw_uid;
  }

  return uid_not_found;
}


static char* name_by_uid(uid_t uid) {
  struct passwd pwd;
  struct passwd* pp;
  char buf[8192];
  int rc;

  errno = 0;
  pp = nullptr;

  if ((rc = getpwuid_r(uid, &pwd, buf, sizeof(buf), &pp)) == 0 &&
      pp != nullptr) {
    return strdup(pp->pw_name);
  }

  if (rc == 0) {
    errno = ENOENT;
  }

  return nullptr;
}


static gid_t gid_by_name(const char* name) {
  struct group pwd;
  struct group* pp;
  char buf[8192];

  errno = 0;
  pp = nullptr;

  if (getgrnam_r(name, &pwd, buf, sizeof(buf), &pp) == 0 && pp != nullptr) {
    return pp->gr_gid;
  }

  return gid_not_found;
}


#if 0  // For future use.
static const char* name_by_gid(gid_t gid) {
  struct group pwd;
  struct group* pp;
  char buf[8192];
  int rc;

  errno = 0;
  pp = nullptr;

  if ((rc = getgrgid_r(gid, &pwd, buf, sizeof(buf), &pp)) == 0 &&
      pp != nullptr) {
    return strdup(pp->gr_name);
  }

  if (rc == 0) {
    errno = ENOENT;
  }

  return nullptr;
}
#endif


static uid_t uid_by_name(Isolate* isolate, Local<Value> value) {
  if (value->IsUint32()) {
    return static_cast<uid_t>(value->Uint32Value());
  } else {
    node::Utf8Value name(isolate, value);
    return uid_by_name(*name);
  }
}


static gid_t gid_by_name(Isolate* isolate, Local<Value> value) {
  if (value->IsUint32()) {
    return static_cast<gid_t>(value->Uint32Value());
  } else {
    node::Utf8Value name(isolate, value);
    return gid_by_name(*name);
  }
}

static void GetUid(const FunctionCallbackInfo<Value>& args) {
  // uid_t is an uint32_t on all supported platforms.
  args.GetReturnValue().Set(static_cast<uint32_t>(getuid()));
}


static void GetGid(const FunctionCallbackInfo<Value>& args) {
  // gid_t is an uint32_t on all supported platforms.
  args.GetReturnValue().Set(static_cast<uint32_t>(getgid()));
}


static void GetEUid(const FunctionCallbackInfo<Value>& args) {
  // uid_t is an uint32_t on all supported platforms.
  args.GetReturnValue().Set(static_cast<uint32_t>(geteuid()));
}


static void GetEGid(const FunctionCallbackInfo<Value>& args) {
  // gid_t is an uint32_t on all supported platforms.
  args.GetReturnValue().Set(static_cast<uint32_t>(getegid()));
}


static void SetGid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("\x73\x65\x74\x67\x69\x64\x20\x61\x72\x67\x75\x6d\x65\x6e\x74\x20\x6d\x75\x73\x74\x20\x62\x65\x20\x61\x20\x6e\x75\x6d\x62\x65\x72\x20\x6f\x72\x20\x61\x20\x73\x74\x72\x69\x6e\x67");
  }

  gid_t gid = gid_by_name(env->isolate(), args[0]);

  if (gid == gid_not_found) {
    return env->ThrowError("\x73\x65\x74\x67\x69\x64\x20\x67\x72\x6f\x75\x70\x20\x69\x64\x20\x64\x6f\x65\x73\x20\x6e\x6f\x74\x20\x65\x78\x69\x73\x74");
  }

  if (setgid(gid)) {
    return env->ThrowErrnoException(errno, "\x73\x65\x74\x67\x69\x64");
  }
}


static void SetEGid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("\x73\x65\x74\x65\x67\x69\x64\x20\x61\x72\x67\x75\x6d\x65\x6e\x74\x20\x6d\x75\x73\x74\x20\x62\x65\x20\x61\x20\x6e\x75\x6d\x62\x65\x72\x20\x6f\x72\x20\x73\x74\x72\x69\x6e\x67");
  }

  gid_t gid = gid_by_name(env->isolate(), args[0]);

  if (gid == gid_not_found) {
    return env->ThrowError("\x73\x65\x74\x65\x67\x69\x64\x20\x67\x72\x6f\x75\x70\x20\x69\x64\x20\x64\x6f\x65\x73\x20\x6e\x6f\x74\x20\x65\x78\x69\x73\x74");
  }

  if (setegid(gid)) {
    return env->ThrowErrnoException(errno, "\x73\x65\x74\x65\x67\x69\x64");
  }
}


static void SetUid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("\x73\x65\x74\x75\x69\x64\x20\x61\x72\x67\x75\x6d\x65\x6e\x74\x20\x6d\x75\x73\x74\x20\x62\x65\x20\x61\x20\x6e\x75\x6d\x62\x65\x72\x20\x6f\x72\x20\x61\x20\x73\x74\x72\x69\x6e\x67");
  }

  uid_t uid = uid_by_name(env->isolate(), args[0]);

  if (uid == uid_not_found) {
    return env->ThrowError("\x73\x65\x74\x75\x69\x64\x20\x75\x73\x65\x72\x20\x69\x64\x20\x64\x6f\x65\x73\x20\x6e\x6f\x74\x20\x65\x78\x69\x73\x74");
  }

  if (setuid(uid)) {
    return env->ThrowErrnoException(errno, "\x73\x65\x74\x75\x69\x64");
  }
}


static void SetEUid(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("\x73\x65\x74\x65\x75\x69\x64\x20\x61\x72\x67\x75\x6d\x65\x6e\x74\x20\x6d\x75\x73\x74\x20\x62\x65\x20\x61\x20\x6e\x75\x6d\x62\x65\x72\x20\x6f\x72\x20\x73\x74\x72\x69\x6e\x67");
  }

  uid_t uid = uid_by_name(env->isolate(), args[0]);

  if (uid == uid_not_found) {
    return env->ThrowError("\x73\x65\x74\x65\x75\x69\x64\x20\x75\x73\x65\x72\x20\x69\x64\x20\x64\x6f\x65\x73\x20\x6e\x6f\x74\x20\x65\x78\x69\x73\x74");
  }

  if (seteuid(uid)) {
    return env->ThrowErrnoException(errno, "\x73\x65\x74\x65\x75\x69\x64");
  }
}


static void GetGroups(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  int ngroups = getgroups(0, nullptr);

  if (ngroups == -1) {
    return env->ThrowErrnoException(errno, "\x67\x65\x74\x67\x72\x6f\x75\x70\x73");
  }

  gid_t* groups = new gid_t[ngroups];

  ngroups = getgroups(ngroups, groups);

  if (ngroups == -1) {
    delete[] groups;
    return env->ThrowErrnoException(errno, "\x67\x65\x74\x67\x72\x6f\x75\x70\x73");
  }

  Local<Array> groups_list = Array::New(env->isolate(), ngroups);
  bool seen_egid = false;
  gid_t egid = getegid();

  for (int i = 0; i < ngroups; i++) {
    groups_list->Set(i, Integer::New(env->isolate(), groups[i]));
    if (groups[i] == egid)
      seen_egid = true;
  }

  delete[] groups;

  if (seen_egid == false) {
    groups_list->Set(ngroups, Integer::New(env->isolate(), egid));
  }

  args.GetReturnValue().Set(groups_list);
}


static void SetGroups(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsArray()) {
    return env->ThrowTypeError("\x61\x72\x67\x75\x6d\x65\x6e\x74\x20\x31\x20\x6d\x75\x73\x74\x20\x62\x65\x20\x61\x6e\x20\x61\x72\x72\x61\x79");
  }

  Local<Array> groups_list = args[0].As<Array>();
  size_t size = groups_list->Length();
  gid_t* groups = new gid_t[size];

  for (size_t i = 0; i < size; i++) {
    gid_t gid = gid_by_name(env->isolate(), groups_list->Get(i));

    if (gid == gid_not_found) {
      delete[] groups;
      return env->ThrowError("\x67\x72\x6f\x75\x70\x20\x6e\x61\x6d\x65\x20\x6e\x6f\x74\x20\x66\x6f\x75\x6e\x64");
    }

    groups[i] = gid;
  }

  int rc = setgroups(size, groups);
  delete[] groups;

  if (rc == -1) {
    return env->ThrowErrnoException(errno, "\x73\x65\x74\x67\x72\x6f\x75\x70\x73");
  }
}


static void InitGroups(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (!args[0]->IsUint32() && !args[0]->IsString()) {
    return env->ThrowTypeError("\x61\x72\x67\x75\x6d\x65\x6e\x74\x20\x31\x20\x6d\x75\x73\x74\x20\x62\x65\x20\x61\x20\x6e\x75\x6d\x62\x65\x72\x20\x6f\x72\x20\x61\x20\x73\x74\x72\x69\x6e\x67");
  }

  if (!args[1]->IsUint32() && !args[1]->IsString()) {
    return env->ThrowTypeError("\x61\x72\x67\x75\x6d\x65\x6e\x74\x20\x32\x20\x6d\x75\x73\x74\x20\x62\x65\x20\x61\x20\x6e\x75\x6d\x62\x65\x72\x20\x6f\x72\x20\x61\x20\x73\x74\x72\x69\x6e\x67");
  }

  node::Utf8Value arg0(env->isolate(), args[0]);
  gid_t extra_group;
  bool must_free;
  char* user;

  if (args[0]->IsUint32()) {
    user = name_by_uid(args[0]->Uint32Value());
    must_free = true;
  } else {
    user = *arg0;
    must_free = false;
  }

  if (user == nullptr) {
    return env->ThrowError("\x69\x6e\x69\x74\x67\x72\x6f\x75\x70\x73\x20\x75\x73\x65\x72\x20\x6e\x6f\x74\x20\x66\x6f\x75\x6e\x64");
  }

  extra_group = gid_by_name(env->isolate(), args[1]);

  if (extra_group == gid_not_found) {
    if (must_free)
      free(user);
    return env->ThrowError("\x69\x6e\x69\x74\x67\x72\x6f\x75\x70\x73\x20\x65\x78\x74\x72\x61\x20\x67\x72\x6f\x75\x70\x20\x6e\x6f\x74\x20\x66\x6f\x75\x6e\x64");
  }

  int rc = initgroups(user, extra_group);

  if (must_free) {
    free(user);
  }

  if (rc) {
    return env->ThrowErrnoException(errno, "\x69\x6e\x69\x74\x67\x72\x6f\x75\x70\x73");
  }
}

#endif  // __POSIX__ && !defined(__ANDROID__)


static void WaitForInspectorDisconnect(Environment* env) {
#if HAVE_INSPECTOR
  if (env->inspector_agent()->IsConnected()) {
    // Restore signal dispositions, the app is done and is no longer
    // capable of handling signals.
#if defined(__POSIX__) && !defined(NODE_SHARED_MODE)
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    for (unsigned nr = 1; nr < kMaxSignal; nr += 1) {
      if (nr == SIGKILL || nr == SIGSTOP || nr == SIGPROF)
        continue;
      act.sa_handler = (nr == SIGPIPE) ? SIG_IGN : SIG_DFL;
      CHECK_EQ(0, sigaction(nr, &act, nullptr));
    }
#endif
    env->inspector_agent()->WaitForDisconnect();
  }
#endif
}


void Exit(const FunctionCallbackInfo<Value>& args) {
  WaitForInspectorDisconnect(Environment::GetCurrent(args));
  platform_exit(args[0]->Int32Value());
}


static void Uptime(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  double uptime;

  uv_update_time(env->event_loop());
  uptime = uv_now(env->event_loop()) - prog_start_time;

  args.GetReturnValue().Set(Number::New(env->isolate(), uptime / 1000));
}


void MemoryUsage(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  size_t rss;
  int err = uv_resident_set_memory(&rss);
  if (err) {
    return env->ThrowUVException(err, "\x75\x76\x5f\x72\x65\x73\x69\x64\x65\x6e\x74\x5f\x73\x65\x74\x5f\x6d\x65\x6d\x6f\x72\x79");
  }

  Isolate* isolate = env->isolate();
  // V8 memory usage
  HeapStatistics v8_heap_stats;
  isolate->GetHeapStatistics(&v8_heap_stats);

  // Get the double array pointer from the Float64Array argument.
  CHECK(args[0]->IsFloat64Array());
  Local<Float64Array> array = args[0].As<Float64Array>();
  CHECK_EQ(array->Length(), 4);
  Local<ArrayBuffer> ab = array->Buffer();
  double* fields = static_cast<double*>(ab->GetContents().Data());

  fields[0] = rss;
  fields[1] = v8_heap_stats.total_heap_size();
  fields[2] = v8_heap_stats.used_heap_size();
  fields[3] = isolate->AdjustAmountOfExternalAllocatedMemory(0);
}


void Kill(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() != 2) {
    return env->ThrowError("\x42\x61\x64\x20\x61\x72\x67\x75\x6d\x65\x6e\x74\x2e");
  }

  int pid = args[0]->Int32Value();
  int sig = args[1]->Int32Value();
  int err = uv_kill(pid, sig);
  args.GetReturnValue().Set(err);
}

// used in Hrtime() below
#define NANOS_PER_SEC 1000000000

// Hrtime exposes libuv's uv_hrtime() high-resolution timer.
// The value returned by uv_hrtime() is a 64-bit int representing nanoseconds,
// so this function instead returns an Array with 2 entries representing seconds
// and nanoseconds, to avoid any integer overflow possibility.
// Pass in an Array from a previous hrtime() call to instead get a time diff.
void Hrtime(const FunctionCallbackInfo<Value>& args) {
  uint64_t t = uv_hrtime();

  Local<ArrayBuffer> ab = args[0].As<Uint32Array>()->Buffer();
  uint32_t* fields = static_cast<uint32_t*>(ab->GetContents().Data());

  // These three indices will contain the values for the hrtime tuple. The
  // seconds value is broken into the upper/lower 32 bits and stored in two
  // uint32 fields to be converted back in JS.
  fields[0] = (t / NANOS_PER_SEC) >> 32;
  fields[1] = (t / NANOS_PER_SEC) & 0xffffffff;
  fields[2] = t % NANOS_PER_SEC;
}

// Microseconds in a second, as a float, used in CPUUsage() below
#define MICROS_PER_SEC 1e6

// CPUUsage use libuv's uv_getrusage() this-process resource usage accessor,
// to access ru_utime (user CPU time used) and ru_stime (system CPU time used),
// which are uv_timeval_t structs (long tv_sec, long tv_usec).
// Returns those values as Float64 microseconds in the elements of the array
// passed to the function.
void CPUUsage(const FunctionCallbackInfo<Value>& args) {
  uv_rusage_t rusage;

  // Call libuv to get the values we'll return.
  int err = uv_getrusage(&rusage);
  if (err) {
    // On error, return the strerror version of the error code.
    Local<String> errmsg = OneByteString(args.GetIsolate(), uv_strerror(err));
    args.GetReturnValue().Set(errmsg);
    return;
  }

  // Get the double array pointer from the Float64Array argument.
  CHECK(args[0]->IsFloat64Array());
  Local<Float64Array> array = args[0].As<Float64Array>();
  CHECK_EQ(array->Length(), 2);
  Local<ArrayBuffer> ab = array->Buffer();
  double* fields = static_cast<double*>(ab->GetContents().Data());

  // Set the Float64Array elements to be user / system values in microseconds.
  fields[0] = MICROS_PER_SEC * rusage.ru_utime.tv_sec + rusage.ru_utime.tv_usec;
  fields[1] = MICROS_PER_SEC * rusage.ru_stime.tv_sec + rusage.ru_stime.tv_usec;
}

extern "C" void node_module_register(void* m) {
  struct node_module* mp = reinterpret_cast<struct node_module*>(m);

  if (mp->nm_flags & NM_F_BUILTIN) {
    mp->nm_link = modlist_builtin;
    modlist_builtin = mp;
  } else if (!node_is_initialized) {
    // "Linked" modules are included as part of the node project.
    // Like builtins they are registered *before* node::Init runs.
    mp->nm_flags = NM_F_LINKED;
    mp->nm_link = modlist_linked;
    modlist_linked = mp;
  } else {
    modpending = mp;
  }
}

struct node_module* get_builtin_module(const char* name) {
  struct node_module* mp;

  for (mp = modlist_builtin; mp != nullptr; mp = mp->nm_link) {
    if (strcmp(mp->nm_modname, name) == 0)
      break;
  }

  CHECK(mp == nullptr || (mp->nm_flags & NM_F_BUILTIN) != 0);
  return (mp);
}

struct node_module* get_linked_module(const char* name) {
  struct node_module* mp;

  for (mp = modlist_linked; mp != nullptr; mp = mp->nm_link) {
    if (strcmp(mp->nm_modname, name) == 0)
      break;
  }

  CHECK(mp == nullptr || (mp->nm_flags & NM_F_LINKED) != 0);
  return mp;
}

// DLOpen is process.dlopen(module, filename).
// Used to load 'module.node' dynamically shared objects.
//
// FIXME(bnoordhuis) Not multi-context ready. TBD how to resolve the conflict
// when two contexts try to load the same shared object. Maybe have a shadow
// cache that's a plain C list or hash table that's shared across contexts?
void DLOpen(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  uv_lib_t lib;

  CHECK_EQ(modpending, nullptr);

  if (args.Length() != 2) {
    env->ThrowError("\x70\x72\x6f\x63\x65\x73\x73\x2e\x64\x6c\x6f\x70\x65\x6e\x20\x74\x61\x6b\x65\x73\x20\x65\x78\x61\x63\x74\x6c\x79\x20\x32\x20\x61\x72\x67\x75\x6d\x65\x6e\x74\x73\x2e");
    return;
  }

  Local<Object> module = args[0]->ToObject(env->isolate());  // Cast
  node::Utf8Value filename(env->isolate(), args[1]);  // Cast
#ifdef __MVS__
  __a2e_s(*filename);
#endif
  const bool is_dlopen_error = uv_dlopen(*filename, &lib);

  // Objects containing v14 or later modules will have registered themselves
  // on the pending list.  Activate all of them now.  At present, only one
  // module per object is supported.
  node_module* const mp = modpending;
  modpending = nullptr;

  if (is_dlopen_error) {
#ifdef __MVS__
    Local<String> errmsg = OneByteString(env->isolate(), *E2A(uv_dlerror(&lib)));
#else
    Local<String> errmsg = OneByteString(env->isolate(), uv_dlerror(&lib));
#endif
    uv_dlclose(&lib);
#ifdef _WIN32
    // Windows needs to add the filename into the error message
    errmsg = String::Concat(errmsg, args[1]->ToString(env->isolate()));
#endif  // _WIN32
    env->isolate()->ThrowException(Exception::Error(errmsg));
    return;
  }

  if (mp == nullptr) {
    uv_dlclose(&lib);
    env->ThrowError("\x4d\x6f\x64\x75\x6c\x65\x20\x64\x69\x64\x20\x6e\x6f\x74\x20\x73\x65\x6c\x66\x2d\x72\x65\x67\x69\x73\x74\x65\x72\x2e");
    return;
  }

  // -1 is used for N-API modules
  if ((mp->nm_version != -1) && (mp->nm_version != NODE_MODULE_VERSION)) {
    char errmsg[1024];
#ifdef __MVS__
    __snprintf_a(errmsg,
#else
    snprintf(errmsg,
#endif
             sizeof(errmsg),
             u8"Module version mismatch. Expected %d, got %d.",
             NODE_MODULE_VERSION, mp->nm_version);

    // NOTE: `mp` is allocated inside of the shared library's memory, calling
    // `uv_dlclose` will deallocate it
    uv_dlclose(&lib);
    env->ThrowError(errmsg);
    return;
  }
  if (mp->nm_flags & NM_F_BUILTIN) {
    uv_dlclose(&lib);
    env->ThrowError("\x42\x75\x69\x6c\x74\x2d\x69\x6e\x20\x6d\x6f\x64\x75\x6c\x65\x20\x73\x65\x6c\x66\x2d\x72\x65\x67\x69\x73\x74\x65\x72\x65\x64\x2e");
    return;
  }

  mp->nm_dso_handle = lib.handle;
  mp->nm_link = modlist_addon;
  modlist_addon = mp;

  Local<String> exports_string = env->exports_string();
  Local<Object> exports = module->Get(exports_string)->ToObject(env->isolate());

  if (mp->nm_context_register_func != nullptr) {
    mp->nm_context_register_func(exports, module, env->context(), mp->nm_priv);
  } else if (mp->nm_register_func != nullptr) {
    mp->nm_register_func(exports, module, mp->nm_priv);
  } else {
    uv_dlclose(&lib);
    env->ThrowError("\x4d\x6f\x64\x75\x6c\x65\x20\x68\x61\x73\x20\x6e\x6f\x20\x64\x65\x63\x6c\x61\x72\x65\x64\x20\x65\x6e\x74\x72\x79\x20\x70\x6f\x69\x6e\x74\x2e");
    return;
  }

  // Tell coverity that 'handle' should not be freed when we return.
  // coverity[leaked_storage]
}


static void OnFatalError(const char* location, const char* message) {
  if (location) {
    PrintErrorString(u8"FATAL ERROR: %s %s\n", location, message);
  } else {
    PrintErrorString(u8"FATAL ERROR: %s\n", message);
  }
  fflush(stderr);
  OS390ThreadManager::Destroy(0);
  ABORT();
}


NO_RETURN void FatalError(const char* location, const char* message) {
  OnFatalError(location, message);
  // to suppress compiler warning
  ABORT();
}


void FatalException(Isolate* isolate,
                    Local<Value> error,
                    Local<Message> message) {
  HandleScope scope(isolate);

  Environment* env = Environment::GetCurrent(isolate);
  Local<Object> process_object = env->process_object();
  Local<String> fatal_exception_string = env->fatal_exception_string();
  Local<Function> fatal_exception_function =
      process_object->Get(fatal_exception_string).As<Function>();

  int exit_code = 0;
  if (!fatal_exception_function->IsFunction()) {
    // failed before the process._fatalException function was added!
    // this is probably pretty bad.  Nothing to do but report and exit.
    ReportException(env, error, message);
    exit_code = 6;
  }

  if (exit_code == 0) {
    TryCatch fatal_try_catch(isolate);

    // Do not call FatalException when _fatalException handler throws
    fatal_try_catch.SetVerbose(false);

    // this will return true if the JS layer handled it, false otherwise
    Local<Value> caught =
        fatal_exception_function->Call(process_object, 1, &error);

    if (fatal_try_catch.HasCaught()) {
      // the fatal exception function threw, so we must exit
      ReportException(env, fatal_try_catch);
      exit_code = 7;
    }

    if (exit_code == 0 && false == caught->BooleanValue()) {
      ReportException(env, error, message);
      exit_code = 1;
    }
  }

  if (exit_code) {
#if HAVE_INSPECTOR
    if (use_inspector) {
      env->inspector_agent()->FatalException(error, message);
    }
#endif
    platform_exit(exit_code);
  }
}


void FatalException(Isolate* isolate, const TryCatch& try_catch) {
  HandleScope scope(isolate);
  if (!try_catch.IsVerbose()) {
    FatalException(isolate, try_catch.Exception(), try_catch.Message());
  }
}


void OnMessage(Local<Message> message, Local<Value> error) {
  // The current version of V8 sends messages for errors only
  // (thus `error` is always set).
  FatalException(Isolate::GetCurrent(), error, message);
}


void ClearFatalExceptionHandlers(Environment* env) {
  Local<Object> process = env->process_object();
  Local<Value> events =
      process->Get(env->context(), env->events_string()).ToLocalChecked();

  if (events->IsObject()) {
    events.As<Object>()->Set(
        env->context(),
        OneByteString(env->isolate(), "\x75\x6e\x63\x61\x75\x67\x68\x74\x45\x78\x63\x65\x70\x74\x69\x6f\x6e"),
        Undefined(env->isolate())).FromJust();
  }

  process->Set(
      env->context(),
      env->domain_string(),
      Undefined(env->isolate())).FromJust();
}

// Call process.emitWarning(str), fmt is a snprintf() format string
void ProcessEmitWarning(Environment* env, const char* fmt, ...) {
  char warning[1024];
  va_list ap;

  va_start(ap, fmt);
  __vsnprintf_a(warning, sizeof(warning), fmt, ap);
  va_end(ap);

  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());

  Local<Object> process = env->process_object();
  MaybeLocal<Value> emit_warning = process->Get(env->context(),
      FIXED_ONE_BYTE_STRING(env->isolate(), "\x65\x6d\x69\x74\x57\x61\x72\x6e\x69\x6e\x67"));
  Local<Value> arg = node::OneByteString(env->isolate(), warning);

  Local<Value> f;

  if (!emit_warning.ToLocal(&f)) return;
  if (!f->IsFunction()) return;

  // MakeCallback() unneeded, because emitWarning is internal code, it calls
  // process.emit('warning', ..), but does so on the nextTick.
  f.As<v8::Function>()->Call(process, 1, &arg);
}


static void Binding(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  Local<String> module = args[0]->ToString(env->isolate());
  node::Utf8Value module_v(env->isolate(), module);

  Local<Object> cache = env->binding_cache_object();
  Local<Object> exports;

  if (cache->Has(env->context(), module).FromJust()) {
    exports = cache->Get(module)->ToObject(env->isolate());
    args.GetReturnValue().Set(exports);
    return;
  }

  // Append a string to process.moduleLoadList
  char buf[1024];
#ifdef __MVS__
  __snprintf_a(buf, sizeof(buf), u8"Binding %s", *module_v);
#else
  snprintf(buf, sizeof(buf), "Binding %s", *module_v);
#endif

  Local<Array> modules = env->module_load_list_array();
  uint32_t l = modules->Length();
  modules->Set(l, OneByteString(env->isolate(), buf));

  node_module* mod = get_builtin_module(*module_v);
  if (mod != nullptr) {
    exports = Object::New(env->isolate());
    // Internal bindings don't have a "module" object, only exports.
    CHECK_EQ(mod->nm_register_func, nullptr);
    CHECK_NE(mod->nm_context_register_func, nullptr);
    Local<Value> unused = Undefined(env->isolate());
    mod->nm_context_register_func(exports, unused,
      env->context(), mod->nm_priv);
    cache->Set(module, exports);
  } else if (!strcmp(*module_v, "\x63\x6f\x6e\x73\x74\x61\x6e\x74\x73")) {
    exports = Object::New(env->isolate());
    DefineConstants(env->isolate(), exports);
    cache->Set(module, exports);
  } else if (!strcmp(*module_v, "\x6e\x61\x74\x69\x76\x65\x73")) {
    exports = Object::New(env->isolate());
    DefineJavaScript(env, exports);
    cache->Set(module, exports);
  } else {
    char errmsg[1024];
#ifdef __MVS__
    __snprintf_a(errmsg,
#else
    snprintf(errmsg,
#endif
             sizeof(errmsg),
             u8"No such module: %s",
             *module_v);
    return env->ThrowError(errmsg);
  }

  args.GetReturnValue().Set(exports);
}

static void LinkedBinding(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args.GetIsolate());

  Local<String> module_name = args[0]->ToString(env->isolate());

  Local<Object> cache = env->binding_cache_object();
  Local<Value> exports_v = cache->Get(module_name);

  if (exports_v->IsObject())
    return args.GetReturnValue().Set(exports_v.As<Object>());

  node::Utf8Value module_name_v(env->isolate(), module_name);
  node_module* mod = get_linked_module(*module_name_v);

  if (mod == nullptr) {
    char errmsg[1024];
#ifdef __MVS__
    __snprintf_a(errmsg,
#else
    snprintf(errmsg,
#endif
             sizeof(errmsg),
             u8"No such module was linked: %s",
             *module_name_v);
    return env->ThrowError(errmsg);
  }

  Local<Object> module = Object::New(env->isolate());
  Local<Object> exports = Object::New(env->isolate());
  Local<String> exports_prop = String::NewFromUtf8(env->isolate(), "\x65\x78\x70\x6f\x72\x74\x73");
  module->Set(exports_prop, exports);

  if (mod->nm_context_register_func != nullptr) {
    mod->nm_context_register_func(exports,
                                  module,
                                  env->context(),
                                  mod->nm_priv);
  } else if (mod->nm_register_func != nullptr) {
    mod->nm_register_func(exports, module, mod->nm_priv);
  } else {
    return env->ThrowError("\x4c\x69\x6e\x6b\x65\x64\x20\x6d\x6f\x64\x75\x6c\x65\x20\x68\x61\x73\x20\x6e\x6f\x20\x64\x65\x63\x6c\x61\x72\x65\x64\x20\x65\x6e\x74\x72\x79\x20\x70\x6f\x69\x6e\x74\x2e");
  }

  auto effective_exports = module->Get(exports_prop);
  cache->Set(module_name, effective_exports);

  args.GetReturnValue().Set(effective_exports);
}

static void ProcessTitleGetter(Local<Name> property,
                               const PropertyCallbackInfo<Value>& info) {
  char buffer[512];
  uv_get_process_title(buffer, sizeof(buffer));
#ifdef __MVS__
  __e2a_s(buffer);
#endif
  info.GetReturnValue().Set(String::NewFromUtf8(info.GetIsolate(), buffer));
}


static void ProcessTitleSetter(Local<Name> property,
                               Local<Value> value,
                               const PropertyCallbackInfo<void>& info) {
  node::Utf8Value title(info.GetIsolate(), value);
  // TODO(piscisaureus): protect with a lock
#ifdef __MVS__
  __a2e_s(*title);
#endif
  uv_set_process_title(*title);
}


static void EnvGetter(Local<Name> property,
                      const PropertyCallbackInfo<Value>& info) {
  Isolate* isolate = info.GetIsolate();
#ifdef __POSIX__
  node::Utf8Value key(isolate, property);
#ifdef __MVS__
  __a2e_s(*key);
#endif
  const char* val = getenv(*key);
  if (val) {
#ifdef __MVS__
    char *utf8val = strdup(val);
    __e2a_s(utf8val);
    Local<String> vall = String::NewFromUtf8(isolate, utf8val);
    free(utf8val);
    return info.GetReturnValue().Set(vall);
#else
    return info.GetReturnValue().Set(String::NewFromUtf8(isolate, val));
#endif
  }
#else  // _WIN32
  String::Value key(property);
  WCHAR buffer[32767];  // The maximum size allowed for environment variables.
  SetLastError(ERROR_SUCCESS);
  DWORD result = GetEnvironmentVariableW(reinterpret_cast<WCHAR*>(*key),
                                         buffer,
                                         arraysize(buffer));
  // If result >= sizeof buffer the buffer was too small. That should never
  // happen. If result == 0 and result != ERROR_SUCCESS the variable was not
  // not found.
  if ((result > 0 || GetLastError() == ERROR_SUCCESS) &&
      result < arraysize(buffer)) {
    const uint16_t* two_byte_buffer = reinterpret_cast<const uint16_t*>(buffer);
    Local<String> rc = String::NewFromTwoByte(isolate, two_byte_buffer);
    return info.GetReturnValue().Set(rc);
  }
#endif
}


static void EnvSetter(Local<Name> property,
                      Local<Value> value,
                      const PropertyCallbackInfo<Value>& info) {
#ifdef __POSIX__
  node::Utf8Value key(info.GetIsolate(), property);
  node::Utf8Value val(info.GetIsolate(), value);
#ifdef __MVS__
  __a2e_s(*key);
  __a2e_s(*val);
#endif
  setenv(*key, *val, 1);
#ifdef __MVS__
  __e2a_s(*key);
  __e2a_s(*val);
#endif
#else  // _WIN32
  String::Value key(property);
  String::Value val(value);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  // Environment variables that start with '=' are read-only.
  if (key_ptr[0] != L'\x3d') {
    SetEnvironmentVariableW(key_ptr, reinterpret_cast<WCHAR*>(*val));
  }
#endif
  // Whether it worked or not, always return value.
  info.GetReturnValue().Set(value);
}


static void EnvQuery(Local<Name> property,
                     const PropertyCallbackInfo<Integer>& info) {
  int32_t rc = -1;  // Not found unless proven otherwise.
#ifdef __POSIX__
  node::Utf8Value key(info.GetIsolate(), property);
#ifdef __MVS__
  __a2e_s(*key);
#endif
  if (getenv(*key))
    rc = 0;
#else  // _WIN32
  String::Value key(property);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  SetLastError(ERROR_SUCCESS);
  if (GetEnvironmentVariableW(key_ptr, nullptr, 0) > 0 ||
      GetLastError() == ERROR_SUCCESS) {
    rc = 0;
    if (key_ptr[0] == L'\x3d') {
      // Environment variables that start with '=' are hidden and read-only.
      rc = static_cast<int32_t>(v8::ReadOnly) |
           static_cast<int32_t>(v8::DontDelete) |
           static_cast<int32_t>(v8::DontEnum);
    }
  }
#endif
  if (rc != -1)
    info.GetReturnValue().Set(rc);
}


static void EnvDeleter(Local<Name> property,
                       const PropertyCallbackInfo<Boolean>& info) {
#ifdef __POSIX__
  node::Utf8Value key(info.GetIsolate(), property);
#ifdef __MVS__
  __a2e_s(*key);
#endif
  unsetenv(*key);
#else
  String::Value key(property);
  WCHAR* key_ptr = reinterpret_cast<WCHAR*>(*key);
  SetEnvironmentVariableW(key_ptr, nullptr);
#endif

  // process.env never has non-configurable properties, so always
  // return true like the tc39 delete operator.
  info.GetReturnValue().Set(true);
}


static void EnvEnumerator(const PropertyCallbackInfo<Array>& info) {
  Environment* env = Environment::GetCurrent(info);
  Isolate* isolate = env->isolate();
  Local<Context> ctx = env->context();
  Local<Function> fn = env->push_values_to_array_function();
  Local<Value> argv[NODE_PUSH_VAL_TO_ARRAY_MAX];
  size_t idx = 0;

#ifdef __POSIX__
  int size = 0;
  while (environ[size])
    size++;

  Local<Array> envarr = Array::New(isolate);

  for (int i = 0; i < size; ++i) {
    const char* var = environ[i];
#ifdef __MVS__
    char * ascii_var = strdup(var);
    __e2a_s(ascii_var);
    var = ascii_var;
#endif
    const char* s = strchr(var, '\x3d');
    const int length = s ? s - var : strlen(var);
    argv[idx] = String::NewFromUtf8(isolate,
                                    var,
                                    String::kNormalString,
                                    length);
#ifdef __MVS__
    free((void*)ascii_var);
#endif
    if (++idx >= arraysize(argv)) {
      fn->Call(ctx, envarr, idx, argv).ToLocalChecked();
      idx = 0;
    }
  }
  if (idx > 0) {
    fn->Call(ctx, envarr, idx, argv).ToLocalChecked();
  }
#else  // _WIN32
  WCHAR* environment = GetEnvironmentStringsW();
  if (environment == nullptr)
    return;  // This should not happen.
  Local<Array> envarr = Array::New(isolate);
  WCHAR* p = environment;
  while (*p) {
    WCHAR *s;
    if (*p == L'\x3d') {
      // If the key starts with '=' it is a hidden environment variable.
      p += wcslen(p) + 1;
      continue;
    } else {
      s = wcschr(p, L'\x3d');
    }
    if (!s) {
      s = p + wcslen(p);
    }
    const uint16_t* two_byte_buffer = reinterpret_cast<const uint16_t*>(p);
    const size_t two_byte_buffer_len = s - p;
    argv[idx] = String::NewFromTwoByte(isolate,
                                       two_byte_buffer,
                                       String::kNormalString,
                                       two_byte_buffer_len);
    if (++idx >= arraysize(argv)) {
      fn->Call(ctx, envarr, idx, argv).ToLocalChecked();
      idx = 0;
    }
    p = s + wcslen(s) + 1;
  }
  if (idx > 0) {
    fn->Call(ctx, envarr, idx, argv).ToLocalChecked();
  }
  FreeEnvironmentStringsW(environment);
#endif

  info.GetReturnValue().Set(envarr);
}


static void GetParentProcessId(Local<Name> property,
                               const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(Integer::New(info.GetIsolate(), uv_os_getppid()));
}


static Local<Object> GetFeatures(Environment* env) {
  EscapableHandleScope scope(env->isolate());

  Local<Object> obj = Object::New(env->isolate());
#if defined(DEBUG) && DEBUG
  Local<Value> debug = True(env->isolate());
#else
  Local<Value> debug = False(env->isolate());
#endif  // defined(DEBUG) && DEBUG

  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x64\x65\x62\x75\x67"), debug);
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x75\x76"), True(env->isolate()));
  // TODO(bnoordhuis) ping libuv
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x69\x70\x76\x36"), True(env->isolate()));

#ifndef OPENSSL_NO_NEXTPROTONEG
  Local<Boolean> tls_npn = True(env->isolate());
#else
  Local<Boolean> tls_npn = False(env->isolate());
#endif
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x74\x6c\x73\x5f\x6e\x70\x6e"), tls_npn);

#ifdef TLSEXT_TYPE_application_layer_protocol_negotiation
  Local<Boolean> tls_alpn = True(env->isolate());
#else
  Local<Boolean> tls_alpn = False(env->isolate());
#endif
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x74\x6c\x73\x5f\x61\x6c\x70\x6e"), tls_alpn);

#ifdef SSL_CTRL_SET_TLSEXT_SERVERNAME_CB
  Local<Boolean> tls_sni = True(env->isolate());
#else
  Local<Boolean> tls_sni = False(env->isolate());
#endif
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x74\x6c\x73\x5f\x73\x6e\x69"), tls_sni);

#if !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_set_tlsext_status_cb)
  Local<Boolean> tls_ocsp = True(env->isolate());
#else
  Local<Boolean> tls_ocsp = False(env->isolate());
#endif  // !defined(OPENSSL_NO_TLSEXT) && defined(SSL_CTX_set_tlsext_status_cb)
  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x74\x6c\x73\x5f\x6f\x63\x73\x70"), tls_ocsp);

  obj->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x74\x6c\x73"),
           Boolean::New(env->isolate(),
                        get_builtin_module("\x63\x72\x79\x70\x74\x6f") != nullptr));

  return scope.Escape(obj);
}


static void DebugPortGetter(Local<Name> property,
                            const PropertyCallbackInfo<Value>& info) {
  info.GetReturnValue().Set(debug_port);
}


static void DebugPortSetter(Local<Name> property,
                            Local<Value> value,
                            const PropertyCallbackInfo<void>& info) {
  debug_port = value->Int32Value();
}


static void DebugProcess(const FunctionCallbackInfo<Value>& args);
static void DebugPause(const FunctionCallbackInfo<Value>& args);
static void DebugEnd(const FunctionCallbackInfo<Value>& args);


void NeedImmediateCallbackGetter(Local<Name> property,
                                 const PropertyCallbackInfo<Value>& info) {
  Environment* env = Environment::GetCurrent(info);
  const uv_check_t* immediate_check_handle = env->immediate_check_handle();
  bool active = uv_is_active(
      reinterpret_cast<const uv_handle_t*>(immediate_check_handle));
  info.GetReturnValue().Set(active);
}


static void NeedImmediateCallbackSetter(
    Local<Name> property,
    Local<Value> value,
    const PropertyCallbackInfo<void>& info) {
  Environment* env = Environment::GetCurrent(info);

  uv_check_t* immediate_check_handle = env->immediate_check_handle();
  bool active = uv_is_active(
      reinterpret_cast<const uv_handle_t*>(immediate_check_handle));

  if (active == value->BooleanValue())
    return;

  uv_idle_t* immediate_idle_handle = env->immediate_idle_handle();

  if (active) {
    uv_check_stop(immediate_check_handle);
    uv_idle_stop(immediate_idle_handle);
  } else {
    uv_check_start(immediate_check_handle, CheckImmediate);
    // Idle handle is needed only to stop the event loop from blocking in poll.
    uv_idle_start(immediate_idle_handle, IdleImmediateDummy);
  }
}


void SetIdle(uv_prepare_t* handle) {
  Environment* env = Environment::from_idle_prepare_handle(handle);
  env->isolate()->GetCpuProfiler()->SetIdle(true);
}


void ClearIdle(uv_check_t* handle) {
  Environment* env = Environment::from_idle_check_handle(handle);
  env->isolate()->GetCpuProfiler()->SetIdle(false);
}


void StartProfilerIdleNotifier(Environment* env) {
  uv_prepare_start(env->idle_prepare_handle(), SetIdle);
  uv_check_start(env->idle_check_handle(), ClearIdle);
}


void StopProfilerIdleNotifier(Environment* env) {
  uv_prepare_stop(env->idle_prepare_handle());
  uv_check_stop(env->idle_check_handle());
}


void StartProfilerIdleNotifier(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  StartProfilerIdleNotifier(env);
}


void StopProfilerIdleNotifier(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  StopProfilerIdleNotifier(env);
}


#define READONLY_PROPERTY(obj, str, var)                                      \
  do {                                                                        \
    obj->DefineOwnProperty(env->context(),                                    \
                           OneByteString(env->isolate(), str),                \
                           var,                                               \
                           v8::ReadOnly).FromJust();                          \
  } while (0)

#define READONLY_DONT_ENUM_PROPERTY(obj, str, var)                            \
  do {                                                                        \
    obj->DefineOwnProperty(env->context(),                                    \
                           OneByteString(env->isolate(), str),                \
                           var,                                               \
                           static_cast<v8::PropertyAttribute>(v8::ReadOnly |  \
                                                              v8::DontEnum))  \
        .FromJust();                                                          \
  } while (0)


void SetupProcessObject(Environment* env,
                        int argc,
                        const char* const* argv,
                        int exec_argc,
                        const char* const* exec_argv) {
  HandleScope scope(env->isolate());

  Local<Object> process = env->process_object();

  auto title_string = FIXED_ONE_BYTE_STRING(env->isolate(), "\x74\x69\x74\x6c\x65");
  CHECK(process->SetAccessor(env->context(),
                             title_string,
                             ProcessTitleGetter,
                             ProcessTitleSetter,
                             env->as_external()).FromJust());

  // process.version
  READONLY_PROPERTY(process,
                    "\x76\x65\x72\x73\x69\x6f\x6e",
                    FIXED_ONE_BYTE_STRING(env->isolate(), NODE_VERSION));

  // process.moduleLoadList
  READONLY_PROPERTY(process,
                    "\x6d\x6f\x64\x75\x6c\x65\x4c\x6f\x61\x64\x4c\x69\x73\x74",
                    env->module_load_list_array());

  // process.versions
  Local<Object> versions = Object::New(env->isolate());
  READONLY_PROPERTY(process, "\x76\x65\x72\x73\x69\x6f\x6e\x73", versions);

  const char http_parser_version[] = NODE_STRINGIFY(HTTP_PARSER_VERSION_MAJOR)
                                     "\x2e"
                                     NODE_STRINGIFY(HTTP_PARSER_VERSION_MINOR)
                                     "\x2e"
                                     NODE_STRINGIFY(HTTP_PARSER_VERSION_PATCH);
  READONLY_PROPERTY(versions,
                    "\x68\x74\x74\x70\x5f\x70\x61\x72\x73\x65\x72",
                    FIXED_ONE_BYTE_STRING(env->isolate(), http_parser_version));

  // +1 to get rid of the leading 'v'
  READONLY_PROPERTY(versions,
                    "\x6e\x6f\x64\x65",
                    OneByteString(env->isolate(), NODE_VERSION + 1));
  READONLY_PROPERTY(versions,
                    "\x76\x38",
                    OneByteString(env->isolate(), V8::GetVersion()));
  READONLY_PROPERTY(versions,
                    "\x75\x76",
                    OneByteString(env->isolate(), *E2A(uv_version_string())));
  READONLY_PROPERTY(versions,
                    "\x7a\x6c\x69\x62",
                    FIXED_ONE_BYTE_STRING(env->isolate(), ZLIB_VERSION));
  READONLY_PROPERTY(versions,
                    "\x61\x72\x65\x73",
                    FIXED_ONE_BYTE_STRING(env->isolate(), NODE_STRINGIFY(ARES_VERSION_STR)));

#if defined(NODE_HAVE_I18N_SUPPORT) && defined(U_ICU_VERSION)
  READONLY_PROPERTY(versions,
                    "\x69\x63\x75",
                    OneByteString(env->isolate(), u8"" U_ICU_VERSION));

  if (!icu_data_dir.empty()) {
    // Did the user attempt (via env var or parameter) to set an ICU path?
    READONLY_PROPERTY(process,
                      "\x69\x63\x75\x5f\x64\x61\x74\x61\x5f\x64\x69\x72",
                      OneByteString(env->isolate(), *E2A(icu_data_dir.c_str())));
  }
#endif

  const char node_modules_version[] = NODE_STRINGIFY(NODE_MODULE_VERSION);
  READONLY_PROPERTY(
      versions,
      "\x6d\x6f\x64\x75\x6c\x65\x73",
      FIXED_ONE_BYTE_STRING(env->isolate(), node_modules_version));

  const char node_napi_version[] = NODE_STRINGIFY(NAPI_VERSION);
  READONLY_PROPERTY(
      versions,
      "\x6e\x61\x70\x69",
      FIXED_ONE_BYTE_STRING(env->isolate(), node_napi_version));

  // process._promiseRejectEvent
  Local<Object> promiseRejectEvent = Object::New(env->isolate());
  READONLY_DONT_ENUM_PROPERTY(process,
                              "\x5f\x70\x72\x6f\x6d\x69\x73\x65\x52\x65\x6a\x65\x63\x74\x45\x76\x65\x6e\x74",
                              promiseRejectEvent);
  READONLY_PROPERTY(promiseRejectEvent,
                    "\x75\x6e\x68\x61\x6e\x64\x6c\x65\x64",
                    Integer::New(env->isolate(),
                                 v8::kPromiseRejectWithNoHandler));
  READONLY_PROPERTY(promiseRejectEvent,
                    "\x68\x61\x6e\x64\x6c\x65\x64",
                    Integer::New(env->isolate(),
                                 v8::kPromiseHandlerAddedAfterReject));

#if HAVE_OPENSSL
  // Stupid code to slice out the version string.
  {  // NOLINT(whitespace/braces)
    size_t i, j, k;
    int c;
    for (i = j = 0, k = sizeof(OPENSSL_VERSION_TEXT) - 1; i < k; ++i) {
      c = OPENSSL_VERSION_TEXT[i];
      if ('\x30' <= c && c <= '\x39') {
        for (j = i + 1; j < k; ++j) {
          c = OPENSSL_VERSION_TEXT[j];
          if (c == '\x20')
            break;
        }
        break;
      }
    }
    READONLY_PROPERTY(
        versions,
        "\x6f\x70\x65\x6e\x73\x73\x6c",
        OneByteString(env->isolate(), &OPENSSL_VERSION_TEXT[i], j - i));
  }
#endif

  // process.arch
  READONLY_PROPERTY(process, "\x61\x72\x63\x68", OneByteString(env->isolate(), *E2A(NODE_ARCH)));

  // process.platform
  READONLY_PROPERTY(process,
                    "\x70\x6c\x61\x74\x66\x6f\x72\x6d",
                    OneByteString(env->isolate(), *E2A(NODE_PLATFORM)));

  // process.release
  Local<Object> release = Object::New(env->isolate());
  READONLY_PROPERTY(process, "\x72\x65\x6c\x65\x61\x73\x65", release);
  READONLY_PROPERTY(release, "\x6e\x61\x6d\x65",
                    OneByteString(env->isolate(), NODE_RELEASE));

#if NODE_VERSION_IS_LTS
  READONLY_PROPERTY(release, "\x6c\x74\x73",
                    OneByteString(env->isolate(), NODE_VERSION_LTS_CODENAME));
#endif

// if this is a release build and no explicit base has been set
// substitute the standard release download URL
#ifndef NODE_RELEASE_URLBASE
# if NODE_VERSION_IS_RELEASE
#  define NODE_RELEASE_URLBASE "\x68\x74\x74\x70\x73\x3a\x2f\x2f\x6e\x6f\x64\x65\x6a\x73\x2e\x6f\x72\x67\x2f\x64\x6f\x77\x6e\x6c\x6f\x61\x64\x2f\x72\x65\x6c\x65\x61\x73\x65\x2f"
# endif
#endif

#if defined(NODE_RELEASE_URLBASE)
#  define NODE_RELEASE_URLPFX NODE_RELEASE_URLBASE "\x76" NODE_VERSION_STRING "\x2f"
#  define NODE_RELEASE_URLFPFX NODE_RELEASE_URLPFX "\x6e\x6f\x64\x65\x2d\x76" NODE_VERSION_STRING

  READONLY_PROPERTY(release,
                    "\x73\x6f\x75\x72\x63\x65\x55\x72\x6c",
                    OneByteString(env->isolate(),
                    NODE_RELEASE_URLFPFX "\x2e\x74\x61\x72\x2e\x67\x7a"));
  READONLY_PROPERTY(release,
                    "\x68\x65\x61\x64\x65\x72\x73\x55\x72\x6c",
                    OneByteString(env->isolate(),
                    NODE_RELEASE_URLFPFX "\x2d\x68\x65\x61\x64\x65\x72\x73\x2e\x74\x61\x72\x2e\x67\x7a"));
#  ifdef _WIN32
  READONLY_PROPERTY(release,
                    "\x6c\x69\x62\x55\x72\x6c",
                    OneByteString(env->isolate(),
                    strcmp(NODE_ARCH, "\x69\x61\x33\x32") ? NODE_RELEASE_URLPFX "\x77\x69\x6e\x2d"
                                                NODE_ARCH "\x2f\x6e\x6f\x64\x65\x2e\x6c\x69\x62"
                                              : NODE_RELEASE_URLPFX
                                                "\x77\x69\x6e\x2d\x78\x38\x36\x2f\x6e\x6f\x64\x65\x2e\x6c\x69\x62"));
#  endif
#endif

  // process.argv
  Local<Array> arguments = Array::New(env->isolate(), argc);
  for (int i = 0; i < argc; ++i) {
    arguments->Set(i, String::NewFromUtf8(env->isolate(), argv[i]));
  }
  process->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x61\x72\x67\x76"), arguments);

  // process.execArgv
  Local<Array> exec_arguments = Array::New(env->isolate(), exec_argc);
  for (int i = 0; i < exec_argc; ++i) {
    exec_arguments->Set(i, String::NewFromUtf8(env->isolate(), exec_argv[i]));
  }
  process->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x65\x78\x65\x63\x41\x72\x67\x76"),
               exec_arguments);

  // create process.env
  Local<ObjectTemplate> process_env_template =
      ObjectTemplate::New(env->isolate());
  process_env_template->SetHandler(NamedPropertyHandlerConfiguration(
          EnvGetter,
          EnvSetter,
          EnvQuery,
          EnvDeleter,
          EnvEnumerator,
          env->as_external(),
          PropertyHandlerFlags::kOnlyInterceptStrings));

  Local<Object> process_env =
      process_env_template->NewInstance(env->context()).ToLocalChecked();
  process->Set(FIXED_ONE_BYTE_STRING(env->isolate(), u8"env"), process_env);

  READONLY_PROPERTY(process, "\x70\x69\x64", Integer::New(env->isolate(), getpid()));
  READONLY_PROPERTY(process, "\x66\x65\x61\x74\x75\x72\x65\x73", GetFeatures(env));

  process->SetAccessor(FIXED_ONE_BYTE_STRING(env->isolate(), "\x70\x70\x69\x64"),
                       GetParentProcessId);

  auto need_immediate_callback_string =
      FIXED_ONE_BYTE_STRING(env->isolate(), "\x5f\x6e\x65\x65\x64\x49\x6d\x6d\x65\x64\x69\x61\x74\x65\x43\x61\x6c\x6c\x62\x61\x63\x6b");
  CHECK(process->SetAccessor(env->context(), need_immediate_callback_string,
                             NeedImmediateCallbackGetter,
                             NeedImmediateCallbackSetter,
                             env->as_external()).FromJust());

  // -e, --eval
  if (eval_string) {
    READONLY_PROPERTY(process,
                      "\x5f\x65\x76\x61\x6c",
                      String::NewFromUtf8(env->isolate(), eval_string));
  }

  // -p, --print
  if (print_eval) {
    READONLY_PROPERTY(process, "\x5f\x70\x72\x69\x6e\x74\x5f\x65\x76\x61\x6c", True(env->isolate()));
  }

  // -c, --check
  if (syntax_check_only) {
    READONLY_PROPERTY(process, "\x5f\x73\x79\x6e\x74\x61\x78\x5f\x63\x68\x65\x63\x6b\x5f\x6f\x6e\x6c\x79", True(env->isolate()));
  }

  // -i, --interactive
  if (force_repl) {
    READONLY_PROPERTY(process, "\x5f\x66\x6f\x72\x63\x65\x52\x65\x70\x6c", True(env->isolate()));
  }

  // -r, --require
  if (!preload_modules.empty()) {
    Local<Array> array = Array::New(env->isolate());
    for (unsigned int i = 0; i < preload_modules.size(); ++i) {
      Local<String> module = String::NewFromUtf8(env->isolate(),
                                                 preload_modules[i].c_str());
      array->Set(i, module);
    }
    READONLY_PROPERTY(process,
                      "\x5f\x70\x72\x65\x6c\x6f\x61\x64\x5f\x6d\x6f\x64\x75\x6c\x65\x73",
                      array);

    preload_modules.clear();
  }

  // --insecure-http-parser
  if (insecure_http_parser) {
    READONLY_PROPERTY(process, "\x69\x6e\x73\x65\x63\x75\x72\x65\x48\x54\x54\x50\x50\x61\x72\x73\x65\x72", True(env->isolate()));
  }

  // --no-deprecation
  if (no_deprecation) {
    READONLY_PROPERTY(process, "\x6e\x6f\x44\x65\x70\x72\x65\x63\x61\x74\x69\x6f\x6e", True(env->isolate()));
  }

  // --no-warnings
  if (no_process_warnings) {
    READONLY_PROPERTY(process, "\x6e\x6f\x50\x72\x6f\x63\x65\x73\x73\x57\x61\x72\x6e\x69\x6e\x67\x73", True(env->isolate()));
  }

  // --trace-warnings
  if (trace_warnings) {
    READONLY_PROPERTY(process, "\x74\x72\x61\x63\x65\x50\x72\x6f\x63\x65\x73\x73\x57\x61\x72\x6e\x69\x6e\x67\x73", True(env->isolate()));
  }

  // --throw-deprecation
  if (throw_deprecation) {
    READONLY_PROPERTY(process, "\x74\x68\x72\x6f\x77\x44\x65\x70\x72\x65\x63\x61\x74\x69\x6f\x6e", True(env->isolate()));
  }

#ifdef NODE_NO_BROWSER_GLOBALS
  // configure --no-browser-globals
  READONLY_PROPERTY(process, "\x5f\x6e\x6f\x42\x72\x6f\x77\x73\x65\x72\x47\x6c\x6f\x62\x61\x6c\x73", True(env->isolate()));
#endif  // NODE_NO_BROWSER_GLOBALS

  // --prof-process
  if (prof_process) {
    READONLY_PROPERTY(process, "\x70\x72\x6f\x66\x50\x72\x6f\x63\x65\x73\x73", True(env->isolate()));
  }

  // --trace-deprecation
  if (trace_deprecation) {
    READONLY_PROPERTY(process, "\x74\x72\x61\x63\x65\x44\x65\x70\x72\x65\x63\x61\x74\x69\x6f\x6e", True(env->isolate()));
  }

  // --debug-brk
  if (debug_wait_connect) {
    READONLY_PROPERTY(process, "\x5f\x64\x65\x62\x75\x67\x57\x61\x69\x74\x43\x6f\x6e\x6e\x65\x63\x74", True(env->isolate()));
  }

  // --security-revert flags
#define V(code, _, __)                                                        \
  do {                                                                        \
    if (IsReverted(SECURITY_REVERT_ ## code)) {                               \
      READONLY_PROPERTY(process, "\x52\x45\x56\x45\x52\x54\x5f" USTR(#code), True(env->isolate()));      \
    }                                                                         \
  } while (0);
  SECURITY_REVERSIONS(V)
#undef V

  size_t exec_path_len = 2 * PATH_MAX;
  char* exec_path = new char[exec_path_len];
  Local<String> exec_path_value;
  if (uv_exepath(exec_path, &exec_path_len) == 0) {
#ifdef __MVS__
    __e2a_s(exec_path);
#endif
    exec_path_value = String::NewFromUtf8(env->isolate(),
                                          exec_path,
                                          String::kNormalString,
                                          exec_path_len);
  } else {
    exec_path_value = String::NewFromUtf8(env->isolate(), argv[0]);
  }
  process->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x65\x78\x65\x63\x50\x61\x74\x68"),
               exec_path_value);
  delete[] exec_path;

  auto debug_port_string = FIXED_ONE_BYTE_STRING(env->isolate(), "\x64\x65\x62\x75\x67\x50\x6f\x72\x74");
  CHECK(process->SetAccessor(env->context(),
                             debug_port_string,
                             DebugPortGetter,
                             DebugPortSetter,
                             env->as_external()).FromJust());

  // define various internal methods
  env->SetMethod(process,
                 "\x5f\x73\x74\x61\x72\x74\x50\x72\x6f\x66\x69\x6c\x65\x72\x49\x64\x6c\x65\x4e\x6f\x74\x69\x66\x69\x65\x72",
                 StartProfilerIdleNotifier);
  env->SetMethod(process,
                 "\x5f\x73\x74\x6f\x70\x50\x72\x6f\x66\x69\x6c\x65\x72\x49\x64\x6c\x65\x4e\x6f\x74\x69\x66\x69\x65\x72",
                 StopProfilerIdleNotifier);
  env->SetMethod(process, "\x5f\x67\x65\x74\x41\x63\x74\x69\x76\x65\x52\x65\x71\x75\x65\x73\x74\x73", GetActiveRequests);
  env->SetMethod(process, "\x5f\x67\x65\x74\x41\x63\x74\x69\x76\x65\x48\x61\x6e\x64\x6c\x65\x73", GetActiveHandles);
  env->SetMethod(process, "\x72\x65\x61\x6c\x6c\x79\x45\x78\x69\x74", Exit);
  env->SetMethod(process, "\x61\x62\x6f\x72\x74", Abort);
  env->SetMethod(process, "\x63\x68\x64\x69\x72", Chdir);
  env->SetMethod(process, "\x63\x77\x64", Cwd);

  env->SetMethod(process, "\x75\x6d\x61\x73\x6b", Umask);

#if defined(__POSIX__) && !defined(__ANDROID__)
  env->SetMethod(process, "\x67\x65\x74\x75\x69\x64", GetUid);
  env->SetMethod(process, "\x67\x65\x74\x65\x75\x69\x64", GetEUid);
  env->SetMethod(process, "\x73\x65\x74\x75\x69\x64", SetUid);
  env->SetMethod(process, "\x73\x65\x74\x65\x75\x69\x64", SetEUid);

  env->SetMethod(process, "\x73\x65\x74\x67\x69\x64", SetGid);
  env->SetMethod(process, "\x73\x65\x74\x65\x67\x69\x64", SetEGid);
  env->SetMethod(process, "\x67\x65\x74\x67\x69\x64", GetGid);
  env->SetMethod(process, "\x67\x65\x74\x65\x67\x69\x64", GetEGid);

  env->SetMethod(process, "\x67\x65\x74\x67\x72\x6f\x75\x70\x73", GetGroups);
  env->SetMethod(process, "\x73\x65\x74\x67\x72\x6f\x75\x70\x73", SetGroups);
  env->SetMethod(process, "\x69\x6e\x69\x74\x67\x72\x6f\x75\x70\x73", InitGroups);
#endif  // __POSIX__ && !defined(__ANDROID__)

  env->SetMethod(process, "\x5f\x6b\x69\x6c\x6c", Kill);

  env->SetMethod(process, "\x5f\x64\x65\x62\x75\x67\x50\x72\x6f\x63\x65\x73\x73", DebugProcess);
  env->SetMethod(process, "\x5f\x64\x65\x62\x75\x67\x50\x61\x75\x73\x65", DebugPause);
  env->SetMethod(process, "\x5f\x64\x65\x62\x75\x67\x45\x6e\x64", DebugEnd);

  env->SetMethod(process, "\x68\x72\x74\x69\x6d\x65", Hrtime);

  env->SetMethod(process, "\x63\x70\x75\x55\x73\x61\x67\x65", CPUUsage);

  env->SetMethod(process, "\x64\x6c\x6f\x70\x65\x6e", DLOpen);

  env->SetMethod(process, "\x75\x70\x74\x69\x6d\x65", Uptime);
  env->SetMethod(process, "\x6d\x65\x6d\x6f\x72\x79\x55\x73\x61\x67\x65", MemoryUsage);

  env->SetMethod(process, "\x62\x69\x6e\x64\x69\x6e\x67", Binding);
  env->SetMethod(process, "\x5f\x6c\x69\x6e\x6b\x65\x64\x42\x69\x6e\x64\x69\x6e\x67", LinkedBinding);

  env->SetMethod(process, "\x5f\x73\x65\x74\x75\x70\x50\x72\x6f\x63\x65\x73\x73\x4f\x62\x6a\x65\x63\x74", SetupProcessObject);
  env->SetMethod(process, "\x5f\x73\x65\x74\x75\x70\x4e\x65\x78\x74\x54\x69\x63\x6b", SetupNextTick);
  env->SetMethod(process, "\x5f\x73\x65\x74\x75\x70\x50\x72\x6f\x6d\x69\x73\x65\x73", SetupPromises);
  env->SetMethod(process, "\x5f\x73\x65\x74\x75\x70\x44\x6f\x6d\x61\x69\x6e\x55\x73\x65", SetupDomainUse);

  // pre-set _events object for faster emit checks
  Local<Object> events_obj = Object::New(env->isolate());
  CHECK(events_obj->SetPrototype(env->context(),
                                 Null(env->isolate())).FromJust());
  process->Set(env->events_string(), events_obj);
}


#undef READONLY_PROPERTY


static void AtProcessExit() {
  uv_tty_reset_mode();
}


void SignalExit(int signo) {
  uv_tty_reset_mode();
#ifdef __FreeBSD__
  // FreeBSD has a nasty bug, see RegisterSignalHandler for details
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  CHECK_EQ(sigaction(signo, &sa, nullptr), 0);
#endif
#ifdef __MVS__
  signalHandlerExit = signo;
#else
  raise(signo);
#endif
}


// Most of the time, it's best to use `console.error` to write
// to the process.stderr stream.  However, in some cases, such as
// when debugging the stream.Writable class or the process.nextTick
// function, it is useful to bypass JavaScript entirely.
static void RawDebug(const FunctionCallbackInfo<Value>& args) {
  CHECK(args.Length() == 1 && args[0]->IsString() &&
        "\x6d\x75\x73\x74\x20\x62\x65\x20\x63\x61\x6c\x6c\x65\x64\x20\x77\x69\x74\x68\x20\x61\x20\x73\x69\x6e\x67\x6c\x65\x20\x73\x74\x72\x69\x6e\x67");
  node::Utf8Value message(args.GetIsolate(), args[0]);
  PrintErrorString(u8"%s\n", *message);
  fflush(stderr);
}


void LoadEnvironment(Environment* env) {
  HandleScope handle_scope(env->isolate());

  env->isolate()->SetFatalErrorHandler(node::OnFatalError);
  env->isolate()->AddMessageListener(OnMessage);

  atexit(AtProcessExit);

  TryCatch try_catch(env->isolate());

  // Disable verbose mode to stop FatalException() handler from trying
  // to handle the exception. Errors this early in the start-up phase
  // are not safe to ignore.
  try_catch.SetVerbose(false);

  // Execute the lib/internal/bootstrap_node.js file which was included as a
  // static C string in node_natives.h by node_js2c.
  // 'internal_bootstrap_node_native' is the string containing that source code.
  Local<String> script_name = FIXED_ONE_BYTE_STRING(env->isolate(),
                                                    "\x62\x6f\x6f\x74\x73\x74\x72\x61\x70\x5f\x6e\x6f\x64\x65\x2e\x6a\x73");
  Local<Value> f_value = ExecuteString(env, MainSource(env), script_name);
  if (try_catch.HasCaught())  {
    ReportException(env, try_catch);
    platform_exit(10);
  }
  // The bootstrap_node.js file returns a function 'f'
  CHECK(f_value->IsFunction());
  Local<Function> f = Local<Function>::Cast(f_value);

  // Add a reference to the global object
  Local<Object> global = env->context()->Global();

#if defined HAVE_DTRACE || defined HAVE_ETW
  InitDTrace(env, global);
#endif

#if defined HAVE_LTTNG
  InitLTTNG(env, global);
#endif

#if defined HAVE_PERFCTR
  InitPerfCounters(env, global);
#endif

  // Enable handling of uncaught exceptions
  // (FatalException(), break on uncaught exception in debugger)
  //
  // This is not strictly necessary since it's almost impossible
  // to attach the debugger fast enought to break on exception
  // thrown during process startup.
  try_catch.SetVerbose(true);

  env->SetMethod(env->process_object(), "\x5f\x72\x61\x77\x44\x65\x62\x75\x67", RawDebug);

  // Expose the global object as a property on itself
  // (Allows you to set stuff on `global` from anywhere in JavaScript.)
  global->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x67\x6c\x6f\x62\x61\x6c"), global);

  // Now we call 'f' with the 'process' variable that we've built up with
  // all our bindings. Inside bootstrap_node.js and internal/process we'll
  // take care of assigning things to their places.

  // We start the process this way in order to be more modular. Developers
  // who do not like how bootstrap_node.js sets up the module system but do
  // like Node's I/O bindings may want to replace 'f' with their own function.
  Local<Value> arg = env->process_object();
  f->Call(Null(env->isolate()), 1, &arg);
}


void FreeEnvironment(Environment* env) {
  CHECK_NE(env, nullptr);
  env->Dispose();
}


static void PrintHelp();

static bool ParseDebugOpt(const char* arg) {
  const char* port = nullptr;

  if (!strcmp(arg, "\x2d\x2d\x64\x65\x62\x75\x67")) {
    use_debug_agent = true;
  } else if (!strncmp(arg, "\x2d\x2d\x64\x65\x62\x75\x67\x3d", sizeof("\x2d\x2d\x64\x65\x62\x75\x67\x3d") - 1)) {
    use_debug_agent = true;
    port = arg + sizeof("\x2d\x2d\x64\x65\x62\x75\x67\x3d") - 1;
  } else if (!strcmp(arg, "\x2d\x2d\x64\x65\x62\x75\x67\x2d\x62\x72\x6b")) {
    use_debug_agent = true;
    debug_wait_connect = true;
  } else if (!strncmp(arg, "\x2d\x2d\x64\x65\x62\x75\x67\x2d\x62\x72\x6b\x3d", sizeof("\x2d\x2d\x64\x65\x62\x75\x67\x2d\x62\x72\x6b\x3d") - 1)) {
    use_debug_agent = true;
    debug_wait_connect = true;
    port = arg + sizeof("\x2d\x2d\x64\x65\x62\x75\x67\x2d\x62\x72\x6b\x3d") - 1;
  } else if (!strncmp(arg, "\x2d\x2d\x64\x65\x62\x75\x67\x2d\x70\x6f\x72\x74\x3d", sizeof("\x2d\x2d\x64\x65\x62\x75\x67\x2d\x70\x6f\x72\x74\x3d") - 1)) {
    // XXX(bnoordhuis) Misnomer, configures port and listen address.
    port = arg + sizeof("\x2d\x2d\x64\x65\x62\x75\x67\x2d\x70\x6f\x72\x74\x3d") - 1;
#if HAVE_INSPECTOR
  // Specifying both --inspect and --debug means debugging is on, using Chromium
  // inspector.
  } else if (!strcmp(arg, "\x2d\x2d\x69\x6e\x73\x70\x65\x63\x74")) {
    use_debug_agent = true;
    use_inspector = true;
  } else if (!strncmp(arg, "\x2d\x2d\x69\x6e\x73\x70\x65\x63\x74\x3d", sizeof("\x2d\x2d\x69\x6e\x73\x70\x65\x63\x74\x3d") - 1)) {
    use_debug_agent = true;
    use_inspector = true;
    port = arg + sizeof("--inspect=") - 1;
  } else if (!strcmp(arg, "\x2d\x2d\x69\x6e\x73\x70\x65\x63\x74\x2d\x62\x72\x6b")) {
    use_debug_agent = true;
    use_inspector = true;
    debug_wait_connect = true;
  } else if (!strncmp(arg, "\x2d\x2d\x69\x6e\x73\x70\x65\x63\x74\x2d\x62\x72\x6b\x3d", sizeof("--inspect-brk=") - 1)) {
    use_debug_agent = true;
    use_inspector = true;
    debug_wait_connect = true;
    port = arg + sizeof("--inspect-brk=") - 1;
#else
  } else if (!strncmp(arg, "\x2d\x2d\x69\x6e\x73\x70\x65\x63\x74", sizeof("\x2d\x2d\x69\x6e\x73\x70\x65\x63\x74") - 1)) {
    PrintErrorString(
            u8"Inspector support is not available with this Node.js build\n");
    return false;
#endif
  } else {
    return false;
  }

  if (port == nullptr) {
    return true;
  }

  std::string** const the_host = use_inspector ? &inspector_host : &debug_host;
  int* const the_port = use_inspector ? &inspector_port : &debug_port;

  // FIXME(bnoordhuis) Move IPv6 address parsing logic to lib/net.js.
  // It seems reasonable to support [address]:port notation
  // in net.Server#listen() and net.Socket#connect().
  const size_t port_len = strlen(port);
  if (port[0] == '\x5b' && port[port_len - 1] == '\x5d') {
    *the_host = new std::string(port + 1, port_len - 2);
    return true;
  }

  const char* const colon = strrchr(port, '\x3a');
  if (colon == nullptr) {
    // Either a port number or a host name.  Assume that
    // if it's not all decimal digits, it's a host name.
    for (size_t n = 0; port[n] != '\x0'; n += 1) {
      if (port[n] < '\x30' || port[n] > '\x39') {
        *the_host = new std::string(port);
        return true;
      }
    }
  } else {
    const bool skip = (colon > port && port[0] == '\x5b' && colon[-1] == '\x5d');
    *the_host = new std::string(port + skip, colon - skip);
  }

  char* endptr;
  errno = 0;
  const char* const digits = colon != nullptr ? colon + 1 : port;
#ifdef __MVS__
  const long result = __strtol_a(digits, &endptr, 10);  // NOLINT(runtime/int)
#else
  const long result = strtol(digits, &endptr, 10);  // NOLINT(runtime/int)
#endif
  if (errno != 0 || *endptr != '\x0' || result < 1024 || result > 65535) {
    PrintErrorString(u8"Debug port must be in range 1024 to 65535.\n");
    PrintHelp();
    platform_exit(12);
  }

  *the_port = static_cast<int>(result);

  return true;
}

static void PrintHelp() {
  // XXX: If you add an option here, please also add it to doc/node.1 and
  // doc/api/cli.md
  PrintOutString(u8"Usage: node [options] [ -e script | script.js ] [arguments] \n"
         u8"       node debug script.js [arguments] \n"
         u8"\n"
         u8"Options:\n"
         u8"  -v, --version         print Node.js version\n"
         u8"  -e, --eval script     evaluate script\n"
         u8"  -p, --print           evaluate script and print result\n"
         u8"  -c, --check           syntax check script without executing\n"
         u8"  -i, --interactive     always enter the REPL even if stdin\n"
         u8"                        does not appear to be a terminal\n"
         u8"  -r, --require         module to preload (option can be repeated)\n"
         u8"  --insecure-http-parser     use an insecure HTTP parser that\n"
         u8"                             accepts invalid HTTP headers\n"
         u8"  --no-deprecation      silence deprecation warnings\n"
         u8"  --trace-deprecation   show stack traces on deprecations\n"
         u8"  --throw-deprecation   throw an exception anytime a deprecated "
         u8"function is used\n"
         u8"  --max-http-header-size     Specify the maximum size of HTTP\n"
         u8"                             headers in bytes. Defaults to 8KB.\n"
         u8"  --no-warnings         silence all process warnings\n"
         u8"  --napi-modules        load N-API modules (no-op - option kept for\n"
         u8"                        compatibility)\n"
         u8"  --trace-warnings      show stack traces on process warnings\n"
         u8"  --redirect-warnings=path\n"
         u8"                        write warnings to path instead of stderr\n"
         u8"  --trace-sync-io       show stack trace when use of sync IO\n"
         u8"                        is detected after the first tick\n"
         u8"  --track-heap-objects  track heap object allocations for heap "
         u8"snapshots\n"
         u8"  --prof-process        process v8 profiler output generated\n"
         u8"                        using --prof\n"
         u8"  --zero-fill-buffers   automatically zero-fill all newly allocated\n"
         u8"                        Buffer and SlowBuffer instances\n"
         u8"  --v8-options          print v8 command line options\n"
         u8"  --v8-pool-size=num    set v8's thread pool size\n"
#if HAVE_OPENSSL
         u8"  --tls-cipher-list=val    use an alternative default TLS cipher "
         u8"list\n"
         u8"  --use-bundled-ca         use bundled CA store"
#if !defined(NODE_OPENSSL_CERT_STORE)
         u8" (default)"
#endif
         u8"\n"
         u8"  --use-openssl-ca         use OpenSSL's default CA store"
#if defined(NODE_OPENSSL_CERT_STORE)
         u8" (default)"
#endif
         u8"\n"
#if NODE_FIPS_MODE
         u8"  --enable-fips         enable FIPS crypto at startup\n"
         u8"  --force-fips          force FIPS crypto (cannot be disabled)\n"
#endif  /* NODE_FIPS_MODE */
         u8"  --openssl-config=path load OpenSSL configuration file from the\n"
         u8"                        specified file (overrides OPENSSL_CONF)\n"
#endif /* HAVE_OPENSSL */
#if defined(NODE_HAVE_I18N_SUPPORT)
         u8"  --icu-data-dir=dir    set ICU data load path to dir\n"
         u8"                        (overrides NODE_ICU_DATA)\n"
#if !defined(NODE_HAVE_SMALL_ICU)
         u8"                        note: linked-in ICU data is\n"
         u8"                        present.\n"
#endif
         u8"  --preserve-symlinks   preserve symbolic links when resolving\n"
         u8"                        and caching modules.\n"
#endif
         u8"\n"
         u8"Environment variables:\n"
#ifdef _WIN32
         u8"NODE_PATH                ';'-separated list of directories\n"
#else
         u8"NODE_PATH                ':'-separated list of directories\n"
#endif
         u8"                         prefixed to the module search path.\n"
         u8"NODE_DISABLE_COLORS      set to 1 to disable colors in the REPL\n"
#if defined(NODE_HAVE_I18N_SUPPORT)
         u8"NODE_ICU_DATA            data path for ICU (Intl object) data\n"
#if !defined(NODE_HAVE_SMALL_ICU)
         u8"                         (will extend linked-in data)\n"
#endif
#endif
         u8"NODE_NO_WARNINGS           set to 1 to silence process warnings\n"
#if !defined(NODE_WITHOUT_NODE_OPTIONS)
         u8"NODE_OPTIONS            set CLI options in the environment\n"
#endif
#ifdef _WIN32
         "NODE_PATH               ';'-separated list of directories\n"
#else
         u8"NODE_PATH               ':'-separated list of directories\n"
#endif
         u8"                        prefixed to the module search path\n"
         u8"NODE_PENDING_DEPRECATION     set to 1 to emit pending deprecation\n"
         u8"                             warnings\n"
         u8"NODE_REPL_HISTORY       path to the persistent REPL history file\n"
         u8"OPENSSL_CONF            load OpenSSL configuration from file\n"
         u8"NODE_REDIRECT_WARNINGS  write warnings to path instead of stderr\n"
         u8"\n"
         u8"Documentation can be found at https://nodejs.org/\n"
#ifdef NODE_TAG
         NODE_EXE_VERSION      
         u8"\n"
         );
#else
         );
#endif
}


static bool ArgIsAllowed(const char* arg, const char* allowed) {
  for (; *arg && *allowed; arg++, allowed++) {
    // Like normal strcmp(), except that a '_' in `allowed` matches either a '-'
    // or '_' in `arg`.
    if (*allowed == '\x5f') {
      if (!(*arg == '\x5f' || *arg == '\x2d'))
        return false;
    } else {
      if (*arg != *allowed)
        return false;
    }
  }

  // "--some-arg=val" is allowed for "--some-arg"
  if (*arg == '\x3d')
    return true;

  // Both must be null, or one string is just a prefix of the other, not a
  // match.
  return !*arg && !*allowed;
}


static void CheckIfAllowedInEnv(const char* exe, bool is_env,
                                const char* arg) {
  if (!is_env)
    return;

  // Find the arg prefix when its --some_arg=val
  const char* eq = strchr(arg, '\x3d');
  size_t arglen = eq ? eq - arg : strlen(arg);

  static const char* whitelist[] = {
    // Node options, sorted in `node --help` order for ease of comparison.
    u8"--require", u8"-r",
    u8"--insecure-http-parser",
    u8"--debug",
    u8"--debug-brk",
    u8"--debug-port",
    u8"--no-deprecation",
    u8"--trace-deprecation",
    u8"--throw-deprecation",
    u8"--pending-deprecation",
    u8"--no-warnings",
    u8"--napi-modules",
    u8"--max-http-header-size",
    u8"--trace-warnings",
    u8"--redirect-warnings",
    u8"--trace-sync-io",
    u8"--track-heap-objects",
    u8"--zero-fill-buffers",
    u8"--v8-pool-size",
    u8"--tls-cipher-list",
    u8"--use-bundled-ca",
    u8"--use-openssl-ca",
    u8"--enable-fips",
    u8"--force-fips",
    u8"--openssl-config",
    u8"--icu-data-dir",

    // V8 options (define with '_', which allows '-' or '_')
    u8"--abort_on_uncaught_exception",
    u8"--max_old_space_size",
  };

  for (unsigned i = 0; i < arraysize(whitelist); i++) {
    const char* allowed = whitelist[i];
    if (ArgIsAllowed(arg, allowed))
      return;
  }

  PrintErrorString(u8"%s: %s is not allowed in NODE_OPTIONS\n", exe, arg);
  platform_exit(9);
}


// Parse command line arguments.
//
// argv is modified in place. exec_argv and v8_argv are out arguments that
// ParseArgs() allocates memory for and stores a pointer to the output
// vector in.  The caller should free them with delete[].
//
// On exit:
//
//  * argv contains the arguments with node and V8 options filtered out.
//  * exec_argv contains both node and V8 options and nothing else.
//  * v8_argv contains argv[0] plus any V8 options
static void ParseArgs(int* argc,
                      const char** argv,
                      int* exec_argc,
                      const char*** exec_argv,
                      int* v8_argc,
                      const char*** v8_argv,
                      bool is_env) {
  const unsigned int nargs = static_cast<unsigned int>(*argc);
  const char** new_exec_argv = new const char*[nargs];
  const char** new_v8_argv = new const char*[nargs];
  const char** new_argv = new const char*[nargs];
#if HAVE_OPENSSL
  bool use_bundled_ca = false;
  bool use_openssl_ca = false;
#endif  // HAVE_OPENSSL

  for (unsigned int i = 0; i < nargs; ++i) {
    new_exec_argv[i] = nullptr;
    new_v8_argv[i] = nullptr;
    new_argv[i] = nullptr;
  }

  // exec_argv starts with the first option, the other two start with argv[0].
  unsigned int new_exec_argc = 0;
  unsigned int new_v8_argc = 1;
  unsigned int new_argc = 1;
  new_v8_argv[0] = argv[0];
  new_argv[0] = argv[0];

  unsigned int index = 1;
  bool short_circuit = false;
  while (index < nargs && argv[index][0] == '\x2d' && !short_circuit) {
    const char* const arg = argv[index];
    unsigned int args_consumed = 1;

    CheckIfAllowedInEnv(argv[0], is_env, arg);

    if (ParseDebugOpt(arg)) {
      // Done, consumed by ParseDebugOpt().
    } else if (strcmp(arg, u8"--version") == 0 || strcmp(arg, "-v") == 0) {
      PrintOutString(u8"%s\n", NODE_VERSION);
      platform_exit(0);
    } else if (strcmp(arg, "\x2d\x2d\x68\x65\x6c\x70") == 0 || strcmp(arg, "\x2d\x68") == 0) {
      PrintHelp();
      platform_exit(0);
    } else if (strcmp(arg, "\x2d\x2d\x65\x76\x61\x6c") == 0 ||
               strcmp(arg, "\x2d\x65") == 0 ||
               strcmp(arg, "\x2d\x2d\x70\x72\x69\x6e\x74") == 0 ||
               strcmp(arg, "\x2d\x70\x65") == 0 ||
               strcmp(arg, "\x2d\x70") == 0) {
      bool is_eval = strchr(arg, '\x65') != nullptr;
      bool is_print = strchr(arg, '\x70') != nullptr;
      print_eval = print_eval || is_print;
      // --eval, -e and -pe always require an argument.
      if (is_eval == true) {
        args_consumed += 1;
        eval_string = argv[index + 1];
        if (eval_string == nullptr) {
          PrintErrorString(u8"%s: %s requires an argument\n", argv[0], arg);
          platform_exit(9);
        }
      } else if ((index + 1 < nargs) &&
                 argv[index + 1] != nullptr &&
                 argv[index + 1][0] != '\x2d') {
        args_consumed += 1;
        eval_string = argv[index + 1];
        if (strncmp(eval_string, "\x5c\x2d", 2) == 0) {
          // Starts with "\\-": escaped expression, drop the backslash.
          eval_string += 1;
        }
      }
    } else if (strcmp(arg, "\x2d\x2d\x72\x65\x71\x75\x69\x72\x65") == 0 ||
               strcmp(arg, "\x2d\x72") == 0) {
      const char* module = argv[index + 1];
      if (module == nullptr) {
        PrintErrorString(u8"%s: %s requires an argument\n", argv[0], arg);
        platform_exit(9);
      }
      args_consumed += 1;
      preload_modules.push_back(module);
    } else if (strcmp(arg, "\x2d\x2d\x63\x68\x65\x63\x6b") == 0 || strcmp(arg, "\x2d\x63") == 0) {
      syntax_check_only = true;
    } else if (strcmp(arg, "\x2d\x2d\x69\x6e\x74\x65\x72\x61\x63\x74\x69\x76\x65") == 0 || strcmp(arg, "\x2d\x69") == 0) {
      force_repl = true;
    } else if (strcmp(arg, "\x2d\x2d\x69\x6e\x73\x65\x63\x75\x72\x65\x2d\x68\x74\x74\x70\x2d\x70\x61\x72\x73\x65\x72") == 0) {
      insecure_http_parser = true;
    } else if (strcmp(arg, "\x2d\x2d\x6e\x6f\x2d\x64\x65\x70\x72\x65\x63\x61\x74\x69\x6f\x6e") == 0) {
      no_deprecation = true;
    } else if (strcmp(arg, "\x2d\x2d\x6e\x61\x70\x69\x2d\x6d\x6f\x64\x75\x6c\x65\x73") == 0) {
      // no-op
    } else if (strcmp(arg, "\x2d\x2d\x6e\x6f\x2d\x77\x61\x72\x6e\x69\x6e\x67\x73") == 0) {
      no_process_warnings = true;
    } else if (strcmp(arg, "\x2d\x2d\x74\x72\x61\x63\x65\x2d\x77\x61\x72\x6e\x69\x6e\x67\x73") == 0) {
      trace_warnings = true;
    } else if (strncmp(arg, "\x2d\x2d\x72\x65\x64\x69\x72\x65\x63\x74\x2d\x77\x61\x72\x6e\x69\x6e\x67\x73\x3d", 20) == 0) {
      config_warning_file = arg + 20;
    } else if (strcmp(arg, "\x2d\x2d\x74\x72\x61\x63\x65\x2d\x64\x65\x70\x72\x65\x63\x61\x74\x69\x6f\x6e") == 0) {
      trace_deprecation = true;
    } else if (strcmp(arg, "\x2d\x2d\x74\x72\x61\x63\x65\x2d\x73\x79\x6e\x63\x2d\x69\x6f") == 0) {
      trace_sync_io = true;
    } else if (strcmp(arg, "\x2d\x2d\x74\x72\x61\x63\x6b\x2d\x68\x65\x61\x70\x2d\x6f\x62\x6a\x65\x63\x74\x73") == 0) {
      track_heap_objects = true;
    } else if (strcmp(arg, "\x2d\x2d\x74\x68\x72\x6f\x77\x2d\x64\x65\x70\x72\x65\x63\x61\x74\x69\x6f\x6e") == 0) {
      throw_deprecation = true;
    } else if (strncmp(arg, "\x2d\x2d\x73\x65\x63\x75\x72\x69\x74\x79\x2d\x72\x65\x76\x65\x72\x74\x3d", 18) == 0) {
      const char* cve = arg + 18;
      Revert(cve);
    } else if (strcmp(arg, "\x2d\x2d\x70\x72\x65\x73\x65\x72\x76\x65\x2d\x73\x79\x6d\x6c\x69\x6e\x6b\x73") == 0) {
      config_preserve_symlinks = true;
    } else if (strcmp(arg, "\x2d\x2d\x70\x72\x6f\x66\x2d\x70\x72\x6f\x63\x65\x73\x73") == 0) {
      prof_process = true;
      short_circuit = true;
    } else if (strcmp(arg, "\x2d\x2d\x7a\x65\x72\x6f\x2d\x66\x69\x6c\x6c\x2d\x62\x75\x66\x66\x65\x72\x73") == 0) {
      zero_fill_all_buffers = true;
    } else if (strcmp(arg, "\x2d\x2d\x76\x38\x2d\x6f\x70\x74\x69\x6f\x6e\x73") == 0) {
      new_v8_argv[new_v8_argc] = "\x2d\x2d\x68\x65\x6c\x70";
      new_v8_argc += 1;
    } else if (strncmp(arg, "\x2d\x2d\x76\x38\x2d\x70\x6f\x6f\x6c\x2d\x73\x69\x7a\x65\x3d", 15) == 0) {
#ifdef __MVS__
      char* temp_arg = strdup(argv[index]);
      __a2e_s(temp_arg);
      v8_thread_pool_size = atoi(temp_arg + 15);
      free(temp_arg);
#else
      v8_thread_pool_size = atoi(arg + 15);
#endif
    } else if (strncmp(arg, "\x2d\x2d\x6d\x61\x78\x2d\x68\x74\x74\x70\x2d\x68\x65\x61\x64\x65\x72\x2d\x73\x69\x7a\x65\x3d", 23) == 0) {
#ifdef __MVS__
      char* temp_arg = strdup(argv[index]);
      __a2e_s(temp_arg);
      max_http_header_size = atoi(temp_arg + 23);
      free(temp_arg);
#else
      max_http_header_size = atoi(arg + 23);
#endif
#if HAVE_OPENSSL
    } else if (strncmp(arg, "\x2d\x2d\x74\x6c\x73\x2d\x63\x69\x70\x68\x65\x72\x2d\x6c\x69\x73\x74\x3d", 18) == 0) {
      default_cipher_list = arg + 18;
    } else if (strncmp(arg, u8"--use-openssl-ca", 16) == 0) {
      ssl_openssl_cert_store = true;
      use_openssl_ca = true;
    } else if (strncmp(arg, u8"--use-bundled-ca", 16) == 0) {
      use_bundled_ca = true;
      ssl_openssl_cert_store = false;
#if NODE_FIPS_MODE
    } else if (strcmp(arg, "\x2d\x2d\x65\x6e\x61\x62\x6c\x65\x2d\x66\x69\x70\x73") == 0) {
      enable_fips_crypto = true;
    } else if (strcmp(arg, "\x2d\x2d\x66\x6f\x72\x63\x65\x2d\x66\x69\x70\x73") == 0) {
      force_fips_crypto = true;
#endif /* NODE_FIPS_MODE */
    } else if (strncmp(arg, "\x2d\x2d\x6f\x70\x65\x6e\x73\x73\x6c\x2d\x63\x6f\x6e\x66\x69\x67\x3d", 17) == 0) {
      openssl_config.assign(arg + 17);
#endif /* HAVE_OPENSSL */
#if defined(NODE_HAVE_I18N_SUPPORT)
    } else if (strncmp(arg, "\x2d\x2d\x69\x63\x75\x2d\x64\x61\x74\x61\x2d\x64\x69\x72\x3d", 15) == 0) {
      icu_data_dir.assign(arg + 15);
#endif
    } else if (strcmp(arg, "\x2d\x2d\x65\x78\x70\x6f\x73\x65\x2d\x69\x6e\x74\x65\x72\x6e\x61\x6c\x73") == 0 ||
               strcmp(arg, "\x2d\x2d\x65\x78\x70\x6f\x73\x65\x5f\x69\x6e\x74\x65\x72\x6e\x61\x6c\x73") == 0) {
      config_expose_internals = true;
    } else if (strcmp(arg, u8"--") == 0) {
      index += 1;
      break;
    } else {
      // V8 option.  Pass through as-is.
      new_v8_argv[new_v8_argc] = arg;
      new_v8_argc += 1;
    }

    memcpy(new_exec_argv + new_exec_argc,
           argv + index,
           args_consumed * sizeof(*argv));

    new_exec_argc += args_consumed;
    index += args_consumed;
  }

#if HAVE_OPENSSL
  if (use_openssl_ca && use_bundled_ca) {
    PrintErrorString(
            u8"%s: either --use-openssl-ca or --use-bundled-ca can be used, "
            "not both\n",
            argv[0]);
    platform_exit(9);
  }
#endif

  // Copy remaining arguments.
  const unsigned int args_left = nargs - index;

  if (is_env && args_left) {
    PrintErrorString(u8"%s: %s is not supported in NODE_OPTIONS\n",
            argv[0], argv[index]);
    platform_exit(9);
  }

  memcpy(new_argv + new_argc, argv + index, args_left * sizeof(*argv));
  new_argc += args_left;

  *exec_argc = new_exec_argc;
  *exec_argv = new_exec_argv;
  *v8_argc = new_v8_argc;
  *v8_argv = new_v8_argv;

  // Copy new_argv over argv and update argc.
  memcpy(argv, new_argv, new_argc * sizeof(*argv));
  delete[] new_argv;
  *argc = static_cast<int>(new_argc);
}


// Called from V8 Debug Agent TCP thread.
static void DispatchMessagesDebugAgentCallback(Environment* env) {
  // TODO(indutny): move async handle to environment
  uv_async_send(&dispatch_debug_messages_async);
}


static void StartDebug(Environment* env, const char* path, bool wait) {
  CHECK(!debugger_running);
  if (use_inspector) {
    const char* host = inspector_host ? inspector_host->c_str() : "127.0.0.1";
    debugger_running = v8_platform.StartInspector(env, path,
                                                  host,
                                                  inspector_port, wait);
  } else {
    env->debugger_agent()->set_dispatch_handler(
          DispatchMessagesDebugAgentCallback);
    const char* host = debug_host ? debug_host->c_str() : "127.0.0.1";
    debugger_running =
        env->debugger_agent()->Start(host, debug_port, wait);
    if (debugger_running == false) {
      PrintErrorString(u8"Starting debugger on %s:%d failed\n",
              host, debug_port);
      fflush(stderr);
      return;
    }
  }
}


// Called from the main thread.
static void EnableDebug(Environment* env) {
  CHECK(debugger_running);

  if (use_inspector) {
    return;
  }

  // Send message to enable debug in workers
  HandleScope handle_scope(env->isolate());

  Local<Object> message = Object::New(env->isolate());
  message->Set(FIXED_ONE_BYTE_STRING(env->isolate(), "\x63\x6d\x64"),
               FIXED_ONE_BYTE_STRING(env->isolate(), "\x4e\x4f\x44\x45\x5f\x44\x45\x42\x55\x47\x5f\x45\x4e\x41\x42\x4c\x45\x44"));
  Local<Value> argv[] = {
    FIXED_ONE_BYTE_STRING(env->isolate(), "\x69\x6e\x74\x65\x72\x6e\x61\x6c\x4d\x65\x73\x73\x61\x67\x65"),
    message
  };
  MakeCallback(env, env->process_object(), "\x65\x6d\x69\x74", arraysize(argv), argv);

  // Enabled debugger, possibly making it wait on a semaphore
  env->debugger_agent()->Enable();
}


// Called from an arbitrary thread.
static void TryStartDebugger() {
  Mutex::ScopedLock scoped_lock(node_isolate_mutex);
  if (auto isolate = node_isolate) {
    v8::Debug::DebugBreak(isolate);
    uv_async_send(&dispatch_debug_messages_async);
  }
}


// Called from the main thread.
static void DispatchDebugMessagesAsyncCallback(uv_async_t* handle) {
  Mutex::ScopedLock scoped_lock(node_isolate_mutex);
  if (auto isolate = node_isolate) {
    if (debugger_running == false) {
      fprintf(stderr, "Starting debugger agent.\n");

      HandleScope scope(isolate);
      Environment* env = Environment::GetCurrent(isolate);
      Context::Scope context_scope(env->context());

      StartDebug(env, nullptr, false);
      EnableDebug(env);
    }

    Isolate::Scope isolate_scope(isolate);
    v8::Debug::ProcessDebugMessages(isolate);
  }
}


#ifdef __POSIX__
static void EnableDebugSignalHandler(int signo) {
  uv_sem_post(&debug_semaphore);
}


void RegisterSignalHandler(int signal,
                           void (*handler)(int signal),
                           bool reset_handler) {
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = handler;
#ifndef __FreeBSD__
  // FreeBSD has a nasty bug with SA_RESETHAND reseting the SA_SIGINFO, that is
  // in turn set for a libthr wrapper. This leads to a crash.
  // Work around the issue by manually setting SIG_DFL in the signal handler
  sa.sa_flags = reset_handler ? SA_RESETHAND : 0;
#endif
  sigfillset(&sa.sa_mask);
  CHECK_EQ(sigaction(signal, &sa, nullptr), 0);
}


void DebugProcess(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);

  if (args.Length() != 1) {
    return env->ThrowError("\x49\x6e\x76\x61\x6c\x69\x64\x20\x6e\x75\x6d\x62\x65\x72\x20\x6f\x66\x20\x61\x72\x67\x75\x6d\x65\x6e\x74\x73\x2e");
  }

  pid_t pid;
  int r;

  pid = args[0]->IntegerValue();
  r = kill(pid, SIGUSR1);
  if (r != 0) {
    return env->ThrowErrnoException(errno, "\x6b\x69\x6c\x6c");
  }
}


inline void* DebugSignalThreadMain(void* unused) {
  for (;;) {
    uv_sem_wait(&debug_semaphore);
    if (debug_semaphore_active == false)
      break;
    TryStartDebugger();
  }

  return nullptr;
}


static void StopDebugSignalHandler() {
  if (debug_semaphore_active == true) {
    debug_semaphore_active = false;
    uv_sem_post(&debug_semaphore);
  }
}


static int RegisterDebugSignalHandler() {
  // Start a watchdog thread for calling v8::Debug::DebugBreak() because
  // it's not safe to call directly from the signal handler, it can
  // deadlock with the thread it interrupts.
  CHECK_EQ(0, uv_sem_init(&debug_semaphore, 0));
  debug_semaphore_active = true;
  pthread_attr_t attr;
  CHECK_EQ(0, pthread_attr_init(&attr));
  // Don't shrink the thread's stack on FreeBSD.  Said platform decided to
  // follow the pthreads specification to the letter rather than in spirit:
  // https://lists.freebsd.org/pipermail/freebsd-current/2014-March/048885.html
#ifndef __FreeBSD__
  CHECK_EQ(0, pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN));
#endif  // __FreeBSD__
  CHECK_EQ(0, pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
  sigset_t sigmask;
  sigfillset(&sigmask);
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, &sigmask));
  pthread_t thread;
  const int err =
      pthread_create(&thread, &attr, DebugSignalThreadMain, nullptr);
  CHECK_EQ(0, pthread_sigmask(SIG_SETMASK, &sigmask, nullptr));
  CHECK_EQ(0, pthread_attr_destroy(&attr));
  if (err != 0) {
    fprintf(stderr, "node[%d]: pthread_create: %s\n", getpid(), strerror(err));
    fflush(stderr);
    // Leave SIGUSR1 blocked.  We don't install a signal handler,
    // receiving the signal would terminate the process.
    return -err;
  }
  RegisterSignalHandler(SIGUSR1, EnableDebugSignalHandler);
  // Unblock SIGUSR1.  A pending SIGUSR1 signal will now be delivered.
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGUSR1);
  CHECK_EQ(0, pthread_sigmask(SIG_UNBLOCK, &sigmask, nullptr));
  return 0;
}
#endif  // __POSIX__


#ifdef _WIN32
DWORD WINAPI EnableDebugThreadProc(void* arg) {
  TryStartDebugger();
  return 0;
}


static int GetDebugSignalHandlerMappingName(DWORD pid, wchar_t* buf,
    size_t buf_len) {
  return _snwprintf(buf, buf_len, L"\x6e\x6f\x64\x65\x2d\x64\x65\x62\x75\x67\x2d\x68\x61\x6e\x64\x6c\x65\x72\x2d\x6c\xa4", pid);
}


static int RegisterDebugSignalHandler() {
  wchar_t mapping_name[32];
  HANDLE mapping_handle;
  DWORD pid;
  LPTHREAD_START_ROUTINE* handler;

  pid = GetCurrentProcessId();

  if (GetDebugSignalHandlerMappingName(pid,
                                       mapping_name,
                                       arraysize(mapping_name)) < 0) {
    return -1;
  }

  mapping_handle = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                      nullptr,
                                      PAGE_READWRITE,
                                      0,
                                      sizeof *handler,
                                      mapping_name);
  if (mapping_handle == nullptr) {
    return -1;
  }

  handler = reinterpret_cast<LPTHREAD_START_ROUTINE*>(
      MapViewOfFile(mapping_handle,
                    FILE_MAP_ALL_ACCESS,
                    0,
                    0,
                    sizeof *handler));
  if (handler == nullptr) {
    CloseHandle(mapping_handle);
    return -1;
  }

  *handler = EnableDebugThreadProc;

  UnmapViewOfFile(static_cast<void*>(handler));

  return 0;
}


static void DebugProcess(const FunctionCallbackInfo<Value>& args) {
  Environment* env = Environment::GetCurrent(args);
  Isolate* isolate = args.GetIsolate();
  DWORD pid;
  HANDLE process = nullptr;
  HANDLE thread = nullptr;
  HANDLE mapping = nullptr;
  wchar_t mapping_name[32];
  LPTHREAD_START_ROUTINE* handler = nullptr;

  if (args.Length() != 1) {
    env->ThrowError("\x49\x6e\x76\x61\x6c\x69\x64\x20\x6e\x75\x6d\x62\x65\x72\x20\x6f\x66\x20\x61\x72\x67\x75\x6d\x65\x6e\x74\x73\x2e");
    goto out;
  }

  pid = (DWORD) args[0]->IntegerValue();

  process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                            PROCESS_VM_OPERATION | PROCESS_VM_WRITE |
                            PROCESS_VM_READ,
                        FALSE,
                        pid);
  if (process == nullptr) {
    isolate->ThrowException(
        WinapiErrnoException(isolate, GetLastError(), "\x4f\x70\x65\x6e\x50\x72\x6f\x63\x65\x73\x73"));
    goto out;
  }

  if (GetDebugSignalHandlerMappingName(pid,
                                       mapping_name,
                                       arraysize(mapping_name)) < 0) {
    env->ThrowErrnoException(errno, "\x73\x70\x72\x69\x6e\x74\x66");
    goto out;
  }

  mapping = OpenFileMappingW(FILE_MAP_READ, FALSE, mapping_name);
  if (mapping == nullptr) {
    isolate->ThrowException(WinapiErrnoException(isolate,
                                             GetLastError(),
                                             "\x4f\x70\x65\x6e\x46\x69\x6c\x65\x4d\x61\x70\x70\x69\x6e\x67\x57"));
    goto out;
  }

  handler = reinterpret_cast<LPTHREAD_START_ROUTINE*>(
      MapViewOfFile(mapping,
                    FILE_MAP_READ,
                    0,
                    0,
                    sizeof *handler));
  if (handler == nullptr || *handler == nullptr) {
    isolate->ThrowException(
        WinapiErrnoException(isolate, GetLastError(), "\x4d\x61\x70\x56\x69\x65\x77\x4f\x66\x46\x69\x6c\x65"));
    goto out;
  }

  thread = CreateRemoteThread(process,
                              nullptr,
                              0,
                              *handler,
                              nullptr,
                              0,
                              nullptr);
  if (thread == nullptr) {
    isolate->ThrowException(WinapiErrnoException(isolate,
                                                 GetLastError(),
                                                 "\x43\x72\x65\x61\x74\x65\x52\x65\x6d\x6f\x74\x65\x54\x68\x72\x65\x61\x64"));
    goto out;
  }

  // Wait for the thread to terminate
  if (WaitForSingleObject(thread, INFINITE) != WAIT_OBJECT_0) {
    isolate->ThrowException(WinapiErrnoException(isolate,
                                                 GetLastError(),
                                                 "\x57\x61\x69\x74\x46\x6f\x72\x53\x69\x6e\x67\x6c\x65\x4f\x62\x6a\x65\x63\x74"));
    goto out;
  }

 out:
  if (process != nullptr)
    CloseHandle(process);
  if (thread != nullptr)
    CloseHandle(thread);
  if (handler != nullptr)
    UnmapViewOfFile(handler);
  if (mapping != nullptr)
    CloseHandle(mapping);
}
#endif  // _WIN32


static void DebugPause(const FunctionCallbackInfo<Value>& args) {
  v8::Debug::DebugBreak(args.GetIsolate());
}


static void DebugEnd(const FunctionCallbackInfo<Value>& args) {
  if (debugger_running) {
    Environment* env = Environment::GetCurrent(args);
#if HAVE_INSPECTOR
    if (use_inspector) {
      env->inspector_agent()->Stop();
    } else {
#endif
      env->debugger_agent()->Stop();
#if HAVE_INSPECTOR
    }
#endif
    debugger_running = false;
  }
}


void SignalHandlerThread(void* data) {
  struct sigaction sa;
  int old;

  CHECK_EQ(pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, &old), 0);
  CHECK_EQ(pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &old),0);

  while(!signalHandlerExit) {
    CHECK_EQ(pause(),-1);
    CHECK_EQ(errno,EINTR);
  }
  OS390ThreadManager::Destroy(signalHandlerExit);
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = SIG_DFL;
  CHECK_EQ(sigaction(signalHandlerExit, &sa, nullptr), 0);
  if (signalHandlerExit == SIGABRT)
    abort();
  raise(signalHandlerExit);
}


inline void PlatformInit() {
#ifdef __POSIX__
  sigset_t sigmask;
  sigemptyset(&sigmask);
  sigaddset(&sigmask, SIGUSR1);
  const int err = pthread_sigmask(SIG_SETMASK, &sigmask, nullptr);

  // Make sure file descriptors 0-2 are valid before we start logging anything.
  for (int fd = STDIN_FILENO; fd <= STDERR_FILENO; fd += 1) {
    struct stat ignored;
    if (fstat(fd, &ignored) == 0)
      continue;
    // Anything but EBADF means something is seriously wrong.  We don't
    // have to special-case EINTR, fstat() is not interruptible.
    if (errno != EBADF)
      ABORT();
    if (fd != open("/dev/null", O_RDWR))
      ABORT();
  }

  CHECK_EQ(err, 0);

#ifndef NODE_SHARED_MODE
  // Restore signal dispositions, the parent process may have changed them.
  struct sigaction act;
  memset(&act, 0, sizeof(act));

  // The hard-coded upper limit is because NSIG is not very reliable; on Linux,
  // it evaluates to 32, 34 or 64, depending on whether RT signals are enabled.
  // Counting up to SIGRTMIN doesn't work for the same reason.
  for (unsigned nr = 1; nr < kMaxSignal; nr += 1) {
    if (nr == SIGKILL || nr == SIGSTOP)
      continue;
    act.sa_handler = (nr == SIGPIPE) ? SIG_IGN : SIG_DFL;
    CHECK_EQ(0, sigaction(nr, &act, nullptr));
  }
#endif  // !NODE_SHARED_MODE

  RegisterSignalHandler(SIGINT, SignalExit, true);
  RegisterSignalHandler(SIGTERM, SignalExit, true);
  RegisterSignalHandler(SIGABRT, SignalExit, true);

  // Raise the open file descriptor limit.
  struct rlimit lim;
  if (getrlimit(RLIMIT_NOFILE, &lim) == 0 && lim.rlim_cur != lim.rlim_max) {
    // Do a binary search for the limit.
    rlim_t min = lim.rlim_cur;
    rlim_t max = 1 << 20;
    // But if there's a defined upper bound, don't search, just set it.
    if (lim.rlim_max != RLIM_INFINITY) {
      min = lim.rlim_max;
      max = lim.rlim_max;
    }
    do {
      lim.rlim_cur = min + (max - min) / 2;
      if (setrlimit(RLIMIT_NOFILE, &lim)) {
        max = lim.rlim_cur;
      } else {
        min = lim.rlim_cur;
      }
    } while (min + 1 < max);
  }
#endif  // __POSIX__
#ifdef _WIN32
  for (int fd = 0; fd <= 2; ++fd) {
    auto handle = reinterpret_cast<HANDLE>(_get_osfhandle(fd));
    if (handle == INVALID_HANDLE_VALUE ||
        GetFileType(handle) == FILE_TYPE_UNKNOWN) {
      // Ignore _close result. If it fails or not depends on used Windows
      // version. We will just check _open result.
      _close(fd);
      if (fd != _open("nul", _O_RDWR))
        ABORT();
    }
  }
#endif  // _WIN32
}


void ProcessArgv(int* argc,
                 const char** argv,
                 int* exec_argc,
                 const char*** exec_argv,
                 bool is_env = false) {
  // Parse a few arguments which are specific to Node.
  int v8_argc;
  const char** v8_argv;
  ParseArgs(argc, argv, exec_argc, exec_argv, &v8_argc, &v8_argv, is_env);

  // TODO(bnoordhuis) Intercept --prof arguments and start the CPU profiler
  // manually?  That would give us a little more control over its runtime
  // behavior but it could also interfere with the user's intentions in ways
  // we fail to anticipate.  Dillema.
  for (int i = 1; i < v8_argc; ++i) {
    if (strncmp(v8_argv[i], "--prof", sizeof("--prof") - 1) == 0) {
      v8_is_profiling = true;
      break;
    }
  }

#ifdef __POSIX__
  // Block SIGPROF signals when sleeping in epoll_wait/kevent/etc.  Avoids the
  // performance penalty of frequent EINTR wakeups when the profiler is running.
  // Only do this for v8.log profiling, as it breaks v8::CpuProfiler users.
  if (v8_is_profiling) {
    uv_loop_configure(uv_default_loop(), UV_LOOP_BLOCK_SIGNAL, SIGPROF);
  }
#endif

  // The const_cast doesn't violate conceptual const-ness.  V8 doesn't modify
  // the argv array or the elements it points to.
  if (v8_argc > 1)
    V8::SetFlagsFromCommandLine(&v8_argc, const_cast<char**>(v8_argv), true);

#ifdef __MVS__
  V8::SetFlagsFromString(u8"--nohard_abort", 14);
#endif

  // Anything that's still in v8_argv is not a V8 or a node option.
  for (int i = 1; i < v8_argc; i++) {
    PrintErrorString(u8"%s: bad option: %s\n", argv[0], v8_argv[i]);
  }
  delete[] v8_argv;
  v8_argv = nullptr;

  if (v8_argc > 1) {
    platform_exit(9);
  }
}


void Init(int* argc,
          const char** argv,
          int* exec_argc,
          const char*** exec_argv) {
  // Initialize prog_start_time to get relative uptime.
  prog_start_time = static_cast<double>(uv_now(uv_default_loop()));

  // Make inherited handles noninheritable.
  uv_disable_stdio_inheritance();

  // init async debug messages dispatching
  // Main thread uses uv_default_loop
  CHECK_EQ(0, uv_async_init(uv_default_loop(),
                            &dispatch_debug_messages_async,
                            DispatchDebugMessagesAsyncCallback));
  uv_unref(reinterpret_cast<uv_handle_t*>(&dispatch_debug_messages_async));

#if defined(NODE_V8_OPTIONS)
  // Should come before the call to V8::SetFlagsFromCommandLine()
  // so the user can disable a flag --foo at run-time by passing
  // --no_foo from the command line.
  V8::SetFlagsFromString(NODE_V8_OPTIONS, sizeof(NODE_V8_OPTIONS) - 1);
#endif

  if (openssl_config.empty())
    SafeGetenv("OPENSSL_CONF", &openssl_config);

  if (config_warning_file.empty()) {
    SafeGetenv("NODE_REDIRECT_WARNINGS", &config_warning_file);
#ifdef __MVS__
    transform(config_warning_file.begin(), config_warning_file.end(), config_warning_file.begin(), [](char c) -> char {
      __e2a_l(&c, 1);
      return c;
    });
#endif
  }

#if !defined(NODE_WITHOUT_NODE_OPTIONS)
  std::string node_options;
  if (SafeGetenv("NODE_OPTIONS", &node_options)) {
    // Smallest tokens are 2-chars (a not space and a space), plus 2 extra
    // pointers, for the prepended executable name, and appended NULL pointer.
    size_t max_len = 2 + (node_options.length() + 1) / 2;
    const char** argv_from_env = new const char*[max_len];
    int argc_from_env = 0;
    // [0] is expected to be the program name, fill it in from the real argv.
    argv_from_env[argc_from_env++] = argv[0];

    char* cstr = strdup(node_options.c_str());
#ifdef __MVS__
    __e2a_s(cstr);
#endif
    char* initptr = cstr;
    char* token;
    while ((token = strtok(initptr, "\x20"))) {  // NOLINT(runtime/threadsafe_fn)
      initptr = nullptr;
      argv_from_env[argc_from_env++] = token;
    }
    argv_from_env[argc_from_env] = nullptr;
    int exec_argc_;
    const char** exec_argv_ = nullptr;
    ProcessArgv(&argc_from_env, argv_from_env, &exec_argc_, &exec_argv_, true);
    delete[] exec_argv_;
    delete[] argv_from_env;
    free(cstr);
  }
#endif

  ProcessArgv(argc, argv, exec_argc, exec_argv);

#if defined(NODE_HAVE_I18N_SUPPORT)
  // If the parameter isn't given, use the env variable.
  if (icu_data_dir.empty())
    SafeGetenv("NODE_ICU_DATA", &icu_data_dir);
#ifdef __MVS__
  transform(icu_data_dir.begin(), icu_data_dir.end(), back_inserter(icu_data_dir), [](char c) -> char {
    __a2e_l(&c, 1);
    return c;
  });
#endif
  // Initialize ICU.
  // If icu_data_dir is empty here, it will load the 'minimal' data.
  if (!i18n::InitializeICUDirectory(icu_data_dir)) {
    FatalError(nullptr, "\x43\x6f\x75\x6c\x64\x20\x6e\x6f\x74\x20\x69\x6e\x69\x74\x69\x61\x6c\x69\x7a\x65\x20\x49\x43\x55\x20"
                     "\x28\x63\x68\x65\x63\x6b\x20\x4e\x4f\x44\x45\x5f\x49\x43\x55\x5f\x44\x41\x54\x41\x20\x6f\x72\x20\x2d\x2d\x69\x63\x75\x2d\x64\x61\x74\x61\x2d\x64\x69\x72\x20\x70\x61\x72\x61\x6d\x65\x74\x65\x72\x73\x29\x0a");
  }
#endif

  // Unconditionally force typed arrays to allocate outside the v8 heap. This
  // is to prevent memory pointers from being moved around that are returned by
  // Buffer::Data().
  const char no_typed_array_heap[] = "\x2d\x2d\x74\x79\x70\x65\x64\x5f\x61\x72\x72\x61\x79\x5f\x6d\x61\x78\x5f\x73\x69\x7a\x65\x5f\x69\x6e\x5f\x68\x65\x61\x70\x3d\x30";
  V8::SetFlagsFromString(no_typed_array_heap, sizeof(no_typed_array_heap) - 1);

  if (!use_debug_agent) {
    RegisterDebugSignalHandler();
  }

  // We should set node_is_initialized here instead of in node::Start,
  // otherwise embedders using node::Init to initialize everything will not be
  // able to set it and native modules will not load for them.
  node_is_initialized = true;
}


struct AtExitCallback {
  void (*cb_)(void* arg);
  void* arg_;
};

static std::list<AtExitCallback> at_exit_functions;


// TODO(bnoordhuis) Turn into per-context event.
void RunAtExit(Environment* env) {
  for (AtExitCallback at_exit : at_exit_functions) {
    at_exit.cb_(at_exit.arg_);
  }
  at_exit_functions.clear();
}


void AtExit(void (*cb)(void* arg), void* arg) {
  at_exit_functions.push_back(AtExitCallback{cb, arg});
}


void EmitBeforeExit(Environment* env) {
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Object> process_object = env->process_object();
  Local<String> exit_code = FIXED_ONE_BYTE_STRING(env->isolate(), "\x65\x78\x69\x74\x43\x6f\x64\x65");
  Local<Value> args[] = {
    FIXED_ONE_BYTE_STRING(env->isolate(), "\x62\x65\x66\x6f\x72\x65\x45\x78\x69\x74"),
    process_object->Get(exit_code)->ToInteger(env->isolate())
  };
  MakeCallback(env, process_object, "\x65\x6d\x69\x74", arraysize(args), args);
}


int EmitExit(Environment* env) {
  // process.emit('exit')
  HandleScope handle_scope(env->isolate());
  Context::Scope context_scope(env->context());
  Local<Object> process_object = env->process_object();
  process_object->Set(env->exiting_string(), True(env->isolate()));

  Local<String> exitCode = env->exit_code_string();
  int code = process_object->Get(exitCode)->Int32Value();

  Local<Value> args[] = {
    env->exit_string(),
    Integer::New(env->isolate(), code)
  };

  MakeCallback(env, process_object, "\x65\x6d\x69\x74", arraysize(args), args);

  // Reload exit code, it may be changed by `emit('exit')`
  return process_object->Get(exitCode)->Int32Value();
}


// Just a convenience method
Environment* CreateEnvironment(Isolate* isolate,
                               Local<Context> context,
                               int argc,
                               const char* const* argv,
                               int exec_argc,
                               const char* const* exec_argv) {
  Environment* env;
  Context::Scope context_scope(context);

  env = CreateEnvironment(isolate,
                          uv_default_loop(),
                          context,
                          argc,
                          argv,
                          exec_argc,
                          exec_argv);

  LoadEnvironment(env);

  return env;
}

static Environment* CreateEnvironment(Isolate* isolate,
                                      Local<Context> context,
                                      NodeInstanceData* instance_data) {
  return CreateEnvironment(isolate,
                           instance_data->event_loop(),
                           context,
                           instance_data->argc(),
                           instance_data->argv(),
                           instance_data->exec_argc(),
                           instance_data->exec_argv());
}


static void HandleCloseCb(uv_handle_t* handle) {
  Environment* env = reinterpret_cast<Environment*>(handle->data);
  env->FinishHandleCleanup(handle);
}


static void HandleCleanup(Environment* env,
                          uv_handle_t* handle,
                          void* arg) {
  handle->data = env;
  uv_close(handle, HandleCloseCb);
}


Environment* CreateEnvironment(Isolate* isolate,
                               uv_loop_t* loop,
                               Local<Context> context,
                               int argc,
                               const char* const* argv,
                               int exec_argc,
                               const char* const* exec_argv) {
  HandleScope handle_scope(isolate);

  Context::Scope context_scope(context);
  Environment* env = Environment::New(context, loop);

  isolate->SetAutorunMicrotasks(false);

  uv_check_init(env->event_loop(), env->immediate_check_handle());
  uv_unref(
      reinterpret_cast<uv_handle_t*>(env->immediate_check_handle()));

  uv_idle_init(env->event_loop(), env->immediate_idle_handle());

  // Inform V8's CPU profiler when we're idle.  The profiler is sampling-based
  // but not all samples are created equal; mark the wall clock time spent in
  // epoll_wait() and friends so profiling tools can filter it out.  The samples
  // still end up in v8.log but with state=IDLE rather than state=EXTERNAL.
  // TODO(bnoordhuis) Depends on a libuv implementation detail that we should
  // probably fortify in the API contract, namely that the last started prepare
  // or check watcher runs first.  It's not 100% foolproof; if an add-on starts
  // a prepare or check watcher after us, any samples attributed to its callback
  // will be recorded with state=IDLE.
  uv_prepare_init(env->event_loop(), env->idle_prepare_handle());
  uv_check_init(env->event_loop(), env->idle_check_handle());
  uv_unref(reinterpret_cast<uv_handle_t*>(env->idle_prepare_handle()));
  uv_unref(reinterpret_cast<uv_handle_t*>(env->idle_check_handle()));

  uv_idle_init(env->event_loop(), env->destroy_ids_idle_handle());
  uv_unref(reinterpret_cast<uv_handle_t*>(env->destroy_ids_idle_handle()));

  // Register handle cleanups
  env->RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(env->immediate_check_handle()),
      HandleCleanup,
      nullptr);
  env->RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(env->immediate_idle_handle()),
      HandleCleanup,
      nullptr);
  env->RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(env->idle_prepare_handle()),
      HandleCleanup,
      nullptr);
  env->RegisterHandleCleanup(
      reinterpret_cast<uv_handle_t*>(env->idle_check_handle()),
      HandleCleanup,
      nullptr);

  if (v8_is_profiling) {
    StartProfilerIdleNotifier(env);
  }

  Local<FunctionTemplate> process_template = FunctionTemplate::New(isolate);
  process_template->SetClassName(FIXED_ONE_BYTE_STRING(isolate, "\x70\x72\x6f\x63\x65\x73\x73"));

  Local<Object> process_object =
      process_template->GetFunction()->NewInstance(context).ToLocalChecked();
  env->set_process_object(process_object);

  SetupProcessObject(env, argc, argv, exec_argc, exec_argv);
  LoadAsyncWrapperInfo(env);

  return env;
}


// Entry point for new node instances, also called directly for the main
// node instance.
static void StartNodeInstance(void* arg) {
  NodeInstanceData* instance_data = static_cast<NodeInstanceData*>(arg);
  Isolate::CreateParams params;
  ArrayBufferAllocator* array_buffer_allocator = new ArrayBufferAllocator();
  params.array_buffer_allocator = array_buffer_allocator;
#ifdef NODE_ENABLE_VTUNE_PROFILING
  params.code_event_handler = vTune::GetVtuneCodeEventHandler();
#endif
  Isolate* isolate = Isolate::New(params);

  {
    Mutex::ScopedLock scoped_lock(node_isolate_mutex);
    if (instance_data->is_main()) {
      CHECK_EQ(node_isolate, nullptr);
      node_isolate = isolate;
    }
  }

  if (track_heap_objects) {
    isolate->GetHeapProfiler()->StartTrackingHeapObjects(true);
  }

  {
    Locker locker(isolate);
    Isolate::Scope isolate_scope(isolate);
    HandleScope handle_scope(isolate);
    Local<Context> context = Context::New(isolate);
    Environment* env = CreateEnvironment(isolate, context, instance_data);
    array_buffer_allocator->set_env(env);
    Context::Scope context_scope(context);

    isolate->SetAbortOnUncaughtExceptionCallback(
        ShouldAbortOnUncaughtException);

    // Start debug agent when argv has --debug
    if (instance_data->use_debug_agent()) {
      const char* path = instance_data->argc() > 1
                         ? instance_data->argv()[1]
                         : nullptr;
      StartDebug(env, path, debug_wait_connect);
      if (use_inspector && !debugger_running) {
        platform_exit(12);
      }
    }

    {
      Environment::AsyncCallbackScope callback_scope(env);
      LoadEnvironment(env);
    }

    env->set_trace_sync_io(trace_sync_io);

    // Enable debugger
    if (instance_data->use_debug_agent())
      EnableDebug(env);

    {
      SealHandleScope seal(isolate);
      bool more;
      do {
        v8_platform.PumpMessageLoop(isolate);
        more = uv_run(env->event_loop(), UV_RUN_ONCE);

        if (more == false) {
          v8_platform.PumpMessageLoop(isolate);
          EmitBeforeExit(env);

          // Emit `beforeExit` if the loop became alive either after emitting
          // event, or after running some callbacks.
          more = uv_loop_alive(env->event_loop());
          if (uv_run(env->event_loop(), UV_RUN_NOWAIT) != 0)
            more = true;
        }
      } while (more == true);
    }

    env->set_trace_sync_io(false);

    int exit_code = EmitExit(env);
    if (instance_data->is_main())
      instance_data->set_exit_code(exit_code);
    RunAtExit(env);

    WaitForInspectorDisconnect(env);
#if defined(LEAK_SANITIZER)
    __lsan_do_leak_check();
#endif

    array_buffer_allocator->set_env(nullptr);
    env->Dispose();
    env = nullptr;
  }

  {
    Mutex::ScopedLock scoped_lock(node_isolate_mutex);
    if (node_isolate == isolate)
      node_isolate = nullptr;
  }

  CHECK_NE(isolate, nullptr);
  isolate->Dispose();
  isolate = nullptr;
  delete array_buffer_allocator;
}

int Start(int argc, char** argv) {
  OS390ThreadManager threadPoolObj;
  PlatformInit();

  CHECK_GT(argc, 0);

  // Hack around with the argv pointer. Used for process.title = "blah".
  argv = uv_setup_args(argc, argv);

#ifdef __MVS__
  for (int i = 0; i < argc; i++)
    __e2a_s(argv[i]);
#endif

  // This needs to run *before* V8::Initialize().  The const_cast is not
  // optional, in case you're wondering.
  int exec_argc;
  const char** exec_argv;
  Init(&argc, const_cast<const char**>(argv), &exec_argc, &exec_argv);

#ifdef __MVS__
  signalHandlerExit = 0;
  sigset_t set;
  sigfillset(&set);
  uv_thread_create(&signalHandlerThread, SignalHandlerThread, NULL);
  sigprocmask(SIG_BLOCK, &set, NULL);
#endif

#if HAVE_OPENSSL
  {
    std::string extra_ca_certs;
    if (SafeGetenv("NODE_EXTRA_CA_CERTS", &extra_ca_certs))
      crypto::UseExtraCaCerts(*E2A(extra_ca_certs.c_str()));
  }
#ifdef NODE_FIPS_MODE
  // In the case of FIPS builds we should make sure
  // the random source is properly initialized first.
  OPENSSL_init();
#endif  // NODE_FIPS_MODE
  // V8 on Windows doesn't have a good source of entropy. Seed it from
  // OpenSSL's pool.
  V8::SetEntropySource(crypto::EntropySource);
#endif  // HAVE_OPENSSL

  v8_platform.Initialize(v8_thread_pool_size);
  V8::Initialize();
  v8_initialized = true;

  int exit_code = 1;
  {
    NodeInstanceData instance_data(NodeInstanceType::MAIN,
                                   uv_default_loop(),
                                   argc,
                                   const_cast<const char**>(argv),
                                   exec_argc,
                                   exec_argv,
                                   use_debug_agent);
    StartNodeInstance(&instance_data);
    exit_code = instance_data.exit_code();
  }
  v8_initialized = false;
  V8::Dispose();

  v8_platform.Dispose();

  if (pthread_cancel(signalHandlerThread) == -1)
    abort();

  StopDebugSignalHandler();

  delete[] exec_argv;
  exec_argv = nullptr;

  return exit_code;
}


}  // namespace node
