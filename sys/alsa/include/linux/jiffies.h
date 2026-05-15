#ifndef _LINUX_JIFFIES_H_
#define _LINUX_JIFFIES_H_

#include <sys/param.h>
#include <sys/systm.h>

#define jiffies		ticks
#define msecs_to_jiffies(m)	((m) * hz / 1000)

#endif
