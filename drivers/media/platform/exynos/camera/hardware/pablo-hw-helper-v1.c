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
#include "is-interface-ddk.h"

static struct lib_interface_func *lib_func[LIB_FUNC_TYPE_END];

#define GET_LME_COREX_OFFSET(SET_ID) \
	((SET_ID <= COREX_SET_D) ? (0x0 + ((SET_ID) * 0x2000)) : 0x8000)

int pablo_hw_helper_set_iq_set(void __iomem *base, u32 set_id, u32 hw_id,
				struct is_hw_iq *iq_set,
				struct is_hw_iq *cur_iq_set)
{
	int i;
	struct cr_set *regs = iq_set->regs;
	u32 regs_size = iq_set->size;
	u32 offset;

	if (hw_id == DEV_HW_LME0)
		offset = GET_LME_COREX_OFFSET(set_id);
	else
		offset = GET_COREX_OFFSET(set_id);

	if (cur_iq_set->size == regs_size &&
	    !memcmp(cur_iq_set->regs, regs, sizeof(struct cr_set) * regs_size))
		return 0;

	for (i = 0; i < regs_size; i++)
		writel_relaxed(regs[i].reg_data, base + offset + regs[i].reg_addr);

	memcpy(cur_iq_set->regs, regs, sizeof(struct cr_set) * regs_size);
	cur_iq_set->size = regs_size;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_hw_helper_set_iq_set);

static bool pablo_hw_helper_set_rta_regs(struct is_hw_ip *hw_ip, u32 instance, u32 set_id,
					bool skip, struct is_frame *frame, void *buf)
{
	struct is_hw_iq *iq_set = &hw_ip->iq_set[instance];
	struct is_hw_iq *cur_iq_set = &hw_ip->cur_hw_iq_set[set_id];
	unsigned long flag;
	bool configured = false;
	ulong debug_iq = (unsigned long)is_get_debug_param(IS_DEBUG_PARAM_IQ);

	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return true;

	if (unlikely(test_bit(hw_ip->id, &debug_iq))) {
		msdbg_hw(1, "bypass s_iq_regs\n", instance, hw_ip);
		return true;
	}

	if (skip)
		return false;

	msdbg_hw(4, "%s\n", instance, hw_ip, __func__);

	spin_lock_irqsave(&iq_set->slock, flag);

	_is_hw_frame_dbg_trace(hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_E);

	if (test_and_clear_bit(CR_SET_CONFIG, &iq_set->state)) {
		pablo_hw_helper_set_iq_set(hw_ip->regs[REG_SETA], set_id, hw_ip->id,
					iq_set, cur_iq_set);

		set_bit(CR_SET_EMPTY, &iq_set->state);
		configured = true;
	}

	_is_hw_frame_dbg_trace(hw_ip, frame->fcount, DEBUG_POINT_RTA_REGS_X);

	spin_unlock_irqrestore(&iq_set->slock, flag);

	if (!configured) {
		mswarn_hw("[F%d]iq_set is NOT configured. iq_set (%d/0x%lx) cur_iq_set %d",
				instance, hw_ip,
				atomic_read(&hw_ip->fcount),
				iq_set->fcount, iq_set->state, cur_iq_set->fcount);
		return true;
	}

	return false;
}

static int __nocfi pablo_hw_helper_open(struct is_hw_ip *hw_ip, u32 instance, void *lib,
					u32 lf_type)
{
	int ret;

	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	is_fpsimd_get_func();
	get_lib_func(lf_type, (void **)&lib_func[lf_type]);
	is_fpsimd_put_func();

	if (!lib_func[lf_type]) {
		mserr_hw("lib_func(null)", instance, hw_ip);
		is_load_clear();
		return -EINVAL;
	}
	msinfo_hw("get_lib_func is set\n", instance, hw_ip);

	((struct is_lib_isp *)lib)->func = lib_func[lf_type];

	ret = is_lib_isp_chain_create(hw_ip, (struct is_lib_isp *)lib, instance);
	if (ret) {
		mserr_hw("chain create fail", instance, hw_ip);
		return ret;
	}

	return 0;
}

static int pablo_hw_helper_close(struct is_hw_ip *hw_ip, u32 instance, void *lib)
{
	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	is_lib_isp_chain_destroy(hw_ip, (struct is_lib_isp *)lib, instance);

	return 0;
}

