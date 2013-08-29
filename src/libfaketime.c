/*
 *  This file is part of the FakeTime Preload Library, version 0.8.
 *
 *  The FakeTime Preload Library is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License v2 as
 *  published by the Free Software Foundation.
 *
 *  The FakeTime Preload Library is distributed in the hope that it will
 *  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License v2
 *  along with the FakeTime Preload Library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#define _GNU_SOURCE             /* required to get RTLD_NEXT defined */
#define _XOPEN_SOURCE           /* required to get strptime() defined */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>

/* pthread-handling contributed by David North, TDI in version 0.7 */
#ifdef PTHREAD
#include <pthread.h>

static pthread_mutex_t once_mutex=PTHREAD_MUTEX_INITIALIZER;

#define SINGLE_IF_MTX(ifcondition,mtxaddr) \
  if (ifcondition) { \
    pthread_mutex_lock(mtxaddr); \
    pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, (void *)mtxaddr); \
    if (ifcondition) {
#define SINGLE_IF(ifcondition) SINGLE_IF_MTX(ifcondition,&once_mutex)
#define END_SINGLE_IF \
    } \
    pthread_cleanup_pop(1); \
  }

#else

#define SINGLE_IF_MTX(ifcondition,mtxaddr) if (ifcondition) {
#define SINGLE_IF(ifcondition) if (ifcondition) {
#define END_SINGLE_IF }

#endif

#include <sys/timeb.h>
#include <dlfcn.h>

#define BUFFERLEN   256

/* prototypes */
time_t fake_time(time_t *time_tptr);
int    fake_ftime(struct timeb *tp);
int    fake_gettimeofday(struct timeval *tv, void *tz);
#ifdef POSIX_REALTIME
int    fake_clock_gettime(clockid_t clk_id, struct timespec *tp);
#endif

/*
 * Intercepted system calls:
 *  - time()
 *  - ftime()
 *  - gettimeofday()
 *  - clock_gettime()
 *
 *  Since version 0.7, if FAKE_INTERNAL_CALLS is defined, also calls to
 *  __time(), __ftime(), __gettimeofday(), and __clock_gettime() will be
 *  intercepted.
 *
 *  Thanks to a contribution by Philipp Hachtmann, the following
 *  system calls will also be time-adjusted depending on the compile
 *  switches used + any environmental variables present.
 *
 *  - stat()
 *  - fstat()
 *  - lstat()
 *  - the 64-bit versions of those three
 *
 */

/**
 * When advancing time linearly with each time(), etc. call, the calls are
 * counted in shared memory pointed at by ticks and protected by ticks_sem
 * semaphore */
static sem_t *ticks_sem = NULL;
static uint64_t *ticks = NULL;


void ft_cleanup (void) __attribute__ ((destructor));

static void ft_shm_init (void)
{
  int ticks_shm_fd;
  char sem_name[256], shm_name[256], *ft_shared = getenv("FAKETIME_SHARED");
  if (ft_shared != NULL) {
    if (sscanf(ft_shared, "%255s %255s", sem_name, shm_name) < 2 ) {
      printf("Error parsing semaphor name and shared memory id from string: %s", ft_shared);
      exit(1);
    }

    if (SEM_FAILED == (ticks_sem = sem_open(sem_name, 0))) {
      perror("sem_open");
      exit(1);
    }

    if (-1 == (ticks_shm_fd = shm_open(shm_name, O_CREAT|O_RDWR, S_IWUSR|S_IRUSR))) {
      perror("shm_open");
      exit(1);
    }
    if (MAP_FAILED == (ticks = mmap(NULL, sizeof(uint64_t), PROT_READ|PROT_WRITE,
            MAP_SHARED, ticks_shm_fd, 0))) {
      perror("mmap");
      exit(1);
    }
  }
}

void ft_cleanup (void)
{
  /* detach from shared memory */
  munmap(ticks, sizeof(uint64_t));
  sem_close(ticks_sem);
}

static time_t next_time(double ticklen)
{
  time_t ret = 0;

  if (ticks_sem != NULL) {
    /* lock */
    if (sem_wait(ticks_sem) == -1) {
      perror("sem_wait");
      exit(1);
    }

    /* calculate and update elapsed time */
    ret = ticklen * (*ticks)++;
    /* unlock */
    if (sem_post(ticks_sem) == -1) {
      perror("sem_post");
      exit(1);
    }
  }
  return ret;

}

