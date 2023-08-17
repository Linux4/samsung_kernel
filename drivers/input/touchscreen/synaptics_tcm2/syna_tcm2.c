// SPDX-License-Identifier: GPL-2.0
/*
 * Synaptics TouchCom touchscreen driver
 *
 * Copyright (C) 2017-2020 Synaptics Incorporated. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * INFORMATION CONTAINED IN THIS DOCUMENT IS PROVIDED "AS-IS," AND SYNAPTICS
 * EXPRESSLY DISCLAIMS ALL EXPRESS AND IMPLIED WARRANTIES, INCLUDING ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE,
 * AND ANY WARRANTIES OF NON-INFRINGEMENT OF ANY INTELLECTUAL PROPERTY RIGHTS.
 * IN NO EVENT SHALL SYNAPTICS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, PUNITIVE, OR CONSEQUENTIAL DAMAGES ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OF THE INFORMATION CONTAINED IN THIS DOCUMENT, HOWEVER CAUSED
 * AND BASED ON ANY THEORY OF LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, AND EVEN IF SYNAPTICS WAS ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE. IF A TRIBUNAL OF COMPETENT JURISDICTION DOES
 * NOT PERMIT THE DISCLAIMER OF DIRECT DAMAGES OR ANY OTHER DAMAGES, SYNAPTICS'
 * TOTAL CUMULATIVE LIABILITY TO ANY PARTY SHALL NOT EXCEED ONE HUNDRED U.S.
 * DOLLARS.
 */

/**
 * @file syna_tcm2.c
 *
 * This file implements the Synaptics device driver running under Linux kernel
 * input device subsystem, and also communicate with Synaptics touch controller
 * through TouchComm command-response protocol.
 */

#include "syna_tcm2.h"
#include "syna_tcm2_platform.h"
#include "synaptics_touchcom_core_dev.h"
#include "synaptics_touchcom_func_base.h"
#include "synaptics_touchcom_func_touch.h"
#ifdef STARTUP_REFLASH
#include "synaptics_touchcom_func_reflash.h"
#endif
#ifdef MULTICHIP_DUT_REFLASH
#include "synaptics_touchcom_func_romboot.h"
#endif

/**
 * @section: USE_CUSTOM_TOUCH_REPORT_CONFIG
 *           Open if willing to set up the format of touch report.
 *           The custom_touch_format[] array can be used to describe the
 *           customized report format.
 */
#ifdef USE_CUSTOM_TOUCH_REPORT_CONFIG
static unsigned char custom_touch_format[] = {
	/* entity code */                    /* bits */
#ifdef ENABLE_WAKEUP_GESTURE
	TOUCH_REPORT_GESTURE_ID,                8,
#endif
	TOUCH_REPORT_NUM_OF_ACTIVE_OBJECTS,     8,
	TOUCH_REPORT_FOREACH_ACTIVE_OBJECT,
	TOUCH_REPORT_OBJECT_N_INDEX,            8,
	TOUCH_REPORT_OBJECT_N_CLASSIFICATION,   8,
	TOUCH_REPORT_OBJECT_N_X_POSITION,       16,
	TOUCH_REPORT_OBJECT_N_Y_POSITION,       16,
	TOUCH_REPORT_FOREACH_END,
	TOUCH_REPORT_END
};
#endif

/**
 * @section: STARTUP_REFLASH_DELAY_TIME_MS
 *           The delayed time to start fw update during the startup time.
 *           This configuration depends on STARTUP_REFLASH.
 */
#ifdef STARTUP_REFLASH
#define STARTUP_REFLASH_DELAY_TIME_MS (200)

#define FW_IMAGE_NAME "synaptics/firmware.img"
#endif

/**
 * @section: RESET_ON_RESUME_DELAY_MS
 *           The delayed time to issue a reset on resume state.
 *           This configuration depends on RESET_ON_RESUME.
 */
#ifdef RESET_ON_RESUME
#define RESET_ON_RESUME_DELAY_MS (100)
#endif

/**
 * @section: VERIFY_SEC_REPORTS
 *           indicate the code segmant to verify the SEC reports
 */
#define VERIFY_SEC_REPORTS

/**
 * @section: POWER_ALIVE_AT_SUSPEND
 *           indicate that the power is still alive even at
 *           system suspend.
 */
/* #define POWER_ALIVE_AT_SUSPEND */

/**
 * @section: global variables for an active drm panel
 *           in order to register display notifier
 */
#ifdef USE_DRM_PANEL_NOTIFIER
	struct drm_panel *active_panel;
#endif


/**
 * syna_dev_read_from_sponge()
 *
 * Helper to retrieve raw data from sponge public area
 *
 * @param
 *    [ in] tcm:      tcm driver handle
 *    [ in] rd_data:  buffer to store the data read in
 *    [ in] size_buf: size of given buffer
 *    [ in] offset:   offset to read
 *    [ in] rd_len:   size to read
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_read_from_sponge(struct syna_tcm *tcm,
		unsigned char *rd_buf, int size_buf,
		unsigned short offset, unsigned int rd_len)
{
	int retval = 0;
	struct tcm_dev *tcm_dev = tcm->tcm_dev;
	unsigned char payload[4] = {0};
	unsigned char resp_code;
	struct tcm_buffer resp;

	if (tcm_dev->dev_mode != MODE_APPLICATION_FIRMWARE) {
		LOGN("Application firmware not running, current mode: %02x\n",
			tcm_dev->dev_mode);
		return -EINVAL;
	}

	if ((!rd_buf) || (size_buf == 0) || (size_buf < rd_len)) {
		LOGE("Invalid buffer size\n");
		return -EINVAL;
	}

	if (rd_len == 0)
		return 0;

	syna_tcm_buf_init(&resp);

	payload[0] = (unsigned char)(offset & 0xFF);
	payload[1] = (unsigned char)((offset >> 8) & 0xFF);
	payload[2] = (unsigned char)(rd_len & 0xFF);
	payload[3] = (unsigned char)((rd_len >> 8) & 0xFF);

	retval = syna_tcm_send_command(tcm_dev,
			CMD_READ_SEC_SPONGE_REG,
			payload,
			sizeof(payload),
			&resp_code,
			&resp,
			0);
	if (retval < 0) {
		LOGE("Fail to read from sponge, status:%x (offset:%x len:%d)\n",
			resp_code, offset, rd_len);
		goto exit;
	}

	if (resp.data_length != rd_len) {
		LOGE("Invalid data length\n");
		retval = -EINVAL;
		goto exit;
	}

	retval = syna_pal_mem_cpy(rd_buf,
			size_buf,
			resp.buf,
			resp.buf_size,
			rd_len);
	if (retval < 0) {
		LOGE("Fail to copy sponge data to caller\n");
		goto exit;
	}

exit:
	syna_tcm_buf_release(&resp);

	return retval;
}

/**
 * syna_dev_write_to_sponge()
 *
 * Helper to Write raw data to sponge public area
 *
 * @param
 *    [ in] tcm:      tcm driver handle
 *    [ in] wr_buf:   buffer to write
 *    [ in] size_buf: size of given buffer
 *    [ in] offset:   offset to write
 *    [ in] wr_len:   size to write
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_write_to_sponge(struct syna_tcm *tcm,
		unsigned char *wr_buf, int size_buf,
		unsigned short offset, unsigned int wr_len)
{
	int retval = 0;
	unsigned char *payload = NULL;
	unsigned int payload_size;
	struct tcm_dev *tcm_dev = tcm->tcm_dev;
	unsigned char resp_code;
	struct tcm_buffer resp;

	if (tcm_dev->dev_mode != MODE_APPLICATION_FIRMWARE) {
		LOGN("Application firmware not running, current mode: %02x\n",
			tcm_dev->dev_mode);
		return -EINVAL;
	}

	if ((!wr_buf) || (size_buf == 0) || (size_buf < wr_len)) {
		LOGE("Invalid buffer size\n");
		return -EINVAL;
	}

	if (wr_len == 0)
		return 0;

	syna_tcm_buf_init(&resp);

	payload_size = wr_len + 2;
	payload = syna_pal_mem_alloc(payload_size, sizeof(unsigned char));
	if (!payload) {
		LOGE("Fail to allocate payload buffer\n");
		goto exit;
	}

	payload[0] = (unsigned char)(offset & 0xFF);
	payload[1] = (unsigned char)((offset >> 8) & 0xFF);

	retval = syna_pal_mem_cpy(&payload[2],
			payload_size - 2,
			wr_buf,
			size_buf,
			wr_len);
	if (retval < 0) {
		LOGE("Fail to copy sponge data to wr buffer\n");
		goto exit;
	}

	retval = syna_tcm_send_command(tcm_dev,
			CMD_WRITE_SEC_SPONGE_REG,
			payload,
			sizeof(payload),
			&resp_code,
			&resp,
			0);
	if (retval < 0) {
		LOGE("Fail to write to sponge, status:%x (offset:%x len:%d)\n",
			resp_code, offset, wr_len);
		goto exit;
	}

exit:
	if (payload)
		syna_pal_mem_free(payload);

	syna_tcm_buf_release(&resp);

	return retval;
}


#ifdef SEC_NATIVE_EAR_DETECTION
/**
 * syna_dev_detect_proximity()
 *
 * Enable or disable the ear/proximity detection
 *
 * Enable  - 0x00 0x00
 * Disable - 0x01 0x00
 *
 * @param
 *    [ in] tcm: tcm driver handle
 *    [ in] en:  flag to enable or disable
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_detect_proximity(struct syna_tcm *tcm, bool en)
{
	int retval = 0;
	struct tcm_dev *tcm_dev = tcm->tcm_dev;
	unsigned short value = (en) ? 0 : 1;

	if (tcm_dev->dev_mode != MODE_APPLICATION_FIRMWARE) {
		LOGN("Application firmware not running, current mode: %02x\n",
			tcm_dev->dev_mode);
		return -EINVAL;
	}

	/* set up ear detection mode */
	retval = syna_tcm_set_dynamic_config(tcm_dev,
			DC_DISABLE_PROXIMITY,
			value);
	if (retval < 0) {
		LOGE("Fail to set %d with dynamic command 0x%x\n",
			DC_DISABLE_PROXIMITY, value);
		return retval;
	}

	tcm->ed_enable = (int)en;
	LOGI("Proximity detection %s\n",
		(tcm->ed_enable)?"enabled":"disabled");

	return 0;
}
#endif

