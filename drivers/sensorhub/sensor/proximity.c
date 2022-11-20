#include "../comm/shub_comm.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_dev_core.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_file_manager.h"
#include "proximity.h"

#include <linux/of_gpio.h>
#include <linux/slab.h>

static int init_proximity_variable(struct proximity_data *data)
{
	if (data->chipset_funcs && data->chipset_funcs->init_proximity_variable)
		data->chipset_funcs->init_proximity_variable(data);

	if (data->cal_data_len) {
		data->cal_data = kzalloc(data->cal_data_len, GFP_KERNEL);
		if (data->cal_data)
			return -ENOMEM;
	}
	return 0;
}

static void parse_dt_proximity(struct device *dev)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->chipset_funcs && data->chipset_funcs->parse_dt)
		data->chipset_funcs->parse_dt(dev);
}

#define TEMPERATURE_THRESH 50
#define COMPENSATION_VALUE 40
#define BATTERY_TEMP_PATH  "/sys/class/power_supply/battery/temp"

int get_proximity_thresh_temperature_compensation(void)
{
	int ret = 0;
	char temp_str[10] = {0, };
	int temp_value = 0;

	ret = shub_file_read(BATTERY_TEMP_PATH, (char *)&temp_str, sizeof(temp_str), 0);
	if (ret <= 0) {
		shub_errf("can't proximity read temperature file %d", ret);
		return 0;
	}

	ret = kstrtos32(temp_str, 10, &temp_value);
	if (ret < 0) {
		shub_errf("kstrtou32 failed(%d)", ret);
		ret = 0;
	} else if (temp_value < TEMPERATURE_THRESH)
		ret = COMPENSATION_VALUE;

	shub_infof("%s temp value %d compensation %d", temp_str, temp_value, ret);
	return ret;
}

void set_proximity_threshold(void)
{
	int ret = 0;
	u8 prox_th_mode = -1;
	u16 prox_th[PROX_THRESH_SIZE] = {0, };
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (!get_sensor_probe_state(SENSOR_TYPE_PROXIMITY)) {
		shub_infof("proximity sensor is not connected");
		return;
	}

	memcpy(prox_th, data->prox_threshold, sizeof(prox_th));

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_PROXIMITY, PROXIMITY_THRESHOLD, (char *)prox_th,
				sizeof(prox_th));
	if (ret < 0) {
		shub_err("SENSOR_PROXTHRESHOLD CMD fail %d", ret);
		return;
	}

	if (data->need_compensation) {
		int compensation = get_proximity_thresh_temperature_compensation();

		prox_th[0] += compensation;
		prox_th[1] += compensation;
	}

	if (data->chipset_funcs && data->chipset_funcs->get_proximity_threshold_mode)
		prox_th_mode = data->chipset_funcs->get_proximity_threshold_mode();

	shub_info("Proximity Threshold[%d] - %u, %u", prox_th_mode, data->prox_threshold[PROX_THRESH_HIGH],
		  data->prox_threshold[PROX_THRESH_LOW]);
}

typedef struct proximity_chipset_funcs *(get_proximity_function_pointer)(char *);

get_proximity_function_pointer *get_prox_funcs_ary[] = {
	get_proximity_stk3x6x_function_pointer,
	get_proximity_gp2ap110s_function_pointer,
	get_proximity_stk3328_function_pointer,
};

int init_proximity_chipset(char *name, char *vendor)
{
	int ret;
	uint64_t i;
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
	struct proximity_data *data = sensor->data;
	struct proximity_chipset_funcs *funcs;

	shub_infof("");

	if (data->chipset_funcs)
		return 0;

	strcpy(sensor->chipset_name, name);
	strcpy(sensor->vendor, vendor);

	for (i = 0; i < ARRAY_LEN(get_prox_funcs_ary); i++) {
		funcs = get_prox_funcs_ary[i](name);
		if (funcs) {
			data->chipset_funcs = funcs;
			if (data->chipset_funcs->init)
				data->chipset_funcs->init(data);
			break;
		}
	}

	if (!data->chipset_funcs) {
		shub_errf("cannot find proximity sensor chipset.");
		return -EINVAL;
	}

	parse_dt_proximity(get_shub_device());
	ret = init_proximity_variable(data);

	return ret;
}

static int sync_proximity_status(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	shub_infof();

	set_proximity_threshold();
	if (data->chipset_funcs && data->chipset_funcs->sync_proximity_state)
		data->chipset_funcs->sync_proximity_state(data);

	return ret;
}

