/*
 * Samsung Exynos SoC series FIMC-IS2 driver
 *
 * exynos fimc-is2 device interface functions
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-interface-wrap.h"
#include "fimc-is-api-common.h"

int fimc_is_itf_s_param_wrap(struct fimc_is_device_ischain *device,
	u32 lindex,
	u32 hindex,
	u32 indexes)
{
	struct fimc_is_hardware *hardware = NULL;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	ret = fimc_is_hardware_set_param(hardware,
		instance,
		device->is_region,
		lindex,
		hindex,
		hardware->hw_path[instance]);
	if (ret) {
		merr("fimc_is_hardware_set_param is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_itf_a_param_wrap(struct fimc_is_device_ischain *device,
	u32 group)
{
	struct fimc_is_hardware *hardware = NULL;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

#if !defined(SETFILE_DISABLE)
	ret = fimc_is_hardware_apply_setfile(hardware,
		instance,
		(group & GROUP_ID_PARM_MASK),
		device->setfile,
		hardware->hw_path[instance]);
	if (ret) {
		merr("fimc_is_hardware_apply_setfile is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
#endif
	return ret;
}

int fimc_is_itf_f_param_wrap(struct fimc_is_device_ischain *device,
	u32 group)
{
	struct fimc_is_hardware *hardware= NULL;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

#if !defined(SETFILE_DISABLE)
	ret = fimc_is_hardware_apply_setfile(hardware,
		instance,
		(group & GROUP_ID_PARM_MASK),
		device->setfile,
		hardware->hw_path[instance]);
	if (ret) {
		merr("fimc_is_hardware_apply_setfile is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
#endif
	return ret;
}

int fimc_is_itf_enum_wrap(struct fimc_is_device_ischain *device)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	/* DICO function */

	return ret;
}

void fimc_is_itf_storefirm_wrap(struct fimc_is_device_ischain *device)
{
	dbg_hw("%s\n", __func__);

	/* DICO function */
}

void fimc_is_itf_restorefirm_wrap(struct fimc_is_device_ischain *device)
{
	dbg_hw("%s\n", __func__);

	/* DICO function */
}

int fimc_is_itf_set_fwboot_wrap(struct fimc_is_device_ischain *device, u32 val)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	/* DICO function */

	return ret;
}

