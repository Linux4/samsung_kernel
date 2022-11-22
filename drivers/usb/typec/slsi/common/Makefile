#
# Makefile for usbpd devices
#

obj-$(CONFIG_PDIC_SLSI_NON_MCU)		+= usbpd.o usbpd_cc.o usbpd_policy.o usbpd_manager.o s2m_pdic_notifier.o
GCOV_PROFILE := $(CONFIG_KUNIT)
obj-$(CONFIG_KUNIT) += test/
