/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Samsung Exynos SoC series Pablo IS driver
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-icpu-adapter.h"
#include "pablo-sensor-adapter.h"
#include "is-config.h"
#include "pablo-icpu-itf.h"
#include "pablo-crta-bufmgr.h"
#include "pablo-debug.h"
#include "is-common-config.h"
#include "is-core.h"

#define MBOX_HIC_PARAM			16
#define MBOX_IHC_PARAM			8

#define SET_MSG_VALUE(val, mask, shift)	((val << shift) & mask)
#define GET_MSG_VALUE(msg, mask, shift)	((msg & mask) >> shift)

#define GET_DVA(dva_high, dva_low)	((((u64)dva_high << DVA_HIGH_SHIFT) & DVA_HIGH_MASK) | \
					 (((u64)dva_low << DVA_LOW_MASK) & DVA_LOW_SHIFT))

#define GET_DVA_HIGH(dva)		((dva & DVA_HIGH_MASK) >> DVA_HIGH_SHIFT)
#define GET_DVA_LOW(dva)		((dva & DVA_LOW_MASK) >> DVA_LOW_SHIFT)

#define TIMEOUT_MSG			(HZ / 4)
#define TIMEOUT_ICPU			1000

#define CHK_CMD_FCOUNT(cmd)			\
	(((cmd == PABLO_HIC_SHOT)		\
	|| (cmd == PABLO_HIC_CSTAT_FRAME_START)	\
	|| (cmd == PABLO_HIC_CSTAT_CDAF_END)	\
	|| (cmd == PABLO_HIC_CSTAT_FRAME_END)	\
	|| (cmd == PABLO_HIC_PDP_STAT0_END)	\
	|| (cmd == PABLO_HIC_PDP_STAT1_END)) ? 1 : 0)

#define ITF_API(i, op, args...) (((i) && (i)->op) ? i->op(args) : 0)

struct cmd_info {
	char *str;
	u32 prio;
	u32 data_len;
};

struct block_msg_wait_q {
	u32			instance;
	u32			cmd;
	bool			block;
	refcount_t		state;
	wait_queue_head_t	head;
};

struct cb_handler {
	pablo_response_msg_cb			cb[IS_STREAM_COUNT];
	struct block_msg_wait_q			wait_q[IS_STREAM_COUNT];
};

struct icpu_adt_v2 {
	struct cb_handler			cb_handler[PABLO_HIC_MAX];
	struct pablo_sensor_adt			*sensor_adt[IS_STREAM_COUNT];
	struct pablo_crta_bufmgr		*bufmgr[IS_STREAM_COUNT];
};

static struct cmd_info hic_cmd_info[PABLO_HIC_MAX] = {
	{ "PABLO_HIC_OPEN", ICPU_MSG_PRIO_NORMAL, 5, },
	{ "PABLO_HIC_PUT_BUF", ICPU_MSG_PRIO_NORMAL, 5, },
	{ "PABLO_HIC_SET_SHARED_BUF_IDX", ICPU_MSG_PRIO_NORMAL, 3, },
	{ "PABLO_HIC_START", ICPU_MSG_PRIO_NORMAL, 3, },
	{ "PABLO_HIC_SHOT", ICPU_MSG_PRIO_NORMAL, 6, },
	{ "PABLO_HIC_CSTAT_FRAME_START", ICPU_MSG_PRIO_NORMAL, 4, },
	{ "PABLO_HIC_CSTAT_CDAF_END", ICPU_MSG_PRIO_NORMAL, 0, },
	{ "PABLO_HIC_PDP_STAT0_END", ICPU_MSG_PRIO_NORMAL, 0, },
	{ "PABLO_HIC_PDP_STAT1_END", ICPU_MSG_PRIO_NORMAL, 0, },
	{ "PABLO_HIC_CSTAT_FRAME_END", ICPU_MSG_PRIO_NORMAL, 0, },
	{ "PABLO_HIC_STOP", ICPU_MSG_PRIO_NORMAL, 2, },
	{ "PABLO_HIC_CLOSE", ICPU_MSG_PRIO_NORMAL, 2, },
};

static struct cmd_info ihc_cmd_info[PABLO_IHC_MAX] = {
	{ "PABLO_HIC_CONTROL_SENSOR", ICPU_MSG_PRIO_NORMAL, 1, },
};

const static char *srta_msg_type[PABLO_MESSAGE_MAX] = {
	"CMD",
	"RES"
};

const static char *srta_rsp[PABLO_RSP_MAX] = {
	"SUCESS",
	"FAIL"
};

const static char *buf_name[PABLO_BUFFER_MAX] = {
	"RTA",
	"SS_CTRL",
	"STATIC",
};

static struct pablo_icpu_adt			*__icpu_adt;
static struct pablo_icpu_itf_api		*__itf;

static inline int __pabli_icpu_adt_is_open(struct pablo_icpu_adt *icpu_adt, u32 instance)
{
	if (!atomic_read(&icpu_adt->rsccount)) {
		merr_adt("icpu_adt is not open", instance);
		return -ENODEV;
	}

	return 0;
}

static inline u32 __pablo_icpu_adt_get_msg(u32 instance, enum pablo_message_type type,
	enum pablo_hic_cmd_id cmd, enum pablo_response_id res)
{
	u32 msg = 0;

