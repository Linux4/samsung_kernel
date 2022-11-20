/*
 * driver/irled/ice5lp.c fpga driver
 *
 * Copyright (C) 2013 Samsung Electronics
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
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/i2c-gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/err.h>
#include <linux/platform_data/ice5lp.h>
#include <linux/sec_sysfs.h>
#include <linux/firmware.h>

#ifdef CONFIG_OF
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#include "ice5lp_fw.h"
#include "irled-common.h"

/* to enable mclk */
#include <linux/io.h>
#include <linux/clk.h>

#define IRDA_TEST_CODE_SIZE	144
#define IRDA_TEST_CODE_ADDR	0x00
#define READ_LENGTH		8

/* #define CDONE_ENABLE */

/* K 3G FW has a limitation of data register : 1000 */
enum {
	REG_LIMIT = 990,
};

struct ice5_fpga_data {
	struct workqueue_struct		*firmware_dl;
	struct delayed_work		fw_dl;
	const struct firmware		*fw;
	struct i2c_client *client;
	struct mutex mutex;
	struct {
		unsigned char addr;
		unsigned char data[REG_LIMIT];
	} i2c_block_transfer;
	int i2c_len;

	int ir_freq;
	int ir_sum;
	struct clk *clock;
	struct irled_power irled_power;
};

static struct ice5lp_platform_data *g_pdata;
static int fw_loaded;
static bool send_success;

#ifdef CDONE_ENABLE
static int ice5_check_cdone(void);
#endif


/*
 * Send barcode emulator firmware data through spi communication
 * Firmware Update Code
 */
static int fw_data_send(const u8 *data, int length)
{
	unsigned int i, j;

	for (i = 0; i < length; i++) {
		unsigned char spibit;

		spibit = data[i];
		for (j = 0; j < 8; j++) {
			gpio_set_value(g_pdata->spi_clk_scl, GPIO_LEVEL_LOW);

			if (spibit & 0x80)
				gpio_set_value(g_pdata->spi_si_sda,	GPIO_LEVEL_HIGH);
			else
				gpio_set_value(g_pdata->spi_si_sda,	GPIO_LEVEL_LOW);

			gpio_set_value(g_pdata->spi_clk_scl, GPIO_LEVEL_HIGH);
			spibit = spibit<<1;
		}
	}

	for (i = 0; i < 200; i++) {
		gpio_set_value(g_pdata->spi_clk_scl, GPIO_LEVEL_LOW);
		gpio_set_value(g_pdata->spi_clk_scl, GPIO_LEVEL_HIGH);
	}

	return 0;
}

static int fw_update_start(const u8 *data, int length)
{
#ifdef CDONE_ENABLE
	int retry_count = 0;
#endif
	int error = 0;

	pr_info("%s\n", __func__);

	gpio_set_value(g_pdata->creset_b, GPIO_LEVEL_LOW);
	usleep_range(30, 40);

	gpio_set_value(g_pdata->creset_b, GPIO_LEVEL_HIGH);
	usleep_range(1000, 1100);

#ifdef CDONE_ENABLE
	while (!ice5_check_cdone()) {
		usleep_range(10, 20);
		fw_data_send(data, length);
		usleep_range(50, 60);

		if (retry_count > 9) {
			pr_info("%s: firmware update error\n", __func__);
			error = EBUSY;
			break;
		}

		retry_count++;
		if (ice5_check_cdone()) {
			gpio_set_value(g_pdata->spi_en_rstn, GPIO_LEVEL_HIGH);
			pr_info("%s: firmware update success\n", __func__);
			fw_loaded = 1;
			break;
		} else {
			pr_info("%s: firmware update fail\n", __func__);
		}
	}
#else
	fw_data_send(data, length);
	usleep_range(50, 60);

	gpio_set_value(g_pdata->spi_en_rstn, GPIO_LEVEL_HIGH);
	pr_info("%s: firmware update success\n", __func__);
	fw_loaded = 1;
#endif

	return error;
}

void fw_update(struct work_struct *work)
{
	pr_info("%s\n", __func__);
	fw_update_start(spiword, sizeof(spiword));
}

