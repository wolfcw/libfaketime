/*
 *  This file is part of libfaketime, version 0.9.8
 *
 *  libfaketime is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License v2 as published by the
 *  Free Software Foundation.
 *
 *  libfaketime is distributed in the hope that it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 *  more details.
 *
 *  You should have received a copy of the GNU General Public License v2 along
 *  with the libfaketime; if not, write to the Free Software Foundation,
 *  Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 *      =======================================================================
 *      Global settings, includes, and macros                          === HEAD
 *      =======================================================================
 */

#define _GNU_SOURCE             /* required to get RTLD_NEXT defined */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#ifdef __linux__
#include <sys/epoll.h>
#endif
#include <time.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <limits.h>

#include "uthash.h"

#include "time_ops.h"
#include "faketime_common.h"


/* pthread-handling contributed by David North, TDI in version 0.7 */
#if defined PTHREAD_SINGLETHREADED_TIME || defined FAKE_PTHREAD
#include <pthread.h>
#endif

#include <sys/timeb.h>
#include <dlfcn.h>

#define BUFFERLEN   256

#ifndef __APPLE__
extern char *__progname;
#ifdef __sun
#include "sunos_endian.h"
#else
#include <endian.h>
#endif
#else
/* endianness related macros */
#ifndef OSSwapHostToBigInt64
#define OSSwapHostToBigInt64(x) ((uint64_t)(x))
#endif
#define htobe64(x) OSSwapHostToBigInt64(x)
#ifndef OSSwapHostToLittleInt64
#define OSSwapHostToLittleInt64(x) OSSwapInt64(x)
#endif
#define htole64(x) OSSwapHostToLittleInt64(x)
#ifndef OSSwapBigToHostInt64
#define OSSwapBigToHostInt64(x) ((uint64_t)(x))
#endif
#define be64toh(x) OSSwapBigToHostInt64(x)
#ifndef OSSwapLittleToHostInt64
#define OSSwapLittleToHostInt64(x) OSSwapInt64(x)
#endif
#define le64toh(x) OSSwapLittleToHostInt64(x)

/* clock_gettime() and related clock definitions are missing on __APPLE__ */
#ifndef CLOCK_REALTIME
/* from GNU C Library time.h */
/* Identifier for system-wide realtime clock. ( == 1) */
#define CLOCK_REALTIME               CALENDAR_CLOCK
/* Monotonic system-wide clock. (== 0) */
#define CLOCK_MONOTONIC              SYSTEM_CLOCK
/* High-resolution timer from the CPU.  */
#define CLOCK_PROCESS_CPUTIME_ID     2
/* Thread-specific CPU-time clock.  */
#define CLOCK_THREAD_CPUTIME_ID      3
/* Monotonic system-wide clock, not adjusted for frequency scaling.  */
#define CLOCK_MONOTONIC_RAW          4
typedef int clockid_t;
#include <mach/clock.h>
#include <mach/mach.h>
#endif
#endif

/* some systems lack raw clock */
#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW (CLOCK_MONOTONIC + 1)
#endif

/*
 * Per thread variable, which we turn on inside real_* calls to avoid modifying
 * time multiple times of for the whole process to prevent faking time
 */
static __thread bool dont_fake = false;

/* Wrapper for function calls, which we want to return system time */
#define DONT_FAKE_TIME(call)          \
  {                                   \
    bool dont_fake_orig = dont_fake;  \
    if (!dont_fake)                   \
    {                                 \
      dont_fake = true;               \
    }                                 \
    call;                             \
    dont_fake = dont_fake_orig;       \
  } while (0)

/* pointers to real (not faked) functions */
static int          (*real_stat)            (int, const char *, struct stat *);
static int          (*real_fstat)           (int, int, struct stat *);
static int          (*real_fstatat)         (int, int, const char *, struct stat *, int);
static int          (*real_lstat)           (int, const char *, struct stat *);
static int          (*real_stat64)          (int, const char *, struct stat64 *);
static int          (*real_fstat64)         (int, int , struct stat64 *);
static int          (*real_fstatat64)       (int, int , const char *, struct stat64 *, int);
static int          (*real_lstat64)         (int, const char *, struct stat64 *);
static time_t       (*real_time)            (time_t *);
static int          (*real_ftime)           (struct timeb *);
static int          (*real_gettimeofday)    (struct timeval *, void *);
static int          (*real_clock_gettime)   (clockid_t clk_id, struct timespec *tp);
#ifdef FAKE_INTERNAL_CALLS
static int          (*real___ftime)           (struct timeb *);
static int          (*real___gettimeofday)    (struct timeval *, void *);
static int          (*real___clock_gettime)   (clockid_t clk_id, struct timespec *tp);
#endif
#ifdef FAKE_PTHREAD
static int          (*real_pthread_cond_timedwait_225)  (pthread_cond_t *, pthread_mutex_t*, struct timespec *);
static int          (*real_pthread_cond_timedwait_232)  (pthread_cond_t *, pthread_mutex_t*, struct timespec *);
static int          (*real_pthread_cond_init_232) (pthread_cond_t *restrict, const pthread_condattr_t *restrict);
static int          (*real_pthread_cond_destroy_232) (pthread_cond_t *);
static pthread_rwlock_t monotonic_conds_lock;
#endif

#ifndef __APPLEOSX__
#ifdef FAKE_TIMERS
static int          (*real_timer_settime_22)   (int timerid, int flags, const struct itimerspec *new_value,
                                                struct itimerspec * old_value);
static int          (*real_timer_settime_233)  (timer_t timerid, int flags,
                                                const struct itimerspec *new_value,
                                                struct itimerspec * old_value);
static int          (*real_timer_gettime_22)   (int timerid,
                                                struct itimerspec *curr_value);
static int          (*real_timer_gettime_233)  (timer_t timerid,
                                                struct itimerspec *curr_value);
#endif
#endif
#ifdef FAKE_SLEEP
static int          (*real_nanosleep)       (const struct timespec *req, struct timespec *rem);
#ifndef __APPLE__
static int          (*real_clock_nanosleep) (clockid_t clock_id, int flags, const struct timespec *req, struct timespec *rem);
#endif
static int          (*real_usleep)          (useconds_t usec);
static unsigned int (*real_sleep)           (unsigned int seconds);
static unsigned int (*real_alarm)           (unsigned int seconds);
static int          (*real_poll)            (struct pollfd *, nfds_t, int);
static int          (*real_ppoll)           (struct pollfd *, nfds_t, const struct timespec *, const sigset_t *);
#ifdef __linux__
static int          (*real_epoll_wait)      (int epfd, struct epoll_event *events, int maxevents, int timeout);
static int          (*real_epoll_pwait)     (int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask);
#endif
static int          (*real_select)          (int nfds, fd_set *restrict readfds,
                                             fd_set *restrict writefds,
                                             fd_set *restrict errorfds,
                                             struct timeval *restrict timeout);
#ifdef __linux__
static int          (*real_pselect)         (int nfds, fd_set *restrict readfds,
                                             fd_set *restrict writefds,
                                             fd_set *restrict errorfds,
                                             const struct timespec *timeout,
                                             const sigset_t *sigmask);
#endif
static int          (*real_sem_timedwait)   (sem_t*, const struct timespec*);
#endif
#ifdef __APPLEOSX__
static int          (*real_clock_get_time)  (clock_serv_t clock_serv, mach_timespec_t *cur_timeclockid_t);
static int          apple_clock_gettime     (clockid_t clk_id, struct timespec *tp);
static clock_serv_t clock_serv_real;
#endif

static int initialized = 0;

/* prototypes */
static int    fake_gettimeofday(struct timeval *tv);
static int    fake_clock_gettime(clockid_t clk_id, struct timespec *tp);

/** Semaphore protecting shared data */
static sem_t *shared_sem = NULL;

/** Data shared among faketime-spawned processes */
static struct ft_shared_s *ft_shared = NULL;

/** Storage format for timestamps written to file. Big endian.*/
struct saved_timestamp
{
  int64_t sec;
  uint64_t nsec;
};

