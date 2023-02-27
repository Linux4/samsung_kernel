/******************************************************************************
 ** File Name:    sprdfb_chip_7710.c                                     *
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


#include "sprdfb_chip_7710.h"
#include "sprdfb_chip_common.h"

void dispc_print_clk(void)
{
	printf("sprdfb: 0x20900220 = 0x%x\n", __raw_readl(SPRD_AHB_BASE + 0x23c));
}



