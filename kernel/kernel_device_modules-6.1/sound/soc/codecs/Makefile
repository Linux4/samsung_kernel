# SPDX-License-Identifier: GPL-2.0

ifneq ($(CONFIG_MTK_TINYSYS_SCP_SUPPORT),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/misc/mediatek/scp/include
endif

ifneq ($(CONFIG_DEVICE_MODULES_SND_SOC_MEDIATEK),)
ccflags-y += -I$(DEVICE_MODULES_PATH)/sound/soc/mediatek/common
endif
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/include/linux/firmware/cirrus
subdir-ccflags-y += -I$(DEVICE_MODULES_PATH)/include/sound/cirrus/

snd-soc-cirrus-amp-objs := cirrus-amp.o cirrus-bd.o cirrus-cal.o cirrus-pwr.o cirrus-cal-cspl.o bigdata_cirrus_sysfs_cb.o
snd-soc-cs35l45-spi-objs := cs35l45-spi.o cs35l45.o cs35l45-tables.o
snd-soc-cs35l45-i2c-objs := cs35l45-i2c.o cs35l45.o cs35l45-tables.o
snd-soc-wm-adsp-objs := wm_adsp.o
snd-soc-mt6338-objs := mt6338.o
snd-soc-mt6338-accdet-objs := mt6338-accdet.o
snd-soc-mt6351-objs := mt6351.o
snd-soc-mt6358-objs := mt6358.o
snd-soc-mt6359-objs := mt6359.o
snd-soc-mt6359-accdet-objs := mt6359-accdet.o
snd-soc-mt6359p-accdet-objs := mt6359p-accdet.o
snd-soc-mt6368-objs := mt6368.o
snd-soc-mt6369-objs := mt6369.o
snd-soc-mt6681-objs := mt6681.o
snd-soc-mt6681-accdet-objs := mt6681-accdet.o
snd-soc-hdmi-codec-objs := hdmi-codec.o

obj-$(CONFIG_DEVICE_MODULES_SND_SOC_CIRRUS_AMP)	+= snd-soc-cirrus-amp.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_CS35L45_SPI)	+= snd-soc-cs35l45-spi.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_CS35L45_I2C)	+= snd-soc-cs35l45-i2c.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_WM_ADSP)	+= snd-soc-wm-adsp.o
obj-$(CONFIG_SND_SOC_MT6338)	+= snd-soc-mt6338.o
obj-$(CONFIG_SND_SOC_MT6338_ACCDET) += mt6338-accdet.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6351)	+= snd-soc-mt6351.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6358)	+= snd-soc-mt6358.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6359)	+= snd-soc-mt6359.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6359_ACCDET) += mt6359-accdet.o
obj-$(CONFIG_SND_SOC_MT6660)	+= snd-soc-mt6660.o
obj-$(CONFIG_SND_SOC_MT6359P)	+= snd-soc-mt6359.o
obj-$(CONFIG_SND_SOC_MT6359P_ACCDET) += mt6359p-accdet.o
obj-$(CONFIG_SND_SOC_MT6368)	+= snd-soc-mt6368.o
obj-$(CONFIG_SND_SOC_MT6368_ACCDET) += mt6368-accdet.o
obj-$(CONFIG_SND_SOC_MT6369_ACCDET) += mt6369-accdet.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_MT6369)	+= snd-soc-mt6369.o
obj-$(CONFIG_SND_SOC_MT6681)	+= snd-soc-mt6681.o
obj-$(CONFIG_SND_SOC_MT6681_ACCDET) += mt6681-accdet.o
obj-$(CONFIG_DEVICE_MODULES_SND_SOC_HDMI_CODEC)	+= snd-soc-hdmi-codec.o
obj-$(CONFIG_SND_SOC_RT5512)	+= richtek/
obj-$(CONFIG_SND_SOC_TFA9874)	+= tfa98xx/
