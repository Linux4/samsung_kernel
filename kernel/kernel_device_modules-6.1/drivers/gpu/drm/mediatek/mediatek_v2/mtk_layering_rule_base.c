// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/semaphore.h>
#include <linux/module.h>
#include <linux/wait.h>
#include <linux/kthread.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/file.h>
#include <linux/string.h>
#include <linux/mm.h>
//#include <mt-plat/mtk_chip.h>
#include <drm/drm_modes.h>
#include <drm/drm_property.h>
#ifdef CONFIG_MTK_DCS
#include <mt-plat/mtk_meminfo.h>
#endif
#include "mtk_layering_rule.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_assert.h"
#include "mtk_log.h"
#include "mtk_drm_mmp.h"
#define CREATE_TRACE_POINTS
#include "mtk_layer_layout_trace.h"
#include "mtk_drm_gem.h"
#include "mtk_dsi.h"

#include "../mml/mtk-mml.h"
#include "../mml/mtk-mml-color.h"
#include "../mml/mtk-mml-drm-adaptor.h"
#include "mtk_disp_oddmr/mtk_disp_oddmr.h"

#include <linux/module.h>

int skip_mdp;
module_param(skip_mdp, int, 0644);


extern unsigned int g_mml_mode;

static struct drm_mtk_layering_info layering_info;
static bool g_hrt_valid;
#ifdef HRT_UT_DEBUG
static int debug_resolution_level;
#endif
static struct layering_rule_info_t *l_rule_info;
static struct layering_rule_ops *l_rule_ops;
static int ext_id_tuning(struct drm_device *dev,
			  struct drm_mtk_layering_info *disp_info,
			  int disp_idx);
static unsigned int roll_gpu_for_idle;
static int g_emi_bound_table[HRT_LEVEL_NUM];
int have_force_gpu_layer;
int sum_overlap_w_of_bwm;
bool need_rollback_to_gpu_before_gpuc;
static bool have_gpu_cached;
static int last_fbt_weight;

static DEFINE_MUTEX(layering_info_lock);

#define DISP_MML_LAYER_LIMIT 1
#define DISP_LAYER_RULE_MAX_NUM 1024

static struct {
	enum LYE_HELPER_OPT opt;
	unsigned int val;
	const char *desc;
} help_info[] = {
	{LYE_OPT_DUAL_PIPE, 0, "LYE_OPT_DUAL_PIPE"},
	{LYE_OPT_EXT_LAYER, 0, "LYE_OPT_EXTENDED_LAYER"},
	{LYE_OPT_RPO, 0, "LYE_OPT_RPO"},
	{LYE_OPT_CLEAR_LAYER, 0, "LYE_OPT_CLEAR_LAYER"},
	{LYE_OPT_OVL_BW_MONITOR, 0, "LYE_OPT_OVL_BW_MONITOR"},
	{LYE_OPT_GPU_CACHE, 0, "LYE_OPT_GPU_CACHE"},
	{LYE_OPT_SPHRT, 0, "LYE_OPT_SPHRT"},
	{LYE_OPT_SPDA_OVL_SWITCH, 0, "LYE_OPT_SPDA_OVL_SWITCH"},
};

struct debug_gles_range {
	int head;
	int tail;
	char msg[256];
	int written;
};

void mtk_set_layering_opt(enum LYE_HELPER_OPT opt, int value)
{
	if (opt >= LYE_OPT_NUM) {
		DDPMSG("%s invalid layering opt:%d\n", __func__, opt);
		return;
	}
	if (value < 0) {
		DDPPR_ERR("%s invalid opt value:%d\n", __func__, value);
		return;
	}


	help_info[opt].val = !!value;
}

int get_layering_opt(enum LYE_HELPER_OPT opt)
{
	if (opt >= LYE_OPT_NUM) {
		DDPMSG("%s invalid layering opt:%d\n", __func__, opt);
		return -1;
	}

	return help_info[opt].val;
}

/*
 * bool is_decouple_path(struct drm_mtk_layering_info *disp_info) {
 *	if (disp_info->disp_mode[HRT_PRIMARY] != 1)
 *		return true;
 *	else
 *		return false;
 * }
 */

/*
 * static bool is_argb_fmt(uint32_t format) {
 *	switch (format) {
 *	case DRM_FORMAT_ARGB8888:
 *	case DRM_FORMAT_ABGR8888:
 *	case DRM_FORMAT_RGBA8888:
 *	case DRM_FORMAT_BGRA8888:
 *		return true;
 *	default:
 *		return false;
 *	}
 * }
 */

bool mtk_is_yuv(uint32_t format)
{
	switch (format) {
	case DRM_FORMAT_YUV422:
	case DRM_FORMAT_YVU422:
	case DRM_FORMAT_YUV444:
	case DRM_FORMAT_YVU444:
		return true;
	default:
		return false;
	}
}

bool mtk_is_layer_id_valid(struct drm_mtk_layering_info *disp_info,
			   int idx, int i)
{
	if (i < 0 || i >= disp_info->layer_num[idx])
		return false;
	else
		return true;
}

bool mtk_is_gles_layer(struct drm_mtk_layering_info *disp_info, int idx,
		       int layer_idx)
{
	if (layer_idx >= disp_info->gles_head[idx] &&
	    layer_idx <= disp_info->gles_tail[idx])
		return true;
	else
		return false;
}

static void mtk_gles_incl_layer(struct drm_mtk_layering_info *disp_info,
				const u32 disp_idx, const u32 layer_idx)
{
	if (unlikely(disp_idx >= LYE_CRTC)) {
		DDPPR_ERR("%s[%d]:disp_idx:%u\n", __func__, __LINE__, disp_idx);
		return;
	}

	if (disp_info->gles_head[disp_idx] == -1 ||
		disp_info->gles_head[disp_idx] > layer_idx)
		disp_info->gles_head[disp_idx] = layer_idx;
	if (disp_info->gles_tail[disp_idx] == -1 ||
		disp_info->gles_tail[disp_idx] < layer_idx)
		disp_info->gles_tail[disp_idx] = layer_idx;
}

inline bool mtk_has_layer_cap(struct drm_mtk_layer_config *layer_info,
			      enum MTK_LAYERING_CAPS l_caps)
{
	if (layer_info->layer_caps & l_caps)
		return true;
	return false;
}

static int is_overlap_on_yaxis(struct drm_mtk_layer_config *lhs,
			       struct drm_mtk_layer_config *rhs)
{
	/*
	 * HWC may adjust the offset of yuv layer due to alignment limitation
	 * after querying layering rule.
	 * So it have chance to make yuv layer overlap with other ext layer.
	 * We add the workaround here to avoid the yuv as the base layer of
	 * extended layer and will remove it once the HWC correct the problem.
	 */
	if (mtk_is_yuv(lhs->src_fmt))
		return 1;

	if ((lhs->dst_offset_y + lhs->dst_height <= rhs->dst_offset_y) ||
	    (rhs->dst_offset_y + rhs->dst_height <= lhs->dst_offset_y))
		return 0;
	return 1;
}

static bool is_layer_across_each_pipe(struct drm_crtc *crtc,
	struct drm_mtk_layer_config *layer_info, unsigned int disp_w)
{
	unsigned int dst_x, dst_w;
	struct mtk_drm_crtc *mtk_crtc;

	if (crtc == NULL)
		return true;

	mtk_crtc = to_mtk_crtc(crtc);
	if (!mtk_crtc->is_dual_pipe)
		return true;

	dst_x = layer_info->dst_offset_x;
	dst_w = layer_info->dst_width;

	if ((dst_x + dst_w <= disp_w / 2) ||
	    (dst_x > disp_w / 2))
		return false;

	return true;
}

static inline bool is_extended_layer(struct drm_mtk_layer_config *layer_info)
{
	return (layer_info->ext_sel_layer != -1);
}

static bool is_extended_base_layer_valid(struct drm_crtc *crtc,
		struct drm_mtk_layer_config *configs, int layer_idx)
{
	if ((layer_idx == 0) ||
		(configs->src_fmt == MTK_DRM_FORMAT_DIM) ||
		mtk_has_layer_cap(configs, MTK_DISP_RSZ_LAYER |
			MTK_MML_DISP_DIRECT_DECOUPLE_LAYER |
			MTK_MML_DISP_DIRECT_LINK_LAYER))
		return false;

	/*
	 * Under dual pipe, if the layer is not included in each pipe,
	 * it cannot be used as a base layer for extended layer
	 * because extended layer would not find base layer in one of
	 * display pipe.
	 * So always mark this specific layer as overlap to avoid the fail case.
	 * *
	 * UPDATE @ 2020/12/17
	 * Could skip this step through revise ovl extended layer config
	 * flow; by enable attached layer index's RDMA, extended layer
	 * can work well even attached layer does not enable.
	 */
	//if (!is_layer_across_each_pipe(crtc, configs))
	//	return false;

	return true;
}

static inline bool is_extended_over_limit(int ext_cnt)
{
	if (ext_cnt > 3)
		return true;
	return false;
}

/**
 * check if continuous ext layers are overlapped with each other
 * also need to check the below nearest phy layer
 * which these ext layers will be attached to
 * 1. check all ext layers, if overlapped with any one, change it to phy layer
 * 2. if more than 1 ext layer exist, need to check the phy layer
 */
static int is_continuous_ext_layer_overlap(struct drm_crtc *crtc,
		struct drm_mtk_layer_config *configs, int curr)
{
	int overlapped;
	struct drm_mtk_layer_config *src_info, *dst_info;
	int i;

	overlapped = 0;
	dst_info = &configs[curr];
	for (i = curr - 1; i >= 0; i--) {
		src_info = &configs[i];
		if (is_extended_layer(src_info)) {
			overlapped |= is_overlap_on_yaxis(src_info, dst_info);
			if (overlapped)
				break;
		} else {
			overlapped |= is_overlap_on_yaxis(src_info, dst_info);
			if (!is_extended_base_layer_valid(crtc, src_info, i))
				overlapped |= 1;
			break;
		}
	}
	return overlapped;
}

bool is_triple_disp(struct drm_mtk_layering_info *disp_info)
{
	if (disp_info->layer_num[HRT_PRIMARY] &&
		disp_info->layer_num[HRT_SECONDARY] &&
		disp_info->layer_num[HRT_THIRD])
		return true;
	return false;
}

static int get_phy_ovl_layer_cnt(struct drm_mtk_layering_info *info,
				int idx)
{
	int total_cnt = 0;
	int i;
	struct drm_mtk_layer_config *layer_info;

	if (info->layer_num[idx] > 0) {
		total_cnt = info->layer_num[idx];

		if (info->gles_head[idx] >= 0) {
			total_cnt -= info->gles_tail[idx] -
				     info->gles_head[idx];
		}

		if (get_layering_opt(LYE_OPT_EXT_LAYER)) {
			for (i = 0; i < info->layer_num[idx]; i++) {
				layer_info = &info->input_config[idx][i];
				if (is_extended_layer(layer_info) &&
				    !mtk_is_gles_layer(info, idx, i))
					total_cnt--;
			}
		}
	}
	return total_cnt;
}

static int get_phy_ovl_layer_cnt_after_gc(struct drm_mtk_layering_info *info,
				int disp_idx, int *layerset_head, int *layerset_tail)
{
	int total_cnt = 0;
	int i;
	struct drm_mtk_layer_config *layer_info;
	int j = 0;

	if (disp_idx >= LYE_CRTC || disp_idx < 0) {
		DDPPR_ERR("%s[%d]:disp_idx:%d\n", __func__, __LINE__, disp_idx);
		return -1;
	}

	if (info->layer_num[disp_idx] > 0) {
		total_cnt = info->layer_num[disp_idx];

		for (i = 0; i < info->layer_num[disp_idx]; i++) {
			layer_info = &info->input_config[disp_idx][i];
			if ((layer_info != NULL) &&
					(layer_info->layer_caps & MTK_HWC_INACTIVE_LAYER)) {
				if (j == 0) {
					*layerset_head = i;
					*layerset_tail = i;
					j++;
				}
				*layerset_tail = i;
				DDPDBG_BWM("%s:%d GPUC: Inactive layer:%d\n",
					__func__, __LINE__, i);
			}
		}

		if ((*layerset_tail) > (*layerset_head))
			total_cnt -= *layerset_tail - *layerset_head;

		if (get_layering_opt(LYE_OPT_EXT_LAYER)) {
			for (i = 0; i < info->layer_num[disp_idx]; i++) {
				layer_info = &info->input_config[disp_idx][i];
				if (is_extended_layer(layer_info) &&
				    !mtk_is_gles_layer(info, disp_idx, i) &&
					((i < *layerset_head) || (i > *layerset_tail)))
					total_cnt--;
			}
		}
		DDPDBG_BWM("%s:%d GPUC: total_cnt layer:%d layer set head:%d tail:%d\n",
				__func__, __LINE__, total_cnt, *layerset_head, *layerset_tail);
	}
	return total_cnt;
}

int mtk_get_phy_layer_limit(uint16_t layer_map_tb)
{
	int total_cnt = 0;
	int i;

	for (i = 0; i < 16; i++) {
		if (layer_map_tb & 0x1)
			total_cnt++;
		layer_map_tb >>= 1;
	}
	return total_cnt;
}

static int get_ovl_by_phy(struct drm_device *dev, int disp_idx, int disp_list,
			  uint16_t layer_map_tb, int phy_layer_idx, const char *caller)
{
	uint16_t ovl_mapping_tb, layer_map_tb_bak = layer_map_tb;
	int i, ovl_idx = 0, layer_idx = 0, phy_layer_idx_bak = phy_layer_idx;

	ovl_mapping_tb =
		l_rule_ops->get_mapping_table(dev, disp_idx, disp_list, DISP_HW_OVL_TB, 0);
	for (layer_idx = 0; layer_idx < MAX_PHY_OVL_CNT; layer_idx++) {
		if (layer_map_tb & 0x1) {
			if (phy_layer_idx == 0)
				break;
			phy_layer_idx--;
		}
		layer_map_tb >>= 1;
	}

	if (layer_idx > MAX_PHY_OVL_CNT) {
		DDPPR_ERR("%s %s fail, %u %u (layer_map_tb %x phy_lay_idx %u) phy_layer_idx:%d\n",
			__func__, caller, disp_idx, disp_list,
			layer_map_tb_bak, phy_layer_idx_bak, phy_layer_idx);
		return -1;
	}

	for (i = 0; i < layer_idx; i++) {
		if (ovl_mapping_tb & 0x1)
			ovl_idx++;
		ovl_mapping_tb >>= 1;
	}
#ifdef HRT_DEBUG_LEVEL2
	DDPMSG("%s,phy:%d,layer_tb:0x%x,L_idx:%d ovl_idx:%d, ov_tb:0x%x\n",
	       __func__, phy_layer_idx, layer_map_tb, layer_idx, ovl_idx,
	       ovl_mapping_tb);
#endif
	return ovl_idx;
}

static int get_phy_ovl_index(struct drm_device *dev, int disp_idx, int disp_list,
			     int layer_idx)
{
	uint16_t ovl_mapping_tb =
		l_rule_ops->get_mapping_table(dev, disp_idx, disp_list, DISP_HW_OVL_TB, 0);
	int phy_layer_cnt, layer_flag;

	phy_layer_cnt = 0;
	layer_flag = 1 << layer_idx;
	while (layer_idx) {
		layer_idx--;
		layer_flag >>= 1;
		if (ovl_mapping_tb & layer_flag)
			break;
		phy_layer_cnt++;
	}

	return phy_layer_cnt;
}

static int get_larb_by_ovl(struct drm_device *dev, int ovl_idx, int disp_idx)
{
	uint16_t larb_mapping_tb;
	int larb_idx;

	/* no need for disp_list */
	larb_mapping_tb = l_rule_ops->get_mapping_table(dev, disp_idx, 0,
							DISP_HW_LARB_TB, 0);
	larb_idx = (larb_mapping_tb >> ovl_idx * 4) & 0xF;

	return larb_idx;
}

static void dump_disp_info(struct drm_mtk_layering_info *disp_info,
			   enum DISP_DEBUG_LEVEL debug_level)
{
	int i = 0, j = 0, ret = 0;
	struct drm_mtk_layer_config *layer_info = NULL;

#define _HRT_FMT \
	"HRT hrt_num:0x%x/disp_idx:%x/disp_list:%x/mod:%d/dal:%d/addon_scn:(%d, %d, %d)/bd_tb:%d/i:%d\n"
#define _L_FMT \
	"L%d->%d/(%d,%d,%d,%d)/(%d,%d,%d,%d)/f0x%x/ds%d/e%d/cap0x%x" \
	"/compr%d/secure%d/frame:%u/weight:%u/force_gpu:%u/AID:0x%llx\n"

	layer_info = kzalloc(sizeof(struct drm_mtk_layer_config),
			GFP_KERNEL);

	if (!layer_info) {
		DDPPR_ERR("%s:%d NULL layer_info\n",
			__func__, __LINE__);
		return;
	}

	if (debug_level < DISP_DEBUG_LEVEL_INFO) {
		DDPMSG(_HRT_FMT,
			disp_info->hrt_num,
			disp_info->disp_idx,
			disp_info->disp_list,
			disp_info->disp_mode_idx[0],
			l_rule_info->dal_enable,
			l_rule_info->addon_scn[HRT_PRIMARY],
			l_rule_info->addon_scn[HRT_SECONDARY],
			l_rule_info->addon_scn[HRT_THIRD],
			l_rule_info->bound_tb_idx,
			roll_gpu_for_idle);

		for (i = 0; i < HRT_DISP_TYPE_NUM; i++) {
			if (disp_info->layer_num[i] <= 0)
				continue;
			else if (disp_info->layer_num[i] >= DISP_LAYER_RULE_MAX_NUM) {
				DDPPR_ERR("%s layer_num[%d] %d > DISP_LAYER_RULE_MAX_NUM\n",
						__func__, i, disp_info->layer_num[i]);
				continue;
			}

			DDPMSG("HRT D%d/M%d/LN%d/hrt:0x%x/G(%d,%d)/id%u\n", i,
			       disp_info->disp_mode[i], disp_info->layer_num[i],
			       disp_info->hrt_num, disp_info->gles_head[i],
			       disp_info->gles_tail[i], disp_info->hrt_idx);

			for (j = 0; j < disp_info->layer_num[i]; j++) {
				if (access_ok(&disp_info->input_config[i][j],
					sizeof(struct drm_mtk_layer_config))) {
					ret = copy_from_user(layer_info,
							&disp_info->input_config[i][j],
							sizeof(struct drm_mtk_layer_config));
					DDPMSG("%s layer_info copy_from_user ret %d\n",
						__func__, ret);
					if (ret != sizeof(struct drm_mtk_layer_config)) {
						DDPPR_ERR("%s ret %d\n", __func__, ret);
						break;
					}
				} else if (disp_info->input_config[i])
					memcpy(layer_info, &disp_info->input_config[i][j],
						sizeof(struct drm_mtk_layer_config));

				if (layer_info == NULL) {
					DDPPR_ERR("%s[%d]:layer_info = null\n", __func__, __LINE__);
					return;
				}

				DDPMSG(_L_FMT, j, layer_info->ovl_id,
				       layer_info->src_offset_x,
				       layer_info->src_offset_y,
				       layer_info->src_width,
				       layer_info->src_height,
				       layer_info->dst_offset_x,
				       layer_info->dst_offset_y,
				       layer_info->dst_width,
				       layer_info->dst_height,
				       layer_info->src_fmt,
				       layer_info->dataspace,
				       layer_info->ext_sel_layer,
				       layer_info->layer_caps,
				       layer_info->compress,
				       layer_info->secure,
				       disp_info->frame_idx[i],
				       layer_info->layer_hrt_weight,
				       layer_info->wcg_force_gpu,
				       layer_info->buffer_alloc_id);
			}
		}
	} else {
		DDPINFO(_HRT_FMT, disp_info->hrt_num, disp_info->disp_idx, disp_info->disp_list,
			disp_info->disp_mode_idx[0],
			l_rule_info->dal_enable,
			l_rule_info->addon_scn[HRT_PRIMARY],
			l_rule_info->addon_scn[HRT_SECONDARY],
			l_rule_info->addon_scn[HRT_THIRD],
			l_rule_info->bound_tb_idx,
			roll_gpu_for_idle);

		for (i = 0; i < HRT_DISP_TYPE_NUM; i++) {
			if (disp_info->layer_num[i] <= 0)
				continue;
			else if (disp_info->layer_num[i] >= DISP_LAYER_RULE_MAX_NUM) {
				DDPPR_ERR("%s layer_num[%d] %d > DISP_LAYER_RULE_MAX_NUM\n",
						__func__, i, disp_info->layer_num[i]);
				continue;
			}

			DDPINFO("HRT D%d/M%d/LN%d/hrt:0x%x/G(%d,%d)/CM%d/id%u\n", i,
				disp_info->disp_mode[i],
				disp_info->layer_num[i], disp_info->hrt_num,
				disp_info->gles_head[i],
				disp_info->gles_tail[i],
				disp_info->color_mode[i],
				disp_info->hrt_idx);

			for (j = 0; j < disp_info->layer_num[i]; j++) {
				if (access_ok(&disp_info->input_config[i][j],
					sizeof(struct drm_mtk_layer_config))) {
					ret = copy_from_user(layer_info,
							&disp_info->input_config[i][j],
							sizeof(struct drm_mtk_layer_config));
					DDPINFO("%s layer_info copy_from_user ret %d\n",
						__func__, ret);
					if (ret != sizeof(struct drm_mtk_layer_config)) {
						DDPPR_ERR("%s ret %d\n", __func__, ret);
						break;
					}
				} else if (disp_info->input_config[i])
					memcpy(layer_info, &disp_info->input_config[i][j],
						sizeof(struct drm_mtk_layer_config));

				if (layer_info == NULL) {
					DDPPR_ERR("%s[%d]:layer_info = null\n", __func__, __LINE__);
					break;
				}

				DDPINFO(_L_FMT, j, layer_info->ovl_id,
					layer_info->src_offset_x,
					layer_info->src_offset_y,
					layer_info->src_width,
					layer_info->src_height,
					layer_info->dst_offset_x,
					layer_info->dst_offset_y,
					layer_info->dst_width,
					layer_info->dst_height,
					layer_info->src_fmt,
					layer_info->dataspace,
					layer_info->ext_sel_layer,
					layer_info->layer_caps,
					layer_info->compress,
					layer_info->secure,
					disp_info->frame_idx[i],
					layer_info->layer_hrt_weight,
					layer_info->wcg_force_gpu,
					layer_info->buffer_alloc_id);

			}
		}
	}

	kfree(layer_info);
	layer_info = NULL;
}

static void dump_disp_caps_info(struct drm_device *dev,
					struct drm_mtk_layering_info *disp_info, const int line)
{
	int j;
	unsigned int disp_idx = 0;
	struct mtk_drm_private *priv = dev->dev_private;

	if (priv->data->mmsys_id != MMSYS_MT6878)
		return;

	if (get_layering_opt(LYE_OPT_SPHRT))
		disp_idx = disp_info->disp_idx;

	for (; disp_idx < HRT_DISP_TYPE_NUM ; disp_idx++) {
		if (disp_info->layer_num[disp_idx] <= 0)
			continue;

		for (j = 0; j < disp_info->layer_num[disp_idx]; j++) {
			struct drm_mtk_layer_config *c = &disp_info->input_config[disp_idx][j];

			if (MTK_MML_OVL_LAYER & c->layer_caps)
				DDPINFO("%s, %d, idx=%d, j=%d, cap=0x%x\n", __func__, line, disp_idx, j, c->layer_caps);
		}
	}
}

static void check_gles_change(struct debug_gles_range *dbg_gles, const int line, const bool print)
{
	if (dbg_gles->head != layering_info.gles_head[0] ||
	    dbg_gles->tail != layering_info.gles_tail[0]) {
		dbg_gles->head = layering_info.gles_head[0];
		dbg_gles->tail = layering_info.gles_tail[0];
		dbg_gles->written +=
		    scnprintf(dbg_gles->msg + dbg_gles->written, 256 - dbg_gles->written,
			      "(%d:%d,%d)", line, dbg_gles->head, dbg_gles->tail);
	}
	if (print && dbg_gles->written)
		DDPINFO("%s:%s\n", __func__, dbg_gles->msg);
}

static void dump_disp_trace(struct drm_mtk_layering_info *disp_info)
{
#define LEN 1000
	int i, j;
	struct drm_mtk_layer_config *c;
	char msg[LEN];
	int n = 0;

	for (i = 0; i < HRT_DISP_TYPE_NUM; i++) {
		if (disp_info->layer_num[i] <= 0)
			continue;
		if (n >= LEN)
			break;
		n = snprintf(msg, LEN, "D%d,ovp:%d,dal:%d,LN:%d,G(%d,%d)",
			     i, disp_info->hrt_weight, l_rule_info->dal_enable,
			     disp_info->layer_num[i], disp_info->gles_head[i],
			     disp_info->gles_tail[i]);

		if (n < 0 || n >= LEN) {
			DDPPR_ERR("unknown error, n:%d\n", n);
		} else {
			for (j = 0; j < disp_info->layer_num[i]; j++) {
				if (n >= LEN)
					break;
				c = &disp_info->input_config[i][j];
				n += snprintf(msg + n, LEN - n,
						  "|L%d->%d(%u,%u,%ux%u),f:0x%x,c:%d",
						  j, c->ovl_id, c->dst_offset_x,
						  c->dst_offset_y, c->dst_width,
						  c->dst_height, c->src_fmt, c->compress);
			}

			trace_layer_layout(msg);
		}
	}
}

