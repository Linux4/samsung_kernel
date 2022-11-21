subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek
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
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_TS)	+=  focaltech_touch/
obj-$(CONFIG_TOUCHSCREEN_MTK_IPIO)	+=  ipio_touch_driver/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET)	+= hxchipset/
obj-$(CONFIG_TOUCHSCREEN_MTK_ILITEK_9882N_N6)	+=  ilitek_9882n_n6/
obj-$(CONFIG_TOUCHSCREEN_CHIPONE) += chipone_tddi/
#bug621774, liuguohua.wt, add, 20200120, tp bring up
obj-$(CONFIG_TOUCHSCREEN_MTK_ILITEK_TS)	+=  ilitek_tddi_touch/
#Bug621774, chensibo.wt, add, 20200121, tp bring up
obj-$(CONFIG_TOUCHSCREEN_MTK_ILITEK_TDDI_TOUCH_V3)	+= ilitek_tddi_touch_v3/
