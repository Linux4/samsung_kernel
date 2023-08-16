#
# Makefile for the Linux fat filesystem support.
#
ifeq ($(CONFIG_FAT_FS_VIRTUAL_XATTR),y)
EXTRA_CFLAGS += -DFAT_FS_VIRTUAL_XATTR
endif

obj-$(CONFIG_FAT_FS) += fat.o
obj-$(CONFIG_VFAT_FS) += vfat.o
obj-$(CONFIG_MSDOS_FS) += msdos.o

fat-y := cache.o dir.o fatent.o file.o inode.o misc.o nfs.o
fat-$(CONFIG_FAT_FS_VIRTUAL_XATTR) += xattr.o
vfat-y := namei_vfat.o
msdos-y := namei_msdos.o
