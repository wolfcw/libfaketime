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
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>

#include "time_ops.h"
#include "faketime_common.h"

/* pthread-handling contributed by David North, TDI in version 0.7 */
#ifdef PTHREAD
#include <pthread.h>

#else

#endif

#include <sys/timeb.h>
#include <dlfcn.h>

#define BUFFERLEN   256

/* real pointer to faked functions */
static int (*real_stat) (int, const char *, struct stat *);
static int (*real_fstat) (int, int, struct stat *);
static int (*real_fstatat) (int, int, const char *, struct stat *, int);
static int (*real_lstat)(int, const char *, struct stat *);
static int (*real_stat64) (int, const char *, struct stat64 *);
static int (*real_fstat64)(int, int , struct stat64 *);
static int (*real_fstatat64)(int, int , const char *, struct stat64 *, int);
static int (*real_lstat64) (int, const char *, struct stat64 *);
static time_t (*real_time)(time_t *);
static int (*real_ftime)(struct timeb *);
static int (*real_gettimeofday)(struct timeval *, void *);
static int (*real_clock_gettime)(clockid_t clk_id, struct timespec *tp);
static int (*real_nanosleep)(const struct timespec *req, struct timespec *rem);
static int (*real_usleep)(useconds_t usec);
static unsigned int (*real_sleep)(unsigned int seconds);
static unsigned int (*real_alarm)(unsigned int seconds);

/* prototypes */
time_t fake_time(time_t *time_tptr);
int    fake_ftime(struct timeb *tp);
int    fake_gettimeofday(struct timeval *tv, void *tz);
int    fake_clock_gettime(clockid_t clk_id, struct timespec *tp);

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

/** Semaphore protecting shared data */
static sem_t *shared_sem = NULL;

/** Data shared among faketime-spawned processes */
static struct ft_shared_s *ft_shared = NULL;

/** Storage format for timestamps written to file. Big endian.*/
struct saved_timestamp {
  int64_t sec;
  uint64_t nsec;
};

static inline void timespec_from_saved (struct timespec *tp,
					struct saved_timestamp *saved)
{
  /* read as big endian */
#if __BYTE_ORDER == __BIG_ENDIAN
  tp->tv_sec = saved->sec;
  tp->tv_nsec = saved->nsec;
#else
  if (saved->sec < 0) {
    uint64_t abs_sec = 0 - saved->sec;
    ((uint32_t*)&(tp->tv_sec))[0] = ntohl(((uint32_t*)&abs_sec)[1]);
    ((uint32_t*)&(tp->tv_sec))[1] = ntohl(((uint32_t*)&abs_sec)[0]);
    tp->tv_sec = 0 - tp->tv_sec;
  } else {
    ((uint32_t*)&(tp->tv_sec))[0] = ntohl(((uint32_t*)&(saved->sec))[1]);
    ((uint32_t*)&(tp->tv_sec))[1] = ntohl(((uint32_t*)&(saved->sec))[0]);
  }
  ((uint32_t*)&(tp->tv_nsec))[0] = ntohl(((uint32_t*)&(saved->nsec))[1]);
  ((uint32_t*)&(tp->tv_nsec))[1] = ntohl(((uint32_t*)&(saved->nsec))[0]);

#endif
}

/** Saved timestamps */
static struct saved_timestamp *stss = NULL;
static size_t infile_size;
static bool infile_set = false;

/** File fd to save timestamps to */
static int outfile = -1;

static bool limited_faking = false;
static long callcounter = 0;
static long ft_start_after_secs = -1;
static long ft_stop_after_secs = -1;
static long ft_start_after_ncalls = -1;
static long ft_stop_after_ncalls = -1;


static bool spawnsupport = false;
static int spawned = 0;
static char ft_spawn_target[1024];
static long ft_spawn_secs = -1;
static long ft_spawn_ncalls = -1;


/**
 * Static timespec to store our startup time, followed by a load-time library
 * initialization declaration.
 */
static struct system_time_s ftpl_starttime = {{0, -1}, {0, -1}, {0, -1}};

static char user_faked_time_fmt[BUFSIZ] = {0};

/** User supplied base time to fake */
static struct timespec user_faked_time_timespec = {0, -1};
/** User supplied base time is set */
static bool user_faked_time_set = false;
/** Fractional user offset provided through FAKETIME env. var.*/
static struct timespec user_offset = {0, -1};
/** Speed up or slow down clock */
static double user_rate = 1.0;
static bool user_rate_set = false;
static struct timespec user_per_tick_inc = {0, -1};
static bool user_per_tick_inc_set = false;

