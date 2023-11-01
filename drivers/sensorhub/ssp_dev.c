/*
 *  Copyright (C) 2015, Samsung Electronics Co. Ltd. All Rights Reserved.
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/version.h>

#include "ssp_dev.h"
#include "ssp.h"
#include "ssp_debug.h"
#include "ssp_sysfs.h"
#include "ssp_iio.h"
#include "ssp_sensorlist.h"
#include "ssp_data.h"
#include "ssp_comm.h"
#include "ssp_scontext.h"
#include "ssp_cmd_define.h"
#include "ssp_platform.h"
#include "ssp_injection.h"
#include "ssp_system_checker.h"
#ifdef CONFIG_SEC_VIB_NOTIFIER
#include <linux/vibrator/sec_vibrator_notifier.h>
#include "ssp_motor.h"
#endif
#ifdef CONFIG_SENSORS_SSP_DUMP
#include "ssp_dump.h"
#endif

#define NORMAL_SENSOR_STATE_K   0x3FEFF

struct ssp_data *m_ssp_data;

struct ssp_data *get_ssp_data(void)
{
	return m_ssp_data;
}

static void init_sensorlist(struct ssp_data *data)
{
	struct sensor_info sensorinfo[SENSOR_TYPE_MAX] = {
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_ACCELEROMETER,
		SENSOR_INFO_GEOMAGNETIC_FIELD,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_GYRO,
		SENSOR_INFO_LIGHT,
		SENSOR_INFO_PRESSURE,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_PROXIMITY,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_ROTATION_VECTOR,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_MAGNETIC_FIELD_UNCALIBRATED,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_GYRO_UNCALIBRATED,
		SENSOR_INFO_SIGNIFICANT_MOTION,
		SENSOR_INFO_STEP_DETECTOR,
		SENSOR_INFO_STEP_COUNTER,
		SENSOR_INFO_GEOMAGNETIC_ROTATION_VECTOR,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_TILT_DETECTOR,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_PICK_UP_GESTURE,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_DEVICE_ORIENTATION,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_PROXIMITY_RAW,
		SENSOR_INFO_GEOMAGNETIC_POWER,
		SENSOR_INFO_INTERRUPT_GYRO,
		SENSOR_INFO_SCONTEXT,
		SENSOR_INFO_SENSORHUB,
		SENSOR_INFO_LIGHT_CCT,
		SENSOR_INFO_CALL_GESTURE,
		SENSOR_INFO_WAKE_UP_MOTION,
		SENSOR_INFO_AUTO_BIRGHTNESS,
		SENSOR_INFO_VDIS_GYRO,
		SENSOR_INFO_POCKET_MODE_LITE,
		SENSOR_INFO_PROXIMITY_CALIBRATION,
		SENSOR_INFO_PROTOS_MOTION, // 14
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_ACCELEROMETER_UNCALIBRATED,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_DEVICE_ORIENTATION_WU,
		SENSOR_INFO_UNKNOWN,
		SENSOR_INFO_SAR_BACKOFF_MOTION,
	};

	memcpy(&data->info, sensorinfo, sizeof(data->info));
}

#ifdef CONFIG_SEC_VIB_NOTIFIER
static int vibrator_notifier(struct notifier_block *nb, unsigned long action, void *data)
{
	struct vib_notifier_context *vib_data = (struct vib_notifier_context *)data;
	bool enable = (bool)action;
	int index = vib_data->index;
	int duration = vib_data->timeout;

	ssp_infof("enable %d, index %d, duration %d", enable, index, duration);
	setSensorCallback(enable, duration);
	return 0;
}

static struct notifier_block vib_notifier = {
	.notifier_call = vibrator_notifier,
};
#endif

static void initialize_variable(struct ssp_data *data)
{
	int type;
#ifdef CONFIG_SENSORS_SSP_LIGHT
	int light_coef[7] = {-947, -425, -1777, 1754, 3588, 1112, 1370};
#endif

	ssp_infof("");

	init_sensorlist(data);

	for (type = 0; type < SENSOR_TYPE_MAX; type++) {
		data->delay[type].sampling_period = DEFAULT_POLLING_DELAY;
		data->delay[type].max_report_latency = 0;
	}

	data->sensor_probe_state = NORMAL_SENSOR_STATE_K;

	data->cnt_reset = -1;
	for (type = 0 ; type <= RESET_TYPE_MAX ; type++)
		data->cnt_ssp_reset[type] = 0;
	data->check_noevent_reset_cnt = -1;
	data->reset_type = RESET_TYPE_MAX;

	data->last_resume_status = SCONTEXT_AP_STATUS_RESUME;

	INIT_LIST_HEAD(&data->pending_list);

#ifdef CONFIG_SENSORS_SSP_LIGHT
	memcpy(data->light_coef, light_coef, sizeof(light_coef));
	data->camera_lux_en = false;
	data->brightness = -1;
#endif

#ifdef CONFIG_SENSORS_SSP_MAGNETIC
	data->geomag_cntl_regdata = 1;
	data->is_geomag_raw_enabled = false;

	/* STATUS AK09916C doesn't need FuseRomdata more*/
	data->uFuseRomData[0] = 0;
	data->uFuseRomData[1] = 0;
	data->uFuseRomData[2] = 0;
