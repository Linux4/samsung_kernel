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

#include "ovt_tcm_sec_fn.h"

struct ovt_tcm_hcd *private_tcm_hcd;

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int otg_flag;
#endif
#endif

enum {
	BUILT_IN = 0,
	UMS,
	BL,
	FFU,
};

void sec_run_rawdata(struct ovt_tcm_hcd *tcm_hcd)
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
#ifdef SUPPORT_FW_SIGNED
extern int long spu_firmware_signature_verify(const char* fw_name, const u8* fw_data, const long fw_size);
#endif

static int sec_fn_load_fw(struct ovt_tcm_hcd *tcm_hcd,  bool signing, const char *file_path)
{
	const struct firmware *fw_entry = NULL;
	long fw_size;
	int error = 0;
	unsigned char *fw_data;
	size_t spu_fw_size;
	size_t spu_ret = 0;

	error = request_firmware(&fw_entry, file_path, tcm_hcd->pdev->dev.parent);
	if (error) {
		input_err(true, tcm_hcd->pdev->dev.parent, "firmware load failed, error=%d\n", error);
		return error;
	}

	fw_size = fw_entry->size;

	if (signing) {
		/* name 3, digest 32, signature 512 */
		spu_fw_size = fw_entry->size;
#ifdef SUPPORT_FW_SIGNED
		fw_size -= SPU_METADATA_SIZE(TSP);
#endif
	}

	if (fw_size > 0) {
		fw_data = vzalloc(fw_entry->size);
		if (!fw_data) {
			input_err(true, tcm_hcd->pdev->dev.parent, "%s: failed to alloc mem\n", __func__);
			error = -ENOMEM;
			goto open_err;
		}

		if (signing) {
			unsigned char *spu_fw_data;

			spu_fw_data = vzalloc(spu_fw_size);
			if (!spu_fw_data) {
				input_err(true, tcm_hcd->pdev->dev.parent, "%s: failed to alloc mem for spu\n", __func__);
				error = -ENOMEM;
				vfree(fw_data);
				goto open_err;
			}
			memcpy(spu_fw_data, fw_entry->data, spu_fw_size);

			input_info(true, tcm_hcd->pdev->dev.parent,
				"%s: start, file path %s, size %ld Bytes\n", __func__, file_path, spu_fw_size);
#ifdef SUPPORT_FW_SIGNED
			spu_ret = spu_firmware_signature_verify("TSP", spu_fw_data, spu_fw_size);
#endif
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
			memcpy(fw_data, fw_entry->data, fw_entry->size);
			input_info(true, tcm_hcd->pdev->dev.parent,
				"%s: start, file path %s, size %ld Bytes\n", __func__, file_path, fw_size);
		}

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
	release_firmware(fw_entry);
	return error;
}

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
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
#if IS_ENABLED(CONFIG_SAMSUNG_PRODUCT_SHIP)
		retval = sec_fn_load_fw(tcm_hcd, SIGNING, TSP_EXTERNAL_FW_SIGNED);
#else
		retval = sec_fn_load_fw(tcm_hcd, NORMAL, TSP_EXTERNAL_FW);
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
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	char buff[16] = { 0 };

	strncpy(buff, "OMNIVISION", sizeof(buff));
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
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	char buff[7] = { 0 };

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %s\n", __func__, tcm_hcd->id_info.part_number);
	snprintf(buff, sizeof(buff), "%s", tcm_hcd->id_info.part_number);

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
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
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
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
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
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
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
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
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
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
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

static void run_sram_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	sec_cmd_set_default_result(sec);
	test_fw_ram(sec);
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

static void run_fdm_noise_test_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	struct sec_factory_test_mode mode;
	int sum;

	sec_cmd_set_default_result(sec);

	memset(&mode, 0x00, sizeof(struct sec_factory_test_mode));
	ovt_tcm_get_face_area(&sum, &mode);
	input_info(true, tcm_hcd->pdev->dev.parent, "%d %d\n", mode.min, mode.max);

	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strlen(tcm_hcd->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "FDM_NOISE");

	sec->cmd_state = SEC_CMD_STATUS_OK;
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
#ifdef CONFIG_SEC_FACTORY
	int retval;
#endif
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
	run_sram_test(sec);

#ifdef CONFIG_SEC_FACTORY
	retval = tcm_hcd->set_dynamic_config(tcm_hcd, DC_ENABLE_EDGE_REJECT, 1);
	if (retval < 0)
		input_err(true, tcm_hcd->pdev->dev.parent, "Failed to enable edge reject\n");
	else
		input_info(true, tcm_hcd->pdev->dev.parent, "enable edge reject\n");

	msleep(50);
#endif

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	input_info(true, tcm_hcd->pdev->dev.parent,
		"%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void factory_lcdoff_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	run_dynamic_range_test_read(sec);
	run_noise_test_read(sec);
	run_fdm_noise_test_read(sec);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	input_info(true, tcm_hcd->pdev->dev.parent,
				"%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void incell_power_control(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: invalid parameter %d\n", __func__, sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	tcm_hcd->lcdoff_test = sec->cmd_param[0];

	input_info(true, tcm_hcd->pdev->dev.parent, "%s : lcdoff_test %s\n",
				__func__, tcm_hcd->lcdoff_test ? "ON" : "OFF");
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void check_connection(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	sec_cmd_set_default_result(sec);

	test_check_connection(sec);

}
/*
static void get_crc_check(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	sec_cmd_set_default_result(sec);

	test_fw_crc(sec);
}
*/
static void set_grip_data(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	int retval;
	unsigned char out_buf[3] = { 0 }; 
	unsigned char *resp_buf;
	unsigned int resp_buf_size;
	unsigned int resp_length;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	resp_buf = NULL;
	resp_buf_size = 0;

	if (tcm_hcd->lp_state != PWR_ON) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: not power on\n", __func__);
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

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (!tcm_hcd->hw_if->bdata->enable_settings_aot) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: Not support AOT(%d)\n", __func__, sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		tcm_hcd->ovt_tcm_gesture_type.doubletap = sec->cmd_param[0];
		ovt_set_config_mode(tcm_hcd, DC_ENABLE_GESTURE_TYPE, "gesture",
			((tcm_hcd->ovt_tcm_gesture_type.swipeup << 15) | tcm_hcd->ovt_tcm_gesture_type.doubletap));

		input_info(true, tcm_hcd->pdev->dev.parent, "%s: AOT %s(%d)\n",
				__func__, sec->cmd_param[0] ? "on" : "off", tcm_hcd->ovt_tcm_gesture_type.doubletap);

		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);

	if (!tcm_hcd->hw_if->bdata->support_spay_gesture) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: Not support SPAY!\n", __func__);
		goto out;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: Not defined value.\n", __func__);
		goto out;
	} else {
		tcm_hcd->ovt_tcm_gesture_type.swipeup = sec->cmd_param[0];
		ovt_set_config_mode(tcm_hcd, DC_ENABLE_GESTURE_TYPE, "gesture",
			((tcm_hcd->ovt_tcm_gesture_type.swipeup << 15) | tcm_hcd->ovt_tcm_gesture_type.doubletap));
	}

	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: spay(swipeup) %s(%d)\n",
			__func__, sec->cmd_param[0] ? "on" : "off", tcm_hcd->ovt_tcm_gesture_type.swipeup);

	return;
out:
	snprintf(buff, sizeof(buff), "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}


/*
 *	cmd_param
 *		[0], 0 normal debounce
 *		     1 lower debounce
 */
static void set_sip_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retval;

	sec_cmd_set_default_result(sec);

	if (tcm_hcd->lp_state != PWR_ON) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: not power on\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: abnormal param[%d]\n", __func__, sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	tcm_hcd->sip_mode = sec->cmd_param[0];

	retval = tcm_hcd->set_dynamic_config(tcm_hcd, DC_SIP_MODE, tcm_hcd->sip_mode);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,"%s: Failed to set sip\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	input_info(true, tcm_hcd->pdev->dev.parent, "%s : sip mode %s\n",
				__func__, tcm_hcd->sip_mode ? "ON" : "OFF");
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

/*
*	0 disable game mode
*	1 enable game mode
*/
static void set_game_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int retval;

	sec_cmd_set_default_result(sec);

	if (tcm_hcd->lp_state != PWR_ON) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: not power on\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: abnormal param[%d]\n", __func__, sec->cmd_param[0]);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	tcm_hcd->game_mode = sec->cmd_param[0];

	retval = tcm_hcd->set_dynamic_config(tcm_hcd, DC_GAME_MODE, tcm_hcd->game_mode);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,"%s: Failed to set game mode\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto exit;
	}

	input_info(true, tcm_hcd->pdev->dev.parent, "%s : game mode %s\n",
					__func__, tcm_hcd->game_mode ? "ON" : "OFF");
	snprintf(buff, sizeof(buff), "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;

exit:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

#if !IS_ENABLED(CONFIG_SEC_FACTORY)
static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
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

static void ear_detect_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);

	if (!(sec->cmd_param[0] == 0 || sec->cmd_param[0] == 1 || sec->cmd_param[0] == 3)) {
		input_err(true, tcm_hcd->pdev->dev.parent,
					"%s: abnormal cmd parm[%d]!\n", __func__, sec->cmd_param[0]);
		goto out;
	}

	tcm_hcd->ear_detect_enable = sec->cmd_param[0];

	if (tcm_hcd->sec_features.facemode1support && tcm_hcd->lp_state == PWR_OFF) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: face mode 1 support - %s skip!(%d)\n",
					__func__, "Power off", tcm_hcd->lp_state);
		goto out;
	} else if (!tcm_hcd->sec_features.facemode1support && tcm_hcd->lp_state != PWR_ON) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: %s skip!(%d)\n",
					__func__, tcm_hcd->lp_state == PWR_OFF ? "Power off" : "LP mode", tcm_hcd->lp_state);
		goto out;
	}

	mutex_lock(&tcm_hcd->mode_change_mutex);
	ret = tcm_hcd->set_dynamic_config(tcm_hcd, DC_ENABLE_FACE_DETECT, tcm_hcd->ear_detect_enable);
	if (ret < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"%s: Failed to enable ear detect mode\n", __func__);
		mutex_unlock(&tcm_hcd->mode_change_mutex);
		goto out;
	}
	mutex_unlock(&tcm_hcd->mode_change_mutex);


	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %s,%d\n", __func__, buff, tcm_hcd->ear_detect_enable);
	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);


}

