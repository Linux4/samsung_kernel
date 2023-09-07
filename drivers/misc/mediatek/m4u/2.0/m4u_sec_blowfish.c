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

#include "m4u_priv.h"
#include "m4u.h"
#include "m4u_hw.h"

#include <tzdev/kernel_api.h>
#include <tzdev/tzdev.h>
#include "sysdep.h"
#include "tzdev.h"
#include "tz_mem.h"
#include "../blowfish/tdriver.h"

#define CTX_TYPE_TA	0
#define CTX_TYPE_TDRV	1

struct m4u_blowfish_context {
	struct tz_uuid uuid;
	int client_id;
	int notify_event;
	struct completion completion;
	struct notifier_block notifier;
	/* share memory */
	struct page *shmem_page;
	int shmem_id;
	void *shmem_va;
	struct mutex ctx_lock;
	int inited;
	int ctx_type;
};

static int m4u_tee_notifier(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct m4u_blowfish_context *bf_ctx =
		container_of(nb, struct m4u_blowfish_context, notifier);

	complete(&bf_ctx->completion);
	return 0;
}

#define M4U_DRV_UUID \
{0x00000000, 0x4D54, 0x4B5F, {0x42, 0x46, 0x4D, 0x34, 0x55, 0x44, 0x54, 0x41} }
static struct m4u_blowfish_context m4u_bf_tdrv_ctx = {
	.uuid = M4U_DRV_UUID,
	.client_id = 0,
	.notify_event = TZ_IWNOTIFY_OEM_NOTIFICATION_FLAG_2,
	.completion = COMPLETION_INITIALIZER(m4u_bf_tdrv_ctx.completion),
	.notifier = {
		.notifier_call	= m4u_tee_notifier,
	},
	.ctx_lock = __MUTEX_INITIALIZER(m4u_bf_tdrv_ctx.ctx_lock),
	.ctx_type = CTX_TYPE_TDRV,
};

static struct m4u_sec_context m4u_tdrv_ctx = {
	.name = "m4u_tdrv",
	.imp = &m4u_bf_tdrv_ctx,
};


#define M4U_TL_UUID \
{0x00000000, 0x4D54, 0x4B5F, {0x42, 0x46, 0x00, 0x00, 0x00, 0x4D, 0x34, 0x55} }
static struct m4u_blowfish_context m4u_bf_ta_ctx = {
	.uuid = M4U_TL_UUID,
	.client_id = 0,
	.notify_event = TZ_IWNOTIFY_OEM_NOTIFICATION_FLAG_14,
	.completion = COMPLETION_INITIALIZER(m4u_bf_ta_ctx.completion),
	.notifier = {
		.notifier_call	= m4u_tee_notifier,
	},
	.ctx_lock = __MUTEX_INITIALIZER(m4u_bf_ta_ctx.ctx_lock),
	.ctx_type = CTX_TYPE_TA,
};

static struct m4u_sec_context m4u_ta_ctx = {
	.name = "m4u_ta",
	.imp = &m4u_bf_ta_ctx,
};



static int m4u_send_tdrv_msg(struct m4u_sec_context *ctx, enum tdriver_cmd cmd)
{
	struct tdriver_msg tdrv_msg;
	struct m4u_blowfish_context *bf_ctx = ctx->imp;
	int ret = 0;

	tdrv_msg.cmd = cmd;
	tdrv_msg.arg = PAGE_SIZE;
	tdrv_msg.iwshmem_id = bf_ctx->shmem_id;
	ret = tzdev_kapi_send(bf_ctx->client_id, &tdrv_msg, sizeof(tdrv_msg));
	if (ret < 0) {
		M4UERR("%s tzdev_kapi_send failed - %d\n", ctx->name, ret);
		goto err_out;
	}

	M4ULOG_HIGH("wait for notification()\n");
	ret = wait_for_completion_interruptible_timeout(&bf_ctx->completion,
			5 * HZ);
	if (ret <= 0) {
		M4UERR("%s wait_completion_timeout failed - %d\n",
				ctx->name, ret);
		goto err_out;
	}

	M4ULOG_HIGH("tzdev_kapi_recv()\n");
	ret = tzdev_kapi_recv(bf_ctx->client_id, &tdrv_msg, sizeof(tdrv_msg));
	if (ret < 0) {
		M4UERR("%s tzdev_kapi_recv failed - %d\n", ctx->name, ret);
		goto err_out;
	}

	if (tdrv_msg.result) {
		M4UERR("%s tdrv_msg execution failed - %d\n",
				ctx->name, tdrv_msg.result);
		goto err_out;
	}
	return 0;

err_out:
	m4u_aee_print("%s  m4u_send_tdrv_msg failed\n", ctx->name);
	return -1;
}

