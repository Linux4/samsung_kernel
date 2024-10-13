AM_CFLAGS = -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -Wno-unused-variable

if KERNELMODULES
AM_CFLAGS += -I./../inc $(KERNEL_DIR)/include
else
AM_CFLAGS += -I./../inc
endif

#AM_CFLAGS += -g -DDEBUG -DNAT_DEBUG

common_CFLAGS =  -DUSE_GLIB @GLIB_CFLAGS@

if !KERNELMODULES
common_LDFLAGS = -lrt @GLIB_LIBS@
endif

if KERNELMODULES
library_includedir = ../inc $(KERNEL_DIR)/include $(pkgincludedir)
else
library_includedir = $(pkgincludedir)
endif

cpp_sources = \
  ipa_nat_map.cpp

c_sources   = \
  ipa_table.c \
  ipa_nat_statemach.c \
  ipa_nat_drvi.c \
  ipa_nat_drv.c \
  ipa_mem_descriptor.c \
  ipa_nat_utils.c \
  ipa_ipv6ct.c

library_include_HEADERS = \
  ../inc/ipa_mem_descriptor.h \
  ../inc/ipa_nat_drv.h \
  ../inc/ipa_nat_drvi.h \
  ../inc/ipa_nat_map.h \
  ../inc/ipa_nat_statemach.h \
  ../inc/ipa_nat_utils.h \
  ../inc/ipa_table.h

if KERNELMODULES
noinst_LIBRARIES = libipanat.a
libipanat_a_C = @C@
libipanat_a_CC = @CC@
libipanat_a_SOURCES = $(c_sources) $(cpp_sources)
libipanat_a_CFLAGS = $(AM_CFLAGS) $(common_CFLAGS)
libipanat_a_CXXFLAGS = $(AM_CFLAGS) $(common_CPPFLAGS)
else
lib_LTLIBRARIES = libipanat.la
libipanat_la_C = @C@
libipanat_la_CC = @CC@
libipanat_la_SOURCES = $(c_sources) $(cpp_sources)
libipanat_la_CFLAGS = $(AM_CFLAGS) $(common_CFLAGS)
libipanat_la_CXXFLAGS = $(AM_CFLAGS) $(common_CPPFLAGS)
libipanat_la_LDFLAGS = -shared $(common_LDFLAGS) -version-info 1:0:0
endif
