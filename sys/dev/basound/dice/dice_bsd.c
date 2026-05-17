#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/sound/pcm/sound.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/rawmidi.h>

#include "../audio_stream.h"

/* DICE FireWire driver - bridges FreeBSD fw_device to ALSA DICE driver */

MALLOC_DECLARE(M_ALSA);

#define OUI_WEISS		0x001c6a
#define OUI_LOUD		0x000ff2
#define OUI_FOCUSRITE		0x00130e
#define OUI_TCELECTRONIC	0x000166
#define OUI_ALESIS		0x000595
#define OUI_MAUDIO		0x000d6c
#define OUI_MYTEK		0x001ee8
#define OUI_SSL			0x0050c2
#define OUI_PRESONUS		0x000a92
#define OUI_HARMAN		0x000fd7
#define OUI_AVID		0x00a07e

#define DICE_CATEGORY_ID	0x04
#define WEISS_CATEGORY_ID	0x00
#define LOUD_CATEGORY_ID	0x10
#define HARMAN_CATEGORY_ID	0x20

struct dice_bsd_softc {
	device_t dev;
	struct fw_device *fwdev;
	void *alsa_dice;
	
	/* Audio streaming framework */
	struct audio_stream playback_stream;
	struct audio_stream capture_stream;
	
	/* FireWire isochronous context (framework ready) */
	void *tx_context;  /* Transmit/playback context */
	void *rx_context;  /* Receive/capture context */
};

MALLOC_DEFINE(M_DICE_BSD, "dice_bsd", "DICE BSD softc");

/* Check if device is DICE category based on device config ROM */
static int
dice_bsd_check_dice_category(struct fw_device *fwdev)
{
	uint32_t vendor_id = fwdev->csrrom[3] >> 8;
	uint32_t category;

	if (vendor_id == OUI_WEISS)
		category = WEISS_CATEGORY_ID;
	else if (vendor_id == OUI_LOUD)
		category = LOUD_CATEGORY_ID;
	else if (vendor_id == OUI_HARMAN)
		category = HARMAN_CATEGORY_ID;
	else
		category = DICE_CATEGORY_ID;

	/* Verify category matches */
	if ((fwdev->csrrom[3] & 0xFF) != category)
		return -ENODEV;

	return 0;
}

/* Check if device matches a known DICE vendor ID */
static int
dice_bsd_match_vendor(struct fw_device *fwdev)
{
	uint32_t vendor_id = fwdev->csrrom[3] >> 8;

	switch (vendor_id) {
	case OUI_WEISS:
	case OUI_LOUD:
	case OUI_FOCUSRITE:
	case OUI_TCELECTRONIC:
	case OUI_ALESIS:
	case OUI_MAUDIO:
	case OUI_MYTEK:
	case OUI_SSL:
	case OUI_PRESONUS:
	case OUI_HARMAN:
	case OUI_AVID:
		return 0;
	default:
		return -ENODEV;
	}
}

/* PCM callback implementations with audio streaming framework */
static int
dice_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct snd_card *card = substream->pcm->card;
	struct dice_bsd_softc *sc = (struct dice_bsd_softc *)card->dev->bsddev;
	struct audio_stream *stream;
	
	if (runtime == NULL)
		return -ENOMEM;
	
	/* Select appropriate stream based on direction */
	stream = (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) ?
		&sc->playback_stream : &sc->capture_stream;
	
	/* Set hardware constraints for playback/capture */
	runtime->hw.info = SNDRV_PCM_INFO_MMAP |
			   SNDRV_PCM_INFO_MMAP_VALID |
			   SNDRV_PCM_INFO_INTERLEAVED;
	runtime->hw.formats = SNDRV_PCM_FMTBIT_S24_3LE |
			      SNDRV_PCM_FMTBIT_S32_LE;
	runtime->hw.rates = SNDRV_PCM_RATE_44100 |
			    SNDRV_PCM_RATE_48000 |
			    SNDRV_PCM_RATE_96000 |
			    SNDRV_PCM_RATE_192000;
	runtime->hw.rate_min = 44100;
	runtime->hw.rate_max = 192000;
	runtime->hw.channels_min = 2;
	runtime->hw.channels_max = 8;
	runtime->hw.buffer_bytes_max = 1 << 24; /* 16MB */
	runtime->hw.period_bytes_min = 512;
	runtime->hw.period_bytes_max = 1 << 16; /* 64KB */
	runtime->hw.periods_min = 2;
	runtime->hw.periods_max = 1024;
	
	/* Initialize stream framework */
	stream->state = AUDIO_STREAM_PREPARED;
	runtime->private_data = stream;
	
	return 0;
}

