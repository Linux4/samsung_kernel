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

#define pr_fmt(fmt)		"dispc: " fmt

#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/delay.h>
#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || \
					defined(CONFIG_SPRD_SCX35_DMC_FREQ))
#include <linux/devfreq.h>
#endif
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <asm/cacheflush.h>
#include "sprdfb_dispc_reg.h"
#include "sprdfb_panel.h"
#include "sprdfb.h"
#include "sprdfb_chip_common.h"
#ifdef CONFIG_FB_DYNAMIC_FPS_SUPPORT
#include "sprdfb_notifier.h"
#endif
#include <mach/cpuidle.h>
#include <mach/board.h>

#ifdef CONFIG_FB_SCX15
#define DISPC_CLOCK_PARENT	("clk_192m")
#define DISPC_CLOCK		(192*1000000)
#define DISPC_DBI_CLOCK_PARENT	("clk_256m")
#define DISPC_DBI_CLOCK		(256*1000000)
#ifdef CONFIG_FB_DISPC_DYNAMIC_PCLK_SUPPORT
#define DISPC_DPI_CLOCK_PARENT	("clk_384m")
#define SPRDFB_DPI_CLOCK_SRC	(384000000)
#else
#define DISPC_DPI_CLOCK_PARENT	("clk_192m")
#define SPRDFB_DPI_CLOCK_SRC	(192000000)
#endif

#else
#define DISPC_CLOCK_PARENT	("clk_256m")
#define DISPC_CLOCK		(256*1000000)
#define DISPC_DBI_CLOCK_PARENT	("clk_256m")
#define DISPC_DBI_CLOCK		(256*1000000)
#define DISPC_DPI_CLOCK_PARENT	("clk_384m")
#define SPRDFB_DPI_CLOCK_SRC	(384000000)

#endif

#define DISPC_EMC_EN_PARENT	("clk_aon_apb")

#ifdef CONFIG_FB_SCX15
#define SPRDFB_BRIGHTNESS	(0x02<<16) /*(0x03<<16)  9-bits*/
#define SPRDFB_CONTRAST		(0x12A<<0) /*10-bits*/
#define SPRDFB_OFFSET_U		(0x80<<16) /*8-bits*/
#define SPRDFB_SATURATION_U	(0x123<<0) /*(0x12A<<0) 10-bits*/
#define SPRDFB_OFFSET_V		(0x80<<16) /*8-bits*/
#define SPRDFB_SATURATION_V	(0x123<<0) /*(0x12A<<0) 10-bits*/
#else
#define SPRDFB_CONTRAST		(74)
#define SPRDFB_SATURATION	(73)
#define SPRDFB_BRIGHTNESS	(2)
#endif

typedef enum {
	SPRDFB_DYNAMIC_CLK_FORCE,	/*force enable/disable*/
	SPRDFB_DYNAMIC_CLK_REFRESH,	/*enable for refresh/display_overlay*/
	SPRDFB_DYNAMIC_CLK_COUNT,	/*enable disable in pairs*/
	SPRDFB_DYNAMIC_CLK_MAX,
} SPRDFB_DYNAMIC_CLK_SWITCH_E;

struct sprdfb_dispc_context {
	struct clk *clk_dispc;
	struct clk *clk_dispc_dpi;
	struct clk *clk_dispc_dbi;
	struct clk *clk_dispc_emc;
	bool	is_inited;
	bool	is_first_frame;
	bool	clk_is_open;
	bool	clk_is_refreshing;
	int	clk_open_count;
	spinlock_t clk_spinlock;

	struct sprdfb_device *dev;

	uint32_t vsync_waiter;
	wait_queue_head_t vsync_queue;
	uint32_t vsync_done;

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	/* overlay */
	uint32_t  overlay_state;  /*0-closed, 1-configed, 2-started*/
#endif

#ifdef CONFIG_FB_VSYNC_SUPPORT
	uint32_t		waitfor_vsync_waiter;
	wait_queue_head_t	waitfor_vsync_queue;
	uint32_t		waitfor_vsync_done;
#endif
#ifdef CONFIG_FB_MMAP_CACHED
	struct vm_area_struct *vma;
#endif
};

static struct sprdfb_dispc_context dispc_ctx = {0};
extern void dsi_irq_trick(uint32_t int_id, uint32_t int_status);
extern void sprdfb_panel_suspend(struct sprdfb_device *dev);
extern void sprdfb_panel_resume(struct sprdfb_device *dev, bool from_deep_sleep);
extern void sprdfb_panel_before_refresh(struct sprdfb_device *dev);
extern void sprdfb_panel_after_refresh(struct sprdfb_device *dev);
extern void sprdfb_panel_invalidate(struct panel_spec *self);
extern void sprdfb_panel_invalidate_rect(struct panel_spec *self,
				uint16_t left, uint16_t top,
				uint16_t right, uint16_t bottom);

extern uint32_t lcd_base_from_uboot;

#ifdef CONFIG_FB_DYNAMIC_FPS_SUPPORT
extern int32_t dsi_dpi_init(struct sprdfb_device *dev);
static int32_t sprdfb_dispc_change_fps(struct sprdfb_device *dev,
							int fps_level);
static int32_t sprdfb_dispc_notify_change_fps(struct dispc_dbs *h,
							int fps_level);

int32_t original_fps = -1;
struct dispc_dbs sprd_fps_notify = {
	.level = 0,
	.type = DISPC_DBS_FPS,
	.data = (void *)&dispc_ctx,
	.dispc_notifier = sprdfb_dispc_notify_change_fps,
};
#endif

#ifdef CONFIG_LCD_ESD_RECOVERY
extern void sprdfb_panel_ESD_reset(struct sprdfb_device *dev);
#endif

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
static int overlay_start(struct sprdfb_device *dev, uint32_t layer_index);
static int overlay_close(struct sprdfb_device *dev);
#endif

static int sprdfb_dispc_clk_disable(struct sprdfb_dispc_context *dispc_ctx_ptr,
				SPRDFB_DYNAMIC_CLK_SWITCH_E clock_switch_type);
static int sprdfb_dispc_clk_enable(struct sprdfb_dispc_context *dispc_ctx_ptr,
				SPRDFB_DYNAMIC_CLK_SWITCH_E clock_switch_type);
static int32_t sprdfb_dispc_init(struct sprdfb_device *dev);
static void dispc_reset(void);
static void dispc_stop(struct sprdfb_device *dev);
static void dispc_module_enable(void);
static void dispc_stop_for_feature(struct sprdfb_device *dev);
static void dispc_run_for_feature(struct sprdfb_device *dev);
static unsigned int sprdfb_dispc_change_threshold(struct devfreq_dbs *h,
							unsigned int state);
#ifdef CONFIG_FB_DISPC_DYNAMIC_PCLK_SUPPORT
static int32_t sprdfb_dispc_chg_pclk(struct sprdfb_device *fb_dev,
							u32 new_pclk);
#endif

static volatile Trick_Item s_trick_record[DISPC_INT_MAX] = {
	/*en interval begin dis_cnt en_cnt*/
	{0,  0,  0,  0,  0},/*DISPC_INT_DONE*/
	{0,  0,  0,  0,  0},/*DISPC_INT_TE*/
	{1, 300, 0,  0,  0},/*DISPC_INT_ERR, interval == 3s*/
	{0,  0,  0,  0,  0},/*DISPC_INT_EDPI_TE*/
	{0,  0,  0,  0,  0},/*DISPC_INT_UPDATE_DONE*/
};

/*
func:dispc_irq_trick
desc:if a xxx interruption come many times in a short time,
print the firt one, mask the follows.
a fixed-long time later, enable this interruption.
*/
static void dispc_irq_trick_in(uint32_t int_status)
{
	static uint32_t mask_irq_times;
	uint32_t i = 0;

	while (i < DISPC_INT_MAX) {
		if ((int_status & (1UL << i))
		&& s_trick_record[i].trick_en != 0) {
			if (s_trick_record[i].begin_jiffies == 0) {
				/*disable this interruption*/
				DISPC_INTERRUPT_SET(i, 0);
				s_trick_record[i].begin_jiffies = jiffies;
				s_trick_record[i].disable_cnt++;
				mask_irq_times++;
				pr_debug("%s[%d]:INT[%d] disable times:0x%08x\n",
					__func__, __LINE__, i,
					s_trick_record[i].disable_cnt);
			}
		}
		i++;
	}
	pr_debug("%s[%d]: total mask_irq_times:0x%08x \n",
					__func__, __LINE__, mask_irq_times);
}

/*
func:dispc_irq_trick
desc:if a xxx interruption come many times in a short time,
print the firt one, mask the follows.
a fixed-long time later, enable this interruption.
*/
static void dispc_irq_trick_out(void)
{
	static uint32_t open_irq_times;
	uint32_t i = 0;

	while (i < DISPC_INT_MAX) {
		if ((s_trick_record[i].trick_en != 0)
		&& (s_trick_record[i].begin_jiffies > 0)) {
			if ((s_trick_record[i].begin_jiffies + s_trick_record[i].interval) < jiffies) {
				/*re-enable this interruption*/
				DISPC_INTERRUPT_SET(i, 1);
				s_trick_record[i].begin_jiffies = 0;
				s_trick_record[i].enable_cnt++;
				open_irq_times++;
				pr_debug("%s[%d]:INT[%d] enable times:0x%08x\n",
					__func__, __LINE__, i,
					s_trick_record[i].enable_cnt);
			}
		}
		i++;
	}
	pr_debug(KERN_INFO "%s[%d]: total open_irq_times:0x%08x\n",
					__func__, __LINE__, open_irq_times);
}
/*static uint32_t underflow_ever_happened = 0;*/
static irqreturn_t dispc_isr(int irq, void *data)
{
	struct sprdfb_dispc_context *dispc_ctx =
					(struct sprdfb_dispc_context *) data;
	uint32_t reg_val;
	struct sprdfb_device *dev = dispc_ctx->dev;
	bool done = false;
#ifdef CONFIG_FB_VSYNC_SUPPORT
	bool vsync =  false;
#endif

	reg_val = dispc_read(DISPC_INT_STATUS);

	pr_debug("dispc_isr (0x%x)\n", reg_val);
	dispc_irq_trick_in(reg_val);

	if (reg_val & 0x04) {
		pr_warn("Warning: dispc underflow (0x%x)!\n",
								reg_val);
		/*underflow_ever_happened++;*/
		dispc_write(0x04, DISPC_INT_CLR);
		dispc_clear_bits(BIT(2), DISPC_INT_EN);
	}

	if (NULL == dev)
		return IRQ_HANDLED;

	if ((reg_val & 0x10) && (SPRDFB_PANEL_IF_DPI ==  dev->panel_if_type)) {
		/*dispc update done isr*/
#if 0
		if (dispc_ctx->is_first_frame) {
			/*dpi register update with SW and VSync*/
			dispc_clear_bits(BIT(4), DISPC_DPI_CTRL);

			/* start refresh */
			dispc_set_bits((1 << 4), DISPC_CTRL);

			dispc_ctx->is_first_frame = false;
		}
#endif
		dispc_write(0x10, DISPC_INT_CLR);
		done = true;
	} else if ((reg_val & 0x1) &&
				(SPRDFB_PANEL_IF_DPI !=  dev->panel_if_type)) {
			/* dispc done isr */
			dispc_write(1, DISPC_INT_CLR);
			dispc_ctx->is_first_frame = false;
			done = true;
	}

#ifdef CONFIG_FB_VSYNC_SUPPORT
	if ((reg_val & 0x1) && (SPRDFB_PANEL_IF_DPI ==	dev->panel_if_type)) {
		/*dispc done isr*/
		dispc_write(1, DISPC_INT_CLR);
		vsync = true;
	} else if ((reg_val & 0x2) &&
				(SPRDFB_PANEL_IF_EDPI == dev->panel_if_type)) {
		/*dispc te isr*/
		dispc_write(2, DISPC_INT_CLR);
		vsync = true;
	}
	if (vsync) {
		dispc_ctx->waitfor_vsync_done = 1;
		if (dispc_ctx->waitfor_vsync_waiter)
			wake_up_interruptible_all(&(dispc_ctx->waitfor_vsync_queue));
	}
#endif
	if (done) {
		dispc_ctx->vsync_done = 1;

#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if (SPRDFB_PANEL_IF_DPI !=  dev->panel_if_type) {
			sprdfb_dispc_clk_disable(dispc_ctx,
			SPRDFB_DYNAMIC_CLK_REFRESH);
		}
#endif
		if (dispc_ctx->vsync_waiter) {
			wake_up_interruptible_all(&(dispc_ctx->vsync_queue));
			dispc_ctx->vsync_waiter = 0;
		}
		sprdfb_panel_after_refresh(dev);
		pr_debug(KERN_INFO "sprdfb:[%s]:Done INT, reg_val = %d !\n",
							__func__, reg_val);
	}

	return IRQ_HANDLED;
}

