/*
 * include/linux/mfd/rt5511/pdata.h -- Platform data for RT5511
 *
 * Richtek RT5511 Codec driver platform data definitions
 *
 * Copyright (C) 2013 Richtek Technology Corp.
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __MFD_RT5511_PDATA_H__
#define __MFD_RT5511_PDATA_H__
#include <linux/mfd/rt5511/core.h>
#include <linux/mfd/rt5511/registers.h>

struct rt5511_eq_cfg {
	bool en[RT5511_BQ_REG_NUM];
	u8 coeff[RT5511_BQ_REG_NUM][RT5511_BQ_REG_SIZE];
};

struct rt5511_drc_cfg {
	u8 coeff1[RT5511_DRC_REG1_SIZE];
	u8 coeff2[RT5511_DRC_REG2_SIZE];
};

struct rt5511_pdata {
	/* RT5511 microphone biases: 1.9V, 2.2V, 2.5V 2.8V*/
	unsigned int micbias[2];
	unsigned int mic1_differential:1;
	unsigned int mic2_differential:1;
	unsigned int mic3_differential:1;

	bool *eq_en;
	struct rt5511_eq_cfg *eq_cfgs;

	bool *drc_en;
	struct rt5511_drc_cfg *drc_cfgs;

	u8 drc_ng_en;
	u8 *drc_ng_cfg;

	u8 *dre_cfg;

	int gpio_sda;
	int gpio_scl;
	u32 sda_gpio_flags;
	u32 scl_gpio_flags;
};

#endif
