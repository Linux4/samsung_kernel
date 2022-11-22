#include "../../gyroscope.h"
#include "../../accelerometer.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../sensormanager/shub_sensor_type.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../utility/shub_utility.h"

#include <linux/device.h>

#define ICM42605M_NAME		"ICM42605M"
static void parse_dt_gyroscope_icm24605m(struct device *dev)
{
	struct accelerometer_data *acc_data =  get_sensor(SENSOR_TYPE_ACCELEROMETER)->data;
	struct gyroscope_data *data = get_sensor(SENSOR_TYPE_GYROSCOPE)->data;

	data->position = acc_data->position;
	shub_infof("position[%d]", data->position);
}

struct gyroscope_chipset_funcs gyro_icm42605m_ops = {
	.parse_dt = parse_dt_gyroscope_icm24605m,
};

struct gyroscope_chipset_funcs *get_gyroscope_icm42605m_function_pointer(char *name)
{
	if (strcmp(name, ICM42605M_NAME) != 0)
		return NULL;

	return &gyro_icm42605m_ops;
}
