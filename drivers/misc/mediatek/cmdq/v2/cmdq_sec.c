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

#include <linux/slab.h>
#include <linux/delay.h>

#include "cmdq_sec.h"
#include "cmdq_def.h"
#include "cmdq_virtual.h"
#include "cmdq_device.h"
#ifdef CMDQ_MET_READY
#include <linux/met_drv.h>
#endif

static DEFINE_MUTEX(gCmdqSecExecLock);	/* lock to protect atomic secure task execution */
static DEFINE_MUTEX(gCmdqSecProfileLock);	/* ensure atomic enable/disable secure path profile */
static struct list_head gCmdqSecContextList;	/* secure context list. note each porcess has its own sec context */

#if defined(CMDQ_SECURE_PATH_SUPPORT)
/* blowfish interface */
#include <tzdev/kernel_api.h>
#include <tzdev/tzdev.h>
#include "sysdep.h"
#include "tzdev.h"
#include "tz_mem.h"

/* sectrace interface */
#ifdef CMDQ_SECTRACE_SUPPORT
#include <linux/sectrace.h>
#endif
/* secure path header */
#include "cmdqsectl_api.h"

/* #define CMDQ_DR_UUID { { 2, 0xb, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } } */
/* #define CMDQ_TL_UUID { { 9, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } } */
#if defined(CMDQ_SECURE_PATH_SUPPORT)
/*
 * Blowfish CMDQ TA/TDriver uuid
 * TA:		00000000-4D54-4B5F-4246-434D44515441
 * TDriver:	00000000-4D54-4B5F-4246-434D44445256
 */
static struct tz_uuid CMDQ_TL_UUID = {
	.time_low = 0x00000000,
	.time_mid = 0x4D54,
	.time_hi_and_version = 0x4B5F,
	.clock_seq_and_node = {
		0x42, 0x46, 0x43, 0x4D, 0x44, 0x51, 0x54, 0x41
	}
};

#define IWNOTIFY_FLAG_CMDQ TZ_IWNOTIFY_OEM_NOTIFICATION_FLAG_8

static struct cmdqSecContextStruct *gCmdqSecContextHandle;	/* secure context to cmdqSecTL */
#endif

#ifdef CMDQ_SECTRACE_SUPPORT
/* sectrace */
struct mc_bulk_map gCmdqSectraceMappedInfo;	/* sectrace log buffer, which mapped between NWd and SWd */
#endif

/* internal control */

/* Set 1 to open once for each process context, because of below reasons:
 * 1. open is too slow (major)
 * 2. we entry secure world for config and trigger GCE, and wait in normal world
 */
#define CMDQ_OPEN_SESSION_ONCE (1)

/********************************************************************************
 * operator API
 *******************************************************************************/
s32 cmdq_sec_mem_register(void *wsm, u32 wms_size, s32 *shmem_id_out)
{
	s32 status = 0;

	if (!shmem_id_out) {
		CMDQ_ERR("[SEC]wsm id out cannot NULL\n");
		return -1;
	}

	/* register Blowfish world share memory */
	*shmem_id_out = tzdev_mem_register(wsm, (unsigned long)wms_size, 1);
	if (*shmem_id_out < 0) {
		/* in fail case it is error not id */
		status = *shmem_id_out;
		CMDQ_ERR("[SEC]wsm register failed, result:%d\n", status);
	}

	CMDQ_MSG("[SEC]tzdev_mem_register addr:%p size:%u id:%d\n",
		wsm, wms_size, *shmem_id_out);

	return status;
}

s32 cmdq_sec_mem_release(s32 shmem_id)
{
	s32 status = tzdev_mem_release(shmem_id);

	if (status < 0) {
		/* print error but do nothing */
		CMDQ_ERR("[SEC]wsm release failed share mem id:%d status:%d\n", shmem_id, status);
	}

	CMDQ_MSG("[SEC]tzdev_mem_release id:%d\n", shmem_id);
	return status;
}

static int cmdq_sec_cmd_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
	CMDQ_VERBOSE("[SEC]Sec notify action:%lu\n", action);
	complete(&gCmdqSecContextHandle->iwc_complete);
	return 0;
}

s32 cmdq_sec_notify_chain_register(struct notifier_block *kcmd_notifier_ptr)
{
	s32 status;

	status = tz_iwnotify_chain_register(IWNOTIFY_FLAG_CMDQ, kcmd_notifier_ptr);
	if (status < 0) {
		/* print error but do nothing */
		CMDQ_ERR("[SEC]Register notifier fail status:%d\n", status);
	}

	return status;
}

s32 cmdq_sec_notify_chain_unregister(struct notifier_block *kcmd_notifier_ptr)
{
	s32 status = tz_iwnotify_chain_unregister(0, kcmd_notifier_ptr);

	if (status < 0) {
		/* print error but do nothing */
		CMDQ_ERR("[SEC]Unregister notifier fail status:%d\n", status);
	}

	return status;
}

s32 cmdq_sec_trustzone_open(const struct tz_uuid *uuid, s32 *client_id_out)
{
	int32_t status = -1;
	int retry_cnt = 0, max_retry = 30;
	s32 client_id = 0;

	do {
		if (!tzdev_is_up()) {
			CMDQ_ERR("[SEC]trustzone not ready\n");
			msleep_interruptible(2000);
			retry_cnt++;
			continue;
		}

		client_id = tzdev_kapi_open(uuid);
		if (client_id < 0) {
			CMDQ_ERR("[SEC]tz open failed, status:%d uuid:%08x tzup:%s\n",
				client_id, uuid->time_low, tzdev_is_up() ? "True" : "False");
			msleep_interruptible(2000);
			retry_cnt++;
			continue;
		}

		status = 0;
		break;
	} while (retry_cnt < max_retry);

	if (retry_cnt >= max_retry) {
		/* print error message */
		CMDQ_ERR("[SEC]tz open fail: status:%d clientid:%d, retry:%d\n",
			status, client_id, retry_cnt);
	} else {
		CMDQ_MSG("[SEC]tz open: status:%d clientid:%d, retry:%d\n",
			status, client_id, retry_cnt);
	}

	if (client_id_out)
		*client_id_out = client_id;

	return status;
}

