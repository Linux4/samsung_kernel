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

#include "synaptics_tcm_testing.h"
#include "synaptics_tcm_sec_fn.h"

static struct testing_hcd *sec_fn_test;

static void sec_factory_print_frame(struct sec_cmd_data *sec, struct syna_tcm_hcd *tcm_hcd, unsigned char *buf, 
	struct sec_factory_test_mode *mode)
{
	int i = 0;
	int j = 0;
	unsigned char *pStr = NULL;
	unsigned char pTmp[16] = { 0 };
	int lsize;
	short data;

	input_info(true, tcm_hcd->pdev->dev.parent, "%s\n", __func__);

	lsize = 7 * (tcm_hcd->cols + 1);

	pStr = kzalloc(lsize, GFP_KERNEL);
	if (pStr == NULL)
		return;

	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), "      TX");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < tcm_hcd->cols; i++) {
		snprintf(pTmp, sizeof(pTmp), " %02d ", i);
		strlcat(pStr, pTmp, lsize);
	}

	input_info(true, tcm_hcd->pdev->dev.parent, "%s\n", pStr);
	memset(pStr, 0x0, lsize);
	snprintf(pTmp, sizeof(pTmp), " +");
	strlcat(pStr, pTmp, lsize);

	for (i = 0; i < tcm_hcd->cols; i++) {
		snprintf(pTmp, sizeof(pTmp), "----");
		strlcat(pStr, pTmp, lsize);
	}

	input_info(true, tcm_hcd->pdev->dev.parent, "%s\n", pStr);

	for (i = 0; i < tcm_hcd->rows; i++) {
		memset(pStr, 0x0, lsize);
		snprintf(pTmp, sizeof(pTmp), "Rx%02d | ", i);
		strlcat(pStr, pTmp, lsize);
		for (j = 0; j < tcm_hcd->cols; j++) {
			data = le2_to_uint(&buf[(j + (tcm_hcd->cols * i)) * 2]);
			snprintf(pTmp, sizeof(pTmp), " %5d", data);

			if (i == 0 && j == 0)
				mode->min = mode->max = data;

			mode->min = min(mode->min, data);
			mode->max = max(mode->max, data);

			strlcat(pStr, pTmp, lsize);
		}
		input_info(true, tcm_hcd->pdev->dev.parent, "%s\n", pStr);
	}
	input_info(true, tcm_hcd->pdev->dev.parent, "%d %d\n", mode->min, mode->max);
	kfree(pStr);
}

int testing_run_prod_test_item(struct testing_hcd *sec_fn_test, enum test_code test_code)
{
	int retval;
	struct syna_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;

	if (tcm_hcd->features.dual_firmware &&
			tcm_hcd->id_info.mode != MODE_PRODUCTIONTEST_FIRMWARE) {
		retval = tcm_hcd->switch_mode(tcm_hcd, FW_MODE_PRODUCTION_TEST);
		if (retval < 0) {
			input_err(true, tcm_hcd->pdev->dev.parent,
					"Failed to run production test firmware\n");
			return retval;
		}
	} else if (IS_NOT_FW_MODE(tcm_hcd->id_info.mode) ||
			tcm_hcd->app_status != APP_STATUS_OK) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Identifying mode = 0x%02x\n",
				tcm_hcd->id_info.mode);
		return -ENODEV;
	}

	LOCK_BUFFER(sec_fn_test->out);

	retval = syna_tcm_alloc_mem(tcm_hcd,
			&sec_fn_test->out,
			1);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for sec_fn_test->out.buf\n");
		UNLOCK_BUFFER(sec_fn_test->out);
		return retval;
	}

	sec_fn_test->out.buf[0] = test_code;

	LOCK_BUFFER(sec_fn_test->resp);

	retval = tcm_hcd->write_message(tcm_hcd,
			CMD_PRODUCTION_TEST,
			sec_fn_test->out.buf,
			1,
			&sec_fn_test->resp.buf,
			&sec_fn_test->resp.buf_size,
			&sec_fn_test->resp.data_length,
			NULL,
			0);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to write command %s\n",
				STR(CMD_PRODUCTION_TEST));
		UNLOCK_BUFFER(sec_fn_test->resp);
		UNLOCK_BUFFER(sec_fn_test->out);
		return retval;
	}

	UNLOCK_BUFFER(sec_fn_test->resp);
	UNLOCK_BUFFER(sec_fn_test->out);

	return 0;
}

