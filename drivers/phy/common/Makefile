#
# Makefile for usbpd common drivers
#
subdir-ccflags-y := -Wformat
obj-$(CONFIG_USB_REPEATER)		+= usb_repeater_module.o
usb_repeater_module-y		:= repeater_core.o
GCOV_PROFILE_repeater_core.o := $(CONFIG_SEC_KUNIT)
