/*
 * MMP v4l2 overlay driver
 *
 * adapted from omap_vout.c
 * Move to videobuf2 layer to support R/L frame pairs per HDMI 3D requirement.
 *	- Jun 27 2011. Jun Nie(njun@marvell.com)
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 *
 */

#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/videodev2.h>
#include <linux/clk.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ioctl.h>
#include <media/v4l2-common.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-dma-contig.h>

#include "../../video/pxa168fb_common.h"

/* configuration macros */
#define VOUT_NAME		"mmp_vout"

#define VID_MAX_WIDTH		1920	/* Largest width */
#define VID_MAX_HEIGHT		1080	/* Largest height */
#define VID_QVGA_WIDTH		320
#define VID_QVGA_HEIGHT		240
#define VID_MIN_WIDTH		16
#define VID_MIN_HEIGHT		16

#define BUF_TYPE (ovly->hdmi3d ? V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE : \
	V4L2_BUF_TYPE_VIDEO_OUTPUT)

#define MAX_BUF_SIZE (VID_MAX_WIDTH*VID_MAX_HEIGHT*4)

/* max control: hue, alpha, chroma key, contrast, saturation, gamma */
#define MAX_CID		5
#define COLOR_KEY_GFX_DST 0
#define COLOR_KEY_VID_SRC 1

/*
 * Maximum amount of memory to use for rendering buffers.
 * Default is enough to four (RGB24) DVI 720P buffers.
 */
#define MAX_ALLOWED_VIDBUFFERS            4

const static struct v4l2_fmtdesc mmp_formats[] = {
	{
	 .description = "YUV420, planer",
	 .pixelformat = V4L2_PIX_FMT_YUV420,
	},
	{
	 .description = "YVU420, planer",
	 .pixelformat = V4L2_PIX_FMT_YVU420,
	},
	{
	 .description = "YUV422, planer",
	 .pixelformat = V4L2_PIX_FMT_YUV422P,
	},
	{
	 .description = "YUYV (YUV 4:2:2), packed",
	 .pixelformat = V4L2_PIX_FMT_YUYV,
	},
	{
	 .description = "UYVY (YUV 4:2:2), packed",
	 .pixelformat = V4L2_PIX_FMT_UYVY,
	},
	{
	 .description = "RGB565",
	 .pixelformat = V4L2_PIX_FMT_RGB565X,
	},
	{
	 .description = "RGB8888, unpacked",
	 .pixelformat = V4L2_PIX_FMT_RGB32,
	},
	{
	 .description = "BGR8888, unpacked",
	 .pixelformat = V4L2_PIX_FMT_BGR32,
	},
	{
	 .description = "RGB888, packed",
	 .pixelformat = V4L2_PIX_FMT_RGB24,
	},
	{
	 .description = "BGR888, packed",
	 .pixelformat = V4L2_PIX_FMT_BGR24,
	},
};
#define NUM_OUTPUT_FORMATS (ARRAY_SIZE(mmp_formats))

struct mmp_buf {
	/* common v4l buffer stuff -- must be first */
	struct vb2_buffer       vb;
	struct list_head        list;
};

struct mmp_overlay {
	int id;
	struct device *dev;
	const char *name;
	void *reg_base;
	struct clk *clk;

	struct video_device *vdev;
	int opened;

	int enabled;
	bool hdmi3d;
	bool alpha_enabled;
	dma_addr_t paddr[2];
	u16 ypitch;
	u16 uvpitch;
	int y_size;
	int uv_size;
	int trans_enabled;
	int trans_key_type;
	int trans_key;
	bool mirror;

	u16 pos_x;
	u16 pos_y;
	u16 out_width;		/* if 0, out_width == width */
	u16 out_height;		/* if 0, out_height == height */
	u8 global_alpha;
	struct _sColorKeyNAlpha ckey_alpha;	/* fix me */
	u32 dma_ctl0;
	bool update;		/* if 1, update the overlay control info */

	spinlock_t vbq_lock;	/* spinlock for videobuf queues */

	/* non-NULL means streaming is in progress.
	 * We could use native vb2 streaming flag vb2_is_streaming(&ovly->vbq)
	 * But this hacking flag is needed for clone mode.
	 * So ignore vb2 flag in driver
	 */
	bool streaming;

	struct v4l2_pix_format pix;
	struct v4l2_pix_format_mplane	pix_mp;
	struct v4l2_rect crop;
	struct v4l2_window win;
	struct v4l2_framebuffer fbuf;	/* graphics layer info */

	/* Lock to protect the shared data structures in ioctl */
	struct mutex lock;

	/* V4L2 control structure for different control id */
	struct v4l2_control control[MAX_CID];
	int flicker_filter;
	/* V4L2 control structure for different control id */

	int ps, vr_ps, line_length, field_id;
	enum v4l2_memory memory;
	struct mmp_buf *cur_frm;
	struct vb2_alloc_ctx    *alloc_ctx;
	struct list_head dma_queue;
	u32 cropped_offset;
	s32 tv_field1_offset;

	/* Buffer queue variabled */
	enum v4l2_buf_type type;
	struct vb2_queue vbq;
};

struct mmp_overlay *v4l2_ovly[3];

/* Local Helper functions */
static void mmp_ovly_cleanup_device(struct mmp_overlay *ovly);
static bool debug;

module_param(debug, bool, S_IRUGO);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

/* Return the default overlay cropping rectangle in crop given the image
 * size in pix and the video display size in fbuf.  The default
 * cropping rectangle is the largest rectangle no larger than the capture size
 * that will fit on the display.  The default cropping rectangle is centered in
 * the image.  All dimensions and offsets are rounded down to even numbers.
 */
void mmp_ovly_default_crop(struct v4l2_pix_format *pix,
			      struct v4l2_framebuffer *fbuf,
			      struct v4l2_rect *crop)
{
	crop->width = (pix->width < VID_MAX_WIDTH) ?
	    pix->width : VID_MAX_WIDTH;
	crop->height = (pix->height < VID_MAX_HEIGHT) ?
	    pix->height : VID_MAX_HEIGHT;
	crop->width &= ~1;
	crop->height &= ~1;
	crop->left = ((pix->width - crop->width) >> 1) & ~1;
	crop->top = ((pix->height - crop->height) >> 1) & ~1;
}

/* Given a new render window in new_win, adjust the window to the
 * nearest supported configuration.  The adjusted window parameters are
 * returned in new_win.
 * Returns zero if succesful, or -EINVAL if the requested window is
 * impossible and cannot reasonably be adjusted.
 */
int mmp_ovly_try_window(struct v4l2_framebuffer *fbuf,
			   struct v4l2_window *new_win)
{
	struct v4l2_rect try_win;

	/* make a working copy of the new_win rectangle */
	try_win = new_win->w;

	/* adjust the preview window so it fits on the display by clipping any
	 * offscreen areas
	 */
	if (try_win.left < 0) {
		try_win.width += try_win.left;
		try_win.left = 0;
	}
	if (try_win.top < 0) {
		try_win.height += try_win.top;
		try_win.top = 0;
	}
	try_win.width = (try_win.width < fbuf->fmt.width) ?
	    try_win.width : fbuf->fmt.width;
	try_win.height = (try_win.height < fbuf->fmt.height) ?
	    try_win.height : fbuf->fmt.height;
	if (try_win.left + try_win.width > fbuf->fmt.width)
		try_win.width = fbuf->fmt.width - try_win.left;
	if (try_win.top + try_win.height > fbuf->fmt.height)
		try_win.height = fbuf->fmt.height - try_win.top;
	try_win.width &= ~1;
	try_win.height &= ~1;

	if (try_win.width <= 0 || try_win.height <= 0)
		return -EINVAL;

	/* We now have a valid preview window, so go with it */
	new_win->w = try_win;
	new_win->field = V4L2_FIELD_ANY;
	return 0;
}

/* Given a new render window in new_win, adjust the window to the
 * nearest supported configuration.  The image cropping window in crop
 * will also be adjusted if necessary.  Preference is given to keeping the
 * the window as close to the requested configuration as possible.  If
 * successful, new_win, ovly->win, and crop are updated.
 * Returns zero if succesful, or -EINVAL if the requested preview window is
 * impossible and cannot reasonably be adjusted.
 */
int mmp_ovly_new_window(struct v4l2_rect *crop,
			   struct v4l2_window *win,
			   struct v4l2_framebuffer *fbuf,
			   struct v4l2_window *new_win)
{
	int err;

	err = mmp_ovly_try_window(fbuf, new_win);
	if (err)
		return err;

	/* update our preview window */
	win->w = new_win->w;
	win->field = new_win->field;
	win->chromakey = new_win->chromakey;

	/* adjust the cropping window to allow for resizing limitations */
	if (crop->height >= (win->w.height << 2)) {
		/* The maximum vertical downsizing ratio is 4:1 */
		crop->height = win->w.height << 2;
	}
	if (crop->width >= (win->w.width << 2)) {
		/* The maximum horizontal downsizing ratio is 4:1 */
		crop->width = win->w.width << 2;
	}
	return 0;
}

