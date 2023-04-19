/* SPDX-License-Identifier: GPL-2.0-only
 *
 * Copyright (c) 2022 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Headef file for exynos_drm_dsc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __EXYNOS_DRM_DSC_H__
#define __EXYNOS_DRM_DSC_H__

#include <linux/printk.h>
#include <linux/bits.h>
#include <linux/types.h>
#include <drm/drm_crtc.h>
#include <drm/drm_dsc.h>

#define DSC_NUM_PPS 128
#define EXYNOS_DSC_PPS_NUM 88
#define EXYNOS_DSC_NIBBLE_H(v) ((v >> 4) & 0xf) /* Most */
#define EXYNOS_DSC_NIBBLE_L(v) (v & 0xf)
#define EXYNOS_DSC_BIT_MASK(v, s) (v & BIT(s))
#define EXYNOS_DSC_BITMASK_VAL(v, s) (EXYNOS_DSC_BIT_MASK(v, s) >> s)
#define EXYNOS_DSC_BIG_ENDIAN(v1, v2) ((v1 << DSC_PPS_MSB_SHIFT) | v2)
#define EXYNOS_DSC_FRACTIONAL_BIT(v, f) (v >> f)
#define EXYNOS_DSC_TO_MSB(v) ((v >> DSC_PPS_MSB_SHIFT) & DSC_PPS_LSB_MASK)
#define EXYNOS_DSC_TO_LSB(v) (v & DSC_PPS_LSB_MASK)

struct _rc_range_parameter {
	u16 range_min_qp : 5;
	u16 range_max_qp : 5;
	u16 range_bpg_offset : 6;
};

struct exynos_drm_pps {
	/**
	 * @version:
	 * PPS0[3:0] - dsc_version_minor: Contains Minor version of DSC
	 * PPS0[7:4] - dsc_version_major: Contains major version of DSC
	 */
	u8 dsc_version_major;
	u8 dsc_version_minor;

	/**
	 * @identifier:
	 * PPS1[7:0] - Application specific identifier that can be
	 * used to differentiate between different PPS tables.
	 */
	u8 pps_identifier;
	/**
	 * @pps2_reserved:
	 * PPS2[7:0]- RESERVED Byte
	 */
	//u8 pps2_reserved;

	/**
	 * @pps_3:
	 * PPS3[3:0] - linebuf_depth: Contains linebuffer bit depth used to
	 * generate the bitstream. (0x0 - 16 bits for DSC 1.2, 0x8 - 8 bits,
	 * 0xA - 10 bits, 0xB - 11 bits, 0xC - 12 bits, 0xD - 13 bits,
	 * 0xE - 14 bits for DSC1.2, 0xF - 14 bits for DSC 1.2.
	 * PPS3[7:4] - bits_per_component: Bits per component for the original
	 * pixels of the encoded picture.
	 * 0x0 = 16bpc (allowed only when dsc_version_minor = 0x2)
	 * 0x8 = 8bpc, 0xA = 10bpc, 0xC = 12bpc, 0xE = 14bpc (also
	 * allowed only when dsc_minor_version = 0x2)
	 */
	u8 linebuf_depth;
	u8 bits_per_component; /* bits_per_component */

	/**
	 * @pps_4:
	 * PPS4[1:0] -These are the most significant 2 bits of
	 * compressed BPP bits_per_pixel[9:0] syntax element.
	 * PPS4[2] - vbr_enable: 0 = VBR disabled, 1 = VBR enabled
	 * PPS4[3] - simple_422: Indicates if decoder drops samples to
	 * reconstruct the 4:2:2 picture.
	 * PPS4[4] - Convert_rgb: Indicates if DSC color space conversion is
	 * active.
	 * PPS4[5] - blobk_pred_enable: Indicates if BP is used to code any
	 * groups in picture
	 * PPS4[7:6] - Reseved bits
	 */
	u8 vbr_enable;
	u8 simple_422;
	u8 convert_rgb;
	u8 block_pred_enable;
	/**
	 * @bits_per_pixel_low:
	 * PPS5[7:0] - This indicates the lower significant 8 bits of
	 * the compressed BPP bits_per_pixel[9:0] element.
	 */
	u16 bits_per_pixel;
	/**
	 * @pic_height:
	 * PPS6[7:0], PPS7[7:0] -pic_height: Specifies the number of pixel rows
	 * within the raster.
	 */
	u16 pic_height;
	/**
	 * @pic_width:
	 * PPS8[7:0], PPS9[7:0] - pic_width: Number of pixel columns within
	 * the raster.
	 */
	u16 pic_width;
	/**
	 * @slice_height:
	 * PPS10[7:0], PPS11[7:0] - Slice height in units of pixels.
	 */
	u16 slice_height;
	/**
	 * @slice_width:
	 * PPS12[7:0], PPS13[7:0] - Slice width in terms of pixels.
	 */
	u16 slice_width;
	/**
	 * @chunk_size:
	 * PPS14[7:0], PPS15[7:0] - Size in units of bytes of the chunks
	 * that are used for slice multiplexing.
	 */
	u16 chunk_size;
	/**
	 * @initial_xmit_delay_high:
	 * PPS16[1:0] - Most Significant two bits of initial transmission delay.
	 * It specifies the number of pixel times that the encoder waits before
	 * transmitting data from its rate buffer.
	 * PPS16[7:2] - Reserved
	 */

