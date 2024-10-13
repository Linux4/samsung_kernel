// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/wait.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/pcm.h>
#include <ipc/apr_tal.h>
#include <dsp/apr_audio-v2.h>
#include <dsp/q6audio-v2.h>
#include <dsp/q6voice_adaptation.h>
#include <dsp/audio_cal_utils.h>
#include <dsp/q6voice.h>
#include <dsp/q6asm-v2.h>
#include <dsp/q6common.h>

#define TIMEOUT_MS	1000

struct cvs_ctl {
	void *apr;
	atomic_t state;
	wait_queue_head_t wait;
};

struct cvp_ctl {
	void *apr;
	atomic_t state;
	wait_queue_head_t wait;
};

static struct cvs_ctl this_cvs;
static struct cvp_ctl this_cvp;
static struct common_data *common;

static int loopback_mode;
static int loopback_prev_mode;
static uint32_t echo_ref_mute_val;

/* This function must be sync up from voice_get_session_by_idx() of q6voice.c */
static struct voice_data *voice_get_session_by_idx(int idx)
{
	return ((idx < 0 || idx >= MAX_VOC_SESSIONS) ?
				NULL : &common->voice[idx]);
}

/*
 * This function must be sync up from
 * voice_itr_get_next_session() of q6voice.c
 */
static bool voice_itr_get_next_session(struct voice_session_itr *itr,
					struct voice_data **voice)
{
	bool ret = false;

	if (itr == NULL)
		return false;
	pr_debug("%s : cur idx = %d session idx = %d\n",
			 __func__, itr->cur_idx, itr->session_idx);

	if (itr->cur_idx <= itr->session_idx) {
		ret = true;
		*voice = voice_get_session_by_idx(itr->cur_idx);
		itr->cur_idx++;
	} else {
		*voice = NULL;
	}

	return ret;
}

/* This function must be sync up from voice_itr_init() of q6voice.c */
static void voice_itr_init(struct voice_session_itr *itr,
			   u32 session_id)
{
	if (itr == NULL)
		return;
	itr->session_idx = voice_get_idx_for_session(session_id);
	if (session_id == ALL_SESSION_VSID)
		itr->cur_idx = 0;
	else
		itr->cur_idx = itr->session_idx;
}

/* This function must be sync up from is_voc_state_active() of q6voice.c */
static bool is_voc_state_active(int voc_state)
{
	if ((voc_state == VOC_RUN) ||
		(voc_state == VOC_CHANGE) ||
		(voc_state == VOC_STANDBY))
		return true;

	return false;
}

/* This function must be sync up from voc_set_error_state() of q6voice.c */
static void voc_set_error_state(uint16_t reset_proc)
{
	struct voice_data *v = NULL;
	int i;

	for (i = 0; i < MAX_VOC_SESSIONS; i++) {
		v = &common->voice[i];
		if (v != NULL) {
			v->voc_state = VOC_ERROR;
			v->rec_info.recording = 0;
		}
	}
}

/*
 * This function must be sync up from
 * is_source_tracking_shared_memory_allocated() of q6voice.c
 */
static int is_source_tracking_shared_memory_allocated(void)
{
	bool ret;

	pr_debug("%s: Enter\n", __func__);

	if (common->source_tracking_sh_mem.sh_mem_block.dma_buf != NULL)
		ret = true;
	else
		ret = false;

	pr_debug("%s: Exit\n", __func__);

	return ret;
}

/* This function must be sync up from voice_get_cvs_handle() of q6voice.c */
static u16 voice_get_cvs_handle(struct voice_data *v)
{
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return 0;
	}

	pr_debug("%s: cvs_handle %d\n", __func__, v->cvs_handle);

	return v->cvs_handle;
}

static int32_t q6audio_adaptation_cvs_callback(struct apr_client_data *data,
	void *priv)
{
	int i = 0;
	uint32_t *ptr = NULL;

	if ((data == NULL) || (priv == NULL)) {
		pr_err("%s: data or priv is NULL\n", __func__);
		return -EINVAL;
	}

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: reset event = %d %d apr[%pK]\n",
			__func__,
			data->reset_event, data->reset_proc, this_cvs.apr);

		if (this_cvs.apr) {
			apr_reset(this_cvs.apr);
			atomic_set(&this_cvs.state, 0);
			this_cvs.apr = NULL;
		}

		/* Sub-system restart is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++)
			common->voice[i].cvs_handle = 0;

		cal_utils_clear_cal_block_q6maps(MAX_VOICE_CAL_TYPES,
				common->cal_data);

		/* Free the ION memory and clear handles for Source Tracking */
		if (is_source_tracking_shared_memory_allocated()) {
			msm_audio_ion_free(
				common->source_tracking_sh_mem.sh_mem_block.dma_buf);
				common->source_tracking_sh_mem.mem_handle = 0;
				common->source_tracking_sh_mem.sh_mem_block.dma_buf =
									NULL;
		}

		voc_set_error_state(data->reset_proc);
		return 0;
	}

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;
			pr_debug("%x %x\n", ptr[0], ptr[1]);
			if (ptr[1] != 0) {
				pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, ptr[0], ptr[1]);
			}

			switch (ptr[0]) {
			case VSS_ICOMMON_CMD_SET_PARAM_V2:
			case VSS_ICOMMON_CMD_SET_PARAM_V3:
				pr_info("%s: VSS_ICOMMON_CMD_SET_PARAM\n",
					__func__);
				atomic_set(&this_cvs.state, 0);
				wake_up(&this_cvs.wait);
				break;
			default:
				pr_err("%s: cmd = 0x%x\n", __func__, ptr[0]);
				break;
			}
		}
	}
	return 0;
}

