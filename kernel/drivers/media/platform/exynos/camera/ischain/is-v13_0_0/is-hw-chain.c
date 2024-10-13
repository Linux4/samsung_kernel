// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 * Pablo v9.1 specific functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>

#if IS_ENABLED(CONFIG_EXYNOS_SCI)
#include <soc/samsung/exynos-sci.h>
#endif

#include "is-config.h"
#include "is-param.h"
#include "is-type.h"
#include "is-core.h"
#include "is-hw-chain.h"
#include "is-device-sensor.h"
#include "is-device-csi.h"
#include "is-device-ischain.h"
#include "is-hw.h"
#include "../../interface/is-interface-library.h"
#include "votf/pablo-votf.h"
#include "is-hw-dvfs.h"
#include "pablo-device-iommu-group.h"
#include "api/is-hw-api-csis_pdp_top.h"
#include "pablo-smc.h"
#include "pablo-crta-interface.h"
#include "pablo-crta-bufmgr.h"
#include "pablo-irq.h"

#define CHK_HW_3AA(hw_id)	\
	(hw_id >= DEV_HW_3AA0 && hw_id <= DEV_HW_3AA_MAX)

#define CHK_HW_PDP(hw_id)	\
	(hw_id >= DEV_HW_PAF0 && hw_id <= DEV_HW_PAF_MAX)

static const enum is_subdev_id subdev_id[GROUP_SLOT_MAX] = {
	[GROUP_SLOT_SENSOR] = ENTRY_SENSOR,
	[GROUP_SLOT_PAF] = ENTRY_PAF,
	[GROUP_SLOT_3AA] = ENTRY_3AA,
	[GROUP_SLOT_BYRP] = ENTRY_BYRP,
	[GROUP_SLOT_RGBP] = ENTRY_RGBP,
	[GROUP_SLOT_YUVSC] = ENTRY_YUVSC,
	[GROUP_SLOT_MLSC] = ENTRY_MLSC,
	[GROUP_SLOT_MTNR] = ENTRY_MTNR,
	[GROUP_SLOT_MSNR] = ENTRY_MSNR,
	[GROUP_SLOT_YUVP] = ENTRY_YUVP,
	[GROUP_SLOT_MCS] = ENTRY_MCS,
};

static const char *const subdev_id_name[GROUP_SLOT_MAX] = {
	[GROUP_SLOT_SENSOR] = "SSX",
	[GROUP_SLOT_PAF] = "PXS",
	[GROUP_SLOT_3AA] = "3XS",
	[GROUP_SLOT_BYRP] = "BYRP",
	[GROUP_SLOT_RGBP] = "RGBP",
	[GROUP_SLOT_YUVSC] = "YUVSC",
	[GROUP_SLOT_MLSC] = "MLSC",
	[GROUP_SLOT_MTNR] = "MTNR",
	[GROUP_SLOT_MSNR] = "MSNR",
	[GROUP_SLOT_YUVP] = "YUVP",
	[GROUP_SLOT_MCS] = "MXS",
};

static const struct is_subdev_ops is_subdev_dummy_ops = {
	.bypass	= NULL,
	.cfg	= NULL,
	.tag	= NULL,
	.get	= NULL,
};

static const struct is_subdev_ops *pablo_get_is_subdev_dummy_ops(void)
{
	return &is_subdev_dummy_ops;
}

static const struct is_subdev_ops *(*subdev_ops[GROUP_SLOT_MAX])(void) = {
	[GROUP_SLOT_SENSOR] = pablo_get_is_subdev_sensor_ops,
	[GROUP_SLOT_PAF] = pablo_get_is_subdev_paf_ops,
	[GROUP_SLOT_BYRP] = pablo_get_is_subdev_byrp_ops,
	[GROUP_SLOT_RGBP] = pablo_get_is_subdev_rgbp_ops,
	[GROUP_SLOT_YUVSC] = pablo_get_is_subdev_yuvsc_ops,
	[GROUP_SLOT_MLSC] = pablo_get_is_subdev_dummy_ops,
	[GROUP_SLOT_MTNR] = pablo_get_is_subdev_dummy_ops,
	[GROUP_SLOT_MSNR] = pablo_get_is_subdev_dummy_ops,
	[GROUP_SLOT_YUVP] = pablo_get_is_subdev_yuvp_ops,
	[GROUP_SLOT_MCS] = pablo_get_is_subdev_mcs_ops,
};

static const int ioresource_to_hw_id[IORESOURCE_MAX] = {
	[0 ... IORESOURCE_MAX - 1] = DEV_HW_END,
	[IORESOURCE_BYRP0] = DEV_HW_BYRP0,
	[IORESOURCE_BYRP1] = DEV_HW_BYRP1,
	[IORESOURCE_BYRP2] = DEV_HW_BYRP2,
	[IORESOURCE_BYRP3] = DEV_HW_BYRP3,
	[IORESOURCE_BYRP4] = DEV_HW_BYRP4,
	[IORESOURCE_RGBP0] = DEV_HW_RGBP0,
	[IORESOURCE_RGBP1] = DEV_HW_RGBP1,
	[IORESOURCE_RGBP2] = DEV_HW_RGBP2,
	[IORESOURCE_RGBP3] = DEV_HW_RGBP3,
	[IORESOURCE_YUVSC0] = DEV_HW_YUVSC0,
	[IORESOURCE_YUVSC1] = DEV_HW_YUVSC1,
	[IORESOURCE_YUVSC2] = DEV_HW_YUVSC2,
	[IORESOURCE_YUVSC3] = DEV_HW_YUVSC3,
	[IORESOURCE_MLSC0] = DEV_HW_MLSC0,
	[IORESOURCE_MLSC1] = DEV_HW_MLSC1,
	[IORESOURCE_MLSC2] = DEV_HW_MLSC2,
	[IORESOURCE_MLSC3] = DEV_HW_MLSC3,
	[IORESOURCE_MTNR0] = DEV_HW_MTNR0,
	[IORESOURCE_MTNR1] = DEV_HW_MTNR1,
	[IORESOURCE_MSNR] = DEV_HW_MSNR,
	[IORESOURCE_YUVP] = DEV_HW_YPP,
	[IORESOURCE_MCSC] = DEV_HW_MCSC0,
};

static const u32 s2mpu_address_table[SYSMMU_DMAX_S2] = {
	[SYSMMU_S0_BYRP_S2 ... SYSMMU_DMAX_S2 - 1] = 0,
	[SYSMMU_S0_BYRP_S2] = 0x23160054,
	[SYSMMU_S0_CSIS_S2] = 0x23A20054,
	[SYSMMU_S0_ICPU_S2] = 0x248D0054,
	[SYSMMU_S0_MCSC_S2] = 0x25890054,
	[SYSMMU_S0_MLSC_S2] = 0x278A0054,
	[SYSMMU_S0_MTNR_S2] = 0x26920054,
	[SYSMMU_RGBP_S0_S2] = 0x250A0054,
	[SYSMMU_S0_YUVP_S2] = 0x28890054,
};


const int *is_hw_get_ioresource_to_hw_id(void)
{
	return ioresource_to_hw_id;
}