static int pablo_hw_helper_init(struct is_hw_ip *hw_ip, u32 instance, void *lib,
				bool flag, u32 f_type, u32 lf_type)
{
	int ret;

	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	((struct is_lib_isp *)lib)->object = NULL;
	((struct is_lib_isp *)lib)->func = lib_func[lf_type];

	ret = is_lib_isp_object_create(hw_ip, (struct is_lib_isp *)lib, instance,
					(u32)flag, f_type);
	if (ret) {
		mserr_hw("object create fail", instance, hw_ip);
		return ret;
	}

	return 0;
}

static int pablo_hw_helper_deinit(struct is_hw_ip *hw_ip, u32 instance, void *lib)
{
	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	is_lib_isp_object_destroy(hw_ip, (struct is_lib_isp *)lib, instance);
	((struct is_lib_isp *)lib)->object = NULL;

	return 0;
}

static int pablo_hw_helper_disable(struct is_hw_ip *hw_ip, u32 instance, void *lib)
{
	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	if (!test_bit(HW_RUN, &hw_ip->state)) {
		msdbg_hw(2, "already disabled\n", instance, hw_ip);
		return 0;
	}

	is_lib_isp_stop(hw_ip, (struct is_lib_isp *)lib, instance, 1);

	return 0;
}

static int pablo_hw_helper_lib_shot(struct is_hw_ip *hw_ip, u32 instance, bool skip,
				struct is_frame *frame, void *lib, void *param_set)
{
	int ret;

	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	if (skip)
		return 0;

	ret = is_lib_isp_set_ctrl(hw_ip, (struct is_lib_isp *)lib, frame);
	if (ret)
		return ret;

	ret = is_lib_isp_shot(hw_ip, (struct is_lib_isp *)lib, param_set, frame->shot);
	if (ret)
		return ret;

	return 0;
}

static int pablo_hw_helper_get_meta(struct is_hw_ip *hw_ip, u32 instance,
				struct is_frame *frame, void *lib)
{
	int ret;

	sdbg_hw(4, "%s\n", hw_ip, __func__);

	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	ret = is_lib_isp_get_meta(hw_ip, (struct is_lib_isp *)lib, frame);
	if (ret) {
		mserr_hw("get_meta fail", instance, hw_ip);
		return ret;
	}

	return 0;
}

static int pablo_hw_helper_get_cap_meta(struct is_hw_ip *hw_ip, u32 instance,
					void *lib, u32 fcount, u32 size, ulong addr)
{
	int ret;

	sdbg_hw(4, "%s\n", hw_ip, __func__);

	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	ret = is_lib_isp_get_cap_meta(hw_ip, (struct is_lib_isp *)lib,
					instance, fcount, size, addr);
	if (ret) {
		mserr_hw("get_cap_meta fail ret(%d)", instance, hw_ip, ret);
		return ret;
	}

	return 0;
}

static int pablo_hw_helper_g_setfile_version(struct is_hw_ip *hw_ip, u32 instance, u32 version)
{
	switch (version) {
	case SETFILE_V2:
		return false;
	case SETFILE_V3:
		return true;
	default:
		mserr_hw("invalid version (%d)", instance, hw_ip, version);
		break;
	}

	return false;
}

static int pablo_hw_helper_load_cal_data(struct is_hw_ip *hw_ip, u32 instance, void *lib)
{
	int ret;
	enum exynos_sensor_position sp = hw_ip->hardware->sensor_position[instance];
	struct is_minfo *minfo = is_get_is_minfo();
	ulong cal_addr;

	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	if (sp < SENSOR_POSITION_MAX) {
		cal_addr = minfo->kvaddr_cal[sp];

		msinfo_hw("load cal data, position: %d, addr: %p\n",
				instance, hw_ip, sp, (void *)cal_addr);

		ret = is_lib_isp_load_cal_data((struct is_lib_isp *)lib, instance, cal_addr);
		if (ret)
			return ret;

		ret = is_lib_isp_get_cal_data((struct is_lib_isp *)lib, instance,
				&hw_ip->hardware->cal_info[sp],
				CAL_TYPE_LSC_UVSP);
		if (ret)
			return ret;
	}

	return 0;
}

