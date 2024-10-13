/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "sprdfb_dispc: " fmt

#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/delay.h>
#if defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || defined(CONFIG_SPRD_SCX35_DMC_FREQ)
#include <linux/devfreq.h>
#endif
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#ifdef CONFIG_OF
#include <linux/of_irq.h>
#endif
#include <asm/cacheflush.h>
#include "sprdfb_dispc_reg.h"
#include "sprdfb_panel.h"
#include "sprdfb.h"
#include "sprdfb_chip_common.h"
#include <mach/cpuidle.h>
#include <linux/dma-mapping.h>
#define DISPC_AHB_CLOCK_MCU_SLEEP_FEATURE
#undef SPRDFB_DISPC_DEBUG
#define VSYNC_TIMEOUT_MS	(500)

#ifndef CONFIG_OF
#ifdef CONFIG_FB_SCX15
#define DISPC_CLOCK_PARENT ("clk_192m")
#define DISPC_CLOCK (192*1000000)
#define DISPC_DBI_CLOCK_PARENT ("clk_256m")
#define DISPC_DBI_CLOCK (256*1000000)
#define DISPC_DPI_CLOCK_PARENT ("clk_384m")
#define SPRDFB_DPI_CLOCK_SRC (384000000)
#else
#define DISPC_CLOCK_PARENT ("clk_256m")
#define DISPC_CLOCK (256*1000000)
#define DISPC_DBI_CLOCK_PARENT ("clk_256m")
#define DISPC_DBI_CLOCK (256*1000000)
#define DISPC_DPI_CLOCK_PARENT ("clk_384m")
#define SPRDFB_DPI_CLOCK_SRC (384000000)
#endif
#define DISPC_EMC_EN_PARENT ("clk_aon_apb")
#else
#define DISPC_CLOCK_PARENT ("dispc_clk_parent")
#define DISPC_DBI_CLOCK_PARENT ("dispc_dbi_clk_parent")
#define DISPC_DPI_CLOCK_PARENT ("dispc_dpi_clk_parent")
#define DISPC_EMC_EN_PARENT ("dispc_emc_clk_parent")
#define DISPC_PLL_CLK ("dispc_clk")
#define DISPC_DBI_CLK ("dispc_dbi_clk")
#define DISPC_DPI_CLK ("dispc_dpi_clk")
#define DISPC_EMC_CLK	 ("dispc_emc_clk")

#define DISPC_CLOCK_SRC_ID 0
#define DISPC_DBI_CLOCK_SRC_ID 1
#define DISPC_DPI_CLOCK_SRC_ID 2

#define DISPC_CLOCK_NUM 3
#endif

#if (defined CONFIG_FB_SCX15) || (defined CONFIG_FB_SCX30G)
#define SPRDFB_BRIGHTNESS	(0x02 << 16)
#define SPRDFB_CONTRAST		(0x12A << 0)
#define SPRDFB_OFFSET_U		(0x80 << 16)
#define SPRDFB_SATURATION_U	(0x123 << 0)
#define SPRDFB_OFFSET_V		(0x80 << 16)
#define SPRDFB_SATURATION_V	(0x123 << 0)
#else
#define SPRDFB_CONTRAST		(74)
#define SPRDFB_SATURATION	(73)
#define SPRDFB_BRIGHTNESS	(2)
#endif

typedef enum {
	SPRDFB_DYNAMIC_CLK_FORCE,	/* force enable/disable */
	SPRDFB_DYNAMIC_CLK_REFRESH,	/* enable for refresh/display_overlay */
	SPRDFB_DYNAMIC_CLK_COUNT,	/* enable disable in pairs */
	SPRDFB_DYNAMIC_CLK_MAX,
} SPRDFB_DYNAMIC_CLK_SWITCH_E;

struct sprdfb_dispc_context {
	struct clk		*clk_dispc;
	struct clk		*clk_dispc_dpi;
	struct clk		*clk_dispc_dbi;
	struct clk		*clk_dispc_emc;
	bool			is_inited;
	bool			is_first_frame;
	bool			clk_is_open;
	bool			clk_is_refreshing;
	int			clk_open_count;

	struct sprdfb_device	*dev;

	uint32_t		vsync_waiter;
	wait_queue_head_t	vsync_queue;
	uint32_t		vsync_done;

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	/* overlay */
	/*0-closed, 1-configed, 2-started*/
	uint32_t overlay_state;
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
#ifdef CONFIG_OF
uint32_t g_dispc_base_addr;
#endif
static DEFINE_MUTEX(dispc_lock);
static DEFINE_MUTEX(dispc_ctx_lock);

extern void sprdfb_panel_start(struct sprdfb_device *dev);
extern void sprdfb_panel_suspend(struct sprdfb_device *dev);
extern void sprdfb_panel_resume(struct sprdfb_device *dev, bool);
extern void sprdfb_panel_before_refresh(struct sprdfb_device *dev);
extern void sprdfb_panel_after_refresh(struct sprdfb_device *dev);
extern void sprdfb_panel_invalidate(struct panel_spec *self);
extern void sprdfb_panel_invalidate_rect(struct panel_spec *self,
		uint16_t left, uint16_t top,
		uint16_t right, uint16_t bottom);

#ifdef CONFIG_FB_ESD_SUPPORT
extern uint32_t sprdfb_panel_ESD_check(struct sprdfb_device *dev);
#endif
#ifdef CONFIG_LCD_ESD_RECOVERY
extern void sprdfb_panel_ESD_reset(struct sprdfb_device *dev);
#endif

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
static int overlay_start(struct sprdfb_device *dev, uint32_t layer_index);
static int overlay_close(struct sprdfb_device *dev);
#endif

static int sprdfb_dispc_clk_disable(struct sprdfb_dispc_context *,
		SPRDFB_DYNAMIC_CLK_SWITCH_E);
static int sprdfb_dispc_clk_enable(struct sprdfb_dispc_context *,
		SPRDFB_DYNAMIC_CLK_SWITCH_E);
static int32_t sprdfb_dispc_init(struct sprdfb_device *dev);
static void dispc_reset(void);
static void dispc_stop(struct sprdfb_device *dev);
static void dispc_module_enable(void);
static void dispc_stop_for_feature(struct sprdfb_device *dev);
static void dispc_run_for_feature(struct sprdfb_device *dev);
static unsigned int sprdfb_dispc_change_threshold(struct devfreq_dbs *h,
		unsigned int state);
static int dispc_update_clk(struct sprdfb_device *fb_dev,
		u32 new_val, int howto);

/**
 * author: Yang.Haibing haibing.yang@spreadtrum.com
 *
 * dispc_check_new_clk()
 * check and convert new clock to the value that can be set
 * @fb_dev: spreadtrum specific fb device
 * @new_pclk: actual settable clock via calculating new_val and fps
 * @pclk_src: clock source of dpi clock
 * @new_val: expected clock
 * @type: check the type is fps or pclk
 *
 * Returns 0 on success, MINUS on error.
 */
static int dispc_check_new_clk(struct sprdfb_device *fb_dev,
		u32 *new_pclk, u32 pclk_src,
		u32 new_val, int type)
{
	int divider;
	u32 hpixels, vlines, pclk, fps;
	struct panel_spec *panel = fb_dev->panel;
	struct info_mipi *mipi;
	struct info_rgb *rgb;

	pr_debug("%s: enter\n", __func__);
	if (!panel) {
		pr_err("%s, no panel\n", __func__);
		return -ENXIO;
	}

	mipi = panel->info.mipi;
	rgb = panel->info.rgb;

	if (fb_dev->panel_if_type != SPRDFB_PANEL_IF_DPI) {
		pr_err("%s, panel if should be DPI\n", __func__);
		return -EINVAL;
	}
	if (new_val <= 0 || !new_pclk) {
		pr_err("%s, invalid parameter (%d, %d)\n",
				__func__, new_val, new_pclk);
		return -EINVAL;
	}

	if (fb_dev->panel->type == LCD_MODE_DSI) {
		hpixels = panel->width + mipi->timing->hsync +
			mipi->timing->hbp + mipi->timing->hfp;
		vlines = panel->height + mipi->timing->vsync +
			mipi->timing->vbp + mipi->timing->vfp;
	} else if (fb_dev->panel->type == LCD_MODE_RGB) {
		hpixels = panel->width + rgb->timing->hsync +
			rgb->timing->hbp + rgb->timing->hfp;
		vlines = panel->height + rgb->timing->vsync +
			rgb->timing->vbp + rgb->timing->vfp;
	} else {
		pr_err("%s, unexpected panel type (%d)\n",
				__func__, fb_dev->panel->type);
		return -EINVAL;
	}

	switch (type) {
	/*
	 * FIXME: TODO: SPRDFB_FORCE_FPS will be used for accurate fps someday.
	 * But now it is the same as SPRDFB_DYNAMIC_FPS.
	 */
	case SPRDFB_FORCE_FPS:
	case SPRDFB_DYNAMIC_FPS:
		if (new_val < LCD_MIN_FPS || new_val > LCD_MAX_FPS) {
			pr_err("%s, out of range (%d), range(%d, %d)\n",
					__func__, new_val,
					LCD_MIN_FPS, LCD_MAX_FPS);
			return -EINVAL;
		}
		pclk = hpixels * vlines * new_val;
		divider = ROUND(pclk_src, pclk);
		*new_pclk = pclk_src / divider;
		/* Save the updated fps */
		panel->fps = new_val;
		break;

	case SPRDFB_DYNAMIC_PCLK:
		divider = ROUND(pclk_src, new_val);
		pclk = pclk_src / divider;
		fps = pclk / (hpixels * vlines);
		if (fps < LCD_MIN_FPS || fps > LCD_MAX_FPS) {
			pr_err("%s, out of range (%d), range(%d, %d)\n",
					__func__, fps,
					LCD_MIN_FPS, LCD_MAX_FPS);
			return -EINVAL;
		}
		*new_pclk = pclk;
		/* Save the updated fps */
		panel->fps = fps;
		break;

	default:
		pr_err("%s, unsupported type (%d)\n", __func__, type);
		*new_pclk = 0;
		return -EINVAL;
	}
	return 0;
}

void dispc_irq_trick(struct sprdfb_device *dev)
{
	if (!dev)
		return;

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type)
		dispc_set_bits(DISPC_INT_ERR_MASK, DISPC_INT_EN);
}

