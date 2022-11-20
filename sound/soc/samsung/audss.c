/* sound/soc/samsung/audss.c
 *
 * ALSA SoC Audio Layer - Samsung I2S Controller driver
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd.
 *	HaeKwang Park <haekwang0808.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>

#include <sound/soc.h>
#include <sound/pcm_params.h>

#include <plat/audio.h>
#include <plat/clock.h>
#include <plat/cpu.h>

#include <mach/map.h>
#include <mach/regs-audss.h>

#include "dma.h"
#include "idma.h"
#include "i2s.h"
#include "i2s-regs.h"
#include "srp-type.h"
#include "audss.h"

/* Initialize divider value to avoid over-clock */
#define EXYNOS_AUDSS_DIV_INIT_VAL	(0xF84)

/* Target SRP/BUS clock rate */
#define TARGET_SRPCLK_RATE	(100000000)
#define TARGET_BUSCLK_RATE	(TARGET_SRPCLK_RATE >> 1)

#define msecs_to_loops(t) (loops_per_jiffy / 1000 * HZ * t)

#define LPA_NOISE

/* Lock for audss */
static DEFINE_SPINLOCK(audss_lock);

static atomic_t audss_enable_cnt;

static struct audss_info audss;

void audss_enable(void)
{
	atomic_inc(&audss_enable_cnt);
	pm_runtime_get_sync(audss.rpm_dev);
}

void audss_disable(void)
{
	atomic_dec(&audss_enable_cnt);

	pm_runtime_put_sync(audss.rpm_dev);
}

void audss_iounmap(void)
{
	iounmap(audss.base);
	return;
}

void audss_ioremap(void)
{
	u32 ret;

	audss.base = ioremap(EXYNOS_PA_AUDSS, SZ_4K);

	if (audss.base == NULL) {
		pr_err("cannot ioremap registers\n");
		ret = -ENOMEM;
		return;
	}

	return;
}

void audss_reg_save(void)
{
	audss.suspend_audss_clksrc = readl(audss.base +
					  EXYNOS_CLKSRC_AUDSS_OFFSET);
	audss.suspend_audss_clkdiv = readl(audss.base +
					  EXYNOS_CLKDIV_AUDSS_OFFSET);
	audss.suspend_audss_clkgate = readl(audss.base +
					  EXYNOS_CLKGATE_AUDSS_OFFSET);

	pr_debug("CLKDIV : 0x%08x, CLKGATE : 0x%08x, CLKSRC : 0x%08x\n",
			readl(audss.base + EXYNOS_CLKDIV_AUDSS_OFFSET),
			readl(audss.base + EXYNOS_CLKGATE_AUDSS_OFFSET),
			readl(audss.base + EXYNOS_CLKSRC_AUDSS_OFFSET));

	pr_debug("Registers of Audio Subsystem are saved\n");

	return;
}

void audss_reg_restore(void)
{
	writel(audss.suspend_audss_clkgate,
	       audss.base + EXYNOS_CLKGATE_AUDSS_OFFSET);
	writel(audss.suspend_audss_clkdiv,
	       audss.base + EXYNOS_CLKDIV_AUDSS_OFFSET);
	writel(audss.suspend_audss_clksrc,
	       audss.base + EXYNOS_CLKSRC_AUDSS_OFFSET);

	pr_debug("CLKDIV : 0x%08x, CLKGATE : 0x%08x, CLKSRC : 0x%08x\n",
			readl(audss.base + EXYNOS_CLKDIV_AUDSS_OFFSET),
			readl(audss.base + EXYNOS_CLKGATE_AUDSS_OFFSET),
			readl(audss.base + EXYNOS_CLKSRC_AUDSS_OFFSET));

	pr_debug("Registers of Audio Subsystem are restore\n");
	return;
}


int clk_set_heirachy(void)
{
	unsigned int ret = 0;

	audss.mout_epll = clk_get(NULL, "mout_epll");
	if (IS_ERR(audss.mout_epll)) {
		pr_err("failed to get mout_epll clock\n");
		ret = PTR_ERR(audss.mout_epll);
		goto err0;
	}

	audss.fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(audss.fout_epll)) {
		pr_err("failed to get fout_epll clock\n");
		ret = PTR_ERR(audss.fout_epll);
		goto err1;
	}

	audss.mout_audss = clk_get(NULL, "mout_audss");
	if (IS_ERR(audss.mout_audss)) {
		pr_err("failed to get mout_audss clock\n");
		ret = PTR_ERR(audss.mout_audss);
		goto err2;
	}

	audss.mout_i2s = clk_get(NULL, "mout_i2s");
	if (IS_ERR(audss.mout_i2s)) {
		pr_err("failed to get mout_i2s clock\n");
		ret = PTR_ERR(audss.mout_i2s);
		goto err3;
	}

	audss.dout_srp = clk_get(NULL, "dout_srp");
	if (IS_ERR(audss.dout_srp)) {
		pr_err("failed to get dout_srp div\n");
		ret = PTR_ERR(audss.dout_srp);
		goto err4;
	}

	audss.dout_bus = clk_get(NULL, "dout_bus");
	if (IS_ERR(audss.dout_bus)) {
		pr_err("failed to get dout_bus div\n");
		ret = PTR_ERR(audss.dout_bus);
		goto err5;
	}

	audss.dout_i2s = clk_get(NULL, "dout_i2s");
	if (IS_ERR(audss.dout_i2s)) {
		pr_err("failed to get dout_i2s div\n");
		ret = PTR_ERR(audss.dout_i2s);
		goto err6;
	}

	audss.srpclk = clk_get(NULL, "srpclk");
	if (IS_ERR(audss.srpclk)) {
		pr_err("failed to get srp clock\n");
		ret = PTR_ERR(audss.srpclk);
		goto err7;
	}

	return ret;
