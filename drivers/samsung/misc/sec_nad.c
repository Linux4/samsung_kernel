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
#include <linux/module.h>
#include <linux/sec_nad.h>
#include <linux/fs.h>
#include <linux/sec_class.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/delay.h>

#define NAD_PRINT(format, ...) printk("[NAD] " format, ##__VA_ARGS__)
#define NAD_DEBUG

#define REBOOT "Reboot"


#define SMD_NAD_PROG		"/system/bin/mnad/mtk_nad_exec.sh"
#define ETC_NAD_PROG		"/system/bin/mnad/mtk_nad_exec.sh"
#define ERASE_NAD_PRG		"/system/bin/mnad/remove_mtk_nad_files.sh"

#define SMD_NAD_RESULT		"/sdcard/log/mtknad/NAD_SMD/test_result.csv"
#define ETC_NAD_RESULT		"/sdcard/log/mtknad/NAD/test_result.csv"

#define SMD_NAD_LOGPATH		"logPath:/sdcard/log/mtknad/NAD_SMD"
#define ETC_NAD_LOGPATH		"logPath:/sdcard/log/mtknad/NAD"

struct kobj_uevent_env nad_uevent;

static void sec_nad_param_update(struct work_struct *work)
{
	int ret = -1;
	struct file *fp;
	struct sec_nad_param *param_data = container_of(work, struct sec_nad_param, sec_nad_work);

	NAD_PRINT("%s: set param at %s\n", __func__, param_data->state ? "write" : "read");

	fp = filp_open(NAD_PARAM_NAME, O_RDWR | O_SYNC, 0);
	if (IS_ERR(fp)) {
		pr_err("%s: filp_open error %ld\n", __func__, PTR_ERR(fp));

		/* Rerun workqueue when nad_refer read fail */
		if(param_data->retry_cnt > param_data->curr_cnt) {
			NAD_PRINT("retry count : %d\n", param_data->curr_cnt++);
			schedule_delayed_work(&sec_nad_param_data.sec_nad_delay_work, HZ * 1);
		}
		goto complete_exit;
	}

	ret = fp->f_op->llseek(fp, param_data->offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s: llseek error %d!\n", __func__, ret);
		goto close_fp_out;
	}

	switch(param_data->state){
		case NAD_PARAM_WRITE:
			ret = vfs_write(fp, (char*)&sec_nad_env, sizeof(struct nad_env), &(fp->f_pos));

			NAD_PRINT("curr_step : %d\n", sec_nad_env.curr_step);
			NAD_PRINT("nad_acat_remaining_count : %d\n", sec_nad_env.nad_acat_remaining_count);
			NAD_PRINT("nad_dramtest_remaining_count : %d\n", sec_nad_env.nad_dramtest_remaining_count);
			NAD_PRINT("nad_smd_result : %d\n", sec_nad_env.nad_smd_result);
			NAD_PRINT("nad_dram_test_result : %d\n", sec_nad_env.nad_dram_test_result);
			NAD_PRINT("nad_dram_fail_data : %d\n", sec_nad_env.nad_dram_fail_data);
			NAD_PRINT("nad_dram_fail_address : 0x%016llx\n", sec_nad_env.nad_dram_fail_address);
			NAD_PRINT("smd_item_result : %s\n", sec_nad_env.smd_item_result);

			if (ret < 0) pr_err("%s: write error! %d\n", __func__, ret);
			break;

		case NAD_PARAM_READ:
			ret = vfs_read(fp, (char*)&sec_nad_env, sizeof(struct nad_env), &(fp->f_pos));

			NAD_PRINT("curr_step : %d\n", sec_nad_env.curr_step);
			NAD_PRINT("nad_acat_remaining_count : %d\n", sec_nad_env.nad_acat_remaining_count);
			NAD_PRINT("nad_dramtest_remaining_count : %d\n", sec_nad_env.nad_dramtest_remaining_count);
			NAD_PRINT("nad_smd_result : %d\n", sec_nad_env.nad_smd_result);
			NAD_PRINT("nad_dram_test_result : %d\n", sec_nad_env.nad_dram_test_result);
			NAD_PRINT("nad_dram_fail_data : %d\n", sec_nad_env.nad_dram_fail_data);
			NAD_PRINT("nad_dram_fail_address : 0x%016llx\n", sec_nad_env.nad_dram_fail_address);
			NAD_PRINT("smd_item_result : %s\n", sec_nad_env.smd_item_result);

			if (ret < 0) pr_err("%s: read error! %d\n", __func__, ret);
			break;
	}
	
close_fp_out:
	if (fp) filp_close(fp, NULL);
complete_exit:
	complete(&sec_nad_param_data.comp);
	NAD_PRINT("%s: exit %d\n", __func__, ret);
	return;
}

