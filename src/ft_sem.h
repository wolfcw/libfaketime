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

#ifndef FT_SEM_H
#define FT_SEM_H

/* semaphore backend options */
#define FT_POSIX 1
#define FT_SYSV  2
#define FT_FLOCK 3

/* set default backend */
#ifndef FT_SEMAPHORE_BACKEND
#define FT_SEMAPHORE_BACKEND FT_FLOCK
#endif

/* validate selected backend */
#if FT_SEMAPHORE_BACKEND == FT_POSIX
#elif FT_SEMAPHORE_BACKEND == FT_SYSV
#elif FT_SEMAPHORE_BACKEND == FT_FLOCK
#else
#error "Unknown FT_SEMAPHORE_BACKEND; select between FT_POSIX, FT_SYSV, and FT_FLOCK"
#endif

#if FT_SEMAPHORE_BACKEND == FT_POSIX
#include <semaphore.h>
#endif

typedef struct
{
  char name[256];
#if FT_SEMAPHORE_BACKEND == FT_POSIX
  sem_t *sem;
#elif FT_SEMAPHORE_BACKEND == FT_SYSV
  int semid;
#elif FT_SEMAPHORE_BACKEND == FT_FLOCK
  int fd;
#endif
} ft_sem_t;

int ft_sem_create(char *name, ft_sem_t *ft_sem);
int ft_sem_open(char *name, ft_sem_t *ft_sem);
int ft_sem_lock(ft_sem_t *ft_sem);
int ft_sem_unlock(ft_sem_t *ft_sem);
int ft_sem_close(ft_sem_t *ft_sem);
int ft_sem_unlink(ft_sem_t *ft_sem);

#endif /* FT_SEM_H */
