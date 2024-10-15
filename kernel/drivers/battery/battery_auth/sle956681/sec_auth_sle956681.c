/*
 * sec_auth_sle956681.c
 * Samsung Mobile Battery Authentication Driver
 *
 * Copyright (C) 2023 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/workqueue.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/delay.h>

#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/mutex.h>
#include <linux/random.h>
#include <linux/time.h>
#include <linux/unistd.h>
#include <linux/vmalloc.h>
#include <linux/interrupt.h>
#include <linux/string.h>

#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/ktime.h>
#include <linux/cdev.h>
#include <linux/power_supply.h>
#include<linux/kobject.h>

#include "libs/authon-sdk/authon_config.h"
#include "libs/authon-sdk/authon_api.h"
#include "libs/authon-sdk/authon_status.h"
#include "libs/authon-sdk/platform/helper.h"
#include "libs/authon-sdk/ecc/ecc.h"
#include "libs/authon-sdk/host_auth/host_auth.h"
#include "sec_auth_sle956681.h"
#include "../../common/sec_charging_common.h"

#if !defined(CONFIG_SEC_FACTORY)
static uint32_t slavepcb;
module_param(slavepcb, uint, 0644);
MODULE_PARM_DESC(slavepcb, "slave pcb value");

static char *sales_code;
module_param(sales_code, charp, 0644);
MODULE_PARM_DESC(sales_code, "sales code value");

static unsigned int eur_slavepcb;
#endif

/* GPIO variable */
int SWI_GPIO_PIN = 0;
uint8_t active_device = 0;

/* Chip Name */
static const char *CHIP_NAME = "sle956681";

uint32_t Highspeed_BaudLow;
uint32_t Highspeed_BaudHigh;
uint32_t Highspeed_BaudStop;
uint32_t Highspeed_BaudLow1;
uint32_t Highspeed_BaudHigh1;
uint32_t Highspeed_BaudStop1;
uint32_t Highspeed_BaudLow2;
uint32_t Highspeed_BaudHigh2;
uint32_t Highspeed_BaudStop2;
uint32_t Highspeed_ResponseTimeOut;
uint32_t PowerDownDelay_Time;
uint32_t PowerUpDelay_Time;

// Schedule work delay in sec
static int work_sched_delay = MAX_WORK_DELAY;
#define DEBUG 0
#define MAX_1TAU     153
#define MAX_3TAU     (3*MAX_1TAU)
#define MAX_5TAU     (5*MAX_1TAU)
#define MAX_10TAU    (10*MAX_1TAU)

enum _E_ICCTL_CMD {
	SWI_DEV_POWERUP = 0x20,
	SWI_DEV_POWERDOWN = 0x21,
	SWI_DEV_RESET = 0x22,
	SWI_SET_ACTIVE = 0x23,
	SWI_GET_ACTIVE = 0x24,

	SWI_REG_GET_VALUE = 0x70,
	SWI_REG_GET_LOCK_VALUE = 0x72,

	SWI_GET_UID = 0x80,

	SWI_NVM_READ = 0x90, //NVM related command space
	SWI_NVM_WRITE = 0x91,
	SWI_NVM_SET_PAGE_LOCK = 0x92,
	SWI_NVM_SYNC = 0x93,
	SWI_NVM_SYNC_STATUS = 0x94,

	SWI_HA_GET_RAND_A = 0xA0,
	SWI_HA_SEND_RAND_B = 0xA1,
	SWI_HA_SEND_TAG_A = 0xA2,
	SWI_HA_GET_TAG_B = 0xA3,

	SWI_ECC_GET_CERT = 0xB0,
	SWI_ECC_SEND_CHALLENGE_GET_RESPONSE = 0xB1,
	SWI_ECC_SEND_MAC_BYTE = 0xB2,

	AUTHENTICATION_STATUS = 0xC0,
	INIT_PAGE_BUFFER = 0xC1,
	GET_AUTH_IC_LIST = 0xC2,

} E_IOCTL_CMD;


struct AUTH_ON_S {
	uint8_t UID[12];
	uint32_t ODC[13];
	uint32_t pubKey[7];
	uint8_t challenge[21];
	uint32_t xResponse[7];
	uint32_t zResponse[7];
	uint8_t nvm_data[256];
	uint8_t ub_nonceA[MAC_BYTE_LEN];
	uint8_t ub_nonceB[MAC_BYTE_LEN];
	uint8_t ub_tagA[MAC_BYTE_LEN];
	uint8_t ub_tagB[MAC_BYTE_LEN];
	uint8_t cert[12+72];
	uint8_t response[44];
	struct completion	event;
};

unsigned long authon_driver_flag;
#define IN_USE_BIT 0
#define RETRY_COUNT 20

int sle956681_init_complete;
EXPORT_SYMBOL(sle956681_init_complete);

struct AUTH_ON_S *authon_handler;

static struct authon_driver_data *g_auth_driv;
static uint8_t page_data_buf[256]; /* Buffer to store nvm data in sec_auth_sle956681 driver */
static uint8_t page_data_buf2[256]; /* Buffer2 to store nvm data in sec_auth_sle956681 driver */
static bool pageBufDirty[64];
static bool pageBufDirty2[64];
static bool is_authentic, is_page_buf_init;
static bool is_authentic2, is_page_buf_init2;
static uint8_t auth_ic_list;
static int sync_status;
static int sync_status2;
static bool nvm_sync_status;

struct authon_cisd *authon_cisd_data;
EXPORT_SYMBOL(authon_cisd_data);

struct authon_ops {
	struct module *owner;
	struct device *dev;	/* for dev_dbg() or dev_err support */
};

static struct authon_ops authon_driver_ops = {
	.owner = THIS_MODULE,
};

static int authon_driver_release(struct inode *inode, struct file *file);
static int authon_driver_open(struct inode *s_inode, struct file *s_file);
static long authon_driver_ioctl(struct file *s_file, unsigned int ui_command, unsigned long ul_arg);
static struct class *authon_class;

static struct cdev *mcdev; // this is use for our character devices driver
static int major_num; // to extract major num and to prompt user
static int reg_count_major; // for returning error purposes
static int authon_driver_open_count;
dev_t dev_num; // will hold the major number or minor number

char *sync_event[] = { "AUTH_SYNC=1", NULL };
char *sync_event2[] = { "AUTH_SYNC=2", NULL };
char *use_date_wlock_event[] = { "USE_DATE_WLOCK=1", NULL };
char *use_date_wlock2_event[] = { "USE_DATE_WLOCK=2", NULL };
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
char *auth_pass_event[] = { "AUTH_PASS=1", NULL };
#endif

static struct kobject *nvm_write_kobj;

static const struct file_operations authon_driver_fops = {
	.owner = THIS_MODULE,
	.open = authon_driver_open,
	.release = authon_driver_release,
	.unlocked_ioctl = authon_driver_ioctl,
	.llseek = no_llseek,
};

struct MESSAGE
{
	uint8_t nvm_data[256];
	uint8_t offset;
	uint8_t page_count;
	uint16_t status;
};
typedef struct MESSAGE Message;

struct ECC_MESSAGE
{
	uint8_t cert[12+72];
	uint8_t Challenge[21];
	uint8_t response[44];
	uint8_t key_slot;
	uint16_t status;
	uint8_t mac_byte[10];
};
typedef struct ECC_MESSAGE ECC_Message;

struct REG_MESSAGE
{
	uint16_t address;
	uint8_t length;
	uint8_t reg_data[256];
	uint16_t status;
};
typedef struct REG_MESSAGE REG_Message;

struct LOCK_MESSAGE
{
	uint8_t user_sts_1;
	uint8_t user_sts_2;
	uint8_t user_sts_3;
	uint8_t user_sts_4;
	uint8_t user_sts_5;
	uint8_t user_sts_6;
	uint8_t user_sts_7;
	uint8_t user_sts_8;
	uint8_t LSC_feat_sts;
	uint8_t Kill_sts;
	uint8_t LSC1[4];
	uint8_t LSC2[4];
	uint8_t ack;
};
typedef struct LOCK_MESSAGE LOCK_Message;

extern AuthOn_Capability authon_capability;
AuthOn_Capability *authon_capability_handle = &authon_capability;
extern AuthOn_Enumeration authon_enumeration;

#if !defined(CONFIG_SEC_FACTORY)
#define SALE_CODE_STR_LEN		3
static bool is_sales_code(const char *str)
{
	if (str == NULL || sales_code == NULL)
		return 0;

	pr_info("%s: %s\n", __func__, sales_code);
	return !strncmp(sales_code, str, SALE_CODE_STR_LEN + 1);
}

static int check_authic_supported(struct device_node *np)
{
	int ret = 0;
	int eur_det_gpio = 0;
	int sub6_det_gpios[3];
	int gpio_val[3] = {-1, -1, -1};

	/* Method 1 : sub6_det_gpios */
	sub6_det_gpios[0] = of_get_named_gpio(np, "authon,sub6_det_gpio1", 0); // gpio_r1
	sub6_det_gpios[1] = of_get_named_gpio(np, "authon,sub6_det_gpio2", 0); // gpio_r2
	sub6_det_gpios[2] = of_get_named_gpio(np, "authon,sub6_det_gpio3", 0); // gpio_r3

	if (sub6_det_gpios[0] < 0 || sub6_det_gpios[1] < 0 ||
		sub6_det_gpios[2] < 0) {
		pr_info("%s: sub6_det gpios not found!\n", __func__);
	} else {
		gpio_val[0] = gpio_get_value(sub6_det_gpios[0]); // gpio_r1
		gpio_val[1] = gpio_get_value(sub6_det_gpios[1]); // gpio_r2
		gpio_val[2] = gpio_get_value(sub6_det_gpios[2]); // gpio_r3

		/* E1_EUR dev => r3 r2 r1 = H H L */
		if (!(gpio_val[2] && gpio_val[1] && !(gpio_val[0]))) {
			pr_err("%s: not valid auth IC device, gpio_val2(%d), gpio_val1(%d), gpio_val0(%d)\n",
			__func__, gpio_val[2], gpio_val[1], gpio_val[0]);
			return -1;
		}
		goto AUTH_IC_DEVICE;
	}

	/* Method 2 : PMIC gpio
	 * Pull down i.e. value 0 -> EUR
	 * Pull up i.e. value 1 -> Non EUR
	 */
	eur_det_gpio = of_get_named_gpio(np, "authon,eur_detection", 0);
	if (eur_det_gpio < 0) {
		pr_info("%s: 'authon,eur_detection' not found\n", __func__);
	} else {
		ret = gpio_get_value(eur_det_gpio);
		if (ret != 0) {
			pr_err("%s: not valid auth IC device, gpio_val(%d)\n", __func__, ret);
			return -1;
		}
		goto AUTH_IC_DEVICE;
	}

	/* Method 3 : Read project and region specific slavepcb value */
	ret = of_property_read_u32(np, "samsung,eur_slavepcb", &eur_slavepcb);
	if (ret) {
		pr_err("%s: 'samsung,eur_slavepcb' not found\n", __func__);
	} else {
		/*  Do not proceed if
		 *	1. slacepcb != EUR_SLAVEPCB &&
		 *	2. sales_code != TUR
		 */
		if (slavepcb != eur_slavepcb &&
			!is_sales_code("TUR")) {
			pr_info("%s Neither EUR device nor TUR region, return\n", __func__);
			return -1;
		}
		goto AUTH_IC_DEVICE;
	}

AUTH_IC_DEVICE:
	pr_info("%s: Auth IC device, proceed further...\n", __func__);
	return 0;
}
#endif

static void sec_auth_poll_work_func(struct work_struct *work)
{
	int err = 0;
	u64 elapsed_time;
	u64 current_time = ktime_get_ns();
	union power_supply_propval value = {0, };

	if (current_time < g_auth_driv->sync_time)
		elapsed_time = U64_MAX - (g_auth_driv->sync_time) + current_time;
	else
		elapsed_time = current_time - (g_auth_driv->sync_time);

	pr_info("%s: current(%llu), last_update(%llu), elapsed(%llu)\n",
			__func__, current_time, g_auth_driv->sync_time, elapsed_time);

	psy_do_property("battery", get, POWER_SUPPLY_PROP_STATUS, value);
	if (((value.intval == POWER_SUPPLY_STATUS_CHARGING ||
			value.intval == POWER_SUPPLY_STATUS_FULL) && (elapsed_time > HOUR_NS))
		|| (elapsed_time > DAY_NS)) {
		err = kobject_uevent_env(nvm_write_kobj, KOBJ_CHANGE, sync_event);

		if (err < 0)
			pr_err("Error sending uevent for nvm_write kobject (%d)\n", err);

		err = kobject_uevent_env(nvm_write_kobj, KOBJ_CHANGE, sync_event2);

		if (err < 0)
			pr_err("Error sending uevent2 for nvm_write kobject (%d)\n", err);

		g_auth_driv->sync_time = current_time;
	}

	queue_delayed_work(g_auth_driv->sec_auth_poll_wqueue, &g_auth_driv->sec_auth_poll_work, work_sched_delay * HZ);
}

/**
 * @brief Examples of I/O requests from userspace
 * */
