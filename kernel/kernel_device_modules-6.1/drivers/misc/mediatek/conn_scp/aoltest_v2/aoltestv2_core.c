// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

/*******************************************************************************/
/*                     E X T E R N A L   R E F E R E N C E S                   */
/*******************************************************************************/
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/kthread.h>
#include <linux/mm.h>
#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <linux/time.h>
#include <linux/timer.h>
#include "msg_thread.h"
#include "conap_scp.h"
#include "conap_scp_ipi.h"

#include "aoltestv2_core.h"
#include "aoltestv2_netlink.h"

/*******************************************************************************/
/*                             D A T A   T Y P E S                             */
/*******************************************************************************/
enum em_commd_id {
	EM_MSG_DEFAULT = 0,
	EM_MSG_START_TEST = 1,
	EM_MSG_STOP_TEST = 2,
	EM_MSG_START_DATA_TRANS = 3,
	EM_MSG_STOP_DATA_TRANS = 4,
	EM_MSG_MAX_SIZE
};

enum aoltest_core_opid {
	AOLTEST_OPID_DEFAULT = 0,
	AOLTEST_OPID_MODULE_BIND = 1,
	AOLTEST_OPID_MODULE_UNBIND = 2,
	AOLTEST_OPID_SEND_MSG  = 3,
	AOLTEST_OPID_RECV_MSG  = 4,
	AOLTEST_OPID_MAX
};

enum aoltest_core_status {
	AOLTEST_INACTIVE,
	AOLTEST_ACTIVE,
};

enum aoltest_msg_id {
	AOLTEST_MSG_ID_DEFAULT = 0,
	AOLTEST_MSG_ID_WIFI = 1,
	AOLTEST_MSG_ID_BT = 2,
	AOLTEST_MSG_ID_GPS = 3,
	AOLTEST_MSG_ID_MAX
};



struct aoltestv2_test_info {
	uint32_t start_test_id;
	uint32_t start_data_trans_id;
	uint32_t data_trans;
	uint32_t test_data_sz;
	uint8_t *test_data;
};

struct aoltestv2_module_ctx {
	enum conap_scp_drv_type drv_type;
	bool enable;
	uint32_t port;
	struct netlink_client_info client;
	struct conap_scp_drv_cb scp_cb;
	struct aoltestv2_test_info test_info;
};

struct aoltest_core_ctx {
	struct msg_thread_ctx msg_ctx;
	int status;
	struct aoltestv2_module_ctx *module_ctx[AOLTESTV2_MODULE_SIZE];
};


static int _send_msg_to_scp(struct aoltestv2_module_ctx *mctx, u32 msg_id, u32 msg_size,
					u8 *msg_data);


static void aoltest_em_state_change(int state);
static void aoltest_em_msg_notify(uint32_t msg_id, uint32_t *buf, uint32_t size);

static void aoltest_em_ble_state_change(int state);
static void aoltest_em_ble_msg_notify(uint32_t msg_id, uint32_t *buf, uint32_t size);

static struct aoltestv2_module_ctx g_em_module_ctx = {
	.drv_type = DRV_TYPE_EM,
	.enable = false,
	.client = {
		.port = 0,
		.seqnum = 0,
	},
	.scp_cb = {
		.conap_scp_msg_notify_cb = aoltest_em_msg_notify,
		.conap_scp_state_notify_cb = aoltest_em_state_change,
	},
	.test_info = {
		.start_test_id = 0,
		.start_data_trans_id = 0,
		.test_data = NULL
	},
};

static struct aoltestv2_module_ctx g_em_ble_module_ctx = {
	.drv_type = DRV_TYPE_EM_BLE,
	.enable = false,
	.client = {
		.port = 0,
		.seqnum = 0,
	},
	.scp_cb = {
		.conap_scp_msg_notify_cb = aoltest_em_ble_msg_notify,
		.conap_scp_state_notify_cb = aoltest_em_ble_state_change,
	},
	.test_info = {
		.start_test_id = 0,
		.start_data_trans_id = 0,
		.test_data = NULL
	},
};


static struct aoltest_core_ctx g_aoltestv2_ctx;

/*******************************************************************************/
/*                             Data buffer                                     */
/*******************************************************************************/
#define MAX_BUF_LEN     (3 * 1024)

