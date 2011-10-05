/* Handle loading and unloading shared objects for internal libc purposes.
   Copyright (C) 1999,2000,2001,2002,2004,2005 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Zack Weinberg <zack@rabi.columbia.edu>, 1999.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <dlfcn.h>
#include <stdlib.h>
#include <ldsodefs.h>

#include "libc-collfs.h"
#include "libc-collfs-private.h"
#include <stdarg.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>

extern int __libc_argc attribute_hidden;
extern char **__libc_argv attribute_hidden;

extern char **__environ;

/* The purpose of this file is to provide wrappers around the dynamic
   linker error mechanism (similar to dlopen() et al in libdl) which
   are usable from within libc.  Generally we want to throw away the
   string that dlerror() would return and just pass back a null pointer
   for errors.  This also lets the rest of libc not know about the error
   handling mechanism.

   Much of this code came from gconv_dl.c with slight modifications. */

static int
internal_function
dlerror_run (void (*operate) (void *), void *args)
{
  const char *objname;
  const char *last_errstring = NULL;
  bool malloced;

  (void) GLRO(dl_catch_error) (&objname, &last_errstring, &malloced,
			       operate, args);

  int result = last_errstring != NULL;
  if (result && malloced)
    free ((char *) last_errstring);

  return result;
}

/* These functions are called by dlerror_run... */

struct do_dlopen_args
{
  /* Argument to do_dlopen.  */
  const char *name;
  /* Opening mode.  */
  int mode;

  /* Return from do_dlopen.  */
  struct link_map *map;
};

struct do_dlsym_args
{
  /* Arguments to do_dlsym.  */
  struct link_map *map;
  const char *name;

  /* Return values of do_dlsym.  */
  lookup_t loadbase;
  const ElfW(Sym) *ref;
};

static void
do_dlopen (void *ptr)
{
  struct do_dlopen_args *args = (struct do_dlopen_args *) ptr;
  /* Open and relocate the shared object.  */
  args->map = GLRO(dl_open) (args->name, args->mode, NULL, __LM_ID_CALLER,
			     __libc_argc, __libc_argv, __environ);
}

static void
do_dlsym (void *ptr)
{
  struct do_dlsym_args *args = (struct do_dlsym_args *) ptr;
  args->ref = NULL;
  args->loadbase = GLRO(dl_lookup_symbol_x) (args->name, args->map, &args->ref,
					     args->map->l_local_scope, NULL, 0,
					     DL_LOOKUP_RETURN_NEWEST, NULL);
}

static void
do_dlclose (void *ptr)
{
  GLRO(dl_close) ((struct link_map *) ptr);
}

/* This code is to support __libc_dlopen from __libc_dlopen'ed shared
   libraries.  We need to ensure the statically linked __libc_dlopen
   etc. functions are used instead of the dynamically loaded.  */
struct dl_open_hook
{
  void *(*dlopen_mode) (const char *name, int mode);
  void *(*dlsym) (void *map, const char *name);
  int (*dlclose) (void *map);
};

#ifdef SHARED
extern struct dl_open_hook *_dl_open_hook;
libc_hidden_proto (_dl_open_hook);
struct dl_open_hook *_dl_open_hook __attribute__ ((nocommon));
libc_hidden_data_def (_dl_open_hook);
#else
static void
do_dlsym_private (void *ptr)
{
  lookup_t l;
  struct r_found_version vers;
  vers.name = "GLIBC_PRIVATE";
  vers.hidden = 1;
  /* vers.hash = _dl_elf_hash (vers.name);  */
  vers.hash = 0x0963cf85;
  vers.filename = NULL;

  struct do_dlsym_args *args = (struct do_dlsym_args *) ptr;
  args->ref = NULL;
  l = GLRO(dl_lookup_symbol_x) (args->name, args->map, &args->ref,
				args->map->l_scope, &vers, 0, 0, NULL);
  args->loadbase = l;
}

static struct dl_open_hook _dl_open_hook =
  {
    .dlopen_mode = __libc_dlopen_mode,
    .dlsym = __libc_dlsym,
    .dlclose = __libc_dlclose
  };
#endif

/* ... and these functions call dlerror_run. */

