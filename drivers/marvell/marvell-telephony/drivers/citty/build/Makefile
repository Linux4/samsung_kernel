#
# Makefile for Linux Telephony
#
# CopyRight (c) 2006 Intel Corp ( vincent.c.yeung@intel.com )

#ifndef CITTY_ROOT

#CITTY_ROOT = $(shell pwd)/..

#endif

TARGET = citty

$(TARGET)-objs := ../src/$(TARGET).o

#all: modules

#ifneq ($(KERNELRELEASE),)
 
obj-m   := $(TARGET).o
#CC =arm-linux-gcc
#PATH=$PATH:/usr/local/arm-linux/bin:/bin

#else

#modules: 
#	$(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(CITTY_ROOT)/build modules
#endif

#install:
#	install -d $(INSTALLDIR)
#	install -c ../src/$(TARGET).ko $(INSTALLDIR)

#dev:
#	su -c "rm -f $(INSTALLDIR)/dev/citty0 && mknod $(INSTALLDIR)/dev/citty0 c 244 0"
#	su -c "rm -f $(INSTALLDIR)/dev/citty1 && mknod $(INSTALLDIR)/dev/citty1 c 244 1"
#	su -c "rm -f $(INSTALLDIR)/dev/citty2 && mknod $(INSTALLDIR)/dev/citty2 c 244 2"
#	su -c "rm -f $(INSTALLDIR)/dev/cctdev0 && mknod $(INSTALLDIR)/dev/cctdev0 c 245 0"
#	su -c "rm -f $(INSTALLDIR)/dev/cctdev1 && mknod $(INSTALLDIR)/dev/cctdev1 c 245 1"
#	su -c "rm -f $(INSTALLDIR)/dev/cctdev2 && mknod $(INSTALLDIR)/dev/cctdev2 c 245 2"
#	su -c "rm -f $(INSTALLDIR)/dev/ppp && mknod $(INSTALLDIR)/dev/ppp c 108 0"

#clean:
#	@echo 'Going to cleanup.\n'
#	rm -rf                      \
#	$(CITTY_ROOT)/src/*.o       \
#	$(CITTY_ROOT)/src/*~        \
#	$(CITTY_ROOT)/src/core      \
#	$(CITTY_ROOT)/src/.*.d      \
#	$(CITTY_ROOT)/src/.*.cmd    \
#	$(CITTY_ROOT)/src/*.ko      \
#	$(CITTY_ROOT)/src/*.mod.c   \
#	$(CITTY_ROOT)/build/.tmp_versions   \
#	$(CITTY_ROOT)/build/Module.symvers   \
#	$(CITTY_ROOT)/src/*.bak
