#ifndef _ALSA_MEMALLOC_H_
#define _ALSA_MEMALLOC_H_

#include <sys/param.h>
#include <sys/bus.h>
#include <machine/bus.h>

struct snd_dma_buffer {
	size_t bytes;
	void *area;
	bus_addr_t addr;
	bus_dma_tag_t tag;
	bus_dmamap_t map;
};

#define SNDRV_DMA_TYPE_DEV	1

struct device;

int snd_dma_alloc_pages(int type, struct device *device, size_t size,
			struct snd_dma_buffer *dmab);
void snd_dma_free_pages(struct snd_dma_buffer *dmab);

#endif /* _ALSA_MEMALLOC_H_ */
