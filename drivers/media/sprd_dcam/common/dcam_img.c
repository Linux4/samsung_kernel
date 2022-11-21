/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/kthread.h>
#include <linux/highmem.h>
#include <linux/freezer.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#ifndef CONFIG_64BIT
#include <soc/sprd/board.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/adi.h>
#endif
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/of.h>
#include <linux/of_device.h>

#include <linux/vmalloc.h>
#include "dcam_drv.h"

#include <asm/io.h>


#include <../flash/flash.h>
#include <video/sprd_img.h>

#include "../../sprd_sensor/csi2/csi_api.h"

//#define LOCAL   static
#define LOCAL

#define IMG_DEVICE_NAME                         "sprd_image"
#define IMAGE_MINOR                             MISC_DYNAMIC_MINOR
#define DCAM_INVALID_FOURCC                     0xFFFFFFFF
#define DCAM_MAJOR_VERSION                      1
#define DCAM_MINOR_VERSION                      0
#define DCAM_RELEASE                            0
#define DCAM_QUEUE_LENGTH                       16
#define DCAM_TIMING_LEN                         16
#if CONFIG_SC_FPGA
#define DCAM_TIMEOUT                            (2000*100)
#else
#define DCAM_TIMEOUT                            1500
#endif

#define DCAM_ZOOM_LEVEL_MAX                     4
#define DCAM_ZOOM_STEP(x, y)                   (((x) - (y)) / DCAM_ZOOM_LEVEL_MAX)
#define DCAM_PIXEL_ALIGNED                         4
#define DCAM_WIDTH(w)                          ((w)& ~(DCAM_PIXEL_ALIGNED - 1))
#define DCAM_HEIGHT(h)                          ((h)& ~(DCAM_PIXEL_ALIGNED - 1))

#define DEBUG_STR                               "Error L %d, %s: \n"
#define DEBUG_ARGS                              __LINE__,__FUNCTION__
#define IMG_RTN_IF_ERR(n)          \
	do {                        \
		if(unlikely(n)) {                \
			printk(DEBUG_STR,DEBUG_ARGS);            \
			goto exit;       \
		}                        \
	} while(0)

#define IMG_PRINT_IF_ERR(n)          \
	do {                        \
		if(unlikely(n)) {                \
			printk(DEBUG_STR,DEBUG_ARGS);            \
		}                        \
	} while(0)

#define DCAM_VERSION \
	KERNEL_VERSION(DCAM_MAJOR_VERSION, DCAM_MINOR_VERSION, DCAM_RELEASE)

typedef int (*path_cfg_func)(enum dcam_cfg_id, void *);

enum
{
	PATH_IDLE  = 0x00,
	PATH_RUN,
};

struct dcam_format {
	char                       *name;
	uint32_t                   fourcc;
	int                        depth;
};

struct dcam_node {
	uint32_t                   irq_flag;
	uint32_t                   f_type;
	uint32_t                   index;
	uint32_t                   width;
	uint32_t                   height;
	uint32_t                   yaddr;
	uint32_t                   uaddr;
	uint32_t                   vaddr;
	uint32_t                   yaddr_vir;
	uint32_t                   uaddr_vir;
	uint32_t                   vaddr_vir;
	uint32_t                   invalid_flag;
	struct timeval         time;
	uint32_t                   reserved;
};

struct dcam_queue {
	struct dcam_node           node[DCAM_QUEUE_LENGTH];
	struct dcam_node           *write;
	struct dcam_node           *read;
	uint32_t                           wcnt;
	uint32_t                           rcnt;
};

struct dcam_img_buf_addr {
	struct dcam_addr           frm_addr;
	struct dcam_addr           frm_addr_vir;
};

struct dcam_img_buf_queue {
	struct dcam_img_buf_addr   buf_addr[DCAM_FRM_CNT_MAX];
	struct dcam_img_buf_addr   *write;
	struct dcam_img_buf_addr   *read;
	uint32_t                   wcnt;
	uint32_t                   rcnt;
};

struct dcam_path_spec {
	uint32_t                   is_work;
	uint32_t                   is_from_isp;
	uint32_t		   rot_mode;
	uint32_t                   status;
	struct dcam_size           in_size;
	struct dcam_path_dec       img_deci;
	struct dcam_rect           in_rect;
	struct dcam_rect           in_rect_current;
	struct dcam_rect           in_rect_backup;
	struct dcam_size           out_size;
	enum   dcam_fmt            out_fmt;
	struct dcam_endian_sel     end_sel;
	uint32_t                   fourcc;
	uint32_t                   pixel_depth;
	uint32_t                   frm_id_base;
	uint32_t                   frm_type;
	uint32_t                   index[DCAM_FRM_CNT_MAX];
	//struct dcam_addr           frm_addr[DCAM_FRM_CNT_MAX];
	//struct dcam_addr           frm_addr_vir[DCAM_FRM_CNT_MAX];
	struct dcam_img_buf_queue  buf_queue;
	struct dcam_addr           frm_reserved_addr;
	struct dcam_addr           frm_reserved_addr_vir;
	struct dcam_frame          *frm_ptr[DCAM_FRM_CNT_MAX];
	uint32_t                   frm_cnt_act;
	uint32_t                   path_frm_deci;
	uint32_t                   shrink;
};

struct dcam_info {
	uint32_t                   if_mode;
	uint32_t                   sn_mode;
	uint32_t                   yuv_ptn;
	uint32_t                   data_bits;
	uint32_t                   is_loose;
	uint32_t                   lane_num;
	uint32_t                   pclk;
	struct dcam_cap_sync_pol   sync_pol;
	uint32_t                   frm_deci;
	struct dcam_cap_dec        img_deci;
	struct dcam_size           cap_in_size;
	struct dcam_rect           cap_in_rect;
	struct dcam_size           cap_out_size;
	struct dcam_size           dst_size;
	uint32_t                   pxl_fmt;
	uint32_t                   need_isp_tool;
	uint32_t                   need_isp;
	uint32_t                   need_shrink;
	struct dcam_rect           path_input_rect;

	struct dcam_path_spec      dcam_path[DCAM_PATH_NUM];

	uint32_t                   capture_mode;
	uint32_t                   skip_number;
	uint32_t                   flash_status;
	uint32_t                   after_af;
	uint32_t                   is_smooth_zoom;
	struct timeval             timestamp;
	uint32_t                   camera_id;
};

struct dcam_dev {
	struct mutex             dcam_mutex;
	struct semaphore         irq_sem;
	atomic_t                 users;
	struct dcam_info         dcam_cxt;
	atomic_t                 stream_on;
	uint32_t                 stream_mode;
	struct dcam_queue        queue;
	struct timer_list        dcam_timer;
	atomic_t                 run_flag;
	uint32_t                 got_resizer;
	struct proc_dir_entry*   proc_file;
	struct semaphore         flash_thread_sem;
	struct task_struct*      flash_thread;
	uint32_t                 is_flash_thread_stop;
	uint32_t                 frame_skipped;
	struct semaphore         zoom_thread_sem;
	struct task_struct*      zoom_thread;
	uint32_t                 is_zoom_thread_stop;
	uint32_t                 zoom_level;
	uint32_t                 channel_id;
	void                     *driver_data;
};

#ifndef __SIMULATOR__
LOCAL int sprd_img_tx_done(struct dcam_frame *frame, void* param);
LOCAL int sprd_img_tx_error(struct dcam_frame *frame, void* param);
LOCAL int sprd_img_no_mem(struct dcam_frame *frame, void* param);
LOCAL int sprd_img_queue_write(struct dcam_queue *queue, struct dcam_node *node);
LOCAL int sprd_img_queue_read(struct dcam_queue *queue, struct dcam_node *node);
LOCAL int sprd_img_queue_disable(struct dcam_queue *queue, uint32_t channel_id);
LOCAL int sprd_img_queue_enable(struct dcam_queue *queue, uint32_t channel_id);
LOCAL int sprd_img_buf_queue_init(struct dcam_img_buf_queue *queue);
LOCAL int sprd_img_queue_init(struct dcam_queue *queue);
LOCAL int sprd_img_buf_queue_write(struct dcam_img_buf_queue *queue, struct dcam_img_buf_addr *buf_addr);
LOCAL int sprd_img_buf_queue_read(struct dcam_img_buf_queue *queue, struct dcam_img_buf_addr *buf_addr);
LOCAL int sprd_start_timer(struct timer_list *dcam_timer, uint32_t time_val);
LOCAL int sprd_stop_timer(struct timer_list *dcam_timer);
LOCAL int sprd_img_streampause(struct file *file, uint32_t channel_id, uint32_t reconfig_flag);
LOCAL int sprd_img_streamresume(struct file *file, uint32_t channel_id);
LOCAL int sprd_img_reg_isr(struct dcam_dev* param);
LOCAL int sprd_img_reg_path2_isr(struct dcam_dev* param);
LOCAL int sprd_img_unreg_isr(struct dcam_dev* param);
LOCAL int sprd_img_unreg_path2_isr(struct dcam_dev* param);
/*LOCAL int sprd_img_proc_read(char *page, char **start,off_t off, int count, int *eof, void *data);*/
LOCAL void sprd_img_print_reg(void);
LOCAL int sprd_img_start_zoom(struct dcam_frame *frame, void* param);
LOCAL int sprd_img_path_cfg_output_addr(path_cfg_func path_cfg, struct dcam_path_spec* path_spec);

LOCAL struct dcam_format dcam_img_fmt[] = {
	{
		.name     = "4:2:2, packed, YUYV",
		.fourcc   = IMG_PIX_FMT_YUYV,
		.depth    = 16,
	},
	{
		.name     = "4:2:2, packed, YVYU",
		.fourcc   = IMG_PIX_FMT_YVYU,
		.depth    = 16,
	},
	{
		.name     = "4:2:2, packed, UYVY",
		.fourcc   = IMG_PIX_FMT_UYVY,
		.depth    = 16,
	},
	{
		.name     = "4:2:2, packed, VYUY",
		.fourcc   = IMG_PIX_FMT_VYUY,
		.depth    = 16,
	},
	{
		.name     = "YUV 4:2:2, planar, (Y-Cb-Cr)",
		.fourcc   = IMG_PIX_FMT_YUV422P,
		.depth    = 16,
	},
	{
		.name     = "YUV 4:2:0 planar (Y-CbCr)",
		.fourcc   = IMG_PIX_FMT_NV12,
		.depth    = 12,
	},
	{
		.name     = "YVU 4:2:0 planar (Y-CrCb)",
		.fourcc   = IMG_PIX_FMT_NV21,
		.depth    = 12,
	},

	{
		.name     = "YUV 4:2:0 planar (Y-Cb-Cr)",
		.fourcc   = IMG_PIX_FMT_YUV420,
		.depth    = 12,
	},
	{
		.name     = "YVU 4:2:0 planar (Y-Cr-Cb)",
		.fourcc   = IMG_PIX_FMT_YVU420,
		.depth    = 12,
	},
	{
		.name     = "RGB565 (LE)",
		.fourcc   = IMG_PIX_FMT_RGB565,
		.depth    = 16,
	},
	{
		.name     = "RGB565 (BE)",
		.fourcc   = IMG_PIX_FMT_RGB565X,
		.depth    = 16,
	},
	{
		.name     = "RawRGB",
		.fourcc   = IMG_PIX_FMT_GREY,
		.depth    = 8,
	},
	{
		.name     = "JPEG",
		.fourcc   = IMG_PIX_FMT_JPEG,
		.depth    = 8,
	},
};
#else
LOCAL struct dcam_format dcam_img_fmt[] = {
	{"4:2:2, packed, YUYV", IMG_PIX_FMT_YUYV, 16},
	{"4:2:2, packed, YVYU", IMG_PIX_FMT_YVYU, 16},
	{"4:2:2, packed, UYVY", IMG_PIX_FMT_UYVY, 16},
	{"4:2:2, packed, VYUY", IMG_PIX_FMT_VYUY, 16},
	{"YUV 4:2:2, planar, (Y-Cb-Cr)", IMG_PIX_FMT_YUV422P, 16},
	{"YUV 4:2:0 planar (Y-CbCr)", IMG_PIX_FMT_NV12, 12},
	{"YVU 4:2:0 planar (Y-CrCb)", IMG_PIX_FMT_NV21, 12},
	{"YUV 4:2:0 planar (Y-Cb-Cr)", IMG_PIX_FMT_YUV420, 12},
	{"YVU 4:2:0 planar (Y-Cr-Cb)", IMG_PIX_FMT_YVU420, 12},
	{"RGB565 (LE)", IMG_PIX_FMT_RGB565, 16},
	{"RGB565 (BE)", IMG_PIX_FMT_RGB565X, 16},
	{"RawRGB", IMG_PIX_FMT_GREY, 8},
	{"JPEG", IMG_PIX_FMT_JPEG, 8}
};
#endif

enum cmr_flash_status {
	FLASH_CLOSE = 0x0,
	FLASH_OPEN = 0x1,
	FLASH_TORCH = 0x2,	/*user only set flash to close/open/torch state */
	FLASH_AUTO = 0x3,
	FLASH_CLOSE_AFTER_OPEN = 0x10,	/* following is set to sensor */
	FLASH_HIGH_LIGHT = 0x11,
	FLASH_OPEN_ON_RECORDING = 0x22,
	FLASH_CLOSE_AFTER_AUTOFOCUS = 0x30,
	FLASH_STATUS_MAX
};

#define DISCARD_FRAME_TIME (10000)

