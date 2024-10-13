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

#ifndef __ASM_ARCH_SPRD_DMA_H
#define __ASM_ARCH_SPRD_DMA_H

#include <linux/interrupt.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/globalregs.h>

/**----------------------------------------------------------------------------*
**                               Micro Define                                 **
**----------------------------------------------------------------------------*/
#define DMA_REG_BASE                    SPRD_DMA0_BASE

/* 0X00 */
#define DMA_CFG                         (DMA_REG_BASE + 0x0000)
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
#define DMA_BURST_INT_EN                (DMA_REG_BASE + 0x0044)
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
#define DMA_CH_CFG0                     (0x0000)
#define DMA_CH_CFG1                     (0x0004)
#define DMA_CH_SRC_ADDR                 (0x0008)
#define DMA_CH_DEST_ADDR                (0x000c)
#define DMA_CH_LLPTR                    (0x0010)
#define DMA_CH_SDEP                     (0x0014)
#define DMA_CH_SBP                      (0x0018)
#define DMA_CH_DBP                      (0x001c)

/* Channel x dma contral regisers address */
#define DMA_CHx_CTL_BASE                (DMA_REG_BASE + 0x0400)
#define DMA_CHx_BASE(x)                 (DMA_CHx_CTL_BASE + 0x20 * x )
#define DMA_CHx_CFG0(x)                 (DMA_CHx_CTL_BASE + 0x20 * x + DMA_CH_CFG0)
#define DMA_CHx_CFG1(x)                 (DMA_CHx_CTL_BASE + 0x20 * x + DMA_CH_CFG1)
#define DMA_CHx_SRC_ADDR(x)             (DMA_CHx_CTL_BASE + 0x20 * x + DMA_CH_SRC_ADDR)
#define DMA_CHx_DEST_ADDR(x)            (DMA_CHx_CTL_BASE + 0x20 * x + DMA_CH_DEST_ADDR)
#define DMA_CHx_LLPTR(x)                (DMA_CHx_CTL_BASE + 0x20 * x + DMA_CH_LLPTR)
#define DMA_CHx_SDEP(x)                 (DMA_CHx_CTL_BASE + 0x20 * x + DMA_CH_SDEP)
#define DMA_CHx_SBP(x)                  (DMA_CHx_CTL_BASE + 0x20 * x + DMA_CH_SBP)
#define DMA_CHx_DBP(x)                  (DMA_CHx_CTL_BASE + 0x20 * x + DMA_CH_DBP)

/* DMA user id */

#define DMA_UID_MASK                    0x1f
#define DMA_UID_SHIFT_STP               8
#define DMA_UID_UNIT                    4

/* DMA Priority */
#define DMA_PRI_0                       0
#define DMA_PRI_1                       1
#define DMA_PRI_2                       2
#define DMA_PRI_3                       3
#define DMA_MAX_PRI                     DMA_PRI_3
#define DMA_MIN_PRI                     DMA_PRI_0

/* DMA_CFG */
#define DMA_PAUSE_REQ                  (1 << 8)

/* CH_CFG0 */
#define DMA_LLEND                      (1 << 31)
#define DMA_BIG_ENDIAN                 (1 << 28)
#define DMA_LIT_ENDIAN                 (0 << 28) /* this bit not used by SC8800G2 */
#define DMA_SDATA_WIDTH8               (0 << 26)
#define DMA_SDATA_WIDTH16              (1 << 26)
#define DMA_SDATA_WIDTH32              (2 << 26)
#define DMA_SDATA_WIDTH_MASK           (0x03 << 26)
#define DMA_DDATA_WIDTH8               (0 << 24)
#define DMA_DDATA_WIDTH16              (1 << 24)
#define DMA_DDATA_WIDTH32              (2 << 24)
#define DMA_DDATA_WIDTH_MASK           (0x03 << 24)
#define DMA_REQMODE_NORMAL             (0 << 22)
#define DMA_REQMODE_TRANS              (1 << 22)
#define DMA_REQMODE_LIST               (2 << 22)
#define DMA_REQMODE_INFIINITE          (3 << 22)
#define DMA_SRC_WRAP_EN                (1 << 21)
#define DMA_DST_WRAP_EN                (1 << 20)
#define DMA_NO_AUTO_CLS                (1 << 17)

