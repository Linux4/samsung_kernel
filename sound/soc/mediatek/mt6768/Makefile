# SPDX-License-Identifier: GPL-2.0

# platform driver
snd-soc-mt6768-afe-objs := \
	mt6768-afe-pcm.o \
	mt6768-afe-clk.o \
	mt6768-afe-gpio.o \
	mt6768-dai-adda.o \
	mt6768-afe-control.o \
	mt6768-dai-i2s.o \
	mt6768-dai-hw-gain.o \
	mt6768-dai-pcm.o \
	mt6768-dai-hostless.o \
	mt6768-misc-control.o

obj-$(CONFIG_SND_SOC_MT6768) += snd-soc-mt6768-afe.o

# machine driver
obj-$(CONFIG_SND_SOC_MT6768_MT6358) += mt6768-mt6358.o

# MENU Governor Predict(low power)
ifeq ($(CONFIG_MTK_ACAO_SUPPORT),y)
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/mcdi/include/
endif

# a06 code for AL7160A-18 by hujincan at 20240326 start
ifeq ($(CONFIG_HQ_PROJECT_O8),y)
ccflags-y += -DFSM_FS1512GN_SUPPORT
endif
# a06 code for AL7160A-18 by hujincan at 20240326 end

# spm
ccflags-y += -I$(srctree)/drivers/misc/mediatek/base/power/include

