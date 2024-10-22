// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016 MediaTek Inc.
 * Author: Tiffany Lin <tiffany.lin@mediatek.com>
 */

#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <soc/mediatek/mmdvfs_v3.h>
#include <soc/mediatek/smi.h>

#include "mtk_vcodec_enc_pm.h"
#include "mtk_vcodec_enc_pm_plat.h"
#include "mtk_vcodec_util.h"
#include "mtk_vcu.h"
#include "venc_drv_if.h"

#define USE_GCE 0
#if ENC_DVFS
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include "vcodec_dvfs.h"
#define STD_VENC_FREQ 250000000
#define BW_FACTOR_DENOMINATOR 1000000
#define VENC_ADAPTIVE_OPRATE_INTERVAL 500 //ms
#define VENC_INIT_BOOST_INTERVAL 1500
#endif

#if ENC_EMI_BW
//#include <linux/interconnect-provider.h>
#include "mtk-interconnect.h"
#include "vcodec_bw.h"
#include "mtk-smi-dbg.h"
#endif

//#define VENC_PRINT_DTS_INFO
int venc_smi_monitor_mode = 1; // 0: disable, 1: enable, 2: debug mode
int venc_max_mon_frm = 8;
module_param(venc_smi_monitor_mode, int, 0644);
module_param(venc_max_mon_frm, int, 0644);