#endif

#ifdef CONFIG_SENSORS_SSP_PROXIMITY
#if defined(CONFIG_SENSROS_SSP_PROXIMITY_THRESH_CAL)
	data->prox_cal_mode = 0;
#endif
#ifdef CONFIG_SENSORS_SSP_PROXIMITY_MODIFY_SETTINGS
	data->prox_setting_mode = 1;
#endif
#endif
}

int open_sensor_calibration_data(struct ssp_data *data)
{
#ifdef CONFIG_SENSORS_SSP_GYROSCOPE
	gyro_open_calibration(data);
#endif
#ifdef CONFIG_SENSORS_SSP_BAROMETER
	pressure_open_calibration(data);
#endif
#ifdef CONFIG_SENSORS_SSP_ACCELOMETER
	accel_open_calibration(data);
#endif

#ifdef CONFIG_SENSORS_SSP_PROXIMITY
#if defined(CONFIG_SENSROS_SSP_PROXIMITY_THRESH_CAL) || defined(CONFIG_SENSORS_SSP_PROXIMITY_FACTORY_CROSSTALK_CAL)
	proximity_open_calibration(data);
#endif
#ifdef CONFIG_SENSORS_SSP_PROXIMITY_MODIFY_SETTINGS
	open_proximity_setting_mode(data);
#endif
#endif

#ifdef CONFIG_SENSORS_SSP_MAGNETIC
	mag_open_calibration(data);
#endif
	return 0;
}

int sync_sensor_data(struct ssp_data *data)
{
	int i;
	typedef int (*sync_function)(struct ssp_data *);
	sync_function sync_funcs[] = {
#ifdef CONFIG_SENSORS_SSP_GYROSCOPE
		set_gyro_cal,
#endif
#ifdef CONFIG_SENSORS_SSP_ACCELOMETER
		set_accel_cal,
		set_device_orientation_mode,
#endif
#ifdef CONFIG_SENSORS_SSP_LIGHT
		set_light_coef,
		set_light_brightness,
		set_light_ab_camera_hysteresis,
#ifdef CONFIG_SENSORS_SSP_LIGHT_JPNCONCEPT
		set_light_region,
#endif
#endif
#ifdef CONFIG_SENSORS_SSP_PROXIMITY
		set_proximity_threshold,
#ifdef CONFIG_SENSORS_SSP_PROXIMITY_MODIFY_SETTINGS
		set_proximity_setting_mode,
#endif
#endif
#ifdef CONFIG_SENSORS_SSP_MAGNETIC
		set_pdc_matrix,
		set_mag_cal,
#endif
	};

	ssp_infof("");
	for (i = 0 ; i < ARRAY_SIZE(sync_funcs) ; i++) {
		if (sync_funcs[i](data) < 0) {
			break;
			return FAIL;
		}
	}

	return 0;
}

