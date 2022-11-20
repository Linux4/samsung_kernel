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


#ifdef CONFIG_ARCH_SCX35
#define DMA_CHN_MIN                     1
#define DMA_CHN_MAX			32
#define DMA_CHN_NUM			32

#define DMA_UID_SOFTWARE                0


#define FULL_CHN_START 25
#define FULL_CHN_END DMA_CHN_MAX

#define DMA_LINKLIST_CFG_NODE_SIZE 64

#endif


typedef enum {
	NO_INT = 0x00,
	FRAG_DONE,
	BLK_DONE,
	LIST_DONE,
	TRANS_DONE,
	CONFIG_ERR,
} dma_int_type;

typedef enum {
	BYTE_WIDTH = 0X01,
	SHORT_WIDTH,
	WORD_WIDTH,
} dma_datawidth;

typedef enum {
	DMA_PRI_0 = 0,
	DMA_PRI_1,
	DMA_PRI_2,
	DMA_PRI_3,
} dma_pri_level;

typedef enum {
	STD_DMA_CHN,
	FULL_DMA_CHN,
} dma_chn_type;

struct reg_cfg_addr {
	u32 virt_addr;
	u32 phys_addr;
};

typedef enum {
	FRAG_REQ_MODE = 0x0,
	BLOCK_REQ_MODE,
	TRANS_REQ_MODE,
	LIST_REQ_MODE,
} dma_request_mode;

struct sci_dma_cfg {
	dma_datawidth datawidth;
	u32 src_addr;
	u32 des_addr;
	u32 fragmens_len;
	u32 block_len;
	u32 src_step;
	u32 des_step;
	dma_request_mode req_mode;
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

typedef enum {
	SET_IRQ_TYPE = 0x0,
	SET_WRAP_MODE,
	SET_REQ_MODE,
	SET_CHN_PRIO,
	SET_INT_TYPE,
} dma_cmd;


int sci_dma_request(const char *dev_name, dma_chn_type chn_type);
int sci_dma_free(u32 dma_chn);
int sci_dma_config(u32 dma_chn, struct sci_dma_cfg *cfg_list,
		    u32 node_size, struct reg_cfg_addr *cfg_addr);

int sci_dma_register_irqhandle(u32 dma_chn, dma_int_type int_type,
			       void (*irq_handle) (int, void *), void *data);
int sci_dma_start(u32 dma_chn, u32 dev_id);
int sci_dma_stop(u32 dma_chn, u32 dev_id);
int sci_dma_ioctl(u32 dma_chn, dma_cmd cmd, void *arg);
u32 sci_dma_get_src_addr(u32 dma_chn);
u32 sci_dma_get_dst_addr(u32 dma_chn);
int sci_dma_dump_reg(u32 dma_chn, u32 *reg_base);
int sci_dma_memcpy(u32 dest, u32 src, size_t size);
#endif