static inline void __is_isr_host(void *data, int handler_id)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[handler_id];

	if (intr_hw->valid)
		intr_hw->handler(intr_hw->id, (void *)itf_hw->hw_ip);
	else
		err_itfc("[ID:%d](1) empty handler!!", itf_hw->id);
}

/*
 * Interrupt handler definitions
 */
/* BYRP */
static irqreturn_t __is_isr1_byrp0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_byrp0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_byrp1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_byrp1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_byrp2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_byrp2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_byrp3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_byrp3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_byrp4(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_byrp4(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* RGBP */
static irqreturn_t __is_isr1_rgbp0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_rgbp0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_rgbp1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_rgbp1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_rgbp2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_rgbp2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_rgbp3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_rgbp3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* YUVSC */
static irqreturn_t __is_isr1_yuvsc0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_yuvsc0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_yuvsc1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_yuvsc1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_yuvsc2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_yuvsc2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_yuvsc3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_yuvsc3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* MLSC */
static irqreturn_t __is_isr1_mlsc0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mlsc0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_mlsc1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mlsc1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_mlsc2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mlsc2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_mlsc3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mlsc3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* MTNR */
static irqreturn_t __is_isr1_mtnr0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mtnr0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_mtnr1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mtnr1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* MSNR */
static irqreturn_t __is_isr1_msnr(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_msnr(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* YUVP */
static irqreturn_t __is_isr1_yuvp(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_yuvp(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* MCSC */
static irqreturn_t __is_isr1_mcsc(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mcsc(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/*
 * HW group related functions
 */
void is_hw_get_subdev_info(u32 slot, u32 *id, const char **name, const struct is_subdev_ops **sops)
{
	*id = subdev_id[slot];
	*name = subdev_id_name[slot];
	*sops = subdev_ops[slot]();
}

int is_hw_group_task_cfg(struct is_group_task *gtask, u32 group_id)
{
	return 0;
}

int is_hw_group_open(void *group_data)
{
	int ret = 0;
	u32 group_id;
	struct is_subdev *leader;
	struct is_group *group;
	struct is_device_ischain *device;

	FIMC_BUG(!group_data);

	group = group_data;
	leader = &group->leader;
	device = group->device;
	group_id = group->id;

	switch (group_id) {
	case GROUP_ID_SS0:
	case GROUP_ID_SS1:
	case GROUP_ID_SS2:
	case GROUP_ID_SS3:
	case GROUP_ID_SS4:
	case GROUP_ID_SS5:
		leader->constraints_width = GROUP_SENSOR_MAX_WIDTH;
		leader->constraints_height = GROUP_SENSOR_MAX_HEIGHT;
		break;
	case GROUP_ID_PAF0:
	case GROUP_ID_PAF1:
	case GROUP_ID_PAF2:
	case GROUP_ID_PAF3:
		leader->constraints_width = GROUP_PDP_MAX_WIDTH;
		leader->constraints_height = GROUP_PDP_MAX_HEIGHT;
		break;
	case GROUP_ID_3AA0:
	case GROUP_ID_3AA1:
	case GROUP_ID_3AA2:
	case GROUP_ID_3AA3:
		leader->constraints_width = GROUP_3AA_MAX_WIDTH;
		leader->constraints_height = GROUP_3AA_MAX_HEIGHT;
		break;
	case GROUP_ID_BYRP:
	case GROUP_ID_RGBP:
		leader->constraints_width = GROUP_BYRP_MAX_WIDTH;
		leader->constraints_height = GROUP_BYRP_MAX_HEIGHT;
		break;
	case GROUP_ID_YUVP:
	case GROUP_ID_MCS0:
		leader->constraints_width = GROUP_MCFP_MAX_WIDTH;
		leader->constraints_height = GROUP_MCFP_MAX_HEIGHT;
		break;
	default:
		merr("(%s) is invalid", group, group_id_name[group_id]);
		break;
	}

	return ret;
}

int is_get_hw_list(int group_id, int *hw_list)
{
	int i;
	int hw_index = 0;

	/* initialization */
	for (i = 0; i < GROUP_HW_MAX; i++)
		hw_list[i] = -1;

	switch (group_id) {
	case GROUP_ID_PAF0:
		hw_list[hw_index] = DEV_HW_PAF0; hw_index++;
		break;
	case GROUP_ID_PAF1:
		hw_list[hw_index] = DEV_HW_PAF1; hw_index++;
		break;
	case GROUP_ID_PAF2:
		hw_list[hw_index] = DEV_HW_PAF2; hw_index++;
		break;
	case GROUP_ID_PAF3:
		hw_list[hw_index] = DEV_HW_PAF3; hw_index++;
		break;
	case GROUP_ID_3AA0:
		hw_list[hw_index] = DEV_HW_3AA0; hw_index++;
		break;
	case GROUP_ID_3AA1:
		hw_list[hw_index] = DEV_HW_3AA1; hw_index++;
		break;
	case GROUP_ID_3AA2:
		hw_list[hw_index] = DEV_HW_3AA2; hw_index++;
		break;
	case GROUP_ID_3AA3:
		hw_list[hw_index] = DEV_HW_3AA3; hw_index++;
		break;
	case GROUP_ID_BYRP0:
		hw_list[hw_index] = DEV_HW_BYRP0; hw_index++;
		break;
	case GROUP_ID_BYRP1:
		hw_list[hw_index] = DEV_HW_BYRP1; hw_index++;
		break;
	case GROUP_ID_BYRP2:
		hw_list[hw_index] = DEV_HW_BYRP2; hw_index++;
		break;
	case GROUP_ID_BYRP3:
		hw_list[hw_index] = DEV_HW_BYRP3; hw_index++;
		break;
	case GROUP_ID_BYRP4:
		hw_list[hw_index] = DEV_HW_BYRP4; hw_index++;
		break;
	case GROUP_ID_RGBP0:
		hw_list[hw_index] = DEV_HW_RGBP0; hw_index++;
		break;
	case GROUP_ID_RGBP1:
		hw_list[hw_index] = DEV_HW_RGBP1; hw_index++;
		break;
	case GROUP_ID_RGBP2:
		hw_list[hw_index] = DEV_HW_RGBP2; hw_index++;
		break;
	case GROUP_ID_RGBP3:
		hw_list[hw_index] = DEV_HW_RGBP3; hw_index++;
		break;
	case GROUP_ID_YUVSC0:
		hw_list[hw_index] = DEV_HW_YUVSC0; hw_index++;
		break;
	case GROUP_ID_YUVSC1:
		hw_list[hw_index] = DEV_HW_YUVSC1; hw_index++;
		break;
	case GROUP_ID_YUVSC2:
		hw_list[hw_index] = DEV_HW_YUVSC2; hw_index++;
		break;
	case GROUP_ID_YUVSC3:
		hw_list[hw_index] = DEV_HW_YUVSC3; hw_index++;
		break;
	case GROUP_ID_MLSC0:
		hw_list[hw_index] = DEV_HW_MLSC0; hw_index++;
		break;
	case GROUP_ID_MLSC1:
		hw_list[hw_index] = DEV_HW_MLSC1; hw_index++;
		break;
	case GROUP_ID_MLSC2:
		hw_list[hw_index] = DEV_HW_MLSC2; hw_index++;
		break;
	case GROUP_ID_MLSC3:
		hw_list[hw_index] = DEV_HW_MLSC3; hw_index++;
		break;
	case GROUP_ID_MTNR0:
		hw_list[hw_index] = DEV_HW_MTNR0; hw_index++;
		break;
	case GROUP_ID_MTNR1:
		hw_list[hw_index] = DEV_HW_MTNR1; hw_index++;
		break;
	case GROUP_ID_MSNR:
		hw_list[hw_index] = DEV_HW_MSNR; hw_index++;
		break;
	case GROUP_ID_YUVP:
		hw_list[hw_index] = DEV_HW_YPP; hw_index++;
		break;
	case GROUP_ID_MCS0:
		hw_list[hw_index] = DEV_HW_MCSC0; hw_index++;
		break;
	case GROUP_ID_MAX:
		break;
	default:
		err("Invalid group%d(%s)", group_id, group_id_name[group_id]);
		break;
	}

	return hw_index;
}
/*
 * System registers configurations
 */
static inline int __is_hw_get_address(struct platform_device *pdev,
				struct is_interface_hwip *itf_hwip,
				int hw_id, char *hw_name,
				u32 resource_id, enum base_reg_index reg_index,
				bool alloc_memlog)
{
	struct resource *mem_res = NULL;

	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, resource_id);
	if (!mem_res) {
		dev_err(&pdev->dev, "Failed to get io memory region\n");
		return -EINVAL;
	}

	itf_hwip->hw_ip->regs_start[reg_index] = mem_res->start;
	itf_hwip->hw_ip->regs_end[reg_index] = mem_res->end;
	itf_hwip->hw_ip->regs[reg_index] =
		ioremap(mem_res->start, resource_size(mem_res));
	if (!itf_hwip->hw_ip->regs[reg_index]) {
		dev_err(&pdev->dev, "Failed to remap io region\n");
		return -EINVAL;
	}

	if (alloc_memlog)
		is_debug_memlog_alloc_dump(mem_res->start,
					resource_size(mem_res), hw_name);

	info_itfc("[ID:%2d] %s VA(0x%lx)\n", hw_id, hw_name,
		(ulong)itf_hwip->hw_ip->regs[reg_index]);

	return 0;
}

int is_hw_get_address(void *itfc_data, void *pdev_data, int hw_id)
{
	struct platform_device *pdev = NULL;
	struct is_interface_hwip *itf_hwip = NULL;

	FIMC_BUG(!itfc_data);
	FIMC_BUG(!pdev_data);

	itf_hwip = (struct is_interface_hwip *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	itf_hwip->hw_ip->dump_for_each_reg = NULL;
	itf_hwip->hw_ip->dump_reg_list_size = 0;

	switch (hw_id) {
	case DEV_HW_BYRP0:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "BYRP0", IORESOURCE_BYRP0, REG_SETA, true);
		break;
	case DEV_HW_BYRP1:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "BYRP1", IORESOURCE_BYRP1, REG_SETA, true);
		break;
	case DEV_HW_BYRP2:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "BYRP2", IORESOURCE_BYRP2, REG_SETA, true);
		break;
	case DEV_HW_BYRP3:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "BYRP3", IORESOURCE_BYRP3, REG_SETA, true);
		break;
	case DEV_HW_BYRP4:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "BYRP4", IORESOURCE_BYRP4, REG_SETA, true);
		break;
	case DEV_HW_RGBP0:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "RGBP0", IORESOURCE_RGBP0, REG_SETA, true);
		break;
	case DEV_HW_RGBP1:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "RGBP1", IORESOURCE_RGBP1, REG_SETA, true);
		break;
	case DEV_HW_RGBP2:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "RGBP2", IORESOURCE_RGBP2, REG_SETA, true);
		break;
	case DEV_HW_RGBP3:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "RGBP3", IORESOURCE_RGBP3, REG_SETA, true);
		break;
	case DEV_HW_YUVSC0:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "YUVSC0", IORESOURCE_YUVSC0, REG_SETA, true);
		break;
	case DEV_HW_YUVSC1:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "YUVSC1", IORESOURCE_YUVSC1, REG_SETA, true);
		break;
	case DEV_HW_YUVSC2:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "YUVSC2", IORESOURCE_YUVSC2, REG_SETA, true);
		break;
	case DEV_HW_YUVSC3:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "YUVSC3", IORESOURCE_YUVSC3, REG_SETA, true);
		break;
	case DEV_HW_MLSC0:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MLSC0", IORESOURCE_MLSC0, REG_SETA, true);
		break;
	case DEV_HW_MLSC1:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MLSC1", IORESOURCE_MLSC1, REG_SETA, true);
		break;
	case DEV_HW_MLSC2:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MLSC2", IORESOURCE_MLSC2, REG_SETA, true);
		break;
	case DEV_HW_MLSC3:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MLSC3", IORESOURCE_MLSC3, REG_SETA, true);
		break;
	case DEV_HW_MTNR0:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MTNR0", IORESOURCE_MTNR0, REG_SETA, true);
		break;
	case DEV_HW_MTNR1:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MTNR1", IORESOURCE_MTNR1, REG_SETA, true);
		break;
	case DEV_HW_MSNR:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MSNR", IORESOURCE_MSNR, REG_SETA, true);
		break;
	case DEV_HW_YPP:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "YUVP", IORESOURCE_YUVP, REG_SETA, true);
		break;
	case DEV_HW_MCSC0:
		__is_hw_get_address(
			pdev, itf_hwip, hw_id, "MCSC", IORESOURCE_MCSC, REG_SETA, true);
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return 0;
}

