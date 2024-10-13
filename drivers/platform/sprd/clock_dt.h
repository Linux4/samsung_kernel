/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#undef debug
#define debug(format, arg...) pr_debug("clk: " "@@@%s: " format, __func__, ## arg)
#define debug0(format, arg...)// pr_info("clk: " "@@@%s: " format, __func__, ## arg)
#define debug2(format, arg...) pr_debug("clk: " "@@@%s: " format, __func__, ## arg)

/**
 * struct clk_sel - list of sources for a given clock (pll)
 * @sources: array of pointers to clocks
 * @nr_sources: The size of @sources
 */
struct clk_sel {
	int nr_sources;
	u32 sources[];
};

/**
 * struct clk_reg - register definition for clock control bits
 * @reg: pointer to the register in virtual memory.
 * @shift: the shift in bits to where the bitfield is.
 * @size: the size in bits of the bitfield.
 * @mask: the mask of the bitfield.
 *
 * This specifies the size and position of the bits we are interested
 * in within the register specified by @reg.
 */
struct clk_reg {
	u32 reg;
	//unsigned short shift, size;
	u32 mask;
};

struct clk_regs {
	int id;
	const char *name;
	struct clk_reg enb, div, sel;

	//pll sources select
	int nr_sources;
	char *sources[];
};

/**
 * struct clk_ops - standard clock operations
 * @set_rate: set the clock rate, see clk_set_rate().
 * @get_rate: get the clock rate, see clk_get_rate().
 * @round_rate: round a given clock rate, see clk_round_rate().
 * @set_parent: set the clock's parent, see clk_set_parent().
 *
 * Group the common clock implementations together so that we
 * don't have to keep setting the same fields again. We leave
 * enable in struct clk.
 *
 * Adding an extra layer of indirection into the process should
 * not be a problem as it is unlikely these operations are going
 * to need to be called quickly.
 */

/**
 * struct clk - class of clock for newer style spreadtrum devices.
 * @clk: the standard clock representation
 * @sel: the parent sources select for this clock
 * @reg_sel: the register definition for selecting the pll clock's source
 * @reg_div: the register definition for the clock's output divisor
 * @reg_enb: the register definition for enable or disable the clock's source
 *
 * This clock implements the features required by the newer SoCs where
 * the standard clock block provides an input mux and a post-mux divisor
 * to provide the periperhal's clock.
 *
 * The array of @sel provides the mapping of mux position to the
 * clock, and @reg_sel shows the code where to modify to change the mux
 * position. The @reg_div defines how to change the divider settings on
 * the output.
 */

struct sci_clk {
	struct clk_hw hw;
	unsigned long rate;

	const struct clk_regs *regs;
};

#define MAX_DIV							(1000)

#define SCI_CLK_ADD(ID, RATE, ENB, ENB_BIT, DIV, DIV_MSK, SEL, SEL_MSK, NR_CLKS, ...)								\
static const struct clk_regs REGS_##ID = {  \
	.name = #ID,                            \
	.id = 0,                                \
	.enb = {                                \
		.reg = (u32)ENB,.mask = ENB_BIT,      	\
		},                                  \
	.div = {                                \
		.reg = (u32)DIV,.mask = DIV_MSK,			\
		},                                  \
	.sel = {                                \
		.reg = (u32)SEL,.mask = SEL_MSK,			\
		},                                  \
	.nr_sources = NR_CLKS,                  \
	.sources = {__VA_ARGS__},               \
};                                          \
static struct sci_clk ID = {              		\
	.hw = {0},								\
	.rate = RATE,                           \
	.regs = &REGS_##ID,                     \
};                                          \
const struct clk_lookup __clkinit1 CLK_LK_##ID = { \
	.dev_id = 0,							\
	.con_id = #ID,                          \
	.clk = (struct clk *)&ID,                       		\
};                                       	\

int __init sci_clk_register(struct clk_lookup *cl);

#define __clkinit0	__section(.rodata.clkinit0)
#define __clkinit1	__section(.rodata.clkinit1)
#define __clkinit2	__section(.rodata.clkinit2)
