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

#if 0
#include <asm/irqflags.h>
#include <linux/init.h>
#include <linux/suspend.h>
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <linux/kthread.h>
#include <linux/debugfs.h>
#include <mach/pm_debug.h>
#include <mach/globalregs.h>
#include <mach/regs_glb.h>
#include <mach/regs_ahb.h>
#include <mach/regs_ana_glb.h>
#include <mach/sci.h>
#include <mach/adi.h>

struct dentry * dentry_debug_root = NULL;
static int is_print_sleep_mode = 0;
int is_print_linux_clock = 1;
int is_print_modem_clock = 1;
static int is_print_irq = 1;
static int is_print_wakeup = 1;
static int is_print_irq_runtime = 0;
static int is_print_time = 1;
static unsigned int core_time = 0;
static unsigned int mcu_time = 0;
static unsigned int deep_time_successed = 0;
static unsigned int deep_time_failed = 0;
static unsigned int sleep_time = 0;
#define		SPRD_INTC_NUM			2
#define 	SPRD_HARD_INTERRUPT_NUM 64
#define 	SPRD_HARD_INT_NUM_EACH_INTC 32
#define		SPRD_IRQ_NUM			1024
static u32 sprd_hard_irq[SPRD_HARD_INTERRUPT_NUM]= {0, };
static u32 sprd_irqs[SPRD_IRQ_NUM] = {0, };
static u32 sprd_irqs_sts[SPRD_INTC_NUM] = {0, };
static int is_wakeup = 0;
static int irq_status = 0;
static int hard_irq_status[SPRD_INTC_NUM] = {0, };
static int sleep_mode = SLP_MODE_NON;
static char * sleep_mode_str[]  = {
	"[ARM]",
	"[MCU]",
	"[DEP]",
	"[NON]"
};
#define INT_REG(off) (SPRD_INTC0_BASE + (off))
#define	INTC1_REG(off)		(SPRD_INTC0_BASE + 0x1000 + (off))
#define INT_IRQ_STS            INT_REG(0x0000)
#define INT_IRQ_RAW           INT_REG(0x0004)
#define INT_IRQ_ENB           INT_REG(0x0008)
#define INT_IRQ_DIS            INT_REG(0x000c)
#define INT_FIQ_STS            INT_REG(0x0020)
#define	INTCV1_IRQ_MSKSTS	INTC1_REG(0x0000)
#define	INTCV1_IRQ_RAW		INTC1_REG(0x0004)
#define	INTCV1_IRQ_EN		INTC1_REG(0x0008)
#define	INTCV1_IRQ_DIS		INTC1_REG(0x000C)
#define	INTCV1_FIQ_STS		INTC1_REG(0x0020)
#define INT_IRQ_MASK	(1<<3)

void pm_debug_dump_ahb_glb_regs(void);

static void hard_irq_reset(void)
{
	int i = SPRD_HARD_INTERRUPT_NUM - 1;
	do{
		sprd_hard_irq[i] = 0;
	}while(--i >= 0);
}

static void parse_hard_irq(unsigned long val, unsigned long intc)
{
	int i;
	if(intc == 0){
		for (i = 0; i < SPRD_HARD_INT_NUM_EACH_INTC; i++) {
			if (test_and_clear_bit(i, &val)) sprd_hard_irq[i]++;
		}
	}
	if(intc == 1){
		for (i = 32; i < SPRD_HARD_INT_NUM_EACH_INTC; i++) {
			if (test_and_clear_bit(i, &val)) sprd_hard_irq[i]++;
		}
	}
}

void hard_irq_set(void)
{
	sprd_irqs_sts[0] = __raw_readl(INT_IRQ_STS);
	sprd_irqs_sts[1] = __raw_readl(INT_FIQ_STS);
	irq_status = __raw_readl(INT_IRQ_STS);
	parse_hard_irq(irq_status, 0);
	irq_status = __raw_readl(INTCV1_IRQ_MSKSTS);
	parse_hard_irq(irq_status, 1);
}

#define GPIO_GROUP_NUM		16
#define IRQ_GPIO		(1<<10)
#define IRQ_ADIE		(1<<25)
#define IRQ_DSP0		(1<<26)
#define IRQ_DSP1		(1<<27)
#define IRQ_TIMER0		(1<<6)
#define IRQ_SIM0		(1<<15)
#define IRQ_SIM1		(1<<16)

