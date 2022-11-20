/* linux/drivers/video/exynos/decon/panels/s6e3hf2_wqhd_param.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __S6E3HF2_WQHD_PARAM_H__
#define __S6E3HF2_WQHD_PARAM_H__

#include <linux/types.h>
#include <linux/kernel.h>

struct lcd_seq_info {
	unsigned char	*cmd;
	unsigned int	len;
	unsigned int	sleep;
};

static const unsigned char SEQ_TEST_KEY_ON_F0[] = {
	0xF0,
	0x5A, 0x5A
};

static const unsigned char SEQ_TEST_KEY_OFF_F0[] = {
	0xF0,
	0xA5, 0xA5
};

static const unsigned char SEQ_TEST_KEY_ON_F1[] = {
	0xF1,
	0x5A, 0x5A,
};

static const unsigned char SEQ_TEST_KEY_OFF_F1[] = {
	0xF1,
	0xA5, 0xA5,
};

static const unsigned char SEQ_TEST_KEY_ON_FC[] = {
	0xFC,
	0x5A, 0x5A
};

static const unsigned char SEQ_TEST_KEY_OFF_FC[] = {
	0xFC,
	0xA5, 0xA5
};

static const unsigned char SEQ_SLEEP_OUT[] = {
	0x11,
	0x00, 0x00
};

static const unsigned char SEQ_DISPLAY_ON[] = {
	0x29,
	0x00, 0x00
};

static const unsigned char SEQ_SINGLE_DSI_1[] = {
	0xF2,
	0x67, 0x00
};

static const unsigned char SEQ_SINGLE_DSI_2[] = {
	0xF9,
	0x29, 0x00
};

static const unsigned char SEQ_GAMMA_CONDITION_SET[] = {
	0xCA,
	0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x80, 0x80, 0x80,
	0x00, 0x00, 0x00,
	0x00, 0x00
};

static const unsigned char SEQ_AOR_CONTROL[] = {
	0xB2,
	0x03, 0x10
};

static const unsigned char SEQ_ELVSS_SET[] = {
	0xB6,
	0x9C,	/* MPS_CON: ACL OFF */
	0x0A	/* ELVSS: MAX*/
};

static const unsigned char SEQ_GAMMA_UPDATE[] = {
	0xF7,
	0x03, 0x00
};

static const unsigned char SEQ_GAMMA_UPDATE_L[] = {
	0xF7,
	0x00, 0x00
};

static const unsigned char SEQ_ACL_OFF[] = {
	0x55,
	0x00, 0x00
};

static const unsigned char SEQ_ACL_OFF_OPR[] = {
	0xB5,
	0x40, 0x00
};

static const unsigned char SEQ_TSET_GLOBAL[] = {
	0xB0,
	0x07, 0x00
};

static const unsigned char SEQ_TSET[] = {
	0xB8,
	0x19, 0x00
};

static const unsigned char SEQ_TE_ON[] = {
	0x35,
	0x00, 0x00
};

static const unsigned char SEQ_TSP_TE[] = {
	0xBD,
	0x11, 0x11, 0x02, 0x16, 0x02, 0x16
};

static const unsigned char SEQ_PENTILE_SETTING[] = {
	0xC0,
	0x00, 0x00, 0xD8, 0xD8
};

static const unsigned char SEQ_POC_SETTING1[] = {
	0xC3,
	0xC0, 0x00, 0x33
};

static const unsigned char SEQ_POC_SETTING2[] = {
	0xB0,
	0x20, 0x00
};

static const unsigned char SEQ_POC_SETTING3[] = {
	0xFE,
	0x08, 0x00
};

static const unsigned char SEQ_PCD_SETTING[] = {
	0xCC,
	0x40, 0x51
};

static const unsigned char SEQ_ERR_FG_SETTING[] = {
	0xED,
	0x44, 0x00
};

static const unsigned char SEQ_HBM_OFF[] = {
	0x53,
	0x00, 0x00
};

static const unsigned char SEQ_TE_START_SETTING[] = {
	0xB9,
	0x10, 0x09, 0xFF, 0x00, 0x09
};



#endif /* __S6E3HF2_PARAM_H__ */
