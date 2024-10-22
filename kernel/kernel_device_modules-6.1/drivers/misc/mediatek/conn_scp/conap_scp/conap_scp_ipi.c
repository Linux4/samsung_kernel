// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/notifier.h>

#include "conap_scp.h"
#include "conap_scp_priv.h"
#include "conap_scp_ipi.h"
#include "conap_platform_data.h"

#if IS_ENABLED(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
#define MTK_CONAP_IPI_SUPPORT 1
#endif

#ifdef MTK_CONAP_IPI_SUPPORT
/* SCP */
#include "scp.h"
#endif

/* -------------- IPI -------------- */
#define MAX_IPI_SEND_DATA_SZ 56
struct ipi_send_data {
	uint16_t drv_type;
	uint16_t msg_id;
	uint16_t total_sz;
	uint16_t this_sz;
	char data[MAX_IPI_SEND_DATA_SZ];
};

static struct ipi_send_data g_send_data;

struct conap_scp_ipi_cb g_ipi_cb;

#ifdef MTK_CONAP_IPI_SUPPORT
static char g_ipi_ack_data[128];
static int scp_ctrl_event_handler(struct notifier_block *this,
	unsigned long event, void *ptr);

static struct notifier_block scp_ctrl_notifier = {
	.notifier_call = scp_ctrl_event_handler,
};

/* -------------- MSG Definition-------------- */

struct msg_cmd {
	uint16_t drv_type;
	uint16_t msg_id;
	uint16_t total_sz;
	uint16_t this_sz;
	uint32_t param0;
	uint32_t param1;
	uint32_t param2;
};

struct shm_msg_cmd {
	uint16_t drv_type;
	uint16_t msg_id;
	uint32_t param0;
	uint32_t param1;
	uint32_t param2;
	uint32_t param3;
};

/****** SHM Definition ********/
/*   SHM Layout           */
/*  +-------------------+ */
/*  |  AP2SCP           | */
/*  +-------------------+ */
/*  |  SCP2AP           | */
/*  +-------------------+ */
/******************************/
#define CONAP_SHM_SUPPORT		1
#define CONAP_SHM_MAX_PKT_SZ		(3*1024)

#ifdef CONAP_SHM_SUPPORT
struct conap_buf {
	phys_addr_t buf;
	phys_addr_t size;
	phys_addr_t cur_ptr;
};

struct conap_shm_ctx {
	bool enable;
	phys_addr_t shm_addr;
	phys_addr_t shm_size;

	struct conap_buf tx_buf;
	struct conap_buf rx_buf;
};

static struct conap_shm_ctx g_conap_shm_ctx;
#endif

int scp_ctrl_event_handler(struct notifier_block *this,
	unsigned long event, void *ptr)
{

	switch (event) {
	case SCP_EVENT_STOP:
		pr_info("[%s] SCP STOP", __func__);
		if (g_ipi_cb.conap_scp_ipi_ctrl_notify)
			(*g_ipi_cb.conap_scp_ipi_ctrl_notify)(0);
		break;
	case SCP_EVENT_READY:
		pr_info("[%s] SCP READY", __func__);
		if (g_ipi_cb.conap_scp_ipi_ctrl_notify)
			(*g_ipi_cb.conap_scp_ipi_ctrl_notify)(1);
		break;
	default:
		pr_info("scp notify event error %lu", event);
	}
	return 0;
}
#endif

unsigned int conap_scp_ipi_is_scp_ready(void)
{
#ifdef MTK_CONAP_IPI_SUPPORT
	return is_scp_ready(SCP_A_ID);
#else
	return 0;
#endif
}


unsigned int conap_scp_ipi_msg_sz(void)
{
	return (connsys_scp_ipi_mbox_size() - (sizeof(uint16_t) * 4));
}

int ipi_recv_cb(unsigned int id, void *prdata, void *data, unsigned int len)
{
	struct ipi_send_data *msg;
	struct timespec64 t1, t2;

	ktime_get_real_ts64(&t1);
	msg = (struct ipi_send_data *)data;

	if (g_ipi_cb.conap_scp_ipi_msg_notify)
		(*g_ipi_cb.conap_scp_ipi_msg_notify)(msg->drv_type, msg->msg_id,
				msg->total_sz, &(msg->data[0]), msg->this_sz);

	ktime_get_real_ts64(&t2);
	if ((t2.tv_nsec - t1.tv_nsec) > 3000000)
		pr_info("[%s] ===[%09ld]", __func__, (t2.tv_nsec - t1.tv_nsec));
	return 0;
}

