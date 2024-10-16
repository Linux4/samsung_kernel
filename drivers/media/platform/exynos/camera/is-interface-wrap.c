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

#include "is-interface-wrap.h"
#include "is-interface-library.h"
#include "is-param.h"
#include "pablo-fpsimd.h"
#include "pablo-icpu-adapter.h"
#include "is-device-sensor-peri.h"
#include "is-votfmgr.h"

int is_itf_s_param_wrap(struct is_device_ischain *device,
		IS_DECLARE_PMAP(pmap))
{
	struct is_hardware *hardware = device->hardware;
	int ret;
	unsigned long i;

	dbg_hw(2, "%s\n", __func__);

	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		ret = is_hardware_set_param(hardware, i, device->is_region,
				pmap, hardware->hw_map[i]);
		if (ret) {
			merr("is_hardware_set_param is fail(%d)", device, ret);
			return ret;
		}
	}

	return 0;
}

int is_itf_a_param_wrap(struct is_device_ischain *device, u64 group)
{
	struct is_hardware *hardware = device->hardware;
	int ret;
	unsigned long i;

	dbg_hw(2, "%s\n", __func__);

	if (IS_ENABLED(SKIP_SETFILE_LOAD))
		return 0;


	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		ret = is_hardware_apply_setfile(hardware, i,
				device->setfile & IS_SETFILE_MASK,
				hardware->hw_map[i]);
		if (ret) {
			merr("is_hardware_apply_setfile is fail(%d)", device, ret);
			return ret;
		}
	}

	return 0;
}

static u32 get_cstat_chain_id(struct is_device_ischain *idi, u32 i)
{
	u32 group_id;
	struct is_path_info *path;

	if (test_bit(IS_ISCHAIN_REPROCESSING, &idi->state))
		return 0;

	path = &idi->path;
	group_id = (idi->instance == i) ? path->group[GROUP_SLOT_3AA] : path->group_2nd[GROUP_SLOT_3AA];

	return group_id - GROUP_ID_3AA0;
}

static int is_hw_crta_buf_put(struct pablo_crta_bufmgr *bufmgr,
				struct pablo_icpu_adt *icpu_adt,
				u32 instance)
{
	int ret;
	u32 buf_idx;
	struct pablo_crta_buf_info buf_info = { 0, };

	CALL_CRTA_BUFMGR_OPS(bufmgr, get_static_buf, PABLO_CRTA_STATIC_BUF_SENSOR, &buf_info);
	ret = CALL_ADT_MSG_OPS(icpu_adt, send_msg_put_buf, instance, PABLO_BUFFER_STATIC, &buf_info);
	if (ret) {
		merr_adt("failed to send_msg_put_buf: %d", instance, ret);
		return ret;
	}

	buf_idx = 0;
	while (!CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, PABLO_CRTA_BUF_SS_CTRL,
				     buf_idx++, false, &buf_info)) {
		if (!buf_info.dva)
			break;
		ret = CALL_ADT_MSG_OPS(icpu_adt, send_msg_put_buf, instance,
				       PABLO_BUFFER_SENSOR_CONTROL, &buf_info);
		if (ret) {
			merr_adt("failed to send_msg_put_buf: %d", instance, ret);
			return ret;
		}
	}

	return 0;
}

static void is_hw_icpu_close(struct is_device_ischain *idi, u32 max_instance)
{
	unsigned long i;
	struct pablo_icpu_adt *icpu_adt = pablo_get_icpu_adt();
	struct pablo_crta_bufmgr *bufmgr;

	if (max_instance > IS_STREAM_COUNT) {
		mwarn("invalid max_instance(%d)\n", idi, max_instance);
		max_instance = IS_STREAM_COUNT;
	}

	for_each_set_bit(i, &idi->ginstance_map, max_instance) {
		bufmgr = &idi->resourcemgr->crta_bufmgr[i];
		if (CALL_ADT_MSG_OPS(icpu_adt, send_msg_close, i))
			merr("failed to send_msg_close", idi);
		if (CALL_ADT_OPS(icpu_adt, close, i))
			merr("failed to close", idi);
		if (CALL_CRTA_BUFMGR_OPS(bufmgr, close))
			merr("failed to close crta_buf", idi);
	}
}