s32 cmdq_sec_trustzone_close(s32 client_id)
{
	s32 status = tzdev_kapi_close(client_id);

	if (status < 0) {
		/* print error but do nothing */
		CMDQ_ERR("[SEC]tz close failed client:%d status:%d\n", client_id, status);
	}

	return status;
}

s32 cmdq_sec_mem_grant(s32 client_id, s32 shmem_id)
{
	s32 status = 0;

	CMDQ_MSG("[SEC]grant mem clientid:%d shmemid:%d\n", client_id, shmem_id);
	tzdev_kapi_mem_grant(client_id, shmem_id);

	if (status < 0) {
		/* print error but do nothing */
		CMDQ_ERR("[SEC]grant mem failed status:%d clientid:%d shmemid:%d\n",
			status, client_id, shmem_id);
	}
	return status;
}

s32 cmdq_sec_mem_ungrant(s32 client_id, s32 shmem_id)
{
	s32 status = tzdev_kapi_mem_revoke(client_id, shmem_id);

	if (status < 0) {
		/* print error but do nothing */
		CMDQ_ERR("[SEC]Revoke mem failed status:%d clientid:%d shmemid:%d\n",
			status, client_id, shmem_id);
	}
	return status;
}

int32_t cmdq_sec_init_session_unlocked(const struct tz_uuid *uuid, void *wsm_ptr,
	uint32_t wsmSize, struct notifier_block *kcmd_notifier_ptr,
	s32 *client_id_out, s32 *shmem_id_out,
	enum CMDQ_IWC_STATE_ENUM *iwc_state_out)
{
	int32_t status = 0;

	CMDQ_MSG("[SEC]-->SESSION_INIT: iwcState:%d\n", *iwc_state_out);

	do {
#if CMDQ_OPEN_SESSION_ONCE
		if (*iwc_state_out >= IWC_SEC_OPENED) {
			CMDQ_MSG("SESSION_INIT: already opened\n");
			break;
		}

		CMDQ_MSG("[SEC]SESSION_INIT: open new session:%d\n", *iwc_state_out);
#endif
		CMDQ_VERBOSE("[SEC]SESSION_INIT: wsmSize[%d]\n", wsmSize);

		CMDQ_PROF_START(current->pid, "CMDQ_SEC_INIT");

		if (*iwc_state_out < IWC_MEM_REGISTERED) {
			/* register iwc notify */
			status = cmdq_sec_mem_register(wsm_ptr, wsmSize, shmem_id_out);
			if (status < 0)
				break;
			*iwc_state_out = IWC_MEM_REGISTERED;
		}

		if (*iwc_state_out < IWC_NOTIFY_REGISTERED) {
			/* register iwc notify */
			status = cmdq_sec_notify_chain_register(kcmd_notifier_ptr);
			if (status < 0)
				break;
			*iwc_state_out = IWC_NOTIFY_REGISTERED;
		}

		if (*iwc_state_out < IWC_TRUST_OPENED) {
			/* register iwc notify */
			status = cmdq_sec_trustzone_open(uuid, client_id_out);
			if (status < 0)
				break;
			*iwc_state_out = IWC_TRUST_OPENED;
		}

		if (*iwc_state_out < IWC_MEM_GRANT) {
			/* register iwc notify */
			status = cmdq_sec_mem_grant(*client_id_out, *shmem_id_out);
			if (status < 0)
				break;
			*iwc_state_out = IWC_MEM_GRANT;
		}

		*iwc_state_out = IWC_SEC_OPENED;

		CMDQ_PROF_END(current->pid, "CMDQ_SEC_INIT");
	} while (0);

	CMDQ_MSG("[SEC]<--SESSION_INIT status:%d\n", status);
	return status;
}

int32_t cmdq_sec_fill_iwc_command_basic_unlocked(int32_t iwc_command, void *_pTask, int32_t thread,
						 void *_pIwc)
{
	struct iwcCmdqMessage_t *pIwc;

	pIwc = (struct iwcCmdqMessage_t *) _pIwc;

	/* specify command id only, don't care other other */
	memset(pIwc, 0x0, sizeof(struct iwcCmdqMessage_t));
	pIwc->cmd = iwc_command;

	/* medatada: debug config */
	pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0);
	pIwc->debug.enableProfile = cmdq_core_profile_enabled();

	return 0;
}