static int conap_scp_ipi_send(void *data, uint32_t len)
{
#ifdef MTK_CONAP_IPI_SUPPORT
	unsigned int retry_time = 500;
	unsigned int retry_cnt = 0;
	int ipi_result = -1;

	for (retry_cnt = 0; retry_cnt <= retry_time; retry_cnt++) {
		ipi_result = mtk_ipi_send(&scp_ipidev,
							IPI_OUT_SCP_CONNSYS,
							0,
							data,
							len/4,
							0);
		if (ipi_result == IPI_ACTION_DONE)
			break;
		udelay(1000);
	}
	if (ipi_result != 0) {
		pr_err("[ipi_send_data] send fail [%d]", ipi_result);
		return -1;
	}
#else
	pr_notice("[%s] mtk_ipi_send is not support", __func__);
#endif
	return 0;
}


int conap_scp_ipi_send_data(enum conap_scp_drv_type drv_type, uint16_t msg_id, uint32_t total_sz,
						uint8_t *buf, uint32_t size)
{
	int r;

	if (size + (sizeof(uint16_t)*2) > connsys_scp_ipi_mbox_size())
		return -99;

	g_send_data.drv_type = drv_type;
	g_send_data.msg_id = msg_id;
	g_send_data.total_sz = total_sz;
	g_send_data.this_sz = size;

	if (buf != NULL && size > 0)
		memcpy(&(g_send_data.data[0]), buf, size);

	r = conap_scp_ipi_send(&g_send_data,
							connsys_scp_ipi_mbox_size());
	if (r)
		return r;
	return size;
}

int conap_scp_ipi_send_cmd(enum conap_scp_drv_type drv_type, uint16_t msg_id,
					uint32_t p0, uint32_t p1, uint32_t p2)
{
	struct msg_cmd cmd;
	int r;

	cmd.drv_type = drv_type;
	cmd.msg_id = msg_id;
	cmd.total_sz = 8;
	cmd.this_sz = 8;
	cmd.param0 = p0;
	cmd.param1 = p1;
	cmd.param2 = p2;

	r = conap_scp_ipi_send(&cmd,
							sizeof(struct msg_cmd));
	if (r)
		return r;

	return 0;
}

int conap_scp_ipi_send_shm_cmd(enum conap_scp_drv_type drv_type, uint16_t msg_id,
					uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3)
{
	struct shm_msg_cmd cmd;
	int r;

	cmd.drv_type = drv_type;
	cmd.msg_id = msg_id;
	cmd.param0 = p0;
	cmd.param1 = p1;
	cmd.param2 = p2;
	cmd.param3 = p3;

	r = conap_scp_ipi_send(&cmd,
							sizeof(struct shm_msg_cmd));
	if (r)
		return r;

	return 0;
}


int conap_scp_shm_write(uint8_t *msg_buf, uint32_t msg_sz)
{
	struct conap_shm_ctx *ctx = &g_conap_shm_ctx;
	uint32_t buf_len = ctx->tx_buf.size;
	phys_addr_t wbf_addr = ctx->tx_buf.buf;
	phys_addr_t wptr = ctx->tx_buf.cur_ptr;
	uint32_t cpsz1, cpsz2, ret;

	if (msg_sz == 0)
		return 0;

	if (ctx->enable == false)
		return 0;

	ret = (uint32_t)wptr;

	if (wptr + msg_sz > buf_len) {
		cpsz1 = buf_len - wptr;
		cpsz2 = msg_sz - cpsz1;
		memcpy((uint8_t *)(wbf_addr + wptr), (uint8_t *)msg_buf, cpsz1);
		wptr = 0;
		memcpy((uint8_t *)wbf_addr, msg_buf + cpsz1, cpsz2);
		wptr = cpsz2;
	} else {
		memcpy((uint8_t *)(wbf_addr + wptr), msg_buf, msg_sz);
		wptr += msg_sz;
	}

	ctx->tx_buf.cur_ptr = wptr;

	return ret;
}

