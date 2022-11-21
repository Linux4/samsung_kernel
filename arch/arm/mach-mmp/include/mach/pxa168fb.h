/*
 * linux/arch/arm/mach-mmp/include/mach/pxa168fb.h
 *
 *  Copyright (C) 2009 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ASM_MACH_PXA168FB_H
#define __ASM_MACH_PXA168FB_H
/* ---------------------------------------------- */
/*              Header Files                      */
/* ---------------------------------------------- */
#include <linux/fb.h>
#include <plat/fb_ioctl.h>
#define VSYNC_DSI_CMD

#ifdef __KERNEL__
#include <linux/interrupt.h>
#include <linux/list.h>
#include <mach/cputype.h>
#include <linux/pm_qos.h>

/* Dumb interface */
#define PIN_MODE_DUMB_24		0
#define PIN_MODE_DUMB_18_SPI		1
#define PIN_MODE_DUMB_18_GPIO		2
#define PIN_MODE_DUMB_16_SPI		3
#define PIN_MODE_DUMB_16_GPIO		4
#define PIN_MODE_DUMB_12_SPI_GPIO	5
#define PIN_MODE_SMART_18_SPI		6
#define PIN_MODE_SMART_16_SPI		7
#define PIN_MODE_SMART_8_SPI_GPIO	8
#define PIN_MODE_DUMB_18_SMART_8	9
#define PIN_MODE_DUMB_16_SMART_8_SPI	10
#define PIN_MODE_DUMB_16_SMART_8_GPIO	11
#define PIN_MODE_DUMB_16_DUMB_16	12

/*I/O pad control*/
#define CFG_BOUNDARY_1KB	(1<<5)	/*no crossing 1KB boundary*/
#define CFG_BOUNDARY_4KB	(0<<5)	/*no crossing 4KB boundary*/
#define CFG_CYC_BURST_LEN16	(1<<4)
#define CFG_CYC_BURST_LEN8	(0<<4)

/* Dumb interface pin allocation */
#define DUMB_MODE_RGB565		0
#define DUMB_MODE_RGB565_UPPER		1
#define DUMB_MODE_RGB666		2
#define DUMB_MODE_RGB666_UPPER		3
#define DUMB_MODE_RGB444		4
#define DUMB_MODE_RGB444_UPPER		5
#define DUMB_MODE_RGB888		6

/* default fb buffer size WVGA-32bits */
#if defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7)
#define DEFAULT_FB_SIZE	((1024 * 600 * 4 * 3)+ 4096)
#else
#define DEFAULT_FB_SIZE	((800 * 480 * 8)+ 4096)
#endif

/*
 * Buffer pixel format
 * bit0 is for rb swap.
 * bit12 is for Y UorV swap
 */
#define PIX_FMT_RGB565		0
#define PIX_FMT_BGR565		1
#define PIX_FMT_RGB1555		2
#define PIX_FMT_BGR1555		3
#define PIX_FMT_RGB888PACK	4
#define PIX_FMT_BGR888PACK	5
#define PIX_FMT_RGB888UNPACK	6
#define PIX_FMT_BGR888UNPACK	7
#define PIX_FMT_RGBA888		8
#define PIX_FMT_BGRA888		9
#define PIX_FMT_YUV422PACK	10
#define PIX_FMT_YVU422PACK	11
#define PIX_FMT_YUV422PLANAR	12
#define PIX_FMT_YVU422PLANAR	13
#define PIX_FMT_YUV420PLANAR	14
#define PIX_FMT_YVU420PLANAR	15
#define PIX_FMT_PSEUDOCOLOR	20
#define PIX_FMT_YUYV422PACK	(0x1000|PIX_FMT_YUV422PACK)
#define PIX_FMT_YUV422PACK_IRE_90_270	(0x1000|PIX_FMT_RGB888UNPACK)

/*
 * panel interface
 */
enum {
	DPI = 0,
	DSI2DPI = 1,
	DSI = 2,
	LVDS =4,
};
/* flags indicate update region for regshadow
 * bit[0] = 1, address need update;
 * bit[1] = 1, video mode need update;
 * bit[2] = 1, viewport control info need update;
 */
#define UPDATE_ADDR (0x1 << 0)
#define UPDATE_MODE (0x1 << 1)
#define UPDATE_VIEW (0x1 << 2)

struct regshadow {
	u32	flags;

	/* address */
	u32	paddr0[3];
	u32	paddr1[3];

	/* video mode */
	u32	dma_ctrl0;

	/* viewport info*/
	u32	pitch[2];
	u32	start_point;
	u32	src_size;
	u32	dst_size;
	u32	zoom;
};

