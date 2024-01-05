/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/clk.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/module.h>

#include <plat/gpio-cfg.h>
#include <plat/devs.h>
#include <plat/clock.h>
#include <plat/s5p-clock.h>

#include <mach/map.h>
#include <mach/regs-pmu.h>
#include <mach/regs-audss.h>
#include <mach/regs-clock.h>
#include <mach/pmu.h>
#include <mach/exynos4-audio.h>

#include <mach/regs-clock-exynos4415.h>

static struct clk *mclk_clkout;
static unsigned int mclk_usecount;

static struct platform_device *exynos4_audio_devices[] __initdata = {
	&exynos_device_audss,
#ifdef CONFIG_SND_SAMSUNG_ALP
	&exynos4_device_srp,
#endif
#ifdef CONFIG_SND_SAMSUNG_I2S
	&exynos4_device_i2s0,
#endif
	&samsung_asoc_dma,
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
	&samsung_asoc_idma,
#endif
};

static void exynos4_audio_setup_clocks(void)
{
	struct clk *xusbxti = NULL;
	struct clk *clkout = NULL;

	writel(0x717, EXYNOS_CLKDIV_AUDSS);
	clk_set_rate(&clk_fout_epll, 786432000);

	xusbxti = clk_get(NULL, "xusbxti");
	if (!xusbxti) {
		pr_err("%s: cannot get xusbxti clock\n", __func__);
		return;
	}

	clkout = clk_get(NULL, "clkout");
	if (!clkout) {
		pr_err("%s: cannot get clkout\n", __func__);
		clk_put(xusbxti);
		return;
	}

	clk_set_parent(clkout, xusbxti);
	clk_put(clkout);
	clk_put(xusbxti);

	mclk_clkout = clk_get(NULL, "clkout");
	if (!mclk_clkout) {
		pr_err("%s: cannot get get clkout (clkout)\n", __func__);
		return;
	}
}

void exynos4_audio_set_mclk(bool enable, bool forced)
{
	if (forced) {
		mclk_usecount = 0;
		clk_disable(mclk_clkout);
		return;
	}

	if (enable) {
		if (mclk_usecount == 0) {
			clk_enable(mclk_clkout);
			pr_info("%s: mclk enable\n", __func__);
		}

		mclk_usecount++;
	} else {
		if (--mclk_usecount > 0)
			return;

		clk_disable(mclk_clkout);
		pr_info("%s: mclk disable\n", __func__);
	}
}
EXPORT_SYMBOL(exynos4_audio_set_mclk);

void __init exynos4_audio_init(void)
{
	exynos4_audio_setup_clocks();

	platform_add_devices(exynos4_audio_devices,
			ARRAY_SIZE(exynos4_audio_devices));
}