int initialize_mcu(struct ssp_data *data)
{
	int ret = 0;

	//ssp_dbgf();
	ssp_infof();

	clean_pending_list(data);

	ssp_infof("is_working = %d", is_sensorhub_working(data));

	ret = get_sensor_scanning_info(data);
	if (ret < 0) {
		ssp_errf("get_sensor_scanning_info failed");
		return FAIL;
	}

	ret = get_firmware_rev(data);
	if (ret < 0) {
		ssp_errf("get firmware rev");
		return FAIL;
	}

	ret = set_sensor_position(data);
	if (ret < 0) {
		ssp_errf("set_sensor_position failed");
		return FAIL;
	}

	ssp_infof("is_fs_read %d", data->is_fs_ready);
	if (data->is_fs_ready) {
		ret = sync_sensor_data(data);
		if (ret < 0) {
			ssp_errf("sync_sensors_data failed");
			return FAIL;
		}
	}

#ifdef CONFIG_SEC_VIB_NOTIFIER
	if (data->accel_motor_coef > 0)
		sync_motor_state();
#endif
	ssp_send_status(data, data->last_resume_status);
	if (data->last_ap_status != 0)
		ssp_send_status(data, data->last_ap_status);

	return SUCCESS;
}

static void sync_sensor_state(struct ssp_data *data)
{
	u32 uSensorCnt;

	udelay(10);

	for (uSensorCnt = 0; uSensorCnt < SENSOR_TYPE_MAX; uSensorCnt++) {
		mutex_lock(&data->enable_mutex);
		if (atomic64_read(&data->sensor_en_state) & (1ULL << uSensorCnt)) {
			enable_legacy_sensor(data, uSensorCnt);
			udelay(10);
		}
		mutex_unlock(&data->enable_mutex);
	}
}

static void save_reset_info(struct ssp_data *data)
{
	data->reset_info.timestamp = get_current_timestamp();
	get_tm(&(data->reset_info.time));
	if (data->reset_type < RESET_TYPE_MAX)
		data->reset_info.reason = data->reset_type;
	else
		data->reset_info.reason = RESET_TYPE_HUB_CRASHED;
	data->reset_type = RESET_TYPE_MAX;
}

void refresh_task(struct work_struct *work)
{
	struct ssp_data *data = container_of((struct delayed_work *)work,
					     struct ssp_data, work_refresh);
	if (!is_sensorhub_working(data)) {
		ssp_errf("ssp is not working");
		return;
	}

	__pm_stay_awake(data->ssp_wakelock);
	ssp_infof();
	data->cnt_reset++;
	save_reset_info(data);

	if (data->sensor_spec) {
		ssp_infof("prev sensor_spec free");
		kfree(data->sensor_spec);
		data->sensor_spec = NULL;
		data->sensor_spec_size = 0;
	}

	clean_pending_list(data);

	sensorhub_refresh_func(data);

	if (initialize_mcu(data) < 0) {
		ssp_errf("initialize_mcu is failed. stop refresh task");
		goto exit;
	}
#ifdef CONFIG_SSP_ENG_DEBUG
	if (is_system_checking())
		goto exit;
#endif
	sync_sensor_state(data);
	report_scontext_notice_data(data, SCONTEXT_AP_STATUS_RESET);
	enable_timestamp_sync_timer(data);

exit:
	__pm_relax(data->ssp_wakelock);
	ssp_wake_up_wait_event(&data->reset_lock);
#ifdef CONFIG_SSP_ENG_DEBUG
	if (is_system_checking())
		system_ready_cb();
#endif
}

int queue_refresh_task(struct ssp_data *data, int delay)
{
	cancel_delayed_work_sync(&data->work_refresh);

	ssp_infof();
	queue_delayed_work(data->debug_wq, &data->work_refresh,
			   msecs_to_jiffies(delay));
	return SUCCESS;
}

