#include <sound/hwdep.h>
#include <sys/systm.h>
#include <sys/malloc.h>

MALLOC_DECLARE(M_ALSA);

int
snd_hwdep_new(struct snd_card *card, char *id, int device, struct snd_hwdep **rhwdep)
{
	struct snd_hwdep *hw;

	hw = malloc(sizeof(*hw), M_ALSA, M_WAITOK | M_ZERO);
	if (hw == NULL)
		return -ENOMEM;

	hw->card = card;
	if (id)
		strlcpy(hw->name, id, sizeof(hw->name));

	if (rhwdep)
		*rhwdep = hw;

	return 0;
}
