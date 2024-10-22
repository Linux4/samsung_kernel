// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mtk_layering_rule.h"
#include "mtk_drm_crtc.h"
#include "mtk_disp_pmqos.h"
#include "mtk_drm_mmp.h"
#include "mtk_drm_drv.h"
#include "mtk_dump.h"

#include "mtk_disp_vidle.h"

#include <linux/clk.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>

#include <dt-bindings/interconnect/mtk,mmqos.h>
#include <soc/mediatek/mmqos.h>
#include <soc/mediatek/mmdvfs_v3.h>

#define CRTC_NUM		4
static struct drm_crtc *dev_crtc;
/* add for mm qos */
static u8 vdisp_opp = U8_MAX;
static struct regulator *mm_freq_request;
static unsigned long *g_freq_steps;
static unsigned int lp_freq;
static int g_freq_level[CRTC_NUM] = {-1, -1, -1, -1};
static bool g_freq_lp[CRTC_NUM] = {false, false, false, false};
static long g_freq;
static int step_size = 1;

void mtk_disp_pmqos_get_icc_path_name(char *buf, int buf_len,
				struct mtk_ddp_comp *comp, char *qos_event)
{
	int len;

	/* mtk_dump_comp_str return shorter comp name, add prefix to match icc name in dts */
	len = snprintf(buf, buf_len, "DDP_COMPONENT_%s_%s", mtk_dump_comp_str(comp), qos_event);
	if (!(len > -1 && len < buf_len))
		DDPINFO("%s: snprintf return error", __func__);
}

int __mtk_disp_pmqos_slot_look_up(int comp_id, int mode)
{
	switch (comp_id) {
	case DDP_COMPONENT_OVL0:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL0_FBDC_BW;
		else
			return DISP_PMQOS_OVL0_BW;
	case DDP_COMPONENT_OVL1:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL1_FBDC_BW;
		else
			return DISP_PMQOS_OVL1_BW;
	case DDP_COMPONENT_OVL0_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL0_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL0_2L_BW;
	case DDP_COMPONENT_OVL1_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL1_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL1_2L_BW;
	case DDP_COMPONENT_OVL2_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL2_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL2_2L_BW;
	case DDP_COMPONENT_OVL3_2L:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL3_2L_FBDC_BW;
		else
			return DISP_PMQOS_OVL3_2L_BW;
	case DDP_COMPONENT_OVL0_2L_NWCG:
	case DDP_COMPONENT_OVL2_2L_NWCG:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL0_2L_NWCG_FBDC_BW;
		else
			return DISP_PMQOS_OVL0_2L_NWCG_BW;
	case DDP_COMPONENT_OVL1_2L_NWCG:
	case DDP_COMPONENT_OVL3_2L_NWCG:
		if (mode == DISP_BW_FBDC_MODE)
			return DISP_PMQOS_OVL1_2L_NWCG_FBDC_BW;
		else
			return DISP_PMQOS_OVL1_2L_NWCG_BW;
	case DDP_COMPONENT_RDMA0:
		return DISP_PMQOS_RDMA0_BW;
	case DDP_COMPONENT_RDMA1:
		return DISP_PMQOS_RDMA1_BW;
	case DDP_COMPONENT_RDMA2:
		return DISP_PMQOS_RDMA2_BW;
	case DDP_COMPONENT_WDMA0:
		return DISP_PMQOS_WDMA0_BW;
	case DDP_COMPONENT_WDMA1:
		return DISP_PMQOS_WDMA1_BW;
	case DDP_COMPONENT_OVLSYS_WDMA2:
		return DISP_PMQOS_OVLSYS_WDMA2_BW;
	default:
		DDPPR_ERR("%s, unknown comp %d\n", __func__, comp_id);
		break;
	}

	return -EINVAL;
}

int __mtk_disp_set_module_srt(struct icc_path *request, int comp_id,
				unsigned int bandwidth, unsigned int peak_bw, unsigned int bw_mode)
{
	DDPDBG("%s set %s bw = %u peak %u\n", __func__,
			mtk_dump_comp_str_id(comp_id), bandwidth, peak_bw);
	bandwidth = bandwidth * 133 / 100;

	mtk_icc_set_bw(request, MBps_to_icc(bandwidth), MBps_to_icc(peak_bw));

	DRM_MMP_MARK(pmqos, comp_id, bandwidth);

	return 0;
}

void __mtk_disp_set_module_hrt(struct icc_path *request,
				unsigned int bandwidth)
{
	if (bandwidth > 0)
		mtk_icc_set_bw(request, 0, MTK_MMQOS_MAX_BW);
	else
		mtk_icc_set_bw(request, 0, MBps_to_icc(bandwidth));
}

static bool mtk_disp_check_segment_mt6878(struct mtk_drm_crtc *mtk_crtc,
				struct mtk_drm_private *priv)
{
	bool ret = true;

	switch (priv->seg_id) {
	case 1:
		if (dsi1_status)
			ret = false;
		break;
	default:
		ret = true;
		break;
	}
	//DDPMSG("%s, segment:%d, ret=%d\n", __func__, priv->seg_id, ret);