/**
 * syna_dev_enable_lowpwr_gesture()
 *
 * Enable or disable the low power gesture mode.
 * Furthermore, set up the wake-up irq.
 *
 * @param
 *    [ in] tcm: tcm driver handle
 *    [ in] en:  '1' to enable low power gesture mode; '0' to disable
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_enable_lowpwr_gesture(struct syna_tcm *tcm, bool en)
{
	int retval = 0;
	struct syna_hw_attn_data *attn = &tcm->hw_if->bdata_attn;

	if (!tcm->lpwg_enabled)
		return 0;

	if (attn->irq_id == 0)
		return 0;

	if (en) {
		if (!tcm->irq_wake) {
			enable_irq_wake(attn->irq_id);
			tcm->irq_wake = true;
		}

		/* enable wakeup gesture mode */
		retval = syna_tcm_set_dynamic_config(tcm->tcm_dev,
				DC_ENABLE_WAKEUP_GESTURE_MODE,
				1);
		if (retval < 0) {
			LOGE("Fail to enable wakeup gesture via DC command\n");
			return retval;
		}
	} else {
		if (tcm->irq_wake) {
			disable_irq_wake(attn->irq_id);
			tcm->irq_wake = false;
		}

		/* disable wakeup gesture mode */
		retval = syna_tcm_set_dynamic_config(tcm->tcm_dev,
				DC_ENABLE_WAKEUP_GESTURE_MODE,
				0);
		if (retval < 0) {
			LOGE("Fail to disable wakeup gesture via DC command\n");
			return retval;
		}
	}

	return retval;
}

#ifdef ENABLE_CUSTOM_TOUCH_ENTITY
/**
 * @section: Callback function used for custom entity parsing in touch report
 *
 * Allow to parse the custom "newly" entity in the touch report.
 * Please note that this function will be invoked in ISR
 *
 * @param
 *    [ in]    code:          the code of current touch entity
 *    [ in]    config:        the report configuration stored
 *    [in/out] config_offset: offset of current position in report config,
 *                            and then return the updated position.
 *    [ in]    report:        touch report given
 *    [in/out] report_offset: offset of current position in touch report,
 *                            the updated position should be returned.
 *    [ in]    report_size:   size of given touch report
 *    [ in]    callback_data: pointer to caller data passed to callback function
 *
 * @return
 *    on success, 0 or positive value; otherwise, negative value on error.
 */
static int syna_dev_parse_custom_touch_report(const unsigned char code,
		const unsigned char *config, unsigned int *config_offset,
		const unsigned char *report, unsigned int *report_offset,
		unsigned int report_size, void *callback_data)
{
	int retval;
	unsigned int data;
	unsigned int bits;
	struct syna_tcm *tcm = (struct syna_tcm *)callback_data;

	tcm->display_deep_sleep_state = SLEEP_NO_CHANGE;

	switch (code) {
	case TOUCH_REPORT_DISPLAY_DEEP_SLEEP_STATE:
		bits = config[(*config_offset)++];
		retval = syna_tcm_get_touch_data(report, report_size,
				*report_offset, bits, &data);
		if (retval < 0) {
			LOGE("Fail to get display deep sleep state\n");
			return retval;
		}

		if (tcm->display_deep_sleep_state != data)
			LOGI("display_deep_sleep_state %d\n", data);

		tcm->display_deep_sleep_state = data;

		*report_offset += bits;
		break;
	default:
		LOGW("Unknown touch config code (idx:%d 0x%02x)\n",
			*config_offset, code);
		retval = -1;
		break;
	}

	return retval;
}
#endif

/**
 * syna_tcm_free_input_events()
 *
 * Clear all relevant touched events.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    none.
 */