int32_t cmdq_sec_fill_iwc_command_msg_unlocked(int32_t iwc_command, void *_pTask, int32_t thread,
					       void *_pIwc)
{
	int32_t status;

	const struct TaskStruct *pTask = (struct TaskStruct *) _pTask;
	struct iwcCmdqMessage_t *pIwc;
	/* cmdqSecDr will insert some instr */
	const uint32_t reservedCommandSize = 4 * CMDQ_INST_SIZE;
	struct CmdBufferStruct *cmd_buffer = NULL;

	/* check task first */
	if (!pTask) {
		CMDQ_ERR("[SEC]SESSION_MSG: Unable to fill message by empty task.\n");
		return -EFAULT;
	}

	status = 0;
	pIwc = (struct iwcCmdqMessage_t *) _pIwc;

	/* check command size first */
	if ((pTask->commandSize + reservedCommandSize) > CMDQ_TZ_CMD_BLOCK_SIZE) {
		CMDQ_ERR("[SEC]SESSION_MSG: pTask %p commandSize %d > %d\n",
			 pTask, pTask->commandSize, CMDQ_TZ_CMD_BLOCK_SIZE);
		return -EFAULT;
	}

	CMDQ_MSG("[SEC]-->SESSION_MSG: cmdId[%d]\n", iwc_command);

	/* fill message buffer for inter world communication */
	memset(pIwc, 0x0, sizeof(struct iwcCmdqMessage_t));
	pIwc->cmd = iwc_command;

	/* metadata */
	pIwc->command.metadata.enginesNeedDAPC = pTask->secData.enginesNeedDAPC;
	pIwc->command.metadata.enginesNeedPortSecurity = pTask->secData.enginesNeedPortSecurity;

	if (thread != CMDQ_INVALID_THREAD) {
		uint8_t *current_va = (uint8_t *)pIwc->command.pVABase;

		/* basic data */
		pIwc->command.scenario = pTask->scenario;
		pIwc->command.thread = thread;
		pIwc->command.priority = pTask->priority;
		pIwc->command.engineFlag = pTask->engineFlag;
		pIwc->command.hNormalTask = 0LL | ((unsigned long)pTask);

		list_for_each_entry(cmd_buffer, &pTask->cmd_buffer_list, listEntry) {
			bool is_last = list_is_last(&cmd_buffer->listEntry, &pTask->cmd_buffer_list);
			uint32_t copy_size = is_last ?
				CMDQ_CMD_BUFFER_SIZE - pTask->buf_available_size :
				CMDQ_CMD_BUFFER_SIZE - CMDQ_INST_SIZE;
			uint32_t *end_va = (u32 *)(current_va + copy_size);

			memcpy(current_va, (cmd_buffer->pVABase), (copy_size));

			/* we must reset the jump inst since now buffer is continues */
			if (is_last && ((end_va[-1] >> 24) & 0xff) == CMDQ_CODE_JUMP &&
				(end_va[-1] & 0x1) == 1) {
				end_va[-1] = CMDQ_CODE_JUMP << 24;
				end_va[-2] = 0x8;
			}

			current_va += copy_size;
		}

		pIwc->command.commandSize = (uint32_t)(current_va - (uint8_t *)pIwc->command.pVABase);

		/* cookie */
		pIwc->command.waitCookie = pTask->secData.waitCookie;
		pIwc->command.resetExecCnt = pTask->secData.resetExecCnt;

		CMDQ_MSG("[SEC]SESSION_MSG: task: 0x%p, thread: %d, size: %d, scenario: %d, flag: 0x%016llx\n",
				   pTask, thread, pTask->commandSize, pTask->scenario, pTask->engineFlag);

		CMDQ_VERBOSE("[SEC]SESSION_MSG: addrList[%d][0x%p]\n",
			     pTask->secData.addrMetadataCount,
			     CMDQ_U32_PTR(pTask->secData.addrMetadatas));
		if (pTask->secData.addrMetadataCount > 0) {
			pIwc->command.metadata.addrListLength = pTask->secData.addrMetadataCount;
			memcpy((pIwc->command.metadata.addrList), CMDQ_U32_PTR(pTask->secData.addrMetadatas),
			       (pTask->secData.addrMetadataCount) * sizeof(struct iwcCmdqAddrMetadata_t));
		}

		/* medatada: debug config */
		pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0);
		pIwc->debug.enableProfile = cmdq_core_profile_enabled();
	} else {
		/* relase resource, or debug function will go here */
		CMDQ_VERBOSE("[SEC]-->SESSION_MSG: no task, cmdId[%d]\n", iwc_command);
		pIwc->command.commandSize = 0;
		pIwc->command.metadata.addrListLength = 0;
	}

	CMDQ_MSG("[SEC]<--SESSION_MSG[%d]\n", status);
	return status;
}

/* TODO: when do release secure command buffer */
int32_t cmdq_sec_fill_iwc_resource_msg_unlocked(int32_t iwc_command, void *_pTask, int32_t thread,
						void *_pIwc)
{
	struct iwcCmdqMessage_t *pIwc;
	struct cmdqSecSharedMemoryStruct *pSharedMem;

	pSharedMem = cmdq_core_get_secure_shared_memory();
	if (pSharedMem == NULL) {
		CMDQ_ERR("FILL:RES, NULL shared memory\n");
		return -EFAULT;
	}

	if (pSharedMem && pSharedMem->pVABase == NULL) {
		CMDQ_ERR("FILL:RES, %p shared memory has not init\n", pSharedMem);
		return -EFAULT;
	}

	pIwc = (struct iwcCmdqMessage_t *) _pIwc;
	memset(pIwc, 0x0, sizeof(struct iwcCmdqMessage_t));

	pIwc->cmd = iwc_command;
	pIwc->pathResource.shareMemoyPA = 0LL | (pSharedMem->MVABase);
	pIwc->pathResource.size = pSharedMem->size;
#if defined(CMDQ_SECURE_PATH_NORMAL_IRQ) || defined(CMDQ_SECURE_PATH_HW_LOCK)
	pIwc->pathResource.useNormalIRQ = 1;
#else
	pIwc->pathResource.useNormalIRQ = 0;
#endif

	CMDQ_MSG("FILL:RES, shared memory:%pa(0x%llx), size:%d\n",
		 &(pSharedMem->MVABase), pIwc->pathResource.shareMemoyPA, pSharedMem->size);

	/* medatada: debug config */
	pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0);
	pIwc->debug.enableProfile = cmdq_core_profile_enabled();

	return 0;
}

int32_t cmdq_sec_fill_iwc_cancel_msg_unlocked(int32_t iwc_command, void *task_ptr, int32_t thread,
					      void *iwc_msg_ptr)
{
	const struct TaskStruct *task = (struct TaskStruct *)task_ptr;
	struct iwcCmdqMessage_t *iwc_msg;

	iwc_msg = (struct iwcCmdqMessage_t *)iwc_msg_ptr;
	memset(iwc_msg, 0x0, sizeof(*iwc_msg));

	iwc_msg->cmd = iwc_command;
	iwc_msg->cancelTask.waitCookie = task->secData.waitCookie;
	iwc_msg->cancelTask.thread = thread;

	/* medatada: debug config */
	iwc_msg->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0);
	iwc_msg->debug.enableProfile = cmdq_core_profile_enabled();

	CMDQ_LOG("FILL:CANCEL_TASK: task:%p thread:%d cookie:%d\n",
		 task, thread, task->secData.waitCookie);

	return 0;
}

int32_t cmdq_sec_execute_session_unlocked(struct cmdqSecContextStruct *handle,
	int32_t timeout_ms)
{
	s32 status = 0, wait_ret = 0;

	CMDQ_PROF_START(current->pid, "CMDQ_SEC_EXE");

	do {
		/* init completion before enting secure world */
		init_completion(&handle->iwc_complete);

		/* send message to secure world */
		status = tzdev_kapi_send(handle->client_id,
			&handle->iwc_cmd, sizeof(handle->iwc_cmd));
		if (status < 0) {
			CMDQ_ERR("[SEC]EXEC: tz send fail status:%d\n", status);
			break;
		}

		CMDQ_MSG("[SEC]EXEC: statue:%d\n", status);

		handle->state = IWC_SES_TRANSACTED;

		/* wait respond */
		wait_ret = wait_for_completion_interruptible_timeout(&handle->iwc_complete,
			timeout_ms);
		if (wait_ret <= 0) {
			CMDQ_ERR("[SEC]EXEC: wait complete timed out or interrupted status:%d\n",
				status);
			status = -ETIMEDOUT;
			break;
		}

		handle->state = IWC_SES_ON_TRANSACTED;
	} while (0);

	CMDQ_PROF_END(current->pid, "CMDQ_SEC_EXE");

	return status;
}

