/*
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DISPLAY_PROFILER_H__
#define __DISPLAY_PROFILER_H__

#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/workqueue.h>

#include <media/v4l2-subdev.h>

#include "maskgen.h"
#include "../panel_debug.h"

#define PROFILER_VERSION 191202

#define prof_en(p, enable)	\
		(((p)->conf->profiler_en) && ((p)->conf->enable##_en) ? true : false)

#define prof_disp(p, enable)	\
		((((p)->conf->enable##_en) && ((p)->conf->enable##_disp)) ? true : false)

#define prof_dbg(p, enable)	\
		(((p)->conf->enable##_debug) ? true : false)

#define prof_info(p, en, fmt, ...)							\
	do {									\
		if ((p)->conf->en) {				\
			panel_info(pr_fmt(fmt), ##__VA_ARGS__);		\
		}								\
	} while (0)

#define CYCLE_TIME_DEFAULT 100

enum PROFILER_SEQ {
	PROFILE_WIN_UPDATE_SEQ = 0,
	PROFILE_DISABLE_WIN_SEQ,
	SEND_PROFILE_FONT_SEQ,
	INIT_PROFILE_FPS_SEQ,
	DISPLAY_PROFILE_FPS_SEQ,
	PROFILE_SET_COLOR_SEQ,
	PROFILER_SET_CIRCLR_SEQ,
	DISABLE_PROFILE_FPS_MASK_SEQ,
	WAIT_PROFILE_FPS_MASK_SEQ,
	MEM_SELECT_PROFILE_FPS_MASK_SEQ,
	ENABLE_PROFILE_FPS_MASK_SEQ,
	MAX_PROFILER_SEQ,
};


enum PROFILER_MAPTBL {
	PROFILE_WIN_UPDATE_MAP = 0,
	DISPLAY_PROFILE_FPS_MAP,
	PROFILE_SET_COLOR_MAP,
	PROFILE_SET_CIRCLE,
	PROFILE_FPS_MASK_POSITION_MAP,
	PROFILE_FPS_MASK_COLOR_MAP,
	MAX_PROFILER_MAPTBL,
};

#define FPS_COLOR_RED	0
#define FPS_COLOR_BLUE	1
#define FPS_COLOR_GREEN	2

struct profiler_tune {
	char *name;
	struct seqinfo *seqtbl;
	u32 nr_seqtbl;
	struct maptbl *maptbl;
	u32 nr_maptbl;
	struct profiler_config *conf;
	struct mprint_config *mprint_config;
};

struct prifiler_rect {
	u32 left;
	u32 top;
	u32 right;
	u32 bottom;
	
	bool disp_en;
};


struct fps_slot {
	ktime_t timestamp;
	unsigned int frame_cnt;
};


struct profiler_systrace {
	int pid;
};

#define FPS_MAX 100000
#define MAX_SLOT_CNT 5

struct profiler_fps {
	unsigned int frame_cnt;
	unsigned int prev_frame_cnt;

	unsigned int instant_fps;
	unsigned int average_fps;
	unsigned int comp_fps;

	ktime_t win_config_time;

	struct fps_slot slot[MAX_SLOT_CNT];
	unsigned int slot_cnt;
	unsigned int total_frame;
	unsigned int color;
};

#define MAX_TE_CNT 10
struct profiler_te {
	s64 last_time;
	s64 last_diff;
	s64 times[MAX_TE_CNT];
	int idx;
//	int consume_idx;
	spinlock_t slock;
};

struct profiler_hiber {
	bool hiber_status;
	int hiber_enter_cnt;
	int hiber_exit_cnt;
	s64 hiber_enter_time;
	s64 hiber_exit_time;
};

#define PROFILER_CMDLOG_SIZE 1024
#define PROFILER_CMDLOG_BUF_SIZE 524288

struct profiler_cmdlog_data {
	u32 pkt_type;
	s64 time;
	u32 cmd;
	u32 size;
	u32 offset;
	u8 *data;
	u32 option;
};

#define PROFILER_DATALOG_MASK_DIR(x) ((x) & (0b11 << 30))
/* pkt_type_direction[31:30] */
enum {
	PROFILER_DATALOG_DIRECTION_READ = 1 << 31,
	PROFILER_DATALOG_DIRECTION_WRITE = 1 << 30,
};

