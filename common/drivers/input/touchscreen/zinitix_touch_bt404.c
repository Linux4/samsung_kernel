/*
 *
 * Zinitix touch driver
 *
 * Copyright (C) 2009 Zinitix, Inc.
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
/*
Version 2.0.0 : using reg data file (2010/11/05)
Version 2.0.1 : syntxt bug fix (2010/11/09)
Version 2.0.2 : Save status cmd delay bug (2010/11/10)
Version 2.0.3 : modify delay 10ms -> 50ms for clear hw calibration bit
	: modify ZINITIX_TOTAL_NUMBER_OF_Y register (read only -> read/write )
	: modify SUPPORTED FINGER NUM register (read only -> read/write )
Version 2.0.4 : [20101116]
	Modify Firmware Upgrade routine.
Version 2.0.5 : [20101118]
	add esd timer function & some bug fix.
	you can select request_threaded_irq or
	request_irq, setting USE_THREADED_IRQ.
Version 2.0.6 : [20101123]
	add ABS_MT_WIDTH_MAJOR Report
Version 2.0.7 : [20101201]
	Modify zinitix_early_suspend() / zinitix_late_resume() routine.
Version 2.0.8 : [20101216]
	add using spin_lock option
Version 2.0.9 : [20101216]
	Test Version
Version 2.0.10 : [20101217]
	add USE_THREAD_METHOD option.
	if USE_THREAD_METHOD = 0, you use workqueue
Version 2.0.11 : [20101229]
	add USE_UPDATE_SYSFS option for update firmware.
	&& TOUCH_MODE == 1 mode.
Version 2.0.13 : [20110125]
	modify esd timer routine
Version 2.0.14 : [20110217]
	esd timer bug fix. (kernel panic)
	sysfs bug fix.
Version 2.0.15 : [20110315]
	add power off delay ,250ms
Version 2.0.16 : [20110316]
	add upgrade method using isp
Version 2.0.17 : [20110406]
	change naming rule : sain -> zinitix
	(add) pending interrupt skip
	add isp upgrade mode
	remove warning message when complile
Version 3.0.2 : [20110711]
	support bt4x3 series
Version 3.0.3 : [20110720]
	add raw data monitoring func.
	add the h/w calibration skip option.
Version 3.0.4 : [20110728]
	fix using semaphore bug.
Version 3.0.5 : [20110801]
	fix some bugs.
Version 3.0.6 : [20110802]
	fix Bt4x3 isp upgrade bug.
	add USE_TS_MISC_DEVICE option for showing info & upgrade
	remove USE_UPDATE_SYSFS option
Version 3.0.7 : [201108016]
	merge USE_TS_MISC_DEVICE option and USE_TEST_RAW_TH_DATA_MODE
	fix work proceedure bug.
Version 3.0.8 / Version 3.0.9 : [201108017]
	add ioctl func.
Version 3.0.10 : [201108030]
	support REAL_SUPPORTED_FINGER_NUM
Version 3.0.11 : [201109014]
	support zinitix apps.
Version 3.0.12 : [201109015]
	add disable touch event func.
Version 3.0.13 : [201109015]
	add USING_CHIP_SETTING option :  interrupt mask/button num/finger num
Version 3.0.14 : [201109020]
	support apps. above kernel version 2.6.35
Version 3.0.15 : [201101004]
	modify timing in firmware upgrade retry when failt to upgrade firmware
Version 3.0.16 : [20111024]
	remove thread method.
	add resume/suspend function
	check code using checkpatch.pl
Version 3.0.19 : [20111124]
	modify isp timing : 2ms -> 1ms
Version 3.0.20 : [20120201]
	support icecream sandwich
	remove bt4x2 option
Version 3.0.21~22 : [20120501]
	test
Version 3.0.23 : [20120517]
	support mulit touch protocol type B for ics
*/

#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/hrtimer.h>
#include <linux/ioctl.h>
#include <linux/earlysuspend.h>
#include <linux/string.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/timer.h>
#include <linux/workqueue.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/io.h>
#include <linux/delay.h>
#include <mach/gpio.h>
#include <linux/uaccess.h>

#include "zinitix_touch_bt404.h"
#include "zinitix_touch_bt4x4_firmware.h"


#define	ZINITIX_DEBUG		0

static int m_ts_debug_mode = ZINITIX_DEBUG;

#define	SYSTEM_MAX_X_RESOLUTION	480
#define	SYSTEM_MAX_Y_RESOLUTION	870


/* interrupt pin number*/
#define GPIO_TOUCH_PIN_NUM	141
/* interrupt pin IRQ number */
#define GPIO_TOUCH_IRQ	0

#if !USE_THREADED_IRQ
static struct workqueue_struct *zinitix_workqueue;
#endif

static struct workqueue_struct *zinitix_tmr_workqueue;

#define	zinitix_debug_msg(fmt, args...)	\
	if (m_ts_debug_mode)	\
		printk(KERN_INFO "zinitix : [%-18s:%5d]" fmt, \
		__func__, __LINE__, ## args);

#define	zinitix_printk(fmt, args...)	\
		printk(KERN_INFO "zinitix : [%-18s:%5d]" fmt, \
		__func__, __LINE__, ## args);

struct _ts_zinitix_coord {
	u16	x;
	u16	y;
	u8	width;
	u8	sub_status;
};

struct _ts_zinitix_point_info {
	u16	status;
#if (TOUCH_MODE == 1)
	u16	event_flag;
#else
	u8	finger_cnt;
	u8	time_stamp;
#endif	
	struct _ts_zinitix_coord coord[MAX_SUPPORTED_FINGER_NUM];
};

#define	TOUCH_V_FLIP	0x01
#define	TOUCH_H_FLIP	0x02
#define	TOUCH_XY_SWAP	0x04

struct _ts_capa_info {
	u16 chip_revision;
	u16 chip_firmware_version;
	u16 chip_reg_data_version;
	u16 x_resolution;
	u16 y_resolution;
	u32 chip_fw_size;
	u32 MaxX;
	u32 MaxY;
	u32 MinX;
	u32 MinY;
	u32 Orientation;
	u8 gesture_support;
	u16 multi_fingers;
	u16 button_num;
	u16 chip_int_mask;

	u16 x_node_num;
	u16 y_node_num;
	u16 total_node_num;
	u16 max_y_node;
	u16 total_cal_n;

};

enum _ts_work_proceedure {
	TS_NO_WORK = 0,
	TS_NORMAL_WORK,
	TS_ESD_TIMER_WORK,
	TS_IN_EALRY_SUSPEND,
	TS_IN_SUSPEND,
	TS_IN_RESUME,
	TS_IN_LATE_RESUME,
	TS_IN_UPGRADE,
	TS_REMOVE_WORK,
	TS_SET_MODE,
	TS_HW_CALIBRAION,
};

struct zinitix_touch_dev {
	struct input_dev *input_dev;
	struct task_struct *task;
	wait_queue_head_t	wait;
#if !USE_THREADED_IRQ	
	struct work_struct	work;
#endif
	struct work_struct	tmr_work;
	struct i2c_client *client;
	struct semaphore update_lock;
	u32 i2c_dev_addr;
	struct _ts_capa_info	cap_info;
	char	phys[32];

	bool is_valid_event;
	struct _ts_zinitix_point_info touch_info;
	struct _ts_zinitix_point_info reported_touch_info;
	u16 icon_event_reg;
	u16 event_type;
	u32 int_gpio_num;
	u32 irq;
	u8 button[MAX_SUPPORTED_BUTTON_NUM];
	u8 work_proceedure;
	struct semaphore work_proceedure_lock;
	u8	use_esd_timer;

	bool in_esd_timer;
	struct timer_list esd_timeout_tmr;
	struct timer_list *p_esd_timeout_tmr;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif


	struct semaphore	raw_data_lock;
	u16 raw_mode_flag;
	s16 ref_data[MAX_TEST_RAW_DATA];
	s16 cur_data[MAX_RAW_DATA];
	u8 update;

};


#define ZINITIX_DRIVER_NAME	"Zinitix_tsp"
#define ZINITIX_ISP_NAME	"zinitix_isp"
static struct i2c_client *m_isp_client;

static struct i2c_device_id zinitix_idtable[] = {
	{ZINITIX_ISP_NAME, 0},
	{ZINITIX_DRIVER_NAME, 0},
	{ }
};

#if SEC_TOUCH_INFO_REPORT
extern struct class *sec_class;
struct device *sec_touchscreen_dev;
struct device *sec_touchkey_dev;
#endif


/*<= you must set key button mapping*/
u32 BUTTON_MAPPING_KEY[MAX_SUPPORTED_BUTTON_NUM] = {
	KEY_SEARCH, KEY_BACK, KEY_HOME, KEY_MENU,};

#define  TOUCH_VIRTUAL_KEYS
#ifdef TOUCH_VIRTUAL_KEYS

static ssize_t virtual_keys_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf)
{
	return sprintf(buf,
	 __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":30:859:100:55"
	 ":" __stringify(EV_KEY) ":" __stringify(KEY_BACK) ":430:859:100:55"
	 "\n");
}

static struct kobj_attribute virtual_keys_attr = {
    .attr = {
        .name = "virtualkeys.Zinitix_tsp",
        .mode = S_IRUGO,
    },
    .show = &virtual_keys_show,
};

static struct attribute *properties_attrs[] = {
    &virtual_keys_attr.attr,
    NULL
};

static struct attribute_group properties_attr_group = {
    .attrs = properties_attrs,
};


/* zinitix_ts_virtual_keys_init --  register virutal keys to system
 * @return: none
 */
static void zinitix_ts_virtual_keys_init(void)
{
    int ret;
    struct kobject *properties_kobj;

    //PIXCIR_DBG("%s",__func__);

    properties_kobj = kobject_create_and_add("board_properties", NULL);
    if (properties_kobj)
        ret = sysfs_create_group(properties_kobj,
                     &properties_attr_group);
    if (!properties_kobj || ret)
        pr_err("failed to create board_properties\n");
}


#endif


/* define i2c sub functions*/
static inline s32 ts_read_data(struct i2c_client *client,
	u16 reg, u8 *values, u16 length)
{
	s32 ret;
	/* select register*/
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0)
		return ret;
	/* for setup tx transaction. */
	udelay(DELAY_FOR_TRANSCATION);
	ret = i2c_master_recv(client , values , length);
	if (ret < 0)
		return ret;
	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

static inline s32 ts_write_data(struct i2c_client *client,
	u16 reg, u8 *values, u16 length)
{
	s32 ret;
	u8	pkt[4];
	pkt[0] = (reg)&0xff;
	pkt[1] = (reg >> 8)&0xff;
	pkt[2] = values[0];
	pkt[3] = values[1];
	ret = i2c_master_send(client , pkt , length+2);
	if (ret < 0)
		return ret;
	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

static inline s32 ts_write_reg(struct i2c_client *client, u16 reg, u16 value)
{
	if (ts_write_data(client, reg, (u8 *)&value, 2) < 0)
		return I2C_FAIL;
	return I2C_SUCCESS;
}

static inline s32 ts_write_cmd(struct i2c_client *client, u16 reg)
{
	s32 ret;
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0)
		return I2C_FAIL;
	udelay(DELAY_FOR_POST_TRANSCATION);
	return I2C_SUCCESS;
}

static inline s32 ts_read_raw_data(struct i2c_client *client,
		u16 reg, u8 *values, u16 length)
{
	s32 ret;
	/* select register */
	ret = i2c_master_send(client , (u8 *)&reg , 2);
	if (ret < 0)
		return ret;
	/* for setup tx transaction. */
	udelay(200);
	ret = i2c_master_recv(client , values , length);
	if (ret < 0)
		return ret;
	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}



static inline s32 ts_read_firmware_data(struct i2c_client *client,
	char *addr, u8 *values, u16 length)
{
	s32 ret;
	if (addr != NULL) {
		/* select register*/
		ret = i2c_master_send(client , addr , 2);
		if (ret < 0)
			return ret;
		/* for setup tx transaction. */
		mdelay(1);
	}
	ret = i2c_master_recv(client , values , length);
	if (ret < 0)
		return ret;
	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

static inline s32 ts_write_firmware_data(struct i2c_client *client,
	u8 *values, u16 length)
{
	s32 ret;
	ret = i2c_master_send(client , values , length);
	if (ret < 0)
		return ret;
	udelay(DELAY_FOR_POST_TRANSCATION);
	return length;
}

static int zinitix_touch_probe(struct i2c_client *client,
	const struct i2c_device_id *i2c_id);
static int zinitix_touch_remove(struct i2c_client *client);
static bool ts_init_touch(struct zinitix_touch_dev *touch_dev);
static void zinitix_clear_report_data(struct zinitix_touch_dev *touch_dev);
static void zinitix_parsing_data(struct zinitix_touch_dev *touch_dev);

#ifdef CONFIG_PM_SLEEP
static int zinitix_resume(struct device *dev);
static int zinitix_suspend(struct device *dev);
#endif


#ifdef CONFIG_HAS_EARLYSUSPEND
static void zinitix_early_suspend(struct early_suspend *h);
static void zinitix_late_resume(struct early_suspend *h);
#endif


static void ts_esd_timer_start(u16 sec, struct zinitix_touch_dev *touch_dev);
static void ts_esd_timer_stop(struct zinitix_touch_dev *touch_dev);
static void ts_esd_timer_init(struct zinitix_touch_dev *touch_dev);
static void ts_esd_timeout_handler(unsigned long data);



#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
static int ts_misc_fops_ioctl(struct inode *inode, struct file *filp,
	unsigned int cmd, unsigned long arg);
#else
static long ts_misc_fops_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg);
#endif
static int ts_misc_fops_open(struct inode *inode, struct file *filp);
static int ts_misc_fops_close(struct inode *inode, struct file *filp);

static const struct file_operations ts_misc_fops = {
	.owner = THIS_MODULE,
	.open = ts_misc_fops_open,
	.release = ts_misc_fops_close,
#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
	.ioctl = ts_misc_fops_ioctl,
#else
	.unlocked_ioctl = ts_misc_fops_ioctl,
#endif
};

static struct miscdevice touch_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "zinitix_touch_misc",
	.fops = &ts_misc_fops,
};

#define TOUCH_IOCTL_BASE	0xbc
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


struct zinitix_touch_dev *misc_touch_dev;





//for  Debugging Tol
static bool ts_get_raw_data(struct zinitix_touch_dev *touch_dev)
{
	u32 total_node = touch_dev->cap_info.total_node_num;
	u32 sz;
	u16 udata;
	down(&touch_dev->raw_data_lock);
	if (touch_dev->raw_mode_flag == TOUCH_TEST_RAW_MODE) {
		sz = (total_node*2 + MAX_TEST_POINT_INFO*2);


		if (ts_read_raw_data(touch_dev->client,
			ZINITIX_RAWDATA_REG,
			(char *)touch_dev->cur_data, sz) < 0) {
				zinitix_printk("error : read zinitix tc raw data\n");
				up(&touch_dev->raw_data_lock);
				return false;
		}

		/* no point, so update ref_data*/
		udata = touch_dev->cur_data[total_node];

		if (!zinitix_bit_test(udata, BIT_ICON_EVENT)
			&& !zinitix_bit_test(udata, BIT_PT_EXIST))
			memcpy((u8 *)touch_dev->ref_data,
				(u8 *)touch_dev->cur_data, total_node*2);

		touch_dev->update = 1;
		memcpy((u8 *)(&touch_dev->touch_info),
			(u8 *)&touch_dev->cur_data[total_node],
			sizeof(struct _ts_zinitix_point_info));
		up(&touch_dev->raw_data_lock);
		return true;
	}  else if (touch_dev->raw_mode_flag != TOUCH_NORMAL_MODE) {
		zinitix_debug_msg("read raw data\r\n");
		sz = total_node*2 + sizeof(struct _ts_zinitix_point_info);


		if (touch_dev->raw_mode_flag == TOUCH_ZINITIX_CAL_N_MODE) {
			int total_cal_n = touch_dev->cap_info.total_cal_n;
			if (total_cal_n == 0)
				total_cal_n = 160;

			sz =
			(total_cal_n*2 + sizeof(struct _ts_zinitix_point_info));

			if (ts_read_raw_data(touch_dev->client,
				ZINITIX_RAWDATA_REG,
				(char *)touch_dev->cur_data, sz) < 0) {
				zinitix_printk("error : read zinitix tc raw data\n");
				up(&touch_dev->raw_data_lock);
				return false;
			}
			misc_touch_dev->update = 1;
			memcpy((u8 *)(&touch_dev->touch_info),
				(u8 *)&touch_dev->cur_data[total_cal_n],
				sizeof(struct _ts_zinitix_point_info));
			up(&touch_dev->raw_data_lock);
			return true;
		}

		if (ts_read_raw_data(touch_dev->client,
			ZINITIX_RAWDATA_REG,
			(char *)touch_dev->cur_data, sz) < 0) {
			zinitix_printk("error : read zinitix tc raw data\n");
			up(&touch_dev->raw_data_lock);
			return false;
		}

		udata = touch_dev->cur_data[total_node];
		if (!zinitix_bit_test(udata, BIT_ICON_EVENT)
			&& !zinitix_bit_test(udata, BIT_PT_EXIST))
			memcpy((u8 *)touch_dev->ref_data,
				(u8 *)touch_dev->cur_data, total_node*2);
		touch_dev->update = 1;
		memcpy((u8 *)(&touch_dev->touch_info),
			(u8 *)&touch_dev->cur_data[total_node],
			sizeof(struct _ts_zinitix_point_info));
	}
	up(&touch_dev->raw_data_lock);
	return true;
}



