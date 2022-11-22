/******************************************************************************
 ** File Name:    sprdfb_chip_7715.c                                     *
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


#include "sprdfb_chip_7715.h"
#include "sprdfb_chip_common.h"


void dispc_print_clk(void)
{
    uint32_t reg_val0,reg_val1,reg_val2;
    reg_val0 = dispc_glb_read(SPRD_AONAPB_BASE+0x4);
    reg_val1 = dispc_glb_read(SPRD_AHB_BASE);
    reg_val2 = dispc_glb_read(SPRD_APBREG_BASE);
    printk("sprdfb: 0x402e0004 = 0x%x 0x20d00000 = 0x%x 0x71300000 = 0x%x \n",reg_val0, reg_val1, reg_val2);
    reg_val0 = dispc_glb_read(SPRD_APBCKG_BASE+0x34);
    reg_val1 = dispc_glb_read(SPRD_APBCKG_BASE+0x30);
    reg_val2 = dispc_glb_read(SPRD_APBCKG_BASE+0x2c);
    printk("sprdfb: 0x71200034 = 0x%x 0x71200030 = 0x%x 0x7120002c = 0x%x \n",reg_val0, reg_val1, reg_val2);
}




