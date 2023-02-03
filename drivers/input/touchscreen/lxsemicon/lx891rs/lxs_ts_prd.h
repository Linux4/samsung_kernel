/*
 * SPDX-License-Identifier: GPL-2.0
 *
 * lxs_ts_prd.h
 *
 * LXS touch raw-data debugging
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _LXS_TS_PRD_H_
#define _LXS_TS_PRD_H_

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/signal.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/delay.h>
#include <linux/linkage.h>
#include <linux/syscalls.h>
#include <linux/namei.h>
#include <linux/file.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/firmware.h>
#include <linux/time.h>
#include <linux/fs.h>

//#define __PRD_USE_BIG_ATTR

#define __PRD_SUPPORT_SD_TEST

//#define __PRD_TEST_LOG_OFF

#define __PRD_SUPPORT_SD_MIN_MAX

enum {
	PRD_DBG_FLAG_PATTERN_FW		= BIT(0),
	/* */
	PRD_DBG_FLAG_RAW_CMP		= BIT(7),
	PRD_DBG_FLAG_DIM_OFF		= BIT(8),
	/* */
	PRD_DBG_FLAG_RAW_LOG_OFF	= BIT(16),
	/* */
	PRD_DBG_FLAG_DISABLE		= BIT(30),
};

#define SELF_TEST_PATH_DEFAULT	"/sdcard/lxs_touch_self_test.txt"
#define SPEC_FILE_EXT_DEFAULT	"/sdcard/lxs_touch_spec.txt"
#define SPEC_FILE_INT_DEFAULT	"lxs_touch_spec.txt"

enum {
	PRD_SPEC_LINE_SIZE = (128<<10),
	/* */
	PRD_LOG_POOL_SIZE = (16<<10),
	PRD_LOG_LINE_SIZE = (1<<10),
	/* */
	PRD_RAWBUF_SIZE	= (32<<10),
	/* */
	PRD_BUF_DUMMY		= 128,		//dummy size for avoiding memory overflow
};

enum {
	PRD_APP_INFO_SIZE	= 32,
	/* */
	PRD_M1_COL_SIZE	= 2,
	PRD_SHORT_COL_SZ	= 4,
};

enum {
	TIME_INFO_SKIP = 0,
	TIME_INFO_WRITE,
};

enum {
	PRD_CTRL_IDX_RAW = 0,
	PRD_CTRL_IDX_BASE,
	PRD_CTRL_IDX_DELTA,
	PRD_CTRL_IDX_LABEL,
	PRD_CTRL_IDX_DEBUG,
	PRD_CTRL_IDX_SYSR,
	PRD_CTRL_IDX_SYSB,
	PRD_CTRL_IDX_MAX,
};

enum {
	PRD_CTRL_IDX_SD_OPEN_NODE = 0,
	PRD_CTRL_IDX_SD_SHORT_NODE,
	PRD_CTRL_IDX_SD_OPEN_RX_NODE,
	PRD_CTRL_IDX_SD_OPEN_TX_NODE,
	PRD_CTRL_IDX_SD_SHORT_RX_NODE,
	PRD_CTRL_IDX_SD_SHORT_TX_NODE,
	PRD_CTRL_IDX_SD_SHORT_CH_NODE,
	PRD_CTRL_IDX_SD_SHORT_MUX_NODE,
	PRD_CTRL_IDX_SD_U3_M2_RAW,
	PRD_CTRL_IDX_SD_U3_M2_GAP_X,
	PRD_CTRL_IDX_SD_U3_M2_GAP_Y,
	PRD_CTRL_IDX_SD_U3_M1_RAW,
	PRD_CTRL_IDX_SD_U3_JITTER,
	PRD_CTRL_IDX_SD_U3_M1_JITTER,
	PRD_CTRL_IDX_SD_U3_M2_RAW_SELF,
	PRD_CTRL_IDX_SD_U3_JITTER_SELF,
	PRD_CTRL_IDX_SD_MAX,
};