static bool mtk_enc_tput_init(struct mtk_vcodec_dev *dev)
{
	const int tp_item_num = 6;
	const int cfg_item_num = 4;
	const int bw_item_num = 3;
	int i, larb_cnt, ret;
	struct platform_device *pdev;
	u32 nmin, nmax;
	s32 offset;

	pdev = dev->plat_dev;
	larb_cnt = 0;

	ret = of_property_read_s32(pdev->dev.of_node, "throughput-op-rate-thresh", &nmax);
	if (ret)
		mtk_v4l2_debug(0, "[VENC] Cannot get op rate thresh, default 0");

	dev->venc_dvfs_params.per_frame_adjust_op_rate = nmax;
	dev->venc_dvfs_params.per_frame_adjust = 1;

	ret = of_property_read_u32(pdev->dev.of_node, "throughput-min", &nmin);
	if (ret) {
		nmin = STD_VENC_FREQ;
		mtk_v4l2_debug(0, "[VENC] Cannot get min, default %u", nmin);
	}

	ret = of_property_read_u32(pdev->dev.of_node, "throughput-normal-max", &nmax);
	if (ret) {
		nmax = STD_VENC_FREQ;
		mtk_v4l2_debug(0, "[VENC] Cannot get normal max, default %u", nmax);
	}
	dev->venc_dvfs_params.codec_type = MTK_INST_ENCODER;
	dev->venc_dvfs_params.min_freq = nmin;
	dev->venc_dvfs_params.normal_max_freq = nmax;
	dev->venc_dvfs_params.allow_oc = 0;

	/* throughput */
	dev->venc_tput_cnt = of_property_count_u32_elems(pdev->dev.of_node,
				"throughput-table") / tp_item_num;

	if (!dev->venc_tput_cnt) {
		mtk_v4l2_debug(0, "[VENC] throughput table not exist");
		return false;
	}

	dev->venc_tput = vzalloc(sizeof(struct vcodec_perf) * dev->venc_tput_cnt);
	if (!dev->venc_tput) {
		/* mtk_v4l2_debug(0, "[VENC] vzalloc venc_tput table failed"); */
		return false;
	}

	ret = of_property_read_s32(pdev->dev.of_node, "throughput-config-offset", &offset);
	if (ret)
		mtk_v4l2_debug(0, "[VENC] Cannot get config-offset, default 0");

	for (i = 0; i < dev->venc_tput_cnt; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num, &dev->venc_tput[i].codec_fmt);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get codec_fmt");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num + 1, (u32 *)&dev->venc_tput[i].config);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get config");
			return false;
		}
		dev->venc_tput[i].config -= offset;

		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num + 2, &dev->venc_tput[i].cy_per_mb_1);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get cycle per mb 1");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num + 3, &dev->venc_tput[i].cy_per_mb_2);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get cycle per mb 2");
			return false;
		}
		dev->venc_tput[i].codec_type = 1;

		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num + 4, &dev->venc_tput[i].base_freq);
		mtk_v4l2_debug(0, "[VENC][tput] get base_freq: %d", dev->venc_tput[i].base_freq);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get base_freq");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "throughput-table",
				i * tp_item_num + 5, &dev->venc_tput[i].bw_factor);
		mtk_v4l2_debug(0, "[VENC][tput] get bw_factor: %d", dev->venc_tput[i].bw_factor);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get bw_factor");
			return false;
		}
	}

	/* config */
	dev->venc_cfg_cnt = of_property_count_u32_elems(pdev->dev.of_node,
				"config-table") / cfg_item_num;

	if (!dev->venc_cfg_cnt) {
		mtk_v4l2_debug(0, "[VENC] config table not exist");
		return false;
	}

	dev->venc_cfg = vzalloc(sizeof(struct vcodec_config) * dev->venc_cfg_cnt);
	if (!dev->venc_cfg) {
		/* mtk_v4l2_debug(0, "[VENC] vzalloc venc_cfg table failed"); */
		return false;
	}

	ret = of_property_read_s32(pdev->dev.of_node, "throughput-config-offset", &offset);
	if (ret)
		mtk_v4l2_debug(0, "[VENC] Cannot get config-offset, default 0");

	for (i = 0; i < dev->venc_cfg_cnt; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "config-table",
				i * cfg_item_num, &dev->venc_cfg[i].codec_fmt);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get cfg codec_fmt");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "config-table",
				i * cfg_item_num + 1, (u32 *)&dev->venc_cfg[i].mb_thresh);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get mb_thresh");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "config-table",
				i * cfg_item_num + 2, &dev->venc_cfg[i].config_1);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get config 1");
			return false;
		}
		dev->venc_cfg[i].config_1 -= offset;

		ret = of_property_read_u32_index(pdev->dev.of_node, "config-table",
				i * cfg_item_num + 3, &dev->venc_cfg[i].config_2);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get config 2");
			return false;
		}
		dev->venc_cfg[i].config_2 -= offset;
		dev->venc_cfg[i].codec_type = 1;
	}

	/* bw */
	dev->venc_larb_cnt = of_property_count_u32_elems(pdev->dev.of_node,
				"bandwidth-table") / bw_item_num;

	if (dev->venc_larb_cnt > MTK_VENC_LARB_NUM) {
		mtk_v4l2_debug(0, "[VENC] venc larb over limit %d > %d",
				dev->venc_larb_cnt, MTK_VENC_LARB_NUM);
		dev->venc_larb_cnt = MTK_VENC_LARB_NUM;
	}

	if (!dev->venc_larb_cnt) {
		mtk_v4l2_debug(0, "[VENC] bandwidth table not exist");
		return false;
	}

	dev->venc_larb_bw = vzalloc(sizeof(struct vcodec_larb_bw) * dev->venc_larb_cnt);
	if (!dev->venc_larb_bw) {
		/* mtk_v4l2_debug(0, "[VENC] vzalloc venc_larb_bw table failed"); */
		return false;
	}

	for (i = 0; i < dev->venc_larb_cnt; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "bandwidth-table",
				i * bw_item_num, (u32 *)&dev->venc_larb_bw[i].larb_id);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get bw port type");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "bandwidth-table",
				i * bw_item_num + 1, &dev->venc_larb_bw[i].larb_type);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get base bw");
			return false;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "bandwidth-table",
				i * bw_item_num + 2, &dev->venc_larb_bw[i].larb_base_bw);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get base bw");
			return false;
		}
	}

	mtk_venc_pmqos_monitor_init(dev);

#ifdef VENC_PRINT_DTS_INFO
	mtk_v4l2_debug(0, "[VENC] tput_cnt %d, cfg_cnt %d, port_cnt %d\n",
		dev->venc_tput_cnt, dev->venc_cfg_cnt, dev->venc_port_cnt);

	for (i = 0; i < dev->venc_tput_cnt; i++) {
		mtk_v4l2_debug(0, "[VENC] tput fmt %u, cfg %d, cy1 %u, cy2 %u",
			dev->venc_tput[i].codec_fmt,
			dev->venc_tput[i].config,
			dev->venc_tput[i].cy_per_mb_1,
			dev->venc_tput[i].cy_per_mb_2);
	}

	for (i = 0; i < dev->venc_cfg_cnt; i++) {
		mtk_v4l2_debug(0, "[VENC] config fmt %u, mb_thresh %u, cfg1 %d, cfg2 %d",
			dev->venc_cfg[i].codec_fmt,
			dev->venc_cfg[i].mb_thresh,
			dev->venc_cfg[i].config_1,
			dev->venc_cfg[i].config_2);
	}

	for (i = 0; i < dev->venc_port_cnt; i++) {
		mtk_v4l2_debug(0, "[VENC] port[%d] type %d, bw %u, larb %u", i,
			dev->venc_port_bw[i].port_type,
			dev->venc_port_bw[i].port_base_bw,
			dev->venc_port_bw[i].larb);
	}