static int pablo_hw_helper_load_setfile(struct is_hw_ip *hw_ip, u32 instance, void *lib)
{
	int ret = 0, flag;
	ulong addr;
	u32 size, index;
	enum exynos_sensor_position sp = hw_ip->hardware->sensor_position[instance];
	struct is_hw_ip_setfile *setfile = &hw_ip->setfile[sp];

	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}

	flag = pablo_hw_helper_g_setfile_version(hw_ip, instance, setfile->version);
	msinfo_lib("create_tune_set count %d\n", instance, hw_ip, setfile->using_count);

	for (index = 0; index < setfile->using_count; index++) {
		addr = setfile->table[index].addr;
		size = setfile->table[index].size;
		ret |= is_lib_isp_create_tune_set((struct is_lib_isp *)lib,
						addr, size, index, flag, instance);

		set_bit(index, ((struct is_lib_isp *)lib)->tune_count);
	}

	set_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int pablo_hw_helper_apply_setfile(struct is_hw_ip *hw_ip, u32 instance, void *lib,
					u32 scenario)
{
	int ret;
	u32 setfile_index;
	enum exynos_sensor_position sp = hw_ip->hardware->sensor_position[instance];
	struct is_hw_ip_setfile *setfile = &hw_ip->setfile[sp];

	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return -ESRCH;
	}

	if (setfile->using_count == 0)
		return 0;

	setfile_index = setfile->index[scenario];
	if (setfile_index >= setfile->using_count) {
		mserr_hw("setfile index is out-of-range, [%d:%d]",
				instance, hw_ip, scenario, setfile_index);
		return -EINVAL;
	}

	msinfo_hw("setfile (%d) scenario (%d)\n", instance, hw_ip, setfile_index, scenario);

	ret = is_lib_isp_apply_tune_set((struct is_lib_isp *)lib,
					setfile_index, instance, scenario);

	return ret;
}

static int pablo_hw_helper_delete_setfile(struct is_hw_ip *hw_ip, u32 instance, void *lib)
{
	int ret = 0;
	int i;
	enum exynos_sensor_position sp = hw_ip->hardware->sensor_position[instance];
	struct is_hw_ip_setfile *setfile = &hw_ip->setfile[sp];

	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	if (!test_bit(HW_INIT, &hw_ip->state)) {
		mserr_hw("not initialized!!", instance, hw_ip);
		return 0;
	}

	if (setfile->using_count == 0)
		return 0;

	for (i = 0; i < setfile->using_count; i++) {
		if (!test_bit(i, ((struct is_lib_isp *)lib)->tune_count))
			continue;

		ret |= is_lib_isp_delete_tune_set((struct is_lib_isp *)lib, (u32)i, instance);
		clear_bit(i, ((struct is_lib_isp *)lib)->tune_count);
	}

	clear_bit(HW_TUNESET, &hw_ip->state);

	return ret;
}

static int pablo_hw_helper_restore(struct is_hw_ip *hw_ip, u32 instance, void *lib)
{
	int ret;

	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	if (!test_bit(HW_OPEN, &hw_ip->state))
		return -EINVAL;

	msinfo_hw("restore\n", instance, hw_ip);

	ret = is_lib_isp_reset_recovery(hw_ip, (struct is_lib_isp *)lib, instance);
	if (ret) {
		mserr_hw("is_lib_isp_reset_recovery fail ret(%d)", instance, hw_ip, ret);
		return ret;
	}

	return 0;
}

static void pablo_hw_helper_free_iqset(struct is_hw_ip *hw_ip)
{
	u32 i;

	for (i = 0; i < COREX_MAX; i++) {
		if (hw_ip->cur_hw_iq_set[i].regs) {
			vfree(hw_ip->cur_hw_iq_set[i].regs);
			hw_ip->cur_hw_iq_set[i].regs = NULL;
		}
	}

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		if (hw_ip->iq_set[i].regs) {
			vfree(hw_ip->iq_set[i].regs);
			hw_ip->iq_set[i].regs = NULL;
		}
	}
}

