// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/workqueue.h>

#include "conap_scp.h"
#include "conap_scp_priv.h"
#include "conap_platform_data.h"
#include "msg_thread.h"
#include "conap_scp_ipi.h"

#include "connectivity_build_in_adapter.h"

#define MAX_MSG_SZ_BY_IPI	512

/* platform driver */
const struct of_device_id conn_scp_of_ids[] = {
	{
		.compatible = "mediatek,mt6985-conn_scp",
	#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6985)
		.data = NULL,
	#endif
	},
	{
		.compatible = "mediatek,mt6897-conn_scp",
	#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6897)
		.data = NULL,
	#endif
	},
	{
		.compatible = "mediatek,mt6989-conn_scp",
	#if IS_ENABLED(CONFIG_MTK_COMBO_CHIP_CONSYS_6989)
		.data = NULL,
	#endif
	},

	{}
};


static int conn_scp_probe(struct platform_device *pdev);
static int conn_scp_remove(struct platform_device *pdev);

static struct platform_driver g_connscp_dev_drv = {
	.probe = conn_scp_probe,
	.remove = conn_scp_remove,
	.driver = {
		   .name = "conn_scp",
		   .owner = THIS_MODULE,
		   .of_match_table = conn_scp_of_ids,
		   .probe_type = PROBE_FORCE_SYNCHRONOUS,
		   },
};

struct conap_scp_drv_user g_drv_user[CONAP_SCP_DRV_NUM];
struct conap_scp_core_ctx {
	unsigned int enable;
	unsigned int chip_info;
	phys_addr_t emi_phy_addr;
	unsigned int state; /* stop: 0, ready : 1 */
	struct mutex lock;
	struct msg_thread_ctx tx_msg_thread;

	struct completion msg_send_comp;
	struct completion msg_recv_comp;
};

static struct conap_scp_core_ctx g_core_ctx;

static int opfunc_send_msg(struct msg_op_data *op);
static int opfunc_is_drv_ready(struct msg_op_data *op);
static int opfunc_is_drv_ready_ack(struct msg_op_data *op);
static int opfunc_scp_state_change(struct msg_op_data *op);
static int opfunc_recv_msg(struct msg_op_data *op);
static int opfunc_recv_shm_msg(struct msg_op_data *op);

static const msg_opid_func conap_scp_core_opfunc[] = {
	[CONAP_SCP_OPID_STATE_CHANGE] = opfunc_scp_state_change,
	[CONAP_SCP_OPID_SEND_MSG] = opfunc_send_msg,
	[CONAP_SCP_OPID_DRV_READY] = opfunc_is_drv_ready,
	[CONAP_SCP_OPID_DRV_READY_ACK] = opfunc_is_drv_ready_ack,
	[CONAP_SCP_OPID_RECV_MSG] = opfunc_recv_msg,
	[CONAP_SCP_OPID_RECV_SHM_MSG] = opfunc_recv_shm_msg,
};

#define MAX_MSG_SZ		1024
uint8_t g_msg_buf[MAX_MSG_SZ];

uint16_t g_cur_msg_drv;
uint16_t g_cur_msg_id;
uint16_t g_cur_recv_sz;
uint8_t *g_cur_msg_recv_buf;

struct msg_data_2 {
	uint32_t param0;
	uint32_t param1;
};

struct msg_data_4 {
	uint32_t param0;
	uint32_t param1;
	uint32_t param2;
	uint32_t param3;
};

/* connsys state notifier */
enum state_chg_drv_type {
	STATE_CHG_DRV_NONE = 0,
	STATE_CHG_DRV_CONN = 1,
	STATE_CHG_DRV_SCP = 2,
};

static int conn_state_event_handler(struct notifier_block *this,
			unsigned long event, void *ptr);
static struct notifier_block g_conn_state_notifier = {
	.notifier_call = conn_state_event_handler,
};

/* conn drv state */
enum conn_drv_type {
	CONN_DRV_WIFI	= 0,
	CONN_DRV_BT		= 1,
	CONN_DRV_GPS	= 2,
	CONN_DRV_SIZE
};

