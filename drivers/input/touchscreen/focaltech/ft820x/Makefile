# Makefile for the focaltech touchscreen drivers.

obj-$(CONFIG_TOUCHSCREEN_FTS_FT820X)	+=  focaltech_core.o
ifneq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
obj-$(CONFIG_TOUCHSCREEN_FTS_FT820X)	+=  focaltech_ex_fun.o
endif
obj-$(CONFIG_TOUCHSCREEN_FTS_FT820X)	+=  focaltech_ex_mode.o
obj-$(CONFIG_TOUCHSCREEN_FTS_FT820X)	+=  focaltech_gesture.o
obj-$(CONFIG_TOUCHSCREEN_FTS_FT820X)	+=  focaltech_esdcheck.o
obj-$(CONFIG_TOUCHSCREEN_FTS_FT820X)   +=  focaltech_point_report_check.o
obj-$(CONFIG_TOUCHSCREEN_FTS_FT820X)	+=  focaltech_flash.o
obj-$(CONFIG_TOUCHSCREEN_FTS_FT820X)	+=  focaltech_sec_cmd.o
obj-$(CONFIG_TOUCHSCREEN_FTS_FT820X)   +=  focaltech_spi.o

ccflags-y += -Wformat

