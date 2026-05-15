#include <sound/pci.h>
#include <sys/systm.h>
#include <machine/bus.h>

int
snd_pci_quirk_lookup(struct pci_dev *pci, const void *list)
{
	/* Placeholder for PCI quirk lookup logic */
	return -1;
}

void *
ioremap(bus_addr_t offset, size_t size)
{
	/* In FreeBSD, we usually use bus_space_map on a resource.
	 * This shim will need to be clever.
	 * For now, return the offset as a handle if it's already mapped.
	 */
	return (void *)offset;
}

void
iounmap(void *addr)
{
}
