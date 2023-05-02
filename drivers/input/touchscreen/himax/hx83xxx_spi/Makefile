# Makefile for the Himax touchscreen drivers.

TARGET = himax_ts_83xxx_spi

$(TARGET)-objs := himax_common.o himax_debug.o himax_inspection.o himax_platform_SPI.o himax_ic_incell_core.o

ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83102)),)
  $(TARGET)-objs += himax_ic_HX83102.o
else ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83108)),)
  $(TARGET)-objs += himax_ic_HX83108.o
else ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83191)),)
  $(TARGET)-objs += himax_ic_HX83191.o
else ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112)),)
  $(TARGET)-objs += himax_ic_HX83112.o
else ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83111)),)
  $(TARGET)-objs += himax_ic_HX83111.o
else ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83103)),)
  $(TARGET)-objs += himax_ic_HX83103.o
else ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83121)),)
  $(TARGET)-objs += himax_ic_HX83121.o
else ifneq ($(filter y m, $(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83122)),)
  $(TARGET)-objs += himax_ic_HX83122.o
endif

obj-$(CONFIG_TOUCHSCREEN_HIMAX_HX83XXX_SPI) += $(TARGET).o

ccflags-y += -Wformat

ccflags-$(CONFIG_SAMSUNG_TUI)	+= -Idrivers/misc/tui