static int ssp_parse_dt(struct device *dev, struct ssp_data *data)
{
	struct device_node *np = dev->of_node;
	int sensor_ldo_en;
	int ret;
	enum of_gpio_flags flags;

	/* sensor_ldo_en */
	sensor_ldo_en = of_get_named_gpio_flags(np, "sensor-ldo-en", 0, &flags);
	ssp_info("sensor ldo en = %d", sensor_ldo_en);

	if (sensor_ldo_en >= 0) {
		ret = gpio_request(sensor_ldo_en, "sensor_ldo_en");
		if (ret) {
			ssp_err("failed to request sensor_ldo_en, ret:%d\n", ret);
			return ret;
		}

		ret = gpio_direction_output(sensor_ldo_en, 1);
		if (ret) {
			ssp_err("failed set sensor_ldo_en as output mode, ret:%d", ret);
			return ret;
		}
		gpio_set_value_cansleep(sensor_ldo_en, 1);
	}

	/* sensor positions */
#ifdef CONFIG_SENSORS_SSP_ACCELOMETER
	if (of_property_read_u32(np, "ssp-acc-position", &data->accel_position))
		data->accel_position = 0;

	ssp_info("acc-posi[%d]", data->accel_position);

	if (of_property_read_u32(np, "ssp-acc-motor-coef", &data->accel_motor_coef))
		data->accel_motor_coef = 0;

	ssp_info("acc-acc-motor-coef[%d]", data->accel_motor_coef);
#endif

#ifdef CONFIG_SENSORS_SSP_GYROSCOPE
	if (of_property_read_u32(np, "ssp-gyro-position", &data->gyro_position)) {

#ifdef CONFIG_SENSORS_SSP_ACCELOMETER
		data->gyro_position = data->accel_position;
#else
		data->gyro_position = 0;
#endif
	}
	ssp_info("gyro-posi[%d]", data->gyro_position);

#endif

#ifdef CONFIG_SENSORS_SSP_MAGNETIC
	if (of_property_read_u32(np, "ssp-mag-position", &data->mag_position))
		data->mag_position = 0;

	ssp_info("mag-posi[%d]", data->mag_position);

#endif

#ifdef CONFIG_SENSORS_SSP_PROXIMITY
#if defined(CONFIG_SENSORS_SSP_PROXIMITY_AUTO_CAL_TMD3725)
	/* prox thresh */
	if (of_property_read_u8_array(np, "ssp-prox-thresh",
					  data->prox_thresh, PROX_THRESH_SIZE))
		ssp_err("no prox-thresh, set as 0");

	ssp_info("prox-thresh - %u, %u, %u, %u", data->prox_thresh[PROX_THRESH_HIGH],
		 data->prox_thresh[PROX_THRESH_LOW],
		 data->prox_thresh[PROX_THRESH_DETECT_HIGH], data->prox_thresh[PROX_THRESH_DETECT_LOW]);
#else
	/* prox thresh */
	if (of_property_read_u16_array(np, "ssp-prox-thresh",
					   data->prox_thresh, PROX_THRESH_SIZE))
		ssp_err("no prox-thresh, set as 0");

	ssp_info("prox-thresh - %u, %u", data->prox_thresh[PROX_THRESH_HIGH],
		 data->prox_thresh[PROX_THRESH_LOW]);
#endif

#if defined(CONFIG_SENSROS_SSP_PROXIMITY_THRESH_CAL)
	/* prox thresh additional value */
	if (of_property_read_u16_array(np, "ssp-prox-thresh-addval", data->prox_thresh_addval,
					   ARRAY_SIZE(data->prox_thresh_addval)))
		ssp_err("no prox-thresh_addval, set as 0");

	ssp_info("prox-thresh-addval - %u, %u", data->prox_thresh_addval[PROX_THRESH_HIGH],
		 data->prox_thresh_addval[PROX_THRESH_LOW]);
#endif

#ifdef CONFIG_SENSORS_SSP_PROXIMITY_MODIFY_SETTINGS
	if (of_property_read_u16_array(np, "ssp-prox-setting-thresh",
					   data->prox_setting_thresh, 2))
		ssp_err("no prox-setting-thresh, set as 0");

	ssp_info("prox-setting-thresh - %u, %u", data->prox_setting_thresh[0],
		 data->prox_setting_thresh[1]);

	if (of_property_read_u16_array(np, "ssp-prox-mode-thresh",
					   data->prox_mode_thresh, PROX_THRESH_SIZE))
		ssp_err("no prox-mode-thresh, set as 0");

	ssp_info("prox-mode-thresh - %u, %u", data->prox_mode_thresh[PROX_THRESH_HIGH],
		 data->prox_mode_thresh[PROX_THRESH_LOW]);

#endif

#ifdef CONFIG_SENSORS_SSP_PROXIMITY_FACTORY_CROSSTALK_CAL
	if (of_property_read_u16(np, "ssp-prox-cal-add-value", &data->prox_cal_add_value))
		ssp_err("no prox-cal-add-value, set as 0");

	ssp_info("prox-cal-add-value - %u", data->prox_cal_add_value);

	if (of_property_read_u16_array(np, "ssp-prox-cal-thresh", data->prox_cal_thresh, 2))
		ssp_err("no prox-cal-thresh, set as 0");

	ssp_info("prox-cal-thresh - %u, %u", data->prox_cal_thresh[0], data->prox_cal_thresh[1]);

	data->prox_thresh_default[0] = data->prox_thresh[0];
	data->prox_thresh_default[1] = data->prox_thresh[1];
#endif

#endif

#ifdef CONFIG_SENSORS_SSP_LIGHT
	if (of_property_read_u32_array(np, "ssp-light-position",
					   data->light_position, ARRAY_SIZE(data->light_position)))
		ssp_err("no ssp-light-position, set as 0");


	ssp_info("light-position - %u.%u %u.%u %u.%u",
		 data->light_position[0], data->light_position[1],
		 data->light_position[2], data->light_position[3],
		 data->light_position[4], data->light_position[5]);

	if (of_property_read_u32_array(np, "ssp-light-cam-lux",
					   data->camera_lux_hysteresis, ARRAY_SIZE(data->camera_lux_hysteresis))) {
		ssp_err("no ssp-light-cam-high");
		data->camera_lux_hysteresis[0] = -1;
		data->camera_lux_hysteresis[1] = 0;
	}

	if (of_property_read_u32_array(np, "ssp-light-cam-br",
					   data->camera_br_hysteresis, ARRAY_SIZE(data->camera_br_hysteresis))) {
		ssp_err("no ssp-light-cam-low");
		data->camera_br_hysteresis[0] = 10000;
		data->camera_br_hysteresis[1] = 0;
	}

	if (of_property_read_u32(np, "ssp-brightness-array-len", &data->brightness_array_len)) {
		ssp_err("no brightness array len");
		data->brightness_array_len = 0;
		data->brightness_array = NULL;

	} else {
		data->brightness_array = kcalloc(data->brightness_array_len, sizeof(u32), GFP_KERNEL);
		if (of_property_read_u32_array(np, "ssp-brightness-array",
						   data->brightness_array, data->brightness_array_len)) {
			ssp_err("no brightness array");
			data->brightness_array_len = 0;
			kfree(data->brightness_array);
			data->brightness_array = NULL;
		}
	}

#endif

#ifdef CONFIG_SENSORS_SSP_MAGNETIC
	/* mag matrix */
#ifdef CONFIG_SENSORS_SSP_MAGNETIC_YAS539
	if (of_property_read_u16_array(np, "ssp-mag-array",
					   data->pdc_matrix, ARRAY_SIZE(data->pdc_matrix)))
		ssp_err("no mag-array, set as 0");

	/* check nfc/mst for mag matrix*/
	{
		int check_mst_gpio, check_nfc_gpio;
		int value_mst = 0, value_nfc = 0;

		check_nfc_gpio = of_get_named_gpio(np, "ssp-mag-check-nfc", 0);
		if (check_nfc_gpio >= 0)
			value_nfc = gpio_get_value(check_nfc_gpio);

		check_mst_gpio = of_get_named_gpio(np, "ssp-mag-check-mst", 0);
		if (check_mst_gpio >= 0)
			value_mst = gpio_get_value(check_mst_gpio);

		if (value_mst == 1) {
			ssp_info("mag matrix(%d %d) nfc/mst array", value_nfc, value_mst);
			if (of_property_read_u16_array(np, "ssp-mag-mst-array",
							   data->pdc_matrix, ARRAY_SIZE(data->pdc_matrix)))
				ssp_err("no mag-mst-array");
		} else if (value_nfc == 1) {
			ssp_info("mag matrix(%d %d) nfc only array", value_nfc, value_mst);
			if (of_property_read_u16_array(np, "ssp-mag-nfc-array",
							   data->pdc_matrix, ARRAY_SIZE(data->pdc_matrix)))
				ssp_err("no mag-nfc-array");
		}
	}
#else
	if (of_property_read_u8_array(np, "ssp-mag-array",
					  data->pdc_matrix, ARRAY_SIZE(data->pdc_matrix)))
		ssp_err("no mag-array, set as 0");

	/* check nfc/mst for mag matrix*/
	{
		int check_mst_gpio, check_nfc_gpio;
		int value_mst = 0, value_nfc = 0;

		check_nfc_gpio = of_get_named_gpio(np, "ssp-mag-check-nfc", 0);
		if (check_nfc_gpio >= 0)
			value_nfc = gpio_get_value(check_nfc_gpio);

		check_mst_gpio = of_get_named_gpio(np, "ssp-mag-check-mst", 0);
		if (check_mst_gpio >= 0)
			value_mst = gpio_get_value(check_mst_gpio);

		if (value_mst == 1) {
			ssp_info("mag matrix(%d %d) nfc/mst array", value_nfc, value_mst);
			if (of_property_read_u8_array(np, "ssp-mag-mst-array",
							  data->pdc_matrix, ARRAY_SIZE(data->pdc_matrix)))
				ssp_err("no mag-mst-array");
		} else if (value_nfc == 1) {
			ssp_info("mag matrix(%d %d) nfc only array", value_nfc, value_mst);
			if (of_property_read_u8_array(np, "ssp-mag-nfc-array",
							  data->pdc_matrix, ARRAY_SIZE(data->pdc_matrix)))
				ssp_err("no mag-nfc-array");
		}
	}
#endif

#endif
	return 0;
}

