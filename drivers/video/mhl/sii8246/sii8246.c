/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * Author: Rajucm <rajkumar.m@samsung.com>
 *	   kmini.park <kmini.park@samsung.com>
 *	   Daniel(Philju) Lee <daniel.lee@siliconimage.com>
 *
 * Date: 00:00 AM, 6th September, 2013
 *
 * Based on  Silicon Image MHL SII8246 Transmitter Driver
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <asm/irq.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/battery/sec_charger.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <linux/timer.h>
#include <linux/input.h>
#if defined(CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif
#ifdef CONFIG_MFD_MAX77803
#include <linux/mfd/max77803.h>
#endif
#include "sii8246_drv.h"

static struct mhl_tx *g_mhldev;

#define SFEATURE_INT_HPD_PP_MODE
#undef USE_MHL_WRITE_BURST

static int __devinit sii8246_i2cc8_probe(struct i2c_client *client,
						const struct i2c_device_id *id);
static int __devinit sii8246_i2c72_probe(struct i2c_client *client,
						const struct i2c_device_id *id);
static int __devinit sii8246_i2c7a_probe(struct i2c_client *client,
						const struct i2c_device_id *id);
static int __devinit sii8246_i2c92_probe(struct i2c_client *client,
						const struct i2c_device_id *id);

static int __devexit sii8246_i2cc8_remove(struct i2c_client *client);
static int __devexit sii8246_i2c72_remove(struct i2c_client *client);
static int __devexit sii8246_i2c7a_remove(struct i2c_client *client);
static int __devexit sii8246_tpi_remove(struct i2c_client *client);

#ifdef CONFIG_OF
static int __devinit of_sii8246_probe_dt(struct i2c_client *client,
						const struct i2c_device_id *id);
static struct of_device_id sii8246_dt_ids[] = {
	{ .compatible = "sii8246,tmds",},
	{ .compatible = "sii8246,cbus",},
	{ .compatible = "sii8246,hdmi",},
	{ .compatible = "sii8246,tpi",},
	{}
};
MODULE_DEVICE_TABLE(of, sii8246_dt_ids);

static const struct i2c_device_id sii8246_id[] = {
	{"sii8246_mhlv2", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sii8246_id);

static struct i2c_driver sii8246_i2c_driver = {
	.driver = {
		.owner = THIS_MODULE,
		.name = "sii8246_mhlv2dt",
		.of_match_table = of_match_ptr(sii8246_dt_ids),
	},
	.id_table = sii8246_id,
	.probe = of_sii8246_probe_dt,
};
#endif
void mhl_enable_irq(struct mhl_tx *mhl)
{
	if (mhl->irq_enabled)
		return;

	pr_info("sii8246:Irq Enabled\n");
	enable_irq(mhl->irq);
	mhl->irq_enabled = true;
}

void mhl_disable_irq(struct mhl_tx *mhl)
{
	if (!mhl->irq_enabled)
		return;

	pr_info("sii8246:Irq Disabled\n");
	disable_irq(mhl->irq);
	mhl->irq_enabled = false;
}

void mhl_disable_irq_nosync(struct mhl_tx *mhl)
{
	if (!mhl->irq_enabled)
		return;

	pr_info("sii8246:Irq Disabled\n");
	disable_irq_nosync(mhl->irq);
	mhl->irq_enabled = false;
}

struct i2c_client *get_simgI2C_client(struct mhl_tx *mhl, u8 i2c_id)
{
	struct i2c_client *client_ptr;

	if (i2c_id == 0x72)
		client_ptr = mhl->pdata->simg72_tx_client;
	else if (i2c_id == 0x7A)
		client_ptr = mhl->pdata->simg7A_tx_client;
	else if (i2c_id == 0x92)
		client_ptr = mhl->pdata->simg92_tx_client;
	else if (i2c_id == 0xC8)
		client_ptr = mhl->pdata->simgC8_tx_client;
	else
		client_ptr = NULL;

	return client_ptr;
}


static int mhl_i2c_read_byte(struct mhl_tx *mhl, u8 i2c_id,
							u8 offset, u8 *data)
{
	int ret = 0;
	struct i2c_client *client_ptr = get_simgI2C_client(mhl, i2c_id);

	if (!data) {
		pr_info("sii8246: mhl_i2c_read_byte error1 %x\n", i2c_id);
		return -EINVAL;
	}

	if (!client_ptr) {
		pr_info("sii8246: mhl_i2c_read_byte error2 %x\n", i2c_id);
		return -EINVAL;
	}

	ret = i2c_smbus_read_byte_data(client_ptr, offset);
	if (ret < 0) {
		pr_info("sii8246: mhl_i2c_read_byte error3 %x\n", i2c_id);
		return ret;
	}

	*data = (ret & 0x000000FF);
	return ret;
}

#ifdef USE_MHL_WRITE_BURST
static int mhl_i2c_read_block_reg(struct mhl_tx *mhl, u8 i2c_id,
				unsigned int offset, u8 len, u8 *values)
{
	int ret;
	struct i2c_client *client_ptr = get_simgI2C_client(mhl, i2c_id);

	if (!values)
		return -EINVAL;
	ret = i2c_smbus_read_i2c_block_data(client_ptr, offset, len, values);
	if (ret < 0)
		pr_err("sii8246: %s():%d offset:0x%X ret:%d\n",
				__func__, __LINE__, offset, ret);

	return ret;
}
#endif

static int mhl_i2c_write_byte(struct mhl_tx *mhl, u8 i2c_id,
							u8 offset, u8 value)
{
	int ret;
	struct i2c_client *client_ptr = get_simgI2C_client(mhl, i2c_id);

	if (client_ptr == NULL) {
		pr_err("mhl_i2c_write_byte (client_ptr == NULL)\n");
		return 0;
	}

	ret = i2c_smbus_write_byte_data(client_ptr, offset, value);
	return ret;
}

static int mhl_i2c_modify(struct mhl_tx *mhl, u8 i2c_id,
						u8 Offset, u8 Mask, u8 Data)
{
	u8 rd;
	int ret;
	ret = mhl_i2c_read_byte(mhl, i2c_id, Offset, &rd);

	if (ret < 0) {
		pr_err("sii8246: %s():%d devide ID:%x offset:%x\n",
			__func__, __LINE__, i2c_id, Offset);
		return ret;
	}

	rd &= ~Mask;
	rd |= (Data & Mask);
	ret = mhl_i2c_write_byte(mhl, i2c_id, Offset, rd);

	if (ret < 0) {
		pr_err("sii8246: %s():%d devide ID: %x offset:%x\n",
			__func__, __LINE__, i2c_id, Offset);
		return ret;
	}

	return ret;
}

u8 sii8246_support_packedpixel(void)
{
	 /*sii8246 doesnt support packed pixel*/
	return false;
}

static int hpd_drive_low(struct mhl_tx *mhl)
{
	int ret = 0;

	if (mhl->pdata->hpd_status) {
		pr_info("sii8246: %s", __func__);
#ifdef QC_AP
		mhl_hpd_handler(false);
#endif
		mhl->pdata->hpd_status = false;

		ret = mhl_i2c_modify(mhl, PAGE_0, 0x79, BIT5 | BIT4, BIT4);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n",
				__func__, __LINE__);
			return ret;
		}
	}

	return ret;
}

static int hpd_drive_high(struct mhl_tx *mhl)
{
	int ret = 0;

	if (!mhl->pdata->hpd_status) {
		pr_info("sii8246: %s", __func__);
#ifdef QC_AP
		mhl_hpd_handler(true);
#endif
		mhl->pdata->hpd_status = true;

		ret = mhl_i2c_modify(mhl, PAGE_0, 0x79, BIT5 | BIT4,
								BIT5 | BIT4);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								__LINE__);
			return ret;
		}
	}

	return ret;
}

static int force_usbid_switch_open(struct mhl_tx *mhl)
{
	int ret = 0;

	ret = mhl_i2c_modify(mhl, PAGE_0, 0x90, BIT0, 0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_modify(mhl, PAGE_0, 0x95, BIT6, BIT6);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		return ret;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x92, 0x86);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = hpd_drive_low(mhl);
	if (ret < 0)
		goto exit_func;

exit_func:
	return ret;
}

static int release_usbid_switch_open(struct mhl_tx *mhl)
{
	int ret = 0;

	msleep(50);
	ret = mhl_i2c_modify(mhl, PAGE_0, 0x95, BIT6, 0x00);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_modify(mhl, PAGE_0, 0x90, BIT0, BIT0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

exit_func:
	return ret;
}


static int switch_to_d3_mode(struct mhl_tx *mhl)
{
	int ret = 0;

	ret = force_usbid_switch_open(mhl);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_modify(mhl, PAGE_0, 0x93, BIT7 | BIT6 | BIT5
								| BIT4, 0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_modify(mhl, PAGE_0, 0x94, BIT1 | BIT0, 0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	/* all interrupt status register clearing */
	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR1_STATUS_ADD, 0xFF);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR2_STATUS_ADD, 0xFF);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR3_STATUS_ADD, 0xFF);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR4_STATUS_ADD, 0xFF);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR5_STATUS_ADD, 0xFF);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR7_STATUS_ADD, 0xFF);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR8_STATUS_ADD, 0xFF);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, CBUS_INTR_STATUS_ADD, 0xFF);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, CBUS_INTR0_STATUS_ADD, 0xFF);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, CBUS_INTR1_STATUS_ADD, 0xFF);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, CBUS_INTR2_STATUS_ADD, 0xFF);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	/* all interrupt status register clearing end */
	ret = release_usbid_switch_open(mhl);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	ret = hpd_drive_low(mhl);
	if (ret < 0)
		goto exit_func;

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x01, 0x03);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_modify(mhl, PAGE_1, 0x3D, BIT0, 0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	mhl->status.op_status = MHL_READY_RGND_DETECT;
	pr_info("sii8246: %s() end.. %d  !\n", __func__, __LINE__);

exit_func:
	return ret;
}



static int cbus_reset(struct mhl_tx *mhl)
{
	int ret = 0;
	u8 i = 0;

	pr_debug("debug_message : %s. %d ", __func__, __LINE__);
	ret = mhl_i2c_modify(mhl, PAGE_0, 0x05, BIT3, BIT3);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	usleep_range(2000, 2000); /*equivalant to msleep(2)*/
	ret = mhl_i2c_modify(mhl, PAGE_0, 0x05, BIT3, 0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__,	__LINE__);
		goto exit_func;
	}

	for (i = 0; i < 4; i++) {
		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS,
						(0xE0 + i), 0xFF);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								__LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS,
						(0xF0 + i), 0xFF);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
			goto exit_func;
		}
	}

	pr_debug("debug_message : %s. %d ", __func__, __LINE__);
exit_func:
	return ret;
}

static int init_cbus_regs(struct mhl_tx *mhl)
{
	int ret = 0;
	u8 data = 0;

	pr_debug("sii8246: %s() %d\n", __func__, __LINE__);
	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x07, 0xF2);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x40, 0x03);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x42, 0x06);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x36, 0x0B);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x3D, 0xFD);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x1C, 0x01);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x1D, 0x0F);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	/* set devcap */
	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x80, DEVCAP_VAL_DEV_STATE);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x81, DEVCAP_VAL_MHL_VERSION);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x82, DEVCAP_VAL_DEV_CAT);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x83,
						 DEVCAP_VAL_ADOPTER_ID_H);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x84,
						 DEVCAP_VAL_ADOPTER_ID_L);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x85,
						 DEVCAP_VAL_VID_LINK_MODE);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x86,
						 DEVCAP_VAL_AUD_LINK_MODE);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x87,
						 DEVCAP_VAL_VIDEO_TYPE);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x88,
						 DEVCAP_VAL_LOG_DEV_MAP);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x89, DEVCAP_VAL_BANDWIDTH);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x8A,
						 DEVCAP_VAL_FEATURE_FLAG);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x8B,
						 DEVCAP_VAL_DEVICE_ID_H);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x8C,
						 DEVCAP_VAL_DEVICE_ID_L);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x8D,
						 DEVCAP_VAL_SCRATCHPAD_SIZE);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x8E,
						 DEVCAP_VAL_INT_STAT_SIZE);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x8F, DEVCAP_VAL_RESERVED);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, REG_CBUS_LINK_CONTROL_2,
									 &data);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, REG_CBUS_LINK_CONTROL_2,
							 (data | 0x0C));
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_read_byte(mhl, PAGE_CBUS,
					REG_MSC_TIMEOUT_LIMIT, &data);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	data &= ~MSC_TIMEOUT_LIMIT_MSB_MASK;
	data |= 0x0F;
	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, REG_MSC_TIMEOUT_LIMIT,
					 (data & MSC_TIMEOUT_LIMIT_MSB_MASK));
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, REG_CBUS_LINK_CONTROL_1,
									 0x01);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_modify(mhl, PAGE_CBUS,
					REG_CBUS_LINK_CONTROL_11,
					BIT5 | BIT4 | BIT3, BIT5 | BIT4);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_modify(mhl, PAGE_CBUS, REG_MSC_TIMEOUT_LIMIT,
								 0x0F, 0x0D);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_modify(mhl, PAGE_CBUS, 0x2E, BIT4 | BIT2,
								 BIT4 | BIT2);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

