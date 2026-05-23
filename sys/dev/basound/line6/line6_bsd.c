/*-
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * FreeBSD Line6 USB Audio Driver
 * Bridges Line6 USB audio devices to FreeBSD sound(4) system
 *
 * Supported Devices:
 * - POD (USB ID 0E41:4750)
 * - POD XT (USB ID 0E41:4753)
 * - POD XT Live (USB ID 0E41:4642)
 * - Bass POD XT (USB ID 0E41:4750)
 * - POD HD (USB ID 0E41:5057)
 * - TonePort (USB ID 0E41:4154, 0E41:4159)
 * - Variax (USB ID 0E41:4756)
 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>
#include <dev/usb/usbhid.h>
#include <dev/sound/pcm/sound.h>

#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/rawmidi.h>
#include "../audio_stream.h"

/* Line6 USB driver - bridges FreeBSD usb_device to ALSA Line6 driver */

MALLOC_DECLARE(M_ALSA);

/* Line6 vendor ID */
#define LINE6_VENDOR_ID		0x0e41

/* Line6 device product IDs */
#define LINE6_PRODUCT_POD	0x4750
#define LINE6_PRODUCT_PODXT	0x4753
#define LINE6_PRODUCT_POD_XT_LIVE 0x4642
#define LINE6_PRODUCT_BASS_POD_XT 0x4050
#define LINE6_PRODUCT_PODHD300	0x5057
#define LINE6_PRODUCT_PODHD400	0x5058
#define LINE6_PRODUCT_PODHD500	0x5073
#define LINE6_PRODUCT_PODSTUDIO_UX1 0x4150
#define LINE6_PRODUCT_PODSTUDIO_UX2 0x4151
#define LINE6_PRODUCT_TONEPORT_UX1 0x4141
#define LINE6_PRODUCT_TONEPORT_UX2 0x4142
#define LINE6_PRODUCT_TONEPORT_GX 0x4147
#define LINE6_PRODUCT_VARIAX	0x4756

/* Device capability flags */
#define LINE6_CAP_CONTROL	0x0001  /* Device has MIDI control */
#define LINE6_CAP_AUDIO_IN	0x0002  /* Device has audio input */
#define LINE6_CAP_AUDIO_OUT	0x0004  /* Device has audio output */
#define LINE6_CAP_MIDI		0x0008  /* Device has MIDI I/O */
#define LINE6_CAP_FIRMWARE	0x0010  /* Device supports firmware updates */

struct line6_device_info {
	uint16_t product_id;
	const char *name;
	const char *card_id;
	unsigned int capabilities;
};

