/*
 * linux/arch/arm/mach-mmp/pxa988.c
 *
 * code name PXA988
 *
 * Copyright (C) 2012 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_data/mmp_audio.h>
#include <linux/notifier.h>
#include <linux/memblock.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/clk.h>
#include <linux/ion.h>
#include <linux/dma-mapping.h>
#include <linux/persistent_ram.h>

#include <asm/smp_twd.h>
#include <asm/mach/time.h>
#include <asm/hardware/gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/cacheflush.h>

#include <mach/addr-map.h>
#include <mach/regs-ciu.h>
#include <mach/regs-apbc.h>
#include <mach/regs-apmu.h>
#include <mach/regs-mpmu.h>
#include <mach/cputype.h>
#include <mach/irqs.h>
#include <mach/gpio.h>
#include <mach/gpio-edge.h>
#include <mach/dma.h>
#include <mach/devices.h>
#include <mach/pxa988.h>
#include <mach/regs-timers.h>
#include <mach/regs-usb.h>
#include <mach/soc_coda7542.h>
#include <mach/isp_dev.h>
#include <mach/uio_isp.h>
#include <mach/reset-pxa988.h>
#include <mach/pxa168fb.h>
#include <plat/mfp.h>
#include <mach/gpu_mem.h>

#include "common.h"

#define MFPR_VIRT_BASE	(APB_VIRT_BASE + 0x1e000)
#define RIPC3_VIRT_BASE	(APB_VIRT_BASE + 0x3D000)
#define GPIOE_VIRT_BASE	(APB_VIRT_BASE + 0x19800)
#define RIPC3_STATUS	(RIPC3_VIRT_BASE + 0x300)

static struct mfp_addr_map pxa988_addr_map[] __initdata = {

	MFP_ADDR_X(GPIO0, GPIO54, 0xdc),
	MFP_ADDR_X(GPIO67, GPIO98, 0x1b8),
	MFP_ADDR_X(GPIO100, GPIO109, 0x238),
	MFP_ADDR_X(GPIO110, GPIO116, 0x298),

	MFP_ADDR(DF_IO0, 0x40),
	MFP_ADDR(DF_IO1, 0x3c),
	MFP_ADDR(DF_IO2, 0x38),
	MFP_ADDR(DF_IO3, 0x34),
	MFP_ADDR(DF_IO4, 0x30),
	MFP_ADDR(DF_IO5, 0x2c),
	MFP_ADDR(DF_IO6, 0x28),
	MFP_ADDR(DF_IO7, 0x24),
	MFP_ADDR(DF_IO8, 0x20),
	MFP_ADDR(DF_IO9, 0x1c),
	MFP_ADDR(DF_IO10, 0x18),
	MFP_ADDR(DF_IO11, 0x14),
	MFP_ADDR(DF_IO12, 0x10),
	MFP_ADDR(DF_IO13, 0xc),
	MFP_ADDR(DF_IO14, 0x8),
	MFP_ADDR(DF_IO15, 0x4),

	MFP_ADDR(DF_nCS0_SM_nCS2, 0x44),
	MFP_ADDR(DF_nCS1_SM_nCS3, 0x48),
	MFP_ADDR(SM_nCS0, 0x4c),
	MFP_ADDR(SM_nCS1, 0x50),
	MFP_ADDR(DF_WEn, 0x54),
	MFP_ADDR(DF_REn, 0x58),
	MFP_ADDR(DF_CLE_SM_OEn, 0x5c),
	MFP_ADDR(DF_ALE_SM_WEn, 0x60),
	MFP_ADDR(SM_SCLK, 0x64),
	MFP_ADDR(DF_RDY0, 0x68),
	MFP_ADDR(SM_BE0, 0x6c),
	MFP_ADDR(SM_BE1, 0x70),
	MFP_ADDR(SM_ADV, 0x74),
	MFP_ADDR(DF_RDY1, 0x78),
	MFP_ADDR(SM_ADVMUX, 0x7c),
	MFP_ADDR(SM_RDY, 0x80),
	MFP_ADDR(ANT_SW4, 0x26c),

	MFP_ADDR_X(MMC1_DAT7, MMC1_WP, 0x84),

	MFP_ADDR(GPIO123, 0xcc),
	MFP_ADDR(GPIO124, 0xd0),
	MFP_ADDR(VCXO_REQ, 0xd4),
	MFP_ADDR(VCXO_OUT, 0xd8),

	MFP_ADDR(CLK_REQ, 0xcc),

	MFP_ADDR_END,
};

/*
 * gc, vpu, isp will access the same regsiter to pwr on/off,
 * add spinlock to protect the sequence
 */
static DEFINE_SPINLOCK(gc_vpu_isp_pwr_lock);

/* used to protect GC power sequence */
static DEFINE_SPINLOCK(gc_pwr_lock);

/* GC power control */
#define GC_USE_HW_PWRCTRL	1
#define GC_AUTO_PWR_ON		(0x1 << 0)

#define GC_CLK_EN	\
	((0x1 << 3) | (0x1 << 4) | (0x1 << 5))

#define GC_ACLK_RST	(0x1 << 0)
#define GC_FCLK_RST	(0x1 << 1)
#define GC_HCLK_RST	(0x1 << 2)
#define GC_CLK_RST	\
	(GC_ACLK_RST | GC_FCLK_RST | GC_HCLK_RST)