/* dispc soft reset */
static void dispc_reset(void)
{
	sci_glb_set(REG_AHB_SOFT_RST, (BIT_DISPC_SOFT_RST));
	udelay(10);
	sci_glb_clr(REG_AHB_SOFT_RST, (BIT_DISPC_SOFT_RST));
}

static inline void dispc_set_bg_color(uint32_t bg_color)
{
	dispc_write(bg_color, DISPC_BG_COLOR);
}

static inline void dispc_set_osd_ck(uint32_t ck_color)
{
	dispc_write(ck_color, DISPC_OSD_CK);
}

static inline void dispc_osd_enable(bool is_enable)
{
	uint32_t reg_val;

	reg_val = dispc_read(DISPC_OSD_CTRL);

	if (is_enable)
		reg_val = reg_val | (BIT(0));
	else
		reg_val = reg_val & (~(BIT(0)));

	dispc_write(reg_val, DISPC_OSD_CTRL);
}


static void dispc_dithering_enable(bool enable)
{
	if (enable)
		dispc_set_bits(BIT(6), DISPC_CTRL);
	else
		dispc_clear_bits(BIT(6), DISPC_CTRL);
}

static void dispc_pwr_enable(bool enable)
{
	if (enable)
		dispc_set_bits(BIT(7), DISPC_CTRL);
	else
		dispc_clear_bits(BIT(7), DISPC_CTRL);
}

static void dispc_set_exp_mode(uint16_t exp_mode)
{
	uint32_t reg_val = dispc_read(DISPC_CTRL);

	reg_val &= ~(0x3 << 16);
	reg_val |= (exp_mode << 16);
	dispc_write(reg_val, DISPC_CTRL);
}

static void dispc_module_enable(void)
{
	/*dispc module enable */
	dispc_write((1<<0), DISPC_CTRL);

	/*disable dispc INT*/
	dispc_write(0x0, DISPC_INT_EN);

	/* clear dispc INT */
	dispc_write(0x1F, DISPC_INT_CLR);
}

static inline int32_t  dispc_set_disp_size(struct fb_var_screeninfo *var)
{
	uint32_t reg_val;

	reg_val = (var->xres & 0xfff) | ((var->yres & 0xfff) << 16);
	dispc_write(reg_val, DISPC_SIZE_XY);

	return 0;
}

static void dispc_layer_init(struct fb_var_screeninfo *var)
{
	uint32_t reg_val = 0;

	/*dispc_clear_bits((1<<0),DISPC_IMG_CTRL);*/
	dispc_write(0x0, DISPC_IMG_CTRL);
	dispc_clear_bits((1<<0), DISPC_OSD_CTRL);

	/******************* OSD layer setting ********************/

	/*enable OSD layer*/
	reg_val |= (1 << 0);

	/*disable  color key */

	/* alpha mode select  - block alpha*/
	reg_val |= (1 << 2);

	/* data format */
	if (var->bits_per_pixel == 32) {
		/* ABGR */
		reg_val |= (3 << 4);
		/* rb switch */
		reg_val |= (1 << 15);
	} else {
		/* RGB565 */
		reg_val |= (5 << 4);
		/* B2B3B0B1 */
		reg_val |= (2 << 8);
	}

	dispc_write(reg_val, DISPC_OSD_CTRL);

	/* OSD layer alpha value */
	dispc_write(0xff, DISPC_OSD_ALPHA);

	/* OSD layer size */
	reg_val = (var->xres & 0xfff) | ((var->yres & 0xfff) << 16);
	dispc_write(reg_val, DISPC_OSD_SIZE_XY);

	/* OSD layer start position */
	dispc_write(0, DISPC_OSD_DISP_XY);

	/* OSD layer pitch */
	reg_val = (var->xres & 0xfff);
	dispc_write(reg_val, DISPC_OSD_PITCH);

	/* OSD color_key value */
	dispc_set_osd_ck(0x0);

	/* DISPC workplane size */
	dispc_set_disp_size(var);
}

static void dispc_layer_update(struct fb_var_screeninfo *var)
{
	uint32_t reg_val = 0;

	/******************* OSD layer setting **********************/

	/*enable OSD layer*/
	reg_val |= (1 << 0);

	/*disable  color key */

	/* alpha mode select  - block alpha*/
	reg_val |= (1 << 2);

	/* data format */
	if (var->bits_per_pixel == 32) {
		/* ABGR */
		reg_val |= (3 << 4);
		/* rb switch */
		reg_val |= (1 << 15);
	} else {
		/* RGB565 */
		reg_val |= (5 << 4);
		/* B2B3B0B1 */
		reg_val |= (2 << 8);
	}

	dispc_write(reg_val, DISPC_OSD_CTRL);
}

static int32_t dispc_sync(struct sprdfb_device *dev)
{
	int ret;
	if (dev->enable == 0) {
		pr_err("sprdfb: dispc_sync fb suspeneded already!!\n");
		return -1;
	}

	ret = wait_event_interruptible_timeout(dispc_ctx.vsync_queue,
		dispc_ctx.vsync_done, msecs_to_jiffies(100));

	if (!ret) { /* time out */
		dispc_ctx.vsync_done = 1; /*error recovery */
		pr_err("sprdfb: dispc_sync time out!!!!!\n");

		return -1;
	}
	return 0;
}
static void dispc_run(struct sprdfb_device *dev)
{
	struct panel_spec *panel = dev->panel;
	uint32_t flags = 0;
	uint32_t switch_flags = 0;
	int i = 0;

	if (0 == dev->enable)
		return;

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
#ifdef SHARK_LAYER_COLOR_SWITCH_FEATURE
	/*bug212892
	SHARK_LAYER_COLOR_SWITCH_FEATURE is for layer/color switch in DISPC,
	we developed it because we found that
	some LCD panels displayed a black frame during playing video.
	Actually,
	this feature depends on LCD panel.
	It's possible that, after disabling this feature, there may be some error
	when playing vedio with some LCD panels and not in other LCD panels.
	If you want to disable this feature,
	we suggest you do following test.
	If the test result is OK, then disable it. Else, remain it.

	The test way:
	1.  play video
	2.  touch the screen to display progress bar
	3.  touch the screen not to display progress bar
	4.  repeat step 2 and 3 about 50 times, see if LCD will display a black frame.
	*/
		if (((dispc_read(DISPC_IMG_CTRL) & 0x1) !=
			(dispc_read(SHDW_IMG_CTRL) & 0x1))	/*layer switch*/
		|| ((dispc_read(DISPC_OSD_CTRL) & 0x1) !=
			(dispc_read(SHDW_OSD_CTRL) & 0x1))	/*layer switch*/
		|| ((dispc_read(DISPC_IMG_CTRL) & 0xf0) !=
			(dispc_read(SHDW_IMG_CTRL) & 0xf0))	/*color switch*/
		|| ((dispc_read(DISPC_OSD_CTRL) & 0xf0) !=
			(dispc_read(SHDW_OSD_CTRL) & 0xf0))) {	/*color switch*/

			local_irq_save(flags);
		/* stop dispc first , or the vactive will never be set to zero*/
			dispc_stop(dev);

			while (dispc_read(DISPC_DPI_STS1) & BIT(16)) {
				/*wait until frame send over*/
				if (0x0 == ++i%500000)
					pr_info("sprdfb:[%s] busy waiting stop!\n",
								__func__);
			}
			switch_flags = 1;
		}
#endif
		if (!dispc_ctx.is_first_frame) {
			dispc_ctx.vsync_done = 0;
			dispc_ctx.vsync_waiter++;
		}
		/*dpi register update*/
		dispc_set_bits(BIT(5), DISPC_DPI_CTRL);
		udelay(30);

		if (dispc_ctx.is_first_frame) {
			/*dpi register update with SW and VSync*/
			dispc_clear_bits(BIT(4), DISPC_DPI_CTRL);

			/* start refresh */
			dispc_set_bits((1 << 4), DISPC_CTRL);

			if (dev->panel->type == LCD_MODE_RGB) {
				/*Disable TE interrupt flag */
				dispc_clear_bits(BIT(1), DISPC_INT_EN);
				mdelay(30); /*For SPI Timing*/
				dev->panel->ops->panel_init(dev->panel);
			}
			/*dispc_ctx.is_first_frame = false;*/
		} else
			dispc_sync(dev);

#ifdef SHARK_LAYER_COLOR_SWITCH_FEATURE
		if (switch_flags == 1) {
			local_irq_restore(flags);
			pr_debug("srpdfb: [%s] color or layer swithed\n",
								__func__);
		}
#endif
	} else {
		dispc_ctx.vsync_done = 0;
		/* start refresh */
		dispc_set_bits((1 << 4), DISPC_CTRL);
	}

	if (dispc_ctx.is_first_frame) {
#ifdef CONFIG_FB_BL_EVENT_CTRL

		if (panel->ops->panel_set_brightness != NULL)
			panel->ops->panel_set_brightness(panel, true);
#endif
		dispc_ctx.is_first_frame = false;
	}

	dispc_irq_trick_out();
