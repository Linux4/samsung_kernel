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

#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <mach/globalregs.h>
#include <mach/irqs.h>
#include <mach/sci.h>
#include "sprdfb_dispc_reg.h"
#include "sprdfb_lcdc_reg.h"
#include "sprdfb.h"


#define LCDC_SOFT_RST (3)

#define LCDC_CORE_CLK_EN (9)


struct sprdfb_lcdc_context {
	struct clk		*clk_lcdc;
	bool			is_inited;
	bool			is_first_frame;

	struct sprdfb_device	*dev;

	uint32_t	 	vsync_waiter;
	wait_queue_head_t		vsync_queue;
	uint32_t	        vsync_done;
};

static struct sprdfb_lcdc_context lcdc_ctx = {0};

extern struct display_ctrl sprdfb_dispc_ctrl;

extern void sprdfb_panel_suspend(struct sprdfb_device *dev);
extern void sprdfb_panel_resume(struct sprdfb_device *dev, bool from_deep_sleep);
extern void sprdfb_panel_before_refresh(struct sprdfb_device *dev);
extern void sprdfb_panel_after_refresh(struct sprdfb_device *dev);
extern void sprdfb_panel_invalidate(struct panel_spec *self);
extern void sprdfb_panel_invalidate_rect(struct panel_spec *self,
		uint16_t left, uint16_t top,
		uint16_t right, uint16_t bottom);


static irqreturn_t lcdc_isr(int irq, void *data)
{
	struct sprdfb_lcdc_context *lcdc_ctx = (struct sprdfb_lcdc_context *)data;
	uint32_t reg_val;
	struct sprdfb_device *dev = lcdc_ctx->dev;

	reg_val = lcdc_read(LCDC_INT_STATUS);

	if (reg_val & 1) { /* lcdc done isr */
		lcdc_write(1, LCDC_INT_CLR);

		lcdc_ctx->vsync_done = 1;
		if (lcdc_ctx->vsync_waiter) {
			wake_up_interruptible_all(&(lcdc_ctx->vsync_queue));
			lcdc_ctx->vsync_waiter = 0;
		}
		sprdfb_panel_after_refresh(dev);
		pr_debug(KERN_INFO "sprdfb: [%s]: Done INT !\n", __FUNCTION__);
	}

	return IRQ_HANDLED;
}


/* lcdc soft reset */
static void lcdc_reset(void)
{
#define REG_AHB_SOFT_RST (AHB_SOFT_RST + SPRD_AHB_BASE)
	sci_glb_set(REG_AHB_SOFT_RST, (1<<LCDC_SOFT_RST));
	udelay(10);
	sci_glb_clr(REG_AHB_SOFT_RST, (1<<LCDC_SOFT_RST));
}

static inline void lcdc_set_bg_color(uint32_t bg_color)
{
	lcdc_write(bg_color, LCDC_BG_COLOR);
}

static inline void lcdc_set_osd_ck(uint32_t ck_color)
{
	lcdc_write(ck_color, LCDC_OSD1_CK);
}

static void lcdc_dithering_enable(bool enable)
{
	if(enable){
		lcdc_set_bits(BIT(4), LCDC_CTRL);
	}else{
		lcdc_clear_bits(BIT(4), LCDC_CTRL);
	}
}

static void lcdc_set_exp_mode(uint16_t exp_mode)
{
	uint32_t reg_val = lcdc_read(LCDC_CTRL);

	reg_val &= ~(0x3 << 16);
	reg_val |= (exp_mode << 16);
	lcdc_write(reg_val, LCDC_CTRL);
}

static void lcdc_module_enable(void)
{
	/*lcdc module enable */
	lcdc_write((1<<0), LCDC_CTRL);

	/*lcdc dispc INT*/
	lcdc_write(0x0, LCDC_INT_EN);

	/* clear lcdc INT */
	lcdc_write(0x3, LCDC_INT_CLR);
}

static inline int32_t  lcdc_set_disp_size(struct fb_var_screeninfo *var)
{
	uint32_t reg_val;

	reg_val = (var->xres & 0xfff) | ((var->yres & 0xfff ) << 16);
	lcdc_write(reg_val, LCDC_DISP_SIZE);

	return 0;
}

