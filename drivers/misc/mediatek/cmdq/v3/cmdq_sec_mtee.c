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

#include "cmdq_core.h"
#include "cmdq_sec_mtee.h"

void cmdq_sec_mtee_setup_context(struct cmdq_sec_mtee_context *tee)
{
	const char ta_uuid[32] = "com.mediatek.geniezone.cmdq";
	const char wsm_uuid[32] = "com.mediatek.geniezone.srv.mem";

	memset(tee, 0, sizeof(*tee));
	strncpy(tee->ta_uuid, ta_uuid, sizeof(ta_uuid));
	strncpy(tee->wsm_uuid, wsm_uuid, sizeof(wsm_uuid));
}

// TODO
#if 0
s32 cmdq_sec_init_context(struct cmdq_sec_tee_context *tee)
{
	s32 status;

	CMDQ_MSG("[SEC] enter %s\n", __func__);
#if defined(CONFIG_MICROTRUST_TEE_SUPPORT)
	while (!is_teei_ready()) {
		CMDQ_MSG("[SEC] Microtrust TEE is not ready, wait...\n");
		msleep(1000);
	}
#elif defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
	while (!is_mobicore_ready()) {
		CMDQ_MSG("[SEC] Trustonic TEE is not ready, wait...\n");
		msleep(1000);
	}
#endif
	CMDQ_LOG("[SEC]TEE is ready\n");

	status = TEEC_InitializeContext(NULL, &tee->gp_context);
	if (status != TEEC_SUCCESS)
		CMDQ_ERR("[SEC]init_context fail: status:0x%x\n", status);
	else
		CMDQ_MSG("[SEC]init_context: status:0x%x\n", status);
	return status;
}

s32 cmdq_sec_deinit_context(struct cmdq_sec_tee_context *tee)
{
	TEEC_FinalizeContext(&tee->gp_context);
	return 0;
}
#endif

s32 cmdq_sec_mtee_allocate_shared_memory(struct cmdq_sec_mtee_context *tee,
	const dma_addr_t MVABase, const u32 size)
{
	s32 status;

	tee->mem_param.size = size;
	tee->mem_param.buffer = (void *)(unsigned long)MVABase;
	status = KREE_RegisterSharedmem(tee->wsm_pHandle,
		&tee->mem_handle, &tee->mem_param);
	if (status != TZ_RESULT_SUCCESS)
		CMDQ_ERR("%s: session:%#x handle:%#x size:%#x buffer:%#x\n",
			__func__, tee->wsm_pHandle, tee->mem_handle,
			tee->mem_param.size, tee->mem_param.buffer);
	else
		CMDQ_LOG("%s: session:%#x handle:%#x size:%#x buffer:%#x\n",
			__func__, tee->wsm_pHandle, tee->mem_handle,
			tee->mem_param.size, tee->mem_param.buffer);
	return status;
}

s32 cmdq_sec_mtee_allocate_wsm(struct cmdq_sec_mtee_context *tee,
	void **wsm_buffer, u32 size, void **wsm_buf_ex, u32 size_ex)
{
	s32 status;

	if (!wsm_buffer || !wsm_buf_ex)
		return -EINVAL;

	/* region_id = 0, mapAry = NULL for continuous */
	*wsm_buffer = kzalloc(size, GFP_KERNEL);
	if (!*wsm_buffer)
		return -ENOMEM;

	tee->wsm_param.size = size;
	tee->wsm_param.buffer = (void *)(u64)virt_to_phys(*wsm_buffer);
	status = KREE_RegisterSharedmem(tee->wsm_pHandle,
		&tee->wsm_handle, &tee->wsm_param);
	if (status != TZ_RESULT_SUCCESS) {
		CMDQ_ERR("%s: session:%#x handle:%#x size:%#x buffer:%p:%#x\n",
			__func__, tee->wsm_pHandle, tee->wsm_handle,
			tee->wsm_param.size, *wsm_buffer, *wsm_buffer);
		return status;
	}
	CMDQ_LOG("%s: session:%#x handle:%#x size:%#x buffer:%p:%#x\n",
		__func__, tee->wsm_pHandle, tee->wsm_handle,
		tee->wsm_param.size, *wsm_buffer, *wsm_buffer);

	*wsm_buf_ex = kzalloc(size_ex, GFP_KERNEL);
	if (!*wsm_buf_ex)
		return -ENOMEM;

	tee->wsm_ex_param.size = size_ex;
	tee->wsm_ex_param.buffer = (void *)(u64)virt_to_phys(*wsm_buf_ex);
	status = KREE_RegisterSharedmem(tee->wsm_pHandle,
		&tee->wsm_ex_handle, &tee->wsm_ex_param);
	if (status != TZ_RESULT_SUCCESS)
		CMDQ_ERR("%s: session:%#x handle:%#x size:%#x buffer:%p:%#x\n",
			__func__, tee->wsm_pHandle, tee->wsm_ex_handle,
			tee->wsm_ex_param.size, *wsm_buf_ex, *wsm_buf_ex);
	else
		CMDQ_LOG("%s: session:%#x handle:%#x size:%#x buffer:%p:%#x\n",
			__func__, tee->wsm_pHandle, tee->wsm_ex_handle,
			tee->wsm_ex_param.size, *wsm_buf_ex, *wsm_buf_ex);
	return status;
}

