/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2022 MediaTek Inc.
 * Author: Anthony Huang <anthony.huang@mediatek.com>
 */

#ifndef _MMQOS_VCP_MEMORY_H_
#define _MMQOS_VCP_MEMORY_H_

#if IS_ENABLED(CONFIG_MTK_MMQOS_VCP)
void *mmqos_get_vcp_base(phys_addr_t *pa);
#else
static inline void *mmqos_get_vcp_base(phys_addr_t *pa)
{
	if (pa)
		*pa = 0;
	return NULL;
}
#endif

#define MEM_BASE			mmqos_get_vcp_base(NULL)
#define MEM_LOG_FLAG			(MEM_BASE + 0x0)
#define MEM_MMQOS_STATE			(MEM_BASE + 0x4)
#define MEM_TEST			(MEM_BASE + 0x8)
#define MEM_IPI_SYNC_FUNC		(MEM_BASE + 0xC)
#define MEM_IPI_SYNC_DATA		(MEM_BASE + 0x10)

#define MEM_VCP_TOTAL_BW		(MEM_BASE + 0x14)
#define MEM_SMI_COMM0_CHN0_BW		(MEM_BASE + 0x18)
#define MEM_SMI_COMM0_CHN1_BW		(MEM_BASE + 0x1c)
#define MEM_SMI_COMM1_CHN0_BW		(MEM_BASE + 0x20)
#define MEM_SMI_COMM1_CHN1_BW		(MEM_BASE + 0x24)
#define MEM_SMI_VDEC_COMM0_CHN0_BW	(MEM_BASE + 0x28)
#define MEM_SMI_VDEC_COMM0_CHN1_BW	(MEM_BASE + 0x2c)
#define MEM_SMI_VDEC_COMM1_CHN0_BW	(MEM_BASE + 0x30)
#define MEM_SMI_VDEC_COMM1_CHN1_BW	(MEM_BASE + 0x34)
#define MEM_SMI_VENC_COMM0_CHN0_BW	(MEM_BASE + 0x38)
#define MEM_SMI_VENC_COMM0_CHN1_BW	(MEM_BASE + 0x3c)
#define MEM_SMI_VENC_COMM1_CHN0_BW	(MEM_BASE + 0x40)
#define MEM_SMI_VENC_COMM1_CHN1_BW	(MEM_BASE + 0x44)

#define MEM_APMCU_TOTAL_BW		(MEM_BASE + 0x50)
#define MEM_SMI_LOG_FLAG		(MEM_BASE + 0x54)

#define MEM_TEST_NODE_ID		(MEM_BASE + 0x60)
#define MEM_TEST_SRT_R_BW		(MEM_BASE + 0x64)
#define MEM_TEST_SRT_W_BW		(MEM_BASE + 0x68)

#endif

