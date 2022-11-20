#
# Makefile for muic devices
#

obj-$(CONFIG_MUIC_NOTIFIER)		+= muic_notifier.o
obj-$(CONFIG_MUIC_MANAGER)		+= muic_manager.o
obj-$(CONFIG_MUIC_CORE)			+= muic_core.o
obj-$(CONFIG_MUIC_SYSFS)		+= muic_sysfs.o
obj-$(CONFIG_MUIC_SM5714)		+= sm5714-muic.o sm5714-muic-afc.o
ifeq ($(CONFIG_MUIC_SUPPORT_PDIC), y)
obj-$(CONFIG_MUIC_SM5714)		+= sm5714-muic-pdic.o
endif
obj-$(CONFIG_MUIC_S2MU004)		+= muic_core.o s2mu004-muic.o s2mu004-muic-sysfs.o
obj-$(CONFIG_MUIC_S2MU106)		+= s2mu106-muic.o
obj-$(CONFIG_MUIC_S2MU107)		+= s2mu107-muic.o
obj-$(CONFIG_HV_MUIC_S2MU004_AFC)	+= s2mu004-muic-afc.o
obj-$(CONFIG_HV_MUIC_S2MU106_AFC)	+= s2mu106-muic-afc.o
obj-$(CONFIG_HV_MUIC_S2MU107_AFC)	+= s2mu107-muic-afc.o
ifdef CONFIG_MUIC_SUPPORT_CCIC
obj-$(CONFIG_MUIC_S2MU005)		+= s2mu005-muic-typec.o
else
obj-$(CONFIG_MUIC_S2MU005)		+= muic-core.o s2mu005-muic.o
endif
