#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.

TARGET = novatek_ts_nt36523

ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
	$(TARGET)-objs	:= nt36xxx.o nt36xxx_fw_update.o nt36xxx_sec_fn.o
else
	$(TARGET)-objs	:= nt36xxx.o nt36xxx_fw_update.o nt36xxx_sec_fn.o nvt_ts_only_vendor.o
endif

	obj-$(CONFIG_TOUCHSCREEN_NOVATEK_NT36523_TABLET_I2C) += $(TARGET).o

ccflags-y += -Wformat
