#ifndef _LINUX_SLAB_H_
#define _LINUX_SLAB_H_

#include <sys/param.h>
#include <sys/malloc.h>

#define kmalloc(size, flags) malloc(size, M_DEVBUF, M_WAITOK | M_ZERO)
#define kfree(ptr) free(ptr, M_DEVBUF)

#endif
