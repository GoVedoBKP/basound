// SPDX-License-Identifier: GPL-3.0-or-later
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <dev/pci/pcivar.h>

#include "hdsp.h"
#include "hdsp_fw.h"
#include <sound/pcm_params.h>
#include "alsa_pcm_bsd.h"

MALLOC_DECLARE(M_ALSA);

/* RME HDSP PCI IDs */
#define PCI_DEVICE_ID_RME_DIGIFACE	0x3fc5
#define PCI_DEVICE_ID_RME_MULTIFACE	0x3fc6
#define PCI_DEVICE_ID_RME_H9652		0x3fc4

static int
hdsp_fifo_wait(struct hdsp *hdsp, int value, int timeout)
{
	int i;

	for (i = 0; i < timeout; i++) {
		if ((int)(hdsp_read(hdsp, HDSP_fifoStatus) & 0xff) <= value)
			return 0;
		DELAY(100); /* 100µs per iteration, matching Linux udelay(100) */
	}
	return -EIO;
}

/*
 * Detect whether the attached I/O box is a Multiface or a Digiface.
 *
 * Two paths depending on whether firmware is already loaded:
 *
 * 1. DllError set (firmware not loaded): perform a JTAG S_LOAD probe.
 *    The Digiface has an onboard ROM controller that drains the FIFO
 *    almost instantly; the Multiface has no such controller so the FIFO
 *    stalls.  Do NOT send VERSION_BIT — that activates the ROM firmware,
 *    clears DllError, and would prevent our bitstream from being uploaded.
 *
 * 2. DllError clear (firmware already active, e.g. driver reload):
 *    Read the I/O-box type from status2Register version bits.
 */