static int is_hw_icpu_open(struct is_device_ischain *idi, bool rep_flag)
{
	int ret;
	u32 chain_id;
	u32 f_type;
	unsigned long i;
	struct pablo_icpu_adt *icpu_adt = pablo_get_icpu_adt();
	struct pablo_crta_bufmgr *bufmgr;
	struct is_device_sensor *ids = idi->sensor;
	struct is_sensor_interface *itf = NULL;

	if (!rep_flag) {
		itf = is_sensor_get_sensor_interface(ids);
		if (!itf) {
			merr("sensor_interface is NULL", idi);
			return -ENODEV;
		}
	}

	for_each_set_bit(i, &idi->ginstance_map, IS_STREAM_COUNT) {
		bufmgr = &idi->resourcemgr->crta_bufmgr[i];

		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, open, i);
		if (ret) {
			merr("failed to open crta_buf: %d", idi, ret);
			goto err_bufmgr_open;
		}

		ret = CALL_ADT_OPS(icpu_adt, open, i, itf, bufmgr);
		if (ret) {
			merr("failed to open icpu_adt: %d", idi, ret);
			goto err_adt_open;
		}

		chain_id = get_cstat_chain_id(idi, i);
		f_type = is_sensor_get_frame_type(i);

		ret = CALL_ADT_MSG_OPS(icpu_adt, send_msg_open, i,
					chain_id, rep_flag, ids->position, f_type);
		if (ret) {
			merr("failed to send_msg_open: %d", idi, ret);
			goto err_send_msg_open;
		}

		ret = is_hw_crta_buf_put(bufmgr, icpu_adt, i);
		if (ret) {
			merr("failed to is_hw_crta_buf_put: %d", idi, ret);
			goto err_crta_buf_put;
		}
	}

	return 0;

err_crta_buf_put:
	if (CALL_ADT_MSG_OPS(icpu_adt, send_msg_close, i))
		merr("failed to send_msg_close", idi);

err_send_msg_open:
	if (CALL_ADT_OPS(icpu_adt, close, i))
		merr("failed to close", idi);

err_adt_open:
	if (CALL_CRTA_BUFMGR_OPS(bufmgr, close))
		merr("failed to close crta_buf", idi);

err_bufmgr_open:
	is_hw_icpu_close(idi, i);

	return ret;
}

static int is_hw_icpu_start(struct is_device_ischain *idi)
{
	int ret = 0;
	unsigned long i;
	struct pablo_icpu_adt *icpu_adt = pablo_get_icpu_adt();
	struct pablo_crta_bufmgr *bufmgr;
	struct pablo_crta_buf_info buf_info = { 0, };

	for_each_set_bit(i, &idi->ginstance_map, IS_STREAM_COUNT) {
		bufmgr = &idi->resourcemgr->crta_bufmgr[i];
		ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, PABLO_CRTA_BUF_PCSI, 0, true, &buf_info);
		if (ret) {
			merr("failed to get_free_buf(PCSI%lu/%d)", idi, i, ret);
			return ret;
		}

		ret = CALL_ADT_MSG_OPS(icpu_adt, send_msg_start, i, &buf_info);
		if (ret) {
			merr("failed to send_msg_start(%lu/%d)", idi, i, ret);
			CALL_CRTA_BUFMGR_OPS(bufmgr, put_buf, &buf_info);
			return ret;
		}
	}

	return ret;
}