static int send_packet_loopback_cmd(struct voice_data *v, int mode)
{
	struct cvs_set_loopback_enable_cmd cvs_set_loopback_cmd;
	int ret = 0;
	u16 cvs_handle;

	if (this_cvs.apr == NULL) {
		this_cvs.apr = apr_register("ADSP", "CVS",
					q6audio_adaptation_cvs_callback,
					SEC_ADAPTATAION_LOOPBACK_SRC_PORT,
					&this_cvs);
	}
	cvs_handle = voice_get_cvs_handle(v);

	/* fill in the header */
	cvs_set_loopback_cmd.hdr.hdr_field =
	APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cvs_set_loopback_cmd.hdr.pkt_size =
		APR_PKT_SIZE(APR_HDR_SIZE,
				sizeof(cvs_set_loopback_cmd) - APR_HDR_SIZE);
	cvs_set_loopback_cmd.hdr.src_port =
		SEC_ADAPTATAION_LOOPBACK_SRC_PORT;
	cvs_set_loopback_cmd.hdr.dest_port = cvs_handle;
	cvs_set_loopback_cmd.hdr.token = 0;
	cvs_set_loopback_cmd.hdr.opcode =
	    VSS_ICOMMON_CMD_SET_PARAM_V2;
	cvs_set_loopback_cmd.mem_handle = 0;
	cvs_set_loopback_cmd.mem_address_lsw = 0;
	cvs_set_loopback_cmd.mem_address_msw = 0;
	cvs_set_loopback_cmd.mem_size = 0x10;

	cvs_set_loopback_cmd.vss_set_loopback.module_id =
		VOICEPROC_MODULE_VENC;
	cvs_set_loopback_cmd.vss_set_loopback.param_id =
		VOICE_PARAM_LOOPBACK_ENABLE;
	cvs_set_loopback_cmd.vss_set_loopback.param_size = 4;
	cvs_set_loopback_cmd.vss_set_loopback.reserved = 0;
	cvs_set_loopback_cmd.vss_set_loopback.loopback_enable = mode;
	cvs_set_loopback_cmd.vss_set_loopback.reserved_field = 0;

	atomic_set(&this_cvs.state, 1);
	ret = apr_send_pkt(this_cvs.apr, (uint32_t *) &cvs_set_loopback_cmd);
	if (ret < 0) {
		pr_err("%s: sending cvs set loopback enable failed\n",
			__func__);
		goto fail;
	}
	ret = wait_event_timeout(this_cvs.wait,
		(atomic_read(&this_cvs.state) == 0),
			msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;
fail:
	return -EINVAL;
}

int voice_sec_set_loopback_cmd(u32 session_id, uint16_t mode)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	voice_itr_init(&itr, session_id);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			ret = send_packet_loopback_cmd(v, mode);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}

void voice_sec_loopback_start_cmd(u32 session_id)
{
	int ret = 0;

	if (loopback_mode > LOOPBACK_DISABLE &&
	    loopback_mode < LOOPBACK_MAX) {
		ret = voice_sec_set_loopback_cmd(session_id, loopback_mode);
		if (ret < 0) {
			pr_err("%s: send packet loopback cmd failed(%d)\n",
				__func__, ret);
		} else {
			pr_info("%s: enable packet loopback\n",
				__func__);
		}
	}
}

void voice_sec_loopback_end_cmd(u32 session_id)
{
	int ret = 0;

	if ((loopback_mode == LOOPBACK_DISABLE) &&
	    (loopback_prev_mode > LOOPBACK_DISABLE &&
	     loopback_prev_mode < LOOPBACK_MAX)) {
		ret = voice_sec_set_loopback_cmd(session_id, loopback_mode);
		if (ret < 0) {
			pr_err("%s: packet loopback disable cmd failed(%d)\n",
				__func__, ret);
		} else {
			pr_info("%s: disable packet loopback\n",
				__func__);
		}
		loopback_prev_mode = 0;
	}
}

/* This function must be sync up from voice_get_cvp_handle() of q6voice.c */
static u16 voice_get_cvp_handle(struct voice_data *v)
{
	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return 0;
	}

	pr_debug("%s: cvp_handle %d\n", __func__, v->cvp_handle);

	return v->cvp_handle;
}

