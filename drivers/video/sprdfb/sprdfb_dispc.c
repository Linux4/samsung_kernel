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
#define pr_fmt(fmt) "sprdfb: " fmt

#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <linux/delay.h>
#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || defined(CONFIG_SPRD_SCX35_DMC_FREQ))
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
#ifdef CONFIG_CPU_IDLE
#include <soc/sprd/cpuidle.h>
#endif
#include <linux/dma-mapping.h>
//#define SHARK_LAYER_COLOR_SWITCH_FEATURE // bug212892

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
//#define DISPC_CLOCK (192*1000000)
#define DISPC_DBI_CLOCK_PARENT ("dispc_dbi_clk_parent")
//#define DISPC_DBI_CLOCK (256*1000000)
#define DISPC_DPI_CLOCK_PARENT ("dispc_dpi_clk_parent")
//#define SPRDFB_DPI_CLOCK_SRC (192000000)
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

#if (defined CONFIG_FB_SCX35L)

#define SPRDFB_BRIGHTNESS           (0x00<<16)//(0x03<<16)// 9-bits
#define SPRDFB_CONTRAST             (0x100<<0) //10-bits
#define SPRDFB_OFFSET_U             (0x81<<16)//8-bits
#define SPRDFB_SATURATION_U         (0x100<<0)//(0x12A<<0)//10-bits
#define SPRDFB_OFFSET_V             (0x82<<16)//8-bits
#define SPRDFB_SATURATION_V         (0x100<<0)//(0x12A<<0)//10-bits

#elif ((defined CONFIG_FB_SCX15) || (defined CONFIG_FB_SCX30G))
#if ((defined CONFIG_MACH_J1X3G) || (defined CONFIG_MACH_J1MINI3G))
#define SPRDFB_BRIGHTNESS           (0x00<<16)//(0x03<<16)// 9-bits
#define SPRDFB_CONTRAST             (0x100<<0) //10-bits
#define SPRDFB_OFFSET_U             (0x81<<16)//8-bits
#define SPRDFB_SATURATION_U         (0x100<<0)//(0x12A<<0)//10-bits
#define SPRDFB_OFFSET_V             (0x82<<16)//8-bits
#define SPRDFB_SATURATION_V         (0x100<<0)//(0x12A<<0)//10-bits
#else
#define SPRDFB_BRIGHTNESS           (0x02<<16)//(0x03<<16)// 9-bits
#define SPRDFB_CONTRAST             (0x12A<<0) //10-bits
#define SPRDFB_OFFSET_U             (0x81<<16)//8-bits
#define SPRDFB_SATURATION_U         (0x123<<0)//(0x12A<<0)//10-bits
#define SPRDFB_OFFSET_V             (0x82<<16)//8-bits
#define SPRDFB_SATURATION_V         (0x123<<0)//(0x12A<<0)//10-bits
#endif
#else
#define SPRDFB_CONTRAST (74)
#define SPRDFB_SATURATION (73)
#define SPRDFB_BRIGHTNESS (2)
#endif

typedef enum
{
   SPRDFB_DYNAMIC_CLK_FORCE,		//force enable/disable
   SPRDFB_DYNAMIC_CLK_REFRESH,		//enable for refresh/display_overlay
   SPRDFB_DYNAMIC_CLK_COUNT,		//enable disable in pairs
   SPRDFB_DYNAMIC_CLK_MAX,
} SPRDFB_DYNAMIC_CLK_SWITCH_E;

struct sprdfb_dispc_context {
	struct clk		*clk_dispc;
	struct clk 		*clk_dispc_dpi;
	struct clk 		*clk_dispc_dbi;
	struct clk 		*clk_dispc_emc;
	bool			is_inited;
	bool			is_first_frame;
	bool			clk_is_open;
	bool			clk_is_refreshing;
	int				clk_open_count;
	//spinlock_t clk_spinlock;
	struct semaphore clk_sem;
	struct sprdfb_device	*dev;

	uint32_t	 	vsync_waiter;
	wait_queue_head_t		vsync_queue;
	uint32_t	        vsync_done;

#ifdef  CONFIG_FB_LCD_OVERLAY_SUPPORT
	/* overlay */
	uint32_t  overlay_state;  /*0-closed, 1-configed, 2-started*/
//	struct semaphore   overlay_lock;
#endif

#ifdef CONFIG_FB_VSYNC_SUPPORT
	uint32_t	 	waitfor_vsync_waiter;
	wait_queue_head_t		waitfor_vsync_queue;
	uint32_t	        waitfor_vsync_done;
#endif
#ifdef CONFIG_FB_MMAP_CACHED
	struct vm_area_struct *vma;
#endif
};
static DEFINE_MUTEX(dispc_lock);
static struct sprdfb_dispc_context dispc_ctx = {0};
#ifdef CONFIG_OF
unsigned long g_dispc_base_addr = 0;
EXPORT_SYMBOL(g_dispc_base_addr);
#endif

extern void sprdfb_panel_start(struct sprdfb_device *dev);
extern void sprdfb_panel_suspend(struct sprdfb_device *dev);
extern void sprdfb_panel_shutdown(struct sprdfb_device *dev);
extern void sprdfb_panel_resume(struct sprdfb_device *dev, bool from_deep_sleep);
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

static int sprdfb_dispc_clk_disable(struct sprdfb_dispc_context *dispc_ctx_ptr, SPRDFB_DYNAMIC_CLK_SWITCH_E clock_switch_type);
static int sprdfb_dispc_clk_enable(struct sprdfb_dispc_context *dispc_ctx_ptr, SPRDFB_DYNAMIC_CLK_SWITCH_E clock_switch_type);
static int32_t sprdfb_dispc_init(struct sprdfb_device *dev);
static void dispc_reset(void);
void dispc_stop(struct sprdfb_device *dev);
static void dispc_module_enable(void);
//#if (defined(CONFIG_FB_DYNAMIC_FREQ_SCALING) || !defined(FB_CHECK_ESD_IN_VFP))
static void dispc_stop_for_feature(struct sprdfb_device *dev);
static void dispc_run_for_feature(struct sprdfb_device *dev);
#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || defined(CONFIG_SPRD_SCX35_DMC_FREQ))
static unsigned int sprdfb_dispc_change_threshold(struct devfreq_dbs *h, unsigned int state);
static unsigned int sprdfb_dispc_threshold_check(void);
#endif

static int dispc_update_clk(struct sprdfb_device *fb_dev,
					u32 new_val, int howto);

/**
 * author: Yang.Haibing haibing.yang@spreadtrum.com
 *
 * dispc_check_new_clk - check and convert new clock to the value that can be set
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
	struct panel_spec* panel = fb_dev->panel;
	struct info_mipi * mipi;
	struct info_rgb* rgb;

	pr_debug("%s: enter\n", __func__);
	if(!panel){
		pr_err("No panel is specified!\n");
		return -ENXIO;
	}

	mipi = panel->info.mipi;
	rgb = panel->info.rgb;

	if (fb_dev->panel_if_type != SPRDFB_PANEL_IF_DPI) {
		pr_err("panel interface should be DPI\n");
		return -EINVAL;
	}
	if (new_val <= 0 || !new_pclk) {
		pr_err("new parameter is invalid\n");
		return -EINVAL;
	}

	if (fb_dev->panel->type == LCD_MODE_DSI) {
		hpixels = panel->width + mipi->timing->hsync +
				mipi->timing->hbp + mipi->timing->hfp;
		vlines = panel->height + mipi->timing->vsync +
				mipi->timing->vbp + mipi->timing->vfp;
	} else if(fb_dev->panel->type == LCD_MODE_RGB ||
			fb_dev->panel->type == LCD_MODE_LVDS) {
		hpixels = panel->width + rgb->timing->hsync +
				rgb->timing->hbp + rgb->timing->hfp;
		vlines = panel->height + rgb->timing->vsync +
				rgb->timing->vbp + rgb->timing->vfp;
	} else {
		pr_err("[%s] unexpected panel type (%d)\n",
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
			pr_err("Unsupported FPS. fps range should be [%d, %d]\n",
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
		fps = pclk / ( hpixels * vlines);
		if (fps < LCD_MIN_FPS || fps > LCD_MAX_FPS) {
			pr_err("Unsupported FPS. fps range should be [%d, %d]\n",
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

void dispc_irq_trick(struct sprdfb_device *dev)
{
    if(NULL == dev){
        return;
    }

    if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
        dispc_set_bits(DISPC_INT_ERR_MASK, DISPC_INT_EN);
    }
}

extern void dsi_irq_trick(void);

//static uint32_t underflow_ever_happened = 0;
static irqreturn_t dispc_isr(int irq, void *data)
{
	struct sprdfb_dispc_context *dispc_ctx = (struct sprdfb_dispc_context *)data;
	uint32_t reg_val;
	struct sprdfb_device *dev = dispc_ctx->dev;
	bool done = false;
#ifdef CONFIG_FB_VSYNC_SUPPORT
	bool vsync =  false;
#endif

	reg_val = dispc_read(DISPC_INT_STATUS);

	pr_debug("sprdfb: dispc_isr (0x%x)\n",reg_val );
	//printk("%s%d: underflow_ever_happened:0x%08x \n",__func__,__LINE__,underflow_ever_happened);
	//dispc_irq_trick_in(reg_val);

	if(reg_val & DISPC_INT_ERR_MASK){
		printk("sprdfb: Warning: dispc underflow (0x%x)!\n",reg_val);
		//underflow_ever_happened++;
		dispc_write(DISPC_INT_ERR_MASK, DISPC_INT_CLR);
		/*disable err interupt*/
		dispc_clear_bits(DISPC_INT_ERR_MASK, DISPC_INT_EN);
	}

	if(NULL == dev){
		return IRQ_HANDLED;
	}

	if((reg_val & 0x10) && (SPRDFB_PANEL_IF_DPI ==  dev->panel_if_type)){/*dispc update done isr*/
#if 0
		if(dispc_ctx->is_first_frame){
			/*dpi register update with SW and VSync*/
			dispc_clear_bits(BIT(4), DISPC_DPI_CTRL);

			/* start refresh */
			dispc_set_bits((1 << 4), DISPC_CTRL);

			dispc_ctx->is_first_frame = false;
		}
#endif
		dispc_write(DISPC_INT_UPDATE_DONE_MASK, DISPC_INT_CLR);
		done = true;
	}else if ((reg_val & DISPC_INT_DONE_MASK) && (SPRDFB_PANEL_IF_DPI !=  dev->panel_if_type)){ /* dispc done isr */
			dispc_write(DISPC_INT_DONE_MASK, DISPC_INT_CLR);
			dispc_ctx->is_first_frame = false;
			done = true;
	}
