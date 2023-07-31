/* sec_nad.c
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

#include <linux/device.h>
#include <linux/sec_sysfs.h>
#include <linux/sec_nad.h>
#include <linux/fs.h>
#include <linux/sec_class.h>
#include <linux/module.h>

#define NAD_PRINT(format, ...) printk("[NAD] " format, ##__VA_ARGS__)
#define NAD_DEBUG

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/of_address.h>
#include <linux/io.h>
#include <asm/cacheflush.h>
#include <linux/qcom/sec_debug.h>
#include <soc/qcom/subsystem_restart.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/sec_param.h>

#define SMD_QMVS		0
#define MAIN_QMVS		1
#define NOT_SMD_QMVS	2

#define MAX_LEN_STR	1024

/* flag for nad test mode : SMD_QMVS or NOT_SMD_QMVS or MAIN_QMVS*/
static int nad_test_mode = -1;

#define SMD_QMVS_RESULT 		"/nad_refer/QMVS_SMD/test_result.csv"
#define MAIN_QMVS_RESULT 		"/nad_refer/QMVS_MAIN/test_result.csv"
#define NOT_SMD_QMVS_RESULT		"/nad_refer/QMVS/test_result.csv"

#define SMD_QMVS_LOGPATH 		"logPath:/nad_refer/QMVS_SMD"
#define MAIN_QMVS_LOGPATH 		"logPath:/nad_refer/QMVS_MAIN"
#define NOT_SMD_QMVS_LOGPATH	"logPath:/nad_refer/QMVS"

struct param_qnad param_qnad_data;
static int erase_pass;
extern unsigned int lpcharge;
unsigned int clk_limit_flag;
int curr_smd = NOT_SMD_QMVS;
int main_reboot = 0;

char *STR_TEST_ITEM[ITEM_CNT+1] = {
	"SUSPEND",
	"QMESA",
	"PMIC",
	"SDCARD",
	"CRYTOSANITY",
	//
	"FULL" 
};

struct kobj_uevent_env nad_uevent;
extern void nad_set_unlimit_cpufreq(int period);

int get_nad_result(char* buf, int smd, int piece)
{
	int iCnt;
	unsigned int smd_result, not_smd_result;

	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		goto err_out;
	}
	
	not_smd_result = (param_qnad_data.total_test_result & 0xffff0000) >> 16;
	smd_result = param_qnad_data.total_test_result & 0x0000ffff;
	
	NAD_PRINT("%s : param_qnad_data.total_test_result=%u, not_smd_result=%u, smd_result=%u\n",
		__func__, param_qnad_data.total_test_result, not_smd_result, smd_result);
	
	if(smd == NOT_SMD_QMVS)
		smd_result = not_smd_result;
	
	if(piece == ITEM_CNT)
	{
		for(iCnt=0;iCnt<ITEM_CNT;iCnt++)
		{
			switch(smd_result & 0x3)
			{
				case 0 :
					continue;
				case 1 : 
					strcat(buf,"[F]"); break;
				case 2 : 
					strcat(buf,"[P]"); break;
				case 3 : 
					strcat(buf,"[N]"); break;
			}
			
			smd_result >>= 2;
		}
	}
	else
	{
		smd_result >>= 2 * piece;
		switch(smd_result & 0x3)
		{
			case 1 : 
				strcpy(buf,"FAIL"); break;
			case 2 : 
				strcpy(buf,"PASS"); break;
			case 3 : 
				strcpy(buf,"NA"); break;
		}
	}
	
	return ITEM_CNT;
err_out:
	return 0;
}
EXPORT_SYMBOL(get_nad_result);

