/* SPDX-License-Identifier: GPL-2.0
 *
 *  Himax Android Driver Sample Code for debug nodes
 *
 *  Copyright (C) 2018 Himax Corporation.
 *
 *  This software is licensed under the terms of the GNU General Public
 *  License version 2, as published by the Free Software Foundation, and
 *  may be copied, distributed, and modified under those terms.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef H_HIMAX_DEBUG
#define H_HIMAX_DEBUG

#include "himax_platform.h"
#include "himax_common.h"


#ifdef HX_ESD_RECOVERY
	extern	u8	HX_ESD_RESET_ACTIVATE;
	extern	int	hx_EB_event_flag;
	extern	int	hx_EC_event_flag;
	extern	int	hx_ED_event_flag;
#endif

#define HIMAX_PROC_DEBUG_LEVEL_FILE	"debug_level"
#define HIMAX_PROC_VENDOR_FILE		"vendor"
#define HIMAX_PROC_ATTN_FILE		"attn"
#define HIMAX_PROC_INT_EN_FILE		"int_en"
#define HIMAX_PROC_LAYOUT_FILE		"layout"
#define HIMAX_PROC_CRC_TEST_FILE		"CRC_test"

extern struct proc_dir_entry *himax_proc_debug_level_file;
extern struct proc_dir_entry *himax_proc_vendor_file;
extern struct proc_dir_entry *himax_proc_attn_file;
extern struct proc_dir_entry *himax_proc_int_en_file;
extern struct proc_dir_entry *himax_proc_layout_file;
extern struct proc_dir_entry *himax_proc_CRC_test_file;

int himax_touch_proc_init(void);
void himax_touch_proc_deinit(void);
extern int himax_int_en_set(void);

#define HIMAX_PROC_REGISTER_FILE	"register"
extern struct proc_dir_entry *himax_proc_register_file;
extern uint8_t byte_length;
extern uint8_t register_command[4];
extern uint8_t cfg_flag;


#define HIMAX_PROC_DIAG_FILE	"diag"
extern struct proc_dir_entry *himax_proc_diag_file;
#define HIMAX_PROC_DIAG_ARR_FILE	"diag_arr"
extern struct proc_dir_entry *himax_proc_diag_arrange_file;
extern struct file *diag_sram_fn;
extern uint8_t write_counter;
extern uint8_t write_max_count;
#define IIR_DUMP_FILE "/sdcard/HX_IIR_Dump.txt"
#define DC_DUMP_FILE "/sdcard/HX_DC_Dump.txt"
#define BANK_DUMP_FILE "/sdcard/HX_BANK_Dump.txt"
#ifdef HX_TP_PROC_2T2R
	extern uint32_t *diag_mutual_2;

	int32_t	*getMutualBuffer_2(void);
	void	setMutualBuffer_2(uint8_t x_num, uint8_t y_num);
#endif
extern int32_t *diag_mutual;
extern int32_t *diag_mutual_new;
extern int32_t *diag_mutual_old;
extern uint8_t diag_max_cnt;
extern uint8_t hx_state_info[2];
extern uint8_t diag_coor[128];
extern int32_t diag_self[100];
extern int32_t diag_self_new[100];
extern int32_t diag_self_old[100];
int32_t *getMutualBuffer(void);
int32_t *getMutualNewBuffer(void);
int32_t *getMutualOldBuffer(void);
int32_t *getSelfBuffer(void);
int32_t *getSelfNewBuffer(void);
int32_t *getSelfOldBuffer(void);
void	setMutualBuffer(uint8_t x_num, uint8_t y_num);
void	setMutualNewBuffer(uint8_t x_num, uint8_t y_num);
void	setMutualOldBuffer(uint8_t x_num, uint8_t y_num);

#define HIMAX_PROC_DEBUG_FILE	"debug"
extern struct proc_dir_entry *himax_proc_debug_file;
#define HIMAX_PROC_FW_DEBUG_FILE	"FW_debug"
extern struct proc_dir_entry *himax_proc_fw_debug_file;
#define HIMAX_PROC_DD_DEBUG_FILE	"DD_debug"
extern struct proc_dir_entry *himax_proc_dd_debug_file;
extern bool	fw_update_complete;
extern int handshaking_result;
extern unsigned char debug_level_cmd;
extern uint8_t cmd_set[8];
extern uint8_t mutual_set_flag;

#define HIMAX_PROC_FLASH_DUMP_FILE	"flash_dump"
extern struct proc_dir_entry *himax_proc_flash_dump_file;
extern int Flash_Size;
extern uint8_t *flash_buffer;
extern uint8_t g_flash_cmd;
extern uint8_t g_flash_progress;
extern bool g_flash_dump_rst; /*Fail = 0, Pass = 1*/
void setFlashBuffer(void);

enum flash_dump_prog {
	START,
	ONGOING,
	FINISHED,
};

extern uint32_t **raw_data_array;
extern uint8_t X_NUM4;
extern uint8_t Y_NUM;
extern uint8_t sel_type;

#define HIMAX_PROC_RESET_FILE		"reset"
extern struct proc_dir_entry *himax_proc_reset_file;

#define HIMAX_PROC_SENSE_ON_OFF_FILE "SenseOnOff"
extern struct proc_dir_entry *himax_proc_SENSE_ON_OFF_file;

#ifdef HX_ESD_RECOVERY
	#define HIMAX_PROC_ESD_CNT_FILE "ESD_cnt"
	extern struct proc_dir_entry *himax_proc_ESD_cnt_file;
#endif

#ifdef HX_TP_PROC_GUEST_INFO
	#define HIMAX_PROC_GUEST_INFO_FILE		"guest_info"
	extern struct proc_dir_entry *himax_proc_guest_info_file;
#endif

/* Moved from debug.c */
extern struct himax_debug *debug_data;
extern unsigned char    IC_CHECKSUM;
extern int i2c_error_count;
extern struct proc_dir_entry *himax_touch_proc_dir;

#ifdef HX_TP_PROC_GUEST_INFO
extern struct hx_guest_info *g_guest_info_data;
extern char *g_guest_info_item[];
#endif

extern int himax_input_register(struct himax_ts_data *ts);
#ifdef HX_TP_PROC_2T2R
	extern bool Is_2T2R;
#endif

#ifdef HX_RST_PIN_FUNC
	extern void himax_ic_reset(uint8_t loadconfig, uint8_t int_off);
#endif

#if defined(HX_ZERO_FLASH)
extern char *i_CTPM_firmware_name;
#endif

extern uint8_t HX_PROC_SEND_FLAG;
extern struct himax_target_report_data *g_target_report_data;
extern struct himax_report_data *hx_touch_data;
extern int g_ts_dbg;

/* Moved from debug.c end */
#endif
