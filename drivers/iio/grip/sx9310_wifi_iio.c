/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/sensor/sensors_core.h>
#include <linux/power_supply.h>
#include "sx9310_reg.h"

#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#elif defined(CONFIG_MUIC_NOTI)
#include <linux/i2c/sm5703-muic.h>
#include <linux/i2c/muic_notifier.h>
#endif
#include <linux/iio/iio.h>
#include <linux/iio/buffer.h>
#include <linux/iio/kfifo_buf.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/events.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>

#define VENDOR_NAME              "SEMTECH"
#define MODEL_NAME               "SX9310_WIFI"
#define MODULE_NAME              "grip_sensor_wifi"
#define SX9310_GRIP_SENSOR_NAME	 "sx9310_grip_wifi"
#define CALIBRATION_FILE_PATH    "/efs/FactoryApp/grip_wifi_cal_data"

#define I2C_M_WR                 0 /* for i2c Write */
#define I2c_M_RD                 1 /* for i2c Read */

#define IDLE                     0
#define ACTIVE                   1

#define SX9310_MODE_SLEEP        0
#define SX9310_MODE_NORMAL       1

#define MAIN_SENSOR              1
#define REF_SENSOR               2
#define DIFF_READ_NUM            10
#define GRIP_LOG_TIME            30 /* sec */
#define CSX_STATUS_REG           SX9310_TCHCMPSTAT_TCHSTAT0_FLAG
#define BODY_STATUS_REG		SX9310_BODYCMPSTAT_FLAG
#define RAW_DATA_BLOCK_SIZE      (SX9310_REGOFFSETLSB - SX9310_REGUSEMSB + 1)

/* CS0, CS1, CS2, CS3 */
#define ENABLE_CSX               (1 << MAIN_SENSOR)
#define IRQ_PROCESS_CONDITION   (SX9310_IRQSTAT_TOUCH_FLAG \
				| SX9310_IRQSTAT_RELEASE_FLAG)

#define NONE_ENABLE		-1
#define IDLE_STATE		0
#define TOUCH_STATE		1
#define BODY_STATE		2

#ifdef CONFIG_SENSORS_SX9310_USE_CERTIFY_HALL
#define HALLIC1_PATH		"/sys/class/sec/sec_key/certify_hall_detect"
#endif
#define HALLIC2_PATH		"/sys/class/sec/sec_key/hall_detect"

#ifdef CONFIG_SENSORS_SX9310_WIFI_DEFENCE_CODE_FOR_TA_NOISE
#define SX9310_NORMAL_TOUCH_CABLE_THRESHOLD	232
#endif

struct sx9310_p {
	struct i2c_client *client;
	struct device *factory_device;
	struct delayed_work init_work;
	struct delayed_work irq_work;
	struct delayed_work debug_work;
	struct wake_lock grip_wake_lock;
	struct mutex read_mutex;
#if defined(CONFIG_MUIC_NOTIFIER) || defined(CONFIG_MUIC_NOTI)
	struct notifier_block cpuidle_muic_nb;
#endif
	bool skip_data;
	bool check_usb;
	u8 normal_th;
	u8 normal_th_buf;
#ifdef CONFIG_SENSORS_SX9310_WIFI_DEFENCE_CODE_FOR_TA_NOISE
	u8 cable_norm_th;
#endif
	int irq;
	int gpio_nirq;
	int state;
	int debug_count;
	int diff_avg;
	int diff_cnt;
	int init_done;
	s32 capmain;
	s16 useful;
	s16 avg;
	s16 diff;
	u16 offset;
	u16 freq;
	bool reset_flag;

	int ch1_state;
	int ch2_state;

	atomic_t enable;

	/* iio variables */
	struct iio_trigger *trig;
	struct iio_dev *indio_dev;
	struct mutex lock;
	spinlock_t spin_lock;
	int iio_state;
	atomic_t pseudo_irq_enable;

#ifdef CONFIG_SENSORS_SX9310_USE_CERTIFY_HALL
	unsigned char hall_ic1[5];
#endif
	unsigned char hall_ic2[5];
};

struct sx9310_iio_data {
	struct sx9310_p *cdata;
};

enum {
	SX9310_SCAN_GRIP_CH,
	SX9310_SCAN_GRIP_TIMESTAMP,
};

#define SX9310_GRIP_INFO_SHARED_MASK	(BIT(IIO_CHAN_INFO_SCALE))
#define SX9310_GRIP_INFO_SEPARATE_MASK	(BIT(IIO_CHAN_INFO_RAW))

static const struct iio_chan_spec sx9310_channels[] = {
	{
		.type = IIO_GRIP_WIFI,
		.modified = 1,
		.channel2 = IIO_MOD_GRIP_WIFI,
		.info_mask_separate = SX9310_GRIP_INFO_SEPARATE_MASK,
		.info_mask_shared_by_type = SX9310_GRIP_INFO_SHARED_MASK,
		.scan_index = SX9310_SCAN_GRIP_CH,
		.scan_type = IIO_ST('s', 32, 32, 0),
	},
	IIO_CHAN_SOFT_TIMESTAMP(SX9310_SCAN_GRIP_TIMESTAMP),
};

#ifdef CONFIG_SENSORS_SX9310_WIFI_DEFENCE_CODE_FOR_TA_NOISE
#include <linux/power_supply.h>

static int check_ta_state(void)
{
	static struct power_supply *psy = NULL;
	union power_supply_propval ret = {0,};

	if (psy == NULL) {
		psy = power_supply_get_by_name("battery");
		if (psy == NULL) {
			SENSOR_ERR("[SX9310_W] failed to get ps battery\n");
			return -EINVAL;
		}
	}

	psy->get_property(psy, POWER_SUPPLY_PROP_ONLINE, &ret);

	return ret.intval;
}
#endif

static int check_hallic_state(char *file_path, unsigned char hall_ic_status[])
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *filep;
	u8 hall_sysfs[5];

	memset(hall_sysfs,0, sizeof(hall_sysfs));

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filep = filp_open(file_path, O_RDONLY, 0666);
	if (IS_ERR(filep)) {
		iRet = PTR_ERR(filep);
		if (iRet != -ENOENT)
			SENSOR_ERR("[SX9310_W] file open fail [%s] %d\n",
				file_path, iRet);
		set_fs(old_fs);
		goto exit;
		}

	iRet = filep->f_op->read(filep, (char *)&hall_sysfs,
		sizeof(hall_sysfs), &filep->f_pos);


	if (!iRet) {
		SENSOR_ERR("[SX9310_W] failed : iRet(%d), size(%d), hall= %s\n",
			iRet, (int)sizeof(hall_sysfs), hall_sysfs);

		iRet = -EIO;
	} else {
		strncpy(hall_ic_status, hall_sysfs, sizeof(hall_sysfs));
	}

