/*
 * sec_auth_ds28e30.c
 * Samsung Mobile Battery Authentication Driver
 *
 * Copyright (C) 2023 Samsung Electronics
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/device.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spinlock.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/rtc.h>
#include <linux/ktime.h>
#include <linux/power_supply.h>
#include <linux/sched.h>

#include "../../common/sec_charging_common.h"
#include "sec_auth_ds28e30.h"
#include "ecc_generate_key.h"
#include "deep_cover_coproc.h"
#include "ds28e30.h"
#include "keys.h"
#include "sec_auth_memory_map_ver_1.h"
#include "1wire_protocol.h"

#if !defined(CONFIG_SEC_FACTORY)
static uint32_t slavepcb;
module_param(slavepcb, uint, 0644);
MODULE_PARM_DESC(slavepcb, "slave pcb value");

static char *sales_code;
module_param(sales_code, charp, 0644);
MODULE_PARM_DESC(sales_code, "sales code value");

static unsigned int eur_slavepcb;
#endif

int Active_Device = -1;

static const char *CHIP_NAME = "ds28e30";

#if IS_ENABLED(CONFIG_BATTERY_AUTH_SLE956681)
extern int sle956681_init_complete;
#endif

// Spinlock controls
extern spinlock_t spinlock_swi_gpio;

//define system-level public key, authority public key  and certificate constant variables
unsigned char SystemPublicKeyX[32];
unsigned char SystemPublicKeyY[32];
unsigned char  AuthorityPublicKey_X[32];
unsigned char  AuthorityPublicKey_Y[32];
unsigned char Certificate_Constant[16];

// Define local buffer for pages data
#define DATA_BUF_SIZE 8
static unsigned char page_data_buf[DATA_BUF_SIZE][32];
static unsigned char page_data_buf2[DATA_BUF_SIZE][32];
static bool pageLockStatus[DATA_BUF_SIZE] = {true, false, false, false, false, false, false, false};
static bool pageLockStatus2[DATA_BUF_SIZE] = {true, false, false, false, false, false, false, false};
static bool pageBufDirty[DATA_BUF_SIZE];
static bool pageBufDirty2[DATA_BUF_SIZE];
static bool is_page_buf_init;
static bool is_page_buf_init2;
static bool is_ic_present, is_authentic, is_counter_init;
static bool is_ic_present2, is_authentic2, is_counter_init2;
static int decrement_count_val = 100000;
static int decrement_count_max;
static int decrement_count_val2 = 100000;
static int decrement_count_max2;
static int auth_ic_list[2];

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
int cpu_x = 0, cpu_y = 7;
static int cpu_pass_count[8];
static int cpu_test_count[8];
int test_count;
int pass_count;
#endif

static struct sec_auth_ds28e30_data *g_sec_auth_ds28e30;

// QOS_cpu_and_devfreq related
extern int sec_auth_cpu_related_things_init(void);
extern int sec_auth_remove_devfreq_int_request(void);
extern int sec_auth_add_devfreq_int_request(unsigned int frequency);
extern int sec_auth_remove_qos_request(void);
extern int sec_auth_add_qos_request(void);
extern void sec_auth_setCpuMask(int cpustart, int cpuend);
extern void sec_auth_clearCpuMask(void);

#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
static bool auth_cpufreq_set;
#endif
//static bool auth_devfreq_set;


static void ConfigureDS28E30Parameters(void);
static int check_device_authentication(void);
static int get_batt_discharge_level(unsigned char *data);
static int get_batt_discharge_level2(unsigned char *data);
static void sync_decrement_counter(unsigned char *buf);
static void sync_decrement_counter2(unsigned char *buf);
static void ow_gpio_init(struct sec_auth_ds28e30_platform_data *pdata);

// Schedule work delay in sec
static int work_sched_delay = MAX_WORK_DELAY;
#define DEBUG 0
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
int debug_page;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 0, 0))
#include <linux/time64.h>
#define SEC_TIMESPEC timespec64
#define SEC_GETTIMEOFDAY ktime_get_real_ts64
#define SEC_RTC_TIME_TO_TM rtc_time64_to_tm
#else
#include <linux/time.h>
#define SEC_TIMESPEC timeval
#define SEC_GETTIMEOFDAY do_gettimeofday
#define SEC_RTC_TIME_TO_TM rtc_time_to_tm
#endif

// cpu mask for cpu affinity set
static struct cpumask my_cpumask;
// inital value for cpu affinity
static struct cpumask cpu_allowed;

void sec_auth_setCpuMask(int cpustart, int cpuend)
{
	int i;
	struct task_struct *tsk = current;

	cpu_allowed = tsk->cpus_mask;
	cpumask_clear(&my_cpumask);
	for (i = cpustart; i <= cpuend; i++)
		cpumask_set_cpu(i, &my_cpumask);
	set_cpus_allowed_ptr(tsk, &my_cpumask);
}

void sec_auth_clearCpuMask(void)
{
	struct task_struct *tsk = current;

	cpumask_clear(&my_cpumask);
	set_cpus_allowed_ptr(tsk, &cpu_allowed);
}

static void integer_to_bytes(int num, unsigned char *data, int size)
{
	int i;
	
	for (i = 0; i < size; i++)
		data[i] = (num >> (8 * (size - i - 1))) & 0xFF;
}

static int byte_array_to_int(unsigned char *data, int size)
{
	int i = 0;
	int n = 0;

	for (i = 0; i < size; i++)
		n = (n << 8) | data[i];

	return n;
}

// static int setPageLock(int PageNumber)
// {
	// todo memory page lock api call;
	
	// pageLockStatus[PageNumber] = true;
	// pageBufDirty[PageNumber] = false;
// }

static void add_freq_lock(void)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	/*cpu 4 to cpu 6*/
	sec_auth_setCpuMask(g_sec_auth_ds28e30->pdata->cpu_start, g_sec_auth_ds28e30->pdata->cpu_end);

	if (auth_cpufreq_set) {
//		sec_auth_add_qos_request();
		sec_auth_add_devfreq_int_request(664000);
		auth_info("%s: freq request added\n", __func__);
	}
#elif defined(CONFIG_ARCH_QCOM)
	/*cpu 5 to cpu 6*/
	sec_auth_setCpuMask(g_sec_auth_ds28e30->pdata->cpu_start, g_sec_auth_ds28e30->pdata->cpu_end);
#else
	auth_dbg("%s: do not set cpumask\n", __func__);
#endif
	return;
}

static void remove_freq_lock(void)
{
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	if (auth_cpufreq_set) {
//		sec_auth_remove_qos_request();
		sec_auth_remove_devfreq_int_request();
		auth_info("%s: freq request removed\n", __func__);
	}
	sec_auth_clearCpuMask();
#elif defined(CONFIG_ARCH_QCOM)
	sec_auth_clearCpuMask();
#else
	auth_dbg("%s: no need to clear cpumask - nothing set\n", __func__);
#endif
	return;
}

static int read_from_page_memory(int pageNumber, char *data)
{
	unsigned char flag = false;

	flag = ds28e30_cmd_readMemory(pageNumber, data);
	if (!flag) {
		auth_err("%s: failed for page %d\n", __func__, pageNumber);
		return -EIO;
	}

	return 0;
}


static int write_to_page_memory(int pageNumber, char *data)
{
	unsigned char flag = false;

	flag = ds28e30_cmd_writeMemory(pageNumber, data);
	if (!flag) {
		auth_err("%s: failed for page %d\n", __func__, pageNumber);
		return -EIO;
	}

	return 0;
}

static int get_mem_page_lock_status(int page_num, char *data)
{
	unsigned char flag = false;

	auth_info("%s: called\n", __func__);
	flag = ds28e30_cmd_readStatus(page_num, data, 0, 0);
	if (!flag) {
		auth_err("%s: failed for page %d\n", __func__, page_num);
		return -EIO;
	}

	auth_info("%s: success\n", __func__);
	return 0;
}


static int set_mem_page_write_lock(int page_num)
{
	unsigned char flag = false;

	flag = ds28e30_cmd_setPageProtection(page_num, PROT_WP);
	if (!flag) {
		auth_err("%s: failed for page %d\n", __func__, page_num);
		return -EIO;
	}

	return 0;
}


static int save_buffer_to_memory(void)
{
	int i = 0, err = 0;
	unsigned char temp_buf[32] = {0,};

	Active_Device = 1;
	err = get_batt_discharge_level(temp_buf);
	if (err) {
		auth_err("%s: get batt discharge level failed, return\n", __func__);
		return err;
	}

	auth_info("%s: saving buffer to memory started\n", __func__);
	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);

	add_freq_lock();

	if (!is_authentic) {
		auth_err("%s: authentication fail\n", __func__);
		goto err_save_to_mem;
	}

	sync_decrement_counter(temp_buf);

	for (i = 0; i < DATA_BUF_SIZE; i++) {
		if(pageLockStatus[i])
			continue;

		if(!pageBufDirty[i])
			continue;

		err = write_to_page_memory(i, page_data_buf[i]);
		if (err) {
			auth_err("%s: write fail\n", __func__);
			goto err_save_to_mem;
		}

		pageBufDirty[i] = false;
	}
	remove_freq_lock();

	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	auth_info("%s: saving buffer to memory ended\n", __func__);
	return 0;

err_save_to_mem:

	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	auth_info("%s: saving buffer to memory failed\n", __func__);
	return -EIO;
}

static int save_buffer_to_memory2(void)
{
	int i = 0, err = 0;
	unsigned char temp_buf[32] = {0,};

	Active_Device = 2;
	err = get_batt_discharge_level2(temp_buf);
	if (err) {
		auth_err("%s: get batt discharge level failed, return\n", __func__);
		return err;
	}

	auth_info("%s: saving buffer to memory started\n", __func__);
	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);

	add_freq_lock();

	if (!is_authentic2) {
		auth_err("%s: authentication fail\n", __func__);
		goto err_save_to_mem;
	}

	sync_decrement_counter2(temp_buf);

	for (i = 0; i < DATA_BUF_SIZE; i++) {
		if (pageLockStatus2[i])
			continue;

		if (!pageBufDirty2[i])
			continue;

		err = write_to_page_memory(i, page_data_buf2[i]);
		if (err) {
			auth_err("%s: write fail\n", __func__);
			goto err_save_to_mem;
		}

		pageBufDirty2[i] = false;
	}
	remove_freq_lock();

	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	auth_info("%s: saving buffer to memory ended\n", __func__);
	return 0;

err_save_to_mem:

	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	auth_info("%s: saving buffer to memory failed\n", __func__);
	return -EIO;
}


static int write_to_page_buffer(int pageNumber, int startByte, int cnt, char *data)
{
	int i = 0;

	if (!is_page_buf_init) {
		auth_err("%s: page buf init fail\n", __func__);
		return -EINVAL;
	}

	if (pageLockStatus[pageNumber]) {
		auth_err("%s: page %d locked, return\n", __func__, pageNumber);
		return -EINVAL;
	}

	for(i = 0; i< cnt ; i++)
		page_data_buf[pageNumber][startByte + i] = data[i];

	pageBufDirty[pageNumber] = true;

	return 0;
}

static int read_from_page_buffer(int pageNumber, int startByte, int cnt, char *data)
{
	int i = 0;
	
	if (!is_page_buf_init) {
		auth_err("%s: page buf init fail\n", __func__);
		return -EINVAL;
	}

	for(i = 0; i< cnt; i++)
		data[i] = page_data_buf[pageNumber][startByte + i];

	return 0;
}

