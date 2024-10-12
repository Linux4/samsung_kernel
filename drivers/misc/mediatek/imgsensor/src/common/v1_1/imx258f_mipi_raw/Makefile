# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 Samsung Electronics Inc.

COMMON_SENSOR := imx258_mipi_raw
ifneq ($(findstring $(COMMON_SENSOR), $(CONFIG_CUSTOM_KERNEL_IMGSENSOR)), )
$(info $(COMMON_SENSOR) device driver is already in use)
obj-y += imx258f_dummy.o
else
$(info use $(COMMON_SENSOR) device driver)
obj-y += ../$(COMMON_SENSOR)/imx258mipiraw_Sensor.o
endif
