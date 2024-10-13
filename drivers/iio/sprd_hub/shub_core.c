/*
 * File:shub_core.c
 *
 * Copyright (C) 2015 Spreadtrum Communications Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 */

#include <linux/gpio/consumer.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/fcntl.h>
#include <linux/firmware.h>
#include <linux/iio/buffer.h>
#include <linux/iio/buffer_impl.h>
#include <linux/iio/iio.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/platform_device.h>
#include <linux/pm_runtime.h>
#include <linux/sched.h>
#include <linux/ktime.h>
#include <linux/timekeeping.h>
#include <uapi/linux/sched/types.h>
#include <linux/soc/sprd/sprd_systimer.h>

#include <linux/reboot.h>
#include <linux/sipc.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/suspend.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>
#include <linux/backlight.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>

#include "shub_common.h"
#include "shub_core.h"
#include "shub_protocol.h"
#include "shub_opcode.h"
#include <linux/pm_wakeup.h>

#define UINT32_MAX     (0xFFFFFFFF)
#define BACKLIGHT_LEVEL_MAX    (2047)

#define MAX_SENSOR_HANDLE 200
//static u8 sensor_status[MAX_SENSOR_HANDLE];

static struct task_struct *thread;
static struct task_struct *thread_nwu;
static struct task_struct *thread_cali;
static DECLARE_WAIT_QUEUE_HEAD(waiter);
static int reader_flag;
struct shub_data *g_sensor;
static struct wakeup_source sensorhub_wake_lock;
static u32 sensorhub_version;
static u8 als_ch_raw_buf[12] = {0};
static struct notifier_block backlight_nb;
static bool backlight_notifier_registered = false;

#if SHUB_DATA_DUMP
#define MAX_RX_LEN 102400
static int total_read_byte_cnt;
static u8 sipc_rx_data[MAX_RX_LEN];
#endif
/* for debug flush event */
static int flush_setcnt;
static int flush_getcnt;

u16 data_from_dynamic[4] = {0, 0, 0, 0};
uint8_t proximity_dynamic_thread_flag = 0;

/* sensor id */
static struct hw_sensor_id_tag hw_sensor_id[_HW_SENSOR_TOTAL] = {
	{0, 0xFF, 0xFF, ""},
	{0, 0xFF, 0xFF, ""},
	{0, 0xFF, 0xFF, ""},
	{0, 0xFF, 0xFF, ""},
	{0, 0xFF, 0xFF, ""},
	{0, 0xFF, 0xFF, ""},
	{0, 0xFF, 0xFF, ""},
};

const char calibration_filename[SENSOR_ID_END][CALIB_PATH_MAX_LENG] = {
	"none",
	"acc",
	"mag",
	"orientation",
	"gyro",
	"light",
	"pressure",
	"tempreature",
	"proximity",
	"colortemp"
};
/*
static struct {
	int sensor_handle;
	u8 sensor_status;
	const char *sensor_type_str;
	char sensor_name[128];
} sensor_info[] = {
	{SENSOR_ACCELEROMETER, _IDSTA_NOT, "acc name:", ""},
	{SENSOR_GEOMAGNETIC_FIELD, _IDSTA_NOT, "mag name:", ""},
	{SENSOR_GYROSCOPE, _IDSTA_NOT, "gyro name:", ""},
	{SENSOR_LIGHT, _IDSTA_NOT, "light name:", ""},
	{SENSOR_PROXIMITY, _IDSTA_NOT, "proximity name:", ""},
	{SENSOR_PRESSURE, _IDSTA_NOT, "prs name:", ""},
	{SENSOR_COLOR_TEMP, _IDSTA_NOT, "color temperature name:", ""},
	{SENSOR_LIGHT_FIRST, _IDSTA_NOT, "dual light 1 name:", ""},
	{SENSOR_LIGHT_SECOND, _IDSTA_NOT, "dual light 2 name:", ""}
};
*/
static int debug_flag;
static int shub_send_event_to_iio(struct shub_data *sensor, u8 *data, u16 len);
static int shub_download_calibration_data(struct shub_data *sensor, int sensor_type);
static int shub_download_all_calibration_data(struct shub_data *sensor);
static void shub_save_calibration_data(struct work_struct *work);
static void shub_synctimestamp(struct shub_data *sensor);

#define MAX_COMPATIBLE_SENSORS 6
static unsigned int sensor_fusion_mode;
static char *acc_firms[MAX_COMPATIBLE_SENSORS];
static char *gryo_firms[MAX_COMPATIBLE_SENSORS];
static char *mag_firms[MAX_COMPATIBLE_SENSORS];
static char *light_firms[MAX_COMPATIBLE_SENSORS];
static char *prox_firms[MAX_COMPATIBLE_SENSORS];
static char *pressure_firms[MAX_COMPATIBLE_SENSORS];
static char *color_temp_firms[MAX_COMPATIBLE_SENSORS];
static char *light_first_firms[MAX_COMPATIBLE_SENSORS];
static char *light_second_firms[MAX_COMPATIBLE_SENSORS];

module_param(sensor_fusion_mode, uint, 0644);
module_param_array(acc_firms, charp, 0, 0644);
module_param_array(gryo_firms, charp, 0, 0644);
module_param_array(mag_firms, charp, 0, 0644);
module_param_array(light_firms, charp, 0, 0644);
module_param_array(prox_firms, charp, 0, 0644);
module_param_array(pressure_firms, charp, 0, 0644);
module_param_array(color_temp_firms, charp, 0, 0644);
module_param_array(light_first_firms, charp, 0, 0644);
module_param_array(light_second_firms, charp, 0, 0644);

struct sensor_cali_info {
	unsigned char size;
	void *data;
};

static struct sensor_cali_info acc_cali_info;
static struct sensor_cali_info gyro_cali_info;
static struct sensor_cali_info mag_cali_info;
static struct sensor_cali_info light_cali_info;
static struct sensor_cali_info prox_cali_info;
static struct sensor_cali_info pressure_cali_info;
static struct sensor_cali_info color_temp_cali_info;
static struct sensor_cali_info light_first_cali_info;
static struct sensor_cali_info light_second_cali_info;

#if 0
static void set_sensor_info(char **sensor_name, int sensor_type, int success_num)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sensor_info); i++) {
		if (sensor_info[i].sensor_handle == sensor_type) {
			memcpy(sensor_info[i].sensor_name,
				sensor_name[success_num],
				strlen(sensor_name[success_num]));
			sensor_info[i].sensor_status = _IDSTA_OK;
			break;
		}
	}
}
#endif
#if 1
static void get_sensor_info(char **sensor_name, int sensor_type, int success_num)
{
	int i, now_order = 0;
	int show_info_order[_HW_SENSOR_TOTAL] = {ORDER_ACC, ORDER_MAG,
						 ORDER_GYRO, ORDER_PROX,
						 ORDER_LIGHT, ORDER_PRS,
						 ORDER_COLOR_TEMP};

	for (i = 0; i < _HW_SENSOR_TOTAL; i++) {
		if (show_info_order[i] == sensor_type) {
			now_order = i;
			break;
		}
	}

	memcpy(hw_sensor_id[now_order].pname, sensor_name[success_num],
					strlen(sensor_name[success_num]));
	hw_sensor_id[now_order].id_status = _IDSTA_OK;
}
#endif
/**
 *send data
 * handler time must less than 5s
 */
static int shub_send_data(struct shub_data *sensor, u8 *buf, u32 len)
{
	int nwrite = 0;
	int sent_len = 0;
	int timeout = 100;
	int retry = 0;

	do {
		nwrite =
			sbuf_write(sensor->sipc_sensorhub_id, SMSG_CH_PIPE,
					SIPC_PM_BUFID0,
					(void *)(buf + sent_len),
					len - sent_len,
					msecs_to_jiffies(timeout));
		if (nwrite > 0)
			sent_len += nwrite;
		if (nwrite < len || nwrite < 0)
			dev_err(&sensor->sensor_pdev->dev,
				"nwrite=%d,len=%d,sent_len=%d,timeout=%dms\n",
				nwrite, len, sent_len, timeout);
		/* only handle boot exception */
		if (nwrite < 0) {
			if (nwrite == -ENODEV) {
				msleep(100);
				retry++;
				if (retry > 10)
					break;
			} else {
				dev_err(&sensor->sensor_pdev->dev,
					"nwrite=%d\n", nwrite);
				dev_err(&sensor->sensor_pdev->dev,
					"task #: %s, pid = %d, tgid = %d\n",
					current->comm, current->pid,
					current->tgid);
				WARN_ONCE(1, "%s timeout: %dms\n",
					  __func__, timeout);
				/* BUG(); */
				break;
			}
		}
	} while (nwrite < 0 || sent_len  < len);
	return nwrite;
}

static int shub_send_command(struct shub_data *sensor, int sensor_ID,
			     enum shub_subtype_id opcode,
			     const char *data, int len)
{
	struct cmd_data cmddata;
	int encode_len;
	int nwrite = 0;
	int ret = 0;

	if (len > (MAX_MSG_BUFF_SIZE - SHUB_MAX_HEAD_LEN - SHUB_MAX_DATA_CRC)) {
		dev_err(&sensor->sensor_pdev->dev, "shub send data %d over size (%d)",
			len, (MAX_MSG_BUFF_SIZE - SHUB_MAX_HEAD_LEN - SHUB_MAX_DATA_CRC));
		return -EBADMSG;
	}

	mutex_lock(&sensor->send_command_mutex);

	if (opcode == SHUB_DOWNLOAD_OPCODE_SUBTYPE) {
		sensor->sent_cmddata.type = sensor_ID;
		sensor->sent_cmddata.sub_type = opcode;
		sensor->sent_cmddata.status = RESPONSE_FAIL;
		sensor->sent_cmddata.condition = false;
	}

	cmddata.type = sensor_ID;
	cmddata.subtype = opcode;
	cmddata.length = len;
	/* no data command  set default data 0xFF */
	if (len == 0) {
		cmddata.buff[0] = 0xFF;
		cmddata.length = 1;
		len = 1;
	} else {
		memcpy(cmddata.buff, data, len);
	}
	encode_len =
	    shub_encode_one_packet(&cmddata, sensor->writebuff,
				   MAX_MSG_BUFF_SIZE);

	if (encode_len > SHUB_MAX_HEAD_LEN + SHUB_MAX_DATA_CRC) {
		nwrite =
		    shub_send_data(sensor, sensor->writebuff, encode_len);
	}
	/* command timeout test */

	if (opcode == SHUB_DOWNLOAD_OPCODE_SUBTYPE) {
		ret =
		wait_event_timeout(sensor->rw_wait_queue,
				   (sensor->sent_cmddata.condition),
				   msecs_to_jiffies(RESPONSE_WAIT_TIMEOUT_MS));
		dev_info(&sensor->sensor_pdev->dev,
			 "send_wait end ret=%d\n", ret);
		/* ret= 0 :timeout */
		if (!ret) {
			sensor->sent_cmddata.status = RESPONSE_TIMEOUT;
			ret = RESPONSE_TIMEOUT;
		} else {
			/* get from callback */
			ret = sensor->sent_cmddata.status;
		}
	} else {
		ret = nwrite;
	}

	mutex_unlock(&sensor->send_command_mutex);

	return ret;
}

static int shub_sipc_channel_read(struct shub_data *sensor)
{
	int nread, retry = 0;

	do {
		nread =
			sbuf_read(sensor->sipc_sensorhub_id, SMSG_CH_PIPE,
				SIPC_PM_BUFID0, (void *)sensor->readbuff,
				SERIAL_READ_BUFFER_MAX - 1, -1);
		if (nread < 0) {
			retry++;
			msleep(500);
			dev_err(&sensor->sensor_pdev->dev,
				"nread=%d,retry=%d\n", nread, retry);
			if (retry > 20)
				break;
		}
	} while (nread < 0);

	if (nread > 0) {
		/* for debug */
#if SHUB_DATA_DUMP
		memcpy(sipc_rx_data + total_read_byte_cnt,
		       sensor->readbuff, nread);
		total_read_byte_cnt += nread;
		if (total_read_byte_cnt >= MAX_RX_LEN) {
			total_read_byte_cnt = 0;
			memset(sipc_rx_data, 0, sizeof(sipc_rx_data));
		}
#endif
		shub_parse_one_packet(&shub_stream_processor,
				      sensor->readbuff, nread);
		memset(sensor->readbuff, 0, sizeof(sensor->readbuff));
	} else {
		dev_info(&sensor->sensor_pdev->dev, "can not get data\n");
	}

	return nread;
}

static int shub_sipc_channel_rdnwu(struct shub_data *sensor)
{
	int nread, retry = 0;

	do {
		nread =
			sbuf_read(sensor->sipc_sensorhub_id, SMSG_CH_PIPE,
				SIPC_PM_BUFID1, (void *)sensor->readbuff_nwu,
				SERIAL_READ_BUFFER_MAX - 1, -1);
		if (nread < 0) {
			retry++;
			msleep(500);
			dev_err(&sensor->sensor_pdev->dev,
				"nread=%d,retry=%d\n", nread, retry);
			if (retry > 20)
				break;
		}
	} while (nread < 0);

	if (nread > 0) {
		shub_parse_one_packet(&shub_stream_processor_nwu,
				      sensor->readbuff_nwu, nread);
		memset(sensor->readbuff_nwu, 0, sizeof(sensor->readbuff));
	} else {
		dev_info(&sensor->sensor_pdev->dev, "can not get data\n");
	}
	return nread;
}

static int shub_read_thread(void *data)
{
	struct sched_param param = {.sched_priority = 5 };
	struct shub_data *sensor = data;

	sched_setscheduler(current, SCHED_RR, &param);

	shub_init_parse_packet(&shub_stream_processor);
	wait_event_interruptible(waiter, (reader_flag != 0));
	set_current_state(TASK_RUNNING);
	do {
		shub_sipc_channel_read(sensor);
	} while (!kthread_should_stop());

	return 0;
}

static int shub_read_thread_nwu(void *data)
{
	struct sched_param param = {.sched_priority = 5 };
	struct shub_data *sensor = data;

	sched_setscheduler(current, SCHED_RR, &param);

	shub_init_parse_packet(&shub_stream_processor_nwu);
	wait_event_interruptible(waiter, (reader_flag != 0));
	set_current_state(TASK_RUNNING);
	do {
		shub_sipc_channel_rdnwu(sensor);
	} while (!kthread_should_stop());

	return 0;
}

/* sync from: hardware/libhardware/include/hardware/sensors-base.h */
#define SENSOR_TYPE_LIGHT 5
#define SENSOR_TYPE_PROX  8
#define SENSOR_TYPE_ACC   1

#if 0
static unsigned int channel_data_light_x_int0;
static unsigned long channel_data_light_time;
#endif

static void channel_data_hook(struct shub_data *sensor, void *data)
{
	/* sync with struct mSenMsgTpA_t of HAL */
	int handle_id = ((short *)data)[1];
	char cmdhandle = ((char *)data)[0];

	if (handle_id == SENSOR_TYPE_LIGHT) {
		if(cmdhandle == 129) { //eHalSenData(129),eHalFlush(130)
			if((sensor->caliRunning) && mutex_trylock(&sensor->mutex_alsraw_lock)) {
				memset(als_ch_raw_buf, 0, sizeof(als_ch_raw_buf));
				memcpy(als_ch_raw_buf, &data[20], 12);
				memset(&data[20], 0, (sizeof(int) * 3));
				pr_err("Alscali:Copy light raw data\n");
				//pr_err("Alscali:[common] Copy data from event [%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d]!\n",
				//			als_ch_raw_buf[0],als_ch_raw_buf[1],als_ch_raw_buf[2],als_ch_raw_buf[3],als_ch_raw_buf[4],
				//			als_ch_raw_buf[5],als_ch_raw_buf[6],als_ch_raw_buf[7],als_ch_raw_buf[8],als_ch_raw_buf[9],
				//			als_ch_raw_buf[10],als_ch_raw_buf[11]);
				mutex_unlock(&sensor->mutex_alsraw_lock);
			}
		}
#if 0
		channel_data_light_x_int0 = ((unsigned int *)data)[2];
		channel_data_light_time = jiffies;
		pr_debug("%s() val = %u, time = %lu\n",
			 __func__, channel_data_light_x_int0,
			 channel_data_light_time);
#endif
	}
}

struct sensor_event sensor_event_acc_data;
struct sensor_event sensor_event_als_data;
struct sensor_event sensor_event_ps_data;