static int m4u_bf_ctx_init(struct m4u_sec_context *ctx)
{
	int ret, func_ret = 0;
	struct m4u_blowfish_context *bf_ctx = ctx->imp;

	/* alloc world share memory */
	bf_ctx->shmem_page = alloc_page(GFP_KERNEL);
	if (!bf_ctx->shmem_page) {
		M4UERR("%s alloc_page failed.\n", ctx->name);
		return -ENOMEM;
	}

	bf_ctx->shmem_va = page_address(bf_ctx->shmem_page);
	memset(bf_ctx->shmem_va, 0, PAGE_SIZE);

	M4ULOG_HIGH("%s share memory va=%p()\n", ctx->name, bf_ctx->shmem_va);
	ret = tzdev_mem_register(bf_ctx->shmem_va, PAGE_SIZE, 1);
	if (ret < 0) {
		__free_page(bf_ctx->shmem_page);
		M4UERR("%s tzdev_mem_register failed. ret = %d\n",
				ctx->name, ret);
		return ret;
	}

	bf_ctx->shmem_id = ret;
	M4ULOG_HIGH("shmem_id = %d\n", bf_ctx->shmem_id);

	M4ULOG_HIGH("%s tz_iwnotify_chain_register()\n", ctx->name);
	ret = tz_iwnotify_chain_register(bf_ctx->notify_event,
			&bf_ctx->notifier);
	if (ret < 0) {
		M4UERR("%s tz_iwnotify_chain_register failed. ret = %d\n",
				ctx->name, ret);
		goto out_mem_register;
	}

	M4ULOG_HIGH("%s tzdev_kapi_open()\n", ctx->name);
	ret = tzdev_kapi_open(&bf_ctx->uuid);
	if (ret < 0) {
		func_ret = ret;
		M4UERR("%s tzdev_kapi_open failed - %d\n", ctx->name, ret);
		goto out_iwnotify;
	}
	bf_ctx->client_id = ret;
	M4ULOG_HIGH("%s client_id = %d\n", ctx->name, bf_ctx->client_id);

	/* only t-driver needs share memory (driver framework requires it) */
	if (bf_ctx->ctx_type == CTX_TYPE_TDRV) {
		M4ULOG_HIGH("tzdev_kapi_mem_grant()\n");
		ret = tzdev_kapi_mem_grant(bf_ctx->client_id,
				bf_ctx->shmem_id);
		if (ret) {
			func_ret = ret;
			M4UERR("%s tzdev_kapi_mem_grant failed - %d\n",
					ctx->name, ret);
			goto out_open;
		}
		/* map share memory to driver framework */
		ret = m4u_send_tdrv_msg(ctx, MAP_IWSHMEM);
		if (ret) {
			func_ret = -EPERM;
			goto out_mem_grant;
		}
	}

	/* use shmem as m4u_msg */
	ctx->m4u_msg = bf_ctx->shmem_va;

	bf_ctx->inited = 1;
	return 0;


out_mem_grant:
	M4ULOG_HIGH("tzdev_kapi_mem_revoke()\n");
	ret = tzdev_kapi_mem_revoke(bf_ctx->client_id, bf_ctx->shmem_id);
	if (ret) {
		func_ret = ret;
		M4UERR("%s tzdev_kapi_mem_revoke failed - %d\n",
				ctx->name, ret);
	}

out_open:
	M4ULOG_HIGH("tzdev_kapi_close()\n");
	ret = tzdev_kapi_close(bf_ctx->client_id);
	if (ret < 0) {
		func_ret = ret;
		M4UERR("%s tzdev_kapi_close failed - %d\n", ctx->name, ret);
	}

out_iwnotify:
	ret = tz_iwnotify_chain_unregister(bf_ctx->notify_event,
			&bf_ctx->notifier);
	if (ret < 0)
		M4UERR("tz_iwnotify_chain_unregister failed - %d\n", ret);

out_mem_register:
	M4ULOG_HIGH("tzdev_mem_release()\n");
	ret = tzdev_mem_release(bf_ctx->shmem_id);
	if (ret < 0) {
		func_ret = ret;
		M4UERR("%s tzdev_mem_release failed - %d\n", ctx->name, ret);
	}
	__free_page(bf_ctx->shmem_page);

	return func_ret;
}

static int m4u_bf_ctx_deinit(struct m4u_sec_context *ctx)
{
	int ret, func_ret = 0;
	struct m4u_blowfish_context *bf_ctx = ctx->imp;

	bf_ctx->inited = 0;
	M4ULOG_HIGH("m4u_bf_ctx_deinit() %s\n", ctx->name);

	if (bf_ctx->ctx_type == CTX_TYPE_TDRV) {
		ret = m4u_send_tdrv_msg(ctx, UNMAP_IWSHMEM);

		M4ULOG_HIGH("tzdev_kapi_mem_revoke()\n");
		ret = tzdev_kapi_mem_revoke(bf_ctx->client_id,
				bf_ctx->shmem_id);
		if (ret) {
			func_ret = ret;
			M4UERR("%s tzdev_kapi_mem_revoke failed - %d\n",
					ctx->name, ret);
		}
	}

	ret = tzdev_kapi_close(bf_ctx->client_id);
	if (ret < 0) {
		func_ret = ret;
		M4UERR("%s tzdev_kapi_close failed - %d\n", ctx->name, ret);
	}

	ret = tz_iwnotify_chain_unregister(bf_ctx->notify_event,
			&bf_ctx->notifier);
	if (ret < 0) {
		M4UERR("%s tz_iwnotify_chain_unregister failed - %d\n",
				ctx->name, ret);
		return ret;
	}

	return func_ret;
}


