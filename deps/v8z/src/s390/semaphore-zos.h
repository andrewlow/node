// Copyright 2012 the V8 project authors. All rights reserved.
//
// Copyright IBM Corp. 2016. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_S390_SEMAPHORE_ZOS_H_
#define V8_S390_SEMAPHORE_ZOS_H_

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <sys/sem.h>

#if __WORDSIZE == 64
# define __SIZEOF_SEM_T 32
#else
# define __SIZEOF_SEM_T 16
#endif

#define SEM_FAILED ((sem_t *) 0)

typedef union {
  char __size[__SIZEOF_SEM_T];
  int64_t __align;
} sem_t;

int initsem(key_t key, int nsems);

int sem_create(key_t key);

int sem_initialize(int *semid, int value);

int sem_init(int *semid, int pshared, unsigned int value);

int sem_destroy(int *semid);

int sem_wait(int *semid);

int sem_trywait(int *semid);

int sem_post(int *semid);

int sem_timedwait(int *semid, struct timespec *timeout);

#endif  // V8_S390_SEMAPHORE_ZOS_H_
