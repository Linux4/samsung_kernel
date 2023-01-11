/*
 * mmp map Audio clock operation source file
 *
 * Copyright (C) 2014 Marvell
 * Zhao Ye <zhaoy@marvell.com>
 * Nenghua Cao <nhcao@marvell.com>
 *
 * This file is licensed under the terms of the GNU General Public
 * License version 2. This program is licensed "as is" without any
 * warranty of any kind, whether express or implied.
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/mfd/mmp-map.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <dt-bindings/clock/marvell-audio-map.h>

#include "clk.h"
#include "clk-audio-map.h"

#define to_clk_audio(clk) (container_of(clk, struct clk_audio, hw))

#define AUD_NO_RESET_CTRL (1)

static DEFINE_SPINLOCK(clk_lock);

struct popular_reference_clock_freq refclock_map[] = {
/* refclk refdiv update fbdiv  out      in   bit_0_14 bit_15 bit_hex *
 * -----  ----  -----   --    ------  --------   ---   ---   ---    */
{ 11289600, 2, 5644800, 24, 135475200, 135475200,    0, 0,    0x0 },
{ 11289600, 2, 5644800, 26, 146764800, 147456000, 2469, 0, 0x09A5 },
{ 12288000, 2, 6144000, 22, 135168000, 135475200, 1192, 0, 0x04A8 },
{ 12288000, 2, 6144000, 24, 147456000, 147456000,    0, 1,    0x0 },
{ 13000000, 3, 4333333, 31, 134333333, 135475200, 4457, 0, 0x1169 },
{ 13000000, 3, 4333333, 34, 147333333, 147456000,  437, 0, 0x01B5 },
{ 16934400, 3, 5644800, 24, 135475200, 135475200,    0, 0,    0x0 },
{ 16934400, 3, 5644800, 26, 146764800, 147456000, 2469, 0, 0x09A5 },
{ 18432000, 3, 6144000, 22, 135168000, 135475200, 1192, 0, 0x04A8 },
{ 18432000, 3, 6144000, 24, 147456000, 147456000,    0, 0,    0x0 },
{ 22579200, 4, 5644800, 24, 135475200, 135475200,    0, 0,    0x0 },
{ 22579200, 4, 5644800, 26, 146764800, 147456000, 2469, 0, 0x09A5 },
{ 24576000, 4, 6144000, 22, 135168000, 135475200, 1192, 0, 0x04A8 },
{ 24576000, 4, 6144000, 24, 147456000, 147456000,    0, 0,    0x0 },
{ 26000000, 6, 4333333, 31, 134333333, 135475200, 4457, 0, 0x1169 },
{ 26000000, 6, 4333333, 34, 147333333, 147456000,  437, 0, 0x01B5 },
{ 38400000, 6, 6400000, 21, 134400000, 135475200, 4194, 0, 0x1062 },
{ 38400000, 6, 6400000, 23, 147200000, 147456000,  912, 0, 0x0390 },
};

struct fvco_clk_with_32kref {
	uint32_t fbdiv_int;
	uint32_t fbdiv_dec;
	uint32_t fvco;
};

/* audio pll1 clock table */
struct fvco_clk_with_32kref fvco_clock_with_32kref_map[] = {
	/* fbdiv_int	fbdiv_dec	fvco*
	 * --------	-----		------*/
	{137,	13,		451584000},
	{165,	6,		541900800},
	{180,	0,		589824000},
};

struct map_clk_apll1_table {
	unsigned long input_rate;
	unsigned long output_rate;
	uint32_t fbdiv_int;
	uint32_t fbdiv_dec;
	uint32_t fvco;
	unsigned long vco_div;
};

/* power on/off func of ulcx seires: ulc1 etc */
static void ulcx_power_on(void __iomem *apmu_base, int pwr_on)
{
	u32 val;

	if (pwr_on) {
		/* clock enable: by deafult choose vctcxo */
		val = readl_relaxed(apmu_base + APMU_AUD_CLK);
		val |= 0xd;
		writel_relaxed(val, apmu_base + APMU_AUD_CLK);
	} else {
		/* clock disable */
		val = readl_relaxed(apmu_base + APMU_AUD_CLK);
		val &= ~0xd;
		writel_relaxed(val, apmu_base + APMU_AUD_CLK);
	}
}

/* power on/off func of helan2 seires: helan2/helan3 */
static void helanx_power_on(void __iomem *apmu_base, int pwr_on)
{
	u32 val;

	if (pwr_on) {
		/* clock enable: by deafult choose vctcxo */
		val = readl_relaxed(apmu_base + APMU_AUD_CLK);
		val |= 0xf;
		writel_relaxed(val, apmu_base + APMU_AUD_CLK);
	} else {
		/* clock disable */
		val = readl_relaxed(apmu_base + APMU_AUD_CLK);
		val &= ~0xf;
		writel_relaxed(val, apmu_base + APMU_AUD_CLK);
	}
}

