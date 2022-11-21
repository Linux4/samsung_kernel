#
# Makefile for the sensor drivers.
#

# Each configuration option enables a list of files.
obj-$(CONFIG_SENSORS_SSP_BMI058)	+= accel_bmi058.o gyro_bmi058.o
obj-$(CONFIG_SENSORS_SSP_BMI160)	+= accel_bmi160.o gyro_bmi160.o
obj-$(CONFIG_SENSORS_SSP_MPU6500)	+= accel_mpu6500.o gyro_mpu6500.o
obj-$(CONFIG_SENSORS_SSP_ICM20610)	+= accel_icm20610.o gyro_icm20610.o
obj-$(CONFIG_SENSORS_SSP_YAS532)	+= magnetic_yas532.o
obj-$(CONFIG_SENSORS_SSP_AK09911C)	+= magnetic_ak09911c.o

obj-$(CONFIG_SENSORS_SSP_TMG399X)	+= light_tmg399x.o prox_tmg399x.o gesture_tmg399x.o
obj-$(CONFIG_SENSORS_SSP_LPS25H)	+= pressure_lps25h.o

obj-$(CONFIG_SENSORS_SSP_ATUC128L5HAR)	+= mcu_atuc128l5har.o

obj-$(CONFIG_SENSORS_SSP_MOBEAM)	 += barcode_emul_tmg3992.o