	msg |= SET_MSG_VALUE(instance, MSG_INSTANCE_ID_MASK, MSG_INSTANCE_ID_SHIFT);
	msg |= SET_MSG_VALUE(type, MSG_TYPE_MASK, MSG_TYPE_SHIFT);
	msg |= SET_MSG_VALUE(cmd, MSG_COMMAND_ID_MASK, MSG_COMMAND_ID_SHIFT);
	msg |= SET_MSG_VALUE(res, MSG_RESPONSE_ID_MASK, MSG_RESPONSE_ID_SHIFT);

	return msg;
}

static inline void __pablo_icpu_adt_set_dva(u32 *high, u32 *low, dma_addr_t dva)
{
	*high = GET_DVA_HIGH(dva);
	*low = GET_DVA_LOW(dva);
}

/* handle rsp msg from crta */

static void __pablo_icpu_adt_wake_up_wait_q(struct block_msg_wait_q *wait_q)
{
	refcount_set(&wait_q->state, 1);
	wake_up(&wait_q->head);
}

static void __pablo_icpu_adt_handle_response_msg(void *sender, void *cookie, u32 *data)
{
	int ret;
	struct pablo_icpu_adt_rsp_msg rsp_msg;
	u32 instance, msg_type, cmd, rsp;
	struct icpu_adt_v2 *adt;
	struct cb_handler *cb_handler;

	if (!data) {
		merr_adt("invalid data", 0);
		return;
	}

	instance = GET_MSG_VALUE(data[0], MSG_INSTANCE_ID_MASK, MSG_INSTANCE_ID_SHIFT);
	msg_type = GET_MSG_VALUE(data[0], MSG_TYPE_MASK, MSG_TYPE_SHIFT);
	cmd = GET_MSG_VALUE(data[0], MSG_COMMAND_ID_MASK, MSG_COMMAND_ID_SHIFT);
	rsp = GET_MSG_VALUE(data[0], MSG_RESPONSE_ID_MASK, MSG_RESPONSE_ID_SHIFT);

	if (instance >= IS_STREAM_COUNT) {
		merr_adt("invalid instance(%d)", instance, instance);
		return;
	}

	if (cmd >= PABLO_HIC_MAX) {
		merr_adt("unknown response msg:cmd(%d)", instance, cmd);
		return;
	}

	if (!__icpu_adt || !atomic_read(&__icpu_adt->rsccount)) {
		merr_adt("icpu_adt is not open", instance);
		return;
	}

	mdbg_adt(5, "%s: %s", instance, __func__, hic_cmd_info[cmd].str);

	adt = (struct icpu_adt_v2 *)__icpu_adt->priv;
	cb_handler = &adt->cb_handler[cmd];

	if (!cb_handler->cb[instance])
		goto wake_up;

	mdbg_adt(2, "%s:call callback\n", instance, __func__);

	/* change sign of rsp
	 * error value of crta : positive -> error value of adapter : negative
	 */
	rsp_msg.rsp = -(int)rsp;
	rsp_msg.instance = instance;
	if (CHK_CMD_FCOUNT(cmd))
		rsp_msg.fcount = data[1];

	/* call callback */
	ret = cb_handler->cb[instance](sender, cookie, &rsp_msg);
	if (ret)
		merr_adt("callback err(%d)", instance, ret);

wake_up:
	if (cb_handler->wait_q[instance].block)
		__pablo_icpu_adt_wake_up_wait_q(&cb_handler->wait_q[instance]);
}

/* handle cmd msg from crta */

static void __pablo_icpu_adt_handle_msg_control_sensor(struct icpu_adt_v2 *adt, u32 instance, u32 *param)
{
	int ret;
	dma_addr_t dva;
	u32 buf_idx;
	struct pablo_crta_buf_info buf_info = { 0, };
	struct is_priv_buf *pb;

	/* get buf_info */
	buf_idx = GET_MSG_VALUE(param[0], BUF_IDX_MASK, BUF_IDX_SHIFT);
	CALL_CRTA_BUFMGR_OPS(adt->bufmgr[instance], get_process_buf, PABLO_CRTA_BUF_SS_CTRL,
		buf_idx, &buf_info);

	/* compare dva */
	dva = GET_DVA(param[1], param[2]);
	if (buf_info.dva != dva) {
		merr_adt("control_sensor: invalid dva: idx(%d), dva(0x%llx!=0x%llx)", instance,
			buf_idx, buf_info.dva, dva);
		return;
	}

	if (!IS_ENABLED(ICPU_IO_COHERENCY)) {
		if (buf_info.frame) {
			pb = buf_info.frame->pb_output;
			CALL_BUFOP(pb, sync_for_cpu, pb, 0, pb->size, DMA_FROM_DEVICE);
		}
	}

	/* control sensor */
	ret = CALL_SS_ADT_OPS(adt->sensor_adt[instance], control_sensor,
				buf_info.kva, buf_info.dva, buf_info.id);
	if (ret)
		merr_adt("failed to sensor_adt control_sensor", instance);
}

