
#include "shub_sensor.h"
#include "../sensorhub/shub_device.h"
#include "../utility/shub_utility.h"

struct sensor_chipset_init_funcs *
get_sensor_init_chipset_funcs(char *name, get_init_chipset_funcs_ptr *get_chipset_funcs_array, uint32_t len)
{
	int i;
	void *chipset_funcs = NULL;

	for (i = 0; i < len; i++) {
		chipset_funcs = get_chipset_funcs_array[i](name);
		if (chipset_funcs)
			break;
	}

	return chipset_funcs;
}

int init_shub_sensor(struct shub_sensor *sensor)
{
	int ret;
	struct sensor_chipset_init_funcs *chipset_init_funcs = NULL;
	struct device *dev = get_shub_device();

	if (!sensor || !sensor->funcs)
		return 0;

	shub_infof("%s", sensor->name);

	if (sensor->funcs->get_init_chipset_funcs) {
		int len;
		get_init_chipset_funcs_ptr *get_chipset_funcs_array;

		get_chipset_funcs_array = sensor->funcs->get_init_chipset_funcs(&len);
		chipset_init_funcs = get_sensor_init_chipset_funcs(sensor->spec.name, get_chipset_funcs_array, len);

		if (!chipset_init_funcs)
			shub_infof("can't find chipset init funcs");
	}

	if (chipset_init_funcs && chipset_init_funcs->init) {
		ret = chipset_init_funcs->init();
		if (ret) {
			shub_errf("%s chipset init ret %d", sensor->name, ret);
			return ret;
		}
	}

	if (sensor->funcs->init_variable) {
		ret = sensor->funcs->init_variable();
		if (ret) {
			shub_errf("%s init variable ret %d", sensor->name, ret);
			return ret;
		}
	}

	if (sensor->funcs->parse_dt)
		sensor->funcs->parse_dt(dev);

	if (chipset_init_funcs && chipset_init_funcs->parse_dt)
		chipset_init_funcs->parse_dt(dev);

	if (chipset_init_funcs && chipset_init_funcs->get_chipset_funcs)
		sensor->chipset_funcs = chipset_init_funcs->get_chipset_funcs();

	return 0;
}