/* shadowreg list for flip mode */
struct regshadow_list {
	struct regshadow shadowreg;
	struct list_head dma_queue;
};

struct pxa168fb_vdma_info {
	struct device	*dev;
	void		*reg_base;
	int		ch;
	/* path id, 0->panel, 1->TV, 2->panel2 */
	int		path;
	int		pix_fmt;
	unsigned int	sram_size;
	unsigned int	rotation;
	unsigned int	yuv_format;
	unsigned int	vdma_lines;
	unsigned int	sram_paddr;
	unsigned int	sram_vaddr;
	unsigned	vid:1,
			active:1,
			dma_on:1,
			enable:1;
};
/*
 * PXA LCD controller private state.
 */
struct pxa168fb_info {
	struct device		*dev;
	struct clk		*clk;
	__kernel_suseconds_t	frm_usec;
	int			id;
	int			vid;
	void			*reg_base;
	unsigned char		*filterBufList[MAX_QUEUE_NUM][3];
	struct regshadow_list	buf_freelist;
	struct regshadow_list	buf_waitlist;
	struct regshadow_list	*buf_current;
	dma_addr_t		fb_start_dma;
	void			*fb_start;
	int			fb_size;
	u32			scrn_act_bak;
	atomic_t		op_count;
	atomic_t		irq_en_count;
	atomic_t		w_intr;
	atomic_t		vsync_cnt;
	wait_queue_head_t	w_intr_wq;
	struct mutex		access_ok;
	spinlock_t		buf_lock;
	struct _sOvlySurface    surface;
	struct _sOvlySurface    surface_bak;
	struct regshadow	shadowreg;
	struct _sColorKeyNAlpha ckey_alpha;
	struct fb_videomode	dft_vmode;
	unsigned int		pseudo_palette[16];
	struct fb_info          *fb_info;
	int			pix_fmt;
	int			debug;
	unsigned		is_blanked:1,
				surface_set:1,
				active:1;
	/* indicate dma on/off requirement from user space */
	int			dma_on;
	/* lock for variables, e.g. active */
	spinlock_t		var_lock;
#ifdef CONFIG_MACH_ARUBA_TD
	/* mem_status:
	* 0: DMA mem is from DMA region.
	* 1: DMA mem is from normal region.
	* -1: no frame buffer.
	*/
	int			mem_status;
#endif
	unsigned                wait_vsync;
	unsigned                vsync_en;
	uint64_t		vsync_ts_nano;
	struct work_struct	vsync_work;
	struct delayed_work	panic_work;

	/* Compatibility mode global switch .....
	 *
	 * This is a secret switch for user space programs that may want to
	 * select color spaces and set resolutions the same as legacy PXA
	 * display drivers. The switch is set and unset by setting a specific
	 * value in the var_screeninfo.nonstd variable.
	 *
	 * To turn on/off compatibility with older PXA:
	 * set the MSB of nonstd to 0xAA to turn it on.
	 * set the MSB of nonstd to 0x55 to turn it off.
	 */
	unsigned int	compat_mode;

	unsigned		irq_mask;
	/* if display is enabled in uboot, skip power on sequence in kernel */
	unsigned		skip_pw_on;

	struct fb_var_screeninfo var_bak;

	/* gamma correction */
	struct mvdisp_gamma	gamma;
	struct pm_qos_request   qos_idle;
#ifdef VSYNC_DSI_CMD
		//wait_queue_head_t vsync_detect_wq;
		//int vsync_detected;
		struct mutex cmd_mutex; /* Frequency change mutex */
		struct mutex vsync_mutex; /* Frequency change mutex */

#endif	
	
};

struct dsi_phy {
	unsigned int hs_prep_constant;    /* Unit: ns. */
	unsigned int hs_prep_ui;
	unsigned int hs_zero_constant;
	unsigned int hs_zero_ui;
	unsigned int hs_trail_constant;
	unsigned int hs_trail_ui;
	unsigned int hs_exit_constant;
	unsigned int hs_exit_ui;
	unsigned int ck_zero_constant;
	unsigned int ck_zero_ui;
	unsigned int ck_trail_constant;
	unsigned int ck_trail_ui;
	unsigned int req_ready;
	unsigned int wakeup_constant;
	unsigned int wakeup_ui;
	unsigned int lpx_constant;
	unsigned int lpx_ui;
};

