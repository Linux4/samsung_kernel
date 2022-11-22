/******************************************************************************
 ** File Name:    sprdfb_chip_common.c                                     *
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


#include <linux/kernel.h>
#include "sprdfb_chip_common.h"

u32 dispc_glb_read(unsigned long reg)
{
	return sci_glb_read(reg, 0xffffffff);
}

