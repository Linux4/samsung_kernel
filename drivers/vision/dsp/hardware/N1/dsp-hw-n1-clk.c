// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2019 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "hardware/N1/dsp-hw-n1-system.h"
#include "hardware/N1/dsp-hw-n1-clk.h"

static struct dsp_clk_format n1_clk_array[] = {
	{ NULL, "dnc_bus" },
	{ NULL, "dnc_busm" }, // 1
	{ NULL, "dnc" }, // 2
	{ NULL, "dspc" },
	{ NULL, "out_dnc_bus" },
	{ NULL, "out_dnc_busp" },
	{ NULL, "dsp_bus0" },
	{ NULL, "dsp0" }, // 7
	{ NULL, "out_dsp_bus0" },
	{ NULL, "out_dsp_busp0" },
	{ NULL, "dsp_bus1" },
	{ NULL, "dsp1" }, // 11
	{ NULL, "out_dsp_bus1" },
	{ NULL, "out_dsp_busp1" },
	{ NULL, "dsp_bus2" },
	{ NULL, "dsp2" }, // 15
	{ NULL, "out_dsp_bus2" },
	{ NULL, "out_dsp_busp2" },
};

static void dsp_hw_n1_clk_dump(struct dsp_clk *clk)
{
	long count;
	const char *name;
	unsigned long freq;

	dsp_enter();
	for (count = 0; count < clk->array_size; ++count) {
		/* Only output each core and DMA */
		if ((count != 1) && (count != 2) && (count != 7) &&
				(count != 11) && (count != 15))
			continue;

		name = clk->array[count].name;
		freq = clk_get_rate(clk->array[count].clk);
		dsp_dbg("%15s(%02ld) : %3lu.%06lu MHz\n",
				name, count, freq / 1000000, freq % 1000000);
	}

	dsp_leave();
}

static void dsp_hw_n1_clk_user_dump(struct dsp_clk *clk, struct seq_file *file)
{
	long count;
	const char *name;
	unsigned long freq;

	dsp_enter();
	for (count = 0; count < clk->array_size; ++count) {
		name = clk->array[count].name;
		freq = clk_get_rate(clk->array[count].clk);
		seq_printf(file, "%15s(%ld) : %3lu.%06lu MHz\n",
				name, count, freq / 1000000, freq % 1000000);
	}

	dsp_leave();
}

static int dsp_hw_n1_clk_enable(struct dsp_clk *clk)
{
	int ret;
	long count;

	dsp_enter();
	for (count = 0; count < clk->array_size; ++count) {
		ret = clk_prepare_enable(clk->array[count].clk);
		if (ret) {
			dsp_err("Failed to enable [%s(%ld/%u)]clk(%d)\n",
					clk->array[count].name, count,
					clk->array_size, ret);
			goto p_err;
		}
	}

	clk->ops->dump(clk);
	dsp_leave();
	return 0;
p_err:
	for (count -= 1; count >= 0; --count)
		clk_disable_unprepare(clk->array[count].clk);

	return ret;
}

static int dsp_hw_n1_clk_disable(struct dsp_clk *clk)
{
	long count;

	dsp_enter();
	for (count = 0; count < clk->array_size; ++count)
		clk_disable_unprepare(clk->array[count].clk);

	dsp_leave();
	return 0;
}

static int dsp_hw_n1_clk_open(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_n1_clk_close(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_n1_clk_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_clk *clk;
	long count;

	dsp_enter();
	clk = &sys->clk;
	clk->dev = sys->dev;
	clk->array = n1_clk_array;
	clk->array_size = ARRAY_SIZE(n1_clk_array);

	for (count = 0; count < clk->array_size; ++count) {
		clk->array[count].clk = devm_clk_get(clk->dev,
				clk->array[count].name);
		if (IS_ERR(clk->array[count].clk)) {
			ret = IS_ERR(clk->array[count].clk);
			dsp_err("Failed to get [%s(%ld/%u)]clk(%d)\n",
					clk->array[count].name, count,
					clk->array_size, ret);
			goto p_err;
		}
	}

	dsp_leave();
	return 0;
p_err:
	return ret;
}

static void dsp_hw_n1_clk_remove(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
}

static const struct dsp_clk_ops n1_clk_ops = {
	.dump		= dsp_hw_n1_clk_dump,
	.user_dump	= dsp_hw_n1_clk_user_dump,
	.enable		= dsp_hw_n1_clk_enable,
	.disable	= dsp_hw_n1_clk_disable,

	.open		= dsp_hw_n1_clk_open,
	.close		= dsp_hw_n1_clk_close,
	.probe		= dsp_hw_n1_clk_probe,
	.remove		= dsp_hw_n1_clk_remove,
};

int dsp_hw_n1_clk_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_clk_register_ops(DSP_DEVICE_ID_N1, &n1_clk_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
