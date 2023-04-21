lib_LTLIBRARIES      = libagmclient.la
libagmclient_ladir = $(libdir)
libagmclient_la_LDFLAGS = -ldl -shared -avoid-version -lrt
libagmclient_la_SOURCES = src/agm_client_wrapper_dbus.cpp
libagmclient_la_LDFLAGS += $(GLIB_LIBS) -lgobject-2.0 -lgio-2.0 -lar_osal