int fimc_is_itf_open_wrap(struct fimc_is_device_ischain *device,
	u32 module_id,
	u32 flag,
	u32 offset_path)
{
	struct fimc_is_hardware *hardware;
	struct fimc_is_path_info *path;
	struct fimc_is_group *group;
	struct is_region *region;
	bool rep_flag;
	u32 instance = 0;
	u32 hw_id = 0;
	u32 hw_id_end = 0;
	int ret = 0;

	info_hw("%s: offset_path(0x%8x) flag(%d) sen(%d)\n", __func__, offset_path, flag, module_id);

	if (flag == REPROCESSING_FLAG)
		rep_flag = true;
	else
		rep_flag = false;

	instance = device->instance;
	hardware = device->hardware;
	path = (struct fimc_is_path_info *)&device->is_region->shared[offset_path];

	region = device->is_region;
	region->shared[MAX_SHARED_COUNT-1] = MAGIC_NUMBER;

	for (hw_id = 0; hw_id < DEV_HW_END; hw_id++)
		clear_bit(hw_id, &hardware->hw_path[instance]);

	if (path->group[GROUP_SLOT_3AA] == GROUP_ID_3AA0)
		hw_id = DEV_HW_3AA0;
	else if (path->group[GROUP_SLOT_3AA] == GROUP_ID_3AA1)
		hw_id = DEV_HW_3AA1;
	else {
		merr("Invalid group ID(%d)", device, path->group[GROUP_SLOT_3AA]);
		ret = -EINVAL;
		goto p_err;
	}
	group = &device->group_3aa;

	ret = fimc_is_hardware_open(hardware, hw_id, group, device->instance,
			rep_flag, module_id);
	if (ret) {
		err("fimc_is_hardware_open(%d) is fail", hw_id);
		ret = -EINVAL;
		goto p_err;
	}

	if (path->group[GROUP_SLOT_ISP] == GROUP_ID_ISP0) {
		hw_id = DEV_HW_ISP0;
	} else if (path->group[GROUP_SLOT_ISP] == GROUP_ID_ISP1) {
		hw_id = DEV_HW_ISP1;
	} else {
		merr("Invalid group ID(%d)", device, path->group[GROUP_SLOT_ISP]);
		ret = -EINVAL;
		goto p_err;
	}
	group = &device->group_isp;

	ret = fimc_is_hardware_open(hardware, hw_id, group, instance,
			rep_flag, module_id);
	if (ret) {
		err("fimc_is_hardware_open(%d) is fail", hw_id);
		ret = -EINVAL;
		goto p_err;
	}

	hw_id_end = (rep_flag ? DEV_HW_DIS : DEV_HW_END); /* HACK */
	for (hw_id = DEV_HW_DRC; hw_id < hw_id_end; hw_id++) {
		ret = fimc_is_hardware_open(hardware, hw_id, group, instance,
				rep_flag, module_id);
		if (ret) {
			err("fimc_is_hardware_open(%d) is fail", hw_id);
			ret = -EINVAL;
			goto p_err;
		}

	}

	frame_manager_probe(&hardware->framemgr_late, FRAMEMGR_ID_HW | 0xFF);
	frame_manager_open(&hardware->framemgr_late, FIMC_IS_MAX_HW_FRAME_LATE);

p_err:
	info("%s: done: hw_path[0x%lx]\n", __func__, hardware->hw_path[instance]);

	return ret;
}

