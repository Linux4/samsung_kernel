/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for HX83122 chipset
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

#include "himax_platform_SPI.h"
#include "himax_common.h"
#include "himax_ic_core.h"
#include <linux/slab.h>

#define hx83122a_data_adc_num  1280 /* 640*2 */
#define hx83122a_notouch_frame 0
#define HX83122A_FLASH_SIZE    261120

/* CORE_IC */
#define hx122a_ic_cmd_incr4 0x12
/* CORE_IC */

// original
//#define fw_cfg_base  0x10007000
//#define fw_func_base 0x10007F00
// new architecture
#define hx122a_cfg_base  0x10000400
#define hx122a_func_base 0x10000100
/* CORE_FW */
#define hx122a_fw_addr_hsen_enable		(hx122a_func_base + 0x14)//0x10007F14
#define hx122a_fw_addr_smwp_enable		(hx122a_func_base + 0x10)//0x10007F10
#define hx122a_fw_usb_detect_addr		(hx122a_func_base + 0x38)//0x10007F38

#define hx122a_fw_addr_raw_out_sel		(hx122a_cfg_base + 0x2EC)//0x100072EC

#define hx122a_fw_addr_selftest_addr_en		(hx122a_func_base + 0x18)//0x10007F18
#define hx122a_fw_addr_selftest_result_addr	(hx122a_func_base + 0x24)//0x10007f24

#define hx122a_fw_addr_criteria_addr		(hx122a_func_base + 0x1C)//0x10007f1c

#define hx122a_fw_addr_set_frame_addr		(hx122a_cfg_base + 0x294)//0x10007294

#define hx122a_fw_addr_sorting_mode_en		(hx122a_func_base + 0x04)//0x10007f04
#define hx122a_fw_addr_fw_mode_status		(hx122a_cfg_base + 0x88)//0x10007088

#define hx122a_fw_addr_fw_ver_addr		(hx122a_cfg_base + 0x04)//0x10007004
#define hx122a_fw_addr_fw_cfg_addr		(hx122a_cfg_base + 0x84)//0x10007084
#define hx122a_fw_addr_fw_vendor_addr		hx122a_cfg_base//0x10007000
#define hx122a_fw_addr_cus_info			(hx122a_cfg_base + 0x08)//0x10007008
#define hx122a_fw_addr_proj_info		(hx122a_cfg_base + 0x14)//0x10007014

#define hx122a_fw_addr_fw_dbg_msg_addr		(hx122a_func_base + 0x40)//0x10007f40

#define hx122a_fw_addr_dd_data_addr		(hx122a_func_base + 0x80)//0x10007f80
#define hx122a_fw_addr_clr_fw_record_dd_sts	(hx122a_func_base + 0xCC)//0x10007FCC
#define hx122a_fw_addr_ap_notify_fw_sus		(hx122a_func_base + 0xD0)//0x10007FD0

#define hx122a_fw_addr_ctrl_mpap_ovl		(hx122a_cfg_base + 0x3EC)//0x100073EC

/*Inspection register*/
#define hx122a_fw_addr_normal_noise_thx		(hx122a_cfg_base + 0x8C)//0x1000708C
#define hx122a_fw_addr_lpwug_noise_thx		(hx122a_cfg_base + 0x90)//0x10007090
#define hx122a_fw_addr_noise_scale		(hx122a_cfg_base + 0x94)//0x10007094
#define hx122a_fw_addr_recal_thx		(hx122a_cfg_base + 0x90)//0x10007090
#define hx122a_fw_addr_palm_num			(hx122a_cfg_base + 0xA8)//0x100070A8
#define hx122a_fw_addr_weight_sup		(hx122a_cfg_base + 0x2C8)//0x100072C8
#define hx122a_fw_addr_normal_weight_a		(hx122a_cfg_base + 0x9C)//0x1000709C
#define hx122a_fw_addr_lpwug_weight_a		(hx122a_cfg_base + 0xA0)//0x100070A0
#define hx122a_fw_addr_weight_b			(hx122a_cfg_base + 0x94)//0x10007094
#define hx122a_fw_addr_max_dc			(hx122a_func_base + 0xC8)//0x10007FC8
#define hx122a_fw_addr_skip_frame		(hx122a_cfg_base + 0xF4)//0x100070F4
#define hx122a_fw_addr_neg_noise_sup		(hx122a_func_base + 0xD8)//0x10007FD8
/* CORE_FW */

/* CORE_FLASH */
/* CORE_FLASH */

/* CORE_SRAM */
#define hx122a_sram_adr_mkey			(hx122a_cfg_base + 0xE8)//0x100070E8
#define hx122a_sram_adr_rawdata_addr		0x10000A00
/* CORE_SRAM */

/* CORE_DRIVER */
#define hx122a_driver_addr_fw_define_flash_reload	hx122a_func_base//0x10007f00
#define hx122a_driver_addr_fw_define_2nd_flash_reload	(hx122a_cfg_base + 0x2C0)//0x100072c0

#define hx122a_driver_addr_fw_define_int_is_edge	(hx122a_cfg_base + 0x88)//0x10007088
#define hx122a_driver_addr_fw_define_rxnum_txnum	(hx122a_cfg_base + 0xF4)//0x100070f4

#define hx122a_driver_addr_fw_define_maxpt_xyrvs	(hx122a_cfg_base + 0xF8)//0x100070f8
#define hx122a_driver_addr_fw_define_x_y_res		(hx122a_cfg_base + 0xFC)//0x100070fc
#define hx122a_driver_addr_fw_define_stylus		(hx122a_cfg_base + 0x19C)//0x1000719c
#define hx122a_driver_addr_fw_define_idv2_ratio		(hx122a_cfg_base + 0x1fc)//0x100071fc
#define hx122a_driver_data_df_rx			60
#define hx122a_driver_data_df_tx			40
#define hx122a_driver_data_df_pt			10
/* CORE_DRIVER */

#ifdef SEC_FACTORY_MODE
	#define HX122A_FW_EDGE_BORDER_ON						0x11
	#define HX122A_FW_EDGE_BORDER_OFF						0x00
	#define HX122A_FW_EDGE_BORDER_ADDR						0x10007ff4
//	#define FW_CAL_ON								0x01
//	#define FW_CAL_OFF								0x00
//	#define FW_CAL_ADDR								0x10007088	/*bit[2] */
	#define HX122A_FW_RPORT_THRSH_ADDR						0x1000708C	/*byte[3] */
#endif

#if defined(HX_CODE_OVERLAY)
#define hx122a_zf_addr_ovl_handshake			(hx122a_func_base + 0xFC)//0x10007FFC
#endif

#define hx83122a_addr_ic_ver_name			(hx122a_func_base + 0x350)
#define hx83122a_fw_addr_gesture_history		(hx122a_func_base + 0x8000)

#define hx83122a_dsram_size 131072
#define hx83122a_isram_size 131072
#define hx83122a_dsram_addr 0x10000000
#define hx83122a_isram_addr 0x20000000