static void __pablo_icpu_adt_receive_msg(void *cookie, u32 *data)
{
	u32 msg;
	u32 instance, msg_type, cmd, rsp;
	struct pablo_icpu_adt *icpu_adt;
	struct icpu_adt_v2 *adt;

	icpu_adt = cookie;
	if (!icpu_adt || !data) {
		err("icpu_adt is null");
		return;
	}

	if (__pabli_icpu_adt_is_open(icpu_adt, 0))
		return;

	adt = (struct icpu_adt_v2 *)icpu_adt->priv;

	msg = data[0];
	instance = GET_MSG_VALUE(msg, MSG_INSTANCE_ID_MASK, MSG_INSTANCE_ID_SHIFT);
	msg_type = GET_MSG_VALUE(msg, MSG_TYPE_MASK, MSG_TYPE_SHIFT);
	cmd = GET_MSG_VALUE(msg, MSG_COMMAND_ID_MASK, MSG_COMMAND_ID_SHIFT);
	rsp = GET_MSG_VALUE(msg, MSG_RESPONSE_ID_MASK, MSG_RESPONSE_ID_SHIFT);

	if (msg_type != PABLO_MESSAGE_COMMAND) {
		merr_adt("unknown msg type(%d)", instance, msg_type);
		return;
	}

	if (cmd >= PABLO_IHC_MAX) {
		merr_adt("unknown cmd(%d)", instance, cmd);
		return;
	}

	mdbg_adt(1, "%s:type(%s) cmd(%s)", instance, __func__,
		srta_msg_type[msg_type], ihc_cmd_info[cmd].str);

	switch (cmd) {
	case PABLO_IHC_CONTROL_SENSOR:
		__pablo_icpu_adt_handle_msg_control_sensor(adt, instance, &data[1]);
		break;
	};

	return;
}

static void __pablo_icpu_adt_fw_err_handler(void *cookie, u32 *data)
{
	int i;
	struct is_core *core = is_get_is_core();
	bool en_s2d = true; /* TODO: get value from data */

	if (is_get_debug_param(IS_DEBUG_PARAM_ASSERT_CRASH)) {
		is_debug_icpu_s2d_handler();
		for (i = 0; i < IS_SENSOR_COUNT; i++)
			set_bit(IS_SENSOR_ASSERT_CRASH, &core->sensor[i].state);
	} else {
		is_debug_s2d(en_s2d, "DDK/RTA ASSERT");
	}
}

/* send msg to crta  */

static inline int __send_message(struct cmd_info *info, u32 *data)
{
	return ITF_API(__itf, send_message, NULL, NULL, NULL,
			info->prio, info->data_len, data);
}

static inline int __send_blocking_message(struct cmd_info *info, u32 *data, void *sender, void *cookie,
	struct block_msg_wait_q *wait_q)
{
	int ret = 0;
	long wait;

	wait_q->block = true;
	refcount_set(&wait_q->state, 0);

	ret = ITF_API(__itf, send_message, sender, cookie, __pablo_icpu_adt_handle_response_msg,
			info->prio, info->data_len, data);

	if (!ret) {
		wait = wait_event_timeout(wait_q->head,
			refcount_read(&wait_q->state), TIMEOUT_MSG);

		if (wait <= 0) {
			merr_adt("wait msg(%s) timeout (%ld/%u)", wait_q->instance,
				hic_cmd_info[wait_q->cmd].str, wait, refcount_read(&wait_q->state));
			ret = -ETIMEDOUT;
		} else {
			mdbg_adt(1, "waiting msg(%s) done:%ld", wait_q->instance,
				hic_cmd_info[wait_q->cmd].str, wait);
		}
	}

	return ret;
}

static inline int __send_nonblocking_message(struct cmd_info *info, u32 *data, void *sender, void *cookie,
	struct block_msg_wait_q *wait_q)
{
	wait_q->block = false;
	return ITF_API(__itf, send_message, sender, cookie, __pablo_icpu_adt_handle_response_msg,
			info->prio, info->data_len, data);
}

/* send rsp msg to crta */

static int __pablo_icpu_adt_send_msg_response(struct pablo_icpu_adt *icpu_adt, u32 instance,
	u32 cmd, u32 rsp, u32 *param)
{
	int ret;
	u32 i, data[MBOX_HIC_PARAM] = { 0, };
	struct icpu_adt_v2 *adt;

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	mdbg_adt(1, "%s:cmd(%s) rsp(%s)", instance, __func__, ihc_cmd_info[cmd].str, srta_rsp[rsp]);

	adt = (struct icpu_adt_v2 *)icpu_adt->priv;

	/* prepare data */
	data[0] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_RESPONSE, cmd, rsp);
	for (i = 0; i < ihc_cmd_info[cmd].data_len; i++)
		data[i + 1] = param[i];

	/* send msg */
	ret = __send_message(&ihc_cmd_info[cmd], data);

	return ret;
}

static void __pablo_icpu_adt_control_sensor_callback(void *caller, u32 instance, u32 ret, dma_addr_t dva, u32 idx)
{
	u32 rsp, cmd = PABLO_IHC_CONTROL_SENSOR;
	u32 param[MBOX_HIC_PARAM - 1] = { 0, };
	struct pablo_icpu_adt *icpu_adt = (struct pablo_icpu_adt *)caller;

	if (!icpu_adt || !atomic_read(&icpu_adt->rsccount)) {
		merr_adt("icpu_adt is not open", instance);
		return;
	}

	param[0] = idx;
	__pablo_icpu_adt_set_dva(&param[1], &param[2], dva);

	if (ret)
		rsp = PABLO_RSP_FAIL;
	else
		rsp = PABLO_RSP_SUCCESS;

	__pablo_icpu_adt_send_msg_response(icpu_adt, instance, cmd, rsp, param);
}

/* send cmd msg to crta */

static int pablo_icpu_adt_register_response_msg_cb(struct pablo_icpu_adt *icpu_adt, u32 instance,
	enum pablo_hic_cmd_id msg, pablo_response_msg_cb cb)
{
	int ret;
	struct icpu_adt_v2 *adt;

	mdbg_adt(1, "%s\n", instance, __func__);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	if (instance >= IS_STREAM_COUNT) {
		err("invaid instance(%d)", instance);
		return -EINVAL;
	}

