/*
 * Driver for the SX9385
 * Copyright (c) 2011 Semtech Corp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/pm_wakeup.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/power_supply.h>
#include <linux/version.h>
#include "sx9385.h"

#define SENSOR_ATTR_SIZE 65

#if IS_ENABLED(CONFIG_SENSORS_CORE_AP)
#include <linux/sensor/sensors_core.h>
#endif

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
#include <linux/usb/typec/common/pdic_notifier.h>
#endif
#if IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
#include <linux/usb/typec/manager/usb_typec_manager_notifier.h>
#endif

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
#include <linux/hall/hall_ic_notifier.h>
#define HALL_NAME		"hall"
#define HALL_CERT_NAME		"certify_hall"
#define HALL_FLIP_NAME		"flip"
#define HALL_ATTACH		1
#define HALL_DETACH		0
#endif

#define NUM_MAIN_PHASES         2 //PH2 and PH4 are the main phases of sx9385
#define NUM_OF_ALL_PHASES       4
#define I2C_M_WR				0 /* for i2c Write */
//#define I2C_M_RD				1 /* for i2c Read */

#define HAS_ERROR			   -1
#define IDLE					2
#define ACTIVE				    1

#define DIFF_READ_NUM			10
#define GRIP_LOG_TIME			15 /* 30 sec */

#define REF_OFF_MAIN_OFF		0x00

#define IRQ_PROCESS_CONDITION   (IRQSTAT_TOUCH_FLAG	\
				| IRQSTAT_RELEASE_FLAG	\
				| IRQSTAT_COMPDONE_FLAG)

#define SX9385_MODE_SLEEP		0
#define SX9385_MODE_NORMAL	    1

/* Failure Index */
#define SX9385_ID_ERROR		(1 << 0)
#define SX9385_NIRQ_ERROR	(1 << 1)
#define SX9385_CONN_ERROR	(1 << 2)
#define SX9385_I2C_ERROR	(1 << 3)
#define SX9385_REG_ERROR	(1 << 4)
#define SX9385_SCAN_ERROR	(1 << 5)

#define GRIP_HAS_ERR   -1
#define GRIP_WORKING	1
#define GRIP_RELEASE	2

#define INTERRUPT_HIGH	1
#define INTERRUPT_LOW	0

#define TYPE_USB		1
#define TYPE_HALL  		2
#define TYPE_BOOT		3
#define TYPE_FORCE		4
#define TYPE_OVERFLOW   5

#define UNKNOWN_ON		1
#define UNKNOWN_OFF 	2

#define DIFF_OVERFLOW -32768

#define PH1 0x08
#define PH2 0x10
#define PH3 0x20
#define PH4 0x40

#define MAX_I2C_FAIL_COUNT 3

enum grip_error_state {
	FAIL_UPDATE_PREV_STATE = 1,
	FAIL_SETUP_REGISTER,
	FAIL_I2C_ENABLE,
	FAIL_I2C_READ_3_TIMES,
	FAIL_DATA_STUCK,
	FAIL_RESET,
	FAIL_MCC_RESET,
	FAIL_IRQ_MISS_MATCH,
	FAIL_ENABLE_SCAN_ERROR
};

enum entry_path {
	BY_INTERRUPT_HANDLER = 0,
	BY_ENABLE_FUNC,
	BY_DEBUG_WORK
};

struct reg_addr_val_s {
	u8 addr;
	u8 val;
};

struct channel {
	unsigned int grip_code;
	unsigned int unknown_code;
	unsigned int noti_code;

	int is_unknown_mode;
	int noti_enable;
	int diff_cnt;
	int again_m;
	int dgain_m;

	s32 useful_avg;
	s32 capMain;
	s32 useful;

	u16 detect_threshold;
	u16 release_threshold;
	u16 offset;
	s16 avg;
	s16 diff;
	s16 diff_avg;
	s16 max_diff;
	s16 max_normal_diff;

	u8 num;
	u8 prox_mask;
	u8 state_miss_matching_count;
	s8 state;
	s8 prev_state;

	bool first_working;
	bool unknown_sel;
	bool enabled;
};

struct sx9385_p {
	struct i2c_client *client;
	struct input_dev *input;
	struct input_dev *noti_input_dev;
	struct device *factory_device;
	struct delayed_work init_work;
	struct delayed_work irq_work;
	struct delayed_work debug_work;
	struct wakeup_source *grip_ws;
	struct mutex mode_mutex;
	struct mutex read_mutex;
#if IS_ENABLED(CONFIG_PDIC_NOTIFIER)
	struct notifier_block pdic_nb;
#endif
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
	struct notifier_block hall_nb;
#endif
	struct regulator *dvdd_vreg;	/* regulator */
	const char *dvdd_vreg_name;	/* regulator name */
	struct channel *ch[NUM_MAIN_PHASES];
	struct reg_addr_val_s *regs_addr_val;
	atomic_t enable;
	int gpio_nirq;
	int pre_attach;
	int debug_count;
	int debug_zero_count;
	int fail_status_code;
	int irq_count;
	int abnormal_mode;
	int ldo_en;
	int irq;
	int init_done;
	int motion;
	int num_regs;
	int num_of_channels;
	int num_of_refs;
	int unknown;
/* REF channel */
	int again_r[2];
	int dgain_r[2];
	s32 refMain[2];

	u32 err_state;
	u8 prev_status;
	u8 ic_num;
	u8 i2c_fail_count;
	u8 initial_set_phases;
	bool is_irq_active;
	bool skip_data;
	bool check_abnormal_working;
};

static int sx9385_set_mode(struct sx9385_p *data, unsigned char mode);

static int sx9385_get_nirq_state(struct sx9385_p *data)
{
	return gpio_get_value_cansleep(data->gpio_nirq);
}

static void enter_unknown_mode(struct sx9385_p *data, int type, struct channel *ch)
{
	if (ch->noti_enable && !data->skip_data && ch->unknown_sel) {
		data->motion = 0;
		ch->first_working = false;
		if (ch->is_unknown_mode == UNKNOWN_OFF) {
			ch->is_unknown_mode = UNKNOWN_ON;
			if (atomic_read(&data->enable) == ON) {
				input_report_rel(data->input, ch->unknown_code, UNKNOWN_ON);
				input_sync(data->input);
			}
			GRIP_INFO("UNKNOWN Re-enter\n");
		} else {
			GRIP_INFO("already UNKNOWN\n");
		}
		input_report_rel(data->noti_input_dev, ch->noti_code, type);
		input_sync(data->noti_input_dev);
	}
}

static void enter_error_mode(struct sx9385_p *data, enum grip_error_state err_state)
{
	int i = 0;

	if (data->is_irq_active) {
		disable_irq(data->irq);
		disable_irq_wake(data->irq);
		data->is_irq_active = false;
	}

	data->check_abnormal_working = true;
	data->err_state |= 0x1 << err_state;
	sx9385_set_mode(data, SX9385_MODE_SLEEP);

	for (i = 0; i < data->num_of_channels; i++)
		enter_unknown_mode(data, TYPE_FORCE + err_state, data->ch[i]);

	GRIP_ERR("%d\n", data->err_state);
}

static void check_irq_error(struct sx9385_p *data, u8 irq_state,
	enum entry_path path, struct channel *ch)
{
	if (data->is_irq_active && !data->check_abnormal_working) {
		if (path == BY_INTERRUPT_HANDLER) {
			ch->state_miss_matching_count = 0;
			ch->prev_state = irq_state;
		} else if (path == BY_ENABLE_FUNC) {
			ch->prev_state = irq_state;
		} else if (ch->prev_state != irq_state) {
			ch->state_miss_matching_count++;
			GRIP_INFO("ch[%d] prev %x state %x miss_cnt %d\n", ch->num,
				ch->prev_state, irq_state, ch->state_miss_matching_count);
			ch->prev_state = irq_state;
		}

		if (ch->state_miss_matching_count >= 3) {
			GRIP_INFO("enter_error_mode with IRQ\n");
			enter_error_mode(data, FAIL_IRQ_MISS_MATCH);
		}
	}
}

static int sx9385_i2c_write(struct sx9385_p *data, u8 reg_addr, u8 buf)
{
	int ret = -1;
	struct i2c_msg msg;
	unsigned char w_buf[2];

	w_buf[0] = reg_addr;
	w_buf[1] = buf;

	msg.addr = data->client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 2;
	msg.buf = (char *)w_buf;

	if (data->i2c_fail_count < MAX_I2C_FAIL_COUNT)
		ret = i2c_transfer(data->client->adapter, &msg, 1);

	if (ret < 0) {
		data->fail_status_code |= SX9385_I2C_ERROR;
		if (data->i2c_fail_count < MAX_I2C_FAIL_COUNT)
			data->i2c_fail_count++;
		if (data->i2c_fail_count >= MAX_I2C_FAIL_COUNT)
			enter_error_mode(data, FAIL_I2C_READ_3_TIMES);

		GRIP_ERR("err %d, %u", ret, data->i2c_fail_count);
	} else {
		data->i2c_fail_count = 0;
	}

	return ret;
}

static int sx9385_i2c_write_retry(struct sx9385_p *data, u8 reg_addr, u8 buf)
{
	int i;
	int ret;

	for (i = 0; i < 3; i++) {
		ret = sx9385_i2c_write(data, reg_addr, buf);
		if (ret >= 0)
			return ret;
	}
	return ret;
}

static int sx9385_i2c_read(struct sx9385_p *data, u8 reg_addr, u8 *buf)
{
	int ret = -1;
	struct i2c_msg msg[2];

	msg[0].addr = data->client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = data->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	if (data->i2c_fail_count < MAX_I2C_FAIL_COUNT) {
			ret = i2c_transfer(data->client->adapter, msg, 2);
			if (ret < 0) {
				GRIP_ERR("err %d, %u", ret, msg[0].addr);
		}
	}

	if (ret < 0) {
		data->fail_status_code |= SX9385_I2C_ERROR;
		if (data->i2c_fail_count < MAX_I2C_FAIL_COUNT)
			data->i2c_fail_count++;
		if (data->i2c_fail_count >= MAX_I2C_FAIL_COUNT)
			enter_error_mode(data, FAIL_I2C_READ_3_TIMES);

		GRIP_ERR("err %d, %u", ret, data->i2c_fail_count);
	} else {
		data->i2c_fail_count = 0;
	}

	return ret;
}

static int sx9385_i2c_read_retry(struct sx9385_p *data, u8 reg_addr, u8 *buf)
{
	int i;
	int ret;

	for (i = 0; i < 3; i++) {
		ret = sx9385_i2c_read(data, reg_addr, buf);
		if (ret >= 0)
			return ret;
	}
	return ret;
}

static int sx9385_read_reg_stat(struct sx9385_p *data)
{
	if (data) {
		u8 value = 0;
		if (sx9385_i2c_read_retry(data, REG_IRQ_SRC, &value) >= 0)
			return (value & 0x00FF);
	}
	return 0;
}

