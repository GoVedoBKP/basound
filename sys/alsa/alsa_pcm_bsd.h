#ifndef _ALSA_PCM_BSD_H_
#define _ALSA_PCM_BSD_H_

#include <dev/sound/pcm/sound.h>
#include <sound/pcm.h>

struct basound_chan {
	struct snd_pcm_substream *substream;
	struct pcm_channel *channel;
	struct snd_pcm_runtime *runtime;
	uint32_t format;
	uint32_t speed;
	uint32_t blocksize;
};

#endif /* _ALSA_PCM_BSD_H_ */
