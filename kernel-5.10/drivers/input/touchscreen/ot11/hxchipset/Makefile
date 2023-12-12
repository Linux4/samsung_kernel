# Makefile for the Himax touchscreen drivers.

ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

# Tab A9 code for SR-AX6739A-01-345 by wenghailong at 20230424 start
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc/
# Tab A9 code for SR-AX6739A-01-345 by wenghailong at 20230424 end
# Tab A9 code for SR-AX6739A-01-259 by wenghailong at 20230510 start
ccflags-y += -I$(srctree)/drivers/input/touchscreen/ot11/touch_common/
# Tab A9 code for SR-AX6739A-01-259 by wenghailong at 20230510 end
#Tab A9 code for SR-AX6739A-01-263 by zhawei at 20230522 start
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/
#Tab A9 code for SR-AX6739A-01-263 by zhawei at 20230522 end
# Tab A9 code for AX6739A-1679 by wenghailong at 20230704 start
ccflags-y += -I$(srctree)/drivers/misc/mediatek/extcon/
# Tab A9 code for AX6739A-1679 by wenghailong at 20230704 end
obj-m += hxchipset.o
hxchipset-y += himax_common.o
hxchipset-y += himax_debug.o
hxchipset-y += himax_firmware.o
hxchipset-y += himax_ic_HX83102.o
hxchipset-y += himax_ic_incell_core.o
hxchipset-y += himax_inspection.o
hxchipset-y += himax_platform.o