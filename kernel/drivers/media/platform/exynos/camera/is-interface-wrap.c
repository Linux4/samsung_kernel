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
#include "pablo-crta-bufmgr.h"
#include "is-device-sensor-peri.h"
#include "is-votfmgr.h"
#include "is-devicemgr.h"

int is_itf_s_param_wrap(struct is_device_ischain *device,
		IS_DECLARE_PMAP(pmap))
{
	struct is_hardware *hardware = &is_get_is_core()->hardware;
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
PST_EXPORT_SYMBOL(is_itf_s_param_wrap);

int is_itf_chg_setfile(struct is_device_ischain *device)
{
	struct is_hardware *hardware = &is_get_is_core()->hardware;
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

	minfo("[ISC:D] %s(%d)\n", device, __func__, device->setfile & IS_SETFILE_MASK);

	return 0;
}

static u32 get_cstat_chain_id(u32 i)
{
	struct is_device_ischain *idi = is_get_ischain_device(i);

	if (test_bit(IS_ISCHAIN_REPROCESSING, &idi->state))
		return 0;

	return idi->group[GROUP_SLOT_3AA]->id - GROUP_ID_3AA0;
}

static int is_hw_crta_buf_put(struct pablo_crta_bufmgr *bufmgr,
				struct pablo_icpu_adt *icpu_adt,
				u32 instance)
{
	int ret;
	u32 buf_idx;
	struct pablo_crta_buf_info buf_info = { 0, };

	CALL_CRTA_BUFMGR_OPS(bufmgr, get_static_buf, &buf_info);
	if (buf_info.dva) {
		ret = CALL_ADT_MSG_OPS(icpu_adt, send_msg_put_buf, instance, PABLO_BUFFER_STATIC, &buf_info);
		if (ret) {
			merr_adt("failed to send_msg_put_buf: %d", instance, ret);
			return ret;
		}
	}

	buf_idx = 0;
	buf_info.dva = 0;
	while (!CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, buf_idx++, false, &buf_info)) {
		if (!buf_info.dva)
			break;
		ret = CALL_ADT_MSG_OPS(icpu_adt, send_msg_put_buf, instance,
				       PABLO_BUFFER_SENSOR_CONTROL, &buf_info);
		if (ret) {
			merr_adt("failed to send_msg_put_buf: %d", instance, ret);
			return ret;
		}
		buf_info.dva = 0;
	}

	return 0;
}

static void is_hw_icpu_close(struct is_device_ischain *idi, u32 max_instance)
{
	unsigned long i;
	u32 ch;
	struct pablo_icpu_adt *icpu_adt = pablo_get_icpu_adt();
	struct pablo_crta_bufmgr *bufmgr = NULL;

	if (max_instance > IS_STREAM_COUNT) {
		mwarn("invalid max_instance(%d)\n", idi, max_instance);
		max_instance = IS_STREAM_COUNT;
	}

	for_each_set_bit(i, &idi->ginstance_map, max_instance) {
		if (CALL_ADT_MSG_OPS(icpu_adt, send_msg_close, i))
			merr("failed to send_msg_close", idi);

		if (CALL_ADT_OPS(icpu_adt, close, i))
			merr("failed to close", idi);

		ch = get_cstat_chain_id(i);

		bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_SS_CTRL, i, ch);
		CALL_CRTA_BUFMGR_OPS(bufmgr, close);

		bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_LAF, i, ch);
		CALL_CRTA_BUFMGR_OPS(bufmgr, close);
	}
}