static void
print_disp_info_to_log_buffer(struct drm_mtk_layering_info *disp_info)
{
/*Todo: support fix log buffer*/
#ifdef IF_ZERO
	char *status_buf;
	int i, j, n;
	struct drm_mtk_layer_config *layer_info;

	status_buf = get_dprec_status_ptr(0);
	if (status_buf == NULL)
		return;

	n = 0;
	n += snprintf(status_buf + n, LOGGER_BUFFER_SIZE - n,
		"Last hrt query data[start]\n");
	for (i = 0; i < 2; i++) {
		if (n < LOGGER_BUFFER_SIZE) {
			n += snprintf(status_buf + n, LOGGER_BUFFER_SIZE - n,
				"HRT D%d/M%d/LN%d/hrt_num:%d/G(%d,%d)/fps:%d\n",
				i, disp_info->disp_mode[i], disp_info->layer_num[i],
				disp_info->hrt_num, disp_info->gles_head[i],
				disp_info->gles_tail[i], l_rule_info->primary_fps);

			for (j = 0; j < disp_info->layer_num[i]; j++) {
				layer_info = &disp_info->input_config[i][j];
				n += snprintf(status_buf + n, LOGGER_BUFFER_SIZE - n,
				"L%d->%d/of(%d,%d)/wh(%d,%d)/fmt:0x%x/compr:%u\n",
					j, layer_info->ovl_id,
					layer_info->dst_offset_x,
					layer_info->dst_offset_y,
					layer_info->dst_width,
					layer_info->dst_height,
					layer_info->src_fmt,
					layer_info->compress);
			}
		} else
			DDPPR_ERR("%s[%d]:%d, %d\n", __func__, __LINE__, n, i);
	}

	if (n < LOGGER_BUFFER_SIZE)
		n += snprintf(status_buf + n, LOGGER_BUFFER_SIZE - n,
			"Last hrt query data[end]\n");
	else
		DDPPR_ERR("%s[%d]:%d\n", __func__, __LINE__, n);
#endif
}

void mtk_rollback_layer_to_GPU(struct drm_mtk_layering_info *disp_info,
			       int idx, int i)
{
	if (idx < 0 || idx >= LYE_CRTC) {
		DDPPR_ERR("%s, idx is invalid\n", __func__);
		return;
	}

	if (mtk_is_layer_id_valid(disp_info, idx, i) == false)
		return;

	mtk_gles_incl_layer(disp_info, idx, i);

	disp_info->input_config[idx][i].ext_sel_layer = -1;
}

/* rollback and set NO_FBDC flag */
void mtk_rollback_compress_layer_to_GPU(struct drm_mtk_layering_info *disp_info,
					int idx, int i)
{

	if (idx >= LYE_CRTC || idx < 0) {
		DDPPR_ERR("%s[%d]:idx:%d\n", __func__, __LINE__, idx);
		return;
	}

	if (mtk_is_layer_id_valid(disp_info, idx, i) == false)
		return;

	if (disp_info->input_config[idx][i].compress == 0)
		return;

	mtk_rollback_layer_to_GPU(disp_info, idx, i);
	disp_info->input_config[idx][i].layer_caps |= MTK_NO_FBDC;
}

int mtk_rollback_resize_layer_to_GPU_range(
	struct drm_mtk_layering_info *disp_info, int idx, int start_l_idx,
	int end_l_idx)
{
	int i;
	struct drm_mtk_layer_config *lc;

	if (idx >= LYE_CRTC || idx < 0) {
		DDPPR_ERR("%s[%d]:idx:%d\n", __func__, __LINE__, idx);
		return 0;
	}

	if (disp_info->layer_num[idx] <= 0) {
		/* direct skip */
		return 0;
	}

	if (start_l_idx < 0 || end_l_idx >= disp_info->layer_num[idx])
		return -EINVAL;

	for (i = start_l_idx; i <= end_l_idx; i++) {
		lc = &disp_info->input_config[idx][i];
		if ((lc->src_height != lc->dst_height) ||
		    (lc->src_width != lc->dst_width)) {
			if (mtk_has_layer_cap(lc, MTK_MDP_RSZ_LAYER | MTK_DISP_RSZ_LAYER))
				continue;
			if (mtk_has_layer_cap(lc, DISP_MML_CAPS_MASK))
				continue;

			mtk_gles_incl_layer(disp_info, idx, i);
		}
	}

	if (disp_info->gles_head[idx] != -1) {
		for (i = disp_info->gles_head[idx];
		     i <= disp_info->gles_tail[idx]; i++) {
			lc = &disp_info->input_config[idx][i];
			lc->ext_sel_layer = -1;
		}
	}

	return 0;
}

int mtk_rollback_all_resize_layer_to_GPU(
	struct drm_mtk_layering_info *disp_info, int idx)
{
	if (disp_info->layer_num[idx] <= 0)
		return 0;

	mtk_rollback_resize_layer_to_GPU_range(disp_info, idx, 0, disp_info->layer_num[idx] - 1);
	return 0;
}

static int _rollback_to_GPU_bottom_up(struct drm_mtk_layering_info *info,
				      int idx, int ovl_limit)
{
	int available_ovl_num, i, j;
	struct drm_mtk_layer_config *l_info;

	available_ovl_num = ovl_limit;
	for (i = 0; i < info->layer_num[idx]; i++) {
		l_info = &info->input_config[idx][i];
		if (is_extended_layer(l_info))
			continue;
		available_ovl_num--;

		if (mtk_is_gles_layer(info, idx, i)) {
			info->gles_head[idx] = i;
			if (info->gles_tail[idx] == -1) {
				info->gles_tail[idx] = i;
				for (j = i + 1; j < info->layer_num[idx];
				     j++) {
					l_info = &info->input_config[idx][j];
					if (is_extended_layer(l_info))
						info->gles_tail[idx] = j;
					else
						break;
				}
			}
			break;
		} else if (available_ovl_num <= 0) {
			available_ovl_num = 0;
			info->gles_head[idx] = i;
			info->gles_tail[idx] = info->layer_num[idx] - 1;
			if (info->gles_head[idx] == info->gles_tail[idx]) {
				info->gles_head[idx] = -1;
				info->gles_tail[idx] = -1;
			}
			break;
		}
	}

	if (available_ovl_num < 0)
		DDPPR_ERR("%s available_ovl_num invalid:%d\n", __func__,
			  available_ovl_num);

	return available_ovl_num;
}

static int _rollback_to_GPU_top_down(struct drm_mtk_layering_info *disp_info,
				     int idx, int ovl_limit)
{
	int available_ovl_num, i;
	int tmp_ext = -1;
	struct drm_mtk_layer_config *layer_info;

	available_ovl_num = ovl_limit;
	for (i = disp_info->layer_num[idx] - 1; i > disp_info->gles_tail[idx];
	     i--) {
		layer_info = &disp_info->input_config[idx][i];
		if (!is_extended_layer(layer_info)) {
			if (mtk_is_gles_layer(disp_info, idx, i))
				break;
			if (available_ovl_num <= 0) {
				available_ovl_num = 0;
				if (tmp_ext == -1)
					disp_info->gles_tail[idx] = i;
				else
					disp_info->gles_tail[idx] = tmp_ext;
				break;
			}
			tmp_ext = -1;
			available_ovl_num--;
		} else {
			if (tmp_ext == -1)
				tmp_ext = i;
		}
	}

	if (available_ovl_num < 0)
		DDPPR_ERR("%s available_ovl_num invalid:%d\n", __func__,
			  available_ovl_num);

	return available_ovl_num;
}

static int rollback_to_GPU(struct drm_mtk_layering_info *info, int idx,
			   int available)
{
	int available_ovl_num, i;
	bool has_gles_layer = false;
	struct drm_mtk_layer_config *l_info;

	available_ovl_num = available;

	if (info->gles_head[idx] != -1)
		has_gles_layer = true;

	available_ovl_num =
		_rollback_to_GPU_bottom_up(info, idx, available_ovl_num);
	if (has_gles_layer)
		available_ovl_num = _rollback_to_GPU_top_down(
			info, idx, available_ovl_num);

	if (info->gles_head[idx] == -1 && info->gles_tail[idx] == -1)
		goto out;

	if (mtk_is_layer_id_valid(info, idx, info->gles_head[idx]) == false) {
		dump_disp_info(info, DISP_DEBUG_LEVEL_CRITICAL);
		DDPAEE("invalid gles_head:%d, aval:%d\n",
			  info->gles_head[idx], available);
		goto out;
	}

	if (mtk_is_layer_id_valid(info, idx, info->gles_tail[idx]) == false) {
		dump_disp_info(info, DISP_DEBUG_LEVEL_CRITICAL);
		DDPAEE("invalid gles_tail:%d, aval:%d\n",
			  info->gles_tail[idx], available);
		goto out;
	}

	/* Clear extended layer for all GLES layer */
	for (i = info->gles_head[idx]; i <= info->gles_tail[idx]; i++) {
		l_info = &info->input_config[idx][i];
		l_info->ext_sel_layer = -1;
		if (mtk_has_layer_cap(l_info, MTK_DISP_RSZ_LAYER)) {
			l_info->layer_caps &= ~MTK_DISP_RSZ_LAYER;
		}
	}

	if (info->gles_tail[idx] + 1 < info->layer_num[idx]) {
		l_info = &info->input_config[idx][info->gles_tail[idx] + 1];
		if (is_extended_layer(l_info))
			l_info->ext_sel_layer = -1;
	}

out:
	return available_ovl_num;
}

static bool unset_disp_rsz_attr(struct drm_mtk_layering_info *disp_info,
				int idx)
{
	struct drm_mtk_layer_config *c;

	if (idx >= LYE_CRTC || idx < 0) {
		DDPPR_ERR("%s[%d]:idx:%d\n", __func__, __LINE__, idx);
		return 0;
	}

	c = &disp_info->input_config[idx][0];

	if (l_rule_info->addon_scn[HRT_PRIMARY] == ONE_SCALING &&
	    mtk_has_layer_cap(c, MTK_MDP_RSZ_LAYER) &&
	    mtk_has_layer_cap(c, MTK_DISP_RSZ_LAYER)) {
		c->layer_caps &= ~MTK_DISP_RSZ_LAYER;
		l_rule_info->addon_scn[HRT_PRIMARY] = NONE;
		return true;
	}
	return false;
}

static int _filter_by_ovl_cnt(struct drm_device *dev,
			      struct drm_mtk_layering_info *disp_info,
			      int disp_idx)
{
	int ovl_num_limit, phy_ovl_cnt;
	int idx = disp_idx;
	uint16_t l_tb;
	int phy_ovl_cnt_after_gc = 0;
	int layerset_head = -1;
	int layerset_tail = -1;

	if (get_layering_opt(LYE_OPT_SPHRT))
		idx = 0;

	if (disp_info->layer_num[idx] <= 0) {
		/* direct skip */
		return 0;
	}

retry:
	phy_ovl_cnt = get_phy_ovl_layer_cnt(disp_info, idx);
	if (get_layering_opt(LYE_OPT_GPU_CACHE) && (disp_idx == HRT_PRIMARY))
		phy_ovl_cnt_after_gc = get_phy_ovl_layer_cnt_after_gc(disp_info, disp_idx,
		&layerset_head, &layerset_tail);
	l_tb = l_rule_ops->get_mapping_table(dev, disp_idx, disp_info->disp_list, DISP_HW_LAYER_TB,
					     MAX_PHY_OVL_CNT);

	ovl_num_limit = mtk_get_phy_layer_limit(l_tb);
	if (disp_idx == 0 && l_rule_info->dal_enable)
		ovl_num_limit--;

#ifdef HRT_DEBUG_LEVEL2
	DDPMSG("phy_ovl_cnt:%d,ovl_n_limit:%d\n", phy_ovl_cnt, ovl_num_limit);
#endif

	if (get_layering_opt(LYE_OPT_GPU_CACHE) && (disp_idx == HRT_PRIMARY)) {
		if ((layerset_head != -1) && (layerset_tail != -1) &&
			(phy_ovl_cnt_after_gc <= ovl_num_limit)) {
			DDPDBG_BWM("GPUC: %s:%d after gpu cache not hrt bound\n",
				__func__, __LINE__);
			DDPDBG_BWM("GPUC: %s:%d head;%d tail:%d phy_ovl_cnt:%d limition:%d\n",
				__func__, __LINE__, layerset_head, layerset_tail,
				phy_ovl_cnt_after_gc, ovl_num_limit);
			if (phy_ovl_cnt > ovl_num_limit)
				need_rollback_to_gpu_before_gpuc = true;
			else
				need_rollback_to_gpu_before_gpuc = false;
			return 0;
		}

		DDPDBG_BWM("GPUC: %s:%d Default HRT or After gpu cache be hrt bound\n",
			__func__, __LINE__);
		DDPDBG_BWM("GPUC: %s:%d head;%d tail:%d phy_ovl_cnt:%d limition:%d\n",
			__func__, __LINE__, layerset_head, layerset_tail,
			phy_ovl_cnt_after_gc, ovl_num_limit);
	}

	if (phy_ovl_cnt <= ovl_num_limit)
		return 0;

	if (unset_disp_rsz_attr(disp_info, idx))
		goto retry;

	rollback_to_GPU(disp_info, idx, ovl_num_limit);
	return 0;
}

static void ext_id_adjustment_and_retry(struct drm_device *dev,
					struct drm_mtk_layering_info *info,
					int disp_idx, int layer_idx)
{
	int j, ext_idx;
	int idx = disp_idx;
	struct drm_mtk_layer_config *layer_info;

	if (get_layering_opt(LYE_OPT_SPHRT))
		idx = 0;

	ext_idx = -1;
	for (j = layer_idx; j < layer_idx + 3; j++) {
		layer_info = &info->input_config[idx][j];

		if (ext_idx == -1) {
			layer_info->ext_sel_layer = -1;
			if (is_extended_base_layer_valid(NULL, layer_info, j))
				ext_idx = j;
		} else {
			layer_info->ext_sel_layer = ext_idx;
		}
		if (j == (info->layer_num[idx] - 1) ||
		    !is_extended_layer(&info->input_config[idx][j + 1]))
			break;
	}
#ifdef HRT_DEBUG_LEVEL2
	DDPMSG("[%s]cannot feet current layer layout\n", __func__);
	dump_disp_info(info, DISP_DEBUG_LEVEL_ERR);
#endif
	ext_id_tuning(dev, info, disp_idx);
}

static int ext_id_tuning(struct drm_device *dev,
			  struct drm_mtk_layering_info *info, int disp_idx)
{
	uint16_t ovl_tb, l_tb;
	int phy_ovl_cnt, i;
	int ext_cnt = 0, cur_phy_cnt = 0;
	int idx = disp_idx;
	struct drm_mtk_layer_config *layer_info;

	if (get_layering_opt(LYE_OPT_SPHRT))
		idx = 0;

	if (info->layer_num[idx] <= 0) {
		/* direct skip */
		return 0;
	}

	_filter_by_ovl_cnt(dev, info, disp_idx);
	phy_ovl_cnt = get_phy_ovl_layer_cnt(info, idx);
	if (phy_ovl_cnt > MAX_PHY_OVL_CNT) {
		DDPPR_ERR("phy_ovl_cnt(%d) over OVL count limit\n",
			  phy_ovl_cnt);
		phy_ovl_cnt = MAX_PHY_OVL_CNT;
	}

	ovl_tb = l_rule_ops->get_mapping_table(dev, disp_idx, info->disp_list, DISP_HW_OVL_TB, 0);
	l_tb = l_rule_ops->get_mapping_table(dev, disp_idx, info->disp_list, DISP_HW_LAYER_TB,
					     phy_ovl_cnt);

	if (l_rule_info->dal_enable) {
		l_tb = l_rule_ops->get_mapping_table(
			dev, disp_idx, info->disp_list, DISP_HW_LAYER_TB, MAX_PHY_OVL_CNT);
		l_tb &= HRT_AEE_LAYER_MASK;
	}

	for (i = 0; i < info->layer_num[idx]; i++) {
		layer_info = &info->input_config[idx][i];
		if (is_extended_layer(layer_info)) {
			ext_cnt++;
			if (is_extended_over_limit(ext_cnt)) {
				ext_id_adjustment_and_retry(dev, info, disp_idx, i);
				break;
			}
		} else {
#ifdef HRT_DEBUG_LEVEL2
			DDPMSG("i:%d, cur_phy_cnt:%d\n", i, cur_phy_cnt);
#endif
			if (mtk_is_gles_layer(info, idx, i) &&
			    (i != info->gles_head[idx])) {
#ifdef HRT_DEBUG_LEVEL2
				DDPMSG("is gles layer, continue\n");
#endif
				continue;
			}
			if (cur_phy_cnt > 0) {
				int cur_ovl, pre_ovl;

				cur_ovl = get_ovl_by_phy(dev, disp_idx, info->disp_list, l_tb,
							 cur_phy_cnt, __func__);
				pre_ovl = get_ovl_by_phy(dev, disp_idx, info->disp_list, l_tb,
							 cur_phy_cnt - 1, __func__);
				if (cur_ovl != pre_ovl)
					ext_cnt = 0;
			}
			cur_phy_cnt++;
		}
	}

	return 0;
}

static int rollback_all_to_GPU(struct drm_mtk_layering_info *disp_info,
			       int idx)
{
	if (disp_info->layer_num[idx] <= 0) {
		/* direct skip */
		return 0;
	}

	disp_info->gles_head[idx] = 0;
	disp_info->gles_tail[idx] = disp_info->layer_num[idx] - 1;
	return 0;
}

static int filter_by_ovl_cnt(struct drm_device *dev,
			     struct drm_mtk_layering_info *disp_info)
{
	int ret, disp_idx;

	if (get_layering_opt(LYE_OPT_SPHRT))
		disp_idx = disp_info->disp_idx;
	else
		disp_idx = 0;

	/* 0->primary display, 1->secondary display */
	for (; disp_idx < HRT_DISP_TYPE_NUM; disp_idx++) {
		if (disp_idx == 0 && get_layering_opt(LYE_OPT_EXT_LAYER))
			ret = ext_id_tuning(dev, disp_info, disp_idx);
		else
			ret = _filter_by_ovl_cnt(dev, disp_info, disp_idx);

		if (get_layering_opt(LYE_OPT_SPHRT))
			break;
	}

#ifdef HRT_DEBUG_LEVEL2
	DDPMSG("[%s result]\n", __func__);
	dump_disp_info(disp_info, DISP_DEBUG_LEVEL_INFO);
#endif
	return ret;
}

static struct hrt_sort_entry *x_entry_list, *y_entry_list;
static struct hrt_sort_entry *x_entry_list_for_compare, *y_entry_list_for_compare;

#ifdef HRT_DEBUG_LEVEL2
static int dump_entry_list(bool sort_by_y)
{
	struct hrt_sort_entry *temp;
	struct drm_mtk_layer_config *layer_info;

	if (sort_by_y)
		temp = y_entry_list;
	else
		temp = x_entry_list;

	DDPMSG("%s, sort_by_y:%d, addr:0x%p\n", __func__, sort_by_y, temp);
	while (temp) {
		layer_info = temp->layer_info;
		DDPMSG("key:%d, offset(%d, %d), w/h(%d, %d), overlap_w:%d\n",
		       temp->key, layer_info->dst_offset_x,
		       layer_info->dst_offset_y, layer_info->dst_width,
		       layer_info->dst_height, temp->overlap_w);
		temp = temp->tail;
	}
	DDPMSG("%s end\n", __func__);
	return 0;
}
#endif

static int dump_entry_list_for_compare(bool sort_by_y)
{
	struct hrt_sort_entry *temp;
	struct drm_mtk_layer_config *layer_info;

	if (!g_hrt_by_larb_debug)
		return 0;

	if (sort_by_y)
		temp = y_entry_list_for_compare;
	else
		temp = x_entry_list_for_compare;

	DDPDBG_HBL("%s, sort_by_y:%d, addr:0x%p\n", __func__, sort_by_y, temp);
	while (temp) {
		layer_info = temp->layer_info;
		DDPDBG_HBL("key:%d, offset(%d, %d), w/h(%d, %d), overlap_w:%d, ID:0x%llx\n",
		       temp->key, layer_info->dst_offset_x,
		       layer_info->dst_offset_y, layer_info->dst_width,
		       layer_info->dst_height, temp->overlap_w, layer_info->buffer_alloc_id);
		temp = temp->tail;
	}
	return 0;
}

static void update_layer_hrt_after_scan(struct drm_mtk_layering_info *disp_info,
		unsigned int disp, __u64 max_layer_id)
{
	struct hrt_sort_entry *temp;
	struct drm_mtk_layer_config *layer_info;
	bool clr_neg = true;
	int i = 0;

	if (!g_hrt_by_larb_debug || IS_ERR_OR_NULL(disp_info))
		return;

	temp = y_entry_list_for_compare;

	dump_entry_list_for_compare(1);

	while (temp) {
		if (clr_neg && temp->overlap_w < 0 && temp->layer_info) {
			for (i = 0; i < disp_info->layer_num[disp]; i++) {
				layer_info = &disp_info->input_config[disp][i];
				if (layer_info->buffer_alloc_id ==
					temp->layer_info->buffer_alloc_id) {
					DDPDBG_HBL(
						"%s crtc:%u,-neg:%d,w:%u,off(%d,%d),w/h(%d,%d),ID:0x%llx,gles(%d,%d)\n",
						__func__, disp_info->disp_idx, clr_neg,
						layer_info->layer_hrt_weight,
						layer_info->dst_offset_x,
						layer_info->dst_offset_y,
						layer_info->dst_width,
						layer_info->dst_height,
						layer_info->buffer_alloc_id,
						disp_info->gles_head[disp],
						disp_info->gles_tail[disp]);
					layer_info->layer_hrt_weight = 0;
					break;
				}
			}
		} else if (!clr_neg && temp->overlap_w > 0 && temp->layer_info) {
			for (i = 0; i < disp_info->layer_num[disp]; i++) {
				layer_info = &disp_info->input_config[disp][i];
				if (layer_info->buffer_alloc_id ==
					temp->layer_info->buffer_alloc_id) {
					DDPDBG_HBL(
						"%s crtc:%u,-pos:%d,w:%u,off(%d,%d),w/h(%d,%d),ID:0x%llx,gles(%d,%d)\n",
						__func__, disp_info->disp_idx, clr_neg,
						layer_info->layer_hrt_weight,
						layer_info->dst_offset_x,
						layer_info->dst_offset_y,
						layer_info->dst_width,
						layer_info->dst_height,
						layer_info->buffer_alloc_id,
						disp_info->gles_head[disp],
						disp_info->gles_tail[disp]);
					layer_info->layer_hrt_weight = 0;
					break;
				}
			}
		}

		if (temp->layer_info &&
			temp->layer_info->buffer_alloc_id == max_layer_id &&
			temp->overlap_w >= 0)
			clr_neg = false;
		temp = temp->tail;
	}
}

static int insert_entry(struct hrt_sort_entry **head,
			struct hrt_sort_entry *sort_entry)
{
	struct hrt_sort_entry *temp;

	temp = *head;
	while (temp) {
		if (sort_entry->key < temp->key ||
		    ((sort_entry->key == temp->key) &&
		     (sort_entry->overlap_w > 0))) {
			sort_entry->head = temp->head;
			sort_entry->tail = temp;
			if (temp->head != NULL)
				temp->head->tail = sort_entry;
			else
				*head = sort_entry;
			temp->head = sort_entry;
			break;
		}

		if (temp->tail == NULL) {
			temp->tail = sort_entry;
			sort_entry->head = temp;
			sort_entry->tail = NULL;
			break;
		}
		temp = temp->tail;
	}

	return 0;
}

static int add_layer_entry_for_compare(struct drm_mtk_layer_config *l_info, bool sort_by_y,
			   int overlap_w)
{
	struct hrt_sort_entry *begin_t, *end_t;
	struct hrt_sort_entry **p_entry;

	begin_t = kzalloc(sizeof(struct hrt_sort_entry), GFP_KERNEL);
	end_t = kzalloc(sizeof(struct hrt_sort_entry), GFP_KERNEL);

	begin_t->head = NULL;
	begin_t->tail = NULL;
	end_t->head = NULL;
	end_t->tail = NULL;
	if (sort_by_y) {
		begin_t->key = l_info->dst_offset_y;
		end_t->key = l_info->dst_offset_y + l_info->dst_height - 1;
		p_entry = &y_entry_list_for_compare;
	} else {
		begin_t->key = l_info->dst_offset_x;
		end_t->key = l_info->dst_offset_x + l_info->dst_width - 1;
		p_entry = &x_entry_list_for_compare;
	}

	begin_t->overlap_w = overlap_w;
	begin_t->layer_info = l_info;
	end_t->overlap_w = -overlap_w;
	end_t->layer_info = l_info;

	if (*p_entry == NULL) {
		*p_entry = begin_t;
		begin_t->head = NULL;
		begin_t->tail = end_t;
		end_t->head = begin_t;
		end_t->tail = NULL;
	} else {
		/* Inser begin entry */
		insert_entry(p_entry, begin_t);
#ifdef HRT_DEBUG_LEVEL2
		DDPMSG("Insert key:%d\n", begin_t->key);
		dump_entry_list_for_compare(sort_by_y);
#endif
		/* Inser end entry */
		insert_entry(p_entry, end_t);
#ifdef HRT_DEBUG_LEVEL2
		DDPMSG("Insert key:%d\n", end_t->key);
		dump_entry_list_for_compare(sort_by_y);
#endif
	}

	return 0;
}

static int add_layer_entry(struct drm_mtk_layer_config *l_info, bool sort_by_y,
			   int overlap_w)
{
	struct hrt_sort_entry *begin_t, *end_t;
	struct hrt_sort_entry **p_entry;

	begin_t = kzalloc(sizeof(struct hrt_sort_entry), GFP_KERNEL);
	end_t = kzalloc(sizeof(struct hrt_sort_entry), GFP_KERNEL);

	begin_t->head = NULL;
	begin_t->tail = NULL;
	end_t->head = NULL;
	end_t->tail = NULL;
	if (sort_by_y) {
		begin_t->key = l_info->dst_offset_y;
		end_t->key = l_info->dst_offset_y + l_info->dst_height - 1;
		p_entry = &y_entry_list;
	} else {
		begin_t->key = l_info->dst_offset_x;
		end_t->key = l_info->dst_offset_x + l_info->dst_width - 1;
		p_entry = &x_entry_list;
	}

	begin_t->overlap_w = overlap_w;
	begin_t->layer_info = l_info;
	end_t->overlap_w = -overlap_w;
	end_t->layer_info = l_info;

	if (*p_entry == NULL) {
		*p_entry = begin_t;
		begin_t->head = NULL;
		begin_t->tail = end_t;
		end_t->head = begin_t;
		end_t->tail = NULL;
	} else {
		/* Inser begin entry */
		insert_entry(p_entry, begin_t);
#ifdef HRT_DEBUG_LEVEL2
		DDPMSG("Insert key:%d\n", begin_t->key);
		dump_entry_list(sort_by_y);
#endif
		/* Inser end entry */
		insert_entry(p_entry, end_t);
#ifdef HRT_DEBUG_LEVEL2
		DDPMSG("Insert key:%d\n", end_t->key);
		dump_entry_list(sort_by_y);
#endif
	}

	return 0;
}

