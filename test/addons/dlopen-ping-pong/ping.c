#ifndef _WIN32

#if defined(__MVS__)
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
typedef const char* (*dlopen_pong)(void);
#else
const char* dlopen_pong(void);
#endif

const char* dlopen_ping(void) {
#if defined(__MVS__)
  char* binding_path = getenv("BINDINGPATH");
  if (binding_path) {
    void* handle = dlopen(binding_path, RTLD_NOW);
    if (handle) {
      dlopen_pong func = (dlopen_pong)dlsym(handle, "dlopen_pong");
      if (func) {
        return func();
      }
    }
  }
  return "failed";
#else
  return dlopen_pong();
#endif
}

#endif  // _WIN32
