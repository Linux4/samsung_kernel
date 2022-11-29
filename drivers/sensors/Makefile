#
# Makefile for the sensors drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_SENSORS_CORE)          += sensors_core.o

# accelerometer_sensor

obj-$(CONFIG_SENSORS_LIS2DS)            += lis2ds_core.o
obj-$(CONFIG_SENSORS_LIS2DS_I2C)            += lis2ds_i2c.o
obj-$(CONFIG_SENSORS_LIS2DS_SPI)            += lis2ds_spi.o

# optical_sensor
obj-$(CONFIG_SENSORS_STK3013)       += stk3013.o

#grip_sensor
obj-$(CONFIG_SENSORS_A96T3X6)		+= a96t3x6.o

