/*
* Copyright (c) 2014 TRUSTONIC LIMITED
* All Rights Reserved.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#include <linux/types.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/clk.h>
#include <linux/console.h>
#include <linux/fb.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_ion.h>
#include <linux/dma-buf.h>
#include <linux/ion.h>
#include <t-base-tui.h>
#include <linux/ktime.h>
#include <linux/switch.h>
#include "../../../../video/exynos/decon/decon.h"
#include "tui_ioctl.h"
#include "dciTui.h"
#include "tlcTui.h"
#include "tui-hal.h"
#if defined(CONFIG_TOUCHSCREEN_FTS) || defined(CONFIG_TOUCHSCREEN_FTS5AD56)
#include <linux/i2c/fts.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT75XX)
#include <linux/i2c/zinitix_bt532_ts.h>
#endif
#if defined(CONFIG_TOUCHSCREEN_SEC_TS)
#include "../../../input/touchscreen/sec_ts/sec_ts.h"
#endif

/* I2C register for reset */
#define HSI2C7_PA_BASE_ADDRESS	0x14E10000
#define HSI2C_CTL		0x00
#define HSI2C_TRAILIG_CTL	0x08
#define HSI2C_FIFO_STAT		0x30
#define HSI2C_CONF		0x40
#define HSI2C_TRANS_STATUS	0x50

#define HSI2C_SW_RST		(1u << 31)
#define HSI2C_FUNC_MODE_I2C	(1u << 0)
#define HSI2C_MASTER		(1u << 3)
#define HSI2C_TRAILING_COUNT	(0xf)
#define HSI2C_AUTO_MODE		(1u << 31)
#define HSI2C_RX_FIFO_EMPTY	(1u << 24)
#define HSI2C_TX_FIFO_EMPTY	(1u << 8)
#define HSI2C_FIFO_EMPTY	(HSI2C_RX_FIFO_EMPTY | HSI2C_TX_FIFO_EMPTY)
#define TUI_MEMPOOL_SIZE	0

extern struct switch_dev tui_switch;

extern phys_addr_t hal_tui_video_space_alloc(void);
extern int decon_lpd_block_exit(struct decon_device *decon);

/* for ion_map mapping on smmu */
extern struct ion_device *ion_exynos;
/* ------------end ---------- */

#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
static int tsp_irq_num = 11; // default value

static void tui_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

void trustedui_set_tsp_irq(int irq_num)
{
	tsp_irq_num = irq_num;
	pr_info("%s called![%d]\n",__func__, irq_num);
}
#endif

struct tui_mempool {
	void * va;
	phys_addr_t pa;
	size_t size;
};

static struct tui_mempool g_tuiMemPool;

static struct ion_client *client;
static struct ion_handle *handle;

static bool allocateTuiMemoryPool(struct tui_mempool *pool, size_t size)
{
	bool ret = false;
	void *tuiMemPool = NULL;

	pr_info("%s %s:%d\n", __FUNCTION__, __FILE__, __LINE__);
	if (!size) {
		pr_debug("TUI frame buffer: nothing to allocate.");
		return true;
	}

	tuiMemPool = kmalloc(size, GFP_KERNEL);
	if (!tuiMemPool) {
		pr_debug("ERROR Could not allocate TUI memory pool");
	}
	else if (ksize(tuiMemPool) < size) {
		pr_debug("ERROR TUI memory pool allocated size is too small. required=%zd allocated=%zd", size, ksize(tuiMemPool));
		kfree(tuiMemPool);
	}
	else {
		pool->va = tuiMemPool;
		pool->pa = virt_to_phys(tuiMemPool);
		pool->size = ksize(tuiMemPool);
		ret = true;
	}
	return ret;
}

static void freeTuiMemoryPool(struct tui_mempool *pool)
{
	if(pool->va) {
		kfree(pool->va);
		memset(pool, 0, sizeof(*pool));
	}
}

void hold_i2c_clock(void)
{
	struct clk *touch_i2c_pclk;
	touch_i2c_pclk = clk_get(NULL, "i2c2_pclk");
	if (IS_ERR(touch_i2c_pclk)) {
		pr_err("Can't get [i2c2_pclk]\n");
	}

	clk_prepare_enable(touch_i2c_pclk);

	pr_info("[i2c2_pclk] will be enabled\n");
	clk_put(touch_i2c_pclk);
}