struct conn_drv_state {
	uint16_t drv_type;
	uint16_t drv_en;
};
static uint8_t g_conn_drv_state;


static int _send_msg(enum conap_scp_drv_type drv_type, uint16_t msg_id,
				uint8_t *msg_buf, uint32_t msg_sz)
{
	uint32_t sent_len = 0, sz = 0;
	int wait_ret;
	int ret;
	const unsigned int ipi_msg_data_sz = conap_scp_ipi_msg_sz();

	if (!conap_scp_ipi_is_scp_ready()) {
		pr_warn("[%s] scp is not ready", __func__);
		return -1;
	}

	reinit_completion(&g_core_ctx.msg_send_comp);

	/* send tx request */
	ret = conap_scp_ipi_send_cmd(DRV_TYPE_CORE, CONAP_SCP_CORE_REQ_TX, drv_type, msg_sz, 0);
	if (ret < 0) {
		pr_warn("[%s] ipi_send fail, rety", __func__);
		return -1;
	}

	/* wait for tx request accept */
	wait_ret = wait_for_completion_timeout(&g_core_ctx.msg_send_comp,
					msecs_to_jiffies(2000));
	if (wait_ret == 0) {
		pr_warn("[%s] send msg comp timeout", __func__);
		return CONN_TIMEOUT;
	}

	pr_info("[%s] >>>> type=[%d] id=[%d] sz=[%d] ipi_sz=[%d]", __func__,
			drv_type, msg_id, msg_sz, ipi_msg_data_sz);
	while (sent_len <= msg_sz) {
		sz = min(ipi_msg_data_sz, msg_sz - sent_len);

		ret = conap_scp_ipi_send_data(drv_type, msg_id, msg_sz,
				(msg_buf != NULL ? &(msg_buf[sent_len]) : NULL), sz);
		if (ret < 0) {
			pr_warn("[%s] ipi_send fail, rety", __func__);
			continue;
		}
		sent_len += ret;
		if (sent_len >= msg_sz)
			break;
	}
	//pr_info("[_send_msg] send data done");
	return 0;
}

static int _send_msg_by_shm(enum conap_scp_drv_type drv_type, uint16_t msg_id,
				uint8_t *msg_buf, uint32_t msg_sz)
{
	int ret;
	int wait_ret;

	if (!conap_scp_ipi_is_scp_ready()) {
		pr_notice("[%s] scp is not ready", __func__);
		return -1;
	}

	ret = conap_scp_shm_write(msg_buf, msg_sz);

	pr_info("[%s] msg_id=[%d] sz=[%d] ret=[%x]", __func__, msg_id, msg_sz, ret);

	reinit_completion(&g_core_ctx.msg_send_comp);

	/* send tx request */
	ret = conap_scp_ipi_send_shm_cmd(DRV_TYPE_CORE, CONAP_SCP_CORE_TX_SHM,
				drv_type, msg_id, ret, msg_sz);
	if (ret < 0) {
		pr_notice("[%s] ipi_send fail, rety", __func__);
		return -1;
	}

	/* wait for tx request accept */
	wait_ret = wait_for_completion_timeout(&g_core_ctx.msg_send_comp,
					msecs_to_jiffies(2000));
	if (wait_ret == 0) {
		pr_notice("[%s] send msg comp timeout", __func__);
		return CONN_TIMEOUT;
	}

	return 0;
}

/*********************************************************************/
/* CORE OP FUNC */
/*********************************************************************/
static int opfunc_send_msg(struct msg_op_data *op)
{
	unsigned int drv_type = (unsigned int)op->op_data[0];
	unsigned int msg_id = (unsigned int)op->op_data[1];
	unsigned char *msg_buf = (unsigned char *)op->op_data[2];
	unsigned int msg_sz = (unsigned int)op->op_data[3];

	if (conap_scp_ipi_shm_support() && msg_sz > MAX_MSG_SZ_BY_IPI)
		return _send_msg_by_shm(drv_type, msg_id, msg_buf, msg_sz);
	return _send_msg(drv_type, msg_id, msg_buf, msg_sz);
}

