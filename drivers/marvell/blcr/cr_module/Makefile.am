# Needed by kbuild glue
my_sources = \
		cr_async.c	\
		cr_barrier.c	\
		cr_chkpt_req.c	\
		cr_dest_file.c	\
		cr_dump_self.c	\
		cr_fops.c	\
		cr_io.c		\
		cr_module.c	\
		cr_proc.c	\
		cr_rstrt_req.c	\
		cr_sync.c	\
		cr_task.c	\
		cr_trigger.c	\
		cr_module.h	\
		cr_kcompat.h	\
		cr_barrier.h	\
		cr_ktrace.h	\
		cr_ktrace.c	\
		cr_objects.c	\
		cr_context.h	\
		cr_compat.c	\
		cr_mmaps.c      \
		cr_vmadump.c    \
		cr_timers.c	\
		cr_pipes.c	\
		cr_creds.c	\
		cr_relocate.c	\
		cr_watchdog.c

BPROC_VERSION	= "4.0.0pre8"
vmadump_dir	= $(top_srcdir)/vmadump4
vmad_sources	= $(vmadump_dir)/vmadump_common.c $(vmadump_dir)/vmadump_@CR_KARCH@.c
vmad_include	= -I$(vmadump_dir) -D__NR_vmadump=-1 -DBPROC_VERSION='$(BPROC_VERSION)'

cr_sources	= $(my_sources) $(vmad_sources)
INCLUDES =	-I$(top_builddir)/include	\
		-I$(top_srcdir)/include		\
		-I$(srcdir)/arch/@CR_KARCH@	\
		$(vmad_include)

# echo an arbitrary make/environment variable (needed by kbuild glue)
echoval:
	@echo $($(VARNAME)) | sed 's/"/\\"/'g
.PHONY: echoval

EXTRA_DIST = arch license.txt $(my_sources)
SUBDIRS = kbuild .
