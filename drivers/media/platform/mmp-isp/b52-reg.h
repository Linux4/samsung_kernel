/*
 * Copyright (C) 2013 Marvell International Ltd.
 *				 Jianle Wang <wanjl@marvell.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#ifndef __MMP_B52_REG_H__
#define __MMP_B52_REG_H__

#include <linux/io.h>
#include <uapi/media/b52_api.h>

#include "b52isp.h"

#define FW_FILE_V324          "ispfw_v324.bin"
#define FW_FILE_V325          "ispfw_v325.bin"
#define FW_FILE_V326          "ispfw_v326.bin"

enum mcu_cmd_name {
	CMD_TEST = 0,
	CMD_RAW_DUMP,
	CMD_RAW_PROCESS,
	CMD_IMG_CAPTURE,
	CMD_CHG_FORMAT,
	CMD_SET_FORMAT,
	CMD_SET_ZOOM,
	CMD_HDR_STILL,
	CMD_TEMPORAL_DNS,
	CMD_END,
};

enum mcd_cmd_flag {
	CMD_FLAG_INIT = 0,
	CMD_FLAG_MUTE,
	CMD_FLAG_LOCK_AEAG,
	CMD_FLAG_LOCK_AF,
	CMD_FLAG_META_DATA,
	CMD_FLAG_MS,
	CMD_FLAG_STREAM_OFF,
	CMD_FLAG_LINEAR_YUV,
	CMD_FLAG_MAX,
};

enum mcu_cmd_src_type {
	CMD_SRC_NONE = 0,
	CMD_SRC_SNSR,
	CMD_SRC_AXI,
};

enum mcd_cmd_todo {
	CMD_TODO_FLAG = 0,
	CMD_TODO_SRC_BUFFER,
	CMD_TODO_DST_MAP,
	CMD_TODO_DST_BUFFER,
	CMD_TODO_MAX,
};

#define B52_BUFFER_PER_BRACKET	3
#define B52_OUTPUT_PER_PIPELINE	6

struct b52isp_output {
	struct isp_vnode	*vnode;
	struct v4l2_pix_format_mplane	pix_mp;
	struct isp_videobuf	*buf[B52_BUFFER_PER_BRACKET];
	int nr_buffer;
	int no_zoom;
};

struct b52_adv_dns {
	enum adv_dns_type type;
	__u8 Y_times;
	__u8 UV_times;
	char *Y_buffer;
	dma_addr_t Y_phyaddr;
	char *UV_buffer;
	dma_addr_t UV_phyaddr;
};

struct b52isp_cmd {
	enum mcu_cmd_name	cmd_name;
	int			flags;
	int			path;
	enum mcu_cmd_src_type	src_type;
	/* A source can be a image sensor or memory input */
	union {
		const void				*memory_sensor_data;
		struct v4l2_subdev		*sensor;
	};
	struct {
		int			axi_id;
		struct isp_videobuf	*buf[B52_BUFFER_PER_BRACKET];
		struct isp_vnode	*vnode;
	} mem;
	struct v4l2_pix_format	src_fmt;
	struct v4l2_rect	pre_crop; /* IDI output */
	struct v4l2_rect	post_crop; /* For ISP Pipeline YUV cropper */
	struct b52isp_output	output[B52_OUTPUT_PER_PIPELINE];
	unsigned long		output_map;
	unsigned long		enable_map;
	struct list_head	hook;
	dma_addr_t		meta_dma;
	int			meta_port;
	__u16			b_ratio_1;
	__u16			b_ratio_2;
	struct b52_adv_dns	adv_dns;
	u8 nr_mac;
	void *priv;
};

int b52_rw_pipe_ctdata(int pipe_id, int write,
			struct b52_regval *entry, u32 nr_entry);
void b52_set_base_addr(void __iomem *base);
int b52_load_fw(struct device *dev, void __iomem *base, int enable,
						int pwr, int hw_version);
int b52_get_metadata_len(int path);
int b52_hdl_cmd(struct b52isp_cmd *cmd);
int b52_ctrl_mac_irq(u8 mac_id, u8 port_id, int enable);
void b52_clear_mac_rdy_bit(u8 mac, u8 port);
void b52_clear_overflow_flag(u8 mac, u8 port);
int b52_update_mac_addr(dma_addr_t *addr, dma_addr_t meta,
			u8 plane, u8 mac, u8 port);
void b52_ack_xlate_irq(__u32 *events, int max_mac_num, struct work_struct *work);
int b52_read_pipeline_info(int pipe_id, u8 *buffer);
int b52_read_debug_info(u8 *buffer);
int b52_cmd_anti_shake(u16 block_size, int enable);
int b52_cmd_zoom_in(int path, int zoom);
int b52_cmd_vcm(void);
int b52_cmd_effect(int reg_nums);
int b52_set_focus_win(struct v4l2_rect *win, int id);
struct plat_cam;
extern int b52_fill_mmu_chnl(struct plat_cam *pcam,
			struct isp_videobuf *buf, int num_planes);
void b52_set_sccb_clock_rate(u32 input_rate, u32 sccb_rate);
extern void b52isp_set_ddr_qos(s32 value);
extern void b52isp_set_ddr_threshold(struct work_struct *work, int up);

/* isp MCU memory layout */
/* 1. PROGREM BUFFER(136KB):0x0    ~ 0x021FFF */
/* 2. DATA BUFFER (20KB): 0x030000 ~ 0x034FFF */
	/*CMD BUF A: 0x034000 ~ 0x341FF*/
	/*CMD BUF B: 0x034200 ~ 0x342FF*/
	/*CMD BUF C: 0x034300 ~ 0x343FF*/
	/*CMD BUF D: 0x034400 ~ 0x347FF*/
/* 3. CACHE BUFFER (4KB): 0x03FC00 ~ 0x03FFFF */
/* 4. LINE BUFFER (64KB): 0x040000 ~ 0x04FFFF */
/* 5. HW REGISTER(128KB): 0x050000 ~ 0x06FFFF */
#define FW_BUF_START	(0x00000)
#define FW_BUF_END	(0x21FFF)
#define DATA_BUF_START	(0x30000)
#define DATA_BUF_END	(0x34FFF)
#define CACHE_START	(0x3FC00)
#define CACHE_END	(0x3FFFF)
#define LINE_BUF_START	(0x40000)
#define LINE_BUF_END	(0x4FFFF)
#define HW_REG_START	(0x50000)
#define HW_REG_END	(0x6FFFF)

/* command buffer */
#define CMD_BUF_A           (0x34000)
#define CMD_BUF_A_END       (0x341FF)
#define CMD_BUF_B           (0x34200)
#define CMD_BUF_B_END       (0x342FF)
#define CMD_BUF_C           (0x34300)
#define CMD_BUF_C_END       (0x343FF)
#define CMD_BUF_D           (0x34400)
#define CMD_BUF_D_END       (0x347FF)
#define PIPE_INFO_SIZE		(0x80)
#define PIPE_INFO_OFFSET	(0x80)