	if (ret == false)
		DDPPR_ERR("%s fail: segment:%d\n", __func__, priv->seg_id);

	return ret;
}

static bool mtk_disp_check_segment_mt6897(struct mtk_drm_crtc *mtk_crtc,
				struct mtk_drm_private *priv)
{
	bool ret = true;
	int hact = 0;
	int vact = 0;
	int vrefresh = 0;

	hact = mtk_crtc->base.state->adjusted_mode.hdisplay;
	vact = mtk_crtc->base.state->adjusted_mode.vdisplay;
	vrefresh = drm_mode_vrefresh(&mtk_crtc->base.state->adjusted_mode);

	switch (priv->seg_id) {
	case 1:
	case 2:
		if (hact >= 1440 && vrefresh > 120)
			ret = false;
		break;
	default:
		ret = true;
		break;
	}

/*
 *	DDPMSG("%s, segment:%d, mode(%d, %d, %d)\n",
 *			__func__, priv->seg_id, hact, vact, vrefresh);
 */

	if (ret == false)
		DDPPR_ERR("%s, check sement fail: segment:%d, mode(%d, %d, %d)\n",
			__func__, priv->seg_id, hact, vact, vrefresh);

	return ret;
}

bool mtk_disp_check_segment(struct mtk_drm_crtc *mtk_crtc,
				struct mtk_drm_private *priv)
{
	bool ret = true;

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPPR_ERR("%s, mtk_crtc is NULL\n", __func__);
		return ret;
	}

	if (IS_ERR_OR_NULL(priv)) {
		DDPPR_ERR("%s, private is NULL\n", __func__);
		return ret;
	}

	if (seg_id_dbg) {
		priv->seg_id = seg_id_dbg;
		DDPMSG("%s, seg_id=%d\n", __func__, priv->seg_id);
	}

	if (priv->data->need_seg_id && !priv->is_tablet) {
		if (priv->data->mmsys_id == MMSYS_MT6878)
			ret = mtk_disp_check_segment_mt6878(mtk_crtc, priv);
		else if (priv->data->mmsys_id == MMSYS_MT6897)
			ret = mtk_disp_check_segment_mt6897(mtk_crtc, priv);
	}

	return ret;
}

static unsigned int mtk_disp_getMaxBW(unsigned int arr[], int size,
						unsigned int total_bw)
{
	unsigned int maxVal = arr[0];

	for (int i = 1; i < size; i++) {
		if (arr[i] > maxVal)
			maxVal = arr[i];
	}

	DDPINFO("%s maxVal = %d, total_bw = %d\n",__func__, maxVal, total_bw);

	if (maxVal > total_bw)
		return total_bw;
	else
		return maxVal;
}

static unsigned int mtk_disp_larb_hrt_bw_MT6989(struct mtk_drm_crtc *mtk_crtc,
						unsigned int total_bw, unsigned int bw_base)
{
	int i = 0;
	int max_sub_comm = 4; // 6989 sub common num
	int max_ovl_phy_layer = 12; // 6989 phy ovl layer num
	unsigned int subcomm_bw_sum[4] = {0};
	/* sub_comm0: layer0 + layer4 + layer9
	 * sub_comm1: layer1 + layer5 + layer8
	 * sub_comm2: layer2 + layer7 + layer11
	 * sub_comm3: layer3 + layer6 + layer10
	 */
	for (i = 0; i < max_ovl_phy_layer; i++) {
		if (mtk_crtc->usage_ovl_fmt[i]) {
			if (i == 0 || i == 4 || i == 9)
				subcomm_bw_sum[0] += bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;
			else if (i == 1 || i == 5 || i == 8)
				subcomm_bw_sum[1] += bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;
			else if (i == 2 || i == 7 || i == 11)
				subcomm_bw_sum[2] += bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;
			else if (i == 3 || i == 6 || i == 10)
				subcomm_bw_sum[3] += bw_base * mtk_crtc->usage_ovl_fmt[i] / 4;
			DDPINFO("%s, layer:%d,bpp:%u,channel bw:%u,%u,%u,%d\n",
				__func__, i, mtk_crtc->usage_ovl_fmt[i],subcomm_bw_sum[0], subcomm_bw_sum[1],
				subcomm_bw_sum[2], subcomm_bw_sum[3]);
		}
	}

	return mtk_disp_getMaxBW(subcomm_bw_sum, max_sub_comm, total_bw);
}

unsigned int mtk_disp_get_larb_hrt_bw(struct mtk_drm_crtc *mtk_crtc)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int tmp = NO_PENDING_HRT, bw_base = 0;

	bw_base = mtk_drm_primary_frame_bw(crtc);
	if (priv->data->mmsys_id == MMSYS_MT6989) {
		if (bw_base != 7000)
			tmp = mtk_disp_larb_hrt_bw_MT6989(mtk_crtc, 7000, bw_base);
		else
			tmp = bw_base;
	}
	return tmp;
}

