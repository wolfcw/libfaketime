#include "snippets/include_headers.h"
#define where "library"
void FUNC_NAME_as_needed() {
  printf("  called FUNC_NAME_as_needed() \n");
}
static __attribute__((constructor)) void init_FUNC_NAME() {
#include "snippets/FUNC_NAME.c"
}
