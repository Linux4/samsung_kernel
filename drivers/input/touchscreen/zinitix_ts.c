/*
 *
 * Zinitix bt532 touchscreen driver
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/input.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>

#if defined(CONFIG_PM_RUNTIME)
#include <linux/pm_runtime.h>
#endif
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>

#include "zinitix_ts.h"
#include "zinitix_touch_zxt_firmware.h"

#if 0//def CONFIG_MACH_MEGA2ELTE_USA_ATT
#include <linux/leds.h>
struct led_classdev touch_leds;
#endif

/* define i2c sub functions*/
static inline s32 read_data(struct i2c_client *client,
	u16 reg, u8 *values, u16 length)
{
	s32 ret;
	int count = 0;
retry:
	/* select register*/
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		mdelay(1);

		if (++count < 8)
			goto retry;

		return ret;
	}
	/* for setup tx transaction. */
	udelay(DELAY_FOR_TRANSCATION);
	ret = i2c_master_recv(client , values , length);
	if (ret < 0)
		return ret;

	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

static inline s32 write_data(struct i2c_client *client,
	u16 reg, u8 *values, u16 length)
{
	s32 ret;
	int count = 0;
	u8 pkt[10]; /* max packet */
	pkt[0] = (reg) & 0xff; /* reg addr */
	pkt[1] = (reg >> 8)&0xff;
	memcpy((u8 *)&pkt[2], values, length);

retry:
	ret = i2c_master_send(client , pkt , length + 2);
	if (ret < 0) {
		mdelay(1);

		if (++count < 8)
			goto retry;

		return ret;
	}

	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

static inline s32 write_reg(struct i2c_client *client, u16 reg, u16 value)
{
	if (write_data(client, reg, (u8 *)&value, 2) < 0)
		return I2C_FAIL;

	return I2C_SUCCESS;
}

static inline s32 write_cmd(struct i2c_client *client, u16 reg)
{
	s32 ret;
	int count = 0;

retry:
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		mdelay(1);

		if (++count < 8)
			goto retry;

		return ret;
	}

	udelay(DELAY_FOR_POST_TRANSCATION);
	return I2C_SUCCESS;
}

static inline s32 read_raw_data(struct i2c_client *client,
		u16 reg, u8 *values, u16 length)
{
	s32 ret;
	int count = 0;

retry:
	/* select register */
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0) {
		mdelay(1);

		if (++count < 8)
			goto retry;

		return ret;
	}

	/* for setup tx transaction. */
	udelay(200);

	ret = i2c_master_recv(client , values , length);
	if (ret < 0)
		return ret;
	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

static inline s32 read_firmware_data(struct i2c_client *client,
	u16 addr, u8 *values, u16 length)
{
	s32 ret;
	/* select register*/

	ret = i2c_master_send(client , (u8 *)&addr , 2);
	if (ret < 0)
		return ret;

	/* for setup tx transaction. */
	mdelay(1);

	ret = i2c_master_recv(client , values , length);
	if (ret < 0)
		return ret;

	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bt532_ts_early_suspend(struct early_suspend *h);
static void bt532_ts_late_resume(struct early_suspend *h);
#endif

#define USE_OPEN_CLOSE

#ifdef USE_OPEN_CLOSE
static int  bt532_ts_open(struct input_dev *dev);
static void bt532_ts_close(struct input_dev *dev);
#endif

static bool bt532_power_control(struct bt532_ts_info *info, u8 ctl);

static bool init_touch(struct bt532_ts_info *info, bool force);
static bool mini_init_touch(struct bt532_ts_info *info);
static void clear_report_data(struct bt532_ts_info *info);
static void esd_timer_start(u16 sec, struct bt532_ts_info *info);
static void esd_timer_stop(struct bt532_ts_info *info);
static void esd_timeout_handler(unsigned long data);

static long ts_misc_fops_ioctl(struct file *filp, unsigned int cmd,
								unsigned long arg);
static int ts_misc_fops_open(struct inode *inode, struct file *filp);
static int ts_misc_fops_close(struct inode *inode, struct file *filp);

static const struct file_operations ts_misc_fops = {
	.owner = THIS_MODULE,
	.open = ts_misc_fops_open,
	.release = ts_misc_fops_close,
	.unlocked_ioctl = ts_misc_fops_ioctl,
};

static struct miscdevice touch_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "zinitix_touch_misc",
	.fops = &ts_misc_fops,
};

#define TOUCH_IOCTL_BASE			0xbc
#define TOUCH_IOCTL_GET_DEBUGMSG_STATE		_IOW(TOUCH_IOCTL_BASE, 0, int)
#define TOUCH_IOCTL_SET_DEBUGMSG_STATE		_IOW(TOUCH_IOCTL_BASE, 1, int)
#define TOUCH_IOCTL_GET_CHIP_REVISION		_IOW(TOUCH_IOCTL_BASE, 2, int)
#define TOUCH_IOCTL_GET_FW_VERSION		_IOW(TOUCH_IOCTL_BASE, 3, int)
#define TOUCH_IOCTL_GET_REG_DATA_VERSION	_IOW(TOUCH_IOCTL_BASE, 4, int)
#define TOUCH_IOCTL_VARIFY_UPGRADE_SIZE		_IOW(TOUCH_IOCTL_BASE, 5, int)
#define TOUCH_IOCTL_VARIFY_UPGRADE_DATA		_IOW(TOUCH_IOCTL_BASE, 6, int)
#define TOUCH_IOCTL_START_UPGRADE		_IOW(TOUCH_IOCTL_BASE, 7, int)
#define TOUCH_IOCTL_GET_X_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 8, int)
#define TOUCH_IOCTL_GET_Y_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 9, int)
#define TOUCH_IOCTL_GET_TOTAL_NODE_NUM		_IOW(TOUCH_IOCTL_BASE, 10, int)
#define TOUCH_IOCTL_SET_RAW_DATA_MODE		_IOW(TOUCH_IOCTL_BASE, 11, int)
#define TOUCH_IOCTL_GET_RAW_DATA		_IOW(TOUCH_IOCTL_BASE, 12, int)
#define TOUCH_IOCTL_GET_X_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 13, int)
#define TOUCH_IOCTL_GET_Y_RESOLUTION		_IOW(TOUCH_IOCTL_BASE, 14, int)
#define TOUCH_IOCTL_HW_CALIBRAION		_IOW(TOUCH_IOCTL_BASE, 15, int)
#define TOUCH_IOCTL_GET_REG			_IOW(TOUCH_IOCTL_BASE, 16, int)
#define TOUCH_IOCTL_SET_REG			_IOW(TOUCH_IOCTL_BASE, 17, int)
#define TOUCH_IOCTL_SEND_SAVE_STATUS		_IOW(TOUCH_IOCTL_BASE, 18, int)
#define TOUCH_IOCTL_DONOT_TOUCH_EVENT		_IOW(TOUCH_IOCTL_BASE, 19, int)

struct bt532_ts_info *misc_info;

#define I2C_BUFFER_SIZE 64
static bool get_raw_data(struct bt532_ts_info *info, u8 *buff, int skip_cnt)
{
	struct i2c_client *client = info->client;
	struct bt532_ts_platform_data *pdata = info->pdata;
	u32 total_node = info->cap_info.total_node_num;
	int sz;
	int i;
	u32 temp_sz;

	disable_irq(info->irq);

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		dev_err(&client->dev, "%s: Other process occupied (%d)\n",
				__func__, info->work_state);
	}

	info->work_state = RAW_DATA;

	for(i = 0; i < skip_cnt; i++) {
		while (gpio_get_value(pdata->gpio_int))
			usleep_range(1000, 1000);

		write_cmd(client, BT532_CLEAR_INT_STATUS_CMD);
		usleep_range(1000, 1000);
	}

	sz = total_node * 2;

	while (gpio_get_value(pdata->gpio_int))
		usleep_range(1000, 1000);

	for(i = 0; sz > 0; i++){
		temp_sz = I2C_BUFFER_SIZE;

		if(sz < I2C_BUFFER_SIZE)
			temp_sz = sz;

		if (read_raw_data(client, BT532_RAWDATA_REG + i, (char *)(buff + (i*I2C_BUFFER_SIZE)), temp_sz) < 0) {
			dev_err(&client->dev, "%s: Failed to read raw data\n", __func__);
			info->work_state = NOTHING;
			goto out;
		}
		sz -= I2C_BUFFER_SIZE;
	}

	write_cmd(client, BT532_CLEAR_INT_STATUS_CMD);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return true;

out:
	enable_irq(info->irq);
	up(&info->work_lock);

	return false;
}

static bool ts_get_raw_data(struct bt532_ts_info *info)
{
	struct i2c_client *client = info->client;
	u32 total_node = info->cap_info.total_node_num;
	u32 sz;
	u16 temp_sz;
	int	i;

	if (down_trylock(&info->raw_data_lock)) {
		dev_err(&client->dev, "%s: Failed to occupy work lock\n", __func__);
		info->touch_info.status = 0;

		return true;
	}

	sz = total_node * 2 + sizeof(struct point_info);

	for(i = 0; sz > 0; i++){
		temp_sz = I2C_BUFFER_SIZE;

		if(sz < I2C_BUFFER_SIZE)
			temp_sz = sz;

		if (read_raw_data(client, BT532_RAWDATA_REG + i, (char *)((u8*)(info->cur_data)+ (i*I2C_BUFFER_SIZE)), temp_sz) < 0) {
			dev_err(&client->dev, "%s: Failed to read raw data\n", __func__);
			up(&info->raw_data_lock);

			return false;
		}
		sz -= I2C_BUFFER_SIZE;
	}
	info->update = 1;
	memcpy((u8 *)(&info->touch_info), (u8 *)&info->cur_data[total_node],
			sizeof(struct point_info));
	up(&info->raw_data_lock);

	return true;
}

#ifdef DEBUG_I2C_CHECKSUM
static bool i2c_checksum(struct bt532_ts_info *touch_dev, s16 *pChecksum, u16 wlength)
{
	s16 checksum_result;
	s16 checksum_cur;
	int i;

	checksum_cur = 0;
	for(i=0; i<wlength; i++) {
		checksum_cur += (s16)pChecksum[i];
	}
	if (read_data(touch_dev->client,
		ZINITIX_I2C_CHECKSUM_RESULT,
		(u8 *)(&checksum_result), 2) < 0) {
		printk(KERN_INFO "error read i2c checksum rsult.-\r\n");
		return false;
	}
	if(checksum_cur != checksum_result) {
		printk(KERN_INFO "checksum error : %d,%d\n", checksum_cur, checksum_result);
		return false;
	}
	return true;
}
#endif

static bool ts_read_coord(struct bt532_ts_info *info)
{
	struct i2c_client *client = info->client;
#if (TOUCH_POINT_MODE == 1)
	int i;
#endif

	/* for  Debugging Tool */

	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (ts_get_raw_data(info) == false)
			return false;

		dev_err(&client->dev, "status = 0x%04X\n", info->touch_info.status);

		goto out;
	}