int sec_set_nad_param(int val)
{
	int ret = -1;
	
	mutex_lock(&sec_nad_param_lock);

	switch(val) {
		case NAD_PARAM_WRITE:
		case NAD_PARAM_READ:
			goto set_param;
		default:
			goto unlock_out;
	}

set_param:
	sec_nad_param_data.state = val;
	schedule_work(&sec_nad_param_data.sec_nad_work);
	wait_for_completion(&sec_nad_param_data.comp);
	/* how to determine to return success or fail ? */
	ret = 0;
unlock_out:
	mutex_unlock(&sec_nad_param_lock);

	return ret;
}

static void sec_nad_init_update(struct work_struct *work)
{
	int ret = -1;

	NAD_PRINT("%s\n", __func__);

	ret = sec_set_nad_param(NAD_PARAM_READ);
	if (ret < 0)
		pr_err("%s: read error! %d\n", __func__, ret);
}
void append(char *dst, char *org, int cnt) {
    char *p = dst;
	int i=0;
    while (*p != '\0') {
		if(p >= dst+SMD_ITEM_RESULT_LENGTH) return;
		p++;
	}

	for(i=0;i<cnt;i++){
		if(p >= dst+SMD_ITEM_RESULT_LENGTH) return;
		*p = *org;
		p++;
		org++;
	}
    *p = '\0';
}
void appendc(char *dst, char c) {
    char *p = dst;
    while (*p != '\0') {
		if(p >= dst+SMD_ITEM_RESULT_LENGTH) return;
		p++;
	}
    *p = c;
    *(p+1) = '\0';
}
static int Nad_Result(enum nad_enum_step mode)
{
	int fd = 0;
	int found = 0;
	int fd_proc = 0;
	int ret = NAD_CHECK_NONE;
	char buf[512] = {'\0',};
	char proc_buf[16] = {'\0',};

	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);

	fd_proc = sys_open("/proc/reset_reason", O_RDONLY, 0);
	
	if (fd_proc >= 0) {
		printk(KERN_DEBUG);
		sys_read(fd_proc, proc_buf, 16);

		NAD_PRINT("/proc/reset_reason value = %s\n", proc_buf);
		if(mode == STEP_SMD_NAD_START) {
			append(sec_nad_env.smd_item_result, proc_buf, 2);
			appendc(sec_nad_env.smd_item_result, '_');
		} else {
			appendc(sec_nad_env.smd_item_result, '-');
			appendc(sec_nad_env.smd_item_result, '_');
		}

		sys_close(fd_proc);
	}

	if(mode == STEP_SMD_NAD_START) {
		NAD_PRINT("checking SMD NAD result\n");

		fd = sys_open("/sdcard/log/mtknad/NAD_SMD/scenario_rev.txt", O_RDONLY, 0);
		if (fd >= 0) {
			if(sys_read(fd, buf, 512))
				if(buf[0] != '\n' && buf[0] != '\0')
					append(sec_nad_env.smd_item_result, buf, 1);

			sys_close(fd);
		} else
			appendc(sec_nad_env.smd_item_result, '-');

		appendc(sec_nad_env.smd_item_result, '_');

		fd = sys_open("/sdcard/log/mtknad/NAD_SMD/SOC_maxtemp.txt", O_RDONLY, 0);
		if (fd >= 0) {
			if(sys_read(fd, buf, 512))
				if(buf[0] != '\n' && buf[0] != '\0')
					append(sec_nad_env.smd_item_result, buf, 5);

				sys_close(fd);
		} else
			appendc(sec_nad_env.smd_item_result, '-');

		appendc(sec_nad_env.smd_item_result, '_');

		fd = sys_open("/sdcard/log/mtknad/NAD_SMD/PCB_maxtemp.txt", O_RDONLY, 0);
		if (fd >= 0) {
			if(sys_read(fd, buf, 512))
				if(buf[0] != '\n' && buf[0] != '\0')
					append(sec_nad_env.smd_item_result, buf, 5);

			sys_close(fd);
		} else
			appendc(sec_nad_env.smd_item_result, '-');

		appendc(sec_nad_env.smd_item_result, '_');

		fd = sys_open(SMD_NAD_RESULT, O_RDONLY, 0);
	}
	else if(mode == STEP_ETC) {
		NAD_PRINT("checking ACAT NAD result\n");
		fd = sys_open(ETC_NAD_RESULT, O_RDONLY, 0);
	}

	if (fd >= 0) {
		printk(KERN_DEBUG);

		while(sys_read(fd, buf, 512))
		{
			char *ptr;
			char *div = "\n";
			char *tok = NULL;

			ptr = buf;
			while((tok = strsep(&ptr, div)) != NULL )
			{
				if((strstr(tok, "PASS")))
				{
					if(mode == STEP_SMD_NAD_START) {
						appendc(sec_nad_env.smd_item_result, 'P');
					}
					continue;
				}

				if((strstr(tok, "N/A")))
				{
					if(mode == STEP_SMD_NAD_START) {
						appendc(sec_nad_env.smd_item_result, 'N');
					}
					continue;
				}

				if((strstr(tok, "FAIL")))
				{
					NAD_PRINT("There is a FAIL in test_result.csv %d\n", ret);
					ret = NAD_CHECK_FAIL;

					if(mode == STEP_SMD_NAD_START) {
						appendc(sec_nad_env.smd_item_result, 'F');
					}
					continue;
				}

				if((strstr(tok, "AllTestDone")))
				{
					NAD_PRINT("AllTestDone found in test_result.csv %d\n", ret);
					if(ret == NAD_CHECK_NONE)
						ret = NAD_CHECK_PASS;

					found = 1;
					if(mode == STEP_SMD_NAD_START) {
						appendc(sec_nad_env.smd_item_result, 'C');
					}
					break;
				}
			}
		}

		if(ret == NAD_CHECK_NONE)
			ret = NAD_CHECK_INCOMPLETED;

		sys_close(fd);
	}
	else
	{
		NAD_PRINT("The File is not existed\n");
		ret = NAD_CHECK_NONE;
	}

	set_fs(old_fs);
	
	NAD_PRINT("The result is %d\n", ret);
	return ret;
}

