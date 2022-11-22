/*
 * =================================================================
 *
 *	Description:  samsung display debug common file
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */
/*
<one line to give the program's name and a brief idea of what it does.>
Copyright (C) 2015, Samsung Electronics. All rights reserved.

*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
*/
#ifndef _SAMSUNG_DSI_PANEL_DEBUG_H_
#define _SAMSUNG_DSI_PANEL_DEBUG_H_

#include "ss_dsi_panel_common.h"
#ifdef CONFIG_SEC_DEBUG
#include <linux/qcom/sec_debug.h>
#include <linux/qcom/sec_debug_partition.h>
#endif

struct samsung_display_driver_data;

/**** SAMSUNG XLOG *****/
#define SS_XLOG_ENTRY 256
#define SS_XLOG_BUF_MAX 128
#define SS_XLOG_MAX_DATA 7
#define SS_XLOG_BUF_ALIGN_TIME 14
#define SS_XLOG_BUF_ALIGN_NAME 32
#define SS_XLOG_START 0x1111
#define SS_XLOG_FINISH 0xFFFF
#define SS_XLOG_PANIC_DBG_LENGH 256
#define SS_XLOG_DPCI_LENGTH (700 - 1)
#define DATA_LIMITER (-1)

enum mdss_samsung_xlog_flag {
	SS_XLOG_DEFAULT,
	SS_XLOG_BIGDATA,
	SS_XLOG_MAX
};

struct ss_tlog {
	int pid;
	s64 time;
	u32 data[SS_XLOG_MAX_DATA];
	u32 data_cnt;
	const char *name;
};

/* PANEL DEBUG FUCNTION */
void mdss_samsung_xlog(const char *name, int flag, ...);
void mdss_samsung_dump_regs(void);
void mdss_samsung_dump_xlog(void);
void mdss_samsung_dsi_dump_regs(struct samsung_display_driver_data *vdd, int dsi_num);
void mdss_samsung_store_xlog_panic_dbg(void);
void mdss_mdp_underrun_dump_info(struct samsung_display_driver_data *vdd);
int mdss_samsung_read_rddpm(struct samsung_display_driver_data *vdd);
int mdss_samsung_dsi_te_check(struct samsung_display_driver_data *vdd);
void samsung_image_dump_worker(struct samsung_display_driver_data *vdd, struct work_struct *work);
void samsung_mdss_image_dump(void);
int mdss_sasmung_panel_debug_init(struct samsung_display_driver_data *vdd);

int mdss_samsung_read_rddpm(struct samsung_display_driver_data *vdd);
int mdss_samsung_read_rddsm(struct samsung_display_driver_data *vdd);
int mdss_samsung_read_errfg(struct samsung_display_driver_data *vdd);
int mdss_samsung_read_dsierr(struct samsung_display_driver_data *vdd);
int mdss_samsung_read_self_diag(struct samsung_display_driver_data *vdd);

void ss_inc_ftout_debug(char *name);

extern bool read_debug_partition(enum debug_partition_index index, void *value);
extern bool write_debug_partition(enum debug_partition_index index, void *value);

#define SS_XLOG(...) mdss_samsung_xlog(__func__, SS_XLOG_DEFAULT, \
		##__VA_ARGS__, DATA_LIMITER)
#define SS_XLOG_BG(...) mdss_samsung_xlog(__func__, SS_XLOG_BIGDATA, \
		##__VA_ARGS__, DATA_LIMITER)

#endif