#ifdef CONFIG_FB_ESD_SUPPORT
#ifdef FB_CHECK_ESD_BY_TE_SUPPORT
	if((reg_val & DISPC_INT_TE_MASK) && (SPRDFB_PANEL_IF_DPI ==  dev->panel_if_type)){ /*dispc external TE isr*/
		dispc_write(DISPC_INT_TE_MASK, DISPC_INT_CLR);
		if(0 != dev->esd_te_waiter){
			printk("sprdfb: dispc_isr esd_te_done!");
			dev->esd_te_done =1;
			wake_up_interruptible_all(&(dev->esd_te_queue));
			dev->esd_te_waiter = 0;
		}
	}
#endif
#endif

#ifdef CONFIG_FB_VSYNC_SUPPORT
	if((reg_val & DISPC_INT_HWVSYNC) && (SPRDFB_PANEL_IF_DPI ==  dev->panel_if_type)){/*dispc done isr*/
		dispc_write(DISPC_INT_HWVSYNC, DISPC_INT_CLR);
		vsync = true;
	}else if((reg_val & DISPC_INT_TE_MASK) && (SPRDFB_PANEL_IF_EDPI ==  dev->panel_if_type)){ /*dispc te isr*/
		dispc_write(DISPC_INT_TE_MASK, DISPC_INT_CLR);
		vsync = true;
	}
	if(vsync){
		//printk("sprdfb: got vsync!\n");
		dispc_ctx->waitfor_vsync_done = 1;
		if(dispc_ctx->waitfor_vsync_waiter){
			//printk("sprdfb: wake!\n");
			wake_up_interruptible_all(&(dispc_ctx->waitfor_vsync_queue));
		}
	}
#endif

	if(done){
		dispc_ctx->vsync_done = 1;

#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if(SPRDFB_PANEL_IF_DPI !=  dev->panel_if_type){
			sprdfb_dispc_clk_disable(dispc_ctx,SPRDFB_DYNAMIC_CLK_REFRESH);
		}
#endif

		if (dispc_ctx->vsync_waiter) {
			wake_up_interruptible_all(&(dispc_ctx->vsync_queue));
			dispc_ctx->vsync_waiter = 0;
		}
		sprdfb_panel_after_refresh(dev);
		pr_debug(KERN_INFO "sprdfb: [%s]: Done INT, reg_val = %d !\n", __FUNCTION__, reg_val);
	}

	return IRQ_HANDLED;
}


/* dispc soft reset */
static void dispc_reset(void)
{
	sci_glb_set(REG_AHB_SOFT_RST, (BIT_DISPC_SOFT_RST) );
 	udelay(10);
	sci_glb_clr(REG_AHB_SOFT_RST, (BIT_DISPC_SOFT_RST) );
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
	if(is_enable){
		reg_val = reg_val | (BIT(0));
	}
	else{
		reg_val = reg_val & (~(BIT(0)));
	}
	dispc_write(reg_val, DISPC_OSD_CTRL);
}

static inline void dispc_set_osd_alpha(uint8_t alpha)
{
	dispc_write(alpha, DISPC_OSD_ALPHA);
}

static void dispc_dithering_enable(bool enable)
{
	if(enable){
		dispc_set_bits(BIT(6), DISPC_CTRL);
	}else{
		dispc_clear_bits(BIT(6), DISPC_CTRL);
	}
}

static void dispc_pwr_enable(bool enable)
{
	if(enable){
		dispc_set_bits(BIT(7), DISPC_CTRL);
	}else{
		dispc_clear_bits(BIT(7), DISPC_CTRL);
	}
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

static inline int32_t  dispc_set_disp_size(struct fb_var_screeninfo *var)
{
	uint32_t reg_val;

	reg_val = (var->xres & 0xfff) | ((var->yres & 0xfff ) << 16);
	dispc_write(reg_val, DISPC_SIZE_XY);

	return 0;
}

static void dispc_layer_init(struct fb_var_screeninfo *var)
{
	uint32_t reg_val = 0;

//	dispc_clear_bits((1<<0),DISPC_IMG_CTRL);
	dispc_write(0x0, DISPC_IMG_CTRL);
	dispc_clear_bits((1<<0),DISPC_OSD_CTRL);

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

	/* OSD layer alpha value */
	dispc_set_osd_alpha(0xff);

	/* OSD layer size */
	reg_val = ( var->xres & 0xfff) | (( var->yres & 0xfff ) << 16);
	dispc_write(reg_val, DISPC_OSD_SIZE_XY);

	/* OSD layer start position */
	dispc_write(0, DISPC_OSD_DISP_XY);

	/* OSD layer pitch */
	reg_val = ( var->xres & 0xfff) ;
	dispc_write(reg_val, DISPC_OSD_PITCH);

	/* OSD color_key value */
	dispc_set_osd_ck(0x0);
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
		printk("sprdfb: dispc_sync fb suspeneded already!!\n");
		return -1;
	}

#ifdef CONFIG_SC_FPGA
	ret = wait_event_interruptible_timeout(dispc_ctx.vsync_queue,
			          dispc_ctx.vsync_done, msecs_to_jiffies(500));
#else
	ret = wait_event_interruptible_timeout(dispc_ctx.vsync_queue,
			          dispc_ctx.vsync_done, msecs_to_jiffies(100));
#endif

	if (!ret) { /* time out */
		dispc_ctx.vsync_done = 1; /*error recovery */
		printk(KERN_ERR "sprdfb: dispc_sync time out!!!!!\n");
		{/*for debug*/
			int32_t i = 0;
			for(i=0;i<256;i+=16){
				printk("sprdfb: %x: 0x%x, 0x%x, 0x%x, 0x%x\n", i, dispc_read(i), dispc_read(i+4), dispc_read(i+8), dispc_read(i+12));
			}
			printk("**************************************\n");
		}

		return -1;
	}
	return 0;
}


static void dispc_run(struct sprdfb_device *dev)
{
	if(0 == dev->enable){
		return;
	}

	if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
		if(!dispc_ctx.is_first_frame){
			dispc_ctx.vsync_done = 0;
			dispc_ctx.vsync_waiter ++;
		}
		/*dpi register update*/
		dispc_set_bits(BIT(5), DISPC_DPI_CTRL);
		udelay(30);

		if(dispc_ctx.is_first_frame){
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
            if(SPRD_OVERLAY_STATUS_STARTED == dispc_ctx.overlay_state){
                dispc_sync(dev);
            }
#endif
#endif
		}
	}else{
		dispc_ctx.vsync_done = 0;
		/* start refresh */
		dispc_set_bits((1 << 4), DISPC_CTRL);
	}
	dispc_irq_trick(dev);
	//dispc_irq_trick_out();

#if !defined(CONFIG_FB_SCX15) && !defined(CONFIG_SC_FPGA)
	//dsi_irq_trick(0,0);
	dsi_irq_trick();
#endif
}

void dispc_stop(struct sprdfb_device *dev)
{
	if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
		/*dpi register update with SW only*/
		dispc_set_bits(BIT(4), DISPC_DPI_CTRL);

		/* stop refresh */
		dispc_clear_bits((1 << 4), DISPC_CTRL);

		dispc_ctx.is_first_frame = true;
	}
}

static int32_t sprdfb_dispc_uninit(struct sprdfb_device *dev)
{
	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);

	dev->enable = 0;
	sprdfb_dispc_clk_disable(&dispc_ctx,SPRDFB_DYNAMIC_CLK_FORCE);

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

	pr_debug(KERN_INFO "sprdfb: [%s]\n", __FUNCTION__);

	sci_glb_set(DISPC_CORE_EN, BIT_DISPC_CORE_EN);
//	sci_glb_set(DISPC_EMC_EN, BIT_DISPC_EMC_EN);

#ifdef CONFIG_OF
	ret = of_property_read_u32_array(dev->of_dev->of_node, "clock-src", clk_src, 3);
	if(0 != ret){
		printk("sprdfb: read clock-src fail (%d)\n", ret);
		return -1;
	}

	ret = of_property_read_u32(dev->of_dev->of_node, "dpi_clk_div", &def_dpi_clk_div);
	if(0 != ret){
		printk("sprdfb: read dpi_clk_div fail (%d)\n", ret);
		return -1;
	}
	clk_parent1 = of_clk_get_by_name(dev->of_dev->of_node, DISPC_CLOCK_PARENT);
	dpi_clk = clk_src[DISPC_DPI_CLOCK_SRC_ID]/def_dpi_clk_div;
#else
	dpi_clk = DISPC_DPI_CLOCK;
	clk_parent1 = clk_get(NULL, DISPC_CLOCK_PARENT);
#endif
	if (IS_ERR(clk_parent1)) {
		printk(KERN_WARNING "sprdfb: get clk_parent1 fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_parent1 ok!\n");
	}

#ifdef CONFIG_OF
	clk_parent2 = of_clk_get_by_name(dev->of_dev->of_node, DISPC_DBI_CLOCK_PARENT);
#else
	clk_parent2 = clk_get(NULL, DISPC_DBI_CLOCK_PARENT);
#endif
	if (IS_ERR(clk_parent2)) {
		printk(KERN_WARNING "sprdfb: get clk_parent2 fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_parent2 ok!\n");
	}

#ifdef CONFIG_OF
	clk_parent3 = of_clk_get_by_name(dev->of_dev->of_node, DISPC_DPI_CLOCK_PARENT);
#else
	clk_parent3 = clk_get(NULL, DISPC_DPI_CLOCK_PARENT);
