#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__
#include <sys/random.h>
#else
#include <sys/random.h>
#endif

int main(void)
{
  unsigned char buffer[16];

#ifdef __APPLE__
  if (getentropy(buffer, sizeof(buffer)) != 0)
#else
  if (getrandom(buffer, sizeof(buffer), 0) != (ssize_t)sizeof(buffer))
#endif
  {
    return EXIT_FAILURE;
  }
  for (size_t index = 0; index < sizeof(buffer); index++)
  {
    printf("%02x", buffer[index]);
  }
  putchar('\n');
  return EXIT_SUCCESS;
}
