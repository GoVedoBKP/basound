#ifndef _BASOUND_HDSP_H_
#define _BASOUND_HDSP_H_

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <linux/workqueue.h>
#include <linux/firmware.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pci.h>
#include <sound/control.h>
#include "../audio_stream.h"

/* Register Offsets */
#define HDSP_resetPointer               0
#define HDSP_freqReg                    0
#define HDSP_outputBufferAddress        32
#define HDSP_inputBufferAddress         36
#define HDSP_controlRegister            64
#define HDSP_interruptConfirmation      96
#define HDSP_outputEnable               128
#define HDSP_control2Reg                256
#define HDSP_midiDataOut0               352
#define HDSP_midiDataOut1               356
#define HDSP_fifoData                   368
#define HDSP_inputEnable                384

#define HDSP_statusRegister             0
#define HDSP_timecode                   128
#define HDSP_status2Register            192
#define HDSP_midiDataIn0                360
#define HDSP_midiDataIn1                364

#define HDSP_midiControl                368
#define HDSP_midiStatus                 368

/* MIDI status bits */
#define HDSP_midiStatusInputAvailable  (1<<0)
#define HDSP_midiStatusOutputReady     (1<<1)
#define HDSP_midiControlOutputEnable   (1<<0)
#define HDSP_midiControlInputEnable    (1<<1)

#define HDSP_fifoStatus                 400

#define HDSP_MATRIX_MIXER_SIZE          2048

enum HDSP_IO_Type {
	Digiface,
	Multiface,
	H9652,
	H9632,
	RPM,
	Unknown
};

struct hdsp_midi {
	struct hdsp *hdsp;
	int id;
	/* struct snd_rawmidi_substream *input; */
	/* struct snd_rawmidi_substream *output; */
	char pending;
	struct mtx lock;
	/* struct timer_list timer; */
	int istimer;
};

struct hdsp {
	struct mtx            lock;
	struct snd_pcm_substream *capture_substream;
	struct snd_pcm_substream *playback_substream;
	
	/* MIDI */
	struct hdsp_midi      midi[2];
	struct work_struct    midi_work;
	
	uint32_t              control_register;      /* cached value */
	uint32_t              control2_register;     /* cached value */
	
	char                 *card_name;
	enum HDSP_IO_Type     io_type;
	unsigned short        firmware_rev;
	unsigned short        state;
	
	const struct firmware *firmware;
	
	size_t                period_bytes;
	unsigned char         max_channels;
	unsigned char         ss_in_channels;
	unsigned char         ss_out_channels;
	
	struct snd_dma_buffer capture_dma_buf;
	struct snd_dma_buffer playback_dma_buf;
	unsigned char         *capture_buffer;
	unsigned char         *playback_buffer;
	
	int                   running;
	int                   system_sample_rate;
	uint32_t              dds_value;
	
	int                   irq;
	void                 *iobase;
	
	struct snd_card      *card;
	struct snd_pcm       *pcm;
	struct pci_dev       *pci;
	
	unsigned short        mixer_matrix[HDSP_MATRIX_MIXER_SIZE];
	
	/* Audio streaming framework */
	struct audio_stream   capture_stream;
	struct audio_stream   playback_stream;
};

#define HDSP_audioIRQPending    (1<<0)
#define HDSP_midi0IRQPending    (1<<1)
#define HDSP_midi1IRQPending    (1<<2)

#define HDSP_BufferPositionMask 0x000FFC0 /* Bit 6..15 : h/w buffer pointer */
#define HDSP_BufferID           (1<<26)

#define HDSP_InitializationComplete (1<<0)
#define HDSP_ClockModeMaster    (1<<4)
#define HDSP_AudioInterruptEnable (1<<6)

#define HDSP_DllError (1<<6)
#define HDSP_S_PROGRAM 0x20
#define HDSP_S_LOAD 0x40
#define HDSP_S200 0x02
#define HDSP_S300 0x03
#define HDSP_PROGRAM 0x80

#define DDS_NUMERATOR 110000000000ULL

#define HDSP_FIRMWARE_SIZE 24413
#define HDSP_SHORT_WAIT 1
#define HDSP_LONG_WAIT 500

/* Internal helper */
static inline void *snd_kcontrol_chip(struct snd_kcontrol *kcontrol)
{
	return kcontrol->private_data;
}

/* Internal functions to be ported */
int snd_hdsp_create(struct snd_card *card, struct hdsp *hdsp);
int snd_hdsp_enable_io(struct hdsp *hdsp);
void snd_hdsp_interrupt(void *arg);
int snd_hdsp_upload_firmware(struct hdsp *hdsp);
int hdsp_set_rate(struct hdsp *hdsp, int rate, int called_internally);
int hdsp_read_gain(struct hdsp *hdsp, int addr);
int hdsp_write_gain(struct hdsp *hdsp, unsigned int addr, unsigned short data);
void snd_hdsp_create_mixer(struct snd_card *card, struct hdsp *hdsp);
void snd_hdsp_midi_work(struct work_struct *work);

/* PCM operations */
int snd_hdsp_hw_params(struct snd_pcm_substream *substream, void *hw_params);
int snd_hdsp_prepare(struct snd_pcm_substream *substream);
int snd_hdsp_trigger(struct snd_pcm_substream *substream, int cmd);
unsigned long snd_hdsp_pointer(struct snd_pcm_substream *substream);

/* Register access helpers */
static inline uint32_t hdsp_read(struct hdsp *hdsp, int reg)
{
	return readl((char *)hdsp->iobase + reg);
}

static inline void hdsp_write(struct hdsp *hdsp, int reg, uint32_t val)
{
	writel((char *)hdsp->iobase + reg, val);
}

#endif /* _BASOUND_HDSP_H_ */
