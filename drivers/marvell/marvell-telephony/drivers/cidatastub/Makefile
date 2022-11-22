#
#(C) Copyright 2007 Marvell International Ltd.
#All Rights Reserved
#
BUILDROOT = $(srctree)/drivers/marvell/marvell-telephony/drivers
TARGET = cci_datastub


$(TARGET)-objs := ccidatastub.o data_channel_kernel.o psd_data_channel.o

EXTRA_CFLAGS += -I$(BUILDROOT)/shmem/msocket -I$(BUILDROOT)/include
EXTRA_CFLAGS += -DSAL_LITTLE_ENDIAN
EXTRA_CFLAGS += -DENV_LINUX -DACI_LNX_KERNEL -DCS_PS_PORTS 

#KBUILD_EXTRA_SYMBOLS = $(BUILDROOT)/shmem/msocket/Module.symvers

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
#	su -c "rm -f $(INSTALLDIR)/dev/ccidatastub && mknod $(INSTALLDIR)/dev/ccidatastub c 246 0"

#clean:
#	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.bak Module.symvers
