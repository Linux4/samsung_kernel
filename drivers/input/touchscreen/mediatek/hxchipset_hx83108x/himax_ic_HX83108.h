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

#include "himax_platform.h"
#include "himax_common.h"
#include "himax_ic_core.h"
#include <linux/slab.h>


#define hx83108a_data_adc_num 48
#define hx83108_notouch_frame            0

#define hx83108a_fw_addr_raw_out_sel     0x100072ec
#define hx83108a_notouch_frame            0

#define hx83108_addr_crc_s1				0x800B0048
#define hx83108_addr_crc_s2				0x800B005C
#define hx83108_addr_crc_s3				0x800B0008
#define hx83108_addr_crc_s4				0x800B0044
#define hx83108_data_crc_s1				0x00000030
#define hx83108_data_crc_s2				0x0000CAAC
#define hx83108_data_crc_s3				0x00000001
#define hx83108_data_crc_s4				0x00000000