exit_func:
	return ret;
}




static int mhl_tx_interrupt_set(struct mhl_tx *mhl)
{
	int ret;

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR1_ENABLE_ADD,
					  mhl->status.intr1_mask_value);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR2_ENABLE_ADD,
					  mhl->status.intr2_mask_value);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR3_ENABLE_ADD,
					  mhl->status.intr3_mask_value);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR4_ENABLE_ADD,
					 mhl->status.intr4_mask_value);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR5_ENABLE_ADD,
					 mhl->status.intr5_mask_value);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR7_ENABLE_ADD,
					 mhl->status.intr7_mask_value);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR8_ENABLE_ADD,
					 mhl->status.intr8_mask_value);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, CBUS_INTR_ENABLE_ADD,
					mhl->status.intr_cbus_mask_value);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, CBUS_INTR0_ENABLE_ADD,
					mhl->status.intr_cbus0_mask_value);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, CBUS_INTR1_ENABLE_ADD,
					mhl->status.intr_cbus1_mask_value);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, CBUS_INTR2_ENABLE_ADD,
					mhl->status.intr_cbus2_mask_value);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

exit_func:
	return ret;
}



static int mhl_init_func(struct mhl_tx *mhl)
{
	int ret = 0;
	u8 data = 0;

	mhl->status.op_status = NO_MHL_STATUS;
	mhl->status.linkmode = 0x03;
	mhl->pdata->hpd_status = false;
	mhl->status.mhl_rgnd = false;
	mhl->status.cbus_connected = false;
	mhl->status.connected_ready = false;
	mhl->msc_cmd_abort = false;
	mhl->rsen_ready = false;
	memset((void *)&mhl->rx_cap, 0x0, sizeof(mhl->rx_cap));

	ret = mhl_i2c_write_byte(mhl, PAGE_1, 0x3D, 0x35);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x11, 0x01);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x12, 0x15);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x08, 0x01);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = cbus_reset(mhl);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x17, 0x03);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x1A, 0x20);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x22, 0xE0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x23, 0xC0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x24, 0xA0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x25, 0x80);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x26, 0x60);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x27, 0x40);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x28, 0x20);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x29, 0x10);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x4C, 0xD0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x4D, 0x02);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x80, 0x3C);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x85, 0x00);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x13, 0x40);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x45, 0x06);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x4B, 0x06);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x31, 0x0A);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA0, 0xD0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA1, 0xBC);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA3, 0x74);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA6, 0x03);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x2B, 0x01);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_modify(mhl, PAGE_0, 0x90, BIT3 | BIT2, BIT2);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x91, 0xA5);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x94, 0x77);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x31, &data);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x31, (data | 0x0C));
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA5, 0xA0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA7, 0x08);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x95, 0x11);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x97, 0x00);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x92, 0x86);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x93, 0x8C);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

#ifdef SFEATURE_INT_HPD_PP_MODE
	/* push pull mode...*/
	ret = mhl_i2c_modify(mhl, PAGE_0, 0x79, BIT6 | BIT2 | BIT1, 0x00);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		return ret;
	}

#endif/*SFEATURE_INT_HPD_PP_MODE*/
	ret = hpd_drive_low(mhl);
	if (ret < 0)
		goto exit_func;

	msleep(25);
	ret = mhl_i2c_modify(mhl, PAGE_0, 0x95, BIT6, 0x00);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x9A,
				BIT_DC9_CBUS_LOW_TO_DISCONNECT |
				BIT_DC9_WAKE_DRVFLT |
				BIT_DC9_DISC_PULSE_PROCEED);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x90, 0x26);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = init_cbus_regs(mhl);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x05, 0x04);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x0D, 0x1C);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	pr_info("sii8246: %s() done\n", __func__);

exit_func:
	return ret;
}

static inline void sii8246_muic_mhl_cb(struct mhl_tx *mhl, bool otg, int type)
{
	if (mhl->pdata->muic_mhl_cb)
		mhl->pdata->muic_mhl_cb(mhl->pdata, otg, type);
}

static inline void sii8246_otg_disable(struct mhl_tx *mhl)
{
	if (mhl->pdata->charging_type == POWER_SUPPLY_TYPE_BATTERY)
		sii8246_muic_mhl_cb(mhl, false, -1);
}

static int sii8246_detection_callback(struct notifier_block *this,
						unsigned long event, void *ptr)
{
	struct mhl_tx *mhl = container_of(this, struct mhl_tx, mhl_nb);
	int ret;

	if (!mhl || !mhl->pdata) {
		pr_info("sii8246:  getting resource is failed\n");
		return 0;
	}

	if (mhl->power_state == event) {
		pr_info("sii8246: mhl already %s\n", event ? "on" : "off");
		return 0;
	}

	mutex_lock(&mhl->mhl_lock);

	mhl->power_state = event;
	pr_info("sii8246: mhl %s\n", mhl->power_state ? "on" : "off");
	if (mhl->power_state) {
		wake_lock(&mhl->mhl_wake_lock);
		mhl->pdata->power(mhl->power_state);
		mhl->pdata->hw_reset();
	} else {
		mhl_disable_irq(mhl);
		wake_unlock(&mhl->mhl_wake_lock);
		sii8246_muic_mhl_cb(mhl, false, -1);

		if (mhl->pdata->hpd_status) {
#ifdef QC_AP
			mhl_hpd_handler(false);
#endif
			hpd_drive_low(mhl);
		}

		if (waitqueue_active(&mhl->cbus_cmd_wq))
			wake_up(&mhl->cbus_cmd_wq);
		/* cbus_cmd_work sync: cant block the current thread since
		 * wake_up is asserted*/
		cancel_work_sync(&mhl->cbus_cmd_work);
		mhl->pdata->power(mhl->power_state);
		pr_debug("sii8246: sii8246_detection_callback: End\n");
		goto exit;
	}

	ret = mhl_init_func(mhl);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit;
	}

	mhl->status.intr1_mask_value = 0;
	mhl->status.intr2_mask_value = 0;
	mhl->status.intr3_mask_value = 0;
	mhl->status.intr4_mask_value = BIT_INTR4_RGND;
	mhl->status.intr5_mask_value = 0;
	mhl->status.intr7_mask_value = 0;
	mhl->status.intr8_mask_value = 0;
	mhl->status.intr_cbus_mask_value = 0;
	mhl->status.intr_cbus0_mask_value = 0;
	mhl->status.intr_cbus1_mask_value = 0;
	mhl->status.intr_cbus2_mask_value = 0;

	ret = mhl_tx_interrupt_set(mhl);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit;
	}

	ret = switch_to_d3_mode(mhl);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit;
	}

	if (mhl->power_state)
		mhl_enable_irq(mhl);

exit:
	mutex_unlock(&mhl->mhl_lock);

	return 0;
}

int process_msc_commands(struct mhl_tx *mhl, struct cbus_msg *cbus_packet)
{
	int ret = 0;
	struct cbus_msg *msc_msg = NULL;

	msc_msg = kzalloc(sizeof(struct cbus_msg), GFP_KERNEL);
	if (!msc_msg) {
		pr_err("sii8246: failed to allocate cbus_msg\n");
		ret = -ENOMEM;
		goto exit_func;
	}
	msc_msg->command = cbus_packet->command;
	msc_msg->offset = cbus_packet->offset;

	msc_msg->buff[0] = cbus_packet->buff[0];
	msc_msg->buff[1] = cbus_packet->buff[1];
	msc_msg->buff[2] = cbus_packet->buff[2];

	pr_debug("sii8246 cbus: (%d) CBUS MSG ADDED to LIST\n", __LINE__);
	list_add_tail(&msc_msg->list, &mhl->cbus_msg_list);
	if (!work_pending(&mhl->cbus_cmd_work)) {
		pr_debug("sii8246 cbus: CBUS Work scheduled <======\n");
		queue_work(mhl->cbus_cmd_wqs, &mhl->cbus_cmd_work);
	}

exit_func:
	return 0;
}

static int mhl_rx_connected_func(struct mhl_tx *mhl)
{
	int ret;
	u8 int4status = 0;
	u8 cbus_status;
	struct cbus_msg cbus_packet;
	ret = mhl_i2c_read_byte(mhl, PAGE_0, INTR4_STATUS_ADD, &int4status);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (int4status == 0x00) {
		pr_info("sii8246: int4status == 0x0\n");
		goto exit_func;
	}

	/* clear interrupt status */
	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR4_STATUS_ADD, int4status);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x0A, &cbus_status);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	mhl->status.cbus_connected = (cbus_status & 0x01);
	if ((int4status & BIT_INTR4_MHL_EST) && mhl->status.cbus_connected) {
		mhl->status.op_status = MHL_DISCOVERY_SUCCESS;
		pr_info("sii8246: MHL_DISCOVERY_SUCCESS\n");

		ret = hpd_drive_low(mhl);
		if (ret < 0)
			return ret;

		ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA0, 0x10);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x07, 0xF2);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x66, 0x00);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_modify(mhl, PAGE_0, 0x90, BIT0, BIT0);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		mod_timer(&mhl->rsen_timer, jiffies + msecs_to_jiffies(300));
		/* (BIT_INTR1_HPD_CHG | BIT_INTR1_RSEN_CHG) */
		mhl->status.intr1_mask_value = 0;
		mhl->status.intr2_mask_value = 0;
		mhl->status.intr3_mask_value = 0;
		mhl->status.intr4_mask_value = (BIT_INTR4_CBUS_LKOUT |
						BIT_INTR4_CBUS_DISCONNECT);
		mhl->status.intr5_mask_value = 0; /*BIT_INTR5_CKDT_CHANGE*/
		mhl->status.intr7_mask_value = 0;
		mhl->status.intr8_mask_value = 0;
		mhl->status.intr_cbus_mask_value =
					BIT_CBUS_INTR_CONNECTION_CHG |
					BIT_CBUS_INTR_CEC_ABORT |
					BIT_CBUS_INTR_DDC_ABORT |
					BIT_CBUS_INTR_MSC_MSG_CMD_RCV |
					BIT_CBUS_INTR_MSC_CMD_DONE |
					BIT_CBUS_INTR_MSC_MT_ABORT |
					BIT_CBUS_INTR_MSC_MR_ABORT;
		mhl->status.intr_cbus0_mask_value =
					BIT_CBUS_INTR0_HPD_RCV
					 | BIT_CBUS_INTR0_MSC_DONE_NACK;
		mhl->status.intr_cbus1_mask_value =
					 BIT_CBUS_INTR1_MSC_ABORT
					 | BIT_CBUS_INTR1_MSC_CMD_ABORT;
		mhl->status.intr_cbus2_mask_value =
					BIT_CBUS_INTR2_MSC_WRITE_BURST_RCV |
					BIT_CBUS_INTR2_MSC_HEARTBEAT_MAX_FAIL |
					BIT_CBUS_INTR2_MSC_SET_INT |
					BIT_CBUS_INTR2_MSC_WRITE_STAT_RCV;
		ret = mhl_tx_interrupt_set(mhl);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		mhl->status.connected_ready |= MHL_STATUS_DCAP_RDY;
		cbus_packet.command = MHL_WRITE_STAT;
		cbus_packet.offset = MHL_STATUS_REG_CONNECTED_RDY;

		cbus_packet.buff[0] = mhl->status.connected_ready;
		process_msc_commands(mhl, &cbus_packet);

	} else if (int4status & BIT_INTR4_NON_MHL_EST) {
		mhl->status.op_status = MHL_DISCOVERY_FAIL;
		pr_info("sii8246: MHL_DISCOVERY_FAIL\n");
	} else if (mhl->status.cbus_connected) {
		pr_info("sii8246: MHL_DISCOVERY_ON\n");
		mhl->status.op_status = MHL_DISCOVERY_ON;
		mhl->status.intr1_mask_value = 0;
		mhl->status.intr2_mask_value = 0;
		mhl->status.intr3_mask_value = 0;
		mhl->status.intr4_mask_value = (BIT_INTR4_MHL_EST
						 | BIT_INTR4_NON_MHL_EST);
		mhl->status.intr5_mask_value = 0;
		mhl->status.intr7_mask_value = 0;
		mhl->status.intr8_mask_value = 0;
		mhl->status.intr_cbus_mask_value =
				BIT_CBUS_INTR_CONNECTION_CHG |
				BIT_CBUS_INTR_CEC_ABORT |
				BIT_CBUS_INTR_DDC_ABORT |
				BIT_CBUS_INTR_MSC_MSG_CMD_RCV |
				BIT_CBUS_INTR_MSC_CMD_DONE |
				BIT_CBUS_INTR_MSC_MT_ABORT |
				BIT_CBUS_INTR_MSC_MR_ABORT;

		mhl->status.intr_cbus0_mask_value =
				BIT_CBUS_INTR0_HPD_RCV |
				BIT_CBUS_INTR0_MSC_DONE_NACK;
		mhl->status.intr_cbus1_mask_value =
				BIT_CBUS_INTR1_MSC_ABORT |
				BIT_CBUS_INTR1_MSC_CMD_ABORT;
		mhl->status.intr_cbus2_mask_value =
				BIT_CBUS_INTR2_MSC_WRITE_BURST_RCV |
				BIT_CBUS_INTR2_MSC_HEARTBEAT_MAX_FAIL |
				BIT_CBUS_INTR2_MSC_SET_INT |
				BIT_CBUS_INTR2_MSC_WRITE_STAT_RCV;

		ret = mhl_tx_interrupt_set(mhl);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			return ret;
		}
	}