#define MAX_PIPE_NUM        (0x2)

/*DATA set registers */
#define DATA_BGAIN            (DATA_BUF_START + 0x232)
#define DATA_GGAIN            (DATA_BUF_START + 0x234)
#define DATA_RGAIN            (DATA_BUF_START + 0x236)

/*VTS apply to sensor*/
#define VTS_SYNC_TO_SENSOR            (0x3303d)

/* CMD set registers */
#define CMD_REG0            (0x63900)
#define CMD_REG1            (CMD_REG0 + 0x1)
#define CMD_REG2            (CMD_REG0 + 0x2)
#define CMD_REG3            (CMD_REG0 + 0x3)
#define CMD_REG4            (CMD_REG0 + 0x4)
#define CMD_REG5            (CMD_REG0 + 0x5)
#define CMD_REG6            (CMD_REG0 + 0x6)
#define CMD_REG7            (CMD_REG0 + 0x7)
#define CMD_REG8            (CMD_REG0 + 0x8)
#define CMD_REG9            (CMD_REG0 + 0x9)
#define CMD_REG10           (CMD_REG0 + 0xa)
#define CMD_REG11           (CMD_REG0 + 0xb)
#define CMD_REG12           (CMD_REG0 + 0xc)
#define CMD_REG13           (CMD_REG0 + 0xd)
#define CMD_REG14           (CMD_REG0 + 0xe)
#define CMD_REG15           (CMD_REG0 + 0xf)
#define CMD_REG16           (CMD_REG0 + 0x10)
#define CMD_REG17           (CMD_REG0 + 0x11)
#define CMD_ID              CMD_REG16
#define CMD_RESULT          CMD_REG17
	#define CMD_SET_SUCCESS     (0x1)

#define WAIT_CMD_TIMEOUT        (HZ * 2)

/* CMD set id */
#define CMD_I2C_GRP_WR          (0x1)
#define CMD_SET_FMT             (0x2)
#define CMD_TDNS                (0x3)
#define CMD_CAPTURE_IMG         (0x4)
#define CMD_CAPTURE_RAW         (0x5)
#define CMD_PROCESS_RAW         (0x6)
#define CMD_CHANGE_FMT          (0x7)
#define CMD_HDR_STILL           (0x8)
#define CMD_EFFECT              (0x8)
#define CMD_UV_DENOISE          (0x9)
#define CMD_ABORT               (0xf)
#define CMD_AWB_MODE            (0x11)
#define CMD_ANTI_SHAKE          (0x12)
#define CMD_AF_MODE             (0x13)
#define CMD_ZOOM_IN_MODE        (0x14)
/*firmware interrupt IDs from MCU*/
#define CMD_WB_EXPO_GAIN        (0xf1)
#define CMD_WB_EXPO             (0xf2)
#define CMD_WB_GAIN             (0xf3)
#define CMD_WB_FOCUS            (0xf4)
#define CMD_UPDATE_ADDR         (0xfe)
#define CMD_DOWNLOAD_FW         (0xff)

/* capture image */
/* REG1:I2C choice */
#define I2C_MAX_NUM      (255)
#define I2C_READ         (0x0 << 7)
#define I2C_WRITE        (0x1 << 7)
#define I2C_8BIT_ADDR    (0x0 << 3)
#define I2C_16BIT_ADDR   (0x1 << 3)
#define I2C_8BIT_DATA    (0x0 << 2)
#define I2C_16BIT_DATA   (0x1 << 2)
#define I2C_NONE         (0x0 << 0)
#define I2C_PRIMARY      (0x1 << 0)
#define I2C_SECONDARY    (0x2 << 0)
#define I2C_BOTH         (0x3 << 0)
/* REG3: UV-denoise */
#define ADV_DNS_UYVY    (0x0 << 4)
#define ADV_DNS_NV12    (0x1 << 4)
#define ADV_DNS_I420    (0x2 << 4)
#define ADV_DNS_MERGE    (0x1 << 3)
#define ADV_UV_DNS       (0x1 << 2)
#define ADV_Y_DNS        (0x1 << 1)
#define ENABLE_ADV_DNS   (0x1 << 0)
#define DISABLE_ADV_DNS  (0x0 << 0)
/* REG5: mode configure */
#define INTI_MODE_ENABLE            (0x1 << 7)
#define INTI_MODE_DISABLE           (0x0 << 7)
#define BYPASS_MAC_INT_ENABLE		(0x1 << 6)
#define BYPASS_MAC_INT_DISABLE		(0x0 << 6)
#define KEEP_EXPO_RATIO_ENABLE		(0x1 << 5)
#define KEEP_EXPO_RATIO_DISABLE		(0x0 << 5)
#define ENABEL_METADATA             (0x1 << 4)
#define ENABLE_LINEAR_YUV           (0x1 << 2)
#define BRACKET_3_EXPO              (0x1 << 1)
#define BRACKET_2_EXPO              (0x0 << 1)
#define BRACKET_CAPTURE             (0x1 << 0)
#define NORMAL_CAPTURE              (0x0 << 0)
/* REG6:work mode */
#define PIPELINE_1                  (0x0)
#define PIPELINE_2                  (0x1)
#define STEREO_3D                   (0x2)
#define HIGH_SPEED                  (0x3)
#define HDR                         (0x4)

/* set format and capture format parameters */
/* input configuration */
#define ISP_INPUT_TYPE              (CMD_BUF_A + 0x0)
	#define ISP_ENABLE              (0x1 << 6)
	#define SRC_NONE                (0x0 << 3)
	#define SRC_PRIMARY_MIPI        (0x1 << 3)
	#define SRC_SECONDAY_MIPI       (0x2 << 3)
	#define SRC_MIPI_3D             (0x3 << 3)
	#define SRC_MEMORY              (0x4 << 3)
	#define SRC_MEM_3D              (0x5 << 3)
	#define SRC_LANES_8_MIPI        (0x6 << 3)
#define ISP_INPUT_WIDTH             (CMD_BUF_A + 0x2)
#define ISP_INPUT_HEIGHT            (CMD_BUF_A + 0x4)

#define ISP_IDI_CTRL                (CMD_BUF_A + 0x6)
	#define IDI_SCALE_DOWN_EN       (0x1 << 3)
	#define IDI_SD_3_5              (0x0)
	#define IDI_SD_1_2              (0x1)
	#define IDI_SD_2_5              (0x2)
	#define IDI_SD_1_3              (0x3)
	#define IDI_SD_2_3              (0x4)