static int is_hw_icpu_stop(struct is_device_ischain *idi)
{
	int ret = 0, ret_adt;
	unsigned long i;
	struct pablo_icpu_adt *icpu_adt = pablo_get_icpu_adt();

	for_each_set_bit(i, &idi->ginstance_map, IS_STREAM_COUNT) {
		ret_adt = CALL_ADT_MSG_OPS(icpu_adt, send_msg_stop, i, PABLO_STOP_IMMEDIATE);
		if (ret_adt)
			merr("failed to call send_msg_stop of icpu_adt(%lu/%d)", idi, i, ret_adt);
		ret |= ret_adt;
	}

	return ret;
}

int is_itf_open_wrap(struct is_device_ischain *device, u32 flag)
{
	struct is_hardware *hardware;
	struct is_device_sensor *sensor;
	struct is_groupmgr *groupmgr;
	struct is_group *gnext;
	u32 instance;
	int ret;
	u32 rsccount;
	unsigned long i, ii;

	info_hw("%s: flag(%d)\n", __func__, flag);

	sensor   = device->sensor;
	instance = device->instance;
	hardware = device->hardware;
	groupmgr = device->groupmgr;

	mutex_lock(&hardware->itf_lock);

	ret = is_hw_icpu_open(device, flag);
	if (ret) {
		merr("failed to is_hw_icpu_open(%d)", device, ret);
		mutex_unlock(&hardware->itf_lock);
		return ret;
	}

	rsccount = atomic_read(&hardware->rsccount);

	if (rsccount == 0) {
		ret = is_init_ddk_thread();
		if (ret) {
			err("failed to create threads for DDK, ret %d", ret);
			mutex_unlock(&hardware->itf_lock);
			return ret;
		}

		atomic_set(&hardware->lic_updated, 0);
	}

	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		hardware->sensor_position[i] = sensor->position;

		gnext = groupmgr->leader[i];
		while (gnext) {
			CALL_HW_GRP_OPS(gnext, open, hardware, i, gnext->hw_slot_id);
			gnext = gnext->gnext;
		}

		ret = is_hardware_open(hardware, i, flag);
		if (ret) {
			merr("is_hardware_open is fail", device);
			goto hardware_close;
		}
	}

	atomic_inc(&hardware->rsccount);

	info("%s: done: hw_map[0x%lx][RSC:%d][%d]\n", __func__,
		hardware->hw_map[instance], atomic_read(&hardware->rsccount),
		hardware->sensor_position[instance]);

	mutex_unlock(&hardware->itf_lock);

	return ret;

hardware_close:
	hardware->logical_hw_map[i] = 0;
	hardware->hw_map[i] = 0;

	for_each_set_bit(ii, &device->ginstance_map, i) {
		is_hardware_close(hardware, ii);
		hardware->logical_hw_map[ii] = 0;
		hardware->hw_map[ii] = 0;
	}

	is_hw_icpu_close(device, IS_STREAM_COUNT);

	mutex_unlock(&hardware->itf_lock);

	return ret;
}

int is_itf_close_wrap(struct is_device_ischain *device)
{
	struct is_hardware *hardware;
	struct is_groupmgr *groupmgr;
	struct is_group *gnext;
	u32 instance;
	int ret;
	u32 rsccount;
	unsigned long i;

	dbg_hw(2, "%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;
	groupmgr = device->groupmgr;

	mutex_lock(&hardware->itf_lock);

	rsccount = atomic_read(&hardware->rsccount);

	if (rsccount == 1)
		is_flush_ddk_thread();

	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		if (!IS_ENABLED(SKIP_SETFILE_LOAD)) {
			ret = is_hardware_delete_setfile(hardware, i,
					hardware->logical_hw_map[i]);
			if (ret) {
				merr("is_hardware_delete_setfile is fail(%d)", device, ret);
				mutex_unlock(&hardware->itf_lock);
				return ret;
			}
		}

		if (is_hardware_close(hardware, i))
			merr("is_hardware_close is fail", device);

		gnext = groupmgr->leader[i];
		while (gnext) {
			CALL_HW_GRP_OPS(gnext, close, hardware, i, gnext->hw_slot_id);
			gnext = gnext->gnext;
		}

		hardware->hw_map[i] = 0;
		hardware->logical_hw_map[i] = 0;
	}

	atomic_dec(&hardware->rsccount);

	info("%s: done: hw_map[RSC:%d]\n", __func__, atomic_read(&hardware->rsccount));

	is_hw_icpu_close(device, IS_STREAM_COUNT);

	mutex_unlock(&hardware->itf_lock);

	return 0;
}