exit_func:
	return ret;
}

static void sii8246_setup_charging(struct mhl_tx *mhl)
{
	u8 plim, dev_cat, pow;

	dev_cat = mhl->rx_cap.dev_type;
	pow = ((dev_cat >> 4) & 0x1);

	pr_info("sii8246: %s devcat:0x%x, pow:%d\n", __func__, dev_cat, pow);

	if (mhl->rx_cap.mhl_ver >= 0x20) {
		if (pow == 1) {
			plim = ((dev_cat >> 5) & 0x3);
			sii8246_muic_mhl_cb(mhl, false, plim);
		}
	} else {
		pr_info("sii8246: adopter id:%d, reserved:%d\n",
				mhl->rx_cap.adopter_id, mhl->rx_cap.reserved);

		if (mhl->rx_cap.adopter_id == 321 && mhl->rx_cap.reserved == 2)
			sii8246_muic_mhl_cb(mhl, false, 0x01);
		else if (pow == 1)
			sii8246_muic_mhl_cb(mhl, false, 0x03);
	}
}

static int cbus_cmd_send(struct mhl_tx *mhl, struct cbus_msg *msc_msg)
{
	int ret = 0;
	u8 startbit = 0;

	switch (msc_msg->command) {
	case MHL_SET_INT:
		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x13,
							 msc_msg->offset);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x14,
							 msc_msg->buff[0]);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x12,
					  BIT_CBUS_MSC_WRITE_STAT_OR_SET_INT);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		break;

	case MHL_WRITE_STAT:
		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x13,
							 msc_msg->offset);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x14,
							 msc_msg->buff[0]);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x12,
					  BIT_CBUS_MSC_WRITE_STAT_OR_SET_INT);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		break;

	case MHL_READ_DEVCAP:
		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x13,
							 msc_msg->offset);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x14,
							 msc_msg->buff[0]);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x12,
						  BIT_CBUS_MSC_READ_DEVCAP);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		break;

	case MHL_GET_STATE:
	case MHL_GET_VENDOR_ID:
	case MHL_SET_HPD:
	case MHL_CLR_HPD:
	case MHL_GET_SC1_ERRORCODE:
	case MHL_GET_DDC_ERRORCODE:
	case MHL_GET_MSC_ERRORCODE:
	case MHL_GET_SC3_ERRORCODE:
		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x13,
							 msc_msg->command);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x14,
							 msc_msg->buff[0]);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x12,
						  BIT_CBUS_MSC_PEER_CMD);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		break;

	case MHL_MSC_MSG:
		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x13,
							 msc_msg->command);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x14,
							 msc_msg->buff[0]);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0xBB,
							  msc_msg->buff[1]);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x12,
							  BIT_CBUS_MSC_MSG);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		break;

	case MHL_WRITE_BURST:
		/* will add code later...*/
		startbit = BIT_CBUS_MSC_WRITE_BURST;
		break;

	default:
		pr_err("sii8246: unknown msc cmd 0x%X\n", msc_msg->command);
		break;
	}

exit_func:

	return ret;
}

static int cbus_cmd_done_reaction(struct mhl_tx *mhl, struct cbus_msg *msc_msg)
{
	int ret = 0;
	u8 cap_data;

	switch (msc_msg->command) {
	case MHL_WRITE_STAT:
		if ((msc_msg->offset == MHL_STATUS_REG_CONNECTED_RDY) &&
				(msc_msg->buff[0] == MHL_STATUS_DCAP_RDY)) {
			msc_msg->command = MHL_SET_INT;
			msc_msg->offset = MHL_RCHANGE_INT;
			msc_msg->buff[0] = MHL_INT_DCAP_CHG;
		}

		break;

	case MHL_READ_DEVCAP:
		switch (msc_msg->offset) {
		case MHL_CAP_MHL_VERSION:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.mhl_ver = cap_data;
			pr_info("sii8246: MHL_VERSION:%x\n", cap_data);
			break;

		case MHL_DEV_CATEGORY_OFFSET:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}
			/*MHL2.0 Spec: PLIM define BIT5 & BIT6 as charger type*/
			mhl->rx_cap.dev_type = cap_data & 0xFF;
			sii8246_setup_charging(mhl);

			cap_data = (0x1F & cap_data);

			switch (cap_data) {
			case MHL_DEV_CAT_SOURCE:
			       mhl->rx_cap.dev_type = MHL_DEV_CAT_SOURCE;
				pr_debug("sii8246: %s():%d MHL_DEV_CAT_SOURCE\n",
							 __func__, __LINE__);
				break;

			case MHL_SINK_W_POW:
				mhl->rx_cap.dev_type = MHL_SINK_W_POW;
				 pr_debug("sii8246: %s():%d MHL_SINK_W_POW !\n",
							 __func__, __LINE__);
				/* mhl tx doesn't need power out*/
				break;

			case MHL_SINK_WO_POW:
				mhl->rx_cap.dev_type = MHL_SINK_WO_POW;
				pr_debug("sii8246: %s():%d MHL_SINK_WO_POW !\n",
							 __func__, __LINE__);
				break;

			case MHL_DONGLE_W_POW:
				mhl->rx_cap.dev_type = MHL_DONGLE_W_POW;
				pr_debug("sii8246: %s():%d  MHL_DONGLE_W_POW!\n"
							, __func__, __LINE__);
				break;

			case MHL_DONGLE_WO_POW:
				mhl->rx_cap.dev_type = MHL_DONGLE_WO_POW;
				pr_debug("sii8246: %s():%d MHL_DONGLE_WO_POW !\n"
							, __func__, __LINE__);
				break;

			default:
				mhl->rx_cap.dev_type = cap_data;
				break;
			}

			break;

		case MHL_CAP_ADOPTER_ID_H:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.adopter_id = cap_data;
			mhl->rx_cap.adopter_id <<= 8;
			break;

		case MHL_CAP_ADOPTER_ID_L:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.adopter_id |= cap_data;
			break;

		case MHL_CAP_VID_LINK_MODE:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.vid_link_mode = (0x3F & cap_data);
			pr_info("sii8246: vid_link_mode = 0x%X\n",
						mhl->rx_cap.vid_link_mode);

			if (mhl->rx_cap.vid_link_mode &
					 MHL_DEV_VID_LINK_SUPPRGB444) {
				pr_debug("sii8246: VID_LINK_SUPPRGB444\n");
			}

			if (mhl->rx_cap.vid_link_mode &
					 MHL_DEV_VID_LINK_SUPPYCBCR444) {
				pr_debug("sii8246: VID_LINK_SUPPYCBCR444\n");
			}

			if (mhl->rx_cap.vid_link_mode &
					 MHL_DEV_VID_LINK_SUPPYCBCR422) {
				pr_debug("sii8246: VID_LINK_SUPPYCBCR422\n");
			}

			if (mhl->rx_cap.vid_link_mode &
					 MHL_DEV_VID_LINK_SUPP_PPIXEL) {
				pr_debug("sii8246: VID_LINK_SUPP_PPIXEL\n");
			}

			break;

		case MHL_CAP_AUD_LINK_MODE:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.aud_link_mode = (0x03 & cap_data);
			break;

		case MHL_CAP_VIDEO_TYPE:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.video_type = (0x8F & cap_data);
			break;

		case MHL_CAP_LOG_DEV_MAP:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.log_dev_map = cap_data;
			break;

		case MHL_CAP_BANDWIDTH:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.bandwidth = cap_data;
			break;

		case MHL_CAP_FEATURE_FLAG:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.feature_flag = (0x07 & cap_data);
			mhl->rx_cap.rcp_support = (mhl->rx_cap.feature_flag &
						MHL_FEATURE_RCP_SUPPORT);
			mhl->rx_cap.rap_support = (mhl->rx_cap.feature_flag &
						MHL_FEATURE_RAP_SUPPORT);
			mhl->rx_cap.sp_support = (mhl->rx_cap.feature_flag &
						MHL_FEATURE_SP_SUPPORT);
			break;

		case MHL_CAP_DEVICE_ID_H:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.device_id = cap_data;
			mhl->rx_cap.device_id <<= 8;
			break;

		case MHL_CAP_DEVICE_ID_L:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.device_id |= cap_data;
			break;

		case MHL_CAP_SCRATCHPAD_SIZE:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.scratchpad_size = cap_data;
			break;

		case MHL_CAP_INT_STAT_SIZE:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.int_stat_size = cap_data;
			break;

		case MHL_CAP_DEV_STATE:
		case MHL_CAP_RESERVED:
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x16,
								 &cap_data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->rx_cap.reserved = cap_data;

		default:
			break;
		}

		break;

	case MHL_MSC_MSG:
		if (msc_msg->buff[0] == MHL_MSC_MSG_RCPE) {
			msc_msg->command = MHL_MSC_MSG;
			msc_msg->offset = 0x00;
			msc_msg->buff[0] = MHL_MSC_MSG_RCPK;
			msc_msg->buff[1] = msc_msg->buff[2];
		}

		break;

	case MHL_WRITE_BURST:
	case MHL_SET_INT:
	default:
		break;
	}

exit_func:

	return ret;
}

static void cbus_cmd_thread(struct work_struct *work)
{
	struct mhl_tx *mhl =
		container_of(work, struct mhl_tx, cbus_cmd_work);
	int ret = 0;
	struct cbus_msg *msc_msg, *next_msg;

	pr_debug("sii8246 cbus: ### CBUS_CMD THREAD START  <=====\n");

	list_for_each_entry_safe(msc_msg, next_msg, &mhl->cbus_msg_list, list) {
		mutex_lock(&mhl->cbus_cmd_lock);
		if (mhl->msc_cmd_abort == true) {
			mhl->msc_cmd_abort = false;
			pr_info("sii8246: CMD ABOART WAIT over 2s !\n");
			msleep(2010);
		}

		if (mhl->power_state == false) {
			pr_info("sii8236: %s mhl is disconnected\n", __func__);
			goto free_list;
		}
		if (mhl->status.op_status != MHL_DISCOVERY_SUCCESS)
			goto free_list;

		mhl->msc_cmd_done_intr = MSC_SEND;
		ret = cbus_cmd_send(mhl, msc_msg);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n",
							__func__, __LINE__);
			goto free_list;
		}

		pr_debug("sii8246 cbus: %s: %d WAITING for CMD complition ===>\n",
				__func__, __LINE__);
		/* wait_event_func */
		wait_event_timeout(mhl->cbus_cmd_wq,
					 ((mhl->msc_cmd_done_intr != MSC_SEND)
					 || (mhl->msc_cmd_abort != false)
					 || (mhl->status.op_status
					 != MHL_DISCOVERY_SUCCESS)),
					msecs_to_jiffies(2000));

		if (mhl->status.op_status != MHL_DISCOVERY_SUCCESS)
			goto free_list;

		if (mhl->msc_cmd_done_intr == MSC_DONE_ACK) {
			ret = cbus_cmd_done_reaction(mhl, msc_msg);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto free_list;
			}
		} else if (mhl->msc_cmd_done_intr == MSC_DONE_NACK) {
			pr_info("sii8246: MSC_DONE_NACK received!\n");
		}

free_list:
		pr_debug("sii8246 cbus: CBUS_CMD REMOVED from LIST  <=====\n");
		list_del(&msc_msg->list);
		kfree(msc_msg);
		mutex_unlock(&mhl->cbus_cmd_lock);
	}

	pr_debug("sii8246 cbus: ### CBUS_CMD THREAD END HERE  <=====\n");
	return ;
}

