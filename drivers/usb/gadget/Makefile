# SPDX-License-Identifier: GPL-2.0
#
# USB peripheral controller drivers
#
subdir-ccflags-$(CONFIG_USB_GADGET_DEBUG)	:= -DDEBUG
subdir-ccflags-$(CONFIG_USB_GADGET_VERBOSE)	+= -DVERBOSE_DEBUG
ccflags-y				+= -I$(srctree)/drivers/usb/gadget/udc

#if defined(CONFIG_WT_PROJECT_S96902AA1) //usb if
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/inc
subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/typec/tcpc/inc
#endif /* CONFIG_WT_PROJECT_S96902AA1 */

obj-$(CONFIG_USB_LIBCOMPOSITE)	+= libcomposite.o
libcomposite-y			:= usbstring.o config.o epautoconf.o
libcomposite-y			+= composite.o functions.o configfs.o u_f.o
libcomposite-$(CONFIG_USB_CONFIGFS_MTK_FASTMETA)	+= meta.o

obj-$(CONFIG_USB_GADGET)	+= udc/ function/ legacy/
