/* sec_nad.h
 *
 * Copyright (C) 2016 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#ifndef SEC_NAD_H
#define SEC_NAD_H

#if defined(CONFIG_SEC_NAD)
#define NAD_DRAM_TEST_NONE  0
#define NAD_DRAM_TEST_PASS  1
#define NAD_DRAM_TEST_FAIL  2

#define NAD_MAX_LEN_STR     1024
#define NAD_BUFF_SIZE       10 
#define NAD_CMD_LIST        3
#define NAD_MAIN_CMD_LIST        2
#define NAD_MAIN_TIMEOUT	200

struct param_qnad {
	uint32_t magic;
	uint32_t qmvs_remain_count;
	uint32_t ddrtest_remain_count;
	uint32_t ddrtest_result;
	uint32_t total_test_result;
	uint32_t reserved;
	uint32_t thermal;
	uint32_t tested_clock;
};

#define MAX_DDR_ERR_ADDR_CNT 64

struct param_qnad_ddr_result{
	uint32_t ddr_err_addr_total;
	uint64_t ddr_err_addr[MAX_DDR_ERR_ADDR_CNT];
};

#define TEST_SUSPEND(x) (!strcmp((x), "SUSPENDTEST"))
#define TEST_QMESA(x) (!strcmp((x), "QMESADDRTEST"))
#define TEST_PMIC(x) (!strcmp((x), "PMICTEST"))
#define TEST_SDCARD(x) (!strcmp((x), "SDCARDTEST"))
#define TEST_CRYTOSANITY(x) (!strcmp((x), "CRYPTOSANITYTEST"))
#define TEST_PASS(x) (!strcmp((x), "PASS"))
#define TEST_FAIL(x) (!strcmp((x), "FAIL"))

//#define SET_INIT_ITEM_MASK(temp, curr, item) (temp &= (~(0x11 << (((curr) * 16) + ((item) * 2)))))

/*
	0000 0000 0000 0000	 00 00 00        00         00	   00	 00      00
								    [CRYTOSANITY][SDCARD][PMIC][QMESA][SUSPEND]
	|---NOT_SMD_QMVS---| |------------------SMD_QMVS-----------------|
	
	00 : Not Test
	01 : Failed
	10 : PASS
	11 : N/A
*/
#define TEST_ITEM_RESULT(curr, item, result) \
		(result ? \
			(0x1 << (((curr) * 16) + ((item) * 2) + ((result)-1))) \
				: (0x3 << (((curr) * 16) + ((item) * 2))))

enum TEST_ITEM
{
	NOT_ASSIGN = -1,
	SUSPEND = 0,
	QMESA,
	PMIC,
	SDCARD,
	CRYTOSANITY,
	//
	ITEM_CNT,
};

#endif

#endif

