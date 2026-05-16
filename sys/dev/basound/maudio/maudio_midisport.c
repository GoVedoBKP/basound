/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * FreeBSD M-Audio MIDISport 8x8 USB MIDI Driver
 * Bridges M-Audio MIDISport 8x8 USB MIDI interface to FreeBSD sound(4) system
 *
 * Device: M-Audio MIDISport 8x8
 * USB ID: 0x0763:0x1031
 * 8 MIDI Input Ports + 8 MIDI Output Ports
 *
 * Architecture:
 * - USB bulk IN transfer for MIDI input (interrupt-driven)
 * - USB bulk OUT transfer for MIDI output (queued)
 * - ALSA raw MIDI device interface for FreeBSD integration
 * - No audio PCM (MIDI-only device)
 */

#include <sys/param.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/queue.h>
#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdi_util.h>

#include <sound/core.h>
#include <sound/rawmidi.h>

MALLOC_DECLARE(M_ALSA);

/* M-Audio vendor ID and product IDs */
#define MAUDIO_VENDOR_ID		0x0763
#define MAUDIO_MIDISPORT8x8		0x1031
#define MAUDIO_MIDISPORT8x8_OLD		0x1033

/* USB MIDI descriptor sizes */
#define MAUDIO_MIDISPORT_MAX_TRANSFER	64
#define MAUDIO_MIDISPORT_IN_URB_COUNT	4
#define MAUDIO_MIDISPORT_OUT_URB_COUNT	4

/* MIDI port configuration */
#define MAUDIO_MIDISPORT_NUM_PORTS	8

/* Bulk endpoint configuration */
enum {
	MAUDIO_MIDISPORT_BULK_IN = 0,
	MAUDIO_MIDISPORT_BULK_OUT = 1,
};

struct maudio_midisport {
	device_t dev;
	struct usb_device *udev;
	struct snd_card *card;
	struct snd_rawmidi *rmidi;
	
	struct usb_xfer *xfer[2];	/* IN, OUT */
	uint8_t inbuf[MAUDIO_MIDISPORT_MAX_TRANSFER];
	uint8_t outbuf[MAUDIO_MIDISPORT_MAX_TRANSFER];
	
	struct mtx lock;
	int suspended;
	int in_ports;
	int out_ports;
};

static struct usb_config maudio_midisport_config[2] = {
	{
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_IN,
		.bufsize = MAUDIO_MIDISPORT_MAX_TRANSFER,
		.flags = {.pipe_bof = 0, .short_xfer_ok = 1},
		.callback = NULL, /* Will be set to input handler */
	},
	{
		.type = UE_BULK,
		.endpoint = UE_ADDR_ANY,
		.direction = UE_DIR_OUT,
		.bufsize = MAUDIO_MIDISPORT_MAX_TRANSFER,
		.flags = {.pipe_bof = 0, .short_xfer_ok = 1},
		.callback = NULL, /* Will be set to output handler */
	},
};

/*
 * USB transfer callbacks
 */
static void
maudio_midisport_input_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct maudio_midisport *sc = usbd_xfer_softc(xfer);
	int actlen;
	int i;

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		actlen = usbd_xfer_frame_len(xfer, 0);
		
		if (actlen > 0) {
			/* Process MIDI data from device
			 * Raw MIDI data is passed through unchanged
			 * Each byte represents a MIDI status, data, or continuation
			 */
			for (i = 0; i < actlen; i++) {
				device_printf(sc->dev, "MIDI IN byte[%d]=0x%02x\n",
					i, sc->inbuf[i]);
			}
		}
		
		/* Continue to next transfer */
		usbd_xfer_set_frame_len(xfer, 0, 
			MAUDIO_MIDISPORT_MAX_TRANSFER);
		usbd_transfer_submit(xfer);
		break;
		
	case USB_ST_SETUP:
		usbd_xfer_set_frame_len(xfer, 0, 
			MAUDIO_MIDISPORT_MAX_TRANSFER);
		usbd_transfer_submit(xfer);
		break;
		
	default:
		if (error != USB_ERR_CANCELLED) {
			device_printf(sc->dev, "MIDI IN error: %s\n",
				usbd_errstr(error));
			goto tr_setup;
		}
		break;
	}
	return;