/* DMA_CH_SBP */
#define SRC_BURST_MODE_SINGLE          (0 << 28)
#define SRC_BURST_MODE_ANY             (1 << 28)
#define SRC_BURST_MODE_4               (3 << 28)
#define SRC_BURST_MODE_8               (5 << 28)
#define SRC_BURST_MODE_15              (7 << 28)
#define DMA_BURST_STEP_DIR_BIT         (1 << 25)
#define DMA_BURST_STEP_ABS_SIZE_MASK   (0x1FFFFFF)

#define DMA_UN_SWT_MODE                (0<<28)
#define DMA_FULL_SWT_MODE              (1<<28)
#define DMA_SWT_MODE0                  (2<<28)
#define DMA_SWT_MODE1                  (3<<28)
#define BURST_MODE_SINGLE              (0 << 28)
#define BURST_MODE_ANY                 (1 << 28)
#define BURST_MODE_4                   (3 << 28)
#define BURST_MODE_8                   (5 << 28)
#define BURST_MODE_16                  (7 << 28)

#define DMA_SOFT_WAITTIME              0x0f
#define DMA_HARD_WAITTIME              0x0f

#define DMA_INCREASE                   (0)
#define DMA_DECREASE                   (1)
#define DMA_NOCHANGE                   (0xff)

#define ON                              1
#define OFF                             0

#define DMA_CHN_MIN                     0
#define DMA_CHN_MAX                     31
#define DMA_CHN_NUM                     32

#define DMA_CFG_BLOCK_LEN_MAX           (0xffff)
#define SRC_ELEM_POSTM_SHIFT            16
#define CFG_BLK_LEN_MASK                0xffff
#define SRC_ELEM_POSTM_MASK             0xffff
#define DST_ELEM_POSTM_MASK             0xffff
#define SRC_BLK_POSTM_MASK              0x3ffffff
#define DST_BLK_POSTM_MASK              0x3ffffff
#define SOFTLIST_REQ_PTR_MASK           0xffff
#define SOFTLIST_REQ_PTR_SHIFT          16
#define SOFTLIST_CNT_MASK               0xffff

static inline void reg_writel(u32 reg, u8 shift, u32 val, u32 mask)
{
	u32 tmp;
	tmp = __raw_readl(reg);
	tmp &= ~(mask<<shift);
	tmp |= val << shift;
	__raw_writel(tmp, reg);
}

static inline void reg_bits_or(u32 bits, u32 reg)
{
	writel((__raw_readl(reg) | bits), reg);
}

static inline void reg_bits_and(u32 bits, u32 reg)
{
	writel((__raw_readl(reg) & bits), reg);
}

/* will be removed */
#if 1
static inline void dma_reg_bits_or(u32 bits, u32 reg)
{
	writel((__raw_readl(reg) | bits), reg);
}
static inline u32 dma_get_reg(u32 reg)
{
	return __raw_readl(reg);
}

extern void sprd_dma_channel_disable(int);
#define sprd_dma_start(ch_id) \
    dma_reg_bits_or(1 << ch_id, DMA_CHx_EN) /* Enable DMA channel */

#define sprd_dma_stop(ch_id) \
	sprd_dma_channel_disable(ch_id)

#define sprd_dma_cfg(ch_id, cfg) \
    __raw_writel(cfg,  DMA_CHx_CTL_BASE + (ch_id * 0x20) + 0x00) /* cfg */

#define sprd_dma_tlen(ch_id, tlen) \
    __raw_writel(tlen, DMA_CHx_CTL_BASE + (ch_id * 0x20) + 0x04) /* tlen */

#define sprd_dma_dsrc(ch_id, dsrc) \
    __raw_writel(dsrc, DMA_CHx_CTL_BASE + (ch_id * 0x20) + 0x08) /* dsrc */