struct aol_data_buf {
	u8 buf[MAX_BUF_LEN];
	u32 size;
	struct list_head list;
};
struct aol_buf_list {
	struct mutex lock;
	struct list_head list;
};
static struct aol_buf_list g_aol_data_buf_list;
static struct mutex g_aoltest_msg_lock;

/*******************************************************************************/
/*                  Message Thread Definition                  */
/*******************************************************************************/
static int opfunc_scp_register(struct msg_op_data *op);
static int opfunc_scp_unregister(struct msg_op_data *op);
static int opfunc_send_msg(struct msg_op_data *op);
static int opfunc_recv_msg(struct msg_op_data *op);

static const msg_opid_func aoltest_core_opfunc[] = {
	[AOLTEST_OPID_MODULE_BIND] = opfunc_scp_register,
	[AOLTEST_OPID_MODULE_UNBIND] = opfunc_scp_unregister,
	[AOLTEST_OPID_SEND_MSG] = opfunc_send_msg,
	[AOLTEST_OPID_RECV_MSG] = opfunc_recv_msg,
};

/*******************************************************************************/
/*                              F U N C T I O N S                              */
/*******************************************************************************/

static struct aol_data_buf *aoltest_data_buffer_alloc(void)
{
	struct aol_data_buf *data_buf = NULL;

	if (list_empty(&g_aol_data_buf_list.list)) {
		data_buf = kmalloc(sizeof(struct aol_data_buf), GFP_KERNEL);
		if (data_buf == NULL)
			return NULL;
		INIT_LIST_HEAD(&data_buf->list);
	} else {
		mutex_lock(&g_aol_data_buf_list.lock);
		data_buf = list_first_entry(&g_aol_data_buf_list.list, struct aol_data_buf, list);
		list_del(&data_buf->list);
		mutex_unlock(&g_aol_data_buf_list.lock);
	}
	return data_buf;
}
static void aoltest_data_buffer_free(struct aol_data_buf *data_buf)
{
	if (!data_buf)
		return;
	mutex_lock(&g_aol_data_buf_list.lock);
	list_add_tail(&data_buf->list, &g_aol_data_buf_list.list);
	mutex_unlock(&g_aol_data_buf_list.lock);
}

static int is_scp_ready(void)
{
	return conap_scp_is_ready();
}

/*******************************************************************************/
/*      O P          F U N C T I O N S                                         */
/*******************************************************************************/
static int opfunc_scp_register(struct msg_op_data *op)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;
	uint32_t module_id = op->op_data[0];
	uint32_t port = op->op_data[1];
	struct aoltestv2_module_ctx *mctx = NULL;

	if (module_id >= AOLTESTV2_MODULE_SIZE) {
		pr_notice("[%s] incorrect module=[%d]", __func__, module_id);
		return -1;
	}

	mctx = ctx->module_ctx[module_id];

	if (mctx->enable && mctx->client.port == port) {
		pr_info("[%s] mod=[%d] already registered", __func__, module_id);
		return 0;
	}

	mctx->client.port = port;
	ret = conap_scp_register_drv(mctx->drv_type, &mctx->scp_cb);
	pr_info("[%s] SCP register drv_type=[%d], ret=[%d]", __func__, mctx->drv_type, ret);

	mctx->enable = true;

	return ret;
}

static int opfunc_scp_unregister(struct msg_op_data *op)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;
	uint32_t module_id = op->op_data[0];
	struct aoltestv2_module_ctx *mctx = NULL;

	if (module_id >= AOLTESTV2_MODULE_SIZE) {
		pr_notice("[%s] incorrect module=[%d]", __func__, module_id);
		return -1;
	}
	mctx = ctx->module_ctx[module_id];

	ret = conap_scp_unregister_drv(mctx->drv_type);
	pr_info("[%s] SCP unregister drv_type=[%d], ret=[%d]", __func__, mctx->drv_type, ret);
	mctx->enable = false;

	return ret;
}

static int opfunc_send_msg(struct msg_op_data *op)
{
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;
	uint32_t msg_id, size, module_id;
	struct aoltestv2_module_ctx *mctx = NULL;
	struct aol_data_buf *data_buf;
	int ret = 0;

	module_id = (uint32_t)op->op_data[0];
	msg_id = (u32)op->op_data[1];
	data_buf = (struct aol_data_buf *)op->op_data[2];
	size = (u32)op->op_data[3];

	if (module_id >= AOLTESTV2_MODULE_SIZE) {
		pr_notice("[%s] incorrect module id=[%d]", __func__, module_id);
		return -1;
	}

	mctx = ctx->module_ctx[module_id];

	ret = _send_msg_to_scp(mctx, msg_id, size, (data_buf == NULL ? NULL : data_buf->buf));

	aoltest_data_buffer_free(data_buf);
	if (ret) {
		pr_notice("[%s] snd_msg fail [%d]", __func__, ret);
		return ret;
	}

	return ret;
}

