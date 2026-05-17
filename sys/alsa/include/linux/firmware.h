// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef _LINUX_FIRMWARE_H_
#define _LINUX_FIRMWARE_H_

#include <sys/types.h>

struct firmware {
	size_t size;
	uint8_t *data;
	void *priv;
};

struct device;

int request_firmware(const struct firmware **fw, const char *name, struct device *device);
void release_firmware(const struct firmware *fw);

#endif /* _LINUX_FIRMWARE_H_ */