static int ice5lp_read(struct i2c_client *client, u16 addr, u8 length, u8 *value)
{

	struct i2c_msg msg[2];
	int ret;

	msg[0].addr  = client->addr;
	msg[0].flags = 0x00;
	msg[0].len   = 1;
	msg[0].buf   = (u8 *) &addr;

	msg[1].addr  = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len   = length;
	msg[1].buf   = (u8 *) value;

	ret = i2c_transfer(client->adapter, msg, 2);
	if  (ret == 2) {
		return 0;
	} else {
		pr_info("%s: err1 %d\n", __func__, ret);
		return -EIO;
	}
}

/* When IR test does not work, we need to check some gpios' status */
static void ice5lp_print_gpio_status(void)
{
/* to guess the status of CDONE during probe time */
	if (!fw_loaded)
		pr_info("%s: firmware is not loaded\n", __func__);
	else
		pr_info("%s: firmware is loaded\n", __func__);


#ifdef CDONE_ENABLE
	pr_info("%s : CDONE    : %d\n", __func__, gpio_get_value(g_pdata->cdone));
#endif
	pr_info("%s : RST_N    : %d\n", __func__, gpio_get_value(g_pdata->spi_en_rstn));
	pr_info("%s : CRESET_B : %d\n", __func__, gpio_get_value(g_pdata->creset_b));
}

#ifdef CDONE_ENABLE
static int ice5_check_cdone(void)
{
	if (!g_pdata->cdone) {
		pr_info("%s: no cdone pin\n", __func__);
		return 0;
	}

	/* Device in Operation when CDONE='1'; Device Failed when CDONE='0'. */
	return gpio_get_value(g_pdata->cdone);
}
#endif

static int ir_send_data_to_device(struct ice5_fpga_data *ir_data)
{

	struct ice5_fpga_data *data = ir_data;
	struct i2c_client *client = data->client;
	int buf_size = data->i2c_len;
	int retry_count = 0;

	int ret;
	int emission_time;
	int check_num;
	static int count_number;

#if defined(DEBUG)
	u8 *temp;
	int i;

	temp = kzalloc(sizeof(u8)*(buf_size+20), GFP_KERNEL);
	if (NULL == temp)
		pr_err("Failed to data allocate %s\n", __func__);
#endif

	/* count the number of sending */
	if (count_number >= 100)
		count_number = 0;

	count_number++;

	pr_info("%s: total buf_size: %d\n", __func__, buf_size);

	mutex_lock(&data->mutex);
	ret = i2c_master_send(client, (unsigned char *) &(data->i2c_block_transfer), buf_size);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client, (unsigned char *) &(data->i2c_block_transfer), buf_size);
		if (ret < 0) {
			dev_err(&client->dev, "%s: err2 %d\n", __func__, ret);
			ice5lp_print_gpio_status();
		}
	}

#if defined(DEBUG)
	/* Registers can be read with special firmware */
	/*ice40lm_read(client, IRDA_TEST_CODE_ADDR, buf_size, temp);*/

	for (i = 0; i < buf_size; i++)
		pr_info("%s: %4d data %5x\n", __func__, i, data->i2c_block_transfer.data[i]);

	kfree(temp);
#endif

	mdelay(10);

	check_num = 0;

	if (gpio_get_value(g_pdata->irda_irq)) {
		pr_info("%s: %d before sending status NG\n", __func__, count_number);
		check_num = 1;
	} else {
		pr_info("%s: %d before sending status OK!\n", __func__, count_number);
		check_num = 2;
	}

	mutex_unlock(&data->mutex);

	emission_time = data->ir_sum / 1000;
	pr_info("%s: sum (%d) freq (%d) time (%d)\n", __func__, data->ir_sum, data->ir_freq, emission_time);
	if (emission_time > 0) {
		msleep(emission_time);
		pr_info("%s: emission_time = %d\n", __func__, emission_time);
	}

	while (!gpio_get_value(g_pdata->irda_irq)) {
		mdelay(10);
		pr_info("%s: %d try to check IRDA_IRQ\n", __func__, retry_count);
		retry_count++;

		if (retry_count > 5)
			break;
	}

	if (gpio_get_value(g_pdata->irda_irq)) {
		pr_info("%s: %d after  sending status OK!\n", __func__, count_number);
		check_num += 4;
	} else {
		pr_info("%s: %d after  sending status NG!\n", __func__, count_number);
		check_num += 2;
	}

	if (check_num != 6 || retry_count != 0) {
		pr_info("%s: fail to check the ir sending %d", __func__, check_num);
		return -1;
	} else {
		pr_info("%s: done", __func__);
		return 0;
	}
}

