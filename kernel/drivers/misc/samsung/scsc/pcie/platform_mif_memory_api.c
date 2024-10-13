#include "pcie/platform_mif_memory_api.h"
#include "pcie/platform_mif_irq_api.h"
#include "pcie/platform_mif_pcie_api.h"
#include "mif_reg.h"

#define SCSC_STATIC_MIFRAM_PAGE_TABLE

extern enum pcie_ctrl_state state;

void __iomem *platform_mif_map_region(unsigned long phys_addr, size_t size)
{
	size_t      i;
	struct page **pages;
	void        *vmem;

	size = PAGE_ALIGN(size);
#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	pages = kmalloc((size >> PAGE_SHIFT) * sizeof(*pages), GFP_KERNEL);
	if (!pages) {
		SCSC_TAG_ERR(PLAT_MIF, "wlbt: kmalloc of %zd byte pages table failed\n", (size >> PAGE_SHIFT) * sizeof(*pages));
		return NULL;
	}
#else
	/* Reserve the table statically, but make sure .dts doesn't exceed it */
	{
		static struct page *mif_map_pages[(MIFRAMMAN_MAXMEM >> PAGE_SHIFT)];
		static struct page *mif_map_pagesT[(MIFRAMMAN_MAXMEM >> PAGE_SHIFT) * sizeof(*pages)];

		pages = mif_map_pages;

		SCSC_TAG_INFO(PLAT_MIF, "count %d, PAGE_SHFIT = %d\n", MIFRAMMAN_MAXMEM >> PAGE_SHIFT,PAGE_SHIFT);
		SCSC_TAG_INFO(PLAT_MIF, "static mif_map_pages size %zd\n", sizeof(mif_map_pages));
		SCSC_TAG_INFO(PLAT_MIF, "static mif_map_pagesT size %zd\n", sizeof(mif_map_pagesT));

		if (size > MIFRAMMAN_MAXMEM) { /* Size passed in from .dts exceeds array */
			SCSC_TAG_ERR(PLAT_MIF, "wlbt: shared DRAM requested in .dts %zd exceeds mapping table %d\n",
					size, MIFRAMMAN_MAXMEM);
			return NULL;
		}
	}
#endif

	/* Map NORMAL_NC pages with kernel virtual space */
	for (i = 0; i < (size >> PAGE_SHIFT); i++) {
		pages[i] = phys_to_page(phys_addr);
		phys_addr += PAGE_SIZE;
	}

	vmem = vmap(pages, size >> PAGE_SHIFT, VM_MAP, pgprot_writecombine(PAGE_KERNEL));

#ifndef SCSC_STATIC_MIFRAM_PAGE_TABLE
	kfree(pages);
#endif
	if (!vmem)
		SCSC_TAG_ERR(PLAT_MIF, "wlbt: vmap of %zd pages failed\n", (size >> PAGE_SHIFT));
	return (void __iomem *)vmem;
}

void platform_mif_unmap_region(void *vmem)
{
	vunmap(vmem);
}

void *platform_mif_map(struct scsc_mif_abs *interface, size_t *allocated)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	state = PCIE_STATE_OFF;

	/* register interrupts */
	if (platform_mif_register_wakeup_irq(platform)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmap: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
		pcie_unmap(platform->pcie);
		return NULL;
	}

	return pcie_map(platform->pcie, allocated);
}

/* HERE: Not sure why mem is passed in - its stored in platform -
 * as it should be
 */
void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* Avoid unused parameter error */
	(void)mem;

	platform_mif_unregister_wakeup_irq(platform);

	pcie_unmap(platform->pcie);

}

int platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val = 0;

	val = pcie_get_mbox_pmu(platform->pcie);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read WB2AP_MAILBOX: %u\n", val);
	return val;
}

int platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Write AP2WB_MAILBOX: %u\n", val);

	pcie_set_mbox_pmu(platform->pcie, val);

	return 0;
}

int platform_mif_get_mbox_pmu_pcie_off(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val = 0;

	val = pcie_get_mbox_pmu_pcie_off(platform->pcie);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read PMU_PCIE_OFF_MAILBOX: %u\n", val);
	return val;
}

int platform_mif_set_mbox_pmu_pcie_off(struct scsc_mif_abs *interface, u32 val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Write PMU_PCIE_OFF_MAILBOX: %u\n", val);

	pcie_set_mbox_pmu_pcie_off(platform->pcie, val);

	return 0;
}

int platform_mif_get_mbox_pmu_error(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val = 0;

	val = pcie_get_mbox_pmu_error(platform->pcie);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read PMU_ERROR_MAILBOX: %u\n", val);
	return val;
}

void *platform_mif_get_mifram_phy_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return pcie_get_mifram_phy_ptr(platform->pcie, ref);
}

uintptr_t platform_mif_get_mif_pfn(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return pci_get_mif_pfn(platform->pcie);
}

int platform_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	pci_get_mifram_ref(platform->pcie, ptr, ref);

	return 0;
}

void *platform_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return pcie_get_mifram_ptr(platform->pcie, ref);
}

void platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr,
				   enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting remapper address %u target %s\n", remap_addr,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN" : "WLAN");

	pcie_remap_set(platform->pcie, remap_addr, target);
}

void platform_mif_memory_api_init(struct scsc_mif_abs *interface)
{
	interface->map = platform_mif_map;
	interface->unmap = platform_mif_unmap;
	interface->remap_set = platform_mif_remap_set;
	interface->get_mifram_ptr = platform_mif_get_mifram_ptr;
	interface->get_mifram_ref = platform_mif_get_mifram_ref;
	interface->get_mifram_pfn = platform_mif_get_mif_pfn;
	interface->get_mbox_pmu = platform_mif_get_mbox_pmu;
	interface->get_mifram_phy_ptr = platform_mif_get_mifram_phy_ptr;
	interface->set_mbox_pmu = platform_mif_set_mbox_pmu;

}
