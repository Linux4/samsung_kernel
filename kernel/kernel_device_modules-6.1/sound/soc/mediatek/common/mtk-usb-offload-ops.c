// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: Yu-Syuan Cai <yusyuan.cai@mediatek.com>
 */

#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>

#include "mtk-usb-offload-ops.h"
#include "mtk-sram-manager.h"

//Basic + DL & UL
#define MAX_ALLOCATED_AFE_SRAM_COUNT 3

unsigned int audio_usb_offload_log;
module_param(audio_usb_offload_log, uint, 0644);
MODULE_PARM_DESC(audio_usb_offload_log, "Enable/Disable allocate audio USB Offload log");

unsigned int use_dram_only;
module_param(use_dram_only, uint, 0644);
MODULE_PARM_DESC(use_dram_only, "Enable/Disable allocate audio USB Offload SRAM");

#define AUDIO_USB_OFFLOAD_DBG(fmt, args...) do { \
	if (audio_usb_offload_log > 1) \
		pr_info("[AUO]%s " fmt, __func__, ## args); \
	} while (0)

#define AUDIO_USB_OFFLOAD_INFO(fmt, args...) do { \
	if (1) \
		pr_info("[AUO]%s " fmt, __func__, ## args); \
	} while (0)

#define AUDIO_USB_OFFLOAD_ERR(fmt, args...) do { \
	if (1) \
		pr_info("[AUO]%s " fmt, __func__, ## args); \
	} while (0)

/* ops */
extern const struct mtk_audio_usb_offload_sram_ops mtk_usb_offload_ops;

/* Global SRAM table*/
struct mtk_audio_usb_offload_mem {
	struct mtk_base_afe *afe;

	struct mtk_audio_usb_mem rsv_basic_sram;
	struct mtk_audio_usb_mem rsv_urb_sram;
	struct mtk_audio_usb_mem alloc_sram[MAX_ALLOCATED_AFE_SRAM_COUNT];
};
static struct mtk_audio_usb_offload_mem g_auo_sram;

/* Event implementation handler*/
static int mtk_audio_usb_offload_event_handler(unsigned long event, void *ptr)
{
	struct mtk_audio_usb_mem *umem = NULL;
	int ret = 0;

	AUDIO_USB_OFFLOAD_DBG("%s() event[%lu]\n", __func__, event);

	switch (event) {
	case EVENT_PM_AFE_SRAM_ON:
	if (g_auo_sram.afe)
		pm_runtime_get_sync(g_auo_sram.afe->dev);
	break;
	case EVENT_PM_AFE_SRAM_OFF:
	if (g_auo_sram.afe)
		pm_runtime_put(g_auo_sram.afe->dev);
	break;
	case EVENT_AFE_SRAM_ALLOCATE:
	case EVENT_AFE_SRAM_ALLOCATE_FROM_END:
	if (g_auo_sram.afe && ptr) {
		umem = (struct mtk_audio_usb_mem *)ptr;

		ret = mtk_audio_sram_allocate(g_auo_sram.afe->sram,
					    &umem->phys_addr,
					    (unsigned char **)&umem->virt_addr,
					    umem->size,
					    ptr,
					    SNDRV_PCM_FORMAT_S24,
					    true,
					    event == EVENT_AFE_SRAM_ALLOCATE ? false : true);
		if (ret == 0)
			umem->sram_inited = true;
	}
	break;
	case EVENT_AFE_SRAM_FREE:
	if (g_auo_sram.afe && ptr) {
		umem = (struct mtk_audio_usb_mem *)ptr;

		if (umem->sram_inited) {
			ret = mtk_audio_sram_free(g_auo_sram.afe->sram, ptr);
			umem->sram_inited = false;
		}
	}
	break;
	default:
	break;
	}

	return ret;
}


/* Parse devicetree for reserved sram */
int mtk_audio_usb_offload_sram_init(char *of_compatible,
				    struct mtk_audio_usb_mem *type_mem)
{
	struct device_node *sram_node = NULL;
	const __be32 *regaddr_p;
	int ret = 0;
	u64 regaddr64, size64;

