#ifndef _LINUX_FIREWIRE_H_
#define _LINUX_FIREWIRE_H_

/* FireWire shim: Map to FreeBSD FireWire stack */
#include <dev/firewire/firewire.h>

/* Forward declarations for ALSA drivers */
struct fw_unit {
	struct fw_device *device;
};

struct fw_iso_buffer {
	/* FreeBSD uses bus_dma; this is a placeholder */
	void *buf;
	size_t size;
};

#endif
