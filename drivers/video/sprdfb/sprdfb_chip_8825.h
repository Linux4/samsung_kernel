/******************************************************************************
 ** File Name:    sprdfb_chip_8825.h                                     *
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
#ifndef _SC8825_DISPC_GLB_REG_K_H_
#define _SC8825_DISPC_GLB_REG_K_H_

#include <linux/kernel.h>


#define BIT(x) (1<<x)

#define	SPRD_MIPI_DPHY_GEN1

#ifndef CONFIG_OF
#define DISPC_PLL_CLK					("clk_dispc")
#define DISPC_DBI_CLK					("clk_dispc_dbi")
#define DISPC_DPI_CLK					("clk_dispc_dpi")
#define DISPC_DPI_CLOCK 				(384*1000000/11)

#define SPRD_DISPC_BASE			SPRD_DISPLAY_BASE
#define	IRQ_DISPC_INTN				IRQ_DISPC_INT
#endif

#ifndef CONFIG_OF
#define	IRQ_DSI_INTN0				IRQ_DSI_INT0
#define	IRQ_DSI_INTN1				IRQ_DSI_INT1
#endif

#define REG_AHB_SOFT_RST			(SPRD_AHB_BASE + AHB_SOFT_RST + 0x200)
#define BIT_DISPC_SOFT_RST		 	BIT(20)

#define DSI_AHB_SOFT_RST			(SPRD_AHB_BASE + AHB_SOFT_RST + 0x200) 
#define BIT_DSI_SOFT_RST	 		BIT(26)

#define AHB_MIPI_PHY_CTRL 		(0x021c)
#define DSI_REG_EB 				(AHB_MIPI_PHY_CTRL + SPRD_AHB_BASE)
#define DSI_BIT_EB					BIT(0)

#define DISPC_AHB_EN				(SPRD_AHB_BASE + AHB_CTL0 + 0x200)
#define BIT_DISPC_AHB_EN			(BIT(22))

#define DISPC_CORE_EN			(SPRD_AHB_BASE + AHB_CTL2 + 0x200)
#define BIT_DISPC_CORE_EN			(BIT(9))

#define DISPC_EMC_EN				(SPRD_AHB_BASE + AHB_CTL2 + 0x200)
#define BIT_DISPC_EMC_EN			(BIT(11))

#define DISPC_PLL_SEL_CFG			(SPRD_AHB_BASE+0x220)
#define BITS_DISPC_PLL_SEL_CFG		1
#define BIT0_DISPC_PLL_SEL_CFG		BIT(1)
#define BIT1_DISPC_PLL_SEL_CFG		BIT(2)
#define BIT_DISPC_PLL_SEL_MSK		BIT1_DISPC_PLL_SEL_CFG | BIT0_DISPC_PLL_SEL_CFG

#define DISPC_PLL_DIV_CFG			(SPRD_AHB_BASE+0x220)
#define BITS_DISPC_PLL_DIV_CFG		3
#define BIT0_DISPC_PLL_DIV_CFG		BIT(3)
#define BIT1_DISPC_PLL_DIV_CFG		BIT(4)
#define BIT2_DISPC_PLL_DIV_CFG		BIT(5)
#define BIT_DISPC_PLL_DIV_MSK		BIT0_DISPC_PLL_DIV_CFG | BIT1_DISPC_PLL_DIV_CFG | BIT2_DISPC_PLL_DIV_CFG

#define DISPC_DBI_SEL_CFG			(SPRD_AHB_BASE+0x220)
#define BITS_DISPC_DBI_SEL_CFG		9
#define BIT0_DISPC_DBI_SEL_CFG		BIT(9)
#define BIT1_DISPC_DBI_SEL_CFG		BIT(10)
#define BIT_DISPC_DBI_SEL_MSK		BIT0_DISPC_DBI_SEL_CFG | BIT1_DISPC_DBI_SEL_CFG

#define DISPC_DBI_DIV_CFG			(SPRD_AHB_BASE+0x220)
#define BITS_DISPC_DBI_DIV_CFG		11
#define BIT0_DISPC_DBI_DIV_CFG		BIT(11)
#define BIT1_DISPC_DBI_DIV_CFG		BIT(12)
#define BIT2_DISPC_DBI_DIV_CFG		BIT(13)
#define BIT_DISPC_DBI_DIV_MSK		BIT0_DISPC_DBI_DIV_CFG | BIT1_DISPC_DBI_DIV_CFG | BIT2_DISPC_DBI_DIV_CFG

#define DISPC_DPI_SEL_CFG			(SPRD_AHB_BASE+0x220)
#define BITS_DISPC_DPI_SEL_CFG		17
#define BIT0_DISPC_DPI_SEL_CFG		BIT(17)
#define BIT1_DISPC_DPI_SEL_CFG		BIT(18)
#define BIT_DISPC_DPI_SEL_MSK		BIT0_DISPC_DPI_SEL_CFG | BIT1_DISPC_DPI_SEL_CFG

#define DISPC_DPI_DIV_CFG			(SPRD_AHB_BASE+0x220)
#define BITS_DISPC_DPI_DIV_CFG		19
#define BIT0_DISPC_DPI_DIV_CFG		BIT(19)
#define BIT1_DISPC_DPI_DIV_CFG		BIT(20)
#define BIT2_DISPC_DPI_DIV_CFG		BIT(21)
#define BIT3_DISPC_DPI_DIV_CFG		BIT(22)
#define BIT4_DISPC_DPI_DIV_CFG		BIT(23)
#define BIT5_DISPC_DPI_DIV_CFG		BIT(24)
#define BIT6_DISPC_DPI_DIV_CFG		BIT(25)
#define BIT7_DISPC_DPI_DIV_CFG		BIT(26)
#define BIT_DISPC_DPI_DIV_MSK		BIT0_DISPC_DPI_DIV_CFG | BIT1_DISPC_DPI_DIV_CFG | BIT2_DISPC_DPI_DIV_CFG | BIT3_DISPC_DPI_DIV_CFG \
					| BIT4_DISPC_DPI_DIV_CFG | BIT5_DISPC_DPI_DIV_CFG | BIT6_DISPC_DPI_DIV_CFG | BIT7_DISPC_DPI_DIV_CFG


enum{
	DISPC_PLL_SEL_256M = 0,
	DISPC_PLL_SEL_192M,
	DISPC_PLL_SEL_153_6M,
	DISPC_PLL_SEL_128M
};

enum{
	DISPC_DBI_SEL_256M = 0,
	DISPC_DBI_SEL_192M,
	DISPC_DBI_SEL_153_6M,
	DISPC_DBI_SEL_128M
};

enum{
	DISPC_DPI_SEL_384M = 0,
	DISPC_DPI_SEL_192M,
	DISPC_DPI_SEL_153_6M,
	DISPC_DPI_SEL_128M
};

#endif
