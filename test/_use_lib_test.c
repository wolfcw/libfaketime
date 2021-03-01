#include "snippets/include_headers.h"
extern void SNIPPET_NAME_as_needed();
#define where "program"
int main() {
  SNIPPET_NAME_as_needed();
#include "snippets/SNIPPET_NAME.c"
}