static int opfunc_is_drv_ready(struct msg_op_data *op)
{
	int ret = 0;
	unsigned int drv_type = (unsigned int)op->op_data[0];

	pr_info("[%s] drv=[%d] ", __func__, drv_type);
	ret = conap_scp_ipi_send_cmd(DRV_TYPE_CORE, CONAP_SCP_CORE_DRV_QRY, drv_type, 0, 0);

	if (ret) {
		pr_warn("[%s] fail [%d]", __func__, ret);
		return -2;
	}
	return 0;
}

static int opfunc_is_drv_ready_ack(struct msg_op_data *op)
{
	unsigned int drv_type = (unsigned int)op->op_data[0];
	int ret;

	ret = g_drv_user[drv_type].enable;
	pr_info("[%s] ack type=[%d] ret=[%d]", __func__, drv_type, ret);
	ret = conap_scp_ipi_send_cmd(DRV_TYPE_CORE, CONAP_SCP_CORE_DRV_QRY_ACK, drv_type, ret, 0);
	if (ret)
		pr_warn("[%s] send ipi cmd fail [%d]", __func__, ret);
	return ret;
}


static int opfunc_scp_state_change(struct msg_op_data *op)
{
	unsigned int drv_type = (unsigned int)op->op_data[0];
	unsigned int cur_state = (unsigned int)op->op_data[1];
	int ret, i;

	if (drv_type > STATE_CHG_DRV_SCP)
		return -1;


	pr_info("[%s] type=[%d] state=[%d] conn=[%d] scp ready=[%d]", __func__,
				drv_type, cur_state, g_core_ctx.enable,
				conap_scp_ipi_is_scp_ready());

	if (cur_state == 1 && conap_scp_ipi_is_scp_ready() == 0) {
		g_core_ctx.enable = 0;
		pr_info("[%s] scp not enable yet", __func__);
		return 0;
	}

	/* SCP ready */
	if (cur_state == 1) {
		pr_info("[%s] send init cmd", __func__);
		ret = conap_scp_ipi_send_cmd(DRV_TYPE_CORE, CONAP_SCP_CORE_INIT,
					connsys_scp_shm_get_addr(), connsys_scp_shm_get_size(), 0);
		if (ret) {
			pr_info("[SCP_STATE_CHG] ipi handshake ret=[%d]", ret);
			return -1;
		}
	}

	if (cur_state == 1)
		g_core_ctx.enable = 1;
	else
		g_core_ctx.enable = 0;

	g_core_ctx.state = cur_state;

	/* make sure intf is init done */
	msleep(100);

	pr_info("[%s] drv state=[%d]", __func__, g_conn_drv_state);
	if (g_conn_drv_state > 0) {
		struct conn_drv_state state;

		for (i = 0; i < CONN_DRV_SIZE; i++) {
			if ((g_conn_drv_state & (1 << i)) > 0) {
				state.drv_type = i;
				state.drv_en = 1;
				pr_info("[%s] type=[%d] en", __func__, i);
				ret = _send_msg(DRV_TYPE_CONN, 0,
							(unsigned char *)&state, sizeof(state));
				if (ret)
					pr_notice("[%s] send msg fail ret=[%d]", __func__, ret);
			}
		}
	}

	/* notify users */
	for (i = 0; i < CONAP_SCP_DRV_NUM; i++) {
		if (g_drv_user[i].enable && g_drv_user[i].drv_cb.conap_scp_state_notify_cb)
			(*g_drv_user[i].drv_cb.conap_scp_state_notify_cb)(cur_state);
	}

	return 0;
}