struct dsi_info {
	unsigned	id;
	unsigned	regs;
	unsigned	lanes;
	unsigned	bpp;
	unsigned	rgb_mode;
	unsigned	burst_mode;
	unsigned	master_mode;
	unsigned	lpm_line_en;
	unsigned	lpm_frame_en;
	unsigned	last_line_turn;
	unsigned	hex_slot_en;
	unsigned	all_slot_en;
	unsigned	hbp_en;
	unsigned	hact_en;
	unsigned	hfp_en;
	unsigned	hex_en;
	unsigned	hlp_en;
	unsigned	hsa_en;
	unsigned	hse_en;
	unsigned	eotp_en;
	struct dsi_phy  *phy;
};

/* LVDS info */
struct lvds_info {
#define LVDS_SRC_PN	0
#define LVDS_SRC_CMU	1
#define LVDS_SRC_PN2	2
#define LVDS_SRC_TV	3
	u32	src;
#define LVDS_FMT_24BIT	0
#define LVDS_FMT_18BIT	1
	u32	fmt;
};

/*
 * CMU information
 */
struct cmu_calibration {
	int left;
	int right;
	int top;
	int bottom;
};
struct cmu_res {
	int width;
	int height;
};

/*
 * PXA fb machine information
 */
struct pxa168fb_mach_info {
	char	id[16];
	unsigned int	sclk_src;
	unsigned int	sclk_div;

	int		num_modes;
	struct fb_videomode *modes;
	unsigned int max_fb_size;
	unsigned int xres_virtual;
	/* recovery mode of system */


	/*
	 * Pix_fmt
	 */
	unsigned	pix_fmt;
	/*
	 *ISR clear mask
	 */
	unsigned	isr_clear_mask;
	/*
	 * I/O pad control.
	 */
	unsigned int	io_pad_ctrl;

	/*
	 * Dumb panel -- assignment of R/G/B component info to the 24
	 * available external data lanes.
	 */
	unsigned	dumb_mode:4;
	unsigned	panel_rgb_reverse_lanes:1;

	/*
	 * Dumb panel -- GPIO output data.
	 */
	unsigned	gpio_output_mask:8;
	unsigned	gpio_output_data:8;

	/*
	 * Dumb panel -- configurable output signal polarity.
	 */
	unsigned	invert_composite_blank:1;
	unsigned	invert_pix_val_ena:1;
	unsigned	invert_pixclock:1;
	unsigned	invert_vsync:1;
	unsigned	invert_hsync:1;
	unsigned	panel_rbswap:1;
	unsigned	active:1;
	unsigned	enable_lcd:1;
	unsigned	mmap:2;

	/*
	 * dither option
	 */
	unsigned	dither_en:1;
#define DITHER_TBL_4X4	0
#define DITHER_TBL_4X8	1
	unsigned	dither_table:1;
#define DITHER_MODE_RGB444	0
#define DITHER_MODE_RGB565	1
#define DITHER_MODE_RGB666	2
	unsigned	dither_mode:2;

	/*
	 * SPI control
	 */
	unsigned int	spi_ctrl;
	unsigned int	spi_gpio_cs;
	unsigned int	spi_gpio_reset;

	/*
	 * panel interface
	*/
	unsigned int	phy_type;
	int		(*phy_init)(struct pxa168fb_info *);
	void		*phy_info;

	/*
	 * vdma option
	 */
	unsigned int vdma_enable;
	unsigned int sram_size;
	/*
	 * power on/off function.
	 */
	int (*pxa168fb_lcd_power)(struct pxa168fb_info *,
			 unsigned int, unsigned int, int);

	/*
	 * dsi to dpi setting function
	 */
	int (*dsi2dpi_set)(struct pxa168fb_info *);
	int (*xcvr_reset)(struct pxa168fb_info *);

	/* init config for panel via dsi */
	void (*dsi_panel_config)(struct pxa168fb_info *);
	/*
	 * special ioctls
	 */
	int (*ioctl)(struct fb_info *info, unsigned int cmd, unsigned long arg);
	/*CMU platform calibration*/
	struct cmu_calibration cmu_cal[3];
	struct cmu_calibration cmu_cal_letter_box[3];
};

struct fbi_info {
	struct pxa168fb_info *fbi[3];
};

enum dsi_packet_di {
	/* for sleep in/out display on/off */
	DSI_DI_DCS_SWRITE = 0x5,
	/* for set_pixel_format */
	DSI_DI_DCS_SWRITE1 = 0x15,
	DSI_DI_DCS_LWRITE = 0x39,
	DSI_DI_DCS_READ = 0x6,
	DSI_DI_SET_MAX_PKT_SIZE = 0x37,
	/* for video mode off */
	DSI_DI_PERIPHE_CMD_OFF = 0x22,
	/* for video mode on */
	DSI_DI_PERIPHE_CMD_ON = 0x32,
	/* for long packet gen command */
	DSI_DI_DCS_GEN_LWRITE = 0x29,	
};

