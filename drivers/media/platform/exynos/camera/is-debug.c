/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>

#if IS_ENABLED(CONFIG_DEBUG_SNAPSHOT)
#include <soc/samsung/debug-snapshot.h>
#endif

#include "is-config.h"
#include "is-debug.h"
#include "is-device-ischain.h"

struct is_debug is_debug;
EXPORT_SYMBOL(is_debug);

struct is_debug_event is_debug_event;

#define DEBUG_FS_ROOT_NAME	"is"
#define DEBUG_FS_LOGFILE_NAME	"isfw-msg"
#define DEBUG_FS_EVENT_DIR_NAME	"event"
#define DEBUG_FS_EVENT_FILTER "filter"
#define DEBUG_FS_EVENT_LOGFILE "log"
#define DEBUG_FS_EVENT_LOGEN "log_enable"

static const struct file_operations debug_log_fops;
static const struct file_operations debug_img_fops;
static const struct file_operations debug_event_fops;

#define DBG_DIGIT_CNT	5 /* Max count of total digit */
#define DBG_DIGIT_W	6
#define DBG_DIGIT_H	9

unsigned int dbg_draw_digit_ctrl;
module_param_named(draw_digit_ctrl, dbg_draw_digit_ctrl, uint, S_IRUGO | S_IWUSR);

/*
 * Decimal digit dot pattern.
 * 6 bits consist a single line of 6x9 digit pattern.
 *
 * e.g) Digit 2 pattern
 *    [5][4][3][2][1][0]
 * [0] 0  0  0  0  0  0 : 0x00
 * [1] 0  1  1  1  1  0 : 0x1E
 * [2] 0  0  0  0  1  0 : 0x02
 * [3] 0  0  0  0  1  0 : 0x02
 * [4] 0  1  1  1  1  0 : 0x1E
 * [5] 0  1  0  0  0  0 : 0x10
 * [6] 0  1  0  0  0  0 : 0x10
 * [7] 0  1  1  1  1  0 : 0x1E
 * [8] 0  0  0  0  0  0 : 0x00
 */
uint8_t is_digits[10][DBG_DIGIT_H] = {
	{0x00, 0x1E, 0x12, 0x12, 0x12, 0x12, 0x12, 0x1E, 0x00}, /* 0 */
	{0x00, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x02, 0x00}, /* 1 */
	{0x00, 0x1E, 0x02, 0x02, 0x1E, 0x10, 0x10, 0x1E, 0x00}, /* 2 */
	{0x00, 0x1E, 0x02, 0x02, 0x1E, 0x02, 0x02, 0x1E, 0x00}, /* 3 */
	{0x00, 0x12, 0x12, 0x12, 0x1E, 0x02, 0x02, 0x02, 0x00}, /* 4 */
	{0x00, 0x1E, 0x10, 0x10, 0x1E, 0x02, 0x02, 0x1E, 0x00}, /* 5 */
	{0x00, 0x1E, 0x10, 0x10, 0x1E, 0x12, 0x12, 0x1E, 0x00}, /* 6 */
	{0x00, 0x1E, 0x02, 0x02, 0x04, 0x08, 0x08, 0x08, 0x00}, /* 7 */
	{0x00, 0x1E, 0x12, 0x12, 0x1E, 0x12, 0x12, 0x1E, 0x00}, /* 8 */
	{0x00, 0x1E, 0x12, 0x12, 0x1E, 0x02, 0x02, 0x1E, 0x00}, /* 9 */
};

enum is_dot_align {
	IS_DOT_LEFT_TOP,
	IS_DOT_CENTER_TOP,
	IS_DOT_RIGHT_TOP,
	IS_DOT_LEFT_CENTER,
	IS_DOT_CENTER_CENTER,
	IS_DOT_RIGHT_CENTER,
	IS_DOT_LEFT_BOTTOM,
	IS_DOT_CENTER_BOTTOM,
	IS_DOT_RIGHT_BOTTOM,
	IS_DOT_ALIGN_MAX
};

