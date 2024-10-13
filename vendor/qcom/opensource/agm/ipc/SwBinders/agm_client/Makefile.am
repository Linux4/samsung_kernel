
AM_CPPFLAGS := -I $(top_srcdir) -I $(PKG_CONFIG_SYSROOT_DIR)/usr/include/agm/
AM_CPPFLAGS += -DDYNAMIC_LOG_ENABLED
lib_LTLIBRARIES      = libagmclient.la
libagmclient_ladir = $(libdir)
libagmclient_la_SOURCES   = src/agm_client_wrapper.cpp
libagmclient_la_CPPFLAGS := $(AM_CPPFLAGS)
libagmclient_la_LDFLAGS   = -lcutils -llog -ldl -lbinder -shared -avoid-version -lutils -lrt
libagmclient_la_LIBADD     =  -lagmproxy -laudio_log_utils