static bool ts_read_coord(struct zinitix_touch_dev *touch_dev)
{
#if (TOUCH_MODE == 1)
	int i;
#endif

	zinitix_debug_msg("ts_read_coord+\r\n");

	if (gpio_get_value(touch_dev->int_gpio_num)) {
		/*interrupt pin is high, not valid data.*/
		touch_dev->touch_info.status = 0;
		zinitix_debug_msg("woops... inturrpt pin is high\r\n");
		return true;
	}

	//for  Debugging Tool

	if (touch_dev->raw_mode_flag != TOUCH_NORMAL_MODE) {
		if (ts_get_raw_data(touch_dev) == false)
			return false;
		goto continue_check_point_data;
	}



#if (TOUCH_MODE == 1)
	memset(&touch_dev->touch_info,
		0x0, sizeof(struct _ts_zinitix_point_info));

	if (ts_read_data(touch_dev->client,
		ZINITIX_POINT_STATUS_REG,
		(u8 *)(&touch_dev->touch_info), 4) < 0) {
		zinitix_debug_msg("error read point info using i2c.-\r\n");
		return false;
	}
	zinitix_debug_msg("status reg = 0x%x , event_flag = 0x%04x\r\n",
		touch_dev->touch_info.status, touch_dev->touch_info.event_flag);

	if (touch_dev->touch_info.status == 0x0) {
		zinitix_debug_msg("periodical esd repeated int occured\r\n");
		ts_write_cmd(touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
		udelay(DELAY_FOR_SIGNAL_DELAY);		
		return true;
	}

	if (zinitix_bit_test(touch_dev->touch_info.status, BIT_ICON_EVENT)) {
		udelay(20);
		if (ts_read_data(touch_dev->client,
			ZINITIX_ICON_STATUS_REG,
			(u8 *)(&touch_dev->icon_event_reg), 2) < 0) {
			printk(KERN_INFO "error read icon info using i2c.\n");
			return false;
		}
		ts_write_cmd(touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
		udelay(DELAY_FOR_SIGNAL_DELAY);				
		return true;
	}



	if(touch_dev->touch_info.event_flag == 0) {
		zinitix_printk( "error : event flag == 0?\n");
		ts_write_cmd(touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
		udelay(DELAY_FOR_SIGNAL_DELAY);		
		return true;
	}
		
	for (i = 0; i < touch_dev->cap_info.multi_fingers; i++) {
		if (zinitix_bit_test(touch_dev->touch_info.event_flag, i)) {
			udelay(20);
			if (ts_read_data(touch_dev->client,
				ZINITIX_POINT_STATUS_REG+2+(i*3),
				(u8 *)(&touch_dev->touch_info.coord[i]),
				sizeof(struct _ts_zinitix_coord)) < 0) {
				zinitix_debug_msg("error read point info\n");
				return false;
			}			
		}
	}

	ts_write_cmd(touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
	udelay(DELAY_FOR_SIGNAL_DELAY);		
	return true;
#else
	if (ts_read_data(touch_dev->client,
		ZINITIX_POINT_STATUS_REG,
		(u8 *)(&touch_dev->touch_info),
		sizeof(struct _ts_zinitix_point_info)) < 0) {
		zinitix_debug_msg("error read point info using i2c.-\n");
		return false;
	}
	
continue_check_point_data:
	zinitix_debug_msg("status = 0x%x , pt cnt = %d, t stamp = %d\n",
		touch_dev->touch_info.status,
		touch_dev->touch_info.finger_cnt,
		touch_dev->touch_info.time_stamp);

	if (touch_dev->touch_info.status == 0x0) {
		zinitix_debug_msg("periodical esd repeated int occured\r\n");
		ts_write_cmd(touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
		udelay(DELAY_FOR_SIGNAL_DELAY);		
		return true;
	}

	if (zinitix_bit_test(touch_dev->touch_info.status, BIT_ICON_EVENT)) {
		udelay(20);
		if (ts_read_data(touch_dev->client,
			ZINITIX_ICON_STATUS_REG,
			(u8 *)(&touch_dev->icon_event_reg), 2) < 0) {
			zinitix_printk("error read icon info using i2c.\n");
			return false;
		}
	}
	
	ts_write_cmd(touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
	udelay(DELAY_FOR_SIGNAL_DELAY);
	zinitix_debug_msg("ts_read_coord-\r\n");	
	return true;
#endif	
}


static irqreturn_t ts_int_handler(int irq, void *dev)
{

	struct zinitix_touch_dev *touch_dev = (struct zinitix_touch_dev *)dev;
	/* zinitix_debug_msg("interrupt occured +\r\n"); */
	if(touch_dev == NULL)	return IRQ_HANDLED;
	/* remove pending interrupt */
	if (gpio_get_value(touch_dev->int_gpio_num)) {
		zinitix_debug_msg("invalid interrupt occured +\r\n");
		return IRQ_HANDLED;
	}
#if USE_THREADED_IRQ	
	return IRQ_WAKE_THREAD;
#else	
	disable_irq_nosync(irq);
	queue_work(zinitix_workqueue, &touch_dev->work);
	return IRQ_HANDLED;
#endif	
}



static void ts_power_control(u8 ctl)
{
	if (ctl == POWER_OFF)
		;
	else if (ctl == POWER_ON)
		;

}

static bool ts_mini_init_touch(struct zinitix_touch_dev *touch_dev)
{
	u16 reg_val;
	int i;
#if USE_CHECKSUM
	u16 chip_eeprom_info;
	u16 chip_check_reg[4];
#endif	

	if (touch_dev == NULL) {
		zinitix_printk("error (touch_dev == NULL?)\r\n");
		return false;
	}

	zinitix_debug_msg("check checksum\r\n");

#if USE_CHECKSUM
	if (ts_read_data(touch_dev->client,
			ZINITIX_EEPROM_INFO_REG,
			(u8 *)&chip_eeprom_info, 2) < 0) {
		zinitix_printk("fail to read eeprom info(%d)\r\n", i);
		goto fail_mini_init;
	}
			
	if (zinitix_bit_test(chip_eeprom_info, 0)) // not calibration
		goto fail_mini_init_not_cal;			

	if (ts_read_data(touch_dev->client,
		ZINITIX_CHECK_REG0, (u8 *)chip_check_reg, 8) < 0) 
		goto fail_mini_init;
	if( (chip_check_reg[0]!=chip_check_reg[2]) || (chip_check_reg[1]!=chip_check_reg[3]) ) {
		zinitix_printk("firmware checksum error\r\n");
		goto fail_mini_init;
	}		
#endif

	if (ts_write_cmd(touch_dev->client,
		ZINITIX_SWRESET_CMD) != I2C_SUCCESS) {
		zinitix_printk("fail to write reset command\r\n");
		goto fail_mini_init;
	}


	/* initialize */
	if (ts_write_reg(touch_dev->client,
		ZINITIX_X_RESOLUTION,
		(u16)(touch_dev->cap_info.x_resolution)) != I2C_SUCCESS)
		goto fail_mini_init;

	if (ts_write_reg(touch_dev->client,
		ZINITIX_Y_RESOLUTION,
		(u16)( touch_dev->cap_info.y_resolution)) != I2C_SUCCESS)
		goto fail_mini_init;

	zinitix_debug_msg("touch max x = %d\r\n", touch_dev->cap_info.x_resolution);
	zinitix_debug_msg("touch max y = %d\r\n", touch_dev->cap_info.y_resolution);

	if (ts_write_reg(touch_dev->client,
		ZINITIX_BUTTON_SUPPORTED_NUM,
		(u16)touch_dev->cap_info.button_num) != I2C_SUCCESS)
		goto fail_mini_init;

	if (ts_write_reg(touch_dev->client,
		ZINITIX_SUPPORTED_FINGER_NUM,
		(u16)MAX_SUPPORTED_FINGER_NUM) != I2C_SUCCESS)
		goto fail_mini_init;


	if (touch_dev->raw_mode_flag != TOUCH_NORMAL_MODE) {	/* Test Mode */
		if (ts_write_reg(touch_dev->client,
			ZINITIX_TOUCH_MODE,
			touch_dev->raw_mode_flag) != I2C_SUCCESS)
			goto fail_mini_init;
	} else

	{
		reg_val = TOUCH_MODE;
		if (ts_write_reg(touch_dev->client,
			ZINITIX_TOUCH_MODE,
			reg_val) != I2C_SUCCESS)
			goto fail_mini_init;
	}
	/* soft calibration */
	if (ts_write_cmd(touch_dev->client,
		ZINITIX_CALIBRATE_CMD) != I2C_SUCCESS)
		goto fail_mini_init;
	if (ts_write_reg(touch_dev->client,
		ZINITIX_INT_ENABLE_FLAG,
		touch_dev->cap_info.chip_int_mask) != I2C_SUCCESS)
		goto fail_mini_init;

	/* read garbage data */
	for (i = 0; i < 10; i++)	{
		ts_write_cmd(touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
		udelay(10);
	}


	if (touch_dev->raw_mode_flag != TOUCH_NORMAL_MODE) { /* Test Mode */
		if (ts_write_reg(touch_dev->client,
			ZINITIX_PERIODICAL_INTERRUPT_INTERVAL,
			ZINITIX_SCAN_RATE_HZ
			*ZINITIX_RAW_DATA_ESD_TIMER_INTERVAL) != I2C_SUCCESS)
			zinitix_printk("[zinitix_touch] Fail to set ZINITIX_RAW_DATA_ESD_TIMER_INTERVAL.\r\n");
	} else
	{
		if (ts_write_reg(touch_dev->client,
			ZINITIX_PERIODICAL_INTERRUPT_INTERVAL,
			ZINITIX_SCAN_RATE_HZ
			*ZINITIX_ESD_TIMER_INTERVAL) != I2C_SUCCESS)
			goto fail_mini_init;

	}
	if (touch_dev->use_esd_timer) {
		ts_esd_timer_start(ZINITIX_CHECK_ESD_TIMER, touch_dev);
		zinitix_debug_msg("esd timer start\r\n");
	}

	zinitix_debug_msg("successfully mini initialized\r\n");
	return true;

fail_mini_init:

	zinitix_printk("early mini init fail\n");
	ts_power_control(POWER_OFF);
	mdelay(CHIP_POWER_OFF_DELAY);
	ts_power_control(POWER_ON);
	mdelay(CHIP_ON_DELAY);
fail_mini_init_not_cal:	
	if(ts_init_touch(touch_dev) == false) {
		zinitix_debug_msg("fail initialized\r\n");
		return false;
	}

	if (touch_dev->use_esd_timer) {
		ts_esd_timer_start(ZINITIX_CHECK_ESD_TIMER, touch_dev);
		zinitix_debug_msg("esd timer start\r\n");
	}
	return true;
}


static void zinitix_touch_tmr_work(struct work_struct *work)
{
	struct zinitix_touch_dev *touch_dev =
		container_of(work, struct zinitix_touch_dev, tmr_work);

	zinitix_printk("tmr queue work ++\r\n");
	if (touch_dev == NULL) {
		zinitix_printk("touch dev == NULL ?\r\n");
		goto fail_time_out_init;
	}
	down(&touch_dev->work_proceedure_lock);
	if (touch_dev->work_proceedure != TS_NO_WORK) {
		zinitix_printk("other process occupied (%d)\r\n",
			touch_dev->work_proceedure);
		up(&touch_dev->work_proceedure_lock);
		return;
	}
	touch_dev->work_proceedure = TS_ESD_TIMER_WORK;

	disable_irq(touch_dev->irq);
	zinitix_printk("error. timeout occured. maybe ts device dead. so reset & reinit.\r\n");
	ts_power_control(POWER_OFF);
	mdelay(CHIP_POWER_OFF_DELAY);
	ts_power_control(POWER_ON);
	mdelay(CHIP_ON_DELAY);

	zinitix_debug_msg("clear all reported points\r\n");
	zinitix_clear_report_data(touch_dev);
	if (ts_mini_init_touch(touch_dev) == false)
		goto fail_time_out_init;

	touch_dev->work_proceedure = TS_NO_WORK;
	enable_irq(touch_dev->irq);
	up(&touch_dev->work_proceedure_lock);
	zinitix_printk("tmr queue work ----\r\n");
	return;
fail_time_out_init:
	zinitix_printk("tmr work : restart error\r\n");
	ts_esd_timer_start(ZINITIX_CHECK_ESD_TIMER, touch_dev);
	touch_dev->work_proceedure = TS_NO_WORK;
	enable_irq(touch_dev->irq);
	up(&touch_dev->work_proceedure_lock);
}

static void ts_esd_timer_start(u16 sec, struct zinitix_touch_dev *touch_dev)
{
	if(touch_dev == NULL)	return;
	if (touch_dev->p_esd_timeout_tmr != NULL)
		del_timer(touch_dev->p_esd_timeout_tmr);
	touch_dev->p_esd_timeout_tmr = NULL;

	init_timer(&(touch_dev->esd_timeout_tmr));
	touch_dev->esd_timeout_tmr.data = (unsigned long)(touch_dev);
	touch_dev->esd_timeout_tmr.function = ts_esd_timeout_handler;
	touch_dev->esd_timeout_tmr.expires = jiffies + HZ*sec;
	touch_dev->p_esd_timeout_tmr = &touch_dev->esd_timeout_tmr;
	add_timer(&touch_dev->esd_timeout_tmr);
}

static void ts_esd_timer_stop(struct zinitix_touch_dev *touch_dev)
{
	if(touch_dev == NULL)	return;
	if (touch_dev->p_esd_timeout_tmr)
		del_timer(touch_dev->p_esd_timeout_tmr);
	touch_dev->p_esd_timeout_tmr = NULL;
}

static void ts_esd_timer_init(struct zinitix_touch_dev *touch_dev)
{
	if(touch_dev == NULL)	return;
	init_timer(&(touch_dev->esd_timeout_tmr));
	touch_dev->esd_timeout_tmr.data = (unsigned long)(touch_dev);
	touch_dev->esd_timeout_tmr.function = ts_esd_timeout_handler;
	touch_dev->p_esd_timeout_tmr = NULL;
}

static void ts_esd_timeout_handler(unsigned long data)
{
	struct zinitix_touch_dev *touch_dev = (struct zinitix_touch_dev *)data;
	if(touch_dev == NULL)	return;
	touch_dev->p_esd_timeout_tmr = NULL;
	queue_work(zinitix_tmr_workqueue, &touch_dev->tmr_work);
}

#if TOUCH_ONESHOT_UPGRADE
static bool ts_check_need_upgrade(u16 curVersion, u16 curRegVersion)
{
	u16	newVersion;
	u16	newRegVersion;
	newVersion = (u16) (m_firmware_data[0] | (m_firmware_data[1]<<8));

	zinitix_printk("cur version = 0x%x, new version = 0x%x\n",
		curVersion, newVersion);

	if (curVersion < newVersion)
		return true;
	else if (curVersion > newVersion)
		return false;

	if (m_firmware_data[FIRMWARE_VERSION_POS+2] == 0xff
		&& m_firmware_data[FIRMWARE_VERSION_POS+3] == 0xff)
		return false;
	newRegVersion = (u16) (m_firmware_data[FIRMWARE_VERSION_POS+2]
		| (m_firmware_data[FIRMWARE_VERSION_POS+3]<<8));


	if (curRegVersion < newRegVersion)
		return true;

	return false;
}
#endif

#define	TC_PAGE_SZ		64
#define	TC_SECTOR_SZ		8

static u8 ts_upgrade_firmware(struct zinitix_touch_dev *touch_dev,
	const u8 *firmware_data, u32 size)
{
	u16 flash_addr;
	u8 *verify_data;
	int retry_cnt = 0;
	u8 i2c_buffer[TC_PAGE_SZ+2];

	if (m_isp_client == NULL) {
		zinitix_printk("i2c client for isp is not register \r\n");
		return false;
	}

	verify_data = kzalloc(size, GFP_KERNEL);
	if (verify_data == NULL) {
		zinitix_printk("cannot alloc verify buffer\n");
		return false;
	}

retry_isp_firmware_upgrade:
	ts_power_control(POWER_OFF);
	mdelay(CHIP_POWER_OFF_AF_FZ_DELAY);
	ts_power_control(POWER_ON);
	mdelay(1);		/* under 4ms*/

	zinitix_printk("write firmware data\n");

	for (flash_addr = 0; flash_addr < size; flash_addr += TC_PAGE_SZ) {

		printk(KERN_INFO ".");
		zinitix_debug_msg("firmware write : addr = %04x, len = %d\n",
			flash_addr, TC_PAGE_SZ);

		i2c_buffer[0] = (flash_addr>>8)&0xff;	/*addr_h*/
		i2c_buffer[1] = (flash_addr)&0xff;	/*addr_l*/
		memcpy(&i2c_buffer[2], &firmware_data[flash_addr], TC_PAGE_SZ);

		if (ts_write_firmware_data(m_isp_client,
			i2c_buffer, TC_PAGE_SZ+2) < 0) {
			zinitix_printk("error : write zinitix tc firmare\n");
			goto fail_upgrade;
		}
		mdelay(20);
	}
	mdelay(CHIP_POWER_OFF_AF_FZ_DELAY);

	zinitix_printk("\r\nread firmware data\n");


	flash_addr = 0;
	i2c_buffer[0] = (flash_addr>>8)&0xff;	/*addr_h*/
	i2c_buffer[1] = (flash_addr)&0xff;	/*addr_l*/

	if (ts_read_firmware_data(m_isp_client,
		i2c_buffer, &verify_data[flash_addr], size) < 0) {
		zinitix_printk("error : read zinitix tc firmare: addr = %04x, len = %d\n",
			flash_addr, size);
		goto fail_upgrade;
	}
	if (memcmp((u8 *)&firmware_data[flash_addr],
		(u8 *)&verify_data[flash_addr], size) != 0) {
		zinitix_printk("error : verify error : addr = %04x, len = %d\n",
			flash_addr, size);
		goto fail_upgrade;
	}

	mdelay(CHIP_POWER_OFF_AF_FZ_DELAY);
	ts_power_control(POWER_OFF);
	mdelay(CHIP_POWER_OFF_AF_FZ_DELAY);
	ts_power_control(POWER_ON);
	mdelay(CHIP_ON_AF_FZ_DELAY);
	zinitix_printk("upgrade finished\n");
	kfree(verify_data);
	return true;

fail_upgrade:

	mdelay(CHIP_POWER_OFF_AF_FZ_DELAY);
	ts_power_control(POWER_OFF);
	mdelay(CHIP_POWER_OFF_AF_FZ_DELAY);
	ts_power_control(POWER_ON);
	mdelay(CHIP_ON_AF_FZ_DELAY);

	zinitix_printk("upgrade fail : so retry... (%d)\n", ++retry_cnt);
	if (retry_cnt <= ZINITIX_INIT_RETRY_CNT)
		goto retry_isp_firmware_upgrade;

	if (verify_data != NULL)
		kfree(verify_data);

	zinitix_printk("upgrade fail..\n");
	return false;

}

static bool ts_hw_calibration(struct zinitix_touch_dev *touch_dev)
{
	u16	chip_eeprom_info;
	 /* h/w calibration */
	if (ts_write_reg(touch_dev->client,
		ZINITIX_TOUCH_MODE, 0x07) != I2C_SUCCESS)
		return false;
	mdelay(250);
	if (ts_write_cmd(touch_dev->client,
		ZINITIX_CALIBRATE_CMD) != I2C_SUCCESS)
		return false;
	if (ts_write_cmd(touch_dev->client,
		ZINITIX_SWRESET_CMD) != I2C_SUCCESS)
		return false;
	mdelay(10);
	ts_write_cmd(touch_dev->client,	ZINITIX_CLEAR_INT_STATUS_CMD);
		
	/* wait for h/w calibration*/
	do {
		mdelay(1000);
		ts_write_cmd(touch_dev->client,
				ZINITIX_CLEAR_INT_STATUS_CMD);			
		if (ts_read_data(touch_dev->client,
			ZINITIX_EEPROM_INFO_REG,
			(u8 *)&chip_eeprom_info, 2) < 0)
			return false;
		zinitix_debug_msg("touch eeprom info = 0x%04X\r\n",
			chip_eeprom_info);
		if (!zinitix_bit_test(chip_eeprom_info, 0))
			break;
	} while (1);
	
	if (ts_write_reg(touch_dev->client,
		ZINITIX_TOUCH_MODE, TOUCH_MODE) != I2C_SUCCESS)
		return false;

	if (touch_dev->cap_info.chip_int_mask != 0)
		if (ts_write_reg(touch_dev->client,
			ZINITIX_INT_ENABLE_FLAG,
			touch_dev->cap_info.chip_int_mask)
			!= I2C_SUCCESS)
			return false;	
	if (ts_write_cmd(touch_dev->client,
		ZINITIX_SAVE_CALIBRATION_CMD) != I2C_SUCCESS)
		return false;
	mdelay(1000);	
	return true;				

}

static bool ts_init_touch(struct zinitix_touch_dev *touch_dev)
{
	u16 reg_val;
	int i;
	u16 SetMaxX = SYSTEM_MAX_X_RESOLUTION;
	u16 SetMaxY = SYSTEM_MAX_Y_RESOLUTION;
	u16 chip_revision;
	u16 chip_firmware_version;
	u16 chip_reg_data_version;
	u16 chip_eeprom_info;
#if USE_CHECKSUM	
	u16 chip_check_reg[4];
	u8 checksum_err;
#endif	
#if TOUCH_ONESHOT_UPGRADE
	s16 stmp;
#endif
	int retry_cnt = 0;

	if (touch_dev == NULL) {
		zinitix_printk("error touch_dev == null?\r\n");
		return false;
	}

retry_init:

#if USE_CHECKSUM
	for(i = 0; i < ZINITIX_INIT_RETRY_CNT; i++) {		
		if (ts_read_data(touch_dev->client,
				ZINITIX_EEPROM_INFO_REG,
				(u8 *)&chip_eeprom_info, 2) < 0) {
			zinitix_printk("fail to read eeprom info(%d)\r\n", i);
			mdelay(10);
			continue;
		} else
			break;
	}
	if (i == ZINITIX_INIT_RETRY_CNT) {		
		goto fail_init;
	}
		
	if (!zinitix_bit_test(chip_eeprom_info, 0)) {	// no exist checksum data
			
		zinitix_debug_msg("check checksum\r\n");			

		checksum_err = 0;

		for (i = 0; i < ZINITIX_INIT_RETRY_CNT; i++) {
			if (ts_read_data(touch_dev->client,
				ZINITIX_CHECK_REG0, (u8 *)chip_check_reg, 8) < 0) {
				mdelay(10);
				continue;
			}

			zinitix_debug_msg("0x%02X, 0x%02X, 0x%02X, 0x%02X\r\n",
				chip_check_reg[0], chip_check_reg[1], chip_check_reg[2], chip_check_reg[3]);

			if( (chip_check_reg[0]!=chip_check_reg[2]) &&
				(chip_check_reg[1]==chip_check_reg[3]) )				
				break;
			else {
				checksum_err = 1;
				break;
			}
		}
		
		if (i == ZINITIX_INIT_RETRY_CNT || checksum_err) {
			zinitix_printk("fail to check firmware data\r\n");
			if(checksum_err == 1 && retry_cnt < ZINITIX_INIT_RETRY_CNT)
				retry_cnt = ZINITIX_INIT_RETRY_CNT;
			goto fail_init;
		}
	} else
		zinitix_printk("not calibration, so skip checksum.\r\n");
#endif

	if (ts_write_cmd(touch_dev->client,
		ZINITIX_SWRESET_CMD) != I2C_SUCCESS) {
		zinitix_printk("fail to write reset command\r\n");
		goto fail_init;
	}

	touch_dev->cap_info.button_num = SUPPORTED_BUTTON_NUM;

	reg_val = 0;
	zinitix_bit_set(reg_val, BIT_PT_CNT_CHANGE);
	zinitix_bit_set(reg_val, BIT_DOWN);
	zinitix_bit_set(reg_val, BIT_MOVE);
	zinitix_bit_set(reg_val, BIT_UP);
	if (touch_dev->cap_info.button_num > 0)
		zinitix_bit_set(reg_val, BIT_ICON_EVENT);
	touch_dev->cap_info.chip_int_mask = reg_val;


	if (ts_write_reg(touch_dev->client,
		ZINITIX_INT_ENABLE_FLAG, 0x0) != I2C_SUCCESS)
		goto fail_init;

	zinitix_debug_msg("send reset command\r\n");
	if (ts_write_cmd(touch_dev->client,
		ZINITIX_SWRESET_CMD) != I2C_SUCCESS)
		goto fail_init;

	/* get chip revision id */
	if (ts_read_data(touch_dev->client,
		ZINITIX_CHIP_REVISION,
		(u8 *)&chip_revision, 2) < 0) {
		zinitix_printk("fail to read chip revision\r\n");
		goto fail_init;
	}
	zinitix_printk("touch chip revision id = %x\r\n",
		chip_revision);

	touch_dev->cap_info.chip_fw_size = 32*1024;

	//for  Debugging Tool

	if (ts_read_data(touch_dev->client,
		ZINITIX_TOTAL_NUMBER_OF_X,
		(u8 *)&touch_dev->cap_info.x_node_num, 2) < 0)
		goto fail_init;
	zinitix_printk("touch chip x node num = %d\r\n",
		touch_dev->cap_info.x_node_num);
	if (ts_read_data(touch_dev->client,
		ZINITIX_TOTAL_NUMBER_OF_Y,
		(u8 *)&touch_dev->cap_info.y_node_num, 2) < 0)
		goto fail_init;
	zinitix_printk("touch chip y node num = %d\r\n",
		touch_dev->cap_info.y_node_num);

	touch_dev->cap_info.total_node_num =
		touch_dev->cap_info.x_node_num*touch_dev->cap_info.y_node_num;
	zinitix_printk("touch chip total node num = %d\r\n",
		touch_dev->cap_info.total_node_num);

	if (ts_read_data(touch_dev->client,
		ZINITIX_MAX_Y_NUM,
		(u8 *)&touch_dev->cap_info.max_y_node, 2) < 0)
		goto fail_init;

	zinitix_printk("touch chip max y node num = %d\r\n",
		touch_dev->cap_info.max_y_node);

	if (ts_read_data(touch_dev->client,
		ZINITIX_CAL_N_TOTAL_NUM,
		(u8 *)&touch_dev->cap_info.total_cal_n, 2) < 0)
		goto fail_init;
	zinitix_printk("touch chip total cal n data num = %d\r\n",
		touch_dev->cap_info.total_cal_n);
	//----


	/* get chip firmware version */
	if (ts_read_data(touch_dev->client,
		ZINITIX_FIRMWARE_VERSION,
		(u8 *)&chip_firmware_version, 2) < 0)
		goto fail_init;
	zinitix_printk("touch chip firmware version = %x\r\n",
		chip_firmware_version);

#if TOUCH_ONESHOT_UPGRADE
	chip_reg_data_version = 0xffff;

	if (ts_read_data(touch_dev->client,
		ZINITIX_DATA_VERSION_REG,
		(u8 *)&chip_reg_data_version, 2) < 0)
		goto fail_init;
	zinitix_debug_msg("touch reg data version = %d\r\n",
		chip_reg_data_version);

	if (ts_check_need_upgrade(chip_firmware_version,
		chip_reg_data_version) == true) {
		zinitix_printk("start upgrade firmware\n");

		if(ts_upgrade_firmware(touch_dev, &m_firmware_data[2],
			touch_dev->cap_info.chip_fw_size) == false)
			goto fail_init;

		if(ts_hw_calibration(touch_dev) == false)
			goto fail_init;

		/* disable chip interrupt */
		if (ts_write_reg(touch_dev->client,
			ZINITIX_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			goto fail_init;
		goto retry_init;
	}
#endif

	if (ts_read_data(touch_dev->client,
		ZINITIX_DATA_VERSION_REG,
		(u8 *)&chip_reg_data_version, 2) < 0)
		goto fail_init;
	zinitix_debug_msg("touch reg data version = %d\r\n",
		chip_reg_data_version);

	if (ts_read_data(touch_dev->client,
		ZINITIX_EEPROM_INFO_REG,
		(u8 *)&chip_eeprom_info, 2) < 0)
		goto fail_init;
	zinitix_debug_msg("touch eeprom info = 0x%04X\r\n", chip_eeprom_info);


	if (zinitix_bit_test(chip_eeprom_info, 0)) { /* hw calibration bit*/

		 /* h/w calibration */
		if(ts_hw_calibration(touch_dev) == false)
			goto fail_init;

		/* disable chip interrupt */
		if (ts_write_reg(touch_dev->client,
			ZINITIX_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			goto fail_init;

	}


	touch_dev->cap_info.chip_revision = (u16)chip_revision;
	touch_dev->cap_info.chip_firmware_version = (u16)chip_firmware_version;
	touch_dev->cap_info.chip_reg_data_version = (u16)chip_reg_data_version;


	/* initialize */
	if (ts_write_reg(touch_dev->client,
		ZINITIX_X_RESOLUTION, (u16)(SetMaxX)) != I2C_SUCCESS)
		goto fail_init;

	if (ts_write_reg(touch_dev->client,
		ZINITIX_Y_RESOLUTION, (u16)(SetMaxY)) != I2C_SUCCESS)
		goto fail_init;

	touch_dev->cap_info.x_resolution = SetMaxX;
	touch_dev->cap_info.y_resolution = SetMaxY;	

	zinitix_debug_msg("touch max x = %d\r\n",
		touch_dev->cap_info.x_resolution);
	zinitix_debug_msg("touch max y = %d\r\n",
		touch_dev->cap_info.y_resolution);

	touch_dev->cap_info.MinX = (u32)0;
	touch_dev->cap_info.MinY = (u32)0;
	touch_dev->cap_info.MaxX = (u32)touch_dev->cap_info.x_resolution;
	touch_dev->cap_info.MaxY = (u32)touch_dev->cap_info.y_resolution;

	if (ts_write_reg(touch_dev->client,
		ZINITIX_BUTTON_SUPPORTED_NUM,
		(u16)touch_dev->cap_info.button_num) != I2C_SUCCESS)
		goto fail_init;

	if (ts_write_reg(touch_dev->client,
		ZINITIX_SUPPORTED_FINGER_NUM,
		(u16)MAX_SUPPORTED_FINGER_NUM) != I2C_SUCCESS)
		goto fail_init;
	touch_dev->cap_info.multi_fingers = MAX_SUPPORTED_FINGER_NUM;

	zinitix_debug_msg("max supported finger num = %d\r\n",
		touch_dev->cap_info.multi_fingers);
	touch_dev->cap_info.gesture_support = 0;
	zinitix_debug_msg("set other configuration\r\n");


	if (touch_dev->raw_mode_flag != TOUCH_NORMAL_MODE) {	/* Test Mode */
		if (ts_write_reg(touch_dev->client,
			ZINITIX_TOUCH_MODE,
			touch_dev->raw_mode_flag) != I2C_SUCCESS)
			goto fail_init;
	} else
	{
		reg_val = TOUCH_MODE;
		if (ts_write_reg(touch_dev->client,
			ZINITIX_TOUCH_MODE, reg_val) != I2C_SUCCESS)
			goto fail_init;
	}
	/* soft calibration */
	if (ts_write_cmd(touch_dev->client,
		ZINITIX_CALIBRATE_CMD) != I2C_SUCCESS)
		goto fail_init;
	if (ts_write_reg(touch_dev->client,
		ZINITIX_INT_ENABLE_FLAG,
		touch_dev->cap_info.chip_int_mask) != I2C_SUCCESS)
		goto fail_init;

	/* read garbage data */
	for (i = 0; i < 10; i++)	{
		ts_write_cmd(touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
		udelay(10);
	}

	if (touch_dev->raw_mode_flag != TOUCH_NORMAL_MODE) { /* Test Mode */
		if (ts_write_reg(touch_dev->client,
			ZINITIX_PERIODICAL_INTERRUPT_INTERVAL,
			ZINITIX_SCAN_RATE_HZ
			*ZINITIX_RAW_DATA_ESD_TIMER_INTERVAL) != I2C_SUCCESS)
			zinitix_printk("fail to set ZINITIX_RAW_DATA_ESD_TIMER_INTERVAL.\r\n");
	} else
	{
		if (ts_write_reg(touch_dev->client,
			ZINITIX_PERIODICAL_INTERRUPT_INTERVAL,
			ZINITIX_SCAN_RATE_HZ
			*ZINITIX_ESD_TIMER_INTERVAL) != I2C_SUCCESS)
			goto fail_init;
		zinitix_debug_msg("esd timer register = %d\r\n", reg_val);
	}

	zinitix_debug_msg("successfully initialized\r\n");
	return true;

fail_init:

	if (++retry_cnt <= ZINITIX_INIT_RETRY_CNT) {
		ts_power_control(POWER_OFF);
		mdelay(CHIP_POWER_OFF_DELAY);
		ts_power_control(POWER_ON);
		mdelay(CHIP_ON_DELAY);
		zinitix_debug_msg("retry to initiallize(retry cnt = %d)\r\n",
			retry_cnt);
		goto	retry_init;
	} else if(retry_cnt == ZINITIX_INIT_RETRY_CNT+1) {
		touch_dev->cap_info.chip_fw_size = 32*1024;

		zinitix_debug_msg("retry to initiallize(retry cnt = %d)\r\n", retry_cnt);
#if TOUCH_FORCE_UPGRADE
		if( ts_upgrade_firmware(touch_dev, &m_firmware_data[2], 
			touch_dev->cap_info.chip_fw_size) == false) {
			zinitix_printk("upgrade fail\n");
			return false;
		}
		mdelay(100);
		// hw calibration and make checksum			
		if(ts_hw_calibration(touch_dev) == false)
			goto fail_hwcal_init;
		goto retry_init;
#endif	  
	}
fail_hwcal_init:		
	zinitix_printk("failed to initiallize\r\n");
	return false;
}

static void zinitix_clear_report_data(struct zinitix_touch_dev *touch_dev)
{
	int i;
	u8 reported = 0;
	u8 sub_status;

	
	for (i = 0; i < touch_dev->cap_info.button_num; i++) {
		if (touch_dev->button[i] == ICON_BUTTON_DOWN) {
			touch_dev->button[i] = ICON_BUTTON_UP;
			input_report_key(touch_dev->input_dev,
				BUTTON_MAPPING_KEY[i], 0);
			reported = true;
			zinitix_debug_msg("button up = %d \r\n", i);
		}
	}
		

	for (i = 0; i < touch_dev->cap_info.multi_fingers; i++) {
		sub_status = touch_dev->reported_touch_info.coord[i].sub_status;
		if (zinitix_bit_test(sub_status, SUB_BIT_EXIST)) {
#if MULTI_PROTOCOL_TYPE_B
			input_mt_slot(touch_dev->input_dev, i);
			input_mt_report_slot_state(touch_dev->input_dev,
				MT_TOOL_FINGER, 0);

			input_report_abs(touch_dev->input_dev,
					ABS_MT_TRACKING_ID, -1);
#else
			input_report_abs(touch_dev->input_dev,
					ABS_MT_TRACKING_ID, i);
			input_report_abs(touch_dev->input_dev,
				ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(touch_dev->input_dev,
				ABS_MT_WIDTH_MAJOR, 0);		
			input_report_abs(touch_dev->input_dev,
				ABS_MT_POSITION_X,
				touch_dev->reported_touch_info.coord[i].x);
			input_report_abs(touch_dev->input_dev,
				ABS_MT_POSITION_Y,
				touch_dev->reported_touch_info.coord[i].y);
			input_mt_sync(touch_dev->input_dev);
#endif

			reported = true;
			if (!m_ts_debug_mode)
				printk(KERN_INFO "[TSP] R %02d\r\n", i);
		}
		touch_dev->reported_touch_info.coord[i].sub_status = 0;
	}
	if (reported) {
#if !MULTI_PROTOCOL_TYPE_B
		input_report_key(touch_dev->input_dev, BTN_TOUCH, 0);
#endif
		input_sync(touch_dev->input_dev);
	}

}


static void zinitix_parsing_data(struct zinitix_touch_dev *touch_dev)
{
	
	int i;
	u8 reported = false;
	u8 sub_status;
	u8 prev_sub_status;
	u32 x, y;
	u32 w;
	u32 tmp;
	
	if(touch_dev == NULL) {
		printk(KERN_INFO "zinitix_parsing_data : touch_dev == NULL?\n");
		return;
	}

	if (touch_dev->use_esd_timer) {
		ts_esd_timer_stop(touch_dev);
		zinitix_debug_msg("esd timer stop\r\n");
	}

#if !USE_THREADED_IRQ	
	down(&touch_dev->work_proceedure_lock);
#endif
	if (touch_dev->work_proceedure != TS_NO_WORK) {
		zinitix_debug_msg("zinitix_touch_thread : \
			[warning] other process occupied..\r\n");
		udelay(DELAY_FOR_SIGNAL_DELAY);
		if (!gpio_get_value(touch_dev->int_gpio_num)) {
			ts_write_cmd(touch_dev->client,
				ZINITIX_CLEAR_INT_STATUS_CMD);
			udelay(DELAY_FOR_SIGNAL_DELAY);
		}

		goto continue_parsing_data;
	}
	touch_dev->work_proceedure = TS_NORMAL_WORK;
	if (ts_read_coord(touch_dev) == false || touch_dev->touch_info.status == 0xffff) {
		zinitix_debug_msg("couldn't read touch_dev coord. maybe chip is dead\r\n");
		ts_power_control(POWER_OFF);
		mdelay(CHIP_POWER_OFF_DELAY);
		ts_power_control(POWER_ON);
		mdelay(CHIP_ON_DELAY);
		
		zinitix_debug_msg("clear all reported points\r\n");
		zinitix_clear_report_data(touch_dev);
		ts_mini_init_touch(touch_dev);
		goto continue_parsing_data;
	}
	

	if (touch_dev->raw_mode_flag == TOUCH_TEST_RAW_MODE)
		goto continue_parsing_data;
	
	/* invalid : maybe periodical repeated int. */
	if (touch_dev->touch_info.status == 0x0)
		goto continue_parsing_data;

	reported = false;

	if (zinitix_bit_test(touch_dev->touch_info.status, BIT_ICON_EVENT)) {
		for (i = 0; i < touch_dev->cap_info.button_num; i++) {
			if (zinitix_bit_test(touch_dev->icon_event_reg,
				(BIT_O_ICON0_DOWN+i))) {
				touch_dev->button[i] = ICON_BUTTON_DOWN;
				input_report_key(touch_dev->input_dev,
					BUTTON_MAPPING_KEY[i], 1);
				reported = true;
					zinitix_debug_msg("button down = %d\r\n", i);
				if (!m_ts_debug_mode)
					printk(KERN_INFO "[TSP] button down = %d\r\n", i);
			}
		}

		for (i = 0; i < touch_dev->cap_info.button_num; i++) {
			if (zinitix_bit_test(touch_dev->icon_event_reg,
				(BIT_O_ICON0_UP+i))) {
				touch_dev->button[i] = ICON_BUTTON_UP;
				input_report_key(touch_dev->input_dev,
					BUTTON_MAPPING_KEY[i], 0);
				reported = true;
				
				zinitix_debug_msg("button up = %d\r\n", i);
				if (!m_ts_debug_mode)
					printk(KERN_INFO "[TSP] button down = %d\r\n", i);
			}
		}
	}

	/* if button press or up event occured... */
	if (reported == true || !zinitix_bit_test(touch_dev->touch_info.status, BIT_PT_EXIST)) {
		for (i = 0; i < touch_dev->cap_info.multi_fingers; i++) {
			prev_sub_status =
			touch_dev->reported_touch_info.coord[i].sub_status;
			if (zinitix_bit_test(prev_sub_status, SUB_BIT_EXIST)) {
				zinitix_debug_msg("finger [%02d] up\r\n", i);

#if MULTI_PROTOCOL_TYPE_B
				input_mt_slot(touch_dev->input_dev, i);
				input_mt_report_slot_state(touch_dev->input_dev,
					MT_TOOL_FINGER, 0);

				input_report_abs(touch_dev->input_dev,
						ABS_MT_TRACKING_ID, -1);
#else
				input_report_abs(touch_dev->input_dev,
						ABS_MT_TRACKING_ID, i);
				input_report_abs(touch_dev->input_dev,
					ABS_MT_TOUCH_MAJOR, 0);
				input_report_abs(touch_dev->input_dev,
					ABS_MT_WIDTH_MAJOR, 0);		
				input_report_abs(touch_dev->input_dev,
					ABS_MT_POSITION_X,
					touch_dev->reported_touch_info.coord[i].x);
				input_report_abs(touch_dev->input_dev,
					ABS_MT_POSITION_Y,
					touch_dev->reported_touch_info.coord[i].y);
				input_mt_sync(touch_dev->input_dev);
#endif				
				if (!m_ts_debug_mode)
					printk(KERN_INFO "[TSP] R %02d\r\n", i);
			}
		}
		memset(&touch_dev->reported_touch_info,
			0x0, sizeof(struct _ts_zinitix_point_info));
#if !MULTI_PROTOCOL_TYPE_B
		input_report_key(touch_dev->input_dev, BTN_TOUCH, 0);
#endif		
		input_sync(touch_dev->input_dev);
		if(reported == true)	// for button event
			udelay(100);
		goto continue_parsing_data;
	}
	if (touch_dev->touch_info.finger_cnt > MAX_SUPPORTED_FINGER_NUM)
		touch_dev->touch_info.finger_cnt = MAX_SUPPORTED_FINGER_NUM;
		
	for (i = 0; i < touch_dev->cap_info.multi_fingers; i++) {
		sub_status = touch_dev->touch_info.coord[i].sub_status;

		if (zinitix_bit_test(sub_status, SUB_BIT_DOWN)
			|| zinitix_bit_test(sub_status, SUB_BIT_MOVE)
			|| zinitix_bit_test(sub_status, SUB_BIT_EXIST)) {

			x = touch_dev->touch_info.coord[i].x;
			y = touch_dev->touch_info.coord[i].y;
			w = touch_dev->touch_info.coord[i].width;

			 /* transformation from touch to screen orientation */
			if (touch_dev->cap_info.Orientation & TOUCH_V_FLIP)
				y = touch_dev->cap_info.MaxY
					+ touch_dev->cap_info.MinY - y;
			if (touch_dev->cap_info.Orientation & TOUCH_H_FLIP)
				x = touch_dev->cap_info.MaxX
					+ touch_dev->cap_info.MinX - x;
			if (touch_dev->cap_info.Orientation & TOUCH_XY_SWAP)
				zinitix_swap_v(x, y, tmp);
			touch_dev->touch_info.coord[i].x = x;
			touch_dev->touch_info.coord[i].y = y;
			zinitix_debug_msg("finger [%02d] x = %d, y = %d\r\n",
				i, x, y);

			if (w == 0)
				w = 1;

			prev_sub_status =
			touch_dev->reported_touch_info.coord[i].sub_status;

			if (!m_ts_debug_mode && !zinitix_bit_test(prev_sub_status, SUB_BIT_EXIST))
				printk(KERN_INFO "[TSP] P %02d\r\n", i);

#if MULTI_PROTOCOL_TYPE_B
			input_mt_slot(touch_dev->input_dev, i);
			input_mt_report_slot_state(touch_dev->input_dev,
				MT_TOOL_FINGER, 1);
			if (!zinitix_bit_test(prev_sub_status, SUB_BIT_EXIST))
#endif				
			input_report_abs(touch_dev->input_dev,
					ABS_MT_TRACKING_ID, i);

			
			input_report_abs(touch_dev->input_dev,
				ABS_MT_TOUCH_MAJOR, (u32)w);
			input_report_abs(touch_dev->input_dev,
				ABS_MT_WIDTH_MAJOR, (u32)w);
			input_report_abs(touch_dev->input_dev,
				ABS_MT_POSITION_X, x);
			input_report_abs(touch_dev->input_dev,
				ABS_MT_POSITION_Y, y);
#if !MULTI_PROTOCOL_TYPE_B
			input_mt_sync(touch_dev->input_dev);			
#endif
		}else if (zinitix_bit_test(sub_status, SUB_BIT_UP)) {
			zinitix_debug_msg("finger [%02d] up \r\n", i);
			
			memset(&touch_dev->touch_info.coord[i],
				0x0, sizeof(struct _ts_zinitix_coord));
#if MULTI_PROTOCOL_TYPE_B			
			input_mt_slot(touch_dev->input_dev, i);
			input_mt_report_slot_state(touch_dev->input_dev,
				MT_TOOL_FINGER, 0);

			input_report_abs(touch_dev->input_dev,
					ABS_MT_TRACKING_ID, -1);
#else			
			input_report_abs(touch_dev->input_dev,
					ABS_MT_TRACKING_ID, i);
			input_report_abs(touch_dev->input_dev,
				ABS_MT_TOUCH_MAJOR, 0);
			input_report_abs(touch_dev->input_dev,
				ABS_MT_WIDTH_MAJOR, 0); 	
			input_report_abs(touch_dev->input_dev,
				ABS_MT_POSITION_X,
				touch_dev->reported_touch_info.coord[i].x);
			input_report_abs(touch_dev->input_dev,
				ABS_MT_POSITION_Y,
				touch_dev->reported_touch_info.coord[i].y);
			input_mt_sync(touch_dev->input_dev);
#endif								

			if (!m_ts_debug_mode)
				printk(KERN_INFO "[TSP] R %02d\r\n", i);
	
		} else
			memset(&touch_dev->touch_info.coord[i],
				0x0, sizeof(struct _ts_zinitix_coord));
		
	}
	memcpy((char *)&touch_dev->reported_touch_info,
		(char *)&touch_dev->touch_info,
		sizeof(struct _ts_zinitix_point_info));
#if !MULTI_PROTOCOL_TYPE_B
	input_report_key(touch_dev->input_dev, BTN_TOUCH, 1);
#endif
	input_sync(touch_dev->input_dev);
	
continue_parsing_data:
	
	/* check_interrupt_pin, if high, enable int & wait signal */
	if (touch_dev->work_proceedure == TS_NORMAL_WORK) {
		if (touch_dev->use_esd_timer) {
			ts_esd_timer_start(ZINITIX_CHECK_ESD_TIMER, touch_dev);
			zinitix_debug_msg("esd tmr start\n");
		}
		touch_dev->work_proceedure = TS_NO_WORK;
	}
#if !USE_THREADED_IRQ
	up(&touch_dev->work_proceedure_lock);	
#endif
}


#if USE_THREADED_IRQ
static irqreturn_t zinitix_touch_work(int irq, void *data)
{
	struct zinitix_touch_dev* touch_dev = (struct zinitix_touch_dev*)data;
	zinitix_debug_msg("threaded irq signalled\r\n");
	zinitix_parsing_data(touch_dev);
	return IRQ_HANDLED;
}
#else
static void zinitix_touch_work(struct work_struct *work)
{

	struct zinitix_touch_dev *touch_dev =
		container_of(work, struct zinitix_touch_dev, work);

	zinitix_debug_msg("signalled\r\n");
	zinitix_parsing_data(touch_dev);
	enable_irq(touch_dev->irq);
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void zinitix_late_resume(struct early_suspend *h)
{

	struct zinitix_touch_dev *touch_dev;
	touch_dev = container_of(h, struct zinitix_touch_dev, early_suspend);

	if (touch_dev == NULL)
		return;
	zinitix_printk("late resume++\r\n");

	down(&touch_dev->work_proceedure_lock);
	if (touch_dev->work_proceedure != TS_IN_RESUME
		&& touch_dev->work_proceedure != TS_IN_EALRY_SUSPEND) {
		zinitix_printk("invalid work proceedure (%d)\r\n",
			touch_dev->work_proceedure);
		up(&touch_dev->work_proceedure_lock);
		return;
	}
#ifdef CONFIG_PM_SLEEP	
	ts_write_cmd(touch_dev->client, ZINITIX_WAKEUP_CMD);
	mdelay(10);
#else
	ts_power_control(POWER_ON);
	mdelay(CHIP_ON_DELAY);
#endif	
	if (ts_mini_init_touch(touch_dev) == false)
		goto fail_late_resume;
	enable_irq(touch_dev->irq);
	touch_dev->work_proceedure = TS_NO_WORK;
	up(&touch_dev->work_proceedure_lock);
	zinitix_printk("resume--\n");
	return;
fail_late_resume:
	zinitix_printk("failed to resume\n");
	enable_irq(touch_dev->irq);
	touch_dev->work_proceedure = TS_NO_WORK;
	up(&touch_dev->work_proceedure_lock);
	return;
}


static void zinitix_early_suspend(struct early_suspend *h)
{
	struct zinitix_touch_dev *touch_dev =
		container_of(h, struct zinitix_touch_dev, early_suspend);

	if (touch_dev == NULL)
		return;

	zinitix_printk("early suspend++\n");

	disable_irq(touch_dev->irq);
	if (touch_dev->use_esd_timer)
		flush_work(&touch_dev->tmr_work);
#if !USE_THREADED_IRQ	
	flush_work(&touch_dev->work);
#endif

	down(&touch_dev->work_proceedure_lock);
	if (touch_dev->work_proceedure != TS_NO_WORK) {
		zinitix_printk("invalid work proceedure (%d)\r\n",
			touch_dev->work_proceedure);
		up(&touch_dev->work_proceedure_lock);
		enable_irq(touch_dev->irq);		
		return;
	}
	touch_dev->work_proceedure = TS_IN_EALRY_SUSPEND;

	zinitix_debug_msg("clear all reported points\r\n");
	zinitix_clear_report_data(touch_dev);

	if (touch_dev->use_esd_timer) {
		ts_write_reg(touch_dev->client,
			ZINITIX_PERIODICAL_INTERRUPT_INTERVAL, 0);
		ts_esd_timer_stop(touch_dev);
		zinitix_printk("ts_esd_timer_stop\n");
	}

#ifdef CONFIG_PM_SLEEP
	ts_write_reg(touch_dev->client, ZINITIX_INT_ENABLE_FLAG, 0x0);

	udelay(100);
	if (ts_write_cmd(touch_dev->client, ZINITIX_SLEEP_CMD) != I2C_SUCCESS) {
		zinitix_printk("failed to enter into sleep mode\n");
		up(&touch_dev->work_proceedure_lock);
		return;
	}
#else
	ts_power_control(POWER_OFF);
	mdelay(CHIP_POWER_OFF_DELAY);
#endif	
	zinitix_printk("early suspend--\n");
	up(&touch_dev->work_proceedure_lock);
	return;
}

#endif	/* CONFIG_HAS_EARLYSUSPEND */



#ifdef CONFIG_PM_SLEEP

static int zinitix_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct zinitix_touch_dev *touch_dev = i2c_get_clientdata(client);
	if (touch_dev == NULL)
		return 0;

	zinitix_debug_msg("resume++\n");
	down(&touch_dev->work_proceedure_lock);
	if (touch_dev->work_proceedure != TS_IN_SUSPEND) {
		zinitix_printk("invalid work proceedure (%d)\r\n",
			touch_dev->work_proceedure);
		up(&touch_dev->work_proceedure_lock);
		return 0;
	}

	ts_power_control(POWER_ON);
	msleep(CHIP_ON_DELAY);

#ifdef CONFIG_HAS_EARLYSUSPEND
	touch_dev->work_proceedure = TS_IN_RESUME;
#else
	touch_dev->work_proceedure = TS_NO_WORK;
	if (ts_mini_init_touch(touch_dev) == false)
		zinitix_printk("failed to resume\n");
	enable_irq(touch_dev->irq);
#endif


	zinitix_debug_msg("resume--\n");
	up(&touch_dev->work_proceedure_lock);
	return 0;
}

static int zinitix_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct zinitix_touch_dev *touch_dev = i2c_get_clientdata(client);
	if (touch_dev == NULL)
		return 0;

	zinitix_debug_msg("suspend++\n");
	
#ifndef CONFIG_HAS_EARLYSUSPEND	
	disable_irq(touch_dev->irq);	
#endif	
	if (touch_dev->use_esd_timer)
		flush_work(&touch_dev->tmr_work);
#if !USE_THREADED_IRQ	
	flush_work(&touch_dev->work);
#endif
	down(&touch_dev->work_proceedure_lock);
	if (touch_dev->work_proceedure != TS_NO_WORK
		&& touch_dev->work_proceedure != TS_IN_EALRY_SUSPEND) {
		zinitix_printk("zinitix_suspend : invalid work proceedure (%d)\r\n",
			touch_dev->work_proceedure);
		up(&touch_dev->work_proceedure_lock);
#ifndef CONFIG_HAS_EARLYSUSPEND	
		enable_irq(touch_dev->irq);	
#endif					
		return 0;
	}

#ifndef CONFIG_HAS_EARLYSUSPEND	
	zinitix_clear_report_data(touch_dev);

	if (touch_dev->use_esd_timer) {
		ts_esd_timer_stop(touch_dev);
		zinitix_printk("ts_esd_timer_stop\n");
	}

	ts_write_cmd(touch_dev->client, ZINITIX_SLEEP_CMD);
	udelay(100);
#endif

	ts_power_control(POWER_OFF);
	mdelay(CHIP_POWER_OFF_DELAY);

	touch_dev->work_proceedure = TS_IN_SUSPEND;

	zinitix_debug_msg("suspend--\n");
	up(&touch_dev->work_proceedure_lock);
	return 0;
}

#endif

static SIMPLE_DEV_PM_OPS(zinitix_dev_pm_ops,
			 zinitix_suspend, zinitix_resume);



#if SEC_TOUCH_INFO_REPORT

#define	MAX_X_NODE_CNT		20
#define	MAX_Y_NODE_CNT		16
#define ZINITIX_MENU_KEY        158
#define ZINITIX_BACK_KEY        151
#define	CAL_MIN_NUM		1400
#define	CAL_MAX_NUM		2030
#define	INT_WAIT_TIME		50

#define	CHIP_INFO		"ZINITIX"

static ssize_t zinitix_get_test_raw_data(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i,j;
	int k = 0;

	int written_bytes = 0 ;  /* & error check */

	int16_t raw_data[MAX_X_NODE_CNT][MAX_Y_NODE_CNT]={{0,},};
	int16_t diff_data[MAX_X_NODE_CNT][MAX_Y_NODE_CNT]={{0,},};

	if(misc_touch_dev == NULL) {
		zinitix_debug_msg("device NULL : NULL\n");
		return 0;
	}

	down(&misc_touch_dev->raw_data_lock);

	for(j = 0 ; j < misc_touch_dev->cap_info.y_node_num; j++)		
	{
		k = j;
		for(i = 0 ; i < misc_touch_dev->cap_info.x_node_num ; i++)
		{
			raw_data[i][j] =(int)(misc_touch_dev->ref_data[k]&0xffff);
			diff_data[i][j] =(int)(((s16)(misc_touch_dev->cur_data[k]-misc_touch_dev->ref_data[k]))&0xffff);
			k+=misc_touch_dev->cap_info.y_node_num;
		}
	}

	up(&misc_touch_dev->raw_data_lock);	

	if(m_ts_debug_mode) {		
		for(i = 0 ; i < misc_touch_dev->cap_info.x_node_num ; i++) {
			printk("[TSP]");
			for(j = 0 ; j < misc_touch_dev->cap_info.y_node_num ; j++ )
				printk(" %5d", raw_data[i][j]);
			printk("\n");
		}

		for(i = 0 ; i < misc_touch_dev->cap_info.x_node_num ; i++) {
			printk("[TSP]");
			for(j = 0 ; j < misc_touch_dev->cap_info.y_node_num ; j++ )
				printk(" %5d", diff_data[i][j]);
			printk("\n");
		}
	}

	for(j = 0 ; j < misc_touch_dev->cap_info.y_node_num ; j++)
	{
		for (i = 0; i < misc_touch_dev->cap_info.x_node_num ; i++)
			written_bytes += sprintf(buf+written_bytes, "%d %d\n", raw_data[i][j], diff_data[i][j]);
	}

	if (written_bytes > 0)
		return written_bytes;

	return sprintf(buf, "-1");
}


ssize_t zinitix_set_testmode(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned char value = 0;
	int i;

	printk(KERN_INFO "[zinitix_touch] zinitix_set_testmode, buf = %d\r\n", *buf);

	if(misc_touch_dev == NULL) {
		zinitix_debug_msg("device NULL : NULL\n");
		return 0;
	}

	sscanf(buf, "%c", &value);

	if(value != TOUCH_TEST_RAW_MODE && value != TOUCH_NORMAL_MODE)
	{
		printk(KERN_WARNING "[zinitix ts] test mode setting value error. you must set %d[=normal] or %d[=raw mode]\r\n", TOUCH_NORMAL_MODE, TOUCH_TEST_RAW_MODE);
		return 1;
	}

	down(&misc_touch_dev->work_proceedure_lock);
	if (misc_touch_dev->work_proceedure != TS_NO_WORK) {
		printk(KERN_INFO "other process occupied.. (%d)\n",
			misc_touch_dev->work_proceedure);
		up(&misc_touch_dev->work_proceedure_lock);
		return -1;
	}	
	misc_touch_dev->work_proceedure = TS_SET_MODE;

	misc_touch_dev->raw_mode_flag = value;

	printk(KERN_INFO "[zinitix_touch] zinitix_set_testmode, touchkey_testmode = %d\r\n", misc_touch_dev->raw_mode_flag);

	if(misc_touch_dev->raw_mode_flag == TOUCH_NORMAL_MODE) {
		printk(KERN_INFO "[zinitix_touch] TEST Mode Exit\r\n");

		if (ts_write_reg(misc_touch_dev->client, ZINITIX_PERIODICAL_INTERRUPT_INTERVAL, ZINITIX_SCAN_RATE_HZ*ZINITIX_ESD_TIMER_INTERVAL)!=I2C_SUCCESS)
			printk(KERN_INFO "[zinitix_touch] Fail to set ZINITIX_PERIODICAL_INTERRUPT_INTERVAL.\r\n");

		if (ts_write_reg(misc_touch_dev->client, ZINITIX_TOUCH_MODE, TOUCH_MODE)!=I2C_SUCCESS)
			printk(KERN_INFO "[zinitix_touch] Fail to set ZINITX_TOUCH_MODE %d.\r\n", TOUCH_MODE);

	} else {
		printk(KERN_INFO "[zinitix_touch] TEST Mode Enter\r\n");

		if (ts_write_reg(misc_touch_dev->client, ZINITIX_PERIODICAL_INTERRUPT_INTERVAL, ZINITIX_SCAN_RATE_HZ*ZINITIX_RAW_DATA_ESD_TIMER_INTERVAL)!=I2C_SUCCESS)
			printk(KERN_INFO "[zinitix_touch] Fail to set ZINITIX_RAW_DATA_ESD_TIMER_INTERVAL.\r\n");

		if (ts_write_reg(misc_touch_dev->client, ZINITIX_TOUCH_MODE, TOUCH_TEST_RAW_MODE)!=I2C_SUCCESS)
			printk(KERN_INFO "[zinitix_touch] TEST Mode : Fail to set ZINITX_TOUCH_MODE %d.\r\n", TOUCH_TEST_RAW_MODE);

		memset(&misc_touch_dev->reported_touch_info, 0x0, sizeof(struct _ts_zinitix_point_info));
		memset(&misc_touch_dev->touch_info, 0x0, sizeof(struct _ts_zinitix_point_info));
	}
	// clear garbage data
	for(i=0; i<=20; i++) {
		mdelay(20);			
		ts_write_cmd(misc_touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
	}

	misc_touch_dev->work_proceedure = TS_NO_WORK;
	up(&misc_touch_dev->work_proceedure_lock);
	return 1;             

}

static ssize_t zinitix_tkey_reference(struct device *dev, struct device_attribute *attr, char *buf)
{

	printk(KERN_INFO "[zinitix_touch] %s\r\n", __FUNCTION__);

	if(misc_touch_dev == NULL) {
		zinitix_debug_msg("device NULL : NULL\n");
		return 0;
	}

	printk(KERN_INFO "[zinitix_touch] %s :  menu(%d), back(%d)\r\n", __FUNCTION__, misc_touch_dev->ref_data[ZINITIX_MENU_KEY], misc_touch_dev->ref_data[ZINITIX_BACK_KEY]);

	return sprintf(buf, "%d %d", misc_touch_dev->ref_data[ZINITIX_MENU_KEY], misc_touch_dev->ref_data[ZINITIX_BACK_KEY]);    
}

static ssize_t zinitix_tkey_data(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk(KERN_INFO "[zinitix_touch] %s\r\n", __FUNCTION__);

	if(misc_touch_dev == NULL) {
		zinitix_debug_msg("device NULL : NULL\n");
		return 0;
	}

	printk(KERN_INFO "[zinitix_touch] %s :  menu(%d)(%d), back(%d)(%d)\r\n", __FUNCTION__, 
						misc_touch_dev->ref_data[ZINITIX_MENU_KEY], misc_touch_dev->cur_data[ZINITIX_MENU_KEY], 
						misc_touch_dev->ref_data[ZINITIX_BACK_KEY], misc_touch_dev->cur_data[ZINITIX_BACK_KEY]);  

	return sprintf(buf, "%d\n%d\n%d\n%d\n", misc_touch_dev->ref_data[ZINITIX_MENU_KEY], misc_touch_dev->cur_data[ZINITIX_MENU_KEY], 
						misc_touch_dev->ref_data[ZINITIX_BACK_KEY], misc_touch_dev->cur_data[ZINITIX_BACK_KEY]);    
}
static int16_t zinitix_cal_ndata[MAX_X_NODE_CNT][MAX_Y_NODE_CNT] = {{0,},};
static ssize_t zinitix_touch_cal_ndata(struct device *dev, struct device_attribute *attr, char *buf)
{
	int time_out = 0;
	int i, j;
	int interrup_detecting;
	int node_num;
	int written_bytes = 0;	/* error check */

	int16_t raw_data[MAX_X_NODE_CNT][MAX_Y_NODE_CNT] = {{0,},};

	printk(KERN_INFO "[zinitix_touch] zinitix_read_raw_data( )\r\n");	

	down(&misc_touch_dev->work_proceedure_lock);
	if (misc_touch_dev->work_proceedure != TS_NO_WORK) {
		printk(KERN_INFO "other process occupied.. (%d)\n",
			misc_touch_dev->work_proceedure);
		up(&misc_touch_dev->work_proceedure_lock);
		return -1;
	}

	misc_touch_dev->work_proceedure = TS_SET_MODE;

	disable_irq(misc_touch_dev->client->irq);

	if (ts_write_reg(misc_touch_dev->client,
			ZINITIX_TOUCH_MODE, 7) != I2C_SUCCESS) {
		printk(KERN_INFO "[zinitix_touch] ZINITIX_TOUCH_MODE fail\r\n");
		goto fail_read_raw;
	}
	udelay(10);
	ts_write_cmd(misc_touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);

	//garbage data
	for(i=0; i<20; i++) {
		mdelay(20);
		ts_write_cmd(misc_touch_dev->client,
			ZINITIX_CLEAR_INT_STATUS_CMD);
	}
		

	for (i = 0; i < INT_WAIT_TIME; i++) {
		ts_write_cmd(misc_touch_dev->client,
		ZINITIX_CLEAR_INT_STATUS_CMD);
		time_out = 0;
		interrup_detecting = 1;

		while (gpio_get_value(misc_touch_dev->int_gpio_num)) {
			mdelay(1);

			if (time_out++ > 20) {
				printk(KERN_INFO "[zinitix_touch] interrupt disable timed out\r\n");
				interrup_detecting = 0;
				break;
			}
		}
		if (i == INT_WAIT_TIME) {
			printk(KERN_INFO "[zinitix_touch] interrupt disable timed out\r\n");
			goto fail_read_raw;
		}

		if ((interrup_detecting == 1) && (time_out < 20)) {
			if (ts_read_raw_data(misc_touch_dev->client,
			ZINITIX_RAWDATA_REG, (char *)raw_data,
			MAX_TEST_RAW_DATA*2) < 0) {
				printk(KERN_INFO "[zinitix_touch] ZINITIX_RAWDATA_REG fail\r\n");
				goto fail_read_raw;
			}
			memcpy(zinitix_cal_ndata, raw_data, MAX_TEST_RAW_DATA*2);
			break;
		}
	}

	if (ts_write_reg(misc_touch_dev->client,
		ZINITIX_TOUCH_MODE, TOUCH_MODE) != I2C_SUCCESS) {
		printk(KERN_INFO "[zinitix_touch] ZINITIX_TOUCH_MODE fail\r\n");
		goto fail_read_raw;
	}
	ts_write_cmd(misc_touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
	udelay(20);
	ts_write_cmd(misc_touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
	mdelay(20);
	ts_write_cmd(misc_touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
	misc_touch_dev->work_proceedure = TS_NO_WORK;
	up(&misc_touch_dev->work_proceedure_lock);	
	enable_irq(misc_touch_dev->client->irq);

	if(m_ts_debug_mode) {
		for (i = 0; i < misc_touch_dev->cap_info.x_node_num; i++) {
			printk(KERN_CONT "[TSP]");
			for (j = 0; j < misc_touch_dev->cap_info.y_node_num; j++)
				printk(KERN_CONT " %5d", raw_data[i][j]);
			printk(KERN_CONT "\n");
		}
	}

	// check cal ndata range
	for (i = 0; i < misc_touch_dev->cap_info.x_node_num-1; i++) {
		for (j = 0; j < misc_touch_dev->cap_info.y_node_num; j++) {
			if (raw_data[i][j] < CAL_MIN_NUM)
				return snprintf(buf, 2, "0");

			if (raw_data[i][j] > CAL_MAX_NUM)
				return snprintf(buf, 2, "0");
		}
	}

	// check cal touch key ndata range
	for (j = 0; j < misc_touch_dev->cap_info.y_node_num; j++) {
		node_num = ((misc_touch_dev->cap_info.x_node_num-1)*misc_touch_dev->cap_info.y_node_num) + j;
		if(node_num == ZINITIX_MENU_KEY || node_num == ZINITIX_BACK_KEY) {
			if (raw_data[misc_touch_dev->cap_info.x_node_num-1][j]
			< CAL_MIN_NUM)
				return snprintf(buf, 2, "0");

			if (raw_data[misc_touch_dev->cap_info.x_node_num-1][j]
			> CAL_MAX_NUM)
				return snprintf(buf, 2, "0");
		}
	}

	for (i = 0; i < misc_touch_dev->cap_info.x_node_num; i++) {
		for (j = 0; j < misc_touch_dev->cap_info.y_node_num; j++) {
			written_bytes += sprintf(buf+written_bytes, "%d,",raw_data[i][j]);
		}
	}
	if(written_bytes > 0)
		return written_bytes;    
fail_read_raw:
	if (ts_write_reg(misc_touch_dev->client,
		ZINITIX_TOUCH_MODE, TOUCH_MODE) != I2C_SUCCESS) {
		printk(KERN_INFO "[zinitix_touch] ZINITIX_TOUCH_MODE fail\r\n");
		goto fail_read_raw;
	}
	udelay(20);
	ts_write_cmd(misc_touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
	mdelay(20);
	ts_write_cmd(misc_touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);

	misc_touch_dev->work_proceedure = TS_NO_WORK;
	up(&misc_touch_dev->work_proceedure_lock);	
	enable_irq(misc_touch_dev->client->irq);

	return snprintf(buf, 2, "-1");
}

static ssize_t zinitix_touch_ndata_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i,j,written_bytes=0;
	printk("[TSP] %s\n",__func__);	
	
	for (i = 0; i < misc_touch_dev->cap_info.x_node_num-1; i++) {
		for (j = 0; j < misc_touch_dev->cap_info.y_node_num; j++) {
			written_bytes += sprintf(buf+written_bytes, "%d,",zinitix_cal_ndata[i][j]);
		}
	}
	if(written_bytes > 0)
		return written_bytes;  
}
static ssize_t zinitix_delta(struct device *dev, struct device_attribute *attr, char *buf)
{
	int written_bytes = 0, i;

	printk(KERN_INFO "[zinitix_touch] %s\r\n", __FUNCTION__);

	if(misc_touch_dev == NULL) {
		zinitix_debug_msg("device NULL : NULL\n");
		return 0;
	}

	for(i = 0 ; i < misc_touch_dev->cap_info.total_node_num; i++) {
		written_bytes += sprintf(buf+written_bytes, "%d,", misc_touch_dev->cur_data[i] - misc_touch_dev->ref_data[i]) ;
	}

	if(written_bytes > 0)
		return written_bytes;    
}

static ssize_t zinitix_cal_ncount(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i, j;
	int written_bytes = 0;	/* error check */

	u8 raw_data[MAX_X_NODE_CNT][MAX_Y_NODE_CNT] = {{0,},};

	printk(KERN_INFO "[zinitix_touch] zinitix_cal_ncount( )\r\n");
	down(&misc_touch_dev->work_proceedure_lock);
	if (misc_touch_dev->work_proceedure != TS_NO_WORK) {
		printk(KERN_INFO "other process occupied.. (%d)\n",
			misc_touch_dev->work_proceedure);
		up(&misc_touch_dev->work_proceedure_lock);
		return -1;
	}
	
	misc_touch_dev->work_proceedure = TS_SET_MODE;
	
	disable_irq(misc_touch_dev->client->irq);


	if (ts_write_reg(misc_touch_dev->client,
			ZINITIX_TOUCH_MODE, 8) != I2C_SUCCESS) {
		printk(KERN_INFO "[zinitix_touch] ZINITIX_TOUCH_MODE fail\r\n");
		goto fail_read_raw;
	}
	//clear garbage data
	ts_write_cmd(misc_touch_dev->client,
			ZINITIX_CLEAR_INT_STATUS_CMD);
	for(i=0; i<20; i++) {
		mdelay(20);
		ts_write_cmd(misc_touch_dev->client,
			ZINITIX_CLEAR_INT_STATUS_CMD);
	}


	if (ts_read_raw_data(misc_touch_dev->client,
			ZINITIX_RAWDATA_REG, (char *)raw_data, MAX_X_NODE_CNT*MAX_Y_NODE_CNT) < 0) {
		printk(KERN_INFO "[TSP] error : read zinitix tc Cal N count data\n");
		goto fail_read_raw;
	}

	if (ts_write_reg(misc_touch_dev->client,
			ZINITIX_TOUCH_MODE, TOUCH_MODE) != I2C_SUCCESS) {
		printk(KERN_INFO "[zinitix_touch] ZINITIX_TOUCH_MODE fail\r\n");
		goto fail_read_raw;
	}
	ts_write_cmd(misc_touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
	mdelay(20);
	ts_write_cmd(misc_touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
	mdelay(20);	
	ts_write_cmd(misc_touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);	
	misc_touch_dev->work_proceedure = TS_NO_WORK;
	up(&misc_touch_dev->work_proceedure_lock);	
	enable_irq(misc_touch_dev->client->irq);


	if(m_ts_debug_mode) {
		for (i = 0; i < misc_touch_dev->cap_info.x_node_num; i++) {
			printk("[TSP]");
			for (j = 0; j < misc_touch_dev->cap_info.y_node_num; j++) {
				printk(" %d", raw_data[i][j]);
			}
			printk("\n");
		}
	}


	for (i = 0; i < misc_touch_dev->cap_info.x_node_num; i++) {
		for (j = 0; j < misc_touch_dev->cap_info.y_node_num; j++) {
			written_bytes +=sprintf(buf+written_bytes, "%d,",raw_data[i][j]);

		}
	}

	if(written_bytes > 0)
		return written_bytes;    

	return snprintf(buf, 2, "-1");

fail_read_raw:
	if (ts_write_reg(misc_touch_dev->client,
			ZINITIX_TOUCH_MODE, TOUCH_MODE) != I2C_SUCCESS) {
		printk(KERN_INFO "[zinitix_touch] ZINITIX_TOUCH_MODE fail\r\n");
		goto fail_read_raw;
	}
	ts_write_cmd(misc_touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
	udelay(10);

	misc_touch_dev->work_proceedure = TS_NO_WORK;
	up(&misc_touch_dev->work_proceedure_lock);	
	enable_irq(misc_touch_dev->client->irq);

	return snprintf(buf, 2, "-1");
}

static ssize_t zinitix_enter_testmode(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	

	if(misc_touch_dev == NULL) {
		zinitix_debug_msg("device NULL : NULL\n");
		return 0;
	}
	//printk(KERN_INFO "[zinitix_touch] %s name : %s\r\n", __FUNCTION__, misc_touch_dev->client->name);

	down(&misc_touch_dev->work_proceedure_lock);
	if (misc_touch_dev->work_proceedure != TS_NO_WORK) {
		printk(KERN_INFO "other process occupied.. (%d)\n",
			misc_touch_dev->work_proceedure);
		up(&misc_touch_dev->work_proceedure_lock);
		return -1;
	}
	
	misc_touch_dev->work_proceedure = TS_SET_MODE;	
	misc_touch_dev->raw_mode_flag = TOUCH_TEST_RAW_MODE;

	if (ts_write_reg(misc_touch_dev->client, ZINITIX_PERIODICAL_INTERRUPT_INTERVAL, ZINITIX_SCAN_RATE_HZ*ZINITIX_RAW_DATA_ESD_TIMER_INTERVAL)!=I2C_SUCCESS)
		printk(KERN_INFO "[zinitix_touch] Fail to set ZINITIX_RAW_DATA_ESD_TIMER_INTERVAL.\r\n");

	if (ts_write_reg(misc_touch_dev->client, ZINITIX_TOUCH_MODE, TOUCH_TEST_RAW_MODE)!=I2C_SUCCESS)
	{
		printk(KERN_INFO "[zinitix_touch] TEST Mode : Fail to set ZINITX_TOUCH_MODE %d.\r\n", TOUCH_TEST_RAW_MODE);
	}

	ts_write_cmd(misc_touch_dev->client, ZINITIX_CLEAR_INT_STATUS_CMD);
	//clear garbage data
	ts_write_cmd(misc_touch_dev->client,
			ZINITIX_CLEAR_INT_STATUS_CMD);
	for(i=0; i<20; i++) {
		mdelay(20);
		ts_write_cmd(misc_touch_dev->client,
			ZINITIX_CLEAR_INT_STATUS_CMD);
	}		
	zinitix_clear_report_data(misc_touch_dev);
	memset(&misc_touch_dev->reported_touch_info, 0x0, sizeof(struct _ts_zinitix_point_info));
	memset(&misc_touch_dev->touch_info, 0x0, sizeof(struct _ts_zinitix_point_info));

	misc_touch_dev->work_proceedure = TS_NO_WORK;
	up(&misc_touch_dev->work_proceedure_lock);	
	enable_irq(misc_touch_dev->client->irq);
	return 1;             
}

static ssize_t zinitix_enter_ncountmode(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;
	printk(KERN_INFO "[zinitix_touch] %s 111 name : %s\r\n", __FUNCTION__, misc_touch_dev->client->name);

	if(misc_touch_dev == NULL) {
		zinitix_debug_msg("device NULL : NULL\n");
		return 0;
	}

	down(&misc_touch_dev->work_proceedure_lock);
	if (misc_touch_dev->work_proceedure != TS_NO_WORK) {
		printk(KERN_INFO "other process occupied.. (%d)\n",
			misc_touch_dev->work_proceedure);
		up(&misc_touch_dev->work_proceedure_lock);
		return -1;
	}
	misc_touch_dev->work_proceedure = TS_SET_MODE;
	misc_touch_dev->raw_mode_flag = TOUCH_ZINITIX_CAL_N_MODE;

	if (ts_write_reg(misc_touch_dev->client, ZINITIX_PERIODICAL_INTERRUPT_INTERVAL, ZINITIX_SCAN_RATE_HZ*ZINITIX_RAW_DATA_ESD_TIMER_INTERVAL)!=I2C_SUCCESS)
		printk(KERN_INFO "[zinitix_touch] Fail to set ZINITIX_RAW_DATA_ESD_TIMER_INTERVAL.\r\n");

	if (ts_write_reg(misc_touch_dev->client, ZINITIX_TOUCH_MODE, TOUCH_ZINITIX_CAL_N_MODE)!=I2C_SUCCESS)
	{
		printk(KERN_INFO "[zinitix_touch] TEST Mode : Fail to set ZINITX_TOUCH_MODE %d.\r\n", TOUCH_ZINITIX_CAL_N_MODE);
	}

	//clear garbage data
	ts_write_cmd(misc_touch_dev->client,
			ZINITIX_CLEAR_INT_STATUS_CMD);
	for(i=0; i<20; i++) {
		mdelay(20);
		ts_write_cmd(misc_touch_dev->client,
			ZINITIX_CLEAR_INT_STATUS_CMD);
	}
	zinitix_clear_report_data(misc_touch_dev);
	memset(&misc_touch_dev->reported_touch_info, 0x0, sizeof(struct _ts_zinitix_point_info));
	memset(&misc_touch_dev->touch_info, 0x0, sizeof(struct _ts_zinitix_point_info));

	misc_touch_dev->work_proceedure = TS_NO_WORK;
	up(&misc_touch_dev->work_proceedure_lock);	
	enable_irq(misc_touch_dev->client->irq);
	return 1;             
}

static ssize_t zinitix_enter_normalmode(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i;

	printk(KERN_INFO "[zinitix_touch] %s\r\n", __FUNCTION__);

	if(misc_touch_dev == NULL) {
		zinitix_debug_msg("device NULL : NULL\n");
		return 0;
	}

	down(&misc_touch_dev->work_proceedure_lock);
	if (misc_touch_dev->work_proceedure != TS_NO_WORK) {
		printk(KERN_INFO "other process occupied.. (%d)\n",
			misc_touch_dev->work_proceedure);
		up(&misc_touch_dev->work_proceedure_lock);
		return -1;
	}
	misc_touch_dev->work_proceedure = TS_SET_MODE;
	misc_touch_dev->raw_mode_flag = TOUCH_NORMAL_MODE;

	printk(KERN_INFO "[zinitix_touch] TEST Mode Exit\r\n");

	if (ts_write_reg(misc_touch_dev->client, ZINITIX_PERIODICAL_INTERRUPT_INTERVAL, ZINITIX_SCAN_RATE_HZ*ZINITIX_ESD_TIMER_INTERVAL)!=I2C_SUCCESS)
		printk(KERN_INFO "[zinitix_touch] Fail to set ZINITIX_PERIODICAL_INTERRUPT_INTERVAL.\r\n");

	if (ts_write_reg(misc_touch_dev->client, ZINITIX_TOUCH_MODE, TOUCH_MODE)!=I2C_SUCCESS)
	{
		printk(KERN_INFO "[zinitix_touch] Fail to set ZINITX_TOUCH_MODE %d.\r\n", TOUCH_MODE);
	}
	//clear garbage data
	ts_write_cmd(misc_touch_dev->client,
			ZINITIX_CLEAR_INT_STATUS_CMD);
	for(i=0; i<20; i++) {
		mdelay(20);
		ts_write_cmd(misc_touch_dev->client,
			ZINITIX_CLEAR_INT_STATUS_CMD);
	}	

	misc_touch_dev->work_proceedure = TS_NO_WORK;
	up(&misc_touch_dev->work_proceedure_lock);	
	enable_irq(misc_touch_dev->client->irq);
	return 1;                
}
static ssize_t zinitix_count_node(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("[TSP] %s\n",__func__);
	return sprintf(buf, "%d,%d\n", misc_touch_dev->cap_info.x_node_num,misc_touch_dev->cap_info.y_node_num);
}
static ssize_t zinitix_chip_info(struct device *dev, struct device_attribute *attr, char *buf)
{
	printk("[TSP] %s\n",__func__);
	return sprintf(buf, "%s, BT%03X\n", CHIP_INFO, (misc_touch_dev->cap_info.chip_revision>>4));
}
static ssize_t zinitix_menu_sensitivity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	u16 menu_sensitivity;

	ret = ts_read_data(misc_touch_dev->client, 0x00A8, (u8*)&menu_sensitivity, 2);

	if (ret<0)
		return sprintf(buf, "%s\n",  "fail to read menu_sensitivity.");	
	else
		return sprintf(buf, "%d\n",  menu_sensitivity);
}

static ssize_t zinitix_back_sensitivity_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int ret = 0;
	u16 back_sensitivity;

	ret = ts_read_data(misc_touch_dev->client, 0x00A9, (u8*)&back_sensitivity, 2);

	if (ret<0)
		return sprintf(buf, "%s\n",  "fail to read back_sensitivity.");	
	else
		return sprintf(buf, "%d\n",  back_sensitivity);
}
static DEVICE_ATTR(get_touch_raw_data, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, zinitix_get_test_raw_data, zinitix_set_testmode);
static DEVICE_ATTR(tkey_reference, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, zinitix_tkey_reference, NULL);
static DEVICE_ATTR(touch_cal_ndata, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, zinitix_touch_cal_ndata, NULL);
static DEVICE_ATTR(delta, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, zinitix_delta, NULL);
static DEVICE_ATTR(cal_ncount, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, zinitix_cal_ncount, NULL);
static DEVICE_ATTR(enter_testmode, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, zinitix_enter_testmode, NULL);
static DEVICE_ATTR(enter_normalmode, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, zinitix_enter_normalmode, NULL);
static DEVICE_ATTR(count_node, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, zinitix_count_node, NULL);	
static DEVICE_ATTR(chip_info, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, zinitix_chip_info, NULL);	
static DEVICE_ATTR(menu_sensitivity, S_IRUGO | S_IWUSR | S_IWGRP, zinitix_menu_sensitivity_show, NULL);
static DEVICE_ATTR(back_sensitivity, S_IRUGO | S_IWUSR | S_IWGRP, zinitix_back_sensitivity_show, NULL);
static DEVICE_ATTR(touch_ndata, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, zinitix_touch_ndata_show, NULL);
static DEVICE_ATTR(tkey_data, S_IRUGO | S_IWUSR | S_IWOTH | S_IXOTH, zinitix_tkey_data, NULL);

#endif

static int ts_misc_fops_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int ts_misc_fops_close(struct inode *inode, struct file *filp)
{
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 36)
static int ts_misc_fops_ioctl(struct inode *inode,
	struct file *filp, unsigned int cmd,
	unsigned long arg)
#else
static long ts_misc_fops_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg)
#endif
{
	void __user *argp = (void __user *)arg;
	struct _raw_ioctl raw_ioctl;
	u8 *u8Data;
	int i, j;
	int ret = 0;
	size_t sz = 0;
	u16 version;
	u8 div_node;
	int total_cal_n;

	u16 mode;

	struct _reg_ioctl reg_ioctl;
	u16 val;
	int nval = 0;

	if (misc_touch_dev == NULL)
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
		ret = misc_touch_dev->cap_info.chip_revision;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_FW_VERSION:
		ret = misc_touch_dev->cap_info.chip_firmware_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_REG_DATA_VERSION:
		ret = misc_touch_dev->cap_info.chip_reg_data_version;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_VARIFY_UPGRADE_SIZE:
		if (copy_from_user(&sz, argp, sizeof(size_t)))
			return -1;

		printk(KERN_INFO "firmware size = %d\r\n", sz);
		if (misc_touch_dev->cap_info.chip_fw_size != sz) {
			printk(KERN_INFO "firmware size error\r\n");
			return -1;
		}
		break;

	case TOUCH_IOCTL_VARIFY_UPGRADE_DATA:
		if (copy_from_user(&m_firmware_data[2],
			argp, misc_touch_dev->cap_info.chip_fw_size))
			return -1;

		version =
			(u16)(((u16)m_firmware_data[FIRMWARE_VERSION_POS+1]<<8)
			|(u16)m_firmware_data[FIRMWARE_VERSION_POS]);

		printk(KERN_INFO "firmware version = %x\r\n", version);

		if (copy_to_user(argp, &version, sizeof(version)))
			return -1;
		break;

	case TOUCH_IOCTL_START_UPGRADE:
		disable_irq(misc_touch_dev->irq);
		down(&misc_touch_dev->work_proceedure_lock);
		misc_touch_dev->work_proceedure = TS_IN_UPGRADE;

		if (misc_touch_dev->use_esd_timer)
			ts_esd_timer_stop(misc_touch_dev);
		zinitix_debug_msg("clear all reported points\r\n");
		zinitix_clear_report_data(misc_touch_dev);

		printk(KERN_INFO "start upgrade firmware\n");
		if (ts_upgrade_firmware(misc_touch_dev,
			&m_firmware_data[2],
			misc_touch_dev->cap_info.chip_fw_size) == false) {
			enable_irq(misc_touch_dev->irq);
			misc_touch_dev->work_proceedure = TS_NO_WORK;
			up(&misc_touch_dev->work_proceedure_lock);
			return -1;
		}

		if (ts_init_touch(misc_touch_dev) == false) {
			enable_irq(misc_touch_dev->irq);
			misc_touch_dev->work_proceedure = TS_NO_WORK;
			up(&misc_touch_dev->work_proceedure_lock);
			return -1;
		}


		if (misc_touch_dev->use_esd_timer) {
			ts_esd_timer_start(ZINITIX_CHECK_ESD_TIMER,
				misc_touch_dev);
			zinitix_debug_msg("esd timer start\r\n");
		}

		enable_irq(misc_touch_dev->irq);
		misc_touch_dev->work_proceedure = TS_NO_WORK;
		up(&misc_touch_dev->work_proceedure_lock);
		break;

	case TOUCH_IOCTL_GET_X_RESOLUTION:
		ret = misc_touch_dev->cap_info.x_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_RESOLUTION:
		ret = misc_touch_dev->cap_info.y_resolution;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_X_NODE_NUM:
		ret = misc_touch_dev->cap_info.x_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_Y_NODE_NUM:
		ret = misc_touch_dev->cap_info.y_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_GET_TOTAL_NODE_NUM:
		ret = misc_touch_dev->cap_info.total_node_num;
		if (copy_to_user(argp, &ret, sizeof(ret)))
			return -1;
		break;

	case TOUCH_IOCTL_HW_CALIBRAION:
		ret = -1;
		disable_irq(misc_touch_dev->irq);
		down(&misc_touch_dev->work_proceedure_lock);
		if (misc_touch_dev->work_proceedure != TS_NO_WORK) {
			printk(KERN_INFO"other process occupied.. (%d)\r\n",
				misc_touch_dev->work_proceedure);
			up(&misc_touch_dev->work_proceedure_lock);
			return -1;
		}
		misc_touch_dev->work_proceedure = TS_HW_CALIBRAION;
		mdelay(100);

		/* h/w calibration */
		if(ts_hw_calibration(misc_touch_dev) == false)
			goto fail_hw_cal;

		ret = 0;
fail_hw_cal:
		if (misc_touch_dev->raw_mode_flag == TOUCH_NORMAL_MODE)
			mode = TOUCH_MODE;
		else
			mode = misc_touch_dev->raw_mode_flag;
		if (ts_write_reg(misc_touch_dev->client,
			ZINITIX_TOUCH_MODE, mode) != I2C_SUCCESS) {
			printk(KERN_INFO "fail to set touch mode %d.\n",
				mode);
			goto fail_hw_cal2;
		}

		if (ts_write_cmd(misc_touch_dev->client,
			ZINITIX_SWRESET_CMD) != I2C_SUCCESS)
			goto fail_hw_cal2;

		enable_irq(misc_touch_dev->irq);
		misc_touch_dev->work_proceedure = TS_NO_WORK;
		up(&misc_touch_dev->work_proceedure_lock);
		return ret;
fail_hw_cal2:
		enable_irq(misc_touch_dev->irq);
		misc_touch_dev->work_proceedure = TS_NO_WORK;
		up(&misc_touch_dev->work_proceedure_lock);
		return -1;

	case TOUCH_IOCTL_SET_RAW_DATA_MODE:
		if (misc_touch_dev == NULL) {
			zinitix_debug_msg("misc device NULL?\n");
			return -1;
		}

		down(&misc_touch_dev->work_proceedure_lock);
		if (misc_touch_dev->work_proceedure != TS_NO_WORK) {
			printk(KERN_INFO"other process occupied.. (%d)\r\n",
				misc_touch_dev->work_proceedure);
			up(&misc_touch_dev->work_proceedure_lock);
			return -1;
		}
		misc_touch_dev->work_proceedure = TS_SET_MODE;

		if (copy_from_user(&nval, argp, 4)) {
			printk(KERN_INFO "[zinitix_touch] error : copy_from_user\r\n");
			misc_touch_dev->work_proceedure = TS_NO_WORK;
			return -1;
		}

		zinitix_debug_msg("[zinitix_touch] touchkey_testmode = %d\r\n",
			misc_touch_dev->raw_mode_flag);

		if (nval == TOUCH_NORMAL_MODE &&
			misc_touch_dev->raw_mode_flag != TOUCH_NORMAL_MODE) {
			/* enter into normal mode */
			misc_touch_dev->raw_mode_flag = nval;
			printk(KERN_INFO "[zinitix_touch] raw data mode exit\r\n");

			if (ts_write_reg(misc_touch_dev->client,
				ZINITIX_PERIODICAL_INTERRUPT_INTERVAL,
				ZINITIX_SCAN_RATE_HZ*ZINITIX_ESD_TIMER_INTERVAL)
				!= I2C_SUCCESS)
				printk(KERN_INFO "[zinitix_touch] Fail to set ZINITIX_PERIODICAL_INTERRUPT_INTERVAL.\n");

			if (ts_write_reg(misc_touch_dev->client,
				ZINITIX_TOUCH_MODE, TOUCH_MODE) != I2C_SUCCESS)
				printk(KERN_INFO "[zinitix_touch] fail to set TOUCH_MODE.\r\n");

		} else if (nval != TOUCH_NORMAL_MODE) {
			/* enter into test mode*/
			misc_touch_dev->raw_mode_flag = nval;
			printk(KERN_INFO "[zinitix_touch] raw data mode enter\r\n");

			if (ts_write_reg(misc_touch_dev->client,
				ZINITIX_PERIODICAL_INTERRUPT_INTERVAL,
				ZINITIX_SCAN_RATE_HZ
				*ZINITIX_RAW_DATA_ESD_TIMER_INTERVAL)
				!= I2C_SUCCESS)
				printk(KERN_INFO "[zinitix_touch] Fail to set ZINITIX_RAW_DATA_ESD_TIMER_INTERVAL.\n");

			if (ts_write_reg(misc_touch_dev->client,
				ZINITIX_TOUCH_MODE,
				misc_touch_dev->raw_mode_flag) != I2C_SUCCESS)
				printk(KERN_INFO "[zinitix_touch] raw data mode : Fail to set TOUCH_MODE %d.\n",
					misc_touch_dev->raw_mode_flag);
		}
		/* clear garbage data */
		ts_write_cmd(misc_touch_dev->client,
			ZINITIX_CLEAR_INT_STATUS_CMD);
		mdelay(100);
		ts_write_cmd(misc_touch_dev->client,
			ZINITIX_CLEAR_INT_STATUS_CMD);

		misc_touch_dev->work_proceedure = TS_NO_WORK;
		up(&misc_touch_dev->work_proceedure_lock);
		return 0;

	case TOUCH_IOCTL_GET_REG:
		if (misc_touch_dev == NULL) {
			zinitix_debug_msg("misc device NULL?\n");
			return -1;
		}
		down(&misc_touch_dev->work_proceedure_lock);
		if (misc_touch_dev->work_proceedure != TS_NO_WORK) {
			printk(KERN_INFO "other process occupied.. (%d)\n",
				misc_touch_dev->work_proceedure);
			up(&misc_touch_dev->work_proceedure_lock);
			return -1;
		}

		misc_touch_dev->work_proceedure = TS_SET_MODE;

		if (copy_from_user(&reg_ioctl,
			argp, sizeof(struct _reg_ioctl))) {
			misc_touch_dev->work_proceedure = TS_NO_WORK;
			up(&misc_touch_dev->work_proceedure_lock);
			printk(KERN_INFO "[zinitix_touch] error : copy_from_user\n");
			return -1;
		}

		if (ts_read_data(misc_touch_dev->client,
			reg_ioctl.addr, (u8 *)&val, 2) < 0)
			ret = -1;

		nval = (int)val;

		if (copy_to_user(reg_ioctl.val, (u8 *)&nval, 4)) {
			misc_touch_dev->work_proceedure = TS_NO_WORK;
			up(&misc_touch_dev->work_proceedure_lock);
			printk(KERN_INFO "[zinitix_touch] error : copy_to_user\n");
			return -1;
		}

		zinitix_debug_msg("read : reg addr = 0x%x, val = 0x%x\n",
			reg_ioctl.addr, nval);

		misc_touch_dev->work_proceedure = TS_NO_WORK;
		up(&misc_touch_dev->work_proceedure_lock);
		return ret;

	case TOUCH_IOCTL_SET_REG:

		if (misc_touch_dev == NULL) {
			zinitix_debug_msg("misc device NULL?\n");
			return -1;
		}
		down(&misc_touch_dev->work_proceedure_lock);
		if (misc_touch_dev->work_proceedure != TS_NO_WORK) {
			printk(KERN_INFO "other process occupied.. (%d)\n",
				misc_touch_dev->work_proceedure);
			up(&misc_touch_dev->work_proceedure_lock);
			return -1;
		}

		misc_touch_dev->work_proceedure = TS_SET_MODE;
		if (copy_from_user(&reg_ioctl,
				argp, sizeof(struct _reg_ioctl))) {
			misc_touch_dev->work_proceedure = TS_NO_WORK;
			up(&misc_touch_dev->work_proceedure_lock);
			printk(KERN_INFO "[zinitix_touch] error : copy_from_user\n");
			return -1;
		}

		if (copy_from_user(&val, reg_ioctl.val, 4)) {
			misc_touch_dev->work_proceedure = TS_NO_WORK;
			up(&misc_touch_dev->work_proceedure_lock);
			printk(KERN_INFO "[zinitix_touch] error : copy_from_user\n");
			return -1;
		}

		if (ts_write_reg(misc_touch_dev->client,
			reg_ioctl.addr, val) != I2C_SUCCESS)
			ret = -1;

		zinitix_debug_msg("write : reg addr = 0x%x, val = 0x%x\r\n",
			reg_ioctl.addr, val);
		misc_touch_dev->work_proceedure = TS_NO_WORK;
		up(&misc_touch_dev->work_proceedure_lock);
		return ret;

	case TOUCH_IOCTL_DONOT_TOUCH_EVENT:

		if (misc_touch_dev == NULL) {
			zinitix_debug_msg("misc device NULL?\n");
			return -1;
		}
		down(&misc_touch_dev->work_proceedure_lock);
		if (misc_touch_dev->work_proceedure != TS_NO_WORK) {
			printk(KERN_INFO"other process occupied.. (%d)\r\n",
				misc_touch_dev->work_proceedure);
			up(&misc_touch_dev->work_proceedure_lock);
			return -1;
		}

		misc_touch_dev->work_proceedure = TS_SET_MODE;
		if (ts_write_reg(misc_touch_dev->client,
			ZINITIX_INT_ENABLE_FLAG, 0) != I2C_SUCCESS)
			ret = -1;
		zinitix_debug_msg("write : reg addr = 0x%x, val = 0x0\r\n",
			ZINITIX_INT_ENABLE_FLAG);

		misc_touch_dev->work_proceedure = TS_NO_WORK;
		up(&misc_touch_dev->work_proceedure_lock);
		return ret;

	case TOUCH_IOCTL_SEND_SAVE_STATUS:
		if (misc_touch_dev == NULL) {
			zinitix_debug_msg("misc device NULL?\n");
			return -1;
		}
		down(&misc_touch_dev->work_proceedure_lock);
		if (misc_touch_dev->work_proceedure != TS_NO_WORK) {
			printk(KERN_INFO"other process occupied.. (%d)\r\n",
				misc_touch_dev->work_proceedure);
			up(&misc_touch_dev->work_proceedure_lock);
			return -1;
		}
		misc_touch_dev->work_proceedure = TS_SET_MODE;
		ret = 0;
		if (ts_write_cmd(misc_touch_dev->client,
			ZINITIX_SAVE_STATUS_CMD) != I2C_SUCCESS)
			ret = -1;
		mdelay(1000);	/* for fusing eeprom */

		misc_touch_dev->work_proceedure = TS_NO_WORK;
		up(&misc_touch_dev->work_proceedure_lock);
		return ret;

	case TOUCH_IOCTL_GET_RAW_DATA:
		if (misc_touch_dev == NULL) {
			zinitix_debug_msg("misc device NULL?\n");
			return -1;
		}

		if (misc_touch_dev->raw_mode_flag == TOUCH_NORMAL_MODE)
			return -1;

		down(&misc_touch_dev->raw_data_lock);
		if (misc_touch_dev->update == 0) {
			up(&misc_touch_dev->raw_data_lock);
			return -2;
		}

		if (copy_from_user(&raw_ioctl,
			argp, sizeof(raw_ioctl))) {
			up(&misc_touch_dev->raw_data_lock);
			printk(KERN_INFO "[zinitix_touch] error : copy_from_user\r\n");
			return -1;
		}

		misc_touch_dev->update = 0;

		u8Data = (u8 *)&misc_touch_dev->cur_data[0];
		if (misc_touch_dev->raw_mode_flag == TOUCH_ZINITIX_CAL_N_MODE) {


			j = 0;
			total_cal_n =
				misc_touch_dev->cap_info.total_cal_n;
			if (total_cal_n == 0)
				total_cal_n = 160;
			div_node =
				(u8)misc_touch_dev->cap_info.max_y_node;
			if (div_node == 0)
				div_node = 16;
			u8Data = (u8 *)&misc_touch_dev->cur_data[0];
			for (i = 0; i < total_cal_n*2; i++) {
				if (i%div_node <
					misc_touch_dev->cap_info.y_node_num) {
					misc_touch_dev->ref_data[j] =
						(u16)u8Data[i];
					j++;
				}
			}

			u8Data = (u8 *)&misc_touch_dev->ref_data[0];
		}

		if (copy_to_user(raw_ioctl.buf, (u8 *)u8Data,
			raw_ioctl.sz)) {
			up(&misc_touch_dev->raw_data_lock);
			return -1;
		}

		up(&misc_touch_dev->raw_data_lock);
		return 0;

	default:
		break;
	}
	return 0;
}



static int zinitix_touch_probe(struct i2c_client *client,
		const struct i2c_device_id *i2c_id)
{
	int ret = 0;
	struct zinitix_touch_dev *touch_dev;
	int i;

	zinitix_debug_msg("zinitix_touch_probe+\r\n");


	zinitix_debug_msg("Above BT4x3 Driver\r\n");

	printk(KERN_INFO "[zinitix touch] driver version = %s\r\n",
		TS_DRVIER_VERSION);


	if (strcmp(client->name, ZINITIX_ISP_NAME) == 0) {
		printk(KERN_INFO "isp client probe \r\n");
		m_isp_client = client;
		return 0;
	}

	zinitix_debug_msg("i2c check function \r\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ERR "error : not compatible i2c function \r\n");
		goto err_check_functionality;
	}

	zinitix_debug_msg("touch data alloc \r\n");
	touch_dev = kzalloc(sizeof(struct zinitix_touch_dev), GFP_KERNEL);
	if (!touch_dev) {
		printk(KERN_ERR "unabled to allocate touch data \r\n");
		goto err_alloc_dev_data;
	}
	touch_dev->client = client;
	i2c_set_clientdata(client, touch_dev);

#if !USE_THREADED_IRQ
	INIT_WORK(&touch_dev->work, zinitix_touch_work);
	zinitix_debug_msg("touch workqueue create \r\n");
	zinitix_workqueue = create_singlethread_workqueue("zinitix_workqueue");
	if (!zinitix_workqueue) {
		printk(KERN_ERR "unabled to create touch thread \r\n");
		goto err_kthread_create_failed;
	}
#endif

	zinitix_debug_msg("allocate input device \r\n");
	touch_dev->input_dev = input_allocate_device();
	if (touch_dev->input_dev == 0) {
		printk(KERN_ERR "unabled to allocate input device \r\n");
		goto err_input_allocate_device;
	}

	/* initialize zinitix touch ic */
	touch_dev->int_gpio_num = GPIO_TOUCH_PIN_NUM;	/*	for upgrade */

	memset(&touch_dev->reported_touch_info,
		0x0, sizeof(struct _ts_zinitix_point_info));


	/*	not test mode */
	touch_dev->raw_mode_flag = TOUCH_NORMAL_MODE;

	if(ts_init_touch(touch_dev) == false) {
		goto err_input_register_device;
	}

	for (i = 0; i < MAX_SUPPORTED_BUTTON_NUM; i++)
		touch_dev->button[i] = ICON_BUTTON_UNCHANGE;
		
	touch_dev->use_esd_timer = 0;

	INIT_WORK(&touch_dev->tmr_work, zinitix_touch_tmr_work);
	zinitix_tmr_workqueue =
		create_singlethread_workqueue("zinitix_tmr_workqueue");
	if (!zinitix_tmr_workqueue) {
		printk(KERN_ERR "unabled to create touch tmr work queue \r\n");
		goto err_kthread_create_failed;
	}
	
	if (ZINITIX_ESD_TIMER_INTERVAL) {
		touch_dev->use_esd_timer = 1;
		ts_esd_timer_init(touch_dev);
		ts_esd_timer_start(ZINITIX_CHECK_ESD_TIMER, touch_dev);
		printk(KERN_INFO " ts_esd_timer_start\n");
	}


	sprintf(touch_dev->phys, "input(ts)");
	touch_dev->input_dev->name = ZINITIX_DRIVER_NAME;
	touch_dev->input_dev->id.bustype = BUS_I2C;
	touch_dev->input_dev->id.vendor = 0x0001;
	touch_dev->input_dev->phys = touch_dev->phys;
	touch_dev->input_dev->id.product = 0x0002;
	touch_dev->input_dev->id.version = 0x0100;

	set_bit(EV_SYN, touch_dev->input_dev->evbit);
	set_bit(EV_KEY, touch_dev->input_dev->evbit);
	set_bit(EV_ABS, touch_dev->input_dev->evbit);


	for (i = 0; i < MAX_SUPPORTED_BUTTON_NUM; i++)
		set_bit(BUTTON_MAPPING_KEY[i], touch_dev->input_dev->keybit);

	if (touch_dev->cap_info.Orientation & TOUCH_XY_SWAP) {
		input_set_abs_params(touch_dev->input_dev, ABS_MT_POSITION_Y,
			touch_dev->cap_info.MinX,
			480 + ABS_PT_OFFSET,
			0, 0);
		input_set_abs_params(touch_dev->input_dev, ABS_MT_POSITION_X,
			touch_dev->cap_info.MinY,
			800 + ABS_PT_OFFSET,
			0, 0);
	} else {
		input_set_abs_params(touch_dev->input_dev, ABS_MT_POSITION_X,
			touch_dev->cap_info.MinX,
			480 + ABS_PT_OFFSET,
			0, 0);
		input_set_abs_params(touch_dev->input_dev, ABS_MT_POSITION_Y,
			touch_dev->cap_info.MinY,
			800 + ABS_PT_OFFSET,
			0, 0);
	}

	input_set_abs_params(touch_dev->input_dev, ABS_TOOL_WIDTH,
		0, 255, 0, 0);
	input_set_abs_params(touch_dev->input_dev, ABS_MT_TOUCH_MAJOR,
		0, 255, 0, 0);
	input_set_abs_params(touch_dev->input_dev, ABS_MT_WIDTH_MAJOR,
		0, 255, 0, 0);

#if MULTI_PROTOCOL_TYPE_B
	set_bit(MT_TOOL_FINGER, touch_dev->input_dev->keybit);
	input_mt_init_slots(touch_dev->input_dev, touch_dev->cap_info.multi_fingers);
#else
	set_bit(BTN_TOUCH, touch_dev->input_dev->keybit);
#endif


	input_set_abs_params(touch_dev->input_dev, ABS_MT_TRACKING_ID, 
		0, touch_dev->cap_info.multi_fingers, 0, 0);
	

	zinitix_debug_msg("register %s input device \r\n",
		touch_dev->input_dev->name);
	ret = input_register_device(touch_dev->input_dev);
	if (ret) {
		printk(KERN_ERR "unable to register %s input device\r\n",
			touch_dev->input_dev->name);
		goto err_input_register_device;
	}

#ifdef TOUCH_VIRTUAL_KEYS
	zinitix_ts_virtual_keys_init();
#endif
	/* configure touchscreen interrupt gpio */
	ret = gpio_request(GPIO_TOUCH_PIN_NUM, "zinitix_irq_gpio");
	if (ret) {
		printk(KERN_ERR "unable to request gpio.(%s)\r\n",
			touch_dev->input_dev->name);		
		goto err_request_irq;
	}

	ret = gpio_direction_input(GPIO_TOUCH_PIN_NUM);

	touch_dev->int_gpio_num = GPIO_TOUCH_PIN_NUM;


	touch_dev->irq = gpio_to_irq(touch_dev->int_gpio_num);
	if (touch_dev->irq < 0)
		printk(KERN_INFO "error. gpio_to_irq(..) function is not \
			supported? you should define GPIO_TOUCH_IRQ.\r\n");

	zinitix_debug_msg("request irq (irq = %d, pin = %d) \r\n",
		touch_dev->irq, touch_dev->int_gpio_num);

	touch_dev->work_proceedure = TS_NO_WORK;
	sema_init(&touch_dev->work_proceedure_lock, 1);

#if USE_THREADED_IRQ
	ret = request_threaded_irq(touch_dev->irq, ts_int_handler, zinitix_touch_work, 
		IRQF_TRIGGER_FALLING | IRQF_ONESHOT , ZINITIX_DRIVER_NAME, touch_dev);
#else		
	ret = request_irq(touch_dev->irq, ts_int_handler,
		IRQF_TRIGGER_LOW, ZINITIX_DRIVER_NAME, touch_dev);
#endif
	if (ret) {
		printk(KERN_ERR "unable to register irq.(%s)\r\n",
			touch_dev->input_dev->name);
		goto err_request_irq;
	}
	dev_info(&client->dev, "zinitix touch probe.\r\n");

#ifdef CONFIG_HAS_EARLYSUSPEND
	touch_dev->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	touch_dev->early_suspend.suspend = zinitix_early_suspend;
	touch_dev->early_suspend.resume = zinitix_late_resume;
	register_early_suspend(&touch_dev->early_suspend);
#endif


	sema_init(&touch_dev->raw_data_lock, 1);

	misc_touch_dev = touch_dev;

	ret = misc_register(&touch_misc_device);
	if (ret)
		zinitix_debug_msg("Fail to register touch misc device.\n");

#if SEC_TOUCH_INFO_REPORT	
	//sys/class/misc/touch_misc_fops/....
	sec_touchscreen_dev = device_create(sec_class, NULL, 0, NULL, "sec_touchscreen");

	if (IS_ERR(sec_touchscreen_dev))
		pr_err("Failed to create device(sec_touchscreen)!\n");
	
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_get_touch_raw_data) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_get_touch_raw_data.attr.name);
	
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_touch_cal_ndata) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_touch_cal_ndata.attr.name);

	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_delta) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_delta.attr.name);
	
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_cal_ncount) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_cal_ncount.attr.name);
	
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_enter_testmode) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_enter_testmode.attr.name);
	
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_enter_normalmode) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_enter_normalmode.attr.name);
	
	if (device_create_file(sec_touchscreen_dev, &dev_attr_count_node) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_count_node.attr.name);
	
	if (device_create_file(sec_touchscreen_dev,
			&dev_attr_chip_info) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_chip_info.attr.name);
	
	if (device_create_file(sec_touchscreen_dev,
			    &dev_attr_touch_ndata) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_touch_ndata.attr.name);
		
	sec_touchkey_dev = device_create(sec_class, NULL, 0, NULL, "sec_touchkey");
	if (IS_ERR(sec_touchkey_dev))
		pr_err("Failed to create device(sec_touchkey)!\n");
	
	if (device_create_file(sec_touchkey_dev, &dev_attr_tkey_data) < 0)
		pr_err("Failed to create device file(%s)!\n",
				dev_attr_tkey_data.attr.name);
	
	if (device_create_file(sec_touchkey_dev, &dev_attr_menu_sensitivity) < 0)
		pr_err("Failed to create device file(%s)!\n",
					dev_attr_menu_sensitivity.attr.name);
	if (device_create_file(sec_touchkey_dev, &dev_attr_back_sensitivity) < 0)
		pr_err("Failed to create device file(%s)!\n",
			dev_attr_back_sensitivity.attr.name);		
#endif


	return 0;

err_request_irq:
	input_unregister_device(touch_dev->input_dev);
err_input_register_device:
	input_free_device(touch_dev->input_dev);
err_kthread_create_failed:
err_input_allocate_device:
	kfree(touch_dev);
err_alloc_dev_data:
err_check_functionality:
	touch_dev = NULL;

	misc_touch_dev = NULL;

	i2c_set_clientdata(client, touch_dev);
	ts_power_control(POWER_OFF);
	return -ENODEV;
}


static int zinitix_touch_remove(struct i2c_client *client)
{
	struct zinitix_touch_dev *touch_dev = i2c_get_clientdata(client);
	if(touch_dev == NULL)	return 0;

	zinitix_debug_msg("zinitix_touch_remove+ \r\n");
	down(&touch_dev->work_proceedure_lock);
#if !USE_THREADED_IRQ	
	if (touch_dev->work_proceedure != TS_NO_WORK)
		flush_work(&touch_dev->work);
#endif
	touch_dev->work_proceedure = TS_REMOVE_WORK;

	if (touch_dev->use_esd_timer != 0) {
		flush_work(&touch_dev->tmr_work);
		ts_write_reg(touch_dev->client,
			ZINITIX_PERIODICAL_INTERRUPT_INTERVAL, 0);
		ts_esd_timer_stop(touch_dev);
		zinitix_debug_msg(KERN_INFO " ts_esd_timer_stop\n");
		destroy_workqueue(zinitix_tmr_workqueue);
	}

	if (touch_dev->irq)
		free_irq(touch_dev->irq, touch_dev);

#if SEC_TOUCH_INFO_REPORT
	//device_remove_file(touch_misc_device.this_device, &dev_attr_get_touch_raw_data); --> remove attr
	//.....
#endif
	misc_deregister(&touch_misc_device);



#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&touch_dev->early_suspend);
#endif

#if !USE_THREADED_IRQ
	destroy_workqueue(zinitix_workqueue);
#endif
	if (gpio_is_valid(touch_dev->int_gpio_num) != 0)
		gpio_free(touch_dev->int_gpio_num);

	input_unregister_device(touch_dev->input_dev);
	input_free_device(touch_dev->input_dev);
	up(&touch_dev->work_proceedure_lock);
	kfree(touch_dev);

	return 0;
}


static struct i2c_driver zinitix_touch_driver = {
	.probe	= zinitix_touch_probe,
	.remove	= zinitix_touch_remove,
	.id_table	= zinitix_idtable,
	.driver		= {
		.owner	= THIS_MODULE,
		.name	= ZINITIX_DRIVER_NAME,
		.pm	= &zinitix_dev_pm_ops,
	},
};


static int __devinit zinitix_touch_init(void)
{

	m_isp_client = NULL;
	return i2c_add_driver(&zinitix_touch_driver);
}

static void __exit zinitix_touch_exit(void)
{
	i2c_del_driver(&zinitix_touch_driver);
}

module_init(zinitix_touch_init);
module_exit(zinitix_touch_exit);

MODULE_DESCRIPTION("zinitix touch-screen device driver using i2c interface");
MODULE_AUTHOR("sohnet <seonwoong.jang@zinitix.com>");
MODULE_LICENSE("GPL");


