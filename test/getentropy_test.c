#include <unistd.h>
#include <stdio.h>

int main() {
  unsigned char buf[16];
  if (getentropy(buf, sizeof(buf))) {
    perror("failed to getentropy()");
    return 1;
  }
  for (size_t i = 0; i < sizeof(buf); i++)
    printf("%02x", buf[i]);
  printf("\n");
  return 0;
}