void release_i2c_clock(void)
{
	struct clk *touch_i2c_pclk;
	touch_i2c_pclk = clk_get(NULL, "i2c2_pclk");
	if (IS_ERR(touch_i2c_pclk)) {
		pr_err("Can't get [i2c2_pclk]\n");
	}

	clk_disable_unprepare(touch_i2c_pclk);

	pr_info("[i2c2_pclk] will be disabled\n");

	clk_put(touch_i2c_pclk);
}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
#if LINUX_VERSION_CODE <= KERNEL_VERSION(3,9,0)
static int is_device_ok(struct device *fbdev, void *p)
#else
static int is_device_ok(struct device *fbdev, const void *p)
#endif
{
	return 1;
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static struct device *get_fb_dev(void)
{
	struct device *fbdev = NULL;

	/* get the first framebuffer device */
	/* [TODO] Handle properly when there are more than one framebuffer */
	fbdev = class_find_device(fb_class, NULL, NULL, is_device_ok);
	if (NULL == fbdev) {
		pr_debug("ERROR cannot get framebuffer device\n");
		return NULL;
	}
	return fbdev;
	}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static struct fb_info *get_fb_info(struct device *fbdev)
{
	struct fb_info *fb_info;

	if (!fbdev->p) {
		pr_debug("ERROR framebuffer device has no private data\n");
		return NULL;
	}

	fb_info = (struct fb_info *) dev_get_drvdata(fbdev);
	if (!fb_info) {
		pr_debug("ERROR framebuffer device has no fb_info\n");
		return NULL;
	}

	return fb_info;
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static void fb_tui_protection(void)
{
	struct device *fbdev = NULL;
	struct fb_info *fb_info;
	struct decon_win *win;
	struct decon_device *decon;

	fbdev = get_fb_dev();
	if (!fbdev) {
		pr_debug("get_fb_dev failed\n");
		return;
	}

	fb_info = get_fb_info(fbdev);
	if (!fb_info) {
		pr_debug("get_fb_info failed\n");
		return;
	}

	win = fb_info->par;
	decon = win->decon;

	lock_fb_info(fb_info);
	// FIX ME !! //
	decon_tui_protection(decon, true);
	unlock_fb_info(fb_info);
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static void fb_tui_unprotection(void)
{
	struct device *fbdev = NULL;
	struct fb_info *fb_info;
	struct decon_win *win;
	struct decon_device *decon;

	fbdev = get_fb_dev();
	if (!fbdev) {
		pr_debug("get_fb_dev failed\n");
		return;
	}

	fb_info = get_fb_info(fbdev);
	if (!fb_info) {
		printk("get_fb_info failed\n");
		return;
	}

	win = fb_info->par;
	if (win == NULL) {
		printk("get win failed\n");
		return;
	}

	decon = win->decon;
	if (decon == NULL) {
		printk("get decon failed\n");
		return;
	}

	/*
	if (decon->pdata->trig_mode == DECON_HW_TRIG)
	decon_reg_set_trigger(decon->id, decon->pdata->dsi_mode,
	decon->pdata->trig_mode, DECON_TRIG_ENABLE);
	*/
	lock_fb_info(fb_info);
	// FIX ME //
	decon_tui_protection(decon, false);
	unlock_fb_info(fb_info);
}
#endif

uint32_t hal_tui_init(void)
{
	/* Allocate memory pool for the framebuffer
	 */
	if (!allocateTuiMemoryPool(&g_tuiMemPool, TUI_MEMPOOL_SIZE)) {
		return TUI_DCI_ERR_INTERNAL_ERROR;
	}

	return TUI_DCI_OK;
}

void hal_tui_exit(void)
{
	/* delete memory pool if any */
	if (g_tuiMemPool.va) {
		freeTuiMemoryPool(&g_tuiMemPool);
	}
}

uint32_t hal_tui_alloc(tuiAllocBuffer_t *allocbuffer, size_t allocsize, uint32_t count)
{
	int ret = TUI_DCI_ERR_INTERNAL_ERROR;
	ion_phys_addr_t phys_addr;
	unsigned long offset = 0;
	size_t size;

	size=allocsize*(count+1);

	client = ion_client_create(ion_exynos, "TUI module");
	if (client == NULL) {
		printk("[%s:%d] ION client creation fail.\n", __func__, __LINE__);
		return ret;
	}

	handle = ion_alloc(client, size, 0, EXYNOS_ION_HEAP_EXYNOS_CONTIG_MASK,
		ION_EXYNOS_VIDEO_MASK);

	printk("[%s:%s:%d] Check ION alloc result.\n", __FILE__, __func__, __LINE__);
	if (IS_ERR_OR_NULL(handle)) {
		printk("[%s:%d] ION memory allocation fail.\n", __func__, __LINE__);
		return ret;
	}
	/* FIX ME */
	printk("[%s:%s:%d] request physical address\n", __FILE__, __func__, __LINE__);
	if (1)
		ion_phys(client, handle, &phys_addr, &size);
	else
		phys_addr = 0xBA000000;

	/* TUI frame buffer must be aligned 8M */
	if(phys_addr % 0x800000){
		offset = 0x800000 - (phys_addr % 0x800000);
		phys_addr = phys_addr+offset;
	}

	printk("phys_addr : %lx\n",phys_addr);

	g_tuiMemPool.pa = phys_addr;
	g_tuiMemPool.size = allocsize*count;

	if ((size_t)(allocsize*count) <= g_tuiMemPool.size) {
		allocbuffer[0].pa = (uint64_t) g_tuiMemPool.pa;
		allocbuffer[1].pa = (uint64_t) (g_tuiMemPool.pa + g_tuiMemPool.size/count);
	}else{
		/* requested buffer is bigger than the memory pool, return an error */
		pr_debug("%s(%d): %s\n", __func__, __LINE__, "Memory pool too small");
		ret = TUI_DCI_ERR_INTERNAL_ERROR;
		return ret;
	}

	ret = TUI_DCI_OK;

	return ret;
}

#ifdef CONFIG_TRUSTED_UI_TOUCH_ENABLE
void tui_i2c_reset(void)
{
	void __iomem *i2c_reg;
	u32 tui_i2c;
	u32 i2c_conf;

	i2c_reg = ioremap(HSI2C7_PA_BASE_ADDRESS, SZ_4K);
	tui_i2c = readl(i2c_reg + HSI2C_CTL);
	tui_i2c |= HSI2C_SW_RST;
	writel(tui_i2c, i2c_reg + HSI2C_CTL);

	tui_i2c = readl(i2c_reg + HSI2C_CTL);
	tui_i2c &= ~HSI2C_SW_RST;
	writel(tui_i2c, i2c_reg + HSI2C_CTL);

	writel(0x4c4c4c00, i2c_reg + 0x0060);
	writel(0x26004c4c, i2c_reg + 0x0064);
	writel(0x99, i2c_reg + 0x0068);

	i2c_conf = readl(i2c_reg + HSI2C_CONF);
	writel((HSI2C_FUNC_MODE_I2C | HSI2C_MASTER), i2c_reg + HSI2C_CTL);

	writel(HSI2C_TRAILING_COUNT, i2c_reg + HSI2C_TRAILIG_CTL);
	writel(i2c_conf | HSI2C_AUTO_MODE, i2c_reg + HSI2C_CONF);

	iounmap(i2c_reg);
}
#endif



void hal_tui_free(void)
{
	ion_free(client, handle);
	ion_client_destroy(client);
}

uint32_t hal_tui_deactivate(void)
{

	switch_set_state(&tui_switch, TRUSTEDUI_MODE_VIDEO_SECURED);
	pr_info(KERN_ERR "Disable touch!\n");
	disable_irq(tsp_irq_num);

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT75XX) || defined(CONFIG_TOUCHSCREEN_SEC_TS)
		tui_delay(5);
		/*esd timer disable*/
		trustedui_mode_on();
		tui_delay(95);
#else
		tui_delay(1);
#endif

	pr_info(KERN_ERR "tsp_irq_num =%d\n",tsp_irq_num);

	/* Set linux TUI flag */
	trustedui_set_mask(TRUSTEDUI_MODE_TUI_SESSION);
	trustedui_blank_set_counter(0);

	hold_i2c_clock();

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info(KERN_ERR "blanking!\n");
	fb_tui_protection();
#endif
	trustedui_set_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);

	pr_info(KERN_ERR "Ready to use TUI!\n");

	return TUI_DCI_OK;
}

uint32_t hal_tui_activate(void)
{
	// Protect NWd
	trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info("Unblanking\n");
	fb_tui_unprotection();
#endif

	switch_set_state(&tui_switch, TRUSTEDUI_MODE_OFF);
//	tui_i2c_reset();
	enable_irq(tsp_irq_num);
	release_i2c_clock();

#if defined(CONFIG_TOUCHSCREEN_ZINITIX_ZT75XX)
	tui_delay(5);
	/*esd timer enable*/
	trustedui_mode_off();
	tui_delay(95);
#endif

	/* Clear linux TUI flag */
	trustedui_set_mode(TRUSTEDUI_MODE_OFF);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info("Unsetting TUI flag (blank counter=%d)", trustedui_blank_get_counter());

if (0 < trustedui_blank_get_counter()) {
	//		blank_framebuffer(0);
}
#endif
	pr_info(KERN_ERR "Closed TUI session.\n");

	return TUI_DCI_OK;
}