static int remove_layer_entry_for_compare(struct drm_mtk_layer_config *layer_info,
			      bool sort_by_y)
{
	struct hrt_sort_entry *temp, *free_entry;

	if (sort_by_y)
		temp = y_entry_list_for_compare;
	else
		temp = x_entry_list_for_compare;

	while (temp) {
		if (temp->layer_info == layer_info) {
			free_entry = temp;
			temp = temp->tail;
			if (free_entry->head == NULL) {
				/* Free head entry */
				if (temp != NULL)
					temp->head = NULL;
				if (sort_by_y)
					y_entry_list_for_compare = temp;
				else
					x_entry_list_for_compare = temp;
				kfree(free_entry);
			} else {
				free_entry->head->tail = free_entry->tail;
				if (temp)
					temp->head = free_entry->head;
				kfree(free_entry);
			}
		} else {
			temp = temp->tail;
		}
	}
	return 0;
}


static int remove_layer_entry(struct drm_mtk_layer_config *layer_info,
			      bool sort_by_y)
{
	struct hrt_sort_entry *temp, *free_entry;

	if (sort_by_y)
		temp = y_entry_list;
	else
		temp = x_entry_list;

	while (temp) {
		if (temp->layer_info == layer_info) {
			free_entry = temp;
			temp = temp->tail;
			if (free_entry->head == NULL) {
				/* Free head entry */
				if (temp != NULL)
					temp->head = NULL;
				if (sort_by_y)
					y_entry_list = temp;
				else
					x_entry_list = temp;
				kfree(free_entry);
			} else {
				free_entry->head->tail = free_entry->tail;
				if (temp)
					temp->head = free_entry->head;
				kfree(free_entry);
			}
		} else {
			temp = temp->tail;
		}
	}
	return 0;
}

static int free_all_layer_entry_for_compare(bool sort_by_y)
{
	struct hrt_sort_entry *cur_entry, *next_entry;

	if (sort_by_y)
		cur_entry = y_entry_list_for_compare;
	else
		cur_entry = x_entry_list_for_compare;

	while (cur_entry) {
		next_entry = cur_entry->tail;
		kfree(cur_entry);
		cur_entry = next_entry;
	}

	if (sort_by_y)
		y_entry_list_for_compare = NULL;
	else
		x_entry_list_for_compare = NULL;

	return 0;
}

static int free_all_layer_entry(bool sort_by_y)
{
	struct hrt_sort_entry *cur_entry, *next_entry;

	if (sort_by_y)
		cur_entry = y_entry_list;
	else
		cur_entry = x_entry_list;

	while (cur_entry) {
		next_entry = cur_entry->tail;
		kfree(cur_entry);
		cur_entry = next_entry;
	}

	if (sort_by_y)
		y_entry_list = NULL;
	else
		x_entry_list = NULL;

	return 0;
}

static int scan_x_overlap_for_compare(struct drm_mtk_layering_info *disp_info,
			  int disp_index, int ovl_overlap_limit_w)
{
	struct hrt_sort_entry *tmp_entry;
	int overlap_w_sum, max_overlap;

	overlap_w_sum = 0;
	max_overlap = 0;
	tmp_entry = x_entry_list_for_compare;
	while (tmp_entry) {
		overlap_w_sum += tmp_entry->overlap_w;
		max_overlap = (overlap_w_sum > max_overlap) ? overlap_w_sum
							    : max_overlap;
		tmp_entry = tmp_entry->tail;
	}
	return max_overlap;
}

static int scan_x_overlap(struct drm_mtk_layering_info *disp_info,
			  int disp_index, int ovl_overlap_limit_w)
{
	struct hrt_sort_entry *tmp_entry;
	int overlap_w_sum, max_overlap;

	overlap_w_sum = 0;
	max_overlap = 0;
	tmp_entry = x_entry_list;
	while (tmp_entry) {
		overlap_w_sum += tmp_entry->overlap_w;
		max_overlap = (overlap_w_sum > max_overlap) ? overlap_w_sum
							    : max_overlap;
		tmp_entry = tmp_entry->tail;
	}
	return max_overlap;
}

static int scan_y_overlap_for_compare(struct drm_mtk_layering_info *disp_info,
			  int disp_index, int ovl_overlap_limit_w, __u64 *max_layer_id)
{
	struct hrt_sort_entry *tmp_entry;
	int overlap_w_sum, tmp_overlap, max_overlap;

	overlap_w_sum = 0;
	tmp_overlap = 0;
	max_overlap = 0;
	tmp_entry = y_entry_list_for_compare;
	while (tmp_entry) {
		overlap_w_sum += tmp_entry->overlap_w;

		if (tmp_entry->overlap_w > 0) {
			add_layer_entry_for_compare(tmp_entry->layer_info, false,
					tmp_entry->overlap_w);
		} else {
			remove_layer_entry_for_compare(tmp_entry->layer_info, false);
		}

		if (overlap_w_sum > ovl_overlap_limit_w &&
		    overlap_w_sum > max_overlap) {
			tmp_overlap = scan_x_overlap_for_compare(disp_info, disp_index,
						     ovl_overlap_limit_w);
		} else {
			tmp_overlap = overlap_w_sum;
		}

		if (tmp_overlap >= max_overlap) {
			max_overlap = tmp_overlap;
			if (tmp_entry->layer_info->buffer_alloc_id != (__u64)(-1))
				*max_layer_id = tmp_entry->layer_info->buffer_alloc_id;
		}

		tmp_entry = tmp_entry->tail;
	}
	DDPDBG_HBL("%s,crtc:%u,limit:%u,max_overlap:%u,max_ID:0x%llx\n",
		__func__, disp_info->disp_idx, ovl_overlap_limit_w, max_overlap, *max_layer_id);

	return max_overlap;
}

static int scan_y_overlap(struct drm_mtk_layering_info *disp_info,
			  int disp_index, int ovl_overlap_limit_w)
{
	struct hrt_sort_entry *tmp_entry;
	int overlap_w_sum, tmp_overlap, max_overlap;

	overlap_w_sum = 0;
	tmp_overlap = 0;
	max_overlap = 0;
	tmp_entry = y_entry_list;
	while (tmp_entry) {
		overlap_w_sum += tmp_entry->overlap_w;
		if (tmp_entry->overlap_w > 0) {
			add_layer_entry(tmp_entry->layer_info, false,
					tmp_entry->overlap_w);
		} else {
			remove_layer_entry(tmp_entry->layer_info, false);
		}

		if (overlap_w_sum > ovl_overlap_limit_w &&
		    overlap_w_sum > max_overlap) {
			tmp_overlap = scan_x_overlap(disp_info, disp_index,
						     ovl_overlap_limit_w);
		} else {
			tmp_overlap = overlap_w_sum;
		}

		max_overlap =
			(tmp_overlap > max_overlap) ? tmp_overlap : max_overlap;
		tmp_entry = tmp_entry->tail;
	}

	return max_overlap;
}

static int get_hrt_level(int sum_w, int is_larb)
{
	int hrt_level;
	int *bound_table;
	enum DISP_HW_MAPPING_TB_TYPE type;

	if (is_larb)
		type = DISP_HW_LARB_BOUND_TB;
	else
		type = DISP_HW_EMI_BOUND_TB;

	bound_table = l_rule_ops->get_bound_table(type);
	for (hrt_level = 0; hrt_level < HRT_LEVEL_NUM; hrt_level++) {
		if (bound_table[hrt_level] != -1 &&
		    sum_w <= bound_table[hrt_level] * HRT_UINT_BOUND_BPP)
			return hrt_level;
	}
	return hrt_level;
}

static bool has_hrt_limit(struct drm_mtk_layering_info *disp_info, int idx)
{
	if (disp_info->layer_num[idx] <= 0)
		return false;

	if (disp_info->disp_mode[idx] == MTK_DRM_SESSION_DC_MIRROR)
		return false;

	return true;
}

static int get_hrt_disp_num(struct drm_mtk_layering_info *disp_info)
{
	int cnt = 0, i;

	for (i = 0; i < HRT_DISP_TYPE_NUM; i++)
		if (has_hrt_limit(disp_info, i))
			cnt++;

	return cnt;
}

/* For BW monitor debug */
static void print_bwm_table(void)
{
	int i = 0;

	DDPMSG("BWMT===== normal_layer_compress_ratio_tb =====\n");
	DDPMSG("BWMT===== Item   Frame   Key   avg   peak   valid   active =====\n");
	for (i = 0; i < MAX_FRAME_RATIO_NUMBER*MAX_LAYER_RATIO_NUMBER; i++) {
		if ((normal_layer_compress_ratio_tb[i].key_value) &&
				(normal_layer_compress_ratio_tb[i].average_ratio != 0) &&
				(normal_layer_compress_ratio_tb[i].peak_ratio != 0))
			DDPMSG("BWMT===== %4d   %u   %llu   %u   %u   %u   %u =====\n", i,
					normal_layer_compress_ratio_tb[i].frame_idx,
					normal_layer_compress_ratio_tb[i].key_value,
					normal_layer_compress_ratio_tb[i].average_ratio,
					normal_layer_compress_ratio_tb[i].peak_ratio,
					normal_layer_compress_ratio_tb[i].valid,
					normal_layer_compress_ratio_tb[i].active);
	}
	DDPMSG("BWMT===== fbt_layer_compress_ratio_tb =====\n");
	DDPMSG("BWMT===== Item   Frame   Key   avg   peak   valid   active =====\n");
	for (i = 0; i < MAX_FRAME_RATIO_NUMBER; i++) {
		if ((fbt_layer_compress_ratio_tb[i].key_value) &&
				(fbt_layer_compress_ratio_tb[i].average_ratio != 0) &&
				(fbt_layer_compress_ratio_tb[i].peak_ratio != 0))
			DDPMSG("BWMT===== %4d   %u   %llu   %u   %u   %u   %u =====\n", i,
					fbt_layer_compress_ratio_tb[i].frame_idx,
					fbt_layer_compress_ratio_tb[i].key_value,
					fbt_layer_compress_ratio_tb[i].average_ratio,
					fbt_layer_compress_ratio_tb[i].peak_ratio,
					fbt_layer_compress_ratio_tb[i].valid,
					fbt_layer_compress_ratio_tb[i].active);
	}
	DDPMSG("BWMT===== unchanged_compress_ratio_table =====\n");
	DDPMSG("BWMT===== Item   Frame   Key   avg   peak   valid   active =====\n");
	for (i = 0; i < MAX_LAYER_RATIO_NUMBER; i++) {
		if ((unchanged_compress_ratio_table[i].key_value) &&
				(unchanged_compress_ratio_table[i].average_ratio != 0) &&
				(unchanged_compress_ratio_table[i].peak_ratio != 0))
			DDPMSG("BWMT===== %4d   %u   %llu   %u   %u   %u   %u =====\n", i,
					unchanged_compress_ratio_table[i].frame_idx,
					unchanged_compress_ratio_table[i].key_value,
					unchanged_compress_ratio_table[i].average_ratio,
					unchanged_compress_ratio_table[i].peak_ratio,
					unchanged_compress_ratio_table[i].valid,
					unchanged_compress_ratio_table[i].active);
	}
}

/**
 * Return the HRT layer weight.
 * If the layer_info is NULL, return GLES layer weight.
 */
static int get_layer_weight(struct drm_device *dev, int disp_idx,
		struct drm_mtk_layer_config *layer_info,
		unsigned int frame_idx, bool is_gles, bool is_dal)
{
	int bpp, weight;
	struct mtk_drm_private *priv = dev->dev_private;
	static bool aee_trigger = true;

	if (layer_info)
		bpp = mtk_get_format_bpp(layer_info->src_fmt);
	else
		bpp = HRT_UINT_BOUND_BPP;
	if (disp_idx == HRT_SECONDARY) {
		/* To Be Impl */
#ifdef IF_ZERO
		struct disp_session_info dispif_info;

		/* For seconary display, set the weight 4K@30 as 2K@60.	*/
		hdmi_get_dev_info(true, &dispif_info);

		if (dispif_info.displayWidth > 2560)
			weight = HRT_UINT_WEIGHT * 2;
		else if (dispif_info.displayWidth > 1920)
			weight = HRT_UINT_WEIGHT;
		else
			weight = HRT_UINT_WEIGHT / 2;

		if (dispif_info.vsyncFPS <= 30)
			weight /= 2;

		return weight * bpp;
#endif
	}

	weight = HRT_UINT_WEIGHT;

	if (is_dal)
		bpp = HRT_AEE_WEIGHT / weight;

	if (priv->data->mmsys_id == MMSYS_MT6878 && is_dal &&
		!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_LAYERING_RULE_BY_LARB))
		bpp = (HRT_AEE_WEIGHT + 400 * 3 / 2) / weight;

	if (get_layering_opt(LYE_OPT_OVL_BW_MONITOR) && frame_idx && is_gles &&
			get_layering_opt(LYE_OPT_GPU_CACHE) && (disp_idx == HRT_PRIMARY)
			&& (have_force_gpu_layer == 0)) {
		uint64_t key_value = frame_idx - BWM_GPUC_TUNING_FRAME;
		int i = 0;
		struct drm_mtk_layering_info *disp_info = &layering_info;

		DDPDBG_BWM("GPUC: %s:%d fbt layer frame_idx:%u key:%llu\n",
				__func__, __LINE__, frame_idx, key_value);
#ifdef HRT_DEBUG_LEVEL1
		print_bwm_table();
#endif
		if (have_gpu_cached) {
			disp_info->disp_caps[HRT_PRIMARY] |= MTK_GLES_FBT_GET_RATIO;
			DDPMSG("GPUC: FBT ratio use last ratio:%d bpp:%d\n",
				last_fbt_weight, bpp);
			return last_fbt_weight * bpp;
		}

		for (i = 0; i < MAX_FRAME_RATIO_NUMBER; i++) {
			if ((fbt_layer_compress_ratio_tb[i].key_value == key_value) &&
					(fbt_layer_compress_ratio_tb[i].valid == 1) &&
					(fbt_layer_compress_ratio_tb[i].peak_ratio != 0) &&
					(fbt_layer_compress_ratio_tb[i].peak_ratio <= 1000)) {
				unsigned int index = 0;

				weight *= fbt_layer_compress_ratio_tb[i].peak_ratio;
				do_div(weight, 1000);
				last_fbt_weight = weight;
				DDPDBG_BWM("GPUC: fbt layer frame_idx:%u key:%llu\n",
					frame_idx, key_value);
				DDPDBG_BWM("GPUC: ratio:%u weight:%d\n",
					fbt_layer_compress_ratio_tb[i].peak_ratio, weight);

				if (disp_info->disp_caps[HRT_PRIMARY] & MTK_GLES_FBT_UNCHANGED) {
					disp_info->disp_caps[HRT_PRIMARY] |= MTK_GLES_FBT_GET_RATIO;
					DDPMSG("GPUC: GLES FBT ratio is valid\n");
					have_gpu_cached = true;
				}

				/* Just from emi efficency table to find level index */
				index = (fbt_layer_compress_ratio_tb[i].peak_ratio*256)/(1000*16);
				if (index) {
					weight = weight*10000/emi_eff_tb[index-1];
					DDPDBG_BWM("%d BWM:index:%u eff:%u weight:%d\n",
						__LINE__, index, emi_eff_tb[index-1], weight);
				} else {
					weight = weight*10000/emi_eff_tb[0];
					DDPDBG_BWM("%d BWM:index:%u eff:%u weight:%d\n",
							__LINE__, index, emi_eff_tb[0], weight);
				}
				return weight * bpp;
			}
		}
	}

	if (get_layering_opt(LYE_OPT_OVL_BW_MONITOR) && frame_idx &&
		(disp_idx == HRT_PRIMARY) && layer_info && layer_info->compress &&
		(layer_info->layer_caps & MTK_HWC_UNCHANGED_LAYER)) {
		uint64_t key_value = frame_idx + layer_info->buffer_alloc_id -
			BWM_GPUC_TUNING_FRAME;
		int i = 0;

		DDPDBG_BWM("BWM: line:%d unchanged layer frame_idx:%u key:%llu\n",
				__LINE__, frame_idx, key_value);
#ifdef HRT_DEBUG_LEVEL1
		print_bwm_table();
#endif
		for (i = 0; i < MAX_LAYER_RATIO_NUMBER; i++) {
			if ((unchanged_compress_ratio_table[i].key_value
				== layer_info->buffer_alloc_id) &&
				(unchanged_compress_ratio_table[i].valid == 1) &&
				(unchanged_compress_ratio_table[i].peak_ratio != 0)) {
				unsigned int index = 0;
				unsigned int peak_ratio =
					unchanged_compress_ratio_table[i].peak_ratio;
				struct drm_mtk_layering_info *disp_info = &layering_info;

				/* Due to the problem of calculation accuracy use 1024 */
				if (peak_ratio > 1024) {
					if (aee_trigger) {
						print_bwm_table();
						dump_disp_info(disp_info,
							DISP_DEBUG_LEVEL_CRITICAL);
						mtk_drm_crtc_diagnose();
						DDPAEE("%s:%d gets ratio:%u > 1000\n",
							__func__, __LINE__, peak_ratio);
						aee_trigger = false;
					}
					weight *= 1000;
					peak_ratio = 1000;
				} else
					weight *= peak_ratio;

				do_div(weight, 1000);
				DDPDBG_BWM("BWM: unchgd f_idx:%u allocid:%llu ratio:%u weight:%d\n",
					frame_idx, layer_info->buffer_alloc_id,
					peak_ratio, weight);

				/* Just from emi efficency table to find level index */
				index = (peak_ratio * 256) / (1000 * 16);
				if (index) {
					weight = weight*10000/emi_eff_tb[index-1];
					DDPDBG_BWM("%d BWM:index:%u eff:%u weight:%d\n",
						__LINE__, index, emi_eff_tb[index-1], weight);
				} else {
					weight = weight*10000/emi_eff_tb[0];
					DDPDBG_BWM("%d BWM:index:%u eff:%u weight:%d\n",
						__LINE__, index, emi_eff_tb[0], weight);
				}
				layer_info->layer_caps |= MTK_DISP_UNCHANGED_RATIO_VALID;
				return weight * bpp;
			}
		}

		for (i = 0; i < MAX_FRAME_RATIO_NUMBER * MAX_LAYER_RATIO_NUMBER; i++) {
			if ((normal_layer_compress_ratio_tb[i].key_value == key_value) &&
				(normal_layer_compress_ratio_tb[i].valid == 1) &&
				(normal_layer_compress_ratio_tb[i].peak_ratio != 0)) {
				unsigned int index = 0;
				unsigned int peak_ratio =
					normal_layer_compress_ratio_tb[i].peak_ratio;
				struct drm_mtk_layering_info *disp_info = &layering_info;

				/* Due to the problem of calculation accuracy use 1024 */
				if (peak_ratio > 1024) {
					if (aee_trigger) {
						print_bwm_table();
						dump_disp_info(disp_info,
							DISP_DEBUG_LEVEL_CRITICAL);
						mtk_drm_crtc_diagnose();
						DDPAEE("%s:%d gets ratio:%u > 1000\n",
							__func__, __LINE__, peak_ratio);
						aee_trigger = false;
					}
					weight *= 1000;
					peak_ratio = 1000;
				} else
					weight *= peak_ratio;

				do_div(weight, 1000);
				DDPDBG_BWM("BWM:fidx:%u allocid:%llu key:%llu ratio:%u weight:%d\n",
					frame_idx, layer_info->buffer_alloc_id, key_value,
					peak_ratio, weight);

				index = (peak_ratio * 256) / (1000 * 16);
				if (index) {
					weight = weight*10000/emi_eff_tb[index-1];
					DDPDBG("%d BWM:index:%u eff:%u weight:%d\n",
						__LINE__, index, emi_eff_tb[index-1], weight);
				} else {
					weight = weight*10000/emi_eff_tb[0];
					DDPDBG("%d BWM:index:%u eff:%u weight:%d\n",
						__LINE__, index, emi_eff_tb[0], weight);
				}
				layer_info->layer_caps |= MTK_DISP_UNCHANGED_RATIO_VALID;
				return weight * bpp;
			}
		}
	}

	if (hrt_lp_switch_get() > 0 && hrt_lp_switch_get() < 100 &&
			((!layer_info) || (layer_info && layer_info->compress))) {
		weight *= hrt_lp_switch_get();
		do_div(weight, 100);
	}

	if (priv->data->need_emi_eff)
		return (weight * bpp * 10000)/default_emi_eff;

	return weight * bpp;
}

static u32 calc_mml_rsz_ratio(struct mml_frame_info *mml_info)
{
	u32 ratio = 100;
	u32 src_w = 0, src_h = 0, dst_w = 0, dst_h = 0;

	if (unlikely(!mml_info))
		return ratio;

	src_w = mml_info->dest[0].crop.r.width;
	src_h = mml_info->dest[0].crop.r.height;
	dst_w = mml_info->dest[0].compose.width;
	dst_h = mml_info->dest[0].compose.height;

	if (mml_info->dest[0].rotate == MML_ROT_90 || mml_info->dest[0].rotate == MML_ROT_270) {
		dst_w = mml_info->dest[0].compose.height;
		dst_h = mml_info->dest[0].compose.width;
	}

	if (src_w > dst_w)
		ratio = ratio * src_w / dst_w;
	if (src_h > dst_h)
		ratio = ratio * src_h / dst_h;

	DDPDBG("%s ratio:%u (%ux%u)->(%ux%u)\n",
		__func__, ratio, src_w, src_h, dst_w, dst_h);

	return ratio;
}

void calc_mml_layer_weight(struct drm_mtk_layering_info *disp_info,
	int idx, int layer_idx, int *overlap_w)
{
	u32 ratio = 0;

	if (disp_info == NULL || overlap_w == NULL || idx >= LYE_CRTC || idx < 0)
		return;

	ratio = calc_mml_rsz_ratio(&disp_info->mml_cfg[idx][layer_idx]);
	if (ratio == 100)
		return;

	*overlap_w = ((u64)*overlap_w) * ratio;
	do_div(*overlap_w, 100);
	DDPINFO("%s overlap_w:%d ratio:%u\n", __func__, *overlap_w, ratio);
}