enum is_dot_channel {
	IS_DOT_B,
	IS_DOT_W,
	IS_DOT_CH_MAX
};

/*
 * struct is_dot_info
 *
 * pixelperdot: num of pixel for a single dot
 * byteperdot: byte length of a single dot
 * scale: scaling factor for scarving digit on the image.
 *	This is applied for both direction & upscaling only.
 * align: digit alignment on the image coordination.
 * offset_y: offset for digit based on the align. -: up / +: down
 * pattern: byte pattern for a single dot.
 *	It has two channels, black & white.
 */
struct is_dot_info {
	uint8_t pixelperdot;
	uint8_t byteperdot;
	uint8_t scale;
	uint8_t align;
	int32_t offset_y;
	uint8_t pattern[IS_DOT_CH_MAX][16];
};

enum is_dot_format {
	IS_DOT_8BIT,
	IS_DOT_10BIT,
	IS_DOT_12BIT,
	IS_DOT_13BIT_S,
	IS_DOT_16BIT,
	IS_DOT_YUV,
	IS_DOT_YUYV,
	IS_DOT_FORMAT_MAX
};

static struct is_dot_info is_dot_infos[IS_DOT_FORMAT_MAX] = {
	/* IS_DOT_8BIT */
	{
		.pixelperdot = 1,
		.byteperdot = 1,
		.scale = 8,
		.align = IS_DOT_LEFT_TOP,
		.offset_y = 0,
		.pattern = {
			{0x00},
			{0xFF},
		},
	},
	/* IS_DOT_10BIT */
	{
	},
	/* IS_DOT_12BIT */
	{
		.pixelperdot = 2,
		.byteperdot = 3,
		.scale = 4,
		.align = IS_DOT_CENTER_CENTER,
		.offset_y = -2,
		.pattern = {
			{0x00, 0x00, 0x00},
			{0xFF, 0xFF, 0xFF},
		},
	},
	/* IS_DOT_13BIT_S */
	{
		.pixelperdot = 8,
		.byteperdot = 13,
		.scale = 3,
		.align = IS_DOT_CENTER_CENTER,
		.offset_y = -2,
		.pattern = {
			{0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
			{0xFF, 0xEF, 0xFF, 0xFD, 0xBF, 0xFF,
			0xF7, 0xFF, 0xFE, 0xDF, 0xFF, 0xFB, 0x7F},
		},
	},
	/* IS_DOT_16BIT */
	{
	},
	/* IS_DOT_YUV */
	{
		.pixelperdot = 1,
		.byteperdot = 1,
		.scale = 8,
		.align = IS_DOT_CENTER_CENTER,
		.offset_y = 2,
		.pattern = {
			{0x00},
			{0xFF},
		},
	},
	/* IS_DOT_YUYV */
	{
		.pixelperdot = 2,
		.byteperdot = 2,
		.scale = 4,
		.align = IS_DOT_CENTER_CENTER,
		.offset_y = 2,
		.pattern = {
			{0x00, 0x00},
			{0xFF, 0xFF},
		},
	},
};

static ulong is_get_carve_addr(ulong addr, struct is_dot_info *d_info,
		int num_length, int bytesperline, int height)
{
	ulong carve_addr = addr;
	u32 num_width, num_height, img_width, img_height;
	int offset_x, offset_y;

	/* dot domain */
	num_width = DBG_DIGIT_W * d_info->scale * num_length;
	num_height = DBG_DIGIT_H * d_info->scale * d_info->pixelperdot;
	img_width = bytesperline / d_info->byteperdot;
	img_height = height;

	switch (d_info->align) {
	case IS_DOT_CENTER_TOP:
		offset_x = (img_width >> 1) - (num_width >> 1);
		offset_y = 0;
		break;
	case IS_DOT_RIGHT_TOP:
		offset_x = img_width - num_width;
		offset_y = 0;
		break;
	case IS_DOT_LEFT_CENTER:
		offset_x = 0;
		offset_y = (img_height >> 1) - (num_height >> 1);
		break;
	case IS_DOT_CENTER_CENTER:
		offset_x = (img_width >> 1) - (num_width >> 1);
		offset_y = (img_height >> 1) - (num_height >> 1);
		break;
	case IS_DOT_RIGHT_CENTER:
		offset_x = img_width - num_width;
		offset_y = (img_height >> 1) - (num_height >> 1);
		break;
	case IS_DOT_LEFT_BOTTOM:
		offset_x = 0;
		offset_y = img_height - num_height - 1;
		break;
	case IS_DOT_CENTER_BOTTOM:
		offset_x = (img_width >> 1) - (num_width >> 1);
		offset_y = img_height - num_height - 1;
		break;
	case IS_DOT_RIGHT_BOTTOM:
		offset_x = img_width - num_width;
		offset_y = img_height - num_height - 1;
		break;
	case IS_DOT_LEFT_TOP:
	default:
		offset_x = 0;
		offset_y = 0;
		break;
	}

	offset_y += (d_info->offset_y * (int) num_height);

	if (offset_x < 0)
		offset_x = 0;
	if (offset_x + num_width > img_width)
		offset_x = img_width - num_width;
	if (offset_y < 0)
		offset_y = 0;
	if (offset_y + num_height > img_height)
		offset_y = img_height - num_height;

	/* byte domain */
	carve_addr = addr + (bytesperline * offset_y);
	carve_addr += (offset_x * d_info->byteperdot);

	return carve_addr;
}

static void is_carve_digit(int digit, struct is_dot_info *d_info,
			int bytesperline, ulong addr)
{
	uint8_t *digit_buf;
	int w, h, digit_w, digit_h, pos_w, pos_h, dot_ch, byteperdot;

	digit_buf = is_digits[digit];
	byteperdot = d_info->byteperdot * d_info->scale;
	digit_w = DBG_DIGIT_W * d_info->scale;
	digit_h = DBG_DIGIT_H * d_info->scale * d_info->pixelperdot;

	/* per-line of digit pattern */
	for (h = 0; h < digit_h; h++) {
		pos_h = h / (d_info->scale * d_info->pixelperdot);
		for (w = digit_w - 1; w >= 0; w--) {
			pos_w = w / d_info->scale;
			dot_ch = (digit_buf[pos_h] & (1 << pos_w)) ?
				IS_DOT_W : IS_DOT_B;
			memcpy((void *)addr, d_info->pattern[dot_ch],
				d_info->byteperdot);
			addr += d_info->byteperdot;
		}
		addr += (bytesperline - (byteperdot * DBG_DIGIT_W));
	}
}

static void is_carve_num(u32 num, u32 dotformat, u32 bytesperline,
		u32 height, ulong addr)
{
	struct is_dot_info *d_info;
	u8 num_buf[DBG_DIGIT_CNT] = { 0 };
	u8 num_length = 0;
	u32 digit_offset = 0;
	int i;

	d_info = &is_dot_infos[dotformat];

	for (i = 0; i < DBG_DIGIT_CNT && num; i++) {
		num_buf[i] = num % 10;
		num /= 10;
	}

	num_length = i;
	addr = is_get_carve_addr(addr, d_info, num_length,
			bytesperline, height);

	for (i = num_length - 1; i >= 0; i--) {
		is_carve_digit(num_buf[i], d_info, bytesperline, addr);
		digit_offset = d_info->byteperdot * d_info->scale * DBG_DIGIT_W;
		addr += digit_offset;
	}
}

void is_dbg_draw_digit(struct is_queue *queue, struct is_frame *frame,
		u64 digit, int unit)
{

	struct is_video_ctx *vctx;
	u8 i, bitsperpixel, dotformat;
	u32 j, img_w, img_h, flag_pixel_size, pixelformat, bytesperline;
	ulong addr = queue->buf_kva[frame->index][0];
	u64 none_img_id = BIT(ENTRY_IXV)
			| BIT(ENTRY_IXW)
			| BIT(ENTRY_ORBXC)
			| BIT(ENTRY_CLHC);

	if (!dbg_draw_digit_ctrl || !addr || (queue->id & none_img_id))
		return;

	vctx = container_of(queue, struct is_video_ctx, queue);

	img_w = (frame->width) ? frame->width : queue->framecfg.width;
	img_h = (frame->height) ? frame->height : queue->framecfg.height;

	flag_pixel_size = queue->framecfg.hw_pixeltype & PIXEL_TYPE_SIZE_MASK;
	bitsperpixel = queue->framecfg.format->bitsperpixel[0];
	pixelformat = queue->framecfg.format->pixelformat;

	switch (pixelformat) {
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
		dotformat = IS_DOT_YUYV;
		break;
	case V4L2_PIX_FMT_SBGGR12P:
		if (flag_pixel_size == CAMERA_PIXEL_SIZE_13BIT) {
			dotformat = IS_DOT_13BIT_S;
			bitsperpixel = 13;
		} else {
			dotformat = IS_DOT_12BIT;
		}
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
	case V4L2_PIX_FMT_NV16M:
	case V4L2_PIX_FMT_NV61M:
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
	case V4L2_PIX_FMT_YUV422P:
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		dotformat = IS_DOT_YUV;
		break;
	default:
		dotformat = IS_DOT_8BIT;
		break;
	}

	bytesperline = ALIGN(img_w * bitsperpixel / BITS_PER_BYTE, 16);

	for (i = 0, j = digit; i < frame->num_buffers; i++, j += unit)
		is_carve_num(j, dotformat, bytesperline, img_h, addr);
}

void is_dmsg_init(void)
{
	is_debug.dsentence_pos = 0;
	memset(is_debug.dsentence, 0x0, DEBUG_SENTENCE_MAX);
}

void is_dmsg_concate(const char *fmt, ...)
{
	va_list ap;
	char term[50];
	u32 copy_len;

	va_start(ap, fmt);
	vsnprintf(term, sizeof(term), fmt, ap);
	va_end(ap);

	copy_len = (u32)min((DEBUG_SENTENCE_MAX - is_debug.dsentence_pos), strlen(term));
	strncpy(is_debug.dsentence + is_debug.dsentence_pos, term, copy_len);
	is_debug.dsentence_pos += copy_len;
}

char *is_dmsg_print(void)
{
	return is_debug.dsentence;
}

void is_print_buffer(char *buffer, size_t len)
{
	u32 sentence_i;
	size_t read_cnt;
	char sentence[250];
	char letter;

	FIMC_BUG_VOID(!buffer);

	sentence_i = 0;

	for (read_cnt = 0; read_cnt < len; read_cnt++) {
		letter = buffer[read_cnt];
		if (letter) {
			sentence[sentence_i] = letter;
			if (sentence_i >= 247) {
				sentence[sentence_i + 1] = '\n';
				sentence[sentence_i + 2] = 0;
				printk(KERN_DEBUG "%s", sentence);
				sentence_i = 0;
			} else if (letter == '\n') {
				sentence[sentence_i + 1] = 0;
				printk(KERN_DEBUG "%s", sentence);
				sentence_i = 0;
			} else {
				sentence_i++;
			}
		}
	}
}

int is_debug_probe(void)
{
	is_debug.read_vptr = 0;
	is_debug.minfo = NULL;

	is_debug.dsentence_pos = 0;
	memset(is_debug.dsentence, 0x0, DEBUG_SENTENCE_MAX);


#ifdef ENABLE_DBG_FS
	is_debug.root = debugfs_create_dir(DEBUG_FS_ROOT_NAME, NULL);
	if (is_debug.root)
		probe_info("%s is created\n", DEBUG_FS_ROOT_NAME);

	is_debug.logfile = debugfs_create_file(DEBUG_FS_LOGFILE_NAME, S_IRUSR,
		is_debug.root, &is_debug, &debug_log_fops);
	if (is_debug.logfile)
		probe_info("%s is created\n", DEBUG_FS_LOGFILE_NAME);

	is_debug.event_dir = debugfs_create_dir(DEBUG_FS_EVENT_DIR_NAME, is_debug.root);
	debugfs_create_u32(DEBUG_FS_EVENT_FILTER, 0644, is_debug.event_dir,
							&is_debug_event.log_filter);
	is_debug_event.log = debugfs_create_file(DEBUG_FS_EVENT_LOGFILE, 0644,
		is_debug.event_dir,
		&is_debug_event,
		&debug_event_fops);
	debugfs_create_u32(DEBUG_FS_EVENT_LOGEN, 0644, is_debug.event_dir,
						&is_debug_event.log_enable);

	is_debug_event.log_enable = 1;
	is_debug_event.log_filter = (0x1 << IS_EVENT_ALL) - 1;

#ifdef ENABLE_DBG_EVENT_PRINT
	atomic_set(&is_debug_event.event_index, -1);
	atomic_set(&is_debug_event.critical_log_tail, -1);
	atomic_set(&is_debug_event.normal_log_tail, -1);
#endif
#endif
	clear_bit(IS_DEBUG_OPEN, &is_debug.state);

	return 0;
}

int is_debug_open(struct is_minfo *minfo)
{
	/*
	 * debug control should be reset on camera entrance
	 * because firmware doesn't update this area after reset
	 */
	*((int *)(minfo->kvaddr_debug_cnt)) = 0;
	is_debug.read_vptr = 0;
	is_debug.minfo = minfo;

	set_bit(IS_DEBUG_OPEN, &is_debug.state);

	return 0;
}

int is_debug_close(void)
{
	clear_bit(IS_DEBUG_OPEN, &is_debug.state);

	return 0;
}

/**
  * is_debug_dma_dump: dump buffer by is_queue.
  *                         should be enable DBG_IMAGE_KMAPPING for kernel addr
  * @queue: buffer info
  * @index: buffer index
  * @vid: video node id for filename
  * @type: enum dbg_dma_dump_type
  **/
int is_dbg_dma_dump(struct is_queue *queue, u32 instance, u32 index, u32 vid, u32 type)
{
	int i;
	int ret;
	u32 flags;
	char *filename;
	struct is_binary bin;
	struct is_frame *frame = &queue->framemgr.frames[index];
	u32 region_id = 0;
	int total_size;
	struct vb2_buffer *buf = queue->vbq->bufs[index];
	u32 framecount = frame->fcount;

	switch (type) {
	case DBG_DMA_DUMP_IMAGE:
		/* Dump each region */
		do {
			filename = __getname();
			if (unlikely(!filename))
				return -ENOMEM;

			snprintf(filename, PATH_MAX, "%s/V%02d_F%08d_I%02d_R%d.raw",
					DBG_DMA_DUMP_PATH, vid, framecount, instance, region_id);

			/* Dump each plane */
			for (i = 0; i < (buf->num_planes - 1); i++) {
				if (frame->stripe_info.region_num) {
					bin.data = (void *)frame->stripe_info.kva[region_id][i];
					bin.size = frame->stripe_info.size[region_id][i];
				} else {
					bin.data = (void *)queue->buf_kva[index][i];
					bin.size = queue->framecfg.size[i];
				}

				if (!bin.data) {
					err("[V%d][F%d][I%d][R%d] kva is NULL\n",
							vid, frame->fcount, index, region_id);
					__putname(filename);
					return -EINVAL;
				}

				if (i == 0) {
					/* first plane for image */
					flags = O_TRUNC | O_CREAT | O_EXCL | O_WRONLY | O_APPEND;
					total_size = bin.size;
				} else {
					/* after first plane for image */
					flags = O_WRONLY | O_APPEND;
					total_size += bin.size;
				}

				ret = put_filesystem_binary(filename, &bin, flags);
				if (ret) {
					err("failed to dump %s (%d)", filename, ret);
					__putname(filename);
					return -EINVAL;
				}
			}

			info("[V%d][F%d][I%d][R%d] img dumped..(%s, %d)\n",
					vid, frame->fcount, index, region_id, filename, total_size);

			__putname(filename);
		} while (++region_id < frame->stripe_info.region_num);

		break;
	case DBG_DMA_DUMP_META:
		filename = __getname();
		if (unlikely(!filename))
			return -ENOMEM;

		snprintf(filename, PATH_MAX, "%s/V%02d_F%08d_I%02d.meta",
				DBG_DMA_DUMP_PATH, vid, frame->fcount, index);

		bin.data = (void *)queue->buf_kva[index][buf->num_planes - 1];
		bin.size = queue->framecfg.size[buf->num_planes - 1];

		/* last plane for meta */
		flags = O_TRUNC | O_CREAT | O_EXCL | O_WRONLY;
		total_size = bin.size;

		ret = put_filesystem_binary(filename, &bin, flags);
		if (ret) {
			err("failed to dump %s (%d)", filename, ret);
			__putname(filename);
			return -EINVAL;
		}

		info("[V%d][F%d] meta dumped..(%s, %d)\n", vid, frame->fcount, filename, total_size);
		__putname(filename);

		break;
	default:
		err("invalid type(%d)", type);
		break;
	}

	return 0;
}

int is_dbg_dma_dump_by_frame(struct is_frame *frame, u32 vid, u32 type)
{
	int i = 0;
	int ret = 0;
	u32 flags = 0;
	int total_size = 0;
	u32 framecount = frame->fcount;
	char *filename;
	struct is_binary bin;

	switch (type) {
	case DBG_DMA_DUMP_IMAGE:
		filename = __getname();

		if (unlikely(!filename))
			return -ENOMEM;

		snprintf(filename, PATH_MAX, "%s/V%02d_F%08d.raw",
				DBG_DMA_DUMP_PATH, vid, framecount);

		for (i = 0; i < (frame->planes / frame->num_buffers); i++) {
			bin.data = (void *)frame->kvaddr_buffer[i];
			bin.size = frame->size[i];

			if (!i) {
				/* first plane for image */
				flags = O_TRUNC | O_CREAT | O_EXCL | O_WRONLY | O_APPEND;
				total_size += bin.size;
			} else {
				/* after first plane for image */
				flags = O_WRONLY | O_APPEND;
				total_size += bin.size;
			}

			ret = put_filesystem_binary(filename, &bin, flags);
			if (ret) {
				err("failed to dump %s (%d)", filename, ret);
				__putname(filename);
				return -EINVAL;
			}
		}

		info("[V%d][F%d] img dumped..(%s, %d)\n", vid, framecount, filename, total_size);
		__putname(filename);

		break;
	default:
		err("invalid type(%d)", type);
		break;
	}

	return 0;
}

static int islib_debug_open(struct inode *inode, struct file *file)
{
	if (inode->i_private)
		file->private_data = inode->i_private;

	return 0;
}

static ssize_t islib_debug_read(struct file *file, char __user *user_buf,
	size_t buf_len, loff_t *ppos)
{
	int ret = 0;
	void *read_ptr;
	size_t write_vptr, read_vptr, buf_vptr;
	size_t read_cnt, read_cnt1, read_cnt2;
	struct is_minfo *minfo;

	if (!test_bit(IS_DEBUG_OPEN, &is_debug.state))
		return 0;

	minfo = is_debug.minfo;

	write_vptr = *((int *)(minfo->kvaddr_debug_cnt));
	read_vptr = is_debug.read_vptr;
	buf_vptr = buf_len;

	if (write_vptr >= read_vptr) {
		read_cnt1 = write_vptr - read_vptr;
		read_cnt2 = 0;
	} else {
		read_cnt1 = DEBUG_REGION_SIZE - read_vptr;
		read_cnt2 = write_vptr;
	}

	if (buf_vptr && read_cnt1) {
		read_ptr = (void *)(minfo->kvaddr_debug + is_debug.read_vptr);

		if (read_cnt1 > buf_vptr)
			read_cnt1 = buf_vptr;

		ret = copy_to_user(user_buf, read_ptr, read_cnt1);
		if (ret) {
			err("[DBG] failed copying %d bytes of debug log\n", ret);
			return ret;
		}
		is_debug.read_vptr += read_cnt1;
		buf_vptr -= read_cnt1;
	}

	if (is_debug.read_vptr >= DEBUG_REGION_SIZE) {
		if (is_debug.read_vptr > DEBUG_REGION_SIZE)
			err("[DBG] read_vptr(%zd) is invalid", is_debug.read_vptr);
		is_debug.read_vptr = 0;
	}

	if (buf_vptr && read_cnt2) {
		read_ptr = (void *)(minfo->kvaddr_debug + is_debug.read_vptr);

		if (read_cnt2 > buf_vptr)
			read_cnt2 = buf_vptr;

		ret = copy_to_user(user_buf, read_ptr, read_cnt2);
		if (ret) {
			err("[DBG] failed copying %d bytes of debug log\n", ret);
			return ret;
		}
		is_debug.read_vptr += read_cnt2;
		buf_vptr -= read_cnt2;
	}

	read_cnt = buf_len - buf_vptr;

	/* info("[DBG] FW_READ : read_vptr(%zd), write_vptr(%zd) - dump(%zd)\n", read_vptr, write_vptr, read_cnt); */

	return read_cnt;
}

static const struct file_operations debug_log_fops = {
	.open	= islib_debug_open,
	.read	= islib_debug_read,
	.llseek	= default_llseek
};

struct seq_file *seq_f;

/* this func for print seq_msg
in is_debug_event_add(callfunc)  */
void is_dbg_print(char *fmt, ...)
{
	char buf[512] = {0, };
	va_list ap;

	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	seq_printf(seq_f, buf);
}

static int is_event_show(struct seq_file *s, void *unused)
{
	struct is_debug_event *event_log = s->private;

	is_debug_info_dump(s, event_log);

	return 0;
}

static int is_debug_event_open(struct inode *inode, struct file *file)
{
	return single_open(file, is_event_show, inode->i_private);
}

#ifdef ENABLE_DBG_EVENT_PRINT
void is_debug_event_print(is_event_store_type_t event_store_type,
	void (*callfunc)(void *),
	void *ptrdata,
	size_t datasize,
	const char *fmt, ...)
{
	int index;
	struct is_debug_event_log *event_log;
	unsigned int log_num;
	va_list args;

	if (is_debug_event.log_enable == 0)
		return;

	switch (event_store_type) {
	case IS_EVENT_CRITICAL:
		index = atomic_inc_return(&is_debug_event.critical_log_tail);
		if (index >= IS_EVENT_MAX_NUM - 1) {
			if (index % LOG_INTERVAL_OF_WARN == 0)
				warn("critical event log buffer full...!");

			return;
		}
		event_log = &is_debug_event.event_log_critical[index];
		break;
	case IS_EVENT_NORMAL:
		index = atomic_inc_return(&is_debug_event.normal_log_tail) &
				(IS_EVENT_MAX_NUM - 1);

		event_log = &is_debug_event.event_log_normal[index];
		break;
	default:
		warn("invalid event type(%d)", event_store_type);
		goto p_err;
	}

	log_num = atomic_inc_return(&is_debug_event.event_index);

	event_log->time = ktime_get();
	va_start(args, fmt);
	vsnprintf(event_log->dbg_msg, sizeof(event_log->dbg_msg), fmt, args);
	va_end(args);
	event_log->log_num = log_num;
	event_log->event_store_type = event_store_type;
	event_log->callfunc = callfunc;
	event_log->cpu = raw_smp_processor_id();

	/* ptrdata should be used in non-atomic context */
	if (!in_atomic()) {
		if (event_log->ptrdata) {
			vfree(event_log->ptrdata);
			event_log->ptrdata = NULL;
		}

		if (datasize) {
			event_log->ptrdata = vmalloc(datasize);
			if (event_log->ptrdata)
				memcpy(event_log->ptrdata, ptrdata, datasize);
			else
				warn("couldn't allocate ptrdata");
		}
	}

p_err:
	return;
}
#endif

void is_debug_event_count(is_event_store_type_t event_store_type)
{

	switch (event_store_type) {
	case IS_EVENT_OVERFLOW_CSI:
		atomic_inc(&is_debug_event.overflow_csi);
		break;
	case IS_EVENT_OVERFLOW_3AA:
		atomic_inc(&is_debug_event.overflow_3aa);
		break;
	default:
		warn("invalid event type(%d)", event_store_type);
		goto p_err;
	}

p_err:
	return;
}

int is_debug_info_dump(struct seq_file *s, struct is_debug_event *debug_event)
{
	seq_f = s;
	seq_printf(s, "------------------- FIMC-IS EVENT LOGGER - START --------------\n");

#ifdef ENABLE_DBG_EVENT_PRINT
	{
		int index_normal = 0;
		int latest_normal = atomic_read(&is_debug_event.normal_log_tail);
		int index_critical = 0;
		int latest_critical = atomic_read(&is_debug_event.critical_log_tail);
		bool normal_done = 0;
		bool critical_done = 0;
		struct timespec64 tv;
		struct is_debug_event_log *log_critical;
		struct is_debug_event_log *log_normal;
		struct is_debug_event_log *log_print;

		if ((latest_normal < 0) || !(is_debug_event.log_filter & IS_EVENT_NORMAL))
			normal_done = 1; /* normal log empty */
		else if (latest_normal > IS_EVENT_MAX_NUM - 1)
			index_normal = (latest_normal % IS_EVENT_MAX_NUM) + 1;
		else
			index_normal = 0;

		latest_normal = latest_normal % IS_EVENT_MAX_NUM;

		if ((latest_critical < 0) || !(is_debug_event.log_filter & IS_EVENT_CRITICAL))
			critical_done = 1; /* critical log empty */

		while (!(normal_done) || !(critical_done)) {
			index_normal = index_normal % IS_EVENT_MAX_NUM;
			index_critical = index_critical % IS_EVENT_MAX_NUM;
			log_normal = &is_debug_event.event_log_normal[index_normal];
			log_critical = &is_debug_event.event_log_critical[index_critical];

			if (!normal_done && !critical_done) {
				if (log_normal->log_num < log_critical->log_num) {
					log_print = log_normal;
					index_normal++;
				} else {
					log_print = log_critical;
					index_critical++;
				}
			} else if (!normal_done) {
				log_print = log_normal;
				index_normal++;
			} else if (!critical_done) {
				log_print = log_critical;
				index_critical++;
			}

			if (latest_normal == index_normal)
				normal_done = 1;

			if (latest_critical == index_critical)
				critical_done = 1;

			tv = ktime_to_timespec64(log_print->time);
			seq_printf(s, "[%d][%6ld.%06ld] num(%d) ", log_print->cpu,
				tv.tv_sec, tv.tv_nsec / 1000, log_print->log_num);
			seq_printf(s, "%s\n", log_print->dbg_msg);

			if (log_print->callfunc != NULL)
				log_print->callfunc(log_print->ptrdata);

		}
	}
#endif

	seq_printf(s, "overflow: csi(%d), 3aa(%d)\n",
		atomic_read(&debug_event->overflow_csi),
		atomic_read(&debug_event->overflow_3aa));

	seq_printf(s, "------------------- FIMC-IS EVENT LOGGER - END ----------------\n");
	return 0;
}

static const struct file_operations debug_event_fops = {
	.open = is_debug_event_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

void is_debug_s2d(bool en_s2d, const char *fmt, ...)
{
	static char buf[1024];
	va_list args;

	va_start(args, fmt);
	vsnprintf(buf, sizeof(buf), fmt, args);
	va_end(args);

	if (en_s2d || debug_s2d) {
		err("[DBG] S2D!!!", buf);
		dump_stack();
#if defined(CONFIG_S3C2410_WATCHDOG)
		s3c2410wdt_set_emergency_reset(100, 0);
#elif defined(CONFIG_DEBUG_SNAPSHOT_MODULE)
		dbg_snapshot_expire_watchdog();
#else
		panic("S2D is not enabled.", buf);
#endif
	} else {
		panic(buf);
	}
}