extern void dsi_irq_trick(void);

static irqreturn_t dispc_isr(int irq, void *data)
{
	struct sprdfb_dispc_context *dispc_ctx =
		(struct sprdfb_dispc_context *)data;
	uint32_t reg_val;
	struct sprdfb_device *dev = dispc_ctx->dev;
	bool done = false;
#ifdef CONFIG_FB_VSYNC_SUPPORT
	bool vsync = false;
#endif
	reg_val = dispc_read(DISPC_INT_STATUS);

	pr_debug("%s, status : 0x%X\n", __func__, reg_val);

	if (reg_val & DISPC_INT_ERR_MASK) {
		pr_err("%s, dispc underflow : 0x%x\n", __func__, reg_val);
		dispc_write(DISPC_INT_ERR_MASK, DISPC_INT_CLR);
		/* disable err interupt */
		dispc_clear_bits(DISPC_INT_ERR_MASK, DISPC_INT_EN);
	}

	if (!dev)
		return IRQ_HANDLED;

	if ((reg_val & DISPC_INT_UPDATE_DONE_MASK) &&
			(SPRDFB_PANEL_IF_DPI == dev->panel_if_type)) {
		/* dispc update done isr */
		dispc_write(DISPC_INT_UPDATE_DONE_MASK, DISPC_INT_CLR);
		done = true;
	} else if ((reg_val & DISPC_INT_DONE_MASK) &&
			(SPRDFB_PANEL_IF_DPI != dev->panel_if_type)) {
		/* dispc done isr */
		dispc_write(DISPC_INT_DONE_MASK, DISPC_INT_CLR);
		dispc_ctx->is_first_frame = false;
		done = true;
	}
#ifdef CONFIG_FB_ESD_SUPPORT
#ifdef FB_CHECK_ESD_BY_TE_SUPPORT
	if ((reg_val & DISPC_INT_TE_MASK) &&
			(SPRDFB_PANEL_IF_DPI == dev->panel_if_type)) {
		/*dispc external TE isr*/
		dispc_write(DISPC_INT_TE_MASK, DISPC_INT_CLR);
		if (dev->esd_te_waiter) {
			pr_info("%s, esd_te_done\n", __func__);
			dev->esd_te_done = 1;
			wake_up_interruptible_all(&(dev->esd_te_queue));
			dev->esd_te_waiter = 0;
		}
	}
#endif
#endif

#ifdef CONFIG_FB_VSYNC_SUPPORT
	if ((reg_val & DISPC_INT_HWVSYNC) &&
			(SPRDFB_PANEL_IF_DPI == dev->panel_if_type)) {
		/* dispc done isr */
		dispc_write(DISPC_INT_HWVSYNC, DISPC_INT_CLR);
		vsync = true;
	} else if ((reg_val & DISPC_INT_TE_MASK) &&
			(SPRDFB_PANEL_IF_EDPI == dev->panel_if_type)) {
		/* dispc te isr */
		dispc_write(DISPC_INT_TE_MASK, DISPC_INT_CLR);
		vsync = true;
	}
	if (vsync) {
		pr_debug("%s, got vsync\n", __func__);
		dispc_ctx->waitfor_vsync_done = 1;
		if (dispc_ctx->waitfor_vsync_waiter)
			wake_up_interruptible_all(
					&(dispc_ctx->waitfor_vsync_queue));
	}
#endif
	if (done) {
		dispc_ctx->vsync_done = 1;
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
			sprdfb_dispc_clk_disable(dispc_ctx,
					SPRDFB_DYNAMIC_CLK_REFRESH);
#endif
		if (dispc_ctx->vsync_waiter) {
			wake_up_interruptible_all(&(dispc_ctx->vsync_queue));
			dispc_ctx->vsync_waiter = 0;
		}
		sprdfb_panel_after_refresh(dev);
		pr_debug("%s, done interrupt (0x%x)\n", __func__, reg_val);
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

static inline void dispc_set_osd_alpha(uint8_t alpha)
{
	dispc_write(alpha, DISPC_OSD_ALPHA);
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

/*
 * thres0: module fetch data when buffer depth <= thres0
 * thres1: module release busy and close AXI clock
 * thres2: module open AXI clock and prepare to fetch data
 */
static void dispc_set_threshold(uint16_t thres0, uint16_t thres1, uint16_t thres2)
{
	dispc_write((thres0)|(thres1<<14)|(thres2<<16), DISPC_BUF_THRES);
}

static void dispc_module_enable(void)
{
	/*dispc module enable */
	dispc_write((1<<0), DISPC_CTRL);

	/*disable dispc INT*/
	dispc_write(0x0, DISPC_INT_EN);

	/* clear dispc INT */
	dispc_write(0x3F, DISPC_INT_CLR);
}

static inline int32_t dispc_set_disp_size(struct fb_var_screeninfo *var)
{
	uint32_t reg_val;

	reg_val = (var->xres & 0xfff) | ((var->yres & 0xfff) << 16);
	dispc_write(reg_val, DISPC_SIZE_XY);

	return 0;
}

static void dispc_layer_init(struct fb_var_screeninfo *var)
{
	uint32_t reg_val = 0;

	dispc_write(0x0, DISPC_IMG_CTRL);
	dispc_clear_bits((1<<0), DISPC_OSD_CTRL);

	/******************* OSD layer setting **********************/
	reg_val |= (1 << 0);	/*enable OSD layer*/
	reg_val |= (1 << 2);	/* alpha mode select - block alpha*/
	if (var->bits_per_pixel == 32) {
		reg_val |= (3 << 4);	/* ABGR */
		reg_val |= (1 << 15);	/* rb switch */
	} else {
		reg_val |= (5 << 4);	/* RGB565 */
		reg_val |= (2 << 8);	/* B2B3B0B1 */
	}
	dispc_write(reg_val, DISPC_OSD_CTRL);
	dispc_set_osd_alpha(0xFF);
	dispc_write(var->xres | (var->yres << 16), DISPC_OSD_SIZE_XY);
	dispc_write(0, DISPC_OSD_DISP_XY);
	dispc_write(var->xres, DISPC_OSD_PITCH);
	dispc_set_osd_ck(0x0);	/* OSD color_key value */
}

static void dispc_layer_update(struct fb_var_screeninfo *var)
{
	uint32_t reg_val = 0;

	/******************* OSD layer setting **********************/
	reg_val |= (1 << 0);	/* enable OSD Layer */
	reg_val |= (1 << 2);	/* alpha mode : block alpha */

	/* data format */
	if (var->bits_per_pixel == 32) {
		reg_val |= (3 << 4);	/* ABGR */
		reg_val |= (1 << 15);	/* rb switch */
	} else {
		reg_val |= (5 << 4);	/* RGB565 */
		reg_val |= (2 << 8);	/* B2B3B0B1 */
	}

	dispc_write(reg_val, DISPC_OSD_CTRL);
}

#ifdef SPRDFB_DISPC_DEBUG
static void dump_dispc_reg(void)
{
	uint32_t i;

	for (i = 0; i < 256; i += 16) {
		pr_info("sprdfb: %x: 0x%x, 0x%x, 0x%x, 0x%x\n",
				i, dispc_read(i), dispc_read(i+4),
				dispc_read(i+8), dispc_read(i+12));
	}
	pr_info("**************************************\n");
}
#else
static inline void dump_dispc_reg(void) {}
#endif

static int32_t dispc_sync(struct sprdfb_device *dev)
{
	int ret;

	if (!dev->enable) {
		pr_err("%s, fb suspended\n", __func__);
		return -1;
	}

	ret = wait_event_interruptible_timeout(dispc_ctx.vsync_queue,
			dispc_ctx.vsync_done, msecs_to_jiffies(VSYNC_TIMEOUT_MS));

	if (!ret) { /* time out */
		dispc_ctx.vsync_done = 1; /*error recovery */
		pr_err("%s timeout\n", __func__);
		dump_dispc_reg();
		return -1;
	}
	return 0;
}

static void dispc_run(struct sprdfb_device *dev)
{
	uint32_t flags = 0;
	int i = 0;

	if (!dev->enable)
		return;

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
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

			dispc_ctx.is_first_frame = false;
		} else {
#ifndef CONFIG_FB_TRIPLE_FRAMEBUFFER
			dispc_sync(dev);
#else
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
			if (SPRD_OVERLAY_STATUS_STARTED ==
					dispc_ctx.overlay_state)
				dispc_sync(dev);
#endif
#endif
		}
	} else {
		dispc_ctx.vsync_done = 0;
		/* start refresh */
		dispc_set_bits((1 << 4), DISPC_CTRL);
	}
	dispc_irq_trick(dev);
#ifndef CONFIG_FB_SCX15
	dsi_irq_trick();
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
	}
}

static int32_t sprdfb_dispc_uninit(struct sprdfb_device *dev)
{
	pr_info("%s +\n", __func__);

	dev->enable = 0;
	sprdfb_dispc_clk_disable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_FORCE);

	return 0;
}

static int32_t dispc_clk_init(struct sprdfb_device *dev)
{
	int ret = 0;
	u32 dpi_clk;
	struct clk *clk_parent1, *clk_parent2, *clk_parent3, *clk_parent4;
#ifdef CONFIG_OF
	uint32_t clk_src[DISPC_CLOCK_NUM];
	uint32_t def_dpi_clk_div = 1;
#endif

	pr_info("%s +\n", __func__);

	sci_glb_set(DISPC_CORE_EN, BIT_DISPC_CORE_EN);

#ifdef CONFIG_OF
	ret = of_property_read_u32_array(dev->of_dev->of_node,
			"clock-src", clk_src, 3);
	if (ret) {
		pr_err("%s, read clock-src failed (%d)\n", __func__, ret);
		return -1;
	}

	ret = of_property_read_u32(dev->of_dev->of_node, "dpi_clk_div",
			&def_dpi_clk_div);
	if (ret) {
		pr_err("%s, read dpi_clk_div failed (%d)\n", __func__, ret);
		return -1;
	}
	clk_parent1 = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_CLOCK_PARENT);
	dpi_clk = clk_src[DISPC_DPI_CLOCK_SRC_ID]/def_dpi_clk_div;
#else
	dpi_clk = DISPC_DPI_CLOCK;
	clk_parent1 = clk_get(NULL, DISPC_CLOCK_PARENT);
