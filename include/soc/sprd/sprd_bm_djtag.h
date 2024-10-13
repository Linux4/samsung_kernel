/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
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

#ifndef __SPRD_DJTAG_H__
#define __SPRD_DJTAG_H__

/**---------------------------------------------------------------------------*
**                               DJTAG  Define                                **
**---------------------------------------------------------------------------*/
//#define SPRD_DJTAG_DEBUG
#ifdef SPRD_DJTAG_DEBUG
#define DJTAG_DBG(f,x...)	printk(KERN_DEBUG "DJTAG_DEBUG " f, ##x)
#define DJTAG_INFO(f,x...)	printk(KERN_INFO "DJTAG_INFO " f, ##x)
#define DJTAG_WRN(f,x...)	printk(KERN_WARN "DJTAG_WARN " f, ##x)
#define DJTAG_ERR(f,x...)	printk(KERN_ERR "DJTAG_ERR " f, ##x)
#else
#define DJTAG_DBG(f,x...)
#define DJTAG_INFO(f,x...)
#define DJTAG_WRN(f,x...)
#define DJTAG_ERR(f,x...)	printk(KERN_ERR "DJTAG_ERR " f, ##x)
#endif

#define BIT_DJTAG_PUB0_SOFTRESET				(BIT(17))
#define BIT_DJTAG_AON_SOFTRESET					(BIT(16))
#define BIT_DJTAG_AGCP_SOFTRESET				(BIT(10))
#define BIT_DJTAG_WTLCP_SOFTRESET				(BIT(9))
#define BIT_DJTAG_PUBCP_SOFTRESET				(BIT(8))
#define BIT_DJTAG_DISP_SOFTRESET				(BIT(5))
#define BIT_DJTAG_CAM_SOFTRESET					(BIT(4))
#define BIT_DJTAG_VSP_SOFTRESET					(BIT(3))
#define BIT_DJTAG_GPU_SOFTRESET					(BIT(2))
#define BIT_DJTAG_CA53_SOFTRESET				(BIT(1))
#define BIT_DJTAG_AP_SOFTRESET					(BIT(0))

#define BIT_DJTAG_CHAIN_UPDATE					(BIT(8))

#define DJTAG_SOFTRESET_MASK					0x3073F
#define DJTAG_IR_LEN							0x9
#define DJTAG_DR_LEN							0x20
#define DJTAG_DAP_OFFSET						0x8
#define DJTAG_AUTO_SCAN_IR						0x200
#define DJTAG_DAP_MUX_RESET						0x108

#define SPRD_DJTAG_SUCCESS						0x0
#define DJTAG_MAX_AUTOSCAN_CHANS				16

/* djtag register definition */
struct sprd_djtag_reg {
	volatile u32 ir_len_cfg;//0x0
	volatile u32 dr_len_cfg;//0x4
	volatile u32 ir_cfg;//0x8
	volatile u32 dr_cfg;//0xc
	volatile u32 dr_pause_recov_cfg;//0x10
	volatile u32 rnd_en_cfg;//0x14
	volatile u32 upd_dr_cfg;//0x18
	volatile u32 autoscan_chain_addr[16];//0x1c-0x58
	volatile u32 autoscan_chain_pattern[16];//0x5c-0x98
	volatile u32 autoscan_chain_data[16];//0x9c-0xd8
	volatile u32 autoscan_chain_mask[16];//0xdc-0x118
	volatile u32 autoscan_en;//0x11c
	volatile u32 autoscan_int_raw;//0x120
	volatile u32 autoscan_int_mask;//0x124
	volatile u32 autoscan_int_en;//0x128    -tmp:0x124
	volatile u32 autoscan_int_clr;//0x12c	-tmp:0x128
	volatile u32 dap_mux_ctrl_rst;//0x130 -tmp:0x12c
};

struct djtag_autoscan_set {
	 u32 autoscan_chain_addr;
	 u32 autoscan_chain_pattern;
	 u32 autoscan_chain_mask;
	 bool scan;
};

struct jtag_autoscan_info {
	u32 autoscan_chain_addr;
	u32 autoscan_chain_pattern;
	u32 autoscan_chain_data;
	u32 autoscan_chain_mask;
	bool occurred;
};

/**---------------------------------------------------------------------------*
**                               DJTAG Bus Monitor Reg Define                                **
**---------------------------------------------------------------------------*/

#define BM_INT_MASK_STATUS			BIT(31)
#define BM_INT_RAW_STATUS			BIT(30)
#define BM_INT_CLR					BIT(29)
#define BM_INT_EN					BIT(28)
#define BM_BUF_CLR					BIT(27)
#define BM_RD_WR_SEL				BIT(26)
#define BM_BUF_RD_EN				BIT(25)
#define BM_BUF_EN 					BIT(24)

