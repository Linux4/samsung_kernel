subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/ILI9882Q/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek
ccflags-y += -I$(srctree)/drivers/misc/mediatek/lcm/inc
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/hxchipset_hx83108x/
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
obj-$(CONFIG_TOUCHSCREEN_MTK_IPIO)	+=  ipio_touch_driver/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET)	+= hxchipset/
obj-$(CONFIG_TOUCHSCREEN_MTK_ILITEK_9882N_N6)	+=  ilitek_9882n_n6/
obj-$(CONFIG_TOUCHSCREEN_CHIPONE) += chipone-tddi/
obj-$(CONFIG_TOUCHSCREEN_ILI9882Q)	+= ILI9882Q/
#bug717431, chensibo.wt, add, 20220118, tp bring up
obj-$(CONFIG_TOUCHSCREEN_MTK_ILITEK_TS)	+=  ilitek_tddi_touch/
#Bug717431, chensibo.wt, add, 20220118, tp bring up
obj-$(CONFIG_TOUCHSCREEN_MTK_ILITEK_TDDI_TOUCH_V3)	+= ilitek_tddi_touch_v3/
obj-$(CONFIG_TOUCHSCREEN_GC7202H)	+=  GC7202H/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_HX83108X)	+=  hxchipset_hx83108x/
