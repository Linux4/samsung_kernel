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


void sec_factory_print_frame(u32 *buf)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[32] = { 0 };
	int lsize;
	u32 data;
	char temp[SEC_CMD_STR_LEN] = { 0 };

	input_info(true, ilits->dev, "%s\n", __func__);

	lsize = 9 * (ilits->xch_num + 1);

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(temp, 0x00, SEC_CMD_STR_LEN);

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), "      TX");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ilits->xch_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "%7d ", i);
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, ilits->dev, "%s\n", pStr);
	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < ilits->xch_num; i++) {
		snprintf(pTmp, sizeof(pTmp), "----");
		strlcat(pStr, pTmp, lsize);
	}

	input_raw_info(true, ilits->dev, "%s\n", pStr);

	for (i = 0; i < ilits->ych_num; i++) {
		memset(pStr, 0x0, lsize);
		snprintf(pTmp, sizeof(pTmp), "Rx%02d |", i);
		strlcat(pStr, pTmp, lsize);
		for (j = 0; j < ilits->xch_num; j++) {
			data = buf[(j + (ilits->xch_num * i))];
			snprintf(pTmp, sizeof(pTmp), " %7d", data);
			strlcat(pStr, pTmp, lsize);
			if (ilits->allnode) {
				snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", data);
				strlcat(ilits->print_buf, temp, ilits->xch_num * ilits->ych_num * CMD_RESULT_WORD_LEN);
			}

			if (i == 0 && j == 0)
				ilits->node_min = ilits->node_max = data;

			ilits->node_min = min(ilits->node_min, data);
			ilits->node_max = max(ilits->node_max, data);
		}
		input_raw_info(true, ilits->dev, "%s\n", pStr);
	}
	if (!ilits->allnode)
		snprintf(ilits->print_buf, SEC_CMD_STR_LEN, "%d,%d", ilits->node_min, ilits->node_max);

	kfree(pStr);
}

int ilitek_node_mp_test_read(struct sec_cmd_data *sec, char *ini_path, int lcm_state)
{
	int ret = 0, len = 2;
	bool esd_en = ilits->wq_esd_ctrl, bat_en = ilits->wq_bat_ctrl;
	static unsigned char g_user_buf[USER_STR_BUFF] = {0};

	ilits->md_ini_path = ini_path;
	input_info(true, ilits->dev, "%s path : %s, lcm_state:%d\n", __func__, ilits->md_ini_rq_path, lcm_state);

	mutex_lock(&ilits->touch_mutex);

	input_info(true, ilits->dev, "%s\n", __func__);
	memset(ilits->print_buf, 0, ilits->ych_num * ilits->xch_num * CMD_RESULT_WORD_LEN);
	/* Create the directory for mp_test result */
	if (lcm_state) {
		if ((dev_mkdir(CSV_LCM_ON_PATH, S_IRUGO | S_IWUSR)) != 0)
			input_err(true, ilits->dev, "%s Failed to create directory for mp_test\n", __func__);
	} else {
		if ((dev_mkdir(CSV_LCM_OFF_PATH, S_IRUGO | S_IWUSR)) != 0)
			input_err(true, ilits->dev, "%s Failed to create directory for mp_test\n", __func__);
	}

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, DISABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, DISABLE);

	memset(g_user_buf, 0, USER_STR_BUFF * sizeof(unsigned char));
	ilits->mp_ret_len = 0;

	ret = ili_mp_test_handler(g_user_buf, lcm_state);
	input_info(true, ilits->dev, "%s MP TEST %s, Error code = %d\n", __func__, (ret < 0) ? "FAIL" : "PASS", ret);

	g_user_buf[0] = 3;
	g_user_buf[1] = (ret < 0) ? -ret : ret;
	len += ilits->mp_ret_len;

	if (g_user_buf[1] == EMP_MODE)
		input_err(true, ilits->dev, "%s Failed to switch MP mode, abort!", __func__);
	else if (g_user_buf[1] == EMP_FW_PROC)
		input_err(true, ilits->dev, "%s FW still upgrading, abort!", __func__);
	else if (g_user_buf[1] == EMP_FORMUL_NULL)
		input_err(true, ilits->dev, "%s MP formula is null, abort!", __func__);
	else if (g_user_buf[1] == EMP_INI)
		input_err(true, ilits->dev, "%s Not found ini file, abort!", __func__);
	else if (g_user_buf[1] == EMP_NOMEM)
		input_err(true, ilits->dev, "%s Failed to allocated memory, abort!", __func__);
	else if (g_user_buf[1] == EMP_PROTOCOL)
		input_err(true, ilits->dev, "%s Protocol version isn't matched, abort!", __func__);
	else if (g_user_buf[1] == EMP_TIMING_INFO)
		input_err(true, ilits->dev, "%s Failed to get timing info, abort!", __func__);
	else if (g_user_buf[1] == EMP_PARA_NULL)
		input_err(true, ilits->dev, "%s Failed to get mp parameter, abort!", __func__);

	if (esd_en)
		ili_wq_ctrl(WQ_ESD, ENABLE);
	if (bat_en)
		ili_wq_ctrl(WQ_BAT, ENABLE);

	mutex_unlock(&ilits->touch_mutex);

	return ret;
}


