#include <stdio.h>

int thefunc(void) {
  return printf("called %s\n", __func__) <= 0;
}
