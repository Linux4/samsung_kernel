// SPDX-License-Identifier: GPL
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2021 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/sched/clock.h>

#include "pablo-icpu.h"
#include "pablo-icpu-itf.h"
#include "pablo-icpu-core.h"
#include "pablo-icpu-msg-queue.h"
#include "pablo-icpu-debug.h"
#include "mbox/pablo-icpu-mbox.h"
#include "pablo-dvfs.h"

static struct icpu_logger _log = {
	.level = LOGLEVEL_INFO,
	.prefix = "[ICPU-ITF]",
};

struct icpu_logger *get_icpu_itf_log(void)
{
	return &_log;
}

enum itf_state {
	ITF_STATE_INIT,
	ITF_STATE_WAIT_FW_READY,
	ITF_STATE_RUNNING,
	ITF_STATE_CLOSING,
	ITF_STATE_MAX,
};

struct icpu_itf;
struct itf_fsm {
	void (*pre)(struct icpu_itf *itf);
	void (*action)(struct icpu_itf *itf);
	void (*post)(struct icpu_itf *itf, enum itf_state);
};
#define FSM_PRE(x) (x.pre ? x.pre(itf) : 0)
#define FSM_ACTION(x) (x.action ? x.action(itf) : 0)
#define FSM_POST(x, s) (x.post ? x.post(itf, s) : 0)

#define SYNC_WAIT_TIMEOUT_MS (60)

#define ICPU_TRACE_BEGIN_CB(_d, _l) \
	ICPU_TRACE_BEGIN("[%08x %08x] len:%d", _d[0], _d[1], _l)

#define ICPU_TRACE_END_CB(_d) \
	ICPU_TRACE_END("[%08x %08x]", _d[0], _d[1])

#ifdef ENABLE_ICPU_TRACE
#define ICPU_TRACE_PREPARE_TX() u32 _trace_data[2]
#define ICPU_TRACE_BEGIN_TX(_d, _l)                                                                \
	do {                                                                                       \
		_trace_data[0] = _d[0];                                                            \
		_trace_data[1] = _d[1];                                                            \
		ICPU_TRACE_BEGIN_CB(_trace_data, _l);                                              \
	} while (0)
#define ICPU_TRACE_END_TX() ICPU_TRACE_END_CB(_trace_data)

#define ICPU_TRACE_PREPARE_MSG() union icpu_itf_msg_cmd trace_cmd
#define ICPU_TRACE_BEGIN_MSG(_c, _m)                                                               \
	do {                                                                                       \
		trace_cmd = _c;                                                                    \
		ICPU_TRACE_BEGIN("[%08x %08x] len:%d prio:%d cb:%d data:%x %x %x %x %x %x",        \
				 _c.data[0], _c.data[1], _m->len, _m->prio,                        \
				 _m->cb_ctx.callback ? 1 : 0, _m->data[2], _m->data[3],            \
				 _m->data[4], _m->data[5], _m->data[6], _m->data[7]);              \
	} while (0)
#define ICPU_TRACE_END_MSG() ICPU_TRACE_END_CB(trace_cmd.data)
#else
#define ICPU_TRACE_PREPARE_TX()
#define ICPU_TRACE_BEGIN_TX(_d, _l)
#define ICPU_TRACE_END_TX()

#define ICPU_TRACE_PREPARE_MSG()
#define ICPU_TRACE_BEGIN_MSG(_c, _m)
#define ICPU_TRACE_END_MSG()
#endif

struct ihc_msg_handler {
	void *cookie;
	icpu_itf_ihc_msg_handler_t cb;
};

struct ihc_err_handler {
	void *cookie;
	icpu_itf_ihc_err_handler_t cb;
};

static struct icpu_itf {
	struct pablo_icpu_itf_api ops;
	struct icpu_mbox_client cl;
	struct pablo_icpu_mbox_chan *tx;
	struct pablo_icpu_mbox_chan **rx;

	enum itf_state state;
	struct itf_fsm fsm[ITF_STATE_MAX];
	struct completion boot_done;

	struct work_struct work;
	int cpu;

	struct icpu_itf_msg msgs[MESSAGE_MAX_COUNT];

	struct icpu_msg_queue free_queue;
	struct icpu_msg_queue pending_queue;
	struct icpu_msg_queue response_queue;

	struct ihc_msg_handler ihc_msg_handler;
	struct ihc_err_handler ihc_err_handler;
} *itf;

static void __fsm_wait_fw_ready_action(struct icpu_itf *itf)
{
	init_completion(&itf->boot_done);
}