long authon_driver_ioctl(struct file *s_file, unsigned int ui_command, unsigned long ul_arg)
{
	uint8_t i = 0;
	int x = 0;
	long lResult = 0;
	int retry=RETRY_COUNT;
	uint16_t ret = 0;

	uint16_t regaddr;
	uint8_t rd_data;
	uint8_t busy=1;
	uint8_t timeout=100;
	uint8_t RE_READ_COUNT = 10;

	uint8_t eccpublicKey[PUBLICKEY_BYTE_LEN];
	uint8_t gf2nODC[((ODC_BYTE_LEN + PAGE_SIZE_BYTE - 1) / PAGE_SIZE_BYTE) * PAGE_SIZE_BYTE];

	uint8_t challenge[ECC_CHALLENGE_LEN] = {0};
	uint8_t ubp_resp[ECC_RESPONSE_LEN] = {0};

	uint8_t uid[UID_SIZE_BYTE];
	uint8_t tid[2];
	uint8_t pid[2];

	uint8_t nvm_page_lock;

	unsigned long flags;

	REG_Message *rrmsg = kmalloc(sizeof(struct REG_MESSAGE), GFP_KERNEL);
	ECC_Message *ecc_msg = kmalloc(sizeof(struct ECC_MESSAGE), GFP_KERNEL);
	REG_Message *wrmsg = kmalloc(sizeof(struct REG_MESSAGE), GFP_KERNEL);
	LOCK_Message *lockmsg = kmalloc(sizeof(struct LOCK_MESSAGE), GFP_KERNEL);
	Message *rmsg = kmalloc(sizeof(struct MESSAGE), GFP_KERNEL);
	Message *wmsg = kmalloc(sizeof(struct MESSAGE), GFP_KERNEL);
	ECC_Message *ecc_msg1 = kmalloc(sizeof(struct ECC_MESSAGE), GFP_KERNEL);

	pr_info("ui_command %x \r\n", ui_command);

	switch (ui_command) {
	case SWI_DEV_POWERUP:
		pr_info("SWI_DEV_POWERUP:\n");
		ret = authon_exe_power_up();
		if(ret!=EXE_SUCCESS){
			pr_err("Error: device power up\n");
			lResult = -EFAULT;
			goto cleanup;
		}
		break;
	case SWI_DEV_POWERDOWN:
		pr_info("SWI_DEV_POWERDOWN:\n");
		ret = authon_exe_power_down();
		if(ret!=EXE_SUCCESS){
			pr_err("Error: device power down\n");
			lResult = -EFAULT;
			goto cleanup;
		}

		break;
	case SWI_DEV_RESET:
		pr_info("SWI_DEV_RESET:\n");

		ret = authon_exe_reset();
		if(ret!=EXE_SUCCESS){
			pr_err("Error: device reset\n");
			lResult = -EFAULT;
			goto cleanup;
		}

		ret = authon_exe_power_down();
		if(ret!=EXE_SUCCESS){
			pr_err("Error: device power down\n");
			lResult = -EFAULT;
			goto cleanup;
		}

		ret = authon_exe_power_up();
		if(ret!=EXE_SUCCESS){
			pr_err("Error: device power up\n");
			lResult = -EFAULT;
			goto cleanup;
		}

		Send_EDA(0);
		Send_SDA(ON_DEFAULT_ADDRESS);
		break;

	case SWI_SET_ACTIVE:
		if (copy_from_user(&active_device, (uint32_t __user *)ul_arg, sizeof(active_device))!= 0){
			pr_err("Error: failed to get active device");
		} else {
			pr_info("Selected Active interface:%d\r\n", active_device);
		}

		switch (active_device) {
			case 1:
				SWI_GPIO_PIN = g_auth_driv->pdata->swi_gpio[0];
				pr_info("Selected Active interface 1\r\n");
				Highspeed_BaudLow = Highspeed_BaudLow1;
				Highspeed_BaudHigh = Highspeed_BaudHigh1;
				Highspeed_BaudStop = Highspeed_BaudStop1;
				break;
			case 2:
				SWI_GPIO_PIN = g_auth_driv->pdata->swi_gpio[1];
				pr_info("Selected Active interface 2\r\n");
				Highspeed_BaudLow = Highspeed_BaudLow2;
				Highspeed_BaudHigh = Highspeed_BaudHigh2;
				Highspeed_BaudStop = Highspeed_BaudStop2;
				break;
			default:
				active_device = 1;
				SWI_GPIO_PIN = g_auth_driv->pdata->swi_gpio[0];
				pr_info("Default Active interface 1\r\n");
				Highspeed_BaudLow = Highspeed_BaudLow1;
				Highspeed_BaudHigh = Highspeed_BaudHigh1;
				Highspeed_BaudStop = Highspeed_BaudStop1;
				break;
		}

		pr_info("Highspeed_BaudLow: %d\n", Highspeed_BaudLow);
		pr_info("Highspeed_BaudHigh: %d\n", Highspeed_BaudHigh);
		pr_info("Highspeed_BaudStop: %d\n", Highspeed_BaudStop);
		break;

	case SWI_GET_ACTIVE:
		if (copy_to_user((uint32_t __user *) ul_arg, &active_device, sizeof(active_device)) != 0) {
			pr_info("Error: Unable to copy active device");
		}
		pr_info("Active interface: %x\r\n", active_device);
		break;

	case SWI_GET_UID:
		memset(authon_capability_handle, 0, sizeof(AuthOn_Capability));
		authon_capability_handle->swi.gpio = SWI_GPIO_PIN;

		while (retry) {
			ret = authon_init_sdk(authon_capability_handle);
			if(ret!= SDK_SUCCESS){
				pr_err("Error: failed to initialize SDK ret=0x%X retry:%d", ret, retry);
			}else{
				pr_info("Found %d device(s) on the bus\r\n", authon_enumeration.device_found);
				if(authon_enumeration.device_found!=0){
					if (copy_to_user((uint32_t __user *) ul_arg, &(authon_capability.uid[0][0]),  UID_BYTE_LEN) != 0) {
						pr_err("Error: Get UID.\n");
						lResult = -EFAULT;
						goto cleanup;
					}else{
						lResult = 0;
						break;
					}
				}else{
					pr_err("Error: Device not found retry_left:%d\r\n", retry);
				}
			}
			retry--;
		}

		break;

	case SWI_REG_GET_VALUE:
		pr_info("SWI_REG_GET_VALUE:\n");
		if (copy_from_user(rrmsg, (uint32_t __user *)ul_arg, sizeof(struct REG_MESSAGE))!=0 ){
    		lResult = -EFAULT;
			goto cleanup;
		}

		//pr_info("SWI_REG_GET_VALUE: 0x%x 0x%x\r\n", rrmsg->address, rrmsg->length);

		ret= authon_read_swi_sfr( rrmsg->address, rrmsg->reg_data, rrmsg->length);
		if(ret!=SDK_INTERFACE_SUCCESS){
			pr_err("Error: failed to read reg ret=0x%X ", ret);
			lResult = -EFAULT;
			goto cleanup;
		}

		if (copy_to_user((uint32_t __user *) ul_arg, rrmsg, sizeof(struct REG_MESSAGE)) != 0) {
			pr_err("Error: Unable to read NVM data. ret=%x", ret);
			lResult = -EFAULT;
			goto cleanup;
		}

		break;
	
	case SWI_NVM_SET_PAGE_LOCK:
		pr_info("SWI_NVM_SET_PAGE_LOCK:\n");
		if (copy_from_user(&nvm_page_lock, (uint32_t __user *)ul_arg, sizeof(nvm_page_lock))!=0 ){
			pr_err("Error: failed to get nvm page set lock");
		}else{
			pr_info("Selected NVM page lock:%d\r\n", nvm_page_lock);
		}

		if(nvm_page_lock>64){
			pr_err("Error: invalid nvm lock page");
			goto cleanup;
		}else{
			ret = authon_set_nvm_page_lock(nvm_page_lock);
			if(ret!= APP_NVM_SUCCESS){
				pr_err("Error: unable to lock the NVM page");
			}else{
				pr_err("NVM page done");
			}
		}

		break;

	case SWI_REG_GET_LOCK_VALUE:
		pr_info("SWI_REG_LOCK_VALUE:\n");
		retry=RE_READ_COUNT;
		lockmsg->ack=0xff;
		do{
			ret= authon_read_swi_sfr( ON_SFR_USER_NVM_LOCK_STS_1, &(lockmsg->user_sts_1), 1);
			if(ret==SDK_SUCCESS){
				pr_info("ON_SFR_USER_NVM_LOCK_STS_1: 0x%x \r\n", lockmsg->user_sts_1);
				lockmsg->ack=0x00;
				break;
				}
			retry--;
		}while(retry);

		if(retry==0){
			pr_err("error: read ON_SFR_USER_NVM_LOCK_STS_1 ret=%x \r\n", ret);
			goto Send_Status;
		}
		

		retry=RE_READ_COUNT;
		lockmsg->ack=0xff;
		do{
			ret= authon_read_swi_sfr( ON_SFR_USER_NVM_LOCK_STS_2, &(lockmsg->user_sts_2), 1);
			if(ret==SDK_SUCCESS){
				pr_info("ON_SFR_USER_NVM_LOCK_STS_2: 0x%x \r\n", lockmsg->user_sts_2);
				lockmsg->ack=0x00;
				break;
				}
			retry--;
		}while(retry);
		if(retry==0){
			pr_err("error: read ON_SFR_USER_NVM_LOCK_STS_2 ret=%x \r\n", ret);
			goto Send_Status;
		}

		retry=RE_READ_COUNT;
		lockmsg->ack=0xff;
		do{
			ret= authon_read_swi_sfr( ON_SFR_USER_NVM_LOCK_STS_3, &(lockmsg->user_sts_3), 1);
			if(ret==SDK_SUCCESS){
				pr_info("ON_SFR_USER_NVM_LOCK_STS_3: 0x%x \r\n", lockmsg->user_sts_3);
				lockmsg->ack=0x00;
				break;
				}
			retry--;
		}while(retry);

		if(retry==0){
			pr_err("error: read ON_SFR_USER_NVM_LOCK_STS_3 ret=%x \r\n", ret);
			goto Send_Status;
		}

		retry=RE_READ_COUNT;
		lockmsg->ack=0xff;
		do{
			ret= authon_read_swi_sfr( ON_SFR_USER_NVM_LOCK_STS_4, &(lockmsg->user_sts_4), 1);
			if(ret==SDK_SUCCESS){
				pr_info("ON_SFR_USER_NVM_LOCK_STS_4: 0x%x \r\n", lockmsg->user_sts_4);
				lockmsg->ack=0x00;
				break;
				}
			retry--;
		}while(retry);
		
		if(retry==0){
			pr_err("error: read ON_SFR_USER_NVM_LOCK_STS_4 ret=%x \r\n", ret);
			goto Send_Status;
		}

		retry=RE_READ_COUNT;
		lockmsg->ack=0xff;
		do{
			ret= authon_read_swi_sfr( ON_SFR_USER_NVM_LOCK_STS_5, &(lockmsg->user_sts_5), 1);
			if(ret==SDK_SUCCESS){
				pr_info("ON_SFR_USER_NVM_LOCK_STS_5: 0x%x \r\n", lockmsg->user_sts_5);
				lockmsg->ack=0x00;
				break;
				}
			retry--;
		}while(retry);

		if(retry==0){
			pr_err("error: read ON_SFR_USER_NVM_LOCK_STS_5 ret=%x \r\n", ret);
			goto Send_Status;
		}

		retry=RE_READ_COUNT;
		lockmsg->ack=0xff;
		do{
			ret= authon_read_swi_sfr( ON_SFR_USER_NVM_LOCK_STS_6, &(lockmsg->user_sts_6), 1);
			if(ret==SDK_SUCCESS){
				pr_info("ON_SFR_USER_NVM_LOCK_STS_6: 0x%x \r\n", lockmsg->user_sts_6);
				lockmsg->ack=0x00;
				break;
				}
			retry--;
		}while(retry);
		if(retry==0){
			pr_err("error: read ON_SFR_USER_NVM_LOCK_STS_6 ret=%x \r\n", ret);
			goto Send_Status;
		}

		retry=RE_READ_COUNT;
		lockmsg->ack=0xff;
		do{
			ret= authon_read_swi_sfr( ON_SFR_USER_NVM_LOCK_STS_7, &(lockmsg->user_sts_7), 1);
			if(ret==SDK_SUCCESS){
				pr_info("ON_SFR_USER_NVM_LOCK_STS_7: 0x%x \r\n", lockmsg->user_sts_7);
				lockmsg->ack=0x00;
				break;
				}
			retry--;
		}while(retry);

		if(retry==0){
			pr_err("error: read ON_SFR_USER_NVM_LOCK_STS_7 ret=%x \r\n", ret);
			goto Send_Status;
		}

		retry=RE_READ_COUNT;
		lockmsg->ack=0xff;
		do{
			ret= authon_read_swi_sfr( ON_SFR_USER_NVM_LOCK_STS_8, &(lockmsg->user_sts_8), 1);
			if(ret==SDK_SUCCESS){
				pr_info("ON_SFR_USER_NVM_LOCK_STS_8: 0x%x \r\n", lockmsg->user_sts_8);
				lockmsg->ack=0x00;
				break;
				}
			retry--;
		}while(retry);
		
		if(retry==0){
			pr_err("error: read ON_SFR_USER_NVM_LOCK_STS_8 ret=%x \r\n", ret);
			goto Send_Status;
		}

		retry=RE_READ_COUNT;
		lockmsg->ack=0xff;
		do{
			ret= authon_read_swi_sfr( ON_SFR_LSC_FEAT_STS, &(lockmsg->LSC_feat_sts), 1);
			if(ret==SDK_SUCCESS){
				pr_info("ON_SFR_LSC_FEAT_STS: 0x%x \r\n", lockmsg->LSC_feat_sts);
				lockmsg->ack=0x00;
				break;
				}
			retry--;
		}while(retry);

		if(retry==0){
			pr_err("error: read sfr ON_SFR_LSC_FEAT_STS ret=%x \r\n", ret);
			goto Send_Status;
		}

		retry=RE_READ_COUNT;
		lockmsg->ack=0xff;
		do{
			ret= authon_read_swi_sfr( ON_SFR_LSC_KILL_ACT_STS, &(lockmsg->Kill_sts), 1);
			if(ret==SDK_SUCCESS){
				pr_info("ON_SFR_LSC_KILL_ACT_STS: 0x%x \r\n", lockmsg->Kill_sts);
				lockmsg->ack=0x00;
				break;
				}
			retry--;
		}while(retry);

		if(retry==0){
			pr_err("error: read ON_SFR_LSC_KILL_ACT_STS ret=%x \r\n", ret);
			goto Send_Status;
		}

		retry=RE_READ_COUNT;
		lockmsg->ack=0xff;
		do{			
			ret = authon_read_nvm(ON_LSC_1, lockmsg->LSC1, 1);
			if(ret==SDK_SUCCESS){
				pr_info("lockmsg->LSC1: %.2x %.2x %.2x %.2x \r\n", lockmsg->LSC1[0], lockmsg->LSC1[1], lockmsg->LSC1[2], lockmsg->LSC1[3] );
				lockmsg->ack=0x00;
				break;
				}
			retry--;
		}while(retry);

		if(retry==0){
			pr_err("error: read LSC1 %x \r\n", ret);
			goto Send_Status;
		}

		retry=RE_READ_COUNT;
		lockmsg->ack=0xff;
		do{
			ret = authon_read_nvm(ON_LSC_2, lockmsg->LSC2, 1);
			if(ret==SDK_SUCCESS){
				pr_info("lockmsg->LSC2: %.2x %.2x %.2x %.2x \r\n", lockmsg->LSC2[0], lockmsg->LSC2[1], lockmsg->LSC2[2], lockmsg->LSC2[3] );
				lockmsg->ack=0x00;
				break;
				}
			retry--;
		}while(retry);

		if(retry==0){
			pr_err("error: read LSC2 %x \r\n", ret);
			goto Send_Status;
		}

Send_Status:
		if (copy_to_user((uint32_t __user *) ul_arg, lockmsg, sizeof(struct LOCK_MESSAGE)) != 0) {
			pr_err("Error: Unable to read register data. ret=%x", ret);
			lResult = -EFAULT;
			goto cleanup;
		}
		break;
	case SWI_NVM_READ:
		pr_info("SWI_NVM_READ:\n");
		if (copy_from_user(rmsg, (uint32_t __user *)ul_arg, sizeof(struct MESSAGE))!=0 ){
    		lResult = -EFAULT;
			goto cleanup;
		}

		pr_info("SWI_NVM_READ: %d %d\n", rmsg->page_count, rmsg->offset);
		ret = authon_read_nvm(rmsg->offset, rmsg->nvm_data, rmsg->page_count);
		if(APP_NVM_SUCCESS != ret){
			pr_err("Error: Read NVM failed, ret=0x%x\r\n", ret);
			rmsg->status = ret;
			lResult = -EFAULT;
			goto cleanup;
		}else{
			rmsg->status = 0x0;
		}
		
		if (copy_to_user((uint32_t __user *) ul_arg, rmsg, sizeof(struct MESSAGE)) != 0) {
			pr_err("ERROR: Unable to read NVM data. ret=%x", ret);
			rmsg->status = ret;
			lResult = -EFAULT;
			goto cleanup;
		} else {
			rmsg->status = 0x0;
			lResult = 0;
		}
		break;

	case SWI_NVM_WRITE:
		pr_info("SWI_NVM_WRITE:\n");
		if (copy_from_user(wmsg, (uint32_t __user *)ul_arg, sizeof(struct MESSAGE))!=0 ){
    		lResult = -EFAULT;
			goto cleanup;
		}

		Send_EDA(0);
		Send_SDA(ON_DEFAULT_ADDRESS);
		
		pr_info("SWI_NVM_WRITE: %d %d\n", wmsg->offset, wmsg->page_count);
	
		ret = authon_write_nvm(wmsg->offset, wmsg->nvm_data, wmsg->page_count);
		if(ret == APP_NVM_PAGE_LOCKED){
			pr_err("NVM page is locked\n");
			wmsg->status = APP_NVM_PAGE_LOCKED;
			goto Send_NVM_Status;
		}

		if(APP_NVM_SUCCESS != ret){
			pr_err("Error: Write NVM failed, ret=0x%x remain retry: %d\r\n", ret, retry);				
			lResult = -EFAULT;
			wmsg->status = ret;
			goto cleanup;
		}else{
			wmsg->status = 0x0;
			lResult = 0;
		}

Send_NVM_Status:
		if (copy_to_user((uint32_t __user *) ul_arg, wmsg, sizeof(struct MESSAGE)) != 0) {
			pr_err("Error: Unable to write NVM data. ret=%x", ret);
			wmsg->status = ret;
			lResult = -EFAULT;
			goto cleanup;
		}
		break;

	case SWI_NVM_SYNC:
		pr_info("SWI_NVM_SYNC:\n");

		if (copy_from_user(&active_device, (uint32_t __user *)ul_arg, sizeof(active_device)) != 0)
			pr_err("Error: failed to get active device");
		else
			pr_info("Selected Active interface:%d\n", active_device);

		if (active_device == 1)
			sync_status = SYNC_PROGRESS;
		else if (active_device == 2)
			sync_status2 = SYNC_PROGRESS;

		Send_EDA(0);
		Send_SDA(ON_DEFAULT_ADDRESS);

		/* Sync buffer to memory : Pages 8 to 41 */
		for (i = SYNC_START_PAGE; i <= SYNC_END_PAGE; i++) {
			if (active_device == 1) {
				if (pageBufDirty[i] == false)
					continue;
				ret = authon_write_nvm(i, page_data_buf + (i * 4), 1);
				if (ret == APP_NVM_PAGE_LOCKED)
					pr_err("Active device %d NVM page %d is locked\n", active_device, i);
				else if (ret != APP_NVM_SUCCESS) {
					lResult = -EFAULT;
					pr_err("Error: Active device %d NVM page %d Sync NVM failed, ret=0x%x\n",
							active_device, i, ret);
					goto cleanup;
				} else
					pr_info("Active device %d NVM page %d Sync NVM Success\n", active_device, i);
			} else if (active_device == 2) {
				if (pageBufDirty2[i] == false)
					continue;
				ret = authon_write_nvm(i, page_data_buf2 + (i * 4), 1);
				if (ret == APP_NVM_PAGE_LOCKED)
					pr_err("Active device %d NVM page %d is locked\n", active_device, i);
				else if (ret != APP_NVM_SUCCESS) {
					lResult = -EFAULT;
					pr_err("Error: Active device %d NVM page %d Sync NVM failed, ret=0x%x\n",
							active_device, i, ret);
					goto cleanup;
				} else
					pr_info("Active device %d NVM page %d Sync NVM Success\n", active_device, i);
			}
		}
		break;

	case SWI_NVM_SYNC_STATUS:
		if (copy_from_user(&nvm_sync_status, (uint32_t __user *)ul_arg, sizeof(bool)) != 0) {
			lResult = -EFAULT;
			pr_err("Error: Unable to copy NVM Sync status");
			goto cleanup;
		}

		if (nvm_sync_status) {
			if (active_device == 1) {
				sync_status = SYNC_SUCCESS;
				for (x = 0; x < 64; x++)
					pageBufDirty[x] = false;
			} else if (active_device == 2) {
				sync_status2 = SYNC_SUCCESS;
				for (x = 0; x < 64; x++)
					pageBufDirty2[x] = false;
			}
		} else {
			if (active_device == 1)
				sync_status = SYNC_FAILURE;
			else if (active_device == 2)
				sync_status2 = SYNC_FAILURE;
		}
		break;

	case SWI_HA_GET_RAND_A:
		pr_info("SWI_HA_GET_RAND_A:\n");
		if(GetRandomNumber(authon_handler->ub_nonceA)!=INF_SWI_SUCCESS){
			lResult = -EFAULT;
			goto cleanup;
		}

		if (copy_to_user((uint32_t __user *) ul_arg, authon_handler->ub_nonceA, MAC_BYTE_LEN) != 0) {
			pr_err("Error: Unable to read random number from device.");
			lResult = -EFAULT;
			goto cleanup;
		}
		break;

	case SWI_HA_SEND_RAND_B:
		pr_info("SWI_HA_SEND_RAND_B:\n");
		if (copy_from_user(authon_handler->ub_nonceB, (uint32_t __user *) ul_arg, MAC_BYTE_LEN) != 0) {
			pr_err("Error: Unable to get nonce B data from User space.\n");
			lResult = -EFAULT;
			goto cleanup;
		}
		/*
		pr_info("RandB: %x %x %x %x %x %x %x %x %x %x", authon_handler->ub_nonceB[0], authon_handler->ub_nonceB[1],
		                                                authon_handler->ub_nonceB[2], authon_handler->ub_nonceB[3],
		                                                authon_handler->ub_nonceB[4], authon_handler->ub_nonceB[5],
		                                                authon_handler->ub_nonceB[6], authon_handler->ub_nonceB[7],
		                                                authon_handler->ub_nonceB[8], authon_handler->ub_nonceB[9]);*/

		local_irq_save(flags);
		preempt_disable();
		ret = Send_DRRES(authon_handler->ub_nonceB);
		if (ret != INF_SWI_SUCCESS) {
			local_irq_restore(flags);
			preempt_enable();
			lResult = -EFAULT;
			goto cleanup;
		}

		local_irq_restore(flags);
		preempt_enable();

		break;

	case SWI_HA_SEND_TAG_A :
		pr_info("SWI_HA_SEND_TAG_A:\n");
		if (copy_from_user(authon_handler->ub_tagA, (uint32_t __user *) ul_arg, MAC_BYTE_LEN) != 0) {
			pr_err("Error: Unable to read TagA data from User space.\n");
			lResult = -EFAULT;
			goto cleanup;
		}
		/*
		pr_info("TagA: %x %x %x %x %x %x %x %x %x %x", authon_handler->ub_tagA[0], authon_handler->ub_tagA[1],
														authon_handler->ub_tagA[2], authon_handler->ub_tagA[3],
														authon_handler->ub_tagA[4], authon_handler->ub_tagA[5],
														authon_handler->ub_tagA[6], authon_handler->ub_tagA[7],
														authon_handler->ub_tagA[8], authon_handler->ub_tagA[9]);*/
		local_irq_save(flags);
		preempt_disable();
		ret = Send_HREQ1(authon_handler->ub_tagA);
		if (INF_SWI_SUCCESS != ret) {			
			local_irq_restore(flags);
			preempt_enable();
			lResult = -EFAULT;
			goto cleanup;
		}

		local_irq_restore(flags);
		preempt_enable();

		//Required extra time required for HREQ1
		delay_ms(10);

		// wait for MAC done
		regaddr = ON_SFR_BUSY_STS;
		timeout = 50;
		do{
			ret = Intf_ReadRegister(regaddr, &rd_data, 1);		
			if (SDK_INTERFACE_SUCCESS != ret) {
				rd_data=(0x1<< BIT_AUTH_MAC_BUSY) | (0x1<<BIT_RANDOM_NUM_BUSY);//default busy if unable to read actual register value
			}else{
				busy = (rd_data >> BIT_AUTH_MAC_BUSY) & 0x01;
			}
			timeout--;	
			if (timeout == 0) {				
				lResult = -EFAULT;
				goto cleanup;
			}
		}while(busy == 1);

		break;

	case SWI_HA_GET_TAG_B:
		pr_info("SWI_HA_GET_TAG_B:\n");

		local_irq_save(flags);
		preempt_disable();
		ret = Send_DRES1(authon_handler->ub_tagB);
		if (INF_SWI_SUCCESS != ret) {
			local_irq_restore(flags);
			preempt_enable();
			lResult = -EFAULT;
			goto cleanup;
		}

		local_irq_restore(flags);
		preempt_enable();

		delay_ms(10);

		if (copy_to_user((uint32_t __user *) ul_arg, authon_handler->ub_tagB, MAC_BYTE_LEN) != 0) {
			pr_err("Error: Unable to read TagB from device. ret=%x", ret);
			lResult = -EFAULT;
			goto cleanup;
		}

		break;

	case SWI_ECC_GET_CERT:
		pr_info("SWI_ECC_GET_CERT:\n");		
		if (copy_from_user(ecc_msg, (uint32_t __user *)ul_arg, sizeof(struct ECC_MESSAGE))!=0 ){
    		lResult = -EFAULT;
			goto cleanup;
		}

		//pr_info("SWI_ECC_GET_CERT: %d \n", ecc_msg->key_slot);
		//pr_info("SWI_ECC_GET_CERT: Read ODC \n");
		//pr_info("SWI_ECC_GET_CERT: Read UID\n");
		ret = authon_exe_read_uid(uid, tid, pid);
		if(ret!=EXE_SUCCESS){
			pr_err("Error: Read UID failed\n");
			ecc_msg->status = ret;
			lResult = -EFAULT;
			goto cleanup;
		}
		/*
		pr_info("UID: %x %x %x %x %x %x %x %x %x %x %x %x ",uid[0],uid[1],uid[2],uid[3],
															uid[4],uid[5],uid[6],uid[7],
															uid[8],uid[9],uid[10],uid[11]);											
		*/

		// read ODC and public key
		ret = authon_get_odc(ecc_msg->key_slot, gf2nODC);
		if( ret != APP_ECC_SUCCESS){
			ecc_msg->status = ret;
			pr_err("Error: Read ODC failed\n");
			//For device with host auth enabled, ensure that host authentication is executed prior to reading the ODC.
			lResult = -EFAULT;
			goto cleanup;
			
		}else{
			pr_info("SWI_ECC_GET_CERT: Read Public key\n");
			ret = authon_get_ecc_publickey(ecc_msg->key_slot, eccpublicKey);
			if(ret!=APP_ECC_SUCCESS){
				ecc_msg->status = ret;
				pr_err("Error: Read Public key failed\n");
				lResult = -EFAULT;
				goto cleanup;
			}else{
				ecc_msg->status = 0;
				//pr_info("SWI_ECC_GET_CERT: Read Public key ok\n");
			}
		}


		memcpy(ecc_msg->cert, uid, sizeof(uid));
		memcpy(ecc_msg->cert+sizeof(uid), eccpublicKey, PUBLICKEY_BYTE_LEN);
		memcpy(ecc_msg->cert+sizeof(uid)+PUBLICKEY_BYTE_LEN, gf2nODC, ODC_BYTE_LEN);
		ecc_msg->status = 0;

		lResult = 0;

		//pr_info("SWI_ECC_GET_CERT: send certificate useland\n");
		if (copy_to_user((uint32_t __user *) ul_arg, ecc_msg, sizeof(struct ECC_MESSAGE)) != 0) {
			pr_err("Error: Unable to read certificate information from device. ret=%x", ret);
			lResult = -EFAULT;
			goto cleanup;
		}
		break;

	case SWI_ECC_SEND_CHALLENGE_GET_RESPONSE:
		//pr_info("SWI_ECC_SEND_CHALLENGE_GET_RESPONSE:\n");		
		if (copy_from_user(ecc_msg1, (uint32_t __user *)ul_arg, sizeof(struct ECC_MESSAGE))!=0 ){
    		lResult = -EFAULT;
			goto cleanup;
		}
		/*
		pr_info("Received Challenge: ");
		for(i=0; i<sizeof(ecc_msg1->Challenge); i++){
			pr_info("%x ", ecc_msg1->Challenge[i]);
		}
		*/

		memcpy(challenge, ecc_msg1->Challenge, sizeof(ecc_msg1->Challenge));
		/*
		for(i=0; i<sizeof(challenge); i++){
			pr_info("%x ", challenge[i]);
		}
		pr_info("keyslot: %d\n", ecc_msg1->key_slot);
		*/
		//Send challenge to device and start ECC
		//ret = challenge_response(challenge, xResponse, zResponse, ecc_msg1->key_slot, FIXED_WAIT);
		ret = Send_SWI_ChallengeAndGetResponse( (uint8_t*)challenge, ubp_resp, ecc_msg1->key_slot, FIXED_WAIT);
		if (ret != APP_ECC_SUCCESS) {
			ecc_msg1->status=ret;
			lResult = -EFAULT;
			goto cleanup;
		}

		/*
		pr_info("ubp_resp: ");
		for(i=0; i<sizeof(ubp_resp); i++){
			pr_info("%x ", ubp_resp[i]);
		}*/

		memcpy(ecc_msg1->response, ubp_resp, sizeof(ubp_resp));

		if (copy_to_user((uint32_t __user *) ul_arg, ecc_msg1, sizeof(struct ECC_MESSAGE)) != 0) {
			pr_err("Error: Unable to copy response to User space. ret=%x", ret);
			lResult = -EFAULT;
			goto cleanup;
		}
		break;
	case SWI_ECC_SEND_MAC_BYTE:
		pr_info("SWI_ECC_SEND_MAC_BYTE:\n");
		if (copy_from_user(ecc_msg1, (uint32_t __user *)ul_arg, sizeof(struct ECC_MESSAGE))!=0 ){
			lResult = -EFAULT;
			goto cleanup;
		}

		pr_info("SWI_ECC_SEND_MAC_BYTE: %x %x %x %x %x %x %x %x %x %x\n", ecc_msg1->mac_byte[0],ecc_msg1->mac_byte[1],ecc_msg1->mac_byte[2],
		                                                                  ecc_msg1->mac_byte[3],ecc_msg1->mac_byte[4],ecc_msg1->mac_byte[5],
		                                                                  ecc_msg1->mac_byte[6],ecc_msg1->mac_byte[7],ecc_msg1->mac_byte[8],
		                                                                  ecc_msg1->mac_byte[9]);

		authon_unlock_nvm_locked(ecc_msg1->mac_byte);

		break;
	case AUTHENTICATION_STATUS:
		if (active_device == 1) {
			if (copy_from_user(&is_authentic, (uint32_t __user *)ul_arg, sizeof(bool)) != 0) {
				pr_err("Error: Unable to copy authentication status");
				lResult = -EFAULT;
				goto cleanup;
			}
		} else if (active_device == 2) {
			if (copy_from_user(&is_authentic2, (uint32_t __user *)ul_arg, sizeof(bool)) != 0) {
				pr_err("Error: Unable to copy authentication status");
				lResult = -EFAULT;
				goto cleanup;
			}
		}
		break;

	case INIT_PAGE_BUFFER:
		pr_info("INIT_PAGE_BUFFER:\n");
		if (copy_from_user(rmsg, (uint32_t __user *)ul_arg, sizeof(struct MESSAGE)) != 0) {
			lResult = -EFAULT;
			goto cleanup;
		}

		pr_info("INIT_PAGE_BUFFER: %d %d\n", rmsg->page_count, rmsg->offset);
		ret = authon_read_nvm(rmsg->offset, rmsg->nvm_data, rmsg->page_count);
		if (ret != APP_NVM_SUCCESS) {
			pr_err("Error: Read NVM failed, ret=0x%x\r\n", ret);
			rmsg->status = ret;
			lResult = -EFAULT;
			goto cleanup;
		} else {
			rmsg->status = 0x0;
			if(active_device == 1) {
				memcpy(page_data_buf, rmsg->nvm_data, sizeof(rmsg->nvm_data));
				is_page_buf_init = true;
			} else if(active_device == 2) {
				memcpy(page_data_buf2, rmsg->nvm_data, sizeof(rmsg->nvm_data));
				is_page_buf_init2 = true;
			}
		}

		if (copy_to_user((uint32_t __user *) ul_arg, rmsg, sizeof(struct MESSAGE)) != 0) {
			pr_err("ERROR: Unable to read NVM data. ret=%x", ret);
			rmsg->status = ret;
			lResult = -EFAULT;
			goto cleanup;
		} else {
			rmsg->status = 0x0;
			lResult = 0;
		}
		cancel_delayed_work(&g_auth_driv->sec_auth_poll_work);
		queue_delayed_work(g_auth_driv->sec_auth_poll_wqueue,
						  &g_auth_driv->sec_auth_poll_work, work_sched_delay * HZ);
		break;

	case GET_AUTH_IC_LIST:
		if (copy_to_user((uint32_t __user *) ul_arg, &auth_ic_list, sizeof(auth_ic_list)) != 0)
			pr_info("Error: Unable to copy auth_ic_list");
		pr_info("Available auth ICs: %d\n", auth_ic_list);
		break;

	default:
		lResult = -EINVAL;
		pr_info(KERN_ERR "Invalid IOCTL: %d\n", ui_command);
		goto cleanup;
	}

cleanup:
	kfree(rrmsg);
	kfree(ecc_msg);
	kfree(wrmsg);
	kfree(lockmsg);
	kfree(rmsg);
	kfree(wmsg);
	kfree(ecc_msg1);

	return lResult;
}

