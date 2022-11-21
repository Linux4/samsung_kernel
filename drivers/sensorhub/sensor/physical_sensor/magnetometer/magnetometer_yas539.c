#include "../../../comm/shub_comm.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../utility/shub_utility.h"
#include "../../magnetometer.h"

#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

#define YAS539_NAME   "YAS539"

static void init_mag_yas539(void)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;

	data->mag_matrix_len = 18;
	data->cal_data_len = sizeof(struct calibration_data_yas539);
}

static void parse_dt_magnetometer_yas539(struct device *dev)
{
	struct magnetometer_data *data = get_sensor(SENSOR_TYPE_GEOMAGNETIC_FIELD)->data;
	struct device_node *np = dev->of_node;
	int check_mst_gpio, check_nfc_gpio;
	int value_mst = 0, value_nfc = 0;

	if (of_property_read_u32(np, "mag-yas539-position", &data->position)) {
		data->position = 0;
		shub_err("no mag-yas539-position, set as 0");
	}

	shub_info("position[%d]", data->position);

	if (of_property_read_u16_array(np, "mag-yas539-array", (s16 *)data->mag_matrix, data->mag_matrix_len/2))
		shub_err("no mag-yas539-array, set as 0");

	// check nfc/mst for mag matrix
	check_nfc_gpio = of_get_named_gpio(np, "shub-mag-check-nfc", 0);
	if (check_nfc_gpio >= 0)
		value_nfc = gpio_get_value(check_nfc_gpio);

	check_mst_gpio = of_get_named_gpio(np, "shub-mag-check-mst", 0);
	if (check_mst_gpio >= 0) {
		value_mst = gpio_get_value(check_mst_gpio);
		if (value_mst == 1) {
			shub_info("mag matrix(%d %d) nfc/mst array", value_nfc, value_mst);
			if (of_property_read_u16_array(np, "mag-yas539-mst-array", (s16 *)data->mag_matrix,
						       data->mag_matrix_len/2))
				shub_err("no mag-yas539-mst-array");
		} else if (value_nfc == 1) {
			shub_info("mag matrix(%d %d) nfc only array", value_nfc, value_mst);
			if (of_property_read_u16_array(np, "mag-yas539-nfc-array", (s16 *)data->mag_matrix,
						       data->mag_matrix_len/2))
				shub_err("no mag-yas539-nfc-array");
		}
	}
}

struct magnetometer_chipset_funcs magnetic_yas539_ops = {
	.init = init_mag_yas539,
	.parse_dt = parse_dt_magnetometer_yas539,
};

struct magnetometer_chipset_funcs *get_magnetic_yas539_function_pointer(char *name)
{
	if (strcmp(name, YAS539_NAME) != 0)
		return NULL;

	return &magnetic_yas539_ops;
}
