/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#ifndef __DSP_CLK_H__
#define __DSP_CLK_H__

#include <linux/clk.h>

#include "dsp-hw-clk.h"

struct dsp_system;

struct dsp_clk_format {
	struct clk	*clk;
	const char	*name;
};

void dsp_clk_dump(struct dsp_clk *clk);
void dsp_clk_user_dump(struct dsp_clk *clk, struct seq_file *file);

int dsp_clk_enable(struct dsp_clk *clk);
int dsp_clk_disable(struct dsp_clk *clk);

int dsp_clk_open(struct dsp_clk *clk);
int dsp_clk_close(struct dsp_clk *clk);
int dsp_clk_probe(struct dsp_system *sys);
void dsp_clk_remove(struct dsp_clk *clk);

#endif