/*	SENSOR_INFO(" info : iRet(%d), size(%d), hall= %s\n",
		iRet, (int)sizeof(hall_sysfs), hall_sysfs);*/

	filp_close(filep, current->files);
	set_fs(old_fs);

	exit:
	return iRet;
}

static int sx9310_get_nirq_state(struct sx9310_p *data)
{
	return gpio_get_value_cansleep(data->gpio_nirq);
}

static int sx9310_i2c_write(struct sx9310_p *data, u8 reg_addr, u8 buf)
{
	int ret;
	struct i2c_msg msg;
	unsigned char w_buf[2];

	w_buf[0] = reg_addr;
	w_buf[1] = buf;

	msg.addr = data->client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 2;
	msg.buf = (char *)w_buf;

	ret = i2c_transfer(data->client->adapter, &msg, 1);
	if (ret < 0)
		SENSOR_ERR("[SX9310_W] i2c write error %d\n", ret);

	return ret;
}

static int sx9310_i2c_read(struct sx9310_p *data, u8 reg_addr, u8 *buf)
{
	int ret;
	struct i2c_msg msg[2];

	msg[0].addr = data->client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = data->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	ret = i2c_transfer(data->client->adapter, msg, 2);
	if (ret < 0)
		SENSOR_ERR("[SX9310_W] i2c read error %d\n", ret);

	return ret;
}

static int sx9310_i2c_read_block(struct sx9310_p *data, u8 reg_addr,
	u8 *buf, u8 buf_size)
{
      int ret;
      struct i2c_msg msg[2];

      msg[0].addr = data->client->addr;
      msg[0].flags = I2C_M_WR;
      msg[0].len = 1;
      msg[0].buf = &reg_addr;

      msg[1].addr = data->client->addr;
      msg[1].flags = I2C_M_RD;
      msg[1].len = buf_size;
      msg[1].buf = buf;

      ret = i2c_transfer(data->client->adapter, msg, 2);
      if (ret < 0)
		SENSOR_ERR("[SX9310_W] i2c read error %d\n", ret);

      return ret;
}

static u8 sx9310_read_irqstate(struct sx9310_p *data)
{
	u8 val = 0;

	if (sx9310_i2c_read(data, SX9310_IRQSTAT_REG, &val) >= 0)
		return val;

	return 0;
}

static void sx9310_initialize_register(struct sx9310_p *data)
{
	u8 val = 0;
	int idx;

	for (idx = 0; idx < (sizeof(setup_reg) / 2); idx++) {
		sx9310_i2c_write(data, setup_reg[idx].reg, setup_reg[idx].val);
		SENSOR_INFO("[SX9310_W] Write Reg: 0x%x Value: 0x%x\n",
			setup_reg[idx].reg, setup_reg[idx].val);

		sx9310_i2c_read(data, setup_reg[idx].reg, &val);
		SENSOR_INFO("[SX9310_W] Read Reg: 0x%x Value: 0x%x\n\n",
			setup_reg[idx].reg, val);
	}
	data->init_done = ON;
}

static void sx9310_initialize_chip(struct sx9310_p *data)
{
	int cnt = 0;

	while ((sx9310_get_nirq_state(data) == 0) && (cnt++ < 10)) {
		sx9310_read_irqstate(data);
		msleep(20);
	}

	if (cnt >= 10)
		SENSOR_ERR("[SX9310_W] s/w reset fail(%d)\n", cnt);

	sx9310_initialize_register(data);
}

static int sx9310_set_offset_calibration(struct sx9310_p *data)
{
	int ret = 0;

	ret = sx9310_i2c_write(data, SX9310_IRQSTAT_REG, 0xFF);

	return ret;
}

static inline s64 sx9310_iio_get_boottime_ns(void)
{
	struct timespec ts;

	ts = ktime_to_timespec(ktime_get_boottime());

	return timespec_to_ns(&ts);
}

static int sx9310_grip_data_rdy_trig_poll(struct sx9310_p *data)
{
	unsigned long flags;

	spin_lock_irqsave(&data->spin_lock, flags);
	iio_trigger_poll(data->trig, sx9310_iio_get_boottime_ns());
	spin_unlock_irqrestore(&data->spin_lock, flags);

	return 0;
}

static void send_event(struct sx9310_p *data, u8 state)
{
	data->normal_th = data->normal_th_buf;

#ifdef CONFIG_SENSORS_SX9310_WIFI_DEFENCE_CODE_FOR_TA_NOISE
	if (check_ta_state() > 1) {
		if (data->cable_norm_th == 0)
		data->normal_th = SX9310_NORMAL_TOUCH_CABLE_THRESHOLD;
		else
			data->normal_th = data->cable_norm_th;
		SENSOR_INFO("[SX9310_W] TA cable connected\n");
	}
#endif

	if (state == ACTIVE) {
		data->state = ACTIVE;

#if (MAIN_SENSOR == 1)
		sx9310_i2c_write(data, SX9310_CPS_CTRL9_REG, data->normal_th);
#else
		sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, data->normal_th);
#endif
		SENSOR_INFO("[SX9310_W] button touched\n");

	} else {
		data->state = IDLE;

#if (MAIN_SENSOR == 1)
		sx9310_i2c_write(data, SX9310_CPS_CTRL9_REG, data->normal_th);
#else
		sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, data->normal_th);
#endif
		SENSOR_INFO("[SX9310_W] button released\n");

	}
	if (data->skip_data == true)
		return;

	if (state == ACTIVE) {
		data->iio_state = ACTIVE;
		sx9310_grip_data_rdy_trig_poll(data);
	} else {
		data->iio_state = IDLE;
		sx9310_grip_data_rdy_trig_poll(data);
	}
}

static void sx9310_display_data_reg(struct sx9310_p *data)
{
	u8 val, reg;

	sx9310_i2c_write(data, SX9310_REGSENSORSELECT, MAIN_SENSOR);
	for (reg = SX9310_REGUSEMSB; reg <= SX9310_REGOFFSETLSB; reg++) {
		sx9310_i2c_read(data, reg, &val);
		SENSOR_INFO("[SX9310_W] Register(0x%2x) data(0x%2x)\n", reg, val);
	}
}

