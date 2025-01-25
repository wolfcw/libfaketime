/*
 *  Copyright (C) 2022 be.storaged GmbH
 *
 *  This file is part of libfaketime
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

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void print_msg(const char *msg) {
	size_t out;
	out = write(0, msg, strlen(msg));
	(void) out; /* unused */
}

static void* actual_malloc(size_t size) {
	/* We would like to use "the real malloc", but cannot. Thus, this
	 * implements a trivial, allocate-only bump allocator to make things
	 * work.
	 */
	static char memory_arena[16 << 20];
	static size_t allocated_index = 0;

	void *result = &memory_arena[allocated_index];

	allocated_index += size;
	/* align to a multiple of 8 bytes */
	allocated_index = (allocated_index + 7) / 8 * 8;

	if (allocated_index >= sizeof(memory_arena)) {
		print_msg("libmallocintercept is out of memory!");
		abort();
	}

	return result;
}

static void poke_faketime(void) {
#ifdef FAIL_PRE_INIT_CALLS
	/* To complicate things for libfaketime, this calls clock_gettime()
	 * while holding a lock. This should simulate problems that occurred
	 * with address sanitizer.
	 */
	static pthread_mutex_t time_mutex = PTHREAD_MUTEX_INITIALIZER;
	struct timespec timespec;

	pthread_mutex_lock(&time_mutex);
	clock_gettime(CLOCK_REALTIME, &timespec);
	pthread_mutex_unlock(&time_mutex);
#else
	print_msg("FAIL_PRE_INIT_CALLS not defined, skipping poke_faketime() ");
#endif
}

void *malloc(size_t size) {
	print_msg("Called malloc() from libmallocintercept...");
	poke_faketime();
	print_msg("successfully\n");
	return actual_malloc(size);
}

void free(void *ptr) {
  long int ptr2 = (long int) ptr; ptr2 -= (long int) ptr;
	print_msg("Called free() on from libmallocintercept...");
	poke_faketime();
	print_msg("successfully\n");

	/* We cannot actually free memory */
}
