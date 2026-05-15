#ifndef _LINUX_COMPAT_H_
#define _LINUX_COMPAT_H_

#include <sys/param.h>
#include <sys/types.h>

/* Type definitions for Linux compatibility */
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

#define __init
#define __exit
#define module_init(x)
#define module_exit(x)
#define MODULE_AUTHOR(x)
#define MODULE_DESCRIPTION(x)
#define MODULE_LICENSE(x)

#endif
