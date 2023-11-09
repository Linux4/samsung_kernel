#
# Makefile for the sensors drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_SENSORS)		+= sensors_core.o

# accelerometer_sensor
obj-$(CONFIG_SENSORS_K2HH)	+= k2hh.o
obj-$(CONFIG_SENSORS_BMA254)	+= bma254.o

# optical_sensor
obj-$(CONFIG_SENSORS_CM36672P)  += cm36672p.o
obj-$(CONFIG_SENSORS_CM36686)	+= cm36686.o
obj-$(CONFIG_SENSORS_GP2A002)	+= gp2a002.o
obj-$(CONFIG_SENSORS_SX9500) += sx9500.o
obj-$(CONFIG_SENSORS_SX9500_WIFI) += sx9500_wifi.o
