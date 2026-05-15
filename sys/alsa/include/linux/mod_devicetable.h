#ifndef _LINUX_MOD_DEVICETABLE_H_
#define _LINUX_MOD_DEVICETABLE_H_

#include <sys/types.h>

struct ieee1394_device_id {
	unsigned int match_flags;
	unsigned int vendor_id;
	unsigned int model_id;
};

#endif
