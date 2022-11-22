#include "fingerprint_common.h"

void set_sensor_type(const int type_value, int *sensortype)
{
	if (type_value >= SENSOR_OOO && type_value < SENSOR_MAXIMUM) {
		if (type_value == SENSOR_OOO && *sensortype == SENSOR_FAILED) {
			pr_info("maintain type check from out of order :%s\n",
				sensor_status[*sensortype + 2]);
		} else {
			*sensortype = type_value;
			pr_info("FP_SET_SENSOR_TYPE :%s\n", sensor_status[*sensortype + 2]);
		}
	} else {
		pr_err("FP_SET_SENSOR_TYPE invalid value %d\n", type_value);
		*sensortype = SENSOR_UNKNOWN;
	}
}