LOCAL int img_get_timestamp(struct timeval *tv)
{
	struct timespec ts;

	ktime_get_ts(&ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = ts.tv_nsec / NSEC_PER_USEC;
	return 0;
}

extern int flash_torch_status;
LOCAL int sprd_img_setflash(uint32_t flash_mode)
{

	if(flash_torch_status==1)
		return 0;
	switch (flash_mode) {
	case FLASH_OPEN:        /*flash on */
	case FLASH_TORCH:        /*for torch */
		/*low light */
		sprd_flash_on();
		break;
	case FLASH_HIGH_LIGHT:
		/*high light */
		sprd_flash_high_light();
		break;
	case FLASH_CLOSE_AFTER_OPEN:     /*close flash */
	case FLASH_CLOSE_AFTER_AUTOFOCUS:
	case FLASH_CLOSE:
		/*close the light */
		sprd_flash_close();
		break;
	default:
		printk("sprd_img_setflash unknow mode:flash_mode 0x%x \n", flash_mode);
		break;
	}

	DCAM_TRACE("sprd_img_setflash: flash_mode 0x%x  \n", flash_mode);

	return 0;
}

LOCAL int sprd_img_opt_flash(struct dcam_frame *frame, void* param)
{
	struct dcam_dev          *dev = (struct dcam_dev*)param;
	struct dcam_info         *info = NULL;

	if (dev == NULL) {
		DCAM_TRACE("SPRD_IMG: sprd_img_opt_flash, dev is NULL \n");
		return 0;
	}

	info = &dev->dcam_cxt;
	if (info->flash_status < FLASH_STATUS_MAX) {
		DCAM_TRACE("SPRD_IMG: sprd_img_opt_flash, status %d \n", info->flash_status);
		if(info->flash_status == FLASH_CLOSE_AFTER_AUTOFOCUS) {
			img_get_timestamp(&info->timestamp);
			info->after_af = 1;
			DCAM_TRACE("SPRD_IMG: sprd_img_opt_flash, time, %d %d \n",
				(int)info->timestamp.tv_sec, (int)info->timestamp.tv_usec);
		}
		sprd_img_setflash(info->flash_status);
		info->flash_status = FLASH_STATUS_MAX;
	}

	return 0;
}

LOCAL int sprd_img_start_flash(struct dcam_frame *frame, void* param)
{
	struct dcam_dev          *dev = (struct dcam_dev*)param;
	struct dcam_info         *info = NULL;
	uint32_t                 need_light = 1;

	if (dev == NULL) {
		DCAM_TRACE("SPRD_IMG: sprd_img_start_flash, dev is NULL \n");
		return -1;
	}

	info = &dev->dcam_cxt;
	if (info->flash_status < FLASH_STATUS_MAX) {
		if (FLASH_HIGH_LIGHT == info->flash_status) {
			dev->frame_skipped ++;
			if (dev->frame_skipped >= info->skip_number) {
				//flash lighted at the last SOF before the right capture frame
				DCAM_TRACE("V4L2: waiting finished \n");
			} else {
				need_light = 0;
				DCAM_TRACE("V4L2: wait for the next SOF, %d ,%d \n",
					dev->frame_skipped, info->skip_number);
			}
		}
		if (need_light) {
			up(&dev->flash_thread_sem);
		}
	}

	return 0;
}

int flash_thread_loop(void *arg)
{
	struct dcam_dev          *dev = (struct dcam_dev*)arg;

	if (dev == NULL) {
		DCAM_TRACE("SPRD_IMG: flash_thread_loop, dev is NULL \n");
		return -1;
	}
	while (1) {
		if (0 == down_interruptible(&dev->flash_thread_sem)) {
			if (dev->is_flash_thread_stop) {
				sprd_img_setflash(0);
				DCAM_TRACE("SPRD_IMG: flash_thread_loop stop \n");
				break;
			}
			sprd_img_opt_flash(NULL, arg);
		} else {
			DCAM_TRACE("SPRD_IMG: flash int!");
			break;
		}
	}
	dev->is_flash_thread_stop = 0;

	return 0;
}

int dcam_create_flash_thread(void* param)
{
	struct dcam_dev          *dev = (struct dcam_dev*)param;

	if (dev == NULL) {
		DCAM_TRACE("SPRD_IMG: dcam_create_flash_thread, dev is NULL \n");
		return -1;
	}
	dev->is_flash_thread_stop = 0;
	sema_init(&dev->flash_thread_sem, 0);
	dev->flash_thread = kthread_run(flash_thread_loop, param, "dcam_flash_thread");
	if (IS_ERR(dev->flash_thread)) {
		printk("SPRD_IMG: dcam_create_flash_thread error!\n");
		return -1;
	}
	return 0;
}

int dcam_stop_flash_thread(void* param)
{
	struct dcam_dev          *dev = (struct dcam_dev*)param;
	int cnt = 0;

	if (dev == NULL) {
		DCAM_TRACE("SPRD_IMG: sprd_img_opt_flash, dev is NULL \n");
		return -1;
	}
	if (dev->flash_thread) {
		dev->is_flash_thread_stop = 1;
		up(&dev->flash_thread_sem);
		if (0 != dev->is_flash_thread_stop) {
			while (cnt < 500) {
				cnt++;
				if (0 == dev->is_flash_thread_stop)
					break;
				msleep(1);
			}
		}
		dev->flash_thread = NULL;
	}

	return 0;
}

LOCAL int sprd_img_discard_frame(struct dcam_frame *frame, void* param)
{
	int                                  ret = DCAM_RTN_PARA_ERR;
	struct dcam_dev          *dev = (struct dcam_dev*)param;
	struct dcam_info         *info = NULL;
	struct timeval           timestamp;
	uint32_t                 flag = 0;

	info = &dev->dcam_cxt;
	img_get_timestamp(&timestamp);
	DCAM_TRACE("SPRD_IMG: sprd_img_discard_frame, time, %d %d \n",
		(int)timestamp.tv_sec, (int)timestamp.tv_usec);
	if ((timestamp.tv_sec == info->timestamp.tv_sec)
		&& (timestamp.tv_usec -info->timestamp.tv_usec >= DISCARD_FRAME_TIME)){
		flag = 1;
	} else if (timestamp.tv_sec > info->timestamp.tv_sec) {
		if ((1000000 -info->timestamp.tv_sec) + timestamp.tv_sec >= DISCARD_FRAME_TIME) {
			flag = 1;
		}
	}

	if (flag) {
		DCAM_TRACE("SPRD_IMG: sprd_img_discard_frame,unlock frame %p \n", frame);
		//dcam_frame_unlock(frame);
		ret =  DCAM_RTN_SUCCESS;
	}
	return ret;
}

LOCAL struct dcam_format *sprd_img_get_format(uint32_t fourcc)
{
	struct dcam_format       *fmt;
	uint32_t                 i;

	for (i = 0; i < ARRAY_SIZE(dcam_img_fmt); i++) {
		fmt = &dcam_img_fmt[i];
		if (fmt->fourcc == fourcc)
			break;
	}

	if (unlikely(i == ARRAY_SIZE(dcam_img_fmt)))
		return NULL;

	return &dcam_img_fmt[i];
}

LOCAL uint32_t sprd_img_get_fourcc(struct dcam_path_spec *path)
{
	uint32_t                 fourcc = DCAM_INVALID_FOURCC;

	switch (path->out_fmt) {
	case DCAM_YUV422:
		fourcc = IMG_PIX_FMT_YUV422P;
		break;
	case DCAM_YUV420:
		if (likely(DCAM_ENDIAN_LITTLE == path->end_sel.uv_endian))
			fourcc = IMG_PIX_FMT_NV12;
		else
			fourcc = IMG_PIX_FMT_NV21;
		break;
	case DCAM_YUV420_3FRAME:
		fourcc = IMG_PIX_FMT_YUV420;
		break;
	case DCAM_RGB565:
		if (likely(DCAM_ENDIAN_HALFBIG == path->end_sel.uv_endian))
			fourcc = IMG_PIX_FMT_RGB565;
		else
			fourcc = IMG_PIX_FMT_RGB565X;
		break;
	case DCAM_RAWRGB:
		fourcc = IMG_PIX_FMT_GREY;
		break;
	case DCAM_JPEG:
		fourcc = IMG_PIX_FMT_JPEG;
		break;
	default:
		break;
	}
	return fourcc;
}

LOCAL uint32_t sprd_img_get_deci_factor(uint32_t src_size, uint32_t dst_size)
{
	uint32_t                 factor = 0;

	if (0 == src_size || 0 == dst_size) {
		return factor;
	}

	for (factor = 0; factor < DCAM_CAP_X_DECI_FAC_MAX; factor ++) {
		if (src_size < (uint32_t)(dst_size * (1 << factor))) {
			break;
		}
	}

	return factor;
}

LOCAL uint32_t sprd_img_endian_sel(uint32_t fourcc, struct dcam_path_spec *path)
{
	uint32_t                 depth = 0;

	if (IMG_PIX_FMT_YUV422P == fourcc ||
		IMG_PIX_FMT_RGB565 == fourcc ||
		IMG_PIX_FMT_RGB565X == fourcc) {
		depth = 16;
		if (IMG_PIX_FMT_YUV422P == fourcc) {
			path->out_fmt = DCAM_YUV422;
		} else {
			path->out_fmt = DCAM_RGB565;
			if (IMG_PIX_FMT_RGB565 == fourcc) {
				path->end_sel.y_endian = DCAM_ENDIAN_HALFBIG;
			} else {
				path->end_sel.y_endian = DCAM_ENDIAN_BIG;
			}
		}
	} else {
		depth = 12;
		if (IMG_PIX_FMT_YUV420 == fourcc ||
			IMG_PIX_FMT_YVU420 == fourcc) {
			path->out_fmt = DCAM_YUV420_3FRAME;
		} else {
			path->out_fmt = DCAM_YUV420;
			if (IMG_PIX_FMT_NV12 == fourcc) {
				path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
			} else {
				path->end_sel.uv_endian = DCAM_ENDIAN_HALFBIG;
			}
		}
	}

	return depth;
}

LOCAL int sprd_img_check_path0_cap(uint32_t fourcc,
					struct sprd_img_format *f,
					struct dcam_info   *info)
{
	struct dcam_path_spec    *path = &info->dcam_path[DCAM_PATH0];

	DCAM_TRACE("SPRD_IMG: check format for path0 \n");

	path->is_from_isp = f->need_isp;
	path->rot_mode = f->reserved[0];
	path->frm_type = f->channel_id;
	path->is_work = 0;

	switch (fourcc) {
	case IMG_PIX_FMT_GREY:
		path->out_fmt = info->is_loose; // 0 - word/packet, 1 - half word
		break;

	case IMG_PIX_FMT_JPEG:
		path->out_fmt = 0; // 0 - word, 1 - half word
		path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
		break;

	case IMG_PIX_FMT_YUYV:
		path->out_fmt = DCAM_OUTPUT_YVYU_1FRAME;
		path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
		break;

	case IMG_PIX_FMT_NV21:
	case IMG_PIX_FMT_NV12:
		path->out_fmt = DCAM_OUTPUT_YUV420;
		path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
		if (IMG_PIX_FMT_NV12 == fourcc) {
			path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
		} else {
			path->end_sel.uv_endian = DCAM_ENDIAN_HALFBIG;
		}
		printk("SPRD_IMG: path->end_sel.uv_endian=%d \n", path->end_sel.uv_endian);
		break;

	default:
		printk("SPRD_IMG: unsupported image format for path0 0x%x \n", fourcc);
		return -EINVAL;
	}
	path->fourcc = fourcc;

	DCAM_TRACE("SPRD_IMG: check format for path0: out_fmt=%d, is_loose=%d \n", path->out_fmt, info->is_loose);
	path->out_size.w = f->width;
	path->out_size.h = f->height;

	path->is_work = 1;

	return 0;
}

LOCAL int sprd_img_check_path1_cap(uint32_t fourcc,
					struct sprd_img_format *f,
					struct dcam_info   *info)
{
	uint32_t                 maxw, maxh, tempw,temph;
	uint32_t                 depth_pixel = 0;
	struct dcam_path_spec    *path = &info->dcam_path[DCAM_PATH1];
	uint32_t                 need_recal = 0;

	DCAM_TRACE("SPRD_IMG: check format for path1 \n");

	path->frm_type = f->channel_id;
	path->is_from_isp = f->need_isp;
	path->rot_mode = f->reserved[0];
	path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
	path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
	path->is_work = 0;
	path->pixel_depth = 0;
	path->img_deci.x_factor = 0;
	path->img_deci.y_factor = 0;
	tempw = path->in_rect.w;
	temph = path->in_rect.h;
	info->img_deci.x_factor = 0;
	f->need_binning = 0;
	printk("zcf sprd_img_check_path1_cap  rot_mode:%d\n",path->rot_mode);
	/*app should fill in this field(fmt.pix.priv) to set the base index of frame buffer,
	and lately this field will return the flag whether ISP is needed for this work path*/

	switch (fourcc) {
	case IMG_PIX_FMT_GREY:
	case IMG_PIX_FMT_JPEG:
	case IMG_PIX_FMT_YUYV:
	case IMG_PIX_FMT_YVYU:
	case IMG_PIX_FMT_UYVY:
	case IMG_PIX_FMT_VYUY:
		if (unlikely(f->width != tempw ||
			f->height != temph)) {
			/*need need scaling or triming*/
			printk("SPRD_IMG: Can not scaling for this image fmt src %d %d, dst %d %d \n",
					tempw,
					temph,
					f->width,
					f->height);
			return -EINVAL;
		}

		if (unlikely(DCAM_CAP_MODE_JPEG != info->sn_mode &&
			IMG_PIX_FMT_JPEG == fourcc)) {
			/* the output of sensor is not JPEG which is needed by app*/
			printk("SPRD_IMG: It's not JPEG sensor \n");
			return -EINVAL;
		}

		if (unlikely(DCAM_CAP_MODE_RAWRGB != info->sn_mode &&
			IMG_PIX_FMT_GREY == fourcc)) {
			/* the output of sensor is not RawRGB which is needed by app*/
			printk("SPRD_IMG: It's not RawRGB sensor \n");
			return -EINVAL;
		}

		if (IMG_PIX_FMT_GREY == fourcc) {
			if (DCAM_CAP_IF_CSI2 == info->if_mode && 0 == info->is_loose) {
				depth_pixel = 10;
			} else {
				depth_pixel = 16;
			}
			DCAM_TRACE("SPRD_IMG: RawRGB sensor, %d %d \n", info->is_loose, depth_pixel);
		} else if (IMG_PIX_FMT_JPEG == fourcc) {
			depth_pixel = 8;
		} else {
			depth_pixel = 16;
		}

		if (IMG_PIX_FMT_GREY == fourcc) {
			path->out_fmt = DCAM_RAWRGB;
			path->end_sel.y_endian = DCAM_ENDIAN_BIG;
		} else {
			path->out_fmt = DCAM_JPEG;
		}
		break;
	case IMG_PIX_FMT_YUV422P:
	case IMG_PIX_FMT_YUV420:
	case IMG_PIX_FMT_YVU420:
	case IMG_PIX_FMT_NV12:
	case IMG_PIX_FMT_NV21:
	case IMG_PIX_FMT_RGB565:
	case IMG_PIX_FMT_RGB565X:
		if (DCAM_CAP_MODE_RAWRGB == info->sn_mode &&
			path->is_from_isp) {
			if (unlikely(info->cap_out_size.w > DCAM_ISP_LINE_BUF_LENGTH)) {
				if (0 == info->if_mode) {
					/*CCIR CAP, no bining*/
					printk("SPRD_IMG: CCIR CAP, no bining for this path, %d %d \n",
						f->width,
						f->height);
					return -EINVAL;
				} else {
					/*MIPI CAP, support 1/2 bining*/
					DCAM_TRACE("Need Binning \n");
					tempw = tempw >> 1;
					if (unlikely(tempw > DCAM_ISP_LINE_BUF_LENGTH)) {
						printk("SPRD_IMG: the width is out of ISP line buffer, %d %d \n",
							tempw,
							DCAM_ISP_LINE_BUF_LENGTH);
						return -EINVAL;
					}
					info->img_deci.x_factor = 1;
					f->need_binning = 1;
					DCAM_TRACE("x_factor, %d \n", info->img_deci.x_factor);
					path->in_size.w = path->in_size.w >> 1;
					path->in_rect.x = path->in_rect.x >> 1;
					path->in_rect.w = path->in_rect.w >> 1;
					path->in_rect.w = path->in_rect.w & (~3);
				}
			}
		}
		if (DCAM_CAP_MODE_YUV == info->sn_mode ||
			DCAM_CAP_MODE_SPI == info->sn_mode) {
			if (tempw != f->width || temph != f->height) {
				/*scaling needed*/
				if (unlikely(f->width > DCAM_PATH1_LINE_BUF_LENGTH)) {
					/*out of scaling capbility*/
					printk("SPRD_IMG: the output width %d can not be more than %d \n",
						f->width,
						DCAM_PATH1_LINE_BUF_LENGTH);
					return -EINVAL;
				}

				/* To check whether the output size is too lager*/
				maxw = tempw * DCAM_SC_COEFF_UP_MAX;
				maxh = temph * DCAM_SC_COEFF_UP_MAX;
				if (unlikely(f->width > maxw || f->height > maxh)) {
					/*out of scaling capbility*/
					printk("SPRD_IMG: the output size is too large, %d %d \n",
						f->width,
						f->height);
					return -EINVAL;
				}

				/* To check whether the output size is too small*/
				maxw = f->width * DCAM_SC_COEFF_DOWN_MAX;
				if (unlikely( tempw > maxw)) {
					path->img_deci.x_factor = sprd_img_get_deci_factor(tempw, maxw);
					if (path->img_deci.x_factor > DCAM_PATH_DECI_FAC_MAX) {
						printk("SPRD_IMG: the output size is too small, %d %d \n",
							f->width,
							f->height);
						return -EINVAL;
					}
				}

				maxh = f->height * DCAM_SC_COEFF_DOWN_MAX;
				if (unlikely(temph > maxh)) {
					path->img_deci.y_factor = sprd_img_get_deci_factor(temph, maxh);
					if (path->img_deci.y_factor > DCAM_PATH_DECI_FAC_MAX) {
						printk("SPRD_IMG: the output size is too small, %d %d \n",
							f->width,
							f->height);
						return -EINVAL;
					}
				}

				if (path->img_deci.x_factor) {
					tempw           = path->in_rect.w >> 1;
					need_recal      = 1;
				}

				if (path->img_deci.y_factor) {
					temph           = path->in_rect.h >> 1;
					need_recal      = 1;
				}

				if (need_recal && (tempw != f->width || temph != f->height)) {
					/*scaling needed*/
					if (unlikely(f->width > DCAM_PATH1_LINE_BUF_LENGTH)) {
						/*out of scaling capbility*/
						printk("SPRD_IMG: the output width %d can not be more than %d \n",
							f->width,
							DCAM_PATH1_LINE_BUF_LENGTH);
						return -EINVAL;
					}
				}
			}
		} else if (DCAM_CAP_MODE_RAWRGB == info->sn_mode) {
			if (path->is_from_isp) {

				if (tempw != f->width ||
					temph != f->height) {
					/*scaling needed*/
					maxw = f->width * DCAM_SC_COEFF_DOWN_MAX;
					maxw = maxw * (1 << DCAM_PATH_DECI_FAC_MAX);
					maxh = f->height * DCAM_SC_COEFF_DOWN_MAX;
					maxh = maxh * (1 << DCAM_PATH_DECI_FAC_MAX);
					if (unlikely(tempw > maxw || temph > maxh)) {
						/*out of scaling capbility*/
						printk("SPRD_IMG: the output size is too small, %d %d \n",
								f->width,
								f->height);
						return -EINVAL;
					}

					if (unlikely(f->width > DCAM_PATH1_LINE_BUF_LENGTH)) {
						/*out of scaling capbility*/
						printk("SPRD_IMG: the output width %d can not be more than %d \n",
							f->width,
							DCAM_PATH1_LINE_BUF_LENGTH);
						return -EINVAL;
					}

					maxw = tempw * DCAM_SC_COEFF_UP_MAX;
					maxh = temph * DCAM_SC_COEFF_UP_MAX;
					if (unlikely(f->width > maxw || f->height > maxh)) {
						/*out of scaling capbility*/
						printk("SPRD_IMG: the output size is too large, %d %d \n",
							f->width,
							f->height);
						return -EINVAL;
					}
				}
			} else {
				/*no ISP ,only RawRGB data can be sampled*/
				printk("SPRD_IMG: RawRGB sensor, no ISP, format 0x%x can't be supported \n",
						fourcc);
				return -EINVAL;
			}
		}

		depth_pixel = sprd_img_endian_sel(fourcc, path);
		break;
	default:
		printk("SPRD_IMG: unsupported image format for path2 0x%x \n", fourcc);
		return -EINVAL;
	}

	path->fourcc = fourcc;
	path->pixel_depth = depth_pixel;
	f->bytesperline = (f->width * depth_pixel) >> 3;
	path->out_size.w = f->width;
	path->out_size.h = f->height;
	path->is_work = 1;
	return 0;
}

LOCAL int sprd_img_check_path2_cap(uint32_t fourcc,
					struct sprd_img_format *f,
					struct dcam_info   *info)
{
	uint32_t                 maxw, maxh, tempw,temph;
	uint32_t                 depth_pixel = 0;
	struct dcam_path_spec    *path = &info->dcam_path[DCAM_PATH2];
	uint32_t                 need_recal = 0;

	//return -EINVAL;

	DCAM_TRACE("SPRD_IMG: check format for path2 \n");

	path->frm_type = f->channel_id;
	path->is_from_isp = f->need_isp;
	path->rot_mode= f->reserved[0];
	path->end_sel.y_endian = DCAM_ENDIAN_LITTLE;
	path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE;
	path->is_work = 0;
	path->pixel_depth = 0;
	printk("zcf sprd_img_check_path2_cap rot:%d\n",path->rot_mode);
	if (info->img_deci.x_factor) {
		tempw = path->in_rect.w >> 1;
		path->in_size.w = path->in_size.w >> 1;
		path->in_rect.w = path->in_rect.w >> 1;
		path->in_rect.x = path->in_rect.x >> 1;
	} else {
		tempw = path->in_rect.w;
	}
	temph = path->in_rect.h;
	switch (fourcc) {
	case IMG_PIX_FMT_YUV422P:
	case IMG_PIX_FMT_YUV420:
	case IMG_PIX_FMT_YVU420:
	case IMG_PIX_FMT_NV12:
	case IMG_PIX_FMT_NV21:

		if (DCAM_CAP_MODE_JPEG == info->sn_mode ||
			info->sn_mode >= DCAM_CAP_MODE_MAX) {
			return -EINVAL;
		}

		if (DCAM_CAP_MODE_RAWRGB == info->sn_mode) {
			path->is_from_isp = 1;
		}

		if (DCAM_CAP_MODE_YUV == info->sn_mode ||
			DCAM_CAP_MODE_SPI == info->sn_mode) {
			if (tempw != f->width || temph != f->height) {
				/*scaling needed*/
				if (unlikely(f->width > DCAM_PATH2_LINE_BUF_LENGTH)) {
					/*out of scaling capbility*/
					printk("SPRD_IMG: the output width %d can not be more than %d \n",
						f->width,
						DCAM_PATH2_LINE_BUF_LENGTH);
					return -EINVAL;
				}

				/* To check whether the output size is too lager*/
				maxw = tempw * DCAM_SC_COEFF_UP_MAX;
				maxh = temph * DCAM_SC_COEFF_UP_MAX;
				if (unlikely(f->width > maxw || f->height > maxh)) {
					/*out of scaling capbility*/
					printk("SPRD_IMG: the output size is too large, %d %d \n",
						f->width,
						f->height);
					return -EINVAL;
				}

				/* To check whether the output size is too small*/
				maxw = f->width * DCAM_SC_COEFF_DOWN_MAX;
				if (unlikely( tempw > maxw)) {
					path->img_deci.x_factor = sprd_img_get_deci_factor(tempw, maxw);
					if (path->img_deci.x_factor > DCAM_PATH_DECI_FAC_MAX) {
						printk("SPRD_IMG: the output size is too small, %d %d \n",
							f->width,
							f->height);
						return -EINVAL;
					}
				}

				maxh = f->height * DCAM_SC_COEFF_DOWN_MAX;
				if (unlikely(temph > maxh)) {
					path->img_deci.y_factor = sprd_img_get_deci_factor(temph, maxh);
					if (path->img_deci.y_factor > DCAM_PATH_DECI_FAC_MAX) {
						printk("SPRD_IMG: the output size is too small, %d %d \n",
							f->width,
							f->height);
						return -EINVAL;
					}
				}

				if (path->img_deci.x_factor) {
					tempw           = path->in_rect.w >> 1;
					need_recal      = 1;
				}

				if (path->img_deci.y_factor) {
					temph           = path->in_rect.h >> 1;
					need_recal      = 1;
				}

				if (need_recal && (tempw != f->width || temph != f->height)) {
					/*scaling needed*/
					if (unlikely(f->width > DCAM_PATH2_LINE_BUF_LENGTH)) {
						/*out of scaling capbility*/
						printk("SPRD_IMG: the output width %d can not be more than %d \n",
							f->width,
							DCAM_PATH2_LINE_BUF_LENGTH);
						return -EINVAL;
					}
				}
			}
		} else if (DCAM_CAP_MODE_RAWRGB == info->sn_mode) {
			if (path->is_from_isp) {

				if (tempw != f->width ||
					temph != f->height) {
					/*scaling needed*/
					maxw = f->width * DCAM_SC_COEFF_DOWN_MAX;
					maxw = maxw * (1 << DCAM_PATH_DECI_FAC_MAX);
					maxh = f->height * DCAM_SC_COEFF_DOWN_MAX;
					maxh = maxh * (1 << DCAM_PATH_DECI_FAC_MAX);
					if (unlikely(tempw > maxw || temph > maxh)) {
						/*out of scaling capbility*/
						printk("SPRD_IMG: the output size is too small, %d %d \n",
								f->width,
								f->height);
						return -EINVAL;
					}

					if (unlikely(f->width > DCAM_PATH2_LINE_BUF_LENGTH)) {
						/*out of scaling capbility*/
						printk("SPRD_IMG: the output width %d can not be more than %d \n",
							f->width,
							DCAM_PATH2_LINE_BUF_LENGTH);
						return -EINVAL;
					}

					maxw = tempw * DCAM_SC_COEFF_UP_MAX;
					maxh = temph * DCAM_SC_COEFF_UP_MAX;
					if (unlikely(f->width > maxw || f->height > maxh)) {
						/*out of scaling capbility*/
						printk("SPRD_IMG: the output size is too large, %d %d \n",
							f->width,
							f->height);
						return -EINVAL;
					}
				}
			} else {
				/*no ISP ,only RawRGB data can be sampled*/
				printk("SPRD_IMG: RawRGB sensor, no ISP, format 0x%x can't be supported \n",
						fourcc);
				return -EINVAL;
			}
		}


		depth_pixel = sprd_img_endian_sel(fourcc, path);
		//path->end_sel.uv_endian = DCAM_ENDIAN_LITTLE; // tmp fix: output is vu, jpeg only support vu
		break;
	default:
		printk("SPRD_IMG: unsupported image format for path2 0x%x \n", fourcc);
		return -EINVAL;
	}

	path->fourcc = fourcc;
	f->need_isp = path->is_from_isp;
	path->pixel_depth = depth_pixel;
	f->bytesperline = (f->width * depth_pixel) >> 3;
	path->out_size.w = f->width;
	path->out_size.h = f->height;
	path->is_work = 1;

	return 0;
}

LOCAL int sprd_img_cap_cfg(struct dcam_info* info)
{
	int                      ret = DCAM_RTN_SUCCESS;
	uint32_t                 param = 0;

	if (NULL == info)
		return -EINVAL;

	ret = dcam_cap_cfg(DCAM_CAP_SYNC_POL, &info->sync_pol);
	IMG_RTN_IF_ERR(ret);

	if ((info->dcam_path[DCAM_PATH0].is_work && info->dcam_path[DCAM_PATH0].is_from_isp) ||
		(info->dcam_path[DCAM_PATH1].is_work && info->dcam_path[DCAM_PATH1].is_from_isp) ||
		(info->dcam_path[DCAM_PATH2].is_work && info->dcam_path[DCAM_PATH2].is_from_isp)) {
		param = 1;
	} else if (!info->dcam_path[DCAM_PATH0].is_work &&
			!info->dcam_path[DCAM_PATH1].is_work &&
			!info->dcam_path[DCAM_PATH2].is_work &&
			(DCAM_CAP_MODE_RAWRGB == info->sn_mode)) {
			param = 1;
	} else {
		param = 0;
	}
	ret = dcam_cap_cfg(DCAM_CAP_TO_ISP, &param);
	IMG_RTN_IF_ERR(ret);

	ret = dcam_cap_cfg(DCAM_CAP_YUV_TYPE, &info->yuv_ptn);
	IMG_RTN_IF_ERR(ret);

	ret = dcam_cap_cfg(DCAM_CAP_DATA_BITS, &info->data_bits);
	IMG_RTN_IF_ERR(ret);

	if (DCAM_CAP_MODE_RAWRGB == info->sn_mode &&
		DCAM_CAP_IF_CSI2 == info->if_mode)
	{
		ret = dcam_cap_cfg(DCAM_CAP_DATA_PACKET, &info->is_loose);
		IMG_RTN_IF_ERR(ret);
	}

	ret = dcam_cap_cfg(DCAM_CAP_FRM_DECI, &info->frm_deci);
	IMG_RTN_IF_ERR(ret);

	ret = dcam_cap_cfg(DCAM_CAP_INPUT_RECT, &info->cap_in_rect);
	IMG_RTN_IF_ERR(ret);

	ret = dcam_cap_cfg(DCAM_CAP_FRM_COUNT_CLR, NULL);
	IMG_RTN_IF_ERR(ret);

	ret = dcam_cap_cfg(DCAM_CAP_PRE_SKIP_CNT, &info->skip_number);
	IMG_RTN_IF_ERR(ret);

	ret = dcam_cap_cfg(DCAM_CAP_SAMPLE_MODE, &info->capture_mode);
	IMG_RTN_IF_ERR(ret);

	ret = dcam_cap_cfg(DCAM_CAP_ZOOM_MODE, &info->is_smooth_zoom);

	ret = dcam_cap_cfg(DCAM_CAP_IMAGE_XY_DECI, &info->img_deci);

exit:
	return ret;
}

LOCAL void sprd_img_frm_pop(struct dcam_frame *frame, void* param)
{
#if 0
	uint32_t                 i = 0;
	uint32_t                 frame_count = 0;
	struct dcam_path_spec    *path = NULL;
	struct dcam_dev          *dev = (struct dcam_dev*)param;
	struct dcam_info         *info = NULL;

	info = &dev->dcam_cxt;

	switch (frame->type) {
	case DCAM_PATH0:
		path = &info->dcam_path[DCAM_PATH0];
		break;
	case DCAM_PATH1:
		path = &info->dcam_path[DCAM_PATH1];
		break;
	case DCAM_PATH2:
		path = &info->dcam_path[DCAM_PATH2];
		break;
	default:
		printk("SPRD_IMG error: sprd_img_frm_pop, path 0x%x \n", frame->type);
		return -EINVAL;
	}
	DCAM_TRACE("SPRD_IMG: sprd_img_frm_pop start, path 0x%x \n", frame->type);
	DCAM_TRACE("SPRD_IMG: sprd_img_frm_pop, yaddr 0x%x 0x%x \n", frame->yaddr, path->frm_addr[0].yaddr);
	if (frame->yaddr == path->frm_addr[0].yaddr) {
		for (i = 0; i < path->frm_cnt_act - 1; i++) {
			path->frm_addr[i]     = path->frm_addr[i+1];
			path->frm_addr_vir[i] = path->frm_addr_vir[i+1];
		}
		path->frm_cnt_act--;
		memset(&path->frm_addr[path->frm_cnt_act], 0, sizeof(struct dcam_addr));
		memset(&path->frm_addr_vir[path->frm_cnt_act], 0, sizeof(struct dcam_addr));
	} else {
		printk("SPRD_IMG error: sprd_img_frm_pop, yaddr 0x%x 0x%x \n", frame->yaddr, path->frm_addr[0].yaddr);
	}
	DCAM_TRACE("SPRD_IMG: sprd_img_frm_pop end, path 0x%x yaddr 0x%x \n", frame->type, frame->yaddr);
#endif
	return;
}

LOCAL int sprd_img_tx_done(struct dcam_frame *frame, void* param)
{
	int                      ret = DCAM_RTN_SUCCESS;
	struct dcam_dev          *dev = (struct dcam_dev*)param;
	struct dcam_path_spec    *path;
	struct dcam_node         node;
	uint32_t                 fmr_index;
	struct dcam_img_buf_addr buf_addr;
	struct timeval           time;

	if (NULL == frame || NULL == param || 0 == atomic_read(&dev->stream_on))
		return -EINVAL;
	path = &dev->dcam_cxt.dcam_path[frame->type];
	if (PATH_IDLE == path->status) {
		return ret;
	}

	atomic_set(&dev->run_flag, 1);

	img_get_timestamp(&time);
	DCAM_TRACE("time, %ld %ld \n", (unsigned long)time.tv_sec, (unsigned long)time.tv_usec);

	memset((void*)&node, 0, sizeof(struct dcam_node));
	node.irq_flag = IMG_TX_DONE;
	node.f_type   = frame->type;
	node.index    = frame->fid;
	node.width   = frame->width;
	node.height   = frame->height;
	node.yaddr    = frame->yaddr;
	node.uaddr    = frame->uaddr;
	node.vaddr    = frame->vaddr;
	node.yaddr_vir = frame->yaddr_vir;
	node.uaddr_vir = frame->uaddr_vir;
	node.vaddr_vir = frame->vaddr_vir;
	node.time.tv_sec = time.tv_sec;
	node.time.tv_usec = time.tv_usec;
	node.reserved = frame->zsl_private;

	DCAM_TRACE("SPRD_IMG: flag 0x%x type 0x%x index 0x%x \n",
		node.irq_flag, node.f_type, node.index);

/*	path = &dev->dcam_cxt.dcam_path[frame->type];
	ret = sprd_img_buf_queue_read(&path->buf_queue, &buf_addr);
	if (ret)
		return ret;

	node.yaddr_vir = buf_addr.frm_addr_vir.yaddr;
	node.uaddr_vir = buf_addr.frm_addr_vir.uaddr;
	node.vaddr_vir = buf_addr.frm_addr_vir.vaddr;
*/
	if (DCAM_PATH0 == frame->type && DCAM_CAP_MODE_JPEG == dev->dcam_cxt.sn_mode) {
		dcam_cap_get_info(DCAM_CAP_JPEG_GET_LENGTH, &node.reserved);
		printk("SPRD_IMG: JPEG length 0x%x \n", node.reserved);
		if (node.reserved < DCAM_JPEG_LENGTH_MIN) {
			return sprd_img_tx_error(frame, param);
		}
	}

	if (dev->dcam_cxt.after_af && DCAM_PATH1 == frame->type) {
		ret = sprd_img_discard_frame(frame, param);
		if (DCAM_RTN_SUCCESS == ret) {
			dev->dcam_cxt.after_af = 0;
			node.irq_flag = IMG_CANCELED_BUF;
		}
	}

	ret = sprd_img_queue_write(&dev->queue, &node);
	if (ret)
		return ret;

	up(&dev->irq_sem);
	return ret;
}

LOCAL int sprd_img_tx_error(struct dcam_frame *frame, void* param)
{
	int                      ret = DCAM_RTN_SUCCESS;
	struct dcam_dev          *dev = (struct dcam_dev*)param;
	struct dcam_path_spec    *path;
	struct dcam_node         node;
	struct dcam_img_buf_addr buf_addr;

	if (NULL == param || 0 == atomic_read(&dev->stream_on))
		return -EINVAL;

	memset((void*)&node, 0, sizeof(struct dcam_node));
	atomic_set(&dev->run_flag, 1);//to avoid time out processing
	node.irq_flag = IMG_TX_ERR;
	if (NULL != frame) {
		node.f_type   = frame->type;
		node.index    = frame->fid;
		node.height   = frame->height;
		node.yaddr    = frame->yaddr;
		node.uaddr    = frame->uaddr;
		node.vaddr    = frame->vaddr;
		node.yaddr_vir = frame->yaddr_vir;
		node.uaddr_vir = frame->uaddr_vir;
		node.vaddr_vir = frame->vaddr_vir;
		node.reserved = frame->zsl_private;

/*		path = &dev->dcam_cxt.dcam_path[frame->type];
		ret = sprd_img_buf_queue_read(&path->buf_queue, &buf_addr);
		if (ret)
			return ret;

		node.yaddr_vir = buf_addr.frm_addr_vir.yaddr;
		node.uaddr_vir = buf_addr.frm_addr_vir.uaddr;
		node.vaddr_vir = buf_addr.frm_addr_vir.vaddr;*/
	}
	ret = sprd_img_queue_write(&dev->queue, &node);
	if (ret)
		return ret;

	up(&dev->irq_sem);

	printk("SPRD_IMG: tx_error \n");
	//mm_clk_register_trace();
	return ret;
}

LOCAL int sprd_img_no_mem(struct dcam_frame *frame, void* param)
{
	int                      ret = DCAM_RTN_SUCCESS;
	struct dcam_dev          *dev = (struct dcam_dev*)param;
	struct dcam_path_spec    *path;
	struct dcam_node         node;
	struct dcam_img_buf_addr buf_addr;

	if (NULL == param || 0 == atomic_read(&dev->stream_on))
		return -EINVAL;
	atomic_set(&dev->run_flag, 1);//to avoid time out processing

	memset((void*)&node, 0, sizeof(struct dcam_node));
	node.irq_flag = IMG_NO_MEM;
	node.f_type   = frame->type;
	node.index    = frame->fid;
	node.height   = frame->height;
	node.yaddr    = frame->yaddr;
	node.uaddr    = frame->uaddr;
	node.vaddr    = frame->vaddr;
	node.yaddr_vir = frame->yaddr_vir;
	node.uaddr_vir = frame->uaddr_vir;
	node.vaddr_vir = frame->vaddr_vir;


/*	path = &dev->dcam_cxt.dcam_path[frame->type];
	ret = sprd_img_buf_queue_read(&path->buf_queue, &buf_addr);
	if (ret)
		return ret;

	node.yaddr_vir = buf_addr.frm_addr_vir.yaddr;
	node.uaddr_vir = buf_addr.frm_addr_vir.uaddr;
	node.vaddr_vir = buf_addr.frm_addr_vir.vaddr;*/
	ret = sprd_img_queue_write(&dev->queue, &node);
	if (ret)
		return ret;

	up(&dev->irq_sem);
	printk("SPRD_IMG: no mem \n");
	return ret;
}

LOCAL int sprd_img_tx_stop(void* param)
{
	int                      ret = DCAM_RTN_SUCCESS;
	struct dcam_dev          *dev = (struct dcam_dev*)param;
	struct dcam_node         node;

	ret = sprd_img_queue_init(&dev->queue);
	if (unlikely(ret != 0)) {
		printk("SPRD_IMG: Failed to init queue STOP\n");
	}

	memset((void*)&node, 0, sizeof(struct dcam_node));
	node.irq_flag = IMG_TX_STOP;
	ret = sprd_img_queue_write(&dev->queue, &node);
	if (ret)
		return ret;

	up(&dev->irq_sem);

	return ret;
}

LOCAL int sprd_img_reg_isr(struct dcam_dev* param)
{
	dcam_reg_isr(DCAM_PATH0_DONE,   sprd_img_tx_done,       param);
	dcam_reg_isr(DCAM_PATH1_DONE,   sprd_img_tx_done,       param);
	dcam_reg_isr(DCAM_PATH0_OV,     sprd_img_tx_error,      param);
	dcam_reg_isr(DCAM_PATH1_OV,     sprd_img_tx_error,      param);
	dcam_reg_isr(DCAM_ISP_OV,       sprd_img_tx_error,      param);
	dcam_reg_isr(DCAM_MIPI_OV,      sprd_img_tx_error,      param);
	dcam_reg_isr(DCAM_SN_LINE_ERR,  sprd_img_tx_error,      param);
	dcam_reg_isr(DCAM_SN_FRAME_ERR, sprd_img_tx_error,      param);
	dcam_reg_isr(DCAM_JPEG_BUF_OV,  sprd_img_no_mem,        param);
	dcam_reg_isr(DCAM_SN_EOF,       sprd_img_start_flash,   param);
	dcam_reg_isr(DCAM_PATH1_SOF,    sprd_img_start_zoom,    param);

	return 0;
}

LOCAL int sprd_img_reg_path2_isr(struct dcam_dev* param)
{
	dcam_reg_isr(DCAM_PATH2_DONE,   sprd_img_tx_done,  param);
	dcam_reg_isr(DCAM_PATH2_OV,     sprd_img_tx_error, param);

	return 0;
}

LOCAL int sprd_img_unreg_isr(struct dcam_dev* param)
{
	dcam_reg_isr(DCAM_PATH0_DONE,   NULL, param);
	dcam_reg_isr(DCAM_PATH1_DONE,   NULL, param);
	dcam_reg_isr(DCAM_PATH0_OV,     NULL, param);
	dcam_reg_isr(DCAM_PATH1_OV,     NULL, param);
	dcam_reg_isr(DCAM_ISP_OV,       NULL, param);
	dcam_reg_isr(DCAM_MIPI_OV,      NULL, param);
	dcam_reg_isr(DCAM_SN_LINE_ERR,  NULL, param);
	dcam_reg_isr(DCAM_SN_FRAME_ERR, NULL, param);
	dcam_reg_isr(DCAM_JPEG_BUF_OV,  NULL, param);
	dcam_reg_isr(DCAM_SN_EOF,       NULL,   param);
	dcam_reg_isr(DCAM_PATH1_SOF,    NULL,    param);

	return 0;
}

LOCAL int sprd_img_unreg_path2_isr(struct dcam_dev* param)
{
	dcam_reg_isr(DCAM_PATH2_DONE,   NULL,  NULL);
	dcam_reg_isr(DCAM_PATH2_OV,     NULL,  NULL);

	return 0;
}

LOCAL int sprd_img_path0_cfg(path_cfg_func path_cfg,
				struct dcam_path_spec* path_spec)
{
	int                      ret = DCAM_RTN_SUCCESS;
	uint32_t                 param, i;

	if (NULL == path_cfg || NULL == path_spec)
		return -EINVAL;

	if (path_spec->is_from_isp) {
		param = 1;
	} else {
		param = 0;
	}
	ret = path_cfg(DCAM_PATH_SRC_SEL, &param);
	/*IMG_RTN_IF_ERR(ret);*/

	ret = path_cfg(DCAM_PATH_ROT_MODE, &path_spec->rot_mode);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_INPUT_SIZE, &path_spec->in_size);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_INPUT_RECT, &path_spec->in_rect);
	/*IMG_RTN_IF_ERR(ret);*/

	ret = path_cfg(DCAM_PATH_OUTPUT_FORMAT, &path_spec->out_fmt);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_FRAME_BASE_ID, &path_spec->frm_id_base);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_FRAME_TYPE, &path_spec->frm_type);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_DATA_ENDIAN, &path_spec->end_sel);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_FRM_DECI, &path_spec->path_frm_deci);
	IMG_RTN_IF_ERR(ret);

	ret = sprd_img_path_cfg_output_addr(path_cfg, path_spec);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_OUTPUT_RESERVED_ADDR, &path_spec->frm_reserved_addr);

	param = 1;
	ret = path_cfg(DCAM_PATH_ENABLE, &param);

