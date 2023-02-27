/******************************************************************************
 ** File Name:    sprdfb_chip_8830.h                                     *
 ** Author:       congfu.zhao                                           *
 ** DATE:         30/04/2013                                        *
 ** Copyright:    2013 Spreatrum, Incoporated. All Rights Reserved. *
 ** Description:                                                    *
 ******************************************************************************/
/******************************************************************************
 **                   Edit    History                               *
 **---------------------------------------------------------------------------*
 ** DATE          NAME            DESCRIPTION                       *

 ******************************************************************************/
#ifndef _SC8830_DISPC_GLB_REG_K_H_
#define _SC8830_DISPC_GLB_REG_K_H_


#include <linux/kernel.h>

#include <mach/sci_glb_regs.h>


//#define BIT(x) (1<<x)
#define	SPRD_MIPI_DPHY_GEN2

#ifndef CONFIG_OF
#define DISPC_PLL_CLK				("clk_disc0")
#define DISPC_DBI_CLK				("clk_disc0_dbi")
#define DISPC_DPI_CLK				("clk_disc0_dpi")
#define DISPC_EMC_CLK				("clk_disp_emc")
#define DISPC_DPI_CLOCK 			(384*1000000/7)

#define SPRD_DISPC_BASE			SPRD_LCDC_BASE
#define	IRQ_DISPC_INT			IRQ_DISPC0_INT

#define SPRD_MIPI_DSIC_BASE 		SPRD_DSI_BASE
#else
#define SPRD_MIPI_DSIC_BASE 		g_dsi_base_addr
#endif

#define DSI_AHB_SOFT_RST           		REG_AP_AHB_AHB_RST
//#define BIT_DSI_SOFT_RST	 		BIT_DSI_SOFT_RST
//#define BIT_DSI_SOFT_RST	 		( BIT(0) )

#define DSI_REG_EB				REG_AP_AHB_AHB_EB
#define DSI_BIT_EB					BIT_DSI_EB

#ifndef CONFIG_OF
#define	IRQ_DSI_INTN0			IRQ_DSI0_INT
#define	IRQ_DSI_INTN1			IRQ_DSI1_INT
#endif

#define REG_AHB_SOFT_RST 			(0x4 + SPRD_AHB_BASE)
#define BIT_DISPC_SOFT_RST			BIT_DISPC0_SOFT_RST

#define DISPC_CORE_EN			(REG_AP_APB_APB_EB)
#define BIT_DISPC_CORE_EN			(BIT_AP_CKG_EB)

#ifndef CONFIG_OF
#define DISPC_AHB_EN				(REG_AP_AHB_AHB_EB)
#define BIT_DISPC_AHB_EN			(BIT_DISPC0_EB)

#define DISPC_EMC_EN				(REG_AON_APB_APB_EB1)
#define BIT_DISPC_EMC_EN			(BIT_DISP_EMC_EB)

#define DISPC_PLL_SEL_CFG			REG_AP_CLK_DISPC0_CFG
#define BITS_DISPC_PLL_SEL_CFG		0
#define BIT0_DISPC_PLL_SEL_CFG		BIT(0)
#define BIT1_DISPC_PLL_SEL_CFG		BIT(1)
#define BIT_DISPC_PLL_SEL_MSK		BIT1_DISPC_PLL_SEL_CFG | BIT0_DISPC_PLL_SEL_CFG

#define DISPC_PLL_DIV_CFG			REG_AP_CLK_DISPC0_CFG
#define BITS_DISPC_PLL_DIV_CFG		8
#define BIT0_DISPC_PLL_DIV_CFG		BIT(8)
#define BIT1_DISPC_PLL_DIV_CFG		BIT(9)
#define BIT2_DISPC_PLL_DIV_CFG		BIT(10)
#define BIT_DISPC_PLL_DIV_MSK		BIT0_DISPC_PLL_DIV_CFG | BIT1_DISPC_PLL_DIV_CFG | BIT2_DISPC_PLL_DIV_CFG

#define DISPC_DBI_SEL_CFG			REG_AP_CLK_DISPC0_DBI_CFG
#define BITS_DISPC_DBI_SEL_CFG		0
#define BIT0_DISPC_DBI_SEL_CFG		BIT(0)
#define BIT1_DISPC_DBI_SEL_CFG		BIT(1)
#define BIT_DISPC_DBI_SEL_MSK		BIT0_DISPC_DBI_SEL_CFG | BIT1_DISPC_DBI_SEL_CFG

#define DISPC_DBI_DIV_CFG			REG_AP_CLK_DISPC0_DBI_CFG
#define BITS_DISPC_DBI_DIV_CFG		8
#define BIT0_DISPC_DBI_DIV_CFG		BIT(8)
#define BIT1_DISPC_DBI_DIV_CFG		BIT(9)
#define BIT2_DISPC_DBI_DIV_CFG		BIT(10)
#define BIT_DISPC_DBI_DIV_MSK		BIT0_DISPC_DBI_DIV_CFG | BIT1_DISPC_DBI_DIV_CFG | BIT2_DISPC_DBI_DIV_CFG

#define DISPC_DPI_SEL_CFG			REG_AP_CLK_DISPC0_DPI_CFG
#define BITS_DISPC_DPI_SEL_CFG		0
#define BIT0_DISPC_DPI_SEL_CFG		BIT(0)
#define BIT1_DISPC_DPI_SEL_CFG		BIT(1)
#define BIT_DISPC_DPI_SEL_MSK		BIT0_DISPC_DPI_SEL_CFG | BIT1_DISPC_DPI_SEL_CFG

#define DISPC_DPI_DIV_CFG			REG_AP_CLK_DISPC0_DPI_CFG
#define BITS_DISPC_DPI_DIV_CFG		8
#define BIT0_DISPC_DPI_DIV_CFG		BIT(8)
#define BIT1_DISPC_DPI_DIV_CFG		BIT(9)
#define BIT2_DISPC_DPI_DIV_CFG		BIT(10)
#define BIT3_DISPC_DPI_DIV_CFG		BIT(11)
#define BIT4_DISPC_DPI_DIV_CFG		BIT(12)
#define BIT5_DISPC_DPI_DIV_CFG		BIT(13)
#define BIT6_DISPC_DPI_DIV_CFG		BIT(14)
#define BIT7_DISPC_DPI_DIV_CFG		BIT(15)
#define BIT_DISPC_DPI_DIV_MSK		BIT0_DISPC_DPI_DIV_CFG | BIT1_DISPC_DPI_DIV_CFG | BIT2_DISPC_DPI_DIV_CFG | BIT3_DISPC_DPI_DIV_CFG \
					| BIT4_DISPC_DPI_DIV_CFG | BIT5_DISPC_DPI_DIV_CFG | BIT6_DISPC_DPI_DIV_CFG | BIT7_DISPC_DPI_DIV_CFG


enum{
	DISPC_PLL_SEL_96M = 0,
	DISPC_PLL_SEL_192M,
	DISPC_PLL_SEL_256M,
	DISPC_PLL_SEL_312M
};

enum{
	DISPC_DBI_SEL_128M = 0,
	DISPC_DBI_SEL_153_6M,
	DISPC_DBI_SEL_192M,
	DISPC_DBI_SEL_256M
};

enum{
	DISPC_DPI_SEL_128M = 0,
	DISPC_DPI_SEL_153_6M,
	DISPC_DPI_SEL_192M,
	DISPC_DPI_SEL_384M
};
#endif

#endif