static void sx9310_get_data(struct sx9310_p *data)
{
	u8 ms_byte = 0;
	u8 is_byte = 0;
	u8 ls_byte = 0;

	u8 buf[RAW_DATA_BLOCK_SIZE];

#if (MAIN_SENSOR == 0)
	s32 gain = 1 << ((setup_reg[SX9310_CPS_CTRL3].val >> 2) & 0x03);
#else
	s32 gain = 1 << (setup_reg[SX9310_CPS_CTRL3].val & 0x03);
#endif

	mutex_lock(&data->read_mutex);

	sx9310_i2c_write(data, SX9310_REGSENSORSELECT, MAIN_SENSOR);
	sx9310_i2c_read_block(data, SX9310_REGUSEMSB,
		&buf[0], RAW_DATA_BLOCK_SIZE);

	data->useful = (s16)((s32)buf[0] << 8) | ((s32)buf[1]);
	data->avg = (s16)((s32)buf[2] << 8) | ((s32)buf[3]);
	data->offset = ((u16)buf[6] << 8) | ((u16)buf[7]);
	data->diff = (data->useful - data->avg) >> 4;

	ms_byte = (u8)(data->offset >> 13);
	is_byte = (u8)((data->offset & 0x1fc0) >> 6);
	ls_byte = (u8)(data->offset & 0x3f);

	data->capmain = (((s32)ms_byte * 234000) + ((s32)is_byte * 9000) +
		((s32)ls_byte * 450)) + (((s32)data->useful * 50000) /
		(gain * 65536));

	mutex_unlock(&data->read_mutex);
	SENSOR_INFO("[SX9310_W] Capmain: %d, Useful: %d, avg: %d, diff: %d, Offset: %u\n",
		data->capmain, data->useful, data->avg, data->diff, data->offset);
}


static int sx9310_set_mode(struct sx9310_p *data, unsigned char mode)
{
	int ret = -EINVAL;

	if (mode == SX9310_MODE_SLEEP) {
		ret = sx9310_i2c_write(data, SX9310_CPS_CTRL0_REG,
			setup_reg[SX9310_CPS_CTRL0].val);
	} else if (mode == SX9310_MODE_NORMAL) {
		ret = sx9310_i2c_write(data, SX9310_CPS_CTRL0_REG,
			setup_reg[SX9310_CPS_CTRL0].val | ENABLE_CSX);
		msleep(20);

		sx9310_set_offset_calibration(data);
	}

	SENSOR_INFO("[SX9310_W] change the mode : %u\n", mode);
	return ret;
}

static void sx9310_ch_interrupt_read(struct sx9310_p *data, u8 status)
{
	if (status & (CSX_STATUS_REG << MAIN_SENSOR)) {
		if (status & (BODY_STATUS_REG << (MAIN_SENSOR+1)))
			data->ch1_state = BODY_STATE;
		else
			data->ch1_state = TOUCH_STATE;
	} else
		data->ch1_state = IDLE_STATE;

	SENSOR_INFO("[SX9310_W] ch1:%d, ch2:%d\n",
		data->ch1_state, data->ch2_state);
}
static void sx9310_set_enable(struct sx9310_p *data, int enable)
{
	u8 status = 0;

	SENSOR_INFO("[SX9310_W]\n");

	atomic_set(&data->enable, enable);
	if (enable == ON) {
		sx9310_i2c_read(data, SX9310_STAT0_REG, &status);
		SENSOR_INFO("[SX9310_W](status : 0x%x)\n", status);

		sx9310_ch_interrupt_read(data, status);

		data->diff_avg = 0;
		data->diff_cnt = 0;
		sx9310_get_data(data);

		if (data->skip_data == true) {
			data->iio_state = IDLE;
			sx9310_grip_data_rdy_trig_poll(data);
		} else if ((status & (CSX_STATUS_REG << MAIN_SENSOR))
				&& (data->reset_flag == false)) {
			send_event(data, ACTIVE);
		} else {
			send_event(data, IDLE);
		}

		msleep(20);
		/* make sure no interrupts are pending since enabling irq
		 * will only work on next falling edge */
		sx9310_read_irqstate(data);

		/* enable interrupt */
		sx9310_i2c_write(data, SX9310_IRQ_ENABLE_REG,
				setup_reg[SX9310_IRQ_ENABLE].val);

		enable_irq(data->irq);
		enable_irq_wake(data->irq);
	} else {
		SENSOR_INFO("[SX9310_W]disable\n");

		/* disable interrupt */
		sx9310_i2c_write(data, SX9310_IRQ_ENABLE_REG,0x00);
		disable_irq(data->irq);
		disable_irq_wake(data->irq);
	}

	data->reset_flag = false;
}

static void sx9310_set_debug_work(struct sx9310_p *data, u8 enable,
		unsigned int time_ms)
{
	if (enable == ON) {
		data->debug_count = 0;
		schedule_delayed_work(&data->debug_work,
			msecs_to_jiffies(time_ms));
	} else {
		cancel_delayed_work_sync(&data->debug_work);
	}
}

static ssize_t sx9310_get_offset_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	struct sx9310_p *data = dev_get_drvdata(dev);

	sx9310_i2c_read(data, SX9310_IRQSTAT_REG, &val);

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t sx9310_set_offset_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &val)) {
		SENSOR_ERR("[SX9310_W] Invalid Argument\n");
		return -EINVAL;
	}

	if (val)
		sx9310_set_offset_calibration(data);

	return count;
}

static ssize_t sx9310_register_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int regist = 0, val = 0;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%d,%d", &regist, &val) != 2) {
		SENSOR_ERR("[SX9310_W] The number of data are wrong\n");
		return -EINVAL;
	}

	sx9310_i2c_write(data, (unsigned char)regist, (unsigned char)val);
	SENSOR_INFO("[SX9310_W] Register(0x%2x) data(0x%2x)\n", regist, val);

	return count;
}

static ssize_t sx9310_register_read_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char val[30], i;
	struct sx9310_p *data = dev_get_drvdata(dev);

	for (i = 0; i < 19; i++) {
		sx9310_i2c_read(data, i + 0x10, &val[i]);
		SENSOR_INFO("[SX9310_W] Register(0x%2x) data(0x%2x)\n",
			i + 0x10, val[i]);
	}

	for (i = 26; i < 29; i++) {
		sx9310_i2c_read(data, i + 0x10, &val[i]);
		SENSOR_INFO("[SX9310_W] Register(0x%2x) data(0x%2x)\n",
		i + 0x10, val[i]);
	}
	return snprintf(buf, PAGE_SIZE,
		"0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
		val[0], val[1], val[2], val[3], val[4],
		val[5], val[6], val[7], val[8], val[9]);
}


static ssize_t sx9310_read_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	sx9310_display_data_reg(data);

	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t sx9310_sw_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);
	SENSOR_INFO("[SX9310_W] sw reset start\n");

	data->reset_flag = true;
	sx9310_set_offset_calibration(data);
	msleep(400);
	sx9310_get_data(data);

	SENSOR_INFO("[SX9310_W] sw reset end\n");

	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}


static ssize_t sx9310_freq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &val)) {
		SENSOR_ERR("[SX9310_W] Invalid Argument\n");
		return count;
	}

	data->freq = (u16)val;
	val = ((val << 3) | (setup_reg[SX9310_CPS_CTRL4].val & 0x07)) & 0xff;
	sx9310_i2c_write(data, SX9310_CPS_CTRL4_REG, (u8)val);

	SENSOR_INFO("[SX9310_W] Freq : 0x%x\n", data->freq);

	return count;
}

static ssize_t sx9310_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	SENSOR_INFO("[SX9310_W] Freq : 0x%x\n", data->freq);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->freq);
}

static ssize_t sx9310_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t sx9310_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t sx9310_touch_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1\n");
}