static void prox_lp_scan_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;

	sec_cmd_set_default_result(sec);

	if (!tcm_hcd->hw_if->bdata->prox_lp_scan_enabled) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: not support lp scan!\n", __func__);
		goto out_fail;
	}

	if (tcm_hcd->early_resume_cnt > 0) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: already early resume called(%d)!\n",
					__func__, tcm_hcd->early_resume_cnt);
		goto out_fail;
	}

	if (!tcm_hcd->ear_detect_enable) {
		input_err(true, tcm_hcd->pdev->dev.parent,
					"%s: face detect is not enable, ignore this command\n", __func__);
		goto out_fail;
	}

	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: abnormal parm[%d]!\n", __func__, sec->cmd_param[0]);
		goto out_fail;
	}

	if (tcm_hcd->lp_state == PWR_OFF) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: PWR OFF skip!\n", __func__);
		goto out_fail;
	}

	if (tcm_hcd->lp_state == PWR_ON) {
		input_info(true, tcm_hcd->pdev->dev.parent, "%s: save call cnt and handle later\n", __func__);
		tcm_hcd->prox_lp_scan_cnt++;
		goto out;
	}

	mutex_lock(&tcm_hcd->mode_change_mutex);
	ret = ovt_tcm_set_scan_start_stop_cmd(tcm_hcd, sec->cmd_param[0] ? 1 : 0);
	if (ret < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
			"Failed to write command %s\n", STR(CMD_SET_SCAN_START_STOP));
		mutex_unlock(&tcm_hcd->mode_change_mutex);
		goto out;
	}
	mutex_unlock(&tcm_hcd->mode_change_mutex);