static inline void timespec_from_saved (struct timespec *tp,
  struct saved_timestamp *saved)
{
  /* read as big endian */
  tp->tv_sec = be64toh(saved->sec);
  tp->tv_nsec = be64toh(saved->nsec);
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

#ifdef __ARM_ARCH
static int fake_monotonic_clock = 0;
#else
static int fake_monotonic_clock = 1;
#endif
static int cache_enabled = 1;
static int cache_duration = 10;     /* cache fake time input for 10 seconds */
static int force_cache_expiration = 0;

/*
 * Static timespec to store our startup time, followed by a load-time library
 * initialization declaration.
 */
#ifndef CLOCK_BOOTTIME
static struct system_time_s ftpl_starttime = {{0, -1}, {0, -1}, {0, -1}};
static struct system_time_s ftpl_timecache = {{0, -1}, {0, -1}, {0, -1}};
static struct system_time_s ftpl_faketimecache = {{0, -1}, {0, -1}, {0, -1}};
#else
static struct system_time_s ftpl_starttime = {{0, -1}, {0, -1}, {0, -1}, {0, -1}};
static struct system_time_s ftpl_timecache = {{0, -1}, {0, -1}, {0, -1}, {0, -1}};
static struct system_time_s ftpl_faketimecache = {{0, -1}, {0, -1}, {0, -1}, {0, -1}};
#endif

static char user_faked_time_fmt[BUFSIZ] = {0};

/* User supplied base time to fake */
static struct timespec user_faked_time_timespec = {0, -1};
/* User supplied base time is set */
static bool user_faked_time_set = false;
static char user_faked_time_saved[BUFFERLEN] = {0};

/* Fractional user offset provided through FAKETIME env. var.*/
static struct timespec user_offset = {0, -1};
/* Speed up or slow down clock */
static double user_rate = 1.0;
static bool user_rate_set = false;
static struct timespec user_per_tick_inc = {0, -1};
static bool user_per_tick_inc_set = false;
enum ft_mode_t {FT_FREEZE, FT_START_AT, FT_NOOP} ft_mode = FT_FREEZE;

/* Time to fake is not provided through FAKETIME env. var. */
static bool parse_config_file = true;

static void ft_cleanup (void) __attribute__ ((destructor));
static void ftpl_init (void) __attribute__ ((constructor));


/*
 *      =======================================================================
 *      Shared memory related functions                                 === SHM
 *      =======================================================================
 */

static bool shmCreator = false;

static void ft_shm_create(void) {
  char sem_name[256], shm_name[256], sem_nameT[256], shm_nameT[256];
  int shm_fdN;
  sem_t *semN;
  struct ft_shared_s *ft_sharedN;
  char shared_objsN[513];
  sem_t *shared_semT = NULL;

  snprintf(sem_name, 255, "/faketime_sem_%ld", (long)getpid());
  snprintf(shm_name, 255, "/faketime_shm_%ld", (long)getpid());
  if (SEM_FAILED == (semN = sem_open(sem_name, O_CREAT|O_EXCL, S_IWUSR|S_IRUSR, 1)))
  { /* silently fail on platforms that do not support sem_open() */
    return;
  }
  /* create shm */
  if (-1 == (shm_fdN = shm_open(shm_name, O_CREAT|O_EXCL|O_RDWR, S_IWUSR|S_IRUSR)))
  {
    perror("libfaketime: In ft_shm_create(), shm_open failed");
    exit(EXIT_FAILURE);
  }
  /* set shm size */
  if (-1 == ftruncate(shm_fdN, sizeof(uint64_t)))
  {
    perror("libfaketime: In ft_shm_create(), ftruncate failed");
    exit(EXIT_FAILURE);
  }
  /* map shm */
  if (MAP_FAILED == (ft_sharedN = mmap(NULL, sizeof(struct ft_shared_s), PROT_READ|PROT_WRITE,
                     MAP_SHARED, shm_fdN, 0)))
  {
    perror("libfaketime: In ft_shm_create(), mmap failed");
    exit(EXIT_FAILURE);
  }
  if (sem_wait(semN) == -1)
  {
    perror("libfaketime: In ft_shm_create(), sem_wait failed");
    exit(EXIT_FAILURE);
  }
  /* init elapsed time ticks to zero */
  ft_sharedN->ticks = 0;
  ft_sharedN->file_idx = 0;
  ft_sharedN->start_time.real.tv_sec = 0;
  ft_sharedN->start_time.real.tv_nsec = -1;
  ft_sharedN->start_time.mon.tv_sec = 0;
  ft_sharedN->start_time.mon.tv_nsec = -1;
  ft_sharedN->start_time.mon_raw.tv_sec = 0;
  ft_sharedN->start_time.mon_raw.tv_nsec = -1;

  if (-1 == munmap(ft_sharedN, (sizeof(struct ft_shared_s))))
  {
    perror("libfaketime: In ft_shm_create(), munmap failed");
    exit(EXIT_FAILURE);
  }
  if (sem_post(semN) == -1)
  {
    perror("libfaketime: In ft_shm_create(), sem_post failed");
    exit(EXIT_FAILURE);
  }

  snprintf(shared_objsN, sizeof(shared_objsN), "%s %s", sem_name, shm_name);

  int semSafetyCheckPassed = 0;
  sem_close(semN);

  sscanf(shared_objsN, "%255s %255s", sem_nameT, shm_nameT);
  if (SEM_FAILED == (shared_semT = sem_open(sem_nameT, 0)))
  {
      fprintf(stderr, "libfaketime: In ft_shm_create(), non-fatal sem_open issue with %s", sem_nameT);
  }
  else {
    semSafetyCheckPassed = 1;
    sem_close(shared_semT);
  }

  if (semSafetyCheckPassed == 1) {
    setenv("FAKETIME_SHARED", shared_objsN, true);
    shmCreator = true;
  }
}

static void ft_shm_destroy(void)
{
  char sem_name[256], shm_name[256], *ft_shared_env = getenv("FAKETIME_SHARED");

  if (ft_shared_env != NULL)
  {
    if (sscanf(ft_shared_env, "%255s %255s", sem_name, shm_name) < 2)
    {
      printf("libfaketime: In ft_shm_destroy(), error parsing semaphore name and shared memory id from string: %s", ft_shared_env);
      exit(1);
    }
    /*
       To avoid shared memory / semaphores left after quitting, we have to clean
       up here similar to how the faketime wrapper does.
       However, there is no guarantee that all child processes have quit before
       we clean up here, which potentially leaves us in a stale state.
       Since there is no easy solution for this problem (see issue #56),
       ft_shm_init() below at least tries to handle this carefully.
    */
    sem_unlink(sem_name);
    shm_unlink(shm_name);
    unsetenv("FAKETIME_SHARED");
  }
}

static void ft_shm_init (void)
{
  int ticks_shm_fd;
  char sem_name[256], shm_name[256], *ft_shared_env = getenv("FAKETIME_SHARED");
  sem_t *shared_semR = NULL;

  /* create semaphore and shared memory locally unless it has been passed along */
  if (ft_shared_env == NULL)
  {
    ft_shm_create();
    ft_shared_env = getenv("FAKETIME_SHARED");
  }

  /* check for stale semaphore / shared memory information */
  if (ft_shared_env != NULL)
  {
    if (sscanf(ft_shared_env, "%255s %255s", sem_name, shm_name) < 2)
    {
      printf("libfaketime: In ft_shm_init(), error parsing semaphore name and shared memory id from string: %s", ft_shared_env);
      exit(1);
    }
    if (SEM_FAILED == (shared_semR = sem_open(sem_name, 0))) /* gone stale? */
    {
      ft_shm_create();
      ft_shared_env = getenv("FAKETIME_SHARED");
    }
    else
    {
      sem_close(shared_semR);
    }
  }

  /* process the semaphore / shared memory information */
  if (ft_shared_env != NULL)
  {
    if (sscanf(ft_shared_env, "%255s %255s", sem_name, shm_name) < 2)
    {
      printf("libfaketime: In ft_shm_init(), error parsing semaphore name and shared memory id from string: %s", ft_shared_env);
      exit(1);
    }

    if (SEM_FAILED == (shared_sem = sem_open(sem_name, 0)))
    {
      perror("libfaketime: In ft_shm_init(), sem_open failed");
      fprintf(stderr, "libfaketime: sem_name was %s, created locally: %s\n", sem_name, shmCreator ? "true":"false");
      fprintf(stderr, "libfaketime: parsed from env: %s\n", ft_shared_env);
      exit(1);
    }

    if (-1 == (ticks_shm_fd = shm_open(shm_name, O_CREAT|O_RDWR, S_IWUSR|S_IRUSR)))
    {
      perror("libfaketime: In ft_shm_init(), shm_open failed");
      exit(1);
    }

    if (MAP_FAILED == (ft_shared = mmap(NULL, sizeof(struct ft_shared_s), PROT_READ|PROT_WRITE,
            MAP_SHARED, ticks_shm_fd, 0)))
    {
      perror("libfaketime: In ft_shm_init(), mmap failed");
      exit(1);
    }
  }
}

static void ft_cleanup (void)
{
  /* detach from shared memory */
  if (ft_shared != NULL)
  {
    munmap(ft_shared, sizeof(uint64_t));
  }
  if (stss != NULL)
  {
    munmap(stss, infile_size);
  }
  if (shared_sem != NULL)
  {
    sem_close(shared_sem);
  }
#ifdef FAKE_PTHREAD
  if (pthread_rwlock_destroy(&monotonic_conds_lock) != 0) {
    fprintf(stderr, "libfaketime: In ft_cleanup(), monotonic_conds_lock destroy failed\n");
    exit(-1);
  }
#endif
  if (shmCreator == true) ft_shm_destroy();
}


/*
 *      =======================================================================
 *      Internal time retrieval                                     === INTTIME
 *      =======================================================================
 */

/* Get system time from system for all clocks */
static void system_time_from_system (struct system_time_s * systime)
{
#ifdef __APPLEOSX__
  /* from http://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x */
  clock_serv_t cclock;
  mach_timespec_t mts;
  host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &clock_serv_real);
  (*real_clock_get_time)(clock_serv_real, &mts);
  systime->real.tv_sec = mts.tv_sec;
  systime->real.tv_nsec = mts.tv_nsec;
  host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &cclock);
  (*real_clock_get_time)(cclock, &mts);
  mach_port_deallocate(mach_task_self(), cclock);
  systime->mon.tv_sec = mts.tv_sec;
  systime->mon.tv_nsec = mts.tv_nsec;
  systime->mon_raw.tv_sec = mts.tv_sec;
  systime->mon_raw.tv_nsec = mts.tv_nsec;
#else
  DONT_FAKE_TIME((*real_clock_gettime)(CLOCK_REALTIME, &systime->real))
   ;
  DONT_FAKE_TIME((*real_clock_gettime)(CLOCK_MONOTONIC, &systime->mon))
   ;
  DONT_FAKE_TIME((*real_clock_gettime)(CLOCK_MONOTONIC_RAW, &systime->mon_raw))
   ;
#ifdef CLOCK_BOOTTIME
  DONT_FAKE_TIME((*real_clock_gettime)(CLOCK_BOOTTIME, &systime->boot))
   ;
#endif
#endif
}

static void next_time(struct timespec *tp, struct timespec *ticklen)
{
  if (shared_sem != NULL)
  {
    struct timespec inc;
    /* lock */
    if (sem_wait(shared_sem) == -1)
    {
      perror("libfaketime: In next_time(), sem_wait failed");
      exit(1);
    }
    /* calculate and update elapsed time */
    timespecmul(ticklen, ft_shared->ticks, &inc);
    timespecadd(&user_faked_time_timespec, &inc, tp);
    (ft_shared->ticks)++;
    /* unlock */
    if (sem_post(shared_sem) == -1)
    {
      perror("libfaketime: In next_time(), sem_post failed");
      exit(1);
    }
  }
}


/*
 *      =======================================================================
 *      Saving & loading time                                          === SAVE
 *      =======================================================================
 */

static void save_time(struct timespec *tp)
{
  if ((shared_sem != NULL) && (outfile != -1))
  {
    struct saved_timestamp time_write;
    ssize_t written;
    size_t n = 0;

    time_write.sec = htobe64(tp->tv_sec);
    time_write.nsec = htobe64(tp->tv_nsec);

    /* lock */
    if (sem_wait(shared_sem) == -1)
    {
      perror("libfaketime: In save_time(), sem_wait failed");
      exit(1);
    }

    lseek(outfile, 0, SEEK_END);
    do
    {
      written = write(outfile, &(((char*)&time_write)[n]), sizeof(time_write) - n);
    }
    while (((written == -1) && (errno == EINTR)) ||
            (sizeof(time_write) < (n += written)));

    if ((written == -1) || (n < sizeof(time_write)))
    {
      perror("libfaketime: In save_time(), saving timestamp to file failed");
    }

    /* unlock */
    if (sem_post(shared_sem) == -1)
    {
      perror("libfaketime: In save_time(), sem_post failed");
      exit(1);
    }
  }
}

/*
 * Provide faked time from file.
 * @return time is set from filen
 */
static bool load_time(struct timespec *tp)
{
  bool ret = false;
  if ((shared_sem != NULL) && (infile_set))
  {
    /* lock */
    if (sem_wait(shared_sem) == -1)
    {
      perror("libfaketime: In load_time(), sem_wait failed");
      exit(1);
    }

    if ((sizeof(stss[0]) * (ft_shared->file_idx + 1)) > infile_size)
    {
      /* we are out of timstamps to replay, return to faking time by rules
       * using last timestamp from file as the user provided timestamp */
      timespec_from_saved(&user_faked_time_timespec, &stss[(infile_size / sizeof(stss[0])) - 1 ]);

      if (ft_shared->ticks == 0)
      {
        /* we set shared memory to stop using infile */
        ft_shared->ticks = 1;
        system_time_from_system(&ftpl_starttime);
        ft_shared->start_time = ftpl_starttime;
      }
      else
      {
        ftpl_starttime = ft_shared->start_time;
      }

      munmap(stss, infile_size);
      infile_set = false;
    }
    else
    {
      timespec_from_saved(tp, &stss[ft_shared->file_idx]);
      ft_shared->file_idx++;
      ret = true;
    }

    /* unlock */
    if (sem_post(shared_sem) == -1)
    {
      perror("libfaketime: In load_time(), sem_post failed");
      exit(1);
    }
  }
  return ret;
}


/*
 *      =======================================================================
 *      Faked system functions: file related                     === FAKE(FILE)
 *      =======================================================================
 */

#ifdef FAKE_STAT

#ifndef NO_ATFILE
#ifndef _ATFILE_SOURCE
#define _ATFILE_SOURCE
#endif
#include <fcntl.h> /* Definition of AT_* constants */
#endif

#include <sys/stat.h>

static int fake_stat_disabled = 0;
static bool user_per_tick_inc_set_backup = false;

void lock_for_stat()
{
  if (shared_sem != NULL)
  {
    if (sem_wait(shared_sem) == -1)
    {
      perror("libfaketime: In lock_for_stat(), sem_wait failed");
      exit(1);
    }
  }
  user_per_tick_inc_set_backup = user_per_tick_inc_set;
  user_per_tick_inc_set = false;
  return;
}

void unlock_for_stat()
{
  user_per_tick_inc_set = user_per_tick_inc_set_backup;

  if (shared_sem != NULL)
  {
    if (sem_post(shared_sem) == -1)
    {
      perror("libfaketime: In unlock_for_stat(), sem_post failed");
      exit(1);
    }
  }
  return;
}

