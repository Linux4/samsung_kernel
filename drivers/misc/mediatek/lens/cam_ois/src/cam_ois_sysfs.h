#ifndef CAM_OIS_SYSFS_H
#define CAM_OIS_SYSFS_H

#include "cam_ois_mcu_fw.h"

int cam_ois_gyro_calibration(struct mcu_info *mcu_info, struct i2c_client *client,
	long *raw_data_x, long *raw_data_y, long *raw_data_z);
int cam_ois_get_offset_data(struct mcu_info *mcu_info, struct i2c_client *client,
	const char *buf, int size, long *out_gyro_x, long *out_gyro_y, long *out_gyro_z);
int cam_ois_get_offset_testdata(struct mcu_info *mcu_info, struct i2c_client *client,
	long *out_gyro_x, long *out_gyro_y, long *out_gyro_z);
int cam_ois_selftest(struct mcu_info *mcu_info, struct i2c_client *client);
int cam_ois_sine_wavecheck(int *sin_x, int *sin_y, int *result, struct i2c_client *client, int threshold);

int cam_ois_sysfs_init(void);
void cam_ois_destroy_sysfs(void);

#endif