static void shub_data_callback(struct shub_data *sensor, u8 *data, u32 len)
{
	switch (data[0]) {
	case HAL_FLUSH:
#if SHUB_DATA_DUMP
		flush_getcnt++;
#endif
	case HAL_SEN_DATA:
/*		for(i = 0;i < len;i++)
 *			pr_info("len=%d,data[%d]=%d\n",len,i,data[i]);
 */
        if(data[2]==1){//accel
            memcpy(&sensor_event_acc_data,data,sizeof(struct sensor_event));
        } else if(data[2]==5){//light
            memcpy(&sensor_event_als_data,data,sizeof(struct sensor_event));
        }else if(data[2]==58){//proximity
            memcpy(&sensor_event_ps_data,data,sizeof(struct sensor_event));
        }
		channel_data_hook(sensor, data);
		shub_send_event_to_iio(sensor, data, len);
		break;

	default:
		break;
	}
}

static void shub_readcmd_callback(struct shub_data *sensor, u8 *data, u32 len)
{
	switch (data[0]) {
	case KNL_CMD:
		if (sensor->rx_buf && sensor->rx_len ==  (len - 1)) {
			memcpy(sensor->rx_buf, (data + 1), sensor->rx_len);
			sensor->rx_status = true;
			wake_up_interruptible(&sensor->rxwq);
		}
		break;
	default:
		break;
	}
}

static void shub_cm4_read_callback(struct shub_data *sensor,
				   enum shub_subtype_id subtype,
				   u8 *data, u32 len)
{
	switch (subtype) {
	case SHUB_SET_TIMESYNC_SUBTYPE:
		shub_synctimestamp(sensor);
		break;
	default:
		break;
	}
}

static void parse_cmd_response_callback(struct shub_data *sensor,
					u8 *data, u32 len)
{
	struct shub_data *mcu = sensor;

	dev_info(&sensor->sensor_pdev->dev,
		 "mcu->sent_cmddata.condition=%d data[0]=%d  data[1]=%d\n",
		 mcu->sent_cmddata.condition, data[0], data[1]);
	/* already timeout ,do nothing */
	if (mcu->sent_cmddata.status == RESPONSE_TIMEOUT)
		return;

	/*   the response is  real sent command */
	if (mcu->sent_cmddata.sub_type == data[0]) {
		mcu->sent_cmddata.status = data[1];	/* cmd_resp_status_t */
		/* maybe the write thread is not hold on,
		 * the routine have done
		 */
		mcu->sent_cmddata.condition = true;
		wake_up(&mcu->rw_wait_queue);
		dev_info(&sensor->sensor_pdev->dev,
			 "send_wait wake up rw_wait_queue\n");
	}
}

static int get_file_size(struct file *f)
{
	int error;
	struct kstat stat;

	error = vfs_getattr(&f->f_path, &stat, STATX_SIZE,
			    AT_STATX_SYNC_AS_STAT);

	if (error) {
		pr_err("get conf file stat error\n");
		return error;
	}
	return stat.size;
}

static void request_send_firmware(struct shub_data *sensor,
				  char **sensor_firms,
				  int sensor_type,
				  struct sensor_cali_info *cali_info)
{
	struct fwshub_head *fw_head = NULL;
	char *cali_data;
	char *fw_data;
	struct iic_unit *fw_body = NULL;
	char firmware_name[128];
	int i, size;
	int ret;
	int success = 0;
	int opcode_download_count;
	struct file *file;
	loff_t *pos;

	dev_info(&sensor->sensor_pdev->dev,
		 "%s sensor_type = %d start\n", __func__, sensor_type);
	for (i = 0; i < MAX_COMPATIBLE_SENSORS; i++) {
		if (!sensor_firms[i])
			break;

		if (strlen(sensor_firms[i]) == 0)
			break;
		dev_info(&sensor->sensor_pdev->dev,
			 "try compatible sensor: %s\n", sensor_firms[i]);
		sprintf(firmware_name, "/mnt/vendor/sensorhub/shub_fw_%s_cali",
			sensor_firms[i]);
		dev_info(&sensor->sensor_pdev->dev,
			 "fw path: %s\n", firmware_name);

		file = filp_open(firmware_name, O_RDONLY, 0);
		if (IS_ERR(file)) {
			dev_warn(&sensor->sensor_pdev->dev, "open fw failed!\n");
			continue;
		}

		size = get_file_size(file);
		if (size <= 0) {
			dev_warn(&sensor->sensor_pdev->dev,
				 "load file:%s error\n", firmware_name);
			filp_close(file, NULL);
			continue;
		}

		cali_data = kzalloc(size + 1, GFP_KERNEL);
		if (!cali_data) {
			filp_close(file, NULL);
			continue;
		}
		pos = &file->f_pos;
		kernel_read(file, cali_data, size, pos);

		cali_info->data = kmalloc(size, GFP_KERNEL);
		if (!cali_info->data) {
			filp_close(file, NULL);
			kfree(cali_data);
			continue;
		}
		cali_info->size = size;
		memcpy(cali_info->data, cali_data, size);
		kfree(cali_data);
		filp_close(file, NULL);

		sprintf(firmware_name, "/mnt/vendor/sensorhub/shub_fw_%s",
			sensor_firms[i]);
		file = filp_open(firmware_name, O_RDONLY, 0);
		if (IS_ERR(file)) {
			kfree(cali_info->data);
			cali_info->size = 0;
			continue;
		}

		size = get_file_size(file);
		if (size <= 0) {
			dev_warn(&sensor->sensor_pdev->dev,
				 "load file:%s error\n", firmware_name);
			filp_close(file, NULL);
			kfree(cali_info->data);
			cali_info->size = 0;
			continue;
		}

		pos = &file->f_pos;
		fw_data = kzalloc(size + 1, GFP_KERNEL);
		if (!fw_data) {
			dev_warn(&sensor->sensor_pdev->dev,
				 "kmalloc fw_data failed\n");
			filp_close(file, NULL);
			kfree(cali_info->data);
			cali_info->size = 0;
			continue;
		}
		kernel_read(file, (void __user *)fw_data, size, pos);
		filp_close(file, NULL);

		fw_head = (struct fwshub_head *)fw_data;
		fw_body = (struct iic_unit *)(fw_data +
			sizeof(struct fwshub_head));

		opcode_download_count = 0;
		do {
			if (opcode_download_count)
				msleep(200);
			ret = shub_send_command(sensor, sensor_type,
					SHUB_DOWNLOAD_OPCODE_SUBTYPE, fw_data,
					size);
			opcode_download_count++;
		} while (ret == RESPONSE_TIMEOUT &&
			opcode_download_count < 10);

		if (ret) {
			dev_err(&sensor->sensor_pdev->dev,
				"Failed to init sensor, ret = %d\n", ret);
			kfree(fw_data);
			kfree(cali_info->data);
			cali_info->size = 0;
			continue;
		}
		kfree(fw_data);
		//set_sensor_info(sensor_firms, sensor_type, i);
		get_sensor_info(sensor_firms, sensor_type, i);
		dev_info(&sensor->sensor_pdev->dev, "init sensor success\n");
		success = 1;
		break;
	}
	if (!success)
		dev_err(&sensor->sensor_pdev->dev, "%s failed\n", __func__);
}

static int shub_download_opcode(struct shub_data *sensor)
{
	int ret = 0;
	int cali_data_size;
	char *cali_data;
	void *p;

	dev_info(&sensor->sensor_pdev->dev, "loading opcode\n");

	request_send_firmware(sensor, acc_firms,
			      SENSOR_ACCELEROMETER, &acc_cali_info);
	msleep(200);

	request_send_firmware(sensor, gryo_firms,
			      SENSOR_GYROSCOPE, &gyro_cali_info);
	msleep(200);

	request_send_firmware(sensor, mag_firms,
			      SENSOR_GEOMAGNETIC_FIELD, &mag_cali_info);
	msleep(200);

	request_send_firmware(sensor, prox_firms,
			      SENSOR_PROXIMITY, &prox_cali_info);
	msleep(200);

	request_send_firmware(sensor, pressure_firms,
			      SENSOR_PRESSURE, &pressure_cali_info);
	msleep(200);

	request_send_firmware(sensor, color_temp_firms,
			      SENSOR_COLOR_TEMP, &color_temp_cali_info);
	msleep(200);

	request_send_firmware(sensor, light_first_firms,
			      SENSOR_LIGHT_FIRST, &light_first_cali_info);
	msleep(200);

	request_send_firmware(sensor, light_second_firms,
			      SENSOR_LIGHT_SECOND, &light_second_cali_info);
	msleep(200);

	request_send_firmware(sensor, light_firms,
			      SENSOR_LIGHT, &light_cali_info);
	msleep(200);

	cali_data_size = 8 * sizeof(unsigned char);
	cali_data_size += acc_cali_info.size;
	cali_data_size += gyro_cali_info.size;
	cali_data_size += mag_cali_info.size;
	cali_data_size += light_cali_info.size;
	cali_data_size += prox_cali_info.size;
	cali_data_size += pressure_cali_info.size;
	cali_data_size += sizeof(sensor_fusion_mode);
	cali_data_size += color_temp_cali_info.size;
	cali_data = kmalloc(cali_data_size, GFP_KERNEL);
	if (!cali_data) {
		dev_err(&sensor->sensor_pdev->dev,
			"malloc cali_data %d failed\n", cali_data_size);
		ret = -ENOMEM;
		goto release_cali_info;
	}
	cali_data[0] = mag_cali_info.size;
	cali_data[1] = prox_cali_info.size;
	cali_data[2] = light_cali_info.size;
	cali_data[3] = acc_cali_info.size;
	cali_data[4] = gyro_cali_info.size;
	cali_data[5] = pressure_cali_info.size;
	cali_data[6] = sizeof(sensor_fusion_mode);
	cali_data[7] = color_temp_cali_info.size;
	p = &cali_data[8];
	if (mag_cali_info.size != 0) {
		memcpy(p, mag_cali_info.data, mag_cali_info.size);
		p += mag_cali_info.size;
	}
	if (prox_cali_info.size != 0) {
		memcpy(p, prox_cali_info.data, prox_cali_info.size);
		p += prox_cali_info.size;
	}
	if (light_cali_info.size != 0) {
		memcpy(p, light_cali_info.data, light_cali_info.size);
		p += light_cali_info.size;
	}
	if (acc_cali_info.size != 0) {
		memcpy(p, acc_cali_info.data, acc_cali_info.size);
		p += acc_cali_info.size;
	}
	if (gyro_cali_info.size != 0) {
		memcpy(p, gyro_cali_info.data, gyro_cali_info.size);
		p += gyro_cali_info.size;
	}
	if (pressure_cali_info.size != 0) {
		memcpy(p, pressure_cali_info.data, pressure_cali_info.size);
		p += pressure_cali_info.size;
	}
	if (color_temp_cali_info.size != 0) {
		memcpy(p, color_temp_cali_info.data, color_temp_cali_info.size);
		p += color_temp_cali_info.size;
	}
	memcpy(p, &sensor_fusion_mode, sizeof(sensor_fusion_mode));
	p += sizeof(sensor_fusion_mode);
	dev_info(&sensor->sensor_pdev->dev,
		 "mag_cali_info.size = %d\n", mag_cali_info.size);
	dev_info(&sensor->sensor_pdev->dev,
		 "prox_cali_info.size = %d\n", prox_cali_info.size);
	dev_info(&sensor->sensor_pdev->dev,
		 "light_cali_info.size = %d\n", light_cali_info.size);
	dev_info(&sensor->sensor_pdev->dev,
		 "acc_cali_info.size = %d\n", acc_cali_info.size);
	dev_info(&sensor->sensor_pdev->dev,
		 "gyro_cali_info.size = %d\n", gyro_cali_info.size);
	dev_info(&sensor->sensor_pdev->dev,
		 "pressure_cali_info.size = %d\n", pressure_cali_info.size);
	dev_info(&sensor->sensor_pdev->dev,
		 "sensor_fusion_mode = %d\n", sensor_fusion_mode);
	dev_info(&sensor->sensor_pdev->dev,
		 "color_temp_cali_info.size = %d\n", color_temp_cali_info.size);
	dev_info(&sensor->sensor_pdev->dev,
		 "cali_data_size = %d\n", cali_data_size);
	dev_info(&sensor->sensor_pdev->dev,
		 "cali_data = %p, p = %p\n", cali_data, p);
	ret = shub_send_command(sensor,
				SENSOR_TYPE_CALIBRATION_CFG,
				SHUB_DOWNLOAD_CALIBRATION_SUBTYPE,
				cali_data,
				cali_data_size);
	dev_info(&sensor->sensor_pdev->dev,
		 "send cali command ret = %d\n", ret);
	kfree(cali_data);

release_cali_info:
	if (acc_cali_info.size != 0)
		kfree(acc_cali_info.data);
	if (gyro_cali_info.size != 0)
		kfree(gyro_cali_info.data);
	if (mag_cali_info.size != 0)
		kfree(mag_cali_info.data);
	if (light_cali_info.size != 0)
		kfree(light_cali_info.data);
	if (prox_cali_info.size != 0)
		kfree(prox_cali_info.data);
	if (pressure_cali_info.size != 0)
		kfree(pressure_cali_info.data);
	if (color_temp_cali_info.size != 0)
		kfree(color_temp_cali_info.data);

	return ret;
}

static int shub_sipc_read(struct shub_data *sensor,
			  u8 reg_addr, u8 *data, u8 len)
{
	int err = 0;
	int wait_ret;

	if (reader_flag == 0) {
		dev_info(&sensor->sensor_pdev->dev, "run kthread\n");
		reader_flag = 1;
		wake_up_interruptible(&waiter);
	}

	mutex_lock(&sensor->mutex_read);
	sensor->rx_buf = data;
	sensor->rx_len = len;
	sensor->rx_status = false;

	shub_send_command(sensor, HANDLE_MAX,
			  (enum shub_subtype_id)reg_addr,
			  NULL, 0);
	wait_ret =
	    wait_event_interruptible_timeout(sensor->rxwq,
					     (sensor->rx_status),
					     msecs_to_jiffies
					     (RECEIVE_TIMEOUT_MS));
	sensor->rx_buf = NULL;
	sensor->rx_len = 0;
	if (!sensor->rx_status)
		err = -ETIMEDOUT;
	mutex_unlock(&sensor->mutex_read);

	return err;
}

static int shub_send_event_to_iio(struct shub_data *sensor,
				  u8 *data, u16 len)
{
	u8 event[MAX_CM4_MSG_SIZE];
	u8 i = 0;

	mutex_lock(&sensor->mutex_send);
	memset(event, 0x00, MAX_CM4_MSG_SIZE);
	memcpy(event, data, len);

	if (sensor->log_control.udata[5] == 1) {
		for (i = 0; i < len; i++)
			dev_info(&sensor->sensor_pdev->dev,
				 "data[%d]=%d\n", i, data[i]);
	}
	if (sensor->indio_dev->active_scan_mask &&
	    (!bitmap_empty(sensor->indio_dev->active_scan_mask,
			   sensor->indio_dev->masklength))) {
		iio_push_to_buffers(sensor->indio_dev, event);
	} else if (!sensor->indio_dev->active_scan_mask) {
		dev_err(&sensor->sensor_pdev->dev,
			"active_scan_mask = NULL, event might be missing\n");
	}

	mutex_unlock(&sensor->mutex_send);

	return 0;
}

static int shub_download_opcodefile(struct shub_data *sensor)
{
	int ret = 0;

	ret = shub_download_opcode(sensor);

	return ret;
}

static void shub_send_ap_status(struct shub_data *sensor, u8 status)
{
	int ret = 0;

	dev_info(&sensor->sensor_pdev->dev, "status=%d\n", status);
	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_info(&sensor->sensor_pdev->dev, "mcu_mode == CW_BOOT!\n");
		return;
	}
	sensor->mcu_mode = status;
	ret = shub_send_command(sensor, HANDLE_MAX,
				SHUB_SET_HOST_STATUS_SUBTYPE, &status, 1);
}

