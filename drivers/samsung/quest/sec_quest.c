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

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/sec_class.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <linux/sec_debug.h>
#include <linux/sec_param.h>
#include <linux/types.h>
#include <linux/sec_quest.h>
#include <linux/workqueue.h>
#include <linux/jiffies.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/notifier.h>


extern unsigned int lpcharge;

static struct kobj_uevent_env quest_uevent;
static int erase_pass;
static int quest_step = -1;	/* flag for quest test mode */
struct mutex uefi_parse_lock; 
struct mutex hlos_parse_lock; 
struct mutex common_lock;
struct mutex qdaf_call_lock;


static void print_param_quest_data(struct param_quest_t param_quest_data)
{
#if defined(CONFIG_SEC_QUEST_UEFI)
	QUEST_PRINT(
		"%s : magic %x, hlos cnt %d, uefi cnt %d, ddr cnt %d, smd_test_result 0x%2X, quest_uefi_result 0x%2X, ddr result 0x%2X, ddr_test_mode 0x%3X, quest_step %d\n",
		__func__, param_quest_data.magic,
		param_quest_data.quest_hlos_remain_count,
		param_quest_data.quest_uefi_remain_count,
		param_quest_data.ddrtest_remain_count,		
		param_quest_data.smd_test_result,
		param_quest_data.quest_uefi_result,
		param_quest_data.ddrtest_result,
		param_quest_data.ddrtest_mode,
		param_quest_data.quest_step);   
#else
	QUEST_PRINT(
		"%s : magic %x, hlos cnt %d, ddr cnt %d, smd_test_result 0x%2X, ddr result 0x%2X, ddr_test_mode 0x%3X, quest_step %d\n",
		__func__, param_quest_data.magic,
		param_quest_data.quest_hlos_remain_count,
		param_quest_data.ddrtest_remain_count,		
		param_quest_data.smd_test_result,
		param_quest_data.ddrtest_result,
		param_quest_data.ddrtest_mode,
		param_quest_data.quest_step);   
#endif
}


// get the result of ddr_scan at smd, cal or main
static int get_ddr_scan_result(int which_quest)
{
	struct param_quest_t param_quest_data;
	int ddr_result = DDRTEST_INCOMPLETED;

	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		return ddr_result;
	}	
	
	ddr_result = GET_DDR_TEST_RESULT(which_quest, param_quest_data.ddrtest_result);
	QUEST_PRINT("%s : (step=%d) (ddr_result=%d)\n", __func__, which_quest, ddr_result);

	return ddr_result;
}


// update bitmap_item_result
static void set_ddr_scan_result_for_smd()
{
	struct param_quest_t param_quest_data;
	int ddr_result = DDRTEST_INCOMPLETED;
	
	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		return;
	}

	ddr_result = get_ddr_scan_result(SMD_QUEST);

	if (ddr_result == DDRTEST_PASS) {
		param_quest_data.bitmap_item_result |=
		    TEST_ITEM_RESULT(SMD_QUEST, DDR_SCAN, QUEST_ITEM_TEST_PASS);
	} else if ( ddr_result == DDRTEST_FAIL ) {
		param_quest_data.bitmap_item_result |=
		    TEST_ITEM_RESULT(SMD_QUEST, DDR_SCAN, QUEST_ITEM_TEST_FAIL);
	} 

	if (!sec_set_param(param_index_quest,&param_quest_data)) {
		QUEST_PRINT("%s : fail - set param!! param_quest_data\n",__func__);
	}
}


// check test_result.csv
static int get_quest_hlos_result(int which_quest)
{
	int fd = 0;
	int hlos_result = TOTALTEST_UNKNOWN;
	char buf[512] = { '\0', };
	mm_segment_t old_fs = get_fs();

	mutex_lock(&hlos_parse_lock);

	set_fs(KERNEL_DS);

	switch (which_quest) {
	case SMD_QUEST:
		fd = sys_open(SMD_QUEST_RESULT, O_RDONLY, 0);
		break;		
	case CAL_QUEST:
		fd = sys_open(CAL_QUEST_RESULT, O_RDONLY, 0);
		break;
	case MAIN_QUEST:
		fd = sys_open(MAIN_QUEST_RESULT, O_RDONLY, 0);
		break;
	default:
		break;
	}

	if (fd >= 0) {
		int found = 0;

		printk(KERN_DEBUG);

		while (sys_read(fd, buf, 512)) {
			char *ptr;
			char *div = "\n";
			char *tok = NULL;

			ptr = buf;
			while ((tok = strsep(&ptr, div)) != NULL) {

				if ((strstr(tok, "FAIL"))) {
					QUEST_PRINT("%s : (step=%d) The result is FAIL\n",__func__, which_quest);
					hlos_result = TOTALTEST_FAIL;
					found = 1;
					break;
				}

				if ((strstr(tok, "AllTestDone"))) {
					QUEST_PRINT("%s : (step=%d) The result is PASS\n",__func__, which_quest);
					hlos_result = TOTALTEST_PASS;
					found = 1;
					break;
				}
			}

			if (found)
				break;
		}

		if (!found) {
			QUEST_PRINT("%s : (step=%d) no result string\n",__func__, which_quest);
			hlos_result = TOTALTEST_NO_RESULT_STRING;
		}

		sys_close(fd);
	} else {
		QUEST_PRINT("%s : (step=%d) The result file is not existed (fd=%d)\n",__func__, which_quest, fd);
		hlos_result = TOTALTEST_NO_RESULT_FILE;
	}

	set_fs(old_fs);

	mutex_unlock(&hlos_parse_lock);

	return hlos_result;
}


// check bitmap_item_result
static int get_smd_item_result(struct param_quest_t param_quest_data, char *buf, int piece)
{
	int iCnt, failed_cnt=0;
	unsigned int smd_quest_result;

	if( param_quest_data.bitmap_item_result == 0 ) {
		QUEST_PRINT("%s : param_quest_data.bitmap_item_result = 0\n", __func__);
		goto err_out;
	}	

	smd_quest_result = param_quest_data.bitmap_item_result;

	QUEST_PRINT("%s : param_quest_data.bitmap_item_result=%u,", __func__,
		smd_quest_result);

	if (piece == ITEM_CNT) {
		for (iCnt = 0; iCnt < ITEM_CNT; iCnt++) {
			switch (smd_quest_result & 0x3) {
			case 1:
				strcat(buf, "[F]");
				failed_cnt++;
				break;
			case 2:
				strcat(buf, "[P]");
				break;
			default:
				strcat(buf, "[X]");
				break;
			}

			smd_quest_result >>= 2;
		}
	} else {
		smd_quest_result >>= 2 * piece;
		switch (smd_quest_result & 0x3) {
		case 1:
			strlcpy(buf, "FAIL", sizeof(buf));
			failed_cnt++;
			break;
		case 2:
			strlcpy(buf, "PASS", sizeof(buf));
			break;
		default:
			strlcpy(buf, "NA", sizeof(buf));
			break;
		}
	}

	return failed_cnt;
err_out:
	return -1;
}


static void do_quest()
{
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char *envp[5] = { 
		"HOME=/", 
		"PATH=/system/bin/quest:/system/bin:/system/xbin", 
		"ANDROID_DATA=/data", 
		"ANDROID_ROOT=/system", 
		NULL };
	int ret_userapp;
	char log_path[50] = { '\0', };

	switch (quest_step) {
		case SMD_QUEST:
			argv[0] = SMD_QUEST_PROG;
			snprintf(log_path, 50, "logPath:%s\0", SMD_QUEST_LOGPATH); 
			argv[1] = log_path;
			QUEST_PRINT("SMD_QUEST, quest_step : %d", quest_step);
			break;
		case CAL_QUEST:
			argv[0] = CAL_QUEST_PROG;
			snprintf(log_path, 50, "logPath:%s\0", CAL_QUEST_LOGPATH); 
			argv[1] = log_path;
			argv[2] = "Reboot";
			QUEST_PRINT("CAL_QUEST_HLOS, quest_step : %d", quest_step);
			QUEST_PRINT("reboot option enabled \n");
			break;		
		case MAIN_QUEST:
			argv[0] = MAIN_QUEST_PROG;
			snprintf(log_path, 50, "logPath:%s\0", MAIN_QUEST_LOGPATH); 
			argv[1] = log_path;
			argv[2] = "Reboot";		
			QUEST_PRINT("MAIN_QUEST, quest_step : %d\n", quest_step);
			QUEST_PRINT("reboot option enabled \n");
			break;
		default:
			QUEST_PRINT("Invalid quest value, quest_step : %d\n", quest_step);
			return;
	}

	ret_userapp = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_EXEC);

	if (!ret_userapp) {
		QUEST_PRINT("%s is executed. ret_userapp = %d\n", argv[0],
			  ret_userapp);

		if (erase_pass) erase_pass = 0;
	} else {
		QUEST_PRINT("%s is NOT executed. ret_userapp = %d\n", argv[0],
			  ret_userapp);
		quest_step = -1;
	}
}


static void remove_specific_logs(char* remove_log_path)
{
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char *envp[5] = { 
		"HOME=/", 
		"PATH=/system/bin/quest:/system/bin:/system/xbin", 
		"ANDROID_DATA=/data", 
		"ANDROID_ROOT=/system", 
		NULL };
	int ret_userapp;

	QUEST_PRINT("%s : will delete log files at \n", __func__, remove_log_path);
	
	argv[0] = ERASE_QUEST_PRG;
	argv[1] = remove_log_path;
	ret_userapp = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	if (!ret_userapp) {
		QUEST_PRINT("%s is executed. ret_userapp = %d\n", argv[0], ret_userapp);
	} else {
		QUEST_PRINT("%s is NOT executed. ret_userapp = %d\n", argv[0], ret_userapp);
	}
}


static void make_debugging_logs(char* options)
{
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char *envp[5] = { 
		"HOME=/", 
		"PATH=/system/bin/quest:/system/bin:/system/xbin", 
		"ANDROID_DATA=/data", 
		"ANDROID_ROOT=/system", 
		NULL };
	int ret_userapp;

	QUEST_PRINT("%s : will make debugging logs with options %s\n", __func__, options);
	
	argv[0] = QUEST_DEBUGGING_PRG;
	argv[1] = options;
	ret_userapp = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	if (!ret_userapp) {
		QUEST_PRINT("%s is executed. ret_userapp = %d\n", argv[0], ret_userapp);
	} else {
		QUEST_PRINT("%s is NOT executed. ret_userapp = %d\n", argv[0], ret_userapp);
	}
}


static void print_reset_info()
{
	char options[50] = { '\0', };

	// refer to qpnp_pon_reason (index=boot_reason-1)
	QUEST_PRINT("%s : boot_reason was %d\n", __func__, boot_reason);

	// reset history
	snprintf(options, 50, "action:resethist\0"); 
	make_debugging_logs(options);
}


#if defined(CONFIG_SEC_QUEST_UEFI)

static void arrange_quest_logs(char* log_path)
{
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char *envp[5] = { 
		"HOME=/", 
		"PATH=/system/bin/quest:/system/bin:/system/xbin", 
		"ANDROID_DATA=/data", 
		"ANDROID_ROOT=/system", 
		NULL };
	int ret_userapp;

	QUEST_PRINT("%s : will arrange quest log files at (%s) before test\n", 
		__func__, log_path);
	
	argv[0] = ARRANGE_QUEST_LOGS_PRG;
	argv[1] = log_path;
	ret_userapp = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	if (!ret_userapp) {
		QUEST_PRINT("%s is executed. ret_userapp = %d\n", argv[0], ret_userapp);
	} else {
		QUEST_PRINT("%s is NOT executed. ret_userapp = %d\n", argv[0], ret_userapp);
	}
}