static int write_to_page_buffer2(int pageNumber, int startByte, int cnt, char *data)
{
	int i = 0;

	if (!is_page_buf_init2) {
		auth_info("%s: page buf init fail\n", __func__);
		return -EINVAL;
	}

	if (pageLockStatus2[pageNumber]) {
		auth_info("%s: page %d locked, return\n", __func__, pageNumber);
		return -EINVAL;
	}

	for (i = 0; i < cnt ; i++)
		page_data_buf2[pageNumber][startByte + i] = data[i];

	pageBufDirty2[pageNumber] = true;

	return 0;
}

static int read_from_page_buffer2(int pageNumber, int startByte, int cnt, char *data)
{
	int i = 0;

	if (!is_page_buf_init2) {
		auth_info("%s: page buf init fail\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < cnt; i++)
		data[i] = page_data_buf2[pageNumber][startByte + i];

	return 0;
}

static int check_device_presence(void)
{
	int is_present = 0;
	int i = 0;

	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		auth_info("%s: retry count(%d)\n", __func__, i);
		is_present = ow_reset();
		if (is_present) {
			if (Active_Device == 1)
				is_ic_present = true;
			else if (Active_Device == 2)
				is_ic_present2 = true;
			break;
		}
	}
	auth_info("%s: device present(%d)\n", __func__, is_present);
	return is_present;
}

static int check_family_and_configure_paramters(void)
{
	unsigned char flag = false;
	int i = 0;
	
	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		auth_info("%s: Retry count(%d)\n", __func__, i);
		flag = ds28e30_read_ROMNO_MANID_HardwareVersion();
		if (flag)
			break;
	}

	if (!flag) {
		auth_err("%s: CRC Error\n", __func__);
		return CRC_Error;
	}

	//0xDB: Samsung Path, 0x5B: Generic Path
	if ((ROM_NO[Active_Device - 1][0] == 0xDB) || (ROM_NO[Active_Device - 1][0] == 0x5B))
		ConfigureDS28E30Parameters();

	return 0;
}

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
static int check_device_authentication_test(void)
{
	int is_auth = 0;
	int i = 0;
	int cpu = get_cpu();

	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		auth_info("%s: retry count = %d, cpu(%d)\n", __func__, i, cpu);
		is_auth = Authenticate_DS28E30(PG_USER_EEPROM_0);
		if (is_auth > 0) {
			cpu_pass_count[cpu]++;
			break;
		}
	}
	cpu_test_count[cpu]++;
	auth_info("%s: Device Authentication(%d), cpu(%d), Active_Device(%d)\n", __func__, is_auth, cpu, Active_Device);
	put_cpu();

	return is_auth;
}
#endif

static int check_device_authentication(void)
{
	int is_auth = 0; 
	int i = 0;

	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		auth_info("%s: retry count = %d\n", __func__, i);
		is_auth = Authenticate_DS28E30(PG_USER_EEPROM_0);
		if (is_auth > 0) {
			if (Active_Device == 1)
				is_authentic = is_auth;
			else if (Active_Device == 2)
				is_authentic2 = is_auth;
			break;
		}			
	}
	auth_info("%s: Device Authentication(%d), Active_Device(%d)\n", __func__, is_auth, Active_Device);
	return is_auth;
}

static int get_qr_code(char *data)
{
	int ret = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	ret = read_from_page_buffer(0, 0, 32, data);
	if (ret)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return ret;
}

static int get_qr_code2(char *data)
{
	int ret = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	ret = read_from_page_buffer2(0, 0, 32, data);
	if (ret)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return ret;
}

#if 0
static void get_current_date(char *tbuf, int len)
{
	struct SEC_TIMESPEC tv;
	struct rtc_time tm;
	unsigned long local_time;

	/* Format the Log time R#: [hr:min:sec.microsec] */
	SEC_GETTIMEOFDAY(&tv);
	/* Convert rtc to local time */
	local_time = (u32)(tv.tv_sec - (sys_tz.tz_minuteswest * 60));
	SEC_RTC_TIME_TO_TM(local_time, &tm);

	scnprintf(tbuf, len,
		  "%d%02d%02d",
		  tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}
#endif


static int get_decrement_counter(void)
{
	unsigned char temp_buf[33] = {0, };
	int num = 0, err = 0;
	unsigned char temp;

	err = read_from_page_memory(DECREMENT_COUNTER_PAGE, temp_buf);
	if (err) {
		auth_err("%s: read decrement counter failed\n", __func__);
		return err;
	}

	// Swap first and 3rd byte to convert to integer
	temp = temp_buf[0];
	temp_buf[0] = temp_buf[2];
	temp_buf[2] = temp;
	
	num = byte_array_to_int(temp_buf, DECREMENT_COUNTER_BYTE_CNT);

	return num;
}

static int get_decrement_counter_cycle(void)
{
	int diff = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	diff = decrement_count_max - decrement_count_val;
	auth_info("%s: decrement counter cycle(%d), max decrement_counter(%d)\n", __func__, diff, decrement_count_max);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return diff;
}

static int get_decrement_counter_cycle2(void)
{
	int diff = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	diff = decrement_count_max2 - decrement_count_val2;
	auth_info("%s: decrement counter cycle(%d), max decrement_counter(%d)\n", __func__, diff, decrement_count_max2);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return diff;
}

static int decrease_counter(void)
{
	unsigned char flag = false;
	int i = 0, num = 0;

	if (!is_authentic) {
		auth_info("%s: authentication fail\n", __func__);
		goto err_dec_count;
	}

	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		flag = ds28e30_cmd_decrementCounter();
		if (!flag) {	//check if counter is decreased then exit loop
			num = get_decrement_counter(); //read decrement counter value
			if (num < 0) {	//error reading decrement counter
				auth_err("%s: get decrement counter fail\n", __func__);
				goto err_dec_count;
			}
			
			if (decrement_count_val == (num + 1)) {	//counter value decreased by 1
				decrement_count_val = num;
				break;
			}
		} else {	//counter decreased by 1 for sure
			decrement_count_val -= 1;
			break;
		}
	}
	return 0;

err_dec_count:
	return -EIO;
}

static int decrease_counter2(void)
{
	unsigned char flag = false;
	int i = 0, num = 0;

	if (!is_authentic2) {
		auth_info("%s: authentication fail\n", __func__);
		goto err_dec_count2;
	}

	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		flag = ds28e30_cmd_decrementCounter();
		if (!flag) {  //check if counter is decreased then exit loop
			num = get_decrement_counter(); //read decrement counter value
			if (num < 0) {	//error reading decrement counter
				auth_err("%s: get decrement counter fail\n", __func__);
				goto err_dec_count2;
			}

			if (decrement_count_val2 == (num+1)) { //counter value decreased by 1
				decrement_count_val2 = num;
				break;
			}
		} else {  //counter decreased by 1 for sure
			decrement_count_val2 -= 1;
			break;
		}
	}
	return 0;

err_dec_count2:
	return -EIO;
}

static int set_first_use_date_page_lock(void)
{
	int err = 0, i = 0, flag = 0;
	char data = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	add_freq_lock();

	if (!is_authentic) {
		auth_info("%s: authentication fail\n", __func__);
		goto err_set_write_lock;
	}

	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		Active_Device = 1;
		err = set_mem_page_write_lock(FIRST_USE_DATE_PAGE);
		if (err < 0) { // Check by reading lock status
			flag = get_mem_page_lock_status(FIRST_USE_DATE_PAGE, &data);
			if (flag) {	// error reading lock status
				auth_err("%s: get page lock status fail\n", __func__);
				goto err_set_write_lock;
			}

			if(data == PROT_WP) // write Lock set success\n
				break;
		} else //success lock for sure
			break;	
	}
	pageLockStatus[FIRST_USE_DATE] = true;

	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	return 0;

err_set_write_lock:
	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	return -EIO;
}

static int set_first_use_date_page_lock2(void)
{
	int err = 0, i = 0, flag = 0;
	char data = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	add_freq_lock();

	if (!is_authentic2) {
		auth_info("%s: authentication fail\n", __func__);
		goto err_set_write_lock;
	}

	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		Active_Device = 2;
		err = set_mem_page_write_lock(FIRST_USE_DATE_PAGE);
		if (err < 0) { // Check by reading lock status
			flag = get_mem_page_lock_status(FIRST_USE_DATE_PAGE, &data);
			if (flag) {	// error reading lock status
				auth_err("%s: get page lock status fail\n", __func__);
				goto err_set_write_lock;
			}

			if (data == PROT_WP) // write Lock set success\n
				break;
		} else //sucess lock for sure
			break;
	}
	pageLockStatus2[FIRST_USE_DATE] = true;

	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	return 0;

err_set_write_lock:
	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	return -EIO;
}