static void __fsm_wait_fw_ready_post(struct icpu_itf *itf, enum itf_state s)
{
	switch (s) {
	case ITF_STATE_RUNNING:
		ICPU_INFO("Firmware ready");
		complete(&itf->boot_done);
		break;
	case ITF_STATE_INIT:
		ICPU_ERR("Core boot fail");
		break;
	case ITF_STATE_CLOSING:
		ICPU_ERR("Firmware boot fail");
		pablo_icpu_print_debug_reg();
		break;
	default:
		ICPU_ERR("Wrong next state: %d", s);
		break;
	}
}

static int __send_powerdown_msg(void)
{
	int ret;
	int retry = 50;
	enum icpu_msg_priority prio = ICPU_MSG_PRIO_HIGH;
	u32 data[2] = {
		TO_CMD(PABLO_HIC_POWER_DOWN),
		0,
	};

	do {
		ret = itf->ops.send_message(NULL, NULL, NULL, prio, 1, data);
		if (ret == 0) {
			break;
		} else if (ret == -EBUSY && retry) {
			usleep_range(5000, 5100);
			retry--;
		} else {
			ICPU_INFO("Fail to send power down msg, ret(%d), retry(%d)", ret, retry);
			break;
		}
	} while (retry);

	return ret;
}

static void __fsm_running_post(struct icpu_itf *itf, enum itf_state s)
{
	switch (s) {
	case ITF_STATE_CLOSING:
		__send_powerdown_msg();
		break;
	default:
		break;
	}
}

static bool __check_current_state(enum itf_state s)
{
	if (!itf)
		return false;

	return itf->state == s ? true : false;
}

static void __transit_state(enum itf_state new_state)
{
	enum itf_state cur_state;

	if (!itf)
		return;

	cur_state = itf->state;

	FSM_POST(itf->fsm[cur_state], new_state);

	FSM_PRE(itf->fsm[new_state]);

	itf->state = new_state;

	FSM_ACTION(itf->fsm[new_state]);
}

static DEFINE_MUTEX(msg_mutex);
/* spinlock use for ISR */
static struct workqueue_struct *msg_wq;

static inline void __queue_pending_msg_work(struct icpu_itf *itf)
{

	if (!__check_current_state(ITF_STATE_RUNNING))
		return;

	if (!work_pending(&itf->work))
		queue_work_on(itf->cpu, msg_wq, &itf->work);
}

static inline void __init_msg(struct icpu_itf_msg *msg)
{
	msg->cb_ctx.sender = NULL;
	msg->cb_ctx.cookie = NULL;
	msg->cb_ctx.callback = NULL;
	msg->cmd.key = 0;
	msg->prio = 0;
	memset(msg->data, 0, sizeof(msg->data));
	msg->len = 0;
}

static void __print_all_queues(struct icpu_itf *itf, int num_pending, int num_response)
{
	ICPU_INFO("Max num of message=%d", MESSAGE_MAX_COUNT);
	ICPU_INFO("Num of free_msg=%d, pending_msg=%d, response_msg=%d",
		QUEUE_LEN(&itf->free_queue), QUEUE_LEN(&itf->pending_queue),
		QUEUE_LEN(&itf->response_queue));

	ICPU_INFO("List of pending message:");
	QUEUE_DUMP(&itf->pending_queue, num_pending);

	ICPU_INFO("List of response message:");
	QUEUE_DUMP(&itf->response_queue, num_response);
}

static inline struct icpu_itf_msg *__get_free_msg_lock(struct icpu_itf *itf)
{
	struct icpu_itf_msg *msg = QUEUE_GET_MSG(&itf->free_queue);
	struct ihc_err_handler *handler = &itf->ihc_err_handler;

	if (msg)
		return msg;

	ICPU_ERR("fail to acquire msg");

	if (QUEUE_FULL(&itf->response_queue, MESSAGE_MAX_COUNT)) {
		if (handler->cb)
			handler->cb(handler->cookie, "ICPU/CRTA NO RESPONSE");
	}

	__print_all_queues(itf, 1, 0);

	return NULL;
}

static inline void __set_free_msg_lock(struct icpu_itf *itf, struct icpu_itf_msg *msg)
{
	unsigned long _lock_flags;
	struct icpu_msg_queue *q = &itf->free_queue;

	if (!msg) {
		ICPU_ERR("Invalid msg, msg(%p)", msg);
		return;
	}

	spin_lock_irqsave(&q->lock, _lock_flags);

	/* msg->len should be 0 if msg is already in free queue */
	if (msg->len) {
		__init_msg(msg);
		q->set_msg(q, msg);
	}

	spin_unlock_irqrestore(&q->lock, _lock_flags);
}

static inline struct icpu_itf_msg *__get_pending_msg_prio_lock(struct icpu_itf *itf)
{
	return QUEUE_GET_MSG(&itf->pending_queue);
}

static inline void __set_pending_msg_prio_lock(struct icpu_itf *itf, struct icpu_itf_msg *msg)
{
	msg->time = local_clock();
	QUEUE_SET_MSG(&itf->pending_queue, msg);
	__queue_pending_msg_work(itf);
}

