/*
 *  Copyright (C) 2003,2007 Wolfgang Hommel
 *
 *  This file is part of the FakeTime Preload Library.
 *
 *  The FakeTime Preload Library is free software; you can redistribute it
 *  and/or modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  The FakeTime Preload Library is distributed in the hope that it will
 *  be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with the FakeTime Preload Library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>

#ifndef __APPLE__
#ifdef FAKE_STAT
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#else
#include <unistd.h>
#endif

#ifndef __APPLE__
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#define VERBOSE 0

#define SIG SIGUSR1

static void
handler(int sig, siginfo_t *si, void *uc)
{
  /* Note: calling printf() from a signal handler is not
     strictly correct, since printf() is not async-signal-safe;
     see signal(7) */

  if ((si == NULL) || (si != uc))
  {
    printf("Caught signal %d\n", sig);
  }
}

void* pthread_test(void* args)
{
  pthread_mutex_t fakeMutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t fakeCond = PTHREAD_COND_INITIALIZER;

  pthread_cond_t monotonic_cond;
  pthread_condattr_t attr;

  struct timespec timeToWait, now;
  int rt;

  args = args; // silence compiler warning about unused argument

  clock_gettime(CLOCK_REALTIME, &now);
  timeToWait.tv_sec = now.tv_sec+1;
  timeToWait.tv_nsec = now.tv_nsec;

  printf("pthread_cond_timedwait: CLOCK_REALTIME test\n");
  printf("(Intentionally sleeping 1 second...)\n");
  fflush(stdout);

  pthread_mutex_lock(&fakeMutex);
  rt = pthread_cond_timedwait(&fakeCond, &fakeMutex, &timeToWait);
  if (rt != ETIMEDOUT)
  {
    printf("pthread_cond_timedwait failed\n");
    pthread_mutex_unlock(&fakeMutex);
    exit(EXIT_FAILURE);
  }
  pthread_mutex_unlock(&fakeMutex);


  pthread_condattr_init(&attr);
  pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
  pthread_cond_init(&monotonic_cond, &attr);

  clock_gettime(CLOCK_MONOTONIC, &now);
  timeToWait.tv_sec = now.tv_sec+1;
  timeToWait.tv_nsec = now.tv_nsec;

  printf("pthread_cond_timedwait: CLOCK_MONOTONIC test\n");
  printf("(Intentionally sleeping 1 second...)\n");
  printf("(If this test hangs for more than a few seconds, please report\n your glibc version and system details as FORCE_MONOTONIC_FIX\n issue at https://github.com/wolfcw/libfaketime)\n");
  fflush(stdout);

  pthread_mutex_lock(&fakeMutex);
  rt = pthread_cond_timedwait(&monotonic_cond, &fakeMutex, &timeToWait);
  if (rt != ETIMEDOUT)
  {
    printf("pthread_cond_timedwait failed\n");
    pthread_mutex_unlock(&fakeMutex);
    exit(EXIT_FAILURE);
  }
  pthread_mutex_unlock(&fakeMutex);

  pthread_cond_destroy(&monotonic_cond);

  return NULL;
}

#endif

