# SPDX-License-Identifier: GPL-2.0

obj-$(CONFIG_F2FS_FS) += f2fs.o
GCOV_PROFILE_f2fs.o := $(CONFIG_SPRD_FS_GCOV)

f2fs-y		:= dir.o file.o inode.o namei.o hash.o super.o inline.o
f2fs-y		+= checkpoint.o gc.o data.o node.o segment.o recovery.o
f2fs-y		+= shrinker.o extent_cache.o sysfs.o
GCOV_PROFILE_dir.o := $(CONFIG_SPRD_FS_GCOV)
GCOV_PROFILE_file.o := $(CONFIG_SPRD_FS_GCOV)
GCOV_PROFILE_inode.o := $(CONFIG_SPRD_FS_GCOV)

f2fs-$(CONFIG_F2FS_STAT_FS) += debug.o
f2fs-$(CONFIG_F2FS_FS_XATTR) += xattr.o
f2fs-$(CONFIG_F2FS_FS_POSIX_ACL) += acl.o
f2fs-$(CONFIG_F2FS_IO_TRACE) += trace.o
f2fs-$(CONFIG_FS_VERITY) += verity.o
f2fs-$(CONFIG_F2FS_FS_COMPRESSION) += compress.o