out:
	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state =  SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, tcm_hcd->pdev->dev.parent, "%s : switch to %s mode OK\n",
					__func__, sec->cmd_param[0] ? "SCAN START" : "SCAN STOP");

	return;
out_fail:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, tcm_hcd->pdev->dev.parent, "%s : failed to switch %s mode\n",
					__func__, sec->cmd_param[0] ? "SCAN START" : "SCAN STOP");
}

static void cmd_run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);
	input_info(true, tcm_hcd->pdev->dev.parent, "%s: start\n", __func__);

	ret = get_proximity();

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: sum = %d\n", __func__, ret);

	if(ret == -1)
		goto exit;

	snprintf(buff, sizeof(buff), "SUM_X:%d THD_X:%d", ret, 0);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_exit(sec);

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: total end\n", __func__);

	return;

exit:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	return;
}

static void glove_mode(void *device_data)
{
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);

	sec_cmd_set_default_result(sec);
	input_info(true, tcm_hcd->pdev->dev.parent, "%s,%d\n", __func__, sec->cmd_param[0]);

	tcm_hcd->glove_enabled = sec->cmd_param[0];

	if (tcm_hcd->lp_state != PWR_ON) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: not power on\n", __func__);
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		goto out;
	}

	switch (sec->cmd_param[0]) {
	case 0:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, tcm_hcd->pdev->dev.parent, "%s Unset Glove Mode\n", __func__);
		tcm_hcd->set_dynamic_config(private_tcm_hcd, DC_ENABLE_SENSITIVITY, UNSET_GLOVE_MODE);
		break;
	case 1:
		sec->cmd_state = SEC_CMD_STATUS_OK;
		input_info(true, tcm_hcd->pdev->dev.parent, "%s set Glove Mode\n", __func__);
		tcm_hcd->set_dynamic_config(private_tcm_hcd, DC_ENABLE_SENSITIVITY, SET_GLOVE_MODE);
		break;
	default:
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_info(true, tcm_hcd->pdev->dev.parent, "%s Invalid Argument\n", __func__);
		break;
	}

	if (sec->cmd_state == SEC_CMD_STATUS_OK)
		snprintf(buff, sizeof(buff), "%s", "OK");
	else
		snprintf(buff, sizeof(buff), "%s", "NG");
out:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, tcm_hcd->pdev->dev.parent, "%s %s(%d)\n", __func__, buff, (int)strnlen(buff, sizeof(buff)));
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
	{SEC_CMD("get_threshold", get_threshold),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("run_dynamic_range_test_read", run_dynamic_range_test_read),},
	{SEC_CMD("run_dynamic_range_test_read_all", run_dynamic_range_test_read_all),},
	{SEC_CMD("run_open_short_test_read", run_open_short_test_read),},
	{SEC_CMD("run_open_short_test_read_all", run_open_short_test_read_all),},
	{SEC_CMD("run_noise_test_read", run_noise_test_read),},
	{SEC_CMD("run_noise_test_read_all", run_noise_test_read_all),},
	{SEC_CMD("run_sram_test", run_sram_test),},
	{SEC_CMD("get_gap_data_x_all", get_gap_data_x_all),},
	{SEC_CMD("get_gap_data_y_all", get_gap_data_y_all),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("check_connection", check_connection),},
//	{SEC_CMD("get_crc_check", get_crc_check),},
	{SEC_CMD("set_grip_data", set_grip_data),},
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD_H("spay_enable", spay_enable),},
	{SEC_CMD("set_sip_mode", set_sip_mode),},
	{SEC_CMD("set_game_mode", set_game_mode),},
	{SEC_CMD_H("ear_detect_enable", ear_detect_enable),},
	{SEC_CMD_H("prox_lp_scan_mode", prox_lp_scan_mode),},
	{SEC_CMD("run_prox_intensity_read_all", cmd_run_prox_intensity_read_all),},
	{SEC_CMD("incell_power_control", incell_power_control),},
	{SEC_CMD("factory_lcdoff_cmd_result_all", factory_lcdoff_cmd_result_all),},
#if !IS_ENABLED(CONFIG_SEC_FACTORY)	
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
#endif
	{SEC_CMD("glove_mode", glove_mode),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
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

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	return count;
}