#if (TOUCH_POINT_MODE == 1)
	memset(&info->touch_info,
			0x0, sizeof(struct point_info));

	if (read_data(info->client, BT532_POINT_STATUS_REG,
			(u8 *)(&info->touch_info), 4) < 0) {
		dev_err(&client->dev, "Failed to read point info\n");

		return false;
	}

	if (info->touch_info.event_flag == 0)
		goto out;

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		if (zinitix_bit_test(info->touch_info.event_flag, i)) {
			udelay(20);

			if (read_data(info->client, BT532_POINT_STATUS_REG + 2 + ( i * 4),
					(u8 *)(&info->touch_info.coord[i]),
					sizeof(struct coord)) < 0) {
				dev_err(&client->dev, "Failed to read point info\n");

				return false;
			}
		}
	}
#else
#ifdef DEBUG_I2C_CHECKSUM
	if(info->cap_info.i2s_checksum)
		if (write_reg(info->client,
				ZINITIX_I2C_CHECKSUM_WCNT, (sizeof(struct point_info)/2)) != I2C_SUCCESS)
				return false;
#endif
	if (read_data(info->client, BT532_POINT_STATUS_REG,
			(u8 *)(&info->touch_info), sizeof(struct point_info)) < 0) {
		dev_err(&client->dev, "Failed to read point info\n");

		return false;
	}
#ifdef DEBUG_I2C_CHECKSUM
	if(info->cap_info.i2s_checksum)
		if(i2c_checksum (info, (s16 *)(&info->touch_info), sizeof(struct point_info)/2) == false)
			return false;
#endif
#endif

out:
	/* error */
#ifdef DEBUG_I2C_CHECKSUM
	if (info->touch_info.status == 0x0) {
		//zinitix_debug_msg("periodical esd repeated int occured\r\n");
		if(write_cmd(info->client, BT532_CLEAR_INT_STATUS_CMD) != I2C_SUCCESS)
			return false;
		udelay(DELAY_FOR_SIGNAL_DELAY);	
		return true;
	}
#endif
	// error
	if (zinitix_bit_test(info->touch_info.status, BIT_MUST_ZERO)) {
		dev_err(&client->dev, "Invalid must zero bit(%04x)\n",
			info->touch_info.status);
#ifdef DEBUG_I2C_CHECKSUM
		udelay(DELAY_FOR_SIGNAL_DELAY);
		return false;
#endif
	}
#ifdef DEBUG_I2C_CHECKSUM
	if (zinitix_bit_test(info->touch_info.status, BIT_ICON_EVENT)) {
		if (read_data(info->client, BT532_ICON_STATUS_REG,
			(u8 *)(&info->icon_event_reg), 2) < 0) {
			dev_err(&client->dev, "Failed to read button info\n");
		return false;
	}
	}
#endif

	if(write_cmd(info->client, BT532_CLEAR_INT_STATUS_CMD) != I2C_SUCCESS)
		return false;

	return true;
}

#if ESD_TIMER_INTERVAL
static void esd_timeout_handler(unsigned long data)
{
	struct bt532_ts_info *info = (struct bt532_ts_info *)data;

	info->p_esd_timeout_tmr = NULL;
	queue_work(info->esd_tmr_workqueue, &info->tmr_work);
}

static void esd_timer_start(u16 sec, struct bt532_ts_info *info)
{
	mutex_lock(&info->prob_int_sync);
	if (info->p_esd_timeout_tmr != NULL)
		del_timer(info->p_esd_timeout_tmr);

	info->p_esd_timeout_tmr = NULL;
	init_timer(&(info->esd_timeout_tmr));
	info->esd_timeout_tmr.data = (unsigned long)(info);
	info->esd_timeout_tmr.function = esd_timeout_handler;
	info->esd_timeout_tmr.expires = jiffies + (HZ * sec);
	info->p_esd_timeout_tmr = &info->esd_timeout_tmr;
	add_timer(&info->esd_timeout_tmr);
	mutex_unlock(&info->prob_int_sync);
}

static void esd_timer_stop(struct bt532_ts_info *info)
{
	if (info->p_esd_timeout_tmr)
		del_timer(info->p_esd_timeout_tmr);

	info->p_esd_timeout_tmr = NULL;
}

static void ts_tmr_work(struct work_struct *work)
{
	struct bt532_ts_info *info =
				container_of(work, struct bt532_ts_info, tmr_work);
	struct i2c_client *client = info->client;

	printk(KERN_INFO "%s\n",__func__);

	if(down_trylock(&info->work_lock)) {
		dev_err(&client->dev, "%s: Failed to occupy work lock\n", __func__);
		esd_timer_start(CHECK_ESD_TIMER, info);

		return;
	}

	if (info->work_state != NOTHING) {
		dev_err(&client->dev, "%s: Other process occupied (%d)\n",
			__func__, info->work_state);
		up(&info->work_lock);

		return;
	}
	info->work_state = ESD_TIMER;

	disable_irq(info->irq);
	bt532_power_control(info, POWER_OFF);
	bt532_power_control(info, POWER_ON_SEQUENCE);

	clear_report_data(info);
	if (mini_init_touch(info) == false)
		goto fail_time_out_init;

	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);
#if defined(TSP_VERBOSE_DEBUG)
	dev_info(&client->dev, "tmr queue work\n");
#endif

	return;

fail_time_out_init:
	dev_err(&client->dev, "Failed to restart\n");
	esd_timer_start(CHECK_ESD_TIMER, info);
	info->work_state = NOTHING;
	enable_irq(info->irq);
	up(&info->work_lock);

	return;
}
#endif

static bool bt532_power_sequence(struct bt532_ts_info *info)
{
	struct i2c_client *client = info->client;
	int retry = 0;

retry_power_sequence:
	if (write_reg(client, 0xc000, 0x0001) != I2C_SUCCESS)
		goto fail_power_sequence;

	udelay(10);

	if (write_cmd(client, 0xc004) != I2C_SUCCESS)
		goto fail_power_sequence;

	udelay(10);

	if (write_reg(client, 0xc002, 0x0001) != I2C_SUCCESS)
		goto fail_power_sequence;

	mdelay(2);

	if (write_reg(client, 0xc001, 0x0001) != I2C_SUCCESS)
		goto fail_power_sequence;

	msleep(FIRMWARE_ON_DELAY);	/* wait for checksum cal */

	return true;

fail_power_sequence:
	if (retry++ < 3) {
		msleep(CHIP_ON_DELAY);
		dev_err(&client->dev, "retry = %d\n", retry);

		goto retry_power_sequence;
	}

	dev_err(&client->dev, "Failed to send power sequence\n");

	return false;
}

static bool bt532_power_control(struct bt532_ts_info *info, u8 ctl)
{
	printk(KERN_INFO "%s\n",__func__);

	if (ctl == POWER_OFF) {
		info->pdata->power(false);
		msleep(CHIP_OFF_DELAY);
	} else if (ctl == POWER_ON_SEQUENCE) {
		info->pdata->power(true);
		msleep(CHIP_ON_DELAY);

		return bt532_power_sequence(info);
	} else if (ctl == POWER_ON)
		info->pdata->power(true);

	return true;
}

#if TOUCH_ONESHOT_UPGRADE
static bool ts_check_need_upgrade(struct bt532_ts_info *info,
		u16 version, u16 minor_version, u16 reg_version, u16 hw_id)
{
	struct i2c_client *client = info->client;
	u16	new_version;
	u16	new_minor_version;
	u16	new_reg_version;
	u16	new_hw_id;

	new_version = (u16) (m_firmware_data[52] | (m_firmware_data[53]<<8));
	new_minor_version = (u16) (m_firmware_data[56] | (m_firmware_data[57]<<8));
	new_reg_version = (u16) (m_firmware_data[60] | (m_firmware_data[61]<<8));

	new_hw_id = (u16) (m_firmware_data[0x6b12] | (m_firmware_data[0x6b13]<<8));

	printk(KERN_INFO " %s : %d \n", __func__, __LINE__);

#if CHECK_HWID
	dev_err(&client->dev, "HW_ID = 0x%x, new HW_ID = 0x%x\n",
				hw_id, new_hw_id);
	if (hw_id != new_hw_id)
		return false;
#endif

#if defined(TSP_VERBOSE_DEBUG)
	dev_err(&client->dev, "version = 0x%x, new version = 0x%x\n",
				version, new_version);
#endif
	if (version < new_version)
		return true;
	else if (version > new_version)
		return false;

#if defined(TSP_VERBOSE_DEBUG)
	dev_err(&client->dev, "minor version = 0x%x, new minor version = 0x%x\n",
				minor_version, new_minor_version);
#endif
	if (minor_version < new_minor_version)
		return true;
	else if (minor_version > new_minor_version)
		return false;

	dev_err(&client->dev, "reg data version = 0x%x, new reg data"
				" version = 0x%x\n", reg_version, new_reg_version);
	if (reg_version < new_reg_version)
		return true;

	return false;
}
#endif

#define TC_SECTOR_SZ		8

static u8 ts_upgrade_firmware(struct bt532_ts_info *info,
	const u8 *firmware_data, u32 size)
{
	struct bt532_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	u16 flash_addr;
	u8 *verify_data;
	int retry_cnt = 0;
	int i;

	verify_data = kzalloc(size, GFP_KERNEL);
	if (verify_data == NULL) {
		dev_err(&client->dev, "%s: Failed to allocate memory\n", __func__);

		return false;
	}

retry_upgrade:
	bt532_power_control(info, POWER_OFF);
	bt532_power_control(info, POWER_ON);
	mdelay(100);

	if (write_reg(client, 0xc000, 0x0001) != I2C_SUCCESS)
		goto fail_upgrade;

	udelay(10);

	if (write_cmd(client, 0xc004) != I2C_SUCCESS)
		goto fail_upgrade;

	udelay(10);

	if (write_reg(client, 0xc002, 0x0001) != I2C_SUCCESS)
		goto fail_upgrade;

	mdelay(5);

#if defined(TSP_VERBOSE_DEBUG)
	dev_err(&client->dev, "init flash\n");
#endif

	if (write_reg(client, 0xc003, 0x0001) != I2C_SUCCESS)
		goto fail_upgrade;


	if (write_reg(client, 0xc104, 0x0001) != I2C_SUCCESS)
		goto fail_upgrade;


	if (write_cmd(client, BT532_INIT_FLASH) != I2C_SUCCESS) {
		dev_err(&client->dev, "Failed to init flash\n");

		goto fail_upgrade;
	}

	dev_err(&client->dev, "Write firmware data\n");
	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < pdata->page_size / TC_SECTOR_SZ; i++) {
			/*zinitix_debug_msg("write :addr=%04x, len=%d\n",
							flash_addr, TC_SECTOR_SZ);*/
			if (write_data(client, BT532_WRITE_FLASH,
				(u8 *)&firmware_data[flash_addr],TC_SECTOR_SZ) < 0) {
				dev_err(&client->dev, "Failed to write firmare\n");

				goto fail_upgrade;
			}
			flash_addr += TC_SECTOR_SZ;
			udelay(100);
		}

		mdelay(30); /*for fuzing delay*/
	}

	if (write_reg(client, 0xc003, 0x0000) != I2C_SUCCESS)
		goto fail_upgrade;


	if (write_reg(client, 0xc104, 0x0000) != I2C_SUCCESS)
		goto fail_upgrade;


