// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 * Pablo v9.1 specific functions
 *
 * Copyright (c) 2020 Samsung Electronics Co., Ltd
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
#include "is-hw-settle-4nm-lpe.h"
#include "is-device-sensor.h"
#include "is-device-csi.h"
#include "is-device-ischain.h"
#include "is-hw.h"
#include "../../interface/is-interface-library.h"
#include "votf/pablo-votf.h"
#include "is-hw-dvfs.h"
#include "pablo-device-iommu-group.h"
#include "pablo-irq.h"

/* SYSREG register description */
static const struct is_reg sysreg_csis_regs[SYSREG_CSIS_REG_CNT] = {
	{0x0108, "MEMCLK"},
	{0x0180, "POWER_FAIL_DETECT"},
	{0x031C, "CSIS_EMA_STATUS"},
	{0x040C, "CSIS_PDP_VOTF"},
	{0x0410, "CSIS_PDP_SC_CON0"},
	{0x0414, "CSIS_PDP_SC_CON1"},
	{0x0418, "CSIS_PDP_SC_CON2"},
	{0x041C, "CSIS_PDP_SC_CON3"},
	{0x0420, "CSIS_PDP_SC_CON4"},
	{0x0424, "CSIS_PDP_SC_CON5"},
	{0x0428, "CSIS_PDP_SC_CON6"},
	{0x042C, "CSIS_PDP_SC_CON7"},
	{0x0430, "PDP_VC_CON0"},
	{0x0434, "PDP_VC_CON1"},
	{0x0438, "PDP_VC_CON2"},
	{0x043C, "PDP_VC_CON3"},
	{0x0480, "CSIS_PDP_SC_PDP0_IN_EN"},
	{0x0480, "CSIS_PDP_SC_PDP1_IN_EN"},
	{0x0480, "CSIS_PDP_SC_PDP2_IN_EN"},
	{0x0480, "CSIS_PDP_SC_PDP3_IN_EN"},
	{0x0490, "CSIS_PDP_SC_CON8"},
	{0x0494, "CSIS_PDP_BNS_MUX"},
	{0x04A0, "CSIS0_FRAME_ID_EN"},
	{0x04A4, "CSIS1_FRAME_ID_EN"},
	{0x04A8, "CSIS2_FRAME_ID_EN"},
	{0x04AC, "CSIS3_FRAME_ID_EN"},
	{0x04B0, "CSIS4_FRAME_ID_EN"},
	{0x04B4, "CSIS5_FRAME_ID_EN"},
	{0x04B8, "CSIS6_FRAME_ID_EN"},
	{0x0500, "MIPI_PHY_CON"},
};

static const struct is_field sysreg_csis_fields[SYSREG_CSIS_REG_FIELD_CNT] = {
	{"EN", 0, 1, RW, 0x1},
	{"PFD_AVDD085_MIPI_CPHY_STAT", 4, 1, RO, 0x0},
	{"PFD_AVDD085_MIPI_CPHY", 0, 1, RW, 0x0},
	{"SFR_ENABLE", 8, 1, RW, 0x0},
	{"WIDTH_DATA2REQ", 4, 2, RW, 0x3},
	{"EMA_BUSY", 0, 1, RO, 0x0},
	{"I_PDP3_VOTF_VVALID_EN", 3, 1, RW, 0x0},
	{"I_PDP2_VOTF_VVALID_EN", 2, 1, RW, 0x0},
	{"I_PDP1_VOTF_VVALID_EN", 1, 1, RW, 0x0},
	{"I_PDP0_VOTF_VVALID_EN", 0, 1, RW, 0x0},
	{"GLUEMUX_PDP_VAL", 0, 4, RW, 0x0},
	{"GLUEMUX_CSIS_DMA_OTF_SEL", 0, 6, RW, 0x0},
	{"MUX_IMG_VC_PDP", 16, 4, RW, 0x0},
	{"MUX_AF1_VC_PDP", 4, 4, RW, 0x2},
	{"MUX_AF0_VC_PDP", 0, 4, RW, 0x1},
	{"PDP_IN_CSIS6_EN", 6, 1, RW, 0x0},
	{"PDP_IN_CSIS5_EN", 5, 1, RW, 0x0},
	{"PDP_IN_CSIS4_EN", 4, 1, RW, 0x0},
	{"PDP_IN_CSIS3_EN", 3, 1, RW, 0x0},
	{"PDP_IN_CSIS2_EN", 2, 1, RW, 0x0},
	{"PDP_IN_CSIS1_EN", 1, 1, RW, 0x0},
	{"PDP_IN_CSIS0_EN", 0, 1, RW, 0x0},
	{"GLUEMUX_CSIS_BNS_SEL", 0, 2, RW, 0x0},
	{"FID_LOC_BYTE", 16, 16, RW, 0x1b},
	{"FID_LOC_LINE", 8, 2, RW, 0x0},
	{"FRAME_ID_EN_CSIS", 0, 1, RW, 0x0},
	{"MIPI_DPHY_CONFIG", 16, 1, RW, 0x0},
	{"MIPI_RESETN_DPHY_S1", 6, 1, RW, 0x0},
	{"MIPI_RESETN_DPHY_S", 5, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S4", 4, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S3", 3, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S2", 2, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S1", 1, 1, RW, 0x0},
	{"MIPI_RESETN_DCPHY_S", 0, 1, RW, 0x0},
};

