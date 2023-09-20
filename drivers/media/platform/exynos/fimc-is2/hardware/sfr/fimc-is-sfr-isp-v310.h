/*
 *  Samsung EXYNOS FIMC-IS2 (Imaging Subsystem) driver
 *
 *  Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#ifndef FIMC_IS_SFR_ISP_V310_H
#define FIMC_IS_SFR_ISP_V310_H

#include "fimc-is-hw-api-common.h"

#define ISP_REG_CNT 6
enum fimc_is_isp_reg_name {
	ISP_R_CIN2ISP_COL_CNT,
	ISP_R_CIN2ISP_ROW_CNT,
	ISP_R_BCROP1_START_X,
	ISP_R_BCROP1_START_Y,
	ISP_R_BCROP1_SIZE_X,
	ISP_R_BCROP1_SIZE_Y,
};

static struct fimc_is_reg isp_regs[ISP_REG_CNT] = {
	{0x3520, "CIN2ISP_COL_CNT"},
	{0x3524, "CIN2ISP_ROW_CNT"},
	{0x4104, "BCROP1_START_X"},
	{0x4108, "BCROP1_START_Y"},
	{0x410c, "BCROP1_SIZE_X"},
	{0x4110, "BCROP1_SIZE_Y"},
};

#define ISP_REG_FIELD_CNT 6
enum fimc_is_isp_reg_field {
	ISP_F_CIN2ISP_COL_CNT,
	ISP_F_CIN2ISP_ROW_CNT,
	ISP_F_BCROP1_START_X,
	ISP_F_BCROP1_START_Y,
	ISP_F_BCROP1_SIZE_X,
	ISP_F_BCROP1_SIZE_Y,
};

static struct fimc_is_field isp_fields[ISP_REG_FIELD_CNT] = {
	/* 1. sfr addr 2. register name 3. bit start 4. bit width 5. access type 6. reset */
	{"CIN2ISP_COL_CNT",		0,	14,	RO,		0},		/* CIN2ISP_COL_CNT */
	{"CIN2ISP_ROW_CNT",		0,	14,	RO,		0},		/* CIN2ISP_ROW_CNT */
	{"BCROP1_START_X",		0,	14,	RWS,	0},		/* BCROP1_START_X */
	{"BCROP1_START_Y",		0,	14,	RWS,	0},		/* BCROP1_START_Y */
	{"BCROP1_SIZE_X",		0,	14,	RWS,	0x0fc0},	/* BCROP1_SIZE_X */
	{"BCROP1_SIZE_Y",		0,	14,	RWS,	0x0bd0},	/* BCROP1_SIZE_Y */
};

#endif
