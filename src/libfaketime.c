/*
 *  This file is part of libfaketime, version 0.9.11
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

#define _LARGEFILE64_SOURCE 1   /* required for stat64 on musl */

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
#ifdef __GLIBC__
#ifndef __APPLE__
#include <gnu/libc-version.h>
#endif
#endif
#include <time.h>
#ifdef MACOS_DYLD_INTERPOSE
#include <pthread.h>
#include <sys/time.h>
#include <utime.h>
#endif
#include <math.h>
#include <errno.h>
#include <string.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <limits.h>
#ifdef INTERCEPT_SYSCALL
#ifdef __linux__
#include <stdarg.h>
#include <sys/syscall.h>
#ifdef INTERCEPT_FUTEX
#include <linux/futex.h>
#endif
#else
#error INTERCEPT_SYSCALL should only be defined on GNU/Linux systems.
#endif
#endif
#ifdef __linux__
#include <sys/timerfd.h>
#endif

#include "uthash.h"

#include "time_ops.h"
#include "faketime_common.h"

#if defined PTHREAD_SINGLETHREADED_TIME && defined FAKE_STATELESS
#undef PTHREAD_SINGLETHREADED_TIME
#endif

/* pthread-handling contributed by David North, TDI in version 0.7 */
#if defined PTHREAD_SINGLETHREADED_TIME || defined FAKE_PTHREAD
#include <pthread.h>
#ifdef __aarch64__
#define _SYS_TIME_H 1
#endif
#include <signal.h>
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

#ifdef MACOS_DYLD_INTERPOSE
void do_macos_dyld_interpose(void);
#define DYLD_INTERPOSE(_new,_target) \
   __attribute__((used)) static struct{ const void* new; const void* target; } _interpose_##_target \
            __attribute__ ((section ("__DATA,__interpose"))) = { (const void*)(unsigned long)&_new, (const void*)(unsigned long)&_target };
#endif

#endif

/* some systems lack raw clock */
#ifndef CLOCK_MONOTONIC_RAW
#define CLOCK_MONOTONIC_RAW (CLOCK_MONOTONIC + 1)
#endif

#if defined FAKE_UTIME && !defined FAKE_FILE_TIMESTAMPS
#define FAKE_FILE_TIMESTAMPS
#endif

#ifdef FAKE_FILE_TIMESTAMPS
#ifndef __APPLE__
struct utimbuf {
  time_t actime;       /* access time */
  time_t modtime;      /* modification time */
};
#endif
#endif

#ifdef FAKE_RANDOM
#include <sys/random.h>
#endif

/* __timespec64 is needed for clock_gettime64 on 32-bit architectures */
struct __timespec64
{
  uint64_t tv_sec;         /* Seconds */
  uint32_t tv_nsec;        /* this is 32-bit, apparently! */
};

/* __timespec64 is needed for clock_gettime64 on 32-bit architectures */
struct __timeval64
{
  uint64_t tv_sec;         /* Seconds */
  uint64_t tv_usec;        /* this is 64-bit, apparently! */
};

/*
 * Per thread variable, which we turn on inside real_* calls to avoid modifying
 * time multiple times of for the whole process to prevent faking time
 */
static __thread bool dont_fake = false;

/* Wrapper for function calls, which we want to return system time */
#define DONT_FAKE_TIME(call)          \
  do {                                \
    bool dont_fake_orig = dont_fake;  \
    if (!dont_fake)                   \
    {                                 \
      dont_fake = true;               \
    }                                 \
    call;                             \
    dont_fake = dont_fake_orig;       \
  } while (0)

/* pointers to real (not faked) functions */
static int          (*real_stat)            (const char *, struct stat *);
static int          (*real_fstat)           (int, struct stat *);
static int          (*real_lstat)           (const char *, struct stat *);
static int          (*real_xstat)           (int, const char *, struct stat *);
static int          (*real_fxstat)          (int, int, struct stat *);
static int          (*real_fxstatat)        (int, int, const char *, struct stat *, int);
static int          (*real_lxstat)          (int, const char *, struct stat *);
static int          (*real_xstat64)         (int, const char *, struct stat64 *);
static int          (*real_fxstat64)        (int, int , struct stat64 *);
static int          (*real_fxstatat64)      (int, int , const char *, struct stat64 *, int);
static int          (*real_lxstat64)        (int, const char *, struct stat64 *);
#ifdef STATX_TYPE
static int          (*real_statx)           (int dirfd, const char *pathname, int flags, unsigned int mask, struct statx *statxbuf);
#endif
static time_t       (*real_time)            (time_t *);
static int          (*real_ftime)           (struct timeb *);
static int          (*real_gettimeofday)    (struct timeval *, void *);
static int          (*real_clock_gettime)   (clockid_t clk_id, struct timespec *tp);
static int          (*real_clock_gettime64) (clockid_t clk_id, struct __timespec64 *tp);
#ifdef TIME_UTC
static int          (*real_timespec_get)    (struct timespec *ts, int base);
#endif
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
static int          (*real_timerfd_settime)    (int fd, int flags,
                                                const struct itimerspec *new_value,
                                                struct itimerspec *old_value);
