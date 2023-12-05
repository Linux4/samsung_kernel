ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/O22

# Makefile for the Jadard touchscreen drivers.
obj-$(CONFIG_TOUCHSCREEN_JADARD_MODULE)         += jadard_module.o
obj-$(CONFIG_TOUCHSCREEN_JADARD_IC_JD9522T)     += jadard_ic_JD9522T.o
obj-$(CONFIG_TOUCHSCREEN_JADARD_COMMON)         += jadard_common.o jadard_platform.o
obj-$(CONFIG_TOUCHSCREEN_JADARD_DEBUG)          += jadard_debug.o
obj-$(CONFIG_TOUCHSCREEN_JADARD_SORTING)        += jadard_sorting.o
# hs14 code for SR-AL6528A-01-482 by hehaoran5 at 20220910 start
obj-$(CONFIG_TOUCHSCREEN_JADARD_SEC)            += jadard_sec.o
# hs14 code for SR-AL6528A-01-482 by hehaoran5 at 20220910 end