#define FAKE_STRUCT_STAT_TIME(which) {                \
    struct timespec t = {buf->st_##which##time,       \
                         buf->st_##which##timensec};  \
    fake_clock_gettime(CLOCK_REALTIME, &t);           \
    buf->st_##which##time = t.tv_sec;                 \
    buf->st_##which##timensec = t.tv_nsec;            \
  } while (0)

static inline void fake_statbuf (struct stat *buf) {
#ifndef st_atime
  lock_for_stat();
  FAKE_STRUCT_STAT_TIME(c);
  FAKE_STRUCT_STAT_TIME(a);
  FAKE_STRUCT_STAT_TIME(m);
  unlock_for_stat();
#else
  lock_for_stat();
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_ctim);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_atim);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_mtim);
  unlock_for_stat();
#endif
}

static inline void fake_stat64buf (struct stat64 *buf) {
#ifndef st_atime
  lock_for_stat();
  FAKE_STRUCT_STAT_TIME(c);
  FAKE_STRUCT_STAT_TIME(a);
  FAKE_STRUCT_STAT_TIME(m);
  unlock_for_stat();
#else
  lock_for_stat();
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_ctim);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_atim);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_mtim);
  unlock_for_stat();
#endif
}

/* Contributed by Philipp Hachtmann in version 0.6 */
int __xstat (int ver, const char *path, struct stat *buf)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_stat)
  { /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original stat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_stat(ver, path, buf));
  if (result == -1)
  {
    return -1;
  }

   if (buf != NULL)
   {
     if (!fake_stat_disabled)
     {
       fake_statbuf(buf);
     }
   }

  return result;
}

/* Contributed by Philipp Hachtmann in version 0.6 */
int __fxstat (int ver, int fildes, struct stat *buf)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_fstat)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_fstat(ver, fildes, buf));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_statbuf(buf);
    }
  }
  return result;
}

/* Added in v0.8 as suggested by Daniel Kahn Gillmor */
#ifndef NO_ATFILE
int __fxstatat(int ver, int fildes, const char *filename, struct stat *buf, int flag)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_fstatat)
  { /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstatat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_fstatat(ver, fildes, filename, buf, flag));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_statbuf(buf);
    }
  }
  return result;
}
#endif

/* Contributed by Philipp Hachtmann in version 0.6 */
int __lxstat (int ver, const char *path, struct stat *buf)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_lstat)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original lstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_lstat(ver, path, buf));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_statbuf(buf);
    }
  }
  return result;
}

/* Contributed by Philipp Hachtmann in version 0.6 */
int __xstat64 (int ver, const char *path, struct stat64 *buf)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_stat64)
  { /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original stat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_stat64(ver, path, buf));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_stat64buf(buf);
    }
  }
  return result;
}

/* Contributed by Philipp Hachtmann in version 0.6 */
int __fxstat64 (int ver, int fildes, struct stat64 *buf)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_fstat64)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_fstat64(ver, fildes, buf));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_stat64buf(buf);
    }
  }
  return result;
}

/* Added in v0.8 as suggested by Daniel Kahn Gillmor */
#ifndef NO_ATFILE
int __fxstatat64 (int ver, int fildes, const char *filename, struct stat64 *buf, int flag)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_fstatat64)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original fstatat64() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_fstatat64(ver, fildes, filename, buf, flag));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_stat64buf(buf);
    }
  }
  return result;
}
#endif

/* Contributed by Philipp Hachtmann in version 0.6 */
int __lxstat64 (int ver, const char *path, struct stat64 *buf)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (NULL == real_lstat64)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original lstat() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  int result;
  DONT_FAKE_TIME(result = real_lstat64(ver, path, buf));
  if (result == -1)
  {
    return -1;
  }

  if (buf != NULL)
  {
    if (!fake_stat_disabled)
    {
      fake_stat64buf(buf);
    }
  }
  return result;
}
#endif

/*
 *      =======================================================================
 *      Faked system functions: sleep/alarm/poll/timer related  === FAKE(SLEEP)
 *      =======================================================================
 *      Contributed by Balint Reczey in v0.9.5
 */

#ifdef FAKE_SLEEP
/*
 * Faked nanosleep()
 */
int nanosleep(const struct timespec *req, struct timespec *rem)
{
  int result;
  struct timespec real_req;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_nanosleep == NULL)
  {
    return -1;
  }
  if (req != NULL)
  {
    if (user_rate_set && !dont_fake)
    {
      timespecmul(req, 1.0 / user_rate, &real_req);
    }
    else
    {
      real_req = *req;
    }
  }
  else
  {
    return -1;
  }

  DONT_FAKE_TIME(result = (*real_nanosleep)(&real_req, rem));
  if (result == -1)
  {
    return result;
  }

  /* fake returned parts */
  if ((rem != NULL) && ((rem->tv_sec != 0) || (rem->tv_nsec != 0)))
  {
    if (user_rate_set && !dont_fake)
    {
      timespecmul(rem, user_rate, rem);
    }
  }
  /* return the result to the caller */
  return result;
}

#ifndef __APPLE__
/*
 * Faked clock_nanosleep()
 */
int clock_nanosleep(clockid_t clock_id, int flags, const struct timespec *req, struct timespec *rem)
{
  int result;
  struct timespec real_req;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_clock_nanosleep == NULL)
  {
    return -1;
  }
  if (req != NULL)
  {
    if (flags & TIMER_ABSTIME) /* sleep until absolute time */
    {
      struct timespec tdiff, timeadj;
      timespecsub(req, &user_faked_time_timespec, &timeadj);
      if (user_rate_set)
      {
        timespecmul(&timeadj, 1.0/user_rate, &tdiff);
      }
      else
      {
        tdiff = timeadj;
      }
      if (clock_id == CLOCK_REALTIME)
      {
        timespecadd(&ftpl_starttime.real, &tdiff, &real_req);
      }
      else if (clock_id == CLOCK_MONOTONIC)
      {
        timespecadd(&ftpl_starttime.mon, &tdiff, &real_req);
      }
      else /* presumably only CLOCK_PROCESS_CPUTIME_ID, leave untouched */
      {
       real_req = *req;
      }
    }
    else /* sleep for a relative time interval */
    {
      if (user_rate_set && !dont_fake && ((clock_id == CLOCK_REALTIME) || (clock_id == CLOCK_MONOTONIC))) /* don't touch CLOCK_PROCESS_CPUTIME_ID */
      {
        timespecmul(req, 1.0 / user_rate, &real_req);
      }
      else
      {
        real_req = *req;
      }
    }
  }
  else
  {
    return -1;
  }

  DONT_FAKE_TIME(result = (*real_clock_nanosleep)(clock_id, flags, &real_req, rem));
  if (result == -1)
  {
    return result;
  }
  /* fake returned parts */
  if ((rem != NULL) && ((rem->tv_sec != 0) || (rem->tv_nsec != 0)))
  {
    if (user_rate_set && !dont_fake)
    {
      timespecmul(rem, user_rate, rem);
    }
  }
  /* return the result to the caller */
  return result;
}
#endif

/*
 * Faked usleep()
 */
int usleep(useconds_t usec)
{
  int result;

  if (!initialized)
  {
    ftpl_init();
  }
  if (user_rate_set && !dont_fake)
  {
    struct timespec real_req;

    if (real_nanosleep == NULL)
    {
      /* fall back to usleep() */
      if (real_usleep == NULL)
      {
        return -1;
      }
      DONT_FAKE_TIME(result = (*real_usleep)((1.0 / user_rate) * usec));
      return result;
    }

    real_req.tv_sec = usec / 1000000;
    real_req.tv_nsec = (usec % 1000000) * 1000;
    timespecmul(&real_req, 1.0 / user_rate, &real_req);
    DONT_FAKE_TIME(result = (*real_nanosleep)(&real_req, NULL));
  }
  else
  {
    DONT_FAKE_TIME(result = (*real_usleep)(usec));
  }
  return result;
}

/*
 * Faked sleep()
 */
unsigned int sleep(unsigned int seconds)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (user_rate_set && !dont_fake)
  {
    if (real_nanosleep == NULL)
    {
      /* fall back to sleep */
      unsigned int ret;
      if (real_sleep == NULL)
      {
        return 0;
      }
      DONT_FAKE_TIME(ret = (*real_sleep)((1.0 / user_rate) * seconds));
      return (user_rate_set && !dont_fake)?(user_rate * ret):ret;
    }
    else
    {
      int result;
      struct timespec real_req = {seconds, 0}, rem;
      timespecmul(&real_req, 1.0 / user_rate, &real_req);
      DONT_FAKE_TIME(result = (*real_nanosleep)(&real_req, &rem));
      if (result == -1)
      {
        return 0;
      }

      /* fake returned parts */
      if ((rem.tv_sec != 0) || (rem.tv_nsec != 0))
      {
        timespecmul(&rem, user_rate, &rem);
      }
      /* return the result to the caller */
      return rem.tv_sec;
    }
  }
  else
  {
    /* no need to fake anything */
    unsigned int ret;
    DONT_FAKE_TIME(ret = (*real_sleep)(seconds));
    return ret;
  }
}

/*
 * Faked alarm()
 * @note due to rounding alarm(2) with faketime -f '+0 x7' won't wait 2/7
 * wall clock seconds but 0 seconds
 */
unsigned int alarm(unsigned int seconds)
{
  unsigned int ret;
  unsigned int seconds_real = (user_rate_set && !dont_fake)?((1.0 / user_rate) * seconds):seconds;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_alarm == NULL)
  {
    return -1;
  }

  DONT_FAKE_TIME(ret = (*real_alarm)(seconds_real));
  return (user_rate_set && !dont_fake)?(user_rate * ret):ret;
}

/*
 * Faked ppoll()
 */
int ppoll(struct pollfd *fds, nfds_t nfds,
    const struct timespec *timeout_ts, const sigset_t *sigmask)
{
  struct timespec real_timeout, *real_timeout_pt;
  int ret;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_ppoll == NULL)
  {
    return -1;
  }
  if (timeout_ts != NULL)
  {
    if (user_rate_set && !dont_fake && (timeout_ts->tv_sec > 0))
    {
      timespecmul(timeout_ts, 1.0 / user_rate, &real_timeout);
      real_timeout_pt = &real_timeout;
    }
    else
    {
      /* cast away constness */
      real_timeout_pt = (struct timespec *)timeout_ts;
    }
  }
  else
  {
    real_timeout_pt = NULL;
  }

  DONT_FAKE_TIME(ret = (*real_ppoll)(fds, nfds, real_timeout_pt, sigmask));
  return ret;
}

#ifdef __linux__
/*
 * Faked epoll_wait()
 */
int epoll_wait(int epfd, struct epoll_event *events, int maxevents, int timeout)
{
  int ret, real_timeout;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_epoll_wait == NULL)
  {
    return -1;
  }
  if (user_rate_set && !dont_fake && timeout > 0)
  {
    real_timeout = (int) timeout * 1.0/user_rate;
  }
  else
  {
    real_timeout = timeout;
  }
  DONT_FAKE_TIME(ret = (*real_epoll_wait)(epfd, events, maxevents, real_timeout));
  return ret;
}

/*
 * Faked epoll_pwait()
 */
int epoll_pwait(int epfd, struct epoll_event *events, int maxevents, int timeout, const sigset_t *sigmask)
{
  int ret, real_timeout;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_epoll_pwait == NULL)
  {
    return -1;
  }
  if (user_rate_set && !dont_fake && timeout > 0)
  {
    real_timeout = (int) timeout * 1.0/user_rate;
  }
  else
  {
    real_timeout = timeout;
  }
  DONT_FAKE_TIME(ret = (*real_epoll_pwait)(epfd, events, maxevents, real_timeout, sigmask));
  return ret;
}
#endif

/*
 * Faked poll()
 */
int poll(struct pollfd *fds, nfds_t nfds, int timeout)
{
  int ret, timeout_real = (user_rate_set && !dont_fake && (timeout > 0))?(timeout / user_rate):timeout;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_poll == NULL)
  {
    return -1;
  }

  DONT_FAKE_TIME(ret = (*real_poll)(fds, nfds, timeout_real));
  return ret;
}

