// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Kuan-Hsin Lee <Kuan-Hsin.Lee@mediatek.com>
 */
#include <linux/types.h>
#include "clkbuf-util.h"
#include "clkbuf-pmif.h"


/* PMIF V1 */
#define RC_INF_EN_V1_ADDR		(0x24)
#define RC_INF_EN_V1_MASK		(0x1)
#define RC_INF_EN_V1_SHIFT		(4)
#define CONN_INF_EN_V1_ADDR		(0x28)
#define CONN_INF_EN_V1_MASK		(0x1)
#define CONN_INF_EN_V1_SHIFT		(0)
#define NFC_INF_EN_V1_ADDR		(0x28)
#define NFC_INF_EN_V1_MASK		(0x1)
#define NFC_INF_EN_V1_SHIFT		(1)
#define CONN_CMD_DEST_V1_ADDR		(0x5C)
#define CONN_CLR_CMD_DEST_V1_ADDR	(CONN_CMD_DEST_V1_ADDR)
#define CONN_CLR_CMD_DEST_V1_MASK	(0xFFFF)
#define CONN_CLR_CMD_DEST_V1_SHIFT	(0)
#define CONN_SET_CMD_DEST_V1_ADDR	(CONN_CMD_DEST_V1_ADDR)
#define CONN_SET_CMD_DEST_V1_MASK	(0xFFFF)
#define CONN_SET_CMD_DEST_V1_SHIFT	(16)
#define CONN_CMD_V1_ADDR		(0x60)
#define CONN_CLR_CMD_V1_ADDR		(CONN_CMD_V1_ADDR)
#define CONN_CLR_CMD_V1_MASK		(0xFFFF)
#define CONN_CLR_CMD_V1_SHIFT		(0)
#define CONN_SET_CMD_V1_ADDR		(CONN_CMD_V1_ADDR)
#define CONN_SET_CMD_V1_MASK		(0xFFFF)
#define CONN_SET_CMD_V1_SHIFT		(16)
#define NFC_CMD_DEST_V1_ADDR		(0x64)
#define NFC_CLR_CMD_DEST_V1_ADDR	(NFC_CMD_DEST_V1_ADDR)
#define NFC_CLR_CMD_DEST_V1_MASK	(0xFFFF)
#define NFC_CLR_CMD_DEST_V1_SHIFT	(0)
#define NFC_SET_CMD_DEST_V1_ADDR	(NFC_CMD_DEST_V1_ADDR)
#define NFC_SET_CMD_DEST_V1_MASK	(0xFFFF)
#define NFC_SET_CMD_DEST_V1_SHIFT	(16)
#define NFC_CMD_V1_ADDR			(0x68)
#define NFC_CLR_CMD_V1_ADDR		(NFC_CMD_V1_ADDR)
#define NFC_CLR_CMD_V1_MASK		(0xFFFF)
#define NFC_CLR_CMD_V1_SHIFT		(0)
#define NFC_SET_CMD_V1_ADDR		(NFC_CMD_V1_ADDR)
#define NFC_SET_CMD_V1_MASK		(0xFFFF)
#define NFC_SET_CMD_V1_SHIFT		(16)
#define SLP_PROTECT_V1_ADDR		(0x3E8)
#define SLP_PROTECT_V1_MASK		(0xFFFFFFFF)
#define SLP_PROTECT_V1_SHIFT		(0)
#define MODE_CTRL_V1_ADDR		(0x400)
#define MODE_CTRL_V1_MASK		(0xFFFFFFFF)
#define MODE_CTRL_V1_SHIFT		(0)

#define RC_INF_EN_V2_ADDR		(0x24)
#define RC_INF_EN_V2_MASK		(0x1)
#define RC_INF_EN_V2_SHIFT		(4)
#define CONN_INF_EN_V2_ADDR		(0x28)
#define CONN_INF_EN_V2_MASK		(0x1)
#define CONN_INF_EN_V2_SHIFT		(0)
#define NFC_INF_EN_V2_ADDR		(0x28)
#define NFC_INF_EN_V2_MASK		(0x1)
#define NFC_INF_EN_V2_SHIFT		(1)
#define CONN_CMD_DEST_V2_ADDR		(0x5C)
#define CONN_CLR_CMD_DEST_V2_ADDR	(CONN_CMD_DEST_V2_ADDR)
#define CONN_CLR_CMD_DEST_V2_MASK	(0xFFFF)
#define CONN_CLR_CMD_DEST_V2_SHIFT	(0)
#define CONN_SET_CMD_DEST_V2_ADDR	(CONN_CMD_DEST_V2_ADDR)
#define CONN_SET_CMD_DEST_V2_MASK	(0xFFFF)
#define CONN_SET_CMD_DEST_V2_SHIFT	(16)
#define CONN_CMD_V2_ADDR		(0x60)
#define CONN_CLR_CMD_V2_ADDR		(CONN_CMD_V2_ADDR)
#define CONN_CLR_CMD_V2_MASK		(0xFFFF)
#define CONN_CLR_CMD_V2_SHIFT		(0)
#define CONN_SET_CMD_V2_ADDR		(CONN_CMD_V2_ADDR)
#define CONN_SET_CMD_V2_MASK		(0xFFFF)
#define CONN_SET_CMD_V2_SHIFT		(16)
#define NFC_CMD_DEST_V2_ADDR		(0x64)
#define NFC_CLR_CMD_DEST_V2_ADDR	(NFC_CMD_DEST_V2_ADDR)
#define NFC_CLR_CMD_DEST_V2_MASK	(0xFFFF)
#define NFC_CLR_CMD_DEST_V2_SHIFT	(0)
#define NFC_SET_CMD_DEST_V2_ADDR	(NFC_CMD_DEST_V2_ADDR)
#define NFC_SET_CMD_DEST_V2_MASK	(0xFFFF)
#define NFC_SET_CMD_DEST_V2_SHIFT	(16)
#define NFC_CMD_V2_ADDR			(0x68)
#define NFC_CLR_CMD_V2_ADDR		(NFC_CMD_V2_ADDR)
#define NFC_CLR_CMD_V2_MASK		(0xFFFF)
#define NFC_CLR_CMD_V2_SHIFT		(0)
#define NFC_SET_CMD_V2_ADDR		(NFC_CMD_V2_ADDR)
#define NFC_SET_CMD_V2_MASK		(0xFFFF)
#define NFC_SET_CMD_V2_SHIFT		(16)
#define SLP_PROTECT_V2_ADDR		(0x3F0)
#define SLP_PROTECT_V2_MASK		(0xFFFFFFFF)
#define SLP_PROTECT_V2_SHIFT		(0)
#define MODE_CTRL_V2_ADDR		(0x408)
#define MODE_CTRL_V2_MASK		(0xFFFFFFFF)
#define MODE_CTRL_V2_SHIFT		(0)

