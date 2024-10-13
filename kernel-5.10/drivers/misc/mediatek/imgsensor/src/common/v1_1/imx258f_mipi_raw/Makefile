# SPDX-License-Identifier: GPL-2.0
# Copyright (C) 2022 Samsung Electronics Inc.

COMMON_SENSOR := imx258
CHECK_DRIVER_DIR := $(COMMON_SENSOR)_mipi_raw

ifneq ($(findstring $(CHECK_DRIVER_DIR), $(CONFIG_CUSTOM_KERNEL_IMGSENSOR)), )
$(info $(COMMON_SENSOR) device driver is already in use)
imgsensor_isp6s-objs += \
	$(subdrv-rpath)/$(COMMON_SENSOR)f_dummy.o
else
$(info used common driver: $(COMMON_SENSOR))
subdrv-rpath := ../common/$(COMMON_VERSION)/$(COMMON_SENSOR)_mipi_raw
include $(IMGSENSOR_DRIVER_PATH)/common/$(COMMON_VERSION)/$(COMMON_SENSOR)_mipi_raw/Makefile
endif
