/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __VERTEX_CTRL_H__
#define __VERTEX_CTRL_H__

struct vertex_system;

enum {
	VERTEX_REG_SS1,
	VERTEX_REG_SS2,
	VERTEX_REG_MAX
};

struct vertex_reg {
	const unsigned int	offset;
	const char		*name;
};

struct vertex_ctrl_ops {
	int (*reset)(struct vertex_system *sys);
	int (*start)(struct vertex_system *sys);
	int (*get_interrupt)(struct vertex_system *sys, int direction);
	int (*set_interrupt)(struct vertex_system *sys, int direction);
	int (*debug_dump)(struct vertex_system *sys);
};

extern const struct vertex_ctrl_ops vertex_ctrl_ops;

#endif
