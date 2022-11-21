/*
 *  Copyright (C) 2020, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include "../sensormanager/shub_sensor_manager.h"

#include "../comm/shub_comm.h"
#include "../comm/shub_iio.h"
#include "../debug/shub_system_checker.h"
#include "../debug/shub_debug.h"
#include "../sensor/scontext.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_file_manager.h"
#include "../vendor/shub_vendor.h"
#include "shub_sensor_type.h"
#include "../sensor/sensor.h"

#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/notifier.h>

#define INIT_SENSOR_STATE   0x3FEFF
#define EXECUTE_FUNC(sensor, f) if ((sensor) && (sensor)->funcs && f != NULL) f()

struct sensor_manager_t *sensor_manager;

init_sensor init_sensor_funcs[] = {
	init_accelerometer,
	init_accelerometer_uncal,
	init_step_counter,
	init_magnetometer,
	init_magnetometer_uncal,
	init_flip_cover_detector,
	init_gyroscope,
	init_gyroscope_uncal,
	init_light,
	init_light_cct,
	init_light_ir,
	init_light_seamless,
	init_light_autobrightness,
	init_proximity,
	init_proximity_raw,
	init_proximity_calibration,
	init_pressure,
	init_scontext,
	init_interrupt_gyroscope,
	init_vdis_gyroscope,
	init_step_detector,
	init_magnetometer_power,
	init_significant_motion,
	init_tilt_detector,
	init_pick_up_gesture,
	init_call_gesture,
	init_wake_up_motion,
	init_protos_motion,
	init_pocket_mode,
	init_pocket_mode_lite,
	init_super,
	init_hub_debugger,
	init_thermistor,
	init_tap_tracker,
	init_shake_tracker,
	init_move_detector,
	init_led_cover_event,
	init_device_orientation,
	init_device_orientation_wu,
};

static int make_sensor_instance(void)
{
	int type;
	struct shub_sensor *sensor;

	for (type = 0; type < SENSOR_TYPE_MAX; type++) {
		if (type >= SENSOR_TYPE_LEGACY_MAX && type <= SENSOR_TYPE_SS_BASE)
			continue;

		if (get_sensor_probe_state(type)) {
			sensor = kzalloc(sizeof(struct shub_sensor), GFP_KERNEL);
			if (sensor == NULL) {
				shub_errf("kzalloc failed");
				return -ENOMEM;
			}
			sensor->type = type;
			mutex_init(&sensor->enabled_mutex);
			mutex_lock(&sensor->enabled_mutex);
			sensor->enabled = false;
			sensor->enabled_cnt = 0;
			mutex_unlock(&sensor->enabled_mutex);
			sensor->sampling_period = 0;
			sensor->max_report_latency = 0;
			memset(&(sensor->funcs), 0, sizeof(sensor->funcs));
			if (type > SENSOR_TYPE_SS_BASE) {
				get_ss_sensor_name(type, sensor->name, sizeof(sensor->name));
				sensor->receive_event_size = 0;
				sensor->report_event_size = 0;
			}

			sensor_manager->sensor_list[type] = sensor;
			shub_infof("name %s type %d", sensor->name, sensor->type);
		}
	}

	return 0;
}

static void remove_sensor_list(struct device *dev)
{
	uint64_t i;

	for (i = 0; i < ARRAY_SIZE(init_sensor_funcs); i++)
		init_sensor_funcs[i](false);

	for (i = 0; i < SENSOR_TYPE_MAX; i++) {
		kfree(sensor_manager->sensor_list[i]);
		sensor_manager->sensor_list[i] = NULL;
	}
}

/*
 * enable_sensor - enable sensor
 *	@type : sensor type(defined at shub_sensor_type.h)
 *	@buf : buffer for enable sensor. If buffer is not required(it depends on type), it can be NULL.
 *		   If type is android sensor(not scontext sensor), buf is not used.
 *	@buf_len : length of buf
 *
 * Return: zero on success, else a negative error code.
 */
