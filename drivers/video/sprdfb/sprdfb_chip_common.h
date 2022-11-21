/******************************************************************************
 ** File Name:    sprdfb_chip_common.h                                     *
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
#include "sprdfb_chip_9630.h"
#define SPRDFB_SUPPORT_LVDS_PANEL
#elif defined(CONFIG_FB_SCX35) || defined(CONFIG_FB_SCX30G)
#include "sprdfb_chip_8830.h"
#elif defined(CONFIG_FB_SCX15)
#include "sprdfb_chip_7715.h"
#else
#error "Unknown architecture specification"
#endif


//void __raw_bits_set_value(unsigned int reg, unsigned int value, unsigned int bit, unsigned int mask);

void dispc_pll_clk_set(unsigned int clk_src, unsigned int clk_div);

void dispc_dbi_clk_set(unsigned int clk_src, unsigned int clk_div);

void dispc_dpi_clk_set(unsigned int clk_src, unsigned int clk_div);

void dsi_enable(void);
void dsi_disable(void);

u32 dispc_glb_read(unsigned long reg);

void dispc_print_clk(void);
//void dsi_print_global_config(void);


#endif
