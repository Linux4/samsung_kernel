# include header files for MTK
ccflags-y += -I$(srctree)/drivers/spi/mediatek/mt6797/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/ipio_touch_driver/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/mediatek/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/

ccflags-y += -Wall

BUILD_INFAE := spi
BUILD_PLATFORM := qcom
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
	ilitek_node.o \
	ilitek_sec.o
else
	obj-m += ilitek.o
	ilitek-y := ilitek_main.o \
		$(interface).o \
		$(platform).o \
		ilitek_ic.o \
		ilitek_touch.o \
		ilitek_mp.o \
		$(fwupdate).o \
		ilitek_node.o \
		ilitek_sec.o

KERNEL_DIR= /home/likewise-open/ILI/1061279/workplace/rk3288_sdk/kernel
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
endif