int enable_sensor(int type, char *buf, int buf_len)
{
	int ret = 0;
	char *send_buffer;
	int send_buffer_len;
	struct shub_sensor *sensor;
	u8 delay_buf[8] = {0, };

	sensor = get_sensor(type);
	if (!sensor) {
		shub_err("sensor(%d) is null", type);
		return -EINVAL;
	}

	mutex_lock(&sensor->enabled_mutex);
	sensor->enabled_cnt++;
	if (sensor->enabled_cnt > 1) {
		shub_infof("%s(%d) is already enabled (%d)", sensor->name, type, sensor->enabled_cnt);
		mutex_unlock(&sensor->enabled_mutex);
		if (type > SENSOR_TYPE_LEGACY_MAX) {
			shub_infof("%s(%d) extra external client added", sensor->name, type);
			ret = shub_send_command(CMD_SETVALUE, type, EXTRA_EXTERNAL_CLIENT_ADDED, buf, buf_len);
		}
		return ret;
	}

	EXECUTE_FUNC(sensor, sensor->funcs->enable);

	if (type < SENSOR_TYPE_LEGACY_MAX) {
		memcpy(&delay_buf[0], &sensor->sampling_period, 4);
		memcpy(&delay_buf[4], &sensor->max_report_latency, 4);
		shub_info("ADD sensor : %s, type %d sampling %d report %d ", sensor->name, type,
			  sensor->sampling_period, sensor->max_report_latency);
		send_buffer = delay_buf;
		send_buffer_len = sizeof(delay_buf);
	} else {
		send_buffer = buf;
		send_buffer_len = buf_len;
		shub_info("ADD scontext : %s, type %d", sensor->name, type);
	}

	if (type != SENSOR_TYPE_SCONTEXT)
		ret = shub_send_command(CMD_ADD, type, 0, send_buffer, send_buffer_len);

	if (ret < 0) {
		shub_errf("commnd error %d", ret);
		sensor->enabled_cnt--;
	} else {
		sensor->enabled = true;
		sensor->enable_timestamp = get_current_timestamp();
		get_tm(&(sensor->enable_time));
	}

	mutex_unlock(&sensor->enabled_mutex);
	return ret;
}

/*
 * disable_sensor - disable sensor
 *	@type : sensor type(defined at shub_sensor_type.h)
 *	@buf : buffer for disable sensor. If buffer is not required(it depends on type), it can be NULL.
 *		  If type is android sensor(not scontext sensor), buf is not used.
 *	@buf_len : length of buf
 *
 * Return: zero on success, else a negative error code.
 */
int disable_sensor(int type, char *buf, int buf_len)
{
	int ret = 0;
	char *send_buffer;
	int send_buffer_len;
	struct shub_sensor *sensor;
	u8 delay_buf[4] = {0, };

	sensor = get_sensor(type);
	if (!sensor) {
		shub_err("sensor(%d) is null", type);
		return -EINVAL;
	}

	mutex_lock(&sensor->enabled_mutex);
	if (!sensor->enabled) { // sensorAllDisabled when device boot.
		shub_infof("%s(%d) is already disabled", sensor->name, type);
		mutex_unlock(&sensor->enabled_mutex);
		return ret;
	}
	sensor->enabled_cnt--;
	if (sensor->enabled_cnt > 0) {
		shub_infof("%s(%d) enabled cnt is %d", sensor->name, type, sensor->enabled_cnt);
		mutex_unlock(&sensor->enabled_mutex);

		if (type > SENSOR_TYPE_LEGACY_MAX) {
			shub_infof("%s(%d) extra external client removed", sensor->name, type);
			ret = shub_send_command(CMD_SETVALUE, type, EXTRA_EXTERNAL_CLIENT_REMOVED, buf, buf_len);
		}
		return ret;
	}

	EXECUTE_FUNC(sensor, sensor->funcs->disable);

	if (type < SENSOR_TYPE_LEGACY_MAX) {
		memcpy(delay_buf, &sensor->sampling_period, 4);
		send_buffer = delay_buf;
		send_buffer_len = sizeof(delay_buf);
		shub_info("REMOVE sensor : %s, type %d", sensor->name, type);
	} else {
		send_buffer = buf;
		send_buffer_len = buf_len;
		shub_info("REMOVE scontext : %s, type %d", sensor->name, type);
	}

	if (type != SENSOR_TYPE_SCONTEXT)
		ret = shub_send_command(CMD_REMOVE, type, 0, send_buffer, send_buffer_len);

	if (ret < 0) {
		shub_errf("commnd error %d", ret);
		sensor->enabled_cnt++;
	} else {
		struct sensor_event *event = &sensor->event_buffer;

		sensor->enabled = false;
		sensor->disable_timestamp = get_current_timestamp();
		get_tm(&(sensor->disable_time));
		if (event) {
			if (event->value)
				memset(event->value, 0, sizeof(*(event->value)));
			event->timestamp = 0;
			event->received_timestamp = 0;
		}

	}

	mutex_unlock(&sensor->enabled_mutex);

	return ret;
}

