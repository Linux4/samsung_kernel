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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/export.h>
#include <asm/io.h>
#include <asm/setup.h>
#include <asm/mach/time.h>
#include <asm/mach/arch.h>
#include <asm/mach-types.h>
#include <linux/irqchip/arm-gic.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/localtimer.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/clocksource.h>
#include <linux/clk-provider.h>
#include <soc/sprd/hardware.h>
#include <soc/sprd/adi.h>
#include <soc/sprd/adc.h>
#include <soc/sprd/board.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/hardware.h>

#include <linux/proc_avc.h>

extern void __init sci_reserve(void);
extern void __init sci_map_io(void);
extern void __init sci_init_irq(void);
extern void __init sci_timer_init(void);
extern int __init sci_clock_init(void);
extern int __init sci_regulator_init(void);
#ifdef CONFIG_ANDROID_RAM_CONSOLE
extern int __init sprd_ramconsole_init(void);
#endif

struct class *sec_class;
EXPORT_SYMBOL(sec_class);

static int calibration_mode = false;
static int __init calibration_start(char *str)
{
	int calibration_device =0;
	int mode=0,freq=0,device=0;
	if(str){
		pr_info("modem calibartion:%s\n", str);
		sscanf(str, "%d,%d,%d", &mode,&freq,&device);
	}
	if(device & 0x80){
		calibration_device = device & 0xf0;
		calibration_mode = true;
		pr_info("calibration device = 0x%x\n",calibration_device);
	}
	return 1;
}
__setup("calibration=", calibration_start);

//int in_calibration(void)
//{
//	return (int)(calibration_mode == true);
//}

//EXPORT_SYMBOL(in_calibration);

//int in_autotest(void)
//{
//	return 0;
//}

//EXPORT_SYMBOL(in_autotest);

int __init __clock_init_early(void)
{
	pr_info("ahb ctl0 %08x, ctl2 %08x glb aon apb0 %08x aon apb1 %08x clk_en %08x\n",
			sci_glb_raw_read(REG_AP_AHB_AHB_EB),
			sci_glb_raw_read(REG_AP_AHB_AHB_RST),
			sci_glb_raw_read(REG_AON_APB_APB_EB0),
			sci_glb_raw_read(REG_AON_APB_APB_EB1),
			sci_glb_raw_read(REG_AON_CLK_PUB_AHB_CFG));

	sci_glb_clr(REG_AP_AHB_AHB_EB,
			BIT_BUSMON2_EB		|
			BIT_BUSMON1_EB		|
			BIT_BUSMON0_EB		|
			//BIT_SPINLOCK_EB		|
			BIT_GPS_EB		|
			//BIT_EMMC_EB		|
			//BIT_SDIO2_EB		|
			//BIT_SDIO1_EB		|
			//BIT_SDIO0_EB		|
			BIT_DRM_EB		|
			BIT_NFC_EB		|
			//BIT_DMA_EB		|
			//BIT_USB_EB		|
			//BIT_GSP_EB		|
			//BIT_DISPC1_EB		|
			//BIT_DISPC0_EB		|
			//BIT_DSI_EB		|
			0);
	sci_glb_clr(REG_AP_APB_APB_EB,
			BIT_INTC3_EB		|
			BIT_INTC2_EB		|
			BIT_INTC1_EB		|
			BIT_IIS1_EB		|
			BIT_UART2_EB		|
			BIT_UART0_EB		|
			BIT_SPI1_EB		|
			BIT_SPI0_EB		|
			BIT_IIS0_EB		|
			BIT_I2C0_EB		|
			BIT_SPI2_EB		|
			BIT_UART3_EB		|
			0);
	sci_glb_clr(REG_AON_APB_APB_RTC_EB,
			BIT_KPD_RTC_EB		|
			BIT_KPD_EB		|
			BIT_EFUSE_EB		|
			0);

	sci_glb_clr(REG_AON_APB_APB_EB0,
			BIT_AUDIF_EB			|
			BIT_VBC_EB			|
			BIT_PWM3_EB			|
			BIT_PWM1_EB			|
			0);
	sci_glb_clr(REG_AON_APB_APB_EB1,
			BIT_AUX1_EB			|
			BIT_AUX0_EB			|
			0);

	printk("sc clock module early init ok\n");
	return 0;
}

static inline int	__sci_get_chip_id(void)
{
	return __raw_readl(CHIP_ID_LOW_REG);
}
const struct of_device_id of_sprd_default_bus_match_table[] = {
	{ .compatible = "simple-bus", },
	{ .compatible = "sprd,adi-bus", },
	{}
};

