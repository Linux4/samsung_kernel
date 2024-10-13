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
#define pr_fmt(fmt) "sprd: " fmt

#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/delay.h>
#if defined(CONFIG_SPRD_SCX35_DMC_FREQ)
#include <linux/devfreq.h>
#endif
#include <linux/irqreturn.h>
#include <linux/interrupt.h>
#include <linux/of_irq.h>
#include <linux/time.h>
#include <asm/cacheflush.h>
#include "sprd_dispc_reg.h"

#include "sprd_display.h"
#include "sprd_adf_common.h"
#include "sprd_interface.h"
#include <linux/dma-mapping.h>
#include "sprd_adf_adapter.h"

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


#if (defined CONFIG_FB_SCX35L)
#define SPRDFB_BRIGHTNESS           (0x00<<16)
#define SPRDFB_CONTRAST             (0x100<<0)
#define SPRDFB_OFFSET_U             (0x81<<16)

#define SPRDFB_SATURATION_U         (0x100<<0)
#define SPRDFB_OFFSET_V             (0x82<<16)

#define SPRDFB_SATURATION_V         (0x100<<0)
#elif ((defined CONFIG_FB_SCX15) || (defined CONFIG_FB_SCX30G))
#define SPRDFB_BRIGHTNESS           (0x02<<16)
#define SPRDFB_CONTRAST             (0x12A<<0)
#define SPRDFB_OFFSET_U             (0x81<<16)
#define SPRDFB_SATURATION_U         (0x123<<0)
#define SPRDFB_OFFSET_V             (0x82<<16)
#define SPRDFB_SATURATION_V         (0x100<<0)
#else
#define SPRDFB_SATURATION_V         (0x123<<0)
#define SPRDFB_CONTRAST (74)
#define SPRDFB_SATURATION (73)
#define SPRDFB_BRIGHTNESS (2)
#endif

enum SPRDFB_DYNAMIC_CLK_SWITCH_E {
	/* force enable/disable */
	SPRDFB_DYNAMIC_CLK_FORCE,
	/* enable for refresh/display_overlay */
	SPRDFB_DYNAMIC_CLK_REFRESH,
	/* enable disable in pairs */
	SPRDFB_DYNAMIC_CLK_COUNT, SPRDFB_DYNAMIC_CLK_MAX,
};
unsigned long g_dispc_base_addr;
EXPORT_SYMBOL(g_dispc_base_addr);
int use_buffer;
module_param_named(debug, use_buffer, int, 0600);
MODULE_PARM_DESC(debug, "Enable use buffer");
bool use_buffer_flag = false, vsynced_flag = false;
unsigned long buffer_addr;
struct timespec start, end;

#define SPRDFB_DEVICE_TO_CONTEX(p) ((p)->dispc_ctx)
extern void dispc_set_dsi_irq(struct panel_if_device *intf);
static int sprd_dispc_clk_disable(struct sprd_dispc_context *dispc_ctx_ptr,
				   enum SPRDFB_DYNAMIC_CLK_SWITCH_E
				    clock_switch_type);
static int sprd_dispc_clk_enable(struct sprd_dispc_context *dispc_ctx_ptr,
				  enum SPRDFB_DYNAMIC_CLK_SWITCH_E
				   clock_switch_type);
static int32_t sprd_dispc_init(struct sprd_device *dev);
static void dispc_reset(int dev_id);
static void dispc_stop(struct sprd_device *dev);
static void dispc_module_enable(int dev_id);
static int dispc_update_clk(struct sprd_device *dev,
	u32 new_val, int howto);
static void dispc_stop_for_feature(struct sprd_device *dev);
static void dispc_run_for_feature(struct sprd_device *dev);
static int32_t dispc_img_ctrl(struct sprd_device *dev,
	__u32 layer_id, __u32 format);
#if defined(CONFIG_SPRD_SCX35_DMC_FREQ)
static unsigned int sprd_dispc_change_threshold(struct devfreq_dbs *h,
						  unsigned int state);
static unsigned int sprd_dispc_threshold_check(void);
#endif

static int dispc_check_new_clk(struct sprd_device *dev,
			       u32 *new_pclk, u32 pclk_src,
			       u32 new_val, int type)
{
	int divider;
	u32 hpixels, vlines, pclk, fps;
	struct panel_spec *panel = dev->intf->get_panel(dev->intf);
	struct info_mipi *mipi;
	struct info_rgb *rgb;

	pr_debug("%s: enter\n", __func__);
	if (!panel) {
		pr_err("No panel is specified!\n");
		return -ENXIO;
	}

	mipi = dev->intf->info.mipi;
	rgb = dev->intf->info.rgb;

	if (dev->intf->panel_if_type != SPRDFB_PANEL_IF_DPI) {
		pr_err("panel interface should be DPI\n");
		return -EINVAL;
	}
	if (new_val <= 0 || !new_pclk) {
		pr_err("new parameter is invalid\n");
		return -EINVAL;
	}

	if (panel->type == LCD_MODE_DSI) {
		struct timing_rgb *timing = &panel->timing.rgb_timing;
		hpixels = panel->width + timing->hsync +
		    timing->hbp + timing->hfp;
		vlines = panel->height + timing->vsync +
		    timing->vbp + timing->vfp;
	} else if (panel->type == LCD_MODE_RGB
		|| panel->type == LCD_MODE_LVDS) {
		struct timing_rgb *timing = &panel->timing.rgb_timing;
		hpixels = panel->width + timing->hsync +
		    timing->hbp + timing->hfp;
		vlines = panel->height + timing->vsync +
		    timing->vbp + timing->vfp;
	} else {
		pr_err("[%s] unexpected panel type (%d)\n",
		       __func__, panel->type);
		return -EINVAL;
	}

	switch (type) {
	case SPRDFB_FORCE_FPS:
	case SPRDFB_DYNAMIC_FPS:
		if (new_val < LCD_MIN_FPS || new_val > LCD_MAX_FPS) {
			pr_err
			    ("Unsupported FPS. fps range should be [%d, %d]\n",
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
			pr_err
			    ("Unsupported FPS. fps range should be [%d, %d]\n",
			     LCD_MIN_FPS, LCD_MAX_FPS);
			return -EINVAL;
		}
		*new_pclk = pclk;
		/* Save the updated fps */
		panel->fps = fps;
		break;

	default:
		pr_err("This checked type is unsupported.\n");
		*new_pclk = 0;
		return -EINVAL;
	}
	return 0;
}

void dispc_irq_trick(struct sprd_device *dev)
{
	if (NULL == dev)
		return;

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type)
		dispc_set_bits(dev->dev_id, DISPC_INT_ERR_MASK, DISPC_INT_EN);
}

