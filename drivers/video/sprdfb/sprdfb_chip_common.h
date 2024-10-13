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

#if defined(CONFIG_FB_SCX35) || defined(CONFIG_FB_SCX30G)
#include "sprdfb_chip_8830.h"
#elif defined(CONFIG_FB_SCX15)
#include "sprdfb_chip_7715.h"
#elif defined(CONFIG_FB_SC8825)
#include "sprdfb_chip_8825.h"
#elif defined(CONFIG_FB_SC7710)
#include "sprdfb_chip_7710.h"
#else
#error "Unknown architecture specification"
#endif

#include <mach/hardware.h>
#include <mach/globalregs.h>
#include <linux/io.h>
#include <mach/irqs.h>
#include <mach/sci.h>


typedef struct _trick_item_
{
	volatile uint32_t trick_en;// trick enable flag
	volatile uint32_t interval;// this interruption should be re-enable after interval time later, time count by jiffies
	volatile uint32_t begin_jiffies;// this variable recorrd the jiffies last time the interrupt occur
	volatile uint32_t disable_cnt;
	volatile uint32_t enable_cnt;
}
Trick_Item;

void __raw_bits_set_value(unsigned int reg, unsigned int value, unsigned int bit, unsigned int mask);

void dispc_pll_clk_set(unsigned int clk_src, unsigned int clk_div);

void dispc_dbi_clk_set(unsigned int clk_src, unsigned int clk_div);

void dispc_dpi_clk_set(unsigned int clk_src, unsigned int clk_div);

u32 dispc_glb_read(u32 reg);

void dsi_enable(void);
void dsi_disable(void);

void dispc_print_clk(void);


#endif
