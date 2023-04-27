# SPDX-License-Identifier: GPL-2.0-only

obj-$(CONFIG_CNSS2) += cnss2.o

cnss2-y := main.o bus.o debug.o pci.o power.o genl.o

ifneq ($(CONFIG_DEV_COREDUMP), y)
	cnss2-y += cnss_devcoredump.o
endif

cnss2-$(CONFIG_CNSS2_QMI) += qmi.o wlan_firmware_service_v01.o coexistence_service_v01.o ip_multimedia_subsystem_private_service_v01.o
