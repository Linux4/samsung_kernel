#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/of_gpio.h>
#include <linux/uaccess.h>

#include "../../../sensor/proximity.h"
#include "../../../sensormanager/shub_sensor.h"
#include "../../../sensormanager/shub_sensor_manager.h"
#include "../../../comm/shub_comm.h"
#include "../../../sensorhub/shub_device.h"
#include "../../../utility/shub_utility.h"
#include "../../../utility/shub_file_manager.h"

#define STK3X6X_NAME   "STK33617"
#define STK3X6X_VENDOR "Sitronix"

#define PROX_CALIBRATION_FILE_MODE2_PATH "/efs/FactoryApp/prox_cal_data2"

static u8 prox_thresh_mode;
static u16 prox_thresh_addval[PROX_THRESH_SIZE];

void init_proximity_stk3x6x_variable(struct proximity_data *data)
{
	struct proximity_stk3x6x_data *thd_data = data->threshold_data;

	data->need_compensation = true;
	thd_data->prox_cal_mode = 0;
}

void parse_dt_proximity_stk3x6x(struct device *dev)
{
	struct device_node *np = dev->of_node;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	if (of_property_read_u16_array(np, "prox-stk33617-thresh", data->prox_threshold, PROX_THRESH_SIZE))
		shub_err("no prox-stk33617-thresh, set as 0");

	shub_info("thresh %u, %u", data->prox_threshold[PROX_THRESH_HIGH], data->prox_threshold[PROX_THRESH_LOW]);

	if (of_property_read_u16_array(np, "prox-stk33617-thresh-addval", prox_thresh_addval,
				       sizeof(prox_thresh_addval)))
		shub_errf("no prox-stk33617-thresh_addval, set as 0");

	shub_infof("thresh-addval - %u, %u", prox_thresh_addval[PROX_THRESH_HIGH], prox_thresh_addval[PROX_THRESH_LOW]);
}

u8 get_proximity_stk3x6x_threshold_mode(void)
{
	return prox_thresh_mode;
}

void set_proximity_stk3x6x_threshold_mode(u8 mode)
{
	prox_thresh_mode = mode;
}

int proximity_open_calibration(void)
{
	int ret = 0;
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	ret = shub_file_read(PROX_CALIBRATION_FILE_PATH,
			     (char *)&data->prox_threshold, sizeof(data->prox_threshold), 0);
	if (ret != sizeof(data->prox_threshold)) {
		ret = shub_file_read(PROX_CALIBRATION_FILE_MODE2_PATH,
				     (char *)&data->prox_threshold, sizeof(data->prox_threshold), 0);
		if (ret != sizeof(data->prox_threshold)) {
			shub_errf("failed");
			ret = -EIO;
		} else {
			prox_thresh_mode = 2;
		}
	} else {
		prox_thresh_mode = 1;
	}

	shub_infof("mode %d, thresh %d, %d", prox_thresh_mode, data->prox_threshold[0], data->prox_threshold[1]);

	return ret;
}

int save_prox_cal_threshold_data(struct proximity_data *data)
{
	int ret = 0;
	struct proximity_stk3x6x_data *thd_data = data->threshold_data;

	shub_infof("mode %d thresh %d, %d ", thd_data->prox_cal_mode,
		data->prox_threshold[0], data->prox_threshold[1]);

	if (thd_data->prox_cal_mode == 1) {
		ret = shub_file_write(PROX_CALIBRATION_FILE_PATH, (char *)&data->prox_threshold,
				      sizeof(data->prox_threshold), 0);
	} else if (thd_data->prox_cal_mode == 2) {
		ret = shub_file_write(PROX_CALIBRATION_FILE_MODE2_PATH, (char *)&data->prox_threshold,
				      sizeof(data->prox_threshold), 0);
	} else {
		return -EINVAL;
	}

	thd_data->prox_cal_mode = 0;
	if (ret != sizeof(data->prox_threshold)) {
		shub_errf("can't write prox cal to file");
		ret = -EIO;
	}

	return ret;
}

void pre_report_event_proximity_stk3x6x(void)
{
	struct proximity_data *data = get_sensor(SENSOR_TYPE_PROXIMITY)->data;

	save_prox_cal_threshold_data(data);
	proximity_open_calibration();
}

int init_proximity_stk3x6x(struct proximity_data *data)
{
	if (data->threshold_data == NULL) {
		data->threshold_data = kzalloc(sizeof(struct proximity_stk3x6x_data), GFP_KERNEL);
		if (!data->threshold_data)
			return -ENOMEM;
	}

	return 0;
}

struct proximity_chipset_funcs prox_stk3x6x_ops = {
	.init = init_proximity_stk3x6x,
	.init_proximity_variable = init_proximity_stk3x6x_variable,
	.get_proximity_threshold_mode = get_proximity_stk3x6x_threshold_mode,
	.set_proximity_threshold_mode = set_proximity_stk3x6x_threshold_mode,
	.pre_report_event_proximity = pre_report_event_proximity_stk3x6x,
	.parse_dt = parse_dt_proximity_stk3x6x,
	.open_calibration_file = proximity_open_calibration,
};

struct proximity_chipset_funcs *get_proximity_stk3x6x_function_pointer(char *name)
{
	if (strcmp(name, STK3X6X_NAME) != 0)
		return NULL;

	return &prox_stk3x6x_ops;
}
