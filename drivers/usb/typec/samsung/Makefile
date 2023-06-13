
#s2m pdic phy layer
s2m_pdic_module-$(CONFIG_PDIC_S2MU106)		+= s2mu106/s2mu106-usbpd.o
s2m_pdic_module-$(CONFIG_PDIC_S2MF301)		+= s2mf301/s2mf301-usbpd.o

#s2m pdic common layer
s2m_pdic_module-$(CONFIG_USE_S2M_PDIC)		+= common/usbpd.o common/usbpd_cc.o common/usbpd_policy.o common/usbpd_manager.o
s2m_pdic_module-$(CONFIG_PDIC_SYSFS)		+= common/usbpd_sysfs.o

obj-$(CONFIG_USE_S2M_PDIC)		+= s2m_pdic_module.o