enum dsi_rx_packet_di {
	DSI_DI_ACK_ERR_RESP = 0x2,
	DSI_DI_EOTP = 0x8,
	DSI_DI_GEN_READ1_RESP = 0x11,
	DSI_DI_GEN_READ2_RESP = 0x12,
	DSI_DI_GEN_LREAD_RESP = 0x1A,
	DSI_DI_DCS_READ1_RESP = 0x21,
	DSI_DI_DCS_READ2_RESP = 0x22,
	DSI_DI_DCS_LREAD_RESP = 0x1C,
};

struct dsi_cmd_desc {
	enum dsi_packet_di data_type;
	u8  lp;    /*command tx through low power mode or high-speed mode */
	u32 delay; /* time to delay */
	u32 length; /* cmds length */
	u8 *data;
};

#define BPP_16		0x55
#define BPP_18		0x66
#define BPP_24		0x77

/* LCD partial display */
#define THRESHOLD_PN	64
#define THRESHOLD_TV	100
#define BURST_LEN		64

/* DSI burst mode */
#define DSI_BURST_MODE_SYNC_PULSE			0x0
#define DSI_BURST_MODE_SYNC_EVENT			0x1
#define DSI_BURST_MODE_BURST				0x2

/* DSI input data RGB mode */
#define DSI_LCD_INPUT_DATA_RGB_MODE_565			0
#define DSI_LCD_INPUT_DATA_RGB_MODE_666PACKET		1
#define DSI_LCD_INPUT_DATA_RGB_MODE_666UNPACKET		2
#define DSI_LCD_INPUT_DATA_RGB_MODE_888			3

/* DSI maximum packet data buffer */
#define DSI_MAX_DATA_BYTES	256
/* LCD ISR clear mask */
#define LCD_ISR_CLEAR_MASK_PXA168       0xffffffff
#define LCD_ISR_CLEAR_MASK_PXA910       0xffff00cc

struct dsi_buf {
	enum dsi_rx_packet_di data_type;
	u32 length; /* cmds length */
	u8 data[DSI_MAX_DATA_BYTES];
};

extern struct lcd_regs *get_regs(int id);
extern struct cmu_calibration cmu_cal[3];
extern struct cmu_calibration cmu_cal_letter_box[3];
extern struct cmu_res res;

extern u32 dma_ctrl_read(int id, int ctrl1);
extern void dma_ctrl_write(int id, int ctrl1, u32 value);
extern void dma_ctrl_set(int id, int ctrl1, u32 mask, u32 value);
extern void panel_dma_ctrl(bool flag);
extern void irq_mask_set(int id, u32 mask, u32 val);
extern void irq_status_clear(int id, u32 mask);
extern int lcd_clk_get(int id, u32 type);
extern void lcd_clk_set(int id, u32 type, u32 mask, u32 val);

extern void pxa988_reserve_fb_mem(void);
extern int pxa168fb_spi_send(struct pxa168fb_info *fbi, void *cmd,
				 int count, unsigned int spi_gpio_cs);
extern int pxa688_cmu_ioctl(struct fb_info *info, unsigned int cmd,
				 unsigned long arg);
extern void irq_mask_eof(int id);
extern void irq_unmask_eof(int id);
#ifdef CONFIG_PXA688_PHY
/* dsi related */
#ifdef VSYNC_DSI_CMD
extern void pxa168_dsi_cmd_array_tx(struct pxa168fb_info *fbi, struct dsi_cmd_desc cmds[],
		int count);
		
extern void dsi_prepare_cmd_array_tx(struct pxa168fb_info *fbi, struct dsi_cmd_desc cmds[],
		int count, u8 *buffer, u8 *packet_len);
extern void dsi_send_prepared_cmd_tx(struct pxa168fb_info *fbi, struct dsi_cmd_desc cmds,
		u8 *buffer,u8 len);
#endif
extern void dsi_cmd_array_tx(struct pxa168fb_info *fbi,
		struct dsi_cmd_desc cmds[], int count);
extern void dsi_cmd_array_rx(struct pxa168fb_info *fbi, struct dsi_buf *dbuf,
		struct dsi_cmd_desc cmds[], int count);
