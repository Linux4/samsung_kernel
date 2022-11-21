#include "../pwrcal.h"
#include "../pwrcal-clk.h"
#include "../pwrcal-env.h"
#include "../pwrcal-rae.h"
#include "../pwrcal-pmu.h"
#include "S5E7570-cmusfr.h"
#include "S5E7570-pmusfr.h"
#include "S5E7570-cmu.h"

/* PLLs */
/* PLL141XX Clock Type */
#define PLL141XX_MDIV_SHIFT		16
#define PLL141XX_PDIV_SHIFT		8
#define PLL141XX_SDIV_SHIFT		0
#define PLL141XX_MDIV_MASK		0x3FF
#define PLL141XX_PDIV_MASK		0x3F
#define PLL141XX_SDIV_MASK		0x7
#define PLL141XX_ENABLE			31
#define PLL141XX_LOCKED			29
#define PLL141XX_BYPASS			22

/* PLL1431X Clock Type */
#define PLL1431X_MDIV_SHIFT		16
#define PLL1431X_PDIV_SHIFT		8
#define PLL1431X_SDIV_SHIFT		0
#define PLL1431X_K_SHIFT		0
#define PLL1431X_MDIV_MASK		0x3FF
#define PLL1431X_PDIV_MASK		0x3F
#define PLL1431X_SDIV_MASK		0x7
#define PLL1431X_K_MASK		0xFFFF
#define PLL1431X_ENABLE			31
#define PLL1431X_LOCKED			29
#define PLL1431X_BYPASS			4

/* WPLL_USB_PLL Clock Type */
#define WIFI2AP_USBPLL_ACK	1
#define AP2WIFI_USBPLL_REQ	0
#define AP2WLBT_SR7 ((void *)(0x1058009C))

#define USBPLL_WPLL_FREF_SEL	3
#define USBPLL_WPLL_AFC_START	2
#define USBPLL_WPLL_EN	1
#define USBPLL_WPLL_SEL	0

#define FIN_HZ_26M		(26*MHZ)

static const struct pwrcal_pll_rate_table *_clk_get_pll_settings(
					struct pwrcal_pll *pll_clk,
					unsigned long long rate)
{
	int i;
	const struct pwrcal_pll_rate_table  *prate_table = pll_clk->rate_table;

	for (i = 0; i < pll_clk->rate_count; i++) {
		if (rate == prate_table[i].rate)
			return &prate_table[i];
	}

	return NULL;
}

static int _clk_pll141xx_find_pms(struct pll_spec *pll_spec,
				struct pwrcal_pll_rate_table *rate_table,
				unsigned long long rate)
{
	unsigned int p, m, s;
	unsigned long long fref, fvco, fout;
	unsigned long long tmprate, tmpfout = 0;
	unsigned long long mindiffrate = 0xFFFFFFFFFFFFFFFF;
	unsigned int min_p = 0, min_m = 0, min_s = 0, min_fout = 0;

	for (p = pll_spec->pdiv_min; p <= pll_spec->pdiv_max; p++) {
		fref = FIN_HZ_26M / p;
		if ((fref < pll_spec->fref_min) || (fref > pll_spec->fref_max))
			continue;

		for (s = pll_spec->sdiv_min; s <= pll_spec->sdiv_max; s++) {
			tmprate = rate;
			do_div(tmprate, KHZ);
			tmprate = tmprate * p * (1 << s);
			do_div(tmprate, (FIN_HZ_26M / KHZ));
			m = (unsigned int)tmprate;

			if ((m < pll_spec->mdiv_min)
					|| (m > pll_spec->mdiv_max))
				continue;

			fvco = ((unsigned long long)FIN_HZ_26M) * m;
			do_div(fvco, p);
			if ((fvco < pll_spec->fvco_min)
					|| (fvco > pll_spec->fvco_max))
				continue;

			fout = fvco >> s;
			if ((fout >= pll_spec->fout_min)
					&& (fout <= pll_spec->fout_max)) {
				tmprate = rate;
				do_div(tmprate, KHZ);
				tmpfout = fout;
				do_div(tmpfout, KHZ);
				if (tmprate == tmpfout) {
					rate_table->rate = fout;
					rate_table->pdiv = p;
					rate_table->mdiv = m;
					rate_table->sdiv = s;
					rate_table->kdiv = 0;
					return 0;
				}
				if (tmpfout < tmprate && mindiffrate > tmprate - tmpfout) {
					mindiffrate = tmprate - tmpfout;
					min_fout = fout;
					min_p = p;
					min_m = m;
					min_s = s;
				}
			}
		}
	}

	if (mindiffrate != 0xFFFFFFFFFFFFFFFF) {
		rate_table->rate = min_fout;
		rate_table->pdiv = min_p;
		rate_table->mdiv = min_m;
		rate_table->sdiv = min_s;
		rate_table->kdiv = 0;
		return 0;
	}
	return -1;
}


