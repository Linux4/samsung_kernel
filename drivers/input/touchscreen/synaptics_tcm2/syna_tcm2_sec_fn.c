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
 * @file syna_tcm2_sec_fn.c
 *
 * This file implements the SEC command interface for TCM2 SEC custom driver.
 */

#include "syna_tcm2_sec_fn.h"
#include "synaptics_touchcom_core_dev.h"
#include "synaptics_touchcom_func_base.h"
#ifdef CONFIG_TOUCHSCREEN_SYNA_TCM2_REFLASH
#include "synaptics_touchcom_func_reflash.h"
#endif

#define SEC_FW_IMAGE_NAME "sec/firmware.img"

/* #define SEC_FAC_GROUP */

#define SEC_FN_SYS_ATTRIBUTES_GROUP

/* g_sec_dir represents the root folder of testing sysfs
 */
static struct kobject *g_sec_dir;
static struct syna_tcm *g_tcm_ptr;



static void syna_sec_fn_fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm *tcm = g_tcm_ptr;
	char buff[64] = { 0 };
	int retval = 0;
	const struct firmware *fw_entry = NULL;
	const unsigned char *fw_image = NULL;
	unsigned int fw_image_size = 0;
	int update_type;

	LOGD("start\n");

	sec_cmd_set_default_result(sec);

	if (tcm->lp_state == PWR_OFF) {
		LOGE("Device is powered off\n");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	update_type = sec->cmd_param[0];

	switch (update_type) {
	case 0: /* 0 : [BUILT_IN] Getting firmware which is for user. */
		retval = request_firmware(&fw_entry,
				SEC_FW_IMAGE_NAME,
				tcm->pdev->dev.parent);
		if (retval < 0) {
			LOGE("Fail to request firmware image %s\n",
				SEC_FW_IMAGE_NAME);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto exit;
		}

		fw_image = fw_entry->data;
		fw_image_size = fw_entry->size;
		break;
	case 1:  /* 1 : [UMS] Getting firmware from sd card. */
	case 3:  /* 3 : [FFU] Getting firmware from apk. */
		break;
	default:
		LOGE("Not support command:%d\n", update_type);
		break;
	}

	if (!fw_image || (fw_image_size == 0)) {
		LOGE("Invalid firmare image\n");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	LOGD("Firmware image size = %d\n", fw_image_size);

	retval = syna_tcm_do_fw_update(tcm->tcm_dev,
			fw_image,
			fw_image_size,
			(REFLASH_ERASE_DELAY << 16) | REFLASH_WRITE_DELAY,
			false);
	if (retval < 0) {
		LOGE("Fail to do reflash\n");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	LOGI("Finish firmware update\n");

	/* re-initialize the app fw */
	retval = tcm->dev_set_up_app_fw(tcm);
	if (retval < 0) {
		LOGE("Fail to set up app fw after fw update\n");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	LOGD("done, result: %s\n", buff);
}

static void syna_sec_fn_get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm *tcm = g_tcm_ptr;
	char buff[64] = { 0 };

	sec_cmd_set_default_result(sec);

	if (tcm->lp_state == PWR_OFF) {
		LOGE("Device is powered off\n");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	snprintf(buff, sizeof(buff), "%d", tcm->tcm_dev->packrat_number);

	sec->cmd_state = SEC_CMD_STATUS_OK;
exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	LOGD("done, result: %s\n", buff);
}

static void syna_sec_fn_get_config_ver(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm *tcm = g_tcm_ptr;
	char buff[64] = { 0 };
	int retval = 0;
	int i;

	sec_cmd_set_default_result(sec);

	if (tcm->lp_state == PWR_OFF) {
		LOGE("Device is powered off\n");
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	if (IS_NOT_APP_FW_MODE(tcm->tcm_dev->dev_mode)) {
		LOGE("Device not in application fw mode, current: %02x\n",
			tcm->tcm_dev->dev_mode);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	for (i = 0; i < MAX_SIZE_CONFIG_ID; i++) {
		retval = snprintf(buff + strlen(buff), sizeof(buff),
			"%02x ", tcm->tcm_dev->config_id[i]);
		if (retval < 0) {
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
			goto exit;
		}
	}

	sec->cmd_state = SEC_CMD_STATUS_OK;
exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	LOGD("done, result: %s\n", buff);
}

static void syna_sec_fn_set_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm *tcm = g_tcm_ptr;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retval;
	unsigned char data[8] = {0};
	unsigned short offset;

	LOGD("start\n");

	sec_cmd_set_default_result(sec);

	LOGI("w:%d, h:%d, x:%d, y:%d\n",
		sec->cmd_param[0], sec->cmd_param[1],
		sec->cmd_param[2], sec->cmd_param[3]);

	data[0] = sec->cmd_param[0] & 0xff;
	data[1] = (sec->cmd_param[0] >> 8) & 0xff;
	data[2] = sec->cmd_param[1] & 0xff;
	data[3] = (sec->cmd_param[1] >> 8) & 0xff;
	data[4] = sec->cmd_param[2] & 0xff;
	data[5] = (sec->cmd_param[2] >> 8) & 0xff;
	data[6] = sec->cmd_param[3] & 0xff;
	data[7] = (sec->cmd_param[3] >> 8) & 0xff;

	offset = 2;

	retval = tcm->dev_write_sponge(tcm,
			data, sizeof(data), offset, sizeof(data));
	if (retval < 0) {
		LOGE("Fail to set sponge reg, offset:0x%x\n",
			offset);

		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	LOGD("done, result: %s\n", buff);
}

static void syna_sec_fn_get_aod_rect(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm *tcm = g_tcm_ptr;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retval;
	unsigned char data[8] = {0};
	unsigned short offset;
	unsigned short rect_data[4] = {0};
	int i;

	LOGD("start\n");

	sec_cmd_set_default_result(sec);

	offset = 2;

	retval = tcm->dev_read_sponge(tcm,
			data, sizeof(data), offset, sizeof(data));
	if (retval < 0) {
		LOGE("Fail to set sponge reg, offset:0x%x\n",
			offset);

		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	for (i = 0; i < 4; i++) {
		rect_data[i] =
			(data[i * 2 + 1] & 0xFF) << 8 | (data[i * 2] & 0xFF);
	}

	LOGI("w:%d, h:%d, x:%d, y:%d\n",
		rect_data[0], rect_data[1], rect_data[2], rect_data[3]);

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	LOGD("done, result: %s\n", buff);
}

static int syna_sec_fn_sponge_feature_enable(struct syna_tcm *tcm,
		unsigned char feature, bool en)
{
	int retval;
	unsigned short offset;
	unsigned char data;
	unsigned char data_orig;

	offset = 0;

	retval = tcm->dev_read_sponge(tcm,
			&data, sizeof(data), offset, 1);
	if (retval < 0) {
		LOGE("Fail to read sponge reg, offset:0x%x len:%d\n",
			offset, 1);
		return retval;
	}

	data_orig = data;

	if (en)
		data |= feature;
	else
		data &= ~feature;

	LOGD("sponge data:0x%x (original:0x%x) (enable:%d) \n",
			data, data_orig, en);

	retval = tcm->dev_write_sponge(tcm,
			&data, sizeof(data), offset, 1);
	if (retval < 0) {
		LOGE("Fail to write sponge reg, data:0x%x offset:0x%x\n",
			data, offset);
		return retval;
	}

	return 0;
}

static void syna_sec_fn_aod_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm *tcm = g_tcm_ptr;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retval;
	bool enable;

	LOGD("start\n");

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	enable = (sec->cmd_param[0] == 1);

	retval = syna_sec_fn_sponge_feature_enable(tcm,
			SEC_TS_MODE_SPONGE_AOD, enable);
	if (retval < 0) {
		LOGE("Fail to %s SPONGE_AOD\n",
			(enable) ? "enable" : "disable");

		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	LOGD("done, result: %s\n", buff);
}

static void syna_sec_fn_aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm *tcm = g_tcm_ptr;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retval;
	bool enable;

	LOGD("start\n");

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	enable = (sec->cmd_param[0] == 1);

	retval = syna_sec_fn_sponge_feature_enable(tcm,
			SEC_TS_MODE_SPONGE_DOUBLETAP_TO_WAKEUP, enable);
	if (retval < 0) {
		LOGE("Fail to %s SPONGE_DOUBLETAP_TO_WAKEUP\n",
			(enable) ? "enable" : "disable");

		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	LOGD("done, result: %s\n", buff);
}

static void syna_sec_fn_spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm *tcm = g_tcm_ptr;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retval;
	bool enable;

	LOGD("start\n");

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	enable = (sec->cmd_param[0] == 1);

	retval = syna_sec_fn_sponge_feature_enable(tcm,
			SEC_TS_MODE_SPONGE_SWIPE, enable);
	if (retval < 0) {
		LOGE("Fail to %s SPONGE_SWIPE\n",
			(enable) ? "enable" : "disable");

		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	LOGD("done, result: %s\n", buff);
}

static void syna_sec_fn_singletap_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm *tcm = g_tcm_ptr;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retval;
	bool enable;

	LOGD("start\n");

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	enable = (sec->cmd_param[0] == 1);

	retval = syna_sec_fn_sponge_feature_enable(tcm,
			SEC_TS_MODE_SPONGE_SINGLE_TAP, enable);
	if (retval < 0) {
		LOGE("Fail to %s SPONGE_SINGLE_TAP\n",
			(enable) ? "enable" : "disable");

		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	LOGD("done, result: %s\n", buff);
}

static void syna_sec_fn_ear_detect_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
#if defined(SEC_NATIVE_EAR_DETECTION)
	struct syna_tcm *tcm = g_tcm_ptr;
	int retval = 0;
#endif
	char buff[SEC_CMD_STR_LEN] = { 0 };

	LOGD("start\n");

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

#if defined(SEC_NATIVE_EAR_DETECTION)
	retval = tcm->dev_detect_prox(tcm,
			(bool)sec->cmd_param[0]);
	if (retval < 0) {
		LOGE("Fail to enable ear detect mode, %d\n",
			sec->cmd_param[0]);

		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}
#else
	LOGN("Ear detection is disabled\n");
#endif

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	LOGD("done, result: %s\n", buff);

}

static void syna_sec_fn_run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm *tcm = g_tcm_ptr;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retval;

	LOGD("start\n");

	sec_cmd_set_default_result(sec);

	retval = syna_sec_fn_test_get_proximity(tcm);
	LOGI("sum = %d\n", retval);

	if (retval == -1) {
		snprintf(buff, sizeof(buff), "%s", "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	snprintf(buff, sizeof(buff), "SUM_X:%d THD_X:%d", retval, 0);
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	LOGD("done, result: %s\n", buff);
}

static void syna_sec_fn_not_support_cmd(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "NA");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_NOT_APPLICABLE;
	sec_cmd_set_cmd_exit(sec);
}

static struct sec_cmd sec_cmds[] = {
	{SEC_CMD("fw_update", syna_sec_fn_fw_update),},
	{SEC_CMD("get_fw_ver_ic", syna_sec_fn_get_fw_ver_ic),},
	{SEC_CMD("get_config_ver", syna_sec_fn_get_config_ver),},
	{SEC_CMD("set_aod_rect", syna_sec_fn_set_aod_rect),},
	{SEC_CMD("get_aod_rect", syna_sec_fn_get_aod_rect),},
	{SEC_CMD_H("aod_enable", syna_sec_fn_aod_enable),},
	{SEC_CMD_H("aot_enable", syna_sec_fn_aot_enable),},
	{SEC_CMD_H("spay_enable", syna_sec_fn_spay_enable),},
	{SEC_CMD_H("singletap_enable", syna_sec_fn_singletap_enable),},
	{SEC_CMD_H("ear_detect_enable", syna_sec_fn_ear_detect_enable),},
	{SEC_CMD("run_prox_intensity_read_all",
		syna_sec_fn_run_prox_intensity_read_all),},
	{SEC_CMD("not_support_cmd", syna_sec_fn_not_support_cmd),},
};

#if defined(SEC_FN_SYS_ATTRIBUTES_GROUP)
/** get_fw_ver_ic **/
static ssize_t syna_sec_fn_get_fw_ver_ic_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct syna_tcm *tcm = g_tcm_ptr;

	syna_sec_fn_get_fw_ver_ic((void *)&tcm->sec);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", tcm->sec.cmd_state);
}

static struct kobj_attribute kobj_attr_get_fw_ver_ic =
	__ATTR(get_fw_ver_ic, 0444,
		syna_sec_fn_get_fw_ver_ic_show, NULL);

/** get_config_ver **/
static ssize_t syna_sec_fn_get_config_ver_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct syna_tcm *tcm = g_tcm_ptr;

	syna_sec_fn_get_config_ver((void *)&tcm->sec);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", tcm->sec.cmd_state);
}

static struct kobj_attribute kobj_attr_get_config_ver =
	__ATTR(get_config_ver, 0444,
		syna_sec_fn_get_config_ver_show, NULL);

/** aod_enable **/
static ssize_t syna_sec_fn_aod_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct syna_tcm *tcm = g_tcm_ptr;
	long data;
	int retval;

	retval = kstrtol(buf, 10, &data);
	if (retval < 0)
		return retval;

	tcm->sec.cmd_param[0] = data;

	syna_sec_fn_aod_enable((void *)&tcm->sec);

	return count;
}

static struct kobj_attribute kobj_attr_aod_enable =
	__ATTR(aod_enable, 0220, NULL, syna_sec_fn_aod_enable_store);

/** aot_enable **/
static ssize_t syna_sec_fn_aot_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct syna_tcm *tcm = g_tcm_ptr;
	long data;
	int retval;

	retval = kstrtol(buf, 10, &data);
	if (retval < 0)
		return retval;

	tcm->sec.cmd_param[0] = data;

	syna_sec_fn_aot_enable((void *)&tcm->sec);

	return count;
}

static struct kobj_attribute kobj_attr_aot_enable =
	__ATTR(aot_enable, 0220, NULL, syna_sec_fn_aot_enable_store);

/** spay_enable **/
static ssize_t syna_sec_fn_spay_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct syna_tcm *tcm = g_tcm_ptr;
	long data;
	int retval;

	retval = kstrtol(buf, 10, &data);
	if (retval < 0)
		return retval;

	tcm->sec.cmd_param[0] = data;

	syna_sec_fn_spay_enable((void *)&tcm->sec);

	return count;
}

