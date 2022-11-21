/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_CLK_H__
#define __VERTEX_CLK_H__

#include <linux/clk.h>

struct vertex_system;

struct vertex_clk {
	struct clk	*clk;
	const char	*name;
};

struct vertex_clk_ops {
	int (*clk_init)(struct vertex_system *sys);
	void (*clk_deinit)(struct vertex_system *sys);
	int (*clk_cfg)(struct vertex_system *sys);
	int (*clk_on)(struct vertex_system *sys);
	int (*clk_off)(struct vertex_system *sys);
	int (*clk_dump)(struct vertex_system *sys);
};

extern const struct vertex_clk_ops vertex_clk_ops;

#endif
