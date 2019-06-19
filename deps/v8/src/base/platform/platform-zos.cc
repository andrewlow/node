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

#define MAP_FAILED ((void *)-1L)

namespace v8 {
namespace base {

static const int kMegaByte = 1024*1024;

//------------------------------------------accounting for memory allocation


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

#pragma convert("IBM-1047")
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
  __asm(
      " LLGF %0,16(0,0) \n"
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
  __asm volatile(
      " lgr 1,%3 \n"
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
    fprintf(stderr, "__iarv64_alloc: pid %d tid %d ptr=%p size=%lu rc=%lld\n",
            getpid(), (int)(pthread_self().__ & 0x7fffffff), parm.xorigin,
            (unsigned long)(segs * 1024 * 1024), rc);
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
    fprintf(stderr, "__iarv64_free pid %d tid %d ptr=%p rc=%lld\n", getpid(),
            (int)(pthread_self().__ & 0x7fffffff), org, rc);
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
    cursor_t;
typedef std::unordered_map<key_type, bool, __hash_func>::const_iterator
    rmode_cursor_t;

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
    if (mem_account()) fprintf(stderr, "ADDED: @%lx size %lu\n", k, v);
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
        fprintf(stderr, "ADDED:@%lx size %lu RMODE64\n", k,
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
      cursor_t c = cache.find(k);
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
        fprintf(stderr, "ADDED:@%lx size %lu RMODE64\n", k,
                (size_t)(segs * 1024 * 1024));
    }
    return p;
  }
  int free_seg(void* ptr) {
    unsigned long k = (unsigned long)ptr;
    int rc = __mo_free(ptr);
    std::lock_guard<std::mutex> guard(access_lock);
    if (rc == 0) {
      cursor_t c = cache.find(k);
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
    cursor_t c = cache.find(k);
    if (c != cache.end()) {
      return 1;
    }
    return 0;
  }
  int is_rmode64(const void* ptr) {
    unsigned long k = (unsigned long)ptr;
    std::lock_guard<std::mutex> guard(access_lock);
    cursor_t c = cache.find(k);
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
      for (cursor_t it = cache.begin(); it != cache.end(); ++it) {
        fprintf(stderr, "LIST: @%lx size %lu\n", it->first, it->second);
      }
  }
  void freeptr(const void* ptr) {
    unsigned long k = (unsigned long)ptr;
    std::lock_guard<std::mutex> guard(access_lock);
    cursor_t c = cache.find(k);
    if (c != cache.end()) {
      cache.erase(c);
    }
  }
  ~__Cache() {
    std::lock_guard<std::mutex> guard(access_lock);
    if (mem_account())
      for (cursor_t it = cache.begin(); it != cache.end(); ++it) {
        fprintf(stderr,
                "Error: DEBRIS (allocated but never free'd): @%lx size %lu\n",
                it->first, it->second);
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
    __asm(
        " SYSSTATE ARCHLVL=2,AMODE64=YES\n"
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
    __asm(
        " SYSSTATE ARCHLVL=2\n"
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
    __asm(
        " SYSSTATE ARCHLVL=2,AMODE64=YES\n"
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
    __asm(
        "SYSSTATE ARCHLVL=2"
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

static void* anon_mmap(void* _, size_t len) {
  void* ret = anon_mmap_inner(_, len);
  if (ret == MAP_FAILED) {
    if (mem_account())
      fprintf(stderr, "Error: anon_mmap request size %zu failed\n", len);
    return ret;
  }
  return ret;
}

static int anon_munmap(void* addr, size_t len) {
  if (alloc_info.is_exist_ptr(addr)) {
    if (mem_account())
      fprintf(stderr, "Address found, attempt to free @%p size %d\n", addr,
              (int)len);
    int rc = anon_munmap_inner(addr, len, alloc_info.is_rmode64(addr));
    if (rc != 0) {
      if (mem_account())
        fprintf(stderr, "Error: anon_munmap @%p size %zu failed\n", addr, len);
      return rc;
    }
    return 0;
  } else {
    if (mem_account())
      fprintf(stderr, "Error: attempt to free %p size %d (not allocated)\n",
              addr, (int)len);
    return 0;
  }
}
#pragma convert(pop)
//----------------------------------------------------------------------------------------------

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
