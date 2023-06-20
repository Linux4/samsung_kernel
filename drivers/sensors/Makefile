#
#  Makefile for the kernel Sensor device drivers.
#

# Object files in subdirectories
obj-$(CONFIG_SENSORS_CORE)		+= sensors_core.o
obj-$(CONFIG_SENSORS_HSCDTD008A)	+= hscdtd008a_i2c.o alps-input.o
obj-$(CONFIG_INPUT_MPU6500)		+= mpu6500/
obj-$(CONFIG_BMA250_ACC_DRIVER) += accelerometer_bma250/
obj-$(CONFIG_INPUT_YAS_ACCELEROMETER)	+=accelerometer/
obj-$(CONFIG_PROXIMITY_SENSOR)	+= optical/
obj-$(CONFIG_SENSORS_K3DH)	+= accelerometer_k2dh/
obj-$(CONFIG_SENSORS_ASP01)	+= grip_sensor/
obj-$(CONFIG_INPUT_YAS_MAGNETOMETER)	+=geomagnetic/