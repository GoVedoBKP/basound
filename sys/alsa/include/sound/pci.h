// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef _ALSA_PCI_H_
#define _ALSA_PCI_H_

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <sound/core.h>

/* ALSA PCI device structure */
struct pci_dev {
	device_t bsddev;
	uint16_t vendor;
	uint16_t device;
	uint16_t subsystem_vendor;
	uint16_t subsystem_device;
	struct resource *res[PCIR_MAX_BAR_0 + 1];
	int res_rid[PCIR_MAX_BAR_0 + 1];
};

#define pci_resource_start(pdev, bar) rman_get_start((pdev)->res[(bar)])
#define pci_resource_len(pdev, bar)   rman_get_size((pdev)->res[(bar)])

int snd_pci_quirk_lookup(struct pci_dev *pci, const void *list);

/* IO Access macros - should be implemented using the correct bus handle and tag */
/* Drivers should use bus_space_read/write directly or via safe helpers */

void *ioremap(bus_addr_t offset, size_t size);
void iounmap(void *addr);

#endif /* _ALSA_PCI_H_ */