#endif
	if (IS_ERR(clk_parent3)) {
		printk(KERN_WARNING "sprdfb: get clk_parent3 fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_parent3 ok!\n");
	}

#ifdef CONFIG_OF
	clk_parent4 = of_clk_get_by_name(dev->of_dev->of_node, DISPC_EMC_EN_PARENT);
#else
	clk_parent4 = clk_get(NULL, DISPC_EMC_EN_PARENT);
#endif
	if (IS_ERR(clk_parent3)) {
		printk(KERN_WARNING "sprdfb: get clk_parent4 fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_parent4 ok!\n");
	}

#ifdef CONFIG_OF
	dispc_ctx.clk_dispc = of_clk_get_by_name(dev->of_dev->of_node, DISPC_PLL_CLK);
#else
	dispc_ctx.clk_dispc = clk_get(NULL, DISPC_PLL_CLK);
#endif
	if (IS_ERR(dispc_ctx.clk_dispc)) {
		printk(KERN_WARNING "sprdfb: get clk_dispc fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_dispc ok!\n");
	}

#ifdef CONFIG_OF
	dispc_ctx.clk_dispc_dbi = of_clk_get_by_name(dev->of_dev->of_node, DISPC_DBI_CLK);
#else
	dispc_ctx.clk_dispc_dbi = clk_get(NULL, DISPC_DBI_CLK);
#endif
	if (IS_ERR(dispc_ctx.clk_dispc_dbi)) {
		printk(KERN_WARNING "sprdfb: get clk_dispc_dbi fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_dispc_dbi ok!\n");
	}

#ifdef CONFIG_OF
	dispc_ctx.clk_dispc_dpi = of_clk_get_by_name(dev->of_dev->of_node, DISPC_DPI_CLK);
#else
	dispc_ctx.clk_dispc_dpi = clk_get(NULL, DISPC_DPI_CLK);
#endif
	if (IS_ERR(dispc_ctx.clk_dispc_dpi)) {
		printk(KERN_WARNING "sprdfb: get clk_dispc_dpi fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_dispc_dpi ok!\n");
	}

#ifdef CONFIG_OF
	dispc_ctx.clk_dispc_emc = of_clk_get_by_name(dev->of_dev->of_node, DISPC_EMC_CLK);
#else
	dispc_ctx.clk_dispc_emc = clk_get(NULL, DISPC_EMC_CLK);
#endif
	if (IS_ERR(dispc_ctx.clk_dispc_emc)) {
		printk(KERN_WARNING "sprdfb: get clk_dispc_dpi fail!\n");
		return -1;
	} else {
		pr_debug(KERN_INFO "sprdfb: get clk_dispc_emc ok!\n");
	}

	ret = clk_set_parent(dispc_ctx.clk_dispc, clk_parent1);
	if(ret){
		printk(KERN_ERR "sprdfb: dispc set clk parent fail\n");
	}
#ifdef CONFIG_OF
	ret = clk_set_rate(dispc_ctx.clk_dispc, clk_src[DISPC_CLOCK_SRC_ID]);
#else
	ret = clk_set_rate(dispc_ctx.clk_dispc, DISPC_CLOCK);
#endif
	if(ret){
		printk(KERN_ERR "sprdfb: dispc set clk parent fail\n");
	}

	ret = clk_set_parent(dispc_ctx.clk_dispc_dbi, clk_parent2);
	if(ret){
		printk(KERN_ERR "sprdfb: dispc set dbi clk parent fail\n");
	}
#ifdef CONFIG_OF
	ret = clk_set_rate(dispc_ctx.clk_dispc_dbi, clk_src[DISPC_DBI_CLOCK_SRC_ID]);
#else
	ret = clk_set_rate(dispc_ctx.clk_dispc_dbi, DISPC_DBI_CLOCK);
#endif
	if(ret){
		printk(KERN_ERR "sprdfb: dispc set dbi clk parent fail\n");
	}

	ret = clk_set_parent(dispc_ctx.clk_dispc_dpi, clk_parent3);
	if(ret){
		printk(KERN_ERR "sprdfb: dispc set dpi clk parent fail\n");
	}

	ret = clk_set_parent(dispc_ctx.clk_dispc_emc, clk_parent4);
	if(ret){
		printk(KERN_ERR "sprdfb: dispc set emc clk parent fail\n");
	}

	if (dev->panel && dev->panel->fps)
		dispc_update_clk(dev, dev->panel->fps, SPRDFB_FORCE_FPS);
	else
		dispc_update_clk(dev, dpi_clk, SPRDFB_FORCE_PCLK);

	ret = clk_prepare_enable(dispc_ctx.clk_dispc_emc);
	if(ret){
		printk("sprdfb: enable clk_dispc_emc error!!!\n");
	}

	ret = sprdfb_dispc_clk_enable(&dispc_ctx,SPRDFB_DYNAMIC_CLK_FORCE);
	if (ret) {
		printk(KERN_WARNING "sprdfb: [%s] enable dispc_clk fail!\n",__FUNCTION__);
		return -1;
	} else {
		pr_debug(KERN_INFO "sprdfb: [%s] enable dispc_clk ok!\n",__FUNCTION__);
	}

	dispc_print_clk();

	return 0;
}

#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || defined(CONFIG_SPRD_SCX35_DMC_FREQ))
struct devfreq_dbs sprd_fb_notify = {
	.level = 0,
	.data = &dispc_ctx,
	.devfreq_notifier = sprdfb_dispc_change_threshold,
};
#endif

#ifdef CONFIG_CPU_IDLE

static int scxx30_dispc_cpuidle_notify(struct notifier_block *nb, unsigned long event, void *dummy)
{
	if (event == SC_CPUIDLE_PREPARE){
		 if(0 == dispc_ctx.vsync_done
#ifdef CONFIG_FB_VSYNC_SUPPORT
		|| 0 == dispc_ctx.waitfor_vsync_done
#endif
)
		{
			return NOTIFY_BAD;  //work now, can't not sleep
		}
		else
		{
			return NOTIFY_OK;
		}
	}
	else{
		printk("sprdfb: error in cpuidle notify type!\n");
	}
	return 0;

}

static struct notifier_block scxx30_dispc_cpuidle_notifier = {
	.notifier_call = scxx30_dispc_cpuidle_notify,
};
#endif

static int32_t sprdfb_dispc_module_init(struct sprdfb_device *dev)
{
	int ret = 0;
	int irq_num = 0;

	if(dispc_ctx.is_inited){
		printk(KERN_WARNING "sprdfb: dispc_module has already initialized! warning!!");
		return 0;
	}
	else{
		printk(KERN_INFO "sprdfb: dispc_module_init. call only once!");
	}
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
	printk("sprdfb: dispc irq_num = %d\n", irq_num);

	//ret = request_irq(IRQ_DISPC_INT, dispc_isr, IRQF_DISABLED, "DISPC", &dispc_ctx);
	ret = request_irq(irq_num, dispc_isr, IRQF_DISABLED, "DISPC", &dispc_ctx);
	if (ret) {
		printk(KERN_ERR "sprdfb: dispc failed to request irq!\n");
		sprdfb_dispc_uninit(dev);
		return -1;
	}

	dispc_ctx.is_inited = true;

#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || defined(CONFIG_SPRD_SCX35_DMC_FREQ))
	ret=sprdfb_dispc_threshold_check();
	if(ret)
		devfreq_notifier_register(&sprd_fb_notify);
#endif

#ifdef CONFIG_CPU_IDLE
	if(1 == dev->capability.need_light_sleep)
	{
		ret = register_sc_cpuidle_notifier(&scxx30_dispc_cpuidle_notifier);
		if (ret) {
			printk("sprdfb: Failed to setup light sleep notifier!\n");
		}
	}
#endif
	return 0;

}

static int32_t sprdfb_dispc_early_init(struct sprdfb_device *dev)
{
	int ret = 0;

	if (!dispc_ctx.is_inited) {
		//spin_lock_init(&dispc_ctx.clk_spinlock);
		sema_init(&dispc_ctx.clk_sem, 1);
	}

	ret = dispc_clk_init(dev);
	if(ret){
		printk(KERN_WARNING "sprdfb: dispc_clk_init fail!\n");
		return -1;
	}

	if(!dispc_ctx.is_inited){
		//init
		if(dev->panel_ready){
			//panel ready
			printk(KERN_INFO "sprdfb: [%s]: dispc has alread initialized\n", __FUNCTION__);
			dispc_ctx.is_first_frame = false;
		}else{
			//panel not ready
			printk(KERN_INFO "sprdfb: [%s]: dispc is not initialized\n", __FUNCTION__);
			dispc_reset();
			dispc_module_enable();
			dispc_ctx.is_first_frame = true;
		}
		ret = sprdfb_dispc_module_init(dev);
	}else{
		//resume
		printk(KERN_INFO "sprdfb: [%s]: sprdfb_dispc_early_init resume\n", __FUNCTION__);
		dispc_reset();
		dispc_module_enable();
		dispc_ctx.is_first_frame = true;
	}

	return ret;
}