static void shub_synctimestamp(struct shub_data *sensor)
{
	unsigned long irq_flags;
	struct cnter_to_boottime convert_para;
	ktime_t kt = 0;

	if (sensor->mcu_mode != SHUB_NORMAL)
		return;

	get_convert_para(&convert_para);
	local_irq_save(irq_flags);
	preempt_disable();
	kt = ktime_get_boottime();
	convert_para.last_systimer_counter = sprd_systimer_read();
	convert_para.last_sysfrt_counter = sprd_sysfrt_read();
	local_irq_restore(irq_flags);
	preempt_enable();

	if (unlikely(sensorhub_version == 20181201))
		convert_para.last_boottime = ktime_to_ms(kt);
	else
		convert_para.last_boottime = ktime_to_ns(kt);

	shub_send_command(sensor,
			  HANDLE_MAX,
			  SHUB_SET_TIMESYNC_SUBTYPE,
			  (char *)&convert_para,
			  sizeof(struct cnter_to_boottime));
}

static void shub_synctime_work(struct work_struct *work)
{
	struct shub_data *sensor = container_of((struct delayed_work *)work,
		struct shub_data, time_sync_work);

	shub_synctimestamp(sensor);
	atomic_set(&sensor->delay, SYNC_TIME_DELAY_MS);
	queue_delayed_work(sensor->driver_wq,
			   &sensor->time_sync_work,
			   msecs_to_jiffies(atomic_read(&sensor->delay)));
}

static ssize_t debug_data_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
#if SHUB_DATA_DUMP
	/* save rx raw data file
	 * productinfo/sensor_calibration_data/none
	 * for debug
	 */
	struct shub_data *sensor = dev_get_drvdata(dev);

	dev_info(&sensor->sensor_pdev->dev, "total_read_byte_cnt =%d\n",
		total_read_byte_cnt);
	total_read_byte_cnt = 0;
	memset(sipc_rx_data, 0, sizeof(sipc_rx_data));
	return sprintf(buf, "total_read_byte_cnt=%d\n", total_read_byte_cnt);
#else
	char *prompt = "not dump data\n";

	return sprintf(buf, "%s", prompt);
#endif
}

static ssize_t debug_data_store(struct device *dev,
				struct device_attribute *attr, const char *buf,
				size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);

	shub_send_ap_status(sensor, SHUB_NORMAL);
	return count;
}
static DEVICE_ATTR_RW(debug_data);

static ssize_t reader_enable_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	return sprintf(buf, "debug_flag=%d\n", debug_flag);
}

static ssize_t reader_enable_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	if (sscanf(buf, "%d\n", &debug_flag) != 1)
		return -EINVAL;
	return count;
}
static DEVICE_ATTR_RW(reader_enable);

static void shub_download_calibration_data_work(struct work_struct *work)
{
	struct shub_data *sensor = container_of(work,
		struct shub_data, download_cali_data_work);

	if (sensor->mcu_mode == SHUB_NORMAL) {
		shub_download_all_calibration_data(sensor);
	}
}

static ssize_t op_download_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	u8 data[4];
	u32 version = 0, i;

	for (i = 0; i < 15; i++) {
		if (sensor->mcu_mode == SHUB_BOOT) {
			if (shub_sipc_read(sensor,
				SHUB_GET_FWVERSION_SUBTYPE,
				data, 4) >= 0) {
				sensor->mcu_mode = SHUB_OPDOWNLOAD;
				memcpy(&version, data, sizeof(version));
				sensorhub_version = version;
				dev_info(&sensor->sensor_pdev->dev, "CM4 Version:%u\n",
					version);
				break;
			} else {
				dev_warn(&sensor->sensor_pdev->dev,
					"Get Version Fail retry %d times\n", i + 1);
				msleep(1000);
				continue;
			}
		}
	}
	if (i == 15) {
		dev_err(&sensor->sensor_pdev->dev, "Get Version Fail, cm4 or hub is not up.\n");
		return sprintf(buf, "%u\n", version);
	}

	if (sensor->mcu_mode == SHUB_OPDOWNLOAD) {
		shub_download_opcodefile(sensor);
		sensor->mcu_mode = SHUB_NORMAL;
		schedule_work(&sensor->download_cali_data_work);
		/* start time sync */
		cancel_delayed_work_sync(&sensor->time_sync_work);
		queue_delayed_work(sensor->driver_wq,
				   &sensor->time_sync_work, 0);
	}

	return sprintf(buf, "%u\n", version);
}
static DEVICE_ATTR_RO(op_download);

static ssize_t logctl_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	char *a = "Description:\n";
	char *b = "\techo \"bit val \" > logctl\n\n";
	char *c = "Detail:\n";
	char *d = "\t bit: [0 ~ 7]\n";
	char *d1 = "\t\tbit0: debug message\n";
	char *d2 = "\t\tbit1: sysfs operate message\n";
	char *d3 = "\t\tbit2: function entry message\n";
	char *d4 = "\t\tbit3: data path message\n";
	char *d5 = "\t\tbit4: dump sensor data\n";
	char *e = "\t val: [0 ~ 1], 0 means close, 1 means open\n\n";
	char *f = "\t bit [8] control all flag, all open or close\n";

	return sprintf(buf, "%s%s%s%s%s%s%s%s%s%s%s",
		a, b, c, d, d1, d2, d3, d4, d5, e, f);
}

static ssize_t logctl_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	/* buf:  bit-flag, bit-val
	 *       bit-flag, bit-val
	 */
	int idx = 0;
	int i = 0;
	int total = count / 4;
	u32 bit_flag = 0;
	u32 bit_val = 0;

	struct shub_data *sensor = dev_get_drvdata(dev);

	sensor->log_control.cmd = HAL_LOG_CTL;
	sensor->log_control.length = total;

	dev_info(&sensor->sensor_pdev->dev,
		 "%s total[%d], count[%d]\n", __func__, total, (int)count);
	dev_info(&sensor->sensor_pdev->dev, "char %s\n", buf);

	for (idx = 0; idx < total; idx++)	{
		if (sscanf(buf + 4 * idx, "%u %u\n",
			   &bit_flag, &bit_val) != 2)
			return -EINVAL;
		dev_info(&sensor->sensor_pdev->dev, "%s bit_flag[%d], bit_val[%d]\n",
			 __func__, bit_flag, bit_val);

		if (bit_flag < MAX_SENSOR_LOG_CTL_FLAG_LEN)
			sensor->log_control.udata[bit_flag] = bit_val;

		if (bit_flag == MAX_SENSOR_LOG_CTL_FLAG_LEN) {
			for (i = 0; i < MAX_SENSOR_LOG_CTL_FLAG_LEN; i++)
				sensor->log_control.udata[i] = bit_val;
		}
	}

	shub_send_event_to_iio(sensor, (u8 *)&sensor->log_control,
			       sizeof(sensor->log_control));

	return count;
}
static DEVICE_ATTR_RW(logctl);

static int check_proximity_cali_data(void *cali_data)
{
	struct prox_cali_data prox_cali;
	struct shub_data *sensor = container_of(cali_data, struct shub_data,
		calibrated_data);

	memcpy(&prox_cali, cali_data, sizeof(prox_cali));

	dev_info(&sensor->sensor_pdev->dev,
		"ground_noise = %d; high_thrd = %d; low_thrd = %d; cali_flag = %d\n",
		prox_cali.ground_noise,
		prox_cali.high_threshold,
		prox_cali.low_threshold,
		prox_cali.cali_flag);
/* prox sensor factory auto calibration */
	if ((prox_cali.cali_flag & 0x01) == 0x01 &&
		prox_cali.ground_noise < PROX_SENSOR_MIN_VALUE) {
		dev_err(&sensor->sensor_pdev->dev,
			"prox sensor auto cali out of minrange failed!\n");
		return CALIB_STATUS_OUT_OF_MINRANGE;
	}

/* prox sensor factory manual calibration */
	if ((prox_cali.cali_flag & 0x06) == 0x06 &&
		prox_cali.high_threshold < prox_cali.low_threshold) {
		dev_err(&sensor->sensor_pdev->dev,
			"prox sensor cali failed! the high_thrd < low_thrd!\n");
		return -EINVAL;
	}

	return 0;
}

static int check_acc_cali_data(void *cali_data)
{
	struct acc_gyro_cali_data acc_cali;
	struct shub_data *sensor = container_of(cali_data, struct shub_data,
		calibrated_data);

	memcpy(&acc_cali, cali_data, sizeof(acc_cali));

	dev_info(&sensor->sensor_pdev->dev,
		"acc_cali x_bias = %d; y_bias = %d; z_bias = %d\n",
		acc_cali.x_bias,
		acc_cali.y_bias,
		acc_cali.z_bias);

	if (abs(acc_cali.x_bias) > ACC_MAX_X_Y_BIAS_VALUE ||
		abs(acc_cali.y_bias) > ACC_MAX_X_Y_BIAS_VALUE ||
		abs(acc_cali.z_bias) > ACC_MAX_Z_BIAS_VALUE) {
		dev_err(&sensor->sensor_pdev->dev,
			"acc sensor cali failed! the bias is too big!\n");
		return -EINVAL;
	}

	return 0;
}

static int check_gyro_cali_data(void *cali_data)
{
	struct acc_gyro_cali_data gyro_cali;
	struct shub_data *sensor = container_of(cali_data, struct shub_data,
		calibrated_data);

	memcpy(&gyro_cali, cali_data, sizeof(gyro_cali));

	dev_info(&sensor->sensor_pdev->dev,
		"gyro_cali x_bias = %d; y_bias = %d; z_bias = %d\n",
		gyro_cali.x_bias,
		gyro_cali.y_bias,
		gyro_cali.z_bias);

	if (abs(gyro_cali.x_bias) > GYRO_MAX_X_Y_Z_BIAS_VALUE ||
		abs(gyro_cali.y_bias) > GYRO_MAX_X_Y_Z_BIAS_VALUE ||
		abs(gyro_cali.z_bias) > GYRO_MAX_X_Y_Z_BIAS_VALUE) {
		dev_err(&sensor->sensor_pdev->dev,
			"gyro sensor cali failed! the bias is too big!\n");
		return -EINVAL;
	}

	return 0;
}

/**
 * return 0:success
 * return < 0:fail
 */
static void shub_save_calibration_data(struct work_struct *work)
{
	int err = 0, nwrite = 0;
	struct file *pfile = NULL;
	char file_path[CALIB_PATH_MAX_LENG];
	struct shub_data *sensor = container_of(work,
		struct shub_data, savecalifile_work);

	switch (sensor->cal_id) {
	case SENSOR_ACCELEROMETER:
		err = check_acc_cali_data(sensor->calibrated_data);
		break;
	case SENSOR_GYROSCOPE:
		err = check_gyro_cali_data(sensor->calibrated_data);
		break;
	case SENSOR_PROXIMITY:
		err = check_proximity_cali_data(sensor->calibrated_data);
		break;
	default:
		break;
	}

	if (err < 0) {
		sensor->cal_savests = err;
		shub_download_calibration_data(sensor, sensor->cal_id);
		return;
	}

	if ((strlen(CALIBRATION_NODE) +
	     strlen(calibration_filename[sensor->cal_id])) >
	    (sizeof(file_path) - 1)) {
		dev_err(&sensor->sensor_pdev->dev,
			"calibration node path is oversize.\n");
		return;
	}
	snprintf(file_path, sizeof(file_path), "%s%s",
		 CALIBRATION_NODE, calibration_filename[sensor->cal_id]);
	dev_info(&sensor->sensor_pdev->dev,
		 "sensor_id=%d,file_path=%s\n", sensor->cal_id, file_path);
	pfile = filp_open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0660);
	if (IS_ERR(pfile)) {
		err = PTR_ERR(pfile);
		dev_err(&sensor->sensor_pdev->dev,
			"open file %s error=%d\n", file_path, err);
		sensor->cal_savests = err;
		return;
	}
	nwrite = kernel_write(pfile, sensor->calibrated_data,
			      CALIBRATION_DATA_LENGTH,
			      &pfile->f_pos);
	if (nwrite < 0) {
		dev_err(&sensor->sensor_pdev->dev, "nwrite=%d\n", nwrite);
		err = nwrite;
		sensor->cal_savests = err;
	} else {
		sensor->cal_savests = 0;
	}
	if (pfile)
		filp_close(pfile, NULL);

	shub_download_calibration_data(sensor, sensor->cal_id);
}

static void shub_save_mag_offset(struct shub_data *sensor,
				 u8 *data, u32 len)
{
	int err = 0, nwrite = 0;
	struct file *pfile = NULL;
	char file_path[CALIB_PATH_MAX_LENG];

	if ((strlen(CALIBRATION_NODE) + strlen(calibration_filename[2])) >
	    (sizeof(file_path) - 1)) {
		dev_err(&sensor->sensor_pdev->dev,
			"calibration node path is oversize.\n");
		return;
	}
	snprintf(file_path, sizeof(file_path), "%s%s",
		 CALIBRATION_NODE, calibration_filename[2]);
	dev_info(&sensor->sensor_pdev->dev, "file_path=%s\n", file_path);
	pfile = filp_open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
	if (IS_ERR(pfile)) {
		err = PTR_ERR(pfile);
		dev_err(&sensor->sensor_pdev->dev,
			"open file %s error=%d\n", file_path, err);
		return;
	}
	nwrite = kernel_write(pfile, (const char __user *)data,
			      CALIBRATION_DATA_LENGTH,
			      &pfile->f_pos);

	if (nwrite < 0) {
		dev_err(&sensor->sensor_pdev->dev, "nwrite=%d\n", nwrite);
		err = nwrite;
	}
	if (pfile)
		filp_close(pfile, NULL);
}

static int backlight_level_notify(struct notifier_block *nb,
									unsigned long val, void *bd)
{
	//struct backlight_device *backlight = bd;
	int bl = (int)val;
	int err = 0;

	if((bl >= 0) && (bl <= BACKLIGHT_LEVEL_MAX)) {
		pr_err("Alscali:Backlight level changed->[%d]\n", bl);
		err = shub_send_command(g_sensor, SENSOR_LIGHT, SHUB_SET_MODE,
								((char *)&bl), sizeof(bl));
		if (err < 0) {
			pr_err("Alscali:Send backlight to CM4 failed![%d]\n", err);
		}
	} else {
		pr_err("Alscali:Backlight notify err level[%d]\n", bl);
	}

	return NOTIFY_OK;
}  

static int shub_download_all_calibration_data(struct shub_data *sensor)
{
	int sensor_type = 0, err = 0, bl = 0;

	for (sensor_type = 0; sensor_type < 9; sensor_type++)
		shub_download_calibration_data(sensor, sensor_type);

	bl = get_backlight_brightness();
	if((bl < 0) || (bl > BACKLIGHT_LEVEL_MAX)) {
		bl = 0;
	}
	
	err = shub_send_command(sensor, SENSOR_LIGHT, SHUB_SET_MODE,
							((char *)&bl), sizeof(bl));
	if (err < 0) {
		pr_err("Alscali:Init backlight to CM4 failed![%d]\n", err);
	}
	
	backlight_nb.notifier_call = backlight_level_notify;
	backlight_nb.priority = 0;
	if(backlight_level_notifier_register(&backlight_nb) == 0) {
		backlight_notifier_registered = true;
	}
	
	return err;
}

static int shub_download_calibration_data(struct shub_data *sensor, int sensor_type)
{
	int err = 0;
	struct file *pfile = NULL;
	char file_path[CALIB_PATH_MAX_LENG];
	char raw_cali_data[CALIBRATION_DATA_LENGTH] =  {0};
	int cal_file_size = CALIBRATION_DATA_LENGTH;
	int j = 0;

	if ((strlen(CALIBRATION_NODE) +
	     strlen(calibration_filename[sensor_type])) >
	    (sizeof(file_path) - 1)) {
		dev_err(&sensor->sensor_pdev->dev,
			"calibration node path is oversize.\n");
		return err;
	}
	snprintf(file_path, sizeof(file_path), "%s%s",
		 CALIBRATION_NODE, calibration_filename[sensor_type]);
	dev_info(&sensor->sensor_pdev->dev,
		 "sensor_id=%d file_path=%s\n", sensor_type, file_path);

	pfile = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		err = (int)PTR_ERR(pfile);
		dev_info(&sensor->sensor_pdev->dev,
			 "open file %s ret=%d\n", file_path, err);
		goto send_calibration_data;
	}

	cal_file_size = get_file_size(pfile);
	if (cal_file_size != CALIBRATION_DATA_LENGTH) {
		dev_warn(&sensor->sensor_pdev->dev,
			 "Unable to get file size:%s\n", file_path);
		filp_close(pfile, NULL);
		return err;
	}
	dev_info(&sensor->sensor_pdev->dev,
		 "cal_file_size=%d\n", cal_file_size);

	if (kernel_read(pfile, raw_cali_data,
			cal_file_size, &pfile->f_pos) != cal_file_size) {
		dev_err(&sensor->sensor_pdev->dev, "Error: file read failed\n");
		filp_close(pfile, NULL);
		return err;
	} else {
		filp_close(pfile, NULL);
	}

	if(sensor_type == SENSOR_TYPE_LIGHT) {
		memcpy(&sensor->alsCoeff, (raw_cali_data), sizeof(sensor->alsCoeff));
		//pr_err("Als Cali coeff in file:[%d, %d, %d, %d, %d, %d]\n", sensor->alsCoeff.lux_coeff,
		//                   sensor->alsCoeff.ch0_coeff, sensor->alsCoeff.ch1_coeff,
		//				   sensor->alsCoeff.ch2_coeff, sensor->alsCoeff.ch3_coeff,
		//				   sensor->alsCoeff.ch4_coeff);
	}

	/* for debug */
	for (j = 0; j < cal_file_size; j++)
		dev_info(&sensor->sensor_pdev->dev,
			 "raw_cali_data[%d]=%d\n", j, raw_cali_data[j]);