static int pablo_hw_helper_alloc_iqset(struct is_hw_ip *hw_ip, u32 reg_cnt)
{
	int ret;
	u32 i;

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		hw_ip->iq_set[i].regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
		if (!hw_ip->iq_set[i].regs) {
			serr_hw("failed to alloc iq_set[%d].regs", hw_ip, i);
			ret = -ENOMEM;
			goto err_alloc;
		}
	}

	for (i = 0; i < COREX_MAX; i++) {
		hw_ip->cur_hw_iq_set[i].regs = vzalloc(sizeof(struct cr_set) * reg_cnt);
		if (!hw_ip->cur_hw_iq_set[i].regs) {
			serr_hw("failed to alloc cur_hw_iq_set[%d].regs", hw_ip, i);
			ret = -ENOMEM;
			goto err_alloc;
		}
	}

	for (i = 0; i < IS_STREAM_COUNT; i++) {
		clear_bit(CR_SET_CONFIG, &hw_ip->iq_set[i].state);
		set_bit(CR_SET_EMPTY, &hw_ip->iq_set[i].state);
		spin_lock_init(&hw_ip->iq_set[i].slock);
	}

	return 0;

err_alloc:
	pablo_hw_helper_free_iqset(hw_ip);

	return ret;
}

static int pablo_hw_helper_set_regs(struct is_hw_ip *hw_ip, u32 instance,
				struct cr_set *regs, u32 regs_size)
{
	struct is_hw_iq *iq_set;
	ulong flag = 0;

	if (!regs) {
		mserr_hw("regs is NULL", instance, hw_ip);
		return -EINVAL;
	}

	iq_set = &hw_ip->iq_set[instance];
	if (!iq_set->regs) {
		mserr_hw("iq_set->regs is NULL", instance, hw_ip);
		return -EINVAL;
	}

	if (!test_and_clear_bit(CR_SET_EMPTY, &iq_set->state))
		return -EBUSY;

	spin_lock_irqsave(&iq_set->slock, flag);

	iq_set->size = regs_size;
	memcpy((void *)iq_set->regs, (void *)regs, sizeof(struct cr_set) * regs_size);

	spin_unlock_irqrestore(&iq_set->slock, flag);
	set_bit(CR_SET_CONFIG, &iq_set->state);

	msdbg_hw(2, "Store IQ regs set: %p, size(%d)\n", instance, hw_ip, regs, regs_size);

	return 0;
}

static int pablo_hw_helper_notify_timeout(struct is_hw_ip *hw_ip, u32 instance, void *lib)
{
	if (!CALL_HW_CHAIN_INFO_OPS(hw_ip->hardware, check_valid_lib, hw_ip->id))
		return 0;

	if (test_bit(HW_END, &hw_ip->state))
		msinfo_hw("HW end interrupt occur but no end callback", instance, hw_ip);

	return is_lib_notify_timeout((struct is_lib_isp *)lib, instance);
}

const static struct pablo_hw_helper_ops pablo_hw_helper_ops_v1 = {
	.set_rta_regs = pablo_hw_helper_set_rta_regs,
	.open = pablo_hw_helper_open,
	.close = pablo_hw_helper_close,
	.init = pablo_hw_helper_init,
	.deinit = pablo_hw_helper_deinit,
	.disable = pablo_hw_helper_disable,
	.lib_shot = pablo_hw_helper_lib_shot,
	.get_meta = pablo_hw_helper_get_meta,
	.get_cap_meta = pablo_hw_helper_get_cap_meta,
	.load_cal_data = pablo_hw_helper_load_cal_data,
	.load_setfile = pablo_hw_helper_load_setfile,
	.apply_setfile = pablo_hw_helper_apply_setfile,
	.delete_setfile = pablo_hw_helper_delete_setfile,
	.restore = pablo_hw_helper_restore,
	.free_iqset = pablo_hw_helper_free_iqset,
	.alloc_iqset = pablo_hw_helper_alloc_iqset,
	.set_regs = pablo_hw_helper_set_regs,
	.notify_timeout = pablo_hw_helper_notify_timeout,
};

int pablo_hw_helper_probe(struct is_hw_ip *hw_ip)
{
	hw_ip->help_ops = &pablo_hw_helper_ops_v1;

	return 0;
}
KUNIT_EXPORT_SYMBOL(pablo_hw_helper_probe);
