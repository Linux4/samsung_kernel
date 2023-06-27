#
# Makefile for Linux Telephony
#
# CopyRight (c) 2006 Intel Corp ( vincent.c.yeung@intel.com )
BUILDROOT = $(srctree)/drivers/marvell/marvell-telephony/drivers
TARGET = cploaddev

#$(TARGET)-objs := pxa_cp_load.o ../errorhandler/src/watchdog.o
$(TARGET)-objs := pxa_cp_load.o watchdog.o

ifeq ($(CONFIG_CPU_PXA988),y)
$(TARGET)-objs += pxa988_cp.o
endif

ifeq ($(CONFIG_CPU_PXA1088),y)
$(TARGET)-objs += pxa988_cp.o
endif

EXTRA_CFLAGS += -DENV_LINUX \
		 -I$(BUILDROOT)/errorhandler/inc\
		 -I$(BUILDROOT)/include

#INSTALLDIR = $(PXA_TARGET_FS_DIR)

#all: modules 

#ifneq ($(KERNELRELEASE),) 
obj-m	:= $(TARGET).o

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