#define REG_GPIO_MIS            (0x0020)
#define ANA_REG_INT_MASK_STATUS (SPRD_MISC_BASE + 0x380 +0x0000)
void print_hard_irq_inloop(int ret)
{
	unsigned int i, j, val;
	unsigned int gpio_irq[GPIO_GROUP_NUM];

	if(!((sprd_irqs_sts[0]&IRQ_DSP0) ||
		(sprd_irqs_sts[0]&IRQ_DSP1) ||
		(sprd_irqs_sts[0]&IRQ_TIMER0) ||
		(sprd_irqs_sts[0]&IRQ_SIM0) ||
		(sprd_irqs_sts[0]&IRQ_SIM1) ) ){

		if(sprd_irqs_sts[0] != 0)
			printk("%c#:INTC0: %08x\n", ret?'S':'F', sprd_irqs_sts[0]);
		if(sprd_irqs_sts[1] != 0)
			printk("%c#:INTC0 FIQ: %08x\n", ret?'S':'F', sprd_irqs_sts[1]);
	}

	if(sprd_irqs_sts[0] & IRQ_GPIO){
		for(i=0; i<(GPIO_GROUP_NUM/2); i++){
			j = 2*i;
			gpio_irq[j]= __raw_readl(SPRD_GPIO_BASE + 0x100*i + REG_GPIO_MIS);
			gpio_irq[j+1]= __raw_readl(SPRD_GPIO_BASE + 0x100*i + 0x80 + REG_GPIO_MIS);
			printk("gpio_irq[%d]:0x%x, gpio_irq[%d]:0x%x \n", j, gpio_irq[j], j+1, gpio_irq[j+1]);
		}
		for(i=0; i<GPIO_GROUP_NUM; i++){
			if(gpio_irq[i] != 0){
				val = gpio_irq[i];
				while(val){
					j = __ffs(val);
					printk("gpio irq number : %d \n", (j+16*(i+1)) );
					val &= ~(1<<j);
				}
			}
		}
	}
	if(sprd_irqs_sts[0] & IRQ_ADIE){
		printk("adie, irq status:0x%x \n", sci_adi_read(ANA_REG_INT_MASK_STATUS));
	}

}

static void print_hard_irq(void)
{
	int i = SPRD_HARD_INTERRUPT_NUM -1;
	if(!is_print_irq)
		return;
	do{
		if(0 != sprd_hard_irq[i])
			printk("##: sprd_hard_irq[%d] = %d.\n",
                                i, sprd_hard_irq[i]);
	}while(--i >= 0);
}

static void irq_reset(void)
{
	int i = SPRD_IRQ_NUM - 1;
	do{
		sprd_irqs[i] = 0;
	}while(--i >= 0);
}


void inc_irq(int irq)
{
	if(is_wakeup){
		if (irq >= SPRD_IRQ_NUM) {
			printk("## bad irq number %d.\n", irq);
			return;
		}
		sprd_irqs[irq]++;
		if(is_print_wakeup)
			printk("\n#####: wakeup irq = %d.\n", irq);
		is_wakeup = 0;
	}
}
EXPORT_SYMBOL(inc_irq);

void irq_wakeup_set(void)
{
	is_wakeup = 1;
}

static void print_irq(void)
{
	int i = SPRD_IRQ_NUM - 1;
	if(!is_print_irq)
		return;
	do{
		if(0 != sprd_irqs[i])
			printk("##: sprd_irqs[%d] = %d.\n",
                                i, sprd_irqs[i]);
	}while(--i >= 0);
}

void time_add(unsigned int time, int ret)
{
	switch(sleep_mode){
		case SLP_MODE_ARM:
			core_time += time;
			break;
		case SLP_MODE_MCU:
			mcu_time += time;
			break;
		case SLP_MODE_DEP:
			if(ret)
				deep_time_successed += time;
			else 
				deep_time_failed += time;
			break;
		default:
			break;
	}
}

void time_statisic_begin(void)
{
	core_time = 0;
	mcu_time = 0;
	deep_time_successed = 0;
	deep_time_failed = 0;
	sleep_time = get_sys_cnt();
	hard_irq_reset();
}

void time_statisic_end(void)
{
	sleep_time = get_sys_cnt() - sleep_time;
}

void print_time(void)
{
	if(!is_print_time)
		return;
	printk("time statisics : sleep_time=%d, core_time=%d, mcu_time=%d, deep_sus=%d, dep_fail=%d\n",
		sleep_time, core_time, mcu_time, deep_time_successed, deep_time_failed);
}