/*
 * Faked select()
 */
int select(int nfds, fd_set *readfds,
           fd_set *writefds,
           fd_set *errorfds,
           struct timeval *timeout)
{
  int ret;
  struct timeval timeout_real;

  if (!initialized)
  {
    ftpl_init();
  }

  if (real_select == NULL)
  {
    return -1;
  }

  if (timeout != NULL)
  {
    if (user_rate_set && !dont_fake && (timeout->tv_sec > 0 || timeout->tv_usec > 0))
    {
      struct timespec ts;

      ts.tv_sec = timeout->tv_sec;
      ts.tv_nsec = timeout->tv_usec * 1000;

      timespecmul(&ts, 1.0 / user_rate, &ts);

      timeout_real.tv_sec = ts.tv_sec;
      timeout_real.tv_usec = ts.tv_nsec / 1000;
    }
    else
    {
      timeout_real.tv_sec = timeout->tv_sec;
      timeout_real.tv_usec = timeout->tv_usec;
    }
  }

  DONT_FAKE_TIME(ret = (*real_select)(nfds, readfds, writefds, errorfds, timeout == NULL ? timeout : &timeout_real));
  return ret;
}

#ifdef __linux__
/*
 * Faked pselect()
 */
int pselect(int nfds, fd_set *readfds,
            fd_set *writefds,
            fd_set *errorfds,
            const struct timespec *timeout,
            const sigset_t *sigmask)
{
  int ret;
  struct timespec timeout_real;

  if (!initialized)
  {
    ftpl_init();
  }

  if (real_pselect == NULL)
  {
    return -1;
  }

  if (timeout != NULL)
  {
    if (user_rate_set && !dont_fake && (timeout->tv_sec > 0 || timeout->tv_nsec > 0))
    {
      timespecmul(timeout, 1.0 / user_rate, &timeout_real);
    }
    else
    {
      timeout_real.tv_sec = timeout->tv_sec;
      timeout_real.tv_nsec = timeout->tv_nsec;
    }
  }

  DONT_FAKE_TIME(ret = (*real_pselect)(nfds, readfds, writefds, errorfds, timeout == NULL ? timeout : &timeout_real, sigmask));
  return ret;
}
#endif

int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout)
{
  int result;
  struct timespec real_abs_timeout, *real_abs_timeout_pt;

  /* sanity check */
  if (abs_timeout == NULL)
  {
    return -1;
  }

  if (NULL == real_sem_timedwait)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original sem_timedwait() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  if (!dont_fake)
  {
    struct timespec tdiff, timeadj;

    timespecsub(abs_timeout, &user_faked_time_timespec, &tdiff);

    if (user_rate_set)
    {
      timespecmul(&tdiff, user_rate, &timeadj);
    }
    else
    {
        timeadj = tdiff;
    }
    timespecadd(&ftpl_starttime.real, &timeadj, &real_abs_timeout);
    real_abs_timeout_pt = &real_abs_timeout;
  }
  else
  {
    /* cast away constness */
    real_abs_timeout_pt = (struct timespec *)abs_timeout;
  }

  DONT_FAKE_TIME(result = (*real_sem_timedwait)(sem, real_abs_timeout_pt));
  return result;
}
#endif

#ifndef __APPLE__
#ifdef FAKE_TIMERS

/* timer related functions and structures */
typedef union {
  int int_member;
  timer_t timer_t_member;
} timer_t_or_int;

/*
 * Faketime's function implementation's compatibility mode
 */
typedef enum {FT_COMPAT_GLIBC_2_2, FT_COMPAT_GLIBC_2_3_3} ft_lib_compat_timer;


/*
 * Faked timer_settime()
 * Does not affect timer speed when stepping clock with each time() call.
 */
static int
timer_settime_common(timer_t_or_int timerid, int flags,
         const struct itimerspec *new_value,
         struct itimerspec *old_value, ft_lib_compat_timer compat)
{
  int result;
  struct itimerspec new_real;
  struct itimerspec *new_real_pt = &new_real;

  if (!initialized)
  {
    ftpl_init();
  }
  if (new_value == NULL)
  {
    new_real_pt = NULL;
  }
  else if (dont_fake)
  {
    /* cast away constness*/
    new_real_pt = (struct itimerspec *)new_value;
  }
  else
  {
    /* set it_value */
    if ((new_value->it_value.tv_sec != 0) ||
        (new_value->it_value.tv_nsec != 0))
    {
      if (flags & TIMER_ABSTIME)
      {
        struct timespec tdiff, timeadj;
        timespecsub(&new_value->it_value, &user_faked_time_timespec, &timeadj);
        if (user_rate_set)
        {
          timespecmul(&timeadj, 1.0/user_rate, &tdiff);
        }
        else
        {
          tdiff = timeadj;
        }
        /* only CLOCK_REALTIME is handled */
        timespecadd(&ftpl_starttime.real, &tdiff, &new_real.it_value);
      }
      else
      {
        if (user_rate_set)
        {
          timespecmul(&new_value->it_value, 1.0/user_rate, &new_real.it_value);
        }
        else
        {
          new_real.it_value = new_value->it_value;
        }
      }
    }
    else
    {
      new_real.it_value = new_value->it_value;
    }
    /* set it_interval */
    if (user_rate_set && ((new_value->it_interval.tv_sec != 0) ||
       (new_value->it_interval.tv_nsec != 0)))
    {
      timespecmul(&new_value->it_interval, 1.0/user_rate, &new_real.it_interval);
    }
    else
    {
      new_real.it_interval = new_value->it_interval;
    }
  }

  switch (compat)
  {
    case FT_COMPAT_GLIBC_2_2:
      DONT_FAKE_TIME(result = (*real_timer_settime_22)(timerid.int_member, flags,
                    new_real_pt, old_value));
      break;
    case FT_COMPAT_GLIBC_2_3_3:
       DONT_FAKE_TIME(result = (*real_timer_settime_233)(timerid.timer_t_member,
                    flags, new_real_pt, old_value));
       break;
    default:
      result = -1;
      break;
  }

  if (result == -1)
  {
    return result;
  }

  /* fake returned parts */
  if ((old_value != NULL) && !dont_fake)
  {
    if ((old_value->it_value.tv_sec != 0) ||
        (old_value->it_value.tv_nsec != 0))
    {
      result = fake_clock_gettime(CLOCK_REALTIME, &old_value->it_value);
    }
    if (user_rate_set && ((old_value->it_interval.tv_sec != 0) ||
       (old_value->it_interval.tv_nsec != 0)))
    {
      timespecmul(&old_value->it_interval, user_rate, &old_value->it_interval);
    }
  }

  /* return the result to the caller */
  return result;
}

/*
 * Faked timer_settime() compatible with implementation in GLIBC 2.2
 */
int timer_settime_22(int timerid, int flags,
         const struct itimerspec *new_value,
         struct itimerspec *old_value)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (real_timer_settime_22 == NULL)
  {
    return -1;
  }
  else
  {
    return (timer_settime_common((timer_t_or_int)timerid, flags, new_value, old_value,
            FT_COMPAT_GLIBC_2_2));
  }
}

/*
 * Faked timer_settime() compatible with implementation in GLIBC 2.3.3
 */
int timer_settime_233(timer_t timerid, int flags,
      const struct itimerspec *new_value,
      struct itimerspec *old_value)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (real_timer_settime_233 == NULL)
  {
    return -1;
  }
  else
  {
    return (timer_settime_common((timer_t_or_int)timerid, flags, new_value, old_value,
            FT_COMPAT_GLIBC_2_3_3));
  }
}

/*
 * Faked timer_gettime()
 * Does not affect timer speed when stepping clock with each time() call.
 */
int timer_gettime_common(timer_t_or_int timerid, struct itimerspec *curr_value, ft_lib_compat_timer compat)
{
  int result;

  if (!initialized)
  {
    ftpl_init();
  }
  if (real_timer_gettime_233 == NULL)
  {
    return -1;
  }

  switch (compat)
  {
    case FT_COMPAT_GLIBC_2_2:
      DONT_FAKE_TIME(result = (*real_timer_gettime_22)(timerid.int_member, curr_value));
      break;
    case FT_COMPAT_GLIBC_2_3_3:
      DONT_FAKE_TIME(result = (*real_timer_gettime_233)(timerid.timer_t_member, curr_value));
      break;
    default:
      result = -1;
      break;
  }

  if (result == -1)
  {
    return result;
  }

  /* fake returned parts */
  if (curr_value != NULL)
  {
    if (user_rate_set && !dont_fake)
    {
      timespecmul(&curr_value->it_interval, user_rate, &curr_value->it_interval);
      timespecmul(&curr_value->it_value, user_rate, &curr_value->it_value);
    }
  }
  /* return the result to the caller */
  return result;
}

/*
 * Faked timer_gettime() compatible with implementation in GLIBC 2.2
 */
int timer_gettime_22(timer_t timerid, struct itimerspec *curr_value)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (real_timer_gettime_22 == NULL)
  {
    return -1;
  }
  else
  {
    return (timer_gettime_common((timer_t_or_int)timerid, curr_value,
         FT_COMPAT_GLIBC_2_2));
  }
}

/*
 * Faked timer_gettime() compatible with implementation in GLIBC 2.3.3
 */
int timer_gettime_233(timer_t timerid, struct itimerspec *curr_value)
{
  if (!initialized)
  {
    ftpl_init();
  }
  if (real_timer_gettime_233 == NULL)
  {
    return -1;
  }
  else
  {
    return (timer_gettime_common((timer_t_or_int)timerid, curr_value,
            FT_COMPAT_GLIBC_2_3_3));
  }
}

__asm__(".symver timer_gettime_22, timer_gettime@GLIBC_2.2");
__asm__(".symver timer_gettime_233, timer_gettime@@GLIBC_2.3.3");
__asm__(".symver timer_settime_22, timer_settime@GLIBC_2.2");
__asm__(".symver timer_settime_233, timer_settime@@GLIBC_2.3.3");

#endif
#endif


/*
 *      =======================================================================
 *      Faked system functions: basic time functions             === FAKE(TIME)
 *      =======================================================================
 */

/*
 * time() implementation using clock_gettime()
 * @note Does not check for EFAULT, see man 2 time
 */
time_t time(time_t *time_tptr)
{
  struct timespec tp;
  time_t result;

  if (!initialized)
  {
    ftpl_init();
  }
  DONT_FAKE_TIME(result = (*real_clock_gettime)(CLOCK_REALTIME, &tp));
  if (result == -1) return -1;

  /* pass the real current time to our faking version, overwriting it */
  (void)fake_clock_gettime(CLOCK_REALTIME, &tp);

  if (time_tptr != NULL)
  {
    *time_tptr = tp.tv_sec;
  }
  return tp.tv_sec;
}

int ftime(struct timeb *tb)
{
  struct timespec tp;
  int result;

  if (!initialized)
  {
    ftpl_init();
  }
  /* sanity check */
  if (tb == NULL)
    return 0;               /* ftime() always returns 0, see manpage */

  /* Check whether we've got a pointer to the real ftime() function yet */
  if (NULL == real_ftime)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original ftime() not found.\n");
#endif
    return 0; /* propagate error to caller */
  }

  /* initialize our TZ result with the real current time */
  DONT_FAKE_TIME(result = (*real_ftime)(tb));
  if (result == -1)
  {
    return result;
  }

  DONT_FAKE_TIME(result = (*real_clock_gettime)(CLOCK_REALTIME, &tp));
  if (result == -1) return -1;

  /* pass the real current time to our faking version, overwriting it */
  (void)fake_clock_gettime(CLOCK_REALTIME, &tp);

  tb->time = tp.tv_sec;
  tb->millitm = tp.tv_nsec / 1000000;

  /* return the result to the caller */
  return result; /* will always be 0 (see manpage) */
}

