#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#include "../src/faketime_common.h"

static int failures = 0;

static void check_offset(const char *field, size_t actual, size_t expected)
{
  if (actual != expected)
  {
    fprintf(stderr, "FAIL: offsetof(ft_shared_s, %s) = %zu, expected %zu\n",
            field, actual, expected);
    failures++;
  }
  else
  {
    printf("OK: offsetof(ft_shared_s, %s) = %zu\n", field, actual);
  }
}

static void check_size(const char *name, size_t actual, size_t expected)
{
  if (actual != expected)
  {
    fprintf(stderr, "FAIL: sizeof(%s) = %zu, expected %zu\n",
            name, actual, expected);
    failures++;
  }
  else
  {
    printf("OK: sizeof(%s) = %zu\n", name, actual);
  }
}

int main()
{
  /* ft_shared_time_s must be exactly 16 bytes: two int64_t */
  check_size("ft_shared_time_s", sizeof(struct ft_shared_time_s), 16);

  /* Field offsets in ft_shared_s */
  check_offset("magic", offsetof(struct ft_shared_s, magic), 0);
  check_offset("version", offsetof(struct ft_shared_s, version), 4);
  check_offset("size", offsetof(struct ft_shared_s, size), 6);
  check_offset("ticks", offsetof(struct ft_shared_s, ticks), 8);
  check_offset("file_idx", offsetof(struct ft_shared_s, file_idx), 16);
  check_offset("start_time_real", offsetof(struct ft_shared_s, start_time_real), 24);
  check_offset("start_time_mon", offsetof(struct ft_shared_s, start_time_mon), 40);
  check_offset("start_time_mon_raw", offsetof(struct ft_shared_s, start_time_mon_raw), 56);
#ifdef CLOCK_BOOTTIME
  check_offset("start_time_boot", offsetof(struct ft_shared_s, start_time_boot), 72);
  check_size("ft_shared_s", sizeof(struct ft_shared_s), 88);
#else
  check_size("ft_shared_s", sizeof(struct ft_shared_s), 72);
#endif

  struct ft_shared_s shared = {0};
  shared.magic = FT_SHARED_MAGIC;
  shared.version = FT_SHARED_VERSION;
  shared.size = sizeof(shared);
  shared.start_time_real.nsec = 999999999;
  shared.start_time_mon.nsec = 0;
  shared.start_time_mon_raw.nsec = 1;
#ifdef CLOCK_BOOTTIME
  shared.start_time_boot.nsec = 2;
#endif
  if (!ft_shared_header_valid(&shared))
  {
    fprintf(stderr, "FAIL: valid shared-memory header was rejected\n");
    failures++;
  }
  shared.start_time_real.nsec = -2;
  if (ft_shared_header_valid(&shared))
  {
    fprintf(stderr, "FAIL: out-of-range shared-memory nanoseconds accepted\n");
    failures++;
  }

  if (failures > 0)
  {
    fprintf(stderr, "%d layout check(s) failed\n", failures);
    return EXIT_FAILURE;
  }

  printf("All layout checks passed\n");
  return EXIT_SUCCESS;
}