static void move_questresult_to_sub_dir(int quest_step)
{
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char *envp[5] = { 
		"HOME=/", 
		"PATH=/system/bin/quest:/system/bin:/system/xbin", 
		"ANDROID_DATA=/data", 
		"ANDROID_ROOT=/system", 
		NULL };
	int ret_userapp;

	argv[0] = MOVE_QUESTRESULT_PRG;
	switch (quest_step) {
		//case SMD_QUEST: {
		//	argv[1] = SMD_QUEST_LOGPATH;
		//} break;
		case CAL_QUEST: {
			argv[1] = CAL_QUEST_LOGPATH;
		} break;
		case MAIN_QUEST: {
			argv[1] = MAIN_QUEST_LOGPATH;
		} break;
	}		
	QUEST_PRINT("%s : will move questresult.txt to %s\n", __func__, argv[1]);	

	ret_userapp = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
	if (!ret_userapp) {
		QUEST_PRINT("%s is executed. ret_userapp = %d\n", argv[0], ret_userapp);
	} else {
		QUEST_PRINT("%s is NOT executed. ret_userapp = %d\n", argv[0], ret_userapp);
	}	
}


static int get_quest_uefi_result(char* uefi_log_path, int will_print)
{
	int i, fd = 0;
	int uefi_result = TOTALTEST_UNKNOWN;
	char buf[512] = { '\0', };
	mm_segment_t old_fs = get_fs();
	int test_done = 0;
	int check_strings = sizeof(parsing_info)/sizeof(parsing_info[0]);
	char questresult_file[50] = { '\0', };

	mutex_lock(&uefi_parse_lock);
	
	set_fs(KERNEL_DS);

	snprintf(questresult_file, 50, "%s/%s\0", uefi_log_path, "questresult.txt"); 
	QUEST_PRINT("%s : will parse %s\n",__func__, questresult_file);
	
	fd = sys_open(questresult_file, O_RDONLY, 0);
	
	if (fd < 0) {
		QUEST_PRINT("%s : The result file of quest_uefi is not existed (fd=%d)\n", __func__, fd);
		uefi_result = TOTALTEST_NO_RESULT_FILE;
		goto err_out;
	}

	for(i=0; i<check_strings; i++) {
		parsing_info[i].result = 0;
	}

	while (sys_read(fd, buf, 512)) {
		char *ptr;
		char *div = "\n";
		char *tok = NULL;
		
		ptr = buf;
		while ((tok = strsep(&ptr, div)) != NULL) {

			if( will_print )
				QUEST_PRINT("%s : *** %s\n", __func__, tok);
			
			for(i=0; i<check_strings; i++) {
				
				if( !parsing_info[i].result && strstr(tok, parsing_info[i].item_name) ) {
					
					//QUEST_PRINT("%s : debug : tok=%s\n", __func__, tok);
						
					if( strstr(tok, PASS_STR) ) {
						//QUEST_PRINT("%s : %s result => %s\n", 
						//	__func__, parsing_info[i].item_name, PASS_STR);
						parsing_info[i].result = QUEST_ITEM_TEST_PASS;
					} else if( strstr(tok, FAIL_STR) ) {
						//QUEST_PRINT("%s : %s result => %s\n", 
						//	__func__, parsing_info[i].item_name, FAIL_STR);
						parsing_info[i].result = QUEST_ITEM_TEST_FAIL;
					} else if( i == check_strings-1 ) {	// for checking test done
						//QUEST_PRINT("%s : test done\n", __func__);
						test_done = 1;
					}			
					
					break;
				}	
				
			}
			if( test_done ) break;
		}				
	}

	if( !test_done ) {
		QUEST_PRINT("%s : There is no \"QUEST Test Done!\"\n", __func__);
		uefi_result = TOTALTEST_NO_RESULT_STRING;
	} else {
		for(i=0; i<check_strings; i++) {
			if( parsing_info[i].result == QUEST_ITEM_TEST_FAIL ) {
				QUEST_PRINT("%s : FAIL (%s : %d)\n", __func__, 
					parsing_info[i].item_name, parsing_info[i].result);
				uefi_result = TOTALTEST_FAIL;
				break;
			}
		}
		if( i==check_strings ) {
			QUEST_PRINT("%s : PASS\n", __func__);
			uefi_result = TOTALTEST_PASS;			
		}
	}

	sys_close(fd);

err_out:
	set_fs(old_fs);	
	mutex_unlock(&uefi_parse_lock);
	return uefi_result;
}

#endif


static int call_qdaf_from_quest_driver(int cmd, int wait_val)
{
	int ret_userapp;
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char qdaf_cmd[BUFF_SZ];
	char *envp[5] = { 
		"HOME=/", 
		"PATH=/system/bin/quest:/system/bin:/system/xbin", 
		"ANDROID_DATA=/data", 
		"ANDROID_ROOT=/system", 
		NULL };		

	mutex_lock(&qdaf_call_lock);

	argv[0] = QDAF_PROG;
	snprintf(qdaf_cmd, BUFF_SZ, "%d", cmd);
	argv[1] = qdaf_cmd;
	
	qdaf_cur_cmd_mode = cmd;

	ret_userapp = call_usermodehelper(argv[0], argv, envp, wait_val);
	QUEST_PRINT("%s : is called (%d,%d) and returned (%d)\n",
		__func__, cmd, wait_val, ret_userapp);

	mutex_unlock(&qdaf_call_lock);	
	
	return ret_userapp;
}


static ssize_t show_quest_acat(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct param_quest_t param_quest_data;
#if defined(CONFIG_SEC_QUEST_UEFI)			
	int uefi_result = TOTALTEST_UNKNOWN;
	int hlos_result = TOTALTEST_UNKNOWN;
	mm_segment_t old_fs = get_fs();
	int fd = 0;	
	int can_trigger_panic = 0;	
#endif	
	int total_result = TOTALTEST_UNKNOWN;
	
	char options[50] = { '\0', };
	

	mutex_lock(&common_lock);

	QUEST_PRINT("%s : at boot time, second/fourth call from factory app (read sec_nad/nad_acat)\n", __func__);

	// 0. print param informations
	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		goto err_out;
	}
	print_param_quest_data(param_quest_data);


// 1. parsing the result of quest_uefi
#if defined(CONFIG_SEC_QUEST_UEFI)	

	// to parse hlos result
	hlos_result = get_quest_hlos_result(CAL_QUEST);

	// to arrange files
	{
		static arranged = 0;
		if( (!arranged) && (hlos_result!=TOTALTEST_FAIL)
			&& (param_quest_data.quest_step == CAL_QUEST)
			&& (param_quest_data.quest_hlos_remain_count>0) ) {
			QUEST_PRINT("%s : will arrange CAL directory for next executing", __func__);
			arranged = 1;			
			arrange_quest_logs(CAL_QUEST_LOGPATH);
		}

		if ( param_quest_data.quest_step == CAL_QUEST )
		{
			set_fs(KERNEL_DS);
			if ( (fd = sys_open(UEFI_QUESTRESULT_FILE, O_RDONLY, 0))>=0 ) {
				QUEST_PRINT("%s : the questresult.txt exists", __func__);
				can_trigger_panic = 1;
			}else
				QUEST_PRINT("%s : no questresult.txt", __func__);
			move_questresult_to_sub_dir(CAL_QUEST);	// to move UefiLog always let's consider case of no questresult also
			set_fs(old_fs);
		}
	}

	// to parse uefi result
	uefi_result = get_quest_uefi_result(CAL_QUEST_LOGPATH, 1);

	// to integrate results
	// In case of cal quest, we can run only quest_uefi or both quest_uefi and quest_hlos.
	// At factory line, we run only quest_uefi using cal 1time button,
	// so let's consider the result of quest_uefi only except for the case of quest_hlos failure.
	if( uefi_result==TOTALTEST_FAIL || hlos_result==TOTALTEST_FAIL ) 
		total_result = TOTALTEST_FAIL;
	else  
		total_result = uefi_result;
	QUEST_PRINT("%s : uefi_result(%d) / hlos_result(%d) / total_result(%d)\n",
		__func__, uefi_result, hlos_result, total_result );
 	
#else
	total_result = get_quest_hlos_result(CAL_QUEST); // get quest_hlos result
#endif


	// 2.1. update param
	if( param_quest_data.quest_step == CAL_QUEST ) {

#if defined(CONFIG_SEC_QUEST_UEFI)			
		param_quest_data.quest_uefi_result
			= SET_UEFI_RESULT(CAL_QUEST, param_quest_data.quest_uefi_result, uefi_result);

		if( param_quest_data.quest_hlos_remain_count==0 && 
			param_quest_data.quest_uefi_remain_count==0 ) {

			QUEST_PRINT("%s : cal step finished\n", __func__);
			param_quest_data.quest_step = -1;
		}
#else
		if( param_quest_data.quest_hlos_remain_count==0 ) {
			QUEST_PRINT("%s : cal step finished\n", __func__);
			param_quest_data.quest_step = -1;
		}		
#endif		
		
		if (!sec_set_param(param_index_quest, &param_quest_data)) {
			QUEST_PRINT("%s : fail - set param!! param_quest_data\n", __func__);
			goto err_out;
		}	
	}
	

	// 3. result processing
	switch (total_result) {
		case TOTALTEST_PASS: {
			QUEST_PRINT("ACAT QUEST Passed\n");
			mutex_unlock(&common_lock);
			return snprintf(buf, BUFF_SZ, "OK_ACAT_NONE\n");
		} break;

		case TOTALTEST_FAIL: {
#if defined(CONFIG_SEC_QUEST_UEFI)			
			// in case of CAL, the device can enter to ramdump mode using nad_end as soon as hlos failed
			// so, let's consider only the case of uefi failure
			if( uefi_result==TOTALTEST_FAIL && can_trigger_panic ) {

				QUEST_PRINT("%s : quest_uefi result was failed, so trigger panic\n", __func__ );
				QUEST_PRINT("%s : current step is CAL and panic will occur, so initialize quest_step=-1\n",__func__);

				// if failed, let's do not run quest_uefi any more.
				param_quest_data.quest_uefi_remain_count = 0;
				param_quest_data.quest_hlos_remain_count = 0;
				param_quest_data.quest_step = -1;
				if (!sec_set_param(param_index_quest, &param_quest_data)) {
					QUEST_PRINT("%s : fail - set param!! param_quest_data\n", __func__);
					goto err_out;
				}		

				// trigger panic
				mutex_unlock(&common_lock);				
				panic("quest_uefi failed");				
			}
			QUEST_PRINT("%s : uefi_result(%d), current quest_step(%d), so will not trigger panic\n", 
				__func__, uefi_result, param_quest_data.quest_step );			
#else
			param_quest_data.quest_step = -1;
			if (!sec_set_param(param_index_quest, &param_quest_data)) {
				QUEST_PRINT("%s : fail - set param!! param_quest_data\n", __func__);
				goto err_out;
			}			
#endif

			QUEST_PRINT("ACAT QUEST fail\n");
			mutex_unlock(&common_lock);
			return snprintf(buf, BUFF_SZ, "NG_ACAT_ASV\n"); 
		} break;

		default: {
			if( param_quest_data.quest_step == CAL_QUEST ) {
				QUEST_PRINT("%s : total_result = %d, so let's make lastkmsg\n", __func__, total_result );
				snprintf(options, 50, "action:lastkmsg output_log_path:%s\0", CAL_QUEST_LOGPATH); 
				make_debugging_logs(options);
			}

			QUEST_PRINT("ACAT QUEST No Run\n");
			mutex_unlock(&common_lock);
			return snprintf(buf, BUFF_SZ, "OK\n");
		}
	}		