static void mtk_disp_calc_larb_hrt_bw_func(struct mtk_larb_bw *data,
	struct mtk_larb_bw *larb_list, unsigned int *larb_count,
	unsigned int *larb_total, unsigned int *max_larb_bw)
{
	unsigned int id = 0;

	if (!data || data->larb_id < 0 || !larb_list ||
		!larb_count || *larb_count > MAX_HRT_LARB_NR)
		return;

	for (id = 0; id < *larb_count; id++) {
		if (larb_list[id].larb_id == data->larb_id)
			break;
	}

	if (id == *larb_count && id < MAX_HRT_LARB_NR) {
		larb_list[*larb_count].larb_id = data->larb_id;
		*larb_count = *larb_count + 1;
	} else if (id >= MAX_HRT_LARB_NR) {
		DDPPR_ERR("%s, exceed larb count:%u,larb:%d, bw:%u\n",
			__func__, id, data->larb_id, data->larb_bw);
		return;
	}

	larb_list[id].larb_bw += data->larb_bw;
	if (larb_total)
		*larb_total += data->larb_bw;
	if (max_larb_bw && *max_larb_bw < larb_list[id].larb_bw)
		*max_larb_bw = larb_list[id].larb_bw;
}

static void mtk_disp_calc_larb_hrt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int path_idx,
		unsigned int bw, unsigned int *larb_count, struct mtk_larb_bw *larb_list,
		unsigned int *max_larb_bw)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_private *priv = NULL;
	struct mtk_ddp_comp *comp;
	unsigned int crtc_idx = 0, larb_total = 0;
	struct mtk_larb_bw larb_data = { 0 };
	int j = 0, ret = 0;
	int diff = 0;

	if (IS_ERR_OR_NULL(larb_list) || IS_ERR_OR_NULL(mtk_crtc) ||
		IS_ERR_OR_NULL(larb_count))
		return;

	crtc = &mtk_crtc->base;
	if (IS_ERR_OR_NULL(crtc) || IS_ERR_OR_NULL(crtc->dev))
		return;

	crtc_idx = drm_crtc_index(crtc);
	priv = crtc->dev->dev_private;
	if (!priv || !mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_LAYERING_RULE_BY_LARB))
		return;

	for_each_comp_in_crtc_target_path(comp, mtk_crtc, j, path_idx) {
		larb_data.larb_id = -1;
		larb_data.larb_bw = bw;
		ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_GET_HRT_BW,
					&larb_data);
		mtk_disp_calc_larb_hrt_bw_func(&larb_data, &larb_list[0],
			larb_count, &larb_total, max_larb_bw);
		if (larb_data.larb_id != -1) {
			DRM_MMP_MARK(hrt_bw, comp->id, larb_data.larb_bw);
			DDP_HBL("%s,crtc:%u,p:%u,bw:%u,cmp:%u,larb(%u,%u),cnt:%u,total:%u,max:%u\n",
				__func__, crtc_idx, path_idx, bw,
				comp->id, larb_data.larb_id,
				larb_data.larb_bw, *larb_count, larb_total,
				max_larb_bw? *max_larb_bw : 0);
		}
	}

	if (mtk_crtc->is_dual_pipe) {
		for_each_comp_in_dual_pipe(comp, mtk_crtc, j, path_idx) {
			larb_data.larb_id = -1;
			larb_data.larb_bw = bw;
			ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_GET_HRT_BW,
						&larb_data);
			mtk_disp_calc_larb_hrt_bw_func(&larb_data, &larb_list[0],
				larb_count, &larb_total, max_larb_bw);
			if (larb_data.larb_id != -1) {
				DRM_MMP_MARK(hrt_bw, comp->id, larb_data.larb_bw);
				DDP_HBL("%s,crtc:%u,p:%u,bw:%u,cmp:%u,larb(%u,%u),cnt:%u,total:%u,max:%u\n",
					__func__, crtc_idx, path_idx, bw,
					comp->id, larb_data.larb_id,
					larb_data.larb_bw, *larb_count, larb_total,
					max_larb_bw? *max_larb_bw : 0);
			}
		}
	}

	if (!g_hrt_by_larb_debug)
		return;

	diff = (int)(larb_total - bw);
	if (diff > 10 || diff < -10)
		DDP_HBL("%s,CRTC%d,diff:%d,bw=%u,larb_total=%u,larb_max=%u,count=%u\n",
			__func__, crtc_idx, diff, bw, larb_total,
			max_larb_bw ? *max_larb_bw : 0, *larb_count);
}