static struct of_dev_auxdata of_sprd_default_bus_lookup[] = {
	{ .compatible = "sprd,sprd_backlight", .name = "sprd_backlight" },
	{ .compatible = "sprd,sdhci-shark", .name = "sdio_sd",
						.phys_addr = 0x20300000 },
	{ .compatible = "sprd,sdhci-shark", .name = "sdio_wifi",
						.phys_addr = 0x20400000 },
	{ .compatible = "sprd,sdhci-shark", .name = "sprd-sdhci.2",
						.phys_addr = 0x20500000 },
	{ .compatible = "sprd,sdhci-shark", .name = "sdio_emmc",
						.phys_addr = 0x20600000 },
	{}
};

struct iotable_sprd io_addr_sprd;
EXPORT_SYMBOL(io_addr_sprd);

int iotable_build()
{
	struct device_node *np;
	struct resource res;

#define ADD_SPRD_DEVICE(compat, id)			\
do {							\
	np = of_find_compatible_node(NULL, NULL, compat);\
	if (of_can_translate_address(np)) {		\
		of_address_to_resource(np, 0, &res);	\
		io_addr_sprd.id.paddr = res.start;	\
		io_addr_sprd.id.length =		\
			resource_size(&res);		\
		io_addr_sprd.id.vaddr =			\
		ioremap_nocache(res.start, io_addr_sprd.id.length);\
		pr_debug("sprd io map: phys=%08x virt=%08x size=%08x\n", \
	io_addr_sprd.id.paddr, io_addr_sprd.id.vaddr, io_addr_sprd.id.length);\
	}						\
} while (0)
#define ADD_SPRD_DEVICE_BY_NAME(name, id)		\
do {							\
	np = of_find_node_by_name(NULL, name);		\
	if (of_can_translate_address(np)) {		\
		of_address_to_resource(np, 0, &res);	\
		io_addr_sprd.id.paddr = res.start;	\
		io_addr_sprd.id.length =		\
			resource_size(&res);		\
		io_addr_sprd.id.vaddr =			\
		ioremap_nocache(res.start, io_addr_sprd.id.length);\
		pr_debug("sprd io map: phys=%16lx virt=%16lx size=%16lx\n", \
	io_addr_sprd.id.paddr, io_addr_sprd.id.vaddr, io_addr_sprd.id.length);\
	}						\
} while (0)
	ADD_SPRD_DEVICE("sprd,ahb", AHB);
	ADD_SPRD_DEVICE("sprd,aonapb", AONAPB);
	ADD_SPRD_DEVICE("sprd,aonckg", AONCKG);
	ADD_SPRD_DEVICE("sprd,apbreg", APBREG);
	ADD_SPRD_DEVICE("sprd,coresight", CORESIGHT);
	ADD_SPRD_DEVICE("sprd,core", CORE);
	ADD_SPRD_DEVICE("sprd,mmahb", MMAHB);
	ADD_SPRD_DEVICE("sprd,pmu", PMU);
	ADD_SPRD_DEVICE("sprd,mmckg", MMCKG);
	ADD_SPRD_DEVICE("sprd,gpuapb", GPUAPB);
	ADD_SPRD_DEVICE("sprd,apbckg", APBCKG);
	ADD_SPRD_DEVICE("sprd,gpuckg", GPUCKG);
	ADD_SPRD_DEVICE("sprd,int", INT);
	ADD_SPRD_DEVICE("sprd,intc0", INTC0);
	ADD_SPRD_DEVICE("sprd,intc1", INTC1);
	ADD_SPRD_DEVICE("sprd,intc2", INTC2);
	ADD_SPRD_DEVICE("sprd,intc3", INTC3);
	ADD_SPRD_DEVICE("sprd,uidefuse", UIDEFUSE);
	ADD_SPRD_DEVICE("sprd,isp", ISP);
	ADD_SPRD_DEVICE("sprd,ca7wdg", CA7WDG);
	ADD_SPRD_DEVICE("sprd,csi2", CSI2);
	ADD_SPRD_DEVICE("sprd,d-eic-gpio", EIC);
	ADD_SPRD_DEVICE("sprd,wdg", WDG);
	ADD_SPRD_DEVICE("sprd,ipi", IPI);
	ADD_SPRD_DEVICE("sprd,dcam", DCAM);
	ADD_SPRD_DEVICE("sprd,syscnt", SYSCNT);
	ADD_SPRD_DEVICE("sprd,dma0", DMA0);
	ADD_SPRD_DEVICE("sprd,pub", PUB);
	ADD_SPRD_DEVICE("sprd,pin", PIN);
	ADD_SPRD_DEVICE("sprd,d-gpio-gpio", GPIO);
#if defined(CONFIG_ARCH_SCX30G2)
	ADD_SPRD_DEVICE("sprd,codecahb", CODECAHB);
#endif
#if defined(CONFIG_ARCH_SCX30G3)
	ADD_SPRD_DEVICE("sprd,crypto", CRYPTO);
#endif
	ADD_SPRD_DEVICE_BY_NAME("hwspinlock0", HWSPINLOCK0);
	ADD_SPRD_DEVICE_BY_NAME("hwspinlock1", HWSPINLOCK1);

	return 0;
}