void cmdq_sec_deinit_session_unlocked(const enum CMDQ_IWC_STATE_ENUM iwc_state,
	struct notifier_block *kcmd_notifier_ptr, s32 client_id, s32 shmem_id)
{
	CMDQ_MSG("[SEC]-->SESSION_DEINIT\n");

	switch (iwc_state) {
	case IWC_SES_ON_TRANSACTED:
	case IWC_SES_TRANSACTED:
	case IWC_SES_MSG_PACKAGED:
		/* continue next clean-up */
	case IWC_SEC_OPENED:
		/* continue next clean-up */
	case IWC_MEM_GRANT:
		cmdq_sec_mem_ungrant(client_id, shmem_id);
		/* continue next clean-up */
	case IWC_TRUST_OPENED:
		cmdq_sec_trustzone_close(client_id);
		/* continue next clean-up */
	case IWC_NOTIFY_REGISTERED:
		cmdq_sec_notify_chain_unregister(kcmd_notifier_ptr);
		/* Fall through */
	case IWC_MEM_REGISTERED:
		cmdq_sec_mem_release(shmem_id);
		/* Fall through */
	case IWC_INIT:
		break;
	default:
		break;
	}

	CMDQ_MSG("[SEC]<--SESSION_DEINIT\n");
}

/********************************************************************************
 * context handle
 *******************************************************************************/
s32 cmdq_sec_setup_context_session(struct cmdqSecContextStruct *handle)
{
	s32 status = 0;

	/* init secure session */
	status = cmdq_sec_init_session_unlocked(&handle->uuid,
		handle->iwcMessage,
		handle->iwc_msg_size,
		&handle->kcmd_notifier,
		&handle->client_id,
		&handle->iwc_cmd.shmem_id,
		&handle->state);
	CMDQ_MSG("SEC_SETUP: status:%d clientid:%d shmemid:%d state:%d\n",
		status, handle->client_id, handle->iwc_cmd.shmem_id,
		(u32)handle->state);
	return status;
}


void cmdq_sec_handle_attach_status(struct TaskStruct *pTask, uint32_t iwc_command,
	const struct iwcCmdqMessage_t *pIwc, int32_t sec_status_code, char **dispatch_mod_ptr)
{
	int index = 0;
	const struct iwcCmdqSecStatus_t *secStatus = NULL;

	if (!pIwc || !dispatch_mod_ptr)
		return;

	/* assign status ptr to print without task */
	secStatus = &pIwc->secStatus;

	if (pTask) {
		if (pTask->secStatus) {
			if (iwc_command == CMD_CMDQ_TL_CANCEL_TASK) {
				const struct iwcCmdqSecStatus_t *last_sec_status = pTask->secStatus;

				/* cancel task uses same errored task thus secStatus may exist */
				CMDQ_ERR(
					"Last secure status: %d step: 0x%08x args: 0x%08x 0x%08x 0x%08x 0x%08x dispatch: %s task: 0x%p\n",
					last_sec_status->status, last_sec_status->step,
					last_sec_status->args[0], last_sec_status->args[1],
					last_sec_status->args[2], last_sec_status->args[3],
					last_sec_status->dispatch, pTask);
			} else {
				/* task should not send to secure twice, aee it */
				CMDQ_AEE("CMDQ", "Last secure status still exists, task: 0x%p command: %u\n",
					pTask, iwc_command);
			}
			kfree(pTask->secStatus);
			pTask->secStatus = NULL;
		}

		pTask->secStatus = kzalloc(sizeof(struct iwcCmdqSecStatus_t), GFP_KERNEL);
		if (pTask->secStatus) {
			memcpy(pTask->secStatus, &pIwc->secStatus, sizeof(struct iwcCmdqSecStatus_t));
			secStatus = pTask->secStatus;
		}
	}

	if (secStatus->status != 0 || sec_status_code != 0) {
		/* secure status may contains debug information */
		CMDQ_ERR(
			"Secure status: %d (%d) step: 0x%08x args: 0x%08x 0x%08x 0x%08x 0x%08x dispatch: %s task: 0x%p\n",
			secStatus->status, sec_status_code, secStatus->step,
			secStatus->args[0], secStatus->args[1],
			secStatus->args[2], secStatus->args[3],
			secStatus->dispatch, pTask);
		for (index = 0; index < secStatus->inst_index; index += 2) {
			CMDQ_ERR("Secure instruction %d: 0x%08x:%08x\n", (index / 2),
				secStatus->sec_inst[index],
				secStatus->sec_inst[index+1]);
		}

		switch (secStatus->status) {
		case -CMDQ_ERR_ADDR_CONVERT_HANDLE_2_PA:
			*dispatch_mod_ptr = "TEE";
			break;
		case -CMDQ_ERR_ADDR_CONVERT_ALLOC_MVA:
		case -CMDQ_ERR_ADDR_CONVERT_ALLOC_MVA_N2S:
			switch (pIwc->command.thread) {
			case CMDQ_THREAD_SEC_PRIMARY_DISP:
			case CMDQ_THREAD_SEC_SUB_DISP:
				*dispatch_mod_ptr = "DISP";
				break;
			case CMDQ_THREAD_SEC_MDP:
				*dispatch_mod_ptr = "MDP";
				break;
			}
			break;
		}
	}
}