#ifndef CONFIG_FB_SCX15
	dsi_irq_trick(0, 0);
#endif
}

static void dispc_stop(struct sprdfb_device *dev)
{
	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		/*dpi register update with SW only*/
		dispc_set_bits(BIT(4), DISPC_DPI_CTRL);

		/* stop refresh */
		dispc_clear_bits((1 << 4), DISPC_CTRL);

		dispc_ctx.is_first_frame = true;
	} else
		dispc_ctx.is_first_frame = true;
}

static void dispc_update_clock(struct sprdfb_device *dev)
{
	uint32_t hpixels, vlines, need_clock,  dividor;
	int ret = 0;

	struct panel_spec *panel = dev->panel;
	struct info_mipi *mipi;
	struct info_rgb *rgb;

	pr_debug("sprdfb:[%s]\n", __func__);

	if ((NULL == panel) || (0 == panel->fps)) {
		pr_err("sprdfb: No panel->fps specified!\n");
		return;
	}

	mipi = panel->info.mipi;
	rgb = panel->info.rgb;

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		if (LCD_MODE_DSI == dev->panel->type) {
			hpixels = panel->width + mipi->timing->hsync +
					mipi->timing->hbp + mipi->timing->hfp;
			vlines = panel->height + mipi->timing->vsync +
					mipi->timing->vbp + mipi->timing->vfp;
		} else if (LCD_MODE_RGB == dev->panel->type) {
			hpixels = panel->width + rgb->timing->hsync +
					rgb->timing->hbp + rgb->timing->hfp;
			vlines = panel->height + rgb->timing->vsync +
					rgb->timing->vbp + rgb->timing->vfp;
		} else {
			pr_err("sprdfb:[%s] unexpected panel type!(%d)\n",
						__func__, dev->panel->type);
			return;
		}

		need_clock = hpixels * vlines * panel->fps;
		dividor  = SPRDFB_DPI_CLOCK_SRC / need_clock;
		if (SPRDFB_DPI_CLOCK_SRC - dividor * need_clock > (need_clock / 2))
			dividor += 1;

		if ((dividor < 1) || (dividor > 0x100)) {
			pr_err("sprdfb:[%s]: Invliad dividor(%d)!Not update dpi clock!\n",
							__func__, dividor);
			return;
		}

		dev->dpi_clock = SPRDFB_DPI_CLOCK_SRC / dividor;

		ret = clk_set_rate(dispc_ctx.clk_dispc_dpi, dev->dpi_clock);

		if (ret)
			pr_err("sprdfb:[%s]:fail\n", __func__);

		pr_info("sprdfb:[%s] need_clock=%d,dividor=%d,dpi_clock=%d\n",
			__func__, need_clock, dividor, dev->dpi_clock);
	}

}

static int32_t sprdfb_dispc_uninit(struct sprdfb_device *dev)
{
	pr_debug(KERN_INFO "sprdfb:[%s]\n", __func__);

	dev->enable = 0;
	sprdfb_dispc_clk_disable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_FORCE);

	return 0;
}

static int32_t dispc_clk_init(struct sprdfb_device *dev)
{
	int ret = 0;
	struct clk *clk_parent1, *clk_parent2, *clk_parent3, *clk_parent4;

	pr_debug(KERN_INFO "sprdfb:[%s]\n", __func__);

	sci_glb_set(DISPC_CORE_EN, BIT_DISPC_CORE_EN);
	/*sci_glb_set(DISPC_EMC_EN, BIT_DISPC_EMC_EN);*/

	clk_parent1 = clk_get(NULL, DISPC_CLOCK_PARENT);
	if (IS_ERR(clk_parent1)) {
		pr_warn("sprdfb: get clk_parent1 fail!\n");
		return -1;
	} else
		pr_debug(KERN_INFO "sprdfb: get clk_parent1 ok!\n");

	clk_parent2 = clk_get(NULL, DISPC_DBI_CLOCK_PARENT);
	if (IS_ERR(clk_parent2)) {
		pr_warn("sprdfb: get clk_parent2 fail!\n");
		return -1;
	} else
		pr_debug(KERN_INFO "sprdfb: get clk_parent2 ok!\n");

	clk_parent3 = clk_get(NULL, DISPC_DPI_CLOCK_PARENT);
	if (IS_ERR(clk_parent3)) {
		pr_warn("sprdfb: get clk_parent3 fail!\n");
		return -1;
	} else
		pr_debug(KERN_INFO "sprdfb: get clk_parent3 ok!\n");

	clk_parent4 = clk_get(NULL, DISPC_EMC_EN_PARENT);
	if (IS_ERR(clk_parent3)) {
		pr_warn("sprdfb: get clk_parent4 fail!\n");
		return -1;
	} else
		pr_debug(KERN_INFO "sprdfb: get clk_parent4 ok!\n");

	dispc_ctx.clk_dispc = clk_get(NULL, DISPC_PLL_CLK);
	if (IS_ERR(dispc_ctx.clk_dispc)) {
		pr_warn("sprdfb: get clk_dispc fail!\n");
		return -1;
	} else
		pr_debug(KERN_INFO "sprdfb: get clk_dispc ok!\n");

	dispc_ctx.clk_dispc_dbi = clk_get(NULL, DISPC_DBI_CLK);
	if (IS_ERR(dispc_ctx.clk_dispc_dbi)) {
		pr_warn("sprdfb: get clk_dispc_dbi fail!\n");
		return -1;
	} else
		pr_debug(KERN_INFO "sprdfb: get clk_dispc_dbi ok!\n");

	dispc_ctx.clk_dispc_dpi = clk_get(NULL, DISPC_DPI_CLK);
	if (IS_ERR(dispc_ctx.clk_dispc_dpi)) {
		pr_warn("sprdfb: get clk_dispc_dpi fail!\n");
		return -1;
	} else
		pr_debug(KERN_INFO "sprdfb: get clk_dispc_dpi ok!\n");

	dispc_ctx.clk_dispc_emc = clk_get(NULL, DISPC_EMC_CLK);
	if (IS_ERR(dispc_ctx.clk_dispc_emc)) {
		pr_warn("sprdfb: get clk_dispc_dpi fail!\n");
		return -1;
	} else
		pr_debug(KERN_INFO "sprdfb: get clk_dispc_emc ok!\n");

	ret = clk_set_parent(dispc_ctx.clk_dispc, clk_parent1);
	if (ret)
		pr_err("sprdfb: dispc set clk parent fail\n");

	ret = clk_set_rate(dispc_ctx.clk_dispc, DISPC_CLOCK);
	if (ret)
		pr_err("sprdfb: dispc set clk parent fail\n");

	ret = clk_set_parent(dispc_ctx.clk_dispc_dbi, clk_parent2);
	if (ret)
		pr_err("sprdfb: dispc set dbi clk parent fail\n");

	ret = clk_set_rate(dispc_ctx.clk_dispc_dbi, DISPC_DBI_CLOCK);
	if (ret)
		pr_err("sprdfb: dispc set dbi clk parent fail\n");

	ret = clk_set_parent(dispc_ctx.clk_dispc_dpi, clk_parent3);
	if (ret)
		pr_err("sprdfb: dispc set dpi clk parent fail\n");

	ret = clk_set_parent(dispc_ctx.clk_dispc_emc, clk_parent4);
	if (ret)
		pr_err("sprdfb: dispc set emc clk parent fail\n");

#ifdef CONFIG_FB_DISPC_DYNAMIC_PCLK_SUPPORT
	if (dev->dyna_dpi_clk) {
		ret = sprdfb_dispc_chg_pclk(dev, dev->dyna_dpi_clk);
		if (ret)  {
			pr_err("%s: failed to change pclk. ret=%d\n",
							__func__, ret);
			return -EIO;
		}
	} else
#endif
	if ((dev->panel != NULL) && (0 != dev->panel->fps)) {
		dispc_update_clock(dev);
	} else {
		dev->dpi_clock = DISPC_DPI_CLOCK;
		ret = clk_set_rate(dispc_ctx.clk_dispc_dpi, DISPC_DPI_CLOCK);
		if (ret)
			pr_err("sprdfb:[%s] fail\n", __func__);
	}

	ret = clk_enable(dispc_ctx.clk_dispc_emc);
	if (ret) {
		pr_err("sprdfb:enable clk_dispc_emc error!!!\n");
		ret = -1;
	}

	ret = sprdfb_dispc_clk_enable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_FORCE);
	if (ret) {
		pr_warn("sprdfb:[%s] enable dispc_clk fail!\n",
								__func__);
		return -1;
	} else {
		pr_debug(KERN_INFO "sprdfb:[%s] enable dispc_clk ok!\n",
								__func__);
	}

	dispc_print_clk();

	return 0;
}

#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || \
					defined(CONFIG_SPRD_SCX35_DMC_FREQ))
struct devfreq_dbs sprd_fb_notify = {
	.level = 0,
	.data = &dispc_ctx,
	.devfreq_notifier = sprdfb_dispc_change_threshold,
};
#endif

static int scxx30_dispc_cpuidle_notify(struct notifier_block *nb,
					unsigned long event, void *dummy)
{
	if (event == SC_CPUIDLE_PREPARE) {
		 if (0 == dispc_ctx.vsync_done
#ifdef CONFIG_FB_VSYNC_SUPPORT
		|| 0 == dispc_ctx.waitfor_vsync_done
#endif
		)
			return NOTIFY_BAD;	/*work now, can't not sleep*/
		else
			return NOTIFY_OK;
	} else
		pr_info("sprdfb: error in cpuidle notify type!\n");

	return NOTIFY_BAD;
}

static struct notifier_block scxx30_dispc_cpuidle_notifier = {
	.notifier_call = scxx30_dispc_cpuidle_notify,
};




static int32_t sprdfb_dispc_module_init(struct sprdfb_device *dev)
{
	int ret = 0;

	if (dispc_ctx.is_inited) {
		pr_warn("sprdfb:[%s]initialized!\n", __func__);
		return 0;
	} else
		pr_info("sprdfb:[%s].call only once!\n", __func__);

	dispc_ctx.vsync_done = 1;
	dispc_ctx.vsync_waiter = 0;
	init_waitqueue_head(&(dispc_ctx.vsync_queue));

#ifdef CONFIG_FB_VSYNC_SUPPORT
	dispc_ctx.waitfor_vsync_done = 1;
	dispc_ctx.waitfor_vsync_waiter = 0;
	init_waitqueue_head(&(dispc_ctx.waitfor_vsync_queue));
#endif
	sema_init(&dev->refresh_lock, 1);

	ret = request_irq(IRQ_DISPC_INT, dispc_isr, IRQF_DISABLED, "DISPC",
								&dispc_ctx);
	if (ret) {
		pr_err("sprdfb: dispcfailed to request irq!\n");
		sprdfb_dispc_uninit(dev);
		return -1;
	}

	dispc_ctx.is_inited = true;

#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) ||\
					defined(CONFIG_SPRD_SCX35_DMC_FREQ))
	devfreq_notifier_register(&sprd_fb_notify);
