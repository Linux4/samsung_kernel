/*
 * drivers/debug/sec_quest.c
 *
 * COPYRIGHT(C) 2006-2017 Samsung Electronics Co., Ltd. All Right Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef SEC_QUEST_H
#define SEC_QUEST_H


#include <linux/kern_levels.h>

#define QUEST_PRINT(format, ...) printk(KERN_ERR "[QUEST] " format, ##__VA_ARGS__)

#define QUEST_SMD_MAGIC		0xfaceb00c

#define MAX_LEN_STR			1024
#define QUEST_BUFF_SIZE		10
#define QUEST_CMD_LIST		3
#define QUEST_MAIN_CMD_LIST	2
#define MAIN_QUEST_TIMEOUT 5400
#define BUFF_SZ 			256

#define SMD_QUEST_PROG			"/system/bin/quest/quest.sh"
#define CAL_QUEST_PROG			"/system/bin/quest/quest.sh"
#define MAIN_QUEST_PROG			"/system/bin/quest/quest_main.sh"
#define ERASE_QUEST_PRG			"/system/bin/quest/remove_files.sh"
#define ARRANGE_QUEST_LOGS_PRG	"/system/bin/quest/arrange_quest_logs.sh"
#define MOVE_QUESTRESULT_PRG	"/system/bin/quest/move_questresult.sh"
#define QUEST_DEBUGGING_PRG		"/system/bin/quest/quest_debugging.sh"

#define SMD_QUEST_RESULT		"/data/log/quest/SMD/test_result.csv"
#define CAL_QUEST_RESULT		"/data/log/quest/CAL/test_result.csv"
#define MAIN_QUEST_RESULT		"/data/log/quest/MAIN/test_result.csv"

#define SMD_QUEST_LOGPATH		"/data/log/quest/SMD"
#define CAL_QUEST_LOGPATH		"/data/log/quest/CAL"
#define MAIN_QUEST_LOGPATH		"/data/log/quest/MAIN"

#define MOUNTPOINT_LOGFS		"/data/log/quest"
#define UEFI_QUESTRESULT_FILE	"/data/log/quest/questresult.txt"
#define UEFI_ENHANCEMENT_START_FILE	"/data/log/quest/EnhanceStart.txt"


#if defined(CONFIG_SEC_QUEST_UEFI)
/*-----  for parsing log of quest_uefi -----*/

//struct delayed_work response_work;
//struct device *saved_dev;

#define LOG_PATH_QUEST_UEFI_LOG "/data/log/quest/questresult.txt"
#define PASS_STR "PASS"
#define FAIL_STR "FAIL"
#define WAIT_TIME_BEFORE_RESPONSE 3000

/*-----------------------------------*/
#endif


struct param_quest_t {
	uint32_t magic;
	int32_t quest_hlos_remain_count;
	int32_t ddrtest_remain_count;
	uint32_t ddrtest_result;
	uint32_t ddrtest_mode;
	uint32_t bitmap_item_result;
	uint32_t smd_test_result;
	uint32_t thermal;
	uint32_t tested_clock;
#if defined(CONFIG_SEC_QUEST_UEFI)	
	int32_t quest_uefi_remain_count;
	uint32_t quest_uefi_result;
#else
	int32_t padding_quest_uefi_remain_count;
	uint32_t padding_quest_uefi_result;
#endif		
	uint32_t quest_step;
#if defined(CONFIG_SEC_SKP)
	uint32_t quest_cpu_serial;
#endif
};

enum quest_step_t {
SMD_QUEST=0,
CAL_QUEST=1,
MAIN_QUEST=2,
#if defined(CONFIG_SEC_SKP)
SKP_QUEST=3,
#endif
};

enum ddrtest_result_t {
DDRTEST_INCOMPLETED = 0x00,
DDRTEST_FAIL = 0x11,
DDRTEST_PASS = 0x22,
};

enum quest_item_test_result_t {
QUEST_ITEM_TEST_INCOMPLETED = 0,
QUEST_ITEM_TEST_FAIL = 1,
QUEST_ITEM_TEST_PASS = 2,
};

enum totaltest_result_t {
TOTALTEST_UNKNOWN = 0,
TOTALTEST_FAIL = 1,
TOTALTEST_PASS = 2,
TOTALTEST_NO_RESULT_STRING = 3,
TOTALTEST_NO_RESULT_FILE = 4,
};

#define MAX_DDR_ERR_ADDR_CNT 64

struct param_quest_ddr_result_t {
	uint32_t ddr_err_addr_total;
	uint64_t ddr_err_addr[MAX_DDR_ERR_ADDR_CNT];
};