static int QmvsResult(int smd_nad)
{
	int fd, ret = 0;
	char buf[200] = {'\0',};
 
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
 
	switch(smd_nad) {
		case SMD_QMVS:
			fd = sys_open(SMD_QMVS_RESULT, O_RDONLY, 0);
			break;
		case MAIN_QMVS:
			fd = sys_open(MAIN_QMVS_RESULT, O_RDONLY, 0);
			break;
		case NOT_SMD_QMVS:
			fd = sys_open(NOT_SMD_QMVS_RESULT, O_RDONLY, 0);
			break;
		default :
			break;
	}

	if (fd >= 0) {
		printk(KERN_DEBUG);

		while(sys_read(fd, buf, 200))
		{
			char *ptr;
			char *div = " ";
			char *tok = NULL;

			ptr = buf;
			while((tok = strsep(&ptr, div)) != NULL )
			{
				if(!(strncasecmp(tok, "FAIL",4)))
				{
//					NAD_PRINT("%s",buf);
					ret = -1;
					break;
				}

				if(!(strncasecmp(tok, "NONE",4)))
				{
					ret = -2;
					break;
				}

			}
		}

		sys_close(fd);
	}
	else
	{
		NAD_PRINT("The File is not existed. %s\n", __func__);
		ret = -2;
	}

	set_fs(old_fs);
	
	return ret;
}

static void do_qmvs(int smd_nad)
{
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char *envp[3] = { NULL, NULL, NULL };
	int ret_userapp;

	argv[0] = "/system/vendor/bin/qmvs_sa.sh";
	
	switch(smd_nad) {
		case SMD_QMVS:
			argv[1] = SMD_QMVS_LOGPATH;
			NAD_PRINT("SMD_QMVS, nad_test_mode : %d\n", nad_test_mode);
			break;
		case MAIN_QMVS:
			argv[0] = "/system/vendor/bin/qmvs_sa_main.sh";
			argv[1] = MAIN_QMVS_LOGPATH;
			if(main_reboot == 1)
				argv[2] = "Reboot";
			NAD_PRINT("MAIN_SMD_QMVS, nad_test_mode : %d", nad_test_mode);
			//NAD_PRINT("Setting an argument to reboot after QMVS completes.");
			break;
		case NOT_SMD_QMVS:
			argv[1] = NOT_SMD_QMVS_LOGPATH;
			NAD_PRINT("NOT_SMD_QMVS, nad_test_mode : %d\n", nad_test_mode);
			argv[2] = "Reboot";
			NAD_PRINT("Setting an argument to reboot after QMVS completes.\n");
			break;
		default :
			NAD_PRINT("Invalid smd_nad value, nad_test_mode : %d\n", nad_test_mode);
			break;
	}

	nad_set_unlimit_cpufreq(0);

	envp[0] = "HOME=/";
	envp[1] = "PATH=system/vendor/bin/:/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin";
	ret_userapp = call_usermodehelper( argv[0], argv, envp, UMH_WAIT_EXEC);

	if(!ret_userapp)
	{
		NAD_PRINT("qmvs_sa.sh is executed. ret_userapp = %d\n", ret_userapp);
		
		if(erase_pass)
			erase_pass = 0;
	}
	else
	{
		NAD_PRINT("qmvs_sa.sh is NOT executed. ret_userapp = %d\n", ret_userapp);
		nad_test_mode = -1;
	}
}

static ssize_t show_nad_acat(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		goto err_out;
	}
	NAD_PRINT("%s : magic %x, qmvs cnt %d, ddr cnt %d, ddr result %d\n", __func__, param_qnad_data.magic,param_qnad_data.qmvs_remain_count,param_qnad_data.ddrtest_remain_count,param_qnad_data.ddrtest_result);
	if(QmvsResult(NOT_SMD_QMVS) == 0)
	{
		NAD_PRINT("QMVS Passed\n");
		return sprintf(buf, "OK_ACAT_NONE\n");
	}else if(QmvsResult(NOT_SMD_QMVS) == -1)
	{
		NAD_PRINT("QMVS fail\n");
		return sprintf(buf, "NG_ACAT_ASV\n");
	}else
	{
		NAD_PRINT("QMVS No Run\n");
		return sprintf(buf, "OK\n");
	}
err_out:
	return sprintf(buf, "ERROR\n");
}

