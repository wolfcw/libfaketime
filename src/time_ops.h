/*
 * Time operation macros based on sys/time.h
 * Copyright 2013 Balint Reczey <balint@balintreczey.hu>
 *
 * This file is part of libfaketime.
 *
 * libfaketime is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License v2 as published by the Free
 * Software Foundation.
 *
 * libfaketime is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License v2 along
 * with libfaketime; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef TIME_OPS_H
#define TIME_OPS_H
#include <errno.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <time.h>

#define SEC_TO_uSEC 1000000
#define SEC_TO_nSEC 1000000000
#define FT_TIME_MAX(value) ((__typeof__(value))(((__typeof__(value))~0) ^ \
  ((__typeof__(value))1 << (sizeof(value) * CHAR_BIT - 1))))
#define FT_TIME_MIN(value) (-FT_TIME_MAX(value) - 1)

/* Convenience macros for operations on timevals.
   NOTE: `timercmp' does not work for >= or <=.  */
#define timerisset2(tvp, prefix) ((tvp)->tv_sec || (tvp)->tv_##prefix##sec)
#define timerclear2(tvp, prefix) ((tvp)->tv_sec = (tvp)->tv_##prefix##sec = 0)
#define timercmp2(a, b, CMP, prefix)                                \
  (((a)->tv_sec == (b)->tv_sec) ?                                   \
   ((a)->tv_##prefix##sec CMP (b)->tv_##prefix##sec) :              \
   ((a)->tv_sec CMP (b)->tv_sec))
#define timeradd2(a, b, result, prefix)                             \
  do                                                                \
  {                                                                 \
    __int128 _ft_sec = (__int128)(a)->tv_sec + (__int128)(b)->tv_sec; \
    __int128 _ft_subsec = (__int128)(a)->tv_##prefix##sec +          \
      (__int128)(b)->tv_##prefix##sec;                               \
    if (_ft_subsec >= SEC_TO_##prefix##SEC)                          \
    {                                                                 \
      _ft_sec++;                                                       \
      _ft_subsec -= SEC_TO_##prefix##SEC;                             \
    }                                                                 \
    else if (_ft_subsec < 0)                                          \
    {                                                                 \
      _ft_sec--;                                                       \
      _ft_subsec += SEC_TO_##prefix##SEC;                             \
    }                                                                 \
    if (_ft_sec > (__int128)FT_TIME_MAX((result)->tv_sec))            \
    {                                                                 \
      errno = EOVERFLOW;                                               \
      (result)->tv_sec = FT_TIME_MAX((result)->tv_sec);                \
      (result)->tv_##prefix##sec = SEC_TO_##prefix##SEC - 1;          \
    }                                                                 \
    else if (_ft_sec < (__int128)FT_TIME_MIN((result)->tv_sec))       \
    {                                                                 \
      errno = EOVERFLOW;                                               \
      (result)->tv_sec = FT_TIME_MIN((result)->tv_sec);                \
      (result)->tv_##prefix##sec = 0;                                 \
    }                                                                 \
    else                                                              \
    {                                                                 \
      (result)->tv_sec = (__typeof__((result)->tv_sec))_ft_sec;        \
      (result)->tv_##prefix##sec = (__typeof__((result)->tv_##prefix##sec))_ft_subsec; \
    }                                                                 \
  } while (0)
#define timersub2(a, b, result, prefix)                             \
  do                                                                \
  {                                                                 \
    __int128 _ft_sec = (__int128)(a)->tv_sec - (__int128)(b)->tv_sec; \
    __int128 _ft_subsec = (__int128)(a)->tv_##prefix##sec -           \
      (__int128)(b)->tv_##prefix##sec;                               \
    if (_ft_subsec < 0)                                               \
    {                                                               \
      _ft_sec--;                                                       \
      _ft_subsec += SEC_TO_##prefix##SEC;                             \
    }                                                               \
    else if (_ft_subsec >= SEC_TO_##prefix##SEC)                      \
    {                                                                 \
      _ft_sec++;                                                       \
      _ft_subsec -= SEC_TO_##prefix##SEC;                             \
    }                                                                 \
    if (_ft_sec > (__int128)FT_TIME_MAX((result)->tv_sec) ||          \
        _ft_sec < (__int128)FT_TIME_MIN((result)->tv_sec))            \
    {                                                                 \
      errno = EOVERFLOW;                                               \
      (result)->tv_sec = (_ft_sec > 0) ? FT_TIME_MAX((result)->tv_sec) : \
        FT_TIME_MIN((result)->tv_sec);                                 \
      (result)->tv_##prefix##sec = (_ft_sec > 0) ?                    \
        SEC_TO_##prefix##SEC - 1 : 0;                                 \
    }                                                                 \
    else                                                              \
    {                                                                 \
      (result)->tv_sec = (__typeof__((result)->tv_sec))_ft_sec;        \
      (result)->tv_##prefix##sec = (__typeof__((result)->tv_##prefix##sec))_ft_subsec; \
    }                                                                 \
  } while (0)
#define timermul2(tvp, c, result, prefix)                           \
  do                                                                \
  {                                                                 \
    long double _ft_total =                                          \
      ((long double)(tvp)->tv_sec * SEC_TO_##prefix##SEC +            \
       (long double)(tvp)->tv_##prefix##sec) * (long double)(c);      \
    long double _ft_max_total =                                    \
      (long double)FT_TIME_MAX((result)->tv_sec) * SEC_TO_##prefix##SEC + \
      (SEC_TO_##prefix##SEC - 1);                                    \
    long double _ft_min_total =                                    \
      (long double)FT_TIME_MIN((result)->tv_sec) * SEC_TO_##prefix##SEC; \
    if (!isfinite(_ft_total) || _ft_total > _ft_max_total ||          \
        _ft_total < _ft_min_total)                                    \
    {                                                                 \
      errno = EOVERFLOW;                                               \
      _ft_total = (_ft_total < 0) ? _ft_min_total : _ft_max_total;    \
    }                                                                 \
    __int128 _ft_time = (__int128)_ft_total;                          \
    (result)->tv_##prefix##sec = _ft_time % SEC_TO_##prefix##SEC;     \
    (result)->tv_sec = (_ft_time - (result)->tv_##prefix##sec) /      \
      SEC_TO_##prefix##SEC;                                           \
    if ((result)->tv_##prefix##sec < 0)                               \
    {                                                               \
      (result)->tv_##prefix##sec +=  SEC_TO_##prefix##SEC;          \
      (result)->tv_sec -= 1;                                        \
    }                                                               \
  } while (0)

/* ops for microsecs */
#ifndef timerisset
#define timerisset(tvp) timerisset2(tvp,u)
#endif
#ifndef timerclear
#define timerclear(tvp) timerclear2(tvp, u)
#endif
#ifndef timercmp
#define timercmp(a, b, CMP) timercmp2(a, b, CMP, u)
#endif
#ifndef timeradd
#define timeradd(a, b, result) timeradd2(a, b, result, u)
#endif
#ifndef timersub
#define timersub(a, b, result) timersub2(a, b, result, u)
#endif
#ifndef timermul
#define timermul(a, c, result) timermul2(a, c, result, u)
#endif

/* ops for nanosecs */
#ifndef timespecisset
#define timespecisset(tvp) timerisset2(tvp,n)
#endif
#ifndef timespecclear
#define timespecclear(tvp) timerclear2(tvp, n)
#endif
#ifndef timespeccmp
#define timespeccmp(a, b, CMP) timercmp2(a, b, CMP, n)
#endif
#ifndef timespecadd
#define timespecadd(a, b, result) timeradd2(a, b, result, n)
#endif
#ifndef timespecsub
#define timespecsub(a, b, result) timersub2(a, b, result, n)
#endif
#ifndef timespecmul
#define timespecmul(a, c, result) timermul2(a, c, result, n)
#endif

#endif