#define GC_ISOB		(0x1 << 8)
#define GC_PWRON1	(0x1 << 9)
#define GC_PWRON2	(0x1 << 10)
#define GC_HWMODE	(0x1 << 11)

#define GC_FCLK_SEL_MASK	(0x3 << 6)
#define GC_ACLK_SEL_MASK	(0x3 << 20)
#define GC_FCLK_DIV_MASK	(0x7 << 12)
#define GC_ACLK_DIV_MASK	(0x7 << 17)
#define GC_FCLK_REQ		(0x1 << 15)
#define GC_ACLK_REQ		(0x1 << 16)

#define GC_CLK_SEL_WIDTH	(2)
#define GC_CLK_DIV_WIDTH	(3)
#define GC_FCLK_SEL_SHIFT	(6)
#define GC_ACLK_SEL_SHIFT	(20)
#define GC_FCLK_DIV_SHIFT	(12)
#define GC_ACLK_DIV_SHIFT	(17)

#define GC_REG_WRITE(val)	{	\
	__raw_writel(val, APMU_GC);	\
}

void gc_pwr(int power_on)
{
	unsigned int val = __raw_readl(APMU_GC);
	int timeout = 5000;

	spin_lock(&gc_pwr_lock);

	if (power_on) {
#ifdef GC_USE_HW_PWRCTRL
		/* enable hw mode */
		val |= GC_HWMODE;
		GC_REG_WRITE(val);

		spin_lock(&gc_vpu_isp_pwr_lock);
		/* set PWR_BLK_TMR_REG to recommend value */
		__raw_writel(0x20001FFF, APMU_PWR_BLK_TMR_REG);

		/* pwr on GC */
		val = __raw_readl(APMU_PWR_CTRL_REG);
		val |= GC_AUTO_PWR_ON;
		__raw_writel(val, APMU_PWR_CTRL_REG);
		spin_unlock(&gc_vpu_isp_pwr_lock);

		/* polling pwr status */
		while (!(__raw_readl(APMU_PWR_STATUS_REG) & GC_AUTO_PWR_ON)) {
			udelay(200);
			timeout -= 200;
			if (timeout < 0) {
				pr_err("%s: power on timeout\n", __func__);
				return;
			}
		}
#else
		/* enable bus and function clock  */
		val |= GC_CLK_EN;
		GC_REG_WRITE(val);

		/* enable power_on1, wait at least 200us */
		val |= GC_PWRON1;
		GC_REG_WRITE(val);
		udelay(200);

		/* enable power_on2 */
		val |= GC_PWRON2;
		GC_REG_WRITE(val);

		/* fRst release */
		val |= GC_FCLK_RST;
		GC_REG_WRITE(val);
		udelay(100);

		/* aRst hRst release at least 48 cycles later than fRst */
		val |= (GC_ACLK_RST | GC_HCLK_RST);
		GC_REG_WRITE(val);

		/* disable isolation */
		val |= GC_ISOB;
		GC_REG_WRITE(val);
#endif
	} else {
#ifdef GC_USE_HW_PWRCTRL
		spin_lock(&gc_vpu_isp_pwr_lock);
		/* pwr on GC */
		val = __raw_readl(APMU_PWR_CTRL_REG);
		val &= ~GC_AUTO_PWR_ON;
		__raw_writel(val, APMU_PWR_CTRL_REG);
		spin_unlock(&gc_vpu_isp_pwr_lock);

		/* polling pwr status */
		while ((__raw_readl(APMU_PWR_STATUS_REG) & GC_AUTO_PWR_ON)) {
			udelay(200);
			timeout -= 200;
			if (timeout < 0) {
				pr_err("%s: power off timeout\n", __func__);
				return;
			}
		}
#else
		/* enable isolation */
		val &= ~GC_ISOB;
		GC_REG_WRITE(val);

		/* disable power_on2 */
		val &= ~GC_PWRON2;
		GC_REG_WRITE(val);

		/* disable power_on1 */
		val &= ~GC_PWRON1;
		GC_REG_WRITE(val);

		/* fRst aRst hRst */
		val &= ~(GC_CLK_RST | GC_CLK_EN);
		GC_REG_WRITE(val);
		udelay(100);

#endif
	}
	spin_unlock(&gc_pwr_lock);
}
EXPORT_SYMBOL(gc_pwr);

#define VPU_HW_MODE	(0x1 << 19)
#define VPU_AUTO_PWR_ON	(0x1 << 2)
#define VPU_PWR_STAT	(0x1 << 2)