send_calibration_data:
	err = shub_send_command(sensor, sensor_type,
				SHUB_SET_CALIBRATION_DATA_SUBTYPE,
				raw_cali_data, cal_file_size);

	if (err < cal_file_size)
		dev_err(&sensor->sensor_pdev->dev,
			"%s download calibration data failed\n",
			calibration_filename[sensor_type]);
	else
		dev_err(&sensor->sensor_pdev->dev,
			"%s download calibration data success\n",
			calibration_filename[sensor_type]);

	return err;
}

static ssize_t enable_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int handle, enabled;
	enum shub_subtype_id subtype;

	dev_info(&sensor->sensor_pdev->dev, "buf=%s\n", buf);
	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_info(&sensor->sensor_pdev->dev,
			 "[%s]mcu_mode == %d!\n", __func__, sensor->mcu_mode);
		return -EAGAIN;
	}

	if (sscanf(buf, "%d %d\n", &handle, &enabled) != 2)
		return -EINVAL;
	dev_info(&sensor->sensor_pdev->dev,
		"handle = %d, enabled = %d\n", handle, enabled);
	subtype = (enabled == 0) ? SHUB_SET_DISABLE_SUBTYPE :
		SHUB_SET_ENABLE_SUBTYPE;
	if (shub_send_command(sensor, handle, subtype, NULL, 0) < 0)
		dev_err(&sensor->sensor_pdev->dev, "Write SetEn/Disable fail\n");

	return count;
}
static DEVICE_ATTR_WO(enable);

static ssize_t batch_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int flag = 0;
	struct sensor_batch_cmd batch_cmd;

	dev_info(&sensor->sensor_pdev->dev, "buf=%s\n", buf);
	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_info(&sensor->sensor_pdev->dev,
			"mcu_mode == %d!\n",  sensor->mcu_mode);
		return -EAGAIN;
	}

	if (sscanf(buf, "%d %d %d %lld\n",
		   &batch_cmd.handle, &flag,
		   &batch_cmd.report_rate,
		   &batch_cmd.batch_timeout) != 4)
		return -EINVAL;
	dev_info(&sensor->sensor_pdev->dev,
		 "handle = %d, rate = %d, batch_latency = %lld, enabled = %d\n",
		 batch_cmd.handle,
		 batch_cmd.report_rate, batch_cmd.batch_timeout, flag);

	if (batch_cmd.handle == 58 || batch_cmd.handle == 8){
		batch_cmd.report_rate = 100 * 1000;//us
		dev_info(&sensor->sensor_pdev->dev, "alsps set ps report_rate 10\n");
	}

	if (shub_send_command(sensor, batch_cmd.handle,
			      SHUB_SET_BATCH_SUBTYPE,
			      (char *)&batch_cmd.report_rate, 12) < 0)
		dev_err(&sensor->sensor_pdev->dev, "%s Fail\n", __func__);

	return count;
}
static DEVICE_ATTR_WO(batch);

static ssize_t flush_show(struct device *dev,
			  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "flush_setcnt=%d,flush_getcnt=%d\n",
		flush_setcnt, flush_getcnt);
}

static ssize_t flush_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int handle;

	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT\n");
		return -EAGAIN;
	}
	if (sscanf(buf, "%d\n", &handle) != 1)
		return -EINVAL;
#if SHUB_DATA_DUMP
	flush_setcnt++;
#endif
	if (shub_send_command(sensor, handle,
			      SHUB_SET_FLUSH_SUBTYPE,
			      NULL, 0x00) < 0)
		dev_err(&sensor->sensor_pdev->dev, " Fail\n");

	__pm_wakeup_event(&(sensorhub_wake_lock), jiffies_to_msecs(200));

	return count;
}
static DEVICE_ATTR_RW(flush);

static int set_calib_cmd(struct shub_data *sensor, u8 cmd, u8 id,
			 u8 type, int golden_value)
{
	char data[6];
	int err;

	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT\n");
		return -EINVAL;
	}

	data[0] = cmd;
	data[1] = type;
	memcpy(&data[2], &golden_value, sizeof(golden_value));
	if (cmd == CALIB_EN) {
		err = shub_send_command(sensor, id,
			SHUB_DISABLE_CALI_RANGE_CHECK_SUBTYPE,
			data, sizeof(data));
		dev_info(&sensor->sensor_pdev->dev,
			"disable cali range check, err = %d\n", err);
	}
	err = shub_send_command(sensor, id,
				SHUB_SET_CALIBRATION_CMD_SUBTYPE,
				data, sizeof(data));
	if (!err)
		dev_err(&sensor->sensor_pdev->dev,
			"Write CalibratorCmd Fail\n");

	return err;
}

static ssize_t mcu_mode_show(struct device *dev, struct device_attribute *attr,
			     char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", sensor->mcu_mode);
}

static ssize_t mcu_mode_store(struct device *dev, struct device_attribute *attr,
			      const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int mode = 0;

	if (sscanf(buf, "%d\n", &mode) != 1)
		return -EINVAL;
	sensor->mcu_mode = mode;
	return count;
}
static DEVICE_ATTR_RW(mcu_mode);