static ssize_t store_nad_acat(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
    int ret = -1;
    int idx = 0;
	int qmvs_loop_count, dram_loop_count;
    char temp[NAD_BUFF_SIZE*3];
    char nad_cmd[NAD_CMD_LIST][NAD_BUFF_SIZE];
    char *nad_ptr, *string;
    NAD_PRINT("buf : %s count : %d\n", buf, (int)count);
    if((int)count < NAD_BUFF_SIZE)
        return -EINVAL;

    /* Copy buf to nad temp */
    strncpy(temp, buf, NAD_BUFF_SIZE*3);
    string = temp;

    while(idx < NAD_CMD_LIST) {
        nad_ptr = strsep(&string,",");
        strcpy(nad_cmd[idx++], nad_ptr);
    }

	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		goto err_out;
	}

	if (!strncmp(buf, "nad_acat", 8)) {
		// checking 1st boot and setting test count
		if(param_qnad_data.magic != 0xfaceb00c) // <======================== 1st boot at right after SMD D/L done
		{
			nad_test_mode = SMD_QMVS;
			NAD_PRINT("1st boot at SMD\n");
			param_qnad_data.magic = 0xfaceb00c;
			param_qnad_data.qmvs_remain_count = 0x0;
			param_qnad_data.ddrtest_remain_count = 0x0;
			
			// flushing to param partition
			if (!sec_set_param(param_index_qnad, &param_qnad_data)) {
				pr_err("%s : fail - set param!! param_qnad_data\n", __func__);
				goto err_out;
			}
			curr_smd = nad_test_mode;
			do_qmvs(nad_test_mode);
		}
		else //  <========================not SMD, it can be LCIA, CAL, FINAL and 15 ACAT.
		{
			nad_test_mode = NOT_SMD_QMVS;
			ret = sscanf(nad_cmd[1], "%d\n", &qmvs_loop_count);
			if (ret != 1)
				return -EINVAL;

			ret = sscanf(nad_cmd[2], "%d\n", &dram_loop_count);
			if (ret != 1)
				return -EINVAL;
			
			NAD_PRINT("not SMD, nad_acat%d,%d\n", qmvs_loop_count, dram_loop_count);
			if(!qmvs_loop_count && !dram_loop_count) { // <+++++++++++++++++ both counts are 0, it means 1. testing refers to current remain_count
				
				// stop retrying when failure occur during retry test at ACAT/15test
				if(param_qnad_data.magic == 0xfaceb00c && QmvsResult(NOT_SMD_QMVS) == -1)
				{
					pr_err("%s : qmvs test fail - set the remain counts to 0\n", __func__);
					param_qnad_data.qmvs_remain_count = 0;
					param_qnad_data.ddrtest_remain_count = 0;
					
					// flushing to param partition
					if (!sec_set_param(param_index_qnad, &param_qnad_data)) {
						pr_err("%s : fail - set param!! param_qnad_data\n", __func__);
						goto err_out;
					}
				}

				if(param_qnad_data.qmvs_remain_count > 0) { // QMVS count still remain
					NAD_PRINT("qmvs : qmvs_remain_count = %d, ddrtest_remain_count = %d\n", param_qnad_data.qmvs_remain_count, param_qnad_data.ddrtest_remain_count);
					param_qnad_data.qmvs_remain_count--;
					param_qnad_data.total_test_result &= 0x0000ffff; // NOT smd result is initialized.
					
/*					if(param_qnad_data.ddrtest_remain_count && param_qnad_data.qmvs_remain_count == 0) { // last QMVS count, and next is ddr test. rebooting will be done by QMVS
						NAD_PRINT("switching : qmvs_remain_count = %d, ddrtest_remain_count = %d\n", param_qnad_data.qmvs_remain_count, param_qnad_data.ddrtest_remain_count);
						param_qnad_data.ddrtest_remain_count--;
						
						do_ddrtest();
					}*/

					// flushing to param partition
					if (!sec_set_param(param_index_qnad, &param_qnad_data)) {
						pr_err("%s : fail - set param!! param_qnad_data\n", __func__);
						goto err_out;
					}

					curr_smd = nad_test_mode;
					do_qmvs(nad_test_mode);
				}
				else if(param_qnad_data.ddrtest_remain_count) { // QMVS already done before, only ddr test remains. then it needs selfrebooting.
					NAD_PRINT("ddrtest : qmvs_remain_count = %d, ddrtest_remain_count = %d\n", param_qnad_data.qmvs_remain_count, param_qnad_data.ddrtest_remain_count);

					do_msm_restart(REBOOT_HARD, "sec_debug_hw_reset");
					while (1)
						;
				}
			}
			else { // <+++++++++++++++++ not (0,0) means 1. new test count came, 2. so overwrite the remain_count, 3. and not reboot by itsself, 4. reboot cmd will come from outside like factory PGM
				
				param_qnad_data.qmvs_remain_count = qmvs_loop_count;
				param_qnad_data.ddrtest_remain_count = dram_loop_count;
				// flushing to param partition
				if (!sec_set_param(param_index_qnad, &param_qnad_data)) {
					pr_err("%s : fail - set param!! param_qnad_data\n", __func__);
					goto err_out;
				}
				
				NAD_PRINT("new cmd : qmvs_remain_count = %d, ddrtest_remain_count = %d\n", param_qnad_data.qmvs_remain_count, param_qnad_data.ddrtest_remain_count);
			}
		}
		return count;
	} else
		return count;
