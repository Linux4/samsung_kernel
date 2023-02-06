# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022 Samsung Electronics Co., Ltd

COMMON_SENSOR := s5k4hayx
CHECK_DRIVER_DIR := $(COMMON_SENSOR)_mipi_raw

ifneq ($(findstring $(CHECK_DRIVER_DIR), $(CONFIG_CUSTOM_KERNEL_IMGSENSOR)), )
$(info $(COMMON_SENSOR) device driver is already in use)
obj-y += $(COMMON_SENSOR)f_dummy.o
else
$(info used common driver:  $(COMMON_SENSOR))
obj-y += ../$(COMMON_SENSOR)_mipi_raw/
endif