#endif
	return true;
}

static void mtk_enc_tput_deinit(struct mtk_vcodec_dev *dev)
{
	if (dev->venc_tput) {
		vfree(dev->venc_tput);
		dev->venc_tput = 0;
	}

	if (dev->venc_cfg) {
		vfree(dev->venc_cfg);
		dev->venc_cfg = 0;
	}

	if (dev->venc_larb_bw) {
		vfree(dev->venc_larb_bw);
		dev->venc_larb_bw = 0;
	}
	mtk_venc_pmqos_monitor_deinit(dev);
}

void mtk_prepare_venc_dvfs(struct mtk_vcodec_dev *dev)
{
#if ENC_DVFS
	int ret;
	struct dev_pm_opp *opp = 0;
	unsigned long freq = 0;
	int i = 0, venc_req = 0, flag = 0;
	bool tput_ret;
	struct platform_device *pdev = 0;

	pdev = dev->plat_dev;
	INIT_LIST_HEAD(&dev->venc_dvfs_inst);

	ret = of_property_read_s32(pdev->dev.of_node, "venc-mmdvfs-in-vcp", &venc_req);
	if (ret)
		mtk_v4l2_debug(0, "[VENC] Faile get venc-mmdvfs-in-vcp, default %d", venc_req);
	dev->venc_dvfs_params.mmdvfs_in_vcp = venc_req;

	ret = of_property_read_s32(pdev->dev.of_node, "venc-mmdvfs-in-adaptive", &venc_req);
	if (ret)
		mtk_v4l2_debug(0, "[VENC] no need venc-mmdvfs-in-adaptive");
	dev->venc_dvfs_params.mmdvfs_in_adaptive = venc_req;

	ret = of_property_read_s32(pdev->dev.of_node, "venc-cpu-hint-mode", &flag);
	if (ret) {
		mtk_v4l2_debug(0, "[VENC] no need venc-cpu-hint-mode");
		dev->cpu_hint_mode = (1 << MTK_CPU_UNSUPPORT);
	} else
		dev->cpu_hint_mode = flag;


	ret = dev_pm_opp_of_add_table(&dev->plat_dev->dev);
	if (ret < 0) {
		dev->venc_reg = 0;
		mtk_v4l2_debug(0, "[VENC] Failed to get opp table (%d)", ret);
		return;
	}

	dev->venc_reg = devm_regulator_get_optional(&dev->plat_dev->dev,
						"mmdvfs-dvfsrc-vcore");
	if (IS_ERR_OR_NULL(dev->venc_reg)) {
		mtk_v4l2_debug(0, "[VENC] Failed to get regulator");
		dev->venc_reg = 0;
		dev->venc_mmdvfs_clk = devm_clk_get(&dev->plat_dev->dev, "mmdvfs_clk");
		if (IS_ERR_OR_NULL(dev->venc_mmdvfs_clk)) {
			mtk_v4l2_debug(0, "[VENC] Failed to mmdvfs_clk");
			dev->venc_mmdvfs_clk = 0;
		} else
			mtk_v4l2_debug(0, "[VENC] get venc_mmdvfs_clk successfully");
	} else {
		mtk_v4l2_debug(0, "[VENC] get regulator successfully");
	}

	dev->venc_freq_cnt = dev_pm_opp_get_opp_count(&dev->plat_dev->dev);
	freq = 0;
	while (!IS_ERR(opp =
		dev_pm_opp_find_freq_ceil(&dev->plat_dev->dev, &freq))) {
		dev->venc_freqs[i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}

	tput_ret = mtk_enc_tput_init(dev);
#endif
}

void mtk_unprepare_venc_dvfs(struct mtk_vcodec_dev *dev)
{
#if ENC_DVFS
	mtk_enc_tput_deinit(dev);
#endif
}

void mtk_prepare_venc_emi_bw(struct mtk_vcodec_dev *dev)
{
#if ENC_EMI_BW
	int i, ret;
	struct platform_device *pdev = 0;
	u32 larb_num = 0;
	const char *path_strs[MTK_VENC_LARB_NUM];

	pdev = dev->plat_dev;
	for (i = 0; i < MTK_VENC_LARB_NUM; i++)
		dev->venc_qos_req[i] = 0;

	ret = of_property_read_u32(pdev->dev.of_node, "interconnect-num", &larb_num);
	if (ret) {
		mtk_v4l2_debug(0, "[VENC] Cannot get interconnect num, skip");
		return;
	}

	ret = of_property_read_string_array(pdev->dev.of_node, "interconnect-names",
		path_strs, larb_num);

	if (ret < 0) {
		mtk_v4l2_debug(0, "[VENC] Cannot get interconnect names, skip");
		return;
	} else if (ret != (int)larb_num) {
		mtk_v4l2_debug(0, "[VENC] Interconnect name count not match %u %d", larb_num, ret);
	}

	if (larb_num > MTK_VENC_LARB_NUM) {
		mtk_v4l2_debug(0, "[VENC] venc larb over limit %u > %d",
				larb_num, MTK_VENC_LARB_NUM);
		larb_num = MTK_VENC_LARB_NUM;
	}

	for (i = 0; i < larb_num; i++) {
		dev->venc_qos_req[i] = of_mtk_icc_get(&pdev->dev, path_strs[i]);
		mtk_v4l2_debug(0, "[VENC] %d %p %s", i, dev->venc_qos_req[i], path_strs[i]);
	}
#endif
}

void mtk_unprepare_venc_emi_bw(struct mtk_vcodec_dev *dev)
{
#if ENC_EMI_BW
#endif
}

void set_venc_opp(struct mtk_vcodec_dev *dev, u32 freq)
{
	struct dev_pm_opp *opp = 0;
	int volt = 0;
	int ret = 0;
	unsigned long freq_64 = (unsigned long)freq;

	if (dev->venc_reg || dev->venc_mmdvfs_clk) {
		opp = dev_pm_opp_find_freq_ceil(&dev->plat_dev->dev, &freq_64);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);

		if (dev->venc_mmdvfs_clk) {
			mtk_mmdvfs_enable_vcp(true, VCP_PWR_USR_VENC);
			ret = clk_set_rate(dev->venc_mmdvfs_clk, freq_64);
			if (ret) {
				mtk_v4l2_err("[VENC] Failed to set mmdvfs rate %lu\n",
						freq_64);
			}
			mtk_mmdvfs_enable_vcp(false, VCP_PWR_USR_VENC);
			mtk_v4l2_debug(8, "[VENC] freq %u, find_freq %lu", freq, freq_64);
		} else if (dev->venc_reg) {
			ret = regulator_set_voltage(dev->venc_reg, volt, INT_MAX);
			if (ret) {
				mtk_v4l2_err("[VENC] Failed to set regulator voltage %d\n",
						volt);
			}
			mtk_v4l2_debug(8, "[VENC] freq %u, voltage %d", freq, volt);
		}
	}
}

