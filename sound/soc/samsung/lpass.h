#ifndef __SND_SOC_SAMSUNG_AUDSS_H
#define __SND_SOC_SAMSUNG_AUDSS_H

struct audss_info {

	/* AUDSS CMU SFRs */
	void __iomem	*audss_cmu_base;
	/* AUDSS SFRs */
	void __iomem	*audss_addr;
	/* Physical base address of SFRs */
	u32	base;

	void __iomem	*audss_base;
	struct device	*dev;

	/* Audss's source clock */
	struct clk *ext_xtal;

	struct clk *fout_epll;

	/* I2S Controller's core clock */
	struct clk *aud_user;
	struct clk *sclk_i2s;
	struct clk *clk_audss;

	/* TOP & PERI */
	struct clk *sclk_epll;
	struct clk *sclk_top_pll;
	struct clk *sclk_peri;

	/* Clock for generating I2S signals */
	struct clk *op_clk;
	/* Array of clock names for op_clk */
	const char **src_clk;

	/* Driver for this DAI */
	struct snd_soc_dai_driver i2s_dai_drv;

	u32	quirks;
	u32	opencnt;
	u32	clk_init;

	u32	suspend_i2smod;
	u32	suspend_i2scon;
	u32	suspend_i2spsr;
	u32	suspend_i2sahb[((I2SSTR1 - I2SAHB) >> 2) + 1];

	u32	suspend_audss_clksrc_sel;
	u32	suspend_audss_clkdiv0;
	u32	suspend_audss_clkdiv1;
	u32	suspend_audss_clksrc_clkgate;
	u32	suspend_audss_aclk_clkgate;
	u32	suspend_audss_pclk_clkgate;
	u32	suspend_audss_sclk_clkgate;

};

void audss_enable(void);
void audss_disable(void);
void audss_sw_reset(void);

extern struct clk *audss_get_i2s_opclk(int clk_id);

#ifdef CONFIG_SOC_EXYNOS5260
#define PA_AUDSS EXYNOS5260_PA_AUDSS;
#else
#define PA_AUDSS EXYNOS_PA_AUDSS;
#endif

#endif