#define BM_CNT_CLR					BIT(4)
#define BM_CNT_INTERNAL_START		BIT(3)
#define BM_CNT_EN					BIT(1)
#define BM_CHN_EN					BIT(0)

#define BM_AXI_SIZE_DWORD			(BIT(21) | BIT(22))
#define BM_AXI_SIZE_WORD			BIT(22)
#define BM_AXI_SIZE_HWORD			BIT(21)
#define BM_AXI_SIZE_EN				BIT(20)
#define BM_WRITE_CFG				BIT(1)
#define BM_WRITE_EN					BIT(0)

#define BM_NUM_AP					14
#define BM_NUM_CA53					2
#define BM_NUM_PUBCP				8
#define BM_NUM_WTLCP				49
#define BM_NUM_AGCP					6
#define BM_NUM_AON					5
#define BM_DEV_ID_MAX 				(BM_NUM_AP + BM_NUM_CA53 + BM_NUM_PUBCP + BM_NUM_WTLCP + BM_NUM_AGCP + BM_NUM_AON)
#define BM_DEBUG_ALL_CHANNEL		0xFF
#define BM_CONTINUE_DEBUG_SIZE		20

#define AUTOSCAN_ADDRESS(SUBSYS,DAP,CHAIN)  ((SUBSYS<<16)|(DAP<<8)|(CHAIN))

typedef enum _ahb_busmon_djtag_ir{
	AHB_CHN_INT = 8,
	AHB_CHN_CFG,
	AHB_ADDR_MIN,
	AHB_ADDR_MAX,
	AHB_ADDR_MASK,
	AHB_DATA_MIN_L32,
	AHB_DATA_MIN_H32,
	AHB_DATA_MAX_L32,
	AHB_DATA_MAX_H32,
	AHB_DATA_MASK_L32,
	AHB_DATA_MASK_H32,
	AHB_CNT_WIN_LEN,
	AHB_PEAK_WIN_LEN,
	AHB_MATCH_ADDR,
	AHB_MATCH_CMD,
	AHB_MATCH_DATA_L32,
	AHB_MATCH_DATA_H32,
	AHB_RTRANS_IN_WIN,
	AHB_RBW_IN_WIN,
	AHB_RLATENCE_IN_WIN,
	AHB_WTRANS_IN_WIN,
	AHB_WBW_IN_WIN,
	AHB_WLATENCE_IN_WIN,
	AHB_PEAKBW_IN_WIN,
	AHB_RESERVED,
	AHB_MON_TRANS_LEN,
	AHB_BUS_PEEK,
	AHB_ADDR_MIN_H32,
	AHB_ADDR_MAX_H32,
	AHB_ADDR_MASK_H32,
	AHB_MATCH_ADDR_H32,
}ahb_busmon_djtag_ir;

typedef enum _axi_busmon_djtag_ir{
	AXI_CHN_INT = 8,
	AXI_CHN_CFG,
	AXI_ID_CFG,
	AXI_ADDR_MIN,
	AXI_ADDR_MIN_H32,
	AXI_ADDR_MAX,
	AXI_ADDR_MAX_H32,
	AXI_ADDR_MASK,
	AXI_ADDR_MASK_H32,
	AXI_DATA_MIN_L32,
	AXI_DATA_MIN_H32,
	AXI_DATA_MIN_EXT_L32,
	AXI_DATA_MIN_EXT_H32,
	AXI_DATA_MAX_L32,
	AXI_DATA_MAX_H32,
	AXI_DATA_MAX_EXT_L32,
	AXI_DATA_MAX_EXT_H32,
	AXI_DATA_MASK_L32,
	AXI_DATA_MASK_H32,
	AXI_DATA_MASK_EXT_L32,
	AXI_DATA_MASK_EXT_H32,
	AXI_MON_TRANS_LEN,
	AXI_MATCH_ID,
	AXI_MATCH_ADDR,
	AXI_MATCH_ADDR_H32,
	AXI_MATCH_CMD,
	AXI_MATCH_DATA_L32,
	AXI_MATCH_DATA_H32,
	AXI_MATCH_DATA_EXT_L32,
	AXI_MATCH_DATA_EXT_H32,
	AXI_BUS_STATUS,
}axi_busmon_djtag_ir;

