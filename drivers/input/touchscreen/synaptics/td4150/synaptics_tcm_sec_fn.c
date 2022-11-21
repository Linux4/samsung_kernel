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
//#define TSP_PATH_SPU_FW_SIGNED		"/spu/TSP/ffu_tsp.bin"

enum {
	BUILT_IN = 0,
	UMS,
	BL,
	FFU,
};

void sec_run_rawdata(struct syna_tcm_hcd *tcm_hcd)
{
	struct sec_factory_test_mode mode;
	
	memset(&mode, 0x00, sizeof(struct sec_factory_test_mode));
	tcm_hcd->tsp_dump_lock = true;

	input_raw_data_clear();

	input_raw_info(true, tcm_hcd->pdev->dev.parent, "%s: start ##\n", __func__);

	if (run_test(tcm_hcd, &mode, TEST_PT7_DYNAMIC_RANGE)) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s cp error\n", __func__);
		goto out;
	}

	if (run_test(tcm_hcd, &mode, TEST_PT11_OPEN_DETECTION)) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s cp short error\n", __func__);
		goto out;
	}

	if (run_test(tcm_hcd, &mode, TEST_PT10_DELTA_NOISE)) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s cp lpm error\n", __func__);
		goto out;
	}

out:
	input_raw_info(true, tcm_hcd->pdev->dev.parent, "%s: done ##\n", __func__);
	tcm_hcd->tsp_dump_lock = false;
}

//extern int long spu_firmware_signature_verify(const char* fw_name, const u8* fw_data, const long fw_size);

static int sec_fn_load_fw(struct syna_tcm_hcd *tcm_hcd,  bool signing, const char *file_path)
{
	struct file *fp;
	mm_segment_t old_fs;
	int fw_size, nread;
	int error = 0;
	unsigned char *fw_data;
//	size_t spu_fw_size;
	//size_t spu_ret = 0;

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	fp = filp_open(file_path, O_RDONLY, S_IRUSR);
	if (IS_ERR(fp)) {
		input_err(true, tcm_hcd->pdev->dev.parent,
			"%s: failed to open %s\n", __func__,file_path);
		set_fs(old_fs);
		return error;
	}

	fw_size = fp->f_path.dentry->d_inode->i_size;
#if 0
	if (signing) {
		/* name 3, digest 32, signature 512 */
		spu_fw_size = fw_size;
		fw_size -= SPU_METADATA_SIZE(TSP);
	}
#endif
	if (fw_size > 0) {
		fw_data = vzalloc(fw_size);
		if (!fw_data) {
			input_err(true, tcm_hcd->pdev->dev.parent, "%s: failed to alloc mem\n", __func__);
			error = -ENOMEM;
			goto open_err;
		}
#if 0
		if (signing) {
			unsigned char *spu_fw_data;

			spu_fw_data = vzalloc(spu_fw_size);
			if (!spu_fw_data) {
				input_err(true, tcm_hcd->pdev->dev.parent, "%s: failed to alloc mem for spu\n", __func__);
				error = -ENOMEM;
				vfree(fw_data);
				goto open_err;
			}			
			nread = vfs_read(fp, (char __user *)spu_fw_data, spu_fw_size, &fp->f_pos);

			input_info(true, tcm_hcd->pdev->dev.parent,
				"%s: start, file path %s, size %ld Bytes\n", __func__, file_path, spu_fw_size);

			if (nread != spu_fw_size) {
				input_err(true, tcm_hcd->pdev->dev.parent,
					"%s: failed to read firmware file, nread %u Bytes\n",
					__func__, nread);
				error = -EIO;
				vfree(fw_data);
				vfree(spu_fw_data);
				goto open_err;
			}

			spu_ret = spu_firmware_signature_verify("TSP", spu_fw_data, spu_fw_size);
			if (spu_ret != fw_size) {
				input_err(true, tcm_hcd->pdev->dev.parent, "%s: signature verify failed, %zu\n",
						__func__, spu_ret);
				error = -EINVAL;
				vfree(spu_fw_data);
				vfree(fw_data);
				goto open_err;
			}

			memcpy(fw_data, spu_fw_data, fw_size);
			vfree(spu_fw_data);

		} else {
#endif
			nread = vfs_read(fp, (char __user *)fw_data,
				fw_size, &fp->f_pos);

			input_info(true, tcm_hcd->pdev->dev.parent,
				"%s: start, file path %s, size %ld Bytes\n", __func__, file_path, fw_size);

			if (nread != fw_size) {
				input_err(true, tcm_hcd->pdev->dev.parent,
					"%s: failed to read firmware file, nread %u Bytes\n",
					__func__, nread);
				error = -EIO;
				vfree(fw_data);
				goto open_err;
			}
//		}

		if (tcm_hcd->image)
			vfree(tcm_hcd->image);

		tcm_hcd->image = vzalloc(fw_size);
		if (!tcm_hcd->image) {
			input_err(true, tcm_hcd->pdev->dev.parent,
				"%s: failed to alloc tcm_hcd->image mem\n", __func__);
			error = -ENOMEM;
			vfree(fw_data);
			goto open_err;
		}

		memcpy(tcm_hcd->image, fw_data, fw_size);

		vfree(fw_data);
	}

open_err:
	filp_close(fp, current->files);
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
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
		retval = sec_fn_load_fw(tcm_hcd, SIGNING, TSP_PATH_EXTERNAL_FW_SIGNED);
#else
		retval = sec_fn_load_fw(tcm_hcd, NORMAL, TSP_PATH_EXTERNAL_FW);
#endif
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
	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
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
	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	
	snprintf(buff, sizeof(buff), "SY%02X%02X%02X%02X",
			tcm_hcd->img_version[0], tcm_hcd->img_version[1],
			tcm_hcd->img_version[2],tcm_hcd->img_version[3]); 

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;
 
	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
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

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
}

static void get_threshold(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);
	char buff[16] = { 0 };

	strncpy(buff, "120", sizeof(buff)); /* for a21s */

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
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
	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
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
	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, buff);
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

	input_info(true, tcm_hcd->pdev->dev.parent,
		"%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
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

static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);
	int retval;
	unsigned char out_buf[3] = { 0 }; 
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	resp_buf = NULL;
	resp_buf_size = 0;

	if (tcm_hcd->in_suspend) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: power off\n", __func__);
		goto exit;
	}

	if (sec->cmd_param[0] == 2) {	// landscape mode
		if (sec->cmd_param[1] == 0) {	// normal mode 
			out_buf[0] = (unsigned char)sec->cmd_param[1];
			out_buf[1] = 0;
			out_buf[2] = 0;
		} else if (sec->cmd_param[1] == 1) {
			out_buf[0] = (unsigned char)sec->cmd_param[1];
			out_buf[1] = (unsigned char)sec->cmd_param[4];
			out_buf[2] = (unsigned char)sec->cmd_param[5];
		} else {
			input_err(true, tcm_hcd->pdev->dev.parent, "%s: cmd1 is abnormal, %d (%d)\n",
					__func__, sec->cmd_param[1], __LINE__);
			goto exit;
		}
	}

	retval = tcm_hcd->write_message(tcm_hcd, CMD_SET_GRIP_DATA,
				out_buf, sizeof(out_buf), &resp_buf,
				&resp_buf_size, &resp_length, NULL, 0);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
			"Failed to write command %s\n", STR(CMD_SET_DYNAMIC_CONFIG));
		goto exit;
	}

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
	return;

