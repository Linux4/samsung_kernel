/*
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Clock support for EXYNOS Audio Subsystem
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <plat/clock.h>
#include <plat/s5p-clock.h>
#include <plat/clock-clksrc.h>

#include <mach/map.h>
#include <mach/regs-audss.h>
#include <mach/regs-clock-exynos5260.h>

#define clk_fin_rpll clk_ext_xtal_mux

static int exynos_clk_audss_mux_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKSRC_ENABLE_AUD, clk, enable);
}

static int exynos_clk_audss_aclk_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_ACLK_AUD, clk, enable);
}

static int exynos_clk_audss_pclk_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_PCLK_AUD, clk, enable);
}

static int exynos_clk_audss_sclk_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_SCLK_AUD, clk, enable);
}

static int exynos_clk_audss_ip_ctrl(struct clk *clk, int enable)
{
	return s5p_gatectrl(EXYNOS5260_CLKGATE_IP_AUD, clk, enable);
}

static struct clk *exynos_clkset_aud_pll_user_list[] = {
	&clk_ext_xtal_mux,
	&clk_fout_epll,
};

static struct clksrc_sources clkset_aud_pll_user = {
	.sources	= exynos_clkset_aud_pll_user_list,
	.nr_sources	= ARRAY_SIZE(exynos_clkset_aud_pll_user_list),
};

static struct clksrc_clk exynos_clk_aud_pll_user = {
	.clk	= {
		.name		= "aud_pll_user",
		/* The aud_pll_user clock cannot be controled by gating.
		   If we are gating this, the system hangs up.
		   So this should be fixed later
		 */
#if 0
		.enable         = exynos_clk_audss_mux_ctrl,
		.ctrlbit        = EXYNOS_LPAUDSS_CLKGATE_AUDPLLUSER,
#endif
	},
	.sources = &clkset_aud_pll_user,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_AUD, .shift = 0, .size = 1 },
};

static struct clk *exynos_clkset_sclk_aud_i2s_list[] = {
	&exynos_clk_aud_pll_user.clk,
};

static struct clksrc_sources clkset_sclk_aud_i2s = {
	.sources	= exynos_clkset_sclk_aud_i2s_list,
	.nr_sources	= ARRAY_SIZE(exynos_clkset_sclk_aud_i2s_list),
};

static struct clksrc_clk exynos_clk_sclk_aud_i2s = {
	.clk	= {
		.name		= "sclk_aud_i2s",
		.enable         = exynos_clk_audss_mux_ctrl,
		.ctrlbit        = EXYNOS_LPAUDSS_CLKGATE_I2SSEL,
	},
	.sources = &clkset_sclk_aud_i2s,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_AUD, .shift = 4, .size = 1 },
};

static struct clk *exynos_clkset_sclk_aud_pcm_list[] = {
	&exynos_clk_aud_pll_user.clk,
};

static struct clksrc_sources clkset_sclk_aud_pcm = {
	.sources	= exynos_clkset_sclk_aud_pcm_list,
	.nr_sources	= ARRAY_SIZE(exynos_clkset_sclk_aud_pcm_list),
};

static struct clksrc_clk exynos_clk_sclk_aud_pcm = {
	.clk	= {
		.name		= "sclk_aud_pcm",
		.enable         = exynos_clk_audss_mux_ctrl,
		.ctrlbit        = EXYNOS_LPAUDSS_CLKGATE_PCMSEL,
	},
	.sources = &clkset_sclk_aud_pcm,
	.reg_src = { .reg = EXYNOS5260_CLKSRC_SEL_AUD, .shift = 8, .size = 1 },
};