int gettimeofday(struct timeval *tv, void *tz)
{
  int result;

  if (!initialized)
  {
    ftpl_init();
  }
  /* sanity check */
  if (tv == NULL)
  {
    return -1;
  }

  /* Check whether we've got a pointer to the real ftime() function yet */
  if (NULL == real_gettimeofday)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original gettimeofday() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  /* initialize our result with the real current time */
  DONT_FAKE_TIME(result = (*real_gettimeofday)(tv, tz));
  if (result == -1) return result; /* original function failed */

  /* pass the real current time to our faking version, overwriting it */
  result = fake_gettimeofday(tv);

  /* return the result to the caller */
  return result;
}

int clock_gettime(clockid_t clk_id, struct timespec *tp)
{
  int result;
  static int recursion_depth = 0;

  if (!initialized)
  {
    recursion_depth++;
    if (recursion_depth == 2)
    {
      fprintf(stderr, "libfaketime: Unexpected recursive calls to clock_gettime() without proper initialization. Trying alternative.\n");
      DONT_FAKE_TIME(ftpl_init()) ;
    }
    else if (recursion_depth == 3)
    {
      fprintf(stderr, "libfaketime: Cannot recover from unexpected recursive calls to clock_gettime().\n");
      fprintf(stderr, "libfaketime:  Please check whether any other libraries are in use that clash with libfaketime.\n");
      fprintf(stderr, "libfaketime:  Returning -1 on clock_gettime() to break recursion now... if that does not work, please check other libraries' error handling.\n");
      if (tp != NULL)
      {
        tp->tv_sec = 0;
        tp->tv_nsec = 0;
      }
      return -1;
    }
    else {
      ftpl_init();
    }
    recursion_depth--;
  }
  /* sanity check */
  if (tp == NULL)
  {
    return -1;
  }

  if (NULL == real_clock_gettime)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original clock_gettime() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  /* initialize our result with the real current time */
  DONT_FAKE_TIME(result = (*real_clock_gettime)(clk_id, tp));
  if (result == -1) return result; /* original function failed */

  /* pass the real current time to our faking version, overwriting it */
  if (fake_monotonic_clock || (clk_id != CLOCK_MONOTONIC && clk_id != CLOCK_MONOTONIC_RAW
#ifdef CLOCK_MONOTONIC_COARSE
      && clk_id != CLOCK_MONOTONIC_COARSE
#endif
#ifdef CLOCK_BOOTTIME
      && clk_id != CLOCK_BOOTTIME
#endif
      ))
  {
    result = fake_clock_gettime(clk_id, tp);
  }

  /* return the result to the caller */
  return result;
}


/*
 *      =======================================================================
 *      Parsing the user's faketime requests                          === PARSE
 *      =======================================================================
 */

static void parse_ft_string(const char *user_faked_time)
{
  struct tm user_faked_time_tm;
  char * tmp_time_fmt;
  char * nstime_str;

  if (!strncmp(user_faked_time, user_faked_time_saved, BUFFERLEN))
  {
      /* No change */
      return;
  }

  /* check whether the user gave us an absolute time to fake */
  switch (user_faked_time[0])
  {

    default:  /* Try and interpret this as a specified time */
      if (ft_mode != FT_NOOP) ft_mode = FT_FREEZE;
      user_faked_time_tm.tm_isdst = -1;
      nstime_str = strptime(user_faked_time, user_faked_time_fmt, &user_faked_time_tm);

      if (NULL != nstime_str)
      {
        user_faked_time_timespec.tv_sec = mktime(&user_faked_time_tm);
        user_faked_time_timespec.tv_nsec = 0;

        if (nstime_str[0] == '.')
        {
          double nstime = atof(--nstime_str);
          user_faked_time_timespec.tv_nsec = (nstime - floor(nstime)) * SEC_TO_nSEC;
        }
        user_faked_time_set = true;
      }
      else
      {
        perror("libfaketime: In parse_ft_string(), failed to parse FAKETIME timestamp");
        fprintf(stderr, "Please check specification %s with format %s\n", user_faked_time, user_faked_time_fmt);
        exit(EXIT_FAILURE);
      }
      break;

    case '+':
    case '-': /* User-specified offset */
      if (ft_mode != FT_NOOP) ft_mode = FT_START_AT;
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
      nstime_str = strptime(&user_faked_time[1], user_faked_time_fmt, &user_faked_time_tm);
      if (NULL != nstime_str)
      {
        user_faked_time_timespec.tv_sec = mktime(&user_faked_time_tm);
        user_faked_time_timespec.tv_nsec = 0;

        if (nstime_str[0] == '.')
        {
          double nstime = atof(--nstime_str);
          user_faked_time_timespec.tv_nsec = (nstime - floor(nstime)) * SEC_TO_nSEC;
        }
      }
      else
      {
        perror("libfaketime: In parse_ft_string(), failed to parse FAKETIME timestamp");
        exit(EXIT_FAILURE);
      }

      /* Reset starttime */
      if (NULL == getenv("FAKETIME_DONT_RESET"))
        system_time_from_system(&ftpl_starttime);
      goto parse_modifiers;
      break;

    case '%': /* follow file timestamp as suggested by Hitoshi Harada (umitanuki) */
      ft_mode = FT_START_AT;
      struct stat master_file_stats;
      int ret;
      if (NULL == getenv("FAKETIME_FOLLOW_FILE"))
      {
        fprintf(stderr, "libfaketime: %% operator in FAKETIME setting requires environment variable FAKETIME_FOLLOW_FILE set.\n");
        exit(1);
      }
      else
      {
        DONT_FAKE_TIME(ret = stat(getenv("FAKETIME_FOLLOW_FILE"), &master_file_stats));
        if (ret == -1)
        {
          fprintf(stderr, "libfaketime: Cannot get timestamp of file %s as requested by %% operator.\n", getenv("FAKETIME_FOLLOW_FILE"));
          exit(1);
        }
        else
        {
          user_faked_time_timespec.tv_sec = master_file_stats.st_mtime;
          user_faked_time_timespec.tv_nsec = 0;
        }
      }
      if (NULL == getenv("FAKETIME_DONT_RESET"))
        system_time_from_system(&ftpl_starttime);
      goto parse_modifiers;
      break;

    case 'i':
    case 'x': /* Only modifiers are passed, don't fall back to strptime */
parse_modifiers:
      /* Speed-up / slow-down contributed by Karl Chen in v0.8 */
      if (strchr(user_faked_time, 'x') != NULL)
      {
        user_rate = atof(strchr(user_faked_time, 'x')+1);
        user_rate_set = true;
        if (NULL != getenv("FAKETIME_XRESET")) {
          if (ftpl_timecache.real.tv_nsec >= 0) {
            user_faked_time_timespec.tv_sec  = ftpl_faketimecache.real.tv_sec;
            user_faked_time_timespec.tv_nsec = ftpl_faketimecache.real.tv_nsec;
            ftpl_starttime.real.tv_sec       = ftpl_timecache.real.tv_sec;
            ftpl_starttime.real.tv_nsec      = ftpl_timecache.real.tv_nsec;
            ftpl_starttime.mon.tv_sec        = ftpl_timecache.mon.tv_sec;
            ftpl_starttime.mon.tv_nsec       = ftpl_timecache.mon.tv_nsec;
            ftpl_starttime.mon_raw.tv_sec    = ftpl_timecache.mon_raw.tv_sec;
            ftpl_starttime.mon_raw.tv_nsec   = ftpl_timecache.mon_raw.tv_nsec;
#ifdef CLOCK_BOOTTIME
            ftpl_starttime.boot.tv_sec       = ftpl_timecache.boot.tv_sec;
            ftpl_starttime.boot.tv_nsec      = ftpl_timecache.boot.tv_nsec;
#endif
          }
        }
      }
      else if (NULL != (tmp_time_fmt = strchr(user_faked_time, 'i')))
      {
        double tick_inc = atof(tmp_time_fmt + 1);
        /* increment time with every time() call*/
        user_per_tick_inc.tv_sec = floor(tick_inc);
        user_per_tick_inc.tv_nsec = (tick_inc - user_per_tick_inc.tv_sec) * SEC_TO_nSEC ;
        user_per_tick_inc_set = true;
      }
      break;
  } // end of switch

  strncpy(user_faked_time_saved, user_faked_time, BUFFERLEN-1);
  user_faked_time_saved[BUFFERLEN-1] = 0;
#ifdef DEBUG
  fprintf(stderr, "new FAKETIME: %s\n", user_faked_time_saved);
#endif
}


/*
 *      =======================================================================
 *      Initialization                                                 === INIT
 *      =======================================================================
 */