#endif

	ret = register_sc_cpuidle_notifier(&scxx30_dispc_cpuidle_notifier);
	if (ret)
		pr_info("sprdfb: Failed to setup light sleep notifier!\n");

	return 0;

}
#if defined(CONFIG_FB_LCD_ILI9486_RGB_SPI)
void temporary_reset(struct sprdfb_device *dev)
{
	int ret = 0;

	dispc_write(1, DISPC_RSTN);
	usleep_range(5000, 10000);
	dispc_write(0, DISPC_RSTN);
	usleep_range(5000, 10000);
	dispc_write(1, DISPC_RSTN);
	usleep_range(5000, 10000);
	/*msleep(120);*/

	ret = dispc_clk_init(dev);
	if (ret)
		pr_warn("sprdfb: dispc_clk_init fail!\n");
}
#endif
static int32_t sprdfb_dispc_early_init(struct sprdfb_device *dev)
{
	int ret = 0;

	if (!dispc_ctx.is_inited)
		spin_lock_init(&dispc_ctx.clk_spinlock);

#if defined(CONFIG_FB_LCD_ILI9486_RGB_SPI)
	if (!dispc_ctx.is_inited) {
		ret = dispc_clk_init(dev);

		if (ret) {
			pr_warn("sprdfb: dispc_clk_init fail!\n");
			return -1;
		}
	}
#else
	ret = dispc_clk_init(dev);
	if (ret) {
		pr_warn("sprdfb: dispc_clk_init fail!\n");
		return -1;
	}
#endif

	if (!dispc_ctx.is_inited) {
		if (dev->panel_ready) {
			/*panel ready*/
			pr_info("sprdfb:[%s]: alread initialized\n",
								__func__);
			dispc_ctx.is_first_frame = false;
		} else {
			/*panel not ready*/
			pr_info("sprdfb:[%s]: not initialized\n",
								__func__);
			dispc_reset();
			dispc_module_enable();
			dispc_ctx.is_first_frame = true;
		}
			dispc_update_clock(dev);
			ret = sprdfb_dispc_module_init(dev);
	} else {
		/*resume*/
		pr_info("sprdfb:[%s]: resume\n", __func__);

#if defined(CONFIG_FB_LCD_ILI9486_RGB_SPI)
		temporary_reset(dev);
#endif
		dispc_reset();
		dispc_module_enable();
		dispc_ctx.is_first_frame = true;
	}
#ifdef CONFIG_FB_DYNAMIC_FPS_SUPPORT
	dispc_notifier_register(&sprd_fps_notify);
#endif


	return ret;
}

static int sprdfb_dispc_clk_disable(struct sprdfb_dispc_context *dispc_ctx_ptr,
				SPRDFB_DYNAMIC_CLK_SWITCH_E clock_switch_type)
{
	bool is_need_disable = false;
	unsigned long irqflags;

	pr_debug(KERN_INFO "sprdfb:[%s]\n", __func__);
	if (!dispc_ctx_ptr)
		return 0;

	spin_lock_irqsave(&dispc_ctx.clk_spinlock, irqflags);
	switch (clock_switch_type) {
	case SPRDFB_DYNAMIC_CLK_FORCE:
		is_need_disable = true;
		break;
	case SPRDFB_DYNAMIC_CLK_REFRESH:
		dispc_ctx_ptr->clk_is_refreshing = false;
		if (dispc_ctx_ptr->clk_open_count <= 0)
			is_need_disable = true;
		break;
	case SPRDFB_DYNAMIC_CLK_COUNT:
		if (dispc_ctx_ptr->clk_open_count > 0) {
			dispc_ctx_ptr->clk_open_count--;
			if (dispc_ctx_ptr->clk_open_count == 0) {
				if (!dispc_ctx_ptr->clk_is_refreshing)
					is_need_disable = true;
			}
		}
		break;
	default:
		break;
	}

	if (dispc_ctx_ptr->clk_is_open && is_need_disable) {
		pr_debug(KERN_INFO "sprdfb:sprdfb_dispc_clk_disable real\n");
		clk_disable(dispc_ctx_ptr->clk_dispc);
		clk_disable(dispc_ctx_ptr->clk_dispc_dpi);
		clk_disable(dispc_ctx_ptr->clk_dispc_dbi);
		dispc_ctx_ptr->clk_is_open = false;
		dispc_ctx_ptr->clk_is_refreshing = false;
		dispc_ctx_ptr->clk_open_count = 0;
	}

	spin_unlock_irqrestore(&dispc_ctx.clk_spinlock, irqflags);

	pr_debug(KERN_INFO "sprdfb:[%s] type=%d refresh=%d,count=%d\n",
					__func__, clock_switch_type,
					dispc_ctx_ptr->clk_is_refreshing,
					dispc_ctx_ptr->clk_open_count);
	return 0;
}

static int sprdfb_dispc_clk_enable(struct sprdfb_dispc_context *dispc_ctx_ptr,
				SPRDFB_DYNAMIC_CLK_SWITCH_E clock_switch_type)
{
	int ret = 0;
	bool is_dispc_enable = false;
	bool is_dispc_dpi_enable = false;
	unsigned long irqflags;

	pr_debug(KERN_INFO "sprdfb:[%s]\n", __func__);
	if (!dispc_ctx_ptr)
		return -1;

	spin_lock_irqsave(&dispc_ctx.clk_spinlock, irqflags);

	if (!dispc_ctx_ptr->clk_is_open) {
		pr_debug(KERN_INFO "sprdfb:sprdfb_dispc_clk_enable real\n");
		ret = clk_enable(dispc_ctx_ptr->clk_dispc);
		if (ret) {
			pr_err("sprdfb:enable clk_dispc error!!!\n");
			ret = -1;
			goto ERROR_CLK_ENABLE;
		}
		is_dispc_enable = true;
		ret = clk_enable(dispc_ctx_ptr->clk_dispc_dpi);
		if (ret) {
			pr_err("sprdfb:enable clk_dispc_dpi error!!!\n");
			ret = -1;
			goto ERROR_CLK_ENABLE;
		}
		is_dispc_dpi_enable = true;
		ret = clk_enable(dispc_ctx_ptr->clk_dispc_dbi);
		if (ret) {
			pr_err("sprdfb:enable clk_dispc_dbi error!!!\n");
			ret = -1;
			goto ERROR_CLK_ENABLE;
		}
		dispc_ctx_ptr->clk_is_open = true;
	}

	switch (clock_switch_type) {
	case SPRDFB_DYNAMIC_CLK_FORCE:
		break;
	case SPRDFB_DYNAMIC_CLK_REFRESH:
		dispc_ctx_ptr->clk_is_refreshing = true;
		break;
	case SPRDFB_DYNAMIC_CLK_COUNT:
		dispc_ctx_ptr->clk_open_count++;
		break;
	default:
		break;
	}

	spin_unlock_irqrestore(&dispc_ctx.clk_spinlock, irqflags);

	pr_debug(KERN_INFO "sprdfb:[%s] type=%d refresh=%d,count=%d,ret=%d\n",
					__func__, clock_switch_type,
					dispc_ctx_ptr->clk_is_refreshing,
					dispc_ctx_ptr->clk_open_count, ret);
	return ret;

ERROR_CLK_ENABLE:
	if (is_dispc_enable)
		clk_disable(dispc_ctx_ptr->clk_dispc);

	if (is_dispc_dpi_enable)
		clk_disable(dispc_ctx_ptr->clk_dispc_dpi);

	spin_unlock_irqrestore(&dispc_ctx.clk_spinlock, irqflags);

	pr_err("sprdfb:sprdfb_dispc_clk_enable error!!!!!!\n");
	return ret;
}

static int32_t sprdfb_dispc_init(struct sprdfb_device *dev)
{
	pr_debug(KERN_INFO "sprdfb:[%s]\n", __func__);

	/*set bg color*/
	dispc_set_bg_color(0xFFFFFFFF);
	/*enable dithering*/
	dispc_dithering_enable(true);
	/*use MSBs as img exp mode*/
	dispc_set_exp_mode(0x0);
	/*enable DISPC Power Control*/
	dispc_pwr_enable(true);

	if (dispc_ctx.is_first_frame)
		dispc_layer_init(&(dev->fb->var));
	else
		dispc_layer_update(&(dev->fb->var));

	/*dispc_update_clock(dev);*/

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		if (dispc_ctx.is_first_frame) {
			/*set dpi register update only with SW*/
			dispc_set_bits(BIT(4), DISPC_DPI_CTRL);
		} else	/*set dpi register update with SW & VSYNC*/
			dispc_clear_bits(BIT(4), DISPC_DPI_CTRL);

		/*enable dispc update done INT*/
		dispc_write((1<<4), DISPC_INT_EN);
		/*Disable TE interrupt flag */
		dispc_clear_bits(BIT(1), DISPC_INT_EN);
	} else	/* enable dispc DONE  INT*/
		dispc_write((1<<0), DISPC_INT_EN);

	dispc_set_bits(BIT(2), DISPC_INT_EN);
	/*dev->enable = 1;*/
	return 0;
}


static void sprdfb_dispc_clean_lcd(struct sprdfb_device *dev)
{
	struct fb_info *fb = NULL;
	uint32_t size = 0;

	pr_debug(KERN_INFO "sprdfb:[%s]\n", __func__);

	down(&dev->refresh_lock);
	if (!dispc_ctx.is_first_frame) {
		pr_info("sprdfb:[%s] not first_frame\n", __func__);
		up(&dev->refresh_lock);
		return;
	}

	fb = dev->fb;
	size = (fb->var.xres & 0xffff) | ((fb->var.yres) << 16);

	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
		sprdfb_panel_invalidate(dev->panel);

	pr_info("sprdfb:[%s] clean lcd!\n", __func__);

	dispc_write(size, DISPC_SIZE_XY);

	dispc_osd_enable(false);
	dispc_set_bg_color(0x00);
	dispc_run(dev);
	/*dispc_osd_enable(true);*/
	up(&dev->refresh_lock);
	mdelay(30);
}

/* Requires: lhs > rhs */
static inline u32 timespec_ms_diff(struct timespec lhs, struct timespec rhs)
{
	struct timespec tmp_ts = timespec_sub(lhs, rhs);
	u64 tmp_ns = (u64)timespec_to_ns(&tmp_ts);

	do_div(tmp_ns, NSEC_PER_MSEC);
	return (u32)tmp_ns;
}

