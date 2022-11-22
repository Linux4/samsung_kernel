BUILD_INFAE := spi
BUILD_PLATFORM := mtk
BUILD_MODULE := n

ccflags-y += -Wall

ifeq ($(BUILD_PLATFORM),mtk)
platform=ili9881x_mtk
ccflags-y += -I$(srctree)/drivers/spi/mediatek/mt6797/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/new_tddi_9881x/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/new_tddi_9881x/firmware/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include/
endif

ifeq ($(BUILD_PLATFORM),qcom)
platform=ili9881x_qcom
ccflags-y += -I$(srctree)/drivers/input/touchscreen/oreo_9881n/
ccflags-y += -I$(srctree)/drivers/input/touchscreen/oreo_9881n/firmware/
endif

ifeq ($(BUILD_INFAE),i2c)
interface=ili9881x_i2c
fwupdate=ili9881x_flash
endif

ifeq ($(BUILD_INFAE),spi)
interface=ili9881x_spi
fwupdate=ili9881x_hostdl
endif

ifeq ($(BUILD_MODULE),n)
obj-y += ili9881x.o \
	$(interface).o \
	$(platform).o \
	ili9881x_ic.o \
	ili9881x_touch.o \
	ili9881x_mp.o \
	$(fwupdate).o \
	ili9881x_node.o

obj-y += mtk_tpd.o \
	tpd_button.o \
	tpd_calibrate.o \
	tpd_debug.o \
	tpd_default.o \
	tpd_misc.o \
	tpd_setting.o
else
	obj-m += ilitek.o
	ilitek-y := ili9881x.o \
		$(interface).o \
		$(platform).o \
		ili9881x_ic.o \
		ili9881x_touch.o \
		ili9881x_mp.o \
		$(fwupdate).o \
		ili9881x_node.o

KERNEL_DIR= /home/likewise-open/ILI/1061279/workplace/rk3288_sdk/kernel
all:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
endif