s32 cmdq_sec_handle_session_reply_unlocked(struct cmdqSecContextStruct *handle,
	const int32_t iwc_command, struct TaskStruct *task, void *data)
{
	s32 status = 0;
	struct cmdqSecCancelTaskResultStruct *cancel_result = NULL;
	struct iwcCmdqMessage_t *iwc_msg = (struct iwcCmdqMessage_t *)handle->iwcMessage;

	status = tzdev_kapi_recv(handle->client_id, &handle->iwc_cmd, sizeof(handle->iwc_cmd));
	if (status < 0) {
		CMDQ_ERR("[SEC]RECV: receive failed status:%d clientid:%u\n",
			status, handle->client_id);
		return -1;
	}

	/* get secure task execution result */
	status = iwc_msg->rsp;

	if (iwc_command == CMD_CMDQ_TL_CANCEL_TASK) {
		cancel_result = (struct cmdqSecCancelTaskResultStruct *)data;
		if (cancel_result) {
			cancel_result->throwAEE = iwc_msg->cancelTask.throwAEE;
			cancel_result->hasReset = iwc_msg->cancelTask.hasReset;
			cancel_result->irqFlag = iwc_msg->cancelTask.irqFlag;
			cancel_result->errInstr[0] = iwc_msg->cancelTask.errInstr[0];	/* argB */
			cancel_result->errInstr[1] = iwc_msg->cancelTask.errInstr[1];	/* argA */
			cancel_result->regValue = iwc_msg->cancelTask.regValue;
			cancel_result->pc = iwc_msg->cancelTask.pc;
		}

		/* for WFE, we specifically dump the event value */
		if (((iwc_msg->cancelTask.errInstr[1] & 0xFF000000) >> 24) == CMDQ_CODE_WFE) {
			const uint32_t eventID = 0x3FF & iwc_msg->cancelTask.errInstr[1];

			CMDQ_ERR
			    ("=============== [CMDQ] Error WFE Instruction Status ===============\n");
			CMDQ_ERR("CMDQ_SYNC_TOKEN_VAL of %s is %d\n",
				 cmdq_core_get_event_name(eventID), iwc_msg->cancelTask.regValue);
		}

		CMDQ_ERR
		    ("CANCEL_TASK: pTask %p, INST:(0x%08x, 0x%08x), throwAEE:%d, hasReset:%d, pc:0x%08x\n",
		     task, iwc_msg->cancelTask.errInstr[1], iwc_msg->cancelTask.errInstr[0],
		     iwc_msg->cancelTask.throwAEE, iwc_msg->cancelTask.hasReset, iwc_msg->cancelTask.pc);
	} else if (iwc_command == CMD_CMDQ_TL_PATH_RES_ALLOCATE
		   || iwc_command == CMD_CMDQ_TL_PATH_RES_RELEASE) {
		/* do nothing */
	} else {
		/* note we etnry SWd to config GCE, and wait execution result in NWd */
		/* update taskState only if config failed */
		if (task && status < 0)
			task->taskState = TASK_STATE_ERROR;
	}

	/* log print */
	if (status < 0) {
		CMDQ_ERR("[SEC]RECV: status:%d command id:%d\n",
			 status, iwc_command);
	} else {
		CMDQ_MSG("[SEC]RECV: status:%d command id:%d\n\n",
			 status, iwc_command);
	}

	return status;
}

CmdqSecFillIwcCB cmdq_sec_get_iwc_msg_fill_cb_by_iwc_command(uint32_t iwc_command)
{
	CmdqSecFillIwcCB cb = NULL;

	switch (iwc_command) {
	case CMD_CMDQ_TL_CANCEL_TASK:
		cb = cmdq_sec_fill_iwc_cancel_msg_unlocked;
		break;
	case CMD_CMDQ_TL_PATH_RES_ALLOCATE:
	case CMD_CMDQ_TL_PATH_RES_RELEASE:
		cb = cmdq_sec_fill_iwc_resource_msg_unlocked;
		break;
	case CMD_CMDQ_TL_SUBMIT_TASK:
		cb = cmdq_sec_fill_iwc_command_msg_unlocked;
		break;
	case CMD_CMDQ_TL_TEST_HELLO_TL:
	case CMD_CMDQ_TL_TEST_DUMMY:
	default:
		cb = cmdq_sec_fill_iwc_command_basic_unlocked;
		break;
	};

	return cb;
}

int32_t cmdq_sec_send_context_session_message(struct cmdqSecContextStruct *handle,
	uint32_t iwc_command, struct TaskStruct *task,
	int32_t thread, CmdqSecFillIwcCB cb, void *data)
{
	int32_t status;
	const int32_t timeout_ms = (3 * 1000);
	const CmdqSecFillIwcCB iwcFillCB = (cb == NULL) ?
	    cmdq_sec_get_iwc_msg_fill_cb_by_iwc_command(iwc_command) : (cb);

	do {
		/* fill message bufer */
		status = iwcFillCB(iwc_command, task, thread, (void *)handle->iwcMessage);

		if (status < 0) {
			CMDQ_ERR("[SEC]fill msg buffer failed[%d], pid[%d:%d], cmdId[%d]\n",
				 status, current->tgid, current->pid, iwc_command);
			break;
		}

		/* fill kernel command */
		handle->iwc_cmd.command = iwc_command;
		handle->iwc_cmd.shmem_size = handle->iwc_msg_size;
		handle->iwc_cmd.log_level = (cmdq_core_should_print_msg()) ? (1) : (0);

		/* send message */
		status = cmdq_sec_execute_session_unlocked(handle, timeout_ms);
		if (status < 0) {
			CMDQ_ERR("[SEC]%s failed[%d], pid[%d:%d]\n",
				__func__, status, current->tgid, current->pid);
			break;
		}

		status = cmdq_sec_handle_session_reply_unlocked(handle, iwc_command, task, data);
	} while (0);

	return status;
}

int32_t cmdq_sec_teardown_context_session(struct cmdqSecContextStruct *handle)
{
	int32_t status = 0;

	if (handle) {
		CMDQ_MSG("[SEC]SEC_TEARDOWN: state: %d, iwcMessage:0x%p\n",
			handle->state, handle->iwcMessage);

		cmdq_sec_deinit_session_unlocked(handle->state, &handle->kcmd_notifier,
			handle->client_id, handle->iwc_cmd.shmem_id);

		/* clrean up handle's attritubes */
		handle->state = IWC_INIT;

	} else {
		CMDQ_ERR("[SEC]SEC_TEARDOWN: null secCtxHandle\n");
		status = -1;
	}
	return status;
}

void cmdq_sec_track_task_record(const uint32_t iwc_command,
	struct TaskStruct *pTask, CMDQ_TIME *pEntrySec, CMDQ_TIME *pExitSec)
{
	if (pTask == NULL)
		return;

	if (iwc_command != CMD_CMDQ_TL_SUBMIT_TASK)
		return;

