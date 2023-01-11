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
#include <linux/io.h>
#include <linux/module.h>
#include "pxa_cp_load.h"
#include "common_regs.h"

/**
 * CP release procedure
 * 1. acquire fc mutex
 * 2. unmask bits CP_CLK_OFF_ACK, CP_HALT of register APMU_DEBUG to
 *    enable ACK from CP
 * 3. clear bit CPR of register APRR to release CP
 * 4. wait until read back zero value after clear bit CPR
 * 5. release fc mutex
 */
void __cp988_releasecp(void)
{
	int value;
	int timeout = 1000000;
	u8 apmu_debug;

	acquire_fc_mutex();
	pr_info("%s: acquire fc mutex\n", __func__);

	/*
	 * unmask CP_CLK_OFF_ACK and CP_HALT
	 */
	pr_info("%s: unmask cp halt and cp clk off ack bit\n",
	       __func__);
	apmu_debug = __raw_readb(APMU_DEBUG);
	pr_info("%s: APMU_DEBUG before %02x\n", __func__, apmu_debug);
	apmu_debug &= ~(APMU_DEBUG_CP_HALT | APMU_DEBUG_CP_CLK_OFF_ACK);
	__raw_writeb(apmu_debug, APMU_DEBUG);
	pr_info("%s: APMU_DEBUG after %02x\n", __func__,
	       __raw_readb(APMU_DEBUG));

	pr_info("release cp...\n");
	value = __raw_readl(MPMU_APRR);
	pr_info("the PMUM_APRR value is %p\n",
	       MPMU_APRR);
	pr_info("the value is 0x%08x\n", value);
	/* set CPR(bit[0]) bit of APRR register low */
	value &= ~MPMU_APRR_CPR;
	__raw_writel(value, MPMU_APRR);
	/*
	 * avoid endless loop
	 * TODO: accurate the timeout time
	 */
	while (((__raw_readl(MPMU_APRR)
		 & MPMU_APRR_CPR) != 0x0) && timeout)
		timeout--;
	if (!timeout) {
		pr_info("fail!\n");
		/*
		 * release CP failed, invoke panic() here???
		 * mask CP_CLK_OFF_ACK and CP_HALT again to avoid hang in FC
		 */
		pr_info(
		       "%s: mask cp halt and cp clk off ack bit again\n",
		       __func__);
		apmu_debug = __raw_readb(APMU_DEBUG);
		pr_info("%s: APMU_DEBUG before %02x\n",
		       __func__, apmu_debug);
		apmu_debug |= (APMU_DEBUG_CP_HALT | APMU_DEBUG_CP_CLK_OFF_ACK);
		__raw_writeb(apmu_debug, APMU_DEBUG);
		pr_info("%s: APMU_DEBUG after %02x\n", __func__,
		       __raw_readb(APMU_DEBUG));
	} else
		pr_info("done!\n");

	release_fc_mutex();
	pr_info("%s: release fc mutex\n", __func__);
}

void cp988_releasecp(void)
{
	writel(arbel_bin_phys_addr, CIU_SW_BRANCH_ADDR);
	__cp988_releasecp();
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
void cp988_holdcp(void)
{
	int value;
	u8 apmu_debug;

	acquire_fc_mutex();
	pr_info("%s: acquire fc mutex\n", __func__);

	if (watchdog_count_stop_fp)
		watchdog_count_stop_fp();

	pr_info("hold cp...");
	/* set CPR(bit[0]) bit of APRR register high */
	value = __raw_readl(MPMU_APRR) | MPMU_APRR_CPR;
	__raw_writel(value, MPMU_APRR);
	pr_info("done!\n");
	pr_info("hold msa...");
	value = __raw_readl(MPMU_CPRR);
	/* set DSPR(bit[2]), BBR(bit[3]) and DSRAMINT(bit[5])
	 * bits of CPRR register high */
	value |= MPMU_CPRR_DSPR | MPMU_CPRR_BBR | MPMU_CPRR_DSRAMINT;
	__raw_writel(value, MPMU_CPRR);
	pr_info("done!\n");
	/*
	 * recommended software wait several us after hold CP/MSA
	 * in reset status and then start load the new image
	 * TODO: accurate the wait time
	 */
	udelay(100);

	/*
	 * mask CP_CLK_OFF_ACK and CP_HALT
	 */
	pr_info("%s: mask cp halt and cp clk off ack bit\n",
	       __func__);
	apmu_debug = __raw_readb(APMU_DEBUG);
	pr_info("%s: APMU_DEBUG before %02x\n", __func__, apmu_debug);
	apmu_debug |= (APMU_DEBUG_CP_HALT | APMU_DEBUG_CP_CLK_OFF_ACK);
	__raw_writeb(apmu_debug, APMU_DEBUG);
	pr_info("%s: APMU_DEBUG after %02x\n", __func__,
	       __raw_readb(APMU_DEBUG));

	release_fc_mutex();
	pr_info("%s: release fc mutex\n", __func__);
}

bool cp988_get_status(void)
{
	return !(readl(MPMU_APRR) & MPMU_APRR_CPR);
}
