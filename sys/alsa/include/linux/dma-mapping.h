#ifndef _LINUX_DMA_MAPPING_H_
#define _LINUX_DMA_MAPPING_H_

/* Minimal DMA mapping shim - FreeBSD uses bus_dma directly */

enum dma_data_direction {
	DMA_BIDIRECTIONAL = 0,
	DMA_TO_DEVICE = 1,
	DMA_FROM_DEVICE = 2,
	DMA_NONE = 3,
};

#endif