static bool _calc_gpu_cache_layerset_hrt_num(struct drm_device *dev,
			 struct drm_mtk_layering_info *disp_info, int disp,
			 int hrt_type, bool force_scan_y, bool has_dal_layer)
{
	int i, sum_overlap_w, overlap_l_bound;
	uint16_t layer_map;
	int overlap_w, layer_idx, phy_layer_idx, ovl_cnt;
	bool has_gles = false;
	struct drm_mtk_layer_config *layer_info;
	int overlap_w_of_bwm = 0;
	int sum_overlap_w_gc = 0;
	int j = 0;
	int bw_monitor_is_on = 0;
	unsigned int disp_list;

	/* BWM + GPU Cache */
	if (get_layering_opt(LYE_OPT_OVL_BW_MONITOR))
		bw_monitor_is_on = 1;
	else
		bw_monitor_is_on = 0;

	if (disp >= LYE_CRTC || disp < 0) {
		DDPPR_ERR("%s[%d]:disp:%d\n", __func__, __LINE__, disp);
		return false;
	}

	if (!has_hrt_limit(disp_info, disp))
		return 0;

	/* 1.Initial overlap conditions. */
	sum_overlap_w = 0;
	sum_overlap_w_gc = 0;

	/*
	 * The parameters of hrt table are base on ARGB color format.
	 * Multiply the bpp of it.
	 */
	overlap_l_bound = g_emi_bound_table[0] * HRT_UINT_BOUND_BPP;

	/*
	 * 2.Add each layer info to layer list and sort it by yoffset.
	 * Also add up each layer overlap weight.
	 */
	layer_idx = -1;
	ovl_cnt = get_phy_ovl_layer_cnt(disp_info, disp);
	disp_list = disp_info->disp_list;
	layer_map = l_rule_ops->get_mapping_table(dev, disp, disp_list, DISP_HW_LAYER_TB,
						  ovl_cnt);

	if (l_rule_info->dal_enable) {
		layer_map = l_rule_ops->get_mapping_table(
			dev, disp, disp_list, DISP_HW_LAYER_TB, MAX_PHY_OVL_CNT);
		layer_map &= HRT_AEE_LAYER_MASK;
	}

	/* Inactive layer need > 1 */
	j = 0;
	for (i = 0; i < disp_info->layer_num[disp]; i++) {
		layer_info = &disp_info->input_config[disp][i];
		if ((layer_info != NULL) &&
				(layer_info->layer_caps & MTK_HWC_INACTIVE_LAYER)) {
			DDPDBG_BWM("GPUC: Inactiver layer %d\n", i);
			j++;
		}
	}
	if (j <= 1) {
		DDPMSG("GPUC: Inactiver layer is less than or equal 1\n");
		return false;
	}
	DDPMSG("GPUC: Have %d Inactiver layer gles head:%d tail:%d\n", j,
		disp_info->gles_head[disp], disp_info->gles_tail[disp]);

	for (i = 0; i < disp_info->layer_num[disp]; i++) {
		int ovl_idx;
		int skipped = 0;

		layer_info = &disp_info->input_config[disp][i];
		DDPDBG_BWM("GPUC: layer idx %d\n", i);
		if (disp_info->gles_head[disp] == -1 ||
		    (i < disp_info->gles_head[disp] ||
		     i > disp_info->gles_tail[disp])) {
			if (hrt_type != HRT_TYPE_EMI) {
				if (layer_idx == -1)
					layer_idx = 0;
				else if (!is_extended_layer(layer_info))
					layer_idx++;

				phy_layer_idx =
					get_phy_ovl_index(dev, disp, disp_list, layer_idx);
				ovl_idx = get_ovl_by_phy(dev, disp, disp_list, layer_map,
							 layer_idx, __func__);
				if (get_larb_by_ovl(dev, ovl_idx, disp) !=
				    hrt_type)
					continue;
			}
			DDPDBG_BWM("GPUC: layer idx %d\n", i);
			overlap_w = get_layer_weight(dev, disp, layer_info, 0, false, false);
			if (bw_monitor_is_on) {
				overlap_w_of_bwm = get_layer_weight(dev, disp, layer_info,
					disp_info->frame_idx[disp], false, false);
				DDPDBG_BWM("GPUC line:%d overlap_w:%d overlap_w_of_bwm:%d\n",
					__LINE__, overlap_w, overlap_w_of_bwm);
			}

			if (mtk_has_layer_cap(layer_info, MTK_MML_DISP_DIRECT_DECOUPLE_LAYER |
							  MTK_MML_DISP_DIRECT_LINK_LAYER)) {
				calc_mml_layer_weight(disp_info, disp, i, &overlap_w);
				if ((disp == HRT_PRIMARY) && bw_monitor_is_on) {
					calc_mml_layer_weight(disp_info,
						disp, i, &overlap_w_of_bwm);
					DDPDBG_BWM("GPUC line:%d overlap_w:%d overlapw_of_bwm:%d\n",
						__LINE__, overlap_w, overlap_w_of_bwm);
				}
			}

			if (layer_info->src_width > 40 || skipped == 1) {
				if (!bw_monitor_is_on) {
					sum_overlap_w += overlap_w;
					add_layer_entry(layer_info, true, overlap_w);
				} else {
					sum_overlap_w += overlap_w_of_bwm;
					add_layer_entry(layer_info, true, overlap_w_of_bwm);
				}
				DDPDBG_BWM("GPUC: layer idx:%d caps:0x%08x\n",
					i, layer_info->layer_caps);
				if ((!bw_monitor_is_on) &&
					!(layer_info->layer_caps & MTK_HWC_INACTIVE_LAYER)) {
					sum_overlap_w_gc += overlap_w;
					DDPDBG_BWM("GPUC line:%d sum_o_w:%d sum_overlap_w_gc:%d\n",
						__LINE__, sum_overlap_w, sum_overlap_w_gc);
					add_layer_entry_for_compare(layer_info, true, overlap_w);
				}
				if ((bw_monitor_is_on) &&
					!(layer_info->layer_caps & MTK_HWC_INACTIVE_LAYER)) {
					sum_overlap_w_gc += overlap_w_of_bwm;
					DDPDBG_BWM("GPUC line:%d sumow:%d sum_overlap_w_gcbwm:%d\n",
						__LINE__, sum_overlap_w, sum_overlap_w_gc);
					add_layer_entry_for_compare(layer_info, true,
						overlap_w_of_bwm);
				}
			} else {
				skipped = 1;
			}
		} else if (i == disp_info->gles_head[disp]) {
			/* Add GLES layer */
			if (hrt_type != HRT_TYPE_EMI) {
				if (layer_idx == -1)
					layer_idx = 0;
				else if (!is_extended_layer(layer_info))
					layer_idx++;

				phy_layer_idx =
					get_phy_ovl_index(dev, disp, disp_list, layer_idx);
				ovl_idx = get_ovl_by_phy(dev, disp, disp_list, layer_map,
							 layer_idx, __func__);

				if (get_larb_by_ovl(dev, ovl_idx, disp) !=
				    hrt_type)
					continue;
			}
			has_gles = true;
		}

	}

	DDPDBG_BWM("GPUC line:%d default:%d gc:%d overlap bound:%d\n",
		__LINE__, sum_overlap_w,
		sum_overlap_w_gc, overlap_l_bound);

	/* Add overlap weight of Gles layer and Assert layer. */
	if (has_gles || need_rollback_to_gpu_before_gpuc)
		sum_overlap_w += get_layer_weight(dev, disp, NULL, 0, has_gles, false);

	sum_overlap_w_gc += get_layer_weight(dev, disp, NULL,
		disp_info->frame_idx[disp], true, false);
	DDPDBG_BWM("GPUC line:%d sum_overlap_w:%d sum_overlap_w_gc:%d\n",
		__LINE__, sum_overlap_w, sum_overlap_w_gc);

	if (has_dal_layer) {
		sum_overlap_w += get_layer_weight(dev, disp, NULL, 0, false, has_dal_layer);
		sum_overlap_w_gc += get_layer_weight(dev, disp, NULL, 0, false, has_dal_layer);
		DDPDBG_BWM("GPUC line:%d sum_overlap_w:%d sum_overlap_w_gc:%d\n",
			__LINE__, sum_overlap_w, sum_overlap_w_gc);
	}

	/*
	 * 3.Calculate the HRT bound if the total layer weight over the
	 * lower bound or has secondary display.
	 */
	if (sum_overlap_w > overlap_l_bound ||
	    has_hrt_limit(disp_info, HRT_SECONDARY) || force_scan_y) {
		__u64 max_layer_id = 0x0;

		sum_overlap_w =
			scan_y_overlap(disp_info, disp, overlap_l_bound);
		sum_overlap_w_gc =
			scan_y_overlap_for_compare(disp_info, disp,
					overlap_l_bound, &max_layer_id);
		DDPDBG_BWM("GPUC line:%d sum_overlap_w:%d sum_overlap_w_gc:%d\n",
			__LINE__, sum_overlap_w, sum_overlap_w_gc);

		/* Add overlap weight of Gles layer and Assert layer. */
		if (has_gles || need_rollback_to_gpu_before_gpuc)
			sum_overlap_w += get_layer_weight(dev, disp, NULL, 0, has_gles, false);

		sum_overlap_w_gc += get_layer_weight(dev, disp, NULL,
			disp_info->frame_idx[disp], true, false);
		DDPDBG_BWM("GPUC line:%d sum_overlap_w:%d sum_overlap_w_gc:%d\n",
			__LINE__, sum_overlap_w, sum_overlap_w_gc);

		if (has_dal_layer) {
			sum_overlap_w += get_layer_weight(dev, disp, NULL, 0, false, has_dal_layer);
			sum_overlap_w_gc += get_layer_weight(dev, disp, NULL, 0, false, has_dal_layer);
			DDPDBG_BWM("GPUC line:%d sum_overlap_w:%d sum_overlap_w_gc:%d\n",
				__LINE__, sum_overlap_w, sum_overlap_w_gc);
		}
	}

	DDPDBG_BWM("GPUC %s disp:%d, hrt_type:%d, sum_overlap_w:%d\n", __func__,
			disp, hrt_type, sum_overlap_w);
	DDPMSG("GPUC sum_overlap_w:%d sum_overlap_w_gc:%d max:%d\n",
			sum_overlap_w, sum_overlap_w_gc, overlap_l_bound);

	free_all_layer_entry(true);
	free_all_layer_entry_for_compare(true);

	/* Judgment conditions for using GPU Cache */
	/* P.S. should use HRT bw, this use overlap */
	/* 1. This frame HRT BW(N) need more than one layer no compress HRT BW; */
	/* 2. Layerset HRT BW(M) need less than or equal N; */
	/* 3. N & M need less than or equal max HRT BW(bound). */
	if (sum_overlap_w > 400) {
		if (sum_overlap_w_gc > sum_overlap_w) {
			DDPMSG("GPUC: %s:%d M > N\n", __func__, __LINE__);
			return false;
		}
		if (sum_overlap_w_gc > overlap_l_bound) {
			DDPMSG("GPUC: %s:%d max<M<N\n", __func__, __LINE__);
			return false;
		}

		DDPMSG("GPUC: %s:%d M<=max<N or M<N<=max\n", __func__, __LINE__);
		return true;
	}

	DDPMSG("GPUC: %s:%d N<1 uncompress layer\n", __func__, __LINE__);
	return false;
}

static int _calc_hrt_num(struct drm_device *dev,
			 struct drm_mtk_layering_info *disp_info, int disp,
			 int hrt_type, bool force_scan_y, bool has_dal_layer,
			 bool need_gpu_cache)
{
	struct mtk_drm_private *priv = NULL;
	int i, sum_overlap_w, overlap_l_bound;
	uint16_t layer_map;
	int overlap_w, layer_idx, phy_layer_idx, ovl_cnt;
	bool has_gles = false;
	struct drm_mtk_layer_config *layer_info;
	int j = 0;
	int layerset_head = -1;
	int layerset_tail = -1;
	int overlap_w_of_bwm = 0;
	int bw_monitor_is_on = 0;
	int gpu_cache_is_on = 0;
	unsigned int disp_list;
	unsigned int disp_idx = disp;

	if (get_layering_opt(LYE_OPT_SPHRT))
		disp_idx = disp_info->disp_idx;

	/* BWM + GPU Cache */
	if (get_layering_opt(LYE_OPT_OVL_BW_MONITOR))
		bw_monitor_is_on = 1;
	else
		bw_monitor_is_on = 0;
	if (get_layering_opt(LYE_OPT_GPU_CACHE))
		gpu_cache_is_on = 1;
	else
		gpu_cache_is_on = 0;

	if (!has_hrt_limit(disp_info, disp))
		return 0;

	/* 1.Initial overlap conditions. */
	sum_overlap_w = 0;
	if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on)
		sum_overlap_w_of_bwm = 0;
	/*
	 * The parameters of hrt table are base on ARGB color format.
	 * Multiply the bpp of it.
	 */
	overlap_l_bound = g_emi_bound_table[0] * HRT_UINT_BOUND_BPP;

	/*
	 * 2.Add each layer info to layer list and sort it by yoffset.
	 * Also add up each layer overlap weight.
	 */
	layer_idx = -1;
	ovl_cnt = get_phy_ovl_layer_cnt(disp_info, disp);
	disp_list = disp_info->disp_list;
	layer_map = l_rule_ops->get_mapping_table(dev, disp, disp_list, DISP_HW_LAYER_TB,
						  ovl_cnt);

	if (l_rule_info->dal_enable && (disp_idx == HRT_PRIMARY)) {
		layer_map = l_rule_ops->get_mapping_table(
			dev, disp, disp_list, DISP_HW_LAYER_TB, MAX_PHY_OVL_CNT);
		layer_map &= HRT_AEE_LAYER_MASK;
	}

	/* BWM + GPU Cache */
	if (gpu_cache_is_on && (disp_idx == HRT_PRIMARY) &&
		(have_force_gpu_layer == 0) && need_gpu_cache &&
			(disp_info->gles_head[disp] == -1) &&
			(disp_info->gles_tail[disp] == -1)) {
		DDPDBG_BWM("GPUC: 1 layerset head:%d tail:%d\n",
			layerset_head, layerset_tail);
		for (i = 0; i < disp_info->layer_num[disp]; i++) {
			layer_info = &disp_info->input_config[disp][i];
			if ((layer_info != NULL) &&
				(layer_info->layer_caps & MTK_HWC_INACTIVE_LAYER)) {
				if (j == 0) {
					layerset_head = i;
					layerset_tail = i;
					j++;
				}
				layer_info->layer_caps |= MTK_DISP_CLIENT_LAYER;
				layerset_tail = i;
				mtk_rollback_layer_to_GPU(disp_info, disp, i);
				DDPDBG_BWM("GPUC: Inactive layer:%d\n", i);
			}
		}

		DDPDBG_BWM("GPUC: 2 layerset head:%d tail:%d\n",
			layerset_head, layerset_tail);

		/* If use GPU cache need inactive layer >= 2 layers */
		if (layerset_head != layerset_tail) {
			disp_info->gles_head[disp] = layerset_head;
			disp_info->gles_tail[disp] = layerset_tail;
			if (!(disp_info->disp_caps[HRT_PRIMARY] & MTK_GLES_FBT_UNCHANGED))
				have_gpu_cached = false;
		}
		DDPDBG_BWM("GPUC: 3 layerset head:%d tail:%d\n",
			layerset_head, layerset_tail);
	}

	if (dev)
		priv = dev->dev_private;

	for (i = 0; i < disp_info->layer_num[disp]; i++) {
		int ovl_idx;

		layer_info = &disp_info->input_config[disp][i];
		layer_info->layer_hrt_weight = 0;
		if (disp_info->gles_head[disp] == -1 ||
		    (i < disp_info->gles_head[disp] ||
		     i > disp_info->gles_tail[disp])) {
			if (hrt_type != HRT_TYPE_EMI) {
				if (layer_idx == -1)
					layer_idx = 0;
				else if (!is_extended_layer(layer_info))
					layer_idx++;

				phy_layer_idx =
					get_phy_ovl_index(dev, disp, disp_list, layer_idx);
				ovl_idx = get_ovl_by_phy(dev, disp, disp_list, layer_map,
							 layer_idx, __func__);
				if (get_larb_by_ovl(dev, ovl_idx, disp) !=
				    hrt_type)
					continue;
			}
			overlap_w = get_layer_weight(dev, disp_idx, layer_info, 0, false, false);
			if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on) {
				overlap_w_of_bwm = get_layer_weight(dev, disp_idx, layer_info,
					disp_info->frame_idx[disp], false, false);
				DDPDBG_BWM("BWM line:%d overlap_w:%d overlap_w_of_bwm:%d\n",
					__LINE__, overlap_w, overlap_w_of_bwm);
			}

			if (mtk_has_layer_cap(layer_info, MTK_MML_DISP_DIRECT_DECOUPLE_LAYER |
							  MTK_MML_DISP_DIRECT_LINK_LAYER)) {
				calc_mml_layer_weight(disp_info, disp, i, &overlap_w);
				if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on) {
					calc_mml_layer_weight(disp_info,
						disp, i, &overlap_w_of_bwm);
					DDPDBG_BWM("BWM line:%d overlap_w:%d overlap_w_of_bwm:%d\n",
						__LINE__, overlap_w, overlap_w_of_bwm);
				}
			}

			sum_overlap_w += overlap_w;
			add_layer_entry(layer_info, true, overlap_w);
			if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on) {
				sum_overlap_w_of_bwm += overlap_w_of_bwm;
				DDPDBG_BWM("BWM line:%d sum_o_w:%d sum_o_w_of_bwm:%d\n",
					__LINE__, sum_overlap_w, sum_overlap_w_of_bwm);
				add_layer_entry_for_compare(layer_info, true,
					overlap_w_of_bwm);
			}

			if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_LAYERING_RULE_BY_LARB)) {
				layer_info->layer_hrt_weight = overlap_w;
				if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on)
					layer_info->layer_hrt_weight =
							overlap_w_of_bwm;
			}
		} else if (i == disp_info->gles_head[disp]) {
			/* Add GLES layer */
			if (hrt_type != HRT_TYPE_EMI) {
				if (layer_idx == -1)
					layer_idx = 0;
				else if (!is_extended_layer(layer_info))
					layer_idx++;

				phy_layer_idx =
					get_phy_ovl_index(dev, disp, disp_list, layer_idx);
				ovl_idx = get_ovl_by_phy(dev, disp, disp_list, layer_map,
							 layer_idx, __func__);

				if (get_larb_by_ovl(dev, ovl_idx, disp) !=
				    hrt_type)
					continue;
			}
			has_gles = true;
		}
	}

	DDPDBG_HBL("%s,%d,crtc:%d BWM sum:%d sum_bwm:%d bound:%d,gles:(%d,%d),dal:%d\n",
		__func__, __LINE__, disp_idx, sum_overlap_w,
		sum_overlap_w_of_bwm, overlap_l_bound,
		disp_info->gles_head[disp],
		disp_info->gles_tail[disp], has_dal_layer);
	if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on)
		DDPDBG_BWM("BWM line:%d sum_overlap_w:%d sum_overlap_w_of_bwm:%d overlap bound:%d\n",
			__LINE__, sum_overlap_w, sum_overlap_w_of_bwm, overlap_l_bound);

	/* Add overlap weight of Gles layer and Assert layer. */
	if (has_gles) {
		overlap_w = get_layer_weight(dev, disp_idx, NULL, 0, has_gles, false);
		sum_overlap_w += overlap_w;
		if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on) {
			overlap_w_of_bwm = get_layer_weight(dev, disp_idx, NULL,
				disp_info->frame_idx[disp], need_gpu_cache, false);
			sum_overlap_w_of_bwm += overlap_w_of_bwm;
			DDPDBG_BWM("BWM line:%d sum_overlap_w:%d sum_overlap_w_of_bwm:%d\n",
				__LINE__, sum_overlap_w, sum_overlap_w_of_bwm);
		}
		if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_LAYERING_RULE_BY_LARB) &&
			disp_info->gles_head[disp] >= 0) {
			unsigned int gles_layer_id = disp_info->gles_head[disp];

			layer_info = &disp_info->input_config[disp][gles_layer_id];
			layer_info->layer_hrt_weight = overlap_w;
			if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on)
				layer_info->layer_hrt_weight = overlap_w_of_bwm;
			DDPDBG_HBL("%s,%d,GLES crtc:%u,bwm:%d,gles(%d,%d),l:%u,w:%u,sum:%d,sum_bwm:%d\n",
				__func__, __LINE__, disp_idx, bw_monitor_is_on,
				disp_info->gles_head[disp], disp_info->gles_tail[disp], i,
				layer_info->layer_hrt_weight, sum_overlap_w,
				sum_overlap_w_of_bwm);
		}
	}

	if (has_dal_layer && (disp_idx == HRT_PRIMARY)) {
		DDPINFO("%s 1, overlap_num=%d,%d, (%d)\n", __func__, sum_overlap_w,
			sum_overlap_w_of_bwm, get_layer_weight(dev, disp_idx, NULL, 0, false, has_dal_layer));
		overlap_w = get_layer_weight(dev, disp_idx, NULL, 0, false, has_dal_layer);
		sum_overlap_w += overlap_w;
		if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on) {
			overlap_w_of_bwm = get_layer_weight(dev, disp_idx, NULL, 0, false, has_dal_layer);
			sum_overlap_w_of_bwm += overlap_w_of_bwm;
			DDPDBG_BWM("BWM line:%d sum_overlap_w:%d sum_overlap_w_of_bwm:%d\n",
				__LINE__, sum_overlap_w, sum_overlap_w_of_bwm);
		}
		if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_LAYERING_RULE_BY_LARB)) {
			if (bw_monitor_is_on)
				mtk_drm_set_dal_weight(overlap_w_of_bwm);
			else
				mtk_drm_set_dal_weight(overlap_w);
				DDPDBG_HBL("%s,%d,crtc:%u,DAL,w:%u/%u,sum:%d,sum_bwm:%d\n",
					__func__, __LINE__, disp_idx,
					overlap_w, overlap_w_of_bwm,
					sum_overlap_w, sum_overlap_w_of_bwm);
		}
	}

	/*
	 * 3.Calculate the HRT bound if the total layer weight over the
	 * lower bound or has secondary display.
	 */
	if (sum_overlap_w > overlap_l_bound ||
	    has_hrt_limit(disp_info, HRT_SECONDARY) || force_scan_y) {
		__u64 max_layer_id = 0x0;

		sum_overlap_w =
			scan_y_overlap(disp_info, disp, overlap_l_bound);
		if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on) {
			sum_overlap_w_of_bwm =
				scan_y_overlap_for_compare(disp_info, disp,
						overlap_l_bound, &max_layer_id);
			DDPDBG_BWM("BWM line:%d sum_overlap_w:%d sum_overlap_w_of_bwm:%d\n",
				__LINE__, sum_overlap_w, sum_overlap_w_of_bwm);

			if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_LAYERING_RULE_BY_LARB) && max_layer_id > 0)
				update_layer_hrt_after_scan(disp_info, disp, max_layer_id);
		}

		/* Add overlap weight of Gles layer and Assert layer. */
		if (has_gles) {
			overlap_w = get_layer_weight(dev, disp_idx, NULL, 0, has_gles, false);
			sum_overlap_w += overlap_w;
			if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on) {
				overlap_w_of_bwm = get_layer_weight(dev, disp_idx, NULL,
						disp_info->frame_idx[disp], need_gpu_cache, false);
				sum_overlap_w_of_bwm += overlap_w_of_bwm;
				DDPDBG_BWM("BWM line:%d sum_overlap_w:%d sum_overlap_w_of_bwm:%d\n",
					__LINE__, sum_overlap_w, sum_overlap_w_of_bwm);
			}
			if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_LAYERING_RULE_BY_LARB) &&
				disp_info->gles_head[disp] >= 0) {
				unsigned int gles_layer_id = disp_info->gles_head[disp];

				layer_info = &disp_info->input_config[disp][gles_layer_id];
				layer_info->layer_hrt_weight = overlap_w;
				if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on)
					layer_info->layer_hrt_weight = overlap_w_of_bwm;
				DDPDBG_HBL("%s,%d,gles crtc:%u,bwm:%d,gles(%d,%d),l:%u,w:%u,sum:%d,sum_bwm:%d\n",
					__func__, __LINE__, disp_idx, bw_monitor_is_on,
					disp_info->gles_head[disp], disp_info->gles_tail[disp],
					i, layer_info->layer_hrt_weight, sum_overlap_w,
					sum_overlap_w_of_bwm);
			}
		}
		if (has_dal_layer && (disp_idx == HRT_PRIMARY)) {
			DDPINFO("%s 2, overlap_num=%d,%d, (%d)\n", __func__, sum_overlap_w,
				sum_overlap_w_of_bwm, get_layer_weight(dev, disp_idx, NULL, 0, false, has_dal_layer));
			overlap_w = get_layer_weight(dev, disp_idx, NULL, 0, false, has_dal_layer);
			sum_overlap_w += overlap_w;
			if (bw_monitor_is_on) {
				overlap_w_of_bwm = get_layer_weight(dev,
						disp_idx, NULL, 0, false, has_dal_layer);
				sum_overlap_w_of_bwm += overlap_w_of_bwm;
				DDPDBG_BWM("BWM line:%d sum_ow:%d sum_ow_of_bwm:%d bound:%d\n",
					__LINE__, sum_overlap_w,
					sum_overlap_w_of_bwm, overlap_l_bound);
			}
			if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_LAYERING_RULE_BY_LARB)) {
				if (bw_monitor_is_on)
					mtk_drm_set_dal_weight(overlap_w_of_bwm);
				else
					mtk_drm_set_dal_weight(overlap_w);
				DDPDBG_HBL("%s,%d,crtc:%u,DAL,w:%u/%u,sum:%d,sum_bwm:%d bound:%d\n",
					__func__, __LINE__, disp_idx, overlap_w, overlap_w_of_bwm,
					sum_overlap_w, sum_overlap_w_of_bwm, overlap_l_bound);
			}
		}
	}

#ifdef HRT_DEBUG_LEVEL1
	DDPMSG("%s disp:%d, disp:%d, hrt_type:%d, sum_overlap_w:%d\n", __func__,
		disp, disp, hrt_type, sum_overlap_w);
#endif

	free_all_layer_entry(true);
	if ((disp_idx == HRT_PRIMARY) && bw_monitor_is_on)
		free_all_layer_entry_for_compare(true);
	return sum_overlap_w;
}

#ifdef HAS_LARB_HRT
static int calc_larb_hrt_level(struct drm_device *dev,
			       struct drm_mtk_layering_info *disp_info)
{
	int larb_hrt_level, i, sum_overlap_w;

	larb_hrt_level = 0;
	for (i = HRT_TYPE_LARB0; i <= HRT_TYPE_LARB1; i++) {
		int tmp_hrt_level;

		sum_overlap_w = _calc_hrt_num(dev, disp_info, HRT_PRIMARY, i,
					      true, l_rule_info->dal_enable, false);
		sum_overlap_w += _calc_hrt_num(dev, disp_info, HRT_SECONDARY, i,
					       true, false, false);
		sum_overlap_w += _calc_hrt_num(dev, disp_info, HRT_THIRD, i,
					       true, false, false);
		tmp_hrt_level = get_hrt_level(sum_overlap_w, true);
		if (tmp_hrt_level > larb_hrt_level)
			larb_hrt_level = tmp_hrt_level;
	}

	return larb_hrt_level;
}
#endif

static int check_is_force_gpu(struct drm_device *dev,
			struct drm_mtk_layering_info *disp_info)
{
	int disp_idx = HRT_PRIMARY;

	if (get_layering_opt(LYE_OPT_SPHRT) && disp_info->disp_idx != 0)
		return 0;

	DDPMSG("GPUC: Disp_caps:%u\n", disp_info->disp_caps[HRT_PRIMARY]);
	if ((disp_info->gles_head[disp_idx] == -1) &&
			(disp_info->gles_tail[disp_idx] == -1)) {
		have_force_gpu_layer = 0;
		DDPMSG("GPUC: no force gpu layer\n");
	} else {
		have_force_gpu_layer = 1;
		DDPMSG("GPUC: have force gpu layer\n");
	}

	return 0;
}

static int calc_hrt_num(struct drm_device *dev,
			struct drm_mtk_layering_info *disp_info)
{
	int emi_hrt_level;
	int sum_overlap_w = 0;
	int sum_overlap_w_of_second_disp = 0;
	int comp_hrt_added = 0;
#ifdef HAS_LARB_HRT
	int larb_hrt_level;
#endif
	int overlap_num;
	/* TODO support display helper */
	/* bool scan_overlap = !!disp_helper_get_option(DISP_OPT_HRT_MODE); */
	bool scan_overlap = true;
	bool need_gpu_cache = false;
	unsigned int disp_idx = 0;

	if (get_layering_opt(LYE_OPT_SPHRT))
		disp_idx = disp_info->disp_idx;

	/* Calculate HRT for EMI level */
	if (has_hrt_limit(disp_info, HRT_PRIMARY)) {
		if ((disp_idx == 0) && get_layering_opt(LYE_OPT_GPU_CACHE)) {
			if (g_gpuc_direct_push) {
				need_gpu_cache = true;
				DDPMSG("GPUC: layerset direct push to gpu cache\n");
			} else {
				DDPMSG("GPUC: layerset need to cal gain\n");
				need_gpu_cache =
					_calc_gpu_cache_layerset_hrt_num(dev, disp_info,
					HRT_PRIMARY, HRT_TYPE_EMI,
					scan_overlap, l_rule_info->dal_enable);
			}
			if (!need_gpu_cache)
				have_gpu_cached = false;
		}
		sum_overlap_w =
			_calc_hrt_num(dev, disp_info, HRT_PRIMARY, HRT_TYPE_EMI,
				      scan_overlap, l_rule_info->dal_enable, need_gpu_cache);
	}
	if (has_hrt_limit(disp_info, HRT_SECONDARY)) {
		sum_overlap_w_of_second_disp =
			_calc_hrt_num(dev, disp_info, HRT_SECONDARY,
				      HRT_TYPE_EMI, scan_overlap, false, false);
		sum_overlap_w += sum_overlap_w_of_second_disp;
		if (get_layering_opt(LYE_OPT_OVL_BW_MONITOR))
			sum_overlap_w_of_bwm += sum_overlap_w_of_second_disp;
	}
	/* Virtual Display should not add to HRT sum: ovl -> wdma */
	/*
	if (has_hrt_limit(disp_info, HRT_THIRD)) {
		sum_overlap_w += calc_hrt_num(dev, disp_info, HRT_THIRD,
				HRT_TYPE_EMI, scan_overlap,
				false);
	}
	*/
	mtk_oddmr_hrt_cal_notify(&comp_hrt_added);

