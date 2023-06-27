#
# Makefile for Linux Telephony
#
# CopyRight (c) 2006 Intel Corp ( vincent.c.yeung@intel.com )
BUILDROOT = $(srctree)/drivers/marvell/marvell-telephony/drivers
TARGET = cidatattydev

$(TARGET)-objs := cidatatty.o


EXTRA_CFLAGS += -I$(BUILDROOT)/cidatastub\
		-I$(BUILDROOT)/include

EXTRA_CFLAGS += -DENV_LINUX

#KBUILD_EXTRA_SYMBOLS = $(BUILDROOT)/cidatastub/Module.symvers


#all: modules 

#ifneq ($(KERNELRELEASE),)
 
obj-m	:= $(TARGET).o
#CC =arm-linux-gcc

#else

#modules: 
#	$(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(shell pwd) modules
#endif

#install:
#	install -d $(INSTALLDIR)
#	install -c $(TARGET).ko $(INSTALLDIR)

#dev:
	
#clean:
#	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.bak *.symvers *.order