void *
__libc_dlopen_mode (const char *name, int mode)
{
  struct do_dlopen_args args;
  args.name = name;
  args.mode = mode;

#ifdef SHARED
  if (__builtin_expect (_dl_open_hook != NULL, 0))
    return _dl_open_hook->dlopen_mode (name, mode);
  return (dlerror_run (do_dlopen, &args) ? NULL : (void *) args.map);
#else
  if (dlerror_run (do_dlopen, &args))
    return NULL;

  __libc_register_dl_open_hook (args.map);
  __libc_register_dlfcn_hook (args.map);
  return (void *) args.map;
#endif
}
libc_hidden_def (__libc_dlopen_mode)

#ifndef SHARED
void *
__libc_dlsym_private (struct link_map *map, const char *name)
{
  struct do_dlsym_args sargs;
  sargs.map = map;
  sargs.name = name;

  if (! dlerror_run (do_dlsym_private, &sargs))
    return DL_SYMBOL_ADDRESS (sargs.loadbase, sargs.ref);
  return NULL;
}

void
__libc_register_dl_open_hook (struct link_map *map)
{
  struct dl_open_hook **hook;

  hook = (struct dl_open_hook **) __libc_dlsym_private (map, "_dl_open_hook");
  if (hook != NULL)
    *hook = &_dl_open_hook;
}
#endif

void *
__libc_dlsym (void *map, const char *name)
{
  struct do_dlsym_args args;
  args.map = map;
  args.name = name;

#ifdef SHARED
  if (__builtin_expect (_dl_open_hook != NULL, 0))
    return _dl_open_hook->dlsym (map, name);
#endif
  return (dlerror_run (do_dlsym, &args) ? NULL
	  : (void *) (DL_SYMBOL_ADDRESS (args.loadbase, args.ref)));
}
libc_hidden_def (__libc_dlsym)

int
__libc_dlclose (void *map)
{
#ifdef SHARED
  if (__builtin_expect (_dl_open_hook != NULL, 0))
    return _dl_open_hook->dlclose (map);
#endif
  return dlerror_run (do_dlclose, map);
}
libc_hidden_def (__libc_dlclose)


libc_freeres_fn (free_mem)
{
  struct link_map *l;
  struct r_search_path_elem *d;

  /* Remove all search directories.  */
  d = GL(dl_all_dirs);
  while (d != GLRO(dl_init_all_dirs))
    {
      struct r_search_path_elem *old = d;
      d = d->next;
      free (old);
    }

  /* Remove all additional names added to the objects.  */
  for (Lmid_t ns = 0; ns < DL_NNS; ++ns)
    for (l = GL(dl_ns)[ns]._ns_loaded; l != NULL; l = l->l_next)
      {
	struct libname_list *lnp = l->l_libname->next;

	l->l_libname->next = NULL;

	while (lnp != NULL)
	  {
	    struct libname_list *old = lnp;
	    lnp = lnp->next;
	    if (! old->dont_free)
	    free (old);
	  }
      }
}

/* Declare the unwrapped private libc API */
extern int __fxstat64(int vers, int fd, struct stat64 *buf);
extern int __xstat64 (int vers, const char *file, struct stat64 *buf);
extern int __close(int fd);
extern int __open (__const char *__file, int __oflag, ...);
extern void *__mmap(void *addr, size_t len, int prot, int flags, int fildes, off_t off);
extern int __munmap (__ptr_t addr, size_t len);
extern off_t __lseek(int fildes, off_t offset, int whence);
extern ssize_t __read(int fd,void *buf,size_t count);

/* Struct containing the collfs API used by libc_collfs, provided to libc_collfs_initialize() */
static struct libc_collfs_api collfs;

/* This function pointer is provided by libcollfs */
static collfs_debug_vprintf_fp collfs_debug_vprintf;

/* Always use this function for debugging. */
static int debug_printf(int level, const char *fmt, ...)
{
  va_list ap;
  int ret = 0;
  va_start(ap, fmt);
  if (collfs_debug_vprintf) ret = collfs_debug_vprintf(level, fmt, ap);
  va_end(ap);
  return ret;
}

/***********************************************************************
 * The API used by libcollfs
 ***********************************************************************/

/* Provide callbacks for the filesystem API, optionally get access to the unwrapped API. */
int libc_collfs_initialize(collfs_debug_vprintf_fp vprintf_, const struct libc_collfs_api *api, struct libc_collfs_api *unwrap)
{
  collfs_debug_vprintf = vprintf_;
  memcpy(&collfs, api, sizeof(struct libc_collfs_api));
  debug_printf(1, "%s: registered external API", __func__);
  if (unwrap) {
    unwrap->fxstat64 = __fxstat64;
    unwrap->xstat64  = __xstat64;
    unwrap->open     = __open;
    unwrap->close    = __close;
    unwrap->read     = __read;
    unwrap->lseek    = __lseek;
    unwrap->mmap     = __mmap;
    unwrap->munmap   = __munmap;
    debug_printf(1, "%s: provided unwrapped API", __func__);
  }
  return 0;
}