/* static uint32_t underflow_ever_happened = 0; */
static irqreturn_t dispc_isr(int irq, void *data)
{
	struct sprd_dispc_context *dispc_ctx =
	    (struct sprd_dispc_context *)data;
	uint32_t reg_val;
	struct sprd_device *dev = dispc_ctx->dev;
	bool done = false;
	bool vsync = false;

	reg_val = dispc_read(dev->dev_id, DISPC_INT_STATUS);
	pr_debug("sprd:<dispc%d> dispc_isr (0x%x)\n", dev->dev_id, reg_val);

	if (reg_val & DISPC_INT_ERR_MASK) {
		PERROR("Warning: dispc underflow (0x%x)!\n", reg_val);
		dispc_write(dev->dev_id, DISPC_INT_ERR_MASK, DISPC_INT_CLR);
		/*disable err interupt */
		dispc_clear_bits(dev->dev_id, DISPC_INT_ERR_MASK, DISPC_INT_EN);
	}

	if (NULL == dev)
		return IRQ_HANDLED;

	if ((reg_val & 0x10) && (SPRDFB_PANEL_IF_DPI == dev->panel_if_type)) {
		/*dispc update done isr */
		dispc_write(dev->dev_id, DISPC_INT_UPDATE_DONE_MASK,
			    DISPC_INT_CLR);
		done = true;
	} else if ((reg_val & DISPC_INT_DONE_MASK)
		   && (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)) {
		/* dispc done isr */
		dispc_write(dev->dev_id, DISPC_INT_DONE_MASK, DISPC_INT_CLR);
		dispc_ctx->is_first_frame = false;
		done = true;
	}
	if ((reg_val & DISPC_INT_HWVSYNC)
		&& (SPRDFB_PANEL_IF_DPI == dev->panel_if_type)) {
		/*dispc done isr */
		dispc_write(dev->dev_id, DISPC_INT_HWVSYNC, DISPC_INT_CLR);
		vsync = true;
	} else if ((reg_val & DISPC_INT_TE_MASK)
			   && (SPRDFB_PANEL_IF_EDPI == dev->panel_if_type)) {
		/*dispc te isr */
		dispc_write(dev->dev_id, DISPC_INT_TE_MASK, DISPC_INT_CLR);
		vsync = true;
	}
	if (vsync)
		sprd_adf_report_vsync(dev->pdev, ADF_INTF_FLAG_PRIMARY);

	if (done) {
		dispc_ctx->vsync_done = 1;

#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
			sprd_dispc_clk_disable(dispc_ctx,
						 SPRDFB_DYNAMIC_CLK_REFRESH);
		}
#endif

		if (dispc_ctx->vsync_waiter) {
			wake_up_interruptible_all(&(dispc_ctx->vsync_queue));
			dispc_ctx->vsync_waiter = 0;
		}
		pr_debug(KERN_INFO "sprd: [%s]: Done INT, reg_val = %d !\n",
			 __func__, reg_val);
	}
	return IRQ_HANDLED;
}

/* dispc soft reset */
static void dispc_reset(int dev_id)
{
	sci_glb_set(REG_AHB_SOFT_RST, (BIT_DISPC_SOFT_RST) << dev_id);
	udelay(10);
	sci_glb_clr(REG_AHB_SOFT_RST, (BIT_DISPC_SOFT_RST) << dev_id);
}

static inline void dispc_set_bg_color(int dev_id, uint32_t bg_color)
{
	dispc_write(dev_id, bg_color, DISPC_BG_COLOR);
}

static inline void dispc_set_osd_ck(int dev_id, uint32_t ck_color)
{
	dispc_write(dev_id, ck_color, DISPC_OSD_CK);
}

static inline void dispc_osd_enable(int dev_id, bool is_enable)
{
	uint32_t reg_val;

	reg_val = dispc_read(dev_id, DISPC_OSD_CTRL);
	if (is_enable)
		reg_val = reg_val | (BIT(0));
	else
		reg_val = reg_val & (~(BIT(0)));
	dispc_write(dev_id, reg_val, DISPC_OSD_CTRL);
}

static inline void dispc_set_osd_alpha(int dev_id, uint8_t alpha)
{
	dispc_write(dev_id, alpha, DISPC_OSD_ALPHA);
}

static void dispc_dithering_enable(int dev_id, bool enable)
{
	if (enable)
		dispc_set_bits(dev_id, BIT(6), DISPC_CTRL);
	else
		dispc_clear_bits(dev_id, BIT(6), DISPC_CTRL);
}

static void dispc_pwr_enable(int dev_id, bool enable)
{
	if (enable)
		dispc_set_bits(dev_id, BIT(7), DISPC_CTRL);
	else
		dispc_clear_bits(dev_id, BIT(7), DISPC_CTRL);
}

static void dispc_set_exp_mode(int dev_id, uint16_t exp_mode)
{
	uint32_t reg_val = dispc_read(dev_id, DISPC_CTRL);

	reg_val &= ~(0x3 << 16);
	reg_val |= (exp_mode << 16);
	dispc_write(dev_id, reg_val, DISPC_CTRL);
}

/*
 * thres0: module fetch data when buffer depth <= thres0
 * thres1: module release busy and close AXI clock
 * thres2: module open AXI clock and prepare to fetch data
 */
static void dispc_set_threshold(int dev_id, uint16_t thres0, uint16_t thres1,
				uint16_t thres2)
{
	dispc_write(dev_id, (thres0) | (thres1 << 14) | (thres2 << 16),
		    DISPC_BUF_THRES);
}

static void dispc_module_enable(int dev_id)
{
	/*dispc module enable */
	dispc_write(dev_id, (1 << 0), DISPC_CTRL);

	/*disable dispc INT */
	dispc_write(dev_id, 0x0, DISPC_INT_EN);

	/* clear dispc INT */
	dispc_write(dev_id, 0x3F, DISPC_INT_CLR);
}

static int32_t dispc_sync(struct sprd_device *dev)
{
	int ret;
	int dev_id = dev->dev_id;
	struct sprd_dispc_context *dispc_ctx = SPRDFB_DEVICE_TO_CONTEX(dev);

	if (dispc_ctx->enable == 0) {
		PERROR("dispc_sync fb suspeneded already!!\n");
		return -1;
	}

	ret = wait_event_interruptible_timeout(dispc_ctx->vsync_queue,
					       dispc_ctx->vsync_done,
					       msecs_to_jiffies(100));
	if (!ret) {
		/* time out */
		dispc_ctx->vsync_done = 1;
		PERROR("dispc_sync time out!!!!!\n");
		{
		/*for debug */
			int32_t i = 0;
			for (i = 0; i < 256; i += 16) {
				PERROR("%x: 0x%x, 0x%x, 0x%x, 0x%x\n", i,
				      dispc_read(dev_id, i),
				      dispc_read(dev_id, i + 4),
				      dispc_read(dev_id, i + 8),
				      dispc_read(dev_id, i + 12));
			}
			PERROR("**************************************\n");
		}
		return -1;
	}
	if (NULL != dev->logo_buffer_addr_v) {
		PERROR("free logo proc buffer!\n");
		free_pages((unsigned long)dev->logo_buffer_addr_v,
			   get_order(dev->logo_buffer_size));
		dev->logo_buffer_addr_v = 0;
	}
	return 0;
}