	sum_overlap_w += comp_hrt_added;
	if ((disp_idx == HRT_PRIMARY) && get_layering_opt(LYE_OPT_OVL_BW_MONITOR))
		sum_overlap_w_of_bwm += comp_hrt_added;

	emi_hrt_level = get_hrt_level(sum_overlap_w, false);
	if ((disp_idx == HRT_PRIMARY) && get_layering_opt(LYE_OPT_OVL_BW_MONITOR))
		emi_hrt_level = get_hrt_level(sum_overlap_w_of_bwm, false);

	overlap_num = sum_overlap_w;

	/*
	 * The larb bound always meets the limit for HRT_LEVEL2
	 * in 8+4 ovl architecture.
	 * So calculate larb bound only for HRT_LEVEL2.
	 */
	disp_info->hrt_num = emi_hrt_level;
#ifdef HRT_DEBUG_LEVEL1
	DDPMSG("EMI hrt lv2:%d,overlap_w:%d\n", emi_hrt_level, sum_overlap_w);
#endif

#ifdef HAS_LARB_HRT
	/* Need to calculate larb hrt for HRT_LEVEL_LOW level. */
	/* TODO: Should revise larb calculation statement here */
	/* if (hrt_level != HRT_LEVEL_NUM - 2) */
	/*	return hrt_level; */

	/* Check Larb Bound here */
	larb_hrt_level = calc_larb_hrt_level(dev, disp_info);

#ifdef HRT_DEBUG_LEVEL1
	DDPMSG("Larb hrt level:%d\n", larb_hrt_level);
#endif

	if (emi_hrt_level < larb_hrt_level)
		disp_info->hrt_num = larb_hrt_level;
	else
#endif
		disp_info->hrt_num = emi_hrt_level;

	return overlap_num;
}

/**
 * dispatch which one layer could be ext layer
 */
static int ext_layer_grouping(struct drm_device *dev,
			      struct drm_mtk_layering_info *disp_info)
{
	int cont_ext_layer_cnt = 0, ext_idx = 0;
	int max_ext_layer_cnt = 0, total_ext_layer_cnt = 0;
	int is_ext_layer, disp_idx, i;
	struct drm_mtk_layer_config *src_info, *dst_info;
	int available_layers = 0, phy_layer_cnt = 0;
	struct drm_crtc *crtc;
	int idx;

	if (get_layering_opt(LYE_OPT_SPHRT))
		disp_idx = disp_info->disp_idx;
	else
		disp_idx = 0;

	for (idx = 0; disp_idx < HRT_DISP_TYPE_NUM; disp_idx++, idx++) {
		/* initialize ext layer info */
		if (idx >= 0 && idx < LYE_CRTC)
			for (i = 0; i < disp_info->layer_num[idx]; i++)
				disp_info->input_config[idx][i].ext_sel_layer = -1;

		if (!get_layering_opt(LYE_OPT_EXT_LAYER))
			continue;

#ifndef LAYERING_SUPPORT_EXT_LAYER_ON_2ND_DISP
		if (disp_idx != HRT_PRIMARY)
			continue;
#endif

		/* this crtc variable is no used in is_continuous_ext_layer_overlap */
		if (disp_idx == HRT_PRIMARY) {
			drm_for_each_crtc(crtc, dev)
				if (drm_crtc_index(crtc) == disp_idx)
					break;
		} else {
			crtc = NULL;
		}
		/*
		 * If the physical layer > input layer,
		 * then skip using extended layer.
		 */
		phy_layer_cnt =
			mtk_get_phy_layer_limit(l_rule_ops->get_mapping_table(
				dev, disp_idx, disp_info->disp_list, DISP_HW_LAYER_TB,
				MAX_PHY_OVL_CNT));
		/* Remove the rule here so that we can have more oppotunity to
		 * test extended layer
		 * if (phy_layer_cnt > disp_info->layer_num[disp_idx])
		 *	continue;
		 */

		/* Workaround for OVL cnt > 2, it will make layer check KE
		 * when layer > 12
		 * Now when layer > 12, limit ext layer cnt
		 */
		max_ext_layer_cnt = PRIMARY_OVL_LAYER_NUM - phy_layer_cnt;
		if (max_ext_layer_cnt > PRIMARY_OVL_EXT_LAYER_NR)
			max_ext_layer_cnt = PRIMARY_OVL_EXT_LAYER_NR;

		for (i = 1; i < disp_info->layer_num[disp_idx]; i++) {
			dst_info = &disp_info->input_config[disp_idx][i];
			src_info = &disp_info->input_config[disp_idx][i - 1];

			/* skip if attached layer is reading from sram */
			if (mtk_has_layer_cap(src_info, MTK_MML_DISP_DIRECT_DECOUPLE_LAYER))
				continue;

			/* skip other GPU layers */
			if (mtk_is_gles_layer(disp_info, disp_idx, i) ||
			    mtk_is_gles_layer(disp_info, disp_idx, i - 1)) {
				cont_ext_layer_cnt = 0;
				if (i > disp_info->gles_tail[disp_idx]) {
					int tmp;

					tmp = disp_info->gles_tail[disp_idx] -
					      disp_info->gles_head[disp_idx];
					ext_idx = i - tmp;
				}
				continue;
			}

			is_ext_layer = !is_continuous_ext_layer_overlap(crtc,
				disp_info->input_config[disp_idx], i);

			if (disp_info->layer_num[disp_idx] > PRIMARY_OVL_LAYER_NUM) {
				if (total_ext_layer_cnt >= max_ext_layer_cnt) {
					DDPINFO("total_ext_layer_cnt %d, max_ext_layer_cnt %d\n",
						total_ext_layer_cnt, max_ext_layer_cnt);
					is_ext_layer = false;
				}
			}

			/*
			 * The yuv layer is not supported as extended layer
			 * as the HWC has a special for yuv content.
			 */
			if (mtk_is_yuv(dst_info->src_fmt))
				is_ext_layer = false;

			if (mtk_has_layer_cap(dst_info, MTK_DISP_RSZ_LAYER |
							MTK_MML_DISP_DIRECT_DECOUPLE_LAYER |
							MTK_MML_DISP_DIRECT_LINK_LAYER))
				is_ext_layer = false;

			if (is_ext_layer && cont_ext_layer_cnt < 3) {
				++cont_ext_layer_cnt;
				dst_info->ext_sel_layer = ext_idx;
				++total_ext_layer_cnt;
			} else {
				cont_ext_layer_cnt = 0;
				ext_idx = i;
				if (i > disp_info->gles_tail[disp_idx]) {
					ext_idx -=
						disp_info->gles_tail[disp_idx] -
						disp_info->gles_head[disp_idx];
				}
			}
		}

		if (get_layering_opt(LYE_OPT_SPHRT))
			break;
	}

#ifdef HRT_DEBUG_LEVEL1
	DDPMSG("[ext layer grouping]\n");
	dump_disp_info(disp_info, DISP_DEBUG_LEVEL_INFO);
#endif

	return available_layers;
}

void lye_add_lye_priv_blob(struct mtk_plane_comp_state *comp_state,
			   struct mtk_drm_lyeblob_ids *lyeblob_ids,
			   int plane_idx, int disp_idx,
			   struct drm_device *drm_dev)
{
	struct drm_property_blob *blob;

	blob = drm_property_create_blob(
		drm_dev, sizeof(struct mtk_plane_comp_state), comp_state);

	if (disp_idx >= LYE_CRTC || disp_idx < 0) {
		DDPPR_ERR("%s[%d]:disp_idx:%d\n", __func__, __LINE__, disp_idx);
		return;
	}


	lyeblob_ids->lye_plane_blob_id[disp_idx][plane_idx] = blob->base.id;
}

static int mtk_lye_get_comp_id(int disp_idx, int disp_list, struct drm_device *drm_dev,
			       int layer_map_idx)
{
	uint16_t ovl_mapping_tb = l_rule_ops->get_mapping_table(
		drm_dev, disp_idx, disp_list, DISP_HW_OVL_TB, 0);
	struct mtk_drm_private *priv = drm_dev->dev_private;
	unsigned int temp, temp1, i, comp_id_nr, *comp_id_list = NULL;
	unsigned int valid_ovl_map, idx = 0;

	if (get_layering_opt(LYE_OPT_SPDA_OVL_SWITCH)) {
		comp_id_nr = mtk_ddp_ovl_resource_list(priv, &comp_id_list);
		if (unlikely(comp_id_nr > DDP_COMPONENT_ID_MAX)) {
			DDPPR_ERR("%s gets invalid ovl comp_id_list\n", __func__);
			return comp_id_nr;
		}

		/* calculate ovl list idx from ovl_map */
		temp = ovl_mapping_tb & ~BIT(0);
		for (i = 0 ; i < comp_id_nr ; ++i) {
			temp1 = HRT_GET_FIRST_SET_BIT(temp);
			DDPDBG("%s %d map_tb %x %x map_idx %x\n", __func__, disp_idx, temp, temp1, layer_map_idx);
			if (temp1 >= layer_map_idx) {
				idx = i;
				break;
			}
			temp &= ~temp1;
		}
		if (unlikely(idx >= comp_id_nr)) {
			DDPPR_ERR("%s abnormal input ovl_mapping_tb %x & layer_map_idx %x idx %x\n",
						__func__, ovl_mapping_tb, layer_map_idx, idx);
			return comp_id_nr;
		}

		/* get valid ovl from disp_list */
		valid_ovl_map = priv->ovl_usage[disp_idx];
		if (priv->pre_defined_bw[disp_idx] == 0xFFFFFFFF) {
			for (i = 0 ; i < MAX_CRTC ; ++i) {
				if (i == disp_idx)
					continue;
				if ((disp_list & BIT(i)) != 0)
					valid_ovl_map &= ~priv->ovl_usage[i];
			}
		}

		/* get ovl comp according to ovl list idx and valid ovl */
		temp1 = valid_ovl_map;
		for (i = 0 ; i <= idx ; ++i) {
			temp = HRT_GET_FIRST_SET_BIT(temp1);
			DDPDBG("tmp %x valid_ovl_map %x\n", temp, temp1);
			temp1 &= ~temp;
		}
		temp = __builtin_ffs(temp);
		temp = (temp == 0) ? 0 : temp - 1;
		DDPDBG("%s %d idx %x to temp %x tmp1 %x\n", __func__, disp_idx, idx, temp, valid_ovl_map);

		if (unlikely(temp >= comp_id_nr)) {
			DDPPR_ERR("%s comp_list ovl_map %x & layer_map %x valid %x idx %x\n",
				__func__, ovl_mapping_tb, layer_map_idx,
				valid_ovl_map, valid_ovl_map);
			return comp_id_nr;
		}

		return comp_id_list[temp];
	}
	/* TODO: The component ID should be changed by ddp path and platforms */
	if (disp_idx == 0) {
		if (HRT_GET_FIRST_SET_BIT(ovl_mapping_tb) >= layer_map_idx)
			return DDP_COMPONENT_DMDP_RDMA0;
		/* When open VDS path switch feature, primary OVL have OVL0 only */
		else if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_VDS_PATH_SWITCH) &&
			priv->need_vds_path_switch)
			return DDP_COMPONENT_OVL0;
		if (priv->data->mmsys_id == MMSYS_MT6985 ||
			priv->data->mmsys_id == MMSYS_MT6897 ||
			priv->data->mmsys_id == MMSYS_MT6989 ||
			priv->data->mmsys_id == MMSYS_MT6878) {
			if (HRT_GET_FIRST_SET_BIT(ovl_mapping_tb -
				HRT_GET_FIRST_SET_BIT(ovl_mapping_tb)) >=
				layer_map_idx) {
				return DDP_COMPONENT_OVL0_2L;
			} else if (HRT_GET_FIRST_SET_BIT(ovl_mapping_tb -
				HRT_GET_FIRST_SET_BIT(ovl_mapping_tb -
				HRT_GET_FIRST_SET_BIT(ovl_mapping_tb)) -
				HRT_GET_FIRST_SET_BIT(ovl_mapping_tb)) >=
				layer_map_idx) {
				return DDP_COMPONENT_OVL1_2L;
			}
			// else
			//	return DDP_COMPONENT_OVL2_2L;
		} else {
			if (HRT_GET_FIRST_SET_BIT(ovl_mapping_tb -
				HRT_GET_FIRST_SET_BIT(ovl_mapping_tb)) >=
				layer_map_idx) {
				if ((priv->data->mmsys_id == MMSYS_MT6895) ||
					(priv->data->mmsys_id == MMSYS_MT6886) ||
					(priv->data->mmsys_id == MMSYS_MT6835) ||
					(priv->data->mmsys_id == MMSYS_MT6855))
					return DDP_COMPONENT_OVL1_2L;
				else
					return DDP_COMPONENT_OVL0_2L;
			} else
				return DDP_COMPONENT_OVL0;
		}
	} else if (disp_idx == 1) {
		if (get_layering_opt(LYE_OPT_SPDA_OVL_SWITCH)) {
			struct drm_crtc *crtc = priv->crtc[disp_idx];
			struct mtk_drm_crtc *mtk_crtc;
			struct mtk_ddp_comp *comp;
			unsigned int i, j;

			if (!crtc)
				return DDP_COMPONENT_OVL2_2L;

			mtk_crtc = to_mtk_crtc(crtc);
			//disp_idx 1 does not exist multiple mode yet
			for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
				if (comp)
					break;
			if (comp)
				return comp->id;
		} else if (priv->data->mmsys_id == MMSYS_MT6885)
			return DDP_COMPONENT_OVL2_2L;
		else if (priv->data->mmsys_id == MMSYS_MT6983)
			return DDP_COMPONENT_OVL2_2L_NWCG;
		else if (priv->data->mmsys_id == MMSYS_MT6985)
			return DDP_COMPONENT_OVL3_2L;
		else if (priv->data->mmsys_id == MMSYS_MT6897)
			return DDP_COMPONENT_OVL2_2L;
		else if (priv->data->mmsys_id == MMSYS_MT6895)
			return DDP_COMPONENT_OVL2_2L;
		else if (priv->data->mmsys_id == MMSYS_MT6989)
			return DDP_COMPONENT_OVL4_2L;
	} else if (disp_idx == 2) {
		if (mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_VDS_PATH_SWITCH))
			return DDP_COMPONENT_OVL0_2L;
		else if (get_layering_opt(LYE_OPT_SPDA_OVL_SWITCH)) {
			struct drm_crtc *crtc = priv->crtc[disp_idx];
			struct mtk_drm_crtc *mtk_crtc;
			struct mtk_ddp_comp *comp;
			unsigned int i, j;

			if (!crtc)
				return DDP_COMPONENT_OVL2_2L;

			mtk_crtc = to_mtk_crtc(crtc);
			//disp_idx 2 does not exist multiple mode yet
			for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
				if (comp)
					break;
			if (comp)
				return comp->id;
		} else if (priv->data->mmsys_id == MMSYS_MT6983)
			return DDP_COMPONENT_OVL1_2L_NWCG;
		else if (priv->data->mmsys_id == MMSYS_MT6985)
			return DDP_COMPONENT_OVL7_2L;
		else if (priv->data->mmsys_id == MMSYS_MT6897)
			return DDP_COMPONENT_OVL3_2L;
		else if (priv->data->mmsys_id == MMSYS_MT6895 ||
			 priv->data->mmsys_id == MMSYS_MT6886)
			return DDP_COMPONENT_OVL0_2L;
		else if (priv->data->mmsys_id == MMSYS_MT6879)
			return DDP_COMPONENT_OVL0_2L_NWCG;
		else if (priv->data->mmsys_id == MMSYS_MT6989)
			return DDP_COMPONENT_OVL4_2L;
		else
			return DDP_COMPONENT_OVL2_2L;
	} else if (disp_idx == 3) {
		if (get_layering_opt(LYE_OPT_SPDA_OVL_SWITCH)) {
			struct drm_crtc *crtc = priv->crtc[disp_idx];
			struct mtk_drm_crtc *mtk_crtc;
			struct mtk_ddp_comp *comp;
			unsigned int i, j;

			if (!crtc)
				return DDP_COMPONENT_OVL2_2L;

			mtk_crtc = to_mtk_crtc(crtc);
			//disp_idx 3 does not exist multiple mode yet
			for_each_comp_in_cur_crtc_path(comp, mtk_crtc, i, j)
				if (comp)
					break;
			if (comp)
				return comp->id;
		} else if (priv->data->mmsys_id == MMSYS_MT6983)
			return DDP_COMPONENT_OVL0_2L_NWCG;
		else if (priv->data->mmsys_id == MMSYS_MT6985 ||
				priv->data->mmsys_id == MMSYS_MT6897)
			return DDP_COMPONENT_OVL7_2L;
		else if (priv->data->mmsys_id == MMSYS_MT6989)
			return DDP_COMPONENT_OVL5_2L;
		else
			return DDP_COMPONENT_OVL2_2L;
	}

	DDPPR_ERR("Invalid disp_idx:%d\n", disp_idx);
	return DDP_COMPONENT_OVL2_2L;
}

static int mtk_lye_get_lye_id(int disp_idx, int disp_list, struct drm_device *drm_dev,
			      int layer_map_idx)
{
	int cnt = 0;

	if (layer_map_idx != 0)
		while (!(layer_map_idx & 0x1)) {
			cnt++;
			layer_map_idx >>= 1;
		}
	layer_map_idx = cnt;
	return get_phy_ovl_index(drm_dev, disp_idx, disp_list, layer_map_idx);
}

static void clear_layer(struct drm_mtk_layering_info *disp_info,
			enum SCN_FACTOR *scn_decision_flag, struct drm_device *drm_dev)
{
	int di = 0;
	int i = 0;
	struct drm_mtk_layer_config *c;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc;
	struct mtk_drm_private *priv = drm_dev->dev_private;

	if (!get_layering_opt(LYE_OPT_CLEAR_LAYER))
		return;

	if ((priv->data->mmsys_id == MMSYS_MT6878) &&
		(disp_info->disp_idx == 3))
		return;

	drm_for_each_crtc(crtc, drm_dev) {
		if (drm_crtc_index(crtc) == 0)
			break;
	}
	mtk_crtc = to_mtk_crtc(crtc);
	if (mtk_crtc->mml_debug & DISP_MML_IR_CLEAR)
		return;

	for (di = 0; di < HRT_DISP_TYPE_NUM; di++) {
		int g_head = disp_info->gles_head[di];
		int top = -1;

		if (disp_info->layer_num[di] <= 1)
			continue;
		if (g_head == -1)
			continue;

		for (i = disp_info->layer_num[di] - 1; i >= g_head; i--) {
			c = &disp_info->input_config[di][i];
			if (mtk_has_layer_cap(c, MTK_LAYERING_OVL_ONLY) &&
			    mtk_has_layer_cap(c, MTK_CLIENT_CLEAR_LAYER)) {
				top = i;
				break;
			}
		}
		if (top == -1)
			continue;
		if (!mtk_is_gles_layer(disp_info, di, top))
			continue;

		DDPMSG("%s:D%d:L%d\n", __func__, di, top);

		rollback_all_to_GPU(disp_info, di);

		c = &disp_info->input_config[di][top];
		if (top == disp_info->gles_head[di])
			disp_info->gles_head[di]++;
		else if (top == disp_info->gles_tail[di])
			disp_info->gles_tail[di]--;
		else
			c->layer_caps |= MTK_DISP_CLIENT_CLEAR_LAYER;

/* Clear layer with RPO should check more condition since dual pipe enable */
#ifdef IF_ZERO
		if ((c->src_width < c->dst_width &&
		     c->src_height < c->dst_height) &&
		     get_layering_opt(LYE_OPT_RPO) &&
		    top < disp_info->gles_tail[di] &&
		    di == HRT_PRIMARY) {
			c->layer_caps |= MTK_DISP_RSZ_LAYER;
			l_rule_info->addon_scn[di] = ONE_SCALING;
		} else
#endif
		{
			c->layer_caps &= ~MTK_DISP_RSZ_LAYER;

			if ((c->src_width != c->dst_width ||
			     c->src_height != c->dst_height) &&
			    !mtk_has_layer_cap(c, MTK_MDP_RSZ_LAYER)) {
				c->layer_caps &= ~MTK_DISP_CLIENT_CLEAR_LAYER;
				mtk_rollback_layer_to_GPU(disp_info, di, top);
				DDPMSG("%s:remove clear(rsz), caps:0x%08x\n",
				       __func__, c->layer_caps);
			}
		}

		if (mtk_has_layer_cap(c, MTK_DISP_CLIENT_CLEAR_LAYER)) {
			*scn_decision_flag |= SCN_CLEAR;
			if ((*scn_decision_flag & SCN_IDLE)) {
				if (priv->data->need_emi_eff)
					disp_info->hrt_weight += (400 * 10000) / default_emi_eff;
				else
					disp_info->hrt_weight += 400;
			}
		}

		for (i = 0; i < disp_info->layer_num[di]; i++) {
			c = &disp_info->input_config[di][i];
			c->ext_sel_layer = -1;
			if (i != top)
				c->layer_caps &= ~MTK_DISP_RSZ_LAYER;
		}
	}
}

static int _dispatch_lye_blob_idx(struct drm_mtk_layering_info *disp_info,
				  int layer_map, int disp_idx,
				  struct mtk_drm_lyeblob_ids *lyeblob_ids,
				  struct drm_device *drm_dev, struct mtk_lye_ddp_state *lye_state)
{
	struct drm_mtk_layer_config *layer_info;
	int ext_cnt = 0, plane_idx = 0, layer_map_idx = 0;
	struct mtk_plane_comp_state comp_state;
	int prev_comp_id = -1;
	int i;
	int clear_idx = -1;
	int no_compress_layer_num = 0;
	int idx = disp_idx;
	static u32 last_mml_ir_lye;
	bool exclusive_chance = false;
	u32 rpo_comp = 0, mml_comp = 0;
	int rpo_idx = 0, mml_idx = 0;
	unsigned int *comp_id_list = NULL, comp_id_nr;
	struct mtk_drm_private *priv = drm_dev->dev_private;

	if (get_layering_opt(LYE_OPT_SPHRT))
		idx = 0;

	if (idx >= LYE_CRTC || idx < 0) {
		DDPPR_ERR("%s[%d]:idx:%d\n", __func__, __LINE__, idx);
		return 0;
	}

	for (i = 0; i < disp_info->layer_num[idx]; i++) {
		layer_info = &disp_info->input_config[idx][i];
		if (mtk_has_layer_cap(layer_info,
				      MTK_DISP_CLIENT_CLEAR_LAYER)) {
			clear_idx = i;
			break;
		}
	}

	comp_id_nr = mtk_ddp_ovl_resource_list(priv, &comp_id_list);
	exclusive_chance = ((disp_info->gles_head[idx] == -1) &&
			(disp_info->layer_num[idx] < (2 * comp_id_nr)));