static struct kobj_attribute kobj_attr_spay_enable =
	__ATTR(spay_enable, 0220, NULL, syna_sec_fn_spay_enable_store);

/** singletap_enable **/
static ssize_t syna_sec_fn_singletap_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct syna_tcm *tcm = g_tcm_ptr;
	long data;
	int retval;

	retval = kstrtol(buf, 10, &data);
	if (retval < 0)
		return retval;

	tcm->sec.cmd_param[0] = data;

	syna_sec_fn_singletap_enable((void *)&tcm->sec);

	return count;
}

static struct kobj_attribute kobj_attr_singletap_enable =
	__ATTR(singletap_enable, 0220, NULL, syna_sec_fn_singletap_enable_store);

/** ear_detect_enable **/
static ssize_t syna_sec_fn_ear_detect_enable_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct syna_tcm *tcm = g_tcm_ptr;
	long data;
	int retval;

	retval = kstrtol(buf, 10, &data);
	if (retval < 0)
		return retval;

	tcm->sec.cmd_param[0] = data;

	syna_sec_fn_ear_detect_enable((void *)&tcm->sec);

	return count;
}

static struct kobj_attribute kobj_attr_ear_detect_enable =
	__ATTR(ear_detect_enable, 0220, NULL, syna_sec_fn_ear_detect_enable_store);

