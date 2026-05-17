// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef _ALSA_RAWMIDI_H_
#define _ALSA_RAWMIDI_H_

#include <sound/core.h>

struct snd_rawmidi {
	struct snd_card *card;
	char id[64];
	char name[80];
	void *private_data;
};

int snd_rawmidi_new(struct snd_card *card, char *id, int device,
		    int output_count, int input_count,
		    struct snd_rawmidi **rrawmidi);

#endif /* _ALSA_RAWMIDI_H_ */