struct pmif_m pmif_m_v1 = {
	SET_REG_BY_NAME(conn_inf_en, CONN_INF_EN_V1)
	SET_REG_BY_NAME(nfc_inf_en, NFC_INF_EN_V1)
	SET_REG_BY_NAME(rc_inf_en, RC_INF_EN_V1)
	SET_REG_BY_NAME(conn_clr_addr, CONN_CLR_CMD_DEST_V1)
	SET_REG_BY_NAME(conn_set_addr, CONN_SET_CMD_DEST_V1)
	SET_REG_BY_NAME(conn_clr_cmd, CONN_CLR_CMD_V1)
	SET_REG_BY_NAME(conn_set_cmd, CONN_SET_CMD_V1)
	SET_REG_BY_NAME(nfc_clr_addr, NFC_CLR_CMD_DEST_V1)
	SET_REG_BY_NAME(nfc_set_addr, NFC_SET_CMD_DEST_V1)
	SET_REG_BY_NAME(nfc_clr_cmd, NFC_CLR_CMD_V1)
	SET_REG_BY_NAME(nfc_set_cmd, NFC_SET_CMD_V1)
	SET_REG_BY_NAME(mode_ctrl, MODE_CTRL_V1)
	SET_REG_BY_NAME(slp_ctrl, SLP_PROTECT_V1)
};

struct pmif_m pmif_m_v2 = {
	SET_REG_BY_NAME(conn_inf_en, CONN_INF_EN_V2)
	SET_REG_BY_NAME(nfc_inf_en, NFC_INF_EN_V2)
	SET_REG_BY_NAME(rc_inf_en, RC_INF_EN_V2)
	SET_REG_BY_NAME(conn_clr_addr, CONN_CLR_CMD_DEST_V2)
	SET_REG_BY_NAME(conn_set_addr, CONN_SET_CMD_DEST_V2)
	SET_REG_BY_NAME(conn_clr_cmd, CONN_CLR_CMD_V2)
	SET_REG_BY_NAME(conn_set_cmd, CONN_SET_CMD_V2)
	SET_REG_BY_NAME(nfc_clr_addr, NFC_CLR_CMD_DEST_V2)
	SET_REG_BY_NAME(nfc_set_addr, NFC_SET_CMD_DEST_V2)
	SET_REG_BY_NAME(nfc_clr_cmd, NFC_CLR_CMD_V2)
	SET_REG_BY_NAME(nfc_set_cmd, NFC_SET_CMD_V2)
	SET_REG_BY_NAME(mode_ctrl, MODE_CTRL_V2)
	SET_REG_BY_NAME(slp_ctrl, SLP_PROTECT_V2)
};

struct pmif_p pmif_p_v2 = {
	SET_REG_BY_NAME(conn_inf_en, CONN_INF_EN_V2)
	SET_REG_BY_NAME(nfc_inf_en, NFC_INF_EN_V2)
	SET_REG_BY_NAME(rc_inf_en, RC_INF_EN_V2)
	SET_REG_BY_NAME(conn_clr_addr, CONN_CLR_CMD_DEST_V2)
	SET_REG_BY_NAME(conn_set_addr, CONN_SET_CMD_DEST_V2)
	SET_REG_BY_NAME(conn_clr_cmd, CONN_CLR_CMD_V2)
	SET_REG_BY_NAME(conn_set_cmd, CONN_SET_CMD_V2)
	SET_REG_BY_NAME(nfc_clr_addr, NFC_CLR_CMD_DEST_V2)
	SET_REG_BY_NAME(nfc_set_addr, NFC_SET_CMD_DEST_V2)
	SET_REG_BY_NAME(nfc_clr_cmd, NFC_CLR_CMD_V2)
	SET_REG_BY_NAME(nfc_set_cmd, NFC_SET_CMD_V2)
	SET_REG_BY_NAME(mode_ctrl, MODE_CTRL_V2)
	SET_REG_BY_NAME(slp_ctrl, SLP_PROTECT_V2)
};

struct plat_pmifdata pmif_data_v1 = {
	.pmif_m = &pmif_m_v1,
};

struct plat_pmifdata pmif_data_v2 = {
	.pmif_m = &pmif_m_v2,
};

struct plat_pmifdata pmif_data_v3 = {
	.pmif_m = &pmif_m_v2,
	.pmif_p = &pmif_p_v2,
};
