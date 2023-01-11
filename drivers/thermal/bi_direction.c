#include <linux/thermal.h>

#include "thermal_core.h"

static unsigned long get_target_state(struct thermal_zone_device *tz, int old)
{
	int i;
	unsigned long cur_state = 0, tmp_state = 0;
	enum thermal_trend trend;
	int trip_temp;
	trend = get_tz_trend(tz, THERMAL_TRIPS_NONE);
	if (THERMAL_TREND_RAISING == trend) {
		for (i = 0; i < tz->trips; i++) {
			tz->ops->get_trip_temp(tz, i, &trip_temp);
			if (tz->temperature < trip_temp)
				break;
		}
		if (tz->tzdctrl.state_ctrl) {
			tmp_state = i;
			/* only care up */
			if (tmp_state > old)
				cur_state = tmp_state;
			else
				cur_state = old;
		} else
			cur_state = i;
	} else if (THERMAL_TREND_DROPPING == trend) {
		for (i = 0; i < tz->trips; i++) {
			if (tz->ops->get_trip_hyst)
				tz->ops->get_trip_hyst(tz, i, &trip_temp);
			else
				tz->ops->get_trip_temp(tz, i, &trip_temp);
			if (tz->temperature <= trip_temp)
				break;
		}
		if (tz->tzdctrl.state_ctrl) {
			tmp_state = i;
			/* only care down */
			if (tmp_state < old)
				cur_state = tmp_state;
			else
				cur_state = old;
		} else
			cur_state = i;
	} else if (THERMAL_TREND_STABLE == trend)
		cur_state = old;
	else
		cur_state = old;

	return cur_state;
}

static int bi_direction_throttle(struct thermal_zone_device *tz, int trip)
{
	struct thermal_instance *instance;
	int old_target;
	list_for_each_entry(instance, &tz->thermal_instances, tz_node) {
		old_target = instance->target;
		instance->target = get_target_state(tz,
			(THERMAL_NO_TARGET == old_target) ? 0 : old_target);
		if (old_target != instance->target) {
			instance->cdev->updated = false;
			thermal_cdev_update(instance->cdev);
		}
	}
	return 0;
}

static int bi_direction_gov_switch(struct thermal_zone_device *tz,
		struct thermal_governor *from, struct thermal_governor *to)
{
	int i;
	struct thermal_instance *ins;
	int trip_temp;

	if (!strnicmp(from->name, "user_space", THERMAL_NAME_LENGTH)) {
		for (i = 0; i < tz->trips; i++) {
			if (tz->ops->get_trip_hyst)
				tz->ops->get_trip_hyst(tz, i, &trip_temp);
			else
				tz->ops->get_trip_temp(tz, i, &trip_temp);
			if (tz->temperature <= trip_temp)
				break;
		}
		/* init kernel constraints */
		list_for_each_entry(ins, &tz->thermal_instances, tz_node) {
			ins->target = i;
			ins->cdev->updated = false;
			thermal_cdev_update(ins->cdev);
		}
	} else if (!strnicmp(to->name, "user_space", THERMAL_NAME_LENGTH)) {
		/* release kernel constraints */
		list_for_each_entry(ins, &tz->thermal_instances, tz_node) {
			ins->target = 0;
			ins->cdev->updated = false;
			thermal_cdev_update(ins->cdev);
		}
	}
	return 0;
}

static struct thermal_governor thermal_gov_bi_direction = {
	.name           = "bi_direction",
	.throttle       = bi_direction_throttle,
	.switch_gov	= bi_direction_gov_switch,
};

int thermal_gov_bi_direction_register(void)
{
	return thermal_register_governor(&thermal_gov_bi_direction);
}

void thermal_gov_bi_direction_unregister(void)
{
	thermal_unregister_governor(&thermal_gov_bi_direction);
}