static int32_t q6audio_adaptation_cvp_callback(struct apr_client_data *data,
	void *priv)
{
	int i;
	uint32_t *ptr = NULL;

	if ((data == NULL) || (priv == NULL)) {
		pr_err("%s: data or priv is NULL\n", __func__);
		return -EINVAL;
	}

	if (data->opcode == RESET_EVENTS) {
		pr_debug("%s: reset event = %d %d apr[%pK]\n",
			__func__,
			data->reset_event, data->reset_proc, this_cvp.apr);

		if (this_cvp.apr) {
			apr_reset(this_cvp.apr);
			atomic_set(&this_cvp.state, 0);
			this_cvp.apr = NULL;
		}

		/* Sub-system restart is applicable to all sessions. */
		for (i = 0; i < MAX_VOC_SESSIONS; i++)
			common->voice[i].cvp_handle = 0;

		cal_utils_clear_cal_block_q6maps(MAX_VOICE_CAL_TYPES,
				common->cal_data);

		/* Free the ION memory and clear handles for Source Tracking */
		if (is_source_tracking_shared_memory_allocated()) {
			msm_audio_ion_free(
			common->source_tracking_sh_mem.sh_mem_block.dma_buf);
			common->source_tracking_sh_mem.mem_handle = 0;
			common->source_tracking_sh_mem.sh_mem_block.dma_buf =
									NULL;
		}

		voc_set_error_state(data->reset_proc);

		return 0;
	}

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		if (data->payload_size) {
			ptr = data->payload;
			pr_debug("%x %x\n", ptr[0], ptr[1]);
			if (ptr[1] != 0) {
				pr_err("%s: cmd = 0x%x returned error = 0x%x\n",
					__func__, ptr[0], ptr[1]);
			}

			switch (ptr[0]) {
			case VSS_ICOMMON_CMD_SET_PARAM_V2:
			case VSS_ICOMMON_CMD_SET_PARAM_V3:
				pr_info("%s: VSS_ICOMMON_CMD_SET_PARAM\n",
					__func__);
				atomic_set(&this_cvp.state, 0);
				wake_up(&this_cvp.wait);
				break;
			case VSS_ICOMMON_CMD_GET_PARAM_V2:
			case VSS_ICOMMON_CMD_GET_PARAM_V3:
				pr_info("%s: VSS_ICOMMON_CMD_GET_PARAM\n",
					__func__);
				atomic_set(&this_cvp.state, 0);
				wake_up(&this_cvp.wait);
				break;
			case VSS_ICOMMON_CMD_SET_UI_PROPERTY:
			case VSS_ICOMMON_CMD_SET_UI_PROPERTY_V2:
				pr_info("%s: VSS_ICOMMON_CMD_SET_UI_PROPERTY\n",
					__func__);
				atomic_set(&this_cvp.state, 0);
				wake_up(&this_cvp.wait);
				break;
			default:
				pr_err("%s: cmd = 0x%x\n", __func__, ptr[0]);
				break;
			}
		}
	} else if (data->opcode == VSS_ICOMMON_RSP_GET_PARAM ||
		   data->opcode == VSS_ICOMMON_RSP_GET_PARAM_V3) {
		ptr = data->payload;
		pr_info("%s: VSS_ICOMMON_RSP_GET_PARAM, value: %d\n", __func__, ptr[5]);
		echo_ref_mute_val = ptr[5];
		atomic_set(&this_cvp.state, 0);
		wake_up(&this_cvp.wait);
	}
	return 0;
}

static int sec_voice_send_adaptation_sound_cmd(struct voice_data *v,
			uint16_t mode, uint16_t select, int16_t *parameters)
{
	struct cvp_adaptation_sound_parm_send_cmd
		cvp_adaptation_sound_param_cmd;
	int ret = 0;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_adaptation_sound_param_cmd.hdr.hdr_field =
				APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_adaptation_sound_param_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_adaptation_sound_param_cmd) - APR_HDR_SIZE);
	cvp_adaptation_sound_param_cmd.hdr.src_port =
		SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_adaptation_sound_param_cmd.hdr.dest_port = cvp_handle;
	cvp_adaptation_sound_param_cmd.hdr.token = 0;
	cvp_adaptation_sound_param_cmd.hdr.opcode =
		VSS_ICOMMON_CMD_SET_PARAM_V2;
	cvp_adaptation_sound_param_cmd.mem_handle = 0;
	cvp_adaptation_sound_param_cmd.mem_address_lsw = 0;
	cvp_adaptation_sound_param_cmd.mem_address_msw = 0;
	cvp_adaptation_sound_param_cmd.mem_size = 40;
	cvp_adaptation_sound_param_cmd.adaptation_sound_data.module_id =
		VOICE_VOICEMODE_MODULE;
	cvp_adaptation_sound_param_cmd.adaptation_sound_data.param_id =
		VOICE_ADAPTATION_SOUND_PARAM;
	cvp_adaptation_sound_param_cmd.adaptation_sound_data.param_size = 28;
	cvp_adaptation_sound_param_cmd.adaptation_sound_data.reserved = 0;
	cvp_adaptation_sound_param_cmd.adaptation_sound_data.eq_mode = mode;
	cvp_adaptation_sound_param_cmd.adaptation_sound_data.select = select;

	memcpy(cvp_adaptation_sound_param_cmd.adaptation_sound_data.param,
		parameters, sizeof(int16_t)*12);

	pr_info("%s: send adaptation_sound param, mode = %d, select=%d\n",
		__func__,
		cvp_adaptation_sound_param_cmd.adaptation_sound_data.eq_mode,
		cvp_adaptation_sound_param_cmd.adaptation_sound_data.select);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr,
		(uint32_t *) &cvp_adaptation_sound_param_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_adaptation_sound_param_cmd\n",
			__func__);
		return -EINVAL;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));

	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		return -EINVAL;
	}

	return 0;
}

