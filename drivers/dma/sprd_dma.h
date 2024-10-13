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

#ifndef __SPRD_DMA_H
#define __SPRD_DMA_H
#include <linux/dmaengine.h>

#define DMA_UID_SOFTWARE		0

#define CHN_PRIORITY_OFFSET		12
#define CHN_PRIORITY_MASK		0x3
#define LLIST_EN_OFFSET			4

#define SRC_DATAWIDTH_OFFSET	30
#define DES_DATAWIDTH_OFFSET	28
#define SWT_MODE_OFFSET			26
#define REQ_MODE_OFFSET			24
#define ADDR_WRAP_SEL_OFFSET	23
#define ADDR_WRAP_EN_OFFSET		22
#define ADDR_FIX_SEL_OFFSET		21
#define ADDR_FIX_SEL_EN			20
#define LLIST_END_OFFSET		19
#define BLK_LEN_REC_H_OFFSET	17
#define FRG_LEN_OFFSET			0

#define DEST_TRSF_STEP_OFFSET	16
#define SRC_TRSF_STEP_OFFSET	0
#define DEST_FRAG_STEP_OFFSET	16
#define SRC_FRAG_STEP_OFFSET	0

#define TRSF_STEP_MASK			0xffff
#define FRAG_STEP_MASK			0xffff

#define DATAWIDTH_MASK			0x3
#define SWT_MODE_MASK			0x3
#define REQ_MODE_MASK			0x3
#define ADDR_WRAP_SEL_MASK		0x1
#define ADDR_WRAP_EN_MASK		0x1
#define ADDR_FIX_SEL_MASK		0x1
#define ADDR_FIX_SEL_MASK		0x1
#define LLIST_END_MASK			0x1
#define BLK_LEN_REC_H_MASKT		0x3
#define FRG_LEN_MASK			0x1ffff

#define BLK_LEN_MASK			0x1ffff
#define TRSC_LEN_MASK			0xfffffff

typedef enum {
	BYTE_WIDTH = 0x01,
	SHORT_WIDTH,
	WORD_WIDTH,
} dma_datawidth;

typedef enum {
	FRAG_REQ_MODE = 0x0,
	BLOCK_REQ_MODE,
	TRANS_REQ_MODE,
	LIST_REQ_MODE,
} dma_request_mode;

typedef enum {
	NO_INT = 0x00,
	FRAG_DONE,
	BLK_DONE,
	TRANS_DONE,
	LIST_DONE,
	CONFIG_ERR,
} dma_int_type;

typedef enum {
	DMA_PRI_0 = 0,
	DMA_PRI_1,
	DMA_PRI_2,
	DMA_PRI_3,
} dma_pri_level;

typedef enum {
	SET_IRQ_TYPE = 0x0,
	SET_WRAP_MODE,
	SET_REQ_MODE,
	SET_CHN_PRIO,
	SET_INT_TYPE,
} dma_cmd;

struct sprd_dma_glb_reg {
	u32 pause;
	u32 frag_wait;
	u32 pend0_en;
	u32 pend1_en;
	u32 int_raw_sts;
	u32 int_msk_sts;
	u32 req_sts;
	u32 en_sts;
	u32 debug_sts;
	u32 arb_sel_sts;
};

struct sprd_dma_cfg {
	unsigned long link_cfg_v;
	unsigned long link_cfg_p;
	u32 dev_id;
	dma_datawidth datawidth;
	u32 src_addr;
	u32 des_addr;
	u32 fragmens_len;
	u32 block_len;
	u32 src_step;
	u32 des_step;
	dma_request_mode req_mode;
	dma_int_type irq_mode;
	/*only full chn need following config*/
	u32 transcation_len;
	u32 src_frag_step;
	u32 dst_frag_step;
	u32 wrap_ptr;
	u32 wrap_to;
	u32 src_blk_step;
	u32 dst_blk_step;
	u32 linklist_ptr;
	u32 is_end;
};

enum dma_chn_type {
	STANDARD_DMA,
	FULL_DMA,
};

enum dma_filter_param {
	AP_STANDARD_DMA = 0x11,
	AP_FULL_DMA = 0x22,
	AON_STANDARD_DMA = 0x33,
	AON_FULL_DMA = 0x44,
	NUM_REQUEST_DMA = 0x100,
};

enum dma_chn_status {
	NO_USED,
	USED,
};

