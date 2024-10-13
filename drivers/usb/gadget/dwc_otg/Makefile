#
# Makefile for DWC_otg Highspeed USB controller driver
#

#PWD		:= $(shell pwd)
#PORTLIB		:= $(PWD)/../dwc_common_port

ifeq ($(CONFIG_USB_GADGET_DEBUG),y)
EXTRA_CFLAGS            += -DDEBUG
endif

# Use one of the following flags to compile the software in host-only or
# device-only mode.
#EXTRA_CFLAGS        += -DDWC_HOST_ONLY
ifeq ($(CONFIG_SPRD_USB_DEVICE_ONLY),y)
EXTRA_CFLAGS        += -DDWC_DEVICE_ONLY
endif

#EXTRA_CFLAGS	+= -Dlinux -DDWC_HS_ELECT_TST
#EXTRA_CFLAGS	+= -DDWC_EN_ISOC
EXTRA_CFLAGS   	+= -DDWC_LINUX
#EXTRA_CFLAGS   	+= $(CFI)
obj-y		+= dwc_otg.o usb_hw.o

dwc_otg-objs	:= dwc_otg_driver.o dwc_otg_attr.o
dwc_otg-objs	+= dwc_otg_cil.o dwc_otg_cil_intr.o
dwc_otg-objs	+= dwc_otg_pcd_linux.o dwc_otg_pcd.o dwc_otg_pcd_intr.o
dwc_otg-objs	+= dwc_common_linux.o dwc_otg_adp.o
ifndef CONFIG_SPRD_USB_DEVICE_ONLY
#host related 
dwc_otg-objs	+= dwc_otg_hcd.o dwc_otg_hcd_intr.o
dwc_otg-objs	+= dwc_otg_hcd_linux.o dwc_otg_hcd_queue.o 
dwc_otg-objs	+= dwc_otg_hcd_ddma.o  
endif