static int ice5lp_set_data(struct ice5_fpga_data *data, int *arg, int count)
{
	int i;
	/* offset means 1 (slave address) + 2 (data size) + 3 (frequency data) */
	int offset = 6;
	int irdata_arg_size = count - 1;
#ifdef CONFIG_REPEAT_ENABLE
#if 0
	/* SAMSUNG : upper word of the third byte in frequecy means the number of repeat frame */
	int nr_repeat_frame = (arg[0] >> 20) & 0xF;
#endif
	/* PEEL : 1206 */
	int nr_repeat_frame = (arg[1] >>  4) & 0xF;
#else
	int nr_repeat_frame = 0;
#endif
	/* remove the frequency data from the data length */
	int irdata_size = irdata_arg_size - nr_repeat_frame;
	int temp = 0;

	/* calculate total length for i2c transferring */
	data->i2c_len = irdata_arg_size * 4 + offset;

	pr_info("%s: count %d\n", __func__, count);
	pr_info("%s: irdata_arg_size %d\n", __func__, irdata_arg_size);
	pr_info("%s: irdata_size %d\n", __func__, irdata_size);
	pr_info("%s: i2c length %d\n", __func__, data->i2c_len);

	data->i2c_block_transfer.addr = 0x00;

	/* i2c address will be stored separatedly */
	data->i2c_block_transfer.data[0] = ((data->i2c_len - 1) >> 8) & 0xFF;
	data->i2c_block_transfer.data[1] = ((data->i2c_len - 1) >> 0) & 0xFF;

	data->i2c_block_transfer.data[2] = (arg[0] >> 16) & 0xFF;
	data->i2c_block_transfer.data[3] = (arg[0] >>  8) & 0xFF;
	data->i2c_block_transfer.data[4] = (arg[0] >>  0) & 0xFF;

	/* i2c address will be stored separatedly */
	offset--;

	/* arg[1]~[end] are the data for i2c transferring */
	for (i = 0; i < irdata_arg_size; i++) {
#ifdef DEBUG
		pr_info("%s: %d array value : %x\n", __func__, i, arg[i]);
#endif
		data->i2c_block_transfer.data[i * 4 + offset + 0] = (arg[i+1] >> 24) & 0xFF;
		data->i2c_block_transfer.data[i * 4 + offset + 1] = (arg[i+1] >> 16) & 0xFF;
		data->i2c_block_transfer.data[i * 4 + offset + 2] = (arg[i+1] >>  8) & 0xFF;
		data->i2c_block_transfer.data[i * 4 + offset + 3] = (arg[i+1] >>  0) & 0xFF;
	}

	/* +1 is for removing frequency data */
#ifdef CONFIG_REPEAT_ENABLE
	for (i = 1; i < irdata_arg_size; i++)
		temp = temp + arg[i + 1];
#else
	for (i = 0; i < irdata_arg_size; i++)
		temp = temp + arg[i + 1];
#endif

	data->ir_sum = temp;
	data->ir_freq = arg[0];

	return 0;
}

