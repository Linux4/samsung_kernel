# SPRD CPU DAI Support

SPRD_SOUND_TREE := sound/soc/sprd/

ccflags-y += -I$(SPRD_SOUND_TREE) \
			-I$(SPRD_SOUND_TREE)/platform/

snd-soc-sprd-dai-objs := sprd-dai.o

obj-$(CONFIG_SND_SOC_SPRD_DAI) += snd-soc-sprd-dai.o
