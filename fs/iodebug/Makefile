#
# Makefile for the SPRD IO debug function.
#

obj-$(CONFIG_SPRD_IODEBUG) += iodebug.o

iodebug-objs += iodebug_common.o
iodebug-$(CONFIG_SPRD_IODEBUG_HOTPOINT)	+= iodebug_hotpoint.o
iodebug-$(CONFIG_SPRD_IODEBUG_VFS)	+= iodebug_vfs.o
iodebug-$(CONFIG_SPRD_IODEBUG_BDI)	+= iodebug_bdi.o

