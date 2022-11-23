/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "ili9881x_sec_fn.h"

static int sec_fn_load_fw(int update_mode, bool signing)
{
	int ret = 0;
	bool esd_en = ilits->wq_esd_ctrl, bat_en = ilits->wq_bat_ctrl;
	static unsigned char g_user_buf[USER_STR_BUFF] = {0};

	input_info(true, ilits->dev, "%s Prepar to upgarde firmware\n", __func__);

	mutex_lock(&ilits->touch_mutex);

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, DISABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, DISABLE);

	ilits->signing = signing;

	if (update_mode == BUILT_IN) {
		ilits->fw_open = REQUEST_FIRMWARE;
	} else if (update_mode == UMS) {
		ilits->fw_open = FILP_OPEN;
		if (ilits->signing)
			ilits->md_fw_filp_path = TSP_PATH_EXTERNAL_FW_SIGNED;
		else
			ilits->md_fw_filp_path = TSP_PATH_EXTERNAL_FW;
	} else {
		ilits->fw_open = REQUEST_FIRMWARE;
	}

	ilits->force_fw_update = ENABLE;
	ilits->node_update = true;

	ret = ili_fw_upgrade_handler(NULL);

	ilits->node_update = false;
	ilits->force_fw_update = DISABLE;
	ilits->fw_open = REQUEST_FIRMWARE;

	g_user_buf[0] = 0x0;
	g_user_buf[1] = (ret < 0) ? -ret : ret;

	if (g_user_buf[1] == 0)
		input_info(true, ilits->dev, "%s Upgrade firmware = PASS\n", __func__);
	else
		input_err(true, ilits->dev, "%s Upgrade firmware = FAIL\n", __func__);

	/* Reason for fail */
	if (g_user_buf[1] == EFW_CONVERT_FILE) {
		g_user_buf[0] = 0xFF;
		input_err(true, ilits->dev, "%s Failed to convert hex/ili file, abort!\n", __func__);
	} else if (g_user_buf[1] == ENOMEM) {
		input_err(true, ilits->dev, "%s Failed to allocate memory, abort!\n", __func__);
	} else if (g_user_buf[1] == EFW_ICE_MODE) {
		input_err(true, ilits->dev, "%s Failed to operate ice mode, abort!\n", __func__);
	} else if (g_user_buf[1] == EFW_CRC) {
		input_err(true, ilits->dev, "%s CRC not matched, abort!\n", __func__);
	} else if (g_user_buf[1] == EFW_REST) {
		input_err(true, ilits->dev, "%s Failed to do reset, abort!\n", __func__);
	} else if (g_user_buf[1] == EFW_ERASE) {
		input_err(true, ilits->dev, "%s Failed to erase flash, abort!\n", __func__);
	} else if (g_user_buf[1] == EFW_PROGRAM) {
		input_err(true, ilits->dev, "%s Failed to program flash, abort!\n", __func__);
	} else if (g_user_buf[1] == EFW_INTERFACE) {
		input_err(true, ilits->dev, "%s Failed to hex file interface no match, abort!\n", __func__);
	}

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, ENABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, ENABLE);

	set_current_ic_mode(SET_MODE_NORMAL);

	mutex_unlock(&ilits->touch_mutex);
	return ret;
}

static void fw_update(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
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

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	switch (sec->cmd_param[0]) {
	case BUILT_IN:
		retval = sec_fn_load_fw(BUILT_IN, NORMAL);
		break;
	case UMS:
#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
		retval = sec_fn_load_fw(UMS, SIGNING);
#else
		retval = sec_fn_load_fw(UMS, NORMAL);
#endif
		break;
	default:
		input_info(true, ilits->dev, "%s: Not supported value:%d\n", __func__, sec->cmd_param[0]);
		retval = -1;
		break;
	}
	if (retval < 0)
		snprintf(buff, sizeof(buff), "FAIL");
	else
		snprintf(buff, sizeof(buff), "OK");

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;

out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));

}

static void aot_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct gesture_symbol *ptr_sym = &ilits->ges_sym;
	u8 *ptr;
	u32 ges_sym_value = 0;

	sec_cmd_set_default_result(sec);

	if (!ilits->enable_settings_aot) {
		input_err(true, ilits->dev, "%s: Not support AOT!\n", __func__);
		goto out;
	}

	ptr = (u8 *) ptr_sym;
	ges_sym_value = (ptr[0] | (ptr[1] << 8) | (ptr[2] << 16)) & DOUBLE_TAP_MASK;

	input_info(true, ilits->dev, "%s cmd:%d, gesture:%d, ilits->ges_sym:0x%x, ges_sym_value:0x%x\n",
			__func__, sec->cmd_param[0], ilits->gesture, ilits->ges_sym, ges_sym_value);

	if (!!sec->cmd_param[0]) {
		ilits->gesture = !!sec->cmd_param[0];
	} else {
		if (!ges_sym_value)
			ilits->gesture = !!sec->cmd_param[0];
	}
	ilits->ges_sym.double_tap = (u8)!!sec->cmd_param[0];

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, ilits->dev, "%s, %s, gesture:%d, ilits->ges_sym:0x%x\n",
			__func__, sec->cmd_result, ilits->gesture, ilits->ges_sym);
	return;

