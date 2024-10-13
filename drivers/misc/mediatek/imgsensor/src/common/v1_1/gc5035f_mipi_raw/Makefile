# SPDX-License-Identifier: GPL-2.0

COMMON_SENSOR := gc5035
CHECK_DRIVER_DIR := $(COMMON_SENSOR)_mipi_raw

ifneq ($(findstring $(CHECK_DRIVER_DIR), $(CONFIG_CUSTOM_KERNEL_IMGSENSOR)), )
$(info $(COMMON_SENSOR) device driver is already in use)
obj-y += $(COMMON_SENSOR)f_dummy.o
else
$(info used common driver: $(COMMON_SENSOR))
obj-y += ../$(COMMON_SENSOR)_mipi_raw/
endif
