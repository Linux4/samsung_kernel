subdir-ccflags-y += -Werror
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek
#+bug773028, fangzhihua.wt,mod, 20220628,TP bringup
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/ILI7807S

ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
ccflags-y += -I$(srctree)/drivers/gpu/drm/panel/
# In case the platform does NOT support this type of sensors
obj-y   +=  mtk_tpd.o
obj-y   +=  tpd_button.o
obj-y   +=  tpd_calibrate.o
obj-y   +=  tpd_debug.o
obj-y   +=  tpd_default.o
obj-y   +=  tpd_misc.o
obj-y   +=  tpd_setting.o
obj-$(CONFIG_TOUCHSCREEN_MTK_SOLOMON)	+=  SOLOMON/
obj-$(CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI) += NT36672C/
obj-$(CONFIG_TOUCHSCREEN_ILI7807S)	+=  ILI7807S/
obj-$(CONFIG_TOUCHSCREEN_FTS)	+=  focaltech_touch/
obj-$(CONFIG_TOUCHSCREEN_FTS_FT8722)	+=  FT8722/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT1151)	+=  GT1151/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT911)	+=  GT911/
obj-$(CONFIG_TOUCHSCREEN_MTK_GSLX680)	+=  gslX680/
obj-$(CONFIG_TOUCHSCREEN_MTK_NT36xxx)	+=  NT36xxx/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT9XX_LM)	+=  GT9XX_LM/
obj-$(CONFIG_TOUCHSCREEN_FT5X26)	+=  FT5X26/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT5688)	+=  GT5688/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_I2C)	+=  hxchipset/
obj-$(CONFIG_TOUCHSCREEN_MTK_FOCALTECH_FHD)	+=  focaltech_fhd_touch/
obj-$(CONFIG_TOUCHSCREEN_MTK_GT9157)    +=  GT9157/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET_8789P1_8185P3)    +=  hxchipset_8789p1_8185p3/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_CHIPSET) += hx83112f/
#-bug773028, fangzhihua.wt,mod, 20220628,TP bringup