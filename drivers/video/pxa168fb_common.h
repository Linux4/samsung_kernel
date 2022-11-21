#ifndef _PXA168FB_COMMON_
#define _PXA168FB_COMMON_

#include <mach/pxa168fb.h>
#include "pxa168fb.h"

#define RESET_BUF	0x1
#define FREE_ENTRY	0x2

#define DBG_VSYNC_SHIFT	(0)
#define DBG_VSYNC_MASK	(0x3 << DBG_VSYNC_SHIFT)
#define DBG_VSYNC_PATH	((debug_flag & DBG_VSYNC_MASK) >> DBG_VSYNC_SHIFT)
#define DBG_ERR_SHIFT	(2)
#define DBG_ERR_MASK	(0x1 << DBG_ERR_SHIFT)
#define DBG_ERR_IRQ	((debug_flag & DBG_ERR_MASK) >> DBG_ERR_SHIFT)
#define DBG_IRQ_PATH	(debug_flag & DBG_VSYNC_MASK)

#ifdef CONFIG_CPU_PXA988
#define NEED_VSYNC(fbi)	(fbi->wait_vsync)
#else
#define NEED_VSYNC(fbi)	(fbi->wait_vsync && dispd_dma_enabled(fbi))
#endif

#define DUMP_SPRINTF	(1 << 0)
#define DUMP_PRINFO	(1 << 1)
#define mvdisp_dump(flag, fmt, ...)	do {			\
	if (flag & DUMP_SPRINTF)				\
		s += sprintf(buf + s, fmt, ##__VA_ARGS__);	\
	if (flag & DUMP_PRINFO)					\
		pr_info(fmt, ##__VA_ARGS__);			\
} while (0)

extern int fb_share;
extern int gfx_udflow_count;
extern int vid_udflow_count;
extern int irq_retry_count;
extern int axi_err_count;
extern int debug_flag;
extern struct fbi_info gfx_info;
extern struct fbi_info ovly_info;
extern struct device_attribute dev_attr_lcd;
extern struct device_attribute dev_attr_phy;
extern struct device_attribute dev_attr_vdma;
extern struct attribute_group pxa_android_power_sysfs_files;

#ifdef CONFIG_LCD_MDNIE_ENABLE
extern struct device_attribute dev_attr_tuning;
#endif
extern struct device_attribute dev_attr_lvds_clk_switch;

extern u32 clk_reg(int id, u32 type);
extern void vsync_check_count(void);
extern int unsupport_format(struct pxa168fb_info *fbi,
	 struct _sViewPortInfo viewPortInfo, FBVideoMode videoMode);
extern int convert_pix_fmt(u32 vmode);
extern int set_pix_fmt(struct fb_var_screeninfo *var, int pix_fmt);
extern int determine_best_pix_fmt(struct fb_var_screeninfo *var,
	struct pxa168fb_info *fbi);
extern int pxa168fb_check_var(struct fb_var_screeninfo *var,
	 struct fb_info *fi);
extern int check_surface(struct fb_info *fi, struct _sOvlySurface *surface);
extern int check_surface_addr(struct fb_info *fi,
	 struct _sOvlySurface *surface);
extern int check_modex_active(struct pxa168fb_info *fbi);
extern void *pxa168fb_alloc_framebuffer(size_t size, dma_addr_t *dma);
extern void pxa168fb_free_framebuffer(size_t size, void *vaddr,
		dma_addr_t *dma);

extern void buf_endframe(void *point);
extern void clear_buffer(struct pxa168fb_info *fbi);
extern void pxa168fb_list_init(struct pxa168fb_info *fbi);
extern int flip_buffer(struct fb_info *info, unsigned long arg);
extern int flip_buffer_vsync(struct fb_info *info, unsigned long arg);
extern int get_freelist(struct fb_info *info, unsigned long arg);

extern void set_dma_active(struct pxa168fb_info *fbi);
extern int dispd_dma_enabled(struct pxa168fb_info *fbi);
extern void wait_for_vsync(struct pxa168fb_info *fbi, unsigned char param);
extern void pxa168fb_misc_update(struct pxa168fb_info *fbi);
extern void set_start_address(struct fb_info *info, int xoffset,
		 int yoffset, struct regshadow *shadowreg);
extern void set_dma_control0(struct pxa168fb_info *fbi,
		 struct regshadow *shadowreg);
extern void set_screen(struct pxa168fb_info *fbi, struct regshadow *shadowreg);
extern int pxa168fb_set_var(struct fb_info *info,
		 struct regshadow *shadowreg, u32 flags);
extern void pxa168fb_set_regs(struct pxa168fb_info *fbi,
		 struct regshadow *shadowreg);
extern irqreturn_t pxa168_fb_isr(int id);
extern irqreturn_t mmp_v4l2_isr(int id);

#ifdef CONFIG_EOF_FC_WORKAROUND
extern atomic_t displayon;
extern atomic_t fc_trigger;
extern int wakeup_fc_seq(void);
extern int wakeup_ddr_fc_seq(void);
#endif

/* power api usage for android early supsend support */
extern void android_stop_drawing(void);
extern void android_start_drawing(void);

#ifdef CONFIG_VIDEO_MVISP
extern void isp_reset_clock(void);
#endif
#endif