/*prepare mmdvfs data to vcp to begin*/
void mtk_venc_prepare_vcp_dvfs_data(struct mtk_vcodec_ctx *ctx, struct venc_enc_param *param)
{
	param->venc_dvfs_state = MTK_INST_START;
	ctx->last_monitor_op = 0; // for monitor op rate
	ctx->op_rate_adaptive = ctx->enc_params.operationrate; // for monitor op rate
}

/*prepare mmdvfs data to vcp to begin*/
void mtk_venc_unprepare_vcp_dvfs_data(struct mtk_vcodec_ctx *ctx, struct venc_enc_param *param)
{
	param->venc_dvfs_state = MTK_INST_END;
}

void mtk_venc_dvfs_reset_vsi_data(struct mtk_vcodec_dev *dev)
{
	dev->venc_dvfs_params.target_freq = 0;
	dev->venc_dvfs_params.target_bw_factor = 0;
}

void mtk_venc_dvfs_sync_vsi_data(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_vcodec_dev *dev = ctx->dev;
	struct venc_inst *inst = (struct venc_inst *) ctx->drv_handle;

	if (mtk_vcodec_is_state(ctx, MTK_STATE_ABORT))
		return;

	dev->venc_dvfs_params.target_freq = inst->vsi->config.target_freq;
	dev->venc_dvfs_params.target_bw_factor = inst->vsi->config.target_bw_factor;
	mtk_vcodec_cpu_adaptive_ctrl(ctx, inst->vsi->config.cpu_hint);
}