static inline struct icpu_itf_msg *__get_response_msg_match_lock(struct icpu_itf *itf,
								 union icpu_itf_msg_cmd cmd)
{
	return QUEUE_GET_MSG_KEY(&itf->response_queue, cmd.key);
}

static inline void __set_response_msg_lock(struct icpu_itf *itf, struct icpu_itf_msg *msg)
{
	QUEUE_SET_MSG(&itf->response_queue, msg);
}

static inline void __del_response_msg_match_lock(struct icpu_itf *itf, union icpu_itf_msg_cmd cmd)
{
	struct icpu_itf_msg *tmp_msg = QUEUE_GET_MSG_KEY(&itf->response_queue, cmd.key);

	if (!tmp_msg)
		ICPU_ERR("Could not find msg, key[%08x %08x]", cmd.data[0], cmd.data[1]);
}

/* Return 1 when message add to pending queue otherwise return 0 */
static inline int __check_pending_msg_and_set(struct icpu_itf *itf, struct icpu_itf_msg *msg)
{
	int ret;

	ret = QUEUE_SET_MSG_IF_NOT_EMPTY(&itf->pending_queue, msg);
	if (ret)
		__queue_pending_msg_work(itf);

	return ret;
}

static void __print_str_with_u32_array(char *str, u32 *data, u32 len)
{
	int i;

	ICPU_ERR("%s: ", str);

	for (i = 0; i < len; i++)
		pr_cont("0x%x, ", data[i]);
}

static inline bool __is_response_msg(u32 cmd)
{
	return GET_TYPE(cmd) == MSG_TYPE_RSP;
}

static void __ihc_error_handler(char *str, u32 *rx_data, u32 len)
{
	__print_str_with_u32_array(str, rx_data, len);

	if (itf->ihc_err_handler.cb)
		itf->ihc_err_handler.cb(itf->ihc_err_handler.cookie, str);
}

static void __send_timestamp_msg(void)
{
	int ret;
	u32 data[3] = { TO_CMD(PABLO_HIC_SET_TIMESTAMP), 0, 0, };
	u64 ts_usec = sched_clock() / NSEC_PER_USEC;

	data[1] = ts_usec >> 32;
	data[2] = ts_usec & 0xFFFFFFFF;

	ICPU_DEBUG("time stamp: 0x%llx (0x%x, 0x%x)", ts_usec, data[1], data[2]);

	ret = itf->ops.send_message(NULL, NULL, NULL, ICPU_MSG_PRIO_HIGH, 3, data);
	if (ret)
		ICPU_INFO("Fail to send timestamp down msg, ret(%d)", ret);
}

static void __send_icpu_clock_msg(u32 clock)
{
	int ret;
	u32 data[3] = {
		TO_CMD(PABLO_HIC_SET_ICPUCLK),
		0,
		0,
	};

	data[1] = clock;

	ICPU_DEBUG("icpu clock: %dMHz (0x%x, 0x%x)", clock, data[1], data[2]);

	ret = itf->ops.send_message(NULL, NULL, NULL, ICPU_MSG_PRIO_HIGH, 3, data);
	if (ret)
		ICPU_INFO("Fail to send timestamp down msg, ret(%d)", ret);
}

static void __handle_icpu_fw_msg(u32 *rx_data, u32 len)
{
	u32 cmd = GET_CMD(rx_data[0]);

	if (cmd == PABLO_IHC_READY) {
		__transit_state(ITF_STATE_RUNNING);
		ICPU_INFO("Debug RUNNING state, current state(%d)", itf->state);
		__send_timestamp_msg();
	} else if (cmd == PABLO_IHC_ERROR) {
		__ihc_error_handler("ICPU/CRTA ASSERT", rx_data, len);
	} else {
		pablo_icpu_handle_msg(rx_data, len);
	}
}

static void __handle_ihc_msg(u32 *rx_data, u32 len)
{
	if (CHECK_ICPU_FW_CMD(rx_data[0]) == true)
		__handle_icpu_fw_msg(rx_data, len);
	else if (itf->ihc_msg_handler.cb)
		itf->ihc_msg_handler.cb(itf->ihc_msg_handler.cookie, rx_data);
	else
		__print_str_with_u32_array("No IHC msg handler", rx_data, len);
}

static struct callback_ctx __get_response_callback_ctx(u32 *rx_data, u32 len)
{
	struct callback_ctx cb_ctx = { 0, };
	struct icpu_itf_msg *msg;
	union icpu_itf_msg_cmd cmd;

	cmd.data[0] = CLR_TYPE_ERR(rx_data[0]);
	cmd.data[1] = rx_data[1];

