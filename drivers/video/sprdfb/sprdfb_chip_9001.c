/******************************************************************************
 ** File Name:    sprdfb_chip_whale.c                                *
 ** Author:       junxiao.feng junxiao.feng@spreadtrum.com          *
 ** DATE:         2015-05-05                                        *
 ** Copyright:    2015 Spreatrum, Incoporated. All Rights Reserved. *
 ** Description:                                                    *
 ******************************************************************************/

#include "sprdfb_chip_common.h"
#include "sprdfb.h"

void dsi_enable(void)
{
	sci_glb_clr(REG_ANA_APB_PWR_CTRL1, BIT_ANA_APB_MIPI_DSI_PS_PD_S_S);
	sci_glb_clr(REG_ANA_APB_PWR_CTRL1, BIT_ANA_APB_MIPI_DSI_PS_PD_L_S);
	sci_glb_set(REG_ANA_APB_MIPI_PHY_CTRL1,
		BIT_ANA_APB_DSI_CFGCLK_SEL
		|BIT_ANA_APB_DSI_REFCLK_SEL);

	sci_glb_set(REG_ANA_APB_MIPI_PHY_CTRL2,
		BIT_ANA_APB_DSI_ENABLECLK 
		|BIT_ANA_APB_DSI_ENABLE_0_S 
		|BIT_ANA_APB_DSI_ENABLE_1_S
		|BIT_ANA_APB_DSI_ENABLE_2_S
		|BIT_ANA_APB_DSI_ENABLE_3_S);
	sci_glb_set(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_REF_CKG_EN);
	sci_glb_set(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_CFG_CKG_EN);
	pr_info("sprdfb: dsi_enable %08x\n", REG_AP_AHB_MISC_CKG_EN);
	sci_glb_set(DSI_REG_EB, DSI_BIT_EB);
}

void dsi_disable(void)
{
	sci_glb_set(REG_ANA_APB_PWR_CTRL1, BIT_ANA_APB_MIPI_DSI_PS_PD_S_S);
	sci_glb_set(REG_ANA_APB_PWR_CTRL1, BIT_ANA_APB_MIPI_DSI_PS_PD_L_S);
	sci_glb_clr(REG_ANA_APB_MIPI_PHY_CTRL1,
		BIT_ANA_APB_DSI_CFGCLK_SEL
		|BIT_ANA_APB_DSI_REFCLK_SEL);

	sci_glb_clr(REG_ANA_APB_MIPI_PHY_CTRL2,
		BIT_ANA_APB_DSI_ENABLECLK
		|BIT_ANA_APB_DSI_ENABLE_0_S
		|BIT_ANA_APB_DSI_ENABLE_1_S
		|BIT_ANA_APB_DSI_ENABLE_2_S
		|BIT_ANA_APB_DSI_ENABLE_3_S);
	sci_glb_clr(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_REF_CKG_EN);
	sci_glb_clr(REG_AP_AHB_MISC_CKG_EN, BIT_DPHY_CFG_CKG_EN);
	sci_glb_clr(DSI_REG_EB, DSI_BIT_EB);
}

int disp_domain_power_ctrl(int enable)
{
        if (enable) {
                printk("disp_domain_power_ctrl Enter \n");
		sci_glb_set(REG_PMU_APB_PD_DISP_SYS_CFG, ~BIT_PMU_APB_PD_DISP_SYS_FORCE_SHUTDOWN);
                //__raw_bits_and(~BIT_PMU_APB_PD_DISP_SYS_FORCE_SHUTDOWN, REG_PMU_APB_PD_DISP_SYS_CFG);//SPRD_PMU_BASE
                printk("disp_domain_power_ctrl __raw_bits_and Exit \n");
                mdelay(10);
		sci_glb_set(REG_AON_APB_APB_EB1, BIT_AON_APB_DISP_EMC_EB | BIT_DISP_EB);
                //__raw_bits_or(BIT_DISP_EMC_EB | BIT_DISP_EB, REG_AON_APB_APB_EB1);//SPRD_AONAPB_BASE
        } else {
		sci_glb_set(REG_AON_APB_APB_EB1, ~(BIT_DISP_EB | BIT_AON_APB_DISP_EMC_EB));
                //__raw_bits_and(~(BIT_DISP_EB | BIT_DISP_EMC_EB), REG_AON_APB_APB_EB1);//SPRD_AONAPB_BASE
                mdelay(10);
		sci_glb_set(REG_PMU_APB_PD_DISP_SYS_CFG, BIT_PMU_APB_PD_DISP_SYS_FORCE_SHUTDOWN);
               // __raw_bits_or(BIT_PMU_APB_PD_DISP_SYS_FORCE_SHUTDOWN, REG_PMU_APB_PD_DISP_SYS_CFG);//SPRD_AONAPB_BASE
        }

        return 0;
}
void dispc_early_configs(void)
{
	sci_glb_set(REG_DISP_AHB_AHB_EB,
		BIT_DISP_AHB_DISPC0_EB |
		BIT_DISP_AHB_DISPC1_EB |
		BIT_DISP_AHB_DISPC_MMU_EB |
		BIT_DISP_AHB_DISPC_MTX_EB);
	sci_glb_set(REG_DISP_AHB_GEN_CKG_CFG,
		BIT_DISP_AHB_DISPC_NOC_AUTO_CKG_EN |
		BIT_DISP_AHB_DISPC_MTX_AUTO_CKG_EN |
		BIT_DISP_AHB_DISPC_MTX_FORCE_CKG_EN |
		BIT_DISP_AHB_DISPC_NOC_FORCE_CKG_EN |
		BIT_DISP_AHB_DPHY1_CFG_CKG_EN);

        printk("disp_pw_domain: REG_DISP_AHB_GEN_CKG_CFG = 0x%x\n",__raw_readl(REG_DISP_AHB_GEN_CKG_CFG));
}
void dispc_disable_configs(void)
{
	sci_glb_clr(REG_DISP_AHB_AHB_EB,
		BIT_DISP_AHB_DISPC0_EB |
		BIT_DISP_AHB_DISPC1_EB |
		BIT_DISP_AHB_DISPC_MMU_EB |
		BIT_DISP_AHB_DISPC_MTX_EB);
	sci_glb_clr(REG_DISP_AHB_GEN_CKG_CFG,
		BIT_DISP_AHB_DISPC_NOC_AUTO_CKG_EN |
		BIT_DISP_AHB_DISPC_MTX_AUTO_CKG_EN |
		BIT_DISP_AHB_DISPC_MTX_FORCE_CKG_EN /*|
		BIT_DISP_AHB_DISPC_NOC_FORCE_CKG_EN*/);
        printk("disp_pw_domain: REG_DISP_AHB_GEN_CKG_CFG = 0x%x\n",__raw_readl(REG_DISP_AHB_GEN_CKG_CFG));
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

void misc_noc_ctrl(unsigned long base, unsigned int mode, unsigned int sel)
{
	unsigned int val;

	val = *(volatile unsigned int *)base;
	val &= ~(INTERLEAVE_MODE_MASK | INTERLEAVE_SEL_MASK);
	val |= (mode << INTERLEAVE_MODE_OFFSET) |
		(sel << INTERLEAVE_SEL_OFFSET);
	*(volatile unsigned int *)base = val;
}
