# M55 code for SR-QN6887A-01-401 by yuli at 202300816 start
ccflags-y += -I$(srctree)/drivers/input/touchscreen/x55/touch_common/
# M55 code for SR-QN6887A-01-401 by yuli at 202300816 end
obj-$(CONFIG_TOUCHSCREEN_GOODIX_BRL_I2C) += goodix.o
goodix-y := \
                goodix_brl_i2c.o \
                goodix_brl_spi.o \
                goodix_ts_core.o \
                goodix_brl_hw.o \
                goodix_cfg_bin.o \
                goodix_ts_utils.o \
                goodix_brl_fwupdate.o \
                goodix_ts_gesture.o \
                goodix_ts_inspect.o \
                goodix_ts_tools.o