	if (type_mem == NULL)
		return -ENODEV;

	/* Step 1, query reserved AFE SRAM */
	/* parse info from device tree */
	sram_node = of_find_compatible_node(NULL, NULL, of_compatible);
	if (!sram_node) {
		AUDIO_USB_OFFLOAD_ERR("find sram node failed.\n");
		goto of_error;
	}

	/* Check afe sram is reserved */
	if (!of_property_read_bool(sram_node, "afe-reserved")) {
		AUDIO_USB_OFFLOAD_ERR("Do not reserved afe sram.\n");
		goto of_error;
	}

	/* get physical address, size */
	regaddr_p = of_get_address(sram_node, 0, &size64, NULL);
	if (!regaddr_p) {
		AUDIO_USB_OFFLOAD_ERR("get sram address fail.\n");
		goto of_error;
	}

	/* iomap sram address */
	type_mem->virt_addr = of_iomap(sram_node, 0);
	if (type_mem->virt_addr == NULL) {
		AUDIO_USB_OFFLOAD_ERR("type_mem->virt_addr == NULL.\n");
		goto of_error;
	}

	regaddr64 = of_translate_address(sram_node, regaddr_p);
	type_mem->phys_addr = (dma_addr_t)regaddr64;
	type_mem->size = (unsigned int)size64;
	type_mem->type = MEM_TYPE_SRAM_AFE;
	type_mem->sram_inited = true;

	/* TODO Step 2, query reserved ADSP L2 SRAM */

	of_node_put(sram_node);

	return ret;

of_error:
	type_mem->phys_addr = 0;
	type_mem->size = 0;
	type_mem->virt_addr = NULL;
	type_mem->sram_inited = false;

	of_node_put(sram_node);

	return -ENODEV;
}

/* get reserved AFE sram */
struct mtk_audio_usb_mem *mtk_audio_usb_offload_get_basic_sram(void)
{
	int ret = 0;
	struct mtk_audio_usb_mem *basic = NULL;

	if (use_dram_only) {
		AUDIO_USB_OFFLOAD_ERR("User force to not get basic SRAM.\n");
		goto basic_error;
	}

	basic = &(g_auo_sram.rsv_basic_sram);

	if (!basic->sram_inited) {
		ret = mtk_audio_usb_offload_sram_init(
			"mediatek,audio_xhci_sram", basic);

		if (ret < 0)
			goto basic_error;
	}

	AUDIO_USB_OFFLOAD_INFO("ret = %d, size %d, virt_addr %p, phys_addr %pad\n",
		 ret, basic->size, basic->virt_addr, &basic->phys_addr);

	return basic;

basic_error:
	return NULL;
}
EXPORT_SYMBOL_GPL(mtk_audio_usb_offload_get_basic_sram);

/* AFE SRAM allocated */
int get_avail_idx_from_list(struct mtk_audio_usb_mem *list)
{
	int i = 0;

	for (i = 0; i < MAX_ALLOCATED_AFE_SRAM_COUNT; i++) {
		if (!list[i].sram_inited)
			return i;
	}

	return -EINVAL;
}

int get_target_idx_from_list(struct mtk_audio_usb_mem *list, dma_addr_t target)
{
	int i = 0;

	for (i = 0; i < MAX_ALLOCATED_AFE_SRAM_COUNT; i++) {
		if (list[i].sram_inited) {
			if (list[i].phys_addr == target)
				return i;
		}
	}

	return -EINVAL;
}