	/* TODO: check BW monitor with SPHRT */
	if (disp_idx == HRT_PRIMARY) {
		lyeblob_ids->fbt_layer_id = -1;
		lyeblob_ids->fbt_gles_head = -1;
		lyeblob_ids->fbt_gles_tail = -1;
		if (disp_info->gles_head[HRT_PRIMARY] != -1) {
			lyeblob_ids->fbt_gles_head = disp_info->gles_head[HRT_PRIMARY];
			lyeblob_ids->fbt_gles_tail = disp_info->gles_tail[HRT_PRIMARY];
		}
	}
	for (i = 0; i < disp_info->layer_num[idx]; i++) {

		layer_info = &disp_info->input_config[idx][i];

		if (clear_idx < 0 &&
		    mtk_has_layer_cap(layer_info, MTK_DISP_CLIENT_CLEAR_LAYER))
			continue;

		if (i < clear_idx) {
			continue;
		} else if (i == clear_idx) {
			i = -1;
			clear_idx = -1;
		}

		if (mtk_is_gles_layer(disp_info, idx, i) &&
		    i != disp_info->gles_head[idx]) {
			layer_info->ovl_id = plane_idx - 1;
			if (disp_idx == HRT_PRIMARY)
				lyeblob_ids->fbt_layer_id = plane_idx - 1;
			DDPDBG_HBL("%s,%d,crtc:%u skip gles l:%d, w:%u gles(%d,%d)\n",
				__func__, __LINE__, disp_info->disp_idx, i, layer_info->layer_hrt_weight,
				disp_info->gles_head[idx], disp_info->gles_tail[idx]);
			continue;
		}

		if (!is_extended_layer(layer_info))
			layer_map &= ~layer_map_idx;

		layer_map_idx = HRT_GET_FIRST_SET_BIT(layer_map);
		comp_state.comp_id =
			mtk_lye_get_comp_id(disp_idx, disp_info->disp_list, drm_dev, layer_map_idx);
		comp_state.lye_id =
			mtk_lye_get_lye_id(disp_idx, disp_info->disp_list, drm_dev, layer_map_idx);

		if (comp_state.comp_id > DDP_COMPONENT_ID_MAX) {
			DDPPR_ERR("%s get invalid comp_id %d\n", __func__, comp_state.comp_id);
			break;
		}

		if (is_extended_layer(layer_info)) {
			comp_state.ext_lye_id = LYE_EXT0 + ext_cnt;
			ext_cnt++;
		} else {
			if (comp_state.comp_id != prev_comp_id)
				ext_cnt = 0;
			comp_state.ext_lye_id = LYE_NORMAL;
		}

		if (disp_idx == 0 &&
			((comp_state.comp_id == DDP_COMPONENT_OVL0_2L) ||
			(comp_state.comp_id == DDP_COMPONENT_OVL1_2L)) &&
			!is_extended_layer(layer_info) &&
			layer_info->compress != 1) {

			DDPINFO("%s layer_id %d no compress phy layer\n",
				__func__, i);
			no_compress_layer_num++;
		}

		if (disp_idx >= MAX_CRTC || plane_idx >= OVL_LAYER_NR) {
			dump_disp_info(disp_info, DISP_DEBUG_LEVEL_INFO);
			DDPAEE("%s Error disp_idx %d, plane_idx %d\n", __func__,
				disp_idx, plane_idx);
			break;
		}

		if (mtk_has_layer_cap(layer_info, MTK_MML_DISP_DIRECT_DECOUPLE_LAYER |
						  MTK_MML_DISP_DIRECT_LINK_LAYER)) {
			mml_comp = comp_state.comp_id;
			mml_idx = i;
		} else if (mtk_has_layer_cap(layer_info, MTK_DISP_RSZ_LAYER)) {
			rpo_comp = comp_state.comp_id;
			rpo_idx = i;
		}

		if (mml_comp && (mml_comp == rpo_comp)) {
			if (exclusive_chance && (mml_comp != comp_id_list[comp_id_nr - 1]) &&
				(priv->data->mmsys_id != MMSYS_MT6897) &&
				(mml_idx < rpo_idx)) {
				DDPMSG("MML RPO use the same OVL, got exclusive_chance\n");
				layer_map |= (layer_map_idx << 1);
				i--;
				continue;
			} else {
				DDPMSG("MML RPO use the same OVL, change layer_cap\n");
				if (mtk_has_layer_cap(layer_info, MTK_DISP_RSZ_LAYER)) {
					layer_info->layer_caps &= ~MTK_DISP_RSZ_LAYER;
					mtk_gles_incl_layer(disp_info, idx, i);
				} else {
					/* for both IR and DL */
					layer_info->layer_caps &= ~DISP_MML_CAPS_MASK;
					layer_info->layer_caps |= MTK_MML_DISP_MDP_LAYER;
					disp_info->disp_caps[disp_idx] |= MTK_NEED_REPAINT;
				}
			}
		}

		if (mtk_has_layer_cap(layer_info, MTK_MML_DISP_DIRECT_DECOUPLE_LAYER) &&
		    last_mml_ir_lye) {
			u8 last_lye = 0;
			u32 last_comp = 0;

			mtk_addon_get_comp(last_mml_ir_lye, &last_comp, &last_lye);
			if ((comp_state.comp_id != last_comp) || (comp_state.lye_id != last_lye)) {
				DDPMSG("MML IR layer changed\n");
				layer_info->layer_caps &= ~DISP_MML_CAPS_MASK;
				layer_info->layer_caps |= MTK_MML_DISP_MDP_LAYER;
				disp_info->disp_caps[disp_idx] |= MTK_NEED_REPAINT;
			}
		}

		comp_state.layer_caps = layer_info->layer_caps;
		if (mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_LAYERING_RULE_BY_LARB)) {
			comp_state.layer_hrt_weight = layer_info->layer_hrt_weight;
			DDPDBG_HBL(
				"%s,%d crtc:%u,l:%u,comp:%u,lye:%u,ext:%u,off(%u,%u),w/h(%u,%u),w:%u,ID:0x%llx,gles(%d,%d),mml:%u,clr:%d\n",
				__func__, __LINE__, disp_info->disp_idx, i, comp_state.comp_id,
				comp_state.lye_id, comp_state.ext_lye_id,
				layer_info->dst_offset_x, layer_info->dst_offset_y,
				layer_info->dst_width, layer_info->dst_height,
				comp_state.layer_hrt_weight, layer_info->buffer_alloc_id,
				disp_info->gles_head[idx], disp_info->gles_tail[idx],
				mml_idx, clear_idx);
		} else
			comp_state.layer_hrt_weight = 0;

		lye_add_lye_priv_blob(&comp_state, lyeblob_ids, plane_idx,
				      disp_idx, drm_dev);

		layer_info->ovl_id = plane_idx;
		plane_idx++;
		prev_comp_id = comp_state.comp_id;

		if (mtk_has_layer_cap(layer_info, MTK_MML_DISP_DIRECT_DECOUPLE_LAYER))
			mtk_addon_set_comp(&lye_state->mml_ir_lye,
					   comp_state.comp_id, comp_state.lye_id);
		else if (mtk_has_layer_cap(layer_info, MTK_MML_DISP_DIRECT_LINK_LAYER))
			mtk_addon_set_comp(&lye_state->mml_dl_lye,
					   comp_state.comp_id, comp_state.lye_id);
		else if (mtk_has_layer_cap(layer_info, MTK_DISP_RSZ_LAYER))
			mtk_addon_set_comp(&lye_state->rpo_lye,
					   comp_state.comp_id, comp_state.lye_id);
	}

	last_mml_ir_lye = lye_state->mml_ir_lye;

	if (disp_idx == 0) {
		HRT_SET_NO_COMPRESS_FLAG(disp_info->hrt_num,
				no_compress_layer_num);
		DDPINFO("%s disp_info->hrt_num=0x%x,no_comp_layer_num=%d\n",
				__func__, disp_info->hrt_num,
				no_compress_layer_num);
	}

	return 0;
}

static int dispatch_gles_range(struct drm_mtk_layering_info *disp_info,
			struct drm_device *drm_dev)
{
	int disp_idx;
	bool no_disp = true;
	struct mtk_drm_private *priv = drm_dev->dev_private;
	unsigned int disp = 0;

	if (get_layering_opt(LYE_OPT_SPHRT))
		disp = disp_info->disp_idx;

	for (disp_idx = 0; disp_idx < HRT_DISP_TYPE_NUM; disp_idx++)
		if (disp_info->layer_num[disp_idx] > 0) {
			no_disp = false;
			break;
		}

	if (no_disp) {
		DDPINFO("There is no disp need dispatch\n");
		return 0;
	}

	/* Dispatch gles range if necessary */
	if (disp_info->hrt_num > HRT_LEVEL_NUM - 1) {
		int max_ovl_cnt = g_emi_bound_table[HRT_LEVEL_NUM - 1];
		int valid_ovl_cnt = max_ovl_cnt;
		int hrt_disp_num = get_hrt_disp_num(disp_info);
		int gles_head_old, gles_head_new, gles_tail_old, gles_tail_new, weight;
		struct drm_mtk_layer_config *layer_info_new = NULL;

		if (priv->data->mmsys_id == MMSYS_MT6878 &&
			(disp == 0) && l_rule_info->dal_enable &&
			!mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_LAYERING_RULE_BY_LARB))
			valid_ovl_cnt -= 200; /* 2 layer */
		else if ((disp == 0) && l_rule_info->dal_enable)
			valid_ovl_cnt -= (HRT_AEE_WEIGHT / HRT_UINT_BOUND_BPP);
		valid_ovl_cnt /= HRT_UINT_WEIGHT;
		hrt_disp_num--;
		for (disp_idx = HRT_DISP_TYPE_NUM - 1; disp_idx >= 0; disp_idx--) {
			if (!has_hrt_limit(disp_info, disp_idx))
				continue;

			if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_LAYERING_RULE_BY_LARB)) {
				gles_head_old = disp_info->gles_head[disp_idx];
				gles_tail_old = disp_info->gles_tail[disp_idx];
			}

			valid_ovl_cnt =
				rollback_to_GPU(disp_info, disp_idx,
					valid_ovl_cnt - hrt_disp_num);
			valid_ovl_cnt += hrt_disp_num;
			hrt_disp_num--;

			if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
				MTK_DRM_OPT_LAYERING_RULE_BY_LARB)) {
				gles_head_new = disp_info->gles_head[disp_idx];
				gles_tail_new = disp_info->gles_tail[disp_idx];
				if ((gles_head_old != gles_head_new ||
					gles_tail_old != gles_tail_new) &&
					gles_head_new != gles_tail_new) {
					layer_info_new = &disp_info->input_config[
							disp_idx][gles_head_new];
					weight = get_layer_weight(drm_dev, disp_idx, NULL,
						disp_info->frame_idx[disp_idx], true, false);
					DDPDBG_HBL(
						"%s,crtc:%u,update gles(%d,%d)->(%d,%d),w:%u->%u,off(0x%x,0x%x),w/h(%u,%u),ID:0x%llx\n",
						__func__, disp_info->disp_idx,
						gles_head_old, gles_tail_old,
						gles_head_new, gles_tail_new,
						layer_info_new->layer_hrt_weight, weight,
						layer_info_new->dst_offset_x,
						layer_info_new->dst_offset_y,
						layer_info_new->dst_width,
						layer_info_new->dst_height,
						layer_info_new->buffer_alloc_id);
					layer_info_new->layer_hrt_weight = weight;
				}
			}
		}

		/* adjust hrt_num */
		disp_info->hrt_num =
			get_hrt_level(max_ovl_cnt * HRT_UINT_BOUND_BPP, 0);
		disp_info->hrt_weight = max_ovl_cnt * 4;
		if ((disp == 0) && get_layering_opt(LYE_OPT_OVL_BW_MONITOR) &&
			(disp_info->hrt_weight <= sum_overlap_w_of_bwm))
			sum_overlap_w_of_bwm = disp_info->hrt_weight;
		DDPINFO("%s,crtc:%u max_ovl_cnt=%d, overlap:%d,%d\n", __func__,
			disp_info->disp_idx, max_ovl_cnt,
			disp_info->hrt_weight, sum_overlap_w_of_bwm);
	}

	return 0;
}

static int dispatch_ovl_id(struct drm_mtk_layering_info *disp_info,
			   struct mtk_drm_lyeblob_ids *lyeblob_ids,
			   struct drm_device *drm_dev, struct mtk_lye_ddp_state *lye_state)
{

	bool no_disp = true;
	int disp_idx, idx;

	for (idx = 0; idx < HRT_DISP_TYPE_NUM; idx++)
		if (disp_info->layer_num[idx] > 0) {
			no_disp = false;
			break;
		}

	if (no_disp) {
		DDPINFO("There is no disp need dispatch\n");
		return 0;
	}

	if (get_layering_opt(LYE_OPT_SPHRT))
		disp_idx = disp_info->disp_idx;
	else
		disp_idx = 0;

	/* Dispatch OVL id */
	for (idx = 0 ; disp_idx < HRT_DISP_TYPE_NUM; idx++, disp_idx++) {
		int ovl_cnt;
		uint16_t layer_map;
		struct drm_mtk_layer_config *c;
		bool clear = false;
		int i = 0;

		if (disp_info->layer_num[idx] <= 0)
			continue;

		/* check exist clear layer */
		for (i = 0; i < disp_info->layer_num[idx]; i++) {
			c = &disp_info->input_config[idx][i];
			if (mtk_has_layer_cap(c, MTK_DISP_CLIENT_CLEAR_LAYER)) {
				clear = true;
				break;
			}
		}

		ovl_cnt = get_phy_ovl_layer_cnt(disp_info, idx);
		if (clear)
			ovl_cnt++;
		layer_map = l_rule_ops->get_mapping_table(
			drm_dev, disp_idx, disp_info->disp_list, DISP_HW_LAYER_TB, ovl_cnt);

		_dispatch_lye_blob_idx(disp_info, layer_map,
			disp_idx, lyeblob_ids, drm_dev, lye_state);

		if (get_layering_opt(LYE_OPT_SPHRT))
			break;
	}
	return 0;
}

static int check_layering_result(struct drm_mtk_layering_info *info)
{
	int disp_idx;
	bool no_disp = true;

	for (disp_idx = 0; disp_idx < HRT_DISP_TYPE_NUM; disp_idx++)
		if (info->layer_num[disp_idx] > 0) {
			no_disp = false;
			break;
		}

	if (no_disp) {
		DDPINFO("There is no disp need check\n");
		return 0;
	}

	for (disp_idx = 0; disp_idx < HRT_DISP_TYPE_NUM; disp_idx++) {
		int layer_num, max_ovl_id, ovl_layer_num;
		struct drm_mtk_layer_config *c = NULL;
		int i;

		if (info->layer_num[disp_idx] <= 0)
			continue;

		if (disp_idx == HRT_PRIMARY)
			ovl_layer_num = PRIMARY_OVL_LAYER_NUM;
		else
			ovl_layer_num = SECONDARY_OVL_LAYER_NUM;
		layer_num = info->layer_num[disp_idx];
		max_ovl_id = info->input_config[disp_idx][layer_num - 1].ovl_id;

		if (max_ovl_id >= ovl_layer_num)
			DDPAEE("Inv ovl:%d,disp:%d\n", max_ovl_id, disp_idx);

		if (info->gles_head[HRT_PRIMARY] == -1)
			continue;

		for (i = info->gles_head[HRT_PRIMARY]; i <= info->gles_tail[HRT_PRIMARY]; i++) {
			c = &info->input_config[HRT_PRIMARY][i];
			c->ext_sel_layer = -1;

			if (mtk_has_layer_cap(c, MTK_DISP_CLIENT_CLEAR_LAYER))
				continue;

			if (mtk_has_layer_cap(c, MTK_MML_DISP_DIRECT_DECOUPLE_LAYER |
						 MTK_MML_DISP_DIRECT_LINK_LAYER))
				c->layer_caps &= ~DISP_MML_CAPS_MASK;
		}
	}

	return 0;
}

static int check_disp_info(struct drm_mtk_layering_info *disp_info)
{
	int disp_idx;

	if (disp_info == NULL) {
		DDPPR_ERR("[HRT]disp_info is empty\n");
		return -1;
	}

	for (disp_idx = 0; disp_idx < HRT_DISP_TYPE_NUM; disp_idx++) {
		int mode = disp_info->disp_mode[disp_idx];
		int ghead = disp_info->gles_head[disp_idx];
		int gtail = disp_info->gles_tail[disp_idx];
		int layer_num = disp_info->layer_num[disp_idx];

		if (mode < 0 || mode >= MTK_DRM_SESSION_NUM) {
			DDPPR_ERR("[HRT] disp_idx %d, invalid mode %d\n", disp_idx, mode);
			return -1;
		}

		if (layer_num < 0) {
			DDPPR_ERR("[HRT] disp_idx %d, invalid layer num %d\n", disp_idx, layer_num);
			return -1;
		}

		if (layer_num > 0 && disp_info->input_config[disp_idx] == NULL) {
			DDPPR_ERR("[HRT] input config is empty, disp:%d, l_num:%d\n", disp_idx,
				  layer_num);
			return -1;
		}

		if ((ghead < 0 && gtail >= 0) || (gtail < 0 && ghead >= 0) || (gtail < ghead) ||
		    (gtail >= layer_num)) {
			dump_disp_info(disp_info, DISP_DEBUG_LEVEL_ERR);
			DDPPR_ERR("[HRT] gles invalid, disp:%d, head:%d, tail:%d\n", disp_idx,
				  disp_info->gles_head[disp_idx], disp_info->gles_tail[disp_idx]);
			return -1;
		}
	}

	/* these are set by kernel, should be 0 */
	if (disp_info->res_idx || disp_info->hrt_weight || disp_info->hrt_idx) {
		DDPPR_ERR("[HRT] fail, res_idx %d, hrt_weight %u, hrt_idx %u\n", disp_info->res_idx,
			  disp_info->hrt_weight, disp_info->hrt_idx);
		return -1;
	}

	return 0;
}

static int
_copy_layer_info_from_disp(struct drm_mtk_layering_info *disp_info_user,
			   int debug_mode, int disp_idx)
{
	struct drm_mtk_layering_info *l_info = &layering_info;
	unsigned long layer_size = 0, mml_cfg_size = 0;
	int ret = 0, layer_num = 0;

	if (disp_idx < 0 || disp_idx >= LYE_CRTC) {
		DDPPR_ERR("%s disp_idx=%d is invalid\n", __func__, disp_idx);
		return -EFAULT;
	}

	if (l_info->layer_num[disp_idx] <= 0) {
		/* direct skip */
		return 0;
	}

	layer_num = l_info->layer_num[disp_idx];
	layer_size = sizeof(struct drm_mtk_layer_config) * layer_num;
	l_info->input_config[disp_idx] = vzalloc(layer_size);
	if (l_info->input_config[disp_idx] == NULL) {
		DDPPR_ERR("%s:%d invalid input_config[%d]:0x%p\n",
			__func__, __LINE__,
			disp_idx, l_info->input_config[disp_idx]);
		return -ENOMEM;
	}

	mml_cfg_size = sizeof(struct mml_frame_info) * layer_num;
	l_info->mml_cfg[disp_idx] = vzalloc(mml_cfg_size);
	if (l_info->mml_cfg[disp_idx] == NULL) {
		DDPPR_ERR("%s:%d invalid mml_cfg[%d]:0x%p\n",
			__func__, __LINE__,
			disp_idx, l_info->mml_cfg[disp_idx]);
		return -ENOMEM;
	}

	if (debug_mode) {
		memcpy(l_info->input_config[disp_idx],
		       disp_info_user->input_config[disp_idx], layer_size);
	} else {
		if (copy_from_user(l_info->input_config[disp_idx],
				   disp_info_user->input_config[disp_idx],
				   layer_size)) {
			DDPPR_ERR("%s:%d copy failed:(0x%p,0x%p), size:%ld\n",
					__func__, __LINE__,
					l_info->input_config[disp_idx],
					disp_info_user->input_config[disp_idx],
					layer_size);
			return -EFAULT;
		}
		if (copy_from_user(l_info->mml_cfg[disp_idx],
				   disp_info_user->mml_cfg[disp_idx],
				   mml_cfg_size)) {
			DDPMSG("%s:%d copy failed:(0x%p,0x%p), size:%ld\n",
					__func__, __LINE__,
					l_info->mml_cfg[disp_idx],
					disp_info_user->mml_cfg[disp_idx],
					mml_cfg_size);
			return -EFAULT;
		}
	}

	return ret;
}

static int set_disp_info(struct drm_mtk_layering_info *disp_info_user,
			 int debug_mode)
{
	int i;

	mutex_lock(&layering_info_lock);
	memcpy(&layering_info, disp_info_user,
		sizeof(struct drm_mtk_layering_info));


	for (i = 0; i < HRT_DISP_TYPE_NUM; i++) {
		if (_copy_layer_info_from_disp(disp_info_user, debug_mode, i)) {
			mutex_unlock(&layering_info_lock);
			return -EFAULT;
		}
	}

	memset(l_rule_info->addon_scn, 0x0, sizeof(l_rule_info->addon_scn));
	mutex_unlock(&layering_info_lock);
	return 0;
}

static int
_copy_layer_info_by_disp(struct drm_mtk_layering_info *disp_info_user,
			 int debug_mode, unsigned int disp_idx)
{
	struct drm_mtk_layering_info *l_info = &layering_info;
	unsigned long layer_size = 0;
	int ret = 0;

	if (l_info->layer_num[disp_idx] <= 0) {
		/* direct skip */
		return -EFAULT;
	}

	disp_info_user->gles_head[disp_idx] = l_info->gles_head[disp_idx];
	disp_info_user->gles_tail[disp_idx] = l_info->gles_tail[disp_idx];
	disp_info_user->disp_caps[disp_idx] = l_info->disp_caps[disp_idx];

	layer_size = sizeof(struct drm_mtk_layer_config) *
		     disp_info_user->layer_num[disp_idx];

	if (debug_mode) {
		memcpy(disp_info_user->input_config[disp_idx],
		       l_info->input_config[disp_idx], layer_size);
	} else {
		if (copy_to_user(disp_info_user->input_config[disp_idx],
				 l_info->input_config[disp_idx], layer_size)) {
			DDPINFO("[DISP][FB]: copy_to_user failed! line:%d\n",
				__LINE__);
			ret = -EFAULT;
		}
		vfree(l_info->input_config[disp_idx]);
		vfree(l_info->mml_cfg[disp_idx]);
	}

	return ret;
}

static int copy_layer_info_to_user(struct drm_device *dev,
	struct drm_mtk_layering_info *disp_info_user, int debug_mode)
{
	int ret = 0;
	unsigned int i;
	struct drm_mtk_layering_info *l_info = &layering_info;

	disp_info_user->hrt_num = l_info->hrt_num;
	if (get_layering_opt(LYE_OPT_SPHRT))
		disp_info_user->hrt_idx = _layering_rule_get_hrt_idx(l_info->disp_idx);
	else
		disp_info_user->hrt_idx = _layering_rule_get_hrt_idx(0);
	disp_info_user->hrt_weight = l_info->hrt_weight;

	if ((l_info->disp_idx == 0) && get_layering_opt(LYE_OPT_OVL_BW_MONITOR))
		disp_info_user->hrt_weight = sum_overlap_w_of_bwm;

	for (i = 0; i < HRT_DISP_TYPE_NUM; i++)
		_copy_layer_info_by_disp(disp_info_user, debug_mode, i);

	return ret;
}

#ifdef HRT_UT_DEBUG
static int set_hrt_state(enum HRT_SYS_STATE sys_state, int en)
{
	switch (sys_state) {
	case DISP_HRT_MJC_ON:
		if (en)
			l_rule_info->hrt_sys_state |= (1 << sys_state);
		else
			l_rule_info->hrt_sys_state &= ~(1 << sys_state);
		break;
	case DISP_HRT_FORCE_DUAL_OFF:
		if (en)
			l_rule_info->hrt_sys_state |= (1 << sys_state);
		else
			l_rule_info->hrt_sys_state &= ~(1 << sys_state);
		break;
	case DISP_HRT_MULTI_TUI_ON:
		if (en)
			l_rule_info->hrt_sys_state |= (1 << sys_state);
		else
			l_rule_info->hrt_sys_state &= ~(1 << sys_state);
		break;
	default:
		DDPPR_ERR("unknown hrt scenario\n");
		break;
	}

	DDPMSG("Set hrt sys_state:%d, en:%d\n", sys_state, en);
	return 0;
}
#endif

void mtk_register_layering_rule_ops(struct layering_rule_ops *ops,
				    struct layering_rule_info_t *info)
{
	l_rule_ops = ops;
	l_rule_info = info;
}

void lye_add_blob_ids(struct drm_mtk_layering_info *l_info,
		      struct mtk_drm_lyeblob_ids *lyeblob_ids,
		      struct drm_device *drm_dev,
		      int crtc_num, int crtc_mask,
		      struct mtk_lye_ddp_state *lye_state)
{
	struct drm_property_blob *blob;
	struct mtk_drm_private *priv = drm_dev->dev_private;
	struct drm_crtc *crtc;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	unsigned int disp_idx = 0;
	unsigned int i;

	memcpy(lye_state->scn, l_rule_info->addon_scn, sizeof(lye_state->scn));
	for (i = 0 ; i < HRT_DISP_TYPE_NUM ; i++) {
		if (lye_state->scn[i] >= ADDON_SCN_NR) {
			DDPPR_ERR("[%s]abnormal scn[%u]:%d,set scn to 0\n",
				  __func__, i, lye_state->scn[i]);
			lye_state->scn[i] = NONE;
		}
	}
	lye_state->lc_tgt_layer = 0;
	l_rule_info->bk_mml_dl_lye = lye_state->mml_dl_lye;

	if (get_layering_opt(LYE_OPT_SPHRT))
		disp_idx = l_info->disp_idx;

	lye_state->need_repaint = (l_info->disp_caps[disp_idx]==MTK_NEED_REPAINT);

	crtc = priv->crtc[disp_idx];
	if (crtc) {
		mtk_crtc = to_mtk_crtc(crtc);
	} else {
		DDPPR_ERR("%s:%d can't get mtk_crtc\n", __func__, __LINE__);
		return;
	}

	blob = drm_property_create_blob(
		drm_dev, sizeof(struct mtk_lye_ddp_state), lye_state);

	lyeblob_ids->lye_idx = _layering_rule_get_hrt_idx(disp_idx);
	lyeblob_ids->frame_weight = l_info->hrt_weight;
	if (disp_idx == 0)
		lyeblob_ids->frame_weight_of_bwm = sum_overlap_w_of_bwm;
	lyeblob_ids->hrt_num = l_info->hrt_num;
	lyeblob_ids->ddp_blob_id = blob->base.id;
	lyeblob_ids->ref_cnt = crtc_num;
	lyeblob_ids->ref_cnt_mask = crtc_mask;
	lyeblob_ids->free_cnt_mask = crtc_mask;
	lyeblob_ids->hrt_valid = g_hrt_valid;
	lyeblob_ids->disp_status = l_info->disp_list;
	INIT_LIST_HEAD(&lyeblob_ids->list);
	mutex_lock(&priv->lyeblob_list_mutex);
	if (get_layering_opt(LYE_OPT_SPHRT))
		list_add_tail(&lyeblob_ids->list, &mtk_crtc->lyeblob_head);
	else
		list_add_tail(&lyeblob_ids->list, &priv->lyeblob_head);
	DRM_MMP_MARK(layering_blob, lyeblob_ids->lye_idx, lyeblob_ids->frame_weight);
	mutex_unlock(&priv->lyeblob_list_mutex);
}

static bool is_rsz_valid(struct drm_mtk_layer_config *c)
{
	if (c->src_width == c->dst_width && c->src_height == c->dst_height)
		return false;
	if (c->src_width > c->dst_width || c->src_height > c->dst_height)
		return false;
	/*
	 * HWC adjusts MDP layer alignment after query_valid_layer.
	 * This makes the decision of layering rule unreliable. Thus we
	 * add constraint to avoid frame_cfg becoming scale-down.
	 *
	 * TODO: If HWC adjusts MDP layer alignment before
	 * query_valid_layer, we could remove this if statement.
	 */

	/* HWC adjusts MDP layer alignment, we remove this if statement */
#ifdef IF_ZERO
	if ((mtk_has_layer_cap(c, MTK_MDP_RSZ_LAYER) ||
	     mtk_has_layer_cap(c, MTK_DISP_RSZ_LAYER)) &&
	    (c->dst_width - c->src_width <= MDP_ALIGNMENT_MARGIN ||
	     c->dst_height - c->src_height <= MDP_ALIGNMENT_MARGIN))
		return false;
#endif
	return true;
}


#define RATIO_LIMIT  2
static bool same_ratio_limitation(struct drm_crtc *crtc,
	struct drm_mtk_layer_config *tgt, int limitation,
	int panel_w, int panel_h)
{
	int diff_w = 0, diff_h = 0;

	diff_w = tgt->dst_width - tgt->src_width;
	diff_h = tgt->dst_height - tgt->src_height;
	if (panel_w <= 0 || panel_h <= 0)
		return false;
	if (((100 * diff_w/panel_w < limitation) && (diff_w > 0)) ||
			((100 * diff_h/panel_h < limitation) && (diff_h > 0)))
		return true;
	else
		return false;
}