static void syna_dev_free_input_events(struct syna_tcm *tcm)
{
	struct input_dev *input_dev = tcm->input_dev;
#ifdef TYPE_B_PROTOCOL
	unsigned int idx;
#endif

	if (input_dev == NULL)
		return;

	syna_pal_mutex_lock(&tcm->tp_event_mutex);

	if (tcm->prox_power_off) {
		LOGI("cancel proximity\n");

		input_report_key(tcm->input_dev_proximity,
			KEY_INT_CANCEL, 1);
		input_sync(tcm->input_dev_proximity);
		input_report_key(tcm->input_dev_proximity,
			KEY_INT_CANCEL, 0);
		input_sync(input_dev);
	}

#ifdef TYPE_B_PROTOCOL
	for (idx = 0; idx < MAX_NUM_OBJECTS; idx++) {
		input_mt_slot(input_dev, idx);
		input_mt_report_slot_state(input_dev, MT_TOOL_FINGER, 0);
	}
#endif
	input_report_key(input_dev, BTN_TOUCH, 0);
	input_report_key(input_dev, BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
	input_mt_sync(input_dev);
#endif
	input_sync(input_dev);

	syna_pal_mutex_unlock(&tcm->tp_event_mutex);

}

/**
 * syna_dev_handle_sec_gesture_events()
 *
 * Parse a SEC gesture event .
 *
 * Retrieve data from custom gesture report generated by the device and
 * send input events to the input subsystem.
 *
 * The contents of event are defined by SEC, and the implementation is
 * also reference to SEC's open source.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    none.
 */
static void syna_dev_handle_sec_gesture_events(struct syna_tcm *tcm)
{
	unsigned int report_size;
	struct sec_gesture_event_data *p_event;
	struct input_dev *input_dev = tcm->input_dev;

	if (input_dev == NULL)
		return;

	syna_pal_mutex_lock(&tcm->tp_event_mutex);

	if (tcm->lp_state == PWR_OFF)
		goto exit;

	report_size = tcm->event_data.data_length;

	if (report_size > sizeof(struct sec_gesture_event_data)) {
		LOGE("Invalid report size %d (expected %d)\n",
			report_size,
			(int)sizeof(struct sec_gesture_event_data));
		goto exit;
	}

	if (tcm->lp_state == LP_MODE)
		pm_wakeup_event(tcm->pdev->dev.parent, 1000);

	p_event = (struct sec_gesture_event_data *)tcm->event_data.buf;

	switch (p_event->type) {
	case SEC_TS_GESTURE_CODE_SWIPE:
		tcm->scrub_id = SPONGE_EVENT_TYPE_SPAY;
		LOGD("SEC_TS_GESTURE_CODE_SWIPE\n");
		LOGD("SPAY: id:%d\n", tcm->scrub_id);

		input_report_key(input_dev, KEY_BLACK_UI_GESTURE, 1);

		break;
	case SEC_TS_GESTURE_CODE_DOUBLE_TAP:
		if (p_event->gesture_id == SEC_TS_GESTURE_ID_AOD) {
			tcm->scrub_id = SPONGE_EVENT_TYPE_AOD_DOUBLETAB;
			tcm->scrub_x = (p_event->gesture_data_1 << 4)|
					(p_event->gesture_data_3 >> 4);
			tcm->scrub_y = (p_event->gesture_data_2 << 4) |
					(p_event->gesture_data_3 & 0x0F);
			LOGD("SEC_TS_GESTURE_CODE_DOUBLE_TAP, id: AOD\n");
			LOGD("AOD: id:%d, x:%d, y:%d\n",
				tcm->scrub_id, tcm->scrub_x, tcm->scrub_y);

			input_report_key(input_dev, KEY_BLACK_UI_GESTURE, 1);

		} else if (p_event->gesture_id == SEC_TS_GESTURE_ID_DOUBLETAP_TO_WAKEUP) {
			LOGD("SEC_TS_GESTURE_CODE_DOUBLE_TAP, id: TO_WAKEUP\n");
			LOGD("AOT\n");

			if ((tcm->lp_state == LP_MODE) && tcm->irq_wake) {
				input_report_key(input_dev, KEY_WAKEUP, 1);
				input_sync(input_dev);
				input_report_key(input_dev, KEY_WAKEUP, 0);
			}
		}
		break;
	case SEC_TS_GESTURE_CODE_SINGLE_TAP:
		tcm->scrub_id = SPONGE_EVENT_TYPE_SINGLE_TAP;
		tcm->scrub_x = (p_event->gesture_data_1 << 4) |
					(p_event->gesture_data_3 >> 4);
		tcm->scrub_y = (p_event->gesture_data_2 << 4) |
					(p_event->gesture_data_3 & 0x0F);
		LOGD("SEC_TS_GESTURE_CODE_SINGLE_TAP\n");
		LOGD("SINGLE TAP: id:%d, x:%d, y:%d\n",
				tcm->scrub_id, tcm->scrub_x, tcm->scrub_y);

		input_report_key(input_dev, KEY_BLACK_UI_GESTURE, 1);

		break;
	case SEC_TS_GESTURE_CODE_PRESS:
		LOGD("SEC_TS_GESTURE_CODE_PRESS\n");
		LOGD("FOD: %sPRESS\n", p_event->gesture_id ? "" : "LONG");
		break;
	default:
		LOGE("Invalid gesture type, 0x%x\n", p_event->type);
		goto exit;

	}

	input_sync(input_dev);
	input_report_key(input_dev, KEY_BLACK_UI_GESTURE, 0);

exit:
	syna_pal_mutex_unlock(&tcm->tp_event_mutex);
}

/**
 * syna_dev_handle_sec_touch_events()
 *
 * Parse a SEC coordinate event and report coordinate data to input
 * device subsystem.
 *
 * After syna_tcm_get_event_data() returns and original report data was
 * read in from firmware, parse and retrieve the essential data from the
 * given report data. After that, report to input device subsystm through
 * input events.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    none.
 */
static void syna_dev_handle_sec_touch_events(struct syna_tcm *tcm)
{
	unsigned int report_size;
	struct sec_touch_event_data *p_event;
	struct input_dev *input_dev = tcm->input_dev;
	struct sec_ts_coordinate *ts = tcm->ts_coord;
	unsigned int ts_objs = 0;
	unsigned int idx;
	unsigned char t_id;
	unsigned char max_energy_flag = 0;

	if (input_dev == NULL)
		return;

	syna_pal_mutex_lock(&tcm->tp_event_mutex);

	if (tcm->lp_state == PWR_OFF)
		goto exit;

	syna_pal_mem_set(tcm->ts_coord,
		0x00, sizeof(tcm->ts_coord));

	report_size = tcm->event_data.data_length;

	if (report_size % sizeof(struct sec_touch_event_data) != 0) {
		LOGE("Invalid report size %d (expected %d)\n",
			report_size, (int)sizeof(struct sec_touch_event_data));
		goto exit;
	}

	/* parse the received data and
	 * place into the correspendings position at ts_coord buffer
	 */
	ts_objs = report_size/sizeof(struct sec_touch_event_data);
	p_event = (struct sec_touch_event_data *)tcm->event_data.buf;

	for (idx = 0; idx < ts_objs; idx++) {
		if (p_event[idx].tid >= MAX_NUM_OBJECTS) {
			LOGW("Incorrect tID %d at %d obj\n",
				p_event[idx].tid, idx);
			goto exit;
		}

		t_id = p_event[idx].tid;
		ts[t_id].id = t_id;
		ts[t_id].action = p_event[idx].tchsta;
		ts[t_id].x = (p_event[idx].x_11_4 << 4) | (p_event[idx].x_3_0);
		ts[t_id].y = (p_event[idx].y_11_4 << 4) | (p_event[idx].y_3_0);
		ts[t_id].z = p_event[idx].z;
		ts[t_id].ttype = p_event[idx].ttype_3_2 << 2 |
						p_event[idx].ttype_1_0 << 0;
		ts[t_id].major = p_event[idx].major;
		ts[t_id].minor = p_event[idx].minor;
#if defined(SEC_EVENT_16_BITS)
		max_energy_flag = p_event[t_id].max_energy_flag;
#endif

		if (!ts[t_id].palm && (ts[t_id].ttype == SEC_TS_TOUCHTYPE_PALM))
			ts[t_id].palm_count++;

		ts[t_id].palm = (ts[t_id].ttype == SEC_TS_TOUCHTYPE_PALM);
		ts[t_id].left_event = p_event[idx].left_event;
#if defined(SEC_EVENT_16_BITS)
		ts[t_id].noise_level = MAX(ts[t_id].noise_level,
			(unsigned char)p_event[idx].noise_level);
		ts[t_id].max_strength = MAX(ts[t_id].max_strength,
			(unsigned char)p_event[idx].max_sensitivity);
		ts[t_id].hover_id_num = MAX(ts[t_id].hover_id_num,
			(unsigned char)p_event[idx].hover_id_num);
#endif

		LOGD("[tID:%d] [action:%d] type:%d x:%d y:%d z:%d major:%d minor:%d\n",
			t_id, ts[t_id].action, ts[t_id].ttype, ts[t_id].x,
			ts[t_id].y, ts[t_id].z, ts[t_id].major, ts[t_id].minor);

		LOGD("[tID:%d] palm:%d noise:%x noiselvl:%d maxsen:%d hoverid:%d\n",
			t_id, ts[t_id].palm_count, ts[t_id].noise_status,
			ts[t_id].noise_level, ts[t_id].max_strength, ts[t_id].hover_id_num);

		if (ts[t_id].z <= 0)
			ts[t_id].z = 1;

#ifdef REPORT_SWAP_XY
			ts[t_id].x = ts[t_id].x ^ ts[t_id].y;
			ts[t_id].y = ts[t_id].x ^ ts[t_id].y;
			ts[t_id].x = ts[t_id].x ^ ts[t_id].y;
#endif
#ifdef REPORT_FLIP_X
			ts[t_id].x = tcm->input_dev_params.max_x - ts[t_id].x;
#endif
#ifdef REPORT_FLIP_Y
			ts[t_id].y = tcm->input_dev_params.max_y - ts[t_id].y;
#endif
	}

	/* report to input device subsystem */
	tcm->touch_count = 0;

	for (idx = 0; idx < MAX_NUM_OBJECTS; idx++) {
		if ((ts[idx].action == SEC_TS_COORDINATE_ACTION_NONE) &&
			(tcm->pre_action[idx] == SEC_TS_COORDINATE_ACTION_NONE)) {
			continue;
		}

		if ((ts[idx].action == SEC_TS_COORDINATE_ACTION_NONE) &&
			((tcm->pre_action[idx] == SEC_TS_COORDINATE_ACTION_MOVE) ||
			(tcm->pre_action[idx] == SEC_TS_COORDINATE_ACTION_PRESS))) {

			LOGD("[idx:%d][tID:%d] jumping:%x->%x\n",
				idx, ts[idx].id, tcm->pre_action[idx], ts[t_id].action);

			ts[idx].action = SEC_TS_COORDINATE_ACTION_RELEASE;
		}

		switch (ts[idx].action) {
		case SEC_TS_COORDINATE_ACTION_RELEASE:
			input_mt_slot(input_dev, idx);
			input_report_abs(input_dev, ABS_MT_CUSTOM, 0);
			input_mt_report_slot_state(input_dev,
				MT_TOOL_FINGER, 0);

			LOGD("[idx:%d][tID:%d] finger UP\n", idx, ts[idx].id);

			ts[idx].action = SEC_TS_COORDINATE_ACTION_NONE;
			ts[idx].mcount = 0;
			ts[idx].palm_count = 0;
			ts[idx].noise_level = 0;
			ts[idx].max_strength = 0;
			ts[idx].hover_id_num = 0;
			break;
		case SEC_TS_COORDINATE_ACTION_PRESS:
			ts[idx].max_energy_x = 0;
			ts[idx].max_energy_y = 0;

			ts[idx].p_x = ts[idx].x;
			ts[idx].p_y = ts[idx].y;

			if (max_energy_flag) {
				ts[idx].max_energy_x = ts[idx].x;
				ts[idx].max_energy_y = ts[idx].y;
			}

			LOGD("[idx:%d][tID:%d] finger DOWN\n", idx, ts[idx].id);

		case SEC_TS_COORDINATE_ACTION_MOVE:
#ifdef TYPE_B_PROTOCOL
			input_mt_slot(input_dev, idx);
			input_mt_report_slot_state(input_dev,
				MT_TOOL_FINGER, 1);
#endif
			input_report_key(input_dev, BTN_TOUCH, 1);
			input_report_key(input_dev, BTN_TOOL_FINGER, 1);

			input_report_abs(input_dev, ABS_MT_POSITION_X,
				ts[idx].x);
			input_report_abs(input_dev, ABS_MT_POSITION_Y,
				ts[idx].y);
#ifdef REPORT_TOUCH_WIDTH
			input_report_abs(input_dev, ABS_MT_TOUCH_MAJOR,
				ts[idx].major);
			input_report_abs(input_dev, ABS_MT_TOUCH_MINOR,
				ts[idx].minor);
#endif

			input_report_abs(input_dev, ABS_MT_CUSTOM,
				((max_energy_flag << 16) | (1 << 8) |
				(BRUSH_Z_DATA << 1) | ts[idx].palm));

#ifndef TYPE_B_PROTOCOL
			input_mt_sync(input_dev);
#endif
			tcm->touch_count++;
			break;
		default:
			break;
		}

		tcm->pre_action[idx] = ts[idx].action;
		tcm->pre_ttype[idx] = ts[idx].ttype;
	}

	if (tcm->touch_count == 0) {
		input_report_key(input_dev, BTN_TOUCH, 0);
		input_report_key(input_dev, BTN_TOOL_FINGER, 0);
#ifndef TYPE_B_PROTOCOL
		input_mt_sync(input_dev);
#endif
	}

	input_sync(input_dev);

exit:
	syna_pal_mutex_unlock(&tcm->tp_event_mutex);
}

/**
 * syna_dev_handle_sec_status_events()
 *
 * Handle a SEC status report.
 *
 * After syna_tcm_get_event_data() returns and original report data was
 * read in from firmware, parse and retrieve the essential data from the
 * given report data.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    none.
 */
static void syna_dev_handle_sec_status_events(struct syna_tcm *tcm)
{
	unsigned int report_size;
	struct sec_status_event_data *p_event;
	struct input_dev *input_dev = tcm->input_dev;

	if (input_dev == NULL)
		return;

	syna_pal_mutex_lock(&tcm->tp_event_mutex);

	if (tcm->lp_state == PWR_OFF)
		goto exit;

	report_size = tcm->event_data.data_length;

	if (report_size > sizeof(struct sec_status_event_data)) {
		LOGE("Invalid report size %d (expected %d)\n",
			report_size, (int)sizeof(struct sec_status_event_data));
		goto exit;
	}

	p_event = (struct sec_status_event_data *)tcm->event_data.buf;

	LOGD("type:%d id:%d\n",p_event->stype, p_event->status_id);

	if ((p_event->stype == TYPE_STATUS_EVENT_INFO) &&
		(p_event->status_id == SEC_TS_ACK_WET_MODE)) {

		tcm->wet_count = p_event->status_data_1;
	}

exit:
	syna_pal_mutex_unlock(&tcm->tp_event_mutex);
}

#if defined(SEC_NATIVE_EAR_DETECTION)
/**
 * syna_dev_create_input_proximity_device()
 *
 * Allocate an input device and set up relevant parameters to the
 * input subsystem.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_create_input_proximity_device(struct syna_tcm *tcm)
{
	int retval = 0;
	struct input_dev *input_dev = NULL;
	static char mms_phys[64] = { 0 };
#ifdef DEV_MANAGED_API
	struct device *dev = syna_request_managed_device();

	if (!dev) {
		LOGE("Invalid managed device\n");
		return -EINVAL;
	}

	input_dev = devm_input_allocate_device(dev);
#else /* Legacy API */
	input_dev = input_allocate_device();
#endif
	if (input_dev == NULL) {
		LOGE("Fail to allocate sec_touchproximity\n");
		return -ENODEV;
	}

	input_dev->name = "sec_touchproximity";

	snprintf(mms_phys, sizeof(mms_phys), "%s1", TOUCH_INPUT_PHYS_PATH);
	input_dev->phys = mms_phys;
	input_dev->dev.parent = tcm->pdev->dev.parent;

	input_set_drvdata(input_dev, tcm);

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_SW, input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_abs_params(input_dev, ABS_MT_CUSTOM, 0, 0xFFFFFFFF, 0, 0);

	retval = input_register_device(input_dev);
	if (retval < 0) {
		LOGE("Fail to register input device, sec_touchproximity\n");
		input_free_device(input_dev);
		input_dev = NULL;
		return retval;
	}

	tcm->input_dev_proximity = input_dev;

	return 0;
}

