#ifndef _LINUX_MUTEX_H_
#define _LINUX_MUTEX_H_

#include <sys/lock.h>
#include <sys/mutex.h>

typedef struct mtx mutex_t;

#define mutex_init(m) mtx_init(m, "mutex", NULL, MTX_DEF)
#define mutex_lock(m) mtx_lock(m)
#define mutex_unlock(m) mtx_unlock(m)
#define mutex_destroy(m) mtx_destroy(m)

#endif
