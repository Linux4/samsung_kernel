/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef __DRV_CLK_MMDVFS_V3_H
#define __DRV_CLK_MMDVFS_V3_H

#include <dt-bindings/clock/mmdvfs-clk.h>
#include <linux/workqueue.h>

#define MAX_OPP		(8)
#define IPI_TIMEOUT_MS	(200U)

#define MMDVFS_RST_CLK_NUM	(3)

#define MMDVFS_DBG(fmt, args...) \
	pr_notice("[mmdvfs][dbg]%s: "fmt"\n", __func__, ##args)
#define MMDVFS_ERR(fmt, args...) \
	pr_notice("[mmdvfs][err]%s: "fmt"\n", __func__, ##args)

struct mtk_mmdvfs_clk {
	const char *name;
	u8 clk_id;
	u8 pwr_id;
	u8 user_id;
	u8 ipi_type;
	u8 spec_type;
	u8 opp;
	u8 freq_num;
	u32 freqs[MAX_OPP];
	struct clk_hw clk_hw;
};

/* vcp/.../mmdvfs_private.h */
enum {
	LOG_DEBUG_ON,
	LOG_IPI,
	LOG_CLOCK,
	LOG_HOPPING,
	LOG_POWER,
	LOG_MUX,
	LOG_IRQ,
	LOG_LEVEL,
	LOG_PROFILE,
	LOG_NUM
};

enum {
	FUNC_MMDVFS_INIT,
	FUNC_CLKMUX_ENABLE,
	FUNC_VMM_CEIL_ENABLE,
	FUNC_VMM_GENPD_NOTIFY,
	FUNC_VMM_AVS_UPDATE,
	FUNC_CAMSV_DC_ENABLE,
	FUNC_FORCE_OPP,
	FUNC_VOTE_OPP,
	FUNC_MMDVFSRC_INIT,
	FUNC_MMDVFS_LP_MODE,
	FUNC_SWRGO_INIT,

	TEST_SET_CLOCK = 11,
	TEST_SET_MUX,
	TEST_GET_OPP,
	TEST_SET_RATE,
	TEST_AP_SET_OPP,
	TEST_AP_SET_USER_RATE,
	FUNC_FORCE_VOL,
	FUNC_FORCE_CLK,
	FUNC_FORCE_SINGLE_CLK,
	FUNC_MMDVFS_PROFILE,

	FUNC_NUM
};

struct mmdvfs_ipi_data {
	uint8_t func;
	uint8_t idx;
	uint8_t opp;
	uint8_t ack;
	uint32_t base;
};

struct mmdvfs_vmm_notify_work {
	struct work_struct vmm_notify_work;
	bool enable;
};

#endif /* __DRV_CLK_MMDVFS_V3_H */