exit:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	return;
}

#if !defined(CONFIG_SEC_FACTORY)
static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct syna_tcm_hcd *tcm_hcd = container_of(sec, struct syna_tcm_hcd, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		ret = tcm_hcd->set_dynamic_config(tcm_hcd, DC_ENABLE_DEADZONE,
			sec->cmd_param[0]);
		if (ret < 0) {
			input_err(true, tcm_hcd->pdev->dev.parent,
					"Failed to enable dead zone\n");
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else {
			snprintf(buff, sizeof(buff), "OK");
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}
#endif
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
	{SEC_CMD("get_threshold", get_threshold),},
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
	{SEC_CMD("set_grip_data", set_grip_data),},
#if !defined(CONFIG_SEC_FACTORY)	
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
#endif
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

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: sensitivity mode : %s\n", __func__, buff);

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
		input_err(true, tcm_hcd->pdev->dev.parent,
						"%s: test_init\n", __func__);
		goto exit;
	}

	retval = sec_cmd_init(&tcm_hcd->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
						"%s: Failed to sec_cmd_init\n", __func__);
		goto exit;
	}

	retval = sysfs_create_group(&tcm_hcd->sec.fac_dev->kobj,
			&cmd_attr_group);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"%s: Failed to create sysfs attributes\n", __func__);
		goto exit;
	}

	tcm_hcd->print_buf = kzalloc(tcm_hcd->cols * tcm_hcd->rows * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!tcm_hcd->print_buf) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"%s: Failed to alloc memory print_buf\n", __func__);
		retval = -EINVAL;
		goto exit;
	}

	tcm_hcd->pFrame = kzalloc(tcm_hcd->cols * tcm_hcd->rows * sizeof(short), GFP_KERNEL);
	if (!tcm_hcd->pFrame) {
		input_err(true, tcm_hcd->pdev->dev.parent,
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
	input_info(true, tcm_hcd->pdev->dev.parent, "%s\n", __func__);

	kfree(tcm_hcd->pFrame);

	sysfs_remove_group(&tcm_hcd->sec.fac_dev->kobj,
			&cmd_attr_group);

	sec_cmd_exit(&tcm_hcd->sec, SEC_CLASS_DEVT_TSP);

	return;
}
