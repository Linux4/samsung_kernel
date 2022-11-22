#
# Makefile for muic devices
#

#s2m muic phy layer
s2m_muic_module-$(CONFIG_MUIC_S2MU106)	+= s2mu106/s2mu106-muic.o
s2m_muic_module-$(CONFIG_MUIC_S2MF301)	+= s2mf301/s2mf301-muic.o

#s2m muic common layer
s2m_muic_module-$(CONFIG_USE_S2M_MUIC)	+= common/muic_core.o common/muic_manager.o
s2m_muic_module-$(CONFIG_MUIC_SYSFS)	+= common/muic_sysfs.o

obj-$(CONFIG_USE_S2M_MUIC)	+= s2m_muic_module.o
