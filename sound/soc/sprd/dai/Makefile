# SPRD DMA Support
SPRD_SOUND_TREE := sound/soc/sprd/
KBUILD_CFLAGS	+= -I$(SPRD_SOUND_TREE) -I$(SPRD_SOUND_TREE)/dai/vbc/ -I$(SPRD_SOUND_TREE)/dai/vaudio/

ifneq ($(CONFIG_SND_SOC_SPRD_AUDIO_DMA_ENGINE),)
	snd-soc-sprd-audio-dma-objs       := sprd-dmaengine-pcm.o
else
	snd-soc-sprd-audio-dma-objs       := sprd-pcm.o
endif

obj-$(CONFIG_SND_SOC_SPRD_AUDIO_DMA)       += snd-soc-sprd-audio-dma.o
