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
#ifdef __MVS__
#ifndef _AE_BIMODAL
#define _AE_BIMODAL 1
#endif
#include "zos.h"
#endif
#include "node.h"
#include <stdio.h>

#ifdef _WIN32
#include <windows.h>
#include <VersionHelpers.h>
#include <WinError.h>

int wmain(int argc, wchar_t *wargv[]) {
  if (!IsWindows7OrGreater()) {
    fprintf(stderr, "This application is only supported on Windows 7, "
                    "Windows Server 2008 R2, or higher.");
    exit(ERROR_EXE_MACHINE_TYPE_MISMATCH);
  }

  // Convert argv to UTF8
  char** argv = new char*[argc + 1];
  for (int i = 0; i < argc; i++) {
    // Compute the size of the required buffer
    DWORD size = WideCharToMultiByte(CP_UTF8,
                                     0,
                                     wargv[i],
                                     -1,
                                     nullptr,
                                     0,
                                     nullptr,
                                     nullptr);
    if (size == 0) {
      // This should never happen.
      AEWRAP_VOID(__fprintf_a(stderr, "Could not convert arguments to utf8."));
      exit(1);
    }
    // Do the actual conversion
    argv[i] = new char[size];
    DWORD result = WideCharToMultiByte(CP_UTF8,
                                       0,
                                       wargv[i],
                                       -1,
                                       argv[i],
                                       size,
                                       nullptr,
                                       nullptr);
    if (result == 0) {
      // This should never happen.
      AEWRAP_VOID(__fprintf_a(stderr, "Could not convert arguments to utf8."));
      exit(1);
    }
  }
  argv[argc] = nullptr;
  // Now that conversion is done, we can finally start.
  return node::Start(argc, argv);
}
#else
// UNIX
#ifdef __linux__
#include <elf.h>
#ifdef __LP64__
#define Elf_auxv_t Elf64_auxv_t
#else
#define Elf_auxv_t Elf32_auxv_t
#endif  // __LP64__
extern char** environ;
#endif  // __linux__
#if defined(__POSIX__) && defined(NODE_SHARED_MODE)
#include <string.h>
#include <signal.h>
#endif

namespace node {
  extern bool linux_at_secure;
}  // namespace node

#if defined(__MVS__)
#include <assert.h>
#include <dlfcn.h>
#include <libgen.h>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <sys/ps.h>
#include <unistd.h>

class __setlibpath {
  void *p;

public:
  __setlibpath() {
    std::vector<char> argv(512, 0);
    std::vector<char> parent(512, 0);
    W_PSPROC buf;
    int token = 0;
    pid_t mypid = getpid();
    memset(&buf, 0, sizeof(buf));
    buf.ps_pathlen = argv.size();
    buf.ps_pathptr = &argv[0];
    while ((token = w_getpsent(token, &buf, sizeof(buf))) > 0) {
      if (buf.ps_pid == mypid) {
        /* Found our process. */

        /* Resolve path to find true location of executable. */
        if (realpath(&argv[0], &parent[0]) == NULL)
          break;

        /* Get parent directory. */
        dirname(&parent[0]);
        /* Get parent's parent directory. */
        std::vector<char> parent2(parent.begin(), parent.end());
        dirname(&parent2[0]);

#pragma convert("ibm-1047")
        /* Append new paths to libpath. */
        std::ostringstream libpath;
        libpath << getenv("LIBPATH");
        libpath << ":" << &parent[0] << "/obj.target/";
        libpath << ":" << &parent2[0] << "/lib/";
        setenv("LIBPATH", libpath.str().c_str(), 1);
#pragma convert(pop)
        int size = libpath.str().length();
        char *libpath_a = (char *)alloca(size + 1);
        memcpy(libpath_a, libpath.str().c_str(), size + 1);
        __e2a_l(libpath_a, size);
        __setenv_a("LIBPATH", libpath_a,1);
#pragma convert("ibm-1047")
        char *error;
        p = dlopen("libnode.so", RTLD_NOW);
        if (!p) {
          error = dlerror();
          fprintf(stderr, "dlopen:%s\n", error);
          assert(p);
        }
        void (*__xfer_env_p)(void) = (void (*)(void))dlsym(p, "__xfer_env");
        if (!p) {
          error = dlerror();
          fprintf(stderr, "dlsym:__xfer_env:%s\n", error);
          assert(p);
        }
        __xfer_env_p();
        void (*__chgfdccsid_p)(int, int) =
            (void (*)(int, int))dlsym(p, "__chgfdccsid");
        if (!p) {
          error = dlerror();
          fprintf(stderr, "dlsym:__chgfdccsid:%s\n", error);
          assert(p);
        }
        __chgfdccsid_p(STDOUT_FILENO, 1047);
        __chgfdccsid_p(STDERR_FILENO, 1047);
#pragma convert(pop)
        break;
      }
    }
  }

  ~__setlibpath() {
    if (p)
      dlclose(p);
  }
};
__setlibpath __zossetup;
#endif

int main(int argc, char *argv[]) {
#if defined(__POSIX__) && defined(NODE_SHARED_MODE)
  // In node::PlatformInit(), we squash all signal handlers for non-shared lib
  // build. In order to run test cases against shared lib build, we also need
  // to do the same thing for shared lib build here, but only for SIGPIPE for
  // now. If node::PlatformInit() is moved to here, then this section could be
  // removed.
  {
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &act, nullptr);
  }
#endif

#if defined(__linux__)
  char** envp = environ;
  while (*envp++ != nullptr) {}
  Elf_auxv_t* auxv = reinterpret_cast<Elf_auxv_t*>(envp);
  for (; auxv->a_type != AT_NULL; auxv++) {
    if (auxv->a_type == AT_SECURE) {
      node::linux_at_secure = auxv->a_un.a_val;
      break;
    }
  }
#endif
  // Disable stdio buffering, it interacts poorly with printf()
  // calls elsewhere in the program (e.g., any logging from V8.)
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  return node::Start(argc, argv);
}
#endif
