#ifndef _MPU6500_LP_H_
#define _MPU6500_LP_H_


#define DMP_START_ADDR						0x400
#define DMP_ANGLE_SCALE						15
#define DMP_ORIENTATION_ANGLE			30
#define DMP_ORIENTATION_TIME			30
#define DMP_DEFAULT_FIFO_RATE			200

#define MPU6500_X_UP							0x01
#define MPU6500_X_DOWN						0x02
#define MPU6500_Y_UP							0x04
#define MPU6500_Y_DOWN						0x08
#define MPU6500_Z_UP							0x10
#define MPU6500_Z_DOWN						0x20
#define MPU6500_ORIENTATION_ALL		0x3F
#define MPU6500_ORIENTATION_XY		0x0F

#define MPU6500_ORIENTATION_FLIP              0x40
#define MPU6500_X_AXIS_INDEX                  0x00
#define MPU6500_Y_AXIS_INDEX                  0x01
#define MPU6500_Z_AXIS_INDEX                  0x02


#define MPU6500_ELEMENT_1                     0x0001
#define MPU6500_ELEMENT_2                     0x0002
#define MPU6500_ELEMENT_3                     0x0004
#define MPU6500_ELEMENT_4                     0x0008
#define MPU6500_ELEMENT_5                     0x0010
#define MPU6500_ELEMENT_6                     0x0020
#define MPU6500_ELEMENT_7                     0x0040
#define MPU6500_ELEMENT_8                     0x0080
#define MPU6500_ALL                           0xFFFF
#define MPU6500_ELEMENT_MASK                  0x00FF
#define MPU6500_GYRO_ACC_MASK                 0x007E


int mpu6500_lp_init_dmp(struct i2c_client *i2c_client, __s8 orientation[9]);
int mpu6500_lp_activate_screen_orientation(struct i2c_client *i2c_client,
	bool enable, int current_delay);
int mpu6500_lp_set_delay(struct i2c_client *i2c_client, int current_delay);
int mpu6500_lp_set_interrupt_on_gesture_event(struct i2c_client *i2c_client,
	bool on);

#endif
