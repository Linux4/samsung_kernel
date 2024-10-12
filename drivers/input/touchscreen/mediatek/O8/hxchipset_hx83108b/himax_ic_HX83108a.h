/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83108a chipset
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


#define HX_83108A_SERIES_PWON        "HX83108A"

#define HX83108A_DATA_ADC_NUM 48

#define HX83108A_ADDR_RAWOUT_SEL     0x100072ec

#define HX83108A_REG_ICID 0x900000d0
#define HX83108A_ISRAM_SZ 65536
#define HX83108A_DSRAM_SZ 49152
#define HX83108A_FLASH_SIZE 131072

#define hx83108_addr_crc_s1                0x800B0048
#define hx83108_addr_crc_s2                0x800B005C
#define hx83108_addr_crc_s3                0x800B0008
#define hx83108_addr_crc_s4                0x800B0044
#define hx83108_data_crc_s1                0x00000030
#define hx83108_data_crc_s2                0x0000CAAC
#define hx83108_data_crc_s3                0x00000001
#define hx83108_data_crc_s4                0x00000000
