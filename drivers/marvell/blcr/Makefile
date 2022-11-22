$(obj)/blcr_imports/imports.c: $(objtree)/System.map 
		-cd $(srctree)/$(src);cp -f blcr_kcon blcr_symb blcr_symb.sh $(objtree)/$(obj)
		cd $(obj);./blcr_symb.sh  $< 2>/dev/null

clean-files = blcr_imports/imports.c blcr_config.h

#-----------------------------------------------------------------------------

obj-m    := blcr_imports.o
blcr_imports-objs := blcr_imports/imports.o blcr_imports/module.o

$(addprefix $(obj)/,$(blcr_imports-objs)) : | $(obj)/blcr_imports/imports.c

EXTRA_CFLAGS := -I$(obj) -I$(obj)/include

#-----------------------------------------------------------------------------

obj-m += blcr.o
blcr-objs := \
	cr_module/cr_async.o \
	cr_module/cr_barrier.o \
	cr_module/cr_chkpt_req.o \
	cr_module/cr_dest_file.o \
	cr_module/cr_dump_self.o \
	cr_module/cr_fops.o \
	cr_module/cr_io.o	\
	cr_module/cr_module.o	\
	cr_module/cr_proc.o \
	cr_module/cr_rstrt_req.o \
	cr_module/cr_sync.o \
	cr_module/cr_task.o \
	cr_module/cr_trigger.o \
	cr_module/cr_ktrace.o	\
	cr_module/cr_objects.o \
	cr_module/cr_compat.o	\
	cr_module/cr_mmaps.o \
	cr_module/cr_vmadump.o \
	cr_module/cr_timers.o	\
	cr_module/cr_pipes.o \
	cr_module/cr_creds.o \
	cr_module/cr_relocate.o \
	cr_module/cr_watchdog.o \
	cr_module/vmadump_common.o \
	cr_module/vmadump_arm.o

EXTRA_CFLAGS := -I$(obj)/cr_module -I$(obj)  -I$(obj)/include \
	-I$(obj)/vmadump4 -I$(obj)/cr_module/arch/arm \
	-D__NR_vmadump=-1 -DBPROC_VERSION="4.0.0pre8"

$(addprefix $(obj)/,$(blcr-objs)) : | $(obj)/blcr_imports/imports.c