err_out:
	mutex_unlock(&common_lock);
	return snprintf(buf, BUFF_SZ, "UNKNOWN\n");	
}

static ssize_t store_quest_acat(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct param_quest_t param_quest_data;
	int ret = -1;
	int idx = 0;
	int quest_loop_count, dram_loop_count;
	char temp[QUEST_BUFF_SIZE * 3];
	char quest_cmd[QUEST_CMD_LIST][QUEST_BUFF_SIZE];
	char *quest_ptr, *string;

	mutex_lock(&common_lock);

	if( erase_pass ) {
		QUEST_PRINT("%s : store_quest_erase was called. so let's ignore this call\n", __func__);
		mutex_unlock(&common_lock);
		return count;
	}

	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		goto err_out;
	}
	print_param_quest_data(param_quest_data);

#if !defined(CONFIG_SEC_SKP)
	// check if smd quest_hlos should start
	if (param_quest_data.magic != QUEST_SMD_MAGIC) {	
		// <======================== 1st boot at right after SMD D/L done
		QUEST_PRINT("1st boot at SMD\n");
		param_quest_data.magic = QUEST_SMD_MAGIC;
		param_quest_data.smd_test_result = 0;
		param_quest_data.bitmap_item_result = 0;		
		param_quest_data.quest_hlos_remain_count = 0;
		param_quest_data.ddrtest_remain_count = 0;
		param_quest_data.quest_step = SMD_QUEST;
		if (!sec_set_param(param_index_quest,&param_quest_data)) {
			QUEST_PRINT("%s : fail - set param!! param_quest_data\n",__func__);
			goto err_out;
		}
		
		quest_step = SMD_QUEST;
		mutex_unlock(&common_lock);
		do_quest();

		// check smd ddr test and update bitmap_item_result here!!!
		set_ddr_scan_result_for_smd();
		
		return count;
	}	
#endif

	QUEST_PRINT("%s: buf(%s) count(%d)\n", __func__, buf, (int)count);
	if ((int)count < QUEST_BUFF_SIZE) {
		mutex_unlock(&common_lock);
		return -EINVAL;
	}

	/* Copy buf to quest temp */
	strlcpy(temp, buf, QUEST_BUFF_SIZE * 3);
	string = temp;

	while (idx < QUEST_CMD_LIST) {
		quest_ptr = strsep(&string, ",");
		strlcpy(quest_cmd[idx++], quest_ptr, QUEST_BUFF_SIZE);
	}	

	if (!strncmp(buf, "nad_acat", 8)) {

		// get quest_loop_count and dram_loop_count
		ret = sscanf(quest_cmd[1], "%d\n", &quest_loop_count);
		if (ret != 1) return -EINVAL;
		ret = sscanf(quest_cmd[2], "%d\n", &dram_loop_count);
		if (ret != 1) return -EINVAL;
		QUEST_PRINT("%s : nad_acat%d,%d\n", 
			__func__, quest_loop_count, dram_loop_count);

		if (!quest_loop_count && !dram_loop_count) {	
			// both counts are 0, means 
			// 1. testing refers to current remain_count

			// stop retrying when failure occur during retry test at ACAT/15test
#if defined(CONFIG_SEC_QUEST_UEFI)			
			if ( get_quest_hlos_result(CAL_QUEST)== TOTALTEST_FAIL 
					|| get_quest_uefi_result(CAL_QUEST_LOGPATH, 0)==TOTALTEST_FAIL )
#else
			if ( get_quest_hlos_result(CAL_QUEST)==TOTALTEST_FAIL )
#endif					
			{
				QUEST_PRINT("%s : we did not initialize params when quest_hlos failed\n", __func__);
				QUEST_PRINT("%s : current step is CAL and panic occurred, so force to initialize params\n",__func__);				

				param_quest_data.quest_hlos_remain_count = 0;
#if defined(CONFIG_SEC_QUEST_UEFI)					
				param_quest_data.quest_uefi_remain_count = 0;
#endif
				param_quest_data.ddrtest_remain_count = 0;
				param_quest_data.quest_step = -1;
				if (!sec_set_param(param_index_quest,&param_quest_data)) {
					QUEST_PRINT("%s : fail - set param!! param_quest_data\n",__func__);
					goto err_out;
				}
			}

			// QUEST count still remain
#if defined(CONFIG_SEC_QUEST_UEFI)			
			if( (param_quest_data.quest_hlos_remain_count > 0) &&
				(param_quest_data.quest_hlos_remain_count > param_quest_data.quest_uefi_remain_count))
#else
			if( (param_quest_data.quest_hlos_remain_count > 0) )
#endif
			{
				QUEST_PRINT("%s : quest_hlos_remain_count = %d, ddrtest_remain_count = %d\n",
					__func__, param_quest_data.quest_hlos_remain_count, 
					param_quest_data.ddrtest_remain_count);

				// ongoing quest_step (can skip)
				param_quest_data.quest_step = CAL_QUEST;
				if( param_quest_data.quest_hlos_remain_count >= 1 )		
					param_quest_data.quest_hlos_remain_count--;
				
				if (!sec_set_param(param_index_quest, &param_quest_data)) {
					QUEST_PRINT("%s : fail - set param!! param_quest_data\n",__func__);
					goto err_out;
				}

				quest_step = CAL_QUEST;
				mutex_unlock(&common_lock);
				do_quest();
			}
		}
		else {	
			// not (0,0) means 
			// 1. new test count came, 
			// 2. so overwrite the remain_count, 
			// 3. and not reboot by itsself, 
			// 4. reboot cmd will come from outside like factory PGM

			/*--- set quest_uefi and quest_hlos remain count ---*/
#if defined(CONFIG_SEC_QUEST_UEFI)
			param_quest_data.quest_uefi_remain_count = quest_loop_count;

			// run quest_hlos if quest_loop_count>1
			if( quest_loop_count > 1 )
				param_quest_data.quest_hlos_remain_count = quest_loop_count;
#else
			param_quest_data.quest_hlos_remain_count = quest_loop_count;
#endif

			param_quest_data.quest_step = CAL_QUEST;

			/*--- set ddrtest_mode ---*/
			if( (param_quest_data.ddrtest_remain_count = dram_loop_count) > 0 )
				param_quest_data.ddrtest_mode = UPLOAD_CAUSE_QUEST_DDR_TEST_CAL;

			/*--- set additional things for skp scenario ---*/		
#if defined(CONFIG_SEC_SKP)
			param_quest_data.quest_uefi_remain_count = quest_loop_count+5;
			param_quest_data.quest_hlos_remain_count = 1;
			param_quest_data.magic = QUEST_SMD_MAGIC;
			param_quest_data.quest_step = SKP_QUEST;
#endif

			if (!sec_set_param(param_index_quest, &param_quest_data)) {
				QUEST_PRINT("%s : fail - set param!! param_quest_data\n",__func__);
				goto err_out;
			}
			
			// remove cal logs before new cal test
			remove_specific_logs("cal");

			QUEST_PRINT("%s : new cmd : quest_hlos_remain_count = %d, ddrtest_remain_count = %d\n",
				__func__, 
				param_quest_data.quest_hlos_remain_count, 
				param_quest_data.ddrtest_remain_count);
		}
	}else
		QUEST_PRINT("%s : wrong arguments\n", __func__);

err_out:
	mutex_unlock(&common_lock);
	return count;
}

static DEVICE_ATTR(nad_acat, S_IRUGO | S_IWUSR, show_quest_acat, store_quest_acat);

#if defined(CONFIG_SEC_SKP)
static void param_quest_init()
{
	struct param_quest_t param_quest_data;
	
    if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
	}
	print_param_quest_data(param_quest_data);

	QUEST_PRINT("%s : param_quest_init\n",__func__);

	param_quest_data.magic = 0;	
	param_quest_data.quest_hlos_remain_count = 0;
	param_quest_data.quest_uefi_remain_count = 0;
	param_quest_data.quest_step = -1;

	if (!sec_set_param(param_index_quest,&param_quest_data)) {
		QUEST_PRINT("%s : fail - set param!! param_quest_data\n",__func__);
	}
}
#endif

// parsing result of quest_smd
static ssize_t show_quest_stat(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct param_quest_t param_quest_data;
	int hlos_result = TOTALTEST_UNKNOWN;
	int ddr_result = DDRTEST_INCOMPLETED;
	int first_check = 0;
	char options[50] = { '\0', };
	char strResult[BUFF_SZ-14] = { '\0', };
	char tempstrResult[BUFF_SZ-14] = { '\0', };
	int failed_cnt = 0;

	// 0.0.update nad_result for qdaf
	// this will call store_smd_quest_result and try to acquire common_lock
	// so let's call this before acquiring common_lock.
	/*------------ for QDAF ------------*/	
	call_qdaf_from_quest_driver(QUEST_QDAF_ACTION_RESULT_TO_NAD_RESULT, UMH_WAIT_PROC);
	/*--------------------------------*/	

	mutex_lock(&common_lock);

	QUEST_PRINT("%s : at boot time, third call from factory app (read sec_nad/nad_stat)\n", __func__);

	print_reset_info();	

	// 0. print param informations
	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		goto err_out;
	}
	print_param_quest_data(param_quest_data);
	if( param_quest_data.smd_test_result == TOTALTEST_UNKNOWN )
		first_check = 1;		