	if (msg >= PABLO_HIC_MAX) {
		err("invaid msg(%d)", msg);
		return -EINVAL;
	}

	adt = icpu_adt->priv;

	mdbg_adt(1, "register callback: msg(%s), cb(%p -> %p)\n", instance,
		hic_cmd_info[msg].str, adt->cb_handler[msg].cb[instance], cb);

	adt->cb_handler[msg].cb[instance] = cb;

	return 0;
}

static int pablo_icpu_adt_send_msg_open(struct pablo_icpu_adt *icpu_adt, u32 instance,
					u32 hw_id, u32 rep_flag, u32 position, u32 f_type)
{
	int ret;
	u32 data[MBOX_HIC_PARAM] = { 0, };
	struct icpu_adt_v2 *adt;
	u32 cmd = PABLO_HIC_OPEN;

	mdbg_adt(1, "send_msg_open: hw_id(%d), rep(%d), position(%d), f_type(%d)", instance,
		hw_id, rep_flag, position, f_type);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	adt = icpu_adt->priv;

	/* prepare data */
	data[0] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_COMMAND, cmd, PABLO_RSP_SUCCESS);
	data[1] = hw_id;
	data[2] = rep_flag;
	data[3] = position;
	data[4] = f_type;

	/* send msg */
	ret = __send_blocking_message(&hic_cmd_info[cmd], data, icpu_adt, NULL,
		&adt->cb_handler[cmd].wait_q[instance]);

	return ret;
}

static int pablo_icpu_adt_send_msg_put_buf(struct pablo_icpu_adt *icpu_adt, u32 instance,
						enum pablo_buffer_type type,
						struct pablo_crta_buf_info *buf)
{
	int ret;
	u32 data[MBOX_HIC_PARAM] = { 0, };
	struct icpu_adt_v2 *adt;
	u32 cmd = PABLO_HIC_PUT_BUF;

	if (type >= PABLO_BUFFER_MAX) {
		merr_adt("invalid buf type(%d)", instance, type);
		return -EINVAL;
	}

	minfo_adt("send_msg_put_buf: type(%s), idx(%d), dva(%pad), kva(0x%p), size(0x%zx)", instance,
		buf_name[type], buf->id, &buf->dva, buf->kva, buf->size);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	adt = icpu_adt->priv;

	/* prepare data */
	data[0] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_COMMAND,
				       cmd, PABLO_RSP_SUCCESS);
	data[1] = SET_MSG_VALUE(buf->id, BUF_IDX_MASK, BUF_IDX_SHIFT) |
		  SET_MSG_VALUE(type, BUF_TYPE_MASK, BUF_TYPE_SHIFT);
	__pablo_icpu_adt_set_dva(&data[2], &data[3], buf->dva);
	data[4] = (u32)buf->size;

	/* send msg */
	ret = __send_nonblocking_message(&hic_cmd_info[cmd], data, icpu_adt, NULL,
		&adt->cb_handler[cmd].wait_q[instance]);

	return ret;

}

static int pablo_icpu_adt_send_msg_set_shared_buf_idx(struct pablo_icpu_adt *icpu_adt, u32 instance,
						enum pablo_buffer_type type,
						u32 fcount)
{
	int ret;
	u32 data[MBOX_HIC_PARAM] = { 0, };
	struct icpu_adt_v2 *adt;
	u32 cmd = PABLO_HIC_SET_SHARED_BUF_IDX;

	if (type >= PABLO_BUFFER_MAX) {
		merr_adt("invalid buf type(%d)", instance, type);
		return -EINVAL;
	}

	mdbg_adt(1, "[F%d]send_msg_set_shared_buf_idx: type(%s)", instance, fcount, buf_name[type]);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	adt = icpu_adt->priv;

	/* prepare data */
	data[0] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_COMMAND,
				       cmd, PABLO_RSP_SUCCESS);
	data[1] = type;
	data[2] = fcount;

	/* send msg */
	ret = __send_message(&hic_cmd_info[cmd], data);

	return ret;

}

static int pablo_icpu_adt_send_msg_start(struct pablo_icpu_adt *icpu_adt, u32 instance,
					struct pablo_crta_buf_info *pcsi_buf)
{
	int ret;
	u32 data[MBOX_HIC_PARAM] = { 0, };
	struct icpu_adt_v2 *adt;
	u32 cmd = PABLO_HIC_START;
	struct is_priv_buf *pb;

	mdbg_adt(1, "send_msg_start: dva(0x%llx) kva(%p)", instance,
		pcsi_buf->dva, pcsi_buf->kva);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	adt = icpu_adt->priv;

	CALL_SS_ADT_OPS(adt->sensor_adt[instance], start);

	/* prepare data */
	CALL_SS_ADT_OPS(adt->sensor_adt[instance], update_actuator_info, true);
	ret = CALL_SS_ADT_OPS(adt->sensor_adt[instance], get_sensor_info, pcsi_buf->kva);
	if (ret)
		merr_adt("failed to sensor_adt get_sensor_info", instance);

	if (!IS_ENABLED(ICPU_IO_COHERENCY)) {
		if (pcsi_buf->frame) {
			pb = pcsi_buf->frame->pb_output;
			CALL_BUFOP(pb, sync_for_device, pb, 0, pb->size, DMA_TO_DEVICE);
		}
	}

