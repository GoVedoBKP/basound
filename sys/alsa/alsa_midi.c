// SPDX-License-Identifier: GPL-3.0-or-later
#include <sound/rawmidi.h>
#include <sys/systm.h>
#include <sys/malloc.h>

MALLOC_DECLARE(M_ALSA);

int
snd_rawmidi_new(struct snd_card *card, char *id, int device,
		int output_count, int input_count,
		struct snd_rawmidi **rrawmidi)
{
	struct snd_rawmidi *rmidi;

	rmidi = malloc(sizeof(*rmidi), M_ALSA, M_WAITOK | M_ZERO);
	if (rmidi == NULL)
		return -ENOMEM;

	rmidi->card = card;
	if (id)
		strlcpy(rmidi->id, id, sizeof(rmidi->id));

	if (rrawmidi)
		*rrawmidi = rmidi;

	dev_info(card->dev, "Created RawMIDI %s\n", rmidi->id);
	return 0;
}