#if defined(CONFIG_SEC_SKP)
	{
		int uefi_result = TOTALTEST_UNKNOWN;
		static int checked = 0;		

		if( checked++ > 0 ) {
			QUEST_PRINT("%s : already checked for skp, so just return\n", __func__ );
			mutex_unlock(&common_lock);			
			return snprintf(buf, BUFF_SZ, "OK_2.0\n");
		}		
		
		QUEST_PRINT("%s : if questresult logs exist, let's move them to cal directory\n", __func__);
		move_questresult_to_sub_dir(CAL_QUEST);
		uefi_result = get_quest_uefi_result(CAL_QUEST_LOGPATH, 1);
		param_quest_data.quest_uefi_result = 
					SET_UEFI_RESULT(CAL_QUEST, param_quest_data.quest_uefi_result, uefi_result);
		if (!sec_set_param(param_index_quest, &param_quest_data)) {
				QUEST_PRINT("%s : fail - set param!! param_quest_data\n",__func__);
				goto err_out;
		}
		
		if ( uefi_result == TOTALTEST_NO_RESULT_FILE ) {
			param_quest_init();
			QUEST_PRINT("%s : maybe first boot, so dont' run quest_uefi\n", __func__ );
			mutex_unlock(&common_lock);
			return snprintf(buf, BUFF_SZ, "OK_2.0\n");
		}else if ( (uefi_result == TOTALTEST_FAIL) ||
					(uefi_result == TOTALTEST_NO_RESULT_STRING) ) {
			if( param_quest_data.quest_step == SKP_QUEST ) {
				QUEST_PRINT("%s : quest_uefi result was abnormal, so trigger panic\n", __func__ );
				param_quest_init();					
				mutex_unlock(&common_lock);
				panic("quest_uefi failed");
			}else {
				QUEST_PRINT("%s : already entered to ramdump mode and initialized param, so return\n", __func__);
				mutex_unlock(&common_lock);
				return snprintf(buf, BUFF_SZ, "OK_2.0\n");
			}		
		}else {
		
			QUEST_PRINT("%s : uefi_remain_count remained(%d)\n", 
					__func__, param_quest_data.quest_uefi_remain_count );
		
			if ( param_quest_data.quest_uefi_remain_count > 0 ) {
				
				QUEST_PRINT("%s : quest_uefi will be started\n", __func__);

				param_quest_data.quest_uefi_result = 
					SET_UEFI_RESULT(CAL_QUEST, param_quest_data.quest_uefi_result, 0x0);
				param_quest_data.magic = QUEST_SMD_MAGIC;
				if (!sec_set_param(param_index_quest, &param_quest_data)) {
					QUEST_PRINT("%s : fail - set param!! param_quest_data\n",__func__); 
					goto err_out;		
				}
				mutex_unlock(&common_lock);
				kernel_restart(NULL);			
				// not reached
			}else {
				param_quest_data.quest_step = -1;
				if (!sec_set_param(param_index_quest, &param_quest_data)) {
					QUEST_PRINT("%s : fail - set param!! param_quest_data\n",__func__); 
					goto err_out;		
				}
				QUEST_PRINT("%s : quest_uefi done, so will run quest_hlos\n", __func__ );
				mutex_unlock(&common_lock);
				return snprintf(buf, BUFF_SZ, "OK_2.0\n");
			}
		}
	}
#endif

	// 1. if smd magic is not written, NOT_TESTED
	if (param_quest_data.magic != QUEST_SMD_MAGIC) {
		QUEST_PRINT("SMD QUEST NOT_TESTED\n");
		mutex_unlock(&common_lock);
		return snprintf(buf, BUFF_SZ, "NOT_TESTED\n");
	}


	// 2.1. parsing the result of quest_hlos
	hlos_result = get_quest_hlos_result(SMD_QUEST);

	// 2.2. get smd result from nad_result and combine the result of quest_hlos and qdaf
	failed_cnt = get_smd_item_result(param_quest_data, tempstrResult, QDAF);
	if( failed_cnt==-1 ) {
		QUEST_PRINT("%s : wrong param_quest_data state\n", __func__);
		//mutex_unlock(&common_lock);
		//panic("abnormal param_quest_data state");
	}else if( failed_cnt > 0 ) {
		// case1) smd quest_hlos -> upload -> reboot
		//		=> hlos_result=no_result_string, qdaf not executed so failed_cnt will not >0
		// case2) smd_quest_hlos -> PASS/FAIL -> qdaf failed
		//		=> hlos_result=PASS/FAIL, qdaf failed so we have to update smd test result as FAIL
		QUEST_PRINT("%s : qdaf failed, so let's update smd total test result as FAIL\n",__func__);
		hlos_result = TOTALTEST_FAIL;
	}

	// 2.3. if smd ddr scan was not completed
	ddr_result = get_ddr_scan_result(SMD_QUEST);

	// 2.4. in case of SMD, let's save result to param
	param_quest_data.smd_test_result = hlos_result;
	if (!sec_set_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - set param!! param_quest_data\n",__func__);
		goto err_out;
	}	
	
	// update strResult using ITEM_CNT
	get_smd_item_result(param_quest_data, strResult, ITEM_CNT);	

	mutex_unlock(&common_lock);		

	
	// 3. result processing	
	switch (hlos_result) {
		case TOTALTEST_PASS: {
			if( ddr_result == DDRTEST_INCOMPLETED ) {
				QUEST_PRINT("%s : SMD QUEST PASS but DDR SCAN INCOMPLETED\n", __func__);
				return snprintf(buf, BUFF_SZ, "RE_WORK\n");
			}
			
			// there is "AllTestDone" at SMD/test_result.csv
			QUEST_PRINT("%s : SMD QUEST PASS\n", __func__);
			return snprintf(buf, BUFF_SZ, "OK_2.0\n");
		} break;
		case TOTALTEST_FAIL: {
			// there is "FAIL" at SMD/test_result.csv			
			QUEST_PRINT("%s : SMD QUEST FAIL\n", __func__);
			return snprintf(buf, BUFF_SZ, "NG_2.0_FAIL_%s\n", strResult);
		} break;

		case TOTALTEST_NO_RESULT_FILE: {
			if (quest_step == SMD_QUEST) {
				// will be executed soon
				QUEST_PRINT("SMD QUEST TESTING\n");
				return snprintf(buf, BUFF_SZ, "TESTING\n");
			} else {
				// not exeuted by unknown reasons
				// ex1) magic exists but /data/log/quest was removed
				// ex2) fail to execute quest.sh
				// ex3) fail to make test_result.csv
				// ex4) etc...
				QUEST_PRINT("SMD QUEST NO_RESULT_FILE && not started\n");

				// we want to know lastkmsg
				if( first_check ) {
					snprintf(options, 50, "action:lastkmsg output_log_path:%s\0", SMD_QUEST_LOGPATH); 
					make_debugging_logs(options);
				}
				
				return snprintf(buf, BUFF_SZ, "RE_WORK\n");
			}
		} break;

		case TOTALTEST_NO_RESULT_STRING: {
			// sm8150 does not execute quest at hlos
			if (quest_step == SMD_QUEST) {
				// will be completed
				QUEST_PRINT("SMD QUEST TESTING\n");
				return snprintf(buf, BUFF_SZ, "TESTING\n");
			} else {
				// need to rework
				QUEST_PRINT("SMD QUEST NO_RESULT_STRING && not started\n");

				// we want to know lastkmsg
				if( first_check ) {
					snprintf(options, 50, "action:lastkmsg output_log_path:%s\0", SMD_QUEST_LOGPATH); 
					make_debugging_logs(options);
				}
				
				return snprintf(buf, BUFF_SZ, "RE_WORK\n");
			}
		} break;
	}


	// (skip) 4. continue scenario and return current progress to factory app


err_out:
	mutex_unlock(&common_lock);
	QUEST_PRINT("SMD QUEST UNKNOWN\n");
	return snprintf(buf, BUFF_SZ, "RE_WORK\n");	
}

static DEVICE_ATTR(nad_stat, S_IRUGO, show_quest_stat, NULL);

static ssize_t show_ddrtest_remain_count(struct device *dev,
					 struct device_attribute *attr,
					 char *buf)
{
	struct param_quest_t param_quest_data;
	
	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		goto err_out;
	}

	return snprintf(buf, BUFF_SZ, "%d\n",
		 param_quest_data.ddrtest_remain_count);
err_out:
	return snprintf(buf, BUFF_SZ, "PARAM ERROR\n");
}

static DEVICE_ATTR(nad_ddrtest_remain_count, S_IRUGO, show_ddrtest_remain_count,
		   NULL);

static ssize_t show_quest_hlos_remain_count(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct param_quest_t param_quest_data;
	
	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		goto err_out;
	}

	return snprintf(buf, BUFF_SZ, "%d\n", param_quest_data.quest_hlos_remain_count);
err_out:
	return snprintf(buf, BUFF_SZ, "PARAM ERROR\n");
}

static DEVICE_ATTR(nad_qmvs_remain_count, S_IRUGO, show_quest_hlos_remain_count, NULL);

static ssize_t store_quest_erase(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct param_quest_t param_quest_data;
	
	if (!strncmp(buf, "erase", 5)) {
		char *argv[4] = { NULL, NULL, NULL, NULL };
		char *envp[3] = { NULL, NULL, NULL };
		int ret_userapp;
		int api_gpio_test = 0;
		char api_gpio_test_result[256] = { 0, };

		mutex_lock(&common_lock);

		argv[0] = ERASE_QUEST_PRG;
		argv[1] = "all";

		envp[0] = "HOME=/";
		envp[1] =
		    "PATH=/system/bin/quest/:/system/bin:/system/xbin";
		ret_userapp =
		    call_usermodehelper(argv[0], argv, envp, UMH_WAIT_EXEC);

		if (!ret_userapp) {
			QUEST_PRINT
			    ("remove_files.sh is executed. ret_userapp = %d\n",
			     ret_userapp);
			erase_pass = 1;
		} else {
			QUEST_PRINT
			    ("remove_files.sh is NOT executed. ret_userapp = %d\n",
			     ret_userapp);
			erase_pass = 0;
		}

		if (!sec_get_param(param_index_quest, &param_quest_data)) {
			QUEST_PRINT("%s : fail - get param!! param_quest_data\n",
			       __func__);
			goto err_out;
		}

		param_quest_data.magic = 0x0;
#if defined(CONFIG_SEC_QUEST_UEFI)		
		param_quest_data.quest_uefi_remain_count = 0x0;
		param_quest_data.quest_uefi_result = 0x0;
#endif
		param_quest_data.quest_hlos_remain_count = 0x0;
		param_quest_data.ddrtest_remain_count = 0x0;
		param_quest_data.ddrtest_result = 0x0;
		param_quest_data.ddrtest_mode = 0x0;
		param_quest_data.bitmap_item_result = 0x0;
		param_quest_data.smd_test_result = 0x0;	
		param_quest_data.quest_step = -1;

		// flushing to param partition
		if (!sec_set_param(param_index_quest, &param_quest_data)) {
			QUEST_PRINT("%s : fail - set param!! param_quest_data\n",
			       __func__);
			goto err_out;
		}

		QUEST_PRINT("clearing MAGIC code done = %d\n",
			  param_quest_data.magic);
		QUEST_PRINT("quest_hlos_remain_count = %d\n",
			  param_quest_data.quest_hlos_remain_count);
		QUEST_PRINT("ddrtest_remain_count = %d\n",
			  param_quest_data.ddrtest_remain_count);
		QUEST_PRINT("ddrtest_result = 0x%8X\n",
			  param_quest_data.ddrtest_result);
		QUEST_PRINT("clearing smd ddr test MAGIC code done = %d\n",
			  param_quest_data.magic);

		// clearing API test result
		if (!sec_set_param(param_index_api_gpio_test, &api_gpio_test)) {
			QUEST_PRINT("%s : fail - set param!! param_quest_data\n",
			       __func__);
			goto err_out;
		}

		if (!sec_set_param
		    (param_index_api_gpio_test_result, api_gpio_test_result)) {
			QUEST_PRINT("%s : fail - set param!! param_quest_data\n",
			       __func__);
			goto err_out;
		}
		mutex_unlock(&common_lock);
		return count;
	} else {
		mutex_unlock(&common_lock);
		return count;
	}
err_out:
	mutex_unlock(&common_lock);
	return count;
}

static ssize_t show_quest_erase(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	if (erase_pass)
		return snprintf(buf, BUFF_SZ, "OK\n");
	else
		return snprintf(buf, BUFF_SZ, "NG\n");
}

static DEVICE_ATTR(nad_erase, S_IRUGO | S_IWUSR, show_quest_erase, store_quest_erase);