static ssize_t sx9310_raw_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int sum;
	struct sx9310_p *data = dev_get_drvdata(dev);

	sx9310_get_data(data);
	if (data->diff_cnt == 0)
		sum = data->diff;
	else
		sum += data->diff;

	if (++data->diff_cnt >= DIFF_READ_NUM) {
		data->diff_avg = sum / DIFF_READ_NUM;
		data->diff_cnt = 0;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%u,%d,%d\n", data->capmain,
		data->useful, data->offset, data->diff, data->avg);
}

static ssize_t sx9310_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* It's for init touch */
	return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t sx9310_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t sx9310_normal_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);
	u16 thresh_temp = 0, hysteresis = 0;
	u16 thresh_table[32] = {2, 4, 6, 8, 12, 16, 20, 24, 28, 32,
				40, 48, 56, 64, 72, 80, 88, 96, 112, 128,
				144, 160, 192, 224, 256, 320, 384, 512, 640,
				768, 1024, 1536};

	thresh_temp = (data->normal_th >> 3) & 0x1f;
	thresh_temp = thresh_table[thresh_temp];

	/* CTRL10 */
	hysteresis = (setup_reg[SX9310_CPS_CTRL10].val >> 4) & 0x03;

	switch (hysteresis) {
	case 0x01: /* 6% */
		hysteresis = thresh_temp >> 4;
		break;
	case 0x02: /* 12% */
		hysteresis = thresh_temp >> 3;
		break;
	case 0x03: /* 25% */
		hysteresis = thresh_temp >> 2;
		break;
	default:
		/* None */
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", thresh_temp + hysteresis,
			thresh_temp - hysteresis);
}

static ssize_t sx9310_normal_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	/* It's for normal touch */
	if (kstrtoul(buf, 10, &val)) {
		SENSOR_ERR("[SX9310_W] Invalid Argument\n");
		return -EINVAL;
	}
	data->normal_th &= 0x07;
	data->normal_th |= val;

	SENSOR_INFO("[SX9310_W] normal threshold %lu\n", val);
	data->normal_th_buf = data->normal_th = (u8)(val);

	return count;
}

static ssize_t sx9310_onoff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", !data->skip_data);
}

static ssize_t sx9310_onoff_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct sx9310_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		SENSOR_ERR("[SX9310_W] Invalid Argument\n");
		return ret;
	}

	if (val == 0) {
		data->skip_data = true;
		data->state = IDLE;
		data->iio_state = IDLE;
		if (atomic_read(&data->enable) == ON) {
			sx9310_grip_data_rdy_trig_poll(data);
		}
	} else {
		data->skip_data = false;
	}

	SENSOR_INFO("[SX9310_W]%u\n", val);
	return count;
}

static ssize_t sx9310_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "2,0,0\n");
}

static ssize_t sx9310_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t sx9310_gain_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
#if (MAIN_SENSOR == 0)
	u8 gain = (setup_reg[SX9310_CPS_CTRL3].val >> 2) & 0x03;
#else
	u8 gain = setup_reg[SX9310_CPS_CTRL3].val & 0x03;
#endif

	switch (gain) {
	case 0x00:
		ret = snprintf(buf, PAGE_SIZE, "x1\n");
		break;
	case 0x01:
		ret = snprintf(buf, PAGE_SIZE, "x2\n");
		break;
	case 0x02:
		ret = snprintf(buf, PAGE_SIZE, "x4\n");
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "x8\n");
		break;
	}

	return ret;
}

static ssize_t sx9310_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	switch ((setup_reg[SX9310_CPS_CTRL5].val >> 6) & 0x03) {
	case 0x00:
		ret = snprintf(buf, PAGE_SIZE, "Large\n");
		break;
	case 0x01:
		ret = snprintf(buf, PAGE_SIZE, "Medium\n");
		break;
	case 0x02:
		ret = snprintf(buf, PAGE_SIZE, "Medium Small\n");
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "Small\n");
		break;
	}

	return ret;
}

static ssize_t sx9310_diff_avg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->diff_avg);
}


static ssize_t sx9310_ch_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (data->skip_data == true) {
		ret = snprintf(buf, PAGE_SIZE, "%d,%d\n",
			NONE_ENABLE, NONE_ENABLE);
	} else if (atomic_read(&data->enable) == ON) {
		ret = snprintf(buf, PAGE_SIZE, "%d,%d\n",
			data->ch1_state, data->ch2_state);
	} else {
		ret = snprintf(buf, PAGE_SIZE, "%d,%d\n",
			NONE_ENABLE, NONE_ENABLE);
	}

	return ret;
	}

static ssize_t sx9310_body_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);
	u16 thresh_temp = 0, hysteresis = 0;
	u16 thresh_table[8] = {0, 300, 600, 900, 1200, 1500, 1800, 30000};

	thresh_temp = (data->normal_th) & 0x07;
	thresh_temp = thresh_table[thresh_temp];

	/* CTRL10 */
	hysteresis = (setup_reg[SX9310_CPS_CTRL10].val >> 4) & 0x3;

	switch (hysteresis) {
	case 0x01: /* 6% */
		hysteresis = thresh_temp >> 4;
		break;
	case 0x02: /* 12% */
		hysteresis = thresh_temp >> 3;
		break;
	case 0x03: /* 25% */
		hysteresis = thresh_temp >> 2;
		break;
	default:
		/* None */
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", thresh_temp + hysteresis,
			thresh_temp - hysteresis);
	}

static ssize_t sx9310_body_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &val)) {
		SENSOR_ERR("[SX9310_W] Invalid Argument\n");
		return -EINVAL;
	}

	data->normal_th &= 0xf8;
	data->normal_th |= val;

	SENSOR_INFO("[SX9310_W] body threshold %lu\n", val);
	data->normal_th_buf = data->normal_th = (u8)(val);

	return count;
}
static DEVICE_ATTR(menual_calibrate, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_get_offset_calibration_show,
		sx9310_set_offset_calibration_store);
static DEVICE_ATTR(register_write, S_IWUSR | S_IWGRP,
		NULL, sx9310_register_write_store);
static DEVICE_ATTR(register_read, S_IRUGO,
		sx9310_register_read_show, NULL);
static DEVICE_ATTR(readback, S_IRUGO, sx9310_read_data_show, NULL);
static DEVICE_ATTR(reset, S_IRUGO, sx9310_sw_reset_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, sx9310_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, sx9310_vendor_show, NULL);
static DEVICE_ATTR(gain, S_IRUGO, sx9310_gain_show, NULL);
static DEVICE_ATTR(range, S_IRUGO, sx9310_range_show, NULL);
static DEVICE_ATTR(mode, S_IRUGO, sx9310_touch_mode_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, sx9310_raw_data_show, NULL);
static DEVICE_ATTR(diff_avg, S_IRUGO, sx9310_diff_avg_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_calibration_show, sx9310_calibration_store);
static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_onoff_show, sx9310_onoff_store);
static DEVICE_ATTR(threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_threshold_show, sx9310_threshold_store);
static DEVICE_ATTR(normal_threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_normal_threshold_show, sx9310_normal_threshold_store);
static DEVICE_ATTR(freq, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_freq_show, sx9310_freq_store);
static DEVICE_ATTR(ch_state, S_IRUGO, sx9310_ch_state_show, NULL);
static DEVICE_ATTR(body_threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_body_threshold_show, sx9310_body_threshold_store);


