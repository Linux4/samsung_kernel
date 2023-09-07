/*
 * Copyright (C) 2018 MediaTek Inc.
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

#define PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS
#include "private/tmem_pr_fmt.h" PR_FMT_HEADER_MUST_BE_INCLUDED_BEFORE_ALL_HDRS

#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "private/mld_helper.h"
#include "private/tmem_device.h"
#include "private/tmem_error.h"
#include "private/tmem_utils.h"
#include "tee_impl/tee_priv.h"
#include "tee_impl/tee_common.h"
#include "tee_impl/tee_gp_def.h"

#include "sysdep.h"
#include "tzdev.h"
#include "tzdev/tzdev.h"
#include "tzdev/kernel_api.h"

static struct tz_uuid secmem_uuid = {
	.time_low = 0x00000000,
	.time_mid = 0x4D54,
	.time_hi_and_version = 0x4B5F,
	/* clang-format off */
	.clock_seq_and_node = {0x42, 0x46, 0x53, 0x4D, 0x45, 0x4D, 0x54, 0x41},
	/* clang-format on */
};

struct csmc_payload {
	uint32_t cmd;
	uint32_t iwshmem_id;
	uint32_t result;
};

struct TEE_GP_SESSION_DATA {
	uint32_t client_id;
	uint32_t shmem_id;
	char *shmem_va;
	struct page *shmem_page;
};

static struct TEE_GP_SESSION_DATA *g_sess_data;
static bool is_sess_ready;
static unsigned int sess_ref_cnt;

#define TEE_SESSION_LOCK() mutex_lock(&g_sess_lock)
#define TEE_SESSION_UNLOCK() mutex_unlock(&g_sess_lock)
static DEFINE_MUTEX(g_sess_lock);

static DECLARE_COMPLETION(kcmd_event);
static int __kcmd_notifier(struct notifier_block *nb, unsigned long action,
			   void *data)
{
	pr_debug("Complete kcmd_event\n");
	complete(&kcmd_event);
	return TMEM_OK;
}

static struct notifier_block kcmd_notifier = {
	.notifier_call = __kcmd_notifier,
};

static struct TEE_GP_SESSION_DATA *tee_gp_create_session_data(void)
{
	struct TEE_GP_SESSION_DATA *data;

	data = mld_kmalloc(sizeof(struct TEE_GP_SESSION_DATA), GFP_KERNEL);
	if (INVALID(data))
		return NULL;

	memset(data, 0x0, sizeof(struct TEE_GP_SESSION_DATA));
	return data;
}

static void tee_gp_destroy_session_data(void)
{
	if (VALID(g_sess_data)) {
		mld_kfree(g_sess_data);
		g_sess_data = NULL;
	}
}