static int _clk_pll1419x_find_pms(struct pll_spec *pll_spec,
				struct pwrcal_pll_rate_table *rate_table,
				unsigned long long rate)
{
	unsigned int p, m, s;
	unsigned long long fref, fvco, fout;
	unsigned long long tmprate, tmpfout = 0;
	unsigned long long mindiffrate = 0xFFFFFFFFFFFFFFFF;
	unsigned int min_p = 0, min_m = 0, min_s = 0, min_fout = 0;

	for (p = pll_spec->pdiv_min; p <= pll_spec->pdiv_max; p++) {
		fref = FIN_HZ_26M / p;
		if ((fref < pll_spec->fref_min) || (fref > pll_spec->fref_max))
			continue;

		for (s = pll_spec->sdiv_min; s <= pll_spec->sdiv_max; s++) {
			/*tmprate = rate;*/
			tmprate = rate/2; /*for PLL1419*/
			do_div(tmprate, KHZ);
			tmprate = tmprate * p * (1 << s);
			do_div(tmprate, (FIN_HZ_26M / KHZ));
			m = (unsigned int)tmprate;

			if ((m < pll_spec->mdiv_min)
					|| (m > pll_spec->mdiv_max))
				continue;

			fvco = ((unsigned long long)FIN_HZ_26M) * m;
			do_div(fvco, p);
			if ((fvco < pll_spec->fvco_min)
					|| (fvco > pll_spec->fvco_max))
				continue;

			fout = fvco >> s;
			if ((fout >= pll_spec->fout_min)
					&& (fout <= pll_spec->fout_max)) {
				tmprate = rate;
				do_div(tmprate, KHZ);
				tmpfout = fout;
				do_div(tmpfout, KHZ);
				if (tmprate == tmpfout) {
					rate_table->rate = fout;
					rate_table->pdiv = p;
					rate_table->mdiv = m;
					rate_table->sdiv = s;
					rate_table->kdiv = 0;
					return 0;
				}
			}
			if (tmpfout < tmprate && mindiffrate > tmprate - tmpfout) {
				mindiffrate = tmprate - tmpfout;
				min_fout = fout;
				min_p = p;
				min_m = m;
				min_s = s;
			}
		}
	}

	if (mindiffrate != 0xFFFFFFFFFFFFFFFF) {
		rate_table->rate = min_fout;
		rate_table->pdiv = min_p;
		rate_table->mdiv = min_m;
		rate_table->sdiv = min_s;
		rate_table->kdiv = 0;
		return 0;
	}
	return -1;
}

static int _clk_pll141xx_is_enabled(struct pwrcal_clk *clk)
{
	return (int)(pwrcal_getbit(clk->offset, PLL141XX_ENABLE));
}