static void sx9385_initialize_register(struct sx9385_p *data)
{
	u8 val = 0;
	int ret;
	int i = 0;
	u16 reg_addr;
	u32 reg_val;
	u8 th_reg[] = {REG_PROX_CTRL5_PH2, REG_PROX_CTRL5_PH4};
	u8 hyst_reg[] = {REG_PROX_CTRL4_PH2, REG_PROX_CTRL4_PH4};

	data->init_done = OFF;

	for (i = 0; i < data->num_regs; i++) {
		reg_addr = data->regs_addr_val[i].addr;
		reg_val  = data->regs_addr_val[i].val;

		if (reg_addr == REG_PHEN) {
			data->initial_set_phases = reg_val;
			continue;
		}

		ret = sx9385_i2c_write_retry(data, reg_addr, reg_val);
		if (ret < 0) {
			GRIP_ERR("Failed to write reg=0x%x value=0x%X", reg_addr, reg_val);
			data->fail_status_code |= SX9385_REG_ERROR;
			enter_error_mode(data, FAIL_SETUP_REGISTER);
			return;
		}

		{
			u8 buf = 0;

			sx9385_i2c_read_retry(data, reg_addr, &buf);
			if (reg_val != buf) {
				GRIP_ERR("addr[0x%x] : write =0x%x read=0x%X", reg_addr, reg_val, buf);
				data->fail_status_code = SX9385_REG_ERROR;
				enter_error_mode(data, FAIL_SETUP_REGISTER);
			}
		}
	}

	for (i = 0; i < data->num_of_channels; i++) {
		val = 0;
		ret = sx9385_i2c_read_retry(data, th_reg[i], &val);

		if (ret < 0) {
			data->ch[i]->detect_threshold = 0;
			data->ch[i]->release_threshold = 0;
			GRIP_ERR("detect threshold update1 fail\n");
		} else {
			data->ch[i]->detect_threshold = (u16)val * (u16)val / 2;
			data->ch[i]->release_threshold = (u16)val * (u16)val / 2;
			val = 0;
			ret = sx9385_i2c_read(data, hyst_reg[i], &val);
			if (ret < 0) {
				GRIP_ERR("detect hyst_reg fail\n");
			} else {
				val = (val & 0x30) >> 4;
				if (val) {
					data->ch[i]->detect_threshold += data->ch[i]->detect_threshold >> (5 - val);
					data->ch[i]->release_threshold  += data->ch[i]->release_threshold  >> (5 - val);
				}
			}
		}
		if (ret < 0)
			enter_error_mode(data, FAIL_SETUP_REGISTER);
		GRIP_INFO("detect threshold %u\n", data->ch[i]->detect_threshold);
	}

	data->init_done = ON;
}

static int sx9385_hardware_check(struct sx9385_p *data)
{
	int ret;
	u8 whoami = 0;
	u8 loop = 0;

	//Check th IRQ Status
	while (sx9385_get_nirq_state(data) == INTERRUPT_LOW) {
		sx9385_read_reg_stat(data);
		GRIP_INFO("irq state : %d", sx9385_get_nirq_state(data));
		if (++loop > 10) {
			data->fail_status_code |= SX9385_NIRQ_ERROR;
			return -ENXIO;
		}
		usleep_range(10000, 11000);
	}

	ret = sx9385_i2c_read_retry(data, REG_DEV_INFO, &whoami);
	if (ret < 0) {
		data->fail_status_code |= SX9385_ID_ERROR;
		return data->fail_status_code;
	}

	GRIP_INFO("whoami 0x%x\n", whoami);

	if (whoami != SX9385_WHOAMI_VALUE) {
		data->fail_status_code |= SX9385_ID_ERROR;
		return data->fail_status_code;
	}

	return ret;
}

static int sx9385_set_phase_enable(struct sx9385_p *data, u8 ph)
{
	int ret = 0;
	u8 temp = 0;
	u8 phases[] = {PH1, PH2, PH3, PH4};

	if (data->initial_set_phases & phases[ph]) {
		ret = sx9385_i2c_read_retry(data, REG_GNRL_CTRL2, &temp);
		if (ret < 0) {
			data->fail_status_code |= SX9385_SCAN_ERROR;
			GRIP_ERR("ph en err\n");
			return ret;
		}
		temp &= ~(1 << 7); //clear

		ret = sx9385_i2c_write_retry(data, REG_GNRL_CTRL2, temp);
		if (ret < 0) {
			data->fail_status_code |= SX9385_SCAN_ERROR;
			GRIP_ERR("ph en err\n");
			return ret;
		}

		ret = sx9385_i2c_read_retry(data, REG_PHEN, &temp);
		if (ret < 0) {
			GRIP_ERR("ph en err\n");
			data->fail_status_code |= SX9385_SCAN_ERROR;
			return ret;
		}

		temp |= phases[ph];

		ret = sx9385_i2c_write_retry(data, REG_PHEN, temp);
		if (ret < 0) {
			data->fail_status_code |= SX9385_SCAN_ERROR;
			GRIP_ERR("ph en err\n");
		}
	}

	return ret;
}


static int wait_for_convstat_clear(struct sx9385_p *data)
{
	int retry = 0;
	s8 convstat = -1;

	while (1) {
		sx9385_i2c_read(data, REG_PROX_STATUS, &convstat);
		convstat &= 0x04;

		if (++retry > 10 || convstat == 0)
			break;

		usleep_range(10000, 11000);
	}
	return convstat;
}

static int sx9385_set_phase_disable(struct sx9385_p *data, u8 ph)
{
	int ret = 0;
	u8 temp = 0;
	u8 phases[] = {PH1, PH2, PH3, PH4};

	if (data->initial_set_phases & phases[ph]) {
		ret = sx9385_i2c_read_retry(data, REG_PHEN, &temp);
		if (ret < 0) {
			data->fail_status_code |= SX9385_SCAN_ERROR;
			GRIP_ERR("ph disalbe err\n");
			return ret;
		}
		temp &= ~phases[ph];
		if (temp == 0x00) {
			ret = wait_for_convstat_clear(data);
			if (ret < 0) {
				data->fail_status_code |= SX9385_SCAN_ERROR;
				GRIP_ERR("ph convstat_clear err\n");
				return ret;
			}
		}
		ret = sx9385_i2c_write_retry(data, REG_PHEN, temp);
		if (ret < 0) {
			data->fail_status_code |= SX9385_SCAN_ERROR;
			GRIP_ERR("ph disalbe err\n");
			return ret;
		}
	}

	return ret;
}


static int sx9385_manual_offset_calibration(struct sx9385_p *data)
{
	int ret = 0;
	int ph = 0;

	GRIP_INFO("\n");
	if (data->check_abnormal_working)
		return -1;

	for (ph = 0; ph < NUM_OF_ALL_PHASES; ph++) {
		ret = sx9385_set_phase_disable(data, ph);
		if (ret < 0)
			break;
		ret = sx9385_set_phase_enable(data, ph);
		if (ret < 0)
			break;
	}

	if (ret < 0)
		GRIP_ERR("Failed to calibrate. ret=%d", ret);

	return ret;
}


static void sx9385_send_event(struct sx9385_p *data, u8 state, struct channel *ch)
{
	if (state == ACTIVE) {
		ch->state = ACTIVE;
		data->prev_status |= ch->prox_mask;
		GRIP_INFO("ch[%d] touched\n", ch->num);
	} else if (state == IDLE) {
		ch->state = IDLE;
		data->prev_status &= ~(ch->prox_mask);
		GRIP_INFO("ch[%d] released\n", ch->num);
	} else {
		ch->state = HAS_ERROR;
		data->prev_status &= ~(ch->prox_mask);
		GRIP_INFO("ch[%d] released\n", ch->num);
	}

	if (data->skip_data == true) {
		GRIP_INFO("skip grip event\n");
		return;
	}

	input_report_rel(data->input, ch->grip_code, ch->state);

	if (ch->unknown_sel)
		input_report_rel(data->input, ch->unknown_code, ch->is_unknown_mode);
	input_sync(data->input);
}

static void sx9385_get_gain(struct sx9385_p *data)
{
	u8 gain_reg[] = {REG_AFE_PARAM2_PH2, REG_AFE_PARAM2_PH4};
	int ret = 0;
	int i = 0;
	static const int again_phm_default = 3875; //X10,000

	if (data->check_abnormal_working)
		return;

	for (i = 0; i < data->num_of_channels; i++) {
		u8 msByte = 0;
		u8 again = 0;
		u8 dgain = 0;

		ret = sx9385_i2c_read(data, gain_reg[i], &msByte);
		if (ret < 0)
			GRIP_INFO("fail get gain val:%d\n", i);
		again = (msByte) & 0x1F;
		data->ch[i]->again_m = again_phm_default * again;

		dgain = (msByte >> 5) & 0x07;
		if (dgain)
			data->ch[i]->dgain_m = 1 << (dgain - 1);
		else
			data->ch[i]->dgain_m = 1;
	}
}

static void sx9385_get_ref_gain(struct sx9385_p *data)
{
	u8 gain_reg[] = {REG_AFE_PARAM2_PH1, REG_AFE_PARAM2_PH3};
	int ret = 0;
	int i = 0;
	static const int again_phm_default = 3875; //X10,000

	if (data->check_abnormal_working)
		return;

	for (i = 0; i < data->num_of_refs; i++) {
		u8 msByte = 0;
		u8 again = 0;
		u8 dgain = 0;

		ret = sx9385_i2c_read(data, gain_reg[i], &msByte);
		if (ret < 0)
			GRIP_INFO("fail get gain val:%d\n", i);
		again = (msByte) & 0x1F;
		data->again_r[i] = again_phm_default * again;

		dgain = (msByte >> 5) & 0x07;
		if (dgain)
			data->dgain_r[i] = 1 << (dgain - 1);
		else
			data->dgain_r[i] = 1;
	}
}

static void sx9385_get_refcap(struct sx9385_p *data)
{
	u8 reg_use_msb[] = {0xBA, 0xCD};
	u8 reg_off_msb[] = {0x4B, 0x58};
	u8 msb, lsb;
	s16 use[NUM_MAIN_PHASES];
	u16 off[NUM_MAIN_PHASES];
	int idx;
	int coffset, cuseful;

	for (idx = 0; idx < data->num_of_refs; idx++) {
		sx9385_i2c_read(data, reg_use_msb[idx],   &msb);
		sx9385_i2c_read(data, reg_use_msb[idx] + 1, &lsb);
		use[idx] = msb << 8 | lsb;

		sx9385_i2c_read(data, reg_off_msb[idx],   &msb);
		sx9385_i2c_read(data, reg_off_msb[idx] + 1, &lsb);
		off[idx] = msb << 8 | lsb;

		coffset = ((msb & 0xFE) >> 1) * 38000 + (((msb & 0x01) << 8) + lsb) * 125;
		cuseful = (use[idx] * data->again_r[idx]) / (32768 * data->dgain_r[idx]);
		data->refMain[idx] = coffset + cuseful;

		GRIP_INFO("refCap[%d] = %d\n", idx+1, data->refMain[idx]);
	}
}

