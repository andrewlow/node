// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform-specific code for zOS/Unix goes here. For the POSIX-compatible
// parts, the implementation is in platform-posix.cc.
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/types.h>

// Ubuntu Dapper requires memory pages to be marked as
// executable. Otherwise, OS raises an exception when executing code
// in that page.
#include <errno.h>
#include <fcntl.h>      // open
#include <stdarg.h>
#include <strings.h>    // index
#undef index
#include <sys/mman.h>   // mmap & munmap
#include <sys/stat.h>   // open
#include <sys/types.h>  // mmap & munmap
#include <unistd.h>     // sysconf
#include "zos.h"

// GLibc on ARM defines mcontext_t has a typedef for 'struct sigcontext'.
// Old versions of the C library <signal.h> didn't define the type.
#if defined(__ANDROID__) && !defined(__BIONIC_HAVE_UCONTEXT_T) && \
    (defined(__arm__) || defined(__aarch64__)) && \
    !defined(__BIONIC_HAVE_STRUCT_SIGCONTEXT)
#include <asm/sigcontext.h>  // NOLINT
#endif

#if defined(LEAK_SANITIZER)
#include <sanitizer/lsan_interface.h>
#endif

#include <cmath>

#undef MAP_TYPE

#include "src/base/macros.h"
#include "src/base/platform/platform.h"
#include "src/base/platform/platform-posix.h"
#include "src/s390/semaphore-zos.h"
#include "src/base/sys-info.h"

#include <mutex>
#include <unordered_map>

#if !defined(MAP_FAILED)
#define MAP_FAILED ((void *)-1L)
#endif


namespace v8 {
namespace base {

bool OS::Free(void* address, const size_t size) {
  // TODO(1240712): munmap has a return value which is ignored here.
  int result = anon_munmap(address, size);
  USE(result);
  DCHECK(result == 0);
  return result == 0;;
}

bool OS::Release(void* address, size_t size) {
  DCHECK_EQ(0, reinterpret_cast<uintptr_t>(address) % CommitPageSize());
  DCHECK_EQ(0, size % CommitPageSize());
  return anon_munmap(address, size) == 0;
}

class ZOSTimezoneCache : public PosixTimezoneCache {
  const char * LocalTimezone(double time) override;

  double LocalTimeOffset(double time_ms, bool is_utc) override;

  ~ZOSTimezoneCache() override {}
};


const char* ZOSTimezoneCache::LocalTimezone(double time) {

  if (isnan(time)) return "";
  time_t tv = static_cast<time_t>(std::floor(time/msPerSecond));
  struct tm tm;
  struct tm* t= localtime_r(&tv, &tm);
  if (NULL == t) 
    return "";

  return tzname[0];
}

double ZOSTimezoneCache::LocalTimeOffset(double time_ms, bool is_utc) {
  time_t tv = time(NULL);
  struct tm* gmt = gmtime(&tv);
  double gm_secs = gmt->tm_sec + (gmt->tm_min * 60) + (gmt->tm_hour * 3600);
  struct tm* localt = localtime(&tv);
  double local_secs = localt->tm_sec + (localt->tm_min * 60) +
                      (localt->tm_hour * 3600);
  return (local_secs - gm_secs) * msPerSecond -
         (localt->tm_isdst > 0 ? 3600 * msPerSecond : 0);
}

TimezoneCache * OS::CreateTimezoneCache() { return new ZOSTimezoneCache(); }

void* OS::Allocate(void* address, size_t size, size_t alignment,
                   MemoryPermission access) {
  size_t page_size = AllocatePageSize();
  DCHECK_EQ(0, size % page_size);
  DCHECK_EQ(0, alignment % page_size);
  address = AlignedAddress(address, alignment);
  // Add the maximum misalignment so we are guaranteed an aligned base address.
  size_t request_size = size + (alignment - page_size);
  request_size = RoundUp(request_size, OS::AllocatePageSize());
  void* result = anon_mmap(address, request_size);
  if (result == nullptr) return nullptr;

  // Unmap memory allocated before the aligned base address.
  uint8_t* base = static_cast<uint8_t*>(result);
  uint8_t* aligned_base = reinterpret_cast<uint8_t*>(
      RoundUp(reinterpret_cast<uintptr_t>(base), alignment));
  if (aligned_base != base) {
    DCHECK_LT(base, aligned_base);
    size_t prefix_size = static_cast<size_t>(aligned_base - base);
    CHECK(Free(base, prefix_size));
    request_size -= prefix_size;
  }
  // Unmap memory allocated after the potentially unaligned end.
  if (size != request_size) {
    DCHECK_LT(size, request_size);
    size_t suffix_size = request_size - size;
    CHECK(Free(aligned_base + size, suffix_size));
    request_size -= suffix_size;
  }

  DCHECK_EQ(size, request_size);
  return static_cast<void*>(aligned_base);
}

std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  std::vector<SharedLibraryAddress> result;
  return result;
}


void OS::SignalCodeMovingGC() {
  // Support for ll_prof.py.
  //
  // The Linux profiler built into the kernel logs all mmap's with
  // PROT_EXEC so that analysis tools can properly attribute ticks. We
  // do a mmap with a name known by ll_prof.py and immediately munmap
  // it. This injects a GC marker into the stream of events generated
  // by the kernel and allows us to synchronize V8 code log and the
  // kernel log.
  long size = sysconf(_SC_PAGESIZE);  // NOLINT(runtime/int)
  FILE* f = fopen(OS::GetGCFakeMMapFile(), "w+");
  if (f == nullptr) {
    OS::PrintError("Failed to open %s\n", OS::GetGCFakeMMapFile());
    OS::Abort();
  }
  void* addr = mmap(OS::GetRandomMmapAddr(), size, PROT_READ | PROT_EXEC,
                    MAP_PRIVATE, fileno(f), 0);
  DCHECK_NE(MAP_FAILED, addr);
  CHECK(Free(addr, size));
  fclose(f);
}

static const int kMmapFd = -1;
// Constants used for mmap.
static const int kMmapFdOffset = 0;

inline int GetFirstFlagFrom(const char* format_e, int start = 0) {
  int flag_pos = start;
  // find the first flag
  for (; format_e[flag_pos] != '\0' && format_e[flag_pos] != '%'; flag_pos++);
  return flag_pos;
}

} }  // namespace v8::base