#define TEST_CRYPTO(x) (!strcmp((x), "CRYPTOTEST"))
#define TEST_ICACHE(x) (!strcmp((x), "ICACHETEST"))
#define TEST_CCOHERENCY(x) (!strcmp((x), "CCOHERENCYTEST"))
#define TEST_SUSPEND(x) (!strcmp((x), "SUSPENDTEST"))
#define TEST_VDDMIN(x) (!strcmp((x), "VDDMINTEST"))
#define TEST_QMESADDR(x) (!strcmp((x), "QMESADDRTEST"))
#define TEST_QMESACACHE(x) (!strcmp((x), "QMESACACHETEST"))
#define TEST_UFS(x) (!strcmp((x), "UFSTEST"))
#define TEST_GFX(x) (!strcmp((x), "GFXTEST"))
#define TEST_SENSOR(x) (!strcmp((x), "SENSORTEST"))
#define TEST_SENSORPROBE(x) (!strcmp((x), "SENSORPROBETEST"))
#define TEST_DDR_SCAN(x) (!strcmp((x), "DDRSCANTEST"))
#define TEST_A75G(x) (!strcmp((x), "A75GTEST"))
#define TEST_Q65G(x) (!strcmp((x), "Q65GTEST"))
#define TEST_THERMAL(x) (!strcmp((x), "THERMALTEST"))
#define TEST_QDAF(x) (!strcmp((x), "QDAFTEST"))
#define TEST_PASS(x) (!strcmp((x), "PASS"))
#define TEST_FAIL(x) (!strcmp((x), "FAIL"))

/*
 *	      00       00    00    00   00           00      00        00     00      00         00       00      00        00          00   00
 *    [QDAF][THERMAL][Q65G][A75G][GFX][SENSORPROBE][SENSOR][DDR_SCAN][CRYTO][ICACHE][CCOHRENCY][SUSPEND][VDDMIN][QMESADDR][QMESACACHE][UFS]
 *
 *	00 : Not Test
 *	01 : Failed
 *	10 : PASS
 */

/* Please sync with STR_TEST_ITEM in sec_quest.c */
enum TEST_ITEM {
	NOT_ASSIGN = -1,
	UFS,
	QMESACACHE,
	QMESADDR,
	VDDMIN,
	SUSPEND,
	CCOHERENCY,
	ICACHE,
	CRYPTO,
	DDR_SCAN,
	SENSOR,
	SENSORPROBE,
	GFX,
	A75G,
	Q65G,
	THERMAL,
	QDAF,
	//
	ITEM_CNT,
};

#define SET_UEFI_RESULT(step,old_val,new_val) \
	(old_val)&(~(0xFF<<(8*step)))|(new_val<<(8*step))


#define TEST_ITEM_RESULT(curr, item, result) \
		(result ? \
			(0x1 << (((curr) * 16) + ((item) * 2) + ((result)-1))) \
				: (0x0 << (((curr) * 16) + ((item) * 2))))

#define GET_DDR_TEST_RESULT(step, ddrtest_result) \
	(((ddrtest_result)&((0xFF<<(8*step))))>>(8*step))


#if defined(CONFIG_SEC_QUEST_UEFI)
enum UEFI_TEST_ITEM {
	GROUP_CACHE,
	GROUP_MEMORY,
	GROUP_CPUSS,
	GROUP_CPUMAX,
	GROUP_HIGHPER,
	GROUP_SLEEP,
	//
	UEFI_ITEM_CNT,
}; 

#define GET_ONLY_QUEST_UEFI_RESULT(total_test_result) \
	((0xFFFF&~(0x3<<DDR_SCAN*2))&total_test_result)

struct quest_uefi_parsing_info_t {
	uint16_t item_num;
	uint16_t result;
	char item_name[32];
};

static struct quest_uefi_parsing_info_t parsing_info[] = {
	{ GROUP_CACHE,		0, 	"GROUP - CACHE"},
	{ GROUP_MEMORY,		0, 	"GROUP - MEMORY"},
	{ GROUP_CPUSS,		0, 	"GROUP - CPUSS"},
	{ GROUP_CPUMAX,		0, 	"GROUP - Set CPU to Max"},
	{ GROUP_HIGHPER, 	0, 	"GROUP - High Performance"},	
	{ GROUP_SLEEP,		0,	"GROUP - SLEEP"},
	// string for test done should be at last index
	{ 0, 				0, 	"QUEST Test Done"},
};	
#endif

enum quest_qdaf_action_t {
	QUEST_QDAF_ACTION_EMPTY = 0,
		
	/*==== from AtNadCheck.java to qdaf.sh ====*/
	QUEST_QDAF_ACTION_CONTROL_START_WITHOUT_PANIC = 1,
	QUEST_QDAF_ACTION_CONTROL_START_WITH_PANIC = 2,	
	QUEST_QDAF_ACTION_CONTROL_STOP = 3,
	/*==== from quest.sh to qdaf.sh ====*/	
	QUEST_QDAF_ACTION_CONTROL_STOP_WATING = 4,	

	/*==== from AtNadCheck.java to qdaf.sh ====*/
	QUEST_QDAF_ACTION_RESULT_ERASE = 5,
	QUEST_QDAF_ACTION_RESULT_GET = 6,		

	/*==== from qdaf.sh ====*/
	QUEST_QDAF_ACTION_DEBUG_SAVE_LOGS = 7,
	QUEST_QDAF_ACTION_DEBUG_TRIGGER_PANIC = 8,

	/*==== additional cmds ====*/
	QUEST_QDAF_ACTION_RESULT_TO_NAD_RESULT = 9,
};

enum quest_qdaf_result_string_t {
	QUEST_QDAF_RESULT_OK = 0,
	QUEST_QDAF_RESULT_NG = 1,
	QUEST_QDAF_RESULT_NONE = 2,
};

#define QDAF_PROG "/system/bin/quest/qdaf.sh"
#define QDAF_QMESA_LOG "/data/log/qdaf_qmesa_log.txt"

static int qdaf_cur_cmd_mode;


#endif
