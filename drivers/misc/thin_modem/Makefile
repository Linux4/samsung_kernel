# Makefile of modem_v2

EXTRA_CFLAGS += -Idrivers/misc/modem_v2

obj-y += sipc5_modem.o sipc5_io_device.o
obj-y += sipc4_modem.o sipc4_io_device.o
obj-y += modem_net_flowcontrol_device.o modem_utils.o

obj-$(CONFIG_LTE_MODEM_SHANNON) += modem_modemctl_device_shannon.o
obj-$(CONFIG_LTE_MODEM_M74XX) += modem_modemctl_device_m74xx.o

ifeq ($(CONFIG_LINK_DEVICE_HSIC), y)
obj-y += modem_link_device_hsic_ncm.o link_usb_cdc_ncm.o
obj-$(CONFIG_LTE_MODEM_SHANNON) += link_pm_hsic_shannon.o
obj-$(CONFIG_LTE_MODEM_M74XX) += link_pm_hsic_m74xx.o
endif
obj-$(CONFIG_LINK_DEVICE_SPI) += modem_link_device_spi_boot.o
