#include "../../accelerometer.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../sensormanager/shub_sensor_type.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../utility/shub_utility.h"

#include <linux/device.h>
#include <linux/of_gpio.h>

#define LSM6DSL_NAME		"LSM6DSL"
static void parse_dt_accelerometer_lsm6dsl(struct device *dev)
{
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;
	struct device_node *np = dev->of_node;

	if (of_property_read_u32(np, "acc-lsm6dsl-position", &data->position))
		data->position = 0;
	shub_infof("position[%d]", data->position);
}

struct accelerometer_chipset_funcs accel_lsm6dsl_ops = {
	.parse_dt = parse_dt_accelerometer_lsm6dsl,
};

struct accelerometer_chipset_funcs *get_accelometer_lsm6dsl_function_pointer(char *name)
{
	if (strcmp(name, LSM6DSL_NAME) != 0)
		return NULL;

	return &accel_lsm6dsl_ops;
}
