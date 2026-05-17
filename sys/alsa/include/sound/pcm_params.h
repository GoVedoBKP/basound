// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef _SOUND_PCM_PARAMS_H_
#define _SOUND_PCM_PARAMS_H_

#include <sound/pcm.h>

/* Simplified stubs for PCM parameters */
#define params_rate(p)		48000
#define params_buffer_bytes(p)	4096

/* PCM stream info capabilities */
#define SNDRV_PCM_INFO_MMAP		(1 << 0)
#define SNDRV_PCM_INFO_MMAP_VALID	(1 << 1)
#define SNDRV_PCM_INFO_INTERLEAVED	(1 << 2)
#define SNDRV_PCM_INFO_NONINTERLEAVED	(1 << 3)
#define SNDRV_PCM_INFO_COMPLEX		(1 << 4)

/* PCM format bits */
#define SNDRV_PCM_FMTBIT_S8		(1ULL << 0)
#define SNDRV_PCM_FMTBIT_U8		(1ULL << 1)
#define SNDRV_PCM_FMTBIT_S16_LE		(1ULL << 2)
#define SNDRV_PCM_FMTBIT_S16_BE		(1ULL << 3)
#define SNDRV_PCM_FMTBIT_U16_LE		(1ULL << 4)
#define SNDRV_PCM_FMTBIT_U16_BE		(1ULL << 5)
#define SNDRV_PCM_FMTBIT_S24_LE		(1ULL << 6)
#define SNDRV_PCM_FMTBIT_S24_BE		(1ULL << 7)
#define SNDRV_PCM_FMTBIT_U24_LE		(1ULL << 8)
#define SNDRV_PCM_FMTBIT_U24_BE		(1ULL << 9)
#define SNDRV_PCM_FMTBIT_S32_LE		(1ULL << 10)
#define SNDRV_PCM_FMTBIT_S32_BE		(1ULL << 11)
#define SNDRV_PCM_FMTBIT_U32_LE		(1ULL << 12)
#define SNDRV_PCM_FMTBIT_U32_BE		(1ULL << 13)
#define SNDRV_PCM_FMTBIT_S24_3LE	(1ULL << 14)
#define SNDRV_PCM_FMTBIT_S24_3BE	(1ULL << 15)
#define SNDRV_PCM_FMTBIT_U24_3LE	(1ULL << 16)
#define SNDRV_PCM_FMTBIT_U24_3BE	(1ULL << 17)

/* PCM rate bits */
#define SNDRV_PCM_RATE_5512		(1 << 0)
#define SNDRV_PCM_RATE_8000		(1 << 1)
#define SNDRV_PCM_RATE_11025		(1 << 2)
#define SNDRV_PCM_RATE_16000		(1 << 3)
#define SNDRV_PCM_RATE_22050		(1 << 4)
#define SNDRV_PCM_RATE_32000		(1 << 5)
#define SNDRV_PCM_RATE_44100		(1 << 6)
#define SNDRV_PCM_RATE_48000		(1 << 7)
#define SNDRV_PCM_RATE_64000		(1 << 8)
#define SNDRV_PCM_RATE_88200		(1 << 9)
#define SNDRV_PCM_RATE_96000		(1 << 10)
#define SNDRV_PCM_RATE_176400		(1 << 11)
#define SNDRV_PCM_RATE_192000		(1 << 12)

/* PCM trigger command */
#define SNDRV_PCM_TRIGGER_START		0
#define SNDRV_PCM_TRIGGER_STOP		1
#define SNDRV_PCM_TRIGGER_PAUSE_PUSH	3
#define SNDRV_PCM_TRIGGER_PAUSE_RELEASE	4

/* PCM stream states */
#define SNDRV_PCM_STATE_OPEN		0
#define SNDRV_PCM_STATE_SETUP		1
#define SNDRV_PCM_STATE_PREPARED	2
#define SNDRV_PCM_STATE_RUNNING		3
#define SNDRV_PCM_STATE_XRUN		4
#define SNDRV_PCM_STATE_DRAINING	5
#define SNDRV_PCM_STATE_PAUSED		6
#define SNDRV_PCM_STATE_SUSPENDED	7
#define SNDRV_PCM_STATE_DISCONNECTED	8
#define SNDRV_PCM_STATE_STOPPED		9

#endif