static void dispc_run(struct sprd_device *dev)
{
	struct sprd_dispc_context *dispc_ctx = SPRDFB_DEVICE_TO_CONTEX(dev);
	if (0 == dispc_ctx->enable)
		return;

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		if (!dispc_ctx->is_first_frame) {
			dispc_ctx->vsync_done = 0;
			dispc_ctx->vsync_waiter++;
		}
		/*dpi register update */
		dispc_set_bits(dev->dev_id, BIT(5), DISPC_DPI_CTRL);
		udelay(30);

		if (dispc_ctx->is_first_frame) {
			/*dpi register update with SW and VSync */
			dispc_clear_bits(dev->dev_id, BIT(4), DISPC_DPI_CTRL);

			/* start refresh */
			dispc_set_bits(dev->dev_id, (1 << 4), DISPC_CTRL);

			dispc_ctx->is_first_frame = false;
		}
	} else {
		dispc_ctx->vsync_done = 0;
		/* start refresh */
		dispc_set_bits(dev->dev_id, (1 << 4), DISPC_CTRL);
	}
	dispc_irq_trick(dev);
}

static void dispc_stop(struct sprd_device *dev)
{
	struct sprd_dispc_context *dispc_ctx = SPRDFB_DEVICE_TO_CONTEX(dev);
	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		/*dpi register update with SW only */
		dispc_set_bits(dev->dev_id, BIT(4), DISPC_DPI_CTRL);
		/* stop refresh */
		dispc_clear_bits(dev->dev_id, (1 << 4), DISPC_CTRL);
		dispc_ctx->is_first_frame = true;
	}
}

static int32_t sprd_dispc_uninit(struct sprd_device *dev)
{
	struct sprd_dispc_context *dispc_ctx = SPRDFB_DEVICE_TO_CONTEX(dev);

	pr_debug(KERN_INFO "sprd: [%s]\n", __func__);
	dispc_ctx->enable = 0;
	sprd_dispc_clk_disable(dispc_ctx, SPRDFB_DYNAMIC_CLK_FORCE);
	return 0;
}

void dispc_print_clk(int index)
{
	u32 reg_val0, reg_val1, reg_val2;
	reg_val0 = dispc_glb_read(SPRD_AONAPB_BASE + 0x4);
	reg_val1 = dispc_glb_read(SPRD_AHB_BASE);
	reg_val2 = dispc_glb_read(SPRD_APBREG_BASE);
	pr_info("sprd:0x402e0004 = 0x%x 0x20d00000 = 0x%x 0x71300000 = 0x%x\n",
			reg_val0, reg_val1, reg_val2);

	reg_val0 = dispc_glb_read(SPRD_APBCKG_BASE + 0x34);
	reg_val1 = dispc_glb_read(SPRD_APBCKG_BASE + 0x30);
	reg_val2 = dispc_glb_read(SPRD_APBCKG_BASE + 0x2c);
	pr_info("sprd:0x71200034 = 0x%x 0x71200030 = 0x%x 0x7120002c = 0x%x\n",
			reg_val0, reg_val1, reg_val2);
}

static int32_t dispc_clk_init(struct sprd_device *dev)
{
	int ret = 0;
	u32 dpi_clk;
	int index;
	struct clk *clk_parent1, *clk_parent2, *clk_parent3, *clk_parent4;
	struct panel_spec *panel = dev->intf->get_panel(dev->intf);
	struct sprd_dispc_context *dispc_ctx = SPRDFB_DEVICE_TO_CONTEX(dev);
	uint32_t clk_src[DISPC_CLOCK_NUM];
	uint32_t def_dpi_clk_div = 1;

	index = dispc_ctx->dev->dev_id;
	/* dispc enable bit */
	sci_glb_set(DISPC_CORE_EN, BIT_DISPC_CORE_EN + index);
	ret = of_property_read_u32_array(dev->of_dev->of_node,
			 "clock-src", clk_src, 3);
	if (0 != ret) {
		PERROR("read clock-src fail (%d)\n", ret);
		return -1;
	}

	ret = of_property_read_u32(dev->of_dev->of_node, "dpi_clk_div",
				 &def_dpi_clk_div);
	if (0 != ret) {
		PERROR("read dpi_clk_div fail (%d)\n", ret);
		return -1;
	}
	clk_parent1 = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_CLOCK_PARENT);
	dpi_clk = clk_src[DISPC_DPI_CLOCK_SRC_ID] / def_dpi_clk_div;
	PRINT("dpi_clk = %d\n", dpi_clk);
	dev->intf->pclk_src = dpi_clk;
	if (IS_ERR(clk_parent1)) {
		PERROR("get DISPC_CLOCK_PARENT fail!\n");
		return -1;
	} else
		PRINT("sprd: get DISPC_CLOCK_PARENT ok!\n");

	clk_parent2 = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_DBI_CLOCK_PARENT);
	if (IS_ERR(clk_parent2)) {
		PERROR("get DISPC_DBI_CLOCK_PARENT fail!\n");
		return -1;
	} else
		PRINT("sprd: get DISPC_DBI_CLOCK_PARENT ok!\n");

	clk_parent3 = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_DPI_CLOCK_PARENT);
	if (IS_ERR(clk_parent3)) {
		PERROR("get DISPC_DPI_CLOCK_PARENT fail!\n");
		return -1;
	} else
		PRINT("sprd: get DISPC_DPI_CLOCK_PARENT ok!\n");

	clk_parent4 = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_EMC_EN_PARENT);
	if (IS_ERR(clk_parent3)) {
		PERROR("get DISPC_EMC_EN_PARENT fail!\n");
		return -1;
	} else
		PRINT("sprd: get DISPC_EMC_EN_PARENT ok!\n");

	dispc_ctx->clk_dispc = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_PLL_CLK);
	if (IS_ERR(dispc_ctx->clk_dispc)) {
		PERROR("get DISPC_PLL_CLK fail!\n");
		return -1;
	} else
		PRINT("sprd: get DISPC_PLL_CLK ok!\n");

	dispc_ctx->clk_dispc_dbi = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_DBI_CLK);
	if (IS_ERR(dispc_ctx->clk_dispc_dbi)) {
		PERROR("get DISPC_DBI_CLK fail!\n");
		return -1;
	} else
		PRINT("sprd: get DISPC_DBI_CLK ok!\n");

	dispc_ctx->clk_dispc_dpi = of_clk_get_by_name(dev->of_dev->of_node,
			DISPC_DPI_CLK);
	if (IS_ERR(dispc_ctx->clk_dispc_dpi)) {
		PERROR("get clk_dispc_dpi fail!\n");
		return -1;
	} else
		PRINT("sprd: get clk_dispc_dpi ok!\n");

	dispc_ctx->clk_dispc_emc = of_clk_get_by_name(dev->of_dev->of_node,
				DISPC_EMC_CLK);
	if (IS_ERR(dispc_ctx->clk_dispc_emc)) {
		PERROR("get DISPC_EMC_CLK fail!\n");
		return -1;
	} else
		PRINT("sprd: get DISPC_EMC_CLK ok!\n");

	ret = clk_set_parent(dispc_ctx->clk_dispc, clk_parent1);
	if (ret)
		PERROR("dispc set clk parent fail\n");
	ret = clk_set_rate(dispc_ctx->clk_dispc, clk_src[DISPC_CLOCK_SRC_ID]);
	if (ret)
		PERROR("dispc set clk parent fail\n");

	ret = clk_set_parent(dispc_ctx->clk_dispc_dbi, clk_parent2);
	if (ret)
		PERROR("dispc set dbi clk parent fail\n");
	ret =
	    clk_set_rate(dispc_ctx->clk_dispc_dbi,
			 clk_src[DISPC_DBI_CLOCK_SRC_ID]);
	if (ret)
		PERROR("dispc set dbi clk parent fail\n");

	ret = clk_set_parent(dispc_ctx->clk_dispc_dpi, clk_parent3);
	if (ret)
		PERROR("dispc set dpi clk parent fail\n");

	ret = clk_set_parent(dispc_ctx->clk_dispc_emc, clk_parent4);
	if (ret)
		PERROR("dispc set emc clk parent fail\n");

	if (panel && panel->fps)
		dispc_update_clk(dev, panel->fps, SPRDFB_FORCE_FPS);
	else
		dispc_update_clk(dev, dpi_clk, SPRDFB_FORCE_PCLK);
	dev->intf->dpi_clock = dev->dpi_clock;
	ret = clk_prepare_enable(dispc_ctx->clk_dispc_emc);
	if (ret)
		PERROR("enable clk_dispc_emc error!!!\n");

	ret = sprd_dispc_clk_enable(dispc_ctx, SPRDFB_DYNAMIC_CLK_FORCE);
	if (ret) {
		PERROR("enable dispc_clk fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "sprd: [%s] enable dispc_clk ok!\n",
			 __func__);
	}

	dispc_print_clk(dev->dev_id);

	return 0;
}

