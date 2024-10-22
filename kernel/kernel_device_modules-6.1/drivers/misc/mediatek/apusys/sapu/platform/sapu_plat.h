/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 */

#ifndef __SAPU_PLAT_H_
#define __SAPU_PLAT_H_
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/of_device.h>
#include <linux/types.h>
#include <linux/dma-direct.h>
#include <linux/dma-heap.h>
#include <linux/dma-buf.h>
#include <linux/miscdevice.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/rpmsg.h>
#include <linux/completion.h>
#include <linux/refcount.h>
#include "linux/of.h"
#include "asm-generic/errno-base.h"

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/compat.h>
#endif

#include <uapi/linux/dma-buf.h>
#include <uapi/linux/dma-heap.h>
#include <uapi/asm-generic/errno.h>
#include <gz-trusty/smcall.h>
#include <kree/system.h>
#include <kree/mem.h>
#include <mtk_heap.h>

#include "sapu_driver.h"

extern struct sapu_platdata sapu_platdata;

#define MTEE_SESSION_NAME_LEN 64

struct sapu_dram_fb {
	struct dma_buf_attachment *dram_fb_attach;
	struct dma_buf *dram_fb_dmabuf;
	u64 dram_dma_addr;
};

struct sapu_private {
	struct platform_device *pdev;
	struct miscdevice mdev;
	struct sapu_platdata *platdata;
	struct kref lock_ref_cnt;
	struct sapu_dram_fb dram_fb_info;
	struct mutex dmabuf_lock;
	uint32_t ref_count;
	bool dram_register;
};

struct sapu_datamem_info {
	int fd;
	char haSrvName[MTEE_SESSION_NAME_LEN];
	int command;
	uint64_t model_hd_ha;
};

struct sapu_power_ctrl {
	uint32_t lock;
};

struct sapu_lock_rpmsg_device {
	struct rpmsg_endpoint *ept;
	struct rpmsg_device *rpdev;
	struct completion ack;
};

struct sapu_ops {
	void (*detect)(void);
	int (*power_ctrl)(struct sapu_private *sapu, u32 power_on);
};

struct sapu_platdata {
	struct sapu_ops ops;
};

/* Callback - Common */
void plat_detect(void);

/* Callback - Power control */
int power_ctrl_v1(struct sapu_private *sapu, u32 power_on);
int power_ctrl_v2(struct sapu_private *sapu, u32 power_on);

/* Runtime set platdata */
int set_sapu_platdata_by_dts(void);

#endif // __SAPU_PLAT_H_