int sec_voice_set_adaptation_sound(uint16_t mode,
	uint16_t select, int16_t *parameters)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_send_adaptation_sound_cmd(v,
					mode, select, parameters);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(sec_voice_set_adaptation_sound);

static int sec_voice_send_nb_mode_cmd(struct voice_data *v, int enable)
{
	struct cvp_set_nbmode_enable_cmd cvp_nbmode_cmd;
	int ret = 0;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_nbmode_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_nbmode_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_nbmode_cmd) - APR_HDR_SIZE);
	cvp_nbmode_cmd.hdr.src_port =
		SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_nbmode_cmd.hdr.dest_port = cvp_handle;
	cvp_nbmode_cmd.hdr.token = 0;
	cvp_nbmode_cmd.hdr.opcode =
		q6common_is_instance_id_supported() ? VSS_ICOMMON_CMD_SET_UI_PROPERTY_V2 :
				VSS_ICOMMON_CMD_SET_UI_PROPERTY;
	cvp_nbmode_cmd.cvp_set_nbmode.module_id =
		VOICE_VOICEMODE_MODULE;
	cvp_nbmode_cmd.cvp_set_nbmode.instance_id =
		INSTANCE_ID_0;
	cvp_nbmode_cmd.cvp_set_nbmode.param_id =
		VOICE_NBMODE_PARAM;
	cvp_nbmode_cmd.cvp_set_nbmode.param_size = 4;
	cvp_nbmode_cmd.cvp_set_nbmode.reserved = 0;
	cvp_nbmode_cmd.cvp_set_nbmode.enable = enable;
	cvp_nbmode_cmd.cvp_set_nbmode.reserved_field = 0;

	pr_info("%s: enable = %d\n", __func__,
					cvp_nbmode_cmd.cvp_set_nbmode.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_nbmode_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_nbmode_cmd\n",
			__func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;

fail:
	return ret;
}

int sec_voice_set_nb_mode(short enable)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_send_nb_mode_cmd(v, enable);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(sec_voice_set_nb_mode);