static void mtk_disp_set_larb_hrt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int larb_count,
		struct mtk_larb_bw *larb_list)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_private *priv = NULL;
	int i = 0, j = 0;
	unsigned int crtc_idx = 0;

	if (larb_count == 0 || IS_ERR_OR_NULL(larb_list) ||
		IS_ERR_OR_NULL(mtk_crtc))
		return;

	crtc = &mtk_crtc->base;
	if (IS_ERR_OR_NULL(crtc) || IS_ERR_OR_NULL(crtc->dev))
		return;

	crtc_idx = drm_crtc_index(crtc);
	priv = crtc->dev->dev_private;
	if (!priv || !mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_LAYERING_RULE_BY_LARB))
		return;

	/*update larb bw of current crtc*/
	for (i = 0; i < larb_count; i++) {
		for (j = 0; j < MAX_HRT_LARB_NR; j++) {
			if (priv->larbs_hrt_bw[crtc_idx][j].larb_id < 0) {
				DDPMSG("%s, cannot find larb id:%d, bw:%u\n",
					__func__, larb_list[i].larb_id, larb_list[i].larb_bw);
				break;
			}
			if (priv->larbs_hrt_bw[crtc_idx][j].larb_id == larb_list[i].larb_id) {
				if (priv->larbs_hrt_bw[crtc_idx][j].larb_bw != larb_list[i].larb_bw) {
					DDPDBG_HBL("%s,crtc:%u,larb:%u,update bw:%u->%u\n",
						__func__, crtc_idx, larb_list[i].larb_id,
						priv->larbs_hrt_bw[crtc_idx][j].larb_bw,
						larb_list[i].larb_bw);
					priv->larbs_hrt_bw[crtc_idx][j].larb_bw = larb_list[i].larb_bw;
				}
				break;
			}
		}
	}

	/*update larb bw of all crtc*/
	for (j = 0; j < MAX_HRT_LARB_NR; j++) {
		unsigned int total = 0;
		unsigned int larb_idx = priv->larbs_hrt_req[j].larb_id;

		if (IS_ERR_OR_NULL(priv->larbs_hrt_req[j].hrt_req))
			break;

		for (crtc_idx = 0; crtc_idx < MAX_CRTC; crtc_idx++) {
			if (priv->larbs_hrt_bw[crtc_idx][j].larb_bw == 0)
				continue;
			total += priv->larbs_hrt_bw[crtc_idx][j].larb_bw;
			DRM_MMP_MARK(hrt_bw, (0xf000000 | (crtc_idx < 16) | larb_idx),
				priv->larbs_hrt_bw[crtc_idx][j].larb_bw);
			DDPDBG_HBL("%s>>>>>crtc:%d,larb:%u,bw:%u\n",
				__func__, crtc_idx, larb_idx,
				priv->larbs_hrt_bw[crtc_idx][j].larb_bw);
		}

		if (total != priv->larbs_hrt_req[j].last_larb_bw) {
			DDP_HBL("%s>>>>>update VMM dvfs level,larb:%u,bw:%u->%u\n",
				__func__, larb_idx,
				priv->larbs_hrt_req[j].last_larb_bw, total);
			DRM_MMP_MARK(hrt_bw, (0xff000000 | larb_idx), total);
			mtk_icc_set_bw(priv->larbs_hrt_req[j].hrt_req, 0,
				MBps_to_icc(total));
			priv->larbs_hrt_req[j].last_larb_bw = total;
		}
	}
}

