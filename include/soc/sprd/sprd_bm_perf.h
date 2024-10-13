/*
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SPRD_BM_H__
#define __SPRD_BM_H__

/**---------------------------------------------------------------------------*
**                               BM  Define                                **
**---------------------------------------------------------------------------*/
#define SPRD_BM_DEBUG
#ifdef SPRD_BM_DEBUG
#define BM_DBG(f,x...)	printk(KERN_DEBUG "BM_DEBUG " f, ##x)
#define BM_INFO(f,x...)	printk(KERN_INFO "BM_INFO " f, ##x)
#define BM_WRN(f,x...)	printk(KERN_WARN "BM_WARN " f, ##x)
#define BM_ERR(f,x...)	printk(KERN_ERR "BM_ERR " f, ##x)
#else
#define BM_DBG(f,x...)
#define BM_INFO(f,x...)
#define BM_WRN(f,x...)
#define BM_ERR(f,x...)	printk(KERN_ERR "BM_ERR " f, ##x)
#endif

/* bm register CHN CFG bit map definition */
#define BM_INT_CLR					BIT(31)
#define BM_PERFOR_INT_MASK			BIT(30)
#define BM_F_DN_INT_MASK			BIT(29)
#define BM_F_UP_INT_MASK			BIT(28)

#define BM_PERFOR_INT_RAW			BIT(26)
#define BM_F_DN_INT_RAW				BIT(25)
#define BM_F_UP_INT_RAW				BIT(24)

#define BM_PERFOR_INT_EN			BIT(22)
#define BM_F_DN_INT_EN				BIT(21)
#define BM_F_UP_INT_EN				BIT(20)

#define BM_PERFOR_REQ_EN			BIT(8)
#define BM_F_DN_REQ_EN				BIT(7)
#define BM_F_UP_REQ_EN				BIT(6)
#define BM_RLATENCY_EN				BIT(5)
#define BM_RBW_EN					BIT(4)
#define BM_WLATENCY_EN				BIT(3)
#define BM_WBW_EN					BIT(2)
#define BM_AUTO_MODE_EN				BIT(1)
#define BM_CHN_EN					BIT(0)

#define SPRD_BM_CHN_REG(base, index)	((unsigned long)base + 0x10000 * index)

/*the buf can store 8 secondes data*/
#define BM_PER_CNT_RECORD_SIZE		800
#define BM_PER_CNT_BUF_SIZE			(64 * 4 * BM_PER_CNT_RECORD_SIZE)
#define BM_LOG_FILE_PATH			"/mnt/obb/axi_per_log"
#define BM_LOG_FILE_SECONDS			(60  * 30)
#define BM_LOG_FILE_MAX_RECORDS		(BM_LOG_FILE_SECONDS * 100)
/* 0x6590 is 1ms */
#define BM_TMR_1_MS					0x6590
#define BM_TMR_H_LEN				0x3F7A0//0x6590 *10 = 0x3f7a0 = 10ms
#define BM_TMR_L_LEN				0x1

/* bm register definition */
struct sprd_bm_reg {
	u32 chn_cfg;//0x0
	u32 peak_win_len;//0x4
	u32 f_dn_rbw_set;//0x8
	u32 f_dn_rl_set;//0xc
	u32 f_dn_wbw_set;//0x10
	u32 f_dn_wl_set;//0x14
	u32 f_up_rbw_set;//0x18
	u32 f_up_rl_set;//0x1c
	u32 f_up_wbw_set;//0x20
	u32 f_up_wl_set;//0x24
	u32 rtrns_in_win;//0x28
	u32 rbw_in_win;//0x2c
	u32 rl_in_win;//0x30
	u32 wtrns_in_win;//0x34
	u32 wbw_in_win;//0x38
	u32 wl_in_win;//0x3c
	u32 peakbm_in_win;//0x40
};

#define BM_TMR2_CNT_CLR				BIT(3)
#define BM_TMR2_EN					BIT(2)
#define BM_TMR1_CNT_CLR				BIT(1)
#define BM_TMR1_EN					BIT(0)

#define BM_CNT_STR2_ST				BIT(2)
#define BM_CNT_STR1_ST				BIT(1)
#define BM_TMR_SEL					BIT(0)

/* bm timer register definition */
struct sprd_bm_tmr_reg {
	u32 tmr_ctrl;//0x0
	u32 high_len_t1;//0x4
	u32 low_len_t1;//0x8
	u32 cnt_num_t1;//0xc
	u32 high_len_t2;//0x10
	u32 low_len_t2;//0x14
	u32 cnt_num_t2;//0x18
	u32 tmr_out_sel;//0x1c
};

/* bm timer triger BM mode definition */
enum sprd_bm_tmr_sel {
	ALL_FROM_TIMER1 = 0x0,
	TRIGER_BY_EACH,
};

/* bm mode definition */
enum sprd_bm_mode {
	BM_CIRCLE_MODE = 0x0,
	BM_NORMAL_MODE,
	BM_FREQ_MODE,
};

/* bm cmd definetion, 0x16 compatible the old BM cmds */
enum sprd_bm_cmd {
	BM_DISABLE = 0x14,
	BM_ENABLE,
	BM_PROF_SET,
	BM_PROF_CLR,
	BM_CHN_CNT,
	BM_RD_CNT,
	BM_WR_CNT,
	BM_RD_BW,
	BM_WR_BW,
	BM_RD_LATENCY,
	BM_WR_LATENCY,
	BM_KERNEL_TIME,
	BM_TMR_CLR,
	BM_VER_GET,
	BM_CMD_MAX,
};

#define BM_NUM_FOR_DDR		10
unsigned char *sprd_bm_name_list[BM_NUM_FOR_DDR] = {
	"CPU",
	"DISP",
	"GPU",
	"AP",
	"CAM",
	"VSP",
	"LWT-DSP",
	"LWT-ACC",
	"AUD-CP",
	"PUB CP",
};
#endif
