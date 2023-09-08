#ifndef MTK_OIS_MCU_SYSFS_H
#define MTK_OIS_MCU_SYSFS_H

int ois_mcu_gyro_calibration(struct mcu_info *mcu_info, struct i2c_client *client,
	long *raw_data_x, long *raw_data_y);
int ois_mcu_get_offset_data(struct mcu_info *mcu_info, struct i2c_client *client,
	long *out_gyro_x, long *out_gyro_y);
int ois_mcu_get_offset_testdata(struct mcu_info *mcu_info, struct i2c_client *client,
	long *out_gyro_x, long *out_gyro_y);
int ois_mcu_selftest(struct mcu_info *mcu_info, struct i2c_client *client);
bool ois_mcu_sine_wavecheck(int *sin_x, int *sin_y, int *result, struct i2c_client *client, int threshold);
#endif
