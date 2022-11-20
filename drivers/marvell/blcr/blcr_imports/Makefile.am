# Additional dependence for file generated via AC_SUBST_FILE()
all-local: imports.c
imports.c: $(top_builddir)/.import_calls

# Vars needed by kbuild
SOURCES =	imports.c module.c
INCLUDES =	-I$(top_builddir)/include	\
		-I$(top_srcdir)/include

# echo an arbitrary make/environment variable (needed by kbuild glue)
echoval:
	@echo $($(VARNAME)) | sed 's/"/\\"/'g
.PHONY: echoval

EXTRA_DIST = license.txt module.c

# Must visit . first, to keep generated source file imports.c up to date
SUBDIRS = . kbuild

dist-hook:
	rm -f $(distdir)/imports.c