static int _clk_pll141xx_enable(struct pwrcal_clk *clk)
{
	int timeout;

	pwrcal_setbit(clk->offset, PLL141XX_ENABLE, 1);

	for (timeout = 0;; timeout++) {
		if (pwrcal_getbit(clk->offset, PLL141XX_LOCKED)) {
			exynos_ss_printk("%s : 0x%X locktime = %d", __func__,
					pwrcal_readl(clk->offset), timeout);
			break;
		}
		if (timeout > CLK_WAIT_CNT)
			return -1;
		cpu_relax();
	}

	return 0;
}

static int _clk_pll1419x_enable(struct pwrcal_clk *clk)
{
	int timeout;

	pwrcal_setbit(clk->offset, PLL141XX_ENABLE, 1);

	for (timeout = 0;; timeout++) {
		if (pwrcal_getbit(clk->offset, PLL141XX_LOCKED))
			break;
		if (timeout > CLK_WAIT_CNT)
			return -1;
		cpu_relax();
	}

	return 0;
}

static int _clk_pll141xx_disable(struct pwrcal_clk *clk)
{
	pwrcal_setbit(clk->offset, PLL141XX_ENABLE, 0);

	return 0;
}

int _clk_pll141xx_is_disabled_bypass(struct pwrcal_clk *clk)
{
	if (pwrcal_getbit(clk->offset + 1, PLL141XX_BYPASS))
		return 0;

	return 1;
}


int _clk_pll141xx_set_bypass(struct pwrcal_clk *clk, int bypass_disable)
{
	if (bypass_disable == 0)
		pwrcal_setbit(clk + 1, PLL141XX_BYPASS, 1);
	else
		pwrcal_setbit(clk + 1, PLL141XX_BYPASS, 0);

	return 0;
}

static int _clk_pll141xx_set_pms(struct pwrcal_clk *clk,
				const struct pwrcal_pll_rate_table  *rate_table)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0;

	int timeout;

	pdiv = rate_table->pdiv;
	mdiv = rate_table->mdiv;
	sdiv = rate_table->sdiv;

	pll_con0 = pwrcal_readl(clk->offset);
	pll_con0 &= ~((PLL141XX_MDIV_MASK << PLL141XX_MDIV_SHIFT)
			| (PLL141XX_PDIV_MASK << PLL141XX_PDIV_SHIFT)
			| (PLL141XX_SDIV_MASK << PLL141XX_SDIV_SHIFT));
	pll_con0 |=  (mdiv << PLL141XX_MDIV_SHIFT)
			| (pdiv << PLL141XX_PDIV_SHIFT)
			| (sdiv << PLL141XX_SDIV_SHIFT);
	pll_con0 &= ~(1<<26);
	pll_con0 |= (1<<5);

	exynos_ss_printk("%s : 0x%X = 0x%X, %d, %d, %d", __func__,
				clk->offset, pll_con0, pdiv, mdiv, sdiv);

	pwrcal_writel(clk->status, pdiv*150);
	pwrcal_writel(clk->offset, pll_con0);

	if (pll_con0 & (1 << PLL141XX_ENABLE)) {
		for (timeout = 0;; timeout++) {
			if (pwrcal_getbit(clk->offset, PLL141XX_LOCKED)) {
				exynos_ss_printk("%s : 0x%X locktime = %d", __func__,
						pwrcal_readl(clk->offset), timeout);
				break;
			}
			if (timeout > CLK_WAIT_CNT)
				return -1;
			cpu_relax();
		}
	}

	return 0;

}