#define PMUA_AUDIO_PWRCTRL		(0x010c)
#define PMUA_SRAM_PWRCTRL		(0x0240)
#define PMUA_ISLD_AUDIO_CTRL		(0x01A8)
/* power on/off func of eden series: eden etc */
static void edenx_power_on(void __iomem *apmu_base, int pwr_on)
{
	unsigned int pmua_audio_pwrctrl, pmua_audio_clk_res_ctrl;
	unsigned int pmua_isld_audio_ctrl, pmua_sram_pwrctrl;

	if (pwr_on) {
		/* Audio island power on */
		pmua_audio_pwrctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_PWRCTRL);
		if ((pmua_audio_pwrctrl & (0x3 << 9)) == 3) {
			pr_info("%s audio island already power on\n", __func__);
			return;
		}
		pmua_sram_pwrctrl = readl_relaxed(apmu_base +
			PMUA_SRAM_PWRCTRL);
		pmua_sram_pwrctrl |= (1 << 2);
		writel_relaxed(pmua_sram_pwrctrl,
			apmu_base + PMUA_SRAM_PWRCTRL);
		pmua_sram_pwrctrl = readl_relaxed(apmu_base +
			PMUA_SRAM_PWRCTRL);
		pmua_sram_pwrctrl |= (1 << 0);
		writel_relaxed(pmua_sram_pwrctrl,
			apmu_base + PMUA_SRAM_PWRCTRL);
		pmua_audio_pwrctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_PWRCTRL);
		pmua_audio_pwrctrl |= (1 << 9);
		writel_relaxed(pmua_audio_pwrctrl,
			apmu_base + PMUA_AUDIO_PWRCTRL);
		udelay(10);
		pmua_sram_pwrctrl = readl_relaxed(apmu_base +
			PMUA_SRAM_PWRCTRL);
		pmua_sram_pwrctrl |= (1 << 3);
		writel_relaxed(pmua_sram_pwrctrl,
			apmu_base + PMUA_SRAM_PWRCTRL);
		pmua_sram_pwrctrl = readl_relaxed(apmu_base +
			PMUA_SRAM_PWRCTRL);
		pmua_sram_pwrctrl |= (1 << 1);
		writel_relaxed(pmua_sram_pwrctrl,
			apmu_base + PMUA_SRAM_PWRCTRL);
		pmua_audio_pwrctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_PWRCTRL);
		pmua_audio_pwrctrl |= (1 << 10);
		writel_relaxed(pmua_audio_pwrctrl,
			apmu_base + PMUA_AUDIO_PWRCTRL);
		udelay(10);

		/* disable sram FW */
		pmua_isld_audio_ctrl = readl_relaxed(apmu_base +
			PMUA_ISLD_AUDIO_CTRL);
		pmua_isld_audio_ctrl &= ~(3 << 1);
		writel_relaxed(pmua_isld_audio_ctrl, apmu_base +
			PMUA_ISLD_AUDIO_CTRL);
		pmua_isld_audio_ctrl = readl_relaxed(apmu_base +
			PMUA_ISLD_AUDIO_CTRL);
		pmua_isld_audio_ctrl &= ~(1 << 0);
		writel_relaxed(pmua_isld_audio_ctrl, apmu_base +
			PMUA_ISLD_AUDIO_CTRL);

		/* Enable Dummy clocks to srams*/
		pmua_isld_audio_ctrl = readl_relaxed(apmu_base +
			PMUA_ISLD_AUDIO_CTRL);
		pmua_isld_audio_ctrl |= (1 << 4);
		writel_relaxed(pmua_isld_audio_ctrl, apmu_base +
			PMUA_ISLD_AUDIO_CTRL);
		udelay(1);

		/* disbale dummy clock to srams*/
		pmua_isld_audio_ctrl = readl_relaxed(apmu_base +
			PMUA_ISLD_AUDIO_CTRL);
		pmua_isld_audio_ctrl &= ~(1 << 4);
		writel_relaxed(pmua_isld_audio_ctrl, apmu_base +
			PMUA_ISLD_AUDIO_CTRL);

		/*
		 * program audio sram & apb clocks
		 * to source from 26MHz vctcxo
		 */
		pmua_audio_clk_res_ctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_CLK_RES_CTRL);
		pmua_audio_clk_res_ctrl |= (1 << 3);
		writel_relaxed(pmua_audio_clk_res_ctrl, apmu_base +
			PMUA_AUDIO_CLK_RES_CTRL);
		pmua_audio_clk_res_ctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_CLK_RES_CTRL);
		pmua_audio_clk_res_ctrl |= (1 << 1);
		writel_relaxed(pmua_audio_clk_res_ctrl, apmu_base +
			PMUA_AUDIO_CLK_RES_CTRL);

		/* disable island fw*/
		pmua_audio_pwrctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_PWRCTRL);
		pmua_audio_pwrctrl |= (1 << 8);
		writel_relaxed(pmua_audio_pwrctrl,
			apmu_base + PMUA_AUDIO_PWRCTRL);

		/* start sram repairt */
		pmua_audio_pwrctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_PWRCTRL);
		pmua_audio_pwrctrl |= (1 << 2);
		writel_relaxed(pmua_audio_pwrctrl,
			apmu_base + PMUA_AUDIO_PWRCTRL);

		/* wait for sram repair completion*/
		pmua_audio_pwrctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_PWRCTRL);
		while (pmua_audio_pwrctrl & (1 << 2)) {
			pmua_audio_pwrctrl = readl_relaxed(apmu_base +
				PMUA_AUDIO_PWRCTRL);
		}

		/* release sram and apb reset*/
		pmua_audio_clk_res_ctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_CLK_RES_CTRL);
		pmua_audio_clk_res_ctrl |= (1 << 2);
		writel_relaxed(pmua_audio_clk_res_ctrl, apmu_base +
			PMUA_AUDIO_CLK_RES_CTRL);
		pmua_audio_clk_res_ctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_CLK_RES_CTRL);
		pmua_audio_clk_res_ctrl |= (1 << 0);
		writel_relaxed(pmua_audio_clk_res_ctrl, apmu_base +
			PMUA_AUDIO_CLK_RES_CTRL);
		udelay(1);
	} else {
		/* Audio island power down */
		pmua_audio_pwrctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_PWRCTRL);
		/* Check if audio already power off, if yes then return */
		if ((pmua_audio_pwrctrl & ((0x3 << 9) | (0x1 << 8))) == 0x0) {
			/* Audio is powered off, return */
			return;
		}

		/* Enable Dummy clocks to srams*/
		pmua_isld_audio_ctrl = readl_relaxed(apmu_base +
			PMUA_ISLD_AUDIO_CTRL);
		pmua_isld_audio_ctrl |= (1 << 4);
		writel_relaxed(pmua_isld_audio_ctrl, apmu_base +
			PMUA_ISLD_AUDIO_CTRL);
		udelay(1);

		/* disbale dummy clock to srams*/
		pmua_isld_audio_ctrl = readl_relaxed(apmu_base +
			PMUA_ISLD_AUDIO_CTRL);
		pmua_isld_audio_ctrl &= ~(1 << 4);
		writel_relaxed(pmua_isld_audio_ctrl, apmu_base +
			PMUA_ISLD_AUDIO_CTRL);

		/* enable sram fw */
		pmua_isld_audio_ctrl = readl_relaxed(apmu_base +
			PMUA_ISLD_AUDIO_CTRL);
		pmua_isld_audio_ctrl &= ~(3 << 1);
		writel_relaxed(pmua_isld_audio_ctrl, apmu_base +
			PMUA_ISLD_AUDIO_CTRL);

		pmua_isld_audio_ctrl = readl_relaxed(apmu_base +
			PMUA_ISLD_AUDIO_CTRL);
		pmua_isld_audio_ctrl |= 1 << 0;
		writel_relaxed(pmua_isld_audio_ctrl, apmu_base +
			PMUA_ISLD_AUDIO_CTRL);

		/* enable audio fw */
		pmua_audio_pwrctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_PWRCTRL);
		pmua_audio_pwrctrl &= ~(1 << 8);
		writel_relaxed(pmua_audio_pwrctrl,
			apmu_base + PMUA_AUDIO_PWRCTRL);

		/* assert audio sram and apb reset */
		pmua_audio_clk_res_ctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_CLK_RES_CTRL);
		pmua_audio_clk_res_ctrl &= ~(1 << 2);
		writel_relaxed(pmua_audio_clk_res_ctrl, apmu_base +
			PMUA_AUDIO_CLK_RES_CTRL);
		pmua_audio_clk_res_ctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_CLK_RES_CTRL);
		pmua_audio_clk_res_ctrl &= ~(1 << 0);
		writel_relaxed(pmua_audio_clk_res_ctrl, apmu_base +
			PMUA_AUDIO_CLK_RES_CTRL);

		/* power off the island */
		pmua_audio_pwrctrl = readl_relaxed(apmu_base +
			PMUA_AUDIO_PWRCTRL);
		pmua_audio_pwrctrl &= ~(3 << 9);
		writel_relaxed(pmua_audio_pwrctrl,
			apmu_base + PMUA_AUDIO_PWRCTRL);

		/* if needed, power off the audio buffer sram */
		pmua_sram_pwrctrl = readl_relaxed(apmu_base +
			PMUA_SRAM_PWRCTRL);
		pmua_sram_pwrctrl &= ~(3 << 2);
		writel_relaxed(pmua_sram_pwrctrl,
			apmu_base + PMUA_SRAM_PWRCTRL);
		/*
		 * Fixme: if needed, power off the audio map code sram
		 * but, we keep it here due to we don't want to
		 * load map firmware again */
		pmua_sram_pwrctrl = readl_relaxed(apmu_base +
			PMUA_SRAM_PWRCTRL);
		pmua_sram_pwrctrl &= ~(3 << 0);
		writel_relaxed(pmua_sram_pwrctrl,
			apmu_base + PMUA_SRAM_PWRCTRL);
	}
}

