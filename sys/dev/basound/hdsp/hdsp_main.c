#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <dev/pci/pcivar.h>

#include "hdsp.h"

MALLOC_DECLARE(M_ALSA);

/* RME HDSP PCI IDs */
#define PCI_DEVICE_ID_RME_DIGIFACE	0x3fc5
#define PCI_DEVICE_ID_RME_MULTIFACE	0x3fc6
#define PCI_DEVICE_ID_RME_H9652		0x3fc4

static int
hdsp_get_iobox_version(struct hdsp *hdsp)
{
	uint32_t status = hdsp_read(hdsp, HDSP_statusRegister);

	if (status & 0x100)
		return Multiface;
	else
		return Digiface;
}

int
snd_hdsp_create(struct snd_card *card, struct hdsp *hdsp)
{
	struct snd_pcm *pcm;
	int err, i;

	hdsp->card = card;
	hdsp->pci = (struct pci_dev *)card->dev; /* Our shim uses card->dev for pci_dev */
	hdsp->iobase = (void *)pci_resource_start(hdsp->pci, 0);

	mtx_init(&hdsp->lock, "hdsp_lock", NULL, MTX_DEF);
	
	/* Identify card type */
	if (hdsp->pci->device == PCI_DEVICE_ID_RME_DIGIFACE ||
	    hdsp->pci->device == PCI_DEVICE_ID_RME_MULTIFACE) {
		hdsp->io_type = hdsp_get_iobox_version(hdsp);
	} else if (hdsp->pci->device == PCI_DEVICE_ID_RME_H9652) {
		hdsp->io_type = H9652;
	}

	switch (hdsp->io_type) {
	case Digiface:
		hdsp->card_name = "RME Digiface";
		hdsp->ss_in_channels = 26;
		hdsp->ss_out_channels = 26;
		break;
	case Multiface:
		hdsp->card_name = "RME Multiface";
		hdsp->ss_in_channels = 18;
		hdsp->ss_out_channels = 18;
		break;
	case H9652:
		hdsp->card_name = "RME Hammerfall DSP 9652";
		hdsp->ss_in_channels = 26;
		hdsp->ss_out_channels = 26;
		break;
	default:
		hdsp->card_name = "RME HDSP Unknown";
		break;
	}

	strlcpy(card->driver, "hdsp", sizeof(card->driver));
	strlcpy(card->shortname, hdsp->card_name, sizeof(card->shortname));

	/* Create PCM device */
	err = snd_pcm_new(card, hdsp->card_name, 0, 1, 1, &pcm);
	if (err)
		return err;

	pcm->private_data = hdsp;
	hdsp->pcm = pcm;
	hdsp->playback_substream = pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream;
	hdsp->capture_substream = pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream;

	/* Initialize MIDI */
	for (i = 0; i < 2; i++) {
		hdsp->midi[i].hdsp = hdsp;
		hdsp->midi[i].id = i;
		mtx_init(&hdsp->midi[i].lock, "hdsp_midi_lock", NULL, MTX_DEF);
	}
	INIT_WORK(&hdsp->midi_work, (void *)snd_hdsp_midi_work);

	snd_hdsp_create_mixer(card, hdsp);

	dev_info(card->dev, "Found %s at 0x%jx", hdsp->card_name, (uintmax_t)hdsp->iobase);

	return 0;
}

void
snd_hdsp_interrupt(void *arg)
{
	struct hdsp *hdsp = arg;
	uint32_t status;
	int audio;

	mtx_lock(&hdsp->lock);

	status = hdsp_read(hdsp, HDSP_statusRegister);
	audio = status & HDSP_audioIRQPending;

	if (!audio) {
		mtx_unlock(&hdsp->lock);
		return;
	}

	hdsp_write(hdsp, HDSP_interruptConfirmation, 0);

	if (!(hdsp->state & HDSP_InitializationComplete)) {
		mtx_unlock(&hdsp->lock);
		return;
	}

	if (audio) {
		/* Notify FreeBSD PCM channels */
		if (hdsp->capture_substream)
			snd_pcm_period_elapsed(hdsp->capture_substream);
		if (hdsp->playback_substream)
			snd_pcm_period_elapsed(hdsp->playback_substream);
	}
	
	/* MIDI handling */
	if (status & HDSP_midi0IRQPending) {
		/* Handle MIDI Port 0 */
		schedule_work(&hdsp->midi_work);
	}
	if (status & HDSP_midi1IRQPending) {
		/* Handle MIDI Port 1 */
		schedule_work(&hdsp->midi_work);
	}

	mtx_unlock(&hdsp->lock);
}

static int
hdsp_fifo_wait(struct hdsp *hdsp, int value, int timeout)
{
	int i;

	for (i = 0; i < timeout; i++) {
		if ((hdsp_read(hdsp, HDSP_fifoStatus) & 0xff) == value)
			return 0;
		DELAY(1000); /* 1ms delay */
	}
	return -EIO;
}

#define MINUS_INFINITY_GAIN 0

int
hdsp_read_gain(struct hdsp *hdsp, int addr)
{
	if (addr >= HDSP_MATRIX_MIXER_SIZE)
		return 0;
	return hdsp->mixer_matrix[addr];
}

static void
hdsp_set_dds_value(struct hdsp *hdsp, int rate)
{
	uint64_t n;

	if (rate >= 112000)
		rate /= 4;
	else if (rate >= 56000)
		rate /= 2;

	n = DDS_NUMERATOR;
	n = n / rate;
	
	hdsp->dds_value = (uint32_t)n;
	hdsp_write(hdsp, HDSP_freqReg, hdsp->dds_value);
}