void coda7542_power_switch(int on)
{
	unsigned int val;
	int timeout = 2000;

	/* HW mode power on */
	if (on) {
		/* set VPU HW on/off mode  */
		val = __raw_readl(APMU_VPU_CLK_RES_CTRL);
		val |= VPU_HW_MODE;
		__raw_writel(val, APMU_VPU_CLK_RES_CTRL);

		spin_lock(&gc_vpu_isp_pwr_lock);
		/* on1, on2, off timer */
		__raw_writel(0x20001fff, APMU_PWR_BLK_TMR_REG);

		/* VPU auto power on */
		val = __raw_readl(APMU_PWR_CTRL_REG);
		val |= VPU_AUTO_PWR_ON;
		__raw_writel(val, APMU_PWR_CTRL_REG);
		spin_unlock(&gc_vpu_isp_pwr_lock);
		/*
		 * VPU power on takes 316us, usleep_range(280,290) takes about
		 * 300~320us, so it can reduce the duty cycle.
		 */
		usleep_range(280, 290);

		/* polling VPU_PWR_STAT bit */
		while (!(__raw_readl(APMU_PWR_STATUS_REG) & VPU_PWR_STAT)) {
			udelay(1);
			timeout -= 1;
			if (timeout < 0) {
				pr_err("%s: VPU power on timeout\n", __func__);
				return;
			}
		}
	/* HW mode power off */
	} else {
		spin_lock(&gc_vpu_isp_pwr_lock);
		/* VPU auto power off */
		val = __raw_readl(APMU_PWR_CTRL_REG);
		val &= ~VPU_AUTO_PWR_ON;
		__raw_writel(val, APMU_PWR_CTRL_REG);
		spin_unlock(&gc_vpu_isp_pwr_lock);
		/*
		 * VPU power off takes 23us, add a pre-delay to reduce the
		 * number of polling
		 */
		udelay(20);

		/* polling VPU_PWR_STAT bit */
		while ((__raw_readl(APMU_PWR_STATUS_REG) & VPU_PWR_STAT)) {
			udelay(1);
			timeout -= 1;
			if (timeout < 0) {
				pr_err("%s: VPU power off timeout\n", __func__);
				return;
			}
		}
	}
}

#ifdef CONFIG_ANDROID_RAM_CONSOLE
static void pxa988_ram_console_init(void)
{
	static struct persistent_ram ram;
	static struct persistent_ram_descriptor desc;
	static char name[20] = "ram_console";

	/* reserver 1M memory from DDR address 0x8100000 */
	ram.start = 0x8100000;
	ram.size = 0x100000;
	ram.num_descs = 1;

	desc.size = 0x100000;
	desc.name = name;
	ram.descs = &desc;

	persistent_ram_early_init(&ram);
}
#endif

#ifdef CONFIG_ION
static struct ion_platform_data ion_data = {
	.nr	= 2,
	.heaps	= {
		[0] = {
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.id	= ION_HEAP_TYPE_CARVEOUT,
			.name	= "carveout_heap",
			.size   = 0x01000000,
			.base   = 0x09000000,
		},
		[1] = {
			.type	= ION_HEAP_TYPE_SYSTEM,
			.id	= ION_HEAP_TYPE_SYSTEM,
			.name	= "system_heap",
		},
	},
};

struct platform_device device_ion = {
	.name	= "pxa-ion",
	.id	= -1,
	.dev	= {
		.platform_data = &ion_data,
	},
};

static int __init ion_mem_carveout(char *p)
{
	struct ion_platform_data *ip = &ion_data;
	unsigned long size;
	phys_addr_t start;
	char *endp;
	int i;

	size  = memparse(p, &endp);
	if (*endp == '@')
		start = memparse(endp + 1, NULL);
	else
		BUG_ON(1);

	pr_info("ION carveout memory: 0x%08lx@0x%08lx\n",
		size, (unsigned long)start);
	/* set the carveout heap range */
	ion_data.heaps[0].size = size;
	ion_data.heaps[0].base = start;

	for (i = 0; i < ip->nr; i++)
		BUG_ON(memblock_reserve(ip->heaps[i].base, ip->heaps[i].size));

	return 0;
}
early_param("ioncarv", ion_mem_carveout);
#endif

/* CP memeory reservation, 32MB by default */
static u32 cp_area_size = 0x02000000;
static u32 cp_area_addr = 0x06000000;

static int __init early_cpmem(char *p)
{
	char *endp;

	cp_area_size = memparse(p, &endp);
	if (*endp == '@')
		cp_area_addr = memparse(endp + 1, NULL);

	return 0;
}
early_param("cpmem", early_cpmem);

static void __init pxa988_reserve_cpmem(void)
{
	/* Reserve memory for CP */
	BUG_ON(memblock_reserve(cp_area_addr, cp_area_size) != 0);
	memblock_free(cp_area_addr, cp_area_size);
	memblock_remove(cp_area_addr, cp_area_size);
	pr_info("Reserved CP memory: 0x%x@0x%x\n", cp_area_size, cp_area_addr);
}

static void __init pxa988_reserve_obmmem(void)
{
	/* Reserve 1MB memory for obm */
	BUG_ON(memblock_reserve(PLAT_PHYS_OFFSET, 0x100000) != 0);
	memblock_free(PLAT_PHYS_OFFSET, 0x100000);
	memblock_remove(PLAT_PHYS_OFFSET, 0x100000);
	pr_info("Reserved OBM memory: 0x%x@0x%lx\n",
		0x100000, PLAT_PHYS_OFFSET);
}

/*
* We will arrange the reserved memory for the following usage.
* 0 ~ PAGE_SIZE -1:  For low power
* PAGE_SIZE ~ 2 * PAGE_SIZE -1: For reset handler
*/
static void __init pxa988_reserve_pmmem(void)
{
	u32 pm_area_addr = 0x08000000;
	u32 pm_area_size = 0x00100000;
	pm_reserve_pa = pm_area_addr;
	/* Reserve 1MB memory for power management use */
	BUG_ON(memblock_reserve(pm_area_addr, pm_area_size) != 0);
	BUG_ON(memblock_free(pm_area_addr, pm_area_size));
	BUG_ON(0 != memblock_remove(pm_area_addr, pm_area_size));
}