static int map_32k_apll_enable(void *dspaux_base, u32 srate)
{
	void __iomem *reg_addr;
	u32 refdiv, post_div, vco_div, fbdec_div, fbdec_int, vco_en, div_en;
	u32 ICP, ICP_DLL, CTUNE, TEST_MON, KVCO, FD_SEL, VDDL;
	u32 val, time_out = 2000;
	unsigned long fvco;

	if (dspaux_base == NULL) {
		pr_err("wrong audio aux base\n");
		return -EINVAL;
	}

	/* below value are fixed */
	KVCO = 1;
	ICP = 2;
	FD_SEL = 1;
	CTUNE = 1;
	ICP_DLL = 1;
	VDDL = 1;
	TEST_MON = 0;
	/* 32K crysto input */
	refdiv = 1;
	vco_en = 1;
	div_en = 1;

	if ((srate % 8000) == 0) {
		/* 8k famliy */
		fbdec_div = 0;
		fbdec_int = 0xb4;
		/* over-sample rate = 192 */
		post_div = 0x18;
		vco_div = 4;
		fvco = 589824000 / vco_div;
	} else if ((srate % 11025) == 0) {
		/* 8k famliy */
		fbdec_div = 6;
		fbdec_int = 0xa5;
		/* over-sample rate = 192 */
		post_div = 0x18;
		vco_div = 4;
		fvco = 541900800 / vco_div;
	} else {
		pr_err("error: no pll setting for such clock!\n");
		return -EINVAL;
	}

	/*
	 * 1: Assert reset for the APLL and PU: apll1_config1
	 */
	reg_addr = dspaux_base + DSP_AUDIO_PLL1_CONF_1;
	val = readl_relaxed(reg_addr);
	/* set power up, and also set reset */
	val |= 3;
	writel_relaxed(val, reg_addr);
	val = readl_relaxed(reg_addr);

	/*
	 * 2: set ICP, REV_DIV, FBDIV_INT, FBDIV_DEC, ICP_PLL, KVCO
	 */
	reg_addr = dspaux_base + DSP_AUDIO_PLL1_CONF_1;
	val = readl_relaxed(reg_addr);
	/* clear bits: [31-2] */
	val &= 3;
	val |=
	    ((KVCO << 31) | (ICP << 27) | (fbdec_div << 23) | (fbdec_int << 15)
	     | (refdiv << 6) | (FD_SEL << 4) | (CTUNE << 2));
	writel_relaxed(val, reg_addr);
	val = readl_relaxed(reg_addr);

	/*
	 * 3: Set the required POSTDIV_AUDIO value
	 * POSTDIV_AUDIO = 0x93(147) for 48KHz, over-sample rate 64
	 */
	/* 3.1: config apll1 fast clock: VCO_DIV = 1 */
	reg_addr = dspaux_base + DSP_AUDIO_PLL1_CONF_4;
	val = readl_relaxed(reg_addr);
	val &= ~(0xfff);
	val |= vco_div;
	writel_relaxed(val, reg_addr);
	val = readl_relaxed(reg_addr);

	/* 3.2: config apll1 slow clock */
	reg_addr = dspaux_base + DSP_AUDIO_PLL1_CONF_2;
	val = readl_relaxed(reg_addr);
	val &= (0xf << 28);
	val |=
	    ((TEST_MON << 24) | (vco_en << 23) | (post_div << 11) |
	     (div_en << 10) | (ICP_DLL << 5) | (0x1 << 4) | VDDL);
	writel_relaxed(val, reg_addr);
	val = readl_relaxed(reg_addr);

	/*
	 * 4: de-assert reset
	 */
	reg_addr = dspaux_base + DSP_AUDIO_PLL1_CONF_1;
	val = readl_relaxed(reg_addr);
	/* release reset */
	val &= ~(0x1 << 1);
	writel_relaxed(val, reg_addr);
	val = readl_relaxed(reg_addr);

	/*
	 * 5: check DLL lock status
	 */
	reg_addr = dspaux_base + DSP_AUDIO_PLL1_CONF_2;
	val = readl_relaxed(reg_addr);
	while ((!(val & (0x1 << 28))) && time_out) {
		udelay(10);
		val = readl_relaxed(reg_addr);
		time_out--;
	}
	if (time_out == 0) {
		pr_err("32K-PLL: DLL lock fail!\n");
		return -EBUSY;
	}

	/*
	 * 6: check PLL lock status
	 */
	/* somehow we need a large delay on bringup stage */
	time_out = 2000 * 8;
	while ((!(val & (0x1 << 29))) && time_out) {
		udelay(10);
		val = readl_relaxed(reg_addr);
		time_out--;
	}

	if (time_out == 0) {
		pr_err("32K-PLL: PLL lock fail!\n");
		return -EBUSY;
	}

	return 0;
}