err_out:
	return count;
}

static DEVICE_ATTR(nad_acat, S_IRUGO | S_IWUSR,  show_nad_acat, store_nad_acat);

static ssize_t show_nad_stat(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		goto err_out;
	}
	NAD_PRINT("%s : magic %x, qmvs cnt %d, ddr cnt %d, ddr result %d\n", __func__, param_qnad_data.magic,param_qnad_data.qmvs_remain_count,param_qnad_data.ddrtest_remain_count,param_qnad_data.ddrtest_result);

	if(QmvsResult(SMD_QMVS) == 0 && param_qnad_data.magic == 0xfaceb00c)
	{
		NAD_PRINT("QMVS Passed\n");
		return sprintf(buf, "OK_2.0\n");
	}else if(QmvsResult(SMD_QMVS) == -1 && param_qnad_data.magic == 0xfaceb00c)
	{
		ssize_t ret=0;
		char strResult[NAD_BUFF_SIZE*3]={'\0',};
		NAD_PRINT("QMVS fail\n");
		
		get_nad_result(strResult, SMD_QMVS, ITEM_CNT);
		ret = snprintf(buf, sizeof("NG_2.0_FAIL_")+sizeof(strResult)+1,"NG_2.0_FAIL_%s\n", strResult);
		NAD_PRINT("%s, %s", __func__, buf);

		return ret;
	}else if(param_qnad_data.magic == 0xfaceb00c)
	{
		NAD_PRINT("QMVS Magic exist and Empty log\n");
		return sprintf(buf, "RE_WORK\n");
	}else
	{
		NAD_PRINT("QMVS No Run\n");
		return sprintf(buf, "OK\n");
	}
err_out:
	return sprintf(buf, "ERROR\n");
}

static DEVICE_ATTR(nad_stat, S_IRUGO, show_nad_stat, NULL);

static ssize_t show_ddrtest_remain_count(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		goto err_out;
	}

	return sprintf(buf, "%d\n", param_qnad_data.ddrtest_remain_count);
err_out:
    return sprintf(buf, "PARAM ERROR\n");
}

static DEVICE_ATTR(nad_ddrtest_remain_count, S_IRUGO, show_ddrtest_remain_count, NULL);

static ssize_t show_qmvs_remain_count(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		goto err_out;
	}

	return sprintf(buf, "%d\n", param_qnad_data.qmvs_remain_count);
err_out:
    return sprintf(buf, "PARAM ERROR\n");	
}

static DEVICE_ATTR(nad_qmvs_remain_count, S_IRUGO, show_qmvs_remain_count, NULL);

