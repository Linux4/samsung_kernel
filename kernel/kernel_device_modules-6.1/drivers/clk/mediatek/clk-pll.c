// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: James Liao <jamesjj.liao@mediatek.com>
 */

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/clkdev.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <mt-plat/aee.h>
#include "clk-mtk.h"

#define REG_CON0		0
#define REG_CON1		4

#define CON0_BASE_EN		BIT(0)
#define CON0_PWR_ON		BIT(0)
#define CON0_ISO_EN		BIT(1)
#define PCW_CHG_MASK		BIT(31)

#define AUDPLL_TUNER_EN		BIT(31)

#define POSTDIV_MASK		0x7

/* default 7 bits integer, can be overridden with pcwibits. */
#define INTEGER_BITS		7

#define MTK_WAIT_HWV_PLL_PREPARE_CNT	500
#define MTK_WAIT_HWV_PLL_PREPARE_US		1
#define MTK_WAIT_HWV_PLL_VOTE_CNT		100
#define MTK_WAIT_HWV_PLL_LONG_VOTE_CNT		2500
#define MTK_WAIT_HWV_PLL_VOTE_US		2
#define MTK_WAIT_HWV_PLL_DONE_CNT		100000
#define MTK_WAIT_HWV_PLL_DONE_US		1

#define MTK_WAIT_HWV_RES_PREPARE_CNT	500
#define MTK_WAIT_HWV_RES_PREPARE_US		1
#define MTK_WAIT_HWV_RES_VOTE_CNT		1000
#define MTK_WAIT_HWV_RES_VOTE_US		2
#define MTK_WAIT_HWV_RES_DONE_CNT		100000
#define MTK_WAIT_HWV_RES_DONE_US		1

#define PLL_EN_TYPE				0
#define PLL_RSTB_TYPE				1

#define PLL_MMINFRA_VOTE_BIT		26

static bool is_registered;

/*
 * MediaTek PLLs are configured through their pcw value. The pcw value describes
 * a divider in the PLL feedback loop which consists of 7 bits for the integer
 * part and the remaining bits (if present) for the fractional part. Also they
 * have a 3 bit power-of-two post divider.
 */

struct mtk_clk_pll {
	struct clk_hw	hw;
	void __iomem	*base_addr;
	void __iomem	*pd_addr;
	void __iomem	*pwr_addr;
	void __iomem	*tuner_addr;
	void __iomem	*tuner_en_addr;
	void __iomem	*pcw_addr;
	void __iomem	*pcw_chg_addr;
	void __iomem	*en_addr;
	void __iomem	*en_set_addr;
	void __iomem	*en_clr_addr;
	void __iomem	*rstb_addr;
	void __iomem	*rstb_set_addr;
	void __iomem	*rstb_clr_addr;
	const struct mtk_pll_data *data;
	struct regmap	*hwv_regmap;
	unsigned int	en_msk;
	unsigned int	rstb_msk;
	unsigned int	flags;
};

static bool (*mtk_fh_set_rate)(const char *name, unsigned long dds, int postdiv) = NULL;

int register_fh_set_rate(bool (*fh_set_rate)(const char *name, unsigned long dds, int postdiv)){

	if(!fh_set_rate) {
		pr_err("register_fh_set_rate fail\n");
		return -EINVAL;
        } else
		mtk_fh_set_rate = fh_set_rate;

	return 0;
}
EXPORT_SYMBOL_GPL(register_fh_set_rate);

static inline struct mtk_clk_pll *to_mtk_clk_pll(struct clk_hw *hw)
{
	return container_of(hw, struct mtk_clk_pll, hw);
}

static int mtk_pll_is_prepared(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	if (!is_registered)
		return 0;

	return (readl(pll->en_addr) & BIT(pll->data->pll_en_bit)) != 0;
}

static int mtk_pll_setclr_is_prepared(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	if (!is_registered)
		return 0;

	return (readl(pll->en_addr) & pll->en_msk) != 0;
}

static unsigned long __mtk_pll_recalc_rate(struct mtk_clk_pll *pll, u32 fin,
		u32 pcw, int postdiv)
{
	int pcwbits = pll->data->pcwbits;
	int pcwfbits = 0;
	int ibits;
	u64 vco;
	u8 c = 0;

	/* The fractional part of the PLL divider. */
	ibits = pll->data->pcwibits ? pll->data->pcwibits : INTEGER_BITS;
	if (pcwbits > ibits)
		pcwfbits = pcwbits - ibits;

	vco = (u64)fin * pcw;

	if (pcwfbits && (vco & GENMASK(pcwfbits - 1, 0)))
		c = 1;

	vco >>= pcwfbits;

	if (c)
		vco++;

	return ((unsigned long)vco + postdiv - 1) / postdiv;
}

static void __mtk_pll_tuner_enable(struct mtk_clk_pll *pll)
{
	u32 r;

	if (pll->tuner_en_addr) {
		r = readl(pll->tuner_en_addr) | BIT(pll->data->tuner_en_bit);
		writel(r, pll->tuner_en_addr);
	} else if (pll->tuner_addr) {
		r = readl(pll->tuner_addr) | AUDPLL_TUNER_EN;
		writel(r, pll->tuner_addr);
	}
}

static void __mtk_pll_tuner_disable(struct mtk_clk_pll *pll)
{
	u32 r;

	if (pll->tuner_en_addr) {
		r = readl(pll->tuner_en_addr) & ~BIT(pll->data->tuner_en_bit);
		writel(r, pll->tuner_en_addr);
	} else if (pll->tuner_addr) {
		r = readl(pll->tuner_addr) & ~AUDPLL_TUNER_EN;
		writel(r, pll->tuner_addr);
	}
}

