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

#define AXQOS_BYPASS	8
#define AXQOS_VAL	12
#define AXQOS_ONOFF	0
#define BLOCK_UPPER	0
#define BLOCK_UPPER0	0
#define BLOCK_UPPER1	16
#define TIMEOUT_CNT_R	0
#define TIMEOUT_CNT_W	16
#define QURGENT_EN	20
#define BLOCKING_EN	0

#define WRITE_FLUSH_CONFIG_0	0x34
#define WRITE_FLUSH_CONFIG_1	0x38
#define QOS_TIMER_0	0x300
#define QOS_TIMER_1	0x304
#define QOS_TIMER_2	0x308
#define QOS_TIMER_3	0x30C
#define QOS_TIMER_4	0x310
#define QOS_TIMER_5	0x314
#define QOS_TIMER_6	0x318
#define QOS_TIMER_7	0x31C
#define QOS_TIMER_8	0x320
#define QOS_TIMER_9	0x324
#define QOS_TIMER_10	0x328
#define QOS_TIMER_11	0x32C
#define QOS_TIMER_12	0x330
#define QOS_TIMER_13	0x334
#define QOS_TIMER_14	0x338
#define QOS_TIMER_15	0x33C
#define VC_TIMER_TH_0	0x340
#define VC_TIMER_TH_1	0x344
#define VC_TIMER_TH_2	0x348
#define VC_TIMER_TH_3	0x34C
#define VC_TIMER_TH_4	0x350
#define VC_TIMER_TH_5	0x354
#define VC_TIMER_TH_6	0x358
#define VC_TIMER_TH_7	0x35C
#define CUTOFF_CON	0x370
#define BRB_CUTOFF_CON	0x374

#define RREQ_THRT_CON	0x02C
#define RREQ_THRT_MO_P2	0x044
#define PF_QOS_TIMER_0	0x070
#define PF_QOS_TIMER_1	0x074
#define PF_QOS_TIMER_2	0x078
#define PF_QOS_TIMER_3	0x07C
#define PF_QOS_TIMER_4	0x080
#define PF_QOS_TIMER_5	0x084
#define PF_QOS_TIMER_6	0x088
#define PF_QOS_TIMER_7	0x08C

#define DEFAULT_QMAX_RD_TH	0x41
#define DEFAULT_QMAX_WR_TH	0xF

#define QMAX_THRESHOLD_R	0x0050
#define QMAX_THRESHOLD_W	0x0054
