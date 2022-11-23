
# Makefile for WCNSS triple-play driver


ifeq ($(HQ_FACTORY_BUILD), true)
EXTRA_CFLAGS += -DHQ_FACTORY_BUILD
$(warning 'wcnss HQ_FACTORY_BUILD =' $(HQ_FACTORY_BUILD))
$(warning 'wcnss KBUILD_CFLAGS =' $(KBUILD_CFLAGS))
endif

wcnsscore-objs += wcnss_wlan.o wcnss_vreg.o

obj-$(CONFIG_WCNSS_CORE) += wcnsscore.o