static int opfunc_recv_msg(struct msg_op_data *op)
{
	int ret = 0;

	uint32_t module_id = (uint32_t)op->op_data[0];
	uint32_t msg_id = (uint32_t)op->op_data[1];
	struct aol_data_buf *data_buf = (struct aol_data_buf *)op->op_data[2];
	uint32_t size = (uint32_t)op->op_data[3];
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;
	struct aoltestv2_module_ctx *mctx = NULL;

	if (module_id >= AOLTESTV2_MODULE_SIZE) {
		pr_notice("[%s] incorrect module [%d]", __func__, module_id);
		return -1;
	}
	mctx = ctx->module_ctx[module_id];

	if (mctx == NULL) {
		pr_notice("[%s] no module context", __func__);
		return -1;
	}

	pr_info("[%s] Send to netlink client, sz=[%d]", __func__, size);
	aoltestv2_netlink_send_to_native(&mctx->client, "[AOLTEST]", msg_id, data_buf->buf, size);
	aoltest_data_buffer_free(data_buf);

	return ret;
}

/*******************************************************************************/
/*      handlers                                  */
/*******************************************************************************/


static void aoltest_msg_notify(uint32_t module_id, uint32_t msg_id,
				uint32_t *buf, uint32_t size)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;
	struct aoltestv2_module_ctx *mctx = NULL;
	struct aol_data_buf *data_buf = NULL;

	pr_info("[%s] msg_id=[%d] size=[%d]\n", __func__, msg_id, size);

	if (module_id >= AOLTESTV2_MODULE_SIZE) {
		pr_notice("[%s] incorrect module [%d]", __func__, module_id);
		return;
	}

	mctx = ctx->module_ctx[module_id];
	if (mctx == NULL || mctx->enable == false) {
		pr_notice("[%s] module=[%d] is not enable", __func__, module_id);
		return;
	}

	if (size > MAX_BUF_LEN) {
		pr_info("[%s] size [%d] exceed expected [%d]", __func__, size, MAX_BUF_LEN);
		return;
	}

	if (size > 0) {
		data_buf = aoltest_data_buffer_alloc();
		if (!data_buf) {
			pr_notice("[%s] data buf is empty", __func__);
			return;
		}
		memcpy(&(data_buf->buf[0]), buf, size);
	}
	ret = msg_thread_send_4(&ctx->msg_ctx, AOLTEST_OPID_RECV_MSG,
							module_id, msg_id, (size_t)data_buf, size);

	if (ret)
		pr_info("[%s] Notify recv msg fail, ret=[%d]\n", __func__, ret);

}

void aoltest_em_msg_notify(uint32_t msg_id, uint32_t *buf, uint32_t size)
{
	aoltest_msg_notify(AOLTESTV2_MODULE_EM, msg_id, buf, size);
}

static int _send_msg_to_scp(struct aoltestv2_module_ctx *mctx, uint32_t msg_id,
		uint32_t msg_size, uint8_t *msg_data)
{
	int ret;

	ret = is_scp_ready();
	if (ret == 0) {
		pr_notice("[%s] scp not ready", __func__);
		return -1;
	}

	mutex_lock(&g_aoltest_msg_lock);

	ret = conap_scp_send_message(mctx->drv_type, msg_id, msg_data, msg_size);
	pr_info("[%s] send ret=%d msg_id=[%d]",
							__func__, ret, msg_id);
	mutex_unlock(&g_aoltest_msg_lock);
	return ret;
}


