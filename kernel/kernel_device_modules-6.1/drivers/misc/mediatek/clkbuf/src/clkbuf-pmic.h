/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#ifndef CLKBUF_PMIC_H
#define CLKBUF_PMIC_H

#include "clkbuf-util.h"
#include "clkbuf-ctrl.h"

#define MAX_XO_CMD (sizeof(xo_api_cmd)\
	/ sizeof(const char *))
enum CLKBUF_DBG_CMD_ID {
	SET_XO_MODE = 0x0001,
	SET_XO_EN_M = 0x0002,
	SET_XO_IMPEDANCE = 0x0004,
	SET_XO_DESENSE = 0x0008,
	SET_XO_VOTER = 0x0010,
};

enum CLKBUF_DBG_TB_CMD_ID {
	SET_CAPID_PRE_1 = 0x1000,
	SET_CAPID_PRE_2 = 0x2000,
	SET_CAPID = 0x4000,
	SET_HEATER = 0x8000,
};

struct xo_buf_t {
	struct reg_t _xo_mode;
	struct reg_t _xo_en;
	struct reg_t _xo_en_auxout;
	struct reg_t _hwbblpm_msk;
	struct reg_t _impedance;
	struct reg_t _de_sense;
	struct reg_t _rc_voter;
	u32 xo_en_auxout_sel;
};

struct common_regs {
	u32 bblpm_auxout_sel;
	u32 mode_num;
	/*restrict to 16bits when write*/
	u32 spmi_mask;
	//struct mutex lock;
	struct reg_t _static_aux_sel;
	struct reg_t _xo_en_auxout;
	struct reg_t _bblpm_auxout;
	struct reg_t _swbblpm_en;
	struct reg_t _hwbblpm_sel;
	struct reg_t _pmrc_en_l;
	struct reg_t _pmrc_en_h;
	struct reg_t _cdac_fpm;
	struct reg_t _aac_fpm_swen;
	struct reg_t _heater_sel;
};

struct plat_xodata {
	struct xo_buf_t *xo_buf_t;
	struct reg_t *debug_regs;
	struct common_regs *common_regs;
	struct clkbuf_hw hw;
	spinlock_t *lock;
};

extern struct plat_xodata mt6685_data;
extern struct plat_xodata mt6685_tb_data;
extern struct plat_xodata mt6377_data;

#endif /* CLKBUF_DCXO_6685P_H */
