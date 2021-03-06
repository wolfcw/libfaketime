#include <stdarg.h>
#include <time.h>
#include "../../src/faketime_common.h"

extern long inner0(char *out, ...);
extern long inner1(char *out, ...);

long outer(long num, char *out, ...) {
  va_list ap;
  va_start(ap, out);
  variadic_promotion_t a[syscall_max_args];
  for (int i = 0; i < syscall_max_args; i++)
    a[i] = va_arg(ap, variadic_promotion_t);
  va_end(ap);

  if (num == 0)
    return inner0(out, a[0], a[1], a[2], a[3], a[4], a[5]);
  if (num == 1)
    return inner1(out, a[0], a[1], a[2], a[3], a[4], a[5]);
  else return -1;
}
