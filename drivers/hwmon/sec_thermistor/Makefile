# SPDX-License-Identifier: GPL-2.0
#
# Makefile for sensor chip drivers.
#
obj-$(CONFIG_SENSORS_SEC_THERMISTOR)	+= sec_thermistor.o

ccflags-$(CONFIG_HWMON_DEBUG_CHIP) := -DDEBUG


GCOV_PROFILE := $(CONFIG_KUNIT)
obj-$(CONFIG_KUNIT) += test/