out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, ilits->dev, "%s, %s, gesture:%d, ilits->ges_sym:0x%x\n",
			__func__, sec->cmd_result, ilits->gesture, ilits->ges_sym);
}

static void spay_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	struct gesture_symbol *ptr_sym = &ilits->ges_sym;
	u8 *ptr;
	u32 ges_sym_value = 0;

	sec_cmd_set_default_result(sec);

	if (!ilits->support_spay_gesture_mode) {
		input_err(true, ilits->dev, "%s: Not support SPAY!\n", __func__);
		goto out;
	}

	ptr = (u8 *) ptr_sym;
	ges_sym_value = (ptr[0] | (ptr[1] << 8) | (ptr[2] << 16)) & ALPHABET_LINE_2_TOP_MASK;

	input_info(true, ilits->dev, "%s cmd:%d, gesture:%d, ilits->ges_sym:0x%x, ges_sym_value:0x%x\n",
			__func__, sec->cmd_param[0], ilits->gesture, ilits->ges_sym, ges_sym_value);

	if (!!sec->cmd_param[0]) {
		ilits->gesture = !!sec->cmd_param[0];
	} else {
		if (!ges_sym_value)
			ilits->gesture = !!sec->cmd_param[0];
	}
	ilits->ges_sym.alphabet_line_2_top = (u8)!!sec->cmd_param[0];

	snprintf(buff, sizeof(buff), "%s", "OK");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, ilits->dev, "%s, %s, gesture:%d, ilits->ges_sym:0x%x\n",
			__func__, sec->cmd_result, ilits->gesture, ilits->ges_sym);
	return;
out:
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);

	input_info(true, ilits->dev, "%s, %s, gesture:%d, ilits->ges_sym:0x%x\n",
			__func__, sec->cmd_result, ilits->gesture, ilits->ges_sym);
}

static void get_chip_vendor(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	char buff[16] = { 0 };

	strncpy(buff, "ILI", sizeof(buff));
	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_VENDOR");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, ilits->dev, "%s: %s\n", __func__, buff);
}

static void get_chip_name(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	char buff[16] = { 0 };

	strncpy(buff, "ILI9881X", sizeof(buff));

	sec_cmd_set_default_result(sec);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "IC_NAME");
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, ilits->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_bin(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	snprintf(buff, sizeof(buff), "%c%c%02d%02d%02d%02d",
		ilits->fw_customer_info[0], ilits->fw_customer_info[1], ilits->fw_customer_info[2],
		ilits->fw_customer_info[3], ilits->fw_customer_info[4], ilits->fw_customer_info[5]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_BIN");
	sec->cmd_state = SEC_CMD_STATUS_OK;

	input_info(true, ilits->dev, "%s: %s\n", __func__, buff);
}

static void get_fw_ver_ic(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	u8 cmd[1] = {0x26};
	u8 buf[8] = {0};
	u8 retry = 25, checksum = 0;

	char buff[16] = { 0 };
	char model[7] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	do {
		if (ilits->wrapper(cmd, sizeof(u8), buf, 8, ON, OFF) < 0)
			input_err(true, ilits->dev, "%s Write pre cmd failed\n", __func__);

		checksum = ili_calc_packet_checksum(buf, 7);

		if (retry <= 0) {
			input_info(true, ilits->dev, "%s failed get ic version\n", __func__);
			memset(&buf, 0x00, sizeof(buf));
			break;
		}
		retry--;
	} while (checksum != buf[7]);
	input_info(true, ilits->dev, "%s %02d%c%c%02d%02d%02d%02d%02d\n",
		__func__, buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);

	snprintf(buff, sizeof(buff), "%c%c%02d%02d%02d%02d", buf[1], buf[2], buf[3], buf[4], buf[5], buf[6]);
	snprintf(model, sizeof(model), "%c%c%02d%02d", buf[1], buf[2], buf[3], buf[4]);

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, model, strnlen(model, sizeof(model)), "FW_MODEL");
	}
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, ilits->dev, "%s: %s\n", __func__, buff);
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "FW_VER_IC");
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(model, sizeof(model)), "FW_MODEL");
	}
}

static void get_x_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", ilits->xch_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, ilits->dev, "%s: %s\n", __func__, buff);
}

static void get_y_num(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);
	snprintf(buff, sizeof(buff), "%d", ilits->ych_num);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	input_info(true, ilits->dev, "%s: %s\n", __func__, buff);
}

static void run_sram_test(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	sec_cmd_set_default_result(sec);

	test_sram(sec);

}

static void run_raw_test_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	/* only for 9882q */
	if (ilits->chip->id == ILI9882_CHIP && ilits->chip->type == 0x1A) {
		ilits->current_mpitem = "raw data(have bk)";
		ilits->allnode = TEST_MODE_MIN_MAX;
		ilits->node_min = ilits->node_max = 0;
		ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[RAWDATAHAVEBK_LCMON_PATH_NUM], ON);
	} else {
		ilits->current_mpitem = "raw data(no bk)";
		ilits->allnode = TEST_MODE_MIN_MAX;
		ilits->node_min = ilits->node_max = 0;
		ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[RAWDATANOBK_LCMON_PATH_NUM], ON);
	}
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "RAW");
	}

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "RAW");
	}

}