#define UNIT 32768
static int check_cross_pipe_rpo(struct mtk_rsz_param param[2],
	struct mtk_drm_crtc *mtk_crtc,
	unsigned int src_x, unsigned int src_w,
	unsigned int dst_x, unsigned int dst_w,
	unsigned int disp_w)
{
	int left = dst_x;
	int right = dst_x + dst_w - 1;
	int tile_idx = 0;
	u32 out_tile_loss[2] = {0, 0};
	u32 in_tile_loss[2] = {out_tile_loss[0] + 4, out_tile_loss[1] + 4};
	u32 step = 0;
	s32 init_phase = 0;
	s32 offset[2] = {0};
	s32 int_offset[2] = {0};
	s32 sub_offset[2] = {0};
	u32 tile_in_len[2] = {0};
	u32 tile_out_len[2] = {0};
	u32 out_x[2] = {0};
	int width = disp_w;
	struct total_tile_overhead to_info;

	to_info = mtk_crtc_get_total_overhead(mtk_crtc);
	DDPINFO("%s:overhead is_support:%d, width L:%d R:%d\n", __func__,
			to_info.is_support, to_info.left_in_width, to_info.right_in_width);

	if (right < width / 2)
		tile_idx = 0;
	else if (left >= width / 2)
		tile_idx = 1;

	step = (UNIT * (src_w - 1) + (dst_w - 2)) /
			(dst_w - 1);

	offset[0] = (step * (dst_w - 1) -
		UNIT * (src_w - 1)) / 2;
	init_phase = UNIT - offset[0];
	sub_offset[0] = -offset[0];
	if (sub_offset[0] < 0) {
		int_offset[0]--;
		sub_offset[0] = UNIT + sub_offset[0];
	}
	if (sub_offset[0] >= UNIT) {
		int_offset[0]++;
		sub_offset[0] = sub_offset[0] - UNIT;
	}

	/*left side*/
	out_tile_loss[0] = (to_info.is_support ? to_info.left_overhead : 0);
	in_tile_loss[0] = out_tile_loss[0] + 4;

	tile_in_len[0] = (((width / 2 - dst_x) * src_w * 10) /
		dst_w + 5) / 10;
	if (tile_in_len[0] + in_tile_loss[0] >= src_w)
		in_tile_loss[0] = src_w - tile_in_len[0];

	tile_out_len[0] = width / 2 - dst_x + out_tile_loss[0];
	if (tile_in_len[0] + in_tile_loss[0] > tile_out_len[0])
		in_tile_loss[0] = tile_out_len[0] - tile_in_len[0];
	tile_in_len[0] += in_tile_loss[0];
	out_x[0] = dst_x;

	param[tile_idx].out_x = out_x[0];
	param[tile_idx].step = step;
	param[tile_idx].int_offset = (u32)(int_offset[0] & 0xffff);
	param[tile_idx].sub_offset = (u32)(sub_offset[0] & 0x1fffff);
	param[tile_idx].in_len = tile_in_len[0];
	param[tile_idx].out_len = tile_out_len[0];

	if (int_offset[0] < -1){
		DDPDBG("pipe0_scale_err\n");
		return -1;
	}

	/* right half */
	out_tile_loss[1] = (to_info.is_support ? to_info.right_overhead : 0);
	in_tile_loss[1] = out_tile_loss[1] + 4;
	tile_out_len[1] = dst_w - (tile_out_len[0] - out_tile_loss[0]) + out_tile_loss[1];
	tile_in_len[1] = (((tile_out_len[1] - out_tile_loss[1]) * src_w * 10) /
		dst_w + 5) / 10;
	if (tile_in_len[1] + in_tile_loss[1] >= src_w)
		in_tile_loss[1] = src_w - tile_in_len[1];

	if (tile_in_len[1] + in_tile_loss[1] > tile_out_len[1])
		in_tile_loss[1] = tile_out_len[1] - tile_in_len[1];
	tile_in_len[1] += in_tile_loss[1];

	offset[1] = (-offset[0]) + ((tile_out_len[0] - out_tile_loss[0] -
			out_tile_loss[1]) * step) -
			(src_w - tile_in_len[1]) * UNIT;
	int_offset[1] = offset[1] / UNIT;
	if (offset[1] >= 0)
		sub_offset[1] = offset[1] - UNIT * int_offset[1];
	else
		sub_offset[1] = UNIT * int_offset[1] - offset[1];

	param[1].step = step;
	param[1].out_x = 0;
	param[1].int_offset = (u32)(int_offset[1] & 0xffff);
	param[1].sub_offset = (u32)(sub_offset[1] & 0x1fffff);
	param[1].in_len = tile_in_len[1];
	param[1].out_len = tile_out_len[1];

	DDPINFO("%s in_ph:%d offset1:%d step:%u\n", __func__, init_phase, offset[1], step);
	DDPINFO("%s offset:%u.%u len:%u->%u out_x:%u out_tileloss:%d, in_tileloss:%d\n", __func__,
		param[0].int_offset, param[0].sub_offset, param[0].in_len, param[0].out_len,
		param[0].out_x, out_tile_loss[0], in_tile_loss[0]);
	DDPINFO("%s offset:%u.%u len:%u->%u out_x:%u out_tileloss:%d, in_tileloss:%d\n", __func__,
		param[1].int_offset, param[1].sub_offset, param[1].in_len, param[1].out_len,
		param[1].out_x, out_tile_loss[1], in_tile_loss[1]);

	if ((param[1].in_len == param[1].out_len) ||
		(int_offset[1] < -1 || tile_out_len[1] >= dst_w)) {
		DDPDBG("pipe1_scale_err\n");
		return -1;
	}

	if (tile_in_len[1] > tile_out_len[1] || tile_in_len[0] > tile_out_len[0])
		return -1;

	return 0;
}

static void RPO_rule(struct mtk_drm_private *priv, struct drm_crtc *crtc,
		struct drm_mtk_layering_info *disp_info, int disp_idx)
{
	struct drm_mtk_layer_config *c = NULL;
	struct mtk_rect src_layer_roi = {0};
	struct mtk_rect dst_layer_roi = {0};
	struct mtk_rect src_roi = {0};
	struct mtk_rect dst_roi = {0};
	unsigned int disp_w, disp_h;
	struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtc);
	struct drm_display_mode *mode;
	int i = 0;
	u8 scale_cnt = 0;

	if (disp_idx >= LYE_CRTC || disp_idx < 0) {
		DDPPR_ERR("%s[%d]:idx:%d\n", __func__, __LINE__, disp_idx);
		return;
	}

	if (disp_info->layer_num[disp_idx] <= 0)
		return;

	mode = mtk_drm_crtc_avail_disp_mode(crtc, disp_info->disp_mode_idx[0]);
	if (mode) {
		disp_w = mode->hdisplay;
		disp_h = mode->vdisplay;
	} else {
		disp_w = crtc->state->adjusted_mode.hdisplay;
		disp_h = crtc->state->adjusted_mode.vdisplay;
	}

	for (i = 0; i < disp_info->layer_num[disp_idx]; i++) {
		struct mtk_rsz_param param[2] = {0};

		c = &disp_info->input_config[disp_idx][i];

		/*if (i == 0 && c->src_fmt == MTK_DRM_FORMAT_DIM)
		 *	continue;
		 */

		if (disp_info->gles_head[disp_idx] >= 0 &&
		    disp_info->gles_head[disp_idx] <= i)
			continue;

		/* RSZ HW limitation */
		/* 4x4 < input resolution size */
		if ((c->src_width <= 4) || (c->src_height <= 4))
			continue;

		if (!is_rsz_valid(c))
			continue;

		if (same_ratio_limitation(crtc, c, RATIO_LIMIT, disp_w, disp_h))
			continue;

		mtk_rect_make(&src_layer_roi,
			((c->dst_offset_x * c->src_width * 10) / c->dst_width + 5) / 10,
			((c->dst_offset_y * c->src_height * 10)	/ c->dst_height + 5) / 10,
			c->src_width, c->src_height);
		mtk_rect_make(&dst_layer_roi,
			c->dst_offset_x, c->dst_offset_y,
			c->dst_width, c->dst_height);
		mtk_rect_join(&src_layer_roi, &src_roi, &src_roi);
		mtk_rect_join(&dst_layer_roi, &dst_roi, &dst_roi);
		if (src_roi.width > dst_roi.width || src_roi.height > dst_roi.height) {
			DDPPR_ERR(
				"L%d:scale down(%d,%d,%dx%d)->(%d,%d,%dx%d)\n",
				i, src_roi.x, src_roi.y, src_roi.width,
				src_roi.height, dst_roi.x, dst_roi.y,
				dst_roi.width, dst_roi.height);
			continue;
		}

		if (!is_layer_across_each_pipe(crtc, c, disp_w))
			continue;

		if (mtk_crtc->is_dual_pipe &&
			check_cross_pipe_rpo(param, mtk_crtc, src_roi.x, src_roi.width,
						dst_roi.x, dst_roi.width, disp_w))
			continue;

		if (mtk_crtc->is_dual_pipe) {
			if (param[0].in_len > l_rule_info->rpo_tile_length ||
				param[1].in_len > l_rule_info->rpo_tile_length ||
				src_roi.height > l_rule_info->rpo_in_max_height)
				continue;
		} else {
			if (src_roi.width > l_rule_info->rpo_tile_length ||
				src_roi.height > l_rule_info->rpo_in_max_height)
				continue;
		}

		if (mtk_has_layer_cap(c, MTK_MML_DISP_DIRECT_DECOUPLE_LAYER |
					 MTK_MML_DISP_DIRECT_LINK_LAYER))
			continue;

		if (mtk_has_layer_cap(c, MTK_MDP_RSZ_LAYER) &&
			priv->data->mmsys_id != MMSYS_MT6897 &&
			priv->data->mmsys_id != MMSYS_MT6878)
			continue;

		if (scale_cnt >= l_rule_info->rpo_scale_num)
			break;

		c->layer_caps |= MTK_DISP_RSZ_LAYER;
		++scale_cnt;
	}
}

/* resizing_rule - layering rule resize layer layout */
static void resizing_rule(struct drm_device *dev,
			struct drm_mtk_layering_info *disp_info)
{
	const u8 disp_idx = get_layering_opt(LYE_OPT_SPHRT) ? disp_info->disp_idx : 0;

	mtk_rollback_all_resize_layer_to_GPU(disp_info, HRT_SECONDARY);
	mtk_rollback_all_resize_layer_to_GPU(disp_info, HRT_THIRD);

	/* if SPHRT, every crtc is HRT_PRIMARY, but only crtc 0 support RPO */
	if (disp_idx == 0) {
		struct mtk_drm_private *priv = dev->dev_private;
		struct drm_crtc *crtc = priv->crtc[disp_idx];

		if (crtc)
			RPO_rule(priv, crtc, disp_info, disp_idx);
	}

	/* for crtc N layers that cannot supported by RPO */
	mtk_rollback_all_resize_layer_to_GPU(disp_info, HRT_PRIMARY);
}

static enum MTK_LAYERING_CAPS query_MML(struct drm_device *dev, struct drm_crtc *crtc,
					struct mml_frame_info *mml_info, const u32 line_time_ns)
{
	enum mml_mode mode = MML_MODE_UNKNOWN;
	enum MTK_LAYERING_CAPS ret = MTK_MML_DISP_NOT_SUPPORT;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct mtk_drm_private *priv = NULL;
	struct drm_crtc *crtcx = NULL;
	struct mml_drm_ctx *mml_ctx = NULL;

	if (!dev || !crtc) {
		DDPMSG("%s !dev, !crtc\n", __func__);
		return ret;
	}
	priv = dev->dev_private;
	mtk_crtc = to_mtk_crtc(crtc);

	if (!(mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MML_PRIMARY)))
		return ret;

	if (unlikely(!mml_info)) {
		DDPMSG("%s !mml_info\n", __func__);
		return ret;
	}

	if (unlikely(mtk_crtc->mml_debug & DISP_MML_DBG_LOG))
		print_mml_frame_info(*mml_info);

	if (unlikely(g_mml_mode != MML_MODE_UNKNOWN)) {
		mode = g_mml_mode;
		goto mode_mapping;
	}

	mml_info->act_time = mml_info->dest[0].compose.height * line_time_ns;

	/* set to DC if another display is on */
	drm_for_each_crtc(crtcx, dev) {
		if (crtcx && (drm_crtc_index(crtcx) != 0)) {
			struct mtk_drm_crtc *mtk_crtc = to_mtk_crtc(crtcx);

			if (mtk_crtc->wk_lock->active)
				mml_info->mode = MML_MODE_MML_DECOUPLE;
		}
	}

	if (mtk_crtc->mml_prefer_dc)
		mml_info->mode = MML_MODE_MML_DECOUPLE;

	if (mtk_crtc_is_frame_trigger_mode(crtc) && (!mtk_crtc->mml_cmd_ir) &&
		!mtk_drm_helper_get_opt(priv->helper_opt, MTK_DRM_OPT_MML_SUPPORT_CMD_MODE))
		mml_info->mode = MML_MODE_MML_DECOUPLE;

	mml_ctx = mtk_drm_get_mml_drm_ctx(dev, crtc);
	if (mml_ctx != NULL) {
		mode = mml_drm_query_cap(mml_ctx, mml_info);
		DDPDBG("%s, mml_drm_query_cap mode:%d\n", __func__, mode);
	} else
		return ret;

mode_mapping:
	switch (mode) {
	case MML_MODE_NOT_SUPPORT:
		ret = MTK_MML_DISP_NOT_SUPPORT;
		break;
	case MML_MODE_DIRECT_LINK:
		ret = MTK_MML_DISP_DIRECT_LINK_LAYER;
		break;
	case MML_MODE_RACING:
		ret = MTK_MML_DISP_DIRECT_DECOUPLE_LAYER;
		break;
	case MML_MODE_MML_DECOUPLE:
		ret = MTK_MML_DISP_DECOUPLE_LAYER;
		break;
	case MML_MODE_MDP_DECOUPLE:
		ret = MTK_MML_DISP_MDP_LAYER;
		break;
	case MML_MODE_UNKNOWN:
	default:
		ret = MTK_MML_DISP_NOT_SUPPORT;
		break;
	}

	return ret;
}

static void check_is_mml_layer(const int disp_idx,
	struct drm_mtk_layering_info *disp_info, struct drm_device *dev,
	const unsigned int hrt_idx)
{
	struct drm_crtc *crtc = NULL;
	struct mtk_drm_crtc *mtk_crtc = NULL;
	struct drm_mtk_layer_config *c = NULL;
	int i = 0;
	enum MTK_LAYERING_CAPS mml_capacity = DISP_MML_CAPS_MASK;
	struct mtk_drm_private *priv = NULL;
	struct mml_frame_info *mml_info = NULL;
	struct mtk_ddp_comp *output_comp = NULL;
	u32 ns = 0;
	u32 mml_ovl_layers = 0;
	u8 down_scale_cnt = 0;
	bool mml_dc_layers = false;

	if (!dev || !disp_info)
		return;

	if (disp_idx >= LYE_CRTC || disp_idx < 0) {
		DDPPR_ERR("%s[%d]:disp_idx:%d\n", __func__, __LINE__, disp_idx);
		return;
	}

	drm_for_each_crtc(crtc, dev)
		if (drm_crtc_index(crtc) == disp_idx)
			break;
	mtk_crtc = to_mtk_crtc(crtc);
	priv = dev->dev_private;

	for (i = 0; i < disp_info->layer_num[disp_idx]; i++) {
		c = &disp_info->input_config[disp_idx][i];
		if ((MTK_MML_OVL_LAYER & c->layer_caps) && (!c->wcg_force_gpu)) {
			if (i < 32)
				mml_ovl_layers |= (1 << i);
			else if (priv->data->mmsys_id != MMSYS_MT6878) {
				DDPMSG("disp can't handle layer_idx%d as mml layer\n", i);
				c->layer_caps &= ~MTK_MML_OVL_LAYER;
				return;
			} else {
				DDPMSG("disp can't handle layer_idx%d as mml layer&no support MDP, handle by gpu\n", i);
				c->layer_caps |= MTK_MML_DISP_NOT_SUPPORT;
			}
			if (calc_mml_rsz_ratio(&(disp_info->mml_cfg[disp_idx][i])) > 100)
				down_scale_cnt++;
		}
	}
	if ((down_scale_cnt > 1) && !(mtk_crtc->is_mml_dl
		|| mtk_crtc->is_mml || l_rule_info->bk_mml_dl_lye)) {
		u32 cnt = __builtin_popcount(mml_ovl_layers);
		enum MTK_LAYERING_CAPS dc_cap = MTK_MML_DISP_DECOUPLE_LAYER;

		while (cnt--) {
			i = __builtin_ffs(mml_ovl_layers) - 1;
			mml_ovl_layers &= ~(1 << i);
			c = &disp_info->input_config[disp_idx][i];
			c->layer_caps |= dc_cap;
			DDPINFO("MML down scale layer(%d) caps(%u)\n", i, dc_cap);
			dc_cap = MTK_MML_DISP_MDP_LAYER;
		}
		return;
	}

	for (i = 0; i < disp_info->layer_num[disp_idx]; i++) {
		if (disp_idx >= 0 && disp_idx < LYE_CRTC)
			c = &disp_info->input_config[disp_idx][i];
		if (!(MTK_MML_OVL_LAYER & c->layer_caps) || c->wcg_force_gpu)
			continue;

		if (disp_idx >= 0 && disp_idx < LYE_CRTC)
			mml_info = &(disp_info->mml_cfg[disp_idx][i]);
		else
			continue;

		if (!mml_info)
			continue;

		if (!output_comp) {
			/* Check line time and slbc state once per HRT */
			mutex_lock(&priv->commit.lock);
			output_comp = mtk_ddp_comp_request_output(mtk_crtc);
			if (output_comp && (mtk_ddp_comp_get_type(output_comp->id) == MTK_DSI))
				mtk_ddp_comp_io_cmd(output_comp, NULL, DSI_GET_LINE_TIME_NS, &ns);

			if (mtk_crtc->slbc_state == SLBC_NEED_FREE) {
				if (kref_read(&mtk_crtc->mml_ir_sram.ref))
					mml_info->mode = MML_MODE_MML_DECOUPLE;
				else
					mtk_crtc->slbc_state = SLBC_CAN_ALLOC;
			}
			mutex_unlock(&priv->commit.lock);
		}

		c->layer_caps |= query_MML(dev, crtc, mml_info, ns);

		if (MML_FMT_IS_YUV(disp_info->mml_cfg[disp_idx][i].src.format))
			c->layer_caps |= MTK_DISP_SRC_YUV_LAYER;

		/* Try to alloc sram for IR, rollback to DC if failed */
		if (MTK_MML_DISP_DIRECT_DECOUPLE_LAYER & c->layer_caps) {
			if (!mtk_crtc_alloc_sram(mtk_crtc, hrt_idx)) {
				c->layer_caps &= ~MTK_MML_DISP_DIRECT_DECOUPLE_LAYER;
				c->layer_caps |= MTK_MML_DISP_DECOUPLE_LAYER;
				DDPINFO("%s hrt_idx:%d unable to get sram, set to MML_DC\n",
					__func__, hrt_idx);
			}
		}

		/* If more than 1 MML layer, support only IR+GPU, DC+MDP */
		if (mtk_has_layer_cap(c, DISP_MML_CAPS_MASK)) {
			if (mml_capacity & c->layer_caps) {
				if (mtk_has_layer_cap(c, MTK_MML_DISP_DIRECT_DECOUPLE_LAYER |
							 MTK_MML_DISP_DIRECT_LINK_LAYER)) {
					mml_capacity = 0;
				} else {
					mml_capacity &= ~(MTK_MML_DISP_DIRECT_DECOUPLE_LAYER |
							  MTK_MML_DISP_DIRECT_LINK_LAYER);
					mml_capacity &= ~(DISP_MML_CAPS_MASK & c->layer_caps);
				}
			} else {
				DDPINFO("%s capacity exhausted %x -> %x\n", __func__,
					c->layer_caps & DISP_MML_CAPS_MASK, mml_capacity);
				c->layer_caps &= ~DISP_MML_CAPS_MASK;
				c->layer_caps |=
				    (mml_capacity ? mml_capacity : MTK_MML_DISP_NOT_SUPPORT);
			}
		}
		if ((MTK_MML_DISP_DECOUPLE_LAYER & c->layer_caps) &&
		    (kref_read(&mtk_crtc->mml_ir_sram.ref) ||
		     (mtk_crtc->mml_link_state == MML_IR_IDLE) ||
		     mtk_crtc->is_mml_dl || l_rule_info->bk_mml_dl_lye)) {
			c->layer_caps &= ~MTK_MML_DISP_DECOUPLE_LAYER;
			c->layer_caps |= MTK_MML_DISP_MDP_LAYER;
			disp_info->disp_caps[disp_idx] |= MTK_NEED_REPAINT;
			DDPINFO("Use MDP for %s-DC transition\n",
				mtk_crtc->is_mml_dl ? "DL" : "IR");
			DRM_MMP_MARK(layering, 0x331, __LINE__);
		}

		if ((MTK_MML_DISP_DIRECT_DECOUPLE_LAYER & c->layer_caps) &&
		    l_rule_info->bk_mml_dl_lye) {
			c->layer_caps &= ~MTK_MML_DISP_DIRECT_DECOUPLE_LAYER;
			c->layer_caps |= MTK_MML_DISP_MDP_LAYER;
			disp_info->disp_caps[disp_idx] |= MTK_NEED_REPAINT;
			DDPINFO("Use MDP for DL-IR transition\n");
			DRM_MMP_MARK(layering, 0x331, __LINE__);
		}

		if (skip_mdp) {
			if (mtk_crtc->dli_relay_1tnp && (MTK_MML_DISP_MDP_LAYER & c->layer_caps)) {
				c->layer_caps &= ~MTK_MML_DISP_MDP_LAYER;
				c->layer_caps |= MTK_MML_DISP_NOT_SUPPORT;
				DDPINFO("WA: replace MDP by GPU\n");
			}
		}

		if (MTK_MML_DISP_NOT_SUPPORT & c->layer_caps)
			mtk_gles_incl_layer(disp_info, disp_idx, i);

		if (mml_dc_layers == false &&
			(c->layer_caps & MTK_MML_DISP_DECOUPLE_LAYER))
			mml_dc_layers = true;
	}

	if (mtk_crtc->mml_cfg_dc && mml_dc_layers == false) {
		for (i = 0; i < MML_MAX_OUTPUTS; i++)
			kfree(mtk_crtc->mml_cfg_dc->pq_param[i]);
		kfree(mtk_crtc->mml_cfg_dc->job);
		kfree(mtk_crtc->mml_cfg_dc);
		mtk_crtc->mml_cfg_dc = NULL;
		DDPINFO("%s, dc:%d, cap:0x%x w/o 0x%x destroy mml_cfg_dc\n",
			__func__, mtk_crtc->is_mml_dc, c->layer_caps,
			MTK_MML_DISP_DECOUPLE_LAYER);
	}

	if (disp_info->gles_head[disp_idx] != -1) {
		int adjusted_gles_head = -1;
		int adjusted_gles_tail = -1;

		for (i = disp_info->gles_head[disp_idx]; i <= disp_info->gles_tail[disp_idx]; ++i) {
			c = &disp_info->input_config[disp_idx][i];
			c->ext_sel_layer = -1;
		}

		/* Scan gles range, bottom up */
		for (i = disp_info->gles_head[disp_idx]; i <= disp_info->gles_tail[disp_idx]; ++i) {
			c = &disp_info->input_config[disp_idx][i];
			if (!(DISP_MML_CAPS_MASK & c->layer_caps)) {
				adjusted_gles_head = i;
				break;
			}
		}

		/* Scan gles range, top down */
		for (i = disp_info->gles_tail[disp_idx]; i >= disp_info->gles_head[disp_idx]; --i) {
			c = &disp_info->input_config[disp_idx][i];
			if (!(DISP_MML_CAPS_MASK & c->layer_caps)) {
				adjusted_gles_tail = i;
				break;
			}
		}

		if (adjusted_gles_head <= adjusted_gles_tail) {
			disp_info->gles_head[disp_idx] = adjusted_gles_head;
			disp_info->gles_tail[disp_idx] = adjusted_gles_tail;
		} else {
			disp_info->gles_head[disp_idx] = -1;
			disp_info->gles_tail[disp_idx] = -1;
		}
	}
}

static int get_crtc_num(
	struct drm_mtk_layering_info *disp_info_user,
	int *crtc_mask)
{
	int i;
	int crtc_num;
	int input_config_num;

	if (!crtc_mask) {
		DDPPR_ERR("%s:%d null crtc_mask\n",
				__func__, __LINE__);
		return 0;
	}

	switch (disp_info_user->disp_mode[0]) {
	case MTK_DRM_SESSION_DL:
	case MTK_DRM_SESSION_DC_MIRROR:
		crtc_num = 1;
		break;
	case MTK_DRM_SESSION_DOUBLE_DL:
		crtc_num = 2;
		break;
	case MTK_DRM_SESSION_TRIPLE_DL:
		crtc_num = 3;
		break;
	default:
		crtc_num = 0;
		break;
	}

	/*
	 * when CRTC 0 disabled, disp_mode[0] would be 0,
	 * but it might still exist other display.
	 * Thus traverse each CRTC's disp_mode for
	 * active CRTC number
	 */
	if (crtc_num == 0) {
		for (i = 0 ; i < HRT_DISP_TYPE_NUM; i++)
			crtc_num += !!disp_info_user->disp_mode[i];
	}

	/* check input config number */
	input_config_num = 0;
	*crtc_mask = 0;
	for (i = 0; i < HRT_DISP_TYPE_NUM; i++) {
		if (disp_info_user->input_config[i]) {
			*crtc_mask |= (1 << i);
			input_config_num++;
		}
	}

	if (get_layering_opt(LYE_OPT_SPHRT)) {
		int disp_idx;

		disp_idx = disp_info_user->disp_idx;
		*crtc_mask = 1 << (disp_idx);

		/* in separate HRT, each iteration would only calculate one display information */
		crtc_num = 1;
	} else if (input_config_num != crtc_num) {
		DDPPR_ERR("%s:%d mode[%d] num:%d not matched config num:%d\n",
				__func__, __LINE__,
				disp_info_user->disp_mode[0],
				crtc_num, input_config_num);
		crtc_num = min(crtc_num, input_config_num);
	}

	return crtc_num;
}