err7:
	clk_put(audss.srpclk);
err6:
	clk_put(audss.dout_bus);
err5:
	clk_put(audss.dout_srp);
err4:
	clk_put(audss.mout_i2s);
err3:
	clk_put(audss.mout_audss);
err2:
	clk_put(audss.fout_epll);
err1:
	clk_put(audss.mout_epll);
err0:
	return ret;
}

void audss_do_enable(void)
{
	int ret;
	
	ret = clk_set_parent(audss.mout_epll, audss.fout_epll);
 
	if (ret)
		pr_err( "failed to set parent %s of clock %s.\n",
				audss.fout_epll->name, audss.mout_epll->name);

	if (!audss.clk_init) {
		/* To avoid over-clock */
		writel(EXYNOS_AUDSS_DIV_INIT_VAL,
				audss.base + EXYNOS_CLKDIV_AUDSS_OFFSET);

		ret = clk_set_parent(audss.mout_epll, audss.fout_epll);
		if (ret)
			pr_err("failed to set parent %s of clock %s.\n",
					audss.fout_epll->name, audss.mout_epll->name);

		ret = clk_set_parent(audss.mout_audss, audss.fout_epll);
		if (ret)
			pr_err("failed to set parent %s of clock %s.\n",
					audss.fout_epll->name, audss.mout_audss->name);

		ret = clk_set_parent(audss.mout_i2s, audss.mout_audss);
		if (ret)
			pr_err("failed to set parent %s of clock %s.\n",
					audss.mout_audss->name, audss.mout_i2s->name);

		clk_set_rate(audss.dout_srp, TARGET_SRPCLK_RATE);
		clk_set_rate(audss.dout_bus, TARGET_BUSCLK_RATE);

		pr_info("EPLL rate = %ld\n", clk_get_rate(audss.fout_epll));
		pr_info("SRP rate = %ld\n", clk_get_rate(audss.dout_srp));
		pr_info("BUS rate = %ld\n", clk_get_rate(audss.dout_bus));

		audss.clk_init = true;
		audss_reg_save();
	}

	spin_lock(&audss_lock);
	audss_reg_restore();
#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
	clk_enable(audss.srpclk);
#endif
	srp_core_reset();
 	spin_unlock(&audss_lock);
}

void audss_do_disable(void)
{
	spin_lock(&audss_lock);

#ifdef CONFIG_SND_SAMSUNG_USE_IDMA
	clk_disable(audss.srpclk);
#endif
#ifndef LPA_NOISE
	audss_reg_save();
#endif

	spin_unlock(&audss_lock);
}

struct clk *audss_get_i2s_opclk(int clk_id)
{
	return clk_id ? audss.mout_i2s : audss.dout_bus;
}

#if defined(CONFIG_PM_SLEEP)
static int audss_suspend(struct device *dev)
{
	if (atomic_read(&audss_enable_cnt) > 0) {
#if defined(CONFIG_PM_RUNTIME)
		audss_reg_save();
#else
		audss_do_disable();
#endif
	}

	return 0;
}

static int audss_resume(struct device *dev)
{
	if (atomic_read(&audss_enable_cnt) > 0) {
#if defined(CONFIG_PM_RUNTIME)
		audss_reg_restore();
#else
		audss_do_enable();
#endif
	}

	return 0;
}
#else
static int audss_suspend(struct device *dev)
{
	if (atomic_read(&audss_enable_cnt) > 0)
		audss_do_disable();

        return 0;
}

static int audss_resume(struct device *dev)
{
	if (atomic_read(&audss_enable_cnt) > 0)
		audss_do_enable();

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int audss_runtime_suspend(struct device *dev)
{
        pr_debug("%s entered\n", __func__);

        audss_do_disable();

        return 0;
}

static int audss_runtime_resume(struct device *dev)
{
        pr_debug("%s entered\n", __func__);

        audss_do_enable();

        return 0;
}
#endif

static int audss_probe(struct platform_device *pdev)
{
       pm_runtime_enable(&pdev->dev);

       audss.rpm_dev = &pdev->dev;

       if (audss.rpm_dev == NULL) {
               pr_err("Device is NULL\n");
               return -1;
       }

       return 0;
}

static int audss_remove(struct platform_device *pdev)
{
       pm_runtime_disable(&pdev->dev);

       return 0;
}

static const struct dev_pm_ops audss_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		audss_suspend,
		audss_resume
        )
        SET_RUNTIME_PM_OPS(
		audss_runtime_suspend,
		audss_runtime_resume,
                NULL
        )
};

static struct platform_driver audss_driver = {
       .probe          = audss_probe,
       .remove         = audss_remove,
        .driver         = {
                .name   = "samsung-audss",
                .owner  = THIS_MODULE,
                .pm     = &audss_pmops,
        },
};

static int __init audss_driver_init(void)
{
        return platform_driver_register(&audss_driver);
}
subsys_initcall(audss_driver_init);

/* Module information */
MODULE_AUTHOR("HaeKwang Park, <haekwang0808.park@samsung.com>");
MODULE_DESCRIPTION("Samsung Audio Subsystem Interface");
MODULE_ALIAS("platform:samsung-audss");
MODULE_LICENSE("GPL");