int mtk_disp_set_hrt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	unsigned int crtc_idx = drm_crtc_index(crtc);
	struct mtk_ddp_comp *comp;
	unsigned int tmp, total = 0, max_larb_bw = 0, bw_base = 0;
	int i, j, ret = 0, ovl_num = 0;
	struct mtk_larb_bw larb_list[MAX_HRT_LARB_NR] = { 0 };
	unsigned int larb_count = 0;
	static bool vidle_ready;

	tmp = bw;

	if (mtk_crtc == NULL)
		return 0;

	if (mtk_crtc->ddp_mode >= DDP_MODE_NR)
		return 0;

	DRM_MMP_EVENT_START(hrt_bw, crtc_idx, bw);

	for (i = 0; i < DDP_PATH_NR; i++) {
		if (mtk_crtc->ddp_mode >= DDP_MODE_NR)
			continue;
		if (!(mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].req_hrt[i]))
			continue;

		/* calculate VMM dvfs level with real larb HRT BW */
		if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_LAYERING_RULE_BY_LARB))
			mtk_disp_calc_larb_hrt_bw(mtk_crtc, i, tmp,
				&larb_count, &larb_list[0], &max_larb_bw);

		/* update OTSD of smi port */
		for_each_comp_in_crtc_target_path(comp, mtk_crtc, j, i) {
			ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
						   &tmp);
		}
		if (!mtk_crtc->is_dual_pipe)
			continue;
		for_each_comp_in_dual_pipe(comp, mtk_crtc, j, i)
			ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
					&tmp);
	}

	/* update VMM dvfs level with real larb HRT BW */
	if (mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_LAYERING_RULE_BY_LARB))
		mtk_disp_set_larb_hrt_bw(mtk_crtc, larb_count, larb_list);

	if (ret == RDMA_REQ_HRT)
		tmp = mtk_drm_primary_frame_bw(crtc);
	else if (ret == MDP_RDMA_REQ_HRT) {
		DRM_MMP_EVENT_END(hrt_bw, crtc_idx, 0);
		return 0;
	}

	/* skip same HRT BW */
	if (priv->req_hrt[crtc_idx] == tmp) {
		DRM_MMP_EVENT_END(hrt_bw, crtc_idx, 0);
		return 0;
	}
	priv->req_hrt[crtc_idx] = tmp;

	for (i = 0; i < MAX_CRTC; ++i)
		total += priv->req_hrt[i];

	/* update VCORE dvfs level with total HRT BW */
	if ((priv->data->mmsys_id == MMSYS_MT6897) &&
		(mtk_disp_check_segment(mtk_crtc, priv) == false))
		mtk_icc_set_bw(priv->hrt_bw_request, 0, MBps_to_icc(1));
	else if (!mtk_vidle_support_hrt_bw() ||
		!mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_LAYERING_RULE_BY_LARB)) {
		DRM_MMP_MARK(hrt_bw, 0xffff0000, total);
		mtk_icc_set_bw(priv->hrt_bw_request, 0, MBps_to_icc(total));
	} else if (!vidle_ready && mtk_vidle_support_hrt_bw() &&
		mtk_drm_helper_get_opt(priv->helper_opt,
		MTK_DRM_OPT_LAYERING_RULE_BY_LARB)) {
		/*clear mmqos total bw after DPC is available for HRT BW update*/
		DRM_MMP_MARK(hrt_bw, 0xffff0000, 1);
		DDPMSG("%s,clear mmqos bw\n", __func__);
		mtk_icc_set_bw(priv->hrt_bw_request, 0, MBps_to_icc(1));
		vidle_ready = true;
	}
	mtk_vidle_hrt_bw_set(total);

	if (mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_HRT_BY_LARB)) {

		comp = mtk_ddp_comp_request_output(mtk_crtc);

		if (comp && mtk_ddp_comp_get_type(comp->id) == MTK_DISP_DPTX) {
			tmp = tmp / (mtk_crtc->is_dual_pipe + 1);
			mtk_icc_set_bw(priv->dp_hrt_by_larb, 0, MBps_to_icc(tmp));
			DDPINFO("%s, CRTC%d(DP) HRT total=%u larb bw=%u dual=%d\n",
				__func__, crtc_idx, total, tmp, mtk_crtc->is_dual_pipe);
		} else if (comp && mtk_ddp_comp_get_type(comp->id) == MTK_DSI &&
			(priv->data->mmsys_id != MMSYS_MT6989)) {
			if (total > 0 && !mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_LAYERING_RULE_BY_LARB)) {
				bw_base = mtk_drm_primary_frame_bw(crtc);
				ovl_num = bw_base > 0 ? total / bw_base : 0;
				max_larb_bw = ((bw_base / 2) > total) ? total : (ovl_num < 3) ?
					(bw_base / 2) : (ovl_num < 5) ?
					bw_base : (bw_base * 3 / 2);
			}

			/* update VMM dvfs level with max larb hrt bw*/
			if ((priv->data->mmsys_id == MMSYS_MT6897) &&
				(mtk_disp_check_segment(mtk_crtc, priv) == false))
				mtk_icc_set_bw(priv->hrt_by_larb, 0, MBps_to_icc(1));
			else if (!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_LAYERING_RULE_BY_LARB))
				mtk_icc_set_bw(priv->hrt_by_larb, 0, MBps_to_icc(max_larb_bw));

			/* update VDISP dvfs level with max larb hrt bw*/
			mtk_vidle_dvfs_bw_set(max_larb_bw);
			mtk_crtc->qos_ctx->last_larb_hrt_max = max_larb_bw;

			DDPINFO("%s,CRTC%d update VDISP dvfs,bw=%u,total=%u,max=%u,bw_base=%d\n",
				__func__, crtc_idx, tmp, total, max_larb_bw, bw_base);
		}
	} else
		DDPINFO("set CRTC:%d HRT bw %u %u\n", crtc_idx, tmp, total);

	DRM_MMP_EVENT_END(hrt_bw, priv->req_hrt[crtc_idx], total);
	return ret;
}

int mtk_disp_set_per_larb_hrt_bw(struct mtk_drm_crtc *mtk_crtc, unsigned int bw)
{
	struct drm_crtc *crtc = &mtk_crtc->base;
	struct mtk_drm_private *priv = crtc->dev->dev_private;
	struct mtk_ddp_comp *comp;
	unsigned int total = 0xFFFFFFFF, tmp1 = 0, bw_base  = 0;
	unsigned int crtc_idx = drm_crtc_index(crtc);

	if (mtk_crtc == NULL)
		return 0;

	if (mtk_crtc->ddp_mode >= DDP_MODE_NR)
		return 0;

	if (!mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_HRT_BY_LARB))
		return 0;

	comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (bw == 0 && crtc_idx == 0)
		total = bw;

	if (comp && mtk_ddp_comp_get_type(comp->id) == MTK_DSI) {
		if (total > 0) {
			bw_base = mtk_drm_primary_frame_bw(crtc);
			if (priv->data->mmsys_id == MMSYS_MT6989) {
				if (bw != 7000)
					tmp1 = mtk_disp_larb_hrt_bw_MT6989(mtk_crtc, total, bw_base);
				else
					tmp1 = bw;
			}
		} else {
			if (priv->data->mmsys_id == MMSYS_MT6989) {
				DDPMSG("%s, crtc:%d clear channel BW, bw:%u", __func__, crtc_idx, bw);
				tmp1 = bw;
			}
		}

		mtk_icc_set_bw(priv->hrt_by_larb, 0, MBps_to_icc(tmp1));

		mtk_vidle_dvfs_bw_set(tmp1);
		DDPINFO("%s, CRTC%d larb bw=%u bw_base=%d\n",
			__func__, crtc_idx, tmp1, bw_base);
	}

	return 0;
}