static int layering_rule_start(struct drm_mtk_layering_info *disp_info_user,
			       int debug_mode, struct drm_device *dev)
{
	int ret;
	int overlap_num;
	struct mtk_drm_lyeblob_ids *lyeblob_ids;
	enum SCN_FACTOR scn_decision_flag = SCN_NO_FACTOR;
	int crtc_num, crtc_mask;
	int disp_idx = 0, hrt_idx;
	struct debug_gles_range dbg_gles = {-1, -1};
	struct mtk_lye_ddp_state lye_state = {0};
	struct mtk_drm_private *priv = dev->dev_private;

	DRM_MMP_EVENT_START(layering, (unsigned long)disp_info_user,
			(unsigned long)dev);

	roll_gpu_for_idle = 0;

	if (l_rule_ops == NULL || l_rule_info == NULL) {
		DRM_MMP_MARK(layering, 0, 0);
		DRM_MMP_EVENT_END(layering, 0, 0);
		DDPPR_ERR("Layering rule has not been initialize:(%p,%p)\n",
				l_rule_ops, l_rule_info);
		return -EFAULT;
	}

	if (check_disp_info(disp_info_user) < 0) {
		DRM_MMP_MARK(layering, 0, 1);
		DRM_MMP_EVENT_END(layering, 0, 0);
		DDPPR_ERR("check_disp_info fail\n");
		return -EFAULT;
	}

	if (set_disp_info(disp_info_user, debug_mode)) {
		DRM_MMP_MARK(layering, 0, 2);
		DRM_MMP_EVENT_END(layering, 0, 0);
		return -EFAULT;
	}

	check_gles_change(&dbg_gles, __LINE__, false);
	print_disp_info_to_log_buffer(&layering_info);
#ifdef HRT_DEBUG_LEVEL1
	DDPMSG("[Input data]\n");
	dump_disp_info(&layering_info, DISP_DEBUG_LEVEL_INFO);
#endif
	dump_disp_caps_info(dev, &layering_info, __LINE__);

	if (get_layering_opt(LYE_OPT_SPHRT))
		disp_idx = disp_info_user->disp_idx;
	/* fix the hrt bandwidth before the 1st valid input layers
	 * else hrt bandwidth will be 0 at boot logo scenario
	 * and underflow/underrun issue may happened
	 */
	if (g_hrt_valid == false &&
	    layering_info.layer_num[HRT_PRIMARY] > 0)
		g_hrt_valid = true;

	hrt_idx = _layering_rule_get_hrt_idx(disp_idx);
	if (++hrt_idx == 0xffffffff)
		hrt_idx = 0;
	_layering_rule_set_hrt_idx(disp_idx, hrt_idx);

	l_rule_ops->copy_hrt_bound_table(&layering_info,
		0, g_emi_bound_table, dev);

	/* 1.Pre-distribution */
	l_rule_info->dal_enable = mtk_drm_dal_enable();

	/* BWM + GPU Cache */
	/* check have force gpu layer or not */
	have_force_gpu_layer = 0;
	if (get_layering_opt(LYE_OPT_GPU_CACHE))
		check_is_force_gpu(dev, &layering_info);

	if (l_rule_ops->rollback_to_gpu_by_hw_limitation)
		ret = l_rule_ops->rollback_to_gpu_by_hw_limitation(
			dev, &layering_info);

	/* Check can do MML or not */
	if (disp_idx == 0 && layering_info.layer_num[HRT_PRIMARY] > 0) {
		check_is_mml_layer(disp_idx, &layering_info,
			dev, _layering_rule_get_hrt_idx(disp_idx));
		check_gles_change(&dbg_gles, __LINE__, false);
	}

	/* Check and choose the Resize Scenario */
	if (get_layering_opt(LYE_OPT_RPO)) {
		resizing_rule(dev, &layering_info);
	} else {
		mtk_rollback_all_resize_layer_to_GPU(&layering_info, HRT_PRIMARY);
		mtk_rollback_all_resize_layer_to_GPU(&layering_info, HRT_SECONDARY);
		mtk_rollback_all_resize_layer_to_GPU(&layering_info, HRT_THIRD);
		mtk_rollback_all_resize_layer_to_GPU(&layering_info, HRT_FOURTH);
	}
	check_gles_change(&dbg_gles, __LINE__, false);

	/* fbdc_rule should be after resizing_rule
	 * for optimizing secondary display BW
	 */
	if (l_rule_ops->fbdc_rule)
		l_rule_ops->fbdc_rule(dev, &layering_info);

	/* Add for FBDC */
	if (l_rule_ops->fbdc_pre_calculate)
		l_rule_ops->fbdc_pre_calculate(&layering_info);

	/* Layer Grouping */
	if (l_rule_ops->fbdc_adjust_layout)
		l_rule_ops->fbdc_adjust_layout(&layering_info,
					       ADJUST_LAYOUT_EXT_GROUPING);

	ret = ext_layer_grouping(dev, &layering_info);

	if (l_rule_ops->fbdc_restore_layout)
		l_rule_ops->fbdc_restore_layout(&layering_info,
						ADJUST_LAYOUT_EXT_GROUPING);

	/* GLES adjustment and ext layer checking */
	ret = filter_by_ovl_cnt(dev, &layering_info);
	check_gles_change(&dbg_gles, __LINE__, false);

	/*
	 * 2.Overlapping
	 * Calculate overlap number of available input layers.
	 * If the overlap number is out of bound, then decrease
	 * the number of available layers to overlap number.
	 */
	/* [PVRIC] change dst layout before calculate overlap */
	if (l_rule_ops->fbdc_adjust_layout)
		l_rule_ops->fbdc_adjust_layout(&layering_info,
					       ADJUST_LAYOUT_OVERLAP_CAL);

	overlap_num = calc_hrt_num(dev, &layering_info);
	layering_info.hrt_weight = overlap_num;
	DDPINFO("overlap_num %u of frame %u\n", layering_info.hrt_weight,
		layering_info.frame_idx[HRT_PRIMARY]);
	if ((disp_idx == 0) && get_layering_opt(LYE_OPT_OVL_BW_MONITOR) &&
		(sum_overlap_w_of_bwm != 0))
		DDPINFO("overlap_num of BW monitor:%u of frame %u\n", sum_overlap_w_of_bwm,
			layering_info.frame_idx[HRT_PRIMARY]);

	/* If GPU Cache will change gles layer head and tail, So should to re-grouping */
	if (disp_idx == 0 && get_layering_opt(LYE_OPT_GPU_CACHE) &&
		((dbg_gles.head != layering_info.gles_head[HRT_PRIMARY]) ||
		(dbg_gles.tail != layering_info.gles_tail[HRT_PRIMARY]))) {
		/* Again Layer Grouping */
		if (l_rule_ops->fbdc_adjust_layout)
			l_rule_ops->fbdc_adjust_layout(&layering_info,
					ADJUST_LAYOUT_EXT_GROUPING);

		ret = ext_layer_grouping(dev, &layering_info);

		if (l_rule_ops->fbdc_restore_layout)
			l_rule_ops->fbdc_restore_layout(&layering_info,
					ADJUST_LAYOUT_EXT_GROUPING);

		/* GLES adjustment and ext layer checking */
		ret = filter_by_ovl_cnt(dev, &layering_info);
		check_gles_change(&dbg_gles, __LINE__, false);

		if (l_rule_ops->fbdc_adjust_layout)
			l_rule_ops->fbdc_adjust_layout(&layering_info,
					ADJUST_LAYOUT_OVERLAP_CAL);

		if (l_rule_ops->fbdc_restore_layout)
			l_rule_ops->fbdc_restore_layout(&layering_info,
					ADJUST_LAYOUT_OVERLAP_CAL);
	}

	/*
	 * 3.Dispatching
	 * Fill layer id for each input layers.
	 * All the gles layers set as same layer id.
	 */
	// SPHRT: TODO: support other display enter idle through GPU
	if (disp_idx == 0 && l_rule_ops->rollback_all_to_GPU_for_idle != NULL &&
			l_rule_ops->rollback_all_to_GPU_for_idle(dev)) {
		int i;
		struct drm_mtk_layer_config *c;
		unsigned int weight = 0;

		roll_gpu_for_idle = 1;
		rollback_all_to_GPU(&layering_info, HRT_PRIMARY);
		/* TODO: assume resize layer would be 2 */
		for (i = 0 ; i < layering_info.layer_num[HRT_PRIMARY] ; i++) {
			c = &layering_info.input_config[HRT_PRIMARY][i];
			c->layer_caps &= ~MTK_DISP_RSZ_LAYER;
		}
		scn_decision_flag |= SCN_IDLE;
		layering_info.hrt_num = HRT_LEVEL_LEVEL0;
		if (priv && priv->data->need_emi_eff) {
			weight = (400 * 10000) / default_emi_eff;
			layering_info.hrt_weight = weight;
			if (l_rule_info->dal_enable)
				layering_info.hrt_weight += (200 * 10000) / default_emi_eff;
			if (get_layering_opt(LYE_OPT_OVL_BW_MONITOR)) {
				sum_overlap_w_of_bwm = weight;
				if (l_rule_info->dal_enable)
					sum_overlap_w_of_bwm += (200 * 10000) / default_emi_eff;
			}
		} else {
			weight = 400;
			layering_info.hrt_weight = weight;
			if (l_rule_info->dal_enable)
				layering_info.hrt_weight += 200;
			if (get_layering_opt(LYE_OPT_OVL_BW_MONITOR)) {
				sum_overlap_w_of_bwm = weight;
				if (l_rule_info->dal_enable)
					sum_overlap_w_of_bwm += 200;
			}
		}
		if (priv && mtk_drm_helper_get_opt(priv->helper_opt,
			MTK_DRM_OPT_LAYERING_RULE_BY_LARB)) {
			if (layering_info.gles_head[disp_idx] >= 0) {
				unsigned int gles_id;
				struct drm_mtk_layer_config *gles_layer_info = NULL;

				gles_id = layering_info.gles_head[disp_idx];
				gles_layer_info =
					&layering_info.input_config[disp_idx][gles_id];
				gles_layer_info->layer_hrt_weight = weight;
				DDPDBG_HBL("%s,%d,crtc:%u,update gles l:%u,w:%u,sum_overlap_w_of_bwm:%u\n",
					 __func__, __LINE__, layering_info.disp_idx, gles_id,
					 gles_layer_info->layer_hrt_weight,
					 sum_overlap_w_of_bwm);
			}
		}
	}
	check_gles_change(&dbg_gles, __LINE__, true);

	lyeblob_ids = kzalloc(sizeof(struct mtk_drm_lyeblob_ids), GFP_KERNEL);

	dispatch_gles_range(&layering_info, dev);
	check_gles_change(&dbg_gles, __LINE__, false);

	clear_layer(&layering_info, &scn_decision_flag, dev);
	check_gles_change(&dbg_gles, __LINE__, true);

	check_layering_result(&layering_info);
	/* Double Check GLES adjustment and ext layer checking */
	ret = filter_by_ovl_cnt(dev, &layering_info);
	ret = dispatch_ovl_id(&layering_info, lyeblob_ids, dev, &lye_state);

	layering_info.hrt_idx = _layering_rule_get_hrt_idx(disp_idx);
	HRT_SET_AEE_FLAG(layering_info.hrt_num, l_rule_info->dal_enable);
	HRT_SET_WROT_SRAM_FLAG(layering_info.hrt_num, l_rule_info->wrot_sram);
	dump_disp_info(&layering_info, DISP_DEBUG_LEVEL_INFO);

	dump_disp_trace(&layering_info);

	/* Remove MMP */
	/* mmprofile_log_ex(ddp_mmp_get_events()->hrt, MMPROFILE_FLAG_PULSE,
	 *		 layering_info.hrt_num,
	 *		 (layering_info.gles_head[0] << 24) |
	 *		 (layering_info.gles_tail[0] << 16) |
	 *		 (layering_info.layer_num[0] << 8) |
	 *		 layering_info.layer_num[1]);
	 */
	crtc_num = get_crtc_num(disp_info_user, &crtc_mask);
	lye_add_blob_ids(&layering_info, lyeblob_ids, dev, crtc_num, crtc_mask, &lye_state);

	for (disp_idx = 0; disp_idx < HRT_DISP_TYPE_NUM; disp_idx++)
		DRM_MMP_MARK(layering, layering_info.hrt_num,
				(layering_info.gles_head[disp_idx] << 24) |
				(layering_info.gles_tail[disp_idx] << 16) |
				(layering_info.layer_num[disp_idx] << 8) |
				disp_idx);

	ret = copy_layer_info_to_user(dev, disp_info_user, debug_mode);

	DRM_MMP_EVENT_END(layering, (unsigned long)disp_info_user,
			(unsigned long)dev);

	return ret;
}

/**** UT Program ****/
#ifdef HRT_UT_DEBUG
static void debug_set_layer_data(struct drm_mtk_layering_info *disp_info,
				 int disp_id, int data_type, int value)
{
	static int layer_id = -1;
	struct drm_mtk_layer_config *layer_info = NULL;

	if (data_type != HRT_LAYER_DATA_ID && layer_id == -1)
		return;

	layer_info = &disp_info->input_config[disp_id][layer_id];
	switch (data_type) {
	case HRT_LAYER_DATA_ID:
		layer_id = value;
		break;
	case HRT_LAYER_DATA_SRC_FMT:
		layer_info->src_fmt = value;
		break;
	case HRT_LAYER_DATA_DST_OFFSET_X:
		layer_info->dst_offset_x = value;
		break;
	case HRT_LAYER_DATA_DST_OFFSET_Y:
		layer_info->dst_offset_y = value;
		break;
	case HRT_LAYER_DATA_DST_WIDTH:
		layer_info->dst_width = value;
		break;
	case HRT_LAYER_DATA_DST_HEIGHT:
		layer_info->dst_height = value;
		break;
	case HRT_LAYER_DATA_SRC_WIDTH:
		layer_info->src_width = value;
		break;
	case HRT_LAYER_DATA_SRC_HEIGHT:
		layer_info->src_height = value;
		break;
	case HRT_LAYER_DATA_SRC_OFFSET_X:
		layer_info->src_offset_x = value;
		break;
	case HRT_LAYER_DATA_SRC_OFFSET_Y:
		layer_info->src_offset_y = value;
		break;
	case HRT_LAYER_DATA_COMPRESS:
		layer_info->compress = value;
		break;
	case HRT_LAYER_DATA_CAPS:
		layer_info->layer_caps = value;
		break;
	default:
		break;
	}
}

static char *parse_hrt_data_value(char *start, long *value)
{
	char *tok_start = NULL, *tok_end = NULL;
	int ret;

	tok_start = strchr(start + 1, ']');
	tok_end = strchr(tok_start + 1, '[');
	if (tok_end)
		*tok_end = 0;
	ret = kstrtol(tok_start + 1, 10, value);
	if (ret)
		DDPINFO("Parsing error gles_num:%d, p:%s, ret:%d\n",
			(int)*value, tok_start + 1, ret);

	return tok_end;
}

static void print_drm_mtk_layer_config(struct drm_mtk_layer_config *c)
{
	DDPMSG("L%u/(%u,%u,%u,%u)/(%u,%u,%u,%u)/cpr%d/e%d/cap0x%x/cl%x\n",
	       c->ovl_id, c->src_offset_x, c->src_offset_y, c->src_width,
	       c->src_height, c->dst_offset_x, c->dst_offset_y, c->dst_width,
	       c->dst_height, c->compress, c->ext_sel_layer, c->layer_caps,
	       c->clip);
}

static void print_hrt_result(struct drm_mtk_layering_info *disp_info)
{
	unsigned int i = 0, j = 0;

	for (i = 0; i < HRT_DISP_TYPE_NUM; i++) {
		DDPMSG("### DISP%d ###\n", i);
		DDPMSG("[head]%d[tail]%d\n", disp_info->gles_head[i],
		       disp_info->gles_tail[i]);
		DDPMSG("[hrt_num]%d\n", disp_info->hrt_num);
		for (j = 0; j < disp_info->layer_num[i]; j++)
			print_drm_mtk_layer_config(
				&(disp_info->input_config[i][j]));
	}
}

static int load_hrt_test_data(struct drm_mtk_layering_info *disp_info,
			      struct drm_device *dev)
{
	char filename[] = "/sdcard/hrt_data.txt";
	char line_buf[512];
	char *tok;
	struct file *filp;
	mm_segment_t oldfs;
	int ret, pos, i;
	long disp_id, test_case;
	bool is_end = false, is_test_pass = false;
	struct drm_mtk_layer_config *input_config;

	pos = 0;
	test_case = -1;
	oldfs = get_fs();
	set_fs(KERNEL_DS);
	filp = filp_open(filename, O_RDONLY, 0777);
	if (IS_ERR(filp)) {
		DDPINFO("File open error:%s\n", filename);
		return -1;
	}

	if (!filp->f_op) {
		DDPINFO("File Operation Method Error!!\n");
		return -1;
	}

	while (1) {
		ret = filp->f_op->llseek(filp, filp->f_pos, pos);
		memset(line_buf, 0x0, sizeof(line_buf));
		ret = filp->f_op->read(filp, line_buf, sizeof(line_buf),
				       &filp->f_pos);
		tok = strchr(line_buf, '\n');
		if (tok != NULL)
			*tok = '\0';
		else
			is_end = true;

		pos += strlen(line_buf) + 1;
		filp->f_pos = pos;

		if (strncmp(line_buf, "#", 1) == 0) {
			continue;
		} else if (strncmp(line_buf, "[layer_num]", 11) == 0) {
			unsigned long layer_num = 0;
			unsigned long layer_size = 0;

			tok = parse_hrt_data_value(line_buf, &layer_num);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);

			if (disp_id >= HRT_DISP_TYPE_NUM)
				goto end;

			if (layer_num != 0) {
				layer_size =
					sizeof(struct drm_mtk_layer_config) *
					layer_num;
				disp_info->input_config[disp_id] =
					vzalloc(layer_size);
			}
			disp_info->layer_num[disp_id] = layer_num;

			if (disp_info->input_config[disp_id] == NULL)
				return 0;
		} else if (strncmp(line_buf, "[set_layer]", 11) == 0) {
			unsigned long tmp_info;

			tok = strchr(line_buf, ']');
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);
			for (i = 0; i < HRT_LAYER_DATA_NUM; i++) {
				tok = parse_hrt_data_value(tok, &tmp_info);
				debug_set_layer_data(disp_info, disp_id, i,
						     tmp_info);
			}
		} else if (strncmp(line_buf, "[test_start]", 12) == 0) {
			tok = parse_hrt_data_value(line_buf, &test_case);
			layering_rule_start(disp_info, 1, dev);
			is_test_pass = true;
		} else if (strncmp(line_buf, "[test_end]", 10) == 0) {
			vfree(disp_info->input_config[0]);
			vfree(disp_info->input_config[1]);
			memset(disp_info, 0x0,
			       sizeof(struct drm_mtk_layering_info));
			is_end = true;
		} else if (strncmp(line_buf, "[print_out_test_result]", 23) ==
			   0) {
			DDPINFO("Test case %d is %s\n", (int)test_case,
				is_test_pass ? "Pass" : "Fail");
		} else if (strncmp(line_buf, "[layer_result]", 14) == 0) {
			long layer_result = 0, layer_id;

			tok = strchr(line_buf, ']');
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &layer_id);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &layer_result);
			input_config =
				&disp_info->input_config[disp_id][layer_id];
			if (layer_result != input_config->ovl_id) {
				DDPINFO("case:%d,ovl_id incorrect,%d/%d\n",
					(int)test_case, input_config->ovl_id,
					(int)layer_result);
				is_test_pass = false;
			}
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &layer_result);
			if (layer_result != input_config->ext_sel_layer) {
				DDPINFO("case:%d,ext_sel_layer wrong,%d/%d\n",
					(int)test_case,
					input_config->ext_sel_layer,
					(int)layer_result);
				is_test_pass = false;
			}
		} else if (strncmp(line_buf, "[gles_result]", 13) == 0) {
			long gles_num = 0;

			tok = strchr(line_buf, ']');
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &gles_num);
			if (gles_num != disp_info->gles_head[disp_id]) {
				DDPINFO("case:%d,gles head err,%d/%d\n",
					(int)test_case,
					disp_info->gles_head[disp_id],
					(int)gles_num);
				is_test_pass = false;
			}

			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &gles_num);
			if (gles_num != disp_info->gles_tail[disp_id]) {
				DDPINFO("case:%d,gles tail err,%d/%d\n",
					(int)test_case,
					disp_info->gles_tail[disp_id],
					(int)gles_num);
				is_test_pass = false;
			}
		} else if (strncmp(line_buf, "[hrt_result]", 12) == 0) {
			unsigned long hrt_num = 0;
			int path_scen;

			tok = parse_hrt_data_value(line_buf, &hrt_num);
			if (hrt_num != HRT_GET_DVFS_LEVEL(disp_info->hrt_num))
				DDPINFO("case:%d,hrt num err,%d/%d\n",
					(int)test_case,
					HRT_GET_DVFS_LEVEL(disp_info->hrt_num),
					(int)hrt_num);

			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &hrt_num);
			path_scen = HRT_GET_PATH_SCENARIO(disp_info->hrt_num) &
				    0x1F;
			if (hrt_num != path_scen) {
				DDPINFO("case:%d,hrt path err,%d/%d\n",
					(int)test_case, path_scen,
					(int)hrt_num);
				is_test_pass = false;
			}

			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &hrt_num);
			if (hrt_num !=
			    HRT_GET_SCALE_SCENARIO(disp_info->hrt_num)) {
				DDPINFO("case:%d, hrt scale err,%d/%d\n",
					(int)test_case,
					HRT_GET_SCALE_SCENARIO(
						disp_info->hrt_num),
					(int)hrt_num);
				is_test_pass = false;
			}

		} else if (strncmp(line_buf, "[change_layer_num]", 18) == 0) {
			unsigned long layer_num = 0;

			tok = parse_hrt_data_value(line_buf, &layer_num);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);
			disp_info->layer_num[disp_id] = layer_num;
		} else if (!strncmp(line_buf, "[force_dual_pipe_off]", 21)) {
			unsigned long force_off = 0;

			tok = parse_hrt_data_value(line_buf, &force_off);
			set_hrt_state(DISP_HRT_FORCE_DUAL_OFF, force_off);
		} else if (!strncmp(line_buf, "[resolution_level]", 18)) {
			unsigned long resolution_level = 0;

			tok = parse_hrt_data_value(line_buf, &resolution_level);
			debug_resolution_level = resolution_level;
		} else if (!strncmp(line_buf, "[set_gles]", 10)) {
			long gles_num = 0;

			tok = strchr(line_buf, ']');
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &gles_num);
			disp_info->gles_head[disp_id] = gles_num;

			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &gles_num);
			disp_info->gles_tail[disp_id] = gles_num;
		} else if (!strncmp(line_buf, "[disp_mode]", 11)) {
			unsigned long disp_mode = 0;

			tok = parse_hrt_data_value(line_buf, &disp_mode);
			if (!tok)
				goto end;
			tok = parse_hrt_data_value(tok, &disp_id);
			disp_info->disp_mode[disp_id] = disp_mode;
		} else if (!strncmp(line_buf, "[print_out_hrt_result]", 22))
			print_hrt_result(disp_info);

		if (is_end)
			break;
	}

end:
	filp_close(filp, NULL);
	set_fs(oldfs);
	DDPINFO("end set_fs\n");
	return 0;
}

static int gen_hrt_pattern(struct drm_device *dev)
{
#ifdef HRT_UT_DEBUG
	struct drm_mtk_layering_info disp_info;
	struct drm_mtk_layer_config *layer_info;
	int i;

	memset(&disp_info, 0x0, sizeof(struct drm_mtk_layering_info));
	disp_info.gles_head[0] = -1;
	disp_info.gles_head[1] = -1;
	disp_info.gles_tail[0] = -1;
	disp_info.gles_tail[1] = -1;
	if (!load_hrt_test_data(&disp_info))
		return 0;

	/* Primary Display */
	disp_info.disp_mode[0] = DRM_DISP_SESSION_DIRECT_LINK_MODE;
	disp_info.layer_num[0] = 5;
	disp_info.gles_head[0] = 3;
	disp_info.gles_tail[0] = 5;
	disp_info.input_config[0] =
		vzalloc(sizeof(struct drm_mtk_layer_config) * 5);
	layer_info = disp_info.input_config[0];
	for (i = 0; i < disp_info.layer_num[0]; i++)
		layer_info[i].src_fmt = DRM_FORMAT_ARGB8888;

	layer_info = disp_info.input_config[0];
	layer_info[0].dst_offset_x = 0;
	layer_info[0].dst_offset_y = 0;
	layer_info[0].dst_width = 1080;
	layer_info[0].dst_height = 1920;
	layer_info[1].dst_offset_x = 0;
	layer_info[1].dst_offset_y = 0;
	layer_info[1].dst_width = 1080;
	layer_info[1].dst_height = 1920;
	layer_info[2].dst_offset_x = 269;
	layer_info[2].dst_offset_y = 72;
	layer_info[2].dst_width = 657;
	layer_info[2].dst_height = 612;
	layer_info[3].dst_offset_x = 0;
	layer_info[3].dst_offset_y = 0;
	layer_info[3].dst_width = 1080;
	layer_info[3].dst_height = 72;
	layer_info[4].dst_offset_x = 1079;
	layer_info[4].dst_offset_y = 72;
	layer_info[4].dst_width = 1;
	layer_info[4].dst_height = 1704;

	/* Secondary Display */
	disp_info.disp_mode[1] = DRM_DISP_SESSION_DIRECT_LINK_MODE;
	disp_info.layer_num[1] = 0;
	disp_info.gles_head[1] = -1;
	disp_info.gles_tail[1] = -1;

	DDPMSG("free test pattern\n");
	vfree(disp_info.input_config[0]);
	msleep(50);
#endif
	return 0;
}
#endif

/**** UT Program end ****/

int mtk_layering_rule_ioctl(struct drm_device *dev, void *data,
			    struct drm_file *file_priv)
{
	struct drm_mtk_layering_info *disp_info_user = data;
	int ret;

	ret = layering_rule_start(disp_info_user, 0, dev);
	if (ret < 0)
		DDPPR_ERR("layering_rule_start error:%d\n", ret);

	return 0;
}