static inline int32_t lcdc_set_lcm_size(struct fb_var_screeninfo *var)
{
	uint32_t reg_val;

	lcdc_write(0, LCDC_LCM_START);

	reg_val = (var->xres & 0xfff) | ((var->yres & 0xfff ) << 16);
	lcdc_write(reg_val, LCDC_LCM_SIZE);

	return 0;
}


static void lcdc_layer_init(struct fb_var_screeninfo *var)
{
	uint32_t reg_val = 0;

	lcdc_clear_bits((1<<0),LCDC_IMG_CTRL);
	lcdc_clear_bits((1<<0),LCDC_OSD1_CTRL);
	lcdc_clear_bits((1<<0),LCDC_OSD2_CTRL);
	lcdc_clear_bits((1<<0),LCDC_OSD3_CTRL);
	lcdc_clear_bits((1<<0),LCDC_OSD4_CTRL);
	lcdc_clear_bits((1<<0),LCDC_OSD5_CTRL);

	/******************* OSD1 layer setting **********************/

	/*enable OSD1 layer*/
	reg_val |= (1 << 0);

	/*disable  color key */

	/* alpha mode select  - block alpha*/
	reg_val |= (1 << 2);

	/* data format */
	if (var->bits_per_pixel == 32) {
		/* ABGR */
		reg_val |= (3 << 3);
		/* rb switch */
		reg_val |= (1 << 9);
	} else {
		/* RGB565 */
		reg_val |= (5 << 3);
		/* B2B3B0B1 */
		reg_val |= (2 <<7);
	}

	lcdc_write(reg_val, LCDC_OSD1_CTRL);

	/* OSD1 layer alpha value */
	lcdc_write(0xff, LCDC_OSD1_ALPHA);

	/* alpha base addr */

	/* OSD1 layer size */
	reg_val = ( var->xres & 0xfff) | (( var->yres & 0xfff ) << 16);
	lcdc_write(reg_val, LCDC_OSD1_SIZE_XY);

	/* OSD1 layer start position */
	lcdc_write(0, LCDC_OSD1_DISP_XY);

	/* OSD1 layer pitch */
	reg_val = ( var->xres & 0xfff) ;
	lcdc_write(reg_val, LCDC_OSD1_PITCH);

	/* OSD1 color_key value */
	lcdc_set_osd_ck(0x0);

	/* OSD1 grey RGB */

	/* LCDC workplane size */
	lcdc_set_disp_size(var);

	/*LCDC LCM rect size */
	lcdc_set_lcm_size(var);
}

static void lcdc_layer_update(struct fb_var_screeninfo *var)
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
		reg_val |= (3 << 3);
		/* rb switch */
		reg_val |= (1 << 9);
	} else {
		/* RGB565 */
		reg_val |= (5 << 3);
		/* B2B3B0B1 */
		reg_val |= (2 <<7);
	}

	lcdc_write(reg_val, LCDC_OSD1_CTRL);
}


static int32_t lcdc_sync(struct sprdfb_device *dev)
{
	int ret;

	if (dev->enable == 0) {
		printk("sprdfb: lcdc_sync fb suspeneded already!!\n");
		return -1;
	}

	ret = wait_event_interruptible_timeout(lcdc_ctx.vsync_queue,
			lcdc_ctx.vsync_done, msecs_to_jiffies(100));

	if (!ret) { /* time out */
		lcdc_ctx.vsync_done = 1; /*error recovery */
		printk(KERN_ERR "sprdfb: lcdc_sync time out!!!!!\n");
		return -1;
	}
	return 0;
}

