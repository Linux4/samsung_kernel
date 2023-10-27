# include header files for MTK
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/ilitek_tddi_touch/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/

BUILD_INFAE := spi
BUILD_PLATFORM := mtk
BUILD_MODULE := n

ifeq ($(BUILD_PLATFORM),mtk)
platform=ilitek_plat_mtk
endif

ifeq ($(BUILD_PLATFORM),qcom)
platform=ilitek_plat_qcom
endif

ifeq ($(BUILD_INFAE),i2c)
interface=ilitek_i2c
fwupdate=ilitek_flash
endif

ifeq ($(BUILD_INFAE),spi)
interface=ilitek_spi
fwupdate=ilitek_hostdl
endif

ifeq ($(BUILD_MODULE),n)
obj-y += ilitek_main.o \
	$(interface).o \
	$(platform).o \
	ilitek_ic.o \
	ilitek_touch.o \
	ilitek_mp.o \
	$(fwupdate).o \
	ilitek_node.o
else
	obj-m += ilitek.o
	ilitek-y := ilitek_main.o \
		$(interface).o \
		$(platform).o \
		ilitek_ic.o \
		ilitek_touch.o \
		ilitek_mp.o \
		$(fwupdate).o \
		ilitek_node.o
endif