static int tee_session_open_single_session_unlocked(void)
{
	int ret = TMEM_OK;

	if (is_sess_ready) {
		pr_debug("Session is already created!\n");
		return TMEM_OK;
	}

	while (!tzdev_is_up()) {
		pr_info("blowfish NOT ready!, sleep 1s\n");
		msleep(1000);
	}

	g_sess_data = tee_gp_create_session_data();
	if (INVALID(g_sess_data)) {
		pr_err("Create session data failed: out of memory!\n");
		return TMEM_TEE_CREATE_SESSION_DATA_FAILED;
	}

	g_sess_data->shmem_page = alloc_page(GFP_KERNEL);
	if (INVALID(g_sess_data->shmem_page)) {
		pr_err("Open session failed: alloc_page failed!\n");
		ret = TMEM_CSMC_OPEN_SESSION_OUT_OF_MEMORY;
		goto err_alloc_page;
	}

	g_sess_data->shmem_va = page_address(g_sess_data->shmem_page);
	memset(g_sess_data->shmem_va, 0, PAGE_SIZE);
	ret = tzdev_mem_register(g_sess_data->shmem_va, PAGE_SIZE, 1);
	if (ret < 0) {
		pr_err("Register share memory failed: %d!\n", ret);
		ret = TMEM_CSMC_REGISTER_SHARE_MEMORY_FAILED;
		goto err_register_shmem;
	}

	g_sess_data->shmem_id = ret;
	ret = tz_iwnotify_chain_register(TZ_IWNOTIFY_OEM_NOTIFICATION_FLAG_16,
					 &kcmd_notifier);
	if (ret < 0) {
		pr_err("Register notify chain failed: %d!\n", ret);
		ret = TMEM_CSMC_REGISTER_NOTIFY_CHAIN_FAILED;
		goto err_register_notify_chain;
	}

	g_sess_data->client_id = tzdev_kapi_open(&secmem_uuid);
	if (g_sess_data->client_id < 0) {
		pr_err("Open device failed: %d!\n", g_sess_data->client_id);
		ret = TMEM_CSMC_OPEN_DEVICE_FAILED;
		goto err_open_device;
	}

	ret = tzdev_kapi_mem_grant(g_sess_data->client_id,
				   g_sess_data->shmem_id);
	if (ret) {
		pr_err("Grant memory access failed: %d!\n", ret);
		ret = TMEM_CSMC_GRANT_ACCESS_FAILED;
		goto err_grant_access;
	}

	is_sess_ready = true;
	return TMEM_OK;

err_grant_access:
	ret = tzdev_kapi_close(g_sess_data->client_id);
	if (ret < 0)
		pr_err("Close device failed: %d!\n", g_sess_data->client_id);
err_open_device:
	ret = tz_iwnotify_chain_unregister(TZ_IWNOTIFY_OEM_NOTIFICATION_FLAG_16,
					   &kcmd_notifier);
	if (ret < 0)
		pr_err("Unregister notify chain failed: %d!\n", ret);
err_register_notify_chain:
	ret = tzdev_mem_release(g_sess_data->shmem_id);
	if (ret < 0)
		pr_err("Release share memory failed: %d!\n", ret);
err_register_shmem:
	__free_page(g_sess_data->shmem_page);
err_alloc_page:
	tee_gp_destroy_session_data();

	return ret;
}

static int tee_session_close_single_session_unlocked(void)
{
	int ret = TMEM_OK;

	if (!is_sess_ready) {
		pr_debug("Session is already closed!\n");
		return TMEM_OK;
	}

	ret = tzdev_kapi_mem_revoke(g_sess_data->client_id,
				    g_sess_data->shmem_id);
	if (ret)
		pr_err("Revoke memory access failed: %d!\n", ret);

	ret = tzdev_kapi_close(g_sess_data->client_id);
	if (ret < 0)
		pr_err("Close device failed: %d!\n", g_sess_data->client_id);

	ret = tz_iwnotify_chain_unregister(TZ_IWNOTIFY_OEM_NOTIFICATION_FLAG_16,
					   &kcmd_notifier);
	if (ret < 0)
		pr_err("Unregister notify chain failed: %d!\n", ret);

	ret = tzdev_mem_release(g_sess_data->shmem_id);
	if (ret < 0)
		pr_err("Release share memory failed: %d!\n", ret);

	__free_page(g_sess_data->shmem_page);

	tee_gp_destroy_session_data();
	is_sess_ready = false;

	return TMEM_OK;
}

int tee_session_open(void **tee_data, void *priv)
{
	UNUSED(tee_data);
	UNUSED(priv);

	TEE_SESSION_LOCK();

	if (tee_session_open_single_session_unlocked() == TMEM_OK)
		sess_ref_cnt++;

	TEE_SESSION_UNLOCK();
	return TMEM_OK;
}

int tee_session_close(void *tee_data, void *priv)
{
	UNUSED(tee_data);
	UNUSED(priv);

	TEE_SESSION_LOCK();

	if (sess_ref_cnt == 0) {
		pr_err("Session is already closed!\n");
		TEE_SESSION_UNLOCK();
		return TMEM_OK;
	}

	sess_ref_cnt--;
	if (sess_ref_cnt == 0) {
		pr_debug("Try closing session!\n");
		tee_session_close_single_session_unlocked();
	}

	TEE_SESSION_UNLOCK();
	return TMEM_OK;
}

