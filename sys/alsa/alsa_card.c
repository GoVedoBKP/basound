// SPDX-License-Identifier: GPL-3.0-or-later
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>
#include <sys/systm.h>

MALLOC_DEFINE(M_ALSA, "alsa", "ALSA shim memory");

int
snd_card_new(struct device *parent, int idx, const char *xid,
	     struct module *module, int extra_size,
	     struct snd_card **card_ret)
{
	struct snd_card *card;

	card = malloc(sizeof(*card) + extra_size, M_ALSA, M_WAITOK | M_ZERO);
	if (card == NULL)
		return -ENOMEM;

	card->dev = parent;
	STAILQ_INIT(&card->pcm_list);
	STAILQ_INIT(&card->ctl_list);

	if (extra_size > 0)
		card->private_data = (void *)(card + 1);

	if (xid)
		strlcpy(card->id, xid, sizeof(card->id));

	*card_ret = card;
	return 0;
}

int
snd_card_free(struct snd_card *card)
{
	if (card == NULL)
		return 0;

	free(card, M_ALSA);
	return 0;
}

int
snd_card_register(struct snd_card *card)
{
	struct snd_pcm *pcm;

	STAILQ_FOREACH(pcm, &card->pcm_list, next_pcm) {
		basound_pcm_register(pcm);
	}

	basound_mixer_register(card);

	dev_info(card->dev, "Registered card %s (%s)\n", card->id, card->shortname);
	return 0;
}
