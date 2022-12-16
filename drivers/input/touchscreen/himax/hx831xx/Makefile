# Makefile for the Himax touchscreen drivers.

obj-$(CONFIG_TOUCHSCREEN_HIMAX_DEBUG) += himax_debug.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_INSPECT) += himax_inspection.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_INCELL) += himax_ic_incell_core.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_ONCELL_CORE) += himax_ic_oncell_core.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102) += himax_ic_HX83102.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_COMMON) += himax_common.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_I2C) += himax_platform.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_SPI) += himax_platform_SPI.o

ccflags-y += -Wformat