void mtk_venc_dvfs_begin_inst(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_vcodec_dev *dev = ctx->dev;

	mtk_v4l2_debug(8, "[VENC] ctx = %p",  ctx);

	if (need_update(ctx)) {
		update_freq(dev, MTK_INST_ENCODER);
		mtk_v4l2_debug(4, "[VENC] freq %u", dev->venc_dvfs_params.target_freq);
		if (dev->venc_dvfs_params.mmdvfs_in_adaptive) {
			dev->venc_dvfs_params.last_boost_time = jiffies_to_msecs(jiffies);
			dev->venc_dvfs_params.init_boost = 1;
			set_venc_opp(dev, dev->venc_dvfs_params.normal_max_freq); // boost at beginning
		} else
			set_venc_opp(dev, dev->venc_dvfs_params.target_freq);
	}
}

void mtk_venc_dvfs_end_inst(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_vcodec_dev *dev = ctx->dev;

	mtk_v4l2_debug(8, "[VENC] ctx = %p",  ctx);

	if (remove_update(ctx)) {
		update_freq(dev, MTK_INST_ENCODER);
		mtk_v4l2_debug(4, "[VENC] freq %u", dev->venc_dvfs_params.target_freq);
		set_venc_opp(dev, dev->venc_dvfs_params.target_freq);
	}
}

void mtk_venc_init_boost(struct mtk_vcodec_ctx *ctx)
{
	ctx->dev->venc_dvfs_params.last_boost_time = jiffies_to_msecs(jiffies);
	ctx->dev->venc_dvfs_params.init_boost = 1;
}

void mtk_venc_dvfs_check_boost(struct mtk_vcodec_dev *dev)
{
	unsigned int cur_in_timestamp;

	if (!dev->venc_dvfs_params.mmdvfs_in_adaptive || !dev->venc_dvfs_params.init_boost)
		return;

	cur_in_timestamp = jiffies_to_msecs(jiffies);
	mtk_v4l2_debug(8, "[VDVFS] cur_time:%u, last_boost_time:%u",
		cur_in_timestamp, dev->venc_dvfs_params.last_boost_time);

	if (cur_in_timestamp - dev->venc_dvfs_params.last_boost_time >=
		VENC_INIT_BOOST_INTERVAL && dev->venc_dvfs_params.init_boost) {
		mutex_lock(&dev->enc_dvfs_mutex);
		dev->venc_dvfs_params.init_boost = 0;
		set_venc_opp(dev, dev->venc_dvfs_params.target_freq);
		mtk_v4l2_debug(0, "[VDVFS][VENC] stop boost, set freq %u", dev->venc_dvfs_params.target_freq);
		mutex_unlock(&dev->enc_dvfs_mutex);
	}
}

void mtk_venc_pmqos_begin_inst(struct mtk_vcodec_ctx *ctx)
{
	int i;
	struct mtk_vcodec_dev *dev = 0;
	u32 target_bw = 0;

	dev = ctx->dev;
	mtk_v4l2_debug(4, "[VENC] ctx:%p", ctx);

	for (i = 0; i < dev->venc_larb_cnt; i++) {
		target_bw = (u64) dev->venc_larb_bw[i].larb_base_bw
			* dev->venc_dvfs_params.target_bw_factor / BW_FACTOR_DENOMINATOR;
		if (dev->venc_larb_bw[i].larb_type < VCODEC_LARB_SUM) {
			mtk_icc_set_bw(dev->venc_qos_req[i],
					MBps_to_icc((u32)target_bw), 0);
			mtk_v4l2_debug(8, "[VENC] set larb%d: %dMB/s",
				dev->venc_larb_bw[i].larb_id, target_bw);
		} else {
			mtk_v4l2_debug(8, "[VENC] unknown port type %d\n",
					dev->venc_larb_bw[i].larb_type);
		}
		dev->venc_qos.prev_comm_bw[i] = target_bw;
	}
}

