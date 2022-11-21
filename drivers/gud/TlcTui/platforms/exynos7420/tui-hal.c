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
#elif defined(CONFIG_TOUCHSCREEN_MELFAS_MMS449)
#include <linux/input/mt.h>
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

static struct decon_dma_buf_data dma;

struct tui_mempool {
	void * va;
	phys_addr_t pa;
	size_t size;
};

static struct tui_mempool g_tuiMemPool;

static u32 va;
static struct ion_client *client;
static struct ion_handle *handle;
static struct dma_buf *dbuf;

static bool allocateTuiMemoryPool(struct tui_mempool *pool, size_t size)
{
	bool ret = false;
	void * tuiMemPool = NULL;

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
	touch_i2c_pclk = clk_get(NULL, "pclk-hsi2c7");
	if (IS_ERR(touch_i2c_pclk)) {
		pr_err("Can't get [pclk-hsi2c7]\n");
	}

	clk_prepare_enable(touch_i2c_pclk);

	pr_info("[pclk-hsi2c7] will be enabled\n");
	clk_put(touch_i2c_pclk);
}

void release_i2c_clock(void)
{
	struct clk *touch_i2c_pclk;
	touch_i2c_pclk = clk_get(NULL, "pclk-hsi2c7");
	if (IS_ERR(touch_i2c_pclk)) {
		pr_err("Can't get [pclk-hsi2c7]\n");
	}

	clk_disable_unprepare(touch_i2c_pclk);

	pr_info("[pclk-hsi2c7] will be disabled\n");

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
static int fb_tui_protection(void)
{
	struct device *fbdev = NULL;
	struct fb_info *fb_info;
	struct decon_win *win;
	struct decon_device *decon;
	int ret = -ENODEV;

	fbdev = get_fb_dev();
	if (!fbdev) {
		pr_debug("get_fb_dev failed\n");
		return ret;
	}

	fb_info = get_fb_info(fbdev);
	if (!fb_info) {
		pr_debug("get_fb_info failed\n");
		return ret;
	}

	win = fb_info->par;
	decon = win->decon;

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_get_sync(decon->dev);
#endif

	lock_fb_info(fb_info);	
	ret = decon_tui_protection(decon, true);
	unlock_fb_info(fb_info);
	return ret;

#if 0 // time check
	ktime_t start, end;
	long long ns;

	start = ktime_get();            /* get time stamp before execution */
#endif
//	decon_tui_protection(decon, true);
#if 0 // time check
	end = ktime_get();              /* get time stamp after execution */

	ns = ktime_to_ns( ktime_sub(end, start) ); /* get the elapsed time in nano-seconds */
	printk(KERN_INFO "[TIME_CHECK] blank -> Elapsed time %lld ns\n", ns); /* print it on the console */
#endif
}

static void set_va_to_decon(u32 va)
{
	decon_reg_set_tui_va(0, va);
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
static int fb_tui_unprotection(void)
{
	struct device *fbdev = NULL;
	struct fb_info *fb_info;
	struct decon_win *win;
	struct decon_device *decon;
	int ret = -ENODEV;

	fbdev = get_fb_dev();
	if (!fbdev) {
		pr_debug("get_fb_dev failed\n");
		return ret;
	}

	fb_info = get_fb_info(fbdev);
	if (!fb_info) {
		printk("get_fb_info failed\n");
		return ret;
	}

	win = fb_info->par;
	decon = win->decon;

#ifdef CONFIG_PM_RUNTIME
	pm_runtime_put_sync(decon->dev);
#endif
	lock_fb_info(fb_info);
	ret = decon_tui_protection(decon, false);
	unlock_fb_info(fb_info);
	return ret; 
//	decon_tui_protection(decon, false);
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

dma_addr_t decon_map_sec_dma_buf(struct dma_buf *dbuf, int plane)
{
        struct decon_device *decon = get_decon_drvdata(0); /* 0: decon Int ID */

        if (!dbuf || (plane >= MAX_BUF_PLANE_CNT) || (plane < 0))
                return -EINVAL;

        dma.ion_handle = NULL;
        dma.fence = NULL;

        dma.dma_buf = dbuf;
	dma.attachment = dma_buf_attach(dbuf, decon->dev);

        if (IS_ERR(dma.attachment)) {
		decon_err("dma_buf_attach() failed: %ld\n",
				PTR_ERR(dma.attachment));
		goto err_buf_map_attach;
	}

	dma.sg_table = dma_buf_map_attachment(dma.attachment,
			DMA_TO_DEVICE);

	if (IS_ERR(dma.sg_table)) {
		decon_err("dma_buf_map_attachment() failed: %ld\n",
				PTR_ERR(dma.sg_table));
		goto err_buf_map_attachment;
	}

	dma.dma_addr = ion_iovmm_map(dma.attachment, 0,
			dma.dma_buf->size, DMA_TO_DEVICE, plane);

	if (IS_ERR_VALUE(dma.dma_addr)) {
		decon_err("iovmm_map() failed: %pa\n", &dma.dma_addr);
		goto err_iovmm_map;
	}

	exynos_ion_sync_dmabuf_for_device(decon->dev, dma.dma_buf,
			dma.dma_buf->size, DMA_TO_DEVICE);

	return dma.dma_addr;

err_iovmm_map:
	dma_buf_unmap_attachment(dma.attachment, dma.sg_table,
			DMA_TO_DEVICE);
err_buf_map_attachment:
	dma_buf_detach(dma.dma_buf, dma.attachment);
err_buf_map_attach:
        return 0;
}

uint32_t hal_tui_alloc(tuiAllocBuffer_t allocbuffer[MAX_DCI_BUFFER_NUMBER],
		size_t allocsize, uint32_t count)
{
	int ret = TUI_DCI_ERR_INTERNAL_ERROR;
	dma_addr_t buf_addr;
	ion_phys_addr_t phys_addr;
	unsigned long offset = 0;
	size_t size;
	dbuf = NULL;

	size=allocsize*(count+1);

	client = ion_client_create(ion_exynos, "TUI module");
    if (IS_ERR(client)) {
        pr_err("failed to ion_client_create\n");
        return ret;
    }

	handle = ion_alloc(client, size, 0, EXYNOS_ION_HEAP_EXYNOS_CONTIG_MASK,
							ION_EXYNOS_VIDEO_MASK);
    if (IS_ERR_OR_NULL(handle)) {
        pr_err("[%s:%d] ION memory allocation fail.[err:%lx]\n", __func__, __LINE__, PTR_ERR(handle));
		handle = NULL;
        hal_tui_free();
        return ret;
    }

	dbuf = ion_share_dma_buf(client, handle);
	buf_addr = decon_map_sec_dma_buf(dbuf, 0);

	ion_phys(client, handle, (unsigned long *)&phys_addr, &dbuf->size);

	/* TUI frame buffer must be aligned 16M */
	if(phys_addr % 0x1000000){
		offset = 0x1000000 - (phys_addr % 0x1000000);
	}

	phys_addr = phys_addr+offset;
	va = buf_addr + offset;
	printk("buf_addr : %x\n",va);
	printk("phys_addr : %lx\n",phys_addr);
#if 0 // this is testing. MUST BE REMOVE
	void *kernel_addr;
	//kernel_addr = (void*)ion_map_kernel(client, handle);
	kernel_addr = phys_to_virt(phys_addr+0x2000000);
	*((u32*)kernel_addr) = va;
	printk("DATA ON phys_addr : addr[%lx] val[%x]\n"
			,phys_addr+0x2000000
			,*((u32*)kernel_addr));
#endif

        g_tuiMemPool.pa = phys_addr;
        g_tuiMemPool.size = allocsize*count;

	if ((size_t)(allocsize*count) <= g_tuiMemPool.size) {
		allocbuffer[0].pa = (uint64_t) g_tuiMemPool.pa;
		allocbuffer[1].pa = (uint64_t) (g_tuiMemPool.pa + g_tuiMemPool.size/2);
	}else{
                /* requested buffer is bigger than the memory pool, return an
                   error */
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

void decon_free_sec_dma_buf(int plane)
{
	struct decon_device *decon = get_decon_drvdata(0); /* 0: decon Int ID */ 

	if (IS_ERR_VALUE(dma.dma_addr) || !dma.dma_buf)
		return;

	ion_iovmm_unmap(dma.attachment, dma.dma_addr);

	dma_buf_unmap_attachment(dma.attachment, dma.sg_table,
		DMA_TO_DEVICE);

	exynos_ion_sync_dmabuf_for_cpu(decon->dev, dma.dma_buf,
				dma.dma_buf->size, DMA_FROM_DEVICE);

	dma_buf_detach(dma.dma_buf, dma.attachment);
	memset(&dma, 0, sizeof(dma));
}

void hal_tui_free(void)
{
	if (dbuf != NULL) {
		decon_free_sec_dma_buf(0);
		dma_buf_put(dbuf);
	}
	if (handle != NULL){
		ion_free(client, handle);
	}
	ion_client_destroy(client);
}

uint32_t hal_tui_deactivate(void)
{
	int ret;
	switch_set_state(&tui_switch, TRUSTEDUI_MODE_VIDEO_SECURED);
	pr_info(KERN_ERR "Disable touch!\n");
	disable_irq(tsp_irq_num);
#if defined(CONFIG_TOUCHSCREEN_FTS) || defined(CONFIG_TOUCHSCREEN_FTS5AD56)|| defined(CONFIG_TOUCHSCREEN_MELFAS_MMS449)
	tui_delay(5);
	trustedui_mode_on();
	tui_delay(95);
#else
	tui_delay(100);
#endif
	/* Set linux TUI flag */
	trustedui_set_mask(TRUSTEDUI_MODE_TUI_SESSION);
	trustedui_blank_set_counter(0);
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info(KERN_ERR "blanking!\n");

	hold_i2c_clock();

	ret = fb_tui_protection();
	if(ret < 0){
		pr_info(KERN_ERR "Decon state disable!, ret = %d\n", ret);
		goto err;
	}
	set_va_to_decon(va);
#endif
	trustedui_set_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
	pr_info(KERN_ERR "blanking!\n");

	return TUI_DCI_OK;

err:	
	/* Clear linux TUI flag */
//	trustedui_blank_get_counter();
	trustedui_clear_mask(TRUSTEDUI_MODE_OFF);
	enable_irq(tsp_irq_num);
	switch_set_state(&tui_switch, TRUSTEDUI_MODE_OFF);
	return TUI_DCI_ERR_INTERNAL_ERROR;
}

uint32_t hal_tui_activate(void)
{
	int ret;
	// Protect NWd
	trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info("Unblanking\n");

	ret = fb_tui_unprotection();
	if(ret < 0){
		pr_info(KERN_ERR "Decon state disable!, ret = %d\n", ret);
		goto err;
	}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI_FB_BLANK
	pr_info("Unsetting TUI flag (blank counter=%d)", trustedui_blank_get_counter());
	if (0 < trustedui_blank_get_counter()) {
//		blank_framebuffer(0);
	}
#endif
	switch_set_state(&tui_switch, TRUSTEDUI_MODE_OFF);
	tui_i2c_reset();
	enable_irq(tsp_irq_num);
	release_i2c_clock();

	/* Clear linux TUI flag */
	trustedui_set_mode(TRUSTEDUI_MODE_OFF);
	pr_info("Enable touch! & Clear TUI MASK\n");

	return TUI_DCI_OK;

err:
	/* Clear linux TUI flag */
	trustedui_set_mode(TRUSTEDUI_MODE_OFF);
	switch_set_state(&tui_switch, TRUSTEDUI_MODE_OFF);
	enable_irq(tsp_irq_num);
	return TUI_DCI_ERR_INTERNAL_ERROR;
}
