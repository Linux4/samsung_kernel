/* linux/drivers/decon_display/decon_mic.c
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Haowei Li <haowei.li@samsung.com>
 *
 * Samsung MIC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/io.h>
#include <mach/regs-clock.h>

#include "decon_mic.h"
#include "decon_display_driver.h"
#include "decon_mipi_dsi.h"
#include "decon_fb.h"
#include "decon_dt.h"
#include "decon_pm.h"
#include "mic_reg.h"

#ifdef CONFIG_OF
static const struct of_device_id exynos5_mic[] = {
	{ .compatible = "samsung,exynos5-mic" },
	{},
};
MODULE_DEVICE_TABLE(of, exynos5_mic);
#endif

struct decon_mic *mic_for_decon_ext;
EXPORT_SYMBOL(mic_for_decon_ext);

#ifdef CONFIG_FB_HIBERNATION_DISPLAY2
int decon_mic1_hibernation_power_on(struct display_driver *dispdrv);
int decon_mic1_hibernation_power_off(struct display_driver *dispdrv);
#endif

static int decon_mic_set_sys_reg(struct decon_mic *mic, bool enable)
{
	u32 data;
	void __iomem *sysreg_va;

	sysreg_va = ioremap(mic->mic_config->sysreg1, 0x4);

	if (enable) {
		data = readl(sysreg_va) & ~(0xc0);
		data |= (1 << 9) | (1 << 7) | (1 << 6);
#if defined(CONFIG_FB_I80_HW_TRIGGER)
		data |= (1 << 13);
#endif
		writel(data, sysreg_va);
		iounmap(sysreg_va);

#ifndef CONFIG_SOC_EXYNOS5433
		sysreg_va = ioremap(mic->mic_config->sysreg2, 0x4);
		writel(0x80000000, sysreg_va);
		iounmap(sysreg_va);
#endif
	} else {
		data = readl(sysreg_va) & ~(0xc0);
		writel(data, sysreg_va);
		iounmap(sysreg_va);
	}

	return 0;
}

int decon_mic_enable_ext(struct decon_mic *mic)
{
	if (!mic->lcd->mic)
		return 0;
	if (mic->decon_mic_on == true)
		return 0;

	decon_mic_set_sys_reg(mic, DECON_MIC_ON);
	mic_reg_start_ext(mic->lcd);

	mic->decon_mic_on = true;

	dev_dbg(mic->dev, "MIC driver is ON;\n");

	return 0;
}

int decon_mic_disable_ext(struct decon_mic *mic)
{
	if (!mic->lcd->mic)
		return 0;
	if (mic->decon_mic_on == false)
		return 0;

	mic_reg_stop_ext(mic->lcd);
	decon_mic_set_sys_reg(mic, DECON_MIC_OFF);

	mic->decon_mic_on = false;

	dev_dbg(mic->dev, "MIC driver is OFF;\n");

	return 0;
}

int create_decon_mic_ext(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct display_driver *dispdrv;
	struct decon_mic *mic;
	struct resource *res;

	dispdrv = get_display_driver_ext();

	mic = devm_kzalloc(dev, sizeof(struct decon_mic), GFP_KERNEL);
	if (!mic) {
		mic_err("no memory for mic driver");
		return -ENOMEM;
	}

	mic->dev = dev;

	mic->lcd = decon_get_lcd_info_ext();

	mic->mic_config = dispdrv->dt_ops.get_display_mic_config();

	mic->decon_mic_on = false;

	res = dispdrv->mic_driver.regs;
	if (!res) {
		mic_err("failed to find resource\n");
		return -ENOENT;
	}

	mic->reg_base = ioremap(res->start, resource_size(res));
	if (!mic->reg_base) {
		mic_err("failed to map registers\n");
		return -ENXIO;
	}

	mic_for_decon_ext = mic;

	dispdrv->mic_driver.mic = mic;

#ifdef CONFIG_S5P_LCD_EXT_INIT
	decon_mic_enable_ext(mic);
#endif
#ifdef CONFIG_FB_HIBERNATION_DISPLAY2
	dispdrv->mic_driver.ops->pwr_on = decon_mic1_hibernation_power_on;
	dispdrv->mic_driver.ops->pwr_off = decon_mic1_hibernation_power_off;
#endif

	mic_info("MIC driver has been probed\n");
	return 0;
}

#ifdef CONFIG_FB_HIBERNATION_DISPLAY2
static int decon_mic_sw_reset(struct decon_mic *mic)
{
	void __iomem *regs = mic->reg_base + MIC_OP;

	u32 data = readl(regs);

	data |= MIC_OP_SW_RST;
	writel(data, regs);

	return 0;
}


int decon_mic1_hibernation_power_on(struct display_driver *dispdrv)
{
	struct decon_mic *mic = dispdrv->mic_driver.mic;

	decon_mic_enable_ext(mic);

	return 0;
}

int decon_mic1_hibernation_power_off(struct display_driver *dispdrv)
{
	struct decon_mic *mic = dispdrv->mic_driver.mic;

	/* decon_mic_set_mic_base_operation(mic, DECON_MIC_OFF); */
	decon_mic_sw_reset(mic); /* should be fixed */
	/* decon_mic_set_sys_reg(mic, DECON_MIC_OFF); */

	decon_mic_disable_ext(mic);

	mic->decon_mic_on = false;

	return 0;
}
#endif

MODULE_AUTHOR("Haowei Li <Haowei.li@samsung.com>");
MODULE_DESCRIPTION("Samsung MIC driver");
MODULE_LICENSE("GPL");