enum {
	PRD_CTRL_IDX_LPWG_SD_U0_M2_RAW = 0,
	PRD_CTRL_IDX_LPWG_SD_U0_M1_RAW,
	PRD_CTRL_IDX_LPWG_SD_U0_JITTER,
	PRD_CTRL_IDX_LPWG_SD_U0_M1_JITTER,
	PRD_CTRL_IDX_LPWG_SD_U0_M1_RAW_SELF,
	PRD_CTRL_IDX_LPWG_SD_MAX,
};

#define RAW_CTRL_INIT(_id, _name, _cmd, _data_fmt, _flag, _row, _col, _offset, _get_func, _mod_func, _prt_func)	\
		{	\
			.id = _id, .name = _name, .cmd = _cmd,	\
			.data_fmt = _data_fmt, .flag = _flag,\
			.row_size = _row, .col_size = _col,	\
			.offset = _offset, .get_raw_func = _get_func, .mod_raw_func = _mod_func, .prt_raw_func = _prt_func,	\
		}

#define SD_CTRL_OS_INIT(_id, _name, _cmd, _data_fmt, _flag, _row, _col, _offset, _get_func, _mod_func, _prt_func, _cmp_func, _delay)	\
		{	\
			.id = _id, .name = _name, .cmd = _cmd,	\
			.data_fmt = _data_fmt, .flag = _flag,\
			.row_size = _row, .col_size = _col,	\
			.offset = _offset, .get_sd_func = _get_func, .mod_sd_func = _mod_func, .prt_sd_func = _prt_func, .cmp_sd_func = _cmp_func,	\
			.delay = _delay, .is_sd = 1, .is_os = 1, \
		}

#define SD_CTRL_INIT(_id, _name, _cmd, _data_fmt, _flag, _row, _col, _offset, _get_func, _mod_func, _prt_func, _cmp_func, _delay)	\
		{	\
			.id = _id, .name = _name, .cmd = _cmd,	\
			.data_fmt = _data_fmt, .flag = _flag,\
			.row_size = _row, .col_size = _col,	\
			.offset = _offset, .get_sd_func = _get_func, .mod_sd_func = _mod_func, .prt_sd_func = _prt_func, .cmp_sd_func = _cmp_func,	\
			.delay = _delay, .is_sd = 1, .is_os = 0,	\
		}

#define LPWG_SD_CTRL_INIT(_id, _name, _cmd, _data_fmt, _flag, _row, _col, _offset, _get_func, _mod_func, _prt_func, _cmp_func, _delay)	\
		{	\
			.id = _id, .name = _name, .cmd = _cmd,	\
			.data_fmt = _data_fmt, .flag = _flag,\
			.row_size = _row, .col_size = _col,	\
			.offset = _offset, .get_sd_func = _get_func, .mod_sd_func = _mod_func, .prt_sd_func = _prt_func, .cmp_sd_func = _cmp_func,	\
			.delay = _delay, .is_sd = 0, .is_os = 0, \
		}

enum {
	PRD_RAW_FLAG_PEN		= BIT(0),	//NA
	/* */
	PRD_RAW_FLAG_TBL_RXTX	= BIT(4),	//keep table value : row_size, col_size
	PRD_RAW_FLAG_TBL_OFFSET	= BIT(5),	//keep table value : offset
	/* */
	PRD_RAW_FLAG_IGN_EDGE_T	= BIT(8),	//Ignore Top Edge
	PRD_RAW_FLAG_IGN_EDGE_B	= BIT(9),	//Ignore Bottom Edge
	/* */
	PRD_RAW_FLAG_DATA_TMP	= BIT(12),	//Final data is stored in tmp buffer
	/* */
	PRD_RAW_FLAG_FW_SKIP	= BIT(16),
	PRD_RAW_FLAG_FW_NO_STS	= BIT(17),
	PRD_RAW_FLAG_RESP_LSB	= BIT(18),
	/* */
	PRD_RAW_FLAG_SPEC_ARRAY	= BIT(24),
	PRD_RAW_FLAG_SPEC_SHOW	= BIT(25),
	/* */
	PRD_RAW_FLAG_TYPE_SYS	= BIT(31),
};