/**
 * syna_dev_release_input_proximity_device()
 *
 * Release an input device allocated previously.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    none.
 */
static void syna_dev_release_input_proximity_device(struct syna_tcm *tcm)
{
	if (!tcm->input_dev_proximity)
		return;

	input_unregister_device(tcm->input_dev_proximity);
	input_free_device(tcm->input_dev_proximity);

	tcm->input_dev_proximity = NULL;
}
#endif /* end of defined(SEC_NATIVE_EAR_DETECTION) */

/**
 * syna_dev_open_input_device()
 *
 * By request, configure the device into normal power mode.
 * Thus, call dev_resume_pwr() function directly
 *
 * @param
 *    [ in] dev: an instance of input device
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
int syna_dev_open_input_device(struct input_dev *dev)
{
	int retval;
	struct syna_tcm *tcm = input_get_drvdata(dev);
	struct syna_hw_interface *hw_if = tcm->hw_if;
	bool support_ear_detect = hw_if->bdata.support_ear_detect;

	if (!tcm->is_connected)
		return 0;

	LOGD("lpwg_enabled:%d, ear_detection:%d\n",
		tcm->lpwg_enabled, support_ear_detect);

	if (tcm->lpwg_enabled || support_ear_detect) {
		retval = tcm->dev_set_normal_sensing(tcm);
		if (retval < 0) {
			LOGE("Fail to resume to normal power mode\n");
			return retval;
		}
	}

	return 0;
}

/**
 * syna_dev_close_input_device()
 *
 * By request, configure the device into low-power mode.
 * Thus, call dev_suspend_pwr() function directly
 *
 * @param
 *    [ in] dev: an instance of input device
 *
 * @return
 *    none.
 */
void syna_dev_close_input_device(struct input_dev *dev)
{
	int retval;
	struct syna_tcm *tcm = input_get_drvdata(dev);
	struct syna_hw_interface *hw_if = tcm->hw_if;
	bool support_ear_detect = hw_if->bdata.support_ear_detect;

	LOGD("lpwg_enabled:%d, ear_detection:%d\n",
		tcm->lpwg_enabled, support_ear_detect);

	if (tcm->lpwg_enabled || support_ear_detect) {
		retval = tcm->dev_set_lowpwr_sensing(tcm);
		if (retval < 0) {
			LOGE("Fail to enter to low power mode\n");
			return;
		}
	}
}
/**
 * syna_dev_create_input_device()
 *
 * Allocate an input device and set up relevant parameters to the
 * input subsystem.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_create_input_device(struct syna_tcm *tcm)
{
	int retval = 0;
	struct tcm_dev *tcm_dev = tcm->tcm_dev;
	struct input_dev *input_dev = NULL;
#ifdef DEV_MANAGED_API
	struct device *dev = syna_request_managed_device();

	if (!dev) {
		LOGE("Invalid managed device\n");
		return -EINVAL;
	}

	input_dev = devm_input_allocate_device(dev);
#else /* Legacy API */
	input_dev = input_allocate_device();
#endif
	if (input_dev == NULL) {
		LOGE("Fail to allocate input device\n");
		return -ENODEV;
	}

	input_dev->name = TOUCH_INPUT_NAME;
	input_dev->phys = TOUCH_INPUT_PHYS_PATH;
	input_dev->id.product = SYNAPTICS_TCM_DRIVER_ID;
	input_dev->id.version = SYNAPTICS_TCM_DRIVER_VERSION;
	input_dev->dev.parent = tcm->pdev->dev.parent;
	input_dev->open = syna_dev_open_input_device;
	input_dev->close = syna_dev_close_input_device;
	input_set_drvdata(input_dev, tcm);

	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, input_dev->propbit);
#endif

#ifdef ENABLE_WAKEUP_GESTURE
	set_bit(KEY_WAKEUP, input_dev->keybit);
	input_set_capability(input_dev, EV_KEY, KEY_WAKEUP);
#endif

	input_set_abs_params(input_dev,
			ABS_MT_POSITION_X, 0, tcm_dev->max_x, 0, 0);
	input_set_abs_params(input_dev,
			ABS_MT_POSITION_Y, 0, tcm_dev->max_y, 0, 0);

	input_mt_init_slots(input_dev, tcm_dev->max_objects,
			INPUT_MT_DIRECT);

