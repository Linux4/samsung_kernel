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
#define hx122a_zone_base 0x10000600
/* CORE_FW */
#define hx122a_fw_addr_hsen_enable		(hx122a_func_base + 0x14)//0x10000114
#define hx122a_fw_addr_smwp_enable		(hx122a_func_base + 0x10)//0x10000110
#define hx122a_fw_usb_detect_addr		(hx122a_func_base + 0x38)//0x10000138

#define hx122a_fw_addr_raw_out_sel		(hx122a_cfg_base + 0x2EC)//0x100006EC

#define hx122a_fw_addr_selftest_addr_en		(hx122a_func_base + 0x18)//0x10000118
#define hx122a_fw_addr_selftest_result_addr	(hx122a_func_base + 0x24)//0x10000124
#define hx122a_fw_addr_gesture_en        (hx122a_func_base + 0x24)//0x10000124

#define hx122a_fw_addr_criteria_addr		(hx122a_func_base + 0x1C)//0x1000011c

#define hx122a_fw_addr_set_frame_addr		(hx122a_cfg_base + 0x294)//0x10000694

#define hx122a_fw_addr_sorting_mode_en		(hx122a_func_base + 0x04)//0x10000104
#define hx122a_fw_addr_fw_mode_status		(hx122a_cfg_base + 0x88)//0x10000488

#define hx122a_fw_addr_fw_ver_addr		(hx122a_cfg_base + 0x04)//0x10000404
#define hx122a_fw_addr_fw_cfg_addr		(hx122a_cfg_base + 0x84)//0x10000484
#define hx122a_fw_addr_fw_vendor_addr		hx122a_cfg_base//0x10007000
#define hx122a_fw_addr_cus_info			(hx122a_cfg_base + 0x08)//0x10000408
#define hx122a_fw_addr_proj_info		(hx122a_cfg_base + 0x14)//0x10000414

#define hx122a_fw_addr_fw_dbg_msg_addr		(hx122a_func_base + 0x40)//0x10000140

#define hx122a_fw_addr_dd_data_addr		(hx122a_func_base + 0x80)//0x10000180
#define hx122a_fw_addr_clr_fw_record_dd_sts	(hx122a_func_base + 0xCC)//0x100001CC
#define hx122a_fw_addr_ap_notify_fw_sus		(hx122a_func_base + 0xD0)//0x100001D0

#define hx122a_fw_addr_ctrl_mpap_ovl		(hx122a_cfg_base + 0x3EC)//0x100007EC

/*Inspection register*/
#define hx122a_fw_addr_normal_noise_thx		(hx122a_cfg_base + 0x8C)//0x1000048C
#define hx122a_fw_addr_lpwug_noise_thx		(hx122a_cfg_base + 0x90)//0x10000490
#define hx122a_fw_addr_noise_scale		(hx122a_cfg_base + 0x94)//0x10000494
#define hx122a_fw_addr_recal_thx		(hx122a_cfg_base + 0x90)//0x10000490
#define hx122a_fw_addr_palm_num			(hx122a_cfg_base + 0xA8)//0x100004A8
#define hx122a_fw_addr_weight_sup		(hx122a_cfg_base + 0x2C8)//0x100006C8
#define hx122a_fw_addr_normal_weight_a		(hx122a_cfg_base + 0x9C)//0x1000049C
#define hx122a_fw_addr_lpwug_weight_a		(hx122a_cfg_base + 0xA0)//0x100004A0
#define hx122a_fw_addr_weight_b			(hx122a_cfg_base + 0x94)//0x10000494
#define hx122a_fw_addr_max_dc			(hx122a_func_base + 0xC8)//0x100001C8
#define hx122a_fw_addr_skip_frame		(hx122a_cfg_base + 0xF4)//0x100004F4
#define hx122a_fw_addr_neg_noise_sup		(hx122a_func_base + 0xD8)//0x100001D8
#define hx122a_fw_addr_rotate           (hx122a_func_base + 0x3C)//0x1000013c 
#define hx122a_fw_addr_proximity          (hx122a_func_base + 0x20)//0x10000120 

