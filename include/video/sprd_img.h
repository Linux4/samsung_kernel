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
#ifndef _SPRD_IMG_H_
#define _SPRD_IMG_H_

/*  Four-character-code (FOURCC) */
#define img_fourcc(a, b, c, d)\
	((uint32_t)(a) | ((uint32_t)(b) << 8) | ((uint32_t)(c) << 16) | ((uint32_t)(d) << 24))

/* RGB formats */
#define IMG_PIX_FMT_RGB565  img_fourcc('R', 'G', 'B', 'P') /* 16  RGB-5-6-5     */
#define IMG_PIX_FMT_RGB565X img_fourcc('R', 'G', 'B', 'R') /* 16  RGB-5-6-5 BE  */

/* Grey formats */
#define IMG_PIX_FMT_GREY    img_fourcc('G', 'R', 'E', 'Y') /*  8  Greyscale     */

/* Luminance+Chrominance formats */
#define IMG_PIX_FMT_YVU420  img_fourcc('Y', 'V', '1', '2') /* 12  YVU 4:2:0     */
#define IMG_PIX_FMT_YUYV    img_fourcc('Y', 'U', 'Y', 'V') /* 16  YUV 4:2:2     */
#define IMG_PIX_FMT_YVYU    img_fourcc('Y', 'V', 'Y', 'U') /* 16 YVU 4:2:2 */
#define IMG_PIX_FMT_UYVY    img_fourcc('U', 'Y', 'V', 'Y') /* 16  YUV 4:2:2     */
#define IMG_PIX_FMT_VYUY    img_fourcc('V', 'Y', 'U', 'Y') /* 16  YUV 4:2:2     */
#define IMG_PIX_FMT_YUV422P img_fourcc('4', '2', '2', 'P') /* 16  YVU422 planar */
#define IMG_PIX_FMT_YUV420  img_fourcc('Y', 'U', '1', '2') /* 12  YUV 4:2:0     */

/* two planes -- one Y, one Cr + Cb interleaved  */
#define IMG_PIX_FMT_NV12    img_fourcc('N', 'V', '1', '2') /* 12  Y/CbCr 4:2:0  */
#define IMG_PIX_FMT_NV21    img_fourcc('N', 'V', '2', '1') /* 12  Y/CrCb 4:2:0  */

/* compressed formats */
#define IMG_PIX_FMT_JPEG     img_fourcc('J', 'P', 'E', 'G') /* JFIF JPEG     */

#define SPRD_IMG_PATH_MAX    6

#define SPRD_FLASH_MAX_CELL  40

enum {
	IMG_TX_DONE       = 0x00,
	IMG_NO_MEM        = 0x01,
	IMG_TX_ERR        = 0x02,
	IMG_CSI2_ERR      = 0x03,
	IMG_SYS_BUSY      = 0x04,
	IMG_CANCELED_BUF  = 0x05,
	IMG_TIMEOUT       = 0x10,
	IMG_TX_STOP       = 0xFF
};

enum {
	IMG_ENDIAN_BIG = 0,
	IMG_ENDIAN_LITTLE,
	IMG_ENDIAN_HALFBIG,
	IMG_ENDIAN_HALFLITTLE,
	IMG_ENDIAN_MAX
};

enum if_status {
	IF_OPEN = 0,
	IF_CLOSE
};

enum {
	SPRD_IMG_GET_SCALE_CAP = 0,
	SPRD_IMG_GET_FRM_BUFFER,
	SPRD_IMG_STOP_DCAM,
	SPRD_IMG_FREE_FRAME,
	SPRD_IMG_GET_PATH_CAP
};

enum sprd_flash_type {
	FLASH_TYPE_PREFLASH,
	FLASH_TYPE_MAIN,
	FLASH_TYPE_MAX
};

enum sprd_flash_io_id {
	FLASH_IOID_GET_CHARGE,
	FLASH_IOID_GET_TIME,
	FLASH_IOID_GET_MAX_CAPACITY,
	FLASH_IOID_SET_CHARGE,
	FLASH_IOID_SET_TIME,
	FLASH_IOID_MAX
};