enum {
	PRD_DATA_FMT_MASK = 0x03,	/* 8bit or 16bit */
	PRD_DATA_FMT_S16	= BIT(0),
	PRD_DATA_FMT_U16	= BIT(1),
	PRD_DATA_FMT_PAD	= BIT(2),
	/* */
	PRT_DATA_FMT_DIM	= BIT(4),
	/* */
	PRD_DATA_FMT_OS_DATA	= BIT(8),
	PRD_DATA_FMT_SELF		= BIT(9),
	PRD_DATA_FMT_RXTX		= BIT(10),
	PRD_DATA_FMT_DATA		= BIT(11),
	/* */
	PRD_DATA_FMT_SYS_ROW	= BIT(12),
	PRD_DATA_FMT_SYS_COL	= BIT(13),
	PRD_DATA_FMT_SYS_PDATA	= BIT(14),
	PRD_DATA_FMT_SYS_PDBG	= BIT(15),
	/* */
	PRD_DATA_FMT_SELF_TX_FIRST	= BIT(16),	// for print_rxtx
	PRD_DATA_FMT_SELF_MISALIGN	= BIT(17),	// for print_raw
	PRD_DATA_FMT_SELF_REV_C		= BIT(18),
	PRD_DATA_FMT_SELF_REV_R	 	= BIT(19),
};

#if 0	/* TBD */
enum {
	PRD_DATA_DIM_LT_LB		= BIT(0),	// 'Left-Top to Left-Bottom'
	PRD_DATA_DIM_RB_RT		= BIT(1),	// 'Right-Bottom to Right-Top'
	PRD_DATA_DIM_LB_LT		= BIT(2),	// 'Left-Bottom to Left-Top'
	PRD_DATA_DIM_RT_RB		= BIT(3),	// 'Right-Top to Right-Bottom'
	PRD_DATA_DIM_FLIP_H		= BIT(4),	// Flip Horizontal
	PRD_DATA_DIM_FLIP_V		= BIT(5),	// Flip Vertical
};
#endif

/*
 * invalid node zone : 'col_s <= col <= col_e' & 'row_s <= row <= row_e'
 */
struct lxs_prd_empty_node {
	int col_s;
	int col_e;
	int row_s;
	int row_e;
};

struct lxs_prd_raw_pad {
	int row;
	int col;
};

struct lxs_prd_raw_ctrl {
	int id;
	char *name;
	int cmd;
	u32 data_fmt;
	u32 data_dim;	//TBD
	u32 flag;
	int row_size;
	int col_size;
	int row_act;	//actual row for print
	int col_act;	//actual col for print
	int data_size;
	u32 offset;
	/* */
	char *raw_buf;
	char *raw_tmp;
	char *prt_buf;
	void *priv;	/* TBD */
	/* */
	int (*get_raw_func)(void *prd_data, void *ctrl_data);
	int (*mod_raw_func)(void *prd_data, void *ctrl_data);
	int (*prt_raw_func)(void *prd_data, void *ctrl_data, int prt_size);
	/* */
	struct lxs_prd_raw_pad *pad;	//pad info
	int is_pad;
	/* */
	int is_dim;		//TBD
};

struct lxs_prd_sd_ctrl {
	int id;
	char *name;
	int cmd;
	u32 data_fmt;
	u32 data_dim;	//TBD
	u32 flag;
	int row_size;
	int col_size;
	int row_act;	//actual row for print
	int col_act;	//actual col for print
	int data_size;
	u32 offset;
	/* */
	char *raw_buf;
	char *raw_tmp;
	char *prt_buf;
	void *priv;	/* TBD */
	/* */
	int (*odd_sd_func)(void *prd_data, void *ctrl_data);	/* specific case */
	int (*get_sd_func)(void *prd_data, void *ctrl_data);
	int (*mod_sd_func)(void *prd_data, void *ctrl_data);
	int (*prt_sd_func)(void *prd_data, void *ctrl_data);
	int (*cmp_sd_func)(void *prd_data, void *ctrl_data);
	/* */
	int is_sd;	//1: sd, 0: lpwg_sd
	int is_os;	//1: open/short, 0: else
	int delay;
	/* */
	struct lxs_prd_raw_pad *pad;	//pad info
	int is_pad;
	/* */
	int is_dim;		//TBD
	/* */
	struct lxs_prd_empty_node *empty_node;
	/* */
	int min;
	int max;
};