static void sx9385_get_data(struct sx9385_p *data)
{
	int idx;
	int coffset, cuseful;
	int i = 0;
	int retry = 0;
	int dif[NUM_MAIN_PHASES];
	u8 convstat = 0xFF;

	u8 msb, mid, lsb;
	u16 off[NUM_MAIN_PHASES];
	s16 avg[NUM_MAIN_PHASES];
	s16 use[NUM_MAIN_PHASES], dlt[NUM_MAIN_PHASES];
	u16 dlt_hex[NUM_MAIN_PHASES];
	u8 reg_avg_msb[] = {0xC7, 0xDA};
	u8 reg_dlt_msb[] = {0xC2, 0xD5};
	u8 reg_use_msb[] = {0xBD, 0xD0};
	u8 reg_off_msb[] = {0x51, 0x5E};

	mutex_lock(&data->read_mutex);

	// Semtech reference code receives the DIFF data, when 'CONVERSION' is completed (IRQ falling),
	// but Samsung receive diff data by POLLING. So we have to check the completion of 'CONVERSION'.

	while (1) {
		sx9385_i2c_read(data, REG_PROX_STATUS, &convstat);
		convstat &= 0x04;

		if (++retry > 10 || convstat == 0)
			break;

		usleep_range(10000, 11000);
	}

	if (retry > 10) {
		GRIP_INFO("fail, retry %d CONV %u\n", retry, convstat);
		mutex_unlock(&data->read_mutex);
		return;
	}

	for (idx = 0; idx < data->num_of_channels; idx++) { // sensing
		sx9385_i2c_read(data, reg_use_msb[idx],   &msb);
		sx9385_i2c_read(data, reg_use_msb[idx] + 1, &lsb);
		use[idx] = msb << 8 | lsb;

		sx9385_i2c_read(data, reg_off_msb[idx],   &msb);
		sx9385_i2c_read(data, reg_off_msb[idx] + 1, &lsb);
		off[idx] = msb << 8 | lsb;
/*
 Ctotal = Coffset + Cuseful
 Coffset (pF) = PROXOFFSET[15:9]*3.8 + PROXOFFSET[8:0]*0.0125
 Cuseful = PROXUSEFUL*[|AGAIN|/(32768*DGAIN)]
*/
		coffset = ((msb & 0xFE) >> 1) * 38000 + (((msb & 0x01) << 8) + lsb) * 125;
		cuseful = (use[idx] * data->ch[idx]->again_m) / (32768 * data->ch[idx]->dgain_m);
		data->ch[idx]->capMain = coffset + cuseful;

		sx9385_i2c_read(data, reg_avg_msb[idx],   &msb);
		sx9385_i2c_read(data, reg_avg_msb[idx]+1, &mid);
		sx9385_i2c_read(data, reg_avg_msb[idx]+2, &lsb);

		avg[idx] = (s16)(msb & 0x03)<<14 | mid<<6 | lsb >> 2;

		dif[idx] = use[idx] - avg[idx];
		if (dif[idx] > 32767)
			dif[idx] = 32767;
		else if (dif[idx] < -32768)
			dif[idx] = -32768;

		sx9385_i2c_read(data, reg_dlt_msb[idx],   &msb);
		sx9385_i2c_read(data, reg_dlt_msb[idx]+1, &lsb);
		dlt_hex[idx] = msb | lsb;
		dlt[idx] = ((s16)msb<<8 | lsb)/16;
	}

	for (i = 0; i < data->num_of_channels; i++) {
		data->ch[i]->useful = use[i];
		data->ch[i]->avg = avg[i];
		data->ch[i]->diff = dif[i];
		data->ch[i]->offset = off[i];
		GRIP_INFO("ch[%d]: diff= %d useful= %d average = %d dlt= %d offset= %d skip= :%d\n", i,
					dif[i],  use[i], avg[i], dlt[i], off[i], data->skip_data);
	}

	mutex_unlock(&data->read_mutex);

	return;
}

static int sx9385_set_mode(struct sx9385_p *data, unsigned char mode)
{
	int ret = -EINVAL;
	int ph;

	if (data->check_abnormal_working) {
		GRIP_INFO("abnormal working\n");
		return -1;
	}
	GRIP_INFO("%u\n", mode);

	mutex_lock(&data->mode_mutex);
	if (mode == SX9385_MODE_SLEEP) {
		for (ph = 0; ph < NUM_OF_ALL_PHASES; ph++) {
			ret = sx9385_set_phase_disable(data, ph);
			if (ret < 0)  {
				enter_error_mode(data, FAIL_ENABLE_SCAN_ERROR);
				goto exit_set_mode;
			}
		}
	} else if (mode == SX9385_MODE_NORMAL) {
		for (ph = 0; ph < NUM_OF_ALL_PHASES; ph++) {
			ret = sx9385_set_phase_enable(data, ph);
			if (ret < 0) {
				enter_error_mode(data, FAIL_ENABLE_SCAN_ERROR);
				goto exit_set_mode;
			}
		}
	}
exit_set_mode:
	mutex_unlock(&data->mode_mutex);
	return ret;
}

static void sx9385_check_status(struct sx9385_p *data, struct channel *ch)
{
	int ret;
	u8 status = 0;

	if (data->skip_data == true) {
		input_report_rel(data->input, ch->grip_code, GRIP_RELEASE);
		if (ch->unknown_sel)
			input_report_rel(data->input, ch->unknown_code, UNKNOWN_OFF);
		input_sync(data->input);
		return;
	}

	if (data->check_abnormal_working)
		return;

	ret = sx9385_i2c_read_retry(data, REG_PROX_STATUS, &status);
	GRIP_INFO("ret %d 0x%x\n", ret, status);

	if ((ret < 0) || (ch->detect_threshold == 0)
			|| (data->check_abnormal_working == true))
		sx9385_send_event(data, HAS_ERROR, ch);
	else if (status & ch->prox_mask)
		sx9385_send_event(data, ACTIVE, ch);
	else
		sx9385_send_event(data, IDLE, ch);
}

static void sx9385_set_enable(struct sx9385_p *data, int enable)
{
	int pre_enable = atomic_read(&data->enable);

	GRIP_INFO("%d\n", enable);
	if (data->check_abnormal_working == true) {
		if (enable)
			atomic_set(&data->enable, ON);
		else
			atomic_set(&data->enable, OFF);

		if (enable) {
			GRIP_INFO("abnormal working\n");
			enter_error_mode(data, FAIL_UPDATE_PREV_STATE);
		}
		return;
	}

	if (enable) {
		if (pre_enable == OFF) {
			int ret;
			int i = 0;
			u8 status[2] = {0, 0};

			sx9385_get_data(data);

			for (i = 0; i < data->num_of_channels; i++) {
				sx9385_check_status(data, data->ch[i]);
				data->ch[i]->diff_avg = 0;
				data->ch[i]->diff_cnt = 0;
				data->ch[i]->useful_avg = 0;
				if (data->ch[i]->state == HAS_ERROR)
					status[i] = IDLE;
				else
					status[i] = data->ch[i]->state;
			}

			msleep(20);
			/* make interrupt high */
			sx9385_read_reg_stat(data);

			/* enable interrupt */
			ret = sx9385_i2c_write(data, REG_IRQ_MSK, 0x30);
			if (ret < 0)
				GRIP_INFO("set enable irq reg fail\n");

			if (data->is_irq_active == false) {
				enable_irq(data->irq);
				enable_irq_wake(data->irq);
				data->is_irq_active = true;
			}

			for (i = 0; i < data->num_of_channels; i++)
				check_irq_error(data, status[i], BY_ENABLE_FUNC, data->ch[i]);

			atomic_set(&data->enable, ON);
		}
	} else {
		if (pre_enable == ON) {
			int ret;
			/* disable interrupt */
			ret = sx9385_i2c_write(data, REG_IRQ_MSK, 0x00);
			if (ret < 0)
				GRIP_INFO("set disable irq reg fail\n");

			if (data->is_irq_active == true) {
				disable_irq(data->irq);
				disable_irq_wake(data->irq);
				data->is_irq_active = false;
			}
			atomic_set(&data->enable, OFF);
		}
	}
}

static void sx9385_set_debug_work(struct sx9385_p *data, u8 enable,
				  unsigned int time_ms)
{
	if (enable == ON && !data->check_abnormal_working) {
		data->debug_count = 0;
		data->debug_zero_count = 0;
		schedule_delayed_work(&data->debug_work,
					  msecs_to_jiffies(time_ms));
	} else {
		cancel_delayed_work_sync(&data->debug_work);
	}
}

static ssize_t sx9385_ref_cap_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	sx9385_get_refcap(data);

	return sprintf(buf, "%d\n", data->refMain[0]);
}
static ssize_t sx9385_ref_cap_b_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	sx9385_get_refcap(data);

	return sprintf(buf, "%d\n", data->refMain[1]);
}
static ssize_t sx9385_get_offset_calibration_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	u8 reg_value = 0;
	struct sx9385_p *data = dev_get_drvdata(dev);

	sx9385_i2c_read(data, REG_COMPENSATION, &reg_value); //check

	return sprintf(buf, "%d\n", reg_value);
}

static ssize_t sx9385_set_offset_calibration_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;
	if (val)
		sx9385_manual_offset_calibration(data);

	return count;
}

static ssize_t sx9385_sw_reset_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 compstat = 0xFF;
	int retry = 0;

	GRIP_INFO("\n");
	sx9385_manual_offset_calibration(data);
	msleep(450);
	sx9385_get_data(data);

	while (retry++ <= 10) {
		sx9385_i2c_read(data, REG_COMPENSATION, &compstat);
		GRIP_INFO("compstat 0x%x\n", compstat);
		if (data->num_of_channels == 2)
			compstat &= 0x0A;
		else
			compstat &= 0x02;

		if (compstat == 0)
			break;

		msleep(50);
	}

	if (compstat == 0)
		return snprintf(buf, PAGE_SIZE, "%d\n", 0);
	return snprintf(buf, PAGE_SIZE, "%d\n", -1);
}

static ssize_t sx9385_vendor_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t sx9385_name_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n", device_name[data->ic_num]);
}

static ssize_t sx9385_touch_mode_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1\n");
}

static ssize_t sx9385_raw_data_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	static s32 sum_diff, sum_useful;
	struct sx9385_p *data = dev_get_drvdata(dev);

	sx9385_get_data(data);

	if (data->ch[0]->diff_cnt == 0) {
		sum_diff = (s32)data->ch[0]->diff;
		sum_useful = data->ch[0]->useful;
	} else {
		sum_diff += (s32)data->ch[0]->diff;
		sum_useful += data->ch[0]->useful;
	}

	if (++data->ch[0]->diff_cnt >= DIFF_READ_NUM) {
		data->ch[0]->diff_avg = (s16)(sum_diff / DIFF_READ_NUM);
		data->ch[0]->useful_avg = sum_useful / DIFF_READ_NUM;
		data->ch[0]->diff_cnt = 0;
	}

	return snprintf(buf, PAGE_SIZE, "%ld,%ld,%u,%d,%d\n", (long int)data->ch[0]->capMain,
			(long int)data->ch[0]->useful, data->ch[0]->offset, data->ch[0]->diff, data->ch[0]->avg);
}


static ssize_t sx9385_diff_avg_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->ch[0]->diff_avg);
}

static ssize_t sx9385_useful_avg_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%ld\n", (long int)data->ch[0]->useful_avg);
}



static ssize_t sx9385_avgnegfilt_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 avgnegfilt = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL3_PH2, &avgnegfilt);

	avgnegfilt = (avgnegfilt & 0x70) >> 3;

	if (avgnegfilt == 7)
		return snprintf(buf, PAGE_SIZE, "1\n");
	else if (avgnegfilt > 0 && avgnegfilt < 7)
		return snprintf(buf, PAGE_SIZE, "1-1/%d\n", 1 << avgnegfilt);
	else if (avgnegfilt == 0)
		return snprintf(buf, PAGE_SIZE, "0\n");

	return snprintf(buf, PAGE_SIZE, "not set\n");
}