void mtk_venc_pmqos_end_inst(struct mtk_vcodec_ctx *ctx)
{
	int i;
	struct mtk_vcodec_dev *dev = 0;
	u32 target_bw = 0;

	dev = ctx->dev;

	for (i = 0; i < dev->venc_larb_cnt; i++) {
		target_bw = (u64) dev->venc_larb_bw[i].larb_base_bw
			* dev->venc_dvfs_params.target_bw_factor / BW_FACTOR_DENOMINATOR;

		if (list_empty(&dev->venc_dvfs_inst)) /* no more instances */
			target_bw = 0;

		if (dev->venc_larb_bw[i].larb_type < VCODEC_LARB_SUM) {
			mtk_icc_set_bw(dev->venc_qos_req[i],
					MBps_to_icc((u32)target_bw), 0);
			mtk_v4l2_debug(8, "[VENC] set larb %d: %dMB/s",
					dev->venc_larb_bw[i].larb_id, target_bw);
		} else {
			mtk_v4l2_debug(8, "[VENC] unknown port type %d\n",
					dev->venc_larb_bw[i].larb_type);
		}
	}
}

void mtk_venc_pmqos_lock_unlock(struct mtk_vcodec_dev *dev, bool is_lock)
{
	if (is_lock) {
		if (dev->venc_dvfs_params.mmdvfs_in_vcp)
			mutex_lock(&dev->enc_qos_mutex);
		else
			mutex_lock(&dev->enc_dvfs_mutex);
	} else {
		if (dev->venc_dvfs_params.mmdvfs_in_vcp)
			mutex_unlock(&dev->enc_qos_mutex);
		else
			mutex_unlock(&dev->enc_dvfs_mutex);
	}
}


void mtk_venc_pmqos_monitor(struct mtk_vcodec_dev *dev, u32 state)
{
	struct vcodec_dev_qos *qos = &dev->venc_qos;

	u32 data_comm0[MTK_SMI_MAX_MON_REQ] = {0};

	if (unlikely(!qos->need_smi_monitor || !venc_smi_monitor_mode))
		return;

	switch (state) {
	case VCODEC_SMI_MONITOR_START:
		mtk_v4l2_debug(8, "[VQOS] start to monitor BW...\n");
		smi_monitor_start(qos->dev, qos->common_id[SMI_COMMON_ID_0],
			qos->commlarb_id[SMI_COMMON_ID_0], qos->rw_flag,
			qos->monitor_id[SMI_COMMON_ID_0]);
		break;
	case VCODEC_SMI_MONITOR_STOP:
		smi_monitor_stop(qos->dev, qos->common_id[SMI_COMMON_ID_0],
			data_comm0, qos->monitor_id[SMI_COMMON_ID_0]);

		qos->data_total[SMI_COMMON_ID_0][SMI_MON_READ] += data_comm0[0]; // MB
		qos->data_total[SMI_COMMON_ID_0][SMI_MON_WRITE] += data_comm0[1];

		mtk_v4l2_debug(8, "[VQOS] frm_cnt %d: Acquire: (%d, %d)\n",
			qos->monitor_ring_frame_cnt, data_comm0[0], data_comm0[1]);
		mtk_v4l2_debug(8, "[VQOS] Total bytes: (%llu, %llu)\n",
			qos->data_total[SMI_COMMON_ID_0][SMI_MON_READ],
			qos->data_total[SMI_COMMON_ID_0][SMI_MON_WRITE]);

		if (++qos->monitor_ring_frame_cnt >= qos->max_mon_frm_cnt)
			qos->apply_monitor_config = true;

		mtk_v4l2_debug(8, "[VQOS] stop to monitor BW: frm_cnt: %d...\n",
			qos->monitor_ring_frame_cnt);
		break;
	default:
		mtk_v4l2_debug(0, "[VQOS] unknown smi monitor state...\n");
		break;
	}
};

