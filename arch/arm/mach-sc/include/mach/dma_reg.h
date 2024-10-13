/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ASM_ARCH_SPRD_DMA_REG_H
#define __ASM_ARCH_SPRD_DMA_REG_H

#include <linux/interrupt.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/dma.h>

/**----------------------------------------------------------------------------*
**                               Micro Define                                 **
**----------------------------------------------------------------------------*/

/*dma r1p0 register offset define start*/
#ifdef DMA_VER_R1P0
#define DMA_REG_BASE                    SPRD_DMA0_BASE

/* 0X00 */
#define DMA_BLK_WAIT                    (DMA_REG_BASE + 0x0000)
#define DMA_CHN_EN_STATUS               (DMA_REG_BASE + 0x0004)
#define DMA_LINKLIST_EN                 (DMA_REG_BASE + 0x0008)
#define DMA_SOFTLINK_EN                 (DMA_REG_BASE + 0x000C)

/* 0X10 */
#define DMA_SOFTLIST_SIZE               (DMA_REG_BASE + 0x0010)
#define DMA_SOFTLIST_CMD                (DMA_REG_BASE + 0x0014)
#define DMA_SOFTLIST_STS                (DMA_REG_BASE + 0x0018)
#define DMA_SOFTLIST_BASEADDR           (DMA_REG_BASE + 0x001C)

/* 0X20 */
#define DMA_PRI_REG0                    (DMA_REG_BASE + 0x0020)
#define DMA_PRI_REG1                    (DMA_REG_BASE + 0x0024)

/* 0X30 */
#define DMA_INT_STS                     (DMA_REG_BASE + 0x0030)
#define DMA_INT_RAW                     (DMA_REG_BASE + 0x0034)

/* 0X40 */
#define DMA_LISTDONE_INT_EN             (DMA_REG_BASE + 0x0040)
#define DMA_BLOCK_INT_EN                (DMA_REG_BASE + 0x0044)
#define DMA_TRANSF_INT_EN               (DMA_REG_BASE + 0x0048)

/* 0X50 */
#define DMA_LISTDONE_INT_STS            (DMA_REG_BASE + 0x0050)
#define DMA_BURST_INT_STS               (DMA_REG_BASE + 0x0054)
#define DMA_TRANSF_INT_STS              (DMA_REG_BASE + 0x0058)

/* 0X60 */
#define DMA_LISTDONE_INT_RAW            (DMA_REG_BASE + 0x0060)
#define DMA_BURST_INT_RAW               (DMA_REG_BASE + 0x0064)
#define DMA_TRANSF_INT_RAW              (DMA_REG_BASE + 0x0068)

/* 0X70 */
#define DMA_LISTDONE_INT_CLR            (DMA_REG_BASE + 0x0070)
#define DMA_BURST_INT_CLR               (DMA_REG_BASE + 0x0074)
#define DMA_TRANSF_INT_CLR              (DMA_REG_BASE + 0x0078)

/* 0X80 */
#define DMA_SOFT_REQ                    (DMA_REG_BASE + 0x0080)
#define DMA_TRANS_STS                   (DMA_REG_BASE + 0x0084)
#define DMA_REQ_PEND                    (DMA_REG_BASE + 0x0088)

/* 0X90 */
#define DMA_WRAP_START                  (DMA_REG_BASE + 0x0090)
#define DMA_WRAP_END                    (DMA_REG_BASE + 0x0094)

#define DMA_CHN_UID_BASE                (DMA_REG_BASE + 0x0098)
#define DMA_CHN_UID0                    (DMA_REG_BASE + 0x0098)
#define DMA_CHN_UID1                    (DMA_REG_BASE + 0x009C)
#define DMA_CHN_UID2                    (DMA_REG_BASE + 0x00A0)
#define DMA_CHN_UID3                    (DMA_REG_BASE + 0x00A4)
#define DMA_CHN_UID4                    (DMA_REG_BASE + 0x00A8)
#define DMA_CHN_UID5                    (DMA_REG_BASE + 0x00AC)
#define DMA_CHN_UID6                    (DMA_REG_BASE + 0x00B0)
#define DMA_CHN_UID7                    (DMA_REG_BASE + 0x00B4)

