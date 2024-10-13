#ifndef _UAPI_VIDEO_SPRD_ADF_H_
#define _UAPI_VIDEO_SPRD_ADF_H_

#include <linux/types.h>
#include <video/adf.h>

#define SPRD_ADF_MAX_PLANE 4

/**
 * overlay engine or hw layer's transform capability
 *
 * @ADF_TRANSFORM_NONE: don't has transform capability
 * @ADF_TRANSFORM_FLIP_H: flip source image horizontally
 * @ADF_TRANSFORM_FLIP_V: flip source image vertically
 * @ADF_TRANSFORM_ROT_90: rotate source image 90 degrees clockwise
 * @ADF_TRANSFORM_ROT_180: rotate source image 180 degrees
 * @ADF_TRANSFORM_ROT_270: rotate source image 270 degrees clockwise
 * @ADF_TRANSFORM_RESERVED: don't use. see system/window.h
 */
enum {
	ADF_TRANSFORM_NONE    = 0x00,
	ADF_TRANSFORM_FLIP_H    = 0x01,
	ADF_TRANSFORM_FLIP_V    = 0x02,
	ADF_TRANSFORM_ROT_90    = 0x04,
	ADF_TRANSFORM_ROT_180   = 0x03,
	ADF_TRANSFORM_ROT_270   = 0x07,
	ADF_TRANSFORM_RESERVED  = 0x08,
};

/**
 * overlay engine or hw layer's scale capability
 *
 * @ADF_SCALE_NONE: don't has scale capability
 */
enum {
	ADF_SCALE_NONE = 0,
};

/**
 * buffer's compressed flag
 *
 * @ADF_UNCOMPRESSED: hw layer buffer uncompressed
 * @ADF_COMPRESSED: hw layer buffer compressed
 */
enum {
	ADF_UNCOMPRESSED,
	ADF_COMPRESSED,
};

/**
 * overlay engine or hw layer's support of blending
 *
 * @ADF_BLENDING_NONE: don't has blending capability
 * @ADF_BLENDING_PREMULT: ONE / ONE_MINUS_SRC_ALPHA
 * @ADF_BLENDING_COVERAGE: SRC_ALPHA / ONE_MINUS_SRC_ALPHA
 *
 * this definitions is the copy of
 * hardware/libhardware/include/hardware/hwcomposer_defs.h,
 * for more info,you can see the target file hwcomposer_defs.h.
 */
enum {
	ADF_BLENDING_NONE     = 0x0100,
	ADF_BLENDING_PREMULT  = 0x0105,
	ADF_BLENDING_COVERAGE = 0x0405
};

/**
 * overlay engine or hw layer's support of format
 *
 * this definitions is the copy of
 * system/core/include/system/graphics.h,for more info,you
 * can see the target file graphics.h.
 */
enum {
	ADF_PIXEL_FORMAT_RGBA_8888          = 1,
	ADF_PIXEL_FORMAT_RGBX_8888          = 2,
	ADF_PIXEL_FORMAT_RGB_888            = 3,
	ADF_PIXEL_FORMAT_RGB_565            = 4,
	ADF_PIXEL_FORMAT_BGRA_8888          = 5,
	ADF_PIXEL_FORMAT_sRGB_A_8888        = 0xC,
	ADF_PIXEL_FORMAT_sRGB_X_8888        = 0xD,
	ADF_PIXEL_FORMAT_YV12   = 0x32315659,
	ADF_PIXEL_FORMAT_Y8     = 0x20203859,
	ADF_PIXEL_FORMAT_Y16    = 0x20363159,
	ADF_PIXEL_FORMAT_RAW16 = 0x20,
	ADF_PIXEL_FORMAT_RAW_SENSOR = 0x20,
	ADF_PIXEL_FORMAT_RAW10 = 0x25,
	ADF_PIXEL_FORMAT_RAW_OPAQUE = 0x24,
	ADF_PIXEL_FORMAT_BLOB = 0x21,
	ADF_PIXEL_FORMAT_IMPLEMENTATION_DEFINED = 0x22,
	ADF_PIXEL_FORMAT_YCbCr_420_888 = 0x23,
	ADF_PIXEL_FORMAT_YCbCr_422_SP       = 0x10,
	ADF_PIXEL_FORMAT_YCrCb_420_SP       = 0x11,
	ADF_PIXEL_FORMAT_YCbCr_422_P  = 0x12,
	ADF_PIXEL_FORMAT_YCbCr_420_P  = 0x13,
	ADF_PIXEL_FORMAT_YCbCr_422_I        = 0x14,
	ADF_PIXEL_FORMAT_YCbCr_420_I  = 0x15,
	ADF_PIXEL_FORMAT_CbYCrY_422_I = 0x16,
	ADF_PIXEL_FORMAT_CbYCrY_420_I = 0x17,
	ADF_PIXEL_FORMAT_YCbCr_420_SP_TILED = 0x18,
	ADF_PIXEL_FORMAT_YCbCr_420_SP       = 0x19,
	ADF_PIXEL_FORMAT_YCrCb_420_SP_TILED = 0x1A,
	ADF_PIXEL_FORMAT_YCrCb_422_SP       = 0x1B,
	ADF_PIXEL_FORMAT_YCrCb_420_P  = 0x1C,
};

