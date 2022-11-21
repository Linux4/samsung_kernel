#include "../comm/shub_comm.h"
#include "../sensor/pressure.h"
#include "../sensorhub/shub_device.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"
#include "../utility/shub_file_manager.h"

#include <linux/of_gpio.h>
#include <linux/slab.h>

#define CALIBRATION_FILE_PATH "/efs/FactoryApp/baro_delta"

static int open_pressure_calibration_file(void)
{
	char chBuf[10] = {0, };
	int ret = 0;
	struct pressure_event *sensor_value = (struct pressure_event *)(get_sensor_event(SENSOR_TYPE_PRESSURE)->value);

	ret = shub_file_read(CALIBRATION_FILE_PATH, chBuf, sizeof(chBuf), 0);
	if (ret < 0) {
		shub_errf("Can't read the cal data from file (%d)\n", ret);
		return ret;
	}

	ret = kstrtoint(chBuf, 10, &sensor_value->pressure_cal);
	if (ret < 0) {
		shub_errf("kstrtoint failed. %d", ret);
		return ret;
	}

	shub_infof("open pressure calibration %d", sensor_value->pressure_cal);

	return ret;
}

int init_pressure_chipset(char *name, char *vendor)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PRESSURE);

	shub_infof("");

	strcpy(sensor->chipset_name, name);
	strcpy(sensor->vendor, vendor);

	return 0;
}

int sync_pressure_status()
{
	shub_infof();
	return 0;
}

void print_pressure_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PRESSURE);
	struct sensor_event *event = &(sensor->event_buffer);
	struct pressure_event *sensor_value = (struct pressure_event *)(event->value);

	shub_info("%s(%u) : %d, %d (%lld) (%ums, %dms)", sensor->name, SENSOR_TYPE_PRESSURE, sensor_value->pressure,
		  sensor_value->temperature, event->timestamp, sensor->sampling_period, sensor->max_report_latency);
}

void init_pressure(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PRESSURE);

	if (!sensor)
		return;

	if (en) {
		strcpy(sensor->name, "pressure_sensor");
		sensor->report_mode_continuous = true;
		sensor->receive_event_size = 6;
		sensor->report_event_size = 14;
		sensor->event_buffer.value = kzalloc(sizeof(struct pressure_event), GFP_KERNEL);

		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		sensor->funcs->print_debug = print_pressure_debug;
		sensor->funcs->init_chipset = init_pressure_chipset;
		sensor->funcs->open_calibration_file = open_pressure_calibration_file;
	} else {
		kfree(sensor->funcs);
		sensor->funcs = NULL;

		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
}