exit:
	return ret;
}

LOCAL int sprd_img_path_cfg(path_cfg_func path_cfg,
				struct dcam_path_spec* path_spec)
{
	int                      ret = DCAM_RTN_SUCCESS;
	uint32_t                 param, i;

	if (NULL == path_cfg || NULL == path_spec)
		return -EINVAL;

	if (path_spec->is_from_isp) {
		param = 1;
	} else {
		param = 0;
	}
	ret = path_cfg(DCAM_PATH_SRC_SEL, &param);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_ROT_MODE, &path_spec->rot_mode);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_INPUT_SIZE, &path_spec->in_size);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_INPUT_RECT, &path_spec->in_rect);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_OUTPUT_SIZE, &path_spec->out_size);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_OUTPUT_FORMAT, &path_spec->out_fmt);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_FRAME_BASE_ID, &path_spec->frm_id_base);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_FRAME_TYPE, &path_spec->frm_type);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_DATA_ENDIAN, &path_spec->end_sel);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_FRM_DECI, &path_spec->path_frm_deci);
	IMG_RTN_IF_ERR(ret);

	ret = sprd_img_path_cfg_output_addr(path_cfg, path_spec);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_OUTPUT_RESERVED_ADDR, &path_spec->frm_reserved_addr);

	ret = path_cfg(DCAM_PATH_SHRINK, &path_spec->shrink);

	param = 1;
	ret = path_cfg(DCAM_PATH_ENABLE, &param);

