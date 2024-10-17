/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>

#include <soc/samsung/exynos/debug-snapshot.h>

#include "debug-snapshot-s3c2410-wdt.h"

struct dss_s3c2410_builtin_wdt {
	struct device		*dev;
	void __iomem		*reg_base;
};

static struct dss_s3c2410_builtin_wdt *dss_s3c_reset_wdt;

static void do_s3c2410wdt_dss_builtin_expire_watchdog(struct dss_s3c2410_builtin_wdt *wdt)
{
	unsigned long wtcon;

	/* emergency reset with wdt reset */
	wtcon = readl(wdt->reg_base + S3C2410_WTCON);
	wtcon |= S3C2410_WTCON_RSTEN | S3C2410_WTCON_ENABLE;

	writel(0x00, wdt->reg_base + EXYNOS_WTMINCNT);
	writel(0x00, wdt->reg_base + S3C2410_WTDAT);
	writel(0x01, wdt->reg_base + S3C2410_WTCNT);
	writel(wtcon, wdt->reg_base + S3C2410_WTCON);
}

int s3c2410wdt_builtin_expire_watchdog_raw(unsigned int unused1, int unused2)
{
	struct dss_s3c2410_builtin_wdt *wdt = dss_s3c_reset_wdt;

	do_s3c2410wdt_dss_builtin_expire_watchdog(wdt);
	cpu_park_loop();

	return 0;
}

static const struct of_device_id s3c2410_dss_builtin_wdt_dt_ids[] = {
	 { .compatible = "samsung,s3c2410-dss-builtin-wdt" },
	 { }
};

int register_dss_builtin_wdt(struct device *dev)
{
	struct dss_s3c2410_builtin_wdt *wdt;
	struct device_node *np;
	const struct of_device_id *match;
	u32 base;
	int ret = 0;

	/*
	 * devm_platform_ioremap_resource cannot be used here.
	 * Because normal WDT Driver module use it to handle kicking WDT
	 * and do emergency reset to support sudden WDT for modules.
	 * So use only of_iomap. With just writing values to SFR in WDT device,
	 * WDT reset will be triggered.
	 * But usually, it must be done after normal WDT module driver probed.
	 */
	np = of_find_matching_node_and_match(dev->of_node,
					 s3c2410_dss_builtin_wdt_dt_ids, &match);

	if (!np || !match) {
		dev_info(dev, "%s: No builtin-dss-wdt node or match\n", __func__);
		return 0;
	}

	/* get the memory region for the watchdog timer */
	ret = of_property_read_u32(np, "wdt_base", &base);
	if (ret) {
		dev_err(dev, "%s: fail to find wdt_base\n", __func__);
		return ret;
	}

	wdt = devm_kzalloc(dev, sizeof(*wdt), GFP_KERNEL);
	if (!wdt)
		return -ENOMEM;

	wdt->reg_base = ioremap((phys_addr_t)base, 0x20);
	if (IS_ERR(wdt->reg_base)) {
		ret = PTR_ERR(wdt->reg_base);
		return ret;
	}
	wdt->dev = dev;

	dss_s3c_reset_wdt = wdt;
	dbg_snapshot_register_wdt_ops(NULL, (void *)s3c2410wdt_builtin_expire_watchdog_raw, NULL);

	dev_info(dev, "%s: done\n", __func__);

	return ret;
}
