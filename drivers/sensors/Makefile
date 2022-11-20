#
# Makefile for grip sensors
#

obj-$(CONFIG_SENSORS_A96T3X6)		+= a96t3x6.o
obj-$(CONFIG_SENSORS_A96T3X6_SUB)	+= a96t3x6_sub.o
obj-$(CONFIG_SENSORS_ISG5320A)		+= isg5320a.o
obj-$(CONFIG_SENSORS_ISG5320A_SUB)	+= isg5320a_sub.o
obj-$(CONFIG_SENSORS_SX9360)		+= sx9360.o
obj-$(CONFIG_SENSORS_SX9360_SUB)	+= sx9360_sub.o
obj-$(CONFIG_SENSORS_ISG5320A_A02)	+= isg5320a_a02.o
obj-$(CONFIG_SENSORS_ISG5320A_SUB_A02)	+= isg5320a_sub_a02.o

# Each configuration option enables a list of files.

obj-$(CONFIG_SENSORS_CORE)            += sensors_core.o

# accelerometer_sensor

obj-$(CONFIG_SENSORS_LIS2DS)            += lis2ds_core.o
obj-$(CONFIG_SENSORS_LIS2DS_I2C)            += lis2ds_i2c.o
obj-$(CONFIG_SENSORS_LIS2DS_SPI)            += lis2ds_spi.o
obj-$(CONFIG_SENSORS_LIS2DS_A02)            += lis2ds_core_a02.o
obj-$(CONFIG_SENSORS_LIS2DS_I2C_A02)            += lis2ds_i2c_a02.o
obj-$(CONFIG_SENSORS_LIS2DS_SPI_A02)            += lis2ds_spi_a02.o