/** run_prox_intensity_read_all **/
static ssize_t syna_sec_fn_run_prox_intensity_read_all_show(
		struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct syna_tcm *tcm = g_tcm_ptr;

	syna_sec_fn_run_prox_intensity_read_all((void *)&tcm->sec);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", tcm->sec.cmd_state);
}

static struct kobj_attribute kobj_attr_run_prox_intensity_read_all =
	__ATTR(run_prox_intensity_read_all, 0444,
		syna_sec_fn_run_prox_intensity_read_all_show, NULL);

/** prox_power_off **/
static ssize_t syna_sec_fn_prox_power_off_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct syna_tcm *tcm = g_tcm_ptr;

	LOGI("Prox_power_off: %ld\n", tcm->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%ld", tcm->prox_power_off);
}

static ssize_t syna_sec_fn_prox_power_off_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	struct syna_tcm *tcm = g_tcm_ptr;
	long data;
	int retval;

	retval = kstrtol(buf, 10, &data);
	if (retval < 0)
		return retval;

	tcm->prox_power_off = data;

	LOGD("set prox_power_off: %ld\n", tcm->prox_power_off);

	return count;
}

static struct kobj_attribute kobj_attr_prox_power_off =
	__ATTR(prox_power_off, 0664,
		syna_sec_fn_prox_power_off_show,
		syna_sec_fn_prox_power_off_store);