int is_hw_get_irq(void *itfc_data, void *pdev_data, int hw_id)
{
	struct is_interface_hwip *itf_hwip = NULL;
	struct platform_device *pdev = NULL;
	int ret = 0;

	FIMC_BUG(!itfc_data);

	itf_hwip = (struct is_interface_hwip *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	switch (hw_id) {
	case DEV_HW_BYRP0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 0);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq byrp0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 1);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq byrp0-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_BYRP1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 2);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq byrp1-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 3);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq byrp1-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_BYRP2:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 4);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq byrp2-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 5);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq byrp2-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_BYRP3:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 6);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq byrp3-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 7);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq byrp3-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_BYRP4:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 8);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq byrp4-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 9);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq byrp4-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_RGBP0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 10);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq rgbp0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 11);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq rgbp0-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_RGBP1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 12);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq rgbp1-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 13);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq rgbp1-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_RGBP2:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 14);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq rgbp2-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 15);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq rgbp2-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_RGBP3:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 16);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq rgbp3-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 17);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq rgbp3-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_YUVSC0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 18);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvsc0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 19);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq yuvsc0-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_YUVSC1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 20);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvsc1-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 21);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq yuvsc1-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_YUVSC2:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 22);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvsc2-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 23);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq yuvsc2-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_YUVSC3:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 24);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvsc3-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 25);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq yuvsc3-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MLSC0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 26);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mlsc0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 27);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mlsc0-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MLSC1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 28);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mlsc1-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 29);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mlsc1-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MLSC2:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 30);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mlsc2-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 31);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mlsc2-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MLSC3:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 32);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mlsc3-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 33);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mlsc3-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MTNR0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 34);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mtnr0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 35);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mtnr0-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MTNR1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 36);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mtnr1-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 37);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mtnr1-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MSNR:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 38);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq msnr-0");
			return -EINVAL;
		}
		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 39);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq msnr-1");
			return -EINVAL;
		}
		break;
	case DEV_HW_YPP:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 40);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvp-0");
			return -EINVAL;
		}
		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 41);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq yuvp-1");
			return -EINVAL;
		}
		break;
	case DEV_HW_MCSC0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 42);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mcsc-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 43);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mcsc-1\n");
			return -EINVAL;
		}
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return ret;
}