static ssize_t sx9385_avgposfilt_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 avgposfilt = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL3_PH2, &avgposfilt);
	avgposfilt = avgposfilt & 0x0F;

	if (avgposfilt == 0x0F)
		return snprintf(buf, PAGE_SIZE, "1\n");
	else if (avgposfilt > 0 && avgposfilt <= 0x0A)
		return snprintf(buf, PAGE_SIZE, "1-1/%d\n", 1 << avgposfilt);
	else
		return snprintf(buf, PAGE_SIZE, "0\n");
}



static ssize_t sx9385_gain_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 msByte = 0;
	u8 dgain = 0;

	sx9385_i2c_read(data, REG_AFE_PARAM2_PH2, &msByte);

	dgain = (msByte >> 5) & 0x07;

	if (dgain > 0 && dgain < 5)
		return snprintf(buf, PAGE_SIZE, "x%u\n", 1 << (dgain - 1));

	return snprintf(buf, PAGE_SIZE, "Reserved\n");
}



static ssize_t sx9385_avgthresh_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 avgthresh = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL1_PH2, &avgthresh);
	avgthresh = avgthresh & 0x3F;

	return snprintf(buf, PAGE_SIZE, "%ld\n", 512 * (long int)avgthresh);
}


static ssize_t sx9385_rawfilt_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 rawfilt = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL0_PH12, &rawfilt);
	rawfilt = rawfilt & 0x07;

	if (rawfilt > 0 && rawfilt < 8)
		return snprintf(buf, PAGE_SIZE, "1-1/%d\n", 1 << rawfilt);
	else
		return snprintf(buf, PAGE_SIZE, "0\n");
}



static ssize_t sx9385_sampling_freq_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 sampling_freq = 0;
	const char *table[16] = {
		"250", "200", "166.67", "142.86", "125", "100",	"83.33", "71.43",
		"62.50", "50", "41.67", "35.71", "27.78", "20.83", "15.62", "7.81"
	};

	sx9385_i2c_read(data, REG_AFE_PARAM0_PH2, &sampling_freq);
	sampling_freq = sampling_freq & 0x0F;

	return snprintf(buf, PAGE_SIZE, "%skHz\n", table[sampling_freq]);
}



static ssize_t sx9385_scan_period_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 scan_period = 0;

	sx9385_i2c_read(data, REG_GNRL_CTRL2, &scan_period);

	scan_period = scan_period & 0x7F;

	return snprintf(buf, PAGE_SIZE, "%ld\n",
			(long int)(((long int)scan_period << 12) / 4000));
}

static ssize_t sx9385_again_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 again = 0;
	const char *table[32] = {
		"+/-0.3875", "+/-0.775", "+/-1.1625", "+/-1.55",
		"+/-1.9375", "+/-2.325", "+/-2.7125", "+/-3.1",
		"+/-3.4875", "+/-3.875", "+/-4.2625", "+/-4.65",
		"+/-5.0375", "+/-5.425", "+/-5.8125", "+/-6.2",
		"+/-6.5875", "+/-6.975", "+/-7.3625", "+/-7.75",
		"+/-8.1375", "+/-8.525", "+/-8.9125", "+/-9.3",
		"+/-9.6875", "+/-10.075", "+/-10.4625", "+/-10.85",
		"+/-11.2375", "+/-11.625", "+/-12.0125", "+/-12.4",
	};

	sx9385_i2c_read(data, REG_AFE_PARAM2_PH2, &again);
	again = again & 0x1F;

	return snprintf(buf, PAGE_SIZE, "%spF\n", table[again]);
}


static ssize_t sx9385_phase_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1\n");
}

static ssize_t sx9385_hysteresis_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	const char *table[4] = {"None", "+/-6%", "+/-12%", "+/-25%"};
	u8 hyst = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL4_PH2, &hyst);
	hyst = (hyst & 0x30) >> 4;

	return snprintf(buf, PAGE_SIZE, "%s\n", table[hyst]);
}

static ssize_t sx9385_resolution_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 resolution = 0;

	sx9385_i2c_read(data, REG_AFE_PARAM0_PH2_MSB, &resolution);
	resolution = (resolution & 0x70) >> 4;

	return snprintf(buf, PAGE_SIZE, "%u\n", 1 << (resolution + 3));
}



static ssize_t sx9385_useful_filt_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 useful_filt = 0;

	sx9385_i2c_read(data, REG_USEFILTER0_PH2, &useful_filt);
	useful_filt = useful_filt & 0x80;

	return snprintf(buf, PAGE_SIZE, "%s\n", useful_filt ? "on" : "off");
}



static ssize_t sx9385_closedeb_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 closedeb = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL4_PH2, &closedeb);
	closedeb = closedeb & 0x0C;
	closedeb = closedeb >> 2;

	if (closedeb == 0)
		return snprintf(buf, PAGE_SIZE, "off");
	return snprintf(buf, PAGE_SIZE, "%d\n", 1 << closedeb);
}



static ssize_t sx9385_fardeb_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 fardeb = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL4_PH2, &fardeb);
	fardeb  = fardeb  & 0x03;

	if (fardeb == 0)
		return snprintf(buf, PAGE_SIZE, "off");
	return snprintf(buf, PAGE_SIZE, "%d\n", 1 << fardeb);
}


static ssize_t sx9385_irq_count_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	int ret = 0;
	s16 max_diff_val = 0;

	if (data->irq_count) {
		ret = -1;
		max_diff_val = data->ch[0]->max_diff;
	} else {
		max_diff_val = data->ch[0]->max_normal_diff;
	}

	GRIP_INFO("called\n");

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
			ret, data->irq_count, max_diff_val);
}

static ssize_t sx9385_irq_count_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t count)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	u8 onoff;
	int ret;

	ret = kstrtou8(buf, 10, &onoff);
	if (ret < 0) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return count;
	}

	mutex_lock(&data->read_mutex);

	if (onoff == 0) {
		data->abnormal_mode = OFF;
	} else if (onoff == 1) {
		data->abnormal_mode = ON;
		data->irq_count = 0;
		data->ch[0]->max_diff = 0;
		data->ch[0]->max_normal_diff = 0;
	} else {
		GRIP_ERR("unknown val %d\n", onoff);
	}

	mutex_unlock(&data->read_mutex);

	GRIP_INFO("%d\n", onoff);

	return count;
}

static ssize_t sx9385_normal_threshold_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 th_buf = 0, hyst = 0;
	u32 threshold = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL5_PH2, &th_buf);
	threshold = (u32)th_buf * (u32)th_buf / 2;

	sx9385_i2c_read(data, REG_PROX_CTRL4_PH2, &hyst);
	hyst = (hyst & 0x30) >> 4;

	switch (hyst) {
	case 0x01: /* 6% */
		hyst = threshold >> 4;
		break;
	case 0x02: /* 12% */
		hyst = threshold >> 3;
		break;
	case 0x03: /* 25% */
		hyst = threshold >> 2;
		break;
	default:
		/* None */
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%lu,%lu\n",
			(u32)threshold + (u32)hyst, (u32)threshold - (u32)hyst);
}


static ssize_t sx9385_onoff_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", !data->skip_data);
}

static ssize_t sx9385_onoff_store(struct device *dev,
				  struct device_attribute *attr, const char *buf, size_t count)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	int ret;
	u8 val;
	int i = 0;

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		GRIP_ERR("kstrtoint fail %d, %s\n", ret, buf);
		return ret;
	}

	if (val == 0) {
		data->skip_data = true;
		for (i = 0; i < data->num_of_channels; i++) {
			if (atomic_read(&data->enable) == ON) {
				input_report_rel(data->input, data->ch[i]->grip_code, GRIP_RELEASE);
				if (data->ch[i]->unknown_sel)
					input_report_rel(data->input, data->ch[i]->unknown_code, UNKNOWN_OFF);
				input_sync(data->input);
			}

			data->motion = 1;
			data->ch[i]->is_unknown_mode = UNKNOWN_OFF;
			data->ch[i]->first_working = false;
		}
	} else {
		data->skip_data = false;
	}

	GRIP_INFO("%u\n", val);
	return count;
}

static ssize_t sx9385_register_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	int idx;
	int i = 0;
	u8 val;
	char *p = buf;
	struct sx9385_p *data = dev_get_drvdata(dev);

	for (idx = 0; idx < data->num_regs; idx++) {
		sx9385_i2c_read(data, data->regs_addr_val[idx].addr, &val);
		i += snprintf(p + i, PAGE_SIZE - i, "(0x%02x)=0x%02x\n", data->regs_addr_val[idx].addr, val);
	}

	return i;
}


static ssize_t sx9385_register_write_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	int reg_address = 0, val = 0;
	struct sx9385_p *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%x,%x", &reg_address, &val) != 2) {
		GRIP_ERR("sscanf err\n");
		return -EINVAL;
	}

	sx9385_i2c_write(data, (unsigned char)reg_address, (unsigned char)val);
	GRIP_INFO("Regi 0x%x Val 0x%x\n", reg_address, val);

	sx9385_get_gain(data);
	sx9385_get_ref_gain(data);

	return count;
}

static unsigned int register_read_store_reg;
static unsigned int register_read_store_val;
static ssize_t sx9385_register_read_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "0x%02x\n", register_read_store_val);
}

static ssize_t sx9385_register_read_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val = 0;
	int regist = 0;
	struct sx9385_p *data = dev_get_drvdata(dev);

	GRIP_INFO("\n");

	if (sscanf(buf, "%x", &regist) != 1) {
		GRIP_ERR("sscanf err\n");
		return -EINVAL;
	}

	sx9385_i2c_read(data, regist, &val);
	GRIP_INFO("Regi 0x%2x Val 0x%2x\n", regist, val);
	register_read_store_reg = (unsigned int)regist;
	register_read_store_val = (unsigned int)val;
	return count;
}

/* show far near status reg data */
static ssize_t sx9385_status_regdata_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	u8 val0;
	struct sx9385_p *data = dev_get_drvdata(dev);

	sx9385_i2c_read(data, REG_PROX_STATUS, &val0);
	GRIP_INFO("Status 0x%2x\n", val0);

	return sprintf(buf, "%d\n", val0);
}

static ssize_t sx9385_status_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	int status = 0;
	struct sx9385_p *data = dev_get_drvdata(dev);

	sx9385_i2c_read(data, REG_PROX_STATUS, &val);

	if ((val & 0x10) == 0)//bit3 for proxstat
		status = 0;
	else
		status = 1;

	return sprintf(buf, "%d\n", status);
}


/* check if manual calibrate success or not */
static ssize_t sx9385_cal_state_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	int status = 0;
	struct sx9385_p *data = dev_get_drvdata(dev);

	sx9385_i2c_read(data, REG_COMPENSATION, &val);
	GRIP_INFO("Regi 0x01 Val 0x%2x\n", val);
	if ((val & 0x0F) == 0)
		status = 1;
	else
		status = 0;

	return sprintf(buf, "%d\n", status);
}

static ssize_t sx9385_motion_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
		data->motion == 1 ? "motion_detect" : "motion_non_detect");
}

static ssize_t sx9385_motion_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct sx9385_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return ret;
	}

	data->motion = val;

	GRIP_INFO("%u\n", val);
	return count;
}

