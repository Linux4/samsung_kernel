/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Based on vexpress-poweroff.c
 *
 */

#include <linux/jiffies.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/stat.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <asm/system_misc.h>

#define REBOOT_TIME (0x20)
#define MPMU_APRR	(0x1020)
#define MPMU_APRR_WDTR	(1<<4)

/* Watchdog Timer Registers Offset */
#define TMR_WMER	(0x0064)
#define TMR_WMR		(0x0068)
#define TMR_WVR		(0x006c)
#define TMR_WCR		(0x0098)
#define TMR_WSR		(0x0070)
#define TMR_WFAR	(0x009c)
#define TMR_WSAR	(0x00A0)

static struct resource *rtc_br0_mem, *wdt_mem, *mpmu_mem;
static void __iomem *rtc_br0_reg, *wdt_base, *mpmu_base;

/* use watchdog to reset system */
void pxa_wdt_reset(void __iomem *watchdog_virt_base, void __iomem *mpmu_vaddr)
{
	u32 reg;
	void __iomem *mpmu_aprr;

	BUG_ON(!watchdog_virt_base);
	BUG_ON(!mpmu_vaddr);

	/* reset counter */
	writel(0xbaba, watchdog_virt_base + TMR_WFAR);
	writel(0xeb10, watchdog_virt_base + TMR_WSAR);
	writel(0x1, watchdog_virt_base + TMR_WCR);

	/*
	 * enable WDT
	 * 1. write 0xbaba to match 1st key
	 * 2. write 0xeb10 to match 2nd key
	 * 3. enable wdt count, generate interrupt when experies
	 */
	writel(0xbaba, watchdog_virt_base + TMR_WFAR);
	writel(0xeb10, watchdog_virt_base + TMR_WSAR);
	writel(0x3, watchdog_virt_base + TMR_WMER);

	/* negate hardware reset to the WDT after system reset */
	mpmu_aprr = mpmu_vaddr + MPMU_APRR;
	reg = readl(mpmu_aprr) | MPMU_APRR_WDTR;
	writel(reg, mpmu_aprr);

	/* clear previous WDT status */
	writel(0xbaba, watchdog_virt_base + TMR_WFAR);
	writel(0xeb10, watchdog_virt_base + TMR_WSAR);
	writel(0, watchdog_virt_base + TMR_WSR);

	writel(0xbaba, watchdog_virt_base + TMR_WFAR);
	writel(0xeb10, watchdog_virt_base + TMR_WSAR);
	writel(REBOOT_TIME, watchdog_virt_base + TMR_WMR);

	return;
}
EXPORT_SYMBOL(pxa_wdt_reset);

static void do_pxa_reset(enum reboot_mode mode, const char *cmd)
{
	u32 backup;
	int i;

	if (cmd && (!strcmp(cmd, "recovery")
		|| !strcmp(cmd, "bootloader") || !strcmp(cmd, "boot")
		|| !strcmp(cmd, "product") || !strcmp(cmd, "prod")
		|| !strcmp(cmd, "fastboot") || !strcmp(cmd, "fast"))) {
		for (i = 0, backup = 0; i < 4; i++) {
			backup <<= 8;
			backup |= *(cmd + i);
		}
		do {
			writel(backup, rtc_br0_reg);
		} while (readl(rtc_br0_reg) != backup);
	}

	pxa_wdt_reset(wdt_base, mpmu_base);

	/* Give a grace period for failure to restart of 1s */
	mdelay(1000);
}

static struct of_device_id pxa_reset_of_match[] = {
	{.compatible = "marvell,pxa-reset",},
	{}
};

static int pxa_reset_probe(struct platform_device *pdev)
{
	wdt_mem = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (wdt_mem == NULL) {
		dev_err(&pdev->dev, "no memory resource specified for WDT\n");
		return -ENOENT;
	}

	wdt_base = devm_ioremap_nocache(&pdev->dev, wdt_mem->start,
					resource_size(wdt_mem));
	if (IS_ERR(wdt_base))
		return PTR_ERR(wdt_base);

	mpmu_mem = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	if (mpmu_mem == NULL) {
		dev_err(&pdev->dev, "no memory resource specified for MPMU\n");
		return -ENOENT;
	}

	mpmu_base = devm_ioremap_nocache(&pdev->dev, mpmu_mem->start,
					 resource_size(mpmu_mem));
	if (IS_ERR(mpmu_base))
		return PTR_ERR(mpmu_base);

	rtc_br0_mem = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	if (rtc_br0_mem == NULL) {
		dev_err(&pdev->dev, "no memory resource specified\n");
		return -ENOENT;
	}

	rtc_br0_reg = devm_ioremap_nocache(&pdev->dev, rtc_br0_mem->start,
					   resource_size(rtc_br0_mem));
	if (IS_ERR(rtc_br0_reg))
		return PTR_ERR(rtc_br0_reg);
	arm_pm_restart = do_pxa_reset;

	return 0;
}

static const struct platform_device_id pxa_reset_id_table[] = {
	{.name = "pxa-reset",},
	{}
};

static struct platform_driver pxa_reset_driver = {
	.probe = pxa_reset_probe,
	.driver = {
		   .name = "pxa-reset",
		   .of_match_table = pxa_reset_of_match,
		   },
	.id_table = pxa_reset_id_table,
};

static int __init pxa_reset_init(void)
{
	return platform_driver_register(&pxa_reset_driver);
}

device_initcall(pxa_reset_init);