static ssize_t store_nad_erase(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
    if (!strncmp(buf, "erase", 5)) {
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char *envp[3] = { NULL, NULL, NULL };
	int ret_userapp;
	int api_gpio_test = 0;
	char api_gpio_test_result[256] = {0,};

	argv[0] = "/system/vendor/bin/remove_files.sh";
			
	envp[0] = "HOME=/";
	envp[1] = "PATH=system/vendor/bin/:/sbin:/vendor/bin:/system/sbin:/system/bin:/system/xbin";
	ret_userapp = call_usermodehelper( argv[0], argv, envp, UMH_WAIT_EXEC);
	
	if(!ret_userapp)
	{
		NAD_PRINT("remove_files.sh is executed. ret_userapp = %d\n", ret_userapp);
		erase_pass = 1;
	}
	else
	{
		NAD_PRINT("remove_files.sh is NOT executed. ret_userapp = %d\n", ret_userapp);
		erase_pass = 0;
	}

		if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
			pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
			goto err_out;
		}

		param_qnad_data.magic = 0x0;
		param_qnad_data.qmvs_remain_count = 0x0;
		param_qnad_data.ddrtest_remain_count = 0x0;
		param_qnad_data.ddrtest_result= 0x0;
		param_qnad_data.total_test_result = 0x0;

		// flushing to param partition
		if (!sec_set_param(param_index_qnad, &param_qnad_data)) {
			pr_err("%s : fail - set param!! param_qnad_data\n", __func__);
			goto err_out;
		}

		NAD_PRINT("clearing MAGIC code done = %d\n", param_qnad_data.magic);
		NAD_PRINT("qmvs_remain_count = %d\n", param_qnad_data.qmvs_remain_count);
		NAD_PRINT("ddrtest_remain_count = %d\n", param_qnad_data.ddrtest_remain_count);
		NAD_PRINT("ddrtest_result = %d\n", param_qnad_data.ddrtest_result);

		// clearing API test result
		if (!sec_set_param(param_index_api_gpio_test, &api_gpio_test)) {
			pr_err("%s : fail - set param!! param_qnad_data\n", __func__);
			goto err_out;
		}
		
		if (!sec_set_param(param_index_api_gpio_test_result, api_gpio_test_result)) {
			pr_err("%s : fail - set param!! param_qnad_data\n", __func__);
			goto err_out;
		}
        return count;
    } else
        return count;
err_out:
        return count;
}

static ssize_t show_nad_erase(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
    if (erase_pass)
        return sprintf(buf, "OK\n");
    else
        return sprintf(buf, "NG\n");
}

static DEVICE_ATTR(nad_erase, S_IRUGO | S_IWUSR, show_nad_erase, store_nad_erase);

static ssize_t show_nad_dram(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	int ret=0;

	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		goto err_out;
	}
	
	ret = param_qnad_data.ddrtest_result;
  
	if (ret == 0x11)
	    return sprintf(buf, "OK_DRAM\n");
	else if (ret == 0x22)
	    return sprintf(buf, "NG_DRAM_DATA\n");
    else
        return sprintf(buf, "NO_DRAMTEST\n");
err_out:
    return sprintf(buf, "READ ERROR\n");
}
static DEVICE_ATTR(nad_dram, S_IRUGO, show_nad_dram, NULL);

static ssize_t show_nad_dram_debug(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
//	int ret=0;

	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		goto err_out;
	}
	
	return sprintf(buf, "0x%x\n", param_qnad_data.ddrtest_result);
err_out:
    return sprintf(buf, "READ ERROR\n");	
}
static DEVICE_ATTR(nad_dram_debug, S_IRUGO, show_nad_dram_debug, NULL);

static ssize_t show_nad_dram_err_addr(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	int ret=0;
	int i=0;
	struct param_qnad_ddr_result param_qnad_ddr_result_data;

	if (!sec_get_param(param_index_qnad_ddr_result, &param_qnad_ddr_result_data)) {
		pr_err("%s : fail - get param!! param_qnad_ddr_result_data\n", __func__);
		goto err_out;
	}

	ret = sprintf(buf, "Total : %d\n\n", param_qnad_ddr_result_data.ddr_err_addr_total);
	for(i = 0; i < param_qnad_ddr_result_data.ddr_err_addr_total; i++)
	{
		ret += sprintf(buf+ret-1, "[%d] 0x%llx\n", i, param_qnad_ddr_result_data.ddr_err_addr[i]);
	}
	
	return ret;
err_out:
    return sprintf(buf, "READ ERROR\n");
}
static DEVICE_ATTR(nad_dram_err_addr, S_IRUGO, show_nad_dram_err_addr, NULL);

