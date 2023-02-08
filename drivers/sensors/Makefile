#
# Makefile for the sensors drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_SENSORS)		+= sensors_core.o
obj-$(CONFIG_SENSORS_SSC)	+= sensors_ssc.o

# accelerometer_sensor
obj-$(CONFIG_SENSORS_K2HH)	+= k2hh.o
# optical_sensor
obj-$(CONFIG_SENSORS_CM36672P)	+= cm36672p.o
obj-$(CONFIG_SENSORS_CM36652)           += cm36652.o
obj-$(CONFIG_SENSORS_CM36686)	+= cm36686.o
obj-$(CONFIG_SENSORS_GP2A002)	    += gp2a002.o
obj-$(CONFIG_SENSORS_TCS3490)	    += tcs3490.o
# grip_sensor
obj-$(CONFIG_SENSORS_SX9500)        +=sx9500.o
obj-$(CONFIG_SENSORS_SX9500_WIFI)   +=sx9500_wifi.o
# magnetic_sensor
obj-$(CONFIG_SENSORS_K303C)	+= k303c_acc.o k303c_mag.o
obj-$(CONFIG_SENSORS_BMA2X2)	+= bma2x2.o
obj-$(CONFIG_SENSORS_BMM050)	+= bmm050_driver.o bmm050.o