/* start of sysfs code */
static ssize_t ir_send_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct ice5_fpga_data *data = dev_get_drvdata(dev);
	int tdata, i, count = 0;

	int *arr_data;
	int err;

	send_success = false;

	clk_prepare_enable(data->clock);

	pr_info("%s: ir_send called with %d\n", __func__, size);
	if (!fw_loaded) {
		pr_info("%s: firmware is not loaded\n", __func__);
		return 1;
	}

	/* arr_data will store the decimal data(int) from sysfs(char) */
	/* every arr_data will be split by 2 (EX, 173(dec) will be 0x00(hex) and 0xAD(hex) */
	arr_data = kmalloc((sizeof(int) * (REG_LIMIT/2)), GFP_KERNEL);
	if (!arr_data) {
		pr_info("%s: fail to alloc\n", __func__);
		return size;
	}

	err = irled_power_on(&data->irled_power);
	if (err) {
		pr_info("%s: power on error\n", __func__);
		goto fail;
	}

	for (i = 0; i < (REG_LIMIT/2); i++) {
		tdata = simple_strtol(buf++, NULL, 10);

#ifdef DEBUG	/* debugging, it will cause lagging */
		pr_info("%s: %d sysfs value(%x)\n", __func__, i, tdata);
#endif
		if (tdata < 0) {
			pr_info("%s: error at simple_strtoul\n", __func__);
			break;
		} else if ((tdata == 0) && (i > 10)) {
			pr_info("%s: end of sysfs input(%x)\n", __func__, i);
			break;
		}

		arr_data[count] = tdata;
		count++;

		while (tdata > 0) {
			buf++;
			tdata = tdata / 10;
		}
	}

	if (count >= (REG_LIMIT/2)) {
		pr_info("%s: overflow\n", __func__);
		goto fail;
	}

	pr_info("%s: string to array size(%x)\n", __func__, count);

	err = ice5lp_set_data(data, arr_data, count);
	if (err != 0)
		pr_info("%s: set data error\n", __func__);

	err = ir_send_data_to_device(data);
	if (err) {
		pr_info("%s: ir send might fail, retry\n", __func__);

		err = ir_send_data_to_device(data);
		if (err)
			pr_info("%s: ir send might fail, again\n", __func__);
	}

	if (!err)
		send_success = true;

fail:
	data->ir_freq = 0;
	data->ir_sum = 0;

	kfree(arr_data);

	err = irled_power_off(&data->irled_power);
	if (err)
		pr_info("%s: power off error\n", __func__);

	clk_disable_unprepare(data->clock);

	return size;
}

static ssize_t ir_send_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct ice5_fpga_data *data = dev_get_drvdata(dev);
	int i;
	char *bufp = buf;

	for (i = 5; i < REG_LIMIT - 1; i++) {
		if (data->i2c_block_transfer.data[i] == 0
			&& data->i2c_block_transfer.data[i+1] == 0)
			break;
		else
			bufp += sprintf(bufp, "%u,",
					data->i2c_block_transfer.data[i]);
	}
	return strlen(buf);
}

static DEVICE_ATTR(ir_send, S_IRUGO|S_IWUSR|S_IWGRP, ir_send_show, ir_send_store);

/* sysfs node ir_send_result */
static ssize_t ir_send_result_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	if (send_success)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static ssize_t ir_send_result_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}
static DEVICE_ATTR(ir_send_result, S_IRUGO|S_IWUSR|S_IWGRP, ir_send_result_show, ir_send_result_store);

/* sysfs node irda_test */
static ssize_t irda_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, i;
	struct ice5_fpga_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	struct {
			unsigned char addr;
			unsigned char data[IRDA_TEST_CODE_SIZE];
	} i2c_block_transfer;
	unsigned char bsr_data[IRDA_TEST_CODE_SIZE] = {
		0x00, 0x8D, 0x00, 0x96, 0x00, 0x01, 0x50, 0x00,
		0xA8, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x15, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x15, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F, 0x00, 0x15, 0x00,
		0x3F, 0x00, 0x15, 0x00, 0x3F
	};

#if defined(DEBUG)
		u8 *temp;
#endif

	if (gpio_get_value(g_pdata->cdone) != 1) {
		pr_info("%s: cdone error", __func__);
		return 1;
	}

	if (!fw_loaded) {
		pr_info("%s: firmware is not loaded\n", __func__);
		return 1;
	}

	irled_power_on(&data->irled_power);

	/* make data for sending */
	for (i = 0; i < IRDA_TEST_CODE_SIZE; i++)
		i2c_block_transfer.data[i] = bsr_data[i];

	/* sending data by I2C */
	i2c_block_transfer.addr = IRDA_TEST_CODE_ADDR;
	ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer,
			IRDA_TEST_CODE_SIZE);
	if (ret < 0) {
		pr_err("%s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client,
		(unsigned char *) &i2c_block_transfer, IRDA_TEST_CODE_SIZE);
		if (ret < 0)
			pr_err("%s: err2 %d\n", __func__, ret);
	}