static void mtk_pll_set_rate_regs(struct mtk_clk_pll *pll, u32 pcw,
		int postdiv)
{
	u32 chg, val;

	/* disable tuner */
	__mtk_pll_tuner_disable(pll);

	/* set postdiv */
	val = readl(pll->pd_addr);
	val &= ~(POSTDIV_MASK << pll->data->pd_shift);
	val |= (ffs(postdiv) - 1) << pll->data->pd_shift;

	/* postdiv and pcw need to set at the same time if on same register */
	if (pll->pd_addr != pll->pcw_addr) {
		writel(val, pll->pd_addr);
		val = readl(pll->pcw_addr);
	}

	/* set pcw */
	val &= ~GENMASK(pll->data->pcw_shift + pll->data->pcwbits - 1,
			pll->data->pcw_shift);
	val |= pcw << pll->data->pcw_shift;
	writel(val, pll->pcw_addr);
	chg = readl(pll->pcw_chg_addr) | PCW_CHG_MASK;
	writel(chg, pll->pcw_chg_addr);
	if (pll->tuner_addr)
		writel(val + 1, pll->tuner_addr);

	/* restore tuner_en */
	__mtk_pll_tuner_enable(pll);

	udelay(20);
}

/*
 * mtk_pll_calc_values - calculate good values for a given input frequency.
 * @pll:	The pll
 * @pcw:	The pcw value (output)
 * @postdiv:	The post divider (output)
 * @freq:	The desired target frequency
 * @fin:	The input frequency
 *
 */
static void mtk_pll_calc_values(struct mtk_clk_pll *pll, u32 *pcw, u32 *postdiv,
		u32 freq, u32 fin)
{
	unsigned long fmin = pll->data->fmin ? pll->data->fmin : (1000 * MHZ);
	const struct mtk_pll_div_table *div_table = pll->data->div_table;
	u64 _pcw;
	int ibits;
	u32 val;

	if (freq > pll->data->fmax)
		freq = pll->data->fmax;

	if (div_table) {
		if (freq > div_table[0].freq)
			freq = div_table[0].freq;

		for (val = 0; div_table[val + 1].freq != 0; val++) {
			if (freq > div_table[val + 1].freq)
				break;
		}
		*postdiv = 1 << val;
	} else {
		for (val = 0; val < 5; val++) {
			*postdiv = 1 << val;
			if ((u64)freq * *postdiv >= fmin)
				break;
		}
	}

	/* _pcw = freq * postdiv / fin * 2^pcwfbits */
	ibits = pll->data->pcwibits ? pll->data->pcwibits : INTEGER_BITS;
	_pcw = ((u64)freq << val) << (pll->data->pcwbits - ibits);
	do_div(_pcw, fin);

	*pcw = (u32)_pcw;
}

static int mtk_pll_set_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 pcw = 0;
	u32 postdiv;

	mtk_pll_calc_values(pll, &pcw, &postdiv, rate, parent_rate);
	if (!mtk_fh_set_rate || !mtk_fh_set_rate(pll->data->name, pcw, postdiv))
		mtk_pll_set_rate_regs(pll, pcw, postdiv);

	return 0;
}

static unsigned long mtk_pll_recalc_rate(struct clk_hw *hw,
		unsigned long parent_rate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 postdiv;
	u32 pcw;

	postdiv = (readl(pll->pd_addr) >> pll->data->pd_shift) & POSTDIV_MASK;
	postdiv = 1 << postdiv;

	pcw = readl(pll->pcw_addr) >> pll->data->pcw_shift;
	pcw &= GENMASK(pll->data->pcwbits - 1, 0);

	return __mtk_pll_recalc_rate(pll, parent_rate, pcw, postdiv);
}

static long mtk_pll_round_rate(struct clk_hw *hw, unsigned long rate,
		unsigned long *prate)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 pcw = 0;
	int postdiv;

	mtk_pll_calc_values(pll, &pcw, &postdiv, rate, *prate);

	return __mtk_pll_recalc_rate(pll, *prate, pcw, postdiv);
}

static int mtk_pll_prepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	r = readl(pll->pwr_addr) | CON0_PWR_ON;
	writel(r, pll->pwr_addr);
	udelay(1);

	r = readl(pll->pwr_addr) & ~CON0_ISO_EN;
	writel(r, pll->pwr_addr);
	udelay(1);

	if (pll->data->en_mask) {
		r = readl(pll->en_addr) | pll->data->en_mask;
		writel(r, pll->en_addr);
	}

	r = readl(pll->en_addr) | BIT(pll->data->pll_en_bit);
	writel(r, pll->en_addr);

	__mtk_pll_tuner_enable(pll);

	udelay(20);

	if (pll->data->flags & HAVE_RST_BAR) {
		r = readl(pll->base_addr + REG_CON0);
		r |= pll->data->rst_bar_mask;
		writel(r, pll->base_addr + REG_CON0);
	}

	return 0;
}

static void mtk_pll_unprepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 r;

	if (pll->data->flags & HAVE_RST_BAR) {
		r = readl(pll->base_addr + REG_CON0);
		r &= ~pll->data->rst_bar_mask;
		writel(r, pll->base_addr + REG_CON0);
	}

	__mtk_pll_tuner_disable(pll);

	r = readl(pll->en_addr) & ~BIT(pll->data->pll_en_bit);
	writel(r, pll->en_addr);

	if (pll->data->en_mask) {
		r = readl(pll->en_addr) & ~pll->data->en_mask;
		writel(r, pll->en_addr);
	}

	r = readl(pll->pwr_addr) | CON0_ISO_EN;
	writel(r, pll->pwr_addr);

	r = readl(pll->pwr_addr) & ~CON0_PWR_ON;
	writel(r, pll->pwr_addr);
}

static int mtk_hwv_pll_res_is_prepared_done(struct mtk_clk_pll *pll)
{
	u32 val = 0;

	regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);

	if (val & BIT(pll->data->hwv_shift)) {
		regmap_read(pll->hwv_regmap, pll->data->hwv_set_sta_ofs, &val);
		if ((val & BIT(pll->data->hwv_shift)) == 0x0)
			return 1;
	}

	return 0;
}