#ifdef REPORT_TOUCH_WIDTH
	input_set_abs_params(input_dev,
			ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(input_dev,
			ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
#endif

	tcm->input_dev_params.max_x = tcm_dev->max_x;
	tcm->input_dev_params.max_y = tcm_dev->max_y;
	tcm->input_dev_params.max_objects = tcm_dev->max_objects;

	retval = input_register_device(input_dev);
	if (retval < 0) {
		LOGE("Fail to register input device\n");
		input_free_device(input_dev);
		input_dev = NULL;
		return retval;
	}

	tcm->input_dev = input_dev;

	return 0;
}

/**
 * syna_dev_release_input_device()
 *
 * Release an input device allocated previously.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    none.
 */
static void syna_dev_release_input_device(struct syna_tcm *tcm)
{
	if (!tcm->input_dev)
		return;

	input_unregister_device(tcm->input_dev);
	input_free_device(tcm->input_dev);

	tcm->input_dev = NULL;
}

/**
 * syna_dev_check_input_params()
 *
 * Check if any of the input parameters registered to the input subsystem
 * has changed.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    return 1, parameters are changed; otherwise, return 0.
 */
static int syna_dev_check_input_params(struct syna_tcm *tcm)
{
	struct tcm_dev *tcm_dev = tcm->tcm_dev;

	if (tcm_dev->max_x == 0 && tcm_dev->max_y == 0)
		return 0;

	if (tcm->input_dev_params.max_x != tcm_dev->max_x)
		return 1;

	if (tcm->input_dev_params.max_y != tcm_dev->max_y)
		return 1;

	if (tcm->input_dev_params.max_objects != tcm_dev->max_objects)
		return 1;

	if (tcm_dev->max_objects > MAX_NUM_OBJECTS) {
		LOGW("Out of max num objects defined, in app_info: %d\n",
			tcm_dev->max_objects);
		return 0;
	}

	LOGN("Input parameters unchanged\n");

	return 0;
}

/**
 * syna_dev_set_up_input_device()
 *
 * Set up input device to the input subsystem by confirming the supported
 * parameters and creating the device.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_set_up_input_device(struct syna_tcm *tcm)
{
	int retval = 0;

	if (IS_NOT_APP_FW_MODE(tcm->tcm_dev->dev_mode)) {
		LOGN("Application firmware not running, current mode: %02x\n",
			tcm->tcm_dev->dev_mode);
		return 0;
	}

	syna_dev_free_input_events(tcm);

	syna_pal_mutex_lock(&tcm->tp_event_mutex);

	retval = syna_dev_check_input_params(tcm);
	if (retval == 0)
		goto exit;

	if (tcm->input_dev != NULL)
		syna_dev_release_input_device(tcm);

	retval = syna_dev_create_input_device(tcm);
	if (retval < 0) {
		LOGE("Fail to create input device\n");
		goto exit;
	}
#if defined(SEC_NATIVE_EAR_DETECTION)
	if (tcm->hw_if->bdata.support_ear_detect) {
		retval = syna_dev_create_input_proximity_device(tcm);
		if (retval < 0) {
			LOGE("Fail to create input proximity device\n");
			syna_dev_release_input_device(tcm);
			goto exit;
		}
	}
#endif

	syna_pal_mem_set(tcm->pre_action, 0x00,
		sizeof(tcm->pre_action));
	syna_pal_mem_set(tcm->pre_ttype, 0x00,
		sizeof(tcm->pre_ttype));

exit:
	syna_pal_mutex_unlock(&tcm->tp_event_mutex);

	return retval;
}

/**
 * syna_dev_isr()
 *
 * This is the function to be called when the interrupt is asserted.
 * The purposes of this handler is to read events generated by device and
 * retrieve all enqueued messages until ATTN is no longer asserted.
 *
 * @param
 *    [ in] irq:  interrupt line
 *    [ in] data: private data being passed to the handler function
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static irqreturn_t syna_dev_isr(int irq, void *data)
{
	int retval;
	unsigned char code = 0;
	struct syna_tcm *tcm = data;
	struct syna_hw_attn_data *attn = &tcm->hw_if->bdata_attn;

	if (unlikely(gpio_get_value(attn->irq_gpio) != attn->irq_on_state))
		goto exit;

	tcm->isr_pid = current->pid;

#ifdef CONFIG_TOUCHSCREEN_SYNA_TCM2_SYSFS
	if (tcm->is_attn_redirecting) {
		syna_cdev_redirect_attn(tcm);
		goto exit;
	}
#endif
	/* retrieve the original report date generated by firmware */
	retval = syna_tcm_get_event_data(tcm->tcm_dev,
			&code,
			&tcm->event_data,
			&tcm->tp_data);
	if (retval < 0) {
		LOGE("Fail to get event data\n");
		goto exit;
	}

#ifdef ENABLE_EXTERNAL_FRAME_PROCESS
	if (tcm->report_to_queue[code] == EFP_ENABLE) {
		syna_cdev_update_report_queue(tcm, code,
		    &tcm->tcm_dev->external_buf);
#ifndef REPORT_CONCURRENTLY
		goto exit;
#endif
	}
#endif

	/* process SEC reports */
	if (code == REPORT_SEC_COORDINATE_EVENT)
		syna_dev_handle_sec_touch_events(tcm);
	else if (code == REPORT_SEC_GESTURE_EVENT)
		syna_dev_handle_sec_gesture_events(tcm);
	else if (code == REPORT_SEC_STATUS_EVENT)
		syna_dev_handle_sec_status_events(tcm);

exit:
	return IRQ_HANDLED;
}


/**
 * syna_dev_request_irq()
 *
 * Allocate an interrupt line and register the ISR handler
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_request_irq(struct syna_tcm *tcm)
{
	int retval;
	struct syna_hw_attn_data *attn = &tcm->hw_if->bdata_attn;
#ifdef DEV_MANAGED_API
	struct device *dev = syna_request_managed_device();

	if (!dev) {
		LOGE("Invalid managed device\n");
		retval = -EINVAL;
		goto exit;
	}
#endif

	if (attn->irq_gpio < 0) {
		LOGE("Invalid IRQ GPIO\n");
		retval = -EINVAL;
		goto exit;
	}

	attn->irq_id = gpio_to_irq(attn->irq_gpio);

#ifdef DEV_MANAGED_API
	retval = devm_request_threaded_irq(dev,
			attn->irq_id,
			NULL,
			syna_dev_isr,
			attn->irq_flags,
			PLATFORM_DRIVER_NAME,
			tcm);
#else /* Legacy API */
	retval = request_threaded_irq(attn->irq_id,
			NULL,
			syna_dev_isr,
			attn->irq_flags,
			PLATFORM_DRIVER_NAME,
			tcm);
#endif
	if (retval < 0) {
		LOGE("Fail to request threaded irq\n");
		goto exit;
	}

	attn->irq_enabled = true;

	LOGI("Interrupt handler registered\n");

exit:
	return retval;
}

/**
 * syna_dev_release_irq()
 *
 * Release an interrupt line allocated previously
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    none.
 */
static void syna_dev_release_irq(struct syna_tcm *tcm)
{
	struct syna_hw_attn_data *attn = &tcm->hw_if->bdata_attn;
#ifdef DEV_MANAGED_API
	struct device *dev = syna_request_managed_device();

	if (!dev) {
		LOGE("Invalid managed device\n");
		return;
	}
#endif

	if (attn->irq_id <= 0)
		return;

	if (tcm->hw_if->ops_enable_irq)
		tcm->hw_if->ops_enable_irq(tcm->hw_if, false);

#ifdef DEV_MANAGED_API
	devm_free_irq(dev, attn->irq_id, tcm);
#else
	free_irq(attn->irq_id, tcm);
#endif

	attn->irq_id = 0;
	attn->irq_enabled = false;

	LOGI("Interrupt handler released\n");
}

/**
 * syna_dev_set_up_app_fw()
 *
 * Implement the essential steps for the initialization including the
 * preparation of app info and the configuration of touch report.
 *
 * This function should be called whenever the device initially powers
 * up, resets, or firmware update.
 *
 * @param
 *    [ in] tcm: tcm driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_set_up_app_fw(struct syna_tcm *tcm)
{
	int retval = 0;
	struct tcm_dev *tcm_dev = tcm->tcm_dev;

	if (IS_NOT_APP_FW_MODE(tcm_dev->dev_mode)) {
		LOGN("Application firmware not running, current mode: %02x\n",
			tcm_dev->dev_mode);
		return -EINVAL;
	}

	/* collect app info containing most of sensor information */
	retval = syna_tcm_get_app_info(tcm_dev, &tcm_dev->app_info);
	if (retval < 0) {
		LOGE("Fail to get application info\n");
		return -EIO;
	}

	/* VERIFY_SEC_REPORTS code segmant is used to verify the
	 * SEC reports in the prior stage, it should be disabled
	 * at the final release and configured in default fw.
	 */
#ifdef VERIFY_SEC_REPORTS
	/* disable the generic touchcomm touch report */
	retval = syna_tcm_enable_report(tcm_dev,
			REPORT_TOUCH, false);
	if (retval < 0) {
		LOGE("Fail to disable touch report\n");
		return -EIO;
	}
	/* enable the sec reports */
	retval = syna_tcm_enable_report(tcm_dev,
			REPORT_SEC_GESTURE_EVENT, true);
	if (retval < 0) {
		LOGE("Fail to enable sec gesture report\n");
		return -EIO;
	}
	retval = syna_tcm_enable_report(tcm_dev,
			REPORT_SEC_COORDINATE_EVENT, true);
	if (retval < 0) {
		LOGE("Fail to enable sec coordinate report\n");
		return -EIO;
	}
	retval = syna_tcm_enable_report(tcm_dev,
			REPORT_SEC_STATUS_EVENT, true);
	if (retval < 0) {
		LOGE("Fail to enable sec status report\n");
		return -EIO;
	}

	return retval;
#endif

	/* set up the format of touch report */
#ifdef USE_CUSTOM_TOUCH_REPORT_CONFIG
	retval = syna_tcm_set_touch_report_config(tcm_dev,
			custom_touch_format,
			(unsigned int)sizeof(custom_touch_format));
	if (retval < 0) {
		LOGE("Fail to setup the custom touch report format\n");
		return -EIO;
	}
#endif
	/* preserve the format of touch report */
	retval = syna_tcm_preserve_touch_report_config(tcm_dev);
	if (retval < 0) {
		LOGE("Fail to preserve touch report config\n");
		return -EIO;
	}

