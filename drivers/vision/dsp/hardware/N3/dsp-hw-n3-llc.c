// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series dsp driver
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 */

#include <dt-bindings/soc/samsung/exynos-dsp.h>

#include "dsp-log.h"
#include "dsp-hw-n3-llc.h"
#include "hardware/dummy/dsp-hw-dummy-llc.h"

static const struct dsp_llc_ops n3_llc_ops = {
	.llc_get_by_name	= dsp_hw_dummy_llc_get_by_name,
	.llc_put_by_name	= dsp_hw_dummy_llc_put_by_name,
	.llc_get_by_id		= dsp_hw_dummy_llc_get_by_id,
	.llc_put_by_id		= dsp_hw_dummy_llc_put_by_id,
	.llc_all_put		= dsp_hw_dummy_llc_all_put,

	.open			= dsp_hw_dummy_llc_open,
	.close			= dsp_hw_dummy_llc_close,
	.probe			= dsp_hw_dummy_llc_probe,
	.remove			= dsp_hw_dummy_llc_remove,
};

int dsp_hw_n3_llc_init(void)
{
	int ret;

	dsp_enter();
	ret = dsp_llc_register_ops(DSP_DEVICE_ID_N3, &n3_llc_ops);
	if (ret)
		goto p_err;

	dsp_leave();
	return 0;
p_err:
	return ret;
}
