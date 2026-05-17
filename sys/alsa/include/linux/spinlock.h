// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef _LINUX_SPINLOCK_H_
#define _LINUX_SPINLOCK_H_

#include <sys/mutex.h>

typedef struct mtx spinlock_t;

#define spin_lock_init(m) mtx_init(m, "spinlock", NULL, MTX_SPIN)
#define spin_lock(m) mtx_lock_spin(m)
#define spin_unlock(m) mtx_unlock_spin(m)
#define spin_lock_irqsave(m, flags) do { (flags) = 0; mtx_lock_spin(m); } while (0)
#define spin_unlock_irqrestore(m, flags) mtx_unlock_spin(m)

#endif
