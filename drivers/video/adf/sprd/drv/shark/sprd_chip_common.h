/******************************************************************************
 ** File Name:    sprd_chip_common.h                                     *
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
#ifndef __DISPC_CHIP_COM_H_
#define __DISPC_CHIP_COM_H_

#ifdef CONFIG_64BIT
#include <soc/sprd/sci_glb_regs.h>
#include <soc/sprd/sci.h>
#else
#include <soc/sprd/hardware.h>
#include <soc/sprd/globalregs.h>
#include <mach/irqs.h>
#include <soc/sprd/sci.h>
#include <soc/sprd/sci_glb_regs.h>
#endif
#include <linux/io.h>

#if defined(CONFIG_FB_SCX35L)
#include "sprd_chip_9630.h"
#define SPRDFB_SUPPORT_LVDS_PANEL
#elif defined(CONFIG_FB_SCX35) || defined(CONFIG_FB_SCX30G)
#include "sprd_chip_8830.h"
#elif defined(CONFIG_FB_SCX15)
#include "sprd_chip_7715.h"
#else
#error "Unknown architecture specification"
#endif

void dsi_enable(int dev_id);
void dsi_disable(int dev_id);


#endif