#define DMA_CHx_EN                      (DMA_REG_BASE + 0x00C0)
#define DMA_CHx_DIS                     (DMA_REG_BASE + 0x00C4)

/* Chanel x dma contral regisers address offset*/
#define DMA_CH_CFG                      (0x0000)
#define DMA_CH_TOTAL_LEN                (0x0004)
#define DMA_CH_SRC_ADDR                 (0x0008)
#define DMA_CH_DEST_ADDR                (0x000c)
#define DMA_CH_LLPTR                    (0x0010)
#define DMA_CH_SDEP                     (0x0014)
#define DMA_CH_SBP                      (0x0018)
#define DMA_CH_DBP                      (0x001c)

/* Channel x dma contral regisers address */
#define DMA_CHx_CTL_BASE                (DMA_REG_BASE + 0x0400)
#define DMA_CHx_OFFSET			(0x20)
#define DMA_CHx_BASE(x)                 (DMA_CHx_CTL_BASE + DMA_CHx_OFFSET * x )
#define DMA_CHx_CFG(x)                  (DMA_CHx_BASE(x) + DMA_CH_CFG)
#define DMA_CHx_TOTAL_LEN(x)		(DMA_CHx_BASE(x) + DMA_CH_TOTAL_LEN)
#define DMA_CHx_SRC_ADDR(x)             (DMA_CHx_BASE(x) + DMA_CH_SRC_ADDR)
#define DMA_CHx_DEST_ADDR(x)            (DMA_CHx_BASE(x) + DMA_CH_DEST_ADDR)
#define DMA_CHx_LLPTR(x)                (DMA_CHx_BASE(x) + DMA_CH_LLPTR)
#define DMA_CHx_SDEP(x)                 (DMA_CHx_BASE(x) + DMA_CH_SDEP)
#define DMA_CHx_SBP(x)                  (DMA_CHx_BASE(x) + DMA_CH_SBP)
#define DMA_CHx_DBP(x)                  (DMA_CHx_BASE(x) + DMA_CH_DBP)

/*there is no different between full and std chn is DMA r1p0*/
struct sci_dma_reg {
	u32 cfg;
	u32 total_len;
	u32 src_addr;
	u32 des_addr;
	u32 llist_ptr;
	u32 elem_postm;
	u32 src_blk_postm;
	u32 des_blk_postm;
};

#endif
/*dma r1p0 register offset define end*/

/*dma r3p0 and r4p0 register offset define start*/
#ifdef DMA_VER_R4P0

#include <mach/sci_glb_regs.h>
#if 0
#define DMA_REG_BASE	SPRD_DMA0_BASE

#define DMA_PAUSE	(DMA_REG_BASE + 0x0000)
#define DMA_FRAG_WAIT	(DMA_REG_BASE + 0x0004)
#define DMA_PEND0_EN	(DMA_REG_BASE + 0x0008)
#define DMA_PEND1_EN	(DMA_REG_BASE + 0x000C)
#define DMA_INT_RAW_STS	(DMA_REG_BASE + 0x0010)
#define DMA_INT_MSK_STS	(DMA_REG_BASE + 0x0014)
#define DMA_REQ_STS	(DMA_REG_BASE + 0x0018)
#define DMA_EN_STS	(DMA_REG_BASE + 0x001C)
#define DMA_DEGUG_STS	(DMA_REG_BASE + 0x0020)
#define DMA_ARB_SEL_STS	(DMA_REG_BASE + 0x0024)

#define DMA_CHx_OFFSET	(0x40)
#define DMA_CHx_BASE(x)	(DMA_REG_BASE + 0x1000 + DMA_CHx_OFFSET * (x - 1))

