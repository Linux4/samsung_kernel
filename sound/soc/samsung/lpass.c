/* sound/soc/samsung/i2s.c
 *
 * ALSA SoC Audio Layer - Samsung I2S Controller driver
 *
 * Copyright (c) 2010 Samsung Electronics Co. Ltd.
 *	Jaswinder Singh <jassisinghbrar@gmail.com>
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
#include <sound/exynos.h>

#include <plat/audio.h>
#include <plat/clock.h>
#include <plat/cpu.h>

#include <mach/map.h>
#include <mach/regs-audss.h>
#include <mach/regs-pmu.h>

#include "dma.h"
#include "idma.h"
#include "i2s.h"
#include "i2s-regs.h"
#include "lpass.h"

/* Initialize divider value to avoid over-clock */
#define EXYNOS5260_AUDSS_DIV0_INIT_VAL	(0x1)
#define EXYNOS5260_AUDSS_DIV1_INIT_VAL	(0x0FFF)

/* Target clock rate */
#define TARGET_AUD_PLL_RATE	(196608000)
#define TARGET_SCLK_AUD_I2S	(TARGET_AUD_PLL_RATE / 4)
#define TARGET_SCLK_PERI_AUD	(TARGET_AUD_PLL_RATE / 8)

#define AUDSS_SWRESET_OFFSET		(0x8)
#define AUDSS_CPU_INTR_OFFSET		(0x50)
#define AUDSS_ENABLE_INTR_OFFSET	(0x58)

#define msecs_to_loops(t) (loops_per_jiffy / 1000 * HZ * t)

#define SLEEP	(0)
#define RUNTIME	(1)

/* Lock for audss */
static DEFINE_SPINLOCK(audss_lock);
static atomic_t audss_enable_cnt;
static struct audss_info audss;

static int lpass_runtime_suspend(struct device *dev);
static int lpass_runtime_resume(struct device *dev);

void audss_enable(void)
{
	atomic_inc(&audss_enable_cnt);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_get_sync(audss.dev);
#else
	if (atomic_read(&audss_enable_cnt) == 1)
		lpass_runtime_resume(audss.dev);
#endif
}

void audss_disable(void)
{
	atomic_dec(&audss_enable_cnt);

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_put_sync(audss.dev);
#else
	if (atomic_read(&audss_enable_cnt) == 0)
		lpass_runtime_suspend(audss.dev);
#endif
}

void audss_sw_reset(void)
{
	u32 tmp;

	pr_debug("Entered %s function\n", __func__);

        /* s/w reset ADMA block to access by NSEC */
        tmp = readl(audss.audss_base + AUDSS_SWRESET_OFFSET);
        tmp &= 0xffe;
        writel(tmp, audss.audss_base + AUDSS_SWRESET_OFFSET);
        tmp |= 0x1;
        writel(tmp, audss.audss_base + AUDSS_SWRESET_OFFSET);

        /* enable ADMA interrupt */
        writel(0x1, audss.audss_base + AUDSS_CPU_INTR_OFFSET);
        writel((0x1 << 6) | (0x1 << 1),
		audss.audss_base + EXYNOS5260_AUDSS_INTR_ENABLE_OFFSET);

        return;
}

void audss_reg_save(void)
{
	audss.suspend_audss_clksrc_sel = readl(audss.audss_cmu_base +
					EXYNOS5260_CLKSRC_AUDSS_SEL_OFFSET);
	audss.suspend_audss_clksrc_clkgate = readl(audss.audss_cmu_base +
					EXYNOS5260_CLKSRC_AUDSS_GATE_OFFSET);
	audss.suspend_audss_clkdiv0 = readl(audss.audss_cmu_base +
					EXYNOS5260_CLKDIV_AUDSS_0_OFFSET);
	audss.suspend_audss_clkdiv1 = readl(audss.audss_cmu_base +
					EXYNOS5260_CLKDIV_AUDSS_1_OFFSET);
	audss.suspend_audss_aclk_clkgate = readl(audss.audss_cmu_base +
					EXYNOS5260_CLKGATE_AUDSS_ACLK_OFFSET);
	audss.suspend_audss_pclk_clkgate = readl(audss.audss_cmu_base +
					EXYNOS5260_CLKGATE_AUDSS_PCLK_OFFSET);
	audss.suspend_audss_sclk_clkgate = readl(audss.audss_cmu_base +
					EXYNOS5260_CLKGATE_AUDSS_SCLK_OFFSET);

	pr_debug("CLKSRC : 0x%08x, CLKSRC_GATE : 0x%08x, CLKDIV0 : 0x%08x\n"
		"CLKDIV0 : 0x%08x, CLKGATE_ACLK : 0x%08x, "
		"CLKGATE_SCLK : 0x%08x, CLKGATE_PCLK : 0x%08x\n",
		readl(audss.audss_cmu_base + EXYNOS5260_CLKSRC_AUDSS_SEL_OFFSET),
		readl(audss.audss_cmu_base + EXYNOS5260_CLKSRC_AUDSS_GATE_OFFSET),
		readl(audss.audss_cmu_base + EXYNOS5260_CLKDIV_AUDSS_0_OFFSET),
		readl(audss.audss_cmu_base + EXYNOS5260_CLKDIV_AUDSS_1_OFFSET),
		readl(audss.audss_cmu_base + EXYNOS5260_CLKGATE_AUDSS_ACLK_OFFSET),
		readl(audss.audss_cmu_base + EXYNOS5260_CLKGATE_AUDSS_PCLK_OFFSET),
		readl(audss.audss_cmu_base + EXYNOS5260_CLKGATE_AUDSS_SCLK_OFFSET));

	pr_debug("Registers of Audio Subsystem are saved\n");

	return;
}

