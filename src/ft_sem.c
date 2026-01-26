/*
 *  This file is part of libfaketime, version 0.9.12
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
  snprintf(path, pathlen, "/dev/shm/faketime_lock_%s", pid_str);
  path[pathlen - 1] = '\0';
  return 0;
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
  if (ft_sem_name_to_path(name, path, sizeof(path)) < 0)
  {
    return -1;
  }
  ft_sem->fd = open(path, O_CREAT|O_EXCL|O_RDWR, S_IWUSR|S_IRUSR);
  if (ft_sem->fd < 0)
  {
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
  if (ft_sem_name_to_path(name, path, sizeof(path)) < 0)
  {
    return -1;
  }
  ft_sem->fd = open(path, O_RDWR);
  if (ft_sem->fd < 0)
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
  return flock(ft_sem->fd, LOCK_EX);
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
  return flock(ft_sem->fd, LOCK_UN);
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
  int ret = close(ft_sem->fd);
  ft_sem->fd = -1;
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
