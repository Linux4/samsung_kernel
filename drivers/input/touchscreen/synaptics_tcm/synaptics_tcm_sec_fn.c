/*
 * Synaptics TCM touchscreen driver
 *
 * Copyright (C) 2017-2018 Synaptics Incorporated. All rights reserved.
 *
 * Copyright (C) 2017-2018 Scott Lin <scott.lin@tw.synaptics.com>
 * Copyright (C) 2018-2019 Ian Su <ian.su@tw.synaptics.com>
 * Copyright (C) 2018-2019 Joey Zhou <joey.zhou@synaptics.com>
 * Copyright (C) 2018-2019 Yuehao Qiu <yuehao.qiu@synaptics.com>
 * Copyright (C) 2018-2019 Aaron Chen <aaron.chen@tw.synaptics.com>
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

#include "synaptics_tcm_sec_fn.h"

#define TSP_PATH_EXTERNAL_FW		"/sdcard/Firmware/TSP/tsp.bin"
#define TSP_PATH_EXTERNAL_FW_SIGNED	"/sdcard/Firmware/TSP/tsp_signed.bin"
#define TSP_PATH_SPU_FW_SIGNED		"/spu/TSP/ffu_tsp.bin"

enum {
	BUILT_IN = 0,
	UMS,
	BL,
	FFU,
};

static int sec_fn_load_fw(struct syna_tcm_hcd *tcm_hcd, const char *file_path)
{
	struct file *fp;
	mm_segment_t old_fs;
	int fw_size, nread;
	int error = 0;
	unsigned char *fw_data;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(file_path, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		LOGE(tcm_hcd->pdev->dev.parent, "%s: failed to open %s\n", __func__, file_path);
		error = -ENOENT;
		goto open_err;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;

	if (fw_size > 0) {
		fw_data = vzalloc(fw_size);
		if (!fw_data) {
			LOGE(tcm_hcd->pdev->dev.parent, "%s: failed to alloc mem\n", __func__);
			error = -ENOMEM;
			filp_close(fp, NULL);
			goto open_err;
		}
		nread = vfs_read(fp, (char __user *)fw_data,
			fw_size, &fp->f_pos);

		LOGI(tcm_hcd->pdev->dev.parent, "%s: start, file path %s, size %ld Bytes\n",
				__func__, file_path, fw_size);

		if (nread != fw_size) {
			LOGE(tcm_hcd->pdev->dev.parent, "%s: failed to read firmware file, nread %u Bytes\n",
				__func__, nread);
			error = -EIO;
			vfree(fw_data);
			filp_close(fp, current->files);
			goto open_err;
		}

		tcm_hcd->image = kzalloc(fw_size, GFP_KERNEL);
		if (!tcm_hcd->image) {
			LOGE(tcm_hcd->pdev->dev.parent, "%s: failed to alloc tcm_hcd->image mem\n", __func__);
			error = -ENOMEM;
			vfree(fw_data);
			filp_close(fp, current->files);
			goto open_err;
		}

		memcpy(tcm_hcd->image, fw_data, fw_size);

		vfree(fw_data);
	}

	filp_close(fp, current->files);

 open_err:
	set_fs(old_fs);
	return error;
}

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);
	char buff[64] = { 0 };
	int retval = 0;

	sec_cmd_set_default_result(sec);

	/* Factory cmd for firmware update
	 * argument represent what is source of firmware like below.
	 *
	 * 0 : [BUILT_IN] Getting firmware which is for user.
	 * 1 : [UMS] Getting firmware from sd card.
	 * 2 : none
	 * 3 : [FFU] Getting firmware from apk.
	 */

	tcm_hcd->force_update = true;

	switch (sec->cmd_param[0]) {
	case BUILT_IN:
		tcm_hcd->get_fw = BUILT_IN;
		break;
	case UMS:
		tcm_hcd->get_fw = UMS;
		retval = sec_fn_load_fw(tcm_hcd, TSP_PATH_EXTERNAL_FW);
		if (retval < 0) {
			tcm_hcd->force_update = false;
			tcm_hcd->get_fw = BUILT_IN;
		}
		break;
	default:
		break;
	}

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

}
static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);
	char buff[16] = { 0 };

	strncpy(buff, "SYNAPTICS", sizeof(buff));
	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	LOGI(tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);
	char buff[16] = { 0 };

	strncpy(buff, "TD4150", sizeof(buff));

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	LOGI(tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	
	snprintf(buff, sizeof(buff), "SY%02X%02X%02X%02X",
			tcm_hcd->img_version[0], tcm_hcd->img_version[1], tcm_hcd->img_version[2], tcm_hcd->img_version[3]); 

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;
 
	LOGI(tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);
	char buff[16] = { 0 };
	char model[16] = { 0 };

	sec_cmd_set_default_result(sec);
	
	snprintf(buff, sizeof(buff), "SY%02X%02X%02X%02X",
			tcm_hcd->app_info.customer_config_id[0], tcm_hcd->app_info.customer_config_id[1],
			tcm_hcd->app_info.customer_config_id[2], tcm_hcd->app_info.customer_config_id[3]);
	snprintf(model, sizeof(model), "SY%02X%02X",
		tcm_hcd->app_info.customer_config_id[0], tcm_hcd->app_info.customer_config_id[1]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;

	LOGI(tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", tcm_hcd->cols);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	LOGI(tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", tcm_hcd->rows);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	LOGI(tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
}

static void run_dynamic_range_test_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct sec_factory_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct sec_factory_test_mode));

	test_abs_cap(sec, &mode);

}

