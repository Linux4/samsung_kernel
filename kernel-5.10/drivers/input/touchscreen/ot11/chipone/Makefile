#obj-$(CONFIG_TOUCHSCREEN_CHIPONE) += chipone-tddi.o
ifeq ($(CONFIG_DRM_MEDIATEK), y)
ccflags-$(CONFIG_DRM_MEDIATEK) += -I$(srctree)/drivers/gpu/drm/mediatek/
endif
ccflags-$(CONFIG_DRM_MEDIATEK_V2) += -I$(srctree)/drivers/gpu/drm/mediatek/mediatek_v2/

# Tab A9 code for SR-AX6739A-01-345 by wenghailong at 20230424 start
MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat
# Tab A9 code for SR-AX6739A-01-652 by suyurui at 20230616 start
subdir-ccflags-y += -I$(srctree)/drivers/power/supply/
# Tab A9 code for SR-AX6739A-01-652 by suyurui at 20230616 end
ccflags-y += -I$(srctree)/drivers/misc/mediatek/eccci/inc/
# Tab A9 code for SR-AX6739A-01-345 by wenghailong at 20230424 end
# Tab A9 code for SR-AX6739A-01-259 by wenghailong at 20230510 start
ccflags-y += -I$(srctree)/drivers/input/touchscreen/ot11/touch_common/
# Tab A9 code for SR-AX6739A-01-259 by wenghailong at 20230510 end
# Tab A9 code for AX6739A-1679 by wenghailong at 20230704 start
ccflags-y += -I$(srctree)/drivers/misc/mediatek/extcon/
# Tab A9 code for AX6739A-1679 by wenghailong at 20230704 end
obj-m += chipone.o

chipone-objs := cts_driver.o
chipone-objs += cts_core.o
chipone-objs += cts_sfctrlv2.o
chipone-objs += cts_spi_flash.o
chipone-objs += cts_firmware.o
chipone-objs += cts_test.o
chipone-objs += cts_charger_detect.o
chipone-objs += cts_earjack_detect.o
chipone-objs += cts_tcs.o
chipone-objs += cts_platform.o
chipone-objs += cts_tool.o
chipone-objs += cts_sysfs.o
chipone-objs += cts_strerror.o
chipone-objs += cts_oem.o

