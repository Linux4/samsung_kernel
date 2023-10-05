/*
 * Samsung EXYNOS CAMERA PostProcessing VRA driver
 *
 * Copyright (C) 2022 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef CAMERAPP_VRA__H_
#define CAMERAPP_VRA__H_

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_media.h>
#include "videodev2_exynos_camera.h"
#include <linux/io.h>
#include <linux/dma-buf.h>
#include <linux/version.h>
#include <media/videobuf2-core.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-ctrls.h>
#if IS_ENABLED(CONFIG_VIDEOBUF2_DMA_SG)
#include <media/videobuf2-dma-sg.h>
#endif
#include "pablo-kernel-variant.h"

#include "is-video.h"

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS)
#define vra_pm_qos_request		exynos_pm_qos_request
#define vra_pm_qos_add_request		exynos_pm_qos_add_request
#define vra_pm_qos_update_request	exynos_pm_qos_update_request
#define vra_pm_qos_remove_request	exynos_pm_qos_remove_request
#else
#define vra_pm_qos_request		dev_pm_qos_request
#define vra_pm_qos_add_request		(arg...)
#define vra_pm_qos_update_request	(arg...)
#define vra_pm_qos_remove_request	(arg...)
#endif

int vra_get_debug_level(void);
#define vra_dbg(fmt, args...)						\
	do {								\
		if (vra_get_debug_level())				\
			pr_info("[%s:%d] "				\
			fmt, __func__, __LINE__, ##args);		\
	} while (0)

#define vra_info(fmt, args...)						\
	pr_info("[%s:%d] "						\
	fmt, __func__, __LINE__, ##args);

#define call_bufop(q, op, args...)					\
({									\
	int ret = 0;							\
	if (q && q->buf_ops && q->buf_ops->op)				\
		ret = q->buf_ops->op(args);				\
	ret;								\
})

/* VRA test */
/* #define USE_VELOCE */
/* #define USE_CLOCK_INFO */

#define VRA_MODULE_NAME		"camerapp-vra"
#ifdef USE_VELOCE
#define VRA_TIMEOUT		msecs_to_jiffies(2000)  /* 2000ms */
#define VRA_WDT_CNT		10
#else
#define VRA_TIMEOUT		msecs_to_jiffies(1000)  /* 1000ms */
#define VRA_WDT_CNT		5
#endif

#define VRA_MAX_PLANES		3

#define BGR_COLOR_CHANNELS	3
#define BGRA_COLOR_CHANNELS	4

/* VRA hardware device state */
#define DEV_RUN			1
#define DEV_SUSPEND		2
#define DEV_READY		3

enum VRA_DEBUG_MODE {
	VRA_DEBUG_OFF,
	VRA_DEBUG_LOG,
	VRA_DEBUG_BUF_DUMP,
	VRA_DEBUG_MEASURE,
};

/* VRA dump */
/* #define FILE_DUMP */

#define VRA_DUMP_PATH		("/data/vendor/camera")
#define VRA_INSTRUCTION_DUMP	("vra_dump_instruction")
#define VRA_CONSTANT_DUMP	("vra_dump_constant")
#define VRA_TEMP_DUMP		("vra_dump_temp")
#define VRA_UNKNOWN_DUMP	("vra_dump_unknown")
#define VRA_INPUT_DUMP		("vra_dump_input")
#define VRA_DMA_COUNT		(3)

enum VRA_BUF_TYPE {
	VRA_BUF_INSTRUCTION,
	VRA_BUF_CONSTANT,
	VRA_BUF_TEMP,
};

enum VRA_IRQ_DONE_TYPE {
	VRA_IRQ_FRAME_END,
	VRA_IRQ_ERROR,
};

#define VRA_CLOCK_MIN		(200000)

/* VRA context state */
#define CTX_PARAMS	1
#define CTX_STREAMING	2
#define CTX_RUN		3
#define CTX_ABORT	4
#define CTX_SRC_FMT	5
#define CTX_DST_FMT	6
#define CTX_INT_FRAME	7 /* intermediate frame available */

#define fh_to_vra_ctx(__fh)	container_of(__fh, struct vra_ctx, fh)
#define vra_fmt_is_rgb888(x)	((x == V4L2_PIX_FMT_RGB32) || \
		(x == V4L2_PIX_FMT_BGR32))
#define vra_fmt_is_yuv422(x)	((x == V4L2_PIX_FMT_YUYV) || \
			(x == V4L2_PIX_FMT_UYVY) || (x == V4L2_PIX_FMT_YVYU) || \
			(x == V4L2_PIX_FMT_YUV422P) || (x == V4L2_PIX_FMT_NV16) || \
			(x == V4L2_PIX_FMT_NV61))