int ssp_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = &pdev->dev;
	struct ssp_data *data;

	ssp_infof();
	data = kzalloc(sizeof(struct ssp_data), GFP_KERNEL);

	data->is_probe_done = false;

	data->pdev = pdev;
	if (dev->of_node) {
		ret = ssp_parse_dt(dev, data);
		if (ret) {
			ssp_errf("failed to parse dt");
			goto err_setup;
		}
	} else {
		ssp_errf("failed to get device node");
		ret = -ENODEV;
		goto err_setup;
	}

	mutex_init(&data->comm_mutex);
	mutex_init(&data->pending_mutex);
	mutex_init(&data->enable_mutex);
#ifdef CONFIG_SSP_ENG_DEBUG
	ssp_system_checker_init();
#endif

	pr_info("\n#####################################################\n");

	INIT_DELAYED_WORK(&data->work_refresh, refresh_task);
	INIT_WORK(&data->work_reset, reset_task);
#if (KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE) || defined(CONFIG_SSP_PM_WAKEUP_LINUX_VER_5_4)
	wakeup_source_register(&pdev->dev, "ssp_wake_lock");
#else
	wakeup_source_register("ssp_wake_lock");
#endif
	init_waitqueue_head(&data->reset_lock.waitqueue);
	atomic_set(&data->reset_lock.state, 1);
	initialize_variable(data);

	ret = initialize_debug_timer(data);
	if (ret < 0) {
		ssp_errf("could not create workqueue");
		goto err_create_workqueue;
	}

	ret = initialize_timestamp_sync_timer(data);
	if (ret < 0) {
		ssp_errf("could not create ts_sync workqueue");
		goto err_create_ts_sync_workqueue;
	}

	ret = initialize_sysfs(data);
	if (ret < 0) {
		ssp_errf("could not create sysfs");
		goto err_sysfs_create;
	}

	ret = initialize_indio_dev(&(data->pdev->dev), data);
	if (ret < 0) {
		ssp_errf("could not create input device");
		//return FAIL;
	}

	ret = ssp_scontext_initialize(data);
	if (ret < 0) {
		ssp_errf("ssp_scontext_initialize err(%d)", ret);
		ssp_scontext_remove(data);
		goto err_init_scontext;
	}

	ret = ssp_injection_initialize(data);
	if (ret < 0) {
		ssp_errf("ssp_injection_initialize err(%d)", ret);
		ssp_injection_remove(data);
		goto err_init_injection;
	}