static ssize_t show_nad_support(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
#if defined(CONFIG_ARCH_MSM8998) || defined(CONFIG_ARCH_MSM8996)
        return sprintf(buf, "SUPPORT\n");
#else
	return sprintf(buf, "NOT_SUPPORT\n");
#endif
}
static DEVICE_ATTR(nad_support, S_IRUGO, show_nad_support, NULL);

static ssize_t store_qmvs_logs(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
	int fd = 0, i = 0;
	char logbuf[500] = {'\0'};
	char path[100] = {'\0'};
	char ptr;

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	NAD_PRINT("%s\n",buf);

	sscanf(buf, "%s", path);
	fd = sys_open(path, O_RDONLY, 0);

	if (fd >= 0) {
		while(sys_read(fd, &ptr, 1) && ptr != -1)
		{
			//NAD_PRINT("%c\n", ptr);
			logbuf[i] = ptr;
			i++;
			if(ptr == '\n')
			{
				NAD_PRINT("%s",logbuf);
				i = 0;
			}
		}

		sys_close(fd);
	}
	else
	{
		NAD_PRINT("The File is not existed. %s\n", __func__);
	}
	
	set_fs(old_fs);
	return count;
}
static DEVICE_ATTR(nad_logs, S_IRUGO | S_IWUSR, NULL, store_qmvs_logs);

static ssize_t store_qmvs_end(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
	char result[20] = {'\0'};

	NAD_PRINT("result : %s\n",buf);

	sscanf(buf, "%s", result);

	if(!strcmp(result, "nad_pass"))
	{
		kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, nad_uevent.envp);
		NAD_PRINT("NAD result : %s, nad_pass, Send to Process App for Nad test end : %s\n", result, __func__);
	}
	else
	{
		if(nad_test_mode == NOT_SMD_QMVS)
		{
			NAD_PRINT("NAD result : %s, Device enter the upload mode because it is NOT_SMD_QMVS : %s\n", result, __func__);
			panic(result);
		}
		else
		{
			kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, nad_uevent.envp);
			NAD_PRINT("NAD result : %s, Send to Process App for Nad test end : %s\n", result, __func__);
		}
	}

	nad_test_mode = -1;
	return count;
}
static DEVICE_ATTR(nad_end, S_IRUGO | S_IWUSR, NULL, store_qmvs_end);

static ssize_t show_nad_api(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	unsigned int api_gpio_test;
	char api_gpio_test_result[256];

	if (!sec_get_param(param_index_api_gpio_test, &api_gpio_test)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		goto err_out;
	}
	
	if(api_gpio_test) {
		if (!sec_get_param(param_index_api_gpio_test_result, api_gpio_test_result)) {
			pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
			goto err_out;
		}
		return sprintf(buf, "%s", api_gpio_test_result);
	} else
		return sprintf(buf, "NONE\n");
		
err_out:
    return sprintf(buf, "READ ERROR\n");
}

static DEVICE_ATTR(nad_api, S_IRUGO | S_IWUSR, show_nad_api, NULL);

static ssize_t store_qmvs_result(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
	int _result = -1;
	char test_name[NAD_BUFF_SIZE*2] = {'\0',};
    char temp[NAD_BUFF_SIZE*3] = {'\0',};
    char nad_test[2][NAD_BUFF_SIZE*2]; // 2: "test_name", "result"
	char result_string[NAD_BUFF_SIZE] = {'\0',};
    char *nad_ptr, *string;
	int smd = curr_smd;
	int item = -1;
	
	NAD_PRINT("buf : %s count : %d\n", buf, (int)count);
    
	if(NAD_BUFF_SIZE*3 < (int)count || (int)count < 4)
	{
		NAD_PRINT("result cmd size too long : NAD_BUFF_SIZE<%d\n", (int)count);
        return -EINVAL;
	}

    /* Copy buf to nad temp */
    strncpy(temp, buf, count);
    string = temp;
	
	nad_ptr = strsep(&string,",");
	strcpy(nad_test[0], nad_ptr);
	nad_ptr = strsep(&string,",");
	strcpy(nad_test[1], nad_ptr);
	
	sscanf(nad_test[0], "%s", test_name);
	sscanf(nad_test[1], "%s", result_string);
	
	NAD_PRINT("test_name : %s, test result=%s\n", test_name, result_string);
	
	if(TEST_PASS(result_string))
		_result = 2;
	else if(TEST_FAIL(result_string))
		_result = 1;
	else
		_result = 0;
	
	// _results = 0(N/A), 1(FAIL), 2(PASS) from qmvs_sa.sh
	
	if(TEST_SUSPEND(test_name))
		item = SUSPEND;
	else if(TEST_QMESA(test_name)) 
		item = QMESA;
	else if(TEST_PMIC(test_name)) 
		item = PMIC;
	else if(TEST_SDCARD(test_name)) 
		item = SDCARD;
	else if(TEST_CRYTOSANITY(test_name)) 
		item = CRYTOSANITY;
	else
		item = NOT_ASSIGN;
	
	if(item == NOT_ASSIGN)
	{
		pr_err("%s : fail - get test item in qmvs_sa.sh!! \n", __func__);
		return count;	
	}

	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		return -EINVAL;
	}

	param_qnad_data.total_test_result |= TEST_ITEM_RESULT(smd, item, _result);
	
	NAD_PRINT("total_test_result=%u, smd=%d, item=%d, _result=%d\n",  param_qnad_data.total_test_result, smd, item, _result);
	
	if (!sec_set_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		return -EINVAL;
	}

	return count;
}

