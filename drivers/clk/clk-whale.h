/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __CLK_SC_H__
#define __CLK_SC_H__

#define WHALE_PLL_MAX_RATE		(0xFFFFFFFF)

struct whale_ibias_table {
	unsigned long rate;
	u8 ibias;
};

struct whale_ibias_table whale_adjustable_pll_table[] = {
	{
		.rate = 936000000,
		.ibias = 0x0,
	},
	{
		.rate = 1300000000,
		.ibias = 0x1,
	},
	{
		.rate = 1612000000,
		.ibias = 0x10,
	},
	{
		.rate = WHALE_PLL_MAX_RATE,
		.ibias = 0x10,
	},
};

struct whale_ibias_table whale_mpll1_table[] = {
	{
		.rate = 1404000000,
		.ibias = 0x0,
	},
	{
		.rate = 1872000000,
		.ibias = 0x1,
	},
	{
		.rate = 2548000000,
		.ibias = 0x10,
	},
	{
		.rate = WHALE_PLL_MAX_RATE,
		.ibias = 0x10,
	},
};

struct whale_ibias_table whale_rpll_table[] = {
	{
		.rate = 988000000,
		.ibias = 0x0,
	},
	{
		.rate = 1274000000,
		.ibias = 0x1,
	},
	{
		.rate = 1612000000,
		.ibias = 0x10,
	},
	{
		.rate = WHALE_PLL_MAX_RATE,
		.ibias = 0x10,
	},
};

struct whale_pll_config {
	const char *name;
	u32 lock_done;
	u32 div_s;
	u32 mod_en;
	u32 sdm_en;
	u32 refin_msk;
	u32 ibias_msk;
	u32 pll_n_msk;
	u32 nint_msk;
	u32 kint_msk;
	u32 prediv_msk;
};

struct whale_pll_config whale_pll_config_table[] = {
	{
		.name  = "clk_mpll0",
		.lock_done = (1 << 28),
		.div_s = (1 << 27),
		.mod_en = (1 << 26),
		.sdm_en = (1 << 25),
		.refin_msk = (3 << 18),
		.ibias_msk = (3 << 16),
		.pll_n_msk = (0x7ff),
		.nint_msk = (0x3f << 24),
		.kint_msk = 0xfffff,
		.prediv_msk = 0x0,
	},
	{
		.name  = "clk_mpll1",
		.lock_done = (1 << 28),
		.div_s = (1 << 27),
		.mod_en = (1 << 26),
		.sdm_en = (1 << 25),
		.refin_msk = (3 << 18),
		.ibias_msk = (3 << 16),
		.pll_n_msk = 0x7ff,
		.nint_msk = (0x3f << 24),
		.kint_msk = 0xfffff,
		.prediv_msk = (1 << 29),
	},
	{
		.name  = "clk_dpll0",
		.lock_done = (1 << 28),
		.div_s = (1 << 27),
		.mod_en = (1 << 26),
		.sdm_en = (1 << 25),
		.refin_msk = (3 << 20),
		.ibias_msk = (3 << 18),
		.pll_n_msk = 0x7ff,
		.nint_msk = (0x3f << 24),
		.kint_msk = 0xfffff,
		.prediv_msk = 0x0,
	},
	{
		.name  = "clk_dpll1",
		.lock_done = (1 << 29),
		.div_s = (1 << 26),
		.mod_en = (1 << 25),
		.sdm_en = (1 << 24),
		.refin_msk = (3 << 19),
		.ibias_msk = (3 << 17),
		.pll_n_msk = 0x7ff,
		.nint_msk = (0x3f << 24),
		.kint_msk = 0xfffff,
		.prediv_msk = 0x0,
	},
	{
		.name  = "clk_twpll",
		.lock_done = (1 << 29),
		.div_s = (1 << 26),
		.mod_en = (1 << 25),
		.sdm_en = (1 << 24),
		.refin_msk = (3 << 19),
		.ibias_msk = (3 << 17),
		.pll_n_msk = 0x7ff,
		.nint_msk = (0x3f << 23),
		.kint_msk = 0xfffff,
		.prediv_msk = 0x0,
	},
	{
		.name  = "clk_ltepll0",
		.lock_done = (1 << 30),
		.div_s = (1 << 26),
		.mod_en = (1 << 25),
		.sdm_en = (1 << 24),
		.refin_msk = (3 << 19),
		.ibias_msk = (3 << 17),
		.pll_n_msk = 0x7ff,
		.nint_msk = (0x3f << 24),
		.kint_msk = 0xfffff,
		.prediv_msk = 0x0,
	},
	{
		.name  = "clk_ltepll1",
		.lock_done = (1 << 30),
		.div_s = (1 << 26),
		.mod_en = (1 << 25),
		.sdm_en = (1 << 24),
		.refin_msk = (3 << 19),
		.ibias_msk = (3 << 17),
		.pll_n_msk = 0x7ff,
		.nint_msk = (0x3f << 24),
		.kint_msk = 0xfffff,
		.prediv_msk = 0x0,
	},
	{
		.name  = "clk_gpll",
		.lock_done = (1 << 24),
		.div_s = (1 << 21),
		.mod_en = (1 << 20),
		.sdm_en = (1 << 19),
		.refin_msk = (3 << 14),
		.ibias_msk = (3 << 12),
		.pll_n_msk = 0x7ff,
		.nint_msk = (0x3f << 24),
		.kint_msk = 0xfffff,
		.prediv_msk = 0x0,
	},
	{
		.name  = "clk_lvdsdispll",
		.lock_done = (1 << 27),
		.div_s = (1 << 26),
		.mod_en = (1 << 25),
		.sdm_en = (1 << 24),
		.refin_msk = (3 << 19),
		.ibias_msk = (3 << 17),
		.pll_n_msk = 0x7ff,
		.nint_msk = (0x3f << 24),
		.kint_msk = 0xfffff,
		.prediv_msk = 0x0,
	},
	{
	},
};

/* bits definitions for register REG RPLL CFG0 */
#define BITS_RPLL_N(_X_)				((_X_) << 17 & ((BIT(17)|BIT(18)|BIT(19)|BIT(20)|BIT(21)|BIT(22)|BIT(23)|BIT(24)|BIT(25)|BIT(26)|BIT(27))))
#define BITS_RPLL_IBIAS(_X_)				((_X_) << 15 & (BIT(15)|BIT(16)))
#define BITS_RPLL_REFIN(_X_)				((_X_) << 10 & (BIT(10)|BIT(11)))
#define BITS_RPLL_NINT(_X_)				((_X_) << 4  & (BIT(4)|BIT(5)|BIT(6)|BIT(7)|BIT(8)|BIT(9)))
#define BIT_RPLL_DIV_S					(BIT(3))
#define BITS_RPLL_REF_SEL(_X_)				((_X_) << 1  & (BIT(1)|BIT(2)))
#define BITS_RPLL_LOCK_DONE				(BIT(0))
/* bits definition for register REG RPLL CFG1 */
#define BITS_RPLL_KINT(_X_)				((_X_) & (0x7fffff))
/* bits definition for register REG RPLL CFG2 */
#define BIT_RPLL_SDM_EN					(BIT(17))
#define BIT_RPLL_MOD_EN					(BIT(16))

#endif