static void run_raw_test_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	/* only for 9882q */
	if (ilits->chip->id == ILI9882_CHIP && ilits->chip->type == 0x1A) {
		ilits->current_mpitem = "raw data(have bk)";
		ilits->allnode = TEST_MODE_ALL_NODE;
		ilits->node_min = ilits->node_max = 0;
		ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[RAWDATAHAVEBK_LCMON_PATH_NUM], ON);
	} else {
		ilits->current_mpitem = "raw data(no bk)";
		ilits->allnode = TEST_MODE_ALL_NODE;
		ilits->node_min = ilits->node_max = 0;
		ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[RAWDATANOBK_LCMON_PATH_NUM], ON);
	}
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void run_raw_doze_test_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "doze raw data";
	ilits->allnode = TEST_MODE_MIN_MAX;
	ilits->node_min = ilits->node_max = 0;

	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[DOZERAW_PATH_NUM], ON);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "RAW_DOZE");
	}
	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "RAW_DOZE");
	}
	ilits->current_mpitem = "";
}

static void run_raw_doze_test_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "doze raw data";
	ilits->allnode = TEST_MODE_ALL_NODE;
	ilits->node_min = ilits->node_max = 0;

	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[DOZERAW_PATH_NUM], ON);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "RAW_DOZE");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "RAW_DOZE");
}

static void run_cal_dac_test_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "calibration data(dac)";
	ilits->allnode = TEST_MODE_MIN_MAX;
	ilits->node_min = ilits->node_max = 0;
	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[DAC_PATH_NUM], ON);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "CAL_DAC");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "CAL_DAC");
}

static void run_cal_dac_test_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "calibration data(dac)";
	ilits->allnode = TEST_MODE_ALL_NODE;
	ilits->node_min = ilits->node_max = 0;
	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[DAC_PATH_NUM], ON);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void run_open_test_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "open test_c";
	ilits->allnode = TEST_MODE_MIN_MAX;
	ilits->node_min = ilits->node_max = 0;
	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[OPENTESTC_PATH_NUM], ON);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "OPEN");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "OPEN");
}

static void run_open_test_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "open test_c";
	ilits->allnode = TEST_MODE_ALL_NODE;
	ilits->node_min = ilits->node_max = 0;
	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[OPENTESTC_PATH_NUM], ON);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "OPEN");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "OPEN");
}

static void run_short_test_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "short test";
	ilits->allnode = TEST_MODE_MIN_MAX;
	ilits->node_min = ilits->node_max = 0;
	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[SHORTTEST_PATH_NUM], ON);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "SHORT");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "SHORT");
}

static void run_short_test_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "short test";
	ilits->allnode = TEST_MODE_ALL_NODE;
	ilits->node_min = ilits->node_max = 0;
	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[SHORTTEST_PATH_NUM], ON);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "SHORT");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "SHORT");

}

static void run_noise_test_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "noise peak to peak(with panel)";
	ilits->allnode = TEST_MODE_MIN_MAX;
	ilits->node_min = ilits->node_max = 0;
	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[NOISEPP_LCMON_PATH_NUM], ON);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "NOISE");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "NOISE");
}

static void run_noise_test_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "noise peak to peak(with panel)";
	ilits->allnode = TEST_MODE_ALL_NODE;
	ilits->node_min = ilits->node_max = 0;
	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[NOISEPP_LCMON_PATH_NUM], ON);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void run_noise_doze_test_read(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "doze peak to peak";
	ilits->allnode = TEST_MODE_MIN_MAX;
	ilits->node_min = ilits->node_max = 0;

	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[DOZEPP_PATH_NUM], ON);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "NOISE_DOZE");
	}

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING) {
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "NOISE_DOZE");
	}
}

static void run_noise_doze_test_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "doze peak to peak";
	ilits->allnode = TEST_MODE_ALL_NODE;
	ilits->node_min = ilits->node_max = 0;

	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[DOZEPP_PATH_NUM], ON);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "NOISE_DOZE");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "NOISE_DOZE");
}

static void run_noise_test_read_lcdoff(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->power_status == POWER_OFF_STATUS) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "noise peak to peak(with panel) (lcm off)";
	ilits->allnode = TEST_MODE_MIN_MAX;
	ilits->node_min = ilits->node_max = 0;
	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[NOISEPP_LCMOFF_PATH_NUM], OFF);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "NOISE_OFF");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "NOISE_OFF");
}

static void run_noise_test_read_all_lcdoff(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->power_status == POWER_OFF_STATUS) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "noise peak to peak(with panel) (lcm off)";
	ilits->allnode = TEST_MODE_ALL_NODE;
	ilits->node_min = ilits->node_max = 0;
	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[NOISEPP_LCMOFF_PATH_NUM], OFF);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void run_noise_doze_test_read_lcdoff(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->power_status == POWER_OFF_STATUS) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "peak to peak_td (lcm off)";
	ilits->allnode = TEST_MODE_MIN_MAX;
	ilits->node_min = ilits->node_max = 0;

	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[P2P_TD_PATH_NUM], OFF);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "NOISE_DOZE_OFF");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "NOISE_DOZE_OFF");
}

static void run_noise_doze_test_read_all_lcdoff(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->power_status == POWER_OFF_STATUS) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "peak to peak_td (lcm off)";
	ilits->allnode = TEST_MODE_ALL_NODE;
	ilits->node_min = ilits->node_max = 0;

	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[P2P_TD_PATH_NUM], OFF);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "NOISE_DOZE_OFF");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "NOISE_DOZE_OFF");
}