static int sec_voice_send_rcv_mode_cmd(struct voice_data *v, int enable)
{
	struct cvp_set_rcvmode_enable_cmd cvp_rcvmode_cmd;
	int ret = 0;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_rcvmode_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_rcvmode_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_rcvmode_cmd) - APR_HDR_SIZE);
	cvp_rcvmode_cmd.hdr.src_port = SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_rcvmode_cmd.hdr.dest_port = cvp_handle;
	cvp_rcvmode_cmd.hdr.token = 0;
	cvp_rcvmode_cmd.hdr.opcode =
		q6common_is_instance_id_supported() ? VSS_ICOMMON_CMD_SET_UI_PROPERTY_V2 :
				VSS_ICOMMON_CMD_SET_UI_PROPERTY;
	cvp_rcvmode_cmd.cvp_set_rcvmode.module_id = VOICE_VOICEMODE_MODULE;
	cvp_rcvmode_cmd.cvp_set_rcvmode.instance_id =
		INSTANCE_ID_0;
	cvp_rcvmode_cmd.cvp_set_rcvmode.param_id = VOICE_RCVMODE_PARAM;
	cvp_rcvmode_cmd.cvp_set_rcvmode.param_size = 4;
	cvp_rcvmode_cmd.cvp_set_rcvmode.reserved = 0;
	cvp_rcvmode_cmd.cvp_set_rcvmode.enable = enable;
	cvp_rcvmode_cmd.cvp_set_rcvmode.reserved_field = 0;

	pr_info("%s: Voice Module enable = %d\n", __func__,
					cvp_rcvmode_cmd.cvp_set_rcvmode.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_rcvmode_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_rcvmode_cmd\n", __func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	cvp_rcvmode_cmd.cvp_set_rcvmode.module_id = VOICE_FVSAM_MODULE;

	pr_info("%s: FVSAM Module enable = %d\n", __func__,
					cvp_rcvmode_cmd.cvp_set_rcvmode.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_rcvmode_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_rcvmode_cmd\n", __func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	cvp_rcvmode_cmd.cvp_set_rcvmode.module_id = VOICE_WISEVOICE_MODULE;

	pr_info("%s: WiseVoice Module enable = %d\n", __func__,
					cvp_rcvmode_cmd.cvp_set_rcvmode.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_rcvmode_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_rcvmode_cmd\n", __func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);

		goto fail;
	}
	return 0;

fail:
	return ret;
}

int sec_voice_set_rcv_mode(short enable)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_send_rcv_mode_cmd(v, enable);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(sec_voice_set_rcv_mode);

static int sec_voice_send_spk_mode_cmd(struct voice_data *v, int enable)
{
	struct cvp_set_spkmode_enable_cmd cvp_spkmode_cmd;
	int ret = 0;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_spkmode_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_spkmode_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_spkmode_cmd) - APR_HDR_SIZE);
	cvp_spkmode_cmd.hdr.src_port = SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_spkmode_cmd.hdr.dest_port = cvp_handle;
	cvp_spkmode_cmd.hdr.token = 0;
	cvp_spkmode_cmd.hdr.opcode =
		q6common_is_instance_id_supported() ? VSS_ICOMMON_CMD_SET_UI_PROPERTY_V2 :
				VSS_ICOMMON_CMD_SET_UI_PROPERTY;
	cvp_spkmode_cmd.cvp_set_spkmode.module_id = VOICE_VOICEMODE_MODULE;
	cvp_spkmode_cmd.cvp_set_spkmode.instance_id =
		INSTANCE_ID_0;
	cvp_spkmode_cmd.cvp_set_spkmode.param_id = VOICE_SPKMODE_PARAM;
	cvp_spkmode_cmd.cvp_set_spkmode.param_size = 4;
	cvp_spkmode_cmd.cvp_set_spkmode.reserved = 0;
	cvp_spkmode_cmd.cvp_set_spkmode.enable = enable;
	cvp_spkmode_cmd.cvp_set_spkmode.reserved_field = 0;

	pr_info("%s: Voice Module enable = %d\n", __func__,
					cvp_spkmode_cmd.cvp_set_spkmode.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_spkmode_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_spkmode_cmd\n", __func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	cvp_spkmode_cmd.cvp_set_spkmode.module_id = VOICE_FVSAM_MODULE;

	pr_info("%s: FVSAM Module enable = %d\n", __func__,
					cvp_spkmode_cmd.cvp_set_spkmode.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_spkmode_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_spkmode_cmd\n", __func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	cvp_spkmode_cmd.cvp_set_spkmode.module_id = VOICE_WISEVOICE_MODULE;

	pr_info("%s: WiseVoice Module enable = %d\n", __func__,
					cvp_spkmode_cmd.cvp_set_spkmode.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_spkmode_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_spkmode_cmd\n", __func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);

		goto fail;
	}
	return 0;

fail:
	return ret;
}

int sec_voice_set_spk_mode(short enable)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_send_spk_mode_cmd(v, enable);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(sec_voice_set_spk_mode);

static int sec_voice_send_device_info_cmd(struct voice_data *v, int device)
{
	struct cvp_set_device_info_cmd cvp_device_info_cmd;
	int ret = 0;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_device_info_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_device_info_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_device_info_cmd) - APR_HDR_SIZE);
	cvp_device_info_cmd.hdr.src_port = SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_device_info_cmd.hdr.dest_port = cvp_handle;
	cvp_device_info_cmd.hdr.token = 0;
	cvp_device_info_cmd.hdr.opcode =
		q6common_is_instance_id_supported() ? VSS_ICOMMON_CMD_SET_UI_PROPERTY_V2 :
				VSS_ICOMMON_CMD_SET_UI_PROPERTY;
	cvp_device_info_cmd.cvp_set_device_info.module_id =
		VOICE_MODULE_SET_DEVICE;
	cvp_device_info_cmd.cvp_set_device_info.instance_id =
		INSTANCE_ID_0;
	cvp_device_info_cmd.cvp_set_device_info.param_id =
		VOICE_MODULE_SET_DEVICE_PARAM;
	cvp_device_info_cmd.cvp_set_device_info.param_size = 4;
	cvp_device_info_cmd.cvp_set_device_info.reserved = 0;
	cvp_device_info_cmd.cvp_set_device_info.enable = device;
	cvp_device_info_cmd.cvp_set_device_info.reserved_field = 0;

	pr_info("%s: voice device info = %d\n",
		__func__, cvp_device_info_cmd.cvp_set_device_info.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_device_info_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_set_device_info_cmd\n",
			__func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;

fail:
	return ret;
}

int sec_voice_set_device_info(short device)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_send_device_info_cmd(v, device);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(sec_voice_set_device_info);