static int          (*real_timerfd_gettime)    (int fd,
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
static int          (*real_sem_clockwait)   (sem_t *sem, clockid_t clockid, const struct timespec *abstime);
#endif
#ifdef __APPLEOSX__
static int          (*real_clock_get_time)  (clock_serv_t clock_serv, mach_timespec_t *cur_timeclockid_t);
static int          apple_clock_gettime     (clockid_t clk_id, struct timespec *tp);
static clock_serv_t clock_serv_real;
#endif

#ifdef FAKE_FILE_TIMESTAMPS
static int          (*real_utimes)          (const char *filename, const struct timeval times[2]);
static int          (*real_utime)           (const char *filename, const struct utimbuf *times);
static int          (*real_utimensat)       (int dirfd, const char *filename, const struct timespec times[2], int flags);
static int          (*real_futimens)        (int fd, const struct timespec times[2]);
#endif

#ifdef FAKE_RANDOM
static ssize_t     (*real_getrandom)        (void *buf, size_t buflen, unsigned int flags);
static int         (*real_getentropy)       (void *buffer, size_t length);
#endif
#ifdef FAKE_PID
static pid_t       (*real_getpid)        ();
#endif

#ifdef INTERCEPT_SYSCALL
static long        (*real_syscall)        (long, ...);
#endif

static bool check_missing_real(const char *name, bool missing)
{
  if (missing)
  { /* dlsym() failed */
#ifdef DEBUG
    (void) fprintf(stderr, "faketime problem: original %s not found.\n", name);
#else
    (void) name; /* unused */
#endif
    return false;
  }
  return true;
}
#define CHECK_MISSING_REAL(name) \
  check_missing_real(#name, (NULL == real_##name))

static pthread_once_t initialized_once_control = PTHREAD_ONCE_INIT;

/* prototypes */
static int    fake_gettimeofday(struct timeval *tv);
static int    fake_clock_gettime(clockid_t clk_id, struct timespec *tp);
int           read_config_file();
bool          str_array_contains(const char *haystack, const char *needle);
void *ft_dlvsym(void *handle, const char *symbol, const char *version, const char *full_name, char *ignore_list, bool should_debug_dlsym);

/** Semaphore protecting shared data */
static sem_t *shared_sem = NULL;

/** Data shared among faketime-spawned processes */
static struct ft_shared_s *ft_shared = NULL;

/** Storage format for timestamps written to file. Big endian. */
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

/* Fractional user offset provided through FAKETIME env. var. */
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
  pid_t pid;

#ifdef FAKE_PID
  pid = real_getpid();
#else
  pid = getpid();
#endif
  snprintf(sem_name, 255, "/faketime_sem_%ld", (long)pid);
  snprintf(shm_name, 255, "/faketime_shm_%ld", (long)pid);
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

static pthread_once_t ft_shm_initialized_once_control = PTHREAD_ONCE_INIT;

static void ft_shm_really_init (void);
static void ft_shm_init (void)
{
  pthread_once(&ft_shm_initialized_once_control, ft_shm_really_init);
}

static void ft_shm_really_init (void)
{
  int ticks_shm_fd;
  char sem_name[256], shm_name[256], *ft_shared_env = getenv("FAKETIME_SHARED");
  sem_t *shared_semR = NULL;
  static int nt=1;

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
      if (shmCreator)
      {
        perror("libfaketime: In ft_shm_init(), sem_open failed");
        fprintf(stderr, "libfaketime: sem_name was %s, created locally: %s\n", sem_name, shmCreator ? "true":"false");
        fprintf(stderr, "libfaketime: parsed from env: %s\n", ft_shared_env);
        exit(1);
      }
      else
      {
        nt++;
        if (nt > 3)
        {
          perror("libfaketime: In ft_shm_init(), sem_open failed and recreation attempts failed");
          fprintf(stderr, "libfaketime: sem_name was %s, created locally: %s\n", sem_name, shmCreator ? "true":"false");
          exit(1);
        }
        else{
          ft_shm_init();
          return;
        }

      }
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
  if (getenv("FAKETIME_FLSHM") != NULL)
  { /* force the deletion of the shm sync env variable */
    unsetenv("FAKETIME_SHARED");
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
    shared_sem = NULL;
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
 *      Get monotonic faketime setting                               === GETENV
 *      =======================================================================
 */

static void get_fake_monotonic_setting(int* current_value)
{
  char *tmp_env;
  if ((tmp_env = getenv("FAKETIME_DONT_FAKE_MONOTONIC")) != NULL
    || (tmp_env = getenv("DONT_FAKE_MONOTONIC")) != NULL)
  {
    if (0 == strcmp(tmp_env, "1"))
    {
      (*current_value) = 0;
    }
    else
    {
      (*current_value) = 1;
    }
  }
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
  /* from https://stackoverflow.com/questions/5167269/clock-gettime-alternative-in-mac-os-x */
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
      if (errno == EINTR)
      {
        next_time(tp, ticklen);
        return;
      }
      else
      {
        perror("libfaketime: In next_time(), sem_wait failed");
        exit(1);
      }
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

static void reset_time()
{
  system_time_from_system(&ftpl_starttime);
  if (shared_sem != NULL)
  {
    if (sem_wait(shared_sem) == -1)
    {
      perror("libfaketime: In reset_time(), sem_wait failed");
      exit(1);
    }
    ft_shared->start_time = ftpl_starttime;
    if (sem_post(shared_sem) == -1)
    {
      perror("libfaketime: In reset_time(), sem_post failed");
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
      if (errno == EINTR)
      {
        save_time(tp);
        return;
      }
      else
      {
        perror("libfaketime: In save_time(), sem_wait failed");
        exit(1);
      }
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
      if (errno == EINTR)
      {
        return load_time(tp);
      }
      else
      {
        perror("libfaketime: In load_time(), sem_wait failed");
        exit(1);
      }
    }

    if ((sizeof(stss[0]) * (ft_shared->file_idx + 1)) > infile_size)
    {
      /* we are out of timestamps to replay, return to faking time by rules
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
#ifdef FAKE_UTIME
static int fake_utime_disabled = 1;
#endif


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
      if (errno == EINTR)
      {
        lock_for_stat();
        return;
      }
      else
      {
        perror("libfaketime: In lock_for_stat(), sem_wait failed");
        exit(1);
      }
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
#ifndef __APPLE__
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_ctim);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_atim);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_mtim);
#else
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_ctimespec);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_atimespec);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_mtimespec);
#endif
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
#ifndef __APPLE__
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_ctim);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_atim);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_mtim);
#else
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_ctimespec);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_atimespec);
  fake_clock_gettime(CLOCK_REALTIME, &buf->st_mtimespec);
#endif
  unlock_for_stat();
#endif
}

/* macOS dyld interposing uses the function's real name instead of real_name */
#ifdef MACOS_DYLD_INTERPOSE
#define STAT_HANDLER_COMMON(name, buf, fake_statbuf, ...) \
  ftpl_init(); \
  if (!CHECK_MISSING_REAL(name)) return -1; \
  \
  int result; \
  DONT_FAKE_TIME(result = name(__VA_ARGS__)); \
  if (result == -1) \
  { \
    return -1; \
  } \
  \
  if (buf != NULL) \
  { \
    if (!fake_stat_disabled) \
    { \
      if (!dont_fake) fake_statbuf(buf); \
    } \
  } \
  \
  return result;
#else
#define STAT_HANDLER_COMMON(name, buf, fake_statbuf, ...) \
  ftpl_init(); \
  if (!CHECK_MISSING_REAL(name)) return -1; \
  \
  int result; \
  DONT_FAKE_TIME(result = real_##name(__VA_ARGS__)); \
  if (result == -1) \
  { \
    return -1; \
  } \
  \
  if (buf != NULL) \
  { \
    if (!fake_stat_disabled) \
    { \
      if (!dont_fake) fake_statbuf(buf); \
    } \
  } \
  \
  return result;
#endif
#define STAT_HANDLER(name, buf, ...) \
  STAT_HANDLER_COMMON(name, buf, fake_statbuf, __VA_ARGS__)
#define STAT64_HANDLER(name, buf, ...) \
  STAT_HANDLER_COMMON(name, buf, fake_stat64buf, __VA_ARGS__)

#ifdef MACOS_DYLD_INTERPOSE
int macos_stat (const char *path, struct stat *buf)
#else
int stat (const char *path, struct stat *buf)
#endif
{
  STAT_HANDLER(stat, buf, path, buf);
}

#ifdef MACOS_DYLD_INTERPOSE
int macos_fstat (int fildes, struct stat *buf)
#else
int fstat (int fildes, struct stat *buf)
#endif
{
  STAT_HANDLER(fstat, buf, fildes, buf);
}

#ifdef MACOS_DYLD_INTERPOSE
int macos_lstat (const char *path, struct stat *buf)
#else
int lstat (const char *path, struct stat *buf)
#endif
{
  STAT_HANDLER(lstat, buf, path, buf);
}

#ifndef __APPLE__
/* Contributed by Philipp Hachtmann in version 0.6 */
int __xstat (int ver, const char *path, struct stat *buf)
{
  STAT_HANDLER(xstat, buf, ver, path, buf);
}
#endif

#ifndef __APPLE__
/* Contributed by Philipp Hachtmann in version 0.6 */
int __fxstat (int ver, int fildes, struct stat *buf)
{
  STAT_HANDLER(fxstat, buf, ver, fildes, buf);
}
#endif

#ifndef __APPLE__
/* Added in v0.8 as suggested by Daniel Kahn Gillmor */
#ifndef NO_ATFILE
int __fxstatat(int ver, int fildes, const char *filename, struct stat *buf, int flag)
{
  STAT_HANDLER(fxstatat, buf, ver, fildes, filename, buf, flag);
}
#endif
#endif

#ifndef __APPLE__
/* Contributed by Philipp Hachtmann in version 0.6 */
int __lxstat (int ver, const char *path, struct stat *buf)
{
  STAT_HANDLER(lxstat, buf, ver, path, buf);
}
#endif

#ifndef __APPLE__
/* Contributed by Philipp Hachtmann in version 0.6 */
int __xstat64 (int ver, const char *path, struct stat64 *buf)
{
  STAT64_HANDLER(xstat64, buf, ver, path, buf);
}
#endif

#ifndef __APPLE__
/* Contributed by Philipp Hachtmann in version 0.6 */
int __fxstat64 (int ver, int fildes, struct stat64 *buf)
{
  STAT64_HANDLER(fxstat64, buf, ver, fildes, buf);
}
#endif

#ifndef __APPLE__
/* Added in v0.8 as suggested by Daniel Kahn Gillmor */
#ifndef NO_ATFILE
int __fxstatat64 (int ver, int fildes, const char *filename, struct stat64 *buf, int flag)
{
  STAT64_HANDLER(fxstatat64, buf, ver, fildes, filename, buf, flag);
}
#endif
#endif

#ifndef __APPLE__
/* Contributed by Philipp Hachtmann in version 0.6 */
int __lxstat64 (int ver, const char *path, struct stat64 *buf)
{
  STAT64_HANDLER(lxstat64, buf, ver, path, buf);
}
#endif
#endif

#ifdef STATX_TYPE
static inline void fake_statx_timestamp(struct statx_timestamp* p)
{
  struct timespec t = {p->tv_sec,p->tv_nsec};
  fake_clock_gettime(CLOCK_REALTIME, &t);
  p->tv_sec = t.tv_sec;
  p->tv_nsec = t.tv_nsec;
}

static inline void fake_statxbuf (struct statx *buf) {
  lock_for_stat();
  if (buf->stx_mask & STATX_ATIME) {
    fake_statx_timestamp(&buf->stx_atime);
  }
  if (buf->stx_mask & STATX_CTIME) {
    fake_statx_timestamp(&buf->stx_ctime);
  }
  if (buf->stx_mask & STATX_MTIME) {
    fake_statx_timestamp(&buf->stx_mtime);
  }
  if (buf->stx_mask & STATX_BTIME) {
    fake_statx_timestamp(&buf->stx_btime);
  }
  unlock_for_stat();
}

int statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx *statxbuf)
{
  STAT_HANDLER_COMMON(statx, statxbuf, fake_statxbuf, dirfd, pathname, flags, mask, statxbuf)
}
#endif

#ifdef FAKE_FILE_TIMESTAMPS
#ifdef MACOS_DYLD_INTERPOSE
int macos_utime(const char *filename, const struct utimbuf *times)
#else
int utime(const char *filename, const struct utimbuf *times)
#endif
{
  ftpl_init();
  if (!CHECK_MISSING_REAL(utime)) return -1;

  int result;
  struct utimbuf ntbuf;
  if (fake_utime_disabled)
  {
    if (times == NULL)
    { /* The user wants their given fake times left alone but they requested NOW, so turn it into fake NOW */
      ntbuf.actime = ntbuf.modtime = time(NULL);
      times = &ntbuf;
    }
  }
  else if (times != NULL)
  {
    ntbuf.actime = times->actime - user_offset.tv_sec;
    ntbuf.modtime = times->modtime - user_offset.tv_sec;
    times = &ntbuf;
  }
#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(result = utime(filename, times));
#else
  DONT_FAKE_TIME(result = real_utime(filename, times));
#endif
  return result;
}

#ifdef MACOS_DYLD_INTERPOSE
int macos_utimes(const char *filename, const struct timeval times[2])
#else
int utimes(const char *filename, const struct timeval times[2])
#endif
{
  ftpl_init();
  if (!CHECK_MISSING_REAL(utimes)) return -1;

  int result;
  struct timeval tn[2];
  if (fake_utime_disabled)
  {
    if (times == NULL)
    { /* The user wants their given fake times left alone but they requested NOW, so turn it into fake NOW */
      fake_gettimeofday(&tn[0]);
      tn[1] = tn[0];
      times = tn;
    }
  }
  else if (times != NULL)
  {
    struct timeval user_offset2;
    user_offset2.tv_sec = user_offset.tv_sec;
    user_offset2.tv_usec = user_offset.tv_nsec / 1000;
    timersub(&times[0], &user_offset2, &tn[0]);
    timersub(&times[1], &user_offset2, &tn[1]);
    times = tn;
  }
#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(result = utimes(filename, times));
#else
  DONT_FAKE_TIME(result = real_utimes(filename, times));
#endif
  return result;
}

/* This conditionally offsets 2 timespec values. The caller's out_times array
 * always contains valid translated values, even if in_times was NULL. */
static void fake_two_timespec(const struct timespec in_times[2], struct timespec out_times[2])
{
  if (in_times == NULL) /* Translate NULL into 2 UTIME_NOW values */
  {
    out_times[0].tv_sec = out_times[1].tv_sec = 0;
    out_times[0].tv_nsec = out_times[1].tv_nsec = UTIME_NOW;
    in_times = out_times;
  }
  struct timespec now;
  now.tv_nsec = UTIME_OMIT; /* Wait to grab the current time to see if it's actually needed */
  int j;
  for (j = 0; j <= 1; j++)
  {
    /* We need to preserve 2 special time values in addition to when the user disables utime offsets */
    if (fake_utime_disabled || in_times[j].tv_nsec == UTIME_OMIT || in_times[j].tv_nsec == UTIME_NOW)
    {
      if (fake_utime_disabled && in_times[j].tv_nsec == UTIME_NOW)
      { /* The user wants their given fake times left alone but they requested NOW, so turn it into fake NOW */
        if (now.tv_nsec == UTIME_OMIT) /* did we grab "now" yet? */
        {
          DONT_FAKE_TIME(real_clock_gettime(CLOCK_REALTIME, &now));
        }
        timeradd2(&now, &user_offset, &out_times[j], n);
      }
      else if (out_times != in_times)
      { /* Just preserve the input value */
        out_times[j] = in_times[j];
      }
    }
    else
    {
      timersub2(&in_times[j], &user_offset, &out_times[j], n);
    }
  }
}

#ifdef MACOS_DYLD_INTERPOSE
int macos_utimensat(int dirfd, const char *filename, const struct timespec times[2], int flags)
#else
int utimensat(int dirfd, const char *filename, const struct timespec times[2], int flags)
#endif
{
  ftpl_init();
  if (!CHECK_MISSING_REAL(utimensat)) return -1;

  int result;
  struct timespec tn[2];
  fake_two_timespec(times, tn);
#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(result = utimensat(dirfd, filename, tn, flags));
#else
  DONT_FAKE_TIME(result = real_utimensat(dirfd, filename, tn, flags));
#endif
  return result;
}

#ifdef MACOS_DYLD_INTERPOSE
int macos_futimens(int fd, const struct timespec times[2])
#else
int futimens(int fd, const struct timespec times[2])
#endif
{
  ftpl_init();
  if (!CHECK_MISSING_REAL(futimens)) return -1;

  int result;
  struct timespec tn[2];
  fake_two_timespec(times, tn);
#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(result = futimens(fd, tn));
#else
  DONT_FAKE_TIME(result = real_futimens(fd, tn));
#endif
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
#ifdef MACOS_DYLD_INTERPOSE
int macos_nanosleep(const struct timespec *req, struct timespec *rem)
#else
int nanosleep(const struct timespec *req, struct timespec *rem)
#endif
{
  int result;
  struct timespec real_req;

  ftpl_init();
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

#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(result = (*nanosleep)(&real_req, rem));
#else
  DONT_FAKE_TIME(result = (*real_nanosleep)(&real_req, rem));
#endif
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

  ftpl_init();
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
        get_fake_monotonic_setting(&fake_monotonic_clock);
        if (fake_monotonic_clock)
        {
          timespecadd(&ftpl_starttime.mon, &tdiff, &real_req);
        }
        else
        { /* leave untouched if CLOCK_MONOTONIC but faking monotonic clock disabled */
            real_req = *req;
        }
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
#ifdef MACOS_DYLD_INTERPOSE
int macos_usleep(useconds_t usec)
#else
int usleep(useconds_t usec)
#endif
{
  int result;

  ftpl_init();
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
#ifdef MACOS_DYLD_INTERPOSE
      DONT_FAKE_TIME(result = (*usleep)((1.0 / user_rate) * usec));
#else
      DONT_FAKE_TIME(result = (*real_usleep)((1.0 / user_rate) * usec));
#endif
      return result;
    }

    real_req.tv_sec = usec / 1000000;
    real_req.tv_nsec = (usec % 1000000) * 1000;
    timespecmul(&real_req, 1.0 / user_rate, &real_req);
#ifdef MACOS_DYLD_INTERPOSE
    DONT_FAKE_TIME(result = (*nanosleep)(&real_req, NULL));
#else
    DONT_FAKE_TIME(result = (*real_nanosleep)(&real_req, NULL));
#endif
  }
  else
  {
#ifdef MACOS_DYLD_INTERPOSE
    DONT_FAKE_TIME(result = (*usleep)(usec));
#else
    DONT_FAKE_TIME(result = (*real_usleep)(usec));
#endif
  }
  return result;
}

/*
 * Faked sleep()
 */
#ifdef MACOS_DYLD_INTERPOSE
unsigned int macos_sleep(unsigned int seconds)
#else
unsigned int sleep(unsigned int seconds)
#endif
{
  ftpl_init();
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
#ifdef MACOS_DYLD_INTERPOSE
      DONT_FAKE_TIME(ret = (*sleep)((1.0 / user_rate) * seconds));
#else
      DONT_FAKE_TIME(ret = (*real_sleep)((1.0 / user_rate) * seconds));
#endif
      return (user_rate_set && !dont_fake)?(user_rate * ret):ret;
    }
    else
    {
      int result;
      struct timespec real_req = {seconds, 0}, rem;
      timespecmul(&real_req, 1.0 / user_rate, &real_req);
#ifdef MACOS_DYLD_INTERPOSE
      DONT_FAKE_TIME(result = (*nanosleep)(&real_req, &rem));
#else
      DONT_FAKE_TIME(result = (*real_nanosleep)(&real_req, &rem));
#endif
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
#ifdef MACOS_DYLD_INTERPOSE
    DONT_FAKE_TIME(ret = (*sleep)(seconds));
#else
    DONT_FAKE_TIME(ret = (*real_sleep)(seconds));
#endif
    return ret;
  }
}

/*
 * Faked alarm()
 * @note due to rounding alarm(2) with faketime -f '+0 x7' won't wait 2/7
 * wall clock seconds but 0 seconds
 */
#ifdef MACOS_DYLD_INTERPOSE
unsigned int macos_alarm(unsigned int seconds)
#else
unsigned int alarm(unsigned int seconds)
#endif
{
  unsigned int ret;
  unsigned int seconds_real = (user_rate_set && !dont_fake)?((1.0 / user_rate) * seconds):seconds;

  ftpl_init();
  if (real_alarm == NULL)
  {
    return -1;
  }

#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(ret = (*alarm)(seconds_real));
#else
  DONT_FAKE_TIME(ret = (*real_alarm)(seconds_real));
#endif
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

  ftpl_init();
  if (real_ppoll == NULL)
  {
    return -1;
  }
  if (timeout_ts != NULL)
  {
    if (user_rate_set && !dont_fake && ((timeout_ts->tv_sec > 0) || (timeout_ts->tv_nsec > 0)))
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

  ftpl_init();
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

  ftpl_init();
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
#ifdef MACOS_DYLD_INTERPOSE
int macos_poll(struct pollfd *fds, nfds_t nfds, int timeout)
#else
int poll(struct pollfd *fds, nfds_t nfds, int timeout)
#endif
{
  int ret, timeout_real = (user_rate_set && !dont_fake && (timeout > 0))?(timeout / user_rate):timeout;

  ftpl_init();
  if (real_poll == NULL)
  {
    return -1;
  }

#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(ret = (*poll)(fds, nfds, timeout_real));
#else
  DONT_FAKE_TIME(ret = (*real_poll)(fds, nfds, timeout_real));
#endif
  return ret;
}

/*
 * Faked select()
 */
#ifdef MACOS_DYLD_INTERPOSE
int macos_select(int nfds, fd_set *readfds,
           fd_set *writefds,
           fd_set *errorfds,
           struct timeval *timeout)
#else
int select(int nfds, fd_set *readfds,
           fd_set *writefds,
           fd_set *errorfds,
           struct timeval *timeout)
#endif
{
  int ret;
  struct timeval timeout_real;

  ftpl_init();

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

#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(ret = (*select)(nfds, readfds, writefds, errorfds, timeout == NULL ? timeout : &timeout_real));
#else
  DONT_FAKE_TIME(ret = (*real_select)(nfds, readfds, writefds, errorfds, timeout == NULL ? timeout : &timeout_real));
#endif

  /* scale timeout back if user rate is set, #382 */
  if (user_rate_set && (timeout != NULL))
  {
    struct timespec ts;

    ts.tv_sec = timeout_real.tv_sec;
    ts.tv_nsec = timeout_real.tv_usec * 1000;

    timespecmul(&ts, user_rate, &ts);

    timeout->tv_sec = ts.tv_sec;
    timeout->tv_usec = ts.tv_nsec / 1000;
  }

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

  ftpl_init();

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

  if (!CHECK_MISSING_REAL(sem_timedwait)) return -1;

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

/* EXPERIMENTAL */
int sem_clockwait(sem_t *sem, clockid_t clockid, const struct timespec *abstime)
{
  int result;
  struct timespec real_abstime, *real_abstime_pt;

  if ((!fake_monotonic_clock) && (clockid == CLOCK_MONOTONIC))
  {
    DONT_FAKE_TIME(result = (*real_sem_clockwait)(sem, clockid, abstime));
    return result;
  }

  /* sanity check */
  if (abstime == NULL)
  {
    return -1;
  }

  if (!CHECK_MISSING_REAL(sem_clockwait)) return -1;

  if (!dont_fake)
  {
    struct timespec tdiff, timeadj;

    timespecsub(abstime, &user_faked_time_timespec, &tdiff);

    if (user_rate_set)
    {
      timespecmul(&tdiff, 1.0 / user_rate, &timeadj);
    }
    else
    {
        timeadj = tdiff;
    }
    if (clockid == CLOCK_REALTIME)
    {
      timespecadd(&ftpl_starttime.real, &timeadj, &real_abstime);
    }
    if (clockid == CLOCK_MONOTONIC)
    {
      timespecadd(&ftpl_starttime.mon, &timeadj, &real_abstime);
    }
    real_abstime_pt = &real_abstime;
  }
  else
  {
    /* cast away constness */
    real_abstime_pt = (struct timespec *)abstime;
  }

  DONT_FAKE_TIME(result = (*real_sem_clockwait)(sem, clockid, real_abstime_pt));
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
typedef enum {
  FT_COMPAT_GLIBC_2_2,
  FT_COMPAT_GLIBC_2_3_3,
  FT_FD,
} ft_lib_compat_timer;


/*
 * Faked timer_settime()
 * Does not affect timer speed when stepping clock with each time() call.
 */
static int
timer_settime_common(timer_t_or_int timerid, int flags,
         const struct itimerspec *new_value,
         struct itimerspec *old_value, ft_lib_compat_timer compat,
         int abstime_flag)
{
  int result;
  struct itimerspec new_real;
  struct itimerspec *new_real_pt = &new_real;

  ftpl_init();
  if (new_value == NULL)
  {
    new_real_pt = NULL;
  }
  else if (dont_fake)
  {
    /* cast away constness */
    new_real_pt = (struct itimerspec *)new_value;
  }
  else
  {
    /* set it_value */
    if ((new_value->it_value.tv_sec != 0) ||
        (new_value->it_value.tv_nsec != 0))
    {
      if (flags & abstime_flag)
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
    case FT_FD:
       DONT_FAKE_TIME(result = (*real_timerfd_settime)(timerid.int_member,
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
  ftpl_init();
  if (real_timer_settime_22 == NULL)
  {
    return -1;
  }
  else
  {
    timer_t_or_int temp;
    temp.int_member = timerid;
    return (timer_settime_common(temp, flags, new_value, old_value,
            FT_COMPAT_GLIBC_2_2, TIMER_ABSTIME));
  }
}

/*
 * Faked timer_settime() compatible with implementation in GLIBC 2.3.3
 */
int timer_settime_233(timer_t timerid, int flags,
      const struct itimerspec *new_value,
      struct itimerspec *old_value)
{
  ftpl_init();
  if (real_timer_settime_233 == NULL)
  {
    return -1;
  }
  else
  {
    timer_t_or_int temp;
    temp.timer_t_member = timerid;
    return (timer_settime_common(temp, flags, new_value, old_value,
            FT_COMPAT_GLIBC_2_3_3, TIMER_ABSTIME));
  }
}

/*
 * Faked timer_gettime()
 * Does not affect timer speed when stepping clock with each time() call.
 */
int timer_gettime_common(timer_t_or_int timerid, struct itimerspec *curr_value, ft_lib_compat_timer compat)
{
  int result;

  ftpl_init();
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
    case FT_FD:
       DONT_FAKE_TIME(result = (*real_timerfd_gettime)(timerid.int_member, curr_value));
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
  ftpl_init();
  if (real_timer_gettime_22 == NULL)
  {
    return -1;
  }
  else
  {
    timer_t_or_int temp;
    temp.timer_t_member = timerid;
    return (timer_gettime_common(temp, curr_value,
         FT_COMPAT_GLIBC_2_2));
  }
}

/*
 * Faked timer_gettime() compatible with implementation in GLIBC 2.3.3
 */
int timer_gettime_233(timer_t timerid, struct itimerspec *curr_value)
{
  ftpl_init();
  if (real_timer_gettime_233 == NULL)
  {
    return -1;
  }
  else
  {
    timer_t_or_int temp;
    temp.timer_t_member = timerid;
    return (timer_gettime_common(temp, curr_value,
            FT_COMPAT_GLIBC_2_3_3));
  }
}

__asm__(".symver timer_gettime_22, timer_gettime@GLIBC_2.2");
__asm__(".symver timer_gettime_233, timer_gettime@@GLIBC_2.3.3");
__asm__(".symver timer_settime_22, timer_settime@GLIBC_2.2");
__asm__(".symver timer_settime_233, timer_settime@@GLIBC_2.3.3");

#ifdef __linux__
/*
 * Faked timerfd_settime
 */
int timerfd_settime(int fd, int flags,
         const struct itimerspec *new_value,
         struct itimerspec *old_value)
{
  ftpl_init();
  if (real_timerfd_settime == NULL)
  {
    return -1;
  }
  else
  {
    timer_t_or_int temp;
    temp.int_member = fd;
    return (timer_settime_common(temp, flags, new_value, old_value, FT_FD,
                                 TFD_TIMER_ABSTIME));
  }
}

/*
 * Faked timerfd_gettime()
 */
int timerfd_gettime(int fd, struct itimerspec *curr_value)
{
  ftpl_init();
  if (real_timerfd_gettime == NULL)
  {
    return -1;
  }
  else
  {
    timer_t_or_int temp;
    temp.int_member = fd;
    return (timer_gettime_common(temp, curr_value, FT_FD));
  }
}
#endif

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
#ifdef MACOS_DYLD_INTERPOSE
time_t macos_time(time_t *time_tptr)
#else
time_t time(time_t *time_tptr)
#endif
{
  struct timespec tp;
  time_t result;

  ftpl_init();
#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(result = (*clock_gettime)(CLOCK_REALTIME, &tp));
#else
  DONT_FAKE_TIME(result = (*real_clock_gettime)(CLOCK_REALTIME, &tp));
#endif
  if (result == -1) return -1;

  /* pass the real current time to our faking version, overwriting it */
  (void)fake_clock_gettime(CLOCK_REALTIME, &tp);

  if (time_tptr != NULL)
  {
    *time_tptr = tp.tv_sec;
  }
  return tp.tv_sec;
}

#ifdef MACOS_DYLD_INTERPOSE
int macos_ftime(struct timeb *tb)
#else
int ftime(struct timeb *tb)
#endif
{
  struct timespec tp;
  int result;

  ftpl_init();
  /* sanity check */
  if (tb == NULL)
    return 0;               /* ftime() always returns 0, see manpage */

  /* Check whether we've got a pointer to the real ftime() function yet */
  if (!CHECK_MISSING_REAL(ftime)) return 0;

  /* initialize our TZ result with the real current time */
#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(result = (*ftime)(tb));
#else
  DONT_FAKE_TIME(result = (*real_ftime)(tb));
#endif
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

#ifdef MACOS_DYLD_INTERPOSE
int macos_gettimeofday(struct timeval *tv, void *tz)
#else
int gettimeofday(struct timeval *tv, void *tz)
#endif
{
  int result;

  ftpl_init();
  /* sanity check */
  if (tv == NULL)
  {
    return -1;
  }

  /* Check whether we've got a pointer to the real ftime() function yet */
  if (!CHECK_MISSING_REAL(gettimeofday)) return -1;

  /* initialize our result with the real current time */
#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(result = (*gettimeofday)(tv, tz));
#else
  DONT_FAKE_TIME(result = (*real_gettimeofday)(tv, tz));
#endif
  if (result == -1) return result; /* original function failed */

  /* pass the real current time to our faking version, overwriting it */
  result = fake_gettimeofday(tv);

  /* return the result to the caller */
  return result;
}

#ifdef MACOS_DYLD_INTERPOSE
int macos_clock_gettime(clockid_t clk_id, struct timespec *tp)
#else
int clock_gettime(clockid_t clk_id, struct timespec *tp)
#endif
{
  int result;

  ftpl_init();
  // If ftpl_init ends up recursing, pthread_once will deadlock.
  // (Previously we attempted to detect this situation, and bomb out,
  // but the approach taken wasn't thread-safe and broke in practice.)

  /* sanity check */
  if (tp == NULL)
  {
    return -1;
  }

  if (!CHECK_MISSING_REAL(clock_gettime)) return -1;

  /* initialize our result with the real current time */
#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(result = (*clock_gettime)(clk_id, tp));
#else
  DONT_FAKE_TIME(result = (*real_clock_gettime)(clk_id, tp));
#endif
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

/* this is used by 32-bit architectures only */
int __clock_gettime64(clockid_t clk_id, struct __timespec64 *tp64)
{
  struct timespec tp;
  int result;

  result = clock_gettime(clk_id, &tp);
  tp64->tv_sec = tp.tv_sec;
  tp64->tv_nsec = tp.tv_nsec;
  return result;
}

/* this is used by 32-bit architectures only */
int __gettimeofday64(struct __timeval64 *tv64, void *tz)
{
  struct timeval tv;
  int result;

  result = gettimeofday(&tv, tz);
  tv64->tv_sec = tv.tv_sec;
  tv64->tv_usec = tv.tv_usec;
  return result;
}

/* this is used by 32-bit architectures only */
uint64_t __time64(uint64_t *write_out)
{
  struct timespec tp;
  uint64_t output;
  int error;

  error = clock_gettime(CLOCK_REALTIME, &tp);
  if (error == -1)
  {
    return (uint64_t)error;
  }
  output = tp.tv_sec;

  if (write_out)
  {
    *write_out = output;
  }
  return output;
}

#ifdef TIME_UTC
#ifdef MACOS_DYLD_INTERPOSE
int macos_timespec_get(struct timespec *ts, int base)
#else
int timespec_get(struct timespec *ts, int base)
#endif
{
  int result;

  ftpl_init();
  /* sanity check */
  if (ts == NULL)
  {
    return 0;
  }

  if (!CHECK_MISSING_REAL(timespec_get)) return 0;

  /* initialize our result with the real current time */
#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(result = (*timespec_get)(ts, base));
#else
  DONT_FAKE_TIME(result = (*real_timespec_get)(ts, base));
#endif
  if (result == 0) return result; /* original function failed */

  /* pass the real current time to our faking version, overwriting it */
  (void)fake_clock_gettime(CLOCK_REALTIME, ts);

  /* return the result to the caller */
  return result;
}
#endif


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
      /* No change but eventually when using FAKETIME_FOLLOW_FILE */
      if (user_faked_time[0] != '%')
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
        fprintf(stderr, "libfaketime: In parse_ft_string(), failed to parse FAKETIME timestamp.\n"
                "Please check specification %s with format %s\n", user_faked_time, user_faked_time_fmt);
        exit(EXIT_FAILURE);
      }
      goto parse_modifiers;
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
        fprintf(stderr, "libfaketime: In parse_ft_string(), failed to parse FAKETIME timestamp.\n");
        exit(EXIT_FAILURE);
      }

      /* Reset starttime */
      if (NULL == getenv("FAKETIME_DONT_RESET"))
        reset_time();
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
        reset_time();
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
        /* increment time with every time() call */
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

static void ftpl_really_init(void)
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

  char *ignore_list = getenv("FAKETIME_IGNORE_SYMBOLS");
  bool should_debug_dlsym = getenv("FAKETIME_DEBUG_DLSYM");
#define dlsym(handle, symbol) ft_dlvsym(handle, symbol, NULL, symbol, ignore_list, should_debug_dlsym)
#define dlvsym(handle, symbol, version) ft_dlvsym(handle, symbol, version, symbol "@" version, ignore_list, should_debug_dlsym)

  /* Look up all real_* functions. NULL will mark missing ones. */
  real_stat =               dlsym(RTLD_NEXT, "stat");
  real_lstat =              dlsym(RTLD_NEXT, "lstat");
  real_fstat =              dlsym(RTLD_NEXT, "fstat");
  real_xstat =              dlsym(RTLD_NEXT, "__xstat");
  real_fxstat =             dlsym(RTLD_NEXT, "__fxstat");
  real_fxstatat =           dlsym(RTLD_NEXT, "__fxstatat");
  real_lxstat =             dlsym(RTLD_NEXT, "__lxstat");
  real_xstat64 =            dlsym(RTLD_NEXT,"__xstat64");
  real_fxstat64 =           dlsym(RTLD_NEXT, "__fxstat64");
  real_fxstatat64 =         dlsym(RTLD_NEXT, "__fxstatat64");
  real_lxstat64 =           dlsym(RTLD_NEXT, "__lxstat64");
#ifdef STATX_TYPE
  real_statx =              dlsym(RTLD_NEXT, "statx");
#endif
  real_time =               dlsym(RTLD_NEXT, "time");
  real_ftime =              dlsym(RTLD_NEXT, "ftime");
#ifdef TIME_UTC
  real_timespec_get =       dlsym(RTLD_NEXT, "timespec_get");
#endif
#ifdef FAKE_FILE_TIMESTAMPS
  real_utimes  =            dlsym(RTLD_NEXT, "utimes");
  real_utime   =            dlsym(RTLD_NEXT, "utime");
  real_utimensat =          dlsym(RTLD_NEXT, "utimensat");
  real_futimens =           dlsym(RTLD_NEXT, "futimens");
#endif
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
  real_sem_clockwait =      dlsym(RTLD_NEXT, "sem_clockwait");
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

#ifdef FAKE_RANDOM
  real_getrandom = dlsym(RTLD_NEXT, "getrandom");
  real_getentropy = dlsym(RTLD_NEXT, "getentropy");
#endif

#ifdef FAKE_PID
  real_getpid = dlsym(RTLD_NEXT, "getpid");
#endif

#ifdef INTERCEPT_SYSCALL
  real_syscall = dlsym(RTLD_NEXT, "syscall");
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
  real_clock_gettime64 =    dlsym(RTLD_NEXT, "clock_gettime64");
  if (NULL == real_clock_gettime64)
  {
    real_clock_gettime64 =  dlsym(RTLD_NEXT, "__clock_gettime64");
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
#ifdef __linux__
  real_timerfd_gettime =  dlsym(RTLD_NEXT, "timerfd_gettime");
  real_timerfd_settime =  dlsym(RTLD_NEXT, "timerfd_settime");
#endif
#endif
#endif

#ifdef MACOS_DYLD_INTERPOSE
  do_macos_dyld_interpose();
#endif

#undef dlsym
#undef dlvsym

#ifdef FAKE_STATELESS
  if (0) ft_shm_init();
#else
  tmp_env = getenv("FAKETIME_DISABLE_SHM");
  if (!tmp_env || tmp_env[0] == '0') {
    ft_shm_init();
  }
#endif
#ifdef FAKE_STAT
  if (getenv("NO_FAKE_STAT")!=NULL)
  {
    fake_stat_disabled = 1;  //Note that this is NOT re-checked
  }
#endif
#if defined FAKE_FILE_TIMESTAMPS
#ifdef FAKE_UTIME
  // fake_utime_disabled is 0 by default
  if ((tmp_env = getenv("FAKE_UTIME")) != NULL) //Note that this is NOT re-checked
  {
    if (!*tmp_env || *tmp_env == 'y' || *tmp_env == 'Y' || *tmp_env == 't' || *tmp_env == 'T')
    { /* an empty string or a yes/true value turns off disabling */
      fake_utime_disabled = 0;
    }
    else
    { /* Any other non-number disables the utime functions, but we also support FAKE_UTIME=1 to enable */
      fake_utime_disabled = !atoi(tmp_env);
    }
  }
#else
  // compiled without FAKE_UTIME support, so don't allow it to be controlled by the env var
  fake_utime_disabled = 1;
#endif
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
  get_fake_monotonic_setting(&fake_monotonic_clock);
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

  /* load file only if reading timestamps from it is not finished yet */
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
  else
  {
    read_config_file();
  }

  dont_fake = dont_fake_final;
}

inline static void ftpl_init(void) {
  pthread_once(&initialized_once_control, ftpl_really_init);
}

void *ft_dlvsym(void *handle, const char *symbol, const char *version,
    const char *full_name, char *ignore_list, bool should_debug_dlsym)
{
  // dlsym or dlvsym with a non-resolving symbol results in a malloc call,
  // which can trigger infinite recursion or deadlock, as seen in
  // https://github.com/wolfcw/libfaketime/issues/130
  // As a work-around, enable users to identify the list of calls,
  // FAKETIME_DEBUG_DLSYM=1 faketime ...
  // and then repeat the faketime call again to skip dlsym/dlvsym calls
  // for these symbols, for example:
  // FAKETIME_IGNORE_SYMBOLS=__ftime,timer_settime@GLIBC_2.2,timer_gettime@GLIBC_2.2 faketime ...
  if (ignore_list && str_array_contains(ignore_list, full_name)) {
    return NULL;
  }
  void *addr = NULL;
#ifdef __GLIBC__
  if (version) {
    addr = dlvsym(handle, symbol, version);
  } else {
    addr = dlsym(handle, symbol);
  }
#else
  // dlvsym does not exists, version is always NULL at compile time.
  addr = dlsym(handle, symbol);
  if (version != NULL) fprintf(stderr, "ft_dlvsym(): version is not NULL\n");
#endif
  if (!addr && should_debug_dlsym) {
    fprintf(stderr, "[FAKETIME_DEBUG_DLSYM] Cannot find symbol: %s\n", full_name);
  }
  return addr;
}


/*
 *      =======================================================================
 *      Helper functions                                             === HELPER
 *      =======================================================================
 */

static void prepare_config_contents(char *contents)
{
  /* This function
   * - removes line separators (\r and \n)
   * - removes lines beginning with a comment character (# or ;)
   */
  char *read_position = contents;
  char *write_position = contents;
  bool in_comment = false;
  bool beginning_of_line = true;

  while (*read_position != '\0') {
    if (beginning_of_line && (*read_position == '#' || *read_position == ';')) {
      /* The line begins with a comment character and should be completely ignored */
      in_comment = true;
    }
    beginning_of_line = false;

    if (*read_position == '\n') {
      /* We reached the end of the line that should be ignored (if any is ignored) */
      in_comment = false;
      /* The next character begins a new line */
      beginning_of_line = true;
    }

    /* If we are not in a comment and are not looking at a line break, copy the
     * character from the read position to the write position. */
    if (!in_comment && *read_position != '\r' && *write_position != '\n') {
      *write_position = *read_position;
      write_position++;
    }
    read_position++;
  }
  *write_position = '\0';
}

bool str_array_contains(const char *haystack, const char *needle)
{
  size_t needle_len = strlen(needle);
  char *pos = strstr(haystack, needle);
  while (pos) {
    if (pos == haystack || *(pos - 1) == ',') {
      char nextc = *(pos + needle_len);
      if (nextc == '\0' || nextc == ',') {
        // Found needle in comma-separated haystack.
        return true;
      }
    }
    pos = strstr(pos + 1, needle);
  }
  return false;
}


/*
 *      =======================================================================
 *      Implementation of faked functions                        === FAKE(FAKE)
 *      =======================================================================
 */

#ifdef PTHREAD_SINGLETHREADED_TIME
/*
 * To avoid a deadlock if a faketime function is interrupted by a signal while
 * holding the lock, we block all signals while the mutex is locked.
 * The original_mask field is used to restore the previous set of signals
 * after the lock has been released.
 * (Prompted by issues with parallel garbage collection in D 2.090. D uses signals
 * to freeze all but one thread. The frozen threads may be in faketime operations.)
 */
struct LockedState {
  pthread_mutex_t *mutex;
  sigset_t original_mask;
};

static void pthread_cleanup_mutex_lock(void *data)
{
  struct LockedState *state = data;
  pthread_mutex_unlock(state->mutex);
  pthread_sigmask(SIG_SETMASK, &state->original_mask, NULL);
}
#endif

int read_config_file()
{
  static char user_faked_time[BUFFERLEN]; /* changed to static for caching in v0.6 */
  static char custom_filename[BUFSIZ];
  static char filename[BUFSIZ];
  int faketimerc;
  /* check whether there's a .faketimerc in the user's home directory, or
   * a system-wide /etc/faketimerc present.
   * The /etc/faketimerc handling has been contributed by David Burley,
   * Jacob Moorman, and Wayne Davison of SourceForge, Inc. in version 0.6 */
  (void) snprintf(custom_filename, BUFSIZ, "%s", getenv("FAKETIME_TIMESTAMP_FILE"));
  (void) snprintf(filename, BUFSIZ, "%s/.faketimerc", getenv("HOME"));
  if ((faketimerc = open(custom_filename, O_RDONLY)) != -1 ||
      (faketimerc = open(filename, O_RDONLY)) != -1 ||
      (faketimerc = open("/etc/faketimerc", O_RDONLY)) != -1)
  {
    ssize_t bytes;
    ssize_t length = 0;
    while ((bytes = read(faketimerc, user_faked_time + length, sizeof(user_faked_time) - 1 - length)) > 0) {
      length += bytes;
    }
    close(faketimerc);
    if (bytes < 0) {
      length = 0;
    }
    user_faked_time[length] = 0;

    prepare_config_contents(user_faked_time);
    parse_ft_string(user_faked_time);
    return 1;
  }
  return 0;
}

int fake_clock_gettime(clockid_t clk_id, struct timespec *tp)
{
  /* variables used for caching, introduced in version 0.6 */
  static time_t last_data_fetch = 0;  /* not fetched previously at first call */
  static int cache_expired = 1;       /* considered expired at first call */

  /* Karl Chan's v0.8 sanity check moved here for 0.9.9 */
  if (tp == NULL) return -1;

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

  // {ret = value; goto abort;} to call matching pthread_cleanup_pop and return value
  volatile int ret = INT_MAX;

#ifdef PTHREAD_SINGLETHREADED_TIME
  static pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;

  // block all signals while locked. prevents deadlocks if signal interrupts in in mid-operation.
  sigset_t all_signals, original_mask;
  sigfillset(&all_signals);
  pthread_sigmask(SIG_SETMASK, &all_signals, &original_mask);
  pthread_mutex_lock(&time_mutex);

  struct LockedState state = { .mutex = &time_mutex, .original_mask = original_mask };
  pthread_cleanup_push(pthread_cleanup_mutex_lock, &state);
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
      if (((ft_start_after_secs != -1)    && (tmp_ts.tv_sec < ft_start_after_secs))
        || ((ft_stop_after_secs != -1)    && (tmp_ts.tv_sec >= ft_stop_after_secs))
        || ((ft_start_after_ncalls != -1) && (callcounter < ft_start_after_ncalls))
        || ((ft_stop_after_ncalls != -1)  && (callcounter >= ft_stop_after_ncalls)))
      {
        ret = 0;
        goto abort;
      }
      /* fprintf(stderr, "(libfaketime limits -> runtime: %lu, callcounter: %lu continues\n", (*time_tptr - ftpl_starttime), callcounter); */
    }

    if (spawnsupport)
    {
      /* check whether we should spawn an external command */
      if (spawned == 0)
      { /* exec external command once only */
        if ((((ft_spawn_secs > -1) && (tmp_ts.tv_sec >= ft_spawn_secs)) || (callcounter == ft_spawn_ncalls)) && (spawned == 0))
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
      if (read_config_file() == 0) parse_ft_string(user_faked_time);
    } /* read fake time from file */
    else
    {
      parse_ft_string(user_faked_time);
    }
    /* read monotonic faketime setting from envar */
    get_fake_monotonic_setting(&fake_monotonic_clock);
  } /* cache had expired */

  if (infile_set)
  {
    if (load_time(tp))
    {
      ret = 0;
      goto abort;
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
        /* increment time with every time() call */
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
      ret = -1;
      goto abort;
  } // end of switch(ft_mode)

abort:
#ifdef PTHREAD_SINGLETHREADED_TIME
  pthread_cleanup_pop(1);
#endif
  // came here via goto abort?
  if (ret != INT_MAX) return ret;
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

  if (!CHECK_MISSING_REAL(clock_get_time)) return -1;

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
  if (!CHECK_MISSING_REAL(__gettimeofday)) return -1;

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

  if (!CHECK_MISSING_REAL(__clock_gettime)) return -1;

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
  if (!CHECK_MISSING_REAL(__ftime)) return 0;

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

  ftpl_init();
  if (!CHECK_MISSING_REAL(pthread_cond_init_232)) return -1;
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

  ftpl_init();

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

/*
 * Check whether we need a run-time activation of the
 * forced monotonic fix to avoid faked / unfaked timestamp
 * confusion between the application and glibc internals.
 */
bool needs_forced_monotonic_fix(char *function_name)
{
  bool result = false;
  char *env_var;
#ifdef __GLIBC__
  const char *glibc_version_string = gnu_get_libc_version();
#endif

  if (function_name == NULL) return false;

  /* The forced monotonic fix can be activated by setting an
   * environment variable to 1, or disabled by setting it to 0 */
  if ((env_var = getenv("FAKETIME_FORCE_MONOTONIC_FIX")) != NULL)
  {
    if (env_var[0] == '0')
      result = false;
    else
      result = true;
  }
  else
  {
#ifdef __GLIBC__
    /* Here we try to derive the necessity for a forced monotonic fix *
     * based on glibc version. What could possibly go wrong?          */

    int glibc_major, glibc_minor;
    sscanf(glibc_version_string, "%d.%d", &glibc_major, &glibc_minor);

    /* The following decision logic is yet purely based on experiences
     * with pthread_cond_timedwait(). The used boundaries may still be
     * imprecise. */
    if ( (glibc_major == 2) &&
         ((glibc_minor <= 24) || (glibc_minor >= 30)) )
    {
      result = true;
    }
    else
#endif
      result = false; // avoid forced monotonic fixes unless really necessary
  }

  if (getenv("FAKETIME_DEBUG") != NULL)
#ifdef __GLIBC__
    fprintf(stderr, "libfaketime: forced monotonic fix for %s = %s (glibc version %s)\n",
		    function_name, result ? "yes":"no", glibc_version_string);
#else
    fprintf(stderr, "libfaketime: forced monotonic fix for %s = %s (not glibc-compiled)\n",
		    function_name, result ? "yes":"no");
#endif

  return result;
}

int pthread_cond_timedwait_common(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime, ft_lib_compat_pthread compat)
{
  struct timespec tp, tdiff_actual, realtime, faketime;
  struct timespec *tf = NULL;
  struct pthread_cond_monotonic* e;
  char *tmp_env;
  int wait_ms;
  clockid_t clk_id;
  int result = 0;

  ftpl_init();

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
    if (clk_id == CLOCK_MONOTONIC) {
      if (needs_forced_monotonic_fix("pthread_cond_timedwait") == true) {
        timespecadd(&realtime, &tdiff_actual, &tp);
      }
      else {
        timespecadd(&faketime, &tdiff_actual, &tp);
      }
    }
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
#ifdef MACOS_DYLD_INTERPOSE
int macos_clock_settime(clockid_t clk_id, const struct timespec *tp) {
#else
int clock_settime(clockid_t clk_id, const struct timespec *tp) {
#endif

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
#ifdef MACOS_DYLD_INTERPOSE
  DONT_FAKE_TIME(macos_clock_gettime(clk_id, &current_time))
#else
  DONT_FAKE_TIME(clock_gettime(clk_id, &current_time))
#endif
   ;

  time_t sec_diff = tp->tv_sec - current_time.tv_sec;
  long nsec_diff = tp->tv_nsec - current_time.tv_nsec;
  char newenv_string[256];
  double offset = (double) sec_diff;
  offset += (double) nsec_diff/SEC_TO_nSEC;
  snprintf(newenv_string, 255, "%+f", offset);

  parse_config_file = false; /* #247: make sure environment takes precedence */
  setenv("FAKETIME", newenv_string, 1);
  force_cache_expiration = 1; /* make sure it becomes effective immediately */

  /* If FAKETIME_TIMESTAMP_FILE was given in environment,
   * and if FAKETIME_UPDATE_TIMESTAMP_FILE=1, then update it.
   * This allows other process instances to share the same time. */
  if (    (getenv("FAKETIME_TIMESTAMP_FILE") != NULL)
       && (*getenv("FAKETIME_TIMESTAMP_FILE") != '\0')
       && (getenv("FAKETIME_UPDATE_TIMESTAMP_FILE") != NULL)
       && (strcmp(getenv("FAKETIME_UPDATE_TIMESTAMP_FILE"), "1") == 0))
  {
    const char *error = NULL;
    FILE *envfile;
    static char custom_filename[BUFSIZ];
    (void) snprintf(custom_filename, BUFSIZ, "%s", getenv("FAKETIME_TIMESTAMP_FILE"));

    if ((envfile = fopen(custom_filename, "wt")) != NULL)
    {
      if (fprintf(envfile, "%+f\n", offset) < 0)
      {
        error = "to write to file";
      }
      if (fclose(envfile) != 0)
      {
        error = "to close file";
      }
    }
    else
    {
      error = "to open file";
    }
    if (error)
    {
      fprintf(stderr, "libfaketime: In clock_settime(), failed to "
        "%s while updating FAKETIME_TIMESTAMP_FILE (`%s'): %s\n",
        error, getenv("FAKETIME_TIMESTAMP_FILE"), strerror(errno));
    }
  }

  return 0;
}

#ifdef MACOS_DYLD_INTERPOSE
int macos_settimeofday(const struct timeval *tv, void *tz)
#else
int settimeofday(const struct timeval *tv, void *tz)
#endif
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
#ifdef MACOS_DYLD_INTERPOSE
    macos_clock_settime(CLOCK_REALTIME, &tp);
#else
    clock_settime(CLOCK_REALTIME, &tp);
#endif
  }
  return 0;
}

#ifdef MACOS_DYLD_INTERPOSE
int macos_adjtime (const struct timeval *delta, struct timeval *olddelta)
#else
int adjtime (const struct timeval *delta, struct timeval *olddelta)
#endif
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
#ifdef MACOS_DYLD_INTERPOSE
    macos_clock_gettime(CLOCK_REALTIME, &tp);
#else
    clock_gettime(CLOCK_REALTIME, &tp);
#endif
    tp.tv_sec += delta->tv_sec;
    tp.tv_nsec += delta->tv_usec * 1000;
    /* This actually will make the clock jump instead of gradually
       adjusting it, but we fulfill the caller's intention and an
       additional thread just for the gradual changes does not seem
       to be worth the effort presently. */
#ifdef MACOS_DYLD_INTERPOSE
    clock_settime(CLOCK_REALTIME, &tp);
#else
    clock_settime(CLOCK_REALTIME, &tp);
#endif
  }
  return 0;
}
#endif

#ifdef FAKE_RANDOM
/*
  Local copy of
  Middle Square Weyl Sequence Random Number Generator
  Copyright (c) 2014-2020 Bernard Widynski
  License: GNU GPL v3
  see https://mswsrng.wixsite.com/rand

  adapted to take the seed s as a parameter and return only a byte
*/
inline static uint32_t fakerandom_msws(uint64_t s) {
   static uint64_t x = 0, w = 0;
   x *= x; x += (w += s);
   x = (x>>32) | (x<<32);
   return (char) x & 0xFF;
}

/* return 0 if no FAKERANDOM_SEED was seen */
static int bypass_randomness(void* buf, size_t buflen) {
  char *seedstring = getenv("FAKERANDOM_SEED");
  char *b = buf;

  if (seedstring != NULL) {
    long long int seed = strtoll(seedstring, NULL, 0);
    for (size_t i = 0; i < buflen; i++) {
      b[i] = fakerandom_msws(seed);
    }
    return 1;
  }
  return 0;
}
ssize_t getrandom(void *buf, size_t buflen, unsigned int flags) {
  if (bypass_randomness(buf, buflen)) {
      return buflen;
  } else {
    ftpl_init();
    return real_getrandom(buf, buflen, flags);
  }
}
#ifdef MACOS_DYLD_INTERPOSE
int macos_getentropy(void *buffer, size_t length) {
#else
int getentropy(void *buffer, size_t length) {
#endif
  if (bypass_randomness(buffer, length)) {
      return 0;
  } else {
    ftpl_init();
#ifdef MACOS_DYLD_INTERPOSE
    return getentropy(buffer, length);
#else
    return real_getentropy(buffer, length);
#endif
  }
}
#endif

#ifdef FAKE_PID
#ifdef MACOS_DYLD_INTERPOSE
pid_t macos_getpid() {
#else
pid_t getpid() {
#endif
  const char *pidstring = getenv("FAKETIME_FAKEPID");
  if (pidstring != NULL) {
    long int pid = strtol(pidstring, NULL, 0);
    return (pid_t)(pid);
  } else {
    ftpl_init();
    return real_getpid();
  }
}
#endif

#ifdef INTERCEPT_SYSCALL
#ifdef INTERCEPT_FUTEX
long handle_futex_syscall(long number, uint32_t* uaddr, int futex_op, uint32_t val, struct timespec* timeout, uint32_t* uaddr2, uint32_t val3) {
  if (timeout == NULL) {
    // not timeout related, just call the real syscall
    goto futex_fallback;
  }

  // if ((futex_op & FUTEX_CMD_MASK) == FUTEX_WAIT_BITSET) {
  if (1) {
    clockid_t clk_id = CLOCK_MONOTONIC;
    if (futex_op & FUTEX_CLOCK_REALTIME)
      clk_id = CLOCK_REALTIME;

    struct timespec real_tp, fake_tp;

    DONT_FAKE_TIME((*real_clock_gettime)(clk_id, &real_tp));
    fake_tp = real_tp;
    if (fake_clock_gettime(clk_id, &fake_tp) == -1) {
      goto futex_fallback;
    }
    // Create a corrected timeout by adjusting with the difference between
    // real and fake timestamps
    struct timespec adjusted_timeout, time_diff;
    timespecsub(&fake_tp, &real_tp, &time_diff);
    timespecsub(timeout, &time_diff, &adjusted_timeout);
    // fprintf(stdout, "libfaketime: adjusted timeout: %ld.%09ld\n", adjusted_timeout.tv_sec, adjusted_timeout.tv_nsec);
    long result;
    result = real_syscall(number, uaddr, futex_op, val, &adjusted_timeout, uaddr2, val3);
    if (result != 0) {
      return result;
    }

    // Check if the futex timeout has already passed according to fake time
    struct timespec now_fake;
    if (fake_clock_gettime(clk_id, &now_fake) != 0) {
      return result;
    }

    // If the timeout is already passed in fake time, return 0.
    while (!timespeccmp(&now_fake, timeout, >=)) {
      // Calculate how much real time we need to wait
      struct timespec real_now, fake_now, wait_time;
      DONT_FAKE_TIME((*real_clock_gettime)(clk_id, &real_now));
      fake_clock_gettime(clk_id, &fake_now);

      // Calculate how much fake time is left until the timeout
      struct timespec fake_time_left;
      timespecsub(timeout, &fake_now, &fake_time_left);

      // Scale the fake time left by the user rate if set
      if (user_rate_set && !dont_fake) {
        timespecmul(&fake_time_left, 1.0 / user_rate, &wait_time);
      } else {
        wait_time = fake_time_left;
      }

      // Calculate the real timeout by adding the wait time to the current real time
      struct timespec real_timeout;
      timespecadd(&real_now, &wait_time, &real_timeout);

      // fprintf(stdout, "libfaketime: recalculated real timeout: %ld.%09ld\n",
      //         real_timeout.tv_sec, real_timeout.tv_nsec);

      // Call the real syscall with the recalculated timeout
      result = real_syscall(number, uaddr, futex_op, val, &real_timeout, uaddr2, val3);
      if (result != 0) {
        return result;
      }

      // Check if the futex timeout has already passed according to fake time
      if (fake_clock_gettime(clk_id, &now_fake) != 0) {
        return result;
      }
    }
    return 0;
  } else {
    return real_syscall(number, uaddr, futex_op, val, timeout, uaddr2, val3);
  }

  futex_fallback:
    return real_syscall(number, uaddr, futex_op, val, timeout, uaddr2, val3);
}
#endif

/* see https://github.com/wolfcw/libfaketime/issues/301 */
long syscall(long number, ...) {
  va_list ap;
  va_start(ap, number);
#ifdef FAKE_RANDOM
  if (number == __NR_getrandom && getenv("FAKERANDOM_SEED")) {
    void *buf;
    size_t buflen;
    unsigned int flags;
    buf = va_arg(ap, void*);
    buflen = va_arg(ap, size_t);
    flags = va_arg(ap, unsigned int);
    va_end(ap);
    return getrandom(buf, buflen, flags);
  }
#endif
// static int (*real_clock_gettime) (clockid_t clk_id, struct timespec *tp);
  if (number == __NR_clock_gettime && (getenv("FAKETIME") || getenv("FAKETIME_TIMESTAMP_FILE"))) {
    clockid_t clk_id;
    struct timespec *tp;
    clk_id = va_arg(ap, clockid_t);
    tp = va_arg(ap, struct timespec*);
    va_end(ap);
    return clock_gettime(clk_id, tp);
  }

#ifdef INTERCEPT_FUTEX
  if (number == __NR_futex) {
    uint32_t *uaddr;
    int futex_op;
    uint32_t val;
    struct timespec *timeout;  /* or: uint32_t val2 */
    uint32_t* uaddr2;
    uint32_t val3;

    uaddr = va_arg(ap, uint32_t*);
    futex_op = va_arg(ap, int);
    val = va_arg(ap, uint32_t);
    timeout = va_arg(ap, struct timespec*);
    uaddr2 = va_arg(ap, uint32_t*);
    val3 = va_arg(ap, uint32_t);
    va_end(ap);

    return handle_futex_syscall(number, uaddr, futex_op, val, timeout, uaddr2, val3);
  }
#endif

  variadic_promotion_t a[syscall_max_args];
  for (int i = 0; i < syscall_max_args; i++)
    a[i] = va_arg(ap, variadic_promotion_t);
  va_end(ap);
  ftpl_init();
  return real_syscall(number, a[0], a[1], a[2], a[3], a[4], a[5]);
}
#endif

#ifdef MACOS_DYLD_INTERPOSE
void do_macos_dyld_interpose(void) {
  DYLD_INTERPOSE(macos_clock_gettime, clock_gettime);
  DYLD_INTERPOSE(macos_gettimeofday, gettimeofday);
  DYLD_INTERPOSE(macos_time, time);
  DYLD_INTERPOSE(macos_ftime, ftime);
#ifdef FAKE_SLEEP
  DYLD_INTERPOSE(macos_alarm, alarm);
  DYLD_INTERPOSE(macos_sleep, sleep);
  DYLD_INTERPOSE(macos_usleep, usleep);
  DYLD_INTERPOSE(macos_nanosleep, nanosleep);
  DYLD_INTERPOSE(macos_poll, poll);
#endif
#ifdef TIME_UTC
  DYLD_INTERPOSE(macos_timespec_get, timespec_get);
#endif
  DYLD_INTERPOSE(macos_select, select);
#ifdef FAKE_RANDOM
  DYLD_INTERPOSE(macos_getentropy, getentropy);
#endif
#ifdef FAKE_SETTIME
  DYLD_INTERPOSE(macos_clock_settime, clock_settime);
  DYLD_INTERPOSE(macos_settimeofday, settimeofday);
  DYLD_INTERPOSE(macos_adjtime, adjtime);
#endif
#ifdef FAKE_PID
  DYLD_INTERPOSE(macos_getpid, getpid);
#endif
#ifdef FAKE_STAT
  DYLD_INTERPOSE(macos_stat, stat);
//  DYLD_INTERPOSE(macos_fstat, fstat);
  DYLD_INTERPOSE(macos_lstat, lstat);
#endif
#ifdef FAKE_FILE_TIMESTAMPS
  DYLD_INTERPOSE(macos_utime, utime);
  DYLD_INTERPOSE(macos_utimes, utimes);
  DYLD_INTERPOSE(macos_utimensat, utimensat);
  DYLD_INTERPOSE(macos_futimens, futimens);
#endif
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