static void sii8246_rsen_work(struct work_struct *work)
{
	struct mhl_tx *mhl =
		container_of(work, struct mhl_tx, mhl_rsen_work);

	mutex_lock(&mhl->mhl_lock);
	mhl->status.intr1_mask_value = BIT_INTR1_RSEN_CHG;
	if (mhl_tx_interrupt_set(mhl) < 0)
		pr_info("sii8246: %s() intr set fail\n", __func__);
	mhl->rsen_ready = true;
	pr_info("sii8246: %s() intrrupt set\n", __func__);
	mutex_unlock(&mhl->mhl_lock);
}

static void sii8246_rsen_timer(unsigned long data)
{
	struct mhl_tx *mhl = (struct mhl_tx *) data;

	schedule_work(&mhl->mhl_rsen_work);
}

static int mhl_rgnd_check_func(struct mhl_tx *mhl)
{
	u8 int4status = 0, rgnd_value = 0, data = 0;
	int ret = 0;

	ret = mhl_i2c_write_byte(mhl, PAGE_1, 0x3D, 0x35);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x11, 0x01);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x12, 0x15);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x08, 0x01);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = cbus_reset(mhl);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x10, 0xC1);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x19, 0x07);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x17, 0x03);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x1A, 0x20);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x22, 0xE0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x23, 0xC0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x24, 0xA0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x25, 0x80);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x26, 0x60);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x27, 0x40);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x28, 0x20);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x29, 0x10);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x4C, 0xD0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x4D, 0x02);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x80, 0x3C);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x85, 0x00);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x13, 0x40);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x45, 0x06);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x4B, 0x06);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_2, 0x31, 0x0A);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA0, 0xD0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA1, 0xBC);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (mhl->pdata->swing_level)
		ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA3,
				mhl->pdata->swing_level);
	else
		ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA3, 0x74);

	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA6, 0x03);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x2B, 0x01);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_modify(mhl, PAGE_0, 0x90, BIT3 | BIT2, BIT2);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x91, 0xA5);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x94, 0x77);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x31, &data);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x31, (data | 0x0C));
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA5, 0xBC);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA7, 0x08);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x95, 0x71);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x97, 0x00);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x92, 0x86);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x93, 0x8C);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

#ifdef SFEATURE_INT_HPD_PP_MODE
	/* push pull mode...*/
	ret = mhl_i2c_modify(mhl, PAGE_0, 0x79,	BIT6 | BIT2 | BIT1, 0x00);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		return ret;
	}

#endif/*SFEATURE_INT_HPD_PP_MODE*/
	ret = hpd_drive_low(mhl);
	if (ret < 0)
		goto exit_func;

	msleep(25);
	ret = mhl_i2c_modify(mhl, PAGE_0, 0x95, BIT6, 0x00);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x9A,
				BIT_DC9_CBUS_LOW_TO_DISCONNECT |
				BIT_DC9_WAKE_DRVFLT |
				BIT_DC9_DISC_PULSE_PROCEED);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x90, 0x27);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = init_cbus_regs(mhl);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x05, 0x04);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_write_byte(mhl, PAGE_0, 0x0D, 0x1C);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_modify(mhl, PAGE_0, 0x90, BIT1, 0);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_read_byte(mhl, PAGE_0, INTR4_STATUS_ADD, &int4status);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		usleep_range(2000, 2000); /*equivalant to msleep(2)*/
		ret = mhl_i2c_read_byte(mhl, PAGE_0, INTR4_STATUS_ADD,
								 &int4status);

		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}
	}

	if (int4status == 0x00) {
		pr_err("sii8246: %s() int4status = 0x0\n", __func__);
		goto exit_func;
	}

	/* clear interrupt status...*/
	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR4_STATUS_ADD,
					int4status);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (int4status & BIT_INTR4_RGND) {
		/* read rgnd value...*/
		ret = mhl_i2c_read_byte(mhl, PAGE_0, 0x99, &rgnd_value);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		rgnd_value = (rgnd_value & 0x03);
		if (rgnd_value == 0x02) {
			mhl->status.mhl_rgnd = true;
			mhl->status.op_status = MHL_RX_CONNECTED;
			pr_info("sii8246: RGND 1K detected !\n");

			sii8246_muic_mhl_cb(mhl, true, 0x03);

			mhl->status.intr1_mask_value = 0;
			mhl->status.intr4_mask_value =
				 (BIT_INTR4_MHL_EST | BIT_INTR4_NON_MHL_EST);
			ret = mhl_tx_interrupt_set(mhl);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				return ret;
			}
		} else {
			mhl->status.mhl_rgnd = false;
			mhl->status.op_status = MHL_USB_CONNECTED;
			pr_info("sii8246: RGND not 1K(0x%X)\n", rgnd_value);

			/* for TV off charging */
			if (mhl->pdata->vbus_present())
				sii8246_muic_mhl_cb(mhl, false, 0x03);
		}
	}

exit_func:
	return ret;
}

static int get_write_burst_data(struct mhl_tx *mhl)
{
	int ret = 0;
#ifdef USE_MHL_WRITE_BURST
	u8 tmp[16];
	memset(tmp, 0x00, 16);

	ret = mhl_i2c_read_block_reg(mhl, PAGE_CBUS, 0xC0, 16, tmp);
	if (ret < 0)
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
#endif
	return ret;
}

static int tmds_control(struct mhl_tx *mhl, bool tmds_on)
{
	int ret = 0;

	if (tmds_on) {
		pr_info("sii8246: TMDS ON\n");
		ret = mhl_i2c_modify(mhl, PAGE_0, 0x80, BIT4, BIT4);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA1, 0xBC);

		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}
	} else {
		pr_info("sii8246: TMDS OFF\n");
		ret = mhl_i2c_modify(mhl, PAGE_0, 0x80, BIT4, 0);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA1, 0xC0);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}
	}

exit_func:
	return ret;
}

static int cbusprocsess_error(struct mhl_tx *mhl, u8 status, bool *abort)
{
	int ret = 0;
	u8 ddcabortreason = 0;
	u8 msc_abort_intstatus = 0;

	status &= (BIT_MSC_REQ_ABORT | BIT_CBUS_DDC_ABORT | BIT_MSC_RESP_ABORT);
	*abort = false;

	if (status) {
		if (status & BIT_CBUS_DDC_ABORT) {
			pr_info("sii8246: BIT_CBUS_DDC_ABORT\n");
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x0C,
							 &ddcabortreason);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x57,
							 ddcabortreason);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			*abort = true;
		}

		if (status & BIT_MSC_REQ_ABORT) {
			pr_info("sii8246: requester BIT_MSC_REQ_ABORT\n");
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x0D,
							 &msc_abort_intstatus);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x0D, 0xFF);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}
			*abort = true;
			if (msc_abort_intstatus) {
				*abort = true;
				pr_info("sii8246: MSC Transfer ABORTED\n");

				if (BIT_CBUS_MSC_MT_ABORT_INT_MAX_FAIL &
							 msc_abort_intstatus) {
					pr_cont(": INT_MAX_FAIL\n");
				}

				if (BIT_CBUS_MSC_MT_ABORT_INT_PROTO_ERR &
							 msc_abort_intstatus) {
					pr_info(": INT_PROTO_ERR\n");
				}

				if (BIT_CBUS_MSC_MT_ABORT_INT_TIMEOUT &
							 msc_abort_intstatus) {
					pr_info(": INT_TIMEOUT\n");
				}

				if (BIT_CBUS_MSC_MT_ABORT_INT_UNDEF_CMD &
							 msc_abort_intstatus) {
					pr_info(": INT_UNDEF_CMD\n");
				}

				if (BIT_CBUS_MSC_MT_ABORT_INT_MSC_MT_PEER_ABORT
							& msc_abort_intstatus) {
					pr_info(": INT_MSC_MT_PEER_ABORT\n");
				}
				pr_info("\n");
			}
		}

		if (status & BIT_MSC_RESP_ABORT) {
			pr_info("sii8246: responder BIT_MSC_RESP_ABORT\n");
			ret = mhl_i2c_write_byte(mhl, PAGE_CBUS, 0x0E, 0xFF);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							__func__, __LINE__);
				goto exit_func;
			}
		}
	}

exit_func:
	return ret;
}


static int cbus_set_int_check(struct mhl_tx *mhl, u8 intr_0, u8 intr_1)
{
	int ret = 0;
	struct cbus_msg cbus_packet;
	if (MHL_INT_DCAP_CHG & intr_0) {
		pr_info("sii8246: DCAP_CHG received\n");
		cbus_packet.buff[0] = 0;
		cbus_packet.command = MHL_READ_DEVCAP;
		cbus_packet.offset = MHL_CAP_FEATURE_FLAG;
		process_msc_commands(mhl, &cbus_packet);

		cbus_packet.command = MHL_READ_DEVCAP;
		cbus_packet.offset = MHL_CAP_VID_LINK_MODE;
		process_msc_commands(mhl, &cbus_packet);

		cbus_packet.command = MHL_READ_DEVCAP;
		cbus_packet.offset = MHL_CAP_MHL_VERSION;
		process_msc_commands(mhl, &cbus_packet);

		cbus_packet.command = MHL_READ_DEVCAP;
		cbus_packet.offset = MHL_CAP_ADOPTER_ID_H;
		process_msc_commands(mhl, &cbus_packet);

		cbus_packet.command = MHL_READ_DEVCAP;
		cbus_packet.offset = MHL_CAP_ADOPTER_ID_L;
		process_msc_commands(mhl, &cbus_packet);

		cbus_packet.command = MHL_READ_DEVCAP;
		cbus_packet.offset = MHL_CAP_RESERVED;
		process_msc_commands(mhl, &cbus_packet);

		cbus_packet.command = MHL_READ_DEVCAP;
		cbus_packet.offset = MHL_DEV_CATEGORY_OFFSET;
		process_msc_commands(mhl, &cbus_packet);
	}

	if (MHL_INT_DSCR_CHG & intr_0) {
		pr_info("sii8246: MHL_INT_DSCR_CHG !\n");
		ret = get_write_burst_data(mhl);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
			 __LINE__);
			return ret;
		}
	}

	if (MHL_INT_REQ_WRT  & intr_0) {
		pr_info("sii8246: MHL_INT_REQ_WRT\n");
		cbus_packet.command = MHL_SET_INT;
		cbus_packet.offset = MHL_RCHANGE_INT;
		cbus_packet.buff[0] = MHL_INT_GRT_WRT;
		process_msc_commands(mhl, &cbus_packet);
	}

	if (MHL_INT_GRT_WRT & intr_0)
		pr_info("sii8246: MHL_INT_GRT_WRT\n");

	/* removed "else", since interrupts are not mutually exclusive of
	* each other. */
	if (MHL_INT_EDID_CHG & intr_1) {
		pr_info("sii8246: MHL_INT_EDID_CHG\n");
		ret = tmds_control(mhl, false);
		if (ret < 0)
			return ret;

		ret = hpd_drive_low(mhl);
		if (ret < 0)
			goto exit_func;

		msleep(110);
		ret = hpd_drive_high(mhl);
		if (ret < 0)
			goto exit_func;
	}

exit_func:
	return ret;
}

static int cbus_status_cmd_check(struct mhl_tx *mhl, u8 status0, u8 status1)
{
	int ret = 0;
	struct cbus_msg cbus_packet;

	if ((MHL_STATUS_PATH_ENABLED & status1) &&
		!(mhl->status.linkmode & MHL_STATUS_PATH_ENABLED)) {
		pr_info("sii8246: MHL_STATUS_PATH_ENABLED\n");

		mhl->status.linkmode |= MHL_STATUS_PATH_ENABLED;
		cbus_packet.command = MHL_WRITE_STAT;
		cbus_packet.offset = MHL_STATUS_REG_LINK_MODE;
		cbus_packet.buff[0] = mhl->status.linkmode;
		process_msc_commands(mhl, &cbus_packet);
		ret = tmds_control(mhl, true);
		if (ret < 0)
			return ret;

	} else if (!(MHL_STATUS_PATH_ENABLED & status1) &&
			   (mhl->status.linkmode &
					 MHL_STATUS_PATH_ENABLED)) {
		pr_info("sii8246: MHL_STATUS_PATH_DISABLED !\n");

		mhl->status.linkmode &= ~MHL_STATUS_PATH_ENABLED;
		cbus_packet.command = MHL_WRITE_STAT;
		cbus_packet.offset = MHL_STATUS_REG_LINK_MODE;
		cbus_packet.buff[0] = mhl->status.linkmode;
		process_msc_commands(mhl, &cbus_packet);

		ret = tmds_control(mhl, false);
		if (ret < 0)
			return ret;
	}

	if (MHL_STATUS_DCAP_RDY & status0) {
		pr_info("sii8246: DCAP_RDY received\n");
		cbus_packet.buff[0] = 0;
		cbus_packet.command = MHL_READ_DEVCAP;
		cbus_packet.offset = MHL_CAP_FEATURE_FLAG;
		process_msc_commands(mhl, &cbus_packet);

		cbus_packet.command = MHL_READ_DEVCAP;
		cbus_packet.offset = MHL_CAP_VID_LINK_MODE;
		process_msc_commands(mhl, &cbus_packet);

		cbus_packet.command = MHL_READ_DEVCAP;
		cbus_packet.offset = MHL_CAP_MHL_VERSION;
		process_msc_commands(mhl, &cbus_packet);

		cbus_packet.command = MHL_READ_DEVCAP;
		cbus_packet.offset = MHL_CAP_RESERVED;
		process_msc_commands(mhl, &cbus_packet);

		cbus_packet.command = MHL_READ_DEVCAP;
		cbus_packet.offset = MHL_DEV_CATEGORY_OFFSET;
		process_msc_commands(mhl, &cbus_packet);
	}
	return 0;
}

