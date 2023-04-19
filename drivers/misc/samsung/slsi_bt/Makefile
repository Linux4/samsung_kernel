# Needed since this subdir is symlinked in the main Kernel tree
# without this our samsung subdir is NOT cleaned.
clean-files := *.o *.ko

#
# SLSI Bluetooth driver
ifeq ($(CONFIG_KUNIT_SLSI_BT_TEST),y)
obj-$(CONFIG_KUNIT_SLSI_BT_TEST) += unittest/
obj-$(CONFIG_KUNIT_SLSI_BT_TEST) += mock/
obj-$(CONFIG_KUNIT_SLSI_BT_TEST) += scsc_bt.o
scsc_bt-y += \
	slsi_bt_err.o\
	slsi_bt_controller.o\
	slsi_bt_property.o\
	hci_h4.o\
	slsi_bt_tty.o\
	slsi_bt_log.o\

ccflags-$(CONFIG_KUNIT_SLSI_BT_TEST) += -DCONFIG_SCSC_INDEPENDENT_SUBSYSTEM
else

obj-$(CONFIG_SLSI_BT) += scsc_bt.o

scsc_bt-y += slsi_bt_io.o
scsc_bt-y += \
	slsi_bt_err.o\
	slsi_bt_controller.o\
	hci_pkt.o\
	hci_trans.o\
	slsi_bt_property.o\
	slsi_bt_log.o\

scsc_bt-$(CONFIG_SLSI_BT_H4) += hci_h4.o
ifneq ($(CONFIG_SLSI_BT_USE_UART_INTERFACE), )
scsc_bt-$(CONFIG_SLSI_BT_USE_UART_INTERFACE) += slsi_bt_tty.o
scsc_bt-$(CONFIG_SLSI_BT_BCSP) += hci_bcsp.o
endif

ifeq ($(CONFIG_SLSI_BT_USE_MIF_INTERFACE), m)
ccflags-y += -DCONFIG_SLSI_BT_USE_MIF_INTERFACE
endif
ifeq ($(CONFIG_SLSI_BT_H4), m)
ccflags-y += -DCONFIG_SLSI_BT_H4
endif
ifeq ($(CONFIG_SLSI_BT_BCSP), m)
ccflags-y += -DCONFIG_SLSI_BT_BCSP
endif

ifeq ($(CONFIG_SCSC_LOG_COLLECTION), m)
ccflags-y += -DCONFIG_SCSC_LOG_COLLECTION
endif

## See sibling scsc/ Makefile for an explanation of the reasons of
## the following ifeq/else
ifeq ($(CONFIG_SCSC_INDEPENDENT_SUBSYSTEM), m)
ccflags-y += -DCONFIG_SCSC_INDEPENDENT_SUBSYSTEM
endif

ifeq ($(CONFIG_SCSC_LOGRING), m)
ccflags-y += -DCONFIG_SCSC_PRINTK
else
ccflags-$(CONFIG_SCSC_LOGRING) += -DCONFIG_SCSC_PRINTK
endif

ifneq ($(CONFIG_ARCH_EXYNOS), )
ccflags-y += -DCONFIG_EXYNOS_UNIQUE_ID
endif
ifneq ($(CONFIG_ARCH_EXYNOS9), )
ccflags-y += -DCONFIG_EXYNOS_UNIQUE_ID
endif
endif

GCOV_PROFILE := y