static ssize_t read_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	u32 feature = 0;

	if (tcm_hcd->hw_if->bdata->enable_settings_aot)
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;
	if (tcm_hcd->hw_if->bdata->prox_lp_scan_enabled)
		feature |= INPUT_FEATURE_ENABLE_PROX_LP_SCAN_ENABLED;
	if (tcm_hcd->hw_if->bdata->enable_sysinput_enabled)
		feature |= INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED;


	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %d %s %s %s\n",
			__func__, feature,
			feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "",
			feature & INPUT_FEATURE_ENABLE_PROX_LP_SCAN_ENABLED ? " LPSCAN" : "",
			feature & INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED ? " SE" : "");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", feature);
}

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %ld\n", __func__,
			tcm_hcd->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%ld", tcm_hcd->prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	long data;
	int ret;

	ret = kstrtol(buf, 10, &data);
	if (ret < 0)
		return ret;

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %ld\n", __func__, data);

	tcm_hcd->prox_power_off = data;

	return count;
}

/** virtual_prox **/
static ssize_t protos_event_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %d\n", __func__,
			tcm_hcd->hover_event);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", tcm_hcd->hover_event != 3 ? 0 : 3);
}

static ssize_t protos_event_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	u8 data;
	int ret;

	ret = kstrtou8(buf, 10, &data);
	if (ret < 0)
		return ret;

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %d\n", __func__, data);

	if (data != 0 && data != 1) {
		input_err(true, tcm_hcd->pdev->dev.parent, "%s: incorrect data\n", __func__);
		return -EINVAL;
	}
	
	tcm_hcd->ear_detect_enable = data;
	ret = tcm_hcd->set_dynamic_config(tcm_hcd, DC_ENABLE_FACE_DETECT,
		tcm_hcd->ear_detect_enable);
	if (ret < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"%s: Failed to enable ear detect mode\n", __func__);
	}

	return count;
}

static ssize_t enabled_show(struct device *dev, struct device_attribute *attr,
					char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: lp_state %d\n", __func__, tcm_hcd->lp_state);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", tcm_hcd->lp_state);
}

static ssize_t enabled_store(struct device *dev, struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);

	int buff[2];
	int ret;
	int retval = 0;

	if (atomic_read(&tcm_hcd->shutdown)) {
		input_info(true, tcm_hcd->pdev->dev.parent,"%s: shutdown & not handled\n", __func__);
		return 0;
	}

	ret = sscanf(buf, "%d,%d", &buff[0], &buff[1]);
	if (ret != 2) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"%s: failed read params [%d]\n", __func__, ret);
		return -EINVAL;
	}

	input_info(true, tcm_hcd->pdev->dev.parent, "%s: %d %d\n", __func__, buff[0], buff[1]);

	/* handle same sequence : buff[0] = DISPLAY_STATE_ON, DISPLAY_STATE_DOZE, DISPLAY_STATE_DOZE_SUSPEND */
	if (buff[0] == DISPLAY_STATE_DOZE || buff[0] == DISPLAY_STATE_DOZE_SUSPEND)
		buff[0] = DISPLAY_STATE_ON;

	if (buff[0] == DISPLAY_STATE_ON) {
		if (buff[1] == DISPLAY_EVENT_EARLY)
			ovt_tcm_early_resume(&tcm_hcd->pdev->dev);
		else if (buff[1] == DISPLAY_EVENT_LATE)
			ovt_tcm_resume(&tcm_hcd->pdev->dev);
	} else if (buff[0] == DISPLAY_STATE_OFF) {
		if (atomic_read(&tcm_hcd->firmware_flashing)) {
			retval = wait_event_interruptible_timeout(tcm_hcd->reflash_wq,
						!atomic_read(&tcm_hcd->firmware_flashing),
						msecs_to_jiffies(RESPONSE_TIMEOUT_MS));
			if (retval == 0) {
				input_err(true, tcm_hcd->pdev->dev.parent,
					"Timed out waiting for completion of flashing firmware\n");
				atomic_set(&tcm_hcd->firmware_flashing, 0);
				return -EIO;
			} else {
				retval = 0;
			}
		}

		if (buff[1] == DISPLAY_EVENT_EARLY)
			ovt_tcm_early_suspend(&tcm_hcd->pdev->dev);
		else if (buff[1] == DISPLAY_EVENT_LATE)
			ovt_tcm_suspend(&tcm_hcd->pdev->dev);
	} else if (buff[0] == DISPLAY_STATE_LPM_OFF || buff[0] == DISPLAY_STATE_SERVICE_SHUTDOWN) {
		if (buff[0] == DISPLAY_STATE_LPM_OFF) {
			input_info(true, tcm_hcd->pdev->dev.parent, "%s: DISPLAY_STATE_LPM_OFF \n", __func__);
		} else if (buff[0] == DISPLAY_STATE_SERVICE_SHUTDOWN) {
			input_info(true, tcm_hcd->pdev->dev.parent, "%s: DISPLAY_STATE_SERVICE_SHUTDOWN", __func__);
		}
		atomic_set(&tcm_hcd->shutdown, 1);
		tcm_hcd->lp_state = PWR_OFF;
		retval = tcm_hcd->enable_irq(tcm_hcd, false, false);
		if (retval < 0) {
			input_err(true, tcm_hcd->pdev->dev.parent, "Failed to disable interrupt before\n");
		}
		ovt_pinctrl_configure(tcm_hcd, false);
		input_info(true, tcm_hcd->pdev->dev.parent, "%s: lpm, disable interrupt and work--\n", __func__);
	}

	return count;
}
ssize_t ovt_get_lp_dump(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);

	ovt_lpwg_dump_buf_read(tcm_hcd, buf);

	return strlen(buf);
}

