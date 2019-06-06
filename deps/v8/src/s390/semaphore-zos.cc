// Copyright 2012 the V8 project authors. All rights reserved.
//
// Copyright IBM Corp. 2019. All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "semaphore-zos.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/modes.h>

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
  case EFBIG:
    errno = EINVAL;
    break;
  case EIDRM:
    errno = EINTR;
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


// On success returns 0. On error returns -1 and errno is set.
int sem_init(sem_t *sem, int pshared, unsigned int value) {
  int err;
  if (sem == NULL)
      return ENOMEM;

  if ((err = pthread_mutex_init(&sem->mutex, NULL)) != 0) {
     errno = err;   
     return -1;
  }
  
  if ((err = pthread_cond_init(&sem->cond, NULL)) != 0) {
     if (pthread_mutex_destroy(&sem->mutex)) 
         abort();
     errno = err;
     return - 1;
  }
  sem->value = value;
  return 0;
}


/* sem_destroy -- destroys the semaphore using semctl() */
int sem_destroy(sem_t *sem) {
  if (pthread_cond_destroy(&sem->cond))
      abort();
  if (pthread_mutex_destroy(&sem->mutex))
      abort();
  
}

/* sem_wait -- it gets a lock on semaphore and implemented using semop() */
int sem_wait(sem_t *sem) {
  if (pthread_mutex_lock(&sem->mutex))
      abort();

  while (sem->value == 0)
      if (pthread_cond_wait(&sem->cond, &sem->mutex))
          abort();
  sem->value--;

  if (pthread_mutex_unlock(&sem->mutex))
      abort();

  return 0;
}


/* sem_timedwait -- it waits for a specific time-period to get a lock on
 * semaphore. Implemented using __semop_timed() */
int sem_timedwait(sem_t *sem, const struct timespec *ts) {
  int err;

  if (pthread_mutex_lock(&sem->mutex))
      abort();

  err = pthread_cond_timedwait(&sem->cond, &sem->mutex, ts);
  if ( err != 0 && err != ETIMEDOUT) 
      abort();

  if (err == 0)
      sem->value--;

  if (pthread_mutex_unlock(&sem->mutex))
      abort();

  if (err != 0) {
     errno = err;
     return -1;
  }
  return 0;
}


/* sem_post -- it releases lock on semaphore using semop */
int sem_post(sem_t *sem) {
  
  if (pthread_mutex_lock(&sem->mutex))
      abort();
  
  sem->value++;

  if (sem->value == 1)
      if (pthread_cond_signal(&sem->cond))
          abort();

  if (pthread_mutex_unlock(&sem->mutex))
      abort();
  
  return 0;
}