void set_sleep_mode(int sm){
	int is_print = (sm == sleep_mode);
	sleep_mode = sm;
	if(is_print_sleep_mode == 0 || is_print )
		return;
	switch(sm){
		case SLP_MODE_ARM:
			printk("\n[ARM]\n");
			break;
		case SLP_MODE_MCU:
			printk("\n[MCU]\n");
			break;
		case SLP_MODE_DEP:
			printk("\n[DEP]\n");
			break;
		default:
			printk("\nNONE\n");
	}
}
void clr_sleep_mode(void)
{
	sleep_mode = SLP_MODE_NON;
}
void print_statisic(void)
{
	print_time();
	print_hard_irq();
	print_irq();
	pm_debug_dump_ahb_glb_regs();
	if(is_print_wakeup){
		printk("###wake up form %s : %08x\n",  sleep_mode_str[sleep_mode],  sprd_irqs_sts[0]);
		printk("###wake up form %s : %08x\n",  sleep_mode_str[sleep_mode],  sprd_irqs_sts[1]);
	}
}
#ifdef PM_PRINT_ENABLE
static struct wake_lock messages_wakelock;
#endif
#ifdef PM_PRINT_ENABLE
/* save pm message for debug when enter deep sleep*/
unsigned int debug_status[10];
void pm_debug_save_ahb_glb_regs(void)
{
	debug_status[0] = sci_glb_read(REG_AHB_AHB_STATUS, -1UL);
	debug_status[1] = sci_glb_read(REG_AHB_AHB_CTL0, -1UL);
	debug_status[2] = sci_glb_read(REG_AHB_AHB_CTL1, -1UL);
	debug_status[3] = sci_glb_read(REG_AHB_AHB_STATUS, -1UL);
	debug_status[4] = sci_glb_read(REG_GLB_GEN0, -1UL);
	debug_status[5] = sci_glb_read(REG_GLB_GEN1, -1UL);
	debug_status[6] = sci_glb_read(REG_GLB_STC_DSP_ST, -1UL);
	debug_status[7] = sci_glb_read(REG_GLB_BUSCLK, -1UL);
	debug_status[8] = sci_glb_read(REG_GLB_CLKDLY, -1UL);
}
void pm_debug_dump_ahb_glb_regs(void)
{
	printk("***** ahb and globle registers before last deep sleep **********\n");

	printk("*** AHB_CTL0:  0x%x ***\n", debug_status[1] );
	printk("*** AHB_CTL1:  0x%x ***\n", debug_status[2] );
	printk("*** GR_GEN0:  0x%x ***\n", debug_status[4] );
	printk("*** GR_GEN1:  0x%x ***\n", debug_status[5] );
	printk("*** GR_BUSCLK:  0x%x ***\n", debug_status[7] );

	printk("*** AHB_STS:  0x%x ***\n", debug_status[3] );
	printk("*** GR_STC_STATE:  0x%x ***\n", debug_status[6] );
	printk("*** GR_CLK_DLY:  0x%x ***\n", debug_status[8] );
}