typedef enum _subsys_mux{
	MUX_AP=0,
	MUX_CA53,
	MUX_GPU,
	MUX_VSP,
	MUX_CAM,
	MUX_DISP,
	MUX_PUBCP,
	MUX_WTLCP,
	MUX_AGCP,
	MUX_PUB0,
	MUX_PUB1,
	MUX_AON,
	MUX_DAP_MAX,
}subsys_mux;

typedef enum _dap_mux{
	DAP0=0,
	DAP1,
	DAP2,
	DAP3,
	DAP4,
	DAP5,
	DAP6,
	DAP7,
	DAP8,
	DAP9,
	DAP10,
	DAP11,
	DAP12,
	DAP13,
	DAP14,
	//DAPN
	DAP_MUX,
}dap_mux;

typedef enum _chain_ir{
	CHAN_BYPASS=0,
	CHAN_IDCODE=1,
	CHAN_USERCODE=2,
	//IR3-7 reserved
	CHAN_0=8,
	CHAN_1,
	CHAN_2,
	CHAN_3,
	CHAN_4,
	CHAN_5,
	CHAN_6,
	CHAN_7,
	CHAN_8,
	CHAN_9,
	CHAN_10,
	CHAN_11,
	CHAN_12,
	CHAN_13,
	CHAN_14,
	CHAN_15,
	CHAN_16,
	CHAN_17,
	CHAN_18,
	CHAN_19,
	CHAN_20,
	CHAN_21,
	CHAN_22,
	CHAN_23,
	CHAN_24,
	CHAN_25,
	CHAN_26,
	CHAN_27,
	CHAN_28,
	CHAN_29,
	CHAN_30,
}chan_ir;

typedef enum
{
	BM_DAP_0 = 0,
	BM_DAP_1,
	BM_DAP_2,
	BM_DAP_3,
	BM_DAP_4,
	BM_DAP_5,
	BM_DAP_6,
	BM_DAP_7,
	BM_DAP_8,
	BM_DAP_9,
	BM_DAP_10,
	BM_DAP_11,
	BM_DAP_12,
	BM_DAP_13,
	BM_DAP_14,
	BM_DAP_15,
	BM_DAP_16,
	BM_DAP_17,
	BM_DAP_18,
	BM_DAP_19,
	BM_DAP_20,
	BM_DAP_21,
	BM_DAP_22,
	BM_DAP_23,
	BM_DAP_24,
	BM_DAP_25,
	BM_DAP_26,
	BM_DAP_27,
	BM_DAP_28,
	BM_DAP_29,
	BM_DAP_30,
	BM_DAP_31,
	BM_DAP_32,
	BM_DAP_33,
	BM_DAP_34,
	BM_DAP_35,
	BM_DAP_36,
	BM_DAP_37,
	BM_DAP_38,
	BM_DAP_39,
	BM_DAP_40,
	BM_DAP_41,
	BM_DAP_42,
	BM_DAP_43,
	BM_DAP_44,
	BM_DAP_45,
	BM_DAP_46,
	BM_DAP_47,
	BM_DAP_48,
	BM_DAP_49,
	BM_DAP_50,
	BM_DAP_51,
	BM_DAP_52,
	BM_DAP_53,
	BM_DAP_54,
	BM_DAP_MAX = 0xff
} BM_DAP_ID;

typedef enum
{
	AHB_CHN_0 = 0,
	AHB_CHN_1,
	AHB_CHN_2,
	AHB_CHN_3,
	NO_CHN = 0xff
} AHB_CHN_SEL_ID;

typedef enum
{
	BM_CHN_0 = 0,
	BM_CHN_1,
	BM_CHN_2,
	BM_CHN_3,
	BM_CHN_4,
	BM_CHN_5,
	BM_CHN_6,
	BM_CHN_7,
	BM_CHN_8,
	BM_CHN_9,
	BM_CHN_10,
	BM_CHN_11,
	BM_CHN_12,
	BM_CHN_13,
	BM_CHN_14,
	BM_CHN_15,
	BM_CHN_16,
	BM_CHN_17,
	BM_CHN_18,
	BM_CHN_19,
	BM_CHN_20,
	BM_CHN_21,
	BM_CHN_22,
	BM_CHN_23,
	BM_CHN_24,
	BM_CHN_25,
	BM_CHN_26,
	BM_CHN_27,
	BM_CHN_28,
	BM_CHN_29,
	BM_CHN_30,
	BM_CHN_31,
	BM_CHN_32,
	BM_CHN_33,
	BM_CHN_34,
	BM_CHN_35,
	BM_CHN_36,
	BM_CHN_37,
	BM_CHN_38,
	BM_CHN_39,
	BM_CHN_40,
	BM_CHN_41,
	BM_CHN_42,
	BM_CHN_43,
	BM_CHN_44,
	BM_CHN_45,
	BM_CHN_46,
	BM_CHN_47,
	BM_CHN_48,
	BM_CHN_49,
	BM_CHN_50,
	BM_CHN_51,
	BM_CHN_52,
	BM_CHN_53,
	BM_CHN_MAX
} BM_CHN_ID;

