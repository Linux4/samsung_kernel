pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = agmserver.pc
EXTRA_DIST = $(pkgconfig_DATA)

h_sources = ./inc/agm-dbus-utils.h \
            ./inc/agm_server_wrapper_dbus.h

AM_CPPFLAGS := -I ./inc
AM_CPPFLAGS += -D__unused=__attribute__\(\(__unused__\)\)

library_include_HEADERS = $(h_sources)
library_includedir = $(includedir)/qti-agm-service/

lib_LTLIBRARIES = libagmserverwrapper.la
libagmserverwrapper_la_SOURCES = ./src/agm-dbus-utils.cpp ./src/agm_server_wrapper_dbus.cpp
libagmserverwrapper_la_CPPFLAGS := $(AM_CPPFLAGS) $(DBUS_CFLAGS)
libagmserverwrapper_la_LIBADD = -lagm
libagmserverwrapper_la_LDFLAGS = -ldl -shared -avoid-version -lpthread -lar_osal $(DBUS_LIBS)

bin_PROGRAMS := agm_server

agm_server_SOURCES := ./src/agm-server-daemon.cpp
agm_server_la_CPPFLAGS := $(AM_CPPFLAGS)
agm_server_LDADD := libagmserverwrapper.la $(GLIB_LIBS)
agm_server_la_LDFLAGS = -ldl -shared -avoid-version