static void print_ahb(void)
{
	u32 val = sci_glb_read(REG_AHB_AHB_CTL0, -1UL);
	printk("##: REG_AHB_AHB_CTL0 = %08x.\n", val);
	if (val & AHB_CTL0_DCAM_EN) printk("AHB_CTL0_DCAM_EN =1.\n");	
	if (val & AHB_CTL0_CCIR_IN_EN) printk("AHB_CTL0_CCIR_IN_EN =1. CCIR enable\n");
	if (val & AHB_CTL0_LCDC_EN) printk("AHB_CTL0_LCDC_EN =1.\n");
	if (val & AHB_CTL0_SDIO0_EN) printk("AHB_CTL0_SDIO0_EN =1.\n");
	if (val & AHB_CTL0_USBD_EN) printk("AHB_CTL0_USBD_EN =1.\n");
	if (val & AHB_CTL0_DMA_EN) printk("AHB_CTL0_DMA_EN =1.\n");
	if (val & AHB_CTL0_BM0_EN) printk("AHB_CTL0_BM0_EN =1.\n");
	if (val & AHB_CTL0_NFC_EN) printk("AHB_CTL0_NFC_EN =1.\n");	
	if (val & AHB_CTL0_CCIR_EN) printk("AHB_CTL0_CCIR_EN =1. CCIR clock enable\n");
	if (val & AHB_CTL0_DCAM_MIPI_EN) printk("AHB_CTL0_DCAM_MIPI_EN =1.\n");	
	if (val & AHB_CTL0_BM1_EN) printk("AHB_CTL0_BM1_EN =1.\n");	
	if (val & AHB_CTL0_ISP_EN) printk("AHB_CTL0_ISP_EN =1.\n");
	if (val & AHB_CTL0_VSP_EN) printk("AHB_CTL0_VSP_EN =1.\n");
	if (val & AHB_CTL0_ROT_EN) printk("AHB_CTL0_ROT_EN =1.\n");
	if (val & AHB_CTL0_BM2_EN) printk("AHB_CTL0_BM2_EN =1.\n");
	if (val & AHB_CTL0_BM3_EN) printk("AHB_CTL0_BM3_EN =1.\n");
	if (val & AHB_CTL0_BM4_EN) printk("AHB_CTL0_BM4_EN =1.\n");	
	if (val & AHB_CTL0_SDIO1_EN) printk("AHB_CTL0_SDIO1_EN =1.\n");
	if (val & AHB_CTL0_G2D_EN) printk("AHB_CTL0_G2D_EN =1.\n");
	if (val & AHB_CTL0_G3D_EN) printk("AHB_CTL0_G3D_EN =1.\n");
	if (val & AHB_CTL0_DISPC_EN) printk("AHB_CTL0_DISPC_EN =1.\n");
	if (val & AHB_CTL0_EMMC_EN) printk("AHB_CTL0_EMMC_EN =1.\n");
	if (val & AHB_CTL0_SDIO2_EN) printk("AHB_CTL0_SDIO2_EN =1.\n");
	if (val & AHB_CTL0_SPINLOCK_EN) printk("AHB_CTL0_SPINLOCK_EN =1.\n");
	if (val & AHB_CTL0_AHB_ARCH_EB) printk("AHB_CTL0_AHB_ARCH_EB =1. AHB bus HCLK enable\n");
	if (val & AHB_CTL0_EMC_EN) printk("AHB_CTL0_EMC_EN =1.\n");
	if (val & AHB_CTL0_AXIBUSMON0_EN) printk("AHB_CTL0_AXIBUSMON0_EN =1.\n");
	if (val & AHB_CTL0_AXIBUSMON1_EN) printk("AHB_CTL0_AXIBUSMON1_EN =1.\n");	
	if (val & AHB_CTL0_AXIBUSMON2_EN) printk("AHB_CTL0_AXIBUSMON2_EN =1.\n");

	val = sci_glb_read(REG_AHB_AHB_CTL1, -1UL);
	printk("##: REG_AHB_AHB_CTL1 = %08x.\n", val);

	val = sci_glb_read(REG_AHB_AHB_CTL2, -1UL);
	printk("##: REG_AHB_AHB_CTL2 = %08x.\n", val);
	if (val & AHB_CTL2_DISPMTX_CLK_EN ) printk("AHB_CTL2_DISPMTX_CLK_EN =1.\n");
	if (val & AHB_CTL2_MMMTX_CLK_EN ) printk("AHB_CTL2_MMMTX_CLK_EN =1.\n");
	if (val & AHB_CTL2_DISPC_CORE_CLK_EN) printk("AHB_CTL2_DISPC_CORE_CLK_EN=1.\n");
	if (val & AHB_CTL2_LCDC_CORE_CLK_EN) printk("AHB_CTL2_LCDC_CORE_CLK_EN=1.\n");
	if (val & AHB_CTL2_ISP_CORE_CLK_EN) printk("AHB_CTL2_ISP_CORE_CLK_EN=1.\n");
	if (val & AHB_CTL2_VSP_CORE_CLK_EN) printk("AHB_CTL2_VSP_CORE_CLK_EN=1.\n");
	if (val & AHB_CTL2_DCAM_CORE_CLK_EN) printk("AHB_CTL2_DCAM_CORE_CLK_EN=1.\n");

	val = sci_glb_read(REG_AHB_AHB_CTL3, -1UL);
	printk("##: REG_AHB_AHB_CTL3 = %08x.\n", val);

	val = sci_glb_read(REG_AHB_MIPI_PHY_CTRL, -1UL);
	printk("##: REG_AHB_MIPI_PHY_CTRL= %08x.\n", val);

	val = sci_glb_read(REG_AHB_AHB_STATUS, -1UL);
	printk("##:REG_AHB_AHB_STATUS = %08x.\n", val);
}
static void print_gr(void)
{
	u32 val = sci_glb_read(REG_GLB_GEN0, -1UL);
	printk("##: GR_GEN0 = %08x.\n", val);	
	if (val & GEN0_UART3_EN) printk("GEN0_UART3_EN =1.\n");
	if (val & GEN0_SPI2_EN) printk("GEN0_SPI2_EN =1.\n");
	if (val & GEN0_TIMER_EN) printk("GEN0_TIMER_EN =1.\n");
	if (val & GEN0_SIM0_EN) printk("GEN0_SIM0_EN =1.\n");
	if (val & GEN0_I2C_EN) printk("GEN0_I2C_EN =1.\n");
	if (val & GEN0_GPIO_EN) printk("GEN0_GPIO_EN =1.\n");
	if (val & GEN0_ADI_EN) printk("GEN0_ADI_EN =1.\n");
	if (val & GEN0_EFUSE_EN) printk("GEN0_EFUSE_EN =1.\n");
	if (val & GEN0_KPD_EN) printk("GEN0_KPD_EN =1.\n");
	if (val & GEN0_EIC_EN) printk("GEN0_EIC_EN =1.\n");
	if (val & GEN0_I2S0_EN) printk("GEN0_I2S0_EN =1.\n");
	if (val & GEN0_PIN_EN) printk("GEN0_PIN_EN =1.\n");
	if (val & GEN0_CCIR_MCLK_EN) printk("GEN0_CCIR_MCLK_EN =1.\n");
	if (val & GEN0_EPT_EN) printk("GEN0_EPT_EN =1.\n");
	if (val & GEN0_SIM1_EN) printk("GEN0_SIM1_EN =1.\n");
	if (val & GEN0_SPI0_EN) printk("GEN0_SPI0_EN =1.\n");
	if (val & GEN0_SPI1_EN) printk("GEN0_SPI1_EN =1.\n");
	if (val & GEN0_SYST_EN) printk("GEN0_SYST_EN =1.\n");
	if (val & GEN0_UART0_EN) printk("GEN0_UART0_EN =1.\n");
	if (val & GEN0_UART1_EN) printk("GEN0_UART1_EN =1.\n");
	if (val & GEN0_UART2_EN) printk("GEN0_UART2_EN =1.\n");
	if (val & GEN0_VB_EN) printk("GEN0_VB_EN =1.\n");
	if (val & GEN0_EIC_RTC_EN) printk("GEN0_EIC_RTC_EN =1.\n");
	if (val & GEN0_I2S1_EN) printk("GEN0_I2S1_EN =1.\n");
	if (val & GEN0_KPD_RTC_EN) printk("GEN0_KPD_RTC_EN =1.\n");
	if (val & GEN0_SYST_RTC_EN) printk("GEN0_SYST_RTC_EN =1.\n");
	if (val & GEN0_TMR_RTC_EN) printk("GEN0_TMR_RTC_EN =1.\n");	
	if (val & GEN0_I2C1_EN) printk("GEN0_I2C1_EN =1.\n");
	if (val & GEN0_I2C2_EN) printk("GEN0_I2C2_EN =1.\n");
	if (val & GEN0_I2C3_EN) printk("GEN0_I2C3_EN =1.\n");

	val = sci_glb_read(REG_GLB_GEN1, -1UL);
	printk("##: REG_GLB_GEN1 = %08x.\n", val);
	if (val & BIT_CLK_AUX0_EN ) printk(" CLK_AUX0_EN =1.\n");
	if (val & BIT_CLK_AUX1_EN ) printk(" CLK_AUX1_EN =1.\n");
	if (val & BIT_AUD_IF_EB ) printk(" AUD_IF_EB =1.\n");
	if (val & BIT_AUD_TOP_EB ) printk(" AUD_TOP_EB =1.\n");
	if (val & BIT_VBC_EN ) printk(" VBC_EN =1.\n");
	if (val & BIT_AUDIF_AUTO_EN ) printk(" AUDIF_AUTO_EN =1.\n");

	val = sci_glb_read(REG_GLB_CLK_EN, -1UL);
	printk("##: GR_CLK_EN = %08x.\n", val);
	if (val & CLK_PWM0_EN) printk("CLK_PWM0_EN =1.\n");
	if (val & CLK_PWM1_EN) printk("CLK_PWM1_EN = 1.\n");
	if (val & CLK_PWM2_EN) printk("CLK_PWM2_EN = 1.\n");
	if (val & CLK_PWM3_EN) printk("CLK_PWM3_EN = 1.\n");

	val = sci_glb_read(REG_GLB_BUSCLK, -1UL);
	printk("##: GR_BUSCLK_ALM = %08x.\n", val);
	if (val & ARM_VB_MCLKON) printk("ARM_VB_MCLKON =1.\n");
	if (val & ARM_VB_DA0ON) printk("ARM_VB_DA0ON = 1.\n");
	if (val & ARM_VB_DA1ON) printk("ARM_VB_DA1ON = 1.\n");
	if (val & ARM_VB_ADCON) printk("ARM_VB_ADCON = 1.\n");
	if (val & ARM_VB_ANAON) printk("ARM_VB_ANAON = 1.\n");
	if (val & ARM_VB_ACC) printk("ARM_VB_ACC = 1.\n");

	val = sci_glb_read(REG_GLB_CLK_GEN5, -1UL);
	printk("##: GR_CLK_GEN5 = %08x.\n", val);
	if (val & BIT(9)) printk("LDO_USB_PD =1.\n");

	val = sci_glb_raw_read(REG_GLB_PCTRL);
	printk("##: GLB_PCTL = %08x.\n", val);
	if (val & BIT_MCU_MPLL_EN) printk("MCU_MPLL = 1.\n");
	if (val & BIT_MCU_GPLL_EN) printk("MCU_GPLL = 1.\n");
	if (val & BIT_MCU_DPLL_EN) printk("MCU_DPLL = 1.\n");
	if (val & BIT_MCU_TDPLL_EN) printk("MCU_TDPLL = 1.\n");

	val = sci_glb_raw_read(REG_GLB_TD_PLL_CTL);
	printk("##: GLB_TD_PLL_CTL = %08x.\n", val);
	if (!(val & BIT_TDPLL_DIV2OUT_FORCE_PD)) printk("clk_384m = 1.\n");
	if (!(val & BIT_TDPLL_DIV3OUT_FORCE_PD)) printk("clk_256m = 1.\n");
	if (!(val & BIT_TDPLL_DIV4OUT_FORCE_PD)) printk("clk_192m = 1.\n");
	if (!(val & BIT_TDPLL_DIV5OUT_FORCE_PD)) printk("clk_153p6m = 1.\n");

	val = sci_glb_read(REG_GLB_POWCTL1, -1UL);
	printk("##: GR_POWCTL1 = %08x.\n", val);

	val = sci_glb_read(REG_GLB_M_PLL_CTL0, -1UL);
	printk("##: REG_GLB_M_PLL_CTL0 = %08x.\n", val);

	val = sci_glb_read(REG_GLB_MM_PWR_CTL, -1UL);
	printk("##: REG_GLB_MM_PWR_CTL = %08x.\n", val);
	val = sci_glb_read(REG_GLB_CEVA_L1RAM_PWR_CTL, -1UL);
	printk("##: REG_GLB_CEVA_L1RAM_PWR_CTL = %08x.\n", val);
	val = sci_glb_read(REG_GLB_GSM_PWR_CTL, -1UL);
	printk("##: REG_GLB_GSM_PWR_CTL = %08x.\n", val);
	val = sci_glb_read(REG_GLB_TD_PWR_CTL, -1UL);
	printk("##: GR_TD_PWR_CTRL = %08x.\n", val);
	val = sci_glb_read(REG_GLB_PERI_PWR_CTL, -1UL);
	printk("##: GR_PERI_PWR_CTRL = %08x.\n", val);
	val = sci_glb_read(REG_GLB_ARM_SYS_PWR_CTL, -1UL);
	printk("##: GR_ARM_SYS_PWR_CTRL = %08x.\n", val);
	val = sci_glb_read(REG_GLB_G3D_PWR_CTL, -1UL);
	printk("##: GR_G3D_PWR_CTRL = %08x.\n", val);
}
#define LDO_USB_CTL		0x2
#define LDO_SIM0_CTL	0x20
#define LDO_SIM1_CTL	0x80
#define LDO_BPCAMCORE_CTL	0x200
#define LDO_BPCAMIO_CTL		0x800
#define LDO_BPCAMA_CTL		0x2000
#define LDO_BPCAMMOT_CTL	0x8000
#define LDO_SDIO0_CTL	0x2
#define LDO_SDIO1_CTL	0x8
#define LDO_SDIO3_CTL	0x20
#define LDO_VDD3V_CTL	0x80
#define LDO_CMMB1P8_CTL	0x200
#define LDO_CMMB1V2_CTL	0x800
#define AUDIO_PA_ENABLE		0x1
#define AUDIO_PA_ENABLE_RST		0x2
#define AUDIO_PA_LDO_ENABLE		0x100
#define AUDIO_PA_LDO_ENABLE_RST		0x200
#define VIBR_PD_RST 0x8
#define VIBR_PD		0x4
#define	LDO_REG_BASE		(SPRD_MISC_BASE + 0x600)
#define	ANA_LDO_PD_CTL0		(LDO_REG_BASE  + 0x10)
#define	ANA_LDO_PD_CTL1		(LDO_REG_BASE  + 0x14)
#define	ANA_LED_CTL			(LDO_REG_BASE  + 0x70)
#define ANA_VIBRATOR_CTRL0	(LDO_REG_BASE  + 0x74)
#define	ANA_AUD_CLK_RST		(LDO_REG_BASE  + 0x7C)

