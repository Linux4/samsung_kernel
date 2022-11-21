#include "../../accelerometer.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../sensormanager/shub_sensor_type.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../utility/shub_utility.h"

#include <linux/device.h>
#include <linux/of_gpio.h>

#define LIS2DLC12_NAME		"LIS2DLC12"
static void parse_dt_accelerometer_lis2dlc12(struct device *dev)
{
	struct accelerometer_data *data = get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;
	struct device_node *np = dev->of_node;

	if (of_property_read_u32(np, "acc-lis2dlc12-position", &data->position))
		data->position = 0;
	shub_infof("position[%d]", data->position);
}

struct accelerometer_chipset_funcs accel_lis2dlc12_ops = {
	.parse_dt = parse_dt_accelerometer_lis2dlc12,
};

struct accelerometer_chipset_funcs *get_accelometer_lis2dlc12_function_pointer(char *name)
{
	if (strcmp(name, LIS2DLC12_NAME) != 0)
		return NULL;

	return &accel_lis2dlc12_ops;
}
