# Note: build is done via cr_module/kbuild/Makefile.in

EXTRA_DIST = license.txt vmadump_common.c \
	     vmadump_i386.c vmadump_x86_64.c \
	     vmadump_ppc.c vmadump_ppc64.c \
	     vmadump_sparc.c vmadump_sparc64.c \
	     vmadump_alpha.c vmadump_arm.c

BPROC_VERSION = "4.0.0pre8"
INCLUDES = -D__NR_vmadump=-1 -DBPROC_VERSION='$(BPROC_VERSION)' -I$(top_srcdir)/include
AM_CFLAGS = -Wall -s

# vmadcheck is needed only for cr_info and is currently broken -PHH 12.5.2003
if BUILD_CR_INFO
vmadcheck = vmadcheck
else
vmadcheck =
endif

# vmadlib is not used with blcr
if BUILD_VMADLIB
vmadlib = vmadlib
else
vmadlib =
endif

libexec_PROGRAMS = $(vmadcheck) $(vmadlib)
noinst_HEADERS = vmadump.h vmadump_x86.h
check_PROGRAMS = vmadtest vmadstress

AM_INSTALLCHECK_STD_OPTIONS_EXEMPT = $(libexec_PROGRAMS)