static void run_dynamic_range_test_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct sec_factory_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct sec_factory_test_mode));
	mode.allnode = TEST_MODE_ALL_NODE;

	test_abs_cap(sec, &mode);
}

static void run_open_short_test_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct sec_factory_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct sec_factory_test_mode));

	test_open_short(sec, &mode);
}

static void run_open_short_test_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct sec_factory_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct sec_factory_test_mode));
	mode.allnode = TEST_MODE_ALL_NODE;

	test_open_short(sec, &mode);
}

static void run_noise_test_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct sec_factory_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct sec_factory_test_mode));

	test_noise(sec, &mode);
}

static void run_noise_test_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct sec_factory_test_mode mode;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct sec_factory_test_mode));
	mode.allnode = TEST_MODE_ALL_NODE;

	test_noise(sec, &mode);
}

static void get_gap_data_x_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct sec_factory_test_mode mode;

	sec_cmd_set_default_result(sec);
	memset(&mode, 0x00, sizeof(struct sec_factory_test_mode));
	mode.allnode = TEST_MODE_ALL_NODE;

	raw_data_gap_x(sec, &mode);

}

static void get_gap_data_y_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct sec_factory_test_mode mode;

	sec_cmd_set_default_result(sec);
	memset(&mode, 0x00, sizeof(struct sec_factory_test_mode));
	mode.allnode = TEST_MODE_ALL_NODE;

	raw_data_gap_y(sec, &mode);

}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	run_dynamic_range_test_read(sec);
	run_open_short_test_read(sec);
	run_noise_test_read(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	LOGI(tcm_hcd->pdev->dev.parent, "%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	sec_cmd_set_default_result(sec);

	test_check_connection(sec);

}

static void get_crc_check(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	sec_cmd_set_default_result(sec);

	test_fw_crc(sec);
}

static void not_support_cmd(void *device_data)
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
	{SEC_CMD("fw_update", fw_update),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("run_dynamic_range_test_read", run_dynamic_range_test_read),},
	{SEC_CMD("run_dynamic_range_test_read_all", run_dynamic_range_test_read_all),},
	{SEC_CMD("run_open_short_test_read", run_open_short_test_read),},
	{SEC_CMD("run_open_short_test_read_all", run_open_short_test_read_all),},
	{SEC_CMD("run_noise_test_read", run_noise_test_read),},
	{SEC_CMD("run_noise_test_read_all", run_noise_test_read_all),},
	{SEC_CMD("get_gap_data_x_all", get_gap_data_x_all),},
	{SEC_CMD("get_gap_data_y_all", get_gap_data_y_all),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("check_connection", check_connection),},
	{SEC_CMD("get_crc_check", get_crc_check),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);
	int i;
	char tempv[10] = { 0 };
	char buff[SENSITIVITY_POINT_CNT * 10] = { 0 };

	test_sensitivity();

	for (i = 0; i < SENSITIVITY_POINT_CNT; i++) {
		if (i != 0)
			strlcat(buff, ",", sizeof(buff));
		snprintf(tempv, 10, "%d", tcm_hcd->pFrame[i]);
		strlcat(buff, tempv, sizeof(buff));
	}

	LOGI(tcm_hcd->pdev->dev.parent, "%s: sensitivity mode : %s\n", __func__, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, buff);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	return count;
}

static DEVICE_ATTR(sensitivity_mode, 0664, sensitivity_mode_show, sensitivity_mode_store);

static struct attribute *cmd_attributes[] = {
	&dev_attr_sensitivity_mode.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

int sec_fn_init(struct syna_tcm_hcd *tcm_hcd)
{
	int retval;

	retval = test_init(tcm_hcd);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
						"%s: test_init\n", __func__);
		goto exit;
	}

	retval = sec_cmd_init(&tcm_hcd->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
						"%s: Failed to sec_cmd_init\n", __func__);
		goto exit;
	}

	retval = sysfs_create_group(&tcm_hcd->sec.fac_dev->kobj,
			&cmd_attr_group);
	if (retval < 0) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"%s: Failed to create sysfs attributes\n", __func__);
		goto exit;
	}

	tcm_hcd->print_buf = kzalloc(sizeof(u8) * 4096, GFP_KERNEL);
	if (!tcm_hcd->print_buf) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"%s: Failed to alloc memory print_buf\n", __func__);
		retval = -EINVAL;
		goto exit;
	}

	tcm_hcd->pFrame = kzalloc(tcm_hcd->rows * tcm_hcd->cols * 8, GFP_KERNEL);
	if (!tcm_hcd->pFrame) {
		LOGE(tcm_hcd->pdev->dev.parent,
				"%s: Failed to alloc memory pFrame\n", __func__);
		retval = -EINVAL;
		goto error_alloc_pFram;
	}

	return 0;

error_alloc_pFram:
	kfree(tcm_hcd->print_buf);
exit:
	return retval;
}

void sec_fn_remove(struct syna_tcm_hcd *tcm_hcd)
{
	LOGI(tcm_hcd->pdev->dev.parent, "%s\n", __func__);

	kfree(tcm_hcd->pFrame);

	sysfs_remove_group(&tcm_hcd->sec.fac_dev->kobj,
			&cmd_attr_group);

	sec_cmd_exit(&tcm_hcd->sec, SEC_CLASS_DEVT_TSP);

	return;
}