void audss_reg_restore(void)
{
	writel(audss.suspend_audss_clksrc_sel,
		audss.audss_cmu_base + EXYNOS5260_CLKSRC_AUDSS_SEL_OFFSET);
	writel(audss.suspend_audss_clksrc_clkgate,
		audss.audss_cmu_base + EXYNOS5260_CLKSRC_AUDSS_GATE_OFFSET);
	writel(audss.suspend_audss_clkdiv0,
		audss.audss_cmu_base + EXYNOS5260_CLKDIV_AUDSS_0_OFFSET);
	writel(audss.suspend_audss_clkdiv1,
		audss.audss_cmu_base + EXYNOS5260_CLKDIV_AUDSS_1_OFFSET);
	writel(audss.suspend_audss_aclk_clkgate,
		audss.audss_cmu_base + EXYNOS5260_CLKGATE_AUDSS_ACLK_OFFSET);
	writel(audss.suspend_audss_pclk_clkgate,
		audss.audss_cmu_base + EXYNOS5260_CLKGATE_AUDSS_PCLK_OFFSET);
	writel(audss.suspend_audss_sclk_clkgate,
		audss.audss_cmu_base + EXYNOS5260_CLKGATE_AUDSS_SCLK_OFFSET);

	pr_debug("CLKSRC : 0x%08x, CLKSRC_GATE : 0x%08x, CLKDIV0 : 0x%08x\n"
		"CLKDIV0 : 0x%08x, CLKGATE_ACLK : 0x%08x, "
		"CLKGATE_SCLK : 0x%08x, CLKGATE_PCLK : 0x%08x\n",
		readl(audss.audss_cmu_base + EXYNOS5260_CLKSRC_AUDSS_SEL_OFFSET),
		readl(audss.audss_cmu_base + EXYNOS5260_CLKSRC_AUDSS_GATE_OFFSET),
		readl(audss.audss_cmu_base + EXYNOS5260_CLKDIV_AUDSS_0_OFFSET),
		readl(audss.audss_cmu_base + EXYNOS5260_CLKDIV_AUDSS_1_OFFSET),
		readl(audss.audss_cmu_base + EXYNOS5260_CLKGATE_AUDSS_ACLK_OFFSET),
		readl(audss.audss_cmu_base + EXYNOS5260_CLKGATE_AUDSS_PCLK_OFFSET),
		readl(audss.audss_cmu_base + EXYNOS5260_CLKGATE_AUDSS_SCLK_OFFSET));

	pr_debug("Registers of Audio Subsystem are restore\n");
	return;
}