static ssize_t sx9385_unknown_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
		(data->ch[0]->is_unknown_mode == UNKNOWN_ON) ?	"UNKNOWN" : "NORMAL");
}

static ssize_t sx9385_unknown_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct sx9385_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return ret;
	}

	if (val == 1)
		enter_unknown_mode(data, TYPE_FORCE, data->ch[0]);
	else if (val == 0)
		data->ch[0]->is_unknown_mode = UNKNOWN_OFF;
	else
		GRIP_INFO("Invalid Val %u\n", val);

	GRIP_INFO("%u\n", val);

	return count;
}

static ssize_t sx9385_noti_enable_store(struct device *dev,
					 struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	u8 enable;
	struct sx9385_p *data = dev_get_drvdata(dev);
	int i = 0;

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return size;
	}

	GRIP_INFO("new val %d\n", (int)enable);

	for (i = 0; i < data->num_of_channels; i++) {
		data->ch[i]->noti_enable = enable & data->ch[i]->unknown_sel;
		if (data->ch[i]->noti_enable)
			enter_unknown_mode(data, TYPE_BOOT, data->ch[i]);
		else {
			data->motion = 1;
			data->ch[i]->first_working = false;
			data->ch[i]->is_unknown_mode = UNKNOWN_OFF;
		}

	}
	return size;
}

static ssize_t sx9385_noti_enable_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", data->ch[0]->noti_enable);
}

#ifdef CONFIG_MULTI_CHANNEL
static ssize_t sx9385_unknown_state_b_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct sx9385_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return ret;
	}

	if (val == 1)
		enter_unknown_mode(data, TYPE_FORCE, data->ch[1]);
	else if (val == 0)
		data->ch[1]->is_unknown_mode = UNKNOWN_OFF;
	else
		GRIP_INFO("Invalid Val %u\n", val);

	GRIP_INFO("%u\n", val);

	return count;
}

static ssize_t sx9385_raw_data_b_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	static s32 sum_diff, sum_useful;
	struct sx9385_p *data = dev_get_drvdata(dev);

	sx9385_get_data(data);

	if (data->ch[1]->diff_cnt == 0) {
		sum_diff = (s32)data->ch[1]->diff;
		sum_useful = data->ch[1]->useful;
	} else {
		sum_diff += (s32)data->ch[1]->diff;
		sum_useful += data->ch[1]->useful;
	}

	if (++data->ch[1]->diff_cnt >= DIFF_READ_NUM) {
		data->ch[1]->diff_avg = (s16)(sum_diff / DIFF_READ_NUM);
		data->ch[1]->useful_avg = sum_useful / DIFF_READ_NUM;
		data->ch[1]->diff_cnt = 0;
	}

	return snprintf(buf, PAGE_SIZE, "%ld,%ld,%u,%d,%d\n", (long int)data->ch[1]->capMain,
			(long int)data->ch[1]->useful, data->ch[1]->offset, data->ch[1]->diff, data->ch[1]->avg);
}

static ssize_t sx9385_irq_count_b_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	int ret = 0;
	s16 max_diff_val = 0;

	if (data->irq_count) {
		ret = -1;
		max_diff_val = data->ch[1]->max_diff;
	} else {
		max_diff_val = data->ch[1]->max_normal_diff;
	}

	GRIP_INFO("called\n");

	return snprintf(buf, PAGE_SIZE, "%d,%d,%d\n",
			ret, data->irq_count, max_diff_val);
}

static ssize_t sx9385_irq_count_b_store(struct device *dev,
					  struct device_attribute *attr, const char *buf, size_t count)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	u8 onoff;
	int ret;

	ret = kstrtou8(buf, 10, &onoff);
	if (ret < 0) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return count;
	}

	mutex_lock(&data->read_mutex);

	if (onoff == 0) {
		data->abnormal_mode = OFF;
	} else if (onoff == 1) {
		data->abnormal_mode = ON;
		data->irq_count = 0;
		data->ch[1]->max_diff = 0;
		data->ch[1]->max_normal_diff = 0;
	} else {
		GRIP_ERR("unknown val %d\n", onoff);
	}

	mutex_unlock(&data->read_mutex);

	GRIP_INFO("%d\n", onoff);

	return count;
}

static ssize_t sx9385_diff_avg_b_show(struct device *dev,
					struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->ch[1]->diff_avg);
}
static ssize_t sx9385_useful_avg_b_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%ld\n", (long int)data->ch[1]->useful_avg);
}
static ssize_t sx9385_avgnegfilt_b_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 avgnegfilt = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL3_PH4, &avgnegfilt);

	avgnegfilt = (avgnegfilt & 0x70) >> 3;

	if (avgnegfilt == 7)
		return snprintf(buf, PAGE_SIZE, "1\n");
	else if (avgnegfilt > 0 && avgnegfilt < 7)
		return snprintf(buf, PAGE_SIZE, "1-1/%d\n", 1 << avgnegfilt);
	else if (avgnegfilt == 0)
		return snprintf(buf, PAGE_SIZE, "0\n");

	return snprintf(buf, PAGE_SIZE, "not set\n");
}
static ssize_t sx9385_avgposfilt_b_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 avgposfilt = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL3_PH4, &avgposfilt);
	avgposfilt = avgposfilt & 0x0F;

	if (avgposfilt == 0x0F)
		return snprintf(buf, PAGE_SIZE, "1\n");
	else if (avgposfilt > 0 && avgposfilt <= 0x0A)
		return snprintf(buf, PAGE_SIZE, "1-1/%d\n", 1 << avgposfilt);
	else
		return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t sx9385_gain_b_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 msByte = 0;
	u8 dgain = 0;

	sx9385_i2c_read(data, REG_AFE_PARAM2_PH4, &msByte);
	dgain = (msByte >> 5) & 0x07;

	if (dgain > 0 && dgain < 5)
		return snprintf(buf, PAGE_SIZE, "x%u\n", 1 << (dgain - 1));

	return snprintf(buf, PAGE_SIZE, "Reserved\n");
}

static ssize_t sx9385_avgthresh_b_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 avgthresh = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL1_PH4, &avgthresh);
	avgthresh = avgthresh & 0x3F;

	return snprintf(buf, PAGE_SIZE, "%ld\n", 512 * (long int)avgthresh);
}
static ssize_t sx9385_rawfilt_b_show(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 rawfilt = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL0_PH34, &rawfilt);
	rawfilt = rawfilt & 0x07;

	if (rawfilt > 0 && rawfilt < 8)
		return snprintf(buf, PAGE_SIZE, "1-1/%d\n", 1 << rawfilt);
	else
		return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t sx9385_sampling_freq_b_show(struct device *dev,
					 struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 sampling_freq = 0;
	const char *table[16] = {
		"250", "200", "166.67", "142.86", "125", "100",	"83.33", "71.43",
		"62.50", "50", "41.67", "35.71", "27.78", "20.83", "15.62", "7.81"
	};

	sx9385_i2c_read(data, REG_AFE_PARAM0_PH4, &sampling_freq);
	sampling_freq = sampling_freq & 0x0F;

	return snprintf(buf, PAGE_SIZE, "%skHz\n", table[sampling_freq]);
}

static ssize_t sx9385_again_b_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 again = 0;
	const char *table[32] = {
		"+/-0.3875", "+/-0.775", "+/-1.1625", "+/-1.55",
		"+/-1.9375", "+/-2.325", "+/-2.7125", "+/-3.1",
		"+/-3.4875", "+/-3.875", "+/-4.2625", "+/-4.65",
		"+/-5.0375", "+/-5.425", "+/-5.8125", "+/-6.2",
		"+/-6.5875", "+/-6.975", "+/-7.3625", "+/-7.75",
		"+/-8.1375", "+/-8.525", "+/-8.9125", "+/-9.3",
		"+/-9.6875", "+/-10.075", "+/-10.4625", "+/-10.85",
		"+/-11.2375", "+/-11.625", "+/-12.0125", "+/-12.4",
	};

	sx9385_i2c_read(data, REG_AFE_PARAM2_PH4, &again);
	again = again & 0x1F;

	return snprintf(buf, PAGE_SIZE, "%spF\n", table[again]);
}
static ssize_t sx9385_hysteresis_b_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	const char *table[4] = {"None", "+/-6%", "+/-12%", "+/-25%"};
	u8 hyst = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL4_PH4, &hyst);
	hyst = (hyst & 0x30) >> 4;

	return snprintf(buf, PAGE_SIZE, "%s\n", table[hyst]);
}
static ssize_t sx9385_resolution_b_show(struct device *dev,
					  struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 resolution = 0;

	sx9385_i2c_read(data, REG_AFE_PARAM0_PH4_MSB, &resolution);
	resolution = resolution & 0x70 >> 4;

	return snprintf(buf, PAGE_SIZE, "%u\n", 1 << (resolution + 3));
}

static ssize_t sx9385_useful_filt_b_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 useful_filt = 0;

	sx9385_i2c_read(data, REG_USEFILTER0_PH4, &useful_filt);
	useful_filt = useful_filt & 0x80;

	return snprintf(buf, PAGE_SIZE, "%s\n", useful_filt ? "on" : "off");
}

static ssize_t sx9385_closedeb_b_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 closedeb = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL4_PH4, &closedeb);
	closedeb = closedeb & 0x0C;
	closedeb = closedeb >> 2;

	if (closedeb == 0)
		return snprintf(buf, PAGE_SIZE, "off");
	return snprintf(buf, PAGE_SIZE, "%d\n", 1 << closedeb);
}

static ssize_t sx9385_fardeb_b_show(struct device *dev,
					   struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 fardeb = 0;

	sx9385_i2c_read(data, REG_PROX_CTRL4_PH4, &fardeb);
	fardeb  = fardeb  & 0x03;

	if (fardeb == 0)
		return snprintf(buf, PAGE_SIZE, "off");
	return snprintf(buf, PAGE_SIZE, "%d\n", 1 << fardeb);
}

static ssize_t sx9385_normal_threshold_b_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	u8 th_buf = 0, hyst = 0;
	u32 threshold = 0;
	int ret = 0;

	ret = sx9385_i2c_read(data, REG_PROX_CTRL5_PH4, &th_buf);
	if (ret < 0) {
		GRIP_ERR("detect threshold update fail\n");
	} else {
		ret = sx9385_i2c_read(data, REG_PROX_CTRL4_PH4, &hyst);
		if (ret < 0)
			GRIP_ERR("detect threshold update2 fail\n");
	}

	sx9385_i2c_read(data, REG_PROX_CTRL5_PH4, &th_buf);
	threshold = (u32)th_buf * (u32)th_buf / 2;

	sx9385_i2c_read(data, REG_PROX_CTRL4_PH4, &hyst);
	hyst = (hyst & 0x30) >> 4;

	switch (hyst) {
	case 0x01: /* 6% */
		hyst = threshold >> 4;
		break;
	case 0x02: /* 12% */
		hyst = threshold >> 3;
		break;
	case 0x03: /* 25% */
		hyst = threshold >> 2;
		break;
	default:
		/* None */
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%lu,%lu\n",
			(u32)threshold + (u32)hyst, (u32)threshold - (u32)hyst);
}

static ssize_t sx9385_status_b_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	int status = 0;
	struct sx9385_p *data = dev_get_drvdata(dev);

	sx9385_i2c_read(data, REG_PROX_STATUS, &val);

	if ((val & 0x20) == 0)//bit3 for proxstat
		status = 0;
	else
		status = 1;

	return sprintf(buf, "%d\n", status);
}