static void run_raw_doze_test_read_lcdoff(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->power_status == POWER_OFF_STATUS) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "raw data_td (lcm off)";
	ilits->allnode = TEST_MODE_MIN_MAX;
	ilits->node_min = ilits->node_max = 0;

	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[RAWDATATD_PATH_NUM], OFF);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "RAW_DOZE_OFF");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "RAW_DOZE_OFF");
}

static void run_raw_doze_test_read_all_lcdoff(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->power_status == POWER_OFF_STATUS) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		goto out;
	}

	ilits->current_mpitem = "raw data_td (lcm off)";
	ilits->allnode = TEST_MODE_ALL_NODE;
	ilits->node_min = ilits->node_max = 0;

	ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[RAWDATATD_PATH_NUM], OFF);
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "RAW_DOZE_OFF");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, SEC_CMD_STR_LEN, "RAW_DOZE_OFF");
}

static void run_raw_test_read_lcdoff(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->power_status == POWER_OFF_STATUS) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		goto out;
	}

	/* only for 9882q */
	if (ilits->chip->id == ILI9882_CHIP && ilits->chip->type == 0x1A) {
		ilits->current_mpitem = "raw data(have bk) (lcm off)";
		ilits->allnode = TEST_MODE_MIN_MAX;
		ilits->node_min = ilits->node_max = 0;
		ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[RAWDATAHAVEBK_LCMOFF_PATH_NUM], OFF);
	} else {
		ilits->current_mpitem = "raw data(no bk) (lcm off)";
		ilits->allnode = TEST_MODE_MIN_MAX;
		ilits->node_min = ilits->node_max = 0;
		ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[RAWDATANOBK_LCMOFF_PATH_NUM], OFF);
	}
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "RAW_OFF");

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, ilits->print_buf, SEC_CMD_STR_LEN, "RAW_OFF");
}

static void run_raw_test_read_all_lcdoff(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->power_status == POWER_OFF_STATUS) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		goto out;
	}

	/* only for 9882q */
	if (ilits->chip->id == ILI9882_CHIP && ilits->chip->type == 0x1A) {
		ilits->current_mpitem = "raw data(have bk) (lcm off)";
		ilits->allnode = TEST_MODE_ALL_NODE;
		ilits->node_min = ilits->node_max = 0;
		ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[RAWDATAHAVEBK_LCMOFF_PATH_NUM], OFF);
	} else {
		
		ilits->current_mpitem = "raw data(no bk) (lcm off)";
		ilits->allnode = TEST_MODE_ALL_NODE;
		ilits->node_min = ilits->node_max = 0;
		ret = ilitek_node_mp_test_read(sec, ilits->mp_ini_path[RAWDATANOBK_LCMOFF_PATH_NUM], OFF);
	}
	if (ret < 0) {
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	}
	sec_cmd_set_cmd_result(sec, ilits->print_buf, strlen(ilits->print_buf));

	ilits->current_mpitem = "";
	sec->cmd_state = SEC_CMD_STATUS_OK;
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void factory_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	sec->item_count = 0;

	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	get_chip_vendor(sec);
	get_chip_name(sec);
	get_fw_ver_bin(sec);
	get_fw_ver_ic(sec);

	run_sram_test(sec);
	run_cal_dac_test_read(sec);
	run_short_test_read(sec);
	run_noise_test_read(sec);
	/* only for 9882q */
	if (ilits->chip->id == ILI9882_CHIP && ilits->chip->type == 0x1A) {
		run_raw_test_read(sec);
		run_raw_doze_test_read(sec);
		run_noise_doze_test_read(sec);
	}

	ilits->actual_tp_mode = P5_X_FW_AP_MODE;
	if (ilits->fw_upgrade_mode == UPGRADE_IRAM) {
		if (ili_fw_upgrade_handler(NULL) < 0)
			input_err(true, ilits->dev, "%s FW upgrade failed during mp test\n", __func__);
	} else {
		if (ili_reset_ctrl(ilits->reset) < 0)
			input_err(true, ilits->dev, "%s TP Reset failed during mp test\n", __func__);
	}

	atomic_set(&ilits->mp_stat, DISABLE);
	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	input_info(true, ilits->dev,
		"%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void factory_lcdoff_cmd_result_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;

	if (ilits->power_status == POWER_OFF_STATUS) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		sec->cmd_all_factory_state = SEC_CMD_STATUS_FAIL;
		return;
	}

	sec->item_count = 0;
	memset(sec->cmd_result_all, 0x00, SEC_CMD_RESULT_STR_LEN);

	sec->cmd_all_factory_state = SEC_CMD_STATUS_RUNNING;

	/* only for 9882q */
	if (ilits->chip->id == ILI9882_CHIP && ilits->chip->type == 0x1A) {
		run_noise_test_read_lcdoff(sec);
		run_noise_doze_test_read_lcdoff(sec);
		run_raw_test_read_lcdoff(sec);
	} else {
		run_noise_test_read_lcdoff(sec);
	}

	atomic_set(&ilits->mp_stat, DISABLE);
	sec->cmd_all_factory_state = SEC_CMD_STATUS_OK;

	input_info(true, ilits->dev,
		"%s: %d%s\n", __func__, sec->item_count, sec->cmd_result_all);
}