#endif
	if (IS_ERR(clk_parent1)) {
		pr_err("%s, failed to get parent1 clock (%s)\n",
				__func__, DISPC_CLOCK_PARENT);
		return -1;
	}
	pr_debug("%s, get parent1 clock (%s)\n",
			__func__, DISPC_CLOCK_PARENT);

#ifdef CONFIG_OF
	clk_parent2 = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_DBI_CLOCK_PARENT);
#else
	clk_parent2 = clk_get(NULL, DISPC_DBI_CLOCK_PARENT);
#endif
	if (IS_ERR(clk_parent2)) {
		pr_err("%s, failed to get parent2 clock (%s)\n",
				__func__, DISPC_DBI_CLOCK_PARENT);
		return -1;
	}
	pr_debug("%s, get parent2 clock (%s)\n",
			__func__, DISPC_DBI_CLOCK_PARENT);

#ifdef CONFIG_OF
	clk_parent3 = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_DPI_CLOCK_PARENT);
#else
	clk_parent3 = clk_get(NULL, DISPC_DPI_CLOCK_PARENT);
#endif
	if (IS_ERR(clk_parent3)) {
		pr_err("%s, failed to get parent3 clock (%s)\n",
				__func__, DISPC_DPI_CLOCK_PARENT);
		return -1;
	}
	pr_debug("%s, get parent3 clock (%s)\n",
			__func__, DISPC_DPI_CLOCK_PARENT);

#ifdef CONFIG_OF
	clk_parent4 = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_EMC_EN_PARENT);
#else
	clk_parent4 = clk_get(NULL, DISPC_EMC_EN_PARENT);
#endif
	if (IS_ERR(clk_parent4)) {
		pr_err("%s, failed to get parent4 clock (%s)\n",
				__func__, DISPC_EMC_EN_PARENT);
		return -1;
	}
	pr_debug("%s, get parent4 clock (%s)\n",
			__func__, DISPC_EMC_EN_PARENT);

#ifdef CONFIG_OF
	dispc_ctx.clk_dispc = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_PLL_CLK);
#else
	dispc_ctx.clk_dispc = clk_get(NULL, DISPC_PLL_CLK);
#endif
	if (IS_ERR(dispc_ctx.clk_dispc)) {
		pr_err("%s, failed to get pll clock (%s)\n",
				__func__, DISPC_PLL_CLK);
		return -1;
	}
	pr_debug("%s, get pll clock (%s)\n", __func__, DISPC_PLL_CLK);

#ifdef CONFIG_OF
	dispc_ctx.clk_dispc_dbi = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_DBI_CLK);
#else
	dispc_ctx.clk_dispc_dbi = clk_get(NULL, DISPC_DBI_CLK);
#endif
	if (IS_ERR(dispc_ctx.clk_dispc_dbi)) {
		pr_err("%s, failed to get dbi clock (%s)\n",
				__func__, DISPC_DBI_CLK);
		return -1;
	}
	pr_debug("%s, get dbi clock (%s)\n", __func__, DISPC_DBI_CLK);

#ifdef CONFIG_OF
	dispc_ctx.clk_dispc_dpi = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_DPI_CLK);
#else
	dispc_ctx.clk_dispc_dpi = clk_get(NULL, DISPC_DPI_CLK);
#endif
	if (IS_ERR(dispc_ctx.clk_dispc_dpi)) {
		pr_err("%s, failed to get dpi clock (%s)\n",
				__func__, DISPC_DPI_CLK);
		return -1;
	}
	pr_debug("%s, get dpi clock (%s)\n", __func__, DISPC_DPI_CLK);

#ifdef CONFIG_OF
	dispc_ctx.clk_dispc_emc = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_EMC_CLK);
#else
	dispc_ctx.clk_dispc_emc = clk_get(NULL, DISPC_EMC_CLK);
#endif
	if (IS_ERR(dispc_ctx.clk_dispc_emc)) {
		pr_err("%s, failed to get emc clock (%s)\n",
				__func__, DISPC_EMC_CLK);
		return -1;
	}
	pr_debug("%s, get emc clock (%s)\n", __func__, DISPC_EMC_CLK);

	ret = clk_set_parent(dispc_ctx.clk_dispc, clk_parent1);
	if (ret)
		pr_err("%s, failed to set dispc clk parent\n", __func__);
#ifdef CONFIG_OF
	ret = clk_set_rate(dispc_ctx.clk_dispc, clk_src[DISPC_CLOCK_SRC_ID]);
#else
	ret = clk_set_rate(dispc_ctx.clk_dispc, DISPC_CLOCK);
#endif
	if (ret)
		pr_err("%s, failed to set dispc clk rate\n", __func__);

	ret = clk_set_parent(dispc_ctx.clk_dispc_dbi, clk_parent2);
	if (ret)
		pr_err("%s, failed to set dbi clk parent\n", __func__);
#ifdef CONFIG_OF
	ret = clk_set_rate(dispc_ctx.clk_dispc_dbi,
			clk_src[DISPC_DBI_CLOCK_SRC_ID]);
#else
	ret = clk_set_rate(dispc_ctx.clk_dispc_dbi, DISPC_DBI_CLOCK);
#endif
	if (ret)
		pr_err("%s, failed to set dbi clk rate\n", __func__);

	ret = clk_set_parent(dispc_ctx.clk_dispc_dpi, clk_parent3);
	if (ret)
		pr_err("%s, failed to set dpi clk parent\n", __func__);

	ret = clk_set_parent(dispc_ctx.clk_dispc_emc, clk_parent4);
	if (ret)
		pr_err("%s, failed to set emc clk parent\n", __func__);

	if (dev->panel && dev->panel->fps)
		dispc_update_clk(dev, dev->panel->fps, SPRDFB_FORCE_FPS);
	else
		dispc_update_clk(dev, dpi_clk, SPRDFB_FORCE_PCLK);

#ifdef CONFIG_OF
	ret = clk_prepare_enable(dispc_ctx.clk_dispc_emc);
#else
	ret = clk_enable(dispc_ctx.clk_dispc_emc);
#endif
	if (ret) {
		pr_err("%s, failed to enable dispc emc clk\n", __func__);
		ret = -1;
	}

	ret = sprdfb_dispc_clk_enable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_FORCE);
	if (ret) {
		pr_err("%s, failed to enable dispc_clk\n", __func__);
		return -1;
	}
	pr_debug("%s, enable dispc_clk\n", __func__);

	dispc_print_clk();

	return 0;
}

#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || defined(CONFIG_SPRD_SCX35_DMC_FREQ)) && (!defined(CONFIG_FB_SCX30G))
struct devfreq_dbs sprd_fb_notify = {
	.level = 0,
	.data = &dispc_ctx,
	.devfreq_notifier = sprdfb_dispc_change_threshold,
};
#endif

#ifdef DISPC_AHB_CLOCK_MCU_SLEEP_FEATURE
static int scxx30_dispc_cpuidle_notify(struct notifier_block *nb,
		unsigned long event, void *dummy)
{
	if (event != SC_CPUIDLE_PREPARE) {
		pr_err("%s, invalid cpuidle event (%lu)\n",
				__func__, event);
		return 0;
	}

	if (!dispc_ctx.vsync_done)
		return NOTIFY_BAD;
#ifdef CONFIG_FB_VSYNC_SUPPORT
	if (!dispc_ctx.waitfor_vsync_done)
		return NOTIFY_BAD;
#endif

	return NOTIFY_OK;
}

static struct notifier_block scxx30_dispc_cpuidle_notifier = {
	.notifier_call = scxx30_dispc_cpuidle_notify,
};
#endif

static int32_t sprdfb_dispc_module_init(struct sprdfb_device *dev)
{
	int ret = 0;
	int irq_num = 0;

	if (dispc_ctx.is_inited) {
		pr_warn("%s, dispc_module already initialized\n", __func__);
		return 0;
	}

	pr_info("%s +\n", __func__);

	dispc_ctx.vsync_done = 1;
	dispc_ctx.vsync_waiter = 0;
	init_waitqueue_head(&(dispc_ctx.vsync_queue));

#ifdef CONFIG_FB_ESD_SUPPORT
#ifdef FB_CHECK_ESD_BY_TE_SUPPORT
	init_waitqueue_head(&(dev->esd_te_queue));
	dev->esd_te_waiter = 0;
	dev->esd_te_done = 0;
#endif
#endif

#ifdef CONFIG_FB_VSYNC_SUPPORT
	dispc_ctx.waitfor_vsync_done = 1;
	dispc_ctx.waitfor_vsync_waiter = 0;
	init_waitqueue_head(&(dispc_ctx.waitfor_vsync_queue));
#endif
	sema_init(&dev->refresh_lock, 1);

#ifdef CONFIG_OF
	irq_num = irq_of_parse_and_map(dev->of_dev->of_node, 0);
#else
	irq_num = IRQ_DISPC_INT;
#endif
	pr_info("%s, irq_num : %d\n", __func__, irq_num);

	ret = request_irq(irq_num, dispc_isr, IRQF_DISABLED,
			"DISPC", &dispc_ctx);
	if (ret) {
		pr_err("%s, request irq failed (%d)\n", __func__, ret);
		sprdfb_dispc_uninit(dev);
		return -1;
	}

	dispc_ctx.is_inited = true;

#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || defined(CONFIG_SPRD_SCX35_DMC_FREQ)) && (!defined(CONFIG_FB_SCX30G))
	devfreq_notifier_register(&sprd_fb_notify);
#endif

#ifdef DISPC_AHB_CLOCK_MCU_SLEEP_FEATURE
	ret = register_sc_cpuidle_notifier(&scxx30_dispc_cpuidle_notifier);
	if (ret)
		pr_err("%s, light sleep register notifier failed\n", __func__);
#endif
	return 0;
}

static int32_t sprdfb_dispc_early_init(struct sprdfb_device *dev)
{
	int ret = 0;

	ret = dispc_clk_init(dev);
	if (ret) {
		pr_warn("%s, dispc_clk_init failed (%d)\n", __func__, ret);
		return -1;
	}

	if (!dispc_ctx.is_inited) {
		/* init */
		if (dev->panel_ready) {
			pr_info("%s, dispc already initialized\n", __func__);
			dispc_ctx.is_first_frame = false;
		} else {
			pr_debug("%s, dispc not initialized\n", __func__);
			dispc_reset();
			dispc_module_enable();
			dispc_ctx.is_first_frame = true;
		}
		ret = sprdfb_dispc_module_init(dev);
	} else {
		/* resume */
		pr_info("%s, resume\n", __func__);
		dispc_reset();
		dispc_module_enable();
		dispc_ctx.is_first_frame = true;
	}

	return ret;
}