static void
hdsp_get_iobox_version(struct hdsp *hdsp)
{
	if ((hdsp_read(hdsp, HDSP_statusRegister) & HDSP_DllError) != 0) {
		/* Firmware not loaded — JTAG probe */
		hdsp_write(hdsp, HDSP_control2Reg, HDSP_S_LOAD);
		hdsp_write(hdsp, HDSP_fifoData, 0);

		if (hdsp_fifo_wait(hdsp, 0, HDSP_SHORT_WAIT) < 0) {
			hdsp_write(hdsp, HDSP_control2Reg, HDSP_S300);
			hdsp_write(hdsp, HDSP_control2Reg, HDSP_S_LOAD);
		}

		hdsp_write(hdsp, HDSP_control2Reg, HDSP_S200 | HDSP_PROGRAM);
		hdsp_write(hdsp, HDSP_fifoData, 0);
		if (hdsp_fifo_wait(hdsp, 0, HDSP_SHORT_WAIT) < 0)
			goto set_multi;

		hdsp_write(hdsp, HDSP_control2Reg, HDSP_S_LOAD);
		hdsp_write(hdsp, HDSP_fifoData, 0);
		if (hdsp_fifo_wait(hdsp, 0, HDSP_SHORT_WAIT) == 0) {
			hdsp->io_type = Digiface;
			dev_info(hdsp->card->dev, "Digiface detected\n");
			return;
		}

		hdsp_write(hdsp, HDSP_control2Reg, HDSP_S300);
		hdsp_write(hdsp, HDSP_control2Reg, HDSP_S_LOAD);
		hdsp_write(hdsp, HDSP_fifoData, 0);
		if (hdsp_fifo_wait(hdsp, 0, HDSP_SHORT_WAIT) == 0)
			goto set_multi;

		hdsp_write(hdsp, HDSP_control2Reg, HDSP_S300);
		hdsp_write(hdsp, HDSP_control2Reg, HDSP_S_LOAD);
		hdsp_write(hdsp, HDSP_fifoData, 0);
		if (hdsp_fifo_wait(hdsp, 0, HDSP_SHORT_WAIT) < 0)
			goto set_multi;

		/* RPM — treat as unsupported, fall through to Multiface */
		goto set_multi;
	} else {
		/* Firmware already loaded — read type from status2 bits */
		uint32_t status2 = hdsp_read(hdsp, HDSP_status2Register);

		if (status2 & HDSP_version1)
			hdsp->io_type = Multiface;
		else
			hdsp->io_type = Digiface;
		return;
	}

set_multi:
	hdsp->io_type = Multiface;
	dev_info(hdsp->card->dev, "Multiface detected\n");
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

	/* Seed the control register cache with master-clock mode so that
	 * hdsp_set_rate() works and hardware writes after firmware load
	 * don't clear critical configuration bits. */
	hdsp->control_register = HDSP_ClockModeMaster;

	/* Initialize audio_stream structures */
	mtx_init(&hdsp->capture_stream.lock, "hdsp_capture_stream", NULL, MTX_DEF);
	hdsp->capture_stream.state = AUDIO_STREAM_IDLE;
	
	mtx_init(&hdsp->playback_stream.lock, "hdsp_playback_stream", NULL, MTX_DEF);
	hdsp->playback_stream.state = AUDIO_STREAM_IDLE;
	
	/* Identify card type */
	if (hdsp->pci->device == PCI_DEVICE_ID_RME_DIGIFACE ||
	    hdsp->pci->device == PCI_DEVICE_ID_RME_MULTIFACE) {
		hdsp_get_iobox_version(hdsp);
	} else if (hdsp->pci->device == PCI_DEVICE_ID_RME_H9652) {
		hdsp->io_type = H9652;
	}

	switch (hdsp->io_type) {
	case Digiface:
		hdsp->card_name = "RME Digiface";
		hdsp->ss_in_channels = 26;
		hdsp->ss_out_channels = 26;
		hdsp->max_channels = 26;
		break;
	case Multiface:
		hdsp->card_name = "RME Multiface";
		hdsp->ss_in_channels = 18;
		hdsp->ss_out_channels = 18;
		hdsp->max_channels = 18;
		break;
	case H9652:
		hdsp->card_name = "RME Hammerfall DSP 9652";
		hdsp->ss_in_channels = 26;
		hdsp->ss_out_channels = 26;
		hdsp->max_channels = 26;
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

	/* Register trigger/pointer callbacks so basound_chan_trigger and
	 * basound_chan_getptr can reach our hardware implementation. */
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &hdsp_pcm_ops);
	snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE,  &hdsp_pcm_ops);

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
	int audio, midi0, midi1;

	mtx_lock(&hdsp->lock);

	status = hdsp_read(hdsp, HDSP_statusRegister);
	audio  = status & HDSP_audioIRQPending;
	midi0  = status & HDSP_midi0IRQPending;
	midi1  = status & HDSP_midi1IRQPending;

	if (!audio && !midi0 && !midi1) {
		mtx_unlock(&hdsp->lock);
		return;
	}

	/* Acknowledge the interrupt to the hardware before doing anything else. */
	hdsp_write(hdsp, HDSP_interruptConfirmation, 0);

	if (!(hdsp->state & HDSP_InitializationComplete)) {
		mtx_unlock(&hdsp->lock);
		return;
	}

	/*
	 * Release hdsp->lock BEFORE calling snd_pcm_period_elapsed.
	 * snd_pcm_period_elapsed → chn_intr acquires CHN_LOCK.
	 * basound_chan_trigger (called with CHN_LOCK held) acquires hdsp->lock.
	 * Holding hdsp->lock while trying to acquire CHN_LOCK would invert
	 * that ordering and deadlock on SMP.
	 */
	mtx_unlock(&hdsp->lock);

	if (audio) {
		if (hdsp->capture_substream)
			snd_pcm_period_elapsed(hdsp->capture_substream);
		if (hdsp->playback_substream)
			snd_pcm_period_elapsed(hdsp->playback_substream);
	}

	if (midi0 || midi1)
		schedule_work(&hdsp->midi_work);
}

int
hdsp_read_gain(struct hdsp *hdsp, int addr)
{
	if (addr >= HDSP_MATRIX_MIXER_SIZE)
		return 0;
	return hdsp->mixer_matrix[addr];
}

/*
 * Write a gain value into the HDSP's internal matrix mixer RAM.
 *
 * For Digiface / Multiface the hardware matrix mixer is accessed through
 * the same FIFO used for firmware upload.  Each write packs the 11-bit
 * matrix address into the upper 16 bits and the 16-bit gain value into
 * the lower 16 bits of the 32-bit FIFO word.
 *
 * UNITY_GAIN  = 0x8000 (32768) — 0 dB
 * MINUS_INFINITY_GAIN = 0x0000 — silence
 *
 * H9652 / H9632 use a different (memory-mapped) mechanism and are not
 * handled here; their firmware is in ROM and no upload is needed.
 */
