#include <stdio.h>

int thefunc(int verbosity) {
  if (verbosity >= 0) return printf("called %s\n", __func__) <= 0;
  else return 0;
}