exit:
	return ret;
}

LOCAL int sprd_img_path_lightly_cfg(path_cfg_func path_cfg,
				struct dcam_path_spec* path_spec)
{
	int                      ret = DCAM_RTN_SUCCESS;
	uint32_t                 param;

	if (NULL == path_cfg || NULL == path_spec)
		return -EINVAL;

	if (path_spec->is_from_isp) {
		param = 1;
	} else {
		param = 0;
	}
	ret = path_cfg(DCAM_PATH_SRC_SEL, &param);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_INPUT_SIZE, &path_spec->in_size);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_INPUT_RECT, &path_spec->in_rect);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_OUTPUT_SIZE, &path_spec->out_size);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_OUTPUT_FORMAT, &path_spec->out_fmt);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_FRAME_BASE_ID, &path_spec->frm_id_base);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_FRAME_TYPE, &path_spec->frm_type);
	IMG_RTN_IF_ERR(ret);

	ret = path_cfg(DCAM_PATH_DATA_ENDIAN, &path_spec->end_sel);
	IMG_RTN_IF_ERR(ret);

exit:
	return ret;
}


LOCAL int sprd_img_local_deinit(struct dcam_dev *dev)
{
	int                                     ret = 0;
	struct dcam_path_spec    *path = &dev->dcam_cxt.dcam_path[DCAM_PATH1];
	printk("SPRD_IMG: sprd_img_local_deinit, frm_cnt_act %d \n", path->frm_cnt_act);
	if (unlikely(NULL == dev || NULL == path)) {
		return -EINVAL;
	}
	path->is_work = 0;
	path->frm_cnt_act = 0;
	memset((void*)&path->frm_reserved_addr, 0, sizeof(struct dcam_addr));
	sprd_img_buf_queue_init(&path->buf_queue);

	path = &dev->dcam_cxt.dcam_path[DCAM_PATH2];
	if (unlikely(NULL == path))
		return -EINVAL;
	path->is_work = 0;
	path->frm_cnt_act = 0;
	memset((void*)&path->frm_reserved_addr, 0, sizeof(struct dcam_addr));
	sprd_img_buf_queue_init(&path->buf_queue);

	path = &dev->dcam_cxt.dcam_path[DCAM_PATH0];
	if (unlikely(NULL == path))
		return -EINVAL;
	path->is_work = 0;
	path->frm_cnt_act = 0;
	memset((void*)&path->frm_reserved_addr, 0, sizeof(struct dcam_addr));
	sprd_img_buf_queue_init(&path->buf_queue);

	ret = sprd_img_queue_init(&dev->queue);
	if (unlikely(ret != 0)) {
		printk("SPRD_IMG: Failed to init queue \n");
		return -EINVAL;
	}

	printk("SPRD_IMG: sprd_img_local_deinit, frm_cnt_act %d \n", path->frm_cnt_act);

	return 0;
}

LOCAL int sprd_img_queue_init(struct dcam_queue *queue)
{
	if (NULL == queue)
		return -EINVAL;

	memset(queue, 0, sizeof(*queue));
	queue->write = &queue->node[0];
	queue->read  = &queue->node[0];

	return 0;
}

LOCAL int sprd_img_queue_write(struct dcam_queue *queue, struct dcam_node *node)
{
	struct dcam_node         *ori_node;
	unsigned long                 lock_flag;

	if (NULL == queue || NULL == node)
		return -EINVAL;

	if ( IMG_TX_STOP == node->irq_flag) {
		printk("SPRD_IMG IMG_TX_STOP\n");
	}
	ori_node = queue->write;
	queue->wcnt++;
	DCAM_TRACE("SPRD_IMG: sprd_img_queue_write \n");
	*queue->write++ = *node;
	if (queue->write > &queue->node[DCAM_QUEUE_LENGTH-1]) {
		queue->write = &queue->node[0];
	}

	if (queue->write == queue->read) {
		if ( IMG_TX_STOP != node->irq_flag) {
			queue->write = ori_node;
		} else {
			queue->read = ori_node;
		}
		printk("SPRD_IMG: warning, queue is full, cannot write, flag 0x%x type 0x%x index 0x%x wcht %d %d\n",
			node->irq_flag, node->f_type, node->index, queue->wcnt, queue->rcnt);
	}

	return 0;
}

LOCAL int sprd_img_queue_read(struct dcam_queue *queue, struct dcam_node *node)
{
	int                      ret = DCAM_RTN_SUCCESS;
	int                      flag = 0;

	if (NULL == queue || NULL == node)
		return -EINVAL;

	DCAM_TRACE("SPRD_IMG: sprd_img_queue_read \n");

	if  (queue->read != queue->write) {
		flag = 1;
		*node = *queue->read;
		if ( IMG_TX_STOP == node->irq_flag) {
			printk("DCAM stop thr\n");
		}
		queue->read->yaddr = 0;
		queue->read->yaddr_vir = 0;
		queue->read++;
		queue->rcnt++;
		if (queue->read > &queue->node[DCAM_QUEUE_LENGTH-1]) {
			queue->read = &queue->node[0];
		}
	}
	if (!flag) {
		ret = EAGAIN;
	}
	DCAM_TRACE("SPRD_IMG: sprd_img_queue_read type %d \n", node->f_type);
	return ret;
}

LOCAL int sprd_img_queue_disable(struct dcam_queue *queue, uint32_t channel_id)
{
	struct dcam_node            *cur_node;

	if (NULL == queue)
		return -EINVAL;

	cur_node = queue->read;
	 while (cur_node != queue->write) {
		if (channel_id == cur_node->f_type) {
			cur_node->invalid_flag = 1;
		}
		if (cur_node >= &queue->node[DCAM_QUEUE_LENGTH-1]) {
			cur_node = &queue->node[0];
		} else {
			cur_node++;
		}
	}


	return 0;
}

LOCAL int sprd_img_queue_enable(struct dcam_queue *queue, uint32_t channel_id)
{
	struct dcam_node            *cur_node;
	unsigned int                    i = 0;

	if (NULL == queue)
		return -EINVAL;

	for (i=0 ; i < DCAM_QUEUE_LENGTH ; i++) {
			queue->node[i].invalid_flag = 0;
	}

	return 0;
}

LOCAL int sprd_img_buf_queue_init(struct dcam_img_buf_queue *queue)
{
	if (NULL == queue)
		return -EINVAL;

	printk("SPRD_IMG: sprd_img_buf_queue_init \n");
	memset(queue, 0, sizeof(*queue));
	queue->write = &queue->buf_addr[0];
	queue->read  = &queue->buf_addr[0];

	return 0;
}

LOCAL int sprd_img_buf_queue_write(struct dcam_img_buf_queue *queue, struct dcam_img_buf_addr *buf_addr)
{
	struct dcam_img_buf_addr         *ori_node;

	if (NULL == queue || NULL == buf_addr)
		return -EINVAL;

	ori_node = queue->write;
	DCAM_TRACE("SPRD_IMG: sprd_img_buf_queue_write \n");
	*queue->write++ = *buf_addr;
	queue->wcnt++;
	if (queue->write > &queue->buf_addr[DCAM_FRM_CNT_MAX-1]) {
		queue->write = &queue->buf_addr[0];
	}

	if (queue->write == queue->read) {
		queue->write = ori_node;
		printk("SPRD_IMG: warning, queue is full, cannot write %d %d", queue->wcnt, queue->rcnt);
		return -EINVAL;
	}

	return 0;
}

LOCAL int sprd_img_buf_queue_read(struct dcam_img_buf_queue *queue, struct dcam_img_buf_addr *buf_addr)
{
	int                      ret = DCAM_RTN_SUCCESS;

	if (NULL == queue || NULL == buf_addr)
		return -EINVAL;

	DCAM_TRACE("SPRD_IMG: sprd_img_buf_queue_read \n");
	if (queue->read != queue->write) {
		*buf_addr = *queue->read++;
		queue->rcnt++;
		if (queue->read > &queue->buf_addr[DCAM_FRM_CNT_MAX-1]) {
			queue->read = &queue->buf_addr[0];
		}
	} else {
		printk("SPRD_IMG: buf_queue_read fail %d\n", queue->rcnt);
		ret = EAGAIN;
	}

	DCAM_TRACE("SPRD_IMG: sprd_img_buf_queue_read \n");
	return ret;
}

LOCAL int sprd_img_path_cfg_output_addr(path_cfg_func path_cfg, struct dcam_path_spec* path_spec)
{
	int                         ret = DCAM_RTN_SUCCESS;
	struct dcam_img_buf_addr    *cur_node;
	struct dcam_img_buf_queue   *queue;
	struct dcam_addr            frm_addr;

	if (NULL == path_cfg || NULL == path_spec)
		return -EINVAL;

	queue = &path_spec->buf_queue;

	for (cur_node = queue->read; cur_node != queue->write; cur_node++) {
		if (cur_node > &queue->buf_addr[DCAM_FRM_CNT_MAX-1]) {
			cur_node = &queue->buf_addr[0];
		}
		frm_addr.yaddr = cur_node->frm_addr.yaddr;
		frm_addr.uaddr = cur_node->frm_addr.uaddr;
		frm_addr.vaddr = cur_node->frm_addr.vaddr;
		frm_addr.yaddr_vir = cur_node->frm_addr_vir.yaddr;
		frm_addr.uaddr_vir = cur_node->frm_addr_vir.uaddr;
		frm_addr.vaddr_vir = cur_node->frm_addr_vir.vaddr;
		frm_addr.zsl_private = cur_node->frm_addr_vir.zsl_private;
		ret = path_cfg(DCAM_PATH_OUTPUT_ADDR, &frm_addr);
		IMG_RTN_IF_ERR(ret);
	}

exit:
	return ret;
}


LOCAL int sprd_img_get_path_index(uint32_t channel_id)
{
	int path_index;

	switch (channel_id) {
	case DCAM_PATH0:
		path_index = DCAM_PATH_IDX_0;
		break;
	case DCAM_PATH1:
		path_index = DCAM_PATH_IDX_1;
		break;
	case DCAM_PATH2:
		path_index = DCAM_PATH_IDX_2;
		break;
	default:
		path_index = DCAM_PATH_IDX_NONE;
		printk("SPRD_IMG: get path index error, invalid channel_id=0x%x \n", channel_id);
	}
	return path_index;
}

LOCAL int img_get_zoom_rect(struct dcam_rect *src_rect, struct dcam_rect *dst_rect, struct dcam_rect *output_rect, uint32_t zoom_level)
{
	uint32_t trim_width = 0;
	uint32_t trim_height = 0;
	uint32_t zoom_step_w = 0, zoom_step_h = 0;

	if (NULL == src_rect || NULL == dst_rect || NULL == output_rect) {
		printk("SPRD_IMG: img_get_zoom_rect: %p, %p, %p\n", src_rect, dst_rect, output_rect);
		return -EINVAL;
	}

	if (0 == dst_rect->w || 0 == dst_rect->h) {
		printk("SPRD_IMG: img_get_zoom_rect: dst0x%x, 0x%x\n", dst_rect->w, dst_rect->h);
		return -EINVAL;
	}

	if (src_rect->w > dst_rect->w && src_rect->h > dst_rect->h) {
		zoom_step_w = DCAM_ZOOM_STEP(src_rect->w, dst_rect->w);
		zoom_step_w &= ~1;
		zoom_step_w *= zoom_level;
		trim_width = src_rect->w - zoom_step_w;

		zoom_step_h = DCAM_ZOOM_STEP(src_rect->h, dst_rect->h);
		zoom_step_h &= ~1;
		zoom_step_h *= zoom_level;
		trim_height = src_rect->h - zoom_step_h;

		output_rect->x = src_rect->x + ((src_rect->w - trim_width) >> 1);
		output_rect->y = src_rect->y + ((src_rect->h - trim_height) >> 1);
	} else if (src_rect->w < dst_rect->w && src_rect->h < dst_rect->h) {
		zoom_step_w = DCAM_ZOOM_STEP(dst_rect->w, src_rect->w);
		zoom_step_w &= ~1;
		zoom_step_w *= zoom_level;
		trim_width = src_rect->w + zoom_step_w;

		zoom_step_h = DCAM_ZOOM_STEP(dst_rect->h, src_rect->h);
		zoom_step_h &= ~1;
		zoom_step_h *= zoom_level;
		trim_height = src_rect->h + zoom_step_h;

		output_rect->x = src_rect->x - ((trim_width - src_rect->w) >> 1);
		output_rect->y = src_rect->y - ((trim_height - src_rect->h) >> 1);
	} else {
		printk("SPRD_IMG: SPRD_IMG img_get_zoom_rect: param error\n");
		return -EINVAL;
	}

	output_rect->x = DCAM_WIDTH(output_rect->x);
	output_rect->y = DCAM_HEIGHT(output_rect->y);
	output_rect->w = DCAM_WIDTH(trim_width);
	output_rect->h = DCAM_HEIGHT(trim_height);
	DCAM_TRACE("SPRD_IMG: zoom_level %d, trim rect, %d %d %d %d\n",
		zoom_level,
		output_rect->x,
		output_rect->y,
		output_rect->w,
		output_rect->h);

	return 0;
}

LOCAL int sprd_img_start_zoom(struct dcam_frame *frame, void* param)
{
	struct dcam_dev          *dev = (struct dcam_dev*)param;

	if (dev == NULL) {
		DCAM_TRACE("SPRD_IMG: sprd_img_start_zoom, dev is NULL \n");
		return -1;
	}
	DCAM_TRACE("SPRD_IMG: start zoom level %d \n", dev->zoom_level);
	if (dev->zoom_level <= DCAM_ZOOM_LEVEL_MAX && dev->dcam_cxt.is_smooth_zoom) {
		up(&dev->zoom_thread_sem);
	} else {
		dcam_stop_sc_coeff();
	}

	return 0;
}