static int sec_voice_send_ref_lch_mute_cmd(struct voice_data *v, int enable)
{
	struct cvp_set_ref_lch_mute_enable_cmd cvp_ref_lch_mute_cmd;
	int ret = 0;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_ref_lch_mute_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_ref_lch_mute_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_ref_lch_mute_cmd) - APR_HDR_SIZE);
	cvp_ref_lch_mute_cmd.hdr.src_port = SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_ref_lch_mute_cmd.hdr.dest_port = cvp_handle;
	cvp_ref_lch_mute_cmd.hdr.token = 0;
	cvp_ref_lch_mute_cmd.hdr.opcode =
		q6common_is_instance_id_supported() ? VSS_ICOMMON_CMD_SET_UI_PROPERTY_V2 :
				VSS_ICOMMON_CMD_SET_UI_PROPERTY;
	cvp_ref_lch_mute_cmd.cvp_set_ref_lch_mute.module_id = VOICE_MODULE_LVVEFQ_TX;
	cvp_ref_lch_mute_cmd.cvp_set_ref_lch_mute.instance_id = 0x8000;
	cvp_ref_lch_mute_cmd.cvp_set_ref_lch_mute.param_id = VOICE_ECHO_REF_LCH_MUTE_PARAM;
	cvp_ref_lch_mute_cmd.cvp_set_ref_lch_mute.param_size = 4;
	cvp_ref_lch_mute_cmd.cvp_set_ref_lch_mute.reserved = 0;
	cvp_ref_lch_mute_cmd.cvp_set_ref_lch_mute.enable = enable;
	cvp_ref_lch_mute_cmd.cvp_set_ref_lch_mute.reserved_field = 0;

	pr_info("%s: lvvefq module enable(%d)\n", __func__,
					cvp_ref_lch_mute_cmd.cvp_set_ref_lch_mute.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_ref_lch_mute_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_ref_lch_mute_cmd\n",
			__func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	cvp_ref_lch_mute_cmd.cvp_set_ref_lch_mute.module_id = TX_VOICE_SOLOMONVOICE;

	pr_info("%s: solomon module enable(%d)\n", __func__,
					cvp_ref_lch_mute_cmd.cvp_set_ref_lch_mute.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_ref_lch_mute_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_ref_lch_mute_cmd\n",
			__func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;

fail:
	return ret;
}

int sec_voice_ref_lch_mute(short enable)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_send_ref_lch_mute_cmd(v, enable);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(sec_voice_ref_lch_mute);

static int sec_voice_request_echo_ref_mute_cmd(struct voice_data *v)
{
	struct cvp_get_echo_ref_mute_cmd cvp_echo_ref_mute_cmd;
	int ret = 0;
	u16 cvp_handle;

	pr_info("%s: Enter\n", __func__);

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	cvp_echo_ref_mute_cmd.cvp_get_echo_ref_mute.mem_handle = 0;
	cvp_echo_ref_mute_cmd.cvp_get_echo_ref_mute.mem_address = 0;
	cvp_echo_ref_mute_cmd.cvp_get_echo_ref_mute.mem_size = sizeof(struct vss_icommon_cmd_get_param_v3_t) + 2;
	cvp_echo_ref_mute_cmd.cvp_get_echo_ref_mute.module_id = VOICE_MODULE_ECHO_REF_MUTE;
	cvp_echo_ref_mute_cmd.cvp_get_echo_ref_mute.instance_id = 0x8000;
	cvp_echo_ref_mute_cmd.cvp_get_echo_ref_mute.reserved = 0;
	cvp_echo_ref_mute_cmd.cvp_get_echo_ref_mute.param_id = VOICE_MODULE_ECHO_REF_MUTE_PARAM;

	/* fill in the header */
	cvp_echo_ref_mute_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_echo_ref_mute_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_echo_ref_mute_cmd) - APR_HDR_SIZE);
	cvp_echo_ref_mute_cmd.hdr.src_port = SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_echo_ref_mute_cmd.hdr.dest_port = cvp_handle;
	cvp_echo_ref_mute_cmd.hdr.token = 0;
	cvp_echo_ref_mute_cmd.hdr.opcode =
		q6common_is_instance_id_supported() ? VSS_ICOMMON_CMD_GET_PARAM_V3 :
				VSS_ICOMMON_CMD_GET_PARAM_V2;

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_echo_ref_mute_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_set_echo_ref_mute_cmd\n",
			__func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}

	return 0;

fail:
	return ret;
}