/* Line6 device database */
static const struct line6_device_info line6_devices[] = {
	{
		.product_id = LINE6_PRODUCT_POD,
		.name = "Line6 POD",
		.card_id = "Line6POD",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_AUDIO_IN | 
				LINE6_CAP_AUDIO_OUT | LINE6_CAP_MIDI | 
				LINE6_CAP_FIRMWARE
	},
	{
		.product_id = LINE6_PRODUCT_PODXT,
		.name = "Line6 POD XT",
		.card_id = "Line6PODXT",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_AUDIO_IN | 
				LINE6_CAP_AUDIO_OUT | LINE6_CAP_MIDI | 
				LINE6_CAP_FIRMWARE
	},
	{
		.product_id = LINE6_PRODUCT_POD_XT_LIVE,
		.name = "Line6 POD XT Live",
		.card_id = "Line6PODXTLive",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_AUDIO_IN | 
				LINE6_CAP_AUDIO_OUT | LINE6_CAP_MIDI | 
				LINE6_CAP_FIRMWARE
	},
	{
		.product_id = LINE6_PRODUCT_BASS_POD_XT,
		.name = "Line6 Bass POD XT",
		.card_id = "Line6BassPODXT",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_AUDIO_IN | 
				LINE6_CAP_AUDIO_OUT | LINE6_CAP_MIDI | 
				LINE6_CAP_FIRMWARE
	},
	{
		.product_id = LINE6_PRODUCT_PODHD300,
		.name = "Line6 POD HD300",
		.card_id = "Line6PODHD300",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_AUDIO_IN | 
				LINE6_CAP_AUDIO_OUT | LINE6_CAP_MIDI | 
				LINE6_CAP_FIRMWARE
	},
	{
		.product_id = LINE6_PRODUCT_PODHD400,
		.name = "Line6 POD HD400",
		.card_id = "Line6PODHD400",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_AUDIO_IN | 
				LINE6_CAP_AUDIO_OUT | LINE6_CAP_MIDI | 
				LINE6_CAP_FIRMWARE
	},
	{
		.product_id = LINE6_PRODUCT_PODHD500,
		.name = "Line6 POD HD500",
		.card_id = "Line6PODHD500",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_AUDIO_IN | 
				LINE6_CAP_AUDIO_OUT | LINE6_CAP_MIDI | 
				LINE6_CAP_FIRMWARE
	},
	{
		.product_id = LINE6_PRODUCT_PODSTUDIO_UX1,
		.name = "Line6 POD Studio UX1",
		.card_id = "Line6PODStudioUX1",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_AUDIO_IN | 
				LINE6_CAP_AUDIO_OUT | LINE6_CAP_MIDI
	},
	{
		.product_id = LINE6_PRODUCT_PODSTUDIO_UX2,
		.name = "Line6 POD Studio UX2",
		.card_id = "Line6PODStudioUX2",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_AUDIO_IN | 
				LINE6_CAP_AUDIO_OUT | LINE6_CAP_MIDI
	},
	{
		.product_id = LINE6_PRODUCT_TONEPORT_UX1,
		.name = "Line6 TonePort UX1",
		.card_id = "Line6TonePortUX1",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_AUDIO_IN | 
				LINE6_CAP_AUDIO_OUT | LINE6_CAP_MIDI
	},
	{
		.product_id = LINE6_PRODUCT_TONEPORT_UX2,
		.name = "Line6 TonePort UX2",
		.card_id = "Line6TonePortUX2",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_AUDIO_IN | 
				LINE6_CAP_AUDIO_OUT | LINE6_CAP_MIDI
	},
	{
		.product_id = LINE6_PRODUCT_TONEPORT_GX,
		.name = "Line6 TonePort GX",
		.card_id = "Line6TonePortGX",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_AUDIO_IN | 
				LINE6_CAP_AUDIO_OUT | LINE6_CAP_MIDI
	},
	{
		.product_id = LINE6_PRODUCT_VARIAX,
		.name = "Line6 Variax",
		.card_id = "Line6Variax",
		.capabilities = LINE6_CAP_CONTROL | LINE6_CAP_MIDI | 
				LINE6_CAP_FIRMWARE
	},
	{ 0, NULL, NULL, 0 }
};

struct line6_bsd_softc {
	device_t dev;
	struct device alsa_dev;	/* wrapper so card->dev stays valid after attach */
	struct usb_device *usbdev;
	usb_interface_descriptor_t *idesc;
	void *alsa_line6;
	unsigned int capabilities;
	const char *device_name;
};

MALLOC_DEFINE(M_LINE6_BSD, "line6_bsd", "Line6 BSD softc");

/* Find device info by product ID */
static const struct line6_device_info *
line6_bsd_find_device(uint16_t product_id)
{
	int i;

	for (i = 0; line6_devices[i].product_id != 0; i++) {
		if (line6_devices[i].product_id == product_id)
			return &line6_devices[i];
	}

	return NULL;
}

/* PCM callback stubs - implement basic audio stream handling */
static int
line6_pcm_open(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	
	if (runtime == NULL)
		return -ENOMEM;
	
	/* Set hardware constraints for playback/capture */
	runtime->hw.info = SNDRV_PCM_INFO_MMAP |
			   SNDRV_PCM_INFO_MMAP_VALID |
			   SNDRV_PCM_INFO_INTERLEAVED;
	/* Line6 devices typically support 16-bit and 24-bit formats */
	runtime->hw.formats = SNDRV_PCM_FMTBIT_S16_LE |
			      SNDRV_PCM_FMTBIT_S24_3LE;
	/* Line6 devices support 44.1kHz and 48kHz primarily */
	runtime->hw.rates = SNDRV_PCM_RATE_44100 |
			    SNDRV_PCM_RATE_48000;
	runtime->hw.rate_min = 44100;
	runtime->hw.rate_max = 48000;
	/* Most Line6 devices have 2 channels (stereo) */
	runtime->hw.channels_min = 1;
	runtime->hw.channels_max = 2;
	runtime->hw.buffer_bytes_max = 1 << 20; /* 1MB */
	runtime->hw.period_bytes_min = 64;
	runtime->hw.period_bytes_max = 1 << 16; /* 64KB */
	runtime->hw.periods_min = 2;
	runtime->hw.periods_max = 128;
	
	return 0;
}

