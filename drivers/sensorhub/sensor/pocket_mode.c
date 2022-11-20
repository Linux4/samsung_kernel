#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"

#include <linux/slab.h>

int init_pocket_mode_lite(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_POCKET_MODE_LITE);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "pocket_mode_lite");
		sensor->receive_event_size = 5;
		sensor->report_event_size = 5;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
	return 0;

err_no_mem:
	kfree(sensor->event_buffer.value);
	sensor->event_buffer.value = NULL;

	return -ENOMEM;
}

int init_pocket_mode(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_POCKET_MODE);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "pocket_mode");
		sensor->receive_event_size = 62;
		sensor->report_event_size = 62;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
	return 0;

err_no_mem:
	kfree(sensor->event_buffer.value);
	sensor->event_buffer.value = NULL;

	return -ENOMEM;
}