enum ft_mode_t {FT_FREEZE, FT_START_AT} ft_mode = FT_FREEZE;

/** Time to fake is not provided through FAKETIME env. var. */
static bool parse_config_file = true;

void ft_cleanup (void) __attribute__ ((destructor));

static void ft_shm_init (void)
{
  int ticks_shm_fd;
  char sem_name[256], shm_name[256], *ft_shared_env = getenv("FAKETIME_SHARED");
  if (ft_shared_env != NULL) {
    if (sscanf(ft_shared_env, "%255s %255s", sem_name, shm_name) < 2 ) {
      printf("Error parsing semaphor name and shared memory id from string: %s", ft_shared_env);
      exit(1);
    }

    if (SEM_FAILED == (shared_sem = sem_open(sem_name, 0))) {
      perror("sem_open");
      exit(1);
    }

    if (-1 == (ticks_shm_fd = shm_open(shm_name, O_CREAT|O_RDWR, S_IWUSR|S_IRUSR))) {
      perror("shm_open");
      exit(1);
    }
    if (MAP_FAILED == (ft_shared = mmap(NULL, sizeof(struct ft_shared_s), PROT_READ|PROT_WRITE,
            MAP_SHARED, ticks_shm_fd, 0))) {
      perror("mmap");
      exit(1);
    }
  }
}

void ft_cleanup (void)
{
  /* detach from shared memory */
  if (ft_shared != NULL) {
    munmap(ft_shared, sizeof(uint64_t));
  }
  if (stss != NULL) {
    munmap(stss, infile_size);
  }
  if (shared_sem != NULL) {
    sem_close(shared_sem);
  }
}

/** Get system time from system for all clocks */
static void system_time_from_system (struct system_time_s * systime) {
#ifdef __APPLE__
  /* from http://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x */
  clock_serv_t cclock;
  mach_timespec_t mts;
  host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cclock);
  /* this is not faked */
  clock_get_time(cclock, &mts);
  mach_port_deallocate(mach_task_self(), cclock);
  systime->real.tv_sec = mts.tv_sec;
  systime->real.tv_nsec = mts.tv_nsec;
  systime->mon.tv_sec = mts.tv_sec;
  systime->mon.tv_nsec = mts.tv_nsec;
  systime->mon_raw.tv_sec = mts.tv_sec;
  systime->mon_raw.tv_nsec = mts.tv_nsec;
#else
  (*real_clock_gettime)(CLOCK_REALTIME, &systime->real);
  (*real_clock_gettime)(CLOCK_MONOTONIC, &systime->mon);
  (*real_clock_gettime)(CLOCK_MONOTONIC_RAW, &systime->mon_raw);
#endif
}


static void next_time(struct timespec *tp, struct timespec *ticklen)
{
  if (shared_sem != NULL) {
    struct timespec inc;
    /* lock */
    if (sem_wait(shared_sem) == -1) {
      perror("sem_wait");
      exit(1);
    }
    /* calculate and update elapsed time */
    timespecmul(ticklen, ft_shared->ticks, &inc);
    timespecadd(&user_faked_time_timespec, &inc, tp);
    (ft_shared->ticks)++;
    /* unlock */
    if (sem_post(shared_sem) == -1) {
      perror("sem_post");
      exit(1);
    }
  }
}