/* Returns "frames per 1000 secs", divide by 1000 to get fps with 3 decimals */
static u32 update_fps(struct fps_info *fps)
{
	struct timespec now;
	u32 fpks = 0, ms_since_last, num_frames;

	getrawmonotonic(&now);
	fps->frame_counter++;

	ms_since_last = timespec_ms_diff(now, fps->timestamp_last);
	num_frames = fps->frame_counter - fps->frame_counter_last;

	if (num_frames > 1 && ms_since_last >= fps->interval_ms) {

		fpks = (num_frames * 1000000) / ms_since_last;
		fps->timestamp_last = now;
		fps->frame_counter_last = fps->frame_counter;
		fps->fpks = fpks;
	}

	return fpks;
}

static void update_overlay_fps(struct sprdfb_device *dev)
{
	u32 fpks = update_fps(&dev->fps);

	if (fpks && dev->fps.enable_dmesg)
		pr_info("sprdfb: Overlay FPS: fps=%d.%.3d\n",
						fpks / 1000, fpks % 1000);
}

static void update_pan_fps(struct sprdfb_device *dev)
{
	u32 fpks = update_fps(&dev->fps);

	if (fpks && dev->fps.enable_dmesg)
		pr_info("sprdfb: Pan FPS: fps=%d.%.3d\n",
						fpks / 1000, fpks % 1000);
}

static void runtime_fps_check(struct sprdfb_device *dev)
{

	switch (dev->fps.fps_checking_mode) {

	case SPRD_DISPLAY_UPDATE_PAN:
		update_pan_fps(dev);
		break;
	case SPRD_DISPLAY_UPDATE_OVERLAY:
		update_overlay_fps(dev);
		break;
	default:
		break;
	}
	dev->fps.fps_checking_mode = SPRD_DISPLAY_UPDATE_NONE;
}

static int32_t sprdfb_dispc_refresh(struct sprdfb_device *dev)
{
	uint32_t reg_val = 0;
	struct fb_info *fb = dev->fb;

	uint32_t base = fb->fix.smem_start + fb->fix.line_length
						* fb->var.yoffset;

	pr_debug(KERN_INFO "sprdfb:[%s]\n", __func__);

	down(&dev->refresh_lock);
	if (0 == dev->enable) {
		pr_err("sprdfb:[%s]: do not refresh in suspend!!!\n",
								__func__);
		goto ERROR_REFRESH;
	}
#ifdef CONFIG_FB_MMAP_CACHED
	if (NULL != dispc_ctx.vma) {
		pr_debug("[%s] dmac_flush_range dispc_ctx.vma=0x%x\n ",
						__func__, dispc_ctx.vma);
		dmac_flush_range(dispc_ctx.vma->vm_start,
							dispc_ctx.vma->vm_end);
	}
	dispc_dithering_enable(false);
#endif
	dispc_osd_enable(true);

	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
		dispc_ctx.vsync_waiter++;
		dispc_sync(dev);
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if (sprdfb_dispc_clk_enable(&dispc_ctx,
						SPRDFB_DYNAMIC_CLK_REFRESH)) {

			pr_warn("sprdfb: [%s] fail in refresh!\n", __func__);
			goto ERROR_REFRESH;
		}
#endif
	}

	pr_debug(KERN_INFO "srpdfb: [%s] got sync\n", __func__);

	dispc_ctx.dev = dev;
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	if (SPRD_OVERLAY_STATUS_STARTED == dispc_ctx.overlay_state)
		overlay_close(dev);
#endif

#ifdef LCD_UPDATE_PARTLY
	if ((fb->var.reserved[0] == 0x6f766572) &&
				(SPRDFB_PANEL_IF_DPI != dev->panel_if_type)) {

		uint32_t x, y, width, height;

		x = fb->var.reserved[1] & 0xffff;
		y = fb->var.reserved[1] >> 16;
		width  = fb->var.reserved[2] &  0xffff;
		height = fb->var.reserved[2] >> 16;

		base += ((x + y * fb->var.xres) * fb->var.bits_per_pixel / 8);
		dispc_write(base, DISPC_OSD_BASE_ADDR);
		dispc_write(0, DISPC_OSD_DISP_XY);
		dispc_write(fb->var.reserved[2], DISPC_OSD_SIZE_XY);
		dispc_write(fb->var.xres, DISPC_OSD_PITCH);

		dispc_write(fb->var.reserved[2], DISPC_SIZE_XY);

		sprdfb_panel_invalidate_rect(dev->panel,
					x, y, x+width-1, y+height-1);
	} else
#endif
	{
		uint32_t size = (fb->var.xres & 0xffff) |
							((fb->var.yres) << 16);
		dispc_write(base, DISPC_OSD_BASE_ADDR);
		dispc_write(0, DISPC_OSD_DISP_XY);
		dispc_write(size, DISPC_OSD_SIZE_XY);
		dispc_write(fb->var.xres, DISPC_OSD_PITCH);
#ifdef CONFIG_FB_LOW_RES_SIMU
		size = (dev->panel->width & 0xffff) |
						((dev->panel->height)<<16);
#endif
		dispc_write(size, DISPC_SIZE_XY);


#ifdef BIT_PER_PIXEL_SURPPORT
		/* data format */
		if (fb->var.bits_per_pixel == 32) {
			/* ABGR */
			reg_val |= (3 << 4);
			/* rb switch */
			reg_val |= (1 << 15);
			dispc_clear_bits(0x30000, DISPC_CTRL);
		} else {
			/* RGB565 */
			reg_val |= (5 << 4);
			/* B2B3B0B1 */
			reg_val |= (2 << 8);

			dispc_clear_bits(0x30000, DISPC_CTRL);
			dispc_set_bits(0x10000, DISPC_CTRL);
		}
		reg_val |= (1 << 0);

		/* alpha mode select  - block alpha*/
		reg_val |= (1 << 2);

		dispc_write(reg_val, DISPC_OSD_CTRL);
#endif
		if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
			sprdfb_panel_invalidate(dev->panel);
	}

	sprdfb_panel_before_refresh(dev);

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	dispc_set_bits(BIT(0), DISPC_OSD_CTRL);
	if (SPRD_OVERLAY_STATUS_ON == dispc_ctx.overlay_state)
		overlay_start(dev, (SPRD_LAYER_IMG));
#endif
	dispc_run(dev);

	dev->fps.fps_checking_mode = SPRD_DISPLAY_UPDATE_PAN;
	runtime_fps_check(dev);

ERROR_REFRESH:
	up(&dev->refresh_lock);

	if (0 != dev->logo_buffer_addr_v) {
		pr_info("sprdfb: free logo proc buffer!\n");
		free_pages(dev->logo_buffer_addr_v,
		get_order(dev->logo_buffer_size));
		dev->logo_buffer_addr_v = 0;
	}

	pr_debug("DISPC_CTRL: 0x%x\n", dispc_read(DISPC_CTRL));
	pr_debug("DISPC_SIZE_XY: 0x%x\n", dispc_read(DISPC_SIZE_XY));

	pr_debug("DISPC_BG_COLOR: 0x%x\n", dispc_read(DISPC_BG_COLOR));

	pr_debug("DISPC_INT_EN: 0x%x\n", dispc_read(DISPC_INT_EN));

	pr_debug("DISPC_OSD_CTRL: 0x%x\n", dispc_read(DISPC_OSD_CTRL));
	pr_debug("DISPC_OSD_BASE_ADDR: 0x%x\n",
	dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("DISPC_OSD_SIZE_XY: 0x%x\n", dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("DISPC_OSD_PITCH: 0x%x\n", dispc_read(DISPC_OSD_PITCH));
	pr_debug("DISPC_OSD_DISP_XY: 0x%x\n", dispc_read(DISPC_OSD_DISP_XY));
	pr_debug("DISPC_OSD_ALPHA: 0x%x\n", dispc_read(DISPC_OSD_ALPHA));
	return 0;
}

static int32_t sprdfb_dispc_suspend(struct sprdfb_device *dev)
{
	pr_info("[%s], dev->enable = %d\n", __func__, dev->enable);

	if (0 != dev->enable) {
		down(&dev->refresh_lock);

		if (dev->panel->ops->panel_set_brightness != NULL)
			dev->panel->ops->panel_set_brightness(dev->panel,
									false);

		if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
			/* must wait ,dispc_sync() */
			dispc_ctx.vsync_waiter++;
			dispc_sync(dev);
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
			pr_info("sprdfb: open clk in suspend\n");
			if (sprdfb_dispc_clk_enable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_COUNT)) {
				pr_warn("[%s] clk enable fail!\n", __func__);
#endif
			pr_info("[%s] got sync\n", __func__);
		}

		dev->enable = 0;
		up(&dev->refresh_lock);

		sprdfb_panel_suspend(dev);

		dispc_stop(dev);

		if (dev->panel->ops->panel_pwr_ctrl != NULL)
			dev->panel->ops->panel_pwr_ctrl(dev->panel, false);

		/*fps>..margin 2frame 16.7msec * 2*/
		msleep(34);

		clk_disable(dispc_ctx.clk_dispc_emc);
		sprdfb_dispc_clk_disable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_FORCE);
	} else {
		pr_err("sprdfb: [%s]: Invalid device status %d\n",
						__func__, dev->enable);
	}
	return 0;
}

static int32_t sprdfb_dispc_resume(struct sprdfb_device *dev)
{
	pr_info("[%s], dev->enable= %d\n", __func__, dev->enable);

	if (dev->enable == 0) {

		if (dev->panel->ops->panel_pwr_ctrl != NULL)
			dev->panel->ops->panel_pwr_ctrl(dev->panel, true);

		if (sprdfb_dispc_clk_enable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_FORCE))
			pr_warn("[%s] clk enable fail!\n", __func__);

		/*sci_glb_set(DISPC_EMC_EN, BIT_DISPC_EMC_EN);*/

		dispc_ctx.vsync_done = 1;
		if (1/*(dispc_read(DISPC_SIZE_XY) == 0 ) */) {
			/* resume from deep sleep */
			pr_info("[%s] from deep sleep\n", __func__);
			sprdfb_dispc_early_init(dev);
			sprdfb_dispc_init(dev);
			sprdfb_panel_resume(dev, true);
		} else {
			pr_info("[%s] not from deep sleep\n", __func__);

			sprdfb_panel_resume(dev, true);
		}

		dev->enable = 1;
		if (dev->panel->is_clean_lcd)
			sprdfb_dispc_clean_lcd(dev);

	}
	pr_info("[%s], leave dev->enable= %d\n", __func__, dev->enable);

	return 0;
}

#ifdef CONFIG_LCD_ESD_RECOVERY
static void sprdfb_dispc_esd_reset(struct sprdfb_device *dev)
{
	sprdfb_panel_ESD_reset(dev);
	sprdfb_dispc_refresh(dev);
}
#endif

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
static int overlay_open(void)
{
	pr_debug("[%s] : %d\n", __func__, dispc_ctx.overlay_state);

/*
	if (SPRD_OVERLAY_STATUS_OFF  != dispc_ctx.overlay_state) {
		pr_err("sprdfb: Overlay open fail (has been opened)");
		return -1;
	}
*/

	dispc_ctx.overlay_state = SPRD_OVERLAY_STATUS_ON;
	return 0;
}