#define DMA_CHN_PAUSE(x)	(DMA_CHx_BASE(x) + 0x0000)
#define DMA_CHN_REQ(x)		(DMA_CHx_BASE(x) + 0x0004)
#define DMA_CHN_CFG(x)		(DMA_CHx_BASE(x) + 0x0008)
#define DMA_CHN_INT(x)		(DMA_CHx_BASE(x) + 0x000C)
#define DMA_CHN_SRC_ADR(x)	(DMA_CHx_BASE(x) + 0x0010)
#define DMA_CHN_DES_ADR(x)	(DMA_CHx_BASE(x) + 0x0014)
#define DMA_CHN_FRAG_LEN(x)	(DMA_CHx_BASE(x) + 0x0018)
#define DMA_CHN_BLK_LEN(x)	(DMA_CHx_BASE(x) + 0x001C)

#define DMA_CHN_TRSC_LEN(x)	(DMA_CHx_BASE(x) + 0x0020)
#define DMA_CHN_TRSF_STEP(x)	(DMA_CHx_BASE(x) + 0x0024)
#define DMA_CHN_WRAP_PTR(x)	(DMA_CHx_BASE(x) + 0x0028)
#define DMA_CHN_WRAP_TO(x)	(DMA_CHx_BASE(x) + 0x002C)
#define DMA_CHN_LLIST_PRT(x)	(DMA_CHx_BASE(x) + 0x0030)
#define DMA_CHN_FRAP_STEP(x)	(DMA_CHx_BASE(x) + 0x0034)
#define DMA_CHN_SRC_BLK_STEP(x)	(DMA_CHx_BASE(x) + 0x0038)
#define DMA_CHN_DES_BLK_STEP(x)	(DMA_CHx_BASE(x) + 0x003C)
#define DMA_REQ_CID(uid)	(DMA_REG_BASE + 0x2000 + 0x4 * ((uid) -1))
#endif
/*FRAG_LEN*/
#define CHN_PRIORITY_OFFSET	12
#define CHN_PRIORITY_MASK	0x3
#define LLIST_EN_OFFSET	4;

#define SRC_DATAWIDTH_OFFSET	30
#define DES_DATAWIDTH_OFFSET	28
#define SWT_MODE_OFFSET	26
#define REQ_MODE_OFFSET	24
#define ADDR_WRAP_SEL_OFFSET	23
#define ADDR_WRAP_EN_OFFSET	22
#define ADDR_FIX_SEL_OFFSET	21
#define ADDR_FIX_SEL_EN	20
#define LLIST_END_OFFSET	19
#define BLK_LEN_REC_H_OFFSET 17
#define FRG_LEN_OFFSET	0

#define DEST_TRSF_STEP_OFFSET 16
#define SRC_TRSF_STEP_OFFSET 0
#define DEST_FRAG_STEP_OFFSET 16
#define SRC_FRAG_STEP_OFFSET 0

#define TRSF_STEP_MASK	0xffff
#define FRAG_STEP_MASK	0xffff

#define DATAWIDTH_MASK	0x3
#define SWT_MODE_MASK	0x3
#define REQ_MODE_MASK	0x3
#define ADDR_WRAP_SEL_MASK 0x1
#define ADDR_WRAP_EN_MASK 0x1
#define ADDR_FIX_SEL_MASK	0x1
#define ADDR_FIX_SEL_MASK	0x1
#define LLIST_END_MASK	0x1
#define BLK_LEN_REC_H_MASKT 0x3
#define FRG_LEN_MASK	0x1ffff

#define BLK_LEN_MASK	0x1ffff
#define TRSC_LEN_MASK	0xfffffff

struct sci_dma_reg {
	u32 pause;
	u32 req;
	u32 cfg;
	u32 intc;
	u32 src_addr;
	u32 des_addr;
	u32 frg_len;
	u32 blk_len;
	/* only full chn have following regs */
	union {
		struct {
			u32 trsc_len;
			u32 trsf_step;
			u32 wrap_ptr;
			u32 wrap_to;
			u32 llist_ptr;
			u32 frg_step;
			u32 src_blk_step;
			u32 des_blk_step;
		};
		int dummy[0];
	};
};
#endif
#endif