/* Given a new cropping rectangle in new_crop, adjust the cropping rectangle to
 * the nearest supported configuration.  The image render window in win will
 * also be adjusted if necessary.  The preview window is adjusted such that the
 * horizontal and vertical rescaling ratios stay constant.  If the render
 * window would fall outside the display boundaries, the cropping rectangle
 * will also be adjusted to maintain the rescaling ratios.  If successful, crop
 * and win are updated.
 * Returns zero if succesful, or -EINVAL if the requested cropping rectangle is
 * impossible and cannot reasonably be adjusted.
 */
int mmp_ovly_new_crop(struct v4l2_pix_format *pix,
			 struct v4l2_rect *crop, struct v4l2_window *win,
			 struct v4l2_framebuffer *fbuf,
			 const struct v4l2_rect *new_crop)
{
	struct v4l2_rect try_crop;
	unsigned long vresize, hresize;

	/* make a working copy of the new_crop rectangle */
	try_crop = *new_crop;

	/* adjust the cropping rectangle so it fits in the image */
	if (try_crop.left < 0) {
		try_crop.width += try_crop.left;
		try_crop.left = 0;
	}
	if (try_crop.top < 0) {
		try_crop.height += try_crop.top;
		try_crop.top = 0;
	}
	try_crop.width = (try_crop.width < pix->width) ?
	    try_crop.width : pix->width;
	try_crop.height = (try_crop.height < pix->height) ?
	    try_crop.height : pix->height;
	if (try_crop.left + try_crop.width > pix->width)
		try_crop.width = pix->width - try_crop.left;
	if (try_crop.top + try_crop.height > pix->height)
		try_crop.height = pix->height - try_crop.top;
	try_crop.width &= ~1;
	try_crop.height &= ~1;
	if (try_crop.width <= 0 || try_crop.height <= 0)
		return -EINVAL;

	/* vertical resizing */
	vresize = (1024 * crop->height) / win->w.height;
	if (vresize > 4096)
		vresize = 4096;
	else if (vresize == 0)
		vresize = 1;
	win->w.height = ((1024 * try_crop.height) / vresize) & ~1;
	if (win->w.height == 0)
		win->w.height = 2;
	if (win->w.height + win->w.top > fbuf->fmt.height) {
		/* We made the preview window extend below the bottom of the
		 * display, so clip it to the display boundary and resize the
		 * cropping height to maintain the vertical resizing ratio.
		 */
		win->w.height = (fbuf->fmt.height - win->w.top) & ~1;
		if (try_crop.height == 0)
			try_crop.height = 2;
	}
	/* horizontal resizing */
	hresize = (1024 * crop->width) / win->w.width;
	if (hresize > 4096)
		hresize = 4096;
	else if (hresize == 0)
		hresize = 1;
	win->w.width = ((1024 * try_crop.width) / hresize) & ~1;
	if (win->w.width == 0)
		win->w.width = 2;
	if (win->w.width + win->w.left > fbuf->fmt.width) {
		/* We made the preview window extend past the right side of the
		 * display, so clip it to the display boundary and resize the
		 * cropping width to maintain the horizontal resizing ratio.
		 */
		win->w.width = (fbuf->fmt.width - win->w.left) & ~1;
		if (try_crop.width == 0)
			try_crop.width = 2;
	}

	/* Check for resizing constraints */
	if (try_crop.height >= (win->w.height << 2)) {
		/* The maximum vertical downsizing ratio is 4:1 */
		try_crop.height = win->w.height << 2;
	}
	if (try_crop.width >= (win->w.width << 2)) {
		/* The maximum horizontal downsizing ratio is 4:1 */
		try_crop.width = win->w.width << 2;
	}

	/* update our cropping rectangle and we're done */
	*crop = try_crop;
	return 0;
}

/* Given a new format in pix and fbuf,  crop and win
 * structures are initialized to default values. crop
 * is initialized to the largest window size that will fit on the display.  The
 * crop window is centered in the image. win is initialized to
 * the same size as crop and is centered on the display.
 * All sizes and offsets are constrained to be even numbers.
 */
void mmp_ovly_new_format(struct v4l2_pix_format *pix,
			    struct v4l2_framebuffer *fbuf,
			    struct v4l2_rect *crop, struct v4l2_window *win)
{
	/* crop defines the preview source window in the image capture
	 * buffer
	 */
	mmp_ovly_default_crop(pix, fbuf, crop);


	win->w.width = (crop->width < fbuf->fmt.width) ?
	    crop->width : fbuf->fmt.width;
	win->w.height = (crop->height < fbuf->fmt.height) ?
	    crop->height : fbuf->fmt.height;
	win->w.width &= ~1;
	win->w.height &= ~1;
	win->w.left = ((fbuf->fmt.width - win->w.width) >> 1) & ~1;
	win->w.top = ((fbuf->fmt.height - win->w.height) >> 1) & ~1;
}

void pixmp_to_pix(struct v4l2_pix_format *pix,
		 struct v4l2_pix_format_mplane *pix_mp)
{
	pix->width = pix_mp->width;
	pix->height = pix_mp->height;
	pix->pixelformat = pix_mp->pixelformat;
	pix->field = pix_mp->field;
	pix->colorspace = pix_mp->colorspace;
	pix->bytesperline = pix_mp->plane_fmt[0].bytesperline;
	pix->sizeimage = pix_mp->plane_fmt[0].sizeimage;
}

/* Assume 3D R/L frame pairs. We do not support discrete YUV planars */
void pix_to_pixmp(struct v4l2_pix_format *pix,
		 struct v4l2_pix_format_mplane *pix_mp)
{
	pix_mp->width = pix->width;
	pix_mp->height = pix->height;
	pix_mp->pixelformat = pix->pixelformat;
	pix_mp->field = pix->field;
	pix_mp->colorspace = pix->colorspace;
	pix_mp->plane_fmt[0].bytesperline = pix->bytesperline;
	pix_mp->plane_fmt[0].sizeimage = pix->sizeimage;
	pix_mp->plane_fmt[1].bytesperline = pix->bytesperline;
	pix_mp->plane_fmt[1].sizeimage = pix->sizeimage;
	pix_mp->num_planes = 2;
}

/* Try format */
static int mmp_ovly_try_format(struct v4l2_pix_format *pix)
{
	int ifmt, bpp = 0;

	pix->height = clamp(pix->height, (u32) VID_MIN_HEIGHT,
			    (u32) VID_MAX_HEIGHT);
	pix->width =
	    clamp(pix->width, (u32) VID_MIN_WIDTH, (u32) VID_MAX_WIDTH);

	for (ifmt = 0; ifmt < NUM_OUTPUT_FORMATS; ifmt++) {
		if (pix->pixelformat == mmp_formats[ifmt].pixelformat)
			break;
	}

	if (ifmt == NUM_OUTPUT_FORMATS)
		ifmt = 0;

	pix->pixelformat = mmp_formats[ifmt].pixelformat;
	pix->field = V4L2_FIELD_ANY;
	pix->priv = 0;

	switch (pix->pixelformat) {
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	default:
		pix->colorspace = V4L2_COLORSPACE_JPEG;
		bpp = 12;
		break;
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
		pix->colorspace = V4L2_COLORSPACE_JPEG;
		bpp = 16;
		break;
	case V4L2_PIX_FMT_RGB565:
	case V4L2_PIX_FMT_RGB565X:
		pix->colorspace = V4L2_COLORSPACE_SRGB;
		bpp = 16;
		break;
	case V4L2_PIX_FMT_RGB24:
	case V4L2_PIX_FMT_BGR24:
		pix->colorspace = V4L2_COLORSPACE_SRGB;
		bpp = 24;
		break;
	case V4L2_PIX_FMT_RGB32:
	case V4L2_PIX_FMT_BGR32:
		pix->colorspace = V4L2_COLORSPACE_SRGB;
		bpp = 32;
		break;
	}
	pix->bytesperline = (pix->width * bpp) >> 3;
	pix->sizeimage = pix->bytesperline * pix->height;
	return 0;
}

/* Setup the overlay info */
int mmpvid_setup_overlay(struct mmp_overlay *ovly, int posx, int posy,
			    int outw, int outh)
{
	ovly->pos_x = posx;
	ovly->pos_y = posy;
	ovly->out_width = outw;
	ovly->out_height = outh;

	ovly->global_alpha = ovly->win.global_alpha;
	ovly->update = 1;

	v4l2_dbg(1, debug, ovly->vdev,
		 "%s:id %d ovly.enable=%d hdmi3d=%d ovly.addr=%x\n"
		 "pix.width=%d pix.height=%d crop.width=%d crop.height=%d\n"
		 "ovly.posx=%d ovly.posy=%d ovly.out_width = %d "
		 "ovly.out_height=%d\n ",
		 __func__, ovly->id, ovly->enabled, ovly->hdmi3d,
		 ovly->paddr[0], ovly->pix.width, ovly->pix.height,
		 ovly->crop.width, ovly->crop.height,
		 ovly->pos_x, ovly->pos_y, ovly->out_width, ovly->out_height);
	return 0;
}