void mtk_venc_pmqos_monitor_init(struct mtk_vcodec_dev *dev)
{
	struct vcodec_dev_qos *qos = &dev->venc_qos;
	struct platform_device *pdev;
	int need_smi_monitor = 0;
	int commlarb_id = 0;
	int i, j, ret, smi_common_num = 1;

	pdev = dev->plat_dev;

	ret = of_property_read_s32(pdev->dev.of_node, "need-smi-monitor", &need_smi_monitor);
	if (ret) {
		mtk_v4l2_debug(0, "[VENC] Cannot smi monitor flag, default 0");
		return;
	}

	mtk_v4l2_debug(0, "[VQOS] init pmqos monitor, %d", need_smi_monitor);

	memset((void *)qos, 0, sizeof(struct vcodec_dev_qos));
	qos->dev = dev->v4l2_dev.dev;
	qos->monitor_ring_frame_cnt = 0;
	qos->apply_monitor_config = false;
	qos->need_smi_monitor = need_smi_monitor;

	for (i = 0; i < smi_common_num; i++) {
		ret = of_property_read_u32_index(pdev->dev.of_node, "common-id",
			i, &qos->common_id[i]);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get common_id: %d, default 0", i);
			break;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "monitor-id",
			i, &qos->monitor_id[i]);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get monitor_id: %d, default 0", i);
			break;
		}

		ret = of_property_read_u32_index(pdev->dev.of_node, "commlarb-id",
				i, &commlarb_id);
		if (ret) {
			mtk_v4l2_debug(0, "[VENC] Cannot get commlarb_id: %d, default 0", i);
			break;
		}
		for (j = 0; j < MTK_SMI_MAX_MON_REQ; j++)
			qos->commlarb_id[i][j] = commlarb_id;

		mtk_v4l2_debug(0, "[VQOS] comm: %d, monitor_id: %d, commlarb_id: (%d, %d, %d, %d)",
			qos->common_id[i], qos->monitor_id[i],
			qos->commlarb_id[i][0], qos->commlarb_id[i][1],
			qos->commlarb_id[i][2], qos->commlarb_id[i][3]);
	}
	//  1 for read, 2 for write
	for (i = 0; i < MTK_SMI_MAX_MON_REQ; i += 2) {
		qos->rw_flag[i] = SMI_MON_READ + 1;
		qos->rw_flag[i+1] = SMI_MON_WRITE + 1;
		mtk_v4l2_debug(0, "[VQOS] rw_flag %d (%d, %d)",
			i, qos->rw_flag[i], qos->rw_flag[i+1]);
	}
};

void mtk_venc_pmqos_monitor_deinit(struct mtk_vcodec_dev *dev)
{
	struct vcodec_dev_qos *qos = &dev->venc_qos;

	mtk_v4l2_debug(0, "[VQOS] deinit pmqos monitor\n");
	qos->monitor_ring_frame_cnt = 0;
	qos->apply_monitor_config = false;
};

void mtk_venc_pmqos_monitor_reset(struct mtk_vcodec_dev *dev)
{
	struct vcodec_dev_qos *qos = &dev->venc_qos;

	if (unlikely(!qos->need_smi_monitor || !venc_smi_monitor_mode)) {
		mtk_v4l2_debug(0, "[VQOS] no smi monitor:(%d, %d)",
			qos->need_smi_monitor, venc_smi_monitor_mode);
		return;
	}
	mtk_v4l2_debug(0, "[VQOS] smi monitor is enable (%d, %d), reset config",
		qos->need_smi_monitor, venc_smi_monitor_mode);

	qos->max_mon_frm_cnt = (venc_smi_monitor_mode > 1) ? venc_max_mon_frm : MTK_SMI_MAX_MON_FRM;
	qos->monitor_ring_frame_cnt = 0;
	qos->apply_monitor_config = false;
	memset(qos->data_total, 0,
			sizeof(unsigned long long) * SMI_COMMON_NUM * MTK_VCODEC_QOS_TYPE);
};

static void mtk_venc_pmqos_monitor_debugger(struct mtk_vcodec_dev *dev, u32 *cur_common_bw)
{
	int i;

	for (i = 0; i < dev->venc_larb_cnt; i++) {
		if ((dev->venc_qos.prev_comm_bw[i] * 3 / 2) < cur_common_bw[i])
			mtk_v4l2_debug(0, "[VQOS] BW increase 1.5 times, smi monitor may fail");
		dev->venc_qos.prev_comm_bw[i] = cur_common_bw[i];
	}
}

void mtk_venc_pmqos_frame_req(struct mtk_vcodec_ctx *ctx)
{
	struct mtk_vcodec_dev *dev = ctx->dev;
	struct vcodec_dev_qos *qos = &dev->venc_qos;
	u32 common_bw[MTK_SMI_MAX_MON_REQ] = {0};
	u32 cur_fps = dev->venc_dvfs_params.oprate_sum;
	u32 i, smi_common_num = 1;

	if (qos->need_smi_monitor) {

		if (!qos->apply_monitor_config)
			return;

		common_bw[0] = (u32)((((qos->data_total[SMI_COMMON_ID_0][SMI_MON_READ] * cur_fps
			/ qos->max_mon_frm_cnt) >> 2) * 5) >> 20);
		common_bw[1] = (u32)((((qos->data_total[SMI_COMMON_ID_0][SMI_MON_WRITE] * cur_fps
			/ qos->max_mon_frm_cnt) >> 2) * 5) >> 20);

		qos->monitor_ring_frame_cnt = 0;
		qos->apply_monitor_config = false;
		memset(qos->data_total, 0,
			sizeof(unsigned long long) * smi_common_num * MTK_VCODEC_QOS_TYPE);
		mtk_venc_pmqos_monitor_debugger(dev, common_bw);
	} else
		return;

	for (i = 0; i < dev->venc_larb_cnt; i++) {
		mtk_icc_set_bw(dev->venc_qos_req[i], MBps_to_icc(common_bw[i]), 0);
		mtk_v4l2_debug(8, "[VQOS] set larb%d: %dMB/s",
			dev->venc_larb_bw[i].larb_id, common_bw[i]);
	}
}

