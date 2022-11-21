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
#define TOTAL_TEST_CNT 6

#define NAD_RESULT_PASS 0
#define NAD_RESULT_FAIL -1
#define NAD_RESULT_NORUN -2
#define NAD_RESULT_REWORK -3

struct kobj_uevent_env nad_uevent;

static int atoi(const char *str)
{
	int result = 0;
	int count = 0;
	if (str == NULL)
		return -EIO;
	while (str[count] != 0	/* NULL */
		&& str[count] >= '0' && str[count] <= '9')	{
		result = result * 10 + str[count] - '0';
		++count;
	}
	return result;
}

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
		return;
	}

	ret = fp->f_op->llseek(fp, param_data->offset, SEEK_SET);
	if (ret < 0) {
		pr_err("%s: llseek error %d!\n", __func__, ret);
		goto close_fp_out;
	}

	switch(param_data->state){
		case NAD_PARAM_WRITE:
			ret = vfs_write(fp, (char*)&sec_nad_env, sizeof(struct nad_env), &(fp->f_pos));

			NAD_PRINT("nad_acat : %s\n nad_acat_loop_count : %d\n nad_acat_exec_count : %d\n nad_dram_test_need : %s\n nad_dram_test_result : %d nad_dram_fail_data : 0x%x\n nad_dram_fail_address : 0x%08lx\n",
				sec_nad_env.nad_acat, sec_nad_env.nad_acat_loop_count, sec_nad_env.nad_acat_exec_count, sec_nad_env.nad_dram_test_need, sec_nad_env.nad_dram_test_result, sec_nad_env.nad_dram_fail_data, sec_nad_env.nad_dram_fail_address);

			if (ret < 0) pr_err("%s: write error! %d\n", __func__, ret);
			break;

		case NAD_PARAM_READ:
			ret = vfs_read(fp, (char*)&sec_nad_env, sizeof(struct nad_env), &(fp->f_pos));

			NAD_PRINT("nad_acat : %s\n nad_acat_loop_count : %d\n nad_acat_exec_count : %d\n nad_dram_test_need : %s\n nad_dram_test_result : %d nad_dram_fail_data : 0x%x\n nad_dram_fail_address : 0x%08lx\n",
				sec_nad_env.nad_acat, sec_nad_env.nad_acat_loop_count, sec_nad_env.nad_acat_exec_count, sec_nad_env.nad_dram_test_need, sec_nad_env.nad_dram_test_result, sec_nad_env.nad_dram_fail_data, sec_nad_env.nad_dram_fail_address);
			if (ret < 0) pr_err("%s: read error! %d\n", __func__, ret);
			break;
	}
	
close_fp_out:
	if (fp) filp_close(fp, NULL);
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

static int Nad_Result(void)
{
	int fd = 0;
	int fd_proc = 0;
	int ret = NAD_RESULT_PASS;
	char buf[200] = {'\0',};
	char proc_buf[200] = {'\0',};
	int pass_cnt = 0;
 
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
 
	fd_proc = sys_open("/proc/reset_reason", O_RDONLY, 0);
	
	if (fd_proc >= 0) {
		printk(KERN_DEBUG);
		sys_read(fd_proc, proc_buf, 200);

		NAD_PRINT("/proc/reset_reason value = %s\n", proc_buf);
		sys_close(fd_proc);
	}

	fd = sys_open("/nad_refer/MTK_NAD/test_result.csv", O_RDONLY, 0);
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
					ret = NAD_RESULT_FAIL;
					break;
				}

				if(!(strncasecmp(tok, "NONE",4)))
				{
					if( !(strncasecmp(proc_buf, "KPON",4)) || !(strncasecmp(proc_buf, "DPON",4)) )
						ret = NAD_RESULT_FAIL;
					else
						ret = NAD_RESULT_REWORK;
				}

				if(!(strncasecmp(tok, "PASS",4)))
					pass_cnt++;
			}
		}
		sys_close(fd);

		if ( pass_cnt < TOTAL_TEST_CNT ){
			if( ret == NAD_RESULT_PASS ){
				if( !(strncasecmp(proc_buf, "KPON",4)) || !(strncasecmp(proc_buf, "DPON",4)) )
					ret = NAD_RESULT_FAIL;
				else
					ret = NAD_RESULT_REWORK;
			}
		}
	}
	else
	{
		NAD_PRINT("The File is not existed\n");
		ret = NAD_RESULT_NORUN;
	}

	set_fs(old_fs);
	
	return ret;
}

