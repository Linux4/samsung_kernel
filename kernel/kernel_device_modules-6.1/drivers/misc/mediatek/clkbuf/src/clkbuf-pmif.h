/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#ifndef CLKBUF_PMIF_H
#define CLKBUF_PMIF_H

#include "clkbuf-util.h"
#include "clkbuf-ctrl.h"

#define PMIF_M_ID 0
#define PMIF_P_ID 1

enum PMIF_INF_CMD_ID {
	SET_PMIF_CONN_INF = 0x0001,
	SET_PMIF_NFC_INF = 0x0002,
	SET_PMIF_RC_INF = 0x0004,
	PMIF_INF_MAX = SET_PMIF_RC_INF,
};

struct pmif_m {
	struct reg_t _conn_inf_en;
	struct reg_t _nfc_inf_en;
	struct reg_t _rc_inf_en;
	struct reg_t _conn_clr_addr;
	struct reg_t _conn_set_addr;
	struct reg_t _conn_clr_cmd;
	struct reg_t _conn_set_cmd;
	struct reg_t _nfc_clr_addr;
	struct reg_t _nfc_set_addr;
	struct reg_t _nfc_clr_cmd;
	struct reg_t _nfc_set_cmd;
	struct reg_t _mode_ctrl;
	struct reg_t _slp_ctrl;
};

struct pmif_p {
	struct reg_t _conn_inf_en;
	struct reg_t _nfc_inf_en;
	struct reg_t _rc_inf_en;
	struct reg_t _conn_clr_addr;
	struct reg_t _conn_set_addr;
	struct reg_t _conn_clr_cmd;
	struct reg_t _conn_set_cmd;
	struct reg_t _nfc_clr_addr;
	struct reg_t _nfc_set_addr;
	struct reg_t _nfc_clr_cmd;
	struct reg_t _nfc_set_cmd;
	struct reg_t _mode_ctrl;
	struct reg_t _slp_ctrl;
};

struct plat_pmifdata {
	struct pmif_m *pmif_m;
	struct pmif_p *pmif_p;
	struct clkbuf_hw hw;
	spinlock_t *lock;
};

extern void spmi_dump_pmif_record_reg(void);
extern struct plat_pmifdata pmif_data_v1;
extern struct plat_pmifdata pmif_data_v2;
extern struct plat_pmifdata pmif_data_v3;

#endif /* CLKBUF_DCXO_6685P_H */