static int map_32k_apll_disable(void *dspaux_base)
{
	void __iomem *reg_addr;
	u32 val;

	reg_addr = dspaux_base + DSP_AUDIO_PLL1_CONF_1;
	val = readl_relaxed(reg_addr);
	/* reset & power off */
	val &= ~0x1;
	val |= (0x1 << 1);
	writel_relaxed(val, reg_addr);

	return 0;
}

static int map_26m_apll_enable(void *dspaux_base, u32 srate)
{
	void __iomem *reg_addr;
	u32 refdiv, post_div, vco_div, fbdiv, freq_off;
	u32 vco_en, vco_div_en, post_div_en, val;
	u32 ICP, CTUNE, TEST_MON, FD_SEL, CLK_DET_EN, INTPI, PI_EN;
	u32 time_out = 2000;
	unsigned long fvco;

	if (dspaux_base == NULL) {
		pr_err("wrong audio aux base\n");
		return -EINVAL;
	}

	/* below value are fixed */
	ICP = 6;
	FD_SEL = 1;
	CTUNE = 1;
	TEST_MON = 0;
	INTPI = 2;
	CLK_DET_EN = 1;
	PI_EN = 1;
	/* 26M clock input */
	refdiv = 6;
	vco_en = 1;
	vco_div_en = 1;
	post_div_en = 1;

	if ((srate % 8000) == 0) {
		/* 8k famliy */
		fbdiv = 34;
		freq_off = 0x1b5;
		/* over-sample rate = 192 */
		post_div = 0x6;
		vco_div = 4;
		fvco = 589824000 / vco_div;
	} else if ((srate % 11025) == 0) {
		/* 8k famliy */
		fbdiv = 31;
		freq_off = 0x1169;
		/* over-sample rate = 192 */
		post_div = 0x6;
		vco_div = 4;
		fvco = 541900800 / vco_div;
	} else {
		pr_err("error: no pll setting for such clock!\n");
		return -EINVAL;
	}

	/*
	 * 1: power up and reset pll
	 */
	reg_addr = dspaux_base + DSP_AUDIO_PLL2_CONF_1;
	val = readl_relaxed(reg_addr);
	/* set power up, and also set reset */
	val |= 3;
	writel_relaxed(val, reg_addr);
	val = readl_relaxed(reg_addr);

	/*
	 * 2: set ICP, REV_DIV, FBDIV_IN, FBDIV_DEC, ICP_PLL, KVCO
	 */
	reg_addr = dspaux_base + DSP_AUDIO_PLL2_CONF_1;
	val = readl_relaxed(reg_addr);
	val &= 3;
	val |=
	    ((ICP << 27) | (fbdiv << 18) | (refdiv << 9) | (CLK_DET_EN << 8) |
	     (INTPI << 6) | (FD_SEL << 4) | (CTUNE << 2));
	writel_relaxed(val, reg_addr);
	val = readl_relaxed(reg_addr);

	/*
	 * 3: enable clk_vco
	 */
	reg_addr = dspaux_base + DSP_AUDIO_PLL2_CONF_3;
	val = readl_relaxed(reg_addr);
	val &= ~(0x7ff << 14);
	val |=
	    ((vco_div_en << 24) | (vco_div << 15) | (vco_en << 14) |
	     (TEST_MON << 0));
	writel_relaxed(val, reg_addr);
	val = readl_relaxed(reg_addr);

	/*
	 * 4: enable clk_audio
	 */
	reg_addr = dspaux_base + DSP_AUDIO_PLL2_CONF_2;
	val = readl_relaxed(reg_addr);
	val &= ~((0x7fffff << 4) | 0xf);
	val |=
	    ((post_div << 20) | (freq_off << 4) | (post_div_en << 0x1) |
	     (PI_EN << 0));
	writel_relaxed(val, reg_addr);
	val = readl_relaxed(reg_addr);

	/*
	 * 5: de-assert reset
	 */
	reg_addr = dspaux_base + DSP_AUDIO_PLL2_CONF_1;
	val = readl_relaxed(reg_addr);
	/* release reset */
	val &= ~(0x1 << 1);
	writel_relaxed(val, reg_addr);
	val = readl_relaxed(reg_addr);

	/*
	 * 6: apply freq_offset_valid: wait 50us according to DE
	 */
	udelay(50);
	reg_addr = dspaux_base + DSP_AUDIO_PLL2_CONF_2;
	val = readl_relaxed(reg_addr);
	val |= (1 << 2);
	writel_relaxed(val, reg_addr);
	val = readl_relaxed(reg_addr);

	/*
	 * 7: check PLL lock status
	 */
	reg_addr = dspaux_base + DSP_AUDIO_PLL2_CONF_1;
	val = readl_relaxed(reg_addr);
	while (!(val & (0x1 << 31)) && time_out) {
		udelay(10);
		val = readl_relaxed(reg_addr);
		time_out--;
	}
	if (time_out == 0) {
		pr_err("26M-PLL: PLL lock fail!\n");
		return -EBUSY;
	}

	return 0;
}