static int shub_save_als_cali_data(struct shub_data *sensor,
	u8 *data, u32 len)
{
	int err;
	struct file *pfile;
	char file_path[CALIB_PATH_MAX_LENG];

	if ((strlen(CALIBRATION_NODE) + strlen(calibration_filename[5])) >
	    (sizeof(file_path) - 1)) {
		dev_err(&sensor->sensor_pdev->dev,
			"calibration node path is oversize.\n");
		return -EINVAL;
	}
	snprintf(file_path, sizeof(file_path), "%s%s",
		 CALIBRATION_NODE, calibration_filename[5]);
	pfile = filp_open(file_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (IS_ERR(pfile)) {
		err = PTR_ERR(pfile);
		pr_err("open file %s error=%d\n", file_path, err);
		return err;
	}
	err = kernel_write(pfile, data,
			      CALIBRATION_DATA_LENGTH,
			      &pfile->f_pos);
	if (err < 0)
		pr_err("err=%d\n", err);
	filp_close(pfile, NULL);
	pr_debug("shub_save_als_cali_data end\n");

	return err;
}

#if 0
static int set_als_calib_cmd(struct shub_data *sensor, u8 cmd, u8 id)
{
	int err, i, average_als, als_cali_coef, status;
	u8 data[2];
	u16 *ptr = (u16 *)data;
	char als_data[30];
	int light_sum = 0;

	for (i = 0; i < LIGHT_CALI_DATA_COUNT; i++) {
		err = shub_sipc_read(sensor,
			     SHUB_GET_LIGHT_RAWDATA_SUBTYPE, data, sizeof(data));
		if (err < 0) {
			pr_err("read RegMapR_GetLightRawData failed!\n");
			return err;
		}
		/*sleep for light senor collect data every 100ms*/
		msleep(100);
		pr_debug("shub_sipc_read: ptr[0] = %d\n", ptr[0]);
		light_sum += ptr[0];
	}
	average_als = light_sum / LIGHT_CALI_DATA_COUNT;
	pr_info("light sensor cali light_sum:%d, average_als = %d\n",
		light_sum, average_als);

	if (average_als < LIGHT_SENSOR_MIN_VALUE ||
		average_als > LIGHT_SENSOR_MAX_VALUE) {
		als_cali_coef = CALIB_STATUS_FAIL;
		status = CALIB_STATUS_FAIL;
	} else {
		als_cali_coef = LIGHT_SENSOR_CALI_VALUE / average_als;
		status = CALIB_STATUS_PASS;
	}
	memcpy(als_data, &als_cali_coef, sizeof(als_cali_coef));

	err = shub_save_als_cali_data(sensor, als_data, sizeof(als_data));
	if (err < 0) {
		pr_err("Save Light Sensor CalibratorData Fail\n");
		return err;
	}

	err = shub_send_command(sensor, SENSOR_TYPE_LIGHT,
				SHUB_SET_CALIBRATION_DATA_SUBTYPE,
				als_data, CALIBRATION_DATA_LENGTH);
	if (err < 0) {
		pr_err("Write Light Sensor CalibratorData Fail\n");
		return err;
	}
	pr_debug("Light Sensor Calibrator status = %d\n", status);

	return status;
}
#endif
#define STK_LIGHT_DRV_NAME	"light_als02"
#define LTR_LIGHT_DRV_NAME	"light_als01"
/*
static int calcAgain(int gain)
{
	u16 maxGain = 0;

	if(hw_sensor_id[ORDER_LIGHT - 1].id_status == _IDSTA_OK) {
		if(!strncmp(hw_sensor_id[ORDER_LIGHT - 1].pname, LTR_LIGHT_DRV_NAME,20)) {
			pr_err("Alscali:Use vendor_01 max gain 512x\n");
			maxGain = 512;
		} else if(!strncmp(hw_sensor_id[ORDER_LIGHT - 1].pname, STK_LIGHT_DRV_NAME,20)) {
			pr_err("Alscali:Use vendor_02 max gain 128x\n");
			maxGain = 128;
		} else {
			pr_err("Alscali:Use default[1] max gain 128x\n");
			maxGain = 128;
		}
	} else {
		pr_err("Alscali:Use default[2] max gain 128x\n");
		maxGain = 128;
	}

	return maxGain/gain;
}
*/

static int shub_als_cali_thread(void *drvdata)
{
	static u16 again;
	//static unsigned int caliDarkAvg[6];
	//static unsigned int caliLeakAvg[6];

    int als_of_0level = 0, als_of_102level = 0;
	//u16 tempavg = 0, t_gain = 0;
	int err = 0;
	//int i = 0, j = 0;
	int i = 0;
	int average_als = 0;
	int errCount = 0;
	char als_data[30] = {0};
	u16 *ptr = (u16 *)als_data;
	int tempCali[6] = {0};
	unsigned int sum[5] = {0};
	int caliRate = 80;
    u8 backlight_level = 0;

	struct shub_data *sensor = (struct shub_data *)drvdata;

	sensor->caliRunning = true;
	pr_err("Alscali: Cali Enter\n");
	sensor->als_cali_result = CALI_ERR_NONE;

	memcpy(tempCali, &(sensor->alsCoeff), sizeof(struct als_cali_data));
	pr_err("Alscali:Cali Thread,temp cali data[%d %d %d %d %d %d]\n", tempCali[0], tempCali[1], tempCali[2],
	                                                          tempCali[3], tempCali[4], tempCali[5]);
	
	if((sensor->als_cali_cmd == CALIB_ALS_LEAK) || (sensor->als_cali_cmd == CALIB_ALS_DARK)) {
		i = UINT32_MAX;
		err = shub_send_command(sensor, SENSOR_LIGHT, SHUB_SET_MODE, ((char *)&i), sizeof(i));
		if (err < 0) {
			pr_err("Alscali:Set cali mode into cm4 failed!\n");
			sensor->als_cali_result = CALI_ERR_CM4CMDE;
			sensor->caliRunning = false;
			return 0;
		}

		err = shub_send_command(sensor, SENSOR_LIGHT, SHUB_SET_BATCH_SUBTYPE, (char *)&caliRate, sizeof(caliRate));
		if (err < 0) {
			pr_err("Alscali:Set report rate into cm4 failed\n", __func__);
			sensor->als_cali_result = CALI_ERR_CM4CMDE;
			sensor->caliRunning = false;
			return 0;
		}
	}

	msleep(300);
	
	i = 0;
	do{
		if(errCount >= 3) {
			pr_err("Alscali: Retry 3 times!! exit cali thread\n");
			again = 0;
			break;
		}

		err = 0;
		sensor->als_cali_result = CALI_ERR_NONE;
		backlight_level = get_backlight_brightness();
		pr_err("Alscali:backlight_level = %d\n", backlight_level);

        err = shub_sipc_read(sensor, SHUB_GET_LIGHT_RAWDATA_SUBTYPE, als_data, 2);
        if (err < 0) {
            pr_err("Alscali:read RegMapR_GetLightRawData failed!\n");
            sensor->als_cali_result = CALI_ERR_CRD;
            break;
        } else {
            pr_err("Alscali:ptr[0] = %d\n", ptr[0]);
        }

        pr_err("Alscali:sensor->als_cali_cmd = %d\n", sensor->als_cali_cmd);

        switch(sensor->als_cali_cmd){
            case CALIB_ALS_STD://8
                pr_err("Alscali:CALIB_ALS_STD enter!\n");
                sum[0] += ptr[0];
                pr_err("Alscali:CALIB_ALS_STD:shub_sipc_read: ptr[0] = %d, sum[0] = %d\n", ptr[0], sum[0]);
                break;

            case CALIB_ALS_LEAK://7
                if(backlight_level == 0){
                    sensor->als_cali_result = CALI_ERR_BLW;
                    pr_err("Alscali:[leak]:Backlight level is 0\n");
                    err = -EAGAIN;
                    break;
                }
                pr_err("Alscali:CALIB_ALS_LEAK enter!\n");

                sum[2] += ptr[0];
                pr_err("Alscali:CALIB_ALS_LEAK:shub_sipc_read: ptr[0] = %d, sum[2] = %d\n", ptr[0], sum[2]);
                break;
            case CALIB_ALS_DARK://6
                if((sensor->als_cali_cmd == CALIB_ALS_DARK) && (backlight_level != 0)){
                    sensor->als_cali_result = CALI_ERR_BLW;
                    pr_err("Alscali:[dark]:Backlight level is not 0\n");
                    err = -EAGAIN;
                    break;
                }
                pr_err("Alscali:CALIB_ALS_DARK enter!\n");

                sum[1] += ptr[0];
                pr_err("Alscali:CALIB_ALS_DARK:shub_sipc_read: ptr[0] = %d, sum[1] = %d\n", ptr[0], sum[1]);
                break;
            default :
                pr_err("Alscali:CALIB_NONE enter!\n");
                break;
        }

        if (shub_send_command(sensor, 5, SHUB_SET_FLUSH_SUBTYPE, NULL, 0x00) < 0) {
            pr_err("Alscali:Flush light data fail!!\n");
        }

        msleep(200);
        if(err >= 0) {
            i++;
            errCount = 0;
        } else {
            errCount++;
            continue;
        }
    }while((i < LIGHT_CALI_DATA_COUNT) && !kthread_should_stop());

    pr_err("Alscali: i = %d, sum[1] = %d, sum[2] = %d\n", i, sum[1], sum[2]);

    if((!kthread_should_stop()) && (sensor->als_cali_result == CALI_ERR_NONE)) {
        pr_err("Alscali: sum[1] = %d, sum[2] = %d\n", sum[1], sum[2]);
        pr_err("Alscali:sensor->als_cali_cmd = %d\n", sensor->als_cali_cmd);
        if((sensor->als_cali_cmd == CALIB_ALS_LEAK) || (sensor->als_cali_cmd == CALIB_ALS_DARK)) {
            pr_err("Alscali:cp avedata to tempCali!\n");
            pr_err("Alscali: sum[1] = %d, sum[2] = %d\n", sum[1], sum[2]);

            switch(sensor->als_cali_cmd) {
                case CALIB_ALS_DARK:
                    pr_err("Alscali:cp ave dark data to tempCali[1]!\n");

                    als_of_0level = sum[1] *10000 / LIGHT_CALI_DATA_COUNT;
                    sensor->als_cali_result = CALI_ERR_NONE;
                    tempCali[1] = als_of_0level;
                    pr_err("Alscali: als_of_0level = %d\n", als_of_0level);
                    pr_err("Alscali: sum[1] = %d\n", sum[1]);
                    break;
                case CALIB_ALS_LEAK:
                    pr_err("Alscali:cp ave leak data to tempCali[2]!\n");

                    als_of_102level = sum[2] *10000 / LIGHT_CALI_DATA_COUNT;
                    sensor->als_cali_result = CALI_ERR_NONE;
                    tempCali[2] = als_of_102level;
                    pr_err("Alscali: als_of_102level = %d\n", als_of_102level);
                    pr_err("Alscali: sum[2] = %d\n", sum[2]);
                    break;
                default:
                    break;
            }
        }
        else if(sensor->als_cali_cmd == CALIB_ALS_STD)
        {
            average_als = sum[0] / LIGHT_CALI_DATA_COUNT;
            pr_err("Alscali: light_sum:%ld, average_als = %d\n", sum[0], average_als);

            if (average_als < LIGHT_SENSOR_MIN_VALUE || average_als > LIGHT_SENSOR_MAX_VALUE) {
                pr_err("Alscali:Average data out of range\n");
                sensor->als_cali_result = CALI_ERR_OOR;
            } else {
                tempCali[0] = LIGHT_SENSOR_CALI_VALUE / average_als;
                pr_err("Alscali:lux cali coeff update %d\n", tempCali[0]);
                sensor->als_cali_result = CALI_ERR_NONE;
            }
        }

        if(sensor->als_cali_result == CALI_ERR_NONE) {
            memcpy(als_data, tempCali, sizeof(tempCali));
            err = shub_save_als_cali_data(sensor, als_data, sizeof(als_data));
            if (err < 0) {
                pr_err("Alscali:Save Light Sensor CalibratorData Fail\n");
                sensor->als_cali_result = CALI_ERR_CNSAVE;
            }
        }

        if(sensor->als_cali_result == CALI_ERR_NONE) {
            err = shub_send_command(sensor, SENSOR_TYPE_LIGHT,
                                    SHUB_SET_CALIBRATION_DATA_SUBTYPE,
                                    als_data, CALIBRATION_DATA_LENGTH);
            if (err < 0) {
                pr_err("Alscali:Write Light Sensor CalibratorData Fail\n");
                sensor->als_cali_result = CALI_ERR_CSHUB;
            } else {
                memcpy(&(sensor->alsCoeff), tempCali, sizeof(struct als_cali_data));
                pr_err("Alscali:Coeff data after cali:[%d, %d, %d, %d, %d, %d]\n",
                    sensor->alsCoeff.lux_coeff, sensor->alsCoeff.ch0_coeff,
                    sensor->alsCoeff.ch1_coeff, sensor->alsCoeff.ch2_coeff,
                    sensor->alsCoeff.ch3_coeff, sensor->alsCoeff.ch4_coeff);
            }
        }

        pr_err("Alscali: Calibrator Done. Thread exit. [status = %d]\n", sensor->als_cali_result);
    }

    if((sensor->als_cali_cmd == CALIB_ALS_LEAK) || (sensor->als_cali_cmd == CALIB_ALS_DARK)) {
        i = get_backlight_brightness();
        err = shub_send_command(sensor, SENSOR_LIGHT, SHUB_SET_MODE, ((char *)&i), sizeof(i));
        if (err < 0) {
            pr_err("Alscali:Restore backlight level to cm4 failed!!\n");
        }
    }
    again = 0;
    pr_err("Alscali: Cali Exit\n");
    sensor->caliRunning = false;
    return 0;
}

static int shub_other_cali_thread(void *drvdata)
{
	int err = 0;
	int caliRate = 10;
	struct shub_data *sensor = (struct shub_data *)drvdata;
	char temp_cali_data[CALIBRATION_DATA_LENGTH] = {0};

	pr_err("Othercali: Cali Enter\n");
	sensor->als_cali_result = CALI_ERR_NONE;
	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		pr_err("Othercali:mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}
	sensor->caliRunning = true;
	
	err = shub_send_command(sensor, sensor->cal_id, SHUB_SET_ENABLE_SUBTYPE, NULL, 0);
	if (err < 0) {
		pr_err("Othercali:Set enable into cm4 failed\n", __func__);
		sensor->als_cali_result = CALI_ERR_CM4CMDE;
		sensor->caliRunning = false;
		return 0;
	}
	
	err = shub_send_command(sensor, sensor->cal_id, SHUB_SET_BATCH_SUBTYPE, (char *)&caliRate, sizeof(caliRate));
	if (err < 0) {
		pr_err("Othercali:Set report rate into cm4 failed\n", __func__);
		sensor->als_cali_result = CALI_ERR_CM4CMDE;
		sensor->caliRunning = false;
		return 0;
	}

	msleep(500);
	switch(sensor->als_cali_cmd){
		case CALIB_PS_NOCOVER:
			err = shub_send_command(sensor, sensor->cal_id, SHUB_SET_CALIBRATION_DATA_SUBTYPE, temp_cali_data, sizeof(temp_cali_data));
			if (err < 0) {
				pr_err("Write empty psensor CalibratorData Fail!\n");
				break;
			}
			msleep(20);
			err = set_calib_cmd(sensor, CALIB_EN, sensor->cal_id, 1, 0);
			break;
		case CALIB_PS_NEAR:
			err = set_calib_cmd(sensor, CALIB_EN, sensor->cal_id, 5, 0);
			break;
		case CALIB_PS_FAR:
			err = set_calib_cmd(sensor, CALIB_EN, sensor->cal_id, 6, 0);
			break;
		case CALIB_ACC_HORIZONTAL:
			err = set_calib_cmd(sensor, CALIB_EN, sensor->cal_id, 1, 0);
			break;
		default:
			break;
	}
	
	if (err < 0) {
		pr_err("Othercali:Set enable cali cmd into cm4 failed [sensor:%d]\n", sensor->cal_id);
		sensor->als_cali_result = CALI_ERR_CM4CMDE;
		sensor->caliRunning = false;
		return 0;
	}
	
	do {
		msleep(1000);
		err = set_calib_cmd(sensor, CALIB_CHECK_STATUS, sensor->cal_id, 1, 0);
		if (err < 0) {
			pr_err("Othercali:Send CALIB_CHECK_STATUS cmd fail![%d]\n", err);
			if(SENSOR_PROXIMITY == sensor->cal_id){
				shub_download_calibration_data(sensor, sensor->cal_id);
			}
			sensor->als_cali_result = CALI_ERR_CM4CMDE;
			sensor->caliRunning = false;
			return err;
		}
		
		err = shub_sipc_read(sensor, SHUB_GET_CALIBRATION_DATA_SUBTYPE,
							sensor->calibrated_data, CALIBRATION_DATA_LENGTH);
		if (err < 0) {
			pr_err("Othercali:Read CalibratorData fail![%d]\n", err);
			if(SENSOR_PROXIMITY == sensor->cal_id){
				shub_download_calibration_data(sensor, sensor->cal_id);
			}			
			sensor->als_cali_result = CALI_ERR_CM4CMDE;
			sensor->caliRunning = false;
			return err;
		}
		
		if(sensor->calibrated_data[0] == 2) {
			pr_err("Othercali:Sensor [%d] cali done\n", sensor->cal_id);
			break;
		} else if(sensor->calibrated_data[0] != 1) {
			pr_err("Othercali:Sensor [%d] cali fail![%d]\n", sensor->cal_id, sensor->calibrated_data[0]);
			if(SENSOR_PROXIMITY == sensor->cal_id){
				shub_download_calibration_data(sensor, sensor->cal_id);
			}
			sensor->als_cali_result = CALI_ERR_WSTATUS;
			sensor->caliRunning = false;
			return sensor->calibrated_data[0];
		}
	} while(sensor->calibrated_data[0] == 1);

	err = set_calib_cmd(sensor, CALIB_DATA_READ, sensor->cal_id, 1, 0);
	if (err < 0) {
		pr_err("Othercali:Read CalibratorData fail![%d]\n", err);
		if(SENSOR_PROXIMITY == sensor->cal_id){
			shub_download_calibration_data(sensor, sensor->cal_id);
		}
		sensor->als_cali_result = CALI_ERR_CM4CMDE;
		sensor->caliRunning = false;
		return err;
	}
	
	msleep(500);
	err = shub_sipc_read(sensor, SHUB_GET_CALIBRATION_DATA_SUBTYPE, sensor->calibrated_data, CALIBRATION_DATA_LENGTH);
	if (err < 0) {
		pr_err("Othercali:Read CalibratorData fail![%d]\n", err);
		if(SENSOR_PROXIMITY == sensor->cal_id){
			shub_download_calibration_data(sensor, sensor->cal_id);
		}
		sensor->als_cali_result = CALI_ERR_CM4CMDE;
		sensor->caliRunning = false;
		return err;
	}
	
	if((sensor->cal_id != SENSOR_PROXIMITY) || 
		((sensor->cal_id == SENSOR_PROXIMITY) &&
		 (sensor->calibrated_data[12] == 0x07))) {
		sensor->cal_savests = 1;
		schedule_work(&sensor->savecalifile_work);
		while (sensor->cal_savests == 1) {
			msleep(100);
			pr_err("Othercali:saving cali data...\n");
		}
		err = sensor->cal_savests;
		if(err != 0) {
			sensor->als_cali_result = CALI_ERR_CNSAVE;
			pr_err("Othercali:Save cali data fail![%d]\n", err);
		}
	}
	
	err = shub_send_command(sensor, sensor->cal_id, SHUB_SET_DISABLE_SUBTYPE, NULL, 0);
	if (err < 0) {
		pr_err("Othercali:Set disable into cm4 fail![%d]\n", err);
	}
	
	pr_err("Othercali: Cali Exit\n");
	sensor->caliRunning = false;
	return 0;
}

static ssize_t light_sensor_calibrator_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int err, len;

	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		pr_err("mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}

#if 0
	len = sscanf(buf, "%d %d\n", &sensor->cal_cmd, &sensor->cal_id);
	if (len < 2)
		return -EINVAL;
	pr_debug("id:%d,type:%d\n", sensor->cal_cmd, sensor->cal_id);

	if (sensor->cal_cmd != CALIB_DATA_WRITE ||
		sensor->cal_id != SENSOR_TYPE_LIGHT) {
		pr_err("light sensor cali cmd error\n");
		return -EINVAL;
	}

	err = set_als_calib_cmd(sensor, sensor->cal_cmd, sensor->cal_id);
	if (err < 0)
		pr_err("light sensor cali Fail!\n");
#endif
	if(!sensor->caliRunning) {
		thread_cali = NULL;
	} else {
		pr_err("Als cali thread is running!!!!\n");
		return -EBUSY;
	}

	len = sscanf(buf, "%d %d\n", &sensor->als_cali_cmd, &sensor->cal_id);
	if (len < 2)
		return -EINVAL;
	pr_debug("id:%d,type:%d\n", sensor->als_cali_cmd, sensor->cal_id);

	if (sensor->cal_id == SENSOR_TYPE_LIGHT) {
		if((sensor->als_cali_cmd < CALIB_ALS_DARK) || (sensor->als_cali_cmd > CALIB_ALS_STD)) {
			pr_err("light sensor cali cmd error\n");
			return -EINVAL;
		}
	} else if (sensor->cal_id == SENSOR_TYPE_PROX) {
		if((sensor->als_cali_cmd < CALIB_PS_NOCOVER) ||
			(sensor->als_cali_cmd > CALIB_PS_FAR)) {
			pr_err("Proximity sensor cali cmd error\n");
			return -EINVAL;
		}				
	} else if(sensor->cal_id == SENSOR_TYPE_ACC) {
		if(sensor->als_cali_cmd != CALIB_ACC_HORIZONTAL) {
			pr_err("Acceleration sensor cali cmd error\n");
			return -EINVAL;
		}		
	} else {
		pr_err("The type of sensor cali wrong, cali thread doesn't support!\n");
		return -EINVAL;
	}

	if(sensor->cal_id == SENSOR_TYPE_LIGHT) {
		thread_cali = kthread_run(shub_als_cali_thread, sensor, "als_cali_thread");
	} else {
		thread_cali = kthread_run(shub_other_cali_thread, sensor, "other_cali_thread");
	}
	if (IS_ERR(thread_cali)) {
		err = PTR_ERR(thread_cali);
		pr_err("failed to create kernel cali_thread: %d\n", err);
	}

	return err < 0 ? err : count;
}

static ssize_t light_sensor_calibrator_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
#if 0
	int err, cali_data_coef;
	int status = CALIB_STATUS_NON;
	struct file *pfile;
	char file_path[CALIB_PATH_MAX_LENG];
	char *raw_cali_data = NULL;

	if ((strlen(CALIBRATION_NODE) + strlen(calibration_filename[5])) >
	    (sizeof(file_path) - 1)) {
		pr_err("calibration node path is oversize.\n");
		return -EINVAL;
	}
	snprintf(file_path, sizeof(file_path), "%s%s",
		 CALIBRATION_NODE, calibration_filename[5]);

	pfile = filp_open(file_path, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		err = PTR_ERR(pfile);
		pr_err("open file %s ret=%d\n", file_path, err);
		return sprintf(buf, "%d\n", status);
	}

	raw_cali_data = kmalloc(CALIBRATION_DATA_LENGTH, GFP_KERNEL);
	if (raw_cali_data == NULL) {
		filp_close(pfile, NULL);
		return -ENOMEM;
	}

	err = kernel_read(pfile, raw_cali_data,
			      CALIBRATION_DATA_LENGTH,
			      &pfile->f_pos);
	if (err < 0) {
		pr_err("Error: file read failed\n");
		kfree(raw_cali_data);
		filp_close(pfile, NULL);
		return err;
	}

	memcpy(&cali_data_coef, raw_cali_data, sizeof(cali_data_coef));
	pr_info("cali_data_coef = %d\n", cali_data_coef);
	if (cali_data_coef < 0)
		status = CALIB_STATUS_FAIL;
	else
		status = CALIB_STATUS_PASS;

	kfree(raw_cali_data);
	filp_close(pfile, NULL);

	return sprintf(buf, "%d\n", status);
#endif
	int len = 0;
	struct shub_data *sensor = dev_get_drvdata(dev);
	int *cData = (int *)sensor->calibrated_data;

	if (sensor->caliRunning) {
		len += sprintf((buf + len), "Starus: Cali Running!\n");
	} else {
		len += sprintf((buf + len), "Cali Command: ");
		switch(sensor->als_cali_cmd) {
			case CALIB_ALS_DARK:
				len += sprintf((buf + len), "CALIB_ALS_DARK [%d]\n",  sensor->als_cali_cmd);
				break;
			case CALIB_ALS_LEAK:
				len += sprintf((buf + len), "CALIB_ALS_LEAK [%d]\n",  sensor->als_cali_cmd);
				break;
			case CALIB_ALS_STD:
				len += sprintf((buf + len), "CALIB_ALS_STD [%d]\n",  sensor->als_cali_cmd);
				break;
			case CALIB_PS_NOCOVER:
				len += sprintf((buf + len), "CALIB_PS_NOCOVER [%d]\n",  sensor->als_cali_cmd);
				break;
			case CALIB_PS_NEAR:
				len += sprintf((buf + len), "CALIB_PS_NEAR [%d]\n",  sensor->als_cali_cmd);
				break;
			case CALIB_PS_FAR:
				len += sprintf((buf + len), "CALIB_PS_FAR [%d]\n",  sensor->als_cali_cmd);
				break;
			case CALIB_ACC_HORIZONTAL:
				len += sprintf((buf + len), "CALIB_ACC_HORIZONTAL [%d]\n",  sensor->als_cali_cmd);
				break;
			default :
				break;
		}

		if ((sensor->als_cali_cmd == CALIB_NONE) && (sensor->als_cali_result == CALI_ERR_NONE)) {
			len += sprintf((buf + len), "Starus: Never run any cali!\n");
			return len;
		} else {
			len += sprintf((buf + len), "Starus: Cali Done!\n");
			if(sensor->als_cali_result == CALI_ERR_NONE) {
				len += sprintf((buf + len), "Result: Success!\n");
			} else {
				len += sprintf((buf + len), "Result: Failed! [err = %d]\n", sensor->als_cali_result);
			}
		}

		thread_cali = NULL;
	}

	if(sensor->cal_id == SENSOR_TYPE_LIGHT) {
		len += sprintf((buf + len), "Cali Data: Lux coeff[%d], ch0-ch5 coeff[%d %d %d %d %d]\n",
			sensor->alsCoeff.lux_coeff, sensor->alsCoeff.ch0_coeff, sensor->alsCoeff.ch1_coeff,
			sensor->alsCoeff.ch2_coeff, sensor->alsCoeff.ch3_coeff, sensor->alsCoeff.ch4_coeff);
	} else if(sensor->cal_id == SENSOR_TYPE_PROX) {
		len += sprintf((buf + len), "Prox Cali Data:[CT:%d 2CM:%d 6CM:%d]\n", *cData, *(cData + 1), *(cData + 2));		
	} else if(sensor->cal_id == SENSOR_TYPE_ACC) {
		len += sprintf((buf + len), "Acc Cali Data:[X:%d Y:%d Z:%d]\n", *cData, *(cData + 1), *(cData + 2));	
	}
	
	return len;
}
static DEVICE_ATTR_RW(light_sensor_calibrator);

static ssize_t calibrator_cmd_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);

	return sprintf(buf, "Cmd:%d,Id:%d,Type:%d\n", sensor->cal_cmd,
		       sensor->cal_id, sensor->cal_type);
}