static int m4u_sec_ta_open(void)
{
	int ret;

	ret = m4u_bf_ctx_init(&m4u_ta_ctx);
	return ret;
}

static int m4u_sec_tdrv_open(void)
{
	int ret;

	ret = m4u_bf_ctx_init(&m4u_tdrv_ctx);
	return ret;
}

static int m4u_sec_ta_close(void)
{
	return m4u_bf_ctx_deinit(&m4u_ta_ctx);
}
static int m4u_sec_tdrv_close(void)
{
	return m4u_bf_ctx_deinit(&m4u_tdrv_ctx);
}

int m4u_sec_context_init(void)
{
	int ret;

	ret = m4u_sec_ta_open();
	if (ret)
		return ret;

	ret = m4u_sec_tdrv_open();
	if (ret) {
		m4u_sec_ta_close();
		return ret;
	}
	return 0;
}
int m4u_sec_context_deinit(void)
{
	int ret;

	ret = m4u_sec_ta_close();
	ret |= m4u_sec_tdrv_close();
	return ret;
}

int m4u_sec_after_init_cmd(void)
{
	return m4u_sec_ta_close();
}

struct m4u_sec_context *m4u_sec_ctx_get(unsigned int cmd)
{
	struct m4u_sec_context *ctx = NULL;
	struct m4u_blowfish_context *bf_ctx;

	if (cmd == CMD_M4UTL_INIT)
		ctx = &m4u_ta_ctx;
	else
		ctx = &m4u_tdrv_ctx;

	bf_ctx = ctx->imp;
	if (!bf_ctx->inited) {
		M4UERR("m4u_sec_ctx_get before init\n");
		return NULL;
	}
	mutex_lock(&bf_ctx->ctx_lock);
	return ctx;
}

int m4u_sec_ctx_put(struct m4u_sec_context *ctx)
{
	struct m4u_blowfish_context *bf_ctx;

	bf_ctx = ctx->imp;
	mutex_unlock(&bf_ctx->ctx_lock);
	return 0;
}

int m4u_exec_cmd(struct m4u_sec_context *ctx)
{
	int ret;
	struct m4u_blowfish_context *bf_ctx = ctx->imp;

	if (ctx->m4u_msg == NULL) {
		M4UMSG("%s TCI/DCI error\n", __func__);
		return -1;
	}

	if (bf_ctx->ctx_type == CTX_TYPE_TDRV)
		return m4u_send_tdrv_msg(ctx, REE_COMMAND);

	ctx->m4u_msg->bf_msg.notify_event = bf_ctx->notify_event;
	ctx->m4u_msg->bf_msg.notify_flags = 0;
	ret = tzdev_kapi_send(bf_ctx->client_id, ctx->m4u_msg,
			sizeof(*ctx->m4u_msg));
	if (ret < 0) {
		m4u_aee_print("%s tzdev_kapi_send failed:cmd=%d,ret=%d\n",
				ctx->name, ctx->m4u_msg->cmd, ret);
		return ret;
	}

	ret = wait_for_completion_interruptible_timeout(&bf_ctx->completion,
			5 * HZ);
	if (ret <= 0) {
		m4u_aee_print("%s wait completion failed:cmd=%d,ret=%d\n",
				ctx->name, ctx->m4u_msg->cmd, ret);
		return ret;
	}

	ret = tzdev_kapi_recv(bf_ctx->client_id, ctx->m4u_msg,
			sizeof(*ctx->m4u_msg));
	if (ret < 0) {
		m4u_aee_print("%s tzdev_kapi_recv failed:cmd=%d,ret=%d\n",
				ctx->name, ctx->m4u_msg->cmd, ret);
		return ret;
	}

	if (ctx->m4u_msg->bf_msg.notify_flags == 0) {
		m4u_aee_print(
			"%s notify error:0x%x,%d,maybe others use event %d?\n",
			ctx->name, ctx->m4u_msg->cmd,
			ret, bf_ctx->notify_event);
	}

	M4ULOG_HIGH("%s cmd done:cmd=%d,ret=%d, rsp=%d\n",
			ctx->name, ctx->m4u_msg->cmd, ret, ctx->m4u_msg->rsp);
	return 0;
}



