/*
This file implements custom C string streams for read-only collective file system access.  For more information about custom C strings, see:

http://www.gnu.org/software/libc/manual/html_node/Custom-Streams.html#Custom-Streams
*/

#define _GNU_SOURCE  // needed to expose cookie_io_functions_t
#include <errno.h>
#include <libio.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include "collfs-private.h"

typedef struct fcollfs_cookie_struct fcollfs_cookie_t;
struct fcollfs_cookie_struct
{
  int fd;
};


static ssize_t
fcollfs_read (void *cookie, char *b, size_t s)
{
  int fd;
  fcollfs_cookie_t *c;

  c = (fcollfs_cookie_t *) cookie;
  fd = c->fd;

  return __collfs_libc_read(fd, b, s);
}


static ssize_t
fcollfs_write (void *cookie __attribute__ ((unused)), const char *b __attribute__ ((unused)), size_t s __attribute__ ((unused)))
{
  // this is an error!
  return 0;
}


static int
fcollfs_seek (void *cookie, _IO_off64_t *p, int w)
{
  int fd;
  off_t new_p;
  fcollfs_cookie_t *c;

  c = (fcollfs_cookie_t *) cookie;
  fd = c->fd;

  new_p = __collfs_lseek(fd, *p, w);

  if (new_p == -1) {
    return -1;
  }

  *p = new_p;
  return 0;
}
  

static int
fcollfs_close (void *cookie)
{
  int fd;
  fcollfs_cookie_t *c;

  c = (fcollfs_cookie_t *) cookie;
  fd = c->fd;

  free (c);
  return __collfs_close(fd);
}

FILE *
fcollfs_open (const char *pathname, const char *mode)
{
  cookie_io_functions_t iof;
  fcollfs_cookie_t *c;

  if (mode[0] != 'r') 
    return NULL;

  c = (fcollfs_cookie_t *) malloc (sizeof (fcollfs_cookie_t));
  if (c == NULL)
    return NULL;

  c->fd = __collfs_open(pathname, O_RDONLY);

  iof.read = fcollfs_read;
  iof.write = fcollfs_write;
  iof.seek = fcollfs_seek;
  iof.close = fcollfs_close;

  return fopencookie (c, mode, iof);
}