static ssize_t show_quest_dram(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct param_quest_t param_quest_data;
	int ddr_result = DDRTEST_INCOMPLETED;

	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		goto err_out;
	}

	// The factory app needs only the ddrtest result of ACAT now.
	// If the ddrtest result of SMD and MAIN are also needed,
	// implement an additional sysfs node or a modification of app.
	ddr_result = get_ddr_scan_result(CAL_QUEST);

	if (ddr_result == DDRTEST_PASS)
		return snprintf(buf, BUFF_SZ, "OK_DRAM\n");
	else if (ddr_result == DDRTEST_FAIL)
		return snprintf(buf, BUFF_SZ, "NG_DRAM_DATA\n");
	else
		return snprintf(buf, BUFF_SZ, "NO_DRAMTEST\n");
err_out:
	return snprintf(buf, BUFF_SZ, "READ ERROR\n");
}

static DEVICE_ATTR(nad_dram, S_IRUGO, show_quest_dram, NULL);

static ssize_t show_quest_dram_debug(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct param_quest_t param_quest_data;

	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		goto err_out;
	}

	return snprintf(buf, BUFF_SZ, "0x%x\n", param_quest_data.ddrtest_result);
err_out:
	return snprintf(buf, BUFF_SZ, "READ ERROR\n");
}

static DEVICE_ATTR(nad_dram_debug, S_IRUGO, show_quest_dram_debug, NULL);

static ssize_t show_quest_dram_err_addr(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int ret = 0;
	int i = 0;
	struct param_quest_ddr_result_t param_quest_ddr_result_data;

	if (!sec_get_param
	    (param_index_quest_ddr_result, &param_quest_ddr_result_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_ddr_result_data\n",
		       __func__);
		goto err_out;
	}

	ret =
	    snprintf(buf, BUFF_SZ, "Total : %d\n\n",
		    param_quest_ddr_result_data.ddr_err_addr_total);
	for (i = 0; i < param_quest_ddr_result_data.ddr_err_addr_total; i++) {
		ret +=
		    snprintf(buf + ret - 1, BUFF_SZ, "[%d] 0x%llx\n", i,
			    param_quest_ddr_result_data.ddr_err_addr[i]);
	}

	return ret;
err_out:
	return snprintf(buf, BUFF_SZ, "READ ERROR\n");
}

static DEVICE_ATTR(nad_dram_err_addr, S_IRUGO, show_quest_dram_err_addr, NULL);

static ssize_t show_quest_support(struct device *dev,
				struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_ARCH_MSM8998) || defined(CONFIG_ARCH_MSM8996) || defined(CONFIG_ARCH_SDM845) || defined(CONFIG_ARCH_SM8150)
	return snprintf(buf, BUFF_SZ, "SUPPORT\n");
#else
	return snprintf(buf, BUFF_SZ, "NOT_SUPPORT\n");
#endif
}

static DEVICE_ATTR(nad_support, S_IRUGO, show_quest_support, NULL);

static ssize_t store_quest_logs(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int fd = 0, idx = 0;
	char path[100] = { '\0' };
	char temp[1]={'\0'}, tempbuf[BUFF_SZ]={'\0',};

	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);

	QUEST_PRINT("%s\n", buf);

	sscanf(buf, "%s", path);
	fd = sys_open(path, O_RDONLY, 0);

	if (fd >= 0) {
		while (sys_read(fd, temp, 1) == 1) {
			tempbuf[idx++] = temp[0];
			if( temp[0]=='\n' ) {
				tempbuf[idx] = '\0';
				QUEST_PRINT("%s", tempbuf);
				idx = 0;
			}
		}

		sys_close(fd);
	} else {
		QUEST_PRINT("The File is not existed. %s\n", __func__);
	}

	set_fs(old_fs);
	return count;
}

static DEVICE_ATTR(nad_logs, 0200, NULL, store_quest_logs);

static ssize_t store_quest_end(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	char result[20] = { '\0' };

	QUEST_PRINT("result : (step=%d) %s\n", quest_step, buf);

	sscanf(buf, "%s", result);

	if (!strcmp(result, "quest_pass")) {
		kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, quest_uevent.envp);
		if( quest_step==SMD_QUEST )
			QUEST_PRINT("%s : let's update smd_test_result when show_quest_stat is called\n", __func__);
		QUEST_PRINT
		    ("QUEST result : %s, quest_pass, Send to Process App for Quest test end : %s\n",
		     result, __func__);
	} else {

		// if store_quest_acat() is not called, we cannot initialize param, so let's do it here
		if( quest_step == CAL_QUEST ) {
			struct param_quest_t param_quest_data;
			
			QUEST_PRINT("%s : CAL quest_hlos was failed, so initialize param and then trigger panic\n", __func__ );

			if (!sec_get_param(param_index_quest, &param_quest_data)) 
				QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);

#if defined (CONFIG_SEC_QUEST_UEFI)
			param_quest_data.quest_uefi_remain_count = 0;
#endif			
			param_quest_data.quest_hlos_remain_count = 0;
			param_quest_data.quest_step = -1;
			if (!sec_set_param(param_index_quest, &param_quest_data))
				QUEST_PRINT("%s : fail - set param!! param_quest_data\n", __func__);
		}

		if (quest_step == MAIN_QUEST || quest_step == CAL_QUEST) {
			QUEST_PRINT
			    ("QUEST result : %s, Device enter the upload mode because it is SHORT_QUEST : %s\n",
			     result, __func__);
			panic(result);
		} else {
			kobject_uevent_env(&dev->kobj, KOBJ_CHANGE,
					   quest_uevent.envp);
			QUEST_PRINT
			    ("QUEST result : %s, Send to Process App for Quest test end : %s\n",
			     result, __func__);
		}
	}

	/*------------ for QDAF ------------*/
	if( quest_step==SMD_QUEST )
		call_qdaf_from_quest_driver(QUEST_QDAF_ACTION_CONTROL_START_WITHOUT_PANIC, UMH_WAIT_EXEC);
	/*---------------------------------*/

	return count;
}

static DEVICE_ATTR(nad_end, S_IWUSR, NULL, store_quest_end);

/* Please sync with TEST_ITEM in sec_qeust.h */
char *STR_TEST_ITEM[ITEM_CNT + 1] = {
	"UFS",
	"QMESACACHE",
	"QMESADDR",
	"VDDMIN",
	"SUSPEND",
	"CCOHERENCY",
	"ICACHE",
	"CRYPTO",
	"DDRSCAN",
	"SENSOR",
	"SENSORPROBE",
	"GFX",
	"A75G",
	"Q65G",	
	"THERMAL",
	"QDAF",
	//
	"FULL"
};

static ssize_t show_smd_quest_result(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	ssize_t info_size = 0;
	int iCnt;
	struct param_quest_t param_quest_data;

	mutex_lock(&common_lock);

	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		goto err_out;
	}
	print_param_quest_data(param_quest_data);	

	for (iCnt = 0; iCnt < ITEM_CNT; iCnt++) {
		char strResult[QUEST_BUFF_SIZE] = { '\0', };

		if (get_smd_item_result(param_quest_data, strResult, iCnt)==-1)
			goto err_out;

		info_size +=
		    snprintf((char *)(buf + info_size), MAX_LEN_STR - info_size,
			     "\"%s\":\"%s\",", STR_TEST_ITEM[iCnt], strResult);
	}
	info_size += snprintf((char *)(buf + info_size), MAX_LEN_STR - info_size,"\n");

	QUEST_PRINT("%s, result=%s\n", __func__, buf);

	mutex_unlock(&common_lock);

	return info_size;
err_out:
	mutex_unlock(&common_lock);
	return snprintf(buf, BUFF_SZ, "UNKNOWN\n");
}

static ssize_t store_smd_quest_result(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct param_quest_t param_quest_data;
	int _result = -1;
	char test_name[QUEST_BUFF_SIZE * 2] = { '\0', };
	char temp[QUEST_BUFF_SIZE * 3] = { '\0', };
	char quest_test[2][QUEST_BUFF_SIZE * 2];	// 2: "test_name", "result"
	char result_string[QUEST_BUFF_SIZE] = { '\0', };
	char *quest_ptr, *string;
	int item = -1;

	mutex_lock(&common_lock);

#if 0	// this function can be called, so comment out the below for qdaf
	if (quest_step != SMD_QUEST) {
		QUEST_PRINT("store quest_result only at smd_quest\n");
		mutex_unlock(&common_lock);
		return -EIO;
	}
#endif

	QUEST_PRINT("buf : %s count : %d\n", buf, (int)count);

	if (QUEST_BUFF_SIZE * 3 < (int)count || (int)count < 4) {
		QUEST_PRINT("result cmd size too long : QUEST_BUFF_SIZE<%d\n",
			  (int)count);
		mutex_unlock(&common_lock);
		return -EINVAL;
	}

	/* Copy buf to quest temp */
	strlcpy(temp, buf, QUEST_BUFF_SIZE * 3);
	string = temp;

	quest_ptr = strsep(&string, ",");
	strlcpy(quest_test[0], quest_ptr, QUEST_BUFF_SIZE * 2);
	quest_ptr = strsep(&string, ",");
	strlcpy(quest_test[1], quest_ptr, QUEST_BUFF_SIZE * 2);

	sscanf(quest_test[0], "%s", test_name);
	sscanf(quest_test[1], "%s", result_string);

	QUEST_PRINT("test_name : %s, test result=%s\n", test_name, result_string);

	if (TEST_PASS(result_string))
		_result = QUEST_ITEM_TEST_PASS;
	else if (TEST_FAIL(result_string))
		_result = QUEST_ITEM_TEST_FAIL;
	else
		_result = QUEST_ITEM_TEST_INCOMPLETED;

	if (TEST_CRYPTO(test_name))
		item = CRYPTO;
	else if (TEST_ICACHE(test_name))
		item = ICACHE;
	else if (TEST_CCOHERENCY(test_name))
		item = CCOHERENCY;
	else if (TEST_SUSPEND(test_name))
		item = SUSPEND;
	else if (TEST_VDDMIN(test_name))
		item = VDDMIN;
	else if (TEST_QMESADDR(test_name))
		item = QMESADDR;
	else if (TEST_QMESACACHE(test_name))
		item = QMESACACHE;
	else if (TEST_UFS(test_name))
		item = UFS;
	else if (TEST_SENSOR(test_name))
		item = SENSOR;
	else if (TEST_SENSORPROBE(test_name))
		item = SENSORPROBE;
	else if (TEST_GFX(test_name))
		item = GFX;
	else if (TEST_A75G(test_name))
		item = A75G;
	else if (TEST_Q65G(test_name))
		item = Q65G;
	else if (TEST_THERMAL(test_name))
		item = THERMAL;
	else if (TEST_QDAF(test_name))
		item = QDAF;	
	else
		item = NOT_ASSIGN;

	if (item == NOT_ASSIGN) {
		QUEST_PRINT("%s : fail - get test item in QUEST!! \n", __func__);
		mutex_unlock(&common_lock);
		return count;
	}

	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		mutex_unlock(&common_lock);
		return -EINVAL;
	}

#if 0 // this function can be called, so comment out the below for qdaf
	param_quest_data.bitmap_item_result |=
	    TEST_ITEM_RESULT(quest_step, item, _result);
#else
	param_quest_data.bitmap_item_result |=
		TEST_ITEM_RESULT(SMD_QUEST, item, _result);
