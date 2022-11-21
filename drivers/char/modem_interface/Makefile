#
# Makefile for DWC_otg Highspeed USB controller driver
#

#PWD		:= $(shell pwd)
#PORTLIB		:= $(PWD)/../dwc_common_port

ifeq ($(CONFIG_MODEM_INTF_DEBUG),y)
        EXTRA_CFLAGS            += -DDEBUG
endif

obj-$(CONFIG_MODEM_INTF) += modem_interface.o
modem_interface-objs	:= boot_driver.o boot_protocol.o modem_buffer.o
modem_interface-objs	+= normal_mode.o  modem_gpio_drv.o
modem_interface-objs	+= modem_sdio_drv.o modem_interface_driver.o