#define vra_fmt_is_yuv420(x)	((x == V4L2_PIX_FMT_YUV420) || \
			(x == V4L2_PIX_FMT_YVU420) || (x == V4L2_PIX_FMT_NV12) || \
			(x == V4L2_PIX_FMT_NV21) || (x == V4L2_PIX_FMT_NV12M) || \
			(x == V4L2_PIX_FMT_NV21M) || (x == V4L2_PIX_FMT_YUV420M) || \
			(x == V4L2_PIX_FMT_YVU420M) || (x == V4L2_PIX_FMT_NV12MT_16X16))
#define vra_fmt_is_ayv12(x)	((x) == V4L2_PIX_FMT_YVU420)

enum vra_clk_status {
	VRA_CLK_ON,
	VRA_CLK_OFF,
};

enum vra_clocks {
	VRA_GATE_CLK,
	VRA_CHLD_CLK,
	VRA_PARN_CLK
};

/*
 * struct vra_size_limit - VRA variant size information
 *
 * @min_w: minimum pixel width size
 * @min_h: minimum pixel height size
 * @max_w: maximum pixel width size
 * @max_h: maximum pixel height size
 */
struct vra_size_limit {
	u16 min_w;
	u16 min_h;
	u16 max_w;
	u16 max_h;
};

struct vra_variant {
	struct vra_size_limit limit_input;
	struct vra_size_limit limit_output;
	u32 version;
};

/*
 * struct vra_fmt - the driver's internal color format data
 * @name: format description
 * @pixelformat: the fourcc code for this format, 0 if not applicable
 * @num_planes: number of physically non-contiguous data planes
 * @num_comp: number of color components(ex. RGB, Y, Cb, Cr)
 * @h_div: horizontal division value of C against Y for crop
 * @v_div: vertical division value of C against Y for crop
 * @bitperpixel: bits per pixel
 * @color: the corresponding vra_color_fmt
 */
struct vra_fmt {
	char	*name;
	u32	pixelformat;
	u32	cfg_val;
	u8	bitperpixel[VRA_MAX_PLANES];
	u8	num_planes:2; /* num of buffer */
	u8	num_comp:2;	/* num of hw_plane */
	u8	h_shift:1;
	u8	v_shift:1;
};

struct vra_addr {
	dma_addr_t	y;
	dma_addr_t	cb;
	dma_addr_t	cr;
	unsigned int	ysize;
	unsigned int	cbsize;
	unsigned int	crsize;
	dma_addr_t	y_2bit;
	dma_addr_t	cbcr_2bit;
	unsigned int	ysize_2bit;
	unsigned int	cbcrsize_2bit;
};

/*
 * struct vra_frame - source/target frame properties
 * @fmt:	buffer format(like virtual screen)
 * @crop:	image size / position
 * @addr:	buffer start address(access using VRA_ADDR_XXX)
 * @bytesused:	image size in bytes (w x h x bpp)
 */
struct vra_frame {
	const struct vra_fmt	*vra_fmt;
	unsigned short		width;
	unsigned short		height;
	__u32			pixelformat;
	struct vra_addr		addr;
	__u32			bytesused[VRA_MAX_PLANES];
	__u32			pixel_size;
	__u32			num_planes;
};

/*
 * struct vra_m2m_device - v4l2 memory-to-memory device data
 * @v4l2_dev: v4l2 device
 * @vfd: the video device node
 * @m2m_dev: v4l2 memory-to-memory device data
 * @in_use: the open count
 */
struct vra_m2m_device {
	struct v4l2_device	v4l2_dev;
	struct video_device	*vfd;
	struct v4l2_m2m_dev	*m2m_dev;
	atomic_t		in_use;
};

struct vra_wdt {
	struct timer_list	timer;
	atomic_t		cnt;
};

typedef uint32_t DmaAddress;

struct DmaBuffer {
	DmaAddress		address;
	uint8_t			*ptr;
	uint32_t		size;
};

typedef enum {
    VRA_CLOCK_STATIC,	/* Use the highest clock */
    VRA_CLOCK_DYNAMIC,	/* Each model operates at each clock level */
} VRA_ClockMode;

struct vra_model_param {
	struct DmaBuffer	instruction;
	struct DmaBuffer	constant;
	struct DmaBuffer	temporary;
	uint8_t			inputFormat;
	uint16_t		inputWidth;
	uint16_t		inputHeight;
	uint32_t		inputSize;
	uint32_t		outputSize;
	uint32_t		timeOut;
	uint32_t		clockLevel;
	uint32_t		clockMode;
	uint32_t		reserved[6];
};