int batch_sensor(int type, uint32_t sampling_period, uint32_t max_report_latency)
{
	int ret = 0;
	u8 buf[8] = {0, };
	struct shub_sensor *sensor = get_sensor(type);

	if (!sensor) {
		shub_err("sensor(%d) is null", type);
		return -EINVAL;
	}

	if (sensor && sensor->funcs && sensor->funcs->batch != NULL)
		sensor->funcs->batch(sampling_period, max_report_latency);

	if (sensor->enabled &&
	    (sensor->sampling_period != sampling_period || sensor->max_report_latency != max_report_latency)) {
		shub_infof("CHANGE RATE %s, %d(%d, %d)", sensor->name, type, sampling_period, max_report_latency);
		memcpy(&buf[0], &sampling_period, 4);
		memcpy(&buf[4], &max_report_latency, 4);
		if (type != SENSOR_TYPE_SCONTEXT) {
			ret = shub_send_command(CMD_CHANGERATE, type, 0, buf, sizeof(buf));
			if (ret < 0)
				shub_errf("shub_send_command failed");
		}
	}

	sensor->sampling_period = sampling_period;
	sensor->max_report_latency = max_report_latency;

	return ret;
}

int flush_sensor(int type)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(type);

	if (!sensor) {
		shub_err("sensor(%d) is null", type);
		return -EINVAL;
	}

	if (!get_sensor_enabled(type)) {
		shub_infof("%s(%d) is not enabled", sensor->name, type);
		return -EINVAL;
	}

	if (type != SENSOR_TYPE_SCONTEXT)
		ret = shub_send_command(CMD_GETVALUE, type, SENSOR_FLUSH, NULL, 0);

	if (ret < 0)
		shub_errf("fail %d", ret);

	return ret;
}

int inject_sensor_additional_data(int type, char *buf, int buf_len)
{
	int ret = 0;
	struct shub_sensor *sensor = get_sensor(type);

	if (!sensor) {
		shub_err("sensor(%d) is null", type);
		return -EINVAL;
	}

	if (sensor->funcs && sensor->funcs->inject_additional_data)
		ret = sensor->funcs->inject_additional_data(buf, buf_len);

	return ret;
}

void print_sensor_debug(int type)
{
	struct shub_sensor *sensor = get_sensor(type);
	if (!sensor)
		return;

	EXECUTE_FUNC(sensor, sensor->funcs->print_debug);
	if (sensor->funcs == NULL || sensor->funcs->print_debug == NULL) {
		if (type <= SENSOR_TYPE_LEGACY_MAX) {
			shub_info("%s(%u) : %ums, %dms(%lld)", sensor->name, type, sensor->sampling_period,
				  sensor->max_report_latency, sensor->event_buffer.timestamp);
		} else {
			shub_info("%s(%u), last event ts = (%lld)",
				  sensor->name, (type - SENSOR_TYPE_SS_BASE), sensor->event_buffer.timestamp);
		}
	}
}

int get_sensor_value(int type, char *dataframe, int *index, struct sensor_event *event, int frame_len)
{
	struct shub_sensor *sensor = get_sensor(type);
	int receive_event_size;
	int ret = 0;
	u64 current_timestamp = get_current_timestamp();
#ifdef CONFIG_SHUB_DEBUG
	char buf[255];
	int buf_len;
#endif

	if (!sensor)
		return -EINVAL;

	if (sensor->receive_event_size != 0) {
		if (*index + sensor->receive_event_size > frame_len)
			return -EINVAL;
	}

	if (sensor->funcs && sensor->funcs->get_sensor_value) {
		ret = sensor->funcs->get_sensor_value(dataframe, index, event, frame_len);
		if (ret < 0)
			return ret;
	} else {
		receive_event_size = sensor->receive_event_size;
		memcpy(event->value, dataframe + *index, receive_event_size);
		*index += receive_event_size;
	}

	if (*index + sizeof(event->timestamp) > frame_len)
		return -EINVAL;

	memcpy(&event->timestamp, dataframe + *index, 8);
	*index += 8;
#ifdef CONFIG_SHUB_DEBUG
	if (check_debug_log_state(SHUB_LOG_EVENT_TIMESTAMP)) {
		buf_len = snprintf(buf, sizeof(buf), "ts (%d) %lld %lld %lld",
				   type, current_timestamp, event->timestamp, current_timestamp - event->timestamp);
		print_log_debug(buf, buf_len, current_timestamp);
	}
#endif

	event->received_timestamp = current_timestamp;
	if (event->timestamp > current_timestamp)
		event->timestamp = current_timestamp;

	return ret;
}

