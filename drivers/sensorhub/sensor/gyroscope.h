#ifndef __SHUB_GYROSCOPE_COMMON_H__
#define __SHUB_GYROSCOPE_COMMON_H__

#include <linux/types.h>
#include <linux/device.h>

/* Gyroscope DPS */
#define GYROSCOPE_DPS250  250
#define GYROSCOPE_DPS500  500
#define GYROSCOPE_DPS2000 2000

#define VERBOSE_OUT	       (1)
#define DEF_GYRO_FULLSCALE     (2000)
#define DEF_GYRO_SENS	       (32768 / DEF_GYRO_FULLSCALE)
#define DEF_SCALE_FOR_FLOAT    (10000)
#define DEF_RMS_SCALE_FOR_RMS  (10000)
#define DEF_SQRT_SCALE_FOR_RMS (100)
#define GYRO_LIB_DL_FAIL       (9990)

struct gyro_event {
	s16 x;
	s16 y;
	s16 z;
	//u32 gyro_dps;
} __attribute__((__packed__));

struct uncal_gyro_event {
	s16 uncal_x;
	s16 uncal_y;
	s16 uncal_z;
	s16 offset_x;
	s16 offset_y;
	s16 offset_z;
} __attribute__((__packed__));

struct calibraion_data {
	s16 x;
	s16 y;
	s16 z;
};

struct gyroscope_data {
	struct gyroscope_chipset_funcs *chipset_funcs;

	struct calibraion_data cal_data;
	int position;
};

struct gyroscope_chipset_funcs {
	void (*parse_dt)(struct device *dev);
};

struct gyroscope_chipset_funcs *get_gyroscope_lsm6dsl_function_pointer(char *name);
struct gyroscope_chipset_funcs *get_gyroscope_icm42605m_function_pointer(char *name);
struct gyroscope_chipset_funcs *get_gyroscope_lsm6dsotr_function_pointer(char *name);

int save_gyro_calibration_file(s16 *cal_data);

#endif /* __SHUB_GYROSCOPE_COMMON_H_ */
