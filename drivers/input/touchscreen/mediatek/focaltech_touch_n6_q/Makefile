# Makefile for the focaltech touchscreen drivers.

ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/$(CONFIG_TOUCHSCREEN_MTK_FTS_DIRECTORY)
ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
ccflags-y += -I$(srctree)/drivers/base
ccflags-y += -I$(srctree)/drivers/misc/mediatek/hwmon/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/alsps/inc/

obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS_N6_Q)	+=  focaltech_core.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS_N6_Q)	+=  focaltech_ex_fun.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS_N6_Q)	+=  focaltech_ex_mode.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS_N6_Q)	+=  focaltech_gesture.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS_N6_Q)	+=  focaltech_esdcheck.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS_N6_Q)   +=  focaltech_point_report_check.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS_N6_Q)   +=  focaltech_proximity.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS_N6_Q)   +=  focaltech_test/

obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS_N6_Q)   +=  focaltech_spi.o
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS_N6_Q)	+=  focaltech_flash.o