#define ISP_IDI_OUTPUT_WIDTH        (CMD_BUF_A + 0x8)
#define ISP_IDI_OUTPUT_HEIGHT       (CMD_BUF_A + 0xa)
#define ISP_IDI_OUTPUT_H_START      (CMD_BUF_A + 0xc)
#define ISP_IDI_OUTPUT_V_START      (CMD_BUF_A + 0xe)

/* output configuration */
#define ISP_OUTPUT_PORT_SEL         (CMD_BUF_A + 0x20)
#define ISP_OUTPUT1_TYPE            (CMD_BUF_A + 0x22)
	#define ISP_OUTPUT_NO_ZOOM      (0x1 << 4)
#define ISP_OUTPUT1_WIDTH           (CMD_BUF_A + 0x24)
#define ISP_OUTPUT1_HEIGHT          (CMD_BUF_A + 0x26)
#define ISP_OUTPUT1_MEM_WIDTH       (CMD_BUF_A + 0x28)
#define ISP_OUTPUT1_MEM_UV_WIDTH    (CMD_BUF_A + 0x2a)
#define ISP_OUTPUT2_TYPE            (CMD_BUF_A + 0x2c)
#define ISP_OUTPUT2_WIDTH           (CMD_BUF_A + 0x2e)
#define ISP_OUTPUT2_HEIGHT          (CMD_BUF_A + 0x30)
#define ISP_OUTPUT2_MEM_WIDTH       (CMD_BUF_A + 0x32)
#define ISP_OUTPUT2_MEM_UV_WIDTH    (CMD_BUF_A + 0x34)
#define ISP_OUTPUT3_TYPE            (CMD_BUF_A + 0x36)
#define ISP_OUTPUT3_WIDTH           (CMD_BUF_A + 0x38)
#define ISP_OUTPUT3_HEIGHT          (CMD_BUF_A + 0x3a)
#define ISP_OUTPUT3_MEM_WIDTH       (CMD_BUF_A + 0x3c)
#define ISP_OUTPUT3_MEM_UV_WIDTH    (CMD_BUF_A + 0x3e)
#define ISP_OUTPUT4_TYPE            (CMD_BUF_A + 0x40)
#define ISP_OUTPUT4_WIDTH           (CMD_BUF_A + 0x42)
#define ISP_OUTPUT4_HEIGHT          (CMD_BUF_A + 0x44)
#define ISP_OUTPUT4_MEM_WIDTH       (CMD_BUF_A + 0x46)
#define ISP_OUTPUT4_MEM_UV_WIDTH    (CMD_BUF_A + 0x48)
#define ISP_OUTPUT_OFFSET           (0x0a)
#define MAX_OUTPUT_PORT             (0x4)
#define MAX_RD_PORT                 (0x3)

/* read configuration */
#define ISP_RD_PORT_SEL             (CMD_BUF_A + 0x60)
#define ISP_RD_WIDTH                (CMD_BUF_A + 0x62)
#define ISP_RD_HEIGHT               (CMD_BUF_A + 0x64)
#define ISP_RD_MEM_WIDTH            (CMD_BUF_A + 0x66)

/* ISP configration */
#define ISP_EXPO_RATIO              (CMD_BUF_A + 0x70)
#define ISP_MAX_EXPO                (CMD_BUF_A + 0x72)
#define ISP_MIN_EXPO                (CMD_BUF_A + 0x74)
#define ISP_MAX_GAIN                (CMD_BUF_A + 0x76)
#define ISP_MIN_GAIN                (CMD_BUF_A + 0x78)
#define ISP_VTS                     (CMD_BUF_A + 0x7a)
#define ISP_50HZ_BAND               (CMD_BUF_A + 0x7c)
#define ISP_60HZ_BAND               (CMD_BUF_A + 0x7e)
#define ISP_BRACKET_RATIO1          (CMD_BUF_A + 0x80)
#define ISP_BRACKET_RATIO2          (CMD_BUF_A + 0x82)
#define ISP_ZOOM_IN_RATIO           (CMD_BUF_A + 0x84)
#define ISP_HTS				(CMD_BUF_A + 0x86)
	#define ZOOM_RATIO_MAX	    (0x400)
	#define ZOOM_RATIO_MIN	    (0x100)

/* HDR configuration */
#define ISP_HDR_CTRL                (CMD_BUF_A + 0x90)
#define ISP_HDR_L_EXPO              (CMD_BUF_A + 0x92)
#define ISP_HDR_L_GAIN              (CMD_BUF_A + 0x94)
#define ISP_HDR_S_EXPO              (CMD_BUF_A + 0x96)
#define ISP_HDR_S_GAIN              (CMD_BUF_A + 0x98)

/* capture configuration */
#define ISP_OUTPUT_ADDR1            (CMD_BUF_A + 0xa0)
#define ISP_OUTPUT_ADDR2            (CMD_BUF_A + 0xa4)
#define ISP_OUTPUT_ADDR3            (CMD_BUF_A + 0xa8)
#define ISP_OUTPUT_ADDR4            (CMD_BUF_A + 0xac)
#define ISP_OUTPUT_ADDR5            (CMD_BUF_A + 0xb0)
#define ISP_OUTPUT_ADDR6            (CMD_BUF_A + 0xb4)
#define ISP_OUTPUT_ADDR7            (CMD_BUF_A + 0xb8)
#define ISP_OUTPUT_ADDR8            (CMD_BUF_A + 0xbc)
#define ISP_OUTPUT_ADDR9            (CMD_BUF_A + 0xc0)
#define ISP_INPUT_ADDR1             (CMD_BUF_A + 0xc4)
#define ISP_INPUT_ADDR2             (CMD_BUF_A + 0xc8)
#define ISP_META_ADDR               (CMD_BUF_A + 0xcc)

#define ISP_DNS_Y_ADDR              (CMD_BUF_A + 0xd0)
#define ISP_DNS_UV_ADDR             (CMD_BUF_A + 0xd4)
#define ISP_DNS_U_ADDR             (CMD_BUF_A + 0xd4)
#define ISP_DNS_V_ADDR             (CMD_BUF_A + 0xd8)

/* top system control register */
/* power on */
#define REG_TOP_SW_STANDBY	(0x60100)
#define REG_TOP_SW_RST		(0x60103)
	#define SW_STANDBY		(0x1)
	#define SW_RST			(0x1)
	#define MCU_SW_RST		(0x1)
	#define RELEASE_SW_RST		(0x0)
