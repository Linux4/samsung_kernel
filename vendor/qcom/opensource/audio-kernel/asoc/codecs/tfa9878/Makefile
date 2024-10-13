ifneq ($(CONFIG_SND_SOC_TFA9878),)
# TFA audio amplifier - TFA9878

ifeq ($(CONFIG_SND_SOC_TFA9878),m)
TFA_KERNEL_MODULE        =1
endif

TFA_VERSION              =tfa9878
TFA_USE_TFA_CLASS        =1
TFA_USE_TFACAL_NODE      =1
TFA_USE_TFASTC_NODE      =1
ifeq ($(CONFIG_TFA_STEREO_NODE),y)
TFA_STEREO_NODE          =1
endif

TFA98XX_FLAGS            := -DUSE_TFA9878
ifeq ($(CONFIG_PLATFORM_QUALCOMM),y)
TFA98XX_FLAGS            += -DQPLATFORM
endif

# cc flags
#ccflags-y              := -DDEBUG
ccflags-y               += -Werror
ifdef TFA_STEREO_NODE
ccflags-y               += -DTFA_STEREO_NODE
endif

# objs
snd-soc-tfa98xx-objs    := tfa98xx.o
snd-soc-tfa98xx-objs    += tfa_container.o
snd-soc-tfa98xx-objs    += tfa_dsp.o
snd-soc-tfa98xx-objs    += tfa_init.o
ifdef TFA_DEBUG
snd-soc-tfa98xx-objs    += tfa_debug.o
endif
ifdef TFA_USE_TFA_CLASS
ifdef TFA_KERNEL_MODULE
snd-soc-tfa_sysfs-objs  += tfa_sysfs.o
else
snd-soc-tfa98xx-objs    += tfa_sysfs.o
endif
ifdef TFA_USE_TFACAL_NODE
ifdef TFA_KERNEL_MODULE
snd-soc-tfa_sysfs-objs  += tfa_cal.o
else
snd-soc-tfa98xx-objs    += tfa_cal.o
endif
endif
ifdef TFA_USE_TFASTC_NODE
ifdef TFA_KERNEL_MODULE
snd-soc-tfa_sysfs-objs  += tfa_stc.o
else
snd-soc-tfa98xx-objs    += tfa_stc.o
endif
endif
endif

# CFLAGS
CFLAGS_tfa98xx.o       += $(TFA98XX_FLAGS)
CFLAGS_tfa_container.o += $(TFA98XX_FLAGS)
CFLAGS_tfa_dsp.o       += $(TFA98XX_FLAGS)
CFLAGS_tfa_init.o      += $(TFA98XX_FLAGS)
ifdef TFA_DEBUG
CFLAGS_tfa_debug.o     += $(TFA98XX_FLAGS)
endif
ifdef TFA_USE_TFA_CLASS
CFLAGS_tfa_sysfs.o     += $(TFA98XX_FLAGS)
ifdef TFA_USE_TFACAL_NODE
CFLAGS_tfa_cal.o       += $(TFA98XX_FLAGS)
endif
ifdef TFA_USE_TFASTC_NODE
CFLAGS_tfa_stc.o       += $(TFA98XX_FLAGS)
endif
endif

ifdef TFA_KERNEL_MODULE
# module driver
obj-m   := snd-soc-tfa98xx.o
ifdef TFA_USE_TFA_CLASS
obj-m   += snd-soc-tfa_sysfs.o
endif
else
# built-in driver
obj-y	+= snd-soc-tfa98xx.o
endif

endif