struct mtk_audio_usb_mem
*mtk_audio_usb_offload_allocate_afe_sram(unsigned int size)
{
	int avail_idx = -1;
	struct mtk_audio_usb_mem *target_alloc = NULL;

	if (use_dram_only) {
		AUDIO_USB_OFFLOAD_INFO("User force to not allocate SRAM.\n");
		goto force_not_alloc;
	}

	avail_idx = get_avail_idx_from_list(g_auo_sram.alloc_sram);
	if (avail_idx < 0) {
		AUDIO_USB_OFFLOAD_INFO("Do not have avail item.\n");
		goto allocate_error;
	}

	target_alloc = &(g_auo_sram.alloc_sram[avail_idx]);

	if (avail_idx == 0 && g_auo_sram.rsv_basic_sram.sram_inited) {
		// use reserved basic sram
		target_alloc->phys_addr = g_auo_sram.rsv_basic_sram.phys_addr;
		target_alloc->virt_addr = g_auo_sram.rsv_basic_sram.virt_addr;
		target_alloc->size = g_auo_sram.rsv_basic_sram.size;
		target_alloc->sram_inited = g_auo_sram.rsv_basic_sram.sram_inited;
	} else {
		//request allocate size
		target_alloc->size = size;

		mtk_audio_usb_offload_event_handler(avail_idx == 0 ?
							EVENT_AFE_SRAM_ALLOCATE_FROM_END : EVENT_AFE_SRAM_ALLOCATE,
							(void *)target_alloc);
	}

	if (!target_alloc->sram_inited) {
		AUDIO_USB_OFFLOAD_ERR("Failed.\n");
		goto allocate_error;
	}

	target_alloc->type = MEM_TYPE_SRAM_AFE;

	AUDIO_USB_OFFLOAD_INFO("avail_idx[%d]: size %d, virt_addr %p, phys_addr %pad\n",
		 avail_idx, target_alloc->size, target_alloc->virt_addr, &target_alloc->phys_addr);

	return target_alloc;

allocate_error:
	AUDIO_USB_OFFLOAD_ERR("SRAM for USB offload is not enough, return 0!\n");
force_not_alloc:
	return NULL;
}
EXPORT_SYMBOL_GPL(mtk_audio_usb_offload_allocate_afe_sram);

/* Allocate from ADSP L2 SRAM */
struct mtk_audio_usb_mem
*mtk_audio_usb_offload_allocate_adsp_sram(unsigned int size)
{
	/* TODO: L2 SRAM */
	return NULL;
}
EXPORT_SYMBOL_GPL(mtk_audio_usb_offload_allocate_adsp_sram);

struct mtk_audio_usb_mem *mtk_audio_usb_offload_allocate_sram(unsigned int size)
{
	struct mtk_audio_usb_mem *alloc_sram = NULL;

	/* Step 1 : Allocate from AFE SRAM */
	alloc_sram = mtk_audio_usb_offload_allocate_afe_sram(size);
	if (alloc_sram != NULL) {
		AUDIO_USB_OFFLOAD_INFO("allocated from afe sram, addr: %pad\n",
			 &alloc_sram->phys_addr);
		goto allocate_done;
	}

	/* Step 2 : Allocate from ADSP L2 SRAM */
	alloc_sram = mtk_audio_usb_offload_allocate_adsp_sram(size);
	if (alloc_sram != NULL) {
		AUDIO_USB_OFFLOAD_INFO("allocated from dsp sram, addr: %pad\n",
			 &alloc_sram->phys_addr);
		goto allocate_done;
	}
	/* Step 3 : Allocate from SLB SRAM */

	AUDIO_USB_OFFLOAD_ERR("Failed, return NULL.\n");
	return NULL;

allocate_done:
	return alloc_sram;
}
EXPORT_SYMBOL_GPL(mtk_audio_usb_offload_allocate_sram);