static void aoltestv2_common_state_change(uint32_t module_id, int state)
{
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;
	struct aoltestv2_module_ctx *mctx = ctx->module_ctx[module_id];
	int ret;

	pr_info("[%s] reason=[%d] module_id=[%d]", __func__, state, module_id);

	if (ctx->status == AOLTEST_INACTIVE) {
		pr_info("[%s] module ctx [%d] is inactive", __func__, module_id);
		return;
	}
	/* state = 1: scp ready */
	/* state = 0: scp stop */
	/* support test recover */
	if (state == 1) {
		if (mctx && mctx->test_info.test_data) {
			struct aol_data_buf *data_buf = NULL;
			struct aoltestv2_test_info *test_info = NULL;

			test_info = &mctx->test_info;
			data_buf = aoltest_data_buffer_alloc();
			if (!data_buf) {
				pr_notice("[%s] data buf is empty", __func__);
				return;
			}
			memcpy(&(data_buf->buf[0]), test_info->test_data, test_info->test_data_sz);
			ret = msg_thread_send_4(&ctx->msg_ctx, AOLTEST_OPID_SEND_MSG,
						module_id, test_info->start_test_id,
						(size_t)data_buf, test_info->test_data_sz);

			pr_info("[%s] start test ret=[%d] data_trans=[%d]", __func__,
						ret, test_info->data_trans);

			if (ret == 0 && test_info->data_trans == 1) {
				data_buf = aoltest_data_buffer_alloc();
				if (!data_buf) {
					pr_notice("[%s] data buf is empty", __func__);
					return;
				}
				memcpy(&(data_buf->buf[0]), &(test_info->data_trans),
									sizeof(uint32_t));

				ret = msg_thread_send_4(&ctx->msg_ctx, AOLTEST_OPID_SEND_MSG,
							module_id, test_info->start_data_trans_id,
							(size_t)data_buf, sizeof(uint32_t));

				pr_info("[%s] data trans ret=[%d]", __func__, ret);
			}

		}

	}
}

void aoltest_em_state_change(int state)
{
	aoltestv2_common_state_change(AOLTESTV2_MODULE_EM, state);
}


static void aoltest_em_ble_state_change(int state)
{
	aoltestv2_common_state_change(AOLTESTV2_MODULE_BLE, state);
}

static void aoltest_em_ble_msg_notify(unsigned int msg_id, unsigned int *buf, unsigned int size)
{
	aoltest_msg_notify(AOLTESTV2_MODULE_BLE, msg_id, buf, size);
}

static int aoltest_core_handler(uint32_t module_id, u32 msg_id, void *data, u32 sz)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;
	struct aoltestv2_module_ctx *mctx = NULL;

	pr_info("[%s] Get msg_id: %d\n", __func__, msg_id);

	if (module_id >= AOLTESTV2_MODULE_SIZE) {
		pr_notice("[%s] incorrect module [%d]", __func__, module_id);
		return -1;
	}

	mctx = ctx->module_ctx[module_id];

	if (mctx == NULL || mctx->enable == false) {
		pr_info("[%s] module test ctx [%d] is inactive", __func__, module_id);
		return -1;
	}

	if (sz > MAX_BUF_LEN) {
		pr_notice("[%s] module[%d][%d] data size [%d] exceed expected [%d]",
				__func__, module_id, msg_id, sz, MAX_BUF_LEN);
		return -2;
	}

	ret = _send_msg_to_scp(mctx, msg_id, sz, data);

	if (ret)
		pr_info("[%s] Send to msg thread fail, ret=[%d]\n", __func__, ret);

	return ret;
}

static int aoltestv2_core_bind(uint32_t port, uint32_t module_id)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;

	if (module_id >= AOLTESTV2_MODULE_SIZE) {
		pr_notice("[%s] incorrect module=[%d]", __func__, module_id);
		return -1;
	}

	ret = msg_thread_send_2(&ctx->msg_ctx, AOLTEST_OPID_MODULE_BIND, module_id, port);

	if (ret)
		pr_info("[%s] Send to msg thread fail, ret=[%d]\n", __func__, ret);

	pr_info("[%s] ============", __func__);

	return ret;
}

static int aoltestv2_core_unbind(uint32_t module_id)
{
	int ret = 0;
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;

	if (module_id >= AOLTESTV2_MODULE_SIZE) {
		pr_notice("[%s] incorrect module=[%d]", __func__, module_id);
		return -1;
	}

	ret = msg_thread_send_1(&ctx->msg_ctx, AOLTEST_OPID_MODULE_UNBIND, module_id);

	if (ret)
		pr_info("[%s] Send to msg thread fail, ret=[%d]\n", __func__, ret);

	pr_info("[%s] already done", __func__);

	return 0;
}