/* Initialize the overlay structure */
int mmpvid_init(struct mmp_overlay *ovly)
{
	int ret = 0;
	int posx, posy;
	int outw, outh;
	u32 reg;
	struct v4l2_window *win;
	struct v4l2_pix_format *pix;
	struct pxa168fb_mach_info *mi;

	pix = &ovly->pix;
	mi = ovly->dev->platform_data;

	reg = dma_ctrl_read(ovly->id, 0);
	reg &= 0xef0fffe1;
	switch (pix->pixelformat) {
	case V4L2_PIX_FMT_YVU420:
		reg |= CFG_DMA_SWAPUV(1);
	case V4L2_PIX_FMT_YUV420:
	default:
		reg |= CFG_DMAFORMAT(GMODE_YUV420PLANAR) |
			CFG_YUV2RGB_DMA(1) |
			(mi->panel_rbswap << 4);
		ovly->ypitch = pix->width;
		ovly->uvpitch = pix->width >> 1;
		ovly->y_size = pix->width * pix->height;
		ovly->uv_size = ovly->y_size >> 2;
		break;
	case V4L2_PIX_FMT_YUV422P:
		reg |= CFG_DMAFORMAT(GMODE_YUV422PLANAR) |
			CFG_YUV2RGB_DMA(1) |
			(mi->panel_rbswap << 4);
		ovly->ypitch = pix->width;
		ovly->uvpitch = pix->width >> 1;
		ovly->y_size = pix->width * pix->height;
		ovly->uv_size = ovly->y_size >> 1;
		break;
	case V4L2_PIX_FMT_YUYV:
		reg |= CFG_DMA_SWAPYU(1);
	case V4L2_PIX_FMT_UYVY:
		reg |= CFG_DMAFORMAT(GMODE_YUV422PACKED) |
			CFG_YUV2RGB_DMA(1) |
			(mi->panel_rbswap << 4);
		ovly->ypitch = pix->width << 1;
		ovly->uvpitch = 0;
		break;
	case V4L2_PIX_FMT_RGB565X:
		/* most significant byte: RED; least significant byte: BLUE */
		reg |= (mi->panel_rbswap) ? (0) : CFG_DMA_SWAPRB(1);
		ovly->ypitch = pix->width << 1;
		ovly->uvpitch = 0;
		break;
	case V4L2_PIX_FMT_BGR24:
		reg |= CFG_DMAFORMAT(VMODE_RGB888PACKED);
		/* most significant byte: RED; least significant byte: BLUE */
		reg |= (mi->panel_rbswap) ? (0) : CFG_DMA_SWAPRB(1);
		ovly->ypitch = pix->width * 3;
		ovly->uvpitch = 0;
		break;
	case V4L2_PIX_FMT_RGB24:
		reg |= CFG_DMAFORMAT(VMODE_RGB888PACKED);
		/* most significant byte: BLUE; least significant byte: RED */
		reg |= (mi->panel_rbswap) ? CFG_DMA_SWAPRB(1) : (0);
		ovly->ypitch = pix->width * 3;
		ovly->uvpitch = 0;
		break;
	case V4L2_PIX_FMT_BGR32:
		reg |= CFG_DMAFORMAT(VMODE_RGBA888);
		/* most significant byte: RED; least significant byte: BLUE */
		reg |= (mi->panel_rbswap) ? (0) : (CFG_DMA_SWAPRB(1));
		ovly->ypitch = pix->width << 2;
		ovly->uvpitch = 0;
		break;
	case V4L2_PIX_FMT_RGB32:
		reg |= CFG_DMAFORMAT(VMODE_RGBA888);
		/* most significant byte: BLUE; least significant byte: RED */
		reg |= (mi->panel_rbswap) ? CFG_DMA_SWAPRB(1) : (0);
		ovly->ypitch = pix->width << 2;
		ovly->uvpitch = 0;
		break;
	}

	if (ovly->hdmi3d && 1 == ovly->id)
		reg |= CFG_DMA_FTOGGLE_MASK;
	else
		reg &= ~CFG_DMA_FTOGGLE_MASK;

	if (ovly->streaming)
		reg |= CFG_DMA_ENA_MASK;
	else
		reg &= ~CFG_DMA_ENA_MASK;

	ovly->dma_ctl0 = reg;

	win = &ovly->win;
	outw = win->w.width;
	outh = win->w.height;
	posx = win->w.left;
	posy = win->w.top;

	ret = mmpvid_setup_overlay(ovly, posx, posy, outw, outh);
	if (ret)
		goto err;

	return 0;
err:
	pr_err(VOUT_NAME "apply_changes failed\n");
	return ret;
}

int mmpvid_apply_changes(struct mmp_overlay *ovly)
{
	struct lcd_regs *regs;
	struct pxa168fb_vdma_info *lcd_vdma = 0;
	unsigned long addr_y0, addr_u0, addr_v0;
	unsigned long addr_y1 = 0, addr_u1 = 0, addr_v1 = 0;
	u32 bpp, offset;

	regs = get_regs(ovly->id);
	bpp = ovly->pix.bytesperline / ovly->pix.width;
	offset = ovly->crop.top * ovly->ypitch + ovly->crop.left * bpp;

	addr_y0 = ovly->paddr[0] + offset;
	writel(addr_y0, &regs->v_y0);
	if (ovly->hdmi3d)
		addr_y1 = ovly->paddr[1] + offset;
	if (ovly->uvpitch) {
		offset = ovly->crop.top * ovly->uvpitch +
			 ovly->crop.left * bpp;
		addr_u0 = ovly->paddr[0] + ovly->y_size + offset;
		addr_v0 = addr_u0 + ovly->uv_size;
		writel(addr_u0, &regs->v_u0);
		writel(addr_v0, &regs->v_v0);
	}

	if (ovly->hdmi3d) {
		writel(addr_y1, &regs->v_y1);
		if (ovly->uvpitch) {
			addr_u1 = ovly->paddr[1] + ovly->y_size + offset;
			addr_v1 = addr_u1 + ovly->uv_size;
			writel(addr_u1, &regs->v_u1);
			writel(addr_v1, &regs->v_v1);
		}
	}

	if (ovly->update) {
		writel(ovly->ypitch, &regs->v_pitch_yc);
		writel((ovly->uvpitch) << 16 | (ovly->uvpitch),
		       &regs->v_pitch_uv);

		writel(CFG_DMA_OVSA_VLN(ovly->pos_y) | ovly->pos_x,
		       &regs->v_start);
		writel((ovly->crop.height << 16) | ovly->crop.width,
			&regs->v_size);
		writel((ovly->out_height << 16) | ovly->out_width,
			 &regs->v_size_z);

		dma_ctrl_write(ovly->id, 0, ovly->dma_ctl0);
		ovly->update = 0;
	}

	lcd_vdma = request_vdma(ovly->id, 1);
	if (lcd_vdma) {
		vdma_info_update(lcd_vdma, ovly->opened,
			ovly->streaming, ovly->pix.pixelformat, 0, 0);
		pxa688_vdma_config(lcd_vdma);
	}
	if (vid_vsmooth)
		pxa688fb_vsmooth_set(ovly->id, 1, vid_vsmooth);

	return 0;
}