#if defined(DEBUG)
	temp = kzalloc(sizeof(u8)*(IRDA_TEST_CODE_SIZE+20), GFP_KERNEL);
	if (NULL == temp) {
		irled_power_off(&data->irled_power);
		pr_info("%s: kalloc error", __func__);
		return 1;
	}

	ice5lp_read(client, IRDA_TEST_CODE_ADDR, IRDA_TEST_CODE_SIZE, temp);

	for (i = 0; i < IRDA_TEST_CODE_SIZE; i++)
		pr_info("%s: %4d rd %5x, td %5x\n", __func__, i, i2c_block_transfer.data[i], temp[i]);

	ice5lp_print_gpio_status();

	kfree(temp);
#endif
	mdelay(200);
	irled_power_off(&data->irled_power);

	return size;
}

static ssize_t irda_test_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return strlen(buf);
}

static DEVICE_ATTR(irda_test, S_IRUGO|S_IWUSR|S_IWGRP, irda_test_show, irda_test_store);

/* sysfs irda_version */
static ssize_t irda_ver_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ice5_fpga_data *data = dev_get_drvdata(dev);

	u8 fw_ver;
	ice5lp_read(data->client, 0x02, 1, &fw_ver);
	fw_ver = (fw_ver >> 2) & 0x3;

	return sprintf(buf, "%d\n", fw_ver);
}


static DEVICE_ATTR(irda_ver_check, S_IRUGO, irda_ver_check_show, NULL);

static struct attribute *sec_ir_attributes[] = {
	&dev_attr_ir_send.attr,
	&dev_attr_ir_send_result.attr,
	&dev_attr_irda_test.attr,
	&dev_attr_irda_ver_check.attr,
	NULL,
};

static struct attribute_group sec_ir_attr_group = {
	.attrs = sec_ir_attributes,
};
/* end of sysfs code */

#if defined(CONFIG_OF)
static int of_ice5lp_dt(struct device *dev, struct ice5lp_platform_data *pdata,
			struct ice5_fpga_data *data)
{
	struct device_node *np_ice5lp = dev->of_node;
	const char *temp_str;
	int ret;

	if (!np_ice5lp) {
		pr_info("%s: invalid node", __func__);
		return -EINVAL;
	}

#ifdef CDONE_ENABLE
	pdata->cdone = of_get_named_gpio(np_ice5lp, "ice5lp,cdone", 0);
#endif
	pdata->creset_b = of_get_named_gpio(np_ice5lp, "ice5lp,creset_b", 0);
	pdata->irda_irq = of_get_named_gpio(np_ice5lp, "ice5lp,irda_irq", 0);
	pdata->spi_si_sda = of_get_named_gpio(np_ice5lp, "ice5lp,spi_si_sda", 0);
	pdata->spi_clk_scl = of_get_named_gpio(np_ice5lp, "ice5lp,spi_clk_scl", 0);
	pdata->spi_en_rstn = of_get_named_gpio(np_ice5lp, "ice5lp,spi_en_rstn", 0);

	ret = of_property_read_string(np_ice5lp, "clock-names", &temp_str);
	if (ret) {
		pr_info("%s: cannot get clock name(%d)\n", __func__, ret);
		return ret;
	}

	data->clock = devm_clk_get(dev, temp_str);
	if (IS_ERR(data->clock)) {
		pr_info("%s: cannot get clock(%d)\n", __func__, ret);
		return ret;
	}

	ret = of_irled_power_parse_dt(dev, &data->irled_power);

	return ret;
}
#else
static int of_ice5lp_dt(struct device *dev, struct ice5lp_platform_data *pdata)
{
	return -ENODEV;
}
#endif /* CONFIG_OF */

static int ice5lp_gpio_setting(void)
{
	int ret = 0;

	ret = gpio_request_one(g_pdata->creset_b, GPIOF_OUT_INIT_HIGH, "FPGA_CRESET_B");
#ifdef CDONE_ENABLE
	ret += gpio_request_one(g_pdata->cdone, GPIOF_IN, "FPGA_CDONE");
#endif
	ret += gpio_request_one(g_pdata->irda_irq, GPIOF_IN, "FPGA_IRDA_IRQ");
	ret += gpio_request_one(g_pdata->spi_en_rstn, GPIOF_OUT_INIT_LOW, "FPGA_SPI_EN");

	/* i2c-gpio drv already request below pins */
	gpio_free(g_pdata->spi_si_sda);
	gpio_free(g_pdata->spi_clk_scl);
	ret += gpio_request_one(g_pdata->spi_si_sda, GPIOF_OUT_INIT_LOW, "FPGA_SPI_SI_SDA");
	ret += gpio_request_one(g_pdata->spi_clk_scl, GPIOF_OUT_INIT_LOW, "FPGA_SPI_CLK_SCL");

	return ret;
}

