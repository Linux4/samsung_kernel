# SPDX-License-Identifier: GPL-2.0

# MENU Governor Predict(low power) include path
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# adsp include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/audio_ipi/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/audio_dsp

# spm include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include

# pbm include path
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm

# platform driver
obj-$(CONFIG_SND_SOC_MT6989) += snd-soc-mt6989-afe.o
snd-soc-mt6989-afe-objs += \
	mt6989-afe-pcm.o \
	mt6989-afe-clk.o \
	mt6989-afe-gpio.o \
	mt6989-dai-adda.o \
	mt6989-afe-control.o \
	mt6989-dai-i2s.o \
	mt6989-dai-hw-gain.o \
	mt6989-dai-src.o \
	mt6989-dai-pcm.o \
	mt6989-dai-hostless.o \
	mt6989-misc-control.o \
	mt6989-dai-tdm.o \
	mt6989-afe-cm.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6989_MT6681) += mt6989-mt6681.o