#ifdef CONFIG_MTK_ENG_BUILD
#define SECMEM_EXECUTE_CMD_TIMEOUT_MS (20000)
#else
#define SECMEM_EXECUTE_CMD_TIMEOUT_MS (5000)
#endif
static int secmem_execute(u32 cmd, struct secmem_param *param)
{
	int ret = TMEM_OK;
	struct secmem_ta_msg_t *msg;
	struct csmc_payload payload;

	TEE_SESSION_LOCK();

	if (!is_sess_ready) {
		pr_err("Session is not ready!\n");
		TEE_SESSION_UNLOCK();
		return TMEM_TEE_SESSION_IS_NOT_READY;
	}

	init_completion(&kcmd_event);

	payload.cmd = cmd;
	payload.iwshmem_id = g_sess_data->shmem_id;

	memset(g_sess_data->shmem_va, 0, PAGE_SIZE);
	msg = (struct secmem_ta_msg_t *)g_sess_data->shmem_va;
	msg->sec_handle = param->sec_handle;
	msg->alignment = param->alignment;
	msg->size = param->size;
	msg->refcount = param->refcount;

	ret = tzdev_kapi_send(g_sess_data->client_id, &payload,
			      sizeof(payload));
	if (ret < 0) {
		pr_err("tzdev_kapi_send failed! cmd:%d, ret:0x%x\n", cmd, ret);
		ret = TMEM_CSMC_SEND_CMD_FAILED;
		goto _err_invoke_command;
	}

	ret = wait_for_completion_interruptible_timeout(
		&kcmd_event, msecs_to_jiffies(SECMEM_EXECUTE_CMD_TIMEOUT_MS));
	if (ret <= 0) {
		pr_err("invoke %s (%d s)! cmd:%d, ret:0x%x\n",
		       ((ret == 0) ? "timeout" : "interrupted"),
		       SECMEM_EXECUTE_CMD_TIMEOUT_MS, cmd, ret);
		ret = TMEM_CSMC_WAIT_COMPLETE_TIMEOUTED;
		goto _err_invoke_command;
	}

	ret = tzdev_kapi_recv(g_sess_data->client_id, &payload,
			      sizeof(payload));
	if (ret < 0) {
		pr_err("tzdev_kapi_recv failed! cmd:%d, ret:0x%x\n", cmd, ret);
		ret = TMEM_CSMC_RECEIVE_CMD_FAILED;
		goto _err_invoke_command;
	}

	if (payload.result) {
		pr_err("Bad msg.result from TA: 0x%x\n", payload.result);
		ret = TMEM_CSMC_TA_BAD_RESPONSE;
		goto _err_invoke_command;
	}

	param->sec_handle = msg->sec_handle;
	param->refcount = msg->refcount;
	param->alignment = msg->alignment;
	param->size = msg->size;

	pr_debug("shndl=0x%llx refcnt=%d align=0x%llx size=0x%llx\n",
		 (u64)param->sec_handle, param->refcount, (u64)param->alignment,
		 (u64)param->size);

	TEE_SESSION_UNLOCK();
	return TMEM_OK;

_err_invoke_command:
	TEE_SESSION_UNLOCK();
	return ret;
}

int tee_alloc(u32 alignment, u32 size, u32 *refcount, u32 *sec_handle,
	      u8 *owner, u32 id, u32 clean, void *tee_data, void *priv)
{
	int ret;
	struct secmem_param param = {0};
	u32 tee_ta_cmd = (clean ? get_tee_cmd(TEE_OP_ALLOC_ZERO, priv)
				: get_tee_cmd(TEE_OP_ALLOC, priv));

	UNUSED(tee_data);
	UNUSED(owner);
	UNUSED(id);
	UNUSED(priv);

	*refcount = 0;
	*sec_handle = 0;

	param.alignment = alignment;
	param.size = size;
	param.refcount = 0;
	param.sec_handle = 0;

	ret = secmem_execute(tee_ta_cmd, &param);
	if (ret)
		return TMEM_TEE_ALLOC_CHUNK_FAILED;

	*refcount = param.refcount;
	*sec_handle = param.sec_handle;
	pr_debug("ref cnt: 0x%x, sec_handle: 0x%llx\n", param.refcount,
		 param.sec_handle);
	return TMEM_OK;
}