#if defined(CONFIG_SPRD_SCX35_DMC_FREQ)
struct devfreq_dbs sprd_fb_notify = {
	.level = 0,
	.data = 0,
	.devfreq_notifier = sprd_dispc_change_threshold,
};
#endif

static int32_t sprd_dispc_module_init(struct sprd_device *dev)
{
	int ret = 0;
	int irq_num = 0;
	struct sprd_dispc_context *dispc_ctx = SPRDFB_DEVICE_TO_CONTEX(dev);

	if (dispc_ctx->is_inited) {
		PERROR("dispc_module has already initialized! warning!!\n");
		return 0;
	} else {
		PERROR("dispc_module_init. call only once!\n");
	}

	dispc_ctx->vsync_done = 1;
	dispc_ctx->vsync_waiter = 0;
	init_waitqueue_head(&(dispc_ctx->vsync_queue));

	sema_init(&dispc_ctx->refresh_lock, 1);

	irq_num = irq_of_parse_and_map(dev->of_dev->of_node, 0);
	PRINT("sprd: dispc irq_num = %d\n", irq_num);

	ret =
	    request_irq(irq_num, dispc_isr, IRQF_DISABLED, "DISPC", dispc_ctx);
	if (ret) {
		PERROR("dispc failed to request irq!\n");
		sprd_dispc_uninit(dev);
		return -1;
	}

	dispc_ctx->is_inited = true;

#if defined(CONFIG_SPRD_SCX35_DMC_FREQ)
	ret = sprd_dispc_threshold_check();
	if (ret) {
		sprd_fb_notify.data = dispc_ctx;
		devfreq_notifier_register(&sprd_fb_notify);
	}
#endif

	return 0;

}

static int32_t sprd_dispc_early_init(struct sprd_device *dev)
{
	int ret = 0;
	struct sprd_dispc_context *dispc_ctx = dev->dispc_ctx;

	if (!dispc_ctx->is_inited) {
		/* spin_lock_init(&dispc_ctx->clk_spinlock); */
		sema_init(&dispc_ctx->clk_sem, 1);
	}
	ret = dispc_clk_init(dev);
	if (ret) {
		PERROR("dispc_clk_init fail!\n");
		return -1;
	}

	if (!dispc_ctx->is_inited) {
		/* init */
		if (dev->panel_ready) {
			/* panel ready */
			PRINT("dispc has alread initialized\n");
			dispc_ctx->is_first_frame = false;
		} else {
			/* panel not ready */
			PERROR("dispc is not initialized\n");
			dispc_reset(dev->dev_id);
			dispc_module_enable(dev->dev_id);
			dispc_ctx->is_first_frame = true;
		}
		ret = sprd_dispc_module_init(dev);
	} else {
		/* resume */
		PRINT("sprd_dispc_early_init resume\n");
		dispc_reset(dev->dev_id);
		dispc_module_enable(dev->dev_id);
		dispc_ctx->is_first_frame = true;
	}
	return ret;
}

static int sprd_dispc_clk_disable(struct sprd_dispc_context *dispc_ctx_ptr,
				    enum SPRDFB_DYNAMIC_CLK_SWITCH_E
				    clock_switch_type)
{
	bool is_need_disable = false;

	pr_debug(KERN_INFO "sprd: [%s]\n", __func__);
	if (!dispc_ctx_ptr)
		return 0;
	down(&dispc_ctx_ptr->clk_sem);
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
		pr_debug(KERN_INFO "sprd: sprd_dispc_clk_disable real\n");
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc);
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc_dpi);
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc_dbi);
		dispc_ctx_ptr->clk_is_open = false;
		dispc_ctx_ptr->clk_is_refreshing = false;
		dispc_ctx_ptr->clk_open_count = 0;
	}
	up(&dispc_ctx_ptr->clk_sem);

	pr_debug(KERN_INFO
		 "sprd: sprd_dispc_clk_disable type=%d refresh=%d,count=%d\n",
		 clock_switch_type, dispc_ctx_ptr->clk_is_refreshing,
		 dispc_ctx_ptr->clk_open_count);
	return 0;
}

static int sprd_dispc_clk_enable(struct sprd_dispc_context *dispc_ctx_ptr,
				   enum SPRDFB_DYNAMIC_CLK_SWITCH_E
				   clock_switch_type)
{
	int ret = 0;
	bool is_dispc_enable = false;
	bool is_dispc_dpi_enable = false;
	/* unsigned long irqflags; */

	if (!dispc_ctx_ptr) {
		PERROR("fail fail , dispc_ctx_ptr error\n");
		return -1;
	}
	/* spin_lock_irqsave(&dispc_ctx->clk_spinlock, irqflags); */
	down(&dispc_ctx_ptr->clk_sem);
	if (!dispc_ctx_ptr->clk_is_open) {
		PRINT(" sprd_dispc_clk_enable real\n");
		ret = clk_prepare_enable(dispc_ctx_ptr->clk_dispc);
		if (ret) {
			PERROR("enable clk_dispc error!!!\n");
			ret = -1;
			goto ERROR_CLK_ENABLE;
		}
		is_dispc_enable = true;
		ret = clk_prepare_enable(dispc_ctx_ptr->clk_dispc_dpi);
		if (ret) {
			PERROR("enable clk_dispc_dpi error!!!\n");
			ret = -1;
			goto ERROR_CLK_ENABLE;
		}
		is_dispc_dpi_enable = true;
		ret = clk_prepare_enable(dispc_ctx_ptr->clk_dispc_dbi);
		if (ret) {
			PERROR("enable clk_dispc_dbi error!!!\n");
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

	/* spin_unlock_irqrestore(&dispc_ctx->clk_spinlock,irqflags); */
	up(&dispc_ctx_ptr->clk_sem);

	PRINT("sprd_dispc_clk_enable type=%d refresh=%d,count=%d,ret=%d\n",
	      clock_switch_type, dispc_ctx_ptr->clk_is_refreshing,
	      dispc_ctx_ptr->clk_open_count, ret);
	return ret;

ERROR_CLK_ENABLE:
	if (is_dispc_enable)
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc);
	if (is_dispc_dpi_enable)
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc_dpi);
	/* spin_unlock_irqrestore(&dispc_ctx->clk_spinlock,irqflags); */
	up(&dispc_ctx_ptr->clk_sem);

	PERROR("sprd_dispc_clk_enable error!!!!!!\n");
	return ret;
}