	msg = __get_response_msg_match_lock(itf, cmd);
	if (!msg) {
		__print_str_with_u32_array("Could not find msg for response", rx_data, len);
		ICPU_TRACE("RSP_LOST|rx[%08x %08x] cmd[%08x %08x] len:%d",
				rx_data[0], rx_data[1], cmd.data[0], cmd.data[1], len);
		/* TODO: dump queue history */
		return cb_ctx;
	}

	cb_ctx = msg->cb_ctx;
	__set_free_msg_lock(itf, msg);

	return cb_ctx;
}

static void __rx_callback(struct icpu_mbox_client *cl, u32 *rx_data, u32 len)
{
	struct callback_ctx cb_ctx;

	if (!cl || !rx_data)
		return;

	ICPU_DEBUG("rx_data= 0x%x 0x%x 0x%x 0x%x", rx_data[0], rx_data[1], rx_data[2], rx_data[3]);

	ICPU_TRACE_BEGIN_CB(rx_data, len);
	if (__is_response_msg(rx_data[0])) {
		cb_ctx = __get_response_callback_ctx(rx_data, len);

		if (cb_ctx.callback)
			cb_ctx.callback(cb_ctx.sender, cb_ctx.cookie, rx_data);
	} else {
		__handle_ihc_msg(rx_data, len);
	}
	ICPU_TRACE_END_CB(rx_data);
}

static void __tx_done(struct icpu_mbox_client *cl, void *msg, int len)
{
	if (QUEUE_LEN(&itf->pending_queue))
		__queue_pending_msg_work(itf);
}

static int __send_message(struct icpu_itf *itf, struct icpu_itf_msg *msg)
{
	int ret;
	struct pablo_icpu_mbox_chan *chan = itf->tx;

	ICPU_TRACE_PREPARE_TX();

	if (!chan || !chan->mbox->ops->send_data)
		return -EINVAL;

	ICPU_TRACE_BEGIN_TX(msg->data, msg->len);
	msg->time = local_clock();
	ret = chan->mbox->ops->send_data(chan, msg->data, msg->len);
	ICPU_TRACE_END_TX();

	return ret;
}

static void __send_message_err_handler(struct icpu_itf *itf, struct icpu_itf_msg *msg)
{
	MSG_DUMP(msg);

	if (msg->cb_ctx.callback) {
		pablo_icpu_print_debug_reg();
		msg->cb_ctx.callback(msg->cb_ctx.sender, msg->cb_ctx.cookie, msg->data);
		__del_response_msg_match_lock(itf, msg->cmd);
	}
	__set_free_msg_lock(itf, msg);
}

static void __pending_msg_work_func(struct work_struct *w)
{
	int ret;
	struct icpu_itf *itf =
		container_of(w, struct icpu_itf, work);
	struct icpu_itf_msg *msg;

	/* list del pending queue */
	mutex_lock(&msg_mutex);
	msg = __get_pending_msg_prio_lock(itf);
	while (msg) {
		if (msg->cb_ctx.callback)
			__set_response_msg_lock(itf, msg);

		ret = __send_message(itf, msg);
		mutex_unlock(&msg_mutex);
		if (ret) {
			ICPU_ERR("Fail to send message, ret(%d)", ret);
			__send_message_err_handler(itf, msg);
		} else if (!msg->cb_ctx.callback) {
			__set_free_msg_lock(itf, msg);
		}

		mutex_lock(&msg_mutex);
		msg = __get_pending_msg_prio_lock(itf);
	}
	mutex_unlock(&msg_mutex);
}

static int __init_msg_queue(struct icpu_itf *itf)
{
	int ret;
	int i;

	ret = QUEUE_INIT(&itf->free_queue);
	if (ret)
		return ret;

	ret = PRIORITY_QUEUE_INIT(&itf->pending_queue, ICPU_MSG_PRIO_MAX);
	if (ret)
		return ret;

	ret = QUEUE_INIT(&itf->response_queue);
	if (ret)
		return ret;

	for (i = 0; i < MESSAGE_MAX_COUNT; i++) {
		__init_msg(&itf->msgs[i]);
		QUEUE_SET_MSG(&itf->free_queue, &itf->msgs[i]);
	}

	return 0;
}

static void __deinit_msg_queue(struct icpu_itf *itf)
{
	QUEUE_DEINIT(&itf->free_queue);
	QUEUE_DEINIT(&itf->pending_queue);
	QUEUE_DEINIT(&itf->response_queue);
}

static void __setup_client(struct icpu_itf *itf)
{
	itf->cl.rx_callback = __rx_callback;
	itf->cl.tx_prepare = NULL;
	itf->cl.tx_done = __tx_done;
}

static void __reset_client(struct icpu_itf *itf)
{
	memset(&itf->cl, 0x0, sizeof(struct icpu_mbox_client));
}

