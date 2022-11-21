#
# Makefile for the drivers of Marvell devices.
#
obj-y   += marvell-telephony/
ifdef CONFIG_TZ_HYPERVISOR
obj-y   += security/mmp-tzdd/src/
else
obj-y   += security/mmp-geu/
endif
obj-y	+= sd8787/
obj-y	+= mpdc/

obj-y   += graphics/galcore_4x/