#if defined(TSP_VERBOSE_DEBUG)
	dev_err(&client->dev, "init flash\n");
#endif

	if (write_cmd(client, BT532_INIT_FLASH) != I2C_SUCCESS) {
		dev_err(&client->dev, "Failed to init flash\n");

		goto fail_upgrade;
	}

	dev_err(&client->dev, "Read firmware data\n");

	for (flash_addr = 0; flash_addr < size; ) {
		for (i = 0; i < pdata->page_size / TC_SECTOR_SZ; i++) {
			/*zinitix_debug_msg("read :addr=%04x, len=%d\n",
							flash_addr, TC_SECTOR_SZ);*/
			if (read_firmware_data(client, BT532_READ_FLASH,
				(u8*)&verify_data[flash_addr], TC_SECTOR_SZ) < 0) {
				dev_err(&client->dev, "Failed to read firmare\n");

				goto fail_upgrade;
			}

			flash_addr += TC_SECTOR_SZ;
		}
	}
	/* verify */
	dev_err(&client->dev, "verify firmware data\n");
	if (memcmp((u8 *)&firmware_data[0], (u8 *)&verify_data[0], size) == 0) {
		dev_err(&client->dev, "upgrade finished\n");
		kfree(verify_data);
		bt532_power_control(info, POWER_OFF);
		bt532_power_control(info, POWER_ON_SEQUENCE);

		return true;
	}

fail_upgrade:
	bt532_power_control(info, POWER_OFF);

	if (retry_cnt++ < INIT_RETRY_CNT) {
		dev_err(&client->dev, "Failed to upgrade. retry (%d)\n", retry_cnt);

		goto retry_upgrade;
	}

	if (verify_data != NULL)
		kfree(verify_data);

	dev_err(&client->dev, "Failed to upgrade\n");

	return false;
}

static bool ts_hw_calibration(struct bt532_ts_info *info)
{
	struct i2c_client *client = info->client;
	u16	chip_eeprom_info;
	int time_out = 0;

	if (write_reg(client, BT532_TOUCH_MODE, 0x07) != I2C_SUCCESS)
		return false;

	mdelay(10);
	write_cmd(client, BT532_CLEAR_INT_STATUS_CMD);
	mdelay(10);
	write_cmd(client, BT532_CLEAR_INT_STATUS_CMD);
	mdelay(50);
	write_cmd(client, BT532_CLEAR_INT_STATUS_CMD);
	mdelay(10);

	if (write_cmd(client, BT532_CALIBRATE_CMD) != I2C_SUCCESS)
		return false;

	if (write_cmd(client, BT532_CLEAR_INT_STATUS_CMD) != I2C_SUCCESS)
		return false;

	mdelay(10);
	write_cmd(client, BT532_CLEAR_INT_STATUS_CMD);

	/* wait for h/w calibration*/
	do {
		mdelay(500);
		write_cmd(client, BT532_CLEAR_INT_STATUS_CMD);

		if (read_data(client, BT532_EEPROM_INFO_REG,
			(u8 *)&chip_eeprom_info, 2) < 0)
			return false;

#if defined(TSP_VERBOSE_DEBUG)
		dev_info(&client->dev, "touch eeprom info = 0x%04X\n",
			chip_eeprom_info);
#endif
		if (!zinitix_bit_test(chip_eeprom_info, 0))
			break;

		if(time_out++ == 4){
			write_cmd(client, BT532_CALIBRATE_CMD);
			mdelay(10);
			write_cmd(client, BT532_CLEAR_INT_STATUS_CMD);
			dev_err(&client->dev, "h/w calibration retry timeout.\n");
		}

		if(time_out++ > 10){
			dev_err(&client->dev, "h/w calibration timeout.\n");
			break;
		}
	} while (1);

	if (write_reg(client, BT532_TOUCH_MODE, TOUCH_POINT_MODE) != I2C_SUCCESS)
		return false;

	if (info->cap_info.ic_int_mask != 0) {
		if (write_reg(client, BT532_INT_ENABLE_FLAG,
			info->cap_info.ic_int_mask) != I2C_SUCCESS)
			return false;
	}

	write_reg(client, 0xc003, 0x0001);
	write_reg(client, 0xc104, 0x0001);
	udelay(100);
	if (write_cmd(client, BT532_SAVE_CALIBRATION_CMD) != I2C_SUCCESS)
		return false;

	mdelay(1000);
	write_reg(client, 0xc003, 0x0000);
	write_reg(client, 0xc104, 0x0000);

	return true;
}