static int _clk_pll1419x_set_pms(struct pwrcal_clk *clk,
				const struct pwrcal_pll_rate_table  *rate_table)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0;

	int timeout;

	pdiv = rate_table->pdiv;
	mdiv = rate_table->mdiv;
	sdiv = rate_table->sdiv;

	pll_con0 = pwrcal_readl(clk->offset);
	pll_con0 &= ~((PLL141XX_MDIV_MASK << PLL141XX_MDIV_SHIFT)
			| (PLL141XX_PDIV_MASK << PLL141XX_PDIV_SHIFT)
			| (PLL141XX_SDIV_MASK << PLL141XX_SDIV_SHIFT));
	pll_con0 |=  (mdiv << PLL141XX_MDIV_SHIFT)
			| (pdiv << PLL141XX_PDIV_SHIFT)
			| (sdiv << PLL141XX_SDIV_SHIFT);

	pwrcal_writel(clk->status, pdiv * 150);
	pwrcal_writel(clk->offset, pll_con0);

	if (pll_con0 & (1 << PLL141XX_ENABLE)) {
		for (timeout = 0;; timeout++) {
			if (pwrcal_getbit(clk->offset, PLL141XX_LOCKED))
				break;
			if (timeout > CLK_WAIT_CNT)
				return -1;
			cpu_relax();
		}
	}

	return 0;
}

static int _clk_pll141xx_set_rate(struct pwrcal_clk *clk,
					unsigned long long rate)
{
	struct pwrcal_pll *pll = to_pll(clk);
	struct pll_spec *pll_spec;
	struct pwrcal_pll_rate_table  tmp_rate_table;
	const struct pwrcal_pll_rate_table  *rate_table;

	if (rate == 0) {
		if (_clk_pll141xx_is_enabled(clk) != 0)
			if (_clk_pll141xx_disable(clk))
				goto errorout;
		return 0;
	}

	rate_table = _clk_get_pll_settings(pll, rate);
	if (rate_table == NULL) {
		pll_spec = clk_pll_get_spec(clk);
		if (pll_spec == NULL)
			goto errorout;

		if (_clk_pll141xx_find_pms(pll_spec, &tmp_rate_table, rate)) {
			pr_err("can't find pms value for rate(%lldHz) of \'%s\'",
				rate,
				clk->name);
			goto errorout;
		}

		rate_table = &tmp_rate_table;

		pr_warn("not exist in rate table, p(%d), m(%d), s(%d), fout(%lldHz) %s",
				rate_table->pdiv,
				rate_table->mdiv,
				rate_table->sdiv,
				rate,
				clk->name);
	}

	if (_clk_pll141xx_set_pms(clk, rate_table))
		goto errorout;

	if (rate != 0) {
		if (_clk_pll141xx_is_enabled(clk) == 0)
			_clk_pll141xx_enable(clk);
	}
	return 0;

errorout:
	return -1;
}

static int _clk_pll1419x_set_rate(struct pwrcal_clk *clk,
					unsigned long long rate)
{
	struct pwrcal_pll *pll = to_pll(clk);
	struct pll_spec *pll_spec;
	struct pwrcal_pll_rate_table  tmp_rate_table;
	const struct pwrcal_pll_rate_table  *rate_table;

	if (rate == 0) {
		if (_clk_pll141xx_is_enabled(clk) != 0)
			if (_clk_pll141xx_disable(clk))
				goto errorout;
		return 0;
	}

	rate_table = _clk_get_pll_settings(pll, rate);
	if (rate_table == NULL) {
		pll_spec = clk_pll_get_spec(clk);
		if (pll_spec == NULL)
			goto errorout;

		if (_clk_pll1419x_find_pms(pll_spec, &tmp_rate_table, rate)) {
			pr_err("can't find pms value for rate(%lldHz) of \'%s\'",
				rate,
				clk->name);
			goto errorout;
		}

		rate_table = &tmp_rate_table;

		pr_warn("not exist in rate table, p(%d), m(%d), s(%d), fout(%lldHz) %s",
				rate_table->pdiv,
				rate_table->mdiv,
				rate_table->sdiv,
				rate,
				clk->name);
	}

	_clk_pll141xx_disable(clk);

	if (_clk_pll1419x_set_pms(clk, rate_table))
		goto errorout;

	if (rate != 0) {
		if (_clk_pll141xx_is_enabled(clk) == 0)
			_clk_pll1419x_enable(clk);
	}
	return 0;

errorout:
	return -1;
}

