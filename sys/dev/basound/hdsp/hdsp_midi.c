// SPDX-License-Identifier: GPL-3.0-or-later
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/stddef.h>
#include <linux/workqueue.h>
#include "hdsp.h"

void
snd_hdsp_midi_work(struct work_struct *work)
{
	struct hdsp *hdsp = (struct hdsp *)((char *)work - offsetof(struct hdsp, midi_work));
	int i;

	for (i = 0; i < 2; i++) {
		struct hdsp_midi *hmidi = &hdsp->midi[i];
		
		mtx_lock(&hmidi->lock);
		/* Placeholder for reading/writing MIDI data */
		mtx_unlock(&hmidi->lock);
	}
}