int sec_voice_request_echo_ref_mute(void)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_request_echo_ref_mute_cmd(v);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	pr_info("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(sec_voice_request_echo_ref_mute);

int sec_voice_get_echo_ref_mute(void)
{
	return echo_ref_mute_val;
}
EXPORT_SYMBOL(sec_voice_get_echo_ref_mute);

static int sec_voice_send_aec_effect_cmd(struct voice_data *v, int enable)
{
	struct cvp_set_aec_effect_cmd cvp_aec_effect_cmd;
	uint32_t topology = VOICE_TX_SOLOMONVOICE_SM;
	uint32_t module_id = TX_VOICE_SOLOMONVOICE;
	u16 cvp_handle;
	int ret = 0;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}

	topology = voice_get_topology(CVP_VOC_TX_TOPOLOGY_CAL);

	switch (topology) {
	case VPM_TX_SM_LVVEFQ_COPP_TOPOLOGY:
	case VPM_TX_DM_LVVEFQ_COPP_TOPOLOGY:
	case VPM_TX_QM_LVVEFQ_COPP_TOPOLOGY:
	case VPM_TX_SM_LVSAFQ_COPP_TOPOLOGY:
	case VPM_TX_DM_LVSAFQ_COPP_TOPOLOGY:
		module_id = VOICE_MODULE_LVVEFQ_TX;
		break;
	case VOICE_TX_DIAMONDVOICE_FVSAM_SM:
	case VOICE_TX_DIAMONDVOICE_FVSAM_DM:
	case VOICE_TX_DIAMONDVOICE_FVSAM_QM:
	case VOICE_TX_DIAMONDVOICE_FRSAM_DM:
		module_id = VOICE_FVSAM_MODULE;
		break;
	case VOICE_TX_SOLOMONVOICE_SM:
	case VOICE_TX_SOLOMONVOICE_DM:
	case VOICE_TX_SOLOMONVOICE_QM:
		module_id = TX_VOICE_SOLOMONVOICE;
		break;
	default:
		pr_err("%s: undefined topology(0x%x)\n",
			__func__, topology);
		break;
	}

	cvp_handle = voice_get_cvp_handle(v);
	/* fill in the header */
	cvp_aec_effect_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_aec_effect_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_aec_effect_cmd) - APR_HDR_SIZE);
	cvp_aec_effect_cmd.hdr.src_port = SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_aec_effect_cmd.hdr.dest_port = cvp_handle;
	cvp_aec_effect_cmd.hdr.token = 0;
	cvp_aec_effect_cmd.hdr.opcode =
		q6common_is_instance_id_supported() ? VSS_ICOMMON_CMD_SET_UI_PROPERTY_V2 :
				VSS_ICOMMON_CMD_SET_UI_PROPERTY;
	cvp_aec_effect_cmd.cvp_set_aec_effect.module_id = module_id;
	cvp_aec_effect_cmd.cvp_set_aec_effect.instance_id = 0x8000;
	cvp_aec_effect_cmd.cvp_set_aec_effect.param_id = VOICE_NREC_MODE_DYNAMIC_PARAM;
	cvp_aec_effect_cmd.cvp_set_aec_effect.param_size = 4;
	cvp_aec_effect_cmd.cvp_set_aec_effect.reserved = 0;
	cvp_aec_effect_cmd.cvp_set_aec_effect.enable = enable;
	cvp_aec_effect_cmd.cvp_set_aec_effect.reserved_field = 0;

	pr_info("%s: module=0x%x, enable(%d)\n", __func__,
					module_id,
					cvp_aec_effect_cmd.cvp_set_aec_effect.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_aec_effect_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_aec_effect_cmd\n",
			__func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;

fail:
	return ret;
}

int sec_voice_aec_effect(short enable)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_send_aec_effect_cmd(v, enable);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(sec_voice_aec_effect);

static int sec_voice_voice_isolation_cmd(struct voice_data *v, int mode)
{
	struct cvp_set_voice_isolation_cmd cvp_voice_isolation_cmd;
	int ret = 0;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_voice_isolation_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_voice_isolation_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_voice_isolation_cmd) - APR_HDR_SIZE);
	cvp_voice_isolation_cmd.hdr.src_port = SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_voice_isolation_cmd.hdr.dest_port = cvp_handle;
	cvp_voice_isolation_cmd.hdr.token = 0;
	cvp_voice_isolation_cmd.hdr.opcode =
		q6common_is_instance_id_supported() ? VSS_ICOMMON_CMD_SET_UI_PROPERTY_V2 :
				VSS_ICOMMON_CMD_SET_UI_PROPERTY;
	cvp_voice_isolation_cmd.cvp_set_voice_isolation.module_id =
		VOICE_MODULE_LVVEFQ_TX;
	cvp_voice_isolation_cmd.cvp_set_voice_isolation.instance_id =
		0x8000;
	cvp_voice_isolation_cmd.cvp_set_voice_isolation.param_id =
		ENHANCED_VT_CALL_DYNAMIC_PARAM;
	cvp_voice_isolation_cmd.cvp_set_voice_isolation.param_size = 4;
	cvp_voice_isolation_cmd.cvp_set_voice_isolation.reserved = 0;
	cvp_voice_isolation_cmd.cvp_set_voice_isolation.enable = mode;
	cvp_voice_isolation_cmd.cvp_set_voice_isolation.reserved_field = 0;

	pr_info("%s: voice isolation info = %d\n",
		__func__, cvp_voice_isolation_cmd.cvp_set_voice_isolation.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_voice_isolation_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_voice_isolation_cmd\n",
			__func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;

fail:
	return ret;
}

int sec_voice_isolation_mode(short mode)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: mode is %d\n", __func__, mode);

	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_voice_isolation_cmd(v, mode);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(sec_voice_isolation_mode);