typedef enum
{
	AHB_BM = 0,
	AXI_BM,
}BM_TYPE;

typedef enum
{
	MUX_AP_BM=0,
	MUX_CA53_BM,
	MUX_GPU_BM,
	MUX_VSP_BM,
	MUX_CAM_BM,
	MUX_DISP_BM,
	MUX_PUBCP_BM,
	MUX_WTLCP_BM,
	MUX_AGCP_BM,
	MUX_PUB0_BM,
	MUX_PUB1_BM,
	MUX_AON_BM,
	MUX_DAP_MUX_BM,
}BM_ARCH;

typedef enum
{
	BM_READ = 0,
	BM_WRITE,
	BM_READWRITE,
}BM_RW;

typedef struct _BM_MATCH_SETTING
{
	BM_TYPE bm_type;
	BM_CHN_ID bm_id;
	AHB_CHN_SEL_ID chn_id;
	BM_RW rw_cfg;

	volatile u32 bm_addr_min_l32;
	volatile u32 bm_addr_min_h32;
	volatile u32 bm_addr_max_l32;
	volatile u32 bm_addr_max_h32;
	volatile u32 bm_addr_mask_l32;
	volatile u32 bm_addr_mask_h32;
} BM_MATCH_SETTING;

typedef struct _BM_MATCH_SET_REG
{
	volatile u32 bm_chn_cfg;
	volatile u32 bm_addr_min;
	volatile u32 bm_addr_max;
	volatile u32 bm_addr_mask;
	volatile u32 bm_data_min_l32;
	volatile u32 bm_data_min_h32;
	volatile u32 bm_data_max_l32;
	volatile u32 bm_data_max_h32;
	volatile u32 bm_data_mask_l32;
	volatile u32 bm_data_mask_h32;
}BM_MATCH_SET_REG;

typedef struct _AXI_BM_MATCH_SET_REG
{
	volatile u32 bm_chn_cfg;
	volatile u32 bm_id_cfg;
	volatile u32 bm_addr_min_l32;
	volatile u32 bm_addr_min_h32;
	volatile u32 bm_addr_max_l32;
	volatile u32 bm_addr_max_h32;
	volatile u32 bm_addr_mask_l32;
	volatile u32 bm_addr_mask_h32;
	volatile u32 bm_data_min_l32;
	volatile u32 bm_data_min_h32;
	volatile u32 bm_data_min_ext_l32;
	volatile u32 bm_data_min_ext_h32;
	volatile u32 bm_data_max_l32;
	volatile u32 bm_data_max_h32;
	volatile u32 bm_data_max_ext_l32;
	volatile u32 bm_data_max_ext_h32;
	volatile u32 bm_data_mask_l32;
	volatile u32 bm_data_mask_h32;
	volatile u32 bm_data_mask_ext_l32;
	volatile u32 bm_data_mask_ext_h32;
}AXI_BM_MATCH_SET_REG;

typedef struct _BM_PERFORM_SET_REG
{
	volatile u32 bm_cnt_win_len;
	volatile u32 bm_peak_win_len;
}BM_PERFORM_SET_REG;

typedef struct _BM_MATCH_READ_REG
{
	volatile u32 bm_match_addr;
	volatile u32 bm_match_cmd;
	volatile u32 bm_match_data_l32;
	volatile u32 bm_match_data_h32;
}BM_MATCH_READ_REG;

typedef struct _BM_PERFORM_READ_REG
{
	volatile u32 bm_rtrans_in_win;
	volatile u32 bm_rbw_in_win;
	volatile u32 bm_rlatence_in_win;
	volatile u32 bm_wtrans_in_win;
	volatile u32 bm_wbw_in_win;
	volatile u32 bm_wlatence_in_win;
	volatile u32 bm_peakbw_in_win;
}BM_PERFORM_READ_REG;

typedef struct _AHB_BM_REG
{
	volatile u32	ahb_bm_chn_int;
	BM_MATCH_SET_REG	ahb_bm_match_set_reg;
	BM_PERFORM_SET_REG	ahb_bm_perform_set_reg;
	BM_MATCH_READ_REG	ahb_bm_match_read_reg;
	BM_PERFORM_READ_REG 	ahb_bm_perform_read_reg;
	volatile u32	resv1;
	volatile u32	ahb_bm_trans_len;
	volatile u32	ahb_bm_bus_peek;
	volatile u32	ahb_bm_addr_minh;
	volatile u32	ahb_bm_addr_maxh;
	volatile u32	ahb_bm_addr_maskh;
	volatile u32	ahb_bm_match_h;
} AHB_BM_REG;

