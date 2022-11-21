/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CMDQ_SEC_GP_H__
#define __CMDQ_SEC_GP_H__

#include <linux/types.h>
#include <linux/delay.h>

#include "tee_client_api.h"
#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
#include "teei_client_main.h"
#endif
#if defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
#include "mobicore_driver_api.h"
#endif

#if defined(CONFIG_TEEGRIS_TEE_SUPPORT)
	#define TYPE_STRUCT
#else
	#define TYPE_STRUCT struct
#endif
/* context for tee vendor */
struct cmdq_sec_tee_context {
	/* Universally Unique Identifier of secure tl/dr */
	TYPE_STRUCT TEEC_UUID uuid;
	TYPE_STRUCT TEEC_Context gp_context; /* basic context */
	TYPE_STRUCT TEEC_Session session; /* session handle */
	TYPE_STRUCT TEEC_SharedMemory shared_mem; /* shared memory */
	TYPE_STRUCT TEEC_SharedMemory shared_mem_ex; /* shared memory */
	TYPE_STRUCT TEEC_SharedMemory shared_mem_ex2; /* shared memory */
};

void cmdq_sec_setup_tee_context(struct cmdq_sec_tee_context *tee);
s32 cmdq_sec_init_context(struct cmdq_sec_tee_context *tee);
s32 cmdq_sec_allocate_wsm(struct cmdq_sec_tee_context *tee,
	void **wsm_buffer, u32 size, void **wsm_buf_ex, u32 size_ex,
	void **wsm_buf_ex2, u32 size_ex2);
s32 cmdq_sec_open_session(struct cmdq_sec_tee_context *tee, void *wsm_buffer);
s32 cmdq_sec_execute_session(struct cmdq_sec_tee_context *tee,
	u32 cmd, s32 timeout_ms, bool share_mem_ex, bool share_mem_ex2);

int m4u_sec_init(void);

#endif	/* __CMDQ_SEC_GP_H__ */