static int sprdfb_dispc_clk_disable(struct sprdfb_dispc_context *dispc_ctx_ptr,
		SPRDFB_DYNAMIC_CLK_SWITCH_E clock_switch_type)
{
	bool is_need_disable = false;

	pr_debug("%s +\n", __func__);
	if (!dispc_ctx_ptr)
		return 0;

	mutex_lock(&dispc_ctx_lock);
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
		pr_info("%s, disable dispc clk\n", __func__);
#ifdef CONFIG_OF
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc);
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc_dpi);
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc_dbi);
#else
		clk_disable(dispc_ctx_ptr->clk_dispc);
		clk_disable(dispc_ctx_ptr->clk_dispc_dpi);
		clk_disable(dispc_ctx_ptr->clk_dispc_dbi);
#endif
		dispc_ctx_ptr->clk_is_open = false;
		dispc_ctx_ptr->clk_is_refreshing = false;
		dispc_ctx_ptr->clk_open_count = 0;
	}
	mutex_unlock(&dispc_ctx_lock);

	pr_debug("%s, type=%d refresh=%d, count=%d\n",
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

	pr_debug("%s +\n", __func__);
	if (!dispc_ctx_ptr)
		return -1;

	mutex_lock(&dispc_ctx_lock);
	if (!dispc_ctx_ptr->clk_is_open) {
		pr_info("%s, enable dispc clk\n", __func__);
#ifdef CONFIG_OF
		ret = clk_prepare_enable(dispc_ctx_ptr->clk_dispc);
#else
		ret = clk_enable(dispc_ctx_ptr->clk_dispc);
#endif
		if (ret) {
			pr_err("%s, failed to enable dispc_clk (%d)\n",
					__func__, ret);
			ret = -1;
			goto ERROR_CLK_ENABLE;
		}
		is_dispc_enable = true;
		pr_info("%s, dispc clk enabled\n", __func__);
#ifdef CONFIG_OF
		ret = clk_prepare_enable(dispc_ctx_ptr->clk_dispc_dpi);
#else
		ret = clk_enable(dispc_ctx_ptr->clk_dispc_dpi);
#endif
		if (ret) {
			pr_err("%s, failed to enable dpi clk(%d)\n",
					__func__, ret);
			ret = -1;
			goto ERROR_CLK_ENABLE;
		}
		is_dispc_dpi_enable = true;
		pr_info("%s, dpi clk enabled\n", __func__);
#ifdef CONFIG_OF
		ret = clk_prepare_enable(dispc_ctx_ptr->clk_dispc_dbi);
#else
		ret = clk_enable(dispc_ctx_ptr->clk_dispc_dbi);
#endif
		if (ret) {
			pr_err("%s, failed to enable dbi clk(%d)\n",
					__func__, ret);
			ret = -1;
			goto ERROR_CLK_ENABLE;
		}
		dispc_ctx_ptr->clk_is_open = true;
		pr_info("%s, dbi clk enabled\n", __func__);
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
	mutex_unlock(&dispc_ctx_lock);

	pr_debug("%s, type=%d refresh=%d, count=%d, ret=%d\n",
			__func__, clock_switch_type,
			dispc_ctx_ptr->clk_is_refreshing,
			dispc_ctx_ptr->clk_open_count, ret);
	return ret;

ERROR_CLK_ENABLE:
	if (is_dispc_enable) {
#ifdef CONFIG_OF
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc);
#else
		clk_disable(dispc_ctx_ptr->clk_dispc);
#endif
	}
	if (is_dispc_dpi_enable) {
#ifdef CONFIG_OF
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc_dpi);
#else
		clk_disable(dispc_ctx_ptr->clk_dispc_dpi);
#endif
	}
	mutex_unlock(&dispc_ctx_lock);

	pr_err("%s, failed\n", __func__);
	return ret;
}

static int32_t sprdfb_dispc_init(struct sprdfb_device *dev)
{
	uint32_t dispc_int_en_reg_val = 0x00;

	pr_debug("%s +\n", __func__);

	if (!dev) {
		pr_err("%s, no dev\n", __func__);
		return -1;
	}

	dispc_ctx.dev = dev;
	dispc_set_bg_color(0xFFFFFFFF);
	dispc_dithering_enable(true);
	dispc_set_exp_mode(0x0);
	dispc_pwr_enable(true);
#ifdef CONFIG_FB_SCX30G
	dispc_set_threshold(0x960, 0x00, 0x960);
#endif

	if (dispc_ctx.is_first_frame)
		dispc_layer_init(&(dev->fb->var));
	else
		dispc_layer_update(&(dev->fb->var));

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		if (dispc_ctx.is_first_frame) {
			/*set dpi register update only with SW*/
			dispc_set_bits(BIT(4), DISPC_DPI_CTRL);
		} else {
			/*set dpi register update with SW & VSYNC*/
			dispc_clear_bits(BIT(4), DISPC_DPI_CTRL);
		}
		/*enable dispc update done INT*/
		dispc_int_en_reg_val |= DISPC_INT_UPDATE_DONE_MASK;
		/* enable hw vsync */
		dispc_int_en_reg_val |= DISPC_INT_HWVSYNC;
	} else {
		/* enable dispc DONE INT*/
		dispc_int_en_reg_val |= DISPC_INT_DONE_MASK;
	}
	dispc_int_en_reg_val |= DISPC_INT_ERR_MASK;
	dispc_write(dispc_int_en_reg_val, DISPC_INT_EN);
	dev->enable = 1;

	return 0;
}

static void sprdfb_dispc_clean_lcd(struct sprdfb_device *dev)
{
	struct fb_info *fb = dev->fb;
	struct fb_var_screeninfo *var = &fb->var;
	uint32_t base = fb->fix.smem_start +
		(fb->fix.line_length * var->yoffset);
	uint32_t size = (var->xres & 0xFFFF) | ((var->yres & 0xFFFF) << 16);

	pr_info("%s +\n", __func__);

	down(&dev->refresh_lock);
	if (!dispc_ctx.is_first_frame) {
		pr_warn("%s, not first frame\n", __func__);
		up(&dev->refresh_lock);
		return;
	}

	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
		sprdfb_panel_invalidate(dev->panel);

	dispc_write(size, DISPC_SIZE_XY);
	dispc_write(base, DISPC_OSD_BASE_ADDR);

	dispc_set_bg_color(0x00);	/* set bg color black */
	dispc_set_osd_alpha(0x00);	/* make osd transparent */

#if defined(CONFIG_FB_SCX30G)
	/* To set DISPC IMG ready state
	 * IMG Layer need to be enable once before display.
	 * If not, underflow occurs
	 */
	dispc_write(0x1, DISPC_IMG_CTRL);
	dispc_write(base, DISPC_IMG_Y_BASE_ADDR);
	dispc_write(base, DISPC_IMG_UV_BASE_ADDR);
	dispc_write(var->xres | (0x1 << 16), DISPC_IMG_SIZE_XY);
	dispc_write(var->xres, DISPC_IMG_PITCH);
	dispc_write(0x0, DISPC_IMG_DISP_XY);
#endif	/* CONFIG_FB_SCX30G */
	dispc_run(dev);
	dispc_sync(dev);
#if defined(CONFIG_FB_SCX30G)
	dispc_write(0, DISPC_IMG_CTRL);
	dispc_write(0, DISPC_IMG_Y_BASE_ADDR);
	dispc_write(0, DISPC_IMG_UV_BASE_ADDR);
	dispc_run(dev);
	dispc_sync(dev);
#endif	/* CONFIG_FB_SCX30G */
	up(&dev->refresh_lock);
	msleep(33);
}

static int32_t sprdfb_dispc_refresh(struct sprdfb_device *dev)
{
	uint32_t reg_val = 0;
	struct fb_info *fb = dev->fb;
	uint32_t base = fb->fix.smem_start +
		(fb->fix.line_length * fb->var.yoffset);
	uint32_t size = (fb->var.xres & 0xffff) | (fb->var.yres << 16);

	down(&dev->refresh_lock);
	if (!dev->enable) {
		pr_err("%s, dev not enabled\n", __func__);
		goto ERROR_REFRESH;
	}

	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
		dispc_ctx.vsync_waiter++;
		dispc_sync(dev);
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if (sprdfb_dispc_clk_enable(&dispc_ctx,
					SPRDFB_DYNAMIC_CLK_REFRESH)) {
			pr_warn("%s, enable dispc_clk failed\n", __func__);
			goto ERROR_REFRESH;
		}
#endif
	} else {
#ifdef CONFIG_FB_TRIPLE_FRAMEBUFFER
		if ((dispc_read(DISPC_OSD_BASE_ADDR) !=
					dispc_read(SHDW_OSD_BASE_ADDR))
				&& dispc_read(SHDW_OSD_BASE_ADDR)) {
			dispc_ctx.vsync_waiter++;
			dispc_sync(dev);
		}
#endif
	}
	pr_debug("%s, got sync\n", __func__);
#ifdef CONFIG_FB_MMAP_CACHED
	if (dispc_ctx.vma) {
		pr_debug("%s, dispc_ctx.vma=0x%x\n", dispc_ctx.vma);
		dma_sync_single_for_device(dev->of_dev,
				dev->fb->fix.smem_start,
				dev->fb->fix.smem_len, DMA_TO_DEVICE);
	}
	if (fb->var.reserved[3] == 1) {
		dispc_dithering_enable(false);
		if (dev->panel->ops->panel_change_epf)
			dev->panel->ops->panel_change_epf(dev->panel, false);
	} else {
		dispc_dithering_enable(true);
		if (dev->panel->ops->panel_change_epf)
			dev->panel->ops->panel_change_epf(dev->panel, true);
	}
#endif
	dispc_set_osd_alpha(0xff);

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	if (SPRD_OVERLAY_STATUS_STARTED == dispc_ctx.overlay_state)
		overlay_close(dev);
