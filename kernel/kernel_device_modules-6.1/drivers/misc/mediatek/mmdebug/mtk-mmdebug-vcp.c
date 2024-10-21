// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023 MediaTek Inc.
 * Author: mtk21306 <cindy-hy.chen@mediatek.com>
 */

#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <soc/mediatek/smi.h>

#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
#include <aee.h>
#endif

#include "vcp_helper.h"
#include "vcp_reg.h"
#include "vcp_status.h"

#include "mtk-mmdebug-vcp.h"
#include "mtk-mmdvfs-debug.h"

static int vcp_power;
static DEFINE_MUTEX(mmdebug_vcp_pwr_mutex);
static struct mmdebug_ipi_data mmdebug_vcp_ipi_data;

static int mtk_mmdebug_enable_vcp(const bool enable)
{
	int ret = 0;

	if (is_vcp_suspending_ex())
		return -EBUSY;

	mutex_lock(&mmdebug_vcp_pwr_mutex);
	if (enable) {
		if (!vcp_power) {
			ret = vcp_register_feature_ex(MMDEBUG_FEATURE_ID);
			if (ret)
				goto enable_vcp_end;
		}
		vcp_power += 1;
	} else {
		if (!vcp_power) {
			ret = -EINVAL;
			goto enable_vcp_end;
		}
		if (vcp_power == 1) {
			ret = vcp_deregister_feature_ex(MMDEBUG_FEATURE_ID);
			if (ret)
				goto enable_vcp_end;
		}
		vcp_power -= 1;
	}

enable_vcp_end:
	if (ret)
		MMDEBUG_ERR("ret:%d enable:%d vcp_power:%d",
			ret, enable, vcp_power);
	mutex_unlock(&mmdebug_vcp_pwr_mutex);
	return ret;
}

static int mmdebug_vcp_ipi_cb(unsigned int ipi_id, void *prdata, void *data,
	unsigned int len) // vcp > ap
{
	struct mmdebug_ipi_data slot;

	if (ipi_id != IPI_IN_MMDEBUG || !data)
		return 0;

	slot = *(struct mmdebug_ipi_data *)data;

	switch (slot.func) {
	case MMDEBUG_FUNC_SMI_DUMP:
		mtk_smi_dbg_hang_detect("SMI VCP DRIVER");
		break;
	case MMDEBUG_FUNC_KERNEL_WARN:
		if (slot.idx >= ARRAY_SIZE(kernel_warn_type_str)) {
			MMDEBUG_ERR("MMDEBUG kernel warning invalid idx:%hhu ack:%hhu base:%hhu",
				slot.idx, slot.ack, slot.base);
			break;
		}
#if IS_ENABLED(CONFIG_MTK_AEE_FEATURE)
		aee_kernel_warning(kernel_warn_type_str[slot.idx],
			"kernel warning idx:%hhu ack:%hhu base:%hhu",
			slot.idx, slot.ack, slot.base);
#endif
		MMDEBUG_ERR("MMDEBUG kernel warning str:%s idx:%hhu ack:%hhu base:%hhu",
			kernel_warn_type_str[slot.idx], slot.idx, slot.ack, slot.base);
		mmdvfs_debug_status_dump(NULL);
		break;
	default:
		MMDEBUG_ERR("ipi_id:%u func:%hhu idx:%hhu ack:%hhu base:%hhu",
			ipi_id, slot.func, slot.idx, slot.ack, slot.base);
		break;
	}

	return 0;
}

static int mmdebug_vcp_init_thread(void *data)
{
	static struct mtk_ipi_device *vcp_ipi_dev;
	int retry = 0, ret = 0;

	while (mtk_mmdebug_enable_vcp(true)) {
		if (++retry > 100) {
			MMDEBUG_ERR("vcp is not powered on yet");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	retry = 0;
	while (!is_vcp_ready_ex(VCP_A_ID)) {
		if (++retry > VCP_SYNC_TIMEOUT_MS) {
			MMDEBUG_ERR("VCP_A_ID:%d not ready", VCP_A_ID);
			return -ETIMEDOUT;
		}
		mdelay(1);
	}

	retry = 0;
	while (!(vcp_ipi_dev = vcp_get_ipidev())) {
		if (++retry > 100) {
			MMDEBUG_ERR("cannot get vcp ipidev");
			return -ETIMEDOUT;
		}
		ssleep(1);
	}

	ret = mtk_ipi_register(vcp_ipi_dev, IPI_IN_MMDEBUG,
		mmdebug_vcp_ipi_cb, NULL, &mmdebug_vcp_ipi_data);
	if (ret) {
		MMDEBUG_ERR("mtk_ipi_register failed:%d ipi_id:%d", ret, IPI_IN_MMDEBUG);
		return ret;
	}

	mtk_mmdebug_enable_vcp(false);

	return 0;
}

static int mmdebug_vcp_probe(struct platform_device *pdev)
{
	struct task_struct *kthr_vcp;

	kthr_vcp = kthread_run(mmdebug_vcp_init_thread, NULL, "mmdebug-vcp");
	MMDEBUG_DBG("probe success");
	return 0;
}


static const struct of_device_id of_mmdebug_vcp_match_tbl[] = {
	{
		.compatible = "mediatek,mmdebug-vcp",
	},
	{}
};

static struct platform_driver mmdebug_vcp_drv = {
	.probe = mmdebug_vcp_probe,
	.driver = {
		.name = "mtk-mmdebug-vcp",
		.of_match_table = of_mmdebug_vcp_match_tbl,
	},
};

static int __init mmdebug_vcp_init(void)
{
	int ret;

	ret = platform_driver_register(&mmdebug_vcp_drv);
	if (ret)
		MMDEBUG_DBG("failed:%d", ret);

	return ret;
}

static void __exit mmdebug_vcp_exit(void)
{
	platform_driver_unregister(&mmdebug_vcp_drv);
}

module_init(mmdebug_vcp_init);
module_exit(mmdebug_vcp_exit);
MODULE_DESCRIPTION("MMDEBUG vcp Driver");
MODULE_AUTHOR("mtk21306 <cindy-hy.chen@mediatek.com>");
MODULE_LICENSE("GPL");

