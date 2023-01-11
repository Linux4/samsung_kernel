#include <linux/init.h>
#include <linux/io.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>

#include "regs-addr.h"

struct regs_addr {
	void __iomem *va;
	phys_addr_t pa;
};


static struct regs_addr all_regs_addr[REGS_ADDR_MAX];

struct of_device_id regs_addr_matches[] = {
	{
		.compatible = "marvell,mmp-pmu-mpmu",
		.data = &all_regs_addr[REGS_ADDR_MPMU],
	}, {
		.compatible = "marvell,mmp-pmu-apmu",
		.data = &all_regs_addr[REGS_ADDR_APMU],
	}, {
		.compatible = "marvell,mmp-pmu-apbc",
		.data = &all_regs_addr[REGS_ADDR_APBC],
	}, {
		.compatible = "marvell,mmp-ciu",
		.data = &all_regs_addr[REGS_ADDR_CIU],
	}, {
		.compatible = "marvell,mmp-dmcu",
		.data = &all_regs_addr[REGS_ADDR_DMCU],
	}, {
		.compatible = "mrvl,mmp-intc-wakeupgen",
		.data = &all_regs_addr[REGS_ADDR_ICU],
	}, {
		.compatible = "arm,cortex-a7-gic",
		.data = &all_regs_addr[REGS_ADDR_GIC_DIST],
	}, {
		.compatible = "marvell,mmp-dciu",
		.data = &all_regs_addr[REGS_ADDR_DCIU],
	},
	{},
};

void regs_addr_iomap(void)
{
	struct device_node *np;
	struct regs_addr *cell;
	u32 pa;
	void __iomem *va;

	for_each_matching_node(np, regs_addr_matches) {
		const struct of_device_id *match =
				of_match_node(regs_addr_matches, np);
		cell = (struct regs_addr *)match->data;
		of_property_read_u32(np, "reg", &pa);
		cell->pa = pa;
		va = of_iomap(np, 0);
		cell->va = va;
	}
}

phys_addr_t regs_addr_get_pa(unsigned int id)
{
	phys_addr_t pa;

	if (id >= REGS_ADDR_MAX)
		return 0;

	pa = all_regs_addr[id].pa;

	return pa;
}

void __iomem *regs_addr_get_va(unsigned int id)
{
	static void __iomem *va;

	if (id >= REGS_ADDR_MAX)
		return NULL;

	va = all_regs_addr[id].va;

	return va;
}
