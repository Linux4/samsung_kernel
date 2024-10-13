/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for modularize functions
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

#ifndef __HIMAX_IC_USAGE_H__
#define __HIMAX_IC_USAGE_H__

extern bool _hx83112f_init(void);

#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102J)
extern bool _hx83102j_init(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83108A)
extern bool _hx83108a_init(void);
#endif

#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112F)
extern bool _hx83112f_init(void);
#endif


#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83122A)
extern bool _hx83122a_init(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83121A)
extern bool _hx83121a_init(void);
#endif
#if defined(CONFIG_TOUCHSCREEN_HIMAX_IC_HX85200A)
extern bool _hx85200a_init(void);
#endif

extern unsigned char IC_CHECKSUM;

#if defined(HX_EXCP_RECOVERY)
extern u8 HX_EXCP_RESET_ACTIVATE;
#endif


extern uint8_t *ovl_idx;


extern unsigned long FW_VER_MAJ_FLASH_ADDR;
extern unsigned long FW_VER_MIN_FLASH_ADDR;
extern unsigned long CFG_VER_MAJ_FLASH_ADDR;
extern unsigned long CFG_VER_MIN_FLASH_ADDR;
extern unsigned long CID_VER_MAJ_FLASH_ADDR;
extern unsigned long CID_VER_MIN_FLASH_ADDR;
extern uint32_t CFG_TABLE_FLASH_ADDR;
extern uint32_t g_cfg_table_hw_opt_crc;
extern uint32_t CFG_TABLE_FLASH_ADDR_T;

#if defined(HX_TP_PROC_2T2R)
	// static bool Is_2T2R;
#endif

#if defined(HX_USB_DETECT_GLOBAL)
	extern void (himax_cable_detect_func)(bool force_renew);
#endif

#if defined(HX_RST_PIN_FUNC)
	extern void (himax_rst_gpio_set)(int pinnum, uint8_t value);
#endif

extern struct himax_ts_data *hx_s_ts;
extern struct himax_core_fp hx_s_core_fp;
extern struct himax_ic_data *hx_s_ic_data;



extern void himax_mcu_on_cmd_init(void);
extern int himax_mcu_on_cmd_struct_init(void);


extern void hx_parse_assign_cmd(uint32_t addr, uint8_t *cmd, int len);

extern int himax_bus_read(uint8_t cmd, uint8_t *buf, uint32_t len);
extern int himax_bus_write(uint8_t cmd, uint32_t addr, uint8_t *data,
	uint32_t len);


extern void himax_int_enable(int enable);

#endif
