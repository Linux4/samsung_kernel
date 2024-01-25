/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83102J chipset
 *
 *  Copyright (C) 2022 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2,  as published by the Free Software Foundation,  and
 *  may be copied,  distributed,  and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "himax_platform.h"
//#include "himax_common.h"
#include "himax_ic_core.h"
#include <linux/slab.h>


#define HX_83102J_SERIES_PWON		"HX83102J"

#define HX83102J_DATA_ADC_NUM 48

#define HX83102J_ADDR_RAWOUT_SEL     0x100072ec

#define HX83102J_ADDR_TCON_ON_RST 0x80020004

#define HX83102J_REG_ICID 0x900000d0
#define HX83102J_ISRAM_SZ 163840
#define HX83102J_DSRAM_SZ 73728
#define HX83102J_FLASH_SIZE 261120

#define HX83102J_ADDR_EN_HW_CRC 0x80010000
#define HX83102J_DATA_EN_HW_CRC 0x0000ECCE
