// Copyright 2012 the V8 project authors. All rights reserved.
//
// Copyright IBM Corp. 2016. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "semaphore-zos.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>


void assignSemInitializeError() {
  switch (errno) {
  case EACCES:
    errno = EPERM;
    break;
  case EINVAL:
    break;
  case EPERM:
    break;
  case ERANGE:
    errno = EINVAL;
    break;
  }
}


void assignSemDestroyError() {
  switch (errno) {
  case EACCES:
    errno = EINVAL;
    break;
  case EINVAL:
    break;
  case EPERM:
    errno = EINVAL;
    break;
  case ERANGE:
    break;
  }
}


void assignSemgetError() {
  switch (errno) {
  case EACCES:
    errno = EPERM;
    break;
  case EINVAL:
    break;
  case ENOENT:
    errno = EINVAL;
    break;
  case ENOSPC:
    break;
  default:
    break;
  }
}


void assignSemopErrorCode() {
  switch (errno) {
  case EACCES:
    errno = EINVAL;
    break;
  case EINVAL:
    break;
  case EFAULT:
    errno = EINVAL;
    break;
  case EFBIG || EIDRM:
    errno = EINVAL;
    break;
  case ERANGE:
    break;
  case ENOSPC:
    break;
  case EINTR:
    break;
  case EAGAIN:
    break;
  default:
    errno = EINVAL;
    break;
  }
}


/* initsem -- called by sem_create, it gets the semaphore using semget() */
int initsem(key_t key, int nsems) {
  int semid;

  semid = semget(key, nsems, IPC_CREAT | IPC_EXCL | 0666);

  if (semid == -1 && errno == EEXIST) { /* someone else got it first */
    semid = semget(key, nsems, 0); /* get the id */
    /*printf("\n Some one else got it ");*/
    if (semid < 0) {
      return semid;
    }
  } else if (semid == -1) { /*chk for other errors here */
    return semid;
  }

  return semid;
}


/* sem_initialize -- it assigns a value to the semaphore using semctl() */
int sem_initialize(int *semid, int value) {
  int ret = semctl(*semid, 0, SETVAL, value);
  return ret;
}


/* sem_init --
 * it accepts key and semaphore value as its parameters
 * it creates a semaphore using semget and then initialize it
 * it returns semid
*/
int sem_init(int *sem, int pshared, unsigned int value) {
  key_t key;
  int ret = -1;

  if ((*sem = initsem(IPC_PRIVATE, 1)) == -1) {
    assignSemgetError();  /*assign err code*/
    return -1;
  } else {
    ret = sem_initialize(sem, value);
  }
  if (ret == -1) {
    assignSemInitializeError();  /* assign errcode for semctl */
  }
  return ret;
}


/* sem_destroy -- destroys the semaphore using semctl() */
int sem_destroy(int *semid) {
  int ret = semctl(*semid, 0, IPC_RMID);
  if (ret == -1) {
    assignSemDestroyError();  /* assign err code for semctl*/
  }
  return ret;
}


/* sem_wait -- it gets a lock on semaphore and implemented using semop() */
int sem_wait(int *semid) {
  struct sembuf sb;
  sb.sem_num = 0;
  sb.sem_op = -1;
  sb.sem_flg = 0;
  if (semop(*semid, &sb, 1) == -1) {
    assignSemopErrorCode();
    return -1;
  }
  return 0;
}


/* sem_timedwait -- it waits for a specific time-period to get a lock on
 * semaphore. Implemented using __semop_timed() */
int sem_timedwait(int *semid, struct timespec *ts) {
  int ret;
  struct sembuf sb;
  sb.sem_num = 0;
  sb.sem_op = -1;
  sb.sem_flg = 0;

  ret = __semop_timed(*semid, &sb, 1, ts);
  if (ret != 0) {
    assignSemopErrorCode();
  }
  return ret;
}


/* sem_post -- it releases lock on semaphore using semop */
int sem_post(int *semid) {
  struct sembuf sb;
  sb.sem_num = 0;
  sb.sem_op = 1;
  sb.sem_flg = 0;
  if (semop(*semid, &sb, 1) == -1) {
    assignSemopErrorCode();
    return -1;
  }
  return 0;
}
