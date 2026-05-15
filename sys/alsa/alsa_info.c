#include <sound/info.h>
#include <sys/systm.h>
#include <sys/malloc.h>

MALLOC_DECLARE(M_ALSA);

int
snd_card_proc_new(struct snd_card *card, const char *name, struct snd_info_entry **entryp)
{
	struct snd_info_entry *entry;

	entry = malloc(sizeof(*entry), M_ALSA, M_WAITOK | M_ZERO);
	if (entry == NULL)
		return -ENOMEM;
	
	entry->name = name;
	*entryp = entry;
	return 0;
}

void
snd_info_set_text_ops(struct snd_info_entry *entry, void *data, void (*show)(struct snd_info_entry *, void *))
{
	entry->private_data = data;
}