static unsigned long long _clk_pll141xx_get_rate(struct pwrcal_clk *clk)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0;
	unsigned long long fout;

	if (_clk_pll141xx_is_enabled(clk) == 0)
		return 0;
	pll_con0 = pwrcal_readl(clk->offset);
	mdiv = (pll_con0 >> PLL141XX_MDIV_SHIFT) & PLL141XX_MDIV_MASK;
	pdiv = (pll_con0 >> PLL141XX_PDIV_SHIFT) & PLL141XX_PDIV_MASK;
	sdiv = (pll_con0 >> PLL141XX_SDIV_SHIFT) & PLL141XX_SDIV_MASK;

	if (pdiv == 0) {
		pr_err("pdiv is 0, id(%s)", clk->name);
		return 0;
	}
	fout = FIN_HZ_26M * mdiv;
	do_div(fout, (pdiv << sdiv));
	return (unsigned long long)fout;
}

static unsigned long long _clk_pll1419x_get_rate(struct pwrcal_clk *clk)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0;
	unsigned long long fout;

	if (_clk_pll141xx_is_enabled(clk) == 0)
		return 0;

	pll_con0 = pwrcal_readl(clk->offset);
	mdiv = (pll_con0 >> PLL141XX_MDIV_SHIFT) & PLL141XX_MDIV_MASK;
	pdiv = (pll_con0 >> PLL141XX_PDIV_SHIFT) & PLL141XX_PDIV_MASK;
	sdiv = (pll_con0 >> PLL141XX_SDIV_SHIFT) & PLL141XX_SDIV_MASK;

	if (pdiv == 0) {
		pr_err("pdiv is 0, id(%s)", clk->name);
		return 0;
	}
	fout = FIN_HZ_26M * 2 * mdiv;
	do_div(fout, (pdiv << sdiv));
	return (unsigned long long)fout;
}

static int _clk_pll1431x_find_pms(struct pll_spec *pll_spec,
					struct pwrcal_pll_rate_table *rate_table,
					unsigned long long rate)
{
	unsigned int p, m, s;
	signed short k;
	unsigned long long fref, fvco, fout;
	unsigned long long tmprate, tmpfout;

	for (p = pll_spec->pdiv_min; p <= pll_spec->pdiv_max; p++) {
		fref = FIN_HZ_26M / p;
		if ((fref < pll_spec->fref_min) || (fref > pll_spec->fref_max))
			continue;

		for (s = pll_spec->sdiv_min; s <= pll_spec->sdiv_max; s++) {
			tmprate = rate;
			do_div(tmprate, KHZ);
			tmprate = tmprate * p * (1 << s);
			do_div(tmprate, (FIN_HZ_26M / KHZ));
			m = (unsigned int)tmprate;

			if ((m < pll_spec->mdiv_min)
					|| (m > pll_spec->mdiv_max))
				continue;

			tmprate = rate;
			do_div(tmprate, KHZ);
			tmprate = tmprate * p * (1 << s);
			do_div(tmprate, (FIN_HZ_26M / KHZ));
			tmprate = (tmprate - m) * 65536;
			k = (unsigned int)tmprate;
			if ((k < pll_spec->kdiv_min)
					|| (k > pll_spec->kdiv_max))
				continue;

			fvco = FIN_HZ_26M * ((m << 16) + k);
			do_div(fvco, p);
			fvco >>= 16;
			if ((fvco < pll_spec->fvco_min)
					|| (fvco > pll_spec->fvco_max))
				continue;

			fout = fvco >> s;
			if ((fout >= pll_spec->fout_min)
				&& (fout <= pll_spec->fout_max)) {
				tmprate = rate;
				do_div(tmprate, KHZ);
				tmpfout = fout;
				do_div(tmpfout, KHZ);
				if (tmprate == tmpfout) {
					rate_table->rate = fout;
					rate_table->pdiv = p;
					rate_table->mdiv = m;
					rate_table->sdiv = s;
					rate_table->kdiv = k;
					return 0;
				}
			}
		}
	}

	return -1;
}