static int32_t sprdfb_lcdc_early_init(struct sprdfb_device *dev)
{
	int ret = 0;

	pr_debug(KERN_INFO "sprdfb: [%s]\n", __FUNCTION__);

	if(lcdc_ctx.is_inited){
		printk(KERN_WARNING "sprdfb: lcdc early init warning!(has been inited)");
		return 0;
	}

	lcdc_ctx.clk_lcdc = clk_get(NULL, "clk_lcd");
	if (IS_ERR(lcdc_ctx.clk_lcdc)) {
		printk(KERN_WARNING "sprdfb: get clk_lcd fail!\n");
		return 0;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_lcd ok!\n");
	}

	/*usesd to open dipsc matix clock*/
	sci_glb_set(REG_AHB_MATRIX_CLOCK, (1<<LCDC_CORE_CLK_EN));

	if(!dev->panel_ready){
		ret = clk_enable(lcdc_ctx.clk_lcdc);
		if (ret) {
			printk(KERN_WARNING "sprdfb: enable clk_lcdc fail!\n");
			return 0;
		} else {
			pr_debug(KERN_INFO "sprdfb: get clk_lcdc ok!\n");
		}

		/*dispc must be enbale before lcdc enable*/
		sprdfb_dispc_ctrl.early_init(dev);
		dispc_set_bits(BIT(3), DISPC_CTRL);

		lcdc_reset();
		lcdc_module_enable();
		lcdc_ctx.is_first_frame = true;
	}else{
		lcdc_ctx.is_first_frame = false;
	}

	lcdc_ctx.vsync_done = 1;
	lcdc_ctx.vsync_waiter = 0;
	init_waitqueue_head(&(lcdc_ctx.vsync_queue));

	lcdc_ctx.is_inited = true;

	ret = request_irq(IRQ_LCDC_INT, lcdc_isr, IRQF_DISABLED, "LCDC", &lcdc_ctx);
	if (ret) {
		printk(KERN_ERR "sprdfb: lcdc failed to request irq!\n");
		clk_disable(lcdc_ctx.clk_lcdc);
		lcdc_ctx.is_inited = false;
		return -1;
	}

	return 0;
}


static int32_t sprdfb_lcdc_init(struct sprdfb_device *dev)
{
	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);

	lcdc_set_bg_color(0xFFFFFFFF);
	/*enable dithering*/
	lcdc_dithering_enable(true);
	/*use MSBs as img exp mode*/
	lcdc_set_exp_mode(0x0);
	if(lcdc_ctx.is_first_frame){
		lcdc_layer_init(&(dev->fb->var));
	}else{
		lcdc_layer_update(&(dev->fb->var));
	}

	/* enable lcdc DONE  INT*/
	lcdc_write((1<<0), LCDC_INT_EN);
	dev->enable = 1;
	return 0;
}

static int32_t sprdfb_lcdc_uninit(struct sprdfb_device *dev)
{
	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);

	dev->enable = 0;
	if(lcdc_ctx.is_inited){
		clk_disable(lcdc_ctx.clk_lcdc);
		lcdc_ctx.is_inited = false;
	}
	return 0;
}

static int32_t sprdfb_lcdc_refresh (struct sprdfb_device *dev)
{
	struct fb_info *fb = dev->fb;

	uint32_t base = fb->fix.smem_start + fb->fix.line_length * fb->var.yoffset;

	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);

	lcdc_ctx.vsync_waiter ++;
	lcdc_sync(dev);
	pr_debug(KERN_INFO "srpdfb: [%s] got sync\n", __FUNCTION__);

	lcdc_ctx.dev = dev;
	lcdc_ctx.vsync_done = 0;

#ifdef LCD_UPDATE_PARTLY
	if (fb->var.reserved[0] == 0x6f766572) {
		uint32_t x,y, width, height;

		x = fb->var.reserved[1] & 0xffff;
		y = fb->var.reserved[1] >> 16;
		width  = fb->var.reserved[2] &  0xffff;
		height = fb->var.reserved[2] >> 16;

		base += ((x + y * fb->var.xres) * fb->var.bits_per_pixel / 8);
		lcdc_write(base, LCDC_OSD1_BASE_ADDR);
		lcdc_write(0, LCDC_OSD1_DISP_XY);
		lcdc_write(fb->var.reserved[2], LCDC_OSD1_SIZE_XY);
		lcdc_write(fb->var.xres, LCDC_OSD1_PITCH);

		lcdc_write(fb->var.reserved[2], LCDC_DISP_SIZE);
		lcdc_write(0, LCDC_LCM_START);
		lcdc_write(fb->var.reserved[2], LCDC_LCM_SIZE);

		sprdfb_panel_invalidate_rect(dev->panel,
				x, y, x+width-1, y+height-1);
	} else
