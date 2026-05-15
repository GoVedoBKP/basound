#ifndef _SOUND_ASOUND_H_
#define _SOUND_ASOUND_H_

/* ALSA asound.h - Basic ALSA device-agnostic definitions */

/* Max number of sample rates */
#define SNDRV_PCM_FMTBIT_A_LAW		(1ULL << (__SNDRV_PCM_FMTBIT_ALAW))
#define SNDRV_PCM_FMTBIT_MU_LAW		(1ULL << (__SNDRV_PCM_FMTBIT_MULAW))
#define SNDRV_PCM_FMTBIT_IMA_ADPCM	(1ULL << (__SNDRV_PCM_FMTBIT_IMA_ADPCM))
#define SNDRV_PCM_FMTBIT_MS_ADPCM	(1ULL << (__SNDRV_PCM_FMTBIT_MS_ADPCM))
#define SNDRV_PCM_FMTBIT_GSM		(1ULL << (__SNDRV_PCM_FMTBIT_GSM))

#endif
