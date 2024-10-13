/*
 * Samsung Exynos SoC series NPU driver
 *
 * Copyright (c) 2017 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/atomic.h>
#include <linux/mutex.h>

#define NPU_LOG_TAG	"if-protodrv-mbox2"

#include "npu-if-protodrv-mbox2.h"
#include "npu-profile-v2.h"
#include "npu-log.h"
#include "interface/hardware/npu-interface.h"
#include "npu-hw-device.h"
#include "dsp-dhcp.h"
#if IS_ENABLED(CONFIG_NPU_USE_FENCE_SYNC)
#include "npu-util-common.h"
#include "npu-util-regs.h"
#endif

extern struct npu_proto_drv *protodr;

const struct npu_if_protodrv_mbox protodrv_mbox = {
	.dev = NULL,
	.npu_if_protodrv_mbox_ops = &protodrv_mbox_ops,
};

int npu_mbox_op_register_notifier(protodrv_notifier sig_func)
{
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->register_notifier))
		return protodrv_mbox.npu_if_protodrv_mbox_ops->register_notifier(sig_func);

	npu_warn("not defined: register_notifier()\n");
	return 0;
}

/* nw_mbox_ops -> Use npu-if-protodrv-mbox object stored in npu_proto_drv */
int npu_nw_mbox_op_is_available(void)
{
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->nw_result_available))
		return protodrv_mbox.npu_if_protodrv_mbox_ops->nw_result_available();

	npu_warn("not defined: nw_result_available()\n");
	return 0;
}
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
int npu_fw_message_get(struct fw_message *fw_msg)
{
	int ret = 0;

	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->fw_msg_get))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->fw_msg_get(fw_msg);
	return ret;
}

int npu_fw_res_put(struct fw_message *fw_msg)
{
	int ret = 0;

	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->fw_res_put))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->fw_res_put(fw_msg);
	return ret;
}
#endif
int npu_nw_mbox_ops_get(struct msgid_pool *pool, struct proto_req_nw **target)
{
	int msgid = 0;
	struct npu_nw nw;
	int ret = 0;

	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->nw_get_result))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->nw_get_result(&msgid, &nw);
	else {
		npu_warn("not defined: nw_get_result()\n");
		*target = NULL;
		return 0;
	}

	if (unlikely(ret <= 0)) {
		/* No message */
		*target = NULL;
	} else {
		/* Message available */
		*target = msgid_claim_get_ref(pool, msgid, PROTO_DRV_REQ_TYPE_NW);

		if (likely(*target != NULL)) {
			npu_uinfo("mbox->protodrv : NW msgid(%d)\n", &(*target)->nw, msgid);
			(*target)->nw.msgid = -1;
			(*target)->nw.result_code = nw.result_code;
			(*target)->nw.result_value = nw.result_value;
			ret = 1;
		} else {
			npu_err("failed to find request mapped with msgid[%d]\n", msgid);
		}
	}

	return ret;
}

int npu_nw_mbox_ops_put(struct msgid_pool *pool, struct proto_req_nw *src)
{
	int msgid = 0;
	int ret = 0;

	msgid = msgid_issue_save_ref(pool, PROTO_DRV_REQ_TYPE_NW, src, src->nw.session);
	/* intentional msg id setting for global power down */

	src->nw.msgid = msgid;
	npu_uinfo("protodrv-> : NW msgid(%d)\n", &src->nw, msgid);
	if (unlikely(msgid < 0)) {
		npu_uwarn("no message ID available. Posting message is not temporally available.\n", &src->nw);
		return 0;
	}

	/* Generate mailbox message with given msgid and post it */
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->nw_post_request))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->nw_post_request(msgid, &src->nw);
	else {
		npu_warn("not defined: nw_post_request()\n");
		return 0;
	}

	if (unlikely(ret <= 0)) {
		/* Push failed -> return msgid */
		npu_utrace("nw_post_request failed. Reclaiming msgid(%d)\n", &src->nw, msgid);
		msgid_claim(pool, msgid);
	}
	return ret;
}
#if IS_ENABLED(CONFIG_NPU_USE_IMB_ALLOCATOR_V2)
int npu_fwmsg_mbox_op_is_available(void)
{
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->fwmsg_available))
		return protodrv_mbox.npu_if_protodrv_mbox_ops->fwmsg_available();

	npu_warn("not defined: fwmsg_available()\n");
	return 0;
}
#endif