static void ftpl_init(void)
{
  char *tmp_env;
  bool dont_fake_final;

  /* moved up here from below the dlsym calls #130 */
  dont_fake = true; // Do not fake times during initialization
  dont_fake_final = false;

#ifdef __APPLE__
  const char *progname = getprogname();
#else
  const char *progname = __progname;
#endif

  /* Look up all real_* functions. NULL will mark missing ones. */
  real_stat =               dlsym(RTLD_NEXT, "__xstat");
  real_fstat =              dlsym(RTLD_NEXT, "__fxstat");
  real_fstatat =            dlsym(RTLD_NEXT, "__fxstatat");
  real_lstat =              dlsym(RTLD_NEXT, "__lxstat");
  real_stat64 =             dlsym(RTLD_NEXT,"__xstat64");
  real_fstat64 =            dlsym(RTLD_NEXT, "__fxstat64");
  real_fstatat64 =          dlsym(RTLD_NEXT, "__fxstatat64");
  real_lstat64 =            dlsym(RTLD_NEXT, "__lxstat64");
  real_time =               dlsym(RTLD_NEXT, "time");
  real_ftime =              dlsym(RTLD_NEXT, "ftime");
#if defined(__alpha__) && defined(__GLIBC__)
  real_gettimeofday =       dlvsym(RTLD_NEXT, "gettimeofday", "GLIBC_2.1");
#else
  real_gettimeofday =       dlsym(RTLD_NEXT, "gettimeofday");
#endif
#ifdef FAKE_SLEEP
  real_nanosleep =          dlsym(RTLD_NEXT, "nanosleep");
#ifndef __APPLE__
  real_clock_nanosleep =    dlsym(RTLD_NEXT, "clock_nanosleep");
#endif
  real_usleep =             dlsym(RTLD_NEXT, "usleep");
  real_sleep =              dlsym(RTLD_NEXT, "sleep");
  real_alarm =              dlsym(RTLD_NEXT, "alarm");
  real_poll =               dlsym(RTLD_NEXT, "poll");
  real_ppoll =              dlsym(RTLD_NEXT, "ppoll");
#ifdef linux
  real_epoll_wait =         dlsym(RTLD_NEXT, "epoll_wait");
  real_epoll_pwait =        dlsym(RTLD_NEXT, "epoll_pwait");
#endif
  real_select =             dlsym(RTLD_NEXT, "select");
#ifdef __linux__
  real_pselect =            dlsym(RTLD_NEXT, "pselect");
#endif
  real_sem_timedwait =      dlsym(RTLD_NEXT, "sem_timedwait");
#endif
#ifdef FAKE_INTERNAL_CALLS
  real___ftime =              dlsym(RTLD_NEXT, "__ftime");
#  if defined(__alpha__) && defined(__GLIBC__)
  real___gettimeofday =       dlvsym(RTLD_NEXT, "__gettimeofday", "GLIBC_2.1");
#  else
  real___gettimeofday =       dlsym(RTLD_NEXT, "__gettimeofday");
#  endif
  real___clock_gettime  =     dlsym(RTLD_NEXT, "__clock_gettime");
#endif
#ifdef FAKE_PTHREAD

#ifdef __GLIBC__
  real_pthread_cond_timedwait_225 = dlvsym(RTLD_NEXT, "pthread_cond_timedwait", "GLIBC_2.2.5");

  real_pthread_cond_timedwait_232 = dlvsym(RTLD_NEXT, "pthread_cond_timedwait", "GLIBC_2.3.2");
  real_pthread_cond_init_232 = dlvsym(RTLD_NEXT, "pthread_cond_init", "GLIBC_2.3.2");
  real_pthread_cond_destroy_232 = dlvsym(RTLD_NEXT, "pthread_cond_destroy", "GLIBC_2.3.2");
#endif

  if (NULL == real_pthread_cond_timedwait_232)
  {
    real_pthread_cond_timedwait_232 =  dlsym(RTLD_NEXT, "pthread_cond_timedwait");
  }
  if (NULL == real_pthread_cond_init_232)
  {
    real_pthread_cond_init_232 =  dlsym(RTLD_NEXT, "pthread_cond_init");
  }
  if (NULL == real_pthread_cond_destroy_232)
  {
    real_pthread_cond_destroy_232 =  dlsym(RTLD_NEXT, "pthread_cond_destroy");
  }

  if (pthread_rwlock_init(&monotonic_conds_lock,NULL) != 0) {
    fprintf(stderr,"monotonic_conds_lock init failed\n");
    exit(-1);
  }
#endif
#ifdef __APPLEOSX__
  real_clock_get_time =     dlsym(RTLD_NEXT, "clock_get_time");
  real_clock_gettime  =     apple_clock_gettime;
#else
  real_clock_gettime  =     dlsym(RTLD_NEXT, "__clock_gettime");
  if (NULL == real_clock_gettime)
  {
    real_clock_gettime  =   dlsym(RTLD_NEXT, "clock_gettime");
  }
#ifdef FAKE_TIMERS
#if defined(__sun)
    real_timer_gettime_233 =  dlsym(RTLD_NEXT, "timer_gettime");
    real_timer_settime_233 =  dlsym(RTLD_NEXT, "timer_settime");
#else
#ifdef __GLIBC__
  real_timer_settime_22 =   dlvsym(RTLD_NEXT, "timer_settime","GLIBC_2.2");
  real_timer_settime_233 =  dlvsym(RTLD_NEXT, "timer_settime","GLIBC_2.3.3");
#endif
  if (NULL == real_timer_settime_233)
  {
    real_timer_settime_233 =  dlsym(RTLD_NEXT, "timer_settime");
  }
#ifdef __GLIBC__
  real_timer_gettime_22 =   dlvsym(RTLD_NEXT, "timer_gettime","GLIBC_2.2");
  real_timer_gettime_233 =  dlvsym(RTLD_NEXT, "timer_gettime","GLIBC_2.3.3");
#endif
  if (NULL == real_timer_gettime_233)
  {
    real_timer_gettime_233 =  dlsym(RTLD_NEXT, "timer_gettime");
  }
#endif
#endif
#endif

  initialized = 1;

  ft_shm_init();
#ifdef FAKE_STAT
  if (getenv("NO_FAKE_STAT")!=NULL)
  {
    fake_stat_disabled = 1;  //Note that this is NOT re-checked
  }
#endif

  if ((tmp_env = getenv("FAKETIME_CACHE_DURATION")) != NULL)
  {
    cache_duration = atoi(tmp_env);
  }
  if ((tmp_env = getenv("FAKETIME_NO_CACHE")) != NULL)
  {
    if (0 == strcmp(tmp_env, "1"))
    {
      cache_enabled = 0;
    }
  }
  if ((tmp_env = getenv("FAKETIME_DONT_FAKE_MONOTONIC")) != NULL
    || (tmp_env = getenv("DONT_FAKE_MONOTONIC")) != NULL)
  {
    if (0 == strcmp(tmp_env, "1"))
    {
      fake_monotonic_clock = 0;
    }
  }
  /* Check whether we actually should be faking the returned timestamp. */

  /* We can prevent faking time for specified commands */
  if ((tmp_env = getenv("FAKETIME_SKIP_CMDS")) != NULL)
  {
    char *skip_cmd, *saveptr, *tmpvar;
    /* Don't mess with the env variable directly. */
    tmpvar = strdup(tmp_env);
    if (tmpvar != NULL)
    {
      skip_cmd = strtok_r(tmpvar, ",", &saveptr);
      while (skip_cmd != NULL)
      {
        if (0 == strcmp(progname, skip_cmd))
        {
          ft_mode = FT_NOOP;
          dont_fake_final = true;
          break;
        }
        skip_cmd = strtok_r(NULL, ",", &saveptr);
      }
      free(tmpvar);
      tmpvar = NULL;
    }
    else
    {
      fprintf(stderr, "Error: Could not copy the environment variable value.\n");
      exit(EXIT_FAILURE);
    }
  }

  /* We can limit faking time to specified commands */
  if ((tmp_env = getenv("FAKETIME_ONLY_CMDS")) != NULL)
  {
    char *only_cmd, *saveptr, *tmpvar;
    bool cmd_matched = false;

    if (getenv("FAKETIME_SKIP_CMDS") != NULL)
    {
      fprintf(stderr, "Error: Both FAKETIME_SKIP_CMDS and FAKETIME_ONLY_CMDS can't be set.\n");
      exit(EXIT_FAILURE);
    }

    /* Don't mess with the env variable directly. */
    tmpvar = strdup(tmp_env);
    if (tmpvar != NULL) {
      only_cmd = strtok_r(tmpvar, ",", &saveptr);
      while (only_cmd != NULL)
      {
        if (0 == strcmp(progname, only_cmd))
        {
          cmd_matched = true;
          break;
        }
        only_cmd = strtok_r(NULL, ",", &saveptr);
      }

      if (!cmd_matched)
      {
        ft_mode = FT_NOOP;
        dont_fake_final = true;
      }
      free(tmpvar);
    } else {
      fprintf(stderr, "Error: Could not copy the environment variable value.\n");
      exit(EXIT_FAILURE);
    }
  }

  if ((tmp_env = getenv("FAKETIME_START_AFTER_SECONDS")) != NULL)
  {
    ft_start_after_secs = atol(tmp_env);
    limited_faking = true;
  }
  if ((tmp_env = getenv("FAKETIME_STOP_AFTER_SECONDS")) != NULL)
  {
    ft_stop_after_secs = atol(tmp_env);
    limited_faking = true;
  }
  if ((tmp_env = getenv("FAKETIME_START_AFTER_NUMCALLS")) != NULL)
  {
    ft_start_after_ncalls = atol(tmp_env);
    limited_faking = true;
  }
  if ((tmp_env = getenv("FAKETIME_STOP_AFTER_NUMCALLS")) != NULL)
  {
    ft_stop_after_ncalls = atol(tmp_env);
    limited_faking = true;
  }

  /* check whether we should spawn an external command */
  if ((tmp_env = getenv("FAKETIME_SPAWN_TARGET")) != NULL)
  {
    spawnsupport = true;
    (void) strncpy(ft_spawn_target, getenv("FAKETIME_SPAWN_TARGET"), sizeof(ft_spawn_target) - 1);
    ft_spawn_target[sizeof(ft_spawn_target) - 1] = 0;
    if ((tmp_env = getenv("FAKETIME_SPAWN_SECONDS")) != NULL)
    {
      ft_spawn_secs = atol(tmp_env);
    }
    if ((tmp_env = getenv("FAKETIME_SPAWN_NUMCALLS")) != NULL)
    {
      ft_spawn_ncalls = atol(tmp_env);
    }
  }

  if ((tmp_env = getenv("FAKETIME_SAVE_FILE")) != NULL)
  {
    if (-1 == (outfile = open(tmp_env, O_RDWR | O_APPEND | O_CLOEXEC | O_CREAT,
                              S_IWUSR | S_IRUSR)))
    {
      perror("libfaketime: In ftpl_init(), opening file for saving timestamps failed");
      exit(EXIT_FAILURE);
    }
  }

  /* load file only if reading timstamps from it is not finished yet */
  if ((tmp_env = getenv("FAKETIME_LOAD_FILE")) != NULL)
  {
    int infile = -1;
    struct stat sb;
    if (-1 == (infile = open(tmp_env, O_RDONLY|O_CLOEXEC)))
    {
      perror("libfaketime: In ftpl_init(), opening file for loading timestamps failed");
      exit(EXIT_FAILURE);
    }

    fstat(infile, &sb);
    if (sizeof(stss[0]) > (infile_size = sb.st_size))
    {
      printf("There are no timestamps in the provided file to load timestamps from");
      exit(EXIT_FAILURE);
    }

    if ((infile_size % sizeof(stss[0])) != 0)
    {
      printf("File size is not multiple of timestamp size. It is probably damaged.");
      exit(EXIT_FAILURE);
    }

    stss = mmap(NULL, infile_size, PROT_READ, MAP_SHARED, infile, 0);
    if (stss == MAP_FAILED)
    {
      perror("libfaketime: In ftpl_init(), mapping file for loading timestamps failed");
      exit(EXIT_FAILURE);
    }
    infile_set = true;
  }

  tmp_env = getenv("FAKETIME_FMT");
  if (tmp_env == NULL)
  {
    strcpy(user_faked_time_fmt, "%Y-%m-%d %T");
  }
  else
  {
    strncpy(user_faked_time_fmt, tmp_env, BUFSIZ - 1);
    user_faked_time_fmt[BUFSIZ - 1] = 0;
  }

  if (shared_sem != 0)
  {
    if (sem_wait(shared_sem) == -1)
    {
      perror("libfaketime: In ftpl_init(), sem_wait failed");
      exit(1);
    }
    if (ft_shared->start_time.real.tv_nsec == -1)
    {
      /* set up global start time */
      system_time_from_system(&ftpl_starttime);
      ft_shared->start_time = ftpl_starttime;
    }
    else
    {
      /** get preset start time */
      ftpl_starttime = ft_shared->start_time;
    }
    if (sem_post(shared_sem) == -1)
    {
      perror("libfaketime: In ftpl_init(), sem_post failed");
      exit(1);
    }
  }
  else
  {
    system_time_from_system(&ftpl_starttime);
  }
  /* fake time supplied as environment variable? */
  if (NULL != (tmp_env = getenv("FAKETIME")))
  {
    parse_config_file = false;
    parse_ft_string(tmp_env);
  }

  dont_fake = dont_fake_final;
}


/*
 *      =======================================================================
 *      Helper functions                                             === HELPER
 *      =======================================================================
 */

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
  {
    *--endp = '\0';
  }
}


/*
 *      =======================================================================
 *      Implementation of faked functions                        === FAKE(FAKE)
 *      =======================================================================
 */

#ifdef PTHREAD_SINGLETHREADED_TIME
static void pthread_cleanup_mutex_lock(void *data)
{
  pthread_mutex_t *mutex = data;
  pthread_mutex_unlock(mutex);
}
#endif

int fake_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
  /* variables used for caching, introduced in version 0.6 */
  static time_t last_data_fetch = 0;  /* not fetched previously at first call */
  static int cache_expired = 1;       /* considered expired at first call */

  /* create a copy of the timespec containing the real system time for clk_id */
  struct timespec tp_save;
  tp_save.tv_sec = tp->tv_sec;
  tp_save.tv_nsec = tp->tv_nsec;

  if (dont_fake) return 0;
  /* Per process timers are only sped up or slowed down */
  if ((clk_id == CLOCK_PROCESS_CPUTIME_ID ) || (clk_id == CLOCK_THREAD_CPUTIME_ID))
  {
    if (user_rate_set)
    {
      timespecmul(tp, user_rate, tp);
    }
    return 0;
  }

  /* Sanity check by Karl Chan since v0.8 */
  if (tp == NULL) return -1;

#ifdef PTHREAD_SINGLETHREADED_TIME
  static pthread_mutex_t time_mutex=PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&time_mutex);
  pthread_cleanup_push(pthread_cleanup_mutex_lock, &time_mutex);