#ifdef SFEATURE_UCP_FEATURE
static void sii8246_i2cc8_ucpk_send(struct mhl_tx *mhl, u8 keycode)
{
	struct cbus_msg cbus_packet;

	cbus_packet.command = MHL_MSC_MSG;
	cbus_packet.offset = 0x00;
	cbus_packet.buff[0] = MHL_MSC_MSG_UCPK;
	cbus_packet.buff[1] = keycode;
	process_msc_commands(mhl, &cbus_packet);
}

static void sii8246_i2cc8_ucpe_send(struct mhl_tx *mhl, u8 key_code)
{
	struct cbus_msg cbus_packet;

	cbus_packet.command = MHL_MSC_MSG;
	cbus_packet.offset = 0x00;
	cbus_packet.buff[0] = MHL_MSC_MSG_UCPE;
	cbus_packet.buff[1] = key_code;
	process_msc_commands(mhl, &cbus_packet);
}
#endif/*SFEATURE_UCP_FEATURE*/

static void sii8246_i2cc8_rapk_send(struct mhl_tx *mhl)
{
	struct cbus_msg cbus_packet;

	cbus_packet.command = MHL_MSC_MSG;
	cbus_packet.offset = 0x00;
	cbus_packet.buff[0] = MHL_MSC_MSG_RAPK;
	cbus_packet.buff[1] = 0x00;
	process_msc_commands(mhl, &cbus_packet);
}

static void sii8246_i2cc8_rcpk_send(struct mhl_tx *mhl, u8 keycode)
{
	struct cbus_msg cbus_packet;

	cbus_packet.command = MHL_MSC_MSG;
	cbus_packet.offset = 0x00;
	cbus_packet.buff[0] = MHL_MSC_MSG_RCPK;
	cbus_packet.buff[1] = keycode;
	process_msc_commands(mhl, &cbus_packet);

	pr_info("Sii8246: %s(): Data[0x%02x]\n", __func__, keycode);
}

static void sii_mhl_tx_rcpe_send(struct mhl_tx *mhl, u8 erro_code, u8 key_code)
{
	struct cbus_msg cbus_packet;

	cbus_packet.command = MHL_MSC_MSG;
	cbus_packet.offset = 0x00;
	cbus_packet.buff[0] = MHL_MSC_MSG_RCPE;
	cbus_packet.buff[1] = erro_code;
	cbus_packet.buff[2] = key_code;
	process_msc_commands(mhl, &cbus_packet);
	pr_info("Sii8246: %s(): Data[0x%02x]\n", __func__, key_code);
}

static bool is_valid_key(int data)
{
#if defined(FEATURE_SS_MHL_RCP_SUPPORT)
	return is_key_supported(data);
#else
	return false;
#endif
}
static int cbus_msc_cmd_check(struct mhl_tx *mhl, u8 msc_cmd, u8 data)
{
	int ret = 0;

	switch (msc_cmd) {
	case	MHL_MSC_MSG_RAP:
		if (MHL_RAP_CONTENT_ON == data) {
			pr_info("sii8246: %s():%d  MHL_RAP_CONTENT_ON\n",
							 __func__, __LINE__);
			ret = tmds_control(mhl, true);
			if (ret < 0)
				return ret;

		} else if (MHL_RAP_CONTENT_OFF == data) {
			pr_info("sii8246: %s():%d  MHL_RAP_CONTENT_OFF\n",
							__func__, __LINE__);
			ret = tmds_control(mhl, false);
			if (ret < 0)
				return ret;
		}
		sii8246_i2cc8_rapk_send(mhl);
		break;

	case	MHL_MSC_MSG_RCP:
		pr_info("Sii8246: MHL_MSC_MSG_RCP Data Received %d\n", data);

		if (is_valid_key(data & 0x7F)) {
#if defined(FEATURE_SS_MHL_RCP_SUPPORT)
			rcp_key_report(mhl->input_dev, data);
#endif
			sii8246_i2cc8_rcpk_send(mhl, data);
		} else
			sii_mhl_tx_rcpe_send(mhl,
					RCPE_INEEFECTIVE_KEY_CODE, data);
		break;

	case	MHL_MSC_MSG_RCPK:
		break;

	case	MHL_MSC_MSG_RCPE:
		break;

	case	MHL_MSC_MSG_RAPK:
		break;
#ifdef SFEATURE_UCP_FEATURE
	case	MHL_MSC_MSG_UCP:
		/* Customer needs to check UCP code range...*/
#ifdef SFEATURE_SUPPORT_ASCII
		if ((data & 0x7F) <= 0x7F) {
			/* need to send ascii data to framework.*/
			sii8246_i2cc8_ucpk_send(mhl, data);
		} else {
			sii8246_i2cc8_ucpe_send(mhl, data);
		}
#else
		sii8246_i2cc8_ucpe_send(mhl, data);
#endif
		break;

	case	MHL_MSC_MSG_UCPK:
		break;

	case	MHL_MSC_MSG_UCPE:
		break;
#endif/*SFEATURE_UCP_FEATURE*/

	default:
		/* Any freak value here would continue with no event to app */
		break;
	}

	return ret;
}

static int cbus_link_check(struct mhl_tx *mhl)
{
	int ret = 0, i = 0;
	u8 cbus_intr, data;
	u8 status[4] = {0, };

	ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, CBUS_INTR_STATUS_ADD,
								 &cbus_intr);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (cbus_intr == 0xFF) {
		pr_warn("sii8246: cbus status is inoperational\n");
		goto exit_func;
	}

	if (cbus_intr) {
		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS,
					 CBUS_INTR_STATUS_ADD, cbus_intr);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		if (cbus_intr & (BIT_CBUS_INTR_MSC_MT_ABORT |
		    BIT_CBUS_INTR_MSC_MR_ABORT | BIT_CBUS_INTR_DDC_ABORT)) {
			ret = cbusprocsess_error(mhl, cbus_intr,
							 &mhl->msc_cmd_abort);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			if (mhl->msc_cmd_abort == true) {
				pr_info("sii8246: %s():%d msc_cmd_abort\n",
							__func__, __LINE__);

				if (waitqueue_active(&mhl->cbus_cmd_wq)) {
					wake_up(&mhl->cbus_cmd_wq);
					pr_debug("sii8246 cbus: WakeUP CBUS CMD\n");
				}
			}
		}

		if (cbus_intr & BIT_CBUS_INTR_MSC_CMD_DONE) {
			pr_debug("sii8246: BIT_CBUS_INTR_MSC_CMD_DONE!\n");
			mhl->msc_cmd_done_intr = MSC_DONE_ACK;

			if (waitqueue_active(&mhl->cbus_cmd_wq)) {
				wake_up(&mhl->cbus_cmd_wq);
				pr_debug("sii8246 cbus: (%d)WakeUp CBUS CMD\n",
								__LINE__);
			}
		}

		if (cbus_intr & BIT_CBUS_INTR_CONNECTION_CHG)
			pr_info("sii8246: BIT_CBUS_INTR_CONNECTION_CHG!\n");

		if (cbus_intr & BIT_CBUS_INTR_CEC_ABORT)
			pr_info("sii8246: BIT_CBUS_INTR_CEC_ABORT !\n");

		if (cbus_intr & BIT_CBUS_INTR_MSC_MSG_CMD_RCV) {
			pr_info("sii8246: BIT_CBUS_INTR_MSC_MSG_CMD_RCV !\n");
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x18,
								 &status[0]);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, 0x19,
								 &status[1]);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			ret = cbus_msc_cmd_check(mhl, status[0], status[1]);
			if (ret < 0)
				return ret;
		}
	}

	ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, CBUS_INTR0_STATUS_ADD,
								 &cbus_intr);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (cbus_intr) {
		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS,
					 CBUS_INTR0_STATUS_ADD, cbus_intr);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		if (cbus_intr & BIT_CBUS_INTR0_HPD_RCV) {
			pr_info("sii8246: BIT_CBUS_INTR0_HPD_RCV !\n");
			ret = mhl_i2c_read_byte(mhl, PAGE_CBUS,
							0x0D, &data);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			if (data & BIT6) {
				pr_info("sii8246: %s() hpd_status = true\n",
								__func__);
				ret = hpd_drive_high(mhl);
				if (ret < 0)
					goto exit_func;

				ret = tmds_control(mhl, true);
				if (ret < 0)
					return ret;
			} else {
				pr_info("sii8246: %s() hpd_status = false\n",
								__func__);
				ret = tmds_control(mhl, false);
				if (ret < 0)
					return ret;

				ret = hpd_drive_low(mhl);
				if (ret < 0)
					goto exit_func;
			}
		}

		if (cbus_intr & BIT_CBUS_INTR0_MSC_DONE_NACK) {
			pr_info("sii8246: BIT_CBUS_INTR0_MSC_DONE_NACK\n");
			mhl->msc_cmd_done_intr = MSC_DONE_NACK;

			if (waitqueue_active(&mhl->cbus_cmd_wq)) {
				wake_up(&mhl->cbus_cmd_wq);
				pr_debug("sii8246 cbus: (%d)WakeUp  CBUS CMD\n",
								__LINE__);
			}
		}
	}

	ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, CBUS_INTR1_STATUS_ADD,
								 &cbus_intr);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (cbus_intr) {
		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS,
					 CBUS_INTR1_STATUS_ADD, cbus_intr);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		if (cbus_intr & BIT_CBUS_INTR1_MSC_ABORT)
			pr_info("sii8246: BIT_CBUS_INTR1_MSC_ABORT !\n");

		if (cbus_intr & BIT_CBUS_INTR1_MSC_CMD_ABORT)
			pr_info("sii8246: BIT_CBUS_INTR1_MSC_CMD_ABORT !\n");
	}

	ret = mhl_i2c_read_byte(mhl, PAGE_CBUS, CBUS_INTR2_STATUS_ADD,
								&cbus_intr);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (cbus_intr) {
		ret = mhl_i2c_write_byte(mhl, PAGE_CBUS,
					 CBUS_INTR2_STATUS_ADD, cbus_intr);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
			goto exit_func;
		}

		if (cbus_intr & BIT_CBUS_INTR2_MSC_WRITE_BURST_RCV)
			pr_info("sii8246: BIT_CBUS_INTR2_MSC_WRITE_BURST_RCV !\n");

		if (cbus_intr & BIT_CBUS_INTR2_MSC_HEARTBEAT_MAX_FAIL)
			pr_info("sii8246: BIT_CBUS_INTR2_MSC_HEARTBEAT_MAX_FAIL !\n");

		if (cbus_intr & BIT_CBUS_INTR2_MSC_SET_INT) {
			for (i = 0; i < 4; i++) {
				ret = mhl_i2c_read_byte(mhl, PAGE_CBUS,
						 (0xA0 + i), &status[i]);
				if (ret < 0) {
					pr_err("sii8246: %s():%d failed!\n",
						__func__, __LINE__);
					goto exit_func;
				}

				ret = mhl_i2c_write_byte(mhl, PAGE_CBUS,
							 (0xA0 + i), 0xFF);
				if (ret < 0) {
					pr_err("sii8246: %s():%d failed!\n",
						__func__, __LINE__);
					goto exit_func;
				}
			}

			ret = cbus_set_int_check(mhl, status[0], status[1]);
			if (ret < 0)
				goto exit_func;
		}

		memset(status, 0, sizeof(status));
		if (cbus_intr & BIT_CBUS_INTR2_MSC_WRITE_STAT_RCV) {
			for (i = 0; i < 4; i++) {
				ret = mhl_i2c_read_byte(mhl, PAGE_CBUS,
						 (0xB0 + i), &status[i]);
				if (ret < 0) {
					pr_err("sii8246: %s():%d failed!\n",
						__func__, __LINE__);
					goto exit_func;
				}

				ret = mhl_i2c_write_byte(mhl, PAGE_CBUS,
							 (0xB0 + i), 0xFF);
				if (ret < 0) {
					pr_err("sii8246: %s():%d failed!\n",
						__func__, __LINE__);
					goto exit_func;
				}
			}

			pr_info("sii8246: CBUS_INTR2_MSC_WRITE_STAT_RCV\n");
			ret = cbus_status_cmd_check(mhl, status[0], status[1]);
			if (ret < 0)
				goto exit_func;
		}
	}