static ssize_t calibrator_cmd_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int len, err;

	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return count;
	}

	dev_info(&sensor->sensor_pdev->dev, "buf=%s\n", buf);
	len = sscanf(buf, "%d %d %d %d\n", &sensor->cal_cmd, &sensor->cal_id,
		     &sensor->cal_type, &sensor->golden_sample);
	/* The 3rd and 4th parameters are optional. */
	if (len < 2 || len > 4)
		return -EINVAL;
	err = set_calib_cmd(sensor, sensor->cal_cmd, sensor->cal_id,
			    sensor->cal_type, sensor->golden_sample);
	dev_info(&sensor->sensor_pdev->dev, "cmd:%d,id:%d,type:%d,golden:%d\n",
		sensor->cal_cmd, sensor->cal_id, sensor->cal_type, sensor->golden_sample);
	if (err < 0)
		dev_err(&sensor->sensor_pdev->dev, " Write Fail!\n");

	return count;
}
static DEVICE_ATTR_RW(calibrator_cmd);

static ssize_t calibrator_data_show(struct device *dev,
				    struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	int err, i, n = 0;

	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}

	err = shub_sipc_read(sensor,
			     SHUB_GET_CALIBRATION_DATA_SUBTYPE,
			     sensor->calibrated_data, CALIBRATION_DATA_LENGTH);
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev, " Read CalibratorData Fail\n");
		return err;
	}

	dev_info(&sensor->sensor_pdev->dev, "cmd:%d,id:%d,type:%d\n",
		sensor->cal_cmd, sensor->cal_id, sensor->cal_type);
	if (sensor->cal_cmd == CALIB_DATA_READ) {
		/* debuginfor(cal_data,CALIBRATION_DATA_LENGTH); */
		sensor->cal_savests = 1;
		schedule_work(&sensor->savecalifile_work);
		while (sensor->cal_savests == 1) {
			msleep(100);
			dev_info(&sensor->sensor_pdev->dev, "saving cali data...\n");
		}
		/* 0 success ,else error */
		err = sensor->cal_savests;
		dev_info(&sensor->sensor_pdev->dev, "done err=%d\n", err);
		return sprintf(buf, "%d\n", err);
	}
	if (sensor->cal_cmd == CALIB_CHECK_STATUS)
		return sprintf(buf, "%d\n", sensor->calibrated_data[0]);

	for (i = 0; i < CALIBRATION_DATA_LENGTH; i++)
		n += sprintf(buf + n, "%d ", sensor->calibrated_data[i]);
	return n;
}

static ssize_t calibrator_data_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	char data[CALIBRATION_DATA_LENGTH];
	int i, err, temp, cr, offset = 0;

	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}

	for (i = 0; i < CALIBRATION_DATA_LENGTH; i++) {
		if (sscanf(buf + offset, "%d %n", &temp, &cr) != 1)
			return -EINVAL;
		data[i] = (unsigned char)temp;
		offset += cr;
	}
	err = shub_send_command(sensor, HANDLE_MAX,
				SHUB_SET_CALIBRATION_DATA_SUBTYPE,
				data, sizeof(data));
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev, "Write CalibratorData Fail\n");
		return err;
	}

	return count;
}
static DEVICE_ATTR_RW(calibrator_data);

static ssize_t version_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	u8 data[4];
	u32 version = 0;
	int err;

	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}

	err = shub_sipc_read(sensor, SHUB_GET_FWVERSION_SUBTYPE, data, 4);
	if (err >= 0) {
		memcpy(&version, data, sizeof(version));
	} else {
		dev_err(&sensor->sensor_pdev->dev, "Read  FW Version Fail\n");
		return err;
	}

	return sprintf(buf, "%u\n", version);
}
static DEVICE_ATTR_RO(version);

#if 0
static ssize_t als_mode_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	u8 als_mode;
	int err;
	struct shub_data *sensor = dev_get_drvdata(dev);

	if (sscanf(buf, "%4hhx", &als_mode) != 1)
		return -EINVAL;

	err = shub_send_command(sensor, SENSOR_LIGHT, SHUB_SET_MODE,
				&als_mode, sizeof(als_mode));
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev, "Set als_mode Fail\n");
		return err;
	}

	return count;
}
static DEVICE_ATTR_WO(als_mode);
#endif

void get_dynamic_data(struct shub_data *sensor)
{
	int i;
	u8 data[30], type, len;

    //dev_err(&sensor->sensor_pdev->dev, "get_dynamic_data enter\n");

	type = sensor->dynamic_data_get.type;
	len = sensor->dynamic_data_get.length;
	memcpy(data, sensor->dynamic_data_get.customer_data, len);
	for (i = 0; i < len; i++)
		dev_err(&sensor->sensor_pdev->dev,
			"handle %d get dynamic data  data[%d] = %d\n", type,  i, data[i]);

    if(type == ORDER_PROX)//data from proximity dynamic code
    {
        //dev_err(&sensor->sensor_pdev->dev, "get_dynamic_data type is proximity\n");
        memcpy(data_from_dynamic, data, sizeof(data_from_dynamic));
        dev_err(&sensor->sensor_pdev->dev, "get_dynamic_data cali_ct = %d, cali_2cm = %d, psensor_high = %d, psensor_low = %d\n",
                                                            data_from_dynamic[0], data_from_dynamic[1], data_from_dynamic[2], data_from_dynamic[3]);
    }
}

static ssize_t data_to_dynamic_store(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	int err = 0;
	uint8_t data_to_dynamic = 0;
	struct shub_data *sensor = dev_get_drvdata(dev);
    dev_err(&sensor->sensor_pdev->dev, "data_to_dynamic_stores enter\n");

	if (sscanf(buf, "%d\n", &data_to_dynamic) != 1)
		return -EINVAL;

	err = shub_send_command(sensor, ORDER_LIGHT, AP_SEND_DATA_TO_DYNAMIC_SUBTYPE, &data_to_dynamic, sizeof(data_to_dynamic));
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev, "Send dynamic para_data fail\n");
	}
	dev_err(&sensor->sensor_pdev->dev, "data_to_dynamic Send dynamic para_data:%d\n", data_to_dynamic);
    //0:close ps dynamic thread;1:open ps dynamic thread.
    if((data_to_dynamic == 1) || (data_to_dynamic == 0)){
        proximity_dynamic_thread_flag = data_to_dynamic;
    }

	return count;
}
static DEVICE_ATTR_WO(data_to_dynamic);

static ssize_t raw_data_acc_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	u8 data[6];
	u16 *ptr;
	int err;

	ptr = (u16 *)data;
	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}

	err = shub_sipc_read(sensor,
			     SHUB_GET_ACCELERATION_RAWDATA_SUBTYPE, data, 6);
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev, "read RegMapR_GetAccelerationRawData failed!\n");
		return err;
	}
	return sprintf(buf, "%d %u %u %u\n", err, ptr[0], ptr[1], ptr[2]);
}
static DEVICE_ATTR_RO(raw_data_acc);

#if 0

static ssize_t raw_data_mag_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	u8 data[6];
	u16 *ptr;
	int err;

	ptr = (u16 *)data;
	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}
	err = shub_sipc_read(sensor,
			     SHUB_GET_MAGNETIC_RAWDATA_SUBTYPE, data, 6);
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev, "read RegMapR_GetMagneticRawData failed!\n");
		return err;
	}
	return sprintf(buf, "%d %u %u %u\n", err, ptr[0], ptr[1], ptr[2]);
}
static DEVICE_ATTR_RO(raw_data_mag);

static ssize_t raw_data_gyro_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	u8 data[6];
	u16 *ptr;
	int err;

	ptr = (u16 *)data;
	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}
	err = shub_sipc_read(sensor,
			     SHUB_GET_GYROSCOPE_RAWDATA_SUBTYPE, data, 6);
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev, "read RegMapR_GetGyroRawData failed!\n");
		return err;
	}
	return sprintf(buf, "%d %u %u %u\n", err, ptr[0], ptr[1], ptr[2]);
}
static DEVICE_ATTR_RO(raw_data_gyro);
#endif

static ssize_t raw_data_acc1_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	float ptr[6] = {0};
	int data_out[3]={0};
	int err=0;
	memcpy(ptr,sensor_event_acc_data.fdata,6*sizeof(float));

	ptr[0]*=1000;
	ptr[1]*=1000;
	ptr[2]*=1000;
	data_out[0]=(int)ptr[0];
	data_out[1]=(int)ptr[1];
	data_out[2]=(int)ptr[2];
    pr_err("raw_data_acc1_show x=%d, y=%d, z=%d\n", data_out[0], data_out[1], data_out[2]);

	return sprintf(buf, "%d %d %d %d\n", err, data_out[0], data_out[1], data_out[2]);
}
static DEVICE_ATTR_RO(raw_data_acc1);

static ssize_t raw_data_als1_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
    float ptr[6];
    int data_out[3]={0};
    int len = 0;
    memcpy(ptr,sensor_event_als_data.fdata,6*sizeof(float));

    data_out[0]=(int)ptr[0];
    data_out[1]=(int)ptr[1];
    data_out[2]=(int)ptr[2];

    len += sprintf(buf, ": als : %d\n",data_out[0]);
    len += sprintf((buf + len), ": alsval : %d\n",data_out[1]);
    len += sprintf((buf + len), ": irval  : %d\n",data_out[2]);
    len += sprintf((buf + len), ": 1th psensor ratio: %d\n", (data_out[2] * 100 / (data_out[1] + 1)));
    len += sprintf((buf + len), ": 2th psensor ratio: %d\n", (data_out[2] * 100 / (data_out[1] + 1)));
    pr_err("raw_data_als1_show als=%d, alsval=%d, irval=%d\n", data_out[0], data_out[1], data_out[2]);

    return len;
}
static DEVICE_ATTR_RO(raw_data_als1);

static ssize_t raw_data_ps1_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
    float ptr[4];
    int data_out[3]={0};
    int len = 0;
    memcpy(ptr,sensor_event_ps_data.fdata,4*sizeof(float));

    data_out[0]=(int)ptr[0];
    data_out[1]=(int)ptr[1];

    len += sprintf(buf, ": state : %d\n",data_out[0]);
    //len += sprintf((buf + len), ": ps_current_thd_h : %d\n",data_out[1] >> 16);
    //len += sprintf((buf + len), ": ps_current_thd_l  : %d\n",data_out[1] & 0xFFFF);
    len += sprintf((buf + len), ": ps_current_thd_h : %d\n", data_from_dynamic[2]);
    len += sprintf((buf + len), ": ps_current_thd_l  : %d\n",data_from_dynamic[3]);
    len += sprintf((buf + len), ": ps_dynamic_flag  : %d\n",proximity_dynamic_thread_flag);

    return len;
}
static DEVICE_ATTR_RO(raw_data_ps1);

static ssize_t raw_data_als_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	u8 data[2];
	u16 *ptr;
	int err;

	ptr = (u16 *)data;
	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}

	err = shub_sipc_read(sensor,
			     SHUB_GET_LIGHT_RAWDATA_SUBTYPE, data, sizeof(data));
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev, "read RegMapR_GetLightRawData failed!\n");
		return err;
	}
	return sprintf(buf, "%d\n", ptr[0]);
}
static DEVICE_ATTR_RO(raw_data_als);

static ssize_t raw_data_ps_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	u8 data[2];
	u16 *ptr = (u16 *)data;
	int err;

    //dev_err(&sensor->sensor_pdev->dev, "raw_data_ps_show : ls98xx_enable = %d\n", ls98xx_enable);
	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		dev_err(&sensor->sensor_pdev->dev, "mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}
	err = shub_sipc_read(sensor,
			     SHUB_GET_PROXIMITY_RAWDATA_SUBTYPE, data, sizeof(data));
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev, "read RegMapR_GetProximityRawData failed!\n");
		return err;
	}

	return sprintf(buf, "%d\n", ptr[0]);
}
static DEVICE_ATTR_RO(raw_data_ps);

static ssize_t sensorhub_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", sensor->is_sensorhub);
}

static ssize_t sensorhub_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct shub_data *sensor = dev_get_drvdata(dev);

	if (sscanf(buf, "%d\n", &sensor->is_sensorhub) != 1)
		return -EINVAL;
	return count;
}
static DEVICE_ATTR_RW(sensorhub);

static ssize_t hwsensor_id_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i, len = 0;
	u32 temp_id;
	const char * const TB_TYPENAME[] = {
		"acc", "mag", "gyr",
		"prox", "light", "prs", "color"};

	/* get_sensor_id(g_sensor); */
	for (i = 0; i < _HW_SENSOR_TOTAL; i++) {
		if (i > 0)
			len += sprintf(buf + len, "\n");

		len += snprintf(buf + len, 10, "%s:%d",
			TB_TYPENAME[i],
			hw_sensor_id[i].id_status);

		if (hw_sensor_id[i].id_status == _IDSTA_OK) {
			temp_id = hw_sensor_id[i].vendor_id * VENDOR_ID_OFFSET +
					hw_sensor_id[i].chip_id;
			len += snprintf(buf + len, 32, "(0x%04x, %s)",
				temp_id, hw_sensor_id[i].pname);
		}
	}
	len += snprintf(buf + len, 50,
		"\n#read id status:0 not ready, 1 OK, 2 fail.\n");

	return len;
}
static DEVICE_ATTR_RO(hwsensor_id);

static ssize_t sensor_info_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int i;
	int len = 0;
	const char * const SENSOR_TYPENAME[] = {
		"acc", "mag", "gyro",
		"prox", "light", "prs", "color"};

	for (i = 0; i < _HW_SENSOR_TOTAL;) {
		len += snprintf(buf + len, 16, "%d %s name:",
			(hw_sensor_id[i].id_status == _IDSTA_OK),
			SENSOR_TYPENAME[i]);

		if (hw_sensor_id[i].id_status == _IDSTA_OK) {
			len += snprintf(buf + len,
					sizeof(hw_sensor_id[i].pname),
					"%s", hw_sensor_id[i].pname);
		}

		len += sprintf(buf + len, "\n");
		i += 1;
	}

	return len;
}
static DEVICE_ATTR_RO(sensor_info);

#if 0
static ssize_t mag_cali_flag_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	char data_buf[16];
	unsigned short mag_cali_check;
	int err, flag;
	long mag_library_size;
	struct shub_data *sensor = dev_get_drvdata(dev);

	if (sscanf(buf, "%d %ld %hu", &flag, &mag_library_size,
		   &mag_cali_check) != 3)
		return -EINVAL;
	dev_dbg(&sensor->sensor_pdev->dev, "the flag = %d, the mag size is %ld, crc_check = %d\r\n",
		flag, mag_library_size, mag_cali_check);

	memcpy(data_buf, &flag, sizeof(flag));
	memcpy(data_buf + sizeof(flag), &mag_library_size,
	       sizeof(mag_library_size));
	memcpy(data_buf + sizeof(flag) + sizeof(mag_library_size),
	       &mag_cali_check, sizeof(mag_cali_check));

	err = shub_send_command(sensor, SHUB_NODATA,
				SHUB_SEND_MAG_CALIBRATION_FLAG,
				data_buf, 16);
	if (err < 0) {
		dev_err(&sensor->sensor_pdev->dev, " magcali_flag_store Fail\n");
		return err;
	}

	return 0;
}
static DEVICE_ATTR_WO(mag_cali_flag);
#endif