/**
 * sprd_adf_device_capability - device capability
 *
 * @device_id: device unique id
 */
struct  sprd_adf_device_capability {
	__u32	device_id;
};

/**
 * sprd_adf_interface_capability - interface capability
 *
 * @interface_id: interface unique id
 * @fb_count: the nunber of framebuffer
 * @fb_format: framebuffer's format
 */
struct sprd_adf_interface_capability {
	__u32	interface_id;

	__u32	fb_count;
	__u32	fb_format;
};

/**
 * sprd_adf_hwlayer_capability - hwlayer capability
 *
 * @hwlayer_id: hwlayer unique id
 * @format: hwlayer's capability of format
 * @rotation: hwlayer's capability of rotation
 * @scale: hwlayer's capability of scale
 * @blending: hwlayer's capability of blending
 */
struct sprd_adf_hwlayer_capability {
	__u32	hwlayer_id;

	__u32	format;
	__u32	rotation;
	__u32	scale;
	__u32	blending;
};

/**
 * sprd_adf_overlayengine_capability - overlayengine capability
 *
 * @number_hwlayer: the number of hwlayer that attach to it
 * @format: overlayengine's capability of format
 * @rotation: overlayengine's capability of rotation
 * @scale: overlayengine's capability of scale
 * @blending: overlayengine's capability of blending
 * @hwlayers,hwlayer_ptr: hw layer's capability,
 * hwlayer entries will follow this structure in memory
 */
struct sprd_adf_overlayengine_capability {
	__u32	number_hwlayer;

	__u32	format;
	__u32	rotation;
	__u32	scale;
	__u32	blending;

	union {
		struct sprd_adf_hwlayer_capability     hwlayers[0];
		const struct sprd_adf_hwlayer_capability
				*hwlayer_ptr[SPRD_ADF_MAX_PLANE];
	};
};

/**
 * sprd_adf_hwlayer_custom_data - custom set the private data
 *
 * @interface_id: unique interface id
 * @hwlayer_id: unique hwlayer id
 * @buffer_id: unique buffer index in post array(for debug)
 * @alpha: layer's alpha value
 * @dst_x: layer's dest x
 * @dst_y: layer's dest y
 * @dst_w: layer's dest w
 * @dst_h: layer's dest h
 * @blending: layer's blending mode
 * @rotation: layer's rotation value
 * @scale: layer's scale value
 * @compression: layer buffer's Compression mode
 */
struct sprd_adf_hwlayer_custom_data {
	__u32	interface_id;
	__u32	hwlayer_id;
	__u32	buffer_id;
	__u32	alpha;
	__s16	dst_x;
	__s16	dst_y;
	__u16	dst_w;
	__u16	dst_h;
	__u32   blending;
	__u32	rotation;
	__u32	scale;
	__u32   compression;
};

/**
 * sprd_adf_post_custom_data - custom's post private data
 *
 * @version: ADF_VERSION for backwards compatibility support
 * @num_interfaces: the number of interface in current post data
 * @retire_fence: this post cmd's retire_fence,for sf soft vsync
 * @hwlayers: all hwlayer's config data in this post data,
 * entries will follow this structure in memory
 */
struct sprd_adf_post_custom_data {
	__u32	version;
	__u32	num_interfaces;
	__s32	retire_fence;
	struct sprd_adf_hwlayer_custom_data	hwlayers[0];
};

#endif /* _UAPI_VIDEO_SPRD_ADF_H_ */
