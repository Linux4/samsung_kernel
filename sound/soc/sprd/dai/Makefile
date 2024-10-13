# SPRD DMA Support
SPRD_SOUND_TREE := sound/soc/sprd/
KBUILD_CFLAGS	+= -I$(SPRD_SOUND_TREE) -I$(SPRD_SOUND_TREE)/dai/vbc/ -I$(SPRD_SOUND_TREE)/dai/vaudio/

ifneq ($(CONFIG_SND_SOC_SPRD_AUDIO_DMA_ENGINE),)
    ifneq ($(CONFIG_SND_SOC_SPRD_MACHINE_TI),)                                                                                                                                                                      
        snd-soc-sprd-audio-dma-objs       := sprd4whale-dmaengine-pcm.o
    else
        ifneq ($(CONFIG_SND_SOC_SPRD_MACHINE_4AUDIENCE),)
            snd-soc-sprd-audio-dma-objs       := sprd4whale-dmaengine-pcm.o
        else
        	ifneq ($(CONFIG_SND_SOC_SPRD_MACHINE_4REALTEK),)
            	snd-soc-sprd-audio-dma-objs       := sprd4whale-dmaengine-pcm.o
			else
	            snd-soc-sprd-audio-dma-objs       := sprd-dmaengine-pcm.o
			endif
        endif
    endif
else
    snd-soc-sprd-audio-dma-objs       := sprd-pcm.o
endif


ifneq ($(CONFIG_SND_SOC_SPRD_AUDIO_OFFLOAD),)
    ifneq ($(CONFIG_SND_SOC_SPRD_AUDIO_OFFLOAD_2STAGE_DMA),)
        snd-soc-sprd-audio-dma-objs       += sprd-compr_2stage_dma.o
    else
        snd-soc-sprd-audio-dma-objs       += sprd-compr.o
    endif
endif

obj-$(CONFIG_SND_SOC_SPRD_AUDIO_DMA)       += snd-soc-sprd-audio-dma.o