/*
 *	Function name: mtk_venc_dvfs_monitor_op_rate
 *	Description: This function updates the op rate of ctx by monitoring input buffer queued.
 *			1. Montior interval: 500 ms
 *			2. Bypass the first interval
 *			3. The monitored rate needs to be stable (<20% compares to prev interval)
 *			4. Diff > 20% than current used op rate
 *	Returns: Boolean, the op rate needs to be updated
 */
bool mtk_venc_dvfs_monitor_op_rate(struct mtk_vcodec_ctx *ctx, int buf_type)
{
	unsigned int cur_in_timestamp, time_diff, threshold = 20;
	unsigned int prev_op, cur_op, tmp_op; /* monitored op in the prev interval */
	bool update_op = false;
	struct vcodec_inst *inst = 0;
	struct mtk_vcodec_dev *dev = ctx->dev;

	if (buf_type != V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ||
		!dev->venc_dvfs_params.mmdvfs_in_adaptive)
		return false;

	prev_op = ctx->last_monitor_op;
	cur_in_timestamp = jiffies_to_msecs(jiffies);
	if (ctx->prev_inbuf_time == 0) {
		ctx->prev_inbuf_time = cur_in_timestamp;
		return false;
	}

	time_diff = cur_in_timestamp - ctx->prev_inbuf_time;
	ctx->input_buf_cnt++;

	if (time_diff > VENC_ADAPTIVE_OPRATE_INTERVAL) {
		ctx->last_monitor_op =
			ctx->input_buf_cnt * 1000 / time_diff;
		ctx->prev_inbuf_time = cur_in_timestamp;
		ctx->input_buf_cnt = 0;
		cur_op = ctx->op_rate_adaptive;

		mtk_v4l2_debug(4, "[VDVFS][VENC][ADAPTIVE][%d] prev_op: %d, moni_op: %d, cur_adp_op: %d",
			ctx->id, prev_op, ctx->last_monitor_op, cur_op);

		if (prev_op <= 0)
			return false;

		tmp_op = MAX(ctx->last_monitor_op, prev_op);

		update_op = mtk_dvfs_check_op_diff(prev_op, ctx->last_monitor_op, threshold, 1) &&
			mtk_dvfs_check_op_diff(cur_op, tmp_op, threshold, -1);

		update_op |= (dev->venc_dvfs_params.init_boost == 1) && (prev_op > 0) && (ctx->last_monitor_op > 0);

		if (update_op) {
			mutex_lock(&dev->enc_dvfs_mutex);
			ctx->op_rate_adaptive = tmp_op;
			mtk_v4l2_debug(0, "[VDVFS][VENC][ADAPTIVE][%d] op: user:%d, adaptive:%d->%d",
				ctx->id, ctx->enc_params.operationrate, cur_op, tmp_op);

			ctx->enc_params.operationrate_adaptive = ctx->op_rate_adaptive;
			// update venc freq in kernel
			if(!dev->venc_dvfs_params.mmdvfs_in_vcp) {
				inst = get_inst(ctx);
				if (inst) {
					if(need_update(ctx)) {
						update_freq(dev, MTK_INST_ENCODER);
						mtk_v4l2_debug(0, "[VDVFS][VENC][ADAPTIVE][%d] set freq %u",
							ctx->id, dev->venc_dvfs_params.target_freq);
						set_venc_opp(dev, dev->venc_dvfs_params.target_freq);
						dev->venc_dvfs_params.init_boost = 0;
					}
				}
			} else
				dev->venc_dvfs_params.init_boost = 0;

			mutex_unlock(&dev->enc_dvfs_mutex);
			return true;
		}
	}
	return false;
}




