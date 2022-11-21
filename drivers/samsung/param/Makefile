#
#   Makefile for Parameter
#
#   Copyright(c) 2004-2006, Samsung Electronics, Co., Ltd.
#


MOD_NAME = param

ifneq ($(KERNELRELEASE), )

EXTRA_CFLAGS += -I$(PRJROOT)/modules/fsr/Inc

obj-m := $(MOD_NAME).o

else

all:
	@$(MAKE) -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules

clean:
	rm -f *.o *.ko *.mod.* *~ .*.cmd

install:
	@$(MAKE) --no-print-directory -C $(KDIR) \
		SUBDIRS=$(CURDIR) modules_install

endif