static int sprdfb_dispc_clk_disable(struct sprdfb_dispc_context *dispc_ctx_ptr, SPRDFB_DYNAMIC_CLK_SWITCH_E clock_switch_type)
{
	bool is_need_disable=false;
//	unsigned long irqflags;

	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);
	if(!dispc_ctx_ptr){
		return 0;
	}

	//spin_lock_irqsave(&dispc_ctx.clk_spinlock, irqflags);
	down(&dispc_ctx.clk_sem);
	switch(clock_switch_type){
		case SPRDFB_DYNAMIC_CLK_FORCE:
			is_need_disable=true;
			break;
		case SPRDFB_DYNAMIC_CLK_REFRESH:
			dispc_ctx_ptr->clk_is_refreshing=false;
			if(dispc_ctx_ptr->clk_open_count<=0){
				is_need_disable=true;
			}
			break;
		case SPRDFB_DYNAMIC_CLK_COUNT:
			if(dispc_ctx_ptr->clk_open_count>0){
				dispc_ctx_ptr->clk_open_count--;
				if(dispc_ctx_ptr->clk_open_count==0){
					if(!dispc_ctx_ptr->clk_is_refreshing){
						is_need_disable=true;
					}
				}
			}
			break;
		default:
			break;
	}

	if(dispc_ctx_ptr->clk_is_open && is_need_disable){
		pr_debug(KERN_INFO "sprdfb: sprdfb_dispc_clk_disable real\n");
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc);
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc_dpi);
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc_dbi);
		dispc_ctx_ptr->clk_is_open=false;
		dispc_ctx_ptr->clk_is_refreshing=false;
		dispc_ctx_ptr->clk_open_count=0;
	}

	//spin_unlock_irqrestore(&dispc_ctx.clk_spinlock,irqflags);
	up(&dispc_ctx.clk_sem);

	pr_debug(KERN_INFO "sprdfb: sprdfb_dispc_clk_disable type=%d refresh=%d,count=%d\n",clock_switch_type,dispc_ctx_ptr->clk_is_refreshing,dispc_ctx_ptr->clk_open_count);
	return 0;
}

static int sprdfb_dispc_clk_enable(struct sprdfb_dispc_context *dispc_ctx_ptr, SPRDFB_DYNAMIC_CLK_SWITCH_E clock_switch_type)
{
	int ret = 0;
	bool is_dispc_enable=false;
	bool is_dispc_dpi_enable=false;
	//unsigned long irqflags;

	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);
	if(!dispc_ctx_ptr){
		return -1;
	}

	//spin_lock_irqsave(&dispc_ctx.clk_spinlock, irqflags);
	down(&dispc_ctx.clk_sem);

	if(!dispc_ctx_ptr->clk_is_open){
		pr_debug(KERN_INFO "sprdfb: sprdfb_dispc_clk_enable real\n");
		ret = clk_prepare_enable(dispc_ctx_ptr->clk_dispc);
		if(ret){
			printk("sprdfb: enable clk_dispc error!!!\n");
			ret=-1;
			goto ERROR_CLK_ENABLE;
		}
		is_dispc_enable=true;
		ret = clk_prepare_enable(dispc_ctx_ptr->clk_dispc_dpi);
		if(ret){
			printk("sprdfb: enable clk_dispc_dpi error!!!\n");
			ret=-1;
			goto ERROR_CLK_ENABLE;
		}
		is_dispc_dpi_enable=true;
		ret = clk_prepare_enable(dispc_ctx_ptr->clk_dispc_dbi);
		if(ret){
			printk("sprdfb: enable clk_dispc_dbi error!!!\n");
			ret=-1;
			goto ERROR_CLK_ENABLE;
		}
		dispc_ctx_ptr->clk_is_open=true;
	}

	switch(clock_switch_type){
		case SPRDFB_DYNAMIC_CLK_FORCE:
			break;
		case SPRDFB_DYNAMIC_CLK_REFRESH:
			dispc_ctx_ptr->clk_is_refreshing=true;
			break;
		case SPRDFB_DYNAMIC_CLK_COUNT:
			dispc_ctx_ptr->clk_open_count++;
			break;
		default:
			break;
	}

	//spin_unlock_irqrestore(&dispc_ctx.clk_spinlock,irqflags);
	up(&dispc_ctx.clk_sem);

	pr_debug(KERN_INFO "sprdfb: sprdfb_dispc_clk_enable type=%d refresh=%d,count=%d,ret=%d\n",clock_switch_type,dispc_ctx_ptr->clk_is_refreshing,dispc_ctx_ptr->clk_open_count,ret);
	return ret;

ERROR_CLK_ENABLE:
	if(is_dispc_enable){
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc);
	}
	if(is_dispc_dpi_enable){
		clk_disable_unprepare(dispc_ctx_ptr->clk_dispc_dpi);
	}

	//spin_unlock_irqrestore(&dispc_ctx.clk_spinlock,irqflags);
	up(&dispc_ctx.clk_sem);

	printk("sprdfb: sprdfb_dispc_clk_enable error!!!!!!\n");
	return ret;
}

static int32_t sprdfb_dispc_init(struct sprdfb_device *dev)
{
	uint32_t dispc_int_en_reg_val = 0x00;//ONLY for dispc interrupt en reg

	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);

	if (unlikely(!dev)) {
            printk("sprdfb: [%s] Invalid parameter!\n", __FUNCTION__);
            return -1;
	}

	dispc_ctx.dev = dev;

	/*set bg color*/
	//dispc_set_bg_color(0xFFFFFFFF);
	dispc_set_bg_color(0x0);
	/*enable dithering*/
	dispc_dithering_enable(true);
	/*use MSBs as img exp mode*/
	dispc_set_exp_mode(0x0);
	//enable DISPC Power Control
	dispc_pwr_enable(true);
#if defined( CONFIG_FB_SCX30G)
	//set buf thres
	dispc_set_threshold(0x960, 0x00, 0x960);//0x1000: 4K
#elif defined(CONFIG_FB_SCX35L)
	dispc_set_threshold(0x1388, 0x00, 0x1388);//For sharkl, linebuffer size: 5K
#endif

	if(dispc_ctx.is_first_frame){
		dispc_layer_init(&(dev->fb->var));
	}else{
		dispc_layer_update(&(dev->fb->var));
	}

	if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
		if(dispc_ctx.is_first_frame){
			/*set dpi register update only with SW*/
			dispc_set_bits(BIT(4), DISPC_DPI_CTRL);
		}else{
			/*set dpi register update with SW & VSYNC*/
			dispc_clear_bits(BIT(4), DISPC_DPI_CTRL);
		}
		/*enable dispc update done INT*/
		dispc_int_en_reg_val |= DISPC_INT_UPDATE_DONE_MASK;
		/* enable hw vsync */
		dispc_int_en_reg_val |= DISPC_INT_HWVSYNC;
	}else{
		/* enable dispc DONE  INT*/
		dispc_int_en_reg_val |= DISPC_INT_DONE_MASK;
	}
	dispc_int_en_reg_val |= DISPC_INT_ERR_MASK;
	dispc_write(dispc_int_en_reg_val, DISPC_INT_EN);
	dev->enable = 1;

	return 0;
}

static void sprdfb_dispc_clean_lcd (struct sprdfb_device *dev)
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
	/* need 1 frame delay for stability */
	msleep(17);
}

static int32_t sprdfb_dispc_refresh (struct sprdfb_device *dev)
{
	uint32_t reg_val = 0;
	struct fb_info *fb = dev->fb;
	uint32_t size = 0;

	uint32_t base = fb->fix.smem_start + fb->fix.line_length * fb->var.yoffset;

	pr_debug(KERN_INFO "sprdfb: [%s]\n",__FUNCTION__);

	down(&dev->refresh_lock);
	if(0 == dev->enable){
		printk("sprdfb: [%s]: do not refresh in suspend!!!\n", __FUNCTION__);
		goto ERROR_REFRESH;
	}

	if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
		dispc_ctx.vsync_waiter ++;
		dispc_sync(dev);
//		dispc_ctx.vsync_done = 0;
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if(sprdfb_dispc_clk_enable(&dispc_ctx,SPRDFB_DYNAMIC_CLK_REFRESH)){
			printk(KERN_WARNING "sprdfb: enable dispc_clk fail in refresh!\n");
			goto ERROR_REFRESH;
		}
#endif
	}
#ifdef CONFIG_FB_TRIPLE_FRAMEBUFFER
	else{
            if((dispc_read(DISPC_OSD_BASE_ADDR) != dispc_read(SHDW_OSD_BASE_ADDR))
                &&dispc_read(SHDW_OSD_BASE_ADDR) != 0){
                dispc_ctx.vsync_waiter ++;
                dispc_sync(dev);
            }
        }
#endif

	pr_debug(KERN_INFO "srpdfb: [%s] got sync\n", __FUNCTION__);

#ifdef CONFIG_FB_MMAP_CACHED
	if(NULL != dispc_ctx.vma){
		pr_debug("sprdfb: sprdfb_dispc_refresh dispc_ctx.vma=0x%x\n ",dispc_ctx.vma);
		dma_sync_single_for_device(dev->fb->dev, dev->fb->fix.smem_start, dev->fb->fix.smem_len, DMA_TO_DEVICE);
	}
	if(fb->var.reserved[3] == 1){
		dispc_dithering_enable(false);
		if(NULL != dev->panel->ops->panel_change_epf){
			dev->panel->ops->panel_change_epf(dev->panel,false);
		}
	}else{
		dispc_dithering_enable(true);
		if(NULL != dev->panel->ops->panel_change_epf){
			dev->panel->ops->panel_change_epf(dev->panel,true);
		}
	}
#endif
	//dispc_osd_enable(true);
	dispc_set_osd_alpha(0xff);

//	dispc_ctx.dev = dev;
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	if(SPRD_OVERLAY_STATUS_STARTED == dispc_ctx.overlay_state){
		overlay_close(dev);
	}
#endif

		size = (fb->var.xres & 0xffff) | ((fb->var.yres) << 16);

		dispc_write(base, DISPC_OSD_BASE_ADDR);
		dispc_write(0, DISPC_OSD_DISP_XY);
		dispc_write(size, DISPC_OSD_SIZE_XY);
		dispc_write(fb->var.xres, DISPC_OSD_PITCH);
#ifdef CONFIG_FB_LOW_RES_SIMU
		size = (dev->panel->width &0xffff) | ((dev->panel->height)<<16);
#endif
		dispc_write(size, DISPC_SIZE_XY);

#ifdef  BIT_PER_PIXEL_SURPPORT
	        /* data format */
	        if (fb->var.bits_per_pixel == 32) {
		        /* ABGR */
		        reg_val |= (3 << 4);
		        /* rb switch */
		        reg_val |= (1 << 15);
		        dispc_clear_bits(0x30000,DISPC_CTRL);
	        } else {
		        /* RGB565 */
		        reg_val |= (5 << 4);
		        /* B2B3B0B1 */
		        reg_val |= (2 << 8);

		        dispc_clear_bits(0x30000,DISPC_CTRL);
		        dispc_set_bits(0x10000,DISPC_CTRL);
	        }
	        reg_val |= (1 << 0);

	        /* alpha mode select  - block alpha*/
	        reg_val |= (1 << 2);

	        dispc_write(reg_val, DISPC_OSD_CTRL);
