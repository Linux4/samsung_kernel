# SPRD VBC Support
SPRD_SOUND_TREE := sound/soc/sprd/
KBUILD_CFLAGS	+= -I$(SPRD_SOUND_TREE) -I$(SPRD_SOUND_TREE)/dai -I$(SPRD_SOUND_TREE)/dai/vbc/ -I$(SPRD_SOUND_TREE)/dai/vbc/r3p0/

snd-soc-sprd-vbc-r3p0-objs       := vbc-dai.o vbc-utils.o vbc-phy-r3p0.o vbc-codec.o
obj-$(CONFIG_SND_SOC_SPRD_VBC_R3P0)    += snd-soc-sprd-vbc-r3p0.o
