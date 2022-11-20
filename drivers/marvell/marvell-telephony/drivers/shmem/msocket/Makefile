#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License version 2 as
#  published by the Free Software Foundation.
# (C) Copyright 2007 Marvell International Ltd.
BUILDROOT = $(srctree)/drivers/marvell/marvell-telephony/drivers
TARGET = msocketk

$(TARGET)-objs := portqueue.o shm.o msocket.o acipcd.o data_path.o direct_rb.o

EXTRA_CFLAGS += -I$(KERNELDIR)/include \
				 -I$(KERNELDIR)/arch/arm/mach-mmp/include\
				 -I$(BUILDROOT)/include\
				 -I$(BUILDROOT)/diag\
				 -I$(BUILDROOT)/cp_load \
				 -I./

EXTRA_CFLAGS += -DEXPORT_SYMTAB \
	-DENV_LINUX\
	-DACI_LNX_KERNEL \
	-DEXPORT_SYMTAB \
	
	#-DTX_TASK_ENABLE \

#KBUILD_EXTRA_SYMBOLS = $(BUILDROOT)/cp_load/Module.symvers

#all: modules

#ifneq ($(KERNELRELEASE),)
 
obj-m	:= $(TARGET).o
#CC =arm-linux-gcc
						     
#else

#modules: 
#	$(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(shell pwd) modules
#endif

#clean:
#	rm -rf msocket.o
#	rm -rf *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions *.bak *.o *.symvers *.order *.mod