static int map_26m_apll_disable(void *dspaux_base)
{
	void __iomem *reg_addr;
	u32 val;

	reg_addr = dspaux_base + DSP_AUDIO_PLL2_CONF_1;
	val = readl_relaxed(reg_addr);
	/* reset & power off */
	val &= ~0x1;
	val |= (0x1 << 1);
	writel_relaxed(val, reg_addr);

	return 0;
}

static int clk_apll_enable(struct clk_hw *hw)
{
	struct clk_audio *audio = to_clk_audio(hw);

	/* enable audio power domain */
	audio->poweron(audio->apmu_base, 1);

	/* enable 32K-apll */
	audio->apll_enable(audio->dspaux_base, 48000);

	return 0;
}

static void clk_apll_disable(struct clk_hw *hw)
{
	struct clk_audio *audio = to_clk_audio(hw);

	/* enable 32K-apll */
	audio->apll_disable(audio->dspaux_base);

	/* if audio pll is closed, we can close the audio island */
	audio->poweron(audio->apmu_base, 0);
}

static long __map_apll1_get_rate_table(struct clk_hw *hw,
		struct map_clk_apll1_table *freq_tbl,
		unsigned long drate, unsigned long prate)
{
	int i;
	struct fvco_clk_with_32kref *ref_p;

	for (i = 0; i < ARRAY_SIZE(fvco_clock_with_32kref_map); i++) {

		/* find audio pll setting */
		ref_p = &fvco_clock_with_32kref_map[i];

		if ((ref_p->fvco % drate) == 0)
			goto found;
	}

	return -EINVAL;
found:
	freq_tbl->input_rate = prate;
	freq_tbl->fbdiv_int = ref_p->fbdiv_int;
	freq_tbl->fbdiv_dec = ref_p->fbdiv_dec;
	freq_tbl->output_rate = drate;
	freq_tbl->vco_div = ref_p->fvco / drate;

	return drate;
}

