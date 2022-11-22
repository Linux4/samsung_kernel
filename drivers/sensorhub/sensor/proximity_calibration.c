#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../sensor/proximity.h"

#include <linux/slab.h>

void proximity_calibration_off(void)
{
	shub_infof("");
	disable_sensor(SENSOR_TYPE_PROXIMITY_CALIBRATION, NULL, 0);
}

void report_event_proximity_calibration(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY_CALIBRATION)->data;
	struct prox_cal_event *sensor_value =
	    (struct prox_cal_event *)(get_sensor_event(SENSOR_TYPE_PROXIMITY_CALIBRATION)->value);

	data->prox_threshold[0] = sensor_value->prox_cal[0];
	data->prox_threshold[1] = sensor_value->prox_cal[1];
	shub_infof("prox thresh %u %u", data->prox_threshold[0], data->prox_threshold[1]);

	proximity_calibration_off();

	if (data->chipset_funcs->pre_report_event_proximity)
		data->chipset_funcs->pre_report_event_proximity();
}

void init_proximity_calibration(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROXIMITY_CALIBRATION);

	if (!sensor)
		return;

	if (en) {
		strcpy(sensor->name, "proximity_calibration");
		sensor->receive_event_size = 4;
		sensor->report_event_size = 0;
		sensor->event_buffer.value = kzalloc(sizeof(struct prox_cal_event), GFP_KERNEL);

		sensor->data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;
		sensor->funcs = kzalloc(sizeof(struct sensor_funcs), GFP_KERNEL);
		sensor->funcs->report_event = report_event_proximity_calibration;
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;

		kfree(sensor->funcs);
		sensor->funcs = NULL;
	}
}
