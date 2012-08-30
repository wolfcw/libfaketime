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

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
#include <sys/timeb.h>

#ifdef FAKE_STAT
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif


int main (int argc, char **argv) {

    time_t now;
    struct timeb tb;
    struct timeval tv;
#ifndef __APPLE__
    struct timespec ts;
#endif
#ifdef FAKE_STAT
    struct stat buf;
#endif

    time(&now);
    printf("time()         : Current date and time: %s", ctime(&now));
    printf("time(NULL)     : Seconds since Epoch  : %u\n", (unsigned int)time(NULL));

    ftime(&tb);
    printf("ftime()        : Current date and time: %s", ctime(&tb.time));

    printf("(Intentionally sleeping 2 seconds...)\n");
    fflush(stdout);
    sleep(2);

    gettimeofday(&tv, NULL);
    printf("gettimeofday() : Current date and time: %s", ctime(&tv.tv_sec));

#ifndef __APPLE__
    clock_gettime(CLOCK_REALTIME, &ts);
    printf("clock_gettime(): Current date and time: %s", ctime(&ts.tv_sec));
#endif

#ifdef FAKE_STAT
    lstat(argv[0], &buf);
    printf("stat(): mod. time of file '%s': %s", argv[0], ctime(&buf.st_mtime));
#endif

    return 0;
}
