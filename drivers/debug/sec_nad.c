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
//#include <linux/sec_sysfs.h>
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
#include <asm-generic/cacheflush.h>
#include <linux/qcom/sec_debug.h>
#include <soc/qcom/subsystem_restart.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/sec_param.h>

#define SMD_QMVS		0
#define NOT_SMD_QMVS	1

#define SMD_QMVS_RESULT 		"/efs/QMVS/SMD/test_result.csv"
#define NOT_SMD_QMVS_RESULT		"/efs/QMVS/test_result.csv"

#define SMD_QMVS_LOGPATH 		"logPath:/efs/QMVS/SMD"
#define NOT_SMD_QMVS_LOGPATH	"logPath:/efs/QMVS"
struct param_qnad param_qnad_data;
static int erase_pass;
extern unsigned int lpcharge;
unsigned int clk_limit_flag;

struct kobj_uevent_env nad_uevent;

static int QmvsResult(int smd)
{
	int fd, ret = 0;
	char buf[200] = {'\0',};
 
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
 
	switch(smd) {
		case SMD_QMVS:
			fd = sys_open(SMD_QMVS_RESULT, O_RDONLY, 0);
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
					NAD_PRINT("%s",buf);
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
		NAD_PRINT("The File is not existed\n");
		ret = -2;
	}

	set_fs(old_fs);
	
	return ret;
}

static void do_qmvs(int smd)
{
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char *envp[3] = { NULL, NULL, NULL };
//	char options[5] = {'\0',};
	int ret_userapp;
	int i;

	#define MAX_SUBSYSTEM_PUT_COUNT 3
	struct subsys_device *modem_subsys = find_subsys_for_NAD("modem");

	if(subsystem_get_count_for_NAD(modem_subsys) > 0) {
		NAD_PRINT("modem subsystem count : %d\n", subsystem_get_count_for_NAD(modem_subsys));
		NAD_PRINT("try to turn off the modem subsystem.\n");

		modem_subsys = subsystem_get("modem"); 

		if (modem_subsys) {
			for(i = 0; i < MAX_SUBSYSTEM_PUT_COUNT; i++) {
				if(subsystem_get_count_for_NAD(modem_subsys) > 0) {
					subsystem_put((void*)modem_subsys);
					NAD_PRINT("put %d, modem subsystem count : %d\n", i, subsystem_get_count_for_NAD(modem_subsys));
				}
			}
		}
	} else {
		NAD_PRINT("modem subsystem count : %d\n", subsystem_get_count_for_NAD(modem_subsys));
		NAD_PRINT("modem subsystem already turned off.\n");
	}


	argv[0] = "/system/vendor/bin/qmvs_sa.sh";
	
	switch(smd) {
		case SMD_QMVS:
			argv[1] = SMD_QMVS_LOGPATH;
			break;
		case NOT_SMD_QMVS:
			argv[1] = NOT_SMD_QMVS_LOGPATH;
			break;
		default :
			break;
	}
		
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
	}
	clk_limit_flag = 1;
}