static int32_t sprd_dispc_init(struct sprd_device *dev)
{

	/* ONLY for dispc interrupt en reg */
	uint32_t dispc_int_en_reg_val = 0x00;
	uint32_t size;
	struct panel_spec *panel = dev->intf->get_panel(dev->intf);
	struct sprd_dispc_context *dispc_ctx = SPRDFB_DEVICE_TO_CONTEX(dev);

	if (NULL == dev) {
		PERROR("[%s] Invalid parameter!\n", __func__);
		return -1;
	}

	/*set bg color */
	/* dispc_set_bg_color(0xFFFFFFFF); */
	dispc_set_bg_color(dev->dev_id, 0x0);
	/*enable dithering */
	dispc_dithering_enable(dev->dev_id, true);
	/*use MSBs as img exp mode */
	dispc_set_exp_mode(dev->dev_id, 0x0);
	/* enable DISPC Power Control */
	dispc_pwr_enable(dev->dev_id, true);
	dispc_set_threshold(dev->dev_id, 0x1388, 0x00, 0x1388);
	size = (panel->width & 0xffff) | ((panel->height) << 16);
	/*
	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
		sprd_panel_invalidate(panel);
	*/
	dispc_write(dev->dev_id, size, DISPC_SIZE_XY);
	dispc_set_osd_alpha(dev->dev_id, 0xff);
	dispc_img_ctrl(dev, SPRD_LAYER_OSD0, DRM_FORMAT_RGBA8888);

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		if (dispc_ctx->is_first_frame) {
			/*set dpi register update only with SW */
			dispc_set_bits(dev->dev_id, BIT(4), DISPC_DPI_CTRL);
		} else {
			/*set dpi register update with SW & VSYNC */
			dispc_clear_bits(dev->dev_id, BIT(4), DISPC_DPI_CTRL);
		}
		/*enable dispc update done INT */
		dispc_int_en_reg_val |= DISPC_INT_UPDATE_DONE_MASK;
		/* enable hw vsync */
		dispc_int_en_reg_val |= DISPC_INT_HWVSYNC;
	} else {
		/* enable dispc DONE  INT */
		dispc_int_en_reg_val |= DISPC_INT_DONE_MASK;
	}
	dispc_int_en_reg_val |= DISPC_INT_ERR_MASK;
	dispc_write(dev->dev_id, dispc_int_en_reg_val, DISPC_INT_EN);
	dispc_ctx->enable = 1;

	return 0;
}

static int32_t dispc_img_ctrl(struct sprd_device *dev, __u32 layer_id,
			      __u32 format)
{
	int reg_val = 0;

	/*IMG or OSD layer enable */
	reg_val |= BIT(0);
	switch (format) {
	case DRM_FORMAT_RGBA8888:
	case DRM_FORMAT_RGBX8888:
		/* rb switch */
		reg_val |= BIT(15);
	case DRM_FORMAT_BGRA8888:
		/* ABGR */
		reg_val |= (SPRD_DATA_TYPE_RGB888 << 4);
		/* alpha mode select  - block alpha */
		reg_val |= (1 << 2);
		/*disable expand mode: rgb565,rgb666 ... => argb888 */
		dispc_clear_bits(dev->dev_id, (3 << 16), DISPC_CTRL);
		break;
	case DRM_FORMAT_BGR888:
		/* rb switch */
		reg_val |= BIT(15);
	case DRM_FORMAT_RGB888:
		/* packed rgb888 */
		reg_val |= (SPRD_DATA_TYPE_RGB888_PACK << 4);
		/* alpha mode select  - block alpha */
		reg_val |= (1 << 2);
		break;
	case DRM_FORMAT_BGR565:
		/* rb switch */
		reg_val |= BIT(15);
	case DRM_FORMAT_RGB565:
		/* rgb565 */
		reg_val |= (SPRD_DATA_TYPE_RGB565 << 4);
		/* B2B3B0B1 */
		reg_val |= (SPRD_IMG_DATA_ENDIAN_B2B3B0B1 << 8);
		/* alpha mode select  - block alpha */
		reg_val |= (1 << 2);
		/*enable expand mode: rgb565,rgb666 ... => argb888 */
		dispc_clear_bits(dev->dev_id, (3 << 16), DISPC_CTRL);
		dispc_set_bits(dev->dev_id, 0x10000, DISPC_CTRL);
		break;
	case DRM_FORMAT_NV12:
		/*2-Lane: Yuv420 */
		reg_val |= SPRD_DATA_TYPE_YUV420 << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	case DRM_FORMAT_NV21:
		/*2-Lane: Yuv420 */
		reg_val |= SPRD_DATA_TYPE_YUV420 << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B0B1B2B3 << 10;
		break;
	case DRM_FORMAT_NV16:
		/*2-Lane: Yuv422 */
		reg_val |= SPRD_DATA_TYPE_YUV422 << 4;
		/*Y endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 8;
		/*UV endian */
		reg_val |= SPRD_IMG_DATA_ENDIAN_B3B2B1B0 << 10;
		break;
	default:
		PERROR("Invalid format 0x%x\n", format);
		break;
	}
	if (layer_id == SPRD_LAYER_IMG0 || layer_id == SPRD_LAYER_IMG1)
		dispc_write(dev->dev_id, reg_val, DISPC_IMG_CTRL);
	else
		dispc_write(dev->dev_id, reg_val, DISPC_OSD_CTRL);

	return 0;
}

static int32_t dispc_single_layer(struct sprd_device *dev,
				  struct sprd_adf_hwlayer *hwlayer)
{
	int size;
	int offset;
	int i;
	int wd;

	size = (hwlayer->dst_w & 0xffff) | ((hwlayer->dst_h) << 16);
	offset = (hwlayer->dst_x & 0xffff) | ((hwlayer->dst_y) << 16);

