/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Yu-Syuan Cai <yusyuan.cai@mediatek.com>
 */

#ifndef _MTK_USB_OFFLOAD_OPS_H_
#define _MTK_USB_OFFLOAD_OPS_H_

#include <sound/soc.h>
#include <linux/notifier.h>
#include "mtk-base-afe.h"

/* uaudio notify event */
enum AUDIO_USB_OFFLOAD_NOTIFY_EVENT {
	/* AEF SRAM */
	EVENT_AFE_SRAM_ALLOCATE = 0,
	EVENT_AFE_SRAM_ALLOCATE_FROM_END,
	EVENT_AFE_SRAM_FREE,
	EVENT_DSP_SRAM_ALLOCATE,
	EVENT_DSP_SRAM_FREE,
	/* PM */
	EVENT_PM_AFE_SRAM_ON,
	EVENT_PM_AFE_SRAM_OFF,
	EVENT_PM_DSP_SRAM_ON,
	EVENT_PM_DSP_SRAM_OFF,
	USB_OFFLOAD_EVENT_NUM,
};

enum mtk_audio_usb_offload_mem_type {
	MEM_TYPE_SRAM_AFE = 0,
	MEM_TYPE_SRAM_ADSP,
	MEM_TYPE_SRAM_SLB,
	MEM_TYPE_DRAM,
	MEM_TYPE_NUM,
};

struct mtk_audio_usb_mem {
	dma_addr_t phys_addr;
	void *virt_addr;
	enum mtk_audio_usb_offload_mem_type type;

	unsigned int size;
	bool sram_inited;
};

struct mtk_audio_usb_offload {
	struct device *dev;
	const struct mtk_audio_usb_offload_sram_ops *ops;
};

struct mtk_audio_usb_offload_sram_ops {
	/* Reserved SRAM */
	struct mtk_audio_usb_mem *(*get_rsv_basic_sram)(void);
	/* Allocated/Free SRAM */
	struct mtk_audio_usb_mem *(*allocate_sram)(unsigned int size);
	int (*free_sram)(dma_addr_t addr);

	/* PM */
	int (*pm_runtime_control)(enum mtk_audio_usb_offload_mem_type type,
				   bool on);
};

/* ops */
struct mtk_audio_usb_offload *mtk_audio_usb_offload_register_ops(struct device *dev);
int mtk_audio_usb_offload_afe_hook(struct mtk_base_afe *afe);

#endif