int mtk_audio_usb_offload_free_sram(dma_addr_t addr)
{
	int ret = 0;
	int target_idx = -1;
	struct mtk_audio_usb_mem *target_free = NULL;

	if (addr == 0) {
		AUDIO_USB_OFFLOAD_ERR("addr == 0, return.\n");
		goto free_error;
	}

	target_idx = get_target_idx_from_list(g_auo_sram.alloc_sram, addr);
	if (target_idx < 0) {
		AUDIO_USB_OFFLOAD_ERR("Do not found target item\n");
		goto free_error;
	}

	target_free = &(g_auo_sram.alloc_sram[target_idx]);
	AUDIO_USB_OFFLOAD_INFO("target_idx[%d]: size %d, virt_addr %p, phys_addr %pad\n",
		 target_idx, target_free->size, target_free->virt_addr, &target_free->phys_addr);


	switch (target_free->type) {
	case MEM_TYPE_SRAM_AFE:
		mtk_audio_usb_offload_event_handler(EVENT_AFE_SRAM_FREE, (void *)target_free);
	break;
	case MEM_TYPE_SRAM_ADSP:
	break;
	case MEM_TYPE_SRAM_SLB:
	default:
	break;
	}

	memset(target_free, 0, sizeof(struct mtk_audio_usb_mem));

free_error:
	return ret;
}

int mtk_audio_usb_offload_pm_runtime_control(enum mtk_audio_usb_offload_mem_type type,
					     bool on)
{
	AUDIO_USB_OFFLOAD_INFO("mem type = %d, %s", type, on ? "ON" : "OFF");

	switch (type) {
	case MEM_TYPE_SRAM_AFE:
		mtk_audio_usb_offload_event_handler(on ?
			EVENT_PM_AFE_SRAM_ON :
			EVENT_PM_AFE_SRAM_OFF, NULL);

	break;
	case MEM_TYPE_SRAM_ADSP:
		mtk_audio_usb_offload_event_handler(on ?
			EVENT_PM_DSP_SRAM_ON :
			EVENT_PM_DSP_SRAM_OFF, NULL);
	break;
	case MEM_TYPE_SRAM_SLB:
	break;
	case MEM_TYPE_DRAM:
	break;
	default:
	break;
	}
	return 0;
}

/* ops register */
struct mtk_audio_usb_offload *mtk_audio_usb_offload_register_ops(struct device *dev)
{
	struct mtk_audio_usb_offload *auo_intf;

	auo_intf = devm_kzalloc(dev, sizeof(struct mtk_audio_usb_offload), GFP_KERNEL);
	if (!auo_intf) {
		AUDIO_USB_OFFLOAD_ERR("Failed.\n");
		return NULL;
	}

	auo_intf->ops = &mtk_usb_offload_ops;
	auo_intf->dev = dev;

	AUDIO_USB_OFFLOAD_INFO("ops = %p.\n", auo_intf->ops);

	return auo_intf;
}
EXPORT_SYMBOL_GPL(mtk_audio_usb_offload_register_ops);

int mtk_audio_usb_offload_afe_hook(struct mtk_base_afe *afe)
{
	if (mtk_audio_usb_offload_get_basic_sram() != NULL)
		AUDIO_USB_OFFLOAD_INFO("found reserved sram\n");

	if (afe && g_auo_sram.afe == NULL) {
		g_auo_sram.afe = afe;
		AUDIO_USB_OFFLOAD_INFO("hook afe base = %p.\n", afe);
		return 0;
	}

	if (afe == NULL && g_auo_sram.afe != NULL) {
		g_auo_sram.afe = NULL;
		AUDIO_USB_OFFLOAD_INFO("unhook afe base\n");
		return 0;
	}

	return -EINVAL;
}
EXPORT_SYMBOL_GPL(mtk_audio_usb_offload_afe_hook);

const struct mtk_audio_usb_offload_sram_ops mtk_usb_offload_ops = {
	.get_rsv_basic_sram = mtk_audio_usb_offload_get_basic_sram,
	.allocate_sram = mtk_audio_usb_offload_allocate_sram,
	.free_sram = mtk_audio_usb_offload_free_sram,
	.pm_runtime_control = mtk_audio_usb_offload_pm_runtime_control,
};

MODULE_DESCRIPTION("Mediatek USB offload ops");
MODULE_AUTHOR("Yu-Syuan Cai <yusyuan.cai@mediatek.com>");
MODULE_LICENSE("GPL");