static int
line6_pcm_close(struct snd_pcm_substream *substream)
{
	/* Clean up any stream-specific resources */
	return 0;
}

static int
line6_pcm_hw_params(struct snd_pcm_substream *substream, void *hw_params)
{
	/*
	 * DMA memory is already managed by basound_chan_init via sndbuf_alloc;
	 * runtime->dma_area/dma_addr/dma_bytes are set there.  Do not call
	 * snd_pcm_lib_malloc_pages here — that would overwrite private_data
	 * with a snd_dma_buffer pointer and corrupt the runtime state.
	 */
	return 0;
}

static int
line6_pcm_hw_free(struct snd_pcm_substream *substream)
{
	/* DMA buffers are owned by basound_chan_init/basound_chan_free */
	return 0;
}

static int
line6_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	
	if (runtime == NULL || runtime->dma_area == NULL)
		return -EINVAL;
	
	/* Reset playback/capture position */
	runtime->dma_position = 0;
	
	return 0;
}

static int
line6_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_stream *stream;
	
	if (runtime == NULL)
		return -EINVAL;
	
	/* Get audio_stream from private data if available */
	stream = (struct audio_stream *)runtime->private_data;
	
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* Start USB audio streaming with framework integration */
		if (stream) {
			audio_stream_start(stream);
			/* In real implementation:
			 * - Allocate USB isochronous or bulk URBs
			 * - Set up transfer buffers pointing to DMA area
			 * - Submit URBs to USB device for periodic transfers
			 * - Framework position tracking handles updates
			 */
		}
		runtime->state = SNDRV_PCM_STATE_RUNNING;
		runtime->dma_position = 0;
		return 0;
		
	case SNDRV_PCM_TRIGGER_STOP:
		/* Stop USB audio streaming */
		if (stream) {
			audio_stream_stop(stream);
			/* In real implementation:
			 * - Unlink all active URBs
			 * - Stop transfers from device
			 * - Free allocated URB buffers
			 * - Framework handles state cleanup
			 */
		}
		runtime->state = SNDRV_PCM_STATE_STOPPED;
		return 0;
		
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		if (stream)
			audio_stream_pause(stream);
		runtime->state = SNDRV_PCM_STATE_PAUSED;
		return 0;
		
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		if (stream)
			audio_stream_resume(stream);
		runtime->state = SNDRV_PCM_STATE_RUNNING;
		return 0;
		
	default:
		return -EINVAL;
	}
}

static unsigned long
line6_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct audio_stream *stream;
	unsigned long position = 0;
	
	if (runtime == NULL || runtime->dma_area == NULL)
		return 0;
	
	/* Get audio_stream from private data if available */
	stream = (struct audio_stream *)runtime->private_data;
	
	if (stream) {
		/* Get position from audio_stream framework */
		mtx_lock(&stream->lock);
		position = stream->position % runtime->dma_bytes;
		mtx_unlock(&stream->lock);
	} else {
		/* Fallback to runtime position if framework not available */
		position = runtime->dma_position % runtime->dma_bytes;
	}
	
	/* In real implementation:
	 * - Read current frame number from USB device
	 * - Map to DMA buffer position via framework
	 * - Call audio_stream_update_position() on URB completion
	 * - Return wrapped position within buffer
	 */
	return position;
}

static const struct snd_pcm_ops line6_pcm_ops = {
	.open = line6_pcm_open,
	.close = line6_pcm_close,
	.ioctl = NULL,
	.hw_params = line6_pcm_hw_params,
	.hw_free = line6_pcm_hw_free,
	.prepare = line6_pcm_prepare,
	.trigger = line6_pcm_trigger,
	.pointer = line6_pcm_pointer,
};

/* USB device probe routine */
static int
line6_bsd_probe(device_t dev)
{
	struct usb_attach_arg *uaa;
	const struct line6_device_info *info;

	uaa = device_get_ivars(dev);
	if (uaa == NULL)
		return ENXIO;

	/* Check for Line6 vendor ID */
	if (USB_GET_DRIVER_INFO(uaa) == 0) {
		if (uaa->info.idVendor != LINE6_VENDOR_ID)
			return ENXIO;
	}

	/* Find device in database */
	info = line6_bsd_find_device(uaa->info.idProduct);
	if (info == NULL)
		return ENXIO;

	return BUS_PROBE_DEFAULT;
}

