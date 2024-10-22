#include "shub_hall_ic.h"
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
#include <linux/hall/hall_ic_notifier.h>
#include "../utility/shub_utility.h"
#include "../sensorhub/shub_device.h"
#include "../comm/shub_comm.h"

int hall_ic_status;
struct work_struct hall_ic_work;

static void hall_ic_work_function(struct work_struct *work)
{
	shub_infof("hall_ic_status: %d\n", hall_ic_status);
}

static int hall_ic_notifier(struct notifier_block *nb,
			unsigned long val, void *v)
{
	struct hall_notifier_context *context = v;
	hall_ic_status = context->value;

	shub_infof("%s: name: %s, value: %s\n", __func__,
		context->name, context->value ? "CLOSE" : "OPEN");

	shub_queue_work(&hall_ic_work);

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
	shub_infof("hall_ic_status: %d\n", hall_ic_status);
}
#else
void init_hall_ic_callback(void) {}
void remove_hall_ic_callback(void) {}
void sync_hall_ic_state(void) {}
#endif