extern void set_dsi_low_power_mode(struct pxa168fb_info *fbi);
extern void dsi_cclk_set(struct pxa168fb_info *fbi, int en);
extern void dsi_set_dphy(struct pxa168fb_info *fbi);
extern void dsi_reset(struct pxa168fb_info *fbi, int hold);
extern void dsi_set_controller(struct pxa168fb_info *fbi);
extern void dsi_lanes_enable(struct pxa168fb_info *fbi, int en);
extern void dsi_set_panel_interface(struct pxa168fb_info *fbi, bool on);
extern void dsi_set_panel_intf(struct pxa168fb_info *fbi, bool on);
/* LVDS related */
extern int pxa688_lvds_init(struct pxa168fb_info *fbi);
#else
static inline void dsi_cmd_array_tx(struct pxa168fb_info *fbi,
		struct dsi_cmd_desc cmds[], int count){}
static inline void dsi_cmd_array_rx(struct pxa168fb_info *fbi,
		struct dsi_buf *dbuf, struct dsi_cmd_desc cmds[], int count){}
static inline void set_dsi_low_power_mode(struct pxa168fb_info *fbi){}
static inline void dsi_cclk_set(struct pxa168fb_info *fbi, int en){}
static inline void dsi_set_dphy(struct pxa168fb_info *fbi){}
static inline void dsi_reset(struct pxa168fb_info *fbi, int hold){}
static inline void dsi_set_controller(struct pxa168fb_info *fbi){}
static inline void dsi_lanes_enable(struct pxa168fb_info *fbi, int en){}
static inline void dsi_set_panel_intf(struct pxa168fb_info *fbi, bool on){}

static inline int pxa688_lvds_init(struct pxa168fb_info *fbi)
{
	return -EINVAL;
}
#endif

/* VDMA related */
#ifdef CONFIG_PXA688_VDMA
#define EOF_TIMEOUT 20
extern struct pxa168fb_vdma_info *request_vdma(int path, int vid);
extern void pxa688_vdma_init(struct pxa168fb_vdma_info *lcd_vdma);
extern void pxa688_vdma_config(struct pxa168fb_vdma_info *lcd_vdma);
extern void pxa688_vdma_release(int path, int vid);
extern int pxa688_vdma_en(struct pxa168fb_vdma_info *lcd_vdma,
			int enable, int vid);
static inline void vdma_info_update(struct pxa168fb_vdma_info *lcd_vdma,
		int active, int dma_on, int pix_fmt, unsigned int rotation,
		unsigned int yuv_format)
{
	lcd_vdma->active = active;
	lcd_vdma->dma_on = dma_on;
	lcd_vdma->pix_fmt = pix_fmt;
	lcd_vdma->rotation = rotation;
	lcd_vdma->yuv_format = yuv_format;
}
#else
static inline struct pxa168fb_vdma_info *request_vdma(int path, int vid)
{
	return 0;
}
static inline void pxa688_vdma_init(struct pxa168fb_vdma_info *lcd_vdma) {}
static inline void pxa688_vdma_config(struct pxa168fb_vdma_info *lcd_vdma) {}
static inline void pxa688_vdma_release(int path, int vid) {}
static inline int pxa688_vdma_en(struct pxa168fb_vdma_info *lcd_vdma,
				int enable, int vid)
{
	return -EINVAL;
}
static inline void vdma_info_update(struct pxa168fb_vdma_info *lcd_vdma,
		int active, int dma_on, int pix_fmt, unsigned int rotation,
		unsigned int yuv_format) {}
#endif

/* misc */
extern int fb_filter;
extern int fb_vsmooth;
extern int gfx_vsmooth;
extern int vid_vsmooth;
#ifdef CONFIG_MACH_HENDRIX
extern struct fbi_info gfx_info;
extern struct fbi_info ovly_info;
#endif
extern struct device_attribute dev_attr_misc;
extern void pxa168fb_update_modes(struct pxa168fb_info *fbi ,unsigned int index, unsigned int panel);

#ifdef CONFIG_PXA688_MISC
extern int pxa688fb_vsmooth_set(int id, int vid, int en);
extern int pxa688fb_partdisp_set(struct mvdisp_partdisp grap);
extern void pxa688fb_partdisp_update(int id);
extern int gamma_set(int path, int flag, char *gamma_table);
extern void gamma_dump(int path, int lines);
extern void dither_set(struct pxa168fb_info *fbi,
	int table, int mode, int enable);
#else
static inline int pxa688fb_vsmooth_set(int id, int vid, int en)
{
	return 0;
}
static inline int pxa688fb_partdisp_set(struct mvdisp_partdisp grap)
{
	return -EINVAL;
}
static inline void pxa688fb_partdisp_update(int id) { }
#define gamma_set(path, flag, gamma_table)		do {} while (0)
#define gamma_dump(path, lines)				do {} while(0)
#define dither_set(fbi, table, mode, enable)	do {} while (0)
#endif

#endif /* __KERNEL__ */
#endif /* __ASM_MACH_PXA168FB_H */