void __init pxa988_reserve(void)
{
	/*
	 * reserve the first 1MB physical ddr memory for obm. when use EMMD
	 * (Enhanced Marvell Memory Dump), kernel should not make use of this
	 * memory, since it'll be corrupted by next reboot by obm.
	 */
	pxa988_reserve_obmmem();

	pxa988_reserve_cpmem();

#ifdef CONFIG_ANDROID_RAM_CONSOLE
	pxa988_ram_console_init();
#endif
#ifdef CONFIG_GPU_RESERVE_MEM
	pxa_reserve_gpu_memblock();
#endif
	pxa988_reserve_pmmem();
	pxa988_reserve_fb_mem();
}

void __init pxa988_init_irq(void)
{
	mmp_wakeupgen_init();
	gic_init(0, 29, IOMEM(GIC_DIST_VIRT_BASE), IOMEM(GIC_CPU_VIRT_BASE));
}

void pxa988_ripc_lock(void)
{
	int cnt = 0;

	while (__raw_readl(RIPC3_STATUS)) {
		cpu_relax();
		udelay(50);
		cnt++;
		if (cnt >= 10000)
			printk(KERN_WARNING "AP: ripc can not be locked!\n");
	}
}

int pxa988_ripc_trylock(void)
{
	return !__raw_readl(RIPC3_STATUS);
}

void pxa988_ripc_unlock(void)
{
	__raw_writel(1, RIPC3_STATUS);
}

#ifdef CONFIG_CACHE_L2X0

#ifdef CONFIG_PM
static inline void l2x0_save_phys_reg_addr(u32 *addr_ptr, u32 addr)
{
	BUG_ON(!addr_ptr);
	*addr_ptr = addr;
	flush_cache_all();
	outer_clean_range(virt_to_phys(addr_ptr),
		virt_to_phys(addr_ptr) + sizeof(*addr_ptr));
}
#endif

static void pxa988_l2_cache_init(void)
{
	void __iomem *l2x0_base;

	l2x0_base = ioremap(SL2C_PHYS_BASE, SZ_4K);
	BUG_ON(!l2x0_base);

	/* TAG, Data Latency Control */
	writel_relaxed(0x010, l2x0_base + L2X0_TAG_LATENCY_CTRL);
	writel_relaxed(0x010, l2x0_base + L2X0_DATA_LATENCY_CTRL);

	/* L2X0 Power Control  */
	writel_relaxed(0x3, l2x0_base + L2X0_POWER_CTRL);

	/* Enable I/D cache prefetch feature */
	l2x0_init(l2x0_base, 0x30800000, 0xFE7FFFFF);

#ifdef CONFIG_PM
	l2x0_saved_regs.phy_base = SL2C_PHYS_BASE;
	l2x0_save_phys_reg_addr(&l2x0_regs_phys,
				l2x0_saved_regs_phys_addr);
#endif
}
#else
#define pxa988_l2_cache_init()
#endif

#ifdef CONFIG_HAVE_ARM_TWD
static DEFINE_TWD_LOCAL_TIMER(twd_local_timer,
	(unsigned int)TWD_PHYS_BASE, IRQ_LOCALTIMER);

static void __init pxa988_twd_init(void)
{
	int err = twd_local_timer_register(&twd_local_timer);
	if (err)
		pr_err("twd_local_timer_register failed %d\n", err);
}
#else
#define pxa988_twd_init(void)	do {} while (0)
#endif

static void __init pxa988_timer_init(void)
{
	uint32_t clk_rst;

	/* Select the configurable timer clock source to be 3.25MHz */
	__raw_writel(APBC_APBCLK | APBC_RST, APBC_PXA988_TIMERS);
	clk_rst = APBC_APBCLK | APBC_FNCLK | APBC_FNCLKSEL(3);
	__raw_writel(clk_rst, APBC_PXA988_TIMERS);

	timer_init(IRQ_PXA988_AP_TIMER1);
	pxa988_twd_init();
}

struct sys_timer pxa988_timer = {
	.init   = pxa988_timer_init,
};

void pxa988_clear_keypad_wakeup(void)
{
	uint32_t val;
	uint32_t mask = APMU_PXA988_KP_WAKE_CLR;

	/* wake event clear is needed in order to clear keypad interrupt */
	val = __raw_readl(APMU_WAKE_CLR);
	__raw_writel(val | mask, APMU_WAKE_CLR);
}

void pxa988_clear_sdh_wakeup(void)
{
	uint32_t val;
	uint32_t mask = APMU_PXA988_SD1_WAKE_CLR | APMU_PXA988_SD2_WAKE_CLR
					| APMU_PXA988_SD3_WAKE_CLR;

	val = __raw_readl(APMU_WAKE_CLR);
	__raw_writel(val | mask, APMU_WAKE_CLR);

}