int mmp_ovly_set_colorkeyalpha(struct mmp_overlay *ovly)
{
	unsigned int rb, dma0, temp, x, layer, shift;
	struct pxa168fb_mach_info *mi;
	struct _sColorKeyNAlpha *color_a = &ovly->ckey_alpha;
	struct lcd_regs *regs;

	mi = ovly->dev->platform_data;
	regs = get_regs(ovly->id);
	shift = ovly->id ? 20 : 18;
	rb = layer = 0;
	dma0 = dma_ctrl_read(ovly->id, 0);
	x = dma_ctrl_read(ovly->id, 1) & ~(CFG_COLOR_KEY_MASK |
			CFG_ALPHA_MODE_MASK | CFG_ALPHA_MASK);
	/* switch to color key mode */
	switch (color_a->mode) {
	case FB_DISABLE_COLORKEY_MODE:
		/* do nothing */
		break;
	case FB_ENABLE_Y_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x1);
		break;
	case FB_ENABLE_U_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x2);
		break;
	case FB_ENABLE_V_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x4);
		break;
	case FB_ENABLE_RGB_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x3);
		rb = 1;
		break;
	case FB_ENABLE_R_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x1);
		rb = 1;
		break;
	case FB_ENABLE_G_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x6);
		break;
	case FB_ENABLE_B_COLORKEY_MODE:
		x |= CFG_COLOR_KEY_MODE(0x7);
		rb = 1;
		break;
	default:
		pr_info("unknown mode");
		return -1;
	}

	/* switch to alpha path selection and decide whether to do RB swap */
	switch (color_a->alphapath) {
	case FB_VID_PATH_ALPHA:
		x |= CFG_ALPHA_MODE(0x0);
		layer = CFG_CKEY_DMA;
		if (rb)
			rb = ((dma0 & CFG_DMA_SWAPRB_MASK) >> 4) ^
					(mi->panel_rbswap);
		break;
	case FB_GRA_PATH_ALPHA:
		x |= CFG_ALPHA_MODE(0x1);
		layer = CFG_CKEY_GRA;
		if (rb)
			rb = ((dma0 & CFG_GRA_SWAPRB_MASK) >> 12) ^
				 (mi->panel_rbswap);
		break;
	case FB_CONFIG_ALPHA:
		x |= CFG_ALPHA_MODE(0x2);
		rb = 0;
		break;
	default:
		pr_info("unknown alpha path");
		return -1;
	}

	/* check whether DMA turn on RB swap for this pixelformat. */
	if (rb) {
		if (color_a->mode == FB_ENABLE_R_COLORKEY_MODE) {
			x &= ~CFG_COLOR_KEY_MODE(0x1);
			x |= CFG_COLOR_KEY_MODE(0x7);
		}

		if (color_a->mode == FB_ENABLE_B_COLORKEY_MODE) {
			x &= ~CFG_COLOR_KEY_MODE(0x7);
			x |= CFG_COLOR_KEY_MODE(0x1);
		}

		/* exchange r b fields. */
		temp = color_a->Y_ColorAlpha;
		color_a->Y_ColorAlpha = color_a->V_ColorAlpha;
		color_a->V_ColorAlpha = temp;

		/* only alpha_Y take effect, switch back from V */
		if (color_a->mode == FB_ENABLE_RGB_COLORKEY_MODE) {
			color_a->Y_ColorAlpha &= 0xffffff00;
			temp = color_a->V_ColorAlpha & 0xff;
			color_a->Y_ColorAlpha |= temp;
		}
	}

	/* configure alpha */
	x |= CFG_ALPHA((color_a->config & 0xff));
	dma_ctrl_write(ovly->id, 1, x);
	writel(color_a->Y_ColorAlpha, &regs->v_colorkey_y);
	writel(color_a->U_ColorAlpha, &regs->v_colorkey_u);
	writel(color_a->V_ColorAlpha, &regs->v_colorkey_v);

	if (ovly->fbuf.flags & V4L2_FBUF_FLAG_GLOBAL_ALPHA)
		/* disable pix alpha mode because it was enabled by default */
		dma_ctrl_set(ovly->id, 0, CFG_NOBLENDING_MASK,
			CFG_NOBLENDING(1));
	else
		dma_ctrl_set(ovly->id, 0, CFG_NOBLENDING_MASK, 0);

	if (ovly->id != 2) {
		/* enable DMA colorkey on GRA/VID layer in panel/TV path */
		x = readl(ovly->reg_base + LCD_TV_CTRL1);
		x &= ~(3<<shift); x |= layer<<shift;
		writel(x, ovly->reg_base + LCD_TV_CTRL1);
	}

	return 0;
}

/* ------------------------------------------------------------------
* Video buffer call backs
* ------------------------------------------------------------------*/
static int queue_setup(struct vb2_queue *vq, const struct v4l2_format *fmt,
		unsigned int *nbuffers,	unsigned int *nplanes,
		unsigned int sizes[],	void *alloc_ctxs[])
{
	struct mmp_overlay *ovly = vb2_get_drv_priv(vq);
	u32 size;

	if (!ovly)
		return -EINVAL;

	/* *size = PAGE_ALIGN(ovly->pix.width * ovly->pix.height * 2); */
	size = PAGE_ALIGN(ovly->pix.sizeimage);
	/* Why need 3 */
	if (0 == *nbuffers)
		*nbuffers = 3;

	*nplanes = 1;
	sizes[0] = size;
	alloc_ctxs[0] = ovly->alloc_ctx;
	if (ovly->hdmi3d) {
		*nplanes = 2;
		sizes[1] = size;
		alloc_ctxs[1] = ovly->alloc_ctx;
	}

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: queue_setup, buffers=%d "
		"planes=%d, size=%d\n", ovly->id, *nbuffers, *nplanes, size);

	return 0;
}

static int buffer_init(struct vb2_buffer *vb)
{
	struct mmp_buf *buf = container_of(vb, struct mmp_buf, vb);

	INIT_LIST_HEAD(&buf->list);
	return 0;
}

/* This function will be called when VIDIOC_QBUF ioctl is called.
 * It prepare buffers before give out for the display. This function
 * user space virtual address into physical address if userptr memory
 * exchange mechanism is used.
 */
static int buffer_prepare(struct vb2_buffer *vb)
{
	struct mmp_overlay *ovly = vb2_get_drv_priv(vb->vb2_queue);
	int ret = 0;
	u32 size = PAGE_ALIGN(ovly->pix.sizeimage);

	if (vb2_plane_size(vb, 0) < size) {
		v4l2_dbg(1, debug, ovly->vdev, "ovly %d: %s data will not fit "
			"into plane (%lu < %u)\n", ovly->id, __func__,
			 vb2_plane_size(vb, 0), size);
		return -EINVAL;
	}
	vb2_set_plane_payload(vb, 0, size);

	if (!ovly->hdmi3d)
		return ret;

	if (vb2_plane_size(vb, 1) < size) {
		v4l2_dbg(1, debug, ovly->vdev, "ovly %d: %s data will not fit "
			"into plane (%lu < %u)\n", ovly->id, __func__,
			 vb2_plane_size(vb, 1), size);
		return -EINVAL;
	}
	vb2_set_plane_payload(vb, 1, size);

	return ret;
}

static int buffer_finish(struct vb2_buffer *vb)
{
	struct mmp_overlay *ovly = vb2_get_drv_priv(vb->vb2_queue);

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: %s\n", ovly->id, __func__);
	return 0;
}

/* Buffer queue funtion will be called from the videobuf layer when _QBUF
 * ioctl is called. It is used to enqueue buffer, which is ready to be
 * displayed. */
static void buffer_queue(struct vb2_buffer *vb)
{
	struct mmp_overlay *ovly = vb2_get_drv_priv(vb->vb2_queue);
	struct mmp_buf *buf = container_of(vb, struct mmp_buf, vb);
	unsigned long flags = 0;

	/* Driver is also maintainig a queue. So enqueue buffer in the driver
	 * queue */
	spin_lock_irqsave(&ovly->vbq_lock, flags);
	list_add_tail(&buf->list, &ovly->dma_queue);
	spin_unlock_irqrestore(&ovly->vbq_lock, flags);
}

/* Buffer cleanup funtion will be called from the videobuf layer
 * when REQBUFS(0) ioctl is called. It is used to cleanup the buffer
 * queue which driver maintaining. */
static void buffer_cleanup(struct vb2_buffer *vb)
{
	struct mmp_overlay *ovly = vb2_get_drv_priv(vb->vb2_queue);
	struct list_head *pos, *n;
	unsigned long flags = 0;

	/* Driver is also maintainig a queue. So clearup the driver queue */
	spin_lock_irqsave(&ovly->vbq_lock, flags);
	list_for_each_safe(pos, n, &ovly->dma_queue) {
		list_del(pos);
	}
	spin_unlock_irqrestore(&ovly->vbq_lock, flags);
}
static int start_streaming(struct vb2_queue *vq, unsigned int count)
{
	struct mmp_overlay *ovly = vb2_get_drv_priv(vq);

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d : %s\n", ovly->id, __func__);
	return 0;
}

/* abort streaming and wait for last buffer */
static int stop_streaming(struct vb2_queue *vq)
{
	struct mmp_overlay *ovly = vb2_get_drv_priv(vq);
	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: %s\n", ovly->id, __func__);
	return 0;
}

static void mmp_lock(struct vb2_queue *vq)
{
	struct mmp_overlay *ovly = vb2_get_drv_priv(vq);
	mutex_lock(&ovly->lock);
}

static void mmp_unlock(struct vb2_queue *vq)
{
	struct mmp_overlay *ovly = vb2_get_drv_priv(vq);
	mutex_unlock(&ovly->lock);
}

static struct vb2_ops video_vbq_ops = {
	.queue_setup            = queue_setup,
	.buf_init               = buffer_init,
	.buf_prepare            = buffer_prepare,
	.buf_finish             = buffer_finish,
	.buf_queue              = buffer_queue,
	.buf_cleanup		= buffer_cleanup,
	.start_streaming        = start_streaming,
	.stop_streaming         = stop_streaming,
	.wait_prepare           = mmp_unlock,
	.wait_finish            = mmp_lock,
};

/*
 *  File operations
 */