static int _clk_pll1431x_is_enabled(struct pwrcal_clk *clk)
{
	return (int)(pwrcal_getbit(clk->offset, PLL1431X_ENABLE));
}

static int _clk_pll1431x_enable(struct pwrcal_clk *clk)
{
	int timeout;

	pwrcal_setbit(clk->offset, PLL1431X_ENABLE, 1);

	for (timeout = 0;; timeout++) {
		if (pwrcal_getbit(clk->offset, PLL1431X_LOCKED))
			break;
		if (timeout > CLK_WAIT_CNT)
			return -1;
		cpu_relax();
	}

	return 0;
}

static int _clk_pll1431x_disable(struct pwrcal_clk *clk)
{
	pwrcal_setbit(clk->offset, PLL1431X_ENABLE, 0);

	return 0;
}

int _clk_pll1431x_is_disabled_bypass(struct pwrcal_clk *clk)
{
	if (pwrcal_getbit(clk->offset + 2, PLL1431X_BYPASS))
		return 0;

	return 1;
}

int _clk_pll1431x_set_bypass(struct pwrcal_clk *clk, int bypass_disable)
{
	if (bypass_disable == 0)
		pwrcal_setbit(clk->offset + 2, PLL1431X_BYPASS, 1);
	else
		pwrcal_setbit(clk->offset + 2, PLL1431X_BYPASS, 0);

	return 0;
}

static int _clk_pll1431x_set_pms(struct pwrcal_clk *clk,
			const struct pwrcal_pll_rate_table  *rate_table)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0, pll_con1;
	signed short kdiv;
	int timeout;

	pdiv = rate_table->pdiv;
	mdiv = rate_table->mdiv;
	sdiv = rate_table->sdiv;
	kdiv = rate_table->kdiv;

	pll_con0 = pwrcal_readl(clk->offset);
	pll_con1 = pwrcal_readl(clk->offset + 1);
	pll_con0 &= ~((PLL1431X_MDIV_MASK << PLL1431X_MDIV_SHIFT)
			| (PLL1431X_PDIV_MASK << PLL1431X_PDIV_SHIFT)
			| (PLL1431X_SDIV_MASK << PLL1431X_SDIV_SHIFT));
	pll_con0 |=  (mdiv << PLL1431X_MDIV_SHIFT)
			| (pdiv << PLL1431X_PDIV_SHIFT)
			| (sdiv << PLL1431X_SDIV_SHIFT);
	pll_con0 &= ~(1<<26);
	pll_con0 |= (1<<5);

	pll_con1 &= ~(PLL1431X_K_MASK << PLL1431X_K_SHIFT);
	pll_con1 |=  (kdiv << PLL1431X_K_SHIFT);

	if (kdiv == 0)
		pwrcal_writel(clk->status, pdiv*3000);
	else
		pwrcal_writel(clk->status, pdiv*3000);
	pwrcal_writel(clk->offset, pll_con0);
	pwrcal_writel(clk->offset + 1, pll_con1);

	if (pll_con0 & (1 << PLL1431X_ENABLE)) {
		for (timeout = 0;; timeout++) {
			if (pwrcal_getbit(clk->offset, PLL1431X_LOCKED))
				break;
			if (timeout > CLK_WAIT_CNT)
				return -1;
			cpu_relax();
		}
	}

	return 0;
}

