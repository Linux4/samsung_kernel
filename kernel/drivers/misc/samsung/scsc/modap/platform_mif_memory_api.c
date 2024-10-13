#include "modap/platform_mif_memory_api.h"
#include "modap/platform_mif_regmap_api.h"
#include "modap/platform_mif_irq_api.h"
#include "mif_reg.h"

#define SCSC_STATIC_MIFRAM_PAGE_TABLE

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
	u8                  i;

	if (allocated)
		*allocated = 0;

	platform->mem =
		platform_mif_map_region(platform->mem_start, platform->mem_size);

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Error remaping shared memory\n");
		return NULL;
	}

	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Map: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);

	/* Initialise MIF registers with documented defaults */
	/* MBOXes */
	for (i = 0; i < NUM_MBOX_PLAT; i++) {
		platform_mif_reg_write(platform, MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);
		platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);
#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
		platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(ISSR(i)), 0x00000000);
#endif
	}

	/* MRs */ /*1's - set all as Masked */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);

	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);

#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);

	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
#endif
#ifdef CONFIG_SCSC_CHV_SUPPORT
	if (chv_disable_irq == true) {
		if (allocated)
			*allocated = platform->mem_size;
		return platform->mem;
	}
#endif
	/* register interrupts */
	if (platform_mif_register_irq(platform)) {
		SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmap: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
		platform_mif_unmap_region(platform->mem);
		return NULL;
	}

	if (allocated)
		*allocated = platform->mem_size;
	/* Set the CR4 base address in Mailbox??*/
	return platform->mem;
}

/* HERE: Not sure why mem is passed in - its stored in platform - as it should be */
void platform_mif_unmap(struct scsc_mif_abs *interface, void *mem)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	/* Avoid unused parameter error */
	(void)mem;

	/* MRs */ /*1's - set all as Masked */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTMR0), 0xffff0000);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTMR1), 0x0000ffff);
#endif

#ifdef CONFIG_SCSC_CHV_SUPPORT
	/* Restore PIO changed by Maxwell subsystem */
	if (chv_disable_irq == false)
		/* Unregister IRQs */
		platform_mif_unregister_irq(platform);
#else
	platform_mif_unregister_irq(platform);
#endif
	/* CRs */ /* 1's - clear all the interrupts */
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write_wpan(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTCR0), 0xffff0000);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTCR1), 0x0000ffff);
#endif
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Unmap: virt %p phys %lx\n", platform->mem, (uintptr_t)platform->mem_start);
	platform_mif_unmap_region(platform->mem);
	platform->mem = NULL;
}

u32 *platform_mif_get_mbox_ptr(struct scsc_mif_abs *interface, u32 mbox_index, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32                 *addr;

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "mbox index 0x%x target %s\n", mbox_index,
			   (target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");
	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		addr = platform->base + MAILBOX_WLBT_REG(ISSR(mbox_index));
	else
		addr = platform->base_wpan + MAILBOX_WLBT_REG(ISSR(mbox_index));
	return addr;
}

#if IS_ENABLED(CONFIG_WLBT_PMU2AP_MBOX)
int platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	u32 val;
	u32 irq_val;

	irq_val = platform_mif_reg_read_pmu(platform, MAILBOX_WLBT_REG(INTMSR0)) >> 16;
	if (!irq_val){
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Wrong PMU MAILBOX Interrupt!!\n");
		return 0;
	}

	val = platform_mif_reg_read_pmu(platform, MAILBOX_WLBT_REG(ISSR(0)));
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read PMU MAILBOX: %u\n", val);
	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTCR0), (1 << 16));
	return val;
}

int platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(ISSR(0)), val);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Write PMU MAILBOX: %u\n", val);

	platform_mif_reg_write_pmu(platform, MAILBOX_WLBT_REG(INTGR1), 1);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Setting INTGR1: bit 1 on target PMU\n");
	return 0;
}
#else
int platform_mif_get_mbox_pmu(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct regmap *regmap = platform_mif_get_regmap(platform, BOOT_CFG);
	u32 val;

	regmap_read(regmap, WB2AP_MAILBOX, &val);
	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "Read WB2AP_MAILBOX: %u\n", val);
	return val;
}

int platform_mif_set_mbox_pmu(struct scsc_mif_abs *interface, u32 val)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);
	struct regmap *regmap = platform_mif_get_regmap(platform, BOOT_CFG);

	regmap_write(regmap, AP2WB_MAILBOX, val);
	SCSC_TAG_INFO_DEV(PLAT_MIF, platform->dev, "Write AP2WB_MAILBOX: %u\n", val);
	return 0;
}

#endif

void *platform_mif_get_mifram_phy_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_start) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}

	return (void *)((uintptr_t)platform->mem_start + (uintptr_t)ref);
}

uintptr_t platform_mif_get_mif_pfn(struct scsc_mif_abs *interface)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	return vmalloc_to_pfn(platform->mem);
}

int platform_mif_get_mifram_ref(struct scsc_mif_abs *interface, void *ptr, scsc_mifram_ref *ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return -ENOMEM;
	}

	/* Check limits! */
	if (ptr >= (platform->mem + platform->mem_size)) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Unable to get pointer reference\n");
		return -ENOMEM;
	}

	*ref = (scsc_mifram_ref)((uintptr_t)ptr - (uintptr_t)platform->mem);

	return 0;
}

void *platform_mif_get_mifram_ptr(struct scsc_mif_abs *interface, scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}

	/* Check limits */
	if (ref >= 0 && ref < platform->mem_size)
		return (void *)((uintptr_t)platform->mem + (uintptr_t)ref);
	else
		return NULL;
}

void platform_mif_remap_set(struct scsc_mif_abs *interface, uintptr_t remap_addr, enum scsc_mif_abs_target target)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev,
		"Setting remapper address %u target %s\n",
		remap_addr,
		(target == SCSC_MIF_ABS_TARGET_WPAN) ? "WPAN":"WLAN");

	if (target == SCSC_MIF_ABS_TARGET_WLAN)
		platform->remap_addr_wlan = remap_addr;
	else if (target == SCSC_MIF_ABS_TARGET_WPAN)
		platform->remap_addr_wpan = remap_addr;
	else
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev,
			"Incorrect target %d\n", target);
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
	interface->get_mbox_ptr = platform_mif_get_mbox_ptr;
	interface->get_mifram_phy_ptr = platform_mif_get_mifram_phy_ptr;
	interface->set_mbox_pmu = platform_mif_set_mbox_pmu;

}
