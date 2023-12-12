#
# Makefile for the Novatek NT36xxx touchscreen driver.
#

# Each configuration option enables a list of files.
#ifeq ($(CONFIG_SAMSUNG_PRODUCT_SHIP), y)
#    obj-$(CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI) += nt36xxx.o nt36xxx_fw_update.o nt36xxx_sec_fn.o
#else
#    obj-$(CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI) += nt36xxx.o nt36xxx_fw_update.o nt36xxx_sec_fn.o nvt_ts_only_vendor.o
#endif

ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/ot11/touch_common/

# Tab A9 code for AX6739A-1679 by wenghailong at 20230704 start
ccflags-y += -I$(srctree)/drivers/misc/mediatek/extcon/
# Tab A9 code for AX6739A-1679 by wenghailong at 20230704 end
#Tab A9 code for SR-AX6739A-01-712 by yuli at 20230612 start
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/
#Tab A9 code for SR-AX6739A-01-712 by yuli at 20230612 end

#obj-$(CONFIG_TOUCHSCREEN_NT36xxx_HOSTDL_SPI) += nt36xxx.o nt36xxx_fw_update.o nt36xxx_ext_proc.o nt36xxx_mp_ctrlram.o nt36xxx_sec_fn.o
obj-m += novatek.o
novatek-y += nt36xxx.o nt36xxx_fw_update.o
novatek-y += nt36xxx_ext_proc.o
novatek-y += nt36xxx_mp_ctrlram.o
novatek-y += nt36xxx_sec_fn.o
