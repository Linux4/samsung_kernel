#
# this makefile is called from the kernel make system
# "-O" is needed to expand inlines
ifeq ($(MODE),release)
    ccflags-y = -O2 -DNDEBUG
else
    ccflags-y = -O -g3 -DDEBUG -DDEBUG_VERBOSE
endif

ccflags-y += -I$(KERNEL_API_MODULE) -I$(M) -I$(M)/inc -I$(COMP_PATH_MobiCoreDriverKernelApi)/Public

ifdef MOBICORE_CFLAGS
    ccflags-y += $(MOBICORE_CFLAGS)
endif

# Linker script
# KBUILD_LDFLAGS_MODULE += -T $(SUBDIRS)/mcApiTest.lds
# Extra symbols from the API module
KBUILD_EXTRA_SYMBOLS="$(KERNEL_API_MODULE)/Module.symvers"
ccflags-y += -Wno-declaration-after-statement -Wno-missing-noreturn -Wno-return-type

# add our module to kernel.
obj-m += t-base-tui.o

t-base-tui-objs := main.o \
	               tlcTui.o \

clean:
	@echo $(COMP_PATH_MobiCoreDriverKernelApi)
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions \
		Module.markers Module.symvers modules.order

depend .depend dep:
	$(CC) $(CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
    include .depend
endif