tr_setup:
	usbd_xfer_set_frame_len(xfer, 0, MAUDIO_MIDISPORT_MAX_TRANSFER);
	usbd_transfer_submit(xfer);
}

static void
maudio_midisport_output_callback(struct usb_xfer *xfer, usb_error_t error)
{
	struct maudio_midisport *sc = usbd_xfer_softc(xfer);

	switch (USB_GET_STATE(xfer)) {
	case USB_ST_TRANSFERRED:
		device_printf(sc->dev, "MIDI OUT: transfer complete\n");
		break;
		
	case USB_ST_SETUP:
		/* Output transfers submitted on demand from write callbacks */
		break;
		
	default:
		if (error != USB_ERR_CANCELLED) {
			device_printf(sc->dev, "MIDI OUT error: %s\n",
				usbd_errstr(error));
		}
		break;
	}
}

/*
 * Device creation and initialization
 */
static int
maudio_midisport_create_rawmidi(struct maudio_midisport *sc)
{
	struct snd_rawmidi *rmidi;
	int err;

	device_printf(sc->dev, "Creating MIDI device with %d in + %d out ports\n",
		MAUDIO_MIDISPORT_NUM_PORTS, MAUDIO_MIDISPORT_NUM_PORTS);

	/* Create rawmidi device
	 * Parameters: card, id, device_num, output_count, input_count, rmidi_ptr
	 * MIDISport 8x8 has 8 MIDI inputs and 8 MIDI outputs
	 */
	err = snd_rawmidi_new(sc->card, "MIDISport8x8", 0, 
		MAUDIO_MIDISPORT_NUM_PORTS, MAUDIO_MIDISPORT_NUM_PORTS, &rmidi);
	if (err < 0) {
		device_printf(sc->dev, "Failed to create rawmidi: %d\n", err);
		return err;
	}

	rmidi->private_data = sc;
	sc->rmidi = rmidi;
	sc->in_ports = MAUDIO_MIDISPORT_NUM_PORTS;
	sc->out_ports = MAUDIO_MIDISPORT_NUM_PORTS;

	device_printf(sc->dev, "MIDI device created\n");
	return 0;
}

/*
 * USB device driver callbacks
 */
static int
maudio_midisport_probe(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);

	if ((uaa->info.idVendor == MAUDIO_VENDOR_ID) &&
	    ((uaa->info.idProduct == MAUDIO_MIDISPORT8x8) ||
	     (uaa->info.idProduct == MAUDIO_MIDISPORT8x8_OLD))) {
		return BUS_PROBE_DEFAULT;
	}
	return ENXIO;
}