static int is_hw_icpu_open(struct is_device_ischain *idi, bool rep_flag)
{
	int ret;
	u32 ch;
	u32 f_type;
	unsigned long i;
	bool laser = false;
	struct pablo_icpu_adt *icpu_adt = pablo_get_icpu_adt();
	struct pablo_crta_bufmgr *sc_bufmgr, *laf_bufmgr;
	struct is_device_sensor *ids = idi->sensor;
	struct is_sensor_interface *itf = NULL;

	if (!rep_flag) {
		itf = is_sensor_get_sensor_interface(ids);
		if (!itf) {
			merr("sensor_interface is NULL", idi);
			return -ENODEV;
		}
		laser = itf->cis_itf_ops.is_laser_af_available(itf);
	}

	for_each_set_bit(i, &idi->ginstance_map, IS_STREAM_COUNT) {
		ch = get_cstat_chain_id(i);
		f_type = is_sensor_get_frame_type(i);

		sc_bufmgr = NULL;
		laf_bufmgr = NULL;
		if (!rep_flag && !test_bit(IS_ISCHAIN_MULTI_CH, &(is_get_ischain_device(i)->state))) {
			sc_bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_SS_CTRL, i, ch);
			ret = CALL_CRTA_BUFMGR_OPS(sc_bufmgr, open);
			if (ret) {
				merr("failed to open sc_bufmgr: %d", idi, ret);
				goto err_sc_bufmgr_open;
			}

			if (laser) {
				laf_bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_LAF, i, ch);
				ret = CALL_CRTA_BUFMGR_OPS(laf_bufmgr, open);
				if (ret) {
					merr("failed to open laf_bufmgr: %d", idi, ret);
					goto err_laf_bufmgr_open;
				}
			}
		}

		ret = CALL_ADT_OPS(icpu_adt, open, i, itf, sc_bufmgr, ids->ex_scenario);
		if (ret) {
			merr("failed to open icpu_adt: %d", idi, ret);
			goto err_adt_open;
		}

		ret = CALL_ADT_OPS(icpu_adt, register_get_ch_fn, i, get_cstat_chain_id);
		if (ret) {
			merr("failed to register_get_ch_fn: %d", idi, ret);
			goto err_register_fn;
		}

		ret = CALL_ADT_MSG_OPS(icpu_adt, send_msg_open, i,
					ch, rep_flag, ids->position, f_type);
		if (ret) {
			merr("failed to send_msg_open: %d", idi, ret);
			goto err_send_msg_open;
		}

		ret = is_hw_crta_buf_put(sc_bufmgr, icpu_adt, i);
		if (ret) {
			merr("failed to is_hw_crta_buf_put: %d", idi, ret);
			goto err_crta_buf_put;
		}
	}

	return 0;

err_crta_buf_put:
	if (CALL_ADT_MSG_OPS(icpu_adt, send_msg_close, i))
		merr("failed to send_msg_close", idi);

err_register_fn:
err_send_msg_open:
	if (CALL_ADT_OPS(icpu_adt, close, i))
		merr("failed to close", idi);

err_adt_open:
	if (CALL_CRTA_BUFMGR_OPS(laf_bufmgr, close))
		merr("failed to close laf_bufmgr", idi);

err_laf_bufmgr_open:
	if (CALL_CRTA_BUFMGR_OPS(sc_bufmgr, close))
		merr("failed to close sc_bufmgr", idi);

err_sc_bufmgr_open:
	is_hw_icpu_close(idi, i);

	return ret;
}