int is_itf_change_chain_wrap(struct is_device_ischain *device, struct is_group *group, u32 next_id)
{
	int ret = 0;
	struct is_hardware *hardware;

	FIMC_BUG(!device);

	hardware = device->hardware;

	mutex_lock(&hardware->itf_lock);

	ret = is_hardware_change_chain(hardware, group, group->instance, next_id);

	mutex_unlock(&hardware->itf_lock);

	mginfo("%s: %s (next_id: %d)\n", group, group, __func__,
		ret ? "fail" : "success", next_id);

	return ret;
}

int is_itf_setfile_wrap(struct is_interface *itf, struct is_device_ischain *device)
{
	struct is_hardware *hardware = device->hardware;
	int ret;
	unsigned long i;

	dbg_hw(2, "%s\n", __func__);

	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		if (!IS_ENABLED(SKIP_SETFILE_LOAD)) {
			ret = is_hardware_load_setfile(hardware, i, hardware->logical_hw_map[i]);
			if (ret) {
				merr("is_hardware_load_setfile is fail(%d)", device,
						ret);
				return ret;
			}
		}
	}

	return 0;
}

int is_itf_map_wrap(struct is_device_ischain *device,
	u32 group, u32 shot_addr, u32 shot_size)
{
	int ret = 0;

	dbg_hw(2, "%s\n", __func__);

	return ret;
}

int is_itf_unmap_wrap(struct is_device_ischain *device, u32 group)
{
	int ret = 0;

	dbg_hw(2, "%s\n", __func__);

	return ret;
}

int is_itf_stream_on_wrap(struct is_device_ischain *device)
{
	struct is_hardware *hardware;
	u32 instance;
	ulong hw_map;
	int ret;
	struct is_group *group_leader;
	unsigned long i;

	dbg_hw(2, "%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	is_set_ddk_thread_affinity();

	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		hw_map = hardware->hw_map[i];

		group_leader = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN,
				get_ischain_leader_group(device));
		if (instance != i)
			group_leader = group_leader->head->pnext;

		ret = is_hardware_sensor_start(hardware, i, hw_map, group_leader);
		if (ret) {
			merr("is_hardware_stream_on is fail(%d)", device, ret);
			return ret;
		}
	}

	return 0;
}

int is_itf_stream_off_wrap(struct is_device_ischain *device)
{
	struct is_hardware *hardware;
	u32 instance;
	ulong hw_map;
	int ret;
	struct is_group *group_leader;
	unsigned long i;

	dbg_hw(2, "%s\n", __func__);

	hardware = device->hardware;
	instance = device->instance;

	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		hw_map = hardware->hw_map[i];

		group_leader = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN,
				get_ischain_leader_group(device));
		if (instance != i)
			group_leader = group_leader->head->pnext;

		ret = is_hardware_sensor_stop(hardware, i, hw_map, group_leader);
		if (ret) {
			merr("is_hardware_stream_off is fail(%d)", device, ret);
			return ret;
		}
	}

	return ret;
}

int is_itf_process_on_wrap(struct is_device_ischain *device, struct is_group *head)
{
	struct is_hardware *hardware;
	int ret;
	unsigned long i;

	hardware = device->hardware;

	mginfo("%s\n", head, head, __func__);

	mutex_lock(&hardware->itf_lock);

	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		ret = is_hardware_process_start(hardware, i);
		if (ret) {
			merr("is_hardware_process_start is fail(%d)", device, ret);
			mutex_unlock(&hardware->itf_lock);
			return ret;
		}
	}

	mutex_unlock(&hardware->itf_lock);

	return 0;
}