static ssize_t show_qmvs_result(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	ssize_t info_size = 0;
	int iCnt;

	for(iCnt=0; iCnt < ITEM_CNT; iCnt++)
	{
		char strResult[NAD_BUFF_SIZE]={'\0',};
		
		if(!get_nad_result(strResult, SMD_QMVS, iCnt))
			goto err_out;
		
		info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,"\"%s\":\"%s\",", STR_TEST_ITEM[iCnt], strResult);
	}
	
	pr_info("%s, result=%s\n", __func__, buf);

	return info_size;
err_out:
	return sprintf(buf, "PARAM ERROR\n");
}
static DEVICE_ATTR(nad_result, S_IRUGO | S_IWUSR, show_qmvs_result, store_qmvs_result);

static ssize_t store_qmvs_info(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
	char info_name[NAD_BUFF_SIZE*2] = {'\0',};
    char temp[NAD_BUFF_SIZE*3] = {'\0',};
    char nad_test[2][NAD_BUFF_SIZE*2]; // 2: "info_name", "result"
	int resultValue;
    char *nad_ptr, *string;
	
	NAD_PRINT("buf : %s count : %d\n", buf, (int)count);
    
	if(NAD_BUFF_SIZE*3 < (int)count || (int)count < 4)
	{
		NAD_PRINT("result cmd size too long : NAD_BUFF_SIZE<%d\n", (int)count);
        return -EINVAL;
	}

    /* Copy buf to nad temp */
    strncpy(temp, buf, count);
    string = temp;
	
	nad_ptr = strsep(&string,",");
	strcpy(nad_test[0], nad_ptr);
	nad_ptr = strsep(&string,",");
	strcpy(nad_test[1], nad_ptr);
	
	sscanf(nad_test[0], "%s", info_name);
	sscanf(nad_test[1], "%d", &resultValue);
	
	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		return -EINVAL;
	}
	
	if(!strcmp("thermal",info_name))
		param_qnad_data.thermal = resultValue;
	else if(!strcmp("clock",info_name))
		param_qnad_data.tested_clock = resultValue;

	NAD_PRINT("info_name : %s, result=%d\n", info_name, resultValue);
	
	if (!sec_set_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		return -EINVAL;
	}

	return count;
}

static ssize_t show_qmvs_info(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	ssize_t info_size = 0;

	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		goto err_out;
	}
	
	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,"\"REMAIN_CNT\":\"%d\",", param_qnad_data.qmvs_remain_count);
	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,"\"THERMAL\":\"%d\",", param_qnad_data.thermal);
	info_size += snprintf((char*)(buf+info_size), MAX_LEN_STR - info_size,"\"CLOCK\":\"%d\",", param_qnad_data.tested_clock);

	return info_size;	
err_out:
    return sprintf(buf, "PARAM ERROR\n");	
}
static DEVICE_ATTR(nad_info, S_IRUGO | S_IWUSR, show_qmvs_info, store_qmvs_info);