static int is_hw_icpu_start(struct is_device_ischain *idi)
{
	int ret = 0;
	unsigned long i;
	u32 ch;
	struct pablo_icpu_adt *icpu_adt = pablo_get_icpu_adt();
	struct pablo_crta_bufmgr *bufmgr = NULL;
	struct pablo_crta_buf_info buf_info = { 0, };

	for_each_set_bit(i, &idi->ginstance_map, IS_STREAM_COUNT) {
		if (!test_bit(IS_ISCHAIN_REPROCESSING, &idi->state)) {
			ch = get_cstat_chain_id(i);
			bufmgr = pablo_get_crta_bufmgr(PABLO_CRTA_BUF_PCSI, i, ch);

			ret = CALL_CRTA_BUFMGR_OPS(bufmgr, get_free_buf, 0, true, &buf_info);
			if (ret) {
				merr("failed to get_free_buf(PCSI%lu/%d)", idi, i, ret);
				return ret;
			}
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
	struct is_group *gnext;
	u32 instance;
	int ret;
	u32 rsccount;
	unsigned long i, ii;

	info_hw("%s: flag(%d)\n", __func__, flag);

	sensor   = device->sensor;
	instance = device->instance;
	hardware = &is_get_is_core()->hardware;

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

		gnext = get_leader_group(i);
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
PST_EXPORT_SYMBOL(is_itf_open_wrap);

int is_itf_close_wrap(struct is_device_ischain *device)
{
	struct is_hardware *hardware;
	struct is_group *gnext;
	u32 instance;
	int ret;
	u32 rsccount;
	unsigned long i;

	dbg_hw(2, "%s\n", __func__);

	hardware = &is_get_is_core()->hardware;
	instance = device->instance;

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

		gnext = get_leader_group(i);
		while (gnext) {
			CALL_HW_GRP_OPS(gnext, close, hardware, i, gnext->hw_slot_id);
			gnext = gnext->gnext;
		}

		hardware->hw_map[i] = 0;
		hardware->logical_hw_map[i] = 0;
		hardware->sensor_position[i] = 0;
	}

	atomic_dec(&hardware->rsccount);

	info("%s: done: hw_map[RSC:%d]\n", __func__, atomic_read(&hardware->rsccount));

	is_hw_icpu_close(device, IS_STREAM_COUNT);

	mutex_unlock(&hardware->itf_lock);

	return 0;
}
PST_EXPORT_SYMBOL(is_itf_close_wrap);

int is_itf_change_chain_wrap(struct is_group *group, u32 next_id)
{
	int ret = 0;
	struct is_hardware *hardware;

	hardware = &is_get_is_core()->hardware;

	mutex_lock(&hardware->itf_lock);

	ret = is_hardware_change_chain(hardware, group, group->instance, next_id);

	mutex_unlock(&hardware->itf_lock);

	mginfo("%s: %s (next_id: %d)\n", group, group, __func__,
		ret ? "fail" : "success", next_id);

	return ret;
}

int is_itf_setfile_wrap(struct is_device_ischain *device)
{
	struct is_hardware *hardware = &is_get_is_core()->hardware;
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

int is_itf_stream_on_wrap(struct is_device_ischain *device)
{
	struct is_hardware *hardware;
	u32 instance;
	ulong hw_map;
	int ret;
	struct is_group *group_leader;
	unsigned long i;

	dbg_hw(2, "%s\n", __func__);

	hardware = &is_get_is_core()->hardware;
	instance = device->instance;

	is_set_ddk_thread_affinity();

	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		hw_map = hardware->hw_map[i];

		group_leader = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN,
				get_ischain_leader_group(device->instance));
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

	hardware = &is_get_is_core()->hardware;
	instance = device->instance;

	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		hw_map = hardware->hw_map[i];

		group_leader = GET_HEAD_GROUP_IN_DEVICE(IS_DEVICE_ISCHAIN,
				get_ischain_leader_group(device->instance));
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

	hardware = &is_get_is_core()->hardware;

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
PST_EXPORT_SYMBOL(is_itf_process_on_wrap);

int is_itf_process_off_wrap(struct is_device_ischain *device, struct is_group *head,
	u32 fstop)
{
	struct is_hardware *hardware;
	struct is_group *gnext;
	unsigned long i;

	hardware = &is_get_is_core()->hardware;

	mginfo("%s: fstop %d\n", head, head, __func__, fstop);

	mutex_lock(&hardware->itf_lock);

	for_each_set_bit(i, &device->ginstance_map, IS_STREAM_COUNT) {
		gnext = get_leader_group(i);
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

int __nocfi is_itf_power_down_wrap(struct is_core *core)
{
	dbg_hw(2, "%s\n", __func__);

	is_itf_sudden_stop_wrap(&core->ischain[0], 0, NULL);

	is_interface_lib_shut_down(core->resourcemgr.minfo);

	check_lib_memory_leak();

	return 0;
}

static void pablo_rta_shut_down(void *data)
{
#if !IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
	((rta_shut_down_func_t)RTA_SHUT_DOWN_FUNC_ADDR)(data);
#endif
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
		pablo_rta_shut_down(data);
		is_fpsimd_put_func();
	}
#else
	dbg_hw(2, "%s\n", __func__);
#endif

	return 0;
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

int is_itf_shot_wrap(struct is_group *group, struct is_frame *frame)
{
	struct is_hardware *hardware;
	struct is_interface *itf;
	u32 instance;
	int ret;
	ulong flags;

	hardware = &is_get_is_core()->hardware;
	instance = group->instance;

	memcpy(frame->hw_slot_id, group->hw_slot_id, sizeof(group->hw_slot_id));

	ret = CALL_HW_GRP_OPS(group, shot, hardware, instance, frame);
	if (ret) {
		__reset_votf(group, hardware);
		mgerr("is_hardware_grp_shot is fail(%d)", group, group, ret);
		return ret;
	}

	itf = &is_get_is_core()->interface;
	spin_lock_irqsave(&itf->shot_check_lock, flags);
	atomic_set(&itf->shot_check[instance], 1);
	spin_unlock_irqrestore(&itf->shot_check_lock, flags);

	return 0;
}

#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
static struct pablo_kunit_itf_wrap_ops pkt_itf_wrap_ops = {
	.change_chain = is_itf_change_chain_wrap,
	.sudden_stop = is_itf_sudden_stop_wrap,
	.sensor_mode = is_itf_sensor_mode_wrap,
	.shot = is_itf_shot_wrap,
};

struct pablo_kunit_itf_wrap_ops *pablo_kunit_get_itf_wrap(void)
{
	return &pkt_itf_wrap_ops;
}
KUNIT_EXPORT_SYMBOL(pablo_kunit_get_itf_wrap);
#endif