static int mtk_hwv_pll_res_is_unprepared_done(struct mtk_clk_pll *pll)
{
	u32 val = 0;

	regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);

	if ((val & BIT(pll->data->hwv_shift))) {
		regmap_read(pll->hwv_regmap, pll->data->hwv_clr_sta_ofs, &val);
		if ((val & BIT(pll->data->hwv_shift)) == 0x0)
			return 1;
	}

	return 0;
}

static int mtk_hwv_pll_res_prepare(struct mtk_clk_pll *pll)
{
	u32 val = 0, val2 = 0;
	int i = 0;

	if (pll->data->hwv_done_ofs) {
		/* wait for irq idle */
		do {
			regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);
			if ((val & BIT(pll->data->hwv_shift)) != 0)
				break;

			if (i < MTK_WAIT_HWV_RES_PREPARE_CNT)
				udelay(MTK_WAIT_HWV_RES_PREPARE_US);
			else
				goto err_res_prepare;
			i++;
		} while (1);

		i = 0;

		regmap_write(pll->hwv_regmap, pll->data->hwv_res_set_ofs,
				BIT(pll->data->hwv_shift));

		do {
			regmap_read(pll->hwv_regmap, pll->data->hwv_res_set_ofs, &val);
			if ((val & BIT(pll->data->hwv_shift)) != 0)
				break;

			udelay(MTK_WAIT_HWV_RES_VOTE_US);
			if (i > MTK_WAIT_HWV_RES_VOTE_CNT)
				goto err_res_vote;
			i++;
		} while (1);

		i = 0;

		do {
			if (mtk_hwv_pll_res_is_prepared_done(pll))
				break;

			if (i < MTK_WAIT_HWV_RES_DONE_CNT)
				udelay(MTK_WAIT_HWV_RES_DONE_US);
			else
				goto err_res_done;
			i++;
		} while (1);
	}

	return 0;
err_res_done:
	regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);
	regmap_read(pll->hwv_regmap, pll->data->hwv_set_sta_ofs, &val2);
	pr_err("%s res enable timeout(%dus)(%x %x)\n", pll->data->name,
			i * MTK_WAIT_HWV_RES_DONE_US, val, val2);
err_res_vote:
	pr_err("%s res vote timeout(%dus)(0x%x)\n", pll->data->name,
			i * MTK_WAIT_HWV_RES_VOTE_US, val);
err_res_prepare:
	pr_err("%s res prepare timeout(%dus)(0x%x)\n", pll->data->name,
			i * MTK_WAIT_HWV_RES_PREPARE_US, val);
	mtk_clk_notify(NULL, pll->hwv_regmap, NULL,
			pll->data->hwv_res_set_ofs, 0,
			pll->data->hwv_shift, CLK_EVT_HWV_PLL_TIMEOUT);

	return -EBUSY;
}

static void mtk_hwv_pll_res_unprepare(struct mtk_clk_pll *pll)
{
	u32 val = 0, val2 = 0;
	int i = 0;

	if (pll->data->hwv_done_ofs) {
		/* wait for irq idle */
		do {
			regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);
			if ((val & BIT(pll->data->hwv_shift)) != 0)
				break;

			if (i < MTK_WAIT_HWV_RES_PREPARE_CNT)
				udelay(MTK_WAIT_HWV_RES_PREPARE_US);
			else
				goto err_res_prepare;
			i++;
		} while (1);

		i = 0;

		regmap_write(pll->hwv_regmap, pll->data->hwv_res_clr_ofs,
				BIT(pll->data->hwv_shift));

		do {
			regmap_read(pll->hwv_regmap, pll->data->hwv_res_clr_ofs, &val);
			if ((val & BIT(pll->data->hwv_shift)) == 0)
				break;

			udelay(MTK_WAIT_HWV_RES_VOTE_US);
			if (i > MTK_WAIT_HWV_RES_VOTE_CNT)
				goto err_res_vote;
			i++;
		} while (1);

		i = 0;

		do {
			if (mtk_hwv_pll_res_is_unprepared_done(pll))
				break;

			if (i < MTK_WAIT_HWV_RES_DONE_CNT)
				udelay(MTK_WAIT_HWV_RES_DONE_US);
			else
				goto err_res_done;
			i++;
		} while (1);
	}

	return;
err_res_done:
	regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);
	regmap_read(pll->hwv_regmap, pll->data->hwv_clr_sta_ofs, &val2);
	pr_err("%s res disable timeout(%dus)(%x %x)\n", pll->data->name,
			i * MTK_WAIT_HWV_RES_DONE_US, val, val2);
err_res_vote:
	pr_err("%s res unvote timeout(%dus)(0x%x)\n", pll->data->name,
			i * MTK_WAIT_HWV_RES_PREPARE_US, val);
err_res_prepare:
	pr_err("%s res unprepare timeout(%dus)(0x%x)\n", pll->data->name,
			i * MTK_WAIT_HWV_RES_PREPARE_US, val);
	mtk_clk_notify(NULL, pll->hwv_regmap, NULL,
			pll->data->hwv_res_set_ofs, 0,
			pll->data->hwv_shift, CLK_EVT_HWV_PLL_TIMEOUT);
}

#define MAX_PLL_SETCLR_RSTB_RETRY 20
static int mtk_pll_setclr_prepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	int ret = 0, rstb_retries = 0;

	ret = mtk_hwv_pll_res_prepare(pll);
	if (ret)
		return ret;

	writel(pll->en_msk, pll->en_set_addr);

	__mtk_pll_tuner_enable(pll);

	udelay(20);

	if (pll->data->flags & HAVE_RST_BAR) {
		while ((!(readl(pll->rstb_addr) & pll->rstb_msk)) &&
				rstb_retries < MAX_PLL_SETCLR_RSTB_RETRY) {
			writel(pll->rstb_msk, pll->rstb_set_addr);
			rstb_retries += 1;
			udelay(1);
		}
		if (rstb_retries == MAX_PLL_SETCLR_RSTB_RETRY)
			pr_err("PLL RSTB SET fail, rstb: (0x%lx = 0x%x), msk: %x\n",
				(unsigned long)pll->rstb_addr, readl(pll->rstb_addr), pll->rstb_msk);
	}

	return 0;
}