#ifdef CONFIG_SENSORS_SSP_DUMP
	initialize_ssp_dump(data);
#endif

	data->is_probe_done = true;

	enable_debug_timer(data);
	m_ssp_data = data;
#ifdef CONFIG_SEC_VIB_NOTIFIER
	if (data->accel_motor_coef > 0) {
		init_motor_control();
		sec_vib_notifier_register(&vib_notifier);
	}
#endif
	ssp_infof("probe success!");

	sensorhub_platform_probe(data);

	goto exit;

	//ssp_injection_remove(data);
err_init_injection:
	ssp_scontext_remove(data);
err_init_scontext:
	remove_sysfs(data);
err_sysfs_create:
	destroy_workqueue(data->debug_wq);
err_create_ts_sync_workqueue:
	destroy_workqueue(data->ts_sync_wq);
err_create_workqueue:
	wakeup_source_unregister(data->ssp_wakelock);
	mutex_destroy(&data->comm_mutex);
	mutex_destroy(&data->pending_mutex);
	mutex_destroy(&data->enable_mutex);
err_setup:
	kfree(data);
	data = ERR_PTR(ret);
	ssp_errf("probe failed!");

exit:
	pr_info("#####################################################\n\n");
	return ret;
}

void ssp_shutdown(struct platform_device *pdev)
{
	struct ssp_data *data = get_ssp_data();

	ssp_infof();
	if (data->is_probe_done == false)
		goto exit;
	sensorhub_platform_shutdown(data);

	data->is_probe_done = false;
	disable_debug_timer(data);
	disable_timestamp_sync_timer(data);
	clean_pending_list(data);

#ifdef CONFIG_SENSORS_SSP_DUMP
	remove_ssp_dump(data);
#endif
	remove_sysfs(data);
	ssp_injection_remove(data);
	ssp_scontext_remove(data);

#ifdef CONFIG_SEC_VIB_NOTIFIER
	sec_vib_notifier_unregister(&vib_notifier);
#endif
	cancel_delayed_work(&data->work_refresh);
	cancel_work_sync(&data->work_reset);
	del_timer(&data->debug_timer);
	cancel_work_sync(&data->work_debug);
	destroy_workqueue(data->debug_wq);
	del_timer(&data->ts_sync_timer);
	cancel_work_sync(&data->work_ts_sync);
	destroy_workqueue(data->ts_sync_wq);
	wakeup_source_unregister(data->ssp_wakelock);
	mutex_destroy(&data->comm_mutex);
	mutex_destroy(&data->pending_mutex);
	mutex_destroy(&data->enable_mutex);
exit:
	ssp_infof("done");
}

