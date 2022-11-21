/*
 * PXA988 CP related
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2007 Marvell International Ltd.
 * All Rights Reserved
 */
#include <linux/kernel.h>
#include <linux/delay.h>
#include <mach/hardware.h>
#include <asm/io.h>
#include <mach/regs-mpmu.h>
#include <mach/regs-ciu.h>
#include <mach/regs-apmu.h>

#include "watchdog.h"

#define MPMU_APRR_CPR  (1 << 0)

#define MPMU_CPRR_DSPR (1 << 2)
#define MPMU_CPRR_BBR  (1 << 3)
#define MPMU_CPRR_DSRAMINT  (1 << 5)

#define APMU_DEBUG_CP_HALT        (1 << 0)
#define APMU_DEBUG_CP_CLK_OFF_ACK (1 << 3)

extern unsigned long arbel_bin_phys_addr;

/**
 * interface exported by kernel for disabling FC during hold/release CP
 */
extern void acquire_fc_mutex(void);
extern void release_fc_mutex(void);

/**
 * CP release procedure
 * 1. acquire fc mutex
 * 2. unmask bits CP_CLK_OFF_ACK, CP_HALT of register APMU_DEBUG to
 *    enable ACK from CP
 * 3. clear bit CPR of register APRR to release CP
 * 4. wait until read back zero value after clear bit CPR
 * 5. release fc mutex
 */
void cp_releasecp(void)
{
	int value;
	int timeout = 1000000;
	u8 apmu_debug;

	acquire_fc_mutex();
	printk("%s: acquire fc mutex\n", __func__);

	/*
	 * unmask CP_CLK_OFF_ACK and CP_HALT
	 */
	printk("%s: unmask cp halt and cp clk off ack bit\n", __func__);
	apmu_debug = __raw_readb(APMU_DEBUG);
	printk("%s: APMU_DEBUG before %02x\n", __func__, apmu_debug);
	apmu_debug &= ~(APMU_DEBUG_CP_HALT | APMU_DEBUG_CP_CLK_OFF_ACK);
	__raw_writeb(apmu_debug, APMU_DEBUG);
	printk("%s: APMU_DEBUG after %02x\n", __func__, __raw_readb(APMU_DEBUG));

	writel(arbel_bin_phys_addr, CIU_SW_BRANCH_ADDR);

	printk("release cp...\n");
	value = __raw_readl(MPMU_APRR);
	printk("the PMUM_APRR value is 0x%08x\n", MPMU_APRR);
	printk("the value is 0x%08x\n", value);
	value &= ~MPMU_APRR_CPR; //set CPR(bit[0]) bit of APRR register low
	__raw_writel(value, MPMU_APRR);
	//avoid endless loop
	//TODO: accurate the timeout time
	while(((__raw_readl(MPMU_APRR) & MPMU_APRR_CPR) != 0x0) && timeout)
		timeout--;
	if(!timeout) {
		printk("fail!\n");
		/*
		 * release CP failed, invoke panic() here???
		 * mask CP_CLK_OFF_ACK and CP_HALT again to avoid hang in FC
		 */
		printk("%s: mask cp halt and cp clk off ack bit again\n", __func__);
		apmu_debug = __raw_readb(APMU_DEBUG);
		printk("%s: APMU_DEBUG before %02x\n", __func__, apmu_debug);
		apmu_debug |= (APMU_DEBUG_CP_HALT | APMU_DEBUG_CP_CLK_OFF_ACK);
		__raw_writeb(apmu_debug, APMU_DEBUG);
		printk("%s: APMU_DEBUG after %02x\n", __func__, __raw_readb(APMU_DEBUG));
	}
	else
		printk("done!\n");

	release_fc_mutex();
	printk("%s: release fc mutex\n", __func__);
}

/**
 * CP hold procedure
 * 1. acquire fc mutex
 * 2. stop watchdog
 * 3. set bit CPR of register APRR to hold CP in reset
 * 4. set bits DSPR, BBR and DSRAMINT of register CPRR to hold MSA in reset
 * 5. wait several us
 * 6. mask bits CP_CLK_OFF_ACK, CP_HALT of register APMU_DEBUG to
 *    disable ACK from CP
 * 7. release fc mutex
 */
void cp_holdcp(void)
{
	int value;
	u8 apmu_debug;

	acquire_fc_mutex();
	printk("%s: acquire fc mutex\n", __func__);

	watchDogCountStop();
	printk("hold cp...");
	value = __raw_readl(MPMU_APRR) | MPMU_APRR_CPR; //set CPR(bit[0]) bit of APRR register high
	__raw_writel(value, MPMU_APRR);
	printk("done!\n");
	printk("hold msa...");
	value = __raw_readl(MPMU_CPRR);
	value |= MPMU_CPRR_DSPR | MPMU_CPRR_BBR | MPMU_CPRR_DSRAMINT; //set DSPR(bit[2]), BBR(bit[3]) and DSRAMINT(bit[5]) bits of CPRR register high
	__raw_writel(value, MPMU_CPRR);
	printk("done!\n");
	//recommended software wait several us after hold CP/MSA in reset status and then start load the new image
	//TODO: accurate the wait time
	udelay(100);

	/*
	 * mask CP_CLK_OFF_ACK and CP_HALT
	 */
	printk("%s: mask cp halt and cp clk off ack bit\n", __func__);
	apmu_debug = __raw_readb(APMU_DEBUG);
	printk("%s: APMU_DEBUG before %02x\n", __func__, apmu_debug);
	apmu_debug |= (APMU_DEBUG_CP_HALT | APMU_DEBUG_CP_CLK_OFF_ACK);
	__raw_writeb(apmu_debug, APMU_DEBUG);
	printk("%s: APMU_DEBUG after %02x\n", __func__, __raw_readb(APMU_DEBUG));

	release_fc_mutex();
	printk("%s: release fc mutex\n", __func__);
}

bool cp_get_status(void)
{
	return !(readl(MPMU_APRR) & MPMU_APRR_CPR);
}