void mtk_drm_pan_disp_set_hrt_bw(struct drm_crtc *crtc, const char *caller)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct drm_display_mode *mode;
	unsigned int bw = 0;
	struct mtk_drm_private *priv;

	if (drm_crtc_index(crtc) == 0)
		dev_crtc = crtc;
	mtk_crtc = to_mtk_crtc(crtc);
	mode = &crtc->state->adjusted_mode;

	bw = _layering_get_frame_bw(crtc, mode);
	mtk_crtc_init_hrt_usage(crtc);
	mtk_disp_set_hrt_bw(mtk_crtc, bw);

	mtk_crtc->usage_ovl_fmt[0] = 4;
	priv = mtk_crtc->base.dev->dev_private;
	if (priv && mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_HRT_BY_LARB) &&
		priv->data->mmsys_id == MMSYS_MT6989) {
		mtk_disp_set_per_larb_hrt_bw(mtk_crtc, bw);
		mtk_crtc->qos_ctx->last_larb_hrt_max = bw;
	} else
		DDPMSG("%s: invalid priv\n", __func__);

	DDPMSG("%s: crtc:%d pan_disp_set_hrt_bw:%u, last channel:%u\n", caller, drm_crtc_index(crtc), bw,
		mtk_crtc->qos_ctx->last_larb_hrt_max);
}

void mtk_disp_hrt_repaint_blocking(const unsigned int hrt_idx)
{
	int i, ret;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(dev_crtc);

	drm_trigger_repaint(DRM_REPAINT_FOR_IDLE, dev_crtc->dev);
	for (i = 0; i < 5; ++i) {
		ret = wait_event_timeout(
			mtk_crtc->qos_ctx->hrt_cond_wq,
			atomic_read(&mtk_crtc->qos_ctx->hrt_cond_sig),
			HZ / 5);
		if (ret == 0)
			DDPINFO("wait repaint timeout %d\n", i);
		atomic_set(&mtk_crtc->qos_ctx->hrt_cond_sig, 0);
		if (atomic_read(&mtk_crtc->qos_ctx->last_hrt_idx) >= hrt_idx)
			break;
	}
}

/* force report all display's mmqos BW include SRT & HRT */
void mtk_disp_mmqos_bw_repaint(struct mtk_drm_private *priv)
{
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_ddp_comp *comp;
	unsigned int i, j, k, c, tmp = 1, flag = DISP_BW_FORCE_UPDATE;
	int ret = 0;
	bool is_hrt;

	for (c = 0 ; c < MAX_CRTC ; ++c) {
		crtc = priv->crtc[c];
		if (crtc == NULL)
			continue;
		mtk_crtc = to_mtk_crtc(crtc);

		DDP_MUTEX_LOCK_NESTED(&mtk_crtc->lock, c, __func__, __LINE__);

		if (!(mtk_crtc->enabled)) {
			DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->lock, c, __func__, __LINE__);
			continue;
		}

		for (k = 0; k < DDP_PATH_NR; k++) {
			is_hrt = (mtk_crtc->ddp_mode < DDP_MODE_NR) ?
				mtk_crtc->ddp_ctx[mtk_crtc->ddp_mode].req_hrt[k] : false;
			for_each_comp_in_crtc_target_path(comp, mtk_crtc, j, k) {
				//report SRT BW
				ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_UPDATE_BW,
						&flag);
				//report HRT BW if path is HRT
				if (is_hrt)
					ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
							&tmp);
			}
			if (!mtk_crtc->is_dual_pipe)
				continue;
			for_each_comp_in_dual_pipe(comp, mtk_crtc, j, i) {
				//report SRT BW
				ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_UPDATE_BW,
						&flag);
				//report HRT BW if path is HRT
				if (is_hrt)
					ret |= mtk_ddp_comp_io_cmd(comp, NULL, PMQOS_SET_HRT_BW,
							&tmp);
			}
		}

		DDP_MUTEX_UNLOCK_NESTED(&mtk_crtc->lock, c, __func__, __LINE__);
	}
}