#endif

		if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
			sprdfb_panel_invalidate(dev->panel);
		}

	sprdfb_panel_before_refresh(dev);

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	dispc_set_bits(BIT(0), DISPC_OSD_CTRL);
	if(SPRD_OVERLAY_STATUS_ON == dispc_ctx.overlay_state){
		overlay_start(dev, (SPRD_LAYER_IMG));
	}
#endif

	dev->frame_count += 1;
	dispc_run(dev);

#ifdef CONFIG_FB_ESD_SUPPORT
	if(!dev->ESD_work_start){
		printk("sprdfb: schedule ESD work queue!\n");
		schedule_delayed_work(&dev->ESD_work, msecs_to_jiffies(dev->ESD_timeout_val));
		dev->ESD_work_start = true;
	}
#endif

ERROR_REFRESH:
	up(&dev->refresh_lock);

	if(NULL != dev->logo_buffer_addr_v){
		printk("sprdfb: free logo proc buffer!\n");
		free_pages((unsigned long)dev->logo_buffer_addr_v, get_order(dev->logo_buffer_size));
		dev->logo_buffer_addr_v = 0;
	}

	pr_debug("sprdfb: DISPC_CTRL: 0x%x\n", dispc_read(DISPC_CTRL));
	pr_debug("sprdfb: DISPC_SIZE_XY: 0x%x\n", dispc_read(DISPC_SIZE_XY));

	pr_debug("sprdfb: DISPC_BG_COLOR: 0x%x\n", dispc_read(DISPC_BG_COLOR));

	pr_debug("sprdfb: DISPC_INT_EN: 0x%x\n", dispc_read(DISPC_INT_EN));

	pr_debug("sprdfb: DISPC_OSD_CTRL: 0x%x\n", dispc_read(DISPC_OSD_CTRL));
	pr_debug("sprdfb: DISPC_OSD_BASE_ADDR: 0x%x\n", dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("sprdfb: DISPC_OSD_SIZE_XY: 0x%x\n", dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("sprdfb: DISPC_OSD_PITCH: 0x%x\n", dispc_read(DISPC_OSD_PITCH));
	pr_debug("sprdfb: DISPC_OSD_DISP_XY: 0x%x\n", dispc_read(DISPC_OSD_DISP_XY));
	pr_debug("sprdfb: DISPC_OSD_ALPHA	: 0x%x\n", dispc_read(DISPC_OSD_ALPHA));

	return 0;
}

static int32_t sprdfb_dispc_shutdown(struct sprdfb_device *dev)
{
	sprdfb_panel_shutdown(dev);
	return 0;
}

static int32_t sprdfb_dispc_suspend(struct sprdfb_device *dev)
{
	printk(KERN_INFO "sprdfb: [%s], dev->enable = %d\n",__FUNCTION__, dev->enable);

	if (0 != dev->enable){
		down(&dev->refresh_lock);
		if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
			/* must wait ,dispc_sync() */
			dispc_ctx.vsync_waiter ++;
			dispc_sync(dev);
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
			printk("sprdfb: open clk in suspend\n");
			if(sprdfb_dispc_clk_enable(&dispc_ctx,SPRDFB_DYNAMIC_CLK_COUNT)){
				printk(KERN_WARNING "sprdfb: [%s] clk enable fail!!!\n",__FUNCTION__);
			}
#endif
			printk(KERN_INFO "sprdfb: [%s] got sync\n",__FUNCTION__);
		}

		dev->enable = 0;
		up(&dev->refresh_lock);

#ifdef CONFIG_FB_ESD_SUPPORT
		if(dev->ESD_work_start == true){
			printk("sprdfb: cancel ESD work queue\n");
			cancel_delayed_work_sync(&dev->ESD_work);
			dev->ESD_work_start = false;
		}

#endif
		sprdfb_panel_suspend(dev);
#ifndef CONFIG_GEN_PANEL_OCTA
		dispc_stop(dev);
#endif
                dispc_write(0, DISPC_INT_EN);
                msleep(50); /*fps 1frame >      previous interrupt done time*/

		clk_disable_unprepare(dispc_ctx.clk_dispc_emc);
		sprdfb_dispc_clk_disable(&dispc_ctx,SPRDFB_DYNAMIC_CLK_FORCE);
//		sci_glb_clr(DISPC_EMC_EN, BIT_DISPC_EMC_EN);
	}else{
		printk(KERN_ERR "sprdfb: [%s]: Invalid device status %d\n", __FUNCTION__, dev->enable);
	}
	return 0;
}

static int32_t sprdfb_dispc_resume(struct sprdfb_device *dev)
{
	printk(KERN_INFO "sprdfb: [%s], dev->enable= %d\n",__FUNCTION__, dev->enable);

	if (dev->enable == 0) {
		if(sprdfb_dispc_clk_enable(&dispc_ctx,SPRDFB_DYNAMIC_CLK_FORCE)){
			printk(KERN_WARNING "sprdfb: [%s] clk enable fail!!\n",__FUNCTION__);
			//return 0;
		}
//		sci_glb_set(DISPC_EMC_EN, BIT_DISPC_EMC_EN);

		dispc_ctx.vsync_done = 1;
		if (1){//(dispc_read(DISPC_SIZE_XY) == 0 ) { /* resume from deep sleep */
			printk(KERN_INFO "sprdfb: [%s] from deep sleep\n",__FUNCTION__);
			sprdfb_dispc_early_init(dev);
			sprdfb_panel_resume(dev, true);
			sprdfb_dispc_init(dev);
		}else {
			printk(KERN_INFO "sprdfb: [%s]  not from deep sleep\n",__FUNCTION__);

			sprdfb_panel_resume(dev, true);
		}

		dev->enable = 1;
		if(dev->panel->is_clean_lcd){
		sprdfb_dispc_clean_lcd(dev);
		}

      sprdfb_panel_start(dev);
	}
	printk(KERN_INFO "sprdfb: [%s], leave dev->enable= %d\n",__FUNCTION__, dev->enable);

	return 0;
}


#ifdef CONFIG_FB_ESD_SUPPORT
//for video esd check
static int32_t sprdfb_dispc_check_esd_dpi(struct sprdfb_device *dev)
{
	uint32_t ret = 0;
	unsigned long flags;

#ifdef FB_CHECK_ESD_BY_TE_SUPPORT
	ret = sprdfb_panel_ESD_check(dev);
	if(0 !=ret){
		dispc_run_for_feature(dev);
	}
#else
#ifdef FB_CHECK_ESD_IN_VFP
	ret = sprdfb_panel_ESD_check(dev);
#else
	local_irq_save(flags);
	dispc_stop_for_feature(dev);

	ret = sprdfb_panel_ESD_check(dev);	//make sure there is no log in this function

	dispc_run_for_feature(dev);
	local_irq_restore(flags);
#endif
#endif

	return ret;
}

//for cmd esd check
static int32_t sprdfb_dispc_check_esd_edpi(struct sprdfb_device *dev)
{
	uint32_t ret = 0;

	dispc_ctx.vsync_waiter ++;
	dispc_sync(dev);
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
	if(sprdfb_dispc_clk_enable(&dispc_ctx,SPRDFB_DYNAMIC_CLK_COUNT)){
		printk(KERN_WARNING "sprdfb: [%s] clk enable fail!!!\n",__FUNCTION__);
		return -1;
	}
#endif

	ret = sprdfb_panel_ESD_check(dev);

	if(0 !=ret){
		dispc_run_for_feature(dev);
	}
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
	sprdfb_dispc_clk_disable(&dispc_ctx,SPRDFB_DYNAMIC_CLK_COUNT);
#endif

	return ret;
}

static int32_t sprdfb_dispc_check_esd(struct sprdfb_device *dev)
{
	uint32_t ret = 0;
	bool	is_refresh_lock_down=false;

	pr_debug("sprdfb: [%s] \n", __FUNCTION__);

	if(SPRDFB_PANEL_IF_DBI == dev->panel_if_type){
		printk("sprdfb: [%s] leave (not support dbi mode now)!\n", __FUNCTION__);
		ret = -1;
		goto ERROR_CHECK_ESD;
	}
	down(&dev->refresh_lock);
	is_refresh_lock_down=true;
	if(0 == dev->enable){
		printk("sprdfb: [%s] leave (Invalid device status)!\n", __FUNCTION__);
		ret=-1;
		goto ERROR_CHECK_ESD;
	}

        if(0 == (dev->check_esd_time % 30)){
	    printk("sprdfb: [%s] (%d, %d, %d)\n",__FUNCTION__, dev->check_esd_time, dev->panel_reset_time, dev->reset_dsi_time);
	}else{
	    pr_debug("sprdfb: [%s] (%d, %d, %d)\n",__FUNCTION__, dev->check_esd_time, dev->panel_reset_time, dev->reset_dsi_time);
	}
	if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
		ret=sprdfb_dispc_check_esd_dpi(dev);
	}
	else{
		ret=sprdfb_dispc_check_esd_edpi(dev);
	}

ERROR_CHECK_ESD:
	if(is_refresh_lock_down){
		up(&dev->refresh_lock);
	}

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
}
#endif

#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
static int overlay_open(void)
{
	pr_debug("sprdfb: [%s] : %d\n", __FUNCTION__,dispc_ctx.overlay_state);

/*
	if(SPRD_OVERLAY_STATUS_OFF  != dispc_ctx.overlay_state){
		printk(KERN_ERR "sprdfb: Overlay open fail (has been opened)");
		return -1;
	}
*/

	dispc_ctx.overlay_state = SPRD_OVERLAY_STATUS_ON;
	return 0;
}