static void incell_power_control(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	int ret = 0;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);
	input_info(true, ilits->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		ret = -1;
		goto out;
	}

	switch (sec->cmd_param[0]) {
	case INCELL_POWER_DISABLE:
		ilits->incell_power_state = INCELL_POWER_DISABLE;
		ili_incell_power_control(INCELL_POWER_DISABLE);
		break;
	case INCELL_POWER_ENABLE:
		ilits->incell_power_state = INCELL_POWER_ENABLE;
		ili_incell_power_control(INCELL_POWER_ENABLE);
		break;
	default:
		input_info(true, ilits->dev, "%s: Not supported value:%d\n", __func__, sec->cmd_param[0]);
		ret = -1;
		break;
	}
out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}



static void run_prox_intensity_read_all(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char data[SEC_CMD_STR_LEN] = { 0 };
	int ret = 0;
	char buff[16] = { 0 };

	sec_cmd_set_default_result(sec);

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		goto out;
	}

	if (ilits->prox_face_mode && !ilits->started_prox_intensity) {
		input_info(true, ilits->dev, "%s: start\n", __func__);
		ilits->started_prox_intensity = true;
		ret = debug_mode_onoff(true);
		if (ret < 0)
			input_err(true, ilits->dev, "%s: debug_mode_onoff setting  failed\n", __func__);
	}

	if (ret < 0) {
		input_err(true, ilits->dev, "%s: debug_mode_onoff setting  failed\n", __func__);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(data, sizeof(data), "SUM_X:%d THD_X:%d", ilits->proximity_sum, ilits->proximity_thd);
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	sec_cmd_set_cmd_result(sec, data, strlen(data));
	return;
out:
	sec->cmd_state = SEC_CMD_STATUS_FAIL;
	snprintf(buff, sizeof(buff), "%s", "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
}

static void ear_detect_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	int ret = 0;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);
	mutex_lock(&ilits->touch_mutex);
	input_info(true, ilits->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	if ((sec->cmd_param[0] != EAR_DETECT_DISABLE) && (sec->cmd_param[0] != EAR_DETECT_NORMAL_MODE) && (sec->cmd_param[0] != EAR_DETECT_INSENSITIVE_MODE)) {
		input_info(true, ilits->dev, "%s: Not supported value:%d\n", __func__, sec->cmd_param[0]);
		ret = -1;
		goto out;
	}

	ilits->prox_face_mode = sec->cmd_param[0];

	if (ilits->tp_suspend) {
		input_info(true, ilits->dev, "%s: now screen off stae, it'll setting after screen on(%d)\n",
						__func__, ilits->prox_face_mode);
		goto out;
	}

	ret = ili_ic_func_ctrl("proximity", ilits->prox_face_mode);
	if (ret < 0)
		input_err(true, ilits->dev, "%s Ear Detect Mode Setting failed\n", __func__);

	if ((ilits->prox_face_mode == EAR_DETECT_DISABLE) && ilits->started_prox_intensity) {
		debug_mode_onoff(false);
		ilits->started_prox_intensity = false;
	}
out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	mutex_unlock(&ilits->touch_mutex);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void prox_lp_scan_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	int ret = 0;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	sec_cmd_set_default_result(sec);
	mutex_lock(&ilits->touch_mutex);
	input_info(true, ilits->dev, "%s: %d\n", __func__, sec->cmd_param[0]);

	if (!ilits->prox_lp_scan_enabled) {
		input_err(true, ilits->dev, "%s: Not support LPSCAN!\n", __func__);
		ret = -1;
		goto out;
	}

	if (ilits->power_status == POWER_ON_STATUS) {
		input_info(true, ilits->dev, "%s  now screen on stae, it'll setting after screen off\n", __func__);
		if (sec->cmd_param[0] == PORX_LP_SCAN_ON)
			ilits->prox_lp_scan_mode_enabled = true;
		else
			ilits->prox_lp_scan_mode_enabled = false;
		goto out;
	}

	if (ilits->power_status != LP_PROX_STATUS) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		ret = -1;
		goto out;
	}

	switch (sec->cmd_param[0]) {
	case PORX_LP_SCAN_OFF:
		if (ilits->prox_face_mode) {
			if (ili_ic_func_ctrl("sleep", SLEEP_IN) < 0)
				input_err(true, ilits->dev, "%s Write sleep in cmd failed\n", __func__);
			ilits->prox_lp_scan_mode_enabled = false;
		} else {
			input_err(true, ilits->dev, "%s prox_face_mode disabled\n", __func__);
		}
		break;
	case PORX_LP_SCAN_ON:
		if (ilits->prox_face_mode) {
			ili_switch_tp_mode(P5_X_FW_GESTURE_MODE);
			ret = ili_ic_func_ctrl("lpwg", 0x21);//proximity report start
			if (ret < 0)
				input_err(true, ilits->dev, "%s proximity report start failed\n", __func__);
			ilits->prox_lp_scan_mode_enabled = true;
		} else {
			input_err(true, ilits->dev, "%s prox_face_mode disabled\n", __func__);
		}

		break;
	default:
		input_info(true, ilits->dev, "%s: Not supported value:%d\n", __func__, sec->cmd_param[0]);
		ret = -1;
		break;
	}
