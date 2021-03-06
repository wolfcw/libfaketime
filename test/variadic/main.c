#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <wchar.h>

extern long outer(long num, char *out, ...);
extern long inner0(char *out, ...);
extern long inner1(char *out, ...);

#define bufsize 2048

static int compare_buffers(int round,
                            long ret_outer, long ret_inner,
                            const char* outer, const char* inner) {
  int ret = 0;
  if (ret_outer != ret_inner) {
    printf("Round %d: return values differ (outer: %ld inner: %ld)\n", round, ret_outer, ret_inner);
    ret++;
  }
  if (memcmp(outer, inner, bufsize)) {
    printf("Round %d strings differ:\n - outer: %s\n - inner: %s\n", round, outer, inner);
    ret++;
  }
  if (ret == 0)
    printf("Round %d success: %s\n", round, outer);
  return ret;
}

int main() {
/* sizes of intrinsic types as reported by echo | cpp -dM | grep
   SIZEOF, pruned to avoid floating point types.  Should work with
   both clang and gcc, not sure about other C preprocessors.

   Note that we set bits in every high octet and every low octet to
   see that they end up in the right spot.
 */
  char c =             0x03L;
  short s =           (0x04L << ((__SIZEOF_SHORT__ - 1) * 8))     + 0xff;
  wchar_t wc =        (0x05L << ((__SIZEOF_WCHAR_T__ - 1) * 8))   + 0xfe;
  int i =             (0x06L << ((__SIZEOF_INT__  - 1) * 8))      + 0xfd;
  wint_t wi =         (0x07L << ((__SIZEOF_WINT_T__ - 1) * 8))    + 0xfc;
  long l =            (0x08L << ((__SIZEOF_LONG__ - 1) * 8) )     + 0xfb;
  long long ll =      (0x09LL << ((__SIZEOF_LONG_LONG__ - 1) * 8)) + 0xfa;
  void *ptr = (void*)((0x0aL << ((__SIZEOF_POINTER__ - 1) * 8))   + 0xf9);
  ptrdiff_t pd =      (0x0bL << ((__SIZEOF_PTRDIFF_T__ -1) * 8))  + 0xf9;
  size_t sz =         (0x0cL << ((__SIZEOF_SIZE_T__ - 1) * 8))    + 0xf8;

  char *buf[2];
  for (int j = 0; j < 2; j++)
    buf[j] = malloc(bufsize);

  int ret[2];
  int errors = 0;

#define reset_buffers(n) for (int j = 0; j < 2; j++) memset(buf[j], n, bufsize)
#define check_buffers(n) errors += compare_buffers(n, ret[0], ret[1], buf[0], buf[1])

  reset_buffers(0);
  ret[0] = outer(0, buf[0], c, s, wc, i, wi);
  ret[1] = inner0(buf[1], c, s, wc, i, wi);
  check_buffers(0);

  reset_buffers(1);
  ret[0] = outer(1, buf[0], l, ll, ptr, pd, sz);
  ret[1] = inner1(buf[1], l, ll, ptr, pd, sz);
  check_buffers(1);

  return (int)errors;
}