static int opfunc_recv_msg(struct msg_op_data *op)
{
	uint16_t msg_drv = (uint16_t)op->op_data[0];
	uint16_t msg_sz = (uint16_t)op->op_data[1];
	int ret, wait_ret;

	g_cur_msg_drv = msg_drv;
	g_cur_recv_sz = 0;
	g_cur_msg_recv_buf = &(g_msg_buf[0]);

	reinit_completion(&g_core_ctx.msg_recv_comp);
	ret = conap_scp_ipi_send_cmd(DRV_TYPE_CORE, CONAP_SCP_CORE_TX_ACCEP, msg_drv, 0, 0);
	if (ret) {
		g_cur_msg_drv = 0;
		g_cur_recv_sz = 0;
		g_cur_msg_recv_buf = NULL;
		pr_warn("[recv_msg] timeout ret=[%d]", ret);
		return CONN_TIMEOUT;
	}

	wait_ret = wait_for_completion_timeout(&g_core_ctx.msg_recv_comp,
					msecs_to_jiffies(2000));

	if (wait_ret == 0) {
		pr_warn("[%s] send msg timeout", __func__);
		return CONN_TIMEOUT;
	}

	//pr_info("[%s] recv done [%d][%d]", __func__, g_cur_recv_sz, msg_sz);
	if (g_cur_recv_sz == msg_sz) {
		if (g_cur_msg_drv < CONAP_SCP_DRV_NUM && g_drv_user[g_cur_msg_drv].enable) {
			(*g_drv_user[g_cur_msg_drv].drv_cb.conap_scp_msg_notify_cb)(g_cur_msg_id,
							(unsigned int *)&(g_msg_buf[0]), msg_sz);
		}
	}

	g_cur_msg_drv = 0;
	g_cur_recv_sz = 0;
	g_cur_msg_recv_buf = NULL;

	return 0;
}

static int opfunc_recv_shm_msg(struct msg_op_data *op)
{
	uint16_t msg_drv = (uint16_t)op->op_data[0];
	uint32_t msg_id = (uint32_t)op->op_data[1];
	uint32_t msg_oft = (uint32_t)op->op_data[2];
	uint16_t msg_sz = (uint16_t)op->op_data[3];
	int ret;

	if (msg_drv >= CONAP_SCP_DRV_NUM)
		return -1;

	if (msg_sz >= MAX_MSG_SZ)
		return -1;

	ret = conap_scp_shm_read(&(g_msg_buf[0]), msg_oft, msg_sz);
	pr_info("[%s] ret=[%d] ", __func__, ret);

	ret = conap_scp_ipi_send_cmd(DRV_TYPE_CORE, CONAP_SCP_CORE_TX_DONE, msg_drv, 0, 0);
	if (ret)
		pr_notice("[%s] TX_DONE fail", __func__);

	if (g_drv_user[msg_drv].enable) {
		(*g_drv_user[msg_drv].drv_cb.conap_scp_msg_notify_cb)(msg_id,
					(unsigned int *)&(g_msg_buf[0]), msg_sz);
	}

	return 0;
}



/*********************************************************************/
/* PRIVATE API */
/*********************************************************************/
static int _conap_scp_is_scp_ready(void)
{
	if (mutex_lock_killable(&g_core_ctx.lock))
		return -1;
	if (g_core_ctx.state == 0) {
		mutex_unlock(&g_core_ctx.lock);
		return 0;
	}
	mutex_unlock(&g_core_ctx.lock);
	return 1;
}

