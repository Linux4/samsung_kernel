
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
#Tab A9 code for SR-AX6739A-01-692 by hehaoran5 at 20230614 start
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/
#Tab A9 code for SR-AX6739A-01-692 by hehaoran5 at 20230614 end
# Tab A9 code for AX6739A-1679 by wenghailong at 20230704 start
ccflags-y += -I$(srctree)/drivers/misc/mediatek/extcon/
# Tab A9 code for AX6739A-1679 by wenghailong at 20230704 end
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/ot11/touch_common/

ccflags-y += -DCONFIG_TOUCHSCREEN_JADARD_DEBUG=y
ccflags-y += -DCONFIG_TOUCHSCREEN_JADARD_SORTING=y
ccflags-y += -D__JADARD_KMODULE__

obj-m += jdchipset.o
jdchipset-y += jadard_module.o
jdchipset-y += jadard_ic_JD9366T.o
jdchipset-y += jadard_common.o
jdchipset-y += jadard_platform.o
jdchipset-y += jadard_debug.o
jdchipset-y += jadard_sorting.o