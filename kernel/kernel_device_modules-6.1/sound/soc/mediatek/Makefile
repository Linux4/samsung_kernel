# SPDX-License-Identifier: GPL-2.0
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MEDIATEK) += common/
ifneq ($(wildcard $(srctree)/$(src)/mt6853/Makefile),)
obj-$(CONFIG_SND_SOC_MT6853) += mt6853/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6873/Makefile),)
obj-$(CONFIG_SND_SOC_MT6873) += mt6873/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6878/Makefile),)
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6878) += mt6878/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt8188/Makefile),)
obj-$(CONFIG_SND_SOC_MT8188) += mt8188/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6983/Makefile),)
obj-$(CONFIG_SND_SOC_MT6983) += mt6983/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6985/Makefile),)
obj-$(CONFIG_SND_SOC_MT6985) += mt6985/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6886/Makefile),)
obj-$(CONFIG_SND_SOC_MT6886) += mt6886/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6897/Makefile),)
obj-$(CONFIG_SND_SOC_MT6897) += mt6897/
endif
ifneq ($(wildcard $(srctree)/$(src)/mt6989/Makefile),)
obj-$(CONFIG_SND_SOC_MT6989) += mt6989/
endif
# vow related code
obj-$(CONFIG_MTK_VOW_SUPPORT) += vow/

# audio sound dsp related code
obj-$(CONFIG_SND_SOC_MTK_AUDIO_DSP) += audio_dsp/

#scp ultra sound related code
obj-$(CONFIG_MTK_ULTRASND_PROXIMITY) += ultrasound/

obj-$(CONFIG_SND_SOC_MTK_AUDIO_DSP) += audio_scp/

# include path
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include/mt-plat
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/include
subdir-ccflags-y += -Werror -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/pbm
