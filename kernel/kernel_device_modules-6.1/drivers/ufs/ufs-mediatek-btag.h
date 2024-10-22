/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2023 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_BTAG_H
#define _UFS_MEDIATEK_BTAG_H

#if IS_ENABLED(CONFIG_MTK_BLOCK_IO_TRACER)

void ufs_mtk_btag_init(struct ufs_hba *hba);
void ufs_mtk_btag_exit(struct ufs_hba *hba);
void ufs_mtk_btag_compl_command(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);
void ufs_mtk_btag_send_command(struct ufs_hba *hba, struct ufshcd_lrb *lrbp);

#else

#define ufs_mtk_btag_init(...)
#define ufs_mtk_btag_exit(...)
#define ufs_mtk_btag_compl_command(...)
#define ufs_mtk_btag_send_command(...)

#endif /* CONFIG_MTK_BLOCK_IO_TRACER */

#endif /* _UFS_MEDIATEK_BTAG_H */

