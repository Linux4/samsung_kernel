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
#ifndef __ILI9881X_SEC_FN_H
#define __ILI9881X_SEC_FN_H

#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#if IS_ENABLED(CONFIG_SPU_VERIFY)
#define SUPPORT_FW_SIGNED
#endif
#ifdef SUPPORT_FW_SIGNED
#include <linux/spu-verify.h>
#endif
#include "ili9881x.h"
#define TEST_MODE_MIN_MAX		false
#define TEST_MODE_ALL_NODE		true
#define CMD_RESULT_WORD_LEN		10
#define SENSITIVITY_POINT_CNT	9
#define TEST_RESULT_PASS 0
#define TEST_RESULT_FAIL 1
#define USER_STR_BUFF		PAGE_SIZE

#define TSP_PATH_EXTERNAL_FW		"/sdcard/Firmware/TSP/tsp.hex"
#define TSP_PATH_EXTERNAL_FW_SIGNED	"/sdcard/Firmware/TSP/tsp_signed.hex"

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

enum PROX_LP_SCAN {
	PORX_LP_SCAN_OFF = 0,
	PORX_LP_SCAN_ON = 1,
};

enum DER_DETECT {
	EAR_DETECT_DISABLE =  0,
	EAR_DETECT_NORMAL_MODE =  1,
	EAR_DETECT_INSENSITIVE_MODE = 3,
};

enum INCELL_POWER {
	INCELL_POWER_DISABLE = 0,
	INCELL_POWER_ENABLE = 1,
};

enum DEAD_ZONE {
	DEAD_ZONE_DISABLE = 3,
	DEAD_ZONE_ENABLE = 7,
};

enum SIP_MODE {
	SIP_MODE_DISABLE = 0,
	SIP_MODE_ENABLE = 1,
};

enum GAME_MODE {
	GAME_MODE_DISABLE = 0,
	GAME_MODE_ENABLE = 1,
};

enum {
	BUILT_IN = 0,
	UMS,
	BL,
	FFU,
};

int ili_sec_fn_init(void);
void ili_sec_fn_remove(void);
int test_sram(struct sec_cmd_data *sec);
int ilitek_node_mp_test_read(struct sec_cmd_data *sec, char *log_path, int lcm_state);
void sec_factory_print_frame(u32 *buf);
int debug_mode_onoff(bool enabled);
#endif /* __ILI9881X_SEC_FN_H */