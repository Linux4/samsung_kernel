#ifndef _SPRD_THM_H_
#define _SPRD_THM_H_

#include <linux/thermal.h>

#define COOLING_DEV_MAX 8

enum sprd_thm_sensor_id {
	SPRD_ARM_SENSOR = 0,
	SPRD_PMIC_SENSOR = 1,
	SPRD_MAX_SENSOR = 2,
};
struct sprd_trip_point {
	unsigned long temp;
	enum thermal_trip_type type;
	char cdev_name[COOLING_DEV_MAX][THERMAL_NAME_LENGTH];
};
struct sprd_thm_platform_data {
	struct sprd_trip_point trip_points[THERMAL_MAX_TRIPS];
	int num_trips;
};
extern int sprd_thm_temp_read(u32 sensor);

#endif /**/