static struct device_attribute *sensor_attrs[] = {
	&dev_attr_menual_calibrate,
	&dev_attr_register_write,
	&dev_attr_register_read,
	&dev_attr_readback,
	&dev_attr_reset,
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_gain,
	&dev_attr_range,
	&dev_attr_mode,
	&dev_attr_diff_avg,
	&dev_attr_raw_data,
	&dev_attr_threshold,
	&dev_attr_normal_threshold,
	&dev_attr_onoff,
	&dev_attr_calibration,
	&dev_attr_freq,
	&dev_attr_ch_state,
	&dev_attr_body_threshold,
	NULL,
};

static void sx9310_touch_process(struct sx9310_p *data, u8 flag)
{
	u8 status = 0;

	sx9310_i2c_read(data, SX9310_STAT0_REG, &status);
	SENSOR_INFO("[SX9310_W] (status: 0x%x)\n", status);
	sx9310_get_data(data);
	sx9310_ch_interrupt_read(data, status);

	if (data->state == IDLE) {
		if (status & (CSX_STATUS_REG << MAIN_SENSOR))
			send_event(data, ACTIVE);
		else
			SENSOR_INFO("[SX9310_W] already released\n");
	} else {
		if (!(status & (CSX_STATUS_REG << MAIN_SENSOR)))
			send_event(data, IDLE);
		else
			SENSOR_INFO("[SX9310_W] still touched\n");
	}
}

static void sx9310_process_interrupt(struct sx9310_p *data)
{
	u8 flag = 0;

	/* since we are not in an interrupt don't need to disable irq. */
	flag = sx9310_read_irqstate(data);

	if (flag & IRQ_PROCESS_CONDITION)
		sx9310_touch_process(data, flag);
}

static void sx9310_init_work_func(struct work_struct *work)
{
	struct sx9310_p *data = container_of((struct delayed_work *)work,
		struct sx9310_p, init_work);

	sx9310_initialize_chip(data);

#if (MAIN_SENSOR == 1)
	sx9310_i2c_write(data, SX9310_CPS_CTRL9_REG, data->normal_th);
#else
	sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, data->normal_th);
#endif
	sx9310_set_mode(data, SX9310_MODE_NORMAL);
	/* make sure no interrupts are pending since enabling irq
	 * will only work on next falling edge */
	sx9310_read_irqstate(data);

	/* disable interrupt */
	sx9310_i2c_write(data, SX9310_IRQ_ENABLE_REG, 0x00);

}

static void sx9310_irq_work_func(struct work_struct *work)
{
	struct sx9310_p *data = container_of((struct delayed_work *)work,
		struct sx9310_p, irq_work);

	data->reset_flag = false;
	if (sx9310_get_nirq_state(data) == 0)
		sx9310_process_interrupt(data);
	else
		SENSOR_ERR("[SX9310_W] nirq read high %d\n",
			sx9310_get_nirq_state(data));
}

