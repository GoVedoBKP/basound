#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <dev/sound/pcm/sound.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include "alsa_pcm_bsd.h"
#include "channel_if.h"

MALLOC_DECLARE(M_ALSA);

/* FreeBSD Channel Methods */

static void *
basound_chan_init(kobj_t obj, void *devinfo, struct snd_dbuf *b, struct pcm_channel *c, int dir)
{
	struct snd_pcm *pcm = devinfo;
	struct snd_pcm_str *pstr;
	struct snd_pcm_substream *substream;
	struct basound_chan *ch;
	int stream = (dir == PCMDIR_PLAY) ? SNDRV_PCM_STREAM_PLAYBACK : SNDRV_PCM_STREAM_CAPTURE;

	pstr = &pcm->streams[stream];
	if (pstr->substream_count <= 0)
		return NULL;

	/* For now, just pick the first substream */
	substream = &pstr->substream[0];

	ch = malloc(sizeof(*ch), M_ALSA, M_WAITOK | M_ZERO);
	ch->substream = substream;
	ch->channel = c;
	substream->private_data = ch;
	
	/* Initialize ALSA runtime */
	ch->runtime = malloc(sizeof(*ch->runtime), M_ALSA, M_WAITOK | M_ZERO);
	substream->runtime = ch->runtime;

	return ch;
}

static int
basound_chan_setformat(kobj_t obj, void *data, uint32_t format)
{
	struct basound_chan *ch = data;
	ch->format = format;
	return 0;
}

static uint32_t
basound_chan_setspeed(kobj_t obj, void *data, uint32_t speed)
{
	struct basound_chan *ch = data;
	ch->speed = speed;
	return speed;
}

static uint32_t
basound_chan_setblocksize(kobj_t obj, void *data, uint32_t blocksize)
{
	struct basound_chan *ch = data;
	ch->blocksize = blocksize;
	return blocksize;
}

static int
basound_chan_trigger(kobj_t obj, void *data, int go)
{
	struct basound_chan *ch = data;
	struct snd_pcm_substream *substream = ch->substream;
	const struct snd_pcm_ops *ops = substream->pstr->ops;

	if (ops && ops->trigger)
		return ops->trigger(substream, go);
	
	return 0;
}

static uint32_t
basound_chan_getptr(kobj_t obj, void *data)
{
	struct basound_chan *ch = data;
	struct snd_pcm_substream *substream = ch->substream;
	const struct snd_pcm_ops *ops = substream->pstr->ops;

	if (ops && ops->pointer)
		return (uint32_t)ops->pointer(substream);
	
	return 0;
}

static kobj_method_t basound_chan_methods[] = {
	KOBJMETHOD(channel_init,		basound_chan_init),
	KOBJMETHOD(channel_setformat,		basound_chan_setformat),
	KOBJMETHOD(channel_setspeed,		basound_chan_setspeed),
	KOBJMETHOD(channel_setblocksize,	basound_chan_setblocksize),
	KOBJMETHOD(channel_trigger,		basound_chan_trigger),
	KOBJMETHOD(channel_getptr,		basound_chan_getptr),
	KOBJMETHOD_END
};
DEFINE_CLASS(basound_chan, basound_chan_methods, 0);

int
basound_pcm_register(struct snd_pcm *pcm)
{
	struct snd_card *card = pcm->card;
	device_t dev = card->dev->bsddev;
	struct snd_pcm_str *pstr_p = &pcm->streams[SNDRV_PCM_STREAM_PLAYBACK];
	struct snd_pcm_str *pstr_c = &pcm->streams[SNDRV_PCM_STREAM_CAPTURE];
	char status[SND_STATUSLEN];

	pcm_init(dev, pcm);

	snprintf(status, sizeof(status), "at %s", device_get_nameunit(dev));

	/* Register the pcm device with FreeBSD */
	if (pcm_register(dev, status)) {
		dev_err(card->dev, "pcm_register failed\n");
		return -ENXIO;
	}

	/* Add playback channels */
	if (pstr_p->substream_count > 0)
		pcm_addchan(dev, PCMDIR_PLAY, &basound_chan_class, pcm);

	/* Add capture channels */
	if (pstr_c->substream_count > 0)
		pcm_addchan(dev, PCMDIR_REC, &basound_chan_class, pcm);

	return 0;
}