#define REG_TOP_CLK_RST0	(0x6301a)
#define REG_TOP_CLK_RST1	(0x6301b)
#define REG_TOP_CLK_RST2	(0x6301c)
#define REG_TOP_CLK_RST3	(0x6301d)
#define REG_TOP_CORE_CTRL0_H	(0x63022)
#define REG_TOP_CORE_CTRL0_L	(0x63023)
#define REG_TOP_CORE_CTRL1_H	(0x63024)
#define REG_TOP_CORE_CTRL1_L	(0x63025)
#define REG_TOP_INTR_CTRL_H	(0x63027)
#define REG_TOP_CORE_CTRL4_H	(0x6302a)
#define REG_TOP_CORE_CTRL4_L	(0x6302d)
#define REG_TOP_CORE_CTRL5_H	(0x6302e)
#define REG_TOP_CORE_CTRL5_L	(0x63032)
#define REG_TOP_INTR_CTRL_L	(0x63050)

/* ISP REGISTER */
#define ISP1_REG_BASE                (0x65000)
#define ISP2_REG_BASE                (0x68000)
#define ISP1_ISP2_OFFSER (ISP2_REG_BASE - ISP1_REG_BASE)
#define REG_ISP_TOP0                 (0x0)
#define REG_ISP_TOP1                 (0x1)
#define REG_ISP_TOP2                 (0x2)
	#define SDE_ENABLE      (0x1 << 2)
#define REG_ISP_TOP3                 (0x3)
#define REG_ISP_TOP4                 (0x4)
#define REG_ISP_TOP5                 (0x5)
#define REG_ISP_TOP6                 (0x6)
	#define PIXEL_ORDER_GRBG  (0x0)
	#define PIXEL_ORDER_RGGB  (0x1)
	#define PIXEL_ORDER_BGGR  (0x2)
	#define PIXEL_ORDER_GBRG  (0x3)
#define REG_ISP_TOP7                 (0x7)
#define REG_ISP_TOP11                (0xb)
#define REG_ISP_TOP12                (0xc)
#define REG_ISP_TOP13                (0xd)
/*
 * ISP input size:
 * horizol: 0x10[12:8], 0x11[7:0]
 * vertical:0x12[12:8], 0x13[7:0]
 * ISP output size
 * horizol: 0x14[12:8], 0x15[7:0]
 * vertical:0x16[12:8], 0x17[7:0]
 */
#define REG_ISP_IN_WIDTH              (0x10)
#define REG_ISP_IN_HEIGHT             (0x12)
#define REG_ISP_OUT_WIDTH             (0x14)
#define REG_ISP_OUT_HEIGHT            (0x16)
/*
 * ISP LENC horizontal offset:
 * high:	0x18
 * low:		0x19
 * ISP LENC verital offset:
 * high:	0x1a
 * low:		0x1b
 */
#define REG_ISP_LENC_H_OFFSET	      (0x18)
#define REG_ISP_LENC_V_OFFSET		  (0x1a)
/*
 * down_scale_x: 0x24[7:0]
 * down_scale_y: 0x25[7:0]
 * up_scale_x: 0x26[8], 0x27[7:0]
 * up_scale_y: 0x28[8], 0x29[7:0]
 */
#define REG_ISP_SCALE_DOWN_X          (0x24)
#define REG_ISP_SCALE_DOWN_Y          (0x25)
#define REG_ISP_SCALE_UP_Y            (0x26)
#define REG_ISP_SCALE_UP_X            (0x28)
#define REG_ISP_TOP37			(0x2F)
	#define RGBHGMA_ENABLE		(1<<6)

/*
 * raw scale output
 * horizontal: 0x32[11:8], 0x33[7:0]
 * vertical:   0x34[11:8], 0x35[7:0]
 */
#define REG_ISP_RAW_DCW_OUT_H          (0x32)
#define REG_ISP_RAW_DCW_OUT_V          (0x34)
/*
 * YUV crop register
 * yuv crop_x :0xf0[11:8], 0xf1[7:0]
 * yuv crop_y :0xf2[11:8], 0xf3[7:0]
 * yuv crop_w :0xf4[11:8], 0xf5[7:0]
 * yuv crop_h :0xf6[11:8], 0xf7[7:0]
 */
#define REG_ISP_YUV_CROP_LEFT          (0xf0)
#define REG_ISP_YUV_CROP_TOP           (0xf2)
#define REG_ISP_YUV_CROP_WIDTH         (0xf4)
#define REG_ISP_YUV_CROP_HEIGHT        (0xf6)

/*
 * CIP register
 */
#define CIP_REG_BASE1               (0x65600)
#define CIP_REG_OFFSET              (CIP_REG_BASE1 - ISP1_REG_BASE)
#define REG_CIP_MIN_SHARPEN         (CIP_REG_OFFSET + 0x0c)
#define REG_CIP_MAX_SHARPEN         (CIP_REG_OFFSET + 0x0d)

/*
*Contrast register
*/
#define REG_CONTRAST_BASE1			(0x67000)
#define REG_CONTRAST_OFFSET			(REG_CONTRAST_BASE1 - ISP1_REG_BASE)
#define REG_CONTRAST_MIN			(REG_CONTRAST_OFFSET + 0x9)
#define REG_CONTRAST_MAX			(REG_CONTRAST_OFFSET + 0xa)


/*
 * SDE register
 */
#define SDE_REG_BASE1               (0x65b00)
#define SDE_REG_OFFSET              (SDE_REG_BASE1 - ISP1_REG_BASE)
#define REG_SDE_YUVTHRE00           (SDE_REG_OFFSET + 0x00)
#define REG_SDE_YUVTHRE01           (SDE_REG_OFFSET + 0x01)
#define REG_SDE_YUVTHRE10           (SDE_REG_OFFSET + 0x02)
#define REG_SDE_YUVTHRE11           (SDE_REG_OFFSET + 0x03)
#define REG_SDE_YUVTHRE20           (SDE_REG_OFFSET + 0x04)
#define REG_SDE_YUVTHRE21           (SDE_REG_OFFSET + 0x05)
#define REG_SDE_CTRL06              (SDE_REG_OFFSET + 0x06)
#define REG_SDE_YGAIN               (SDE_REG_OFFSET + 0x07)
#define REG_SDE_YGAIN_1             (SDE_REG_OFFSET + 0x08)
#define REG_SDE_UV_M00              (SDE_REG_OFFSET + 0x09)
#define REG_SDE_UV_M00_1            (SDE_REG_OFFSET + 0x0a)
#define REG_SDE_UV_M01              (SDE_REG_OFFSET + 0x0b)
#define REG_SDE_UV_M01_1            (SDE_REG_OFFSET + 0x0c)
#define REG_SDE_UV_M10              (SDE_REG_OFFSET + 0x0d)
#define REG_SDE_UV_M10_1            (SDE_REG_OFFSET + 0x0e)
#define REG_SDE_UV_M11              (SDE_REG_OFFSET + 0x0f)
#define REG_SDE_UV_M11_1            (SDE_REG_OFFSET + 0x10)
#define REG_SDE_YOFFSET             (SDE_REG_OFFSET + 0x11)
#define REG_SDE_YOFFSET_1           (SDE_REG_OFFSET + 0x12)
#define REG_SDE_UVOFFSET0           (SDE_REG_OFFSET + 0x13)
#define REG_SDE_UVOFFSET0_1         (SDE_REG_OFFSET + 0x14)
#define REG_SDE_UVOFFSET1           (SDE_REG_OFFSET + 0x15)
#define REG_SDE_UVOFFSET1_1         (SDE_REG_OFFSET + 0x16)
#define REG_SDE_CTRL17	            (SDE_REG_OFFSET + 0x17)
#define REG_SDE_HTHRE	            (SDE_REG_OFFSET + 0x18)
#define REG_SDE_HGAIN	            (SDE_REG_OFFSET + 0x19)

