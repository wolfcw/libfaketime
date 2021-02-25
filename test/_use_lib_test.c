#include "snippets/include_headers.h"
extern void FUNC_NAME_as_needed();
#define where "program"
int main() {
  FUNC_NAME_as_needed();
#include "snippets/FUNC_NAME.c"
}
