#ifndef _SPRD_THM_H_
#define _SPRD_THM_H_

#include <linux/thermal.h>

#define COOLING_DEV_MAX 8
#define THM_DEV_MAX 8
#define THM_TEMP_DEGREE_SETP   		15
#define THM_TEMP_DEGREE_START  		105
#define THM_TEMP_DEGREE_CRITICAL 	114

enum sprd_thm_sensor_id {
	SPRD_ARM_SENSOR = 0,
	SPRD_GPU_SENSOR = 1,
	SPRD_PMIC_SENSOR = 2,
	SPRD_BCORE_SENSOR = 3,
	SPRD_BOARD_SENSOR= 4,
	SPRD_MAX_SENSOR = 5,
};
struct sprd_trip_point {
	unsigned long temp;
	unsigned long lowoff;
	enum thermal_trip_type type;
	char cdev_name[COOLING_DEV_MAX][THERMAL_NAME_LENGTH];
};

struct sprd_thm {
	struct sprd_thm_platform_data *sprd_data;
	int nsensor;
};
struct sprd_thm_platform_data {
	struct sprd_trip_point trip_points[THERMAL_MAX_TRIPS];
	int num_trips;
	int sensor_id;
	int temp_interval;
	char thm_name[THERMAL_NAME_LENGTH];
};
struct sprdboard_table_data {
	int x;
	int y;
};
struct sprd_board_sensor_config {
    int temp_support;	/*0:do NOT support temperature,1:support temperature */
	int temp_adc_ch;
	int temp_adc_scale;
	int temp_adc_sample_cnt;	/*1-15,should be lower than 16 */
	int temp_table_mode;	//0:adc-degree;1:voltage-degree
	struct sprdboard_table_data *temp_tab;	/* OCV curve adjustment */
	int temp_tab_size;
};
extern int  sprd_thermal_temp_get(enum sprd_thm_sensor_id thermal_id,unsigned long *temp);
#endif /**/