static int __request_mbox_channels(struct icpu_itf *itf)
{
	u32 rx_idx = 0;
	u32 num_rx_mbox = pablo_icpu_get_num_rx_mbox();

	itf->tx = pablo_icpu_request_mbox_chan(&itf->cl, ICPU_MBOX_CHAN_TX);
	if (!itf->tx)
		goto tx_chan_fail;

	itf->rx = kcalloc(num_rx_mbox, sizeof(void *), GFP_KERNEL);
	if (!itf->rx)
		goto rx_chan_fail;

	for (rx_idx = 0; rx_idx < num_rx_mbox; rx_idx++) {
		itf->rx[rx_idx] = pablo_icpu_request_mbox_chan(&itf->cl, ICPU_MBOX_CHAN_RX);
		if (!itf->rx[rx_idx])
			goto rx_chan_fail;
	}

	return 0;

rx_chan_fail:
	while (rx_idx-- > 0) {
		pablo_icpu_free_mbox_chan(itf->rx[rx_idx]);
		itf->rx[rx_idx] = NULL;
	}
	kfree(itf->rx);

tx_chan_fail:
	pablo_icpu_free_mbox_chan(itf->tx);
	itf->tx = NULL;

	return -ENOMEM;
}

static void __free_mbox_channels(struct icpu_itf *itf)
{
	u32 i;
	u32 num_rx_mbox = pablo_icpu_get_num_rx_mbox();

	if (itf->tx)
		pablo_icpu_free_mbox_chan(itf->tx);
	itf->tx = NULL;

	if (itf->rx) {
		for (i = 0; i < num_rx_mbox; i++) {
			if (itf->rx[i]) {
				pablo_icpu_free_mbox_chan(itf->rx[i]);
				itf->rx[i] = NULL;
			}
		}
		kfree(itf->rx);
		itf->rx = NULL;
	}
}

static int __init_wq(struct icpu_itf *itf)
{
	msg_wq = alloc_workqueue("icpu_itf_msg_wq", WQ_HIGHPRI, 0);
	if (!msg_wq)
		return -ENOMEM;

	INIT_WORK(&itf->work, __pending_msg_work_func);
	itf->cpu = WORK_CPU_UNBOUND;

	return 0;
}

static void __reset_wq(struct icpu_itf *itf)
{
	/* TODO: Check flush and cancel */
	flush_work(&itf->work);
	cancel_work_sync(&itf->work);

	destroy_workqueue(msg_wq);
}

static int __handle_async_msg(struct icpu_itf *itf, struct icpu_itf_msg *msg)
{
	int ret;

	if (mutex_trylock(&msg_mutex) == 0) {
		__set_pending_msg_prio_lock(itf, msg);

		/* TODO: how to notify error if fail to send pending msg */
		return 0;
	}

	if (__check_pending_msg_and_set(itf, msg)) {
		mutex_unlock(&msg_mutex);
		return 0;
	}

	if (msg->cb_ctx.callback)
		__set_response_msg_lock(itf, msg);

	ret = __send_message(itf, msg);
	mutex_unlock(&msg_mutex);

	if (ret)
		__send_message_err_handler(itf, msg);
	else if (!msg->cb_ctx.callback)
		__set_free_msg_lock(itf, msg);

	return ret;
}

static int pablo_icpu_itf_open(bool secure_mode)
{
	int ret;

	ICPU_INFO("secure_mode(%d)", secure_mode);

	if (!__check_current_state(ITF_STATE_INIT)) {
		ICPU_ERR("current state(%d) is not ITF_STATE_INIT", itf->state);
		return -ENODEV;
	}

	ret = __init_msg_queue(itf);
	if (ret) {
		ICPU_ERR("__init_msg_queue failed. ret(%d)", ret);
		return ret;
	}

	__setup_client(itf);

	ret = __request_mbox_channels(itf);
	if (ret)
		goto err_request_mbox_channels;

	ret = __init_wq(itf);
	if (ret) {
		ICPU_ERR("__init_wq failed. ret(%d)", ret);
		goto err_init_wq;
	}

	__transit_state(ITF_STATE_WAIT_FW_READY);
	ICPU_INFO("Debug READ state, current state(%d)", itf->state);

	ret = pablo_icpu_boot(secure_mode);
	if (ret)
		goto err_icpu_boot;

	pablo_register_change_dvfs_cb(IS_DVFS_ICPU, __send_icpu_clock_msg);

	return 0;

err_icpu_boot:
	__transit_state(ITF_STATE_INIT);
	ICPU_INFO("Debug err INIT state, current state(%d)", itf->state);
	__reset_wq(itf);

err_init_wq:
	__free_mbox_channels(itf);

err_request_mbox_channels:
	__reset_client(itf);

	return ret;
}

