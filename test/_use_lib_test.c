#include "snippets/include_headers.h"
extern void SNIPPET_NAME_as_needed();
#define where "use_lib_SNIPPET_NAME"
int main() {
  SNIPPET_NAME_as_needed();
#include "snippets/SNIPPET_NAME.c"
}