static ssize_t shub_debug_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	char *a = "Description:\n";
	char *b = "\techo \"op sid reg val mask\" > shub_debug\n";
	char *c = "\tcat shub_debug\n\n";
	char *d = "Detail:\n";
	char *e = "\t op(operator): 0:open 1:close 2:read 3:write\n";
	char *f = "\t sid(sensorid): 0:acc 1:mag 2:gyro ";
	char *g = "3:proximity 4:light 5:pressure 6:new_dev\n";
	char *h = "\t reg: the operate register that you want\n";
	char *i = "\t value: The value to be written or show read value\n";
	char *j = "\t mask: The mask of the operation\n\n";
	char *k = "\t status: the state of execution. 1:success 0:fail\n\n";
	char l[50], m[50];

	snprintf(l, sizeof(l), "[shub_debug]operate:0x%x sensor_id:0x%x",
		 sensor->log_control.debug_data[0],
		 sensor->log_control.debug_data[1]);
	snprintf(m, sizeof(m), " reg:0x%x value:0x%x status:0x%x\n\n",
		 sensor->log_control.debug_data[2],
		 sensor->log_control.debug_data[3],
		 sensor->log_control.debug_data[4]);

	return sprintf(buf, "\n%s%s%s%s%s%s%s%s%s%s%s%s%s\n",
		a, b, c, d, e, f, g, h, i, j, k, l, m);
}

static ssize_t shub_debug_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	u8 data_buf[5];
	struct shub_data *sensor = dev_get_drvdata(dev);

	if (sscanf(buf, "%4hhx %4hhx %4hhx %4hhx %4hhx\n",
		   &data_buf[0], &data_buf[1], &data_buf[2],
		   &data_buf[3], &data_buf[4]) != 5)
		return -EINVAL;

	shub_send_command(sensor, SHUB_NODATA,
			  SHUB_SEND_DEBUG_DATA,
			  data_buf, sizeof(data_buf));

	return count;
}
static DEVICE_ATTR_RW(shub_debug);

static ssize_t cm4_operate_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	char l[50], m[50];

	snprintf(l, sizeof(l), "[cm4_operate]operate:0x%x interface:0x%x ",
		 sensor->cm4_operate_data[0],
		 sensor->cm4_operate_data[1]);

	snprintf(m, sizeof(m),
		 "addr:0x%x reg:0x%x value:0x%x status:0x%x\n\n",
		 sensor->cm4_operate_data[2],
		 sensor->cm4_operate_data[3],
		 sensor->cm4_operate_data[4],
		 sensor->cm4_operate_data[5]);

	memset(sensor->cm4_operate_data, 0, sizeof(sensor->cm4_operate_data));

	return sprintf(buf, "\nDescription :\n"
		 "\techo \"op intf addr reg value mask\" > cm4_operate\n"
		 "\tcat cm4_operate\n\n"
		 "Detail :\n"
		 "\top: 1:read 2:write 3:check_reg 4:set_delay 5:set_gpio "
		 "6:algo_log_control\n"
		 "\tintf(i2c interface): 0:i2c0 1:i2c1\n"
		 "\taddr: IC slave_addr.if find from opcode,need >>1\n"
		 "\treg: IC reg or set_gpio reg\n"
		 "\tvalue: i2c writen value or set_gpio value "
		 "or control algo log(0 or 1)\n"
		 "\tmask: i2c r/w bit operate or set_delay value(ms)\n\n"
		 "\tstatus: show execution result. 1:success 0:fail\n\n"
		 "%s%s\n", l, m);
}

static ssize_t cm4_operate_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	u8 cm4_buf[6];
	struct shub_data *sensor = dev_get_drvdata(dev);

	if (sscanf(buf, "%4hhx %4hhx %4hhx %4hhx %4hhx %4hhx\n", &cm4_buf[0],
		   &cm4_buf[1], &cm4_buf[2], &cm4_buf[3], &cm4_buf[4],
		   &cm4_buf[5]) != 6)
		return -EINVAL;

	shub_send_command(sensor, SHUB_NODATA,
			  SHUB_CM4_OPERATE, cm4_buf,
			  sizeof(cm4_buf));

	return count;
}

static DEVICE_ATTR_RW(cm4_operate);

static ssize_t cm4_spi_set_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	char m[50];

	snprintf(m, sizeof(m), "status:0x%x\n\n", sensor->cm4_operate_data[5]);

	memset(sensor->cm4_operate_data, 0, sizeof(sensor->cm4_operate_data));

	return sprintf(buf, "\nDescription :\n"
		 "\techo \"bus_num set_op freq cs mode bit_per_word\" > cm4_operate\n"
		 "\tcat cm4_spi_set\n\n"
		 "Detail :\n"
		 "\tbus_num: 0: cm4 spi0\n"
		 "\tset_op: 3\n"
		 "\tfreq: spi frequency, for example 9: 9MHz, a: 10MHz, b: 11MHz\n"
		 "\tcs: spi chip_select num, default 0\n"
		 "\tmode: configure spi CPOL, CPHA, valid value: 0, 1, 2, or 3\n"
		 "\tbit_per_word: valid value: 8, 16 or 32\n\n"
		 "\tstatus: show execution result. 1:success 0:fail\n\n"
		 "%s\n", m);
}
static DEVICE_ATTR_RO(cm4_spi_set);

static ssize_t cm4_spi_sync_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct shub_data *sensor = dev_get_drvdata(dev);
	char l[50], m[50];

	snprintf(l, sizeof(l), "[cm4_operate]read_val1:0x%x read_val2:0x%x ",
		 sensor->cm4_operate_data[2],
		 sensor->cm4_operate_data[3]);

	snprintf(m, sizeof(m),
		 "read_val3:0x%x bytes:%u\n\n",
		 sensor->cm4_operate_data[4],
		 sensor->cm4_operate_data[5]);

	memset(sensor->cm4_operate_data, 0, sizeof(sensor->cm4_operate_data));

	return sprintf(buf, "\nDescription :\n"
		 "\techo \"op spi_sync reg_addr value1 value2 len\" > cm4_operate\n"
		 "\tcat cm4_spi_sync\n\n"
		 "Detail :\n"
		 "\top: 1:read 0:write\n"
		 "\tspi_sync: 4\n"
		 "\treg_addr: the reg to be read or written\n"
		 "\tvalue1: the first value to be written\n"
		 "\tvalue2: the second value to be written\n"
		 "\tlen: num of regs to be read or written\n\n"
		 "execution result:\n"
		 "%s%s\n", l, m);
}
static DEVICE_ATTR_RO(cm4_spi_sync);

static ssize_t als_cali_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err;
	int cali_lux_coef = 0, cali_A_coef = 0, cali_R_coef = 0;
	int cali_G_coef = 0, cali_B_coef = 0, cali_C_coef = 0;
	int len = 0;
	struct file *pfile;
	char raw_cali_data[CALIBRATION_DATA_LENGTH] = {0};
	struct shub_data *sensor = dev_get_drvdata(dev);

	pfile = filp_open(SHUB_LIGHT_CALI_DATA_FILE, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		err = PTR_ERR(pfile);
		pr_err("open file light cali fail ret=%d,Use default calibration data!\n", err);
	} else {
		err = kernel_read(pfile, raw_cali_data,
			          CALIBRATION_DATA_LENGTH,
			          &pfile->f_pos);
		if (err < 0) {
			 pr_err("Error: file read failed, Use default calibration data!\n");
		} else {
			memcpy(&cali_lux_coef, (raw_cali_data), sizeof(int));
			memcpy(&cali_A_coef, (raw_cali_data + 4), sizeof(int));
			memcpy(&cali_R_coef, (raw_cali_data + 8), sizeof(int));
			memcpy(&cali_G_coef, (raw_cali_data + 12), sizeof(int));
			memcpy(&cali_B_coef, (raw_cali_data + 16), sizeof(int));
			memcpy(&cali_C_coef, (raw_cali_data + 20), sizeof(int));
		}

		filp_close(pfile, NULL);
	}

    len += sprintf(buf, "Coeff Data[DRV]:[%d,%d,%d,%d,%d,%d]\n", sensor->alsCoeff.lux_coeff,
                        sensor->alsCoeff.ch0_coeff, sensor->alsCoeff.ch1_coeff,
                        sensor->alsCoeff.ch2_coeff, sensor->alsCoeff.ch3_coeff,
                        sensor->alsCoeff.ch4_coeff);
    len += sprintf((buf + len), "Coeff Data[file]:[%d,%d,%d,%d,%d,%d]\n", cali_lux_coef,
                        cali_A_coef, cali_R_coef, cali_G_coef, cali_B_coef, cali_C_coef);

	return len;
}

static ssize_t als_cali_data_store(struct device *dev,
				 struct device_attribute *attr,
				 const char *buf, size_t count)
{
	int err, len;
	int cali_data[6] = {0, 0, 0, 0, 0, 0};
	struct file *pfile;
	char als_data[CALIBRATION_DATA_LENGTH] = {0};
	struct shub_data *sensor = dev_get_drvdata(dev);

	if (sensor->mcu_mode <= SHUB_OPDOWNLOAD) {
		pr_err("mcu_mode == SHUB_BOOT!\n");
		return -EINVAL;
	}

	len = sscanf(buf, "%d %d %d %d %d %d\n", &cali_data[0], &cali_data[1],
	                      &cali_data[2], &cali_data[3], &cali_data[4], &cali_data[5]);

    pfile = filp_open(SHUB_LIGHT_CALI_DATA_FILE, O_WRONLY | O_CREAT | O_TRUNC, 0666);
	if (IS_ERR(pfile)) {
		err = PTR_ERR(pfile);
		pr_err("open file light cali fail error=%d\n", err);
	} else {
		memcpy(als_data, (char *)cali_data, sizeof(cali_data));

		err = kernel_write(pfile, als_data,
			      CALIBRATION_DATA_LENGTH,
			      &pfile->f_pos);
		if (err < 0)
			pr_err("err=%d\n", err);

		filp_close(pfile, NULL);
	}

	if(!(err < 0)) {
		//sensor->alsCoeff.lux_coeff = cali_data[0];
		//sensor->alsCoeff.ch0_coeff = cali_data[1];
		//sensor->alsCoeff.ch1_coeff = cali_data[2];
		//sensor->alsCoeff.ch2_coeff = cali_data[3];
		//sensor->alsCoeff.ch3_coeff = cali_data[4];
		//sensor->alsCoeff.ch4_coeff = cali_data[5];
		memcpy(&(sensor->alsCoeff), cali_data, sizeof(cali_data));
		pr_err("Write light cali file success\n", err);
	}

	return (err < 0) ? err : count;
}
static DEVICE_ATTR_RW(als_cali_data);

static ssize_t ps_cali_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err, len = 0;
	int cali_ct = 0, cali_2cm = 0, cali_6cm = 0;
	struct file *pfile;
	char raw_cali_data[CALIBRATION_DATA_LENGTH] = {0};

	pfile = filp_open(SHUB_PROX_CALI_DATA_FILE, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		err = PTR_ERR(pfile);
		pr_err("open file Psensor cali fail ret=%d,Use default calibration data!\n", err);
		pr_err("ps_cali_data_show 1 [ct:%d, 2cm:%d, 6cm:%d]\n", data_from_dynamic[0], data_from_dynamic[1], 0);
		len = sprintf(buf, "Cali Data[ct:%d, 2cm:%d, 6cm:%d]\n", data_from_dynamic[0], data_from_dynamic[1], 0);
		return len;
	} else {
		err = kernel_read(pfile, raw_cali_data,
			          CALIBRATION_DATA_LENGTH,
			          &pfile->f_pos);
		if (err < 0) {
			 pr_err("Error: file read failed, Use default calibration data!\n");
		} else {
			memcpy(&cali_ct, (raw_cali_data), sizeof(int));
			memcpy(&cali_2cm, (raw_cali_data + 4), sizeof(int));
			memcpy(&cali_6cm, (raw_cali_data + 8), sizeof(int));
		}

		filp_close(pfile, NULL);		
	}
    pr_err("ps_cali_data_show 2[ct:%d, 2cm:%d, 6cm:%d]\n", cali_ct, cali_2cm, cali_6cm);

	len += sprintf(buf, "Cali Data[ct:%d, 2cm:%d, 6cm:%d]\n", cali_ct, cali_2cm, cali_6cm);

	return len;
}
static DEVICE_ATTR_RO(ps_cali_data);

static ssize_t acc_cali_data_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int err, len = 0;
	int x_axis = 0, y_axis = 0, z_axis = 0;
	struct file *pfile;
	char raw_cali_data[CALIBRATION_DATA_LENGTH] = {0};

	pfile = filp_open(SHUB_ACC_CALI_DATA_FILE, O_RDONLY, 0);
	if (IS_ERR(pfile)) {
		err = PTR_ERR(pfile);
		pr_err("open file acc cali fail ret=%d,Use default calibration data!\n", err);
	} else {
		err = kernel_read(pfile, raw_cali_data,
			          CALIBRATION_DATA_LENGTH,
			          &pfile->f_pos);
		if (err < 0) {
			 pr_err("Error: file read failed, Use default calibration data!\n");
		} else {
			memcpy(&x_axis, (raw_cali_data), sizeof(int));
			memcpy(&y_axis, (raw_cali_data + 4), sizeof(int));
			memcpy(&z_axis, (raw_cali_data + 8), sizeof(int));
		}

		filp_close(pfile, NULL);		
	}
	pr_err("acc_cali_data_show [X:%d, Y:%d, Z:%d]\n", x_axis, y_axis, z_axis);
	len += sprintf(buf, "Cali Data[X:%d, Y:%d, Z:%d]\n", x_axis, y_axis, z_axis);

	return len;
}
static DEVICE_ATTR_RO(acc_cali_data);

static struct attribute *sensorhub_attrs[] = {
	&dev_attr_debug_data.attr,
	&dev_attr_reader_enable.attr,
	&dev_attr_op_download.attr,
	&dev_attr_logctl.attr,
	&dev_attr_enable.attr,
	&dev_attr_batch.attr,
	&dev_attr_flush.attr,
	&dev_attr_mcu_mode.attr,
	&dev_attr_calibrator_cmd.attr,
	&dev_attr_calibrator_data.attr,
	&dev_attr_light_sensor_calibrator.attr,
	&dev_attr_version.attr,
	&dev_attr_data_to_dynamic.attr,
	
	&dev_attr_raw_data_acc.attr,
	&dev_attr_raw_data_acc1.attr,

#if 0
	&dev_attr_als_mode.attr,
	&dev_attr_raw_data_acc.attr,
	&dev_attr_raw_data_mag.attr,
	&dev_attr_raw_data_gyro.attr,
#endif
	&dev_attr_raw_data_als1.attr,
	&dev_attr_raw_data_als.attr,
	&dev_attr_raw_data_ps1.attr,
	&dev_attr_raw_data_ps.attr,
	&dev_attr_sensorhub.attr,
	&dev_attr_hwsensor_id.attr,
	&dev_attr_sensor_info.attr,
#if 0
	&dev_attr_mag_cali_flag.attr,
#endif
	&dev_attr_shub_debug.attr,
	&dev_attr_cm4_operate.attr,
	&dev_attr_als_cali_data.attr,
	&dev_attr_ps_cali_data.attr,
	&dev_attr_acc_cali_data.attr,
	&dev_attr_cm4_spi_set.attr,
	&dev_attr_cm4_spi_sync.attr,
	NULL,
};
ATTRIBUTE_GROUPS(sensorhub);

