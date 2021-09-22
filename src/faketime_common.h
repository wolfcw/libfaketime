/*
 * Faketime's common definitions
 *
 * Copyright 2013 Balint Reczey <balint@balintreczey.hu>
 *
 * This file is part of the libfaketime.
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

#ifndef FAKETIME_COMMON_H
#define FAKETIME_COMMON_H

#include <stdint.h>

struct system_time_s
{
  /* System time according to CLOCK_REALTIME */
  struct timespec real;
  /* System time according to CLOCK_MONOTONIC */
  struct timespec mon;
  /* System time according to CLOCK_MONOTONIC_RAW */
  struct timespec mon_raw;
#ifdef CLOCK_BOOTTIME
  /* System time according to CLOCK_BOOTTIME */
  struct timespec boot;
#endif
};

/* Data shared among faketime-spawned processes */
struct ft_shared_s
{
  /*
   * When advancing time linearly with each time(), etc. call, the calls are
   * counted here */
  uint64_t ticks;
  /* Index of timestamp to be loaded from file */
  uint64_t file_idx;
  /* System time Faketime started at */
  struct system_time_s start_time;
};

/* These are all needed in order to properly build on OSX */
#ifdef __APPLE__
#include <mach/clock.h>
#include <mach/mach_host.h>
#include <mach/mach_port.h>
#endif

/*
   Variadic Argument Re-packing

   Functions with variadic arguments typically have most arguments
   passed on the stack, but it varies across ABIs.

   C specifies that variadic arguments that are smaller than some
   standard promotion size are promoted to "int or larger".  If your
   platform's ABI only promotes to "int" and not "long" (and "int" and
   "long" differ on your platform), you should probably add
   -Dvariadic_promotion_t=int to CFLAGS.

   Note that some ABIs do not put all the variadic arguments on the
   stack.  For example, x86-64 puts float and double variadic
   arguments into floating point registers, according to
   https://www.uclibc.org/docs/psABI-x86_64.pdf

   The only variadic function faketime cares about intercepting is
   syscall.  But we don't believe that any syscalls expect float or
   double arguments, so we hope all the rest will be on the stack.
   tests/variadic/ attempts to confirm this if you are compiling
   with -DINTERCEPT_SYSCALL.

   If libc were capable of exposing a variadic form of syscall, we
   could depend on that and drop this approach, which would be
   preferable: https://sourceware.org/bugzilla/show_bug.cgi?id=27508
*/
#ifndef variadic_promotion_t
#define variadic_promotion_t long
#endif

/*
   The Linux kernel appears to have baked-in 6 as the maximum number
   of arguments for a syscall beyond the syscall number itself.
*/
#ifndef syscall_max_args
#define syscall_max_args 6
#endif

#endif