static void mtk_pll_setclr_unprepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	int rstb_retries = 0;

	if (pll->data->flags & HAVE_RST_BAR) {
		while ((readl(pll->rstb_addr) & pll->rstb_msk) &&
				rstb_retries < MAX_PLL_SETCLR_RSTB_RETRY) {
			writel(pll->rstb_msk, pll->rstb_clr_addr);
			rstb_retries += 1;
			udelay(1);
		}
		if (rstb_retries == MAX_PLL_SETCLR_RSTB_RETRY)
			pr_err("PLL RSTB CLR fail, rstb: (0x%lx = 0x%x), msk: %x\n",
				(unsigned long)pll->rstb_addr, readl(pll->rstb_addr), pll->rstb_msk);
	}

	__mtk_pll_tuner_disable(pll);

	writel(pll->en_msk, pll->en_clr_addr);

	mtk_hwv_pll_res_unprepare(pll);
}

static int mtk_hwv_pll_is_prepared_done(struct mtk_clk_pll *pll)
{
	u32 val = 0, pll_sta;

	regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);

	if (val & BIT(pll->data->hwv_shift)) {
		if (pll->data->flags & HWV_CHK_FULL_STA) {
			regmap_read(pll->hwv_regmap, pll->data->hwv_set_sta_ofs, &val);
			pll_sta = readl(pll->en_addr) & BIT(pll->data->pll_en_bit);
			if (((val & BIT(pll->data->hwv_shift)) == 0x0)
					&& ((pll_sta & BIT(pll->data->pll_en_bit))))
				return 1;
		} else
			return 1;
	}

	return 0;
}

static int mtk_hwv_pll_is_unprepared_done(struct mtk_clk_pll *pll)
{
	u32 val = 0;

	regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);

	if (val & BIT(pll->data->hwv_shift)) {
		if (pll->data->flags & HWV_CHK_FULL_STA) {
			regmap_read(pll->hwv_regmap, pll->data->hwv_clr_sta_ofs, &val);
			if ((val & BIT(pll->data->hwv_shift)) == 0x0)
				return 1;
		} else
			return 1;
	}

	return 0;
}

static int mtk_hwv_pll_prepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 val = 0, val2 = 0;
	int i = 0;

	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl_optional(true, PLL_MMINFRA_VOTE_BIT);
	/* wait for irq idle */
	do {
		regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);
		if ((val & BIT(pll->data->hwv_shift)) != 0)
			break;

		if (i < MTK_WAIT_HWV_PLL_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PLL_PREPARE_US);
		else
			goto err_hwv_prepare;
		i++;
	} while (1);

	i = 0;

	/* dummy read to clr idle signal of hw voter bus */
	regmap_read(pll->hwv_regmap, pll->data->hwv_set_ofs, &val);
	regmap_write(pll->hwv_regmap, pll->data->hwv_set_ofs, BIT(pll->data->hwv_shift));

	do {
		regmap_read(pll->hwv_regmap, pll->data->hwv_set_ofs, &val);
		if ((val & BIT(pll->data->hwv_shift)) != 0)
			break;

		udelay(MTK_WAIT_HWV_PLL_VOTE_US);
		if (i > MTK_WAIT_HWV_PLL_VOTE_CNT)
			goto err_hwv_vote;
		i++;
	} while (1);

	i = 0;

	do {
		if (mtk_hwv_pll_is_prepared_done(pll))
			break;

		if (i < MTK_WAIT_HWV_PLL_DONE_CNT)
			udelay(MTK_WAIT_HWV_PLL_DONE_US);
		else
			goto err_hwv_done;
		i++;
	} while (1);

	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl_optional(false, PLL_MMINFRA_VOTE_BIT);

	return 0;

err_hwv_done:
	regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);
	regmap_read(pll->hwv_regmap, pll->data->hwv_set_sta_ofs, &val2);
	pr_err("%s pll enable timeout(%dus)(%x %x)\n", pll->data->name,
			i * MTK_WAIT_HWV_PLL_DONE_US, val, val2);
err_hwv_vote:
	pr_err("%s pll vote timeout(%dus)(0x%x)\n", pll->data->name,
			i * MTK_WAIT_HWV_PLL_VOTE_US, val);
err_hwv_prepare:
	pr_err("%s pll prepare timeout(%dus)(0x%x)\n", pll->data->name,
			i * MTK_WAIT_HWV_PLL_PREPARE_US, val);
	mtk_clk_notify(NULL, pll->hwv_regmap, NULL,
			pll->data->hwv_set_ofs, 0,
			pll->data->hwv_shift, CLK_EVT_HWV_PLL_TIMEOUT);
	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl_optional(false, PLL_MMINFRA_VOTE_BIT);

	return -EBUSY;
}

static void mtk_hwv_pll_unprepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 val = 0, val2 = 0;
	int i = 0;

	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl_optional(true, PLL_MMINFRA_VOTE_BIT);
	/* wait for irq idle */
	do {
		regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);
		if ((val & BIT(pll->data->hwv_shift)) != 0)
			break;

		if (i < MTK_WAIT_HWV_PLL_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PLL_PREPARE_US);
		else
			goto err_hwv_prepare;
		i++;
	} while (1);

	i = 0;

	/* dummy read to clr idle signal of hw voter bus */
	regmap_read(pll->hwv_regmap, pll->data->hwv_clr_ofs, &val);
	regmap_write(pll->hwv_regmap, pll->data->hwv_clr_ofs, BIT(pll->data->hwv_shift));

	do {
		regmap_read(pll->hwv_regmap, pll->data->hwv_clr_ofs, &val);
		if ((val & BIT(pll->data->hwv_shift)) == 0)
			break;

		udelay(MTK_WAIT_HWV_PLL_VOTE_US);
		if (i > MTK_WAIT_HWV_PLL_VOTE_CNT)
			goto err_hwv_vote;
		i++;
	} while (1);

	i = 0;

	/* delay 100us to prevent false ack check */
	udelay(100);
	do {
		if (mtk_hwv_pll_is_unprepared_done(pll))
			break;

		if (i < MTK_WAIT_HWV_PLL_DONE_CNT)
			udelay(MTK_WAIT_HWV_PLL_DONE_US);
		else
			goto err_hwv_done;
		i++;
	} while (1);

	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl_optional(false, PLL_MMINFRA_VOTE_BIT);

	return;