/** virtual_prox **/
static ssize_t syna_sec_fn_virtual_prox_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct syna_tcm *tcm = g_tcm_ptr;

	LOGD("Hover_event: %d\n", tcm->hover_event);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d",
		tcm->hover_event != 3 ? 0 : 3);
}

static ssize_t syna_sec_fn_virtual_prox_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
#if defined(SEC_NATIVE_EAR_DETECTION)
	struct syna_tcm *tcm = g_tcm_ptr;
#endif
	unsigned char data;
	int retval;

	retval = kstrtou8(buf, 10, &data);
	if (retval < 0)
		return retval;

	if (data != 0 && data != 1) {
		LOGE("incorrect data\n");
		return -EINVAL;
	}

#if defined(SEC_NATIVE_EAR_DETECTION)
	retval = tcm->dev_detect_prox(tcm, (bool)data);
	if (retval < 0) {
		LOGE("Failed to enable ear detect mode\n");
		return retval;
	}
#else
	LOGN("Ear detection is disabled\n");
#endif

	return count;
}

static struct kobj_attribute kobj_attr_virtual_prox =
	__ATTR(virtual_prox, 0664,
		syna_sec_fn_virtual_prox_show,
		syna_sec_fn_virtual_prox_store);

/** lp_dump **/
static ssize_t syna_sec_fn_get_lp_dump_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct syna_tcm *tcm = g_tcm_ptr;
	int retval;
	unsigned int count = 0;
	unsigned short offset;
	unsigned char out[10] = {0};
	unsigned char dump_format;
	unsigned char dump_num;
	unsigned short dump_start;
	unsigned short dump_end;
	unsigned short current_index;
	int i;
	unsigned short data0, data1, data2, data3, data4;

	offset = SEC_TS_CMD_SPONGE_LP_DUMP;

	retval = tcm->dev_read_sponge(tcm,
			out, sizeof(out), offset, 4);
	if (retval < 0) {
		LOGE("Fail to read sponge reg, offset:0x%x len:%d\n",
			offset, 4);

		retval = snprintf(buf, PAGE_SIZE,
			"Fail to read sponge reg, offset:0x%x len:%d\n",
			offset, 4);

		return retval;
	}

	dump_format = out[0];
	dump_num = out[1];
	dump_start = SEC_TS_CMD_SPONGE_LP_DUMP + 4;
	dump_end = dump_start + (dump_format * (dump_num - 1));

	current_index = (out[3] & 0xFF) << 8 | (out[2] & 0xFF);

	LOGD("Dump format=%d num=%d start=%d end=%d index=%d\n",
			dump_format, dump_num, dump_start, dump_end, current_index);

	if (current_index > dump_end || current_index < dump_start) {
		LOGE("Fail to sponge lp log index=%d\n", current_index);

		retval = snprintf(buf, PAGE_SIZE - count,
				"Fail to sponge lp log index=%d\n", current_index);

		buf += retval;
		count += retval;

		goto exit;
	}

	for (i = dump_num - 1 ; i >= 0 ; i--) {
		if (current_index < (dump_format * i))
			offset = (dump_format * dump_num) + current_index - (dump_format * i);
		else
			offset = current_index - (dump_format * i);

		if (offset < dump_start)
			offset += (dump_format * dump_num);

		retval = tcm->dev_read_sponge(tcm,
				out, sizeof(out), offset, dump_format);
		if (retval < 0) {
			LOGE("Fail to read sponge reg, offset:0x%x len:%d\n",
				offset, dump_format);

			retval = snprintf(buf, PAGE_SIZE - count,
					"Fail to read sponge reg, offset:0x%x len:%d\n",
					offset, dump_format);

			buf += retval;
			count += retval;

			goto exit;
		}

		data0 = (out[1] & 0xFF) << 8 | (out[0] & 0xFF);
		data1 = (out[3] & 0xFF) << 8 | (out[2] & 0xFF);
		data2 = (out[5] & 0xFF) << 8 | (out[4] & 0xFF);
		data3 = (out[7] & 0xFF) << 8 | (out[6] & 0xFF);
		data4 = (out[9] & 0xFF) << 8 | (out[8] & 0xFF);

		if (data0 || data1 || data2 || data3 || data4) {
			if (dump_format == 10) {
				retval = snprintf(buf, PAGE_SIZE - count,
						"%d: %04x%04x%04x%04x%04x\n\n",
						offset, data0, data1, data2, data3, data4);

			}
			else {
				retval = snprintf(buf, PAGE_SIZE - count,
						"%d: %04x%04x%04x%04x\n\n",
						offset, data0, data1, data2, data3);
			}
			buf += retval;
			count += retval;
		}
	}

