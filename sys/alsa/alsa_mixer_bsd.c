#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <dev/sound/pcm/sound.h>
#include <sound/core.h>
#include <sound/control.h>

#include "mixer_if.h"

MALLOC_DECLARE(M_ALSA);

static int
basound_mixer_init(struct snd_mixer *m)
{
	struct snd_card *card = mix_getdevinfo(m);
	uint32_t mask = 0;

	mask = SOUND_MASK_VOLUME | SOUND_MASK_PCM;
	mix_setdevs(m, mask);

	dev_info(card->dev, "Mixer initialized for card %s\n", card->id);
	return 0;
}

static int
basound_mixer_set(struct snd_mixer *m, unsigned dev, unsigned left, unsigned right)
{
	struct snd_card *card = mix_getdevinfo(m);
	
	dev_dbg(card->dev, "Mixer set: dev=%u left=%u right=%u\n", dev, left, right);
	
	return left | (right << 8);
}

static kobj_method_t basound_mixer_methods[] = {
	KOBJMETHOD(mixer_init,		basound_mixer_init),
	KOBJMETHOD(mixer_set,		basound_mixer_set),
	KOBJMETHOD_END
};
MIXER_DECLARE(basound_mixer);

int
basound_mixer_register(struct snd_card *card)
{
	device_t dev = card->dev->bsddev;

	if (mixer_init(dev, &basound_mixer_class, card)) {
		dev_err(card->dev, "mixer_init failed\n");
		return -ENXIO;
	}

	return 0;
}