static ssize_t show_nad_acat(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	int smd = NOT_SMD_QMVS;

	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		goto err_out;
	}
	
	NAD_PRINT("%s : magic %x, qmvs cnt %d, ddr cnt %d, ddr result %d\n", __func__, param_qnad_data.magic,param_qnad_data.qmvs_remain_count,param_qnad_data.ddrtest_remain_count,param_qnad_data.ddrtest_result);
	if(QmvsResult(smd) == 0)
	{
		NAD_PRINT("QMVS Passed\n");
		return sprintf(buf, "OK_ACAT_NONE\n");
	}else if(QmvsResult(smd) == -1)
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
	int smd = NOT_SMD_QMVS;
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
			NAD_PRINT("1st boot at SMD\n");
			param_qnad_data.magic = 0xfaceb00c;
			param_qnad_data.qmvs_remain_count = 0x0;
			param_qnad_data.ddrtest_remain_count = 0x0;
			
			// flushing to param partition
			if (!sec_set_param(param_index_qnad, &param_qnad_data)) {
				pr_err("%s : fail - set param!! param_qnad_data\n", __func__);
				goto err_out;
			}
			smd = SMD_QMVS;
			do_qmvs(smd);
		}
		else //  <========================not SMD, it can be LCIA, CAL, FINAL and 15 ACAT.
		{
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

				if(param_qnad_data.qmvs_remain_count) { // QMVS count still remain
					NAD_PRINT("qmvs : qmvs_remain_count = %d, ddrtest_remain_count = %d\n", param_qnad_data.qmvs_remain_count, param_qnad_data.ddrtest_remain_count);
					param_qnad_data.qmvs_remain_count--;

					// flushing to param partition
					if (!sec_set_param(param_index_qnad, &param_qnad_data)) {
						pr_err("%s : fail - set param!! param_qnad_data\n", __func__);
						goto err_out;
					}

					do_qmvs(smd);
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
	int smd = SMD_QMVS;

	if (!sec_get_param(param_index_qnad, &param_qnad_data)) {
		pr_err("%s : fail - get param!! param_qnad_data\n", __func__);
		goto err_out;
	}
	NAD_PRINT("%s : magic %x, qmvs cnt %d, ddr cnt %d, ddr result %d\n", __func__, param_qnad_data.magic,param_qnad_data.qmvs_remain_count,param_qnad_data.ddrtest_remain_count,param_qnad_data.ddrtest_result);

	if(QmvsResult(smd) == 0 && param_qnad_data.magic == 0xfaceb00c)
	{
		NAD_PRINT("QMVS Passed\n");
	return sprintf(buf, "OK_2.0\n");
	}else if(QmvsResult(smd) == -1 && param_qnad_data.magic == 0xfaceb00c)
	{
		NAD_PRINT("QMVS fail\n");
        return sprintf(buf, "NG_2.0_FAIL\n");        
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
		param_qnad_data.ddrtest_result = 0x0;

		// flushing to param partition
		if (!sec_set_param(param_index_qnad, &param_qnad_data)) {
			pr_err("%s : fail - set param!! param_qnad_data\n", __func__);
			goto err_out;
		}

		NAD_PRINT("clearing MAGIC code done = %d\n", param_qnad_data.magic);
		NAD_PRINT("qmvs_remain_count = %d\n", param_qnad_data.qmvs_remain_count);
		NAD_PRINT("ddrtest_remain_count = %d\n", param_qnad_data.ddrtest_remain_count);
		NAD_PRINT("ddrtest_result = %d\n", param_qnad_data.ddrtest_result);

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
	int fd = 0;
	char logbuf[256] = {'\0'};
	char path[100] = {'\0'};

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	NAD_PRINT("%s\n",buf);

	sscanf(buf, "%s", path);
	fd = sys_open(path, O_RDONLY, 0);

	if (fd >= 0) {
		while(sys_read(fd, logbuf, 256))
		{
			NAD_PRINT("%s",logbuf);
		}

		sys_close(fd);
	}
	else
	{
		NAD_PRINT("The File is not existed\n");
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
	void *modem_subsys = subsystem_get("modem"); 

	if (modem_subsys)
	{
		NAD_PRINT("failed to get modem\n");
	}

	NAD_PRINT("result : %s\n",buf);

	sscanf(buf, "%s", result);

	if(!strcmp(result, "nad_pass"))
	{
		kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, nad_uevent.envp);
		NAD_PRINT("Send to Process App for Nad test end : %s\n", __func__);
	}
	else
		panic(result);

	return count;
}
static DEVICE_ATTR(nad_end, S_IRUGO | S_IWUSR, NULL, store_qmvs_end);

static int __init sec_nad_init(void)
{
	int ret = 0;
	struct device* sec_nad;

	NAD_PRINT("%s\n", __func__);

	/* Skip nad init when device goes to lp charging */
//	if(lpcharge) 
//		return ret;

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

	if(add_uevent_var(&nad_uevent, "NAD_TEST=%s", "DONE"))
	{
		pr_err("%s : uevent NAD_TEST_AND_PASS is failed to add\n", __func__);
		goto err_create_nad_sysfs;	
	}

	return 0;
err_create_nad_sysfs:
	return ret;
}

module_init(sec_nad_init);
