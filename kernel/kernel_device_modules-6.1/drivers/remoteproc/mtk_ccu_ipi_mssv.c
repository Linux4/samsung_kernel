// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2023 MediaTek Inc.

#include <linux/clocksource.h>
#include <linux/remoteproc/mtk_ccu.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/module.h>

#include "mtk_ccu_common_mssv.h"
#include "mtk_ccu_isp71_mssv.h"
#if defined(SECURE_CCU)
#include <linux/soc/mediatek/mtk_sip_svc.h>
#include <linux/arm-smccc.h>
#endif

int mtk_ccu_rproc_ipc_send(struct platform_device *pdev,
	enum mtk_ccu_feature_type featureType,
	uint32_t msgId, void *inDataPtr, uint32_t inDataSize)
{
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_ccu_rproc_ipc_send);

MODULE_DESCRIPTION("MTK CCU Rproc Driver");
MODULE_LICENSE("GPL");
