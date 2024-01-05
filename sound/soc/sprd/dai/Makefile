# SPRD DMA Support
SPRD_SOUND_TREE := sound/soc/sprd/
KBUILD_CFLAGS	+= -I$(SPRD_SOUND_TREE) -I$(SPRD_SOUND_TREE)/dai/vbc/ -I$(SPRD_SOUND_TREE)/dai/vaudio/

snd-soc-sprd-audio-dma-objs       := sprd-pcm.o
obj-$(CONFIG_SND_SOC_SPRD_AUDIO_DMA)       += snd-soc-sprd-audio-dma.o
