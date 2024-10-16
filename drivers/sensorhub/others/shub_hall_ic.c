#include "shub_hall_ic.h"
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
#include <linux/hall/hall_ic_notifier.h>
#include "../utility/shub_utility.h"
#include "../sensorhub/shub_device.h"
#include "../comm/shub_comm.h"
#include "../sensormanager/shub_sensor_manager.h"
#include "../sensormanager/shub_sensor_type.h"
#include "../sensormanager/shub_sensor.h"
#include "../sensor/magnetometer.h"

int mpp_status = 0;
struct work_struct hall_ic_work;

static void send_mpp_status(u8 status)
{
	struct shub_sensor *sensor = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD);
	int ret = 0;

	if (!sensor || !((struct magnetometer_data *)sensor->data)->mpp_matrix) {
		shub_errf("sensor or mpp_matrix is null");
		return;
	}

	shub_infof("%d", status);

	ret = shub_send_command(CMD_SETVALUE, SENSOR_TYPE_GEOMAGNETIC_FIELD, MAG_SUBCMD_MPP_COVER_STATUS, (char *)&status,
				sizeof(status));

	if (ret < 0) {
		shub_errf("CMD fail %d\n", ret);
		return;
	}
}

static void hall_ic_work_function(struct work_struct *work)
{
	shub_infof("queue work %d\n", mpp_status);
	send_mpp_status(mpp_status);
}

static int hall_ic_notifier(struct notifier_block *nb,
			unsigned long val, void *v)
{
	struct hall_notifier_context *context = v;

	shub_infof("%s: name: %s, value: %s\n", __func__,
		context->name, context->value ? "CLOSE" : "OPEN");

	if (!strcmp(context->name, "back_cover")) {
		mpp_status = context->value;
		shub_queue_work(&hall_ic_work);
	}

	return 0;
}

static struct notifier_block hall_ic_notifier_block = {
	.notifier_call = hall_ic_notifier,
	.priority = 1,
};

void init_hall_ic_callback(void)
{
	shub_infof("init_hall_ic_callback\n");
	hall_notifier_register(&hall_ic_notifier_block);
	INIT_WORK(&hall_ic_work, hall_ic_work_function);
}

void remove_hall_ic_callback(void)
{
	hall_notifier_unregister(&hall_ic_notifier_block);
	cancel_work_sync(&hall_ic_work);
}

void sync_hall_ic_state(void)
{
	shub_info("sync status: %d", mpp_status);
	send_mpp_status(mpp_status);
}
#else
void init_hall_ic_callback(void) {}
void remove_hall_ic_callback(void) {}
void sync_hall_ic_state(void) {}
#endif