#define REG_SDE_BUF_BASE		(0x34800)
	#define SED_EOF_EN          (0x1 << 3)
	#define SED_EOF_DIS         (0x0 << 3)
/*
 * AWB Gain register
 */
#define AWB_REG_BASE1               (0x65300)
#define AWB_REG_OFFSET              (AWB_REG_BASE1 - ISP1_REG_BASE)
#define REG_AWB_GAIN_B              (AWB_REG_OFFSET + 0x00)
#define REG_AWB_GAIN_GB             (AWB_REG_OFFSET + 0x02)
#define REG_AWB_GAIN_GR             (AWB_REG_OFFSET + 0x04)
#define REG_AWB_GAIN_R              (AWB_REG_OFFSET + 0x06)
#define REG_AWB_GAIN_MAN_EN         (AWB_REG_OFFSET + 0x20)
	#define AWB_MAN_EN              (0x1)
	#define AWB_MAN_DIS             (0x0)

/*
 * AFC register
 */
#define AFC_REG_BASE1               (0x66100)
#define AFC_REG_OFFSET              (AFC_REG_BASE1 - ISP1_REG_BASE)
#define REG_AFC_CTRL0               (AFC_REG_OFFSET + 0x00)
	#define AFC_SINGLE_WIN          (0x0 << 1)
	#define AFC_5X5_WIN             (0x1 << 1)
#define REG_AFC_CTRL1               (AFC_REG_OFFSET + 0x01)
#define REG_AFC_CTRL2               (AFC_REG_OFFSET + 0x02)
#define REG_AFC_CTRL3               (AFC_REG_OFFSET + 0x03)
#define REG_AFC_WIN_X0              (AFC_REG_OFFSET + 0x04)
#define REG_AFC_WIN_Y0              (AFC_REG_OFFSET + 0x06)
#define REG_AFC_WIN_W0              (AFC_REG_OFFSET + 0x08)
#define REG_AFC_WIN_H0              (AFC_REG_OFFSET + 0x0a)
#define REG_AFC_WIN_W1              (AFC_REG_OFFSET + 0x0c)
#define REG_AFC_WIN_H1              (AFC_REG_OFFSET + 0x0e)

/*
 * AEC register
 */
#define AEC_REG_BASE1               (0x66400)
#define AEC_REG_OFFSET              (AEC_REG_BASE1 - ISP1_REG_BASE)
#define REG_AEC_STATWIN_LEFT        (AEC_REG_OFFSET + 0x01)
#define REG_AEC_STATWIN_TOP         (AEC_REG_OFFSET + 0x03)
#define REG_AEC_STATWIN_RIGHT       (AEC_REG_OFFSET + 0x05)
#define REG_AEC_STATWIN_BOTTOM      (AEC_REG_OFFSET + 0x07)
#define REG_AEC_WINLEFT             (AEC_REG_OFFSET + 0x09)
#define REG_AEC_WINTOP              (AEC_REG_OFFSET + 0x0d)
#define REG_AEC_WINWIDTH            (AEC_REG_OFFSET + 0x11)
#define REG_AEC_WINHEIGHT           (AEC_REG_OFFSET + 0x15)
#define REG_AEC_ROI_LEFT            (AEC_REG_OFFSET + 0x19)
#define REG_AEC_ROI_TOP             (AEC_REG_OFFSET + 0x1d)
#define REG_AEC_ROI_RIGHT           (AEC_REG_OFFSET + 0x21)
#define REG_AEC_ROI_BOTTOM          (AEC_REG_OFFSET + 0x25)
#define REG_AEC_ROI_SHIFT           (AEC_REG_OFFSET + 0x29)
#define REG_AEC_ROI_WEIGHT_IN       (AEC_REG_OFFSET + 0x2a)
#define REG_AEC_ROI_WEIGHT_OUT      (AEC_REG_OFFSET + 0x2b)
#define REG_AEC_WIN_WEIGHT0         (AEC_REG_OFFSET + 0x2e)
#define REG_AEC_WIN_WEIGHT1         (AEC_REG_OFFSET + 0x2f)
#define REG_AEC_WIN_WEIGHT2         (AEC_REG_OFFSET + 0x30)
#define REG_AEC_WIN_WEIGHT3         (AEC_REG_OFFSET + 0x31)
#define REG_AEC_WIN_WEIGHT4         (AEC_REG_OFFSET + 0x32)
#define REG_AEC_WIN_WEIGHT5         (AEC_REG_OFFSET + 0x33)
#define REG_AEC_WIN_WEIGHT6         (AEC_REG_OFFSET + 0x34)
#define REG_AEC_WIN_WEIGHT7         (AEC_REG_OFFSET + 0x35)
#define REG_AEC_WIN_WEIGHT8         (AEC_REG_OFFSET + 0x36)
#define REG_AEC_WIN_WEIGHT9         (AEC_REG_OFFSET + 0x37)
#define REG_AEC_WIN_WEIGHT10        (AEC_REG_OFFSET + 0x38)
#define REG_AEC_WIN_WEIGHT11        (AEC_REG_OFFSET + 0x39)
#define REG_AEC_WIN_WEIGHT12        (AEC_REG_OFFSET + 0x3a)

/*
 * band detection: only the first pipe has
 */
#define REG_BAND_DET_50HZ            (0x66700)
#define REG_BAND_DET_60HZ            (0x66702)

/*
 * MAC register
 */
#define MAC1_REG_BASE                (0x63b00)
#define MAC2_REG_BASE                (0x63e00)
#define MAC3_REG_BASE                (0x63f00)