#endif
	{
		uint32_t size = (fb->var.xres & 0xffff) | ((fb->var.yres) << 16);

		lcdc_write(base, LCDC_OSD1_BASE_ADDR);
		lcdc_write(0, LCDC_OSD1_DISP_XY);
		lcdc_write(size, LCDC_OSD1_SIZE_XY);
		lcdc_write(fb->var.xres, LCDC_OSD1_PITCH);

		lcdc_write(size, LCDC_DISP_SIZE);
		lcdc_write(0, LCDC_LCM_START);
		lcdc_write(size, LCDC_LCM_SIZE);

		sprdfb_panel_invalidate(dev->panel);
	}

	sprdfb_panel_before_refresh(dev);

	/* start refresh */
	lcdc_set_bits((1 << 3), LCDC_CTRL);

	pr_debug("LCDC_CTRL: 0x%x\n", lcdc_read(LCDC_CTRL));
	pr_debug("LCDC_DISP_SIZE: 0x%x\n", lcdc_read(LCDC_DISP_SIZE));
	pr_debug("LCDC_LCM_START: 0x%x\n", lcdc_read(LCDC_LCM_START));
	pr_debug("LCDC_LCM_SIZE: 0x%x\n", lcdc_read(LCDC_LCM_SIZE));
	pr_debug("LCDC_BG_COLOR: 0x%x\n", lcdc_read(LCDC_BG_COLOR));
	pr_debug("LCDC_FIFO_STATUS: 0x%x\n", lcdc_read(LCDC_FIFO_STATUS));

	pr_debug("LCM_CTRL: 0x%x\n", lcdc_read(LCM_CTRL));
	pr_debug("LCM_TIMING0: 0x%x\n", lcdc_read(LCM_TIMING0));
	pr_debug("LCM_RDDATA: 0x%x\n", lcdc_read(LCM_RDDATA));

	pr_debug("LCDC_IRQ_EN: 0x%x\n", lcdc_read(LCDC_INT_EN));

	pr_debug("LCDC_OSD1_CTRL: 0x%x\n", lcdc_read(LCDC_OSD1_CTRL));
	pr_debug("LCDC_OSD1_BASE_ADDR: 0x%x\n", lcdc_read(LCDC_OSD1_BASE_ADDR));
	pr_debug("LCDC_OSD1_ALPHA_BASE_ADDR: 0x%x\n", lcdc_read(LCDC_OSD1_ALPHA_BASE_ADDR));
	pr_debug("LCDC_OSD1_SIZE_XY: 0x%x\n", lcdc_read(LCDC_OSD1_SIZE_XY));
	pr_debug("LCDC_OSD1_PITCH: 0x%x\n", lcdc_read(LCDC_OSD1_PITCH));
	pr_debug("LCDC_OSD1_DISP_XY: 0x%x\n", lcdc_read(LCDC_OSD1_DISP_XY));
	pr_debug("LCDC_OSD1_ALPHA	: 0x%x\n", lcdc_read(LCDC_OSD1_ALPHA));
	return 0;
}

static int32_t sprdfb_lcdc_suspend(struct sprdfb_device *dev)
{
	printk(KERN_INFO "sprdfb: [%s], dev->enable = %d\n",__FUNCTION__, dev->enable);

	if (0 != dev->enable){
		/* must wait ,lcdc_sync() */
		lcdc_ctx.vsync_waiter ++;
		lcdc_sync(dev);
		printk(KERN_INFO "sprdfb: [%s] got sync\n",__FUNCTION__);

		sprdfb_panel_suspend(dev);

		dev->enable = 0;
		clk_disable(lcdc_ctx.clk_lcdc);
	}else{
		printk(KERN_ERR "sprdfb: [%s]: Invalid device status %d\n", __FUNCTION__, dev->enable);
	}
	return 0;
}

static int32_t sprdfb_lcdc_resume(struct sprdfb_device *dev)
{
	printk(KERN_INFO "sprdfb: [%s], dev->enable= %d\n",__FUNCTION__, dev->enable);

	if (dev->enable == 0) {
		clk_enable(lcdc_ctx.clk_lcdc);
		lcdc_ctx.vsync_done = 1;
		if (0 == lcdc_read(LCDC_CTRL)) { /* resume from deep sleep */
			lcdc_reset();
			lcdc_module_enable();
			sprdfb_lcdc_init(dev);

			sprdfb_panel_resume(dev, true);
		} else {
			sprdfb_panel_resume(dev, false);
		}

		dev->enable = 1;
	}
	return 0;
}




struct display_ctrl sprdfb_lcdc_ctrl = {
	.name		= "lcdc",
	.early_init		= sprdfb_lcdc_early_init,
	.init		 	= sprdfb_lcdc_init,
	.uninit		= sprdfb_lcdc_uninit,
	.refresh		= sprdfb_lcdc_refresh,
	.suspend		= sprdfb_lcdc_suspend,
	.resume		= sprdfb_lcdc_resume,
};