/* Disable collfs redirect. */
int libc_collfs_finalize(void)
{
  memset(&collfs,0,sizeof(collfs));
  debug_printf(1, "%s: Deregistered external API", __func__);
  collfs_debug_vprintf = 0;
  return 0;
}


/***********************************************************************
 * Wrappers for the file IO functions in libc
 *
 * Compilation flags are needed to wrap these appropriately so that they
 * get called instead of the unwrapped versions.
 ***********************************************************************/

int __collfs_fxstat64(int vers, int fd, struct stat64 *buf)
{
  if (collfs.fxstat64) {
    debug_printf(3,"%s(%d, %d, %p): redirected", __func__, vers, fd, (void*)buf);
    return collfs.fxstat64(vers, fd, buf);
  } else {
    debug_printf(3,"%s(%d, %d, %p): libc", __func__, vers, fd, (void*)buf);
    return __fxstat64(vers, fd, buf);
  }
}

int __collfs_xstat64(int vers, const char *file, struct stat64 *buf)
{
  if (collfs.xstat64) {
    debug_printf(3, "%s(%d, \"%s\", %p): redirected to collfs", __func__, vers, file, (void*)buf);
    return collfs.xstat64(vers, file, buf);
  } else {
    debug_printf(3, "%s(%d, \"%s\", %p): libc", __func__, vers, file, (void*)buf);
    return __xstat64(vers, file, buf);
  }
}

int __collfs_open(const char *pathname, int flags, ...)
{
  mode_t mode = 0;
  if (flags & O_CREAT) {
    va_list ap;
    va_start(ap, flags);
    mode = va_arg(ap, int);
    va_end(ap);
  }
  if (collfs.open) {
    debug_printf(3, "%s(\"%s\", %d, %d): redirected to collfs", __func__, pathname, flags, mode);
    return collfs.open(pathname, flags, mode);
  } else {
    debug_printf(3, "%s(\"%s\", %d, %d): libc", __func__, pathname, flags, mode);
    return __open(pathname, flags, mode);
  }
}

int __collfs_close(int fd)
{
  if (collfs.close) {
    debug_printf(3, "%s(%d): redirected to collfs", __func__, fd);
    return collfs.close(fd);
  } else {
    debug_printf(3, "%s(%d): libc", __func__, fd);
    return __close(fd);
  }
}

ssize_t __collfs_read(int fd, void *buf, size_t count)
{
  if (collfs.read) {
    debug_printf(3, "%s(%d, %p, %zu): redirected to collfs", __func__, fd, buf, count);
    return collfs.read(fd, buf, count);
  } else {
    debug_printf(3, "%s(%d, %p, %zu): libc", __func__, fd, buf, count);
    return __read(fd, buf, count);
  }
}

off_t __collfs_lseek(int fd, off_t offset, int whence)
{
  if (collfs.lseek) {
    debug_printf(3, "%s(%d, %lld, %d): redirected to collfs", __func__, fd, (long long)offset, whence);
    return collfs.lseek(fd, offset, whence);
  } else {
    debug_printf(3, "%s(%d, %lld, %d): libc", __func__, fd, (long long)offset, whence);
    return __lseek(fd, offset, whence);
  }
}

void *__collfs_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
  if (collfs.mmap) {
    debug_printf(3, "%s(%p, %zd, %d, %d, %d, %lld): redirected to collfs", __func__, addr, len, prot, flags, fd, (long long)off);
    return collfs.mmap(addr, len, prot, flags, fd, off);
  } else {
    debug_printf(3, "%s(%p, %zd, %d, %d, %d, %lld): libc", __func__, addr, len, prot, flags, fd, (long long)off);
    return __mmap(addr, len, prot, flags, fd, off);
  }
}

int __collfs_munmap (void *addr, size_t len)
{
  if (collfs.munmap) {
    debug_printf(3, "%s(%p, %zd): redirected to collfs", __func__, addr, len);
    return collfs.munmap(addr, len);
  } else {
    debug_printf(3, "%s(%p, %zd): libc", __func__, addr, len);
    return __munmap(addr, len);
  }
}