err_hwv_done:
	regmap_read(pll->hwv_regmap, pll->data->hwv_done_ofs, &val);
	regmap_read(pll->hwv_regmap, pll->data->hwv_clr_sta_ofs, &val2);
	pr_err("%s pll disable timeout(%dus)(%x %x)\n", pll->data->name,
			i * MTK_WAIT_HWV_PLL_DONE_US, val, val2);
err_hwv_vote:
	pr_err("%s pll unvote timeout(%dus)(0x%x)\n", pll->data->name,
			i * MTK_WAIT_HWV_PLL_PREPARE_US, val);
err_hwv_prepare:
	pr_err("%s pll unprepare timeout(%dus)(0x%x)\n", pll->data->name,
			i * MTK_WAIT_HWV_PLL_PREPARE_US, val);
	mtk_clk_notify(NULL, pll->hwv_regmap, NULL,
			pll->data->hwv_set_ofs, 0,
			pll->data->hwv_shift, CLK_EVT_HWV_PLL_TIMEOUT);
	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl_optional(false, PLL_MMINFRA_VOTE_BIT);
}

static int mtk_hwv_pll_setclr_is_prepared(struct mtk_clk_pll *pll,
			unsigned int msk, unsigned int type)
{
	u32 val = 0;

	regmap_read(pll->hwv_regmap, pll->data->hwv_set_ofs + (type * 0x8), &val);

	return (val & msk) != 0;
}

static int mtk_hwv_pll_setclr_is_prepare_done(struct mtk_clk_pll *pll,
			unsigned int msk, unsigned int type)
{
	u32 val = 0, val2 = 0;

	regmap_read(pll->hwv_regmap, pll->data->hwv_sta_ofs + (type * 0x4), &val);
	if (type == PLL_EN_TYPE)
		val2 = readl(pll->en_addr);
	else
		val2 = readl(pll->rstb_addr);
	if ((val & msk) && (val2 & msk))
		return 1;

	return 0;
}

static int mtk_hwv_pll_setclr_is_unprepare_done(struct mtk_clk_pll *pll,
			unsigned int msk, unsigned int type)
{
	u32 val = 0;

	if (!is_registered)
		return 0;

	regmap_read(pll->hwv_regmap, pll->data->hwv_sta_ofs + (type * 0x4), &val);

	return (val & msk) != 0;
}

static int mtk_hwv_pll_no_res_setclr_prepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 val = 0, val2 = 0;
	int i = 0;

	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(true);

	regmap_write(pll->hwv_regmap, pll->data->hwv_set_ofs, pll->en_msk);

	while (!mtk_hwv_pll_setclr_is_prepared(pll, pll->en_msk, PLL_EN_TYPE)) {
		if (i < MTK_WAIT_HWV_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PREPARE_US);
		else
			goto hwv_prepare_fail;
		i++;
	}

	i = 0;

	while (!mtk_hwv_pll_setclr_is_prepare_done(pll, pll->en_msk, PLL_EN_TYPE)) {
		if (i < MTK_WAIT_HWV_DONE_CNT)
			udelay(MTK_WAIT_HWV_DONE_US);
		else
			goto hwv_done_fail;
		i++;
	}

	udelay(20);

	if (pll->data->flags & HAVE_RST_BAR) {
		regmap_write(pll->hwv_regmap, pll->data->hwv_set_ofs + (PLL_RSTB_TYPE * 0x8),
				pll->rstb_msk);

		i = 0;

		while (!mtk_hwv_pll_setclr_is_prepared(pll, pll->rstb_msk, PLL_RSTB_TYPE)) {
			if (i < MTK_WAIT_HWV_PREPARE_CNT)
				udelay(MTK_WAIT_HWV_PREPARE_US);
			else
				goto hwv_rstb_prepare_fail;
			i++;
		}

		i = 0;

		while (!mtk_hwv_pll_setclr_is_prepare_done(pll, pll->rstb_msk, PLL_RSTB_TYPE)) {
			if (i < MTK_WAIT_HWV_DONE_CNT)
				udelay(MTK_WAIT_HWV_DONE_US);
			else
				goto hwv_rstb_done_fail;
			i++;
		}
	}

	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);

	return 0;
hwv_rstb_done_fail:
	val = readl(pll->rstb_addr);
	regmap_read(pll->hwv_regmap, pll->data->hwv_sta_ofs + (PLL_RSTB_TYPE * 0x4), &val2);
	pr_err("%s pll rstb enable timeout(%x %x)\n", clk_hw_get_name(hw), val, val2);
hwv_rstb_prepare_fail:
	regmap_read(pll->hwv_regmap, pll->data->hwv_set_ofs + (PLL_RSTB_TYPE * 0x8), &val);
	pr_err("%s pll rstb vote timeout(%x)\n", clk_hw_get_name(hw), val);
hwv_done_fail:
	val = readl(pll->en_addr);
	regmap_read(pll->hwv_regmap, pll->data->hwv_sta_ofs, &val2);
	pr_err("%s pll enable timeout(%x %x)\n", clk_hw_get_name(hw), val, val2);
hwv_prepare_fail:
	regmap_read(pll->hwv_regmap, pll->data->hwv_set_ofs, &val);
	pr_err("%s pll vote timeout(%x)\n", clk_hw_get_name(hw), val);

	mtk_clk_notify(NULL, pll->hwv_regmap, NULL,
			0, (pll->data->hwv_set_ofs / MTK_HWV_ID_OFS),
			0, CLK_EVT_HWV_CG_TIMEOUT);
	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);

	return -EBUSY;
}

