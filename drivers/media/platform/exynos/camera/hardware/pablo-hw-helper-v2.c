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

#include "pablo-hw-helper.h"
#include "is-hw-common-dma.h"

#define GET_LME_COREX_OFFSET(SET_ID) \
	((SET_ID <= COREX_SET_D) ? (0x0 + ((SET_ID) * 0x2000)) : 0x8000)

int pablo_hw_helper_set_cr_set(void __iomem *base, unsigned int offset,
				u32 regs_size, struct cr_set *regs)
{
	int i;

	for (i = 0; i < regs_size; i++)
		writel_relaxed(regs[i].reg_data, base + offset + regs[i].reg_addr);

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_hw_helper_set_cr_set);

int pablo_hw_helper_ext_fsync(struct pablo_mmio *pmio, unsigned int offset,
			      void *buf, const void *items, size_t block_count)
{
	struct c_loader_buffer *clb = (struct c_loader_buffer *)buf;

	if (block_count == 0)
		return -EINVAL;

	pmio_cache_ext_fsync(pmio, buf, items, block_count);
	if (clb->num_of_pairs > 0)
		clb->num_of_headers++;

	return 0;
}

static bool pablo_hw_helper_set_rta_regs(struct is_hw_ip *hw_ip, u32 instance, u32 set_id,
					bool skip, struct is_frame *frame, void *buf)
{
	struct size_cr_set *scs;
	void *config;
	ulong debug_iq = (unsigned long)is_get_debug_param(IS_DEBUG_PARAM_IQ);
	unsigned int offset = (hw_ip->id == DEV_HW_LME0) ?
				GET_LME_COREX_OFFSET(set_id) : GET_COREX_OFFSET(set_id);

	if (unlikely(test_bit(hw_ip->id, &debug_iq))) {
		msdbg_hw(1, "bypass s_iq_regs\n", instance, hw_ip);
		return true;
	}

	if (skip)
		return false;

	config = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_config, hw_ip->id, frame);
	if (!config) {
		mserr_hw("config plane is NULL", instance, hw_ip);
		return true;
	}

	/* The first 4 byte at config plane has size info. So, 4 byte must be added. */
	if (CALL_HWIP_OPS(hw_ip, set_config, 0, instance, frame->fcount,
			config + sizeof(u32)))
		return true;

	scs = CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, get_cr_set, hw_ip->id, frame);
	if (!scs) {
		mserr_hw("scs plane is NULL", instance, hw_ip);
		return true;
	}

	/* HACK: If zero is invalid input, this code will be changed to shot fail handling. */
	if (!scs->size) {
		mserr_hw("size of cr_set is zero.", instance, hw_ip);
		return true;
	}

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_E);

	if (hw_ip->pmio && buf)
		pablo_hw_helper_ext_fsync(hw_ip->pmio, offset, buf, scs->cr, scs->size);
	else
		pablo_hw_helper_set_cr_set(hw_ip->regs[REG_SETA], offset, scs->size, scs->cr);

	CALL_HW_OPS(hw_ip, dbg_trace, hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_X);

	return false;
}

const static struct pablo_hw_helper_ops pablo_hw_helper_ops_v2 = {
	.set_rta_regs = pablo_hw_helper_set_rta_regs,
};

int pablo_hw_helper_probe(struct is_hw_ip *hw_ip)
{
	hw_ip->help_ops = &pablo_hw_helper_ops_v2;

	return 0;
}
EXPORT_SYMBOL_GPL(pablo_hw_helper_probe);
