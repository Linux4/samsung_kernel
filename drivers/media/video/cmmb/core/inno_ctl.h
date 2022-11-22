#ifndef __INNO_CTL_H__
#define __INNO_CTL_H__

/*
 * Copyright (C) 2010 Innofidei Corporation
 * Author:      sean <zhaoguangyu@innofidei.com>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * 
 */

int get_fw_version(void *data);
int set_frequency(void *data);
int get_frequency(void *data);
int set_ch_config(void *data);
int get_ch_config(void *data);
int get_sys_status(void *data);
int get_err_info(void *data);
int get_chip_id(void *data);

#ifdef IF206
int get_ca_card_sn (void* data);
int set_ca_table (void* data);
int set_emm_channel (void* data);
#endif

int get_ca_err_state (void* data);
int enable_cw_detect_mode(void *data);
int get_cw_freq_offset(void *data);
int set_system_sleep(void *data);

int inno_ctl_init(struct inno_demod *demod);
void inno_ctl_exit(void);
#endif