int main (int argc, char **argv)
{
    time_t now;
    struct timeb tb;
    struct timeval tv;
#ifndef __APPLE__
    struct timespec ts;
    timer_t timerid1 = 0, timerid2;
    struct sigevent sev;
    struct itimerspec its;
    sigset_t mask;
    struct sigaction sa;
#endif
#ifndef __APPLE__
#ifdef FAKE_STAT
    struct stat buf;
#endif
#endif

/* silence compiler warnings */
printf("%s", 0 == 1 ? argv[0] : "");

#ifndef __APPLE__
    pthread_t thread;
    void *ret;

    pthread_create(&thread, NULL, pthread_test, NULL);
    pthread_join(thread, &ret);

    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGUSR1, &sa, NULL) == -1)
    {
      perror("sigaction");
      exit(EXIT_FAILURE);
    }

    /* Block timer signal temporarily */
    printf("Blocking signal %d\n", SIGUSR1);
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    if (sigprocmask(SIG_SETMASK, &mask, NULL) == -1)
    {
      perror("sigaction");
      exit(EXIT_FAILURE);
    }

    /* Create the timer */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGUSR1;
    sev.sigev_value.sival_ptr = &timerid1;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid1) == -1)
    {
      perror("timer_create");
      exit(EXIT_FAILURE);
    }

    /* Start timer1 */

    /* start timer ticking after one second */
    its.it_value.tv_sec = 1;
    its.it_value.tv_nsec = 0;
    /* fire in every 0.3 seconds */
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 300000000;

    if (timer_settime(timerid1, 0, &its, NULL) == -1)
    {
      perror("timer_settime");
      exit(EXIT_FAILURE);
    }

    sev.sigev_value.sival_ptr = &timerid2;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid2) == -1)
    {
      perror("timer_create");
      exit(EXIT_FAILURE);
    }

    /* Start timer2 */

    clock_gettime(CLOCK_REALTIME, &its.it_value);
    /* start timer ticking after one second */
    its.it_value.tv_sec += 3;
    /* fire once */
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    if (timer_settime(timerid2, TIMER_ABSTIME, &its, NULL) == -1)
    {
      perror("timer_settime");
      exit(EXIT_FAILURE);
    }
#endif

    time(&now);
    printf("time()         : Current date and time: %s", ctime(&now));
    printf("time(NULL)     : Seconds since Epoch  : %u\n", (unsigned int)time(NULL));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    ftime(&tb);
#pragma GCC diagnostic pop
    printf("ftime()        : Current date and time: %s", ctime(&tb.time));

    printf("(Intentionally sleeping 2 seconds...)\n");
    fflush(stdout);
    if (argc < 3)
    {
        sleep(1);
        usleep(1000000);
    }

    gettimeofday(&tv, NULL);
    printf("gettimeofday() : Current date and time: %s", ctime(&tv.tv_sec));

#ifndef __APPLE__
    if (sigprocmask(SIG_UNBLOCK, &mask, NULL) == -1)
    {
      perror("sigprocmask");
      exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_REALTIME, &ts);
    printf("clock_gettime(): Current date and time: %s", ctime(&ts.tv_sec));

    int timer_getoverrun_timerid1 = timer_getoverrun(timerid1);
    if (timer_getoverrun_timerid1 != 3)
    {
#ifdef __GNU__
        printf("(Timer overruns are assumed to be fine on Hurd)\n");
#else
        printf("timer_getoverrun(timerid1) FAILED, must be 3 but got: %d\n", timer_getoverrun_timerid1);
#endif
    }

    timer_gettime(timerid1, &its);
    if (VERBOSE == 1)
    {
        printf("timer_gettime(timerid1, &its); its = {{%ld, %ld}, {%ld, %ld}}}\n",
                (long)its.it_interval.tv_sec, (long)its.it_interval.tv_nsec,
                (long)its.it_value.tv_sec, (long)its.it_value.tv_nsec);
    }

    int timer_getoverrun_timerid2 = timer_getoverrun(timerid2);
    if (timer_getoverrun_timerid2 != 0)
    {
        printf("timer_getoverrun(timerid2) FAILED, must be 0 but got: %d\n", timer_getoverrun_timerid2);
    }

    timer_gettime(timerid2, &its);
    if (VERBOSE == 1)
    {
        printf("timer_gettime(timerid2, &its); its = {{%ld, %ld}, {%ld, %ld}}}\n",
            (long)its.it_interval.tv_sec, (long)its.it_interval.tv_nsec,
            (long)its.it_value.tv_sec, (long)its.it_value.tv_nsec);
    }
#endif

#ifndef __APPLE__
#ifdef FAKE_STAT
    lstat(argv[0], &buf);
    printf("stat(): mod. time of file '%s': %s", argv[0], ctime(&buf.st_mtime));
#endif
#endif

    return 0;
}

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
