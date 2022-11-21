/******************************************************************************
 ** File Name:    sprdfb_chip_9630.c                                *
 ** Author:       Yang.Haibing Haibing.yang@spreadtrum.com          *
 ** DATE:         2014-05-14                                        *
 ** Copyright:    2014 Spreatrum, Incoporated. All Rights Reserved. *
 ** Description:                                                    *
 ******************************************************************************/

#include "sprdfb_chip_9630.h"
#include "sprdfb_chip_common.h"


void dsi_enable(void)
{
	sci_glb_clr(REG_AON_APB_PWR_CTRL, BIT_MIPI_DSI_PS_PD_S);
	sci_glb_clr(REG_AON_APB_PWR_CTRL, BIT_MIPI_DSI_PS_PD_L);
	sci_glb_set(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_REF_CKG_EN);
	sci_glb_set(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_CFG_CKG_EN);
	pr_info("sprdfb: dsi_enable %08x\n", REG_AP_AHB_MISC_CKG_EN);
	sci_glb_set(DSI_REG_EB, DSI_BIT_EB);
}

void dsi_disable(void)
{
	sci_glb_set(REG_AON_APB_PWR_CTRL, BIT_MIPI_DSI_PS_PD_S);
	sci_glb_set(REG_AON_APB_PWR_CTRL, BIT_MIPI_DSI_PS_PD_L);
	sci_glb_clr(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_REF_CKG_EN);
	sci_glb_clr(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_CFG_CKG_EN);
	sci_glb_clr(DSI_REG_EB, DSI_BIT_EB);
}

void dsi_print_global_config(void)
{
	u32 reg_val0, reg_val1, reg_val2;

	reg_val0 = sci_glb_read(REG_AP_AHB_MISC_CKG_EN, 0xffffffff);
	reg_val1 = sci_glb_read(DSI_REG_EB, 0xffffffff);
	reg_val2 = sci_glb_read(REG_AON_APB_PWR_CTRL, 0xffffffff);

	printk("sprdfb: dsi: 0x20E00040 = 0x%x, 0x20E00000 = 0x%x, 0x402E0024 = 0x%x\n",
		reg_val0, reg_val1, reg_val2);
}

void dispc_print_clk(void)
{
    u32 reg_val0, reg_val1, reg_val2;
    reg_val0 = dispc_glb_read(SPRD_AONAPB_BASE + 0x4);
    reg_val1 = dispc_glb_read(SPRD_AHB_BASE);
    reg_val2 = dispc_glb_read(SPRD_APBREG_BASE);
    pr_info("sprdfb:0x402e0004 = 0x%x 0x20d00000 = 0x%x 0x71300000 = 0x%x\n",
			reg_val0, reg_val1, reg_val2);

    reg_val0 = dispc_glb_read(SPRD_APBCKG_BASE + 0x34);
    reg_val1 = dispc_glb_read(SPRD_APBCKG_BASE + 0x30);
    reg_val2 = dispc_glb_read(SPRD_APBCKG_BASE + 0x2c);
    pr_info("sprdfb:0x71200034 = 0x%x 0x71200030 = 0x%x 0x7120002c = 0x%x\n",
			reg_val0, reg_val1, reg_val2);
}
