#
# Makefile for the sensors drivers.
#

# Each configuration option enables a list of files.

obj-$(CONFIG_SENSORS)          += sensors_core.o

# accelerometer_sensor
obj-$(CONFIG_SENSORS_K2HH)          += k2hh.o
obj-$(CONFIG_SENSORS_BMA2X2)        += bma2x2.o
# optical_sensor
obj-$(CONFIG_SENSORS_BH1733)	    += bh1733.o
obj-$(CONFIG_SENSORS_TCS3490)	    += tcs3490.o
obj-$(CONFIG_SENSORS_CM3323)	    += cm3323.o
obj-$(CONFIG_SENSORS_CM36652)	    += cm36652.o
obj-$(CONFIG_SENSORS_CM36655)       += cm36655.o
obj-$(CONFIG_SENSORS_CM36672P)     += cm36672p.o
obj-$(CONFIG_SENSORS_CM36686)       += cm36686.o
obj-$(CONFIG_SENSORS_TMD3782)       += tmd3782.o
obj-$(CONFIG_SENSORS_TMD3700)       += tmd37xx.o
obj-$(CONFIG_SENSORS_STK3013)       += stk3013.o
# grip_sensor
obj-$(CONFIG_SENSORS_SX9306)		+= sx9306.o
obj-$(CONFIG_SENSORS_SX9310)        += sx9310.o
obj-$(CONFIG_SENSORS_SX9310_WIFI)   += sx9310_wifi.o
obj-$(CONFIG_SENSORS_SX9500)        += sx9500.o
obj-$(CONFIG_SENSORS_SX9500_WIFI)   += sx9500_wifi.o
# hrm_sensor
obj-$(CONFIG_SENSORS_PPSX60)		+= ppsx60.o
obj-$(CONFIG_SENSORS_MAX86900)		+= max86900.o
obj-$(CONFIG_SENSORS_MAX86902)		+= max86902.o
obj-$(CONFIG_SENSORS_MAX_NOTCHFILTER)	+= max_notchfilter/
# magnetic_sensor
obj-$(CONFIG_SENSORS_BMM050)	+= bmm050_driver.o bmm050.o
obj-$(CONFIG_SENSORS_YAS537)	+= yas_mag_drv-yas537.o yas_mag_kernel.o