typedef struct _AXI_BM_REG
{
	volatile u32	axi_bm_chn_int;
	AXI_BM_MATCH_SET_REG	axi_bm_match_set_reg;
	volatile u32	axi_bm_trans_len;
	volatile u32	axi_bm_match_id;
	volatile u32	axi_bm_match_addr_l32;
	volatile u32	axi_bm_match_addr_h32;
	volatile u32	axi_bm_cmd;
	volatile u32	axi_bm_match_data_l32;
	volatile u32	axi_bm_match_data_h32;
	volatile u32	axi_bm_match_data_ext_l32;
	volatile u32	axi_bm_match_data_ext_h32;
	volatile u32	axi_bm_bus_status;
} AXI_BM_REG;

/**---------------------------------------------------------------------------*
**                               DJTAG Chains Define                         **
**---------------------------------------------------------------------------*/

unsigned char djtag_chain_info[]={
"AP scan chain, subsystem num:0\n\
	DAP0: [chain0]AP module enable status; [chain1]AP sleep status; [chain2]AP busmonitor int; [chain3]AP EMA control\n\
	DAP1:channel[0]-->CA53 AXI busmonitor\n\
	DAP2: channel[1]-->DMA AXI busmonitor\n\
	DAP3: channel[2]-->SDIO0 AXI busmonitor\n\
	DAP4: channel[3]-->SDIO1 AXI busmonitor\n\
	DAP5: channel[4]-->SDIO2AXI busmonitor\n\
	DAP6: channel[5]-->eMMC AXI busmonitor\n\
	DAP7: channel[6]-->NANDC AXI busmonitor\n\
	DAP8: channel[7]-->OTG AHB busmonitor\n\
	DAP9: channel[8]-->USB3 AXI busmonitor\n\
	DAP10: channel[9]-->HSIC AHB busmonitor\n\
	DAP11: channel[10]-->ZIPENC AXI busmonitor\n\
	DAP12: channel[11]-->ZIPDEC AXI busmonitor\n\
	DAP13: channel[12]-->CC63P AXI busmonitor\n\
	DAP14: channel[13]-->CC63S AXI busmonitor\n\n\
CA53 scan chain, subsystem num:1\n\
	DAP0:[chain0] CA53 lit mem; [chain1] CA53 big mem; [chain2] CA53 power\n\
	DAP1: NIC AXI busmonitor\n\
	DAP2: DDR AXI busmonitor\n\n\
GPU scan chain, subsystem num: 2\n\
	DAP0:[chain0]GPU module enable status; [chain1]GPU hmp\n\n\
Vsp scan chain, subsystem num: 3\n\
	DAP0: [chain0]vsp module enable status; [chain1] vsp EMA control\n\n\
Cam scan chain, subsystem num:4\n\
	DAP0: [chain0]Cam module eable status;[chain1]Cam EMA control\n\n\
Disp sacn chain, subsystem num:5\n\
	DAP0:[chain0]Disp enable status\n\n\
Pubcp scan chain, subsystem num:6\n\
	DAP0: CR5_0 AXI busmonitor\n\
	DAP1: CR5_1 AXI busmonitor\n\
	DAP2: DMA AXI busmonitor\n\
	DAP3: SEC0 AXI busmonitor\n\
	DAP4: SEC1 AXI busmonitor\n\
	DAP5: PPP0 AXI busmonitor\n\
	DAP6: PPP1 AXI busmonitor\n\
	DAP7: TFT AXI busmonitor\n\
	DAP8:[chain0]sleep 0; [chain1]sleep 1; [chain2]busmonitor irq; [chain3]EMA control\n\n\
Wtlcp scan chain, subsystem num:7\n\
	DAP0:[chain0]communication status0; [chain1]communication status1; [chain2]clk_eb0; [chain3]clk_eb1; \n\
	     [chain4] clk_eb2; [chain5] clk_eb3; [chain6]low power0; [chain7] low power1; \n\
	     [chain8]busmon0_int; [chain9]busmon1_int\n\
	DAP1:LDSP P AXI busmonitor\n\
	54 busmonitor, to be add\n\
	DAP54:WCDMA_MATRIX M5 AXI busmonitor\n\n\
AGCP scan chain, subsystem num:8\n\
	DAP0: TL420_PMSS AXI busmonitor\n\
	DAP1: TL420_DMSS AXI busmonitor\n\
	DAP2: AP_DMA AXI busmonitor\n\
	DAP3: CP_DMA AXI busmonitor\n\
	DAP4: AON_AG_CP AXI busmonitor\n\
	DAP5:SRC AXI busmonitor\n\
	DAP6:[chain0]agcp sleep signal0;[chain1]agcp sleep signal1;[chain2]sleep2;[chain3]sleep3;[chain4]agcp bm int;\n\n\
Pub0 scan chain, subsystem num:9\n\
	DAP0:[chain0]Pub0 status0; [chain1]Pub0 status1;[chain2]Pub0 status2; [chain3]Pub0 status3\n\n\
Pub1 scan chain, subsystem num:10\n\
	DAP0:[chain0]Pub1 status0; [chain1]Pub1 status1;[chain2]Pub1 status2; [chain3]Pub1 status3\n\n\
AON scan chain, subsystem num: 11\n\
	DAP0:AP AXI busmonitor\n\
	DAP1:WTL_CP AXI busmonitor\n\
	DAP2:AG_CP AXI busmonitor\n\
	DAP3:PUB_CP AXI busmonitor\n\
	DAP4:SP AXI busmonitor\n\
	DAP5:[chain0]AON EMA control; [chain1]GPU AXI misc monitor; [chain2]GPU AXI rchn monitor; \n\
	     [chain3]GPU AXI wchn monitor; [chain4]MDAR lpdbg\n"
};