int sprd_img_zoom_thread_loop(void *arg)
{
	struct dcam_dev          *dev = (struct dcam_dev*)arg;
	int                      ret = DCAM_RTN_SUCCESS;
	struct dcam_rect         zoom_rect = {0};
	struct dcam_path_spec    *path = NULL;
	enum dcam_path_index     path_index;

	if (dev == NULL) {
		printk("SPRD_IMG: zoom_thread_loop, dev is NULL \n");
		return -1;
	}
	while (1) {
		if (0 == down_interruptible(&dev->zoom_thread_sem)) {
			DCAM_TRACE("SPRD_IMG: zoom thread level %d \n", dev->zoom_level);
			if (dev->is_zoom_thread_stop) {
				DCAM_TRACE("SPRD_IMG: zoom_thread_loop stop \n");
				break;
			}

			if (dev->zoom_level > DCAM_ZOOM_LEVEL_MAX) {
				continue;
			}
			mutex_lock(&dev->dcam_mutex);
			path = &dev->dcam_cxt.dcam_path[dev->channel_id];
			path_index = sprd_img_get_path_index(dev->channel_id);
// J3 LTE & J3 3G will not use the Spreadtrum's zoom calculation path because of 400 step zoom issue (Unsmooth)
#if defined(CONFIG_MACH_J3XLTE) || defined(CONFIG_MACH_J3X3G) || defined(CONFIG_MACH_J1MINI3G) || defined(CONFIG_MACH_GTEXSWIFI)

			dcam_update_path(path_index, &path->in_size, &path->in_rect, &path->out_size);
			memcpy((void*)&path->in_rect_backup, (void*)&path->in_rect, sizeof(struct dcam_rect));
			memcpy((void*)&path->in_rect_current, (void*)&path->in_rect_backup, sizeof(struct dcam_rect));
#else
			if (dev->zoom_level < DCAM_ZOOM_LEVEL_MAX) {
				ret = img_get_zoom_rect(&path->in_rect_backup, &path->in_rect, &zoom_rect, dev->zoom_level);
				if (!ret && path->in_rect_current.x != zoom_rect.x
					&& path->in_rect_current.y != zoom_rect.y
					&& path->in_rect_current.w != zoom_rect.w
					&& path->in_rect_current.h != zoom_rect.h) {
					memcpy((void*)&path->in_rect_current, (void*)&zoom_rect, sizeof(struct dcam_rect));
					dcam_update_path(path_index, &path->in_size, &zoom_rect, &path->out_size);
				}
			} else {
				dcam_update_path(path_index, &path->in_size, &path->in_rect, &path->out_size);
				memcpy((void*)&path->in_rect_backup, (void*)&path->in_rect, sizeof(struct dcam_rect));
				memcpy((void*)&path->in_rect_current, (void*)&path->in_rect_backup, sizeof(struct dcam_rect));
			}
#endif
			DCAM_TRACE("SPRD_IMG: thread level %d, in_size{%d %d}, in_rect{%d %d %d %d}, in_rect_backup{%d %d %d %d}, in_rect_current{%d %d %d %d}, out_size{%d %d}\n",
						dev->zoom_level,
						path->in_size.w,
						path->in_size.h,
						path->in_rect.x,
						path->in_rect.y,
						path->in_rect.w,
						path->in_rect.h,
						path->in_rect_backup.x,
						path->in_rect_backup.y,
						path->in_rect_backup.w,
						path->in_rect_backup.h,
						path->in_rect_current.x,
						path->in_rect_current.y,
						path->in_rect_current.w,
						path->in_rect_current.h,
						path->out_size.w,
						path->out_size.h);
			dev->zoom_level++;
			mutex_unlock(&dev->dcam_mutex);
			DCAM_TRACE("SPRD_IMG: zoom thread level  %d  end \n", dev->zoom_level);

		} else {
			printk("SPRD_IMG: zoom int!");
			break;
		}
	}
	dev->is_zoom_thread_stop = 0;

	return 0;
}

int sprd_img_create_zoom_thread(void* param)
{
	struct dcam_dev          *dev = (struct dcam_dev*)param;

	if (dev == NULL) {
		DCAM_TRACE("create_zoom_thread, dev is NULL \n");
		return -1;
	}
	printk("SPRD_IMG: create_zoom_thread E!\n");

	dev->is_zoom_thread_stop = 0;
	dev->zoom_level = DCAM_ZOOM_LEVEL_MAX + 1;
	sema_init(&dev->zoom_thread_sem, 0);
	dev->zoom_thread = kthread_run(sprd_img_zoom_thread_loop, param, "img_zoom_thread");
	if (IS_ERR(dev->zoom_thread)) {
		printk("SPRD_IMG: create_zoom_thread error!\n");
		return -1;
	}
	return 0;
}

int sprd_img_stop_zoom_thread(void* param)
{
	struct dcam_dev          *dev = (struct dcam_dev*)param;
	int cnt = 0;

	if (dev == NULL) {
		DCAM_TRACE("stop_zoom_thread, dev is NULL \n");
		return -1;
	}
	DCAM_TRACE("SPRD_IMG: stop_zoom_thread E!\n");
	if (dev->zoom_thread) {
		dev->is_zoom_thread_stop = 1;
		up(&dev->zoom_thread_sem);
		if (dev->is_zoom_thread_stop != 0) {
			while (cnt < 500) {
				cnt++;
				if (0 == dev->is_zoom_thread_stop)
					break;
				msleep(1);
			}
		}
		dev->zoom_thread = NULL;
	}

	return 0;
}

LOCAL int sprd_img_update_video(struct file *file, uint32_t channel_id)
{
	struct dcam_dev          *dev = file->private_data;
	int                      ret = DCAM_RTN_SUCCESS;
	struct dcam_path_spec    *path = NULL;
	path_cfg_func            path_cfg;
	enum dcam_path_index     path_index;


	DCAM_TRACE("SPRD_IMG: sprd_img_update_video, channel=%d \n", channel_id);

	mutex_lock(&dev->dcam_mutex);

	path = &dev->dcam_cxt.dcam_path[channel_id];
	path_index = sprd_img_get_path_index(channel_id);

	if (DCAM_PATH0 == channel_id) {
		path_cfg = dcam_path0_cfg;
	} else if (DCAM_PATH1 == channel_id) {
		path_cfg = dcam_path1_cfg;
	}else if (DCAM_PATH2 == channel_id) {
		path_cfg = dcam_path2_cfg;
	}

	if (dev->dcam_cxt.is_smooth_zoom && DCAM_PATH1 == channel_id) {
		dev->zoom_level = 1;
		dev->channel_id = channel_id;
		if (0 == path->in_rect_backup.w || 0 == path->in_rect_backup.h) {
			path->in_rect_backup.x = 0;
			path->in_rect_backup.y = 0;
			path->in_rect_backup.w = path->in_size.w;
			path->in_rect_backup.h = path->in_size.h;
			memcpy((void*)&path->in_rect_current, (void*)&path->in_rect_backup, sizeof(struct dcam_rect));
		} else {
			memcpy((void*)&path->in_rect_backup, (void*)&path->in_rect_current, sizeof(struct dcam_rect));
		}

		DCAM_TRACE("SPRD_IMG: in_size{%d %d}, in_rect{%d %d %d %d}, in_rect_backup{%d %d %d %d}, in_rect_current{%d %d %d %d}, out_size{%d %d}\n",
				path->in_size.w,
				path->in_size.h,
				path->in_rect.x,
				path->in_rect.y,
				path->in_rect.w,
				path->in_rect.h,
				path->in_rect_backup.x,
				path->in_rect_backup.y,
				path->in_rect_backup.w,
				path->in_rect_backup.h,
				path->in_rect_current.x,
				path->in_rect_current.y,
				path->in_rect_current.w,
				path->in_rect_current.h,
				path->out_size.w,
				path->out_size.h);
	} else {
		DCAM_TRACE("SPRD_IMG: in_size{%d %d}, in_rect{%d %d %d %d}, out_size{%d %d}\n",
				path->in_size.w,
				path->in_size.h,
				path->in_rect.x,
				path->in_rect.y,
				path->in_rect.w,
				path->in_rect.h,
				path->out_size.w,
				path->out_size.h);
		ret = dcam_update_path(path_index, &path->in_size, &path->in_rect, &path->out_size);
	}

	mutex_unlock(&dev->dcam_mutex);
	DCAM_TRACE("SPRD_IMG: update video 0x%x \n", ret);
	if (ret) {
		printk("SPRD_IMG: Failed to update video 0x%x \n", ret);
	}

	return ret;
}

LOCAL int sprd_img_streampause(struct file *file, uint32_t channel_id, uint32_t reconfig_flag)
{
	struct dcam_dev          *dev = file->private_data;
	struct dcam_path_spec    *path = NULL;
	int                      ret = 0;
	enum dcam_path_index     path_index;

	printk("SPRD_IMG: pause, channel %d ,recfg flag %d\n", channel_id, reconfig_flag);

	path = &dev->dcam_cxt.dcam_path[channel_id];
	path_index = sprd_img_get_path_index(channel_id);

	if (PATH_RUN == path->status) {
		path->status = PATH_IDLE;
		ret = dcam_stop_path(path_index);
		IMG_PRINT_IF_ERR(ret);
		if ((reconfig_flag)/* && (DCAM_PATH2 == channel_id)*/) {
			//path->is_work = 0;
			path->frm_cnt_act = 0;
			sprd_img_buf_queue_init(&path->buf_queue);
			sprd_img_queue_disable(&dev->queue, channel_id);
			ret = sprd_img_unreg_path2_isr(dev);
			IMG_PRINT_IF_ERR(ret);
		}

		if (DCAM_PATH2 == channel_id && dev->got_resizer) {
			dcam_rel_resizer();
			dev->got_resizer = 0;
		}
		printk("SPRD_IMG: pause, channel=%d done \n", channel_id);
	} else {
		printk("SPRD_IMG: pause, path %d not running, status=%d \n",
			channel_id, path->status);
	}

	return ret;
}

LOCAL int sprd_img_streamresume(struct file *file, uint32_t channel_id)
{
	struct dcam_dev          *dev = NULL;
	struct dcam_path_spec    *path = NULL;
	enum dcam_path_index     path_index;
	path_cfg_func            path_cfg;
	int                      ret = 0;
	int                      on_flag = 1;

	DCAM_TRACE("SPRD_IMG: resume, channel=%d \n", channel_id);

	dev = file->private_data;
	if (!dev) {
		printk("SPRD_IMG: dev is NULL \n");
		return -EFAULT;
	}

	path = &dev->dcam_cxt.dcam_path[channel_id];
	path_index = sprd_img_get_path_index(channel_id);

	if (unlikely(0 == atomic_read(&dev->stream_on))) {
		printk("SPRD_IMG: resume stream not on\n");
		ret = sprd_img_local_deinit(dev);
		if (unlikely(ret)) {
			printk(DEBUG_STR,DEBUG_ARGS);
		}
		on_flag = 0;
	}
	if (PATH_IDLE == path->status && on_flag) {
		if (path->is_work) {
			if (DCAM_PATH0 == channel_id) {
				path_cfg = dcam_path0_cfg;
			} else if (DCAM_PATH1 == channel_id) {
				path_cfg = dcam_path1_cfg;
			} else if (DCAM_PATH2 == channel_id) {
				if (0 == dev->got_resizer) {
					 /* if not owned resiezer, try to get it */
					if (unlikely(dcam_get_resizer(DCAM_WAIT_FOREVER))) {
						/*no wait to get the controller of resizer, failed*/
						printk("SPRD_IMG: resume, path2 has been occupied by other app \n");
						return -EIO;
					}
					dev->got_resizer = 1;
				}
				ret = sprd_img_reg_path2_isr(dev);
				IMG_PRINT_IF_ERR(ret);
				path_cfg = dcam_path2_cfg;
			} else {
				printk("SPRD_IMG: resume, invalid channel_id=0x%x \n", channel_id);
				return -EINVAL;
			}

			if (DCAM_PATH0 == channel_id) {
				ret = sprd_img_path0_cfg(path_cfg, path);
			} else {
				ret = sprd_img_path_cfg(path_cfg, path);
			}
			sprd_img_queue_enable(&dev->queue, channel_id);
			IMG_RTN_IF_ERR(ret);
			ret = dcam_start_path(path_index);
			IMG_PRINT_IF_ERR(ret);

			path->status = PATH_RUN;
		}else{
			DCAM_TRACE("SPRD_IMG: resume, path %d no parameter, is_work=%d, cannot resume \n",
				channel_id, path->is_work);
		}
	}else{
		DCAM_TRACE("SPRD_IMG: resume, path %d not idle, status=%d, cannot resume \n",
			channel_id, path->status);
	}
exit:
	if (ret) {
		DCAM_TRACE("SPRD_IMG: fail resume, path %d, ret = 0x%x\n", channel_id, ret);
	}
	return ret;
}

LOCAL void sprd_img_print_reg(void)
{
	uint32_t*                reg_buf = NULL;
	uint32_t                 reg_buf_len = 0x400;
	int                      ret;
	uint32_t                 print_len = 0, print_cnt = 0;

	reg_buf = (uint32_t*)vzalloc(reg_buf_len);
	if (NULL == reg_buf)
		return;

	ret = dcam_read_registers(reg_buf, &reg_buf_len);
	if (ret) {
		vfree(reg_buf);
		return;
	}

	mm_clk_register_trace();
	printk("dcam registers \n");
	while (print_len < reg_buf_len) {
		printk("offset 0x%03x : 0x%08x, 0x%08x, 0x%08x, 0x%08x \n",
			print_len,
			reg_buf[print_cnt],
			reg_buf[print_cnt+1],
			reg_buf[print_cnt+2],
			reg_buf[print_cnt+3]);
		print_cnt += 4;
		print_len += 16;
	}
#if 0
	ret = csi_read_registers(reg_buf, &reg_buf_len);
	if (ret) {
		vfree(reg_buf);
		return;
	}

	print_len = 0;
	print_cnt = 0;
	printk("csi registers \n");
	while (print_len < reg_buf_len) {
		printk("offset 0x%3x : 0x%8x, 0x%8x, 0x%8x, 0x%8x \n",
			print_len,
			reg_buf[print_cnt],
			reg_buf[print_cnt+1],
			reg_buf[print_cnt+2],
			reg_buf[print_cnt+3]);
		print_cnt += 4;
		print_len += 16;
	}
#endif
	udelay(1);
	vfree(reg_buf);

	return;

}
LOCAL void sprd_timer_callback(unsigned long data)
{
	struct dcam_dev          *dev = (struct dcam_dev*)data;
	struct dcam_node         node;
	int                      ret = 0;

	DCAM_TRACE("SPRD_IMG: sprd_timer_callback \n");

	if (0 == data || 0 == atomic_read(&dev->stream_on)) {
		printk("SPRD_IMG: timer cb error \n");
		return;
	}

	if (0 == atomic_read(&dev->run_flag)) {
		printk("DCAM timeout.\n");
		node.irq_flag = IMG_TIMEOUT;
		node.invalid_flag = 0;
		ret = sprd_img_queue_write(&dev->queue, &node);
		if (ret) {
			printk("SPRD_IMG: timer cb write queue error \n");
		}
		up(&dev->irq_sem);
	}
}
LOCAL int sprd_init_timer(struct timer_list *dcam_timer, unsigned long data)
{
	int                      ret = 0;

	setup_timer(dcam_timer, sprd_timer_callback, data);
	return ret;
}
LOCAL int sprd_start_timer(struct timer_list *dcam_timer, uint32_t time_val)
{
	int                      ret = 0;

	DCAM_TRACE("SPRD_IMG: starting timer %ld \n",jiffies);
	ret = mod_timer(dcam_timer, jiffies + msecs_to_jiffies(time_val));
	if (ret) {
		printk("SPRD_IMG: Error in mod_timer %d \n",ret);
	}
	return ret;
}

LOCAL int sprd_stop_timer(struct timer_list *dcam_timer)
{
	int                      ret = 0;

	DCAM_TRACE("SPRD_IMG: stop timer \n");
	del_timer_sync(dcam_timer);
	return ret;
}

LOCAL int sprd_init_handle(struct dcam_dev *dev)
{
	struct dcam_info         *info = &dev->dcam_cxt;
	struct dcam_path_spec    *path;
	uint32_t                 i = 0;

	if (NULL == info) {
		printk("SPRD_IMG: init handle fail \n");
		return -EINVAL;
	}
	info->flash_status = FLASH_STATUS_MAX;
	info->after_af = 0;
	for (i = 0; i < DCAM_PATH_MAX; i++) {
		path = &info->dcam_path[i];
		if (NULL == path) {
			printk("SPRD_IMG: init path %d fail \n", i);
			return -EINVAL;
		}
		memset((void*)path->frm_ptr,
			0,
			(uint32_t)(DCAM_FRM_CNT_MAX * sizeof(struct dcam_frame*)));
		path->frm_cnt_act = 0;
		sprd_img_buf_queue_init(&path->buf_queue);
		path->status = PATH_IDLE;
	}
	atomic_set(&dev->stream_on, 0);
	dev->got_resizer = 0;
	return 0;
}