	/* record profile data */
	/* tbase timer/time support is not enough currently, */
	/* so we treats entry/exit timing to secure world as the trigger timing */
	pTask->entrySec = *pEntrySec;
	pTask->exitSec = *pExitSec;
	pTask->trigger = *pEntrySec;
}

int32_t cmdq_sec_submit_to_secure_world_async_unlocked(uint32_t iwc_command,
	struct TaskStruct *pTask, int32_t thread,
	CmdqSecFillIwcCB iwcFillCB, void *data,
	bool throwAEE)
{
	const int32_t tgid = current->tgid;
	const int32_t pid = current->pid;
	struct cmdqSecContextStruct *handle = NULL;
	int32_t status = 0;
	int32_t duration = 0;
	char longMsg[CMDQ_LONGSTRING_MAX];
	uint32_t msgOffset;
	int32_t msgMAXSize;
	char *dispatch_mod = "CMDQ";

	CMDQ_TIME tEntrySec;
	CMDQ_TIME tExitSec;

	CMDQ_MSG("[SEC]-->SEC_SUBMIT: tgid[%d:%d]\n", tgid, pid);
	do {
		/* find handle first */

		/* Use global secssion handle to inter-world commumication. */
		if (gCmdqSecContextHandle == NULL)
			gCmdqSecContextHandle = cmdq_sec_context_handle_create(current->tgid);

		handle = gCmdqSecContextHandle;

		if (handle == NULL) {
			CMDQ_ERR("SEC_SUBMIT: tgid %d err[NULL secCtxHandle]\n", tgid);
			status = -(CMDQ_ERR_NULL_SEC_CTX_HANDLE);
			break;
		}

		if (cmdq_sec_setup_context_session(handle) < 0) {
			status = -(CMDQ_ERR_SEC_CTX_SETUP);
			break;
		}

		tEntrySec = sched_clock();

		status =
		    cmdq_sec_send_context_session_message(handle, iwc_command, pTask, thread,
							  iwcFillCB, data);

		tExitSec = sched_clock();
		CMDQ_GET_TIME_IN_US_PART(tEntrySec, tExitSec, duration);
		cmdq_sec_track_task_record(iwc_command, pTask, &tEntrySec, &tExitSec);

		/* check status and attach secure error before session teardown */
		cmdq_sec_handle_attach_status(pTask, iwc_command, handle->iwcMessage, status, &dispatch_mod);

		/* release resource */
#if !(CMDQ_OPEN_SESSION_ONCE)
		cmdq_sec_teardown_context_session(handle)
#endif
		    /* Note we entry secure for config only and wait result in normal world. */
		    /* No need reset module HW for config failed case */
	} while (0);

	if (status == -ETIMEDOUT) {
		cmdq_core_longstring_init(longMsg, &msgOffset, &msgMAXSize);
		cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
			"[SEC]<--SEC_SUBMIT: err:%d (wait timeout) task:0x%p thread:%d",
			status, pTask, thread);
		cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
			" tgid:%d:%d config_duration_ms:%d cmdId:%d\n",
			tgid, pid, duration, iwc_command);

		if (msgOffset > 0) {
			/* print message */
			if (throwAEE) {
				/* In timeout case, error come from TEE API, dispatch to AEE directly. */
				CMDQ_AEE("TEE", "%s", longMsg);
			}
			cmdq_core_turnon_first_dump(pTask);
			cmdq_core_dump_secure_task_status();
			CMDQ_ERR("%s", longMsg);
		}
	} else if (status < 0) {
		cmdq_core_longstring_init(longMsg, &msgOffset, &msgMAXSize);
		cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
			"[SEC]<--SEC_SUBMIT: err:%d task:0x%p thread:%d",
			status, pTask, thread);
		cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
			" tgid:%d:%d config_duration_ms:%d cmdId:%d\n",
			tgid, pid, duration, iwc_command);

		if (msgOffset > 0) {
			/* print message */
			if (throwAEE) {
				/* print message */
				CMDQ_AEE(dispatch_mod, "%s", longMsg);
			}
			cmdq_core_turnon_first_dump(pTask);
			CMDQ_ERR("%s", longMsg);
		}

		/* dump metadata first */
		if (pTask)
			cmdq_core_dump_secure_metadata(&(pTask->secData));
	} else {
		cmdq_core_longstring_init(longMsg, &msgOffset, &msgMAXSize);
		cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
			"[SEC]<--SEC_SUBMIT: err:%d task:0x%p thread:%d",
			status, pTask, thread);
		cmdqCoreLongString(false, longMsg, &msgOffset, &msgMAXSize,
			" tgid:%d:%d config_duration_ms:%d cmdId:%d\n",
			tgid, pid, duration, iwc_command);
		if (msgOffset > 0) {
			/* print message */
			CMDQ_MSG("%s", longMsg);
		}
	}
	return status;
}

int32_t cmdq_sec_init_allocate_resource_thread(void *data)
{
	int32_t status = 0;

	cmdq_sec_lock_secure_path();

	gCmdqSecContextHandle = NULL;
	status = cmdq_sec_allocate_path_resource_unlocked(false);

	cmdq_sec_unlock_secure_path();

	return status;
}

/********************************************************************************
 * sectrace
 *******************************************************************************/
#ifdef CMDQ_SECTRACE_SUPPORT
int32_t cmdq_sec_fill_iwc_command_sectrace_unlocked(int32_t iwc_command, void *_pTask,
						    int32_t thread, void *_pIwc)
{
	struct iwcCmdqMessage_t *pIwc;

	pIwc = (struct iwcCmdqMessage_t *) _pIwc;

	/* specify command id only, don't care other other */
	memset(pIwc, 0x0, sizeof(struct iwcCmdqMessage_t));
	pIwc->cmd = iwc_command;

	switch (iwc_command) {
	case CMD_CMDQ_TL_SECTRACE_MAP:
		pIwc->sectracBuffer.addr = (uint32_t) (gCmdqSectraceMappedInfo.secure_virt_addr);
		pIwc->sectracBuffer.size = (gCmdqSectraceMappedInfo.secure_virt_len);
		break;
	case CMD_CMDQ_TL_SECTRACE_UNMAP:
	case CMD_CMDQ_TL_SECTRACE_TRANSACT:
	default:
		pIwc->sectracBuffer.addr = 0;
		pIwc->sectracBuffer.size = 0;
		break;
	}

	/* medatada: debug config */
	pIwc->debug.logLevel = (cmdq_core_should_print_msg()) ? (1) : (0);
	pIwc->debug.enableProfile = cmdq_core_profile_enabled();

	CMDQ_LOG("[sectrace]SESSION_MSG: iwc_command:%d, msg(sectraceBuffer, addr:0x%x, size:%d)\n",
		 iwc_command, pIwc->sectracBuffer.addr, pIwc->sectracBuffer.size);

	return 0;
}