static int aoltestv2_core_start(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz)
{
	int ret;
	uint8_t *data_buf;
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;
	struct aoltestv2_module_ctx *mctx = NULL;

	ret = aoltest_core_handler(module_id, msg_id, data, sz);
	if (ret == 0) {
		mctx = ctx->module_ctx[module_id];
		/* force free */
		kfree(mctx->test_info.test_data);
		mctx->test_info.test_data = NULL;

		data_buf = kmalloc(sz, GFP_KERNEL);
		if (data_buf == NULL)
			return -1;
		memcpy(data_buf, data, sz);
		mctx->test_info.test_data = data_buf;
		mctx->test_info.test_data_sz = sz;
		mctx->test_info.start_test_id = msg_id;
		mctx->test_info.data_trans = 0;
	}

	pr_info("[%s] done ret=[%d]", __func__, ret);
	return ret;
}

static int aoltestv2_core_stop(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz)
{
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;
	struct aoltestv2_module_ctx *mctx = NULL;
	int ret;

	ret = aoltest_core_handler(module_id, msg_id, data, sz);
	if (ret == 0) {
		mctx = ctx->module_ctx[module_id];
		kfree(mctx->test_info.test_data);
		mctx->test_info.test_data = NULL;
		mctx->test_info.test_data_sz = 0;
		mctx->test_info.start_test_id = 0;
		mctx->test_info.data_trans = 0;
	}
	pr_info("[%s] done ret=[%d]", __func__, ret);

	return ret;
}


static int aoltestv2_core_data_trans(uint32_t module_id, uint32_t msg_id, void *data, uint32_t sz)
{
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;
	struct aoltestv2_module_ctx *mctx = NULL;
	uint32_t data_trans = 0;
	int ret;

	if (sz != sizeof(uint32_t)) {
		pr_notice("[%s] incorrect data size[%d]", __func__, sz);
		return -1;
	}

	ret = aoltest_core_handler(module_id, msg_id, data, sz);
	if (ret == 0) {
		memcpy(&data_trans, data, sz);
		mctx = ctx->module_ctx[module_id];
		if (data_trans == 1) {
			mctx->test_info.data_trans = 1;
			mctx->test_info.start_data_trans_id = msg_id;
		} else if (data_trans == 0) {
			mctx->test_info.data_trans = 0;
			mctx->test_info.start_data_trans_id = 0;
		} else
			pr_notice("[%s] incorrect param=[%d]", __func__, data_trans);
	}
	pr_info("[%s] done ret=[%d]", __func__, ret);

	return ret;
}

int aoltestv2_core_init(void)
{
	struct aoltestv2_netlink_event_cb nl_cb;
	struct aoltest_core_ctx *ctx = &g_aoltestv2_ctx;
	int ret;

	/* data buffer init */
	mutex_init(&g_aoltest_msg_lock);
	mutex_init(&g_aol_data_buf_list.lock);
	INIT_LIST_HEAD(&g_aol_data_buf_list.list);

	/* aoltest ctx */
	memset(&g_aoltestv2_ctx, 0, sizeof(struct aoltest_core_ctx));

	/* Create EM test thread */
	ret = msg_thread_init(&ctx->msg_ctx, "em_test_v2_thrd",
					aoltest_core_opfunc, AOLTEST_OPID_MAX);

	if (ret) {
		pr_info("EM test thread init fail, ret=[%d]\n", ret);
		return -1;
	}

	ctx->module_ctx[0] = &g_em_module_ctx;
	ctx->module_ctx[1] = &g_em_ble_module_ctx;

	/* Init netlink */
	nl_cb.aoltestv2_bind = aoltestv2_core_bind;
	nl_cb.aoltestv2_unbind = aoltestv2_core_unbind;
	nl_cb.aoltestv2_start = aoltestv2_core_start;
	nl_cb.aoltestv2_stop = aoltestv2_core_stop;
	nl_cb.aoltestv2_handler = aoltest_core_handler;
	nl_cb.aoltestv2_data_trans = aoltestv2_core_data_trans;

	ret = aoltestv2_netlink_init(&nl_cb);

	if (ret)
		return ret;

	ctx->status = AOLTEST_ACTIVE;

	return 0;
}

void aoltestv2_core_deinit(void)
{
	aoltestv2_netlink_deinit();
	pr_info("[%s] deinit done", __func__);
}