#define sprd_dma_ddst(ch_id, ddst) \
    __raw_writel(ddst, DMA_CHx_CTL_BASE + (ch_id * 0x20) + 0x0C) /* ddst */

#define sprd_dma_llptr(ch_id, llptr) \
    __raw_writel(llptr,DMA_CHx_CTL_BASE + (ch_id * 0x20) + 0x10) /* llptr */

#define sprd_dma_pmod(ch_id, pmod) \
    __raw_writel(pmod, DMA_CHx_CTL_BASE + (ch_id * 0x20) + 0x14) /* pmod */

#define sprd_dma_sbm(ch_id, sbm) \
    __raw_writel(sbm,  DMA_CHx_CTL_BASE + (ch_id * 0x20) + 0x18) /* sbm */

#define sprd_dma_dbm(ch_id, dbm) \
    __raw_writel(dbm,  DMA_CHx_CTL_BASE + (ch_id * 0x20) + 0x1C) /* dbm */
#endif

struct sprd_irq_handler {
	irq_handler_t handler;
	void *dev_id;
	u32 dma_uid;
	u32 used;/* mark the channel used before new API done */
};

/* DMA_LISTDONE_INT_EN, DMA_BURST_INT_EN, DMA_TRANSF_INT_EN */
typedef enum{
	LINKLIST_DONE,
	BLOCK_DONE,
	TRANSACTION_DONE,
}dma_done_type;


enum {
	INT_NONE = 0x00,
	LLIST_DONE_EN = 0x01,
	BURST_DONE_EN = 0x02,
	TRANS_DONE_EN = 0x04,
};

typedef enum {
	/* For dma mode */
	DMA_WRAP        = (1 << 0),
	DMA_NORMAL      = (1 << 1),
	DMA_LINKLIST    = (1 << 2),
	DMA_SOFTLIST    = (1 << 3),
}dma_work_mode;

struct sprd_dma_channel_desc {
	u32 cfg_swt_mode_sel;
	u32 cfg_src_data_width;
	u32 cfg_dst_data_width;
	u32 cfg_req_mode_sel;/* request mode */
	u32 cfg_src_wrap_en;
	u32 cfg_dst_wrap_en;
	u32 cfg_blk_len;

	u32 total_len;
	u32 src_addr;
	u32 dst_addr;
	u32 llist_ptr;/*linklist mode only, must be 8 words boundary */
	u32 src_elem_postm;
	u32 dst_elem_postm;
	u32 src_burst_mode;
	u32 src_blk_postm;
	u32 dst_burst_mode;
	u32 dst_blk_postm;
};

struct sprd_dma_linklist_desc {
	u32 cfg;
	u32 total_len;
	u32 src_addr;
	u32 dst_addr;
	u32 llist_ptr;
	u32 elem_postm;
	u32 src_blk_postm;
	u32 dst_blk_postm;
};

struct sprd_dma_wrap_addr {
	u32 wrap_start_addr;
	u32 wrap_end_addr;
};

/* those spreadtrum DMA interface must be implemented */
int  sprd_dma_request(u32 uid, irq_handler_t irq_handle, void *data);
void sprd_dma_free(u32 uid);
void sprd_dma_channel_config(u32 chn, dma_work_mode work_mode, const struct sprd_dma_channel_desc *dma_cfg);
void sprd_dma_default_channel_setting(struct sprd_dma_channel_desc *dma_cfg);
void sprd_dma_default_linklist_setting(struct sprd_dma_linklist_desc *chn_cfg);
void sprd_dma_linklist_config(u32 chn_id, u32 dma_cfg);
void sprd_dma_wrap_addr_config(const struct sprd_dma_wrap_addr *wrap_addr);
void sprd_dma_set_irq_type(u32 chn, dma_done_type irq_type, u32 on_off);
void sprd_dma_set_chn_pri(u32 chn, u32 pri);
void sprd_dma_channel_start(u32 chn);
void sprd_dma_channel_stop(u32 chn);
/* ONLY FOR DEBUG */
void sprd_dma_check_channel(void);
void sprd_dma_dump_regs(void);

#endif
