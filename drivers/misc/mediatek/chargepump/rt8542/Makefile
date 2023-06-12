#include $(srctree)/drivers/misc/mediatek/Makefile.custom

MTK_PLATFORM := $(subst ",,$(CONFIG_MTK_PLATFORM))
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/include/mt-plat/
subdir-ccflags-y += -Werror -I$(srctree)/drivers/misc/mediatek/chargepump/inc/

obj-y	:=  rt8542_driver.o