static int overlay_start(struct sprdfb_device *dev, uint32_t layer_index)
{
	pr_debug("sprdfb: [%s] : %d, %d\n", __FUNCTION__,dispc_ctx.overlay_state, layer_index);


	if(SPRD_OVERLAY_STATUS_ON  != dispc_ctx.overlay_state){
		printk(KERN_ERR "sprdfb: overlay start fail. (not opened)");
		return -1;
	}

	if((0 == dispc_read(DISPC_IMG_Y_BASE_ADDR)) && (0 == dispc_read(DISPC_OSD_BASE_ADDR))){
		printk(KERN_ERR "sprdfb: overlay start fail. (not configged)");
		return -1;
	}

/*
	if(0 != dispc_sync(dev)){
		printk(KERN_ERR "sprdfb: overlay start fail. (wait done fail)");
		return -1;
	}
*/
	dispc_set_bg_color(0x0);
	dispc_clear_bits(BIT(2), DISPC_OSD_CTRL); /*use pixel alpha*/
	dispc_set_osd_alpha(0x80);

	if((layer_index & SPRD_LAYER_IMG) && (0 != dispc_read(DISPC_IMG_Y_BASE_ADDR))){
		dispc_set_bits(BIT(0), DISPC_IMG_CTRL);/* enable the image layer */
	}
	if((layer_index & SPRD_LAYER_OSD) && (0 != dispc_read(DISPC_OSD_BASE_ADDR))){
		dispc_set_bits(BIT(0), DISPC_OSD_CTRL);/* enable the osd layer */
	}
	dispc_ctx.overlay_state = SPRD_OVERLAY_STATUS_STARTED;
	return 0;
}

static int overlay_img_configure(struct sprdfb_device *dev, int type, overlay_rect *rect, unsigned char *buffer, int y_endian, int uv_endian, bool rb_switch)
{
	uint32_t reg_value;

	pr_debug("sprdfb: [%s] : %d, (%d, %d,%d,%d), 0x%x\n", __FUNCTION__, type, rect->x, rect->y, rect->h, rect->w, (unsigned int)buffer);


	if(SPRD_OVERLAY_STATUS_ON  != dispc_ctx.overlay_state){
		printk(KERN_ERR "sprdfb: Overlay config fail (not opened)");
		return -1;
	}

	if (type >= SPRD_DATA_TYPE_LIMIT) {
		printk(KERN_ERR "sprdfb: Overlay config fail (type error)");
		return -1;
	}

	if((y_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT) || (uv_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT)){
		printk(KERN_ERR "sprdfb: Overlay config fail (y, uv endian error)");
		return -1;
	}

/*	lcdc_write(((type << 3) | (1 << 0)), LCDC_IMG_CTRL); */
	/*lcdc_write((type << 3) , LCDC_IMG_CTRL);*/
	reg_value = (y_endian << 8)|(uv_endian<< 10)|(type << 4);
	if(rb_switch){
		reg_value |= (1 << 15);
	}
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

	if(type < SPRD_DATA_TYPE_RGB888) {
		dispc_write(1, DISPC_Y2R_CTRL);
#ifndef CONFIG_FB_SCX35
		dispc_write(SPRDFB_BRIGHTNESS|SPRDFB_CONTRAST, DISPC_Y2R_Y_PARAM);
		dispc_write(SPRDFB_OFFSET_U|SPRDFB_SATURATION_U, DISPC_Y2R_U_PARAM);
		dispc_write(SPRDFB_OFFSET_V|SPRDFB_SATURATION_V, DISPC_Y2R_V_PARAM);
#else
		dispc_write(SPRDFB_CONTRAST, DISPC_Y2R_CONTRAST);
		dispc_write(SPRDFB_SATURATION, DISPC_Y2R_SATURATION);
		dispc_write(SPRDFB_BRIGHTNESS, DISPC_Y2R_BRIGHTNESS);
#endif
	}

	pr_debug("sprdfb: DISPC_IMG_CTRL: 0x%x\n", dispc_read(DISPC_IMG_CTRL));
	pr_debug("sprdfb: DISPC_IMG_Y_BASE_ADDR: 0x%x\n", dispc_read(DISPC_IMG_Y_BASE_ADDR));
	pr_debug("sprdfb: DISPC_IMG_UV_BASE_ADDR: 0x%x\n", dispc_read(DISPC_IMG_UV_BASE_ADDR));
	pr_debug("sprdfb: DISPC_IMG_SIZE_XY: 0x%x\n", dispc_read(DISPC_IMG_SIZE_XY));
	pr_debug("sprdfb: DISPC_IMG_PITCH: 0x%x\n", dispc_read(DISPC_IMG_PITCH));
	pr_debug("sprdfb: DISPC_IMG_DISP_XY: 0x%x\n", dispc_read(DISPC_IMG_DISP_XY));
	pr_debug("sprdfb: DISPC_Y2R_CTRL: 0x%x\n", dispc_read(DISPC_Y2R_CTRL));
#ifndef CONFIG_FB_SCX35
	pr_debug("sprdfb: DISPC_Y2R_Y_PARAM: 0x%x\n", dispc_read(DISPC_Y2R_Y_PARAM));
	pr_debug("sprdfb: DISPC_Y2R_U_PARAM: 0x%x\n", dispc_read(DISPC_Y2R_U_PARAM));
	pr_debug("sprdfb: DISPC_Y2R_V_PARAM: 0x%x\n", dispc_read(DISPC_Y2R_V_PARAM));
#else
	pr_debug("sprdfb: DISPC_Y2R_CONTRAST: 0x%x\n", dispc_read(DISPC_Y2R_CONTRAST));
	pr_debug("sprdfb: DISPC_Y2R_SATURATION: 0x%x\n", dispc_read(DISPC_Y2R_SATURATION));
	pr_debug("sprdfb: DISPC_Y2R_BRIGHTNESS: 0x%x\n", dispc_read(DISPC_Y2R_BRIGHTNESS));
#endif
	return 0;
}

