#include "collfs.h"
#include "libc-collfs-private.h"
#include "thefunc.h"
#include <fcntl.h>
#include <stdio.h>

int thefunc(int verbosity) {
  if (verbosity >= 0) return printf("called %s\n", __func__) <= 0;
  return 0;
}

int thetest_fxstat64(const char *path) {
  struct stat64 st;
  int fd, err;
  fd = __collfs_open(path, O_RDONLY, 0);
  if (fd < 0) return -1;
  err = __collfs_fxstat64(_STAT_VER, fd, &st);
  if (err < 0) return -1;
  printf("%s (via __collfs_fxstat64): inode %ld size %ld\n", path, (long)st.st_ino, (long)st.st_size);
  return __collfs_close(fd);
}