static long clk_apll1_round_rate(struct clk_hw *hw, unsigned long drate,
		unsigned long *prate)
{
	struct map_clk_apll1_table freq_tbl;

	memset(&freq_tbl, 0, sizeof(freq_tbl));
	if (__map_apll1_get_rate_table(hw, &freq_tbl, drate, *prate) < 0)
		return -EINVAL;

	*prate = freq_tbl.input_rate;

	return freq_tbl.output_rate;
}

static unsigned long clk_apll1_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	return hw->clk->rate;
}

/* Configures new clock rate */
static int clk_apll1_set_rate(struct clk_hw *hw, unsigned long drate,
				unsigned long prate)
{
	struct clk_audio *audio = to_clk_audio(hw);
	u32 val, vco_div;
	struct map_clk_apll1_table freq_tbl;
	unsigned long flags;

	memset(&freq_tbl, 0, sizeof(freq_tbl));

	spin_lock_irqsave(audio->lock, flags);

	if (__map_apll1_get_rate_table(hw, &freq_tbl, drate, prate) < 0) {
		spin_unlock_irqrestore(audio->lock, flags);
		return -EINVAL;
	}
	vco_div = freq_tbl.vco_div;

	val = readl_relaxed(audio->dspaux_base + DSP_AUDIO_PLL1_CONF_4);
	if ((val & 0xfff) != vco_div) {
		val &= ~(0xfff);
		val |= vco_div;
		writel_relaxed(val, audio->dspaux_base + DSP_AUDIO_PLL1_CONF_4);
	}

	hw->clk->rate = drate;

	spin_unlock_irqrestore(audio->lock, flags);

	return 0;
}

struct clk_ops clk_apll1_ops = {
	.prepare = clk_apll_enable,
	.unprepare = clk_apll_disable,
	.recalc_rate = clk_apll1_recalc_rate,
	.round_rate = clk_apll1_round_rate,
	.set_rate = clk_apll1_set_rate,
};

struct clk *mmp_clk_register_apll1(const char *name, const char *parent_name,
				   struct map_clk_unit *map_unit,
				   spinlock_t *lock)
{
	struct clk_audio *audio;
	struct clk *clk;
	struct clk_init_data init;

	audio = kzalloc(sizeof(*audio), GFP_KERNEL);
	if (!audio)
		return NULL;

	init.name = name;
	init.ops = &clk_apll1_ops;
	init.flags = 0;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	audio->apmu_base = map_unit->apmu_base;
	audio->map_base = map_unit->map_base;
	audio->dspaux_base = map_unit->dspaux_base;
	audio->apbsp_base = map_unit->apbsp_base;
	audio->poweron = map_unit->poweron_cb;
	audio->apll_enable = map_32k_apll_enable;
	audio->apll_disable = map_32k_apll_disable;
	audio->lock = lock;
	audio->hw.init = &init;

	clk = clk_register(NULL, &audio->hw);

	if (IS_ERR(clk))
		kfree(audio);

	return clk;
}

static long __map_apll2_get_rate_table(struct clk_hw *hw,
		struct map_clk_audio_pll_table *freq_tbl,
			unsigned long drate, unsigned long prate)
{
	int i;
	struct popular_reference_clock_freq *ref_p;
	unsigned long fvco;

	for (i = 0; i < ARRAY_SIZE(refclock_map); i++) {
		/* find audio pll setting */
		ref_p = &refclock_map[i];
		if (ref_p->refclk != prate)
			continue;

		/* max fvco clock is 4 * freq_intp_in */
		fvco = ref_p->freq_intp_in * 4;
		if ((fvco % drate) == 0)
			goto found;
	}

	return -EINVAL;
found:
	freq_tbl->input_rate = prate;
	freq_tbl->fbdiv = ref_p->fbdiv;
	freq_tbl->refdiv = ref_p->refdiv;
	freq_tbl->freq_offset = ref_p->freq_offset_0_14_hex;
	freq_tbl->output_rate = drate;
	freq_tbl->vco_div = (fvco / drate);

	return drate;
}

static long clk_apll2_round_rate(struct clk_hw *hw, unsigned long drate,
		unsigned long *prate)
{
	struct map_clk_audio_pll_table freq_tbl;

	memset(&freq_tbl, 0, sizeof(freq_tbl));
	if (__map_apll2_get_rate_table(hw, &freq_tbl, drate, *prate) < 0)
		return -EINVAL;

	*prate = freq_tbl.input_rate;

	return freq_tbl.output_rate;
}

static unsigned long clk_apll2_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	return hw->clk->rate;
}