struct lxs_prd_data {
	void *ts;
	struct device *dev;
	struct mutex lock;
	/* */
	int chip_type;
	int panel_type;
	int dbg_mask;
	int test_type;
	/* */
	int self;
	int row;
	int col;
	int row_adj;
	int col_adj;
	int ch;
	int raw_base;
	int sys_base;
	int sys_cmd;
	u32 sys_dbg_addr;
	u32 sys_pdata_rx;
	u32 sys_pdata_tx;
	u32 sys_pdebug_rx;
	u32 sys_pdebug_tx;
	/* */
	int raw_lookup_table[PRD_CTRL_IDX_MAX];
	int sd_lookup_table[PRD_CTRL_IDX_SD_MAX];
	int lpwg_sd_lookup_table[PRD_CTRL_IDX_LPWG_SD_MAX];
	int num_raw_ctrl;
	int num_sd_ctrl;
	int num_lpwg_sd_ctrl;
	struct lxs_prd_raw_ctrl *raw_ctrl;
	struct lxs_prd_sd_ctrl *sd_ctrl;
	struct lxs_prd_sd_ctrl *lpwg_sd_ctrl;
	/* */
	char spec_line[PRD_SPEC_LINE_SIZE + PRD_BUF_DUMMY];
	char log_line[PRD_LOG_LINE_SIZE + PRD_BUF_DUMMY];
	char log_pool[PRD_LOG_POOL_SIZE + PRD_BUF_DUMMY];
	char log_bin[PRD_LOG_POOL_SIZE + PRD_BUF_DUMMY];
	char raw_buf[PRD_RAWBUF_SIZE + PRD_BUF_DUMMY];
	char raw_tmp[PRD_RAWBUF_SIZE + PRD_BUF_DUMMY];
	char raw_app[PRD_RAWBUF_SIZE + PRD_BUF_DUMMY];
	/* */
	int app_read;
	int app_mode;
	u32 app_reg_addr;
	u32 app_reg_size;
	struct lxs_prd_raw_ctrl *app_raw_ctrl;
	/* */
	int sysfs_flag;
	/* */
	int sysfs_done;
};

enum {
	CMD_TEST_EXIT = 0,
	CMD_TEST_ENTER,
};

/* AIT Algorithm Engine HandShake CMD */
enum {
	PRD_CMD_NONE = 0,
	PRD_CMD_RAW,
	PRD_CMD_BASE,
	PRD_CMD_DELTA,
	PRD_CMD_LABEL,
	PRD_CMD_FILTERED_DELTA,
	PRD_CMD_RESERVED,
	PRD_CMD_DEBUG,
	/* */
	PRD_CMD_RAW_S = 8,
	PRD_CMD_BASE_S,
	PRD_CMD_DELTA_S,
	PRD_CMD_LABEL_S,
	PRD_CMD_DEBUG_S,
	/* */
	PRD_CMD_SYSR = 100,
	PRD_CMD_SYSB,
	/* */
	PRD_CMD_DONT_USE = 0xEE,
	PRD_CMD_WAIT = 0xFF,
};

/* AIT Algorithm Engine HandShake Status */
enum {
	RS_READY	= 0xA0,
	RS_NONE	= 0x05,
	RS_LOG		= 0x77,
	RS_IMAGE	= 0xAA
};

enum {
	PRD_TIME_STR_SZ = 64,
};

enum {
	REPORT_END_RS_NG = 0x05,
	REPORT_END_RS_OK = 0xAA,
};

enum {
	REPORT_OFF = 0,
	REPORT_RAW,
	REPORT_BASE,
	REPORT_DELTA,
	REPORT_LABEL,
	REPORT_DEBUG_BUG,
	/* */
	REPORT_SYSR,
	REPORT_SYSB,
	/* */
	REPORT_MAX,
};

static const char *prd_app_mode_str[] = {
	[REPORT_OFF]		= "OFF",
	[REPORT_RAW]		= "RAW",
	[REPORT_BASE]		= "BASE",
	[REPORT_LABEL]		= "LABEL",
	[REPORT_DELTA]		= "DELTA",
	[REPORT_DEBUG_BUG]	= "DEBUG_BUF",
	/* */
	[REPORT_SYSR]		= "SYSR",
	[REPORT_SYSB]		= "SYSB",
};