int
hdsp_set_rate(struct hdsp *hdsp, int rate, int called_internally)
{
	/* Simplifying: assume master mode for now, as in skeleton */
	if (!(hdsp->control_register & HDSP_ClockModeMaster)) {
		return -1;
	}

	hdsp_set_dds_value(hdsp, rate);
	hdsp->system_sample_rate = rate;
	
	return 0;
}

int
snd_hdsp_upload_firmware(struct hdsp *hdsp)
{
	const struct firmware *fw;
	int i;
	const uint32_t *cache;
	int err;

	err = request_firmware(&fw, "rme_hdsp.bin", (struct device *)hdsp->pci);
	if (err) {
		dev_err(hdsp->card->dev, "Could not load firmware");
		return err;
	}

	cache = (const uint32_t *)fw->data;

	dev_info(hdsp->card->dev, "loading firmware");

	hdsp_write(hdsp, HDSP_control2Reg, HDSP_S_PROGRAM);
	hdsp_write(hdsp, HDSP_fifoData, 0);

	if (hdsp_fifo_wait(hdsp, 0, HDSP_LONG_WAIT)) {
		dev_err(hdsp->card->dev, "timeout waiting for download preparation");
		hdsp_write(hdsp, HDSP_control2Reg, HDSP_S200);
		release_firmware(fw);
		return -EIO;
	}

	hdsp_write(hdsp, HDSP_control2Reg, HDSP_S_LOAD);

	for (i = 0; i < fw->size / 4; ++i) {
		hdsp_write(hdsp, HDSP_fifoData, cache[i]);
		if (hdsp_fifo_wait(hdsp, 127, HDSP_LONG_WAIT)) {
			dev_err(hdsp->card->dev, "timeout during firmware loading");
			hdsp_write(hdsp, HDSP_control2Reg, HDSP_S200);
			release_firmware(fw);
			return -EIO;
		}
	}

	hdsp_fifo_wait(hdsp, 3, HDSP_LONG_WAIT);
	hdsp_write(hdsp, HDSP_control2Reg, HDSP_S200);

	/* Wait for FPGA to initialize */
	pause("hdspfw", 3 * hz);

	dev_info(hdsp->card->dev, "finished firmware loading");
	release_firmware(fw);
	hdsp->state |= HDSP_InitializationComplete;

	return 0;
}

int
snd_hdsp_hw_params(struct snd_pcm_substream *substream, void *hw_params)
{
	return snd_pcm_lib_malloc_pages(substream, 4096 * 1024); /* 4MB buffer */
}

int
snd_hdsp_prepare(struct snd_pcm_substream *substream)
{
	/* Prepare hardware for playback/capture */
	return 0;
}

static unsigned int
hdsp_hw_pointer(struct hdsp *hdsp)
{
	int position;

	position = hdsp_read(hdsp, HDSP_statusRegister);

	/* Assume precise_ptr is always true for now */
	position &= HDSP_BufferPositionMask;
	position /= 4;
	position &= (hdsp->period_bytes/2) - 1;
	return position;
}

int
snd_hdsp_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct hdsp *hdsp = (struct hdsp *)substream->pcm->private_data;

	if (hdsp == NULL)
		return -EINVAL;

	mtx_lock(&hdsp->lock);
	
	switch (cmd) {
	case 1: /* SNDRV_PCM_TRIGGER_START */
		/* Start HDSP hardware streaming */
		if (hdsp->running == 0) {
			/* Enable output */
			uint32_t control = hdsp_read(hdsp, HDSP_controlRegister);
			hdsp_write(hdsp, HDSP_controlRegister, 
				   control | HDSP_AudioInterruptEnable);
			
			/* Set stream as running */
			hdsp->running = 1;
			
			/* In real implementation:
			 * - Configure DMA addresses in HDSP_outputBufferAddress, HDSP_inputBufferAddress
			 * - Set up period size and frame count
			 * - Enable isochronous transfers
			 */
		}
		mtx_unlock(&hdsp->lock);
		return 0;
		
	case 0: /* SNDRV_PCM_TRIGGER_STOP */
		/* Stop HDSP hardware streaming */
		if (hdsp->running != 0) {
			/* Disable output */
			uint32_t control = hdsp_read(hdsp, HDSP_controlRegister);
			hdsp_write(hdsp, HDSP_controlRegister, 
				   control & ~HDSP_AudioInterruptEnable);
			
			/* Clear running flag */
			hdsp->running = 0;
			
			/* In real implementation:
			 * - Disable isochronous transfers
			 * - Reset DMA pointers
			 */
		}
		mtx_unlock(&hdsp->lock);
		return 0;
		
	default:
		mtx_unlock(&hdsp->lock);
		return -EINVAL;
	}
}

unsigned long
snd_hdsp_pointer(struct snd_pcm_substream *substream)
{
	struct hdsp *hdsp = (struct hdsp *)substream->pcm->private_data;
	unsigned long position = 0;

	if (hdsp == NULL)
		return 0;

	mtx_lock(&hdsp->lock);
	
	if (hdsp->running != 0) {
		/* Read current buffer position from HDSP hardware */
		uint32_t status = hdsp_read(hdsp, HDSP_statusRegister);
		
		/* Extract buffer position from status register */
		position = (status & HDSP_BufferPositionMask) >> 6;
		
		/* Convert to byte offset */
		if (hdsp->period_bytes > 0) {
			position = (position * hdsp->period_bytes) % 
				   (hdsp->period_bytes * 2);
		}
	}
	
	mtx_unlock(&hdsp->lock);
	return position;
}