#endif 

	QUEST_PRINT("%s : bitmap_item_result=%u, quest_step=%d, item=%d, _result=%d\n",
		  __func__, param_quest_data.bitmap_item_result, quest_step, item, _result);

	if (!sec_set_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - set param!! param_quest_data\n", __func__);
		mutex_unlock(&common_lock);
		return -EINVAL;
	}

	mutex_unlock(&common_lock);
	return count;
}


static DEVICE_ATTR(nad_result, S_IRUGO | S_IWUSR, show_smd_quest_result, store_smd_quest_result);



static ssize_t store_quest_main(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct param_quest_t param_quest_data;
	int idx = 0;
	int ret = -1;
	char temp[QUEST_BUFF_SIZE * 3];
	char quest_cmd[QUEST_MAIN_CMD_LIST][QUEST_BUFF_SIZE];
	char *quest_ptr, *string;
	int running_time;

	mutex_lock(&common_lock);

	/* Copy buf to quest temp */
	strlcpy(temp, buf, QUEST_BUFF_SIZE * 3);
	string = temp;

	while (idx < QUEST_MAIN_CMD_LIST) {
		quest_ptr = strsep(&string, ",");
		strlcpy(quest_cmd[idx++], quest_ptr, QUEST_BUFF_SIZE);
	}

	if (quest_step == MAIN_QUEST) {
		QUEST_PRINT("duplicated!\n");
		mutex_unlock(&common_lock);
		return count;
	}

	if (!strncmp(buf, "start", 5)) {
		ret = sscanf(quest_cmd[1], "%d\n", &running_time);
		if (ret != 1) {
			mutex_unlock(&common_lock);
			return -EINVAL;
		}

		if (!sec_get_param(param_index_quest, &param_quest_data)) {
			QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
			mutex_unlock(&common_lock);
			return -1;
		}

		param_quest_data.ddrtest_mode = UPLOAD_CAUSE_QUEST_DDR_TEST_MAIN;
		param_quest_data.ddrtest_remain_count = 1;
		param_quest_data.quest_hlos_remain_count = 0;
#if defined(CONFIG_SEC_QUEST_UEFI)
		// the concept of main quest is
		// hlos long -> repeat 5 times (uefi -> boot to idle -> parsing -> reboot ) -> ddr -> main quest done
		// we will decrease hlos count when parsing the result of quest_uefi forcely
		param_quest_data.quest_hlos_remain_count = 5;
		param_quest_data.quest_uefi_remain_count = 5;

		// (skip) 1. let's remove previous questresult
		// let's backup previous uefi log into backup dir using quest_main.sh becuase hlos run first
		//arrange_quest_logs(MAIN_QUEST_LOGPATH);

		// 2. initialize quest_uefi_result
		param_quest_data.quest_uefi_result
			= SET_UEFI_RESULT(MAIN_QUEST, param_quest_data.quest_uefi_result, 0x0);
	
#endif
		// 3. set quest_step
		param_quest_data.quest_step = MAIN_QUEST;

		if (!sec_set_param(param_index_quest, &param_quest_data)) {
			QUEST_PRINT("%s : fail - set param!! param_quest_data\n", __func__);
			mutex_unlock(&common_lock);
			return -1;
		}

		quest_step = MAIN_QUEST;
		mutex_unlock(&common_lock);
		do_quest();
	}

	mutex_unlock(&common_lock);
	return count;
}

static ssize_t show_quest_main(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct param_quest_t param_quest_data;
#if defined(CONFIG_SEC_QUEST_UEFI)	
	int uefi_result = TOTALTEST_UNKNOWN;
	int hlos_result = TOTALTEST_UNKNOWN;
#endif
	int total_result = TOTALTEST_UNKNOWN;
	

	QUEST_PRINT("%s : called\n", __func__);

	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		goto err_out;
	}
	print_param_quest_data(param_quest_data);

#if defined(CONFIG_SEC_QUEST_UEFI)
	// uefi logs were moved into sub dir at show_main_quest_run
	// so just call get_quest_uefi_result
	uefi_result = get_quest_uefi_result(MAIN_QUEST_LOGPATH, 1);
	hlos_result = get_quest_hlos_result(MAIN_QUEST);

#if defined(CONFIG_SEC_QUEST_UEFI_ENHENCEMENT)
	// 2.1. The uefi_enhancement will make EnhanceStart.txt if it was started. 
	// If it is failed the EnhanceStart.txt is remained,
	// and if it is passed the EnhanceStart.txt is deleted.
	// So, let's determin pass/fail with checking existence of EnhanceStart.txt
	{
		int fd = 0;
		mm_segment_t old_fs = get_fs();
		set_fs(KERNEL_DS);
		if ( (fd = sys_open(UEFI_ENHANCEMENT_START_FILE, O_RDONLY, 0))<0 ) {
			QUEST_PRINT("%s : the EnhanceStart.txt does not exist (PASS)", __func__);
		}else {
			QUEST_PRINT("%s : no EnhanceStart.txt (FAIL)", __func__);
			uefi_result = TOTALTEST_FAIL;
		}
		set_fs(old_fs);
	}
#endif	

	// In case of main quest, we ran both quest_uefi and quest_hlos.
	// So, we have to check all result of them.
	if( uefi_result==TOTALTEST_PASS && hlos_result==TOTALTEST_PASS )
		total_result = TOTALTEST_PASS;
	else if( uefi_result==TOTALTEST_FAIL || hlos_result==TOTALTEST_FAIL ) 
		total_result = TOTALTEST_FAIL;
	else  
		total_result = TOTALTEST_UNKNOWN;
	QUEST_PRINT("%s : uefi_result(%d) / hlos_result(%d) / total_result(%d)\n",
		__func__, uefi_result, hlos_result, total_result );	
#else
	total_result = get_quest_hlos_result(MAIN_QUEST); // get quest_hlos result
#endif

	switch (total_result) {
		case TOTALTEST_PASS: {
			QUEST_PRINT("MAIN QUEST Passed\n");
			return snprintf(buf, BUFF_SZ, "OK_2.0\n");
		} break;

		case TOTALTEST_FAIL: {
			QUEST_PRINT("MAIN QUEST fail\n");
			return snprintf(buf, BUFF_SZ, "NG_2.0_FAIL\n");
		} break;

		default: {
			QUEST_PRINT("MAIN QUEST No Run\n");
			return snprintf(buf, BUFF_SZ, "OK\n");
		}
	}

err_out:
	return snprintf(buf, BUFF_SZ, "MAIN QUEST UNKNOWN\n");
}

static DEVICE_ATTR(balancer, S_IRUGO | S_IWUSR, show_quest_main, store_quest_main);

static ssize_t show_main_quest_timeout(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return snprintf(buf, BUFF_SZ, "%d\n", MAIN_QUEST_TIMEOUT);
}
static DEVICE_ATTR(timeout, 0444, show_main_quest_timeout, NULL);


static ssize_t store_main_quest_run(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
#if defined(CONFIG_SEC_QUEST_UEFI)
	struct param_quest_t param_quest_data;
	int uefi_remain_count =0;
	int ddrtest_remain_count = 0;

	mutex_lock(&common_lock);

	QUEST_PRINT("%s is called\n", __func__);
	
	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		mutex_unlock(&common_lock);
		return -EINVAL;
	}
	uefi_remain_count = param_quest_data.quest_uefi_remain_count;
	ddrtest_remain_count = param_quest_data.ddrtest_remain_count;

	// if the count of quest_uefi remains, let's trigger quest_uefi
	if( uefi_remain_count > 0 || ddrtest_remain_count > 0 ) {
		QUEST_PRINT("%s : uefi_remain_count remained(%d)\n", 
				__func__, uefi_remain_count );

#if defined(CONFIG_SEC_QUEST_UEFI_ENHENCEMENT)
		// in case of beyondx which have UEFI ENHANCEMENT, the quest_uefi will run only 4 times
		if( uefi_remain_count > 1 ) {
#else
		if( uefi_remain_count > 0 ) {
#endif
			QUEST_PRINT("%s : will trigger quest_uefi\n", __func__);

			// 1. let's remove previous questresult
			arrange_quest_logs(MAIN_QUEST_LOGPATH);

			// 2. initialize quest_uefi_result
			param_quest_data.quest_uefi_result
				= SET_UEFI_RESULT(MAIN_QUEST, param_quest_data.quest_uefi_result, 0x0);
		}
		
		// 3. we do not repeat quest_hlos, so let's decrease hlos count forcely
		if( param_quest_data.quest_hlos_remain_count >= 1 )		
			param_quest_data.quest_hlos_remain_count--;
	
		if (!sec_set_param(param_index_quest, &param_quest_data)) {
			QUEST_PRINT("%s : fail - set param!! param_quest_data\n", __func__);
			mutex_unlock(&common_lock);
			return -EINVAL;
		}

		// 3.1.backup more dumps
		// let's check only dump gotten when hlos quest runs
		if( uefi_remain_count == 4 ) {
			char options[50] = { '\0', };
			QUEST_PRINT("backup more dumps before reboot\n");	
			snprintf(options, 50, "action:backupmoredumps\0"); 
			make_debugging_logs(options);			
		}

		// 4. reboot device	
		msleep(3000);	// to guarantee saving FLOG
		mutex_unlock(&common_lock);
		kernel_restart(NULL);			
	}else {
		mutex_unlock(&common_lock);
		panic("abnormal uefi_remain_count");
	}

	mutex_unlock(&common_lock);
#endif
	return count; 
}