exit:
	retval = count;

	return retval;
}

static struct kobj_attribute kobj_attr_lp_dump =
	__ATTR(lp_dump, 0444,
		syna_sec_fn_get_lp_dump_show, NULL);

static struct attribute *sec_fn_attributes[] = {
	&kobj_attr_get_fw_ver_ic.attr,
	&kobj_attr_get_config_ver.attr,
	&kobj_attr_aod_enable.attr,
	&kobj_attr_aot_enable.attr,
	&kobj_attr_spay_enable.attr,
	&kobj_attr_singletap_enable.attr,
	&kobj_attr_ear_detect_enable.attr,
	&kobj_attr_run_prox_intensity_read_all.attr,
	&kobj_attr_prox_power_off.attr,
	&kobj_attr_virtual_prox.attr,
	&kobj_attr_lp_dump.attr,
	NULL,
};

static struct attribute_group sec_fn_attr_group = {
	.attrs = sec_fn_attributes,
};
#endif

/**
 * syna_sec_fn_create_dir()
 *
 * Create a directory and register with sysfs.
 *
 * @param
 *    [ in] tcm:  the driver handle
 *    [ in] sysfs_dir: root directory of sysfs nodes
 *
 * @return
 *    on success, 0; otherwise, negative value on error.
 */
int syna_sec_fn_create_dir(struct syna_tcm *tcm,
		struct kobject *sysfs_dir)
{
#if defined(SEC_FN_SYS_ATTRIBUTES_GROUP)
	int retval = 0;