/* Configures new clock rate */
static int clk_apll2_set_rate(struct clk_hw *hw, unsigned long drate,
				unsigned long prate)
{
	struct clk_audio *audio = to_clk_audio(hw);
	unsigned int val;
	struct map_clk_audio_pll_table freq_tbl;
	unsigned long vco_div, flags;

	memset(&freq_tbl, 0, sizeof(freq_tbl));
	spin_lock_irqsave(audio->lock, flags);

	if (__map_apll2_get_rate_table(hw, &freq_tbl, drate, prate) < 0) {
		spin_unlock_irqrestore(audio->lock, flags);
		return -EINVAL;
	}

	vco_div = freq_tbl.vco_div;

	val = readl_relaxed(audio->dspaux_base + DSP_AUDIO_PLL2_CONF_3);
	if (((val >> 15) & 0x1FF) != vco_div) {
		val &= ~(0x1FF << 15);
		val |= (vco_div << 15);
		writel_relaxed(val,
			       audio->dspaux_base + DSP_AUDIO_PLL2_CONF_3);
	}

	hw->clk->rate = drate;
	spin_unlock_irqrestore(audio->lock, flags);

	return 0;
}

struct clk_ops clk_apll2_ops = {
	.enable = clk_apll_enable,
	.disable = clk_apll_disable,
	.recalc_rate = clk_apll2_recalc_rate,
	.round_rate = clk_apll2_round_rate,
	.set_rate = clk_apll2_set_rate,
};

struct clk *mmp_clk_register_apll2(const char *name, const char *parent_name,
				   struct map_clk_unit *map_unit,
				   spinlock_t *lock)
{
	struct clk_audio *audio;
	struct clk *clk;
	struct clk_init_data init;

	audio = kzalloc(sizeof(*audio), GFP_KERNEL);
	if (!audio)
		return NULL;

	init.name = name;
	init.ops = &clk_apll2_ops;
	init.flags = 0;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	audio->apmu_base = map_unit->apmu_base;
	audio->map_base = map_unit->map_base;
	audio->dspaux_base = map_unit->dspaux_base;
	audio->apbsp_base = map_unit->apbsp_base;
	audio->poweron = map_unit->poweron_cb;
	audio->apll_enable = map_26m_apll_enable;
	audio->apll_disable = map_26m_apll_disable;
	audio->lock = lock;
	audio->hw.init = &init;

	clk = clk_register(NULL, &audio->hw);

	if (IS_ERR(clk))
		kfree(audio);

	return clk;
}

/* Common audio component reset/enable bit */

#define to_clk_audio_res(hw) container_of(hw, struct clk_audio_res, hw)
struct clk_audio_res {
	struct clk_hw hw;
	void __iomem *base;
	unsigned int en_bit_offset;
	unsigned int res_bit_offset;
	unsigned int delay;
	unsigned int flags;
	spinlock_t *lock;
};

static int clk_audio_prepare(struct clk_hw *hw)
{
	struct clk_audio_res *audio = to_clk_audio_res(hw);
	unsigned int data;
	unsigned long flags = 0;

	if (audio->lock)
		spin_lock_irqsave(audio->lock, flags);

	data = readl_relaxed(audio->base);
	data |= (1 << audio->en_bit_offset);
	writel_relaxed(data, audio->base);

	if (audio->lock)
		spin_unlock_irqrestore(audio->lock, flags);

	udelay(audio->delay);

	if (!(audio->flags & AUD_NO_RESET_CTRL)) {
		if (audio->lock)
			spin_lock_irqsave(audio->lock, flags);

		data = readl_relaxed(audio->base);
		data |= (1 << audio->res_bit_offset);
		writel_relaxed(data, audio->base);

		if (audio->lock)
			spin_unlock_irqrestore(audio->lock, flags);
	}

	return 0;
}

static void clk_audio_unprepare(struct clk_hw *hw)
{
	struct clk_audio_res *audio = to_clk_audio_res(hw);
	unsigned long data;
	unsigned long flags = 0;

	if (audio->lock)
		spin_lock_irqsave(audio->lock, flags);

	data = readl_relaxed(audio->base);
	data &= ~(1 << audio->en_bit_offset);
	writel_relaxed(data, audio->base);

	if (audio->lock)
		spin_unlock_irqrestore(audio->lock, flags);
}

struct clk_ops clk_audio_res_ops = {
	.prepare = clk_audio_prepare,
	.unprepare = clk_audio_unprepare,
};

struct clk *mmp_clk_register_aud_res(const char *name, const char *parent_name,
		void __iomem *base, unsigned int en_bit_offset,
		unsigned int res_bit_offset, unsigned int delay,
		unsigned int audio_res_flags, spinlock_t *lock)
{
	struct clk_audio_res *audio;
	struct clk *clk;
	struct clk_init_data init;

	audio = kzalloc(sizeof(*audio), GFP_KERNEL);
	if (!audio)
		return NULL;

	init.name = name;
	init.ops = &clk_audio_res_ops;
	init.flags = CLK_SET_RATE_PARENT;
	init.parent_names = (parent_name ? &parent_name : NULL);
	init.num_parents = (parent_name ? 1 : 0);

	audio->base = base;
	audio->en_bit_offset = en_bit_offset;
	audio->res_bit_offset = res_bit_offset;
	audio->delay = delay;
	audio->flags = audio_res_flags;
	audio->lock = lock;
	audio->hw.init = &init;

	clk = clk_register(NULL, &audio->hw);
	if (IS_ERR(clk))
		kfree(audio);

	return clk;
}

