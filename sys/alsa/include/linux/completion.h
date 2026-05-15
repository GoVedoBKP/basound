#ifndef _LINUX_COMPLETION_H_
#define _LINUX_COMPLETION_H_

#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>

struct completion {
	struct mtx lock;
	struct cv cv;
	unsigned int done;
};

#define init_completion(x) do { \
	mtx_init(&(x)->lock, "compl", NULL, MTX_DEF); \
	cv_init(&(x)->cv, "compl"); \
	(x)->done = 0; \
} while (0)

static inline void wait_for_completion(struct completion *x)
{
	mtx_lock(&x->lock);
	while (!x->done)
		cv_wait(&x->cv, &x->lock);
	mtx_unlock(&x->lock);
}

static inline void complete(struct completion *x)
{
	mtx_lock(&x->lock);
	x->done = 1;
	cv_broadcast(&x->cv);
	mtx_unlock(&x->lock);
}

#endif
