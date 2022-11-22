/*
 * stbcfg01_charger.h
 * Samsung Mobile Charger Header for STBCFG01
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __STBCFG01_CHARGER_H
#define __STBCFG01_CHARGER_H __FILE__

#include <linux/mfd/core.h>
#include <linux/mfd/stbcfg01.h>
#include <linux/mfd/stbcfg01-private.h>

#define sec_charger_dev_t	struct stbcfg01_dev
#define sec_charger_pdata_t	struct stbcfg01_platform_data

/* structure of the STBCFG01 charger conf and status data */
typedef struct  {
	int Cfg1;
	int Cfg2;
	int Cfg3;
	int Cfg4;
	int Cfg5;
	int Status1;
	int Status2;
	int IntEn1;
	int IntEn2;
	int GPIO_cd;
	int GPIO_shdn;
} STBCFG01_ChargerDataTypeDef;

static STBCFG01_ChargerDataTypeDef ChargerData;   /* STBCFG01 global charger data */

struct sec_chg_info {
	int Cfg1;
	int Cfg2;
	int Cfg3;
	int Cfg4;
	int Cfg5;
	int Status1;
	int Status2;
	int IntEn1;
	int IntEn2;
	int GPIO_cd;
	int GPIO_shdn;
};

#endif /* __STBCFG01_CHARGER_H */



