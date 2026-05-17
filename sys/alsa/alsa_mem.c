// SPDX-License-Identifier: GPL-3.0-or-later
#include <sound/memalloc.h>
#include <sound/core.h>
#include <sys/systm.h>
#include <sys/malloc.h>

static void
snd_dma_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *addr = arg;

	if (error)
		return;
	*addr = segs[0].ds_addr;
}

int
snd_dma_alloc_pages(int type, struct device *device, size_t size,
		    struct snd_dma_buffer *dmab)
{
	int error;

	dmab->bytes = size;

	error = bus_dma_tag_create(bus_get_dma_tag(device->bsddev),
				   1, 0,			/* alignment, boundary */
				   BUS_SPACE_MAXADDR,	/* lowaddr */
				   BUS_SPACE_MAXADDR,	/* highaddr */
				   NULL, NULL,		/* filter, filterarg */
				   size, 1,		/* maxsize, nsegments */
				   size,			/* maxsegsize */
				   0,			/* flags */
				   NULL, NULL,		/* lockfunc, lockarg */
				   &dmab->tag);
	if (error)
		return -ENOMEM;

	error = bus_dmamem_alloc(dmab->tag, &dmab->area,
				 BUS_DMA_WAITOK | BUS_DMA_ZERO,
				 &dmab->map);
	if (error) {
		bus_dma_tag_destroy(dmab->tag);
		return -ENOMEM;
	}

	error = bus_dmamap_load(dmab->tag, dmab->map, dmab->area,
				size, snd_dma_cb, &dmab->addr, 0);
	if (error) {
		bus_dmamem_free(dmab->tag, dmab->area, dmab->map);
		bus_dma_tag_destroy(dmab->tag);
		return -ENOMEM;
	}

	return 0;
}

void
snd_dma_free_pages(struct snd_dma_buffer *dmab)
{
	if (dmab->tag == NULL)
		return;

	bus_dmamap_unload(dmab->tag, dmab->map);
	bus_dmamem_free(dmab->tag, dmab->area, dmab->map);
	bus_dma_tag_destroy(dmab->tag);
}