#define PROFILER_DATALOG_MASK_PROTO(x) ((x) & (0b111111 << 24))
/* pkt_type_protocol[29:24] */
enum {
	PROFILER_DATALOG_CMD_DSI = 1 << 24,
	PROFILER_DATALOG_CMD_SPI = 2 << 24,
	PROFILER_DATALOG_PANEL = 3 << 24,
};

#define PROFILER_DATALOG_MASK_SUB(x) ((x) & (0b11111111 << 16))
/* pkt_type_subtype[23:16] */
enum {
	PROFILER_DATALOG_DSI_UNKNOWN = 0 << 16,
	PROFILER_DATALOG_DSI_GEN_CMD = 1 << 16,
	PROFILER_DATALOG_DSI_CMD_NO_WAKE = 2 << 16,
	PROFILER_DATALOG_DSI_DSC_CMD = 3 << 16,
	PROFILER_DATALOG_DSI_PPS_CMD = 4 << 16,
	PROFILER_DATALOG_DSI_GRAM_CMD = 5 << 16,
	PROFILER_DATALOG_DSI_SRAM_CMD = 6 << 16,
	PROFILER_DATALOG_DSI_SR_FAST_CMD = 7 << 16,
};
enum {
	PROFILER_DATALOG_PANEL_CMD_FLUSH_START = 1 << 16,
	PROFILER_DATALOG_PANEL_CMD_FLUSH_END = 2 << 16,
};
/* pkt_type[15:0] Reserved */


#define PROFILER_CMDLOG_FILTER_SIZE (0xFF + 1)

struct profiler_config {
	int profiler_en;
	int profiler_debug;
	int systrace;
	int timediff_en;
	int cycle_time;

	int fps_en;
	int fps_disp;
	int fps_debug;

	int te_en;
	int te_disp;
	int te_debug;

	int hiber_en;
	int hiber_disp;
	int hiber_debug;

	int cmdlog_en;
	int cmdlog_debug;
	int cmdlog_disp;
	int cmdlog_level;
	int cmdlog_filter_en;

	int mprint_en;
	int mprint_debug;
};

struct profiler_device {
	bool initialized;
	struct v4l2_subdev sd;

	struct seqinfo *seqtbl;
	u32 nr_seqtbl;
	struct maptbl *maptbl;
	u32 nr_maptbl;

	struct prifiler_rect win_rect;
	//struct profile_data *data;

	struct profiler_fps fps;
	
	int prev_win_cnt;
	int flag_font;
	unsigned int circle_color;

	struct profiler_systrace systrace;

	struct task_struct *thread;

	struct profiler_config *conf;

	struct profiler_te te_info;
	struct profiler_hiber hiber_info;

	struct mprint_config *mask_config;
	struct mprint_props mask_props;

	int cmdlog_idx_head;
	int cmdlog_idx_tail;
	struct profiler_cmdlog_data *cmdlog_list;
	int cmdlog_data_idx;
	u8 *cmdlog_data;	

};

#define PROFILER_IOC_BASE	'P'

#define PROFILE_REG_DECON 	_IOW(PROFILER_IOC_BASE, 1, struct profile_data *)
#define PROFILE_WIN_UPDATE	_IOW(PROFILER_IOC_BASE, 2, struct decon_rect *)
#define PROFILE_WIN_CONFIG	_IOW(PROFILER_IOC_BASE, 3, struct decon_win_config_data *)
#define PROFILER_SET_PID	_IOW(PROFILER_IOC_BASE, 4, int *)
#define PROFILER_COLOR_CIRCLE	_IOW(PROFILER_IOC_BASE, 5, int *)
#define PROFILE_TE			_IOW(PROFILER_IOC_BASE, 6, s64)
#define PROFILE_HIBER_ENTER _IOW(PROFILER_IOC_BASE, 7, s64)
#define PROFILE_HIBER_EXIT  _IOW(PROFILER_IOC_BASE, 8, s64)
#define PROFILE_DATALOG	_IOW(PROFILER_IOC_BASE, 9, struct profiler_cmdlog_data *)

int profiler_probe(struct panel_device *panel, struct profiler_tune *profile_tune);

#endif //__DISPLAY_PROFILER_H__