#endif

  if ((limited_faking &&
     ((ft_start_after_ncalls != -1) || (ft_stop_after_ncalls != -1))) ||
     (spawnsupport && ft_spawn_ncalls))
  {
    if (callcounter < LONG_MAX) callcounter++;
  }

  if (limited_faking || spawnsupport)
  {
    struct timespec tmp_ts;
    /* For debugging, output #seconds and #calls */
    switch (clk_id)
    {
      case CLOCK_REALTIME:
#ifdef CLOCK_REALTIME_COARSE
      case CLOCK_REALTIME_COARSE:
#endif
        timespecsub(tp, &ftpl_starttime.real, &tmp_ts);
        break;
      case CLOCK_MONOTONIC:
#ifdef CLOCK_MONOTONIC_COARSE
      case CLOCK_MONOTONIC_COARSE:
#endif
        timespecsub(tp, &ftpl_starttime.mon, &tmp_ts);
        break;
      case CLOCK_MONOTONIC_RAW:
        timespecsub(tp, &ftpl_starttime.mon_raw, &tmp_ts);
        break;
#ifdef CLOCK_BOOTTIME
      case CLOCK_BOOTTIME:
        timespecsub(tp, &ftpl_starttime.boot, &tmp_ts);
        break;
#endif
      default:
        timespecsub(tp, &ftpl_starttime.real, &tmp_ts);
        break;
    }

    if (limited_faking)
    {
      /* Check whether we actually should be faking the returned timestamp. */
      /* fprintf(stderr, "(libfaketime limits -> runtime: %lu, callcounter: %lu\n", (*time_tptr - ftpl_starttime), callcounter); */
      if ((ft_start_after_secs != -1)   && (tmp_ts.tv_sec < ft_start_after_secs)) return 0;
      if ((ft_stop_after_secs != -1)    && (tmp_ts.tv_sec >= ft_stop_after_secs)) return 0;
      if ((ft_start_after_ncalls != -1) && (callcounter < ft_start_after_ncalls)) return 0;
      if ((ft_stop_after_ncalls != -1)  && (callcounter >= ft_stop_after_ncalls)) return 0;
      /* fprintf(stderr, "(libfaketime limits -> runtime: %lu, callcounter: %lu continues\n", (*time_tptr - ftpl_starttime), callcounter); */
    }

    if (spawnsupport)
    {
      /* check whether we should spawn an external command */
      if (spawned == 0)
      { /* exec external command once only */
        if (((tmp_ts.tv_sec == ft_spawn_secs) || (callcounter == ft_spawn_ncalls)) && (spawned == 0))
        {
          spawned = 1;
          (void) (system(ft_spawn_target) + 1);
        }
      }
    }
  }

  struct timespec current_ts;
  DONT_FAKE_TIME((*real_clock_gettime)(CLOCK_REALTIME, &current_ts));

  if (last_data_fetch > 0)
  {
    if ((current_ts.tv_sec - last_data_fetch) > cache_duration)
    {
      cache_expired = 1;
    }
    else
    {
      cache_expired = 0;
    }
  }

  if (cache_enabled == 0)
  {
    cache_expired = 1;
  }

  if (force_cache_expiration != 0)
  {
    cache_expired = 1;
    force_cache_expiration = 0;
  }

  if (cache_expired == 1)
  {
    static char user_faked_time[BUFFERLEN]; /* changed to static for caching in v0.6 */
    /* initialize with default or env. variable */
    char *tmp_env;

    /* Can be enabled for testing ...
      fprintf(stderr, "***************++ Cache expired ++**************\n");
    */

    if (NULL != (tmp_env = getenv("FAKETIME")))
    {
      strncpy(user_faked_time, tmp_env, BUFFERLEN - 1);
      user_faked_time[BUFFERLEN - 1] = 0;
    }
    else
    {
      snprintf(user_faked_time, BUFFERLEN, "+0");
    }

    last_data_fetch = current_ts.tv_sec;
    /* fake time supplied as environment variable? */
    if (parse_config_file)
    {
      char custom_filename[BUFSIZ];
      char filename[BUFSIZ];
      FILE *faketimerc;
      /* check whether there's a .faketimerc in the user's home directory, or
       * a system-wide /etc/faketimerc present.
       * The /etc/faketimerc handling has been contributed by David Burley,
       * Jacob Moorman, and Wayne Davison of SourceForge, Inc. in version 0.6 */
      (void) snprintf(custom_filename, BUFSIZ, "%s", getenv("FAKETIME_TIMESTAMP_FILE"));
      (void) snprintf(filename, BUFSIZ, "%s/.faketimerc", getenv("HOME"));
      if ((faketimerc = fopen(custom_filename, "rt")) != NULL ||
          (faketimerc = fopen(filename, "rt")) != NULL ||
          (faketimerc = fopen("/etc/faketimerc", "rt")) != NULL)
      {
        char line[BUFFERLEN];
        while(fgets(line, BUFFERLEN, faketimerc) != NULL)
        {
          if ((strlen(line) > 1) && (line[0] != ' ') &&
              (line[0] != '#') && (line[0] != ';'))
          {
            remove_trailing_eols(line);
            strncpy(user_faked_time, line, BUFFERLEN-1);
            user_faked_time[BUFFERLEN-1] = 0;
            break;
          }
        }
        fclose(faketimerc);
      }
    } /* read fake time from file */
    parse_ft_string(user_faked_time);
  } /* cache had expired */

  if (infile_set)
  {
    if (load_time(tp))
    {
      return 0;
    }
  }

  /* check whether the user gave us an absolute time to fake */
  switch (ft_mode)
  {
    case FT_FREEZE:  /* a specified time */
      if (user_faked_time_set)
      {
        *tp = user_faked_time_timespec;
      }
      break;

    case FT_START_AT: /* User-specified offset */
      if (user_per_tick_inc_set)
      {
        /* increment time with every time() call*/
        next_time(tp, &user_per_tick_inc);
      }
      else
      {
        /* Speed-up / slow-down contributed by Karl Chen in v0.8 */
        struct timespec tdiff, timeadj;
        switch (clk_id)
        {
          case CLOCK_REALTIME:
#ifdef CLOCK_REALTIME_COARSE
          case CLOCK_REALTIME_COARSE:
#endif
            timespecsub(tp, &ftpl_starttime.real, &tdiff);
            break;
          case CLOCK_MONOTONIC:
#ifdef CLOCK_MONOTONIC_COARSE
          case CLOCK_MONOTONIC_COARSE:
#endif
            timespecsub(tp, &ftpl_starttime.mon, &tdiff);
            break;
          case CLOCK_MONOTONIC_RAW:
            timespecsub(tp, &ftpl_starttime.mon_raw, &tdiff);
            break;
#ifdef CLOCK_BOOTTIME
          case CLOCK_BOOTTIME:
            timespecsub(tp, &ftpl_starttime.boot, &tdiff);
            break;
#endif
          default:
            timespecsub(tp, &ftpl_starttime.real, &tdiff);
            break;
        } // end of switch (clk_id)
        if (user_rate_set)
        {
          timespecmul(&tdiff, user_rate, &timeadj);
        }
        else
        {
          timeadj = tdiff;
        }
        timespecadd(&user_faked_time_timespec, &timeadj, tp);
      }
      break;

    default:
      return -1;
  } // end of switch(ft_mode)

#ifdef PTHREAD_SINGLETHREADED_TIME
  pthread_cleanup_pop(1);
#endif
  save_time(tp);

  /* Cache this most recent real and faked time we encountered */
  if (clk_id == CLOCK_REALTIME)
  {
    ftpl_timecache.real.tv_sec         = tp_save.tv_sec;
    ftpl_timecache.real.tv_nsec        = tp_save.tv_nsec;
    ftpl_faketimecache.real.tv_sec     = tp->tv_sec;
    ftpl_faketimecache.real.tv_nsec    = tp->tv_nsec;
  }
  else if (clk_id == CLOCK_MONOTONIC)
  {
    ftpl_timecache.mon.tv_sec          = tp_save.tv_sec;
    ftpl_timecache.mon.tv_nsec         = tp_save.tv_nsec;
    ftpl_faketimecache.mon.tv_sec      = tp->tv_sec;
    ftpl_faketimecache.mon.tv_nsec     = tp->tv_nsec;
  }
  else if (clk_id == CLOCK_MONOTONIC_RAW)
  {
    ftpl_timecache.mon_raw.tv_sec      = tp_save.tv_sec;
    ftpl_timecache.mon_raw.tv_nsec     = tp_save.tv_nsec;
    ftpl_faketimecache.mon_raw.tv_sec  = tp->tv_sec;
    ftpl_faketimecache.mon_raw.tv_nsec = tp->tv_nsec;
  }
#ifdef CLOCK_BOOTTIME
  else if (clk_id == CLOCK_BOOTTIME)
  {
    ftpl_timecache.boot.tv_sec         = tp_save.tv_sec;
    ftpl_timecache.boot.tv_nsec        = tp_save.tv_nsec;
    ftpl_faketimecache.boot.tv_sec     = tp->tv_sec;
    ftpl_faketimecache.boot.tv_nsec    = tp->tv_nsec;
  }
#endif

  return 0;
}

int fake_gettimeofday(struct timeval *tv)
{
  struct timespec ts;
  int ret;
  ts.tv_sec = tv->tv_sec;
  ts.tv_nsec = tv->tv_usec * 1000  + ftpl_starttime.real.tv_nsec % 1000;

  ret = fake_clock_gettime(CLOCK_REALTIME, &ts);
  tv->tv_sec = ts.tv_sec;
  tv->tv_usec =ts.tv_nsec / 1000;

  return ret;
}


/*
 *      =======================================================================
 *      Faked system functions: Apple Mac OS X specific           === FAKE(OSX)
 *      =======================================================================
 */

#ifdef __APPLEOSX__
/*
 * clock_gettime implementation for __APPLE__
 * @note It always behave like being called with CLOCK_REALTIME.
 */
static int apple_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
  int result;
  mach_timespec_t cur_timeclockid_t;
  (void) clk_id; /* unused */

  if (NULL == real_clock_get_time)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original clock_get_time() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  DONT_FAKE_TIME(result = (*real_clock_get_time)(clock_serv_real, &cur_timeclockid_t));
  tp->tv_sec =  cur_timeclockid_t.tv_sec;
  tp->tv_nsec = cur_timeclockid_t.tv_nsec;
  return result;
}

int clock_get_time(clock_serv_t clock_serv, mach_timespec_t *cur_timeclockid_t)
{
  int result;
  struct timespec ts;

  /*
   * Initialize our result with the real current time from CALENDAR_CLOCK.
   * This is a bit of cheating, but we don't keep track of obtained clock
   * services.
   */
  DONT_FAKE_TIME(result = (*real_clock_gettime)(CLOCK_REALTIME, &ts));
  if (result == -1) return result; /* original function failed */

  /* pass the real current time to our faking version, overwriting it */
  result = fake_clock_gettime(CLOCK_REALTIME, &ts);
  cur_timeclockid_t->tv_sec = ts.tv_sec;
  cur_timeclockid_t->tv_nsec = ts.tv_nsec;

  /* return the result to the caller */
  return result;
}
#endif


/*
 *      =======================================================================
 *      Faked system-internal functions                           === FAKE(INT)
 *      =======================================================================
 */

#ifdef FAKE_INTERNAL_CALLS
int __gettimeofday(struct timeval *tv, void *tz)
{
  int result;

  /* sanity check */
  if (tv == NULL)
  {
    return -1;
  }

  /* Check whether we've got a pointer to the real ftime() function yet */
  if (NULL == real___gettimeofday)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original __gettimeofday() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  /* initialize our result with the real current time */
  DONT_FAKE_TIME(result = (*real___gettimeofday)(tv, tz));
  if (result == -1) return result; /* original function failed */

  /* pass the real current time to our faking version, overwriting it */
  result = fake_gettimeofday(tv);

  /* return the result to the caller */
  return result;
}

