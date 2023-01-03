/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _DISP_RECOVERY_H_
#define _DISP_RECOVERY_H_
#include "ddp_info.h"
#include "ddp_dsi.h"

#define GPIO_EINT_MODE	0
#define GPIO_DSI_MODE	1



/* defined in mtkfb.c should move to mtkfb.h*/
extern unsigned int islcmconnected;


void primary_display_check_recovery_init(void);
void primary_display_esd_check_enable(int enable);
unsigned int need_wait_esd_eof(void);

void external_display_check_recovery_init(void);
void external_display_esd_check_enable(int enable);

void set_esd_check_mode(unsigned int mode);
int do_lcm_vdo_lp_read(struct dsi_cmd_desc *cmd_tab, unsigned int count);
int do_lcm_vdo_lp_read_v1(struct dsi_cmd_desc *cmd_tab);
int do_lcm_vdo_lp_write(struct dsi_cmd_desc *write_table,
			unsigned int count);



#endif