int tee_free(u32 sec_handle, u8 *owner, u32 id, void *tee_data, void *priv)
{
	struct secmem_param param = {0};
	u32 tee_ta_cmd = get_tee_cmd(TEE_OP_FREE, priv);

	UNUSED(tee_data);
	UNUSED(owner);
	UNUSED(id);
	UNUSED(priv);

	param.sec_handle = sec_handle;

	if (secmem_execute(tee_ta_cmd, &param))
		return TMEM_TEE_FREE_CHUNK_FAILED;

	return TMEM_OK;
}

int tee_mem_reg_add(u64 pa, u32 size, void *tee_data, void *priv)
{
	struct secmem_param param = {0};
	u32 tee_ta_cmd = get_tee_cmd(TEE_OP_REGION_ENABLE, priv);

	UNUSED(tee_data);
	UNUSED(priv);
	param.sec_handle = pa;
	param.size = size;

	if (secmem_execute(tee_ta_cmd, &param))
		return TMEM_TEE_APPEND_MEMORY_FAILED;

	return TMEM_OK;
}

int tee_mem_reg_remove(void *tee_data, void *priv)
{
	struct secmem_param param = {0};
	u32 tee_ta_cmd = get_tee_cmd(TEE_OP_REGION_DISABLE, priv);

	UNUSED(tee_data);
	UNUSED(priv);

	if (secmem_execute(tee_ta_cmd, &param))
		return TMEM_TEE_RELEASE_MEMORY_FAILED;

	return TMEM_OK;
}

#define VALID_INVOKE_COMMAND(cmd)                                              \
	((cmd >= CMD_SEC_MEM_INVOKE_CMD_START)                                 \
	 && (cmd <= CMD_SEC_MEM_INVOKE_CMD_END))

static int tee_invoke_command(struct trusted_driver_cmd_params *invoke_params,
			      void *tee_data, void *priv)
{
	struct secmem_param param = {0};

	UNUSED(tee_data);
	UNUSED(priv);

	if (INVALID(invoke_params))
		return TMEM_PARAMETER_ERROR;

	if (!VALID_INVOKE_COMMAND(invoke_params->cmd)) {
		pr_err("%s:%d unsupported cmd:%d!\n", __func__, __LINE__,
		       invoke_params->cmd);
		return TMEM_COMMAND_NOT_SUPPORTED;
	}

	pr_debug("invoke cmd is %d (0x%llx, 0x%llx, 0x%llx, 0x%llx)\n",
		 invoke_params->cmd, invoke_params->param0,
		 invoke_params->param1, invoke_params->param2,
		 invoke_params->param3);

	param.alignment = invoke_params->param0;
	param.size = invoke_params->param1;
	/* CAUTION: USE IT CAREFULLY!
	 * For param2, secmem ta only supports 32-bit for backward compatibility
	 */
	param.refcount = (u32)invoke_params->param2;
	param.sec_handle = invoke_params->param3;

	if (secmem_execute(invoke_params->cmd, &param))
		return TMEM_TEE_INVOKE_COMMAND_FAILED;

	invoke_params->param0 = param.alignment;
	invoke_params->param1 = param.size;
	/* CAUTION: USE IT CAREFULLY!
	 * For param2, secmem ta only supports 32-bit for backward compatibility
	 */
	invoke_params->param2 = (u64)param.refcount;
	invoke_params->param3 = param.sec_handle;
	return TMEM_OK;
}

static struct trusted_driver_operations tee_gp_peer_ops = {
	.session_open = tee_session_open,
	.session_close = tee_session_close,
	.memory_alloc = tee_alloc,
	.memory_free = tee_free,
	.memory_grant = tee_mem_reg_add,
	.memory_reclaim = tee_mem_reg_remove,
	.invoke_cmd = tee_invoke_command,
};

void get_tee_peer_ops(struct trusted_driver_operations **ops)
{
	pr_info("SECMEM_TEE_GP_OPS\n");
	*ops = &tee_gp_peer_ops;
}