#define hx122a_fw_addr_grip_zone            (hx122a_zone_base + 0x68)//0X10000668
#define hx122a_fw_addr_reject_zone          (hx122a_zone_base + 0x6c)//0X1000066C
#define hx122a_fw_addr_reject_zone_bound    (hx122a_zone_base + 0x70)//0X10000670 
#define hx122a_fw_addr_except_zone          (hx122a_zone_base + 0x74)//0X10000674 
/* CORE_FW */

/* CORE_FLASH */
/* CORE_FLASH */

/* CORE_SRAM */
#define hx122a_sram_adr_mkey			(hx122a_cfg_base + 0xE8)//0x100004E8
#define hx122a_sram_adr_rawdata_addr		0x10000A00
/* CORE_SRAM */

/* CORE_DRIVER */
#define hx122a_driver_addr_fw_define_flash_reload	hx122a_func_base//0x10007f00
#define hx122a_driver_addr_fw_define_2nd_flash_reload	(hx122a_cfg_base + 0x2C0)//0x100006c0

#define hx122a_driver_addr_fw_define_int_is_edge	(hx122a_cfg_base + 0x88)//0x10000488
#define hx122a_driver_addr_fw_define_rxnum_txnum	(hx122a_cfg_base + 0xF4)//0x100004f4

#define hx122a_driver_addr_fw_define_maxpt_xyrvs	(hx122a_cfg_base + 0xF8)//0x100004f8
#define hx122a_driver_addr_fw_define_x_y_res		(hx122a_cfg_base + 0xFC)//0x100004fc
#define hx122a_driver_addr_fw_define_stylus		(hx122a_cfg_base + 0x19C)//0x1000059c
#define hx122a_driver_addr_fw_define_idv2_ratio		(hx122a_cfg_base + 0x1fc)//0x100005fc
#define hx122a_driver_data_df_rx			60
#define hx122a_driver_data_df_tx			40
#define hx122a_driver_data_df_pt			10
/* CORE_DRIVER */

#ifdef SEC_FACTORY_MODE
	#define HX122A_FW_EDGE_BORDER_ON						0x11
	#define HX122A_FW_EDGE_BORDER_OFF						0x00
	#define HX122A_FW_EDGE_BORDER_ADDR						(hx122a_func_base + 0xF4)
//	#define FW_CAL_ON								0x01
//	#define FW_CAL_OFF								0x00
//	#define FW_CAL_ADDR								0x10007088	/*bit[2] */
	#define HX122A_FW_RPORT_THRSH_ADDR						0x1000708C	/*byte[3] */
#endif

#if defined(HX_CODE_OVERLAY)
#define hx122a_zf_addr_ovl_handshake			(hx122a_func_base + 0xFC)//0x10007FFC
#endif

#define hx83122a_addr_ic_ver_name			(hx122a_func_base + 0x350)
#define hx83122a_fw_addr_gesture_history		(hx122a_func_base + 0xCD00)

#define hx83122a_addr_osr_ctrl		(hx122a_cfg_base + 0x1c4) //0x100005c4
#define hx83122a_addr_snr_measurement (hx122a_func_base + 0x30) //0x10000130
#define hx83122a_addr_reject_idle		(hx122a_func_base + 0xD4) //0x100001D4

#define HX122A_ADDR_GRIP_ZONE  0x10000668
#define HX122A_ADDR_REJECT_ZONE  0x1000066C
#define HX122A_ADDR_REJECT_ZONE_BOUD  0x10000670
#define HX122A_ADDR_EXCEPT_ZONE  0x10000674

#define hx83122a_dsram_size 131072
#define hx83122a_isram_size 131072
#define hx83122a_dsram_addr 0x10000000
#define hx83122a_isram_addr 0x20000000

