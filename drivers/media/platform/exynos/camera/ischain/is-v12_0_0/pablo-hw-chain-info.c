// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Exynos SoC series Pablo driver
 *
 * Exynos Pablo image subsystem functions
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "pablo-hw-chain-info.h"
#include "is-votf-id-table.h"
#include "votf/pablo-votf.h"
#include "is-core.h"
#include "pablo-device-iommu-group.h"

static void *get_rta_info_addr(u32 hw_id, struct is_frame *frame, u32 index)
{
	void *addr;

	switch (hw_id) {
	case DEV_HW_BYRP:
		addr = (void *)frame->kva_byrp_rta_info[index];
		break;
	case DEV_HW_RGBP:
		addr = (void *)frame->kva_rgbp_rta_info[index];
		break;
	case DEV_HW_MCFP:
		addr = (void *)frame->kva_mcfp_rta_info[index];
		break;
	case DEV_HW_YPP:
		addr = (void *)frame->kva_yuvp_rta_info[index];
		break;
	case DEV_HW_LME0:
		addr = (void *)frame->kva_lme_rta_info[index];
		break;
	default:
		err("Invalid hw_id(%d)", hw_id);
		addr = NULL;
		break;
	}

	return addr;
}

static struct size_cr_set *pablo_hw_chain_info_get_cr_set(struct is_hardware *hw, u32 hw_id,
							struct is_frame *frame)
{
	return get_rta_info_addr(hw_id, frame, PLANE_INDEX_CR_SET);
}

void *pablo_hw_chain_info_get_config(struct is_hardware *hw, u32 hw_id, struct is_frame *frame)
{
	return get_rta_info_addr(hw_id, frame, PLANE_INDEX_CONFIG);
}

static int pablo_hw_chain_info_get_hw_slot_id(struct is_hardware *hw, u32 hw_id)
{
	int slot_id = -1;

	switch (hw_id) {
	case DEV_HW_PAF0:
		slot_id = 0;
		break;
	case DEV_HW_PAF1:
		slot_id = 1;
		break;
	case DEV_HW_PAF2:
		slot_id = 2;
		break;
	case DEV_HW_PAF3:
		slot_id = 3;
		break;
	case DEV_HW_3AA0:
		slot_id = 4;
		break;
	case DEV_HW_3AA1:
		slot_id = 5;
		break;
	case DEV_HW_3AA2:
		slot_id = 6;
		break;
	case DEV_HW_3AA3:
		slot_id = 7;
		break;
	case DEV_HW_LME0:
		slot_id = 8;
		break;
	case DEV_HW_YPP:
		slot_id = 9;
		break;
	case DEV_HW_MCSC0:
		slot_id = 10;
		break;
	case DEV_HW_BYRP:
		slot_id = 11;
		break;
	case DEV_HW_RGBP:
		slot_id = 12;
		break;
	case DEV_HW_MCFP:
		slot_id = 13;
		break;
	default:
		dbg_hw(2, "Invalid hw id(%d)", hw_id);
		break;
	}

	return slot_id;
}

static struct is_hw_ip *pablo_hw_chain_info_get_hw_ip(struct is_hardware *hw, u32 hw_id)
{
	int hw_slot = pablo_hw_chain_info_get_hw_slot_id(hw, hw_id);

	if (!valid_hw_slot_id(hw_slot)) {
		err("invalid slot (id:%d, slot:%d)", hw_id, hw_slot);
		return NULL;
	}

	return &hw->hw_ip[hw_slot];
}

