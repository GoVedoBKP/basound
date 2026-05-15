#ifndef _LINUX_DELAY_H_
#define _LINUX_DELAY_H_

#include <sys/systm.h>

#define linux_msleep(x)	pause("msleep", (x) * hz / 1000)
#define mdelay(x)	DELAY((x) * 1000)

#endif