// parsing the result of quest_uefi and return the status of main quest scenario
static ssize_t show_main_quest_run(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
#if defined(CONFIG_SEC_QUEST_UEFI)	 
	struct param_quest_t param_quest_data;
	int uefi_remain_count = 0;
	int ddrtest_remain_count = 0;
	int uefi_result = TOTALTEST_UNKNOWN;
	char options[50] = { '\0', };

	mutex_lock(&common_lock);

	QUEST_PRINT("%s : at boot time, always first call from factory app (read sec_nad_balancer/run)\n", __func__);

	print_reset_info();

	// 0. print param informations
	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		goto err_out;
	}
	print_param_quest_data(param_quest_data);
	uefi_remain_count = param_quest_data.quest_uefi_remain_count;
	ddrtest_remain_count = param_quest_data.ddrtest_remain_count;


	// 0.1. if now is not main quest, return
	if( param_quest_data.quest_step != MAIN_QUEST ) {
		QUEST_PRINT("%s : now step is not main quest, so will be returned\n", __func__);
		mutex_unlock(&common_lock);
		return snprintf(buf, BUFF_SZ, "END\n");
	}else
		QUEST_PRINT("%s : now step is main quest\n", __func__);


	// 1. move questresult logs to sub directory
	QUEST_PRINT("%s : if questresult logs exist, let's move them to main directory\n", __func__);
	move_questresult_to_sub_dir(MAIN_QUEST);


	// 2. parsing the result of quest_uefi
	uefi_result = get_quest_uefi_result(MAIN_QUEST_LOGPATH, 1);
	QUEST_PRINT("%s : update param_quest_data.quest_uefi_result \n", __func__);
	param_quest_data.quest_uefi_result
		= SET_UEFI_RESULT(MAIN_QUEST, param_quest_data.quest_uefi_result, uefi_result);
	if (!sec_set_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - set param!! param_quest_data\n", __func__);
		goto err_out;
	}	


	// 3. result processing
	switch( uefi_result ) {
		case TOTALTEST_NO_RESULT_FILE :
		case TOTALTEST_NO_RESULT_STRING : {
			QUEST_PRINT("%s : uefi_result = %d, so let's make lastkmsg\n", __func__, uefi_result );
			snprintf(options, 50, "action:lastkmsg output_log_path:%s\0", MAIN_QUEST_LOGPATH); 
			make_debugging_logs(options);			
			break;
		}
		case TOTALTEST_FAIL : {
			if( !(uefi_remain_count==0 && ddrtest_remain_count==0) ) {
				QUEST_PRINT("%s : quest_uefi result was failed, so trigger panic\n", __func__ );

				// if failed, let's do not run quest_uefi any more.
				param_quest_data.quest_uefi_remain_count = 0;
				param_quest_data.quest_hlos_remain_count = 0;
				if (!sec_set_param(param_index_quest, &param_quest_data)) {
					QUEST_PRINT("%s : fail - set param!! param_quest_data\n", __func__);
					goto err_out;
				}		

				// trigger panic
				mutex_unlock(&common_lock);
				panic("quest_uefi failed");
			}
		}
	}	


	// 4. continue scenario and return current progress to factory app
	if( uefi_remain_count > 0 || ddrtest_remain_count > 0 ) {

		// if quest_uefi count remains, let's return NOT_END
		QUEST_PRINT("%s : NOT_END (uefi=%d, ddrtest=%d)\n", __func__,
			uefi_remain_count, ddrtest_remain_count);
		mutex_unlock(&common_lock);
		return snprintf(buf, BUFF_SZ, "NOT_END\n");
		
	} else if ( uefi_remain_count == 0 && ddrtest_remain_count == 0) {

		// initialize quest_step
		param_quest_data.quest_step = -1;
		if (!sec_set_param(param_index_quest, &param_quest_data)) {
			QUEST_PRINT("%s : fail - set param!! param_quest_data\n", __func__);
			goto err_out;
		}		
		mutex_unlock(&common_lock);
		return snprintf(buf, BUFF_SZ, "END\n"); 
	}

	mutex_unlock(&common_lock);

err_out:
	return snprintf(buf, BUFF_SZ, "UNKNOWN\n");
	
#else 
	return snprintf(buf, BUFF_SZ, "END\n");
#endif
}
static DEVICE_ATTR(run, S_IRUGO | S_IWUSR, show_main_quest_run, store_main_quest_run);


static ssize_t store_quest_info(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct param_quest_t param_quest_data;
	char info_name[QUEST_BUFF_SIZE * 2] = { '\0', };
	char temp[QUEST_BUFF_SIZE * 3] = { '\0', };
	char quest_test[2][QUEST_BUFF_SIZE * 2];	// 2: "info_name", "result"
	int resultValue;
	char *quest_ptr, *string;

	mutex_lock(&common_lock);

	QUEST_PRINT("buf : %s count : %d\n", buf, (int)count);

	if (QUEST_BUFF_SIZE * 3 < (int)count || (int)count < 4) {
		QUEST_PRINT("result cmd size too long : QUEST_BUFF_SIZE<%d\n",
			  (int)count);
		mutex_unlock(&common_lock);
		return -EINVAL;
	}

	/* Copy buf to quest temp */
	strlcpy(temp, buf, QUEST_BUFF_SIZE * 3);
	string = temp;

	quest_ptr = strsep(&string, ",");
	strlcpy(quest_test[0], quest_ptr, QUEST_BUFF_SIZE * 2);
	quest_ptr = strsep(&string, ",");
	strlcpy(quest_test[1], quest_ptr, QUEST_BUFF_SIZE * 2);

	sscanf(quest_test[0], "%s", info_name);
	sscanf(quest_test[1], "%d", &resultValue);

	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		mutex_unlock(&common_lock);
		return -EINVAL;
	}

	if (!strcmp("thermal", info_name))
		param_quest_data.thermal = resultValue;
	else if (!strcmp("clock", info_name))
		param_quest_data.tested_clock = resultValue;

	QUEST_PRINT("info_name : %s, result=%d\n", info_name, resultValue);

	if (!sec_set_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - set param!! param_quest_data\n", __func__);
		mutex_unlock(&common_lock);
		return -EINVAL;
	}
	
	mutex_unlock(&common_lock);
	return count;
}

static ssize_t show_quest_info(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	struct param_quest_t param_quest_data;
	ssize_t info_size = 0;

	if (!sec_get_param(param_index_quest, &param_quest_data)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		goto err_out;
	}

	info_size +=
	    snprintf((char *)(buf + info_size), MAX_LEN_STR - info_size,
		     "\"REMAIN_CNT\":\"%d\",",
		     param_quest_data.quest_hlos_remain_count);
	info_size +=
	    snprintf((char *)(buf + info_size), MAX_LEN_STR - info_size,
		     "\"THERMAL\":\"%d\",", param_quest_data.thermal);
	info_size +=
	    snprintf((char *)(buf + info_size), MAX_LEN_STR - info_size,
		     "\"CLOCK\":\"%d\",", param_quest_data.tested_clock);

	return info_size;
err_out:
	return snprintf(buf, BUFF_SZ, "PARAM ERROR\n");
}

static DEVICE_ATTR(nad_info, S_IRUGO | S_IWUSR, show_quest_info, store_quest_info);

static ssize_t show_quest_api(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	unsigned int api_gpio_test;
	char api_gpio_test_result[256];

	if (!sec_get_param(param_index_api_gpio_test, &api_gpio_test)) {
		QUEST_PRINT("%s : fail - get param!! param_quest_data\n", __func__);
		goto err_out;
	}

	if (api_gpio_test) {
		if (!sec_get_param
		    (param_index_api_gpio_test_result, api_gpio_test_result)) {
			QUEST_PRINT("%s : fail - get param!! param_quest_data\n",
			       __func__);
			goto err_out;
		}
		return snprintf(buf, BUFF_SZ, "%s", api_gpio_test_result);
	} else
		return snprintf(buf, BUFF_SZ, "NONE\n");

err_out:
	return snprintf(buf, BUFF_SZ, "READ ERROR\n");
}

static DEVICE_ATTR(nad_api, 0444, show_quest_api, NULL);

static ssize_t show_quest_version(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	#if 0
		//QUEST_2.0.1_SS_SLT_06142018_NS_GCM - INITPORT
		return snprintf(buf, BUFF_SZ, "SM8150.0204.01.INITPORT\n");	
	
		//QUEST_1.0_SS_07172018_SM8150_preR2_sdcard
		return snprintf(buf, BUFF_SZ, "SM8150.0101.01.INITPORT\n");

		//QUEST_1.0_SS_08312018_SDM855_gcm_flashval_temp
		return snprintf(buf, BUFF_SZ, "SM8150.0102.01.PRERELEASE\n");
	
		//QUEST_1.0_SS_10052018_SDM855_gcm_flashval_temp -  - SLPI Enable
		return snprintf(buf, BUFF_SZ, "SM8150.0102.01.PRERELEASE.SLPIe\n");
	#else	
		//QUEST_1.0.1_SS_10030018_SDM855_naturescene_path
		return snprintf(buf, BUFF_SZ, "SM8150.0103.01.1030RELEASE\n");	
	#endif	
}
static DEVICE_ATTR(nad_version, 0444, show_quest_version, NULL);



static ssize_t show_quest_qdaf_control(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	if( qdaf_cur_cmd_mode==QUEST_QDAF_ACTION_CONTROL_START_WITHOUT_PANIC ||
		qdaf_cur_cmd_mode==QUEST_QDAF_ACTION_CONTROL_START_WITH_PANIC )
		return snprintf(buf, BUFF_SZ, "RUNNING\n");

	return snprintf(buf, BUFF_SZ, "READY\n");
}
static ssize_t store_quest_qdaf_control(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int idx = 0, cmd_mode=0, wait_val=0;
	char temp[QUEST_BUFF_SIZE * 3];
	char quest_cmd[QUEST_CMD_LIST][QUEST_BUFF_SIZE];
	char *quest_ptr, *string;
	int ret_userapp;
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char *envp[5] = { 
		"HOME=/", 
		"PATH=/system/bin/quest:/system/bin:/system/xbin", 
		"ANDROID_DATA=/data", 
		"ANDROID_ROOT=/system", 
		NULL };

	QUEST_PRINT("%s : is called\n",__func__);

	if ((int)count < QUEST_BUFF_SIZE) {
		QUEST_PRINT("%s : return error (count=%d)\n", __func__, (int)count);
		return -EINVAL;;
	}

	if (strncmp(buf, "nad_qdaf_control", 16)) {
		QUEST_PRINT("%s : return error (buf=%s)\n", __func__, buf);
		return -EINVAL;;
	}

	strlcpy(temp, buf, QUEST_BUFF_SIZE * 3);
	string = temp;
	while (idx < QUEST_CMD_LIST) {
		quest_ptr = strsep(&string, ",");
		strlcpy(quest_cmd[idx++], quest_ptr, QUEST_BUFF_SIZE);
	}
	sscanf(quest_cmd[1], "%d", &cmd_mode);

	// let's just return if receiving the same command as before one
	if ( qdaf_cur_cmd_mode==cmd_mode ) {
		if( cmd_mode==QUEST_QDAF_ACTION_CONTROL_START_WITHOUT_PANIC ||
			cmd_mode==QUEST_QDAF_ACTION_CONTROL_START_WITH_PANIC) {
			QUEST_PRINT("%s : return because qdaf.sh already has been running.\n", __func__);
		}else if( cmd_mode==QUEST_QDAF_ACTION_CONTROL_STOP || 
				cmd_mode==QUEST_QDAF_ACTION_CONTROL_STOP_WATING) {
			QUEST_PRINT("%s : return because qdaf.sh has not been running yet.\n", __func__);
		}
		return count;
	}

	// if receiving control command from AtNadCheck when quest is running, let's return NG
	// if receiving control command from quest.sh, invoke qdaf.sh to stop qdaf with UMH_WAIT_PROC
	if( quest_step!=-1 &&
		(cmd_mode==QUEST_QDAF_ACTION_CONTROL_START_WITHOUT_PANIC ||
		cmd_mode==QUEST_QDAF_ACTION_CONTROL_START_WITH_PANIC ||
		cmd_mode==QUEST_QDAF_ACTION_CONTROL_STOP) ) {
		QUEST_PRINT("%s : return because quest is running.\n", __func__);
		return count;
	}
		
	argv[0] = QDAF_PROG;
	argv[1] = quest_cmd[1];
	switch (cmd_mode) {
	case QUEST_QDAF_ACTION_CONTROL_START_WITHOUT_PANIC :
		QUEST_PRINT("%s : qdaf will be started (without panic)\n",__func__);
		wait_val = UMH_WAIT_EXEC;
		break;		
	case QUEST_QDAF_ACTION_CONTROL_START_WITH_PANIC:
		QUEST_PRINT("%s : qdaf will be started (with panic)\n",__func__);
		wait_val = UMH_WAIT_EXEC;		
		break;
	case QUEST_QDAF_ACTION_CONTROL_STOP :
		QUEST_PRINT("%s : qdaf will be stopped\n",__func__);
		wait_val = UMH_WAIT_EXEC;		
		break;
	case QUEST_QDAF_ACTION_CONTROL_STOP_WATING :
		QUEST_PRINT("%s : qdaf will be stopped (waiting)\n",__func__);
		// should wait for completion to gurantee not working of qdaf
		wait_val = UMH_WAIT_PROC;
		break;
	default :
		QUEST_PRINT("%s : return because invalid cmd mode(%d)\n", __func__, cmd_mode);
		return count;
	}
	
	ret_userapp = call_usermodehelper(argv[0], argv, envp, wait_val);
	if (!ret_userapp) {
		qdaf_cur_cmd_mode = cmd_mode;
		QUEST_PRINT("%s : succeded to trigger qdaf.sh\n", __func__);
		QUEST_PRINT("%s : qdaf_cur_cmd_mode=%s\n", __func__, quest_cmd[1]);	
	}else {
		qdaf_cur_cmd_mode = QUEST_QDAF_ACTION_EMPTY;
		// evaulate return value after ret_userapp>>8
		QUEST_PRINT("%s : failed to trigger qdaf.sh. error=%d\n", __func__, ret_userapp);
	}
	return count;
}
static DEVICE_ATTR(nad_qdaf_control, S_IRUGO | S_IWUSR, show_quest_qdaf_control, store_quest_qdaf_control);