	data[0] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_COMMAND,
				       cmd, PABLO_RSP_SUCCESS);
	__pablo_icpu_adt_set_dva(&data[1], &data[2], pcsi_buf->dva);

	/* send msg */
	ret = __send_blocking_message(&hic_cmd_info[cmd], data, icpu_adt, NULL,
		&adt->cb_handler[cmd].wait_q[instance]);

	return ret;
}

static int pablo_icpu_adt_send_msg_shot(struct pablo_icpu_adt *icpu_adt, u32 instance,
	void *cb_caller, void *cb_ctx,
	u32 fcount, struct pablo_crta_buf_info *shot_buf, struct pablo_crta_buf_info *pcfi_buf)
{
	int ret;
	u32 data[MBOX_HIC_PARAM] = { 0, };
	struct icpu_adt_v2 *adt;
	u32 cmd = PABLO_HIC_SHOT;
	struct is_priv_buf *pb;

	mdbg_adt(1, "[F%d]send_msg_shot: shot(0x%llx) pcfi(0x%llx)", instance,
		 fcount, shot_buf->dva, pcfi_buf->dva);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	adt = icpu_adt->priv;

	if (!IS_ENABLED(ICPU_IO_COHERENCY)) {
		if (pcfi_buf->frame) {
			pb = pcfi_buf->frame->pb_output;
			CALL_BUFOP(pb, sync_for_device, pb, 0, pb->size, DMA_TO_DEVICE);
		}
	}

	/* prepare data */
	data[0] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_COMMAND,
				       cmd, PABLO_RSP_SUCCESS);
	data[1] = fcount;
	__pablo_icpu_adt_set_dva(&data[2], &data[3], shot_buf->dva);
	__pablo_icpu_adt_set_dva(&data[4], &data[5], pcfi_buf->dva);

	/* send msg */
	if (in_interrupt())
		ret = __send_nonblocking_message(&hic_cmd_info[cmd], data, cb_caller, cb_ctx,
			&adt->cb_handler[cmd].wait_q[instance]);
	else
		ret = __send_blocking_message(&hic_cmd_info[cmd], data, cb_caller, cb_ctx,
			&adt->cb_handler[cmd].wait_q[instance]);

	return ret;
}

static int pablo_icpu_adt_send_msg_cstat_frame_start(struct pablo_icpu_adt *icpu_adt, u32 instance,
	void *cb_caller, void *cb_ctx,
	u32 fcount, struct pablo_crta_buf_info *pcsi_buf)
{
	int ret;
	u32 data[MBOX_HIC_PARAM] = { 0, };
	struct icpu_adt_v2 *adt;
	u32 cmd = PABLO_HIC_CSTAT_FRAME_START;
	struct is_priv_buf *pb;

	mdbg_adt(1, "[F%d]send_msg_cstat_frame_start: dva_pcsi(0x%llx)", instance,
		fcount, pcsi_buf->dva);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	adt = icpu_adt->priv;

	/* prepare data */
	ret = CALL_SS_ADT_OPS(adt->sensor_adt[instance], get_sensor_info, pcsi_buf->kva);
	if (ret)
		merr_adt("failed to sensor_adt get_sensor_info", instance);

	if (!IS_ENABLED(ICPU_IO_COHERENCY)) {
		if (pcsi_buf->frame) {
			pb = pcsi_buf->frame->pb_output;
			CALL_BUFOP(pb, sync_for_device, pb, 0, pb->size, DMA_TO_DEVICE);
		}
	}

	data[0] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_COMMAND,
				       cmd, PABLO_RSP_SUCCESS);
	data[1] = fcount;
	__pablo_icpu_adt_set_dva(&data[2], &data[3], pcsi_buf->dva);

	/* send msg */
	ret = __send_nonblocking_message(&hic_cmd_info[cmd], data, cb_caller, cb_ctx,
		&adt->cb_handler[cmd].wait_q[instance]);

	return ret;
}

static int pablo_icpu_adt_send_msg_cstat_cdaf_end(struct pablo_icpu_adt *icpu_adt, u32 instance,
	void *cb_caller, void *cb_ctx, u32 fcount, u32 stat_num, struct pablo_crta_buf_info *stat_buf)
{
	int ret;
	u32 data[MBOX_HIC_PARAM] = { 0, };
	u32 data_offset;
	struct icpu_adt_v2 *adt;
	u32 cmd = PABLO_HIC_CSTAT_CDAF_END;
	struct is_priv_buf *pb;

	mdbg_adt(1, "[F%d]send_msg_cdaf_end: num(%d) stat(0x%llx)", instance,
		fcount, stat_num, stat_buf->dva);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	adt = icpu_adt->priv;

	if (!IS_ENABLED(ICPU_IO_COHERENCY)) {
		if (stat_buf->frame) {
			pb = stat_buf->frame->pb_output;
			CALL_BUFOP(pb, sync_for_device, pb, 0, pb->size, DMA_TO_DEVICE);
		}
	}

	/* prepare data */
	data_offset = 0;
	/* cmd */
	data[data_offset++] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_COMMAND,
				       cmd, PABLO_RSP_SUCCESS);
	/* fcount */
	data[data_offset++] = fcount;
	/* BUF_CDAF dva */
	__pablo_icpu_adt_set_dva(&data[data_offset], &data[data_offset + 1], stat_buf[PABLO_CRTA_CDAF_RAW].dva);
	data_offset += 2;
	/* BUF_PDAF_TAIL dva */
	__pablo_icpu_adt_set_dva(&data[data_offset], &data[data_offset + 1], stat_buf[PABLO_CRTA_PDAF_TAIL].dva);
	data_offset += 2;
	/* BUF_LASER_AF_DATA dva */
	__pablo_icpu_adt_set_dva(&data[data_offset], &data[data_offset + 1], stat_buf[PABLO_CRTA_LASER_AF_DATA].dva);
	data_offset += 2;

	hic_cmd_info[cmd].data_len = data_offset;

	/* send msg */
	ret = __send_nonblocking_message(&hic_cmd_info[cmd], data, cb_caller, cb_ctx,
		&adt->cb_handler[cmd].wait_q[instance]);

	/* update actuator info for next frame */
	CALL_SS_ADT_OPS(adt->sensor_adt[instance], update_actuator_info, false);

	return ret;
}

