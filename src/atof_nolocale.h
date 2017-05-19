#include <stdlib.h>

/**
 * \brief Convert string to double indepedently on locale setting.
 */
static inline double atof_nolocale(const char *str)
{
  char *endp;
  double d;
  char buf[256];
  d = strtod(str, &endp);
  if (*endp == ',' || *endp == '.')
  {
    strncpy(buf, str, sizeof(buf));
    buf[endp-str] = *endp == '.' ? ',' : '.';
    d = strtod(str, NULL);
  }
  return d;
}


