# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the kernel battery drivers.
#

# Object files in subdirectories

ifeq ($(CONFIG_BATTERY_SAMSUNG_V3), y)
	obj-$(CONFIG_BATTERY_SAMSUNG)	+= v3/
else
	obj-$(CONFIG_BATTERY_SAMSUNG)	+= fuelgauge/ charger/ wireless/ common/
endif