int clk_set_heirachy(void)
{
	unsigned int ret = 0;

	audss.ext_xtal = clk_get(NULL, "ext_xtal");
	if (IS_ERR(audss.ext_xtal)) {
		pr_err("failed to get ext_xtal clock\n");
		ret = PTR_ERR(audss.ext_xtal);
		goto err0;
	}

	audss.fout_epll = clk_get(NULL, "fout_epll");
	if (IS_ERR(audss.fout_epll)) {
		pr_err("failed to get fout_epll clock\n");
		ret = PTR_ERR(audss.fout_epll);
		goto err1;
	}

        audss.aud_user = clk_get(NULL, "aud_pll_user");
        if (IS_ERR(audss.aud_user)) {
                pr_err("failed to get aud_pll_user clock\n");
                ret = PTR_ERR(audss.aud_user);
                goto err2;
        }

        audss.sclk_i2s = clk_get(NULL, "dout_sclk_i2s");
        if (IS_ERR(audss.sclk_i2s)) {
                pr_err("failed to get dout_sclk_i2s clock\n");
                ret = PTR_ERR(audss.sclk_i2s);
                goto err3;
        }

	audss.clk_audss = clk_get(NULL, "audss");
	if (IS_ERR(audss.clk_audss)) {
		pr_err("failed to get audss clock\n");
		ret = PTR_ERR(audss.clk_audss);
		goto err4;
	}

	audss.sclk_epll = clk_get(NULL, "sclk_epll");
	if (IS_ERR(audss.sclk_epll)) {
		pr_err("failed to get sclk_epll clock\n");
		ret = PTR_ERR(audss.sclk_epll);
		goto err5;
	}

	audss.sclk_top_pll = clk_get(NULL, "sclk_audtop_pll");
	if (IS_ERR(audss.sclk_top_pll)) {
		pr_err("failed to get sclk_audtop_pll clock\n");
		ret = PTR_ERR(audss.sclk_top_pll);
		goto err6;
	}

	audss.sclk_peri = clk_get(NULL, "sclk_peri_aud");
	if (IS_ERR(audss.sclk_peri)) {
		pr_err("failed to get sclk_peri_aud clock\n");
		ret = PTR_ERR(audss.sclk_peri);
		goto err7;
	}

	return ret;

err7:
	clk_put(audss.sclk_top_pll);
err6:
	clk_put(audss.sclk_epll);
err5:
	clk_put(audss.clk_audss);
err4:
	clk_put(audss.sclk_i2s);
err3:
	clk_put(audss.aud_user);
err2:
	clk_put(audss.fout_epll);
err1:
	clk_put(audss.ext_xtal);
err0:
	return ret;
}

void audss_uart_enable(void)
{
	if (audss.audss_cmu_base == NULL)
		return;

	audss_enable();
}

void audss_uart_disable(void)
{
	if (audss.audss_cmu_base == NULL)
		return;

	audss_disable();
}

void lpass_enable(void)
{
	int ret;

	if (!audss.clk_init) {
		/* To avoid over-clock */
		writel(EXYNOS5260_AUDSS_DIV0_INIT_VAL,
			audss.audss_cmu_base + EXYNOS5260_CLKDIV_AUDSS_0_OFFSET);
		writel(EXYNOS5260_AUDSS_DIV1_INIT_VAL,
			audss.audss_cmu_base + EXYNOS5260_CLKDIV_AUDSS_1_OFFSET);

		clk_set_rate(audss.fout_epll, TARGET_AUD_PLL_RATE);

		ret = clk_set_parent(audss.aud_user, audss.fout_epll);
		if (ret)
			pr_err("failed to set parent %s of clock %s.\n",
					audss.aud_user->name, audss.sclk_i2s->name);

		ret = clk_set_parent(audss.sclk_i2s, audss.aud_user);
		if (ret)
			pr_err("failed to set parent %s of clock %s.\n",
					audss.sclk_i2s->name, audss.aud_user->name);

		/* set sclk_i2s clock as 49.152Mhz */
		clk_set_rate(audss.sclk_i2s, TARGET_SCLK_AUD_I2S);

		/* top peri */
		ret = clk_set_parent(audss.sclk_epll, audss.fout_epll);
		if (ret)
			pr_err("failed to set parent %s of clock %s.\n",
					audss.sclk_epll->name, audss.fout_epll->name);

		ret = clk_set_parent(audss.sclk_top_pll, audss.sclk_epll);
		if (ret)
			pr_err("failed to set parent %s of clock %s.\n",
					audss.sclk_top_pll->name, audss.sclk_epll->name);

		/* set sclk_peri_aud clock as 24.576Mhz */
		clk_set_rate(audss.sclk_peri, TARGET_SCLK_PERI_AUD);

		pr_info("EPLL rate = %ld\n", clk_get_rate(audss.fout_epll));
		pr_info("aud_user rate = %ld\n", clk_get_rate(audss.aud_user));
		pr_info("sclk_i2s rate = %ld\n", clk_get_rate(audss.sclk_i2s));
		pr_info("sclk_peri rate = %ld\n", clk_get_rate(audss.sclk_peri));

		audss.clk_init = 1;
		audss_reg_save();
	}

	spin_lock(&audss_lock);

	audss_sw_reset();
	audss_reg_restore();

	clk_set_parent(audss.aud_user, audss.fout_epll);

	/* enable clocks */
	clk_enable(audss.clk_audss);

	if (exynos_maudio_uart_cfg_gpio_pdn != NULL)
		exynos_maudio_uart_cfg_gpio_pdn();

	__raw_writel(0x10000000, EXYNOS_PAD_RET_MAUDIO_OPTION);

	__raw_writel(0x1, EXYNOS5260_GPIO_MODE_AUD_SYS_PWR_REG);

	spin_unlock(&audss_lock);
}