static inline void __nocfi __is_isr_ddk(void *data, int handler_id)
{
	struct is_interface_hwip *itf_hw = NULL;
	struct hwip_intr_handler *intr_hw = NULL;

	itf_hw = (struct is_interface_hwip *)data;
	intr_hw = &itf_hw->handler[handler_id];

	if (intr_hw->valid) {
		is_fpsimd_get_isr();
		intr_hw->handler(intr_hw->id, intr_hw->ctx);
		is_fpsimd_put_isr();
	} else {
		err_itfc("[ID:%d](%d)- chain(%d) empty handler!!",
			itf_hw->id, handler_id, intr_hw->chain_id);
	}
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

/* CSTAT */
static irqreturn_t __is_isr1_cstat0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_cstat0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_cstat1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_cstat1(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_cstat2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_cstat2(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr1_cstat3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_cstat3(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* BYRP */
static irqreturn_t __is_isr1_byrp(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_byrp(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* RGBP */
static irqreturn_t __is_isr1_rgbp(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_rgbp(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* LME */
static irqreturn_t __is_isr1_lme(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

/* YUVP */
static irqreturn_t __is_isr1_yuvp(int irq, void *data)
{
	/* To handle host and ddk both, host isr handler is registerd as slot 5 */
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_yuvp(int irq, void *data)
{
	/* To handle host and ddk both, host isr handler is registerd as slot 5 */
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* MCFP */
static irqreturn_t __is_isr1_mcfp(int irq, void *data)
{
	/* To handle host and ddk both, host isr handler is registerd as slot 5 */
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mcfp(int irq, void *data)
{
	/* To handle host and ddk both, host isr handler is registerd as slot 5 */
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/* MCSC */
static irqreturn_t __is_isr1_mcs0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP1);
	return IRQ_HANDLED;
}

static irqreturn_t __is_isr2_mcs0(int irq, void *data)
{
	__is_isr_host(data, INTR_HWIP2);
	return IRQ_HANDLED;
}

/*
 * HW group related functions
 */
void __is_hw_group_init(struct is_group *group)
{
	int i;

	for (i = ENTRY_SENSOR; i < ENTRY_END; i++)
		group->subdev[i] = NULL;

	INIT_LIST_HEAD(&group->subdev_list);
}

int is_hw_group_cfg(void *group_data)
{
	struct is_group *group;
	struct is_device_sensor *sensor;
	u32 vc;

	FIMC_BUG(!group_data);

	group = (struct is_group *)group_data;
	__is_hw_group_init(group);

	if (group->slot == GROUP_SLOT_SENSOR) {
		sensor = group->sensor;
		if (!sensor) {
			err("device is NULL");
			BUG();
		}

		group->subdev[ENTRY_SENSOR] = &sensor->group_sensor.leader;

		list_add_tail(&sensor->group_sensor.leader.list, &group->subdev_list);

		for (vc = CSI_VIRTUAL_CH_0; vc < CSI_VIRTUAL_CH_MAX; vc++) {
			group->subdev[ENTRY_SSVC0 + vc] = &sensor->ssvc[vc];
			list_add_tail(&sensor->ssvc[vc].list, &group->subdev_list);
		}
	}

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
	case GROUP_ID_SS6:
	case GROUP_ID_SS7:
	case GROUP_ID_SS8:
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
	case GROUP_ID_MCFP:
	case GROUP_ID_YPP:
	case GROUP_ID_YUVP:
	case GROUP_ID_MCS0:
	case GROUP_ID_MCS1:
		leader->constraints_width = GROUP_MCFP_MAX_WIDTH;
		leader->constraints_height = GROUP_MCFP_MAX_HEIGHT;
		break;
	case GROUP_ID_LME0:
		leader->constraints_width = GROUP_LME_MAX_WIDTH;
		leader->constraints_height = GROUP_LME_MAX_HEIGHT;
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
	case GROUP_ID_LME0:
		hw_list[hw_index] = DEV_HW_LME0; hw_index++;
		break;
	case GROUP_ID_ISP0:
		hw_list[hw_index] = DEV_HW_ISP0; hw_index++;
		break;
	case GROUP_ID_YPP:
	case GROUP_ID_YUVP:
		hw_list[hw_index] = DEV_HW_YPP; hw_index++;
		break;
	case GROUP_ID_MCS0:
		hw_list[hw_index] = DEV_HW_MCSC0; hw_index++;
		break;
	/* FIXME: the person in charge of each H/W */
	case GROUP_ID_BYRP:
		hw_list[hw_index] = DEV_HW_BYRP; hw_index++;
		break;
	case GROUP_ID_RGBP:
		hw_list[hw_index] = DEV_HW_RGBP; hw_index++;
		break;
	case GROUP_ID_MCFP:
		hw_list[hw_index] = DEV_HW_MCFP; hw_index++;
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
	int idx;

	FIMC_BUG(!itfc_data);
	FIMC_BUG(!pdev_data);

	itf_hwip = (struct is_interface_hwip *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	itf_hwip->hw_ip->dump_for_each_reg = NULL;
	itf_hwip->hw_ip->dump_reg_list_size = 0;

	switch (hw_id) {
	case DEV_HW_3AA0:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT0", IORESOURCE_CSTAT0, REG_SETA, true);
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT_CMN", IORESOURCE_CSTAT0, REG_EXT1, false);
		break;
	case DEV_HW_3AA1:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT1", IORESOURCE_CSTAT1, REG_SETA, true);
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT_CMN", IORESOURCE_CSTAT0, REG_EXT1, false);
		break;
	case DEV_HW_3AA2:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT2", IORESOURCE_CSTAT2, REG_SETA, true);
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT_CMN", IORESOURCE_CSTAT0, REG_EXT1, false);
		break;
	case DEV_HW_3AA3:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT3", IORESOURCE_CSTAT3, REG_SETA, true);
		__is_hw_get_address(pdev, itf_hwip, hw_id, "CSTAT_CMN", IORESOURCE_CSTAT0, REG_EXT1, false);
		break;
	case DEV_HW_LME0:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "LME", IORESOURCE_LME, REG_SETA, false);

		/* FIXME: remove it */
		idx = 0;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx].start = 0x8000;
		itf_hwip->hw_ip->dump_region[REG_SETA][idx++].end = 0xFFFF;
		break;
	case DEV_HW_BYRP:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "BYRP", IORESOURCE_BYRP, REG_SETA, true);
		break;
	case DEV_HW_RGBP:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "RGBP", IORESOURCE_RGBP, REG_SETA, true);
		break;
	case DEV_HW_MCFP:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "MCFP", IORESOURCE_MCFP, REG_SETA, true);
		break;
	case DEV_HW_YPP:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "YUVP", IORESOURCE_YUVP, REG_SETA, true);
		break;
	case DEV_HW_MCSC0:
		__is_hw_get_address(pdev, itf_hwip, hw_id, "MCSC0", IORESOURCE_MCSC, REG_SETA, true);
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
	case DEV_HW_3AA0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 0);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa0-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 1);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa0-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_3AA1:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 2);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa1-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 3);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa1-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_3AA2:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 4);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa2-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 5);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa2-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_3AA3:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 6);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq 3aa3-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 7);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq 3aa3-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_LME0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 8);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq lme0-0\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_BYRP:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 9);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq byrp0-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 10);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq byrp0-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_RGBP:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 11);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq rgbp0-1\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 12);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq rgbp0-2\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_MCFP:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 13);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mcfp0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 14);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mcfp0-1\n");
			return -EINVAL;
		}
		break;
	case DEV_HW_YPP:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 15);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq yuvp-0");
			return -EINVAL;
		}
		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 16);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq yuvp-1");
			return -EINVAL;
		}
		break;
	case DEV_HW_MCSC0:
		itf_hwip->irq[INTR_HWIP1] = platform_get_irq(pdev, 17);
		if (itf_hwip->irq[INTR_HWIP1] < 0) {
			err("Failed to get irq mcsc0-0\n");
			return -EINVAL;
		}

		itf_hwip->irq[INTR_HWIP2] = platform_get_irq(pdev, 18);
		if (itf_hwip->irq[INTR_HWIP2] < 0) {
			err("Failed to get irq mcsc0-1\n");
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
	case DEV_HW_3AA0:
		ret = __is_hw_request_irq(itf_hwip, "cstat0-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_cstat0);
		ret = __is_hw_request_irq(itf_hwip, "cstat0-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_cstat0);
		break;
	case DEV_HW_3AA1:
		ret = __is_hw_request_irq(itf_hwip, "cstat1-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_cstat1);
		ret = __is_hw_request_irq(itf_hwip, "cstat1-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_cstat1);
		break;
	case DEV_HW_3AA2:
		ret = __is_hw_request_irq(itf_hwip, "cstat2-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_cstat2);
		ret = __is_hw_request_irq(itf_hwip, "cstat2-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_cstat2);
		break;
	case DEV_HW_3AA3:
		ret = __is_hw_request_irq(itf_hwip, "catat3-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_cstat3);
		ret = __is_hw_request_irq(itf_hwip, "cstat3-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_cstat3);
		break;
	case DEV_HW_LME0:
		ret = __is_hw_request_irq(itf_hwip, "lme-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_lme);
		break;
	case DEV_HW_BYRP:
		ret = __is_hw_request_irq(itf_hwip, "byrp-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_byrp);
		ret = __is_hw_request_irq(itf_hwip, "byrp-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_byrp);
		break;
	case DEV_HW_RGBP:
		ret = __is_hw_request_irq(itf_hwip, "rgbp-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_rgbp);
		ret = __is_hw_request_irq(itf_hwip, "rgbp-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_rgbp);
		break;
	case DEV_HW_YPP:
		ret = __is_hw_request_irq(itf_hwip, "yuvp-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_yuvp);
		ret = __is_hw_request_irq(itf_hwip, "yuvp-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_yuvp);
		break;
	case DEV_HW_MCFP:
		ret = __is_hw_request_irq(itf_hwip, "mcfp-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mcfp);
		ret = __is_hw_request_irq(itf_hwip, "mcfp-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mcfp);
		break;
	case DEV_HW_MCSC0:
		ret = __is_hw_request_irq(itf_hwip, "mcs-0", INTR_HWIP1,
				IRQF_TRIGGER_NONE, __is_isr1_mcs0);
		ret = __is_hw_request_irq(itf_hwip, "mcs-1", INTR_HWIP2,
				IRQF_TRIGGER_NONE, __is_isr2_mcs0);
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		return -EINVAL;
	}

	return ret;
}

int is_hw_s_ctrl(void *itfc_data, int hw_id, enum hw_s_ctrl_id id, void *val)
{
	return 0;
}

u32 is_hw_find_settle(u32 mipi_speed, u32 use_cphy)
{
	u32 align_mipi_speed;
	u32 find_mipi_speed;
	const u32 *settle_table;
	size_t max;
	int s, e, m;

	if (use_cphy) {
		settle_table = is_csi_settle_table_cphy;
		max = sizeof(is_csi_settle_table_cphy) / sizeof(u32);
	} else {
		settle_table = is_csi_settle_table;
		max = sizeof(is_csi_settle_table) / sizeof(u32);
	}
	align_mipi_speed = ALIGN(mipi_speed, 10);

	s = 0;
	e = max - 2;

	if (settle_table[s] < align_mipi_speed)
		return settle_table[s + 1];

	if (settle_table[e] > align_mipi_speed)
		return settle_table[e + 1];

	/* Binary search */
	while (s <= e) {
		m = ALIGN((s + e) / 2, 2);
		find_mipi_speed = settle_table[m];

		if (find_mipi_speed == align_mipi_speed)
			break;
		else if (find_mipi_speed > align_mipi_speed)
			s = m + 2;
		else
			e = m - 2;
	}

	return settle_table[m + 1];
}

void is_hw_camif_init(void)
{
}

#if (IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON))
void is_hw_s2mpu_cfg(void)
{
	void __iomem *reg;

	pr_info("[DBG] S2MPU disable\n");

	/* CSIS: SMMU_D0_CSIS_S2 */
	reg = ioremap(0x17180000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* CSIS: SMMU_D1_CSIS_S2 */
	reg = ioremap(0x171B0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* CSIS: SMMU_D2_CSIS_S2 */
	reg = ioremap(0x171E0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* CSTAT: SYSMMU_D_CSTAT_S2 */
	reg = ioremap(0x18560000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* BYRP: SYSMMU_D0_BRP_S2 */
	reg = ioremap(0x17450000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* RGBP: SYSMMU_D1_BRP_S2 */
	reg = ioremap(0x17550000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* RGBP: SYSMMU_D2_BRP_S2 */
	reg = ioremap(0x17590000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* MCFP: SYSMMU_D0_MCSC_S2 */
	reg = ioremap(0x15C90000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* MCFP: SYSMMU_D1_MCSC_S2 */
	reg = ioremap(0x15CC0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* MCFP: SYSMMU_D2_MCSC_S2 */
	reg = ioremap(0x15CF0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* MCFP: SYSMMU_D3_MCSC_S2 */
	reg = ioremap(0x15D20000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* MCFP: SYSMMU_D4_MCSC_S2 */
	reg = ioremap(0x15D50000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* YUVP: SYSMMU_D0_YUVP_S2 */
	reg = ioremap(0x18090000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* YUVP: SYSMMU_D1_YUVP_S2 */
	reg = ioremap(0x180C0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* MCSC: SYSMMU_D0_MCSC_S2 */
	reg = ioremap(0x15C90000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* MCSC: SYSMMU_D1_MCSC_S2 */
	reg = ioremap(0x15CC0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* MCSC: SYSMMU_D2_MCSC_S2 */
	reg = ioremap(0x15CF0000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* MCSC: SYSMMU_D3_MCSC_S2 */
	reg = ioremap(0x15D20000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* MCSC: SYSMMU_D4_MCSC_S2 */
	reg = ioremap(0x15D50000, 0x4);
	writel(0, reg);
	iounmap(reg);

	/* LME: SYSMMU_D_LME_S2 */
	reg = ioremap(0x176C0000, 0x4);
	writel(0, reg);
	iounmap(reg);
}
PST_EXPORT_SYMBOL(is_hw_s2mpu_cfg);
#endif

int is_hw_camif_cfg(void *sensor_data)
{
	struct is_device_sensor *sensor;
	struct is_device_csi *csi;
	struct is_fid_loc *fid_loc;
	void __iomem *sysreg_csis;
	u32 val, csi_ch;

	FIMC_BUG(!sensor_data);

	sensor = sensor_data;
	csi = (struct is_device_csi *)v4l2_get_subdevdata(sensor->subdev_csi);
	fid_loc = &sensor->cfg->fid_loc;
	sysreg_csis = ioremap(SYSREG_CSIS_BASE_ADDR, 0x1000);
	csi_ch = csi->otf_info.csi_ch;

	if (csi->f_id_dec) {
		if (!fid_loc->valid) {
			fid_loc->byte = 27;
			fid_loc->line = 0;
			mwarn("fid_loc is NOT properly set.\n", sensor);
		}

		val = is_hw_get_reg(sysreg_csis,
				&sysreg_csis_regs[SYSREG_R_CSIS_CSIS0_FRAME_ID_EN + csi_ch]);
		val = is_hw_set_field_value(val,
				&sysreg_csis_fields[SYSREG_F_CSIS_FID_LOC_BYTE],
				fid_loc->byte);
		val = is_hw_set_field_value(val,
				&sysreg_csis_fields[SYSREG_F_CSIS_FID_LOC_LINE],
				fid_loc->line);
		val = is_hw_set_field_value(val,
				&sysreg_csis_fields[SYSREG_F_CSIS_FRAME_ID_EN_CSIS],
				1);

		info("SYSREG_R_CSIS%d_FRAME_ID_EN:byte %d line %d\n",
				csi_ch, fid_loc->byte, fid_loc->line);
	} else {
		val = 0;
	}

	is_hw_set_reg(sysreg_csis,
		&sysreg_csis_regs[SYSREG_R_CSIS_CSIS0_FRAME_ID_EN + csi_ch],
		val);

	iounmap(sysreg_csis);

#if (IS_ENABLED(CONFIG_ARCH_VELOCE_HYCON))
	is_hw_s2mpu_cfg();
#endif

	return 0;
}

void is_hw_ischain_qe_cfg(void)
{
	dbg_hw(2, "%s()\n", __func__);
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

	itfc = &core->interface_ischain;
	hw = &core->hardware;

	/* irq affinity should be restored after S2R at gic600 */
	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_3AA0);
	itf_hwip = &(itfc->itf_ip[hw_slot]);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP3], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP4], true);

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_3AA1);
	itf_hwip = &(itfc->itf_ip[hw_slot]);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP3], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP4], true);

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_3AA2);
	itf_hwip = &(itfc->itf_ip[hw_slot]);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP3], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP4], true);

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_3AA3);
	itf_hwip = &(itfc->itf_ip[hw_slot]);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP3], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP4], true);

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_LME0);
	itf_hwip = &(itfc->itf_ip[hw_slot]);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_BYRP);
	itf_hwip = &(itfc->itf_ip[hw_slot]);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_RGBP);
	itf_hwip = &(itfc->itf_ip[hw_slot]);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_YPP);
	itf_hwip = &(itfc->itf_ip[hw_slot]);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_MCFP);
	itf_hwip = &(itfc->itf_ip[hw_slot]);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP2], true);

	hw_slot = CALL_HW_CHAIN_INFO_OPS(hw, get_hw_slot_id, DEV_HW_MCSC0);
	itf_hwip = &(itfc->itf_ip[hw_slot]);
	pablo_set_affinity_irq(itf_hwip->irq[INTR_HWIP1], true);

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
		0x17030004, 0x17040004, 0x17050004, 0x17060004,
		0x17070004, 0x17080004, 0x17090004,
	};
	void __iomem *base;
	u32 val;
	int i;

	for (i = 0; i < NUM_OF_CSIS; i++) {
		base = ioremap(csis_cmn_ctrls[i], SZ_4);
		val = readl(base);
		writel(val | (1 << 20), base);
		iounmap(base);
	}
}
#endif

void is_hw_interrupt_relay(struct is_group *group, void *hw_ip_data)
{
}

#if IS_ENABLED(CONFIG_EXYNOS_SCI)
static struct is_llc_way_num is_llc_way[IS_LLC_SN_END] = {
	/* default : VOTF 0MB, MCFP 0MB */
	[IS_LLC_SN_DEFAULT].votf = 0,
	[IS_LLC_SN_DEFAULT].mcfp = 0,
	/* FHD scenario : VOTF 0MB, MCFP 1MB */
	[IS_LLC_SN_FHD].votf = 0,
	[IS_LLC_SN_FHD].mcfp = 2,
	/* UHD scenario : VOTF 1MB, MCFP 2MB */
	[IS_LLC_SN_UHD].votf = 2,
	[IS_LLC_SN_UHD].mcfp = 4,
	/* 8K scenario : VOTF 2MB, MCFP 2.5MB */
	[IS_LLC_SN_8K].votf = 4,
	[IS_LLC_SN_8K].mcfp = 5,
	/* preview scenario : VOTF 0MB, MCFP 1.5MB */
	[IS_LLC_SN_PREVIEW].votf = 0,
	[IS_LLC_SN_PREVIEW].mcfp = 3,
};
#endif

void is_hw_configure_llc(bool on, struct is_device_ischain *device, ulong *llc_state)
{
#if IS_ENABLED(CONFIG_EXYNOS_SCI)
	struct is_dvfs_scenario_param param;
	u32 votf, mcfp, size;

	/* way 1 means alloc 512K LLC */
	if (on) {
		is_hw_dvfs_get_scenario_param(device, 0, &param);

		if (param.mode == IS_DVFS_MODE_PHOTO) {
			votf = is_llc_way[IS_LLC_SN_PREVIEW].votf;
			mcfp = is_llc_way[IS_LLC_SN_PREVIEW].mcfp;
		} else if (param.mode == IS_DVFS_MODE_VIDEO) {
			switch (param.resol) {
			case IS_DVFS_RESOL_FHD:
				votf = is_llc_way[IS_LLC_SN_FHD].votf;
				mcfp = is_llc_way[IS_LLC_SN_FHD].mcfp;
				break;
			case IS_DVFS_RESOL_UHD:
				votf = is_llc_way[IS_LLC_SN_UHD].votf;
				mcfp = is_llc_way[IS_LLC_SN_UHD].mcfp;
				break;
			case IS_DVFS_RESOL_8K:
				votf = is_llc_way[IS_LLC_SN_8K].votf;
				mcfp = is_llc_way[IS_LLC_SN_8K].mcfp;
				break;
			default:
				votf = is_llc_way[IS_LLC_SN_DEFAULT].votf;
				mcfp = is_llc_way[IS_LLC_SN_DEFAULT].mcfp;
				break;
			}
		} else {
			votf = is_llc_way[IS_LLC_SN_DEFAULT].votf;
			mcfp = is_llc_way[IS_LLC_SN_DEFAULT].mcfp;
		}

		size = is_get_debug_param(IS_DEBUG_PARAM_LLC);
		if (size) {
			votf = size / 100;
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

		info("[LLC] alloc, VOTF:%d.%dMB, MCFP:%d.%dMB",
				votf / 2, (votf % 2) ? 5 : 0,
				mcfp / 2, (mcfp % 2) ? 5 : 0);
	} else {
		if (test_and_clear_bit(LLC_REGION_CAM_CSIS, llc_state))
			llc_region_alloc(LLC_REGION_CAM_CSIS, 0, 0);
		if (test_and_clear_bit(LLC_REGION_CAM_MCFP, llc_state))
			llc_region_alloc(LLC_REGION_CAM_MCFP, 0, 0);

		info("[LLC] release");
	}

	llc_enable(on);
#endif
}

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
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_FHD120:
	case IS_DVFS_SN_FRONT_SINGLE_VIDEO_UHD120:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60_HF:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60_SUPERSTEADY:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_FHD60_HF_SUPERSTEADY:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_SINGLE_VIDEO_UHD60_HF:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_PHOTO:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_CAPTURE:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_TELE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_PHOTO:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_CAPTURE:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD30:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_FHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_ULTRAWIDE_VIDEO_UHD60:
	case IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_PHOTO:
	case IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_CAPTURE:
	case IS_DVFS_SN_REAR_DUAL_WIDE_MACRO_VIDEO_FHD30:
	case IS_DVFS_SN_PIP_DUAL_PHOTO:
	case IS_DVFS_SN_PIP_DUAL_CAPTURE:
	case IS_DVFS_SN_PIP_DUAL_VIDEO_FHD30:
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

int is_hw_get_output_slot(u32 vid)
{
	int ret = -1;

	switch (vid) {
	case IS_VIDEO_SS0_NUM:
	case IS_VIDEO_SS1_NUM:
	case IS_VIDEO_SS2_NUM:
	case IS_VIDEO_SS3_NUM:
	case IS_VIDEO_SS4_NUM:
	case IS_VIDEO_SS5_NUM:
	case IS_VIDEO_PAF0S_NUM:
	case IS_VIDEO_PAF1S_NUM:
	case IS_VIDEO_PAF2S_NUM:
	case IS_VIDEO_PAF3S_NUM:
	case IS_VIDEO_30S_NUM:
	case IS_VIDEO_31S_NUM:
	case IS_VIDEO_32S_NUM:
	case IS_VIDEO_33S_NUM:
	case IS_VIDEO_I0S_NUM:
	case IS_VIDEO_YPP_NUM:
	case IS_VIDEO_MCFP:
	case IS_VIDEO_LME:
		ret = 0;
		break;
	default:
		ret = -1;
	}

	return ret;
}

int is_hw_get_capture_slot(struct is_frame *frame, dma_addr_t **taddr, u64 **taddr_k, u32 vid)
{
	int ret = 0;

	switch(vid) {
	case IS_LVN_CSTAT0_LME_DS0:
	case IS_LVN_CSTAT1_LME_DS0:
	case IS_LVN_CSTAT2_LME_DS0:
	case IS_LVN_CSTAT3_LME_DS0:
		*taddr = frame->txldsTargetAddress[frame->cstat_ctx];
		break;
	case IS_LVN_CSTAT0_LME_DS1:
	case IS_LVN_CSTAT1_LME_DS1:
	case IS_LVN_CSTAT2_LME_DS1:
	case IS_LVN_CSTAT3_LME_DS1:
		*taddr = frame->dva_cstat_lmeds1[frame->cstat_ctx];
		break;
	case IS_LVN_CSTAT0_SVHIST:
	case IS_LVN_CSTAT1_SVHIST:
	case IS_LVN_CSTAT2_SVHIST:
	case IS_LVN_CSTAT3_SVHIST:
		*taddr = frame->dva_cstat_vhist[frame->cstat_ctx];
		break;
	case IS_LVN_CSTAT0_FDPIG:
	case IS_LVN_CSTAT1_FDPIG:
	case IS_LVN_CSTAT2_FDPIG:
	case IS_LVN_CSTAT3_FDPIG:
		*taddr = frame->efdTargetAddress[frame->cstat_ctx];
		break;
	case IS_LVN_CSTAT0_DRC:
	case IS_LVN_CSTAT1_DRC:
	case IS_LVN_CSTAT2_DRC:
	case IS_LVN_CSTAT3_DRC:
		*taddr = frame->txdgrTargetAddress[frame->cstat_ctx];
		break;
	case IS_LVN_CSTAT0_CDS:
	case IS_LVN_CSTAT1_CDS:
	case IS_LVN_CSTAT2_CDS:
	case IS_LVN_CSTAT3_CDS:
		*taddr = frame->txpTargetAddress[frame->cstat_ctx];
		break;
	case IS_LVN_BYRP_BYR:
		*taddr = frame->ixcTargetAddress;
		break;
	case IS_LVN_BYRP_HDR:
		*taddr = frame->dva_byrp_hdr;
		break;
	/* RGBP */
	case IS_LVN_RGBP_HF:
		*taddr = frame->dva_rgbp_hf;
		break;
	case IS_LVN_RGBP_SF:
		*taddr = frame->dva_rgbp_sf;
		break;
	case IS_LVN_RGBP_YUV:
		*taddr = frame->dva_rgbp_yuv;
		break;
	case IS_LVN_RGBP_RGB:
		*taddr = frame->dva_rgbp_rgb;
		break;
	case IS_VIDEO_I0C_NUM:
		*taddr = frame->ixcTargetAddress;
		break;
	case IS_VIDEO_ISC_NUM:
		*taddr = frame->ixscmapTargetAddress;
		break;
	/* YUVPP */
	case IS_LVN_YUVP_YUV:
		*taddr = frame->ypnrdsTargetAddress;
		break;
	case IS_LVN_YUVP_DRC:
		*taddr = frame->ypdgaTargetAddress;
		break;
	case IS_LVN_YUVP_CLAHE:
		*taddr = frame->ypclaheTargetAddress;
		break;
	case IS_LVN_YUVP_SEG:
		*taddr = frame->ixscmapTargetAddress;
		break;
	/* MCSC */
	case IS_LVN_MCSC_P0:
		*taddr = frame->sc0TargetAddress;
		break;
	case IS_LVN_MCSC_P1:
		*taddr = frame->sc1TargetAddress;
		break;
	case IS_LVN_MCSC_P2:
		*taddr = frame->sc2TargetAddress;
		break;
	case IS_LVN_MCSC_P3:
		*taddr = frame->sc3TargetAddress;
		break;
	case IS_LVN_MCSC_P4:
		*taddr = frame->sc4TargetAddress;
		break;
	case IS_LVN_MCSC_P5:
		*taddr = frame->sc5TargetAddress;
		break;
	/* LME */
	case IS_LVN_LME_PREV:
		*taddr = frame->lmesTargetAddress;
		break;
	case IS_LVN_LME_PURE:
		*taddr = frame->lmecTargetAddress;
		if (taddr_k)
			*taddr_k = frame->lmecKTargetAddress;
		break;
	case IS_LVN_LME_PROCESSED:
		/* No DMA out */
		if (taddr_k)
			*taddr_k = frame->lmemKTargetAddress;
		break;
	case IS_LVN_LME_SAD:
		*taddr = frame->lmesadTargetAddress;
		break;
	case IS_LVN_LME_PROCESSED_HDR:
		/* No DMA out */
		break;
	/* MCFP */
	case IS_LVN_MCFP_PREV_YUV:
		*taddr = frame->dva_mcfp_prev_yuv;
		break;
	case IS_LVN_MCFP_PREV_W:
		*taddr = frame->dva_mcfp_prev_wgt;
		break;
	case IS_LVN_MCFP_DRC:
		*taddr = frame->dva_mcfp_cur_drc;
		break;
	case IS_LVN_MCFP_DP:
		*taddr = frame->dva_mcfp_prev_drc;
		break;
	case IS_LVN_MCFP_MV:
		*taddr = frame->dva_mcfp_motion;
		if (taddr_k)
			*taddr_k = frame->kva_mcfp_motion;
		break;
	case IS_LVN_MCFP_SF:
		*taddr = frame->dva_mcfp_sat_flag;
		break;
	case IS_LVN_MCFP_W:
		*taddr = frame->dva_mcfp_wgt;
		break;
	case IS_LVN_MCFP_YUV:
		*taddr = frame->dva_mcfp_yuv;
		break;
	case IS_LVN_BYRP_RTA_INFO:
		*taddr_k = frame->kva_byrp_rta_info;
		break;
	case IS_LVN_RGBP_RTA_INFO:
		*taddr_k = frame->kva_rgbp_rta_info;
		break;
	case IS_LVN_MCFP_RTA_INFO:
		*taddr_k = frame->kva_mcfp_rta_info;
		break;
	case IS_LVN_YUVP_RTA_INFO:
		*taddr_k = frame->kva_yuvp_rta_info;
		break;
	case IS_LVN_LME_RTA_INFO:
		*taddr_k = frame->kva_lme_rta_info;
		break;
	default:
		err_hw("Unsupported vid(%d)", vid);
		ret = -EINVAL;
	}

	/* Clear subdev frame's target address before set */
	if (taddr && *taddr)
		memset(*taddr, 0x0, sizeof(typeof(**taddr)) * IS_MAX_PLANES);
	if (taddr_k && *taddr_k)
		memset(*taddr_k, 0x0, sizeof(typeof(**taddr_k)) * IS_MAX_PLANES);

	return ret;
}

void * is_get_dma_blk(int type)
{
	struct is_lib_support *lib = is_get_lib_support();
	struct lib_mem_block * mblk = NULL;

	switch (type) {
	case ID_DMA_3AAISP:
		mblk = &lib->mb_dma_taaisp;
		break;
	case ID_DMA_MEDRC:
		mblk = &lib->mb_dma_medrc;
		break;
	case ID_DMA_ORBMCH:
		mblk = &lib->mb_dma_orbmch;
		break;
	case ID_DMA_TNR:
		mblk = &lib->mb_dma_tnr;
		break;
	case ID_DMA_CLAHE:
		mblk = &lib->mb_dma_clahe;
		break;
	default:
		err_hw("Invalid DMA type: %d\n", type);
		return NULL;
	}

	return (void *)mblk;
}

int is_get_dma_id(u32 vid)
{
	return -EINVAL;
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
		break;
	case DEV_HW_3AA0:
	case DEV_HW_3AA1:
	case DEV_HW_3AA2:
	case DEV_HW_3AA3:
		TADDR_COPY(dst, src, txpTargetAddress, reset);
		TADDR_COPY(dst, src, efdTargetAddress, reset);
		TADDR_COPY(dst, src, txdgrTargetAddress, reset);
		TADDR_COPY(dst, src, txldsTargetAddress, reset);
		TADDR_COPY(dst, src, dva_cstat_lmeds1, reset);
		TADDR_COPY(dst, src, dva_cstat_vhist, reset);
		break;
	case DEV_HW_LME0:
		TADDR_COPY(dst, src, lmesTargetAddress, reset);
		TADDR_COPY(dst, src, lmecTargetAddress, reset);
		TADDR_COPY(dst, src, lmecKTargetAddress, reset);
		TADDR_COPY(dst, src, lmesadTargetAddress, reset);
		TADDR_COPY(dst, src, lmemKTargetAddress, reset);
		TADDR_COPY(dst, src, kva_lme_rta_info, reset);
		break;
	case DEV_HW_BYRP:
		TADDR_COPY(dst, src, dva_byrp_hdr, reset);
		TADDR_COPY(dst, src, ixcTargetAddress, reset);
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
		TADDR_COPY(dst, src, kva_yuvp_rta_info, reset);
		break;
	case DEV_HW_MCFP:
		TADDR_COPY(dst, src, dva_mcfp_prev_yuv, reset);
		TADDR_COPY(dst, src, dva_mcfp_prev_wgt, reset);
		TADDR_COPY(dst, src, dva_mcfp_cur_drc, reset);
		TADDR_COPY(dst, src, dva_mcfp_prev_drc, reset);
		TADDR_COPY(dst, src, dva_mcfp_motion, reset);
		TADDR_COPY(dst, src, kva_mcfp_motion, reset);
		TADDR_COPY(dst, src, dva_mcfp_sat_flag, reset);
		TADDR_COPY(dst, src, dva_mcfp_wgt, reset);
		TADDR_COPY(dst, src, dva_mcfp_yuv, reset);
		TADDR_COPY(dst, src, kva_mcfp_rta_info, reset);
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

static struct is_video i_video[PABLO_VIDEO_PROBE_MAX];
void is_hw_chain_probe(void *core)
{
	/* PDP */
	is_pafs_video_probe(&i_video[PABLO_VIDEO_PROBE_PAF0S], core, IS_VIDEO_PAF0S_NUM, 0);
	is_pafs_video_probe(&i_video[PABLO_VIDEO_PROBE_PAF1S], core, IS_VIDEO_PAF1S_NUM, 1);
	is_pafs_video_probe(&i_video[PABLO_VIDEO_PROBE_PAF2S], core, IS_VIDEO_PAF2S_NUM, 2);
	is_pafs_video_probe(&i_video[PABLO_VIDEO_PROBE_PAF3S], core, IS_VIDEO_PAF3S_NUM, 3);

	/* CSTAT */
	is_3as_video_probe(&i_video[PABLO_VIDEO_PROBE_30S], core, IS_VIDEO_30S_NUM, 0);
	is_3as_video_probe(&i_video[PABLO_VIDEO_PROBE_31S], core, IS_VIDEO_31S_NUM, 1);
	is_3as_video_probe(&i_video[PABLO_VIDEO_PROBE_32S], core, IS_VIDEO_32S_NUM, 2);
	is_3as_video_probe(&i_video[PABLO_VIDEO_PROBE_33S], core, IS_VIDEO_33S_NUM, 3);

	/* BYRP */
	is_byrp_video_probe(&i_video[PABLO_VIDEO_PROBE_BYRP], core, IS_VIDEO_BYRP, 0);

	/* RGBP */
	is_rgbp_video_probe(&i_video[PABLO_VIDEO_PROBE_RGBP], core, IS_VIDEO_RGBP, 0);

	/* MCFP */
	is_mcfp_video_probe(&i_video[PABLO_VIDEO_PROBE_MCFP], core, IS_VIDEO_MCFP, 0);

	/* YUVP */
	is_yuvp_video_probe(&i_video[PABLO_VIDEO_PROBE_YUVP], core, IS_VIDEO_YUVP, 0);

	/* MCSC */
	is_mcs_video_probe(&i_video[PABLO_VIDEO_PROBE_M0S], core, IS_VIDEO_M0S_NUM, 0);

	/* LME */
	is_lme_video_probe(&i_video[PABLO_VIDEO_PROBE_LME0], core, IS_VIDEO_LME0_NUM, 0);
}

struct is_mem *is_hw_get_iommu_mem(u32 vid)
{
	struct is_core *core = is_get_is_core();
	struct pablo_device_iommu_group *iommu_group;

	switch (vid) {
	case IS_VIDEO_SS0_NUM:
	case IS_VIDEO_SS1_NUM:
	case IS_VIDEO_SS2_NUM:
	case IS_VIDEO_SS3_NUM:
	case IS_VIDEO_SS4_NUM:
	case IS_VIDEO_SS5_NUM:
	case IS_VIDEO_SS6_NUM:
	case IS_VIDEO_SS7_NUM:
	case IS_VIDEO_SS8_NUM:
	case IS_VIDEO_SS0VC0_NUM:
	case IS_VIDEO_SS0VC1_NUM:
	case IS_VIDEO_SS0VC2_NUM:
	case IS_VIDEO_SS0VC3_NUM:
	case IS_VIDEO_SS1VC0_NUM:
	case IS_VIDEO_SS1VC1_NUM:
	case IS_VIDEO_SS1VC2_NUM:
	case IS_VIDEO_SS1VC3_NUM:
	case IS_VIDEO_SS2VC0_NUM:
	case IS_VIDEO_SS2VC1_NUM:
	case IS_VIDEO_SS2VC2_NUM:
	case IS_VIDEO_SS2VC3_NUM:
	case IS_VIDEO_SS3VC0_NUM:
	case IS_VIDEO_SS3VC1_NUM:
	case IS_VIDEO_SS3VC2_NUM:
	case IS_VIDEO_SS3VC3_NUM:
	case IS_VIDEO_SS4VC0_NUM:
	case IS_VIDEO_SS4VC1_NUM:
	case IS_VIDEO_SS4VC2_NUM:
	case IS_VIDEO_SS4VC3_NUM:
	case IS_VIDEO_SS5VC0_NUM:
	case IS_VIDEO_SS5VC1_NUM:
	case IS_VIDEO_SS5VC2_NUM:
	case IS_VIDEO_SS5VC3_NUM:
	case IS_VIDEO_SS6VC0_NUM:
	case IS_VIDEO_SS6VC1_NUM:
	case IS_VIDEO_SS6VC2_NUM:
	case IS_VIDEO_SS6VC3_NUM:
	case IS_LVN_SS0_VC0:
	case IS_LVN_SS0_VC1:
	case IS_LVN_SS0_VC2:
	case IS_LVN_SS0_VC3:
	case IS_LVN_SS0_VC4:
	case IS_LVN_SS0_VC5:
	case IS_LVN_SS0_VC6:
	case IS_LVN_SS0_VC7:
	case IS_LVN_SS0_VC8:
	case IS_LVN_SS0_VC9:
	case IS_LVN_SS0_MCB0:
	case IS_LVN_SS0_MCB1:
	case IS_LVN_SS0_BNS:
	case IS_LVN_SS1_VC0:
	case IS_LVN_SS1_VC1:
	case IS_LVN_SS1_VC2:
	case IS_LVN_SS1_VC3:
	case IS_LVN_SS1_VC4:
	case IS_LVN_SS1_VC5:
	case IS_LVN_SS1_VC6:
	case IS_LVN_SS1_VC7:
	case IS_LVN_SS1_VC8:
	case IS_LVN_SS1_VC9:
	case IS_LVN_SS1_MCB0:
	case IS_LVN_SS1_MCB1:
	case IS_LVN_SS1_BNS:
	case IS_LVN_SS2_VC0:
	case IS_LVN_SS2_VC1:
	case IS_LVN_SS2_VC2:
	case IS_LVN_SS2_VC3:
	case IS_LVN_SS2_VC4:
	case IS_LVN_SS2_VC5:
	case IS_LVN_SS2_VC6:
	case IS_LVN_SS2_VC7:
	case IS_LVN_SS2_VC8:
	case IS_LVN_SS2_VC9:
	case IS_LVN_SS2_MCB0:
	case IS_LVN_SS2_MCB1:
	case IS_LVN_SS2_BNS:
	case IS_LVN_SS3_VC0:
	case IS_LVN_SS3_VC1:
	case IS_LVN_SS3_VC2:
	case IS_LVN_SS3_VC3:
	case IS_LVN_SS3_VC4:
	case IS_LVN_SS3_VC5:
	case IS_LVN_SS3_VC6:
	case IS_LVN_SS3_VC7:
	case IS_LVN_SS3_VC8:
	case IS_LVN_SS3_VC9:
	case IS_LVN_SS3_MCB0:
	case IS_LVN_SS3_MCB1:
	case IS_LVN_SS3_BNS:
	case IS_LVN_SS4_VC0:
	case IS_LVN_SS4_VC1:
	case IS_LVN_SS4_VC2:
	case IS_LVN_SS4_VC3:
	case IS_LVN_SS4_VC4:
	case IS_LVN_SS4_VC5:
	case IS_LVN_SS4_VC6:
	case IS_LVN_SS4_VC7:
	case IS_LVN_SS4_VC8:
	case IS_LVN_SS4_VC9:
	case IS_LVN_SS4_MCB0:
	case IS_LVN_SS4_MCB1:
	case IS_LVN_SS4_BNS:
	case IS_LVN_SS5_VC0:
	case IS_LVN_SS5_VC1:
	case IS_LVN_SS5_VC2:
	case IS_LVN_SS5_VC3:
	case IS_LVN_SS5_VC4:
	case IS_LVN_SS5_VC5:
	case IS_LVN_SS5_VC6:
	case IS_LVN_SS5_VC7:
	case IS_LVN_SS5_VC8:
	case IS_LVN_SS5_VC9:
	case IS_LVN_SS5_MCB0:
	case IS_LVN_SS5_MCB1:
	case IS_LVN_SS5_BNS:
	case IS_LVN_SS6_VC0:
	case IS_LVN_SS6_VC1:
	case IS_LVN_SS6_VC2:
	case IS_LVN_SS6_VC3:
	case IS_LVN_SS6_VC4:
	case IS_LVN_SS6_VC5:
	case IS_LVN_SS6_VC6:
	case IS_LVN_SS6_VC7:
	case IS_LVN_SS6_VC8:
	case IS_LVN_SS6_VC9:
	case IS_LVN_SS6_MCB0:
	case IS_LVN_SS6_MCB1:
	case IS_LVN_SS6_BNS:
	case IS_LVN_SS7_VC0:
	case IS_LVN_SS7_VC1:
	case IS_LVN_SS7_VC2:
	case IS_LVN_SS7_VC3:
	case IS_LVN_SS7_VC4:
	case IS_LVN_SS7_VC5:
	case IS_LVN_SS7_VC6:
	case IS_LVN_SS7_VC7:
	case IS_LVN_SS7_VC8:
	case IS_LVN_SS7_VC9:
	case IS_LVN_SS7_MCB0:
	case IS_LVN_SS7_MCB1:
	case IS_LVN_SS7_BNS:
	case IS_LVN_SS8_VC0:
	case IS_LVN_SS8_VC1:
	case IS_LVN_SS8_VC2:
	case IS_LVN_SS8_VC3:
	case IS_LVN_SS8_VC4:
	case IS_LVN_SS8_VC5:
	case IS_LVN_SS8_VC6:
	case IS_LVN_SS8_VC7:
	case IS_LVN_SS8_VC8:
	case IS_LVN_SS8_VC9:
	case IS_LVN_SS8_MCB0:
	case IS_LVN_SS8_MCB1:
	case IS_LVN_SS8_BNS:
	case IS_VIDEO_PAF0S_NUM:
	case IS_VIDEO_PAF1S_NUM:
	case IS_VIDEO_PAF2S_NUM:
	case IS_VIDEO_PAF3S_NUM:
	case IS_VIDEO_CSTAT0:
	case IS_VIDEO_CSTAT1:
	case IS_VIDEO_CSTAT2:
	case IS_VIDEO_CSTAT3:
	case IS_LVN_CSTAT0_LME_DS0:
	case IS_LVN_CSTAT0_LME_DS1:
	case IS_LVN_CSTAT0_FDPIG:
	case IS_LVN_CSTAT0_RGBHIST:
	case IS_LVN_CSTAT0_SVHIST:
	case IS_LVN_CSTAT0_DRC:
	case IS_LVN_CSTAT0_CDS:
	case IS_LVN_CSTAT1_LME_DS0:
	case IS_LVN_CSTAT1_LME_DS1:
	case IS_LVN_CSTAT1_FDPIG:
	case IS_LVN_CSTAT1_RGBHIST:
	case IS_LVN_CSTAT1_SVHIST:
	case IS_LVN_CSTAT1_DRC:
	case IS_LVN_CSTAT1_CDS:
	case IS_LVN_CSTAT2_LME_DS0:
	case IS_LVN_CSTAT2_LME_DS1:
	case IS_LVN_CSTAT2_FDPIG:
	case IS_LVN_CSTAT2_RGBHIST:
	case IS_LVN_CSTAT2_SVHIST:
	case IS_LVN_CSTAT2_DRC:
	case IS_LVN_CSTAT2_CDS:
	case IS_LVN_CSTAT3_LME_DS0:
	case IS_LVN_CSTAT3_LME_DS1:
	case IS_LVN_CSTAT3_FDPIG:
	case IS_LVN_CSTAT3_RGBHIST:
	case IS_LVN_CSTAT3_SVHIST:
	case IS_LVN_CSTAT3_DRC:
	case IS_LVN_CSTAT3_CDS:
	case IS_VIDEO_BYRP:
	case IS_LVN_BYRP_HDR:
	case IS_LVN_BYRP_BYR:
	case IS_LVN_BYRP_RTA_INFO:
	case IS_LVN_RGBP_RTA_INFO:
	case IS_LVN_MCFP_RTA_INFO:
	case IS_LVN_YUVP_RTA_INFO:
	case IS_LVN_LME_RTA_INFO:
		return &core->resourcemgr.mem;
	case IS_VIDEO_LME:
	case IS_LVN_LME_PREV:
	case IS_LVN_LME_PURE:
	case IS_LVN_LME_PROCESSED:
	case IS_LVN_LME_SAD:
	case IS_LVN_LME_PROCESSED_HDR:
		iommu_group = pablo_iommu_group_get(1);
		return &iommu_group->mem;
	case IS_VIDEO_RGBP:
	case IS_LVN_RGBP_HF:
	case IS_LVN_RGBP_SF:
	case IS_LVN_RGBP_YUV:
	case IS_LVN_RGBP_RGB:
	case IS_VIDEO_MCSC0:
	case IS_LVN_MCSC_P0:
	case IS_LVN_MCSC_P1:
	case IS_LVN_MCSC_P2:
	case IS_LVN_MCSC_P3:
	case IS_LVN_MCSC_P4:
	case IS_LVN_MCSC_P5:
	case IS_VIDEO_MCFP:
	case IS_LVN_MCFP_PREV_YUV:
	case IS_LVN_MCFP_PREV_W:
	case IS_LVN_MCFP_DRC:
	case IS_LVN_MCFP_DP:
	case IS_LVN_MCFP_MV:
	case IS_LVN_MCFP_SF:
	case IS_LVN_MCFP_W:
	case IS_LVN_MCFP_YUV:
		iommu_group = pablo_iommu_group_get(2);
		return &iommu_group->mem;
	case IS_VIDEO_YUVP:
	case IS_LVN_YUVP_SEG:
	case IS_LVN_YUVP_DRC:
	case IS_LVN_YUVP_CLAHE:
	case IS_LVN_YUVP_YUV:
		iommu_group = pablo_iommu_group_get(3);
		return &iommu_group->mem;
	default:
		err("Invalid vid(%d)", vid);
		return NULL;
	}
}

void is_hw_print_target_dva(struct is_frame *leader_frame, u32 instance)
{
	u32 i;
	u32 ctx;

	for (i = 0; i < IS_MAX_PLANES; i++) {
		for (ctx = 0; ctx < CSTAT_CTX_NUM; ctx++)
			IS_PRINT_TARGET_DVA(txpTargetAddress[ctx]);

		for (ctx = 0; ctx < CSTAT_CTX_NUM; ctx++)
			IS_PRINT_TARGET_DVA(efdTargetAddress[ctx]);

		for (ctx = 0; ctx < CSTAT_CTX_NUM; ctx++)
			IS_PRINT_TARGET_DVA(txdgrTargetAddress[ctx]);

		for (ctx = 0; ctx < CSTAT_CTX_NUM; ctx++)
			IS_PRINT_TARGET_DVA(txldsTargetAddress[ctx]);

		for (ctx = 0; ctx < CSTAT_CTX_NUM; ctx++)
			IS_PRINT_TARGET_DVA(dva_cstat_lmeds1[ctx]);

		for (ctx = 0; ctx < CSTAT_CTX_NUM; ctx++)
			IS_PRINT_TARGET_DVA(dva_cstat_vhist[ctx]);

		IS_PRINT_TARGET_DVA(lmesTargetAddress);
		IS_PRINT_TARGET_DVA(lmecTargetAddress);
		IS_PRINT_TARGET_DVA(dva_byrp_hdr);
		IS_PRINT_TARGET_DVA(ixcTargetAddress);
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
	return 0;
}

void is_hw_update_pcfi(struct is_hardware *hardware, struct is_group *group,
			struct is_frame *frame, struct pablo_crta_buf_info *pcfi_buf)
{
}
