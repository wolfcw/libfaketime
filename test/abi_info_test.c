#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

int main(void)
{
    printf("pointer_bits=%zu\n", sizeof(void *) * 8);
    printf("time_t_bits=%zu\n", sizeof(time_t) * 8);
    printf("timespec_sec_bits=%zu\n", sizeof(((struct timespec *)0)->tv_sec) * 8);
    printf("off_t_bits=%zu\n", sizeof(off_t) * 8);
    printf("stat_size=%zu\n", sizeof(struct stat));
    printf("stat_mtime_bits=%zu\n", sizeof(((struct stat *)0)->st_mtime) * 8);

#ifdef __GLIBC__
    printf("libc=glibc\n");
    printf("glibc_version=%d.%d\n", __GLIBC__, __GLIBC_MINOR__);
#elif defined(__MUSL__)
    printf("libc=musl\n");
#else
    printf("libc=unknown\n");
#endif

#ifdef __TIMESIZE
    printf("__TIMESIZE=%d\n", __TIMESIZE);
#else
    printf("__TIMESIZE=undefined\n");
#endif

#ifdef _TIME_BITS
    printf("_TIME_BITS=%d\n", _TIME_BITS);
#else
    printf("_TIME_BITS=undefined\n");
#endif

#ifdef __USE_TIME_BITS64
    printf("__USE_TIME_BITS64=1\n");
#else
    printf("__USE_TIME_BITS64=0\n");
#endif

    return 0;
}
