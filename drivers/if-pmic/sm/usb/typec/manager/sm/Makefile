#
# Makefile for usb typec manager drivers
#
subdir-ccflags-y := -Wformat
obj-$(CONFIG_USB_TYPEC_MANAGER_NOTIFIER_SM5714)	+= usb_typec_manager.o
usb_typec_manager-y				:= usb_typec_manager_notifier.o
usb_typec_manager-$(CONFIG_USB_HW_PARAM)	+= usb_typec_manager_hwparam.o

ifeq ($(CONFIG_SEC_KUNIT), y)
GCOV_PROFILE_usb_typec_manager_notifier.o := $(CONFIG_SEC_KUNIT)
endif

obj-$(CONFIG_IF_CB_MANAGER_SM5714)			+= if_cb_manager.o