#define REG_MAC_ADDR0                (0x00)
#define REG_MAC_ADDR1                (0x04)
#define REG_MAC_ADDR2                (0x08)
#define REG_MAC_ADDR3                (0x0c)
#define REG_MAC_ADDR4                (0x10)
#define REG_MAC_ADDR5                (0x14)
#define REG_MAC_ADDR6                (0x18)
#define REG_MAC_ADDR7                (0x1c)
#define REG_MAC_ADDR8                (0x20)
#define REG_MAC_ADDR9                (0x24)
#define REG_MAC_ADDR10               (0x28)
#define REG_MAC_ADDR11               (0x2c)
#define REG_MAC_ADDR12               (0x60)
#define REG_MAC_ADDR13               (0x64)
#define REG_MAC_ADDR14               (0x68)
#define REG_MAC_ADDR15               (0x6c)
#define REG_MAC_ADDR16               (0x70)
#define REG_MAC_ADDR17               (0x7c)
#define REG_MAC_RDY_ADDR0            (0x30)
	#define W_RDY_0                  (1 << 0)
	#define W_RDY_1                  (1 << 1)
	#define W_RDY_2                  (1 << 2)
	#define W_RDY_3                  (1 << 3)
#define REG_MAC_FRAME_CTRL0          (0x31)
	#define FORCE_OVERFLOW           (1 << 0)
	#define DROP_FLAG                (1 << 5)
	#define RD_CYCLE                 (1 << 6)
	#define WR_CYCLE                 (1 << 7)
#define REG_MAC_INT_STAT1            (0x32)
	#define W_INT_FRAME_START        (1 << 0)
	#define R_INT_DONE               (1 << 1)
	#define R_INT_SOF                (1 << 4)
	#define R_INT_UNDERFLOW          (1 << 6)
#define REG_MAC_INT_STAT0            (0x33)
	#define W_INT_START0             (1 << 0)
	#define W_INT_DONE0              (1 << 1)
	#define W_INT_DROP0              (1 << 2)
	#define W_INT_OVERFLOW0          (1 << 3)
	#define W_INT_START1             (1 << 4)
	#define W_INT_DONE1              (1 << 5)
	#define W_INT_DROP1              (1 << 6)
	#define W_INT_OVERFLOW1          (1 << 7)
#define REG_MAC_W_FMT_CTRL           (0x34)
#define REG_MAC_W_SWITCH_CTRL0       (0x35)
#define REG_MAC_W_OUT_W              (0x36)
#define REG_MAC_W_OUT_ME_W           (0x38)
#define REG_MAC_W_OUT_H              (0x3a)
#define REG_MAC_R_FMT_CTRL           (0x45)
#define REG_MAC_W_SWITCH_CTRL1       (0x46)
#define REG_MAC_R_OUTSTD_CTRL        (0x47)
	#define CHANNEL_CTR_EN           (1 << 2)
#define REG_MAC_W_PP_CTRL            (0x4c)
	#define PINGPANG_EN_WR           (1 << 0)
	#define PINGPANG_EN_RD           (1 << 1)
#define REG_MAC_INT_EN_CTRL          (0x53)
#define REG_MAC_INT_EN_CTRL1         (0x53)
#define REG_MAC_INT_EN_CTRL0         (0x54)
#define REG_MAC_RDY_ADDR1            (0x57)
	#define R_RDY_0                  (1 << 0)
	#define R_RDY_1                  (1 << 1)
#define REG_MAC_INT_SRC			(0xD1)
	#define INT_SRC_W1			(1 << 0)
	#define INT_SRC_W2			(1 << 1)
#define VIRT_IRQ_START			(1 << 0)
#define VIRT_IRQ_DONE			(1 << 1)
#define VIRT_IRQ_DROP			(1 << 2)
#define VIRT_IRQ_FIFO			(1 << 3)
#define virt_irq(port, irq)		((irq) << ((port) * 4))

#define MAC_INT_H	(W_INT_FRAME_START | R_INT_DONE)
#define MAC_INT_L	(W_INT_DONE0 | W_INT_DONE1 | \
			W_INT_DROP0 | W_INT_DROP1 | \
			W_INT_OVERFLOW0 | W_INT_OVERFLOW1 | \
			W_INT_START0 | W_INT_START1)

#define MAX_MAC_NUM                  (0x3)
#define MAX_PORT_NUM                 (0x3)
	#define MAC_PORT_W0         (0)
	#define MAC_PORT_W1         (1)
	#define MAC_PORT_R          (2)
#define MAX_MAC_ADDR_NUM             (0x4)

#define REG_MAC_FRAME_CTRL1		(0x78)
#define REG_MAC_WR_ID0			(0xAB)
#define REG_MAC_WR_ID1			(0xAC)
#define REG_MAC_WR_ID2			(0xAD)
#define REG_MAC_WR_ID3			(0xAE)
#define REG_MAC_RD_ID0			(0xAF)


/* I2C control register */
#define SCCB_MASTER1_REG_BASE       (0x63600)
#define SCCB_MASTER2_REG_BASE       (0x63700)

#define REG_SCCB_SPEED              (0x00)
#define REG_SCCB_SLAVE_ID           (0x01)
#define REG_SCCB_ADDRESS_H          (0x02)
#define REG_SCCB_ADDRESS_L          (0x03)
#define REG_SCCB_OUTPUT_DATA_H      (0x04)
#define REG_SCCB_OUTPUT_DATA_L      (0x05)
#define REG_SCCB_2BYTE_CONTROL      (0x06)
#define REG_SCCB_INPUT_DATA_H       (0x07)
#define REG_SCCB_INPUT_DATA_L       (0x08)
#define REG_SCCB_COMMAND            (0x09)
#define REG_SCCB_STATUS             (0x0a)
	#define SCCB_ST_BUSY	(0x1)
	#define SCCB_ST_IDLE	(0x0)

/* interrupt register*/
#define REG_INT_BASE                (0x63900)
#define REG_CMD_SET_REG0	(REG_INT_BASE + 0)
#define REG_CMD_SET_REG1	(REG_INT_BASE + 1)
#define REG_ISP_GP_REG0		(REG_INT_BASE + 0x10)
#define REG_ISP_GP_REG1		(REG_INT_BASE + 0x11)
#define REG_ISP_GP_REG2		(REG_INT_BASE + 0x12)
#define REG_ISP_GP_REG3		(REG_INT_BASE + 0x13)
#define REG_ISP_GP_REG4		(REG_INT_BASE + 0x14)
#define REG_ISP_GP_REG5		(REG_INT_BASE + 0x15)
#define REG_ISP_GP_REG6		(REG_INT_BASE + 0x16)
#define REG_ISP_GP_REG7		(REG_INT_BASE + 0x17)
#define REG_ISP_INT_CLR		(REG_INT_BASE + 0x20)
#define REG_ISP_INT_EN		(REG_INT_BASE + 0x24)
	#define INT_CMD_DONE		(1 << 30)
	#define INT_MAC_DONE_1		(1 << 21)
	#define INT_MAC_DONE_2		(1 << 22)
	#define INT_MAC_DONE_3		(1 << 23)
	#define INT_MAC_1		(1 << 24)
	#define INT_MAC_2		(1 << 25)
	#define INT_MAC_3		(1 << 26)
	#define EOF_INT                 (1 << 5)
	#define SOF_INT                 (1 << 4)
	#define MAC_INT                 (1 << 1)
	#define GPIO_INT                (1 << 0)