static void sx9310_debug_work_func(struct work_struct *work)
{
	struct sx9310_p *data = container_of((struct delayed_work *)work,
		struct sx9310_p, debug_work);
	int ret;
	static int hall_flag = 1;

	if (atomic_read(&data->enable) == ON) {
		if (data->debug_count >= GRIP_LOG_TIME) {
			sx9310_get_data(data);
			data->debug_count = 0;
		} else {
			data->debug_count++;
		}
	}

#ifdef CONFIG_SENSORS_SX9310_USE_CERTIFY_HALL
	ret = check_hallic_state(HALLIC1_PATH, data->hall_ic1);
	if (ret < 0) {
		SENSOR_ERR("[SX9310_W] hallic 1 detect fail = %d\n",
			ret);
	}
#endif

	ret = check_hallic_state(HALLIC2_PATH, data->hall_ic2);
	if (ret < 0) {
		SENSOR_ERR("[SX9310_W] hallic 2 detect fail = %d\n",
			ret);
	}

	/* Hall IC closed : offset cal (once)*/
#ifdef CONFIG_SENSORS_SX9310_USE_CERTIFY_HALL
	if (strcmp(data->hall_ic1, "CLOSE") == 0 &&
		strcmp(data->hall_ic2, "CLOSE") == 0) {
#else
	if (strcmp(data->hall_ic2, "CLOSE") == 0) {
#endif
		if (hall_flag) {
			SENSOR_INFO("[SX9310_W] hall IC1&2 is closed\n");
		sx9310_set_offset_calibration(data);
			hall_flag = 0;
			SENSOR_INFO("[SX9310_W] TA is removed\n");
		}
	} else
		hall_flag = 1;

	schedule_delayed_work(&data->debug_work, msecs_to_jiffies(1000));
}

static irqreturn_t sx9310_interrupt_thread(int irq, void *pdata)
{
	struct sx9310_p *data = pdata;

	if (sx9310_get_nirq_state(data) == 1) {
		SENSOR_ERR("[SX9310_W] nirq read high\n");
	} else {
		wake_lock_timeout(&data->grip_wake_lock, 3 * HZ);
		schedule_delayed_work(&data->irq_work, msecs_to_jiffies(100));
	}

	return IRQ_HANDLED;
}

static int sx9310_setup_pin(struct sx9310_p *data)
{
	int ret;

	ret = gpio_request(data->gpio_nirq, "SX9310_nIRQ");
	if (ret < 0) {
		SENSOR_ERR("[SX9310_W] gpio %d request failed (%d)\n",
			data->gpio_nirq, ret);
		return ret;
	}

	ret = gpio_direction_input(data->gpio_nirq);
	if (ret < 0) {
		SENSOR_ERR("[SX9310_W] failed to set gpio %d(%d)\n",
			data->gpio_nirq, ret);
		gpio_free(data->gpio_nirq);
		return ret;
	}

	return 0;
}

static void sx9310_initialize_variable(struct sx9310_p *data)
{
	data->state = IDLE;
	data->skip_data = false;
	data->check_usb = false;
	data->freq = setup_reg[SX9310_CPS_CTRL4].val >> 3;
	data->debug_count = 0;
	atomic_set(&data->enable, OFF);
	data->ch1_state = IDLE;
	data->init_done = OFF;
	data->reset_flag = false;
}

irqreturn_t sx9310_wifi_iio_pollfunc_store_boottime(int irq, void *p)
{
	struct iio_poll_func *pf = p;

	pf->timestamp = sx9310_iio_get_boottime_ns();

	return IRQ_WAKE_THREAD;
}

static irqreturn_t sx9310_trigger_handler(int irq, void *p)
{
	struct iio_poll_func *pf = p;
	struct iio_dev *indio_dev = pf->indio_dev;
	struct sx9310_iio_data *sdata = iio_priv(indio_dev);
	struct sx9310_p *data = sdata->cdata;

	int len = 0;
	int32_t *pdata;

	pdata = kmalloc(indio_dev->scan_bytes, GFP_KERNEL);
	if (pdata == NULL)
		goto done;
	if (!bitmap_empty(indio_dev->active_scan_mask, indio_dev->masklength)) {
		/* TODO : data update */
		if (data->iio_state)
			*pdata = 0;
		else
			*pdata = 1;
	}
	len = 4;
	/* Guaranteed to be aligned with 8 byte boundary */
	if (indio_dev->scan_timestamp)
		*(s64 *)((u8 *)pdata + ALIGN(len, sizeof(s64))) = pf->timestamp;
	iio_push_to_buffers(indio_dev, (u8 *)pdata);
	kfree(pdata);
done:
	iio_trigger_notify_done(indio_dev->trig);
	return IRQ_HANDLED;
}

static int sx9310_pseudo_irq_enable(struct iio_dev *indio_dev)
{
	struct sx9310_iio_data *sdata = iio_priv(indio_dev);
	struct sx9310_p *data = sdata->cdata;

	if (!atomic_cmpxchg(&data->pseudo_irq_enable, 0, 1)) {
		mutex_lock(&data->lock);
		sx9310_set_enable(data, 1);
		mutex_unlock(&data->lock);
	}
	return 0;
}

static int sx9310_pseudo_irq_disable(struct iio_dev *indio_dev)
{
	struct sx9310_iio_data *sdata = iio_priv(indio_dev);
	struct sx9310_p *data = sdata->cdata;

	if (atomic_cmpxchg(&data->pseudo_irq_enable, 1, 0)) {
		mutex_lock(&data->lock);
		sx9310_set_enable(data, 0);
		mutex_unlock(&data->lock);
	}
	return 0;
}

static int sx9310_set_pseudo_irq(struct iio_dev *indio_dev, int enable)
{
	if (enable)
		sx9310_pseudo_irq_enable(indio_dev);
	else
		sx9310_pseudo_irq_disable(indio_dev);
	return 0;
}

static int sx9310_data_rdy_trigger_set_state(struct iio_trigger *trig,
		bool state)
{
	struct iio_dev *indio_dev = iio_trigger_get_drvdata(trig);

	sx9310_set_pseudo_irq(indio_dev, state);

	return 0;
}

static const struct iio_trigger_ops sx9310_trigger_ops = {
	.owner = THIS_MODULE,
	.set_trigger_state = &sx9310_data_rdy_trigger_set_state,
};

static int sx9310_read_setupreg(struct device_node *dnode, char *str, u8 *val)
{
	u32 temp_val;
	int ret;
	
	ret = of_property_read_u32(dnode, str, &temp_val);
	if (!ret) {
		*val = (u8)temp_val;
		SENSOR_INFO("[SX9310_W] %s = %u\n", str, temp_val);
	}

	return ret;
}

static int sx9310_parse_dt(struct sx9310_p *data, struct device *dev)
{
	struct device_node *dnode = dev->of_node;
	enum of_gpio_flags flags;
	u32 temp_val;
	int ret;

	if (dnode == NULL)
		return -ENODEV;

	data->gpio_nirq = of_get_named_gpio_flags(dnode,
		"sx9310,nirq-gpio", 0, &flags);
	if (data->gpio_nirq < 0) {
		SENSOR_ERR("[SX9310_W] get gpio_nirq error\n");
		return -ENODEV;
	}

	sx9310_read_setupreg(dnode, "sx9310,ctrl0", &setup_reg[SX9310_CPS_CTRL0].val);
	sx9310_read_setupreg(dnode, "sx9310,ctrl1", &setup_reg[SX9310_CPS_CTRL1].val);
	sx9310_read_setupreg(dnode, "sx9310,ctrl2", &setup_reg[SX9310_CPS_CTRL2].val);
	sx9310_read_setupreg(dnode, "sx9310,ctrl3", &setup_reg[SX9310_CPS_CTRL3].val);
	sx9310_read_setupreg(dnode, "sx9310,ctrl4", &setup_reg[SX9310_CPS_CTRL4].val);
	sx9310_read_setupreg(dnode, "sx9310,ctrl5", &setup_reg[SX9310_CPS_CTRL5].val);
	sx9310_read_setupreg(dnode, "sx9310,ctrl6", &setup_reg[SX9310_CPS_CTRL6].val);
	sx9310_read_setupreg(dnode, "sx9310,ctrl7", &setup_reg[SX9310_CPS_CTRL7].val);
	sx9310_read_setupreg(dnode, "sx9310,ctrl8", &setup_reg[SX9310_CPS_CTRL8].val);
	sx9310_read_setupreg(dnode, "sx9310,ctrl9", &setup_reg[SX9310_CPS_CTRL9].val);
	sx9310_read_setupreg(dnode, "sx9310,ctrl10",&setup_reg[SX9310_CPS_CTRL10].val);
	sx9310_read_setupreg(dnode, "sx9310,sarctrl0",&setup_reg[SX9310_SAR_CTRL0].val);

	ret = of_property_read_u32(dnode, "sx9310,normal_th", &temp_val);
	if (!ret) {
		data->normal_th = (u8)temp_val;
		data->normal_th_buf = data->normal_th;
		SENSOR_INFO("[SX9310_W] Normal Touch Threshold : %u\n",
			data->normal_th);
	} else {
		data->normal_th = 176;
		SENSOR_INFO("[SX9310_W] Can't get normal_th\n");
	}
#ifdef CONFIG_SENSORS_SX9310_WIFI_DEFENCE_CODE_FOR_TA_NOISE
	ret = of_property_read_u32(dnode, "sx9310,cable_norm_th", &temp_val);
	if (!ret) {
		data->cable_norm_th = (u8)temp_val;
		SENSOR_INFO("[SX9310_W] Cable Normal Touch Threshold : %u\n",
			data->cable_norm_th);
	} else {
		data->cable_norm_th = 0;
		SENSOR_INFO("[SX9310_W] Can't get cable normal_th\n");
	}
#endif
	return 0;
}

#if defined(CONFIG_MUIC_NOTIFIER)
static int sx9310_cpuidle_muic_notifier(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct sx9310_p *grip_data;
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;

	grip_data = container_of(nb, struct sx9310_p, cpuidle_muic_nb);

	if (grip_data->init_done == OFF) {
		SENSOR_INFO("[SX9310_W] not initialized\n");
		goto skip_noti;
	}

	switch (attached_dev) {
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) {
			if (grip_data->cable_norm_th == 0)
				grip_data->normal_th = SX9310_NORMAL_TOUCH_CABLE_THRESHOLD;
			else
				grip_data->normal_th = grip_data->cable_norm_th;

			SENSOR_INFO("[SX9310_W] TA/USB is inserted\n");
		}
		else {
			grip_data->normal_th = grip_data->normal_th_buf;
			SENSOR_INFO("[SX9310_W] TA/USB is removed\n");
		}

			sx9310_set_offset_calibration(grip_data);

	default:
		break;
	}

skip_noti:
	SENSOR_INFO("[SX9310_W] dev=%d, action=%lu\n", attached_dev, action);

	return NOTIFY_DONE;
}
#elif defined(CONFIG_MUIC_NOTI)
static int sx9310_cpuidle_muic_notifier(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct sx9310_p *grip_data;
	muic_attached_dev attached_dev = *(muic_attached_dev *)data;

	grip_data = container_of(nb, struct sx9310_p, cpuidle_muic_nb);

	switch (attached_dev) {
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_TA_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) 
			SENSOR_INFO("[SX9310_W] TA/USB is inserted\n");
		else
			SENSOR_INFO("[SX9310_W] TA/USB is removed\n");

		if (grip_data->init_done == ON)
			sx9310_set_offset_calibration(grip_data);
		else
			SENSOR_INFO("[SX9310_W] not initialized\n");
		break;
	default:
		break;
	}

	SENSOR_INFO("[SX9310_W] dev=%d, action=%lu\n", attached_dev, action);

	return NOTIFY_DONE;
}
#endif