/* frame_mbox_ops -> Use npu-if-protodrv-mbox object stored in npu_proto_drv */
int npu_frame_mbox_op_is_available(void)
{
	enum channel_flag c = MEDIUM_CHANNEL;

	if (atomic_read(&protodr->high_prio_frame_count) > 0)
		c = HIGH_CHANNEL;
	else if (atomic_read(&protodr->normal_prio_frame_count) > 0)
		c = MEDIUM_CHANNEL;

	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->frame_result_available))
		return protodrv_mbox.npu_if_protodrv_mbox_ops->frame_result_available(c);

	npu_warn("not defined: frame_result_available()\n");
	return 0;
}
int npu_frame_mbox_ops_get(struct msgid_pool *pool, struct proto_req_frame **target)
{
	int msgid = 0;
	struct npu_frame frame;
	int ret = 0;
	enum channel_flag c = MEDIUM_CHANNEL;

	if (atomic_read(&protodr->high_prio_frame_count) > 0)
		c = HIGH_CHANNEL;
	else if (atomic_read(&protodr->normal_prio_frame_count) > 0)
		c = MEDIUM_CHANNEL;

	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->frame_get_result))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->frame_get_result(&msgid, &frame, c);
	else {
		npu_warn("not defined: frame_get_result()\n");
		*target = NULL;
		return 0;
	}

	if (unlikely(ret <= 0)) {
		/* No message */
		*target = NULL;
	} else {
		*target = msgid_claim_get_ref(pool, msgid, PROTO_DRV_REQ_TYPE_FRAME);

		if (likely(*target != NULL)) {
			if (c == HIGH_CHANNEL)
				atomic_dec(&protodr->high_prio_frame_count);
			else if (c == MEDIUM_CHANNEL)
				atomic_dec(&protodr->normal_prio_frame_count);
			(*target)->frame.msgid = -1;
			(*target)->frame.result_code = frame.result_code;
#if IS_ENABLED(CONFIG_SOC_S5E9945)
			if ((*target)->frame.frame_id == 1) {
				struct npu_session *session = (*target)->frame.session;
				npu_uinfo("target frame_id : %u, frame.ncp_type : %u\n", session, (*target)->frame.frame_id, frame.ncp_type);
				if (frame.ncp_type != 0xC0DECAFE)
					npu_precision_hash_update(session, frame.ncp_type);
			}
#endif
			npu_profile_record_end(PROFILE_DD_FW_IF, (*target)->frame.input->index, ktime_to_us(ktime_get_boottime()), 2, (*target)->frame.uid);
			ret = 1;
		} else {
			npu_err("failed to find request mapped with msgid[%d]\n", msgid);
		}
	}

	return ret;
}

int npu_kpi_frame_mbox_ops_get(struct msgid_pool *pool, struct npu_frame **target)
{
	int msgid = 0;
	struct npu_frame frame;
	int ret = 0;
	enum channel_flag c = HIGH_CHANNEL;

	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->frame_get_result))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->frame_get_result(&msgid, &frame, c);
	else {
		npu_warn("not defined: frame_get_result()\n");
		*target = NULL;
		return 0;
	}

	if (unlikely(ret <= 0)) {
		/* No message */
		*target = NULL;
	} else {
		*target = msgid_claim_get_ref(pool, msgid, PROTO_DRV_REQ_TYPE_FRAME);

		if (likely(*target != NULL)) {
			(*target)->msgid = -1;
			(*target)->result_code = frame.result_code;
			ret = 1;
		} else {
			npu_err("failed to find request mapped with msgid[%d]\n", msgid);
		}
	}

	return ret;
}