static ssize_t sx9385_unknown_state_b_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%s\n",
		(data->ch[1]->is_unknown_mode == UNKNOWN_ON) ?	"UNKNOWN" : "NORMAL");
}
#endif
static DEVICE_ATTR(grip_ref_cap, 0444, sx9385_ref_cap_show, NULL);
static DEVICE_ATTR(grip_ref_cap_b, 0444, sx9385_ref_cap_b_show, NULL);
static DEVICE_ATTR(regproxdata, 0444, sx9385_status_regdata_show, NULL);
static DEVICE_ATTR(proxstatus, 0444, sx9385_status_show, NULL);
static DEVICE_ATTR(cal_state, 0444, sx9385_cal_state_show, NULL);
static DEVICE_ATTR(menual_calibrate, 0664,
		   sx9385_get_offset_calibration_show,
		   sx9385_set_offset_calibration_store);
static DEVICE_ATTR(register_write, 0220, NULL, sx9385_register_write_store);
static DEVICE_ATTR(register_read_all, 0444, sx9385_register_show, NULL);
static DEVICE_ATTR(register_read, 0664,
			sx9385_register_read_show, sx9385_register_read_store);
static DEVICE_ATTR(reset, 0444, sx9385_sw_reset_show, NULL);
static DEVICE_ATTR(name, 0444, sx9385_name_show, NULL);
static DEVICE_ATTR(vendor, 0444, sx9385_vendor_show, NULL);
static DEVICE_ATTR(mode, 0444, sx9385_touch_mode_show, NULL);
static DEVICE_ATTR(raw_data, 0444, sx9385_raw_data_show, NULL);
static DEVICE_ATTR(diff_avg, 0444, sx9385_diff_avg_show, NULL);
static DEVICE_ATTR(useful_avg, 0444, sx9385_useful_avg_show, NULL);
static DEVICE_ATTR(onoff, 0664, sx9385_onoff_show, sx9385_onoff_store);
static DEVICE_ATTR(normal_threshold, 0444, sx9385_normal_threshold_show, NULL);
static DEVICE_ATTR(avg_negfilt, 0444, sx9385_avgnegfilt_show, NULL);
static DEVICE_ATTR(avg_posfilt, 0444, sx9385_avgposfilt_show, NULL);
static DEVICE_ATTR(avg_thresh, 0444, sx9385_avgthresh_show, NULL);
static DEVICE_ATTR(rawfilt, 0444, sx9385_rawfilt_show, NULL);
static DEVICE_ATTR(sampling_freq, 0444, sx9385_sampling_freq_show, NULL);
static DEVICE_ATTR(scan_period, 0444, sx9385_scan_period_show, NULL);
static DEVICE_ATTR(gain, 0444, sx9385_gain_show, NULL);
static DEVICE_ATTR(analog_gain, 0444, sx9385_again_show, NULL);
static DEVICE_ATTR(phase, 0444, sx9385_phase_show, NULL);
static DEVICE_ATTR(hysteresis, 0444, sx9385_hysteresis_show, NULL);
static DEVICE_ATTR(irq_count, 0664, sx9385_irq_count_show, sx9385_irq_count_store);
static DEVICE_ATTR(resolution, 0444, sx9385_resolution_show, NULL);
static DEVICE_ATTR(useful_filt, 0444, sx9385_useful_filt_show, NULL);
//static DEVICE_ATTR(resistor_filter_input, S_IRUGO, sx9385_resistor_filter_input_show, NULL);
static DEVICE_ATTR(closedeb, 0444, sx9385_closedeb_show, NULL);
static DEVICE_ATTR(fardeb, 0444, sx9385_fardeb_show, NULL);
static DEVICE_ATTR(motion, 0664, sx9385_motion_show, sx9385_motion_store);
static DEVICE_ATTR(unknown_state, 0664, sx9385_unknown_state_show, sx9385_unknown_state_store);
static DEVICE_ATTR(noti_enable, 0664, sx9385_noti_enable_show, sx9385_noti_enable_store);
#ifdef CONFIG_MULTI_CHANNEL
static DEVICE_ATTR(avg_negfilt_b, 0444, sx9385_avgnegfilt_b_show, NULL);
static DEVICE_ATTR(avg_posfilt_b, 0444, sx9385_avgposfilt_b_show, NULL);
static DEVICE_ATTR(avg_thresh_b, 0444, sx9385_avgthresh_b_show, NULL);
static DEVICE_ATTR(rawfilt_b, 0444, sx9385_rawfilt_b_show, NULL);
static DEVICE_ATTR(sampling_freq_b, 0444, sx9385_sampling_freq_b_show, NULL);
static DEVICE_ATTR(gain_b, 0444, sx9385_gain_b_show, NULL);
static DEVICE_ATTR(analog_gain_b, 0444, sx9385_again_b_show, NULL);

static DEVICE_ATTR(hysteresis_b, 0444, sx9385_hysteresis_b_show, NULL);
static DEVICE_ATTR(irq_count_b, 0664, sx9385_irq_count_b_show, sx9385_irq_count_b_store);
static DEVICE_ATTR(resolution_b, 0444, sx9385_resolution_b_show, NULL);
static DEVICE_ATTR(useful_filt_b, 0444, sx9385_useful_filt_b_show, NULL);
static DEVICE_ATTR(closedeb_b, 0444, sx9385_closedeb_b_show, NULL);
static DEVICE_ATTR(fardeb_b, 0444, sx9385_fardeb_b_show, NULL);
static DEVICE_ATTR(unknown_state_b, 0664, sx9385_unknown_state_b_show, sx9385_unknown_state_b_store);
static DEVICE_ATTR(proxstatus_b, 0444, sx9385_status_b_show, NULL);
static DEVICE_ATTR(raw_data_b, 0444, sx9385_raw_data_b_show, NULL);
static DEVICE_ATTR(diff_avg_b, 0444, sx9385_diff_avg_b_show, NULL);
static DEVICE_ATTR(useful_avg_b, 0444, sx9385_useful_avg_b_show, NULL);
static DEVICE_ATTR(normal_threshold_b, 0444, sx9385_normal_threshold_b_show, NULL);
#endif

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_grip_ref_cap,
	&dev_attr_grip_ref_cap_b,
	&dev_attr_regproxdata,
	&dev_attr_proxstatus,
	&dev_attr_cal_state,
	&dev_attr_menual_calibrate,
	&dev_attr_register_write,
	&dev_attr_register_read,
	&dev_attr_register_read_all,
	&dev_attr_reset,
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_mode,
	&dev_attr_raw_data,
	&dev_attr_diff_avg,
	&dev_attr_useful_avg,
	&dev_attr_onoff,
	&dev_attr_normal_threshold,
	&dev_attr_avg_negfilt,
	&dev_attr_avg_posfilt,
	&dev_attr_avg_thresh,
	&dev_attr_rawfilt,
	&dev_attr_sampling_freq,
	&dev_attr_scan_period,
	&dev_attr_gain,
	&dev_attr_analog_gain,
	&dev_attr_phase,
	&dev_attr_hysteresis,
	&dev_attr_irq_count,
	&dev_attr_resolution,
	&dev_attr_useful_filt,
//	&dev_attr_resistor_filter_input,
	&dev_attr_closedeb,
	&dev_attr_fardeb,
	&dev_attr_motion,
	&dev_attr_unknown_state,
	&dev_attr_noti_enable,
	NULL,
};

#ifdef CONFIG_MULTI_CHANNEL
static struct device_attribute *multi_sensor_attrs[] = {
	&dev_attr_avg_negfilt_b,
	&dev_attr_avg_posfilt_b,
	&dev_attr_avg_thresh_b,
	&dev_attr_rawfilt_b,
	&dev_attr_sampling_freq_b,
	&dev_attr_gain_b,
	&dev_attr_analog_gain_b,
	&dev_attr_hysteresis_b,
	&dev_attr_irq_count_b,
	&dev_attr_resolution_b,
	&dev_attr_useful_filt_b,
	&dev_attr_closedeb_b,
	&dev_attr_fardeb_b,
	&dev_attr_unknown_state_b,
	&dev_attr_proxstatus_b,
	&dev_attr_raw_data_b,
	&dev_attr_diff_avg_b,
	&dev_attr_useful_avg_b,
	&dev_attr_normal_threshold_b,
	NULL,
};
#endif

static ssize_t sx9385_enable_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&data->enable));
}

static ssize_t sx9385_enable_store(struct device *dev,
				   struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct sx9385_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		GRIP_ERR("kstrtou8 fail %d\n", ret);
		return ret;
	}

	GRIP_INFO("new val %u\n", enable);
	if ((enable == 0) || (enable == 1))
		sx9385_set_enable(data, (int)enable);
	return size;
}

static DEVICE_ATTR(enable, 0664, sx9385_enable_show, sx9385_enable_store);

static struct attribute *sx9385_attributes[] = {
	&dev_attr_enable.attr,
	NULL
};

static struct attribute_group sx9385_attribute_group = {
	.attrs = sx9385_attributes
};

static void sx9385_check_diff_overflow(struct sx9385_p *data, int ch_num)
{
	u8 reg_dif_msb[] = {0xCB, 0xDE};
	u8 msb, lsb;
	s16 dif[NUM_MAIN_PHASES];

	sx9385_i2c_read(data, reg_dif_msb[ch_num] + 1, &lsb);
	sx9385_i2c_read(data, reg_dif_msb[ch_num], &msb);
	dif[ch_num] = msb<<8 | lsb;

	if (dif[ch_num] == DIFF_OVERFLOW) {
		enter_unknown_mode(data, TYPE_OVERFLOW, data->ch[ch_num]);
	}
}

static void sx9385_touch_process_ch(struct sx9385_p *data, struct channel *ch, u8 status)
{
	if (data->abnormal_mode) {
		if (status & ch->prox_mask) {
			if (ch->max_diff < ch->diff)
				ch->max_diff = ch->diff;
			data->irq_count++;
		}
	}

	if (ch->state == IDLE) {
		if (status & ch->prox_mask) {
			if (ch->is_unknown_mode == UNKNOWN_ON && data->motion)
				ch->first_working = true;
			sx9385_send_event(data, ACTIVE, ch);
		} else {
			sx9385_check_diff_overflow(data, ch->num);
			GRIP_INFO("0x%x already released.\n", status);
		}
	} else { /* User released button */
		if (!(status & ch->prox_mask)) {
			if (ch->is_unknown_mode == UNKNOWN_ON && data->motion) {
				GRIP_INFO("unknown mode off\n");
				ch->is_unknown_mode = UNKNOWN_OFF;
			}
			sx9385_check_diff_overflow(data, ch->num);
			sx9385_send_event(data, IDLE, ch);
		} else {
			GRIP_INFO("0x%x still touched\n", status);
		}
	}

	/* Test Code */
	if (ch->state == HAS_ERROR)
		status = IDLE;
	else
		status = ch->state;

	check_irq_error(data, status, BY_INTERRUPT_HANDLER, ch);
}

static void sx9385_touch_process(struct sx9385_p *data)
{
	u8 status = 0;
	u8 status_dif = 0;
	int i = 0;

	sx9385_i2c_read(data, REG_PROX_STATUS, &status);

	GRIP_INFO("0x%x\n", status);

	sx9385_get_data(data);
	status_dif = data->prev_status ^ status;

	for (i = 0; i < data->num_of_channels; i++) {
		if (status_dif & data->ch[i]->prox_mask)
			sx9385_touch_process_ch(data, data->ch[i], status);
	}
}

