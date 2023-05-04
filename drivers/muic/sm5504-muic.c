/*
 * driver/muic/sm5504.c - sm5504 micro USB switch device driver
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
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/wakelock.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/muic/muic.h>
#include <linux/muic/sm5504-muic.h>
#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic_notifier.h>
#endif

int g_adc;

static int sm5504_i2c_read_byte(const struct i2c_client *client, u8 command)
{
	int ret = 0;
	int retry = 0;

	ret = i2c_smbus_read_byte_data(client, command);

	while (ret < 0) {
		pr_err("%s:%s: reg(0x%x), retrying...\n", MUIC_DEV_NAME_SM5504, __func__, command);
		if (retry > 5) {
			pr_err("%s:%s: retry failed!!\n", MUIC_DEV_NAME_SM5504, __func__);
			break;
		}
		msleep(100);
		ret = i2c_smbus_read_byte_data(client, command);
		retry++;
	}

	pr_info("%s:%s: I2C Read - REG[0x%02x] DATA[0x%02x]\n", MUIC_DEV_NAME_SM5504, __func__, command, ret);

	return ret;
}

static int sm5504_i2c_write_byte(const struct i2c_client *client, u8 command, u8 value)
{
	int ret = 0;
	int retry = 0;
	int written = 0;

	ret = i2c_smbus_write_byte_data(client, command, value);
	pr_info("%s:%s: I2C Write - REG[0x%02x] DATA[0x%02x]\n", MUIC_DEV_NAME_SM5504, __func__, command, value);

	while (ret < 0) {
		pr_err("%s:%s: reg(0x%x), retrying...\n", MUIC_DEV_NAME_SM5504, __func__, command);
		if (retry > 5) {
			pr_err("%s:%s: retry failed!!\n", MUIC_DEV_NAME_SM5504, __func__);
			break;
		}
		msleep(100);
		ret = i2c_smbus_read_byte_data(client, command);
		if (written < 0)
			pr_err("%s:%s: reg(0x%x)\n", MUIC_DEV_NAME_SM5504, __func__, command);
		ret = i2c_smbus_write_byte_data(client, command, value);
		retry++;
	}

	return ret;
}

static int sm5504_muic_reg_init(struct sm5504_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;
	int int1 = 0, int2 = 0;
	int ctrl = 0, intmask1 = 0, intmask2 = 0;
	int adc = 0, dev1 = 0, dev2 = 0, mansw1 = 0, mansw2 = 0;

	ret = sm5504_i2c_write_byte(i2c, SM5504_MUIC_REG_CTRL, SM5504_CTRL_INIT);
	if (ret < 0)
		pr_err( "%s:%s: failed to write ctrl(%d)\n", MUIC_DEV_NAME_SM5504, __func__, ret);

	/* Read and Clear Interrupt1/2 */
	int1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT1);
	int2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT2);
	pr_info("%s:%s: INT1[0x%02x],INT2[0x%02x]\n", MUIC_DEV_NAME_SM5504, __func__, int1, int2);

	ctrl = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_CTRL);
	intmask1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT_MASK1);
	intmask2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT_MASK2);
	adc = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_ADC);

	dev1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_DEV_TYPE1);
	dev2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_DEV_TYPE2);
	mansw1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_MAN_SW1);
	mansw2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_MAN_SW2);

	pr_info("%s:%s: CTRL[0x%02x], INTMASK1[0x%02x], INTMASK2[0x%02x]\n", 
		MUIC_DEV_NAME_SM5504, __func__, ctrl, intmask1, intmask2);

	pr_info("%s:%s: ADC[0x%02x], DEV1[0x%02x], DEV2[0x%02x]\n", 
		MUIC_DEV_NAME_SM5504, __func__, adc, dev1, dev2);

	pr_info("%s:%s: MANSW1[0x%02x], MANSW2[0x%02x]n", 
		MUIC_DEV_NAME_SM5504, __func__, mansw1, mansw2);

	return ret;
}


static void sm5504_muic_detect_dev(struct sm5504_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int adc = 0;

	adc = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_ADC);
	
	pr_info("%s:%s: adc:0x%02x\n", MUIC_DEV_NAME_SM5504, __func__, adc);
		
	if (adc == ADC_KEYBOARDDOCK) {
		g_adc = ADC_KEYBOARDDOCK;
		keyboard_notifier_attach();
	} else {
		g_adc = ADC_OPEN;
		keyboard_notifier_detach();
	}

	muic_data->adc_rescan_count = 0;
}

