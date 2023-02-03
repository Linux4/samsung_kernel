#
# Makefile for LXS LX891RS touchscreen driver.
#

TARGET = lxs_ts

$(TARGET)-objs := lxs_ts_dev.o lxs_ts_fn.o lxs_ts_fw.o lxs_ts_cmd.o lxs_ts_prd.o
obj-$(CONFIG_TOUCHSCREEN_LXS_LX891RS) += $(TARGET).o

ccflags-y += -Wformat

ifneq ($(CONFIG_TOUCHSCREEN_LXS_LX891RS), n)
ccflags-y += -I$(srctree)/drivers/battery/common
endif

