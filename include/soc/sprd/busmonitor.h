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

#ifndef __SPRD_BUSMONITOR_H__
#define __SPRD_BUSMONITOR_H__

#define AXI_BM_INTC_REG				0x00
#define AXI_BM_CFG_REG				0x04
#define AXI_BM_ADDR_MIN_REG			0x08
#define AXI_BM_ADDR_MAX_REG			0x0C
#define AXI_BM_ADDR_MSK_REG			0x10
#define AXI_BM_DATA_MIN_L_REG		0x14
#define AXI_BM_DATA_MIN_H_REG		0x18
#define AXI_BM_DATA_MAX_L_REG		0x1C
#define AXI_BM_DATA_MAX_H_REG		0x20
#define AXI_BM_DATA_MSK_L_REG		0x24
#define AXI_BM_DATA_MSK_H_REG		0x28
#define AXI_BM_CNT_WIN_LEN_REG		0x2C
#define AXI_BM_PEAK_WIN_LEN_REG		0x30
#define AXI_BM_MATCH_ADDR_REG		0x34
#define AXI_BM_MATCH_CMD_REG		0x38
#define AXI_BM_MATCH_DATA_L_REG		0x3C
#define AXI_BM_MATCH_DATA_H_REG		0x40
#define AXI_BM_RTRANS_IN_WIN_REG	0x44
#define AXI_BM_RBW_IN_WIN_REG		0x48
#define AXI_BM_RLATENCY_IN_WIN_REG	0x4C
#define AXI_BM_WTRANS_IN_WIN_REG	0x50
#define AXI_BM_WBW_IN_WIN_REG		0x54
#define AXI_BM_WLATENCY_IN_WIN_REG	0x58
#define AXI_BM_PEAKBW_IN_WIN_REG	0x5C
#define AXI_BM_MATCH_ID_REG			0x64

#define AXI_BM_BUS_STATUS_REG       0X68


#define AHB_BM_INTC_REG				0x00

#define SPRD_BM_SUCCESS	0
#define BM_INT_MSK_STS	BIT(31)
#define BM_INT_CLR		BIT(29)
#define BM_INT_EN		BIT(28)
#define BM_CNT_CLR		BIT(4)
#define BM_CNT_START	BIT(3)
#define BM_CNT_EN		BIT(1)
#define BM_CHN_EN		BIT(0)

#define BM_BS_AWVALID   BIT(9)
#define BM_BS_AWREADY   BIT(8)
#define BM_BS_WVALID    BIT(7)
#define BM_BS_WREADY	BIT(6)
#define BM_BS_BVALID    BIT(5)
#define BM_BS_BREADY	BIT(4)
#define BM_BS_ARVALID	BIT(3)
#define BM_BS_ARREADY	BIT(2)
#define BM_BS_RVALID	BIT(1)
#define BM_BS_RREADY    BIT(0)

#define BM_DEBUG_ALL_CHANNEL	0xFF
#define BM_CHANNEL_ENABLE_FLAGE	0x03FF0000
#define BM_CHN_NAME_CMD_OFFSET	0x7

#define BM_DBG(f,x...)	printk(KERN_DEBUG "BM_DEBGU " f, ##x)
#define BM_INFO(f,x...)	printk(KERN_INFO "BM_INFO " f, ##x)
#define BM_WRN(f,x...)	printk(KERN_WARNING"BM_WRN " f, ##x)
#define BM_ERR(f,x...)	printk(KERN_ERR "BM_ERR " f, ##x)

#define PER_COUTN_LIST_SIZE 128
/*the buf can store 8 secondes data*/
#define PER_COUNT_RECORD_SIZE 800
#define PER_COUNT_BUF_SIZE (64 * 4 * PER_COUNT_RECORD_SIZE)

#define LOG_FILE_PATH "/mnt/obb/axi_per_log"
//#define LOG_FILE_PATH "/storage/sdcard0/axi_per_log"
/*the log file size about 1.5Mbytes per min*/
#define LOG_FILE_SECONDS (60  * 30)
#define LOG_FILE_MAX_RECORDS (LOG_FILE_SECONDS * 100)
#define BM_CONTINUE_DEBUG_SIZE	20

static struct file *log_file;
static struct semaphore bm_seam;
static int buf_read_index;
static int buf_write_index;
static bool bm_irq_in_process;
/*the star log include a lot of unuseful info, we need skip it.*/
static int buf_skip_cnt;
long long t_stamp;
static void *per_buf = NULL;