static int sx9310_probe_trigger(struct iio_dev *indio_dev)
{
	int ret;
	struct sx9310_iio_data *sdata = iio_priv(indio_dev);
	struct sx9310_p *data = sdata->cdata;

	indio_dev->pollfunc = iio_alloc_pollfunc(
			&sx9310_wifi_iio_pollfunc_store_boottime,
			&sx9310_trigger_handler, IRQF_ONESHOT, indio_dev,
			"%s_consumer%d", indio_dev->name, indio_dev->id);
	if (indio_dev->pollfunc == NULL) {
		ret = -ENOMEM;
		goto error_ret;
	}

	data->trig = iio_trigger_alloc("%s-dev%d",
			indio_dev->name,
			indio_dev->id);
	if (!data->trig) {
		ret = -ENOMEM;
		goto error_dealloc_pollfunc;
	}

	data->trig->dev.parent = &data->client->dev;
	data->trig->ops = &sx9310_trigger_ops;
	iio_trigger_set_drvdata(data->trig, indio_dev);
	ret = iio_trigger_register(data->trig);
	if (ret)
		goto error_free_trig;

	return 0;

error_free_trig:
	iio_trigger_free(data->trig);
error_dealloc_pollfunc:
	iio_dealloc_pollfunc(indio_dev->pollfunc);
error_ret:
	return ret;
}

static void sx9310_remove_trigger(struct iio_dev *indio_dev)
{
	struct sx9310_iio_data *sdata = iio_priv(indio_dev);
	struct sx9310_p *data = sdata->cdata;

	iio_trigger_unregister(data->trig);
	iio_trigger_free(data->trig);
	iio_dealloc_pollfunc(indio_dev->pollfunc);
}

static const struct iio_buffer_setup_ops sx9310_buffer_setup_ops = {
	.preenable = &iio_sw_buffer_preenable,
	.postenable = &iio_triggered_buffer_postenable,
	.predisable = &iio_triggered_buffer_predisable,
};

static int sx9310_probe_buffer(struct iio_dev *indio_dev)
{
	int ret;
	struct iio_buffer *buffer;
	buffer = iio_kfifo_allocate(indio_dev);
	if (!buffer) {
		ret = -ENOMEM;
		goto error_ret;
	}

	buffer->scan_timestamp = true;
	indio_dev->buffer = buffer;
	indio_dev->setup_ops = &sx9310_buffer_setup_ops;
	indio_dev->modes |= INDIO_BUFFER_TRIGGERED;
	ret = iio_buffer_register(indio_dev, indio_dev->channels,
			indio_dev->num_channels);
	if (ret)
		goto error_free_buf;

	iio_scan_mask_set(indio_dev, indio_dev->buffer, SX9310_SCAN_GRIP_CH);
	return 0;
error_free_buf:
	iio_kfifo_free(indio_dev->buffer);
error_ret:
	return ret;
}

static void sx9310_remove_buffer(struct iio_dev *indio_dev)
{
	iio_buffer_unregister(indio_dev);
	iio_kfifo_free(indio_dev->buffer);
}

static int sx9310_read_raw(struct iio_dev *indio_dev,
	struct iio_chan_spec const *chan, int *val, int *val2, long mask)
{
	struct sx9310_iio_data *sdata = iio_priv(indio_dev);
	struct sx9310_p *data = sdata->cdata;
	int ret = -EINVAL;

	if (chan->type != IIO_GRIP)
		return -EINVAL;
	mutex_lock(&data->lock);
	switch (mask) {
	case 0:
		ret = IIO_VAL_INT;
		break;
	case IIO_CHAN_INFO_SCALE:
		*val = 0;
		*val2 = 1000;
		ret = IIO_VAL_INT_PLUS_MICRO;
		break;
	}
	mutex_unlock(&data->lock);
	return ret;
}

static const struct iio_info sx9310_info = {
	.read_raw = &sx9310_read_raw,
	.driver_module = THIS_MODULE,
};

