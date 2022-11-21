# Makefile for the focaltech touchscreen drivers.
TARGET = focaltech_ts_ft8720

$(TARGET)-objs	:= focaltech_core.o focaltech_ex_mode.o focaltech_gesture.o \
		focaltech_esdcheck.o focaltech_point_report_check.o \
		focaltech_flash.o focaltech_sec_cmd.o focaltech_spi.o

ifneq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
$(TARGET)-objs	+= focaltech_ex_fun.o
endif

obj-$(CONFIG_TOUCHSCREEN_FTS_FT8720) += $(TARGET).o

ccflags-y += -Wformat