static void conap_scp_msg_notify(uint16_t drv_type, uint16_t msg_id,
			uint16_t total_sz, uint8_t *data, uint32_t this_sz)
{
	int ret;

	if (drv_type == DRV_TYPE_CORE) {
		if (msg_id == CONAP_SCP_CORE_ACK) {
			complete(&g_core_ctx.msg_send_comp);
			return;
		} else if (msg_id == CONAP_SCP_CORE_DRV_QRY) {
			struct msg_data_2 *drv_rdy = (struct msg_data_2 *)data;

			ret = msg_thread_send_1(&g_core_ctx.tx_msg_thread
						, CONAP_SCP_OPID_DRV_READY_ACK
						, (size_t)drv_rdy->param0);
			if (ret)
				pr_warn("[conap_msg_notify] send msg fail [%d]", ret);
			return;
		} else if (msg_id == CONAP_SCP_CORE_DRV_QRY_ACK) {
			struct msg_data_2 *rdy_ack = (struct msg_data_2 *)data;
			struct conap_scp_drv_user *user = NULL;

			if (rdy_ack->param0 >= CONAP_SCP_DRV_NUM) {
				pr_warn("[conap_msg_notify] invalid drv [%d]", rdy_ack->param0);
				return;
			}
			user = &g_drv_user[rdy_ack->param0];
			user->is_rdy_ret = rdy_ack->param1;
			complete(&user->is_rdy_comp);
			return;
		} else if (msg_id == CONAP_SCP_CORE_REQ_TX) {
			struct msg_data_2 *msg = (struct msg_data_2 *)data;

			ret = msg_thread_send_2(&g_core_ctx.tx_msg_thread
						, CONAP_SCP_OPID_RECV_MSG
						, (size_t)msg->param0, (size_t)msg->param1);
			if (ret)
				pr_notice("[conap_msg_notify] REQ_TX fail [%d]", ret);
			return;

		} else if (msg_id == CONAP_SCP_CORE_TX_ACCEP) {
			complete(&g_core_ctx.msg_send_comp);
			return;
		} else if (msg_id == CONAP_SCP_CORE_TX_SHM) {
			struct msg_data_4 *msg = (struct msg_data_4 *)data;

			ret = msg_thread_send_4(&g_core_ctx.tx_msg_thread
						, CONAP_SCP_OPID_RECV_SHM_MSG
						, (size_t)msg->param0, (size_t)msg->param1
						, (size_t)msg->param2, (size_t)msg->param3);
			if (ret)
				pr_notice("[conap_msg_notify] TX_SHM fail [%d]", ret);
			return;

		} else if (msg_id == CONAP_SCP_CORE_TX_DONE) {
			complete(&g_core_ctx.msg_send_comp);
			return;
		}
	}

	if (total_sz > MAX_MSG_SZ) {
		pr_warn("[%s] invalid size [%d]", __func__, total_sz);
		return;
	}
	if (g_cur_recv_sz > total_sz) {
		pr_warn("[%s] msg invalid size [%d][%d]", __func__, g_cur_recv_sz, total_sz);
		return;
	}
	if (g_cur_msg_recv_buf == NULL) {
		pr_warn("[%s] msg buf is null", __func__);
		return;
	}

	g_cur_msg_id = msg_id;
	memcpy(&(g_cur_msg_recv_buf[g_cur_recv_sz]), data, this_sz);
	g_cur_recv_sz += this_sz;
	if (g_cur_recv_sz == total_sz) {
		g_cur_msg_recv_buf = NULL;
		complete(&g_core_ctx.msg_recv_comp);
	}

}

static void conap_scp_ipi_ctrl_notify(unsigned int state)
{
	int ret;

	if ((g_core_ctx.state == 1 && state == 1) ||
		(g_core_ctx.state == 0 && state == 0))
		return;

	pr_info("[%s] state=[%d]->[%d]", __func__, g_core_ctx.state, state);

	ret = msg_thread_send_2(&g_core_ctx.tx_msg_thread,
			CONAP_SCP_OPID_STATE_CHANGE, STATE_CHG_DRV_SCP, (size_t)state);
	if (ret)
		pr_warn("[%s] msg_send fail [%d]", __func__, ret);
}