int fimc_is_itf_close_wrap(struct fimc_is_device_ischain *device)
{
	struct fimc_is_hardware *hardware;
	struct fimc_is_path_info *path;
	struct fimc_is_group *group;
	u32 offset_path = 0;
	u32 instance = 0;
	u32 hw_id = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;
	offset_path = (sizeof(struct sensor_open_extended) / 4) + 1;
	path = (struct fimc_is_path_info *)&device->is_region->shared[offset_path];

	if (path->group[GROUP_SLOT_3AA] == GROUP_ID_3AA0)
		hw_id = DEV_HW_3AA0;
	else if (path->group[GROUP_SLOT_3AA] == GROUP_ID_3AA1)
		hw_id = DEV_HW_3AA1;
	else {
		merr("Invalid group ID(%d)", device, path->group[GROUP_SLOT_3AA]);
		ret = -EINVAL;
		goto p_err;
	}
	group = &device->group_3aa;

	ret = fimc_is_hardware_delete_setfile(hardware, instance, hardware->hw_path[instance]);
	if (ret) {
		merr("fimc_is_hardware_delete_setfile is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_hardware_close(hardware, hw_id, device->instance);
	if (ret) {
		err("fimc_is_hardware_close(%d) is fail", hw_id);
		ret = -EINVAL;
		goto p_err;
	}

	if (path->group[GROUP_SLOT_ISP] == GROUP_ID_ISP0) {
		hw_id = DEV_HW_ISP0;
	} else if (path->group[GROUP_SLOT_ISP] == GROUP_ID_ISP1) {
		hw_id = DEV_HW_ISP1;
	} else {
		merr("Invalid group ID(%d)", device, path->group[GROUP_SLOT_ISP]);
		ret = -EINVAL;
		goto p_err;
	}
	group = &device->group_isp;

	ret = fimc_is_hardware_close(hardware, hw_id, device->instance);
	if (ret) {
		err("fimc_is_hardware_close(%d) is fail", hw_id);
		ret = -EINVAL;
		goto p_err;
	}

	for (hw_id = DEV_HW_DRC; hw_id < DEV_HW_END; hw_id++) {
		ret = fimc_is_hardware_close(hardware, hw_id, device->instance);
		if (ret) {
			err("fimc_is_hardware_close(%d) is fail", hw_id);
			ret = -EINVAL;
			goto p_err;
		}
	}

	frame_manager_close(&hardware->framemgr_late);

p_err:
	check_lib_memory_leak();
	info("%s: done: hw_path[0x%lx]\n", __func__, hardware->hw_path[instance]);

	return ret;
}

int fimc_is_itf_setaddr_wrap(struct fimc_is_interface *itf,
	struct fimc_is_device_ischain *device,
	u32 *setfile_addr)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	*setfile_addr = FIMC_IS_SETFILE_OFFSET;

	return ret;
}

int fimc_is_itf_setfile_wrap(struct fimc_is_interface *itf,
	u32 setfile_addr,
	struct fimc_is_device_ischain *device)
{
	struct fimc_is_hardware *hardware;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

#if !defined(SETFILE_DISABLE)
	ret = fimc_is_hardware_load_setfile(hardware, setfile_addr, instance, hardware->hw_path[instance]);
	if (ret) {
		merr("fimc_is_hardware_load_setfile is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
#endif

	return ret;
}

int fimc_is_itf_map_wrap(struct fimc_is_device_ischain *device,
	u32 group, u32 shot_addr, u32 shot_size)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	/* DICO function */

	return ret;
}

int fimc_is_itf_unmap_wrap(struct fimc_is_device_ischain *device,
	u32 group)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	/* DICO function */

	return ret;
}

int fimc_is_itf_stream_on_wrap(struct fimc_is_device_ischain *device)
{
	int ret = 0;
	struct fimc_is_hardware *hardware;
	u32 instance = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	ret = fimc_is_hardware_stream_on(hardware, instance, hardware->hw_path[instance]);
	if (ret) {
		merr("fimc_is_hardware_stream_on is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_itf_stream_off_wrap(struct fimc_is_device_ischain *device)
{
	struct fimc_is_hardware *hardware;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	ret = fimc_is_hardware_stream_off(hardware, instance, hardware->hw_path[instance]);
	if (ret) {
		merr("fimc_is_hardware_stream_off is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_itf_process_on_wrap(struct fimc_is_device_ischain *device,
	u32 group)
{
	struct fimc_is_hardware *hardware;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	/* fimc_is_log_write("[%d]%s: map:0x%08x\n", instance, __func__, (unsigned int)hardware->hw_path[instance]); */
	ret = fimc_is_hardware_process_start(hardware, instance, group);
	if (ret) {
		merr("fimc_is_hardware_process_start is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_itf_process_off_wrap(struct fimc_is_device_ischain *device,
	u32 group,
	u32 fstop)
{
	struct fimc_is_hardware *hardware;
	u32 instance = 0;
	int ret = 0;

	dbg_hw("%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	ret = fimc_is_hardware_process_stop(hardware, instance, group, fstop);
	if (ret) {
		merr("fimc_is_hardware_process_stop is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_itf_power_down_wrap(struct fimc_is_interface *interface)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	/* DICO function */

	return ret;
}

int fimc_is_itf_sys_ctl_wrap(struct fimc_is_device_ischain *device,
			int cmd, int val)
{
	int ret = 0;

	dbg_hw("%s\n", __func__);

	/* DICO function */

	return ret;
}

int fimc_is_itf_sensor_mode_wrap(struct fimc_is_device_ischain *ischain)
{
	dbg_hw("%s\n", __func__);

	/* DICO function */

	return 0;
}

int fimc_is_itf_shot_wrap(struct fimc_is_device_ischain *device,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame)
{
	struct fimc_is_hardware *hardware;
	u32 instance = 0;
	int ret = 0;

	hardware = device->hardware;
	instance = device->instance;

	ret = fimc_is_hardware_grp_shot(hardware,
		instance,
		group,
		frame,
		hardware->hw_path[instance]);
	if (ret) {
		merr("fimc_is_hardware_grp_shot is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}