static irqreturn_t sm5504_muic_irq_thread(int irq, void *data)
{
	struct sm5504_muic_data *muic_data = data;
	struct i2c_client *i2c = muic_data->i2c;
	int intr1 = 0, intr2 = 0;
	int ctrl = 0;
	int dev1 = 0, dev2 = 0;
	int adc = 0;
	int ret = 0;

	/* Read and Clear Interrupt1/2 */
	intr1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT1);
	intr2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_INT2);
	dev1 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_DEV_TYPE1);
	dev2 = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_DEV_TYPE2);
	adc = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_ADC);

	pr_info("%s:%s: INT1[0x%02x],INT2[0x%02x], DEV1[0x%02x], DEV2[0x%02x]\n", 
		MUIC_DEV_NAME_SM5504, __func__, intr1, intr2, dev1, dev2);

	ctrl = sm5504_i2c_read_byte(i2c, SM5504_MUIC_REG_CTRL);
	pr_info("%s:%s: CTRL[0x%02x]\n", MUIC_DEV_NAME_SM5504, __func__, ctrl);
	if (ctrl&0x01) {
		sm5504_muic_reg_init(muic_data);
		return IRQ_HANDLED;
	}

	/* adc fail check and rescan */
	if ( (intr1 & 0x01) && (dev1 == 0x00) && (dev2 == 0x00) && (adc == 0x1f) && (muic_data->adc_rescan_count < 5) ) {
		pr_info("%s:%s: adc scan fail!\n", MUIC_DEV_NAME_SM5504, __func__);
		ctrl &= ~(SM5504_ADC_EN);

		ret = sm5504_i2c_write_byte(i2c, SM5504_MUIC_REG_CTRL, ctrl);
		if (ret < 0) {
			pr_err( "%s:%s: failed to write ADC_EN bit (%d)\n", MUIC_DEV_NAME_SM5504, __func__, ret);
			return IRQ_HANDLED;
		}
		ctrl |= (SM5504_ADC_EN);
		ret = sm5504_i2c_write_byte(i2c, SM5504_MUIC_REG_CTRL, ctrl);
		if (ret < 0) {
			pr_err( "%s:%s: failed to write ADC_EN bit (%d)\n", MUIC_DEV_NAME_SM5504, __func__, ret);
			return IRQ_HANDLED;
		}
		muic_data->adc_rescan_count++;
		pr_info("%s:%s: adc rescan!\n", MUIC_DEV_NAME_SM5504, __func__);
		return IRQ_HANDLED;
	}

	mutex_lock(&muic_data->muic_mutex);
	wake_lock(&muic_data->muic_wake_lock);

	/* device detection */
	sm5504_muic_detect_dev(muic_data);
	
	wake_unlock(&muic_data->muic_wake_lock);
	mutex_unlock(&muic_data->muic_mutex);
	return IRQ_HANDLED;
}

static int sm5504_muic_irq_init(struct sm5504_muic_data *muic_data)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME_SM5504, __func__);

	if (!muic_data->irq_gpio) {
		pr_err("%s:%s: No interrupt specified\n", MUIC_DEV_NAME_SM5504, __func__);
		return -ENXIO;
	}
	i2c->irq = gpio_to_irq(muic_data->irq_gpio);

	if (i2c->irq) {
		ret = request_threaded_irq(i2c->irq, NULL,
				sm5504_muic_irq_thread,
				(IRQF_TRIGGER_LOW |
				IRQF_NO_SUSPEND | IRQF_ONESHOT),
				"sm5504-muic", muic_data);
		if (ret < 0) {
			pr_err("%s:%s: failed to request IRQ(%d)\n",
						MUIC_DEV_NAME_SM5504, __func__, i2c->irq);
			return ret;
		}

		ret = enable_irq_wake(i2c->irq);
		if (ret < 0)
			pr_err("%s:%s: failed to enable wakeup src\n", MUIC_DEV_NAME_SM5504, __func__);
	}

	return ret;
}