static void sx9385_process_interrupt(struct sx9385_p *data)
{
	int status = 0;
	/* since we are not in an interrupt don't need to disable irq. */
	status = sx9385_read_reg_stat(data);

	GRIP_INFO("status %d\n", status);

	if (status & IRQ_PROCESS_CONDITION)
		sx9385_touch_process(data);
}

static void sx9385_init_work_func(struct work_struct *work)
{
	struct sx9385_p *data = container_of((struct delayed_work *)work,
						 struct sx9385_p, init_work);
	int ret = 0;

	if (data && !data->check_abnormal_working) {
		GRIP_INFO("initialize\n");

		sx9385_initialize_register(data);

		sx9385_set_mode(data, SX9385_MODE_NORMAL);

		if (!data->check_abnormal_working) {
			msleep(20);
			sx9385_manual_offset_calibration(data);
		}

		ret = sx9385_read_reg_stat(data);
		if (ret)
			GRIP_ERR("read reg stat : %d\n", ret);
		sx9385_get_gain(data);
	}
}

static void sx9385_irq_work_func(struct work_struct *work)
{
	struct sx9385_p *data = container_of((struct delayed_work *)work,
						 struct sx9385_p, irq_work);

	if (sx9385_get_nirq_state(data) == INTERRUPT_LOW)
		sx9385_process_interrupt(data);
	else
		GRIP_ERR("nirq read high %d\n", sx9385_get_nirq_state(data));
}

static void sx9385_check_first_working(struct sx9385_p *data, struct channel *ch)
{
	if (ch->noti_enable && data->motion) {
		if (ch->detect_threshold < ch->diff) {
			ch->first_working = true;
			GRIP_INFO("first working detected %d\n", ch->diff);
		} else if (ch->release_threshold > ch->diff) {
			if (ch->first_working) {
				ch->is_unknown_mode = UNKNOWN_OFF;
				GRIP_INFO("Release detected %d, unknown mode off\n", ch->diff);
			}
		}
	}
}

static void sx9385_debug_work_func(struct work_struct *work)
{
	struct sx9385_p *data = container_of((struct delayed_work *)work,
						 struct sx9385_p, debug_work);
	int ret = 0;
	int i = 0;

	if ((atomic_read(&data->enable) == ON)
			&& (data->abnormal_mode)) {
		sx9385_get_data(data);
		for (i = 0; i < data->num_of_channels; i++) {
			if (data->ch[i]->max_normal_diff < data->ch[i]->diff)
				data->ch[i]->max_normal_diff = data->ch[i]->diff;
		}
	} else if (data->debug_count >= GRIP_LOG_TIME) {
		sx9385_get_data(data);
		for (i = 0; i < data->num_of_channels; i++) {
			if (data->ch[i]->is_unknown_mode == UNKNOWN_ON && data->motion)
				sx9385_check_first_working(data, data->ch[i]);
		}
		data->debug_count = 0;
	} else {
		data->debug_count++;
	}

	if (data->fail_status_code != 0)
		GRIP_ERR("last err %d", data->fail_status_code);

	if (atomic_read(&data->enable) == ON) {
		u8 buf = 0;

		ret = sx9385_i2c_read(data, REG_PROX_STATUS, &buf);
		if (ret < 0)
			return;

		for (i = 0; i < data->num_of_channels; i++) {
			u8 status;
			status = buf & (data->ch[i]->prox_mask);
			if (status)
				status = ACTIVE;
			else
				status = IDLE;
			check_irq_error(data, status, BY_DEBUG_WORK, data->ch[i]);
		}
	}
	schedule_delayed_work(&data->debug_work, msecs_to_jiffies(2000));
}

static irqreturn_t sx9385_interrupt_thread(int irq, void *pdata)
{
	struct sx9385_p *data = pdata;

	__pm_wakeup_event(data->grip_ws, jiffies_to_msecs(3 * HZ));
	schedule_delayed_work(&data->irq_work, msecs_to_jiffies(0));

	return IRQ_HANDLED;
}

static int sx9385_input_init(struct sx9385_p *data)
{
	int ret = 0;
	int i = 0;
	unsigned int grip_code[2] = {REL_MISC, REL_DIAL};
	unsigned int unknown_code[2] = {REL_X, REL_Y};
	struct input_dev *dev = NULL;

	/* Create the input device */
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name =  module_name[data->ic_num];
	dev->id.bustype = BUS_I2C;

	for (i = 0; i < data->num_of_channels; i++) {
		input_set_capability(dev, EV_REL, grip_code[i]);
		input_set_capability(dev, EV_REL, unknown_code[i]);
	}

	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		return ret;
	}

#if IS_ENABLED(CONFIG_SENSORS_CORE_AP)
	ret = sensors_create_symlink(&dev->dev.kobj, dev->name);
#else
	ret = sensors_create_symlink(dev);
#endif
	if (ret < 0) {
		GRIP_ERR("fail to create symlink %d\n", ret);
		input_unregister_device(dev);
		return ret;
	}

	ret = sysfs_create_group(&dev->dev.kobj, &sx9385_attribute_group);
	if (ret < 0) {
		GRIP_ERR("fail to create sysfs group %d\n", ret);
#if IS_ENABLED(CONFIG_SENSORS_CORE_AP)
		sensors_remove_symlink(&dev->dev.kobj, dev->name);
#else
		sensors_remove_symlink(dev);
#endif
		input_unregister_device(dev);
		return ret;
	}

	/* save the input pointer and finish initialization */
	data->input = dev;

	return 0;
}

static int sx9385_noti_input_init(struct sx9385_p *data)
{
	int ret = 0;
	struct input_dev *noti_input_dev = NULL;
	int i = 0;

	for (i = 0; i < data->num_of_channels; i++)
		data->ch[i]->unknown_sel = (data->unknown >> i) & 0x01;

	if (data->unknown) {
		/* Create the input device */
		noti_input_dev = input_allocate_device();
		if (!noti_input_dev) {
			GRIP_ERR("input_allocate_device fail\n");
			return -ENOMEM;
		}

		noti_input_dev->name = NOTI_MODULE_NAME;
		noti_input_dev->id.bustype = BUS_I2C;

		input_set_capability(noti_input_dev, EV_REL, REL_X);
		input_set_drvdata(noti_input_dev, data);

		ret = input_register_device(noti_input_dev);
		if (ret < 0) {
			GRIP_ERR("fail to regi input dev for noti %d\n", ret);
			input_free_device(noti_input_dev);
			return ret;
		}

		data->noti_input_dev = noti_input_dev;
	}

	return 0;
}

static int sx9385_setup_pin(struct sx9385_p *data)
{
	int ret;

	ret = gpio_request(data->gpio_nirq, "SX9385_nIRQ");
	if (ret < 0) {
		GRIP_ERR("gpio %d req fail %d\n", data->gpio_nirq, ret);
		return ret;
	}

	ret = gpio_direction_input(data->gpio_nirq);
	if (ret < 0) {
		GRIP_ERR("fail to set gpio %d as input %d\n", data->gpio_nirq, ret);
		gpio_free(data->gpio_nirq);
		return ret;
	}

	return 0;
}

static void sx9385_initialize_variable(struct sx9385_p *data)
{
	int i = 0;
	unsigned int grip_code[2] = {REL_MISC, REL_DIAL};
	unsigned int unknown_code[2] = {REL_X, REL_Y};

	data->init_done = OFF;
	data->skip_data = false;
	data->fail_status_code = 0;
	data->pre_attach = -1;
	data->motion = 1;

	for (i = 0; i < data->num_of_channels; i++) {
		data->ch[i]->num = i;
		data->ch[i]->grip_code = grip_code[i];
		data->ch[i]->unknown_code = unknown_code[i];
		data->ch[i]->is_unknown_mode = UNKNOWN_OFF;
		data->ch[i]->first_working = false;
		data->ch[i]->state = IDLE;
		data->ch[i]->prox_mask = 1 << (4 + i);
	}
	atomic_set(&data->enable, OFF);
}

static int sx9385_parse_dt(struct sx9385_p *data, struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;
	int ret = 0;

	if (dNode == NULL)
		return -ENODEV;

	data->gpio_nirq = of_get_named_gpio_flags(dNode,
						  "sx9385,nirq-gpio", 0, &flags);

	if (data->gpio_nirq < 0) {
		GRIP_ERR("get gpio_nirq err\n");
		return -ENODEV;
	}
	GRIP_INFO("get gpio_nirq %d\n", data->gpio_nirq);

	ret = of_property_read_u32(dNode, "sx9385,unknown_sel", &data->unknown);
	GRIP_INFO("unknown_sel %d\n", data->unknown);
	if (ret < 0) {
		GRIP_ERR("unknown_sel read fail %d\n", ret);
		data->unknown = 3;
	}
	//load register settings
	ret = of_property_read_u32(dNode, "sx9385,num_of_channels", &data->num_of_channels);
	if (ret < 0) {
		GRIP_ERR("get num_of_channels fail %d\n", ret);
		data->num_of_channels = 1;
	}
	ret = of_property_read_u32(dNode, "sx9385,num_of_refs", &data->num_of_refs);
	if (ret < 0) {
		GRIP_ERR("get num_of_refs fail %d\n", ret);
		data->num_of_refs = 1;
	}

	GRIP_INFO("number of channel= %d", data->num_of_channels);

	//load register settings
	of_property_read_u32(dNode, "sx9385,reg-num", &data->num_regs);
	GRIP_INFO("number of registers= %d", data->num_regs);

	if (unlikely(data->num_regs <= 0)) {
		GRIP_ERR("Invalid reg_num= %d", data->num_regs);
		return -EINVAL;
	} else {
		// initialize platform reg data array
		data->regs_addr_val = kzalloc(sizeof(struct reg_addr_val_s) * data->num_regs, GFP_KERNEL);
		if (unlikely(!data->regs_addr_val)) {
			GRIP_ERR("Failed to alloc memory, num_reg= %d", data->num_regs);
			return -ENOMEM;
		}

		if (of_property_read_u8_array(dNode, "sx9385,reg-init",
								(u8 *)data->regs_addr_val,
								data->num_regs * 2)) {
			GRIP_ERR("Failed to load registers from the dts");
			return -EINVAL;
		}
	}
	return 0;
}

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
static int sx9385_pdic_handle_notification(struct notifier_block *nb,
					   unsigned long action, void *pdic_data)
{
	PD_NOTI_ATTACH_TYPEDEF usb_typec_info = *(PD_NOTI_ATTACH_TYPEDEF *)pdic_data;
	struct sx9385_p *data = container_of(nb, struct sx9385_p, pdic_nb);
	int i = 0;

	if (usb_typec_info.id != PDIC_NOTIFY_ID_ATTACH)
		return 0;

	if (data->pre_attach == usb_typec_info.attach)
		return 0;

	GRIP_INFO("src %d id %d attach %d rprd %d\n",
		usb_typec_info.src, usb_typec_info.id, usb_typec_info.attach, usb_typec_info.rprd);

	if (data->init_done == ON) {
		for (i = 0; i < data->num_of_channels; i++)
			enter_unknown_mode(data, TYPE_USB, data->ch[i]);
		sx9385_manual_offset_calibration(data);
	}

	data->pre_attach = usb_typec_info.attach;

	return 0;
}
#endif