static int
dice_pcm_close(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_stream *stream = runtime->private_data;
	
	if (stream == NULL)
		return 0;
	
	/* Ensure stream is stopped */
	if (stream->state == AUDIO_STREAM_RUNNING || 
	    stream->state == AUDIO_STREAM_PAUSED)
		audio_stream_stop(stream);
	
	stream->state = AUDIO_STREAM_IDLE;
	return 0;
}

static int
dice_pcm_hw_params(struct snd_pcm_substream *substream, void *hw_params)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_stream *stream = runtime->private_data;
	int err;
	
	if (runtime == NULL)
		return -ENOMEM;
	
	if (stream == NULL)
		return -EINVAL;
	
	/* Validate parameters */
	if (runtime->hw.rate_min == 0 || runtime->hw.rate_max == 0)
		return -EINVAL;
	
	/* Allocate DMA buffer for audio data */
	err = snd_pcm_lib_malloc_pages(substream, 
				       runtime->hw.buffer_bytes_max);
	if (err < 0)
		return err;
	
	/* Verify allocation succeeded */
	if (runtime->dma_area == NULL)
		return -ENOMEM;
	
	/* Store DMA and stream parameters */
	mtx_lock(&stream->lock);
	runtime->dma_bytes = runtime->hw.buffer_bytes_max;
	stream->dma_virt = runtime->dma_area;
	stream->dma_phys = runtime->dma_addr;
	stream->dma_size = runtime->dma_bytes;
	stream->channels = 2;  /* Default, set properly in hw_params parsing */
	stream->sample_rate = 48000;  /* Default */
	mtx_unlock(&stream->lock);
	
	return 0;
}

static int
dice_pcm_hw_free(struct snd_pcm_substream *substream)
{
	/* Free allocated DMA buffers */
	return snd_pcm_lib_free_pages(substream);
}

static int
dice_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_stream *stream = runtime->private_data;
	
	if (runtime == NULL || runtime->dma_area == NULL)
		return -EINVAL;
	
	if (stream == NULL)
		return -EINVAL;
	
	/* Prepare stream for playback/capture */
	mtx_lock(&stream->lock);
	stream->position = 0;
	stream->state = AUDIO_STREAM_PREPARED;
	mtx_unlock(&stream->lock);
	
	return 0;
}

static int
dice_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_stream *stream = runtime->private_data;
	int err = 0;
	
	if (runtime == NULL)
		return -EINVAL;
	
	if (stream == NULL)
		return -EINVAL;
	
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* Start audio streaming via framework */
		err = audio_stream_start(stream);
		if (err == 0)
			runtime->state = SNDRV_PCM_STATE_RUNNING;
		break;
		
	case SNDRV_PCM_TRIGGER_STOP:
		/* Stop audio streaming via framework */
		err = audio_stream_stop(stream);
		if (err == 0)
			runtime->state = SNDRV_PCM_STATE_STOPPED;
		break;
		
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		/* Pause streaming */
		err = audio_stream_pause(stream);
		if (err == 0)
			runtime->state = SNDRV_PCM_STATE_PAUSED;
		break;
		
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		/* Resume from pause */
		err = audio_stream_resume(stream);
		if (err == 0)
			runtime->state = SNDRV_PCM_STATE_RUNNING;
		break;
		
	default:
		return -EINVAL;
	}
	
	return err;
}

static unsigned long
dice_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_stream *stream = runtime->private_data;
	unsigned long position;
	
	if (runtime == NULL || runtime->dma_area == NULL)
		return 0;
	
	if (stream == NULL)
		return 0;
	
	/* Return current streaming position with wraparound handling */
	mtx_lock(&stream->lock);
	position = stream->position % runtime->dma_bytes;
	mtx_unlock(&stream->lock);
	
	return position;
}

static const struct snd_pcm_ops dice_pcm_ops = {
	.open = dice_pcm_open,
	.close = dice_pcm_close,
	.hw_params = dice_pcm_hw_params,
	.hw_free = dice_pcm_hw_free,
	.prepare = dice_pcm_prepare,
	.trigger = dice_pcm_trigger,
	.pointer = dice_pcm_pointer,
};