#endif
	dispc_write(base, DISPC_OSD_BASE_ADDR);
	dispc_write(0, DISPC_OSD_DISP_XY);
	dispc_write(size, DISPC_OSD_SIZE_XY);
	dispc_write(fb->var.xres, DISPC_OSD_PITCH);
#ifdef CONFIG_FB_LOW_RES_SIMU
	size = (dev->panel->width & 0xffff) |
		(dev->panel->height << 16);
#endif
	dispc_write(size, DISPC_SIZE_XY);

#ifdef BIT_PER_PIXEL_SURPPORT
	if (fb->var.bits_per_pixel == 32) {
		reg_val |= (3 << 4);	/* ABGR */
		reg_val |= (1 << 15);	/* rb switch */
		dispc_clear_bits(0x30000, DISPC_CTRL);
	} else {
		reg_val |= (5 << 4);	/* RGB565 */
		reg_val |= (2 << 8);	/* B2B3B0B1 */
		dispc_clear_bits(0x30000, DISPC_CTRL);
		dispc_set_bits(0x10000, DISPC_CTRL);
	}
	reg_val |= (1 << 0);
	reg_val |= (1 << 2);	/* alpha mode select - block alpha*/
	dispc_write(reg_val, DISPC_OSD_CTRL);
#endif
	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
		sprdfb_panel_invalidate(dev->panel);

	sprdfb_panel_before_refresh(dev);

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	dispc_set_bits(BIT(0), DISPC_OSD_CTRL);
	if (SPRD_OVERLAY_STATUS_ON == dispc_ctx.overlay_state)
		overlay_start(dev, SPRD_LAYER_IMG);
#endif
	dispc_run(dev);

#ifdef CONFIG_FB_ESD_SUPPORT
	if (!dev->ESD_work_start) {
		pr_info("%s, schedule esd work\n", __func__);
		schedule_delayed_work(&dev->ESD_work,
				msecs_to_jiffies(dev->ESD_timeout_val));
		dev->ESD_work_start = true;
	}
#endif

ERROR_REFRESH:
	up(&dev->refresh_lock);
	if (dev->logo_buffer_addr_v) {
		pr_info("%s, free logo proc buffer\n", __func__);
		free_pages(dev->logo_buffer_addr_v,
				get_order(dev->logo_buffer_size));
		dev->logo_buffer_addr_v = 0;
	}

	pr_debug("sprdfb: DISPC_CTRL: 0x%x\n",
			dispc_read(DISPC_CTRL));
	pr_debug("sprdfb: DISPC_SIZE_XY: 0x%x\n",
			dispc_read(DISPC_SIZE_XY));
	pr_debug("sprdfb: DISPC_BG_COLOR: 0x%x\n",
			dispc_read(DISPC_BG_COLOR));
	pr_debug("sprdfb: DISPC_INT_EN: 0x%x\n",
			dispc_read(DISPC_INT_EN));
	pr_debug("sprdfb: DISPC_OSD_CTRL: 0x%x\n",
			dispc_read(DISPC_OSD_CTRL));
	pr_debug("sprdfb: DISPC_OSD_BASE_ADDR: 0x%x\n",
			dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("sprdfb: DISPC_OSD_SIZE_XY: 0x%x\n",
			dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("sprdfb: DISPC_OSD_PITCH: 0x%x\n",
			dispc_read(DISPC_OSD_PITCH));
	pr_debug("sprdfb: DISPC_OSD_DISP_XY: 0x%x\n",
			dispc_read(DISPC_OSD_DISP_XY));
	pr_debug("sprdfb: DISPC_OSD_ALPHA: 0x%x\n",
			dispc_read(DISPC_OSD_ALPHA));
	return 0;
}

static int32_t sprdfb_dispc_suspend(struct sprdfb_device *dev)
{
	pr_info("%s +\n", __func__);

	mutex_lock(&dispc_lock);
	if (!dev->enable) {
		pr_warn("%s, dev already suspended\n", __func__);
		mutex_unlock(&dispc_lock);
		return 0;
	}

	down(&dev->refresh_lock);
#ifdef CONFIG_FB_ESD_SUPPORT
	if (dev->ESD_work_start) {
		pr_info("%s, cancel esd work\n", __func__);
		cancel_delayed_work_sync(&dev->ESD_work);
		dev->ESD_work_start = false;
	}
#endif
	if (dev->panel->ops->panel_set_brightness)
		dev->panel->ops->panel_set_brightness(dev->panel, false);

	sprdfb_panel_suspend(dev);

	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
		/* must wait ,dispc_sync() */
		dispc_ctx.vsync_waiter++;
		dispc_sync(dev);
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		pr_info("%s, open clk in suspend\n", __func__);
		if (sprdfb_dispc_clk_enable(&dispc_ctx,
					SPRDFB_DYNAMIC_CLK_COUNT))
			pr_warn("%s, enable dispc_clk failed\n", __func__);
#endif
		pr_info("%s, got sync\n", __func__);
	}

	dev->enable = 0;
	up(&dev->refresh_lock);
	dispc_stop(dev);
	dispc_write(0, DISPC_INT_EN);
	msleep(50); /*fps>20*/
#ifdef CONFIG_OF
	clk_disable_unprepare(dispc_ctx.clk_dispc_emc);
#else
	clk_disable(dispc_ctx.clk_dispc_emc);
#endif
	sprdfb_dispc_clk_disable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_FORCE);
	pr_info("%s -\n", __func__);
	mutex_unlock(&dispc_lock);

	return 0;
}

static int32_t sprdfb_dispc_resume(struct sprdfb_device *dev)
{
	mutex_lock(&dispc_lock);
	pr_info("%s +\n", __func__);

	if (dev->enable) {
		pr_warn("sprdfb: [%s] dispc already enabled\n", __func__);
		mutex_unlock(&dispc_lock);
		return 0;
	}

	if (sprdfb_dispc_clk_enable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_FORCE))
		pr_warn("%s, enable dispc_clk failed\n", __func__);

	dispc_ctx.vsync_done = 1;
	sprdfb_dispc_early_init(dev);
	sprdfb_panel_resume(dev, true);
	sprdfb_dispc_init(dev);

	dev->enable = 1;

#ifdef CONFIG_FB_SCX30G
	sprdfb_dispc_clean_lcd(dev);
#else
	if (dev->panel->is_clean_lcd)
		sprdfb_dispc_clean_lcd(dev);
#endif
	sprdfb_panel_start(dev);

	pr_info("%s -\n", __func__);
	mutex_unlock(&dispc_lock);

	return 0;
}

#ifdef CONFIG_FB_ESD_SUPPORT
/* for video esd check */
static int32_t sprdfb_dispc_check_esd_dpi(struct sprdfb_device *dev)
{
	uint32_t ret = 0;
	unsigned long flags;

#ifdef FB_CHECK_ESD_BY_TE_SUPPORT
	ret = sprdfb_panel_ESD_check(dev);
	if (ret)
		dispc_run_for_feature(dev);
#else
	local_irq_save(flags);
#ifdef FB_CHECK_ESD_IN_VFP
	ret = sprdfb_panel_ESD_check(dev);
#else
	dispc_stop_for_feature(dev);

	ret = sprdfb_panel_ESD_check(dev);

	dispc_run_for_feature(dev);
#endif
	local_irq_restore(flags);
#endif

	return ret;
}

/* for cmd esd check */
static int32_t sprdfb_dispc_check_esd_edpi(struct sprdfb_device *dev)
{
	uint32_t ret = 0;

	dispc_ctx.vsync_waiter++;
	dispc_sync(dev);
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
	if (sprdfb_dispc_clk_enable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_COUNT)) {
		pr_warn("%s, enable dispc_clk failed\n", __func__);
		return -1;
	}
#endif

	ret = sprdfb_panel_ESD_check(dev);
	if (ret)
		dispc_run_for_feature(dev);

#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
	sprdfb_dispc_clk_disable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_COUNT);
#endif

	return ret;
}

static int32_t sprdfb_dispc_check_esd(struct sprdfb_device *dev)
{
	uint32_t ret = 0;
	bool	is_refresh_lock_down = false;

	pr_debug("%s +\n", __func__);

	if (SPRDFB_PANEL_IF_DBI == dev->panel_if_type) {
		pr_err("%s, unsupported in dbi\n", __func__);
		ret = -1;
		goto ERROR_CHECK_ESD;
	}
	down(&dev->refresh_lock);
	is_refresh_lock_down = true;
	if (!dev->enable) {
		pr_err("%s, dev not enabled\n", __func__);
		ret = -1;
		goto ERROR_CHECK_ESD;
	}

	if (!(dev->check_esd_time % 30))
		pr_info("%s, %d, %d, %d\n", __func__, dev->check_esd_time,
				dev->panel_reset_time, dev->reset_dsi_time);
	else
		pr_debug("%s, %d, %d, %d\n", __func__, dev->check_esd_time,
				dev->panel_reset_time, dev->reset_dsi_time);

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type)
		ret = sprdfb_dispc_check_esd_dpi(dev);
	else
		ret = sprdfb_dispc_check_esd_edpi(dev);

ERROR_CHECK_ESD:
	if (is_refresh_lock_down)
		up(&dev->refresh_lock);

	return ret;
}
#endif

#ifdef CONFIG_LCD_ESD_RECOVERY
static int32_t sprdfb_dispc_esd_reset(struct sprdfb_device *dev)
{
	mutex_lock(&dispc_lock);
	pr_info("%s\n", __func__);

	if (!dev->enable) {
		pr_warn("%s, dispc not enabled\n", __func__);
		mutex_unlock(&dispc_lock);
		return;
	}
	sprdfb_panel_ESD_reset(dev);
	sprdfb_dispc_refresh(dev);
	sprdfb_panel_start(dev);
	mutex_unlock(&dispc_lock);

	return 0;
}
#endif

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
static int overlay_open(void)
{
	pr_debug("%s, state : %d\n", __func__, dispc_ctx.overlay_state);

	dispc_ctx.overlay_state = SPRD_OVERLAY_STATUS_ON;
	return 0;
}

