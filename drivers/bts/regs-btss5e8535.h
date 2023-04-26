/*
 * Copyright (c) 2018 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License Version 2 as publised
 * by the Free Software Foundation.
 *
 * Header for BTS Bus Traffic Shaper
 *
 * Includes Register information to control BTS devices
 *
 */

#define CON		0x000
#define TIMEOUT		0x010
#define RCON		0x020
#define RBLK_UPPER	0x024
#define RBLK_UPPER_FULL	0x02C
#define RBLK_UPPER_BUSY	0x030
#define RBLK_UPPER_MAX	0x034
#define WCON		0x040
#define WBLK_UPPER	0x044
#define WBLK_UPPER_FULL	0x04C
#define WBLK_UPPER_BUSY	0x050
#define WBLK_UPPER_MAX	0x054
#define CORE_QOS_EN	0x4
#define TIMEOUT_R0	0x008
#define TIMEOUT_R1	0x00C
#define TIMEOUT_W0	0x010
#define TIMEOUT_W1	0x014

#define AXQOS_BYPASS	8
#define AXQOS_VAL	12

#define SCIQOS_EN	0
#define SCIQOS_R	2
#define SCIQOS_W	0

#define AXQOS_ONOFF	0
#define BLOCK_UPPER	0
#define BLOCK_UPPER0	0
#define BLOCK_UPPER1	16
#define TIMEOUT_CNT_R	0
#define TIMEOUT_CNT_W	16
#define QURGENT_EN	20
#define BLOCKING_EN	0
#define TIMEOUT_CNT_VC0	0
#define TIMEOUT_CNT_VC1	8
#define TIMEOUT_CNT_VC2	16
#define TIMEOUT_CNT_VC3	24

#define RMO_PORT_0	0
#define RMO_PORT_1	16
#define WMO_PORT_0	8
#define WMO_PORT_1	24

#define SCI_CTRL	0x0000
#define CRP_CTL3_0	0x10
#define CRP_CTL3_1	0x38
#define CRP_CTL3_2	0x60
#define CRP_CTL3_3	0x88
#define TH_IMM_R_0	0x010
#define TH_IMM_W_0	0x018
#define TH_HIGH_R_0	0x020
#define TH_HIGH_W_0	0x028

#define CRP0_P0_CTRL	0x88
#define CRP1_P0_CTRL	0xA8
#define CRP2_P0_CTRL	0xC8
#define CRP3_P0_CTRL	0xE8

#define CRP0_P1_CTRL	0x8C
#define CRP1_P1_CTRL	0xAC
#define CRP2_P1_CTRL	0xCC
#define CRP3_P1_CTRL	0xEC

#define HIGH_THRESHOLD_SHIFT	24
#define MID_THRESHOLD_SHIFT	16

/* VC mapping offset base+0x3000 */
#define VC_MAP_RD_MEM0_S0	0x0000
#define VC_MAP_WR_MEM0_S0	0x0004
#define VC_MAP_RD_MEM0_S1	0x0008
#define VC_MAP_WR_MEM0_S1	0x000C
#define VC_MAP_RD_MEM0_S2	0x0010
#define VC_MAP_WR_MEM0_S2	0x0014
#define VC_MAP_RD_MEM0_S3	0x0018
#define VC_MAP_WR_MEM0_S3	0x001C

#define VC_MAP_RD_MEM1_S0	0x0020
#define VC_MAP_WR_MEM1_S0	0x0024
#define VC_MAP_RD_MEM1_S1	0x0028
#define VC_MAP_WR_MEM1_S1	0x002C
#define VC_MAP_RD_MEM1_S2	0x0030
#define VC_MAP_WR_MEM1_S2	0x0034
#define VC_MAP_RD_MEM1_S3	0x0038
#define VC_MAP_WR_MEM1_S3	0x003C

#define VC_MAP_RD_NOCL0_CAM_RT_S0	0x0040
#define VC_MAP_WR_NOCL0_CAM_RT_S0	0x0044
#define VC_MAP_RD_NOCL0_CAM_RT_S1	0x0048
#define VC_MAP_WR_NOCL0_CAM_RT_S1	0x004C
#define VC_MAP_RD_NOCL0_CAM_RT_S2	0x0050
#define VC_MAP_WR_NOCL0_CAM_RT_S2	0x0054
#define VC_MAP_RD_NOCL0_CAM_RT_S3	0x0058
#define VC_MAP_WR_NOCL0_CAM_RT_S3	0x005C

#define VC_MAP_RD_NOCL0_CAM_NRT_S0	0x0060
#define VC_MAP_WR_NOCL0_CAM_NRT_S0	0x0064
#define VC_MAP_RD_NOCL0_CAM_NRT_S1	0x0068
#define VC_MAP_WR_NOCL0_CAM_NRT_S1	0x006C
#define VC_MAP_RD_NOCL0_CAM_NRT_S2	0x0070
#define VC_MAP_WR_NOCL0_CAM_NRT_S2	0x0074
#define VC_MAP_RD_NOCL0_CAM_NRT_S3	0x0078
#define VC_MAP_WR_NOCL0_CAM_NRT_S3	0x007C
#define VC_MAP_RD_NOCL0_CAM_NRT_S4	0x0080
#define VC_MAP_WR_NOCL0_CAM_NRT_S4	0x0084
#define VC_MAP_RD_NOCL0_CAM_NRT_S5	0x0088
#define VC_MAP_WR_NOCL0_CAM_NRT_S5	0x008C

#define VC_MAP_RD_NOCL1A_M0_S0	0x0090
#define VC_MAP_WR_NOCL1A_M0_S0	0x0094

#define VC_MAP_RD_NOCL0_SW0_S0	0x0098
#define VC_MAP_WR_NOCL0_SW0_S0	0x009C
#define VC_MAP_RD_NOCL0_SW0_S1	0x00A0
#define VC_MAP_WR_NOCL0_SW0_S1	0x00A4
#define VC_MAP_RD_NOCL0_SW0_S2	0x00A8
#define VC_MAP_WR_NOCL0_SW0_S2	0x00AC
#define VC_MAP_RD_NOCL0_SW0_S3	0x00B0
#define VC_MAP_WR_NOCL0_SW0_S3	0x00B4
#define VC_MAP_RD_NOCL0_SW0_S4	0x00B8
#define VC_MAP_WR_NOCL0_SW0_S4	0x00BC
#define VC_MAP_RD_NOCL0_SW0_S5	0x00C0
#define VC_MAP_WR_NOCL0_SW0_S5	0x00C4
#define VC_MAP_RD_NOCL0_SW0_S6	0x00C8
#define VC_MAP_WR_NOCL0_SW0_S6	0x00CC

#define VC_MAP_RD_NOCL0_DP_S0	0x00D0
#define VC_MAP_WR_NOCL0_DP_S0	0x00D4
#define VC_MAP_RD_NOCL0_DP_S1	0x00D8
#define VC_MAP_WR_NOCL0_DP_S1	0x00DC
#define VC_MAP_RD_NOCL0_DP_S2	0x00E0
#define VC_MAP_WR_NOCL0_DP_S2	0x00E4
#define VC_MAP_RD_NOCL0_DP_S3	0x00E8
#define VC_MAP_WR_NOCL0_DP_S3	0x00EC