static int check_ana(void)
{
	static u16 tag = 0x555;
	int ret = 0;
	u32 val, reg = SCI_ADDR(SPRD_MISC_BASE, 0x0144);

	/* check adi fifo r/w address */
	val = __raw_readl(SCI_ADDR(SPRD_ADI_BASE, 0x002c));
	ret |= (val >> 4 & 3) == (val >> 8 & 3);
	ret <<= 1;

	/* check ana reg w/r consistency */
	sci_adi_raw_write(reg, tag);
	ret |= sci_adi_read(reg) == tag;
	tag = ~tag & 0xfff;

	return ret;
}

static void print_ana(void)
{
	u32 val;
	val = sci_adi_read(ANA_REG_GLB_ANA_APB_CLK_EN);
	printk("##%d: ANA_REG_GLB_ANA_APB_CLK_EN = %08x.\n", check_ana(), val);

	val = sci_adi_read(ANA_REG_GLB_LDO_PD_CTRL0);
	printk("##: ANA_REG_GLB_LDO_PD_CTRL0 = %04x.\n", val);
	if ((val & BIT_LDO_BP_CAMMOT_RST)) printk("##:LDO_CAMMOT is on.\n");
	else if((val & BIT_LDO_BPCAMMOT)) printk("##: LDO_CAMMOT is off.\n");
	if ((val & BIT_LDO_BPCAMA_RST)) printk("## LDO_BPCAMA:is on.\n");
	else if((val & BIT_LDO_BPCAMA)) printk("##: LDO_BPCAMA is off.\n");
	if ((val & BIT_LDO_BPCAMIO_RST)) printk("## LDO_BPCAMIO :is on.\n");
	else if((val & BIT_LDO_BPCAMIO)) printk("## LDO_BPCAMIO : is off.\n");
	if ((val & BIT_LDO_BPCAMCORE_RST)) printk("## LDO_BPCAMCORE :is on.\n");
	else if((val & BIT_LDO_BPCAMCORE)) printk("## LDO_BPCAMCORE : is off.\n");
	if ((val & BIT_LDO_BPSIM1_RST)) printk("## LDO_BPSIM1 :is on.\n");
	else if((val & BIT_LDO_BPSIM1)) printk("## LDO_BPSIM1 : is off.\n");
	if ((val & BIT_LDO_BPSIM0_RST)) printk("## LDO_BPSIM0 :is on.\n");
	else if((val & BIT_LDO_BPSIM0)) printk("## LDO_BPSIM0 : is off.\n");
	if ((val & BIT_LDO_BPUSB_RST)) printk("## LDO_BPUSB :is on.\n");
	else if((val & BIT_LDO_BPUSB)) printk("## LDO_BPUSB : is off.\n");

	val = sci_adi_read(ANA_REG_GLB_LDO_PD_CTRL1);
	printk("##:ANA_REG_GLB_LDO_PD_CTRL1 = %04x.\n", val);
	if ((val & BIT_LDO_BPCMMB1V2_RST)) printk("## LDO_BPCMMB1V2: is on.\n");
	else if((val & BIT_LDO_BPCMMB1V2)) printk("## LDO_BPCMMB1V2: is off.\n");
	if ((val & BIT_LDO_BPCMMB1P8_RST)) printk("## LDO_BPCMMB1P8: is on.\n");
	else if((val & BIT_LDO_BPCMMB1P8)) printk("## LDO_BPCMMB1P8: is off.\n");
	if ((val & BIT_LDO_BPVDD3V_RST)) printk("## LDO_BPVDD3V: is on.\n");
	else if((val & BIT_LDO_BPVDD3V)) printk("## LDO_BPVDD3V: is off.\n");
	if ((val & BIT_LDO_BPSD3_RST)) printk("## LDO_BPSD3: is on.\n");
	else if((val & BIT_LDO_BPSD3)) printk("## LDO_BPSD3: is off.\n");
	if ((val & BIT_LDO_BPSD1_RST)) printk("## LDO_BPSD1: is on.\n");
	else if((val & BIT_LDO_BPSD1)) printk("## LDO_BPSD1: is off.\n");
	if ((val & BIT_LDO_BPSD0_RST)) printk("## LDO_BPSD0: is on.\n");
	else if((val & BIT_LDO_BPSD0)) printk("## LDO_BPSD0: is off.\n");

	val = sci_adi_read(ANA_LED_CTL);
	printk("##: ANA_LED_CTL = 0x%x.\n", val);

	val = sci_adi_read(ANA_VIBRATOR_CTRL0);
	printk("##: ANA_VIBRATOR_CTRL0 = 0x%x.\n", val);
	if (val & VIBR_PD_RST)	 printk("##: vibrator is power on.\n");	
	else if (val & VIBR_PD) printk("##: vibrator is power off.\n");

	val = sci_adi_read(ANA_AUD_CLK_RST);
	printk("##: ANA_AUD_CLK_RST = 0x%x.\n", val);

	val = sci_adi_read(ANA_REG_GLB_DCDCARM_CTRL0);
	printk("##: ANA_REG_GLB_DCDCARM_CTRL0 = 0x%x.\n", val);

}
static int is_dsp_sleep(void)
{
	u32 val;
	val = sci_glb_read(REG_GLB_STC_DSP_ST, -1UL);
	if (GR_EMC_STOP & val)
		printk("#####: REG_GLB_STC_DSP_ST[GR_EMC_STOP] is set!\n");
	else
		printk("#####: REG_GLB_STC_DSP_ST[GR_EMC_STOP] is NOT set!\n");
	if (GR_MCU_STOP & val)
		printk("#####: REG_GLB_STC_DSP_ST[GR_MCU_STOP] is set!\n");
	else
		printk("#####: REG_GLB_STC_DSP_ST[GR_MCU_STOP] is NOT set!\n");
	if (GR_DSP_STOP & val)
		printk("#####: REG_GLB_STC_DSP_ST[DSP_STOP] is set!\n");
	else
		printk("#####: REG_GLB_STC_DSP_ST[DSP_STOP] is NOT set!\n");
	return 0;
}
extern void sci_clock_dump_active(void);
static int print_thread(void * data)
{
	while(1){
		wake_lock(&messages_wakelock);
		print_ahb();
		print_gr();
		print_ana();
		is_dsp_sleep();
		has_wake_lock(WAKE_LOCK_SUSPEND);
		msleep(100);
		wake_unlock(&messages_wakelock);
		set_current_state(TASK_INTERRUPTIBLE);
		schedule_timeout(30 * HZ);
	}
	return 0;
}
void pm_debug_set_wakeup_timer()
{
	u32 val = get_sys_cnt();
	val = val + 2000;
	__raw_writel(val, SYSCNT_REG(0) );
	__raw_writel(1, SYSCNT_REG(0X8) );
}
static void debugfs_init(void)
{
	dentry_debug_root = debugfs_create_dir("power", NULL);
	if (IS_ERR(dentry_debug_root) || !dentry_debug_root) {
		printk("!!!powermanager Failed to create debugfs directory\n");
		dentry_debug_root = NULL;
		return;
	}
	debugfs_create_u32("print_sleep_mode", 0644, dentry_debug_root,
			   &is_print_sleep_mode);
	debugfs_create_u32("print_linux_clock", 0644, dentry_debug_root,
			   &is_print_linux_clock);
	debugfs_create_u32("print_modem_clock", 0644, dentry_debug_root,
			   &is_print_modem_clock);
	debugfs_create_u32("print_irq", 0644, dentry_debug_root,
			   &is_print_irq);
	debugfs_create_u32("print_wakeup", 0644, dentry_debug_root,
			   &is_print_wakeup);
	debugfs_create_u32("print_irq_runtime", 0644, dentry_debug_root,
			   &is_print_irq_runtime);
	debugfs_create_u32("print_time", 0644, dentry_debug_root,
			   &is_print_time);
}
void pm_debug_init(void)
{
	struct task_struct * task;
#ifdef PM_PRINT_ENABLE
	wake_lock_init(&messages_wakelock, WAKE_LOCK_SUSPEND,
			"pm_message_wakelock");
	task = kthread_create(print_thread, NULL, "pm_print");
	if (task == 0) {
		printk("Can't crate power manager print thread!\n");
	}else
		wake_up_process(task);
#endif
	debugfs_init();
}
void pm_debug_clr(void)
{
	if(dentry_debug_root != NULL)
		debugfs_remove_recursive(dentry_debug_root);
}
#endif
#else
void pm_debug_init(void)
{
}
void pm_debug_clr(void)
{
}
void clr_sleep_mode(void)
{
}
void time_statisic_begin(void){}
void irq_wakeup_set(void){}
void time_statisic_end(void){}
void pm_debug_save_ahb_glb_regs(void){}
void time_add(unsigned int time, int ret){}
void print_hard_irq_inloop(int ret){}
void print_statisic(void){}
void set_sleep_mode(int sm){}
#endif
#if 0
void inc_irq(int irq)
{

}

#include <linux/regulator/consumer.h>
int __init sc_ldo_slp_init(void)
{
	int i;

	static struct {
		const char *vdd_name;
		/*
		  * 0: slp pd disable, very light loads when in STANDBY mode
		  * 1: slp pd enable, this is chip default config for most of LDOs
		  */
		bool pd_en;
	} ldo_slp_config[] __initdata = {
		{"vddsim0",		0},
		{"vddsim1",		0},
		{"avddvb",		0},
	};

	for (i = 0; i < ARRAY_SIZE(ldo_slp_config); i++) {
		if (0 == ldo_slp_config[i].pd_en) {
			struct regulator *ldo =
			    regulator_get(NULL, ldo_slp_config[i].vdd_name);
			if (!WARN_ON(IS_ERR(ldo))) {
				regulator_set_mode(ldo, REGULATOR_MODE_STANDBY);
				regulator_put(ldo);
			}
			pr_info("%s slp pd disable\n", ldo_slp_config[i].vdd_name);
		}
	}
	return 0;
}

late_initcall(sc_ldo_slp_init);
#endif