static int overlay_start(struct sprdfb_device *dev, uint32_t layer_index)
{
	pr_debug("%s, %d, %d\n", __func__,
			dispc_ctx.overlay_state, layer_index);

	if (SPRD_OVERLAY_STATUS_ON != dispc_ctx.overlay_state) {
		pr_err("sprdfb: %s overlay is not opened\n", __func__);
		return -1;
	}

	if ((!dispc_read(DISPC_IMG_Y_BASE_ADDR)) &&
			(!dispc_read(DISPC_OSD_BASE_ADDR))) {
		pr_err("sprdfb: %s overlay is not configured\n", __func__);
		return -1;
	}

	dispc_set_bg_color(0x0);
	dispc_clear_bits(BIT(2), DISPC_OSD_CTRL); /*use pixel alpha*/
	dispc_set_osd_alpha(0x80);

	if ((layer_index & SPRD_LAYER_IMG) &&
			(dispc_read(DISPC_IMG_Y_BASE_ADDR)))
		/* enable the image layer */
		dispc_set_bits(BIT(0), DISPC_IMG_CTRL);

	if ((layer_index & SPRD_LAYER_OSD) &&
			(dispc_read(DISPC_OSD_BASE_ADDR)))
		/* enable the osd layer */
		dispc_set_bits(BIT(0), DISPC_OSD_CTRL);

	dispc_ctx.overlay_state = SPRD_OVERLAY_STATUS_STARTED;
	return 0;
}

static int overlay_img_configure(struct sprdfb_device *dev,
		int type, overlay_rect *rect, unsigned char *buffer,
		int y_endian, int uv_endian, bool rb_switch)
{
	uint32_t reg_value;

	pr_debug("%s, %d, (%d, %d,%d,%d), 0x%x\n", __func__,
			type, rect->x, rect->y, rect->h, rect->w,
			(unsigned int)buffer);

	if (SPRD_OVERLAY_STATUS_ON != dispc_ctx.overlay_state) {
		pr_err("%s, error : overlay off\n", __func__);
		return -1;
	}

	if (type >= SPRD_DATA_TYPE_LIMIT) {
		pr_err("%s error : invalid type(%d)\n",
				__func__, type);
		return -1;
	}

	if ((y_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT) ||
			(uv_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT)) {
		pr_err("%s, error : invalid y uv endian\n", __func__);
		return -1;
	}

	reg_value = (y_endian << 8) | (uv_endian << 10) | (type << 4);
	if (rb_switch)
		reg_value |= (1 << 15);
	dispc_write(reg_value, DISPC_IMG_CTRL);

	dispc_write((uint32_t)buffer, DISPC_IMG_Y_BASE_ADDR);
	if (type < SPRD_DATA_TYPE_RGB888)
		dispc_write((uint32_t)(buffer + rect->w * rect->h),
				DISPC_IMG_UV_BASE_ADDR);

	reg_value = (rect->h << 16) | (rect->w);
	dispc_write(reg_value, DISPC_IMG_SIZE_XY);

	dispc_write(rect->w, DISPC_IMG_PITCH);

	reg_value = (rect->y << 16) | (rect->x);
	dispc_write(reg_value, DISPC_IMG_DISP_XY);

	if (type < SPRD_DATA_TYPE_RGB888) {
		dispc_write(1, DISPC_Y2R_CTRL);
#if (defined CONFIG_FB_SCX15) || (defined CONFIG_FB_SCX30G)
		dispc_write(SPRDFB_BRIGHTNESS | SPRDFB_CONTRAST,
				DISPC_Y2R_Y_PARAM);
		dispc_write(SPRDFB_OFFSET_U | SPRDFB_SATURATION_U,
				DISPC_Y2R_U_PARAM);
		dispc_write(SPRDFB_OFFSET_V | SPRDFB_SATURATION_V,
				DISPC_Y2R_V_PARAM);
#else
		dispc_write(SPRDFB_CONTRAST, DISPC_Y2R_CONTRAST);
		dispc_write(SPRDFB_SATURATION, DISPC_Y2R_SATURATION);
		dispc_write(SPRDFB_BRIGHTNESS, DISPC_Y2R_BRIGHTNESS);
#endif
	}

	pr_debug("sprdfb: DISPC_IMG_CTRL: 0x%x\n",
			dispc_read(DISPC_IMG_CTRL));
	pr_debug("sprdfb: DISPC_IMG_Y_BASE_ADDR: 0x%x\n",
			dispc_read(DISPC_IMG_Y_BASE_ADDR));
	pr_debug("sprdfb: DISPC_IMG_UV_BASE_ADDR: 0x%x\n",
			dispc_read(DISPC_IMG_UV_BASE_ADDR));
	pr_debug("sprdfb: DISPC_IMG_SIZE_XY: 0x%x\n",
			dispc_read(DISPC_IMG_SIZE_XY));
	pr_debug("sprdfb: DISPC_IMG_PITCH: 0x%x\n",
			dispc_read(DISPC_IMG_PITCH));
	pr_debug("sprdfb: DISPC_IMG_DISP_XY: 0x%x\n",
			dispc_read(DISPC_IMG_DISP_XY));
	pr_debug("sprdfb: DISPC_Y2R_CTRL: 0x%x\n",
			dispc_read(DISPC_Y2R_CTRL));
#if (defined CONFIG_FB_SCX15) || (defined CONFIG_FB_SCX30G)
	pr_debug("sprdfb: DISPC_Y2R_Y_PARAM: 0x%x\n",
			dispc_read(DISPC_Y2R_Y_PARAM));
	pr_debug("sprdfb: DISPC_Y2R_U_PARAM: 0x%x\n",
			dispc_read(DISPC_Y2R_U_PARAM));
	pr_debug("sprdfb: DISPC_Y2R_V_PARAM: 0x%x\n",
			dispc_read(DISPC_Y2R_V_PARAM));
#else
	pr_debug("sprdfb: DISPC_Y2R_CONTRAST: 0x%x\n",
			dispc_read(DISPC_Y2R_CONTRAST));
	pr_debug("sprdfb: DISPC_Y2R_SATURATION: 0x%x\n",
			dispc_read(DISPC_Y2R_SATURATION));
	pr_debug("sprdfb: DISPC_Y2R_BRIGHTNESS: 0x%x\n",
			dispc_read(DISPC_Y2R_BRIGHTNESS));
#endif
	return 0;
}