static void mtk_hwv_pll_no_res_setclr_unprepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 val = 0, val2 = 0;
	int i = 0;

	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(true);

	if (pll->data->flags & HAVE_RST_BAR) {
		regmap_write(pll->hwv_regmap, pll->data->hwv_clr_ofs + (PLL_RSTB_TYPE * 0x8),
				pll->rstb_msk);

		while (mtk_hwv_pll_setclr_is_prepared(pll, pll->rstb_msk, PLL_RSTB_TYPE)) {
			if (i < MTK_WAIT_HWV_PREPARE_CNT)
				udelay(MTK_WAIT_HWV_PREPARE_US);
			else
				goto hwv_rstb_prepare_fail;
			i++;
		}

		i = 0;

		while (!mtk_hwv_pll_setclr_is_unprepare_done(pll, pll->rstb_msk , PLL_RSTB_TYPE)) {
			if (i < MTK_WAIT_HWV_DONE_CNT)
				udelay(MTK_WAIT_HWV_DONE_US);
			else
				goto hwv_rstb_done_fail;
			i++;
		}
	}

	regmap_write(pll->hwv_regmap, pll->data->hwv_clr_ofs, pll->en_msk);

	i = 0;

	while (mtk_hwv_pll_setclr_is_prepared(pll, pll->en_msk, PLL_EN_TYPE)) {
		if (i < MTK_WAIT_HWV_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PREPARE_US);
		else
			goto hwv_prepare_fail;
		i++;
	}

	i = 0;

	while (!mtk_hwv_pll_setclr_is_unprepare_done(pll, pll->en_msk, PLL_EN_TYPE)) {
		if (i < MTK_WAIT_HWV_DONE_CNT)
			udelay(MTK_WAIT_HWV_DONE_US);
		else
			goto hwv_done_fail;
		i++;
	}

	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);

	return;

hwv_rstb_done_fail:
	val = readl(pll->rstb_addr);
	regmap_read(pll->hwv_regmap, pll->data->hwv_sta_ofs + (PLL_RSTB_TYPE * 0x4), &val2);
	pr_err("%s pll rstb disable timeout(%x %x)\n", clk_hw_get_name(hw), val, val2);
hwv_rstb_prepare_fail:
	regmap_read(pll->hwv_regmap, pll->data->hwv_clr_ofs + (PLL_RSTB_TYPE * 0x8), &val);
	pr_err("%s pll rstb unvote timeout(%x)\n", clk_hw_get_name(hw), val);
hwv_done_fail:
	val = readl(pll->en_addr);
	pr_err("%s pll disable timeout(%x)\n", clk_hw_get_name(hw), val);
hwv_prepare_fail:
	regmap_read(pll->hwv_regmap, pll->data->hwv_sta_ofs, &val);
	pr_err("%s pll unvote timeout(%x)\n", clk_hw_get_name(hw), val);

	mtk_clk_notify(NULL, pll->hwv_regmap, NULL,
			0, (pll->data->hwv_clr_ofs / MTK_HWV_ID_OFS),
			0, CLK_EVT_HWV_CG_TIMEOUT);
	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl(false);
}

static int mtk_hwv_pll_setclr_prepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 val = 0, val2 = 0;
	int i = 0;
	int ret = 0;

	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl_optional(true, PLL_MMINFRA_VOTE_BIT);

	ret = mtk_hwv_pll_res_prepare(pll);
	if (ret)
		return ret;

	regmap_write(pll->hwv_regmap, pll->data->hwv_set_ofs, pll->en_msk);

	while (!mtk_hwv_pll_setclr_is_prepared(pll, pll->en_msk, PLL_EN_TYPE)) {
		if (i < MTK_WAIT_HWV_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PREPARE_US);
		else
			goto hwv_prepare_fail;
		i++;
	}

	i = 0;

	while (!mtk_hwv_pll_setclr_is_prepare_done(pll, pll->en_msk, PLL_EN_TYPE)) {
		if (i < MTK_WAIT_HWV_DONE_CNT)
			udelay(MTK_WAIT_HWV_DONE_US);
		else
			goto hwv_done_fail;
		i++;
	}

	udelay(20);

	if (pll->data->flags & HAVE_RST_BAR) {
		regmap_write(pll->hwv_regmap, pll->data->hwv_set_ofs + (PLL_RSTB_TYPE * 0x8),
				pll->rstb_msk);

		i = 0;

		while (!mtk_hwv_pll_setclr_is_prepared(pll, pll->rstb_msk, PLL_RSTB_TYPE)) {
			if (i < MTK_WAIT_HWV_PREPARE_CNT)
				udelay(MTK_WAIT_HWV_PREPARE_US);
			else
				goto hwv_rstb_prepare_fail;
			i++;
		}

		i = 0;

		while (!mtk_hwv_pll_setclr_is_prepare_done(pll, pll->rstb_msk, PLL_RSTB_TYPE)) {
			if (i < MTK_WAIT_HWV_DONE_CNT)
				udelay(MTK_WAIT_HWV_DONE_US);
			else
				goto hwv_rstb_done_fail;
			i++;
		}
	}

	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl_optional(false, PLL_MMINFRA_VOTE_BIT);

	return 0;
hwv_rstb_done_fail:
	val = readl(pll->rstb_addr);
	regmap_read(pll->hwv_regmap, pll->data->hwv_sta_ofs + (PLL_RSTB_TYPE * 0x4), &val2);
	pr_err("%s pll rstb enable timeout(%x %x)\n", clk_hw_get_name(hw), val, val2);
hwv_rstb_prepare_fail:
	regmap_read(pll->hwv_regmap, pll->data->hwv_set_ofs + (PLL_RSTB_TYPE * 0x8), &val);
	pr_err("%s pll rstb vote timeout(%x)\n", clk_hw_get_name(hw), val);
