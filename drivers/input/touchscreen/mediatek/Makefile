subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/ILI9882Q/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lcm/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
# In case the platform does NOT support this type of sensors
obj-y   +=  mtk_tpd.o
obj-y   +=  tpd_button.o
obj-y   +=  tpd_calibrate.o
obj-y   +=  tpd_debug.o
obj-y   +=  tpd_default.o
obj-y   +=  tpd_misc.o
obj-y   +=  tpd_setting.o
#obj-y   +=  tpd_notify.o
obj-$(CONFIG_TOUCHSCREEN_FOCALTECH)	+=  focaltech_touch/
obj-$(CONFIG_TOUCHSCREEN_CHIPONE) += chipone_tddi/
obj-$(CONFIG_TOUCHSCREEN_ILI9882Q)	+= ILI9882Q/
