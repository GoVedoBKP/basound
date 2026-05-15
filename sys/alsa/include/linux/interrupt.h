#ifndef _LINUX_INTERRUPT_H_
#define _LINUX_INTERRUPT_H_

#include <sys/param.h>
#include <sys/bus.h>

typedef int irqreturn_t;
#define IRQ_NONE	0
#define IRQ_HANDLED	1

struct device;

/* request_irq shim will be more complex because it needs to map to FreeBSD's bus_setup_intr */

#endif /* _LINUX_INTERRUPT_H_ */