int is_itf_process_off_wrap(struct is_device_ischain *device, struct is_group *head,
	u32 fstop)
{
	struct is_hardware *hardware;
	struct is_groupmgr *groupmgr;
	struct is_group *gnext;
	unsigned long i;

	hardware = device->hardware;
	groupmgr = device->groupmgr;

	mginfo("%s: fstop %d\n", head, head, __func__, fstop);

	mutex_lock(&hardware->itf_lock);

	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		gnext = groupmgr->leader[i];
		while (gnext) {
			if (gnext->head == head)
				CALL_HW_GRP_OPS(gnext, stop, hardware, i, fstop, gnext->hw_slot_id);

			gnext = gnext->gnext;
		}
	}

	mutex_unlock(&hardware->itf_lock);

	return 0;
}

int is_itf_icpu_start_wrap(struct is_device_ischain *device)
{
	return is_hw_icpu_start(device);
}

int is_itf_icpu_stop_wrap(struct is_device_ischain *device)
{
	return is_hw_icpu_stop(device);
}

void is_itf_sudden_stop_wrap(struct is_device_ischain *device, u32 instance, struct is_group *group)
{
	int ret = 0;
	struct is_device_sensor *sensor;
	int index;
	struct is_core *core;
	u32 fstop;

	if (!device) {
		mwarn_hw("%s: device(null)\n", instance, __func__);
		return;
	}

	sensor = device->sensor;
	if (!sensor) {
		mwarn_hw("%s: sensor(null)\n", instance, __func__);
		return;
	}

	core = (struct is_core *)sensor->private_data;
	if (!core) {
		mwarn_hw("%s: core(null)\n", instance, __func__);
		return;
	}

	if (test_bit(IS_SENSOR_FRONT_START, &sensor->state)) {
		for (index = 0; index < IS_SENSOR_COUNT; index++) {
			sensor = &core->sensor[index];
			if (!test_bit(IS_SENSOR_FRONT_START, &sensor->state))
				continue;

			minfo_hw("%s: sensor[%d] sudden close, call sensor_front_stop()\n",
				instance, __func__, index);

			ret = is_sensor_front_stop(sensor, true);
			if (ret)
				merr("is_sensor_front_stop is fail(%d)", sensor, ret);
		}
	}

	if (group) {
		fstop = test_bit(IS_GROUP_FORCE_STOP, &group->state);
		ret = is_itf_process_stop(device, group->head, fstop);
		if (ret)
			mgerr(" is_itf_process_stop is fail(%d)", device, group->head, ret);
	}

	return;
}

int __nocfi is_itf_power_down_wrap(struct is_interface *interface, u32 instance)
{
	int ret = 0;
	struct is_core *core;

	dbg_hw(2, "%s\n", __func__);

	core = (struct is_core *)interface->core;
	if (!core) {
		mwarn_hw("%s: core(null)\n", instance, __func__);
		return ret;
	}

	is_itf_sudden_stop_wrap(&core->ischain[instance], instance, NULL);

	is_interface_lib_shut_down(core->resourcemgr.minfo);

	check_lib_memory_leak();

	return ret;
}

int is_itf_sys_ctl_wrap(struct is_device_ischain *device,
	int cmd, int val)
{
	int ret = 0;

	dbg_hw(2, "%s\n", __func__);

	return ret;
}

int is_itf_sensor_mode_wrap(struct is_device_ischain *device,
	struct is_sensor_cfg *cfg)
{
#ifdef USE_RTA_BINARY
	void *data = NULL;

	dbg_hw(2, "%s\n", __func__);

	if (cfg && cfg->mode == SENSOR_MODE_DEINIT) {
		info_hw("%s: call RTA_SHUT_DOWN\n", __func__);
		is_fpsimd_get_func();
		((rta_shut_down_func_t)RTA_SHUT_DOWN_FUNC_ADDR)(data);
		is_fpsimd_put_func();
	}
#else
	dbg_hw(2, "%s\n", __func__);
#endif