static int pablo_icpu_adt_send_msg_pdp_stat0_end(struct pablo_icpu_adt *icpu_adt, u32 instance,
	void *cb_caller, void *cb_ctx, u32 fcount, u32 stat_num, struct pablo_crta_buf_info *stat_buf)
{
	int ret;
	u32 dva_idx, i;
	u32 data[MBOX_HIC_PARAM] = { 0, };
	u32 data_offset;
	struct icpu_adt_v2 *adt;
	u32 cmd = PABLO_HIC_PDP_STAT0_END;

	mdbg_adt(1, "[F%d]send_msg_pdp_stat0_end: num(%d) stat(0x%llx)", instance,
		fcount, stat_num, stat_buf->dva);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	adt = icpu_adt->priv;

	/* prepare data */
	data_offset = 0;
	data[data_offset++] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_COMMAND,
				       cmd, PABLO_RSP_SUCCESS);
	data[data_offset++] = fcount;

	for (dva_idx = 0; dva_idx < stat_num; dva_idx++) {
		i = dva_idx * 2 + data_offset;
		__pablo_icpu_adt_set_dva(&data[i], &data[i + 1], stat_buf[dva_idx].dva);
	}
	hic_cmd_info[cmd].data_len = data_offset + (stat_num * 2);

	/* send msg */
	ret = __send_nonblocking_message(&hic_cmd_info[cmd], data, cb_caller, cb_ctx,
		&adt->cb_handler[cmd].wait_q[instance]);

	return ret;
}

static int pablo_icpu_adt_send_msg_pdp_stat1_end(struct pablo_icpu_adt *icpu_adt, u32 instance,
	void *cb_caller, void *cb_ctx, u32 fcount, u32 stat_num, struct pablo_crta_buf_info *stat_buf)
{
	int ret;
	u32 dva_idx, i;
	u32 data[MBOX_HIC_PARAM] = { 0, };
	u32 data_offset;
	struct icpu_adt_v2 *adt;
	u32 cmd = PABLO_HIC_PDP_STAT1_END;

	mdbg_adt(1, "[F%d]send_msg_pdp_stat1_end: num(%d) stat(0x%llx)", instance,
		fcount, stat_num, stat_buf->dva);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	adt = icpu_adt->priv;

	/* prepare data */
	data_offset = 0;
	data[data_offset++] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_COMMAND,
				       cmd, PABLO_RSP_SUCCESS);
	data[data_offset++] = fcount;

	for (dva_idx = 0; dva_idx < stat_num; dva_idx++) {
		i = dva_idx * 2 + data_offset;
		__pablo_icpu_adt_set_dva(&data[i], &data[i + 1], stat_buf[dva_idx].dva);
	}
	hic_cmd_info[cmd].data_len = data_offset + (stat_num * 2);

	/* send msg */
	ret = __send_nonblocking_message(&hic_cmd_info[cmd], data, cb_caller, cb_ctx,
		&adt->cb_handler[cmd].wait_q[instance]);

	return ret;
}

static int pablo_icpu_adt_send_msg_cstat_frame_end(struct pablo_icpu_adt *icpu_adt, u32 instance,
	void *cb_caller, void *cb_ctx, u32 fcount, struct pablo_crta_buf_info *shot_buf,
	u32 stat_num, struct pablo_crta_buf_info *stat_buf, u32 edge_score)
{
	int ret;
	u32 dva_idx, i, dva_low;
	u32 data[MBOX_HIC_PARAM] = { 0, };
	u32 data_offset;
	struct icpu_adt_v2 *adt;
	u32 cmd = PABLO_HIC_CSTAT_FRAME_END;

	mdbg_adt(1, "[F%d]send_msg_cstat_frame_end: shot(%pad), edge_score(%d) stat_num(%d)", instance,
		fcount, &shot_buf->dva, edge_score, stat_num);
	for (i = 0; i < stat_num; i++)
		mdbg_adt(5, "stat%d(%pad),", instance, i, &stat_buf[i].dva);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	adt = icpu_adt->priv;

	/* prepare data */
	data_offset = 0;
	/* cmd */
	data[data_offset++] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_COMMAND,
				       cmd, PABLO_RSP_SUCCESS);
	/* fcount */
	data[data_offset++] = fcount;
	/* shot */
	__pablo_icpu_adt_set_dva(&data[data_offset], &data[data_offset + 1], shot_buf->dva);
	data_offset += 2;
	/* edge score */
	data[data_offset++] = edge_score;
	/* BUF_PRE_THUMB, BUF_AE_THUMB, BUF_AWB_THUMB, BUF_RGBY_HIST, BUF_CDAF_MW, BUF_VDAF */
	for (dva_idx = PABLO_CRTA_CSTAT_END_PRE_THUMB; dva_idx < PABLO_CRTA_CSTAT_END_MAX; dva_idx++) {
		i = dva_idx  + data_offset;
		__pablo_icpu_adt_set_dva(&data[i], &dva_low, stat_buf[dva_idx].dva);
		if (dva_low)
			mwarn_adt("dva_low is not 0x0 (0x%x)", instance, dva_low);
	}
	data_offset = dva_idx + data_offset;

	hic_cmd_info[cmd].data_len = data_offset;

	/* send msg */
	ret = __send_nonblocking_message(&hic_cmd_info[cmd], data, cb_caller, cb_ctx,
		&adt->cb_handler[cmd].wait_q[instance]);

	return ret;
}