static int cmdq_sec_sectrace_map(void *va, size_t size)
{
	int status;
	enum mc_result mcRet;

	CMDQ_LOG("[sectrace]-->map: start, va:%p, size:%d\n", va, (int)size);

	status = 0;
	cmdq_sec_lock_secure_path();
	do {
		/* HACK: submit a dummy message to ensure secure path init done */
		status =
		    cmdq_sec_submit_to_secure_world_async_unlocked(CMD_CMDQ_TL_TEST_HELLO_TL, NULL,
								   CMDQ_INVALID_THREAD, NULL, NULL,
								   true);

		/* map log buffer in NWd */
		mcRet = mc_map(&(gCmdqSecContextHandle->sessionHandle),
			       va, (uint32_t) size, &gCmdqSectraceMappedInfo);
		if (mcRet != MC_DRV_OK) {
			CMDQ_ERR("[sectrace]map: failed in NWd, mc_map err: 0x%x\n", mcRet);
			status = -EFAULT;
			break;
		}

		CMDQ_LOG
		    ("[sectrace]map: mc_map sectrace buffer done, gCmdqSectraceMappedInfo(va:0x%08x, size:%d)\n",
		     gCmdqSectraceMappedInfo.secure_virt_addr,
		     gCmdqSectraceMappedInfo.secure_virt_len);

		/* ask secure CMDQ to map sectrace log buffer */
		status = cmdq_sec_submit_to_secure_world_async_unlocked(CMD_CMDQ_TL_SECTRACE_MAP,
									NULL, CMDQ_INVALID_THREAD,
									cmdq_sec_fill_iwc_command_sectrace_unlocked,
									NULL, true);
		if (status < 0) {
			CMDQ_ERR("[sectrace]map: failed in SWd: %d\n", status);
			mc_unmap(&(gCmdqSecContextHandle->sessionHandle), va,
				 &gCmdqSectraceMappedInfo);
			status = -EFAULT;
			break;
		}
	} while (0);

	cmdq_sec_unlock_secure_path();

	CMDQ_LOG("[sectrace]<--map: status: %d\n", status);

	return status;
}

static int cmdq_sec_sectrace_unmap(void *va, size_t size)
{
	int status;
	enum mc_result mcRet;

	status = 0;
	cmdq_sec_lock_secure_path();
	do {
		if (gCmdqSecContextHandle == NULL) {
			status = -EFAULT;
			break;
		}

		/* ask secure CMDQ to unmap sectrace log buffer */
		status = cmdq_sec_submit_to_secure_world_async_unlocked(CMD_CMDQ_TL_SECTRACE_UNMAP,
									NULL, CMDQ_INVALID_THREAD,
									cmdq_sec_fill_iwc_command_sectrace_unlocked,
									NULL, true);
		if (status < 0) {
			CMDQ_ERR("[sectrace]unmap: failed in SWd: %d\n", status);
			mc_unmap(&(gCmdqSecContextHandle->sessionHandle), va,
				 &gCmdqSectraceMappedInfo);
			status = -EFAULT;
			break;
		}

		mcRet =
		    mc_unmap(&(gCmdqSecContextHandle->sessionHandle), va, &gCmdqSectraceMappedInfo);

	} while (0);

	cmdq_sec_unlock_secure_path();

	CMDQ_LOG("[sectrace]unmap: status: %d\n", status);

	return status;
}

static int cmdq_sec_sectrace_transact(void)
{
	int status;

	CMDQ_LOG("[sectrace]-->transact\n");

	status = cmdq_sec_submit_to_secure_world_async_unlocked(CMD_CMDQ_TL_SECTRACE_TRANSACT,
								NULL, CMDQ_INVALID_THREAD,
								cmdq_sec_fill_iwc_command_sectrace_unlocked,
								NULL, true);

	CMDQ_LOG("[sectrace]<--transact: status: %d\n", status);

	return status;
}
#endif

int32_t cmdq_sec_sectrace_init(void)
{
#ifdef CMDQ_SECTRACE_SUPPORT
	int32_t status;
	const uint32_t CMDQ_SECTRACE_BUFFER_SIZE_KB = 64;
	union callback_func sectraceCB;

	/* use callback_tl_function because CMDQ use "TCI" for inter-world communication */
	sectraceCB.tl.map = cmdq_sec_sectrace_map;
	sectraceCB.tl.unmap = cmdq_sec_sectrace_unmap;
	sectraceCB.tl.transact = cmdq_sec_sectrace_transact;

	/* create sectrace entry in debug FS */
	status = init_sectrace("CMDQ_SEC", if_tci,	/* use TCI for inter world communication */
			       usage_tl_dr,	/* print sectrace log for tl and driver */
			       CMDQ_SECTRACE_BUFFER_SIZE_KB, &sectraceCB);

	CMDQ_LOG("cmdq_sec_trace_init, status:%d\n", status);
	return 0;
#else
	return 0;
#endif
}

int32_t cmdq_sec_sectrace_deinit(void)
{
#ifdef CMDQ_SECTRACE_SUPPORT
	/* destroy sectrace entry in debug FS */
	deinit_sectrace("CMDQ_SEC");
	return 0;
#else
	return 0;
#endif
}
#endif				/* CMDQ_SECURE_PATH_SUPPORT */

/********************************************************************************
 * common part: for general projects
 *******************************************************************************/