	/**
	 * @initial_xmit_delay_low:
	 * PPS17[7:0] - Least significant 8 bits of initial transmission delay.
	 */
	u16 initial_xmit_delay;
	/**
	 * @initial_dec_delay:
	 *
	 * PPS18[7:0], PPS19[7:0] - Initial decoding delay which is the number
	 * of pixel times that the decoder accumulates data in its rate buffer
	 * before starting to decode and output pixels.
	 */
	u16 initial_dec_delay;
	/**
	 * @pps20_reserved:
	 *
	 * PPS20[7:0] - Reserved
	 */
	//u16 pps20_reserved;
	/**
	 * @initial_scale_value:
	 * PPS21[5:0] - Initial rcXformScale factor used at beginning
	 * of a slice.
	 * PPS21[7:6] - Reserved
	 */
	u8 initial_scale_value;
	/**
	 * @scale_increment_interval:
	 * PPS22[7:0], PPS23[7:0] - Number of group times between incrementing
	 * the rcXformScale factor at end of a slice.
	 */
	u16 scale_increment_interval;
	/**
	 * @scale_decrement_interval_high:
	 * PPS24[3:0] - Higher 4 bits indicating number of group times between
	 * decrementing the rcXformScale factor at beginning of a slice.
	 * PPS24[7:4] - Reserved
	 */
	/**
	 * @scale_decrement_interval_low:
	 * PPS25[7:0] - Lower 8 bits of scale decrement interval
	 */
	u16 scale_decrement_interval;
	/**
	 * @pps26_reserved:
	 * PPS26[7:0]
	 */
	//u16 pps26_reserved;
	/**
	 * @first_line_bpg_offset:
	 * PPS27[4:0] - Number of additional bits that are allocated
	 * for each group on first line of a slice.
	 * PPS27[7:5] - Reserved
	 */
	u8 first_line_bpg_offset;
	/**
	 * @nfl_bpg_offset:
	 * PPS28[7:0], PPS29[7:0] - Number of bits including frac bits
	 * deallocated for each group for groups after the first line of slice.
	 */
	u16 nfl_bpg_offset;
	/**
	 * @slice_bpg_offset:
	 * PPS30, PPS31[7:0] - Number of bits that are deallocated for each
	 * group to enforce the slice constraint.
	 */
	u16 slice_bpg_offset;
	/**
	 * @initial_offset:
	 * PPS32,33[7:0] - Initial value for rcXformOffset
	 */
	u16 initial_offset;
	/**
	 * @final_offset:
	 * PPS34,35[7:0] - Maximum end-of-slice value for rcXformOffset
	 */
	u16 final_offset;
	/**
	 * @flatness_min_qp:
	 * PPS36[4:0] - Minimum QP at which flatness is signaled and
	 * flatness QP adjustment is made.
	 * PPS36[7:5] - Reserved
	 */
	u8 flatness_min_qp;
	/**
	 * @flatness_max_qp:
	 * PPS37[4:0] - Max QP at which flatness is signalled and
	 * the flatness adjustment is made.
	 * PPS37[7:5] - Reserved
	 */
	u8 flatness_max_qp;
	/**
	 * @rc_model_size:
	 * PPS38,39[7:0] - Number of bits within RC Model.
	 */
	u16 rc_model_size;
	/**
	 * @rc_edge_factor:
	 * PPS40[3:0] - Ratio of current activity vs, previous
	 * activity to determine presence of edge.
	 * PPS40[7:4] - Reserved
	 */
	u8 rc_edge_factor;
	/**
	 * @rc_quant_incr_limit0:
	 * PPS41[4:0] - QP threshold used in short term RC
	 * PPS41[7:5] - Reserved
	 */
	u8 rc_quant_incr_limit0;
	/**
	 * @rc_quant_incr_limit1:
	 * PPS42[4:0] - QP threshold used in short term RC
	 * PPS42[7:5] - Reserved
	 */
	u8 rc_quant_incr_limit1;
	/**
	 * @rc_tgt_offset:
	 * PPS43[3:0] - Lower end of the variability range around the target
	 * bits per group that is allowed by short term RC.
	 * PPS43[7:4]- Upper end of the variability range around the target
	 * bits per group that i allowed by short term rc.
	 */
	u8 rc_tgt_offset_hi;
	u8 rc_tgt_offset_lo;
	/**
	 * @rc_buf_thresh:
	 * PPS44[7:0] - PPS57[7:0] - Specifies the thresholds in RC model for
	 * the 15 ranges defined by 14 thresholds.
	 */
	u16 rc_buf_thresh[DSC_NUM_BUF_RANGES - 1];
	/**
	 * @rc_range_parameters:
	 * PPS58[7:0] - PPS87[7:0]
	 * Parameters that correspond to each of the 15 ranges.
	 */
	struct _rc_range_parameter rc_range_parameters[DSC_NUM_BUF_RANGES];
	/**
	 * @native_422_420:
	 * PPS88[0] - 0 = Native 4:2:2 not used
	 * 1 = Native 4:2:2 used
	 * PPS88[1] - 0 = Native 4:2:0 not use
	 * 1 = Native 4:2:0 used
	 * PPS88[7:2] - Reserved 6 bits
	 */
	u8 native_422;
	u8 native_420;
	/**
	 * @second_line_bpg_offset:
	 * PPS89[4:0] - Additional bits/group budget for the
	 * second line of a slice in Native 4:2:0 mode.
	 * Set to 0 if DSC minor version is 1 or native420 is 0.
	 * PPS89[7:5] - Reserved
	 */
	//u8 second_line_bpg_offset;
	/**
	 * @nsl_bpg_offset:
	 * PPS90[7:0], PPS91[7:0] - Number of bits that are deallocated
	 * for each group that is not in the second line of a slice.
	 */
	//__be16 nsl_bpg_offset;
	/**
	 * @second_line_offset_adj:
	 * PPS92[7:0], PPS93[7:0] - Used as offset adjustment for the second
	 * line in Native 4:2:0 mode.
	 */
	//__be16 second_line_offset_adj;
	/**
	 * @pps_long_94_reserved:
	 * PPS 94, 95, 96, 97 - Reserved
	 */
	//u32 pps_long_94_reserved;
	/**
	 * @pps_long_98_reserved:
	 * PPS 98, 99, 100, 101 - Reserved
	 */
	//u32 pps_long_98_reserved;
	/**
	 * @pps_long_102_reserved:
	 * PPS 102, 103, 104, 105 - Reserved
	 */
	//u32 pps_long_102_reserved;
	/**
	 * @pps_long_106_reserved:
	 * PPS 106, 107, 108, 109 - reserved
	 */
	//u32 pps_long_106_reserved;
	/**
	 * @pps_long_110_reserved:
	 * PPS 110, 111, 112, 113 - reserved
	 */
	//u32 pps_long_110_reserved;
	/**
	 * @pps_long_114_reserved:
	 * PPS 114 - 117 - reserved
	 */
	//u32 pps_long_114_reserved;
	/**
	 * @pps_long_118_reserved:
	 * PPS 118 - 121 - reserved
	 */
	//u32 pps_long_118_reserved;
	/**
	 * @pps_long_122_reserved:
	 * PPS 122- 125 - reserved
	 */
	//u32 pps_long_122_reserved;
	/**
	 * @pps_short_126_reserved:
	 * PPS 126, 127 - reserved
	 */
	//__be16 pps_short_126_reserved;

};

u8 exynos_drm_dsc_get_bpc(struct exynos_drm_pps* pps);
u8 exynos_drm_dsc_get_bpp(struct exynos_drm_pps* pps);
u8 exynos_drm_dsc_get_initial_scale_value(struct exynos_drm_pps* pps);
u8 exynos_drm_dsc_get_nfl_bpg_offset(struct exynos_drm_pps* pps);
u8 exynos_drm_dsc_get_slice_bpg_offset(struct exynos_drm_pps* pps);
u8 exynos_drm_dsc_get_comp_config(struct exynos_drm_pps* pps);
u16 exynos_drm_dsc_get_rc_range_params(struct exynos_drm_pps* pps, int idx);
int convert_pps_to_exynos_pps(struct exynos_drm_pps *pps, const u8 *table);
void exynos_drm_dsc_print_pps_info(struct exynos_drm_pps *pps, const u8* table);

#endif /* __EXYNOS_DRM_DSC_H__ */
