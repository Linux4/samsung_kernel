/*
 * drivers/debug/sec_quest_param.c
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

#include <linux/sec_quest.h>
#include <linux/sec_quest_param.h>
#include <linux/workqueue.h>
#include <linux/completion.h>
#include <linux/printk.h>
#include <linux/sec_param.h>



extern char *STR_ITEM[ITEM_ITEMSCOUNT];
extern char *STR_SUBITEM[SUBITEM_ITEMSCOUNT];

//////////////////////////////////////////////////
/////// param data ////////////////////////////////
//////////////////////////////////////////////////
struct param_quest_t param_quest_data;
struct param_quest_ddr_result_t param_quest_ddr_result_data;
unsigned int param_api_gpio_test;
char param_api_gpio_test_result[256];
//////////////////////////////////////////////////



//////////////////////////////////////////////////
/////// param functions /////////////////////////////
//////////////////////////////////////////////////
char *uint64ToBinary(uint64_t i) {
  static char s[96+1] = { '0', };
  int count = 96;

  do { 
  	s[--count] = '0' + (char) (i & 1);
	i = i >> 1;
	if( ((count-1)%3==0) && count>1 ) s[--count] = ' ';
  } while (count>1);

  s[0] = ' ';

  return s;
}


void print_result_with_item_string(uint64_t item_result, char** str_array)
{
	enum quest_enum_item item;
	int max_item;
	enum quest_enum_item_result result;
	char result_char;
	char results_str[MAX_LEN_STR] = {0,};
	ssize_t results_str_size = 0;

	if( item_result==0 ) {
		QUEST_PRINT(" NOT TESTED\n");
		return;
	}

	if( str_array == STR_ITEM )
		max_item = ITEM_ITEMSCOUNT;
	else if( str_array == STR_SUBITEM )
		max_item = SUBITEM_ITEMSCOUNT;
	
	for( item=1; item<max_item; item++ )
	{
		result = QUEST_GET_ITEM_SUBITEM_RESULT(item_result, item);
		switch (result) {
			case ITEM_RESULT_NONE:
				//result_char = 'I';
				//break;				
				continue;
			case ITEM_RESULT_INCOMPLETED:
				result_char = 'I';
				break;
			case ITEM_RESULT_FAIL:
				result_char = 'F';
				break;
			case ITEM_RESULT_PASS:
				result_char = 'P';
				break;
		}
		results_str_size += snprintf((char *)(results_str + results_str_size), MAX_LEN_STR - results_str_size,
									" %s(%c)", str_array[item], result_char);
	}

	QUEST_PRINT(" %s\n", results_str);
}


void quest_print_param_quest_data()
{
	QUEST_PRINT("======================\n");
	
	QUEST_PRINT("curr_step : %d\n", param_quest_data.curr_step);
	
	//QUEST_PRINT("smd_item_result    : %s\n", uint64ToBinary(param_quest_data.smd_item_result));
	QUEST_PRINT("smd_item_result    : (%d)\n", param_quest_data.smd_item_result);
	print_result_with_item_string(param_quest_data.smd_item_result, STR_ITEM);
	
	//QUEST_PRINT("cal_item_result    : %s\n", uint64ToBinary(param_quest_data.cal_item_result));
	QUEST_PRINT("cal_item_result    : (%d)\n", param_quest_data.cal_item_result);
	print_result_with_item_string(param_quest_data.cal_item_result, STR_ITEM);	
	
	//QUEST_PRINT("main_item_result   : %s\n", uint64ToBinary(param_quest_data.main_item_result));	
	QUEST_PRINT("main_item_result   :(%d)\n", param_quest_data.main_item_result);	
	print_result_with_item_string(param_quest_data.main_item_result, STR_ITEM);	

	//QUEST_PRINT("smd_subitem_result : %s\n", uint64ToBinary(param_quest_data.smd_subitem_result));
	QUEST_PRINT("smd_subitem_result :(%d)\n", param_quest_data.smd_subitem_result);
	print_result_with_item_string(param_quest_data.smd_subitem_result, STR_SUBITEM);
	
	QUEST_PRINT("hlos_count(%d) quefi_count(%d) suefi_count(%d) ddrscan_count(%d)\n",
 		param_quest_data.hlos_remained_count,
		param_quest_data.quefi_remained_count,
		param_quest_data.suefi_remained_count,
		param_quest_data.ddrscan_remained_count);
	
	QUEST_PRINT("err_codes : %x\n",	param_quest_data.err_codes);
	
	QUEST_PRINT("======================\n");
}

void quest_load_param_quest_data()
{
	if (!sec_get_param(param_index_quest, &param_quest_data))
		QUEST_PRINT("%s : failed\n", __func__);
	else
		QUEST_PRINT("%s : succeeded\n", __func__);	
}

void quest_sync_param_quest_data()
{
	if (!sec_set_param(param_index_quest, &param_quest_data))
		QUEST_PRINT("%s : failed\n", __func__);
	else
		QUEST_PRINT("%s : succeeded **********\n", __func__);
	quest_print_param_quest_data();
}

#if defined(CONFIG_SEC_DDR_SKP)
#define QUEST_REPEAT_CNT 3
#define DDR_SCAN_CNT     1
#else
#define QUEST_REPEAT_CNT 0
#define DDR_SCAN_CNT     0
#endif

void quest_clear_param_quest_data()
{
	param_quest_data.smd_item_result = 0;
	param_quest_data.smd_subitem_result = 0;
	param_quest_data.cal_item_result = 0;
	param_quest_data.main_item_result = 0;
	param_quest_data.curr_step = STEP_NONE;
	param_quest_data.hlos_remained_count = QUEST_REPEAT_CNT;
#if defined(CONFIG_SEC_QUEST_UEFI)	
	param_quest_data.quefi_remained_count = QUEST_REPEAT_CNT;
#endif
#if defined(CONFIG_SEC_QUEST_UEFI_ENHANCEMENT)
	param_quest_data.suefi_remained_count = QUEST_REPEAT_CNT;
#endif
	param_quest_data.ddrscan_remained_count = DDR_SCAN_CNT;
#if 0	
	param_quest_data.thermal = 0;
	param_quest_data.tested_clock = 0;
#endif	
	param_quest_data.err_codes = 0;
	quest_sync_param_quest_data();
}

void quest_load_param_quest_ddr_result_data()
{
	if (!sec_get_param(param_index_quest_ddr_result, &param_quest_ddr_result_data))
		QUEST_PRINT("%s : failed\n", __func__);
	else
		QUEST_PRINT("%s : succeeded\n", __func__);
}

void quest_load_param_api_gpio_test()
{
	if (!sec_get_param(param_index_api_gpio_test, &param_api_gpio_test))
		QUEST_PRINT("%s : failed\n", __func__);
	else
		QUEST_PRINT("%s : succeeded\n", __func__);
}

void quest_sync_param_api_gpio_test()
{
	if (!sec_set_param(param_index_api_gpio_test, &param_api_gpio_test))
		QUEST_PRINT("%s : failed\n", __func__);
	else
		QUEST_PRINT("%s : succeeded\n", __func__);		
}

void quest_load_param_api_gpio_test_result()
{
	if (!sec_get_param(param_index_api_gpio_test_result, param_api_gpio_test_result))
		QUEST_PRINT("%s : failed\n", __func__);
	else
		QUEST_PRINT("%s : succeeded\n", __func__);
}

void quest_sync_param_api_gpio_test_result()
{
	if (!sec_set_param(param_index_api_gpio_test_result, param_api_gpio_test_result))
		QUEST_PRINT("%s : failed\n", __func__);
	else
		QUEST_PRINT("%s : succeeded\n", __func__);	
}


//////////////////////////////////////////////////


