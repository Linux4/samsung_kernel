// SPDX-License-Identifier: GPL-2.0-only
/*
 * Unisoc UMS9620 clock driver
 *
 * Copyright (C) 2020 Unisoc, Inc.
 * Author: Xiongpeng Wu <xiongpeng.wu@unisoc.com>
 */

#include <linux/clk-provider.h>
#include <linux/err.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include <dt-bindings/clock/sprd,ums9620-clk.h>

#include "common.h"
#include "composite.h"
#include "div.h"
#include "gate.h"
#include "mux.h"
#include "pll.h"

#define UMS9620_MUX_FLAG	\
	(CLK_GET_RATE_NOCACHE | CLK_SET_RATE_NO_REPARENT)

/* pll gate clock */
static CLK_FIXED_FACTOR(clk_26m_aud, "clk-26m-aud", "ext-26m", 1, 1, 0);
static CLK_FIXED_FACTOR(clk_13m, "clk-13m", "ext-26m", 2, 1, 0);
static CLK_FIXED_FACTOR(clk_6m5, "clk-6m5", "ext-26m", 4, 1, 0);
static CLK_FIXED_FACTOR(clk_4m3, "clk-4m3", "ext-26m", 6, 1, 0);
static CLK_FIXED_FACTOR(clk_4m, "clk-4m", "ext-26m", 13, 2, 0);
static CLK_FIXED_FACTOR(clk_2m, "clk-2m", "ext-26m", 13, 1, 0);
static CLK_FIXED_FACTOR(clk_1m, "clk-1m", "ext-26m", 26, 1, 0);
static CLK_FIXED_FACTOR(clk_250k, "clk-250k", "ext-26m", 104, 1, 0);
static CLK_FIXED_FACTOR(clk_16k, "clk-16k", "ext-26m", 1625, 1, 0);
static CLK_FIXED_FACTOR(rco_100m_25m, "rco-100m-25m", "rco-100m", 4, 1, 0);
static CLK_FIXED_FACTOR(rco_100m_20m, "rco-100m-20m", "rco-100m", 5, 1, 0);
static CLK_FIXED_FACTOR(rco_100m_4m, "rco-100m-4m", "rco-100m", 25, 1, 0);
static CLK_FIXED_FACTOR(rco_100m_2m, "rco-100m-2m", "rco-100m", 50, 1, 0);
static CLK_FIXED_FACTOR(rco_60m_20m, "rco-60m-20m", "rco-60m", 3, 1, 0);
static CLK_FIXED_FACTOR(rco_60m_4m, "rco-60m-4m", "rco-60m", 15, 1, 0);
static CLK_FIXED_FACTOR(rco_60m_2m, "rco-60m-2m", "rco-60m", 30, 1, 0);
static SPRD_PLL_SC_GATE_CLK(phyr8pll_gate, "phyr8pll-gate", "ext-26m", 0xa30,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(psr8pll_gate, "psr8pll-gate", "ext-26m", 0xa34,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(cpll_gate, "cpll-gate", "ext-26m", 0xa80,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(v4nrpll_gate, "v4nrpll-gate", "ext-26m", 0xa44,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(rpll_gate, "rpll-gate", "ext-26m", 0xa0c,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(tgpll_gate, "tgpll-gate", "ext-26m", 0xa40,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(mplll_gate, "mplll-gate", "ext-26m", 0x9f8,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(mpllm_gate, "mpllm-gate", "ext-26m", 0x9fc,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(mpllb_gate, "mpllb-gate", "ext-26m", 0xa00,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(mplls_gate, "mplls-gate", "ext-26m", 0x9f4,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(dpll0_gate, "dpll0-gate", "ext-26m", 0xa14,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(dpll1_gate, "dpll1-gate", "ext-26m", 0xa18,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(dpll2_gate, "dpll2-gate", "ext-26m", 0xa1c,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(gpll_gate, "gpll-gate", "ext-26m", 0xa20,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(aipll_gate, "aipll-gate", "ext-26m", 0xa24,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(vdsppll_gate, "vdsppll-gate", "ext-26m", 0xa2c,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(audpll_gate, "audpll-gate", "ext-26m", 0xa48,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);
static SPRD_PLL_SC_GATE_CLK(pixelpll_gate, "pixelpll-gate", "ext-26m", 0xa5c,
			    0x1000, BIT(2), CLK_IGNORE_UNUSED, 0, 240);

static struct sprd_clk_common *ums9620_pmu_gate_clks[] = {
	/* address base is 0x64910000 */
	&phyr8pll_gate.common,
	&psr8pll_gate.common,
	&cpll_gate.common,
	&v4nrpll_gate.common,
	&rpll_gate.common,
	&tgpll_gate.common,
	&mplll_gate.common,
	&mpllm_gate.common,
	&mpllb_gate.common,
	&mplls_gate.common,
	&dpll0_gate.common,
	&dpll1_gate.common,
	&dpll2_gate.common,
	&gpll_gate.common,
	&aipll_gate.common,
	&vdsppll_gate.common,
	&audpll_gate.common,
	&pixelpll_gate.common,
};

static struct clk_hw_onecell_data ums9620_pmu_gate_hws = {
	.hws	= {
		[CLK_26M_AUD]		= &clk_26m_aud.hw,
		[CLK_13M]		= &clk_13m.hw,
		[CLK_6M5]		= &clk_6m5.hw,
		[CLK_4M3]		= &clk_4m3.hw,
		[CLK_4M]		= &clk_4m.hw,
		[CLK_2M]		= &clk_2m.hw,
		[CLK_1M]		= &clk_1m.hw,
		[CLK_250K]		= &clk_250k.hw,
		[CLK_16K]		= &clk_16k.hw,
		[CLK_RCO_100M_25M]	= &rco_100m_25m.hw,
		[CLK_RCO_100m_20M]      = &rco_100m_20m.hw,
		[CLK_RCO_100m_4M]	= &rco_100m_4m.hw,
		[CLK_RCO_100m_2M]	= &rco_100m_2m.hw,
		[CLK_RCO_60m_20M]	= &rco_60m_20m.hw,
		[CLK_RCO_60m_4M]	= &rco_60m_4m.hw,
		[CLK_RCO_60m_2M]	= &rco_60m_2m.hw,
		[CLK_PHYR8PLL_GATE]	= &phyr8pll_gate.common.hw,
		[CLK_PSR8PLL_GATE]	= &psr8pll_gate.common.hw,
		[CLK_CPLL_GATE]		= &cpll_gate.common.hw,
		[CLK_V4NRPLL_GATE]	= &v4nrpll_gate.common.hw,
		[CLK_TGPLL_GATE]	= &tgpll_gate.common.hw,
		[CLK_MPLLL_GATE]	= &mplll_gate.common.hw,
		[CLK_MPLLM_GATE]	= &mpllm_gate.common.hw,
		[CLK_MPLLB_GATE]	= &mpllb_gate.common.hw,
		[CLK_MPLLS_GATE]	= &mplls_gate.common.hw,
		[CLK_DPLL0_GATE]	= &dpll0_gate.common.hw,
		[CLK_DPLL1_GATE]	= &dpll1_gate.common.hw,
		[CLK_DPLL2_GATE]	= &dpll2_gate.common.hw,
		[CLK_GPLL_GATE]		= &gpll_gate.common.hw,
		[CLK_AIPLL_GATE]	= &aipll_gate.common.hw,
		[CLK_VDSPPLL_GATE]	= &vdsppll_gate.common.hw,
		[CLK_AUDPLL_GATE]	= &audpll_gate.common.hw,
		[CLK_PIXELPLL_GATE]	= &pixelpll_gate.common.hw,

	},
	.num = CLK_PMU_GATE_NUM,
};

static struct sprd_clk_desc ums9620_pmu_gate_desc = {
	.clk_clks	= ums9620_pmu_gate_clks,
	.num_clk_clks	= ARRAY_SIZE(ums9620_pmu_gate_clks),
	.hw_clks        = &ums9620_pmu_gate_hws,
};

/* pll clock at g1 */
static struct freq_table rpll_ftable[5] = {
	{ .ibias = 1, .max_freq = 2000000000ULL },
	{ .ibias = 2, .max_freq = 2800000000ULL },
	{ .ibias = 3, .max_freq = 3200000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_rpll[PLL_FACT_MAX] = {
	{ .shift = 18,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 4 },	/* icp		*/
	{ .shift = 7,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 8 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 66,	.width = 1 },	/* postdiv	*/
};

static SPRD_PLL_WITH_ITABLE_K_FVCO(rpll, "rpll", "ext-26m", 0x10,
				   3, rpll_ftable, f_rpll, 240,
				   1000, 1000, 1, 1560000000);
static CLK_FIXED_FACTOR(rpll_390m, "rpll-390m", "rpll", 2, 1, 0);
static CLK_FIXED_FACTOR(rpll_26m, "rpll-26m", "rpll", 30, 1, 0);

static struct sprd_clk_common *ums9620_g1_pll_clks[] = {
	/* address base is 0x64304000 */
	&rpll.common,
};

static struct clk_hw_onecell_data ums9620_g1_pll_hws = {
	.hws    = {
		[CLK_RPLL]		= &rpll.common.hw,
		[CLK_RPLL_390M]		= &rpll_390m.hw,
		[CLK_RPLL_26M]		= &rpll_26m.hw,
	},
	.num    = CLK_ANLG_PHY_G1_NUM,
};

static struct sprd_clk_desc ums9620_g1_pll_desc = {
	.clk_clks	= ums9620_g1_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(ums9620_g1_pll_clks),
	.hw_clks	= &ums9620_g1_pll_hws,
};

/* pll at g1l */
static struct freq_table dpll_ftable[4] = {
	{ .ibias = 1, .max_freq = 2000000000ULL },
	{ .ibias = 2, .max_freq = 2800000000ULL },
	{ .ibias = 3, .max_freq = 3200000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_dpll[PLL_FACT_MAX] = {
	{ .shift = 18,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 4 },	/* icp		*/
	{ .shift = 7,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 8 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 66,	.width = 1 },	/* postdiv	*/
};

static SPRD_PLL_WITH_ITABLE_K_FVCO(dpll0, "dpll0", "ext-26m", 0x4,
				   3, dpll_ftable, f_dpll, 240,
				   1000, 1000, 1, 1500000000);

static SPRD_PLL_WITH_ITABLE_K_FVCO(dpll1, "dpll1", "ext-26m", 0x24,
				   3, dpll_ftable, f_dpll, 240,
				   1000, 1000, 1, 1500000000);

static SPRD_PLL_WITH_ITABLE_K_FVCO(dpll2, "dpll2", "ext-26m", 0x44,
				   3, dpll_ftable, f_dpll, 240,
				   1000, 1000, 1, 1500000000);

static struct sprd_clk_common *ums9620_g1l_pll_clks[] = {
	/* address base is 0x64308000 */
	&dpll0.common,
	&dpll1.common,
	&dpll2.common,
};

static struct clk_hw_onecell_data ums9620_g1l_pll_hws = {
	.hws    = {
		[CLK_DPLL0]		= &dpll0.common.hw,
		[CLK_DPLL1]		= &dpll1.common.hw,
		[CLK_DPLL2]		= &dpll2.common.hw,
	},
	.num    = CLK_ANLG_PHY_G1L_NUM,
};

static struct sprd_clk_desc ums9620_g1l_pll_desc = {
	.clk_clks	= ums9620_g1l_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(ums9620_g1l_pll_clks),
	.hw_clks	= &ums9620_g1l_pll_hws,
};

/* pll at g5l */
static struct freq_table tgpll_ftable[4] = {
	{ .ibias = 1, .max_freq = 2000000000ULL },
	{ .ibias = 2, .max_freq = 2800000000ULL },
	{ .ibias = 3, .max_freq = 3200000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_tgpll[PLL_FACT_MAX] = {
	{ .shift = 17,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 3 },	/* icp		*/
	{ .shift = 6,	.width = 11 },	/* n		*/
	{ .shift = 32,	.width = 7 },	/* nint		*/
	{ .shift = 39,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 68,	.width = 1 },	/* postdiv	*/
};

static SPRD_PLL_WITH_ITABLE_K_FVCO(tgpll, "tgpll", "ext-26m", 0x4,
				   3, tgpll_ftable, f_tgpll, 240,
				   1000, 1000, 1, 1500000000);
static CLK_FIXED_FACTOR(tgpll_12m, "tgpll-12m", "tgpll", 128, 1, 0);
static CLK_FIXED_FACTOR(tgpll_24m, "tgpll-24m", "tgpll", 64, 1, 0);
static CLK_FIXED_FACTOR(tgpll_38m4, "tgpll-38m4", "tgpll", 40, 1, 0);
static CLK_FIXED_FACTOR(tgpll_48m, "tgpll-48m", "tgpll", 32, 1, 0);
static CLK_FIXED_FACTOR(tgpll_51m2, "tgpll-51m2", "tgpll", 30, 1, 0);
static CLK_FIXED_FACTOR(tgpll_64m, "tgpll-64m", "tgpll", 24, 1, 0);
static CLK_FIXED_FACTOR(tgpll_76m8, "tgpll-76m8", "tgpll", 20, 1, 0);
static CLK_FIXED_FACTOR(tgpll_96m, "tgpll-96m", "tgpll", 16, 1, 0);
static CLK_FIXED_FACTOR(tgpll_128m, "tgpll-128m", "tgpll", 12, 1, 0);
static CLK_FIXED_FACTOR(tgpll_153m6, "tgpll-153m6", "tgpll", 10, 1, 0);
static CLK_FIXED_FACTOR(tgpll_192m, "tgpll-192m", "tgpll", 8, 1, 0);
static CLK_FIXED_FACTOR(tgpll_256m, "tgpll-256m", "tgpll", 6, 1, 0);
static CLK_FIXED_FACTOR(tgpll_307m2, "tgpll-307m2", "tgpll", 5, 1, 0);
static CLK_FIXED_FACTOR(tgpll_384m, "tgpll-384m", "tgpll", 4, 1, 0);
static CLK_FIXED_FACTOR(tgpll_512m, "tgpll-512m", "tgpll", 3, 1, 0);
static CLK_FIXED_FACTOR(tgpll_614m4, "tgpll-614m4", "tgpll", 5, 2, 0);
static CLK_FIXED_FACTOR(tgpll_768m, "tgpll-768m", "tgpll", 2, 1, 0);

static struct clk_bit_field f_psr8pll[PLL_FACT_MAX] = {
	{ .shift = 14,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 0 },	/* div_s	*/
	{ .shift = 0,	.width = 0 },	/* mod_en	*/
	{ .shift = 0,	.width = 0 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 0,	.width = 3 },	/* icp		*/
	{ .shift = 3,	.width = 11 },	/* n		*/
	{ .shift = 0,	.width = 0 },	/* nint		*/
	{ .shift = 0,	.width = 0 },	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 35,	.width = 1 },	/* postdiv	*/
};

#define psr8pll_ftable tgpll_ftable
static SPRD_PLL_WITH_ITABLE_K_FVCO(psr8pll, "psr8pll", "psr8pll-gate", 0x2c,
				   3, psr8pll_ftable, f_psr8pll, 240,
				   1000, 1000, 1, 1600000000);

static struct clk_bit_field f_v4nrpll[PLL_FACT_MAX] = {
	{ .shift = 17,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 3 },	/* icp		*/
	{ .shift = 6,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 8 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 81,	.width = 1 },	/* postdiv	*/
};

#define v4nrpll_ftable tgpll_ftable
static SPRD_PLL_WITH_ITABLE_K_FVCO(v4nrpll, "v4nrpll", "ext-26m", 0x3c,
				   3, v4nrpll_ftable, f_v4nrpll, 240,
				   1000, 1000, 1, 1600000000);
static CLK_FIXED_FACTOR(v4nrpll_38m4, "v4nrpll-38m4", "v4nrpll", 64, 1, 0);
static CLK_FIXED_FACTOR(v4nrpll_409m6, "v4nrpll-409m6", "v4nrpll", 6, 1, 0);
static CLK_FIXED_FACTOR(v4nrpll_614m4, "v4nrpll-614m4", "v4nrpll", 4, 1, 0);
static CLK_FIXED_FACTOR(v4nrpll_819m2, "v4nrpll-819m2", "v4nrpll", 3, 1, 0);
static CLK_FIXED_FACTOR(v4nrpll_1228m8, "v4nrpll-1228m8", "v4nrpll", 2, 1, 0);

static struct sprd_clk_common *ums9620_g5l_pll_clks[] = {
	/* address base is 0x64324000 */
	&tgpll.common,
	&psr8pll.common,
	&v4nrpll.common,
};

static struct clk_hw_onecell_data ums9620_g5l_pll_hws = {
	.hws    = {
		[CLK_TGPLL]		= &tgpll.common.hw,
		[CLK_TGPLL_12M]		= &tgpll_12m.hw,
		[CLK_TGPLL_24M]		= &tgpll_24m.hw,
		[CLK_TGPLL_38M4]	= &tgpll_38m4.hw,
		[CLK_TGPLL_48M]		= &tgpll_48m.hw,
		[CLK_TGPLL_51M2]	= &tgpll_51m2.hw,
		[CLK_TGPLL_64M]		= &tgpll_64m.hw,
		[CLK_TGPLL_76M8]	= &tgpll_76m8.hw,
		[CLK_TGPLL_96M]		= &tgpll_96m.hw,
		[CLK_TGPLL_128M]	= &tgpll_128m.hw,
		[CLK_TGPLL_153M6]	= &tgpll_153m6.hw,
		[CLK_TGPLL_192M]	= &tgpll_192m.hw,
		[CLK_TGPLL_256M]	= &tgpll_256m.hw,
		[CLK_TGPLL_307M2]	= &tgpll_307m2.hw,
		[CLK_TGPLL_384M]	= &tgpll_384m.hw,
		[CLK_TGPLL_512M]	= &tgpll_512m.hw,
		[CLK_TGPLL_614M4]	= &tgpll_614m4.hw,
		[CLK_TGPLL_768M]	= &tgpll_768m.hw,
		[CLK_PSR8PLL]		= &psr8pll.common.hw,
		[CLK_V4NRPLL]		= &v4nrpll.common.hw,
		[CLK_V4NRPLL_38M4]	= &v4nrpll_38m4.hw,
		[CLK_V4NRPLL_409M6]	= &v4nrpll_409m6.hw,
		[CLK_V4NRPLL_614M4]	= &v4nrpll_614m4.hw,
		[CLK_V4NRPLL_819M2]	= &v4nrpll_819m2.hw,
		[CLK_V4NRPLL_1228M8]	= &v4nrpll_1228m8.hw,
	},
	.num    = CLK_ANLG_PHY_G5L_NUM,
};

static struct sprd_clk_desc ums9620_g5l_pll_desc = {
	.clk_clks	= ums9620_g5l_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(ums9620_g5l_pll_clks),
	.hw_clks	= &ums9620_g5l_pll_hws,
};

/* pll at g5r */
static struct freq_table gpll_ftable[4] = {
	{ .ibias = 1, .max_freq = 2000000000ULL },
	{ .ibias = 2, .max_freq = 2800000000ULL },
	{ .ibias = 3, .max_freq = 3200000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_gpll[PLL_FACT_MAX] = {
	{ .shift = 14,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 0 },	/* div_s	*/
	{ .shift = 0,	.width = 0 },	/* mod_en	*/
	{ .shift = 0,	.width = 0 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 0,	.width = 3 },	/* icp		*/
	{ .shift = 3,	.width = 11 },	/* n		*/
	{ .shift = 0,	.width = 0 },	/* nint		*/
	{ .shift = 0,	.width = 0 },	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 35,	.width = 1 },	/* postdiv	*/
};

static SPRD_PLL_WITH_ITABLE_K_FVCO(gpll, "gpll", "ext-26m", 0x0,
				   3, gpll_ftable, f_gpll, 240,
				   1000, 1000, 1, 1600000000);
static CLK_FIXED_FACTOR(gpll_680m, "gpll-680m", "gpll", 5, 2, 0);
static CLK_FIXED_FACTOR(gpll_850m, "gpll-850m", "gpll", 2, 1, 0);

#define aipll_ftable gpll_ftable
#define f_aipll f_gpll
static SPRD_PLL_WITH_ITABLE_K_FVCO(aipll, "aipll", "ext-26m", 0x18,
				   3, aipll_ftable, f_aipll, 240,
				   1000, 1000, 1, 1600000000);

#define vdsppll_ftable gpll_ftable
#define f_vdsppll f_gpll
static SPRD_PLL_WITH_ITABLE_K_FVCO(vdsppll, "vdsppll", "ext-26m", 0x30,
				   3, vdsppll_ftable, f_vdsppll, 240,
				   1000, 1000, 1, 1600000000);

static struct clk_bit_field f_cpll[PLL_FACT_MAX] = {
	{ .shift = 17,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 3 },	/* icp		*/
	{ .shift = 6,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 68,	.width = 1 },	/* postdiv	*/
};
#define cpll_ftable gpll_ftable
static SPRD_PLL_WITH_ITABLE_K_FVCO(cpll, "cpll", "ext-26m", 0x48,
				   3, cpll_ftable, f_cpll, 240,
				   1000, 1000, 1, 1600000000);

static struct clk_bit_field f_audpll[PLL_FACT_MAX] = {
	{ .shift = 17,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 1 },	/* div_s	*/
	{ .shift = 1,	.width = 1 },	/* mod_en	*/
	{ .shift = 2,	.width = 1 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 3,	.width = 3 },	/* icp		*/
	{ .shift = 6,	.width = 11 },	/* n		*/
	{ .shift = 55,	.width = 7 },	/* nint		*/
	{ .shift = 32,	.width = 23},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 67,	.width = 1 },	/* postdiv	*/
};

#define audpll_ftable gpll_ftable
static SPRD_PLL_WITH_ITABLE_K_FVCO(audpll, "audpll", "ext-26m", 0x68,
				   3, audpll_ftable, f_audpll, 240,
				   1000, 1000, 1, 1600000000);
static CLK_FIXED_FACTOR(audpll_38m4, "audpll-38m4", "audpll", 32, 1, 0);
static CLK_FIXED_FACTOR(audpll_24m57, "audpll-24m57", "audpll", 50, 1, 0);
static CLK_FIXED_FACTOR(audpll_19m2, "audpll-19m2", "audpll", 64, 1, 0);
static CLK_FIXED_FACTOR(audpll_12m28, "audpll-12m28", "audpll", 100, 1, 0);

#define phyr8pll_ftable gpll_ftable
#define f_phyr8pll f_gpll
static SPRD_PLL_WITH_ITABLE_K_FVCO(phyr8pll, "phyr8pll", "phyr8pll-gate", 0x90,
				   3, phyr8pll_ftable, f_phyr8pll, 240,
				   1000, 1000, 1, 1600000000);

#define pixelpll_ftable gpll_ftable
#define f_pixelpll f_audpll
static SPRD_PLL_WITH_ITABLE_K_FVCO(pixelpll, "pixelpll", "ext-26m", 0xa8,
				   3, pixelpll_ftable, f_pixelpll, 240,
				   1000, 1000, 1, 1600000000);
static CLK_FIXED_FACTOR(pixelpll_668m25, "pixelpll-668m25", "pixelpll", 4, 1, 0);
static CLK_FIXED_FACTOR(pixelpll_297m, "pixelpll-297m", "pixelpll", 9, 1, 0);

static struct sprd_clk_common *ums9620_g5r_pll_clks[] = {
	/* address base is 0x64320000 */
	&gpll.common,
	&aipll.common,
	&vdsppll.common,
	&cpll.common,
	&audpll.common,
	&phyr8pll.common,
	&pixelpll.common,
};

static struct clk_hw_onecell_data ums9620_g5r_pll_hws = {
	.hws    = {
		[CLK_GPLL]		= &gpll.common.hw,
		[CLK_GPLL_680M]		= &gpll_680m.hw,
		[CLK_GPLL_850M]         = &gpll_850m.hw,
		[CLK_AIPLL]		= &aipll.common.hw,
		[CLK_VDSPPLL]		= &vdsppll.common.hw,
		[CLK_CPLL]		= &cpll.common.hw,
		[CLK_AUDPLL]		= &audpll.common.hw,
		[CLK_AUDPLL_38M4]	= &audpll_38m4.hw,
		[CLK_AUDPLL_24M57]	= &audpll_24m57.hw,
		[CLK_AUDPLL_19M2]       = &audpll_19m2.hw,
		[CLK_AUDPLL_12M28]	= &audpll_12m28.hw,
		[CLK_PHYR8PLL]		= &phyr8pll.common.hw,
		[CLK_PIXELPLL]		= &pixelpll.common.hw,
		[CLK_PIXELPLL_668M25]	= &pixelpll_668m25.hw,
		[CLK_PIXELPLL_297M]	= &pixelpll_297m.hw,
	},
	.num    = CLK_ANLG_PHY_G5R_NUM,
};

static struct sprd_clk_desc ums9620_g5r_pll_desc = {
	.clk_clks	= ums9620_g5r_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(ums9620_g5r_pll_clks),
	.hw_clks	= &ums9620_g5r_pll_hws,
};

/* pll at g8 */
static struct freq_table mpllb_ftable[12] = {
	{ .ibias = 3,  .max_freq = 1200000000ULL },
	{ .ibias = 4,  .max_freq = 1400000000ULL },
	{ .ibias = 5,  .max_freq = 1600000000ULL },
	{ .ibias = 6,  .max_freq = 1800000000ULL },
	{ .ibias = 7,  .max_freq = 2000000000ULL },
	{ .ibias = 8,  .max_freq = 2200000000ULL },
	{ .ibias = 9,  .max_freq = 2400000000ULL },
	{ .ibias = 10, .max_freq = 2600000000ULL },
	{ .ibias = 11, .max_freq = 2800000000ULL },
	{ .ibias = 12, .max_freq = 3000000000ULL },
	{ .ibias = 13, .max_freq = 3200000000ULL },
	{ .ibias = INVALID_MAX_IBIAS, .max_freq = INVALID_MAX_FREQ },
};

static struct clk_bit_field f_mpllb[PLL_FACT_MAX] = {
	{ .shift = 15,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 0 },	/* div_s	*/
	{ .shift = 0,	.width = 0 },	/* mod_en	*/
	{ .shift = 0,	.width = 0 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 0,	.width = 4 },	/* icp		*/
	{ .shift = 4,	.width = 11 },	/* n		*/
	{ .shift = 0,	.width = 0 },	/* nint		*/
	{ .shift = 0,	.width = 0},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 67,	.width = 4 },	/* postdiv	*/
};

static SPRD_PLL_WITH_ITABLE_K_FVCO(mpllb, "mpllb", "ext-26m", 0x0,
				   3, mpllb_ftable, f_mpllb, 240,
				   1000, 1000, 1, 1000000000);

static struct sprd_clk_common *ums9620_g8_pll_clks[] = {
	/* address base is 0x6432c000 */
	&mpllb.common,
};

static struct clk_hw_onecell_data ums9620_g8_pll_hws = {
	.hws    = {
		[CLK_MPLLB]		= &mpllb.common.hw,
	},
	.num    = CLK_ANLG_PHY_G8_NUM,
};

static struct sprd_clk_desc ums9620_g8_pll_desc = {
	.clk_clks	= ums9620_g8_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(ums9620_g8_pll_clks),
	.hw_clks	= &ums9620_g8_pll_hws,
};

/* pll at g9 */
static struct clk_bit_field f_mpllm[PLL_FACT_MAX] = {
	{ .shift = 15,	.width = 1 },	/* lock_done	*/
	{ .shift = 0,	.width = 0 },	/* div_s	*/
	{ .shift = 0,	.width = 0 },	/* mod_en	*/
	{ .shift = 0,	.width = 0 },	/* sdm_en	*/
	{ .shift = 0,	.width = 0 },	/* refin	*/
	{ .shift = 0,	.width = 4 },	/* icp		*/
	{ .shift = 4,	.width = 11 },	/* n		*/
	{ .shift = 0,	.width = 0 },	/* nint		*/
	{ .shift = 0,	.width = 0},	/* kint		*/
	{ .shift = 0,	.width = 0 },	/* prediv	*/
	{ .shift = 67,	.width = 1 },	/* postdiv	*/
};

#define mpllm_ftable mpllb_ftable
static SPRD_PLL_WITH_ITABLE_K_FVCO(mpllm, "mpllm", "ext-26m", 0x0,
				   3, mpllm_ftable, f_mpllm, 240,
				   1000, 1000, 1, 1000000000);

static struct sprd_clk_common *ums9620_g9_pll_clks[] = {
	/* address base is 0x64330000 */
	&mpllm.common,
};

static struct clk_hw_onecell_data ums9620_g9_pll_hws = {
	.hws    = {
		[CLK_MPLLM]		= &mpllm.common.hw,
	},
	.num    = CLK_ANLG_PHY_G9_NUM,
};

static struct sprd_clk_desc ums9620_g9_pll_desc = {
	.clk_clks	= ums9620_g9_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(ums9620_g9_pll_clks),
	.hw_clks	= &ums9620_g9_pll_hws,
};

/* pll at g10 */
#define mplll_ftable mpllb_ftable
#define f_mplll f_mpllm
static SPRD_PLL_WITH_ITABLE_K_FVCO(mplll, "mplll", "ext-26m", 0x0,
				   3, mplll_ftable, f_mplll, 240,
				   1000, 1000, 1, 1000000000);

#define mplls_ftable mpllb_ftable
#define f_mplls f_mpllb
static SPRD_PLL_WITH_ITABLE_K_FVCO(mplls, "mplls", "ext-26m", 0x20,
				   3, mplls_ftable, f_mplls, 240,
				   1000, 1000, 1, 1000000000);

static struct sprd_clk_common *ums9620_g10_pll_clks[] = {
	/* address base is 0x64334000 */
	&mplll.common,
	&mplls.common,
};

static struct clk_hw_onecell_data ums9620_g10_pll_hws = {
	.hws    = {
		[CLK_MPLLL]		= &mplll.common.hw,
		[CLK_MPLLS]		= &mplls.common.hw,
	},
	.num    = CLK_ANLG_PHY_G10_NUM,
};

static struct sprd_clk_desc ums9620_g10_pll_desc = {
	.clk_clks	= ums9620_g10_pll_clks,
	.num_clk_clks	= ARRAY_SIZE(ums9620_g10_pll_clks),
	.hw_clks	= &ums9620_g10_pll_hws,
};

/* ap apb gates */
static SPRD_SC_GATE_CLK(iis0_eb, "iis0-eb", "ext-26m", 0x0,
			0x1000, BIT(1), 0, 0);
static SPRD_SC_GATE_CLK(iis1_eb, "iis1-eb", "ext-26m", 0x0,
			0x1000, BIT(2), 0, 0);
static SPRD_SC_GATE_CLK(iis2_eb, "iis2-eb", "ext-26m", 0x0,
			0x1000, BIT(3), 0, 0);
static SPRD_SC_GATE_CLK(spi0_eb, "spi0-eb", "ext-26m", 0x0,
			0x1000, BIT(5), 0, 0);
static SPRD_SC_GATE_CLK(spi1_eb, "spi1-eb", "ext-26m", 0x0,
			0x1000, BIT(6), 0, 0);
static SPRD_SC_GATE_CLK(spi2_eb, "spi2-eb", "ext-26m", 0x0,
			0x1000, BIT(7), 0, 0);
static SPRD_SC_GATE_CLK(uart3_eb, "uart3-eb", "ext-26m", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart0_eb, "uart0-eb", "ext-26m", 0x0,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart1_eb, "uart1-eb", "ext-26m", 0x0,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(uart2_eb, "uart2-eb", "ext-26m", 0x0,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi0_lfin_eb, "spi0-lfin-eb", "ext-26m", 0x0,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi1_lfin_eb, "spi1-lfin-eb", "ext-26m", 0x0,
			0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi2_lfin_eb, "spi2-lfin-eb", "ext-26m", 0x0,
			0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(spi3_lfin_eb, "spi3-lfin-eb", "ext-26m", 0x0,
			0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ce_sec_eb, "ce-sec-eb", "ext-26m", 0x0,
			0x1000, BIT(30), 0, 0);
static SPRD_SC_GATE_CLK(ce_pub_eb, "ce-pub-eb", "ext-26m", 0x0,
			0x1000, BIT(31), 0, 0);

static struct sprd_clk_common *ums9620_apapb_gate[] = {
	/* address base is 0x20100000 */
	&iis0_eb.common,
	&iis1_eb.common,
	&iis2_eb.common,
	&spi0_eb.common,
	&spi1_eb.common,
	&spi2_eb.common,
	&uart3_eb.common,
	&uart0_eb.common,
	&uart1_eb.common,
	&uart2_eb.common,
	&spi0_lfin_eb.common,
	&spi1_lfin_eb.common,
	&spi2_lfin_eb.common,
	&spi3_lfin_eb.common,
	&ce_sec_eb.common,
	&ce_pub_eb.common,
};

static struct clk_hw_onecell_data ums9620_apapb_gate_hws = {
	.hws	= {
		[CLK_IIS0_EB]		= &iis0_eb.common.hw,
		[CLK_IIS1_EB]		= &iis1_eb.common.hw,
		[CLK_IIS2_EB]		= &iis2_eb.common.hw,
		[CLK_SPI0_EB]		= &spi0_eb.common.hw,
		[CLK_SPI1_EB]		= &spi1_eb.common.hw,
		[CLK_SPI2_EB]		= &spi2_eb.common.hw,
		[CLK_UART3_EB]		= &uart3_eb.common.hw,
		[CLK_UART0_EB]		= &uart0_eb.common.hw,
		[CLK_UART1_EB]		= &uart1_eb.common.hw,
		[CLK_UART2_EB]		= &uart2_eb.common.hw,
		[CLK_SPI0_LFIN_EB]	= &spi0_lfin_eb.common.hw,
		[CLK_SPI1_LFIN_EB]	= &spi1_lfin_eb.common.hw,
		[CLK_SPI2_LFIN_EB]	= &spi2_lfin_eb.common.hw,
		[CLK_SPI3_LFIN_EB]	= &spi3_lfin_eb.common.hw,
		[CLK_CE_SEC_EB]		= &ce_sec_eb.common.hw,
		[CLK_CE_PUB_EB]		= &ce_pub_eb.common.hw,
	},
	.num	= CLK_AP_APB_GATE_NUM,
};

static struct sprd_clk_desc ums9620_apapb_gate_desc = {
	.clk_clks	= ums9620_apapb_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_apapb_gate),
	.hw_clks	= &ums9620_apapb_gate_hws,
};

/* ap ahb gates */
/* ap related gate clocks configure CLK_IGNORE_UNUSED because they are
 * configured as enabled state to support display working during uboot phase.
 * if their clocks are gated during kernel phase, it will affect the normal
 * working of display..
 */
static SPRD_SC_GATE_CLK(sdio0_eb, "sdio0-eb", "ext-26m", 0x0,
			0x1000, BIT(0), 0, 0);
static SPRD_SC_GATE_CLK(sdio1_eb, "sdio1-eb", "ext-26m", 0x0,
			0x1000, BIT(1), 0, 0);
static SPRD_SC_GATE_CLK(sdio2_eb, "sdio2-eb", "ext-26m", 0x0,
			0x1000, BIT(2), 0, 0);
static SPRD_SC_GATE_CLK(emmc_eb, "emmc-eb", "ext-26m", 0x0,
			0x1000, BIT(3), 0, 0);
static SPRD_SC_GATE_CLK(dma_pub_eb, "dma-pub-eb", "ext-26m", 0x0,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ufs_eb, "ufs-eb", "ext-26m", 0x0,
			0x1000, BIT(6), 0, 0);
static SPRD_SC_GATE_CLK(ckg_eb, "ckg-eb", "ext-26m", 0x0,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(busmon_clk_eb, "busmon-clk-eb", "ext-26m", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap2emc_eb, "ap2emc-eb", "ext-26m", 0x0,
			0x1000, BIT(9), 0, 0);
static SPRD_SC_GATE_CLK(i2c0_eb, "i2c0-eb", "ext-26m", 0x0,
			0x1000, BIT(10), 0, 0);
static SPRD_SC_GATE_CLK(i2c1_eb, "i2c1-eb", "ext-26m", 0x0,
			0x1000, BIT(11), 0, 0);
static SPRD_SC_GATE_CLK(i2c2_eb, "i2c2-eb", "ext-26m", 0x0,
			0x1000, BIT(12), 0, 0);
static SPRD_SC_GATE_CLK(i2c3_eb, "i2c3-eb", "ext-26m", 0x0,
			0x1000, BIT(13), 0, 0);
static SPRD_SC_GATE_CLK(i2c4_eb, "i2c4-eb", "ext-26m", 0x0,
			0x1000, BIT(14), 0, 0);
static SPRD_SC_GATE_CLK(i2c5_eb, "i2c5-eb", "ext-26m", 0x0,
			0x1000, BIT(15), 0, 0);
static SPRD_SC_GATE_CLK(i2c6_eb, "i2c6-eb", "ext-26m", 0x0,
			0x1000, BIT(16), 0, 0);
static SPRD_SC_GATE_CLK(i2c7_eb, "i2c7-eb", "ext-26m", 0x0,
			0x1000, BIT(17), 0, 0);
static SPRD_SC_GATE_CLK(i2c8_eb, "i2c8-eb", "ext-26m", 0x0,
			0x1000, BIT(18), 0, 0);
static SPRD_SC_GATE_CLK(i2c9_eb, "i2c9-eb", "ext-26m", 0x0,
			0x1000, BIT(19), 0, 0);
static SPRD_SC_GATE_CLK(ufs_tx_eb, "ufs-tx-eb", "ext-26m", 0x1c,
			0x1000, BIT(0), 0, 0);
static SPRD_SC_GATE_CLK(ufs_rx_0_eb, "ufs-rx-0-eb", "ext-26m", 0x1c,
			0x1000, BIT(1), 0, 0);
static SPRD_SC_GATE_CLK(ufs_rx_1_eb, "ufs-rx-1-eb", "ext-26m", 0x1c,
			0x1000, BIT(2), 0, 0);
static SPRD_SC_GATE_CLK(ufs_cfg_eb, "ufs-cfg-eb", "ext-26m", 0x1c,
			0x1000, BIT(3), 0, 0);

static struct sprd_clk_common *ums9620_apahb_gate[] = {
	/* address base is 0x20000000 */
	&sdio0_eb.common,
	&sdio1_eb.common,
	&sdio2_eb.common,
	&emmc_eb.common,
	&dma_pub_eb.common,
	&ufs_eb.common,
	&ckg_eb.common,
	&busmon_clk_eb.common,
	&ap2emc_eb.common,
	&i2c0_eb.common,
	&i2c1_eb.common,
	&i2c2_eb.common,
	&i2c3_eb.common,
	&i2c4_eb.common,
	&i2c5_eb.common,
	&i2c6_eb.common,
	&i2c7_eb.common,
	&i2c8_eb.common,
	&i2c9_eb.common,
	&ufs_tx_eb.common,
	&ufs_rx_0_eb.common,
	&ufs_rx_1_eb.common,
	&ufs_cfg_eb.common,
};

static struct clk_hw_onecell_data ums9620_apahb_gate_hws = {
	.hws	= {
		[CLK_SDIO0_EB]		= &sdio0_eb.common.hw,
		[CLK_SDIO1_EB]		= &sdio1_eb.common.hw,
		[CLK_SDIO2_EB]		= &sdio2_eb.common.hw,
		[CLK_EMMC_EB]		= &emmc_eb.common.hw,
		[CLK_DMA_PUB_EB]	= &dma_pub_eb.common.hw,
		[CLK_UFS_EB]		= &ufs_eb.common.hw,
		[CLK_CKG_EB]		= &ckg_eb.common.hw,
		[CLK_BUSMON_CLK_EB]	= &busmon_clk_eb.common.hw,
		[CLK_AP2EMC_EB]		= &ap2emc_eb.common.hw,
		[CLK_I2C0_EB]		= &i2c0_eb.common.hw,
		[CLK_I2C1_EB]		= &i2c1_eb.common.hw,
		[CLK_I2C2_EB]		= &i2c2_eb.common.hw,
		[CLK_I2C3_EB]		= &i2c3_eb.common.hw,
		[CLK_I2C4_EB]		= &i2c4_eb.common.hw,
		[CLK_I2C5_EB]		= &i2c5_eb.common.hw,
		[CLK_I2C6_EB]		= &i2c6_eb.common.hw,
		[CLK_I2C7_EB]		= &i2c7_eb.common.hw,
		[CLK_I2C8_EB]		= &i2c8_eb.common.hw,
		[CLK_I2C9_EB]		= &i2c9_eb.common.hw,
		[CLK_UFS_TX_EB]		= &ufs_tx_eb.common.hw,
		[CLK_UFS_RX_0_EB]	= &ufs_rx_0_eb.common.hw,
		[CLK_UFS_RX_1_EB]	= &ufs_rx_1_eb.common.hw,
		[CLK_UFS_CFG_EB]	= &ufs_cfg_eb.common.hw,
	},
	.num	= CLK_AP_AHB_GATE_NUM,
};

static struct sprd_clk_desc ums9620_apahb_gate_desc = {
	.clk_clks	= ums9620_apahb_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_apahb_gate),
	.hw_clks	= &ums9620_apahb_gate_hws,
};

/* ap clks */
static const char * const ap_apb_parents[] = { "ext-26m", "tgpll-64m",
					       "tgpll-96m", "tgpll-128m" };
static SPRD_MUX_CLK(ap_apb, "ap-apb", ap_apb_parents, 0x28,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const ap_axi_parents[] = { "ext-26m", "tgpll-76m8",
					       "tgpll-128m", "tgpll-256m" };
static SPRD_MUX_CLK(ap_axi, "ap-axi", ap_axi_parents, 0x34,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const ap2emc_parents[] = { "ext-26m", "tgpll-76m8",
					       "tgpll-128m", "tgpll-256m",
					       "tgpll-512m" };
static SPRD_MUX_CLK(ap2emc, "ap2emc", ap2emc_parents, 0x40,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const ap_uart_parents[] = { "ext-26m", "tgpll-48m",
						"tgpll-51m2", "tgpll-96m" };
static SPRD_COMP_CLK_OFFSET(ap_uart0, "ap-uart0", ap_uart_parents,
			    0x4c, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_uart1, "ap-uart1", ap_uart_parents,
			    0x58, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_uart2, "ap-uart2", ap_uart_parents,
			    0x64, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_uart3, "ap-uart3", ap_uart_parents,
			    0x70, 0, 2, 0, 3, 0);

static const char * const i2c_parents[] = { "ext-26m", "tgpll-48m",
					    "tgpll-51m2", "tgpll-153m6" };
static SPRD_COMP_CLK_OFFSET(ap_i2c0, "ap-i2c0", i2c_parents,
			    0x7c, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_i2c1, "ap-i2c1", i2c_parents,
			    0x88, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_i2c2, "ap-i2c2", i2c_parents,
			    0x94, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_i2c3, "ap-i2c3", i2c_parents,
			    0xa0, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_i2c4, "ap-i2c4", i2c_parents,
			    0xac, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_i2c5, "ap-i2c5", i2c_parents,
			    0xb8, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_i2c6, "ap-i2c6", i2c_parents,
			    0xc4, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_i2c7, "ap-i2c7", i2c_parents,
			    0xd0, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_i2c8, "ap-i2c8", i2c_parents,
			    0xdc, 0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_i2c9, "ap-i2c9", i2c_parents,
			    0xe8, 0, 2, 0, 3, 0);

static const char * const iis_parents[] = { "ext-26m", "tgpll-128m",
					    "tgpll-153m6", "tgpll-192m",
					    "tgpll-256m", "tgpll-512m" };
static SPRD_COMP_CLK_OFFSET(ap_iis0, "ap-iis0", iis_parents,
			    0xf4, 0, 3, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_iis1, "ap-iis1", iis_parents,
			    0x100, 0, 3, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(ap_iis2, "ap-iis2", iis_parents,
			    0x10c, 0, 3, 0, 3, 0);

static const char * const ap_ce_parents[] = { "ext-26m", "tgpll-96m",
					   "tgpll-192m", "tgpll-256m" };
static SPRD_MUX_CLK(ap_ce, "ap-ce", ap_ce_parents, 0x118,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const sdio_parents[] = { "clk-1m", "ext-26m",
					     "tgpll-307m2", "tgpll-384m",
					     "rpll-390m", "v4nrpll-409m6" };
static SPRD_COMP_CLK_OFFSET(emmc_2x, "emmc-2x", sdio_parents,
			    0x124, 0, 3, 0, 3, 0);
static SPRD_DIV_CLK(emmc_1x, "emmc-1x", "emmc-2x", 0x12c,
		    0, 1, 0);

static const char * const ufs_tx_rx_parents[] = { "ext-26m", "tgpll-128m",
						  "tgpll-256m", "tgpll-307m2" };
static SPRD_COMP_CLK_OFFSET(ufs_tx, "ufs-tx", ufs_tx_rx_parents,
			    0x13c, 0, 2, 0, 2, 0);
static SPRD_COMP_CLK_OFFSET(ufs_rx, "ufs-rx", ufs_tx_rx_parents,
			    0x148, 0, 2, 0, 2, 0);
static SPRD_COMP_CLK_OFFSET(ufs_rx_1, "ufs-rx-1", ufs_tx_rx_parents,
			    0x148, 0, 2, 0, 2, 0);

static const char * const ufs_cfg_parents[] = { "ext-26m", "tgpll-51m2" };
static SPRD_COMP_CLK_OFFSET(ufs_cfg, "ufs-cfg", ufs_cfg_parents,
			    0x160, 0, 1, 0, 6, 0);

static struct sprd_clk_common *ums9620_ap_clks[] = {
	/* address base is 0x20010000 */
	&ap_apb.common,
	&ap_axi.common,
	&ap2emc.common,
	&ap_uart0.common,
	&ap_uart1.common,
	&ap_uart2.common,
	&ap_uart3.common,
	&ap_i2c0.common,
	&ap_i2c1.common,
	&ap_i2c2.common,
	&ap_i2c3.common,
	&ap_i2c4.common,
	&ap_i2c5.common,
	&ap_i2c6.common,
	&ap_i2c7.common,
	&ap_i2c8.common,
	&ap_i2c9.common,
	&ap_iis0.common,
	&ap_iis1.common,
	&ap_iis2.common,
	&ap_ce.common,
	&emmc_2x.common,
	&emmc_1x.common,
	&ufs_tx.common,
	&ufs_rx.common,
	&ufs_rx_1.common,
	&ufs_cfg.common,
};

static struct clk_hw_onecell_data ums9620_ap_clk_hws = {
	.hws	= {
		[CLK_AP_APB]		= &ap_apb.common.hw,
		[CLK_AP_AXI]		= &ap_axi.common.hw,
		[CLK_AP2EMC]		= &ap2emc.common.hw,
		[CLK_AP_UART0]		= &ap_uart0.common.hw,
		[CLK_AP_UART1]		= &ap_uart1.common.hw,
		[CLK_AP_UART2]		= &ap_uart2.common.hw,
		[CLK_AP_UART3]		= &ap_uart3.common.hw,
		[CLK_AP_I2C0]		= &ap_i2c0.common.hw,
		[CLK_AP_I2C1]		= &ap_i2c1.common.hw,
		[CLK_AP_I2C2]		= &ap_i2c2.common.hw,
		[CLK_AP_I2C3]		= &ap_i2c3.common.hw,
		[CLK_AP_I2C4]		= &ap_i2c4.common.hw,
		[CLK_AP_I2C5]		= &ap_i2c5.common.hw,
		[CLK_AP_I2C6]		= &ap_i2c6.common.hw,
		[CLK_AP_I2C7]		= &ap_i2c7.common.hw,
		[CLK_AP_I2C8]		= &ap_i2c8.common.hw,
		[CLK_AP_I2C9]		= &ap_i2c9.common.hw,
		[CLK_AP_IIS0]		= &ap_iis0.common.hw,
		[CLK_AP_IIS1]		= &ap_iis1.common.hw,
		[CLK_AP_IIS2]		= &ap_iis2.common.hw,
		[CLK_AP_CE]		= &ap_ce.common.hw,
		[CLK_EMMC_2X]		= &emmc_2x.common.hw,
		[CLK_EMMC_1X]		= &emmc_1x.common.hw,
		[CLK_UFS_TX]		= &ufs_tx.common.hw,
		[CLK_UFS_RX]		= &ufs_rx.common.hw,
		[CLK_UFS_RX_1]		= &ufs_rx_1.common.hw,
		[CLK_UFS_CFG]		= &ufs_cfg.common.hw,
	},
	.num	= CLK_AP_CLK_NUM,
};

static struct sprd_clk_desc ums9620_ap_clk_desc = {
	.clk_clks	= ums9620_ap_clks,
	.num_clk_clks	= ARRAY_SIZE(ums9620_ap_clks),
	.hw_clks	= &ums9620_ap_clk_hws,
};

/* aon apb gates */
static SPRD_SC_GATE_CLK(rc100m_cal_eb, "rc100m-cal-eb", "ext-26m", 0x0,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rfti_eb, "rfti-eb", "ext-26m", 0x0,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(djtag_eb, "djtag-eb", "ext-26m", 0x0,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aux0_eb, "aux0-eb", "ext-26m", 0x0,
			0x1000, BIT(4), 0, 0);
static SPRD_SC_GATE_CLK(aux1_eb, "aux1-eb", "ext-26m", 0x0,
			0x1000, BIT(5), 0, 0);
static SPRD_SC_GATE_CLK(aux2_eb, "aux2-eb", "ext-26m", 0x0,
			0x1000, BIT(6), 0, 0);
static SPRD_SC_GATE_CLK(probe_eb, "probe-eb", "ext-26m", 0x0,
			0x1000, BIT(7), 0, 0);
static SPRD_SC_GATE_CLK(mm_eb, "mm-eb", "ext-26m", 0x0,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpu_eb, "gpu-eb", "ext-26m", 0x0,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mspi_eb, "mspi-eb", "ext-26m", 0x0,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ai_eb, "ai-eb", "ext-26m", 0x0,
			0x1000, BIT(13), 0, 0);
static SPRD_SC_GATE_CLK(apcpu_dap_eb, "apcpu-dap-eb", "ext-26m", 0x0,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_cssys_eb, "aon-cssys-eb", "ext-26m", 0x0,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_apb_eb, "cssys-apb-eb", "ext-26m", 0x0,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cssys_pub_eb, "cssys-pub-eb", "ext-26m", 0x0,
			0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dpu_vsp_eb, "dpu-vsp-eb", "ext-26m", 0x0,
			0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi_cfg_eb, "dsi-cfg-eb", "ext-26m", 0x0,
			0x1000, BIT(31), 0, 0);
static SPRD_SC_GATE_CLK(efuse_eb, "efuse-eb", "ext-26m", 0x4,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gpio_eb, "gpio-eb", "ext-26m", 0x4,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mbox_eb, "mbox-eb", "ext-26m", 0x4,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_eb, "kpd-eb", "ext-26m", 0x4,
			0x1000, BIT(3), 0, 0);
static SPRD_SC_GATE_CLK(aon_syst_eb, "aon-syst-eb", "ext-26m", 0x4,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_eb, "ap-syst-eb", "ext-26m", 0x4,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_eb, "aon-tmr-eb", "ext-26m", 0x4,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_dvfs_top_eb, "aon-dvfs-top-eb", "ext-26m", 0x4,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_usb2_top_eb, "aon-usb2-top-eb", "ext-26m", 0x4,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(otg_phy_eb, "otg-phy-eb", "ext-26m", 0x4,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(splk_eb, "splk-eb", "ext-26m", 0x4,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pin_eb, "pin-eb", "ext-26m", 0x4,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ana_eb, "ana-eb", "ext-26m", 0x4,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_busmon_eb,  "apcpu-busmon-eb", "ext-26m", 0x4,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ufs_ao_eb, "ufs-ao-eb", "ext-26m", 0x4,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ise_apb_eb, "ise-apb-eb", "ext-26m", 0x4,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apcpu_ts0_eb, "apcpu-ts0-eb", "ext-26m", 0x4,
			0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(apb_busmon_eb,  "apb-busmon-eb", "ext-26m", 0x4,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_iis_eb, "aon-iis-eb", "ext-26m", 0x4,
			0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(scc_eb, "scc-eb", "ext-26m", 0x4,
			0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(serdes_ctrl1_eb, "serdes-ctrl1-eb", "ext-26m", 0x4,
			0x1000, BIT(22), 0, 0);
static SPRD_SC_GATE_CLK(aux3_eb, "aux3-eb", "ext-26m", 0x4,
			0x1000, BIT(30), 0, 0);
static SPRD_SC_GATE_CLK(thm0_eb, "thm0-eb", "ext-26m", 0x8,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm1_eb, "thm1-eb", "ext-26m", 0x8,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm2_eb, "thm2-eb", "ext-26m", 0x8,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(thm3_eb, "thm3-eb", "ext-26m", 0x8,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_intc_eb, "audcp-intc-eb", "ext-26m", 0x8,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pmu_eb, "pmu-eb", "ext-26m", 0x8,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(adi_eb, "adi-eb", "ext-26m", 0x8,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_eb, "eic-eb", "ext-26m", 0x8,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc0_eb, "ap-intc0-eb", "ext-26m", 0x8,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc1_eb, "ap-intc1-eb", "ext-26m", 0x8,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc2_eb, "ap-intc2-eb", "ext-26m", 0x8,
			0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc3_eb, "ap-intc3-eb", "ext-26m", 0x8,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc4_eb, "ap-intc4-eb", "ext-26m", 0x8,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc5_eb, "ap-intc5-eb", "ext-26m", 0x8,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc6_eb, "ap-intc6-eb", "ext-26m", 0x8,
			0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_intc7_eb, "ap-intc7-eb", "ext-26m", 0x8,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pscp_intc_eb, "pscp-intc-eb", "ext-26m", 0x8,
			0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(phycp_intc_eb, "phycp-intc-eb", "ext-26m", 0x8,
			0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ise_intc_eb, "ise-intc-eb", "ext-26m", 0x8,
			0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_eb, "ap-tmr0-eb", "ext-26m", 0x8,
			0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_eb, "ap-tmr1-eb", "ext-26m", 0x8,
			0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_eb, "ap-tmr2-eb", "ext-26m", 0x8,
			0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pwm0_eb, "pwm0-eb", "ext-26m", 0x8,
			0x1000, BIT(25), 0, 0);
static SPRD_SC_GATE_CLK(pwm1_eb, "pwm1-eb", "ext-26m", 0x8,
			0x1000, BIT(26), 0, 0);
static SPRD_SC_GATE_CLK(pwm2_eb, "pwm2-eb", "ext-26m", 0x8,
			0x1000, BIT(27), 0, 0);
static SPRD_SC_GATE_CLK(pwm3_eb, "pwm3-eb", "ext-26m", 0x8,
			0x1000, BIT(28), 0, 0);
static SPRD_SC_GATE_CLK(ap_wdg_eb, "ap-wdg-eb", "ext-26m", 0x8,
			0x1000, BIT(29), 0, 0);
static SPRD_SC_GATE_CLK(apcpu_wdg_eb, "apcpu-wdg-eb", "ext-26m", 0x8,
			0x1000, BIT(30), 0, 0);
static SPRD_SC_GATE_CLK(serdes_ctrl0_eb, "serdes-ctrl0-eb", "ext-26m", 0x8,
			0x1000, BIT(31), 0, 0);
static SPRD_SC_GATE_CLK(arch_rtc_eb, "arch-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(kpd_rtc_eb, "kpd-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_syst_rtc_eb, "aon-syst-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_syst_rtc_eb, "ap-syst-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_tmr_rtc_eb, "aon-tmr-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_rtc_eb, "eic-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(eic_rtcdv5_eb, "eic-rtcdv5-eb", "ext-26m", 0x18,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_wdg_rtc_eb, "ap-wdg-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ac_wdg_rtc_eb, "ac-wdg-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr0_rtc_eb, "ap-tmr0-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr1_rtc_eb, "ap-tmr1-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ap_tmr2_rtc_eb, "ap-tmr2-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dcxo_lc_rtc_eb, "dcxo-lc-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(bb_cal_rtc_eb, "bb-cal-rtc-eb", "ext-26m", 0x18,
			0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi_csi_test_eb, "dsi-csi-test-eb", "ext-26m", 0x138,
			0x1000, BIT(8), 0, 0);
static SPRD_SC_GATE_CLK(djtag_tck_en, "djtag-tck-en", "ext-26m", 0x138,
			0x1000, BIT(9), 0, 0);
static SPRD_SC_GATE_CLK(dphy_ref_eb, "dphy-ref-eb", "ext-26m", 0x138,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dmc_ref_eb, "dmc-ref-eb", "ext-26m", 0x138,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(otg_ref_eb, "otg-ref-eb", "ext-26m", 0x138,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tsen_eb, "tsen-eb", "ext-26m", 0x138,
			0x1000, BIT(13), 0, 0);
static SPRD_SC_GATE_CLK(tmr_eb, "tmr-eb", "ext-26m", 0x138,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rc100m_ref_eb, "rc100m-ref-eb", "ext-26m", 0x138,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(rc100m_fdk_eb, "rc100m-fdk-eb", "ext-26m", 0x138,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(debounce_eb, "debounce-eb", "ext-26m", 0x138,
			0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(det_32k_eb, "det-32k-eb", "ext-26m", 0x138,
			0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(top_cssys_en, "top-cssys-en", "ext-26m", 0x13c,
			0x1000, BIT(0), 0, 0);
static SPRD_SC_GATE_CLK(ap_axi_en, "ap-axi-en", "ext-26m", 0x13c,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sdio0_2x_en, "sdio0-2x-en", "ext-26m", 0x13c,
			0x1000, BIT(2), 0, 0);
static SPRD_SC_GATE_CLK(sdio0_1x_en, "sdio0-1x-en", "ext-26m", 0x13c,
			0x1000, BIT(3), 0, 0);
static SPRD_SC_GATE_CLK(sdio1_2x_en, "sdio1-2x-en", "ext-26m", 0x13c,
			0x1000, BIT(4), 0, 0);
static SPRD_SC_GATE_CLK(sdio1_1x_en, "sdio1-1x-en", "ext-26m", 0x13c,
			0x1000, BIT(5), 0, 0);
static SPRD_SC_GATE_CLK(sdio2_2x_en, "sdio2-2x-en", "ext-26m", 0x13c,
			0x1000, BIT(6), 0, 0);
static SPRD_SC_GATE_CLK(sdio2_1x_en, "sdio2-1x-en", "ext-26m", 0x13c,
			0x1000, BIT(7), 0, 0);
static SPRD_SC_GATE_CLK(emmc_2x_en, "emmc-2x-en", "ext-26m", 0x13c,
			0x1000, BIT(8), 0, 0);
static SPRD_SC_GATE_CLK(emmc_1x_en, "emmc-1x-en", "ext-26m", 0x13c,
			0x1000, BIT(9), 0, 0);
static SPRD_SC_GATE_CLK(pll_test_en, "pll-test-en", "ext-26m", 0x13c,
			0x1000, BIT(14), 0, 0);
static SPRD_SC_GATE_CLK(cphy_cfg_en, "cphy-cfg-en", "ext-26m", 0x13c,
			0x1000, BIT(15), 0, 0);
static SPRD_SC_GATE_CLK(debug_ts_en, "debug-ts-en", "ext-26m", 0x13c,
			0x1000, BIT(18), 0, 0);
static SPRD_SC_GATE_CLK(access_aud_en, "access-aud-en", "ext-26m", 0x14c,
			0x1000, BIT(0), 0, 0);

static const char * const aux_parents[] = { "ext-32k", "phyr8pll-38m18",
					    "psr8pll-38m18", "pixelpll-41m76",
					    "cpll-31m94", "v4nrpll-38m4",
					    "lvdsrfpll-48m", "lvdsrf-rx-48m",
					    "lvdsrf-tx-48m", "rpll-26m",
					    "tgpll-48m", "mplllit-13m2",
					    "mplls-17m26", "mpllb-15m64",
					    "mpllm-13m3", "rco-150m-37m5",
					    "rco-100m-25m", "rco-60m",
					    "rco-6m", "clk-52m-a2d",
					    "clk-26m-a2d", "clk-26m-aud",
					    "gpll-53m5", "aipll-30m87",
					    "pciepll-50m", "pciepllv-26m-52m",
					    "usb31pllv-26m", "vdsppll-31m68",
					    "audpll-38m4", "audpll-19m2",
					    "audpll-12m28", "audpll-24m57" };

static SPRD_COMP_CLK(aux0_clk, "aux0-clk", aux_parents, 0x240,
		    6, 6, 0, 6, 0);
static SPRD_COMP_CLK(aux1_clk, "aux1-clk", aux_parents, 0x244,
		    6, 6, 0, 6, 0);
static SPRD_COMP_CLK(aux2_clk, "aux2-clk", aux_parents, 0x248,
		    6, 6, 0, 6, 0);
static SPRD_COMP_CLK(probe_clk, "probe-clk", aux_parents, 0x24c,
		    6, 6, 0, 6, 0);
static SPRD_COMP_CLK(aux3_clk, "aux3-clk", aux_parents, 0xd20,
		    6, 6, 0, 6, 0);

static struct sprd_clk_common *ums9620_aon_gate[] = {
	/* address base is 0x64900000 */
	&rc100m_cal_eb.common,
	&rfti_eb.common,
	&djtag_eb.common,
	&aux0_eb.common,
	&aux1_eb.common,
	&aux2_eb.common,
	&probe_eb.common,
	&mm_eb.common,
	&gpu_eb.common,
	&mspi_eb.common,
	&ai_eb.common,
	&apcpu_dap_eb.common,
	&aon_cssys_eb.common,
	&cssys_apb_eb.common,
	&cssys_pub_eb.common,
	&dpu_vsp_eb.common,
	&dsi_cfg_eb.common,
	&efuse_eb.common,
	&gpio_eb.common,
	&mbox_eb.common,
	&kpd_eb.common,
	&aon_syst_eb.common,
	&ap_syst_eb.common,
	&aon_tmr_eb.common,
	&aon_dvfs_top_eb.common,
	&aon_usb2_top_eb.common,
	&otg_phy_eb.common,
	&splk_eb.common,
	&pin_eb.common,
	&ana_eb.common,
	&apcpu_busmon_eb.common,
	&ufs_ao_eb.common,
	&ise_apb_eb.common,
	&apcpu_ts0_eb.common,
	&apb_busmon_eb.common,
	&aon_iis_eb.common,
	&scc_eb.common,
	&serdes_ctrl1_eb.common,
	&aux3_eb.common,
	&thm0_eb.common,
	&thm1_eb.common,
	&thm2_eb.common,
	&thm3_eb.common,
	&audcp_intc_eb.common,
	&pmu_eb.common,
	&adi_eb.common,
	&eic_eb.common,
	&ap_intc0_eb.common,
	&ap_intc1_eb.common,
	&ap_intc2_eb.common,
	&ap_intc3_eb.common,
	&ap_intc4_eb.common,
	&ap_intc5_eb.common,
	&ap_intc6_eb.common,
	&ap_intc7_eb.common,
	&pscp_intc_eb.common,
	&phycp_intc_eb.common,
	&ise_intc_eb.common,
	&ap_tmr0_eb.common,
	&ap_tmr1_eb.common,
	&ap_tmr2_eb.common,
	&pwm0_eb.common,
	&pwm1_eb.common,
	&pwm2_eb.common,
	&pwm3_eb.common,
	&ap_wdg_eb.common,
	&apcpu_wdg_eb.common,
	&serdes_ctrl0_eb.common,
	&arch_rtc_eb.common,
	&kpd_rtc_eb.common,
	&aon_syst_rtc_eb.common,
	&ap_syst_rtc_eb.common,
	&aon_tmr_rtc_eb.common,
	&eic_rtc_eb.common,
	&eic_rtcdv5_eb.common,
	&ap_wdg_rtc_eb.common,
	&ac_wdg_rtc_eb.common,
	&ap_tmr0_rtc_eb.common,
	&ap_tmr1_rtc_eb.common,
	&ap_tmr2_rtc_eb.common,
	&dcxo_lc_rtc_eb.common,
	&bb_cal_rtc_eb.common,
	&dsi_csi_test_eb.common,
	&djtag_tck_en.common,
	&dphy_ref_eb.common,
	&dmc_ref_eb.common,
	&otg_ref_eb.common,
	&tsen_eb.common,
	&tmr_eb.common,
	&rc100m_ref_eb.common,
	&rc100m_fdk_eb.common,
	&debounce_eb.common,
	&det_32k_eb.common,
	&top_cssys_en.common,
	&ap_axi_en.common,
	&sdio0_2x_en.common,
	&sdio0_1x_en.common,
	&sdio1_2x_en.common,
	&sdio1_1x_en.common,
	&sdio2_2x_en.common,
	&sdio2_1x_en.common,
	&emmc_2x_en.common,
	&emmc_1x_en.common,
	&pll_test_en.common,
	&cphy_cfg_en.common,
	&debug_ts_en.common,
	&access_aud_en.common,
	&aux0_clk.common,
	&aux1_clk.common,
	&aux2_clk.common,
	&probe_clk.common,
	&aux3_clk.common,
};

static struct clk_hw_onecell_data ums9620_aon_gate_hws = {
	.hws	= {
		[CLK_RC100M_CAL_EB]	= &rc100m_cal_eb.common.hw,
		[CLK_RFTI_EB]		= &rfti_eb.common.hw,
		[CLK_DJTAG_EB]		= &djtag_eb.common.hw,
		[CLK_AUX0_EB]		= &aux0_eb.common.hw,
		[CLK_AUX1_EB]		= &aux1_eb.common.hw,
		[CLK_AUX2_EB]		= &aux2_eb.common.hw,
		[CLK_PROBE_EB]		= &probe_eb.common.hw,
		[CLK_MM_EB]		= &mm_eb.common.hw,
		[CLK_GPU_EB]		= &gpu_eb.common.hw,
		[CLK_MSPI_EB]		= &mspi_eb.common.hw,
		[CLK_AI_EB]		= &ai_eb.common.hw,
		[CLK_APCPU_DAP_EB]	= &apcpu_dap_eb.common.hw,
		[CLK_AON_CSSYS_EB]	= &aon_cssys_eb.common.hw,
		[CLK_CSSYS_APB_EB]	= &cssys_apb_eb.common.hw,
		[CLK_CSSYS_PUB_EB]	= &cssys_pub_eb.common.hw,
		[CLK_DPU_VSP_EB]	= &dpu_vsp_eb.common.hw,
		[CLK_DSI_CFG_EB]	= &dsi_cfg_eb.common.hw,
		[CLK_EFUSE_EB]		= &efuse_eb.common.hw,
		[CLK_GPIO_EB]		= &gpio_eb.common.hw,
		[CLK_MBOX_EB]		= &mbox_eb.common.hw,
		[CLK_KPD_EB]		= &kpd_eb.common.hw,
		[CLK_AON_SYST_EB]	= &aon_syst_eb.common.hw,
		[CLK_AP_SYST_EB]	= &ap_syst_eb.common.hw,
		[CLK_AON_TMR_EB]	= &aon_tmr_eb.common.hw,
		[CLK_AON_DVFS_TOP_EB]	= &aon_dvfs_top_eb.common.hw,
		[CLK_AON_USB2_TOP_EB]	= &aon_usb2_top_eb.common.hw,
		[CLK_OTG_PHY_EB]	= &otg_phy_eb.common.hw,
		[CLK_SPLK_EB]		= &splk_eb.common.hw,
		[CLK_PIN_EB]		= &pin_eb.common.hw,
		[CLK_ANA_EB]		= &ana_eb.common.hw,
		[CLK_APCPU_BUSMON_EB]	= &apcpu_busmon_eb.common.hw,
		[CLK_UFS_AO_EB]		= &ufs_ao_eb.common.hw,
		[CLK_ISE_APB_EB]	= &ise_apb_eb.common.hw,
		[CLK_APCPU_TS0_EB]	= &apcpu_ts0_eb.common.hw,
		[CLK_APB_BUSMON_EB]	= &apb_busmon_eb.common.hw,
		[CLK_AON_IIS_EB]	= &aon_iis_eb.common.hw,
		[CLK_SCC_EB]		= &scc_eb.common.hw,
		[CLK_SERDES_CTRL1_EB]   = &serdes_ctrl1_eb.common.hw,
		[CLK_AUX3_EB]		= &aux3_eb.common.hw,
		[CLK_THM0_EB]		= &thm0_eb.common.hw,
		[CLK_THM1_EB]		= &thm1_eb.common.hw,
		[CLK_THM2_EB]		= &thm2_eb.common.hw,
		[CLK_THM3_EB]		= &thm3_eb.common.hw,
		[CLK_AUDCP_INTC_EB]	= &audcp_intc_eb.common.hw,
		[CLK_PMU_EB]		= &pmu_eb.common.hw,
		[CLK_ADI_EB]		= &adi_eb.common.hw,
		[CLK_EIC_EB]		= &eic_eb.common.hw,
		[CLK_AP_INTC0_EB]	= &ap_intc0_eb.common.hw,
		[CLK_AP_INTC1_EB]	= &ap_intc1_eb.common.hw,
		[CLK_AP_INTC2_EB]	= &ap_intc2_eb.common.hw,
		[CLK_AP_INTC3_EB]	= &ap_intc3_eb.common.hw,
		[CLK_AP_INTC4_EB]	= &ap_intc4_eb.common.hw,
		[CLK_AP_INTC5_EB]	= &ap_intc5_eb.common.hw,
		[CLK_AP_INTC6_EB]	= &ap_intc6_eb.common.hw,
		[CLK_AP_INTC7_EB]	= &ap_intc7_eb.common.hw,
		[CLK_PSCP_INTC_EB]	= &pscp_intc_eb.common.hw,
		[CLK_PHYCP_INTC_EB]	= &phycp_intc_eb.common.hw,
		[CLK_ISE_INTC_EB]	= &ise_intc_eb.common.hw,
		[CLK_AP_TMR0_EB]	= &ap_tmr0_eb.common.hw,
		[CLK_AP_TMR1_EB]	= &ap_tmr1_eb.common.hw,
		[CLK_AP_TMR2_EB]	= &ap_tmr2_eb.common.hw,
		[CLK_PWM0_EB]		= &pwm0_eb.common.hw,
		[CLK_PWM1_EB]		= &pwm1_eb.common.hw,
		[CLK_PWM2_EB]		= &pwm2_eb.common.hw,
		[CLK_PWM3_EB]		= &pwm3_eb.common.hw,
		[CLK_AP_WDG_EB]		= &ap_wdg_eb.common.hw,
		[CLK_APCPU_WDG_EB]	= &apcpu_wdg_eb.common.hw,
		[CLK_SERDES_CTRL0_EB]	= &serdes_ctrl0_eb.common.hw,
		[CLK_ARCH_RTC_EB]	= &arch_rtc_eb.common.hw,
		[CLK_KPD_RTC_EB]	= &kpd_rtc_eb.common.hw,
		[CLK_AON_SYST_RTC_EB]	= &aon_syst_rtc_eb.common.hw,
		[CLK_AP_SYST_RTC_EB]	= &ap_syst_rtc_eb.common.hw,
		[CLK_AON_TMR_RTC_EB]	= &aon_tmr_rtc_eb.common.hw,
		[CLK_EIC_RTC_EB]	= &eic_rtc_eb.common.hw,
		[CLK_EIC_RTCDV5_EB]	= &eic_rtcdv5_eb.common.hw,
		[CLK_AP_WDG_RTC_EB]	= &ap_wdg_rtc_eb.common.hw,
		[CLK_AC_WDG_RTC_EB]	= &ac_wdg_rtc_eb.common.hw,
		[CLK_AP_TMR0_RTC_EB]	= &ap_tmr0_rtc_eb.common.hw,
		[CLK_AP_TMR1_RTC_EB]	= &ap_tmr1_rtc_eb.common.hw,
		[CLK_AP_TMR2_RTC_EB]	= &ap_tmr2_rtc_eb.common.hw,
		[CLK_DCXO_LC_RTC_EB]	= &dcxo_lc_rtc_eb.common.hw,
		[CLK_BB_CAL_RTC_EB]	= &bb_cal_rtc_eb.common.hw,
		[CLK_DSI_CSI_TEST_EB]	= &dsi_csi_test_eb.common.hw,
		[CLK_DJTAG_TCK_EN]	= &djtag_tck_en.common.hw,
		[CLK_DPHY_REF_EB]	= &dphy_ref_eb.common.hw,
		[CLK_DMC_REF_EB]	= &dmc_ref_eb.common.hw,
		[CLK_OTG_REF_EB]	= &otg_ref_eb.common.hw,
		[CLK_TSEN_EB]		= &tsen_eb.common.hw,
		[CLK_TMR_EB]		= &tmr_eb.common.hw,
		[CLK_RC100M_REF_EB]	= &rc100m_ref_eb.common.hw,
		[CLK_RC100M_FDK_EB]	= &rc100m_fdk_eb.common.hw,
		[CLK_DEBOUNCE_EB]	= &debounce_eb.common.hw,
		[CLK_DET_32K_EB]	= &det_32k_eb.common.hw,
		[CLK_TOP_CSSYS_EB]	= &top_cssys_en.common.hw,
		[CLK_AP_AXI_EN]		= &ap_axi_en.common.hw,
		[CLK_SDIO0_2X_EN]	= &sdio0_2x_en.common.hw,
		[CLK_SDIO0_1X_EN]	= &sdio0_1x_en.common.hw,
		[CLK_SDIO1_2X_EN]	= &sdio1_2x_en.common.hw,
		[CLK_SDIO1_1X_EN]	= &sdio1_1x_en.common.hw,
		[CLK_SDIO2_2X_EN]	= &sdio2_2x_en.common.hw,
		[CLK_SDIO2_1X_EN]	= &sdio2_1x_en.common.hw,
		[CLK_EMMC_2X_EN]	= &emmc_2x_en.common.hw,
		[CLK_EMMC_1X_EN]	= &emmc_1x_en.common.hw,
		[CLK_PLL_TEST_EN]	= &pll_test_en.common.hw,
		[CLK_CPHY_CFG_EN]	= &cphy_cfg_en.common.hw,
		[CLK_DEBUG_TS_EN]	= &debug_ts_en.common.hw,
		[CLK_ACCESS_AUD_EN]	= &access_aud_en.common.hw,
		[CLK_AUX0]		= &aux0_clk.common.hw,
		[CLK_AUX1]		= &aux1_clk.common.hw,
		[CLK_AUX2]		= &aux2_clk.common.hw,
		[CLK_PROBE]		= &probe_clk.common.hw,
		[CLK_AUX3]		= &aux3_clk.common.hw,
	},
	.num	= CLK_AON_APB_GATE_NUM,
};

static struct sprd_clk_desc ums9620_aon_gate_desc = {
	.clk_clks	= ums9620_aon_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_aon_gate),
	.hw_clks	= &ums9620_aon_gate_hws,
};

/* aon apb clks */
static const char * const aon_apb_parents[] = { "rco-100m-4m", "clk-4m3",
						"clk-13m", "rco-100m-25m",
						"ext-26m", "rco-100m",
						"tgpll-128m", "tgpll-153m6" };
static SPRD_COMP_CLK_OFFSET(aon_apb, "aon-apb", aon_apb_parents, 0x28,
			    0, 3, 0, 2, 0);

static const char * const adi_parents[] = { "rco-100m-4m", "ext-26m",
					    "rco-100m-25m", "tgpll-38m4",
					    "tgpll-51m2"};
static SPRD_MUX_CLK(adi, "adi", adi_parents, 0x34,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const pwm_parents[] = { "ext-32k", "ext-26m",
					    "rco-100m-4m", "rco-100m-25m",
					    "tgpll-48m" };
static SPRD_MUX_CLK(pwm0, "pwm0", pwm_parents, 0x40,
		    0, 3, UMS9620_MUX_FLAG);
static SPRD_MUX_CLK(pwm1, "pwm1", pwm_parents, 0x4c,
		    0, 3, UMS9620_MUX_FLAG);
static SPRD_MUX_CLK(pwm2, "pwm2", pwm_parents, 0x58,
		    0, 3, UMS9620_MUX_FLAG);
static SPRD_MUX_CLK(pwm3, "pwm3", pwm_parents, 0x64,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const efuse_parents[] = { "rco-100m-25m", "ext-26m" };
static SPRD_MUX_CLK(efuse, "efuse", efuse_parents, 0x70,
		    0, 1, UMS9620_MUX_FLAG);

static const char * const uart_parents[] = { "rco-100m-4m", "ext-26m",
					     "tgpll-48m", "tgpll-51m2",
					     "tgpll-96m", "rco-100m",
					     "tgpll-128m" };
static SPRD_MUX_CLK(uart0, "uart0", uart_parents, 0x7c,
		    0, 3, UMS9620_MUX_FLAG);
static SPRD_MUX_CLK(uart1, "uart1", uart_parents, 0x88,
		    0, 3, UMS9620_MUX_FLAG);
static SPRD_MUX_CLK(uart2, "uart2", uart_parents, 0x94,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const thm_parents[] = { "ext-32k", "clk-250k" };
static SPRD_MUX_CLK(thm0, "thm0", thm_parents, 0xc4,
		    0, 1, UMS9620_MUX_FLAG);
static SPRD_MUX_CLK(thm1, "thm1", thm_parents, 0xd0,
		    0, 1, UMS9620_MUX_FLAG);
static SPRD_MUX_CLK(thm2, "thm2", thm_parents, 0xdc,
		    0, 1, UMS9620_MUX_FLAG);
static SPRD_MUX_CLK(thm3, "thm3", thm_parents, 0xe8,
		    0, 1, UMS9620_MUX_FLAG);

static const char * const aon_iis_parents[] = { "ext-26m", "tgpll-128m",
						"tgpll-153m6" };
static SPRD_MUX_CLK(aon_iis, "aon-iis", aon_iis_parents, 0x118,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const scc_parents[] = { "ext-26m", "tgpll-48m",
					    "tgpll-51m2", "tgpll-96m" };
static SPRD_MUX_CLK(scc, "scc", scc_parents, 0x124,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const apcpu_dap_parents[] = { "ext-26m", "rco-100m-4m",
						  "tgpll-76m8", "rco-100m",
						  "tgpll-128m", "tgpll-153m6" };
static SPRD_MUX_CLK(apcpu_dap, "apcpu-dap", apcpu_dap_parents, 0x130,
		    0, 3, UMS9620_MUX_FLAG);


static const char * const apcpu_ts_parents[] = { "ext-32k", "ext-26m",
						 "tgpll-128m", "tgpll-153m6" };
static SPRD_MUX_CLK(apcpu_ts, "apcpu-ts", apcpu_ts_parents, 0x13c,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const debug_ts_parents[] = { "ext-26m", "tgpll-76m8",
						 "tgpll-128m", "tgpll-192m" };
static SPRD_MUX_CLK(debug_ts, "debug-ts", debug_ts_parents, 0x148,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const pri_sbi_parents[] = { "ext-26m", "tgpll-96m" };
static SPRD_MUX_CLK(pri_sbi, "pri-sbi", pri_sbi_parents, 0x154,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const xo_sel_parents[] = { "ext-26m" };
static SPRD_MUX_CLK(xo_sel, "xo-sel", xo_sel_parents, 0x160,
		    0, 1, UMS9620_MUX_FLAG);

static const char * const rfti_lth_parents[] = { "ext-26m" };
static SPRD_MUX_CLK(rfti_lth, "rfti-lth", rfti_lth_parents, 0x16c,
		    0, 1, UMS9620_MUX_FLAG);

static const char * const afc_lth_parents[] = { "ext-26m" };
static SPRD_MUX_CLK(afc_lth, "afc-lth", afc_lth_parents, 0x178,
		    0, 1, UMS9620_MUX_FLAG);

static SPRD_DIV_CLK(rco100m_fdk, "rco100m-fdk", "rco-100m", 0x18c,
		    0, 6, 0);

static SPRD_DIV_CLK(rco60m_fdk, "rco60m-fdk", "rco-60m", 0x1a4,
		    0, 5, 0);
static const char * const rco6m_ref_parents[] = { "clk-16k", "clk-2m" };
static SPRD_COMP_CLK_OFFSET(rco6m_ref, "rco6m-ref", rco6m_ref_parents, 0x1b4,
			    0, 1, 0, 5, 0);
static SPRD_DIV_CLK(rco6m_fdk, "rco6m-fdk", "rco-6m", 0x1bc,
		    0, 9, 0);

static const char * const djtag_tck_parents[] = { "rco-100m-4m", "ext-26m" };
static SPRD_MUX_CLK(djtag_tck, "djtag-tck", djtag_tck_parents, 0x1cc,
		    0, 1, UMS9620_MUX_FLAG);

static const char * const aon_tmr_parents[] = { "rco-100m-4m", "rco-100m-25m",
						"ext-26m" };
static SPRD_MUX_CLK(aon_tmr, "aon-tmr", aon_tmr_parents, 0x1f0,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const aon_pmu_parents[] = { "ext-32k", "rco-100m-4m",
						"clk-4m3", "rco-60m-4m" };
static SPRD_MUX_CLK(aon_pmu, "aon-pmu", aon_pmu_parents, 0x208,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const debounce_parents[] = { "ext-32k", "rco-100m-4m",
						 "rco-100m-25m", "ext-26m",
						 "rco-60m-4m", "rco-60m-20m"};
static SPRD_MUX_CLK(debounce, "debounce", debounce_parents, 0x214,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const apcpu_pmu_parents[] = { "ext-26m", "tgpll-76m8",
						  "rco-60m", "rco-100m",
						  "tgpll-128m" };
static SPRD_MUX_CLK(apcpu_pmu, "apcpu-pmu", apcpu_pmu_parents, 0x220,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const top_dvfs_parents[] = { "ext-26m", "tgpll-96m",
						"rco-100m", "tgpll-128m" };
static SPRD_MUX_CLK(top_dvfs, "top-dvfs", top_dvfs_parents, 0x22c,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const pmu_26m_parents[] = { "rco-100m-20m", "ext-26m",
						"rco-60m-20m" };
static SPRD_MUX_CLK(pmu_26m, "pmu-26m", aon_pmu_parents, 0x238,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const tzpc_parents[] = { "rco-100m-4m", "clk-4m3",
					     "clk-13m", "rco-100m-25m",
					     "ext-26m", "rco-100m",
					     "tgpll-128m" };
static SPRD_COMP_CLK_OFFSET(tzpc, "tzpc", tzpc_parents, 0x244,
			    0, 3, 0, 2, 0);

static const char * const otg_ref_parents[] = { "tgpll-12m", "ext-26m" };
static SPRD_MUX_CLK(otg_ref, "otg-ref", otg_ref_parents, 0x250,
		    0, 1, UMS9620_MUX_FLAG);

static const char * const cssys_parents[] = { "rco-100m-25m", "ext-26m",
					      "rco-100m", "tgpll-153m6",
					      "tgpll-384m", "tgpll-512m" };
static SPRD_COMP_CLK_OFFSET(cssys, "cssys", cssys_parents, 0x25c,
			    0, 3, 0, 2, 0);
static SPRD_DIV_CLK(cssys_apb, "cssys-apb", "cssys", 0x264,
		    0, 3, 0);

static const char * const sdio_2x_parents[] = { "clk-1m", "ext-26m",
						 "tgpll-307m2", "tgpll-384m",
						 "rpll-390m", "v4nrpll-409m6" };
static SPRD_COMP_CLK_OFFSET(sdio0_2x, "sdio0-2x", sdio_2x_parents, 0x274,
		    0, 3, 0, 11, 0);
static SPRD_DIV_CLK(sdio0_1x, "sdio0-1x", "sdio0-2x", 0x27c,
		    0, 1, 0);

static SPRD_COMP_CLK_OFFSET(sdio1_2x, "sdio1-2x", sdio_2x_parents, 0x28c,
			    0, 3, 0, 11, 0);
static SPRD_DIV_CLK(sdio1_1x, "sdio1-1x", "sdio1-2x", 0x294,
		    0, 1, 0);

static SPRD_COMP_CLK_OFFSET(sdio2_2x, "sdio2-2x", sdio_2x_parents, 0x2a4,
			    0, 3, 0, 11, 0);
static SPRD_DIV_CLK(sdio2_1x, "sdio2-1x", "sdio2-2x", 0x2ac,
		    0, 1, 0);

static const char * const spi_parents[] = { "ext-26m", "tgpll-128m",
					    "tgpll-153m6", "tgpll-192m" };
static SPRD_COMP_CLK_OFFSET(spi0, "spi0", spi_parents, 0x2bc,
			    0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(spi1, "spi1", spi_parents, 0x2c8,
			    0, 2, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(spi2, "spi2", spi_parents, 0x2d4,
			    0, 2, 0, 3, 0);


static const char * const analog_io_apb_parents[] = { "ext-26m", "tgpll-48m" };
static SPRD_COMP_CLK_OFFSET(analog_io_apb, "analog-io-apb",
			    analog_io_apb_parents, 0x2e0, 0, 1, 8, 2, 0);

static const char * const dmc_ref_parents[] = { "clk-6m5", "clk-13m",
						"ext-26m" };
static SPRD_MUX_CLK(dmc_ref, "dmc-ref", dmc_ref_parents, 0x2ec,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const usb_parents[] = { "rco-100m-25m", "ext-26m",
					    "tgpll-76m8", "tgpll-96m",
					    "rco-100m", "tgpll-128m" };
static SPRD_COMP_CLK_OFFSET(usb, "usb", usb_parents, 0x2f8,
			    0, 3, 8, 2, 0);

static const char * const usb_suspend_parents[] = { "ext-32k", "clk-1m" };
static SPRD_MUX_CLK(usb_suspend, "usb-suspend", usb_suspend_parents, 0x31c,
		    0, 1, UMS9620_MUX_FLAG);

static const char * const ufs_aon_parents[] = { "ext-26m", "tgpll-48m",
						"rco-100m", "tgpll-128m",
						"tgpll-192m", "tgpll-256m" };
static SPRD_MUX_CLK(ufs_aon, "ufs-aon", ufs_aon_parents, 0x328,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const ufs_pck_parents[] = { "ext-26m", "tgpll-76m8",
						"tgpll-128m", "tgpll-153m6",
						"tgpll-192m", "tgpll-256m" };
static SPRD_MUX_CLK(ufs_pck, "ufs-pck", ufs_pck_parents, 0x334,
		    0, 3, UMS9620_MUX_FLAG);

static struct sprd_clk_common *ums9620_aonapb_clk[] = {
	/* address base is 0x64920000 */
	&aon_apb.common,
	&adi.common,
	&pwm0.common,
	&pwm1.common,
	&pwm2.common,
	&pwm3.common,
	&efuse.common,
	&uart0.common,
	&uart1.common,
	&uart2.common,
	&thm0.common,
	&thm1.common,
	&thm2.common,
	&thm3.common,
	&aon_iis.common,
	&scc.common,
	&apcpu_dap.common,
	&apcpu_ts.common,
	&debug_ts.common,
	&pri_sbi.common,
	&xo_sel.common,
	&rfti_lth.common,
	&afc_lth.common,
	&rco100m_fdk.common,
	&rco60m_fdk.common,
	&rco6m_ref.common,
	&rco6m_fdk.common,
	&djtag_tck.common,
	&aon_tmr.common,
	&aon_pmu.common,
	&debounce.common,
	&apcpu_pmu.common,
	&top_dvfs.common,
	&pmu_26m.common,
	&tzpc.common,
	&otg_ref.common,
	&cssys.common,
	&cssys_apb.common,
	&sdio0_2x.common,
	&sdio0_1x.common,
	&sdio1_2x.common,
	&sdio1_1x.common,
	&sdio2_2x.common,
	&sdio2_1x.common,
	&spi0.common,
	&spi1.common,
	&spi2.common,
	&analog_io_apb.common,
	&dmc_ref.common,
	&usb.common,
	&usb_suspend.common,
	&ufs_aon.common,
	&ufs_pck.common,
};

static struct clk_hw_onecell_data ums9620_aonapb_clk_hws = {
	.hws	= {
		[CLK_AON_APB]		= &aon_apb.common.hw,
		[CLK_ADI]		= &adi.common.hw,
		[CLK_PWM0]		= &pwm0.common.hw,
		[CLK_PWM1]		= &pwm1.common.hw,
		[CLK_PWM2]		= &pwm2.common.hw,
		[CLK_PWM3]		= &pwm3.common.hw,
		[CLK_EFUSE]		= &efuse.common.hw,
		[CLK_UART0]		= &uart0.common.hw,
		[CLK_UART1]		= &uart1.common.hw,
		[CLK_UART2]		= &uart2.common.hw,
		[CLK_THM0]		= &thm0.common.hw,
		[CLK_THM1]		= &thm1.common.hw,
		[CLK_THM2]		= &thm2.common.hw,
		[CLK_THM3]		= &thm3.common.hw,
		[CLK_AON_IIS]		= &aon_iis.common.hw,
		[CLK_SCC]		= &scc.common.hw,
		[CLK_APCPU_DAP]		= &apcpu_dap.common.hw,
		[CLK_APCPU_TS]		= &apcpu_ts.common.hw,
		[CLK_DEBUG_TS]		= &debug_ts.common.hw,
		[CLK_PRI_SBI]		= &pri_sbi.common.hw,
		[CLK_XO_SEL]		= &xo_sel.common.hw,
		[CLK_RFTI_LTH]		= &rfti_lth.common.hw,
		[CLK_AFC_LTH]		= &afc_lth.common.hw,
		[CLK_RCO100M_FDK]	= &rco100m_fdk.common.hw,
		[CLK_RCO60M_FDK]	= &rco60m_fdk.common.hw,
		[CLK_RCO6M_REF]		= &rco6m_ref.common.hw,
		[CLK_RCO6M_FDK]		= &rco6m_fdk.common.hw,
		[CLK_DJTAG_TCK]		= &djtag_tck.common.hw,
		[CLK_AON_TMR]		= &aon_tmr.common.hw,
		[CLK_AON_PMU]		= &aon_pmu.common.hw,
		[CLK_DEBOUNCE]		= &debounce.common.hw,
		[CLK_APCPU_PMU]		= &apcpu_pmu.common.hw,
		[CLK_TOP_DVFS]		= &top_dvfs.common.hw,
		[CLK_PMU_26M]		= &pmu_26m.common.hw,
		[CLK_TZPC]		= &tzpc.common.hw,
		[CLK_OTG_REF]		= &otg_ref.common.hw,
		[CLK_CSSYS]		= &cssys.common.hw,
		[CLK_CSSYS_APB]		= &cssys_apb.common.hw,
		[CLK_SDIO0_2X]		= &sdio0_2x.common.hw,
		[CLK_SDIO0_1X]		= &sdio0_1x.common.hw,
		[CLK_SDIO1_2X]		= &sdio1_2x.common.hw,
		[CLK_SDIO1_1X]		= &sdio1_1x.common.hw,
		[CLK_SDIO2_2X]		= &sdio2_2x.common.hw,
		[CLK_SDIO2_1X]		= &sdio2_1x.common.hw,
		[CLK_SPI0]		= &spi0.common.hw,
		[CLK_SPI1]		= &spi1.common.hw,
		[CLK_SPI2]		= &spi2.common.hw,
		[CLK_ANALOG_IO_APB]	= &analog_io_apb.common.hw,
		[CLK_DMC_REF]		= &dmc_ref.common.hw,
		[CLK_USB]		= &usb.common.hw,
		[CLK_USB_SUSPEND]	= &usb_suspend.common.hw,
		[CLK_UFS_AON]		= &ufs_aon.common.hw,
		[CLK_UFS_PCK]		= &ufs_pck.common.hw,
	},
	.num	= CLK_AON_APB_NUM,
};

static struct sprd_clk_desc ums9620_aonapb_clk_desc = {
	.clk_clks	= ums9620_aonapb_clk,
	.num_clk_clks	= ARRAY_SIZE(ums9620_aonapb_clk),
	.hw_clks	= &ums9620_aonapb_clk_hws,
};

/* top dvfs clk */
static const char * const lit_core_parents[] = { "ext-26m", "v4nrpll-614m4",
						 "tgpll-768m", "v4nrpll-1228m8",
						 "mplll" };
static SPRD_COMP_CLK(core0, "core0", lit_core_parents, 0xe08,
		     0, 3, 3, 1, 0);
static SPRD_COMP_CLK(core1, "core1", lit_core_parents, 0xe08,
		     4, 3, 7, 1, 0);
static SPRD_COMP_CLK(core2, "core2", lit_core_parents, 0xe08,
		     8, 3, 11, 1, 0);
static SPRD_COMP_CLK(core3, "core3", lit_core_parents, 0xe08,
		     12, 3, 15, 1, 0);

static const char * const mid_core_parents[] = { "ext-26m", "tgpll-768m",
						 "v4nrpll-1228m8",
						 "tgpll-1536m",
						 "mpllm" };
static SPRD_COMP_CLK(core4, "core4", mid_core_parents, 0xe08,
		     16, 3, 19, 1, 0);
static SPRD_COMP_CLK(core5, "core5", mid_core_parents, 0xe08,
		     20, 3, 23, 1, 0);
static SPRD_COMP_CLK(core6, "core6", lit_core_parents, 0xe08,
		     24, 3, 27, 1, 0);

static const char * const big_core_parents[] = { "ext-26m", "v4nrpll-1228m8",
						 "tgpll-1536m", "mpllb" };
static SPRD_COMP_CLK(core7, "core7", big_core_parents, 0xe08,
		     28, 3, 31, 1, 0);

static const char * const scu_parents[] = { "ext-26m", "v4nrpll-614m4",
					    "tgpll-768m", "v4nrpll-1228m8",
					    "mplls" };
static SPRD_COMP_CLK(scu, "scu", scu_parents, 0xe0c,
		     0, 3, 3, 1, 0);
static SPRD_DIV_CLK(ace, "ace", "scu", 0xe0c,
		    24, 1, 0);

static const char * const atb_parents[] = { "ext-26m", "tgpll-153m6",
					    "tgpll-512m", "tgpll-768m" };
static SPRD_COMP_CLK(atb, "atb", atb_parents, 0xe0c,
		     4, 2, 6, 3, 0);
static SPRD_DIV_CLK(debug_apb, "debug_apb", "atb", 0xe0c,
		    25, 2, 0);

static const char * const cps_parents[] = { "ext-26m", "tgpll-153m6",
					    "tgpll-512m", "tgpll-768m" };
static SPRD_COMP_CLK(cps, "cps", cps_parents, 0xe0c,
		     9, 2, 11, 3, 0);

static const char * const gic_parents[] = { "ext-26m", "tgpll-153m6",
					    "tgpll-512m", "tgpll-768m" };
static SPRD_COMP_CLK(gic, "gic", gic_parents, 0xe0c,
		     14, 2, 16, 3, 0);

static const char * const periph_parents[] = { "ext-26m", "tgpll-153m6",
					       "tgpll-512m", "tgpll-768m" };
static SPRD_COMP_CLK(periph, "periph", periph_parents, 0xe0c,
		     19, 2, 21, 3, 0);

static struct sprd_clk_common *ums9620_topdvfs_clk[] = {
	/* address base is 0x64940000 */
	&core0.common,
	&core1.common,
	&core2.common,
	&core3.common,
	&core4.common,
	&core5.common,
	&core6.common,
	&core7.common,
	&scu.common,
	&ace.common,
	&atb.common,
	&debug_apb.common,
	&cps.common,
	&gic.common,
	&periph.common,
};

static struct clk_hw_onecell_data ums9620_topdvfs_clk_hws = {
	.hws    = {
		[CLK_CORE0]		= &core0.common.hw,
		[CLK_CORE1]		= &core1.common.hw,
		[CLK_CORE2]		= &core2.common.hw,
		[CLK_CORE3]		= &core3.common.hw,
		[CLK_CORE4]		= &core4.common.hw,
		[CLK_CORE5]		= &core5.common.hw,
		[CLK_CORE6]		= &core6.common.hw,
		[CLK_CORE7]		= &core7.common.hw,
		[CLK_SCU]		= &scu.common.hw,
		[CLK_ACE]		= &ace.common.hw,
		[CLK_ATB]		= &atb.common.hw,
		[CLK_DEBUG_APB]		= &debug_apb.common.hw,
		[CLK_CPS]		= &cps.common.hw,
		[CLK_GIC]		= &gic.common.hw,
		[CLK_PERIPH]		= &periph.common.hw,
	},
	.num    = CLK_TOPDVFS_CLK_NUM,
};

static struct sprd_clk_desc ums9620_topdvfs_clk_desc = {
	.clk_clks	= ums9620_topdvfs_clk,
	.num_clk_clks	= ARRAY_SIZE(ums9620_topdvfs_clk),
	.hw_clks	= &ums9620_topdvfs_clk_hws,
};

/* gpu apb gate */
static SPRD_SC_GATE_CLK(gpu_core_eb, "gpu-core-eb", "gpu-eb", 0x0,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *ums9620_gpuapb_gate[] = {
	/* address base is 0x23000000 */
	&gpu_core_eb.common,
};

static struct clk_hw_onecell_data ums9620_gpuapb_gate_hws = {
	.hws    = {
		[CLK_GPU_CORE_EB]	= &gpu_core_eb.common.hw,
	},
	.num    = CLK_GPU_APB_GATE_NUM,
};

static struct sprd_clk_desc ums9620_gpuapb_gate_desc = {
	.clk_clks	= ums9620_gpuapb_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_gpuapb_gate),
	.hw_clks	= &ums9620_gpuapb_gate_hws,
};

/* gpu clocks */
static const char * const gpu_parents[] = { "ext-26m", "tgpll-76m8",
					    "tgpll-153m6", "tgpll-384m",
					    "tgpll-512m", "gpll-680m",
					    "gpll-850m" };
static SPRD_COMP_CLK_OFFSET(gpu, "gpu", gpu_parents, 0x28,
			    0, 3, 0, 3, 0);

static const char * const ap_mm_parents[] = { "ext-26m", "tgpll-76m8",
					      "tgpll-153m6" };

static SPRD_MUX_CLK(ap_mm, "ap-mm", ap_mm_parents, 0x40,
		    0, 2, UMS9620_MUX_FLAG);


static struct sprd_clk_common *ums9620_gpu_clk[] = {
	/* address base is 0x23010000 */
	&gpu.common,
	&ap_mm.common,
};

static struct clk_hw_onecell_data ums9620_gpu_clk_hws = {
	.hws	= {
		[CLK_GPU]		= &gpu.common.hw,
		[CLK_AP_MM]		= &ap_mm.common.hw,
	},
	.num	= CLK_GPU_CLK_NUM,
};

static struct sprd_clk_desc ums9620_gpu_clk_desc = {
	.clk_clks	= ums9620_gpu_clk,
	.num_clk_clks	= ARRAY_SIZE(ums9620_gpu_clk),
	.hw_clks	= &ums9620_gpu_clk_hws,
};

/* ipa apb gate clocks */
/* ipa apb related gate clocks configure CLK_IGNORE_UNUSED because their
 * power domain may be shut down, and they are controlled by related module.
 */
static SPRD_SC_GATE_CLK(usb_eb, "usb-eb", "ext-26m", 0x4,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(usb_suspend_eb, "usb-suspend-eb", "ext-26m", 0x4,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(usb_ref_eb, "usb-ref-eb", "ext-26m", 0x4,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pam_usb_eb, "pam-usb-eb", "ext-26m", 0x4,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pam_wifi_eb, "pam-wifi-eb", "ext-26m", 0x4,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *ums9620_ipaapb_gate[] = {
	/* address base is 0x25000000 */
	&usb_eb.common,
	&usb_suspend_eb.common,
	&usb_ref_eb.common,
	&pam_usb_eb.common,
	&pam_wifi_eb.common,
};

static struct clk_hw_onecell_data ums9620_ipaapb_gate_hws = {
	.hws	= {
		[CLK_USB_EB]		= &usb_eb.common.hw,
		[CLK_USB_SUSPEND_EB]	= &usb_suspend_eb.common.hw,
		[CLK_USB_REF_EB]	= &usb_ref_eb.common.hw,
		[CLK_PAM_USB_EB]	= &pam_usb_eb.common.hw,
		[CLK_PAM_WIFI_EB]	= &pam_wifi_eb.common.hw,
	},
	.num	= CLK_IPAAPB_GATE_NUM,
};

static struct sprd_clk_desc ums9620_ipaapb_gate_desc = {
	.clk_clks	= ums9620_ipaapb_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_ipaapb_gate),
	.hw_clks	= &ums9620_ipaapb_gate_hws,
};

/* ipa clocks*/
static const char * const ipa_axi_parents[] = { "ext-26m", "tgpll-192m",
						"tgpll-384m", "v4nrpll-409m6" };
static SPRD_MUX_CLK(ipa_axi, "ipa-axi", ipa_axi_parents, 0x28,
		    0, 2, UMS9620_MUX_FLAG);
static SPRD_DIV_CLK(ipa_apb, "ipa-apb", "ipa-axi", 0x30,
		    0, 2, 0);

static const char * const usb_ref_parents[] = { "ext-32k", "tgpll-24m" };
static SPRD_MUX_CLK(usb_ref, "usb-ref", usb_ref_parents, 0x4c,
		    0, 1, UMS9620_MUX_FLAG);

static const char * const ap_ipa_axi_parents[] = { "ext-26m", "tgpll-512m" };
static SPRD_MUX_CLK(ap_ipa_axi, "ap-ipa-axi", ap_ipa_axi_parents, 0x64,
		    0, 1, UMS9620_MUX_FLAG);
static SPRD_DIV_CLK(ap_ipa_apb, "ap-ipa-apb", "ap-ipa-axi", 0x6c,
		    0, 2, 0);

static const char * const ipa_dpu_parents[] = { "tgpll-192m", "tgpll-256m",
						"tgpll-384m", "tgpll-512m" };
static SPRD_MUX_CLK(ipa_dpu, "ipa-dpu", ipa_dpu_parents, 0x7c,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const ipa_dpi_parents[] = { "tgpll-128m", "tgpll-192m",
						"pixelpll", "tgpll-307m2" };
static SPRD_COMP_CLK_OFFSET(ipa_dpi, "ipa-dpi", ipa_dpi_parents, 0x88,
			    0, 2, 0, 4, 0);

static SPRD_DIV_CLK(usb_153m6, "usb-153m6", "tgpll-153m6", 0xb4,
		    0, 1, 0);

static struct sprd_clk_common *ums9620_ipa_clk[] = {
	/* address base is 0x25010000 */
	&ipa_axi.common,
	&ipa_apb.common,
	&usb_ref.common,
	&ap_ipa_axi.common,
	&ap_ipa_apb.common,
	&ipa_dpu.common,
	&ipa_dpi.common,
	&usb_153m6.common,
};

static struct clk_hw_onecell_data ums9620_ipa_clk_hws = {
	.hws	= {
		[CLK_IPA_AXI]		= &ipa_axi.common.hw,
		[CLK_IPA_APB]		= &ipa_apb.common.hw,
		[CLK_USB_REF]		= &usb_ref.common.hw,
		[CLK_AP_IPA_AXI]	= &ap_ipa_axi.common.hw,
		[CLK_AP_IPA_APB]	= &ap_ipa_apb.common.hw,
		[CLK_IPA_DPU]		= &ipa_dpu.common.hw,
		[CLK_IPA_DPI]		= &ipa_dpi.common.hw,
		[CLK_USB_153M6]		= &usb_153m6.common.hw,
	},
	.num	= CLK_IPA_CLK_NUM,
};

static struct sprd_clk_desc ums9620_ipa_clk_desc = {
	.clk_clks	= ums9620_ipa_clk,
	.num_clk_clks	= ARRAY_SIZE(ums9620_ipa_clk),
	.hw_clks	= &ums9620_ipa_clk_hws,
};

/* ipa glb gate clocks*/
/* ipa glb related gate clocks configure CLK_IGNORE_UNUSED because their
 * power domain may be shut down, and they are controlled by related module.
 */
static SPRD_SC_GATE_CLK(ipa_eb, "ipa-eb", "ext-26m", 0x4,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(tft_eb, "tft-eb", "ext-26m", 0x4,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_access_phycp_en, "ipa-access-phycp-en", "ext-26m",
			0x8, 0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_access_pscp_en, "ipa-access-pscp-en", "ext-26m",
			0x8, 0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *ums9620_ipaglb_gate[] = {
	/* address base is 0x25240000 */
	&ipa_eb.common,
	&tft_eb.common,
	&ipa_access_phycp_en.common,
	&ipa_access_pscp_en.common,
};

static struct clk_hw_onecell_data ums9620_ipaglb_gate_hws = {
	.hws	= {
		[CLK_IPA_EB]			= &ipa_eb.common.hw,
		[CLK_TFT_EB]			= &tft_eb.common.hw,
		[CLK_IPA_ACCESS_PHYCP_EN]	= &ipa_access_phycp_en.common.hw,
		[CLK_IPA_ACCESS_PSCP_EN]	= &ipa_access_pscp_en.common.hw,
	},
	.num	= CLK_IPAGLB_GATE_NUM,
};

static struct sprd_clk_desc ums9620_ipaglb_gate_desc = {
	.clk_clks	= ums9620_ipaglb_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_ipaglb_gate),
	.hw_clks	= &ums9620_ipaglb_gate_hws,
};

/* ipa dispc1 glb gate clocks */
static SPRD_SC_GATE_CLK(ipa_dpu1_eb, "ipa-dup1-eb", "ext-26m", 0x0,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_dptx_eb, "ipa-dptx-eb", "ext-26m", 0x0,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_hdcp_eb, "ipa-hdcp-eb", "ext-26m", 0x0,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_tca_eb, "ipa-tca-eb", "ext-26m", 0x0,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_usb31pll_eb, "ipa-usb31pll-eb", "ext-26m", 0x0,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_ckg_eb, "ipa-ckg-eb", "ext-26m", 0x0,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_trng_eb, "ipa-trng-eb", "ext-26m", 0x0,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *ums9620_ipadispcglb_gate[] = {
	/* address base is 0x31800000 */
	&ipa_dpu1_eb.common,
	&ipa_dptx_eb.common,
	&ipa_hdcp_eb.common,
	&ipa_tca_eb.common,
	&ipa_usb31pll_eb.common,
	&ipa_ckg_eb.common,
	&ipa_trng_eb.common,
};

static struct clk_hw_onecell_data ums9620_ipadispcglb_gate_hws = {
	.hws	= {
		[CLK_IPA_DPU1_EB]	= &ipa_dpu1_eb.common.hw,
		[CLK_IPA_DPTX_EB]	= &ipa_dptx_eb.common.hw,
		[CLK_IPA_HDCP_EB]	= &ipa_hdcp_eb.common.hw,
		[CLK_IPA_TCA_EB]	= &ipa_tca_eb.common.hw,
		[CLK_IPA_USB31PLL_EB]	= &ipa_usb31pll_eb.common.hw,
		[CLK_IPA_CKG_EB]	= &ipa_ckg_eb.common.hw,
		[CLK_IPA_TRNG_EB]	= &ipa_trng_eb.common.hw,
	},
	.num	= CLK_IPADISPC_GATE_NUM,
};

static struct sprd_clk_desc ums9620_ipadispcglb_gate_desc = {
	.clk_clks	= ums9620_ipadispcglb_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_ipadispcglb_gate),
	.hw_clks	= &ums9620_ipadispcglb_gate_hws,
};

/* pcie apb gate clocks */
/* pcie related gate clocks configure CLK_IGNORE_UNUSED because their power
 * domain may be shut down, and they are controlled by related module.
 */
static SPRD_SC_GATE_CLK(pcie3_aux_eb, "pcie3-aux-eb", "ext-26m", 0x4,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pcie3_eb, "pcie3-eb", "ext-26m", 0x4,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nic400_tranmon_eb, "nic400-tranmon-eb", "ext-26m",
			0x4, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nic400_cfg_eb, "nic400-cfg-eb", "ext-26m", 0x4,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(pcie3_phy_eb, "pcie3-phy-eb", "ext-26m", 0x4,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *ums9620_pcieapb_gate[] = {
	/* address base is 0x26000000 */
	&pcie3_aux_eb.common,
	&pcie3_eb.common,
	&nic400_tranmon_eb.common,
	&nic400_cfg_eb.common,
	&pcie3_phy_eb.common,
};

static struct clk_hw_onecell_data ums9620_pcieapb_gate_hws = {
	.hws	= {
		[CLK_PCIE3_AUX_EB]		= &pcie3_aux_eb.common.hw,
		[CLK_PCIE3_EB]			= &pcie3_eb.common.hw,
		[CLK_NIC400_TRANMON_EB]		= &nic400_tranmon_eb.common.hw,
		[CLK_NIC400_CFG_EB]		= &nic400_cfg_eb.common.hw,
		[CLK_PCIE3_PHY]			= &pcie3_phy_eb.common.hw,
	},
	.num	= CLK_PCIEAPB_GATE_NUM,
};

static struct sprd_clk_desc ums9620_pcieapb_gate_desc = {
	.clk_clks	= ums9620_pcieapb_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_pcieapb_gate),
	.hw_clks	= &ums9620_pcieapb_gate_hws,
};

/* pcie clocks*/
static const char * const pcie_axi_parents[] = { "ext-26m", "tgpll-192m",
						 "tgpll-384m", "v4nrpll-409m6" };
static SPRD_MUX_CLK(pcie_axi, "pcie-axi", pcie_axi_parents, 0x20,
		    0, 2, CLK_SET_RATE_NO_REPARENT);
static SPRD_DIV_CLK(pcie_apb, "pcie-apb", "pcie-axi", 0x24,
		    0, 2, 0);

static const char * const pcie_aux_parents[] = { "clk-2m", "rco-100m-2m" };
static SPRD_MUX_CLK(pcie_aux, "pcie-aux", pcie_aux_parents, 0x28,
		    0, 1, UMS9620_MUX_FLAG);

static struct sprd_clk_common *ums9620_pcie_clk[] = {
	/* address base is 0x26004000 */
	&pcie_axi.common,
	&pcie_apb.common,
	&pcie_aux.common,
};

static struct clk_hw_onecell_data ums9620_pcie_clk_hws = {
	.hws	= {
		[CLK_PCIE_AXI]		= &pcie_axi.common.hw,
		[CLK_PCIE_APB]		= &pcie_apb.common.hw,
		[CLK_PCIE_AUX]		= &pcie_aux.common.hw,
	},
	.num	= CLK_PCIE_CLK_NUM,
};

static struct sprd_clk_desc ums9620_pcie_clk_desc = {
	.clk_clks	= ums9620_pcie_clk,
	.num_clk_clks	= ARRAY_SIZE(ums9620_pcie_clk),
	.hw_clks	= &ums9620_pcie_clk_hws,
};

/* ai apb gate clocks */
/* ai related gate clocks configure CLK_IGNORE_UNUSED because their power
 * domain may be shut down, and they are controlled by related module.
 */
static SPRD_SC_GATE_CLK(powervr_eb, "powervr-eb", "ai-eb", 0x0,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mtx_apbreg_eb, "mtx-apbreg-eb", "ai-eb", 0x0,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ai_dvfs_eb, "ai-dvfs-eb", "ai-eb", 0x0,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ocm_eb, "ocm-eb", "ai-eb", 0x0,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(axi_pmon_eb, "axi-pmon-eb", "ai-eb", 0x0,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(aon_to_ocm_eb, "aon-to-ocm-eb", "ai-eb", 0x0,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(nic400_busmon_eb, "nic400-busmon-eb", "ai-eb", 0x0,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *ums9620_aiapb_gate[] = {
	/* address base is 0x27000000 */
	&powervr_eb.common,
	&mtx_apbreg_eb.common,
	&ai_dvfs_eb.common,
	&ocm_eb.common,
	&axi_pmon_eb.common,
	&aon_to_ocm_eb.common,
	&nic400_busmon_eb.common,
};

static struct clk_hw_onecell_data ums9620_aiapb_gate_hws = {
	.hws	= {
		[CLK_POWERVR_EB]	= &powervr_eb.common.hw,
		[CLK_MTX_APBREG_EB]	= &mtx_apbreg_eb.common.hw,
		[CLK_AI_DVFS_EB]	= &ai_dvfs_eb.common.hw,
		[CLK_OCM_EB]		= &ocm_eb.common.hw,
		[CLK_AXI_PMON_EB]	= &axi_pmon_eb.common.hw,
		[CLK_AON_TO_OCM_EB]	= &aon_to_ocm_eb.common.hw,
		[CLK_NIC400_BUSMON_EB]	= &nic400_busmon_eb.common.hw,
	},
	.num	= CLK_AIAPB_GATE_NUM,
};

static struct sprd_clk_desc ums9620_aiapb_gate_desc = {
	.clk_clks	= ums9620_aiapb_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_aiapb_gate),
	.hw_clks	= &ums9620_aiapb_gate_hws,
};

/* ai clocks */
static const char * const ai_cfg_mtx_parents[] = { "tgpll-48m", "tgpll-96m",
						   "tgpll-128m", "tgpll-153m6" };
static SPRD_COMP_CLK_OFFSET(ai_cfg_mtx, "ai-cfg-mtx", ai_cfg_mtx_parents, 0x40,
			    0, 2, 0, 3, 0);

static const char * const ai_dvfs_parents[] = { "tgpll-48m", "tgpll-96m",
						"tgpll-128m", "tgpll-153m6" };
static SPRD_COMP_CLK_OFFSET(ai_dvfs, "ai-dvfs", ai_dvfs_parents, 0x58,
			    0, 2, 0, 3, 0);

static struct sprd_clk_common *ums9620_ai_clk[] = {
	/* address base is 0x27004000 */
	&ai_cfg_mtx.common,
	&ai_dvfs.common,
};

static struct clk_hw_onecell_data ums9620_ai_clk_hws = {
	.hws	= {
		[CLK_AI_CFG_MTX]	= &ai_cfg_mtx.common.hw,
		[CLK_AI_DVFS]		= &ai_dvfs.common.hw,
	},
	.num	= CLK_AI_CLK_NUM,
};

static struct sprd_clk_desc ums9620_ai_clk_desc = {
	.clk_clks	= ums9620_ai_clk,
	.num_clk_clks	= ARRAY_SIZE(ums9620_ai_clk),
	.hw_clks	= &ums9620_ai_clk_hws,
};

/* ai dvfs clocks */
static const char * const powervr_parents[] = { "tgpll-512m", "v4nrpll-614m4",
						"tgpll-768m", "aipll"};
static SPRD_COMP_CLK(powervr, "powervr", powervr_parents, 0xa58,
		     0, 2, 2, 3, 0);

static const char * const ai_main_mtx_parents[] = { "tgpll-512m", "v4nrpll-614m4",
						     "tgpll-768m", "aipll"};
static SPRD_COMP_CLK(ai_main_mtx, "ai-main-mtx", ai_main_mtx_parents, 0xa58,
		     5, 2, 7, 3, 0);

static const char * const ocm_parents[] = { "tgpll-153m6", "tgpll-512m",
					    "tgpll-768m", "aipll"};
static SPRD_COMP_CLK(ocm, "ocm", ocm_parents, 0xa58,
		     10, 2, 12, 3, 0);

static struct sprd_clk_common *ums9620_ai_dvfs_clk[] = {
	/* address base is 0x27008000 */
	&powervr.common,
	&ai_main_mtx.common,
	&ocm.common,
};

static struct clk_hw_onecell_data ums9620_ai_dvfs_clk_hws = {
	.hws	= {
		[CLK_POWERVR]		= &powervr.common.hw,
		[CLK_AI_MAIN_MTX]	= &ai_main_mtx.common.hw,
		[CLK_OCM]		= &ocm.common.hw,
	},
	.num	= CLK_AI_DVFS_CLK_NUM,
};

static struct sprd_clk_desc ums9620_ai_dvfs_clk_desc = {
	.clk_clks	= ums9620_ai_dvfs_clk,
	.num_clk_clks	= ARRAY_SIZE(ums9620_ai_dvfs_clk),
	.hw_clks	= &ums9620_ai_dvfs_clk_hws,
};

/* mm ahb gate clocks */
/* mm related gate clocks configure CLK_IGNORE_UNUSED because their power
 * domain may be shut down, and they are controlled by related module.
 */
static SPRD_SC_GATE_CLK(jpg_en, "jpg-en", "mm-eb", 0x0,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ckg_en, "ckg-en", "mm-eb", 0x0,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mailbox_en, "mailbox-en", "mm-eb", 0x0,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dvfs_en, "dvfs-en", "mm-eb", 0x0,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sys_mtx_cfg_en, "sys_mtx-cfg-en", "mm-eb", 0x0,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sys_cfg_mtx_busmon_en, "sys-cfg-mtx-busmon-en", "mm-eb",
			0x0, 0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sys_mst_busmon_en, "sys-mst-busmon-en", "mm-eb",
			0x0, 0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sys_tck_en, "sys-tck-en", "mm-eb", 0x0,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_mtx_data_en, "mm-mtx-data-en", "mm-eb", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(isp_en, "isp-en", "mm-eb", 0x4,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(cpp_en, "cpp-en", "mm-eb", 0x4,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(depth_en, "depth-en", "mm-eb", 0x4,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(fd_en, "fd-em", "mm-eb", 0x4,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dewarp_en, "dewarp-en", "mm-eb", 0x4,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(isp_mtx_en, "isp-mtx-en", "mm-eb", 0x4,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(isp_blk_cfg_en, "isp-blk-cfg-en", "mm-eb", 0x4,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(isp_blk_en, "isp-blk-en", "mm-eb", 0x4,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dwp_clk_gate_en, "dwp-clk-gate-en", "mm-eb", 0x4,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(isp_blk_mst_busmon_en, "isp-blk-mst-busmon-en", "mm-eb",
			0x4, 0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(isp_tck_en, "isp-tck-en", "mm-eb", 0x4,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vdsp_en, "vdsp-en", "mm-eb", 0x8,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vdsp_m_en, "vdsp-m-en", "mm-eb", 0x8,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vdma_en, "vdma-en", "mm-eb", 0x8,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vdsp_tck_en, "vdsp-tck-en", "mm-eb", 0x8,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vdsp_mtx_data_en, "vdsp-mtx-data-en", "mm-eb", 0x8,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vdsp_blk_cfg_en, "vdsp-blk-cfg-en", "mm-eb", 0x8,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(mm_uart_en, "mm-uart-en", "mm-eb", 0x8,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vdsp_blk_en, "vdsp-blk-en", "mm-eb", 0x8,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vdsp_slv_busmon_en, "vdsp-slv-busmon-en", "mm-eb",
			0x8, 0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vdsp_mst_busmon_en, "vdsp-mst-busmon-en", "mm-eb",
			0x8, 0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(idvau_en, "idvau-en", "mm-eb", 0x8,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dvau_en, "dvau-en", "mm-eb", 0x8,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ivau_en, "ivau-en", "mm-eb", 0x8,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dcam_if_en, "dcam-if-en", "mm-eb", 0xc,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dcam_if_lite_en, "dcam-if-lite-en", "mm-eb", 0xc,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(phy_cfg_en, "phy-cfg-en", "mm-eb", 0xc,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dcam_mtx_en, "dcam-mtx-en", "mm-eb", 0xc,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dcam_lite_mtx_en, "dcam-lite-mtx-en", "mm-eb", 0xc,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dcam_blk_cfg_en, "dcam-blk-cfg-en", "mm-eb", 0xc,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sensor0_en, "sensor0-en", "mm-eb", 0xc,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sensor1_en, "sensor1-en", "mm-eb", 0xc,
			0x1000, BIT(7), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sensor2_en, "sensor2-en", "mm-eb", 0xc,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(sensor3_en, "sensor3-en", "mm-eb", 0xc,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dcam_blk_en, "dcam-blk-en", "mm-eb", 0xc,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dcam_tck_en, "dcam-tck-en", "mm-eb", 0xc,
			0x1000, BIT(11), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(csi0_en, "csi0-en", "mm-eb", 0xc,
			0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(csi1_en, "csi1-en", "mm-eb", 0xc,
			0x1000, BIT(13), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(csi2_en, "csi2-en", "mm-eb", 0xc,
			0x1000, BIT(14), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(csi3_en, "csi3-en", "mm-eb", 0xc,
			0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(ipa_en, "ipa-en", "mm-eb", 0xc,
			0x1000, BIT(16), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *ums9620_mm_gate[] = {
	/* address base is 0x30000000 */
	&jpg_en.common,
	&ckg_en.common,
	&mailbox_en.common,
	&dvfs_en.common,
	&sys_mtx_cfg_en.common,
	&sys_cfg_mtx_busmon_en.common,
	&sys_mst_busmon_en.common,
	&sys_tck_en.common,
	&mm_mtx_data_en.common,
	&isp_en.common,
	&cpp_en.common,
	&depth_en.common,
	&fd_en.common,
	&dewarp_en.common,
	&isp_mtx_en.common,
	&isp_blk_cfg_en.common,
	&isp_blk_en.common,
	&dwp_clk_gate_en.common,
	&isp_blk_mst_busmon_en.common,
	&isp_tck_en.common,
	&vdsp_en.common,
	&vdsp_m_en.common,
	&vdma_en.common,
	&vdsp_tck_en.common,
	&vdsp_mtx_data_en.common,
	&vdsp_blk_cfg_en.common,
	&mm_uart_en.common,
	&vdsp_blk_en.common,
	&vdsp_slv_busmon_en.common,
	&vdsp_mst_busmon_en.common,
	&idvau_en.common,
	&dvau_en.common,
	&ivau_en.common,
	&dcam_if_en.common,
	&dcam_if_lite_en.common,
	&phy_cfg_en.common,
	&dcam_mtx_en.common,
	&dcam_lite_mtx_en.common,
	&dcam_blk_cfg_en.common,
	&sensor0_en.common,
	&sensor1_en.common,
	&sensor2_en.common,
	&sensor3_en.common,
	&dcam_blk_en.common,
	&dcam_tck_en.common,
	&csi0_en.common,
	&csi1_en.common,
	&csi2_en.common,
	&csi3_en.common,
	&ipa_en.common,

};

static struct clk_hw_onecell_data ums9620_mm_gate_hws = {
	.hws	= {
		[CLK_JPG_EN]			= &jpg_en.common.hw,
		[CLK_CKG_EN]			= &ckg_en.common.hw,
		[CLK_MAILBOX_EN]		= &mailbox_en.common.hw,
		[CLK_DVFS_EN]			= &dvfs_en.common.hw,
		[CLK_SYS_MTX_CFG_EN]		= &sys_mtx_cfg_en.common.hw,
		[CLK_SYS_CFG_MTX_BUSMON_EN]	= &sys_cfg_mtx_busmon_en.common.hw,
		[CLK_SYS_MST_BUSMON_EN]		= &sys_mst_busmon_en.common.hw,
		[CLK_SYS_TCK_EN]		= &sys_tck_en.common.hw,
		[CLK_MM_MTX_DATA_EN]		= &mm_mtx_data_en.common.hw,
		[CLK_ISP_EN]			= &isp_en.common.hw,
		[CLK_CPP_EN]			= &cpp_en.common.hw,
		[CLK_DEPTH_EN]			= &depth_en.common.hw,
		[CLK_FD_EN]			= &fd_en.common.hw,
		[CLK_DEWARP_EN]			= &dewarp_en.common.hw,
		[CLK_ISP_MTX_EN]		= &isp_mtx_en.common.hw,
		[CLK_ISP_BLK_CFG_EN]		= &isp_blk_cfg_en.common.hw,
		[CLK_ISP_BLK_EN]		= &isp_blk_en.common.hw,
		[CLK_DWP_CLK_GATE_EN]		= &dwp_clk_gate_en.common.hw,
		[CLK_ISP_BLK_MST_BUSMON_EN]	= &isp_blk_mst_busmon_en.common.hw,
		[CLK_ISP_TCK_EN]		= &isp_tck_en.common.hw,
		[CLK_VDSP_EN]			= &vdsp_en.common.hw,
		[CLK_VDSP_M_EN]			= &vdsp_m_en.common.hw,
		[CLK_VDMA_EN]			= &vdma_en.common.hw,
		[CLK_VDSP_TCK_EN]		= &vdsp_tck_en.common.hw,
		[CLK_VDSP_MTX_DATA_EN]		= &vdsp_mtx_data_en.common.hw,
		[CLK_VDSP_BLK_CFG_EN]		= &vdsp_blk_cfg_en.common.hw,
		[CLK_MM_UART_EN]		= &mm_uart_en.common.hw,
		[CLK_VDSP_BLK_EN]		= &vdsp_blk_en.common.hw,
		[CLK_VDSP_SLV_BUSMON_EN]	= &vdsp_slv_busmon_en.common.hw,
		[CLK_VDSP_MST_BUSMON_EN]	= &vdsp_mst_busmon_en.common.hw,
		[CLK_IDVAU_EN]			= &idvau_en.common.hw,
		[CLK_DVAU_EN]			= &dvau_en.common.hw,
		[CLK_IVAU_EN]			= &ivau_en.common.hw,
		[CLK_DCAM_IF_EN]		= &dcam_if_en.common.hw,
		[CLK_DCAM_IF_LITE_EN]		= &dcam_if_lite_en.common.hw,
		[CLK_PHY_CFG_EN]		= &phy_cfg_en.common.hw,
		[CLK_DCAM_MTX_EN]		= &dcam_mtx_en.common.hw,
		[CLK_DCAM_LITE_MTX_EN]		= &dcam_lite_mtx_en.common.hw,
		[CLK_DCAM_BLK_CFG_EN]		= &dcam_blk_cfg_en.common.hw,
		[CLK_SENSOR0_EN]		= &sensor0_en.common.hw,
		[CLK_SENSOR1_EN]		= &sensor1_en.common.hw,
		[CLK_SENSOR2_EN]		= &sensor2_en.common.hw,
		[CLK_SENSOR3_EN]		= &sensor3_en.common.hw,
		[CLK_DCAM_BLK_EN]		= &dcam_blk_en.common.hw,
		[CLK_DCAM_TCK_EN]		= &dcam_tck_en.common.hw,
		[CLK_CSI0_EN]			= &csi0_en.common.hw,
		[CLK_CSI1_EN]			= &csi1_en.common.hw,
		[CLK_CSI2_EN]			= &csi2_en.common.hw,
		[CLK_CSI3_EN]			= &csi3_en.common.hw,
		[CLK_IPA_EN]			= &ipa_en.common.hw,
	},
	.num	= CLK_MM_GATE_NUM,
};

static struct sprd_clk_desc ums9620_mm_gate_desc = {
	.clk_clks	= ums9620_mm_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_mm_gate),
	.hw_clks	= &ums9620_mm_gate_hws,
};

/* mm clocks */
static const char * const vdsp_parents[] = { "ext-26m", "tgpll-307m2",
					     "tgpll-512m", "tgpll-614m4",
					     "v4nrpll-819m2", "vdsppll"};
static SPRD_MUX_CLK(vdsp, "vdsp", vdsp_parents, 0x28,
		    0, 2, CLK_SET_RATE_NO_REPARENT);
static SPRD_DIV_CLK(vdsp_m, "vdsp-m", "vdsp", 0x30,
		    0, 2, 0);

static const char * const vdma_parents[] = { "ext-26m", "tgpll-153m6",
					     "tgpll-256m", "tgpll-307m2",
					     "v4nrpll-409m6", "tgpll-512m"};
static SPRD_MUX_CLK(vdma, "vdma", vdma_parents, 0x40,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const vdsp_mtx_data_parents[] = { "tgpll-153m6",
						      "tgpll-307m2",
						      "v4nrpll-409m6",
						      "tgpll-512m" };
static SPRD_MUX_CLK(vdsp_mtx_data, "vdsp-mtx-data", vdsp_mtx_data_parents,
		    0x58, 0, 2, UMS9620_MUX_FLAG);

static const char * const vdsp_blk_cfg_parents[] = { "ext-26m", "tgpll-48m",
						     "tgpll-64m", "tgpll-96m" };
static SPRD_MUX_CLK(vdsp_blk_cfg, "vdsp-blk-cfg", vdsp_blk_cfg_parents,
		    0x64, 0, 2, UMS9620_MUX_FLAG);

static const char * const mm_uart_parents[] = { "ext-26m", "tgpll-48m",
						"tgpll-51m2", "tgpll-96m" };
static SPRD_MUX_CLK(mm_uart, "mm-uart", mm_uart_parents, 0x70,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const isp_parents[] = { "tgpll-153m6", "tgpll-256m",
					    "tgpll-307m2", "v4nrpll-409m6",
					    "tgpll-512m"};
static SPRD_MUX_CLK(isp, "isp", isp_parents, 0x7c,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const cpp_parents[] = { "tgpll-128m", "tgpll-192m",
					    "tgpll-256m", "tgpll-307m2",
					    "tgpll-384m"};
static SPRD_MUX_CLK(cpp, "cpp", cpp_parents, 0x88,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const depth_parents[] = { "tgpll-128m", "tgpll-192m",
					      "tgpll-256m", "tgpll-307m2",
					      "tgpll-384m"};
static SPRD_MUX_CLK(depth, "depth", cpp_parents, 0xa0,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const fd_parents[] = { "tgpll-128m", "tgpll-192m",
					   "tgpll-256m", "tgpll-307m2",
					    "tgpll-384m" };
static SPRD_MUX_CLK(fd, "fd", fd_parents, 0xac,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const dcam0_1_parents[] = { "tgpll-153m6", "tgpll-256m",
						"tgpll-307m2", "v4nrpll-409m6",
						"tgpll-512m" };
static SPRD_MUX_CLK(dcam0_1, "dcam0-1", dcam0_1_parents, 0xb8,
		    0, 3, UMS9620_MUX_FLAG);
static SPRD_MUX_CLK(dcam0_1_axi, "dcam0-1-axi", dcam0_1_parents, 0xc4,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const dcam2_3_parents[] = { "tgpll-96m", "tgpll-128m",
						"tgpll-153m6", "tgpll-192m",
						"tgpll-256m" };
static SPRD_MUX_CLK(dcam2_3, "dcam2-3", dcam2_3_parents, 0xdc,
		    0, 3, UMS9620_MUX_FLAG);
static SPRD_MUX_CLK(dcam2_3_axi, "dcam2-3-axi", dcam2_3_parents, 0xe8,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const mipi_csi0_parents[] = { "tgpll-256m", "tgpll-384m",
						  "v4nrpll-409m6",
						  "tgpll-614m4",
						  "tgpll-768m" };
static SPRD_MUX_CLK(mipi_csi0, "mipi-csi0", mipi_csi0_parents, 0x100,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const mipi_csi1_parents[] = { "tgpll-192m", "tgpll-307m2",
						  "v4nrpll-409m6",
						  "tgpll-512m" };
static SPRD_MUX_CLK(mipi_csi1, "mipi-csi1", mipi_csi1_parents, 0x10c,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const mipi_csi2_1_parents[] = { "tgpll-128m", "tgpll-192m",
						    "tgpll-256m",
						    "tgpll-307m2" };
static SPRD_MUX_CLK(mipi_csi2_1, "mipi-csi2_1",  mipi_csi2_1_parents,
		    0x118, 0, 2, UMS9620_MUX_FLAG);

static const char * const mipi_csi2_2_parents[] = { "tgpll-128m", "tgpll-192m",
						    "tgpll-256m",
						    "tgpll-307m2" };
static SPRD_MUX_CLK(mipi_csi2_2, "mipi-csi2_2",  mipi_csi2_2_parents,
		    0x124, 0, 2, UMS9620_MUX_FLAG);

static const char * const mipi_csi3_1_parents[] = { "tgpll-128m", "tgpll-192m",
						    "tgpll-256m",
						    "tgpll-307m2" };
static SPRD_MUX_CLK(mipi_csi3_1, "mipi-csi3_1",  mipi_csi3_1_parents,
		    0x130, 0, 2, UMS9620_MUX_FLAG);

static const char * const mipi_csi3_2_parents[] = { "tgpll-128m", "tgpll-192m",
						    "tgpll-256m",
						    "tgpll-307m2" };
static SPRD_MUX_CLK(mipi_csi3_2, "mipi-csi3_2",  mipi_csi3_2_parents,
		    0x13c, 0, 2, UMS9620_MUX_FLAG);

static const char * const dcam_mtx_parents[] = { "tgpll-153m6", "tgpll-307m2",
						 "v4nrpll-409m6",
						 "tgpll-512m" };
static SPRD_MUX_CLK(dcam_mtx, "dcam-mtx", dcam_mtx_parents, 0x154,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const dcam_blk_cfg_parents[] = { "ext-26m", "tgpll-48m",
						     "tgpll-64m", "tgpll-96m",
						     "tgpll-128m" };
static SPRD_MUX_CLK(dcam_blk_cfg, "dcam-blk-cfg", dcam_blk_cfg_parents,
		    0x160, 0, 3, UMS9620_MUX_FLAG);

static const char * const mm_mtx_data_parents[] = { "tgpll-153m6",
						    "tgpll-256m",
						    "tgpll-307m2",
						    "v4nrpll-409m6" };
static SPRD_MUX_CLK(mm_mtx_data, "mm-mtx-data", mm_mtx_data_parents,
		    0x16c, 0, 2, UMS9620_MUX_FLAG);

static const char * const jpg_parents[] = { "tgpll-153m6", "tgpll-256m",
					    "tgpll-307m2", "v4nrpll-409m6",
					    "tgpll-512m" };
static SPRD_MUX_CLK(jpg, "jpg", jpg_parents, 0x178,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const mm_sys_cfg_parents[] = { "ext-26m", "tgpll-48m",
						   "tgpll-64m", "tgpll-96m",
						   "tgpll-128m" };
static SPRD_MUX_CLK(mm_sys_cfg, "mm-sys-cfg", mm_sys_cfg_parents,
		    0x190, 0, 3, UMS9620_MUX_FLAG);

static const char * const sensor_parents[] = { "ext-26m", "tgpll-48m",
					       "tgpll-51m2", "tgpll-64m",
					       "tgpll-96m" };
static SPRD_COMP_CLK_OFFSET(sensor0, "sensor0", sensor_parents, 0x19c,
		     0, 3, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(sensor1, "sensor1", sensor_parents, 0x1a8,
		     0, 3, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(sensor2, "sensor2", sensor_parents, 0x1b4,
		     0, 3, 0, 3, 0);
static SPRD_COMP_CLK_OFFSET(sensor3, "sensor3", sensor_parents, 0x1c0,
		     0, 3, 0, 3, 0);

static struct sprd_clk_common *ums9620_mm_clk[] = {
	/* address base is 0x30010000 */
	&vdsp.common,
	&vdsp_m.common,
	&vdma.common,
	&vdsp_mtx_data.common,
	&vdsp_blk_cfg.common,
	&mm_uart.common,
	&isp.common,
	&cpp.common,
	&depth.common,
	&fd.common,
	&dcam0_1.common,
	&dcam0_1_axi.common,
	&dcam2_3.common,
	&dcam2_3_axi.common,
	&mipi_csi0.common,
	&mipi_csi1.common,
	&mipi_csi2_1.common,
	&mipi_csi2_2.common,
	&mipi_csi3_1.common,
	&mipi_csi3_2.common,
	&dcam_mtx.common,
	&dcam_blk_cfg.common,
	&mm_mtx_data.common,
	&jpg.common,
	&mm_sys_cfg.common,
	&sensor0.common,
	&sensor1.common,
	&sensor2.common,
	&sensor3.common,
};

static struct clk_hw_onecell_data ums9620_mm_clk_hws = {
	.hws	= {
		[CLK_VDSP]		= &vdsp.common.hw,
		[CLK_VDSP_M]		= &vdsp_m.common.hw,
		[CLK_VDMA]		= &vdma.common.hw,
		[CLK_VDSP_MTX_DATA]	= &vdsp_mtx_data.common.hw,
		[CLK_VDSP_BLK_CFG]	= &vdsp_blk_cfg.common.hw,
		[CLK_MM_UART]		= &mm_uart.common.hw,
		[CLK_ISP]		= &isp.common.hw,
		[CLK_CPP]		= &cpp.common.hw,
		[CLK_DEPTH]		= &depth.common.hw,
		[CLK_FD]		= &fd.common.hw,
		[CLK_DCAM0_1]		= &dcam0_1.common.hw,
		[CLK_DCAM0_1_AXI]	= &dcam0_1_axi.common.hw,
		[CLK_DCAM2_3]		= &dcam2_3.common.hw,
		[CLK_DCAM2_3_AXI]	= &dcam2_3_axi.common.hw,
		[CLK_MIPI_CSI0]		= &mipi_csi0.common.hw,
		[CLK_MIPI_CSI1]		= &mipi_csi1.common.hw,
		[CLK_MIPI_CSI2_1]	= &mipi_csi2_1.common.hw,
		[CLK_MIPI_CSI2_2]	= &mipi_csi2_2.common.hw,
		[CLK_MIPI_CSI3_1]	= &mipi_csi3_1.common.hw,
		[CLK_MIPI_CSI3_2]	= &mipi_csi3_2.common.hw,
		[CLK_DCAM_MTX]		= &dcam_mtx.common.hw,
		[CLK_DCAM_BLK_CFG]	= &dcam_blk_cfg.common.hw,
		[CLK_MM_MTX_DATA]	= &mm_mtx_data.common.hw,
		[CLK_JPG]		= &jpg.common.hw,
		[CLK_MM_SYS_CFG]	= &mm_sys_cfg.common.hw,
		[CLK_SENSOR0]		= &sensor0.common.hw,
		[CLK_SENSOR1]		= &sensor1.common.hw,
		[CLK_SENSOR2]		= &sensor2.common.hw,
		[CLK_SENSOR3]		= &sensor3.common.hw,
	},
	.num	= CLK_MM_CLK_NUM,
};

static struct sprd_clk_desc ums9620_mm_clk_desc = {
	.clk_clks	= ums9620_mm_clk,
	.num_clk_clks	= ARRAY_SIZE(ums9620_mm_clk),
	.hw_clks	= &ums9620_mm_clk_hws,
};

/* dpu vsp apb clock gates */
/* dpu vsp apb clocks configure CLK_IGNORE_UNUSED because these clocks may be
 * controlled by dpu sys at the same time. It may be cause an execption if
 * kernel gates these clock.
 */
static SPRD_SC_GATE_CLK(dpu_eb, "dpu-eb", "dpu-vsp-eb", 0x0,
			0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi0_eb, "dsi0-eb", "dpu-vsp-eb", 0x0,
			0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dsi1_eb, "dsi1-eb", "dpu-vsp-eb", 0x0,
			0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vpu_enc0_eb, "vpu-enc0-eb", "dpu-vsp-eb", 0x0,
			0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vpu_enc1_eb, "vpu-enc1-eb", "dpu-vsp-eb", 0x0,
			0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(vpu_dec_eb, "dpu-dec-eb", "dpu-vsp-eb", 0x0,
			0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gsp0_eb, "gsp0-eb", "dpu-vsp-eb", 0x0,
			0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(gsp1_eb, "gsp1-eb", "dpu-vsp-eb", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dpu_dvfs_eb, "dpu-dvfs-eb", "dpu-vsp-eb", 0x0,
			0x1000, BIT(8), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dpu_ckg_eb, "dpu-ckg-eb", "dpu-vsp-eb", 0x0,
			0x1000, BIT(9), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dpu_busmon_eb, "dpu-busmon-eb", "dpu-vsp-eb", 0x0,
			0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(m_div6clk_gate_en, "m-div6clk-gate-en", "dpu-vsp-eb",
		     0xb0, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_GATE_CLK(s_div6clk_gate_en, "s-div6clk-gate-en", "dpu-vsp-eb",
		     0xb0, BIT(4), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *ums9620_dpu_vsp_gate[] = {
	/* address base is 0x30100000 */
	&dpu_eb.common,
	&dsi0_eb.common,
	&dsi1_eb.common,
	&vpu_enc0_eb.common,
	&vpu_enc1_eb.common,
	&vpu_dec_eb.common,
	&gsp0_eb.common,
	&gsp1_eb.common,
	&dpu_dvfs_eb.common,
	&dpu_ckg_eb.common,
	&dpu_busmon_eb.common,
	&m_div6clk_gate_en.common,
	&s_div6clk_gate_en.common,
};

static struct clk_hw_onecell_data ums9620_dpu_vsp_gate_hws = {
	.hws    = {
		[CLK_DPU_EB]		= &dpu_eb.common.hw,
		[CLK_DSI0_EB]		= &dsi0_eb.common.hw,
		[CLK_DSI1_EB]		= &dsi1_eb.common.hw,
		[CLK_VPU_ENC0_EB]	= &vpu_enc0_eb.common.hw,
		[CLK_VPU_ENC1_EB]	= &vpu_enc1_eb.common.hw,
		[CLK_VPU_DEC_EB]	= &vpu_dec_eb.common.hw,
		[CLK_GSP0_EB]		= &gsp0_eb.common.hw,
		[CLK_GSP1_EB]		= &gsp1_eb.common.hw,
		[CLK_DPU_DVFS_EB]	= &dpu_dvfs_eb.common.hw,
		[CLK_DPU_CKG_EB]	= &dpu_ckg_eb.common.hw,
		[CLK_DPU_BUSMON_EB]	= &dpu_busmon_eb.common.hw,
		[CLK_M_DIV6CLK_GATE_EN]	= &m_div6clk_gate_en.common.hw,
		[CLK_S_DIV6CLK_GATE_EN] = &s_div6clk_gate_en.common.hw,
	},
	.num    = CLK_DPU_VSP_GATE_NUM,
};

static struct sprd_clk_desc ums9620_dpu_vsp_gate_desc = {
	.clk_clks	= ums9620_dpu_vsp_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_dpu_vsp_gate),
	.hw_clks	= &ums9620_dpu_vsp_gate_hws,
};

/* dpu vsp clocks */
static const char * const dpu_cfg_parents[] = { "ext-26m", "tgpll-128m",
						"tgpll-153m6" };
static SPRD_MUX_CLK(dpu_cfg, "dpu-cfg", dpu_cfg_parents, 0x28,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const vpu_mtx_parents[] = { "tgpll-256m", "tgpll-307m2",
						"tgpll-384m", "tgpll-512m" };
static SPRD_MUX_CLK(vpu_mtx, "vpu-mtx", vpu_mtx_parents, 0x34,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const vpu_enc_parents[] = { "tgpll-256m", "tgpll-307m2",
						"tgpll-384m", "tgpll-512m" };
static SPRD_MUX_CLK(vpu_enc, "vpu-enc", vpu_enc_parents, 0x40,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const vpu_dec_parents[] = { "tgpll-256m", "tgpll-307m2",
						"tgpll-384m", "tgpll-512m",
						"pixelpll-668m25" };
static SPRD_MUX_CLK(vpu_dec, "vpu-dec", vpu_dec_parents, 0x4c,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const gsp_parents[] = { "tgpll-256m", "tgpll-307m2",
					    "tgpll-384m", "tgpll-512m" };
static SPRD_MUX_CLK(gsp0, "gsp0", gsp_parents, 0x58,
		    0, 2, UMS9620_MUX_FLAG);
static SPRD_MUX_CLK(gsp1, "gsp1", gsp_parents, 0x64,
		    0, 2, UMS9620_MUX_FLAG);

static const char * const dispc0_parents[] = { "tgpll-256m", "tgpll-307m2",
					       "tgpll-384m", "v4nrpll-409m6",
					       "tgpll-512m", "v4nrpll-614m4" };
static SPRD_MUX_CLK(dispc0, "dispc0", dispc0_parents, 0x70,
		    0, 3, UMS9620_MUX_FLAG);

static const char * const dispc0_dpi_parents[] = { "tgpll-256m", "tgpll-307m2",
						   "dphy-312m5", "tgpll-384m",
						   "dphy-416m7" };
static SPRD_COMP_CLK_OFFSET(dispc0_dpi, "dispc0-dpi", dispc0_dpi_parents, 0x7c,
			    0, 3, 0, 3, 0);

static struct sprd_clk_common *ums9620_dpu_vsp_clk[] = {
	/* address base is 0x30110000 */
	&dpu_cfg.common,
	&vpu_mtx.common,
	&vpu_enc.common,
	&vpu_dec.common,
	&gsp0.common,
	&gsp1.common,
	&dispc0.common,
	&dispc0_dpi.common,
};

static struct clk_hw_onecell_data ums9620_dpu_vsp_clk_hws = {
	.hws    = {
		[CLK_DPU_CFG]		= &dpu_cfg.common.hw,
		[CLK_VPU_MTX]		= &vpu_mtx.common.hw,
		[CLK_VPU_ENC]		= &vpu_enc.common.hw,
		[CLK_VPU_DEC]		= &vpu_dec.common.hw,
		[CLK_GSP0]		= &gsp0.common.hw,
		[CLK_GSP1]		= &gsp1.common.hw,
		[CLK_DISPC0]		= &dispc0.common.hw,
		[CLK_DISPC0_DPI]	= &dispc0_dpi.common.hw,
	},
	.num    = CLK_DPU_VSP_CLK_NUM,
};

static struct sprd_clk_desc ums9620_dpu_vsp_clk_desc = {
	.clk_clks	= ums9620_dpu_vsp_clk,
	.num_clk_clks	= ARRAY_SIZE(ums9620_dpu_vsp_clk),
	.hw_clks	= &ums9620_dpu_vsp_clk_hws,
};

/* audcp global clock gates */
/* Audcp global clocks configure CLK_IGNORE_UNUSED because these clocks may be
 * controlled by audcp sys at the same time. It may be cause an execption if
 * kernel gates these clock.
 */
static SPRD_SC_GATE_CLK(audcp_iis0_eb, "audcp-iis0-eb", "access-aud-en",
			0x0, 0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_iis1_eb, "audcp-iis1-eb", "access-aud-en",
			0x0, 0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_iis2_eb, "audcp-iis2-eb", "access-aud-en",
			0x0, 0x1000, BIT(2), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_uart_eb, "audcp-uart-eb", "access-aud-en",
			0x0, 0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_dma_cp_eb, "audcp-dma-cp-eb", "access-aud-en",
			0x0, 0x1000, BIT(5), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_dma_ap_eb, "audcp-dma-ap-eb", "access-aud-en",
			0x0, 0x1000, BIT(6), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_src48k_eb, "audcp-src48k-eb", "access-aud-en",
			0x0, 0x1000, BIT(10), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_mcdt_eb, "audcp-mcdt-eb", "access-aud-en",
			0x0, 0x1000, BIT(12), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_vbc_eb, "audcp-vbc-eb", "access-aud-en",
			0x0, 0x1000, BIT(15), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_splk_eb, "audcp-splk-eb", "access-aud-en",
			0x0, 0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_icu_eb, "audcp-icu-eb", "access-aud-en",
			0x0, 0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_ap_ashb_eb, "dma-ap-ashb-eb", "access-aud-en",
			0x0, 0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(dma_cp_ashb_eb, "dma-cp-ashb-eb", "access-aud-en",
			0x0, 0x1000, BIT(20), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_aud_eb, "audcp-aud-eb", "access-aud-en",
			0x0, 0x1000, BIT(21), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audif_ckg_auto_en, "audif-ckg-auto-en", "access-aud-en",
			0x0, 0x1000, BIT(22), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_vbc_24m_eb, "audcp-vbc-24m-eb", "access-aud-en",
			0x0, 0x1000, BIT(23), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_tmr_26m_eb, "audcp-tmr-26m-eb", "access-aud-en",
			0x0, 0x1000, BIT(24), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_dvfs_ashb_eb, "audcp-dvfs-ashb-eb",
			"access-aud-en", 0x0, 0x1000, BIT(25),
			CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_matrix_cfg_en, "audcp-matrix-cfg-en",
			"access-aud-en", 0x0, 0x1000, BIT(26),
			CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_tdm_hf_eb, "audcp-tdm-hf-eb", "access-aud-en",
			0x0, 0x1000, BIT(27), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_tdm_eb, "audcp-tdm-eb", "access-aud-en",
			0x0, 0x1000, BIT(28), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_vbc_ap_eb, "audcp-vbc-ap-eb", "access-aud-en",
			0x4, 0x1000, BIT(17), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_mcdt_ap_eb, "audcp-mcdt-ap-eb", "access-aud-en",
			0x4, 0x1000, BIT(18), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_aud_ap_eb, "audcp-aud-ap-eb", "access-aud-en",
			0x4, 0x1000, BIT(19), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *ums9620_audcpglb_gate[] = {
	/* address base is 0x56200000 */
	&audcp_iis0_eb.common,
	&audcp_iis1_eb.common,
	&audcp_iis2_eb.common,
	&audcp_uart_eb.common,
	&audcp_dma_cp_eb.common,
	&audcp_dma_ap_eb.common,
	&audcp_src48k_eb.common,
	&audcp_mcdt_eb.common,
	&audcp_vbc_eb.common,
	&audcp_splk_eb.common,
	&audcp_icu_eb.common,
	&dma_ap_ashb_eb.common,
	&dma_cp_ashb_eb.common,
	&audcp_aud_eb.common,
	&audif_ckg_auto_en.common,
	&audcp_vbc_24m_eb.common,
	&audcp_tmr_26m_eb.common,
	&audcp_dvfs_ashb_eb.common,
	&audcp_matrix_cfg_en.common,
	&audcp_tdm_hf_eb.common,
	&audcp_tdm_eb.common,
	&audcp_vbc_ap_eb.common,
	&audcp_mcdt_ap_eb.common,
	&audcp_aud_ap_eb.common,
};

static struct clk_hw_onecell_data ums9620_audcpglb_gate_hws = {
	.hws	= {
		[CLK_AUDCP_IIS0_EB]		= &audcp_iis0_eb.common.hw,
		[CLK_AUDCP_IIS1_EB]		= &audcp_iis1_eb.common.hw,
		[CLK_AUDCP_IIS2_EB]		= &audcp_iis2_eb.common.hw,
		[CLK_AUDCP_UART_EB]		= &audcp_uart_eb.common.hw,
		[CLK_AUDCP_DMA_CP_EB]		= &audcp_dma_cp_eb.common.hw,
		[CLK_AUDCP_DMA_AP_EB]		= &audcp_dma_ap_eb.common.hw,
		[CLK_AUDCP_SRC48K_EB]		= &audcp_src48k_eb.common.hw,
		[CLK_AUDCP_MCDT_EB]		= &audcp_mcdt_eb.common.hw,
		[CLK_AUDCP_VBC_EB]		= &audcp_vbc_eb.common.hw,
		[CLK_AUDCP_SPLK_EB]		= &audcp_splk_eb.common.hw,
		[CLK_AUDCP_ICU_EB]		= &audcp_icu_eb.common.hw,
		[CLK_AUDCP_DMA_AP_ASHB_EB]	= &dma_ap_ashb_eb.common.hw,
		[CLK_AUDCP_DMA_CP_ASHB_EB]	= &dma_cp_ashb_eb.common.hw,
		[CLK_AUDCP_AUD_EB]		= &audcp_aud_eb.common.hw,
		[CLK_AUDIF_CKG_AUTO_EN]		= &audif_ckg_auto_en.common.hw,
		[CLK_AUDCP_VBC_24M_EB]		= &audcp_vbc_24m_eb.common.hw,
		[CLK_AUDCP_TMR_26M_EB]		= &audcp_tmr_26m_eb.common.hw,
		[CLK_AUDCP_DVFS_ASHB_EB]	= &audcp_dvfs_ashb_eb.common.hw,
		[CLK_AUDCP_MATRIX_CFG_EN]	= &audcp_matrix_cfg_en.common.hw,
		[CLK_AUDCP_TDM_HF_EB]		= &audcp_tdm_hf_eb.common.hw,
		[CLK_AUDCP_TDM_EB]		= &audcp_tdm_eb.common.hw,
		[CLK_AUDCP_VBC_AP_EB]		= &audcp_vbc_ap_eb.common.hw,
		[CLK_AUDCP_MCDT_AP_EB]		= &audcp_mcdt_ap_eb.common.hw,
		[CLK_AUDCP_AUD_AP_EB]		= &audcp_aud_ap_eb.common.hw,
	},
	.num	= CLK_AUDCP_GLB_GATE_NUM,
};

static const struct sprd_clk_desc ums9620_audcpglb_gate_desc = {
	.clk_clks	= ums9620_audcpglb_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_audcpglb_gate),
	.hw_clks	= &ums9620_audcpglb_gate_hws,
};

/* audcp aon apb gates */
/* Audcp aon aphb clocks configure CLK_IGNORE_UNUSED because these clocks may be
 * controlled by audcp sys at the same time. It may be cause an execption if
 * kernel gates these clock.
 */
static SPRD_SC_GATE_CLK(audcp_vad_eb, "audcp-vad-eb", "access-aud-en",
			0x0, 0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_pdm_eb, "audcp-pdm-eb", "access-aud-en",
			0x0, 0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_audif_eb, "audcp-audif-eb", "access-aud-en",
			0x0, 0x1000, BIT(3), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_pdm_iis_eb, "audcp-pdm-iis-eb", "access-aud-en",
			0x0, 0x1000, BIT(4), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_vad_apb_eb, "audcp-vad-apb-eb", "access-aud-en",
			0x4, 0x1000, BIT(0), CLK_IGNORE_UNUSED, 0);
static SPRD_SC_GATE_CLK(audcp_pdm_ap_eb, "audcp-pdm-ap-eb", "access-aud-en",
			0x4, 0x1000, BIT(1), CLK_IGNORE_UNUSED, 0);

static struct sprd_clk_common *ums9620_audcpapb_gate[] = {
	/* address base is 0x56390000 */
	&audcp_vad_eb.common,
	&audcp_pdm_eb.common,
	&audcp_audif_eb.common,
	&audcp_pdm_iis_eb.common,
	&audcp_vad_apb_eb.common,
	&audcp_pdm_ap_eb.common,
};

static struct clk_hw_onecell_data ums9620_audcpapb_gate_hws = {
	.hws	= {
		[CLK_AUDCP_VAD_EB]	= &audcp_vad_eb.common.hw,
		[CLK_AUDCP_PDM_EB]	= &audcp_pdm_eb.common.hw,
		[CLK_AUDCP_AUDIF_EB]	= &audcp_audif_eb.common.hw,
		[CLK_AUDCP_PDM_IIS_EB]	= &audcp_pdm_iis_eb.common.hw,
		[CLK_AUDCP_VAD_APB_EB]	= &audcp_vad_apb_eb.common.hw,
		[CLK_AUDCP_PDM_AP_EB]	= &audcp_pdm_ap_eb.common.hw,
	},
	.num	= CLK_AUDCP_APB_GATE_NUM,
};

static const struct sprd_clk_desc ums9620_audcpapb_gate_desc = {
	.clk_clks	= ums9620_audcpapb_gate,
	.num_clk_clks	= ARRAY_SIZE(ums9620_audcpapb_gate),
	.hw_clks	= &ums9620_audcpapb_gate_hws,
};

static const struct of_device_id sprd_ums9620_clk_ids[] = {
	{ .compatible = "sprd,ums9620-pmu-gate",	/* 0x64910000 */
	  .data = &ums9620_pmu_gate_desc },
	{ .compatible = "sprd,ums9620-g1-pll",		/* 0x64304000 */
	  .data = &ums9620_g1_pll_desc },
	{ .compatible = "sprd,ums9620-g1l-pll",		/* 0x64308000 */
	  .data = &ums9620_g1l_pll_desc },
	{ .compatible = "sprd,ums9620-g5l-pll",		/* 0x64324000 */
	  .data = &ums9620_g5l_pll_desc },
	{ .compatible = "sprd,ums9620-g5r-pll",         /* 0x64320000 */
	  .data = &ums9620_g5r_pll_desc },
	{ .compatible = "sprd,ums9620-g8-pll",		/* 0x6432c000 */
	  .data = &ums9620_g8_pll_desc },
	{ .compatible = "sprd,ums9620-g9-pll",		/* 0x64330000 */
	  .data = &ums9620_g9_pll_desc },
	{ .compatible = "sprd,ums9620-g10-pll",		/* 0x64334000 */
	  .data = &ums9620_g10_pll_desc },
	{ .compatible = "sprd,ums9620-apapb-gate",	/* 0x20100000 */
	  .data = &ums9620_apapb_gate_desc },
	{ .compatible = "sprd,ums9620-ap-clk",		/* 0x20010000 */
	  .data = &ums9620_ap_clk_desc },
	{ .compatible = "sprd,ums9620-apahb-gate",	/* 0x20000000 */
	  .data = &ums9620_apahb_gate_desc },
	{ .compatible = "sprd,ums9620-aon-gate",	/* 0x64900000 */
	  .data = &ums9620_aon_gate_desc },
	{ .compatible = "sprd,ums9620-aonapb-clk",	/* 0x64920000 */
	  .data = &ums9620_aonapb_clk_desc },
	{ .compatible = "sprd,ums9620-topdvfs-clk",	/* 0x64940000 */
	  .data = &ums9620_topdvfs_clk_desc },
	{ .compatible = "sprd,ums9620-gpuapb-gate",	/* 0x23000000 */
	  .data = &ums9620_gpuapb_gate_desc },
	{ .compatible = "sprd,ums9620-gpu-clk",		/* 0x23010000 */
	  .data = &ums9620_gpu_clk_desc },
	{ .compatible = "sprd,ums9620-ipaapb-gate",	/* 0x25000000 */
	  .data = &ums9620_ipaapb_gate_desc },
	{ .compatible = "sprd,ums9620-ipa-clk",		/* 0x25010000 */
	  .data = &ums9620_ipa_clk_desc },
	{ .compatible = "sprd,ums9620-ipaglb-gate",	/* 0x25240000 */
	  .data = &ums9620_ipaglb_gate_desc },
	{ .compatible = "sprd,ums9620-ipadispc-gate",	/* 0x31800000 */
	  .data = &ums9620_ipadispcglb_gate_desc },
	{ .compatible = "sprd,ums9620-pcieapb-gate",	/* 0x26000000 */
	  .data = &ums9620_pcieapb_gate_desc },
	{ .compatible = "sprd,ums9620-pcie-clk",	/* 0x26004000 */
	  .data = &ums9620_pcie_clk_desc },
	{ .compatible = "sprd,ums9620-aiapb-gate",	/* 0x27000000 */
	  .data = &ums9620_aiapb_gate_desc },
	{ .compatible = "sprd,ums9620-ai-clk",		/* 0x27004000 */
	  .data = &ums9620_ai_clk_desc },
	{ .compatible = "sprd,ums9620-ai-dvfs-clk",	/* 0x27008000 */
	  .data = &ums9620_ai_dvfs_clk_desc },
	{ .compatible = "sprd,ums9620-mm-gate",		/* 0x30000000 */
	  .data = &ums9620_mm_gate_desc },
	{ .compatible = "sprd,ums9620-mm-clk",		/* 0x30010000 */
	  .data = &ums9620_mm_clk_desc },
	{ .compatible = "sprd,ums9620-dpu-vsp-gate",	/* 0x30100000 */
	  .data = &ums9620_dpu_vsp_gate_desc },
	{ .compatible = "sprd,ums9620-dpu-vsp-clk",	/* 0x30110000 */
	  .data = &ums9620_dpu_vsp_clk_desc },
	{ .compatible = "sprd,ums9620-audcpglb-gate",	/* 0x56200000 */
	  .data = &ums9620_audcpglb_gate_desc },
	{ .compatible = "sprd,ums9620-audcpapb-gate",	/* 0x56390000 */
	  .data = &ums9620_audcpapb_gate_desc },
	{ }
};
MODULE_DEVICE_TABLE(of, sprd_ums9620_clk_ids);

static int ums9620_clk_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	const struct sprd_clk_desc *desc;
	int ret;

	match = of_match_node(sprd_ums9620_clk_ids, pdev->dev.of_node);
	if (!match) {
		dev_err(&pdev->dev, "of_match_node() failed\n");
		return -ENODEV;
	}

	desc = match->data;
	ret = sprd_clk_regmap_init(pdev, desc);
	if (ret)
		return ret;

	return sprd_clk_probe(&pdev->dev, desc->hw_clks);
}

static struct platform_driver ums9620_clk_driver = {
	.probe	= ums9620_clk_probe,
	.driver	= {
		.name	= "ums9620-clk",
		.of_match_table	= sprd_ums9620_clk_ids,
	},
};
module_platform_driver(ums9620_clk_driver);

MODULE_DESCRIPTION("Unisoc UMS9620 Clock Driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:ums9620-clk");