static int shub_notifier_fn(struct notifier_block *nb,
			    unsigned long action, void *data)
{
	struct shub_data *sensor = container_of(nb, struct shub_data,
			early_suspend);

	switch (action) {
	case PM_SUSPEND_PREPARE:
		cancel_delayed_work_sync(&sensor->time_sync_work);
		shub_synctimestamp(sensor);
		shub_send_ap_status(sensor, SHUB_SLEEP);
		break;
	case PM_POST_SUSPEND:
		shub_send_ap_status(sensor, SHUB_NORMAL);
		queue_delayed_work(sensor->driver_wq,
				   &sensor->time_sync_work, 0);
		break;
	default:
		break;
	}
	return 0;
}
#if 0
static int shub_reboot_notifier_fn(struct notifier_block *nb, unsigned long action, void *data)
{
	struct shub_data *sensor = container_of(nb, struct shub_data,
			shub_reboot_notifier);
	int i = 0;

	for (i = 0; i < MAX_SENSOR_HANDLE; i++) {
		if (sensor_status[i]) {
			if (shub_send_command(sensor, i, sensor_status[i], NULL, 0) < 0)
				dev_err(&sensor->sensor_pdev->dev, "reboot write disable failed\n");
			dev_info(&sensor->sensor_pdev->dev, "sensor_status[%d]\n", i);
		}
	}
	dev_info(&sensor->sensor_pdev->dev, "reboot action=%d\n", action);
	return NOTIFY_OK;
}
#endif
/* iio device reg */
static void iio_trigger_work(struct irq_work *work)
{
	struct shub_data *mcu_data =
	    container_of(work, struct shub_data, iio_irq_work);
	iio_trigger_poll(mcu_data->trig);
}

static irqreturn_t shub_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct shub_data *mcu_data = iio_priv(indio_dev);

	mutex_lock(&mcu_data->mutex_lock);
	iio_trigger_notify_done(mcu_data->indio_dev->trig);
	mutex_unlock(&mcu_data->mutex_lock);
	return IRQ_HANDLED;
}

static const struct iio_buffer_setup_ops shub_iio_buffer_setup_ops = {
	.postenable = iio_triggered_buffer_postenable,
	.predisable = iio_triggered_buffer_predisable,
};

static void shub_pseudo_irq_enable(struct iio_dev *indio_dev)
{
	struct shub_data *mcu_data = iio_priv(indio_dev);

	atomic_cmpxchg(&mcu_data->pseudo_irq_enable, 0, 1);
}

static void shub_pseudo_irq_disable(struct iio_dev *indio_dev)
{
	struct shub_data *mcu_data = iio_priv(indio_dev);

	atomic_cmpxchg(&mcu_data->pseudo_irq_enable, 1, 0);
}

static void shub_set_pseudo_irq(struct iio_dev *indio_dev, int enable)
{
	if (enable)
		shub_pseudo_irq_enable(indio_dev);
	else
		shub_pseudo_irq_disable(indio_dev);
}

static int shub_data_rdy_trigger_set_state(struct iio_trigger *trig, bool state)
{
	struct iio_dev *indio_dev =
		iio_trigger_get_drvdata(trig);
	struct shub_data *mcu_data = iio_priv(indio_dev);

	mutex_lock(&mcu_data->mutex_lock);
	shub_set_pseudo_irq(indio_dev, state);
	mutex_unlock(&mcu_data->mutex_lock);
	return 0;
}

static const struct iio_trigger_ops shub_iio_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &shub_data_rdy_trigger_set_state,
};

static int shub_probe_trigger(struct iio_dev *iio_dev)
{
	struct shub_data *mcu_data = iio_priv(iio_dev);
	int ret;

	iio_dev->pollfunc = iio_alloc_pollfunc(&iio_pollfunc_store_time,
					       &shub_trigger_handler,
					       IRQF_ONESHOT,
					       iio_dev,
					       "%s_consumer%d",
					       iio_dev->name, iio_dev->id);
	if (!iio_dev->pollfunc)
		return -ENOMEM;

	mcu_data->trig =
	    iio_trigger_alloc("%s-dev%d", iio_dev->name, iio_dev->id);
	if (!mcu_data->trig) {
		iio_dealloc_pollfunc(iio_dev->pollfunc);
		return -ENOMEM;
	}
	mcu_data->trig->dev.parent = &mcu_data->sensor_pdev->dev;
	mcu_data->trig->ops = &shub_iio_trigger_ops;
	iio_trigger_set_drvdata(mcu_data->trig, iio_dev);

	ret = iio_trigger_register(mcu_data->trig);
	if (ret)
		goto error_free_trig;

	return 0;

error_free_trig:
	iio_trigger_free(mcu_data->trig);
	iio_dealloc_pollfunc(iio_dev->pollfunc);
	return ret;
}

static int shub_read_raw(struct iio_dev *iio_dev,
			 struct iio_chan_spec const *chan, int *val, int *val2,
			 long mask)
{
	struct shub_data *mcu_data = iio_priv(iio_dev);
	int ret = -EINVAL;

	if (chan->type == IIO_LIGHT) {
#if 0
		*val =  channel_data_light_x_int0;
		dev_dbg(&iio_dev->dev, " val = %u, time = %lu, now = %lu\n",
			*val, channel_data_light_time, jiffies);
#endif
		return IIO_VAL_INT;
	}

	if (chan->type != IIO_ACCEL)
		return ret;

	mutex_lock(&mcu_data->mutex_lock);
	switch (mask) {
	case 0:
		*val = mcu_data->iio_data[chan->channel2 - IIO_MOD_X];
		ret = IIO_VAL_INT;
		break;

	case IIO_CHAN_INFO_SCALE:
		/* Gain : counts / uT = 1000 [nT] */
		/* Scaling factor : 1000000 / Gain = 1000 */
		*val = 0;
		*val2 = 1000;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;

	default:
		break;
	}
	mutex_unlock(&mcu_data->mutex_lock);
	return ret;
}

#define IIO_ST(si, rb, sb, sh)						\
	{ .sign = si, .realbits = rb, .storagebits = sb, .shift = sh }

#define SHUB_CHANNEL(axis, bits) {                  \
	.type = IIO_ACCEL,	                    \
	.modified = 1,                          \
	.channel2 = (axis) + 1,                     \
	.scan_index = (axis),	                    \
	.scan_type = IIO_ST('u', (bits), (bits), 0)	    \
}

static const struct iio_chan_spec shub_channels[] = {
	SHUB_CHANNEL(SHUB_SCAN_ID, SHUB_IIO_CHN_BITS),
	SHUB_CHANNEL(SHUB_SCAN_RAW_0, SHUB_IIO_CHN_BITS),
	SHUB_CHANNEL(SHUB_SCAN_RAW_1, SHUB_IIO_CHN_BITS),
	SHUB_CHANNEL(SHUB_SCAN_RAW_2, SHUB_IIO_CHN_BITS),
	IIO_CHAN_SOFT_TIMESTAMP(SHUB_SCAN_TIMESTAMP),
	{
		.type = IIO_LIGHT,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW),
		.scan_index = -1,
	}
};

static unsigned long shub_allchannel_scan_masks[] = {
	/* timestamp channel is not managed by scan mask */
	BIT(SHUB_SCAN_ID) | BIT(SHUB_SCAN_RAW_0) |
	BIT(SHUB_SCAN_RAW_1) | BIT(SHUB_SCAN_RAW_2),
	0x00000000
};

static const struct iio_info shub_iio_info = {
	.read_raw = &shub_read_raw,
	.driver_module = THIS_MODULE,
};

static int shub_config_kfifo(struct iio_dev *iio_dev)
{
	struct iio_buffer *buffer;

	buffer = devm_iio_kfifo_allocate(&iio_dev->dev);
	if (!buffer)
		return -ENOMEM;

	iio_device_attach_buffer(iio_dev, buffer);
	buffer->scan_timestamp = true;
	/* set scan mask OR set iio:device1/scan_elements/XXX_en */
	buffer->scan_mask = shub_allchannel_scan_masks;
	iio_dev->buffer = buffer;
	iio_dev->setup_ops = &shub_iio_buffer_setup_ops;
	iio_dev->modes |= INDIO_BUFFER_TRIGGERED;

	return 0;
}

static int create_sysfs_interfaces(struct shub_data *mcu_data)
{
	int ret;

	mcu_data->sensor_class = class_create(THIS_MODULE, "sprd_sensorhub");
	if (IS_ERR(mcu_data->sensor_class))
		return PTR_ERR(mcu_data->sensor_class);

	mcu_data->sensor_dev =
	    device_create(mcu_data->sensor_class, NULL, 0, "%s", "sensor_hub");
	if (IS_ERR(mcu_data->sensor_dev)) {
		ret = PTR_ERR(mcu_data->sensor_dev);
		goto err_device_create;
	}
	debugfs_create_symlink("sensor", NULL,
			       "/sys/class/sprd_sensorhub/sensor_hub");

	dev_set_drvdata(mcu_data->sensor_dev, mcu_data);

	ret = sysfs_create_groups(&mcu_data->sensor_dev->kobj,
				  sensorhub_groups);
	if (ret) {
		dev_err(mcu_data->sensor_dev, "failed to create sysfs device attributes\n");
		goto error;
	}

	ret = sysfs_create_link(&mcu_data->sensor_dev->kobj,
				&mcu_data->indio_dev->dev.kobj, "iio");
	if (ret < 0)
		goto error;

	return 0;

error:
	put_device(mcu_data->sensor_dev);
	device_unregister(mcu_data->sensor_dev);
err_device_create:
	class_destroy(mcu_data->sensor_class);
	return ret;
}

static void shub_remove_trigger(struct iio_dev *indio_dev)
{
	struct shub_data *mcu_data = iio_priv(indio_dev);

	iio_trigger_unregister(mcu_data->trig);
	iio_trigger_free(mcu_data->trig);
	iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static void shub_remove_buffer(struct iio_dev *indio_dev)
{
	iio_kfifo_free(indio_dev->buffer);
}

static void shub_config_init(struct shub_data *sensor)
{
	sensor->is_sensorhub = 1;
}

static int shub_probe(struct platform_device *pdev)
{
	struct shub_data *mcu;
	struct device_node *np = pdev->dev.of_node;
	struct iio_dev *indio_dev;
	int error;

	indio_dev = iio_device_alloc(sizeof(*mcu));
	if (!indio_dev) {
		dev_err(&pdev->dev, " iio_device_alloc failed\n");
		return -ENOMEM;
	}

	indio_dev->name = SHUB_NAME;
	indio_dev->dev.parent = &pdev->dev;
	indio_dev->info = &shub_iio_info;
	indio_dev->channels = shub_channels;
	indio_dev->num_channels = ARRAY_SIZE(shub_channels);

	mcu = iio_priv(indio_dev);
	mcu->sensor_pdev = pdev;
	mcu->indio_dev = indio_dev;
	g_sensor = mcu;

	error = of_property_read_u32(pdev->dev.of_node,
		"sipc_sensorhub_id", &(mcu->sipc_sensorhub_id));
	if (error) {
		mcu->sipc_sensorhub_id = SIPC_ID_PM_SYS;
	}
	dev_info(&pdev->dev, "sipc_sensorhub_id=%u\n", mcu->sipc_sensorhub_id);

	mcu->mcu_mode = SHUB_BOOT;
	dev_set_drvdata(&pdev->dev, mcu);

	mcu->data_callback = shub_data_callback;
	mcu->save_mag_offset = shub_save_mag_offset;
	mcu->readcmd_callback = shub_readcmd_callback;
	mcu->cm4_read_callback = shub_cm4_read_callback;
	mcu->dynamic_read = get_dynamic_data;
	init_waitqueue_head(&mcu->rxwq);

	mcu->als_cali_cmd = CALIB_NONE;
	mcu->als_cali_result = CALI_ERR_NONE;
	mcu->caliRunning = false;
	mcu->resp_cmdstatus_callback = parse_cmd_response_callback;
	init_waitqueue_head(&mcu->rw_wait_queue);

	mutex_init(&mcu->mutex_lock);
	mutex_init(&mcu->mutex_read);
	mutex_init(&mcu->mutex_send);
	mutex_init(&mcu->send_command_mutex);
	mutex_init(&mcu->mutex_alsraw_lock);

	error = shub_config_kfifo(indio_dev);
	if (error) {
		dev_err(&pdev->dev, " shub_config_kfifo failed\n");
		goto error_free_dev;
	}

	error = shub_probe_trigger(indio_dev);
	if (error) {
		dev_err(&pdev->dev,
			" shub_probe_trigger failed\n");
		goto error_remove_buffer;
	}
	error = devm_iio_device_register(&pdev->dev, indio_dev);
	if (error) {
		dev_err(&pdev->dev, "iio_device_register failed\n");
		goto error_remove_trigger;
	}

	error = create_sysfs_interfaces(mcu);
	if (error)
		goto err_free_mem;

	INIT_WORK(&mcu->download_cali_data_work,
		shub_download_calibration_data_work);
	INIT_WORK(&mcu->savecalifile_work, shub_save_calibration_data);
	wakeup_source_init(&sensorhub_wake_lock, "sensorhub_wake_lock");
	/* init time sync and download firmware work */
	INIT_DELAYED_WORK(&mcu->time_sync_work, shub_synctime_work);
	mcu->driver_wq = create_singlethread_workqueue("sensorhub_daemon");
	if (!mcu->driver_wq) {
		error = -ENOMEM;
		goto err_free_mem;
	}

	init_irq_work(&mcu->iio_irq_work, iio_trigger_work);

	shub_config_init(mcu);

	thread = kthread_run(shub_read_thread, mcu, SHUB_RD);
	if (IS_ERR(thread)) {
		error = PTR_ERR(thread);
		dev_warn(&pdev->dev,
			 "failed to create kernel thread: %d\n", error);
	}

	thread_nwu = kthread_run(shub_read_thread_nwu, mcu, SHUB_RD_NWU);
	if (IS_ERR(thread_nwu)) {
		error = PTR_ERR(thread_nwu);
		dev_warn(&pdev->dev,
			 "failed to create kernel thread_nwu: %d\n", error);
	}
	sbuf_set_no_need_wake_lock(mcu->sipc_sensorhub_id,
				   SMSG_CH_PIPE, SIPC_PM_BUFID1);
	mcu->early_suspend.notifier_call = shub_notifier_fn;
	register_pm_notifier(&mcu->early_suspend);
	mcu->vled_pin = of_get_named_gpio(np, "sprd,prox-irvdd-gpio", 0);
	if (!gpio_is_valid(mcu->vled_pin)) {
		dev_err(&pdev->dev, "Can not get Proximity ir led vdd gpio!!\n");
	} else {
		error = gpio_request(mcu->vled_pin, "vled");
		if (error)
			dev_err(&pdev->dev, "gpio vled err: %d\n", mcu->vled_pin);		
	}
	
	gpio_direction_output(mcu->vled_pin, 1);
	return 0;

err_free_mem:
	iio_device_unregister(indio_dev);
error_remove_trigger:
	shub_remove_trigger(indio_dev);
error_remove_buffer:
	shub_remove_buffer(indio_dev);
error_free_dev:
	iio_device_free(indio_dev);

	return error;
}

static int shub_remove(struct platform_device *pdev)
{
	struct shub_data *mcu = platform_get_drvdata(pdev);
	struct iio_dev *indio_dev = mcu->indio_dev;

	unregister_pm_notifier(&mcu->early_suspend);
	if (!IS_ERR_OR_NULL(thread))
		kthread_stop(thread);
	if (!IS_ERR_OR_NULL(thread_nwu))
		kthread_stop(thread_nwu);
	if (!IS_ERR_OR_NULL(thread_cali)) {
		if(mcu->caliRunning) {
			kthread_stop(thread_cali);
		}
	}
	
	if(backlight_notifier_registered){
		backlight_level_notifier_unregister(&backlight_nb);
	}
	
	if (gpio_is_valid(mcu->vled_pin)) {
		gpio_free(mcu->vled_pin);
	}
	iio_device_unregister(indio_dev);
	shub_remove_trigger(indio_dev);
	shub_remove_buffer(indio_dev);
	iio_device_free(indio_dev);
	kfree(mcu);
	return 0;
}

static const struct of_device_id shub_match_table[] = {
	{.compatible = "sprd,sensor-hub",},
	{.compatible = "sprd,sharkl3-sensorhub",},
	{.compatible = "sprd,sharkl5-sensorhub",},
	{.compatible = "sprd,roc1-sensorhub",},
	{.compatible = "sprd,sharkl5Pro-sensorhub",},
	{},
};

static struct platform_driver shub_driver = {
	.probe = shub_probe,
	.remove = shub_remove,
	.driver = {
		   .name = SHUB_NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = shub_match_table,
	},
};

module_platform_driver(shub_driver);

MODULE_DESCRIPTION("Spreadtrum Sensor Hub");
MODULE_AUTHOR("Bao Yue <bao.yue@spreadtrum.com>");
MODULE_LICENSE("GPL");