static void save_time(struct timespec *tp)
{
  if ((shared_sem != NULL) && (outfile != -1)) {
    struct saved_timestamp time_write;
    ssize_t n = 0;

    // write as big endian
#if __BYTE_ORDER == __BIG_ENDIAN
    time_write = {tp->tv_sec, tp->tv_nsec};
#else
  if (tp->tv_sec < 0) {
    uint64_t abs_sec = 0 - tp->tv_sec;
    ((uint32_t*)&(time_write.sec))[0] = htonl(((uint32_t*)&abs_sec)[1]);
    ((uint32_t*)&(time_write.sec))[1] = htonl(((uint32_t*)&abs_sec)[0]);
    tp->tv_sec = 0 - tp->tv_sec;
  } else {
    ((uint32_t*)&(time_write.sec))[0] = htonl(((uint32_t*)&(tp->tv_sec))[1]);
    ((uint32_t*)&(time_write.sec))[1] = htonl(((uint32_t*)&(tp->tv_sec))[0]);
  }
    ((uint32_t*)&(time_write.nsec))[0] = htonl(((uint32_t*)&(tp->tv_nsec))[1]);
    ((uint32_t*)&(time_write.nsec))[1] = htonl(((uint32_t*)&(tp->tv_nsec))[0]);
#endif
    /* lock */
    if (sem_wait(shared_sem) == -1) {
      perror("sem_wait");
      exit(1);
    }

    lseek(outfile, 0, SEEK_END);
    while ((sizeof(time_write) < (n += write(outfile, &(((char*)&time_write)[n]),
					     sizeof(time_write) - n))) &&
	   (errno == EINTR));

    if ((n == -1) || (n < sizeof(time_write))) {
      perror("Saving timestamp to file failed");
    }

    /* unlock */
    if (sem_post(shared_sem) == -1) {
      perror("sem_post");
      exit(1);
    }
  }
}

/**
 * Provide faked time from file.
 * @return time is set from filen
 */