enum sprd_buf_flag {
	IMG_BUF_FLAG_INIT,
	IMG_BUF_FLAG_RUNNING,
	IMG_BUF_FLAG_MAX
};


struct sprd_img_size {
	uint32_t w;
	uint32_t h;
};

struct sprd_img_rect {
	uint32_t x;
	uint32_t y;
	uint32_t w;
	uint32_t h;
};

struct sprd_img_frm_addr {
	uint32_t y;
	uint32_t u;
	uint32_t v;
};

struct sprd_img_parm {
	uint32_t                  channel_id;
	uint32_t                  frame_base_id;
	uint32_t                  pixel_fmt;
	uint32_t                  need_isp_tool;
	uint32_t                  deci;
	uint32_t                  shrink;
	uint32_t                  index;
	uint32_t                  need_isp;
	uint32_t                  camera_id;
	uint32_t                  is_reserved_buf;
	uint32_t                  buf_flag;
	struct sprd_img_rect      crop_rect;
	struct sprd_img_size      dst_size;
	struct sprd_img_frm_addr  frame_addr;
	struct sprd_img_frm_addr  frame_addr_vir;
	uint32_t                  reserved[4];
};

struct sprd_img_ccir_if {
	uint32_t v_sync_pol;
	uint32_t h_sync_pol;
	uint32_t pclk_pol;
	uint32_t res1;
	uint32_t padding;
};

struct sprd_img_mipi_if {
	uint32_t use_href;
	uint32_t bits_per_pxl;
	uint32_t is_loose;
	uint32_t lane_num;
	uint32_t pclk;
};

struct sprd_img_sensor_if {
	uint32_t if_type;
	uint32_t img_fmt;
	uint32_t img_ptn;
	uint32_t frm_deci;
	uint32_t res[4];
	union {
		struct sprd_img_ccir_if ccir;
		struct sprd_img_mipi_if mipi;
	} if_spec;
};

struct sprd_img_frm_info {
	uint32_t channel_id;
	uint32_t width;
	uint32_t height;
	uint32_t length;
	uint32_t sec;
	uint32_t usec;
	uint32_t frm_base_id;
	uint32_t index;
	uint32_t real_index;
	uint32_t img_fmt;
	uint32_t yaddr;
	uint32_t uaddr;
	uint32_t vaddr;
	uint32_t yaddr_vir;
	uint32_t uaddr_vir;
	uint32_t vaddr_vir;
	uint32_t reserved[4];
};

struct sprd_img_path_info {
	uint32_t               line_buf;
	uint32_t               support_yuv;
	uint32_t               support_raw;
	uint32_t               support_jpeg;
	uint32_t               support_scaling;
	uint32_t               support_trim;
	uint32_t               is_scaleing_path;;
};

struct sprd_img_path_capability {
	uint32_t               count;
	struct sprd_img_path_info  path_info[SPRD_IMG_PATH_MAX];
};


struct sprd_img_write_op {
	uint32_t cmd;
	uint32_t channel_id;
	uint32_t index;
};

struct sprd_img_read_op {
	uint32_t cmd;
	uint32_t evt;
	union {
		struct sprd_img_frm_info frame;
		struct sprd_img_path_capability capability;
		uint32_t reserved[20];
	} parm;
};

struct sprd_img_get_fmt {
	uint32_t index;
	uint32_t fmt;
};

struct sprd_img_time {
	uint32_t sec;
	uint32_t usec;
};

struct sprd_img_format {
	uint32_t channel_id;
	uint32_t width;
	uint32_t height;
	uint32_t fourcc;
	uint32_t need_isp;
	uint32_t need_binning;
	uint32_t bytesperline;
	uint32_t is_lightly;
	uint32_t reserved[4];
};

struct sprd_flash_element {
	uint16_t index;
	uint16_t val;
};

struct sprd_flash_cell {
	uint8_t type;
	uint8_t count;
	uint8_t def_val;
	struct sprd_flash_element element[SPRD_FLASH_MAX_CELL];
};

