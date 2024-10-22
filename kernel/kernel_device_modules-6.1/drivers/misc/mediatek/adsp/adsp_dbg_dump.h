/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __ADSP_DB_DUMP_H__
#define __ADSP_DB_DUMP_H__

enum adsp_clk_fmeter_type {
	ADSPPLLDIV = 0,
	NUM_OF_ADSP_FMETER
};

int adsp_dbg_dump_init(struct platform_device *pdev);

/* get if need check adsp pll freq when start */
bool adsp_check_adsppll_freq(unsigned int adsp_meter_type);

#endif /* __ADSP_DB_DUMP_H__ */