/**
 * @brief Open Authenticate On driver
 * */
static int authon_driver_open(struct inode *s_inode, struct file *s_file)
{
	int rval;

	pr_info(">> %s\n", __func__);

	if (test_and_set_bit(IN_USE_BIT, &authon_driver_flag)){
		return -EBUSY;
	}
	authon_handler = kzalloc(sizeof(struct AUTH_ON_S), GFP_KERNEL);
	if (!(authon_handler)) {
		authon_handler = NULL;
		return -ENOMEM; // out of memory
	}

	init_completion(&(authon_handler->event));

	s_file->private_data = &authon_driver_ops;
	rval = nonseekable_open(s_inode, s_file);

	authon_driver_open_count++;
	pr_info("You have open file for %d times\n", authon_driver_open_count);
	pr_info("<< %s\n", __func__);
	return 0;
}

/**
 * @brief Release and frees off resources from Authenticate On driver
 * */
static int authon_driver_release(struct inode *inode, struct file *file)
{
	pr_info(">> %s\n", __func__);
	pr_info("You have close file for %d times\n", authon_driver_open_count);
	authon_driver_open_count--;
	authon_handler = NULL;

	clear_bit(IN_USE_BIT, &authon_driver_flag);
	pr_info("<< %s\n", __func__);
	return 0;
}