void lpass_disable(void)
{
	int ret;

	pr_debug("Enter %s\n", __func__);

	spin_lock(&audss_lock);

	if (exynos_maudio_uart_cfg_gpio_pdn != NULL)
		exynos_maudio_uart_cfg_gpio_pdn();

	__raw_writel(0x1, EXYNOS5260_GPIO_MODE_AUD_SYS_PWR_REG);

	clk_disable(audss.clk_audss);

	audss_reg_save();

	ret = clk_set_parent(audss.aud_user, audss.ext_xtal);
	if (ret)
		pr_err("failed to set parent %s of clock %s.\n",
				audss.aud_user->name, audss.ext_xtal->name);

	spin_unlock(&audss_lock);
}

struct clk *audss_get_i2s_opclk(int clk_id)
{
	/* op clk */
	return audss.sclk_i2s;
}

#if defined(CONFIG_PM_SLEEP)
static int lpass_suspend(struct device *dev)
{
	if (atomic_read(&audss_enable_cnt) > 0) {
#if defined(CONFIG_PM_RUNTIME)
		audss_reg_save();
#else
	        lpass_disable();
#endif
	}

        return 0;
}

static int lpass_resume(struct device *dev)
{
	if (atomic_read(&audss_enable_cnt) > 0) {
#if defined(CONFIG_PM_RUNTIME)
		audss_sw_reset();
		audss_reg_restore();
	        __raw_writel(0x10000000, EXYNOS_PAD_RET_MAUDIO_OPTION);
#else
	        lpass_enable();
#endif
	}

        return 0;
}
#else
static int lpass_suspend(struct device *dev)
{
	if (atomic_read(&audss_enable_cnt) > 0)
		lpass_disable();

	return 0;
}

static int lpass_resume(struct device *dev)
{
	if (atomic_read(&audss_enable_cnt) > 0)
		lpass_enable();

        return 0;
}
#endif

static int lpass_runtime_suspend(struct device *dev)
{
        pr_debug("%s entered\n", __func__);

        lpass_disable();

        return 0;
}

static int lpass_runtime_resume(struct device *dev)
{
        pr_debug("%s entered\n", __func__);

        lpass_enable();

        return 0;
}

static int lpass_probe(struct platform_device *pdev)
{
	atomic_set(&audss_enable_cnt, 0);
	audss.dev = &pdev->dev;

	audss.audss_cmu_base = ioremap(EXYNOS5260_PA_CMU_AUDIO, SZ_4K);
	if (audss.audss_cmu_base == NULL) {
		dev_err(&pdev->dev, "cannot ioremap cmu registers\n");
		return -ENOMEM;
	}

	audss.audss_base = ioremap(EXYNOS5260_PA_AUDSS, SZ_256);
	if (audss.audss_base == NULL) {
		dev_err(&pdev->dev, "cannot ioremap registers\n");
		iounmap(audss.audss_cmu_base);
		return -ENOMEM;
	}

	if (clk_set_heirachy() < 0) {
		dev_err(&pdev->dev, "clk initialization failed\n");
		iounmap(audss.audss_cmu_base);
		iounmap(audss.audss_base);
		return -ENODEV;
	}

	pm_runtime_enable(&pdev->dev);

	return 0;
}

static int lpass_remove(struct platform_device *pdev)
{
	pm_runtime_disable(&pdev->dev);
	iounmap(audss.audss_cmu_base);
	iounmap(audss.audss_base);

	return 0;
}

static const struct dev_pm_ops lpass_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		lpass_suspend,
		lpass_resume
	)
        SET_RUNTIME_PM_OPS(
		lpass_runtime_suspend,
		lpass_runtime_resume,
		NULL
	)
};

static struct platform_driver lpass_driver = {
	.probe		= lpass_probe,
	.remove		= lpass_remove,
	.driver		= {
		.name	= "samsung-lpass",
		.owner	= THIS_MODULE,
		.pm	= &lpass_pmops,
	},
};

static int __init lpass_driver_init(void)
{
        return platform_driver_register(&lpass_driver);
}
subsys_initcall(lpass_driver_init);

/* Module information */
MODULE_AUTHOR("Haekwang Park, <haekwang0808.park@samsung.com>");
MODULE_DESCRIPTION("Samsung Low Power Audio Subsystem Interface");
MODULE_ALIAS("platform:samsung-lpass");
MODULE_LICENSE("GPL");
