// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef _SOUND_INFO_H_
#define _SOUND_INFO_H_

#include <sound/core.h>

struct snd_info_entry {
	const char *name;
	void *private_data;
};

int snd_card_proc_new(struct snd_card *card, const char *name, struct snd_info_entry **entryp);
void snd_info_set_text_ops(struct snd_info_entry *entry, void *data, void (*show)(struct snd_info_entry *, void *));

#endif
