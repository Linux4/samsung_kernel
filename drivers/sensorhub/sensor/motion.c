#include "../sensormanager/shub_sensor.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../utility/shub_utility.h"

#include <linux/slab.h>

void init_significant_motion(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_SIGNIFICANT_MOTION);

	if (!sensor)
		return;

	if (en) {
		strcpy(sensor->name, "sig_motion_sensor");
		sensor->receive_event_size = 1;
		sensor->report_event_size = 1;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
}

void init_tilt_detector(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_TILT_DETECTOR);

	if (!sensor)
		return;

	if (en) {
		strcpy(sensor->name, "tilt_detector");
		sensor->receive_event_size = 1;
		sensor->report_event_size = 1;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
}

void init_pick_up_gesture(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PICK_UP_GESTURE);

	if (!sensor)
		return;

	if (en) {
		strcpy(sensor->name, "pickup_gesture");
		sensor->receive_event_size = 1;
		sensor->report_event_size = 1;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
}

void init_call_gesture(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_CALL_GESTURE);

	if (!sensor)
		return;

	if (en) {
		strcpy(sensor->name, "call_gesture");
		sensor->receive_event_size = 1;
		sensor->report_event_size = 1;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
}

void init_wake_up_motion(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_WAKE_UP_MOTION);

	if (!sensor)
		return;

	if (en) {
		strcpy(sensor->name, "wake_up_motion");
		sensor->receive_event_size = 1;
		sensor->report_event_size = 1;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
}

void init_protos_motion(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_PROTOS_MOTION);

	if (!sensor)
		return;

	if (en) {
		strcpy(sensor->name, "protos_motion");
		sensor->receive_event_size = 1;
		sensor->report_event_size = 1;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
}

void init_led_cover_event(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_LED_COVER_EVENT);

	if (!sensor)
		return;

	if (en) {
		strcpy(sensor->name, "led_cover_event");
		sensor->receive_event_size = 1;
		sensor->report_event_size = 1;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
}

void init_tap_tracker(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_TAP_TRACKER);

	if (!sensor)
		return;

	if (en) {
		strcpy(sensor->name, "tap_tracker");
		sensor->receive_event_size = 1;
		sensor->report_event_size = 1;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
}

void init_shake_tracker(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_SHAKE_TRACKER);

	if (!sensor)
		return;

	if (en) {
		strcpy(sensor->name, "shake_tracker");
		sensor->receive_event_size = 1;
		sensor->report_event_size = 1;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
}

void init_move_detector(bool en)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_MOVE_DETECTOR);

	if (!sensor)
		return;

	if (en) {
		strcpy(sensor->name, "move_detector");
		sensor->receive_event_size = 1;
		sensor->report_event_size = 1;
		sensor->event_buffer.value = kzalloc(sensor->receive_event_size, GFP_KERNEL);
	} else {
		kfree(sensor->event_buffer.value);
		sensor->event_buffer.value = NULL;
	}
}
