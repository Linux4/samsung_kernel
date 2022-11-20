#
# Makefile for hwmap module
#
# CopyRight (c) 2008 Marvell (anton.eidelman@marvell.com)
#ifndef MODULE_ROOT
#MODULE_ROOT = $(shell pwd)
#endif

TARGET = hwmap

$(TARGET)-objs :=

#TBD use of install - why parent (Drivers) should do this?
#all: modules

#ifneq ($(KERNELRELEASE),)
 
obj-m   := $(TARGET).o
#CC =$(CROSS_COMPILE)gcc

#else

#modules: 
#	$(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(shell pwd) modules
#endif

#dev:

#clean:
#	@echo 'Going to cleanup.\n'
#	rm -rf $(MODULE_ROOT)/*.o   \
#	$(MODULE_ROOT)/*~        \
#	$(MODULE_ROOT)/.*.d      \
#	$(MODULE_ROOT)/.*.cmd    \
#	$(MODULE_ROOT)/*.ko      \
#	$(MODULE_ROOT)/*.mod.c   \
#	$(MODULE_ROOT)/.tmp_versions   \
#	$(MODULE_ROOT)/Module.symvers   \
#	$(MODULE_ROOT)/*.bak \