static ssize_t show_nad_remain_count(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	int ret = 0;
	char cnt[100] = {'0',};

	ret = sec_set_nad_param(NAD_PARAM_READ);
	if (ret < 0)
		pr_err("%s: read error! %d\n", __func__, ret);

	sprintf(cnt, "%d", sec_nad_env.nad_acat_remaining_count);

	return sprintf(buf, cnt);
}

static DEVICE_ATTR(nad_remain_count, S_IRUGO, show_nad_remain_count, NULL);

static ssize_t show_ddrtest_remain_count(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	int ret = 0;
	char cnt[100] = {'0',};

	ret = sec_set_nad_param(NAD_PARAM_READ);
	if (ret < 0)
		pr_err("%s: read error! %d\n", __func__, ret);

	sprintf(cnt, "%d", sec_nad_env.nad_dramtest_remaining_count);

	return sprintf(buf, cnt);
}

static DEVICE_ATTR(nad_ddrtest_remain_count, S_IRUGO, show_ddrtest_remain_count, NULL);

static ssize_t show_nad_stat(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	int dramscan_result = NAD_CHECK_NONE;
	int hlos_result = NAD_CHECK_NONE;
	int ret = 0;

	NAD_PRINT("show_nad_stat\n");
	ret = sec_set_nad_param(NAD_PARAM_READ);
	if (ret < 0)
		pr_err("%s: read error! %d\n", __func__, ret);

	if(sec_nad_env.curr_step == STEP_SMD_NAD_DONE) {
		dramscan_result = sec_nad_env.nad_smd_result & 0x3;
		hlos_result = (sec_nad_env.nad_smd_result >> 2) & 0x3;
	} else if (sec_nad_env.curr_step == STEP_SMD_NAD_START) {
		dramscan_result = sec_nad_env.nad_smd_result & 0x3;
		if(dramscan_result == NAD_CHECK_PASS) {
			appendc(sec_nad_env.smd_item_result, 'P');
		} else
			appendc(sec_nad_env.smd_item_result, 'F');

		appendc(sec_nad_env.smd_item_result, '_');

		hlos_result = Nad_Result(STEP_SMD_NAD_START);

		sec_nad_env.nad_smd_result = sec_nad_env.nad_smd_result + (hlos_result << 2);
		sec_nad_env.curr_step = STEP_SMD_NAD_DONE;
	}

	ret = sec_set_nad_param(NAD_PARAM_WRITE);
	if (ret < 0)
		pr_err("%s: read error! %d\n", __func__, ret);

	if(dramscan_result == NAD_CHECK_FAIL ||
		hlos_result == NAD_CHECK_FAIL) {

		return sprintf(buf, "NG_2.0_FAIL_%s\n", sec_nad_env.smd_item_result);
	}
	else if (hlos_result == NAD_CHECK_PASS)
		return sprintf(buf, "OK_2.0\n");
	else if(hlos_result == NAD_CHECK_NONE && sec_nad_env.curr_step < STEP_SMD_NAD_START)
		return sprintf(buf, "OK\n");
	else	// NAD test is not finished(in SMD F/T)
		return sprintf(buf, "RE_WORK_%s,\n", sec_nad_env.smd_item_result);
}
static DEVICE_ATTR(nad_stat, S_IRUGO, show_nad_stat, NULL);

