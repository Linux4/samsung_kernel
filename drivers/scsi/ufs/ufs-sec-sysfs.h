// SPDX-License-Identifier: GPL-2.0
/*
 * Samsung Specific feature : sysfs-nodes
 *
 * Copyright (C) 2021 Samsung Electronics Co., Ltd.
 *
 * Authors:
 *	Storage Driver <storage.sec@samsung.com>
 */

#ifndef __UFS_SEC_SYSFS_H__
#define __UFS_SEC_SYSFS_H__

#include <linux/sec_class.h>
#include <linux/sec_debug.h>

#include "ufs-sec-feature.h"

extern struct ufs_sec_err_info ufs_err_info;
extern struct ufs_sec_err_info ufs_err_info_backup;

#define SEC_UFS_ERR_INFO_BACKUP(err_count, member) ({                                     \
		ufs_err_info_backup.err_count.member += ufs_err_info.err_count.member;     \
		ufs_err_info.err_count.member = 0; })

#define SEC_UFS_ERR_INFO_GET_VALUE(err_count, member)                                     \
	(ufs_err_info_backup.err_count.member + ufs_err_info.err_count.member)

#define SEC_UFS_DATA_ATTR_RO(name, fmt, args...)                                              \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, char *buf)      \
{                                                                                             \
	return sprintf(buf, fmt, args);                                                       \
}                                                                                             \
static DEVICE_ATTR_RO(name)

/* store function has to be defined */
#define SEC_UFS_DATA_ATTR_RW(name, fmt, args...)                                              \
static ssize_t name##_show(struct device *dev, struct device_attribute *attr, char *buf)      \
{                                                                                             \
	return sprintf(buf, fmt, args);                                                       \
}                                                                                             \
static DEVICE_ATTR(name, 0664, name##_show, name##_store)

#define SEC_UFS_ERR_COUNT_INC(count, max) ((count) += ((count) < (max)) ? 1 : 0)

void ufs_sec_add_sysfs_nodes(struct ufs_hba *hba);
void ufs_sec_remove_sysfs_nodes(struct ufs_hba *hba);

#endif