/*
static int qdaf_check_prev_result(void)
{
	int fd = 0;
	int ret;
	char buf[512] = { '\0', };
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);

	fd = sys_open(QDAF_QMESA_LOG, O_RDONLY, 0);
	if (fd >= 0) {
		int found = 0;

		printk(KERN_DEBUG);

		while (sys_read(fd, buf, 512)) {
			char *ptr;
			char *div = "\n";
			char *tok = NULL;

			ptr = buf;
			while ((tok = strsep(&ptr, div)) != NULL) {
				if ((strstr(tok, "failure"))) {
					ret = QUEST_QDAF_TEST_RESULT_NG;
					found = 1;
					break;
				}
			}

			if (found) break;
		}

		if (!found) ret = QUEST_QDAF_TEST_RESULT_OK;
		sys_close(fd);
	} else {
		QUEST_PRINT("%s : result file is not existed\n", __func__);
		ret = QUEST_QDAF_TEST_RESULT_NONE;
	}

	set_fs(old_fs);
	return ret;	
}

static ssize_t show_quest_qdaf_prev_result(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int qdaf_result = qdaf_check_prev_result();
	switch (qdaf_result) {
	case QUEST_QDAF_TEST_RESULT_OK:
		QUEST_PRINT("%s : previous test result : pass\n", __func__);
		return snprintf(buf, BUFF_SZ, "OK\n");
		break;
	case QUEST_QDAF_TEST_RESULT_NG:
		QUEST_PRINT("%s : previous test result : fail\n", __func__);
		return snprintf(buf, BUFF_SZ, "NG\n");
		break;
	default:
		QUEST_PRINT("%s : previous test result : unknown\n", __func__);
		return snprintf(buf, BUFF_SZ, "NONE\n");
	}
	
	return snprintf(buf, BUFF_SZ, "NONE\n");

}
*/

static ssize_t show_quest_qdaf_result(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int ret_userapp, wait_val=0, failed_cnt=0;
	char act[QUEST_BUFF_SIZE]={0,};
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char *envp[5] = { 
		"HOME=/", 
		"PATH=/system/bin/quest:/system/bin:/system/xbin", 
		"ANDROID_DATA=/data", 
		"ANDROID_ROOT=/system", 
		NULL };

	QUEST_PRINT("%s : is called\n",__func__);

	print_reset_info();

	snprintf(act, 1, "%d", QUEST_QDAF_ACTION_RESULT_GET);

	argv[0] = QDAF_PROG;
	// TODO : conversion int->string
	argv[1] = "6";	//QUEST_QDAF_ACTION_RESULT_GET 
	wait_val = UMH_WAIT_PROC;
	ret_userapp = call_usermodehelper(argv[0], argv, envp, wait_val);
	if (!ret_userapp) {
		QUEST_PRINT("%s : succeded to trigger qdaf.sh and failed_cnt=0\n", __func__);
		return snprintf(buf, BUFF_SZ, "OK\n");
	}else if (ret_userapp > 0 ) {
		// evaulate return value after ret_userapp>>8
		// qdaf.sh exit with failed_cnt if persist.qdaf.failed_cnt>0, so let's use return value from call_usermodehelper
		failed_cnt = ret_userapp>>8;
		QUEST_PRINT("%s : succeded to trigger qdaf.sh and return_value=%d(failed_cnt=%d)\n", 
			__func__, ret_userapp, failed_cnt);		
		return snprintf(buf, BUFF_SZ, "NG,%d\n", failed_cnt);		
	}else {
		// evaulate return value after ret_userapp>>8
		QUEST_PRINT("%s : failed to trigger qdaf.sh. error=%d\n", __func__, ret_userapp);
		return snprintf(buf, BUFF_SZ, "NONE\n");
	}	

	return snprintf(buf, BUFF_SZ, "NONE\n");
}

static ssize_t store_quest_qdaf_result(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int idx = 0, cmd_mode=0, wait_val=0;
	int ret_userapp;
	char temp[QUEST_BUFF_SIZE * 3];
	char quest_cmd[QUEST_CMD_LIST][QUEST_BUFF_SIZE];
	char *quest_ptr, *string;
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char *envp[5] = { 
		"HOME=/", 
		"PATH=/system/bin/quest:/system/bin:/system/xbin", 
		"ANDROID_DATA=/data", 
		"ANDROID_ROOT=/system", 
		NULL };	

	QUEST_PRINT("%s : is called\n",__func__);

	if ((int)count < QUEST_BUFF_SIZE) {
		QUEST_PRINT("%s : return error (count=%d)\n", __func__, (int)count);
		return -EINVAL;;
	}

	if (strncmp(buf, "nad_qdaf_result", 15)) {
		QUEST_PRINT("%s : return error (buf=%s)\n", __func__, buf);
		return -EINVAL;;
	}

	strlcpy(temp, buf, QUEST_BUFF_SIZE * 3);
	string = temp;
	while (idx < QUEST_CMD_LIST) {
		quest_ptr = strsep(&string, ",");
		strlcpy(quest_cmd[idx++], quest_ptr, QUEST_BUFF_SIZE);
	}
	sscanf(quest_cmd[1], "%d", &cmd_mode);	
	
	argv[0] = QDAF_PROG;
	argv[1] = quest_cmd[1];
	switch (cmd_mode) {
	case QUEST_QDAF_ACTION_RESULT_ERASE :
		QUEST_PRINT("%s : qdaf will erase failed count\n",__func__);
		wait_val = UMH_WAIT_PROC;
		break;
	default :
		QUEST_PRINT("%s : return because invalid cmd mode(%d)\n", __func__, cmd_mode);
		return count;
	}
	
	ret_userapp = call_usermodehelper(argv[0], argv, envp, wait_val);
	if (!ret_userapp)
		QUEST_PRINT("%s : succeded to trigger qdaf.sh\n", __func__);
	else {
		// evaulate return value after ret_userapp>>8
		QUEST_PRINT("%s : failed to trigger qdaf.sh. error=%d\n", __func__, ret_userapp);
	}
	return count;
}

static DEVICE_ATTR(nad_qdaf_result, S_IRUGO | S_IWUSR, show_quest_qdaf_result, store_quest_qdaf_result);

static void qdaf_save_logs(void)
{
	int fd, idx=0;
	char temp[1]={'\0'}, buf[BUFF_SZ]={'\0',};

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd = sys_open(QDAF_QMESA_LOG, O_RDONLY, 0);
	if (fd >= 0) {
		while (sys_read(fd, temp, 1) == 1) {
			buf[idx++] = temp[0];
			if( temp[0]=='\n' ) {
				buf[idx] = '\0';
				QUEST_PRINT("%s", buf);
				idx = 0;
			}
		}
		sys_close(fd);
	}
	set_fs(old_fs);
}

static ssize_t store_quest_qdaf_debug(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	int idx = 0, cmd_mode=0;
	char temp[QUEST_BUFF_SIZE * 3];
	char quest_cmd[QUEST_CMD_LIST][QUEST_BUFF_SIZE];
	char *quest_ptr, *string;

	QUEST_PRINT("%s : is called\n",__func__);

	if ((int)count < QUEST_BUFF_SIZE) {
		QUEST_PRINT("%s : return error (count=%d)\n", __func__, (int)count);
		return -EINVAL;;
	}

	if (strncmp(buf, "nad_qdaf_debug", 14)) {
		QUEST_PRINT("%s : return error (buf=%s)\n", __func__, buf);
		return -EINVAL;;
	}

	strlcpy(temp, buf, QUEST_BUFF_SIZE * 3);
	string = temp;
	while (idx < QUEST_CMD_LIST) {
		quest_ptr = strsep(&string, ",");
		strlcpy(quest_cmd[idx++], quest_ptr, QUEST_BUFF_SIZE);
	}
	sscanf(quest_cmd[1], "%d", &cmd_mode);

	switch (cmd_mode) {
	case QUEST_QDAF_ACTION_DEBUG_SAVE_LOGS :
		QUEST_PRINT("%s : qdaf will save log into kmsg\n",__func__);
		qdaf_save_logs();
		return count;
	case QUEST_QDAF_ACTION_DEBUG_TRIGGER_PANIC :
		QUEST_PRINT("%s : will trigger panic\n",__func__);
		panic("qdaf_fail");
		return count;
	}

	return count;
}
static DEVICE_ATTR(nad_qdaf_debug, S_IWUSR, NULL, store_quest_qdaf_debug);


static int __init sec_quest_init(void)
{
	int ret = 0;
	struct device *sec_nad;
	struct device *sec_nad_balancer;

	QUEST_PRINT("%s\n", __func__);

	/* Skip quest init when device goes to lp charging */
	if (lpcharge)
		return ret;

	mutex_init(&uefi_parse_lock);
	mutex_init(&hlos_parse_lock);	
	mutex_init(&common_lock);
	mutex_init(&qdaf_call_lock);

	sec_nad = sec_device_create(0, NULL, "sec_nad");

	if (IS_ERR(sec_nad)) {
		QUEST_PRINT("%s Failed to create device(sec_nad)!\n", __func__);
		return PTR_ERR(sec_nad);
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_stat);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_ddrtest_remain_count);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_qmvs_remain_count);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_erase);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_acat);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_dram);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_support);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_logs);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_end);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_dram_debug);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_dram_err_addr);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_result);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_api);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_info);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_version);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_qdaf_control);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_qdaf_debug);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_qdaf_result);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}	

	if (add_uevent_var(&quest_uevent, "NAD_TEST=%s", "DONE")) {
		QUEST_PRINT("%s : uevent NAD_TEST_AND_PASS is failed to add\n",
		       __func__);
		goto err_create_nad_sysfs;
	}

	sec_nad_balancer = sec_device_create(0, NULL, "sec_nad_balancer");

	if (IS_ERR(sec_nad)) {
		QUEST_PRINT("%s Failed to create device(sec_nad)!\n", __func__);
		return PTR_ERR(sec_nad);
	}

	ret = device_create_file(sec_nad_balancer, &dev_attr_balancer);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad_balancer, &dev_attr_timeout);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad_balancer, &dev_attr_run);
	if (ret) {
		QUEST_PRINT("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	return 0;
err_create_nad_sysfs:
	return ret;
}

module_init(sec_quest_init);
