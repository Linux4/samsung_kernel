/******************************************************************************
 ** File Name:    sprdfb_chip_whale.h                                *
 ** Author:       junxiao.feng junxiao.feng@spreadtrum.com          *
 ** DATE:         2015-05-05                                        *
 ** Copyright:    2015 Spreatrum, Incoporated. All Rights Reserved. *
 ** Description:                                                    *
 ******************************************************************************/
#ifndef _SC9001_DISPC_GLB_REG_K_H_
#define _SC9001_DISPC_GLB_REG_K_H_

#include <linux/kernel.h>
#include <soc/sprd/sci_glb_regs.h>
#include <linux/delay.h>


#define BIT_DISP_EB			(BIT(30))
#define	SPRD_MIPI_DPHY_GEN2
#define SPRD_MIPI_DSIC_BASE 		g_dsi_base_addr
#define SPRD_LCDC_BASE 			g_dispc_base_addr

#define DSI_AHB_SOFT_RST           		REG_DISP_AHB_AHB_RST
#define BIT_DSI0_SOFT_RST	 		(BIT_DISP_AHB_DSI0_SOFT_RST)
#define BIT_DSI1_SOFT_RST	 		(BIT_DISP_AHB_DSI1_SOFT_RST)

#define DSI_REG_EB				REG_DISP_AHB_AHB_EB
#define DSI_BIT_EB				BIT_DISP_AHB_DSI1_EB

#define REG_AHB_SOFT_RST 			(0x4 + REG_DISP_AHB_AHB_EB)

#define BIT_DISPC0_SOFT_RST			BIT(0)
#define BIT_DISPC1_SOFT_RST			BIT(1)

#define DISPC_CORE_EN			(REG_DISP_AHB_AHB_EB)
#define BIT_DISPC0_CORE_EN			(BIT_DISP_AHB_DISPC0_EB)
#define BIT_DISPC1_CORE_EN			(BIT_DISP_AHB_DISPC1_EB)

/*for noc*/
typedef enum {
	DDR_8G = 0,
	DDR_1G,
	DDR_1_5G,
	DDR_2G,
	DDR_3G,
	DDR_4G
} DDR_CAPACITY;

typedef enum {
	NORMAL_MODE = 0,
	FULL_MODE,
	MIXED_MODE
} INTERLEAVE_MODE;

typedef enum {
	SIZE_64 = 0,
	SIZE_128
} INTERLEAVE_SEL;
#define DDR_CAPACITY_OFFSET	9
#define DDR_CAPACITY_MASK	(0x7 << DDR_CAPACITY_OFFSET)

#define INTERLEAVE_MODE_OFFSET	1
#define INTERLEAVE_MODE_MASK	(0x3 << INTERLEAVE_MODE_OFFSET)

#define INTERLEAVE_SEL_OFFSET	0
#define INTERLEAVE_SEL_MASK	(0x1 << INTERLEAVE_SEL_OFFSET)
void misc_noc_ctrl(unsigned long base, unsigned int mode, unsigned int sel);
#endif