static int _clk_pll1431x_set_rate(struct pwrcal_clk *clk,
					unsigned long long rate)
{
	struct pwrcal_pll *pll = to_pll(clk);
	struct pwrcal_pll_rate_table  tmp_rate_table;
	const struct pwrcal_pll_rate_table  *rate_table;
	struct pll_spec *pll_spec;

	if (rate == 0) {
		if (_clk_pll1431x_is_enabled(clk) != 0)
			if (_clk_pll1431x_disable(clk))
				goto errorout;
		return 0;
	}

	rate_table = _clk_get_pll_settings(pll, rate);
	if (rate_table == NULL) {
		pll_spec = clk_pll_get_spec(clk);
		if (pll_spec == NULL)
			goto errorout;

		if (_clk_pll1431x_find_pms(pll_spec, &tmp_rate_table, rate) < 0) {
			pr_err("can't find pms value for rate(%lldHz) of %s",
				rate,
				clk->name);
			goto errorout;
		}

		rate_table = &tmp_rate_table;
		pr_warn("not exist in rate table, p(%d) m(%d) s(%d) k(%d) fout(%lld Hz) of %s",
				rate_table->pdiv,
				rate_table->mdiv,
				rate_table->sdiv,
				rate_table->kdiv,
				rate,
				clk->name);
	}

	if (_clk_pll1431x_set_pms(clk, rate_table))
		goto errorout;

	if (rate != 0) {
		if (_clk_pll1431x_is_enabled(clk) == 0)
			_clk_pll1431x_enable(clk);
	}

	return 0;

errorout:
	return -1;
}

static unsigned long long _clk_pll1431x_get_rate(struct pwrcal_clk *clk)
{
	unsigned int mdiv, pdiv, sdiv, pll_con0, pll_con1;
	signed short kdiv;
	unsigned long long fout;

	if (_clk_pll1431x_is_enabled(clk) == 0)
		return 0;

	pll_con0 = pwrcal_readl(clk->offset);
	pll_con1 = pwrcal_readl(clk->offset + 1);
	mdiv = (pll_con0 >> PLL1431X_MDIV_SHIFT) & PLL1431X_MDIV_MASK;
	pdiv = (pll_con0 >> PLL1431X_PDIV_SHIFT) & PLL1431X_PDIV_MASK;
	sdiv = (pll_con0 >> PLL1431X_SDIV_SHIFT) & PLL1431X_SDIV_MASK;

	kdiv = (short)(pll_con1 >> PLL1431X_K_SHIFT) & PLL1431X_K_MASK;

	if (pdiv == 0) {
		pr_err("pdiv is 0, id(%s)", clk->name);
		return 0;
	}

	fout = FIN_HZ_26M * ((mdiv << 16) + kdiv);
	do_div(fout, (pdiv << sdiv));
	fout >>= 16;

	return (unsigned long long)fout;
}

static int _clk_wpll_usbpll_is_enabled(struct pwrcal_clk *clk)
{
	if (pwrcal_getbit(clk->offset, USBPLL_WPLL_SEL) == 1) {
		pr_err("%s: USBPLL_WPLL_SEL==1\n", __func__);
		return (int)(pwrcal_getbit(clk->offset, USBPLL_WPLL_EN));
	}
	pr_err("%s: USBPLL_WPLL_SEL==0\n", __func__);
	return (int)pwrcal_getbit(clk->status, WIFI2AP_USBPLL_ACK);
}