static int overlay_osd_configure(struct sprdfb_device *dev, int type, overlay_rect *rect, unsigned char *buffer, int y_endian, int uv_endian, bool rb_switch)
{
	uint32_t reg_value;

	pr_debug("sprdfb: [%s] : %d, (%d, %d,%d,%d), 0x%x\n", __FUNCTION__, type, rect->x, rect->y, rect->h, rect->w, (unsigned int)buffer);


	if(SPRD_OVERLAY_STATUS_ON  != dispc_ctx.overlay_state){
		printk(KERN_ERR "sprdfb: Overlay config fail (not opened)");
		return -1;
	}

	if ((type >= SPRD_DATA_TYPE_LIMIT) || (type <= SPRD_DATA_TYPE_YUV400)) {
		printk(KERN_ERR "sprdfb: Overlay config fail (type error)");
		return -1;
	}

	if(y_endian >= SPRD_IMG_DATA_ENDIAN_LIMIT ){
		printk(KERN_ERR "sprdfb: Overlay config fail (rgb endian error)");
		return -1;
	}

/*	lcdc_write(((type << 3) | (1 << 0)), LCDC_IMG_CTRL); */
	/*lcdc_write((type << 3) , LCDC_IMG_CTRL);*/

	/*use premultiply pixel alpha*/
	reg_value = (y_endian<<8)|(type << 4|(1<<2))|(2<<16);
	if(rb_switch){
		reg_value |= (1 << 15);
	}
	dispc_write(reg_value, DISPC_OSD_CTRL);

	dispc_write((uint32_t)buffer, DISPC_OSD_BASE_ADDR);

	reg_value = (rect->h << 16) | (rect->w);
	dispc_write(reg_value, DISPC_OSD_SIZE_XY);

	dispc_write(rect->w, DISPC_OSD_PITCH);

	reg_value = (rect->y << 16) | (rect->x);
	dispc_write(reg_value, DISPC_OSD_DISP_XY);


	pr_debug("sprdfb: DISPC_OSD_CTRL: 0x%x\n", dispc_read(DISPC_OSD_CTRL));
	pr_debug("sprdfb: DISPC_OSD_BASE_ADDR: 0x%x\n", dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("sprdfb: DISPC_OSD_SIZE_XY: 0x%x\n", dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("sprdfb: DISPC_OSD_PITCH: 0x%x\n", dispc_read(DISPC_OSD_PITCH));
	pr_debug("sprdfb: DISPC_OSD_DISP_XY: 0x%x\n", dispc_read(DISPC_OSD_DISP_XY));

	return 0;
}

static int overlay_close(struct sprdfb_device *dev)
{
	if(SPRD_OVERLAY_STATUS_OFF  == dispc_ctx.overlay_state){
		printk(KERN_ERR "sprdfb: overlay close fail. (has been closed)");
		return 0;
	}

/*
	if (0 != sprd_lcdc_sync(dev)) {
		printk(KERN_ERR "sprdfb: overlay close fail. (wait done fail)\n");
		return -1;
	}
*/
	//dispc_set_bg_color(0xFFFFFFFF);
	dispc_set_bg_color(0x0);
	dispc_set_bits(BIT(2), DISPC_OSD_CTRL);/*use block alpha*/
	dispc_set_osd_alpha(0xff);
	dispc_clear_bits(BIT(0), DISPC_IMG_CTRL);	/* disable the image layer */
	dispc_write(0, DISPC_IMG_Y_BASE_ADDR);
	dispc_write(0, DISPC_OSD_BASE_ADDR);
	dispc_layer_init(&(dev->fb->var));
	dispc_ctx.overlay_state = SPRD_OVERLAY_STATUS_OFF;

	return 0;
}

/*TO DO: need mutext with suspend, resume*/
static int32_t sprdfb_dispc_enable_overlay(struct sprdfb_device *dev, struct overlay_info* info, int enable)
{
	int result = -1;
	bool	is_refresh_lock_down=false;
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
	bool	is_clk_enable=false;
#endif

	pr_debug("sprdfb: [%s]: %d, %d\n", __FUNCTION__, enable,  dev->enable);

	if(enable){  /*enable*/
		if(NULL == info){
			printk(KERN_ERR "sprdfb: sprdfb_dispc_enable_overlay fail (Invalid parameter)\n");
			goto ERROR_ENABLE_OVERLAY;
		}

		down(&dev->refresh_lock);
		is_refresh_lock_down=true;

		if(0 == dev->enable){
			printk(KERN_ERR "sprdfb: sprdfb_dispc_enable_overlay fail. (dev not enable)\n");
			goto ERROR_ENABLE_OVERLAY;
		}

		if(0 != dispc_sync(dev)){
			printk(KERN_ERR "sprdfb: sprdfb_dispc_enable_overlay fail. (wait done fail)\n");
			goto ERROR_ENABLE_OVERLAY;
		}

#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
			if(sprdfb_dispc_clk_enable(&dispc_ctx,SPRDFB_DYNAMIC_CLK_COUNT)){
				printk(KERN_WARNING "sprdfb: [%s] clk enable fail!!!\n",__FUNCTION__);
				goto ERROR_ENABLE_OVERLAY;
			}
			is_clk_enable=true;
		}
#endif
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
		if(SPRD_OVERLAY_STATUS_STARTED == dispc_ctx.overlay_state){
			overlay_close(dev);
		}
#endif
		result = overlay_open();
		if(0 != result){
			result=-1;
			goto ERROR_ENABLE_OVERLAY;
		}

		if(SPRD_LAYER_IMG == info->layer_index){
			result = overlay_img_configure(dev, info->data_type, &(info->rect), info->buffer, info->y_endian, info->uv_endian, info->rb_switch);
		}else if(SPRD_LAYER_OSD == info->layer_index){
			result = overlay_osd_configure(dev, info->data_type, &(info->rect), info->buffer, info->y_endian, info->uv_endian, info->rb_switch);
		}else{
			printk(KERN_ERR "sprdfb: sprdfb_dispc_enable_overlay fail. (invalid layer index)\n");
		}
		if(0 != result){
			result=-1;
			goto ERROR_ENABLE_OVERLAY;
		}
		/*result = overlay_start(dev);*/
	}else{   /*disable*/
		/*result = overlay_close(dev);*/
	}
ERROR_ENABLE_OVERLAY:
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
	if(is_clk_enable){
		sprdfb_dispc_clk_disable(&dispc_ctx,SPRDFB_DYNAMIC_CLK_COUNT);
	}
#endif
	if(is_refresh_lock_down){
		up(&dev->refresh_lock);
	}

	pr_debug("sprdfb: [%s] return %d\n", __FUNCTION__, result);
	return result;
}


static int32_t sprdfb_dispc_display_overlay(struct sprdfb_device *dev, struct overlay_display* setting)
{
#ifndef CONFIG_FB_LOW_RES_SIMU
	struct overlay_rect* rect = &(setting->rect);
	uint32_t size =( (rect->h << 16) | (rect->w & 0xffff));
#else
	uint32_t size = (dev->panel->width &0xffff) | ((dev->panel->height)<<16);
#endif
	dispc_ctx.dev = dev;

	pr_debug("sprdfb: sprdfb_dispc_display_overlay: layer:%d, (%d, %d,%d,%d)\n",
		setting->layer_index, setting->rect.x, setting->rect.y, setting->rect.h, setting->rect.w);

	down(&dev->refresh_lock);
	if(0 == dev->enable){
		printk("sprdfb: [%s] leave (Invalid device status)!\n", __FUNCTION__);
		goto ERROR_DISPLAY_OVERLAY;
	}
	if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
		dispc_ctx.vsync_waiter ++;
		dispc_sync(dev);
		//dispc_ctx.vsync_done = 0;
#ifdef CONFIG_FB_DYNAMIC_CLK_SUPPORT
		if(sprdfb_dispc_clk_enable(&dispc_ctx,SPRDFB_DYNAMIC_CLK_REFRESH)){
			printk(KERN_WARNING "sprdfb: [%s] clk enable fail!!!\n",__FUNCTION__);
			goto ERROR_DISPLAY_OVERLAY;
		}
#endif

	}
#ifdef CONFIG_FB_MMAP_CACHED
	dispc_dithering_enable(true);
	if(NULL != dev->panel->ops->panel_change_epf){
			dev->panel->ops->panel_change_epf(dev->panel,true);
	}

#endif
	pr_debug(KERN_INFO "srpdfb: [%s] got sync\n", __FUNCTION__);

	dispc_ctx.dev = dev;

		dispc_write(size, DISPC_SIZE_XY);

		if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
			sprdfb_panel_invalidate(dev->panel);
		}

	sprdfb_panel_before_refresh(dev);

	dispc_clear_bits(BIT(0), DISPC_OSD_CTRL);
	if(SPRD_OVERLAY_STATUS_ON == dispc_ctx.overlay_state){
		if(overlay_start(dev, setting->layer_index) != 0){
			printk("sprdfb: %s[%d] overlay_start() err, return without run dispc!\n",__func__,__LINE__);
			goto ERROR_DISPLAY_OVERLAY;
		}
	}

	dev->frame_count += 1;
	dispc_run(dev);

	if((SPRD_OVERLAY_DISPLAY_SYNC == setting->display_mode) && (SPRDFB_PANEL_IF_DPI != dev->panel_if_type)){
		dispc_ctx.vsync_waiter ++;
		if (dispc_sync(dev) != 0) {/* time out??? disable ?? */
			printk("sprdfb: sprdfb  do sprd_lcdc_display_overlay  time out!\n");
		}
		//dispc_ctx.vsync_done = 0;
	}

ERROR_DISPLAY_OVERLAY:
	up(&dev->refresh_lock);

	pr_debug("sprdfb: DISPC_CTRL: 0x%x\n", dispc_read(DISPC_CTRL));
	pr_debug("sprdfb: DISPC_SIZE_XY: 0x%x\n", dispc_read(DISPC_SIZE_XY));

	pr_debug("sprdfb: DISPC_BG_COLOR: 0x%x\n", dispc_read(DISPC_BG_COLOR));

	pr_debug("sprdfb: DISPC_INT_EN: 0x%x\n", dispc_read(DISPC_INT_EN));

	pr_debug("sprdfb: DISPC_OSD_CTRL: 0x%x\n", dispc_read(DISPC_OSD_CTRL));
	pr_debug("sprdfb: DISPC_OSD_BASE_ADDR: 0x%x\n", dispc_read(DISPC_OSD_BASE_ADDR));
	pr_debug("sprdfb: DISPC_OSD_SIZE_XY: 0x%x\n", dispc_read(DISPC_OSD_SIZE_XY));
	pr_debug("sprdfb: DISPC_OSD_PITCH: 0x%x\n", dispc_read(DISPC_OSD_PITCH));
	pr_debug("sprdfb: DISPC_OSD_DISP_XY: 0x%x\n", dispc_read(DISPC_OSD_DISP_XY));
	pr_debug("sprdfb: DISPC_OSD_ALPHA	: 0x%x\n", dispc_read(DISPC_OSD_ALPHA));
	return 0;
}

#endif

#ifdef CONFIG_FB_VSYNC_SUPPORT
static int32_t spdfb_dispc_wait_for_vsync(struct sprdfb_device *dev)
{
	int ret = 0;
	uint32_t reg_val0,reg_val1;

	pr_debug("sprdfb: [%s]\n", __FUNCTION__);

	if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
		if(!dispc_ctx.is_first_frame){
			dispc_ctx.waitfor_vsync_done = 0;
			//dispc_set_bits(BIT(0), DISPC_INT_EN);
			dispc_ctx.waitfor_vsync_waiter++;
#ifndef CONFIG_SC_FPGA
			ret  = wait_event_interruptible_timeout(dispc_ctx.waitfor_vsync_queue,
					dispc_ctx.waitfor_vsync_done, msecs_to_jiffies(100));
#else /* fpga dpi clock is 6-7MHz, timeout should be longer than reference phone */
			ret  = wait_event_interruptible_timeout(dispc_ctx.waitfor_vsync_queue,
					dispc_ctx.waitfor_vsync_done, msecs_to_jiffies(900));
#endif
			if (!ret) { /* time out */
				dispc_ctx.waitfor_vsync_done = 1;
				reg_val0 = dispc_read(DISPC_INT_RAW);
				reg_val1 = dispc_read(DISPC_DPI_STS1);
				printk(KERN_ERR "sprdfb: vsync time out!!!!!(0x%x, 0x%x)\n",
					reg_val0, reg_val1);
#ifdef CONFIG_TEMP_DEBUG
				{/*for debug*/
					int32_t i = 0;
					for(i=0;i<256;i+=16){
						printk("sprdfb: %x: 0x%x, 0x%x, 0x%x, 0x%x\n", i,
							dispc_read(i), dispc_read(i+4), dispc_read(i+8), dispc_read(i+12));
					}
					printk("**************************************\n");
				}
#endif
			}
			dispc_ctx.waitfor_vsync_waiter = 0;
		}else{
                    msleep(16);
		}

	}else{
		dispc_ctx.waitfor_vsync_done = 0;
		dispc_set_bits(BIT(1), DISPC_INT_EN);
		dispc_ctx.waitfor_vsync_waiter++;
		ret  = wait_event_interruptible_timeout(dispc_ctx.waitfor_vsync_queue,
				dispc_ctx.waitfor_vsync_done, msecs_to_jiffies(100));

		if (!ret) { /* time out */
			dispc_ctx.waitfor_vsync_done = 1;
			printk(KERN_ERR "sprdfb: vsync time out!!!!!\n");
#ifdef CONFIG_TEMP_DEBUG
			{/*for debug*/
				int32_t i = 0;
				for(i=0;i<256;i+=16){
					printk("sprdfb: %x: 0x%x, 0x%x, 0x%x, 0x%x\n", i,
						dispc_read(i), dispc_read(i+4), dispc_read(i+8), dispc_read(i+12));
				}
				printk("**************************************\n");
			}
#endif
		}
		dispc_ctx.waitfor_vsync_waiter = 0;
	}
	pr_debug("sprdfb: [%s] (%d)\n", __FUNCTION__, ret);
	return 0;
}
#endif