static const int prd_app_mode_ctrl_id[] = {
	[REPORT_OFF]		= -1,
	[REPORT_RAW]		= PRD_CTRL_IDX_RAW,
	[REPORT_BASE]		= PRD_CTRL_IDX_BASE,
	[REPORT_DELTA]		= PRD_CTRL_IDX_DELTA,
	[REPORT_LABEL]		= PRD_CTRL_IDX_LABEL,
	[REPORT_DEBUG_BUG]	= PRD_CTRL_IDX_DEBUG,
	/* */
	[REPORT_SYSR]		= PRD_CTRL_IDX_SYSR,
	[REPORT_SYSB]		= PRD_CTRL_IDX_SYSB,
};

/* Flexible digit width */
#define DIGIT_RANGE_BASE		4

#define PRD_RAWDATA_MAX			(32767)
#define PRD_RAWDATA_MIN			(-32768)

#define RAWDATA_MAX_DIGIT6		(999999)
#define RAWDATA_MIN_DIGIT6		(-99999)

#define RAWDATA_MAX_DIGIT5		(99999)
#define RAWDATA_MIN_DIGIT5		(-9999)

#define RAWDATA_MAX_DIGIT4		(9999)
#define RAWDATA_MIN_DIGIT4		(-999)

#define LXS_PRD_TAG 		"prd: "
#define LXS_PRD_TAG_ERR 	"prd(E): "
#define LXS_PRD_TAG_WARN	"prd(W): "
#define LXS_PRD_TAG_DBG		"prd(D): "

enum _LXS_TOUCH_DBG_FLAG {
	DBG_NONE				= 0,
	DBG_BASE				= BIT(0),
	DBG_TRACE				= BIT(1),
};

