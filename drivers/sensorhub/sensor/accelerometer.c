#include "../comm/shub_comm.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_dev_core.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_file_manager.h"
#include "../others/shub_motor_callback.h"
#include "accelerometer.h"

#include <linux/kernel.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

void parse_dt_accelerometer(struct device *dev)
{
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;
	struct device_node *np = dev->of_node;
	int accel_motor_coef = 0;

	if (data->chipset_funcs->parse_dt)
		data->chipset_funcs->parse_dt(dev);

	if (!of_property_read_u32(np, "acc-motor-coef", &accel_motor_coef))
		set_motor_coef(accel_motor_coef);
	shub_infof("acc-motor-coef[%d]", accel_motor_coef);
}

int set_accel_position(int position)
{
	int ret = 0;
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;

	data->position = position;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_ACCELEROMETER, SENSOR_AXIS, (char *)&(data->position),
				sizeof(data->position));
	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return ret;
	}

	shub_infof("A : %u", data->position);

	return ret;
}

int get_accel_position(void)
{
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;

	return data->position;
}

static int open_accel_calibration_file(void)
{
	int ret = 0;
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;

	ret = shub_file_read(ACCEL_CALIBRATION_FILE_PATH, (char *)&data->cal_data, sizeof(data->cal_data), 0);
	if (ret != sizeof(data->cal_data))
		ret = -EIO;

	shub_infof("open accel calibration %d, %d, %d\n", data->cal_data.x, data->cal_data.y, data->cal_data.z);

	if ((data->cal_data.x == 0) && (data->cal_data.y == 0) && (data->cal_data.z == 0))
		return -EINVAL;

	return ret;
}

int set_accel_cal(struct accelerometer_data *data)
{
	int ret = 0;
	s16 accel_cal[3] = {0, };

	if (!get_sensor_probe_state(SENSOR_TYPE_ACCELEROMETER)) {
		shub_infof("[SHUB] Skip this function!!!, accel sensor is not connected\n");
		return ret;
	}

	accel_cal[0] = data->cal_data.x;
	accel_cal[1] = data->cal_data.y;
	accel_cal[2] = data->cal_data.z;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_ACCELEROMETER, CAL_DATA,
				(char *)accel_cal, sizeof(accel_cal));

	if (ret < 0) {
		shub_errf("CMD Fail %d", ret);
		return ret;
	}
	shub_info("[SHUB] Set accel cal data %d, %d, %d\n", data->cal_data.x, data->cal_data.y, data->cal_data.z);

	return ret;
}

typedef struct accelerometer_chipset_funcs *(get_accelerometer_function_pointer)(char *);
get_accelerometer_function_pointer *get_acc_funcs_ary[] = {
	get_accelometer_icm42605m_function_pointer,
	get_accelometer_lsm6dsl_function_pointer,
	get_accelometer_lis2dlc12_function_pointer,
	get_accelometer_lsm6dsotr_function_pointer,
};

static int init_accelerometer_chipset(char *name, char *vendor)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER);
	struct accelerometer_data *data = (struct accelerometer_data *)sensor->data;
	struct accelerometer_chipset_funcs *funcs;
	uint64_t i;

	shub_infof("");

	if (data->chipset_funcs)
		return 0;

	strcpy(sensor->chipset_name, name);
	strcpy(sensor->vendor, vendor);

	for (i = 0; i < ARRAY_LEN(get_acc_funcs_ary); i++) {
		funcs = get_acc_funcs_ary[i](name);
		if (funcs) {
			data->chipset_funcs = funcs;
			break;
		}
	}

	if (!data->chipset_funcs) {
		shub_errf("cannot find accelerometer sensor chipset (%s)", name);
		return -EINVAL;
	}

	parse_dt_accelerometer(get_shub_device());

	return 0;
}

int sync_accelerometer_status(void)
{
	int ret = 0;
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;

	shub_infof();
	ret = set_accel_position(data->position);
	if (ret < 0) {
		shub_errf("set position failed");
		return ret;
	}

	ret = set_accel_cal(data);
	if (ret < 0) {
		shub_errf("set_mag_cal failed");
		return ret;
	}

	return ret;
}

void print_accelerometer_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER);
	struct sensor_event *event = &(sensor->event_buffer);
	struct accel_event *sensor_value = (struct accel_event *)(event->value);

	shub_info("%s(%u) : %d, %d, %d (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_ACCELEROMETER, sensor_value->x,
		  sensor_value->y, sensor_value->z, event->timestamp, sensor->sampling_period,
		  sensor->max_report_latency);
}

int init_accelerometer(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "accelerometer_sensor");
		sensor->report_mode_continuous = true;
		sensor->receive_event_size = 6;
		sensor->report_event_size = 6;
		sensor->event_buffer.value = kzalloc(sizeof(struct accel_event), GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;

		sensor->data = kzalloc(sizeof(struct accelerometer_data), GFP_KERNEL);
		if (!sensor->data)
			goto err_no_mem;

		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		if (!sensor->funcs)
			goto err_no_mem;

		sensor->funcs->sync_status = sync_accelerometer_status;
		sensor->funcs->print_debug = print_accelerometer_debug;
		sensor->funcs->set_position = set_accel_position;
		sensor->funcs->get_position = get_accel_position;
		sensor->funcs->init_chipset = init_accelerometer_chipset;
		sensor->funcs->open_calibration_file = open_accel_calibration_file;
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;

		kfree(sensor->data);
		sensor->data = NULL;

		kfree(sensor->funcs);
		sensor->funcs = NULL;
	}
	return 0;

err_no_mem:
	shub_errf("err no memory");
	kfree(sensor->event_buffer.value);
	sensor->event_buffer.value = NULL;

	kfree(sensor->data);
	sensor->data = NULL;

	kfree(sensor->funcs);
	sensor->funcs = NULL;

	return -ENOMEM;
}

static void print_accelerometer_uncal_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED);
	struct sensor_event *event = &(sensor->event_buffer);
	struct uncal_accel_event *sensor_value = (struct uncal_accel_event *)(event->value);

	shub_info("%s(%u) : %d, %d, %d, %d, %d, %d (%lld) (%ums, %dms)", sensor->name,
		  SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED, sensor_value->uncal_x, sensor_value->uncal_y,
		  sensor_value->uncal_z, sensor_value->offset_x, sensor_value->offset_y, sensor_value->offset_z,
		  event->timestamp, sensor->sampling_period, sensor->max_report_latency);
}

int init_accelerometer_uncal(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_ACCELEROMETER_UNCALIBRATED);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "uncal_accel_sensor");
		sensor->report_mode_continuous = true;
		sensor->receive_event_size = 12;
		sensor->report_event_size = 12;
		sensor->event_buffer.value = kzalloc(sizeof(struct uncal_accel_event), GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;

		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		if (!sensor->funcs)
			goto err_no_mem;

		sensor->funcs->print_debug = print_accelerometer_uncal_debug;
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;

		kfree(sensor->funcs);
		sensor->funcs = NULL;
	}
	return 0;

err_no_mem:
	kfree(sensor->event_buffer.value);
	sensor->event_buffer.value = NULL;

	kfree(sensor->funcs);
	sensor->funcs = NULL;

	return -ENOMEM;
}