//#if (defined(CONFIG_FB_DYNAMIC_FREQ_SCALING) || !defined(FB_CHECK_ESD_IN_VFP))
static void dispc_stop_for_feature(struct sprdfb_device *dev)
{
	int wait_count = 0;

	if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
		dispc_stop(dev);
		while((dispc_read(DISPC_DPI_STS1) & BIT(16)) && (wait_count < 100)){
			udelay(1000);
			wait_count++;
		}
		if(wait_count >= 100){
			printk("sprdfb:[%s] can't wait till dispc stop!!!\n", __func__);
		}
		udelay(25);
	}
}

static void dispc_run_for_feature(struct sprdfb_device *dev)
{
#ifdef CONFIG_FB_LCD_OVERLAY_SUPPORT
	if(SPRD_OVERLAY_STATUS_ON != dispc_ctx.overlay_state)
#endif
	{
		dispc_run(dev);
	}
}
//#endif

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
	struct panel_spec* panel = fb_dev->panel;

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
			ret=-EINVAL;
			goto exit;
		}
		/* Note: local interrupt will be disabled */
		local_irq_save(flags);
		dispc_stop_for_feature(fb_dev);
		ret = dispc_update_clk(fb_dev, new_val,
				SPRDFB_DYNAMIC_PCLK);
		break;

	case SPRDFB_DYNAMIC_FPS:
		if (fb_dev->panel_if_type != SPRDFB_PANEL_IF_DPI) {
			dispc_ctx.vsync_waiter ++;
			dispc_sync(fb_dev);
			sprdfb_panel_change_fps(fb_dev, new_val);
			ret=0;
			goto exit;
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
		ret = dispc_update_clk(fb_dev, new_val, SPRDFB_DYNAMIC_MIPI_CLK);
		break;

	default:
		pr_err("invalid CMD type\n");
		ret=-EINVAL;
		goto exit;
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
	local_irq_restore(flags);
	up(&fb_dev->refresh_lock);

	pr_debug("%s --leave--", __func__);
	return ret;

exit:
	up(&fb_dev->refresh_lock);
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
	u32 old_val;
	u32 dpi_clk_src;
	struct panel_spec* panel = fb_dev->panel;

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
		if(!panel){
			pr_err("%s: there is no panel!", __func__);
			return -ENXIO;
		}
		if (panel->type != LCD_MODE_DSI) {
			pr_err("%s: panel type isn't dsi mode", __func__);
			return -ENXIO;
		}
		old_val = panel->info.mipi->phy_feq;
		ret = sprdfb_dsi_chg_dphy_freq(fb_dev, new_val);
		if (ret) {
			pr_err("%s: new dphy freq is invalid.", __func__);
			return -EINVAL;
		}
		pr_info("dphy frequency is switched from %dHz to %dHz\n",
						old_val * 1000,
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

#if (defined(CONFIG_SPRD_SCXX30_DMC_FREQ) || defined(CONFIG_SPRD_SCX35_DMC_FREQ))

static unsigned int sprdfb_dispc_threshold_check()
{
#if defined CONFIG_FB_SCX30G || defined CONFIG_FB_SCX35L
	//For Tshark and SharkL, not need to change threshlod
	return 0;
#endif
	return 1;
}

/*return value:
0 -- Allow DMC change frequency
1 -- Don't allow DMC change frequency*/
static unsigned int sprdfb_dispc_change_threshold(struct devfreq_dbs *h, unsigned int state)
{
	struct sprdfb_dispc_context *dispc_ctx = (struct sprdfb_dispc_context *)h->data;
	struct sprdfb_device *dev = dispc_ctx->dev;
	if(NULL == dev || 0 == dev->enable){
		//printk(KERN_ERR "sprdfb: sprdfb_dispc_change_threshold fail.(dev not enable)\n");
		return 0;
	}
	printk(KERN_ERR "sprdfb: DMC change freq(%u)\n", state);
	mdelay(10);
	if(SPRDFB_PANEL_IF_DPI == dev->panel_if_type){
		if(state == DEVFREQ_PRE_CHANGE){
			dispc_set_threshold(0x960, 0x00, 0x960);
		}else{
			dispc_set_threshold(0x500, 0x00, 0x500);
		}
	}
	return 0;
}
#endif

//begin bug210112
extern unsigned long lcd_base_from_uboot;

static int32_t sprdfb_dispc_refresh_logo (struct sprdfb_device *dev)
{
	uint32_t i = 0;
	pr_debug("%s:[%d] panel_if_type:%d\n",__func__,__LINE__,dev->panel_if_type);

	if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type){
		printk(KERN_ERR "sprdfb: sprdfb_dispc_refresh_logo if type error\n!");
		return 0;
	}
	dispc_clear_bits(0x1f, DISPC_INT_EN);//disable all interrupt
	dispc_set_bits(0x1f, DISPC_INT_CLR);// clear all interruption
	dispc_set_bits(BIT(5), DISPC_DPI_CTRL);//update


//wait for update- done interruption
	for(i=0; i<500; i++) {
		if(!(dispc_read(DISPC_INT_RAW) & (0x10))){
			udelay(1000);
		} else {
				break;
			}
		}
		if(i >= 500) {
			printk("sprdfb: [%s] wait dispc done int time out!! (0x%x)\n", __func__, dispc_read(DISPC_INT_RAW));
		}
	dispc_set_bits((1<<5), DISPC_INT_CLR);
	return 0;
}

void sprdfb_dispc_logo_proc(struct sprdfb_device *dev)
{
	//inline size_t roundUpToPageSize(size_t x) {    return (x + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);}
	unsigned long logo_dst_p = 0;
	void *logo_src_v = NULL;
	void *logo_dst_v = NULL; //use the second frame buffer	,virtual
	uint32_t logo_size = 0;// should be rgb565

	pr_debug("sprdfb: %s[%d] enter.\n",__func__,__LINE__);

	if(dev == NULL) {
		printk("sprdfb: %s[%d]: dev == NULL, return without process logo!!\n",__func__,__LINE__);
		return;
	}

	if(lcd_base_from_uboot == 0L) {
		printk("sprdfb: %s[%d]: lcd_base_from_uboot == 0, return without process logo!!\n",__func__,__LINE__);
		return;
	}
	if(SPRDFB_PANEL_IF_DPI != dev->panel_if_type)
		return;

//#define USE_OVERLAY_BUFF
#ifdef CONFIG_FB_LOW_RES_SIMU
	if((0 != dev->display_width) && (0 != dev->display_height)){
		logo_size = dev->display_width * dev->display_height * (dev->bpp / 8);
	}else
#endif
	logo_size = dev->panel->width * dev->panel->height * (dev->bpp / 8);
#if 0
#ifndef USE_OVERLAY_BUFF
	kernel_fb_size = dev->panel->width * dev->panel->height * (dev->bpp / 8);
	kernel_fb_size = 0;
	logo_dst_v = dev->fb->screen_base + kernel_fb_size;//use the second frame buffer
	logo_dst_p = dev->fb->fix.smem_start + kernel_fb_size;//use the second frame buffer
#else
	logo_dst_p = SPRD_ION_OVERLAY_BASE-logo_size;//use overlay frame buffer
	logo_dst_v =  (uint32_t)ioremap(logo_dst_p, logo_size);
#endif
#else
	dev->logo_buffer_size = logo_size;
	dev->logo_buffer_addr_v = (void*)__get_free_pages(GFP_ATOMIC | __GFP_ZERO , get_order(logo_size));
	if (!dev->logo_buffer_addr_v) {
		printk(KERN_ERR "sprdfb: %s Failed to allocate logo proc buffer\n", __FUNCTION__);
		return;
	}
	printk(KERN_INFO "sprdfb: got %d bytes logo proc buffer at 0x%p\n", logo_size,
		dev->logo_buffer_addr_v);

	logo_dst_v = dev->logo_buffer_addr_v;
	logo_dst_p = __pa(dev->logo_buffer_addr_v);

#endif
	logo_src_v =  ioremap(lcd_base_from_uboot, logo_size);
	if (!logo_src_v || !logo_dst_v) {
		printk(KERN_ERR "sprdfb: %s[%d]: Unable to map boot logo memory: src-0x%p, dst-0x%p\n", __func__,
		    __LINE__,logo_src_v, logo_dst_v);
		return;
	}

	printk("sprdfb: %s[%d]: lcd_base_from_uboot: 0x%lx, logo_src_v:0x%p\n",__func__,__LINE__,lcd_base_from_uboot,logo_src_v);
	printk("sprdfb: %s[%d]: logo_dst_p:0x%lx,logo_dst_v:0x%p\n",__func__,__LINE__,logo_dst_p,logo_dst_v);
	memcpy(logo_dst_v, logo_src_v, logo_size);
	dma_sync_single_for_device(dev->fb->dev, logo_dst_p, logo_size, DMA_TO_DEVICE);

	iounmap(logo_src_v);

#if 0
#ifdef USE_OVERLAY_BUFF
	iounmap(logo_dst_v);
#endif
#endif
	//dispc_print_osd_config(__func__,__LINE__);
	dispc_write(logo_dst_p, DISPC_OSD_BASE_ADDR);
	sprdfb_dispc_refresh_logo(dev);
	//dispc_print_osd_config(__func__,__LINE__);
}

//end bug210112
#ifdef CONFIG_FB_MMAP_CACHED
void sprdfb_set_vma(struct vm_area_struct *vma)
{
	if(NULL != vma){
		dispc_ctx.vma = vma;
	}
}
#endif

int32_t sprdfb_is_refresh_done(struct sprdfb_device *dev)
{
	printk("sprdfb: sprdfb_is_refresh_done vsync_done=%d",dispc_ctx.vsync_done);
	return (int32_t)dispc_ctx.vsync_done;
}


struct display_ctrl sprdfb_dispc_ctrl = {
	.name		= "dispc",
	.early_init		= sprdfb_dispc_early_init,
	.init		 	= sprdfb_dispc_init,
	.uninit		= sprdfb_dispc_uninit,
	.refresh		= sprdfb_dispc_refresh,
	.logo_proc		= sprdfb_dispc_logo_proc,
	.shutdown		= sprdfb_dispc_shutdown,
	.suspend		= sprdfb_dispc_suspend,
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


