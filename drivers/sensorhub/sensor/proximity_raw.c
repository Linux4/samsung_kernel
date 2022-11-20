#include <linux/slab.h>

#include "../sensor/proximity.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"

#define PROX_AVG_READ_NUM       80

unsigned int praw_num;
unsigned int praw_sum;
unsigned int praw_min;
unsigned int praw_max;
unsigned int praw_avg;

void report_event_proximity_raw(void)
{
	struct prox_raw_event *sensor_value =
	    (struct prox_raw_event *)(get_sensor_event(SENSOR_TYPE_PROXIMITY_RAW)->value);
	struct proximity_raw_data *data = get_sensor(SENSOR_TYPE_PROXIMITY_RAW)->data;

	if (praw_num++ >= PROX_AVG_READ_NUM) {
		data->avg_data[PROX_RAW_AVG] = praw_sum / PROX_AVG_READ_NUM;
		data->avg_data[PROX_RAW_MIN] = praw_min;
		data->avg_data[PROX_RAW_MAX] = praw_max;

		praw_num = 0;
		praw_min = 0;
		praw_max = 0;
		praw_sum = 0;
	} else {
		praw_sum += sensor_value->prox_raw;

		if (praw_num == 1)
			praw_min = sensor_value->prox_raw;
		else if (sensor_value->prox_raw < praw_min)
			praw_min = sensor_value->prox_raw;

		if (sensor_value->prox_raw > praw_max)
			praw_max = sensor_value->prox_raw;
	}
}

int init_proximity_raw(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY_RAW);

	if (!sensor)
		return 0;

	if (en) {
		strcpy(sensor->name, "proximity_raw");
		sensor->receive_event_size = 2;
		sensor->report_event_size = 0;
		sensor->event_buffer.value = kzalloc(sizeof(struct prox_raw_event), GFP_KERNEL);
		if (!sensor->event_buffer.value)
			goto err_no_mem;

		sensor->data = kzalloc(sizeof(struct proximity_raw_data), GFP_KERNEL);
		if (!sensor->data)
			goto err_no_mem;

		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		if (!sensor->funcs)
			goto err_no_mem;

		sensor->funcs->report_event = report_event_proximity_raw;
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
	kfree(sensor->event_buffer.value);
	sensor->event_buffer.value = NULL;

	kfree(sensor->data);
	sensor->data = NULL;

	kfree(sensor->funcs);
	sensor->funcs = NULL;

	return -ENOMEM;
}
