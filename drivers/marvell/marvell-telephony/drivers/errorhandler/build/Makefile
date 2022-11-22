#
# Makefile for Linux System Error Handler
#
BUILDROOT = $(srctree)/drivers/marvell/marvell-telephony/drivers
#ifndef SEH_ROOT

#SEH_ROOT = $(shell pwd)/..

#endif

TARGET = seh

$(TARGET)-objs := ../src/seh_linux.o \
				 ../src/watchdog.o

#INC_DIR = $(KBUILD_EXTMOD)/../inc
INC_DIR = $(BUILDROOT)/errorhandler/inc
EXTRA_CFLAGS += -I$(INC_DIR)
EXTRA_CFLAGS +=-I$(BUILDROOT)/include
EXTRA_CFLAGS +=-I$(BUILDROOT)/cp_load

#KBUILD_EXTRA_SYMBOLS = $(BUILDROOT)/cp_load/Module.symvers

#all: modules

#ifneq ($(KERNELRELEASE),)

obj-m   := $(TARGET).o
#CC =arm-linux-gcc

#else

#modules: 
#	$(MAKE) -C $(KERNELDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(SEH_ROOT)/build modules
#endif

#install:
#	install -d $(INSTALLDIR)
#	install -c ../src/$(TARGET).ko $(INSTALLDIR)

#clean:
#	@echo 'Going to cleanup.\n'
#	rm -rf                      \
#	$(SEH_ROOT)/src/*.o       \
#	$(SEH_ROOT)/src/*~        \
#	$(SEH_ROOT)/src/core      \
#	$(SEH_ROOT)/src/.*.d      \
#	$(SEH_ROOT)/src/.*.cmd    \
#	$(SEH_ROOT)/src/*.ko      \
#	$(SEH_ROOT)/src/*.mod.c   \
#	$(SEH_ROOT)/build/.tmp_versions   \
#	$(SEH_ROOT)/build/Module.symvers   \
#	$(SEH_ROOT)/src/*.bak \
#	$(SEH_ROOT)/build/*.ko \
#	$(SEH_ROOT)/build/*.mod.c
