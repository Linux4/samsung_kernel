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

#ifndef _ISP_DRV_HEADER_
#define _ISP_DRV_HEADER_

#include <linux/semaphore.h>
#include <linux/spinlock_types.h>

#define ISP_QUEUE_LENGTH 16
#define ISP_BING4AWB_NUM 2

struct isp_node {
	uint32_t irq_val0;
	uint32_t irq_val1;
	uint32_t irq_val2;
	uint32_t irq_val3;
	uint32_t reserved;
	struct isp_time time;
};

struct isp_queue {
	struct isp_node node[ISP_QUEUE_LENGTH];
	struct isp_node *write;
	struct isp_node *read;
};

struct isp_b4awb_buf {
	uint32_t buf_id;
	uint32_t buf_flag;
	unsigned long buf_phys_addr;
};

struct isp_k_private {
	atomic_t users;
	struct device_node *dn;
	struct clk *clock;
	struct semaphore device_lock;
	struct semaphore ioctl_lock;
	unsigned long block_buf_addr;
	uint32_t block_buf_len;
	unsigned long reg_buf_addr;
	uint32_t reg_buf_len;
	uint32_t lsc_load_buf_id;
	uint32_t lsc_update_buf_id;
	unsigned long raw_aem_buf_addr;
	uint32_t raw_aem_buf_len;
	uint32_t ct_load_buf_id;
	unsigned long full_gamma_buf_addr;
	uint32_t full_gamma_buf_len;
	uint32_t full_gamma_buf_id;
	unsigned long yuv_ygamma_buf_addr;
	uint32_t yuv_ygamma_buf_len;
	uint32_t yuv_ygamma_buf_id;
	unsigned long raw_awbm_buf_addr;
	uint32_t raw_awbm_buf_len;
	struct isp_b4awb_buf b4awb_buf[ISP_BING4AWB_NUM];
	uint32_t lsc_buf_phys_addr;
	unsigned long yiq_antiflicker_buf_addr;
	uint32_t yiq_antiflicker_len;
	uint32_t yiq_antiflicker_order;
	unsigned long yiq_aem_buf_addr;
	uint32_t yiq_aem_buf_len;
	uint32_t is_vst_ivst_update;
};

struct isp_drv_private {
	spinlock_t isr_lock;
	struct semaphore isr_done_lock;
	struct isp_queue queue;
};

struct isp_k_file {
	struct isp_k_private *isp_private;
	struct isp_drv_private drv_private;
};

int32_t isp_get_int_num(struct isp_node *node);
void isp_clr_int(void);
void isp_en_irq(uint32_t irq_mode);
int32_t isp_axi_bus_waiting(void);
int32_t isp_capability(void *param);
int32_t isp_cfg_param(void  *param, struct isp_k_private *isp_private);

#endif
