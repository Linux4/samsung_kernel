/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *
 * Author: Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Device Tree binding constants for exynos display.
*/

#ifndef _DT_BINDINGS_EXYNOS_DISPLAY_H
#define _DT_BINDINGS_EXYNOS_DISPLAY_H

/*
 * Attention: Keep the DRM_FORMAT_* bit definitions in sync with
 * include/uapi/drm/drm_fourcc.h bit definitions.
 */
#define fourcc_code(a, b, c, d) ((a) | ((b) << 8) | \
				 ((c) << 16) | ((d) << 24))

#define DRM_FORMAT_ARGB8888	fourcc_code('A', 'R', '2', '4') /* [31:0] A:R:G:B 8:8:8:8 little endian */
#define DRM_FORMAT_ABGR8888	fourcc_code('A', 'B', '2', '4') /* [31:0] A:B:G:R 8:8:8:8 little endian */
#define DRM_FORMAT_RGBA8888	fourcc_code('R', 'A', '2', '4') /* [31:0] R:G:B:A 8:8:8:8 little endian */
#define DRM_FORMAT_BGRA8888	fourcc_code('B', 'A', '2', '4') /* [31:0] B:G:R:A 8:8:8:8 little endian */
#define DRM_FORMAT_XRGB8888	fourcc_code('X', 'R', '2', '4') /* [31:0] x:R:G:B 8:8:8:8 little endian */
#define DRM_FORMAT_XBGR8888	fourcc_code('X', 'B', '2', '4') /* [31:0] x:B:G:R 8:8:8:8 little endian */
#define DRM_FORMAT_RGBX8888	fourcc_code('R', 'X', '2', '4') /* [31:0] R:G:B:x 8:8:8:8 little endian */
#define DRM_FORMAT_BGRX8888	fourcc_code('B', 'X', '2', '4') /* [31:0] B:G:R:x 8:8:8:8 little endian */
#define DRM_FORMAT_RGB565	fourcc_code('R', 'G', '1', '6') /* [15:0] R:G:B 5:6:5 little endian */
#define DRM_FORMAT_BGR565	fourcc_code('B', 'G', '1', '6') /* [15:0] B:G:R 5:6:5 little endian */
#define DRM_FORMAT_ARGB2101010	fourcc_code('A', 'R', '3', '0') /* [31:0] A:R:G:B 2:10:10:10 little endian */
#define DRM_FORMAT_ABGR2101010	fourcc_code('A', 'B', '3', '0') /* [31:0] A:B:G:R 2:10:10:10 little endian */
#define DRM_FORMAT_RGBA1010102	fourcc_code('R', 'A', '3', '0') /* [31:0] R:G:B:A 10:10:10:2 little endian */
#define DRM_FORMAT_BGRA1010102	fourcc_code('B', 'A', '3', '0') /* [31:0] B:G:R:A 10:10:10:2 little endian */
#define DRM_FORMAT_ARGB16161616F fourcc_code('A', 'R', '4', 'H') /* [63:0] A:R:G:B 16:16:16:16 little endian */
#define DRM_FORMAT_ABGR16161616F fourcc_code('A', 'B', '4', 'H') /* [63:0] A:B:G:R 16:16:16:16 little endian */
#define DRM_FORMAT_NV12		fourcc_code('N', 'V', '1', '2') /* 2x2 subsampled Cr:Cb plane */
#define DRM_FORMAT_NV21		fourcc_code('N', 'V', '2', '1') /* 2x2 subsampled Cb:Cr plane */
#define DRM_FORMAT_P010		fourcc_code('P', '0', '1', '0') /* 2x2 subsampled Cr:Cb plane 10 bits per channel */

/*
 * Attention: Keep the DRM_MODE_TYPE_* bit definitions in sync with
 * include/uapi/drm/drm_mode.h bit definitions.
 */
#define DRM_MODE_TYPE_PREFERRED	(1 << 3)
#define DRM_MODE_TYPE_USERDEF	(1 << 5)
#define DRM_MODE_TYPE_DRIVER	(1 << 6)

/*
 * Attention: Keep the DPP_ATTR_* bit definitions in sync with
 * drivers/gpu/drm/samsung/dpu/cal_common/dpp_cal.h bit definitions.
 */
#define DPP_ATTR_AFBC		(1 << 0)
#define DPP_ATTR_SAJC		(1 << 0)
#define DPP_ATTR_BLOCK		(1 << 1)
#define DPP_ATTR_FLIP		(1 << 2)
#define DPP_ATTR_ROT		(1 << 3)
#define DPP_ATTR_CSC		(1 << 4)
#define DPP_ATTR_SCALE		(1 << 5)
#define DPP_ATTR_HDR		(1 << 6)
#define DPP_ATTR_C_HDR		(1 << 7)
#define DPP_ATTR_C_HDR10_PLUS	(1 << 8)
#define DPP_ATTR_WCG            (1 << 9)
#define DPP_ATTR_SBWC           (1 << 10)
#define DPP_ATTR_HDR10_PLUS	(1 << 11)
#define DPP_ATTR_IDMA		(1 << 16)
#define DPP_ATTR_ODMA		(1 << 17)
#define DPP_ATTR_DPP		(1 << 18)
#define DPP_ATTR_SRAMC		(1 << 19)
#define DPP_ATTR_HDR_COMM	(1 << 20)

/*
 * Attention: Keep the HDR_FORMAT_* bit definitions in sync with
 * drivers/gpu/drm/samsung/dpu/exynos_drm_connector.h HDR_* bit definitions.
 */
#define HDR_FORMAT_DV		(1 << 1)
#define HDR_FORMAT_HDR10	(1 << 2)
#define HDR_FORMAT_HLG		(1 << 3)
#define HDR_FORMAT_HDR10P	(1 << 4)
#endif	/* _DT_BINDINGS_EXYNOS_DISPLAY_H */