#ifdef FAKE_STAT

#ifndef NO_ATFILE
#ifndef _ATFILE_SOURCE
#define _ATFILE_SOURCE
#endif
#include <fcntl.h> /* Definition of AT_* constants */
#endif

#include <sys/stat.h>

static int fake_stat_disabled = 0;

/* Contributed by Philipp Hachtmann in version 0.6 */
int __xstat (int ver, const char *path, struct stat *buf) {
  static int (*real_stat) (int, const char *, struct stat *);
  static int has_real_stat=0;

  SINGLE_IF(has_real_stat==0)
    real_stat = NULL;
    real_stat = dlsym(RTLD_NEXT, "__xstat");
    if (dlerror() == NULL) {
      has_real_stat = 1;
    }
  END_SINGLE_IF
  if (!has_real_stat) {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original stat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result = real_stat(ver, path, buf);
  if (result == -1) {
    return -1;
  }

   if (buf != NULL) {
     if (!fake_stat_disabled) {
       buf->st_ctime = fake_time(&(buf->st_ctime));
       buf->st_atime = fake_time(&(buf->st_atime));
       buf->st_mtime = fake_time(&(buf->st_mtime));
     }
   }

  return result;
}

/* Contributed by Philipp Hachtmann in version 0.6 */
int __fxstat (int ver, int fildes, struct stat *buf) {
  static int (*real_fstat) (int, int, struct stat *);
  static int has_real_fstat=0;

  SINGLE_IF(has_real_fstat==0)
    real_fstat = NULL;
    real_fstat = dlsym(RTLD_NEXT, "__fxstat");
    if (dlerror() == NULL) {
      has_real_fstat = 1;
    }
  END_SINGLE_IF
  if (!has_real_fstat) {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result = real_fstat(ver, fildes, buf);
  if (result == -1) {
    return -1;
  }

  if (buf != NULL) {
    if (!fake_stat_disabled) {
      buf->st_ctime = fake_time(&(buf->st_ctime));
      buf->st_atime = fake_time(&(buf->st_atime));
      buf->st_mtime = fake_time(&(buf->st_mtime));
    }
  }
  return result;
}

/* Added in v0.8 as suggested by Daniel Kahn Gillmor */
#ifndef NO_ATFILE
int __fxstatat(int ver, int fildes, const char *filename, struct stat *buf, int flag) {
  static int (*real_fstatat) (int, int, const char *, struct stat *, int);
  static int has_real_fstatat=0;

  SINGLE_IF(has_real_fstatat==0)
    real_fstatat = NULL;
    real_fstatat = dlsym(RTLD_NEXT, "__fxstatat");
    if (dlerror() == NULL) {
      has_real_fstatat = 1;
    }
  END_SINGLE_IF
  if (!has_real_fstatat) {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstatat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result = real_fstatat(ver, fildes, filename, buf, flag);
  if (result == -1) {
    return -1;
  }

  if (buf != NULL) {
    if (!fake_stat_disabled) {
      buf->st_ctime = fake_time(&(buf->st_ctime));
      buf->st_atime = fake_time(&(buf->st_atime));
      buf->st_mtime = fake_time(&(buf->st_mtime));
    }
  }
  return result;
}
#endif

/* Contributed by Philipp Hachtmann in version 0.6 */
int __lxstat (int ver, const char *path, struct stat *buf) {
  static int (*real_lstat)(int, const char *, struct stat *);
  static int has_real_lstat=0;

  SINGLE_IF(has_real_lstat==0)
    real_lstat = NULL;
    real_lstat = dlsym(RTLD_NEXT, "__lxstat");
    if (dlerror() == NULL) {
      has_real_lstat = 1;
    }
  END_SINGLE_IF
  if (!has_real_lstat) {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original lstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result = real_lstat(ver, path, buf);
  if (result == -1) {
    return -1;
  }

  if (buf != NULL) {
    if (!fake_stat_disabled) {
      buf->st_ctime = fake_time(&(buf->st_ctime));
      buf->st_atime = fake_time(&(buf->st_atime));
      buf->st_mtime = fake_time(&(buf->st_mtime));
    }
  }
  return result;
}

/* Contributed by Philipp Hachtmann in version 0.6 */
int __xstat64 (int ver, const char *path, struct stat64 *buf) {
  static int (*real_stat) (int, const char *, struct stat64 *);
  static int has_real_stat = 0;

  SINGLE_IF(has_real_stat==0)
    real_stat = NULL;
    real_stat = dlsym(RTLD_NEXT,"__xstat64");
    if (dlerror() == NULL) {
      has_real_stat = 1;
    }
  END_SINGLE_IF
  if (!has_real_stat) {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original stat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result=real_stat(ver, path, buf);
  if (result == -1) {
    return -1;
  }

  if (buf != NULL){
    if (!fake_stat_disabled) {
      buf->st_ctime = fake_time(&(buf->st_ctime));
      buf->st_atime = fake_time(&(buf->st_atime));
      buf->st_mtime = fake_time(&(buf->st_mtime));
    }
  }
  return result;
}

/* Contributed by Philipp Hachtmann in version 0.6 */
int __fxstat64 (int ver, int fildes, struct stat64 *buf) {
  static int (*real_fstat)(int, int , struct stat64 *);
  static int has_real_fstat=0;

  SINGLE_IF(has_real_fstat==0)
    real_fstat = NULL;
    real_fstat = dlsym(RTLD_NEXT, "__fxstat64");
    if (dlerror() == NULL) {
      has_real_fstat = 1;
    }
  END_SINGLE_IF
  if (!has_real_fstat) {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result = real_fstat(ver, fildes, buf);
  if (result == -1){
    return -1;
  }

  if (buf != NULL){
    if (!fake_stat_disabled) {
      buf->st_ctime = fake_time(&(buf->st_ctime));
      buf->st_atime = fake_time(&(buf->st_atime));
      buf->st_mtime = fake_time(&(buf->st_mtime));
    }
  }
  return result;
}

/* Added in v0.8 as suggested by Daniel Kahn Gillmor */
#ifndef NO_ATFILE
int __fxstatat64 (int ver, int fildes, const char *filename, struct stat64 *buf, int flag) {
  static int (*real_fstatat64)(int, int , const char *, struct stat64 *, int);
  static int has_real_fstatat64=0;

  SINGLE_IF(has_real_fstatat64==0)
    real_fstatat64 = NULL;
    real_fstatat64 = dlsym(RTLD_NEXT, "__fxstatat64");
    if (dlerror() == NULL) {
      has_real_fstatat64 = 1;
    }
  END_SINGLE_IF
  if (!has_real_fstatat64) {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstatat64() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result = real_fstatat64(ver, fildes, filename, buf, flag);
  if (result == -1){
    return -1;
  }

  if (buf != NULL){
    if (!fake_stat_disabled) {
      buf->st_ctime = fake_time(&(buf->st_ctime));
      buf->st_atime = fake_time(&(buf->st_atime));
      buf->st_mtime = fake_time(&(buf->st_mtime));
    }
  }
  return result;
}
#endif

/* Contributed by Philipp Hachtmann in version 0.6 */
int __lxstat64 (int ver, const char *path, struct stat64 *buf){
  static int (*real_lstat) (int, const char *, struct stat64 *);
  static int has_real_lstat = 0;

  SINGLE_IF(has_real_lstat==0)
    real_lstat = NULL;
    real_lstat = dlsym(RTLD_NEXT, "__lxstat64");
    if (dlerror() == NULL) {
      has_real_lstat = 1;
    }
  END_SINGLE_IF
  if (!has_real_lstat) {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original lstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result = real_lstat(ver, path, buf);
  if (result == -1){
    return -1;
  }

  if (buf != NULL){
    if (!fake_stat_disabled) {
      buf->st_ctime = fake_time(&(buf->st_ctime));
      buf->st_atime = fake_time(&(buf->st_atime));
      buf->st_mtime = fake_time(&(buf->st_mtime));
    }
  }
  return result;
}
#endif

/*
 * On MacOS, time() internally uses gettimeofday. If we don't
 * break the cycle by just calling it directly, we double-apply
 * relative changes.
 */

#ifdef __APPLE__
static int (*real_gettimeofday)(struct timeval *, void *);
static int has_real_gettimeofday = 0;
#endif
/*
 *  Our version of time() allows us to return fake values, so the calling
 *  program thinks it's retrieving the current date and time, while it is
 *  not
 *  Note that this routine is split into two parts so that the initialization
 *  piece can call the 'real' time function to establish a base time.
 */
static time_t _ftpl_time(time_t *time_tptr) {
#ifdef __APPLE__
    struct timeval tvm, *tv = &tvm;
#else
    static time_t (*real_time)(time_t *);
    static int has_real_time = 0;
#endif

    time_t result;

    time_t null_dummy;

    /* Handle null pointers correctly, fix as suggested by Andres Ojamaa */
    if (time_tptr == NULL) {
        time_tptr = &null_dummy;
        /* (void) fprintf(stderr, "NULL pointer caught in time().\n"); */
    }

#ifdef __APPLE__
    /* Check whether we've got a pointer to the real ftime() function yet */
    SINGLE_IF(has_real_gettimeofday==0)
        real_gettimeofday = NULL;
        real_gettimeofday = dlsym(RTLD_NEXT, "gettimeofday");

        /* check whether dlsym() worked */
        if (dlerror() == NULL) {
            has_real_gettimeofday = 1;
        }
    END_SINGLE_IF
    if (!has_real_gettimeofday) {  /* dlsym() failed */
#ifdef DEBUG
            (void) fprintf(stderr, "faketime problem: original gettimeofday() not found.\n");
#endif
            return -1; /* propagate error to caller */
    }

    /* initialize our result with the real current time */
    result = (*real_gettimeofday)(tv, NULL);
    if (result == -1) return result; /* original function failed */
    if (time_tptr != NULL)
        *time_tptr = tv->tv_sec;
    result = tv->tv_sec;
#else
    /* Check whether we've got a pointer to the real time function yet */
    SINGLE_IF(has_real_time==0)
        real_time = NULL;
        real_time = dlsym(RTLD_NEXT, "time");

        /* check whether dlsym() worked */
        if (dlerror() == NULL) {
            has_real_time = 1;
        }
    END_SINGLE_IF
    if (!has_real_time) {  /* dlsym() failed */
#ifdef DEBUG
            (void) fprintf(stderr, "faketime problem: original time() not found.\n");
#endif
            if (time_tptr != NULL)
                *time_tptr = -1;
            return -1; /* propagate error to caller */
    }

    /* initialize our result with the real current time */
    result = (*real_time)(time_tptr);
#endif

    return result;
}

time_t time(time_t *time_tptr) {
    time_t result;
    time_t null_dummy;
    if (time_tptr == NULL) {
        time_tptr = &null_dummy;
        /* (void) fprintf(stderr, "NULL pointer caught in time().\n"); */
    }
    result = _ftpl_time(time_tptr);
    if (result == ((time_t) -1)) return result;

    /* pass the real current time to our faking version, overwriting it */
    result = fake_time(time_tptr);

    /* return the result to the caller */
    return result;
}


int ftime(struct timeb *tp) {
    static int (*real_ftime)(struct timeb *);
    static int has_real_ftime = 0;
    int result;

    /* sanity check */
    if (tp == NULL)
        return 0;               /* ftime() always returns 0, see manpage */

    /* Check whether we've got a pointer to the real ftime() function yet */
    SINGLE_IF(has_real_ftime==0)
        real_ftime = NULL;
        real_ftime = dlsym(RTLD_NEXT, "ftime");

        /* check whether dlsym() worked */
        if (dlerror() == NULL) {
            has_real_ftime = 1;
        }
    END_SINGLE_IF
    if (!has_real_ftime) {  /* dlsym() failed */
#ifdef DEBUG
            (void) fprintf(stderr, "faketime problem: original ftime() not found.\n");
#endif
            tp = NULL;
            return 0; /* propagate error to caller */
    }

    /* initialize our result with the real current time */
    result = (*real_ftime)(tp);

    /* pass the real current ftime to our faking version, overwriting it */
    result = fake_ftime(tp);

    /* return the result to the caller */
    return result; /* will always be 0 (see manpage) */
}

int gettimeofday(struct timeval *tv, void *tz) {
#ifndef __APPLE__
    static int (*real_gettimeofday)(struct timeval *, void *);
    static int has_real_gettimeofday = 0;
#endif
    int result;

    /* sanity check */
    if (tv == NULL) {
        return -1;
    }

    /* Check whether we've got a pointer to the real ftime() function yet */
    SINGLE_IF(has_real_gettimeofday==0)
        real_gettimeofday = NULL;
        real_gettimeofday = dlsym(RTLD_NEXT, "gettimeofday");

        /* check whether dlsym() worked */
        if (dlerror() == NULL) {
            has_real_gettimeofday = 1;
        }
    END_SINGLE_IF
    if (!has_real_gettimeofday) {  /* dlsym() failed */
#ifdef DEBUG
            (void) fprintf(stderr, "faketime problem: original gettimeofday() not found.\n");
#endif
            return -1; /* propagate error to caller */
    }

    /* initialize our result with the real current time */
    result = (*real_gettimeofday)(tv, tz);
    if (result == -1) return result; /* original function failed */

    /* pass the real current time to our faking version, overwriting it */
    result = fake_gettimeofday(tv, tz);

    /* return the result to the caller */
    return result;
}

#ifdef POSIX_REALTIME
int clock_gettime(clockid_t clk_id, struct timespec *tp) {
    static int (*real_clock_gettime)(clockid_t clk_id, struct timespec *tp);
    static int has_real_clock_gettime = 0;
    int result;

    /* sanity check */
    if (tp == NULL) {
        return -1;
    }

    /* Check whether we've got a pointer to the real clock_gettime() function yet */
    SINGLE_IF(has_real_clock_gettime==0)
        real_clock_gettime = NULL;
        real_clock_gettime = dlsym(RTLD_NEXT, "clock_gettime");

        /* check whether dlsym() worked */
        if (dlerror() == NULL) {
            has_real_clock_gettime = 1;
        }
    END_SINGLE_IF
    if (!has_real_clock_gettime) {  /* dlsym() failed */
#ifdef DEBUG
            (void) fprintf(stderr, "faketime problem: original clock_gettime() not found.\n");
#endif
            return -1; /* propagate error to caller */
    }

    /* initialize our result with the real current time */
    result = (*real_clock_gettime)(clk_id, tp);
    if (result == -1) return result; /* original function failed */

    /* pass the real current time to our faking version, overwriting it */
    result = fake_clock_gettime(clk_id, tp);

    /* return the result to the caller */
    return result;
}
#endif

/*
 *  Static time_t to store our startup time, followed by a load-time library
 *  initialization declaration.
 */
static time_t ftpl_starttime = 0;

void __attribute__ ((constructor)) ftpl_init(void)
{
    time_t temp_tt;

    ft_shm_init();
#ifdef FAKE_STAT
    if (getenv("NO_FAKE_STAT")!=NULL) {
      fake_stat_disabled = 1;  //Note that this is NOT re-checked
    }
#endif

    ftpl_starttime = _ftpl_time(&temp_tt);
}

static void remove_trailing_eols(char *line)
{
	char *endp = line + strlen(line);
	/*
	 * erase the last char if it's a newline
	 * or carriage return, and back up.
	 * keep doing this, but don't back up
	 * past the beginning of the string.
	 */
# define is_eolchar(c) ((c) == '\n' || (c) == '\r')
	while (endp > line && is_eolchar(endp[-1]))
		*--endp = '\0';
}


time_t fake_time(time_t *time_tptr) {
    static char user_faked_time[BUFFERLEN]; /* changed to static for caching in v0.6 */
    struct tm user_faked_time_tm;
    time_t user_faked_time_time_t;
    long user_offset;
    double frac_user_offset;
    char filename[BUFSIZ], line[BUFFERLEN];
    FILE *faketimerc;
    static const char *user_faked_time_fmt = NULL;
    char * tmp_time_fmt;

    /* variables used for caching, introduced in version 0.6 */
    static time_t last_data_fetch = 0;  /* not fetched previously at first call */
    static int cache_expired = 1;       /* considered expired at first call */
    static int cache_duration = 10;     /* cache fake time input for 10 seconds */

#ifdef LIMITEDFAKING
    static long callcounter = 0;
    static int limited_initialized = 0;
    char envvarbuf[32];
    static long FAKETIME_START_AFTER_SECONDS = -1;
    static long FAKETIME_STOP_AFTER_SECONDS = -1;
    static long FAKETIME_START_AFTER_NUMCALLS = -1;
    static long FAKETIME_STOP_AFTER_NUMCALLS = -1;
#endif

#ifdef SPAWNSUPPORT
    static int spawned = 0;
    static long spawn_callcounter = 0;
    static int spawn_initialized = 0;
    char spawn_envvarbuf[32];
    static char FAKETIME_SPAWN_TARGET[1024];
    static long FAKETIME_SPAWN_SECONDS = -1;
    static long FAKETIME_SPAWN_NUMCALLS = -1;
#endif

/*
 * This no longer appears to be necessary in Mac OS X 10.7 Lion
 */
//#ifdef __APPLE__
//    static int malloc_arena = 0;
//#endif

#ifdef PTHREAD_SINGLETHREADED_TIME
static pthread_mutex_t time_mutex=PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&time_mutex);
    pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, (void *)&time_mutex);
#endif

    /* Sanity check by Karl Chan since v0.8 */
    if (time_tptr == NULL) return -1;

#ifdef LIMITEDFAKING
    /* Check whether we actually should be faking the returned timestamp. */

    if (ftpl_starttime > 0) {
        if (limited_initialized == 0) {
            if (getenv("FAKETIME_START_AFTER_SECONDS") != NULL) {
                (void) strncpy(envvarbuf, getenv("FAKETIME_START_AFTER_SECONDS"), 30);
                FAKETIME_START_AFTER_SECONDS = atol(envvarbuf);
            }
            if (getenv("FAKETIME_STOP_AFTER_SECONDS") != NULL) {
                (void) strncpy(envvarbuf, getenv("FAKETIME_STOP_AFTER_SECONDS"), 30);
                FAKETIME_STOP_AFTER_SECONDS = atol(envvarbuf);
            }
            if (getenv("FAKETIME_START_AFTER_NUMCALLS") != NULL) {
                (void) strncpy(envvarbuf, getenv("FAKETIME_START_AFTER_NUMCALLS"), 30);
                FAKETIME_START_AFTER_NUMCALLS = atol(envvarbuf);
            }
            if (getenv("FAKETIME_STOP_AFTER_NUMCALLS") != NULL) {
                (void) strncpy(envvarbuf, getenv("FAKETIME_STOP_AFTER_NUMCALLS"), 30);
                FAKETIME_STOP_AFTER_NUMCALLS = atol(envvarbuf);
            }
            limited_initialized = 1;
        }
        if ((callcounter + 1) >= callcounter) callcounter++;

        /* For debugging, output #seconds and #calls */
        /* fprintf(stderr, "(libfaketime limits -> runtime: %lu, callcounter: %lu\n", (*time_tptr - ftpl_starttime), callcounter); */
        if ((FAKETIME_START_AFTER_SECONDS != -1) && ((*time_tptr - ftpl_starttime) < FAKETIME_START_AFTER_SECONDS)) return *time_tptr;
        if ((FAKETIME_STOP_AFTER_SECONDS != -1) && ((*time_tptr - ftpl_starttime) >= FAKETIME_STOP_AFTER_SECONDS)) return *time_tptr;
        if ((FAKETIME_START_AFTER_NUMCALLS != -1) && (callcounter < FAKETIME_START_AFTER_NUMCALLS)) return *time_tptr;
        if ((FAKETIME_STOP_AFTER_NUMCALLS != -1) && (callcounter >= FAKETIME_STOP_AFTER_NUMCALLS)) return *time_tptr;
        /* fprintf(stderr, "(libfaketime limits -> runtime: %lu, callcounter: %lu continues\n", (*time_tptr - ftpl_starttime), callcounter); */

    }
#endif

#ifdef SPAWNSUPPORT
    /* check whether we should spawn an external command */

    if (ftpl_starttime > 0) {

        if(spawn_initialized == 0) {
            if (getenv("FAKETIME_SPAWN_TARGET") != NULL) {
                (void) strncpy(FAKETIME_SPAWN_TARGET, getenv("FAKETIME_SPAWN_TARGET"), 1024);

                if (getenv("FAKETIME_SPAWN_SECONDS") != NULL) {
                    (void) strncpy(spawn_envvarbuf, getenv("FAKETIME_SPAWN_SECONDS"), 30);
                    FAKETIME_SPAWN_SECONDS = atol(spawn_envvarbuf);
                }

                if (getenv("FAKETIME_SPAWN_NUMCALLS") != NULL) {
                    (void) strncpy(spawn_envvarbuf, getenv("FAKETIME_SPAWN_NUMCALLS"), 30);
                    FAKETIME_SPAWN_NUMCALLS = atol(spawn_envvarbuf);
                }
            }
            spawn_initialized = 1;
        }

        if (spawned == 0) { /* exec external command once only */
            if ((spawn_callcounter + 1) >= spawn_callcounter) spawn_callcounter++;
            if ((((*time_tptr - ftpl_starttime) == FAKETIME_SPAWN_SECONDS) || (spawn_callcounter == FAKETIME_SPAWN_NUMCALLS)) && (spawned == 0)) {
                spawned = 1;
                system(FAKETIME_SPAWN_TARGET);
            }

        }

    }
#endif


    if (last_data_fetch > 0) {
        if ((*time_tptr - last_data_fetch) > cache_duration) {
            cache_expired = 1;
        }
        else {
            cache_expired = 0;
        }
    }

#ifdef NO_CACHING
    cache_expired = 1;
#endif

    if (cache_expired == 1) {

        last_data_fetch = *time_tptr;

        /* Can be enabled for testing ...
        fprintf(stderr, "***************++ Cache expired ++**************\n");
        */

        /* initialize with default */
        snprintf(user_faked_time, BUFFERLEN, "+0");

        /* fake time supplied as environment variable? */
        if (getenv("FAKETIME") != NULL) {
                (void) strncpy(user_faked_time, getenv("FAKETIME"), BUFFERLEN-2);
                user_faked_time[BUFFERLEN-1] = 0;
        }
        else {
                /* check whether there's a .faketimerc in the user's home directory, or
                * a system-wide /etc/faketimerc present.
                * The /etc/faketimerc handling has been contributed by David Burley,
                * Jacob Moorman, and Wayne Davison of SourceForge, Inc. in version 0.6 */
                (void) snprintf(filename, BUFSIZ, "%s/.faketimerc", getenv("HOME"));
                if ((faketimerc = fopen(filename, "rt")) != NULL ||
                (faketimerc = fopen("/etc/faketimerc", "rt")) != NULL) {
                while(fgets(line, BUFFERLEN, faketimerc) != NULL) {
                        if ((strlen(line) > 1) && (line[0] != ' ') &&
                        (line[0] != '#') && (line[0] != ';')) {
                        remove_trailing_eols(line);
                        strncpy(user_faked_time, line, BUFFERLEN-1);
                        user_faked_time[BUFFERLEN-1] = 0;
                        break;
                        }
                }
                fclose(faketimerc);
                }
        } /* read fake time from file */


        user_faked_time_fmt = getenv("FAKETIME_FMT");
        if (user_faked_time_fmt == NULL)
            user_faked_time_fmt = "%Y-%m-%d %T";

    } /* cache had expired */

/*
 * This no longer appears to be necessary in Mac OS X 10.7 Lion
 */
//#ifdef __APPLE__
//    SINGLE_IF(malloc_arena==0)
//      malloc_arena = 1;
//        return *time_tptr;
//    END_SINGLE_IF
//#endif

    /* check whether the user gave us an absolute time to fake */
    switch (user_faked_time[0]) {

      default:  /* Try and interpret this as a specified time */
        user_faked_time_tm.tm_isdst = -1;
        (void) strptime(user_faked_time, user_faked_time_fmt, &user_faked_time_tm);

        user_faked_time_time_t = mktime(&user_faked_time_tm);
        if (user_faked_time_time_t != -1) {
            if (time_tptr != NULL) /* sanity check */
                *time_tptr = user_faked_time_time_t;
        }
        break;

      case '+':
      case '-': /* User-specified offset */
        /* fractional time offsets contributed by Karl Chen in v0.8 */
        frac_user_offset = atof(user_faked_time);

        /* offset is in seconds by default, but the string may contain
         * multipliers...
         */
        if (strchr(user_faked_time, 'm') != NULL) frac_user_offset *= 60;
        else if (strchr(user_faked_time, 'h') != NULL) frac_user_offset *= 60 * 60;
        else if (strchr(user_faked_time, 'd') != NULL) frac_user_offset *= 60 * 60 * 24;
        else if (strchr(user_faked_time, 'y') != NULL) frac_user_offset *= 60 * 60 * 24 * 365;

        /* Speed-up / slow-down contributed by Karl Chen in v0.8 */
        if (strchr(user_faked_time, 'x') != NULL) {
            const double rate = atof(strchr(user_faked_time, 'x')+1);
            const long tdiff = (long long) *time_tptr - (long long)ftpl_starttime;
            const double timeadj = tdiff * (rate - 1.0);
            *time_tptr += (long) timeadj;
        } else if (NULL != (tmp_time_fmt = strchr(user_faked_time, 'i'))) {
	    /* increment time with every time() call*/
	    *time_tptr += next_time(atof(tmp_time_fmt + 1));
        }

        *time_tptr += (long) frac_user_offset;

        break;

      /* Contributed by David North, TDI in version 0.7 */
      case '@': /* Specific time, but clock along relative to that starttime */
        user_faked_time_tm.tm_isdst = -1;
        (void) strptime(&user_faked_time[1], user_faked_time_fmt, &user_faked_time_tm);

        user_faked_time_time_t = mktime(&user_faked_time_tm);
        if (user_faked_time_time_t != -1) {
            user_offset = - ( (long long int)ftpl_starttime - (long long int)user_faked_time_time_t );

            /* Speed-up / slow-down contributed by Karl Chen in v0.8 */
            if (strchr(user_faked_time, 'x') != NULL) {
                const double rate = atof(strchr(user_faked_time, 'x')+1);
                const long tdiff = (long long) *time_tptr - (long long)ftpl_starttime;
                const double timeadj = tdiff * (rate - 1.0);
                *time_tptr += (long) timeadj;
            } else if (NULL != (tmp_time_fmt = strchr(user_faked_time, 'i'))) {
                /* increment time with every time() call*/
                *time_tptr += next_time(atof(tmp_time_fmt + 1));
            }

            *time_tptr += user_offset;
        }
        break;
    }

#ifdef PTHREAD_SINGLETHREADED_TIME
    pthread_cleanup_pop(1);
#endif

    /* pass the possibly modified time back to caller */
    return *time_tptr;
}

int fake_ftime(struct timeb *tp) {
    time_t temp_tt = tp->time;

    tp->time = fake_time(&temp_tt);

    return 0; /* always returns 0, see manpage */
}

int fake_gettimeofday(struct timeval *tv, void *tz) {
    time_t temp_tt = tv->tv_sec;

    tv->tv_sec = fake_time(&temp_tt);

    return 0;
}

#ifdef POSIX_REALTIME
int fake_clock_gettime(clockid_t clk_id, struct timespec *tp) {
    time_t temp_tt = tp->tv_sec;

    /* Fake only if the call is realtime clock related */
    if (clk_id == CLOCK_REALTIME) {
        tp->tv_sec = fake_time(&temp_tt);
    }

    return 0;
}
#endif

/*
 * This causes serious issues in Mac OS X 10.7 Lion and is disabled there
 */
#ifndef __APPLE__
/* Added in v0.7 as suggested by Jamie Cameron, Google */
#ifdef FAKE_INTERNAL_CALLS
int __gettimeofday(struct timeval *tv, void *tz) {
    return gettimeofday(tv, tz);
}

#ifdef POSIX_REALTIME
int __clock_gettime(clockid_t clk_id, struct timespec *tp) {
    return clock_gettime(clk_id, tp);
}
#endif

int __ftime(struct timeb *tp) {
    return ftime(tp);
}

time_t __time(time_t *time_tptr) {
    return time(time_tptr);
}
#endif
#endif

/* eof */