static ssize_t store_nad_qmvs_main(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
	int idx = 0;
	int ret = -1;
    char temp[NAD_BUFF_SIZE*3];
    char nad_cmd[NAD_MAIN_CMD_LIST][NAD_BUFF_SIZE];
    char *nad_ptr, *string;
	int running_time;


	/* Copy buf to nad temp */
    strncpy(temp, buf, NAD_BUFF_SIZE*3);
    string = temp;

    while(idx < NAD_MAIN_CMD_LIST) {
        nad_ptr = strsep(&string,",");
        strcpy(nad_cmd[idx++], nad_ptr);
    }
	
	if(nad_test_mode == MAIN_QMVS) {
		NAD_PRINT("duplicated!\n");
		return count;
	}
		
	if (!strncmp(buf, "start", 5)) {
		ret = sscanf(nad_cmd[1], "%d\n", &running_time);
			if (ret != 1)
				return -EINVAL;

		main_reboot = 1;

		nad_test_mode = MAIN_QMVS;

		do_qmvs(MAIN_QMVS);
	}
	
	return count;
}


static ssize_t show_nad_qmvs_main(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	if(QmvsResult(MAIN_QMVS) == 0 && param_qnad_data.magic == 0xfaceb00c)
	{
		NAD_PRINT("QMVS Passed\n");
		return sprintf(buf, "OK_2.0\n");
	}else if(QmvsResult(MAIN_QMVS) == -1 && param_qnad_data.magic == 0xfaceb00c)
	{
		NAD_PRINT("QMVS fail\n");
		return sprintf(buf, "NG_2.0_FAIL\n");
	}else
	{
		NAD_PRINT("QMVS No Run\n");
		return sprintf(buf, "OK\n");
	}
}
static DEVICE_ATTR(balancer, S_IRUGO | S_IWUSR, show_nad_qmvs_main, store_nad_qmvs_main);

static ssize_t show_nad_main_timeout(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	return sprintf(buf, "%d\n", NAD_MAIN_TIMEOUT);
}
static DEVICE_ATTR(timeout, S_IRUGO | S_IWUSR, show_nad_main_timeout, NULL);
static int __init sec_nad_init(void)
{
	int ret = 0;
	struct device* sec_nad;
	struct device* sec_nad_balancer;

	NAD_PRINT("%s\n", __func__);

	/* Skip nad init when device goes to lp charging */
	if(lpcharge) 
		return ret;

	sec_nad = device_create(sec_class, NULL, 0, NULL, "sec_nad");

	if (IS_ERR(sec_nad)) {
		pr_err("%s Failed to create device(sec_nad)!\n", __func__);
		return PTR_ERR(sec_nad);
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_stat); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}
	
	ret = device_create_file(sec_nad, &dev_attr_nad_ddrtest_remain_count); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}
	
	ret = device_create_file(sec_nad, &dev_attr_nad_qmvs_remain_count); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_erase); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

    ret = device_create_file(sec_nad, &dev_attr_nad_acat); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_dram); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_support); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}
	
	ret = device_create_file(sec_nad, &dev_attr_nad_logs); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_end); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_dram_debug); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_dram_err_addr); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_result);
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}
	
	ret = device_create_file(sec_nad, &dev_attr_nad_api);
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}
	
	ret = device_create_file(sec_nad, &dev_attr_nad_info);
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	if(add_uevent_var(&nad_uevent, "NAD_TEST=%s", "DONE"))
	{
		pr_err("%s : uevent NAD_TEST_AND_PASS is failed to add\n", __func__);
		goto err_create_nad_sysfs;	  
	}

	sec_nad_balancer = device_create(sec_class, NULL, 0, NULL, "sec_nad_balancer");

	if (IS_ERR(sec_nad)) {
		pr_err("%s Failed to create device(sec_nad)!\n", __func__);
		return PTR_ERR(sec_nad);
	}
	
	ret = device_create_file(sec_nad_balancer, &dev_attr_balancer);
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}
	
	ret = device_create_file(sec_nad_balancer, &dev_attr_timeout);
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	return 0;
err_create_nad_sysfs:
	return ret;
}

module_init(sec_nad_init);
