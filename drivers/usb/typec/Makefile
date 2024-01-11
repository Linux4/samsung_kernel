obj-$(CONFIG_TYPEC)		+= typec.o
obj-$(CONFIG_TYPEC_WCOVE)	+= typec_wcove.o
obj-$(CONFIG_TYPEC_UCSI)	+= ucsi/

obj-$(CONFIG_CCIC_SM5713)	+= sm5713/
obj-$(CONFIG_CCIC_S2MU107)	+= s2mu107/
obj-$(CONFIG_CCIC_S2MU106)	+= s2mu107/
obj-$(CONFIG_CCIC_PM6150)	+= pm6150/
obj-y		+= sep/

ifeq ($(CONFIG_USE_PDIC_LEGO), y)
	obj-$(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)		+= manager/lego/
	obj-$(CONFIG_PDIC_NOTIFIER)		+= common/
	obj-$(CONFIG_PDIC_SM5714)		+= sm/sm5714/
else
	obj-$(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)	+= manager/
endif
