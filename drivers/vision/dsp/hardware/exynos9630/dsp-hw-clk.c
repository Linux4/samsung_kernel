// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include "dsp-log.h"
#include "hardware/dsp-system.h"
#include "hardware/dsp-clk.h"

static struct dsp_clk_format clk_array[] = {
	{ NULL, "dnc_bus" },
	{ NULL, "dnc_busm" },
	{ NULL, "dnc" },
	{ NULL, "dsp_bus" },
	{ NULL, "dsp" },
};

void dsp_clk_dump(struct dsp_clk *clk)
{
	long count;
	const char *name;
	unsigned long freq;

	dsp_enter();
	for (count = 0; count < ARRAY_SIZE(clk_array); ++count) {
		name = clk_array[count].name;
		freq = clk_get_rate(clk_array[count].clk);
		if (!freq)
			continue;

		dsp_info("%15s(%ld) : %3lu.%06lu MHz\n",
				name, count, freq / 1000000, freq % 1000000);
	}

	dsp_leave();
}

void dsp_clk_user_dump(struct dsp_clk *clk, struct seq_file *file)
{
	long count;
	const char *name;
	unsigned long freq;

	dsp_enter();
	for (count = 0; count < ARRAY_SIZE(clk_array); ++count) {
		name = clk_array[count].name;
		freq = clk_get_rate(clk_array[count].clk);
		seq_printf(file, "%15s(%ld) : %3lu.%06lu MHz\n",
				name, count, freq / 1000000, freq % 1000000);
	}

	dsp_leave();
}

int dsp_clk_enable(struct dsp_clk *clk)
{
	int ret;
	long count;

	dsp_enter();
	for (count = 0; count < ARRAY_SIZE(clk_array); ++count) {
		ret = clk_prepare_enable(clk_array[count].clk);
		if (ret) {
			dsp_err("Failed to enable [%s(%ld/%lu)]clk(%d)\n",
					clk_array[count].name, count,
					ARRAY_SIZE(clk_array), ret);
			goto p_err;
		}
	}

	dsp_clk_dump(clk);
	dsp_leave();
	return 0;
p_err:
	for (count -= 1; count >= 0; --count)
		clk_disable_unprepare(clk_array[count].clk);

	return ret;
}

int dsp_clk_disable(struct dsp_clk *clk)
{
	long count;

	dsp_enter();
	for (count = 0; count < ARRAY_SIZE(clk_array); ++count)
		clk_disable_unprepare(clk_array[count].clk);

	dsp_leave();
	return 0;
}

int dsp_clk_open(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_clk_close(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

int dsp_clk_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_clk *clk;
	long count;

	dsp_enter();
	clk = &sys->clk;
	clk->dev = sys->dev;
	clk->array = clk_array;

	for (count = 0; count < ARRAY_SIZE(clk_array); ++count) {
		clk_array[count].clk = devm_clk_get(clk->dev,
				clk_array[count].name);
		if (IS_ERR(clk_array[count].clk)) {
			ret = IS_ERR(clk_array[count].clk);
			dsp_err("Failed to get [%s(%ld/%lu)]clk(%d)\n",
					clk_array[count].name, count,
					ARRAY_SIZE(clk_array), ret);
			goto p_err;
		}
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

void dsp_clk_remove(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
}