static int
dice_bsd_probe(device_t dev)
{
	struct firewire_dev_comm *fdc;
	struct fw_device *fwdev;

	fdc = device_get_softc(dev);
	if (fdc == NULL)
		return ENXIO;

	/* Get the fw_device from the FireWire bus */
	fwdev = NULL;
	/* Note: In FreeBSD, the fw_device is obtained from the bus.
	 * For now, we do basic vendor matching.
	 * The actual device is passed via device_get_ivars(dev)
	 */
	if (device_get_ivars(dev) == NULL)
		return ENXIO;

	fwdev = (struct fw_device *)device_get_ivars(dev);

	/* Check if this is a DICE device */
	if (dice_bsd_match_vendor(fwdev) < 0)
		return ENXIO;

	if (dice_bsd_check_dice_category(fwdev) < 0)
		return ENXIO;

	device_set_desc(dev, "DICE FireWire Audio Device");
	return (BUS_PROBE_DEFAULT);
}

static int
dice_bsd_attach(device_t dev)
{
	struct dice_bsd_softc *sc = device_get_softc(dev);
	struct snd_card *card;
	struct snd_pcm *pcm;
	struct fw_device *fwdev;
	int err, unit;
	
	/* Get the FireWire device */
	fwdev = (struct fw_device *)device_get_ivars(dev);
	if (fwdev == NULL)
		return ENXIO;
	
	sc->dev = dev;
	sc->fwdev = fwdev;
	
	/* Initialize audio streaming framework */
	audio_stream_init(&sc->playback_stream, "dice_playback_stream");
	audio_stream_init(&sc->capture_stream, "dice_capture_stream");
	
	/* Set up streaming callbacks (framework ready for real implementation) */
	sc->playback_stream.dev_private = sc;
	sc->capture_stream.dev_private = sc;
	/* TODO: Set start/stop/pause/resume callbacks for FireWire transfers */
	
	unit = device_get_unit(dev);
	
	/* Create ALSA sound card */
	err = snd_card_new(NULL, unit, "DICE", NULL, 0, &card);
	if (err != 0) {
		device_printf(dev, "Failed to create ALSA card: %d\n", err);
		goto fail;
	}
	
	strlcpy(card->driver, "basound_dice", sizeof(card->driver));
	strlcpy(card->shortname, "DICE FireWire", sizeof(card->shortname));
	snprintf(card->longname, sizeof(card->longname),
		"DICE FireWire audio interface");
	
	/* Create PCM device */
	err = snd_pcm_new(card, "DICE Audio", 0, 1, 1, &pcm);
	if (err != 0) {
		device_printf(dev, "Failed to create PCM device: %d\n", err);
		snd_card_free(card);
		goto fail;
	}
	
	pcm->private_data = sc;
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &dice_pcm_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &dice_pcm_ops);
	
	/* Create MIDI device */
	err = snd_rawmidi_new(card, "DICE MIDI", 0, 1, 1, NULL);
	if (err != 0) {
		device_printf(dev, "Failed to create MIDI device: %d\n", err);
		/* MIDI is optional, continue without it */
	}
	
	/* Register with sound system */
	err = snd_card_register(card);
	if (err != 0) {
		device_printf(dev, "Failed to register ALSA card: %d\n", err);
		snd_card_free(card);
		goto fail;
	}
	
	device_printf(dev, "DICE FireWire audio device attached\n");
	return 0;
	
fail:
	audio_stream_destroy(&sc->playback_stream);
	audio_stream_destroy(&sc->capture_stream);
	return ENXIO;
}

static int
dice_bsd_detach(device_t dev)
{
	struct dice_bsd_softc *sc = device_get_softc(dev);
	
	if (sc != NULL) {
		audio_stream_destroy(&sc->playback_stream);
		audio_stream_destroy(&sc->capture_stream);
	}
	
	return 0;
}

static device_method_t dice_bsd_methods[] = {
	DEVMETHOD(device_probe,		dice_bsd_probe),
	DEVMETHOD(device_attach,	dice_bsd_attach),
	DEVMETHOD(device_detach,	dice_bsd_detach),
	DEVMETHOD_END
};

static driver_t dice_bsd_driver = {
	"basound_dice",
	dice_bsd_methods,
	sizeof(struct firewire_dev_comm),
};

DRIVER_MODULE(basound_dice, firewire, dice_bsd_driver, 0, 0);
MODULE_DEPEND(basound_dice, basound, 1, 1, 1);
MODULE_DEPEND(basound_dice, firewire, 1, 1, 1);
MODULE_DEPEND(basound_dice, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