LOCAL int sprd_img_k_open(struct inode *node, struct file *file)
{
	struct dcam_dev          *dev = NULL;
	struct miscdevice        *md = file->private_data;
	int                      ret = 0;

	dev = vzalloc(sizeof(*dev));
	if (!dev) {
		ret = -ENOMEM;
		printk("sprd_img_k_open fail alloc\n");
		return ret;
	}

	mutex_init(&dev->dcam_mutex);
	sema_init(&dev->irq_sem, 0);

	if (unlikely(atomic_inc_return(&dev->users) > 1)) {
		ret = -EBUSY;
		goto exit;
	}

	DCAM_TRACE("sprd_img_k_open \n");
	ret = dcam_module_en(md->this_device->of_node);
	if (unlikely(ret != 0)) {
		printk("SPRD_IMG: Failed to enable dcam module \n");
		ret = -EIO;
		goto exit;
	}

	ret = sprd_img_queue_init(&dev->queue);
	if (unlikely(ret != 0)) {
		printk("SPRD_IMG: Failed to init queue \n");
		ret = -EIO;
		goto exit;
	}

	ret = sprd_init_timer(&dev->dcam_timer,(unsigned long)dev);
	if (unlikely(ret != 0)) {
		printk("SPRD_IMG: Failed to init timer \n");
		ret = -EIO;
		goto exit;
	}

	ret = sprd_init_handle(dev);
	if (unlikely(ret != 0)) {
		printk("SPRD_IMG: Failed to init queue \n");
		ret = -EIO;
		goto exit;
	}

	ret = dcam_create_flash_thread(dev);
	if (unlikely(ret != 0)) {
		printk("SPRD_IMG: Failed to create flash thread \n");
		ret = -EIO;
		goto exit;
	}

	ret = sprd_img_create_zoom_thread(dev);
	if (unlikely(ret != 0)) {
		printk("SPRD_IMG: Failed to create zoom thread \n");
		ret = -EIO;
		goto exit;
	}

	dev->driver_data = (void*)md;
	file->private_data = (void*)dev;

exit:
	if (unlikely(ret)) {
		atomic_dec(&dev->users);
	} else {
#if 0
		dev->proc_file = create_proc_read_entry(DCAM_PROC_FILE_NAME,
						0444,
						NULL,
						sprd_img_proc_read,
						(void*)dev);
		if (unlikely(NULL == dev->proc_file)) {
			printk("SPRD_IMG: Can't create an entry for video0 in /proc \n");
			ret = ENOMEM;
		}
#endif
	}

	DCAM_TRACE("sprd_img_k_open %d \n", ret);
	return ret;
}

static int sprd_img_k_release(struct inode *node, struct file *file)
{
	struct dcam_dev 		 *dev = NULL;
	struct miscdevice        *md = NULL;
	int 					 ret = 0;

	DCAM_TRACE("SPRD_IMG: Close start\n");
#if 0
	if (dev->proc_file) {
		DCAM_TRACE("SPRD_IMG: sprd_img_k_release \n");
		remove_proc_entry(DCAM_PROC_FILE_NAME, NULL);
		dev->proc_file = NULL;
	}
#endif

	dev = file->private_data;
	if (!dev)
		goto exit;

	md = dev->driver_data;

	mutex_lock(&dev->dcam_mutex);
	dcam_reset(DCAM_RST_ALL, 0);
	if (dev->got_resizer) {
		dcam_rel_resizer();
		dev->got_resizer = 0;
		sprd_img_unreg_path2_isr(dev);
	}

	atomic_set(&dev->stream_on, 0);
	dcam_module_deinit(dev->dcam_cxt.if_mode, dev->dcam_cxt.sn_mode);
	sprd_img_local_deinit(dev);
	sema_init(&dev->irq_sem, 0);
	ret = dcam_module_dis(md->this_device->of_node);
	if (unlikely(0 != ret)) {
		printk("SPRD_IMG: Failed to enable dcam module \n");
		ret = -EIO;
	}
	sprd_stop_timer(&dev->dcam_timer);
	atomic_dec(&dev->users);
	dcam_stop_flash_thread(dev);
	sprd_img_stop_zoom_thread(dev);
	mutex_unlock(&dev->dcam_mutex);

	vfree(dev);
	dev = NULL;
	file->private_data = NULL;

exit:
	DCAM_TRACE("SPRD_IMG: Close end.\n");
	return ret;
}