int parsing_bypass_data(char *dataframe, int *index, int frame_len)
{
	u16 batch_event_count;
	int type = dataframe[(*index)++];
	struct shub_sensor *sensor;
	struct sensor_event *event;

	if ((type < 0) || (type >= SENSOR_TYPE_LEGACY_MAX)) {
		shub_errf("Parsing error : Mcu bypass dataframe err %d", type);
		return -1;
	}

	sensor = get_sensor(type);
	if (!sensor) {
		shub_errf("Parsing error : sensor(%d) is null", type);
		return -1;
	}

	event = get_sensor_event(type);
	memcpy(&batch_event_count, dataframe + (*index), 2);
	(*index) += 2;

	do {
		if (get_sensor_value(type, dataframe, index, event, frame_len) < 0) {
			shub_errf("Parsing error : sensor(%d) event error", type);
			return -EINVAL;
		}
		EXECUTE_FUNC(sensor, sensor->funcs->report_event);
		shub_report_sensordata(type, event->timestamp, event->value, sensor->report_event_size);
#ifdef CONFIG_SHUB_DEBUG
		shub_system_check_lock();
		if (is_system_checking())
			event_test_cb(type, event->timestamp);
		if (is_event_order_checking())
			order_test_cb(type, event->timestamp);
		shub_system_check_unlock();
#endif
		batch_event_count--;
	} while ((batch_event_count > 0) && ((*index) < frame_len));

	return 0;
}

int parsing_scontext_data(char *dataframe, int *index, int frame_len)
{
	int length;

	memcpy(&length, dataframe + (*index), 2);
	(*index) += 2;

	if (*index + length > frame_len || length < 0) {
		shub_errf("error");
		return -1;
	}

	shub_report_scontext_data(dataframe + (*index), length);
	(*index) += length;

	return 0;
}

int parsing_meta_data(char *dataframe, int *index, int frame_len)
{
	int ret = 0;
	struct shub_sensor *sensor;
	int what, type;
	int event_size;
	char *meta_event;

	if (*index + 2 > frame_len) {
		shub_errf("parsing error index = %d, frame_len %d", *index, frame_len);
		return -EINVAL;
	}

	what = dataframe[(*index)++];
	type = dataframe[(*index)++];

	if ((type < 0) || (type >= SENSOR_TYPE_LEGACY_MAX)) {
		shub_errf("Parsing error : meta data sensor dataframe err %d %d", what, type);
		return -EINVAL;
	}

	sensor = get_sensor(type);
	if (!sensor)
		return -EINVAL;

	event_size = sensor->report_event_size;
	meta_event = kzalloc(event_size, GFP_KERNEL);

	if (meta_event != NULL) {
		memset(meta_event, 0, event_size);
		shub_infof("what : %d, sensor : %d", what, type);
		shub_report_sensordata(type, 0, meta_event, event_size);
	} else {
		shub_errf("failed to alloc");
	}

#ifdef CONFIG_SHUB_DEBUG
	shub_system_check_lock();
	if (is_system_checking())
		comm_test_cb(type);
	shub_system_check_unlock();
#endif

	kfree(meta_event);
	return ret;
}

uint64_t get_sensors_legacy_probe_state(void)
{
	return sensor_manager->sensor_probe_state[0];
}

uint64_t get_sensors_legacy_enable_state(void)
{
	int type;
	uint64_t en_state = 0;

	for (type = 0; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		if (get_sensor_enabled(type))
			en_state |= (1ULL << type);
	}

	return en_state;
}