int conn_state_event_handler(struct notifier_block *this,
			unsigned long event, void *ptr)
{
	int ret = 0;
	struct conn_drv_state state;

	if (event == conn_pwr_on || event == conn_pwr_off) {
		pr_info("[%s] ========= event =[%lu]",
					__func__, event);
		return 0;
	}

	if (connsys_scp_conn_state_en() == 0) {
		pr_info("[%s] conn state not enable, evt=[%lu]", __func__, event);
		return 0;
	}

	pr_info("[%s] event=[%lu] +++ ", __func__, event);
	if (event == conn_wifi_on) {
		state.drv_type = CONN_DRV_WIFI;
		state.drv_en = 1;
		ret = conap_scp_send_message(DRV_TYPE_CONN, 0,
						(unsigned char *)&state, sizeof(state));
		if (ret == 0)
			g_conn_drv_state |= (0x1 << CONN_DRV_WIFI);
	} else if (event == conn_bt_on) {
		state.drv_type = CONN_DRV_BT;
		state.drv_en = 1;
		ret = conap_scp_send_message(DRV_TYPE_CONN, 0,
						(unsigned char *)&state, sizeof(state));
		if (ret == 0)
			g_conn_drv_state |= (0x1 << CONN_DRV_BT);
	} else if (event == conn_wifi_off) {
		state.drv_type = CONN_DRV_WIFI;
		state.drv_en = 0;
		ret = conap_scp_send_message(DRV_TYPE_CONN, 0,
						(unsigned char *)&state, sizeof(state));
		if (ret == 0)
			g_conn_drv_state &= ~(0x1 << CONN_DRV_WIFI);
	} else if (event == conn_bt_off) {
		state.drv_type = CONN_DRV_BT;
		state.drv_en = 0;
		ret = conap_scp_send_message(DRV_TYPE_CONN, 0,
						(unsigned char *)&state, sizeof(state));
		if (ret == 0)
			g_conn_drv_state &= ~(0x1 << CONN_DRV_BT);
	}

	pr_info("[%s] ret=[%d] -------", __func__, ret);

	return 0;
}