static void pablo_icpu_itf_close(void)
{
	if (!itf) {
		ICPU_ERR("itf is NULL!!!");
		return;
	}

	if (__check_current_state(ITF_STATE_INIT))
		return;

	__transit_state(ITF_STATE_CLOSING);
	ICPU_INFO("Debug CLOSING state, current state(%d)", itf->state);

	pablo_register_change_dvfs_cb(IS_DVFS_ICPU, NULL);

	pablo_icpu_power_down();

	__reset_wq(itf);
	__deinit_msg_queue(itf);

	__free_mbox_channels(itf);

	__reset_client(itf);

	__transit_state(ITF_STATE_INIT);
	ICPU_INFO("Debug INIT state, current state(%d)", itf->state);
}

static int __send_message_pre(unsigned int num, u32 *data)
{
	if (num < 1 || !data)
		return -EINVAL;

	if (__check_current_state(ITF_STATE_WAIT_FW_READY))
		return -EAGAIN;

	if (!__check_current_state(ITF_STATE_RUNNING))
		return -ENODEV;

	return 0;
}

static struct icpu_itf_msg *__send_message_acquire_msg(struct callback_ctx ctx, enum icpu_msg_priority prio,
		unsigned int num, u32 *data)
{
	int i;
	struct icpu_itf_msg *msg;

	msg = __get_free_msg_lock(itf);

	if (!msg)
		return NULL;

	for (i = 0; i < num; i++)
		msg->data[i] = data[i];

	msg->cmd.data[0] = msg->data[0];
	msg->cmd.data[1] = msg->data[1];
	msg->len = num;
	msg->cb_ctx = ctx;
	msg->prio = prio;

	return msg;
}

static int pablo_icpu_itf_send_message(void *sender, void *cookie, icpu_itf_callback_func_t callback,
		enum icpu_msg_priority msg_prio, unsigned int num, u32 *data)
{
	int ret;
	struct icpu_itf_msg *msg;
	struct callback_ctx cb_ctx = { sender, cookie, callback, };

	ICPU_TRACE_PREPARE_MSG();

	msg = __send_message_acquire_msg(cb_ctx, msg_prio, num, data);
	if (!msg)
		return -EBUSY;

	ICPU_DEBUG("msg key(0x%x, 0x%x), len(%d), prio(%d), pendinglist(%d), rsplist(%d)",
			msg->cmd.data[0], msg->cmd.data[1], num, msg_prio,
			QUEUE_LEN(&itf->pending_queue), QUEUE_LEN(&itf->response_queue));

	ICPU_TRACE_BEGIN_MSG(msg->cmd, msg);

	ret = __handle_async_msg(itf, msg);

	ICPU_TRACE_END_MSG();

	return ret;
}

static int pablo_icpu_itf_send_message_isr(void *sender, void *cookie, icpu_itf_callback_func_t callback,
		enum icpu_msg_priority msg_prio, unsigned int num, u32 *data)
{
	int ret;
	struct icpu_itf_msg *msg;
	struct callback_ctx cb_ctx = { sender, cookie, callback, };

	ICPU_TRACE_PREPARE_MSG();

	msg = __send_message_acquire_msg(cb_ctx, msg_prio, num, data);
	if (!msg)
		return -EBUSY;

	ICPU_DEBUG("msg key(0x%x, 0x%x), len(%d), prio(%d), freelist(%d) pendinglist(%d), rsplist(%d)",
			msg->cmd.data[0], msg->cmd.data[1], num, msg_prio, QUEUE_LEN(&itf->free_queue),
			QUEUE_LEN(&itf->pending_queue), QUEUE_LEN(&itf->response_queue));

	ICPU_TRACE_BEGIN_MSG(msg->cmd, msg);

	if (msg->cb_ctx.callback)
		__set_response_msg_lock(itf, msg);

	ret = __send_message(itf, msg);
	if (ret) {
		ICPU_ERR("Fail to send message, ret(%d)", ret);
		__send_message_err_handler(itf, msg);
	} else if (!msg->cb_ctx.callback) {
		__set_free_msg_lock(itf, msg);
	}

	ICPU_TRACE_END_MSG();

	return 0;
}

static int pablo_icpu_itf_send_message_wrap(void *sender, void *cookie, icpu_itf_callback_func_t callback,
		enum icpu_msg_priority msg_prio, unsigned int num, u32 *data)
{
	int ret;
	static int cnt;

	ret = __send_message_pre(num, data);
	if (ret)
		return ret;

	if (in_interrupt())
		return pablo_icpu_itf_send_message_isr(sender, cookie, callback, msg_prio, num, data);

	ret = pablo_icpu_itf_send_message(sender, cookie, callback, msg_prio, num, data);

	if (++cnt == 100) {
		cnt = 0;
		__send_timestamp_msg();
	}

	return ret;
}

