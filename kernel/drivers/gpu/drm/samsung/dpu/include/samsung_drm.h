/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef __SAMSUNG_DRM_H__
#define __SAMSUNG_DRM_H__

#if defined(__linux__)
#include <linux/types.h>
#endif

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct dpu_u32_range {
	__u32 min;
	__u32 max;
	__u32 align;
} __u32_range;

typedef struct dpu_str_u32 {
	__s8 str[32];
	__u32 val;
} __str_u32;

#define DPP_SIZE_ARG(s) s.min, s.max, s.align
struct dpp_restrict {
	__s32 id;
	__u32 port;
	__u32 dpuf_id;
	__u64 attr;

	__u32_range src_f_w;
	__u32_range src_f_h;
	__u32_range src_w;
	__u32_range src_h;
	__u32 src_x_align;
	__u32 src_y_align;

	__u32_range dst_f_w;
	__u32_range dst_f_h;
	__u32_range dst_w;
	__u32_range dst_h;
	__u32 dst_x_align;
	__u32 dst_y_align;

	__u32_range blk_w;
	__u32_range blk_h;
	__u32 blk_x_align;
	__u32 blk_y_align;

	__u32 src_w_rot_max;
	__u32 src_h_rot_max;

	__u32 scale_down;
	__u32 scale_up;

	__u32_range src_rgb_comp_f_w;
	__u32_range src_rgb_comp_f_h;
	__u32_range src_rgb_comp_w;
	__u32_range src_rgb_comp_h;
};

struct decon_restrict {
	__u32 id;
	__u32 disp_max_clock;
	__u32 disp_margin_pct;
	__u32 disp_factor_pct;
	__u32 ppc;
};

enum dpu_res_key {
	DPU_RES_ID,
	DPU_RES_PORT,
	DPU_RES_DPUF_ID,
	DPU_RES_ATTR,

	DPU_RES_SRC_F_W,
	DPU_RES_SRC_F_H,
	DPU_RES_SRC_W,
	DPU_RES_SRC_H,
	DPU_RES_SRC_X_ALIGN,
	DPU_RES_SRC_Y_ALIGN,
	DPU_RES_SRC_W_ROT_MAX,
	DPU_RES_SRC_H_ROT_MAX,

	DPU_RES_SRC_RGB_COMP_F_W,
	DPU_RES_SRC_RGB_COMP_W,
	DPU_RES_SRC_RGB_COMP_F_H,
	DPU_RES_SRC_RGB_COMP_H,

	DPU_RES_SRC_YUV_COMP_F_W,
	DPU_RES_SRC_YUV_COMP_W,
	DPU_RES_SRC_YUV_COMP_F_H,
	DPU_RES_SRC_YUV_COMP_H,

	DPU_RES_DST_F_W,
	DPU_RES_DST_F_H,
	DPU_RES_DST_W,
	DPU_RES_DST_H,
	DPU_RES_DST_X_ALIGN,
	DPU_RES_DST_Y_ALIGN,

	DPU_RES_BLK_W,
	DPU_RES_BLK_H,
	DPU_RES_BLK_X_ALIGN,
	DPU_RES_BLK_Y_ALIGN,

	DPU_RES_SCALE_DOWN,
	DPU_RES_SCALE_UP,

	DPU_RES_DISP_CLK_MAX,
	DPU_RES_DISP_CLK_MARGIN_PCT,
	DPU_RES_DISP_CLK_FACTOR_PCT,
	DPU_RES_DISP_CLK_PPC,

	DPU_RES_MODE_PLANE_CNT,
};

enum dpu_res_type {
	DPU_RES_TYPE_S32,
	DPU_RES_TYPE_U32,
	DPU_RES_TYPE_S64,
	DPU_RES_TYPE_U64,
	DPU_RES_TYPE_U32_RANGE,
	DPU_RES_TYPE_U64_BITMASK,
	DPU_RES_TYPE_STR_U32,
};

#define DEFINE_DPU_RESTRICT(t, T)						\
struct dpu_res_##t {								\
	enum dpu_res_type	type;						\
	enum dpu_res_key	key;						\
	__##t			val;						\
};										\
static inline void dpu_res_add_##t(char **data, size_t *size,			\
				     enum dpu_res_key key, const void *val)	\
{										\
	struct dpu_res_##t *ptr = (struct dpu_res_##t *)(*data);		\
										\
	WARN(sizeof(*ptr) > *size, "failed to add restriction to blob data");	\
										\
	ptr->type = DPU_RES_TYPE_##T;						\
	ptr->key = (key);							\
	ptr->val = *(__##t *)(val);						\
	*data += sizeof(*ptr);							\
	*size -= sizeof(*ptr);							\
}
DEFINE_DPU_RESTRICT(u32, U32)
DEFINE_DPU_RESTRICT(u64, U64)
DEFINE_DPU_RESTRICT(u32_range, U32_RANGE)
DEFINE_DPU_RESTRICT(str_u32, STR_U32)

#if defined(__cplusplus)
}
#endif

#endif /* __SAMSUNG_DRM_H__ */