int get_sensors_scontext_probe_state(uint64_t *buf)
{
	int size = sizeof(sensor_manager->sensor_probe_state) - sizeof(sensor_manager->sensor_probe_state[0]);

	memcpy(buf, &sensor_manager->sensor_probe_state[1], size);
	return size;
}

bool get_sensor_probe_state(int type)
{
	if (type == SENSOR_TYPE_SCONTEXT || type == SENSOR_TYPE_SENSORHUB || type == SENSOR_TYPE_HUB_DEBUGGER)
		return true;

	if (type < SENSOR_TYPE_LEGACY_MAX)
		return (sensor_manager->sensor_probe_state[0]) & (1ULL << type);

	if (type > SENSOR_TYPE_SS_BASE && type < SENSOR_TYPE_SS_MAX) {
		int32_t ss_type = type - SENSOR_TYPE_SS_BASE;
		int32_t interval = sizeof(sensor_manager->sensor_probe_state[1]) * BITS_PER_BYTE;
		int32_t index = ss_type < interval ? 1 : 2;

		return (sensor_manager->sensor_probe_state[index]) & (1ULL << (ss_type % interval));
	}

	return false;
}

struct shub_sensor *get_sensor(int type)
{
	if (type <= 0 || type >= SENSOR_TYPE_MAX || !get_sensor_probe_state(type))
		return NULL;

	return sensor_manager->sensor_list[type];
}

bool get_sensor_enabled(int type)
{
	struct shub_sensor *sensor = get_sensor(type);

	if (!sensor)
		return false;

	return sensor->enabled;
}

struct sensor_event *get_sensor_event(int type)
{
	struct shub_sensor *sensor = get_sensor(type);

	if (!sensor)
		return NULL;

	return &sensor->event_buffer;
}

int init_sensor_manager(struct device *dev)
{
	sensor_manager = kzalloc(sizeof(struct sensor_manager_t), GFP_KERNEL);
	if (!sensor_manager)
		return -ENOMEM;

	memset(sensor_manager, 0x00, sizeof(struct sensor_manager_t));
	return 0;
}

void exit_sensor_manager(struct device *dev)
{
	remove_sensor_list(dev);
}

int open_sensors_calibration(void)
{
	int i;
	//if (!sensor_manager->is_fs_ready)
	//	return 0;

	shub_infof();
	for (i = 0; i < SENSOR_TYPE_MAX; i++) {
		struct shub_sensor *sensor = sensor_manager->sensor_list[i];

		EXECUTE_FUNC(sensor, sensor->funcs->open_calibration_file);
	}

	return 0;
}

int sync_sensors_attribute(void)
{
	int type;

	if (!sensor_manager->is_fs_ready)
		return 0;

	shub_infof();
	for (type = 0; type < SENSOR_TYPE_MAX; type++) {
		struct shub_sensor *sensor = get_sensor(type);

		EXECUTE_FUNC(sensor, sensor->funcs->sync_status);
	}

	return 0;
}

static void sync_sensors_enable_state(void)
{
	int type;

	shub_infof();

	udelay(10);
	for (type = 0; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		struct shub_sensor *sensor = get_sensor(type);

		if (sensor && sensor->enabled) {
			mutex_lock(&sensor->enabled_mutex);
			sensor->enabled_cnt = 0; // Todo ssadai Temp code. It should be decided by kernel engineer.
			mutex_unlock(&sensor->enabled_mutex);
			enable_sensor(type, NULL, 0);
			udelay(10);
		}
	}
}

static void set_sensor_probe_state(void)
{
	struct shub_system_info *system_info = get_shub_system_info();

	memcpy(sensor_manager->sensor_probe_state, system_info->scan, sizeof(sensor_manager->sensor_probe_state));

	shub_info("probe state 0x%llx, 0x%llx, 0x%llx", sensor_manager->sensor_probe_state[0],
		  sensor_manager->sensor_probe_state[1], sensor_manager->sensor_probe_state[2]);
}

static inline int get_probed_legacy_count(void)
{
	int type = 0, count = 0;

	for (type = 0; type < SENSOR_TYPE_LEGACY_MAX; type++) {
		if ((sensor_manager->sensor_probe_state[0]) & (1ULL << type))
			count++;
	}
	return count;
}