s32 cmdq_sec_mtee_free_wsm(struct cmdq_sec_mtee_context *tee,
	void **wsm_buffer)
{
	if (!wsm_buffer)
		return -EINVAL;

	KREE_UnregisterSharedmem(tee->wsm_pHandle, tee->wsm_handle);
	kfree(*wsm_buffer);
	*wsm_buffer = NULL;
	return 0;
}

s32 cmdq_sec_mtee_open_session(struct cmdq_sec_mtee_context *tee,
	void *wsm_buffer)
{
	s32 status;

	status = KREE_CreateSession(tee->ta_uuid, &tee->pHandle);
	if (status != TZ_RESULT_SUCCESS) {
		CMDQ_ERR("%s:%s:%d\n", __func__, tee->ta_uuid, status);
		return status;
	}
	CMDQ_LOG("%s: tee:%p ta_uuid:%s session:%#x\n",
		__func__, tee, tee->ta_uuid, tee->pHandle);

	status = KREE_CreateSession(tee->wsm_uuid, &tee->wsm_pHandle);
	if (status != TZ_RESULT_SUCCESS) {
		CMDQ_ERR("%s:%s:%d\n", __func__, tee->ta_uuid, status);
		return status;
	}
	CMDQ_LOG("%s: tee:%p ta_uuid:%s session:%#x\n",
		__func__, tee, tee->wsm_uuid, tee->wsm_pHandle);
	return status;
}

s32 cmdq_sec_mtee_close_session(struct cmdq_sec_mtee_context *tee)
{
	KREE_CloseSession(tee->wsm_pHandle);
	return KREE_CloseSession(tee->pHandle);
}

s32 cmdq_sec_mtee_execute_session(struct cmdq_sec_mtee_context *tee,
	u32 cmd, s32 timeout_ms, bool share_mem_ex)
{
	s32 status;
	u32 types = TZ_ParamTypes3(TZPT_VALUE_INOUT,
		share_mem_ex ? TZPT_VALUE_INOUT : TZPT_NONE,
		cmd == 4 ? TZPT_VALUE_INOUT : TZPT_NONE); // TODO
	union MTEEC_PARAM param[4];

	param[0].value.a = tee->wsm_handle;
	param[0].value.b = tee->wsm_param.size;
	param[1].value.a = tee->wsm_ex_handle;
	param[1].value.b = tee->wsm_ex_param.size;
	param[2].value.a = tee->mem_handle;
	param[2].value.b = (u32)(unsigned long)tee->mem_param.buffer;
	CMDQ_LOG(
		"%s: tee:%p session:%#x cmd:%u timeout:%d mem_ex:%d types:%#x wsm:%#x:%#x ex:%#x:%#x mem:%#x:%#x\n",
		__func__,
		tee, tee->pHandle, cmd, timeout_ms, share_mem_ex, types,
		param[0].value.a, param[0].value.b,
		param[1].value.a, param[1].value.b,
		param[2].value.a, param[2].value.b);

	status = KREE_TeeServiceCall(tee->pHandle, cmd, types, param);
	if (status != TZ_RESULT_SUCCESS)
		CMDQ_ERR("%s:%d cmd:%u\n", __func__, status, cmd);
	else
		CMDQ_MSG("%s:%d cmd:%u\n", __func__, status, cmd);
	return status;
}
