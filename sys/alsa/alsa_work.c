// SPDX-License-Identifier: GPL-3.0-or-later
#include <linux/workqueue.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/interrupt.h>

void
basound_work_handler(void *context, int pending)
{
	struct work_struct *work = context;
	work->func(work);
}

int
schedule_work(struct work_struct *work)
{
	return taskqueue_enqueue(taskqueue_thread, &work->task);
}