	switch (hwlayer->hwlayer_id) {
	case SPRD_LAYER_OSD0:
	case SPRD_LAYER_OSD1:
		PRINT("osd layer runing %d\n", hwlayer->hwlayer_id);
		dispc_write(dev->dev_id, offset, DISPC_OSD_DISP_XY);
		dispc_write(dev->dev_id, size, DISPC_OSD_SIZE_XY);
		/*osd layer has only one plane. */
		dispc_write(dev->dev_id, hwlayer->iova_plane[0],
			    DISPC_OSD_BASE_ADDR);
		wd = adf_format_bpp(hwlayer->format) / 8;
		/*byte unit => pixel unit.*/
		dispc_write(dev->dev_id, hwlayer->pitch[0] / wd,
			    DISPC_OSD_PITCH);
		dispc_img_ctrl(dev, hwlayer->hwlayer_id, hwlayer->format);
		break;
	case SPRD_LAYER_IMG0:
	case SPRD_LAYER_IMG1:
		PRINT("img layer runing\n");
		dispc_write(dev->dev_id, offset, DISPC_IMG_DISP_XY);
		dispc_write(dev->dev_id, size, DISPC_IMG_SIZE_XY);
		for (i = 0; i < hwlayer->n_planes; i++) {
			dispc_write(dev->dev_id, hwlayer->iova_plane[i],
				    DISPC_IMG_Y_BASE_ADDR + i * 4);
		}
		/*byte unit => pixel unit.*/
		dispc_write(dev->dev_id, hwlayer->pitch[0],
			    DISPC_IMG_PITCH);
		dispc_write(dev->dev_id, 1, DISPC_Y2R_CTRL);
		dispc_write(dev->dev_id, SPRDFB_BRIGHTNESS | SPRDFB_CONTRAST,
			    DISPC_Y2R_Y_PARAM);
		dispc_write(dev->dev_id, SPRDFB_OFFSET_U | SPRDFB_SATURATION_U,
			    DISPC_Y2R_U_PARAM);
		dispc_write(dev->dev_id, SPRDFB_OFFSET_V | SPRDFB_SATURATION_V,
			    DISPC_Y2R_V_PARAM);
		dispc_img_ctrl(dev, hwlayer->hwlayer_id, hwlayer->format);
		break;
	}
	use_buffer_flag = true;
	buffer_addr = hwlayer->iova_plane[0];
	return 0;
}

int32_t sprd_dispc_flip(struct sprd_device *dev,
			  struct sprd_restruct_config *config)
{
	int i;
	struct sprd_dispc_context *dispc_ctx = SPRDFB_DEVICE_TO_CONTEX(dev);
	for (i = 0; i < config->number_hwlayer; i++) {
		struct sprd_adf_hwlayer *hwlayer = &config->hwlayers[i];

		down(&dispc_ctx->refresh_lock);
		if (0 == dispc_ctx->enable) {
			PERROR("do not refresh in suspend!!!\n");
			goto ERROR_REFRESH;
		}
		if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
			if (sprd_dispc_clk_enable
			    (dispc_ctx, SPRDFB_DYNAMIC_CLK_REFRESH)) {
				PERROR("enable dispc_clk fail in refresh!\n");
				up(&dispc_ctx->refresh_lock);
				goto ERROR_REFRESH;
			}
		}
		/*need disable all layers first */
		dispc_write(dev->dev_id, 0, DISPC_OSD_CTRL);
		dispc_write(dev->dev_id, 0, DISPC_IMG_CTRL);
		dispc_single_layer(dev, hwlayer);
		/*
		   if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
		   sprd_panel_invalidate(panel);
		 */

		dispc_run(dev);
		up(&dispc_ctx->refresh_lock);
		continue;

ERROR_REFRESH:
		up(&dispc_ctx->refresh_lock);
	}
	return 0;
}

static int32_t sprd_dispc_suspend(struct sprd_device *dev)
{
	struct sprd_dispc_context *dispc_ctx = SPRDFB_DEVICE_TO_CONTEX(dev);
	PRINT("dispc_ctx->enable = %d\n", dispc_ctx->enable);

	if (0 != dispc_ctx->enable) {
		down(&dispc_ctx->refresh_lock);
		if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
			/* must wait ,dispc_sync() */
			dispc_ctx->vsync_waiter++;
			dispc_sync(dev);
			PERROR("open clk in suspend\n");
			if (sprd_dispc_clk_enable
			    (dispc_ctx, SPRDFB_DYNAMIC_CLK_COUNT)) {
				PERROR("clk enable fail!!!\n");
			}
			PRINT("got sync\n");
		}
		/*disable dsi irq1 bit7 before suspend*/
		//dispc_set_dsi_irq(dev->intf);
		dispc_ctx->enable = 0;
		up(&dispc_ctx->refresh_lock);
		dispc_stop(dev);
		dispc_write(dev->dev_id, 0, DISPC_INT_EN);

		msleep(50);	/*fps>20 */
		dispc_write(dev->dev_id, 0xff, DISPC_INT_CLR);
		clk_disable_unprepare(dispc_ctx->clk_dispc_emc);
		sprd_dispc_clk_disable(dispc_ctx, SPRDFB_DYNAMIC_CLK_FORCE);
	} else {
		PERROR("[%s]: Invalid device status %d\n",
		      __func__, dispc_ctx->enable);
	}
	return 0;
}

static int32_t sprd_dispc_shutdown(struct sprd_device *dev)
{
	sprd_dispc_suspend(dev);
	return 0;
}
static void dispc_dpi_init_config(struct sprd_device *dev)
{
	struct panel_spec *panel = dev->intf->get_panel(dev->intf);
	uint32_t reg_val;
	if (NULL == panel) {
		PERROR("fail.(Invalid Param)\n");
		return;
	}
	reg_val = dispc_read(dev->dev_id, DISPC_DPI_CTRL);

	if (SPRDFB_PANEL_TYPE_MIPI != panel->type) {
		PERROR("fail.(not mipi panel)\n");
		return;
	}

	if (SPRDFB_MIPI_MODE_CMD == panel->work_mode) {
		/*use edpi as interface */
		dispc_set_bits(dev->dev_id, (1 << 1), DISPC_CTRL);
	} else {
		/*use dpi as interface */
		dispc_clear_bits(dev->dev_id, (3 << 1), DISPC_CTRL);
	}

	/*h sync pol */
	if (SPRDFB_POLARITY_NEG == panel->h_sync_pol)
		reg_val |= (1 << 0);

	/*v sync pol */
	if (SPRDFB_POLARITY_NEG == panel->v_sync_pol)
		reg_val |= (1 << 1);

	/*de sync pol */
	if (SPRDFB_POLARITY_NEG == panel->de_pol)
		reg_val |= (1 << 2);

	if (SPRDFB_MIPI_MODE_VIDEO == panel->work_mode) {
#ifdef CONFIG_DPI_SINGLE_RUN
		/*single run mode */
		reg_val |= (1 << 3);
#endif
	} else {
		if (!(panel->cap & PANEL_CAP_NOT_TEAR_SYNC)) {
			PERROR("mipi_dispc_init_config not support TE\n");
			/*enable te */
			reg_val |= (1 << 8);
		}
		/*te pol */
		if (SPRDFB_POLARITY_NEG == panel->te_pol)
			reg_val |= (1 << 9);
		/*use external te */
		reg_val |= (1 << 10);
	}

	/*dpi bits */
	switch (panel->video_bus_width) {
	case 16:
		break;
	case 18:
		reg_val |= (1 << 6);
		break;
	case 24:
		reg_val |= (2 << 6);
		break;
	default:
		break;
	}

	dispc_write(dev->dev_id, reg_val, DISPC_DPI_CTRL);

	pr_debug("DISPC_DPI_CTRL = %d\n",
		 dispc_read(dev->dev_id, DISPC_DPI_CTRL));
}