static ssize_t scrub_pos_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sec_cmd_data *sec = dev_get_drvdata(dev);
	struct ovt_tcm_hcd *tcm_hcd = container_of(sec, struct ovt_tcm_hcd, sec);
	char buff[256] = { 0 };

	snprintf(buff, sizeof(buff), "%d %d %d", tcm_hcd->hw_if->bdata->scrub_id, 0, 0);
	input_info(true, tcm_hcd->pdev->dev.parent, "%s: scrub_id: %s\n", __func__, buff);

	return snprintf(buf, PAGE_SIZE, "%s", buff);
}

static DEVICE_ATTR(sensitivity_mode, 0664, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(support_feature, 0444, read_support_feature, NULL);
static DEVICE_ATTR(prox_power_off, 0664, prox_power_off_show, prox_power_off_store);
static DEVICE_ATTR(virtual_prox, 0664, protos_event_show, protos_event_store);
static DEVICE_ATTR(enabled, 0664, enabled_show, enabled_store);
static DEVICE_ATTR(get_lp_dump, 0444, ovt_get_lp_dump, NULL);
static DEVICE_ATTR(scrub_pos, 0444, scrub_pos_show, NULL);

static struct attribute *cmd_attributes[] = {
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_support_feature.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_virtual_prox.attr,
	&dev_attr_enabled.attr,
	&dev_attr_get_lp_dump.attr,
	&dev_attr_scrub_pos.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int ovt_ccic_notification(struct notifier_block *nb,
	   unsigned long action, void *data)
{

	PD_NOTI_USB_STATUS_TYPEDEF usb_status = *(PD_NOTI_USB_STATUS_TYPEDEF *)data;

	if (usb_status.dest != PDIC_NOTIFY_DEV_USB)
		return 0;

	switch (usb_status.drp) {
	case USB_STATUS_NOTIFY_ATTACH_DFP:
		otg_flag = 1;
		input_info(true, private_tcm_hcd->pdev->dev.parent, "%s otg_flag %d\n", __func__, otg_flag);
		break;
	case USB_STATUS_NOTIFY_DETACH:
		otg_flag = 0;
		input_info(true, private_tcm_hcd->pdev->dev.parent, "%s otg_flag %d\n", __func__, otg_flag);
		break;
	default:
		break;
	}
	return 0;
}
#endif
static int ovt_vbus_notification(struct notifier_block *nb,
		unsigned long cmd, void *data)
{
	vbus_status_t vbus_type = *(vbus_status_t *) data;
	int ret = 0;

#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	input_info(true, private_tcm_hcd->pdev->dev.parent, "%s otg_flag=%d\n", __func__, otg_flag);
#endif
	input_info(true, private_tcm_hcd->pdev->dev.parent, "%s cmd=%lu, vbus_type=%d\n", __func__, cmd, vbus_type);

	if (atomic_read(&private_tcm_hcd->shutdown)) {
		input_info(true, private_tcm_hcd->pdev->dev.parent,"%s: shutdown & not handled\n", __func__);
		return 0;
	}

	switch (vbus_type) {
	case STATUS_VBUS_HIGH:/* vbus_type == 2 */
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
		if (!otg_flag)
			private_tcm_hcd->USB_detect_flag = USB_PLUG_ATTACHED;
		else
			ret = -1;
#else
		private_tcm_hcd->USB_detect_flag = USB_PLUG_ATTACHED;
#endif
		break;
	case STATUS_VBUS_LOW:/* vbus_type == 1 */
		private_tcm_hcd->USB_detect_flag = USB_PLUG_DETACHED;
		break;
	default:
		ret = -1;
		break;
	}

	queue_work(private_tcm_hcd->usb_notifier_workqueue, &private_tcm_hcd->usb_notifier_work);

	return 0;
}
static void ovt_vbus_notification_work(struct work_struct *work)
{
	int ret = 0;

	input_info(true, private_tcm_hcd->pdev->dev.parent, "%s,USB_detect_flag:%d\n", __func__, private_tcm_hcd->USB_detect_flag);

	if (atomic_read(&private_tcm_hcd->shutdown)) {
		input_info(true, private_tcm_hcd->pdev->dev.parent,"%s: shutdown & not handled\n", __func__);
		return;
	}

	ret = private_tcm_hcd->set_dynamic_config(private_tcm_hcd, DC_CHARGER_CONNECTED, private_tcm_hcd->USB_detect_flag);
	if (ret < 0) {
		input_info(true, private_tcm_hcd->pdev->dev.parent, "%s: Failed to set USB_detect_flag:%d\n",
				__func__, private_tcm_hcd->USB_detect_flag);
		return;
	}
	return;
}
#endif

int sec_fn_init(struct ovt_tcm_hcd *tcm_hcd)
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

	private_tcm_hcd = tcm_hcd;
	private_tcm_hcd->glove_enabled = UNSET_GLOVE_MODE;
	private_tcm_hcd->USB_detect_flag = USB_PLUG_DETACHED;
#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	otg_flag = 0;
	manager_notifier_register(&private_tcm_hcd->ccic_nb, ovt_ccic_notification,
			MANAGER_NOTIFY_PDIC_INITIAL);
#endif
	tcm_hcd->usb_notifier_workqueue = create_singlethread_workqueue("ovt_tcm_usb_noti");
	INIT_WORK(&tcm_hcd->usb_notifier_work, ovt_vbus_notification_work);

	vbus_notifier_register(&private_tcm_hcd->vbus_nb, ovt_vbus_notification,
			VBUS_NOTIFY_DEV_CHARGER);
#endif

	return 0;

error_alloc_pFram:
	kfree(tcm_hcd->print_buf);
exit:
	return retval;
}

void sec_fn_remove(struct ovt_tcm_hcd *tcm_hcd)
{
	input_info(true, tcm_hcd->pdev->dev.parent, "%s\n", __func__);

#if IS_ENABLED(CONFIG_VBUS_NOTIFIER)
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	manager_notifier_unregister(&private_tcm_hcd->ccic_nb);
#endif
	cancel_work_sync(&tcm_hcd->usb_notifier_work);
	flush_workqueue(tcm_hcd->usb_notifier_workqueue);
	destroy_workqueue(tcm_hcd->usb_notifier_workqueue);

	vbus_notifier_unregister(&private_tcm_hcd->vbus_nb);
#endif

	kfree(tcm_hcd->pFrame);

	sysfs_remove_group(&tcm_hcd->sec.fac_dev->kobj,
			&cmd_attr_group);

	sec_cmd_exit(&tcm_hcd->sec, SEC_CLASS_DEVT_TSP);

	return;
}