out:
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		snprintf(buff, sizeof(buff), "OK");
		sec->cmd_state = SEC_CMD_STATUS_OK;
	}
	mutex_unlock(&ilits->touch_mutex);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void dead_zone_enable(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);
	mutex_lock(&ilits->touch_mutex);
	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (ilits->tp_suspend) {
			input_info(true, ilits->dev, "%s: now screen off stae, it'll setting after screen on\n",
					__func__, sec->cmd_param[0]);
			ilits->dead_zone_enabled = sec->cmd_param[0];
			snprintf(buff, sizeof(buff), "OK");
			goto out;
		}
		if (sec->cmd_param[0])
			ret = ili_ic_func_ctrl("dead_zone_ctrl", DEAD_ZONE_ENABLE);
		else
			ret = ili_ic_func_ctrl("dead_zone_ctrl", DEAD_ZONE_DISABLE);

		if (ret < 0) {
			input_err(true, ilits->dev, "%s Failed to enable dead zone, cmd:%d\n",
					__func__, sec->cmd_param[0]);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else {
			snprintf(buff, sizeof(buff), "OK");
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
	}
out:
	mutex_unlock(&ilits->touch_mutex);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_sip_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);
	mutex_lock(&ilits->touch_mutex);
	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (ilits->tp_suspend) {
			input_info(true, ilits->dev, "%s: now screen off stae, it'll setting after screen on\n",
					__func__, sec->cmd_param[0]);
			ilits->sip_mode_enabled = sec->cmd_param[0];
			snprintf(buff, sizeof(buff), "OK");
			goto out;
		}
		if (sec->cmd_param[0])
			ret = ili_ic_func_ctrl("sip_mode", SIP_MODE_ENABLE);
		else
			ret = ili_ic_func_ctrl("sip_mode", SIP_MODE_DISABLE);

		if (ret < 0) {
			input_err(true, ilits->dev, "%s Failed to enable sip_mode, cmd:%d\n",
					__func__, sec->cmd_param[0]);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else {
			snprintf(buff, sizeof(buff), "OK");
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
	}
out:
	mutex_unlock(&ilits->touch_mutex);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
}

static void set_game_mode(void *device_data)
{
	struct sec_cmd_data *sec = (struct sec_cmd_data *)device_data;
	char buff[SEC_CMD_STR_LEN] = { 0 };
	int ret;

	sec_cmd_set_default_result(sec);
	mutex_lock(&ilits->touch_mutex);
	if (sec->cmd_param[0] < 0 || sec->cmd_param[0] > 1) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
	} else {
		if (ilits->tp_suspend) {
			input_info(true, ilits->dev, "%s: now screen off stae, it'll setting after screen on\n",
					__func__, sec->cmd_param[0]);
			ilits->game_mode_enabled = sec->cmd_param[0];
			snprintf(buff, sizeof(buff), "OK");
			goto out;
		}
		if (sec->cmd_param[0])
			ret = ili_ic_func_ctrl("lock_point", GAME_MODE_ENABLE);
		else
			ret = ili_ic_func_ctrl("lock_point", GAME_MODE_DISABLE);

		if (ret < 0) {
			input_err(true, ilits->dev, "%s Failed to enable game mode(lock_point), cmd:%d\n",
					__func__, sec->cmd_param[0]);
			snprintf(buff, sizeof(buff), "NG");
			sec->cmd_state = SEC_CMD_STATUS_FAIL;
		} else {
			snprintf(buff, sizeof(buff), "OK");
			sec->cmd_state = SEC_CMD_STATUS_OK;
		}
	}
