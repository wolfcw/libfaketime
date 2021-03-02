#include "snippets/include_headers.h"
#define where "libSNIPPET_NAME"
void SNIPPET_NAME_as_needed() {
  printf("  called SNIPPET_NAME_as_needed() \n");
}
static __attribute__((constructor)) void init_SNIPPET_NAME() {
#include "snippets/SNIPPET_NAME.c"
}