int32_t cmdq_sec_create_shared_memory(struct cmdqSecSharedMemoryStruct **pHandle, const uint32_t size)
{
	struct cmdqSecSharedMemoryStruct *handle = NULL;
	void *pVA = NULL;
	dma_addr_t PA = 0;

	handle = kzalloc(sizeof(uint8_t *) * sizeof(struct cmdqSecSharedMemoryStruct), GFP_KERNEL);
	if (handle == NULL)
		return -ENOMEM;

	CMDQ_LOG("%s\n", __func__);

	/* allocate non-cachable memory */
	pVA = cmdq_core_alloc_hw_buffer(cmdq_dev_get(), size, &PA, GFP_KERNEL);

	CMDQ_MSG("%s, MVA:%pa, pVA:%p, size:%d\n", __func__, &PA, pVA, size);

	if (pVA == NULL) {
		kfree(handle);
		return -ENOMEM;
	}

	/* update memory information */
	handle->size = size;
	handle->pVABase = pVA;
	handle->MVABase = PA;

	*pHandle = handle;

	return 0;
}

int32_t cmdq_sec_destroy_shared_memory(struct cmdqSecSharedMemoryStruct *handle)
{

	if (handle && handle->pVABase) {
		cmdq_core_free_hw_buffer(cmdq_dev_get(), handle->size,
					 handle->pVABase, handle->MVABase);
	}

	kfree(handle);
	handle = NULL;

	return 0;
}

int32_t cmdq_sec_exec_task_async_unlocked(struct TaskStruct *pTask, int32_t thread)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	status =
		cmdq_sec_submit_to_secure_world_async_unlocked(CMD_CMDQ_TL_SUBMIT_TASK, pTask, thread,
		NULL, NULL, true);
	if (status < 0) {
		/* Error status print */
		CMDQ_ERR("%s[%d]\n", __func__, status);
	}

	return status;

#else
	CMDQ_ERR("secure path not support\n");
	return -EFAULT;
#endif
}

int32_t cmdq_sec_cancel_error_task_unlocked(struct TaskStruct *pTask, int32_t thread,
					    struct cmdqSecCancelTaskResultStruct *pResult)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	if ((pTask == NULL) || (cmdq_get_func()->isSecureThread(thread) == false)
	    || (pResult == NULL)) {

		CMDQ_ERR("%s invalid param, pTask:%p, thread:%d, pResult:%p\n",
			 __func__, pTask, thread, pResult);
		return -EFAULT;
	}

	status = cmdq_sec_submit_to_secure_world_async_unlocked(CMD_CMDQ_TL_CANCEL_TASK,
								pTask, thread, NULL,
								(void *)pResult, true);
	return status;
#else
	CMDQ_ERR("secure path not support\n");
	return -EFAULT;
#endif
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
static atomic_t gCmdqSecPathResource = ATOMIC_INIT(0);
#endif

int32_t cmdq_sec_allocate_path_resource_unlocked(bool throwAEE)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status = 0;

	CMDQ_MSG("%s throwAEE: %s", __func__, throwAEE ? "true" : "false");

	if (atomic_cmpxchg(&gCmdqSecPathResource, 0, 1) != 0) {
		/* has allocated successfully */
		return status;
	}

	status =
	    cmdq_sec_submit_to_secure_world_async_unlocked(CMD_CMDQ_TL_PATH_RES_ALLOCATE, NULL,
							   CMDQ_INVALID_THREAD, NULL, NULL,
							   throwAEE);
	if (status < 0) {
		/* Error status print */
		CMDQ_ERR("%s[%d] reset context\n", __func__, status);

		/* in fail case, we want function alloc again */
		atomic_set(&gCmdqSecPathResource, 0);
	}

	return status;

#else
	CMDQ_ERR("secure path not support\n");
	return -EFAULT;
#endif
}

/********************************************************************************
 * common part: SecContextHandle handle
 *******************************************************************************/
struct cmdqSecContextStruct *cmdq_sec_context_handle_create(u32 tgid)
{
	struct cmdqSecContextStruct *handle = NULL;
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	u32 msg_size = sizeof(struct iwcCmdqMessage_t);
	u32 page_num = 0;
	/* struct page *shmem_page = NULL; */

	/* size must be page aligned */
	page_num = msg_size >> PAGE_SHIFT;
	if (page_num << PAGE_SHIFT < msg_size)
		page_num++;
	msg_size = page_num << PAGE_SHIFT;

	if (unlikely(sizeof(struct iwcCmdqMessage_t) > msg_size)) {
		CMDQ_AEE("CMDQ", "WSM not enough necessary size:%u allocate:%u\n",
			(uint32_t)sizeof(struct iwcCmdqMessage_t), msg_size);
		return NULL;
	}

	handle = kzalloc(sizeof(uint8_t *) * sizeof(struct cmdqSecContextStruct), GFP_ATOMIC);
	if (handle) {
		handle->state = IWC_INIT;
		handle->uuid = CMDQ_TL_UUID;

		handle->iwc_msg_size = msg_size;
		handle->iwcMessage = kzalloc(handle->iwc_msg_size, GFP_KERNEL);
		if (!handle->iwcMessage) {
			CMDQ_ERR("[SEC]SecCtxHandle_CREATE: create iwc mem fail tgid[%d]\n", tgid);
			kfree(handle);
			return NULL;
		}

		handle->kcmd_notifier.notifier_call = cmdq_sec_cmd_notifier;
		handle->tgid = tgid;
		handle->referCount = 0;

		CMDQ_MSG("[SEC]SecCtxHandle_CREATE: create new, H:0x%p tgid:%d iwc size:%u\n",
			handle, tgid, msg_size);
	} else {
		CMDQ_ERR("[SEC]SecCtxHandle_CREATE: err[LOW_MEM], tgid[%d]\n", tgid);
	}
#endif
	return handle;
}


/********************************************************************************
 * common part: init, deinit, path
 *******************************************************************************/
void cmdq_sec_lock_secure_path(void)
{
	mutex_lock(&gCmdqSecExecLock);
	/* set memory barrier for lock */
	smp_mb();
}

void cmdq_sec_unlock_secure_path(void)
{
	mutex_unlock(&gCmdqSecExecLock);
}

void cmdqSecDeInitialize(void)
{
}

void cmdqSecInitialize(void)
{
	INIT_LIST_HEAD(&gCmdqSecContextList);
}

void cmdqSecEnableProfile(const bool enable)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	CMDQ_LOG("[sectrace]enable profile %d\n", enable);

	mutex_lock(&gCmdqSecProfileLock);

	if (enable)
		cmdq_sec_sectrace_init();
	else
		cmdq_sec_sectrace_deinit();

	mutex_unlock(&gCmdqSecProfileLock);
#endif
}