exit_func:
	return ret;
}


static int mhl_tx_intr_handle(struct mhl_tx *mhl)
{
	int ret = 0;
	u8 data = 0, intr_data = 0;

	ret = mhl_i2c_read_byte(mhl, PAGE_0, INTR1_STATUS_ADD,
					&intr_data);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (intr_data == 0x00)
		goto intr4_check;

	/* clear interrupt status */
	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR1_STATUS_ADD,
				intr_data);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	ret = mhl_i2c_read_byte(mhl, PAGE_0, 0x09, &data);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (intr_data & BIT_INTR1_RSEN_CHG && mhl->rsen_ready == true) {
		if (data & 0x04)
			pr_info("sii8246: RSEN event high\n");

		msleep(100);
		ret = mhl_i2c_read_byte(mhl, PAGE_0, 0x09, &data);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
			goto exit_func;
		}

		if (data & 0x04)
			pr_info("sii8246: %s():%d\n", __func__, __LINE__);
		else {
			ret = mhl_i2c_write_byte(mhl, PAGE_0,
						0xA0, 0xD0);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->status.op_status = MHL_RX_DISCONNECTED;
			pr_info("sii8246: RSEN really low\n");
			sii8246_otg_disable(mhl);

			if (mhl->power_state == false) {
				pr_info("sii8246: %s(): mhl off\n", __func__);
				ret = -ECANCELED;
			}
			goto exit_func;
		}
	}

	if (intr_data & BIT_INTR1_HPD_CHG) {
		if (data & 0x02) {
			pr_info("sii8246: HPD event high\n");
			ret = hpd_drive_high(mhl);
			if (ret < 0)
				goto exit_func;
		} else {
			pr_info("sii8246: HPD event low\n");
			ret = hpd_drive_low(mhl);
			if (ret < 0)
				goto exit_func;
		}
	}

intr4_check:
	ret = mhl_i2c_read_byte(mhl, PAGE_0, INTR4_STATUS_ADD,
					&intr_data);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (intr_data == 0x00)
		goto intr5_check;

	/*clear interrupt status..*/
	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR4_STATUS_ADD,
					intr_data);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (intr_data & BIT_INTR4_CBUS_LKOUT) {
		pr_info("sii8246: %s():%d  BIT_INTR4_CBUS_LKOUT\n",
							 __func__, __LINE__);
		ret = force_usbid_switch_open(mhl);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
			return ret;
		}

		ret = release_usbid_switch_open(mhl);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
			return ret;
		}
	}

	if (intr_data & BIT_INTR4_CBUS_DISCONNECT) {
		pr_info("sii8246: BIT_INTR4_CBUS_DISCONNECT\n");
		ret = mhl_i2c_write_byte(mhl, PAGE_0, 0xA0, 0xD0);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
			goto exit_func;
		}
		sii8246_otg_disable(mhl);
		mhl->status.op_status = MHL_RX_DISCONNECTED;
		goto exit_func;
	}

intr5_check:
	ret = mhl_i2c_read_byte(mhl, PAGE_0, INTR5_STATUS_ADD,
						&intr_data);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (intr_data == 0x00)
		goto exit_func;

	/* clear interrupt status...*/
	ret = mhl_i2c_write_byte(mhl, PAGE_0, INTR5_STATUS_ADD,
						intr_data);
	if (ret < 0) {
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);
		goto exit_func;
	}

	if (intr_data & BIT_INTR5_CKDT_CHANGE)
		pr_info("sii8246: BIT_INTR5_CKDT_CHANGE\n");

exit_func:
	return ret;
}

static irqreturn_t mhl_irq_thread(int irq, void *data)
{
	struct mhl_tx *mhl = data;
	int ret = 0;
	u8 status = 0;

	if (mhl->power_state == false) {
		pr_info("sii8246: %s() mhl is disconnected\n", __func__);
		return IRQ_HANDLED ;
	}

	mutex_lock(&mhl->mhl_lock);
	status = mhl->status.op_status;

	if (status == NO_MHL_STATUS) {
		pr_info("sii8246: %s() NO_MHL_STATUS\n", __func__);
		goto exit_func;
	}

	switch (status) {
	case MHL_READY_RGND_DETECT:
		pr_info("sii8246: MHL_READY_RGND_DETECT\n");
		ret = mhl_rgnd_check_func(mhl);
		if (ret < 0)
			goto exit_func;

		if (mhl->status.op_status == MHL_USB_CONNECTED) {
			if (waitqueue_active(&mhl->cbus_cmd_wq)) {
				wake_up(&mhl->cbus_cmd_wq);
				pr_debug("sii8246 cbus: WakeUp  CBUS CMD\n");
			}

			mhl->pdata->hw_reset();
			ret = mhl_init_func(mhl);
			if (ret < 0) {
				pr_err("sii8246: mhl_init_func failed !\n");
				goto exit_func;
			}

			mhl->status.intr1_mask_value = 0;
			mhl->status.intr2_mask_value = 0;
			mhl->status.intr3_mask_value = 0;
			mhl->status.intr4_mask_value = BIT_INTR4_RGND;
			mhl->status.intr5_mask_value = 0;
			mhl->status.intr7_mask_value = 0;
			mhl->status.intr8_mask_value = 0;
			mhl->status.intr_cbus_mask_value = 0;
			mhl->status.intr_cbus0_mask_value = 0;
			mhl->status.intr_cbus1_mask_value = 0;
			mhl->status.intr_cbus2_mask_value = 0;
			ret = mhl_tx_interrupt_set(mhl);
			if (ret < 0)
				goto exit_func;

			ret = switch_to_d3_mode(mhl);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			goto exit_func;
		}

		break;

	case MHL_RX_CONNECTED:
		pr_info("sii8246: MHL_RX_CONNECTED\n");
		ret = mhl_rx_connected_func(mhl);
		if (ret < 0)
			goto exit_func;

		if (mhl->status.op_status == MHL_DISCOVERY_FAIL) {
			sii8246_otg_disable(mhl);

			if (waitqueue_active(&mhl->cbus_cmd_wq)) {
				wake_up(&mhl->cbus_cmd_wq);
				pr_debug("sii8246 cbus: WakeUp CBUS CMD\n");
			}

			mhl->pdata->hw_reset();
			ret = mhl_init_func(mhl);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			mhl->status.intr1_mask_value = 0;
			mhl->status.intr2_mask_value = 0;
			mhl->status.intr3_mask_value = 0;
			mhl->status.intr4_mask_value = BIT_INTR4_RGND;
			mhl->status.intr5_mask_value = 0;
			mhl->status.intr7_mask_value = 0;
			mhl->status.intr8_mask_value = 0;
			mhl->status.intr_cbus_mask_value = 0;
			mhl->status.intr_cbus0_mask_value = 0;
			mhl->status.intr_cbus1_mask_value = 0;
			mhl->status.intr_cbus2_mask_value = 0;
			ret = mhl_tx_interrupt_set(mhl);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			ret = switch_to_d3_mode(mhl);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			goto exit_func;
		} else if (mhl->status.op_status == MHL_DISCOVERY_ON) {
			if (waitqueue_active(&mhl->cbus_cmd_wq)) {
				wake_up(&mhl->cbus_cmd_wq);
				pr_debug("sii8246 cbus: (%d)WakeUp CBUS CMD\n",
					__LINE__);
			}
		} else if (mhl->status.op_status == MHL_RX_CONNECTED) {
			pr_info("sii8246: %s():%d MHL_RX_CONNECTED\n",
							 __func__, __LINE__);
		}

		break;

	case MHL_DISCOVERY_SUCCESS:
		ret = mhl_tx_intr_handle(mhl);
		if (ret < 0) {
			pr_err("sii8246: %s():%d stop !\n",
							 __func__, __LINE__);
			goto exit_func;
		}

		status = mhl->status.op_status;

		if (status == MHL_RX_DISCONNECTED) {
			if (waitqueue_active(&mhl->cbus_cmd_wq)) {
				wake_up(&mhl->cbus_cmd_wq);
				pr_debug("sii8246 cbus: (%d)WakeUp CBUS CMD\n",
					__LINE__);
			}

			mhl->pdata->hw_reset();
			ret = mhl_init_func(mhl);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);

				goto exit_func;
			}

			mhl->status.intr1_mask_value = 0;
			mhl->status.intr2_mask_value = 0;
			mhl->status.intr3_mask_value = 0;
			mhl->status.intr4_mask_value = BIT_INTR4_RGND;
			mhl->status.intr5_mask_value = 0;
			mhl->status.intr7_mask_value = 0;
			mhl->status.intr8_mask_value = 0;
			mhl->status.intr_cbus_mask_value = 0;
			mhl->status.intr_cbus0_mask_value = 0;
			mhl->status.intr_cbus1_mask_value = 0;
			mhl->status.intr_cbus2_mask_value = 0;
			ret = mhl_tx_interrupt_set(mhl);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			ret = switch_to_d3_mode(mhl);
			if (ret < 0) {
				pr_err("sii8246: %s():%d failed !\n",
							 __func__, __LINE__);
				goto exit_func;
			}

			goto exit_func;
		}

		ret = cbus_link_check(mhl);
		if (ret < 0) {
			pr_err("sii8246: %s():%d failed !\n", __func__,
								 __LINE__);
			goto exit_func;
		}

		break;

	default:
		break;
	}

exit_func:

	if (ret < 0)
		pr_err("sii8246: %s():%d failed !\n", __func__, __LINE__);

	mutex_unlock(&mhl->mhl_lock);

	return IRQ_HANDLED;
}

#ifdef CONFIG_OF
/*FIXME, need to use more common/proper function
  for checking a VBUS regardless of H/W charger IC*/
static bool check_muic_mhl_cb(void)
{
	bool ret = true;
	union power_supply_propval value;
	psy_do_property("sec-charger", get, POWER_SUPPLY_PROP_ONLINE, value);
	pr_info("sec-charger : %d\n", value.intval);

	if (value.intval == POWER_SUPPLY_TYPE_BATTERY
		|| value.intval == POWER_SUPPLY_TYPE_WIRELESS)
		ret = false;

	pr_info("VBUS : %s in %s\n", ret ? "IN" : "OUT", __func__);
	return ret;
}

static void sii8246_charger_mhl_cb(bool otg_enable, int charger)
{
	struct mhl_tx *mhl = g_mhldev;
	union power_supply_propval value;
	int i, ret = 0;
	struct power_supply *psy;
	int current_cable_type = POWER_SUPPLY_TYPE_MISC;
	int sub_type = ONLINE_SUB_TYPE_MHL;
	int power_type = ONLINE_POWER_TYPE_UNKNOWN;
#ifdef CONFIG_MFD_MAX77803
	int muic_cable_type = max77803_muic_get_charging_type();
	pr_info("%s: muic cable_type = %d, otg_enable : %d, charger: %d\n",
			__func__, muic_cable_type, otg_enable, charger);

	switch (muic_cable_type) {
	case CABLE_TYPE_SMARTDOCK_MUIC:
	case CABLE_TYPE_SMARTDOCK_TA_MUIC:
	case CABLE_TYPE_SMARTDOCK_USB_MUIC:
		return;

	default:
		break;
	}

#else
	pr_info("%s, otg_en:%d, charger:%d\n", __func__, otg_enable, charger);
#endif

	if (charger == 0x00) {
		pr_info("%s() TA charger 500mA\n", __func__);
		power_type = ONLINE_POWER_TYPE_MHL_500;
	} else if (charger == 0x01) {
		pr_info("%s() TA charger 900mA\n", __func__);
		power_type = ONLINE_POWER_TYPE_MHL_900;
	} else if (charger == 0x02) {
		pr_info("%s() TA charger 1500mA\n", __func__);
		power_type = ONLINE_POWER_TYPE_MHL_1500;
	} else if (charger == 0x03) {
		pr_info("%s() USB charger\n", __func__);
		power_type = ONLINE_POWER_TYPE_USB;
	} else
		current_cable_type = POWER_SUPPLY_TYPE_BATTERY;

	if (otg_enable) {
		if (!check_muic_mhl_cb()) {
#ifdef CONFIG_SAMSUNG_LPM_MODE

			if (!poweroff_charging) {
#else
			{
#endif
				if (mhl->pdata->muic_otg_set)
					mhl->pdata->muic_otg_set(true);

				power_type = ONLINE_POWER_TYPE_UNKNOWN;
				current_cable_type = POWER_SUPPLY_TYPE_OTG;
			}
		}
	} else {
		if (mhl->pdata->muic_otg_set)
			mhl->pdata->muic_otg_set(false);
	}

	for (i = 0; i < 10; i++) {
		psy = power_supply_get_by_name("battery");

		if (psy)
			break;
	}

	if (i == 10) {
		pr_err(" %s: fail to get battery ps\n", __func__);
		return;
	}

	value.intval = current_cable_type << ONLINE_TYPE_MAIN_SHIFT
				   | sub_type << ONLINE_TYPE_SUB_SHIFT
				   | power_type << ONLINE_TYPE_PWR_SHIFT;
	ret = psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);

	if (ret) {
		pr_err(" %s: fail to set power_suppy ONLINE property(%d)\n",
			__func__, ret);
		return;
	}
}

