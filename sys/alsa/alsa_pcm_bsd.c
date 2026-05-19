// SPDX-License-Identifier: GPL-3.0-or-later
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

	substream = &pstr->substream[0];

	ch = malloc(sizeof(*ch), M_ALSA, M_WAITOK | M_ZERO);
	ch->substream = substream;
	ch->channel = c;
	substream->private_data = ch;

	ch->runtime = malloc(sizeof(*ch->runtime), M_ALSA, M_WAITOK | M_ZERO);
	substream->runtime = ch->runtime;

	/* Allocate DMA-capable hardware buffer using the card-level DMA tag.
	 * bus_dmamem_alloc uses dmatag->maxsize (4 MB) as the allocation size,
	 * which matches the size we pass to sndbuf_alloc. */
	if (pcm->card->dmatag == NULL) {
		free(ch->runtime, M_ALSA);
		free(ch, M_ALSA);
		return NULL;
	}
	if (sndbuf_alloc(b, pcm->card->dmatag, 0,
	    4 * 1024 * 1024) != 0) {
		free(ch->runtime, M_ALSA);
		free(ch, M_ALSA);
		return NULL;
	}

	/* Make the DMA address visible to the ALSA runtime and the
	 * hardware trigger path (hdsp_main.c reads these via
	 * hdsp->playback_substream->runtime->dma_addr). */
	substream->runtime->dma_area  = sndbuf_getbuf(b);
	substream->runtime->dma_addr  = sndbuf_getbufaddr(b);
	substream->runtime->dma_bytes = sndbuf_getsize(b);

	return ch;
}

static int
basound_chan_free(kobj_t obj, void *data)
{
	struct basound_chan *ch = data;

	if (ch->runtime != NULL) {
		free(ch->runtime, M_ALSA);
		ch->runtime = NULL;
	}
	free(ch, M_ALSA);
	return 0;
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

static uint32_t basound_fmtlist[] = {
	SND_FORMAT(AFMT_S32_LE, 2, 0),
	SND_FORMAT(AFMT_S16_LE, 2, 0),
	0
};
static struct pcmchan_caps basound_caps = {32000, 192000, basound_fmtlist, 0};

static struct pcmchan_caps *
basound_chan_getcaps(kobj_t obj, void *data)
{
	return &basound_caps;
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
	KOBJMETHOD(channel_free,		basound_chan_free),
	KOBJMETHOD(channel_getcaps,		basound_chan_getcaps),
	KOBJMETHOD(channel_setformat,		basound_chan_setformat),
	KOBJMETHOD(channel_setspeed,		basound_chan_setspeed),
	KOBJMETHOD(channel_setblocksize,	basound_chan_setblocksize),
	KOBJMETHOD(channel_trigger,		basound_chan_trigger),
	KOBJMETHOD(channel_getptr,		basound_chan_getptr),
	KOBJMETHOD_END
};
DEFINE_CLASS(basound_chan, basound_chan_methods, 0);

/*
 * PCM child device driver.
 *
 * The parent PCI device (e.g. basound_hdsp) creates a "pcm" child device via
 * device_add_child(), stores a struct snd_pcm * in its ivars, and calls
 * device_probe_and_attach().  This driver is registered for the
 * "basound_hdsp" bus, so newbus finds it, allocates PCM_SOFTC_SIZE bytes
 * for the snddev_info softc, and calls basound_pcm_attach().
 */
static int
basound_pcm_probe(device_t dev)
{
	device_set_desc(dev, "PCM Audio");
	return BUS_PROBE_DEFAULT;
}

static int
basound_pcm_attach(device_t dev)
{
	struct snd_pcm *pcm = device_get_ivars(dev);
	struct snd_card *card = pcm->card;
	struct snd_pcm_str *pstr_p = &pcm->streams[SNDRV_PCM_STREAM_PLAYBACK];
	struct snd_pcm_str *pstr_c = &pcm->streams[SNDRV_PCM_STREAM_CAPTURE];
	char status[SND_STATUSLEN];

	/* Set description here because basound_pcm_probe is bypassed
	 * (we use device_set_driver + device_attach directly). */
	device_set_desc_copy(dev, pcm->id[0] ? pcm->id : "HDSP PCM");

	/* dev's softc is PCM_SOFTC_SIZE bytes — safe for snddev_info */
	pcm_init(dev, pcm);

	snprintf(status, sizeof(status), "at %s",
	    device_get_nameunit(device_get_parent(dev)));

	if (pcm_register(dev, status) != 0) {
		dev_err(card->dev, "pcm_register failed\n");
		return ENXIO;
	}

	if (pstr_p->substream_count > 0)
		pcm_addchan(dev, PCMDIR_PLAY, &basound_chan_class, pcm);
	if (pstr_c->substream_count > 0)
		pcm_addchan(dev, PCMDIR_REC, &basound_chan_class, pcm);

	return 0;
}

static int
basound_pcm_detach(device_t dev)
{
	return pcm_unregister(dev);
}

static device_method_t basound_pcm_methods[] = {
	DEVMETHOD(device_probe,		basound_pcm_probe),
	DEVMETHOD(device_attach,	basound_pcm_attach),
	DEVMETHOD(device_detach,	basound_pcm_detach),
	DEVMETHOD_END
};

static driver_t basound_pcm_driver = {
	"pcm",
	basound_pcm_methods,
	PCM_SOFTC_SIZE,
};

/*
 * Register the PCM sub-driver under the "basound_hdsp" bus so that
 * device_probe_and_attach() finds it when the parent creates a "pcm" child.
 */
DRIVER_MODULE(basound_pcm, basound_hdsp, basound_pcm_driver, 0, 0);

/*
 * basound_pcm_register — called from snd_card_register().
 *
 * Creates a "pcm" child device under the parent PCI device, stores the
 * snd_pcm pointer as ivars, and lets newbus probe/attach it via the
 * basound_pcm sub-driver above.  The child device gets PCM_SOFTC_SIZE bytes
 * for its softc, so pcm_init() writes into snddev_info, not our own softc.
 */
int
basound_pcm_register(struct snd_pcm *pcm)
{
	struct snd_card *card = pcm->card;
	device_t parent = card->dev->bsddev;
	device_t pcm_dev;
	int err;

	pcm_dev = device_add_child(parent, "pcm", -1);
	if (pcm_dev == NULL) {
		dev_err(card->dev, "device_add_child(pcm) failed\n");
		return -ENXIO;
	}

	device_set_ivars(pcm_dev, pcm);

	/*
	 * Explicitly set our driver so device_set_driver() allocates
	 * PCM_SOFTC_SIZE bytes for the softc.  We then call device_attach()
	 * directly to skip the devclass-based probe path (which would race
	 * against the DRIVER_MODULE SYSINIT if the PCI reprobe fires before
	 * our SYSINIT has run).
	 */
	err = device_set_driver(pcm_dev, &basound_pcm_driver);
	if (err != 0) {
		dev_err(card->dev, "device_set_driver failed: %d\n", err);
		device_delete_child(parent, pcm_dev);
		return -ENXIO;
	}

	card->pcm_dev = pcm_dev;

	err = device_attach(pcm_dev);
	if (err != 0) {
		dev_err(card->dev, "pcm device attach failed: %d\n", err);
		device_delete_child(parent, pcm_dev);
		card->pcm_dev = NULL;
		return -ENXIO;
	}

	return 0;
}

