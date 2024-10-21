/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Wendy-ST Lin <wendy-st.lin@mediatek.com>
 */
#ifndef MMQOS_VCP_H
#define MMQOS_VCP_H

#include "mmqos-global.h"
#include "mmqos-vcp-memory.h"

#define IPI_TIMEOUT_MS	(200U)
#define VCP_TEST_NUM	(100)

#if IS_ENABLED(CONFIG_MTK_MMQOS_VCP)
int mmqos_vcp_init_thread(void *data);
bool mmqos_is_init_done(void);
int mmqos_vcp_ipi_send(const u8 func, const u8 idx, u32 *data);
int mtk_mmqos_enable_vcp(const bool enable);
#else
inline int mmqos_vcp_init_thread(void *data)
{	return -EINVAL; }
inline bool mmqos_is_init_done(void) { return false; }
inline int mmqos_vcp_ipi_send(const u8 func, const u8 idx, u32 *data)
{	return -EINVAL; }
int mtk_mmqos_enable_vcp(const bool enable)
{	return -EINVAL; }
#endif /* CONFIG_MTK_MMQOS_VCP*/

struct mmqos_ipi_data {
	uint8_t func;
	uint8_t idx;
	uint8_t ack;
	uint32_t base;
};

enum vcp_log_flag {
	LOG_OFF = 0,
	LOG_VMMRC = BIT(0),
	LOG_TEST = BIT(1),
	LOG_BW = BIT(2),
	LOG_SMI = BIT(3),
	LOG_PROFILE = BIT(4),
	LOG_DEBUG_ON = BIT(5),
	LOG_ALL_ON = BIT(0) | BIT(1) | BIT(2) | BIT(3) | BIT(4) | BIT(5),
};

enum ipi_func_id {
	FUNC_MMQOS_INIT,
	FUNC_TEST,
	FUNC_SYNC_STATE,
	FUNC_NUM
};
#endif /* MMQOS_VCP_H */