static int overlay_start(struct sprdfb_device *dev, uint32_t layer_index)
{
	pr_debug("[%s] : %d, %d\n", __func__, dispc_ctx.overlay_state,
								layer_index);


	if (SPRD_OVERLAY_STATUS_ON  != dispc_ctx.overlay_state) {
		pr_err("sprdfb: overlay start fail. (not opened)");
		return -1;
	}

	if ((0 == dispc_read(DISPC_IMG_Y_BASE_ADDR)) &&
				(0 == dispc_read(DISPC_OSD_BASE_ADDR))) {

		pr_err("sprdfb: overlay start fail. (not configged)");
		return -1;
	}
/*
	if (0 != dispc_sync(dev)) {
		pr_err("sprdfb: overlay start fail. (wait done fail)");
		return -1;
	}
*/
	dispc_set_bg_color(0x0);
	dispc_clear_bits(BIT(2), DISPC_OSD_CTRL); /*use pixel alpha*/
	dispc_write(0x80, DISPC_OSD_ALPHA);

	if ((layer_index & SPRD_LAYER_IMG) &&
				(0 != dispc_read(DISPC_IMG_Y_BASE_ADDR)))
		dispc_set_bits(BIT(0), DISPC_IMG_CTRL);/*enable the image layer*/

	if ((layer_index & SPRD_LAYER_OSD) &&
				(0 != dispc_read(DISPC_OSD_BASE_ADDR)))
		dispc_set_bits(BIT(0), DISPC_OSD_CTRL);/*enable the osd layer*/

	dispc_ctx.overlay_state = SPRD_OVERLAY_STATUS_STARTED;
	return 0;
}

static int overlay_img_configure(struct sprdfb_device *dev, int type,
		overlay_rect *rect, unsigned char *buffer, int y_endian,
		int uv_endian, bool rb_switch)
{
	uint32_t reg_value;

	pr_debug("sprdfb: [%s] : %d, (%d, %d,%d,%d), 0x%x\n", __func__,
				type, rect->x, rect->y, rect->h, rect->w,
				(unsigned int)buffer);


	if (SPRD_OVERLAY_STATUS_ON  != dispc_ctx.overlay_state) {
		pr_err("sprdfb: Overlay config fail (not opened)");
		return -1;
	}

	if (type >= SPRD_DATA_TYPE_LIMIT) {
		pr_err("sprdfb: Overlay config fail (type error)");
		return -1;
	}

	if ((y_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT) ||
				(uv_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT)) {

		pr_err("sprdfb:[%s] fail (y, uv endian error)",
								__func__);
		return -1;
	}

/*
	lcdc_write(((type << 3) | (1 << 0)), LCDC_IMG_CTRL);
	lcdc_write((type << 3) , LCDC_IMG_CTRL);
*/
	reg_value = (y_endian << 8) | (uv_endian << 10) | (type << 4);
	if (rb_switch)
		reg_value |= (1 << 15);

	dispc_write(reg_value, DISPC_IMG_CTRL);

	dispc_write((uint32_t)buffer, DISPC_IMG_Y_BASE_ADDR);
	if (type < SPRD_DATA_TYPE_RGB888) {
		uint32_t size = rect->w * rect->h;
		dispc_write((uint32_t)(buffer + size), DISPC_IMG_UV_BASE_ADDR);
	}

	reg_value = (rect->h << 16) | (rect->w);
	dispc_write(reg_value, DISPC_IMG_SIZE_XY);

	dispc_write(rect->w, DISPC_IMG_PITCH);

	reg_value = (rect->y << 16) | (rect->x);
	dispc_write(reg_value, DISPC_IMG_DISP_XY);

	if (type < SPRD_DATA_TYPE_RGB888) {
		dispc_write(1, DISPC_Y2R_CTRL);
#ifdef CONFIG_FB_SCX15
		dispc_write(SPRDFB_BRIGHTNESS|SPRDFB_CONTRAST,
							DISPC_Y2R_Y_PARAM);
		dispc_write(SPRDFB_OFFSET_U|SPRDFB_SATURATION_U,
							DISPC_Y2R_U_PARAM);
		dispc_write(SPRDFB_OFFSET_V|SPRDFB_SATURATION_V,
							DISPC_Y2R_V_PARAM);
#else
		dispc_write(SPRDFB_CONTRAST, DISPC_Y2R_CONTRAST);
		dispc_write(SPRDFB_SATURATION, DISPC_Y2R_SATURATION);
		dispc_write(SPRDFB_BRIGHTNESS, DISPC_Y2R_BRIGHTNESS);
#endif
	}

	pr_debug("DISPC_IMG_CTRL: 0x%x\n", dispc_read(DISPC_IMG_CTRL));
	pr_debug("DISPC_IMG_Y_BASE_ADDR: 0x%x\n",
					dispc_read(DISPC_IMG_Y_BASE_ADDR));
	pr_debug("DISPC_IMG_UV_BASE_ADDR: 0x%x\n",
					dispc_read(DISPC_IMG_UV_BASE_ADDR));
	pr_debug("DISPC_IMG_SIZE_XY: 0x%x\n", dispc_read(DISPC_IMG_SIZE_XY));
	pr_debug("DISPC_IMG_PITCH: 0x%x\n", dispc_read(DISPC_IMG_PITCH));
	pr_debug("DISPC_IMG_DISP_XY: 0x%x\n", dispc_read(DISPC_IMG_DISP_XY));
	pr_debug("DISPC_Y2R_CTRL: 0x%x\n", dispc_read(DISPC_Y2R_CTRL));
#ifdef CONFIG_FB_SCX15
	pr_debug("DISPC_Y2R_Y_PARAM: 0x%x\n", dispc_read(DISPC_Y2R_Y_PARAM));
	pr_debug("DISPC_Y2R_U_PARAM: 0x%x\n", dispc_read(DISPC_Y2R_U_PARAM));
	pr_debug("DISPC_Y2R_V_PARAM: 0x%x\n", dispc_read(DISPC_Y2R_V_PARAM));
#else
	pr_debug("DISPC_Y2R_CONTRAST: 0x%x\n", dispc_read(DISPC_Y2R_CONTRAST));
	pr_debug("DISPC_Y2R_SATURATION: 0x%x\n",
					dispc_read(DISPC_Y2R_SATURATION));
	pr_debug("DISPC_Y2R_BRIGHTNESS: 0x%x\n",
					dispc_read(DISPC_Y2R_BRIGHTNESS));
#endif
	return 0;
}

static int overlay_osd_configure(struct sprdfb_device *dev, int type,
				overlay_rect *rect, unsigned char *buffer,
				int y_endian, int uv_endian, bool rb_switch)
{
	uint32_t reg_value;

	pr_debug("sprdfb: [%s] : %d, (%d, %d,%d,%d), 0x%x\n", __func__,
				type, rect->x, rect->y, rect->h, rect->w,
				(unsigned int)buffer);

	if (SPRD_OVERLAY_STATUS_ON  != dispc_ctx.overlay_state) {
		pr_err("sprdfb: Overlay config fail (not opened)");
		return -1;
	}

	if ((type >= SPRD_DATA_TYPE_LIMIT) ||
					(type <= SPRD_DATA_TYPE_YUV400)) {

		pr_err("sprdfb: Overlay config fail (type error)");
		return -1;
	}

	if (y_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT) {
		pr_err("sprdfb:[%s] fail.(rgb endian error)",
								__func__);
		return -1;
	}
/*
	lcdc_write(((type << 3) | (1 << 0)), LCDC_IMG_CTRL);
	lcdc_write((type << 3) , LCDC_IMG_CTRL);
*/

	/*use premultiply pixel alpha*/
	reg_value = (y_endian<<8)|(type << 4|(1<<2))|(2<<16);
	if (rb_switch)
		reg_value |= (1 << 15);

	dispc_write(reg_value, DISPC_OSD_CTRL);

	dispc_write((uint32_t)buffer, DISPC_OSD_BASE_ADDR);

	reg_value = (rect->h << 16) | (rect->w);
	dispc_write(reg_value, DISPC_OSD_SIZE_XY);

	dispc_write(rect->w, DISPC_OSD_PITCH);

	reg_value = (rect->y << 16) | (rect->x);
	dispc_write(reg_value, DISPC_OSD_DISP_XY);


	pr_debug("DISPC_OSD_CTRL: 0x%x\n", dispc_read(DISPC_OSD_CTRL));
	pr_debug("DISPC_OSD_BASE_ADDR: 0x%x\n",
					dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("DISPC_OSD_SIZE_XY: 0x%x\n", dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("DISPC_OSD_PITCH: 0x%x\n", dispc_read(DISPC_OSD_PITCH));
	pr_debug("DISPC_OSD_DISP_XY: 0x%x\n", dispc_read(DISPC_OSD_DISP_XY));

	return 0;
}

static int overlay_close(struct sprdfb_device *dev)
{
	if (SPRD_OVERLAY_STATUS_OFF  == dispc_ctx.overlay_state) {
		pr_err("sprdfb:[%s]fail. (has been closed)\n",
								__func__);
		return 0;
	}

/*
	if (0 != sprd_lcdc_sync(dev)) {
		pr_err("sprdfb: overlay close fail. (wait done fail)\n");
		return -1;
	}
*/
	dispc_set_bg_color(0xFFFFFFFF);
	dispc_set_bits(BIT(2), DISPC_OSD_CTRL); /*use block alpha*/
	dispc_write(0xff, DISPC_OSD_ALPHA);
	dispc_clear_bits(BIT(0), DISPC_IMG_CTRL); /* disable the image layer */
	dispc_write(0, DISPC_IMG_Y_BASE_ADDR);
	dispc_write(0, DISPC_OSD_BASE_ADDR);
	dispc_layer_init(&(dev->fb->var));
	dispc_ctx.overlay_state = SPRD_OVERLAY_STATUS_OFF;

	return 0;
}

/*TO DO: need mutext with suspend, resume*/
static int32_t sprdfb_dispc_enable_overlay(struct sprdfb_device *dev,
					struct overlay_info *info, int enable)
{
	int result = -1;
	bool is_refresh_lock_down = false;
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
	bool is_clk_enable = false;
#endif

	pr_debug("sprdfb: [%s]: %d, %d\n", __func__, enable, dev->enable);

	if (enable) {  /*enable*/
		if (NULL == info) {
			pr_err("sprdfb:[%s] fail(Invalid parameter)\n",
								__func__);
			goto ERROR_ENABLE_OVERLAY;
		}

		down(&dev->refresh_lock);
		is_refresh_lock_down = true;

		if (0 == dev->enable) {
			pr_err("sprdfb:[%s] fail.(dev not enable)\n",
								__func__);
			goto ERROR_ENABLE_OVERLAY;
		}

		if (0 != dispc_sync(dev)) {
			pr_err("sprdfb:[%s] fail.(wait done fail)\n",
								__func__);
			goto ERROR_ENABLE_OVERLAY;
		}

#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
			if (sprdfb_dispc_clk_enable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_COUNT)) {
				pr_warn("sprdfb:[%s] clk enable fail!\n",
								__func__);
				goto ERROR_ENABLE_OVERLAY;
			}
			is_clk_enable = true;
		}
#endif
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
		if (SPRD_OVERLAY_STATUS_STARTED == dispc_ctx.overlay_state)
			overlay_close(dev);
#endif
		result = overlay_open();
		if (0 != result) {
			result = -1;
			goto ERROR_ENABLE_OVERLAY;
		}

		if (SPRD_LAYER_IMG == info->layer_index)
			result = overlay_img_configure(dev, info->data_type,
			&(info->rect), info->buffer, info->y_endian,
			info->uv_endian, info->rb_switch);
		else if (SPRD_LAYER_OSD == info->layer_index)
			result = overlay_osd_configure(dev, info->data_type,
			&(info->rect), info->buffer, info->y_endian,
			info->uv_endian, info->rb_switch);
		else
			pr_err("sprdfb:[%s] fail.(invalid layer)\n",
								__func__);
		if (0 != result) {
			result = -1;
			goto ERROR_ENABLE_OVERLAY;
		}
		/*result = overlay_start(dev);*/
	} else { /*disable*/
		/*result = overlay_close(dev);*/
	}
ERROR_ENABLE_OVERLAY:
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
	if (is_clk_enable)
		sprdfb_dispc_clk_disable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_COUNT);