static void print_debug_proximity(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
	struct sensor_event *event = &(sensor->event_buffer);
	struct prox_event *sensor_value = (struct prox_event *)(event->value);

	shub_infof("%s(%u) : %d, %d (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_PROXIMITY, sensor_value->prox,
		   sensor_value->prox_raw, event->timestamp, sensor->sampling_period, sensor->max_report_latency);
}

static int enable_proximity(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	set_proximity_threshold();
	if (data->chipset_funcs && data->chipset_funcs->pre_enable_proximity)
		data->chipset_funcs->pre_enable_proximity(data);

	return 0;
}

void print_proximity_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);
	struct sensor_event *event = &(sensor->event_buffer);
	struct prox_event *sensor_value = (struct prox_event *)(event->value);

	shub_info("%s(%u) : %d, %d (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_PROXIMITY, sensor_value->prox,
		  sensor_value->prox_raw, event->timestamp, sensor->sampling_period, sensor->max_report_latency);
}

void report_event_proximity(void)
{
	struct prox_event *sensor_value = (struct prox_event *)(get_sensor_event(SENSOR_TYPE_PROXIMITY)->value);

	shub_infof("Proximity Sensor Detect : %u, raw : %u", sensor_value->prox, sensor_value->prox_raw);
}

int parsing_proximity_threshold(char *dataframe, int *index)
{
	u16 thresh[2] = {0, };
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	memcpy(thresh, dataframe + (*index), sizeof(thresh));
	data->prox_threshold[0] = thresh[0];
	data->prox_threshold[1] = thresh[1];

	if (data->chipset_funcs->set_proximity_threshold_mode)
		data->chipset_funcs->set_proximity_threshold_mode(3);

	(*index) += sizeof(thresh);
	shub_infof("prox threshold received %u %u", data->prox_threshold[0], data->prox_threshold[1]);

	return 0;
}

int set_proximity_calibration(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->cal_data_len == 0)
		return ret;

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_PROXIMITY, CAL_DATA, (char *)data->cal_data,
				 data->cal_data_len);
	if (ret < 0)
		shub_errf("failed %d", ret);

	return ret;
}

int save_proximity_calibration(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->cal_data_len == 0)
		return ret;

	ret = shub_file_write_no_wait(PROX_CALIBRATION_FILE_PATH, (char *)data->cal_data, data->cal_data_len, 0);
	if (ret != data->cal_data_len) {
		shub_errf("failed");
		return -EIO;
	}

	return ret;
}

static int open_default_proximity_calibration(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->cal_data_len == 0)
		return ret;

	ret = shub_file_read(PROX_CALIBRATION_FILE_PATH, (char *)data->cal_data, data->cal_data_len, 0);
	if (ret != data->cal_data_len) {
		shub_errf("failed");
		return -EIO;
	}

	return ret;
}

int open_proximity_calibration(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (data->chipset_funcs->open_calibration_file)
		ret = data->chipset_funcs->open_calibration_file();
	else
		ret = open_default_proximity_calibration();

	return ret;
}

int init_proximity(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "proximity_sensor");
		sensor->receive_event_size = 3;
		sensor->report_event_size = 1;
		sensor->event_buffer.value = kzalloc(sizeof(struct prox_event), GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;

		sensor->data = kzalloc(sizeof(struct proximity_data), GFP_KERNEL);
		if (!sensor->data)
			goto err_no_mem;

		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		if (!sensor->funcs)
			goto err_no_mem;

		sensor->funcs->enable = enable_proximity;
		sensor->funcs->sync_status = sync_proximity_status;
		sensor->funcs->print_debug = print_debug_proximity;
		sensor->funcs->report_event = report_event_proximity;
		sensor->funcs->parsing_data = parsing_proximity_threshold;
		sensor->funcs->init_chipset = init_proximity_chipset;
		sensor->funcs->open_calibration_file = open_proximity_calibration;
	} else {
		struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

		kfree(data->threshold_data);
		data->threshold_data  = NULL;
		kfree(data->cal_data);
		data->cal_data = NULL;
		kfree(sensor->data);
		sensor->data = NULL;

		kfree(sensor->funcs);
		sensor->funcs = NULL;

		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
	return 0;

err_no_mem:
	kfree(sensor->event_buffer.value);
	sensor->event_buffer.value = NULL;

	kfree(sensor->data);
	sensor->data = NULL;

	kfree(sensor->funcs);
	sensor->funcs = NULL;

	return -ENOMEM;
}
