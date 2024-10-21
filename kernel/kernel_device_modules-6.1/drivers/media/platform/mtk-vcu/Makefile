subdir-ccflags-y += -Werror
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/smi
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/cmdq
mtk-vcu-y += mtk_vcu.o \
	mtk_vcodec_mem.o \

obj-$(CONFIG_VIDEO_MEDIATEK_VCU) += mtk-vcu.o
ccflags-y += -I$(DEVICE_MODULES_PATH)/arch/arm
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/rv
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/vcp/include