static int mmp_ovly_release(struct file *file)
{

	struct mmp_overlay *ovly = file->private_data;
	struct vb2_queue *q;
	unsigned int ret;
	unsigned long flags = 0;

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: Entering %s\n",
		 ovly->id, __func__);

	if (!ovly)
		return 0;
	q = &ovly->vbq;

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: vb2_is_streaming(q) %d\n",
		 ovly->id, vb2_is_streaming(q));
	spin_lock_irqsave(&ovly->vbq_lock, flags);
	if (ovly->streaming) {
		vb2_streamoff(q, BUF_TYPE);
		ovly->streaming = 0;
	}
	spin_unlock_irqrestore(&ovly->vbq_lock, flags);
	pxa688_vdma_release(ovly->id, 1);
	pxa688fb_vsmooth_set(ovly->id, 1, 0);

	/* Turn off the pipeline */
	ret = mmpvid_apply_changes(ovly);
	if (ret)
		pr_err(VOUT_NAME "Unable to apply changes\n");

	vb2_queue_release(q);
	vb2_dma_contig_cleanup_ctx(ovly->alloc_ctx);
	ovly->opened -= 1;
	file->private_data = NULL;

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: Exiting %s\n",
		 ovly->id, __func__);
	return ret;
}

static int mmp_ovly_open(struct file *file)
{
	struct mmp_overlay *ovly = NULL;
	struct vb2_queue *q;

	ovly = video_drvdata(file);
	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: Entering %s\n",
		ovly->id, __func__);

	if (ovly == NULL)
		return -ENODEV;

	/* for now, we only support single open */
	if (ovly->opened)
		return -EBUSY;

	ovly->opened += 1;

	file->private_data = ovly;
	ovly->type = BUF_TYPE;

	q = &ovly->vbq;
	memset(q, 0, sizeof(ovly->vbq));
	q->type = BUF_TYPE;
	q->io_modes = VB2_MMAP | VB2_USERPTR;
	q->drv_priv = ovly;
	q->buf_struct_size = sizeof(struct mmp_buf);
	q->ops = &video_vbq_ops;
	q->mem_ops = &vb2_dma_contig_memops;

	ovly->alloc_ctx = vb2_dma_contig_init_ctx(ovly->dev);
	if (IS_ERR(ovly->alloc_ctx))
		return PTR_ERR(ovly->alloc_ctx);

	vb2_queue_init(q);

	spin_lock_init(&ovly->vbq_lock);

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: Exiting %s\n",
		ovly->id, __func__);
	return 0;
}

static unsigned int mmp_poll(struct file *file,
				struct poll_table_struct *wait)
{
	struct mmp_overlay *ovly = file->private_data;
	struct vb2_queue *q = &ovly->vbq;

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: %s\n", ovly->id, __func__);

	/*
	if (BUF_TYPE != ovly->type)
		return POLLERR;
		*/

	return vb2_poll(q, file, wait);
}

static int video_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct mmp_overlay *ovly = file->private_data;

	return vb2_mmap(&ovly->vbq, vma);
}

/* V4L2 ioctls */
static int vidioc_querycap(struct file *file, void *fh,
			   struct v4l2_capability *cap)
{
	struct mmp_overlay *ovly = fh;

	strlcpy(cap->driver, VOUT_NAME, sizeof(cap->driver));
	strlcpy(cap->card, ovly->vdev->name, sizeof(cap->card));
	cap->bus_info[0] = '\0';
	cap->capabilities = V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT;
	return 0;
}

static int vidioc_enum_fmt_vid_out(struct file *file, void *fh,
				   struct v4l2_fmtdesc *fmt)
{
	int index = fmt->index;
	enum v4l2_buf_type type = fmt->type;

	fmt->index = index;
	fmt->type = type;
	if (index >= NUM_OUTPUT_FORMATS)
		return -EINVAL;

	fmt->flags = mmp_formats[index].flags;
	strlcpy(fmt->description, mmp_formats[index].description,
		sizeof(fmt->description));
	fmt->pixelformat = mmp_formats[index].pixelformat;
	return 0;
}

static int vidioc_g_fmt_vid_out(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mmp_overlay *ovly = fh;

	f->fmt.pix = ovly->pix;
	return 0;

}

int vidioc_g_fmt_vid_out_mplane(struct file *file, void *fh,
		struct v4l2_format *f)
{
	struct mmp_overlay *ovly = fh;

	pix_to_pixmp(&ovly->pix, &f->fmt.pix_mp);
	return 0;
}

static int vidioc_try_fmt_vid_out(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct mmp_overlay *ovly = fh;

	if (ovly->streaming)
		return -EBUSY;

	mmp_ovly_try_format(&f->fmt.pix);
	return 0;
}

static int vidioc_try_fmt_vid_out_mplane(struct file *file, void *fh,
				  struct v4l2_format *f)
{
	struct mmp_overlay *ovly = fh;

	if (ovly->streaming)
		return -EBUSY;

	if (2 != f->fmt.pix_mp.num_planes) {
		v4l2_dbg(1, debug, ovly->vdev, "Error! ovly %d: "
				"plane number %d is not supported\n",
				ovly->id, f->fmt.pix_mp.num_planes);
		return -EINVAL;
	}

	pixmp_to_pix(&ovly->pix, &f->fmt.pix_mp);
	mmp_ovly_try_format(&ovly->pix);
	return 0;
}

static int vidioc_s_fmt_vid_out_mplane(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct mmp_overlay *ovly = fh;
	struct vb2_queue *q = &ovly->vbq;
	spinlock_t *vbq_lock = &ovly->vbq_lock;
	struct lcd_regs *regs = get_regs(ovly->id);
	unsigned long flags = 0;
	unsigned int x;
	int ret = 0;

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: in %s\n", ovly->id, __func__);
	if (ovly->streaming) {
		v4l2_dbg(1, debug, ovly->vdev, "ovly %d: Error, %s "
				"device busy\n", ovly->id, __func__);
		return -EBUSY;
	}

	mutex_lock(&ovly->lock);
	spin_lock_irqsave(vbq_lock, flags);
	if (V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE == f->type) {
		ovly->hdmi3d = 1;
		q->type = ovly->type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
		pixmp_to_pix(&ovly->pix, &f->fmt.pix_mp);
	} else if (V4L2_BUF_TYPE_VIDEO_OUTPUT == f->type) {
		ovly->hdmi3d = 0;
		q->type = ovly->type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
		ovly->pix = f->fmt.pix;
	} else {
		v4l2_dbg(1, debug, ovly->vdev, "ovly %d: Invalid fmt %d\n",
			 ovly->id, f->type);
		ret = -EINVAL;
		goto s_fmt;
	}

	mmp_ovly_try_format(&ovly->pix);

	/* We rely on base layer screen size to get the LCD size */
	x = readl(&regs->screen_size);
	ovly->fbuf.fmt.height = x >> 16;
	ovly->fbuf.fmt.width = x & 0xffff;

	/* set default crop and win */
	mmp_ovly_new_format(&ovly->pix, &ovly->fbuf, &ovly->crop,
			       &ovly->win);

	/* Save the changes in the overlay strcuture and set controller */
	ret = mmpvid_init(ovly);
	if (ret) {
		pr_err(VOUT_NAME "failed to change mode\n");
		goto s_fmt;
	}
	pix_to_pixmp(&ovly->pix, &f->fmt.pix_mp);
s_fmt:
	spin_unlock_irqrestore(vbq_lock, flags);
	mutex_unlock(&ovly->lock);
	return ret;
}

static int vidioc_try_fmt_vid_overlay(struct file *file, void *fh,
				      struct v4l2_format *f)
{
	int err = -EINVAL;
	struct mmp_overlay *ovly = fh;
	struct v4l2_window *win = &f->fmt.win;

	err = mmp_ovly_try_window(&ovly->fbuf, win);

	if (err)
		return err;

	win->global_alpha = 255;

	return 0;
}

static int vidioc_s_fmt_vid_overlay(struct file *file, void *fh,
				    struct v4l2_format *f)
{
	struct mmp_overlay *ovly = fh;
	struct v4l2_window *win = &f->fmt.win;
	spinlock_t *vbq_lock = &ovly->vbq_lock;
	unsigned long flags = 0;
	int ret = 0;

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: in %s\n", ovly->id, __func__);

	mutex_lock(&ovly->lock);
	spin_lock_irqsave(vbq_lock, flags);

	ret = mmp_ovly_new_window(&ovly->crop, &ovly->win, &ovly->fbuf, win);
	if (ret)
		goto out;
	/* Save the changes in the overlay strcuture and set controller */
	ret = mmpvid_init(ovly);
	if (ret) {
		pr_err(VOUT_NAME "failed to change mode\n");
		goto out;
	}

	if (ovly->trans_enabled) {
		/* color key enabled */
		ovly->win.chromakey = f->fmt.win.chromakey;
		ovly->trans_key = ovly->win.chromakey;
		ovly->ckey_alpha.Y_ColorAlpha = ovly->win.chromakey;
		ovly->ckey_alpha.U_ColorAlpha = ovly->win.chromakey;
		ovly->ckey_alpha.V_ColorAlpha = ovly->win.chromakey;
		ovly->ckey_alpha.mode = FB_ENABLE_RGB_COLORKEY_MODE;
	} else {
		ovly->win.chromakey = 0;
		ovly->trans_key = ovly->win.chromakey;
		ovly->ckey_alpha.Y_ColorAlpha = 0;
		ovly->ckey_alpha.U_ColorAlpha = 0;
		ovly->ckey_alpha.V_ColorAlpha = 0;
		ovly->ckey_alpha.mode = FB_DISABLE_COLORKEY_MODE;
	}

