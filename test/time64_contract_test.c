#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

int main(void)
{
    struct stat st;
    struct timespec ts;
    time_t now;

    now = time(NULL);
    if (getenv("FAKETIME_EXPECT_POST2033") != NULL && now < (time_t)2000000000)
    {
        fprintf(stderr, "time_t did not represent a post-2033 timestamp\n");
        return 1;
    }

    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
    {
        perror("clock_gettime");
        return 1;
    }
    if (getenv("FAKETIME_EXPECT_POST2033") != NULL && ts.tv_sec < (time_t)2000000000)
    {
        fprintf(stderr, "clock_gettime did not represent a post-2033 timestamp\n");
        return 1;
    }

    if (stat(".", &st) != 0)
    {
        perror("stat");
        return 1;
    }

    printf("time64 contract passed: time=%lld clock=%lld mtime=%lld\n",
           (long long)now, (long long)ts.tv_sec, (long long)st.st_mtime);
    return 0;
}
