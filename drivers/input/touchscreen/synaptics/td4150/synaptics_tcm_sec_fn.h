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

#include "synaptics_tcm_core.h"
#include "synaptics_tcm_testing.h"

#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/spu-verify.h>

#define TEST_MODE_MIN_MAX		false
#define TEST_MODE_ALL_NODE		true
#define CMD_RESULT_WORD_LEN		10
#define SENSITIVITY_POINT_CNT	9

/* factory test mode */
struct sec_factory_test_mode {
	u8 type;
	short min;
	short max;
	bool allnode;
};

enum FW_SIGN {
	NORMAL = 0,
	SIGNING = 1,
};

int run_test(struct syna_tcm_hcd *tcm_hcd, struct sec_factory_test_mode *mode, enum test_code test_code);
int test_abs_cap(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode);
int test_noise(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode);
int test_open_short(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode);
int raw_data_gap_x(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode);
int raw_data_gap_y(struct sec_cmd_data *sec, struct sec_factory_test_mode *mode);
int test_fw_crc(struct sec_cmd_data *sec);
int test_check_connection(struct sec_cmd_data *sec);
int test_sensitivity(void);
int get_proximity(void);
int test_init(struct syna_tcm_hcd *tcm_hcd);