out:
	mutex_unlock(&ilits->touch_mutex);
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec_cmd_set_cmd_exit(sec);
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
	{SEC_CMD_H("aot_enable", aot_enable),},
	{SEC_CMD_H("spay_enable", spay_enable),},
	{SEC_CMD("get_chip_vendor", get_chip_vendor),},
	{SEC_CMD("get_chip_name", get_chip_name),},
	{SEC_CMD("get_fw_ver_bin", get_fw_ver_bin),},
	{SEC_CMD("get_fw_ver_ic", get_fw_ver_ic),},
	{SEC_CMD("get_x_num", get_x_num),},
	{SEC_CMD("get_y_num", get_y_num),},
	{SEC_CMD("run_sram_test", run_sram_test),},
	{SEC_CMD("run_raw_test_read", run_raw_test_read),},
	{SEC_CMD("run_raw_test_read_all", run_raw_test_read_all),},
	{SEC_CMD("run_raw_doze_test_read", run_raw_doze_test_read),},
	{SEC_CMD("run_raw_doze_test_read_all", run_raw_doze_test_read_all),},
	{SEC_CMD("run_cal_dac_test_read", run_cal_dac_test_read),},
	{SEC_CMD("run_cal_dac_test_read_all", run_cal_dac_test_read_all),},
	{SEC_CMD("run_open_test_read", run_open_test_read),},
	{SEC_CMD("run_open_test_read_all", run_open_test_read_all),},
	{SEC_CMD("run_short_test_read", run_short_test_read),},
	{SEC_CMD("run_short_test_read_all", run_short_test_read_all),},
	{SEC_CMD("run_noise_test_read", run_noise_test_read),},
	{SEC_CMD("run_noise_test_read_all", run_noise_test_read_all),},
	{SEC_CMD("run_noise_doze_test_read", run_noise_doze_test_read),},
	{SEC_CMD("run_noise_doze_test_read_all", run_noise_doze_test_read_all),},
	{SEC_CMD("run_noise_test_read_lcdoff", run_noise_test_read_lcdoff),},
	{SEC_CMD("run_noise_test_read_all_lcdoff", run_noise_test_read_all_lcdoff),},
	{SEC_CMD("run_noise_doze_test_read_lcdoff", run_noise_doze_test_read_lcdoff),},
	{SEC_CMD("run_noise_doze_test_read_all_lcdoff", run_noise_doze_test_read_all_lcdoff),},
	{SEC_CMD("run_raw_doze_test_read_lcdoff", run_raw_doze_test_read_lcdoff),},
	{SEC_CMD("run_raw_doze_test_read_all_lcdoff", run_raw_doze_test_read_all_lcdoff),},
	{SEC_CMD("run_raw_test_read_lcdoff", run_raw_test_read_lcdoff),},
	{SEC_CMD("run_raw_test_read_all_lcdoff", run_raw_test_read_all_lcdoff),},
	{SEC_CMD("factory_cmd_result_all", factory_cmd_result_all),},
	{SEC_CMD("factory_lcdoff_cmd_result_all", factory_lcdoff_cmd_result_all),},
	{SEC_CMD_H("incell_power_control", incell_power_control),},
	{SEC_CMD("run_prox_intensity_read_all", run_prox_intensity_read_all),},
	{SEC_CMD_H("ear_detect_enable", ear_detect_enable),},
	{SEC_CMD_H("prox_lp_scan_mode", prox_lp_scan_mode),},
	{SEC_CMD("dead_zone_enable", dead_zone_enable),},
	{SEC_CMD("set_sip_mode", set_sip_mode),},
	{SEC_CMD_H("set_game_mode", set_game_mode),},
	{SEC_CMD("not_support_cmd", not_support_cmd),},
};

static ssize_t sensitivity_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int i;
	char tempv[10] = { 0 };
	char buff[SENSITIVITY_POINT_CNT * 10] = { 0 };

	for (i = 0; i < SENSITIVITY_POINT_CNT; i++) {
		if (i != 0)
			strlcat(buff, ",", sizeof(buff));
		snprintf(tempv, 10, "%d", ilits->sensitivity_info[i]);
		strlcat(buff, tempv, sizeof(buff));
	}

	input_info(true, ilits->dev, "%s: sensitivity mode : %s\n", __func__, buff);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%s", buff);
}

static ssize_t sensitivity_mode_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	unsigned long value = 0;

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		return -EINVAL;
	}

	if (count > 2)
		return -EINVAL;

	ret = kstrtoul(buf, 10, &value);
	if (ret != 0)
		return ret;

	input_info(true, ilits->dev, "%s: start on/off:%d\n", __func__, value);

	if (value)
		debug_mode_onoff(true);
	else
		debug_mode_onoff(false);

	input_info(true, ilits->dev, "%s: done\n", __func__);
	return count;
}

static ssize_t read_support_feature(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	u32 feature = 0;

	if (ilits->enable_settings_aot)
		feature |= INPUT_FEATURE_ENABLE_SETTINGS_AOT;

	if (ilits->enable_sysinput_enabled)
		feature |= INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED;

	if(ilits->prox_lp_scan_enabled)
		feature |= INPUT_FEATURE_ENABLE_PROX_LP_SCAN_ENABLED;

	input_info(true, ilits->dev, "%s: %d%s%s%s\n",
				__func__, feature,
				feature & INPUT_FEATURE_ENABLE_SETTINGS_AOT ? " aot" : "",
				feature & INPUT_FEATURE_ENABLE_SYSINPUT_ENABLED ? " SE" : "",
				feature & INPUT_FEATURE_ENABLE_PROX_LP_SCAN_ENABLED ? " LPSCAN" : "");

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", feature);
}

static ssize_t prox_power_off_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	input_info(true, ilits->dev, "%s: %d\n", __func__,
			ilits->prox_power_off);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%ld", ilits->prox_power_off);
}

static ssize_t prox_power_off_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	long data;
	int ret;

	ret = kstrtol(buf, 10, &data);
	if (ret < 0)
		return ret;

	input_info(true, ilits->dev, "%s: %ld\n", __func__, data);

	ilits->prox_power_off = data;

	return count;
}

static ssize_t protos_event_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	input_info(true, ilits->dev, "%s: %d\n", __func__,
			ilits->proxmity_face);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ilits->proxmity_face != 3 ? 0 : 3);
}

