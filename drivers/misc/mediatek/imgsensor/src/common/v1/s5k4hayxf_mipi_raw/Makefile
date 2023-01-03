# SPDX-License-Identifier: GPL-2.0

COMMON_SENSOR := s5k4hayx_mipi_raw
ifneq ($(findstring $(COMMON_SENSOR), $(CONFIG_CUSTOM_KERNEL_IMGSENSOR)), )
$(info $(COMMON_SENSOR) device driver is already in use)
obj-y += s5k4hayxf_dummy.o
else
$(info use $(COMMON_SENSOR) device driver)
obj-y += ../$(COMMON_SENSOR)/s5k4hayxmipiraw_Sensor.o
endif
