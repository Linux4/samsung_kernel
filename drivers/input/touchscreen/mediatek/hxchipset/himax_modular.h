/* SPDX-License-Identifier: GPL-2.0 */
/*  Himax Android Driver Sample Code for modularize functions
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

#ifndef __HIMAX_MODULAR_H__
#define __HIMAX_MODULAR_H__

#if defined(HX_USE_KSYM)
extern struct himax_chip_entry *hx_init_chip_entry(void);
extern struct himax_chip_entry *hx_self_sym_lookup;
extern void hx_release_chip_entry(void);
static bool (*this_detect_fp)(void);
static void himax_add_chip_dt(bool (*detect_fp)(void))
{
	int i = 0;

	this_detect_fp = detect_fp;
	for (i = 0; i < hx_self_sym_lookup->hx_ic_dt_num; i++) {
		if (hx_self_sym_lookup->core_chip_dt[i].fp_chip_detect
			== NULL) {
			hx_self_sym_lookup->core_chip_dt[i].fp_chip_detect
				= kzalloc(sizeof(struct himax_chip_detect),
					GFP_KERNEL);
			if (hx_self_sym_lookup->core_chip_dt[i].fp_chip_detect
				== NULL) {
				E("%s: Failed to allocate core_chip_dt\n",
					__func__);
				return;
			}
			hx_self_sym_lookup->core_chip_dt[i].fp_chip_detect
				= this_detect_fp;
			break;
		}
	}
}
static void free_chip_dt_table(void)
{
	hx_release_chip_entry();
}
#else
static bool (*this_detect_fp)(void);
extern struct himax_chip_entry hx_self_sym_lookup;
extern void hx_release_chip_entry(void);
static void himax_add_chip_dt(bool (*detect_fp)(void))
{
	this_detect_fp = detect_fp;
	if (hx_self_sym_lookup.core_chip_dt == NULL) {
		hx_self_sym_lookup.core_chip_dt = kcalloc(HX_DRIVER_MAX_IC_NUM,
			sizeof(struct himax_chip_detect), GFP_KERNEL);
		if (hx_self_sym_lookup.core_chip_dt == NULL) {
			E("%s: Failed to allocate core_chip_dt\n", __func__);
			return;
		}
		hx_self_sym_lookup.hx_ic_dt_num = 0;
	}

	hx_self_sym_lookup.core_chip_dt
	[hx_self_sym_lookup.hx_ic_dt_num].fp_chip_detect = detect_fp;
	hx_self_sym_lookup.hx_ic_dt_num++;
}



static void free_chip_dt_table(void)
{
	hx_release_chip_entry();
}
#endif

#if !defined(__HIMAX_HX852xH_MOD__) && !defined(__HIMAX_HX852xG_MOD__)
#if !defined(__HIMAX_HX852xJ_MOD__)

extern struct fw_operation **kp_pfw_op;
extern struct ic_operation **kp_pic_op;
extern struct flash_operation **kp_pflash_op;
extern struct driver_operation **kp_pdriver_op;
#endif
#endif

#if defined(HX_ZERO_FLASH) && defined(CONFIG_TOUCHSCREEN_HIMAX_INCELL)
extern struct zf_operation **kp_pzf_op;
extern int *kp_G_POWERONOF;
#endif

extern unsigned char *kp_IC_CHECKSUM;

#if defined(HX_EXCP_RECOVERY)
extern u8 *kp_HX_EXCP_RESET_ACTIVATE;
#endif

#if defined(HX_ZERO_FLASH) && defined(HX_CODE_OVERLAY)
#if defined(CONFIG_TOUCHSCREEN_HIMAX_INCELL)
extern uint8_t **kp_ovl_idx;
#endif
#endif

extern unsigned long *kp_FW_VER_MAJ_FLASH_ADDR;
extern unsigned long *kp_FW_VER_MIN_FLASH_ADDR;
extern unsigned long *kp_CFG_VER_MAJ_FLASH_ADDR;
extern unsigned long *kp_CFG_VER_MIN_FLASH_ADDR;
extern unsigned long *kp_CID_VER_MAJ_FLASH_ADDR;
extern unsigned long *kp_CID_VER_MIN_FLASH_ADDR;
extern uint32_t *kp_CFG_TABLE_FLASH_ADDR;
extern uint32_t *kp_CFG_TABLE_FLASH_ADDR_T;

#if defined(HX_TP_PROC_2T2R)
	// static bool *kp_Is_2T2R;
#endif

#if defined(HX_USB_DETECT_GLOBAL)
	extern void (*kp_himax_cable_detect_func)(bool force_renew);
#endif

#if defined(HX_RST_PIN_FUNC)
	extern void (*kp_himax_rst_gpio_set)(int pinnum, uint8_t value);
#endif

extern struct himax_ts_data **kp_private_ts;
extern struct himax_core_fp *kp_g_core_fp;
extern struct himax_ic_data **kp_ic_data;

#if !defined(__HIMAX_HX852xH_MOD__) && !defined(__HIMAX_HX852xG_MOD__)
	#if !defined(__HIMAX_HX852xJ_MOD__)
		extern void (*kp_himax_mcu_in_cmd_init)(void);
		extern int (*kp_himax_mcu_in_cmd_struct_init)(void);
	#else
		extern struct on_driver_operation **kp_on_pdriver_op;
		extern struct on_flash_operation **kp_on_pflash_op;
		extern void (*kp_himax_mcu_on_cmd_init)(void);
		extern int (*kp_himax_mcu_on_cmd_struct_init)(void);
	#endif
#else
	extern struct on_driver_operation **kp_on_pdriver_op;
	extern struct on_flash_operation **kp_on_pflash_op;
	extern void (*kp_himax_mcu_on_cmd_init)(void);
	extern int (*kp_himax_mcu_on_cmd_struct_init)(void);
#endif
extern void (*kp_himax_parse_assign_cmd)(uint32_t addr, uint8_t *cmd,
		int len);

extern int (*kp_himax_bus_read)(uint8_t command, uint8_t *data,
		uint32_t length, uint8_t toRetry);
extern int (*kp_himax_bus_write)(uint8_t command, uint8_t *data,
		uint32_t length, uint8_t toRetry);

extern void (*kp_himax_int_enable)(int enable);

#endif
