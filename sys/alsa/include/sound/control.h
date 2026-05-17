// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef _ALSA_CONTROL_H_
#define _ALSA_CONTROL_H_

#include <sound/core.h>

#include <sys/queue.h>

struct snd_ctl_elem_info {
	int type;
	int count;
};

struct snd_ctl_elem_value {
	union {
		struct {
			unsigned char status[24];
		} iec958;
		int integer;
	} value;
};

#define SNDRV_CTL_ELEM_TYPE_IEC958	4

struct snd_kcontrol_new {
	const char *name;
	int iface;
	unsigned int access;
	int count;
	void *private_data;
	int (*info)(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
	int (*get)(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
	int (*put)(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
};

struct snd_kcontrol {
	char name[64];
	void *private_data;
	int count;
	int (*info)(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo);
	int (*get)(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
	int (*put)(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol);
	STAILQ_ENTRY(snd_kcontrol) next_ctl;
};

int snd_ctl_add(struct snd_card *card, struct snd_kcontrol *kcontrol);
struct snd_kcontrol *snd_ctl_new1(void *ncontrol, void *private_data);

/* FreeBSD integration */
int basound_mixer_register(struct snd_card *card);

#endif /* _ALSA_CONTROL_H_ */
