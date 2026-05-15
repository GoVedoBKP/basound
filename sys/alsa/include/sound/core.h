#ifndef _ALSA_CORE_H_
#define _ALSA_CORE_H_

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/systm.h>

/* Minimal struct device for ALSA compatibility */
struct device {
	device_t bsddev;
};

#include <sys/queue.h>

struct snd_pcm;

/* ALSA Card Structure */
struct snd_card {
	int number;
	char id[16];
	char driver[16];
	char shortname[32];
	char longname[80];
	void *private_data;
	struct device *dev;
	device_t pcm_dev;	/* FreeBSD pcm device */
	STAILQ_HEAD(, snd_pcm) pcm_list;
	STAILQ_HEAD(, snd_kcontrol) ctl_list;
};

/* ALSA Device Structure */
struct snd_device {
	struct snd_card *card;
	void *device_data;
};

/* Logging macros compatible with ALSA */
#define dev_err(dev, fmt, ...)  printf("basound: ERR: %s: " fmt, device_get_nameunit((dev)->bsddev), ##__VA_ARGS__)
#define dev_warn(dev, fmt, ...) printf("basound: WARN: %s: " fmt, device_get_nameunit((dev)->bsddev), ##__VA_ARGS__)
#define dev_info(dev, fmt, ...) printf("basound: INFO: %s: " fmt, device_get_nameunit((dev)->bsddev), ##__VA_ARGS__)
#define dev_dbg(dev, fmt, ...)  printf("basound: DBG: %s: " fmt, device_get_nameunit((dev)->bsddev), ##__VA_ARGS__)

/* Card management functions */
int snd_card_new(struct device *parent, int idx, const char *xid,
		 struct module *module, int extra_size,
		 struct snd_card **card_ret);
int snd_card_free(struct snd_card *card);
int snd_card_register(struct snd_card *card);

#endif /* _ALSA_CORE_H_ */
