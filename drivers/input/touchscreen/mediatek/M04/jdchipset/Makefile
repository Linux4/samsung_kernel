# HS03S code for DEVAL5625-1433/DEVAL5625-1483 by yuanliding at 20210625 start
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/M04/
# HS03S code for DEVAL5625-1433/DEVAL5625-1483 by yuanliding at 20210625 end
# Makefile for the Jadard touchscreen drivers.
obj-$(CONFIG_TOUCHSCREEN_JADARD_MODULE)         += jadard_module.o
obj-$(CONFIG_TOUCHSCREEN_JADARD_IC_JD9365T)     += jadard_ic_JD9365T.o
obj-$(CONFIG_TOUCHSCREEN_JADARD_COMMON)         += jadard_common.o jadard_platform.o
obj-$(CONFIG_TOUCHSCREEN_JADARD_DEBUG)          += jadard_debug.o
obj-$(CONFIG_TOUCHSCREEN_JADARD_SORTING)        += jadard_sorting.o
# HS03SD code added for SR-AL5625-01-305 by yuanliding at 20210521 start
obj-$(CONFIG_TOUCHSCREEN_JADARD_SEC)            += jadard_sec.o
# HS03SD code added for SR-AL5625-01-305 by yuanliding at 20210521 end
