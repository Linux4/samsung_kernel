// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2023 MediaTek Inc.
 * Authors:
 *	Po-Wen Kao <powen.kao@mediatek.com>
 */
#include "ufs-mediatek-ise.h"

int ufs_mtk_ise_rpmb_probe(struct ufs_hba *hba)
{
	dev_info(hba->dev, "dummy iSE support.");
	return 0;
}
