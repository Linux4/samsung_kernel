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
 #ifndef _ISP_DRV_KERNEL_H_
 #define _ISP_DRV_KERNEL_H_

 enum isp_clk_sel {
	ISP_CLK_312M = 0,
	ISP_CLK_256M,
	ISP_CLK_128M,
	ISP_CLK_76M8,
	ISP_CLK_48M,
	ISP_CLK_NONE
};

struct isp_irq_param {
	uint32_t isp_irq_val;
	uint32_t dcam_irq_val;
	uint32_t irq_val;
} ;
struct isp_reg_bits {
	uint32_t reg_addr;
	uint32_t reg_value;
} ;

 struct isp_reg_param {
	uint32_t reg_param;
	uint32_t counts;
} ;

enum {
	ISP_LNC_STATUS_OK = (1<<0),
};

enum {
	ISP_INT_HIST_STORE = (1<<0),
	ISP_INT_STORE = (1<<1),
	ISP_INT_LENS_LOAD = (1<<2),
	ISP_INT_HIST_CAL = (1<<3),
	ISP_INT_HIST_RST = (1<<4),
	ISP_INT_FETCH_BUF_FULL = (1<<5),
	ISP_INT_DCAM_FULL = (1<<6),
	ISP_INT_STORE_ERR = (1<<7),
	ISP_INT_SHADOW = (1<<8),
	ISP_INT_PREVIEW_STOP = (1<<9),
	ISP_INT_AWB = (1<<10),
	ISP_INT_AF = (1<<11),
	ISP_INT_SLICE_CNT = (1<<12),
	ISP_INT_AE = (1<<13),
	ISP_INT_ANTI_FLICKER = (1<<14),
	ISP_INT_AWBM_START = (1<<15),
	ISP_INT_AFM_START = (1<<16),
	ISP_INT_AE_START = (1<<17),
	ISP_INT_FETCH_SOF = (1<<18),
	ISP_INT_FETCH_EOF = (1<<19),
	ISP_INT_AFM_WIN8 = (1<<20),
	ISP_INT_AFM_WIN7 = (1<<21),
	ISP_INT_AFM_WIN6 = (1<<22),
	ISP_INT_AFM_WIN5 = (1<<23),
	ISP_INT_AFM_WIN4 = (1<<24),
	ISP_INT_AFM_WIN3 = (1<<25),
	ISP_INT_AFM_WIN2 = (1<<26),
	ISP_INT_AFM_WIN1 = (1<<27),
	ISP_INT_AFM_WIN0 = (1<<28),
	ISP_INT_STOP=(1<<31),
};

#define ISP_IO_MAGIC	'R'
#define ISP_IO_IRQ	_IOR(ISP_IO_MAGIC, 0, struct isp_irq_param)
#define ISP_IO_READ	_IOR(ISP_IO_MAGIC, 1, struct isp_reg_param)
#define ISP_IO_WRITE	_IOW(ISP_IO_MAGIC, 2, struct isp_reg_param)
#define ISP_IO_RST	_IOW(ISP_IO_MAGIC, 3, uint32_t)
#define ISP_IO_SETCLK	_IOW(ISP_IO_MAGIC, 4, enum isp_clk_sel)
#define ISP_IO_STOP	_IOW(ISP_IO_MAGIC, 5, uint32_t)
#define ISP_IO_INT	_IOW(ISP_IO_MAGIC, 6, uint32_t)
#define ISP_IO_DCAM_INT	_IOW(ISP_IO_MAGIC, 7, uint32_t)
#define ISP_IO_LNC_PARAM	_IOW(ISP_IO_MAGIC, 8, uint32_t)
#define ISP_IO_LNC	_IOW(ISP_IO_MAGIC, 9, uint32_t)
#define ISP_IO_ALLOC	_IOW(ISP_IO_MAGIC, 10, uint32_t)

#endif
