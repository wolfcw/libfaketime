/*
 *  This file is part of libfaketime, version 0.9.13
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

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ft_sem.h"

#if FT_SEMAPHORE_BACKEND == FT_SYSV
#include <sys/sem.h>
#endif

#if FT_SEMAPHORE_BACKEND == FT_FLOCK
#include <sys/file.h>
#endif

/*
 *      =======================================================================
 *      Semaphore related functions                                     === SEM
 *      =======================================================================
 */

#if FT_SEMAPHORE_BACKEND == FT_SYSV
int ft_sem_name2key(char *name)
{
  key_t key;
  char fullname[256];
  snprintf(fullname, sizeof(fullname), "/tmp%s", name);
  fullname[sizeof(fullname) - 1] = '\0';
  int fd = open(fullname, O_CREAT, S_IRUSR | S_IWUSR);
  if (fd < 0)
  {
    perror("libfaketime: open");
    return -1;
  }
  close(fd);
  if (-1 == (key = ftok(fullname, 'F')))
  {
    perror("libfaketime: ftok");
    return -1;
  }
  return key;
}
#endif

#if FT_SEMAPHORE_BACKEND == FT_FLOCK
static int ft_sem_name_to_path(const char *name, char *path, size_t pathlen)
{
  const char *prefix = "/faketime_sem_";
  const char *p = strstr(name, prefix);
  if (p == NULL)
  {
    return -1;
  }
  const char *pid_str = p + strlen(prefix);
  const char *tmpdir;
#ifdef __ANDROID__
  tmpdir = getenv("TMPDIR");
  if (tmpdir == NULL) tmpdir = "/data/local/tmp";
#elif defined(__APPLE__)
  tmpdir = getenv("TMPDIR");
  if (tmpdir == NULL) tmpdir = "/tmp";
#else
  tmpdir = "/dev/shm";
#endif
  snprintf(path, pathlen, "%s/faketime_lock_%s", tmpdir, pid_str);
  path[pathlen - 1] = '\0';
  return 0;
}

static int ft_sem_init_flock(ft_sem_t *ft_sem)
{
  int ret = pthread_mutex_init(&ft_sem->local_lock, NULL);

  if (ret != 0)
  {
    errno = ret;
    return -1;
  }
  ft_sem->fd = -1;
  ft_sem->lock_dev = 0;
  ft_sem->lock_ino = 0;
  return 0;
}

static bool ft_sem_fd_matches_lock(const ft_sem_t *ft_sem)
{
  struct stat st;

  return ft_sem->fd >= 0 && fstat(ft_sem->fd, &st) == 0 &&
    st.st_dev == ft_sem->lock_dev && st.st_ino == ft_sem->lock_ino;
}
#endif

int ft_sem_create(char *name, ft_sem_t *ft_sem)
{
#if FT_SEMAPHORE_BACKEND == FT_POSIX
  if (SEM_FAILED == (ft_sem->sem = sem_open(name, O_CREAT|O_EXCL, S_IWUSR|S_IRUSR, 1)))
  {
    return -1;
  }
#elif FT_SEMAPHORE_BACKEND == FT_SYSV
  key_t key = ft_sem_name2key(name);
  int nsems = 1;

  ft_sem->semid = semget(key, nsems, IPC_CREAT | IPC_EXCL | S_IWUSR | S_IRUSR);
  if (ft_sem->semid >= 0) { /* we got here first */
    struct sembuf sb = {
      .sem_num = 0,
      .sem_op = 1, /* number of resources the semaphore has (when decremented down to 0 the semaphore will block) */
      .sem_flg = 0,
    };
    /* the number of semaphore operation structures (struct sembuf) passed to semop */
    int nsops = 1;

    /* do a semop() to "unlock" the semaphore */
    if (-1 == semop(ft_sem->semid, &sb, nsops)) {
      return -1;
    }
  } else if (errno == EEXIST) { /* someone else got here before us */
    return -1;
  } else {
    return -1;
  }
#elif FT_SEMAPHORE_BACKEND == FT_FLOCK
  char path[256];
  int fd;
  if (ft_sem_name_to_path(name, path, sizeof(path)) < 0)
  {
    return -1;
  }
  fd = open(path, O_CREAT|O_EXCL|O_RDWR, S_IWUSR|S_IRUSR);
  if (fd < 0)
  {
    return -1;
  }
  if (close(fd) == -1)
  {
    return -1;
  }
  if (ft_sem_init_flock(ft_sem) == -1)
  {
    (void)unlink(path);
    return -1;
  }
#endif
  strncpy(ft_sem->name, name, sizeof ft_sem->name - 1);
  ft_sem->name[sizeof ft_sem->name - 1] = '\0';
  return 0;
}

