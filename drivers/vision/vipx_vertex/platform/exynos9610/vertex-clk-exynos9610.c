/*
 * Samsung Exynos SoC series VIPx driver
 *
 * Copyright (c) 2018 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "vertex-log.h"
#include "vertex-system.h"
#include "platform/vertex-clk.h"

enum vertex_clk_id {
	VERTEX_CLK_UMUX_CLKCMU_VIPX1_BUS,
	VERTEX_CLK_GATE_VIPX1_QCH,
	VERTEX_CLK_UMUX_CLKCMU_VIPX2_BUS,
	VERTEX_CLK_GATE_VIPX2_QCH,
	VERTEX_CLK_GATE_VIPX2_QCH_LOCAL,
	VERTEX_CLK_MAX
};

static struct vertex_clk vertex_exynos9610_clk_array[] = {
	{ NULL, "UMUX_CLKCMU_VIPX1_BUS" },
	{ NULL, "GATE_VIPX1_QCH"        },
	{ NULL, "UMUX_CLKCMU_VIPX2_BUS" },
	{ NULL, "GATE_VIPX2_QCH"        },
	{ NULL, "GATE_VIPX2_QCH_LOCAL"  },
};

static int vertex_exynos9610_clk_init(struct vertex_system *sys)
{
	int ret;
	int index;
	const char *name;
	struct clk *clk;

	vertex_enter();
	if (ARRAY_SIZE(vertex_exynos9610_clk_array) != VERTEX_CLK_MAX) {
		ret = -EINVAL;
		vertex_err("clock array size is invalid (%zu/%d)\n",
				ARRAY_SIZE(vertex_exynos9610_clk_array),
				VERTEX_CLK_MAX);
		goto p_err;
	}

	for (index = 0; index < VERTEX_CLK_MAX; ++index) {
		name = vertex_exynos9610_clk_array[index].name;
		if (!name) {
			ret = -EINVAL;
			vertex_err("clock name is NULL (%d)\n", index);
			goto p_err;
		}

		clk = devm_clk_get(sys->dev, name);
		if (IS_ERR(clk)) {
			ret = PTR_ERR(clk);
			vertex_err("Failed to get clock(%s/%d) (%d)\n",
					name, index, ret);
			goto p_err;
		}

		vertex_exynos9610_clk_array[index].clk = clk;
	}

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static void vertex_exynos9610_clk_deinit(struct vertex_system *sys)
{
	vertex_enter();
	vertex_leave();
}

static int vertex_exynos9610_clk_cfg(struct vertex_system *sys)
{
	vertex_enter();
	vertex_leave();
	return 0;
}

static int vertex_exynos9610_clk_on(struct vertex_system *sys)
{
	int ret;
	int index;
	const char *name;
	struct clk *clk;

	vertex_enter();
	for (index = 0; index < VERTEX_CLK_MAX; ++index) {
		name = vertex_exynos9610_clk_array[index].name;
		clk = vertex_exynos9610_clk_array[index].clk;

		ret = clk_prepare_enable(clk);
		if (ret) {
			vertex_err("Failed to enable clock(%s/%d) (%d)\n",
					name, index, ret);
			goto p_err;
		}
	}

	vertex_leave();
	return 0;
p_err:
	return ret;
}

static int vertex_exynos9610_clk_off(struct vertex_system *sys)
{
	int index;
	const char *name;
	struct clk *clk;

	vertex_enter();
	for (index = VERTEX_CLK_MAX - 1; index >= 0; --index) {
		name = vertex_exynos9610_clk_array[index].name;
		clk = vertex_exynos9610_clk_array[index].clk;

		clk_disable_unprepare(clk);
	}

	vertex_leave();
	return 0;
}

static int vertex_exynos9610_clk_dump(struct vertex_system *sys)
{
	int index;
	const char *name;
	struct clk *clk;
	unsigned long freq;

	vertex_enter();
	for (index = 0; index < VERTEX_CLK_MAX; ++index) {
		name = vertex_exynos9610_clk_array[index].name;
		clk = vertex_exynos9610_clk_array[index].clk;

		freq = clk_get_rate(clk);
		vertex_info("%30s(%d) : %9lu Hz\n", name, index, freq);
	}

	vertex_leave();
	return 0;
}

const struct vertex_clk_ops vertex_clk_ops = {
	.clk_init	= vertex_exynos9610_clk_init,
	.clk_deinit	= vertex_exynos9610_clk_deinit,
	.clk_cfg	= vertex_exynos9610_clk_cfg,
	.clk_on		= vertex_exynos9610_clk_on,
	.clk_off	= vertex_exynos9610_clk_off,
	.clk_dump	= vertex_exynos9610_clk_dump
};
