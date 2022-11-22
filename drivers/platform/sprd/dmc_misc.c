/* * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/init.h>
#include <linux/io.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/common.h>
#include <soc/sprd/hardware.h>
static u32 umctl_perfhpr1_save = 0x08000020;
static u32 umctl_perflpr1_save = 0x08000100;
static u32 umctl_perfwr1_save = 0x10000040;
static u32 pcfgr_1_save = 0;
/*
 * in some scenarios, the default ddr qos setting is not
 * suitable for this case, so the system need dynamic qos
 * change.
 * id: the secnario flag
 * set:if set is 1, change the qos for this scenario,
 * if set is 0, change the qos to the defualt.
 * now this function is not support scenarios confict.
 */
void dynamic_dmc_qos_config(u32 id, u32 set)
{
#if defined(CONFIG_ARCH_SCX30G) || defined(CONFIG_ARCH_SCX35L)
	u32 val;
	if(!set) {
		/* restore the default config*/
		sci_glb_write(SPRD_LPDDR2_BASE+0x25c, umctl_perfhpr1_save, -1UL);
		sci_glb_write(SPRD_LPDDR2_BASE+0x264, umctl_perflpr1_save, -1UL);
		sci_glb_write(SPRD_LPDDR2_BASE+0x26c, umctl_perfwr1_save, -1UL);
#if defined(CONFIG_ARCH_SCX35L)
		sci_glb_write(SPRD_LPDDR2_BASE+0x4b4, pcfgr_1_save, -1UL);
#endif
		return;
	}

	switch(id)
	{
	case 0:
		/*caramer recording + 4xzoom scenorio*/
		sci_glb_write(SPRD_LPDDR2_BASE+0x25c, 0x08000020, -1UL);
		sci_glb_write(SPRD_LPDDR2_BASE+0x264, 0x08000100, -1UL);
		sci_glb_write(SPRD_LPDDR2_BASE+0x26c, 0x10000040, -1UL);
#if defined(CONFIG_ARCH_SCX35L)
		val = pcfgr_1_save;
		val &= ~(0x3ff);
		val |= 0x100;
		sci_glb_write(SPRD_LPDDR2_BASE+0x4b4, val, -1UL);//change dispc timeout to 0x100
#endif
		break;
	default:
			break;
	}
#endif
}
static int __init dmc_misc_init(void)
{
	/* save the defualt config */
	umctl_perfhpr1_save = sci_glb_read(SPRD_LPDDR2_BASE+0x25c, -1UL);
	umctl_perflpr1_save = sci_glb_read(SPRD_LPDDR2_BASE+0x264, -1UL);
	umctl_perfwr1_save = sci_glb_read(SPRD_LPDDR2_BASE+0x26c, -1UL);
	pcfgr_1_save = sci_glb_read(SPRD_LPDDR2_BASE+0x4b4, -1UL);
	return 0;
}
static void  __exit dmc_misc_exit(void)
{
}
module_init(dmc_misc_init);
module_exit(dmc_misc_exit);