/*depending on the platform*/
enum sci_bm_index {
	AXI_BM0 = 0x0,
	AXI_BM1,
	AXI_BM2,
	AXI_BM3,
	AXI_BM4,
	AXI_BM5,
	AXI_BM6,
	AXI_BM7,
	AXI_BM8,
	AXI_BM9,
	AHB_BM0,
	AHB_BM1,
	AHB_BM2,
	BM_SIZE,
};

enum sci_ahb_bm_index {
	AHB_BM0_CHN0 = AHB_BM0,
	AHB_BM0_CHN1,
	AHB_BM0_CHN2,
	AHB_BM0_CHN3,
	AHB_BM1_CHN0,
	AHB_BM1_CHN1,
	AHB_BM1_CHN2,
	AHB_BM1_CHN3,
	AHB_BM2_CHN0,
	AHB_BM2_CHN1,
	AHB_BM2_CHN2,
	BM_CHANNEL_SIZE,
};

struct bm_callback_desc {
	void (*fun)(void *);
	void *data;
};
static struct bm_callback_desc bm_callback_set[BM_SIZE];

enum sci_bm_cmd_index {
	BM_STATE = 0x0,
	BM_CHANNELS,
	BM_AXI_DEBUG_SET,
	BM_AHB_DEBUG_SET,
	BM_PERFORM_SET,
	BM_PERFORM_UNSET,
	BM_OCCUR,
	BM_CONTINUE_SET,
	BM_CONTINUE_UNSET,
	BM_DFS_SET,
	BM_DFS_UNSET,
	BM_PANIC_SET,
	BM_PANIC_UNSET,
	BM_BW_CNT_START,
	BM_BW_CNT_STOP,
	BM_BW_CNT_RESUME,
	BM_BW_CNT,
	BM_BW_CNT_CLR,
	BM_DBG_INT_CLR,
	BM_DBG_INT_SET,
	BM_DISABLE,
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
	BM_CMD_MAX,
};

enum sci_bm_chn {
	CHN0 = 0x0,
	CHN1,
	CHN2,
	CHN3,
};

enum sci_bm_mode {
	R_MODE,
	W_MODE,
	RW_MODE,
	/*
	PER_MODE,
	RST_BUF_MODE,
	*/
};

struct bm_per_info {
	u32 count;
	u32 t_start;
	u32 t_stop;
	u32 tmp1;
	u32 tmp2;
	u32 per_data[10][6];
};

struct sci_bm_cfg {
	u32 bm_bits;
	u32 win_len;
	u32 addr_min;
	u32 addr_max;
	u32 addr_msk;
	u32 data_min_l;
	u32 data_min_h;
	u32 data_max_l;
	u32 data_max_h;
	u32 data_msk_l;
	u32 data_msk_h;
	u32 chn;
	u32 fun;
	u32 data;
	u32 peak_win_len;
	enum sci_bm_mode bm_mode;
};

struct sci_bm_reg {
	u32 intc;
	u32 cfg;
	u32 addr_min;
	u32 addr_max;
	u32 addr_msk;
	u32 data_min_l;
	u32 data_min_h;
	u32 data_max_l;
	u32 data_max_h;
	u32 data_msk_l;
	u32 data_msk_h;
	u32 cnt_win_len;
	u32 peak_win_len;
	u32 match_addr;
	u32 match_cmd;
	u32 match_data_l;
	u32 match_data_h;
	u32 rtrans_in_win;
	u32 rbw_in_win;
	u32 rlatency_in_win;
	u32 wtrans_in_win;
	u32 wbw_in_win;
	u32 wlatency_in_win;
	u32 peakbw_in_win;
};

struct bm_debug_info{
	u32 bm_index;
	u32 msk_addr;
	u32 msk_cmd;
	u32 msk_data_l;
	u32 msk_data_h;
	u32 msk_id;
};
static struct bm_debug_info debug_bm_int_info[BM_SIZE];

struct bm_continue_debug{
	bool bm_continue_dbg;
	u32 loop_cnt;
	u32 current_cnt;
	struct bm_debug_info bm_ctn_info[BM_CONTINUE_DEBUG_SIZE];
};
static struct bm_continue_debug bm_ctn_dbg;