	if (ovly->alpha_enabled) {
		/* pixel alpha or config alpha enabled */
		if (ovly->fbuf.flags & V4L2_FBUF_FLAG_GLOBAL_ALPHA) {
			ovly->win.global_alpha = f->fmt.win.global_alpha;
			ovly->ckey_alpha.config = ovly->win.global_alpha;
			if (ovly->ckey_alpha.mode == FB_DISABLE_COLORKEY_MODE)
				ovly->ckey_alpha.alphapath = FB_CONFIG_ALPHA;
			else
				ovly->ckey_alpha.alphapath = FB_GRA_PATH_ALPHA;
		} else
			/* pixel alpha enabled */
			ovly->ckey_alpha.alphapath = FB_GRA_PATH_ALPHA;
	} else {
		ovly->win.global_alpha = 0xff;
		ovly->ckey_alpha.config = ovly->win.global_alpha;
		ovly->ckey_alpha.alphapath = FB_GRA_PATH_ALPHA;
	}

	mmp_ovly_set_colorkeyalpha(ovly);
out:
	spin_unlock_irqrestore(vbq_lock, flags);
	mutex_unlock(&ovly->lock);
	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: chromakey %d, "
			"global_alpha %d\n", ovly->id, ovly->win.chromakey,
			ovly->win.global_alpha);

	return ret;
}

static int vidioc_enum_fmt_vid_overlay(struct file *file, void *fh,
				       struct v4l2_fmtdesc *fmt)
{
	int index = fmt->index;
	enum v4l2_buf_type type = fmt->type;

	fmt->index = index;
	fmt->type = type;
	if (index >= NUM_OUTPUT_FORMATS)
		return -EINVAL;

	fmt->flags = mmp_formats[index].flags;
	strlcpy(fmt->description, mmp_formats[index].description,
		sizeof(fmt->description));
	fmt->pixelformat = mmp_formats[index].pixelformat;
	return 0;
}

static int vidioc_g_fmt_vid_overlay(struct file *file, void *fh,
				    struct v4l2_format *f)
{
	struct mmp_overlay *ovly = fh;
	struct v4l2_window *win = &f->fmt.win;
	u32 key_value = 0;

	win->w = ovly->win.w;
	win->field = ovly->win.field;
	win->global_alpha = ovly->win.global_alpha;

	key_value = ovly->trans_key;
	/* win->chromakey = key_value; */
	win->chromakey = ovly->win.chromakey;
	return 0;
}

static int vidioc_cropcap(struct file *file, void *fh,
			  struct v4l2_cropcap *cropcap)
{
	struct mmp_overlay *ovly = fh;
	enum v4l2_buf_type type = cropcap->type;
	struct v4l2_pix_format *pix = &ovly->pix;

	cropcap->type = type;
	/*
	if (type != BUF_TYPE)
		return -EINVAL;
		*/

	/* Width and height are always even */
	cropcap->bounds.width = pix->width & ~1;
	cropcap->bounds.height = pix->height & ~1;

	mmp_ovly_default_crop(&ovly->pix, &ovly->fbuf, &cropcap->defrect);
	cropcap->pixelaspect.numerator = 1;
	cropcap->pixelaspect.denominator = 1;
	return 0;
}

static int vidioc_g_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct mmp_overlay *ovly = fh;

	/*
	if (crop->type != BUF_TYPE)
		return -EINVAL;
		*/
	crop->c = ovly->crop;
	return 0;
}

static int vidioc_s_crop(struct file *file, void *fh, struct v4l2_crop *crop)
{
	struct mmp_overlay *ovly = fh;
	struct mutex *ovly_lock = &ovly->lock;
	spinlock_t *vbq_lock = &ovly->vbq_lock;
	unsigned long flags = 0;
	int err = -EINVAL;

	if (ovly->streaming)
		return -EBUSY;

	mutex_lock(ovly_lock);

	/*if (crop->type == BUF_TYPE) {*/
	if (1) {
		spin_lock_irqsave(vbq_lock, flags);
		err = mmp_ovly_new_crop(&ovly->pix, &ovly->crop, &ovly->win,
					   &ovly->fbuf, &crop->c);
		if (!err)
			ovly->update = 1;
		spin_unlock_irqrestore(vbq_lock, flags);
		mutex_unlock(ovly_lock);
		return err;
	} else {
		mutex_unlock(ovly_lock);
		return -EINVAL;
	}
}

static int vidioc_queryctrl(struct file *file, void *fh,
			    struct v4l2_queryctrl *ctrl)
{
	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
	case V4L2_CID_HUE:
	case V4L2_CID_BRIGHTNESS:
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_GAMMA:

		v4l2_ctrl_query_fill(ctrl, 0, 0xFFFFFF, 1, 0);
		break;
	default:
		ctrl->name[0] = '\0';
		return -EINVAL;
	}
	return 0;
}

static int vidioc_g_ctrl(struct file *file, void *fh, struct v4l2_control *ctrl)
{
	/* struct mmp_overlay *ovly = fh; */

	switch (ctrl->id) {
	case V4L2_CID_VFLIP:
		{
			return 0;
		}

	default:
		return -EINVAL;
	}
}

static int vidioc_s_ctrl(struct file *file, void *fh, struct v4l2_control *a)
{
	struct mmp_overlay *ovly = fh;

	switch (a->id) {
	case V4L2_CID_VFLIP:
		{
			unsigned int color = a->value;

			mutex_lock(&ovly->lock);

			ovly->control[1].value = color;
			mutex_unlock(&ovly->lock);
			return 0;
		}

	default:
		return -EINVAL;
	}

}

static int vidioc_reqbufs(struct file *file, void *fh,
			  struct v4l2_requestbuffers *req)
{
	struct mmp_overlay *ovly = fh;
	struct vb2_queue *q = &ovly->vbq;
	unsigned long flags = 0;

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: %s\n", ovly->id, __func__);
	/* if memory is not mmp or userptr
	   return error */
	if ((V4L2_MEMORY_MMAP != req->memory) &&
	    (V4L2_MEMORY_USERPTR != req->memory))
		return -EINVAL;

	/*
	if ((req->type != BUF_TYPE) || (req->count < 0))
		return -EINVAL;
		*/
	spin_lock_irqsave(&ovly->vbq_lock, flags);
	INIT_LIST_HEAD(&ovly->dma_queue);
	spin_unlock_irqrestore(&ovly->vbq_lock, flags);

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: %s: vb2_reqbufs\n",
		 ovly->id, __func__);
	return vb2_reqbufs(q, req);
}

static int vidioc_querybuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct mmp_overlay *ovly = fh;

	return vb2_querybuf(&ovly->vbq, b);
}

static int vidioc_qbuf(struct file *file, void *fh, struct v4l2_buffer *buffer)
{
	struct mmp_overlay *ovly = fh;
	struct vb2_queue *q = &ovly->vbq;

	/*
	if (BUF_TYPE != buffer->type)
		return -EINVAL;
		*/

	if (V4L2_MEMORY_USERPTR == buffer->memory) {
		if (V4L2_BUF_TYPE_VIDEO_OUTPUT == q->type
				&& ((buffer->length < ovly->pix.sizeimage) ||
		    (0 == buffer->m.userptr))) {
			return -EINVAL;
		}
		if (V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE == q->type
			&& ((buffer->m.planes->length < ovly->pix.sizeimage) ||
		    (0 == buffer->m.userptr))) {
			return -EINVAL;
		}
		/* v4l2_dbg(1, debug, ovly->vdev, "qbuf id %d addr %p\n",
		 * buffer->index, buffer->m.userptr); */
	}

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: qbuf id %d vb %x\n", ovly->id,
			 buffer->index, (unsigned int)q->bufs[buffer->index]);

	return vb2_qbuf(q, buffer);
}

static int vidioc_dqbuf(struct file *file, void *fh, struct v4l2_buffer *b)
{
	struct mmp_overlay *ovly = fh;
	struct vb2_queue *q = &ovly->vbq;
	int ret = 0;

	if (!ovly->streaming)
		return -EINVAL;

	mutex_lock(&ovly->lock);
	if (file->f_flags & O_NONBLOCK)
		/* Call videobuf_dqbuf for non blocking mode */
		ret = vb2_dqbuf(q, (struct v4l2_buffer *) b, 1);
	else
		/* Call videobuf_dqbuf for  blocking mode */
		ret = vb2_dqbuf(q, (struct v4l2_buffer *) b, 0);
	if (!ret)
		v4l2_dbg(1, debug, ovly->vdev, "ovly %d: dqbuf id %d vb %x\n",
			ovly->id, b->index, (unsigned int)q->bufs[b->index]);
	mutex_unlock(&ovly->lock);
	return ret;
}