int npu_frame_mbox_ops_put(struct msgid_pool *pool, struct proto_req_frame *src)
{
	int msgid = 0;
	int ret;

	msgid = msgid_issue_save_ref(pool, PROTO_DRV_REQ_TYPE_FRAME, src, src->frame.session);
	src->frame.msgid = msgid;
	if (unlikely(msgid < 0)) {
		npu_ufwarn("no message ID available. Posting message is not temporally available.\n", &src->frame);
		return 0;
	}
	npu_dbg("put uid %d, msgid %d, frame id %d, index %d, frame 0x%p\n",
			src->frame.uid, msgid, src->frame.frame_id, src->frame.output->index, (void *)&src->frame);

	{
		struct npu_session *session = src->frame.session;
		struct npu_vertex_ctx *vctx = &session->vctx;

		if (vctx->state & BIT(NPU_VERTEX_STREAMOFF)) {
			npu_ufwarn("Already session is streamoff.\n", &src->frame);
			return 0;
		}
	}

	/* Generate mailbox message with given msgid and post it */
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->frame_post_request))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->frame_post_request(msgid, &src->frame);
	else {
		npu_warn("not defined: frame_post_request()\n");
		return 0;
	}

	if (unlikely(ret <= 0)) {
		/* Push failed -> return msgid */
		npu_uftrace("frame_post_request failed. Reclaiming msgid(%d)\n", &src->frame, msgid);
		msgid_claim(pool, msgid);
	}
	return ret;
}

int npu_kpi_frame_mbox_put(struct msgid_pool *pool, struct npu_frame *frame)
{
	int msgid = 0;
	int ret;

	msgid = msgid_issue_save_ref(pool, PROTO_DRV_REQ_TYPE_FRAME, frame, frame->session);
	frame->msgid = msgid;
	if (unlikely(msgid < 0)) {
		npu_ufwarn("no message ID available. Posting message is not temporally available.\n", frame);
		return 0;
	}
	npu_dbg("put kpi uid %d, msgid %d, frame id %d, index %d, frame 0x%p\n",
			frame->uid, msgid, frame->frame_id, frame->output->index, frame);

	/* Generate mailbox message with given msgid and post it */
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->frame_post_request))
		ret = protodrv_mbox.npu_if_protodrv_mbox_ops->frame_post_request(msgid, frame);
	else {
		npu_warn("not defined: frame_post_request()\n");
		return 0;
	}

	if (unlikely(ret <= 0)) {
		/* Push failed -> return msgid */
		npu_uftrace("frame_post_request failed. Reclaiming msgid(%d)\n", frame, msgid);
		msgid_claim(pool, msgid);
	}
	return ret;
}

int npu_kpi_response_read(void)
{
	int ret = 0;
	enum channel_flag c = HIGH_CHANNEL;
	struct npu_frame *frame = NULL;

	if ((protodrv_mbox.npu_if_protodrv_mbox_ops->frame_result_available) &&
			(protodrv_mbox.npu_if_protodrv_mbox_ops->frame_result_available(c) > 0)) {
		ret = npu_kpi_frame_mbox_ops_get(&protodr->msgid_pool, &frame);
		if (unlikely(ret <= 0)) {
			npu_err("No frames received %d\n", ret);
			return ret;
		}
		npu_buffer_q_notify_done(frame);
	}
	return 0;
}

int npu_mbox_op_register_msgid_type_getter(int (*msgid_get_type_func)(int))
{
	if (likely(protodrv_mbox.npu_if_protodrv_mbox_ops->register_msgid_type_getter))
		return protodrv_mbox.npu_if_protodrv_mbox_ops->register_msgid_type_getter(msgid_get_type_func);

	npu_warn("not defined: register_msgid_type_getter()\n");
	return 0;
}
