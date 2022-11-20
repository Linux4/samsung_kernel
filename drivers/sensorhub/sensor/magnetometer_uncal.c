#include <linux/slab.h>

#include "../utility/shub_utility.h"
#include "../sensor/magnetometer.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"

void print_magnetometer_uncal_debug(void)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED);
	struct sensor_event *event = &(sensor->event_buffer);
	struct uncal_mag_event *sensor_value = (struct uncal_mag_event *)(event->value);

	shub_info("%s(%u) : %d, %d, %d, %d, %d, %d (%lld) (%ums, %dms)",
		  sensor->name, SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED,
		  sensor_value->uncal_x, sensor_value->uncal_y, sensor_value->uncal_z,
		  sensor_value->offset_x, sensor_value->offset_y, sensor_value->offset_z, event->timestamp,
		  sensor->sampling_period, sensor->max_report_latency);
}

int init_magnetometer_uncal(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_MAGNETIC_FIELD_UNCALIBRATED);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "uncal_geomagnetic_sensor");
		sensor->report_mode_continuous = true;
		sensor->receive_event_size = 12;
		sensor->report_event_size = 12;
		sensor->event_buffer.value = kzalloc(sizeof(struct uncal_mag_event), GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;

		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		if (!sensor->funcs)
			goto err_no_mem;

		sensor->funcs->print_debug = print_magnetometer_uncal_debug;
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
