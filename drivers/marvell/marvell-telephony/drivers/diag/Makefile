#
# Makefile for Linux Telephony
#
# CopyRight (c) 2006 Intel Corp ( vincent.c.yeung@intel.com )
BUILDROOT = $(srctree)/drivers/marvell/marvell-telephony/drivers
diag-objs := diag_drv.o diag_channel_kernel.o
gs_diag-objs := diag_usb.o

#EXTRA_CFLAGS += -I$(PXA_APPS_DIR)/include -I$(BUILDROOT)/include -I$(BUILDROOT)/shmem/msocket -DEXPORT_SYMTAB
EXTRA_CFLAGS += -I$(BUILDROOT)/include -I$(BUILDROOT)/shmem/msocket -DEXPORT_SYMTAB
#EXTRA_CFLAGS += -DDIAG_USB_TEST

#KBUILD_EXTRA_SYMBOLS = $(BUILDROOT)/shmem/msocket/Module.symvers

#all: modules

#ifneq ($(KERNELRELEASE),)

#obj-m	:= $(TARGET).o
obj-m	:= diag.o gs_diag.o


#else

#modules:
#	$(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(shell pwd) modules
#endif


#clean:
#	@echo 'Going to cleanup.\n'
#	rm -rf    \
#	*.o       \
#	*~        \
#	core      \
#	.*.d      \
#	.*.cmd    \
#	*.ko      \
#	*.mod.c   \
#	*.mod.c   \
#	Module.symvers \
#	.tmp_versions   \
#	*.bak