#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
static int sx9385_hall_notifier(struct notifier_block *nb,
				unsigned long action, void *hall_data)
{
	struct hall_notifier_context *hall_notifier;
	struct sx9385_p *data =
			container_of(nb, struct sx9385_p, hall_nb);
	int i = 0;

	hall_notifier = hall_data;

	if (action == HALL_ATTACH) {
		GRIP_INFO("%s attach\n", hall_notifier->name);
		for (i = 0; i < data->num_of_channels; i++)
			enter_unknown_mode(data, TYPE_HALL, data->ch[i]);
		sx9385_manual_offset_calibration(data);
	} else {
		for (i = 0; i < data->num_of_channels; i++)
			enter_unknown_mode(data, TYPE_HALL, data->ch[i]);
		return 0;
	}

	return 0;
}
#endif

static int sx9385_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct sx9385_p *data = NULL;
	enum ic_num ic_num = 0;
	int i = 0;
	struct device_attribute *sensor_attributes[SENSOR_ATTR_SIZE];

	ic_num = (enum ic_num) of_device_get_match_data(&client->dev);
	pr_info("[GRIP_%s] %s start 0x%x\n", grip_name[ic_num], __func__, client->addr);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_info("[GRIP_%s] i2c func err\n", grip_name[ic_num]);
		goto exit;
	}

	data = kzalloc(sizeof(struct sx9385_p), GFP_KERNEL);
	if (data == NULL) {
		pr_info("[GRIP_%s] Fail to mem alloc\n", grip_name[ic_num]);
		ret = -ENOMEM;
		goto exit_kzalloc;
	}

	data->ic_num = ic_num;
	i2c_set_clientdata(client, data);
	data->client = client;
	data->factory_device = &client->dev;
	data->grip_ws = wakeup_source_register(&client->dev, "grip_wake_lock");
	mutex_init(&data->mode_mutex);
	mutex_init(&data->read_mutex);

	ret = sx9385_parse_dt(data, &client->dev);
	if (ret < 0) {
		GRIP_ERR("of_node err\n");
		ret = -ENODEV;
		goto exit_of_node;
	}

	for (i = 0; i < data->num_of_channels; i++) {
		data->ch[i] = kzalloc(sizeof(struct channel), GFP_KERNEL);
		if (data->ch[i] == NULL) {
			pr_info("[GRIP_%s] Fail to channel mem alloc\n", grip_name[ic_num]);
			ret = -ENOMEM;
			goto exit_channel_kmalloc;
		}
	}
	ret = sx9385_input_init(data);
	if (ret < 0)
		goto exit_input_init;

	ret = sx9385_noti_input_init(data);
	if (ret < 0)
		goto exit_noti_input_init;

	ret = sx9385_setup_pin(data);
	if (ret) {
		GRIP_ERR("could not setup pin\n");
		goto exit_setup_pin;
	}

	/* read chip id */
	ret = sx9385_hardware_check(data);
	if (ret < 0) {
		GRIP_ERR("chip id check fail %d\n", ret);
		goto exit_chip_reset;
	}

	sx9385_initialize_variable(data);
	INIT_DELAYED_WORK(&data->init_work, sx9385_init_work_func);
	INIT_DELAYED_WORK(&data->irq_work, sx9385_irq_work_func);
	INIT_DELAYED_WORK(&data->debug_work, sx9385_debug_work_func);

	data->irq = gpio_to_irq(data->gpio_nirq);
	/* initailize interrupt reporting */
	ret = request_threaded_irq(data->irq, NULL, sx9385_interrupt_thread,
				   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					device_name[data->ic_num], data);
	if (ret < 0) {
		GRIP_ERR("fail to req thread irq %d ret %d\n", data->irq, ret);
		goto exit_request_threaded_irq;
	}
	disable_irq(data->irq);
	data->is_irq_active = false;

	memcpy(sensor_attributes, sensor_attrs, sizeof(sensor_attrs));
	if (data->num_of_channels == 2) {
		int multi_sensor_attrs_size = sizeof(multi_sensor_attrs) / sizeof(ssize_t *);
		int grip_sensor_attr_size = sizeof(sensor_attrs) / sizeof(ssize_t *);

		if (SENSOR_ATTR_SIZE <	multi_sensor_attrs_size + grip_sensor_attr_size) {
			GRIP_ERR("fail %d, %d\n", multi_sensor_attrs_size, grip_sensor_attr_size);
			goto err_sysfs_group;
		}
		memcpy(sensor_attributes + grip_sensor_attr_size - 1, multi_sensor_attrs, sizeof(multi_sensor_attrs));
	}

	ret = sensors_register(&data->factory_device, data, sensor_attributes, (char *)module_name[data->ic_num]);
	if (ret) {
		GRIP_ERR("could not regi sensor %d\n", ret);
		goto exit_register_failed;
	}

	schedule_delayed_work(&data->init_work, msecs_to_jiffies(300));
	sx9385_set_debug_work(data, ON, 20000);

#if IS_ENABLED(CONFIG_PDIC_NOTIFIER) && IS_ENABLED(CONFIG_USB_TYPEC_MANAGER_NOTIFIER)
	GRIP_INFO("regi pdic notifier\n");
	manager_notifier_register(&data->pdic_nb,
				  sx9385_pdic_handle_notification,
				  MANAGER_NOTIFY_PDIC_SENSORHUB);
#endif
#if IS_ENABLED(CONFIG_HALL_NOTIFIER)
		GRIP_INFO("regi hall notifier\n");
		data->hall_nb.priority = 1;
		data->hall_nb.notifier_call = sx9385_hall_notifier;
		hall_notifier_register(&data->hall_nb);
#endif

	GRIP_INFO("done!\n");

	return 0;
exit_register_failed:
	sensors_unregister(data->factory_device, sensor_attrs);
err_sysfs_group:
	free_irq(data->irq, data);
exit_request_threaded_irq:
exit_chip_reset:
	gpio_free(data->gpio_nirq);
exit_setup_pin:
	if (data->unknown)
		input_unregister_device(data->noti_input_dev);
exit_noti_input_init:
	sysfs_remove_group(&data->input->dev.kobj, &sx9385_attribute_group);
#if IS_ENABLED(CONFIG_SENSORS_CORE_AP)
	sensors_remove_symlink(&data->input->dev.kobj, data->input->name);
#else
	sensors_remove_symlink(data->input);
#endif
	input_unregister_device(data->input);
exit_input_init:
exit_channel_kmalloc:
	for (i = 0; i < data->num_of_channels; i++) {
		if (data->ch[i] != NULL)
			kfree(data->ch[i]);
	}
exit_of_node:
	mutex_destroy(&data->mode_mutex);
	mutex_destroy(&data->read_mutex);
	wakeup_source_unregister(data->grip_ws);
	kfree(data);
exit_kzalloc:
exit:
	pr_err("[GRIP_%s] Probe fail!\n", grip_name[ic_num]);
	return ret;

}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
void sx9385_remove(struct i2c_client *client)
#else
static int sx9385_remove(struct i2c_client *client)
#endif
{
	struct sx9385_p *data = (struct sx9385_p *)i2c_get_clientdata(client);

	if (atomic_read(&data->enable) == ON)
		sx9385_set_enable(data, OFF);

	sx9385_set_mode(data, SX9385_MODE_SLEEP);

	cancel_delayed_work_sync(&data->init_work);
	cancel_delayed_work_sync(&data->irq_work);
	cancel_delayed_work_sync(&data->debug_work);
	free_irq(data->irq, data);
	gpio_free(data->gpio_nirq);

	wakeup_source_unregister(data->grip_ws);
	sensors_unregister(data->factory_device, sensor_attrs);
#if IS_ENABLED(CONFIG_SENSORS_CORE_AP)
	sensors_remove_symlink(&data->input->dev.kobj, data->input->name);
#else
	sensors_remove_symlink(data->input);
#endif
	sysfs_remove_group(&data->input->dev.kobj, &sx9385_attribute_group);
	input_unregister_device(data->input);
	input_unregister_device(data->noti_input_dev);
	mutex_destroy(&data->mode_mutex);
	mutex_destroy(&data->read_mutex);

	kfree(data);

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	return;
#else
	return 0;
#endif
}

static int sx9385_suspend(struct device *dev)
{
	struct sx9385_p *data = dev_get_drvdata(dev);
	int cnt = 0;

	GRIP_INFO("\n");
	/* before go to sleep, make the interrupt pin as high*/
	while ((sx9385_get_nirq_state(data) == INTERRUPT_LOW) && (cnt++ < 3)) {
		sx9385_read_reg_stat(data);
		msleep(20);
	}
	if (cnt >= 3)
		GRIP_ERR("s/w reset fail(%d)\n", cnt);

	sx9385_set_debug_work(data, OFF, 1000);

	return 0;
}
static int sx9385_resume(struct device *dev)
{
	struct sx9385_p *data = dev_get_drvdata(dev);

	GRIP_INFO("\n");
	sx9385_set_debug_work(data, ON, 1000);

	return 0;
}

static void sx9385_shutdown(struct i2c_client *client)
{
	struct sx9385_p *data = i2c_get_clientdata(client);

	GRIP_INFO("\n");
	sx9385_set_debug_work(data, OFF, 1000);
	if (atomic_read(&data->enable) == ON)
		sx9385_set_enable(data, OFF);

	sx9385_set_mode(data, SX9385_MODE_SLEEP);
}

static const struct of_device_id sx9385_match_table[] = {
#if IS_ENABLED(CONFIG_SENSORS_SX9385)
	{ .compatible = "sx9385", .data = (void *)MAIN_GRIP},
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9385_SUB)
	{ .compatible = "sx9385_sub", .data = (void *)SUB_GRIP},
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9385_SUB2)
	{ .compatible = "sx9385_sub2", .data = (void *)SUB2_GRIP},
#endif
#if IS_ENABLED(CONFIG_SENSORS_SX9385_WIFI)
	{ .compatible = "sx9385_wifi", .data = (void *)WIFI_GRIP},
#endif
	{},
};

static const struct i2c_device_id sx9385_id[] = {
	{ "SX9385", 0 },
	{ }
};

static const struct dev_pm_ops sx9385_pm_ops = {
	.suspend = sx9385_suspend,
	.resume = sx9385_resume,
};

static struct i2c_driver sx9385_driver = {
	.driver = {
		.name	= "SX9385",
		.owner	= THIS_MODULE,
		.of_match_table = sx9385_match_table,
		.pm = &sx9385_pm_ops
	},
	.probe		= sx9385_probe,
	.remove		= sx9385_remove,
	.shutdown	= sx9385_shutdown,
	.id_table	= sx9385_id,
};

static int __init sx9385_init(void)
{
	int ret = 0;
	pr_info("[GRIP] sx9385_driver init\n");

	ret = i2c_add_driver(&sx9385_driver);
	if (ret != 0)
		pr_err("[GRIP] sx9385_driver probe fail\n");
	return ret;

}

static void __exit sx9385_exit(void)
{
	i2c_del_driver(&sx9385_driver);
}

module_init(sx9385_init);
module_exit(sx9385_exit);

MODULE_DESCRIPTION("Semtech Corp. SX9380 Capacitive Touch Controller Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