static bool init_touch(struct bt532_ts_info *info, bool force)
{
	struct bt532_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	struct capa_info *cap = &(info->cap_info);
	u16 reg_val;
	int i;
	u16 chip_eeprom_info;
#if USE_CHECKSUM
	u16 chip_check_sum;
	u8 checksum_err;
#endif
	int retry_cnt = 0;

retry_init:

	printk(KERN_INFO " %s, %d start\n", __func__, __LINE__);
	
	for(i = 0; i < INIT_RETRY_CNT; i++) {
		if (read_data(client, BT532_EEPROM_INFO_REG,
						(u8 *)&chip_eeprom_info, 2) < 0) {
			dev_err(&client->dev, "Failed to read eeprom info(%d)\n", i);
			mdelay(10);
			continue;
		} else
			break;
	}

	if (i == INIT_RETRY_CNT)
		goto fail_init;

#if USE_CHECKSUM
	checksum_err = 0;

	for (i = 0; i < INIT_RETRY_CNT; i++) {
		if (read_data(client, BT532_CHECKSUM_RESULT,
						(u8 *)&chip_check_sum, 2) < 0) {
			mdelay(10);
			continue;
		}

#if defined(TSP_VERBOSE_DEBUG)
		dev_err(&client->dev, "fw checksum: 0x%04X\n", chip_check_sum);
#endif

		if(chip_check_sum == 0x55aa)
			break;
		else {
			checksum_err = 1;
			break;
		}
	}

	if (i == INIT_RETRY_CNT || checksum_err) {
		dev_err(&client->dev, "Failed to check firmware data\n");
		if(checksum_err == 1 && retry_cnt < INIT_RETRY_CNT)
			retry_cnt = INIT_RETRY_CNT;

		goto fail_init;
	}
#endif

	if (write_cmd(client, BT532_SWRESET_CMD) != I2C_SUCCESS)
		goto fail_init;

	cap->button_num = SUPPORTED_BUTTON_NUM;

	reg_val = 0;
	zinitix_bit_set(reg_val, BIT_PT_CNT_CHANGE);
	zinitix_bit_set(reg_val, BIT_DOWN);
	zinitix_bit_set(reg_val, BIT_MOVE);
	zinitix_bit_set(reg_val, BIT_UP);
#if (TOUCH_POINT_MODE == 2)
	zinitix_bit_set(reg_val, BIT_PALM);
	zinitix_bit_set(reg_val, BIT_PALM_REJECT);
#endif

	if (cap->button_num > 0)
		zinitix_bit_set(reg_val, BIT_ICON_EVENT);

	cap->ic_int_mask = reg_val;

	if (write_reg(client, BT532_INT_ENABLE_FLAG, 0x0) != I2C_SUCCESS)
		goto fail_init;

	if (write_cmd(client, BT532_SWRESET_CMD) != I2C_SUCCESS)
		goto fail_init;

	/* get chip information */
	if (read_data(client, BT532_VENDOR_ID, (u8 *)&cap->vendor_id, 2) < 0)
		goto fail_init;


	if (read_data(client, BT532_CHIP_REVISION,
					(u8 *)&cap->ic_revision, 2) < 0)
		goto fail_init;

	printk(KERN_INFO " %s, veid:%d, icver:%d\n", __func__, cap->vendor_id, cap->ic_revision);

	cap->ic_fw_size = 32 * 1024;

	if (read_data(client, BT532_HW_ID, (u8 *)&cap->hw_id, 2) < 0)
		goto fail_init;

	if (read_data(client, BT532_THRESHOLD, (u8 *)&cap->threshold, 2) < 0)
		goto fail_init;

	if (read_data(client, BT532_BUTTON_SENSITIVITY,
					(u8 *)&cap->key_threshold, 2) < 0)
		goto fail_init;

/*	if (read_data(client, BT532_DUMMY_BUTTON_SENSITIVITY,
					(u8 *)&cap->dummy_threshold, 2) < 0)
		goto fail_init;
*/
	if (read_data(client, BT532_TOTAL_NUMBER_OF_X,
					(u8 *)&cap->x_node_num, 2) < 0)
		goto fail_init;

	if (read_data(client, BT532_TOTAL_NUMBER_OF_Y,
					(u8 *)&cap->y_node_num, 2) < 0)
		goto fail_init;

	cap->total_node_num = cap->x_node_num * cap->y_node_num;

	if (read_data(client, BT532_AFE_FREQUENCY,
					(u8 *)&cap->afe_frequency, 2) < 0)
		goto fail_init;

	//--------------------------------------------------------

	/* get chip firmware version */
	if (read_data(client, BT532_FIRMWARE_VERSION,
					(u8 *)&cap->fw_version, 2) < 0)
		goto fail_init;

	if (read_data(client, BT532_MINOR_FW_VERSION,
					(u8 *)&cap->fw_minor_version, 2) < 0)
		goto fail_init;

	if (read_data(client, BT532_DATA_VERSION_REG,
					(u8 *)&cap->reg_data_version, 2) < 0)
		goto fail_init;

	printk(KERN_INFO " %s, hwid:%d, fwver:%d\n", __func__, cap->hw_id, cap->fw_version);

#if TOUCH_ONESHOT_UPGRADE
	if (!force) {
		
		printk(KERN_INFO " updating : 0 \n");
		
		if (ts_check_need_upgrade(info, cap->fw_version,
				cap->fw_minor_version, cap->reg_data_version,
				cap->hw_id) == true) {

			printk(KERN_INFO " updating : 1 \n");

			if(ts_upgrade_firmware(info, m_firmware_data,
				cap->ic_fw_size) == false)
				goto fail_init;

			printk(KERN_INFO " updating : 2 \n");

			if(ts_hw_calibration(info) == false)
				goto fail_init;

			printk(KERN_INFO " updating : 3 \n");

			/* disable chip interrupt */
			if (write_reg(client, BT532_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
				goto fail_init;

			printk(KERN_INFO " updating : 4 \n");

			/* get chip firmware version */
			if (read_data(client, BT532_FIRMWARE_VERSION,
							(u8 *)&cap->fw_version, 2) < 0)
				goto fail_init;

			printk(KERN_INFO " updating : 5 \n");

			if (read_data(client, BT532_MINOR_FW_VERSION,
							(u8 *)&cap->fw_minor_version, 2) < 0)
				goto fail_init;

			printk(KERN_INFO " updating : 6 \n");

			if (read_data(client, BT532_DATA_VERSION_REG,
							(u8 *)&cap->reg_data_version, 2) < 0)
				goto fail_init;
			
			printk(KERN_INFO " updating : 7 -end \n");
		}
	}
#endif

	if (read_data(client, BT532_EEPROM_INFO_REG,
					(u8 *)&chip_eeprom_info, 2) < 0)
		goto fail_init;

	if (zinitix_bit_test(chip_eeprom_info, 0)) { /* hw calibration bit*/
		if(ts_hw_calibration(info) == false)
			goto fail_init;

		/* disable chip interrupt */
		if (write_reg(client, BT532_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			goto fail_init;
	}

	/* initialize */
	if (write_reg(client, BT532_X_RESOLUTION,
					(u16)pdata->x_resolution) != I2C_SUCCESS)
		goto fail_init;

	if (write_reg(client, BT532_Y_RESOLUTION,
					(u16)pdata->y_resolution) != I2C_SUCCESS)
		goto fail_init;

	cap->MinX = (u32)0;
	cap->MinY = (u32)0;
	cap->MaxX = (u32)pdata->x_resolution;
	cap->MaxY = (u32)pdata->y_resolution;

	if (write_reg(client, BT532_BUTTON_SUPPORTED_NUM,
					(u16)cap->button_num) != I2C_SUCCESS)
		goto fail_init;

	if (write_reg(client, BT532_SUPPORTED_FINGER_NUM,
					(u16)MAX_SUPPORTED_FINGER_NUM) != I2C_SUCCESS)
		goto fail_init;

	cap->multi_fingers = MAX_SUPPORTED_FINGER_NUM;
	cap->gesture_support = 0;

	if (write_reg(client, BT532_INITIAL_TOUCH_MODE,
					TOUCH_POINT_MODE) != I2C_SUCCESS)
		goto fail_init;

	if (write_reg(client, BT532_TOUCH_MODE, info->touch_mode) != I2C_SUCCESS)
		goto fail_init;

	if (write_reg(client, BT532_INT_ENABLE_FLAG,
					cap->ic_int_mask) != I2C_SUCCESS)
		goto fail_init;

	/* soft calibration */
	if (write_cmd(client, BT532_SWRESET_CMD) != I2C_SUCCESS)
		goto fail_init;

#ifdef DEBUG_I2C_CHECKSUM
	if (read_data(client, ZINITIX_INTERNAL_FLAG_02,
			(u8 *)&reg_val, 2) < 0)
			goto fail_init;

	cap->i2s_checksum = !(!zinitix_bit_test(reg_val, 15));
	zinitix_printk("use i2s checksum = %d\r\n", cap->i2s_checksum);
#endif

	/* read garbage data */
	for (i = 0; i < 10; i++) {
		write_cmd(client, BT532_CLEAR_INT_STATUS_CMD);
		udelay(10);
	}

	if (info->touch_mode != TOUCH_POINT_MODE) { /* Test Mode */
		if (write_reg(client, BT532_DELAY_RAW_FOR_HOST,
						RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS) {
			dev_err(&client->dev, "%s: Failed to set DELAY_RAW_FOR_HOST\n",
						__func__);

			goto fail_init;
		}
	}
#if ESD_TIMER_INTERVAL
	if (write_reg(client, BT532_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL) != I2C_SUCCESS)
		goto fail_init;
#endif

	if (write_reg(client, 0x003e,	1) != I2C_SUCCESS)
		goto fail_init;
	if (write_reg(client, 0x003f,	40) != I2C_SUCCESS)
		goto fail_init;
	if (write_reg(client, 0x00c6,	70) != I2C_SUCCESS)
		goto fail_init;
	if (write_reg(client, 0x00c7,	20) != I2C_SUCCESS)
		goto fail_init;
	if (write_reg(client, 0x00d4,	3000) != I2C_SUCCESS)
		goto fail_init;
	if (write_reg(client, 0x0033,	0x0103) != I2C_SUCCESS)
		goto fail_init;
	printk(KERN_INFO " %s, %d end \n", __func__, __LINE__);

	return true;

fail_init:

	
	printk(KERN_INFO " %s, %d fail \n", __func__, __LINE__);

	if (++retry_cnt <= INIT_RETRY_CNT) {
		bt532_power_control(info, POWER_OFF);
		bt532_power_control(info, POWER_ON_SEQUENCE);

		goto retry_init;

	} else if(retry_cnt == INIT_RETRY_CNT+1) {
		cap->ic_fw_size = 32*1024;

#if TOUCH_FORCE_UPGRADE
		if (ts_upgrade_firmware(info, m_firmware_data,
						cap->ic_fw_size) == false) {
			dev_err(&client->dev, "Failed to upgrade\n");

			return false;
		}
		mdelay(100);

		// hw calibration and make checksum
		if(ts_hw_calibration(info) == false)
			return false;

		goto retry_init;
#endif
	}

	dev_err(&client->dev, "Failed to initiallize\n");

	return false;
}

static bool mini_init_touch(struct bt532_ts_info *info)
{
	struct bt532_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	int i;
#if USE_CHECKSUM
	u16 chip_check_sum;

	if (read_data(client, BT532_CHECKSUM_RESULT,
					(u8 *)&chip_check_sum, 2) < 0)
		goto fail_mini_init;

	if(chip_check_sum != 0x55aa) {
		dev_err(&client->dev, "%s: Failed to check firmware data\n",
				__func__);

		goto fail_mini_init;
	}
#endif

	if (write_cmd(client, BT532_SWRESET_CMD) != I2C_SUCCESS)
		goto fail_mini_init;


	/* initialize */
	if (write_reg(client, BT532_X_RESOLUTION,
					(u16)(pdata->x_resolution)) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client,BT532_Y_RESOLUTION,
					(u16)(pdata->y_resolution)) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, BT532_BUTTON_SUPPORTED_NUM,
					(u16)info->cap_info.button_num) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, BT532_SUPPORTED_FINGER_NUM,
					(u16)MAX_SUPPORTED_FINGER_NUM) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, BT532_INITIAL_TOUCH_MODE,
					TOUCH_POINT_MODE) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, BT532_TOUCH_MODE, info->touch_mode) != I2C_SUCCESS)
		goto fail_mini_init;

	if (write_reg(client, BT532_INT_ENABLE_FLAG,
					info->cap_info.ic_int_mask) != I2C_SUCCESS)
		goto fail_mini_init;

	/* soft calibration */
	if (write_cmd(client, BT532_CALIBRATE_CMD) != I2C_SUCCESS)
		goto fail_mini_init;

	/* read garbage data */
	for (i = 0; i < 10; i++) {
		write_cmd(client, BT532_CLEAR_INT_STATUS_CMD);
		udelay(10);
	}

	if (info->touch_mode != TOUCH_POINT_MODE) {
		if (write_reg(client, BT532_DELAY_RAW_FOR_HOST,
						RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS)
			goto fail_mini_init;
	}

#if ESD_TIMER_INTERVAL
	if (write_reg(client, BT532_PERIODICAL_INTERRUPT_INTERVAL,
			SCAN_RATE_HZ * ESD_TIMER_INTERVAL) != I2C_SUCCESS)
		goto fail_mini_init;

	esd_timer_start(CHECK_ESD_TIMER, info);
#endif

	if (write_reg(client, 0x003e,	1) != I2C_SUCCESS)
		goto fail_mini_init;
	if (write_reg(client, 0x003f,	40) != I2C_SUCCESS)
		goto fail_mini_init;
	if (write_reg(client, 0x00c6,	70) != I2C_SUCCESS)
		goto fail_mini_init;
	if (write_reg(client, 0x00c7,	20) != I2C_SUCCESS)
		goto fail_mini_init;
	if (write_reg(client, 0x00d4,	3000) != I2C_SUCCESS)
		goto fail_mini_init;
	if (write_reg(client, 0x0033,	0x0103) != I2C_SUCCESS)
		goto fail_mini_init;
	return true;

fail_mini_init:
	
	printk(KERN_INFO "%s, fail mini init \n",__func__);

	bt532_power_control(info, POWER_OFF);
	bt532_power_control(info, POWER_ON_SEQUENCE);

	if(init_touch(info, false) == false) {
		dev_err(&client->dev, "Failed to initialize\n");

		return false;
	}

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, info);
#endif
	return true;
}

#ifdef TOUCH_BOOSTER_DVFS
static void zinitix_change_dvfs_lock(struct work_struct *work)
{
	struct bt532_ts_info *info =
		container_of(work,
			struct bt532_ts_info, work_dvfs_chg.work);
	int retval = 0;

	mutex_lock(&info->dvfs_lock);

	if (info->dvfs_boost_mode == DVFS_STAGE_DUAL) {
                if (info->stay_awake) {
                        dev_info(&info->client->dev,
                                        "%s: do fw update, do not change cpu frequency.\n",
                                        __func__);
                } else {
                        retval = set_freq_limit(DVFS_TOUCH_ID,
                                        MIN_TOUCH_LIMIT_SECOND);
                        info->dvfs_freq = MIN_TOUCH_LIMIT_SECOND;
                }
        } else if (info->dvfs_boost_mode == DVFS_STAGE_SINGLE ||
                        info->dvfs_boost_mode == DVFS_STAGE_TRIPLE) {
                retval = set_freq_limit(DVFS_TOUCH_ID, -1);
                info->dvfs_freq = -1;
        }

        if (retval < 0)
                dev_err(&info->client->dev,
                                "%s: booster change failed(%d).\n",
                                __func__, retval);

	mutex_unlock(&info->dvfs_lock);
}

static void zinitix_set_dvfs_off(struct work_struct *work)
{
	struct bt532_ts_info *info =
		container_of(work,
			struct bt532_ts_info, work_dvfs_off.work);
	int retval;

	if (info->stay_awake) {
                dev_info(&info->client->dev,
                                "%s: do fw update, do not change cpu frequency.\n",
                                __func__);
        } else {
                mutex_lock(&info->dvfs_lock);

                retval = set_freq_limit(DVFS_TOUCH_ID, -1);
                info->dvfs_freq = -1;

                if (retval < 0)
                        dev_err(&info->client->dev,
                                        "%s: booster stop failed(%d).\n",
                                        __func__, retval);
                info->dvfs_lock_status = false;

                mutex_unlock(&info->dvfs_lock);
        }

	/*mutex_lock(&info->dvfs_lock);
	retval = set_freq_limit(DVFS_TOUCH_ID, -1);
	if (retval < 0)
		dev_info(&info->client->dev,
			"%s: booster stop failed(%d).\n",
			__func__, retval);

	info->dvfs_lock_status = false;
	mutex_unlock(&info->dvfs_lock);*/
}