static int authon_driver_parse_dt(struct authon_driver_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "sec-auth-sle956681");
	const char *power_mode;
	int ret = 0;
	int i = 0;

	pr_info("%s: Authon Parsing init\n", __func__);
	if (np == NULL) {
		pr_info("%s: np NULL\n", __func__);
	} else {
#if !defined(CONFIG_SEC_FACTORY)
		// check if authic supported or not
		ret = check_authic_supported(np);
		if (ret) {
			pr_info("%s: Auth IC not supported, return\n", __func__);
			return -EINVAL;
		}
#endif
		pdata->swi_gpio_cnt = sb_of_gpio_named_count(np,
			"authon,swi_gpio");
		if (pdata->swi_gpio_cnt > 0) {
			pr_info("%s: Has %d swi_gpios\n", __func__, pdata->swi_gpio_cnt);
			if (pdata->swi_gpio_cnt > MAX_SWI_GPIO) {
				pr_err("%s: swi_gpio, catch out-of bounds array read\n",
						__func__);
				pdata->swi_gpio_cnt = MAX_SWI_GPIO;
			}
			for (i = 0; i < pdata->swi_gpio_cnt; i++) {
				pdata->swi_gpio[i] = of_get_named_gpio(np, "authon,swi_gpio", i);
				if (pdata->swi_gpio[i] < 0) {
					pr_err("%s: error reading swi_gpio = %d\n",
						__func__, pdata->swi_gpio[i]);
					pdata->swi_gpio[i] = 0;
				}
			}
		} else {
			pr_err("%s: No swi_gpio, gpio_cnt = %d\n",
					__func__, pdata->swi_gpio_cnt);
			return -1;
		}

		/* Read power configuration */
		//For indirect power mode, tPDL is 2.5m and direct power tPDL is 2ms.
		ret = of_property_read_string(np,
			"authon,power_mode", (char const **)&power_mode);
		if (ret) {
			pr_info("%s: Power mode is Empty\n", __func__);
			return -1;
		} else {
			pr_info("%s: power mode is  %s\n", __func__, power_mode);
			if (strcmp(power_mode, "indirect") == 0) {
				pr_info("%s: indirect power\n", __func__);
				PowerDownDelay_Time = 2500;
				PowerUpDelay_Time = 10000;
			} else if (strcmp(power_mode, "direct") == 0) {
				PowerDownDelay_Time = 2000;
				PowerUpDelay_Time = 8000;
			} else {
				PowerDownDelay_Time = 2500;
				PowerUpDelay_Time = 10000;
			}
		}

		//Read 1 Tau value from device overlay
		//For indirect power mode, recommended to use above 3us Tau
		ret = of_property_read_u32(np, "authon,1tau_value", &Highspeed_BaudLow1);
		if (ret) {
			pr_err("%s: Error! Could not read 'authon_1tau_value'\n", __func__);
			return -1;
		}
		if (strcmp(power_mode, "indirect") == 0) {
			if (Highspeed_BaudLow < 3) {
				pr_info("%s: warning tau value: %d should be larger than 3us for indirect power mode\n",
						__func__, Highspeed_BaudLow1);
			}
		}
		if (Highspeed_BaudLow1 > MAX_1TAU)
			pr_info("%s: warning dt_1tau_value: %d should be less than %d us\n",
					__func__, Highspeed_BaudLow1, MAX_1TAU);
		else
			pr_info("%s: dt_1tau_value: %d\n", __func__, Highspeed_BaudLow1);

		//Read 3 Tau value from device overlay
		ret = of_property_read_u32(np, "authon,3tau_value", &Highspeed_BaudHigh1);
		if (ret) {
			pr_err("%s: Error! Could not read 'authon_3tau_value'\n", __func__);
			return -1;
		}
		if (Highspeed_BaudHigh1 > MAX_3TAU)
			pr_info("%s: warning dt_3tau_value: %d should be less than %d us\n",
					__func__, Highspeed_BaudHigh1, MAX_3TAU);
		else
			pr_info("%s: dt_3tau_value: %d\n", __func__, Highspeed_BaudHigh1);

		//Read 5 Tau value from device overlay
		ret = of_property_read_u32(np, "authon,5tau_value", &Highspeed_BaudStop1);
		if (ret) {
			pr_err("%s: Error! Could not read 'authon_5tau_value'\n", __func__);
			return -1;
		}
		if (Highspeed_BaudStop1 > MAX_5TAU)
			pr_info("%s: warning dt_5tau_value: %d should be less than %d us\n",
					__func__, Highspeed_BaudStop1, MAX_5TAU);
		else
			pr_info("%s: dt_5tau_value: %d\n", __func__, Highspeed_BaudStop1);

		//Read 1 Tau value from device overlay
		//For indirect power mode, recommended to use above 3us Tau
		ret = of_property_read_u32(np, "authon,1tau_value2", &Highspeed_BaudLow2);
		if (ret) {
			pr_err("%s: Could not read 'authon_1tau_value2' , setting to default 60\n", __func__);
			Highspeed_BaudLow2 = 60;
		}
		if (strcmp(power_mode, "indirect") == 0) {
			if (Highspeed_BaudLow2 < 3) {
				pr_info("%s: warning tau value2: %d should be larger than 3us for indirect power mode\n",
						__func__, Highspeed_BaudLow2);
			}
		}
		if (Highspeed_BaudLow2 > MAX_1TAU)
			pr_info("%s: warning dt_1tau_value: %d should be less than %d us\n",
					__func__, Highspeed_BaudLow2, MAX_1TAU);
		else
			pr_info("%s: dt_1tau_value2: %d\n", __func__, Highspeed_BaudLow2);

		//Read 3 Tau value from device overlay
		ret = of_property_read_u32(np, "authon,3tau_value2", &Highspeed_BaudHigh2);
		if (ret) {
			pr_err("%s: Could not read 'authon_3tau_value2' setting to default 180\n", __func__);
			Highspeed_BaudHigh2 = 180;
		}
		if (Highspeed_BaudHigh2 > MAX_3TAU)
			pr_info("%s: warning dt_3tau_value2: %d should be less than %d us\n",
					__func__, Highspeed_BaudHigh2, MAX_3TAU);
		else
			pr_info("%s: dt_3tau_value2: %d\n", __func__, Highspeed_BaudHigh2);

		//Read 5 Tau value from device overlay
		ret = of_property_read_u32(np, "authon,5tau_value2", &Highspeed_BaudStop2);
		if (ret) {
			pr_err("%s: Could not read 'authon_5tau_value2', setting to default 300\n", __func__);
			Highspeed_BaudStop2 = 300;
		}
		if (Highspeed_BaudStop2 > MAX_5TAU)
			pr_info("%s: warning dt_5tau_value2: %d should be less than %d us\n",
					__func__, Highspeed_BaudStop2, MAX_5TAU);
		else
			pr_info("%s: dt_5tau_value2: %d\n", __func__, Highspeed_BaudStop2);

		//Read response timeout value from device overlay
		//Worst case Bus time out 1: time 90x9uS = 810uS
		//Worst case Bus time out 2: time 10x153uS = 1530uS
		ret = of_property_read_u32(np, "authon,response_timeout_value", &Highspeed_ResponseTimeOut);
		if (ret) {
			pr_err("%s: Error! Could not read 'authon_response_timeout_value'\n", __func__);
			return -1;
		}

		pr_info("%s: authon_response_timeout_value: %d\n", __func__, Highspeed_ResponseTimeOut);
	}
	return ret;
}

int platform_init(void)
{
	if (gpio_is_valid(SWI_GPIO_PIN) == false) {
		pr_err("GPIO %d is not valid\n", SWI_GPIO_PIN);
		return (-1);
	} else {
		pr_info("SWI GPIO %d is valid\n", SWI_GPIO_PIN);
		if (gpio_request(SWI_GPIO_PIN, "sle956681_swi_gpio") < 0) {
			pr_err("ERROR: GPIO %d request\n", SWI_GPIO_PIN);
			return -1;
		}
	}
	pr_info("%s: done\n", __func__);
	return 0;
}

/**
 * @brief Authenticate On SDK initialization
 * */
static uint16_t authon_sdk_init(void)
{
	int retry = RETRY_COUNT;
	uint16_t device_address;
	uint16_t ret;
	int i=0;

	while (retry) {
		ret = authon_init_sdk(authon_capability_handle);
		if(ret!= SDK_SUCCESS){
			pr_err("%s | Error: failed to initialize SDK ret=0x%X retry:%d\n",__func__, ret, retry);
		}else{
			pr_info("%s | Found %d device(s) on the bus\r\n",__func__, authon_enumeration.device_found);
			for(i=0; i<authon_enumeration.device_found; i++){
				print_row_UID_table((uint8_t *)&(authon_capability.uid[i][0]));
			}
		}
		if(ret==SDK_SUCCESS){
			pr_info("%s | SDK Initialized\r\n",__func__);
			ret = authon_get_swi_address(&device_address);
			if(ret!= SDK_SUCCESS){
				pr_err("%s | Error: unable to read device address ret=0x%X\n",__func__, ret);
			}else{
				pr_info("%s | Device address=0x%X\n",__func__, device_address);
			}
			return SDK_SUCCESS;
		}else{
			memset(authon_capability_handle, 0, sizeof(AuthOn_Capability));
			authon_capability_handle->swi.gpio = SWI_GPIO_PIN;
		}
		retry--;
	}
	pr_info("%s: SDK_INIT = %d\n", __func__, SDK_INIT);
	return SDK_INIT;
}

static void integer_to_bytes(int num, unsigned char *data, int size)
{
	int i;

	for (i = 0; i < size; i++)
		data[i] = (num>>(8*(size-i-1))) & 0xFF;
}

static int byte_array_to_int(unsigned char *data, int size)
{
	int i;
	int n = 0;

	for (i = 0; i < size; i++)
		n = (n<<8) | data[i];
	return n;
}

static int write_to_page_buffer(int pageNumber, int startByte, int cnt, char *data)
{
	int i, startIdx;

	if (!is_page_buf_init)
		return -EINVAL;

	pageBufDirty[pageNumber] = true;
	startIdx = pageNumber * PAGE_BYTE_CNT + startByte;
	if (startIdx % 4 != 0)
		pageNumber++;

	for (i = 0; i < cnt ; i++) {
		page_data_buf[startIdx + i] = data[i];
		if ((startIdx + i) % 4 == 0) {
			pageBufDirty[pageNumber] = true;
			pageNumber++;
		}
	}

	return 0;
}

static int read_from_page_buffer(int pageNumber, int startByte, int cnt, char *data)
{
	int i, startIdx;

	if (!is_page_buf_init)
		return -EINVAL;

	startIdx = pageNumber * PAGE_BYTE_CNT + startByte;
	for (i = 0; i < cnt; i++)
		data[i] = page_data_buf[startIdx + i];

	return 0;
}

static int write_to_page_buffer2(int pageNumber, int startByte, int cnt, char *data)
{
	int i, startIdx;

	if (!is_page_buf_init2)
		return -EINVAL;

	pageBufDirty2[pageNumber] = true;
	startIdx = pageNumber * PAGE_BYTE_CNT + startByte;
	if (startIdx % 4 != 0)
		pageNumber++;

	for (i = 0; i < cnt ; i++) {
		page_data_buf2[startIdx + i] = data[i];
		if ((startIdx + i) % 4 == 0) {
			pageBufDirty2[pageNumber] = true;
			pageNumber++;
		}
	}

	return 0;
}

static int read_from_page_buffer2(int pageNumber, int startByte, int cnt, char *data)
{
	int i, startIdx;

	if (!is_page_buf_init2)
		return -EINVAL;

	startIdx = pageNumber * PAGE_BYTE_CNT + startByte;
	for (i = 0; i < cnt; i++)
		data[i] = page_data_buf2[startIdx + i];

	return 0;
}

static int get_qr_code(char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer(QR_CODE_PAGE,
			QR_CODE_START_BYTE, QR_CODE_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);
	return err;
}

static int get_qr_code2(char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer2(QR_CODE_PAGE,
			QR_CODE_START_BYTE, QR_CODE_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);
	return err;
}