hwv_done_fail:
	val = readl(pll->en_addr);
	regmap_read(pll->hwv_regmap, pll->data->hwv_sta_ofs, &val2);
	pr_err("%s pll enable timeout(%x %x)\n", clk_hw_get_name(hw), val, val2);
hwv_prepare_fail:
	regmap_read(pll->hwv_regmap, pll->data->hwv_set_ofs, &val);
	pr_err("%s pll vote timeout(%x)\n", clk_hw_get_name(hw), val);

	mtk_clk_notify(NULL, pll->hwv_regmap, NULL,
			0, (pll->data->hwv_set_ofs / MTK_HWV_ID_OFS),
			0, CLK_EVT_HWV_CG_TIMEOUT);
	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl_optional(false, PLL_MMINFRA_VOTE_BIT);

	return -EBUSY;
}

static void mtk_hwv_pll_setclr_unprepare(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);
	u32 val = 0, val2 = 0;
	int i = 0;

	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl_optional(true, PLL_MMINFRA_VOTE_BIT);

	if (pll->data->flags & HAVE_RST_BAR) {
		regmap_write(pll->hwv_regmap, pll->data->hwv_clr_ofs + (PLL_RSTB_TYPE * 0x8),
				pll->rstb_msk);

		while (mtk_hwv_pll_setclr_is_prepared(pll, pll->rstb_msk, PLL_RSTB_TYPE)) {
			if (i < MTK_WAIT_HWV_PREPARE_CNT)
				udelay(MTK_WAIT_HWV_PREPARE_US);
			else
				goto hwv_rstb_prepare_fail;
			i++;
		}

		i = 0;

		while (!mtk_hwv_pll_setclr_is_unprepare_done(pll, pll->rstb_msk , PLL_RSTB_TYPE)) {
			if (i < MTK_WAIT_HWV_DONE_CNT)
				udelay(MTK_WAIT_HWV_DONE_US);
			else
				goto hwv_rstb_done_fail;
			i++;
		}
	}

	regmap_write(pll->hwv_regmap, pll->data->hwv_clr_ofs, pll->en_msk);

	i = 0;

	while (mtk_hwv_pll_setclr_is_prepared(pll, pll->en_msk, PLL_EN_TYPE)) {
		if (i < MTK_WAIT_HWV_PREPARE_CNT)
			udelay(MTK_WAIT_HWV_PREPARE_US);
		else
			goto hwv_prepare_fail;
		i++;
	}

	i = 0;

	while (!mtk_hwv_pll_setclr_is_unprepare_done(pll, pll->en_msk, PLL_EN_TYPE)) {
		if (i < MTK_WAIT_HWV_DONE_CNT)
			udelay(MTK_WAIT_HWV_DONE_US);
		else
			goto hwv_done_fail;
		i++;
	}

	mtk_hwv_pll_res_unprepare(pll);
	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl_optional(false, PLL_MMINFRA_VOTE_BIT);

	return;

hwv_rstb_done_fail:
	val = readl(pll->rstb_addr);
	regmap_read(pll->hwv_regmap, pll->data->hwv_sta_ofs + (PLL_RSTB_TYPE * 0x4), &val2);
	pr_err("%s pll rstb disable timeout(%x %x)\n", clk_hw_get_name(hw), val, val2);
hwv_rstb_prepare_fail:
	regmap_read(pll->hwv_regmap, pll->data->hwv_clr_ofs + (PLL_RSTB_TYPE * 0x8), &val);
	pr_err("%s pll rstb unvote timeout(%x)\n", clk_hw_get_name(hw), val);
hwv_done_fail:
	val = readl(pll->en_addr);
	pr_err("%s pll disable timeout(%x)\n", clk_hw_get_name(hw), val);
hwv_prepare_fail:
	regmap_read(pll->hwv_regmap, pll->data->hwv_sta_ofs, &val);
	pr_err("%s pll unvote timeout(%x)\n", clk_hw_get_name(hw), val);

	mtk_clk_notify(NULL, pll->hwv_regmap, NULL,
			0, (pll->data->hwv_clr_ofs / MTK_HWV_ID_OFS),
			0, CLK_EVT_HWV_CG_TIMEOUT);
	if (pll->data->flags & CLK_EN_MM_INFRA_PWR)
		mtk_clk_mminfra_hwv_power_ctrl_optional(false, PLL_MMINFRA_VOTE_BIT);
}

int mtk_hwv_pll_on(struct clk_hw *hw)
{
	return mtk_hwv_pll_prepare(hw);
}
EXPORT_SYMBOL_GPL(mtk_hwv_pll_on);

void mtk_hwv_pll_off(struct clk_hw *hw)
{
	mtk_hwv_pll_unprepare(hw);
}
EXPORT_SYMBOL_GPL(mtk_hwv_pll_off);

bool mtk_hwv_pll_is_on(struct clk_hw *hw)
{
	struct mtk_clk_pll *pll = to_mtk_clk_pll(hw);

	if (!is_registered)
		return 0;

	return mtk_hwv_pll_is_prepared_done(pll);
}
EXPORT_SYMBOL_GPL(mtk_hwv_pll_is_on);

static const struct clk_ops mtk_pll_ops = {
	.is_prepared	= mtk_pll_is_prepared,
	.prepare	= mtk_pll_prepare,
	.unprepare	= mtk_pll_unprepare,
	.recalc_rate	= mtk_pll_recalc_rate,
	.round_rate	= mtk_pll_round_rate,
	.set_rate	= mtk_pll_set_rate,
};

static const struct clk_ops mtk_pll_setclr_ops = {
	.is_prepared	= mtk_pll_setclr_is_prepared,
	.prepare	= mtk_pll_setclr_prepare,
	.unprepare	= mtk_pll_setclr_unprepare,
	.recalc_rate	= mtk_pll_recalc_rate,
	.round_rate	= mtk_pll_round_rate,
	.set_rate	= mtk_pll_set_rate,
};