#ifdef ENABLE_CUSTOM_TOUCH_ENTITY
	/* set up custom touch data parsing method */
	retval = syna_tcm_set_custom_touch_data_parsing_callback(tcm_dev,
			syna_dev_parse_custom_touch_report,
			(void *)tcm);
	if (retval < 0) {
		LOGE("Fail to set up custom touch data parsing method\n");
		return -EIO;
	}
#endif
	return retval;
}

/**
 * syna_dev_reflash_startup_work()
 *
 * Perform firmware update during system startup.
 * Function is available when the 'STARTUP_REFLASH' configuration
 * is enabled.
 *
 * @param
 *    [ in] work: handle of work structure
 *
 * @return
 *    none.
 */
#ifdef STARTUP_REFLASH
static void syna_dev_reflash_startup_work(struct work_struct *work)
{
	int retval;
	struct delayed_work *delayed_work;
	struct syna_tcm *tcm;
	struct tcm_dev *tcm_dev;
	const struct firmware *fw_entry;
	const unsigned char *fw_image = NULL;
	unsigned int fw_image_size;

	delayed_work = container_of(work, struct delayed_work, work);
	tcm = container_of(delayed_work, struct syna_tcm, reflash_work);

	tcm_dev = tcm->tcm_dev;

	/* get firmware image */
	retval = request_firmware(&fw_entry,
			FW_IMAGE_NAME,
			tcm->pdev->dev.parent);
	if (retval < 0) {
		LOGE("Fail to request %s\n", FW_IMAGE_NAME);
		return;
	}

	fw_image = fw_entry->data;
	fw_image_size = fw_entry->size;

	LOGD("Firmware image size = %d\n", fw_image_size);

	pm_stay_awake(&tcm->pdev->dev);

	/* perform fw update */
#ifdef MULTICHIP_DUT_REFLASH
	retval = syna_tcm_romboot_do_multichip_reflash(tcm_dev,
			fw_image,
			fw_image_size,
			(REFLASH_ERASE_DELAY << 16) | REFLASH_WRITE_DELAY,
			false);
#else
	retval = syna_tcm_do_fw_update(tcm_dev,
			fw_image,
			fw_image_size,
			(REFLASH_ERASE_DELAY << 16) | REFLASH_WRITE_DELAY,
			false);
#endif
	if (retval < 0) {
		LOGE("Fail to do reflash\n");
		goto exit;
	}

	/* re-initialize the app fw */
	retval = syna_dev_set_up_app_fw(tcm);
	if (retval < 0) {
		LOGE("Fail to set up app fw after fw update\n");
		goto exit;
	}

	/* allocate the input device if not registered yet */
	if (tcm->input_dev == NULL) {
		retval = syna_dev_set_up_input_device(tcm);
		if (retval < 0) {
			LOGE("Fail to register input device\n");
			goto exit;
		}
	}

exit:
	pm_relax(&tcm->pdev->dev);
}
#endif
/**
 * syna_dev_enter_normal_sensing()
 *
 * Helper to enter normal seneing mode
 *
 * @param
 *    [ in] tcm: tcm driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_enter_normal_sensing(struct syna_tcm *tcm)
{
	int retval = 0;

	if (!tcm)
		return -EINVAL;

	/* bring out of sleep mode. */
	retval = syna_tcm_sleep(tcm->tcm_dev, false);
	if (retval < 0) {
		LOGE("Fail to exit deep sleep\n");
		return retval;
	}

	/* disable low power gesture mode, if needed */
	if (tcm->lpwg_enabled) {
		retval = syna_dev_enable_lowpwr_gesture(tcm, false);
		if (retval < 0) {
			LOGE("Fail to disable low power gesture mode\n");
			return retval;
		}
	}
#if defined(SEC_NATIVE_EAR_DETECTION)
	if (tcm->ed_enable) {
		retval = syna_dev_detect_proximity(tcm, false);
		if (retval < 0) {
			LOGE("Fail to enable ear_detect mode\n");
			return retval;
		}
	}
#endif

	tcm->prox_power_off = 0;
	tcm->lp_state = PWR_ON;

	LOGD("power state:%d\n", tcm->lp_state);

	return 0;
}
/**
 * syna_dev_enter_lowpwr_sensing()
 *
 * Helper to enter power-saved sensing mode, that
 * may be the lower power gesture mode or deep sleep mode.
 *
 * @param
 *    [ in] tcm: tcm driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_enter_lowpwr_sensing(struct syna_tcm *tcm)
{
	int retval = 0;

	if (!tcm)
		return -EINVAL;

	/* enable low power gesture mode, if needed */
	if (tcm->lpwg_enabled) {
		retval = syna_dev_enable_lowpwr_gesture(tcm, true);
		if (retval < 0) {
			LOGE("Fail to disable low power gesture mode\n");
			return retval;
		}
	} else {
	/* enter sleep mode for non-LPWG cases */
		if (!tcm->slept_in_early_suspend) {
			retval = syna_tcm_sleep(tcm->tcm_dev, true);
			if (retval < 0) {
				LOGE("Fail to enter deep sleep\n");
				return retval;
			}
		}
	}

	if (tcm->lpwg_enabled || tcm->ed_enable)
		tcm->lp_state = LP_MODE;
	else
		tcm->lp_state = PWR_OFF;

	LOGD("power state:%d\n", tcm->lp_state);

	return 0;
}
/**
 * syna_dev_resume()
 *
 * Resume from the suspend state.
 * If RESET_ON_RESUME is defined, a reset is issued to the touch controller.
 * Otherwise, the touch controller is brought out of sleep mode.
 *
 * @param
 *    [ in] dev: an instance of device
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_resume(struct device *dev)
{
	int retval;
	struct syna_tcm *tcm = dev_get_drvdata(dev);
	struct syna_hw_interface *hw_if = tcm->hw_if;
	bool irq_enabled = true;

	/* exit directly already in power on state */
	if (tcm->lp_state == PWR_ON)
		return 0;

#ifdef RESET_ON_RESUME
	syna_pal_sleep_ms(RESET_ON_RESUME_DELAY_MS);

	if (hw_if->ops_hw_reset) {
		hw_if->ops_hw_reset(hw_if);
	} else {
		retval = syna_tcm_reset(tcm->tcm_dev);
		if (retval < 0) {
			LOGE("Fail to do reset\n");
			goto exit;
		}
	}
#else
	/* enter normal power mode */
	retval = syna_dev_enter_normal_sensing(tcm);
	if (retval < 0) {
		LOGE("Fail to enter normal power mode\n");
		goto exit;
	}

	retval = syna_tcm_rezero(tcm->tcm_dev);
	if (retval < 0) {
		LOGE("Fail to rezero\n");
		goto exit;
	}
#endif /* end of RESET_ON_RESUME */

	LOGI("Prepare to set up application firmware\n");

	/* set up app firmware */
	retval = syna_dev_set_up_app_fw(tcm);
	if (retval < 0) {
		LOGE("Fail to set up app firmware on resume\n");
		goto exit;
	}

	retval = 0;

	LOGI("Device resumed\n");

exit:
	/* once lpwg is enabled, irq should be enabled already.
	 * otherwise, set irq back to active mode.
	 */
	irq_enabled = (!tcm->lpwg_enabled) && (!tcm->ed_enable);

	/* enable irq */
	if (irq_enabled && (hw_if->ops_enable_irq))
		hw_if->ops_enable_irq(hw_if, true);

	tcm->slept_in_early_suspend = false;

	complete_all(&tcm->resume_done);

	return retval;
}

/**
 * syna_dev_suspend()
 *
 * Put device into suspend state.
 * Enter either the lower power gesture mode or sleep mode.
 *
 * @param
 *    [ in] dev: an instance of device
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_suspend(struct device *dev)
{
	struct syna_tcm *tcm = dev_get_drvdata(dev);
	struct syna_hw_interface *hw_if = tcm->hw_if;
	bool irq_disabled = true;

	/* exit directly once in power off state */
	if (tcm->lp_state == PWR_OFF)
		return 0;

	/* clear all input events  */
	syna_dev_free_input_events(tcm);

	/* clear the event buffer */
	syna_pal_mem_set(tcm->event_data.buf,
		0x00, tcm->event_data.buf_size);
	syna_pal_mem_set(tcm->ts_coord,
		0x00, sizeof(tcm->ts_coord));

#ifdef POWER_ALIVE_AT_SUSPEND
	/* enter power saved mode if power is not off */
	if (syna_dev_suspend_pwr(tcm) < 0) {
		LOGE("Fail to enter suspended power mode\n");
	}
#endif

	/* once lpwg is enabled, irq should be alive.
	 * otherwise, disable irq in suspend.
	 */
	irq_disabled = (!tcm->lpwg_enabled) && (!tcm->ed_enable);

	/* disable irq */
	if (irq_disabled && (hw_if->ops_enable_irq))
		hw_if->ops_enable_irq(hw_if, false);

	reinit_completion(&tcm->resume_done);

	LOGI("Device suspended\n");

	return 0;
}