int mtk_disp_hrt_cond_change_cb(struct notifier_block *nb, unsigned long value,
				void *v)
{
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(dev_crtc);
	unsigned int hrt_idx;

	DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);

	/* No need to repaint when display suspend */
	if (!mtk_crtc->enabled) {
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		return 0;
	}

	switch (value) {
	case BW_THROTTLE_START: /* CAM on */
		DDPMSG("DISP BW Throttle start\n");
		/* TODO: concider memory session */
		DDPINFO("CAM trigger repaint\n");
		hrt_idx = _layering_rule_get_hrt_idx(drm_crtc_index(dev_crtc));
		hrt_idx++;
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);
		mtk_disp_hrt_repaint_blocking(hrt_idx);
		mtk_disp_mmqos_bw_repaint(dev_crtc->dev->dev_private);
		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
		break;
	case BW_THROTTLE_END: /* CAM off */
		DDPMSG("DISP BW Throttle end\n");
		/* TODO: switch DC */
		DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

		/* bw repaint might hold all crtc's mutex, need unlock current mutex first */
		mtk_disp_mmqos_bw_repaint(dev_crtc->dev->dev_private);

		DDP_MUTEX_LOCK(&mtk_crtc->lock, __func__, __LINE__);
		break;
	default:
		break;
	}

	DDP_MUTEX_UNLOCK(&mtk_crtc->lock, __func__, __LINE__);

	return 0;
}

struct notifier_block pmqos_hrt_notifier = {
	.notifier_call = mtk_disp_hrt_cond_change_cb,
};

int mtk_disp_hrt_bw_dbg(void)
{
	mtk_disp_hrt_cond_change_cb(NULL, BW_THROTTLE_START, NULL);

	return 0;
}

int mtk_disp_hrt_cond_init(struct drm_crtc *crtc)
{
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv;

	mtk_crtc = to_mtk_crtc(crtc);

	if (drm_crtc_index(crtc) == 0)
		dev_crtc = crtc;

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPPR_ERR("%s:mtk_crtc is NULL\n", __func__);
		return -EINVAL;
	}

	priv = mtk_crtc->base.dev->dev_private;

	mtk_crtc->qos_ctx = vmalloc(sizeof(struct mtk_drm_qos_ctx));
	if (mtk_crtc->qos_ctx == NULL) {
		DDPPR_ERR("%s:allocate qos_ctx failed\n", __func__);
		return -ENOMEM;
	}
	atomic_set(&mtk_crtc->qos_ctx->last_hrt_idx, 0);
	mtk_crtc->qos_ctx->last_hrt_req = 0;
	mtk_crtc->qos_ctx->last_mmclk_req_idx = 0;
	mtk_crtc->qos_ctx->last_larb_hrt_max = 0;

	if (drm_crtc_index(crtc) == 0 && mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_MMQOS_SUPPORT))
		mtk_mmqos_register_bw_throttle_notifier(&pmqos_hrt_notifier);

	return 0;
}

static void mtk_drm_mmdvfs_get_avail_freq(struct device *dev)
{
	int i = 0;
	struct dev_pm_opp *opp;
	unsigned long freq;
	int ret;

	step_size = dev_pm_opp_get_opp_count(dev);
	g_freq_steps = kcalloc(step_size, sizeof(unsigned long), GFP_KERNEL);
	freq = 0;
	while (!IS_ERR(opp = dev_pm_opp_find_freq_ceil(dev, &freq))) {
		g_freq_steps[i] = freq;
		freq++;
		i++;
		dev_pm_opp_put(opp);
	}

	ret = of_property_read_u32(dev->of_node, "lp-mmclk-freq", &lp_freq);
	DDPINFO("%s lp_freq = %d\n", __func__, lp_freq);
}

void mtk_drm_mmdvfs_init(struct device *dev)
{
	struct device_node *node = dev->of_node;
	int ret = 0;

	dev_pm_opp_of_add_table(dev);
	mtk_drm_mmdvfs_get_avail_freq(dev);

	/* support DPC and VDISP */
	ret = of_property_read_u8(node, "vdisp-dvfs-opp", &vdisp_opp);
	if (ret == 0) {
		mm_freq_request = devm_regulator_get_optional(dev, "dis1-shutdown");
		if (mm_freq_request == NULL)
			DDPMSG("%s use vdisp opp(%u)\n", __func__, vdisp_opp);
		else if (IS_ERR(mm_freq_request))
			mm_freq_request = NULL;
		else
			DDPMSG("%s use vdisp but regulator flow\n", __func__);
		return;
	}

	/* MMDVFS V2 */
	DDPMSG("%s, try to use MMDVFS V2, vdisp_opp:%u, ret:%d\n", __func__, vdisp_opp, ret);
	mm_freq_request = devm_regulator_get_optional(dev, "mmdvfs-dvfsrc-vcore");
	if (IS_ERR_OR_NULL(mm_freq_request))
		DDPPR_ERR("%s, get mmdvfs-dvfsrc-vcore failed\n", __func__);
}

unsigned int mtk_drm_get_mmclk_step_size(void)
{
	return step_size;
}

