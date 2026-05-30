// SPDX-License-Identifier: GPL-3.0-or-later
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <dev/pci/pcivar.h>

#include "hdsp.h"

MALLOC_DECLARE(M_ALSA);

/* Mixer Control Stubs */
static int
snd_hdsp_info_mixer(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = 1; /* SNDRV_CTL_ELEM_TYPE_INTEGER */
	uinfo->count = 3;
	return 0;
}

static int
snd_hdsp_get_mixer(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int addr = 0; /* Full address calculation requires porting more helpers */

	mtx_lock(&hdsp->lock);
	ucontrol->value.integer.value[0] = hdsp_read_gain(hdsp, addr);
	mtx_unlock(&hdsp->lock);
	return 0;
}

static int
snd_hdsp_put_mixer(struct snd_kcontrol *kcontrol, struct snd_ctl_elem_value *ucontrol)
{
	struct hdsp *hdsp = snd_kcontrol_chip(kcontrol);
	int gain = ucontrol->value.integer.value[0];
	int addr = 0; /* Simplified address calculation */
	int change;

	mtx_lock(&hdsp->lock);
	change = (hdsp_read_gain(hdsp, addr) != gain);
	if (change) {
		hdsp_write_gain(hdsp, addr, gain);
	}
	mtx_unlock(&hdsp->lock);
	return change;
}

static struct snd_kcontrol_new hdsp_mixer_ctl = {
	.iface = 1, /* SNDRV_CTL_ELEM_IFACE_MIXER */
	.name = "Mixer",
	.info = snd_hdsp_info_mixer,
	.get = snd_hdsp_get_mixer,
	.put = snd_hdsp_put_mixer
};

void
snd_hdsp_create_mixer(struct snd_card *card, struct hdsp *hdsp)
{
	struct snd_kcontrol *kctl;
	
	kctl = snd_ctl_new1(&hdsp_mixer_ctl, hdsp);
	if (kctl)
		snd_ctl_add(card, kctl);
}
