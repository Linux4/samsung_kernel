// SPDX-License-Identifier: GPL-2.0-only
/*
 * Samsung debugging features for Samsung's SoC's.
 *
 * Copyright (c) 2014-2019 Samsung Electronics Co., Ltd.
 *      http://www.samsung.com
 */

/* keys are grouped by size */
static char key32[][MAX_ITEM_KEY_LEN] = {
	"ID", "KTIME", "BIN", "FTYPE", "RR",
	"DPM", "SMP", "PCB", "SMD", "LEV",
	"WAK", "ASB", "PSITE", "DDRID", "RST",
	"INFO2", "INFO3", "RBASE", "MAGIC", "RSTCNT",
	"HLCPU",
};

static char key64[][MAX_ITEM_KEY_LEN] = {
	"BAT", "FAULT", "PINFO",
	"EPD", "ASV", "IDS",
	"HLEHLD", "PWR", "PWROFF", "PINT", "PSTAT",
	"PWROFFS", "PINTS", "PSTATS",
};

static char key256[][MAX_ITEM_KEY_LEN] = {
	"KLG", "BUS", "PANIC", "PC", "LR",
	"BUG", "ESR", "SMU", "FREQ", "ODR",
	"AUD", "UNFZ", "UP", "DOWN", "WDGC",
	"HLTYPE", "MOCP", "SOCP", "DCN",
};

static char key1024[][MAX_ITEM_KEY_LEN] = {
	"CPU0", "CPU1", "CPU2", "CPU3", "CPU4",
	"CPU5", "CPU6", "CPU7", "MFC", "STACK",
	"FPMU", "REGS", "HLDATA", "HLFREQ",
	"HLCNT",
};

/* keys are grouped by sysfs node */
static char akeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "KTIME", "ODR", "BIN", "FTYPE", "FAULT",
	"BUG", "PC", "LR", "STACK", "RR",
	"RSTCNT", "PINFO", "SMU", "BUS", "DPM",
	"ESR", "PCB", "SMD", "WDGC", "KLG", "PANIC",
	"LEV", "WAK", "BAT", "SMP",
	"HLTYPE", "HLDATA", "HLFREQ", "HLEHLD", "HLCPU",
};

static char bkeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "ASB", "PSITE", "DDRID",
	"MOCP", "SOCP", "RST", "INFO2", "INFO3",
	"RBASE", "MAGIC", "PWR", "PWROFF", "PINT", "PSTAT",
	"PWROFFS", "PINTS", "PSTATS",
	"EPD", "UNFZ", "FREQ",
};

static char ckeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "CPU0", "CPU1", "CPU2",
	"CPU3", "CPU4", "CPU5", "CPU6", "CPU7",
	"HLCNT",
};

static char fkeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "UP", "DOWN", "FPMU",
};

static char mkeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "MFC", "AUD", "DCN",
};

static char tkeys[][MAX_ITEM_KEY_LEN] = {
	"ID", "RR", "ASV", "IDS", "REGS",
};