static int read_nad_remain_count(void)
{
	int fd = 0;
	unsigned int ret = 0;
	char cnt[100] = {'0',};
 
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
 
	fd = sys_open("/nad_refer/MTK_NAD/nad_remain_count.csv", O_RDONLY, 0);

	if (fd >= 0) {
		printk(KERN_DEBUG);

		sys_read(fd, cnt, 100);
		sys_close(fd);
	}

	set_fs(old_fs);
	ret = atoi(cnt);

	return ret;
}

static ssize_t show_nad_remain_count(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	char cnt[100] = {'0',};
	sprintf(cnt, "%d", read_nad_remain_count());

	return sprintf(buf, cnt);
}

static DEVICE_ATTR(nad_remain_count, S_IRUGO, show_nad_remain_count, NULL);

static int read_ddrtest_remain_count(void)
{
	int fd = 0;
	unsigned int ret = 0;
	char cnt[100] = {'0',};
 
	mm_segment_t old_fs = get_fs();
	set_fs(KERNEL_DS);
 
	fd = sys_open("/nad_refer/MTK_NAD/nad_ddrtest_remain_count.csv", O_RDONLY, 0);

	if (fd >= 0) {
		printk(KERN_DEBUG);

		sys_read(fd, cnt, 100);
		sys_close(fd);
	}

	set_fs(old_fs);
	ret = atoi(cnt);

	return ret;
}

static ssize_t show_ddrtest_remain_count(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	char cnt[100] = {'0',};
	sprintf(cnt, "%d", read_ddrtest_remain_count());

	return sprintf(buf, cnt);
}

static DEVICE_ATTR(nad_ddrtest_remain_count, S_IRUGO, show_ddrtest_remain_count, NULL);

static ssize_t show_nad_stat(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	if (Nad_Result() == NAD_RESULT_PASS)
		return sprintf(buf, "OK_2.0\n");
	else if(Nad_Result() == NAD_RESULT_NORUN)
		return sprintf(buf, "OK\n");
	else if(Nad_Result() == NAD_RESULT_FAIL)
		return sprintf(buf, "NG_2.0_FAIL\n");
	else	// NAD test is not finished(in SMD F/T)
		return sprintf(buf, "RE_WORK\n");
}
static DEVICE_ATTR(nad_stat, S_IRUGO, show_nad_stat, NULL);

