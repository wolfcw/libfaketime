#include <stdio.h>
#include <time.h>

int main(void)
{
  time_t now = time(NULL);
  struct tm local;

  if (now == (time_t)-1 || localtime_r(&now, &local) == NULL)
    return 1;
  printf("%04d-%02d-%02d %02d:%02d:%02d %d\n",
         local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
         local.tm_hour, local.tm_min, local.tm_sec, local.tm_isdst);
  return 0;
}