int __clock_gettime(clockid_t clk_id, struct timespec *tp)
{
  int result;

  /* sanity check */
  if (tp == NULL)
  {
    return -1;
  }

  if (NULL == real___clock_gettime)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original __clock_gettime() not found.\n");
#endif
    return -1; /* propagate error to caller */
  }

  /* initialize our result with the real current time */
  DONT_FAKE_TIME(result = (*real___clock_gettime)(clk_id, tp));
  if (result == -1) return result; /* original function failed */

  /* pass the real current time to our faking version, overwriting it */
  if (fake_monotonic_clock || (clk_id != CLOCK_MONOTONIC && clk_id != CLOCK_MONOTONIC_RAW
#ifdef CLOCK_MONOTONIC_COARSE
      && clk_id != CLOCK_MONOTONIC_COARSE
#endif
#ifdef CLOCK_BOOTTIME
      && clk_id != CLOCK_BOOTTIME
#endif
      ))

  {
    result = fake_clock_gettime(clk_id, tp);
  }

  /* return the result to the caller */
  return result;
}

time_t __time(time_t *time_tptr)
{
  struct timespec tp;
  time_t result;

  DONT_FAKE_TIME(result = (*real_clock_gettime)(CLOCK_REALTIME, &tp));
  if (result == -1) return -1;

  /* pass the real current time to our faking version, overwriting it */
  (void)fake_clock_gettime(CLOCK_REALTIME, &tp);

  if (time_tptr != NULL)
  {
    *time_tptr = tp.tv_sec;
  }
  return tp.tv_sec;
}

int __ftime(struct timeb *tb)
{
  struct timespec tp;
  int result;

  /* sanity check */
  if (tb == NULL)
    return 0;               /* ftime() always returns 0, see manpage */

  /* Check whether we've got a pointer to the real ftime() function yet */
  if (NULL == real___ftime)
  {  /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original ftime() not found.\n");
#endif
    return 0; /* propagate error to caller */
  }

  /* initialize our TZ result with the real current time */
  DONT_FAKE_TIME(result = (*real___ftime)(tb));
  if (result == -1)
  {
    return result;
  }

  DONT_FAKE_TIME(result = (*real_clock_gettime)(CLOCK_REALTIME, &tp));
  if (result == -1) return -1;

  /* pass the real current time to our faking version, overwriting it */
  (void)fake_clock_gettime(CLOCK_REALTIME, &tp);

  tb->time = tp.tv_sec;
  tb->millitm = tp.tv_nsec / 1000000;

  /* return the result to the caller */
  return result; /* will always be 0 (see manpage) */
}

#endif

/*
 *      =======================================================================
 *      Faked pthread_cond_timedwait                          === FAKE(pthread)
 *      =======================================================================
 */

/* pthread_cond_timedwait

   The specified absolute time in pthread_cond_timedwait is directly
   passed to the kernel via the futex syscall. The kernel, however,
   does not know about the fake time. In 99.9% of cases, the time
   until this function should wait is calculated by an application
   relatively to the current time, which has been faked in the
   application. Hence, we should convert the waiting time back to real
   time.

   pthread_cond_timedwait in GLIBC_2_2_5 only supports
   CLOCK_REALTIME.  Since the init and destroy functions are not
   redefined for GLIBC_2_2_5, a corresponding cond will never be
   added to monotonic_conds and hence the correct branch will
   always be taken.
*/


#ifdef FAKE_PTHREAD

typedef enum {FT_COMPAT_GLIBC_2_2_5, FT_COMPAT_GLIBC_2_3_2} ft_lib_compat_pthread;

struct pthread_cond_monotonic {
    pthread_cond_t *ptr;
    UT_hash_handle hh;
};

static struct pthread_cond_monotonic *monotonic_conds = NULL;

int pthread_cond_init_232(pthread_cond_t *restrict cond, const pthread_condattr_t *restrict attr)
{
  clockid_t clock_id;
  int result;

  result = real_pthread_cond_init_232(cond, attr);

  if (result != 0 || attr == NULL)
    return result;

  pthread_condattr_getclock(attr, &clock_id);

  if (clock_id == CLOCK_MONOTONIC) {
    struct pthread_cond_monotonic *e = (struct pthread_cond_monotonic*)malloc(sizeof(struct pthread_cond_monotonic));
    e->ptr = cond;

    if (pthread_rwlock_wrlock(&monotonic_conds_lock) != 0) {
      fprintf(stderr,"can't acquire write monotonic_conds_lock\n");
      exit(-1);
    }
    HASH_ADD_PTR(monotonic_conds, ptr, e);
    pthread_rwlock_unlock(&monotonic_conds_lock);
  }

  return result;
}

int pthread_cond_destroy_232(pthread_cond_t *cond)
{
  struct pthread_cond_monotonic* e;

  if (pthread_rwlock_wrlock(&monotonic_conds_lock) != 0) {
    fprintf(stderr,"can't acquire write monotonic_conds_lock\n");
    exit(-1);
  }
  HASH_FIND_PTR(monotonic_conds, &cond, e);
  if (e) {
    HASH_DEL(monotonic_conds, e);
    free(e);
  }
  pthread_rwlock_unlock(&monotonic_conds_lock);

  return real_pthread_cond_destroy_232(cond);
}

//where init in pthread methods????

int pthread_cond_timedwait_common(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime, ft_lib_compat_pthread compat)
{
  struct timespec tp, tdiff_actual, realtime, faketime;
  struct timespec *tf = NULL;
  struct pthread_cond_monotonic* e;
  char *tmp_env;
  int wait_ms;
  clockid_t clk_id;
  int result = 0;

  if (abstime != NULL)
  {
    if (pthread_rwlock_rdlock(&monotonic_conds_lock) != 0) {
      fprintf(stderr,"can't acquire read monotonic_conds_lock\n");
      exit(-1);
    }
    HASH_FIND_PTR(monotonic_conds, &cond, e);
    pthread_rwlock_unlock(&monotonic_conds_lock);
    if (e != NULL)
      clk_id = CLOCK_MONOTONIC;
    else
      clk_id = CLOCK_REALTIME;

    DONT_FAKE_TIME(result = (*real_clock_gettime)(clk_id, &realtime));
    if (result == -1)
    {
      return EINVAL;
    }
    faketime = realtime;
    (void)fake_clock_gettime(clk_id, &faketime);

    if ((tmp_env = getenv("FAKETIME_WAIT_MS")) != NULL)
    {
      wait_ms = atol(tmp_env);
      DONT_FAKE_TIME(result = (*real_clock_gettime)(clk_id, &realtime));
      if (result == -1)
      {
        return EINVAL;
      }

      tdiff_actual.tv_sec = wait_ms / 1000;
      tdiff_actual.tv_nsec = (wait_ms % 1000) * 1000000;
      timespecadd(&realtime, &tdiff_actual, &tp);

      tf = &tp;
    }
    else
    {
      timespecsub(abstime, &faketime, &tp);
      if (user_rate_set)
      {
        timespecmul(&tp, 1.0 / user_rate, &tdiff_actual);
      }
      else
      {
        tdiff_actual = tp;
      }
    }

    /* For CLOCK_MONOTONIC, pthread_cond_timedwait uses clock_gettime
       internally to calculate the appropriate duration for the
       waiting time. This already uses the faked functions, hence, the
       fake time needs to be passed to pthread_cond_timedwait for
       CLOCK_MONOTONIC. */
#ifndef __ARM_ARCH
#ifndef FORCE_MONOTONIC_FIX
    if(clk_id == CLOCK_MONOTONIC)
      timespecadd(&faketime, &tdiff_actual, &tp);
    else
#endif
#endif
      timespecadd(&realtime, &tdiff_actual, &tp);

    tf = &tp;
  }

  switch (compat) {
  case FT_COMPAT_GLIBC_2_3_2:
    result = real_pthread_cond_timedwait_232(cond, mutex, tf);
    break;
  case FT_COMPAT_GLIBC_2_2_5:
    result = real_pthread_cond_timedwait_225(cond, mutex, tf);
    break;
  }
  return result;
}

int pthread_cond_timedwait_225(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
  return pthread_cond_timedwait_common(cond, mutex, abstime, FT_COMPAT_GLIBC_2_2_5);
}

int pthread_cond_timedwait_232(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime)
{
  return pthread_cond_timedwait_common(cond, mutex, abstime, FT_COMPAT_GLIBC_2_3_2);
}

__asm__(".symver pthread_cond_timedwait_225, pthread_cond_timedwait@GLIBC_2.2.5");
#if defined __ARM_ARCH || defined FORCE_PTHREAD_NONVER
__asm__(".symver pthread_cond_timedwait_232, pthread_cond_timedwait@@");
__asm__(".symver pthread_cond_init_232, pthread_cond_init@@");
__asm__(".symver pthread_cond_destroy_232, pthread_cond_destroy@@");
#else
__asm__(".symver pthread_cond_timedwait_232, pthread_cond_timedwait@@GLIBC_2.3.2");
__asm__(".symver pthread_cond_init_232, pthread_cond_init@@GLIBC_2.3.2");
__asm__(".symver pthread_cond_destroy_232, pthread_cond_destroy@@GLIBC_2.3.2");
#endif

#endif

/*
 *  Intercept calls to time-setting functions if compiled with FAKE_SETTIME set.
 *  Based on suggestion and prototype by @ojura, see https://github.com/wolfcw/libfaketime/issues/179
 */
#ifdef FAKE_SETTIME
int clock_settime(clockid_t clk_id, const struct timespec *tp) {

  /* only CLOCK_REALTIME can be set */
  if (clk_id != CLOCK_REALTIME) {
    errno = EPERM;
    return -1;
  }

  /* sanity check for the pointer */
  if (tp == NULL) {
    errno = EFAULT;
    return -1;
  }

  /* When setting the FAKETIME environment variable to the new timestamp,
     we do not have to care about 'x' or 'i' modifiers given previously,
     as they are not erased when parsing them. */
  struct timespec current_time;
  DONT_FAKE_TIME(clock_gettime(clk_id, &current_time))
   ;

  time_t sec_diff = tp->tv_sec - current_time.tv_sec;
  long nsec_diff = tp->tv_nsec - current_time.tv_nsec;
  char newenv_string[256];
  double offset = (double) sec_diff;
  offset += (double) nsec_diff/SEC_TO_nSEC;
  snprintf(newenv_string, 255, "%+f", offset);

  setenv("FAKETIME", newenv_string, 1);
  force_cache_expiration = 1; /* make sure it becomes effective immediately */

  return 0;
}

int settimeofday(const struct timeval *tv, void *tz)
{
  /* The use of timezone *tz is obsolete and simply ignored here. */
  if (tz == NULL) tz = NULL;

  if (tv == NULL)
  {
    errno = EFAULT;
    return -1;
  }
  else
  {
    struct timespec tp;
    tp.tv_sec = tv->tv_sec;
    tp.tv_nsec = tv->tv_usec * 1000;
    clock_settime(CLOCK_REALTIME, &tp);
  }
  return 0;
}

int adjtime (const struct timeval *delta, struct timeval *olddelta)
{
  /* Always signal true full success when olddelta is requested. */
  if (olddelta != NULL)
  {
    olddelta->tv_sec = 0;
    olddelta->tv_usec = 0;
  }

  if (delta != NULL)
  {
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    tp.tv_sec += delta->tv_sec;
    tp.tv_nsec += delta->tv_usec * 1000;
    /* This actually will make the clock jump instead of gradually
       adjusting it, but we fulfill the caller's intention and an
       additional thread just for the gradual changes does not seem
       to be worth the effort presently. */
    clock_settime(CLOCK_REALTIME, &tp);
  }
  return 0;
}
#endif

/*
 * Editor modelines
 *
 * Local variables:
 * c-basic-offset: 2
 * tab-width: 2
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=2 tabstop=2 expandtab:
 * :indentSize=2:tabSize=2:noTabs=true:
 */

/* eof */
