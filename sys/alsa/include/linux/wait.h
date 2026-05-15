#ifndef _LINUX_WAIT_H_
#define _LINUX_WAIT_H_

#include <sys/param.h>
#include <sys/condvar.h>

typedef struct cv wait_queue_head_t;

#define init_waitqueue_head(q) cv_init(q, "waitq")
#define wake_up(q) cv_broadcast(q)
#define wake_up_interruptible(q) cv_broadcast(q)

/* Stub for wait_event_timeout */
#define wait_event_timeout(q, condition, timeout) (0)

#endif