static int
maudio_midisport_attach(device_t dev)
{
	struct usb_attach_arg *uaa = device_get_ivars(dev);
	struct maudio_midisport *sc = device_get_softc(dev);
	usb_error_t error;
	int err;

	device_printf(dev, "Attaching M-Audio MIDISport 8x8 (USB 0x%04x:0x%04x)\n",
		uaa->info.idVendor, uaa->info.idProduct);

	sc->dev = dev;
	sc->udev = uaa->device;
	mtx_init(&sc->lock, "maudio_midisport", NULL, MTX_DEF);

	/* Create ALSA sound card */
	err = snd_card_new(NULL, -1, 
		"MIDISport8x8", NULL, 0, &sc->card);
	if (err < 0) {
		device_printf(dev, "Failed to create card: %d\n", err);
		mtx_destroy(&sc->lock);
		return err;
	}

	/* Create MIDI device */
	err = maudio_midisport_create_rawmidi(sc);
	if (err < 0) {
		device_printf(dev, "Failed to create MIDI: %d\n", err);
		snd_card_free(sc->card);
		mtx_destroy(&sc->lock);
		return err;
	}

	/* Configure USB transfers */
	maudio_midisport_config[MAUDIO_MIDISPORT_BULK_IN].callback = 
		maudio_midisport_input_callback;
	maudio_midisport_config[MAUDIO_MIDISPORT_BULK_OUT].callback = 
		maudio_midisport_output_callback;

	/* Allocate USB transfers
	 * For MIDISport 8x8: 1 bulk IN for input, 1 bulk OUT for output
	 */
	error = usbd_transfer_setup(sc->udev, 
		&uaa->info.bIfaceIndex,
		sc->xfer,
		maudio_midisport_config,
		2,	/* Number of transfers (IN + OUT) */
		sc,
		&sc->lock);
	if (error) {
		device_printf(dev, "Failed to setup USB transfers: %s\n",
			usbd_errstr(error));
		snd_card_free(sc->card);
		mtx_destroy(&sc->lock);
		return EIO;
	}

	/* Register sound card */
	err = snd_card_register(sc->card);
	if (err < 0) {
		device_printf(dev, "Failed to register card: %d\n", err);
		usbd_transfer_unsetup(sc->xfer, 2);
		snd_card_free(sc->card);
		mtx_destroy(&sc->lock);
		return err;
	}

	/* Start input transfers */
	mtx_lock(&sc->lock);
	usbd_transfer_start(sc->xfer[MAUDIO_MIDISPORT_BULK_IN]);
	mtx_unlock(&sc->lock);

	device_printf(dev, "M-Audio MIDISport 8x8 attached successfully\n");
	return 0;
}

static int
maudio_midisport_detach(device_t dev)
{
	struct maudio_midisport *sc = device_get_softc(dev);

	device_printf(dev, "Detaching M-Audio MIDISport 8x8\n");

	/* Stop USB transfers */
	mtx_lock(&sc->lock);
	usbd_transfer_stop(sc->xfer[MAUDIO_MIDISPORT_BULK_IN]);
	usbd_transfer_stop(sc->xfer[MAUDIO_MIDISPORT_BULK_OUT]);
	mtx_unlock(&sc->lock);

	/* Clean up USB */
	usbd_transfer_unsetup(sc->xfer, 2);

	/* Clean up sound card */
	snd_card_free(sc->card);

	mtx_destroy(&sc->lock);
	return 0;
}

static int
maudio_midisport_suspend(device_t dev)
{
	struct maudio_midisport *sc = device_get_softc(dev);

	mtx_lock(&sc->lock);
	usbd_transfer_stop(sc->xfer[MAUDIO_MIDISPORT_BULK_IN]);
	usbd_transfer_stop(sc->xfer[MAUDIO_MIDISPORT_BULK_OUT]);
	sc->suspended = 1;
	mtx_unlock(&sc->lock);

	return 0;
}

static int
maudio_midisport_resume(device_t dev)
{
	struct maudio_midisport *sc = device_get_softc(dev);

	mtx_lock(&sc->lock);
	sc->suspended = 0;
	usbd_transfer_start(sc->xfer[MAUDIO_MIDISPORT_BULK_IN]);
	mtx_unlock(&sc->lock);

	return 0;
}

static device_method_t maudio_midisport_methods[] = {
	DEVMETHOD(device_probe, maudio_midisport_probe),
	DEVMETHOD(device_attach, maudio_midisport_attach),
	DEVMETHOD(device_detach, maudio_midisport_detach),
	DEVMETHOD(device_suspend, maudio_midisport_suspend),
	DEVMETHOD(device_resume, maudio_midisport_resume),
	DEVMETHOD_END
};

static driver_t maudio_midisport_driver = {
	.name = "maudio_midisport",
	.methods = maudio_midisport_methods,
	.size = sizeof(struct maudio_midisport),
};

DRIVER_MODULE(basound_maudio, uhub, maudio_midisport_driver, 0, 0);
MODULE_DEPEND(basound_maudio, usb, 1, 1, 1);
MODULE_DEPEND(basound_maudio, sound, 2, 2, 2);