static void of_sii8246_gpio_init(struct mhl_platform_data *pdata)
{
	if (pdata->gpio_mhl_en > 0) {
		if (gpio_request(pdata->gpio_mhl_en, "mhl_en")) {
			pr_err(" %s: unable to request gpio_mhl_en [%d]\n",
				__func__, pdata->gpio_mhl_en);
			return;
		}

		if (gpio_direction_output(pdata->gpio_mhl_en, 0)) {
			pr_err(" %s: unable to  gpio_mhl_en low[%d]\n",
				   __func__, pdata->gpio_mhl_en);
			return;
		}
	}

	if (pdata->gpio_mhl_reset > 0) {
		if (gpio_request(pdata->gpio_mhl_reset, "mhl_reset")) {
			pr_err(" %s: unable to request gpio_mhl_reset [%d]\n",
				__func__, pdata->gpio_mhl_reset);
			return;
		}

		if (gpio_direction_output(pdata->gpio_mhl_reset, 0)) {
			pr_err(" %s: unable to gpio_mhl_reset low[%d]\n",
				   __func__, pdata->gpio_mhl_reset);
			return;
		}
	}
}

static void of_sii8246_gpio_config(void)
{
	struct mhl_tx *mhl = g_mhldev;
	gpio_tlmm_config(GPIO_CFG(mhl->pdata->gpio_mhl_reset, 0,
			 GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
	gpio_tlmm_config(GPIO_CFG(mhl->pdata->gpio_mhl_en, 0, GPIO_CFG_INPUT,
			 GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), 1);
}

static void of_sii8246_hw_onoff(bool onoff)
{
	int ret;
	struct mhl_tx *mhl = g_mhldev;
	struct mhl_platform_data *pdata = mhl->pdata;
	pr_info("%s: Onoff: %d\n", __func__, onoff);

	if (onoff) {
		if (mhl->pdata->gpio_mhl_en > 0)
			gpio_set_value_cansleep(mhl->pdata->gpio_mhl_en, onoff);

		if (pdata->vcc_1p2v) {
			ret = regulator_set_voltage(pdata->vcc_1p2v, 1200000,
								 1200000);

			if (unlikely(ret < 0)) {
				pr_err(" regulator vcc_1p2v set_vtg failed rc\n");
				return;
			}

			ret = regulator_enable(pdata->vcc_1p2v);

			if (unlikely(ret < 0)) {
				pr_err(" regulator vcc_1p2v enabled failed rc\n");
				return;
			}
		}

		if (pdata->vcc_1p8v) {
			ret = regulator_set_voltage(pdata->vcc_1p8v, 1800000,
								 1800000);

			if (unlikely(ret < 0)) {
				pr_err(" regulator vcc 1p8v set_vtg failed rc\n");
				goto err_regulator_1p8v;
			}

			ret = regulator_enable(pdata->vcc_1p8v);

			if (unlikely(ret < 0)) {
				pr_err(" regulator vcc 1p8v enable failed rc\n");
				goto err_regulator_1p8v;
			}
		}

		if (pdata->vcc_3p3v) {
			ret = regulator_set_voltage(pdata->vcc_3p3v, 3300000,
								 3300000);

			if (unlikely(ret < 0)) {
				pr_err(" regulator vcc_3p3v set_vtg failed rc\n");
				goto err_regulator_3p3v;
			}

			ret = regulator_enable(pdata->vcc_3p3v);

			if (unlikely(ret < 0)) {
				pr_err(" regulator vcc_3p3v enable failed rc\n");
				goto err_regulator_3p3v;
			}
		}
	} else {
		if (mhl->gpio_mhl_en > 0)
			gpio_set_value_cansleep(mhl->gpio_mhl_en, onoff);

		if (pdata->vcc_1p2v) {
			ret = regulator_disable(pdata->vcc_1p2v);

			if (unlikely(ret < 0)) {
				pr_err(" regulator vcc_1p2v disable failed rc\n");
				return;
			}
		}

		if (pdata->vcc_1p8v) {
			ret = regulator_disable(pdata->vcc_1p8v);

			if (unlikely(ret < 0)) {
				pr_err(" regulator vcc_1p8v disable failed rc\n");
				return;
			}
		}

		if (pdata->vcc_3p3v) {
			ret = regulator_disable(pdata->vcc_3p3v);

			if (unlikely(ret < 0)) {
				pr_err(" regulator vcc_3pv3 disable failed rc\n");
				return;
			}
		}

		usleep_range(10000, 20000);

		if (mhl->gpio_mhl_reset > 0)
			gpio_set_value_cansleep(mhl->gpio_mhl_reset, 0);
	}

	return;
err_regulator_3p3v:

	if (pdata->vcc_1p8v)
		regulator_disable(pdata->vcc_1p8v);

err_regulator_1p8v:

	if (pdata->vcc_1p2v)
		regulator_disable(pdata->vcc_1p2v);
}


#if defined(CONFIG_MACH_HLTEDCM)
void of_sii8246_hw_poweron(bool enable)
{
	if (g_sii8246 && g_sii8246->pdata && g_sii8246->pdata->power)
		g_sii8246->pdata->power(enable);
	else
		pr_err("some pointer is not initialized (either pdata or pdata->power)\n");
}

#endif

static void of_sii8246_hw_reset(void)
{
	struct mhl_tx *mhl = g_mhldev;

	pr_info("%s: hw_reset\n" , __func__);
	usleep_range(10000, 20000);
	gpio_set_value_cansleep(mhl->pdata->gpio_mhl_reset, 1);
	usleep_range(5000, 20000);
	gpio_set_value_cansleep(mhl->pdata->gpio_mhl_reset, 0);
	usleep_range(10000, 20000);
	gpio_set_value_cansleep(mhl->pdata->gpio_mhl_reset, 1);
	msleep(30);
}

static int of_sii8246_parse_dt(struct mhl_platform_data *pdata)
{
	struct device_node *np = pdata->simgC8_tx_client->dev.of_node;
	struct device *pdev = &pdata->simgC8_tx_client->dev;
	pdata->gpio_mhl_irq = of_get_named_gpio_flags(np,
					"sii8246,gpio_mhl_irq", 0, NULL);

	if (pdata->gpio_mhl_irq > 0)
		pr_info("gpio: mhl_irq = %d\n", pdata->gpio_mhl_irq);

	pdata->gpio_mhl_reset = of_get_named_gpio_flags(np,
					"sii8246,gpio_mhl_reset", 0, NULL);

	if (pdata->gpio_mhl_reset > 0)
		pr_info("gpio: mhl_reset = %d\n", pdata->gpio_mhl_reset);

	pdata->gpio_mhl_wakeup = of_get_named_gpio_flags(np,
					"sii8246,gpio_mhl_wakeup", 0, NULL);

	if (pdata->gpio_mhl_wakeup > 0)
		pr_info("gpio: mhl_wakeup = %d\n", pdata->gpio_mhl_wakeup);

	pdata->gpio_mhl_scl = of_get_named_gpio_flags(np,
					"sii8246,gpio_mhl_scl", 0, NULL);

	if (pdata->gpio_mhl_scl > 0)
		pr_info("gpio: mhl_scl = %d\n",
				pdata->gpio_mhl_scl);

	pdata->gpio_mhl_sda = of_get_named_gpio_flags(np,
					"sii8246,gpio_mhl_sda", 0, NULL);

	if (pdata->gpio_mhl_sda > 0)
		pr_info("gpio: mhl_sda = %d\n", pdata->gpio_mhl_sda);

	pdata->gpio_mhl_en = of_get_named_gpio_flags(np,
					"sii8246,gpio_mhl_en", 0, NULL);

	if (pdata->gpio_mhl_en > 0)
		pr_info("gpio: mhl_en = %d\n", pdata->gpio_mhl_en);

	if (!of_property_read_u32(np, "sii8246,swing_level",
							 &pdata->swing_level))
		pr_info("swing_level = 0x%X\n", pdata->swing_level);

	pdata->vcc_1p2v = regulator_get(pdev, "vcc_1p2v");

	if (IS_ERR(pdata->vcc_1p2v)) {
		pr_err("sii8246,vcc_1p2v is not exist in device tree\n");
		pdata->vcc_1p2v = NULL;
	}

	pdata->vcc_1p8v = regulator_get(pdev, "vcc_1p8v");

	if (IS_ERR(pdata->vcc_1p8v)) {
		pr_err("sii8246,vcc_1p8v is not exist in device tree\n");
		pdata->vcc_1p8v = NULL;
	}

	pdata->vcc_3p3v = regulator_get(pdev, "vcc_3p3v");

	if (IS_ERR(pdata->vcc_3p3v)) {
		pr_err("sii8246,vcc_3p3v is not exist in device tree\n");
		pdata->vcc_3p3v = NULL;
	}

	return 0;
}

static int __devinit of_sii8246_probe_dt(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	const struct i2c_device_id *id_table = client->driver->id_table;
	static struct mhl_platform_data *pdata;
	u32 client_id = -1;

	if (!client->dev.of_node) {
		dev_err(&client->dev, "sii8246: Client node not-found\n");
		return -EIO;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	/* going to use block read/write, so check for this too */
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	if (of_property_read_u32(client->dev.of_node, "sii8246,client_id",
								 &client_id))
		dev_err(&client->dev, "Wrong Client_id# %d", client_id);

	if (0 == client_id) {
		pdata = kzalloc(sizeof(struct mhl_platform_data), GFP_KERNEL);

		if (!pdata) {
			dev_err(&client->dev, "failed to allocate driver data\n");
			return -ENOMEM;
		}

		pdata->simgC8_tx_client = client;
		pdata->power = of_sii8246_hw_onoff;
		pdata->hw_reset = of_sii8246_hw_reset;
		pdata->gpio_cfg = of_sii8246_gpio_config;
		pdata->muic_mhl_cb = sii8246_charger_mhl_cb;
#ifdef CONFIG_MFD_MAX77803
		pdata->muic_otg_set = muic_otg_control;
#endif
		of_sii8246_parse_dt(pdata);
		of_sii8246_gpio_init(pdata);
		client->dev.platform_data = pdata;
		pr_debug("debug_message : %s, %d", __func__, __LINE__);
		sii8246_i2cc8_probe(client, id_table);
	} else if (pdata) {
		client->dev.platform_data = pdata;

		if (1 == client_id)
			sii8246_i2c72_probe(client, id_table);
		else if (2 == client_id)
			sii8246_i2c7a_probe(client, id_table);
		else if (3 == client_id)
			sii8246_i2c92_probe(client, id_table);
	}

	return 0;
}
#endif /* CONFIG_OF */

static int __devinit sii8246_i2c72_probe(struct i2c_client *client,
						 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct tx_page0 *page0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	page0 = kzalloc(sizeof(struct tx_page0), GFP_KERNEL);

	if (!page0) {
		dev_err(&client->dev, "page0 failed to allocate driver data\n");
		return -ENOMEM;
	}

	page0->pdata = client->dev.platform_data;
	if (!page0->pdata) {
		pr_info("sii8246: SIMG72 no platform data\n");
		kfree(page0);
		return -EINVAL;
	}
	page0->pdata->simg72_tx_client = client;

	i2c_set_clientdata(client, page0);

	return 0;
}

static int __devinit sii8246_i2c7a_probe(struct i2c_client *client,
						 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct tx_page1 *page1;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	page1 = kzalloc(sizeof(struct tx_page1), GFP_KERNEL);

	if (!page1) {
		dev_err(&client->dev, "page1 failed to allocate driver data\n");
		return -ENOMEM;
	}

	page1->pdata = client->dev.platform_data;
	if (!page1->pdata) {
		pr_info("sii8246: SIMG7A no platform data\n");
		kfree(page1);
		return -EINVAL;
	}
	page1->pdata->simg7A_tx_client = client;

	i2c_set_clientdata(client, page1);

	return 0;
}

static int __devinit sii8246_i2c92_probe(struct i2c_client *client,
						 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct tx_page2 *page2;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	page2 = kzalloc(sizeof(struct tx_page2), GFP_KERNEL);

	if (!page2) {
		dev_err(&client->dev, "page2 failed to allocate driver data\n");
		return -ENOMEM;
	}

	page2->pdata = client->dev.platform_data;
	if (!page2->pdata) {
		pr_info("sii8246: SIMG92 no platform data\n");
		kfree(page2);
		return -EINVAL;
	}
	page2->pdata->simg92_tx_client = client;

	i2c_set_clientdata(client, page2);
	return 0;
}

static int __devinit sii8246_i2cc8_probe(struct i2c_client *client,
						 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct mhl_tx *mhl;
	int ret = 0;

	dev_info(&client->dev, "success client_addr 0x%X\n", client->addr);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	mhl = kzalloc(sizeof(struct mhl_tx), GFP_KERNEL);

	if (!mhl) {
		dev_err(&client->dev, " cbus failed to allocate driver data\n");
		return -ENOMEM;
	}

	mhl->pdata = client->dev.platform_data;

	if (!mhl->pdata) {
		dev_err(&client->dev, "failed to find platform data\n");
		ret = -EINVAL;
		goto err_exit0;
	}

	mhl->pdata->simgC8_tx_client = client;
	mhl->status.op_status = NO_MHL_STATUS;

	init_waitqueue_head(&mhl->cbus_cmd_wq);
	mutex_init(&mhl->cbus_cmd_lock);
	mutex_init(&mhl->mhl_lock);
	i2c_set_clientdata(client, mhl);
	g_mhldev = mhl;

	INIT_LIST_HEAD(&mhl->cbus_msg_list);
	INIT_WORK(&mhl->cbus_cmd_work, cbus_cmd_thread);

	INIT_WORK(&mhl->mhl_rsen_work, sii8246_rsen_work);
	setup_timer(&mhl->rsen_timer, sii8246_rsen_timer,
					(unsigned long)mhl);

	dev_info(&client->dev, "sii8246 irq : %d\n", client->irq);
	mhl->irq = client->irq;
	mhl->cbus_cmd_wqs = create_singlethread_workqueue("cbus_cmd_wqs");

	if (!mhl->cbus_cmd_wqs)
		return -ENOMEM;

	ret = request_threaded_irq(mhl->irq, NULL, mhl_irq_thread,
					IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
					 "mhl_intr", mhl);

	if (ret < 0) {
		pr_info("mhl_intr regist fail\n");
		kfree(mhl);
		return ret;
	}

#if defined(FEATURE_SS_MHL_RCP_SUPPORT)
	mhl->input_dev = register_mhl_input_device();
	input_set_drvdata(mhl->input_dev, mhl);
	ret = input_register_device(mhl->input_dev);
	if (ret < 0) {
		pr_warn("sii8246: Failed to create mhl rcp input_dev\n");
		input_free_device(mhl->input_dev);
	}
#endif
	wake_lock_init(&mhl->mhl_wake_lock,
			WAKE_LOCK_SUSPEND, "mhl_wake_lock");
	mhl->mhl_nb.notifier_call = sii8246_detection_callback;
	acc_register_notifier(&mhl->mhl_nb);

	disable_irq(mhl->irq);
	pr_info("sii8246: probe done\n");

	return 0;
err_exit0:
	kfree(mhl);
	return ret;
}

static int __devexit sii8246_i2c72_remove(struct i2c_client *client)
{
	return 0;
}

static int __devexit sii8246_i2c7a_remove(struct i2c_client *client)
{
	return 0;
}

static int __devexit sii8246_tpi_remove(struct i2c_client *client)
{
	return 0;
}

static int __devexit sii8246_i2cc8_remove(struct i2c_client *client)
{
	return 0;
}
#ifndef CONFIG_OF
static struct i2c_device_id sii8246_i2cc8_id[] = {
	{"sii8246_i2cc8", 0},
	{}
};
static struct i2c_device_id sii8246_i2c72_id[] = {
	{"sii8246_i2c72", 0},
	{}
};

static struct i2c_device_id sii8246_i2c7a_id[] = {
	{"sii8246_i2c7a", 0},
	{}
};

static struct i2c_device_id sii8246_i2c92_id[] = {
	{"sii8246_i2c92", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sii8246_i2cc8_id);
MODULE_DEVICE_TABLE(i2c, sii8246_i2c72_id);
MODULE_DEVICE_TABLE(i2c, sii8246_i2c7a_id);
MODULE_DEVICE_TABLE(i2c, sii8246_i2c92_id);

static struct i2c_driver sii8246_i2c72_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sii8246_i2c72",
	},
	.id_table	= sii8246_i2c72_id,
	.probe	= sii8246_i2c72_probe,
	.remove	= __devexit_p(sii8246_i2c72_remove),
	.command = NULL,
};

static struct i2c_driver sii8246_i2c7a_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sii8246_i2c7a",
	},
	.id_table	= sii8246_i2c7a_id,
	.probe	= sii8246_i2c7a_probe,
	.remove	= __devexit_p(sii8246_i2c7a_remove),
	.command = NULL,
};