static long sprd_img_k_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct dcam_dev          *dev = NULL;
	struct dcam_info		 *info = NULL;
	uint32_t				 path_cnt;
	uint32_t                 channel_id;
	uint32_t                 mode;
	uint32_t                 skip_num;
	uint32_t                 flash_status;
	uint32_t                 zoom;
	struct sprd_img_parm     parm;
	struct sprd_img_size     size;
	struct sprd_img_rect     rect;
	struct dcam_path_spec    *path = NULL;
	struct dcam_rect         *input_rect;
	struct dcam_size         *input_size;
	path_cfg_func            path_cfg;
	int                      ret = DCAM_RTN_SUCCESS;

	DCAM_TRACE("sprd_img_k_ioctl: cmd: 0x%x \n", cmd);

	dev = file->private_data;
	if (!dev) {
		ret = -EFAULT;
		printk("sprd_img_k_ioctl: dev is NULL \n");
		goto exit;
	}

	info = &dev->dcam_cxt;

	switch (cmd) {
	case SPRD_IMG_IO_SET_MODE:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&mode, (uint32_t *)arg, sizeof(uint32_t));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.capture_mode = mode;
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("SPRD_IMG: capture mode %d \n", dev->dcam_cxt.capture_mode);
		break;

	case SPRD_IMG_IO_SET_SKIP_NUM:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&skip_num, (uint32_t *)arg, sizeof(uint32_t));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.skip_number = skip_num;
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("SPRD_IMG: cap skip number %d \n", dev->dcam_cxt.skip_number);
		break;

	case SPRD_IMG_IO_SET_SENSOR_SIZE:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&size, (struct sprd_img_size *)arg, sizeof(struct sprd_img_size));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.cap_in_size.w = size.w;
		dev->dcam_cxt.cap_in_size.h = size.h;
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("SPRD_IMG: sensor size %d %d \n",
			dev->dcam_cxt.cap_in_size.w,
			dev->dcam_cxt.cap_in_size.h);
		break;

	case SPRD_IMG_IO_SET_SENSOR_TRIM:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&rect, (struct sprd_img_rect *)arg, sizeof(struct sprd_img_rect));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.cap_in_rect.x = rect.x;
		dev->dcam_cxt.cap_in_rect.y = rect.y;
		dev->dcam_cxt.cap_in_rect.w = rect.w;
		dev->dcam_cxt.cap_in_rect.h = rect.h;

		dev->dcam_cxt.cap_out_size.w = dev->dcam_cxt.cap_in_rect.w;
		dev->dcam_cxt.cap_out_size.h = dev->dcam_cxt.cap_in_rect.h;
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("SPRD_IMG: sensor trim x y w h %d %d %d %d\n",
			dev->dcam_cxt.cap_in_rect.x,
			dev->dcam_cxt.cap_in_rect.y,
			dev->dcam_cxt.cap_in_rect.w,
			dev->dcam_cxt.cap_in_rect.h);
		break;

	case SPRD_IMG_IO_SET_FRM_ID_BASE:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (struct sprd_img_parm *)arg, sizeof(struct sprd_img_parm));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		switch (parm.channel_id) {
		case DCAM_PATH0:
		case DCAM_PATH1:
		case DCAM_PATH2:
			dev->dcam_cxt.dcam_path[parm.channel_id].frm_id_base = parm.frame_base_id;
			break;
		default:
			printk("SPRD_IMG: Wrong channel ID, %d  \n", parm.channel_id);
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("SPRD_IMG: channel %d, base id 0x%x \n",
			parm.channel_id,
			parm.frame_base_id);
		break;

	case SPRD_IMG_IO_SET_CROP:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (struct sprd_img_parm *)arg, sizeof(struct sprd_img_parm));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}

		if (unlikely(parm.crop_rect.x + parm.crop_rect.w > dev->dcam_cxt.cap_in_size.w ||
			parm.crop_rect.y + parm.crop_rect.h > dev->dcam_cxt.cap_in_size.h)) {
			mutex_unlock(&dev->dcam_mutex);
			ret = -EINVAL;
			goto exit;
		}

		DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_SET_CROP, window %d %d %d %d \n",
			parm.crop_rect.x,
			parm.crop_rect.y,
			parm.crop_rect.w,
			parm.crop_rect.h);

		switch (parm.channel_id) {
		case DCAM_PATH0:
		case DCAM_PATH1:
		case DCAM_PATH2:
			input_size = &dev->dcam_cxt.dcam_path[parm.channel_id].in_size;
			input_rect = &dev->dcam_cxt.dcam_path[parm.channel_id].in_rect;
			break;
		default:
			printk("SPRD_IMG: Wrong channel ID, %d  \n", parm.channel_id);
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}

		input_size->w = dev->dcam_cxt.cap_out_size.w;
		input_size->h = dev->dcam_cxt.cap_out_size.h;
		input_rect->x = parm.crop_rect.x;
		input_rect->y = parm.crop_rect.y;
		input_rect->w = parm.crop_rect.w;
		input_rect->h = parm.crop_rect.h;

		DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_SET_CROP, Path %d, cap crop: cap_rect %d %d %d %d, cap_out:%d %d \n",
				parm.channel_id, input_rect->x, input_rect->y,
				input_rect->w, input_rect->h,
				input_size->w, input_size->h);
		mutex_unlock(&dev->dcam_mutex);
		break;

	case SPRD_IMG_IO_SET_FLASH:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&flash_status, (uint32_t *)arg, sizeof(uint32_t));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.flash_status = flash_status;
		mutex_unlock(&dev->dcam_mutex);
		if ((dev->dcam_cxt.flash_status == FLASH_CLOSE_AFTER_OPEN)
			|| (dev->dcam_cxt.flash_status == FLASH_CLOSE)
			|| (dev->dcam_cxt.flash_status == FLASH_CLOSE_AFTER_AUTOFOCUS)) {
			up(&dev->flash_thread_sem);
		}
		DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_SET_FLASH, flash status %d \n", dev->dcam_cxt.flash_status);
		break;

	case SPRD_IMG_IO_SET_OUTPUT_SIZE:
		DCAM_TRACE("SPRD_IMG: set output size \n");
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (struct sprd_img_parm *)arg, sizeof(struct sprd_img_parm));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.dst_size.w = parm.dst_size.w;
		dev->dcam_cxt.dst_size.h = parm.dst_size.h;
		dev->dcam_cxt.pxl_fmt = parm.pixel_fmt;
		dev->dcam_cxt.need_isp_tool = parm.need_isp_tool;
		dev->dcam_cxt.need_isp = parm.need_isp;
		dev->dcam_cxt.need_shrink = parm.shrink;
		dev->dcam_cxt.camera_id = parm.camera_id;
		dev->dcam_cxt.path_input_rect.x = parm.crop_rect.x;
		dev->dcam_cxt.path_input_rect.y = parm.crop_rect.y;
		dev->dcam_cxt.path_input_rect.w = parm.crop_rect.w;
		dev->dcam_cxt.path_input_rect.h = parm.crop_rect.h;
		mutex_unlock(&dev->dcam_mutex);
		break;

	case SPRD_IMG_IO_SET_ZOOM_MODE:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&zoom, (uint32_t *)arg, sizeof(uint32_t));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		dev->dcam_cxt.is_smooth_zoom = zoom;
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_SET_ZOOM_MODE, zoom mode %d \n", dev->dcam_cxt.is_smooth_zoom);
		break;

	case SPRD_IMG_IO_SET_SENSOR_IF:
	{
		struct sprd_img_sensor_if sensor_if;

		DCAM_TRACE("SPRD_IMG: set sensor if \n");
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&sensor_if, (struct sprd_img_sensor_if *)arg, sizeof(struct sprd_img_sensor_if));
		if (unlikely(ret)) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}

		if (IF_OPEN == sensor_if.res[0]) {
			dev->dcam_cxt.if_mode     = sensor_if.if_type;
			dev->dcam_cxt.sn_mode     = sensor_if.img_fmt;
			dev->dcam_cxt.yuv_ptn     = sensor_if.img_ptn;
			dev->dcam_cxt.frm_deci    = sensor_if.frm_deci;

			DCAM_TRACE("SPRD_IMG: interface %d, mode %d frm_deci %d \n",
				dev->dcam_cxt.if_mode,
				dev->dcam_cxt.sn_mode,
				dev->dcam_cxt.frm_deci);

			if (DCAM_CAP_IF_CCIR == dev->dcam_cxt.if_mode) {
				/* CCIR interface */
				dev->dcam_cxt.sync_pol.vsync_pol = sensor_if.if_spec.ccir.v_sync_pol;
				dev->dcam_cxt.sync_pol.hsync_pol = sensor_if.if_spec.ccir.h_sync_pol;
				dev->dcam_cxt.sync_pol.pclk_pol  = sensor_if.if_spec.ccir.pclk_pol;
				dev->dcam_cxt.data_bits          = 8;
				DCAM_TRACE("SPRD_IMG: CIR interface, vsync %d hsync %d pclk %d psrc %d bits %d \n",
					dev->dcam_cxt.sync_pol.vsync_pol,
					dev->dcam_cxt.sync_pol.hsync_pol,
					dev->dcam_cxt.sync_pol.pclk_pol,
					dev->dcam_cxt.sync_pol.pclk_src,
					dev->dcam_cxt.data_bits);
			} else {
				dev->dcam_cxt.sync_pol.need_href = sensor_if.if_spec.mipi.use_href;
				dev->dcam_cxt.is_loose           = sensor_if.if_spec.mipi.is_loose;
				dev->dcam_cxt.data_bits          = sensor_if.if_spec.mipi.bits_per_pxl;
				dev->dcam_cxt.lane_num           = sensor_if.if_spec.mipi.lane_num;
				dev->dcam_cxt.pclk               = sensor_if.if_spec.mipi.pclk;
				DCAM_TRACE("SPRD_IMG: MIPI interface, ref %d is_loose %d bits %d lanes %d pclk %d\n",
					dev->dcam_cxt.sync_pol.need_href,
					dev->dcam_cxt.is_loose,
					dev->dcam_cxt.data_bits,
					dev->dcam_cxt.lane_num,
					dev->dcam_cxt.pclk);
			}
		}
		mutex_unlock(&dev->dcam_mutex);
		break;
	}

	case SPRD_IMG_IO_SET_FRAME_ADDR:
		DCAM_TRACE("SPRD_IMG: set frame addr \n");
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (struct sprd_img_parm *)arg, sizeof(struct sprd_img_parm));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}

		switch (parm.channel_id) {
		case DCAM_PATH0:
			path = &info->dcam_path[DCAM_PATH0];
			path_cnt = DCAM_PATH_0_FRM_CNT_MAX;
			path_cfg = dcam_path0_cfg;
			break;
		case DCAM_PATH1:
			path = &info->dcam_path[DCAM_PATH1];
			path_cnt = DCAM_PATH_1_FRM_CNT_MAX;
			path_cfg = dcam_path1_cfg;
			break;
		case DCAM_PATH2:
			path = &info->dcam_path[DCAM_PATH2];
			path_cnt = DCAM_PATH_2_FRM_CNT_MAX;
			path_cfg = dcam_path2_cfg;
			break;
		default:
			printk("SPRD_IMG error: SPRD_IMG_IO_SET_FRAME_ADDR, path 0x%x \n", parm.channel_id);
			mutex_unlock(&dev->dcam_mutex);
			return -EINVAL;
		}


		DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_SET_FRAME_ADDR, status %d, frm_cnt_act %d \n",
			path->status, path->frm_cnt_act);

		if (unlikely(0 == parm.frame_addr.y)) {
			printk("SPRD_IMG: No yaddr \n");
			mutex_unlock(&dev->dcam_mutex);
			ret = -EINVAL;
		} else {
			if (1 == parm.is_reserved_buf) {
				path->frm_reserved_addr.yaddr = parm.frame_addr.y;
				path->frm_reserved_addr.uaddr = parm.frame_addr.u;
				path->frm_reserved_addr.vaddr = parm.frame_addr.v;
				path->frm_reserved_addr_vir.yaddr = parm.frame_addr_vir.y;
				path->frm_reserved_addr_vir.uaddr = parm.frame_addr_vir.u;
				path->frm_reserved_addr_vir.vaddr = parm.frame_addr_vir.v;
			} else {
				struct dcam_addr         frame_addr;
				struct dcam_img_buf_addr buf_addr;

				frame_addr.yaddr = parm.frame_addr.y;
				frame_addr.uaddr = parm.frame_addr.u;
				frame_addr.vaddr = parm.frame_addr.v;
				frame_addr.yaddr_vir = parm.frame_addr_vir.y;
				frame_addr.uaddr_vir = parm.frame_addr_vir.u;
				frame_addr.vaddr_vir = parm.frame_addr_vir.v;
				frame_addr.zsl_private = parm.reserved[0];

				DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_SET_FRAME_ADDR, yaddr: 0x%x, uaddr: 0x%x, vaddr: 0x%x\n",
					parm.frame_addr.y,
					parm.frame_addr.u,
					parm.frame_addr.v);

				if (unlikely(1 == atomic_read(&dev->stream_on)) && path->status == PATH_RUN && IMG_BUF_FLAG_RUNNING == parm.buf_flag) {
					ret = path_cfg(DCAM_PATH_OUTPUT_ADDR, &frame_addr);
				} else {
					if (IMG_BUF_FLAG_INIT == parm.buf_flag) {
					buf_addr.frm_addr.yaddr = parm.frame_addr.y;
					buf_addr.frm_addr.uaddr = parm.frame_addr.u;
					buf_addr.frm_addr.vaddr = parm.frame_addr.v;
					buf_addr.frm_addr.zsl_private = parm.reserved[0];
					buf_addr.frm_addr_vir.yaddr = parm.frame_addr_vir.y;
					buf_addr.frm_addr_vir.uaddr = parm.frame_addr_vir.u;
					buf_addr.frm_addr_vir.vaddr = parm.frame_addr_vir.v;
					buf_addr.frm_addr_vir.zsl_private = parm.reserved[0];
					ret = sprd_img_buf_queue_write(&path->buf_queue, &buf_addr);
					} else {
						printk("sprd_img_k_ioctl: no need to SET_FRAME_ADDR \n");
					}
				}
			}
		}
		mutex_unlock(&dev->dcam_mutex);
		break;

	case SPRD_IMG_IO_PATH_FRM_DECI:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (struct sprd_img_parm *)arg, sizeof(struct sprd_img_parm));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		path = &dev->dcam_cxt.dcam_path[parm.channel_id];
		path->path_frm_deci = 0;//parm.deci; // aiden tmp changes
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("SPRD_IMG: channel %d, frm_deci=%d \n", parm.channel_id, path->path_frm_deci);
		break;

	case SPRD_IMG_IO_SET_SHRINK:
		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&parm, (struct sprd_img_parm *)arg, sizeof(struct sprd_img_parm));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		if(DCAM_PATH0 <= parm.channel_id && parm.channel_id < DCAM_PATH_NUM) {
			path = &dev->dcam_cxt.dcam_path[parm.channel_id];
			path->shrink = parm.shrink;
		} else {
			printk("sprd_img_k_ioctl: Wrong channel ID, %d  \n", parm.channel_id);
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("SPRD_IMG: channel %d, shrink=%d \n", parm.channel_id, path->shrink);
		break;

	case SPRD_IMG_IO_PATH_PAUSE:
		ret = copy_from_user(&parm, (struct sprd_img_parm *)arg, sizeof(struct sprd_img_parm));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			goto exit;
		}
		sprd_img_streampause(file, parm.channel_id, parm.reserved[0]);
		break;

	case SPRD_IMG_IO_PATH_RESUME:
		ret = copy_from_user(&channel_id, (uint32_t *)arg, sizeof(uint32_t));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			goto exit;
		}
		sprd_img_streamresume(file, channel_id);
		break;

	case SPRD_IMG_IO_STREAM_ON:
	{
		struct dcam_path_spec *path_0 = NULL;
		struct dcam_path_spec *path_1 = NULL;
		struct dcam_path_spec *path_2 = NULL;

		DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_STREAM_ON \n");

		mutex_lock(&dev->dcam_mutex);

		path_0 = &dev->dcam_cxt.dcam_path[DCAM_PATH0];
		path_1 = &dev->dcam_cxt.dcam_path[DCAM_PATH1];
		path_2 = &dev->dcam_cxt.dcam_path[DCAM_PATH2];
		memset((void*)path_0->frm_ptr,
			0,
			(uint32_t)(DCAM_FRM_CNT_MAX * sizeof(struct dcam_frame*)));
		memset((void*)path_1->frm_ptr,
			0,
			(uint32_t)(DCAM_FRM_CNT_MAX * sizeof(struct dcam_frame*)));
		memset((void*)path_2->frm_ptr,
			0,
			(uint32_t)(DCAM_FRM_CNT_MAX * sizeof(struct dcam_frame*)));

		DCAM_TRACE("SPRD_IMG: streamon, is_work: path_0 = %d, path_1 = %d, path_2 = %d, stream_on = %d \n",
			path_0->is_work, path_1->is_work, path_2->is_work, atomic_read(&dev->stream_on));

		memcpy((void*)&path_1->in_rect_backup, (void*)&path_1->in_rect, sizeof(struct dcam_rect));
		memcpy((void*)&path_1->in_rect_current, (void*)&path_1->in_rect, sizeof(struct dcam_rect));

		do {
			/* dcam driver module initialization */
			ret = dcam_module_init(dev->dcam_cxt.if_mode, dev->dcam_cxt.sn_mode);
			if (unlikely(ret)) {
				printk(DEBUG_STR,DEBUG_ARGS);
				break;
			}

			ret = sprd_img_queue_init(&dev->queue);
			if (unlikely(ret != 0)) {
				printk("SPRD_IMG: Failed to init queue \n");
				break;
			}

			ret = sprd_img_reg_isr(dev);
			if (unlikely(ret)) {
				printk(DEBUG_STR,DEBUG_ARGS);
				break;
			}

			/* config cap sub-module */
			ret = sprd_img_cap_cfg(&dev->dcam_cxt);
			if (unlikely(ret)) {
				printk(DEBUG_STR,DEBUG_ARGS);
				break;
			}

			/* config path1 sub-module if necessary*/
			if (path_1->is_work) {
				ret = sprd_img_path_cfg(dcam_path1_cfg, path_1);
				if (unlikely(ret)) {
					printk(DEBUG_STR,DEBUG_ARGS);
					break;
				}
				path_1->status = PATH_RUN;
			} else {
				ret = dcam_path1_cfg(DCAM_PATH_ENABLE, &path_1->is_work);
				if (unlikely(ret)) {
					printk(DEBUG_STR,DEBUG_ARGS);
					break;
				}
			}

			/* config path2 sub-module if necessary*/
			if (path_2->is_work) {
				ret = sprd_img_path_cfg(dcam_path2_cfg, path_2);
				if (unlikely(ret)) {
					printk(DEBUG_STR,DEBUG_ARGS);
					break;
				}
				ret = sprd_img_reg_path2_isr(dev);
				if (unlikely(ret)) {
					printk(DEBUG_STR,DEBUG_ARGS);
					break;
				}
				path_2->status = PATH_RUN;
			} else {
				ret = dcam_path2_cfg(DCAM_PATH_ENABLE, &path_2->is_work);
				if (unlikely(ret)) {
					printk(DEBUG_STR,DEBUG_ARGS);
					break;
				}
			}

			if (path_0->is_work) {
				ret = sprd_img_path0_cfg(dcam_path0_cfg, path_0);
				if (unlikely(ret)) {
					printk(DEBUG_STR,DEBUG_ARGS);
					break;
				}
				path_0->status = PATH_RUN;
			} else {
				ret = dcam_path0_cfg(DCAM_PATH_ENABLE, &path_0->is_work);
				if (unlikely(ret)) {
					printk(DEBUG_STR,DEBUG_ARGS);
					break;
				}
			}
		} while(0);
		dev->frame_skipped = 0;
		if (FLASH_HIGH_LIGHT == dev->dcam_cxt.flash_status) {
			if (0 == dev->dcam_cxt.skip_number) {
				sprd_img_start_flash(NULL, dev);
			}
		}

		ret = dcam_start();
		atomic_set(&dev->stream_on, 1);

		if (ret) {
			sprd_img_unreg_path2_isr(dev);
			sprd_img_unreg_isr(dev);
			printk("SPRD_IMG: Failed to start stream 0x%x \n", ret);
		} else {
			atomic_set(&dev->run_flag, 0);
			if (path_0->is_work || path_1->is_work || path_2->is_work) {
				sprd_start_timer(&dev->dcam_timer, DCAM_TIMEOUT);
			}
		}

		mutex_unlock(&dev->dcam_mutex);
		break;
	}

	case SPRD_IMG_IO_STREAM_OFF:
	{
		struct dcam_path_spec *path_0 = NULL;
		struct dcam_path_spec *path_1 = NULL;
		struct dcam_path_spec *path_2 = NULL;

		DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_STREAM_OFF \n");

		mutex_lock(&dev->dcam_mutex);

		path_0 = &dev->dcam_cxt.dcam_path[DCAM_PATH0];
		path_1 = &dev->dcam_cxt.dcam_path[DCAM_PATH1];
		path_2 = &dev->dcam_cxt.dcam_path[DCAM_PATH2];

		DCAM_TRACE("SPRD_IMG: streamoff, is_work: path_0 = %d, path_1 = %d, path_2 = %d, stream_on = %d \n",
			path_0->is_work, path_1->is_work, path_2->is_work, atomic_read(&dev->stream_on));

		if (unlikely(0 == atomic_read(&dev->stream_on))) {
			printk("SPRD_IMG: stream not on\n");
			ret = sprd_img_local_deinit(dev);
			if (unlikely(ret)) {
				printk(DEBUG_STR,DEBUG_ARGS);
			}
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}

		do {
			ret = sprd_stop_timer(&dev->dcam_timer);
			if (unlikely(ret)) {
				printk(DEBUG_STR,DEBUG_ARGS);
				break;
			}
			ret = dcam_stop();
			if (unlikely(ret)) {
				printk(DEBUG_STR,DEBUG_ARGS);
				break;
			}

			ret = dcam_stop_cap();
			if (unlikely(ret)) {
				printk(DEBUG_STR,DEBUG_ARGS);
				break;
			}

			ret = sprd_img_unreg_isr(dev);
			if (unlikely(ret)) {
				printk(DEBUG_STR,DEBUG_ARGS);
				break;
			}

			if (path_1->is_work) {
				path_1->status = PATH_IDLE;
				path_1->is_work = 0;
			}

			if (path_2->is_work) {
				path_2->status = PATH_IDLE;

				if (dev->got_resizer) {
					ret = sprd_img_unreg_path2_isr(dev);
					if (unlikely(ret)) {
						printk(DEBUG_STR,DEBUG_ARGS);
						break;
					}
					dcam_rel_resizer();
					dev->got_resizer = 0;
				}

				path_2->is_work = 0;
			}

			if (path_0->is_work) {
				path_0->status = PATH_IDLE;
				path_0->is_work = 0;
			}
			DCAM_TRACE("SPRD_IMG: off, path work %d %d %d \n", path_0->is_work, path_1->is_work, path_2->is_work);
			atomic_set(&dev->stream_on, 0);
		#if 0
			if (DCAM_CAP_IF_CSI2 == dev->dcam_cxt.if_mode) {
				ret = csi_api_close();
				if (unlikely(ret)) {
					printk(DEBUG_STR,DEBUG_ARGS);
					break;
				}
			}
		#endif

			ret = dcam_module_deinit(dev->dcam_cxt.if_mode, dev->dcam_cxt.sn_mode);
			if (unlikely(ret)) {
				printk(DEBUG_STR,DEBUG_ARGS);
				break;
			}

			ret = sprd_img_local_deinit(dev);
			if (unlikely(ret)) {
				printk(DEBUG_STR,DEBUG_ARGS);
				break;
			}
		} while(0);

		mutex_unlock(&dev->dcam_mutex);
		break;
	}

	case SPRD_IMG_IO_GET_FMT:
	{
		struct dcam_format          *fmt;
		struct sprd_img_get_fmt     fmt_desc;

		DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_GET_FMT \n");
		ret = copy_from_user(&fmt_desc, (struct sprd_img_get_fmt *)arg, sizeof(struct sprd_img_get_fmt));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			goto exit;
		}
		if (unlikely(fmt_desc.index >= ARRAY_SIZE(dcam_img_fmt)))
			return -EINVAL;

		fmt = &dcam_img_fmt[fmt_desc.index];
		fmt_desc.fmt = fmt->fourcc;

		ret = copy_to_user((struct sprd_img_get_fmt *)arg, &fmt_desc, sizeof(struct sprd_img_get_fmt));
		break;
	}

	case SPRD_IMG_IO_GET_CH_ID:
	{
		struct dcam_path_spec    *path_0 = NULL;
		struct dcam_path_spec    *path_1 = NULL;
		struct dcam_path_spec    *path_2 = NULL;
		struct dcam_get_path_id  path_id;

		DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_GET_CH_ID \n");
		path_0 = &dev->dcam_cxt.dcam_path[DCAM_PATH0];
		path_1 = &dev->dcam_cxt.dcam_path[DCAM_PATH1];
		path_2 = &dev->dcam_cxt.dcam_path[DCAM_PATH2];

		memset((void*)&path_id, 0, sizeof(struct dcam_get_path_id));
		path_id.input_size.w = dev->dcam_cxt.cap_in_rect.w;
		path_id.input_size.h = dev->dcam_cxt.cap_in_rect.h;
		path_id.output_size.w = dev->dcam_cxt.dst_size.w;
		path_id.output_size.h = dev->dcam_cxt.dst_size.h;
		path_id.fourcc = dev->dcam_cxt.pxl_fmt;
		path_id.need_isp_tool = dev->dcam_cxt.need_isp_tool;
		path_id.need_isp = dev->dcam_cxt.need_isp;
		path_id.camera_id = dev->dcam_cxt.camera_id;
		path_id.need_shrink = dev->dcam_cxt.need_shrink;
		path_id.input_trim.x = dev->dcam_cxt.path_input_rect.x;
		path_id.input_trim.y = dev->dcam_cxt.path_input_rect.y;
		path_id.input_trim.w = dev->dcam_cxt.path_input_rect.w;
		path_id.input_trim.h = dev->dcam_cxt.path_input_rect.h;
		DCAM_TRACE("SPRD_IMG: get param, path work %d %d %d \n", path_0->is_work, path_1->is_work, path_2->is_work);
		path_id.is_path_work[DCAM_PATH0] = path_0->is_work;
		path_id.is_path_work[DCAM_PATH1] = path_1->is_work;
		path_id.is_path_work[DCAM_PATH2] = path_2->is_work;
		ret = dcam_get_path_id(&path_id, &channel_id);
		ret = copy_to_user((uint32_t *)arg, &channel_id, sizeof(uint32_t));
		DCAM_TRACE("SPRD_IMG: get channel_id %d \n", channel_id);
		break;
	}

	case SPRD_IMG_IO_GET_TIME:
	{
		struct timeval           time;
		struct sprd_img_time     utime;

		DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_GET_TIME \n");
		img_get_timestamp(&time);
		utime.sec = time.tv_sec;
		utime.usec = time.tv_usec;
		ret = copy_to_user((struct sprd_img_time *)arg, &utime, sizeof(struct sprd_img_time));
		break;
	}

	case SPRD_IMG_IO_CHECK_FMT:
	{
		struct dcam_format       *fmt;
		struct sprd_img_format   img_format;

		DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_CHECK_FMT \n");
		ret = copy_from_user(&img_format, (struct sprd_img_format *)arg, sizeof(struct sprd_img_format));
		if (ret) {
			printk("sprd_img_k_ioctl: fail to get user info \n");
			goto exit;
		}

		fmt = sprd_img_get_format(img_format.fourcc);
		if (unlikely(!fmt)) {
			printk("SPRD_IMG: Fourcc format (0x%08x) invalid. \n",
				img_format.fourcc);
			return -EINVAL;
		}

		if (DCAM_PATH1 == img_format.channel_id) {
			mutex_lock(&dev->dcam_mutex);
			ret = sprd_img_check_path1_cap(fmt->fourcc, &img_format, &dev->dcam_cxt);
			mutex_unlock(&dev->dcam_mutex);
			channel_id = DCAM_PATH1;
		} else if (DCAM_PATH2 == img_format.channel_id) {
			if (img_format.is_lightly) {
				mutex_lock(&dev->dcam_mutex);
				ret = sprd_img_check_path2_cap(fmt->fourcc, &img_format, &dev->dcam_cxt);
				mutex_unlock(&dev->dcam_mutex);
			} else {
				if (unlikely(dcam_get_resizer(DCAM_WAIT_FOREVER))) {
					/*no wait to get the controller of resizer, failed*/
					printk("SPRD_IMG: path2 has been occupied by other app \n");
					return -EIO;
				}
				dev->got_resizer = 1;
				mutex_lock(&dev->dcam_mutex);
				ret = sprd_img_check_path2_cap(fmt->fourcc, &img_format, &dev->dcam_cxt);
				mutex_unlock(&dev->dcam_mutex);
				if (ret) {
					/*failed to set path2, release the controller of resizer*/
					dcam_rel_resizer();
					dev->got_resizer = 0;
				}
			}
			channel_id = DCAM_PATH2;
		} else if (DCAM_PATH0 == img_format.channel_id) {
			mutex_lock(&dev->dcam_mutex);
			ret = sprd_img_check_path0_cap(fmt->fourcc, &img_format, &dev->dcam_cxt);
			mutex_unlock(&dev->dcam_mutex);
			channel_id = DCAM_PATH0;
		} else {
			printk("SPRD_IMG: Buf type invalid. \n");
			return -EINVAL;
		}

		memcpy((void*)&img_format.reserved[0], (void*)&dev->dcam_cxt.dcam_path[channel_id].end_sel, sizeof(struct dcam_endian_sel));
		if ((0 == ret) && (0 != atomic_read(&dev->stream_on))) {
			if (DCAM_PATH0 == channel_id || DCAM_PATH1 == channel_id || DCAM_PATH2 == channel_id) {
				ret = sprd_img_update_video(file, channel_id);
			}
		}
		ret = copy_to_user((struct sprd_img_format *)arg, &img_format, sizeof(struct sprd_img_format));
		break;
	}
	case SPRD_IMG_IO_CFG_FLASH: {
		struct sprd_flash_cfg_param	 cfg_param;

		mutex_lock(&dev->dcam_mutex);
		ret = copy_from_user(&cfg_param, (void *)arg, sizeof(cfg_param));
		if (ret) {
			printk("sprd_img_k_ioctl: CFG FLASH fail to get user info \n");
			mutex_unlock(&dev->dcam_mutex);
			goto exit;
		}
		ret = sprd_flash_cfg(&cfg_param, arg);
		mutex_unlock(&dev->dcam_mutex);
		DCAM_TRACE("SPRD_IMG: SPRD_IMG_IO_CFG_FLASH, ret=%d\n", ret);
	}
		break;

	default:
		printk("sprd_img_k_ioctl: invalid cmd %d \n", cmd);
		break;
	}


exit:
	return ret;
}

