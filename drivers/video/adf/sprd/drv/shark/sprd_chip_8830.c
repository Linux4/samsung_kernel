/******************************************************************************
 ** File Name:    sprdfb_chip_whale.c                                *
 ** Author:       junxiao.feng junxiao.feng@spreadtrum.com          *
 ** DATE:         2015-05-05                                        *
 ** Copyright:    2015 Spreatrum, Incoporated. All Rights Reserved. *
 ** Description:                                                    *
 ******************************************************************************/

#include "sprd_chip_common.h"
#include "sprd_display.h"
#include "sprd_chip_8830.h"
void dsi_enable(int index)
{
#ifdef CONFIG_FB_SCX30G
	sci_glb_clr(REG_AON_APB_PWR_CTRL, BIT_DSI_PHY_PD);
#endif
	sci_glb_set(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_REF_CKG_EN);
	sci_glb_set(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_CFG_CKG_EN);
	pr_info("sprd: dsi_enable %08x\n", REG_AP_AHB_MISC_CKG_EN);
	sci_glb_set(DSI_REG_EB, DSI_BIT_EB<<index);
}

void dsi_disable(int index)
{
	sci_glb_clr(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_REF_CKG_EN);
	sci_glb_clr(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_CFG_CKG_EN);
	sci_glb_clr(DSI_REG_EB, DSI_BIT_EB);
#ifdef CONFIG_FB_SCX30G
    sci_glb_set(REG_AON_APB_PWR_CTRL, BIT_DSI_PHY_PD);
#endif
}

int dispc_domain_power_ctrl(int enable)
{
        return 0;
}
void dispc_early_configs(int dev_id)
{
}

void dispc_disable_configs(int dev_id)
{

}