/* USB device attach routine */
static int
line6_bsd_attach(device_t dev)
{
	struct usb_attach_arg *uaa;
	struct line6_bsd_softc *sc;
	const struct line6_device_info *info;
	struct snd_card *card;
	struct snd_pcm *pcm;
	int err;

	uaa = device_get_ivars(dev);
	if (uaa == NULL)
		return ENXIO;

	/* Find device info */
	info = line6_bsd_find_device(uaa->info.idProduct);
	if (info == NULL)
		return ENXIO;

	sc = device_get_softc(dev);
	if (sc == NULL)
		return ENXIO;

	sc->dev = dev;
	sc->usbdev = uaa->device;
	sc->idesc = usbd_get_interface_descriptor(uaa->iface);
	if (sc->idesc == NULL) {
		device_printf(dev, "Failed to get interface descriptor\n");
		return ENXIO;
	}

	sc->capabilities = info->capabilities;
	sc->device_name = info->name;
	sc->alsa_dev.bsddev = dev;

	device_printf(dev, "Probing %s (USB %04x:%04x)\n",
		      info->name, uaa->info.idVendor, uaa->info.idProduct);

	/* Create ALSA sound card */
	err = snd_card_new(&sc->alsa_dev, 0, info->card_id, NULL, 0, &card);
	if (err < 0 || card == NULL) {
		device_printf(dev, "Failed to create sound card: %d\n", err);
		return ENXIO;
	}

	/* Set card properties */
	snprintf(card->driver, sizeof(card->driver), "line6_bsd");
	snprintf(card->shortname, sizeof(card->shortname), "%s", info->name);
	snprintf(card->longname, sizeof(card->longname),
		 "%s at USB", info->name);

	/* Create PCM device for audio I/O */
	if (info->capabilities & (LINE6_CAP_AUDIO_IN | LINE6_CAP_AUDIO_OUT)) {
		int playback_count = (info->capabilities & LINE6_CAP_AUDIO_OUT) ? 1 : 0;
		int capture_count = (info->capabilities & LINE6_CAP_AUDIO_IN) ? 1 : 0;

		err = snd_pcm_new(card, info->card_id, 0, 
				  playback_count, capture_count, &pcm);
		if (err < 0 || pcm == NULL) {
			device_printf(dev, "Failed to create PCM device: %d\n", err);
			snd_card_free(card);
			return ENXIO;
		}

		/* Set PCM operations */
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &line6_pcm_ops);
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &line6_pcm_ops);

		snprintf(pcm->name, sizeof(pcm->name), "%s", info->name);
	}

	/* Create MIDI device for instrument control */
	if (info->capabilities & LINE6_CAP_MIDI) {
		struct snd_rawmidi *rmidi;

		err = snd_rawmidi_new(card, "Line6 MIDI", 0, 1, 1, &rmidi);
		if (err < 0) {
			device_printf(dev, "Failed to create MIDI device: %d\n", err);
			/* MIDI creation failure is non-fatal */
		}
	}

	/* Register sound card with FreeBSD */
	err = snd_card_register(card);
	if (err < 0) {
		device_printf(dev, "Failed to register sound card: %d\n", err);
		snd_card_free(card);
		return ENXIO;
	}

	sc->alsa_line6 = card;

	device_printf(dev, "Line6 device attached - %s registered\n", info->name);

	return 0;
}

static int
line6_bsd_detach(device_t dev)
{
	struct line6_bsd_softc *sc;
	struct snd_card *card;

	sc = device_get_softc(dev);
	if (sc == NULL)
		return 0;

	if (sc->alsa_line6 != NULL) {
		card = (struct snd_card *)sc->alsa_line6;
		snd_card_free(card);
	}

	return 0;
}

static device_method_t line6_bsd_methods[] = {
	DEVMETHOD(device_probe,		line6_bsd_probe),
	DEVMETHOD(device_attach,	line6_bsd_attach),
	DEVMETHOD(device_detach,	line6_bsd_detach),
	/* Bus methods needed to host the "pcm" child device */
	DEVMETHOD(bus_add_child,	bus_generic_add_child),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD_END
};

static driver_t line6_bsd_driver = {
	"basound_line6",
	line6_bsd_methods,
	sizeof(struct line6_bsd_softc),
};

DRIVER_MODULE(basound_line6, uhub, line6_bsd_driver, 0, 0);
MODULE_DEPEND(basound_line6, basound, 1, 1, 1);
MODULE_DEPEND(basound_line6, usb, 1, 1, 1);
MODULE_DEPEND(basound_line6, sound, SOUND_MINVER, SOUND_PREFVER, SOUND_MAXVER);