#endif
	if (is_refresh_lock_down)
		up(&dev->refresh_lock);

	pr_debug("sprdfb: [%s] return %d\n", __func__, result);
	return result;
}


static int32_t sprdfb_dispc_display_overlay(struct sprdfb_device *dev,
					struct overlay_display *setting)
{
	struct overlay_rect *rect = &(setting->rect);
	uint32_t size = ((rect->h << 16) | (rect->w & 0xffff));

	dispc_ctx.dev = dev;

	pr_debug("sprdfb: [%s]: layer:%d, (%d, %d,%d,%d)\n", __func__,
				setting->layer_index, setting->rect.x,
				setting->rect.y, setting->rect.h,
				setting->rect.w);

	down(&dev->refresh_lock);
	if (0 == dev->enable) {
		pr_err("sprdfb: [%s] leave (Invalid device status)!\n",
								__func__);
		goto ERROR_DISPLAY_OVERLAY;
	}
	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
		dispc_ctx.vsync_waiter++;
		dispc_sync(dev);
		/*dispc_ctx.vsync_done = 0;*/
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if (sprdfb_dispc_clk_enable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_REFRESH)) {
			pr_warn("sprdfb:[%s] clk enable fail!\n",
								__func__);
			goto ERROR_DISPLAY_OVERLAY;
		}
#endif

	}
#ifdef CONFIG_FB_MMAP_CACHED
	dispc_dithering_enable(true);
#endif
	pr_debug(KERN_INFO "srpdfb: [%s] got sync\n", __func__);

	dispc_ctx.dev = dev;

#ifdef LCD_UPDATE_PARTLY
	if ((setting->rect->h < dev->panel->height) ||
		(setting->rect->w < dev->panel->width)) {
		dispc_write(size, DISPC_SIZE_XY);

		sprdfb_panel_invalidate_rect(dev->panel, rect->x, rect->y,
				rect->x + rect->w-1, rect->y + rect->h-1);
	} else
#endif
	{
		dispc_write(size, DISPC_SIZE_XY);

		if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
			sprdfb_panel_invalidate(dev->panel);
	}

	sprdfb_panel_before_refresh(dev);

	dispc_clear_bits(BIT(0), DISPC_OSD_CTRL);
	if (SPRD_OVERLAY_STATUS_ON == dispc_ctx.overlay_state) {
		if (overlay_start(dev, setting->layer_index) != 0) {
			pr_err("%s[%d] err, return without run dispc!\n",
							__func__, __LINE__);
			goto ERROR_DISPLAY_OVERLAY;
		}
	}

	dispc_run(dev);

	if ((SPRD_OVERLAY_DISPLAY_SYNC == setting->display_mode) &&
				(SPRDFB_PANEL_IF_DPI != dev->panel_if_type)) {

		dispc_ctx.vsync_waiter++;
		if (dispc_sync(dev) != 0)/* time out??? disable ?? */
			pr_info("sprdfb:[%s] time out!\n", __func__);
	}

	dev->fps.fps_checking_mode = SPRD_DISPLAY_UPDATE_OVERLAY;
	runtime_fps_check(dev);

ERROR_DISPLAY_OVERLAY:
	up(&dev->refresh_lock);

	pr_debug("DISPC_CTRL: 0x%x\n", dispc_read(DISPC_CTRL));
	pr_debug("DISPC_SIZE_XY: 0x%x\n", dispc_read(DISPC_SIZE_XY));

	pr_debug("DISPC_BG_COLOR: 0x%x\n", dispc_read(DISPC_BG_COLOR));

	pr_debug("DISPC_INT_EN: 0x%x\n", dispc_read(DISPC_INT_EN));

	pr_debug("DISPC_OSD_CTRL: 0x%x\n", dispc_read(DISPC_OSD_CTRL));
	pr_debug("DISPC_OSD_BASE_ADDR: 0x%x\n",
					dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("DISPC_OSD_SIZE_XY: 0x%x\n", dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("DISPC_OSD_PITCH: 0x%x\n", dispc_read(DISPC_OSD_PITCH));
	pr_debug("DISPC_OSD_DISP_XY: 0x%x\n", dispc_read(DISPC_OSD_DISP_XY));
	pr_debug("DISPC_OSD_ALPHA: 0x%x\n", dispc_read(DISPC_OSD_ALPHA));
	return 0;
}

#endif

#ifdef CONFIG_FB_VSYNC_SUPPORT
static int32_t spdfb_dispc_wait_for_vsync(struct sprdfb_device *dev)
{
	pr_debug("sprdfb: [%s]\n", __func__);
	int ret;

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		if (!dispc_ctx.is_first_frame) {
			dispc_ctx.waitfor_vsync_done = 0;
			dispc_set_bits(BIT(0), DISPC_INT_EN);
			dispc_ctx.waitfor_vsync_waiter++;
			ret  = wait_event_interruptible_timeout(dispc_ctx.waitfor_vsync_queue,
					dispc_ctx.waitfor_vsync_done,
					msecs_to_jiffies(100));
			dispc_ctx.waitfor_vsync_waiter = 0;
		}
	} else {
		dispc_ctx.waitfor_vsync_done = 0;
		dispc_set_bits(BIT(1), DISPC_INT_EN);
		dispc_ctx.waitfor_vsync_waiter++;
		ret  = wait_event_interruptible_timeout(dispc_ctx.waitfor_vsync_queue,
				dispc_ctx.waitfor_vsync_done,
				msecs_to_jiffies(100));
		dispc_ctx.waitfor_vsync_waiter = 0;
	}
	pr_debug("sprdfb:[%s] (%d)\n", __func__, ret);
	return 0;
}
#endif

static void dispc_stop_for_feature(struct sprdfb_device *dev)
{
	int i = 0;

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		dispc_stop(dev);
		while (dispc_read(DISPC_DPI_STS1) & BIT(16)) {
			if (0x0 == ++i%500000) {
				pr_info("sprdfb: [%s] busy waiting stop!\n",
								__func__);
			}
		}
		udelay(25);
	}
}

static void dispc_run_for_feature(struct sprdfb_device *dev)
{
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	if (SPRD_OVERLAY_STATUS_ON != dispc_ctx.overlay_state)
#endif
		dispc_run(dev);
}

#ifdef CONFIG_FB_DISPC_DYNAMIC_PCLK_SUPPORT
static int32_t sprdfb_dispc_chg_pclk(struct sprdfb_device *fb_dev,
			u32 new_pclk)
{
	int ret;

	pr_debug("%s --enter--\n", __func__);
	if (!fb_dev) {
		pr_err("fb_dev can't be found\n");
		return -ENXIO;
	}
	/* check if new clock is valid */
	if (new_pclk <= 0) {
		pr_err("new pixel clock is invalid\n");
		return -EINVAL;
	}

	down(&fb_dev->refresh_lock);
	/* Now let's do update pixel clock */
	ret = clk_set_rate(dispc_ctx.clk_dispc_dpi, new_pclk);
	if (ret) {
		pr_warn("Failed to set pixel clock.\n");
		up(&fb_dev->refresh_lock);
		return ret;
	}
	pr_info("pixel clock is switched from %dHz to %dHz\n",
						fb_dev->dpi_clock, new_pclk);
	/* Save the updated clock */
	fb_dev->dpi_clock = new_pclk;
	fb_dev->dyna_dpi_clk = new_pclk;
	up(&fb_dev->refresh_lock);
	pr_debug("%s --exit--\n", __func__);

	return ret;
}
#endif

#ifdef CONFIG_FB_DYNAMIC_FPS_SUPPORT
static int32_t sprdfb_update_fps_clock(struct sprdfb_device *dev, int fps_level)
{
	uint32_t fps, dpi_clock;
	struct panel_spec *panel = dev->panel;
	fps = panel->fps;
	dpi_clock = dev->dpi_clock;

	pr_info("sprdfb:sprdfb_update_fps_clock--fps_level:%dn", fps_level);
	if ((fps_level < 40) || (fps_level > 64)) {
		pr_info("sprdfb: invalid fps set!\n");
		return -1;
	}

	panel->fps = fps_level;

	dispc_update_clock(dev);
	dsi_dpi_init(dev);

	panel->fps = fps;
	dev->dpi_clock = dpi_clock;
	return 0;
}


static int32_t sprdfb_dispc_change_fps(struct sprdfb_device *dev, int fps_level)
{
	int32_t ret = 0;
	if (NULL == dev || 0 == dev->enable) {
		pr_err("sprdfb: [%s] fail.(dev not enable)\n",
								__func__);
		return -1;
	}
	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
		dispc_ctx.vsync_waiter++;
		dispc_sync(dev);
		sprdfb_panel_change_fps(dev, fps_level);
	} else {
		down(&dev->refresh_lock);
		dispc_stop_for_feature(dev);

		ret = sprdfb_update_fps_clock(dev, fps_level);

		dispc_run_for_feature(dev);
		up(&dev->refresh_lock);
	}
	return ret;
}

static int32_t sprdfb_dispc_notify_change_fps(struct dispc_dbs *h, int fps_level)
{
	struct sprdfb_dispc_context *dispc_ctx = (struct sprdfb_dispc_context *)h->data;
	struct sprdfb_device *dev = dispc_ctx->dev;

	pr_info("sprdfb:[%s]: %d\n", __func__, fps_level);

	return sprdfb_dispc_change_fps(dev, fps_level);
}