#define REG_ISP_INT_STAT            (REG_INT_BASE + 0x28)
#define REG_ISP_TMR_CTRL            (REG_INT_BASE + 0x30)
#define REG_ISP_TMR_CTRL_CNT        (REG_INT_BASE + 0x33)

/*
 * Firmware register
 */
#define REG_FW_AEC_RD_EXP1          (0x3020c)
#define REG_FW_AEC_RD_EXP2          (0x3003c)
#define REG_FW_AEC_RD_GAIN1         (0x30214)
#define REG_FW_AEC_RD_GAIN2         (0x30044)
#define REG_FW_AEC_RD_STATE1        (0x30205)
#define REG_FW_AEC_RD_STATE2        (0x30035)
#define REG_FW_AEC_RD_STATE3        (0x30373)
	#define AEC_STATE_STABLE   (0x1)
	#define AEC_STATE_UNSTABLE (0x0)
#define  REG_FW_MEAN_Y				(0x30206)

#define FW_P1_REG_BASE              (0x33000)
#define FW_P2_REG_BASE              (0x33600)
#define FW_P1_P2_OFFSET             (FW_P2_REG_BASE - FW_P1_REG_BASE)

#define REG_FW_IN_WIDTH             (0x000)
#define REG_FW_IN_HEIGHT            (0x002)
#define REG_FW_OUT_WIDTH            (0x004)
#define REG_FW_OUT_HEIGHT           (0x006)

#define REG_FW_SSOR_PRO_FRM_N       (0x008)
#define REG_FW_WORK_MODE            (0x009)
#define REG_FW_SSOR_TYPE            (0x00a)

#define REG_FW_EXPO_GAIN_WR         (0x011)
	#define EXPO_GAIN_HOST_WR     (0x0)
	#define EXPO_GAIN_MCU_WR      (0x1)

#define REG_FW_SSOR_GAIN_MODE       (0x012)
	#define SSOR_GAIN_Q4    (0x2)
#define REG_FW_AEC_EXP_SHIFT        (0x013)
#define REG_FW_AEC_TARGET_LOW       (0x014)
#define REG_FW_AEC_TARGET_HIGH      (0x015)
#define REG_FW_AEC_TARGET_EV	(0x017)
#define REG_FW_AEC_STABLE_RANGE0    (0x018)
#define REG_FW_AEC_STABLE_RANGE1    (0x019)

#define REG_FW_AEC_MANUAL_EN        (0x022)
	#define AEC_MANUAL		(0x1)
	#define AEC_AUTO		(0x0)

#define REG_FW_MAX_CAM_GAIN         (0x024)
#define REG_FW_MIN_CAM_GAIN         (0x026)
#define REG_FW_MAX_CAM_EXP          (0x028)
#define REG_FW_MIN_CAM_EXP          (0x02c)
#define REG_FW_AEC_MAN_EXP          (0x030)
#define REG_FW_AEC_MAN_GAIN         (0x034)

#define REG_FW_AEC_ALLOW_FRATIONAL_EXP         (0x03c)

#define REG_FW_AEC_MAX_FRATIONAL_EXP         (0x3303e)

#define REG_FW_BAND_FILTER_MODE     (0x040)
	#define BAND_FILTER_UNKNOWN		(0x0)
	#define BAND_FILTER_60HZ		(0x1)
	#define BAND_FILTER_50HZ		(0x2)
#define REG_FW_BAND_FILTER_EN       (0x041)
	#define BAND_FILTER_ENABLE		(0x1)
	#define BAND_FILTER_DISABLE		(0x0)
#define REG_FW_BAND_FILTER_LESS1BAND    (0x042)
	#define LESS_THAN_1_BAND_ENABLE		(0x1)
	#define LESS_THAN_1_BAND_DISABLE	(0x0)
#define REG_FW_AUTO_BAND_DETECTION  (0x043)
	#define AUTO_BAND_DETECTION_ON	(0x1)
	#define AUTO_BAND_DETECTION_OFF	(0x0)
#define REG_FW_BAND_60HZ            (0x044)
#define REG_FW_BAND_50HZ            (0x046)
#define BAND_VALUE_SHIFT    (0x4)

#define REG_FW_NIGHT_MODE           (0x048)
	#define NIGHT_MODE_ENABLE    (0x1)
	#define NIGHT_MODE_DISABLE   (0x0)

#define REG_FW_VTS                  (0x04c)
#define REG_FW_CURR_VTS             (0x050)
#define REG_FW_AEC_DGAIN_CHANNEL	(0x052)
#define REG_FW_AEC_GAIN_SHIFT		(0x053)

#define REG_FW_SSOR_DEV_ID          (0x056)
#define REG_FW_SSOR_I2C_OPT         (0x057)
#define REG_FW_SSOR_AEC_ADDR_0      (0x058)
#define REG_FW_SSOR_AEC_ADDR_1      (0x05a)
#define REG_FW_SSOR_AEC_ADDR_2      (0x05c)
#define REG_FW_SSOR_AEC_ADDR_3      (0x05e)
#define REG_FW_SSOR_AEC_ADDR_4      (0x060)
#define REG_FW_SSOR_AEC_ADDR_5      (0x062)
#define REG_FW_SSOR_AEC_ADDR_6      (0x064)
#define REG_FW_SSOR_AEC_ADDR_7      (0x066)
#define REG_FW_SSOR_AEC_ADDR_8      (0x068)
#define REG_FW_SSOR_AEC_ADDR_9      (0x06a)

#define REG_FW_SSOR_FRATIONAL_ADDR_0      (0x06c)
#define REG_FW_SSOR_FRATIONAL_ADDR_1      (0x06d)

#define REG_FW_SSOR_AEC_MSK_0       (0x070)
#define REG_FW_SSOR_AEC_MSK_1       (0x071)
#define REG_FW_SSOR_AEC_MSK_2       (0x072)
#define REG_FW_SSOR_AEC_MSK_3       (0x073)
#define REG_FW_SSOR_AEC_MSK_4       (0x074)
#define REG_FW_SSOR_AEC_MSK_5       (0x075)
#define REG_FW_SSOR_AEC_MSK_6       (0x076)
#define REG_FW_SSOR_AEC_MSK_7       (0x077)
#define REG_FW_SSOR_AEC_MSK_8       (0x078)
#define REG_FW_SSOR_AEC_MSK_9       (0x079)