static int _clk_wpll_usbpll_enable(struct pwrcal_clk *clk)
{
	int timeout;

	/* check changed WPLL input selection to AP (AP2WLBT_USBPLL_WPLL_SEL). */
	if (pwrcal_getbit(clk->offset, USBPLL_WPLL_SEL) == 0x1) {
		pr_info("%s AP %p\n", __func__, clk->offset);

		if (pwrcal_getbit(clk->offset, USBPLL_WPLL_EN) == 0x1) {
			pr_info("%s USBPLL_WPLL_EN==1\n", __func__);
			return 0;
		}

		/* pwrcal_setbit(clk->offset, 0, 0x1); */
		if (pwrcal_getbit(TCXO_SHARED_STATUS, 8) == 1)
			pwrcal_setbit(clk->offset, USBPLL_WPLL_FREF_SEL, 0x1); /* TCXO_SHARED_STATUS = 52Mhz */
		else
			pwrcal_setbit(clk->offset, USBPLL_WPLL_FREF_SEL, 0x0); /* 26Mhz */

		/* Set WPLL enable (AP2WLBT_USBPLL_WPLL_EN) */
		pwrcal_setbit(clk->offset, USBPLL_WPLL_EN, 0x1);

		/* wait 20us for power settle time. */
		for (timeout = 0;; timeout++) {
			if (timeout >= 20)
				break;
			cpu_relax();
		}

		/* Set WPLL AFC Start (AP2WLBT_USBPLL_WPLL_AFC_START) */
		pwrcal_setbit(clk->offset, USBPLL_WPLL_AFC_START, 0x1);

		/* wait 60us for clock stabilization. */
		for (timeout = 0;; timeout++) {
			if (timeout >= 60)
				break;
			cpu_relax();
		}

	} else {
		pr_info("%s WLBT\n", __func__);

		/* WPLL input selection to WLBT */

		/* (IP) use ack */
		if (pwrcal_getbit(clk->status, WIFI2AP_USBPLL_ACK) == 0x1) {
			pr_info("%s USBPLL_ACK==1, already\n", __func__);
			return 0;
		}

		/* AP2WIFI_USBPLL_REQ */
		pwrcal_setbit(clk->status, AP2WIFI_USBPLL_REQ, 0x1);
		pr_info("%s AP2WIFI_USBPLL_REQ=1\n", __func__);
	}

	return 0;
}

static int _clk_wpll_usbpll_disable(struct pwrcal_clk *clk)
{
	if (pwrcal_getbit(clk->offset, USBPLL_WPLL_SEL) == 0) {
		pwrcal_setbit(clk->status, AP2WIFI_USBPLL_REQ, 0x0);
		pr_err("%s AP2WIFI_USBPLL_REQ=0\n", __func__);
	}

	return 0;
}


static int _clk_wpll_usbpll_set_rate(struct pwrcal_clk *clk,
					unsigned long long rate)
{

	if (rate == 0) {
		if (_clk_wpll_usbpll_is_enabled(clk) != 0)
			if (_clk_wpll_usbpll_disable(clk))
				goto errorout;
	} else { /* rate != 0  */
		if (_clk_wpll_usbpll_is_enabled(clk) == 0)
			_clk_wpll_usbpll_enable(clk);
		/*	pr_warn("WPLL_USBPLL set 20Mhz"); */
	}
	return 0;

errorout:
	return -1;
}

static unsigned long long _clk_wpll_usbpll_get_rate(struct pwrcal_clk *clk)
{
	unsigned long long fout;

	if (_clk_wpll_usbpll_is_enabled(clk) == 0)
		return 0;

	fout = (20*MHZ);
	return (unsigned long long)fout;
}

struct pwrcal_pll_ops pll141xx_ops = {
	.is_enabled = _clk_pll141xx_is_enabled,
	.enable = _clk_pll141xx_enable,
	.disable = _clk_pll141xx_disable,
	.set_rate = _clk_pll141xx_set_rate,
	.get_rate = _clk_pll141xx_get_rate,
};

struct pwrcal_pll_ops pll1419x_ops = {
	.is_enabled = _clk_pll141xx_is_enabled,
	.enable = _clk_pll1419x_enable,
	.disable = _clk_pll141xx_disable,
	.set_rate = _clk_pll1419x_set_rate,
	.get_rate = _clk_pll1419x_get_rate,
};

struct pwrcal_pll_ops pll1431x_ops = {
	.is_enabled = _clk_pll1431x_is_enabled,
	.enable = _clk_pll1431x_enable,
	.disable = _clk_pll1431x_disable,
	.set_rate = _clk_pll1431x_set_rate,
	.get_rate = _clk_pll1431x_get_rate,
};

struct pwrcal_pll_ops wpll_usbpll_ops = {
	.is_enabled = _clk_wpll_usbpll_is_enabled,
	.enable = _clk_wpll_usbpll_enable,
	.disable = _clk_wpll_usbpll_disable,
	.set_rate = _clk_wpll_usbpll_set_rate,
	.get_rate = _clk_wpll_usbpll_get_rate,
};