#endif

#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || defined(CONFIG_SPRD_SCX35_DMC_FREQ))
/*
return value:
0 -- Allow DMC change frequency
1 -- Don't allow DMC change frequency
*/
static unsigned int sprdfb_dispc_change_threshold(struct devfreq_dbs *h,
							unsigned int state)
{
	struct sprdfb_dispc_context *dispc_ctx =
					(struct sprdfb_dispc_context *)h->data;

	struct sprdfb_device *dev = dispc_ctx->dev;

	if (NULL == dev || 0 == dev->enable) {
		pr_err("sprdfb: [%s] fail.(dev not enable)\n",
								__func__);
		return 0;
	}

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		down(&dev->refresh_lock);

		if (state == DEVFREQ_PRE_CHANGE)
			dispc_write(0x9600960, 0x0c);
		else
			dispc_write(0x5000500, 0x0c);

		up(&dev->refresh_lock);
	}
	return 0;
}
#endif

/*begin bug210112*/
static int32_t sprdfb_dispc_refresh_logo(struct sprdfb_device *dev)
{
	uint32_t i = 0;
	unsigned long flags;
	pr_debug("%s:[%d] panel_if_type:%d\n", __func__, __LINE__,
							dev->panel_if_type);

	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
		sprdfb_panel_invalidate(dev->panel);

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		local_irq_save(flags);
		dispc_set_bits(BIT(4), DISPC_DPI_CTRL);	/*sw*/
		dispc_clear_bits(BIT(4), DISPC_CTRL);	/*stop running*/
		while (dispc_read(DISPC_DPI_STS1) & BIT(16)) {
			if (0x0 == ++i%500000) {
				printk("sprdfb:[%s] busy waiting stop!\n",
								__func__);
			}
		}
		udelay(25);

		dispc_clear_bits(0x1f, DISPC_INT_EN);	/*disable all interrupt*/
		dispc_set_bits(0x1f, DISPC_INT_CLR);	/* clear all interruption*/

		dispc_set_bits(BIT(5), DISPC_DPI_CTRL);	/*update*/
		udelay(30);
		dispc_clear_bits(BIT(4), DISPC_DPI_CTRL); /*SW and VSync*/
		dispc_set_bits(BIT(4), DISPC_CTRL);	/*run*/
		local_irq_restore(flags);
	} else {
		/* start refresh */
		dispc_set_bits((1 << 4), DISPC_CTRL);
		for (i = 0; i < 500; i++) {
			if (0x1 != (dispc_read(DISPC_INT_RAW) & (1<<0)))
				udelay(1000);
			else
				break;
		}
		if (i >= 1000) {
			pr_info("sprdfb:[%s] dispc done int time out!(0x%x)\n",
					__func__, dispc_read(DISPC_INT_RAW));
		} else {
			pr_info("sprdfb:[%s] got dispc done int (0x%x)\n",
					__func__, dispc_read(DISPC_INT_RAW));
		}
		dispc_set_bits((1<<0), DISPC_INT_CLR);
	}
	return 0;
}


static void sprdfb_dispc_logo_config(struct sprdfb_device *dev, uint32_t logo_dst_p)
{
	uint32_t reg_val = 0;

	pr_debug("%s[%d] enter,dev:0x%08x\n", __func__, __LINE__, dev);

	dispc_clear_bits((1<<0), DISPC_IMG_CTRL);
	dispc_clear_bits((1<<0), DISPC_OSD_CTRL);

	/******************* OSD layer setting **********************/

	/* OSD layer alpha value */
	dispc_write(0xff, DISPC_OSD_ALPHA);
	reg_val = ((dev->panel->width & 0xfff) |
					((dev->panel->height & 0xfff) << 16));
	dispc_write(reg_val, DISPC_SIZE_XY);
#ifdef CONFIG_FB_LOW_RES_SIMU
	if ((0 != dev->panel->display_width) &&
					(0 != dev->panel->display_height)) {
		reg_val = ((dev->panel->display_width & 0xfff) |
				((dev->panel->display_height & 0xfff) << 16));
	}
#endif
	dispc_write(reg_val, DISPC_OSD_SIZE_XY);

	/* OSD layer start position */
	dispc_write(0, DISPC_OSD_DISP_XY);

	/* OSD layer pitch */
#ifdef CONFIG_FB_LOW_RES_SIMU
	if ((0 != dev->panel->display_width) &&
					(0 != dev->panel->display_height))
		reg_val = (dev->panel->display_width & 0xfff);
	else
#endif
	reg_val = (dev->panel->width & 0xfff);
	dispc_write(reg_val, DISPC_OSD_PITCH);

	/*OSD base address*/
	dispc_write(logo_dst_p, DISPC_OSD_BASE_ADDR);

	/* OSD color_key value */
	dispc_set_osd_ck(0x0);

	reg_val = 0;
	/*enable OSD layer*/
	reg_val |= (1 << 0);

	/*disable  color key */

	/* alpha mode select  - block alpha*/
	reg_val |= (1 << 2);

	/* data format */
	/* RGB565 */
	reg_val |= (5 << 4);
	/* B2B3B0B1 */
	reg_val |= (2 << 8);

	dispc_write(reg_val, DISPC_OSD_CTRL);

	dispc_clear_bits(0x30000, DISPC_CTRL);
	dispc_set_bits(0x10000, DISPC_CTRL);
}

void sprdfb_dispc_logo_proc(struct sprdfb_device *dev)
{
	uint32_t kernel_fb_size = 0;
	uint32_t logo_src_v = 0;
	uint32_t logo_dst_v = 0; /*use the second frame buffer,virtual */
	uint32_t logo_dst_p = 0; /*use the second frame buffer ,physica*/
	uint32_t logo_size = 0;	/*should be rgb565*/

	pr_debug("sprdfb: %s[%d] enter.\n", __func__, __LINE__);

	if (dev == NULL) {
		pr_err("%s[%d]: return without process logo!\n",
							__func__, __LINE__);
		return;
	}

	if (lcd_base_from_uboot == 0) {
		pr_err("%s[%d]: return without process logo!\n",
							__func__, __LINE__);
		return;
	}

/*#define USE_OVERLAY_BUFF*/
#ifdef CONFIG_FB_LOW_RES_SIMU
	if ((0 != dev->panel->display_width) &&
					(0 != dev->panel->display_height)) {
		logo_size = dev->panel->display_width *
				dev->panel->display_height * 2;
				/*should be rgb565*/
	} else
#endif
	logo_size = dev->panel->width * dev->panel->height * 2;
				/* should be rgb565*/
#if 0
#ifndef USE_OVERLAY_BUFF
	kernel_fb_size =
			dev->panel->width * dev->panel->height * (dev->bpp / 8);
	kernel_fb_size = 0;

	/*use the second frame buffer*/
	logo_dst_v = dev->fb->screen_base + kernel_fb_size;
	/*use the second frame buffer*/
	logo_dst_p = dev->fb->fix.smem_start + kernel_fb_size;
#else
	/*use overlay frame buffer*/
	logo_dst_p = SPRD_ION_OVERLAY_BASE-logo_size;
	logo_dst_v =  (uint32_t)ioremap(logo_dst_p, logo_size);
#endif
#else
	dev->logo_buffer_size = logo_size;
	dev->logo_buffer_addr_v = __get_free_pages(GFP_ATOMIC | __GFP_ZERO ,
							get_order(logo_size));
	if (!dev->logo_buffer_addr_v) {
		pr_err("sprdfb:[%s] Failed to allocate buffer\n",
								__func__);
		return;
	}
	pr_info("sprdfb:got [%d] bytes logo proc buffer at 0x%lx\n",
					logo_size, dev->logo_buffer_addr_v);

	logo_dst_v = dev->logo_buffer_addr_v;
	logo_dst_p = __pa(dev->logo_buffer_addr_v);

#endif
	logo_src_v =  (uint32_t)ioremap(lcd_base_from_uboot, logo_size);

	if (!logo_src_v || !logo_dst_v) {
		pr_err("%s[%d]: Unable to map boot logo memory: src-0x%08x, dst-0x%0x8\n",
				__func__, __LINE__, logo_src_v, logo_dst_v);
		return;
	}

	pr_info("%s[%d]: lcd_base_from_uboot: 0x%08x, logo_src_v:0x%08x\n",
			__func__, __LINE__, lcd_base_from_uboot, logo_src_v);
	pr_info("%s[%d]: logo_dst_p:0x%08x,logo_dst_v:0x%08x\n",
				__func__, __LINE__, logo_dst_p, logo_dst_v);

	memcpy(logo_dst_v, logo_src_v, logo_size);

	dmac_flush_range(logo_dst_v, logo_dst_v + logo_size);

	iounmap(logo_src_v);
#if 0
#ifdef USE_OVERLAY_BUFF
	iounmap(logo_dst_v);
#endif
#endif
	/*dispc_print_osd_config(__func__,__LINE__);*/
	sprdfb_dispc_logo_config(dev, logo_dst_p);
	sprdfb_dispc_refresh_logo(dev);
	/*dispc_print_osd_config(__func__,__LINE__);*/
}

/*end bug210112*/
#ifdef CONFIG_FB_MMAP_CACHED
void sprdfb_set_vma(struct vm_area_struct *vma)
{
	if (NULL != vma)
		dispc_ctx.vma = vma;
}
#endif

struct display_ctrl sprdfb_dispc_ctrl = {
	.name		= "dispc",
	.early_init	= sprdfb_dispc_early_init,
	.init		= sprdfb_dispc_init,
	.uninit		= sprdfb_dispc_uninit,
	.refresh	= sprdfb_dispc_refresh,
	.logo_proc	= sprdfb_dispc_logo_proc,
	.suspend	= sprdfb_dispc_suspend,
	.resume		= sprdfb_dispc_resume,
	.update_clk	= dispc_update_clock,
#ifdef CONFIG_LCD_ESD_RECOVERY
	.ESD_reset	= sprdfb_dispc_esd_reset,
#endif
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	.enable_overlay = sprdfb_dispc_enable_overlay,
	.display_overlay = sprdfb_dispc_display_overlay,
#endif
#ifdef CONFIG_FB_VSYNC_SUPPORT
	.wait_for_vsync = spdfb_dispc_wait_for_vsync,
#endif
#ifdef CONFIG_FB_DISPC_DYNAMIC_PCLK_SUPPORT
	.change_pclk	= sprdfb_dispc_chg_pclk,
#endif
#ifdef CONFIG_FB_DYNAMIC_FPS_SUPPORT
	.change_fps	= sprdfb_dispc_change_fps,
#endif
#ifdef CONFIG_FB_MMAP_CACHED
	.set_vma	= sprdfb_set_vma,
#endif

};
