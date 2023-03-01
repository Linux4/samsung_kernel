#
# Makefile for muic devices
#

obj-$(CONFIG_MUIC_NOTIFIER)		+= muic_notifier.o
obj-$(CONFIG_MUIC_MANAGER)		+= muic_manager.o
obj-$(CONFIG_MUIC_CORE)			+= muic_core.o
obj-$(CONFIG_MUIC_SYSFS)		+= muic_sysfs.o
GCOV_PROFILE := $(CONFIG_KUNIT)
obj-$(CONFIG_KUNIT) += test/
