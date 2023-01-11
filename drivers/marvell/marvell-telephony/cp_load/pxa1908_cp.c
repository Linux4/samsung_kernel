/*
 * PXA1908 CP related
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html

 * (C) Copyright 2007 Marvell International Ltd.
 * All Rights Reserved
 */
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/module.h>
#include "pxa_cp_load.h"
#include "common_regs.h"

/*
 * pxa1908 cp related operations are almost the same as as pxa988
 * exception the branch address set
 * for pxa988, we need to set the branch address CIU_SW_BRANCH_ADDR
 * but pxa1908, we need to set the remap register CIU_SEAGULL_REMAP
 */
void cp1908_releasecp(void)
{
	/* the load address must be 64k aligned */
	BUG_ON(arbel_bin_phys_addr & 0xFFFF);
	cp_set_seagull_remap_reg(arbel_bin_phys_addr | 0x01);
	__cp988_releasecp();
}

void cp1908_holdcp(void)
{
	cp988_holdcp();
}

bool cp1908_get_status(void)
{
	return cp988_get_status();
}

