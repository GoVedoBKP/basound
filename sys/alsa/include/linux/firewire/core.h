#ifndef _LINUX_FIREWIRE_H_
#define _LINUX_FIREWIRE_H_

#include <sys/param.h>
#include <sys/bus.h>

/* Minimal IEEE 1394 shim for Dice driver */

struct fw_device {
	device_t bsddev;
	/* ... */
};

/* Placeholder for FireWire transaction/streaming primitives */
#endif
