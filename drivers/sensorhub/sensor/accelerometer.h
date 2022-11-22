#ifndef __SHUB_ACCELEROMETER_H_
#define __SHUB_ACCELEROMETER_H_

#include <linux/types.h>
#include <linux/device.h>

#define ACCEL_CALIBRATION_FILE_PATH "/efs/FactoryApp/calibration_data"

struct accel_event {
	s16 x;
	s16 y;
	s16 z;
} __attribute__((__packed__));

struct uncal_accel_event {
	s16 uncal_x;
	s16 uncal_y;
	s16 uncal_z;
	s16 offset_x;
	s16 offset_y;
	s16 offset_z;
} __attribute__((__packed__));

struct calibration_data {
	s16 x;
	s16 y;
	s16 z;
};

struct accelerometer_chipset_funcs {
	void (*parse_dt)(struct device *dev);
};

struct accelerometer_data {
	int position;
	struct calibration_data cal_data;
	bool is_accel_alert;

	struct accelerometer_chipset_funcs *chipset_funcs;
};

struct accelerometer_chipset_funcs *get_accelometer_lsm6dsl_function_pointer(char *name);
struct accelerometer_chipset_funcs *get_accelometer_icm42605m_function_pointer(char *name);
struct accelerometer_chipset_funcs *get_accelometer_lis2dlc12_function_pointer(char *name);
struct accelerometer_chipset_funcs *get_accelometer_lsm6dsotr_function_pointer(char *name);

int set_accel_cal(struct accelerometer_data *data);
#endif
