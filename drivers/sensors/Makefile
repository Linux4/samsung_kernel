#
#  Makefile for the kernel Sensor device drivers.
#

# Object files in subdirectories
obj-$(CONFIG_INPUT_YAS_MAGNETOMETER)		+= geomagnetic/
obj-$(CONFIG_INPUT_YAS_ORIENTATION) +=  yas532_orient.o
obj-y	+= optical/
obj-$(CONFIG_SENSORS_CORE)		+= sensors_core.o
obj-$(CONFIG_SENSORS_AD7146) += ad7146.o
obj-$(CONFIG_INPUT_BMP180)		+= bmp180.o
obj-$(CONFIG_SENSORS_K2DH)		+= k2dh.o
obj-$(CONFIG_SENSORS_ALPS)		+= alps-input.o
obj-$(CONFIG_SENSORS_ALPS_MAG_HSCDTD008A)		+= hscdtd008a_i2c.o
obj-$(CONFIG_SENSORS_ALPS_ACC_BMA254)		+= alps_acc_bma254.o
