# Makefile for the focaltech touchscreen drivers.
#+S96818AA1-1936,daijun1.wt,modify,2023/05/24,nt36528 Modify res control method
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
#-S96818AA1-1936,daijun1.wt,modify,2023/05/24,nt36528 Modify res control method
obj-$(CONFIG_TOUCHSCREEN_FTS_FT8057S)	+=  focaltech_core.o
obj-$(CONFIG_TOUCHSCREEN_FTS_FT8057S)	+=  focaltech_ex_fun.o
obj-$(CONFIG_TOUCHSCREEN_FTS_FT8057S)	+=  focaltech_ex_mode.o
obj-$(CONFIG_TOUCHSCREEN_FTS_FT8057S)	+=  focaltech_gesture.o
obj-$(CONFIG_TOUCHSCREEN_FTS_FT8057S)	+=  focaltech_esdcheck.o
obj-$(CONFIG_TOUCHSCREEN_FTS_FT8057S)   +=  focaltech_point_report_check.o
obj-$(CONFIG_TOUCHSCREEN_FTS_FT8057S)   +=  focaltech_test/



obj-$(CONFIG_TOUCHSCREEN_FTS_FT8057S)	+=  focaltech_flash.o



obj-$(CONFIG_TOUCHSCREEN_FTS_FT8057S)   +=  focaltech_spi.o