#define t_prd_info(_prd, fmt, args...)	\
		input_raw_info_d(true, _prd->dev, LXS_PRD_TAG fmt, ##args)

#define t_prd_err(_prd, fmt, args...)	\
		input_raw_info_d(true, _prd->dev, LXS_PRD_TAG_ERR fmt, ##args)

#define t_prd_warn(_prd, fmt, args...)	\
		input_raw_info_d(true, _prd->dev, LXS_PRD_TAG_WARN fmt, ##args)

#define t_prd_dbg(condition, _prd, fmt, args...)	\
		do {	\
			if (unlikely(t_prd_dbg_mask & (condition)))	\
				input_raw_info_d(true, _prd->dev, LXS_PRD_TAG_DBG fmt, ##args);	\
		} while (0)

#define t_prd_dbg_base(_prd, fmt, args...)	\
		t_prd_dbg(DBG_BASE, _prd, fmt, ##args)

#define t_prd_dbg_trace(_prd, fmt, args...)	\
		t_prd_dbg(DBG_TRACE, _prd, fmt, ##args)

#define __lxs_snprintf(_buf, _buf_max, _size, _fmt, _args...) \
		({	\
			int _n_size = 0;	\
			if (_size < _buf_max)	\
				_n_size = snprintf(_buf + _size, _buf_max - _size,\
								(const char *)_fmt, ##_args);	\
			_n_size;	\
		})


#define lxs_snprintf(_buf, _size, _fmt, _args...) \
		__lxs_snprintf(_buf, PAGE_SIZE, _size, _fmt, ##_args)

#define lxs_prd_buf_snprintf(_prt_buf, _prt_size, _fmt, _args...) \
		__lxs_snprintf(_prt_buf, PRD_LOG_POOL_SIZE, _prt_size, _fmt, ##_args);

#define lxs_prd_log_buf_snprintf(_log_buf, _prt_size, _fmt, _args...) \
		__lxs_snprintf(_log_buf, PRD_LOG_LINE_SIZE, _prt_size, _fmt, ##_args);

#define lxs_prd_log_info(_prd, _buf, _size)	\
		do {	\
			if (!(t_prd_dbg_flag & PRD_DBG_FLAG_RAW_LOG_OFF)) {	\
				if (_size)	t_prd_info(_prd, "%s\n", _buf);	\
			}	\
		} while (0)

#define lxs_snprintf_sd_result(_prt_buf, _prt_size, _item, _ret) \
		lxs_snprintf(_prt_buf, _prt_size, "%s:%s ", _item, (_ret) ? "Fail" : "Pass")

#define lxs_prd_log_flush(_prd, _log_buf, _prt_buf, _log_size, _prt_size)	\
		({	int __prt_size = 0;	\
			lxs_prd_log_info(_prd, _log_buf, _log_size);	\
			__prt_size = lxs_prd_buf_snprintf(_prt_buf, _prt_size, "%s\n", _log_buf);	\
			__prt_size;	\
		})

#define lxs_prd_log_end(_prd, _log_buf, _log_size)	\
		({	int __prt_size = 0;	\
			lxs_prd_log_info(_prd, _log_buf, _log_size);	\
			__prt_size = lxs_prd_log_buf_snprintf(_log_buf, _log_size, "\n");	\
			__prt_size;	\
		})

#define APP_REG_FLAG_LOG	BIT(0)
#define APP_REG_FLAG_WR		BIT(4)

/*
 * App I/F : binary stream
 */
#define DEVICE_ATTR_PRD_APP(_name, _idx)	\
static ssize_t prd_show_app_##_name(struct device *dev,	\
				struct device_attribute *attr, char *buf)	\
{	\
	return prd_show_app_common(dev, buf, REPORT_##_idx);	\
}	\
static DEVICE_ATTR(prd_app_##_name, 0664, prd_show_app_##_name, NULL)

/*
 * Standard - PAGE_SIZE(4KB)
 */
#define DEVICE_ATTR_PRD_STD(_name, _idx, _chk)	\
static ssize_t prd_show_##_name(struct device *dev,	\
				struct device_attribute *attr, char *buf)	\
{	\
	return min(prd_show_common(dev, buf, PRD_CTRL_IDX_##_idx), (ssize_t)PAGE_SIZE);	\
}	\
static DEVICE_ATTR(_name, 0664, prd_show_##_name, NULL)

/*
 * Binary - Big data( > 4KB)
 */
#define PRD_BIN_ATTR_SIZE	(PRD_LOG_POOL_SIZE + PRD_BUF_DUMMY)

#define __DEVICE_ATTR_PRD_BIN(_name, _attr, _show, _store, _size)	\
			struct bin_attribute touch_bin_attr_##_name	\
			= __BIN_ATTR(_name, _attr, _show, _store, _size)

#define DEVICE_ATTR_PRD_BIN(_name, _idx, _chk)	\
static ssize_t prd_show_##_name(struct device *dev,	\
				struct device_attribute *attr, char *buf)	\
{	\
	return prd_show_common(dev, buf, PRD_CTRL_IDX_##_idx);	\
}	\
static ssize_t prd_show_bin_##_name(struct file *filp, struct kobject *kobj,	\
			struct bin_attribute *bin_attr,	\
			char *buf, loff_t off, size_t count)	\
{	\
	struct device *dev = container_of(kobj, struct device, kobj);	\
	struct sec_cmd_data *sec = dev_get_drvdata(dev);	\
	struct lxs_ts_data *ts = container_of(sec, struct lxs_ts_data, sec);	\
	struct lxs_prd_data *prd = __ts_get_prd(ts);	\
	char *log_bin = prd->log_bin;	\
	int prd_buf_size = bin_attr->size;	\
	if (off == 0) {	\
		memset(log_bin, 0, prd_buf_size);	\
		prd_show_##_name(dev, NULL, log_bin);	\
	}	\
	if ((off + count) > prd_buf_size) {	\
		memset(buf, 0, count);	\
	} else {	\
		memcpy(buf, &log_bin[off], count);	\
	}	\
	return (ssize_t)count;	\
}	\
static __DEVICE_ATTR_PRD_BIN(_name, 644, prd_show_bin_##_name, NULL, PRD_BIN_ATTR_SIZE)

#define RUN_DATA_WORD_LEN	10

#endif /* _LXS_TS_PRD_H_ */

