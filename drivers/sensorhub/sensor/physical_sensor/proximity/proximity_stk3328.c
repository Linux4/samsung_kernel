#include "../../../comm/shub_comm.h"
#include "../../../sensor/proximity.h"
#include "../../../sensorhub/shub_device.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../utility/shub_utility.h"

#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define STK3328_NAME   "STK3328"

void parse_dt_proximity_stk3328(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (of_property_read_u16_array(np, "prox-stk3328-thresh", data->prox_threshold, PROX_THRESH_SIZE))
		shub_err("no prox-stk3328-thresh, set as 0");

	shub_info("thresh %u, %u", data->prox_threshold[PROX_THRESH_HIGH], data->prox_threshold[PROX_THRESH_LOW]);
}

struct proximity_chipset_funcs prox_stk3328_ops = {
	.parse_dt = parse_dt_proximity_stk3328,
};

struct proximity_chipset_funcs *get_proximity_stk3328_function_pointer(char *name)
{
	if (strcmp(name, STK3328_NAME) != 0)
		return NULL;

	return &prox_stk3328_ops;
}