#if defined(ENABLE_DISP_NOTIFIER)
/**
 * syna_dev_early_suspend()
 *
 * If having early suspend support, enter the sleep mode for
 * non-lpwg cases.
 *
 * @param
 *    [ in] dev: an instance of device
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_early_suspend(struct device *dev)
{
	int retval;
	struct syna_tcm *tcm = dev_get_drvdata(dev);

	/* exit directly once in power off state */
	if (tcm->lp_state == PWR_OFF)
		return 0;

	if (!tcm->lpwg_enabled) {
		retval = syna_tcm_sleep(tcm->tcm_dev, true);
		if (retval < 0) {
			LOGE("Fail to enter deep sleep\n");
			return retval;
		}
	}

	tcm->slept_in_early_suspend = true;

	return 0;
}
/**
 * syna_dev_fb_notifier_cb()
 *
 * Listen the display screen on/off event and perform the corresponding
 * actions.
 *
 * @param
 *    [ in] nb:     instance of notifier_block
 *    [ in] action: fb action
 *    [ in] data:   fb event data
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_fb_notifier_cb(struct notifier_block *nb,
		unsigned long action, void *data)
{
	int retval;
	int transition;
#if defined(USE_DRM_PANEL_NOTIFIER)
	struct drm_panel_notifier *evdata = data;
#else
	struct fb_event *evdata = data;
#endif
	struct syna_tcm *tcm = container_of(nb, struct syna_tcm, fb_notifier);
	int time = 0;
	int disp_blank_powerdown;
	int disp_early_event_blank;
	int disp_blank;
	int disp_blank_unblank;

	if (!evdata || !evdata->data || !tcm)
		return 0;

	retval = 0;

#if defined(USE_DRM_PANEL_NOTIFIER)
	disp_blank_powerdown = DRM_PANEL_BLANK_POWERDOWN;
	disp_early_event_blank = DRM_PANEL_EARLY_EVENT_BLANK;
	disp_blank = DRM_PANEL_EVENT_BLANK;
	disp_blank_unblank = DRM_PANEL_BLANK_UNBLANK;
#else
	disp_blank_powerdown = FB_BLANK_POWERDOWN;
	disp_early_event_blank = FB_EARLY_EVENT_BLANK;
	disp_blank = FB_EVENT_BLANK;
	disp_blank_unblank = FB_BLANK_UNBLANK;
#endif

	transition = *(int *)evdata->data;

	/* confirm the firmware flashing is completed before screen off */
	if (transition == disp_blank_powerdown) {
		while (ATOMIC_GET(tcm->tcm_dev->firmware_flashing)) {
			syna_pal_sleep_ms(500);

			time += 500;
			if (time >= 5000) {
				LOGE("Timed out waiting for reflashing\n");
				ATOMIC_SET(tcm->tcm_dev->firmware_flashing, 0);
				return -EIO;
			}
		}
	}

	if (action == disp_early_event_blank &&
		transition == disp_blank_powerdown) {
		retval = syna_dev_early_suspend(&tcm->pdev->dev);
	} else if (action == disp_blank) {
		if (transition == disp_blank_powerdown) {
			retval = syna_dev_suspend(&tcm->pdev->dev);
			tcm->fb_ready = 0;
		} else if (transition == disp_blank_unblank) {
#ifndef RESUME_EARLY_UNBLANK
			retval = syna_dev_resume(&tcm->pdev->dev);
			tcm->fb_ready++;
#endif
		} else if (action == disp_early_event_blank &&
			transition == disp_blank_unblank) {
#ifdef RESUME_EARLY_UNBLANK
			retval = syna_dev_resume(&tcm->pdev->dev);
			tcm->fb_ready++;
#endif
		}
	}

	return 0;
}
#endif

/**
 * syna_dev_disconnect()
 *
 * This function will power off the connected device.
 * Then, all the allocated resource will be released.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_disconnect(struct syna_tcm *tcm)
{
	struct syna_hw_interface *hw_if = tcm->hw_if;
	struct tcm_dev *tcm_dev = tcm->tcm_dev;

	if (!tcm_dev) {
		LOGE("Invalid tcm_dev\n");
		return -EINVAL;
	}

	if (tcm->is_connected == false) {
		LOGI("%s already disconnected\n", PLATFORM_DRIVER_NAME);
		return 0;
	}

#ifdef STARTUP_REFLASH
	cancel_delayed_work_sync(&tcm->reflash_work);
	flush_workqueue(tcm->reflash_workqueue);
	destroy_workqueue(tcm->reflash_workqueue);
#endif

	/* free interrupt line */
	if (hw_if->bdata_attn.irq_id)
		syna_dev_release_irq(tcm);
#if 0
	/* remove SEC custom functions */
	syna_sec_fn_remove(tcm);
#endif
	/* unregister input device */
	syna_dev_release_input_device(tcm);

#if defined(SEC_NATIVE_EAR_DETECTION)
	syna_dev_release_input_proximity_device(tcm);
#endif

	tcm->input_dev_params.max_x = 0;
	tcm->input_dev_params.max_y = 0;
	tcm->input_dev_params.max_objects = 0;

	/* power off */
	if (hw_if->ops_power_on)
		hw_if->ops_power_on(hw_if, false);

	tcm->is_connected = false;

	LOGI("%s device disconnected\n", PLATFORM_DRIVER_NAME);

	return 0;
}

/**
 * syna_dev_connect()
 *
 * This function will power on and identify the connected device.
 * At the end of function, the ISR will be registered as well.
 *
 * @param
 *    [ in] tcm: the driver handle
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_connect(struct syna_tcm *tcm)
{
	int retval;
	struct syna_hw_interface *hw_if = tcm->hw_if;
	struct tcm_dev *tcm_dev = tcm->tcm_dev;

	if (!tcm_dev) {
		LOGE("Invalid tcm_dev\n");
		return -EINVAL;
	}

	if (tcm->is_connected) {
		LOGI("%s already connected\n", PLATFORM_DRIVER_NAME);
		return 0;
	}

	/* power on the connected device */
	if (hw_if->ops_power_on) {
		retval = hw_if->ops_power_on(hw_if, true);
		if (retval < 0)
			return -ENODEV;
	}

	/* perform a hardware reset */
	if (hw_if->ops_hw_reset)
		hw_if->ops_hw_reset(hw_if);

	/* detect which modes of touch controller is running
	 *
	 * the signal of the Touch IC ready is called as "identify"
	 * report and generated by firmware
	 */
	retval = syna_tcm_detect_device(tcm->tcm_dev);
	if (retval < 0) {
		LOGE("Fail to detect the device\n");
		goto err_detect_dev;
	}

	switch (retval) {
	case MODE_APPLICATION_FIRMWARE:
		retval = syna_dev_set_up_app_fw(tcm);
		if (retval < 0) {
#ifdef FORCE_CONNECTION
			LOGW("App firmware is not available yet\n");
			LOGW("However, connect and skip initialization\n");
			goto end;
#else
			LOGE("Fail to set up application firmware\n");
			goto err_setup_app_fw;
#endif
		}

		/* allocate and register to input device subsystem */
		retval = syna_dev_set_up_input_device(tcm);
		if (retval < 0) {
			LOGE("Fail to set up input device\n");
			goto err_setup_input_dev;
		}

		break;
	default:
		LOGN("Application firmware not running, current mode: %02x\n",
			retval);
		break;
	}
#if 0
	/* SEC custom functions */
	retval = syna_sec_fn_init(tcm);
	if (retval < 0) {
		LOGE("Fail to do sec_fn_init\n");
		goto err_request_irq;
	}
#endif
	init_completion(&tcm->resume_done);
	complete_all(&tcm->resume_done);

	/* register the interrupt handler */
	retval = syna_dev_request_irq(tcm);
	if (retval < 0) {
		LOGE("Fail to request the interrupt line\n");
		goto err_request_irq;
	}

	/* for the reference,
	 * create a delayed work to perform fw update during the startup time
	 */
#ifdef STARTUP_REFLASH
	tcm->reflash_workqueue =
			create_singlethread_workqueue("syna_reflash");
	INIT_DELAYED_WORK(&tcm->reflash_work, syna_dev_reflash_startup_work);
	queue_delayed_work(tcm->reflash_workqueue, &tcm->reflash_work,
			msecs_to_jiffies(STARTUP_REFLASH_DELAY_TIME_MS));
#endif
#ifdef FORCE_CONNECTION
end:
#endif
	tcm->is_connected = true;

	tcm->lp_state = PWR_ON;

	LOGI("TCM packrat: %d\n", tcm->tcm_dev->packrat_number);
	LOGI("Features: name(%s), lpwg(%s), hw_rst(%s), irq_ctrl(%s)\n",
		PLATFORM_DRIVER_NAME,
		(tcm->lpwg_enabled) ? "yes" : "no",
		(hw_if->ops_hw_reset) ? "yes" : "no",
		(hw_if->ops_enable_irq) ? "yes" : "no");
	LOGI("Features: name(%s), ear_detect(%s)\n",
		PLATFORM_DRIVER_NAME,
		(hw_if->bdata.support_ear_detect) ? "yes" : "no");

	return 0;

err_request_irq:
	/* unregister input device */
	syna_dev_release_input_device(tcm);
#if defined(SEC_NATIVE_EAR_DETECTION)
	syna_dev_release_input_proximity_device(tcm);