int
hdsp_write_gain(struct hdsp *hdsp, unsigned int addr, unsigned short data)
{
	if (addr >= HDSP_MATRIX_MIXER_SIZE)
		return -1;

	if (hdsp_fifo_wait(hdsp, 127, HDSP_LONG_WAIT))
		return -1;

	hdsp_write(hdsp, HDSP_fifoData, (addr << 16) | data);
	hdsp->mixer_matrix[addr] = data;
	return 0;
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
snd_hdsp_enable_io(struct hdsp *hdsp)
{
	int i;

	if (hdsp_fifo_wait(hdsp, 0, 100)) {
		dev_err(hdsp->card->dev, "enable_io: FIFO not empty\n");
		return -EIO;
	}

	for (i = 0; i < hdsp->max_channels; ++i) {
		hdsp_write(hdsp, HDSP_inputEnable  + (4 * i), 1);
		hdsp_write(hdsp, HDSP_outputEnable + (4 * i), 1);
	}
	return 0;
}

/*
 * Initialize the hardware matrix mixer to a usable default state:
 *
 *  1. Silence all 2048 entries (MINUS_INFINITY_GAIN = 0).
 *  2. Route each playback channel N directly to output N at unity gain.
 *
 * Without step 2 the DMA playback buffers are never connected to the
 * physical outputs and no sound is produced, even if the DMA engine is
 * running correctly.
 */
static int
hdsp_init_mixer(struct hdsp *hdsp)
{
	int i, addr;

	/* Silence the entire matrix */
	for (i = 0; i < HDSP_MATRIX_MIXER_SIZE; ++i) {
		if (hdsp_write_gain(hdsp, i, MINUS_INFINITY_GAIN) < 0) {
			dev_err(hdsp->card->dev,
			    "mixer init: FIFO timeout at entry %d\n", i);
			return -EIO;
		}
	}

	/* Direct playback N → output N at 0 dB */
	for (i = 0; i < hdsp->max_channels; ++i) {
		addr = hdsp_playback_to_output_key(hdsp, i, i);
		if (hdsp_write_gain(hdsp, addr, UNITY_GAIN) < 0) {
			dev_err(hdsp->card->dev,
			    "mixer init: unity gain write failed ch %d\n", i);
			return -EIO;
		}
	}

	dev_info(hdsp->card->dev,
	    "mixer initialized: %d channels routed to outputs\n",
	    hdsp->max_channels);
	return 0;
}

int
snd_hdsp_upload_firmware(struct hdsp *hdsp)
{
	const uint32_t *fw_data;
	size_t fw_size;
	int i;
	int is_rev11;

	/*
	 * Select the correct firmware image based on the I/O box type
	 * and the PCI revision byte read during attach.
	 *
	 * Revision 0x11 (HDSP_PCI_REVISION_DSP_11) uses the "rev11"
	 * bitstream; all other DSP revisions use the original image.
	 */
	is_rev11 = (hdsp->firmware_rev == HDSP_PCI_REVISION_DSP_11);

	switch (hdsp->io_type) {
	case Digiface:
		fw_data = is_rev11 ? hdsp_firmware_digiface_rev11
				   : hdsp_firmware_digiface;
		fw_size = is_rev11 ? sizeof(hdsp_firmware_digiface_rev11)
				   : sizeof(hdsp_firmware_digiface);
		break;
	case Multiface:
		fw_data = is_rev11 ? hdsp_firmware_multiface_rev11
				   : hdsp_firmware_multiface;
		fw_size = is_rev11 ? sizeof(hdsp_firmware_multiface_rev11)
				   : sizeof(hdsp_firmware_multiface);
		break;
	default:
		/* H9652 and H9632 have their firmware in ROM; no upload needed */
		return 0;
	}

	dev_info(hdsp->card->dev, "loading firmware (%s rev%s)",
	    hdsp->card_name, is_rev11 ? "11" : "");

	hdsp_write(hdsp, HDSP_control2Reg, HDSP_S_PROGRAM);
	hdsp_write(hdsp, HDSP_fifoData, 0);

	if (hdsp_fifo_wait(hdsp, 0, HDSP_LONG_WAIT)) {
		dev_err(hdsp->card->dev, "timeout waiting for download preparation");
		hdsp_write(hdsp, HDSP_control2Reg, HDSP_S200);
		return -EIO;
	}

	hdsp_write(hdsp, HDSP_control2Reg, HDSP_S_LOAD);

	for (i = 0; i < (int)(fw_size / sizeof(uint32_t)); ++i) {
		hdsp_write(hdsp, HDSP_fifoData, fw_data[i]);
		if (hdsp_fifo_wait(hdsp, 127, HDSP_LONG_WAIT)) {
			dev_err(hdsp->card->dev, "timeout during firmware loading");
			hdsp_write(hdsp, HDSP_control2Reg, HDSP_S200);
			return -EIO;
		}
	}

	hdsp_fifo_wait(hdsp, 3, HDSP_LONG_WAIT);
	hdsp_write(hdsp, HDSP_control2Reg, HDSP_S200);

	/* Wait for FPGA to initialize */
	pause("hdspfw", 3 * hz);

	dev_info(hdsp->card->dev, "finished firmware loading");
	hdsp->state |= HDSP_InitializationComplete;

	/* Apply the full default control register (matching Linux snd_hdsp_set_defaults):
	 *   - HDSP_ClockModeMaster     : use internal clock
	 *   - HDSP_SPDIFInputCoaxial   : coaxial SPDIF input
	 *   - hdsp_encode_latency(7)   : hardware period = 2^(7+7) = 16384 samples
	 *   - HDSP_LineOut             : enable analog line output
	 * Program the DDS for 48 kHz immediately after so the hardware has a
	 * valid clock before any trigger call. */
	hdsp->control_register = HDSP_ClockModeMaster |
	                         HDSP_SPDIFInputCoaxial |
	                         hdsp_encode_latency(7) |
	                         HDSP_LineOut;
	hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	hdsp_set_rate(hdsp, 48000, 1);

	/* Enable input and output channels so the DACs/ADCs are active. */
	if (snd_hdsp_enable_io(hdsp) < 0) {
		dev_err(hdsp->card->dev, "failed to enable I/O channels\n");
		return -EIO;
	}

	/* Program the hardware matrix mixer: silence everything, then route
	 * playback channel N → output N at unity gain so that PCM playback
	 * is immediately audible without requiring hdspmixer configuration. */
	if (hdsp_init_mixer(hdsp) < 0) {
		dev_err(hdsp->card->dev, "failed to initialize mixer\n");
		return -EIO;
	}

	return 0;
}

const struct snd_pcm_ops hdsp_pcm_ops = {
	.hw_params = snd_hdsp_hw_params,
	.prepare   = snd_hdsp_prepare,
	.trigger   = snd_hdsp_trigger,
	.pointer   = snd_hdsp_pointer,
};

int
snd_hdsp_hw_params(struct snd_pcm_substream *substream, void *hw_params)
{
	struct hdsp *hdsp = (struct hdsp *)substream->pcm->private_data;
	struct basound_chan *ch = substream->private_data;
	unsigned int frames, channels;
	int latency;

	if (ch == NULL || ch->format == 0 || ch->blocksize == 0)
		return 0;

	/* Calculate frames per period using actual channel count. */
	channels = AFMT_CHANNEL(ch->format);
	if (channels == 0)
		channels = 2;

	frames = ch->blocksize / (channels * ((ch->format & AFMT_S32_LE) ? 4 : 2));

	/* Map requested frames to the nearest supported HDSP latency bits.
	 * 0: 64, 1: 128, 2: 256, 3: 512, 4: 1024, 5: 2048, 6: 4096, 7: 8192. */
	if (frames <= 64) latency = 0;
	else if (frames <= 128)  latency = 1;
	else if (frames <= 256)  latency = 2;
	else if (frames <= 512)  latency = 3;
	else if (frames <= 1024) latency = 4;
	else if (frames <= 2048) latency = 5;
	else if (frames <= 4096) latency = 6;
	else latency = 7;

	mtx_lock(&hdsp->lock);
	hdsp->control_register &= ~HDSP_LatencyMask;
	hdsp->control_register |= hdsp_encode_latency(latency);
	
	/* If already running, update the hardware immediately. */
	if (hdsp->running) {
		hdsp_write(hdsp, HDSP_controlRegister, hdsp->control_register);
	}
	mtx_unlock(&hdsp->lock);

	dev_info(hdsp->card->dev, "set latency to %d frames (%d bytes)\n",
	    1 << (latency + 6), ch->blocksize);

	return 0;
}

int
snd_hdsp_prepare(struct snd_pcm_substream *substream)
{
	struct hdsp *hdsp = (struct hdsp *)substream->pcm->private_data;
	struct basound_chan *ch = (struct basound_chan *)substream->private_data;

	if (ch != NULL && ch->speed > 0)
		hdsp_set_rate(hdsp, ch->speed, 0);

	return 0;
}
int
snd_hdsp_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct hdsp *hdsp = (struct hdsp *)substream->pcm->private_data;
	struct audio_stream *stream;
	int stream_dir;

	if (hdsp == NULL)
		return -EINVAL;

	/* Determine which stream (playback or capture) */
	stream_dir = substream->stream;
	stream = (stream_dir == SNDRV_PCM_STREAM_PLAYBACK) ? 
		 &hdsp->playback_stream : &hdsp->capture_stream;

	mtx_lock(&hdsp->lock);
	
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		/* Start HDSP hardware streaming */
		if (hdsp->running == 0) {
			/* Program DMA buffer addresses from substream runtimes.
			 * These are populated by basound_chan_init via sndbuf_alloc. */
			if (hdsp->playback_substream != NULL &&
			    hdsp->playback_substream->runtime != NULL &&
			    hdsp->playback_substream->runtime->dma_bytes > 0) {
				hdsp_write(hdsp, HDSP_outputBufferAddress,
					   (uint32_t)hdsp->playback_substream->runtime->dma_addr);
			}
			if (hdsp->capture_substream != NULL &&
			    hdsp->capture_substream->runtime != NULL &&
			    hdsp->capture_substream->runtime->dma_bytes > 0) {
				hdsp_write(hdsp, HDSP_inputBufferAddress,
					   (uint32_t)hdsp->capture_substream->runtime->dma_addr);
			}
			
			/* Enable audio interrupts and start DMA engine using
			 * the cached control register so no previously
			 * configured bits (e.g. ClockModeMaster, DDS rate)
			 * are lost.  HDSP_controlRegister is write-only on
			 * the hardware; reading it returns undefined data. */
			hdsp->control_register |=
			    HDSP_AudioInterruptEnable | HDSP_Start;
			hdsp_write(hdsp, HDSP_controlRegister,
				   hdsp->control_register);
			
			/* Update audio_stream framework state */
			audio_stream_start(stream);
			
			/* Set stream as running */
			hdsp->running = 1;
		}
		mtx_unlock(&hdsp->lock);
		return 0;
		
	case SNDRV_PCM_TRIGGER_STOP:
		/* Stop HDSP hardware streaming */
		if (hdsp->running != 0) {
			/* Update audio_stream framework state */
			audio_stream_stop(stream);
			
			/* Stop DMA engine and disable audio interrupts */
			hdsp->control_register &=
			    ~(HDSP_AudioInterruptEnable | HDSP_Start);
			hdsp_write(hdsp, HDSP_controlRegister,
				   hdsp->control_register);
			
			/* Reset DMA pointer to beginning of buffer */
			hdsp_write(hdsp, HDSP_resetPointer, 0);
			
			/* Clear running flag */
			hdsp->running = 0;
		}
		mtx_unlock(&hdsp->lock);
		return 0;
		
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
		audio_stream_pause(stream);
		mtx_unlock(&hdsp->lock);
		return 0;
		
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		audio_stream_resume(stream);
		mtx_unlock(&hdsp->lock);
		return 0;
		
	default:
		mtx_unlock(&hdsp->lock);
		return -EINVAL;
	}
}

/*
 * snd_hdsp_pointer — called by basound_chan_getptr, which is called from
 * chn_intr (holding CHN_LOCK).  Must NOT acquire hdsp->lock here because
 * the trigger path holds CHN_LOCK when it acquires hdsp->lock; inverting
 * that order causes an SMP deadlock.
 *
 * Use the coarse HDSP_BufferID bit (bit 26 of the status register) to
 * determine which half of the double buffer the hardware is currently in.
 * Returns 0 (first half) or period_bytes (second half) as a byte offset,
 * matching the non-precise-ptr mode of the upstream Linux driver.
 */
unsigned long
snd_hdsp_pointer(struct snd_pcm_substream *substream)
{
	struct hdsp *hdsp = (struct hdsp *)substream->pcm->private_data;
	uint32_t status;

	if (hdsp == NULL || !hdsp->running || substream->runtime == NULL)
		return 0;

	status = hdsp_read(hdsp, HDSP_statusRegister);
	return (status & HDSP_BufferID) ? substream->runtime->period_bytes : 0;
}