static int sx9310_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct sx9310_p *data = NULL;
	struct sx9310_iio_data *sx9310_iio;

	SENSOR_INFO("[SX9310_W] Probe Start!\n");
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		SENSOR_ERR("[SX9310_W] i2c_check_functionality error\n");
		goto exit;
	}

	/* create memory for main struct */
	data = kzalloc(sizeof(struct sx9310_p), GFP_KERNEL);
	if (data == NULL) {
		SENSOR_ERR("[SX9310_W] kzalloc error\n");
		ret = -ENOMEM;
		goto exit_kzalloc;
	}

	/* iio device register */
	data->indio_dev = iio_device_alloc(sizeof(*sx9310_iio));
	if (data->indio_dev == NULL) {
		SENSOR_ERR("[SX9310_W] kzalloc error\n");
		ret = -ENOMEM;
		goto exit_kfree;
	}

	i2c_set_clientdata(client, data);
	data->client = client;
	data->factory_device = &client->dev;

	sx9310_iio = iio_priv(data->indio_dev);
	sx9310_iio->cdata = data;

	data->indio_dev->name = SX9310_GRIP_SENSOR_NAME;
	data->indio_dev->dev.parent = &client->dev;
	data->indio_dev->info = &sx9310_info;
	data->indio_dev->channels = sx9310_channels;
	data->indio_dev->num_channels = ARRAY_SIZE(sx9310_channels);
	data->indio_dev->modes = INDIO_DIRECT_MODE;

	spin_lock_init(&data->spin_lock);
	wake_lock_init(&data->grip_wake_lock,
		WAKE_LOCK_SUSPEND, "grip_wifi_wake_lock");
	mutex_init(&data->read_mutex);
	mutex_init(&data->lock);
	ret = sx9310_parse_dt(data, &client->dev);
	if (ret < 0) {
		SENSOR_ERR("[SX9310_W] of_node error\n");
		ret = -ENODEV;
		goto exit_of_node;
	}

	ret = sx9310_setup_pin(data);
	if (ret) {
		SENSOR_ERR("[SX9310_W] could not setup pin\n");
		goto exit_setup_pin;
	}

	/* read chip id */
	ret = sx9310_i2c_write(data, SX9310_SOFTRESET_REG, SX9310_SOFTRESET);
	if (ret < 0) {
		SENSOR_ERR("[SX9310_W] chip reset failed %d\n", ret);
		goto exit_chip_reset;
	}

	sx9310_initialize_variable(data);
	INIT_DELAYED_WORK(&data->init_work, sx9310_init_work_func);
	INIT_DELAYED_WORK(&data->irq_work, sx9310_irq_work_func);
	INIT_DELAYED_WORK(&data->debug_work, sx9310_debug_work_func);

	/* initailize interrupt reporting */
	data->irq = gpio_to_irq(data->gpio_nirq);
	ret = request_threaded_irq(data->irq, NULL, sx9310_interrupt_thread,
			IRQF_TRIGGER_FALLING|IRQF_ONESHOT,
			"sx9310_wifi_irq", data);
	if (ret < 0) {
		SENSOR_ERR("[SX9310_W] fail to set request irq %d(%d)"
			, data->irq, ret);
		goto exit_request_threaded_irq;
	}
	disable_irq(data->irq);

	ret = sx9310_probe_buffer(data->indio_dev);
	if (ret)
		goto exit_iio_buffer_probe_failed;

	ret = sx9310_probe_trigger(data->indio_dev);
	if (ret)
		goto exit_iio_trigger_probe_failed;

	ret = iio_device_register(data->indio_dev);
	if (ret < 0)
		goto exit_iio_register_failed;

	ret = sensors_register(data->factory_device,
		data, sensor_attrs, MODULE_NAME);
	if (ret) {
		SENSOR_ERR("[SX9310_W] can not register grip_sensor(%d).\n", ret);
		goto grip_sensor_register_failed;
	}
	sensors_create_symlink(&data->indio_dev->dev.kobj, data->indio_dev->name);

	schedule_delayed_work(&data->init_work, msecs_to_jiffies(300));
	sx9310_set_debug_work(data, ON, 20000);

#if defined(CONFIG_MUIC_NOTIFIER) || defined(CONFIG_MUIC_NOTI)
	muic_notifier_register(&data->cpuidle_muic_nb,
		sx9310_cpuidle_muic_notifier, MUIC_NOTIFY_DEV_CPUIDLE);
#endif
	SENSOR_INFO("[SX9310_W] Probe done!\n");

	return 0;

grip_sensor_register_failed:
exit_iio_register_failed:
	sx9310_remove_trigger(data->indio_dev);
exit_iio_trigger_probe_failed:
	sx9310_remove_buffer(data->indio_dev);
exit_iio_buffer_probe_failed:
	sensors_unregister(data->factory_device, sensor_attrs);
exit_request_threaded_irq:
exit_chip_reset:
	free_irq(data->irq, data);
	gpio_free(data->gpio_nirq);
exit_setup_pin:
exit_of_node:
	mutex_destroy(&data->lock);
	wake_lock_destroy(&data->grip_wake_lock);
	mutex_destroy(&data->read_mutex);
exit_kfree:
	kfree(data);
exit_kzalloc:
exit:
	SENSOR_ERR("[SX9310_W] Probe fail!\n");
	return ret;
}

static int sx9310_remove(struct i2c_client *client)
{
	struct sx9310_p *data = i2c_get_clientdata(client);

	if (atomic_read(&data->enable) == ON)
		sx9310_set_enable(data, OFF);
		
		sx9310_set_mode(data, SX9310_MODE_SLEEP);

	cancel_delayed_work_sync(&data->init_work);
	cancel_delayed_work_sync(&data->irq_work);
	cancel_delayed_work_sync(&data->debug_work);
	free_irq(data->irq, data);
	gpio_free(data->gpio_nirq);

	wake_lock_destroy(&data->grip_wake_lock);
	sensors_unregister(data->factory_device, sensor_attrs);
	mutex_destroy(&data->read_mutex);

	iio_device_unregister(data->indio_dev);

	return 0;
}

static int sx9310_suspend(struct device *dev)
{
	struct sx9310_p *data = dev_get_drvdata(dev);
	int cnt = 0;

	SENSOR_INFO("[SX9310_W]\n");
	/* before go to sleep, make the interrupt pin as high*/
	while ((sx9310_get_nirq_state(data) == 0) && (cnt++ < 3)) {
		sx9310_read_irqstate(data);
		msleep(20);
	}
	if (cnt >= 3)
		SENSOR_ERR("[SX9310_W] s/w reset fail(%d)\n", cnt);

	sx9310_set_debug_work(data, OFF, 1000);

	return 0;
}

static int sx9310_resume(struct device *dev)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	SENSOR_INFO("[SX9310_W]\n");
	sx9310_set_debug_work(data, ON, 1000);

	return 0;
}

static void sx9310_shutdown(struct i2c_client *client)
{
	struct sx9310_p *data = i2c_get_clientdata(client);

	SENSOR_INFO("[SX9310_W]\n");
	sx9310_set_debug_work(data, OFF, 1000);
	if (atomic_read(&data->enable) == ON)
		sx9310_set_enable(data, OFF);
	sx9310_set_mode(data, SX9310_MODE_SLEEP);
}

static struct of_device_id sx9310_match_table[] = {
	{ .compatible = "sx9310-wifi-i2c",},
	{},
};

static const struct i2c_device_id sx9310_id[] = {
	{ "sx9310_match_table", 0 },
	{ }
};

static const struct dev_pm_ops sx9310_pm_ops = {
	.suspend = sx9310_suspend,
	.resume = sx9310_resume,
};

static struct i2c_driver sx9310_driver = {
	.driver = {
		.name	= MODEL_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sx9310_match_table,
		.pm = &sx9310_pm_ops
	},
	.probe		= sx9310_probe,
	.remove		= sx9310_remove,
	.shutdown	= sx9310_shutdown,
	.id_table	= sx9310_id,
};

static int __init sx9310_wifi_init(void)
{
	return i2c_add_driver(&sx9310_driver);
}

static void __exit sx9310_wifi_exit(void)
{
	i2c_del_driver(&sx9310_driver);
}

module_init(sx9310_wifi_init);
module_exit(sx9310_wifi_exit);

MODULE_DESCRIPTION("Semtech Corp. SX9310 Capacitive Touch Controller Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
