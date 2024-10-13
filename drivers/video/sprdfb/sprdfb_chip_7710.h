/******************************************************************************
 ** File Name:    sprdfb_chip_7710.h                                     *
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
#ifndef _SC7710_DISPC_GLB_REG_K_H_
#define _SC7710_DISPC_GLB_REG_K_H_


#include <asm/arch/sc8810_reg_ahb.h>
#include <asm/arch/sc8810_reg_global.h>

#define BIT(x) (1<<x)

#ifndef CONFIG_OF
#define DISPC_PLL_CLK					("clk_dispc")
#define DISPC_DBI_CLK					("clk_dispc_dbi")
#define DISPC_DPI_CLK					("clk_dispc_dpi")
#define DISPC_DPI_CLOCK (384*1000000/11)
#endif


#define DISPC_AHB_SOFT_RST            	AHB_SOFT2_RST
#define BIT_DISPC_SOFT_RST		 	BIT(2)

#define DISPC_CORE_EN			(AHB_CTL6)
#define BIT_DISPC_CORE_EN		        (BIT(0))

#ifndef CONFIG_OF
#define DISPC_AHB_EN			        (AHB_CTL6)
#define BIT_DISPC_AHB_EN		        (BIT(0))

#define DISPC_EMC_EN			        (AHB_CTL6)
#define BIT_DISPC_EMC_EN		        (BIT(0))

#define DISPC_PLL_SEL_CFG		        AHB_CTL6
#define BITS_DISPC_PLL_SEL_CFG		30
#define BIT0_DISPC_PLL_SEL_CFG		BIT(30)
#define BIT1_DISPC_PLL_SEL_CFG		BIT(31)
#define BIT_DISPC_PLL_SEL_MSK		BIT1_DISPC_PLL_SEL_CFG | BIT0_DISPC_PLL_SEL_CFG

#define DISPC_PLL_DIV_CFG		        AHB_CTL6
#define BITS_DISPC_PLL_DIV_CFG		27
#define BIT0_DISPC_PLL_DIV_CFG		BIT(27)
#define BIT1_DISPC_PLL_DIV_CFG		BIT(28)
#define BIT2_DISPC_PLL_DIV_CFG		BIT(29)
#define BIT_DISPC_PLL_DIV_MSK		BIT0_DISPC_PLL_DIV_CFG | BIT1_DISPC_PLL_DIV_CFG | BIT2_DISPC_PLL_DIV_CFG

#define DISPC_DBI_SEL_CFG		        AHB_CTL6
#define BITS_DISPC_DBI_SEL_CFG		25
#define BIT0_DISPC_DBI_SEL_CFG		BIT(25)
#define BIT1_DISPC_DBI_SEL_CFG		BIT(26)
#define BIT_DISPC_DBI_SEL_MSK		BIT0_DISPC_DBI_SEL_CFG | BIT1_DISPC_DBI_SEL_CFG

#define DISPC_DBI_DIV_CFG			AHB_CTL6
#define BITS_DISPC_DBI_DIV_CFG		22
#define BIT0_DISPC_DBI_DIV_CFG		BIT(22)
#define BIT1_DISPC_DBI_DIV_CFG		BIT(23)
#define BIT2_DISPC_DBI_DIV_CFG		BIT(24)
#define BIT_DISPC_DBI_DIV_MSK		BIT0_DISPC_DBI_DIV_CFG | BIT1_DISPC_DBI_DIV_CFG | BIT2_DISPC_DBI_DIV_CFG

#define DISPC_DPI_SEL_CFG			AHB_CTL6
#define BITS_DISPC_DPI_SEL_CFG		20
#define BIT0_DISPC_DPI_SEL_CFG		BIT(20)
#define BIT1_DISPC_DPI_SEL_CFG		BIT(21)
#define BIT_DISPC_DPI_SEL_MSK		BIT0_DISPC_DPI_SEL_CFG | BIT1_DISPC_DPI_SEL_CFG

#define DISPC_DPI_DIV_CFG			AHB_CTL6
#define BITS_DISPC_DPI_DIV_CFG		12
#define BIT0_DISPC_DPI_DIV_CFG		BIT(12)
#define BIT1_DISPC_DPI_DIV_CFG		BIT(13)
#define BIT2_DISPC_DPI_DIV_CFG		BIT(14)
#define BIT3_DISPC_DPI_DIV_CFG		BIT(15)
#define BIT4_DISPC_DPI_DIV_CFG		BIT(16)
#define BIT5_DISPC_DPI_DIV_CFG		BIT(17)
#define BIT6_DISPC_DPI_DIV_CFG		BIT(18)
#define BIT7_DISPC_DPI_DIV_CFG		BIT(19)
#define BIT_DISPC_DPI_DIV_MSK		BIT0_DISPC_DPI_DIV_CFG | BIT1_DISPC_DPI_DIV_CFG | BIT2_DISPC_DPI_DIV_CFG | BIT3_DISPC_DPI_DIV_CFG \
					| BIT4_DISPC_DPI_DIV_CFG | BIT5_DISPC_DPI_DIV_CFG | BIT6_DISPC_DPI_DIV_CFG | BIT7_DISPC_DPI_DIV_CFG


enum{
	DISPC_PLL_SEL_312M = 0,
	DISPC_PLL_SEL_256M,
	DISPC_PLL_SEL_192M,
	DISPC_PLL_SEL_96M
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


void dispc_print_clk(void);






#endif