static int set_first_use_date_page_lock(void)
{
	int err = 0;

	/* send uevent for page lock ioctl command*/
	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = kobject_uevent_env(nvm_write_kobj, KOBJ_CHANGE, use_date_wlock_event);
	if (err < 0) {
		pr_err("Error sending uevent for nvm_write kobject (%d)\n", err);
		mutex_unlock(&g_auth_driv->sec_auth_mutex);
		return err;
	}
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_first_use_date_page_lock2(void)
{
	int err = 0;

	/* send uevent for page lock ioctl command*/
	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = kobject_uevent_env(nvm_write_kobj, KOBJ_CHANGE, use_date_wlock2_event);
	if (err < 0) {
		pr_err("Error sending uevent for nvm_write kobject (%d)\n", err);
		mutex_unlock(&g_auth_driv->sec_auth_mutex);
		return err;
	}
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_first_use_date(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer(FIRST_USE_DATE_PAGE,
			FIRST_USE_DATE_START_BYTE, FIRST_USE_DATE_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_first_use_date2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer2(FIRST_USE_DATE_PAGE,
			FIRST_USE_DATE_START_BYTE, FIRST_USE_DATE_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int get_first_use_date(char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer(FIRST_USE_DATE_PAGE,
			FIRST_USE_DATE_START_BYTE, FIRST_USE_DATE_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int get_first_use_date2(char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer2(FIRST_USE_DATE_PAGE,
			FIRST_USE_DATE_START_BYTE, FIRST_USE_DATE_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int get_batt_discharge_level(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer(BATT_DISCHARGE_LEVEL_PAGE,
			BATT_DISCHARGE_LEVEL_START_BYTE, BATT_DISCHARGE_LEVEL_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int get_batt_discharge_level2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer2(BATT_DISCHARGE_LEVEL_PAGE,
			BATT_DISCHARGE_LEVEL_START_BYTE, BATT_DISCHARGE_LEVEL_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_batt_discharge_level(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer(BATT_DISCHARGE_LEVEL_PAGE,
			BATT_DISCHARGE_LEVEL_START_BYTE, BATT_DISCHARGE_LEVEL_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_batt_discharge_level2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer2(BATT_DISCHARGE_LEVEL_PAGE,
			BATT_DISCHARGE_LEVEL_START_BYTE, BATT_DISCHARGE_LEVEL_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int get_batt_full_status_usage(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer(BATT_FULL_STATUS_USAGE_PAGE,
			BATT_FULL_STATUS_USAGE_BYTE, BATT_FULL_STATUS_USAGE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int get_batt_full_status_usage2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer2(BATT_FULL_STATUS_USAGE_PAGE,
			BATT_FULL_STATUS_USAGE_BYTE, BATT_FULL_STATUS_USAGE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_batt_full_status_usage(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer(BATT_FULL_STATUS_USAGE_PAGE,
			BATT_FULL_STATUS_USAGE_BYTE, BATT_FULL_STATUS_USAGE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_batt_full_status_usage2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer2(BATT_FULL_STATUS_USAGE_PAGE,
			BATT_FULL_STATUS_USAGE_BYTE, BATT_FULL_STATUS_USAGE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}


static int get_bsoh(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer(BSOH_PAGE,
			BSOH_START_BYTE, BSOH_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int get_bsoh2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer2(BSOH_PAGE,
			BSOH_START_BYTE, BSOH_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_bsoh(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer(BSOH_PAGE,
			BSOH_START_BYTE, BSOH_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_bsoh2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer2(BSOH_PAGE,
			BSOH_START_BYTE, BSOH_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int get_bsoh_raw(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer(BSOH_RAW_PAGE,
			BSOH_RAW_START_BYTE, BSOH_RAW_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int get_bsoh_raw2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer2(BSOH_RAW_PAGE,
			BSOH_RAW_START_BYTE, BSOH_RAW_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_bsoh_raw(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer(BSOH_RAW_PAGE,
			BSOH_RAW_START_BYTE, BSOH_RAW_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_bsoh_raw2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer2(BSOH_RAW_PAGE,
			BSOH_RAW_START_BYTE, BSOH_RAW_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static void convert_heatmap(u8 heatmap[3][32], int *data, int size)
{
	int j = 0;
	int k = 0;
	int i = 0;
	int flag = 1;

	for (i = 0; i < 64; i++) {
		if (flag == 1) {
			data[i] = (data[i] | heatmap[j][k]) << 4;
			if (k < 31) {
				k++;
			} else if (k == 31 && j < 2) {
				j++;
				k = 0;
			}
			data[i] = data[i] | ((heatmap[j][k] & 0xf0) >> 4);
			flag = 0;
		} else {
			data[i] = (data[i] | (heatmap[j][k] & 0x0f)) << 8;
			if (k < 31) {
				k++;
			} else if (k == 31 && j < 2) {
				j++;
				k = 0;
			}
			data[i] = data[i] | heatmap[j][k];
			if (k < 31) {
				k++;
			} else if (k == 31 && j < 2) {
				j++;
				k = 0;
			}
			flag = 1;
		}
		pr_info("%s: data[%d] = %x\n", __func__, i, data[i]);
	}
}

static int get_heatmap(int *heatmap, int size)
{
	int err;
	u8 read_hmap[3][32] = { 0 };
	char data[32];

	mutex_lock(&g_auth_driv->sec_auth_mutex);

	err = read_from_page_buffer(HEATMAP_FIRST_PAGE, HEATMAP_FIRST_START_BYTE, HEATMAP_FIRST_BYTE_CNT, data);
	if (err)
		goto err_read_from_buffer;
	memcpy(read_hmap[0], data, 32);
	memset(data, '\0', sizeof(data));

	err = read_from_page_buffer(HEATMAP_SECOND_PAGE, HEATMAP_SECOND_START_BYTE, HEATMAP_SECOND_BYTE_CNT, data);
	if (err)
		goto err_read_from_buffer;
	memcpy(read_hmap[1], data, 32);
	memset(data, '\0', sizeof(data));

	err = read_from_page_buffer(HEATMAP_THIRD_PAGE, HEATMAP_THIRD_START_BYTE, HEATMAP_THIRD_BYTE_CNT, data);
	if (err)
		goto err_read_from_buffer;
	memcpy(read_hmap[2], data, 32);
	memset(data, '\0', sizeof(data));

	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	convert_heatmap(read_hmap, heatmap, sizeof(heatmap));
	pr_info("%s: decompress heatmap from memory successfully\n", __func__);
	return 0;

err_read_from_buffer:
	mutex_unlock(&g_auth_driv->sec_auth_mutex);
	pr_info("%s: reading from buffer failed\n", __func__);
	return err;
}

static int get_heatmap2(int *heatmap, int size)
{
	int err;
	u8 read_hmap[3][32] = { 0 };
	char data[32];

	mutex_lock(&g_auth_driv->sec_auth_mutex);

	err = read_from_page_buffer2(HEATMAP_FIRST_PAGE, HEATMAP_FIRST_START_BYTE, HEATMAP_FIRST_BYTE_CNT, data);
	if (err)
		goto err_read_from_buffer;
	memcpy(read_hmap[0], data, 32);
	memset(data, '\0', sizeof(data));

	err = read_from_page_buffer2(HEATMAP_SECOND_PAGE, HEATMAP_SECOND_START_BYTE, HEATMAP_SECOND_BYTE_CNT, data);
	if (err)
		goto err_read_from_buffer;
	memcpy(read_hmap[1], data, 32);
	memset(data, '\0', sizeof(data));

	err = read_from_page_buffer2(HEATMAP_THIRD_PAGE, HEATMAP_THIRD_START_BYTE, HEATMAP_THIRD_BYTE_CNT, data);
	if (err)
		goto err_read_from_buffer;
	memcpy(read_hmap[2], data, 32);
	memset(data, '\0', sizeof(data));

	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	convert_heatmap(read_hmap, heatmap, sizeof(heatmap));
	pr_info("%s: decompress heatmap from memory successfully\n", __func__);
	return 0;

err_read_from_buffer:
	mutex_unlock(&g_auth_driv->sec_auth_mutex);
	pr_info("%s: reading from buffer failed\n", __func__);
	return err;
}

static int save_heatmap(u8 heatmap[3][32])
{
	int err;
	char data[32];

	memset(data, '\0', sizeof(data));

	memcpy(data, heatmap[0], 32);

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer(HEATMAP_FIRST_PAGE, HEATMAP_FIRST_START_BYTE, HEATMAP_FIRST_BYTE_CNT, data);
	if (err)
		goto err_save_to_buffer;

	memset(data, '\0', sizeof(data));
	memcpy(data, heatmap[1], 32);
	err = write_to_page_buffer(HEATMAP_SECOND_PAGE, HEATMAP_SECOND_START_BYTE, HEATMAP_SECOND_BYTE_CNT, data);
	if (err)
		goto err_save_to_buffer;

	memset(data, '\0', sizeof(data));
	memcpy(data, heatmap[2], 32);
	err = write_to_page_buffer(HEATMAP_THIRD_PAGE, HEATMAP_THIRD_START_BYTE, HEATMAP_THIRD_BYTE_CNT, data);
	if (err)
		goto err_save_to_buffer;

	mutex_unlock(&g_auth_driv->sec_auth_mutex);
	pr_info("%s: heatmap saved to page buffer successfully\n", __func__);
	return 0;

err_save_to_buffer:
	mutex_unlock(&g_auth_driv->sec_auth_mutex);
	pr_info("%s: saving to buffer failed\n", __func__);
	return err;
}

static int save_heatmap2(u8 heatmap[3][32])
{
	int err;
	char data[32];

	memset(data, '\0', sizeof(data));

	memcpy(data, heatmap[0], 32);

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer2(HEATMAP_FIRST_PAGE, HEATMAP_FIRST_START_BYTE, HEATMAP_FIRST_BYTE_CNT, data);
	if (err)
		goto err_save_to_buffer;

	memset(data, '\0', sizeof(data));
	memcpy(data, heatmap[1], 32);
	err = write_to_page_buffer2(HEATMAP_SECOND_PAGE, HEATMAP_SECOND_START_BYTE, HEATMAP_SECOND_BYTE_CNT, data);
	if (err)
		goto err_save_to_buffer;

	memset(data, '\0', sizeof(data));
	memcpy(data, heatmap[2], 32);
	err = write_to_page_buffer2(HEATMAP_THIRD_PAGE, HEATMAP_THIRD_START_BYTE, HEATMAP_THIRD_BYTE_CNT, data);
	if (err)
		goto err_save_to_buffer;

	mutex_unlock(&g_auth_driv->sec_auth_mutex);
	pr_info("%s: heatmap saved to page buffer successfully\n", __func__);
	return 0;

err_save_to_buffer:
	mutex_unlock(&g_auth_driv->sec_auth_mutex);
	pr_info("%s: saving to buffer failed\n", __func__);
	return err;
}

static void set_heatmap(int *data, int size)
{
	u8 heatmap[3][32];
	int flag = 1;
	int j = 0;
	int k = 0;
	int i = 0;
	int ret;

	memset(heatmap, 0, sizeof(heatmap));

	for (i = 0; i < size; i++) {
		if (flag) {
			heatmap[j][k] = heatmap[j][k] | ((data[i] & 0x0ff0) >> 4);
			if (k < 31) {
				k++;
			} else if (k == 31 && j < 2) {
				k = 0;
				j++;
			}
			heatmap[j][k] = (heatmap[j][k] | (data[i] & 0x000f)) << 4;
			flag = 0;
		} else {
			heatmap[j][k] = heatmap[j][k] | ((data[i] & 0x0f00) >> 8);
			if (k < 31) {
				k++;
			} else if (k == 31 && j < 2) {
				j++;
				k = 0;
			}
			heatmap[j][k] = heatmap[j][k] | ((data[i] & 0x00ff));
			if (k < 31) {
				k++;
			} else if (k == 31 && j < 2) {
				j++;
				k = 0;
			}
			flag = 1;
		}
	}
#if DEBUG
	for (i = 0 ; i < 3; i++)
		for (j = 0; j < 32; j++)
			pr_info("%s: heapmap[%d][%d] = %x", __func__, i, j, heatmap[i][j]);
#endif
	ret = save_heatmap(heatmap);
	if (ret)
		pr_info("%s: error in writing heatmap", __func__);

}

static void set_heatmap2(int *data, int size)
{
	u8 heatmap[3][32];
	int flag = 1;
	int j = 0;
	int k = 0;
	int i = 0;
	int ret;

	memset(heatmap, 0, sizeof(heatmap));

	for (i = 0; i < size; i++) {
		if (flag) {
			heatmap[j][k] = heatmap[j][k] | ((data[i] & 0x0ff0) >> 4);
			if (k < 31) {
				k++;
			} else if (k == 31 && j < 2) {
				k = 0;
				j++;
			}
			heatmap[j][k] = (heatmap[j][k] | (data[i] & 0x000f)) << 4;
			flag = 0;
		} else {
			heatmap[j][k] = heatmap[j][k] | ((data[i] & 0x0f00) >> 8);
			if (k < 31) {
				k++;
			} else if (k == 31 && j < 2) {
				j++;
				k = 0;
			}
			heatmap[j][k] = heatmap[j][k] | ((data[i] & 0x00ff));
			if (k < 31) {
				k++;
			} else if (k == 31 && j < 2) {
				j++;
				k = 0;
			}
			flag = 1;
		}
	}
#if DEBUG
	for (i = 0 ; i < 3; i++)
		for (j = 0; j < 32; j++)
			pr_info("%s: heapmap[%d][%d] = %x", __func__, i, j, heatmap[i][j]);
#endif
	ret = save_heatmap2(heatmap);
	if (ret)
		pr_info("%s: error in writing heatmap", __func__);

}

static int get_asoc(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer(ASOC_PAGE,
			ASOC_START_BYTE, ASOC_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int get_asoc2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer2(ASOC_PAGE,
			ASOC_START_BYTE, ASOC_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_asoc(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer(ASOC_PAGE,
			ASOC_START_BYTE, ASOC_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_asoc2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer2(ASOC_PAGE,
			ASOC_START_BYTE, ASOC_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int get_fai_expired(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer(FAI_EXPIRED_PAGE,
			FAI_EXPIRED_START_BYTE, FAI_EXPIRED_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int get_fai_expired2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = read_from_page_buffer2(FAI_EXPIRED_PAGE,
			FAI_EXPIRED_START_BYTE, FAI_EXPIRED_BYTE_CNT, data);
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	return err;
}

static int set_fai_expired(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer(FAI_EXPIRED_PAGE,
			FAI_EXPIRED_START_BYTE, FAI_EXPIRED_BYTE_CNT, data);
	if (err) {
		mutex_unlock(&g_auth_driv->sec_auth_mutex);
		return err;
	}
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	/* send uevent for memory sync */
	sync_status = SYNC_PROGRESS;
	err = kobject_uevent_env(nvm_write_kobj, KOBJ_CHANGE, sync_event);
	if (err < 0) {
		sync_status = SYNC_FAILURE;
		pr_err("Error sending uevent for nvm_write kobject (%d)\n", err);
		return err;
	}

	return err;
}

static int set_fai_expired2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_auth_driv->sec_auth_mutex);
	err = write_to_page_buffer2(FAI_EXPIRED_PAGE,
			FAI_EXPIRED_START_BYTE, FAI_EXPIRED_BYTE_CNT, data);
	if (err) {
		mutex_unlock(&g_auth_driv->sec_auth_mutex);
		return err;
	}
	mutex_unlock(&g_auth_driv->sec_auth_mutex);

	/* send uevent for memory sync */
	sync_status2 = SYNC_PROGRESS;
	err = kobject_uevent_env(nvm_write_kobj, KOBJ_CHANGE, sync_event2);
	if (err < 0) {
		sync_status2 = SYNC_FAILURE;
		pr_err("Error sending uevent for nvm_write kobject (%d)\n", err);
		return err;
	}

	return err;
}
static ssize_t battery_svc_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
		char temp_buf[QR_CODE_BYTE_CNT + 1] = {0, };
		int ret = 0;
		int i = 0;

		ret = get_qr_code(temp_buf);
		if (ret)
			return ret;

		pr_info("%s: SVC_Battery(%s) show attrs\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
		return i;
}

static ssize_t battery_svc_show2(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
		char temp_buf[QR_CODE_BYTE_CNT + 1] = {0, };
		int ret = 0;
		int i = 0;

		ret = get_qr_code2(temp_buf);
		if (ret)
			return ret;

		pr_info("%s: SVC_Battery(%s) show attrs\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
		return i;
}

static struct kobj_attribute sysfs_SVC_Battery_attr =
	__ATTR(SVC_battery, 0444, battery_svc_show, NULL);

static struct kobj_attribute sysfs_SVC_Battery_attr2 =
	__ATTR(SVC_battery2, 0444, battery_svc_show2, NULL);

static int sec_auth_create_svc_attrs(struct device *dev)
{
	int ret;
	struct kernfs_node *svc_sd = NULL;
	struct kobject *svc = NULL;
	struct kobject *battery = NULL;

	/* To find /sys/devices/svc/ */
	svc_sd = sysfs_get_dirent(dev->kobj.kset->kobj.sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* not found try to create */
		svc = kobject_create_and_add("svc", &dev->kobj.kset->kobj);
		if (IS_ERR_OR_NULL(svc)) {
			pr_err("Failed to create sys/devices/svc\n");
			return -ENOENT;
		}
	} else {
		svc = (struct kobject *)svc_sd->priv;
	}

	svc_sd = sysfs_get_dirent(svc->sd, "battery");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* create /sys/devices/svc/battery */
		battery = kobject_create_and_add("battery", svc);
		if (IS_ERR_OR_NULL(battery)) {
			pr_err("Failed to create sys/devices/svc/battery\n");
			goto error_create_svc_battery;
		}
	} else {
		battery = (struct kobject *)svc_sd->priv;
	}

	/* create /sys/devices/svc/battery/SVC_battery */
	ret = sysfs_create_file(battery, &sysfs_SVC_Battery_attr.attr);
	if (ret) {
		pr_err("sysfs create fail-%s\n", sysfs_SVC_Battery_attr.attr.name);
		goto error_create_sysfs;
	}
	return 0;

error_create_sysfs:
	kobject_put(battery);
error_create_svc_battery:
	kobject_put(svc);

	return -ENOENT;
}

static int sec_auth_create_svc_attrs2(struct device *dev)
{
	int ret;
	struct kernfs_node *svc_sd = NULL;
	struct kobject *svc = NULL;
	struct kobject *battery = NULL;

	/* To find /sys/devices/svc/ */
	svc_sd = sysfs_get_dirent(dev->kobj.kset->kobj.sd, "svc");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* not found try to create */
		svc = kobject_create_and_add("svc", &dev->kobj.kset->kobj);
		if (IS_ERR_OR_NULL(svc)) {
			pr_err("Failed to create sys/devices/svc\n");
			return -ENOENT;
		}
	} else {
		svc = (struct kobject *)svc_sd->priv;
	}

	svc_sd = sysfs_get_dirent(svc->sd, "battery");
	if (IS_ERR_OR_NULL(svc_sd)) {
		/* create /sys/devices/svc/battery */
		battery = kobject_create_and_add("battery", svc);
		if (IS_ERR_OR_NULL(battery)) {
			pr_err("Failed to create sys/devices/svc/battery\n");
			goto error_create_svc_battery;
		}
	} else {
		battery = (struct kobject *)svc_sd->priv;
	}

	/* create /sys/devices/svc/battery/SVC_battery2 */
	ret = sysfs_create_file(battery, &sysfs_SVC_Battery_attr2.attr);
	if (ret) {
		pr_err("sysfs create fail-%s\n", sysfs_SVC_Battery_attr2.attr.name);
		goto error_create_sysfs;
	}

	return 0;

error_create_sysfs:
	kobject_put(battery);
error_create_svc_battery:
	kobject_put(svc);

	return -ENOENT;
}

static struct device_attribute sec_auth_attrs[] = {
	SEC_AUTH_ATTR(presence),
	SEC_AUTH_ATTR(batt_auth),
	SEC_AUTH_ATTR(decrement_counter),
	SEC_AUTH_ATTR(first_use_date),
	SEC_AUTH_ATTR(use_date_wlock),
	SEC_AUTH_ATTR(batt_discharge_level),
	SEC_AUTH_ATTR(batt_full_status_usage),
	SEC_AUTH_ATTR(bsoh),
	SEC_AUTH_ATTR(bsoh_raw),
	SEC_AUTH_ATTR(qr_code),
	SEC_AUTH_ATTR(asoc),
	SEC_AUTH_ATTR(cap_nom),
	SEC_AUTH_ATTR(cap_max),
	SEC_AUTH_ATTR(batt_thm_min),
	SEC_AUTH_ATTR(batt_thm_max),
	SEC_AUTH_ATTR(unsafety_temp),
	SEC_AUTH_ATTR(vbat_ovp),
	SEC_AUTH_ATTR(recharging_count),
	SEC_AUTH_ATTR(safety_timer),
	SEC_AUTH_ATTR(drop_sensor),
	SEC_AUTH_ATTR(sync_buf_mem),
	SEC_AUTH_ATTR(sync_buf_mem_sts),
	SEC_AUTH_ATTR(fai_expired),
	SEC_AUTH_ATTR(chipname),
	SEC_AUTH_ATTR(batt_heatmap),
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	SEC_AUTH_ATTR(tau_values),
	SEC_AUTH_ATTR(check_passrate),
	SEC_AUTH_ATTR(work_start),
#endif
};

static int sec_auth_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(sec_auth_attrs); i++) {
		rc = device_create_file(dev, &sec_auth_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &sec_auth_attrs[i]);
	return rc;
}

ssize_t sec_auth_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - sec_auth_attrs;
	union power_supply_propval value = {0, };
	int ret = 0;
	int i = 0;

	switch (offset) {
	case PRESENCE:
		value.intval = (auth_ic_list & 1) ? 1 : 0;
		pr_info("%s: Presence(%d)\n", __func__, value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case BATT_AUTH:
		value.intval = is_page_buf_init ? is_authentic : 0;
		pr_info("%s: Authentication (%d), Page Buf Init (%d) , ret (%d)\n",
				__func__, is_authentic, is_page_buf_init, value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case DECREMENT_COUNTER:
		break;
	case FIRST_USE_DATE:
	{
		unsigned char temp_buf[FIRST_USE_DATE_BYTE_CNT + 1] = {0, };

		ret = get_first_use_date(temp_buf);
		if (ret)
			return ret;

		pr_info("%s: First use date(%s) show attr\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case USE_DATE_WLOCK:
	{
		if (pageLockStatus[FIRST_USE_DATE_PAGE] && pageLockStatus[FIRST_USE_DATE_PAGE + 1])
			ret = 0x02;		// PROP_WP - write protected
		else
			ret = 0;		// unlocked

		pr_info("%s: First use date lock status (%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
	}
		break;
	case BATT_DISCHARGE_LEVEL:
	{
		unsigned char temp_buf[BATT_DISCHARGE_LEVEL_BYTE_CNT + 1] = {0,};

		ret = get_batt_discharge_level(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, BATT_DISCHARGE_LEVEL_BYTE_CNT);
		pr_info("%s: Batt Discharge level(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
	}
		break;
	case BATT_FULL_STATUS_USAGE:
	{
		unsigned char temp_buf[BATT_FULL_STATUS_USAGE_CNT + 1] = {0,};

		ret = get_batt_full_status_usage(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, BATT_FULL_STATUS_USAGE_CNT);
		pr_info("%s: Batt full status usage(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
	}
		break;
	case BSOH:
	{
		unsigned char temp_buf[BSOH_BYTE_CNT + 1] = {0,};

		ret = get_bsoh(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, BSOH_BYTE_CNT);
		pr_info("%s: BSOH(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
	}
		break;
	case BSOH_RAW:
	{
		unsigned char temp_buf[BSOH_RAW_BYTE_CNT + 1] = {0,};

		ret = get_bsoh_raw(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, BSOH_RAW_BYTE_CNT);
		pr_info("%s: BSOH raw(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
	}
		break;
	case QR_CODE:
	{
		char temp_buf[QR_CODE_BYTE_CNT + 1] = {0, };

		ret = get_qr_code(temp_buf);
		if (ret)
			return ret;

		pr_info("%s: qr_code(%s) show attrs\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case ASOC:
	{
		unsigned char temp_buf[PAGE_SIZE] = {0,};

		ret = get_asoc(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, ASOC_BYTE_CNT);
		pr_info("%s: Asoc(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
	}
		break;
	case CAP_NOM:
		break;
	case CAP_MAX:
		break;
	case BATT_THM_MIN:
		break;
	case BATT_THM_MAX:
		break;
	case UNSAFETY_TEMP:
		break;
	case VBAT_OVP:
		break;
	case RECHARGING_COUNT:
		break;
	case SAFETY_TIMER:
		break;
	case DROP_SENSOR:
		break;
	case SYNC_BUF_MEM:
		break;
	case SYNC_BUF_MEM_STS:
	{
		pr_info("%s: SYNC_BUF_MEM_STS(%d)\n", __func__, sync_status);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			sync_status);
	}
		break;
	case FAI_EXPIRED:
	{
		char temp_buf[FAI_EXPIRED_BYTE_CNT + 1] = {0, };

		ret = get_fai_expired(temp_buf);
		if (ret)
			return ret;

		pr_info("%s: fai_expired(%s)\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case CHIPNAME:
		pr_info("%s: chipname(%s)\n", __func__, CHIP_NAME);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
			CHIP_NAME);
		break;
	case BATT_HEATMAP:
	{
		int temp_buf[64] = { 0 };
		int itr;

		ret = get_heatmap(temp_buf, 64);
		for (itr = 0; itr < 64; itr++) {
			i += scnprintf(buf + i, sizeof(temp_buf), "%d ",
			temp_buf[itr]);
		}
	}
		break;
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	case TAU_VALUES:
		pr_info("%s: 1tau_value (%d), 3tau_value (%d), 5tau_value (%d), response_timeout_value (%d)\n",
			__func__, Highspeed_BaudLow1, Highspeed_BaudHigh1,
			Highspeed_BaudStop1, Highspeed_ResponseTimeOut);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d %d %d %d\n",
			Highspeed_BaudLow1, Highspeed_BaudHigh1,
			Highspeed_BaudStop1, Highspeed_ResponseTimeOut);
		break;
	case CHECK_PASSRATE:
		break;
	case WORK_START:
		break;
#endif
	case AUTH_SYSFS_MAX:
		break;
	default:
		i = -EINVAL;
		break;
	}
	return i;
}

ssize_t sec_auth_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	const ptrdiff_t offset = attr - sec_auth_attrs;
	int ret = 0;
	int num = 0;
	int err = 0;

	switch (offset) {
	case PRESENCE:
		ret = count;
		break;
	case BATT_AUTH:
		ret = count;
		break;
	case DECREMENT_COUNTER:
		ret = count;
		break;
	case FIRST_USE_DATE:
	{
		unsigned char temp_buf[FIRST_USE_DATE_BYTE_CNT + 1] = {0,};

		if (sscanf(buf, "%s", temp_buf) == 1) {
			// TBD : add page locked check
			// if (pageLockStatus[FIRST_USE_DATE_PAGE] && pageLockStatus[FIRST_USE_DATE_PAGE + 1])
				// return -EINVAL;

			err = set_first_use_date(temp_buf);
			if (err) {
				sync_status = SYNC_FAILURE;
				return err;
			}

			/* send uevent for memory sync */
			sync_status = SYNC_PROGRESS;
			err = kobject_uevent_env(nvm_write_kobj, KOBJ_CHANGE, sync_event);
			if (err < 0) {
				sync_status = SYNC_FAILURE;
				pr_err("Error sending uevent for nvm_write kobject (%d)\n", err);
				return err;
			}
		}
		ret = count;
	}
		break;
	case USE_DATE_WLOCK:
	{
		if (kstrtoint(buf, 0, &num) == 0) {
			if (pageLockStatus[FIRST_USE_DATE_PAGE] && pageLockStatus[FIRST_USE_DATE_PAGE + 1])
				return count;

			err = set_first_use_date_page_lock();
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case BATT_DISCHARGE_LEVEL:
	{
		unsigned char temp_buf[BATT_DISCHARGE_LEVEL_BYTE_CNT + 1] = {0,};

		if (kstrtoint(buf, 0, &num) == 0) {
			integer_to_bytes(num, temp_buf, BATT_DISCHARGE_LEVEL_BYTE_CNT);
			err = set_batt_discharge_level(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case BATT_FULL_STATUS_USAGE:
	{
		unsigned char temp_buf[BATT_FULL_STATUS_USAGE_CNT + 1] = {0,};

		if (kstrtoint(buf, 0, &num) == 0) {
			integer_to_bytes(num, temp_buf, BATT_FULL_STATUS_USAGE_CNT);
			err = set_batt_full_status_usage(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case BSOH:
	{
		unsigned char temp_buf[BSOH_BYTE_CNT + 1] = {0,};

		if (kstrtoint(buf, 0, &num) == 0) {
			integer_to_bytes(num, temp_buf, BSOH_BYTE_CNT);
			err = set_bsoh(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case BSOH_RAW:
	{
		unsigned char temp_buf[BSOH_RAW_BYTE_CNT + 1] = {0,};

		if (kstrtoint(buf, 0, &num) == 0) {
			integer_to_bytes(num, temp_buf, BSOH_RAW_BYTE_CNT);
			err = set_bsoh_raw(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case QR_CODE:
		ret = count;
		break;
	case ASOC:
	{
		unsigned char temp_buf[ASOC_BYTE_CNT + 1] = {0,};

		if (kstrtoint(buf, 0, &num) == 0) {
			integer_to_bytes(num, temp_buf, ASOC_BYTE_CNT);
			err = set_asoc(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case CAP_NOM:
		ret = count;
		break;
	case CAP_MAX:
		ret = count;
		break;
	case BATT_THM_MIN:
		ret = count;
		break;
	case BATT_THM_MAX:
		ret = count;
		break;
	case UNSAFETY_TEMP:
		ret = count;
		break;
	case VBAT_OVP:
		ret = count;
		break;
	case RECHARGING_COUNT:
		ret = count;
		break;
	case SAFETY_TIMER:
		ret = count;
		break;
	case DROP_SENSOR:
		ret = count;
		break;
	case SYNC_BUF_MEM:
	{
		if (kstrtoint(buf, 0, &num) == 0) {
			sync_status = SYNC_PROGRESS;
			err = kobject_uevent_env(nvm_write_kobj, KOBJ_CHANGE, sync_event);

			if (err < 0) {
				sync_status = SYNC_FAILURE;
				pr_err("Error sending uevent for nvm_write kobject (%d)\n", err);
				return err;
			}
		}

		ret = count;
	}
		break;
	case SYNC_BUF_MEM_STS:
		ret = count;
		break;
	case FAI_EXPIRED:
	{
		unsigned char temp_buf[FAI_EXPIRED_BYTE_CNT + 1] = {0,};

		if (sscanf(buf, "%s", temp_buf) == 1) {
			err = set_fai_expired(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case CHIPNAME:
		break;
	case BATT_HEATMAP:
	{
		int i = 0;
		int temp_buf[64] = {0,};

		if (sscanf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
		&temp_buf[0], &temp_buf[1], &temp_buf[2], &temp_buf[3], &temp_buf[4],
		&temp_buf[5], &temp_buf[6], &temp_buf[7], &temp_buf[8], &temp_buf[9],
		&temp_buf[10], &temp_buf[11], &temp_buf[12], &temp_buf[13], &temp_buf[14],
		&temp_buf[15], &temp_buf[16], &temp_buf[17], &temp_buf[18], &temp_buf[19],
		&temp_buf[20], &temp_buf[21], &temp_buf[22], &temp_buf[23], &temp_buf[24],
		&temp_buf[25], &temp_buf[26], &temp_buf[27], &temp_buf[28], &temp_buf[29],
		&temp_buf[30], &temp_buf[31], &temp_buf[32], &temp_buf[33], &temp_buf[34],
		&temp_buf[35], &temp_buf[36], &temp_buf[37], &temp_buf[38], &temp_buf[39],
		&temp_buf[40], &temp_buf[41], &temp_buf[42], &temp_buf[43], &temp_buf[44],
		&temp_buf[45], &temp_buf[46], &temp_buf[47], &temp_buf[48], &temp_buf[49],
		&temp_buf[50], &temp_buf[51], &temp_buf[52], &temp_buf[53], &temp_buf[54],
		&temp_buf[55], &temp_buf[56], &temp_buf[57], &temp_buf[58], &temp_buf[59],
		&temp_buf[60], &temp_buf[61], &temp_buf[62], &temp_buf[63]) <= 64) {
			for (i = 0; i < 64; i++) {
				if (temp_buf[i] > 4095)
					temp_buf[i] = 4095;
				pr_info("%s: temp_buf[%d]: %d  ", __func__, i, temp_buf[i]);
			}
			set_heatmap(temp_buf, 64);
		}
		ret = count;
	}
		break;

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	case TAU_VALUES:
	{
		int tau1 = 0;
		int tau3 = 0;
		int tau5 = 0;
		int response_timeout = 0;

		if (sscanf(buf, "%d %d %d %d", &tau1, &tau3, &tau5, &response_timeout) == 4) {
			Highspeed_BaudLow1 = tau1;
			Highspeed_BaudHigh1 = tau3;
			Highspeed_BaudStop1 = tau5;
			Highspeed_ResponseTimeOut = response_timeout;
			pr_info("%s: Updated 1tau_value (%d), 3tau_value (%d), 5tau_value (%d), response_timeout_value (%d)\n",
				__func__, Highspeed_BaudLow1, Highspeed_BaudHigh1,
				Highspeed_BaudStop1, Highspeed_ResponseTimeOut);
		}
		ret = count;
	}
		break;
	case CHECK_PASSRATE:
	{
		if (kstrtoint(buf, 0, &num) == 0) {
			err = kobject_uevent_env(nvm_write_kobj, KOBJ_CHANGE, auth_pass_event);
			if (err < 0) {
				pr_err("Error sending uevent for nvm_write kobject passrate (%d)\n", err);
				return err;
			}
		}
		ret = count;
	}
		break;
	case WORK_START:
	{
		if (kstrtoint(buf, 0, &num) == 0) {
			work_sched_delay = num;
			cancel_delayed_work(&g_auth_driv->sec_auth_poll_work);
			queue_delayed_work(g_auth_driv->sec_auth_poll_wqueue,
							  &g_auth_driv->sec_auth_poll_work, work_sched_delay * HZ);
		}

		ret = count;
	}
		break;
#endif
	case AUTH_SYSFS_MAX:
		ret = count;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int sec_auth_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	return 0;
}
static int sec_auth_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	return 0;
}
static enum power_supply_property sec_auth_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct power_supply_desc sec_auth_power_supply_desc = {
	.name = "sec_auth",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sec_auth_props,
	.num_properties = ARRAY_SIZE(sec_auth_props),
	.get_property = sec_auth_get_property,
	.set_property = sec_auth_set_property,
};

static struct device_attribute sec_auth_2nd_attrs[] = {
	SEC_AUTH_2ND_ATTR(presence),
	SEC_AUTH_2ND_ATTR(batt_auth),
	SEC_AUTH_2ND_ATTR(decrement_counter),
	SEC_AUTH_2ND_ATTR(first_use_date),
	SEC_AUTH_2ND_ATTR(use_date_wlock),
	SEC_AUTH_2ND_ATTR(batt_discharge_level),
	SEC_AUTH_2ND_ATTR(batt_full_status_usage),
	SEC_AUTH_2ND_ATTR(bsoh),
	SEC_AUTH_2ND_ATTR(bsoh_raw),
	SEC_AUTH_2ND_ATTR(qr_code),
	SEC_AUTH_2ND_ATTR(asoc),
	SEC_AUTH_2ND_ATTR(cap_nom),
	SEC_AUTH_2ND_ATTR(cap_max),
	SEC_AUTH_2ND_ATTR(batt_thm_min),
	SEC_AUTH_2ND_ATTR(batt_thm_max),
	SEC_AUTH_2ND_ATTR(unsafety_temp),
	SEC_AUTH_2ND_ATTR(vbat_ovp),
	SEC_AUTH_2ND_ATTR(recharging_count),
	SEC_AUTH_2ND_ATTR(safety_timer),
	SEC_AUTH_2ND_ATTR(drop_sensor),
	SEC_AUTH_2ND_ATTR(sync_buf_mem),
	SEC_AUTH_2ND_ATTR(sync_buf_mem_sts),
	SEC_AUTH_2ND_ATTR(fai_expired),
	SEC_AUTH_2ND_ATTR(chipname),
	SEC_AUTH_2ND_ATTR(batt_heatmap),
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	SEC_AUTH_2ND_ATTR(tau_values),
	SEC_AUTH_2ND_ATTR(check_passrate),
	SEC_AUTH_2ND_ATTR(work_start),
#endif
};

static int sec_auth_2nd_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(sec_auth_2nd_attrs); i++) {
		rc = device_create_file(dev, &sec_auth_2nd_attrs[i]);
		if (rc)
			goto create_attrs2_failed;
	}
	return rc;

create_attrs2_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &sec_auth_2nd_attrs[i]);
	return rc;
}

ssize_t sec_auth_2nd_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	const ptrdiff_t offset = attr - sec_auth_2nd_attrs;
	union power_supply_propval value = {0, };
	int ret = 0;
	int i = 0;

	switch (offset) {
	case PRESENCE:
		value.intval = (auth_ic_list & (1 << 1)) ? 1 : 0;
		pr_info("%s: Presence(%d)\n", __func__, value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case BATT_AUTH:
		value.intval = is_page_buf_init2 ? is_authentic2 : 0;
		pr_info("%s: Authentication (%d), Page Buf Init (%d) , ret (%d)\n",
				__func__, is_authentic2, is_page_buf_init2, value.intval);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			value.intval);
		break;
	case DECREMENT_COUNTER:
		break;
	case FIRST_USE_DATE:
	{
		unsigned char temp_buf[FIRST_USE_DATE_BYTE_CNT + 1] = {0, };

		ret = get_first_use_date2(temp_buf);
		if (ret)
			return ret;

		pr_info("%s: First use date(%s) show attr\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case USE_DATE_WLOCK:
	{
		if (pageLockStatus2[FIRST_USE_DATE_PAGE] && pageLockStatus2[FIRST_USE_DATE_PAGE + 1])
			ret = 0x02;		// PROP_WP - write protected
		else
			ret = 0;		// unlocked

		pr_info("%s: First use date lock status (%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
	}
		break;
	case BATT_DISCHARGE_LEVEL:
	{
		unsigned char temp_buf[BATT_DISCHARGE_LEVEL_BYTE_CNT + 1] = {0,};

		ret = get_batt_discharge_level2(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, BATT_DISCHARGE_LEVEL_BYTE_CNT);
		pr_info("%s: Batt Discharge level(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
	}
		break;
	case BATT_FULL_STATUS_USAGE:
	{
		unsigned char temp_buf[BATT_FULL_STATUS_USAGE_CNT + 1] = {0,};

		ret = get_batt_full_status_usage2(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, BATT_FULL_STATUS_USAGE_CNT);
		pr_info("%s: Batt full status usage(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
	}
		break;
	case BSOH:
	{
		unsigned char temp_buf[BSOH_BYTE_CNT + 1] = {0,};

		ret = get_bsoh2(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, BSOH_BYTE_CNT);
		pr_info("%s: BSOH(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
	}
		break;
	case BSOH_RAW:
	{
		unsigned char temp_buf[BSOH_RAW_BYTE_CNT + 1] = {0,};

		ret = get_bsoh_raw2(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, BSOH_RAW_BYTE_CNT);
		pr_info("%s: BSOH raw(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
	}
		break;
	case QR_CODE:
	{
		char temp_buf[QR_CODE_BYTE_CNT + 1] = {0, };

		ret = get_qr_code2(temp_buf);
		if (ret)
			return ret;

		pr_info("%s: qr_code(%s) show attrs\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case ASOC:
	{
		unsigned char temp_buf[PAGE_SIZE] = {0,};

		ret = get_asoc2(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, ASOC_BYTE_CNT);
		pr_info("%s: Asoc(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			ret);
	}
		break;
	case CAP_NOM:
		break;
	case CAP_MAX:
		break;
	case BATT_THM_MIN:
		break;
	case BATT_THM_MAX:
		break;
	case UNSAFETY_TEMP:
		break;
	case VBAT_OVP:
		break;
	case RECHARGING_COUNT:
		break;
	case SAFETY_TIMER:
		break;
	case DROP_SENSOR:
		break;
	case SYNC_BUF_MEM:
		break;
	case SYNC_BUF_MEM_STS:
	{
		pr_info("%s: SYNC_BUF_MEM_STS(%d)\n", __func__, sync_status2);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d\n",
			sync_status2);
	}
		break;
	case FAI_EXPIRED:
	{
		char temp_buf[FAI_EXPIRED_BYTE_CNT + 1] = {0, };

		ret = get_fai_expired2(temp_buf);
		if (ret)
			return ret;

		pr_info("%s: fai_expired(%s)\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case CHIPNAME:
		pr_info("%s: chipname(%s)\n", __func__, CHIP_NAME);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%s\n",
			CHIP_NAME);
		break;
	case BATT_HEATMAP:
	{
		int temp_buf[64] = { 0 };
		int itr;

		ret = get_heatmap2(temp_buf, 64);
		for (itr = 0; itr < 64; itr++) {
			i += scnprintf(buf + i, sizeof(temp_buf), "%d ",
			temp_buf[itr]);
		}
	}
		break;
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	case TAU_VALUES:
		pr_info("%s: 1tau_value2 (%d), 3tau_value2 (%d), 5tau_value2 (%d), response_timeout_value (%d)\n",
			__func__, Highspeed_BaudLow2, Highspeed_BaudHigh2,
			Highspeed_BaudStop2, Highspeed_ResponseTimeOut);
		i += scnprintf(buf + i, PAGE_SIZE - i, "%d %d %d %d\n",
			Highspeed_BaudLow2, Highspeed_BaudHigh2,
			Highspeed_BaudStop2, Highspeed_ResponseTimeOut);
		break;
	case CHECK_PASSRATE:
		break;
	case WORK_START:
		break;
#endif
	case AUTH_SYSFS_MAX:
		break;
	default:
		i = -EINVAL;
		break;
	}
	return i;
}

ssize_t sec_auth_2nd_store_attrs(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	const ptrdiff_t offset = attr - sec_auth_2nd_attrs;
	int ret = 0;
	int num = 0;
	int err = 0;

	switch (offset) {
	case PRESENCE:
		ret = count;
		break;
	case BATT_AUTH:
		ret = count;
		break;
	case DECREMENT_COUNTER:
		ret = count;
		break;
	case FIRST_USE_DATE:
	{
		unsigned char temp_buf[FIRST_USE_DATE_BYTE_CNT + 1] = {0,};

		if (sscanf(buf, "%s", temp_buf) == 1) {
			// TBD : add page locked check
			// if (pageLockStatus2[FIRST_USE_DATE_PAGE] && pageLockStatus2[FIRST_USE_DATE_PAGE + 1])
				// return -EINVAL;

			err = set_first_use_date2(temp_buf);
			if (err) {
				sync_status2 = SYNC_FAILURE;
				return err;
			}

			/* send uevent for memory sync */
			sync_status2 = SYNC_PROGRESS;
			err = kobject_uevent_env(nvm_write_kobj, KOBJ_CHANGE, sync_event2);
			if (err < 0) {
				sync_status2 = SYNC_FAILURE;
				pr_err("Error sending uevent for nvm_write kobject (%d)\n", err);
				return err;
			}
		}
		ret = count;
	}
		break;
	case USE_DATE_WLOCK:
	{
		if (kstrtoint(buf, 0, &num) == 0) {
			if (pageLockStatus2[FIRST_USE_DATE_PAGE] && pageLockStatus2[FIRST_USE_DATE_PAGE + 1])
				return count;

			err = set_first_use_date_page_lock2();
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case BATT_DISCHARGE_LEVEL:
	{
		unsigned char temp_buf[BATT_DISCHARGE_LEVEL_BYTE_CNT + 1] = {0,};

		if (kstrtoint(buf, 0, &num) == 0) {
			integer_to_bytes(num, temp_buf, BATT_DISCHARGE_LEVEL_BYTE_CNT);
			err = set_batt_discharge_level2(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case BATT_FULL_STATUS_USAGE:
	{
		unsigned char temp_buf[BATT_FULL_STATUS_USAGE_CNT + 1] = {0,};

		if (kstrtoint(buf, 0, &num) == 0) {
			integer_to_bytes(num, temp_buf, BATT_FULL_STATUS_USAGE_CNT);
			err = set_batt_full_status_usage2(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case BSOH:
	{
		unsigned char temp_buf[BSOH_BYTE_CNT + 1] = {0,};

		if (kstrtoint(buf, 0, &num) == 0) {
			integer_to_bytes(num, temp_buf, BSOH_BYTE_CNT);
			err = set_bsoh2(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case BSOH_RAW:
	{
		unsigned char temp_buf[BSOH_RAW_BYTE_CNT + 1] = {0,};

		if (kstrtoint(buf, 0, &num) == 0) {
			integer_to_bytes(num, temp_buf, BSOH_RAW_BYTE_CNT);
			err = set_bsoh_raw2(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case QR_CODE:
		ret = count;
		break;
	case ASOC:
	{
		unsigned char temp_buf[ASOC_BYTE_CNT + 1] = {0,};

		if (kstrtoint(buf, 0, &num) == 0) {
			integer_to_bytes(num, temp_buf, ASOC_BYTE_CNT);
			err = set_asoc2(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case CAP_NOM:
		ret = count;
		break;
	case CAP_MAX:
		ret = count;
		break;
	case BATT_THM_MIN:
		ret = count;
		break;
	case BATT_THM_MAX:
		ret = count;
		break;
	case UNSAFETY_TEMP:
		ret = count;
		break;
	case VBAT_OVP:
		ret = count;
		break;
	case RECHARGING_COUNT:
		ret = count;
		break;
	case SAFETY_TIMER:
		ret = count;
		break;
	case DROP_SENSOR:
		ret = count;
		break;
	case SYNC_BUF_MEM:
	{
		if (kstrtoint(buf, 0, &num) == 0) {
			sync_status2 = SYNC_PROGRESS;
			err = kobject_uevent_env(nvm_write_kobj, KOBJ_CHANGE, sync_event2);

			if (err < 0) {
				sync_status2 = SYNC_FAILURE;
				pr_err("Error sending uevent for nvm_write kobject (%d)\n", err);
				return err;
			}
		}

		ret = count;
	}
		break;
	case SYNC_BUF_MEM_STS:
		ret = count;
		break;
	case FAI_EXPIRED:
	{
		unsigned char temp_buf[FAI_EXPIRED_BYTE_CNT + 1] = {0,};

		if (sscanf(buf, "%s", temp_buf) == 1) {
			err = set_fai_expired2(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case CHIPNAME:
		break;
	case BATT_HEATMAP:
	{
		int i = 0;
		int temp_buf[64] = {0,};

		if (sscanf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
		&temp_buf[0], &temp_buf[1], &temp_buf[2], &temp_buf[3], &temp_buf[4],
		&temp_buf[5], &temp_buf[6], &temp_buf[7], &temp_buf[8], &temp_buf[9],
		&temp_buf[10], &temp_buf[11], &temp_buf[12], &temp_buf[13], &temp_buf[14],
		&temp_buf[15], &temp_buf[16], &temp_buf[17], &temp_buf[18], &temp_buf[19],
		&temp_buf[20], &temp_buf[21], &temp_buf[22], &temp_buf[23], &temp_buf[24],
		&temp_buf[25], &temp_buf[26], &temp_buf[27], &temp_buf[28], &temp_buf[29],
		&temp_buf[30], &temp_buf[31], &temp_buf[32], &temp_buf[33], &temp_buf[34],
		&temp_buf[35], &temp_buf[36], &temp_buf[37], &temp_buf[38], &temp_buf[39],
		&temp_buf[40], &temp_buf[41], &temp_buf[42], &temp_buf[43], &temp_buf[44],
		&temp_buf[45], &temp_buf[46], &temp_buf[47], &temp_buf[48], &temp_buf[49],
		&temp_buf[50], &temp_buf[51], &temp_buf[52], &temp_buf[53], &temp_buf[54],
		&temp_buf[55], &temp_buf[56], &temp_buf[57], &temp_buf[58], &temp_buf[59],
		&temp_buf[60], &temp_buf[61], &temp_buf[62], &temp_buf[63]) <= 64) {
			for (i = 0; i < 64; i++) {
				if (temp_buf[i] > 4095)
					temp_buf[i] = 4095;
				pr_info("%s: temp_buf[%d]: %d  ", __func__, i, temp_buf[i]);
			}
			set_heatmap2(temp_buf, 64);
		}
		ret = count;
	}
		break;
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	case TAU_VALUES:
	{
		int tau1 = 0;
		int tau3 = 0;
		int tau5 = 0;
		int response_timeout = 0;

		if (sscanf(buf, "%d %d %d %d", &tau1, &tau3, &tau5, &response_timeout) == 4) {
			Highspeed_BaudLow2 = tau1;
			Highspeed_BaudHigh2 = tau3;
			Highspeed_BaudStop2 = tau5;
			Highspeed_ResponseTimeOut = response_timeout;
			pr_info("%s: Updated 1tau_value2 (%d), 3tau_value2 (%d), 5tau_value2 (%d), response_timeout_value (%d)\n",
				__func__, Highspeed_BaudLow2, Highspeed_BaudHigh2,
				Highspeed_BaudStop2, Highspeed_ResponseTimeOut);
		}
		ret = count;
	}
		break;
	case CHECK_PASSRATE:
	{
		pr_info("Inside check_passrate");
		if (kstrtoint(buf, 0, &num) == 0) {
			pr_info("before call");
			err = kobject_uevent_env(nvm_write_kobj, KOBJ_CHANGE, auth_pass_event);
			pr_info("after call");
			if (err < 0) {
				pr_err("Error sending uevent for nvm_write kobject passrate (%d)\n", err);
				return err;
			}
		}
		ret = count;
	}
		break;
	case WORK_START:
	{
		if (kstrtoint(buf, 0, &num) == 0) {
			work_sched_delay = num;
			cancel_delayed_work(&g_auth_driv->sec_auth_poll_work);
			queue_delayed_work(g_auth_driv->sec_auth_poll_wqueue,
							  &g_auth_driv->sec_auth_poll_work, work_sched_delay * HZ);
		}

		ret = count;
	}
		break;
#endif
	case AUTH_SYSFS_MAX:
		ret = count;
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int sec_auth_2nd_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	return 0;
}
static int sec_auth_2nd_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	return 0;
}
static enum power_supply_property sec_auth_2nd_props[] = {
	POWER_SUPPLY_PROP_ONLINE,
};

static const struct power_supply_desc sec_auth_2nd_power_supply_desc = {
	.name = "sec_auth_2nd",
	.type = POWER_SUPPLY_TYPE_UNKNOWN,
	.properties = sec_auth_2nd_props,
	.num_properties = ARRAY_SIZE(sec_auth_2nd_props),
	.get_property = sec_auth_2nd_get_property,
	.set_property = sec_auth_2nd_set_property,
};

/**
 * @brief Authenticate On module driver initialization
 * */
static int __init authon_driver_init(void)
{
	struct authon_driver_data *authon_driver;
	struct authon_driver_platform_data *pdata;
	struct power_supply_config psy_sec_auth = {};
	struct power_supply_config psy_sec_auth_2nd = {};
	struct device *dev;
	int ret = 0;
	uint16_t i = 0;

	pr_info(">> %s \n", __func__);

/***************************************************************************
 * function alloc_chrdev_region will be allocating a major number starting *
 * from minimum value of 0 to any number that's available in kernel, and a *
 * minor number starting from 1 to any number that's available in kernel.  *
 * Both major and minor number will be stored in dev_num.                  *
 **************************************************************************/
	reg_count_major = alloc_chrdev_region(&dev_num, 0, 1, "AuthOn");
	if (reg_count_major < 0) {
		pr_err("[authon_driver_init] Failed to register\n");
		return reg_count_major;
	}
#if (KERNEL_VERSION(6, 3, 0) <= LINUX_VERSION_CODE)
	authon_class = class_create("AuthOn");
#else
	authon_class = class_create(THIS_MODULE, "AuthOn");
#endif
	if (authon_class == NULL) {
		pr_err("[authon_driver_init] Cannot create class %s\n", "AuthOn");
		unregister_chrdev_region(dev_num, 1);
		return -EINVAL;
	}

	major_num = MAJOR(dev_num); // Extracting of major number
	pr_info("[authon_driver_init] Major number registered is %d\n", major_num);
	mcdev = cdev_alloc(); // to create cdev structure, to initialize our cdev
	mcdev->ops = &authon_driver_fops;
	mcdev->owner = THIS_MODULE;

/**************************************************************************
 * Now we have created cdev, next we have to add it into the kernel space *
 *************************************************************************/
	reg_count_major = cdev_add(mcdev, dev_num, 1);
	if (reg_count_major < 0) {
		pr_err("[authon_driver_init] unable to cdev to kernel.\n");
		class_destroy(authon_class);
		unregister_chrdev_region(dev_num, 1);
		return reg_count_major;
	}

	dev = device_create(authon_class, NULL, MKDEV(MAJOR(dev_num),
		MINOR(dev_num)), NULL, "AuthOn");
	if (dev == NULL) {
		class_destroy(authon_class);
		unregister_chrdev_region(dev_num, 1);
		return -1;
	}
	pr_info("[authon_driver_init] Added to kernel space\n");

	authon_driver = kzalloc(sizeof(*authon_driver), GFP_KERNEL);
	if (!authon_driver) {
		ret = -ENOMEM;
		goto cleanup;
	}
	g_auth_driv = authon_driver;
	authon_driver->dev = dev;

	pdata = kzalloc(sizeof(*(authon_driver->pdata)), GFP_KERNEL);
	if (!pdata) {
		ret = -ENOMEM;
		goto err_free;
	}

	authon_driver->pdata = pdata;
	ret = authon_driver_parse_dt(authon_driver->pdata);
	if (ret < 0) {
		pr_err("%s : Auth DTSI parsing failed ret[%d]\n", __func__, ret);
		goto err_pdata_free;
	}

	authon_cisd_data = kzalloc(sizeof(struct authon_cisd), GFP_KERNEL);
	if (!authon_cisd_data) {
		authon_cisd_data = NULL;
		ret = -ENOMEM;
		goto err_pdata_free;
	}

	authon_handler = kzalloc(sizeof(struct AUTH_ON_S), GFP_KERNEL);
	if (!(authon_handler)) {
		authon_handler = NULL;
		ret = -ENOMEM;
		goto err_cisd_data_free;
	}

	init_completion(&(authon_handler->event));
	mutex_init(&authon_driver->sec_auth_mutex);

	auth_ic_list = 0;

	for (i = 0; i < pdata->swi_gpio_cnt; i++) {
		SWI_GPIO_PIN = pdata->swi_gpio[i];
		active_device = i + 1;
		pr_info("%s: swi gpio[%d] = %d\n", __func__, i, SWI_GPIO_PIN);

		if (i == 0) {
			// Initialize Baud for main
			Highspeed_BaudLow = Highspeed_BaudLow1;
			Highspeed_BaudHigh = Highspeed_BaudHigh1;
			Highspeed_BaudStop = Highspeed_BaudStop1;
		} else if (i == 1) {
			// Initialize Baud for sub
			Highspeed_BaudLow = Highspeed_BaudLow2;
			Highspeed_BaudHigh = Highspeed_BaudHigh2;
			Highspeed_BaudStop = Highspeed_BaudStop2;
		}

		ret = platform_init();
		if (ret != SDK_SUCCESS) {
			pr_err("%s | Error: init platform GPIO fails, ret=0x%X\r\n", __func__, ret);
			ret = SDK_INIT;
			continue;
		}

		memset(authon_capability_handle, 0, sizeof(AuthOn_Capability));
		authon_capability_handle->swi.gpio = SWI_GPIO_PIN;
		ret = authon_sdk_init();
		if (ret != SDK_SUCCESS) {
			pr_err("%s | Error: SDK init fails, ret=0x%X\r\n", __func__, ret);
			ret = SDK_INIT;
			gpio_free(SWI_GPIO_PIN);
			continue;
		}
		auth_ic_list |= active_device;
	}
	if (!auth_ic_list) {
		pr_err("%s | Error: Infineon authentication IC not present\n", __func__);
		goto err_authon_handler_free;
	}

	for (i = 1; i <= 2; i++) {
		/* Check if authentication IC present or not */
		if(!(auth_ic_list & i))
			continue;

		if(i == 1) {
			/* Re-initialize SDK to handle case when Infineon IC is present on Main but not on Sub
			 * AuthOn_Capability resets when sub battery infineon auth IC is not found
			 * AuthOn_Capability is required to be initialized at least once for proper operation
			 */
			SWI_GPIO_PIN = pdata->swi_gpio[i-1];
			active_device = i;
			// Initialize Baud for main
			Highspeed_BaudLow = Highspeed_BaudLow1;
			Highspeed_BaudHigh = Highspeed_BaudHigh1;
			Highspeed_BaudStop = Highspeed_BaudStop1;

			memset(authon_capability_handle, 0, sizeof(AuthOn_Capability));
			authon_capability_handle->swi.gpio = SWI_GPIO_PIN;
			ret = authon_sdk_init();
			if (ret != SDK_SUCCESS) {
				pr_err("%s | Error: Re-SDK init fails, ret=0x%X\n", __func__, ret);
				/* unset auth_ic_list bit 0 */
				auth_ic_list &= ~(1 << (i-1));
				continue;
			}

			psy_sec_auth.drv_data = authon_driver;
			authon_driver->psy_sec_auth = power_supply_register(authon_driver->dev,
											&sec_auth_power_supply_desc, &psy_sec_auth);
			if (!authon_driver->psy_sec_auth) {
				dev_err(authon_driver->dev, "%s: Failed to power supply authon register", __func__);
				goto err_authon_handler_free;
			}

			ret = sec_auth_create_attrs(&authon_driver->psy_sec_auth->dev);
			if (ret) {
				pr_info("%s : Failed to create sysfs attrs\n", __func__);
				goto err_supply_unreg;
			}

			// "/sys/devices/svc/battery/SVC_battery" node creation
			ret = sec_auth_create_svc_attrs(authon_driver->dev);
			if (ret) {
				pr_info("%s : Failed to create svc attrs\n", __func__);
				goto err_supply_unreg;
			}
		} else {
			psy_sec_auth_2nd.drv_data = authon_driver;
			authon_driver->psy_sec_auth_2nd = power_supply_register(authon_driver->dev,
											&sec_auth_2nd_power_supply_desc, &psy_sec_auth_2nd);
			if (!authon_driver->psy_sec_auth_2nd) {
				dev_err(authon_driver->dev, "%s: Failed to power supply authon 2 register", __func__);
				goto err_authon_handler_free;
			}

			ret = sec_auth_2nd_create_attrs(&authon_driver->psy_sec_auth_2nd->dev);
			if (ret) {
				pr_info("%s : Failed to create sysfs 2 attrs\n", __func__);
				goto err_supply_unreg;
			}

			// "/sys/devices/svc/battery/SVC_battery2" node creation
			ret = sec_auth_create_svc_attrs2(authon_driver->dev);
			if (ret) {
				pr_info("%s : Failed to create svc2 attrs\n", __func__);
				goto err_supply_unreg;
			}
		}
	}

	/* assing kobject for uevent */
	nvm_write_kobj = &authon_driver->dev->kobj;
	if (!nvm_write_kobj) {
		pr_err("Failed to create nvm_write kobject\n");
		goto err_supply_unreg;
	}

	/* create poll work queue */
	authon_driver->sec_auth_poll_wqueue = create_singlethread_workqueue("sle956681_auth_work_queue");
	if (!authon_driver->sec_auth_poll_wqueue) {
		pr_info("%s : Failed to create poll work queue\n", __func__);
		goto err_nvm_write_kobj;
	}

	INIT_DELAYED_WORK(&authon_driver->sec_auth_poll_work, sec_auth_poll_work_func);

	sle956681_init_complete = 1;
	pr_info("<< %s \n", __func__);
	return 0;

err_nvm_write_kobj:
	kobject_put(nvm_write_kobj);
err_supply_unreg:
	if (authon_driver->psy_sec_auth)
		power_supply_unregister(authon_driver->psy_sec_auth);
	if (authon_driver->psy_sec_auth_2nd)
		power_supply_unregister(authon_driver->psy_sec_auth_2nd);
err_authon_handler_free:
	kfree(authon_handler);
	mutex_destroy(&authon_driver->sec_auth_mutex);
err_cisd_data_free:
	kfree(authon_cisd_data);
err_pdata_free:
	kfree(pdata);
err_free:
	kfree(authon_driver);
cleanup:
	device_destroy(authon_class, MKDEV(MAJOR(dev_num), MINOR(dev_num)));
	cdev_del(mcdev);
	class_destroy(authon_class);
	unregister_chrdev_region(dev_num, 1);

	sle956681_init_complete = 1;
	return 0;
}
module_init(authon_driver_init);

/**
 * @brief Exit Authenticate On module driver
 * */
static void __exit authon_driver_exit(void)
{
	flush_workqueue(g_auth_driv->sec_auth_poll_wqueue);
	destroy_workqueue(g_auth_driv->sec_auth_poll_wqueue);

	/* Triger uevent on removal for nvm_write kobject */
	kobject_uevent(nvm_write_kobj, KOBJ_REMOVE);

	/* Remove nvm_write kobject */
	kobject_put(nvm_write_kobj);

	device_destroy(authon_class, MKDEV(MAJOR(dev_num), MINOR(dev_num)));
	cdev_del(mcdev); // remove the character devices from this system
	class_destroy(authon_class);
	unregister_chrdev_region(dev_num, 1);
	kfree(authon_handler);

	pr_info("[authon_driver_exit] Module removed.\n");
}
module_exit(authon_driver_exit);

/**
 * @brief Cleans up Authenticate On module driver
 * */
void cleanup(void){
	cancel_delayed_work(&g_auth_driv->sec_auth_poll_work);
	kobject_put(nvm_write_kobj);
	device_destroy(authon_class, MKDEV(MAJOR(dev_num), MINOR(dev_num)));
	cdev_del(mcdev); // remove the character devices from this system
	unregister_chrdev_region(dev_num, 1);
	class_destroy(authon_class);
	kfree(authon_handler);
}

MODULE_DESCRIPTION("OPTIGA Authenticate On Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("20230902");