#define REG_FW_AWB_TYPE             (0x088)
	#define AWB_AUTO		(0x2)
	#define AWB_CUSTOMIZED_BASE	(0x3)
#define REG_FW_CURRENT_GAIN		(0x204)
#define REG_FW_CMX_PREGAIN0         (0x3b1)
#define REG_FW_CMX_PREGAIN1         (0x3b2)
#define REG_FW_CMX_PREGAIN2         (0x3b3)

#define REG_FW_UV_MAX_SATURATON     (0x3f8)
#define REG_FW_UV_MIN_SATURATON     (0x3f3)

#define REG_FW_MAX_SHARPNESS	    (0x1ef)
#define REG_FW_MIN_SHARPNESS	    (0x1ea)

#define REG_FW_METADATA_LEN         (0x5e8)
#define REG_FW_METADATA_PORT        (0x5eb)

#define REG_FW_AUTO_FRAME_RATE      (0x5ed)
	#define AFR_ENABLE      (0x1)
	#define AFR_DISABLE     (0x0)

#define REG_FW_AFR_MAX_GAIN1         (0x5ee)
#define REG_FW_AFR_MAX_GAIN2         (0x5f0)
#define REG_FW_AFR_MAX_GAIN3         (0x5f2)
#define REG_FW_AFR_MIN_FPS1         (0x5f4)
#define REG_FW_AFR_MIN_FPS2         (0x5f5)
#define REG_FW_AFR_MIN_FPS3         (0x5f6)
	#define AFR_DEF_VAL_30FPS   (30 * 0x20)

#define REG_FW_MMU_CTRL	(0x33569)
	#define MMU_ENABLE      (0x1)
	#define MMU_DISABLE     (0x0)
#define REG_FW_CPU_CMD_ID	(0x33591)
#define REG_FW_IMG_ADDR_ID	(0x3359f)
#define REG_CURRENT_EXPOSURE	(0x34200)

#define FW_P1_REG_AF_BASE           (0x33c00)
#define FW_P2_REG_AF_BASE           (0x33e00)
#define FW_P1_P2_AF_OFFSET (FW_P2_REG_AF_BASE - FW_P1_REG_AF_BASE)
#define REG_FW_AF_OFFSET	(0x14)

#define REG_FW_AF_CURR_POS          (0x03e)
/*works with video AF*/
#define REG_FW_AF_FORCE_START       (0x06a)
	#define AF_FORCE_START  (0x1)
	#define AF_FORCE_STOP   (0x0)
/*
 * In snapshot AF mode, it will return to 0 after AF is finish
 * In continuous video AF mode, it will keep the value;
 */
#define REG_FW_AF_ACIVE             (0x06e)
	#define AF_START        (0x1)
	#define AF_STOP         (0x0)
#define REG_FW_AF_MODE              (0x06f)
	#define AF_SNAPSHOT     (0x0)
	#define AF_CONTINUOUS   (0x1)

#define REG_FW_FOCUS_I2C_OPT        (0x12a)
	#define FOCUS_PRIMARY    (0x1)
	#define FOCUS_SECONDARY  (0x2)
	#define FOCUS_DATA_16BIT (0x1 << 2)
	#define FOCUS_ADDR_16BIT (0x1 << 3)
#define REG_FW_FOCUS_I2C_ADDR       (0x12b)
#define REG_FW_FOCUS_REG_ADDR_MSB   (0x12c)
#define REG_FW_FOCUS_REG_ADDR_LSB   (0x12e)
#define REG_FW_AF_STATUS            (0x138)
	#define AF_ST_FOCUSING	(0x0)
	#define AF_ST_SUCCESS	(0x1)
	#define AF_ST_FAILED    (0x2)
	#define AF_ST_IDLE	(0x3)
#define REG_FW_VCM_TYPE             (0x140)
	#define VCM_TYPE_BUILD_IN (0x0)
	#define VCM_TYPE_AD5820  (0x1)
	#define VCM_TYPE_DW9714  (0x1)
	#define VCM_TYPE_AD5823  (0x2)
	#define VCM_HOST_WR      (0x3)

#define REG_FW_FOCUS_MAN_TRIGGER    (0x141)
	#define FOCUS_MAN_TRIGGER (0x1)

#define REG_FW_AF_MACRO_POS         (0x016)
#define REG_FW_AF_INFINITY_POS      (0x1ae)

#define REG_FW_AF_5X5_WIN_MODE      (0x1e0)
	#define AF_5X5_WIN_ENABLE    (0x1)
	#define AF_5X5_WIN_DISABLE   (0x0)

#define REG_FW_FOCUS_POS            (0x1ea)
#define REG_FW_FOCUS_MAN_STATUS     (0x1dc)

#define REG_ISP_LSC_PHASE		(0x331e9)
	#define LSC_MIRR		(0x1)
	#define LSC_FLIP		(0x2)

#define REG_HW_VERSION     (0x00bda)
#define REG_SWM_VERSION    (0x00bdc)
#define REG_FW_VERSION     (0x00bde)

u8 b52_readb(const u32 addr);
void b52_writeb(const u32 addr, const u8 value);
u16 b52_readw(const u32 addr);
void b52_writew(const u32 addr, const u16 value);
u32 b52_readl(const u32 addr);
void b52_writel(const u32 addr, const u32 value);
static inline void b52_writeb_mask(const u32 addr, u8 val, u8 mask)
{
	u8 v = b52_readb(addr);

	v = (v & ~mask) | (val & mask);
	b52_writeb(addr, v);
}

static inline void b52_writew_mask(const u32 addr, u16 val, u16 mask)
{
	u16 v = b52_readw(addr);

	v = (v & ~mask) | (val & mask);
	b52_writew(addr, v);
}

static inline void b52_writel_mask(const u32 addr, u32 val, u32 mask)
{
	u32 v = b52_readl(addr);

	v = (v & ~mask) | (val & mask);
	b52_writel(addr, v);
}

static inline void b52_setb_bit(const u32 addr, u8 val)
{
	b52_writeb_mask(addr, val, val);
}

static inline void b52_setw_bit(const u32 addr, u16 val)
{
	b52_writew_mask(addr, val, val);
}

static inline void b52_setl_bit(const u32 addr, u32 val)
{
	b52_writel_mask(addr, val, val);
}

static inline void b52_clearb_bit(const u32 addr, u8 val)
{
	b52_writeb_mask(addr, 0, val);
}

static inline void b52_clearw_bit(const u32 addr, u16 val)
{
	b52_writew_mask(addr, 0, val);
}

static inline void b52_clearl_bit(const u32 addr, u32 val)
{
	b52_writel_mask(addr, 0, val);
}
#ifdef CONFIG_ISP_USE_TWSI3
void b52_init_workqueue(void *data);
#endif
#endif
