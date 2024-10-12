# SPDX-License-Identifier: GPL-2.0

COMMON_SENSOR := gc5035_mipi_raw
ifneq ($(findstring $(COMMON_SENSOR), $(CONFIG_CUSTOM_KERNEL_IMGSENSOR)), )
$(info $(COMMON_SENSOR) device driver is already in use)
obj-y += gc5035u_dummy.o
else
$(info use $(COMMON_SENSOR) device driver)
obj-y += ../$(COMMON_SENSOR)/gc5035mipiraw_Sensor.o
endif