#if defined(CONFIG_OF)
static int of_sm5504_muic_dt(struct device *dev, struct sm5504_muic_data *muic_data)
{
	struct device_node *np_muic = dev->of_node;
	int ret = 0;

	if (np_muic == NULL) {
		pr_err("%s:%s: np NULL\n", MUIC_DEV_NAME_SM5504, __func__);
		return -EINVAL;
	} else {
		muic_data->irq_gpio =
			of_get_named_gpio(np_muic, "sm5504,irq-gpio", 0);
		if (muic_data->irq_gpio < 0) {
			pr_err("%s:%s: failed to get muic_irq = %d\n",
					MUIC_DEV_NAME_SM5504, __func__, muic_data->irq_gpio);
			muic_data->irq_gpio = 0;
		}
	}

	return ret;
}
#endif /* CONFIG_OF */


static int sm5504_muic_probe(struct i2c_client *i2c,
				const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(i2c->dev.parent);
	struct sm5504_muic_data *muic_data;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME_SM5504, __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s:%s: i2c functionality check error\n", MUIC_DEV_NAME_SM5504, __func__);
		ret = -EIO;
		goto err_return;
	}

	muic_data = kzalloc(sizeof(struct sm5504_muic_data), GFP_KERNEL);
	if (!muic_data) {
		pr_err("%s:%s: failed to allocate driver data\n", MUIC_DEV_NAME_SM5504, __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	/* save platfom data for gpio control functions */
	muic_data->dev = &i2c->dev;
	muic_data->i2c = i2c;

#if defined(CONFIG_OF)
	ret = of_sm5504_muic_dt(&i2c->dev, muic_data);
	if (ret < 0)
		pr_err("%s:%s: no muic dt! ret[%d]\n", MUIC_DEV_NAME_SM5504, __func__, ret);
#endif /* CONFIG_OF */

	i2c_set_clientdata(i2c, muic_data);

	mutex_init(&muic_data->muic_mutex);

	muic_data->keyboard_state = 0;
	muic_data->adc_rescan_count = 0;

	ret = sm5504_muic_reg_init(muic_data);
	if (ret) {
		pr_err("%s:%s: failed to init reg\n", MUIC_DEV_NAME_SM5504, __func__);
		goto fail;
	}

	ret = sm5504_muic_irq_init(muic_data);
	if (ret) {
		pr_err("%s:%s: failed to init irq(%d)\n", MUIC_DEV_NAME_SM5504, __func__, ret);
		goto fail_init_irq;
	}

	wake_lock_init(&muic_data->muic_wake_lock, WAKE_LOCK_SUSPEND, "muic_wake");

	/* initial cable detection */
	sm5504_muic_irq_thread(-1, muic_data);

	return 0;

fail_init_irq:
	if (i2c->irq)
		free_irq(i2c->irq, muic_data);
fail:
	mutex_destroy(&muic_data->muic_mutex);
	i2c_set_clientdata(i2c, NULL);
	kfree(muic_data);
err_return:
	return ret;
}

static const struct i2c_device_id sm5504_i2c_id[] = {
	{ "sm5504", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, sm5504_i2c_id);

#if defined(CONFIG_OF)
static struct of_device_id sec_muic_i2c_dt_ids[] = {
	{ .compatible = "sm5504,i2c" },
	{ }
};
MODULE_DEVICE_TABLE(of, sec_muic_i2c_dt_ids);
#endif /* CONFIG_OF */

static struct i2c_driver sm5504_muic_driver = {
	.driver = {
		.name = "sm5504",
		.of_match_table = of_match_ptr(sec_muic_i2c_dt_ids),
	},
	.probe		= sm5504_muic_probe,
	/* TODO: suspend resume remove shutdown */
	.id_table	= sm5504_i2c_id,
};

static int __init sm5504_muic_init(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME_SM5504, __func__);
	return i2c_add_driver(&sm5504_muic_driver);
}
module_init(sm5504_muic_init);

static void __exit sm5504_muic_exit(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME_SM5504, __func__);
	i2c_del_driver(&sm5504_muic_driver);
}
module_exit(sm5504_muic_exit);

MODULE_DESCRIPTION("SM5504 MUIC driver(KEYBOARD version)");
MODULE_LICENSE("GPL");