static int vidioc_streamon(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct mmp_overlay *ovly = fh;
	struct vb2_queue *q = &ovly->vbq;
	struct mutex *ovly_lock = &ovly->lock;
	spinlock_t *vbq_lock = &ovly->vbq_lock;
	unsigned long flags = 0;
	int ret = 0;

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: enter stream on!\n",
		 ovly->id);

	if (ovly->streaming)
		return -EBUSY;

	mutex_lock(ovly_lock);

	ret = vb2_streamon(q, i);
	if (ret < 0)
		goto stream_on;

	spin_lock_irqsave(vbq_lock, flags);
	if (list_empty(&ovly->dma_queue)) {
		ret = -EINVAL;
		goto out;
	}
	/* Get the next frame from the buffer queue */
	ovly->cur_frm = list_first_entry(&ovly->dma_queue,
			struct mmp_buf, list);
	/* Remove buffer from the buffer queue */
	list_del(&ovly->cur_frm->list);
#if 0
	/* Initialize field_id and started member */
	ovly->field_id = 0;
	/* set flag here. Next QBUF will start DMA */
#endif

	ovly->paddr[0] = *((dma_addr_t *) vb2_plane_cookie
				(&ovly->cur_frm->vb, 0));
	if (ovly->hdmi3d)
		ovly->paddr[1] = *((dma_addr_t *) vb2_plane_cookie
					(&ovly->cur_frm->vb, 1));
	/* First save the configuration in ovelray structure */
	ret = mmpvid_init(ovly);
	if (ret)
		pr_err(VOUT_NAME "failed to set overlay ovly\n");
	/* Enable the pipeline and set the Go bit */
	ret = mmpvid_apply_changes(ovly);
	if (ret)
		pr_err(VOUT_NAME "failed to change mode\n");

	ovly->streaming = 1;
	dma_ctrl_set(ovly->id, 0, CFG_DMA_ENA_MASK, CFG_DMA_ENA_MASK);
	pxa688fb_vsmooth_set(ovly->id, 1, vid_vsmooth);
out:
	spin_unlock_irqrestore(vbq_lock, flags);
stream_on:
	mutex_unlock(ovly_lock);
	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: leave stream on!\n",
		 ovly->id);
	return ret;
}

static int vidioc_streamoff(struct file *file, void *fh, enum v4l2_buf_type i)
{
	struct mmp_overlay *ovly = fh;
	spinlock_t *vbq_lock = &ovly->vbq_lock;
	struct mutex *ovly_lock = &ovly->lock;
	unsigned long flags = 0;

	if (!ovly->streaming)
		return -EINVAL;

	mutex_lock(ovly_lock);
	/* We have to hack in clone mode to avoid lock/unlock different ovly. */
	vb2_streamoff(&ovly->vbq, i);

	spin_lock_irqsave(vbq_lock, flags);
	ovly->streaming = 0;
	spin_unlock_irqrestore(vbq_lock, flags);
	pxa688_vdma_release(ovly->id, 1);
	pxa688fb_vsmooth_set(ovly->id, 1, 0);

	mutex_unlock(ovly_lock);
	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: stream off!\n", ovly->id);
	return 0;
}

static int vidioc_s_fbuf(struct file *file, void *fh,
			 struct v4l2_framebuffer *a)
{
	struct mmp_overlay *ovly = fh;
	int key_type = COLOR_KEY_GFX_DST;
	int enable = 0;

	/*
	 * Doesn't support the Destination color key
	 *  and alpha blending together ????
	 */
	if ((a->flags & V4L2_FBUF_FLAG_CHROMAKEY) &&
	    (a->flags & V4L2_FBUF_FLAG_LOCAL_ALPHA))
		return -EINVAL;

	/*
	 *if ((a->flags & V4L2_FBUF_FLAG_SRC_CHROMAKEY)) {
	 *  ovly->fbuf.flags |= V4L2_FBUF_FLAG_SRC_CHROMAKEY;
	 *  key_type =  COLOR_KEY_VID_SRC;
	 *  } else
	 *  ovly->fbuf.flags &= ~V4L2_FBUF_FLAG_SRC_CHROMAKEY;
	 */

	if ((a->flags & V4L2_FBUF_FLAG_CHROMAKEY)) {
		ovly->fbuf.flags |= V4L2_FBUF_FLAG_CHROMAKEY;
		key_type = COLOR_KEY_GFX_DST;
	} else
		ovly->fbuf.flags &= ~V4L2_FBUF_FLAG_CHROMAKEY;

	if (a->flags & V4L2_FBUF_FLAG_CHROMAKEY)
		enable = 1;
	else
		enable = 0;
	ovly->trans_enabled = enable;
	ovly->trans_key_type = key_type;

	if (a->flags & V4L2_FBUF_FLAG_LOCAL_ALPHA) {
		ovly->fbuf.flags |= V4L2_FBUF_FLAG_LOCAL_ALPHA;
		enable = 1;
	} else {
		ovly->fbuf.flags &= ~V4L2_FBUF_FLAG_LOCAL_ALPHA;
		enable = 0;
	}

	if (a->flags & V4L2_FBUF_FLAG_GLOBAL_ALPHA) {
		ovly->fbuf.flags |= V4L2_FBUF_FLAG_GLOBAL_ALPHA;
		enable = 1;
	} else {
		ovly->fbuf.flags &= ~V4L2_FBUF_FLAG_GLOBAL_ALPHA;
		enable = 0;
	}

	/* set par */
	ovly->alpha_enabled = enable;

	return 0;
}

static int vidioc_g_fbuf(struct file *file, void *fh,
			 struct v4l2_framebuffer *a)
{
	struct mmp_overlay *ovly = fh;

	a->flags = 0x0;

	a->capability = V4L2_FBUF_CAP_GLOBAL_ALPHA |
	    V4L2_FBUF_CAP_LOCAL_ALPHA | V4L2_FBUF_CAP_CHROMAKEY;

	/*
	 *if (ovly->trans_key_type == COLOR_KEY_VID_SRC)
	 *  a->flags |= V4L2_FBUF_FLAG_SRC_CHROMAKEY;
	 *  if (ovly->trans_key_type == COLOR_KEY_GFX_DST)
	 *  a->flags = V4L2_FBUF_FLAG_CHROMAKEY;

	 *  if (ovly->alpha_enabled)
	 */
	a->flags = ovly->fbuf.flags;

	return 0;
}

static const struct v4l2_ioctl_ops vout_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_fmt_vid_out = vidioc_enum_fmt_vid_out,
	.vidioc_enum_fmt_vid_out_mplane = vidioc_enum_fmt_vid_out,
	.vidioc_try_fmt_vid_out = vidioc_try_fmt_vid_out,
	.vidioc_try_fmt_vid_out_mplane = vidioc_try_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_out = vidioc_g_fmt_vid_out,
	.vidioc_g_fmt_vid_out_mplane = vidioc_g_fmt_vid_out_mplane,
	.vidioc_s_fmt_vid_out = vidioc_s_fmt_vid_out_mplane,
	.vidioc_s_fmt_vid_out_mplane = vidioc_s_fmt_vid_out_mplane,
	.vidioc_queryctrl = vidioc_queryctrl,
	.vidioc_g_ctrl = vidioc_g_ctrl,
	.vidioc_s_fbuf = vidioc_s_fbuf,
	.vidioc_g_fbuf = vidioc_g_fbuf,
	.vidioc_s_ctrl = vidioc_s_ctrl,
	.vidioc_try_fmt_vid_overlay = vidioc_try_fmt_vid_overlay,
	.vidioc_s_fmt_vid_overlay = vidioc_s_fmt_vid_overlay,
	.vidioc_enum_fmt_vid_overlay = vidioc_enum_fmt_vid_overlay,
	.vidioc_g_fmt_vid_overlay = vidioc_g_fmt_vid_overlay,
	.vidioc_cropcap = vidioc_cropcap,
	.vidioc_g_crop = vidioc_g_crop,
	.vidioc_s_crop = vidioc_s_crop,
	.vidioc_reqbufs = vidioc_reqbufs,
	.vidioc_querybuf = vidioc_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
};

static const struct v4l2_file_operations mmp_ovly_fops = {
	.owner = THIS_MODULE,
	.ioctl = video_ioctl2,
	.poll = mmp_poll,
	.mmap = video_mmap,
	.open = mmp_ovly_open,
	.release = mmp_ovly_release,
};

