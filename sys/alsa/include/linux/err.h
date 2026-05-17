// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef _LINUX_ERR_H_
#define _LINUX_ERR_H_

#include <sys/errno.h>

/*
 * Macros for handling error pointers similar to Linux kernel style.
 * In the Linux kernel, error pointers encode error codes in pointer values.
 * We use a simpler approach with actual error returns, but keep these macros
 * for compatibility with ported code.
 */

#define IS_ERR(ptr) ((long)(ptr) < 0)
#define PTR_ERR(ptr) ((long)(ptr))
#define ERR_PTR(err) ((void *)(long)(err))

#endif