static char *apll_parent[] = {
	"map_apll1",
	"map_apll2",
};

static int clk_parse_map_dt(struct device_node *np, struct map_clk_unit *map_unit)
{
	struct device_node *np_map;
	u32 power_ctrl, apll = 0;
	int ret;

	map_unit->apmu_base = of_iomap(np, 0);
	if (!map_unit->apmu_base) {
		pr_err("failed to map apmu registers\n");
		return -EINVAL;
	}

	map_unit->apbsp_base = of_iomap(np, 1);
	if (!map_unit->apbsp_base) {
		pr_err("failed to map apbsp registers\n");
		return -EINVAL;
	}

	map_unit->map_base = of_iomap(np, 2);
	if (!map_unit->map_base) {
		pr_err("failed to map audio_map registers\n");
		return -EINVAL;
	}

	map_unit->dspaux_base = of_iomap(np, 3);
	if (!map_unit->dspaux_base) {
		pr_err("failed to map dspaux registers\n");
		return -EINVAL;
	}

	/*
	 * get apll settings in DT
	 * apll = 0: APLL_32K
	 * apll = 1: APLL_26M
	 */
	np_map = of_parse_phandle(np, "map", 0);
	if (!np_map)
		return -EINVAL;
	ret = of_property_read_u32(np_map, "marvell,apll", &apll);
	if (ret < 0) {
		/* using default apll = 0, APLL_32K */
		pr_err("could not get marvell,apll in dt in clock init\n");
		return ret;
	}

	ret = of_property_read_u32(np_map, "power_ctrl", &power_ctrl);
	if (ret < 0) {
		pr_err("could not get power_ctrl in dt in clock init\n");
		return ret;
	}

	if (power_ctrl == HELANX_POWER_CTRL)
		map_unit->poweron_cb = helanx_power_on;
	else if (power_ctrl == EDENX_POWER_CTRL)
		map_unit->poweron_cb = edenx_power_on;
	else if (power_ctrl == ULCX_POWER_CTRL)
		map_unit->poweron_cb = ulcx_power_on;
	else {
		pr_err("wrong power ctrl for audio map\n");
		return -EINVAL;
	}

	map_unit->apll = apll;
	map_unit->power_ctrl = power_ctrl;

	return 0;
}

void __init audio_clk_init(struct device_node *np)
{
	struct clk *clk;
	u32 apll, fvco = 589824000;
	char *clk_p;
	struct mmp_clk_unit *unit;
	struct map_clk_unit *map_unit;
	void __iomem *dspaux_base;
	int ret = 0;

	map_unit = kzalloc(sizeof(*map_unit), GFP_KERNEL);
	if (!map_unit) {
		pr_err("failed to allocate memory for audio map clock unit\n");
		return;
	}

	ret = clk_parse_map_dt(np, map_unit);
	if (ret < 0) {
		pr_err("%s: parse audio map dt failed!\n", __func__);
		return;
	}
	mmp_clk_init(np, &map_unit->unit, AUDIO_NR_CLKS);

	unit = &map_unit->unit;
	apll = map_unit->apll;

	if ((apll != APLL_32K) && (apll != APLL_26M)) {
		pr_err("%s: wrong audio pll settings!\n", __func__);
		return;
	}

	/* apll1 */
	clk = mmp_clk_register_apll1("map_apll1", "clk32", map_unit, &clk_lock);
	/* enable power for audio island */
	if (apll != APLL_32K && apll != APLL_26M) {
		pr_err("wrong apll source settings 0x%x\n", apll);
		return;
	}

	if (apll == APLL_32K) {
		clk_prepare_enable(clk);
		clk_set_rate(clk, fvco / 4);
		mmp_clk_add(unit, AUDIO_CLK_MAP, clk);
	}

	/* apll2 */
	clk =
	    mmp_clk_register_apll2("map_apll2", "vctcxo", map_unit, &clk_lock);
	/* enable power for audio island */
	if (apll == APLL_26M) {
		clk_prepare_enable(clk);
		clk_set_rate(clk, fvco / 4);
		mmp_clk_add(unit, AUDIO_CLK_MAP, clk);
	}

	clk_p = apll_parent[apll];

	/* sspa1 */
	dspaux_base = map_unit->dspaux_base;
	clk =
	    mmp_clk_register_aud_res("mmp-sspa-dai.0", clk_p, dspaux_base + 0xc,
				     3, 2, 10, 0, &clk_lock);
	mmp_clk_add(unit, AUDIO_CLK_SSPA0, clk);

	/* sspa2 */
	clk =
	    mmp_clk_register_aud_res("mmp-sspa-dai.1", clk_p,
				     dspaux_base + 0x10, 3, 2, 10, 0,
				     &clk_lock);
	mmp_clk_add(unit, AUDIO_CLK_SSPA1, clk);
}
CLK_OF_DECLARE(audio_map_clk, "marvell,audio-map-clock", audio_clk_init);