static ssize_t show_nad_support(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
#if defined(CONFIG_MACH_MT6739)
	return sprintf(buf, "SUPPORT\n");
#else
	return sprintf(buf, "NOT_SUPPORT\n");
#endif
}
static DEVICE_ATTR(nad_support, S_IRUGO, show_nad_support, NULL);

static ssize_t store_nad_end(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
	kobject_uevent_env(&dev->kobj, KOBJ_CHANGE, nad_uevent.envp);
	NAD_PRINT("Send to Process App for Nad test end : %s\n", __func__);

	return count;
}
static DEVICE_ATTR(nad_end, S_IWUSR, NULL, store_nad_end);

static ssize_t store_nad_erase(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
	int ret = -1;
	if (!strncmp(buf, "erase", 5)) {
		char *argv[4] = { NULL, NULL, NULL, NULL };
		char *envp[3] = { NULL, NULL, NULL };
		char zerostr[SMD_ITEM_RESULT_LENGTH] = {'\0',};
		int ret_userapp;

		NAD_PRINT("store_nad_erase\n");
		sec_nad_env.curr_step = 0;
		sec_nad_env.nad_acat_remaining_count = 0;
		sec_nad_env.nad_dramtest_remaining_count = 0;
		sec_nad_env.nad_smd_result = 0;
		sec_nad_env.nad_dram_test_result = 0;
		sec_nad_env.nad_dram_fail_data = 0;
		sec_nad_env.nad_dram_fail_address = 0;
		strncpy(sec_nad_env.smd_item_result, zerostr, SMD_ITEM_RESULT_LENGTH);

		ret = sec_set_nad_param(NAD_PARAM_WRITE);
		if (ret < 0)
			pr_err("%s: write error! %d\n", __func__, ret);

		argv[0] = ERASE_NAD_PRG;
				
		envp[0] = "HOME=/";
		envp[1] = "PATH=/system/bin:/sbin:/bin:/usr/sbin:/usr/bin";
		ret_userapp = call_usermodehelper( argv[0], argv, envp, UMH_WAIT_EXEC);
	
		if(!ret_userapp)
		{
			NAD_PRINT("remove_mtk_nad_files.sh is executed. ret_userapp = %d\n", ret_userapp);
			erase_pass = 1;
		}
		else
		{
			NAD_PRINT("remove_mtk_nad_files.sh is NOT executed. ret_userapp = %d\n", ret_userapp);
			erase_pass = 0;
		}
		return count;
	} else
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

static ssize_t show_nad_acat(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	if(Nad_Result(STEP_ETC) == NAD_CHECK_NONE){
		NAD_PRINT("MTK_NAD No Run\n");
		return sprintf(buf, "OK\n");
	}else if(Nad_Result(STEP_ETC) == NAD_CHECK_FAIL){
		NAD_PRINT("MTK_NAD fail\n");
		return sprintf(buf, "NG_ACAT_ASV\n");
	}else{
		NAD_PRINT("MTK_NAD Passed\n");
		return sprintf(buf, "OK_ACAT_NONE\n");
	} 
}

static void do_nad(int mode)
{
	char *argv[4] = { NULL, NULL, NULL, NULL };
	char *envp[3] = { NULL, NULL, NULL };
	int ret_userapp;

	envp[0] = "HOME=/";
	envp[1] = "PATH=/system/bin:/sbin:/bin:/usr/sbin:/usr/bin";

	switch (mode) {
		case STEP_SMD_NAD_START:
			argv[0] = SMD_NAD_PROG;
			argv[1] = SMD_NAD_LOGPATH;
			NAD_PRINT("SMD_NAD, nad_test_mode : %d\n", mode);
			break;
		default:
			argv[0] = ETC_NAD_PROG;
			argv[1] = REBOOT;
			argv[2] = ETC_NAD_LOGPATH;
			NAD_PRINT("ETC_NAD, nad_test_mode : %d\n", mode);
			NAD_PRINT
			    ("Setting an argument to reboot after NAD completes.\n");
			break;
	}

	ret_userapp = call_usermodehelper(argv[0], argv, envp, UMH_WAIT_EXEC);

	if (!ret_userapp) {
		NAD_PRINT("%s is executed. ret_userapp = %d\n", argv[0],
			  ret_userapp);

		if (erase_pass)
			erase_pass = 0;
	} else {
		NAD_PRINT("%s is NOT executed. ret_userapp = %d\n", argv[0],
			  ret_userapp);
	}
}


static ssize_t store_nad_acat(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	int ret = -1;
	int idx = 0;
	unsigned int nad_count = 0;
	unsigned int dram_count = 0;
	char temp[NAD_BUFF_SIZE*3];
	char nad_cmd[NAD_CMD_LIST][NAD_BUFF_SIZE];
	char *nad_ptr, *string;
	NAD_PRINT("buf : %s count : %d\n", buf, (int)count);
	if((int)count < NAD_BUFF_SIZE)
		return -EINVAL;

	/* Copy buf to nad temp */
	strncpy(temp, buf, NAD_BUFF_SIZE*3);
	string = temp;

	ret = sec_set_nad_param(NAD_PARAM_READ);
	if (ret < 0)
		pr_err("%s: read error! %d\n", __func__, ret);

	while(idx < NAD_CMD_LIST) {
		nad_ptr = strsep(&string,",");
		strcpy(nad_cmd[idx++], nad_ptr);
	}

	if (!strncmp(buf, "nad_acat", 8)) {
		if(sec_nad_env.curr_step < STEP_SMD_NAD_START) ///////////// SMD NAD section //////////////
		{
			NAD_PRINT("1st boot at SMD\n");
			sec_nad_env.curr_step = STEP_SMD_NAD_START;
			sec_nad_env.nad_acat_remaining_count = 0x0;
			sec_nad_env.nad_dramtest_remaining_count = 0x0;

			ret = sec_set_nad_param(NAD_PARAM_WRITE);
			if (ret < 0)
				pr_err("%s: read error! %d\n", __func__, ret);

			do_nad(STEP_SMD_NAD_START);
			return count;
		}

		ret = sscanf(nad_cmd[1], "%d\n", &nad_count);
		if (ret != 1)
			return -EINVAL;

		ret = sscanf(nad_cmd[2], "%d\n", &dram_count);
		if (ret != 1)
			return -EINVAL;

		NAD_PRINT("ETC NAD, nad_acat%d,%d\n", nad_count, dram_count);

		if( nad_count || dram_count) {
			sec_nad_env.nad_acat_remaining_count = nad_count;
			sec_nad_env.nad_dramtest_remaining_count = dram_count;

			NAD_PRINT
				("new cmd : nad_acat_remaining_count = %d, nad_dramtest_remaining_count = %d\n",
				     sec_nad_env.nad_acat_remaining_count,
				     sec_nad_env.nad_dramtest_remaining_count);

			ret = sec_set_nad_param(NAD_PARAM_WRITE);
			if (ret < 0)
				pr_err("%s: write error! %d\n", __func__, ret);
		} else {
			if(sec_nad_env.curr_step >= STEP_SMD_NAD_START &&
				Nad_Result(STEP_ETC) == NAD_CHECK_FAIL) {
				pr_err("%s : nad test fail - set the remain counts to 0\n",
					     __func__);
				sec_nad_env.nad_acat_remaining_count = 0;
				sec_nad_env.nad_dramtest_remaining_count = 0;

				// flushing to param partition
				ret = sec_set_nad_param(NAD_PARAM_WRITE);
				if (ret < 0)
					pr_err("%s: read error! %d\n", __func__, ret);

				goto err_out;
			}
			if (sec_nad_env.nad_acat_remaining_count > 0) {
				NAD_PRINT("nad : nad_remain_count = %d, ddrtest_remain_count = %d\n",
						sec_nad_env.nad_acat_remaining_count,
						sec_nad_env.nad_dramtest_remaining_count);

				sec_nad_env.nad_acat_remaining_count--;
				ret = sec_set_nad_param(NAD_PARAM_WRITE);
				if (ret < 0)
					pr_err("%s: read error! %d\n", __func__, ret);

				do_nad(STEP_ETC);
			}
		}
	}
err_out:
	return count;

}

static DEVICE_ATTR(nad_acat, S_IRUGO | S_IWUSR,  show_nad_acat, store_nad_acat);

static ssize_t show_nad_dram(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	NAD_PRINT("%s\n", __func__);

	if (sec_nad_env.nad_dram_test_result == NAD_CHECK_FAIL)
		return sprintf(buf, "NG_DRAM_0x%0llx,0x%x\n",
		sec_nad_env.nad_dram_fail_address,
		sec_nad_env.nad_dram_fail_data);
	else if (sec_nad_env.nad_dram_test_result == NAD_CHECK_PASS)
		return sprintf(buf, "OK_DRAM\n");
	else
		return sprintf(buf, "NO_DRAMTEST\n");
}

static DEVICE_ATTR(nad_dram, S_IRUGO, show_nad_dram, NULL);

static int __init sec_nad_init(void)
{
	int ret = 0;

	struct device* sec_nad;

	NAD_PRINT("%s\n", __func__);

	/* Skip nad init when device goes to low power charging */
	if(lpcharge)
		return ret;

	sec_nad = sec_device_create(NULL, "sec_nad");

	if (IS_ERR(sec_nad)) {
		pr_err("%s Failed to create device(sec_nad)!\n", __func__);
		return PTR_ERR(sec_nad);
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_remain_count); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}
	
	ret = device_create_file(sec_nad, &dev_attr_nad_ddrtest_remain_count); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_stat); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad, &dev_attr_nad_support); 
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

	ret = device_create_file(sec_nad, &dev_attr_nad_end); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	if(add_uevent_var(&nad_uevent, "NAD_TEST=%s", "DONE")) {
		pr_err("%s : uevent NAD_TEST_AND_PASS is failed to add\n", __func__);
		goto err_create_nad_sysfs;
	}

	/* Initialize nad param struct */
	sec_nad_param_data.offset = NAD_ENV_DATA_OFFSET;
	sec_nad_param_data.state = NAD_PARAM_EMPTY;
	init_completion(&sec_nad_param_data.comp);

	INIT_WORK(&sec_nad_param_data.sec_nad_work, sec_nad_param_update);
	INIT_DELAYED_WORK(&sec_nad_param_data.sec_nad_delay_work, sec_nad_init_update);

	schedule_delayed_work(&sec_nad_param_data.sec_nad_delay_work, HZ * 10);

	sec_set_nad_param(NAD_PARAM_READ);
	
	return 0;
err_create_nad_sysfs:
	return ret;
}

static void __exit sec_nad_exit(void)
{
    cancel_work_sync(&sec_nad_param_data.sec_nad_work);
    cancel_delayed_work(&sec_nad_param_data.sec_nad_delay_work);
    NAD_PRINT("%s: exit\n", __func__);
}

module_init(sec_nad_init);
module_exit(sec_nad_exit);