static const struct clk_ops mtk_hwv_pll_ops = {
	.is_prepared	= mtk_pll_is_prepared,
	.prepare	= mtk_hwv_pll_prepare,
	.unprepare	= mtk_hwv_pll_unprepare,
	.recalc_rate	= mtk_pll_recalc_rate,
	.round_rate	= mtk_pll_round_rate,
	.set_rate	= mtk_pll_set_rate,
};

static const struct clk_ops mtk_hwv_pll_setclr_ops = {
	.is_prepared	= mtk_pll_setclr_is_prepared,
	.prepare	= mtk_hwv_pll_setclr_prepare,
	.unprepare	= mtk_hwv_pll_setclr_unprepare,
	.recalc_rate	= mtk_pll_recalc_rate,
	.round_rate	= mtk_pll_round_rate,
	.set_rate	= mtk_pll_set_rate,
};

static const struct clk_ops mtk_hwv_pll_no_res_setclr_ops = {
	.is_prepared	= mtk_pll_setclr_is_prepared,
	.prepare	= mtk_hwv_pll_no_res_setclr_prepare,
	.unprepare	= mtk_hwv_pll_no_res_setclr_unprepare,
	.recalc_rate	= mtk_pll_recalc_rate,
	.round_rate	= mtk_pll_round_rate,
	.set_rate	= mtk_pll_set_rate,
};

static struct clk *mtk_clk_register_pll(const struct mtk_pll_data *data,
		void __iomem *base,
		struct regmap *hw_voter_regmap)
{
	struct mtk_clk_pll *pll;
	struct clk_init_data init = {};
	struct clk *clk;
	const char *parent_name = "clk26m";

	pll = kzalloc(sizeof(*pll), GFP_KERNEL);
	if (!pll)
		return ERR_PTR(-ENOMEM);

	pll->base_addr = base + data->reg;
	pll->pwr_addr = base + data->pwr_reg;
	pll->pd_addr = base + data->pd_reg;
	pll->pcw_addr = base + data->pcw_reg;
	if (data->pcw_chg_reg)
		pll->pcw_chg_addr = base + data->pcw_chg_reg;
	else
		pll->pcw_chg_addr = pll->base_addr + REG_CON1;
	if (data->tuner_reg)
		pll->tuner_addr = base + data->tuner_reg;
	if (data->tuner_en_reg)
		pll->tuner_en_addr = base + data->tuner_en_reg;
	if (data->pll_setclr) {
		pll->en_addr = base + data->pll_setclr->en_ofs;
		pll->en_set_addr = base + data->pll_setclr->en_set_ofs;
		pll->en_clr_addr = base + data->pll_setclr->en_clr_ofs;
		pll->en_msk = BIT(data->en_setclr_bit);
		if ((data->flags & HAVE_RST_BAR) == HAVE_RST_BAR) {
			pll->rstb_addr = base + data->pll_setclr->rstb_ofs;
			pll->rstb_set_addr = base + data->pll_setclr->rstb_set_ofs;
			pll->rstb_clr_addr = base + data->pll_setclr->rstb_clr_ofs;
			pll->rstb_msk = BIT(data->rstb_setclr_bit);
		}
	} else {
		if (data->en_reg)
			pll->en_addr = base + data->en_reg;
		else
			pll->en_addr = pll->base_addr + REG_CON0;
	}

	pll->hw.init = &init;
	pll->data = data;

	init.name = data->name;
	init.flags = (data->flags & PLL_AO) ? CLK_IS_CRITICAL : 0;
	if (hw_voter_regmap)
		pll->hwv_regmap = hw_voter_regmap;
	if (hw_voter_regmap && (data->flags & CLK_USE_HW_VOTER)) {
		if (data->pll_setclr) {
			if (data->flags & CLK_NO_RES)
				init.ops = &mtk_hwv_pll_no_res_setclr_ops;
			else
				init.ops = &mtk_hwv_pll_setclr_ops;
		} else
			init.ops = &mtk_hwv_pll_ops;
	} else {
		if (data->pll_setclr)
			init.ops = &mtk_pll_setclr_ops;
		else
			init.ops = &mtk_pll_ops;
	}

	if (data->parent_name)
		init.parent_names = &data->parent_name;
	else
		init.parent_names = &parent_name;
	init.num_parents = 1;

	clk = clk_register(NULL, &pll->hw);

	if (IS_ERR(clk))
		kfree(pll);

	return clk;
}

void mtk_clk_register_plls(struct device_node *node,
		const struct mtk_pll_data *plls, int num_plls, struct clk_onecell_data *clk_data)
{
	void __iomem *base;
	int i;
	struct clk *clk;
	struct regmap *hw_voter_regmap, *hwv_mult_regmap = NULL;

	is_registered = false;

	base = of_iomap(node, 0);
	if (!base) {
		pr_err("%s(): ioremap failed\n", __func__);
		return;
	}

	hw_voter_regmap = syscon_regmap_lookup_by_phandle(node, "hw-voter-regmap");
	if (IS_ERR_OR_NULL(hw_voter_regmap))
		hw_voter_regmap = NULL;

	for (i = 0; i < num_plls; i++) {
		const struct mtk_pll_data *pll = &plls[i];

		if (IS_ERR_OR_NULL(clk_data->clks[pll->id])) {
			if (pll->hwv_comp) {
				hwv_mult_regmap = syscon_regmap_lookup_by_phandle(node,
						pll->hwv_comp);
				if (IS_ERR(hwv_mult_regmap))
					hwv_mult_regmap = NULL;
				hw_voter_regmap = hwv_mult_regmap;
			}

			clk = mtk_clk_register_pll(pll, base, hw_voter_regmap);

			if (IS_ERR_OR_NULL(clk)) {
				pr_err("Failed to register clk %s: %ld\n",
						pll->name, PTR_ERR(clk));
				continue;
			}

			clk_data->clks[pll->id] = clk;
		}
	}

	is_registered = true;
}
EXPORT_SYMBOL_GPL(mtk_clk_register_plls);

MODULE_LICENSE("GPL");
