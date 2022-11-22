if CR_BUILD_INCLUDE
include_HEADERS = blcr_ioctl.h blcr_proc.h blcr_errcodes.h libcr.h
nodist_include_HEADERS = blcr_common.h
noinst_HEADERS = blcr_ksyms.h blcr_vmadump.h
nodist_noinst_HEADERS = blcr_imports.h
else
nodist_noinst_HEADERS = blcr_common.h blcr_imports.h
endif

if CR_BUILD_MODULES
# Additional dependence for file generated via AC_SUBST_FILE()
blcr_imports.h: $(top_builddir)/.import_decls
endif

all-local:
	if test -d $(top_builddir)/libcr32/include; then $(MAKE) $(AM_MAKEFLAGS) -C $(top_builddir)/libcr32/include all; fi