static void zinitix_set_dvfs_lock(struct bt532_ts_info *info,
					uint32_t on)
{
	int ret = 0;

	if (info->dvfs_boost_mode == DVFS_STAGE_NONE) {
                dev_dbg(&info->client->dev,
                                "%s: DVFS stage is none(%d)\n",
                                __func__, info->dvfs_boost_mode);
                return;
        }

        mutex_lock(&info->dvfs_lock);
        if (on == 0) {
                if (info->dvfs_lock_status)
                        schedule_delayed_work(&info->work_dvfs_off,
                                        msecs_to_jiffies(TOUCH_BOOSTER_OFF_TIME));
        } else if (on > 0) {
                cancel_delayed_work(&info->work_dvfs_off);

                if ((!info->dvfs_lock_status) || (info->dvfs_old_stauts < on)) {
                        cancel_delayed_work(&info->work_dvfs_chg);

                        if (info->dvfs_freq != MIN_TOUCH_LIMIT) {
                                if (info->dvfs_boost_mode == DVFS_STAGE_TRIPLE)
                                        ret = set_freq_limit(DVFS_TOUCH_ID,
                                                MIN_TOUCH_LIMIT_SECOND);
                                else
                                        ret = set_freq_limit(DVFS_TOUCH_ID,
                                                MIN_TOUCH_LIMIT);
                                info->dvfs_freq = MIN_TOUCH_LIMIT;

                                if (ret < 0)
					dev_err(&info->client->dev,
                                                        "%s: cpu first lock failed(%d)\n",
                                                        __func__, ret);
                        }
			schedule_delayed_work(&info->work_dvfs_chg,
				msecs_to_jiffies(TOUCH_BOOSTER_CHG_TIME));

                        info->dvfs_lock_status = true;
                }
        } else if (on < 0) {
                if (info->dvfs_lock_status) {
                        cancel_delayed_work(&info->work_dvfs_off);
                        cancel_delayed_work(&info->work_dvfs_chg);
                        schedule_work(&info->work_dvfs_off.work);
                }
        }
        info->dvfs_old_stauts = on;
        mutex_unlock(&info->dvfs_lock);

	/*mutex_lock(&info->dvfs_lock);
	if (on == 0) {
		if (info->dvfs_lock_status) {
			schedule_delayed_work(&info->work_dvfs_off,
				msecs_to_jiffies(TOUCH_BOOSTER_OFF_TIME));
		}
	} else if (on == 1) {
		cancel_delayed_work(&info->work_dvfs_off);
		if (!info->dvfs_lock_status) {
			ret = set_freq_limit(DVFS_TOUCH_ID,
					MIN_TOUCH_LIMIT);
			if (ret < 0)
				dev_info(&info->client->dev,
					"%s: cpu first lock failed(%d)\n",
					__func__, ret);

			schedule_delayed_work(&info->work_dvfs_chg,
				msecs_to_jiffies(TOUCH_BOOSTER_CHG_TIME));
			info->dvfs_lock_status = true;
		}
	} else if (on == 2) {
		if (info->dvfs_lock_status) {
			cancel_delayed_work(&info->work_dvfs_off);
			cancel_delayed_work(&info->work_dvfs_chg);
			schedule_work(&info->work_dvfs_off.work);
		}
	}
	mutex_unlock(&info->dvfs_lock);*/
}


static void zinitix_init_dvfs(struct bt532_ts_info *info)
{
	mutex_init(&info->dvfs_lock);

	info->dvfs_boost_mode = DVFS_STAGE_DUAL;

	INIT_DELAYED_WORK(&info->work_dvfs_off, zinitix_set_dvfs_off);
	INIT_DELAYED_WORK(&info->work_dvfs_chg, zinitix_change_dvfs_lock);

	info->dvfs_lock_status = false;
}
#endif

static void clear_report_data(struct bt532_ts_info *info)
{
	struct i2c_client *client = info->client;
	int i;
	u8 reported = 0;
	u8 sub_status;

	for (i = 0; i < info->cap_info.button_num; i++) {
		if (info->button[i] == ICON_BUTTON_DOWN) {
			info->button[i] = ICON_BUTTON_UP;
			input_report_key(info->input_dev, BUTTON_MAPPING_KEY[i], 0);
			reported = true;
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			dev_info(&client->dev, "Button up = %d\n", i);
#endif
		}
	}

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		sub_status = info->reported_touch_info.coord[i].sub_status;
		if (zinitix_bit_test(sub_status, SUB_BIT_EXIST)) {
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev,	MT_TOOL_FINGER, 0);
			reported = true;
			dev_info(&client->dev, "Finger [%02d] up\n", i);
		}
		info->reported_touch_info.coord[i].sub_status = 0;
	}

	if (reported) {
		input_sync(info->input_dev);
	}

#ifdef TOUCH_BOOSTER_DVFS
	info->finger_cnt1=0;
	/*zinitix_set_dvfs_lock(info, info->finger_cnt1);*/
	zinitix_set_dvfs_lock(info, -1);
#endif
	
}
#if 0
static void zinitix_ta_cb(struct tsp_callbacks *cb, int ta_status)
{
	struct bt532_ts_info *info = misc_info;
	struct i2c_client *client = info->client;
	u16 val;

	dev_info(&client->dev,
		"%s: ta_status : %x\n", __func__, ta_status);

	info->ta_status = ta_status;
	val = ta_status;

	if (info->pdata->is_vdd_on() == 0) {
		/* ta_status 0: charger disconnected
		 * ta_status 1: charger connected
		 * ta_status 2: charger disconnected but tsp ic is power-off
		 * ta_status 3: charger connected but tsp ic is power-off */
		info->ta_status = ta_status ? 3 : 2;
		return;
	}

	switch (info->ta_status) {
	case 0:	/* TA detach */
		write_data(client, BT532_HOLD_POINT_THRESHOLD, (u8 *)&val, 2);
		write_cmd(client, BT532_SAVE_STATUS_CMD);
		break;
	case 1: /* TA attach */
		write_data(client, BT532_HOLD_POINT_THRESHOLD, (u8 *)&val, 2);
		write_cmd(client, BT532_SAVE_STATUS_CMD);
		break;
	}
}
#endif

#define	PALM_REPORT_WIDTH	200
#define	PALM_REJECT_WIDTH	255

static irqreturn_t bt532_touch_irq_handler(int irq, void *data)
{
	struct bt532_ts_info* info = (struct bt532_ts_info*)data;
	struct bt532_ts_platform_data *pdata = info->pdata;
	struct i2c_client *client = info->client;
	int i;
	u8 sub_status;
	u8 prev_sub_status;
	u16 prev_icon_data;
	u32 x, y, maxX, maxY;
	u32 w;
	u32 tmp;
#ifdef DEBUG_I2C_CHECKSUM
	u8 read_result = 1;
#endif
	u8 palm = 0;

	if (gpio_get_value(info->pdata->gpio_int)) {
		dev_err(&client->dev, "Invalid interrupt\n");

		return IRQ_HANDLED;
	}

	if (down_trylock(&info->work_lock)) {
		dev_err(&client->dev, "%s: Failed to occupy work lock\n", __func__);
		write_cmd(client, BT532_CLEAR_INT_STATUS_CMD);

		return IRQ_HANDLED;
	}

	if (info->work_state != NOTHING) {
		dev_err(&client->dev, "%s: Other process occupied\n", __func__);
		udelay(DELAY_FOR_SIGNAL_DELAY);

		if (!gpio_get_value(info->pdata->gpio_int)) {
			write_cmd(client, BT532_CLEAR_INT_STATUS_CMD);
			udelay(DELAY_FOR_SIGNAL_DELAY);
		}

		goto out;
	}

	info->work_state = NORMAL;

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
#endif

	if (ts_read_coord(info) == false || info->touch_info.status == 0xffff
		|| info->touch_info.status == 0x1) {
		// more retry
#ifdef DEBUG_I2C_CHECKSUM
		for(i=0; i< 50; i++) {	// about 30ms
			if(!(ts_read_coord(info) == false|| info->touch_info.status == 0xffff
			|| info->touch_info.status == 0x1))
				break;
		}

		if(i==50)
			read_result = 0;
	}
	if (!read_result) {
#endif
		dev_err(&client->dev, "Failed to read info coord\n");
		bt532_power_control(info, POWER_OFF);
		bt532_power_control(info, POWER_ON_SEQUENCE);

		clear_report_data(info);
		mini_init_touch(info);

		goto out;
	}

	/* invalid : maybe periodical repeated int. */
	if (info->touch_info.status == 0x0)
		goto out;

	if (zinitix_bit_test(info->touch_info.status, BIT_ICON_EVENT)) {
#ifndef DEBUG_I2C_CHECKSUM
		if (read_data(info->client, BT532_ICON_STATUS_REG,
			(u8 *)(&info->icon_event_reg), 2) < 0) {
			dev_err(&client->dev, "Failed to read button info\n");

			goto out;
		}
#endif
		prev_icon_data = info->icon_event_reg ^ info->prev_icon_event;

		for (i = 0; i < info->cap_info.button_num; i++) {
			if (zinitix_bit_test(info->icon_event_reg & prev_icon_data,
									(BIT_O_ICON0_DOWN + i))) {
				info->button[i] = ICON_BUTTON_DOWN;
				input_report_key(info->input_dev, BUTTON_MAPPING_KEY[i], 1);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				dev_info(&client->dev, "Button down = %d\n", i);
#endif
			}
		}

		for (i = 0; i < info->cap_info.button_num; i++) {
			if (zinitix_bit_test(info->icon_event_reg & prev_icon_data,
									(BIT_O_ICON0_UP + i))) {
				info->button[i] = ICON_BUTTON_UP;
				input_report_key(info->input_dev, BUTTON_MAPPING_KEY[i], 0);
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				dev_info(&client->dev, "Button up = %d\n", i);
#endif
			}
		}

		info->prev_icon_event = info->icon_event_reg;
	}

	if (zinitix_bit_test(info->touch_info.status, BIT_PALM)) {
		dev_info(&client->dev, "Palm report\n");
		palm = 1;
	}

	if (zinitix_bit_test(info->touch_info.status, BIT_PALM_REJECT)){
		dev_info(&client->dev, "Palm reject\n");
		palm = 1;
	}

	for (i = 0; i < info->cap_info.multi_fingers; i++) {
		sub_status = info->touch_info.coord[i].sub_status;
		prev_sub_status = info->reported_touch_info.coord[i].sub_status;

		if (zinitix_bit_test(sub_status, SUB_BIT_EXIST)) {
			x = info->touch_info.coord[i].x;
			y = info->touch_info.coord[i].y;
			w = info->touch_info.coord[i].width;

			 /* transformation from touch to screen orientation */
			if (pdata->orientation & TOUCH_V_FLIP)
				y = info->cap_info.MaxY
					+ info->cap_info.MinY - y;

			if (pdata->orientation & TOUCH_H_FLIP)
				x = info->cap_info.MaxX
					+ info->cap_info.MinX - x;

			maxX = info->cap_info.MaxX;
			maxY = info->cap_info.MaxY;

			if (pdata->orientation & TOUCH_XY_SWAP) {
				zinitix_swap_v(x, y, tmp);
				zinitix_swap_v(maxX, maxY, tmp);
			}

			if (x > maxX || y > maxY) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				dev_err(&client->dev,
							"Invalid coord %d : x=%d, y=%d\n", i, x, y);
#endif
				continue;
			}

			info->touch_info.coord[i].x = x;
			info->touch_info.coord[i].y = y;

			if (w == 0)
				w = 1;

			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 1);

#if (TOUCH_POINT_MODE == 2)
			if (palm == 0) {
				if(w >= PALM_REPORT_WIDTH)
					w = PALM_REPORT_WIDTH - 10;
			} else if (palm == 1) {	//palm report
				w = PALM_REPORT_WIDTH;
//				info->touch_info.coord[i].minor_width = PALM_REPORT_WIDTH;
			} else if (palm == 2){	// palm reject
//				x = y = 0;
				w = PALM_REJECT_WIDTH;
//				info->touch_info.coord[i].minor_width = PALM_REJECT_WIDTH;
			}
#endif

			input_report_abs(info->input_dev, ABS_MT_TOUCH_MAJOR, (u32)w);
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, (u32)w);
			input_report_abs(info->input_dev, ABS_MT_WIDTH_MAJOR,
								(u32)((palm == 1)?w-40:w));