struct sprd_flash_capacity {
	uint16_t max_charge;
	uint16_t max_time;
};

struct sprd_flash_cfg_param {
	uint32_t io_id;
	void *data;
};


#define SPRD_IMG_IO_MAGIC            'Z'
#define SPRD_IMG_IO_SET_MODE          _IOW(SPRD_IMG_IO_MAGIC, 0, uint32_t)
#define SPRD_IMG_IO_SET_SKIP_NUM      _IOW(SPRD_IMG_IO_MAGIC, 1, uint32_t)
#define SPRD_IMG_IO_SET_SENSOR_SIZE   _IOW(SPRD_IMG_IO_MAGIC, 2, struct sprd_img_size)
#define SPRD_IMG_IO_SET_SENSOR_TRIM   _IOW(SPRD_IMG_IO_MAGIC, 3, struct sprd_img_rect)
#define SPRD_IMG_IO_SET_FRM_ID_BASE   _IOW(SPRD_IMG_IO_MAGIC, 4, struct sprd_img_parm)
#define SPRD_IMG_IO_SET_CROP          _IOW(SPRD_IMG_IO_MAGIC, 5, struct sprd_img_parm)
#define SPRD_IMG_IO_SET_FLASH         _IOW(SPRD_IMG_IO_MAGIC, 6, uint32_t)
#define SPRD_IMG_IO_SET_OUTPUT_SIZE   _IOW(SPRD_IMG_IO_MAGIC, 7, struct sprd_img_parm)
#define SPRD_IMG_IO_SET_ZOOM_MODE     _IOW(SPRD_IMG_IO_MAGIC, 8, uint32_t)
#define SPRD_IMG_IO_SET_SENSOR_IF     _IOW(SPRD_IMG_IO_MAGIC, 9, struct sprd_img_sensor_if)
#define SPRD_IMG_IO_SET_FRAME_ADDR    _IOW(SPRD_IMG_IO_MAGIC, 10, struct sprd_img_parm)
#define SPRD_IMG_IO_PATH_FRM_DECI     _IOW(SPRD_IMG_IO_MAGIC, 11, struct sprd_img_parm)
#define SPRD_IMG_IO_PATH_PAUSE        _IOW(SPRD_IMG_IO_MAGIC, 12, struct sprd_img_parm)
#define SPRD_IMG_IO_PATH_RESUME       _IOW(SPRD_IMG_IO_MAGIC, 13, uint32_t)
#define SPRD_IMG_IO_STREAM_ON         _IOW(SPRD_IMG_IO_MAGIC, 14, uint32_t)
#define SPRD_IMG_IO_STREAM_OFF        _IOW(SPRD_IMG_IO_MAGIC, 15, uint32_t)
#define SPRD_IMG_IO_GET_FMT           _IOR(SPRD_IMG_IO_MAGIC, 16, struct sprd_img_get_fmt)
#define SPRD_IMG_IO_GET_CH_ID         _IOR(SPRD_IMG_IO_MAGIC, 17, uint32_t)
#define SPRD_IMG_IO_GET_TIME          _IOR(SPRD_IMG_IO_MAGIC, 18, struct sprd_img_time)
#define SPRD_IMG_IO_CHECK_FMT         _IOWR(SPRD_IMG_IO_MAGIC, 19, struct sprd_img_format)
#define SPRD_IMG_IO_SET_SHRINK        _IOW(SPRD_IMG_IO_MAGIC, 20, uint32_t)
#define SPRD_IMG_IO_CFG_FLASH         _IOW(SPRD_IMG_IO_MAGIC, 22, struct sprd_flash_cfg_param)

/*
Dump dcam register.
buf:      input dump buffer addr
buf_len:  input dump buffer size(>=0x400), and buf_len=0x400 is ok
return    real dump size
*/
int32_t sprd_dcam_registers_dump(void *buf, uint32_t buf_len);
#endif //_SPRD_V4L2_H_