int run_test(struct sec_cmd_data *sec, struct syna_tcm_hcd *tcm_hcd, struct sec_factory_test_mode *mode, enum test_code test_code)
{
	int retval = 0;
	unsigned char *buf;
	unsigned int idx;

	char temp[SEC_CMD_STR_LEN] = { 0 };

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing\n");

	memset(tcm_hcd->print_buf, 0, PAGE_SIZE);
	retval = testing_run_prod_test_item(sec_fn_test, test_code);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	buf = sec_fn_test->resp.buf;

	sec_factory_print_frame(sec, tcm_hcd, buf, mode);

	if (mode->allnode) {
		for (idx = 0; idx < tcm_hcd->rows * tcm_hcd->cols; idx++) {
			tcm_hcd->pFrame[idx] = (short)le2_to_uint(&buf[idx * 2]);
			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", tcm_hcd->pFrame[idx]);
			strlcat(tcm_hcd->print_buf, temp, tcm_hcd->rows * tcm_hcd->cols * CMD_RESULT_WORD_LEN);

			memset(temp, 0x00, SEC_CMD_STR_LEN);			
		}
	} else {
		snprintf(tcm_hcd->print_buf, SEC_CMD_STR_LEN, "%d,%d", mode->min, mode->max);
	}

	return retval;

exit:
	return retval;
}


int test_sensitivity(void)
{
	int retval;
	unsigned char *buf;
	unsigned int idx;
	struct syna_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing\n");

	retval = testing_run_prod_test_item(sec_fn_test, TEST_SENSITIVITY);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	LOCK_BUFFER(sec_fn_test->resp);

	buf = sec_fn_test->resp.buf;

	for (idx = 0; idx < SENSITIVITY_POINT_CNT; idx++) {
		tcm_hcd->pFrame[idx] = (short)le2_to_uint(&buf[idx * 2]);
	}

	UNLOCK_BUFFER(sec_fn_test->resp);

	retval = 0;

	return retval;

exit:
	return retval;
}

int raw_data_gap_x(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode)
{
	int retval;
	unsigned int idx;
	struct syna_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;
	char *buff;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	short node_gap = 0;

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing\n");

	buff = kzalloc(tcm_hcd->rows * tcm_hcd->cols * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto error_buff_alloc_mem;

	for (idx = 0; idx < tcm_hcd->cols * tcm_hcd->rows; idx++) {
		if ((idx + tcm_hcd->cols) % (tcm_hcd->cols) != 0) {
			node_gap = ((tcm_hcd->pFrame[idx - 1] - tcm_hcd->pFrame[idx]) * 100) / tcm_hcd->pFrame[idx];

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, tcm_hcd->cols * tcm_hcd->rows * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
		}
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, tcm_hcd->cols * tcm_hcd->rows * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;
	retval = 0;
	kfree(buff);
error_buff_alloc_mem:
	return retval;
}

int raw_data_gap_y(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode)
{
	int retval;
	unsigned int idx;
	struct syna_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;
	char *buff;
	char temp[SEC_CMD_STR_LEN] = { 0 };
	short node_gap = 0;

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing\n");

	buff = kzalloc(tcm_hcd->rows * tcm_hcd->cols * CMD_RESULT_WORD_LEN, GFP_KERNEL);
	if (!buff)
		goto error_buff_alloc_mem;

	for (idx = tcm_hcd->cols; idx < tcm_hcd->cols * tcm_hcd->rows; idx++) {
			node_gap = ((tcm_hcd->pFrame[idx - tcm_hcd->cols] - tcm_hcd->pFrame[idx]) * 100) / tcm_hcd->pFrame[idx];

			snprintf(temp, CMD_RESULT_WORD_LEN, "%d,", node_gap);
			strlcat(buff, temp, tcm_hcd->cols * tcm_hcd->rows * CMD_RESULT_WORD_LEN);
			memset(temp, 0x00, SEC_CMD_STR_LEN);
	}

	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, tcm_hcd->cols * tcm_hcd->rows * CMD_RESULT_WORD_LEN));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	retval = 0;
	kfree(buff);
error_buff_alloc_mem:
	return retval;
}