	return 0;
}

bool check_setfile_change(struct is_group *group_leader,
	struct is_group *group, struct is_hardware *hardware,
	u32 instance, u32 scenario)
{
	struct is_group *group_ischain = group;
	struct is_hw_ip *hw_ip = NULL;
	enum exynos_sensor_position sensor_position;

	if (group_leader->id != group->id)
		return false;

	if ((group->device_type == IS_DEVICE_SENSOR)
		&& (group->next)) {
		group_ischain = group->next;
	}

	hw_ip = is_get_hw_ip(group_ischain->id, hardware);
	if (!hw_ip) {
		mgerr("invalid hw_ip", group_ischain, group_ischain);
		return false;
	}

	sensor_position = hardware->sensor_position[instance];
	/* If the 3AA hardware is shared between front preview and reprocessing instance, (e.g. PIP)
	   apply_setfile funciton needs to be called for sensor control. There is two options to check
	   this, one is to check instance change and the other is to check scenario(setfile_index) change.
	   The scenario(setfile_index) is different front preview instance and reprocessing instance.
	   So, second option is more efficient way to support PIP scenario.
	 */
	if (scenario != hw_ip->applied_scenario) {
		msinfo_hw("[G:0x%lx,0x%lx,0x%lx]%s: scenario(%d->%d), instance(%d->%d)\n",
			instance, hw_ip,
			GROUP_ID(group_leader->id), GROUP_ID(group_ischain->id),
			GROUP_ID(group->id), __func__,
			hw_ip->applied_scenario, scenario,
			atomic_read(&hw_ip->instance), instance);
		return true;
	}

	return false;
}

static void __reset_votf(struct is_group *group, struct is_hardware *hardware)
{
	struct is_group *pos;

	for_each_group_child(pos, group) {
		if (is_votf_check_link(pos)) {
			if (CALL_HW_CHAIN_INFO_OPS(hardware, set_votf_reset))
				mgerr("votf reset fail", pos, pos);

			is_votf_destroy_link(pos);
			is_votf_create_link(pos, 0, 0);
		}
	}
}

int is_itf_shot_wrap(struct is_device_ischain *device,
	struct is_group *group, struct is_frame *frame)
{
	struct is_hardware *hardware;
	struct is_interface *itf;
	struct is_group *group_leader;
	u32 instance;
	int ret = 0;
	ulong flags;
	u32 scenario;
	bool apply_flag = false;
	unsigned long i;

	scenario = device->setfile & IS_SETFILE_MASK;
	hardware = device->hardware;
	instance = device->instance;
	itf = device->interface;
	group_leader = get_ischain_leader_group(device);

	apply_flag = check_setfile_change(group_leader, group, hardware, instance, scenario);

	if (!IS_ENABLED(SKIP_SETFILE_LOAD) &&
			(!atomic_read(&group_leader->scount) || apply_flag)) {
		for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
			dbg_hw(1, "[I%lu][G:0x%lx]%s: call apply_setfile()\n", i,
						GROUP_ID(group->id), __func__);

			ret = is_hardware_apply_setfile(hardware, i, scenario,
							hardware->hw_map[i]);
			if (ret) {
				merr("is_hardware_apply_setfile is fail(%d)",
						device, ret);
				return ret;
			}
		}
	}

	memcpy(frame->hw_slot_id, group->hw_slot_id, sizeof(group->hw_slot_id));

	ret = CALL_HW_GRP_OPS(group, shot, hardware, instance, frame);
	if (ret) {
		__reset_votf(group, hardware);
		merr("is_hardware_grp_shot is fail(%d)", device, ret);
		return ret;
	}

	spin_lock_irqsave(&itf->shot_check_lock, flags);
	atomic_set(&itf->shot_check[instance], 1);
	spin_unlock_irqrestore(&itf->shot_check_lock, flags);

	return ret;
}
