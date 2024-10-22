/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

#ifndef __MTK_CUST_H
#define __MTK_CUST_H

#include "mtk_debug.h"
#include "mtk_panel_ext.h"

#define SET_LCM(handle, msg, ...) set_lcm_default_parameter(handle, msg, (f_args) {__VA_ARGS__})

/* Description
 *  SET_LCM(msg) -> blocking method
 *  SET_LCM(msg, SET_LCM_BLOCKING) -> blocking method
 *  set_lcm(msg) -> blocking method
 *  SET_LCM(msg, SET_LCM_NONBLOCKING) -> non-blocking method
 */

typedef struct {
	int i;
} f_args;

int set_lcm(struct cmdq_pkt *handle, struct mtk_ddic_dsi_msg *cmd_msg);
int read_lcm(struct cmdq_pkt *handle, struct mtk_ddic_dsi_msg *cmd_msg);
int mtk_wait_frame_start(unsigned int timeout);
int mtk_wait_frame_done(unsigned int timeout);
int set_lcm_default_parameter(struct cmdq_pkt *handle, struct mtk_ddic_dsi_msg *cmd_msg, f_args in);
#endif