/*********************************************************************/
/* OPEN API */
/*********************************************************************/
int conap_scp_send_message(enum conap_scp_drv_type type,
					unsigned int msg_id,
					unsigned char *buf, unsigned int size)
{
	int ret = 0;

	if (g_core_ctx.enable == 0)
		return CONN_CONAP_NOT_SUPPORT;

	if (type == DRV_TYPE_CORE || type >= CONAP_SCP_DRV_NUM)
		return CONN_INVALID_ARGUMENT;

	if (size > MAX_MSG_SZ || size % 4 > 0)
		return CONN_INVALID_ARGUMENT;

	if (_conap_scp_is_scp_ready() != 1)
		return CONN_NOT_READY;

	ret = msg_thread_send_wait_4(&g_core_ctx.tx_msg_thread,
					CONAP_SCP_OPID_SEND_MSG, MSG_OP_TIMEOUT,
					(size_t)type, msg_id, (size_t)buf, size);
	if (ret)
		pr_warn("[%s] msg_send fail [%d]", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(conap_scp_send_message);

int conap_scp_is_drv_ready(enum conap_scp_drv_type type)
{
	int ret = 0, wait_ret = 1;
	struct conap_scp_drv_user *user = NULL;
	int is_ready = 0;

	if (_conap_scp_is_scp_ready() != 1)
		return CONN_NOT_READY;

	if (type >= CONAP_SCP_DRV_NUM)
		return CONN_INVALID_ARGUMENT;

	user = &g_drv_user[type];
	user->is_rdy_ret = 0;
	reinit_completion(&user->is_rdy_comp);

	/* send msg */
	ret = msg_thread_send_1(&g_core_ctx.tx_msg_thread, CONAP_SCP_OPID_DRV_READY, (size_t)type);
	if (ret) {
		pr_warn("[%s] send msg fail [%d]", __func__, ret);
		return ret;
	}

	wait_ret = wait_for_completion_timeout(&user->is_rdy_comp,
				msecs_to_jiffies(2000));

	if (wait_ret == 0) {
		pr_warn("[%s] timeout ", __func__);
		return CONN_TIMEOUT;
	}

	is_ready = user->is_rdy_ret;
	pr_info("[%s] is drv ready type=[%d] ret=[%d]", __func__, type, is_ready);

	return is_ready;
}
EXPORT_SYMBOL(conap_scp_is_drv_ready);


int conap_scp_is_ready(void)
{
	return conap_scp_ipi_is_scp_ready();
}
EXPORT_SYMBOL(conap_scp_is_ready);

int conap_scp_register_drv(enum conap_scp_drv_type type, struct conap_scp_drv_cb *cb)
{
	if (g_core_ctx.enable == 0)
		return CONN_CONAP_NOT_SUPPORT;
	if (type < 0 || type >= CONAP_SCP_DRV_NUM)
		return -EINVAL;
	if (g_drv_user[type].enable)
		return -EEXIST;

	if (mutex_lock_killable(&g_core_ctx.lock))
		return -1;

	memcpy(&g_drv_user[type].drv_cb, cb, sizeof(struct conap_scp_drv_cb));
	g_drv_user[type].enable = 1;

	mutex_unlock(&g_core_ctx.lock);
	return 0;
}
EXPORT_SYMBOL(conap_scp_register_drv);

int conap_scp_unregister_drv(enum conap_scp_drv_type type)
{
	if (g_core_ctx.enable == 0)
		return CONN_CONAP_NOT_SUPPORT;
	if (type < 0 || type >= CONAP_SCP_DRV_NUM)
		return -EINVAL;
	if (!g_drv_user[type].enable)
		return -EEXIST;

	if (mutex_lock_killable(&g_core_ctx.lock))
		return -1;
	memset(&g_drv_user[type].drv_cb, 0, sizeof(struct conap_scp_drv_cb));
	g_drv_user[type].enable = 0;

	mutex_unlock(&g_core_ctx.lock);

	return 0;
}
EXPORT_SYMBOL(conap_scp_unregister_drv);


struct conn_debug_cmd {
	uint32_t drv;
	uint32_t cmd;
	uint32_t param;
};

void conap_scp_cmd_handler(int drv_type, int cmd, int param)
{
	int ret;
	struct conn_debug_cmd debug_cmd;

	debug_cmd.drv = drv_type;
	debug_cmd.cmd = cmd;
	debug_cmd.param = param;

	ret = conap_scp_send_message(DRV_TYPE_CONN, 1,
					(unsigned char *)&debug_cmd, sizeof(debug_cmd));
	if (ret)
		pr_notice("[%s] send msg fail [%d]", __func__, ret);

}

/*********************************************************************/
/* init/deinit */
/*********************************************************************/


int conn_scp_probe(struct platform_device *pdev)
{
	connsys_scp_plt_data_init(pdev);
	return 0;
}

int conn_scp_remove(struct platform_device *pdev)
{
	return 0;
}


int conap_scp_init(void)
{
	int ret, i;
	struct conap_scp_ipi_cb ipi_cb;

	memset(&g_core_ctx, 0, sizeof(struct conap_scp_core_ctx));

	/* platform driver register */
	ret = platform_driver_register(&g_connscp_dev_drv);
	if (ret)
		pr_notice("platform driver registered failed(%d)\n", ret);

	/* init drv_user */
	for (i = 0; i < CONAP_SCP_DRV_NUM; i++) {
		memset(&g_drv_user[i], 0, sizeof(struct conap_scp_drv_user));
		init_completion(&(g_drv_user[i].is_rdy_comp));
	}

	/* tx msg thread */
	ret = msg_thread_init(&g_core_ctx.tx_msg_thread, "conap_scp_core",
					conap_scp_core_opfunc, CONAP_SCP_OPID_MAX);
	if (ret) {
		pr_warn("msg thread init fail [%d]", ret);
		return -1;
	}

	mutex_init(&g_core_ctx.lock);

	/* rx thread */
	init_completion(&g_core_ctx.msg_send_comp);
	init_completion(&g_core_ctx.msg_recv_comp);

	/* init ipi */
	ipi_cb.conap_scp_ipi_msg_notify = conap_scp_msg_notify;
	ipi_cb.conap_scp_ipi_ctrl_notify = conap_scp_ipi_ctrl_notify;
	ret = conap_scp_ipi_init(&ipi_cb);
	if (ret) {
		pr_warn("scp ipi init fail [%d]", ret);
		return -1;
	}

	connectivity_register_state_notifier(&g_conn_state_notifier);
	connectivity_register_cmd_handler(conap_scp_cmd_handler);

	pr_info("[%s] DONE", __func__);
	return 0;
}

int conap_scp_deinit(void)
{
	connectivity_unregister_cmd_handler();
	connectivity_unregister_state_notifier(&g_conn_state_notifier);
	conap_scp_ipi_deinit();
	msg_thread_deinit(&g_core_ctx.tx_msg_thread);
	mutex_destroy(&g_core_ctx.lock);

	platform_driver_unregister(&g_connscp_dev_drv);
	return 0;
}