void mtk_drm_set_mmclk(struct drm_crtc *crtc, int level, bool lp_mode,
			const char *caller)
{
	struct dev_pm_opp *opp;
	unsigned long freq;
	int volt, ret, idx, i;
	int final_lp_mode = true;
	int final_level = -1;
	int cnt = 0;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct mtk_ddp_comp *output_comp;

	idx = drm_crtc_index(crtc);

	if (IS_ERR_OR_NULL(mtk_crtc)) {
		DDPPR_ERR("%s invalid mtk_crtc\n", __func__);
		return;
	}

	/* memory session do not use */
	output_comp = mtk_ddp_comp_request_output(mtk_crtc);
	if (IS_ERR_OR_NULL(output_comp) ||
		mtk_ddp_comp_get_type(output_comp->id) == MTK_DISP_WDMA) {
		DDPINFO("crtc%d not support set mmclk\n", idx);
		return;
	}

	DDPINFO("%s[%d] g_freq_level[idx=%d]: %d, g_freq_lp[idx=%d]: %d\n",
		__func__, __LINE__, idx, level, idx, lp_mode);

	if (level < 0 || level > (step_size - 1))
		level = -1;

	if (level == g_freq_level[idx] && lp_mode == g_freq_lp[idx])
		return;

	g_freq_lp[idx] = lp_mode;
	g_freq_level[idx] = level;

	for (i = 0; i < CRTC_NUM; i++) {
		if (g_freq_level[i] == -1)
			cnt++;

		if (g_freq_level[i] != -1 && !g_freq_lp[i])
			final_lp_mode = false;

		if (g_freq_level[i] > final_level)
			final_level = g_freq_level[i];
	}

	if (cnt == CRTC_NUM)
		final_lp_mode = false;

	if (final_level >= 0)
		freq = g_freq_steps[final_level];
	else
		freq = g_freq_steps[0];

	DDPINFO("%s[%d] final_level(freq=%d, %lu) final_lp_mode:%d\n",
		__func__, __LINE__, final_level, freq, final_lp_mode);

	if ((vdisp_opp != U8_MAX) && (mm_freq_request == NULL)) {
		if (final_level >= 0)
			vdisp_opp = final_level;
		mtk_vidle_dvfs_set(vdisp_opp);
		return;
	}

	if (mm_freq_request) {
		if (vdisp_opp == U8_MAX) /* not support for vdisp platform */
			mmdvfs_set_lp_mode(final_lp_mode);

		opp = dev_pm_opp_find_freq_ceil(crtc->dev->dev, &freq);
		volt = dev_pm_opp_get_voltage(opp);
		dev_pm_opp_put(opp);
		DDPINFO("%s, crtc=%d, volt=%d\n", __func__, idx, volt);
		ret = regulator_set_voltage(mm_freq_request, volt, INT_MAX);
		if (ret)
			DDPPR_ERR("%s:regulator_set_voltage fail\n", __func__);
	}
}

void mtk_drm_set_mmclk_by_pixclk(struct drm_crtc *crtc,
	unsigned int pixclk, const char *caller)
{
	int i;
	unsigned long freq = pixclk * 1000000;
	struct mtk_drm_private *priv = crtc->dev->dev_private;

	mutex_lock(&priv->set_mmclk_lock);
	g_freq = freq;

	if (freq > g_freq_steps[step_size - 1]) {
		DDPPR_ERR("%s:pixleclk (%lu) is to big for mmclk (%lu)\n",
			caller, freq, g_freq_steps[step_size - 1]);
		mtk_drm_set_mmclk(crtc, step_size - 1, false, caller);
		mutex_unlock(&priv->set_mmclk_lock);
		return;
	}
	if (!freq) {
		mtk_drm_set_mmclk(crtc, -1, false, caller);
		mutex_unlock(&priv->set_mmclk_lock);
		return;
	}
	for (i = step_size - 2 ; i >= 0; i--) {
		if (freq > g_freq_steps[i]) {
			mtk_drm_set_mmclk(crtc, i + 1, false, caller);
			break;
		}
		if (i == 0) {
			if (!lp_freq || (lp_freq && (freq > lp_freq)))
				mtk_drm_set_mmclk(crtc, 0, false, caller);
			else
				mtk_drm_set_mmclk(crtc, 0, true, caller);
		}
	}
	mutex_unlock(&priv->set_mmclk_lock);
}

unsigned long mtk_drm_get_freq(struct drm_crtc *crtc, const char *caller)
{
	return g_freq;
}

unsigned long mtk_drm_get_mmclk(struct drm_crtc *crtc, const char *caller)
{
	int idx;
	unsigned long freq;

	if (!crtc || !g_freq_steps)
		return 0;

	idx = drm_crtc_index(crtc);

	if (g_freq_level[idx] >= 0)
		freq = g_freq_steps[g_freq_level[idx]];
	else
		freq = g_freq_steps[0];

	DDPINFO("%s[%d]g_freq_level[idx=%d]: %d (freq=%lu)\n",
		__func__, __LINE__, idx, g_freq_level[idx], freq);

	return freq;
}