irqreturn_t mmp_v4l2_isr(int id)
{
	struct mmp_overlay *ovly = v4l2_ovly[id];
	u32 ret = 0;

	if (!ovly)
		return IRQ_NONE;

	if (!ovly->streaming) {
		dma_ctrl_set(ovly->id, 0, CFG_DMA_ENA_MASK, 0);
		return IRQ_NONE;
	}

	spin_lock(&ovly->vbq_lock);
	if (list_empty(&ovly->dma_queue)) {
		/*v4l2_dbg(2, debug, ovly->vdev, "ovly id: %d\n", id); */
		goto irq;
	}

	vb2_buffer_done(&ovly->cur_frm->vb, VB2_BUF_STATE_DONE);
	ovly->cur_frm = list_first_entry(&ovly->dma_queue,
			struct mmp_buf, list);

	list_del(&ovly->cur_frm->list);

	ovly->paddr[0] =
	 *((dma_addr_t *) vb2_plane_cookie(&ovly->cur_frm->vb, 0));
	if (ovly->hdmi3d)
		ovly->paddr[1] =
		*((dma_addr_t *) vb2_plane_cookie(&ovly->cur_frm->vb, 1));

	ret = mmpvid_apply_changes(ovly);
	if (ret)
		pr_err(VOUT_NAME "failed to change mode\n");

irq:
	spin_unlock(&ovly->vbq_lock);

	v4l2_dbg(1, debug, ovly->vdev, "ovly %d: buf %d is active! "
			"paddr[0] 0x%x\n", ovly->id,
			ovly->cur_frm->vb.v4l2_buf.index, ovly->paddr[0]);

	return IRQ_HANDLED;
}

/* Init functions used during driver intitalization */
/* Initial setup of video_data */
static int __init mmp_ovly_setup_video_data(struct mmp_overlay *ovly)
{
	struct v4l2_pix_format *pix;
	struct video_device *vdev;
	struct v4l2_control *control;
	struct lcd_regs *regs = get_regs(ovly->id);
	unsigned int x;

	/* set the default pix */
	pix = &ovly->pix;

	/* Set the default picture of QVGA  */
	pix->width = VID_QVGA_WIDTH;
	pix->height = VID_QVGA_HEIGHT;

	/* Default pixel format is RGB 5-6-5 */
	pix->pixelformat = V4L2_PIX_FMT_YUV420;
	pix->field = V4L2_FIELD_ANY;
	pix->bytesperline = (pix->width * 3) >> 1;
	pix->sizeimage = pix->bytesperline * pix->height;
	pix->priv = 0;
	pix->colorspace = V4L2_COLORSPACE_JPEG;

	/* We rely on base layer screen size to get the LCD size */
	x = readl(&regs->screen_size);
	ovly->fbuf.fmt.height = x >> 16;
	ovly->fbuf.fmt.width = x & 0xffff;

	/* Set the data structures for the overlay parameters */
	ovly->win.global_alpha = 255;
	ovly->fbuf.flags = 0;
	ovly->fbuf.capability = V4L2_FBUF_CAP_GLOBAL_ALPHA;
	ovly->win.chromakey = 0;
	INIT_LIST_HEAD(&ovly->dma_queue);

	mmp_ovly_new_format(pix, &ovly->fbuf, &ovly->crop, &ovly->win);

	/*Initialize the control variables for
	   and background color. */
	control = ovly->control;
	control[0].value = 0;
	ovly->control[2].value = 0;

	control[1].id = V4L2_CID_VFLIP;
	control[1].value = 0;

	/* initialize the video_device struct */
	vdev = ovly->vdev = video_device_alloc();

	if (!vdev) {
		pr_err(VOUT_NAME ": could not allocate video device struct\n");
		return -ENOMEM;
	}
	vdev->release = video_device_release;
	vdev->ioctl_ops = &vout_ioctl_ops;

	strlcpy(vdev->name, ovly->name, sizeof(vdev->name));
	vdev->vfl_type = VFL_TYPE_GRABBER;

	/* need to register for a VID_HARDWARE_* ID in videodev.h */
	vdev->fops = &mmp_ovly_fops;
	mutex_init(&ovly->lock);

	vdev->minor = -1;
	return 0;
}

/* Create video out devices and alloc buffer when boot up if needed */
static int __init mmp_ovly_create_video_devices(struct mmp_overlay *ovly)
{
	int ret = 0;
	struct video_device *vdev;

	/* Setup the default configuration for the video devices
	 */
	if (mmp_ovly_setup_video_data(ovly) != 0) {
		ret = -ENOMEM;
		goto error1;
	}

	vdev = ovly->vdev;
	video_set_drvdata(vdev, ovly);

	/* Register the Video device with V4L2
	 */
	if (video_register_device(vdev, VFL_TYPE_GRABBER, ovly->id + 1) < 0) {
		pr_err(VOUT_NAME ": could not register "
				"Video for Linux device\n");
		vdev->minor = -1;
		ret = -ENODEV;
		goto error2;
	}

	/* Configure the overlay structure */
	ret = mmpvid_init(ovly);
	if (ret)
		pr_err(VOUT_NAME "failed to set overlay ovly\n");

	/* Enable the pipeline and set the Go bit */
	ret = mmpvid_apply_changes(ovly);
	if (ret) {
		pr_err(VOUT_NAME "failed to change mode\n");
		goto error2;
	} else
		goto success;
error2:
	video_device_release(vdev);
error1:
	kfree(ovly);
	return ret;

success:
	pr_info(VOUT_NAME ": registered and initialized video%d: "
			"minor num %d [v4l2]\n", vdev->num, vdev->minor);

	return 0;
}

static void mmp_ovly_cleanup_device(struct mmp_overlay *ovly)
{
	struct video_device *vdev;

	if (!ovly)
		return;
	vdev = ovly->vdev;

	if (vdev) {
		if (vdev->minor == -1) {
			/*
			 * The device was never registered, so release the
			 * video_device struct directly.
			 */
			video_device_release(vdev);
		} else {
			/*
			 * The unregister function will release the video_device
			 * struct as well as unregistering it.
			 */
			video_unregister_device(vdev);
		}
	}

	kfree(ovly);
}

/* Driver functions */
static int mmp_ovly_remove(struct platform_device *pdev)
{
	struct mmp_overlay *ovly = platform_get_drvdata(pdev);

	mmp_ovly_cleanup_device(ovly);

	/* put_device(ovly); */
	kfree(ovly);
	return 0;
}

static int __devinit mmp_ovly_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mmp_overlay *ovly = NULL;
	struct pxa168fb_mach_info *mi;
	struct lcd_regs *regs;
	struct resource *res;
	struct pxa168fb_vdma_info *lcd_vdma = 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL)
		return -EINVAL;

	mi = pdev->dev.platform_data;
	if (mi == NULL)
		return -EINVAL;

	ovly = kzalloc(sizeof(struct mmp_overlay), GFP_KERNEL);
	if (ovly == NULL) {
		ret = -ENOMEM;
		return ret;
	}

	/* get LCD clock information. */
	ovly->clk = clk_get(&pdev->dev, "LCDCLK");

	ovly->id = pdev->id;
	ovly->dev = &pdev->dev;

	/*
	 * Map LCD controller registers.
	 */
	ovly->reg_base = devm_ioremap_nocache(&pdev->dev, res->start,\
			resource_size(res));
	if (ovly->reg_base == NULL) {
		dev_err(&pdev->dev, "no enough memory!\n");
		ret = -ENOMEM;
		goto error0;
	}

	/* init vdma clock/sram, etc. */
	lcd_vdma = request_vdma(ovly->id, 1);
	if (lcd_vdma) {
		lcd_vdma->dev = ovly->dev;
		lcd_vdma->reg_base = ovly->reg_base;
		pxa688_vdma_init(lcd_vdma);
	} else
		pr_warn("path %d video layer : request vdma fail\n", ovly->id);

	/*
	 * Configure default register values.
	 */
	regs = get_regs(ovly->id);
	writel(0, &regs->v_y1);
	writel(0, &regs->v_u1);
	writel(0, &regs->v_v1);
	writel(0, &regs->v_start);
	dma_ctrl_set(ovly->id, 0, CFG_DMA_ENA_MASK, 0);

	/*
	 * Get IRQ number.
	 */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "can't get irq resource!\n");
		ret = -EINVAL;
		goto error0;
	}
	ovly->name = mi->id;

	v4l2_ovly[pdev->id] = ovly;

	/* register v4l2 interface */
	ret = mmp_ovly_create_video_devices(ovly);
	if (ret)
		goto error0;

	v4l2_dbg(1, debug, ovly->vdev, "v4l2_ovly %d probed\n", ovly->id);

	return 0;

error0:
	dev_err(&pdev->dev, "mmp overlay device init failed with %d\n", ret);
	return ret;
}

static const struct platform_device_id mmpv4l2_id_table[] = {
	{"mmp-v4l2_ovly", 0},
};

static struct platform_driver mmp_ovly_driver = {
	.driver = {
		   .name = VOUT_NAME,
		   },
	.probe = mmp_ovly_probe,
	.remove = mmp_ovly_remove,
	.id_table = mmpv4l2_id_table,
};

module_platform_driver(mmp_ovly_driver);

MODULE_AUTHOR("Jun Nie");
MODULE_DESCRIPTION("MMP Video out driver");
MODULE_LICENSE("GPL");