static void pablo_hw_chain_info_query_cap(struct is_hardware *hw, u32 hw_id, void *cap_data)
{
	switch (hw_id) {
	case DEV_HW_MCSC0:
		{
			struct is_hw_mcsc_cap *cap = (struct is_hw_mcsc_cap *)cap_data;

			cap->hw_ver = HW_SET_VERSION(11, 0, 0, 0);
			cap->max_output = 5;
			cap->max_djag = 1;
			cap->max_cac = 1;
			cap->max_uvsp = 2;
			cap->hwfc = MCSC_CAP_SUPPORT;
			cap->in_otf = MCSC_CAP_SUPPORT;
			cap->in_dma = MCSC_CAP_NOT_SUPPORT;
			cap->out_dma[0] = MCSC_CAP_SUPPORT;
			cap->out_dma[1] = MCSC_CAP_SUPPORT;
			cap->out_dma[2] = MCSC_CAP_SUPPORT;
			cap->out_dma[3] = MCSC_CAP_SUPPORT;
			cap->out_dma[4] = MCSC_CAP_SUPPORT;
			cap->out_dma[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[0] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[1] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[2] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_otf[5] = MCSC_CAP_NOT_SUPPORT;
			cap->out_hwfc[3] = MCSC_CAP_SUPPORT;
			cap->out_hwfc[4] = MCSC_CAP_SUPPORT;
			cap->out_post[0] = MCSC_CAP_SUPPORT;
			cap->out_post[1] = MCSC_CAP_SUPPORT;
			cap->out_post[2] = MCSC_CAP_SUPPORT;
			cap->out_post[3] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[4] = MCSC_CAP_NOT_SUPPORT;
			cap->out_post[5] = MCSC_CAP_NOT_SUPPORT;
			cap->enable_shared_output = false;
			cap->tdnr = MCSC_CAP_NOT_SUPPORT;
			cap->djag = MCSC_CAP_SUPPORT;
			cap->cac = MCSC_CAP_NOT_SUPPORT;
			cap->uvsp = MCSC_CAP_NOT_SUPPORT;
			cap->ysum = MCSC_CAP_NOT_SUPPORT;
			cap->ds_vra = MCSC_CAP_NOT_SUPPORT;
		}
		break;
	default:
		break;
	}
}

static int pablo_hw_set_votf_hwfc(struct is_hardware *hw, u32 output_id)
{
	struct votf_info vinfo;

	vinfo.service = TWS;
	vinfo.ip = VOTF_MCSC_W;
	vinfo.id = 0;

	return votf_set_mcsc_hwfc(&vinfo, output_id);
}

static int pablo_hw_set_votf_reset(struct is_hardware *hw)
{
	struct votf_info src_info;
	int ret;

	src_info.service = TWS;
	src_info.ip = VOTF_RGBP_W;
	src_info.id = 0;

	ret = votfitf_reset(&src_info, SW_RESET);
	if (ret) {
		err("L0_Y votf votfitf_reset fail. TWS 0x%04X-%d", src_info.ip, src_info.id);
		return ret;
	}

	return 0;
}

static struct is_mem *pablo_hw_get_iommu_mem(struct is_hardware *hw, u32 gid)
{
	struct is_core *core = is_get_is_core();
	struct pablo_device_iommu_group *iommu_group;

	switch (gid) {
	case GROUP_ID_MCS0:
		iommu_group = pablo_iommu_group_get(0);
		return &iommu_group->mem;
	default:
		return &core->resourcemgr.mem;
	}
}

static bool pablo_hw_check_crta_group(struct is_hardware *hw, int group_id)
{
	if (!IS_ENABLED(CRTA_CALL))
		return false;

	switch (group_id) {
	case GROUP_ID_PAF0:
	case GROUP_ID_PAF1:
	case GROUP_ID_PAF2:
	case GROUP_ID_PAF3:
	case GROUP_ID_3AA0:
	case GROUP_ID_3AA1:
	case GROUP_ID_3AA2:
	case GROUP_ID_3AA3:
		return true;
	default:
		return false;
	}
}

static bool pablo_hw_skip_setfile_load(struct is_hardware *hw, int hw_id)
{

	switch (hw_id) {
	case DEV_HW_MCSC0:
		return 0;
	default:
		return 1;
	}
}

static const struct pablo_hw_chain_info_ops phci_ops = {
	.get_cr_set = pablo_hw_chain_info_get_cr_set,
	.get_config = pablo_hw_chain_info_get_config,
	.get_hw_slot_id = pablo_hw_chain_info_get_hw_slot_id,
	.get_hw_ip = pablo_hw_chain_info_get_hw_ip,
	.query_cap = pablo_hw_chain_info_query_cap,
	.get_ctrl = pablo_hw_cmn_get_ctrl,
	.set_votf_hwfc = pablo_hw_set_votf_hwfc,
	.set_votf_reset = pablo_hw_set_votf_reset,
	.get_iommu_mem = pablo_hw_get_iommu_mem,
	.check_crta_group = pablo_hw_check_crta_group,
	.skip_setfile_load = pablo_hw_skip_setfile_load,
};

int pablo_hw_chain_info_probe(struct is_hardware *hw)
{
	hw->chain_info_ops = &phci_ops;

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_hw_chain_info_probe);