static bool load_time(struct timespec *tp)
{
  bool ret = false;
  if ((shared_sem != NULL) && (infile_set)) {

    /* lock */
    if (sem_wait(shared_sem) == -1) {
      perror("sem_wait");
      exit(1);
    }

    if ((sizeof(stss[0]) * (ft_shared->file_idx + 1)) > infile_size) {
      /* we are out of timstamps to replay, return to faking time by rules
       * using last timestamp from file as the user provided timestamp */
      timespec_from_saved(&user_faked_time_timespec, &stss[(infile_size / sizeof(stss[0])) - 1 ]);

      if (ft_shared->ticks == 0) {
	/* we set shared memory to stop using infile */
	ft_shared->ticks = 1;
	system_time_from_system(&ftpl_starttime);
	ft_shared->start_time = ftpl_starttime;
      } else {
	ftpl_starttime = ft_shared->start_time;
      }

      munmap(stss, infile_size);
      infile_set = false;
    } else {
      timespec_from_saved(tp, &stss[ft_shared->file_idx]);
      ft_shared->file_idx++;
      ret = true;
    }

    /* unlock */
    if (sem_post(shared_sem) == -1) {
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
  if (NULL == real_stat) {  /* dlsym() failed */
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
  if (NULL == real_fstat) {  /* dlsym() failed */
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

  if (NULL == real_fstatat) {  /* dlsym() failed */
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
  if (NULL == real_lstat) {  /* dlsym() failed */
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
  if (NULL == real_stat64) {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original stat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result=real_stat64(ver, path, buf);
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
  if (NULL == real_fstat64) {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result = real_fstat64(ver, fildes, buf);
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
  if (NULL == real_fstatat64) {  /* dlsym() failed */
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
  if (NULL == real_lstat64) {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original lstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result = real_lstat64(ver, path, buf);
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

/**
 * Faked nanosleep()
 */
int nanosleep(const struct timespec *req, struct timespec *rem)
{
  int result;
  struct timespec real_req;

  if (real_nanosleep == NULL) {
    return -1;
  }
  if (req != NULL) {
    if (user_rate_set) {
      timespecmul(req, 1.0 / user_rate, &real_req);
    } else {
      real_req = *req;
    }
  } else {
    return -1;
  }

  result = (*real_nanosleep)(&real_req, rem);
  if (result == -1) {
    return result;
  }

  /* fake returned parts */
  if ((rem != NULL) && ((rem->tv_sec != 0) || (rem->tv_nsec != 0))) {
    if (user_rate_set) {
      timespecmul(rem, user_rate, rem);
    }
  }
  /* return the result to the caller */
  return result;
}

/**
 * Faked usleep()
 */
int usleep(useconds_t usec)
{

  if (real_usleep == NULL) {
    return -1;
  }

  return (*real_usleep)((user_rate_set)?((1.0 / user_rate) * usec):usec);
}

/**
 * Faked sleep()
 */
unsigned int sleep(unsigned int seconds)
{
  unsigned int ret;
  if (real_sleep == NULL) {
    return 0;
  }

  ret = (*real_sleep)((user_rate_set)?((1.0 / user_rate) * seconds):seconds);
  return (user_rate_set)?(user_rate * ret):ret;
}

/**
 * Faked alarm()
 */
unsigned int alarm(unsigned int seconds)
{
  unsigned int ret;
  if (real_alarm == NULL) {
    return -1;
  }

  ret = (*real_alarm)((user_rate_set)?((1.0 / user_rate) * seconds):seconds);
  return (user_rate_set)?(user_rate * ret):ret;
}

time_t time(time_t *time_tptr) {
    time_t result;
    time_t null_dummy;
    if (time_tptr == NULL) {
        time_tptr = &null_dummy;
        /* (void) fprintf(stderr, "NULL pointer caught in time().\n"); */
    }
    result = (*real_time)(time_tptr);
    if (result == ((time_t) -1)) return result;

    /* pass the real current time to our faking version, overwriting it */
    result = fake_time(time_tptr);

    /* return the result to the caller */
    return result;
}


int ftime(struct timeb *tp) {
    int result;

    /* sanity check */
    if (tp == NULL)
        return 0;               /* ftime() always returns 0, see manpage */

    /* Check whether we've got a pointer to the real ftime() function yet */
    if (NULL == real_ftime) {  /* dlsym() failed */
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
    int result;

    /* sanity check */
    if (tv == NULL) {
        return -1;
    }

    /* Check whether we've got a pointer to the real ftime() function yet */
    if (NULL == real_gettimeofday) {  /* dlsym() failed */
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

int clock_gettime(clockid_t clk_id, struct timespec *tp) {
    int result;

    /* sanity check */
    if (tp == NULL) {
        return -1;
    }

    if (NULL == real_clock_gettime) {  /* dlsym() failed */
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

static void parse_ft_string(const char *user_faked_time)
{
  struct tm user_faked_time_tm;
  char * tmp_time_fmt;
  /* check whether the user gave us an absolute time to fake */
  switch (user_faked_time[0]) {

  default:  /* Try and interpret this as a specified time */
    ft_mode = FT_FREEZE;
    user_faked_time_tm.tm_isdst = -1;
    if (NULL != strptime(user_faked_time, user_faked_time_fmt,
			 &user_faked_time_tm)) {
      user_faked_time_timespec.tv_sec = mktime(&user_faked_time_tm);
      user_faked_time_timespec.tv_nsec = 0;
      user_faked_time_set = true;
    }
    break;

  case '+':
  case '-': /* User-specified offset */
    ft_mode = FT_START_AT;
    /* fractional time offsets contributed by Karl Chen in v0.8 */
    double frac_offset = atof(user_faked_time);

    /* offset is in seconds by default, but the string may contain
     * multipliers...
     */
    if (strchr(user_faked_time, 'm') != NULL) frac_offset *= 60;
    else if (strchr(user_faked_time, 'h') != NULL) frac_offset *= 60 * 60;
    else if (strchr(user_faked_time, 'd') != NULL) frac_offset *= 60 * 60 * 24;
    else if (strchr(user_faked_time, 'y') != NULL) frac_offset *= 60 * 60 * 24 * 365;

    user_offset.tv_sec = floor(frac_offset);
    user_offset.tv_nsec = (frac_offset - user_offset.tv_sec) * SEC_TO_nSEC;
    timespecadd(&ftpl_starttime.real, &user_offset, &user_faked_time_timespec);
    goto parse_modifiers;
    break;

    /* Contributed by David North, TDI in version 0.7 */
  case '@': /* Specific time, but clock along relative to that starttime */
    ft_mode = FT_START_AT;
    user_faked_time_tm.tm_isdst = -1;
    (void) strptime(&user_faked_time[1], user_faked_time_fmt, &user_faked_time_tm);

    user_faked_time_timespec.tv_sec = mktime(&user_faked_time_tm);
    user_faked_time_timespec.tv_nsec = 0;
  parse_modifiers:
    /* Speed-up / slow-down contributed by Karl Chen in v0.8 */
    if (strchr(user_faked_time, 'x') != NULL) {
      user_rate = atof(strchr(user_faked_time, 'x')+1);
      user_rate_set = true;
    } else if (NULL != (tmp_time_fmt = strchr(user_faked_time, 'i'))) {
      double tick_inc = atof(tmp_time_fmt + 1);
      /* increment time with every time() call*/
      user_per_tick_inc.tv_sec = floor(tick_inc);
      user_per_tick_inc.tv_nsec = (tick_inc - user_per_tick_inc.tv_sec) * SEC_TO_nSEC ;
      user_per_tick_inc_set = true;
    }
    break;
  }
}

void __attribute__ ((constructor)) ftpl_init(void)
{
    char *tmp_env;

    /* Look up all real_* functions. NULL will mark missing ones. */
    real_stat = dlsym(RTLD_NEXT, "__xstat");
    real_fstat = dlsym(RTLD_NEXT, "__fxstat");
    real_fstatat = dlsym(RTLD_NEXT, "__fxstatat");
    real_lstat = dlsym(RTLD_NEXT, "__lxstat");
    real_stat64 = dlsym(RTLD_NEXT,"__xstat64");
    real_fstat64 = dlsym(RTLD_NEXT, "__fxstat64");
    real_fstatat64 = dlsym(RTLD_NEXT, "__fxstatat64");
    real_lstat64 = dlsym(RTLD_NEXT, "__lxstat64");
    real_time = dlsym(RTLD_NEXT, "time");
    real_ftime = dlsym(RTLD_NEXT, "ftime");
    real_gettimeofday = dlsym(RTLD_NEXT, "gettimeofday");
    real_clock_gettime = dlsym(RTLD_NEXT, "clock_gettime");
    real_nanosleep = dlsym(RTLD_NEXT, "nanosleep");
    real_usleep = dlsym(RTLD_NEXT, "usleep");
    real_sleep = dlsym(RTLD_NEXT, "sleep");
    real_alarm = dlsym(RTLD_NEXT, "alarm");

    ft_shm_init();
#ifdef FAKE_STAT
    if (getenv("NO_FAKE_STAT")!=NULL) {
      fake_stat_disabled = 1;  //Note that this is NOT re-checked
    }
#endif

    /* Check whether we actually should be faking the returned timestamp. */

    if ((tmp_env = getenv("FAKETIME_START_AFTER_SECONDS")) != NULL) {
      ft_start_after_secs = atol(tmp_env);
      limited_faking = true;
    }
    if ((tmp_env = getenv("FAKETIME_STOP_AFTER_SECONDS")) != NULL) {
      ft_stop_after_secs = atol(tmp_env);
      limited_faking = true;
    }
    if ((tmp_env = getenv("FAKETIME_START_AFTER_NUMCALLS")) != NULL) {
      ft_start_after_ncalls = atol(tmp_env);
      limited_faking = true;
    }
    if ((tmp_env = getenv("FAKETIME_STOP_AFTER_NUMCALLS")) != NULL) {
      ft_stop_after_ncalls = atol(tmp_env);
      limited_faking = true;
    }

    /* check whether we should spawn an external command */
    if ((tmp_env = getenv("FAKETIME_SPAWN_TARGET")) != NULL) {
      spawnsupport = true;
      (void) strncpy(ft_spawn_target, getenv("FAKETIME_SPAWN_TARGET"), 1024);

      if ((tmp_env = getenv("FAKETIME_SPAWN_SECONDS")) != NULL) {
	ft_spawn_secs = atol(tmp_env);
      }
      if ((tmp_env = getenv("FAKETIME_SPAWN_NUMCALLS")) != NULL) {
	ft_spawn_ncalls = atol(tmp_env);
      }
    }

    if ((tmp_env = getenv("FAKETIME_SAVE_FILE")) != NULL) {
      if (-1 == (outfile = open(tmp_env, O_RDWR | O_APPEND | O_CLOEXEC | O_CREAT,
				S_IWUSR | S_IRUSR))) {
	perror("Opening file for saving timestamps failed");
	exit(EXIT_FAILURE);
      }
    }

    /* load file only if reading timstamps from it is not finished yet */
    if ((tmp_env = getenv("FAKETIME_LOAD_FILE")) != NULL) {
      int infile = -1;
      struct stat sb;
      if (-1 == (infile = open(tmp_env, O_RDONLY|O_CLOEXEC))) {
	perror("Opening file for loading timestamps failed");
	exit(EXIT_FAILURE);
      }

      fstat(infile, &sb);
      if (sizeof(stss[0]) > (infile_size = sb.st_size)) {
	printf("There are no timstamps in the provided file to load timesamps from");
	exit(EXIT_FAILURE);
      }

      if ((infile_size % sizeof(stss[0])) != 0) {
	printf("File size is not multiple of timstamp size. It is probably damaged.");
	exit(EXIT_FAILURE);
      }

      stss = mmap(NULL, infile_size, PROT_READ, MAP_SHARED, infile, 0);
      if (stss == MAP_FAILED) {
	perror("Mapping file for loading timestamps failed");
	exit(EXIT_FAILURE);
      };
      infile_set = true;
    }

    tmp_env = getenv("FAKETIME_FMT");
    if (tmp_env == NULL) {
      strcpy(user_faked_time_fmt, "%Y-%m-%d %T");
    } else {
      strncpy(user_faked_time_fmt, tmp_env, BUFSIZ);
    }

    if (shared_sem != 0) {
      if (sem_wait(shared_sem) == -1) {
	perror("sem_wait");
	exit(1);
      }
      if (ft_shared->start_time.real.tv_nsec == -1) {
	/* set up global start time */
	system_time_from_system(&ftpl_starttime);
	ft_shared->start_time = ftpl_starttime;
      } else {
	/** get preset start time */
	ftpl_starttime = ft_shared->start_time;
      }
      if (sem_post(shared_sem) == -1) {
	perror("sem_post");
	exit(1);
      }

    } else {
      system_time_from_system(&ftpl_starttime);
    }
    /* fake time supplied as environment variable? */
    if (NULL != (tmp_env = getenv("FAKETIME"))) {
      parse_config_file = false;
      parse_ft_string(tmp_env);
    }

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


int fake_clock_gettime(clockid_t clk_id, struct timespec *tp) {


    /* variables used for caching, introduced in version 0.6 */
    static time_t last_data_fetch = 0;  /* not fetched previously at first call */
    static int cache_expired = 1;       /* considered expired at first call */
    static int cache_duration = 10;     /* cache fake time input for 10 seconds */

    /* Per process timers are only sped up or slowed down */
    if ((clk_id == CLOCK_PROCESS_CPUTIME_ID ) || (clk_id == CLOCK_THREAD_CPUTIME_ID)) {
      if (user_rate_set) {
	timespecmul(tp, user_rate, tp);
      }
      return 0;
    }

    /* Sanity check by Karl Chan since v0.8 */
    if (tp == NULL) return -1;

#ifdef PTHREAD_SINGLETHREADED_TIME
static pthread_mutex_t time_mutex=PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_lock(&time_mutex);
    pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, (void *)&time_mutex);
#endif


    if ((limited_faking &&
	 ((ft_start_after_ncalls != -1) || (ft_stop_after_ncalls != -1))) ||
	(spawnsupport && ft_spawn_ncalls)) {
      if ((callcounter + 1) >= callcounter) callcounter++;
    }

    if (limited_faking || spawnsupport) {
      struct timespec tmp_ts;
      /* For debugging, output #seconds and #calls */
      switch (clk_id) {
      case CLOCK_REALTIME:
	timespecsub(tp, &ftpl_starttime.real, &tmp_ts);
	break;
      case CLOCK_MONOTONIC:
	timespecsub(tp, &ftpl_starttime.mon, &tmp_ts);
	break;
      case CLOCK_MONOTONIC_RAW:
	timespecsub(tp, &ftpl_starttime.mon_raw, &tmp_ts);
	break;
      default:
	printf("Invalid clock_id for clock_gettime: %d", clk_id);
	exit(EXIT_FAILURE);
      }
      if (limited_faking) {
	/* Check whether we actually should be faking the returned timestamp. */
	/* fprintf(stderr, "(libfaketime limits -> runtime: %lu, callcounter: %lu\n", (*time_tptr - ftpl_starttime), callcounter); */
	if ((ft_start_after_secs != -1) && (tmp_ts.tv_sec < ft_start_after_secs)) return 0;
	if ((ft_stop_after_secs != -1) && (tmp_ts.tv_sec >= ft_stop_after_secs)) return 0;
	if ((ft_start_after_ncalls != -1) && (callcounter < ft_start_after_ncalls)) return 0;
	if ((ft_stop_after_ncalls != -1) && (callcounter >= ft_stop_after_ncalls)) return 0;
	/* fprintf(stderr, "(libfaketime limits -> runtime: %lu, callcounter: %lu continues\n", (*time_tptr - ftpl_starttime), callcounter); */

      }

      if (spawnsupport) {
	/* check whether we should spawn an external command */

	if (spawned == 0) { /* exec external command once only */
	  if (((tmp_ts.tv_sec == ft_spawn_secs) || (callcounter == ft_spawn_ncalls)) && (spawned == 0)) {
	    spawned = 1;
	    system(ft_spawn_target);
	  }
	}
      }
    }
    if (last_data_fetch > 0) {
        if ((tp->tv_sec - last_data_fetch) > cache_duration) {
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
        static char user_faked_time[BUFFERLEN]; /* changed to static for caching in v0.6 */
	char filename[BUFSIZ], line[BUFFERLEN];
	FILE *faketimerc;

        last_data_fetch = tp->tv_sec;

        /* Can be enabled for testing ...
        fprintf(stderr, "***************++ Cache expired ++**************\n");
        */

        /* initialize with default */
        snprintf(user_faked_time, BUFFERLEN, "+0");

        /* fake time supplied as environment variable? */
        if (parse_config_file) {
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
		parse_ft_string(user_faked_time);
        } /* read fake time from file */
    } /* cache had expired */

    if (infile_set) {
      if (load_time(tp)) {
	return 0;
      }
    }

    /* check whether the user gave us an absolute time to fake */
    switch (ft_mode) {
    case FT_FREEZE:  /* a specified time */
      if (user_faked_time_set) {
	  *tp = user_faked_time_timespec;
      }
      break;

    case FT_START_AT: /* User-specified offset */
      if (user_per_tick_inc_set) {
	/* increment time with every time() call*/
	next_time(tp, &user_per_tick_inc);
      } else {
	/* Speed-up / slow-down contributed by Karl Chen in v0.8 */
	struct timespec tdiff, timeadj;
	switch (clk_id) {
	case CLOCK_REALTIME:
	  timespecsub(tp, &ftpl_starttime.real, &tdiff);
	  break;
	case CLOCK_MONOTONIC:
	  timespecsub(tp, &ftpl_starttime.mon, &tdiff);
	  break;
	case CLOCK_MONOTONIC_RAW:
	  timespecsub(tp, &ftpl_starttime.mon_raw, &tdiff);
	  break;
	default:
	  printf("Invalid clock_id for clock_gettime: %d", clk_id);
	  exit(EXIT_FAILURE);
	}
	if (user_rate_set) {
	  timespecmul(&tdiff, user_rate, &timeadj);
	} else {
	  timeadj = tdiff;
	}
	timespecadd(&user_faked_time_timespec, &timeadj, tp);
      }
      break;
    default:
      return -1;
    }

#ifdef PTHREAD_SINGLETHREADED_TIME
    pthread_cleanup_pop(1);
#endif
    save_time(tp);
    return 0;
}


time_t fake_time(time_t *time_tptr) {
  struct timespec tp;

  tp.tv_sec = *time_tptr;
  tp.tv_nsec = 0;
  (void)fake_clock_gettime(CLOCK_REALTIME, &tp);
  *time_tptr = tp.tv_sec;
  return *time_tptr;
}

int fake_ftime(struct timeb *tp) {
  struct timespec ts;
  int ret;
  ts.tv_sec = tp->time;
  ts.tv_nsec =tp->millitm * 1000000;

  ret = fake_clock_gettime(CLOCK_REALTIME, &ts);
  tp->time = ts.tv_sec;
  tp->millitm =ts.tv_nsec / 1000000;

  return ret;
}

int fake_gettimeofday(struct timeval *tv, void *tz) {
  struct timespec ts;
  int ret;
  ts.tv_sec = tv->tv_sec;
  ts.tv_nsec =tv->tv_usec * 1000;

  ret = fake_clock_gettime(CLOCK_REALTIME, &ts);
  tv->tv_sec = ts.tv_sec;
  tv->tv_usec =ts.tv_nsec / 1000;

  return ret;
}

/*
 * This causes serious issues in Mac OS X 10.7 Lion and is disabled there
 */
#ifndef __APPLE__
/* Added in v0.7 as suggested by Jamie Cameron, Google */
#ifdef FAKE_INTERNAL_CALLS
int __gettimeofday(struct timeval *tv, void *tz) {
    return gettimeofday(tv, tz);
}

int __clock_gettime(clockid_t clk_id, struct timespec *tp) {
    return clock_gettime(clk_id, tp);
}

int __ftime(struct timeb *tp) {
    return ftime(tp);
}

time_t __time(time_t *time_tptr) {
    return time(time_tptr);
}
#endif
#endif

/* eof */