int get_sensor_spec_from_hub(void)
{
	int probe_cnt = get_probed_legacy_count();
	int ret = 0;
	char *buffer = NULL;
	unsigned int buffer_length;
	int i = 0, count;
	struct sensor_spec_t *specs = NULL;

	ret = shub_send_command_wait(CMD_GETVALUE, TYPE_MCU, SENSOR_SPEC, 1000, NULL, 0, &buffer, &buffer_length, true);

	if (ret < 0 || buffer_length < 1) {
		shub_errf("fail %d", ret);
		return ret;
	}

	count = (int)(buffer[0]);
	if (buffer_length != (sizeof(struct sensor_spec_t) * count) + 1) {
		shub_errf("buffer length error %d", buffer_length);
		return -EINVAL;
	} else if (probe_cnt != count) {
		shub_errf("spec count error probe count %d spec count", probe_cnt, count);
	}

	specs = (struct sensor_spec_t *)(buffer + 1);

	for (i = 0; i < count; i++) {
		struct shub_sensor *sensor = get_sensor(specs[i].uid);

		shub_info("id(%d), name(%s), vendor(%d)", specs[i].uid, specs[i].name, specs[i].vendor);

		if (sensor)
			memcpy(&sensor->spec, &specs[i], sizeof(struct sensor_spec_t));
		else if (specs[i].uid != 0)
			shub_errf("sensor is probed, but sensor spec is reported");
	}

	return ret;
}

unsigned int get_total_sensor_spec(char *buf)
{
	unsigned int type;
	unsigned int len = 0;

	for (type = 0 ; type < SENSOR_TYPE_MAX ; type++) {
		struct shub_sensor *sensor = get_sensor(type);

		if (sensor && sensor->spec.uid == type) {
			memcpy(buf + len, &sensor->spec, sizeof(sensor->spec));
			len += sizeof(sensor->spec);
		}
	}

	return len;
}

void get_sensor_vendor_name(int vendor_type, char *vendor_name)
{
	char *vendor_list[VENDOR_MAX] = VENDOR_LIST;

	strcpy(vendor_name, vendor_list[vendor_type]);
}

static void init_sensors(void)
{
	uint64_t i;
	int ret;

	for (i = 0; i < ARRAY_SIZE(init_sensor_funcs); i++)
		init_sensor_funcs[i](true);

	for (i = 0; i < SENSOR_TYPE_LEGACY_MAX; i++) {
		struct shub_sensor *sensor = get_sensor(i);

		if (sensor) {
			if (sensor->receive_event_size != 0 && !sensor->event_buffer.value) { /* there is a error from init_sensor_funcs */
				shub_errf("%d has error", i);
				kfree(sensor);
				sensor_manager->sensor_list[i] = NULL;
			} else if (sensor->funcs && sensor->funcs->init_chipset) {
				ret = sensor->funcs->init_chipset();
				if (ret < 0) {
					kfree(sensor);
					sensor_manager->sensor_list[i] = NULL;
				}
			}
		}
	}
}

static int fm_ready_sensors(struct notifier_block *this, unsigned long event, void *ptr)
{
	shub_infof("notify event %d", event);

	open_sensors_calibration();
	sync_sensors_attribute();

	return NOTIFY_STOP;
}

static struct notifier_block fm_notifier = {
	.notifier_call = fm_ready_sensors,
};

int refresh_sensors(struct device *dev)
{
	int ret = 0;
	static bool first = true;

	shub_infof();

	set_sensor_probe_state();

	if (first) {
		ret = make_sensor_instance();
		if (ret < 0) {
			shub_errf("make_sensor_instance failed");
			return ret;
		}
	}

	ret = get_sensor_spec_from_hub();
	if (ret < 0) {
		shub_errf("get_sensor_spec_from_hub failed");
		return ret;
	}

	if (first) {
		init_sensors();
		register_file_manager_ready_callback(&fm_notifier);
		initialize_indio_dev(dev);
		first = false;
	} else {
		init_scontext_enable_state();
		shub_report_scontext_notice_data(SCONTEXT_AP_STATUS_RESET);
	}

	sync_sensors_attribute();
	sync_sensors_enable_state();

	return ret;
}

void fs_ready_cb(void)
{
	sensor_manager->is_fs_ready = true;
	sensorhub_fs_ready();
}
