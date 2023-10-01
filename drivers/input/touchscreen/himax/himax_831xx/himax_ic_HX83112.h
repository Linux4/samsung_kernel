/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83112 chipset
 *
 *  Copyright (C) 2019 Himax Corporation.
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

#if defined(CONFIG_TOUCHSCREEN_HIMAX_I2C)
#include "himax_platform.h"
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_SPI)
#include "himax_platform_SPI.h"
#endif

#include "himax_common.h"
#include "himax_ic_core.h"
#include <linux/slab.h>



#define hx83112f_data_adc_num 48
#define hx83112_notouch_frame            0

#define hx83112f_fw_addr_raw_out_sel     0x100072ec
#define hx83112f_addr_ic_ver_name        0x10007050
#define hx83112f_notouch_frame            0
#define hx83112f_dsram_addr 0x10000000
#define hx83112f_isram_addr 0x20000000
#define hx83112f_dsram_size 32768
#define hx83112f_isram_size 65536