static inline int __is_hw_request_irq(struct is_interface_hwip *itf_hwip,
	const char *name, int isr_num,
	unsigned int added_irq_flags,
	irqreturn_t (*func)(int, void *))
{
	size_t name_len = 0;
	int ret = 0;

	name_len = sizeof(itf_hwip->irq_name[isr_num]);
	snprintf(itf_hwip->irq_name[isr_num], name_len, "%s-%d", name, isr_num);

	ret = pablo_request_irq(itf_hwip->irq[isr_num], func,
		itf_hwip->irq_name[isr_num],
		added_irq_flags,
		itf_hwip);
	if (ret) {
		err_itfc("[HW:%s] request_irq [%d] fail", name, isr_num);
		return -EINVAL;
	}

	itf_hwip->handler[isr_num].id = isr_num;
	itf_hwip->handler[isr_num].valid = true;

	return ret;
}

static inline int __is_hw_free_irq(struct is_interface_hwip *itf_hwip, int isr_num)
{
	pablo_free_irq(itf_hwip->irq[isr_num], itf_hwip);

	return 0;
}

int is_hw_request_irq(void *itfc_data, int hw_id)
{
	struct is_interface_hwip *itf_hwip = NULL;
	int ret = 0;

	FIMC_BUG(!itfc_data);


	itf_hwip = (struct is_interface_hwip *)itfc_data;

	switch (hw_id) {
	case DEV_HW_BYRP0:
		ret = __is_hw_request_irq(itf_hwip, "byrp0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_byrp0);
		ret = __is_hw_request_irq(itf_hwip, "byrp0", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_byrp0);
		break;
	case DEV_HW_BYRP1:
		ret = __is_hw_request_irq(itf_hwip, "byrp1", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_byrp1);
		ret = __is_hw_request_irq(itf_hwip, "byrp1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_byrp1);
		break;
	case DEV_HW_BYRP2:
		ret = __is_hw_request_irq(itf_hwip, "byrp2", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_byrp2);
		ret = __is_hw_request_irq(itf_hwip, "byrp2", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_byrp2);
		break;
	case DEV_HW_BYRP3:
		ret = __is_hw_request_irq(itf_hwip, "byrp3", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_byrp3);
		ret = __is_hw_request_irq(itf_hwip, "byrp3", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_byrp3);
		break;
	case DEV_HW_BYRP4:
		ret = __is_hw_request_irq(itf_hwip, "byrp4", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_byrp4);
		ret = __is_hw_request_irq(itf_hwip, "byrp4", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_byrp4);
		break;
	case DEV_HW_RGBP0:
		ret = __is_hw_request_irq(itf_hwip, "rgbp0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_rgbp0);
		ret = __is_hw_request_irq(itf_hwip, "rgbp0", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_rgbp0);
		break;
	case DEV_HW_RGBP1:
		ret = __is_hw_request_irq(itf_hwip, "rgbp1", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_rgbp1);
		ret = __is_hw_request_irq(itf_hwip, "rgbp1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_rgbp1);
		break;
	case DEV_HW_RGBP2:
		ret = __is_hw_request_irq(itf_hwip, "rgbp2", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_rgbp2);
		ret = __is_hw_request_irq(itf_hwip, "rgbp2", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_rgbp2);
		break;
	case DEV_HW_RGBP3:
		ret = __is_hw_request_irq(itf_hwip, "rgbp3", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_rgbp3);
		ret = __is_hw_request_irq(itf_hwip, "rgbp3", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_rgbp3);
		break;
	case DEV_HW_YUVSC0:
		ret = __is_hw_request_irq(itf_hwip, "yuvsc0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_yuvsc0);
		ret = __is_hw_request_irq(itf_hwip, "yuvsc0", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_yuvsc0);
		break;
	case DEV_HW_YUVSC1:
		ret = __is_hw_request_irq(itf_hwip, "yuvsc1", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_yuvsc1);
		ret = __is_hw_request_irq(itf_hwip, "yuvsc1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_yuvsc1);
		break;
	case DEV_HW_YUVSC2:
		ret = __is_hw_request_irq(itf_hwip, "yuvsc2", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_yuvsc2);
		ret = __is_hw_request_irq(itf_hwip, "yuvsc2", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_yuvsc2);
		break;
	case DEV_HW_YUVSC3:
		ret = __is_hw_request_irq(itf_hwip, "yuvsc3", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_yuvsc3);
		ret = __is_hw_request_irq(itf_hwip, "yuvsc3", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_yuvsc3);
		break;
	case DEV_HW_MLSC0:
		ret = __is_hw_request_irq(itf_hwip, "mlsc0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mlsc0);
		ret = __is_hw_request_irq(itf_hwip, "mlsc0", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mlsc0);
		break;
	case DEV_HW_MLSC1:
		ret = __is_hw_request_irq(itf_hwip, "mlsc1", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mlsc1);
		ret = __is_hw_request_irq(itf_hwip, "mlsc1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mlsc1);
		break;
	case DEV_HW_MLSC2:
		ret = __is_hw_request_irq(itf_hwip, "mlsc2", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mlsc2);
		ret = __is_hw_request_irq(itf_hwip, "mlsc2", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mlsc2);
		break;
	case DEV_HW_MLSC3:
		ret = __is_hw_request_irq(itf_hwip, "mlsc3", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mlsc3);
		ret = __is_hw_request_irq(itf_hwip, "mlsc3", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mlsc3);
		break;
	case DEV_HW_MTNR0:
		ret = __is_hw_request_irq(itf_hwip, "mtnr0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mtnr0);
		ret = __is_hw_request_irq(itf_hwip, "mtnr0", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mtnr0);
		break;
	case DEV_HW_MTNR1:
		ret = __is_hw_request_irq(itf_hwip, "mtnr1", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mtnr1);
		ret = __is_hw_request_irq(itf_hwip, "mtnr1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mtnr1);
		break;
	case DEV_HW_MSNR:
		ret = __is_hw_request_irq(itf_hwip, "msnr", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_msnr);
		ret = __is_hw_request_irq(itf_hwip, "msnr", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_msnr);
		break;
	case DEV_HW_YPP:
		ret = __is_hw_request_irq(itf_hwip, "yuvp", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_yuvp);
		ret = __is_hw_request_irq(itf_hwip, "yuvp", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_yuvp);
		break;
	case DEV_HW_MCSC0:
		ret = __is_hw_request_irq(itf_hwip, "mcsc", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mcsc);
		ret = __is_hw_request_irq(itf_hwip, "mcsc", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mcsc);
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return ret;
}

void is_hw_camif_init(void)
{
	u32 mux_addr[] = {0x1A190254, 0x1A190258, 0x1A19025C, 0x1A190260};
	void __iomem *addr;
	u32 mux_val = 0x3F; /* initial value */
	int i;

	for (i = 0; i < ARRAY_SIZE(mux_addr); i++){
		addr = ioremap(mux_addr[i], sizeof(u32));
		writel(mux_val, addr);
		iounmap(addr);
	}
}

#if (IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON))
void is_hw_s2mpu_cfg(void)
{
	void __iomem *reg;
	int idx;

	pr_info("[DBG] S2MPU disable\n");

	for (idx = 0; idx < SYSMMU_DMAX_S2; idx++) {
		if (!s2mpu_address_table[idx])
			continue;

		reg = ioremap(s2mpu_address_table[idx], 0x4);
		writel(0xFF, reg);
		iounmap(reg);
	}

}
PST_EXPORT_SYMBOL(is_hw_s2mpu_cfg);
#endif

int is_hw_camif_cfg(void *sensor_data)
{
	struct is_device_sensor *sensor;
	struct is_device_csi *csi;
	struct is_fid_loc *fid_loc;

	FIMC_BUG(!sensor_data);

	sensor = sensor_data;
	csi = (struct is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);
	fid_loc = &sensor->cfg->fid_loc;

	csis_pdp_top_frame_id_en(csi, fid_loc);

#if (IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON))
	is_hw_s2mpu_cfg();
#endif

	return 0;
}

int is_hw_ischain_cfg(void *ischain_data)
{
	int ret = 0;
	struct is_device_ischain *device;

	FIMC_BUG(!ischain_data);

	device = (struct is_device_ischain *)ischain_data;
	if (test_bit(IS_ISCHAIN_REPROCESSING, &device->state))
		return ret;

	return ret;
}

int is_hw_ischain_enable(struct is_core *core)
{
	int ret = 0;
	struct is_interface_hwip *itf_hwip = NULL;
	struct is_interface_ischain *itfc;
	struct is_hardware *hw;
	int hw_slot;
	u32 idx, i;

	itfc = &core->interface_ischain;
	hw = &core->hardware;

	/* irq affinity should be restored after S2R at gic600 */
	for (idx = 0; idx < IORESOURCE_MAX; idx++) {
		if (ioresource_to_hw_id[idx] >= DEV_HW_END)
			continue;

		hw_slot = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, ioresource_to_hw_id[idx]);
		itf_hwip = &(itfc->itf_ip[hw_slot]);

		for (i = 0; i < INTR_HWIP_MAX; i++) {
			if (!itf_hwip->handler[i].valid)
				continue;

			pablo_set_affinity_irq(itf_hwip->irq[i], true);
		}
	}

	votfitf_disable_service();

	info("%s: complete\n", __func__);

	return ret;
}

int is_hw_ischain_disable(struct is_core *core)
{
	info("%s: complete\n", __func__);

	return 0;
}

#ifdef ENABLE_HWACG_CONTROL
#define NUM_OF_CSIS 7
void is_hw_csi_qchannel_enable_all(bool enable)
{
	phys_addr_t csis_cmn_ctrls[NUM_OF_CSIS] = {
		0x23880004,
		0x23890004,
		0x238A0004,
		0x238B0004,
		0x238C0004,
		0x238D0004,
		0x238E0004,
	};
	phys_addr_t csis_pdp_top = 0x239D0000;
	void __iomem *base;
	u32 val;
	int i;

	if (enable) {
		/* CSIS_LINK */
		for (i = 0; i < NUM_OF_CSIS; i++) {
			base = ioremap(csis_cmn_ctrls[i], SZ_4);
			val = readl(base);
			writel(val | (1 << 20), base);
			iounmap(base);
		}
	}

	/* CSIS_PDP_TOP */
	base = ioremap(csis_pdp_top, 0x20);
	csis_pdp_top_qch_cfg(base, enable);
	iounmap(base);
}
#endif

void is_hw_interrupt_relay(struct is_group *group, void *hw_ip_data)
{
}

#if IS_ENABLED(CONFIG_EXYNOS_SCI)
static struct is_llc_way_num is_llc_way[IS_LLC_SN_END] = {
	/* default : VOTF 0MB, MCFP 0MB, ICPU 0MB*/
	[IS_LLC_SN_DEFAULT].votf = 0,
	[IS_LLC_SN_DEFAULT].mcfp = 0,
	[IS_LLC_SN_DEFAULT].icpu = 0,
	/* FHD scenario : VOTF 0MB, MCFP 1MB, ICPU 2MB */
	[IS_LLC_SN_FHD].votf = 0,
	[IS_LLC_SN_FHD].mcfp = 2,
	[IS_LLC_SN_FHD].icpu = 4,
	/* UHD scenario : VOTF 0.5MB, MCFP 2.5MB, ICPU 2MB */
	[IS_LLC_SN_UHD].votf = 1,
	[IS_LLC_SN_UHD].mcfp = 5,
	[IS_LLC_SN_UHD].icpu = 4,
	/* 8K scenario : VOTF 1MB, MCFP 1.5MB, ICPU 2MB */
	[IS_LLC_SN_8K].votf = 2,
	[IS_LLC_SN_8K].mcfp = 3,
	[IS_LLC_SN_8K].icpu = 4,
	/* preview scenario : VOTF 0MB, MCFP 1.5MB, ICPU 2MB */
	[IS_LLC_SN_PREVIEW].votf = 0,
	[IS_LLC_SN_PREVIEW].mcfp = 3,
	[IS_LLC_SN_PREVIEW].icpu = 4,
};
#endif

void is_hw_configure_llc(bool on, void *ischain, ulong *llc_state)
{
#if IS_ENABLED(CONFIG_EXYNOS_SCI)
	struct is_dvfs_scenario_param param;
	u32 votf, mcfp, icpu, size;
	struct is_device_ischain *device = (struct is_device_ischain *)ischain;

	/* way 1 means alloc 512K LLC */
	if (on) {
		is_hw_dvfs_get_scenario_param(device, 0, &param);

		if (param.mode == IS_DVFS_MODE_PHOTO) {
			votf = is_llc_way[IS_LLC_SN_PREVIEW].votf;
			mcfp = is_llc_way[IS_LLC_SN_PREVIEW].mcfp;
			icpu = is_llc_way[IS_LLC_SN_PREVIEW].icpu;
		} else if (param.mode == IS_DVFS_MODE_VIDEO) {
			switch (param.resol) {
			case IS_DVFS_RESOL_FHD:
				votf = is_llc_way[IS_LLC_SN_FHD].votf;
				mcfp = is_llc_way[IS_LLC_SN_FHD].mcfp;
				icpu = is_llc_way[IS_LLC_SN_FHD].icpu;
				break;
			case IS_DVFS_RESOL_UHD:
				votf = is_llc_way[IS_LLC_SN_UHD].votf;
				mcfp = is_llc_way[IS_LLC_SN_UHD].mcfp;
				icpu = is_llc_way[IS_LLC_SN_UHD].icpu;
				break;
			case IS_DVFS_RESOL_8K:
				votf = is_llc_way[IS_LLC_SN_8K].votf;
				mcfp = is_llc_way[IS_LLC_SN_8K].mcfp;
				icpu = is_llc_way[IS_LLC_SN_8K].icpu;
				break;
			default:
				votf = is_llc_way[IS_LLC_SN_DEFAULT].votf;
				mcfp = is_llc_way[IS_LLC_SN_DEFAULT].mcfp;
				icpu = is_llc_way[IS_LLC_SN_DEFAULT].icpu;
				break;
			}
		} else {
			votf = is_llc_way[IS_LLC_SN_DEFAULT].votf;
			mcfp = is_llc_way[IS_LLC_SN_DEFAULT].mcfp;
			icpu = is_llc_way[IS_LLC_SN_DEFAULT].icpu;
		}

		size = is_get_debug_param(IS_DEBUG_PARAM_LLC);
		if (size) {
			icpu = size / 10000;
			if (icpu > 16) icpu = 0;
			votf = (size / 100) % 100;
			if (votf > 16) votf = 0;
			mcfp = size % 100;
			if (mcfp > 16) mcfp = 0;
		}

		if (votf) {
			llc_region_alloc(LLC_REGION_CAM_CSIS, 1, votf);
			set_bit(LLC_REGION_CAM_CSIS, llc_state);
		}

		if (mcfp) {
			llc_region_alloc(LLC_REGION_CAM_MCFP, 1, mcfp);
			set_bit(LLC_REGION_CAM_MCFP, llc_state);
		}

		if (icpu) {
			llc_region_alloc(LLC_REGION_ICPU, 1, icpu);
			set_bit(LLC_REGION_ICPU, llc_state);
		}

		info("[LLC] alloc, VOTF:%d.%dMB, MCFP:%d.%dMB, ICPU:%d.%dMB",
				votf / 2, (votf % 2) ? 5 : 0,
				mcfp / 2, (mcfp % 2) ? 5 : 0,
				icpu / 2, (icpu % 2) ? 5 : 0);
	} else {
		if (test_and_clear_bit(LLC_REGION_CAM_CSIS, llc_state))
			llc_region_alloc(LLC_REGION_CAM_CSIS, 0, 0);
		if (test_and_clear_bit(LLC_REGION_CAM_MCFP, llc_state))
			llc_region_alloc(LLC_REGION_CAM_MCFP, 0, 0);
		if (test_and_clear_bit(LLC_REGION_ICPU, llc_state))
			llc_region_alloc(LLC_REGION_ICPU, 0, 0);

		info("[LLC] release");
	}

	llc_enable(on);
#endif
}
KUNIT_EXPORT_SYMBOL(is_hw_configure_llc);

void is_hw_configure_bts_scen(struct is_resourcemgr *resourcemgr, int scenario_id)
{
	int bts_index = 0;

	switch (scenario_id) {
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_8K24:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_8K24_HF:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_8K30:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_8K30_HF:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD120:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD240:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD480:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD120:
	case IS_DVFS_SN_REAR_SINGLE_SSM:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD30_RECURSIVE:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD30_RECURSIVE:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD120:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD120:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD30_RECURSIVE:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60_SUPERSTEADY:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD30_RECURSIVE:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_DUAL_PHOTO:
	case IS_DVFS_SN_REAR_DUAL_CAPTURE:
	case IS_DVFS_SN_REAR_DUAL_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_DUAL_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_DUAL_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_DUAL_VIDEO_UHD60:
	case IS_DVFS_SN_PIP_DUAL_PHOTO:
	case IS_DVFS_SN_PIP_DUAL_CAPTURE:
	case IS_DVFS_SN_PIP_DUAL_VIDEO_FHD30:
	case IS_DVFS_SN_PIP_DUAL_VIDEO_UHD30:
	case IS_DVFS_SN_TRIPLE_PHOTO:
	case IS_DVFS_SN_TRIPLE_VIDEO_FHD30:
	case IS_DVFS_SN_TRIPLE_VIDEO_UHD30:
	case IS_DVFS_SN_TRIPLE_VIDEO_FHD60:
	case IS_DVFS_SN_TRIPLE_VIDEO_UHD60:
	case IS_DVFS_SN_TRIPLE_CAPTURE:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD60:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD60:
		bts_index = 1;
		break;
	default:
		bts_index = 0;
		break;
	}

	/* If default scenario & specific scenario selected,
	 * off specific scenario first.
	 */
	if (resourcemgr->cur_bts_scen_idx && bts_index == 0)
		is_bts_scen(resourcemgr, resourcemgr->cur_bts_scen_idx, false);

	if (bts_index && bts_index != resourcemgr->cur_bts_scen_idx)
		is_bts_scen(resourcemgr, bts_index, true);
	resourcemgr->cur_bts_scen_idx = bts_index;
}

int is_hw_get_capture_slot(struct is_frame *frame, dma_addr_t **taddr, u64 **taddr_k, u32 vid)
{
	int ret = 0;
	dma_addr_t *p = NULL;
	u64 *kp = NULL;

	if (taddr)
		*taddr = NULL;
	if (taddr_k)
		*taddr_k = NULL;

	switch (vid) {
	/* BYRP */
	case IS_LVN_BYRP_BYR:
		p = frame->dva_byrp_byr;
		break;
	case IS_LVN_BYRP_HDR:
		p = frame->dva_byrp_hdr;
		break;
	/* RGBP */
	/* YUVP */
	case IS_LVN_YUVP_DRC:
		p = frame->ypdgaTargetAddress;
		break;
	case IS_LVN_YUVP_CLAHE:
		p = frame->ypclaheTargetAddress;
		break;
	case IS_LVN_YUVP_SEG:
		p = frame->ixscmapTargetAddress;
		break;
	/* MCSC */
	case IS_LVN_MCSC_P0:
		p = frame->sc0TargetAddress;
		break;
	case IS_LVN_MCSC_P1:
		p = frame->sc1TargetAddress;
		break;
	case IS_LVN_MCSC_P2:
		p = frame->sc2TargetAddress;
		break;
	case IS_LVN_MCSC_P3:
		p = frame->sc3TargetAddress;
		break;
	case IS_LVN_MCSC_P4:
		p = frame->sc4TargetAddress;
		break;
	case IS_LVN_MCSC_P5:
		p = frame->sc5TargetAddress;
		break;
	case IS_LVN_YUVP_CR:
		kp = frame->kva_yuvp_rta_info;
		break;
	default:
		err_hw("Unsupported vid(%d)", vid);
		ret = -EINVAL;
	}

	if (taddr)
		*taddr = p;
	if (taddr_k)
		*taddr_k = kp;

	/* Clear subdev frame's target address before set */
	if (taddr && *taddr)
		memset(*taddr, 0x0, sizeof(typeof(**taddr)) * IS_MAX_PLANES);
	if (taddr_k && *taddr_k)
		memset(*taddr_k, 0x0, sizeof(typeof(**taddr_k)) * IS_MAX_PLANES);

	return ret;
}

void is_hw_fill_target_address(u32 hw_id, struct is_frame *dst, struct is_frame *src,
	bool reset)
{
	/* A previous address should not be cleared for sysmmu debugging. */
	reset = false;

	switch (hw_id) {
	case DEV_HW_PAF0:
	case DEV_HW_PAF1:
	case DEV_HW_PAF2:
	case DEV_HW_PAF3:
		break;
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
	case DEV_HW_3AA2:
	case DEV_HW_3AA3:
		TADDR_COPY(dst, src, dva_cstat_sat, reset);
		TADDR_COPY(dst, src, dva_cstat_cav, reset);
		TADDR_COPY(dst, src, dva_cstat_pre, reset);
		TADDR_COPY(dst, src, dva_cstat_awb, reset);
		TADDR_COPY(dst, src, efdTargetAddress, reset);
		TADDR_COPY(dst, src, txdgrTargetAddress, reset);
		TADDR_COPY(dst, src, txldsTargetAddress, reset);
		TADDR_COPY(dst, src, dva_cstat_lmeds1, reset);
		break;
	case DEV_HW_LME0:
		TADDR_COPY(dst, src, lmesTargetAddress, reset);
		TADDR_COPY(dst, src, lmeskTargetAddress, reset);
		TADDR_COPY(dst, src, lmecTargetAddress, reset);
		TADDR_COPY(dst, src, lmesadTargetAddress, reset);
		TADDR_COPY(dst, src, kva_lme_rta_info, reset);
		break;
	case DEV_HW_BYRP:
		TADDR_COPY(dst, src, dva_byrp_hdr, reset);
		TADDR_COPY(dst, src, dva_byrp_byr, reset);
		TADDR_COPY(dst, src, dva_byrp_byr_processed, reset);
		TADDR_COPY(dst, src, kva_byrp_rta_info, reset);
		break;
	case DEV_HW_RGBP:
		TADDR_COPY(dst, src, dva_rgbp_hf, reset);
		TADDR_COPY(dst, src, dva_rgbp_sf, reset);
		TADDR_COPY(dst, src, dva_rgbp_yuv, reset);
		TADDR_COPY(dst, src, dva_rgbp_rgb, reset);
		TADDR_COPY(dst, src, kva_rgbp_rta_info, reset);
		break;
	case DEV_HW_YPP:
		TADDR_COPY(dst, src, ixscmapTargetAddress, reset);

		TADDR_COPY(dst, src, ypnrdsTargetAddress, reset);
		TADDR_COPY(dst, src, ypdgaTargetAddress, reset);
		TADDR_COPY(dst, src, ypclaheTargetAddress, reset);
		TADDR_COPY(dst, src, ypsvhistTargetAddress, reset);
		TADDR_COPY(dst, src, kva_yuvp_rta_info, reset);
		break;
	case DEV_HW_SHRP:
		TADDR_COPY(dst, src, dva_shrp_hf, reset);
		TADDR_COPY(dst, src, dva_shrp_seg, reset);

		TADDR_COPY(dst, src, kva_shrp_rta_info, reset);
		break;
	case DEV_HW_MCFP:
		TADDR_COPY(dst, src, dva_mcfp_prev_yuv, reset);
		TADDR_COPY(dst, src, dva_mcfp_prev_wgt, reset);
		TADDR_COPY(dst, src, dva_mcfp_cur_wgt, reset);
		TADDR_COPY(dst, src, dva_mcfp_cur_drc, reset);
		TADDR_COPY(dst, src, dva_mcfp_prev_drc, reset);
		TADDR_COPY(dst, src, dva_mcfp_motion, reset);
		TADDR_COPY(dst, src, kva_mcfp_motion, reset);
		TADDR_COPY(dst, src, dva_mcfp_mv_mixer, reset);
		TADDR_COPY(dst, src, dva_mcfp_sat_flag, reset);
		TADDR_COPY(dst, src, dva_mcfp_wgt, reset);
		TADDR_COPY(dst, src, dva_mcfp_yuv, reset);
		TADDR_COPY(dst, src, kva_mcfp_rta_info, reset);
		break;
	case DEV_HW_DLFE:
		/* DLFE shares the RTA_INFO buffer of MCFP */
		break;
	case DEV_HW_MCSC0:
		TADDR_COPY(dst, src, sc0TargetAddress, reset);
		TADDR_COPY(dst, src, sc1TargetAddress, reset);
		TADDR_COPY(dst, src, sc2TargetAddress, reset);
		TADDR_COPY(dst, src, sc3TargetAddress, reset);
		TADDR_COPY(dst, src, sc4TargetAddress, reset);
		TADDR_COPY(dst, src, sc5TargetAddress, reset);
		break;
	default:
		err("[%d] Invalid hw id(%d)", src->instance, hw_id);
		break;
	}
}

struct is_mem *is_hw_get_iommu_mem(u32 vid)
{
	struct is_core *core = is_get_is_core();
	struct pablo_device_iommu_group *iommu_group;

	switch (vid) {
	case IS_VIDEO_MCSC:
	case IS_LVN_MCSC_P0:
	case IS_LVN_MCSC_P1:
	case IS_LVN_MCSC_P2:
	case IS_LVN_MCSC_P3:
	case IS_LVN_MCSC_P4:
	case IS_LVN_MCSC_P5:
		iommu_group = pablo_iommu_group_get(0);
		return &iommu_group->mem;
	default:
		return &core->resourcemgr.mem;
	}
}

void is_hw_print_target_dva(struct is_frame *leader_frame, u32 instance)
{
	u32 i, j;

	for (i = 0; i < IS_MAX_PLANES; i++) {
		for (j = 0; j < CSI_VIRTUAL_CH_MAX; j++)
			IS_PRINT_MULTI_TARGET_DVA(dva_ssvc);

		for (j = 0; j < CSTAT_CTX_NUM; j++) {
			IS_PRINT_MULTI_TARGET_DVA(dva_cstat_sat);
			IS_PRINT_MULTI_TARGET_DVA(dva_cstat_cav);
			IS_PRINT_MULTI_TARGET_DVA(dva_cstat_pre);
			IS_PRINT_MULTI_TARGET_DVA(dva_cstat_awb);
			IS_PRINT_MULTI_TARGET_DVA(efdTargetAddress);
			IS_PRINT_MULTI_TARGET_DVA(txdgrTargetAddress);
			IS_PRINT_MULTI_TARGET_DVA(txldsTargetAddress);
			IS_PRINT_MULTI_TARGET_DVA(dva_cstat_lmeds1);
		}

		IS_PRINT_TARGET_DVA(lmesTargetAddress);
		IS_PRINT_TARGET_DVA(lmecTargetAddress);
		IS_PRINT_TARGET_DVA(dva_byrp_hdr);
		IS_PRINT_TARGET_DVA(dva_byrp_byr);
		IS_PRINT_TARGET_DVA(dva_byrp_byr_processed);
		IS_PRINT_TARGET_DVA(sc0TargetAddress);
		IS_PRINT_TARGET_DVA(sc1TargetAddress);
		IS_PRINT_TARGET_DVA(sc2TargetAddress);
		IS_PRINT_TARGET_DVA(sc3TargetAddress);
		IS_PRINT_TARGET_DVA(sc4TargetAddress);
		IS_PRINT_TARGET_DVA(sc5TargetAddress);
	}
}

int is_hw_config(struct is_hw_ip *hw_ip, struct pablo_crta_buf_info *buf_info)
{
	int ret = 0;
	u32 fcount, instance;
	struct pablo_crta_frame_info *pcfi;

	instance = atomic_read(&hw_ip->instance);
	fcount = buf_info->fcount;

	/* set results of crta */
	pcfi = (struct pablo_crta_frame_info *)buf_info->kva;
	if (CHK_HW_3AA(hw_ip->id)) {
		ret = CALL_HWIP_OPS(hw_ip, set_config, 0, instance, fcount,
				    (void *)&pcfi->cstat_cfg);
		if (ret)
			mserr_hw("set_config error (%d)", instance, hw_ip, ret);

		ret = CALL_HWIP_OPS(hw_ip, set_regs, 0, instance, fcount,
				    (struct cr_set *)pcfi->cstat_cr, pcfi->cstat_cr_size);
		if (ret)
			mserr_hw("set_regs error (%d)", instance, hw_ip, ret);
	} else if (CHK_HW_PDP(hw_ip->id)) {
		ret = CALL_HWIP_OPS(hw_ip, set_regs, 0, instance, fcount,
				    (struct cr_set *)pcfi->pdp_cr, pcfi->pdp_cr_size);
		if (ret)
			mserr_hw("set_regs error (%d)", instance, hw_ip, ret);
	}

	return ret;
}

void is_hw_update_pcfi(struct is_hardware *hardware, struct is_group *group,
			struct is_frame *frame, struct pablo_crta_buf_info *pcfi_buf)
{
	struct is_hw_ip *hw_ip;
	struct pablo_crta_frame_info *pcfi = (struct pablo_crta_frame_info *)pcfi_buf->kva;
	struct is_group *child = NULL;
	int hw_maxnum = 0;
	int hw_list[GROUP_HW_MAX], hw_index;
	u32 instance;


	if (!pcfi) {
		merr_adt("pcfi kva is null", frame->instance);
		return;
	}

	instance = group->instance;

	child = group->tail;
	while (child && (child->device_type == IS_DEVICE_ISCHAIN)) {
		hw_maxnum = is_get_hw_list(child->id, hw_list);

		for (hw_index = hw_maxnum - 1; hw_index >= 0; hw_index--) {
			hw_ip = CALL_HW_CHAIN_INFO_OPS(hardware, get_hw_ip, hw_list[hw_index]);
			if (!hw_ip) {
				merr_hw("invalid id (%d)", instance, hw_list[hw_index]);
				return;
			}

			CALL_HWIP_OPS(hw_ip, query, instance, PABLO_QUERY_GET_PCFI,
					frame, &pcfi->prfi);
		}
		child = child->parent;
	}

	mdbg_adt(2, "[SENSOR] (%dx%d) -> binning (%dx%d) -> crop (%d,%d,%dx%d)", frame->instance,
		pcfi->prfi.sensor_calibration_size.width, pcfi->prfi.sensor_calibration_size.height,
		pcfi->prfi.sensor_binning.x, pcfi->prfi.sensor_binning.y,
		pcfi->prfi.sensor_crop.offset.x, pcfi->prfi.sensor_crop.offset.y,
		pcfi->prfi.sensor_crop.size.width, pcfi->prfi.sensor_crop.size.height);

	mdbg_adt(2, "[CSTAT] (%dx%d) -> dzoom (%d,%d,%dx%d) -> bns (%dx%d)", frame->instance,
		pcfi->prfi.cstat_input_size.width, pcfi->prfi.cstat_input_size.height,
		pcfi->prfi.cstat_crop_dzoom.offset.x, pcfi->prfi.cstat_crop_dzoom.offset.y,
		pcfi->prfi.cstat_crop_dzoom.size.width, pcfi->prfi.cstat_crop_dzoom.size.height,
		pcfi->prfi.cstat_bns_size.width, pcfi->prfi.cstat_bns_size.height);
}

static inline struct is_group *__group_get_child(struct is_group *grp, u32 id)
{
	struct is_group *pos;

	for_each_group_child(pos, grp->child)
		if (pos->id == id)
			return pos;

	return NULL;
}

void is_hw_check_iteration_state(struct is_frame *frame,
	struct is_dvfs_ctrl *dvfs_ctrl, struct is_group *group)
{
	struct is_dvfs_iteration_mode *iter_mode = dvfs_ctrl->iter_mode;
	struct is_dvfs_rnr_mode *rnr_mode = &dvfs_ctrl->rnr_mode;
	u32 iter = frame->shot_ext->node_group.leader.iterationType;
	u32 rnr = frame->shot_ext->node_group.leader.recursiveNrType;

	if (!__group_get_child(group, GROUP_ID_MTNR0))
		return;

	iter_mode->changed = 0;
	if (iter_mode->iter_svhist != iter) {
		mgrdbgs(1, "iteration state changed(%d -> %d)\n", group, group, frame,
			iter_mode->iter_svhist, iter);
		iter_mode->iter_svhist = iter;
		iter_mode->changed = 1;
	}

	rnr_mode->changed = 0;
	if (BOOL(rnr_mode->rnr) != BOOL(rnr)) {
		mgrdbgs(1, "recursiveNr state changed(%d -> %d)\n", group, group, frame,
			rnr_mode->rnr, rnr);
		rnr_mode->rnr = rnr;
		rnr_mode->changed = 1;
	}
}

#if IS_ENABLED(ENABLE_RECURSIVE_NR)
void is_hw_restore_group_state_2nr(struct is_group *group)
{
/*	struct is_group *pos = __group_get_child(group, GROUP_ID_MCFP);

	if (!pos)
		return;

	if (pos->input_type == GROUP_INPUT_OTF || pos->input_type == GROUP_INPUT_VOTF)
		set_bit(IS_GROUP_OTF_INPUT, &pos->state);
*/
}

void is_hw_update_group_state_2nr(struct is_group *group, struct is_frame *frame)
{
#ifdef HACK_TO_AVOID_BUILD_ERR
	if (!pos)
		return;

	is_hw_restore_group_state_2nr(group);

	/* In case of recursiveNR, MCFP input type is M2M */
	if (frame->shot_ext->node_group.leader.recursiveNrType == NODE_RECURSIVE_NR_TYPE_2ND)
		clear_bit(IS_GROUP_OTF_INPUT, &pos->state);
#endif
}
#endif