static ssize_t show_nad_support(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	#if defined(CONFIG_MACH_MT6757)
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
static DEVICE_ATTR(nad_end, S_IRUGO | S_IWUSR, NULL, store_nad_end);

static ssize_t store_nad_erase(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
	int ret = -1;
	if (!strncmp(buf, "erase", 5)) {
		char *argv[4] = { NULL, NULL, NULL, NULL };
		char *envp[3] = { NULL, NULL, NULL };
		int ret_userapp;

		strncpy(sec_nad_env.nad_dram_test_need, "",4);
		ret = sec_set_nad_param(NAD_PARAM_WRITE);
		if (ret < 0)
			pr_err("%s: write error! %d\n", __func__, ret);

		argv[0] = "/system/bin/remove_mtk_nad_files.sh";
				
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
	if(Nad_Result() == NAD_RESULT_NORUN){
		NAD_PRINT("MTK_NAD No Run\n");
		return sprintf(buf, "OK\n");
	}else if(Nad_Result() == NAD_RESULT_FAIL){
		NAD_PRINT("MTK_NAD fail\n");
		return sprintf(buf, "NG_ACAT_ASV\n");
	}else{
		NAD_PRINT("MTK_NAD Passed\n");
		return sprintf(buf, "OK_ACAT_NONE\n");
	} 
}

static ssize_t store_nad_acat(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	int ret = -1;
	int idx = 0;
	int ret_userapp;
	unsigned int nad_check = 0;
	unsigned int dram_check = 0;
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

	if (!strncmp(buf, "nad_acat", 8)) {
		char *argv[4] = { NULL, NULL, NULL, NULL };
		char *envp[3] = { NULL, NULL, NULL };
		char ary_argv[4][100] = {""};

		/* Buffer parse needed. */
		strcpy(sec_nad_env.nad_acat, "ACAT");

		ret = sscanf(nad_cmd[1], "%d\n", &nad_check);
		if (ret != 1)
			return -EINVAL;

		ret = sscanf(nad_cmd[2], "%d\n", &dram_check);
		if (ret != 1)
			return -EINVAL;

		if( dram_check ){
			strncpy(sec_nad_env.nad_dram_test_need, "DRAM",4);
			sec_nad_env.nad_dram_test_result = 0;
			sec_nad_env.nad_dram_fail_data = 0;
			sec_nad_env.nad_dram_fail_address = 0;
		}

		if( nad_check ) {
			sec_nad_env.nad_acat_loop_count = nad_check;
			sec_nad_env.nad_dram_test_check = dram_check;

			argv[0] = "/system/bin/mtk_set_node_value.sh";
			sprintf(ary_argv[1], "%d", sec_nad_env.nad_acat_loop_count);	// NAD Test Count
			argv[1] = ary_argv[1];

			sprintf(ary_argv[2], "%d", 0);	// DRAM Test count
			argv[2] = ary_argv[2];

			envp[0] = "HOME=/";
			envp[1] = "PATH=/system/bin:/sbin:/bin:/usr/sbin:/usr/bin";
			ret_userapp = call_usermodehelper( argv[0], argv, envp, UMH_WAIT_EXEC);

			if(!ret_userapp) {
				NAD_PRINT("mtk_set_node_value.sh is executed. ret_userapp = %d\n", ret_userapp);

				if(erase_pass)
					erase_pass = 0;
			} else
				NAD_PRINT("mtk_set_node_value.sh is not executed. ret_userapp = %d\n", ret_userapp);

		} else {
			if( Nad_Result() == NAD_RESULT_NORUN || read_nad_remain_count() > 0 ){

				argv[0] = "/system/bin/mtk_nad_exec.sh";

				// argv[2] = 1 : reboot, argv[2] = 0 : not reboot
				if( Nad_Result() == NAD_RESULT_NORUN ) {
					sec_nad_env.nad_acat_loop_count = 0;
					sprintf(ary_argv[1], "%d", 0);	// not reboot
				} else{
					sprintf(ary_argv[1], "%d", 1);	// reboot
				}

				argv[1] = ary_argv[1];

				envp[0] = "HOME=/";
				envp[1] = "PATH=/system/bin:/sbin:/bin:/usr/sbin:/usr/bin";
				ret_userapp = call_usermodehelper( argv[0], argv, envp, UMH_WAIT_EXEC);

				if(!ret_userapp)
				{
					NAD_PRINT("mtk_nad_exec.sh is executed. ret_userapp = %d\n", ret_userapp);

					if(erase_pass)
						erase_pass = 0;
				}
				else
				{
					NAD_PRINT("mtk_nad_exec.sh is not executed. ret_userapp = %d\n", ret_userapp);
				}
			}	// if( Nad_Result() == NAD_RESULT_NORUN || read_nad_remain_count() > 0 ){
		}	// else - if( nad_check )

		ret = sec_set_nad_param(NAD_PARAM_WRITE);
		if (ret < 0)
			pr_err("%s: write error! %d\n", __func__, ret);
			
		return count;
	} else
		return count;
}

static DEVICE_ATTR(nad_acat, S_IRUGO | S_IWUSR,  show_nad_acat, store_nad_acat);

static ssize_t show_nad_dram(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	NAD_PRINT("%s\n", __func__);

	if (sec_nad_env.nad_dram_test_result == NAD_DRAM_TEST_FAIL)
		return sprintf(buf, "NG_DRAM_0x%08lx,0x%x\n", 
		sec_nad_env.nad_dram_fail_address,
		sec_nad_env.nad_dram_fail_data);
	else if (sec_nad_env.nad_dram_test_result == NAD_DRAM_TEST_PASS)
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
	sec_nad_param_data.offset = NAD_DRAM_OFFSET;
	sec_nad_param_data.state = NAD_PARAM_EMPTY;

	INIT_WORK(&sec_nad_param_data.sec_nad_work, sec_nad_param_update);
	INIT_DELAYED_WORK(&sec_nad_param_data.sec_nad_delay_work, sec_nad_init_update);

	schedule_delayed_work(&sec_nad_param_data.sec_nad_delay_work, HZ * 10);
	
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