struct resource pxa988_resource_gpio[] = {
	{
		.start	= 0xd4019000,
		.end	= 0xd40197ff,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_PXA988_GPIO_AP,
		.end	= IRQ_PXA988_GPIO_AP,
		.name	= "gpio_mux",
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device pxa988_device_gpio = {
	.name		= "pxa-gpio",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(pxa988_resource_gpio),
	.resource	= pxa988_resource_gpio,
};

#ifdef CONFIG_USB_MV_UDC
static DEFINE_SPINLOCK(phy_lock);
static int phy_init_cnt;

static int usb_phy_init_internal(void __iomem *base)
{
	struct pxa988_usb_phy *phy = (struct pxa988_usb_phy *)base;
	int i;
	u32 phy_old, phy_power;

	pr_debug("init usb phy.\n");

	/*
	 * power up PHY by PIN.
	 * From the datasheet, it can be controlled by current regiter,
	 * but not pin.
	 * Will remove it after debug.
	 */
	phy_old = (u32)ioremap_nocache(0xD4207100, 0x10);
	phy_power = phy_old + 0x4;
	writel(0x10901003, phy_power);

	/* enable usb device PHY */
	writew(PLLVDD18(0x1) | REFDIV(0xd) | FBDIV(0xf0),
		&phy->utmi_pll_reg0);
	writew(PU_PLL | PLL_LOCK_BYPASS | ICP(0x1) | KVCO(0x3) | PLLCAL12(0x3),
		&phy->utmi_pll_reg1);
	writew(IMPCAL_VTH(0x1) | EXT_HS_RCAL(0x8) | EXT_FS_RCAL(0x8),
		&phy->utmi_tx_reg0);
	writew(TXVDD15(0x1) | TXVDD12(0x3) | LOWVDD_EN |
		AMP(0x4) | CK60_PHSEL(0x4),
		&phy->utmi_tx_reg1);
	writew(DRV_SLEWRATE(0x2) | IMP_CAL_DLY(0x2) |
		FSDRV_EN(0xf) | HSDEV_EN(0xf),
		&phy->utmi_tx_reg2);
	/* SQ_THRESH(0xa), SQ_THRESH(0x8)  for SSG */
	writew(PHASE_FREEZE_DLY | ACQ_LENGTH(0x2) | SQ_LENGTH(0x2) |
		DISCON_THRESH(0x2) | SQ_THRESH(0x8) | INTPI(0x1),
		&phy->utmi_rx_reg0);
	writew(EARLY_VOS_ON_EN | RXDATA_BLOCK_EN | EDGE_DET_EN |
		RXDATA_BLOCK_LENGTH(0x2) | EDGE_DET_SEL(0x1) |
		S2TO3_DLY_SEL(0x2),
		&phy->utmi_rx_reg1);
	writew(USQ_FILTER | SQ_BUFFER_EN | RXVDD18(0x1) | RXVDD12(0x1),
		&phy->utmi_rx_reg2);
	writew(BG_VSEL(0x1) | TOPVDD18(0x1),
		&phy->utmi_ana_reg0);
	writew(PU_ANA | SEL_LPFR | V2I(0x6) | R_ROTATE_SEL,
		&phy->utmi_ana_reg1);
	writew(FS_EOP_MODE | FORCE_END_EN | SYNCDET_WINDOW_EN |
		CLK_SUSPEND_EN | FIFO_FILL_NUM(0x6),
		&phy->utmi_dig_reg0);
	writew(FS_RX_ERROR_MODE2 | FS_RX_ERROR_MODE1 |
		FS_RX_ERROR_MODE | ARC_DPDM_MODE,
		&phy->utmi_dig_reg1);
	writew(0x0, &phy->utmi_charger_reg0);

	for (i = 0; i < 0x80; i = i + 4)
		pr_debug("[0x%x] = 0x%x\n", (u32)base + i,
			readw((u32)base + i));

	iounmap((void __iomem *)phy_old);
	return 0;
}

static int usb_phy_deinit_internal(void __iomem *base)
{
	u32 phy_old, phy_power;
	struct pxa988_usb_phy *phy = (struct pxa988_usb_phy *)base;
	u16 val;

	pr_debug("Deinit usb phy.\n");

	/* power down PHY PLL */
	val = readw(&phy->utmi_pll_reg1);
	val &= ~PU_PLL;
	writew(val, &phy->utmi_pll_reg1);

	/* power down PHY Analog part */
	val = readw(&phy->utmi_ana_reg1);
	val &= ~PU_ANA;
	writew(val, &phy->utmi_ana_reg1);

	/* power down PHY by PIN.
	 * From the datasheet, it can be controlled by current regiter,
	 * but not pin.
	 * Will remove it after debug.
	 */
	phy_old = (u32)ioremap_nocache(0xD4207100, 0x10);
	phy_power = phy_old + 0x4;
	writel(0x10901000, phy_power);

	iounmap((void __iomem *)phy_old);
	return 0;
}

int pxa_usb_phy_init(void __iomem *base)
{
	unsigned long flags;

	spin_lock_irqsave(&phy_lock, flags);
	if (phy_init_cnt++ == 0)
		usb_phy_init_internal(base);
	spin_unlock_irqrestore(&phy_lock, flags);
	return 0;
}

void pxa_usb_phy_deinit(void __iomem *base)
{
	unsigned long flags;

	WARN_ON(phy_init_cnt == 0);

	spin_lock_irqsave(&phy_lock, flags);
	if (--phy_init_cnt == 0)
		usb_phy_deinit_internal(base);
	spin_unlock_irqrestore(&phy_lock, flags);
}

static u64 usb_dma_mask = ~(u32)0;

struct resource pxa988_udc_resources[] = {
	/* regbase */
	[0] = {
		.start	= PXA988_UDC_REGBASE + PXA988_UDC_CAPREGS_RANGE,
		.end	= PXA988_UDC_REGBASE + PXA988_UDC_REG_RANGE,
		.flags	= IORESOURCE_MEM,
		.name	= "capregs",
	},
	/* phybase */
	[1] = {
		.start	= PXA988_UDC_PHYBASE,
		.end	= PXA988_UDC_PHYBASE + PXA988_UDC_PHY_RANGE,
		.flags	= IORESOURCE_MEM,
		.name	= "phyregs",
	},
	[2] = {
		.start	= IRQ_PXA988_USB1,
		.end	= IRQ_PXA988_USB1,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device pxa988_device_udc = {
	.name		= "mv-udc",
	.id		= -1,
	.resource	= pxa988_udc_resources,
	.num_resources	= ARRAY_SIZE(pxa988_udc_resources),
	.dev		=  {
		.dma_mask	= &usb_dma_mask,
		.coherent_dma_mask = 0xffffffff,
	}
};
#endif /* CONFIG_USB_MV_UDC */

#if defined(CONFIG_TOUCHSCREEN_VNC)
struct platform_device pxa988_device_vnc_touch = {
	.name   = "vnc-ts",
	.id     = -1,
};
#endif /* CONFIG_TOUCHSCREEN_VNC */

/*
 * This function is used to adjust the xtc for sram.
 * It is used to achieve better Vmin floor.
 */
static void pxa988_set_xtc(void)
{
	/* wtc/rtc for Z1/Z2 silicon */
	if (cpu_is_pxa988_z1() || \
		cpu_is_pxa988_z2() || \
		cpu_is_pxa986_z1() || \
		cpu_is_pxa986_z2()) {
		/* CORE L1 */
		writel_relaxed(0xAAAAAAAA, CIU_CA9_CPU_CONF_SRAM_0);
		/* CORE L2 */
		writel_relaxed(0x0000A666, CIU_CA9_CPU_CONF_SRAM_1);
		/* GC */
		writel_relaxed(0x00045555, CIU_GPU_XTC_REG);
		/* VPU */
		writel_relaxed(0x00B06655, CIU_VPU_XTC_REG);
	} else {
		/* On Z3, core L1/L2 wtc/rtc change on the fly */
		/* GC */
		writel_relaxed(0x00044444, CIU_GPU_XTC_REG);
		/* VPU keeps default setting */
	}
}

static int __init pxa988_init(void)
{
	pxa988_l2_cache_init();

	mfp_init_base(MFPR_VIRT_BASE);
	mfp_init_addr(pxa988_addr_map);
	pxa_init_dma(IRQ_PXA988_DMA_INT0, 32);
#ifdef CONFIG_ION
	platform_device_register(&device_ion);
#endif
#ifdef CONFIG_GPU_RESERVE_MEM
	pxa_add_gpu();
#endif
	platform_device_register(&pxa988_device_gpio);
#if defined(CONFIG_TOUCHSCREEN_VNC)
	platform_device_register(&pxa988_device_vnc_touch);
#endif /* CONFIG_TOUCHSCREEN_VNC */
	mmp_gpio_edge_init(GPIOE_VIRT_BASE, MFP_PIN_MAX, 128);

	pxa988_set_xtc();
	return 0;
}

postcore_initcall(pxa988_init);

/* on-chip devices */
PXA988_DEVICE(uart0, "pxa2xx-uart", 0, UART0, 0xd4036000, 0x30, 4, 5);
PXA988_DEVICE(uart1, "pxa2xx-uart", 1, UART1, 0xd4017000, 0x30, 21, 22);
PXA988_DEVICE(uart2, "pxa2xx-uart", 2, UART2, 0xd4018000, 0x30, 23, 24);
PXA988_DEVICE(keypad, "pxa27x-keypad", -1, KEYPAD, 0xd4012000, 0x4c);
PXA988_DEVICE(twsi0, "pxa910-i2c", 0, I2C0, 0xd4011000, 0x40);
PXA988_DEVICE(twsi1, "pxa910-i2c", 1, I2C1, 0xd4010800, 0x40);
PXA988_DEVICE(twsi2, "pxa910-i2c", 2, I2C2, 0xd4037000, 0x40);
PXA988_DEVICE(pwm1, "pxa910-pwm", 0, NONE, 0xd401a000, 0x10);
PXA988_DEVICE(pwm2, "pxa910-pwm", 1, NONE, 0xd401a400, 0x10);
PXA988_DEVICE(pwm3, "pxa910-pwm", 2, NONE, 0xd401a800, 0x10);
PXA988_DEVICE(pwm4, "pxa910-pwm", 3, NONE, 0xd401ac00, 0x10);
PXA988_DEVICE(sdh1, "sdhci-pxav3", 0, MMC, 0xd4280000, 0x120);
PXA988_DEVICE(sdh2, "sdhci-pxav3", 1, MMC, 0xd4280800, 0x120);
PXA988_DEVICE(sdh3, "sdhci-pxav3", 2, MMC, 0xd4281000, 0x120);
PXA988_DEVICE(ssp0, "pxa988-ssp", 0, SSP0, 0xd401b000, 0x90, 52, 53);
PXA988_DEVICE(ssp1, "pxa988-ssp", 1, SSP1, 0xd42a0c00, 0x90, 1, 2);
PXA988_DEVICE(ssp2, "pxa988-ssp", 2, SSP2, 0xd401C000, 0x90, 60, 61);
PXA988_DEVICE(gssp, "pxa988-ssp", 4, GSSP, 0xd4039000, 0x90, 6, 7);
PXA988_DEVICE(asram, "asram", 0, NONE, SRAM_AUDIO_BASE, SRAM_AUDIO_SIZE);
PXA988_DEVICE(isram, "isram", 1, NONE, SRAM_VIDEO_BASE, SRAM_VIDEO_SIZE);
PXA988_DEVICE(fb, "pxa168-fb", 0, LCD, 0xd420b000, 0x1fc);
PXA988_DEVICE(fb_ovly, "pxa168fb_ovly", 0, LCD, 0xd420b000, 0x1fc);
PXA988_DEVICE(fb_tv, "pxa168-fb", 1, LCD, 0xd420b000, 0x1fc);
PXA988_DEVICE(fb_tv_ovly, "pxa168fb_ovly", 1, LCD, 0xd420b000, 0x1fc);
PXA988_DEVICE(camera, "mmp-camera", 0, CI, 0xd420a000, 0xfff);
PXA988_DEVICE(thermal, "thermal", -1, DRO_SENSOR, 0xd4013200, 0x34);

static struct resource pxa988_resource_rtc[] = {
	{ 0xd4010000, 0xd40100ff, NULL, IORESOURCE_MEM, },
	{ IRQ_PXA988_RTC, IRQ_PXA988_RTC, "rtc 1Hz", IORESOURCE_IRQ, },
	{ IRQ_PXA988_RTC_ALARM, IRQ_PXA988_RTC_ALARM, "rtc alarm", IORESOURCE_IRQ, },
};

struct platform_device pxa988_device_rtc = {
	.name		= "sa1100-rtc",
	.id		= -1,
	.resource	= pxa988_resource_rtc,
	.num_resources	= ARRAY_SIZE(pxa988_resource_rtc),
};

static struct resource pxa988_resource_squ[] = {
	{
		.start	= 0xd42a0800,
		.end	= 0xd42a08ff,
		.flags	= IORESOURCE_MEM,
	}, {
		.start	= IRQ_PXA988_HIFI_DMA,
		.end	= IRQ_PXA988_HIFI_DMA,
		.flags	= IORESOURCE_IRQ,
	},
};

struct platform_device pxa988_device_squ = {
	.name		= "pxa910-squ",
	.id		= 1,
	.num_resources	= ARRAY_SIZE(pxa988_resource_squ),
	.resource	= pxa988_resource_squ,
	.dev		= {
		.coherent_dma_mask = DMA_BIT_MASK(64),
	},
};

static struct resource pxa988_resource_pcm_audio[] = {
	 {
		 /* playback dma */
		.name	= "pxa910-squ",
		.start	= 0,
		.flags	= IORESOURCE_DMA,
	},
	 {
		 /* record dma */
		.name	= "pxa910-squ",
		.start	= 1,
		.flags	= IORESOURCE_DMA,
	},
};

static struct mmp_audio_platdata mmp_audio_pdata = {
	.period_max_capture = 4 * 1024,
	.buffer_max_capture = 20 * 1024,
	.period_max_playback = 4 * 1024,
	.buffer_max_playback = 20 * 1024,
};

struct platform_device pxa988_device_asoc_platform = {
	.name		= "mmp-pcm-audio",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(pxa988_resource_pcm_audio),
	.resource	= pxa988_resource_pcm_audio,
	.dev = {
		.platform_data  = &mmp_audio_pdata,
	},
};

#ifdef CONFIG_VIDEO_MVISP
static u64 pxa988_dxo_dma_mask = DMA_BIT_MASK(32);

static struct resource pxa988_dxoisp_resources[] = {
	[0] = {
		.start = 0xD420F000,
		.end   = 0xD420FFFF,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = IRQ_PXA988_ISP_DMA,
		.end   = IRQ_PXA988_ISP_DMA,
		.flags = IORESOURCE_IRQ,
	},
	[2] = {
		.start = IRQ_PXA988_DXO,
		.end   = IRQ_PXA988_DXO,
		.flags = IORESOURCE_IRQ,
	},
	[3] = {
		.start = IRQ_PXA988_CI,
		.end   = IRQ_PXA988_CI,
		.flags = IORESOURCE_IRQ,
	},
};

struct platform_device pxa988_device_dxoisp = {
	.name           = "pxa988-mvisp",
	.id             = 0,
	.dev            = {
		.dma_mask = &pxa988_dxo_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.resource       = pxa988_dxoisp_resources,
	.num_resources  = ARRAY_SIZE(pxa988_dxoisp_resources),
};

void pxa988_register_dxoisp(struct mvisp_platform_data *data)
{
	int ret;

	pxa988_device_dxoisp.dev.platform_data = data;

	ret = platform_device_register(&pxa988_device_dxoisp);
	if (ret)
		dev_err(&(pxa988_device_dxoisp.dev),
				"unable to register dxo device: %d\n", ret);
}

#define ISP_HW_MODE         (0x1 << 15)
#define ISP_AUTO_PWR_ON     (0x1 << 4)
#define ISP_PWR_STAT        (0x1 << 4)
#define ISP_CLK_RST         ((1 << 0) | (1 << 8) | (1 << 10))
#define ISP_CLK_EN          ((1 << 1) | (1 << 9) | (1 << 11))

int pxa988_isp_power_control(int on)
{
	unsigned int val;
	int timeout = 5000;

	/*  HW mode power on/off*/
	if (on) {
		/* set isp HW mode*/
		val = __raw_readl(APMU_ISPDXO);
		val |= ISP_HW_MODE;
		__raw_writel(val, APMU_ISPDXO);

		spin_lock(&gc_vpu_isp_pwr_lock);
		/*  on1, on2, off timer */
		__raw_writel(0x20001fff, APMU_PWR_BLK_TMR_REG);

		/*  isp auto power on */
		val = __raw_readl(APMU_PWR_CTRL_REG);
		val |= ISP_AUTO_PWR_ON;
		__raw_writel(val, APMU_PWR_CTRL_REG);
		spin_unlock(&gc_vpu_isp_pwr_lock);

		/*  polling ISP_PWR_STAT bit */
		while (!(__raw_readl(APMU_PWR_STATUS_REG) & ISP_PWR_STAT)) {
			udelay(500);
			timeout -= 500;
			if (timeout < 0) {
				pr_err("%s: isp power on timeout\n", __func__);
				return -ENODEV;
			}
		}

	} else {
		spin_lock(&gc_vpu_isp_pwr_lock);
		/*  isp auto power off */
		val = __raw_readl(APMU_PWR_CTRL_REG);
		val &= ~ISP_AUTO_PWR_ON;
		__raw_writel(val, APMU_PWR_CTRL_REG);
		spin_unlock(&gc_vpu_isp_pwr_lock);

		/*  polling ISP_PWR_STAT bit */
		while ((__raw_readl(APMU_PWR_STATUS_REG) & ISP_PWR_STAT)) {
			udelay(500);
			timeout -= 500;
			if (timeout < 0) {
				pr_err("%s: ISP power off timeout\n", __func__);
				return -ENODEV;
			}
		}

	}

	return 0;
}

#define LCD_CI_ISP_ACLK_EN		(1 << 3)
#define LCD_CI_ISP_ACLK_RST		(1 << 16)
int pxa988_isp_reset_hw(void *param)
{
	unsigned int val;

	/*disable isp clock*/
	val = __raw_readl(APMU_ISPDXO);
	val &= ~ISP_CLK_EN;
	__raw_writel(val, APMU_ISPDXO);

#if defined(ENABLE_LCD_RST)
	val = __raw_readl(APMU_LCD);
	val &= ~LCD_CI_ISP_ACLK_EN;
	__raw_writel(val, APMU_LCD);

	/*reset clock*/
	val = __raw_readl(APMU_LCD);
	val &= ~LCD_CI_ISP_ACLK_RST;
	__raw_writel(val, APMU_LCD);
#endif

	/*reset isp clock*/
	val = __raw_readl(APMU_ISPDXO);
	val &= ~ISP_CLK_RST;
	__raw_writel(val, APMU_ISPDXO);

	/*de-reset isp clock*/
	val = __raw_readl(APMU_ISPDXO);
	val |= ISP_CLK_RST;
	__raw_writel(val, APMU_ISPDXO);

#if defined(ENABLE_LCD_RST)
	val = __raw_readl(APMU_LCD);
	val |= LCD_CI_ISP_ACLK_RST;
	__raw_writel(val, APMU_LCD);

	val = __raw_readl(APMU_LCD);
	val |= LCD_CI_ISP_ACLK_EN;
	__raw_writel(val, APMU_LCD);
#endif

	/*enable isp clock*/
	val = __raw_readl(APMU_ISPDXO);
	val |= ISP_CLK_EN;
	__raw_writel(val, APMU_ISPDXO);

	return 0;
}
#endif

#ifdef CONFIG_UIO_MVISP
static u64 pxa_uio_mvisp_dma_mask = DMA_BIT_MASK(32);

static struct resource pxa_uio_mvisp_resources[] = {
	[0] = {
		.start	= 0xD4240000,
		.end	= 0xD427FFFF,
		.flags	= IORESOURCE_MEM,
	},
};

struct platform_device pxa_device_mvisp = {
	.name	= "uio-mvisp",
	.id	= 0,
	.dev	= {
		.dma_mask = &pxa_uio_mvisp_dma_mask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
	},
	.resource	= pxa_uio_mvisp_resources,
	.num_resources = ARRAY_SIZE(pxa_uio_mvisp_resources),
};

void __init pxa_register_uio_mvisp(void)
{
	int ret;

	ret = platform_device_register(&pxa_device_mvisp);
	if (ret)
		dev_err(&(pxa_device_mvisp.dev),
			"unable to register uio mvisp device:%d\n", ret);
}
#endif