struct subsystem_chain{
	unsigned char *subsystem_name;
	unsigned long dap_index;
	unsigned long chain_num;
};

struct subsystem_chain subsys_chain_tab[MUX_DAP_MAX]={
	{"AP", DAP0, 4},
	{"CA53", DAP0, 3},
	{"GPU", DAP0, 2},
	{"VSP", DAP0, 2},
	{"CAM", DAP0, 2},
	{"DISP", DAP0, 1},
	{"PUBCP", DAP8, 4},
	{"WTLCP", DAP0, 10},
	{"AGCP", DAP6, 5},
	{"PUB0", DAP0, 4},
	{"PUB1", DAP0, 4},
	{"AON", DAP5, 5},
};

/**---------------------------------------------------------------------------*
**             		 DJTAG Bus Monitor Scan Chains Define                    **
**---------------------------------------------------------------------------*/

/* bm cmd definetion, 0x14 compatible the old BM cmds */
enum sprd_djtag_bm_cmd {
	BM_DISABLE = 0x14,
	BM_ENABLE,
};

typedef struct _BM_DEF_MONITOR
{
	BM_ARCH bm_arch;
	BM_TYPE bm_type;
	BM_CHN_ID bm_id;
	AHB_CHN_SEL_ID ahb_sel_id;
	BM_DAP_ID bm_dap;
	BM_RW rw_cfg;
	volatile u32 bm_addr_min_l32;
	volatile u32 bm_addr_min_h32;
	volatile u32 bm_addr_max_l32;
	volatile u32 bm_addr_max_h32;
	volatile u32 bm_addr_mask_l32;
	volatile u32 bm_addr_mask_h32;
	unsigned char *chn_name;
}BM_DEF_MONITOR;