int conap_scp_shm_read(uint8_t *msg_buf, uint32_t oft, uint32_t msg_sz)
{
	struct conap_shm_ctx *ctx = &g_conap_shm_ctx;
	uint32_t buf_len = ctx->rx_buf.size;
	phys_addr_t rbf_addr = ctx->rx_buf.buf;
	phys_addr_t rptr = oft;
	uint32_t cpsz1, cpsz2;

	if (msg_sz >= CONAP_SHM_MAX_PKT_SZ)
		return -1;

	if (oft > buf_len)
		return -1;

	if (rptr + msg_sz > buf_len) {
		cpsz1 = buf_len - rptr;
		cpsz2 = msg_sz - cpsz1;
		memcpy(msg_buf, (uint8_t *)(rbf_addr + rptr), cpsz1);
		memcpy(msg_buf + cpsz1, (uint8_t *)rbf_addr, cpsz2);
		rptr = cpsz2;
	} else {
		memcpy(msg_buf, (uint8_t *)(rbf_addr + rptr), msg_sz);
		rptr += msg_sz;
	}

	ctx->rx_buf.cur_ptr = rptr;

	return 0;
}


bool conap_scp_ipi_shm_support(void)
{
	return g_conap_shm_ctx.enable;
}

static int conap_scp_shm_init(void)
{
	phys_addr_t mem_addr = 0;
	phys_addr_t mem_size = 0;

	memset(&g_conap_shm_ctx, 0, sizeof(g_conap_shm_ctx));

	mem_addr = scp_get_reserve_mem_virt(SCP_CONNSYS_MEM_ID);
	mem_size = scp_get_reserve_mem_size(SCP_CONNSYS_MEM_ID);

	if (mem_addr == 0 || mem_size == 0) {
		pr_notice("[%s] incoorect shm setting [%llX][%llX]", __func__, mem_addr, mem_size);
		return -1;
	}

	g_conap_shm_ctx.shm_addr = mem_addr;
	g_conap_shm_ctx.shm_size = mem_size;

	g_conap_shm_ctx.tx_buf.buf = mem_addr;
	g_conap_shm_ctx.tx_buf.size = mem_size/2;

	g_conap_shm_ctx.rx_buf.buf = mem_addr + (mem_size/2);
	g_conap_shm_ctx.rx_buf.size = mem_size/2;

	g_conap_shm_ctx.enable = true;


	pr_info("[%s] mem=[%llx][%llx] [%llx][%llx]", __func__,
				mem_addr, mem_size,
				g_conap_shm_ctx.tx_buf.buf, g_conap_shm_ctx.rx_buf.buf);

	return 0;
}


int conap_scp_ipi_init(struct conap_scp_ipi_cb *cb)
{
#ifdef MTK_CONAP_IPI_SUPPORT
	int ret = IPI_ACTION_DONE;
#endif

	memcpy(&g_ipi_cb, cb, sizeof(struct conap_scp_ipi_cb));

	conap_scp_shm_init();

#ifdef MTK_CONAP_IPI_SUPPORT
	ret = mtk_ipi_register(&scp_ipidev, IPI_IN_SCP_CONNSYS,
					(void *) ipi_recv_cb, NULL, &g_ipi_ack_data[0]);
	if (ret != IPI_ACTION_DONE) {
		pr_info("[%s] ipi_register ret=[%d]", __func__, ret);
		return -1;
	}

	scp_A_register_notify(&scp_ctrl_notifier);
#else
	pr_notice("[%s] mtk_ipi_send is not support", __func__);
#endif

	return 0;
}

int conap_scp_ipi_deinit(void)
{
	int ret = 0;

#ifdef MTK_CONAP_IPI_SUPPORT
	ret = mtk_ipi_unregister(&scp_ipidev, IPI_IN_SCP_CONNSYS);
	pr_info("[%s] ipi_unregister ret=[%d]", __func__, ret);
#else
	pr_notice("[%s] mtk_ipi_send is not support", __func__);
#endif
	return ret;
}
