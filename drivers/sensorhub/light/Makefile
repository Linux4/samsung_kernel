#
# Makefile for the sensor drivers.
#

# Each configuration option enables a list of files.
obj-y 	+= light_sensor.o
obj-$(CONFIG_SENSORS_SSP_LIGHT_TMD3725)       += light_tmd3725.o
obj-$(CONFIG_SENSORS_SSP_LIGHT_TMD3700) += light_tmd3700.o
obj-$(CONFIG_SENSORS_SSP_LIGHT_TCS3701) += light_tcs3701.o
obj-$(CONFIG_SENSORS_SSP_LIGHT_STK3X6X) += light_stk3x6x.o
obj-$(CONFIG_SENSORS_SSP_LIGHT_VEML3328) += light_veml3328.o
obj-$(CONFIG_SENSORS_SSP_LIGHT_STK3A5X) += light_stk3a5x.o
obj-$(CONFIG_SENSORS_SSP_LIGHT_STK3328) += light_stk3328.o