static int pablo_icpu_itf_register_msg_handler(void *cookie, icpu_itf_ihc_msg_handler_t handler)
{
	if (!itf)
		return -ENODEV;

	// TODO: need to maintain list of handler?
	itf->ihc_msg_handler.cookie = cookie;
	itf->ihc_msg_handler.cb = handler;

	return 0;
}

static int pablo_icpu_itf_register_err_handler(void *cookie, icpu_itf_ihc_err_handler_t handler)
{
	if (!itf)
		return -ENODEV;

	itf->ihc_err_handler.cookie = cookie;
	itf->ihc_err_handler.cb = handler;

	return 0;
}

static int pablo_icpu_itf_wait_boot_complete(u32 timeout_ms)
{
	int ret = 0;

	if (!itf)
		return -ENODEV;

	switch (itf->state) {
	case ITF_STATE_WAIT_FW_READY:
		if (wait_for_completion_timeout(&itf->boot_done, msecs_to_jiffies(timeout_ms)) == 0)
			ret = -ETIMEDOUT;
		break;
	case ITF_STATE_RUNNING:
		break;
	default:
		ret = -1;
		break;
	}

	return ret;
}

static void pablo_icpu_itf_panic_handler(void)
{
	if (itf && !__check_current_state(ITF_STATE_INIT))
		__print_all_queues(itf, 0, 0);
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
int pablo_icpu_test_itf_open(struct icpu_mbox_client cl,
		struct pablo_icpu_mbox_chan *tx,
		struct pablo_icpu_mbox_chan **rx)
{
	int ret;

	if (!__check_current_state(ITF_STATE_INIT))
		return -ENODEV;

	if (!cl.rx_callback || !tx || !rx)
		return -EINVAL;

	ret = __init_wq(itf);
	if (ret)
		return ret;

	__init_msg_queue(itf);

	itf->cl = cl;
	itf->tx = tx;
	itf->rx = rx;

	__transit_state(ITF_STATE_RUNNING);

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_icpu_test_itf_open);

void pablo_icpu_test_itf_close(void)
{
	if (!itf)
		return;

	__reset_wq(itf);
	__deinit_msg_queue(itf);

	memset(&itf->cl, 0x0, sizeof(struct icpu_mbox_client));
	itf->tx = NULL;
	itf->rx = NULL;

	__transit_state(ITF_STATE_INIT);

}
EXPORT_SYMBOL_GPL(pablo_icpu_test_itf_close);

int pablo_icpu_itf_shutdown_firmware(void)
{
	/* TODO: Does itf need to flush all pending message or cancel ??? */
	return __send_powerdown_msg();
}
EXPORT_SYMBOL_GPL(pablo_icpu_itf_shutdown_firmware);

struct icpu_mbox_client pablo_icpu_test_itf_get_client(void)
{
	struct icpu_mbox_client cl;

	cl.rx_callback = __rx_callback;
	cl.tx_prepare = NULL;
	cl.tx_done = __tx_done;

	return cl;
}
EXPORT_SYMBOL_GPL(pablo_icpu_test_itf_get_client);

struct icpu_itf_msg *get_free_msg_wrap(void)
{
	if (!itf)
		return NULL;

	return __get_free_msg_lock(itf);
}
EXPORT_SYMBOL_GPL(get_free_msg_wrap);

struct icpu_itf_msg *get_pending_msg_prio_wrap(void)
{
	struct icpu_itf_msg *msg;

	if (!itf)
		return NULL;

	msg = __get_pending_msg_prio_lock(itf);

	return msg;
}
EXPORT_SYMBOL_GPL(get_pending_msg_prio_wrap);

struct icpu_itf_msg *get_response_msg_match_wrap(union icpu_itf_msg_cmd cmd)
{
	struct icpu_itf_msg *msg;

	if (!itf)
		return NULL;

	msg = __get_response_msg_match_lock(itf, cmd);

	return msg;
}
EXPORT_SYMBOL_GPL(get_response_msg_match_wrap);

int set_free_msg_wrap(struct icpu_itf_msg *msg)
{
	if (!itf || !msg)
		return -EINVAL;

	__set_free_msg_lock(itf, msg);

	return 0;
}
EXPORT_SYMBOL_GPL(set_free_msg_wrap);

int set_pending_msg_prio_wrap(struct icpu_itf_msg *msg)
{
	if (!itf || !msg || msg->prio >= ICPU_MSG_PRIO_MAX)
		return -EINVAL;

	__set_pending_msg_prio_lock(itf, msg);

	return 0;
}
EXPORT_SYMBOL_GPL(set_pending_msg_prio_wrap);

int set_response_msg_wrap(struct icpu_itf_msg *msg)
{
	if (!itf || !msg)
		return -EINVAL;

	__set_response_msg_lock(itf, msg);

	return 0;
}
EXPORT_SYMBOL_GPL(set_response_msg_wrap);

void print_all_message_queue(void)
{
	if (itf)
		__print_all_queues(itf, 0, 0);
}
EXPORT_SYMBOL_GPL(print_all_message_queue);

/* This get cnt functions are not thread safe. */
u32 get_free_msg_cnt_wrap(void)
{
	if (!itf)
		return 0;

	return  QUEUE_LEN(&itf->free_queue);
}
EXPORT_SYMBOL_GPL(get_free_msg_cnt_wrap);

u32 get_pending_msg_cnt_wrap(void)
{
	if (!itf)
		return 0;

	return QUEUE_LEN(&itf->pending_queue);
}
EXPORT_SYMBOL_GPL(get_pending_msg_cnt_wrap);

u32 get_response_msg_cnt_wrap(void)
{
	if (!itf)
		return 0;

	return QUEUE_LEN(&itf->response_queue);
}
EXPORT_SYMBOL_GPL(get_response_msg_cnt_wrap);

void pablo_itf_api_mode_change(enum itf_test_mode mode)
{
	if (!itf)
		return;

	if (mode == ITF_API_MODE_TEST) {
		itf->ops.open = NULL;
		itf->ops.close = NULL;
	} else {
		itf->ops.open = pablo_icpu_itf_open;
		itf->ops.close = pablo_icpu_itf_close;
	}
}
EXPORT_SYMBOL_GPL(pablo_itf_api_mode_change);

#endif

struct pablo_icpu_itf_api *pablo_icpu_itf_api_get(void)
{
	if (!itf)
		return NULL;

	return &itf->ops;
}
EXPORT_SYMBOL_GPL(pablo_icpu_itf_api_get);

struct device *pablo_itf_get_icpu_dev(void)
{
	return get_icpu_dev(get_icpu_core());
}
EXPORT_SYMBOL_GPL(pablo_itf_get_icpu_dev);

int pablo_icpu_itf_preload_firmware(unsigned long flag)
{
	int ret;

	if (!__check_current_state(ITF_STATE_INIT)) {
		ICPU_ERR("current state(%d) is not ITF_STATE_INIT", itf->state);
		return -ENODEV;
	}

	if (!flag) {
		ICPU_ERR("invalid flag(%lx)", flag);
		return -EINVAL;
	}

	ret = pablo_icpu_preload_fw(flag);
	if (ret)
		ICPU_ERR("Fail to preload firmware, ret(%d)", ret);

	return ret;
}
EXPORT_SYMBOL_GPL(pablo_icpu_itf_preload_firmware);

int pablo_icpu_itf_init(void)
{
	int ret;

	if (itf)
		return -EBUSY;

	itf = kzalloc(sizeof(struct icpu_itf), GFP_KERNEL);
	if (!itf)
		return -ENOMEM;

	__transit_state(ITF_STATE_INIT);
	ICPU_INFO("Debug INIT state, current state(%d)", itf->state);

	ret = platform_driver_register(pablo_icpu_get_platform_driver());
	if (ret) {
		ICPU_ERR("pablo_icpu_driver register failed(%d)", ret);
		kfree(itf);
		itf = NULL;
		return ret;
	}

	pablo_icpu_register_panic_handler(pablo_icpu_itf_panic_handler);

	itf->ops.open = pablo_icpu_itf_open;
	itf->ops.close = pablo_icpu_itf_close;
	itf->ops.send_message = pablo_icpu_itf_send_message_wrap;
	itf->ops.register_msg_handler = pablo_icpu_itf_register_msg_handler;
	itf->ops.register_err_handler = pablo_icpu_itf_register_err_handler;
	itf->ops.wait_boot_complete = pablo_icpu_itf_wait_boot_complete;

	itf->fsm[ITF_STATE_INIT] = (struct itf_fsm){ .pre = NULL, .action = NULL, .post = NULL, };
	itf->fsm[ITF_STATE_WAIT_FW_READY] = (struct itf_fsm){
		.pre = NULL,
		.action = __fsm_wait_fw_ready_action,
		.post = __fsm_wait_fw_ready_post,
	};
	itf->fsm[ITF_STATE_RUNNING] = (struct itf_fsm){
		.pre = NULL,
		.action = NULL,
		.post = __fsm_running_post,
	};
	itf->fsm[ITF_STATE_CLOSING] = (struct itf_fsm){ .pre = NULL, .action = NULL, .post = NULL, };

	ICPU_INFO("done");

	return 0;
}
module_init(pablo_icpu_itf_init);

void pablo_icpu_itf_exit(void)
{
	kfree(itf);
	itf = NULL;

	platform_driver_unregister(pablo_icpu_get_platform_driver());

	ICPU_INFO("done");
}
module_exit(pablo_icpu_itf_exit)

MODULE_DESCRIPTION("Exynos PABLO ICPU");
MODULE_LICENSE("GPL");
