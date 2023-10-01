# SPDX-License-Identifier: GPL-2.0
#
# USB peripheral controller drivers
#
subdir-ccflags-$(CONFIG_USB_GADGET_DEBUG)	:= -DDEBUG
subdir-ccflags-$(CONFIG_USB_GADGET_VERBOSE)	+= -DVERBOSE_DEBUG
ccflags-y				+= -I$(srctree)/drivers/usb/gadget/udc
# TabA7 Lite code for USB-IF Ellisys TD 4.10.2 by qiaodan at 20210511 start
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/inc
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
# TabA7 Lite code for USB-IF Ellisys TD 4.10.2 by qiaodan at 20210511 end
obj-$(CONFIG_USB_LIBCOMPOSITE)	+= libcomposite.o
libcomposite-y			:= usbstring.o config.o epautoconf.o
libcomposite-y			+= composite.o functions.o configfs.o u_f.o
libcomposite-$(CONFIG_USB_CONFIGFS_MTK_FASTMETA)	+= meta.o

obj-$(CONFIG_USB_GADGET)	+= udc/ function/ legacy/
