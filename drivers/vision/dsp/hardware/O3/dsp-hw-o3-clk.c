// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-o3-system.h"
#include "dsp-hw-o3-clk.h"

static struct dsp_clk_format o3_clk_array[] = {
#ifndef ENABLE_DSP_VELOCE
	{ NULL, "umux_clkcmu_npus_bus_user" },
	{ NULL, "gate_adm_dap_npus_qch" },
	{ NULL, "gate_ip_npus_qch" },
	{ NULL, "umux_clkcmu_vpd_bus_user" },
	{ NULL, "gate_ip_vpd_qch" },
	{ NULL, "dout_div_clk_vpd_busp"},
#endif
};

static void dsp_hw_o3_clk_dump(struct dsp_clk *clk)
{
	long count;
	const char *name;
	unsigned long freq;

	dsp_enter();
	for (count = 0; count < clk->array_size; ++count) {
		name = clk->array[count].name;
		freq = clk_get_rate(clk->array[count].clk);
		dsp_dbg("%32s(%02ld) : %4lu.%06lu MHz\n",
				name, count, freq / 1000000, freq % 1000000);
	}

	dsp_leave();
}

static void dsp_hw_o3_clk_user_dump(struct dsp_clk *clk, struct seq_file *file)
{
	long count;
	const char *name;
	unsigned long freq;

	dsp_enter();
	for (count = 0; count < clk->array_size; ++count) {
		name = clk->array[count].name;
		freq = clk_get_rate(clk->array[count].clk);
		seq_printf(file, "%32s(%ld) : %4lu.%06lu MHz\n",
				name, count, freq / 1000000, freq % 1000000);
	}

	dsp_leave();
}

static int dsp_hw_o3_clk_enable(struct dsp_clk *clk)
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

static int dsp_hw_o3_clk_disable(struct dsp_clk *clk)
{
	long count;

	dsp_enter();
	for (count = 0; count < clk->array_size; ++count)
		clk_disable_unprepare(clk->array[count].clk);

	dsp_leave();
	return 0;
}

static int dsp_hw_o3_clk_open(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o3_clk_close(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
	return 0;
}

static int dsp_hw_o3_clk_probe(struct dsp_system *sys)
{
	int ret;
	struct dsp_clk *clk;
	long count;

	dsp_enter();
	clk = &sys->clk;
	clk->dev = sys->dev;
	clk->array = o3_clk_array;
	clk->array_size = ARRAY_SIZE(o3_clk_array);

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

static void dsp_hw_o3_clk_remove(struct dsp_clk *clk)
{
	dsp_enter();
	dsp_leave();
}

static const struct dsp_clk_ops o3_clk_ops = {
	.dump		= dsp_hw_o3_clk_dump,
	.user_dump	= dsp_hw_o3_clk_user_dump,
	.enable		= dsp_hw_o3_clk_enable,
	.disable	= dsp_hw_o3_clk_disable,

	.open		= dsp_hw_o3_clk_open,
	.close		= dsp_hw_o3_clk_close,
	.probe		= dsp_hw_o3_clk_probe,
	.remove		= dsp_hw_o3_clk_remove,
};

int dsp_hw_o3_clk_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_clk_register_ops(DSP_DEVICE_ID_O3, &o3_clk_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