int ft_sem_open(char *name, ft_sem_t *ft_sem)
{
#if FT_SEMAPHORE_BACKEND == FT_POSIX
  if (SEM_FAILED == (ft_sem->sem = sem_open(name, 0)))
  {
    return -1;
  }
#elif FT_SEMAPHORE_BACKEND == FT_SYSV
  key_t key = ft_sem_name2key(name);
  ft_sem->semid = semget(key, 1, S_IWUSR | S_IRUSR);
  if (ft_sem->semid < 0) {
    return -1;
   }
#elif FT_SEMAPHORE_BACKEND == FT_FLOCK
  char path[256];
  int fd;
  if (ft_sem_name_to_path(name, path, sizeof(path)) < 0)
  {
    return -1;
  }
  fd = open(path, O_RDWR);
  if (fd < 0)
  {
    return -1;
  }
  if (close(fd) == -1)
  {
    return -1;
  }
  if (ft_sem_init_flock(ft_sem) == -1)
  {
    return -1;
  }
#endif
  strncpy(ft_sem->name, name, sizeof ft_sem->name - 1);
  ft_sem->name[sizeof ft_sem->name - 1] = '\0';
  return 0;
}

int ft_sem_lock(ft_sem_t *ft_sem)
{
#if FT_SEMAPHORE_BACKEND == FT_POSIX
  return sem_wait(ft_sem->sem);
#elif FT_SEMAPHORE_BACKEND == FT_SYSV
  struct sembuf sb = {
    .sem_num = 0,
    .sem_op = -1,  /* allocate resource (lock) */
    .sem_flg = 0,
  };
  return semop(ft_sem->semid, &sb, 1);
#elif FT_SEMAPHORE_BACKEND == FT_FLOCK
  char path[256];
  struct stat st;
  int fd;
  int ret;

  ret = pthread_mutex_lock(&ft_sem->local_lock);
  if (ret != 0)
  {
    errno = ret;
    return -1;
  }
  if (ft_sem_name_to_path(ft_sem->name, path, sizeof(path)) < 0)
  {
    (void)pthread_mutex_unlock(&ft_sem->local_lock);
    return -1;
  }
  fd = open(path, O_RDWR);
  if (fd < 0 || flock(fd, LOCK_EX) == -1 || fstat(fd, &st) == -1)
  {
    int saved_errno = errno;
    if (fd >= 0)
      (void)close(fd);
    (void)pthread_mutex_unlock(&ft_sem->local_lock);
    errno = saved_errno;
    return -1;
  }
  ft_sem->fd = fd;
  ft_sem->lock_dev = st.st_dev;
  ft_sem->lock_ino = st.st_ino;
  return 0;
#endif
}

int ft_sem_unlock(ft_sem_t *ft_sem)
{
#if FT_SEMAPHORE_BACKEND == FT_POSIX
  return sem_post(ft_sem->sem);
#elif FT_SEMAPHORE_BACKEND == FT_SYSV
  struct sembuf sb = {
    .sem_num = 0,
    .sem_op = 1,  /* free resource (unlock) */
    .sem_flg = 0,
  };
  return semop(ft_sem->semid, &sb, 1);
#elif FT_SEMAPHORE_BACKEND == FT_FLOCK
  int saved_errno = 0;
  int ret;

  /* Applications may close all inherited descriptors or reuse this descriptor
   * number.  A closed flock descriptor releases its lock automatically; never
   * operate on a descriptor unless it is still the lock file we opened. */
  if (ft_sem_fd_matches_lock(ft_sem))
  {
    if (flock(ft_sem->fd, LOCK_UN) == -1)
      saved_errno = errno;
    if (close(ft_sem->fd) == -1 && saved_errno == 0)
      saved_errno = errno;
  }
  ft_sem->fd = -1;
  ret = pthread_mutex_unlock(&ft_sem->local_lock);
  if (ret != 0 && saved_errno == 0)
    saved_errno = ret;
  if (saved_errno != 0)
  {
    errno = saved_errno;
    return -1;
  }
  return 0;
#endif
}

int ft_sem_close(ft_sem_t *ft_sem)
{
#if FT_SEMAPHORE_BACKEND == FT_POSIX
  return sem_close(ft_sem->sem);
#elif FT_SEMAPHORE_BACKEND == FT_SYSV
  /* NOP -- there's no "close" equivalent */
  (void)ft_sem;
  return 0;
#elif FT_SEMAPHORE_BACKEND == FT_FLOCK
  int ret = 0;

  if (ft_sem_fd_matches_lock(ft_sem) && close(ft_sem->fd) == -1)
    ret = -1;
  ft_sem->fd = -1;
  if (pthread_mutex_destroy(&ft_sem->local_lock) != 0)
    ret = -1;
  return ret;
#endif
}

int ft_sem_unlink(ft_sem_t *ft_sem)
{
#if FT_SEMAPHORE_BACKEND == FT_POSIX
  return sem_unlink(ft_sem->name);
#elif FT_SEMAPHORE_BACKEND == FT_SYSV
  return semctl(ft_sem->semid, 0, IPC_RMID);
#elif FT_SEMAPHORE_BACKEND == FT_FLOCK
  char path[256];
  if (ft_sem_name_to_path(ft_sem->name, path, sizeof(path)) < 0)
  {
    return -1;
  }
  return unlink(path);
#endif
}
