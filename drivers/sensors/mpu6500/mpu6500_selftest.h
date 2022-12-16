#ifndef _MPU6500_SELFTEST_H_
#define _MPU6500_SELFTEST_H_

#define MPU6500_HWST_ACCEL 0x0001
#define MPU6500_HWST_GYRO  0x0010
#define MPU6500_HWST_ALL \
	(MPU6500_HWST_ACCEL | MPU6500_HWST_GYRO)

int mpu6500_selftest_run(struct i2c_client *client,
			int packet_cnt[3],
			int gyro_bias[3],
			int gyro_rms[3],
			int gyro_lsb_bias[3]);

int mpu6500_hw_self_check(struct i2c_client *client, int gyro_ratio[3],
	int accel_ratio[3], int sensors);

#endif
