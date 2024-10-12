/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83112f chipset
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
#include "himax_common.h"
#include "himax_ic_core.h"
#include <linux/slab.h>


#define HX_83112F_SERIES_PWON		"HX83112f"

#define HX83112F_DATA_ADC_NUM 48

#define HX83112F_ADDR_RAWOUT_SEL     0x100072ec

#define HX83112F_REG_ICID 0x900000d0
#define HX83112F_ISRAM_SZ 65536
#define HX83112F_DSRAM_SZ 49152
#define HX83112F_FLASH_SIZE 131072