static int sec_voice_send_bt_rvc_vol_cmd(struct voice_data *v, int vol)
{
	struct cvp_set_bt_rvc_vol_cmd cvp_bt_rvc_vol_cmd;
	int ret = 0;
	u16 cvp_handle;

	if (v == NULL) {
		pr_err("%s: v is NULL\n", __func__);
		return -EINVAL;
	}

	if (this_cvp.apr == NULL) {
		this_cvp.apr = apr_register("ADSP", "CVP",
					q6audio_adaptation_cvp_callback,
					SEC_ADAPTATAION_VOICE_SRC_PORT,
					&this_cvp);
	}
	cvp_handle = voice_get_cvp_handle(v);

	/* fill in the header */
	cvp_bt_rvc_vol_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE),
				APR_PKT_VER);
	cvp_bt_rvc_vol_cmd.hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE,
		sizeof(cvp_bt_rvc_vol_cmd) - APR_HDR_SIZE);
	cvp_bt_rvc_vol_cmd.hdr.src_port = SEC_ADAPTATAION_VOICE_SRC_PORT;
	cvp_bt_rvc_vol_cmd.hdr.dest_port = cvp_handle;
	cvp_bt_rvc_vol_cmd.hdr.token = 0;
	cvp_bt_rvc_vol_cmd.hdr.opcode =
		q6common_is_instance_id_supported() ? VSS_ICOMMON_CMD_SET_UI_PROPERTY_V2 :
				VSS_ICOMMON_CMD_SET_UI_PROPERTY;
	cvp_bt_rvc_vol_cmd.cvp_set_bt_rvc_vol.module_id = VOICE_VOICEMODE_MODULE;
	cvp_bt_rvc_vol_cmd.cvp_set_bt_rvc_vol.instance_id =
		INSTANCE_ID_0;
	cvp_bt_rvc_vol_cmd.cvp_set_bt_rvc_vol.param_id = VOICE_BT_RVC_VOL_PARAM;
	cvp_bt_rvc_vol_cmd.cvp_set_bt_rvc_vol.param_size = 4;
	cvp_bt_rvc_vol_cmd.cvp_set_bt_rvc_vol.reserved = 0;
	cvp_bt_rvc_vol_cmd.cvp_set_bt_rvc_vol.enable = vol;
	cvp_bt_rvc_vol_cmd.cvp_set_bt_rvc_vol.reserved_field = 0;

	pr_info("%s: volume index = %d\n", __func__,
					cvp_bt_rvc_vol_cmd.cvp_set_bt_rvc_vol.enable);

	atomic_set(&this_cvp.state, 1);
	ret = apr_send_pkt(this_cvp.apr, (uint32_t *) &cvp_bt_rvc_vol_cmd);
	if (ret < 0) {
		pr_err("%s: Failed to send cvp_bt_rvc_vol_cmd\n", __func__);
		goto fail;
	}

	ret = wait_event_timeout(this_cvp.wait,
				(atomic_read(&this_cvp.state) == 0),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		goto fail;
	}
	return 0;

fail:
	return ret;
}

int sec_voice_set_bt_rvc_vol(short vol)
{
	struct voice_data *v = NULL;
	int ret = 0;
	struct voice_session_itr itr;

	pr_debug("%s: Enter\n", __func__);

	voice_itr_init(&itr, ALL_SESSION_VSID);
	while (voice_itr_get_next_session(&itr, &v)) {
		if (v != NULL) {
			mutex_lock(&v->lock);
			if (is_voc_state_active(v->voc_state) &&
				(v->lch_mode != VOICE_LCH_START) &&
				!v->disable_topology)
				ret = sec_voice_send_bt_rvc_vol_cmd(v, vol);
			mutex_unlock(&v->lock);
		} else {
			pr_err("%s: invalid session\n", __func__);
			ret = -EINVAL;
			break;
		}
	}
	pr_debug("%s: Exit, ret=%d\n", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(sec_voice_set_bt_rvc_vol);

int sec_voice_get_loopback_enable(void)
{
	return loopback_mode;
}
EXPORT_SYMBOL(sec_voice_get_loopback_enable);

void sec_voice_set_loopback_enable(int mode)
{
	loopback_prev_mode = loopback_mode;

	if (mode >= LOOPBACK_MAX ||
	    mode < LOOPBACK_DISABLE) {
		pr_err("%s : out of range, mode = %d\n",
			__func__, mode);
		loopback_mode = LOOPBACK_DISABLE;
	} else {
		loopback_mode = mode;
	}

	pr_info("%s : prev_mode = %d, mode = %d\n",
		__func__,
		loopback_prev_mode,
		loopback_mode);
}
EXPORT_SYMBOL(sec_voice_set_loopback_enable);

int __init voice_adaptation_init(void)
{
	int rc = 0;

	pr_info("%s\n", __func__);

	common = voice_get_common_data();
	if (common == NULL) {
		pr_err("%s: common is NULL\n", __func__);

		rc = -EINVAL;
		goto done;
	}

	atomic_set(&this_cvs.state, 0);
	atomic_set(&this_cvp.state, 0);
	this_cvs.apr = NULL;
	this_cvp.apr = NULL;
	init_waitqueue_head(&this_cvs.wait);
	init_waitqueue_head(&this_cvp.wait);

done:
	return rc;
}

void voice_adaptation_exit(void)
{
	pr_info("%s\n", __func__);
}

MODULE_DESCRIPTION("Samsung Voice Adaptation DSP driver");
MODULE_LICENSE("GPL v2");