#if (TOUCH_POINT_MODE == 2)
			input_report_abs(info->input_dev,
				ABS_MT_TOUCH_MINOR, (u32)info->touch_info.coord[i].minor_width);
//			input_report_abs(info->input_dev,
//				ABS_MT_WIDTH_MINOR, (u32)info->touch_info.coord[i].minor_width);
			input_report_abs(info->input_dev, ABS_MT_ANGLE,
						(palm > 1)?70:info->touch_info.coord[i].angle - 90);
//			dev_info(&client->dev, "finger [%02d] angle = %03d\n", i,
//						info->touch_info.coord[i].angle);
			input_report_abs(info->input_dev, ABS_MT_PALM, (palm > 0)?1:0);
//			input_report_abs(info->input_dev, ABS_MT_PALM, 1);
#endif

			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);

			if (zinitix_bit_test(sub_status, SUB_BIT_DOWN)) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
				dev_info(&client->dev, "Finger [%02d] x = %d, y = %d,"
							" w = %d\n", i, x, y, w);
#else
				dev_info(&client->dev, "Finger [%02d] down\n", i);
#endif

#ifdef TOUCH_BOOSTER_DVFS
				info->finger_cnt1++;
				/*zinitix_set_dvfs_lock(info, !!info->finger_cnt1);*/
#endif
			}
		} else if (zinitix_bit_test(sub_status, SUB_BIT_UP)||
			zinitix_bit_test(prev_sub_status, SUB_BIT_EXIST)) {
			memset(&info->touch_info.coord[i], 0x0, sizeof(struct coord));
			input_mt_slot(info->input_dev, i);
			input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);
			dev_info(&client->dev, "Finger [%02d] up\n", i);

#ifdef TOUCH_BOOSTER_DVFS
			info->finger_cnt1--;
			/*zinitix_set_dvfs_lock(info, info->finger_cnt1);*/
#endif
			
		} else {
			memset(&info->touch_info.coord[i], 0x0, sizeof(struct coord));
		}
	}
	memcpy((char *)&info->reported_touch_info, (char *)&info->touch_info,
			sizeof(struct point_info));
	input_sync(info->input_dev);

#ifdef TOUCH_BOOSTER_DVFS
	zinitix_set_dvfs_lock(info, info->finger_cnt1);
#endif

out:
	if (info->work_state == NORMAL) {
#if ESD_TIMER_INTERVAL
		esd_timer_start(CHECK_ESD_TIMER, info);
#endif
		info->work_state = NOTHING;
	}

	up(&info->work_lock);

	return IRQ_HANDLED;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void bt532_ts_late_resume(struct early_suspend *h)
{
	struct bt532_ts_info *info = misc_info;

	printk(KERN_INFO " %s, %d \n", __func__, __LINE__);

	if (info == NULL)
		return;

	down(&info->work_lock);
	if (info->work_state != RESUME
		&& info->work_state != EALRY_SUSPEND) {
		zinitix_printk("invalid work proceedure (%d)\r\n",
			info->work_state);
		up(&info->work_lock);
		return;
	}
#ifdef ZCONFIG_PM_SLEEP
	write_cmd(info->client, BT532_WAKEUP_CMD);
	mdelay(1);
#else
	bt532_power_control(info, POWER_ON_SEQUENCE);
#endif
	if (mini_init_touch(info) == false)
		goto fail_late_resume;
/*
	if (info->ta_status == 2)
		zinitix_ta_cb(charger_callbacks, 0);
	else if (info->ta_status == 3)
		zinitix_ta_cb(charger_callbacks, 1);
*/
	enable_irq(info->irq);
	info->work_state = NOTHING;
	up(&info->work_lock);
	zinitix_printk("late resume--\n");
	return;
fail_late_resume:
	zinitix_printk("failed to late resume\n");
	enable_irq(info->irq);
	info->work_state = NOTHING;
	up(&info->work_lock);
	return;
}

static void bt532_ts_early_suspend(struct early_suspend *h)
{
	struct bt532_ts_info *info = misc_info;
	/*info = container_of(h, struct bt532_ts_info, early_suspend);*/

	printk(KERN_INFO " %s, %d \n", __func__, __LINE__);

	if (info == NULL)
		return;

	disable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
#endif

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		zinitix_printk("invalid work proceedure (%d)\r\n",
			info->work_state);
		up(&info->work_lock);
		enable_irq(info->irq);
		return;
	}
	info->work_state = EALRY_SUSPEND;

	clear_report_data(info);

#if ESD_TIMER_INTERVAL
	/*write_reg(info->client, BT532_PERIODICAL_INTERRUPT_INTERVAL, 0);*/
	esd_timer_stop(info);
#endif

#ifdef ZCONFIG_PM_SLEEP
	write_reg(info->client, BT532_INT_ENABLE_FLAG, 0x0);

	udelay(100);
	if (write_cmd(info->client, BT532_SLEEP_CMD) != I2C_SUCCESS) {
		zinitix_printk("failed to enter into sleep mode\n");
		up(&info->work_lock);
		return;
	}
#else
	bt532_power_control(info, POWER_OFF);
#endif
	zinitix_printk("early suspend--\n");
	up(&info->work_lock);
	return;
}
#endif	/* CONFIG_HAS_EARLYSUSPEND */

#ifdef USE_OPEN_CLOSE
static int  bt532_ts_open(struct input_dev *dev)
{
	struct bt532_ts_info *info = misc_info;

	printk(KERN_INFO "%s, %d \n", __func__, __LINE__);

	if (info == NULL)
		return 0;

	down(&info->work_lock);
	if (info->work_state != RESUME
		&& info->work_state != EALRY_SUSPEND) {
		zinitix_printk("invalid work proceedure (%d)\r\n",
			info->work_state);
		up(&info->work_lock);
		return 0;
	}

	bt532_power_control(info, POWER_ON_SEQUENCE);

	if (mini_init_touch(info) == false)
		goto fail_late_resume;
/*
	if (info->ta_status == 2)
		zinitix_ta_cb(charger_callbacks, 0);
	else if (info->ta_status == 3)
		zinitix_ta_cb(charger_callbacks, 1);
*/
	enable_irq(info->irq);
	info->work_state = NOTHING;
	up(&info->work_lock);
	zinitix_printk("late resume--\n");
	return 0;
fail_late_resume:
	zinitix_printk("failed to late resume\n");
	enable_irq(info->irq);
	info->work_state = NOTHING;
	up(&info->work_lock);
	return 0;
}

static void bt532_ts_close(struct input_dev *dev)
{
	struct bt532_ts_info *info = misc_info;
	/*info = container_of(h, struct bt532_ts_info, early_suspend);*/

	printk(KERN_INFO "%s, %d \n", __func__, __LINE__);

	if (info == NULL)
		return;

	disable_irq(info->irq);
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
#endif

	down(&info->work_lock);
	if (info->work_state != NOTHING) {
		zinitix_printk("invalid work proceedure (%d)\r\n",
			info->work_state);
		up(&info->work_lock);
		enable_irq(info->irq);
		return;
	}
	info->work_state = EALRY_SUSPEND;

	clear_report_data(info);

#if ESD_TIMER_INTERVAL
	/*write_reg(info->client, BT532_PERIODICAL_INTERRUPT_INTERVAL, 0);*/
	esd_timer_stop(info);
#endif

	bt532_power_control(info, POWER_OFF);

	zinitix_printk("early suspend--\n");
	up(&info->work_lock);
	return;
}
#endif	/* USE_OPEN_CLOSE */


#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static int bt532_ts_resume(struct device *dev)
{
	struct bt532_ts_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;

	printk(KERN_INFO " %s, %d \n", __func__, __LINE__);

	down(&info->work_lock);
	if (info->work_state != SUSPEND) {
		dev_err(&client->dev, "%s: Invalid work proceedure (%d)\n",
				__func__, info->work_state);
		up(&info->work_lock);

		return 0;
	}

	bt532_power_control(info, POWER_ON_SEQUENCE);

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->work_state = RESUME;
#else
	enable_irq(info->irq);
	info->work_state = NOTHING;

	if (mini_init_touch(info) == false)
		dev_err(&client->dev, "Failed to resume\n");
#endif

#if defined(TSP_VERBOSE_DEBUG)
	dev_info(&client->dev, "resume\n");
#endif
	up(&info->work_lock);

	return 0;
}

static int bt532_ts_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct bt532_ts_info *info = i2c_get_clientdata(client);

	printk(KERN_INFO " %s, %d \n", __func__, __LINE__);

#ifndef CONFIG_HAS_EARLYSUSPEND
	disable_irq(info->irq);
#endif
#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
#endif

	down(&info->work_lock);
	if (info->work_state != NOTHING	&& info->work_state != EALRY_SUSPEND) {
		dev_err(&client->dev, "%s: Invalid work proceedure (%d)\n",
			__func__, info->work_state);
		up(&info->work_lock);
#ifndef CONFIG_HAS_EARLYSUSPEND
		enable_irq(info->irq);
#endif
		return 0;
	}

#ifndef CONFIG_HAS_EARLYSUSPEND
	clear_report_data(info);

#if ESD_TIMER_INTERVAL
	esd_timer_stop(info);
#endif
#endif
	bt532_power_control(info, POWER_OFF);
	info->work_state = SUSPEND;

#if defined(TSP_VERBOSE_DEBUG)
	dev_info(&client->dev, "suspend\n");
#endif
	up(&info->work_lock);

	return 0;
}
#endif