static void dispc_set_dpi_timing(struct sprd_device *dev)
{

	dispc_write(dev->dev_id,
		    dev->intf->panel_timing.rgb_timing[RGB_LCD_H_TIMING],
		    DISPC_DPI_H_TIMING);
	dispc_write(dev->dev_id,
		    dev->intf->panel_timing.rgb_timing[RGB_LCD_V_TIMING],
		    DISPC_DPI_V_TIMING);
}

static int32_t sprd_dispc_resume(struct sprd_device *dev)
{
	struct sprd_dispc_context *dispc_ctx = SPRDFB_DEVICE_TO_CONTEX(dev);

	PRINT("dispc_ctx->enable= %d\n", dispc_ctx->enable);
	if (dispc_ctx->enable == 0) {
		down(&dispc_ctx->refresh_lock);
		if (sprd_dispc_clk_enable
		    (dispc_ctx, SPRDFB_DYNAMIC_CLK_FORCE)) {
			PRINT("[%s] clk enable fail!!\n", __func__);
			/* return 0; */
		}

		dispc_ctx->vsync_done = 1;

		/* (dispc_read(DISPC_SIZE_XY) == 0 ) { */
		/* resume from deep sleep */
		if (1) {
			sprd_dispc_early_init(dev);
			dispc_dpi_init_config(dev);
			dispc_set_dpi_timing(dev);
			/* sprd_panel_resume(dev, true); */
			sprd_dispc_init(dev);
		} else {
			PRINT("not from deep sleep\n");
			/*sprd_panel_resume(dev, true);*/
		}

		dispc_ctx->enable = 1;
		up(&dispc_ctx->refresh_lock);
	}

	return 0;
}

#ifdef CONFIG_FB_DYNAMIC_FREQ_SCALING
/**
 * author: Yang.Haibing haibing.yang@spreadtrum.com
 *
 * sprd_dispc_chg_clk - interface for sysfs to change dpi or dphy clock
 * @dev: spreadtrum specific fb device
 * @type: check the type is fps or pclk or mipi dphy freq
 * @new_val: fps or new dpi clock
 *
 * Returns 0 on success, MINUS on parsing error.
 */
int sprd_dispc_chg_clk(struct sprd_device *dev, int type, u32 new_val)
{
	int ret = 0;
	unsigned long flags = 0;
	struct panel_spec *panel = dev->intf->info.mipi->panel;
	struct sprd_dispc_context *dispc_ctx = SPRDFB_DEVICE_TO_CONTEX(dev);
	/* check if new value is valid */
	if (new_val <= 0) {
		pr_err("new value is invalid\n");
		return -EINVAL;
	}

	down(&dispc_ctx->refresh_lock);
	/* Now let's do update dpi clock */
	switch (type) {
	case SPRDFB_DYNAMIC_PCLK:
		if (dev->panel_if_type != SPRDFB_PANEL_IF_DPI) {
			pr_err("current dispc interface isn't DPI\n");
			ret = -EINVAL;
			goto exit;
		}
		/* Note: local interrupt will be disabled */
		local_irq_save(flags);
		dispc_stop_for_feature(dev);
		ret = dispc_update_clk(dev, new_val, SPRDFB_DYNAMIC_PCLK);
		break;

	case SPRDFB_DYNAMIC_FPS:
		if (dev->panel_if_type != SPRDFB_PANEL_IF_DPI) {
			dispc_ctx->vsync_waiter++;
			dispc_sync(dev);
			/* sprd_panel_change_fps(dev, new_val); */
			PRINT("sprd_panel_change_fps has been cut\n");

			ret = 0;
			goto exit;
		}
		/* Note: local interrupt will be disabled */
		local_irq_save(flags);
		dispc_stop_for_feature(dev);
		ret = dispc_update_clk(dev, new_val, SPRDFB_DYNAMIC_FPS);
		break;

	case SPRDFB_DYNAMIC_MIPI_CLK:
		/* Note: local interrupt will be disabled */
		local_irq_save(flags);
		dispc_stop_for_feature(dev);
		ret = dispc_update_clk(dev, new_val, SPRDFB_DYNAMIC_MIPI_CLK);
		break;

	default:
		pr_err("invalid CMD type\n");
		ret = -EINVAL;
		goto exit;
	}

	if (ret) {
		pr_err("Failed to set pixel clock, fps or dphy freq.\n");
		goto DONE;
	}
	if (type != SPRDFB_DYNAMIC_MIPI_CLK &&
	    panel->type == LCD_MODE_DSI &&
	    panel->work_mode ==
	    SPRDFB_MIPI_MODE_VIDEO && dispc_ctx->enable == true) {
		/* ret = dsi_dpi_init(dev); */
		PERROR("dsi_dpi_init has been cut\n");
		if (ret)
			pr_err("%s: dsi_dpi_init fail\n", __func__);
	}

DONE:
	dispc_run_for_feature(dev);
	local_irq_restore(flags);
	up(&dispc_ctx->refresh_lock);

	return ret;

exit:
	up(&dispc_ctx->refresh_lock);
	return ret;

}
#endif
static void dispc_stop_for_feature(struct sprd_device *dev)
{
	int i = 0;

	if (SPRDFB_PANEL_IF_DPI == dev->panel_if_type) {
		dispc_stop(dev);
		while (dispc_read(dev->dev_id, DISPC_DPI_STS1) & BIT(16)) {
			if (0x0 == ++i % 500000)
				PRINT("warning: busy waiting stop!\n");
		}
		udelay(25);
	}
}

static void dispc_run_for_feature(struct sprd_device *dev)
{
	dispc_run(dev);
}

static int dispc_update_clk_intf(struct sprd_device *dev)
{
	struct panel_spec *panel = dev->intf->get_panel(dev->intf);
	return dispc_update_clk(dev, panel->fps, SPRDFB_FORCE_FPS);
}

/**
 * author: Yang.Haibing haibing.yang@spreadtrum.com
 *
 * dispc_update_clk - update dpi clock via @howto and @new_val
 * @dev: spreadtrum specific fb device
 * @new_val: fps or new dpi clock
 * @howto: check the type is fps or pclk or mipi dphy freq
 *
 * Returns 0 on success, MINUS on error.
 */
