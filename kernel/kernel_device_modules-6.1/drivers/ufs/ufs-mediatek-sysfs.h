/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_SYSFS_H
#define _UFS_MEDIATEK_SYSFS_H

void ufs_mtk_init_ioctl(struct ufs_hba *hba);
void ufs_mtk_init_sysfs(struct ufs_hba *hba);
void ufs_mtk_remove_sysfs(struct ufs_hba *hba);

#endif /* _UFS_MEDIATEK_SYSFS_H */

