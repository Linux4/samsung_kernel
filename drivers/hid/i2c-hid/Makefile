# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile for the I2C input drivers
#

obj-$(CONFIG_I2C_HID)				+= i2c-hid.o

i2c-hid-objs					=  i2c-hid-core.o
i2c-hid-$(CONFIG_DMI)				+= i2c-hid-dmi-quirks.o

#+P230620-03833, caoxin2.wt, add, 2023.06.23, detect kbd connect status
obj-$(CONFIG_POGOPIN_HID)				+= kbd-connect.o
kbd-connect-y				:= pogopin-keyboard.o
#-P230620-03833, caoxin2.wt, add, 2023.06.23, detect kbd connect status