ssize_t sprd_img_read(struct file *file, char __user *u_data, size_t cnt, loff_t *cnt_ret)
{
	struct dcam_dev          *dev = file->private_data;
	struct dcam_node         node;
	struct dcam_path_spec    *path;
	struct sprd_img_read_op  read_op;
	struct dcam_path_capability path_capability;
	int                      fmr_index, i;
	int                      ret = 0;

	DCAM_TRACE("SPRD_IMG: sprd_img_read %ld, dev %p \n", cnt, dev);

	if (!dev)
		return -EFAULT;

	if (cnt < sizeof(struct sprd_img_read_op)) {
		printk("sprd_img_read: error, cnt %ld read_op %ld \n", cnt, sizeof(struct sprd_img_read_op));
		return -EIO;
	}

	ret = copy_from_user(&read_op, (struct sprd_img_read_op *)u_data, sizeof(struct sprd_img_read_op));
	if (ret) {
		printk("sprd_img_read: fail to get user info \n");
		return ret;
	}

	switch (read_op.cmd) {
	case SPRD_IMG_GET_SCALE_CAP:
		DCAM_TRACE("SPRD_IMG: get scale capbility \n");
		read_op.parm.reserved[0] = DCAM_PATH2_LINE_BUF_LENGTH;
		read_op.parm.reserved[1] = DCAM_SC_COEFF_UP_MAX;
		read_op.parm.reserved[2] = DCAM_SCALING_THRESHOLD;
		DCAM_TRACE("SPRD_IMG: sprd_img_read line threshold %d, sc factor %d, scaling %d.\n",
			read_op.parm.reserved[0],
			read_op.parm.reserved[1],
			read_op.parm.reserved[2]);
		break;

	case SPRD_IMG_GET_FRM_BUFFER:
		DCAM_TRACE("SPRD_IMG: read frame buffer \n");
		memset(&read_op, 0, sizeof(struct sprd_img_read_op));
		while (1) {
			ret = down_interruptible(&dev->irq_sem);
			if (0 == ret) {
				break;
			} else if (-EINTR == ret) {
				read_op.evt = IMG_SYS_BUSY;
				ret = DCAM_RTN_SUCCESS;
				goto read_end;
			}else {
				printk("SPRD_IMG: read frame buffer, failed to down, %d \n", ret);
				return -EPERM;
			}
		}

		if (sprd_img_queue_read(&dev->queue, &node)) {
			printk("SPRD_IMG: read frame buffer, queue is null \n");
			read_op.evt = IMG_SYS_BUSY;
			ret = DCAM_RTN_SUCCESS;
			goto read_end;
		} else {
			if (node.invalid_flag) {
				printk("SPRD_IMG: read frame buffer, invalid node\n");
				read_op.evt = IMG_SYS_BUSY;
				ret = DCAM_RTN_SUCCESS;
				goto read_end;
			}
		}

		DCAM_TRACE("SPRD_IMG: time, %ld %ld \n", (unsigned long)node.time.tv_sec, (unsigned long)node.time.tv_usec);

		read_op.evt = node.irq_flag;
		if (IMG_TX_DONE == read_op.evt || IMG_CANCELED_BUF == read_op.evt) {
			read_op.parm.frame.channel_id = node.f_type;
			path = &dev->dcam_cxt.dcam_path[read_op.parm.frame.channel_id];
			DCAM_TRACE("SPRD_IMG: node, 0x%x %d %d \n", node, node.index, path->frm_id_base);
			read_op.parm.frame.index = path->frm_id_base;//node.index;
			read_op.parm.frame.width = node.width;
			read_op.parm.frame.height = node.height;
			read_op.parm.frame.length = node.reserved;
			read_op.parm.frame.sec = node.time.tv_sec;
			read_op.parm.frame.usec = node.time.tv_usec;
			//fmr_index  = node.index - path->frm_id_base;
			//read_op.parm.frame.real_index = path->index[fmr_index];
			read_op.parm.frame.frm_base_id = path->frm_id_base;
			read_op.parm.frame.img_fmt = path->fourcc;
			read_op.parm.frame.yaddr = node.yaddr;
			read_op.parm.frame.uaddr = node.uaddr;
			read_op.parm.frame.vaddr = node.vaddr;
			read_op.parm.frame.yaddr_vir = node.yaddr_vir;
			read_op.parm.frame.uaddr_vir = node.uaddr_vir;
			read_op.parm.frame.vaddr_vir = node.vaddr_vir;
			read_op.parm.frame.reserved[0] = node.reserved;
			DCAM_TRACE("index %d real_index %d frm_id_base %d \n",
				read_op.parm.frame.index,
				read_op.parm.frame.real_index,
				read_op.parm.frame.frm_base_id);
		} else {
			if (IMG_TIMEOUT == read_op.evt ||
				IMG_TX_ERR == read_op.evt)
				sprd_img_print_reg();
		}
		DCAM_TRACE("SPRD_IMG: read frame buffer, evt 0x%x channel_id 0x%x index 0x%x \n",
			read_op.evt,
			read_op.parm.frame.channel_id,
			read_op.parm.frame.index);
		break;

	case SPRD_IMG_GET_PATH_CAP:
		DCAM_TRACE("SPRD_IMG: get path capbility \n");
		dcam_get_path_capability(&path_capability);
		read_op.parm.capability.count = path_capability.count;
		for (i = 0; i< path_capability.count; i++) {
			read_op.parm.capability.path_info[i].line_buf          = path_capability.path_info[i].line_buf;
			read_op.parm.capability.path_info[i].support_yuv       = path_capability.path_info[i].support_yuv;
			read_op.parm.capability.path_info[i].support_raw       = path_capability.path_info[i].support_raw;
			read_op.parm.capability.path_info[i].support_jpeg      = path_capability.path_info[i].support_jpeg;
			read_op.parm.capability.path_info[i].support_scaling   = path_capability.path_info[i].support_scaling;
			read_op.parm.capability.path_info[i].support_trim       = path_capability.path_info[i].support_trim;
			read_op.parm.capability.path_info[i].is_scaleing_path  = path_capability.path_info[i].is_scaleing_path;
		}
		break;

	default:
		printk("SPRD_IMG: sprd_img_read, invalid cmd \n");
		return EINVAL;
	}
read_end:
	return copy_to_user((struct sprd_img_read_op *)u_data, &read_op, sizeof(struct sprd_img_read_op));
}

ssize_t sprd_img_write(struct file *file, const char __user * u_data, size_t cnt, loff_t *cnt_ret)
{
	struct dcam_dev          *dev = file->private_data;
	struct dcam_info         *info = &dev->dcam_cxt;
	struct dcam_path_spec    *path;
	struct sprd_img_write_op write_op;
	uint32_t                 index;
	int                      ret = 0;

	DCAM_TRACE("SPRD_IMG: sprd_img_write %ld, dev %p \n", cnt, dev);

	if (!dev)
		return -EFAULT;

	if (cnt < sizeof(struct sprd_img_write_op)) {
		printk("sprd_img_write: error, cnt %ld read_op %ld \n", cnt, sizeof(struct sprd_img_write_op));
		return -EIO;
	}

	ret = copy_from_user(&write_op, (struct sprd_img_write_op *)u_data, sizeof(struct sprd_img_write_op));
	if (ret) {
		printk("sprd_img_write: fail to get user info \n");
		return ret;
	}

	switch (write_op.cmd) {
	case SPRD_IMG_STOP_DCAM:
		mutex_lock(&dev->dcam_mutex);
		ret = sprd_img_tx_stop(dev);
		if (ret)
			ret = 0;
		else
			ret = 1;
		mutex_unlock(&dev->dcam_mutex);
		break;

	case SPRD_IMG_FREE_FRAME:
		if (0 == atomic_read(&dev->stream_on)) {
			printk("SPRD_IMG: dev close, no need free!");
			break;
		}
		switch (write_op.channel_id) {
		case DCAM_PATH0:
		case DCAM_PATH1:
		case DCAM_PATH2:
			path = &info->dcam_path[write_op.channel_id];
			break;
		default:
			printk("SPRD_IMG: error free frame buffer, channel_id 0x%x \n", write_op.channel_id);
			return -EINVAL;
		}

		if (PATH_IDLE == path->status) {
			DCAM_TRACE("SPRD_IMG: error free frame buffer, wrong status, channel_id 0x%x \n",
				write_op.channel_id);
			return -EINVAL;
		}

		if (unlikely(write_op.index > path->frm_id_base + path->frm_cnt_act - 1)) {
			printk("SPRD_IMG: error, index %d, frm_id_base %d frm_cnt_act %d \n",
				write_op.index, path->frm_id_base, path->frm_cnt_act);
			ret = -EINVAL;
		} else if (write_op.index < path->frm_id_base) {
			printk("SPRD_IMG: error, index %d, frm_id_base %d \n",
				write_op.index, path->frm_id_base);
			ret = -EINVAL;
		} else {
			index = write_op.index - path->frm_id_base;
			if (path->frm_ptr[index]) {
				dcam_frame_unlock(path->frm_ptr[index]);
			}
			DCAM_TRACE("SPRD_IMG: free frame buffer, channel_id 0x%x, index 0x%x \n",
				write_op.channel_id, write_op.index);
		}
		break;

	default:
		printk("SPRD_IMG: cmd error! \n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int32_t sprd_dcam_registers_dump(void *buf, uint32_t buf_len)
{
	if (NULL == buf || buf_len < 0x400) {
		printk("%s input para is error", __FUNCTION__);
		return -1;
	}

	uint32_t* reg_buf = (uint32_t*)buf;
	uint32_t reg_buf_len = 0x400;
	int ret;

	ret = dcam_read_registers(reg_buf, &reg_buf_len);
	if (ret) {
		printk("dcam_read_registers return error: %d", ret);
		return -1;
	}

	return reg_buf_len;
}

#if 0
LOCAL int  sprd_img_proc_read(char           *page,
			char  	       **start,
			off_t          off,
			int            count,
			int            *eof,
			void           *data)
{
	int                      len = 0, ret;
	struct dcam_dev          *dev = (struct dcam_dev*)data;
	uint32_t*                reg_buf;
	uint32_t                 reg_buf_len = 0x400;
	uint32_t                 print_len = 0, print_cnt = 0;

	(void)start; (void)off; (void)count; (void)eof;

	len += sprintf(page + len, "Context for DCAM device \n");
	len += sprintf(page + len, "*************************************************************** \n");
	len += sprintf(page + len, "SPRD_ION_MM_SIZE: 0x%x \n", SPRD_ION_MM_SIZE);
	len += sprintf(page + len, "the configuration of CAP \n");
	len += sprintf(page + len, " 1. interface mode,       %d \n", dev->dcam_cxt.if_mode);
	len += sprintf(page + len, " 2. sensor mode,          %d \n", dev->dcam_cxt.sn_mode);
	len += sprintf(page + len, " 3. YUV pattern,          %d \n", dev->dcam_cxt.yuv_ptn);
	len += sprintf(page + len, " 4. sync polarities,      v %d h %d p %d href %d \n",
			dev->dcam_cxt.sync_pol.vsync_pol,
			dev->dcam_cxt.sync_pol.hsync_pol,
			dev->dcam_cxt.sync_pol.pclk_pol,
			dev->dcam_cxt.sync_pol.need_href);
	len += sprintf(page + len, " 5. Data bit-width,       %d \n", dev->dcam_cxt.data_bits);
	len += sprintf(page + len, " 6. need ISP,             %d \n", dev->dcam_cxt.dcam_path[DCAM_PATH1].is_from_isp);
	len += sprintf(page + len, "*************************************************************** \n");
	len += sprintf(page + len, "the configuration of PATH1 \n");
	len += sprintf(page + len, " 1. input rect,           %d %d %d %d \n",
			dev->dcam_cxt.dcam_path[DCAM_PATH1].in_rect.x,
			dev->dcam_cxt.dcam_path[DCAM_PATH1].in_rect.y,
			dev->dcam_cxt.dcam_path[DCAM_PATH1].in_rect.w,
			dev->dcam_cxt.dcam_path[DCAM_PATH1].in_rect.h);
	len += sprintf(page + len, " 2. output size,          %d %d \n",
		dev->dcam_cxt.dcam_path[DCAM_PATH1].out_size.w,
		dev->dcam_cxt.dcam_path[DCAM_PATH1].out_size.h);
	len += sprintf(page + len, " 3. output format,        %d \n",
		dev->dcam_cxt.dcam_path[DCAM_PATH1].out_fmt);
	len += sprintf(page + len, " 4. frame base index,     0x%x \n", dev->dcam_cxt.dcam_path[DCAM_PATH1].frm_id_base);
	len += sprintf(page + len, " 5. frame count,          0x%x \n", dev->dcam_cxt.dcam_path[DCAM_PATH1].frm_cnt_act);
	len += sprintf(page + len, " 6. frame type,           0x%x \n", dev->dcam_cxt.dcam_path[DCAM_PATH1].frm_type);
	for (print_cnt = 0; print_cnt < DCAM_PATH_1_FRM_CNT_MAX; print_cnt ++) {
		len += sprintf(page + len, "%2d. frame buffer%d, 0x%08x 0x%08x 0x%08x \n",
			(6 + print_cnt),
			print_cnt,
			dev->dcam_cxt.dcam_path[DCAM_PATH1].frm_addr[print_cnt].yaddr,
			dev->dcam_cxt.dcam_path[DCAM_PATH1].frm_addr[print_cnt].uaddr,
			dev->dcam_cxt.dcam_path[DCAM_PATH1].frm_addr[print_cnt].vaddr);
	}

	if (dev->dcam_cxt.dcam_path[DCAM_PATH2].is_work) {
	len += sprintf(page + len, "*************************************************************** \n");
		len += sprintf(page + len, "the configuration of PATH2 \n");
		len += sprintf(page + len, " 1. input rect,       %d %d %d %d \n",
				dev->dcam_cxt.dcam_path[DCAM_PATH2].in_rect.x,
				dev->dcam_cxt.dcam_path[DCAM_PATH2].in_rect.y,
				dev->dcam_cxt.dcam_path[DCAM_PATH2].in_rect.w,
				dev->dcam_cxt.dcam_path[DCAM_PATH2].in_rect.h);
		len += sprintf(page + len, " 2. output size,      %d %d \n",
			dev->dcam_cxt.dcam_path[DCAM_PATH2].out_size.w,
			dev->dcam_cxt.dcam_path[DCAM_PATH2].out_size.h);
		len += sprintf(page + len, " 3. output format,    %d \n",
			dev->dcam_cxt.dcam_path[DCAM_PATH2].out_fmt);
		len += sprintf(page + len, " 4. frame base index, 0x%x \n", dev->dcam_cxt.dcam_path[DCAM_PATH2].frm_id_base);
		len += sprintf(page + len, " 5. frame count,      0x%x \n", dev->dcam_cxt.dcam_path[DCAM_PATH2].frm_cnt_act);
		len += sprintf(page + len, " 6. frame type,       0x%x \n", dev->dcam_cxt.dcam_path[DCAM_PATH2].frm_type);
		for (print_cnt = 0; print_cnt < DCAM_PATH_2_FRM_CNT_MAX; print_cnt ++) {
			len += sprintf(page + len, "%2d. frame buffer%d, 0x%8x 0x%8x 0x%8x \n",
				(6 + print_cnt),
				print_cnt,
				dev->dcam_cxt.dcam_path[DCAM_PATH2].frm_addr[print_cnt].yaddr,
				dev->dcam_cxt.dcam_path[DCAM_PATH2].frm_addr[print_cnt].uaddr,
				dev->dcam_cxt.dcam_path[DCAM_PATH2].frm_addr[print_cnt].vaddr);
		}

	}

	reg_buf = (uint32_t*)vzalloc(reg_buf_len);
	ret = dcam_read_registers(reg_buf, &reg_buf_len);
	if (ret) {
		vfree(reg_buf);
		return len;
	}

	len += sprintf(page + len, "*************************************************************** \n");
	len += sprintf(page + len, "dcam registers \n");
	print_cnt = 0;
	while (print_len < reg_buf_len) {
		len += sprintf(page + len, "offset 0x%03x: 0x%08x, 0x%08x, 0x%08x, 0x%08x \n",
			print_len,
			reg_buf[print_cnt],
			reg_buf[print_cnt+1],
			reg_buf[print_cnt+2],
			reg_buf[print_cnt+3]);
		print_cnt += 4;
		print_len += 16;
	}

	ret = csi_read_registers(reg_buf, &reg_buf_len);
	if (ret) {
		vfree(reg_buf);
		return len;
	}

	len += sprintf(page + len, "*************************************************************** \n");
	len += sprintf(page + len, "csi host registers \n");
	print_cnt = 0;
	print_len = 0;
	while (print_len < reg_buf_len) {
		len += sprintf(page + len, "offset 0x%03x: 0x%08x, 0x%08x, 0x%08x, 0x%08x \n",
			print_len,
			reg_buf[print_cnt],
			reg_buf[print_cnt+1],
			reg_buf[print_cnt+2],
			reg_buf[print_cnt+3]);
		print_cnt += 4;
		print_len += 16;
	}
	len += sprintf(page + len, "*************************************************************** \n");
	len += sprintf(page + len, "The end of DCAM device \n");
	msleep(10);
	vfree(reg_buf);

	return len;
}
#endif

static struct file_operations image_fops = {
	.owner          = THIS_MODULE,
	.open           = sprd_img_k_open,
	.unlocked_ioctl = sprd_img_k_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = sprd_img_k_ioctl,
#endif
	.release        = sprd_img_k_release,
	.read           = sprd_img_read,
	.write          = sprd_img_write,
};

static struct miscdevice image_dev = {
	.minor = IMAGE_MINOR,
	.name = IMG_DEVICE_NAME,
	.fops = &image_fops,
};


int sprd_img_probe(struct platform_device *pdev)
{
	int ret = 0;

	printk(KERN_ALERT "sprd_img_probe called\n");

	ret = misc_register(&image_dev);
	if (ret) {
		printk(KERN_ERR "cannot register miscdev on minor=%d (%d)\n",
			IMAGE_MINOR, ret);
		ret = -EACCES;
		goto exit;
	}
	image_dev.this_device->of_node = pdev->dev.of_node;
	parse_baseaddress(pdev->dev.of_node);

	printk(KERN_ALERT "sprd_img_probe Success\n");
	goto exit;

exit:
	return ret;
}

static int sprd_img_remove(struct platform_device *pdev)
{
	misc_deregister(&image_dev);
	return 0;
}

static void sprd_img_shutdown(struct platform_device *pdev)
{
	sprd_flash_close();
}

LOCAL const struct of_device_id  of_match_table_dcam[] = {
	{ .compatible = "sprd,sprd_dcam", },
	{ },
};
LOCAL struct platform_driver sprd_img_driver = {
	.probe = sprd_img_probe,
	.remove = sprd_img_remove,
	.shutdown = sprd_img_shutdown,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = IMG_DEVICE_NAME,
		   .of_match_table = of_match_ptr(of_match_table_dcam),
		   },
};

int __init sprd_img_k_init(void)
{
	if (platform_driver_register(&sprd_img_driver) != 0) {
		printk("platform device register Failed \n");
		return -1;
	}

	if (dcam_scale_coeff_alloc()) {
		printk("dcam_scale_coeff_alloc Failed \n");
		return -1;
	}

	printk(KERN_INFO "Video Technology Magazine Virtual Video "
	       "Capture Board ver %u.%u.%u successfully loaded.\n",
	       (DCAM_VERSION >> 16) & 0xFF, (DCAM_VERSION >> 8) & 0xFF,
	       DCAM_VERSION & 0xFF);
	return 0;
}

void sprd_img_k_exit(void)
{
	dcam_scale_coeff_free();
	platform_driver_unregister(&sprd_img_driver);
}

module_init(sprd_img_k_init);
module_exit(sprd_img_k_exit);
MODULE_DESCRIPTION("DCAM Driver");
MODULE_AUTHOR("Multimedia_Camera@Spreadtrum");
MODULE_LICENSE("GPL");