static bool ts_set_touchmode(u16 value){
	int i;

	disable_irq(misc_info->irq);

	down(&misc_info->work_lock);
	if (misc_info->work_state != NOTHING) {
		dev_err(&misc_info->client->dev, "%s: Other process occupied (%d)\n",
			__func__, misc_info->work_state);
		enable_irq(misc_info->irq);
		up(&misc_info->work_lock);

		return -1;
	}

	misc_info->work_state = SET_MODE;

	if (value == TOUCH_DND_MODE) {
		if (write_reg(misc_info->client, BT532_DND_N_COUNT,
						SEC_DND_N_COUNT) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Failed to set DND_N_COUNT\n");

		if (write_reg(misc_info->client, BT532_AFE_FREQUENCY,
						SEC_DND_FREQUENCY) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev,  "Failed to set AFE_FREQUENCY\n");

	} else if (value == TOUCH_PDND_MODE) {
		if (write_reg(misc_info->client, BT532_DND_N_COUNT,
						SEC_PDND_N_COUNT) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Failed to set PDND_N_COUNT\n");

		if (write_reg(misc_info->client, BT532_DND_U_COUNT,
						SEC_PDND_U_COUNT) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Failed to set PDND_N_COUNT\n");

		if (write_reg(misc_info->client, BT532_AFE_FREQUENCY,
						SEC_PDND_FREQUENCY) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev,  "Failed to set AFE_FREQUENCY\n");
	} else if (misc_info->touch_mode == TOUCH_DND_MODE) {
		if (write_reg(misc_info->client, BT532_AFE_FREQUENCY,
						misc_info->cap_info.afe_frequency) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Failed to set AFE_FREQUENCY %d\n",
					misc_info->cap_info.afe_frequency);
	} else if (misc_info->touch_mode == TOUCH_PDND_MODE) {

		if (write_reg(misc_info->client, BT532_DND_U_COUNT,
						2) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Failed to set PDND_N_COUNT\n");

		if (write_reg(misc_info->client, BT532_AFE_FREQUENCY,
						misc_info->cap_info.afe_frequency) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "Failed to set AFE_FREQUENCY %d\n",
					misc_info->cap_info.afe_frequency);
	}

	if(value == TOUCH_SEC_MODE)
		misc_info->touch_mode = TOUCH_POINT_MODE;
	else
		misc_info->touch_mode = value;

#if defined(TSP_VERBOSE_DEBUG)
	dev_info(&misc_info->client->dev, "tsp_set_testmode, touchkey_testmode"
				" = %d\n", misc_info->touch_mode);
#endif

	if(misc_info->touch_mode != TOUCH_POINT_MODE) {
		if (write_reg(misc_info->client, BT532_DELAY_RAW_FOR_HOST,
						RAWDATA_DELAY_FOR_HOST) != I2C_SUCCESS)
			dev_err(&misc_info->client->dev, "%s: Failed to set"
						" DELAY_RAW_FOR_HOST\n", __func__);
	}

	if (write_reg(misc_info->client, BT532_TOUCH_MODE,
					misc_info->touch_mode) != I2C_SUCCESS)
		dev_err(&misc_info->client->dev,  "Failed to set TOUCH_MODE\n");

	// clear garbage data
	for(i=0; i < 10; i++) {
		mdelay(20);
		write_cmd(misc_info->client, BT532_CLEAR_INT_STATUS_CMD);
	}

	misc_info->work_state = NOTHING;
	enable_irq(misc_info->irq);
	up(&misc_info->work_lock);

	return 1;
}

static int ts_upgrade_sequence(const u8 *firmware_data)
{
	disable_irq(misc_info->irq);
	down(&misc_info->work_lock);
	misc_info->work_state = UPGRADE;

	printk(KERN_INFO " %s, %d \n", __func__, __LINE__);

#if ESD_TIMER_INTERVAL
	esd_timer_stop(misc_info);
#endif
	clear_report_data(misc_info);

	if (ts_upgrade_firmware(misc_info, firmware_data,
				misc_info->cap_info.ic_fw_size) == false)
		goto out;

	if (init_touch(misc_info, true) == false)
		goto out;

#if ESD_TIMER_INTERVAL
	esd_timer_start(CHECK_ESD_TIMER, misc_info);
#endif

	enable_irq(misc_info->irq);
	misc_info->work_state = NOTHING;
	up(&misc_info->work_lock);

	printk(KERN_INFO " %s, %d end \n", __func__, __LINE__);
	
	return 0;

out:
	enable_irq(misc_info->irq);
	misc_info->work_state = NOTHING;
	up(&misc_info->work_lock);
	return -1;
}

static int ts_misc_fops_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int ts_misc_fops_close(struct inode *inode, struct file *filp)
{
	return 0;
}

static long ts_misc_fops_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	struct raw_ioctl raw_ioctl;
	u8 *u8Data;
	int ret = 0;
	size_t sz = 0;
	u16 version;
	u16 mode;

	struct reg_ioctl reg_ioctl;
	u16 val;
	int nval = 0;

	if (misc_info == NULL)
		return -1;

	/* zinitix_debug_msg("cmd = %d, argp = 0x%x\n", cmd, (int)argp); */

	switch (cmd) {

	case TOUCH_IOCTL_GET_DEBUGMSG_STATE:
		ret = m_ts_debug_mode;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_SET_DEBUGMSG_STATE:
		if (copy_from_user(&nval, argp, 4)) {
			printk(KERN_INFO "[zinitix_touch] error : copy_from_user\n");
			return -1;
		}
		if (nval)
			printk(KERN_INFO "[zinitix_touch] on debug mode (%d)\n",
				nval);
		else
			printk(KERN_INFO "[zinitix_touch] off debug mode (%d)\n",
				nval);
		m_ts_debug_mode = nval;
		break;

	case TOUCH_IOCTL_GET_CHIP_REVISION:
		ret = misc_info->cap_info.ic_revision;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_FW_VERSION:
		ret = misc_info->cap_info.fw_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_REG_DATA_VERSION:
		ret = misc_info->cap_info.reg_data_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_VARIFY_UPGRADE_SIZE:
		if (copy_from_user(&sz, argp, sizeof(size_t)))
			return -1;

		printk(KERN_INFO "firmware size = %d\r\n", sz);
		if (misc_info->cap_info.ic_fw_size != sz) {
			printk(KERN_INFO "firmware size error\r\n");
			return -1;
		}
		break;

	case TOUCH_IOCTL_VARIFY_UPGRADE_DATA:
		if (copy_from_user(m_firmware_data,
			argp, misc_info->cap_info.ic_fw_size))
			return -1;

		version = (u16) (m_firmware_data[52] | (m_firmware_data[53]<<8));

		printk(KERN_INFO "firmware version = %x\r\n", version);

		if (copy_to_user(argp, &version, sizeof(version)))
			return -1;
		break;

	case TOUCH_IOCTL_START_UPGRADE:
		return ts_upgrade_sequence((u8*)m_firmware_data);

	case TOUCH_IOCTL_GET_X_RESOLUTION:
		ret = misc_info->pdata->x_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_RESOLUTION:
		ret = misc_info->pdata->y_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_X_NODE_NUM:
		ret = misc_info->cap_info.x_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_NODE_NUM:
		ret = misc_info->cap_info.y_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_TOTAL_NODE_NUM:
		ret = misc_info->cap_info.total_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_HW_CALIBRAION:
		ret = -1;
		disable_irq(misc_info->irq);
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			printk(KERN_INFO"other process occupied.. (%d)\r\n",
				misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}
		misc_info->work_state = HW_CALIBRAION;
		mdelay(100);

		/* h/w calibration */
		if(ts_hw_calibration(misc_info) == true)
			ret = 0;

		mode = misc_info->touch_mode;
		if (write_reg(misc_info->client,
			BT532_TOUCH_MODE, mode) != I2C_SUCCESS) {
			printk(KERN_INFO "failed to set touch mode %d.\n",
				mode);
			goto fail_hw_cal;
		}

		if (write_cmd(misc_info->client,
			BT532_SWRESET_CMD) != I2C_SUCCESS)
			goto fail_hw_cal;

		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;
fail_hw_cal:
		enable_irq(misc_info->irq);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return -1;

	case TOUCH_IOCTL_SET_RAW_DATA_MODE:
		if (misc_info == NULL) {
			zinitix_debug_msg("misc device NULL?\n");
			return -1;
		}
		if (copy_from_user(&nval, argp, 4)) {
			printk(KERN_INFO "[zinitix_touch] error : copy_from_user\r\n");
			misc_info->work_state = NOTHING;
			return -1;
		}
		ts_set_touchmode((u16)nval);

		return 0;

	case TOUCH_IOCTL_GET_REG:
		if (misc_info == NULL) {
			zinitix_debug_msg("misc device NULL?\n");
			return -1;
		}
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			printk(KERN_INFO "other process occupied.. (%d)\n",
				misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;

		if (copy_from_user(&reg_ioctl,
			argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			printk(KERN_INFO "[zinitix_touch] error : copy_from_user\n");
			return -1;
		}

		if (read_data(misc_info->client,
			reg_ioctl.addr, (u8 *)&val, 2) < 0)
			ret = -1;

		nval = (int)val;

		if (copy_to_user(reg_ioctl.val, (u8 *)&nval, 4)) {
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			printk(KERN_INFO "[zinitix_touch] error : copy_to_user\n");
			return -1;
		}

		zinitix_debug_msg("read : reg addr = 0x%x, val = 0x%x\n",
			reg_ioctl.addr, nval);

		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;

	case TOUCH_IOCTL_SET_REG:

		if (misc_info == NULL) {
			zinitix_debug_msg("misc device NULL?\n");
			return -1;
		}
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			printk(KERN_INFO "other process occupied.. (%d)\n",
				misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		if (copy_from_user(&reg_ioctl,
				argp, sizeof(struct reg_ioctl))) {
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			printk(KERN_INFO "[zinitix_touch] error : copy_from_user\n");
			return -1;
		}

		if (copy_from_user(&val, reg_ioctl.val, 4)) {
			misc_info->work_state = NOTHING;
			up(&misc_info->work_lock);
			printk(KERN_INFO "[zinitix_touch] error : copy_from_user\n");
			return -1;
		}

		if (write_reg(misc_info->client,
			reg_ioctl.addr, val) != I2C_SUCCESS)
			ret = -1;

		zinitix_debug_msg("write : reg addr = 0x%x, val = 0x%x\r\n",
			reg_ioctl.addr, val);
		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;

	case TOUCH_IOCTL_DONOT_TOUCH_EVENT:

		if (misc_info == NULL) {
			zinitix_debug_msg("misc device NULL?\n");
			return -1;
		}
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			printk(KERN_INFO"other process occupied.. (%d)\r\n",
				misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}

		misc_info->work_state = SET_MODE;
		if (write_reg(misc_info->client,
			BT532_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			ret = -1;
		zinitix_debug_msg("write : reg addr = 0x%x, val = 0x0\r\n",
			BT532_INT_ENABLE_FLAG);

		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;

	case TOUCH_IOCTL_SEND_SAVE_STATUS:
		if (misc_info == NULL) {
			zinitix_debug_msg("misc device NULL?\n");
			return -1;
		}
		down(&misc_info->work_lock);
		if (misc_info->work_state != NOTHING) {
			printk(KERN_INFO"other process occupied.. (%d)\r\n",
				misc_info->work_state);
			up(&misc_info->work_lock);
			return -1;
		}
		misc_info->work_state = SET_MODE;
		ret = 0;
		write_reg(misc_info->client, 0xc003, 0x0001);
		write_reg(misc_info->client, 0xc104, 0x0001);
		if (write_cmd(misc_info->client,
			BT532_SAVE_STATUS_CMD) != I2C_SUCCESS)
			ret =  -1;

		mdelay(1000);	/* for fusing eeprom */
		write_reg(misc_info->client, 0xc003, 0x0000);
		write_reg(misc_info->client, 0xc104, 0x0000);

		misc_info->work_state = NOTHING;
		up(&misc_info->work_lock);
		return ret;

	case TOUCH_IOCTL_GET_RAW_DATA:
		if (misc_info == NULL) {
			zinitix_debug_msg("misc device NULL?\n");
			return -1;
		}

		if (misc_info->touch_mode == TOUCH_POINT_MODE)
			return -1;

		down(&misc_info->raw_data_lock);
		if (misc_info->update == 0) {
			up(&misc_info->raw_data_lock);
			return -2;
		}

		if (copy_from_user(&raw_ioctl,
			argp, sizeof(raw_ioctl))) {
			up(&misc_info->raw_data_lock);
			printk(KERN_INFO "[zinitix_touch] error : copy_from_user\r\n");
			return -1;
		}

		misc_info->update = 0;

		u8Data = (u8 *)&misc_info->cur_data[0];

		if (copy_to_user(raw_ioctl.buf, (u8 *)u8Data,
			raw_ioctl.sz)) {
			up(&misc_info->raw_data_lock);
			return -1;
		}

		up(&misc_info->raw_data_lock);
		return 0;

	default:
		break;
	}
	return 0;
}

/*
static ssize_t zinitix_enabled_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	long input;
	int ret = 0;

	ret = kstrtol(buf, 10, &input);
	if(ret || ret < 0){
            printk(KERN_INFO "Error value zinitix_enabled_store \n");
            return ret;
        }
	pr_info("TSP enabled: %d\n",(int)input);

	if(input == 1)
		bt532_ts_resume(dev);
	else if(input == 0)
		bt532_ts_suspend(dev);
	else
		return -EINVAL;

	return count;
}

static struct device_attribute attrs[] = {
	__ATTR(enabled, (S_IRUGO | S_IWUSR | S_IWGRP),
			NULL,
			zinitix_enabled_store),
};
*/

/* Added for samsung dependent codes such as Factory test,
 * Touch booster, Related debug sysfs.
 */
#include "zinitix_sec.c"

extern unsigned int lpcharge;

static int bt532_ts_probe(struct i2c_client *client,
		const struct i2c_device_id *i2c_id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bt532_ts_platform_data *pdata;
	struct bt532_ts_info *info;
	struct input_dev *input_dev;
	int ret = 0;
	int i;

	printk(KERN_INFO " %s\n", __func__);
	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "Not compatible i2c function\n");

		return -EIO;
	}

	if (lpcharge == 1) {
		dev_err(&client->dev, "%s : Do not load driver due to : lpm %d\n",
			 __func__, lpcharge);
		return -ENODEV;
	}

	pdata = client->dev.platform_data;
	if (!pdata) {
		dev_err(&client->dev, "Not exist platform data\n");
		return -EINVAL;
	}

	info = kzalloc(sizeof(struct bt532_ts_info), GFP_KERNEL);
	if (!info) {
		dev_err(&client->dev, "%s: Failed to allocate memory\n", __func__);

		return -ENOMEM;
	}

	i2c_set_clientdata(client, info);
	info->client = client;
	info->pdata = pdata;

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&client->dev, "Failed to allocate input device\n");
		ret = -ENOMEM;

		goto err_alloc;
	}

	info->input_dev = input_dev;

	if (bt532_power_control(info, POWER_ON_SEQUENCE) == false) {
		ret = -EPERM;

		goto err_power_sequence;
	}

	memset(&info->reported_touch_info, 0x0, sizeof(struct point_info));

	/* init touch mode */
	info->touch_mode = TOUCH_POINT_MODE;
	misc_info = info;

	if(init_touch(info, false) == false) {		
		printk(KERN_INFO " %s, %d - init_touch fail \n", __func__, __LINE__);
		ret = -EPERM;
		goto err_input_register_device;
	}

	for (i = 0; i < MAX_SUPPORTED_BUTTON_NUM; i++)
		info->button[i] = ICON_BUTTON_UNCHANGE;

	snprintf(info->phys, sizeof(info->phys),
		"%s/input0", dev_name(&client->dev));
	input_dev->name = "sec_touchscreen";
	input_dev->id.bustype = BUS_I2C;
/*	input_dev->id.vendor = 0x0001; */
	input_dev->phys = info->phys;
/*	input_dev->id.product = 0x0002; */
/*	input_dev->id.version = 0x0100; */

	set_bit(EV_SYN, info->input_dev->evbit);
	set_bit(EV_KEY, info->input_dev->evbit);
	set_bit(EV_ABS, info->input_dev->evbit);
	set_bit(INPUT_PROP_DIRECT, info->input_dev->propbit);
	set_bit(EV_LED, info->input_dev->evbit);
	set_bit(LED_MISC, info->input_dev->ledbit);

	for (i = 0; i < MAX_SUPPORTED_BUTTON_NUM; i++)
		set_bit(BUTTON_MAPPING_KEY[i], info->input_dev->keybit);

	if (pdata->orientation & TOUCH_XY_SWAP) {
		input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
			info->cap_info.MinX,
			info->cap_info.MaxX + ABS_PT_OFFSET,
			0, 0);
		input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
			info->cap_info.MinY,
			info->cap_info.MaxY + ABS_PT_OFFSET,
			0, 0);
	} else {
		input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
			info->cap_info.MinX,
			info->cap_info.MaxX + ABS_PT_OFFSET,
			0, 0);
		input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
			info->cap_info.MinY,
			info->cap_info.MaxY + ABS_PT_OFFSET,
			0, 0);
	}

	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR,
		0, 255, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_WIDTH_MAJOR,
		0, 255, 0, 0);