static int overlay_osd_configure(struct sprdfb_device *dev, int type,
		overlay_rect *rect, unsigned char *buffer,
		int y_endian, int uv_endian, bool rb_switch)
{
	uint32_t reg_value;

	pr_debug("%s, %d, (%d, %d,%d,%d), 0x%x\n", __func__,
			type, rect->x, rect->y, rect->h, rect->w,
			(unsigned int)buffer);

	if (SPRD_OVERLAY_STATUS_ON != dispc_ctx.overlay_state) {
		pr_err("%s, overlay not opened!! (%d)\n",
				__func__, dispc_ctx.overlay_state);
		return -1;
	}

	if ((type >= SPRD_DATA_TYPE_LIMIT) || (type <= SPRD_DATA_TYPE_YUV400)) {
		pr_err("%s, invalid type (%d)\n", __func__, type);
		return -1;
	}

	if (y_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT) {
		pr_err("%s, invalid y_endian (%d)\n", __func__, y_endian);
		return -1;
	}

	/*use premultiply pixel alpha*/
	reg_value = (y_endian << 8) | (type << 4 | (1 << 2)) | (2 << 16);
	if (rb_switch)
		reg_value |= (1 << 15);

	dispc_write(reg_value, DISPC_OSD_CTRL);

	dispc_write((uint32_t)buffer, DISPC_OSD_BASE_ADDR);

	reg_value = (rect->h << 16) | (rect->w);
	dispc_write(reg_value, DISPC_OSD_SIZE_XY);

	dispc_write(rect->w, DISPC_OSD_PITCH);

	reg_value = (rect->y << 16) | (rect->x);
	dispc_write(reg_value, DISPC_OSD_DISP_XY);

	pr_debug("sprdfb: DISPC_OSD_CTRL: 0x%x\n",
			dispc_read(DISPC_OSD_CTRL));
	pr_debug("sprdfb: DISPC_OSD_BASE_ADDR: 0x%x\n",
			dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("sprdfb: DISPC_OSD_SIZE_XY: 0x%x\n",
			dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("sprdfb: DISPC_OSD_PITCH: 0x%x\n",
			dispc_read(DISPC_OSD_PITCH));
	pr_debug("sprdfb: DISPC_OSD_DISP_XY: 0x%x\n",
			dispc_read(DISPC_OSD_DISP_XY));

	return 0;
}

static int overlay_close(struct sprdfb_device *dev)
{
	if (SPRD_OVERLAY_STATUS_OFF == dispc_ctx.overlay_state) {
		pr_warn("%s, overlay already closed\n", __func__);
		return 0;
	}

	dispc_set_bg_color(0xFFFFFFFF);

	/*use block alpha*/
	dispc_set_bits(BIT(2), DISPC_OSD_CTRL);
	dispc_set_osd_alpha(0xff);

	/* disable the image layer */
	dispc_clear_bits(BIT(0), DISPC_IMG_CTRL);
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
	bool is_clk_enable = false;

	pr_debug("%s +\n", __func__);

	if (enable) {
		if (!info) {
			pr_err("%s, invalid parameter\n", __func__);
			goto ERROR_ENABLE_OVERLAY;
		}
		down(&dev->refresh_lock);
		is_refresh_lock_down = true;
		if (!dev->enable) {
			pr_err("%s, dispc inactive state\n", __func__);
			goto ERROR_ENABLE_OVERLAY;
		}

		if (dispc_sync(dev)) {
			pr_err("%s, dispc_sync timeout\n", __func__);
			goto ERROR_ENABLE_OVERLAY;
		}
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
			if (sprdfb_dispc_clk_enable(&dispc_ctx,
						SPRDFB_DYNAMIC_CLK_COUNT)) {
				pr_warn("%s, clk enable failed\n", __func__);
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
		if (result) {
			result = -1;
			goto ERROR_ENABLE_OVERLAY;
		}

		if (SPRD_LAYER_IMG == info->layer_index) {
			result = overlay_img_configure(dev, info->data_type,
					&(info->rect), info->buffer,
					info->y_endian, info->uv_endian,
					info->rb_switch);
		} else if (SPRD_LAYER_OSD == info->layer_index) {
			result = overlay_osd_configure(dev, info->data_type,
					&(info->rect), info->buffer,
					info->y_endian, info->uv_endian,
					info->rb_switch);
		} else {
			pr_err("%s, invalid layer(%d)\n",
					__func__, info->layer_index);
		}

		if (result) {
			result = -1;
			goto ERROR_ENABLE_OVERLAY;
		}
		/*result = overlay_start(dev);*/
	} else {
		/*disable*/
		/*result = overlay_close(dev);*/
	}
ERROR_ENABLE_OVERLAY:
	if (is_clk_enable)
		sprdfb_dispc_clk_disable(&dispc_ctx, SPRDFB_DYNAMIC_CLK_COUNT);
	if (is_refresh_lock_down)
		up(&dev->refresh_lock);

	pr_debug("%s, - ret %d\n", __func__, result);

	return result;
}


static int32_t sprdfb_dispc_display_overlay(struct sprdfb_device *dev,
		struct overlay_display *setting)
{
	struct overlay_rect *rect = &(setting->rect);
#ifndef CONFIG_FB_LOW_RES_SIMU
	uint32_t size = ((rect->h << 16) | (rect->w & 0xffff));
#else
	uint32_t size = (dev->panel->width & 0xffff) |
		((dev->panel->height)<<16);
#endif
	dispc_ctx.dev = dev;

	pr_debug("%s, layer:%d, (%d, %d,%d,%d)\n", __func__,
		setting->layer_index, setting->rect.x, setting->rect.y,
		setting->rect.h, setting->rect.w);

	down(&dev->refresh_lock);
	if (!dev->enable) {
		pr_err("%s, dev not enabled\n", __func__);
		goto ERROR_DISPLAY_OVERLAY;
	}
	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
		dispc_ctx.vsync_waiter++;
		dispc_sync(dev);
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if (sprdfb_dispc_clk_enable(&dispc_ctx,
					SPRDFB_DYNAMIC_CLK_REFRESH)) {
			pr_warn("%s clk enable failed\n", __func__);
			goto ERROR_DISPLAY_OVERLAY;
		}
#endif
	}
#ifdef CONFIG_FB_MMAP_CACHED
	dispc_dithering_enable(true);
	if (dev->panel->ops->panel_change_epf)
		dev->panel->ops->panel_change_epf(dev->panel, true);

#endif
	pr_debug("%s, got sync\n", __func__);

	dispc_ctx.dev = dev;
	dispc_write(size, DISPC_SIZE_XY);

	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
		sprdfb_panel_invalidate(dev->panel);

	sprdfb_panel_before_refresh(dev);

	dispc_clear_bits(BIT(0), DISPC_OSD_CTRL);
	if (SPRD_OVERLAY_STATUS_ON == dispc_ctx.overlay_state) {
		if (overlay_start(dev, setting->layer_index)) {
			pr_err("%s overlay start failed\n", __func__);
			goto ERROR_DISPLAY_OVERLAY;
		}
	}

	dispc_run(dev);

	if ((SPRD_OVERLAY_DISPLAY_SYNC == setting->display_mode) &&
			(SPRDFB_PANEL_IF_DPI != dev->panel_if_type)) {
		dispc_ctx.vsync_waiter++;
		if (dispc_sync(dev) != 0) {
			/* time out??? disable ?? */
			pr_info("%s, timeout\n", __func__);
		}
	}

ERROR_DISPLAY_OVERLAY:
	up(&dev->refresh_lock);

	pr_debug("sprdfb: DISPC_CTRL: 0x%x\n",
			dispc_read(DISPC_CTRL));
	pr_debug("sprdfb: DISPC_SIZE_XY: 0x%x\n",
			dispc_read(DISPC_SIZE_XY));
	pr_debug("sprdfb: DISPC_BG_COLOR: 0x%x\n",
			dispc_read(DISPC_BG_COLOR));
	pr_debug("sprdfb: DISPC_INT_EN: 0x%x\n",
			dispc_read(DISPC_INT_EN));
	pr_debug("sprdfb: DISPC_OSD_CTRL: 0x%x\n",
			dispc_read(DISPC_OSD_CTRL));
	pr_debug("sprdfb: DISPC_OSD_BASE_ADDR: 0x%x\n",
			dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("sprdfb: DISPC_OSD_SIZE_XY: 0x%x\n",
			dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("sprdfb: DISPC_OSD_PITCH: 0x%x\n",
			dispc_read(DISPC_OSD_PITCH));
	pr_debug("sprdfb: DISPC_OSD_DISP_XY: 0x%x\n",
			dispc_read(DISPC_OSD_DISP_XY));
	pr_debug("sprdfb: DISPC_OSD_ALPHA: 0x%x\n",
			dispc_read(DISPC_OSD_ALPHA));
	return 0;
}

#endif

#ifdef CONFIG_FB_VSYNC_SUPPORT
static int32_t spdfb_dispc_wait_for_vsync(struct sprdfb_device *dev)
{
	int ret = 0;
	uint32_t reg_val0, reg_val1;

	pr_debug("%s +\n", __func__);

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		if (!dispc_ctx.is_first_frame) {
			dispc_ctx.waitfor_vsync_done = 0;
			dispc_ctx.waitfor_vsync_waiter++;
			ret = wait_event_interruptible_timeout(
					dispc_ctx.waitfor_vsync_queue,
					dispc_ctx.waitfor_vsync_done,
					msecs_to_jiffies(VSYNC_TIMEOUT_MS));

			if (!ret) { /* time out */
				reg_val0 = dispc_read(DISPC_INT_RAW);
				reg_val1 = dispc_read(DISPC_DPI_STS1);
				pr_err("%s, vsync timeout (0x%x, 0x%x)\n",
						__func__, reg_val0, reg_val1);
				dump_dispc_reg();
			}
			dispc_ctx.waitfor_vsync_waiter = 0;
		} else {
			msleep(16);
		}
	} else {
		dispc_ctx.waitfor_vsync_done = 0;
		dispc_set_bits(BIT(1), DISPC_INT_EN);
		dispc_ctx.waitfor_vsync_waiter++;
		ret = wait_event_interruptible_timeout(
				dispc_ctx.waitfor_vsync_queue,
				dispc_ctx.waitfor_vsync_done,
				msecs_to_jiffies(VSYNC_TIMEOUT_MS));

		if (!ret) { /* time out */
			pr_err("%s, vsync timeout\n", __func__);
			dump_dispc_reg();
		}
		dispc_ctx.waitfor_vsync_waiter = 0;
	}
	pr_debug("%s - (%d)\n", __func__, ret);
	return 0;
}
#endif

static void dispc_stop_for_feature(struct sprdfb_device *dev)
{
	int i = 0;

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		dispc_stop(dev);
		while (dispc_read(DISPC_DPI_STS1) & BIT(16)) {
			if (!(++i % 500000))
				pr_warn("%s, busy waiting stop\n", __func__);
		}
		udelay(25);
	}
}

static void dispc_run_for_feature(struct sprdfb_device *dev)
{
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	if (SPRD_OVERLAY_STATUS_ON == dispc_ctx.overlay_state)
		return;
#endif
	dispc_run(dev);
}

#ifdef CONFIG_FB_DYNAMIC_FREQ_SCALING
/**
 * author: Yang.Haibing haibing.yang@spreadtrum.com
 *
 * sprdfb_dispc_chg_clk - interface for sysfs to change dpi or dphy clock
 * @fb_dev: spreadtrum specific fb device
 * @type: check the type is fps or pclk or mipi dphy freq
 * @new_val: fps or new dpi clock
 *
 * Returns 0 on success, MINUS on parsing error.
 */
int sprdfb_dispc_chg_clk(struct sprdfb_device *fb_dev,
				int type, u32 new_val)
{
	int ret = 0;
	unsigned long flags = 0;
	struct panel_spec *panel = fb_dev->panel;

	pr_debug("%s --enter--", __func__);
	/* check if new value is valid */
	if (new_val <= 0) {
			pr_err("new value is invalid\n");
			return -EINVAL;
	}

	down(&fb_dev->refresh_lock);
	/* Now let's do update dpi clock */
	switch (type) {
	case SPRDFB_DYNAMIC_PCLK:
		if (fb_dev->panel_if_type != SPRDFB_PANEL_IF_DPI) {
			pr_err("current dispc interface isn't DPI\n");
			up(&fb_dev->refresh_lock);
			return -EINVAL;
		}
		/* Note: local interrupt will be disabled */
		local_irq_save(flags);
		dispc_stop_for_feature(fb_dev);
		ret = dispc_update_clk(fb_dev, new_val,
				SPRDFB_DYNAMIC_PCLK);
		break;

	case SPRDFB_DYNAMIC_FPS:
		if (fb_dev->panel_if_type != SPRDFB_PANEL_IF_DPI) {
			dispc_ctx.vsync_waiter++;
			dispc_sync(fb_dev);
			sprdfb_panel_change_fps(fb_dev, new_val);
			up(&fb_dev->refresh_lock);
			return ret;
		}
		/* Note: local interrupt will be disabled */
		local_irq_save(flags);
		dispc_stop_for_feature(fb_dev);
		ret = dispc_update_clk(fb_dev, new_val, SPRDFB_DYNAMIC_FPS);
		break;

	case SPRDFB_DYNAMIC_MIPI_CLK:
		/* Note: local interrupt will be disabled */
		local_irq_save(flags);
		dispc_stop_for_feature(fb_dev);
		ret = dispc_update_clk(fb_dev, new_val,
				SPRDFB_DYNAMIC_MIPI_CLK);
		break;

	default:
		ret = -EINVAL;
		break;
	}

	if (ret) {
		pr_err("Failed to set pixel clock, fps or dphy freq.\n");
		goto DONE;
	}

	if (type != SPRDFB_DYNAMIC_MIPI_CLK &&
			panel->type == LCD_MODE_DSI &&
			panel->info.mipi->work_mode ==
			SPRDFB_MIPI_MODE_VIDEO &&
			fb_dev->enable == true) {
		ret = dsi_dpi_init(fb_dev);
		if (ret)
			pr_err("%s: dsi_dpi_init fail\n", __func__);
	}

DONE:
	dispc_run_for_feature(fb_dev);
	if (flags)
		local_irq_restore(flags);
	up(&fb_dev->refresh_lock);

	pr_debug("%s --leave--", __func__);
	return ret;
}
#endif

static int dispc_update_clk_intf(struct sprdfb_device *fb_dev)
{
	return dispc_update_clk(fb_dev, fb_dev->panel->fps, SPRDFB_FORCE_FPS);
}

/**
 * author: Yang.Haibing haibing.yang@spreadtrum.com
 *
 * dispc_update_clk - update dpi clock via @howto and @new_val
 * @fb_dev: spreadtrum specific fb device
 * @new_val: fps or new dpi clock
 * @howto: check the type is fps or pclk or mipi dphy freq
 *
 * Returns 0 on success, MINUS on error.
 */
static int dispc_update_clk(struct sprdfb_device *fb_dev,
					u32 new_val, int howto)
{
	int ret;
	u32 new_pclk;
	u32 dpi_clk_src;
	struct panel_spec *panel = fb_dev->panel;

#ifdef CONFIG_OF
	uint32_t clk_src[DISPC_CLOCK_NUM];

	ret = of_property_read_u32_array(fb_dev->of_dev->of_node,
			"clock-src", clk_src, DISPC_CLOCK_NUM);
	if (ret) {
		pr_err("[%s] read clock-src fail (%d)\n", __func__, ret);
		return -EINVAL;
	}
	dpi_clk_src = clk_src[DISPC_DPI_CLOCK_SRC_ID];
#else
	dpi_clk_src = SPRDFB_DPI_CLOCK_SRC;
#endif

	switch (howto) {
	case SPRDFB_FORCE_FPS:
		ret = dispc_check_new_clk(fb_dev, &new_pclk,
				dpi_clk_src, new_val,
				SPRDFB_FORCE_FPS);
		if (ret) {
			pr_err("%s: new forced fps is invalid.", __func__);
			return -EINVAL;
		}
		break;

	case SPRDFB_DYNAMIC_FPS: /* Calc dpi clock via fps */
		ret = dispc_check_new_clk(fb_dev, &new_pclk,
				dpi_clk_src, new_val,
				SPRDFB_DYNAMIC_FPS);
		if (ret) {
			pr_err("%s: new dynamic fps is invalid.", __func__);
			return -EINVAL;
		}
		break;

	case SPRDFB_FORCE_PCLK:
		new_pclk = new_val;
		break;

	case SPRDFB_DYNAMIC_PCLK: /* Given dpi clock */
		ret = dispc_check_new_clk(fb_dev, &new_pclk,
				dpi_clk_src, new_val,
				SPRDFB_DYNAMIC_PCLK);
		if (ret) {
			pr_err("%s: new dpi clock is invalid.", __func__);
			return -EINVAL;
		}
		break;

	case SPRDFB_DYNAMIC_MIPI_CLK: /* Given mipi clock */
		if (!panel) {
			pr_err("%s: panel is NULL!\n", __func__);
			return -ENODEV;
		}
		if (panel->type != LCD_MODE_DSI) {
			pr_err("%s: panel type isn't dsi mode", __func__);
			return -ENXIO;
		}
		ret = sprdfb_dsi_chg_dphy_freq(fb_dev, new_val);
		if (ret) {
			pr_err("%s: new dphy freq is invalid.", __func__);
			return -EINVAL;
		}
		pr_info("dphy frequency is switched from %dHz to %dHz\n",
				panel->info.mipi->phy_feq * 1000,
				new_val * 1000);
		return ret;

	default:
		pr_err("%s: Unsupported clock type.\n", __func__);
		return -EINVAL;
	}

	if (howto != SPRDFB_FORCE_PCLK &&
			howto != SPRDFB_FORCE_FPS &&
			!fb_dev->enable) {
		pr_warn("After fb_dev is resumed, dpi or mipi clk will be updated\n");
		return ret;
	}

	/* Now let's do update dpi clock */
	ret = clk_set_rate(dispc_ctx.clk_dispc_dpi, new_pclk);
	if (ret) {
		pr_err("Failed to set pixel clock.\n");
		return ret;
	}

	pr_info("dpi clock is switched from %dHz to %dHz\n",
					fb_dev->dpi_clock, new_pclk);
	/* Save the updated clock */
	fb_dev->dpi_clock = new_pclk;
	return ret;
}

#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || defined(CONFIG_SPRD_SCX35_DMC_FREQ)) && (!defined(CONFIG_FB_SCX30G))
/*return value:
0 -- Allow DMC change frequency
1 -- Don't allow DMC change frequency*/
static unsigned int sprdfb_dispc_change_threshold(struct devfreq_dbs *h,
		unsigned int state)
{
	struct sprdfb_dispc_context *dispc_ctx =
		(struct sprdfb_dispc_context *)h->data;
	struct sprdfb_device *dev = dispc_ctx->dev;
	bool dispc_run;
	unsigned long flags;

	if (!dev || !dev->enable) {
		pr_err("%s, dev not enabled\n", __func__);
		return 0;
	}

	pr_info("%s, DMC change freq(%u)\n", state);
	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		down(&dev->refresh_lock);

		dispc_run = dispc_read(DISPC_CTRL) & BIT(4);
		if (dispc_run) {
			local_irq_save(flags);
			dispc_stop_for_feature(dev);
		}

		if (state == DEVFREQ_PRE_CHANGE)
			dispc_set_threshold(0x960, 0x00, 0x960);
		else
			dispc_set_threshold(0x500, 0x00, 0x500);

		if (dispc_run) {
			dispc_run_for_feature(dev);
			local_irq_restore(flags);
		}
		up(&dev->refresh_lock);
	}
	return 0;
}
#endif

/* begin bug210112 */
#include <mach/board.h>
extern uint32_t lcd_base_from_uboot;

static int32_t sprdfb_dispc_refresh_logo(struct sprdfb_device *dev)
{
	uint32_t cnt;
	pr_debug("%s +\n", __func__);

	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
		pr_err("%s, panel interface is not dpi\n", __func__);
		return 0;
	}

	/* disable all interrupt */
	dispc_clear_bits(0x1f, DISPC_INT_EN);

	/* clear all interrupt */
	dispc_set_bits(0x1f, DISPC_INT_CLR);
	dispc_set_bits(BIT(5), DISPC_DPI_CTRL);

	/* wait for update done */
	cnt = 500;
	while (!(dispc_read(DISPC_INT_RAW) & 0x10) && --cnt)
			udelay(1000);
	if (!cnt)
		pr_err("%s, wait update done (status :%d) - timeout!!\n",
				__func__, dispc_read(DISPC_INT_RAW));
	dispc_set_bits((1<<5), DISPC_INT_CLR);
	return 0;
}