static void __init sc8830_init_machine(void)
{
	printk("sci get chip id = 0x%x\n",__sci_get_chip_id());

	sec_avc_log_init();

	sci_adc_init();
	sci_regulator_init();
#if defined(CONFIG_PSTORE_RAM) && defined(SPRD_ION_BASE_USE_VARIABLE)
	{
		extern void init_pstore_addr_param(void);
		init_pstore_addr_param();
	}
#endif
	of_platform_populate(NULL, of_sprd_default_bus_match_table, of_sprd_default_bus_lookup, NULL);
	sec_class = class_create(THIS_MODULE, "sec");
        if (IS_ERR(sec_class)) {
		pr_err("Failed to create class(sec)!\n");
		printk("Failed create class \n");
		return PTR_ERR(sec_class);
	}
}

const struct of_device_id of_sprd_late_bus_match_table[] = {
	{ .compatible = "sprd,sound", },
	{}
};

static void __init sc8830_init_late(void)
{
	of_platform_populate(of_find_node_by_path("/sprd-audio-devices"),
			of_sprd_late_bus_match_table, NULL, NULL);
}

extern void __init sc_init_chip_id(void);
void __init sprd_init_before_irq(void)
{
	iotable_build();
	sc_init_chip_id();
	/* earlier init request than irq and timer */
	__clock_init_early();
	/*ipi reg init for sipc*/
	sci_glb_set(REG_AON_APB_APB_EB0, BIT_IPI_EB);
}
static void __init sc8830_pmu_init(void)
{
	__raw_writel(__raw_readl(REG_PMU_APB_PD_MM_TOP_CFG)
			& ~(BIT_PD_MM_TOP_FORCE_SHUTDOWN),
			REG_PMU_APB_PD_MM_TOP_CFG);

	__raw_writel(__raw_readl(REG_PMU_APB_PD_GPU_TOP_CFG)
			& ~(BIT_PD_GPU_TOP_FORCE_SHUTDOWN),
			REG_PMU_APB_PD_GPU_TOP_CFG);

	__raw_writel(__raw_readl(REG_AON_APB_APB_EB0) | BIT_MM_EB | BIT_GPU_EB, 
		REG_AON_APB_APB_EB0);

	/* only need open this power init in Tshark2 */
#if defined(CONFIG_ARCH_SCX30G2)
	__raw_writel(__raw_readl(REG_AON_APB_APB_EB1) | BIT_CODEC_EB, 
		 REG_AON_APB_APB_EB1);
#endif

	__raw_writel(__raw_readl(REG_MM_AHB_AHB_EB) | BIT_MM_CKG_EB,
			REG_MM_AHB_AHB_EB);

	__raw_writel(__raw_readl(REG_MM_AHB_GEN_CKG_CFG)
			| BIT_MM_MTX_AXI_CKG_EN | BIT_MM_AXI_CKG_EN,
			REG_MM_AHB_GEN_CKG_CFG);

	__raw_writel(__raw_readl(REG_MM_CLK_MM_AHB_CFG) | 0x3,
			REG_MM_CLK_MM_AHB_CFG);
}

extern void __init  sci_enable_timer_early(void);
static void __init sprd_init_time(void)
{
	if(of_have_populated_dt()){
		sc8830_pmu_init();
		of_clk_init(NULL);
		clocksource_of_init();
	}else{
		sci_clock_init();
		sci_enable_timer_early();
		sci_timer_init();
	}
}
static const char *sprd_boards_compat[] __initdata = {
	"sprd,scx35",
	NULL,
};
extern struct smp_operations sprd_smp_ops;

DT_MACHINE_START(SCPHONE, "sc8830")
	.smp		= smp_ops(sprd_smp_ops),
	.reserve	= sci_reserve,
	.map_io		= sci_map_io,
	.init_irq	= sci_init_irq,
	.init_time	= sprd_init_time,
	.init_machine	= sc8830_init_machine,
	.init_late	= sc8830_init_late,
	.dt_compat = sprd_boards_compat,
MACHINE_END