//pls do not modify the head of the struct, if you want to add same para, pls add at the end of the struct.
struct bm_state_info{
	u32 bm_def_mode;
	u32 axi_bm_cnt;//the count of axi bm
	u32 ahb_bm_chn;//the count of ahb bm channels
	//the star of the channels.
	u32 cpu_chn;
	u32 disp_chn;
	u32 gpu_chn;
	u32 ap_zip_chn;
	u32 zip_chn;
	u32 ap_chn;
	u32 mm_chn;
	u32 cp0_arm0_chn;
	u32 cp0_arm1_chn;
	u32 cp0_dsp_chn;
	u32 cp0_arm0_1_chn;
	u32 cp1_lte_chn;
	u32 cp1_dsp_chn;
	u32 cp1_arm_chn;
	u32 cp2_chn;

	//ahb channels.
	u32 ap_dap;
	u32 ap_cpu;
	u32 ap_dma_r;
	u32 ap_dma_w;
	u32 ap_sdio_0;
	u32 ap_sdio_1;
	u32 ap_sdio_2;
	u32 ap_emmc;
	u32 ap_nfc;
	u32 ap_usb;
	u32 ap_hsic;
	u32 ap_otg;
	u32 ap_nandc;
	bool bm_dbg_st;
	bool bm_dfs_off_st;
	bool bm_panic_st;
	bool bm_stack_st;
};
static struct bm_state_info bm_st_info;

struct bm_id_name {
	unsigned char *chn_name;
	unsigned long chn_type; /*1 for axi channel, 0 for ahb channel*/
};
static struct bm_id_name bm_chn_name[36];
#define BM_AXI_TOTAL_CHN_NAME	15
#define BM_AHB_TOTAL_CHN_NAME	13
static struct bm_id_name bm_name_list[36] = {
	{"CPU"},
	{"DISP"},
	{"GPU"},
	{"AP/ZIP"},
	{"ZIP"},
	{"AP"},
	{"MM"},
	{"CP0 ARM0"},
	{"CP0 ARM1"},
	{"CP0 DSP"},
	{"CP0 ARM0/1"},
	{"CP1 LTEACC/HARQ"},
	{"CP1 DSP"},
	{"CP1 CA5"},
	{"CP2"},

	{"AP DAP"},
	{"AP CPU"},
	{"AP DMA READ"},
	{"AP DMA WRITE"},
	{"AP SDIO 0"},
	{"AP SDIO 1"},
	{"AP SDIO 2"},
	{"AP EMMC"},
	{"AP NFC"},
	{"AP USB"},
	{"AP HSIC"},
	{"AP OTG"},
	{"AP NANDC"},
	{""},
};

struct bm_chn_def_val {
	u32 str_addr;
	u32 end_addr;
	u32 min_data;
	u32 max_data;
	u32 bm_mask;
	u32 mode;
	u32 chn_sel;
};
static struct bm_chn_def_val bm_store_vale[BM_SIZE];
static struct bm_chn_def_val bm_def_value[BM_SIZE] = {
	{0x00000000, 0x00000000, 0x0fffffff, 0x00000000, W_MODE, 0},
	{0x00000000, 0x00000000, 0x0fffffff, 0x00000000, W_MODE, 0},
	{0x00000000, 0x00000000, 0x0fffffff, 0x00000000, W_MODE, 0},
	{0x00000000, 0x00000000, 0x0fffffff, 0x00000000, W_MODE, 0},
	{0x00000000, 0x00000000, 0x0fffffff, 0x00000000, W_MODE, 0},
	{0x8D600000, 0x9F8AD000, 0x0fffffff, 0x00000000, W_MODE, 0},
	{0x8D600000, 0x9F8AD000, 0x0fffffff, 0x00000000, W_MODE, 0},
	{0x8D600000, 0x9F8AD000, 0x0fffffff, 0x00000000, W_MODE, 0},
	{0x8D600000, 0x9F8AD000, 0x0fffffff, 0x00000000, W_MODE, 0},
	{0x8D600000, 0x9F8AD000, 0x0fffffff, 0x00000000, W_MODE, 0},

	{0x00000000, 0x00000000, 0x00000000, 0x00000000, W_MODE, 0},
	{0x00000000, 0x00000000, 0x00000000, 0x00000000, W_MODE, 0},
	{0x00000000, 0x00000000, 0x00000000, 0x00000000, W_MODE, 0},
};

#endif
