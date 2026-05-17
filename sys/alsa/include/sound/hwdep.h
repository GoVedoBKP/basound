// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef _SOUND_HWDEP_H_
#define _SOUND_HWDEP_H_

#include <sound/core.h>

struct snd_hwdep {
	struct snd_card *card;
	char name[64];
	void *private_data;
};

int snd_hwdep_new(struct snd_card *card, char *id, int device, struct snd_hwdep **rhwdep);

#endif
