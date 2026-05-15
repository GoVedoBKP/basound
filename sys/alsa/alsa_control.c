#include <sound/control.h>
#include <sys/systm.h>
#include <sys/malloc.h>

MALLOC_DECLARE(M_ALSA);

struct snd_kcontrol *
snd_ctl_new1(void *ncontrol, void *private_data)
{
	struct snd_kcontrol_new *nctl = ncontrol;
	struct snd_kcontrol *kctl;

	kctl = malloc(sizeof(*kctl), M_ALSA, M_WAITOK | M_ZERO);
	if (kctl) {
		if (nctl->name)
			strlcpy(kctl->name, nctl->name, sizeof(kctl->name));
		kctl->private_data = private_data;
		kctl->count = nctl->count;
		kctl->info = nctl->info;
		kctl->get = nctl->get;
		kctl->put = nctl->put;
	}

	return kctl;
}

int
snd_ctl_add(struct snd_card *card, struct snd_kcontrol *kcontrol)
{
	STAILQ_INSERT_TAIL(&card->ctl_list, kcontrol, next_ctl);
	return 0;
}