BM_DEF_MONITOR bm_def_monitor[BM_DEV_ID_MAX] = {
/*---bm arch----bm type--bm id-----sel id----bm dap----read/write-addr min l----addr min h---addr max l----addr max h---addr mask l--addr mask h*/
	{MUX_AP_BM, AXI_BM, BM_CHN_0, NO_CHN, BM_DAP_1, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 	   "ap ca53"},//ca53
	{MUX_AP_BM, AXI_BM, BM_CHN_1, NO_CHN, BM_DAP_2, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 	   "ap dma"},//dma
	{MUX_AP_BM, AXI_BM, BM_CHN_2, NO_CHN, BM_DAP_3, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,      "ap sdio0"},//sdio0
	{MUX_AP_BM, AXI_BM, BM_CHN_3, NO_CHN, BM_DAP_4, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 	   "ap sdio1"},//sdio1
	{MUX_AP_BM, AXI_BM, BM_CHN_4, NO_CHN, BM_DAP_5, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 	   "ap sdio2"},//sdio2
	{MUX_AP_BM, AXI_BM, BM_CHN_5, NO_CHN, BM_DAP_6, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 	   "ap emmc"},//emmc
	{MUX_AP_BM, AXI_BM, BM_CHN_6, NO_CHN, BM_DAP_7, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 	   "ap nandc"},//nandc
	{MUX_AP_BM, AHB_BM, BM_CHN_7, NO_CHN, BM_DAP_8, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,      "ap otg"},//otg
	{MUX_AP_BM, AXI_BM, BM_CHN_8, NO_CHN, BM_DAP_9, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 	   "ap usb3"},//usb3
	{MUX_AP_BM, AHB_BM, BM_CHN_9, NO_CHN, BM_DAP_10, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,     "ap hsic"},//hsic
	{MUX_AP_BM, AXI_BM, BM_CHN_10, NO_CHN, BM_DAP_11, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,    "ap zipenc"},//zipenc
	{MUX_AP_BM, AXI_BM, BM_CHN_11, NO_CHN, BM_DAP_12, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,    "ap zipdec"},//zipdec
	{MUX_AP_BM, AXI_BM, BM_CHN_12, NO_CHN, BM_DAP_13, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,    "ap cc63p"},//cc63p
	{MUX_AP_BM, AXI_BM, BM_CHN_13, NO_CHN, BM_DAP_14, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,    "ap cc63s"},//cc63s

	{MUX_CA53_BM, AXI_BM, BM_CHN_0, NO_CHN, BM_DAP_1, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,    "ca53 nic"},//nic
	{MUX_CA53_BM, AXI_BM, BM_CHN_1, NO_CHN, BM_DAP_2, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,    "ca53 ddr"},//ddr

	{MUX_PUBCP_BM, AXI_BM, BM_CHN_0, NO_CHN, BM_DAP_0, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "pubcp cr5 0"},//cr5 0
	{MUX_PUBCP_BM, AXI_BM, BM_CHN_1, NO_CHN, BM_DAP_1, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "pubcp cr5 1"},//cr5 1
	{MUX_PUBCP_BM, AXI_BM, BM_CHN_2, NO_CHN, BM_DAP_2, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "pubcp dma"},//dma
	{MUX_PUBCP_BM, AXI_BM, BM_CHN_3, NO_CHN, BM_DAP_3, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "pubcp sec0"},//sec0
	{MUX_PUBCP_BM, AXI_BM, BM_CHN_4, NO_CHN, BM_DAP_4, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "pubcp sec1"},//sec1
	{MUX_PUBCP_BM, AXI_BM, BM_CHN_5, NO_CHN, BM_DAP_5, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "pubcp ppp0"},//ppp0
	{MUX_PUBCP_BM, AXI_BM, BM_CHN_6, NO_CHN, BM_DAP_6, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "pubcp ppp1"},//ppp1
	{MUX_PUBCP_BM, AXI_BM, BM_CHN_7, NO_CHN, BM_DAP_7, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "pubcp tft"},//tft

	{MUX_WTLCP_BM, AXI_BM, BM_CHN_0, NO_CHN, BM_DAP_1, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "wtlcp ldsp tl420 p"},//ldsp tl420 p
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_1, NO_CHN, BM_DAP_2, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "wtlcp tl420 d"},//ldsp tl420 d
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_2, NO_CHN, BM_DAP_3, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "wtlcp ldsp dma"},//ldsp dma
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_3, NO_CHN, BM_DAP_4, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "wtlcp lte proc p1"},//lte proc p1
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_4, NO_CHN, BM_DAP_5, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "wtlcp tgdsp tl420 p"},//tgdsp tl420 p
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_5, NO_CHN, BM_DAP_6, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "wtlcp tgdsp tl420 d"},//tgdsp tl420 d
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_6, NO_CHN, BM_DAP_7, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "wtlcp tgdsp dma"},//tgdsp dma
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_7, NO_CHN, BM_DAP_8, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "wtlcp pub cp dsp"},//pub cp dsp
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_8, NO_CHN, BM_DAP_9, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,   "wtlcp rxdfe"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_9, NO_CHN, BM_DAP_10, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,  "wtlcp tbuf"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_10, NO_CHN, BM_DAP_11, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp fbuf"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_11, NO_CHN, BM_DAP_12, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp che"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_12, NO_CHN, BM_DAP_13, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp chepp"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_13, NO_CHN, BM_DAP_14, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp csi mea"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_14, NO_CHN, BM_DAP_15, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp hsdl"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_15, NO_CHN, BM_DAP_16, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp p2 s0"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_16, NO_CHN, BM_DAP_17, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp p1 s0"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_17, NO_CHN, BM_DAP_18, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp mea"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_18, NO_CHN, BM_DAP_19, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp p2 ulmac"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_19, NO_CHN, BM_DAP_20, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp p2 dbuf"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_20, NO_CHN, BM_DAP_21, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp p2 m1"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_21, NO_CHN, BM_DAP_22, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp p2 m2"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_22, NO_CHN, BM_DAP_23, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc rxdfe"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_23, NO_CHN, BM_DAP_24, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc tbuf"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_24, NO_CHN, BM_DAP_25, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc fbuf"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_25, NO_CHN, BM_DAP_26, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc che"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_26, NO_CHN, BM_DAP_27, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc chepp"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_27, NO_CHN, BM_DAP_28, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc sci meas"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_28, NO_CHN, BM_DAP_29, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc hsdl"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_29, NO_CHN, BM_DAP_30, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc p2 s0"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_30, NO_CHN, BM_DAP_31, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc p1 s0"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_31, NO_CHN, BM_DAP_32, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc mea"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_32, NO_CHN, BM_DAP_33, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc p2 ulmac"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_33, NO_CHN, BM_DAP_34, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc p2 dbuf"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_34, NO_CHN, BM_DAP_35, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc p2 m1"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_35, NO_CHN, BM_DAP_36, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc p2 m2"},
	//{MUX_WTLCP_BM, AXI_BM, BM_CHN_36, NO_CHN, BM_DAP_37, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp mea"},//unuse
	//{MUX_WTLCP_BM, AXI_BM, BM_CHN_37, NO_CHN, BM_DAP_38, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp mea"},//unuse
	//{MUX_WTLCP_BM, AXI_BM, BM_CHN_38, NO_CHN, BM_DAP_39, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp mea"},//unuse
	//{MUX_WTLCP_BM, AXI_BM, BM_CHN_39, NO_CHN, BM_DAP_40, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp mea"},//unuse
	//{MUX_WTLCP_BM, AXI_BM, BM_CHN_40, NO_CHN, BM_DAP_41, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp mea"},//unuse
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_41, NO_CHN, BM_DAP_42, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp scc p1 s1"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_42, NO_CHN, BM_DAP_43, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp acc2ddr m0"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_43, NO_CHN, BM_DAP_44, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp acc2ddr m1"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_44, NO_CHN, BM_DAP_45, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp acc2ddr m2"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_45, NO_CHN, BM_DAP_46, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp acc2ddr m3"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_46, NO_CHN, BM_DAP_47, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp acc2ddr m4"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_47, NO_CHN, BM_DAP_48, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp acc2ddr m5"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_48, NO_CHN, BM_DAP_49, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp wdma link0"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_49, NO_CHN, BM_DAP_50, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp wdma link1"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_50, NO_CHN, BM_DAP_51, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp wdma1"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_51, NO_CHN, BM_DAP_52, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp wdma2"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_52, NO_CHN, BM_DAP_53, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp pub cp s4"},
	{MUX_WTLCP_BM, AXI_BM, BM_CHN_53, NO_CHN, BM_DAP_54, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, "wtlcp dsp s8"},

	{MUX_AGCP_BM, AXI_BM, BM_CHN_0, NO_CHN, BM_DAP_0, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,    "agcp tl420 pmss"},//tl420 pmss
	{MUX_AGCP_BM, AXI_BM, BM_CHN_1, NO_CHN, BM_DAP_1, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,    "agcp tl420 dmss"},//tl420 dmss
	{MUX_AGCP_BM, AXI_BM, BM_CHN_2, NO_CHN, BM_DAP_2, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,    "agcp ap dma"},//ap dma
	{MUX_AGCP_BM, AXI_BM, BM_CHN_3, NO_CHN, BM_DAP_3, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,    "agcp cp dma"},//cp dma
	{MUX_AGCP_BM, AXI_BM, BM_CHN_4, NO_CHN, BM_DAP_4, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,    "agcp aon ag cp"},//aon ag cp
	{MUX_AGCP_BM, AXI_BM, BM_CHN_5, NO_CHN, BM_DAP_5, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,    "agcp v"},//src

	{MUX_AON_BM, AXI_BM, BM_CHN_0, NO_CHN, BM_DAP_0, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,     "aon ap"},//ap
	{MUX_AON_BM, AXI_BM, BM_CHN_1, NO_CHN, BM_DAP_1, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 	   "aon wtlcp"},//wtlcp
	{MUX_AON_BM, AXI_BM, BM_CHN_2, NO_CHN, BM_DAP_2, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,     "aon agcp"},//agcp
	{MUX_AON_BM, AXI_BM, BM_CHN_3, NO_CHN, BM_DAP_3, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,     "aon pubcp"},//pubcp
	{MUX_AON_BM, AXI_BM, BM_CHN_4, NO_CHN, BM_DAP_4, BM_WRITE, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,     "aon sp"},//sp
};

#endif
