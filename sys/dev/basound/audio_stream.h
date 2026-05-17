// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef _BASOUND_AUDIO_STREAM_H_
#define _BASOUND_AUDIO_STREAM_H_

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>

/* Audio streaming state machine */
enum audio_stream_state {
	AUDIO_STREAM_IDLE,      /* Not initialized */
	AUDIO_STREAM_PREPARED,  /* Ready but not running */
	AUDIO_STREAM_RUNNING,   /* Active data transfer */
	AUDIO_STREAM_PAUSED,    /* Paused but not stopped */
	AUDIO_STREAM_STOPPED,   /* Stopped and cleanup initiated */
};

/* Audio stream descriptor - device-independent streaming control */
struct audio_stream {
	struct mtx lock;
	enum audio_stream_state state;
	
	/* DMA buffer info */
	void *dma_virt;           /* Kernel virtual address */
	bus_addr_t dma_phys;      /* Physical address for hardware */
	size_t dma_size;          /* Total buffer size */
	
	/* Streaming parameters */
	unsigned int sample_rate; /* 44100, 48000, 96000, etc. */
	unsigned int channels;    /* 1-26 depending on device */
	unsigned int format;      /* S16_LE, S24_3LE, S24_LE, S32_LE */
	size_t frame_bytes;       /* bytes per frame (channels * sample_bits/8) */
	
	/* Ring buffer position tracking */
	volatile unsigned long position;   /* Current playback/capture position */
	size_t period_size;       /* Period size in frames */
	unsigned long periods;    /* Number of periods */
	
	/* Device-specific callbacks for hardware control */
	int (*start)(void *dev_private);  /* Start hardware transfers */
	int (*stop)(void *dev_private);   /* Stop hardware transfers */
	int (*pause)(void *dev_private);  /* Pause transfers */
	int (*resume)(void *dev_private); /* Resume from pause */
	
	/* Private device context */
	void *dev_private;
	
	/* Statistics */
	unsigned long interrupts;
	unsigned long underruns;
	unsigned long overruns;
};

/* Audio stream helper functions */

static inline int audio_stream_start(struct audio_stream *stream)
{
	int err = 0;
	
	mtx_lock(&stream->lock);
	if (stream->state != AUDIO_STREAM_PREPARED) {
		mtx_unlock(&stream->lock);
		return -EINVAL;
	}
	
	if (stream->start) {
		err = stream->start(stream->dev_private);
		if (err == 0)
			stream->state = AUDIO_STREAM_RUNNING;
	} else {
		stream->state = AUDIO_STREAM_RUNNING;
	}
	
	mtx_unlock(&stream->lock);
	return err;
}

static inline int audio_stream_stop(struct audio_stream *stream)
{
	int err = 0;
	
	mtx_lock(&stream->lock);
	if (stream->state == AUDIO_STREAM_IDLE || stream->state == AUDIO_STREAM_STOPPED) {
		mtx_unlock(&stream->lock);
		return 0;
	}
	
	if (stream->stop) {
		err = stream->stop(stream->dev_private);
	}
	
	stream->state = AUDIO_STREAM_STOPPED;
	stream->position = 0;
	mtx_unlock(&stream->lock);
	return err;
}

static inline int audio_stream_pause(struct audio_stream *stream)
{
	int err = 0;
	
	mtx_lock(&stream->lock);
	if (stream->state != AUDIO_STREAM_RUNNING) {
		mtx_unlock(&stream->lock);
		return -EINVAL;
	}
	
	if (stream->pause) {
		err = stream->pause(stream->dev_private);
		if (err == 0)
			stream->state = AUDIO_STREAM_PAUSED;
	} else {
		stream->state = AUDIO_STREAM_PAUSED;
	}
	
	mtx_unlock(&stream->lock);
	return err;
}

static inline int audio_stream_resume(struct audio_stream *stream)
{
	int err = 0;
	
	mtx_lock(&stream->lock);
	if (stream->state != AUDIO_STREAM_PAUSED) {
		mtx_unlock(&stream->lock);
		return -EINVAL;
	}
	
	if (stream->resume) {
		err = stream->resume(stream->dev_private);
		if (err == 0)
			stream->state = AUDIO_STREAM_RUNNING;
	} else {
		stream->state = AUDIO_STREAM_RUNNING;
	}
	
	mtx_unlock(&stream->lock);
	return err;
}

/* Update position with wraparound handling */
static inline void audio_stream_update_position(struct audio_stream *stream, 
						  unsigned long new_pos)
{
	mtx_lock(&stream->lock);
	stream->position = new_pos % (stream->period_size * stream->periods);
	mtx_unlock(&stream->lock);
}

/* Initialize stream structure */
static inline void audio_stream_init(struct audio_stream *stream, 
				      const char *name)
{
	mtx_init(&stream->lock, name, NULL, MTX_DEF);
	stream->state = AUDIO_STREAM_IDLE;
	stream->position = 0;
	stream->interrupts = 0;
	stream->underruns = 0;
	stream->overruns = 0;
}

/* Cleanup stream structure */
static inline void audio_stream_destroy(struct audio_stream *stream)
{
	mtx_destroy(&stream->lock);
}

#endif /* _BASOUND_AUDIO_STREAM_H_ */
