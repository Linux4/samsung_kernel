/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _GSP_IPC_TRUSTY_H_
#define _GSP_IPC_TRUSTY_H_

struct gsp_message {
	uint32_t cmd;
	uint8_t payload[4];
};

enum gsp_command {
	TA_SET_SECURE = 8,
	TA_SET_UNSECURE  = 9
};

#ifdef CONFIG_DRM_SPRD_GSP_IPC_TRUSTY
int gsp_tipc_init(void);
ssize_t gsp_tipc_write(void *data_ptr, size_t len);
ssize_t gsp_tipc_read(void *data_ptr, size_t max_len);
void gsp_tipc_exit(void);
#endif

#endif
