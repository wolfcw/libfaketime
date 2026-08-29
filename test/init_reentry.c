#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>

static int constructor_status;

static void __attribute__((constructor)) init_reentry(void)
{
  struct stat status;
  constructor_status = stat(".", &status);
  if (constructor_status != 0)
    _exit(EXIT_FAILURE);
}

int init_reentry_status(void)
{
  return constructor_status;
}