int32_t sprdfb_dispc_logo_proc(struct sprdfb_device *dev)
{
	uint32_t logo_src_v = 0;
	uint32_t logo_dst_v = 0;
	uint32_t logo_dst_p = 0;
	uint32_t logo_size = 0;

	pr_debug("%s +\n", __func__);

	if (!dev) {
		pr_err("%s, no device\n", __func__);
		return -1;
	}

	if (!lcd_base_from_uboot) {
		pr_warn("%s, no lcd_base\n", __func__);
		return -1;
	}

	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
		return -1;

#ifdef CONFIG_FB_LOW_RES_SIMU
	if (dev->panel->display_width && dev->panel->display_height) {
		/* should be rgb565 */
		logo_size = dev->panel->display_width *
			dev->panel->display_height * 2;
	} else
#endif
	/* should be rgb565 */
	logo_size = dev->panel->width * dev->panel->height * 2;

	dev->logo_buffer_size = logo_size;
	dev->logo_buffer_addr_v =
		__get_free_pages(GFP_ATOMIC | __GFP_ZERO,
			get_order(logo_size));
	if (!dev->logo_buffer_addr_v) {
		pr_err("%s, allocation failed\n", __func__);
		return -1;
	}

	logo_dst_v = dev->logo_buffer_addr_v;
	logo_dst_p = __pa(dev->logo_buffer_addr_v);
	logo_src_v = (uint32_t)ioremap(lcd_base_from_uboot, logo_size);
	if (!logo_src_v || !logo_dst_v) {
		pr_err("%s, unable to map logo virt(s:0x%08x, d:0x%08x)\n",
			__func__, logo_src_v, logo_dst_v);
		return -1;
	}

	memcpy((void *)logo_dst_v, (void *)logo_src_v, logo_size);

	dma_sync_single_for_device(dev->of_dev, logo_dst_p,
			logo_size, DMA_TO_DEVICE);
	iounmap((void *)logo_src_v);
	dispc_write(logo_dst_p, DISPC_OSD_BASE_ADDR);
	sprdfb_dispc_refresh_logo(dev);

	pr_info("%s, copy logo - src p:0x%08X dst p:0x%08X size %dk\n",
			__func__, lcd_base_from_uboot,
			logo_dst_p, logo_size >> 10);

	return 0;
}

/* end bug210112 */
#ifdef CONFIG_FB_MMAP_CACHED
void sprdfb_set_vma(struct vm_area_struct *vma)
{
	if (vma)
		dispc_ctx.vma = vma;
}
#endif

int32_t sprdfb_is_refresh_done(struct sprdfb_device *dev)
{
	pr_info("%s, vsync_done (%d)\n", __func__, dispc_ctx.vsync_done);

	return (int32_t)dispc_ctx.vsync_done;
}

struct display_ctrl sprdfb_dispc_ctrl = {
	.name		= "dispc",
	.early_init	= sprdfb_dispc_early_init,
	.init		= sprdfb_dispc_init,
	.uninit		= sprdfb_dispc_uninit,
	.refresh	= sprdfb_dispc_refresh,
	.logo_proc	= sprdfb_dispc_logo_proc,
	.suspend	= sprdfb_dispc_suspend,
	.resume		= sprdfb_dispc_resume,
	.update_clk	= dispc_update_clk_intf,
#ifdef CONFIG_FB_ESD_SUPPORT
	.ESD_check	= sprdfb_dispc_check_esd,
#endif
#ifdef CONFIG_LCD_ESD_RECOVERY
	.ESD_reset = sprdfb_dispc_esd_reset,
#endif
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	.enable_overlay = sprdfb_dispc_enable_overlay,
	.display_overlay = sprdfb_dispc_display_overlay,
#endif
#ifdef CONFIG_FB_VSYNC_SUPPORT
	.wait_for_vsync = spdfb_dispc_wait_for_vsync,
#endif
#ifdef CONFIG_FB_MMAP_CACHED
	.set_vma = sprdfb_set_vma,
#endif
	.is_refresh_done = sprdfb_is_refresh_done,
};