	g_sec_dir = kobject_create_and_add("sec_fn",
			sysfs_dir);
	if (!g_sec_dir) {
		LOGE("Fail to create sec testing directory\n");
		return -EINVAL;
	}

	retval = sysfs_create_group(g_sec_dir, &sec_fn_attr_group);
	if (retval < 0) {
		LOGE("Fail to create sysfs group\n");

		kobject_put(g_sec_dir);
		return retval;
	}

	g_tcm_ptr = tcm;
#endif
	return 0;
}

int syna_sec_fn_init(struct syna_tcm *tcm)
{
	int retval = 0;

	LOGD("start\n");

	retval = sec_cmd_init(&tcm->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		LOGE("Failed to sec_cmd_init\n");
		return retval;
	}

#ifdef SEC_FAC_GROUP
	/* sec.fac group */
	retval = sysfs_create_group(&tcm->sec.fac_dev->kobj,
			&sec_fn_attr_group);
	if (retval < 0) {
		LOGE("Fail to create sec.fac sysfs group\n");
		goto err_sec_fac_group;
	}

	retval = sysfs_create_link(&tcm->sec.fac_dev->kobj,
			&tcm->input_dev->dev.kobj, "input");
	if (retval < 0) {
		LOGE("Failed to create input symbolic link\n");
		goto err_link_create;
	}
#endif
	g_tcm_ptr = tcm;

	LOGD("done\n");

	return 0;

#ifdef SEC_FAC_GROUP
err_link_create:
	sysfs_remove_group(&tcm->sec.fac_dev->kobj,
		&sec_fn_attr_group);
err_sec_fac_group:
#endif

	return retval;
}

void syna_sec_fn_remove(struct syna_tcm *tcm)
{
	LOGD("start\n");

#ifdef SEC_FAC_GROUP
	sysfs_delete_link(&tcm->sec.fac_dev->kobj,
			&tcm->input_dev->dev.kobj, "input");

	sysfs_remove_group(&tcm->sec.fac_dev->kobj,
		&sec_fn_attr_group);
#endif

#if defined(SEC_FN_SYS_ATTRIBUTES_GROUP)
	if (g_sec_dir) {
		sysfs_remove_group(g_sec_dir, &sec_fn_attr_group);
		kobject_put(g_sec_dir);
	}
#endif

	sec_cmd_exit(&tcm->sec, SEC_CLASS_DEVT_TSP);

	LOGD("done\n");
}