static int dispc_update_clk(struct sprd_device *dev, u32 new_val, int howto)
{
	int ret;
	u32 new_pclk;
	u32 old_val;
	u32 dpi_clk_src;
	struct panel_spec *panel = dev->intf->get_panel(dev->intf);
	struct sprd_dispc_context *dispc_ctx = SPRDFB_DEVICE_TO_CONTEX(dev);
	uint32_t clk_src[DISPC_CLOCK_NUM];

	ret = of_property_read_u32_array(dev->of_dev->of_node,
					 "clock-src", clk_src, DISPC_CLOCK_NUM);
	if (ret) {
		pr_err("[%s] read clock-src fail (%d)\n", __func__, ret);
		return -EINVAL;
	}
	dpi_clk_src = clk_src[DISPC_DPI_CLOCK_SRC_ID];

	switch (howto) {
	case SPRDFB_FORCE_FPS:
		ret = dispc_check_new_clk(dev, &new_pclk,
					  dpi_clk_src, new_val,
					  SPRDFB_FORCE_FPS);
		if (ret) {
			pr_err("%s: new forced fps is invalid.", __func__);
			return -EINVAL;
		}
		break;

	case SPRDFB_DYNAMIC_FPS:	/* Calc dpi clock via fps */
		ret = dispc_check_new_clk(dev, &new_pclk, dpi_clk_src, new_val,
					  SPRDFB_DYNAMIC_FPS);
		if (ret) {
			pr_err("%s: new dynamic fps is invalid.", __func__);
			return -EINVAL;
		}
		break;

	case SPRDFB_FORCE_PCLK:
		new_pclk = new_val;
		break;

	case SPRDFB_DYNAMIC_PCLK:	/* Given dpi clock */
		ret = dispc_check_new_clk(dev, &new_pclk,
					  dpi_clk_src, new_val,
					  SPRDFB_DYNAMIC_PCLK);
		if (ret) {
			pr_err("%s: new dpi clock is invalid.", __func__);
			return -EINVAL;
		}
		break;

	case SPRDFB_DYNAMIC_MIPI_CLK:	/* Given mipi clock */
		if (!panel) {
			pr_err("%s: there is no panel!", __func__);
			return -ENXIO;
		}
		if (panel->type != LCD_MODE_DSI) {
			pr_err("%s: panel type isn't dsi mode", __func__);
			return -ENXIO;
		}
		old_val = panel->phy_feq;
		ret = dev->intf->ctrl->panel_if_chg_freq(dev->intf, new_val);
		if (ret) {
			pr_err("%s: new dphy freq is invalid.", __func__);
			return -EINVAL;
		}
		pr_info("dphy frequency is switched from %dHz to %dHz\n",
			old_val * 1000, new_val * 1000);
		return ret;

	default:
		pr_err("%s: Unsupported clock type.\n", __func__);
		return -EINVAL;
	}

	if (howto != SPRDFB_FORCE_PCLK &&
	    howto != SPRDFB_FORCE_FPS && !dispc_ctx->enable) {
		pr_warn
		    ("After dev is resumed, dpi or mipi clk will be updated\n");
		return ret;
	}

	/* Now let's do update dpi clock */
	ret = clk_set_rate(dispc_ctx->clk_dispc_dpi, new_pclk);
	if (ret) {
		pr_err("Failed to set pixel clock.\n");
		return ret;
	}

	pr_info("dpi clock is switched from %dHz to %dHz\n",
		dev->dpi_clock, new_pclk);
	/* Save the updated clock */
	dev->dpi_clock = new_pclk;
	return ret;
}

static int32_t sprd_dispc_refresh_logo(struct sprd_device *dev)
{
	uint32_t i = 0;
	PRINT("panel_if_type:%d\n", dev->panel_if_type);

	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
		PERROR("sprd: sprd_dispc_refresh_logo if type error\n!");
		return 0;
	}

	/* disable all interrupt */
	dispc_clear_bits(dev->dev_id, 0x1f, DISPC_INT_EN);
	/* clear all interruption */
	dispc_set_bits(dev->dev_id, 0x1f, DISPC_INT_CLR);
	/* update */
	dispc_set_bits(dev->dev_id, BIT(5), DISPC_DPI_CTRL);
	/* wait for update- done interruption */
	for (i = 0; i < 500; i++) {
		if (!(dispc_read(dev->dev_id, DISPC_INT_RAW) & (0x10)))
			udelay(1000);
		else
			break;
	}
	if (i >= 500)
		PERROR(" wait dispc done int time out!! (0x%x)\n",
		      dispc_read(dev->dev_id, DISPC_INT_RAW));
	dispc_set_bits(dev->dev_id, (1 << 5), DISPC_INT_CLR);
	return 0;
}

void sprd_dispc_logo_proc(struct sprd_device *dev)
{
	unsigned long logo_dst_p = 0;
	void *logo_src_v = NULL;

	/* use the second frame buffer  ,virtual */
	void *logo_dst_v = NULL;
	/* should be rgb565 */
	uint32_t logo_size = 0;
	struct panel_spec *panel;

	if (dev == NULL) {
		PERROR(" dev == NULL, return without process logo!!\n");
		return;
	}

	if (lcd_base_from_uboot == 0L) {
		PERROR("lcd_base_from_uboot == 0!!\n");
		return;
	}
	if (SPRDFB_PANEL_IF_DPI != dev->panel_if_type) {
		PERROR("not DPI mode , return\n");
		return;
	}
	panel = dev->intf->get_panel(dev->intf);

	/* should be rgb565 */
	logo_size = panel->width * panel->height * 2;
	dev->logo_buffer_size = logo_size;
	dev->logo_buffer_addr_v =
	    (void *)__get_free_pages(GFP_ATOMIC | __GFP_ZERO,
				     get_order(logo_size));
	if (!dev->logo_buffer_addr_v) {
		PERROR("Failed to allocate logo proc buffer\n");
		return;
	}
	PRINT("got %d bytes logo proc buffer at 0x%p\n", logo_size,
	      dev->logo_buffer_addr_v);

	logo_dst_v = dev->logo_buffer_addr_v;
	logo_dst_p = __pa(dev->logo_buffer_addr_v);

	logo_src_v = ioremap(lcd_base_from_uboot, logo_size);
	if (!logo_src_v || !logo_dst_v) {
		PERROR(" Unable to map boot logo memory:src-0x%p, dst-0x%p\n",
		      logo_src_v, logo_dst_v);
		return;
	}

	PRINT("lcd_base_from_uboot: 0x%lx, logo_src_v:0x%p\n",
	      lcd_base_from_uboot, logo_src_v);
	PRINT("logo_dst_p:0x%lx,logo_dst_v:0x%p\n", logo_dst_p, logo_dst_v);
	memcpy(logo_dst_v, logo_src_v, logo_size);
	dma_sync_single_for_device(dev->of_dev, logo_dst_p, logo_size,
				   DMA_TO_DEVICE);
	iounmap(logo_src_v);

	dispc_write(dev->dev_id, logo_dst_p, DISPC_OSD_BASE_ADDR);
	sprd_dispc_refresh_logo(dev);
}

struct display_ctrl sprd_dispc_ctrl = {
	.name = "dispc",
	.early_init = sprd_dispc_early_init,
	.init = sprd_dispc_init,
	.uninit = sprd_dispc_uninit,
	/* .flip         = sprd_dispc_refresh, */
	.logo_proc = sprd_dispc_logo_proc,
	.shutdown = sprd_dispc_shutdown,
	.suspend = sprd_dispc_suspend,
	.resume = sprd_dispc_resume,
	.update_clk = dispc_update_clk_intf,
	.wait_for_sync = dispc_sync,

};