static struct i2c_driver sii8246_i2c92_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sii8246_i2c92",
	},
	.id_table	= sii8246_i2c92_id,
	.probe	= sii8246_i2c92_probe,
	.remove	= __devexit_p(sii8246_tpi_remove),
	.command = NULL,
};

static struct i2c_driver sii8246_i2cc8_driver = {
	.driver = {
		.owner	= THIS_MODULE,
		.name	= "sii8246_i2cc8",
	},
	.id_table	= sii8246_i2cc8_id,
	.probe	= sii8246_i2cc8_probe,
	.remove	= __devexit_p(sii8246_i2cc8_remove),
	.command = NULL,
};
#endif

#if defined(FEATURE_SS_MHL_SWING_LEVEL)
static ssize_t mhl_swing_test_show(struct class *dev,
		struct class_attribute *attr, char *buf)
{
	struct mhl_tx *mhl = g_mhldev;

	u32 clk = (mhl->swing_level >> 3) & 0x07;
	u32 data = mhl->pdata->swing_level & 0x07;
	return snprintf(buf, PAGE_SIZE,
		"mhl_show_value:0x%02x(%d%d:Clk=%d,Data=%d)\n"
			, mhl->pdata->swing_level, clk, data, clk, data);

}
static ssize_t mhl_swing_test_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	struct mhl_tx *mhl = g_mhldev;

	if (buf[0] >= '0' && buf[0] <= '7' &&
			buf[1] >= '0' && buf[1] <= '7')
		mhl->pdata->swing_level = ((buf[0] - '0') << 3) |
			(buf[1] - '0');
	else
		mhl->pdata->swing_level = 0x74; /*Clk=6 and Data=4*/

	return size;
}
static CLASS_ATTR(swing, 0664,
		mhl_swing_test_show, mhl_swing_test_store);

#endif

#if defined(FEATURE_SS_MHL_TEST_APP_SUPPORT)
static ssize_t mhl_timing_test_show(struct class *dev,
		struct class_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hdmi_forced_resolution);

}

static ssize_t mhl_timing_test_store(struct class *dev,
		struct class_attribute *attr,
		const char *buf, size_t size)
{
	int timing, ret;

	ret = kstrtouint(buf, 10, &timing);
	if (unlikely(ret < 0))
		return size;

	if (timing >= 0 && timing <= 44)
		hdmi_forced_resolution = timing;
	else
		hdmi_forced_resolution = -1;

	return size;
}
static CLASS_ATTR(timing, 0664,
		mhl_timing_test_show, mhl_timing_test_store);
#endif

#if defined(FEATURE_SS_FACTORY)
#define SII_ID 0x82

static ssize_t mhl_test_show(struct class *dev,
				struct class_attribute *attr,
				char *buf)
{
	struct mhl_tx *mhl = g_mhldev;
	int size, ret;
	u8 sii_id = 0;

	mhl->pdata->power(1);
	mhl->pdata->hw_reset();

	ret = mhl_i2c_read_byte(mhl, PAGE_0, 0x03, &sii_id);
	if (ret < 0) {
		pr_info("mhl_i2c_read_byte error\n");
		return ret;
	}
	pr_info("sii8246: check mhl : 0x%X\n", sii_id);

	mhl->pdata->power(0);

	size = snprintf(buf, 10, "%d\n", sii_id == SII_ID ? 1 : 0);
	return size;
}
static CLASS_ATTR(test_result, 0664, mhl_test_show, NULL);

static ssize_t sii8246_mhl_on_store(struct class *class,
		struct class_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == '1') {
		pr_info("%s() MHL Attached !!\n", __func__);
		acc_notify(MHL_ATTACHED);
	} else {
		pr_info("%s() MHL Detached !!\n", __func__);
		acc_notify(MHL_DETACHED);
	}
	return size;
}

static CLASS_ATTR(mhl_on, 0660, NULL, sii8246_mhl_on_store);

#endif

static int __init sii8246_init(void)
{
	int ret = 0;
	struct class *sec_mhl = NULL;

#ifdef CONFIG_OF
	ret = i2c_add_driver(&sii8246_i2c_driver);

	if (ret < 0) {
		pr_err(" sii8246: mhl_v2 i2c driver init failed");
		return ret;
	}

#else
	ret = i2c_add_driver(&sii8246_i2c72_driver);
	if (ret != 0)
		goto err_exit1;

	ret = i2c_add_driver(&sii8246_i2c7a_driver);
	if (ret != 0)
		goto err_exit2;

	ret = i2c_add_driver(&sii8246_i2c92_driver);
	if (ret != 0)
		goto err_exit3;

	ret = i2c_add_driver(&sii8246_i2cc8_driver);
	if (ret != 0)
		goto err_exit4;
#endif

	sec_mhl = class_create(THIS_MODULE, "mhl");
	if (IS_ERR(sec_mhl)) {
		pr_warn("sii8246: Failed to create class(mhl)!\n");
		goto end_of_func;
	}

#if defined(FEATURE_SS_FACTORY)
	if (class_create_file(sec_mhl, &class_attr_test_result))
		pr_warn("sii8246: Failed to create test_result device file!\n");
	if (class_create_file(sec_mhl, &class_attr_mhl_on))
		pr_warn("sii8246: Failed to create mhl_on device file!\n");
#endif

#if defined(FEATURE_SS_MHL_SWING_LEVEL)
	if (class_create_file(sec_mhl, &class_attr_swing))
		pr_warn("sii8246: failed to create swing sysfs file\n");
#endif

#if defined(FEATURE_SS_MHL_TEST_APP_SUPPORT)
	if (class_create_file(sec_mhl, &class_attr_timing))
		pr_warn("sii8246: failed to create timing sysfs file\n");
#endif

end_of_func:
	pr_info("sii8246: init done\n");
	return 0;

#ifdef CONFIG_OF
	pr_info("sii8246: simg_driver_dt fail\n");
	i2c_del_driver(&sii8246_i2c_driver);
#else
err_exit4:
	pr_info("sii8246_i2cc8_driver fail\n");
	i2c_del_driver(&sii8246_i2cc8_driver);
err_exit3:
	pr_info("sii8246_i2c92_driver fail\n");
	i2c_del_driver(&sii8246_i2c92_driver);
err_exit2:
	pr_info("sii8246_i2c7a_driver fail\n");
	i2c_del_driver(&sii8246_i2c7a_driver);
err_exit1:
	pr_info("sii8246_i2c72_driver fail\n");
	i2c_del_driver(&sii8246_i2c72_driver);
#endif
	return ret;
}

static void __exit sii8246_exit(void)
{
#ifdef CONFIG_OF
	i2c_del_driver(&sii8246_i2c_driver);
#else
	i2c_del_driver(&sii8246_i2c72_driver);
	i2c_del_driver(&sii8246_i2c7a_driver);
	i2c_del_driver(&sii8246_i2c92_driver);
	i2c_del_driver(&sii8246_i2cc8_driver);
#endif
}

module_init(sii8246_init);
module_exit(sii8246_exit);

MODULE_DESCRIPTION("Silicon Image MHL Transmitter driver");
MODULE_AUTHOR("Rajucm <rajkumar.m@samsung.com>");
MODULE_LICENSE("GPL");