#endif

err_setup_input_dev:
#ifdef FORCE_CONNECTION
#else
err_setup_app_fw:
#endif
err_detect_dev:
	if (hw_if->ops_power_on)
		hw_if->ops_power_on(hw_if, false);

	return retval;
}

#ifdef USE_DRM_PANEL_NOTIFIER
static struct drm_panel *syna_dev_get_panel(struct device_node *np)
{
	int i;
	int count;
	struct device_node *node;
	struct drm_panel *panel;

	count = of_count_phandle_with_args(np, "panel", NULL);
	if (count <= 0)
		return NULL;

	for (i = 0; i < count; i++) {
		node = of_parse_phandle(np, "panel", i);
		panel = of_drm_find_panel(node);
		of_node_put(node);
		if (!IS_ERR(panel)) {
			LOGI("Find available panel\n");
			return panel;
		}
	}

	return NULL;
}
#endif

/**
 * syna_dev_probe()
 *
 * Install the TouchComm device driver
 *
 * @param
 *    [ in] pdev: an instance of platform device
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_probe(struct platform_device *pdev)
{
	int retval;
	struct syna_tcm *tcm = NULL;
	struct tcm_dev *tcm_dev = NULL;
	struct syna_hw_interface *hw_if = NULL;
#if defined(USE_DRM_PANEL_NOTIFIER)
	struct device *dev;
#endif

	hw_if = pdev->dev.platform_data;
	if (!hw_if) {
		LOGE("Fail to find hardware configuration\n");
		return -EINVAL;
	}

	tcm = syna_pal_mem_alloc(1, sizeof(struct syna_tcm));
	if (!tcm) {
		LOGE("Fail to create the instance of syna_tcm\n");
		return -ENOMEM;
	}

	/* allocate the TouchCom device handle */
	retval = syna_tcm_allocate_device(&tcm_dev, hw_if);
	if ((retval < 0) || (!tcm_dev)) {
		LOGE("Fail to allocate TouchCom device handle\n");
		goto err_allocate_cdev;
	}

	tcm->tcm_dev = tcm_dev;
	tcm->pdev = pdev;
	tcm->hw_if = hw_if;

	syna_tcm_buf_init(&tcm->event_data);

	syna_pal_mutex_alloc(&tcm->tp_event_mutex);

#ifdef ENABLE_WAKEUP_GESTURE
	tcm->lpwg_enabled = true;
#else
	tcm->lpwg_enabled = false;
#endif
	tcm->irq_wake = false;

	tcm->is_connected = false;

	tcm->dev_connect = syna_dev_connect;
	tcm->dev_disconnect = syna_dev_disconnect;
	tcm->dev_set_up_app_fw = syna_dev_set_up_app_fw;
	tcm->dev_set_normal_sensing = syna_dev_enter_normal_sensing;
	tcm->dev_set_lowpwr_sensing = syna_dev_enter_lowpwr_sensing;
#if defined(SEC_NATIVE_EAR_DETECTION)
	tcm->dev_detect_prox = syna_dev_detect_proximity;
#endif
	tcm->dev_write_sponge = syna_dev_write_to_sponge;
	tcm->dev_read_sponge = syna_dev_read_from_sponge;

	platform_set_drvdata(pdev, tcm);

	device_init_wakeup(&pdev->dev, 1);

#if defined(TCM_CONNECT_IN_PROBE)
	/* connect to target device */
	retval = tcm->dev_connect(tcm);
	if (retval < 0) {
		LOGE("Fail to connect to the device\n");
		syna_pal_mutex_free(&tcm->tp_event_mutex);
		goto err_connect;
	}
#endif

#ifdef CONFIG_TOUCHSCREEN_SYNA_TCM2_SYSFS
	/* create the device file and register to char device classes */
	retval = syna_cdev_create_sysfs(tcm, pdev);
	if (retval < 0) {
		LOGE("Fail to create the device sysfs\n");
		syna_pal_mutex_free(&tcm->tp_event_mutex);
		goto err_create_cdev;
	}
#endif

#if defined(ENABLE_DISP_NOTIFIER)
#if defined(USE_DRM_PANEL_NOTIFIER)
	dev = syna_request_managed_device();
	active_panel = syna_dev_get_panel(dev->of_node);
	if (active_panel) {
		tcm->fb_notifier.notifier_call = syna_dev_fb_notifier_cb;
		retval = drm_panel_notifier_register(active_panel,
				&tcm->fb_notifier);
		if (retval < 0) {
			LOGE("Fail to register FB notifier client\n");
			goto err_create_cdev;
		}
	} else {
		LOGE("No available drm panel\n");
	}
#else
	tcm->fb_notifier.notifier_call = syna_dev_fb_notifier_cb;
	retval = fb_register_client(&tcm->fb_notifier);
	if (retval < 0) {
		LOGE("Fail to register FB notifier client\n");
		goto err_create_cdev;
	}
#endif
#endif

	LOGI("%s TouchComm driver v%d.%d.%d installed\n",
		PLATFORM_DRIVER_NAME,
		(unsigned char)(SYNAPTICS_TCM_DRIVER_VERSION >> 8),
		(unsigned char)SYNAPTICS_TCM_DRIVER_VERSION,
		(unsigned char)SYNAPTICS_TCM_DRIVER_SUBVER);

	return 0;

#ifdef CONFIG_TOUCHSCREEN_SYNA_TCM2_SYSFS
err_create_cdev:
	syna_tcm_remove_device(tcm->tcm_dev);
#endif
#if defined(TCM_CONNECT_IN_PROBE)
	tcm->dev_disconnect(tcm);
err_connect:
#endif
	syna_tcm_buf_release(&tcm->event_data);
	syna_pal_mutex_free(&tcm->tp_event_mutex);
err_allocate_cdev:
	syna_pal_mem_free((void *)tcm);

	return retval;
}

/**
 * syna_dev_remove()
 *
 * Release all allocated resources and remove the TouchCom device handle
 *
 * @param
 *    [ in] pdev: an instance of platform device
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
static int syna_dev_remove(struct platform_device *pdev)
{
	struct syna_tcm *tcm = platform_get_drvdata(pdev);

	if (!tcm) {
		LOGW("Invalid handle to remove\n");
		return 0;
	}

#if defined(ENABLE_DISP_NOTIFIER)
#if defined(USE_DRM_PANEL_NOTIFIER)
	if (active_panel)
		drm_panel_notifier_unregister(active_panel,
				&tcm->fb_notifier);
#else
	fb_unregister_client(&tcm->fb_notifier);
#endif
#endif

#ifdef CONFIG_TOUCHSCREEN_SYNA_TCM2_SYSFS
	/* remove the cdev and sysfs nodes */
	syna_cdev_remove_sysfs(tcm);
#endif
#if defined(TCM_CONNECT_IN_PROBE)
	/* do disconnection */
	if (tcm->dev_disconnect(tcm) < 0)
		LOGE("Fail to do device disconnection\n");
#endif
	syna_tcm_buf_release(&tcm->event_data);

	syna_pal_mutex_free(&tcm->tp_event_mutex);

	/* remove the allocated tcm device */
	syna_tcm_remove_device(tcm->tcm_dev);

	/* release the device context */
	syna_pal_mem_free((void *)tcm);

	return 0;
}

/**
 * syna_dev_shutdown()
 *
 * Call syna_dev_remove() to release all resources
 *
 * @param
 *    [in] pdev: an instance of platform device
 *
 * @return
 *    none.
 */
static void syna_dev_shutdown(struct platform_device *pdev)
{
	syna_dev_remove(pdev);
}


/**
 * Declare a TouchComm platform device
 */
#ifdef CONFIG_PM
static const struct dev_pm_ops syna_dev_pm_ops = {
#if !defined(ENABLE_DISP_NOTIFIER)
	.suspend = syna_dev_suspend,
	.resume = syna_dev_resume,
#endif
};
#endif

static struct platform_driver syna_dev_driver = {
	.driver = {
		.name = PLATFORM_DRIVER_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &syna_dev_pm_ops,
#endif
	},
	.probe = syna_dev_probe,
	.remove = syna_dev_remove,
	.shutdown = syna_dev_shutdown,
};


/**
 * syna_dev_module_init()
 *
 * The entry function of the reference driver, which initialize the
 * lower-level bus and register a platform driver.
 *
 * @param
 *    void.
 *
 * @return
 *    0 if the driver registered and bound to a device,
 *    else returns a negative error code and with the driver not registered.
 */
static int __init syna_dev_module_init(void)
{
	int retval;

	retval = syna_hw_interface_init();
	if (retval < 0)
		return retval;

	return platform_driver_register(&syna_dev_driver);
}

/**
 * syna_dev_module_exit()
 *
 * Function is called when un-installing the driver.
 * Remove the registered platform driver and the associated bus driver.
 *
 * @param
 *    void.
 *
 * @return
 *    none.
 */
static void __exit syna_dev_module_exit(void)
{
	platform_driver_unregister(&syna_dev_driver);

	syna_hw_interface_exit();
}

module_init(syna_dev_module_init);
module_exit(syna_dev_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics TCM Touch Driver");
MODULE_LICENSE("GPL v2");

