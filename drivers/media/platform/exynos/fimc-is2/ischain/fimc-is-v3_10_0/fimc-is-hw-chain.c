/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd
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

#include <mach/map.h>
#include <mach/regs-clock.h>

#include "fimc-is-config.h"
#include "fimc-is-type.h"
#include "fimc-is-regs.h"
#include "fimc-is-groupmgr.h"
#include "fimc-is-device-sensor.h"
#include "fimc-is-device-ischain.h"
#include "fimc-is-core.h"

#include "../../interface/fimc-is-interface-ischain.h"
#include "../../hardware/fimc-is-hw-control.h"

int fimc_is_hw_group_cfg(void *group_data)
{
	int ret = 0;
	struct fimc_is_group *group;
	struct fimc_is_device_ischain *device;

	BUG_ON(!group_data);

	group = group_data;
	device = group->device;

	if (!device) {
		err("device is NULL");
		BUG();
	}

	switch (group->slot) {
	case GROUP_SLOT_3AA:
		group->subdev[ENTRY_3AA] = NULL;
		group->subdev[ENTRY_3AC] = &device->txc;
		group->subdev[ENTRY_3AP] = &device->txp;
		group->subdev[ENTRY_ISP] = NULL;
		group->subdev[ENTRY_IXC] = NULL;
		group->subdev[ENTRY_IXP] = NULL;
		group->subdev[ENTRY_DRC] = NULL;
		group->subdev[ENTRY_DIS] = NULL;
		group->subdev[ENTRY_ODC] = NULL;
		group->subdev[ENTRY_DNR] = NULL;
		group->subdev[ENTRY_SCC] = NULL;
		group->subdev[ENTRY_SCP] = NULL;
		group->subdev[ENTRY_VRA] = &device->vra;;
		break;
	case GROUP_SLOT_ISP:
		group->subdev[ENTRY_3AA] = NULL;
		group->subdev[ENTRY_3AC] = NULL;
		group->subdev[ENTRY_3AP] = NULL;
		group->subdev[ENTRY_ISP] = NULL;
		group->subdev[ENTRY_IXC] = &device->ixc;
		group->subdev[ENTRY_IXP] = &device->ixp;
		group->subdev[ENTRY_DRC] = NULL;
		group->subdev[ENTRY_DIS] = NULL;
		group->subdev[ENTRY_ODC] = NULL;
		group->subdev[ENTRY_DNR] = NULL;
		group->subdev[ENTRY_SCC] = NULL;
		group->subdev[ENTRY_SCP] = &device->scp;
		group->subdev[ENTRY_VRA] = NULL;
		break;
	case GROUP_SLOT_DIS:
		group->subdev[ENTRY_3AA] = NULL;
		group->subdev[ENTRY_3AC] = NULL;
		group->subdev[ENTRY_3AP] = NULL;
		group->subdev[ENTRY_ISP] = NULL;
		group->subdev[ENTRY_IXC] = NULL;
		group->subdev[ENTRY_IXP] = NULL;
		group->subdev[ENTRY_DRC] = NULL;
		group->subdev[ENTRY_DIS] = NULL;
		group->subdev[ENTRY_ODC] = NULL;
		group->subdev[ENTRY_DNR] = NULL;
		group->subdev[ENTRY_SCC] = NULL;
		group->subdev[ENTRY_SCP] = NULL;
		group->subdev[ENTRY_VRA] = NULL;
		break;
	default:
		probe_err("group slot(%d) is invalid", group->slot);
		BUG();
		break;
	}

	return ret;
}

int fimc_is_hw_group_open(void *group_data)
{
	return 0;
}

inline int fimc_is_hw_slot_id(int hw_id)
{
	int slot_id = -1;

	switch (hw_id) {
	case DEV_HW_3AA0:
		slot_id = 0;
		break;
	case DEV_HW_MCSC:
		slot_id = 1;
		break;
	case DEV_HW_FD:
		slot_id = 2;
		break;
	default:
		break;
	}

	return slot_id;
}

inline int fimc_is_intr_slot_id(int hw_id)
{
	int slot_id = -1;

	switch (hw_id) {
	case DEV_HW_3AA0:
		slot_id = 0;	/* interface_taaisp index */
		break;
	case DEV_HW_MCSC:
		slot_id = 0;	/* interface_subip index */
		break;
	case DEV_HW_FD:
		slot_id = 1;
		break;
	default:
		break;
	}

	return slot_id;
}

