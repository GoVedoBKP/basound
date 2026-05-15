#ifndef _LINUX_WORKQUEUE_H_
#define _LINUX_WORKQUEUE_H_

#include <sys/param.h>
#include <sys/taskqueue.h>

struct work_struct {
	struct task task;
	void (*func)(struct work_struct *work);
};

#define INIT_WORK(_work, _func) do { \
	TASK_INIT(&(_work)->task, 0, (task_fn_t *)basound_work_handler, (_work)); \
	(_work)->func = (_func); \
} while (0)

void basound_work_handler(void *context, int pending);
int schedule_work(struct work_struct *work);

#endif /* _LINUX_WORKQUEUE_H_ */
