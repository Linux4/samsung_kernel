# SPDX-License-Identifier: GPL-2.0
#
# USB peripheral controller drivers
#

ccflags-y := -I$(srctree)/drivers/usb/gadget/
ccflags-y += -I$(srctree)/drivers/usb/gadget/function

ccflags-y += -I$(DEVICE_MODULES_PATH)/drivers/usb/ss_function/

# USB Functions
usb_f_dm-y			:= f_dm.o
obj-$(CONFIG_USB_F_DM)	+= usb_f_dm.o
usb_f_conn_gadget-y             := f_conn_gadget.o
obj-$(CONFIG_USB_F_CONN_GADGET) += usb_f_conn_gadget.o
usb_f_ss_mon_gadget-y             := f_ss_mon_gadget.o
obj-$(CONFIG_USB_F_SS_MON_GADGET) += usb_f_ss_mon_gadget.o