enum dma_link_list {
	LINKLIST,
	NO_LINKLIST,
};

enum request_mode {
	SOFTWARE_REQ,
	HARDWARE_REQ,	
};

enum dma_flags {
	DMA_CFG_FLAG = BIT(0),
	DMA_HARDWARE_FLAG = BIT(1),
	DMA_SOFTWARE_FLAG = BIT(2),
};

enum config_type {
	CONFIG_DESC,
	CONFIG_LINKLIST,	
};

enum addr_type {
    SPRD_SRC_ADDR = 0x55,
    SPRD_DST_ADDR = 0xAA,
};

#if defined(CONFIG_ARCH_SC8825) || defined(CONFIG_ARCH_SC7710)
#define DMA_VER_R1P0

#define DMA_UID_SOFTWARE                0
#define DMA_UART0_TX                    1
#define DMA_UART0_RX                    2
#define DMA_UART1_TX                    3
#define DMA_UART1_RX                    4
#define DMA_UART2_TX                    5
#define DMA_UART2_RX                    6
#define DMA_IIS_TX                      7
#define DMA_IIS_RX                      8
#define DMA_EPT_RX                      9
#define DMA_EPT_TX                      10
#define DMA_VB_DA0                      11
#define DMA_VB_DA1                      12
#define DMA_VB_AD0                      13
#define DMA_VB_AD1                      14
#define DMA_SIM0_TX                     15
#define DMA_SIM0_RX                     16
#define DMA_SIM1_TX                     17
#define DMA_SIM1_RX                     18
#define DMA_SPI0_TX                     19
#define DMA_SPI0_RX                     20
#define DMA_ROT                         21
#define DMA_SPI1_TX                     22
#define DMA_SPI1_RX                     23
#define DMA_IIS1_TX                     24
#define DMA_IIS1_RX                     25
#define DMA_NFC_TX                      26
#define DMA_NFC_RX                      27
#define DMA_DRM_RAW                     29
#define DMA_DRM_CPT                     30

#define DMA_UID_MIN                     0
#define DMA_UID_MAX                     32

#endif


#ifdef CONFIG_ARCH_SCX35
#define DMA_VER_R4P0
/*DMA Request ID def*/
#define DMA_SIM_RX		1
#define DMA_SIM_TX		2
#define DMA_IIS0_RX		3
#define DMA_IIS0_TX		4
#define DMA_IIS1_RX		5
#define DMA_IIS1_TX		6
#define DMA_IIS2_RX             7
#define DMA_IIS2_TX             8
#define DMA_IIS3_RX             9
#define DMA_IIS3_TX             10
#define DMA_SPI0_RX             11
#define DMA_SPI0_TX             12
#define DMA_SPI1_RX             13
#define DMA_SPI1_TX             14
#define DMA_SPI2_RX		15
#define DMA_SPI2_TX             16
#define DMA_UART0_RX		17
#define DMA_UART0_TX		18
#define DMA_UART1_RX		19
#define DMA_UART1_TX		20
#define DMA_UART2_RX		21
#define DMA_UART2_TX		22
#define DMA_UART3_RX		23
#define DMA_UART3_TX		24
#define DMA_UART4_RX		25
#define DMA_UART4_TX		26
#define DMA_DRM_CPT		27
#define DMA_DRM_RAW		28
#ifndef CONFIG_SND_SOC_SPRD_AUDIO_USE_AON_DMA
#define DMA_VB_DA0		29
#define DMA_VB_DA1		30
#else
#define DMA_VB_DA0		7
#define DMA_VB_DA1		8
#endif
#define DMA_VB_AD0		31
#define DMA_VB_AD1		32
#define DMA_VB_AD2		33
#define DMA_VB_AD3		34
#define DMA_GPS			35

#ifdef CONFIG_ARCH_SCX30G
#define DMA_SDIO0_RD		36
#define DMA_SDIO0_WR		37
#define DMA_SDIO1_RD		38
#define DMA_SDIO1_WR		39
#define DMA_SDIO2_RD		40
#define DMA_SDIO2_WR		41
#define DMA_EMMC_RD		42
#define DMA_EMMC_WR		43
#endif

#define DMA_IIS_RX	DMA_IIS0_RX
#define DMA_IIS_TX	DMA_IIS0_TX

#endif

int sprd_dma_check_register(struct dma_chan *c);
bool sprd_dma_filter_fn(struct dma_chan *chan, void *filter_param);
#endif