/*
 * vra_ctx - the abstration for VRA open context
 * @node:		list to be added to vra_dev.context_list
 * @vra_dev:		the VRA device this context applies to
 * @m2m_ctx:		memory-to-memory device context
 * @s_frame:		source frame properties
 * @d_frame:		destination frame properties
 * @i_addr:		instruction dma address
 * @c_addr:		constant dma address
 * @t_addr:		temporary dma address
 * @fh:			v4l2 file handle
 * @flags:		context state flags
 * @model_param:		model information from user
 */
struct vra_ctx {
	struct list_head		node;
	struct vra_dev			*vra_dev;
	struct v4l2_m2m_ctx		*m2m_ctx;
	struct vra_frame		s_frame;
	struct vra_frame		d_frame;
	dma_addr_t 			i_addr;
	dma_addr_t			c_addr;
	dma_addr_t			t_addr;
	struct v4l2_fh			fh;
	unsigned long			flags;
	struct vra_model_param		model_param;
	struct dma_buf			*dmabuf[VRA_DMA_COUNT];
	struct dma_buf_attachment	*attachment[VRA_DMA_COUNT];
	struct sg_table			*sgt[VRA_DMA_COUNT];
};

struct vra_priv_buf {
	dma_addr_t	daddr;
	void		*vaddr;
	size_t		size;
};

/*
 * struct vra_dev - the abstraction for VRA device
 * @dev:	pointer to the VRA device
 * @variant:	the IP variant information
 * @m2m:	memory-to-memory V4L2 device information
 * @aclk:	aclk required for gdc operation
 * @regs_base:	the mapped hardware registers
 * @regs_rsc:	the resource claimed for IO registers
 * @wait:	interrupt handler waitqueue
 * @ws:		work struct
 * @state:	device state flags
 * @alloc_ctx:	videobuf2 memory allocator context
 * @slock:	the spinlock pscecting this data structure
 * @lock:	the mutex pscecting this data structure
 * @wdt:	watchdog timer information
 * @version:	IP version number
 */
struct vra_dev {
	struct device			*dev;
	const struct vra_variant	*variant;
	struct vra_m2m_device		m2m;
	struct clk			*aclk;
	void __iomem			*regs_base;
	struct resource			*regs_rsc;
	wait_queue_head_t		wait;
	unsigned long			state;
	struct vb2_alloc_ctx		*alloc_ctx;
	spinlock_t			slock;
	struct mutex			lock;
	struct vra_wdt			wdt;
	spinlock_t			ctxlist_lock;
	struct vra_ctx			*current_ctx;
	struct list_head		context_list;
	struct vra_pm_qos_request	qosreq_m2m;
	unsigned int			irq;
	s32				qosreq_m2m_level;
	int				dev_id;
	u32				version;
};

static inline struct vra_frame *ctx_get_frame(struct vra_ctx *ctx,
						enum v4l2_buf_type type)
{
	struct vra_frame *frame;

	if (V4L2_TYPE_IS_MULTIPLANAR(type)) {
		if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
			frame = &ctx->s_frame;
		else
			frame = &ctx->d_frame;
	} else {
		dev_err(ctx->vra_dev->dev,
				"Wrong V4L2 buffer type %d\n", type);
		return ERR_PTR(-EINVAL);
	}

	return frame;
}

extern void is_debug_s2d(bool en_s2d, const char *fmt, ...);

#if IS_ENABLED(CONFIG_VIDEOBUF2_DMA_SG)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 20, 0))
static inline dma_addr_t vra_get_dma_address(struct vb2_buffer *vb2_buf, u32 plane)
{
	struct sg_table *sgt;
	sgt = vb2_dma_sg_plane_desc(vb2_buf, plane);

	return (dma_addr_t)sg_dma_address(sgt->sgl);
}

static inline void  *vra_get_kvaddr(struct vb2_buffer *vb2_buf, u32 plane)
{
	return vb2_plane_vaddr(vb2_buf, plane);
}
#else
static inline dma_addr_t vra_get_dma_address(struct vb2_buffer *vb2_buf, u32 plane)
{
	return vb2_dma_sg_plane_dma_addr(vb2_buf, plane);
}

static inline void  *vra_get_kvaddr(struct vb2_buffer *vb2_buf, u32 plane)
{
	return vb2_plane_vaddr(vb2_buf, plane);
}
#endif
#else
static inline dma_addr_t vra_get_dma_address(void *cookie, dma_addr_t *addr)
{
	return NULL;
}

static inline void *vra_get_kernel_address(void *cookie)
{
	return NULL;
}

#endif
#if IS_ENABLED(CONFIG_PABLO_KUNIT_TEST)
#define KUNIT_EXPORT_SYMBOL(x) EXPORT_SYMBOL_GPL(x)
#else
#define KUNIT_EXPORT_SYMBOL(x)
#endif

#endif /* CAMERAPP_VRA__H_ */
