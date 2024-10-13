# Makefile for the focaltech touchscreen drivers.
ccflags-y += -I$(srctree)/drivers/input/touchscreen/x55/touch_common/

obj-$(CONFIG_TOUCHSCREEN_FTS_I2C) += focaltech_ts.o

focaltech_ts-y    := focaltech_core.o focaltech_ex_fun.o focaltech_ex_mode.o \
    focaltech_gesture.o focaltech_esdcheck.o focaltech_point_report_check.o\
    focaltech_i2c.o focaltech_flash.o \
    focaltech_flash/focaltech_upgrade_ft3518.o
focaltech_ts-y += focaltech_flash/focaltech_upgrade_ft5672.o
focaltech_ts-y += focaltech_test/focaltech_test.o
focaltech_ts-y += focaltech_test/focaltech_test_ini.o
focaltech_ts-y += focaltech_test/supported_ic/focaltech_test_ft3683g.o