int test_sram(struct sec_cmd_data *sec)
{

	int ret = 0;
	char buff[16] = { 0 };

	if (ilits->tp_suspend) {
		input_err(true, ilits->dev, "%s failed(screen off state).\n", __func__);
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		snprintf(buff, sizeof(buff), "NG");
		goto sram_out_2;
	}

	ret = ili_reset_ctrl(TP_IC_WHOLE_RST);
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, ilits->dev, "ili_reset_ctrl failed,ret:%d\n", __func__, ret);
		goto sram_out;
	}

	msleep(30);

	ret = ili_ice_mode_ctrl(ENABLE, OFF);
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, ilits->dev, "ili_ice_mode_ctrl failed,ret:%d\n", __func__, ret);
		goto sram_out;
	}

	ret = ili_tddi_ic_sram_test();
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "%d", TEST_RESULT_FAIL);
		input_err(true, ilits->dev, "ili_tddi_ic_sram_test failed,ret:%d\n", __func__, ret);
	} else
		snprintf(buff, sizeof(buff), "%d", TEST_RESULT_PASS);

	sec->cmd_state = SEC_CMD_STATUS_OK;

sram_out:
	/*Need reset after sram test. Because FW already be broke after sram test.*/
	ret = ili_reset_ctrl(TP_IC_WHOLE_RST);
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, ilits->dev, "ili_reset_ctrl 2 failed,ret:%d\n", __func__, ret);
	}

	mdelay(50);

	ret = ili_fw_upgrade_handler(NULL);
	if (ret < 0) {
		snprintf(buff, sizeof(buff), "NG");
		sec->cmd_state = SEC_CMD_STATUS_FAIL;
		input_err(true, ilits->dev, "ili_fw_upgrade_handler fw upgrade Failed, ret = %d\n", ret);
	}

sram_out_2:
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, buff, strnlen(buff, sizeof(buff)), "SRAM");

	input_info(true, ilits->dev, "%s final result: %s\n", __func__, buff);

	return ret;
}

int debug_mode_onoff(bool enabled)
{

	int ret = 0;
	u8 temp[5] = { 0 };


	if ((ilits->power_status == POWER_OFF_STATUS) || ilits->tp_shutdown) {
		input_err(true, ilits->dev, "%s failed(power off state).\n", __func__);
		return -1;
	}

	ili_irq_disable();

	if (enabled)
		temp[0] = DEBUG_MODE;
	else
		temp[0] = AP_MODE;

	ret = ili_tp_data_mode_ctrl(temp);
	if (ret < 0) {
		input_err(true, ilits->dev, "%s: send sensitivity mode fail!\n", __func__);
		ili_irq_enable();
		return ret;
	}

	msleep(50);
	ili_irq_enable();
	msleep(30);
	return ret;
}