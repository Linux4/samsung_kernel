#include <linux/err.h>

#include "../utility/shub_utility.h"
#include "shub_ois.h"

static struct ois_sensor_interface ois_control;
static struct ois_sensor_interface ois_reset;

int ois_fw_update_register(struct ois_sensor_interface *ois)
{
	if (ois->core)
		ois_control.core = ois->core;

	if (ois->ois_func)
		ois_control.ois_func = ois->ois_func;

	if (!ois->core || !ois->ois_func) {
		shub_infof("no ois struct");
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(ois_fw_update_register);

void ois_fw_update_unregister(void)
{
	ois_control.core = NULL;
	ois_control.ois_func = NULL;
}
EXPORT_SYMBOL(ois_fw_update_unregister);

int ois_reset_register(struct ois_sensor_interface *ois)
{
	if (ois->core)
		ois_reset.core = ois->core;

	if (ois->ois_func)
		ois_reset.ois_func = ois->ois_func;

	if (!ois->core || !ois->ois_func) {
		shub_infof("no ois struct");
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(ois_reset_register);

void ois_reset_unregister(void)
{
	ois_reset.core = NULL;
	ois_reset.ois_func = NULL;
}
EXPORT_SYMBOL(ois_reset_unregister);

void notify_ois_reset(void)
{
	if (ois_control.ois_func != NULL) {
		ois_control.ois_func(ois_control.core);
		ois_control.ois_func = NULL;
	}

	if (ois_reset.ois_func != NULL)
		ois_reset.ois_func(ois_reset.core);
}
