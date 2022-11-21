#include "../../comm/shub_comm.h"
#include "../../sensormanager/shub_sensor.h"
#include "../../sensormanager/shub_sensor_manager.h"
#include "../../utility/shub_utility.h"

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

/*************************************************************************/
/* factory Sysfs                                                         */
/*************************************************************************/

#define LSM6DSL_NAME	"LSM6DSL"
#define LSM6DSL_VENDOR	"STM"

static ssize_t name_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", LSM6DSL_NAME);
}

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n", LSM6DSL_VENDOR);
}

static ssize_t selftest_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char *buffer = NULL;
	int buffer_length = 0;
	s8 init_status = 0, result = -1;
	u16 diff_axis[3] = {0, };
	u16 shift_ratio_N[3] = {0, };
	int ret = 0;

	ret = shub_send_command_wait(CMD_GETVALUE, SENSOR_TYPE_ACCELEROMETER, SENSOR_FACTORY, 3000, NULL, 0, &buffer,
				     &buffer_length, true);

	if (ret < 0) {
		shub_errf("shub_send_command_wait Fail %d", ret);
		ret = sprintf(buf, "%d,%d,%d,%d\n", -5, 0, 0, 0);
		return ret;
	}

	if (buffer_length != 14) {
		shub_errf("length err %d", buffer_length);
		ret = sprintf(buf, "%d,%d,%d,%d\n", -5, 0, 0, 0);
		kfree(buffer);
		return -EINVAL;
	}

	init_status = buffer[0];
	diff_axis[0] = ((s16)(buffer[2] << 8)) + buffer[1];
	diff_axis[1] = ((s16)(buffer[4] << 8)) + buffer[3];
	diff_axis[2] = ((s16)(buffer[6] << 8)) + buffer[5];

	/* negative axis */

	shift_ratio_N[0] = ((s16)(buffer[8] << 8)) + buffer[7];
	shift_ratio_N[1] = ((s16)(buffer[10] << 8)) + buffer[9];
	shift_ratio_N[2] = ((s16)(buffer[12] << 8)) + buffer[11];
	result = buffer[13];

	shub_infof("%d, %d, %d, %d, %d, %d, %d, %d\n", init_status, result, diff_axis[0], diff_axis[1], diff_axis[2],
		   shift_ratio_N[0], shift_ratio_N[1], shift_ratio_N[2]);

	ret = sprintf(buf, "%d,%d,%d,%d,%d,%d,%d\n", result, diff_axis[0], diff_axis[1], diff_axis[2], shift_ratio_N[0],
		      shift_ratio_N[1], shift_ratio_N[2]);

	kfree(buffer);

	return ret;
}

static DEVICE_ATTR_RO(name);
static DEVICE_ATTR_RO(vendor);
static DEVICE_ATTR_RO(selftest);

static struct device_attribute *acc_lsm6dsl_attrs[] = {
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_selftest,
	NULL,
};

struct device_attribute **get_accelerometer_lsm6dsl_dev_attrs(char *name)
{
	if (strcmp(name, LSM6DSL_NAME) != 0)
		return NULL;

	return acc_lsm6dsl_attrs;
}
