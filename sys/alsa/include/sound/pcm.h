#ifndef _ALSA_PCM_H_
#define _ALSA_PCM_H_

#include <sound/core.h>
#include <sound/memalloc.h>

struct snd_pcm_substream;

struct snd_pcm_ops {
	int (*open)(struct snd_pcm_substream *substream);
	int (*close)(struct snd_pcm_substream *substream);
	int (*ioctl)(struct snd_pcm_substream *substream,
		     unsigned int cmd, void *arg);
	int (*hw_params)(struct snd_pcm_substream *substream,
			 void *hw_params);
	int (*hw_free)(struct snd_pcm_substream *substream);
	int (*prepare)(struct snd_pcm_substream *substream);
	int (*trigger)(struct snd_pcm_substream *substream, int cmd);
	unsigned long (*pointer)(struct snd_pcm_substream *substream);
};

struct snd_pcm_runtime {
	void *dma_area;
	bus_addr_t dma_addr;
	size_t dma_bytes;
	void *private_data;
	
	/* Stream state tracking */
	unsigned int state;
	unsigned long dma_position;
	
	/* Hardware capabilities and constraints */
	struct {
		unsigned int info;
		unsigned long formats;
		unsigned long rates;
		unsigned int rate_min;
		unsigned int rate_max;
		unsigned int channels_min;
		unsigned int channels_max;
		size_t buffer_bytes_max;
		size_t period_bytes_min;
		size_t period_bytes_max;
		unsigned int periods_min;
		unsigned int periods_max;
	} hw;
};

struct snd_pcm_substream {
	struct snd_pcm *pcm;
	struct snd_pcm_str *pstr;
	int number;
	char name[32];
	int stream;
	struct snd_pcm_runtime *runtime;
	void *private_data;
};

struct snd_pcm_str {
	struct snd_pcm *pcm;
	int stream;
	int substream_count;
	struct snd_pcm_substream *substream;
	const struct snd_pcm_ops *ops;
};

struct snd_pcm {
	struct snd_card *card;
	char id[64];
	char name[80];
	struct snd_pcm_str streams[2];
	void *private_data;
	STAILQ_ENTRY(snd_pcm) next_pcm;
};

#define SNDRV_PCM_STREAM_PLAYBACK	0
#define SNDRV_PCM_STREAM_CAPTURE	1

int snd_pcm_new(struct snd_card *card, const char *id, int device,
		int playback_count, int capture_count,
		struct snd_pcm **rpcm);
void snd_pcm_set_ops(struct snd_pcm *pcm, int direction,
		     const struct snd_pcm_ops *ops);

/* PCM library helpers */
int snd_pcm_lib_malloc_pages(struct snd_pcm_substream *substream, size_t size);
int snd_pcm_lib_free_pages(struct snd_pcm_substream *substream);
void snd_pcm_period_elapsed(struct snd_pcm_substream *substream);

/* FreeBSD integration */
int basound_pcm_register(struct snd_pcm *pcm);

#endif /* _ALSA_PCM_H_ */
