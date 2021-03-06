
#include <stdio.h>
#include <wchar.h>
#include <stdarg.h>
#include <stddef.h>

/*   round 0:  c, s, wc, i, wi */
long inner0(char *out, ...) {
  char c = 0;
  short s = 0;
  wchar_t wc = 0;
  int i = 0;
  wint_t wi = 0;

  va_list ap;
  va_start(ap, out);
  c = va_arg(ap, int);
  s = va_arg(ap, int);
  wc = va_arg(ap, typeof(wc));
  i = va_arg(ap, typeof(i));
  wi = va_arg(ap, typeof(wi));
  va_end(ap);

  int ret = sprintf(out, "c: 0x%x s: 0x%x wc: 0x%lx i: 0x%x wi: 0x%x\n", c, s, (long)wc, i, wi);
  return ret;
}
/*   round 1: l, ll, ptr, pd, sz */
long inner1(char *out, ...) {
  long l = 0;
  long long ll = 0;
  void *ptr = NULL;
  ptrdiff_t pd = 0;
  size_t sz = 0;

  va_list ap;
  va_start(ap, out);
  l = va_arg(ap, typeof(l));
  ll = va_arg(ap, typeof(ll));
  ptr = va_arg(ap, typeof(ptr));
  pd = va_arg(ap, typeof(pd));
  sz = va_arg(ap, typeof(sz));
  va_end(ap);

  int ret = sprintf(out, "l: 0x%lx ll: 0x%llx ptr: %p pd: 0x%tx sz: 0x%zx\n", l, ll, ptr, pd, sz);
  return ret;
}
