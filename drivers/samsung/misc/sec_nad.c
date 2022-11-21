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
//#define NAD_DEBUG

struct kobj_uevent_env nad_uevent;
int curr_event = 0;

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
			NAD_PRINT("nad_acat_result : %d\n", sec_nad_env.nad_acat_result);
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
			NAD_PRINT("nad_acat_result : %d\n", sec_nad_env.nad_acat_result);
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

	dramscan_result = sec_nad_env.nad_smd_result & 0x3;
	hlos_result = (sec_nad_env.nad_smd_result >> 2) & 0x3;

	if(dramscan_result == NAD_CHECK_FAIL ||
		hlos_result == NAD_CHECK_FAIL) {

		return sprintf(buf, "NG_2.0_FAIL_%s\n", sec_nad_env.smd_item_result);
	}
	else if (hlos_result == NAD_CHECK_PASS)
		return sprintf(buf, "OK_2.0\n");
	else if(hlos_result == NAD_CHECK_NONE && sec_nad_env.curr_step < STEP_SMD_NAD_START)
		return sprintf(buf, "OK\n");
	else	// NAD test is not finished(in SMD F/T)
		return sprintf(buf, "RE_WORK_%s\n", sec_nad_env.smd_item_result);
}
static DEVICE_ATTR(nad_stat, S_IRUGO, show_nad_stat, NULL);

static ssize_t show_nad_support(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
#if defined(CONFIG_MACH_MT6757) || defined(CONFIG_MACH_MT6768) \
	|| defined(CONFIG_MACH_MT6853)
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
	if (!strncmp(buf, "erase", 5)) {
		int ret = -1;

		NAD_PRINT("store_nad_erase\n");
		sec_nad_env.curr_step = 0;
		sec_nad_env.nad_acat_remaining_count = 0;
		sec_nad_env.nad_dramtest_remaining_count = 0;
		sec_nad_env.nad_smd_result = 0;
		sec_nad_env.nad_dram_test_result = 0;
		sec_nad_env.nad_dram_fail_data = 0;
		sec_nad_env.nad_dram_fail_address = 0;

		ret = sec_set_nad_param(NAD_PARAM_WRITE);
		if (ret < 0)
			pr_err("%s: write error! %d\n", __func__, ret);

		erase_pass = 1;
	}
	

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
	int ret = 0;
	
	NAD_PRINT("%s\n", __func__);
	ret = sec_set_nad_param(NAD_PARAM_READ);
	if (ret < 0)
		pr_err("%s: read error! %d\n", __func__, ret);

	if(sec_nad_env.nad_acat_result == NAD_CHECK_NONE){
		NAD_PRINT("MTK_NAD No Run\n");
		return sprintf(buf, "OK\n");
	}else if(sec_nad_env.nad_acat_result == NAD_CHECK_FAIL){
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
			curr_event = 1;

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
			sec_nad_env.nad_acat_result = NAD_CHECK_NONE;
			
			NAD_PRINT
				("new cmd : nad_acat_remaining_count = %d, nad_dramtest_remaining_count = %d\n",
				     sec_nad_env.nad_acat_remaining_count,
				     sec_nad_env.nad_dramtest_remaining_count);

			ret = sec_set_nad_param(NAD_PARAM_WRITE);
			if (ret < 0)
				pr_err("%s: write error! %d\n", __func__, ret);
		} else {
			curr_event =2;
		}
	}
//err_out:
	return count;

}

static DEVICE_ATTR(nad_acat, S_IRUGO | S_IWUSR,  show_nad_acat, store_nad_acat);

static ssize_t show_nad_dram(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
	int ret = 0;
	
	NAD_PRINT("%s\n", __func__);
	ret = sec_set_nad_param(NAD_PARAM_READ);
	if (ret < 0)
		pr_err("%s: read error! %d\n", __func__, ret);
	
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


static ssize_t store_nad_event(struct device *dev,
        struct device_attribute *attr,
        const char *buf, size_t count)
{
	NAD_PRINT("%s\n", __func__);

	sscanf(buf, "%d", &curr_event);

	return count;
}

static ssize_t show_nad_event(struct device *dev,
        struct device_attribute *attr,
        char *buf)
{
//	NAD_PRINT("%s\n", __func__);

	return sprintf(buf, "%d", curr_event);
}
static DEVICE_ATTR(nad_event, S_IRUGO | S_IWUSR, show_nad_event, store_nad_event);


//////////////////////////////////////////////////
/////// BALANCER /////////////////////////////////
//////////////////////////////////////////////////
static ssize_t store_nad_main(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	NAD_PRINT("%s\n", __func__);
	curr_event = 3;
	return count;
}

static ssize_t show_nad_main(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int count;
	int ret = 0;
	
	NAD_PRINT("%s\n", __func__);
	ret = sec_set_nad_param(NAD_PARAM_READ);
	if (ret < 0)
		pr_err("%s: read error! %d\n", __func__, ret);

	switch (sec_nad_env.nad_main_result) {
		case NAD_CHECK_PASS: {
			NAD_PRINT("MAIN NAD PASS\n");
			count = sprintf(buf, "OK_2.0\n");
		} break;

		case NAD_CHECK_FAIL: {
			NAD_PRINT("MAIN NAD FAIL\n");
			count = sprintf(buf, "NG_2.0_FAIL\n");
		} break;

		case NAD_CHECK_INCOMPLETED: {
			NAD_PRINT("MAIN NAD INCOMPLETED\n");
			count = sprintf(buf, "OK\n");
		} break;

		case NAD_CHECK_NONE: {
			NAD_PRINT("MAIN NAD NOT_TESTED\n");
			count = sprintf(buf, "OK\n");
		} break;
	}

	return count;
}
static DEVICE_ATTR(balancer, S_IRUGO | S_IWUSR, show_nad_main, store_nad_main);

static ssize_t show_main_nad_timeout(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", STEP_MAIN_TIMEOUT);
}
static DEVICE_ATTR(timeout, 0444, show_main_nad_timeout, NULL);

static ssize_t store_main_nad_run(struct device *dev,
			      struct device_attribute *attr,
			      const char *buf, size_t count)
{
	return count;
}

static ssize_t show_main_nad_run(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "END\n");
}
static DEVICE_ATTR(run, S_IRUGO | S_IWUSR, show_main_nad_run, store_main_nad_run);

static int __init sec_nad_init(void)
{
	int ret = 0;

	struct device* sec_nad;
	struct device* sec_nad_balancer;

	NAD_PRINT("%s\n", __func__);

	/* Skip nad init when device goes to low power charging */
	if(lpcharge)
		return ret;

	sec_nad = sec_device_create(NULL, "sec_nad");

	if (IS_ERR(sec_nad)) {
		pr_err("%s Failed to create device(sec_nad)!\n", __func__);
		return PTR_ERR(sec_nad);
	}

	sec_nad_balancer = sec_device_create(NULL, "sec_nad_balancer");
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
	
	ret = device_create_file(sec_nad, &dev_attr_nad_event); 
	if(ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}
	
	ret = device_create_file(sec_nad_balancer, &dev_attr_balancer);
	if (ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad_balancer, &dev_attr_timeout);
	if (ret) {
		pr_err("%s: Failed to create device file\n", __func__);
		goto err_create_nad_sysfs;
	}

	ret = device_create_file(sec_nad_balancer, &dev_attr_run);
	if (ret) {
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