static int ice5lp_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct ice5_fpga_data *data = NULL;
	struct ice5lp_platform_data *pdata;
	struct device *ice5lp_dev;
	int error = 0;

	pr_info("%s\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C)) {
		error = -EIO;
		pr_err("%s: i2c error\n", __func__);
		goto fail;
	}

	data = kzalloc(sizeof(struct ice5_fpga_data), GFP_KERNEL);
	if (NULL == data) {
		pr_info("%s: kalloc error\n", __func__);
		error = -ENOMEM;
		goto fail;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct ice5lp_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			pr_info("%s: kalloc error\n", __func__);
			error =  -ENOMEM;
			goto fail;
		}
		error = of_ice5lp_dt(&client->dev, pdata, data);
		if (error)
			goto fail;
	} else
		pdata = client->dev.platform_data;

	g_pdata = pdata;
	error = ice5lp_gpio_setting();
	if (error)
		goto fail;

	data->client = client;
	mutex_init(&data->mutex);

	i2c_set_clientdata(client, data);

	ice5lp_dev = sec_device_create(data, "sec_ir");
	if (IS_ERR(ice5lp_dev)) {
		pr_info("%s: create sec_ir error\n", __func__);
		error = IS_ERR(ice5lp_dev);
		goto fail;
	}

	error = sysfs_create_group(&ice5lp_dev->kobj, &sec_ir_attr_group);
	if (error) {
		pr_info("%s: create sysfs group for samsung ir\n", __func__);
		sec_device_destroy(ice5lp_dev->devt);
		goto fail;
	}

	/* Create dedicated thread so that the delay of our work does not affect others */
	fw_loaded = 0;
	data->firmware_dl =
		create_singlethread_workqueue("ice5_firmware_dl");
	INIT_DELAYED_WORK(&data->fw_dl, fw_update);

	/* min 1ms is needed */
	queue_delayed_work(data->firmware_dl,
			&data->fw_dl, msecs_to_jiffies(20));

	return 0;

fail:
	if (NULL != data)
		kfree(data);

	return error;
}

static int ice5lp_remove(struct i2c_client *client)
{
	struct ice5_fpga_data *data = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);
	kfree(data);
	return 0;
}

#ifdef CONFIG_PM
static int ice5lp_suspend(struct device *dev)
{
	gpio_set_value(g_pdata->spi_en_rstn, GPIO_LEVEL_LOW);
	return 0;
}

static int ice5lp_resume(struct device *dev)
{
	gpio_set_value(g_pdata->spi_en_rstn, GPIO_LEVEL_HIGH);
	return 0;
}

static const struct dev_pm_ops ice5lp_pm_ops = {
	.suspend	= ice5lp_suspend,
	.resume		= ice5lp_resume,
};
#endif

static const struct i2c_device_id ice5_id[] = {
	{"ice5lp", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ice5_id);


#if defined(CONFIG_OF)
static struct of_device_id ice5lp_match_table[] = {
	{ .compatible = "lattice,ice5lp",},
	{},
};
#else
#define ice5lp_match_table	NULL
#endif

static struct i2c_driver ice5lp_i2c_driver = {
	.driver = {
		.name	= "ice5lp",
		.of_match_table = ice5lp_match_table,
#ifdef CONFIG_PM
		.pm	= &ice5lp_pm_ops,
#endif
	},
	.probe = ice5lp_probe,
	.remove = __devexit_p(ice5lp_remove),
	.id_table = ice5_id,
};

static int __init ice5lp_init(void)
{
	int ret;

	ret = i2c_add_driver(&ice5lp_i2c_driver);
	pr_info("%s: ret(%d)\n", __func__, ret);

	return ret;
}
late_initcall(ice5lp_init);

static void __exit ice5lp_exit(void)
{
	pr_info("%s\n", __func__);

	i2c_del_driver(&ice5lp_i2c_driver);
}
module_exit(ice5lp_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ICE5LP FPGA FOR IRDA");
