###
# Copyright (C) Fourier Semiconductor Inc. 2016-2021. All rights reserved.
#

# TOPLEVEL=$(PWD)/..
# EXTRA_CFLAGS += -I$(TOPLEVEL)

EXTRA_CFLAGS += -DDEBUG -DFSM_DEBUG
EXTRA_CFLAGS += -Wall -Werror
MODFLAGS = -fno-pic
CFLAGS_MODULE = $(MODFLAGS)
AFLAGS_MODULE = $(MODFLAGS)

PUBLISH_INC = := -I$(fs17xx)/

snd-soc-fs17xx-objs := fsm_core.o
snd-soc-fs17xx-objs += fs17xx.o
snd-soc-fs17xx-objs += fsm_i2c.o
obj-y += snd-soc-fs17xx.o
#obj-$(CONFIG_SND_SOC_FS17XX) += snd-soc-fs17xx.o
