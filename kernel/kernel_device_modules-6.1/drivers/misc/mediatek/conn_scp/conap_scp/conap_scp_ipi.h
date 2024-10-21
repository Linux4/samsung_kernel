/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CONAP_SCP_IPI_H__
#define __CONAP_SCP_IPI_H__

#include "conap_scp.h"

struct conap_scp_ipi_cb {
	void (*conap_scp_ipi_msg_notify)(uint16_t drv_type, uint16_t msg_id,
			uint16_t total_sz, uint8_t *data, uint32_t this_sz);
	void (*conap_scp_ipi_ctrl_notify)(unsigned int state);
};

/* shm */
int conap_scp_shm_write(uint8_t *msg_buf, uint32_t msg_sz);
int conap_scp_shm_read(uint8_t *msg_buf, uint32_t oft, uint32_t msg_sz);

/* IPI */
unsigned int conap_scp_ipi_is_scp_ready(void);
unsigned int conap_scp_ipi_msg_sz(void);

int conap_scp_ipi_send_data(enum conap_scp_drv_type drv_type, uint16_t msg_id, uint32_t total_sz,
						uint8_t *buf, uint32_t size);
int conap_scp_ipi_send_cmd(enum conap_scp_drv_type drv_type, uint16_t msg_id,
						uint32_t p0, uint32_t p1, uint32_t p2);

bool conap_scp_ipi_shm_support(void);
int conap_scp_ipi_send_shm_cmd(enum conap_scp_drv_type drv_type, uint16_t msg_id,
					uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3);

int conap_scp_ipi_is_drv_ready(enum conap_scp_drv_type drv_type);

int conap_scp_ipi_init(struct conap_scp_ipi_cb *cb);
int conap_scp_ipi_deinit(void);

#endif