static int pablo_icpu_adt_send_msg_stop(struct pablo_icpu_adt *icpu_adt, u32 instance, u32 suspend)
{
	int ret;
	u32 data[MBOX_HIC_PARAM] = { 0, };
	struct icpu_adt_v2 *adt;
	u32 cmd = PABLO_HIC_STOP;

	mdbg_adt(1, "send_msg_stop", instance);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	adt = icpu_adt->priv;

	/* prepare data */
	data[0] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_COMMAND,
				       cmd, PABLO_RSP_SUCCESS);
	data[1] = suspend;

	/* send msg */
	if (in_interrupt())
		ret = __send_nonblocking_message(&hic_cmd_info[cmd], data, icpu_adt, NULL,
				&adt->cb_handler[cmd].wait_q[instance]);
	else
		ret = __send_blocking_message(&hic_cmd_info[cmd], data, icpu_adt, NULL,
				&adt->cb_handler[cmd].wait_q[instance]);

	if (suspend == PABLO_STOP_IMMEDIATE)
		CALL_SS_ADT_OPS(adt->sensor_adt[instance], stop);

	return ret;
}

static int pablo_icpu_adt_send_msg_close(struct pablo_icpu_adt *icpu_adt, u32 instance)
{
	int ret;
	u32 data[MBOX_HIC_PARAM] = { 0, };
	struct icpu_adt_v2 *adt;
	u32 cmd = PABLO_HIC_CLOSE;

	mdbg_adt(1, "send_msg_close", instance);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	adt = icpu_adt->priv;

	/* prepare data */
	data[0] = __pablo_icpu_adt_get_msg(instance, PABLO_MESSAGE_COMMAND,
				       cmd, PABLO_RSP_SUCCESS);

	/* send msg */
	ret = __send_blocking_message(&hic_cmd_info[cmd], data, icpu_adt, NULL,
		&adt->cb_handler[cmd].wait_q[instance]);

	return ret;
}

const static struct pablo_icpu_adt_msg_ops icpu_adt_msg_ops = {
	.register_response_msg_cb	= pablo_icpu_adt_register_response_msg_cb,
	.send_msg_open			= pablo_icpu_adt_send_msg_open,
	.send_msg_put_buf		= pablo_icpu_adt_send_msg_put_buf,
	.send_msg_set_shared_buf_idx	= pablo_icpu_adt_send_msg_set_shared_buf_idx,
	.send_msg_start			= pablo_icpu_adt_send_msg_start,
	.send_msg_shot			= pablo_icpu_adt_send_msg_shot,
	.send_msg_cstat_frame_start	= pablo_icpu_adt_send_msg_cstat_frame_start,
	.send_msg_cstat_cdaf_end	= pablo_icpu_adt_send_msg_cstat_cdaf_end,
	.send_msg_pdp_stat0_end		= pablo_icpu_adt_send_msg_pdp_stat0_end,
	.send_msg_pdp_stat1_end		= pablo_icpu_adt_send_msg_pdp_stat1_end,
	.send_msg_cstat_frame_end	= pablo_icpu_adt_send_msg_cstat_frame_end,
	.send_msg_stop			= pablo_icpu_adt_send_msg_stop,
	.send_msg_close			= pablo_icpu_adt_send_msg_close,
};

static int __pablo_icpu_adt_open_icpu_itf(struct pablo_icpu_adt *icpu_adt, u32 instance)
{
	int ret;

	ret = ITF_API(__itf, open);
	if (ret) {
		merr_adt("failed to __itf->open(%d)", instance, ret);
		return ret;
	}

	ret = ITF_API(__itf, wait_boot_complete, TIMEOUT_ICPU);
	if (ret) {
		merr_adt("failed to boot icpu(%d)", instance, ret);
		goto err_itf;
	}

	ret = ITF_API(__itf, register_msg_handler, icpu_adt, __pablo_icpu_adt_receive_msg);
	if (ret) {
		merr_adt("failed to register_msg_handler(%d)", instance, ret);
		goto err_itf;
	}

	ret = ITF_API(__itf, register_err_handler, icpu_adt, __pablo_icpu_adt_fw_err_handler);
	if (ret) {
		merr_adt("failed to register_err_handler(%d)", instance, ret);
		goto err_itf;
	}

	return 0;

err_itf:
	ITF_API(__itf, close);
	return ret;
}

static void __pablo_icpu_adt_close_icpu_itf(void)
{
	ITF_API(__itf, register_msg_handler, NULL, NULL);
	ITF_API(__itf, register_err_handler, NULL, NULL);
	ITF_API(__itf, close);
}

static int pablo_icpu_adt_open(struct pablo_icpu_adt *icpu_adt,
	u32 instance,
	struct is_sensor_interface *sensor_itf,
	struct pablo_crta_bufmgr *bufmgr)
{
	int ret;
	u32 cmd;
	struct icpu_adt_v2 *adt;
	struct block_msg_wait_q *wait_q;