int fimc_is_hw_get_address(void *itfc_data, ulong core_regs,
		void *pdev_data, int hw_id, int intr_slot)
{
	int ret = 0;
	struct resource *mem_res;
	struct platform_device *pdev = NULL;
	struct fimc_is_interface_ischain *itfc = NULL;
	struct fimc_is_interface_subip *itf_subip = NULL;
	struct fimc_is_interface_taaisp *itf_isp = NULL;

	BUG_ON(!itfc_data);

	itfc = (struct fimc_is_interface_ischain *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	switch (hw_id) {
	case DEV_HW_3AA0:
		itf_isp = &itfc->taaisp[intr_slot];
		itf_isp->regs = (void *)core_regs;
		info("ISP setA VA address(0x%8p)\n", itf_isp->regs);

		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			ret = -EINVAL;
			goto p_err;
		}
		itf_isp->regs_b = ioremap_nocache(mem_res->start, resource_size(mem_res));
		info("ISP setB VA address(0x%8p)\n", itf_isp->regs_b);
		if (!itf_isp->regs_b) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case DEV_HW_MCSC:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			ret = -EINVAL;
			goto p_err;
		}

		itf_subip = &itfc->subip[intr_slot];
		itf_subip->regs = ioremap_nocache(mem_res->start, resource_size(mem_res));
		info("MC-SCALER VA address(0x%8p)\n", itf_subip->regs);
		if (!itf_subip->regs) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			ret = -EINVAL;
			goto p_err;
		}
		break;
	case DEV_HW_FD:
		mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		if (!mem_res) {
			dev_err(&pdev->dev, "Failed to get io memory region\n");
			ret = -EINVAL;
			goto p_err;
		}

		itf_subip = &itfc->subip[intr_slot];
		itf_subip->regs = ioremap_nocache(mem_res->start, resource_size(mem_res));
		info("FD VA address(0x%8p)\n", itf_subip->regs);
		if (!itf_subip->regs) {
			dev_err(&pdev->dev, "Failed to remap io region\n");
			ret = -EINVAL;
			goto p_err;
		}
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		BUG();
		break;
	}

p_err:
	return ret;
}

int fimc_is_hw_get_irq(void *itfc_data, void *pdev_data, int hw_id, int intr_slot)
{
	struct fimc_is_interface_ischain *itfc = NULL;
	struct fimc_is_interface_taaisp *itf_isp = NULL;
	struct fimc_is_interface_subip *itf_subip = NULL;
	struct platform_device *pdev = NULL;
	int ret = 0;
	u32 i = 0;

	BUG_ON(!itfc_data);

	dbg_itfc("%s: id(%d)\n", __func__, hw_id);

	itfc = (struct fimc_is_interface_ischain *)itfc_data;
	pdev = (struct platform_device *)pdev_data;

	switch (hw_id) {
	/* ISP get_irq number */
	case DEV_HW_3AA0:
		itf_isp = &itfc->taaisp[intr_slot];

		for (i = 0; i < INTR_3AAISP_MAX; i++){
			itf_isp->irq[i] = platform_get_irq(pdev, i);
			if (itf_isp->irq[i] < 0) {
				err("Failed to get irq isp%d-%d \n", itf_isp->id, i);
				ret = -EINVAL;
				goto p_err;
			}
		}
		break;
	/* MC-SCALER(1x1) get_irq number */
	case DEV_HW_MCSC:
		itf_subip = &itfc->subip[intr_slot];
		itf_subip->irq = platform_get_irq(pdev, 3);
		if (itf_subip->irq < 0) {
			err("Failed to get irq scaler \n");
			ret = -EINVAL;
			goto p_err;
		}
		break;
	/* FD get_irq number */
	case DEV_HW_FD:
		itf_subip = &itfc->subip[intr_slot];
		itf_subip->irq = platform_get_irq(pdev, 4);
		if (itf_subip->irq < 0) {
			err("Failed to get irq fd \n");
			ret = -EINVAL;
			goto p_err;
		}
		break;
	default:
		probe_err("hw_id(%d) is invalid", hw_id);
		BUG();
		break;
	}

p_err:
	return ret;
}
