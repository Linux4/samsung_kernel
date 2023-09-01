#
# Makefile for muic devices
#

ifeq ($(CONFIG_USE_MUIC_LEGO),y) # LEGO style
obj-y					+= common/muic-core.o
obj-$(CONFIG_MUIC_NOTIFIER)		+= common/muic_notifier.o
obj-$(CONFIG_MUIC_SM5714)		+= sm/sm5714/sm5714-muic.o sm/sm5714/sm5714-muic-afc.o
ifeq ($(CONFIG_MUIC_SUPPORT_PDIC), y)
obj-$(CONFIG_MUIC_SM5714)		+= sm/sm5714/sm5714-muic-pdic.o
endif
else # Not LEGO style
obj-$(CONFIG_MUIC_KEYBOARD)		+= keyboard_notifier.o

ifeq ($(CONFIG_MUIC_S2MU004),y)
	obj-y					+= muic_core.o s2mu004-muic.o s2mu004-muic-sysfs.o
	obj-$(CONFIG_CCIC_NOTIFIER)		+= muic_manager.o
	obj-$(CONFIG_USB_EXTERNAL_NOTIFY)	+= muic_usb_notification_manager.o
endif
obj-y				+= muic_core.o
obj-$(CONFIG_MUIC_NOTIFIER)		+= muic_notifier.o
obj-$(CONFIG_MUIC_MANAGER)		+= muic_manager.o
obj-$(CONFIG_MUIC_SYSFS)		+= muic_sysfs.o

obj-$(CONFIG_MUIC_MAX77804)		+= max77804-muic.o
obj-$(CONFIG_MUIC_MAX77804K)		+= max77804k-muic.o
obj-$(CONFIG_MUIC_MAX77828)		+= max77828-muic.o
obj-$(CONFIG_MUIC_MAX77843)		+= max77843-muic.o
obj-$(CONFIG_HV_MUIC_MAX77843_AFC)	+= max77843-muic-afc.o
obj-$(CONFIG_MUIC_MAX77833)		+= max77833-muic.o
obj-$(CONFIG_HV_MUIC_MAX77833_AFC)  += max77833-muic-afc.o
obj-$(CONFIG_MUIC_MAX77854)		+= max77854-muic.o
obj-$(CONFIG_HV_MUIC_MAX77854_AFC)  += max77854-muic-afc.o
obj-$(CONFIG_MUIC_MAX77888)		+= max77888-muic.o
obj-$(CONFIG_MUIC_FSA9480)		+= fsa9480.o
obj-$(CONFIG_MUIC_S2MM001)		+= s2mm001.o

ifeq ($(CONFIG_MUIC_HV),y)
obj-$(CONFIG_MUIC_UNIVERSAL)		+= universal/muic_hv.o
obj-$(CONFIG_MUIC_UNIVERSAL_MAX77854)	+= universal/muic_hv_max77854.o
obj-$(CONFIG_MUIC_UNIVERSAL_SM5720)	+= universal/muic_sm5720_afc.o
obj-$(CONFIG_MUIC_S2MU004)	+= s2mu004-muic-afc.o
endif

obj-$(CONFIG_MUIC_S2MU107)		+= s2mu107-muic.o
obj-$(CONFIG_MUIC_S2MU107)		+= s2mu107-muic-afc.o
obj-$(CONFIG_MUIC_S2MU106)		+= s2mu106-muic.o
obj-$(CONFIG_MUIC_S2MU106)		+= s2mu106-muic-afc.o

# MUIC SM5713
obj-$(CONFIG_MUIC_SM5713)	+= sm5713-muic.o sm5713-muic-afc.o
ifeq ($(CONFIG_MUIC_SUPPORT_CCIC),y)
obj-$(CONFIG_MUIC_SM5713)	+= sm5713-muic-ccic.o
endif
endif