#if (TOUCH_POINT_MODE == 2)
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR,
		0, 255, 0, 0);
/*	input_set_abs_params(info->input_dev, ABS_MT_WIDTH_MINOR,
		0, 255, 0, 0); */
	input_set_abs_params(info->input_dev, ABS_MT_ORIENTATION,
		-128, 127, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_ANGLE,
		-90, 90, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_PALM,
		0, 1, 0, 0);
#endif

	set_bit(MT_TOOL_FINGER, info->input_dev->keybit);
	input_mt_init_slots(info->input_dev, info->cap_info.multi_fingers);

	input_set_drvdata(info->input_dev, info);
	ret = input_register_device(info->input_dev);
	if (ret) {
		dev_err(&client->dev, "Unable to register %s input device\n",
					info->input_dev->name);

		goto err_input_register_device;
	}

	/* configure irq */
	info->irq = gpio_to_irq(pdata->gpio_int);
	if (info->irq < 0)
		dev_err(&client->dev, "Invalid GPIO_TOUCH_IRQ\n");

	info->work_state = NOTHING;
	sema_init(&info->work_lock, 1);

#if ESD_TIMER_INTERVAL
	mutex_init(&info->prob_int_sync);
	esd_timer_start(CHECK_ESD_TIMER, info);
#endif

#ifdef TOUCH_BOOSTER_DVFS
	zinitix_init_dvfs(info);
#endif

	ret = request_threaded_irq(info->irq, NULL, bt532_touch_irq_handler,
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT , ZINITIX_NAME, info);

	if (ret) {
		dev_err(&client->dev, "Unable to register irq\n");

		goto err_request_irq;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	info->early_suspend.suspend = bt532_ts_early_suspend;
	info->early_suspend.resume = bt532_ts_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

#ifdef USE_OPEN_CLOSE
	input_dev->open = bt532_ts_open;
	input_dev->close = bt532_ts_close;
#endif

#if defined(CONFIG_PM_RUNTIME)
	pm_runtime_enable(&client->dev);
#endif

	sema_init(&info->raw_data_lock, 1);

	ret = misc_register(&touch_misc_device);
	if (ret) {
		dev_err(&client->dev, "Failed to register touch misc device\n");

		goto err_misc_register;
	}

#ifdef SEC_FACTORY_TEST
	init_sec_factory(info);
#endif

#if ESD_TIMER_INTERVAL	
	INIT_WORK(&info->tmr_work, ts_tmr_work);
	info->esd_tmr_workqueue =
		create_singlethread_workqueue("esd_tmr_workqueue");

	if (!info->esd_tmr_workqueue) {
		dev_err(&client->dev, "Failed to create esd tmr work queue\n");
		ret = -EPERM;

		goto err_kthread_create_failed;
	}
#endif

#if 0//def CONFIG_MACH_MEGA2ELTE_USA_ATT
	touch_leds.name = "button-backlight";
	touch_leds.brightness = LED_FULL;
	touch_leds.max_brightness = LED_FULL;
	touch_leds.brightness_set = new_touch_led_control;

	ret = led_classdev_register(&client->dev,&touch_leds);

	if(ret)
		printk(KERN_ERR "led_cdev register failed");
#endif	
	printk(KERN_INFO " %s, %d end \n", __func__, __LINE__);

	return 0;

#if ESD_TIMER_INTERVAL
err_kthread_create_failed:
	kfree(info->factory_info);
	kfree(info->raw_data);
#endif
err_misc_register:
	free_irq(info->irq, info);
err_request_irq:
	input_unregister_device(info->input_dev);
err_input_register_device:
	input_free_device(info->input_dev);
err_power_sequence:
err_alloc:
	kfree(info);

	return ret;
}

static int bt532_ts_remove(struct i2c_client *client)
{
	struct bt532_ts_info *info = i2c_get_clientdata(client);
	struct bt532_ts_platform_data *pdata = info->pdata;

	printk(KERN_INFO " %s, %d \n", __func__, __LINE__);

	disable_irq(info->irq);
	down(&info->work_lock);

	info->work_state = REMOVE;

#ifdef SEC_FACTORY_TEST
	exit_sec_factory(info);
#endif

#ifdef TOUCH_BOOSTER_DVFS
	mutex_destroy(&info->dvfs_lock);
#endif

#if ESD_TIMER_INTERVAL
	flush_work(&info->tmr_work);
	/*write_reg(info->client, BT532_PERIODICAL_INTERRUPT_INTERVAL, 0);*/
	esd_timer_stop(info);
	destroy_workqueue(info->esd_tmr_workqueue);
#endif

	if (info->irq)
		free_irq(info->irq, info);

	misc_deregister(&touch_misc_device);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif

	if (gpio_is_valid(pdata->gpio_int) != 0)
		gpio_free(pdata->gpio_int);

	input_unregister_device(info->input_dev);
	input_free_device(info->input_dev);
	up(&info->work_lock);
	kfree(info);

	return 0;
}

static struct i2c_device_id bt532_idtable[] = {
	{ZINITIX_NAME, 0},
	{ }
};

#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
static const struct dev_pm_ops bt532_ts_pm_ops = {
#if defined(CONFIG_PM_RUNTIME)
	SET_RUNTIME_PM_OPS(bt532_ts_suspend, bt532_ts_resume, NULL)
#else
	SET_SYSTEM_SLEEP_PM_OPS(bt532_ts_suspend, bt532_ts_resume)
#endif
};
#endif

static struct i2c_driver bt532_ts_driver = {
	.probe	= bt532_ts_probe,
	.remove	= bt532_ts_remove,
	.id_table	= bt532_idtable,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= ZINITIX_NAME,
#if defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND)
		.pm	= &bt532_ts_pm_ops,
#endif
	},
};

static int __devinit bt532_ts_init(void)
{
	printk(KERN_INFO " %s\n", __func__);
	return i2c_add_driver(&bt532_ts_driver);
}

static void __exit bt532_ts_exit(void)
{
	printk(KERN_INFO " %s\n", __func__);
	i2c_del_driver(&bt532_ts_driver);
}

module_init(bt532_ts_init);
module_exit(bt532_ts_exit);

MODULE_DESCRIPTION("touch-screen device driver using i2c interface");
MODULE_AUTHOR("<Zinitix>");
MODULE_LICENSE("GPL");
