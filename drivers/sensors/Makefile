#
# Makefile for the sensors drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_SENSORS)		+= sensors_core.o

# combo chipset
obj-$(CONFIG_SENSORS_BMI055)	+= bma255.o bmg160.o
obj-$(CONFIG_SENSORS_BMI058)	+= bma280.o bmg160.o
# magnetic_sensor
obj-$(CONFIG_SENSORS_AK8963C)	+= ak8963c.o
obj-$(CONFIG_SENSORS_AK09911C)	+= ak09911c.o
# optical_sensor
obj-$(CONFIG_SENSORS_CM3323)	+= cm3323.o
obj-$(CONFIG_SENSORS_CM36653)	+= cm36653.o
obj-$(CONFIG_SENSORS_GP2A030A)     += gp2ap030.o
obj-$(CONFIG_SENSORS_TMD3782S) += taos_tmd3782.o
# grip_sensor
obj-$(CONFIG_SENSORS_SX9500)	+= sx9500.o
# 6-axis_sensor
obj-$(CONFIG_INPUT_MPU6500)     += mpu6500/
obj-$(CONFIG_SENSORS_YAS532)     += yas532/
obj-$(CONFIG_SENSORS_K3DH)	+= accelerometer_k2dh/
obj-$(CONFIG_SENSORS_K2HH)	+= lis2hh_acc.o
