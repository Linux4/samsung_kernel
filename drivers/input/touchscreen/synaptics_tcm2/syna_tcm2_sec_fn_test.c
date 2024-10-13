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
 * @file syna_tcm2_sec_fn_test.c
 *
 * This file implements the testing interface for TCM2 SEC custom driver.
 */

#include "syna_tcm2_sec_fn.h"
#include "synaptics_touchcom_core_dev.h"
#include "synaptics_touchcom_func_base.h"

static int sec_fn_test_get_face_area(struct syna_tcm *tcm, int *data_sum)
{
	int retval;
	struct tcm_dev *tcm_dev = tcm->tcm_dev;
	unsigned char resp_code;
	struct tcm_buffer resp;
	int row_start, row_end, col_start, col_end, rows, cols;
	int r, c;
	unsigned char *resp_buf;
	short *data;
	const int NOISE_DELTA_MAX = 100;

	rows = tcm_dev->rows;
	cols = tcm_dev->cols;

	LOGI("rows:%d, cols:%d\n", rows, cols);

	syna_tcm_buf_init(&resp);

	retval = syna_tcm_send_command(tcm_dev,
			CMD_GET_FACE_AREA,
			NULL,
			0,
			&resp_code,
			&resp,
			0);
	if (retval < 0) {
		LOGE("Fail to send command %s, 0x%x\n",
			STR(CMD_GET_FACE_AREA), CMD_GET_FACE_AREA);
		goto exit;
	}

	resp_buf = resp.buf;

	if (resp.data_length < 4) {
		LOGE("Invalid data length, %d\n", resp.data_length);
		retval = -EINVAL;
		goto exit;
	}

	row_start = resp_buf[0];
	row_end = resp_buf[1];
	col_start = resp_buf[2];
	col_end = resp_buf[3];

	LOGI("row_start:%d, row_end:%d, col_start:%d, col_end:%d\n",
		row_start, row_end, col_start, col_end);

	if ((row_start > row_end) || (row_end > rows)) {
		LOGE("Invalid row parameter:%d %d %d %d\n",
			row_start, row_end, col_start, col_end);
	}
	if ((col_start > col_end) || (col_end > cols)) {
		LOGE("Invalid cols parameter:%d %d %d %d\n",
			row_start, row_end, col_start, col_end);
	}

	retval = syna_tcm_run_production_test(tcm_dev,
			TEST_FACE_AREA,
			&resp);
	if (retval < 0) {
		LOGE("Fail to run test 0x%x\n", TEST_FACE_AREA);
		goto exit;
	}

	LOGI("face delta length:%d\n", resp.data_length);

	if (resp.data_length != rows * cols * 2) {
		LOGE("Invalid face delta length:%d\n", resp.data_length);
		retval = -EINVAL;
		goto exit;
	}

	*data_sum = 0;
	data = (short *)&resp.buf[row_start * cols + col_start];
	for (r = row_start; r <= row_end; r++) {
		for (c = col_start; c <= col_end; c++) {
			if (*data > NOISE_DELTA_MAX) {
				LOGE("Over the limit, data:%d\n", *data);
				retval = -EINVAL;
				goto exit;
			}
			*data_sum += *data;
			data++;
		}
	}

	LOGI("data_sum: %d\n", *data_sum);
	retval = 0;
exit:

	syna_tcm_buf_release(&resp);

	return retval;
}

int syna_sec_fn_test_get_proximity(struct syna_tcm *tcm)
{
	int sum = 0, retval;

	LOGI("start\n");

	retval = sec_fn_test_get_face_area(tcm, &sum);
	if (retval == 0)
		return sum;

	return -1;
}

int syna_sec_fn_test_init(struct syna_tcm *tcm)
{
	return 0;
}