	mdbg_adt(1, "%s\n", instance, __func__);

	if (instance >= IS_STREAM_COUNT) {
		err("invalid instance(%d)", instance);
		return -EINVAL;
	}

	if (!atomic_read(&icpu_adt->rsccount)) {
		/* alloc priv */
		adt = vzalloc(sizeof(struct icpu_adt_v2));
		if (!adt) {
			merr_adt("failed to alloc icpu_adt_v2", instance);
			return -ENOMEM;
		}
		icpu_adt->priv = (void *)adt;
		__icpu_adt = icpu_adt;

		ret = __pablo_icpu_adt_open_icpu_itf(icpu_adt, instance);
		if (ret)
			goto err_itf;
	} else {
		adt = (struct icpu_adt_v2 *)icpu_adt->priv;
	}

	for (cmd = PABLO_HIC_OPEN; cmd < PABLO_HIC_MAX; cmd++) {
		wait_q = &adt->cb_handler[cmd].wait_q[instance];
		wait_q->instance = instance;
		wait_q->cmd = cmd;
		refcount_set(&wait_q->state, 0);
		init_waitqueue_head(&wait_q->head);
	}

	if (sensor_itf && !adt->sensor_adt[instance]) {
		adt->sensor_adt[instance] = vzalloc(sizeof(struct pablo_sensor_adt));
		if (!adt->sensor_adt[instance]) {
			merr_adt("failed to alloc pablo_sensor_adt", instance);
			ret = -ENOMEM;
			goto err_sensor_adt_alloc;
		}

		ret = pablo_sensor_adt_probe(adt->sensor_adt[instance]);
		if (ret) {
			merr_adt("failed to pablo_sensor_adt_probe", instance);
			goto err_sensor_adt_open;
		}

		ret = CALL_SS_ADT_OPS(adt->sensor_adt[instance], open, instance, sensor_itf);
		if (ret) {
			merr_adt("failed to sensor_adt open", instance);
			goto err_sensor_adt_open;
		}

		ret = CALL_SS_ADT_OPS(adt->sensor_adt[instance], register_control_sensor_callback,
			icpu_adt, __pablo_icpu_adt_control_sensor_callback);
		if (ret) {
			merr_adt("failed to register_control_sensor_callback(%d)", instance, ret);
			goto err_sensor_adt_register;
		}
	}

	adt->bufmgr[instance] = bufmgr;
	atomic_inc(&icpu_adt->rsccount);

	return 0;

err_sensor_adt_register:
	CALL_SS_ADT_OPS(adt->sensor_adt[instance], close);
err_sensor_adt_open:
	vfree(adt->sensor_adt[instance]);
	adt->sensor_adt[instance] = NULL;
err_sensor_adt_alloc:
	if (atomic_read(&icpu_adt->rsccount))
		return ret;
	__pablo_icpu_adt_close_icpu_itf();
err_itf:
	vfree(adt);
	icpu_adt->priv = NULL;
	return ret;
}

static int pablo_icpu_adt_close(struct pablo_icpu_adt *icpu_adt, u32 instance)
{
	int ret;
	struct icpu_adt_v2 *adt;

	adt = (struct icpu_adt_v2 *)icpu_adt->priv;

	mdbg_adt(1, "%s\n", instance, __func__);

	ret = __pabli_icpu_adt_is_open(icpu_adt, instance);
	if (ret)
		return ret;

	ret = CALL_SS_ADT_OPS(adt->sensor_adt[instance], close);
	if (ret)
		merr_adt("failed to close sensor_adt", instance);

	vfree(adt->sensor_adt[instance]);
	adt->sensor_adt[instance] = NULL;

	if (!atomic_dec_return(&icpu_adt->rsccount)) {
		__pablo_icpu_adt_close_icpu_itf();
		vfree(icpu_adt->priv);
		icpu_adt->priv = NULL;
	}

	return ret;
}

const static struct pablo_icpu_adt_ops icpu_adt_ops = {
	.open		= pablo_icpu_adt_open,
	.close		= pablo_icpu_adt_close,
};

struct pablo_icpu_adt *pablo_get_icpu_adt(void)
{
	return __icpu_adt;
}
KUNIT_EXPORT_SYMBOL(pablo_get_icpu_adt);

void pablo_set_icpu_adt(struct pablo_icpu_adt *icpu_adt)
{
	__icpu_adt = icpu_adt;
}
KUNIT_EXPORT_SYMBOL(pablo_set_icpu_adt);

int pablo_icpu_adt_probe(void)
{
	probe_info("%s", __func__);

	__icpu_adt = NULL;

	if (!IS_ENABLED(CRTA_CALL)) {
		pr_info("crta is not supported");
		return 0;
	}

	__itf = pablo_icpu_itf_api_get();
	if (!__itf) {
		pr_err("fail to get __itf api");
		return -ENODEV;
	}

	__icpu_adt = kvzalloc(sizeof(struct pablo_icpu_adt), GFP_KERNEL);
	if (!__icpu_adt) {
		err("failed to allocate pablo_icpu_adt\n");
		return -ENOMEM;
	}

	atomic_set(&__icpu_adt->rsccount, 0);
	__icpu_adt->ops = &icpu_adt_ops;
	__icpu_adt->msg_ops = &icpu_adt_msg_ops;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_adt_probe);

int pablo_icpu_adt_remove(void)
{
	probe_info("%s", __func__);

	kvfree(__icpu_adt);
	__icpu_adt = NULL;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_icpu_adt_remove);
