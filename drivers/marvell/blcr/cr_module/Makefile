
obj-m := blcr.o
blcr-objs := \
	cr_async.o \
	cr_barrier.o \
	cr_chkpt_req.o \
	cr_dest_file.o \
	cr_dump_self.o \
	cr_fops.o \
	cr_io.o	\
	cr_module.o	\
	cr_proc.o \
	cr_rstrt_req.o \
	cr_sync.o \
	cr_task.o \
	cr_trigger.o \
	cr_ktrace.o	\
	cr_objects.o \
	cr_compat.o	\
	cr_mmaps.o \
	cr_vmadump.o \
	cr_timers.o	\
	cr_pipes.o \
	cr_creds.o \
	cr_relocate.o \
	cr_watchdog.o \
	vmadump_common.o \
	vmadump_arm.o

EXTRA_CFLAGS := -I$(obj) -I$(obj)/..  -I$(obj)/../include \
	-I$(obj)/../vmadump4 -I$(obj)/arch/arm \
	-D__NR_vmadump=-1 -DBPROC_VERSION="4.0.0pre8"

#.PHONY: all clean
#all:
#	@[ -e "../blcr_imports/Module.symvers"  ] && cat ../blcr_imports/Module.symvers >> Module.symvers; true
#	$(MAKE) -C $(KDIR) M=$(PWD) CROSS_COMPILE=arm-eabi- ARCH=arm modules

#clean:
#	rm -rf *.o *.ko *.mod.c .*.cmd ./.tmp* *~ modules.order Module.symvers