static ssize_t protos_event_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	u8 data;
	int ret;

	ret = kstrtou8(buf, 10, &data);
	if (ret < 0)
		return ret;

	mutex_lock(&ilits->touch_mutex);
	input_info(true, ilits->dev, "%s: %d\n", __func__, data);

	if ((data != EAR_DETECT_DISABLE) && (data != EAR_DETECT_NORMAL_MODE) && (data != EAR_DETECT_INSENSITIVE_MODE)) {
		input_info(true, ilits->dev, "%s: Not supported value:%d\n", __func__, data);
		mutex_unlock(&ilits->touch_mutex);
		return count;
	}

	ilits->prox_face_mode = data;

	if (ilits->tp_suspend) {
		input_info(true, ilits->dev, "%s: now screen off stae, it'll setting after screen on(%d)\n",
						__func__, ilits->prox_face_mode);
		mutex_unlock(&ilits->touch_mutex);
		return count;
	}

	ret = ili_ic_func_ctrl("proximity", ilits->prox_face_mode);
	if (ret < 0)
		input_err(true, ilits->dev, "%s Ear Detect Mode Setting failed\n", __func__);

	mutex_unlock(&ilits->touch_mutex);
	return count;
}

static ssize_t enabled_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	input_info(true, ilits->dev, "%s: %d\n", __func__, ilits->screen_off_sate);

	return snprintf(buf, SEC_CMD_BUF_SIZE, "%d", ilits->screen_off_sate);
}

static ssize_t enabled_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int buff[2];
	int ret;

	ret = sscanf(buf, "%d,%d", &buff[0], &buff[1]);
	if (ret != 2) {
		input_err(true, ilits->dev,
				"%s: failed read params [%d]\n", __func__, ret);
		return -EINVAL;
	}

	input_info(true, ilits->dev, "%s: %d %d\n", __func__, buff[0], buff[1]);

	/* handle same sequence : buff[0] = LCD_ON, LCD_DOZE1, LCD_DOZE2*/
	if (buff[0] == LCD_DOZE1 || buff[0] == LCD_DOZE2)
		buff[0] = LCD_ON;

	switch (buff[0]) {
	case LCD_OFF:
		if (buff[1] == LCD_EARLY_EVENT) {
			if (ili_sleep_handler(TP_EARLY_SUSPEND) < 0)
				input_err(true, ilits->dev, "%s TP suspend failed\n", __func__);
		}
		break;
	case LCD_ON:
		if (buff[1] == LCD_EARLY_EVENT) {
			if ((ilits->screen_off_sate != TP_EARLY_RESUME) && (ilits->screen_off_sate != TP_RESUME))
				ili_sleep_handler(TP_EARLY_RESUME);
		} else if (buff[1] == LCD_LATE_EVENT) {
			if (ili_sleep_handler(TP_RESUME) < 0)
				input_err(true, ilits->dev, "%s TP resume failed\n", __func__);
		}
		break;
	default:
		ILI_DBG("%s Unknown event\n", __func__);
		break;
	}

	return count;
}

static ssize_t scrub_pos_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char buff[256] = { 0 };

	snprintf(buff, sizeof(buff), "%d %d %d", ilits->scrub_id, 0, 0);
	input_info(true, ilits->dev, "%s: scrub_id: %s\n", __func__, buff);

	return snprintf(buf, PAGE_SIZE, "%s", buff);
}
static DEVICE_ATTR(scrub_pos, S_IRUGO, scrub_pos_show, NULL);
static DEVICE_ATTR(sensitivity_mode, S_IRUGO | S_IWUSR | S_IWGRP, sensitivity_mode_show, sensitivity_mode_store);
static DEVICE_ATTR(support_feature,  S_IRUGO, read_support_feature, NULL);
static DEVICE_ATTR(prox_power_off, S_IRUGO | S_IWUSR | S_IWGRP, prox_power_off_show, prox_power_off_store);
static DEVICE_ATTR(virtual_prox, S_IRUGO | S_IWUSR | S_IWGRP, protos_event_show, protos_event_store);
static DEVICE_ATTR(enabled, S_IRUGO | S_IWUSR | S_IWGRP, enabled_show, enabled_store);


static struct attribute *cmd_attributes[] = {
	&dev_attr_scrub_pos.attr,
	&dev_attr_sensitivity_mode.attr,
	&dev_attr_support_feature.attr,
	&dev_attr_prox_power_off.attr,
	&dev_attr_virtual_prox.attr,
	&dev_attr_enabled.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

int ili_sec_fn_init(void)
{
	int retval;

	input_info(true, ilits->dev, "%s\n", __func__);
	retval = sec_cmd_init(&ilits->sec, sec_cmds,
			ARRAY_SIZE(sec_cmds), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		input_err(true, ilits->dev,
						"%s: Failed to sec_cmd_init\n", __func__);
		goto exit;
	}

	retval = sysfs_create_group(&ilits->sec.fac_dev->kobj,
			&cmd_attr_group);
	if (retval < 0) {
		input_err(true, ilits->dev,
				"%s: Failed to create sysfs attributes\n", __func__);
		goto exit;
	}

	ilits->pFrame = kzalloc(ilits->panel_wid * ilits->panel_hei * sizeof(short), GFP_KERNEL);
	if (!ilits->pFrame) {
		input_err(true, ilits->dev,
				"%s: Failed to alloc memory pFrame\n", __func__);
		retval = -EINVAL;
		goto exit;
	}

	return 0;

exit:
	return retval;
}

void ili_sec_fn_remove(void)
{
	input_info(true, ilits->dev, "%s\n", __func__);

	kfree(ilits->pFrame);

	sysfs_remove_group(&ilits->sec.fac_dev->kobj,
			&cmd_attr_group);

	sec_cmd_exit(&ilits->sec, SEC_CLASS_DEVT_TSP);
}