static int set_first_use_date(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	add_freq_lock();

	err = write_to_page_buffer(FIRST_USE_DATE_PAGE, FIRST_USE_DATE_START_BYTE, FIRST_USE_DATE_BYTE_CNT, data);
	if (!err) {
		Active_Device = 1;
		err = write_to_page_memory(FIRST_USE_DATE_PAGE, page_data_buf[FIRST_USE_DATE_PAGE]);
		if (!err)
			pageBufDirty[FIRST_USE_DATE_PAGE] = false;
	}

	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_first_use_date2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	add_freq_lock();

	err = write_to_page_buffer2(FIRST_USE_DATE_PAGE, FIRST_USE_DATE_START_BYTE, FIRST_USE_DATE_BYTE_CNT, data);
	if (!err) {
		Active_Device = 2;
		err = write_to_page_memory(FIRST_USE_DATE_PAGE, page_data_buf2[FIRST_USE_DATE_PAGE]);
		if (!err)
			pageBufDirty2[FIRST_USE_DATE_PAGE] = false;
	}

	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_first_use_date(char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer(FIRST_USE_DATE_PAGE, FIRST_USE_DATE_START_BYTE,
								FIRST_USE_DATE_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_first_use_date2(char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer2(FIRST_USE_DATE_PAGE, FIRST_USE_DATE_START_BYTE,
								FIRST_USE_DATE_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_batt_discharge_level(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer(BATT_DISCHARGE_LEVEL_PAGE, BATT_DISCHARGE_LEVEL_START_BYTE,
								BATT_DISCHARGE_LEVEL_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_batt_discharge_level2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer2(BATT_DISCHARGE_LEVEL_PAGE, BATT_DISCHARGE_LEVEL_START_BYTE,
								BATT_DISCHARGE_LEVEL_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_batt_discharge_level(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = write_to_page_buffer(BATT_DISCHARGE_LEVEL_PAGE, BATT_DISCHARGE_LEVEL_START_BYTE,
								BATT_DISCHARGE_LEVEL_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_batt_discharge_level2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = write_to_page_buffer2(BATT_DISCHARGE_LEVEL_PAGE, BATT_DISCHARGE_LEVEL_START_BYTE,
								BATT_DISCHARGE_LEVEL_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_batt_full_status_usage(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer(BATT_FULL_STATUS_USAGE_PAGE, BATT_FULL_STATUS_USAGE_BYTE,
								BATT_FULL_STATUS_USAGE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_batt_full_status_usage2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer2(BATT_FULL_STATUS_USAGE_PAGE, BATT_FULL_STATUS_USAGE_BYTE,
									BATT_FULL_STATUS_USAGE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_batt_full_status_usage(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = write_to_page_buffer(BATT_FULL_STATUS_USAGE_PAGE, BATT_FULL_STATUS_USAGE_BYTE,
								BATT_FULL_STATUS_USAGE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_batt_full_status_usage2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = write_to_page_buffer2(BATT_FULL_STATUS_USAGE_PAGE, BATT_FULL_STATUS_USAGE_BYTE,
								BATT_FULL_STATUS_USAGE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_bsoh(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer(BSOH_PAGE, BSOH_START_BYTE, BSOH_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_bsoh2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer2(BSOH_PAGE, BSOH_START_BYTE, BSOH_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_bsoh(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = write_to_page_buffer(BSOH_PAGE, BSOH_START_BYTE, BSOH_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_bsoh2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = write_to_page_buffer2(BSOH_PAGE, BSOH_START_BYTE, BSOH_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_bsoh_raw(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer(BSOH_RAW_PAGE, BSOH_RAW_START_BYTE, BSOH_RAW_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_bsoh_raw2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer2(BSOH_RAW_PAGE, BSOH_RAW_START_BYTE, BSOH_RAW_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_bsoh_raw(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = write_to_page_buffer(BSOH_RAW_PAGE, BSOH_RAW_START_BYTE, BSOH_RAW_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_bsoh_raw2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = write_to_page_buffer2(BSOH_RAW_PAGE, BSOH_RAW_START_BYTE, BSOH_RAW_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_asoc(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer(ASOC_PAGE, ASOC_START_BYTE, ASOC_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_asoc2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer2(ASOC_PAGE, ASOC_START_BYTE, ASOC_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_asoc(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = write_to_page_buffer(ASOC_PAGE, ASOC_START_BYTE, ASOC_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_asoc2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = write_to_page_buffer2(ASOC_PAGE, ASOC_START_BYTE, ASOC_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_fai_expired(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer(FAI_EXPIRED_PAGE, FAI_EXPIRED_START_BYTE,
								FAI_EXPIRED_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int get_fai_expired2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_buffer2(FAI_EXPIRED_PAGE, FAI_EXPIRED_START_BYTE,
								FAI_EXPIRED_BYTE_CNT, data);
	if (err)
		auth_err("%s: fail\n", __func__);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_fai_expired(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	add_freq_lock();

	err = write_to_page_buffer(FAI_EXPIRED_PAGE, FAI_EXPIRED_START_BYTE, FAI_EXPIRED_BYTE_CNT, data);
	if (!err) {
		Active_Device = 1;
		err = write_to_page_memory(FAI_EXPIRED_PAGE, page_data_buf[FAI_EXPIRED_PAGE]);
		if (!err)
			pageBufDirty[FAI_EXPIRED_PAGE] = false;
	}

	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

static int set_fai_expired2(unsigned char *data)
{
	int err = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	add_freq_lock();

	err = write_to_page_buffer2(FAI_EXPIRED_PAGE, FAI_EXPIRED_START_BYTE, FAI_EXPIRED_BYTE_CNT, data);
	if (!err) {
		Active_Device = 2;
		err = write_to_page_memory(FAI_EXPIRED_PAGE, page_data_buf2[FAI_EXPIRED_PAGE]);
		if (!err)
			pageBufDirty2[FAI_EXPIRED_PAGE] = false;
	}

	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	return err;
}

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
static int get_gpio_state(void)
{
	int value = 0;
	value = GPIO_read();
	
	return value;
}

static void set_gpio_state(int val)
{
	pr_info("%s: enter, val=%d\n", __func__, val);

	if (val == 0 || val == 1)
		GPIO_write(val);
	else if (val == 2)
		GPIO_toggle();

	pr_info("%s: done\n", __func__);
}

static void set_gpio_direction(int val)
{
	GPIO_set_dir(val);
}

static int get_ow_read(void)
{
	int i;
	unsigned char flag=false;
	
	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		flag = OWReadROM();
		if(flag)
			break;
	}

	return flag;
}

static void get_system_pub_x(unsigned char *data)
{
	int i;
	int size = SYSFS_PAGE_SIZE;

	for(i = 0; i < 32; i++)
	{
		size = SYSFS_PAGE_SIZE - strlen(data);
		snprintf(data+ strlen(data), size, "%x ",SystemPublicKeyX[i]);
	}
}

static void get_system_pub_y(unsigned char *data)
{
	int i;
	int size = SYSFS_PAGE_SIZE;

	for(i = 0; i < 32; i++)
	{
		size = SYSFS_PAGE_SIZE - strlen(data);
		snprintf(data+ strlen(data), size, "%x ",SystemPublicKeyY[i]);
	}
}

static int get_page_data_buf(unsigned char *data)
{
	int i, err = 0;
	int size = SYSFS_PAGE_SIZE;
	unsigned char temp_buf[33] = {0, };
	
	if (debug_page > DATA_BUF_SIZE - 1)
		return -EINVAL;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	if (Active_Device == 1)
		err = read_from_page_buffer(debug_page, 0, 32, temp_buf);
	else if (Active_Device == 2)
		err = read_from_page_buffer2(debug_page, 0, 32, temp_buf);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	if(err)
			return err;

	for(i = 0; i < 32; i++)
	{
		size = SYSFS_PAGE_SIZE - strlen(data);
		snprintf(data+ strlen(data), size, "%x ",temp_buf[i]);
	}
	return 0;
}

static int get_page_data_mem(unsigned char *data)
{
	int i, err;
	int size = SYSFS_PAGE_SIZE;
	unsigned char temp_buf[33] = {0, };

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	err = read_from_page_memory(debug_page, temp_buf);
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	if(err)
		return err;

	for(i = 0; i < 32; i++)
	{
		size = SYSFS_PAGE_SIZE - strlen(data);
		snprintf(data+ strlen(data), size, "%x ",temp_buf[i]);
	}
	return 0;
}

static void get_certificate_const(unsigned char *data)
{
	int i;
	int size = SYSFS_PAGE_SIZE;

	for(i = 0; i < 16; i++)
	{
		size = SYSFS_PAGE_SIZE - strlen(data);
		snprintf(data+ strlen(data), size, "%x ",Certificate_Constant[i]);
	}
}

static void set_config_param(void)
{
	memcpy(Certificate_Constant, GP_Certificate_Constant, 16 );
	memcpy(SystemPublicKeyX, GP_SystemPublicKeyX, 32 );
	memcpy(SystemPublicKeyY, GP_SystemPublicKeyY, 32 );
	memcpy(AuthorityPublicKey_X, GP_AuthorityPublicKey_X, 32 );
	memcpy(AuthorityPublicKey_Y, GP_AuthorityPublicKey_Y, 32 );
	auth_info("%s: Using General keys and constants\n", __func__);
}

static int get_ecdsa_certi_check(void)
{
	unsigned char  page_cert_r[32]={0xA7,0x86,0x2B,0x39,0x1E,0x2E,0x21,0x39,
												0xED,0x76,0x1E,0x86,0x05,0x1E,0x0E,0x3C,
												0x1A,0xD1,0x4B,0xC9,0xDB,0xCB,0x30,0x47,
												0x8B,0x55,0xEF,0xD0,0x91,0x11,0x50,0x5F};
	unsigned char  page_cert_s[32]={0x70,0x4E,0x15,0x1F,0x08,0x3B,0x79,0x41,
												0xD7,0x29,0x27,0xAC,0x54,0x27,0x32,0x45,
												0x9E,0xFD,0xC2,0x6A,0xE5,0x0A,0xBB,0x73,
												0x2D,0x64,0x03,0x9D,0x8E,0xDF,0xEB,0xF9};
	unsigned char  device_pub_key_x[32]={0x04,0x2C,0x86,0xF7,0x11,0xA4,0x4C,0xC9,
												0xC2,0xEE,0x04,0xA8,0x4B,0x7F,0x01,0x49,
												0x55,0x41,0xF9,0xEA,0x41,0xD7,0xEA,0x51,
												0xAD,0xE8,0x80,0x6C,0x39,0x25,0x2B,0x60};
	unsigned char  device_pub_key_y[32]={0x3E,0x3E,0xD8,0x40,0x4A,0x67,0x7E,0xE0,
												0x85,0x04,0x2C,0x5E,0x10,0x47,0x18,0xB9,
												0x5E,0x95,0xA0,0x59,0xAB,0xA7,0xF3,0xD3,
												0xEA,0x94,0xEA,0xCF,0x84,0x34,0xDF,0xA3};
	unsigned char number_rom[8]={0x5B,0x4D,0xA9,0x03,0x00,0x00,0x00,0xFD};
	unsigned char id_man[2]={0x00,0x00};

	int i = 0;
	unsigned char flag = false;

	for (i = 0; i < SEC_AUTH_RETRY; i++) {
		flag = verifyECDSACertificateOfDevice(page_cert_r, page_cert_s, device_pub_key_x,
				device_pub_key_y, number_rom, id_man, SystemPublicKeyX, SystemPublicKeyY);
		if(flag)
			break;
	}

	return flag;
}

#endif

//setting  system-level public key, authority public key and certificate constants
static void ConfigureDS28E30Parameters(void)
{
	unsigned short CID_Value;

	CID_Value = ROM_NO[Active_Device - 1][6] << 4;
	CID_Value += (ROM_NO[Active_Device - 1][5] & 0xF0) >> 4;
	switch (CID_Value) {

	case 0x050:  //Samsung device
		memcpy(Certificate_Constant, Samsung_Certificate_Constant, 16);
		memcpy(SystemPublicKeyX, Samsung_SystemPublicKeyX, 32);
		memcpy(SystemPublicKeyY, Samsung_SystemPublicKeyY, 32);
		memcpy(AuthorityPublicKey_X, Samsung_AuthorityPublicKey_X, 32);
		memcpy(AuthorityPublicKey_Y, Samsung_AuthorityPublicKey_Y, 32);
		auth_info("%s: Using samsung specific keys and constants\n", __func__);
		break;

	default:
		memcpy(Certificate_Constant, GP_Certificate_Constant, 16);
		memcpy(SystemPublicKeyX, GP_SystemPublicKeyX, 32);
		memcpy(SystemPublicKeyY, GP_SystemPublicKeyY, 32);
		memcpy(AuthorityPublicKey_X, GP_AuthorityPublicKey_X, 32);
		memcpy(AuthorityPublicKey_Y, GP_AuthorityPublicKey_Y, 32);
		auth_info("%s: Using General keys and constants\n", __func__);
		break;
	}
}

static void get_decrement_counter_max(void)
{
	unsigned char buf[32] = {0, };

	buf[0] = CounterValue_MSB;
	buf[1] = CounterValue_2LSB;
	buf[2] = CounterValue_LSB;
	
	decrement_count_max = byte_array_to_int(buf, DECREMENT_COUNTER_BYTE_CNT);
	decrement_count_max2 = decrement_count_max;
	auth_info("%s: max decrement_counter (%d)\n", __func__, decrement_count_max);
	
}

static void sync_decrement_counter(unsigned char *buf)
{
	bool is_date_locked;
	int battdischLevel, diff_decr_cycle, loopmax, diff;
	
	auth_info("%s: called\n", __func__);
	
	//add_freq_lock();
	battdischLevel = byte_array_to_int(buf, BATT_DISCHARGE_LEVEL_BYTE_CNT)/100;
	is_date_locked = pageLockStatus[FIRST_USE_DATE_PAGE];
	diff_decr_cycle = decrement_count_max - decrement_count_val;
	diff = battdischLevel - diff_decr_cycle;
	
	//decrease counter only if first use date is locked
	if(is_date_locked && diff > 0)
	{
		if(diff > MAX_DECR_LOOP)
			loopmax = MAX_DECR_LOOP;
		else
			loopmax = diff;

		while(loopmax)
		{
			auth_info("%s: sync started, loopmax(%d), decrement count(%d)\n",
						__func__, loopmax, decrement_count_val);
			decrease_counter();
			loopmax--;
		}	
	}
	auth_info("%s: end\n", __func__);

}

static void sync_decrement_counter2(unsigned char *buf)
{
	bool is_date_locked;
	int battdischLevel, diff_decr_cycle, loopmax, diff;

	auth_info("%s: called\n", __func__);

	//add_freq_lock();
	battdischLevel = byte_array_to_int(buf, BATT_DISCHARGE_LEVEL_BYTE_CNT)/100;
	is_date_locked = pageLockStatus2[FIRST_USE_DATE_PAGE];
	diff_decr_cycle = decrement_count_max2 - decrement_count_val2;
	diff = battdischLevel - diff_decr_cycle;

	//decrease counter only if first use date is locked
	if (is_date_locked && diff > 0) {
		if (diff > MAX_DECR_LOOP)
			loopmax = MAX_DECR_LOOP;
		else
			loopmax = diff;

		while (loopmax) {
			auth_info("%s: sync started, loopmax(%d), decrement count(%d)\n",
						__func__, loopmax, decrement_count_val2);
			decrease_counter2();
			loopmax--;
		}
	}
	auth_info("%s: end\n", __func__);

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

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);

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

	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	convert_heatmap(read_hmap, heatmap, sizeof(heatmap));
	pr_info("%s: decompress heatmap from memory successfully\n", __func__);
	return 0;

err_read_from_buffer:
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	pr_info("%s: reading from buffer failed\n", __func__);
	return err;
}

static int get_heatmap2(int *heatmap, int size)
{
	int err;
	u8 read_hmap[3][32] = { 0 };
	char data[32];

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);

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

	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	convert_heatmap(read_hmap, heatmap, sizeof(heatmap));
	pr_info("%s: decompress heatmap from memory successfully\n", __func__);
	return 0;

err_read_from_buffer:
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	pr_info("%s: reading from buffer failed\n", __func__);
	return err;
}

static int save_heatmap(u8 heatmap[3][32])
{
	int err;
	char data[32];

	memset(data, '\0', sizeof(data));

	memcpy(data, heatmap[0], 32);

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
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

	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	pr_info("%s: heatmap saved to page buffer successfully\n", __func__);
	return 0;

err_save_to_buffer:
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	pr_info("%s: saving to buffer failed\n", __func__);
	return err;
}

static int save_heatmap2(u8 heatmap[3][32])
{
	int err;
	char data[32];

	memset(data, '\0', sizeof(data));

	memcpy(data, heatmap[0], 32);

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
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

	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	pr_info("%s: heatmap saved to page buffer successfully\n", __func__);
	return 0;

err_save_to_buffer:
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
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

static int init_page_data_buffers(void)
{
	int i = 0, err = 0, auth = 0, val = 0;

	auth_info("%s: called\n", __func__);
	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	add_freq_lock();

	auth = check_device_authentication();
	if (auth <= 0) {
		auth_err("%s: authentication fail\n", __func__);
		goto err_initialise_page_buf;
	}

	for (i = 0; i < DATA_BUF_SIZE; i++) {
		if (Active_Device == 1)
			err = read_from_page_memory(i, page_data_buf[i]);
		else if (Active_Device == 2)
			err = read_from_page_memory(i, page_data_buf2[i]);

		if (err) {
			auth_err("%s: Read page(%d) from mem failed\n", __func__, i);
			goto err_initialise_page_buf;
		}
	}

	//get value of decrement_counter
	val = get_decrement_counter();
	if (val < 0)
		goto err_initialise_page_buf;

	if (Active_Device == 1) {
		decrement_count_val = val;
		//to be removed:- only for test devices having init value 131071
		if (decrement_count_val > decrement_count_max)
			decrement_count_max = 131071;
		is_page_buf_init = true;
	} else if (Active_Device == 2) {
		decrement_count_val2 = val;
		//to be removed:- only for test devices having init value 131071
		if (decrement_count_val2 > decrement_count_max2)
			decrement_count_max2 = 131071;
		is_page_buf_init2 = true;
	}

	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

	queue_delayed_work(g_sec_auth_ds28e30->sec_auth_poll_wqueue, &g_sec_auth_ds28e30->sec_auth_poll_work, work_sched_delay * HZ);
	return 0;
	
err_initialise_page_buf:
	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);	
	return -EIO;
}

static int init_decrement_counter(void)
{
	unsigned char buf[32];
	int ret = 0;

	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	add_freq_lock();

	ret = read_from_page_memory(PG_DEC_COUNTER, buf);
	if (ret) {
		auth_info("%s: Decrement counter not initialised yet, start init\n", __func__);
		memset(buf,0x00,32);
		buf[0] = CounterValue_LSB;
		buf[1] = CounterValue_2LSB;
		buf[2] = CounterValue_MSB;

		ret = write_to_page_memory(PG_DEC_COUNTER, buf);
		if (ret) {
			auth_err("%s: write to decrement counter mem failed\n", __func__);
			goto err_dec_init;
		}
	}
	else
		auth_info("%s: Counter already initialised\n", __func__);
	
	if (Active_Device == 1)
		is_counter_init = true;
	else if (Active_Device == 2)
		is_counter_init2 = true;

	get_decrement_counter_max();

	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	return ret;

err_dec_init:
	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
	return ret;	
}


static void init_page_lock_status(void)
{
	unsigned char data = 0;
	int err = 0;
	int i = 0;
	
	auth_info("%s: called\n", __func__);
	mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
	add_freq_lock();
	
	for (i = 0; i < DATA_BUF_SIZE; i++) {
		err = get_mem_page_lock_status(i, &data);
		if (err)
			auth_err("%s: error reading lock status of page(%d)\n", __func__, i);
		else {
			if (Active_Device == 1)
				pageLockStatus[i] = data;
			else if (Active_Device == 2)
				pageLockStatus2[i] = data;
		}
	}

	remove_freq_lock();
	mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
}

#if !defined(CONFIG_SEC_FACTORY)
#define SALE_CODE_STR_LEN		3
static bool is_sales_code(const char *str)
{
	if (str == NULL || sales_code == NULL)
		return 0;

	auth_info("%s: %s\n", __func__, sales_code);
	return !strncmp(sales_code, str, SALE_CODE_STR_LEN + 1);
}

static int check_authic_supported(struct device_node *np)
{
	int ret = 0;
	int eur_det_gpio = 0;
	int sub6_det_gpios[3];
	int gpio_val[3] = {-1, -1, -1};

	/* Method 1 : sub6_det_gpios */
	sub6_det_gpios[0] = of_get_named_gpio(np, "ds28e30,sub6_det_gpio1", 0); // gpio_r1
	sub6_det_gpios[1] = of_get_named_gpio(np, "ds28e30,sub6_det_gpio2", 0); // gpio_r2
	sub6_det_gpios[2] = of_get_named_gpio(np, "ds28e30,sub6_det_gpio3", 0); // gpio_r3

	if (sub6_det_gpios[0] < 0 || sub6_det_gpios[1] < 0 ||
		sub6_det_gpios[2] < 0) {
		auth_err("%s: sub6_det gpios not found!\n", __func__);
	} else {
		gpio_val[0] = gpio_get_value(sub6_det_gpios[0]); // gpio_r1
		gpio_val[1] = gpio_get_value(sub6_det_gpios[1]); // gpio_r2
		gpio_val[2] = gpio_get_value(sub6_det_gpios[2]); // gpio_r3

		/* E1_EUR dev => r3 r2 r1 = H H L */
		if (!(gpio_val[2] && gpio_val[1] && !(gpio_val[0]))) {
			auth_err("%s: not valid auth IC device, gpio_val2(%d), gpio_val1(%d), gpio_val0(%d)\n",
			__func__, gpio_val[2], gpio_val[1], gpio_val[0]);
			return -1;
		}
		goto AUTH_IC_DEVICE;
	}

	/* Method 2 : PMIC gpio
	 * Pull down i.e. value 0 -> EUR
	 * Pull up i.e. value 1 -> Non EUR
	 */
	eur_det_gpio = of_get_named_gpio(np, "ds28e30,eur_detection", 0);
	if (eur_det_gpio < 0) {
		auth_info("%s: 'ds28e30,eur_detection' not found\n", __func__);
	} else {
		ret = gpio_get_value(eur_det_gpio);
		if (ret != 0)
			return -1;
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
	auth_info("%s: Auth IC device, proceed further...\n", __func__);
	return 0;
}
#endif

static void sec_auth_poll_work_func(struct work_struct *work)
{
	u64 elapsed_time;
	u64 current_time = ktime_get_ns();
	union power_supply_propval value = {0, };

	if (current_time < g_sec_auth_ds28e30->sync_time)
		elapsed_time = U64_MAX - (g_sec_auth_ds28e30->sync_time) + current_time;
	else
		elapsed_time = current_time - (g_sec_auth_ds28e30->sync_time);

	auth_info("%s: current(%llu), last_update(%llu), elapsed(%llu)\n",
			__func__, current_time, g_sec_auth_ds28e30->sync_time, elapsed_time);

	psy_do_property("battery", get, POWER_SUPPLY_PROP_STATUS, value);
	if (((value.intval == POWER_SUPPLY_STATUS_CHARGING ||
			value.intval == POWER_SUPPLY_STATUS_FULL) && (elapsed_time > HOUR_NS))
		|| (elapsed_time > DAY_NS)) {
		if (auth_ic_list[0])
			save_buffer_to_memory();
		if (auth_ic_list[1])
			save_buffer_to_memory2();

		g_sec_auth_ds28e30->sync_time = current_time;
	}

	queue_delayed_work(g_sec_auth_ds28e30->sec_auth_poll_wqueue,
						&g_sec_auth_ds28e30->sec_auth_poll_work, work_sched_delay * HZ);
}

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
static void check_passrate_poll_work_func(struct work_struct *work)
{
	int i;
	int auth;

	pass_count = 0;

	for (i = 0; i < 8; i++) {
		cpu_pass_count[i] = 0;
		cpu_test_count[i] = 0;
	}

	for (i = 0; i < test_count; i++) {
		sec_auth_setCpuMask(cpu_x, cpu_y);
		auth = check_device_authentication_test();
		sec_auth_clearCpuMask();
		if (auth == 1)
			pass_count++;
	}

	for (i = 0; i < 8; i++)
		pr_info("%s: cpu(%d) pass_cnt(%d) test_count(%d)\n", __func__, i, cpu_pass_count[i], cpu_test_count[i]);

	pr_info("%s: Auth pass count out of %d(%d) for Active_Device(%d)\n", __func__,
			test_count, pass_count, Active_Device);
}
#endif

static ssize_t battery_svc_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
		char temp_buf[SYSFS_PAGE_SIZE] = {0, };
		int ret = 0;
		int i = 0;

		ret = get_qr_code(temp_buf);
		if (ret) {
			auth_err("%s: fail\n", __func__);
			return ret;
		}

		auth_info("%s: SVC_Battery(%s) show attrs\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
		return i;
}

static ssize_t battery_svc_show2(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
		char temp_buf[SYSFS_PAGE_SIZE] = {0, };
		int ret = 0;
		int i = 0;

		ret = get_qr_code2(temp_buf);
		if (ret) {
			auth_err("%s: fail\n", __func__);
			return ret;
		}

		auth_info("%s: SVC_Battery(%s) show attrs\n", __func__, temp_buf);
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
			auth_err("Failed to create sys/devices/svc\n");
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
			auth_err("Failed to create sys/devices/svc/battery\n");
			goto error_create_svc_battery;
		}
	} else {
		battery = (struct kobject *)svc_sd->priv;
	}

	/* create /sys/devices/svc/battery/SVC_battery */
	ret = sysfs_create_file(battery, &sysfs_SVC_Battery_attr.attr);
	if (ret) {
		auth_err("sysfs create fail-%s\n", sysfs_SVC_Battery_attr.attr.name);
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
			auth_err("Failed to create sys/devices/svc\n");
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
			auth_err("Failed to create sys/devices/svc/battery\n");
			goto error_create_svc_battery;
		}
	} else {
		battery = (struct kobject *)svc_sd->priv;
	}

	/* create /sys/devices/svc/battery/SVC_battery2 */
	ret = sysfs_create_file(battery, &sysfs_SVC_Battery_attr2.attr);
	if (ret) {
		auth_err("sysfs create fail-%s\n", sysfs_SVC_Battery_attr2.attr.name);
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
	SEC_AUTH_ATTR(fai_expired),
	SEC_AUTH_ATTR(chipname),
	SEC_AUTH_ATTR(batt_heatmap),
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	SEC_AUTH_ATTR(gpio_state),
	SEC_AUTH_ATTR(gpio_direction),
	SEC_AUTH_ATTR(check_ow_read),
	SEC_AUTH_ATTR(check_rom_man),
	SEC_AUTH_ATTR(check_syspub_x),
	SEC_AUTH_ATTR(check_syspub_y),
	SEC_AUTH_ATTR(page_data_buf),
	SEC_AUTH_ATTR(page_data_mem),
	SEC_AUTH_ATTR(page_buf_init),
	SEC_AUTH_ATTR(page_dirty_status),
	SEC_AUTH_ATTR(page_lock_status),
	SEC_AUTH_ATTR(check_certi_const),
	SEC_AUTH_ATTR(check_ecdsa_certi),
	SEC_AUTH_ATTR(configure_param),
	SEC_AUTH_ATTR(work_start),
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	SEC_AUTH_ATTR(auth_cpu_freq),
	SEC_AUTH_ATTR(auth_dev_freq),
#endif
	SEC_AUTH_ATTR(read_write_delay),
	SEC_AUTH_ATTR(cpu_start),
	SEC_AUTH_ATTR(cpu_end),
	SEC_AUTH_ATTR(check_passrate),
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
	//struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - sec_auth_attrs;
	union power_supply_propval value = {0, };
	int ret = 0;
	int i = 0;

	switch (offset) {
	case PRESENCE:
	{
		value.intval = is_ic_present;
		auth_info("%s: Presence(%d)\n", __func__, value.intval);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case BATT_AUTH:
	{
		value.intval = is_page_buf_init ? is_authentic : 0;
		auth_info("%s: Authentication (%d), Page Buf Init (%d) , ret (%d)\n",
				__func__, is_authentic, is_page_buf_init, value.intval);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case DECREMENT_COUNTER:
	{
		int num;
		num = get_decrement_counter_cycle();
	
		auth_info("%s: is_counter_init(%d),decrement_count_max(%d),decrement_count_val(%d), num(%d) show attr\n",
				__func__, is_counter_init, decrement_count_max, decrement_count_val, num);

		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			num);	
	}
		break;
	case FIRST_USE_DATE:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0, };
		
		ret = get_first_use_date(temp_buf);
		if(ret)
			return ret;
		
		auth_info("%s: First use date(%s) show attr\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
		
	}
		break;
	case USE_DATE_WLOCK:
	{
		bool is_locked;
		
		mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
		is_locked = pageLockStatus[FIRST_USE_DATE_PAGE];
		mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
		if(is_locked)
			ret = PROT_WP;
		else
			ret = 0;
		
		auth_info("%s: First use date lock status (%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			ret);
		
	}
		break;
	case BATT_DISCHARGE_LEVEL:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};
		
		ret = get_batt_discharge_level(temp_buf);
		if(ret)
			return ret;
		
		ret = byte_array_to_int(temp_buf, BATT_DISCHARGE_LEVEL_BYTE_CNT);
		auth_info("%s: Batt Discharge level(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			ret);
		
	}
		break;
	case BATT_FULL_STATUS_USAGE:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};
		
		ret = get_batt_full_status_usage(temp_buf);
		if(ret)
			return ret;
		
		ret = byte_array_to_int(temp_buf, BATT_FULL_STATUS_USAGE_CNT);
		auth_info("%s: Batt full status usage(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			ret);
		
	}
		break;
	case BSOH:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};
		
		ret = get_bsoh(temp_buf);
		if(ret)
			return ret;
		
		ret = byte_array_to_int(temp_buf, BSOH_BYTE_CNT);
		auth_info("%s: BSOH(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			ret);
		
	}
		break;
	case BSOH_RAW:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};
		
		ret = get_bsoh_raw(temp_buf);
		if(ret)
			return ret;
		
		ret = byte_array_to_int(temp_buf, BSOH_RAW_BYTE_CNT);
		auth_info("%s: BSOH raw(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			ret);
		
	}
		break;
	case QR_CODE:
	{
		char temp_buf[SYSFS_PAGE_SIZE] = {0, };
		ret = get_qr_code(temp_buf);
		if(ret)
			return ret;
		
		auth_info("%s: qr_code(%s) show attrs\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case ASOC:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};
		
		ret = get_asoc(temp_buf);
		if(ret)
			return ret;
		
		ret = byte_array_to_int(temp_buf, ASOC_BYTE_CNT);
		auth_info("%s: Asoc(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
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
	case FAI_EXPIRED:
	{
		char temp_buf[SYSFS_PAGE_SIZE] = {0, };

		ret = get_fai_expired(temp_buf);
		if (ret)
			return ret;

		auth_info("%s: fai_expired(%s)\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case CHIPNAME:
		auth_info("%s: chipname(%s)\n", __func__, CHIP_NAME);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%s\n",
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
	case GPIO_STATE:
	{
		Active_Device = 1;
		value.intval = get_gpio_state();
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case GPIO_DIRECTION:
		break;
	case CHECK_OW_READ:
	{
		Active_Device = 1;
		value.intval = get_ow_read();
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case CHECK_ROM_MAN:
	{
		Active_Device = 1;
		value.intval = check_family_and_configure_paramters();
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case CHECK_SYSPUB_X:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] ={0,};
		Active_Device = 1;
		get_system_pub_x(temp_buf);
		
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case CHECK_SYSPUB_Y:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] ={0,};
		Active_Device = 1;
		get_system_pub_y(temp_buf);
		
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case PAGE_DATA_BUF:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] ={0,};
		Active_Device = 1;
		ret = get_page_data_buf(temp_buf);
		if(ret)
			return ret;
		
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case PAGE_DATA_MEM:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] ={0,};
		Active_Device = 1;
		ret = get_page_data_mem(temp_buf);
		if(ret)
			return ret;
		
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case PAGE_BUF_INIT:
	{
		value.intval = is_page_buf_init;
		
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case PAGE_DIRTY_STATUS:
	{

		mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
		value.intval = pageBufDirty[debug_page];
		mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
		
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case PAGE_LOCK_STATUS:
	{
		mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
		value.intval = pageLockStatus[debug_page];
		mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
		
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case CHECK_CERTI_CONST:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] ={0,};
		get_certificate_const(temp_buf);
		
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case CHECK_ECDSA_CERTI:
	{
		value.intval = get_ecdsa_certi_check();
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case CONFIGURE_PARAM:
		break;
	case WORK_START:
		break;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	case AUTH_CPU_FREQ:
		break;
	case AUTH_DEV_FREQ:
		break;
#endif
	case READ_WRITE_DELAY:
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d %d\n",
			g_sec_auth_ds28e30->pdata->rw_delay_ns[0], g_sec_auth_ds28e30->pdata->rw_delay_ns[1]);
		break;
	case CPU_START:
	{
		value.intval = cpu_x;
		pr_info("%s: cpu_start(%d)\n", __func__, value.intval);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case CPU_END:
	{
		value.intval = cpu_y;
		pr_info("%s: cpu_end(%d)\n", __func__, value.intval);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case CHECK_PASSRATE:
		break;
#endif
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
//	struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - sec_auth_attrs;
	int ret = 0;
	int num = 0;
	int err = 0;

//	union power_supply_propval value = {0, };

	switch (offset) {
	case PRESENCE:
		break;
	case BATT_AUTH:
		break;
	case DECREMENT_COUNTER:
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	{	
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			Active_Device = 1;
			err = decrease_counter();
			if(err)
				return err;
		}
		ret = count;
	}
#endif
		break;
	case FIRST_USE_DATE:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};

		if(sscanf(buf,"%s\n",temp_buf) == 1)
		{
			if(pageLockStatus[FIRST_USE_DATE_PAGE])
				return -EINVAL;

			err = set_first_use_date(temp_buf);
			if(err)
				return err;
		}
		ret = count;
	}
		break;
	case USE_DATE_WLOCK:
	{
		if(sscanf(buf,"%d\n", &num) == 1)
		{
			if(pageLockStatus[FIRST_USE_DATE_PAGE])
				return -EINVAL;

			err = set_first_use_date_page_lock();
			if(err)
				return err;
		}
		ret = count;
	}
		break;
	case BATT_DISCHARGE_LEVEL:
	{
		unsigned char temp_buf[BATT_DISCHARGE_LEVEL_BYTE_CNT + 1] = {0,};

		if(sscanf(buf,"%d\n",&num) == 1)
		{
			integer_to_bytes(num , temp_buf, BATT_DISCHARGE_LEVEL_BYTE_CNT);
			
			err = set_batt_discharge_level(temp_buf);
			if(err)
				return err;
			
		}
		ret = count;
	}
		break;
	case BATT_FULL_STATUS_USAGE:
	{
		unsigned char temp_buf[BATT_FULL_STATUS_USAGE_CNT + 1] = {0,};
		
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			integer_to_bytes(num , temp_buf, BATT_FULL_STATUS_USAGE_CNT);
			
			err = set_batt_full_status_usage(temp_buf);
			if(err)
				return err;
			
		}
		
		ret = count;
	}
		break;
	case BSOH:
	{
		unsigned char temp_buf[BSOH_BYTE_CNT + 1] = {0,};
		
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			integer_to_bytes(num , temp_buf, BSOH_BYTE_CNT);
			
			err = set_bsoh(temp_buf);
			if(err)
				return err;
			
		}
		
		ret = count;
	}
		break;
	case BSOH_RAW:
	{
		unsigned char temp_buf[BSOH_RAW_BYTE_CNT + 1] = {0,};
		
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			integer_to_bytes(num , temp_buf, BSOH_RAW_BYTE_CNT);
			
			err = set_bsoh_raw(temp_buf);
			if(err)
				return err;
			
		}
		
		ret = count;
	}
		break;
	case QR_CODE:
		break;
	case ASOC:
	{
		unsigned char temp_buf[ASOC_BYTE_CNT + 1] = {0,};
		
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			integer_to_bytes(num , temp_buf, ASOC_BYTE_CNT);
			
			err = set_asoc(temp_buf);
			if(err)
				return err;
			
		}
		
		ret = count;
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
	{
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			err = save_buffer_to_memory();
			if(err)
				return err;
		}
		
		ret = count;
	}
		break;
	case FAI_EXPIRED:
	{
		unsigned char temp_buf[FAI_EXPIRED_BYTE_CNT + 1] = {0,};

		if (sscanf(buf, "%s\n", temp_buf) == 1) {
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
	case GPIO_STATE:
	{
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			Active_Device = 1;
			set_gpio_state(num);
		}
		
		ret = count;
	}
		break;
	case GPIO_DIRECTION:
	{
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			Active_Device = 1;
			set_gpio_direction(num);
		}
		
		ret = count;
	}
		break;
	case CHECK_OW_READ:
		break;
	case CHECK_ROM_MAN:
		break;
	case CHECK_SYSPUB_X:
		break;
	case CHECK_SYSPUB_Y:
		break;
	case PAGE_DATA_BUF:
	{
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			debug_page = num;
		}
		
		ret = count;
	}
		break;
	case PAGE_DATA_MEM:
	{
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			debug_page = num;
		}
		
		ret = count;
	}
		break;
	case PAGE_BUF_INIT:
	{
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			Active_Device = 1;
			err = init_page_data_buffers();
			if(err)
				return err;
		}
		
		ret = count;
	}
		break;
	case PAGE_DIRTY_STATUS:
	{
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			debug_page = num;
		}
		
		ret = count;
	}
		break;
	case PAGE_LOCK_STATUS:
	{
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			debug_page = num;
		}
		
		ret = count;
	}
		break;
	case CHECK_CERTI_CONST:
		break;
	case CHECK_ECDSA_CERTI:
		break;
	case CONFIGURE_PARAM:
	{
		if(sscanf(buf,"%d\n",&num) == 1)
			set_config_param();
		
		ret = count;
	}
		break;
	case WORK_START:
	{
		if(sscanf(buf,"%d\n",&num) == 1)
		{
			work_sched_delay = num;
			
			cancel_delayed_work(&g_sec_auth_ds28e30->sec_auth_poll_work);
			
			queue_delayed_work(g_sec_auth_ds28e30->sec_auth_poll_wqueue, &g_sec_auth_ds28e30->sec_auth_poll_work, work_sched_delay * HZ);
		}
		
		ret = count;
	}
		break;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	case AUTH_CPU_FREQ:
	{
		if(sscanf(buf,"%d\n",&num) == 1 && auth_cpufreq_set)
		{
			if(num > 0)
				sec_auth_add_qos_request();
			else
				sec_auth_remove_qos_request();
		}
		
		ret = count;
	}
		break;
	case AUTH_DEV_FREQ:
	{
		if(sscanf(buf,"%d\n",&num) == 1 && auth_cpufreq_set)
		{
			if(num > 0)
				sec_auth_add_devfreq_int_request(664000);
			else
				sec_auth_remove_devfreq_int_request();
		}
		
		ret = count;
	}
		break;
#endif
	case READ_WRITE_DELAY:
	{
		int read_delay = 0;
		int write_delay = 0;

		if (sscanf(buf, "%d %d", &read_delay, &write_delay) == 2) {
			g_sec_auth_ds28e30->pdata->rw_delay_ns[0] = read_delay;
			g_sec_auth_ds28e30->pdata->rw_delay_ns[1] = write_delay;
			auth_info("%s: Updated rw_delay_ns[0] (%d), rw_delay_ns[1] (%d)\n",
				__func__, g_sec_auth_ds28e30->pdata->rw_delay_ns[0],
				g_sec_auth_ds28e30->pdata->rw_delay_ns[1]);

			/* call ow_gpio_init to reflect updated delay times */
			Active_Device = 1;
			ow_gpio_init(g_sec_auth_ds28e30->pdata);
		}
		ret = count;
	}
		break;
	case CPU_START:
	{
		if (sscanf(buf, "%d\n", &num) == 1)
			cpu_x = num;
		ret = count;
	}
		break;
	case CPU_END:
	{
		if (sscanf(buf, "%d\n", &num) == 1)
			cpu_y = num;
		ret = count;
	}
		break;
	case CHECK_PASSRATE:
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			Active_Device = 1;
			test_count = num;

			queue_delayed_work(g_sec_auth_ds28e30->check_passrate_poll_wqueue,
								&g_sec_auth_ds28e30->check_passrate_poll_work, 0);
		}
		ret = count;
	}
		break;
#endif
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
	SEC_AUTH_2ND_ATTR(fai_expired),
	SEC_AUTH_2ND_ATTR(chipname),
	SEC_AUTH_2ND_ATTR(batt_heatmap),
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	SEC_AUTH_2ND_ATTR(gpio_state),
	SEC_AUTH_2ND_ATTR(gpio_direction),
	SEC_AUTH_2ND_ATTR(check_ow_read),
	SEC_AUTH_2ND_ATTR(check_rom_man),
	SEC_AUTH_2ND_ATTR(check_syspub_x),
	SEC_AUTH_2ND_ATTR(check_syspub_y),
	SEC_AUTH_2ND_ATTR(page_data_buf),
	SEC_AUTH_2ND_ATTR(page_data_mem),
	SEC_AUTH_2ND_ATTR(page_buf_init),
	SEC_AUTH_2ND_ATTR(page_dirty_status),
	SEC_AUTH_2ND_ATTR(page_lock_status),
	SEC_AUTH_2ND_ATTR(check_certi_const),
	SEC_AUTH_2ND_ATTR(check_ecdsa_certi),
	SEC_AUTH_2ND_ATTR(configure_param),
	SEC_AUTH_2ND_ATTR(work_start),
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	SEC_AUTH_2ND_ATTR(auth_cpu_freq),
	SEC_AUTH_2ND_ATTR(auth_dev_freq),
#endif
	SEC_AUTH_2ND_ATTR(read_write_delay),
	SEC_AUTH_2ND_ATTR(cpu_start),
	SEC_AUTH_2ND_ATTR(cpu_end),
	SEC_AUTH_2ND_ATTR(check_passrate),
#endif
};

static int sec_auth_2nd_create_attrs(struct device *dev)
{
	int i, rc;

	for (i = 0; i < (int)ARRAY_SIZE(sec_auth_2nd_attrs); i++) {
		rc = device_create_file(dev, &sec_auth_2nd_attrs[i]);
		if (rc)
			goto create_attrs_failed;
	}
	return rc;

create_attrs_failed:
	dev_err(dev, "%s: failed (%d)\n", __func__, rc);
	while (i--)
		device_remove_file(dev, &sec_auth_2nd_attrs[i]);
	return rc;
}

ssize_t sec_auth_2nd_show_attrs(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	//struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - sec_auth_2nd_attrs;
	union power_supply_propval value = {0, };
	int ret = 0;
	int i = 0;

	switch (offset) {
	case PRESENCE:
	{
		value.intval = is_ic_present2;
		auth_info("%s: Presence(%d)\n", __func__, value.intval);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case BATT_AUTH:
	{
		value.intval = is_page_buf_init2 ? is_authentic2 : 0;
		auth_info("%s: Authentication (%d), Page Buf Init (%d) , ret (%d)\n",
				__func__, is_authentic2, is_page_buf_init2, value.intval);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case DECREMENT_COUNTER:
	{
		int num;

		num = get_decrement_counter_cycle2();

		auth_info("%s: is_counter_init(%d),decrement_count_max(%d),decrement_count_val(%d), num(%d) show attr\n",
				__func__, is_counter_init2, decrement_count_max2, decrement_count_val2, num);

		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			num);
	}
		break;
	case FIRST_USE_DATE:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0, };
		ret = get_first_use_date2(temp_buf);
		if (ret)
			return ret;

		auth_info("%s: First use date(%s) show attr\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);

	}
		break;
	case USE_DATE_WLOCK:
	{
		bool is_locked;

		mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
		is_locked = pageLockStatus2[FIRST_USE_DATE_PAGE];
		mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);
		if (is_locked)
			ret = PROT_WP;
		else
			ret = 0;

		auth_info("%s: First use date lock status (%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			ret);

	}
		break;
	case BATT_DISCHARGE_LEVEL:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};

		ret = get_batt_discharge_level2(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, BATT_DISCHARGE_LEVEL_BYTE_CNT);
		auth_info("%s: Batt Discharge level(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			ret);

	}
		break;
	case BATT_FULL_STATUS_USAGE:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};

		ret = get_batt_full_status_usage2(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, BATT_FULL_STATUS_USAGE_CNT);
		auth_info("%s: Batt full status usage(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			ret);

	}
		break;
	case BSOH:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};

		ret = get_bsoh2(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, BSOH_BYTE_CNT);
		auth_info("%s: BSOH(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			ret);

	}
		break;
	case BSOH_RAW:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};

		ret = get_bsoh_raw2(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, BSOH_RAW_BYTE_CNT);
		auth_info("%s: BSOH raw(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			ret);

	}
		break;
	case QR_CODE:
	{
		char temp_buf[SYSFS_PAGE_SIZE] = {0, };
		ret = get_qr_code2(temp_buf);
		if (ret)
			return ret;

		auth_info("%s: qr_code(%s) show attrs\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case ASOC:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};

		ret = get_asoc2(temp_buf);
		if (ret)
			return ret;

		ret = byte_array_to_int(temp_buf, ASOC_BYTE_CNT);
		auth_info("%s: Asoc(%d) show attr\n", __func__, ret);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
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
	case FAI_EXPIRED:
	{
		char temp_buf[SYSFS_PAGE_SIZE] = {0, };

		ret = get_fai_expired2(temp_buf);
		if (ret)
			return ret;

		auth_info("%s: fai_expired(%s)\n", __func__, temp_buf);
		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case CHIPNAME:
		auth_info("%s: chipname(%s)\n", __func__, CHIP_NAME);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%s\n",
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
	case GPIO_STATE:
	{
		Active_Device = 2;
		value.intval = get_gpio_state();
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case GPIO_DIRECTION:
		break;
	case CHECK_OW_READ:
	{
		Active_Device = 2;
		value.intval = get_ow_read();
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case CHECK_ROM_MAN:
	{
		Active_Device = 2;
		value.intval = check_family_and_configure_paramters();
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case CHECK_SYSPUB_X:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};
		Active_Device = 2;
		get_system_pub_x(temp_buf);

		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case CHECK_SYSPUB_Y:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};
		Active_Device = 2;
		get_system_pub_y(temp_buf);

		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case PAGE_DATA_BUF:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};
		Active_Device = 2;
		ret = get_page_data_buf(temp_buf);
		if (ret)
			return ret;

		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case PAGE_DATA_MEM:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};
		Active_Device = 2;
		ret = get_page_data_mem(temp_buf);
		if (ret)
			return ret;

		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case PAGE_BUF_INIT:
	{
		value.intval = is_page_buf_init2;

		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case PAGE_DIRTY_STATUS:
	{

		mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
		value.intval = pageBufDirty2[debug_page];
		mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case PAGE_LOCK_STATUS:
	{
		mutex_lock(&g_sec_auth_ds28e30->sec_auth_mutex);
		value.intval = pageLockStatus2[debug_page];
		mutex_unlock(&g_sec_auth_ds28e30->sec_auth_mutex);

		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case CHECK_CERTI_CONST:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};
		get_certificate_const(temp_buf);

		i += scnprintf(buf + i, sizeof(temp_buf), "%s\n",
			temp_buf);
	}
		break;
	case CHECK_ECDSA_CERTI:
	{
		value.intval = get_ecdsa_certi_check();
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case CONFIGURE_PARAM:
		break;
	case WORK_START:
		break;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	case AUTH_CPU_FREQ:
		break;
	case AUTH_DEV_FREQ:
		break;
#endif
	case READ_WRITE_DELAY:
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d %d\n",
			g_sec_auth_ds28e30->pdata->rw_delay_ns2[0], g_sec_auth_ds28e30->pdata->rw_delay_ns2[1]);
		break;
	case CPU_START:
	{
		value.intval = cpu_x;
		pr_info("%s: cpu_start(%d)\n", __func__, value.intval);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case CPU_END:
	{
		value.intval = cpu_y;
		pr_info("%s: cpu_end(%d)\n", __func__, value.intval);
		i += scnprintf(buf + i, SYSFS_PAGE_SIZE - i, "%d\n",
			value.intval);
	}
		break;
	case CHECK_PASSRATE:
		break;
#endif
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
//	struct power_supply *psy = dev_get_drvdata(dev);
	const ptrdiff_t offset = attr - sec_auth_2nd_attrs;
	int ret = 0;
	int num = 0;
	int err = 0;
//	union power_supply_propval value = {0, };

	switch (offset) {
	case PRESENCE:
		break;
	case BATT_AUTH:
		break;
	case DECREMENT_COUNTER:
#if defined(CONFIG_ENG_BATTERY_CONCEPT)
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			Active_Device = 2;
			err = decrease_counter2();
			if (err)
				return err;
		}
		ret = count;
	}
#endif
		break;
	case FIRST_USE_DATE:
	{
		unsigned char temp_buf[SYSFS_PAGE_SIZE] = {0,};

		if (sscanf(buf, "%s\n", temp_buf) == 1) {
			if (pageLockStatus2[FIRST_USE_DATE_PAGE])
				return -EINVAL;

			err = set_first_use_date2(temp_buf);
			if (err)
				return err;
		}
		ret = count;
	}
		break;
	case USE_DATE_WLOCK:
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			if (pageLockStatus2[FIRST_USE_DATE_PAGE])
				return -EINVAL;

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

		if (sscanf(buf, "%d\n", &num) == 1) {
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

		if (sscanf(buf, "%d\n", &num) == 1) {
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

		if (sscanf(buf, "%d\n", &num) == 1) {
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

		if (sscanf(buf, "%d\n", &num) == 1) {
			integer_to_bytes(num, temp_buf, BSOH_RAW_BYTE_CNT);

			err = set_bsoh_raw2(temp_buf);
			if (err)
				return err;

		}

		ret = count;
	}
		break;
	case QR_CODE:
		break;
	case ASOC:
	{
		unsigned char temp_buf[ASOC_BYTE_CNT + 1] = {0,};

		if (sscanf(buf, "%d\n", &num) == 1) {
			integer_to_bytes(num, temp_buf, ASOC_BYTE_CNT);

			err = set_asoc2(temp_buf);
			if (err)
				return err;

		}

		ret = count;
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
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			err = save_buffer_to_memory2();
			if (err)
				return err;
		}

		ret = count;
	}
		break;
	case FAI_EXPIRED:
	{
		unsigned char temp_buf[FAI_EXPIRED_BYTE_CNT + 1] = {0,};

		if (sscanf(buf, "%s\n", temp_buf) == 1) {
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
	case GPIO_STATE:
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			Active_Device = 2;
			set_gpio_state(num);
		}

		ret = count;
	}
		break;
	case GPIO_DIRECTION:
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			Active_Device = 2;
			set_gpio_direction(num);
		}

		ret = count;
	}
		break;
	case CHECK_OW_READ:
		break;
	case CHECK_ROM_MAN:
		break;
	case CHECK_SYSPUB_X:
		break;
	case CHECK_SYSPUB_Y:
		break;
	case PAGE_DATA_BUF:
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			debug_page = num;
		}

		ret = count;
	}
		break;
	case PAGE_DATA_MEM:
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			debug_page = num;
		}

		ret = count;
	}
		break;
	case PAGE_BUF_INIT:
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			Active_Device = 2;
			err = init_page_data_buffers();
			if (err)
				return err;
		}

		ret = count;
	}
		break;
	case PAGE_DIRTY_STATUS:
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			debug_page = num;
		}

		ret = count;
	}
		break;
	case PAGE_LOCK_STATUS:
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			debug_page = num;
		}

		ret = count;
	}
		break;
	case CHECK_CERTI_CONST:
		break;
	case CHECK_ECDSA_CERTI:
		break;
	case CONFIGURE_PARAM:
	{
		if (sscanf(buf, "%d\n", &num) == 1)
			set_config_param();

		ret = count;
	}
		break;
	case WORK_START:
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			work_sched_delay = num;

			cancel_delayed_work(&g_sec_auth_ds28e30->sec_auth_poll_work);

			queue_delayed_work(g_sec_auth_ds28e30->sec_auth_poll_wqueue, &g_sec_auth_ds28e30->sec_auth_poll_work, work_sched_delay * HZ);
		}

		ret = count;
	}
		break;
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	case AUTH_CPU_FREQ:
	{
		if (sscanf(buf, "%d\n", &num) == 1 && auth_cpufreq_set) {
			if (num > 0)
				sec_auth_add_qos_request();
			else
				sec_auth_remove_qos_request();
		}

		ret = count;
	}
		break;
	case AUTH_DEV_FREQ:
	{
		if (sscanf(buf, "%d\n", &num) == 1 && auth_cpufreq_set) {
			if (num > 0)
				sec_auth_add_devfreq_int_request(664000);
			else
				sec_auth_remove_devfreq_int_request();
		}

		ret = count;
	}
		break;
#endif
	case READ_WRITE_DELAY:
	{
		int read_delay2 = 0;
		int write_delay2 = 0;

		if (sscanf(buf, "%d %d", &read_delay2, &write_delay2) == 2) {
			g_sec_auth_ds28e30->pdata->rw_delay_ns2[0] = read_delay2;
			g_sec_auth_ds28e30->pdata->rw_delay_ns2[1] = write_delay2;
			auth_info("%s: Updated rw_delay_ns2[0] (%d), rw_delay_ns2[1] (%d)\n",
				__func__, g_sec_auth_ds28e30->pdata->rw_delay_ns2[0],
				g_sec_auth_ds28e30->pdata->rw_delay_ns2[1]);

			/* call ow_gpio_init to reflect updated delay times */
			Active_Device = 2;
			ow_gpio_init(g_sec_auth_ds28e30->pdata);
		}
		ret = count;
	}
		break;
	case CPU_START:
	{
		if (sscanf(buf, "%d\n", &num) == 1)
			cpu_x = num;
		ret = count;
	}
		break;
	case CPU_END:
	{
		if (sscanf(buf, "%d\n", &num) == 1)
			cpu_y = num;
		ret = count;
	}
		break;
	case CHECK_PASSRATE:
	{
		if (sscanf(buf, "%d\n", &num) == 1) {
			Active_Device = 2;
			test_count = num;

			queue_delayed_work(g_sec_auth_ds28e30->check_passrate_poll_wqueue,
								&g_sec_auth_ds28e30->check_passrate_poll_work, 0);
		}
		ret = count;
	}
		break;
#endif
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

static int sec_auth_ds28e30_parse_dt(struct device *dev,
	struct sec_auth_ds28e30_platform_data *pdata)
{
	struct device_node *np = of_find_node_by_name(NULL, "sec-auth-ds28e30");
	int ret = 0;
	int len = 0;
	int i;

	if (np == NULL) {
		auth_err("%s: np NULL\n", __func__);
		return -EINVAL;
	} else {
#if !defined(CONFIG_SEC_FACTORY)
		// check if authic supported or not
		ret = check_authic_supported(np);
		if (ret) {
			auth_info("%s: Auth IC not supported, return\n", __func__);
			return -EINVAL;
		}
#endif

		pdata->swi_gpio_cnt = sb_of_gpio_named_count(np,
			"sec_auth_ds28e30,swi_gpio");

		if (pdata->swi_gpio_cnt > 0) {
			auth_info("%s: Has %d swi_gpios\n", __func__, pdata->swi_gpio_cnt);
			if (pdata->swi_gpio_cnt > MAX_SWI_GPIO) {
				auth_err("%s: swi_gpio, catch out-of bounds array read\n",
						__func__);
				pdata->swi_gpio_cnt = MAX_SWI_GPIO;
			}

			for (i = 0; i < pdata->swi_gpio_cnt; i++) {
				pdata->swi_gpio[i] = of_get_named_gpio(np, "sec_auth_ds28e30,swi_gpio", i);
				if (pdata->swi_gpio[i] < 0) {
					auth_err("%s: error reading swi_gpio = %d\n",
						__func__, pdata->swi_gpio[i]);
						pdata->swi_gpio[i] = -1;
				}
			}
		} else {
			auth_err("%s: No swi_gpio, gpio_cnt = %d\n",
					__func__, pdata->swi_gpio_cnt);
			return -1;
		}

		ret = of_property_read_u32(np, "ds28e30,cpu_start", &pdata->cpu_start);
		if (ret < 0) {
			pdata->cpu_start = 0;
			pr_err("%s: error(%d) reading cpu start, setting to default value 0\n", __func__, ret);
		}

		ret = of_property_read_u32(np, "ds28e30,cpu_end", &pdata->cpu_end);
		if (ret < 0) {
			pdata->cpu_end = -1;
			pr_err("%s: error(%d) reading cpu end, setting to default value -1\n", __func__, ret);
		}

		ret = of_property_read_u32_array(np, "ds28e30,base_phys_addr", pdata->base_phys_addr_and_size, 2);
		if (ret < 0) {
			auth_err("%s: Gpio(%d) block base physical address and size not found\n",
						__func__, pdata->swi_gpio[0]);
			return ret;
		}

		len = of_property_count_elems_of_size(np, "ds28e30,offset", sizeof(u32));
		if (len < 0) {
			auth_err("%s: Gpio(%d) error reading size control and data offset not found\n",
						__func__, pdata->swi_gpio[0]);
			return len;
		}
		auth_info("%s: offset length(%d)\n", __func__, len);

		ret = of_property_read_u32_array(np, "ds28e30,offset", pdata->control_and_data_offset, len);
		if (ret < 0) {
			auth_err("%s: Gpio(%d) block control and data offset not found\n",
						__func__, pdata->swi_gpio[0]);
			return ret;
		}

		ret = of_property_read_u32_array(np, "ds28e30,bit_pos", pdata->control_and_data_bit, 2);
		if (ret < 0) {
			auth_err("%s: Gpio(%d) block control and data bit not found\n",
						__func__, pdata->swi_gpio[0]);
			return ret;
		}

		ret = of_property_read_u32_array(np, "ds28e30,rw_delay_ns", pdata->rw_delay_ns, 2);
		if (ret < 0) {
			pdata->rw_delay_ns[0] = 0;
			pdata->rw_delay_ns[1] = 0;
			auth_err("%s: Gpio(%d) read and write ns_delay not found, using 0 default values\n",
						__func__, pdata->swi_gpio[0]);
			ret = 0;
		}

		ret = of_property_read_u32_array(np, "ds28e30,base_phys_addr2", pdata->base_phys_addr_and_size2, 2);
		if (ret < 0) {
			auth_err("%s: Gpio(%d) block base physical address and size 2nd not found\n",
						__func__, pdata->swi_gpio[1]);
			ret = 0;
			//return ret;
		}

		len = of_property_count_elems_of_size(np, "ds28e30,offset2", sizeof(u32));
		if (len < 0) {
			auth_err("%s: Gpio(%d) error reading size control and data offset not found\n",
						__func__, pdata->swi_gpio[1]);
			//return len;
		}
		auth_info("%s: offset2 length(%d)\n", __func__, len);

		ret = of_property_read_u32_array(np, "ds28e30,offset2", pdata->control_and_data_offset2, len);
		if (ret < 0) {
			auth_err("%s: Gpio(%d) block control and data offset 2nd not found\n",
						__func__, pdata->swi_gpio[1]);
			ret = 0;
			//return ret;
		}

		ret = of_property_read_u32_array(np, "ds28e30,bit_pos2", pdata->control_and_data_bit2, 2);
		if (ret < 0) {
			auth_err("%s: Gpio(%d) block control and data bit 2nd not found\n",
						__func__, pdata->swi_gpio[1]);
			ret = 0;
			//return ret;
		}

		ret = of_property_read_u32_array(np, "ds28e30,rw_delay_ns2", pdata->rw_delay_ns2, 2);
		if (ret < 0) {
			pdata->rw_delay_ns2[0] = 0;
			pdata->rw_delay_ns2[1] = 0;
			auth_err("%s: Gpio(%d) read and write ns_delay 2nd not found, using 0 default values\n",
						__func__, pdata->swi_gpio[1]);
			ret = 0;
		}

		
	}

	return ret;
}

static int sec_auth_ds28e30_gpio_init(int SWI_GPIO_PIN)
{
	if (!gpio_is_valid(SWI_GPIO_PIN)){
		auth_err("%s: GPIO %d is not valid\n", __func__, SWI_GPIO_PIN);
		return -EINVAL;
	} else {
		auth_info("%s: SWI GPIO %d is valid\n", __func__, SWI_GPIO_PIN);
		if(gpio_request(SWI_GPIO_PIN, "ds28e30_swi_gpio")){
			auth_err("%s: ERROR: GPIO %d request\n", __func__, SWI_GPIO_PIN);
			return -1;
		}
		if (!gpio_get_value(SWI_GPIO_PIN)) {
			auth_err("%s: ERROR: GPIO %d default LOW\n", __func__, SWI_GPIO_PIN);
			return -1;
		}
	}
	return 0;
}

static void ow_gpio_init(struct sec_auth_ds28e30_platform_data *pdata)
{
	set_1wire_ow_gpio(pdata);
}

static void search_active_devices(struct sec_auth_ds28e30_platform_data *pdata)
{
	int i;
	int ret = 0;

	for (i = 0; i < pdata->swi_gpio_cnt; i++) {
		Active_Device = i + 1;

		ret = sec_auth_ds28e30_gpio_init(pdata->swi_gpio[i]);
		if (ret < 0) {
			auth_err("%s | Error: init platform GPIO fails, ret=0x%X\r\n", __func__, ret);
			continue;
		}

		ow_gpio_init(pdata);
		if (check_device_presence()) {
		//
			auth_ic_list[i] = 1;
		} else {
		// Free GPIO
			gpio_free(pdata->swi_gpio[i]);
			auth_info("%s: free gpio\n", __func__);
		}
	}
}

static int sec_auth_ds28e30_probe(struct platform_device *pdev)
{
	struct sec_auth_ds28e30_data *sec_auth_ds28e30;
	sec_auth_ds28e30_platform_data_t *pdata = NULL;
	struct power_supply_config psy_sec_auth = {};
	struct power_supply_config psy_sec_auth_2nd = {};
	int ret = 0;
	int i = 0;

#if IS_ENABLED(CONFIG_BATTERY_AUTH_SLE956681)
	if (!sle956681_init_complete) {
		auth_err("%s, sle956681_init not complete\n", __func__);
		return -EPROBE_DEFER;
	}
#endif

	sec_auth_ds28e30 = kzalloc(sizeof(*sec_auth_ds28e30), GFP_KERNEL);
	if (!sec_auth_ds28e30)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		pdata = devm_kzalloc(&pdev->dev,
				sizeof(sec_auth_ds28e30_platform_data_t),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&pdev->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_sec_auth_free;
		}

		sec_auth_ds28e30->pdata = pdata;

		if (sec_auth_ds28e30_parse_dt(&pdev->dev, sec_auth_ds28e30->pdata) < 0) {
			dev_err(&pdev->dev,
				"%s: Failed to get sec_auth dt\n", __func__);
			ret = -EINVAL;
			goto err_sec_auth_free;
		}
	} else {
		pdata = dev_get_platdata(&pdev->dev);
		sec_auth_ds28e30->pdata = pdata;
	}

//intialise global pointer	
	g_sec_auth_ds28e30 = sec_auth_ds28e30;

// 1 wire line spinlock init	
	spin_lock_init(&sec_auth_ds28e30->pdata->swi_lock);

// set spinlock variable in 1 wire protocol file
	spinlock_swi_gpio = sec_auth_ds28e30->pdata->swi_lock;

// mutex init
	mutex_init(&sec_auth_ds28e30->sec_auth_mutex);

/* create poll work queue */
	sec_auth_ds28e30->sec_auth_poll_wqueue =
		create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!sec_auth_ds28e30->sec_auth_poll_wqueue) {
		auth_info("%s : Failed to create poll work queue\n", __func__);
		goto err_free;
	}

	INIT_DELAYED_WORK(&sec_auth_ds28e30->sec_auth_poll_work, sec_auth_poll_work_func);

#if defined(CONFIG_ENG_BATTERY_CONCEPT)
/* create check passrate poll work queue */
	sec_auth_ds28e30->check_passrate_poll_wqueue =
		create_singlethread_workqueue(dev_name(&pdev->dev));
	if (!sec_auth_ds28e30->check_passrate_poll_wqueue) {
		pr_info("%s : Failed to create check passrate poll work queue\n", __func__);
		goto err_free;
	}

	INIT_DELAYED_WORK(&sec_auth_ds28e30->check_passrate_poll_work, check_passrate_poll_work_func);
#endif

//set cpufreq and devfreq
#if IS_ENABLED(CONFIG_EXYNOS_PM_QOS) || IS_ENABLED(CONFIG_EXYNOS_PM_QOS_MODULE)
	if(sec_auth_cpu_related_things_init())
		auth_info("%s: cpufreq, devfreq not set(%d)\n", __func__, auth_cpufreq_set);
	else
	{
		auth_cpufreq_set = true;
		auth_info("%s: cpufreq, devfreq set(%d)\n", __func__, auth_cpufreq_set);
	}
#endif

	search_active_devices(sec_auth_ds28e30->pdata);
	if (!auth_ic_list[0] && !auth_ic_list[1]) {
		//No device present
		auth_info("%s : No Authentication device Present\n", __func__);
		goto err_free;
	}

	for (i = 0; i < 2; i++) {

		if (!auth_ic_list[i])
			continue;

		if (i == 1) {
			auth_info("%s: Second Device Found\n", __func__);
			Active_Device = 2;
			// power supply register 2nd
			psy_sec_auth_2nd.drv_data = sec_auth_ds28e30;
			sec_auth_ds28e30->psy_sec_auth_2nd = power_supply_register(&pdev->dev, &sec_auth_2nd_power_supply_desc, &psy_sec_auth_2nd);
			if (!sec_auth_ds28e30->psy_sec_auth_2nd) {
				dev_err(&pdev->dev, "%s: failed to power supply authon register", __func__);
				goto err_free;
			}

			//sysfs creation
			ret = sec_auth_2nd_create_attrs(&sec_auth_ds28e30->psy_sec_auth_2nd->dev);
			if (ret) {
				auth_info("%s : Failed to create sysfs attrs\n", __func__);
				power_supply_unregister(sec_auth_ds28e30->psy_sec_auth_2nd);
				goto err_free;
			}

			// "/sys/devices/svc/battery/SVC_battery" node creation
			ret = sec_auth_create_svc_attrs2(&pdev->dev);
			if (ret) {
				auth_info("%s : Failed to create svc attrs\n", __func__);
				power_supply_unregister(sec_auth_ds28e30->psy_sec_auth_2nd);
				goto err_free;
			}

			//check family and configure parameters
				if (check_family_and_configure_paramters())
					auth_err("%s: Wrong family code\n", __func__);

			//Init decrement counter
				init_decrement_counter();

			//Init page data buffers
				init_page_data_buffers();

			//Init_page_lock_status
				init_page_lock_status();
		} else {
			auth_info("%s: First Device Found\n", __func__);
			Active_Device = 1;
			// power supply register 1st
			psy_sec_auth.drv_data = sec_auth_ds28e30;
			sec_auth_ds28e30->psy_sec_auth = power_supply_register(&pdev->dev, &sec_auth_power_supply_desc, &psy_sec_auth);
			if (!sec_auth_ds28e30->psy_sec_auth) {
				dev_err(&pdev->dev, "%s: failed to power supply authon register", __func__);
				goto err_free;
			}

			//sysfs creation
			ret = sec_auth_create_attrs(&sec_auth_ds28e30->psy_sec_auth->dev);
			if (ret) {
				auth_info("%s : Failed to create sysfs attrs\n", __func__);
				power_supply_unregister(sec_auth_ds28e30->psy_sec_auth);
				goto err_free;
			}

			// "/sys/devices/svc/battery/SVC_battery" node creation
			ret = sec_auth_create_svc_attrs(&pdev->dev);
			if (ret) {
				auth_info("%s : Failed to create svc attrs\n", __func__);
				power_supply_unregister(sec_auth_ds28e30->psy_sec_auth);
				goto err_free;
			}

			//check family and configure parameters
				if (check_family_and_configure_paramters())
					auth_err("%s: Wrong family code\n", __func__);

			//Init decrement counter
				init_decrement_counter();

			//Init page data buffers
				init_page_data_buffers();

			//Init_page_lock_status
				init_page_lock_status();
		}
	}

	auth_info("%s: SEC Auth Ds28e30 driver loaded\n", __func__);
	return 0;

err_free:
	mutex_destroy(&sec_auth_ds28e30->sec_auth_mutex);
err_sec_auth_free:
	kfree(sec_auth_ds28e30);

	return ret;
}

static int sec_auth_ds28e30_remove(struct platform_device *pdev)
{
	struct sec_auth_ds28e30_data *sec_auth_ds28e30;

	sec_auth_ds28e30 = platform_get_drvdata(pdev);
	auth_info("%s: called\n", __func__);

	flush_workqueue(sec_auth_ds28e30->sec_auth_poll_wqueue);
	destroy_workqueue(sec_auth_ds28e30->sec_auth_poll_wqueue);
	devm_kfree(&pdev->dev, sec_auth_ds28e30->pdata);
	kfree(sec_auth_ds28e30);

	return 0;
}

static int sec_auth_ds28e30_suspend(struct device *dev)
{
	return 0;
}

static int sec_auth_ds28e30_resume(struct device *dev)
{
	return 0;
}
static void sec_auth_ds28e30_shutdown(struct platform_device *pdev)
{
	return;
}

#ifdef CONFIG_OF
static struct of_device_id sec_auth_ds28e30_dt_ids[] = {
	{ .compatible = "samsung,sec_auth_ds28e30" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_auth_ds28e30_dt_ids);
#endif /* CONFIG_OF */

static const struct dev_pm_ops sec_auth_ds28e30_pm_ops = {
	.suspend = sec_auth_ds28e30_suspend,
	.resume = sec_auth_ds28e30_resume,
};

static struct platform_driver sec_auth_ds28e30 = {
	.driver = {
		.name = "sec_auth_ds28e30",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &sec_auth_ds28e30_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = sec_auth_ds28e30_dt_ids,
#endif
	},
	.probe		= sec_auth_ds28e30_probe,
	.remove		= sec_auth_ds28e30_remove,
	.shutdown	= sec_auth_ds28e30_shutdown,
};

static int __init sec_auth_ds28e30_init(void)
{
	auth_info("%s:\n", __func__);
	return platform_driver_register(&sec_auth_ds28e30);
}

static void __exit sec_auth_ds28e30_exit(void)
{
	auth_info("%s:\n", __func__);
	platform_driver_unregister(&sec_auth_ds28e30);
}

module_init(sec_auth_ds28e30_init);
module_exit(sec_auth_ds28e30_exit);

MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Sec Auth Ds28e30 driver");
MODULE_LICENSE("GPL");