int ssp_suspend(struct device *dev)
{
	struct ssp_data *data = get_ssp_data();

	ssp_infof();

	disable_debug_timer(data);
	disable_timestamp_sync_timer(data);

	data->last_resume_status = SCONTEXT_AP_STATUS_SUSPEND;
#ifdef CONFIG_SSP_SENSOR_HUB_MTK
	if (ssp_send_status(data, SCONTEXT_AP_STATUS_SUSPEND) != SUCCESS)
		ssp_errf("SCONTEXT_AP_STATUS_SUSPEND failed");
#endif
	return 0;
}

void ssp_resume(struct device *dev)
{
	struct ssp_data *data = get_ssp_data();

	ssp_infof();

	enable_debug_timer(data);
	enable_timestamp_sync_timer(data);
#ifdef CONFIG_SSP_SENSOR_HUB_MTK
	if (ssp_send_status(data, SCONTEXT_AP_STATUS_RESUME) != SUCCESS)
		ssp_errf("SCONTEXT_AP_STATUS_RESUME failed");
#endif
	data->last_resume_status = SCONTEXT_AP_STATUS_RESUME;
}

#ifdef CONFIG_SSP_SENSOR_HUB_MTK
static struct dev_pm_ops ssp_pm_ops = {
	.prepare = ssp_suspend,
	.complete = ssp_resume,
};

static struct of_device_id ssp_match_table[] = {
	{ .compatible = "ssp",},
	{},
};

static struct platform_driver ssp_driver = {
	.probe = ssp_probe,
	.shutdown = ssp_shutdown,
	//.id_table = ssp_id,
	.driver = {
		.pm = &ssp_pm_ops,
		.owner = THIS_MODULE,
		.name = "ssp",
		.of_match_table = ssp_match_table
	},
};

static int __init ssp_init(void)
{
	int ret = 0;

	ret = platform_driver_register(&ssp_driver);
	ssp_infof("ret %d", ret);
	return ret;

}

static void __exit ssp_exit(void)
{
	platform_driver_unregister(&ssp_driver);
}

late_initcall(ssp_init);
module_exit(ssp_exit);
MODULE_DESCRIPTION("Seamless Sensor Platform(SSP) dev driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
#endif
