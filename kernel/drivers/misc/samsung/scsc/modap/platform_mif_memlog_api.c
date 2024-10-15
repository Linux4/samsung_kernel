#include <linux/kconfig.h>
#include "platform_mif.h"

#if IS_ENABLED(CONFIG_SCSC_MEMLOG)
#include "scsc_mif_abs.h"
#include "mif_reg.h"
#include "modap/platform_mif_memlog_api.h"
#include "modap/platform_mif_regmap_api.h"



static int platform_mif_set_mem_region2(
	struct scsc_mif_abs *interface,
	void __iomem *_mem_region2,
	size_t _mem_size_region2)
{
	/* If memlogger API is enabled, mxlogger's mem should be set
	 * by another routine
	 */
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform->mem_region2 = _mem_region2;
	platform->mem_size_region2 = _mem_size_region2;
	return 0;
}

static void *platform_mif_get_mifram_ptr_region2(
	struct scsc_mif_abs *interface,
	scsc_mifram_ref ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_region2) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return NULL;
	}
	ref -= platform->mem_size;

	/* Check limits */
	if (ref >= 0 && ref < platform->mem_size_region2)
		return (void *)((uintptr_t)platform->mem_region2 + (uintptr_t)ref);
	else
		return NULL;
}

static int platform_mif_get_mifram_ref_region2(
	struct scsc_mif_abs *interface,
	void *ptr,
	scsc_mifram_ref *ref)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	SCSC_TAG_DEBUG_DEV(PLAT_MIF, platform->dev, "\n");

	if (!platform->mem_region2) {
		SCSC_TAG_ERR_DEV(PLAT_MIF, platform->dev, "Memory unmmaped\n");
		return -ENOMEM;
	}

	/* Check limits! */
	if (ptr >= (platform->mem_region2 + platform->mem_size_region2))
		return -ENOMEM;

	*ref = (scsc_mifram_ref)(
		(uintptr_t)ptr
		- (uintptr_t)platform->mem_region2
		+ platform->mem_size);
	return 0;
}

static void platform_mif_set_memlog_paddr(
	struct scsc_mif_abs *interface,
	dma_addr_t paddr)
{
	struct platform_mif *platform = platform_mif_from_mif_abs(interface);

	platform->paddr = paddr;
}

void platform_mif_memlog_init(struct platform_mif *platform)
{
	struct scsc_mif_abs *interface = &platform->interface;

	interface->get_mifram_ptr_region2 = platform_mif_get_mifram_ptr_region2;
	interface->get_mifram_ref_region2 = platform_mif_get_mifram_ref_region2;
	interface->set_mem_region2 = platform_mif_set_mem_region2;
	interface->set_memlog_paddr = platform_mif_set_memlog_paddr;
	platform->paddr = 0;
}
#else
void platform_mif_memlog_init(struct platform_mif *platform)
{
	(void)platform;
}
#endif
