# SPDX-License-Identifier: GPL-2.0

# platform driver
snd-soc-mt6833-afe-objs := \
	mt6833-afe-pcm.o \
	mt6833-afe-clk.o \
	mt6833-afe-gpio.o \
	mt6833-dai-adda.o \
	mt6833-afe-control.o \
	mt6833-dai-i2s.o \
	mt6833-dai-hw-gain.o \
	mt6833-dai-src.o \
	mt6833-dai-pcm.o \
	mt6833-dai-hostless.o \
	mt6833-misc-control.o

obj-$(CONFIG_SND_SOC_MT6833) += snd-soc-mt6833-afe.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6833_MT6359) += mt6833-mt6359.o

# MENU Governor Predict(low power)
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# spm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include
ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(AUDIO_PLATFORM)/include

#zhangsen1 external spk amp
ifeq ($(CONFIG_SND_SOC_FS1815N),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/codecs/fs1815/
endif
#zhangsen1 external spk amp
ifeq ($(CONFIG_SND_SOC_SIA8159),y)
subdir-ccflags-y += -I$(srctree)/sound/soc/codecs/sia8159/
endif