int test_abs_cap(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode)
{
	int retval = 0;
	struct syna_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing\n");

	retval = run_test(sec, tcm_hcd, mode, TEST_PT7_DYNAMIC_RANGE);
	if (retval < 0)
		goto exit;

	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strlen(tcm_hcd->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "ABS_CAP");

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return retval;

exit:
	snprintf(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf), "NG");
	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "ABS_CAP");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return retval;
}

int test_noise(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode)
{
	int retval = 0;
	struct syna_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing\n");

	retval = run_test(sec, tcm_hcd, mode, TEST_PT10_DELTA_NOISE);
	if (retval < 0)
		goto exit;

	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strlen(tcm_hcd->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "NOISE");

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return retval;

exit:
	snprintf(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf), "NG");
	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "NOISE");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return retval;
}

int test_open_short(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode)
{
	int retval = 0;
	struct syna_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing\n");

	retval = run_test(sec, tcm_hcd, mode, TEST_PT11_OPEN_DETECTION);
	if (retval < 0)
		goto exit;

	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strlen(tcm_hcd->print_buf));
	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "OPEN_SHORT");

	sec->cmd_state = SEC_CMD_STATUS_OK;

	return retval;

exit:
	snprintf(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf), "NG");
	sec_cmd_set_cmd_result(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)));

	if (sec->cmd_all_factory_state == SEC_CMD_STATUS_RUNNING)
		sec_cmd_set_cmd_result_all(sec, tcm_hcd->print_buf, strnlen(tcm_hcd->print_buf, sizeof(tcm_hcd->print_buf)), "OPEN_SHORT");
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return retval;
}

int test_check_connection(struct sec_cmd_data *sec)
{
	int retval = 0;
	struct syna_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing\n");

	retval = testing_run_prod_test_item(sec_fn_test, TEST_CHECK_CONNECT);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	input_err(true, tcm_hcd->pdev->dev.parent,
				"%d\n",  sec_fn_test->resp.buf[0]);

	if (sec_fn_test->resp.buf[0] == 0)
		goto exit;

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return retval;

exit:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return retval;
}

int test_fw_crc(struct sec_cmd_data *sec)
{
	int retval = 0;
	struct syna_tcm_hcd *tcm_hcd = sec_fn_test->tcm_hcd;
	char buff[SEC_CMD_STR_LEN] = { 0 };

	input_dbg(true, tcm_hcd->pdev->dev.parent,
			"Start testing\n");

	retval = testing_run_prod_test_item(sec_fn_test, TEST_FW_CRC);
	if (retval < 0) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to run test\n");
		goto exit;
	}

	input_err(true, tcm_hcd->pdev->dev.parent,
				"%d\n", sec_fn_test->resp.buf[0]);

	if (sec_fn_test->resp.buf[0] == 0)
		goto exit;

	snprintf(buff, sizeof(buff), "OK");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_OK;

	return retval;

exit:
	snprintf(buff, sizeof(buff), "NG");
	sec_cmd_set_cmd_result(sec, buff, strnlen(buff, sizeof(buff)));
	sec->cmd_state = SEC_CMD_STATUS_FAIL;

	return retval;
}

int test_init(struct syna_tcm_hcd *tcm_hcd)
{
	sec_fn_test = kzalloc(sizeof(*sec_fn_test), GFP_KERNEL);
	if (!sec_fn_test) {
		input_err(true, tcm_hcd->pdev->dev.parent,
				"Failed to allocate memory for sec_fn_test\n");
		return -ENOMEM;
	}

	sec_fn_test->tcm_hcd = tcm_hcd;

	INIT_BUFFER(sec_fn_test->out, false);
	INIT_BUFFER(sec_fn_test->resp, false);
	INIT_BUFFER(sec_fn_test->report, false);
	INIT_BUFFER(sec_fn_test->process, false);
	INIT_BUFFER(sec_fn_test->output, false);

	return 0;
}