static struct clksrc_clk exynos_clk_aclk_aud_131_nogate = {
	.clk	= {
		.name		= "aclk_aud_131_nogate",
		.parent		= &exynos_clk_aud_pll_user.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_AUD0, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos_clk_sclk_aud_i2s_gated = {
	.clk	= {
		.name		= "dout_sclk_i2s",
		.parent		= &exynos_clk_sclk_aud_i2s.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_AUD1, .shift = 0, .size = 4 },
};

static struct clksrc_clk exynos_clk_sclk_aud_pcm_gated = {
	.clk	= {
		.name		= "dout_sclk_pcm",
		.parent		= &exynos_clk_sclk_aud_pcm.clk,
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_AUD1, .shift = 4, .size = 8 },
};

static struct clksrc_clk exynos_clk_sclk_aud_uart_gated = {
	.clk	= {
		.name		= "clk_uart_baud0",
		.devname	= "exynos4210-uart.3",
		.parent		= &exynos_clk_aud_pll_user.clk,
		.enable		= exynos_clk_audss_ip_ctrl,
		.ctrlbit	= (1 << 4),
	},
	.reg_div = { .reg = EXYNOS5260_CLKDIV_AUD1, .shift = 12, .size = 4 },
};

/* Clock initialization code */
static struct clksrc_clk *exynos_audss_clks[] = {
	&exynos_clk_aud_pll_user,
	&exynos_clk_sclk_aud_i2s,
	&exynos_clk_sclk_aud_pcm,
	&exynos_clk_aclk_aud_131_nogate,
	&exynos_clk_sclk_aud_i2s_gated,
	&exynos_clk_sclk_aud_pcm_gated,
	&exynos_clk_sclk_aud_uart_gated,
};

static struct clk exynos_init_audss_clocks[] = {
	{
		.name		= "audss",
		.enable		= exynos_clk_audss_pclk_ctrl,
		.ctrlbit	= EXYNOS_LPAUDSS_CLKGATE_PCLK_DMAC |
				  EXYNOS_LPAUDSS_CLKGATE_PCLK_SFRCTRL |
				  EXYNOS_LPAUDSS_CLKGATE_PCLK_SYSREG |
				  EXYNOS_LPAUDSS_CLKGATE_PCLK_PMU |
				  EXYNOS_LPAUDSS_CLKGATE_PCLK_GPIO,
	}, {
		.name		= "iis",
		.devname	= "samsung-i2s.0",
		.enable		= exynos_clk_audss_pclk_ctrl,
		.ctrlbit	= EXYNOS_LPAUDSS_CLKGATE_PCLK_I2S,
	}, {
		.name		= "iis",
		.devname	= "samsung-i2s.4",
		.enable		= exynos_clk_audss_pclk_ctrl,
		.ctrlbit	= EXYNOS_LPAUDSS_CLKGATE_PCLK_I2S,
	}, {
		.name		= "sclk_i2s_gated",
		.devname	= "samsung-i2s.0",
		.enable		= exynos_clk_audss_sclk_ctrl,
		.ctrlbit	= EXYNOS_LPAUDSS_CLKGATE_SCLK_I2S,
	}, {
		.name		= "pcm",
		.devname	= "samsung-pcm.0",
		.enable		= exynos_clk_audss_pclk_ctrl,
		.ctrlbit	= EXYNOS_LPAUDSS_CLKGATE_PCLK_PCM,
	}, {
		.name		= "sclk_pcm_gated",
		.devname	= "samsung-pcm.0",
		.enable		= exynos_clk_audss_sclk_ctrl,
		.ctrlbit	= EXYNOS_LPAUDSS_CLKGATE_SCLK_PCM,
	}, {
		.name		= "uart",
		.devname	= "exynos4210-uart.3",
		.parent		= &exynos_clk_aclk_aud_131_nogate.clk,
		.enable		= exynos_clk_audss_ip_ctrl,
		.ctrlbit	= EXYNOS_LPAUDSS_CLKGATE_PCLK_UART,
	}, {
		.name		= "aclk_aud",
		.enable		= exynos_clk_audss_aclk_ctrl,
		.ctrlbit	= EXYNOS_LPAUDSS_CLKGATE_ACLK_DMAC |
				  EXYNOS_LPAUDSS_CLKGATE_ACLK_SRAM |
				  EXYNOS_LPAUDSS_CLKGATE_ACLK_AUDNP |
				  EXYNOS_LPAUDSS_CLKGATE_ACLK_AUDND |
				  EXYNOS_LPAUDSS_CLKGATE_ACLK_XIULPASS |
				  EXYNOS_LPAUDSS_CLKGATE_ACLK_AXI2APB |
				  EXYNOS_LPAUDSS_CLKGATE_ACLK_AXIDS,
	}
};

void __init exynos_register_audss_clocks(void)
{
	int ptr;

	for (ptr = 0; ptr < ARRAY_SIZE(exynos_audss_clks); ptr++)
		s3c_register_clksrc(exynos_audss_clks[ptr], 1);

	s3c_register_clocks(exynos_init_audss_clocks, ARRAY_SIZE(exynos_init_audss_clocks));

	clk_set_rate(&clk_fout_epll, 192000000);
	clk_enable(&clk_fout_epll);

	/* Audio subsystem interrupt enable */
	writel(1, S5P_VA_AUDSS+EXYNOS5260_AUDSS_CPU_INTR_OFFSET);

	/* enable interrupt of ADMA in audss */
	writel((1<<6), S5P_VA_AUDSS+EXYNOS5260_AUDSS_INTR_ENABLE_OFFSET);

	/* s/w reset AUDSS IPs */
	writel(0xf3e,  S5P_VA_AUDSS+EXYNOS5260_AUDSS_SWRESET_OFFSET);
	writel(0xf3f,  S5P_VA_AUDSS+EXYNOS5260_AUDSS_SWRESET_OFFSET);

#if 0
	/* all clock for aud should be disable at the init time
           this should be enable */
	s3c_disable_clocks(exynos_init_audss_clocks, ARRAY_SIZE(exynos_init_audss_clocks));

	/* turn clocks off. out of muxes of CMU_AUD */
	s3c_disable_clocks(&exynos_clk_aud_pll_user.clk, 1);
        s3c_disable_clocks(&exynos_clk_sclk_aud_i2s.clk, 1);
        s3c_disable_clocks(&exynos_clk_sclk_aud_pcm.clk, 1);
#endif
}
