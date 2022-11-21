/*
 * driver/ice40lm fpga driver
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
#include <linux/platform_data/ice40lm.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_sysfs.h>
#include <linux/firmware.h>

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#include "ice40lm_k_640.h"

/* to enable mclk */
#include <asm/io.h>
#include <linux/clk.h>

#define IRDA_TEST_CODE_SIZE	144
#define IRDA_TEST_CODE_ADDR	0x00
#define READ_LENGTH		8

#define US_TO_PATTERN		1000000
#define MIN_FREQ		20000

/* K 3G FW has a limitation of data register : 1000 */
enum {
	REG_LIMIT = 990,
};

struct ice4_fpga_data {
	struct workqueue_struct		*firmware_dl;
	struct delayed_work		fw_dl;
	const struct firmware		*fw;
	struct i2c_client *client;
	struct regulator *regulator;
	char *regulator_name;
	struct mutex mutex;
	struct {
		unsigned char addr;
		unsigned char data[REG_LIMIT];
	} i2c_block_transfer;
	int i2c_len;

	int ir_freq;
	int ir_sum;
	struct clk *clock;
};

enum {
	PWR_ALW,
	PWR_REG,
	PWR_LDO,
};

static struct ice40lm_platform_data *g_pdata;
static int fw_loaded = 0;
static bool send_success = false;
static int power_type = PWR_ALW;

/*
 * Send barcode emulator firmware data through spi communication
 * Firmware Update Code
 */
static int fw_data_send(const u8 *data, int length)
{
	unsigned int i,j;
	unsigned char spibit;

	i=0;
	while (i < length) {
		j=0;
		spibit = data[i];
		while (j < 8) {
			gpio_set_value(g_pdata->spi_clk_scl, GPIO_LEVEL_LOW);

			if (spibit & 0x80) {
				gpio_set_value(g_pdata->spi_si_sda,GPIO_LEVEL_HIGH);
			} else {
				gpio_set_value(g_pdata->spi_si_sda,GPIO_LEVEL_LOW);
			}
			j = j+1;
			gpio_set_value(g_pdata->spi_clk_scl, GPIO_LEVEL_HIGH);
			spibit = spibit<<1;
		}
		i = i+1;
	}

	i = 0;
	while (i < 200) {
		gpio_set_value(g_pdata->spi_clk_scl, GPIO_LEVEL_LOW);
		i = i+1;
		gpio_set_value(g_pdata->spi_clk_scl, GPIO_LEVEL_HIGH);
	}
	return 0;
}

static int fw_update_start(const u8 *data, int length)
{
	int retry_count = 0;

	pr_info("ice40: %s\n", __func__);

	gpio_set_value(g_pdata->creset_b, GPIO_LEVEL_LOW);
	usleep_range(30, 40);

	gpio_set_value(g_pdata->creset_b, GPIO_LEVEL_HIGH);
	usleep_range(1000, 1100);

	while(!ice4_check_cdone()) {
		usleep_range(10, 20);
		fw_data_send(data, length);
		usleep_range(50, 60);

		if (retry_count > 9) {
			pr_info("ice40lm firmware update is NOT loaded\n");
			break;
		} else {
			retry_count++;
		}
	}
	if (ice4_check_cdone()) {
		gpio_set_value(g_pdata->spi_en_rstn, GPIO_LEVEL_HIGH);
		pr_info("ice40lm firmware update success\n");
		fw_loaded = 1;
	} else {
		pr_info("Finally, fail to update ice40lm firmware!\n");
	}

	return 0;
}

void fw_update(struct work_struct *work)
{
	struct ice4_fpga_data *data =
		container_of(work, struct ice4_fpga_data, fw_dl.work);
	struct i2c_client *client = data->client;

	pr_info("ice40: %s\n", __func__);

	if (request_firmware(&data->fw, "ice40lm/k640.fw", &client->dev))
		pr_err("%s: Fail to open firmware file\n", __func__);
	else
		fw_update_start(data->fw->data, data->fw->size);
}

static int ice40lm_read(struct i2c_client *client, u16 addr, u8 length, u8 *value)
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
static void ice40lm_print_gpio_status(void)
{
/* to guess the status of CDONE during probe time */
	if (!fw_loaded)
		pr_info("%s : firmware is not loaded\n", __func__);
	else
		pr_info("%s : firmware is loaded\n", __func__);

	pr_info("%s : CDONE    : %d\n", __func__, gpio_get_value(g_pdata->cdone));
	pr_info("%s : RST_N    : %d\n", __func__, gpio_get_value(g_pdata->spi_en_rstn));
	pr_info("%s : CRESET_B : %d\n", __func__, gpio_get_value(g_pdata->creset_b));
}

static int ice4_check_cdone(void)
{
	if (!g_pdata->cdone) {
		pr_info("%s : no cdone pin data\n", __func__);
		return 0;
	}

	/* Device in Operation when CDONE='1'; Device Failed when CDONE='0'. */
	if (gpio_get_value(g_pdata->cdone) != 1) {
		pr_info("%s : CDONE_FAIL %d\n", __func__,
				gpio_get_value(g_pdata->cdone));
		return 0;
	}

	return 1;
}

static int ir_send_data_to_device(struct ice4_fpga_data *ir_data)
{

	struct ice4_fpga_data *data = ir_data;
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
			ice40lm_print_gpio_status();
		}
	}

#if defined(DEBUG)
	/* Registers can be read with special firmware */
//	ice40lm_read(client, IRDA_TEST_CODE_ADDR, buf_size, temp);

	for (i = 0 ; i < buf_size; i++)
		pr_info("[%s] %4d data %5x\n", __func__, i, data->i2c_block_transfer.data[i]);

	kfree(temp);
#endif

	mdelay(10);

	check_num = 0;

	if (gpio_get_value(g_pdata->irda_irq)) {
		pr_info("%s : %d before sending status NG\n", __func__, count_number);
		check_num = 1;
	} else {
		pr_info("%s : %d before sending status OK!\n", __func__, count_number);
		check_num = 2;
	}

	mutex_unlock(&data->mutex);

	emission_time = (1000 * (data->ir_sum) / (data->ir_freq));
	pr_info("%s : sum (%d) freq (%d) time (%d)\n", __func__, data->ir_sum, data->ir_freq, emission_time);
	if (emission_time > 0) {
		msleep(emission_time);
		pr_info("%s: emission_time = %d\n", __func__, emission_time);
	}

	while (!gpio_get_value(g_pdata->irda_irq)) {
		mdelay(10);
		pr_info("%s : %d try to check IRDA_IRQ\n", __func__, retry_count);
		retry_count++;

		if (retry_count > 5) {
			break;
		}
	}

	if (gpio_get_value(g_pdata->irda_irq)) {
		pr_info("%s : %d after  sending status OK!\n", __func__, count_number);
		check_num += 4;
	} else {
		pr_info("%s : %d after  sending status NG!\n", __func__, count_number);
		check_num += 2;
	}

	if (check_num != 6 || retry_count != 0) {
		pr_info("%s : fail to check the ir sending %d", __func__, check_num);
		return -1;
	} else {
		pr_info("%s done", __func__);
		return 0;
	}
}

static int ice40lm_set_data(struct ice4_fpga_data *data, int *arg, int count)
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
	int temp = 0, converting_factor;

	/* calculate total length for i2c transferring */
	data->i2c_len = irdata_arg_size * 2 + offset;

	pr_info("%s : count %d\n", __func__, count);
	pr_info("%s : irdata_arg_size %d\n", __func__, irdata_arg_size);
	pr_info("%s : irdata_size %d\n", __func__, irdata_size);
	pr_info("%s : i2c length %d\n", __func__, data->i2c_len);

	data->i2c_block_transfer.addr = 0x00;

	/* i2c address will be stored separatedly */
	data->i2c_block_transfer.data[0] = ((data->i2c_len - 1) >> 8) & 0xFF;
	data->i2c_block_transfer.data[1] = ((data->i2c_len - 1) >> 0) & 0xFF;

	data->i2c_block_transfer.data[2] = (arg[0] >> 16) & 0xFF;
	data->i2c_block_transfer.data[3] = (arg[0] >>  8) & 0xFF;
	data->i2c_block_transfer.data[4] = (arg[0] >>  0) & 0xFF;
	converting_factor = US_TO_PATTERN / arg[0];
	/* i2c address will be stored separatedly */
	offset--;

	/* arg[1]~[end] are the data for i2c transferring */
	for (i = 0 ; i < irdata_arg_size ; i++ ) {
#ifdef DEBUG
		pr_info("[%s] %d array value : %x\n", __func__, i, arg[i]);
#endif
		arg[i + 1] = (arg[i + 1] / converting_factor);
		data->i2c_block_transfer.data[i * 2 + offset] = arg[i+1] >> 8;
		data->i2c_block_transfer.data[i * 2 + offset + 1] = arg[i+1] & 0xFF;
	}

	/* +1 is for removing frequency data */
#ifdef CONFIG_REPEAT_ENABLE
	for (i = 1 ; i < irdata_arg_size ; i++)
		temp = temp + arg[i + 1];
#else
	for (i = 0 ; i < irdata_arg_size ; i++)
		temp = temp + arg[i + 1];
#endif

	data->ir_sum = temp;
	data->ir_freq = arg[0];

	return 0;
}

static void ir_led_power_control(bool led, struct ice4_fpga_data *data)
{
	int err;

	if (led) {
		if (power_type == PWR_REG) {
			err = regulator_enable(data->regulator);
			if (err)
				pr_info("%s: fail to turn on regulator\b", __func__);
		} else if (power_type == PWR_LDO) {
			gpio_set_value(g_pdata->ir_en, GPIO_LEVEL_HIGH);
		}
	} else {
		if (power_type == PWR_REG)
			regulator_disable(data->regulator);
		else if (power_type == PWR_LDO)
			gpio_set_value(g_pdata->ir_en, GPIO_LEVEL_LOW);
	}
}

/* start of sysfs code */
static ssize_t ir_send_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	int tdata, i, count = 0;

	int *arr_data;
	int err;

/* mclk on */
	clk_prepare_enable(data->clock);

	pr_info("%s : ir_send called with %d\n", __func__, size);
	if (!fw_loaded) {
		pr_info("%s : firmware is not loaded\n", __func__);
		return 1;
	}

	/* arr_data will store the decimal data(int) from sysfs(char) */
	/* every arr_data will be split by 2 (EX, 173(dec) will be 0x00(hex) and 0xAD(hex) */
	arr_data = kmalloc((sizeof(int) * (REG_LIMIT/2)), GFP_KERNEL);
	if (!arr_data) {
		pr_info("%s: fail to alloc\n", __func__);
		return size;
	}

	ir_led_power_control(true, data);

	for (i = 0 ; i < (REG_LIMIT/2) ; i++) {
		tdata = simple_strtoul(buf++, NULL, 10);
#ifdef DEBUG	/* debugging, it will cause lagging */
		pr_info("[%s] %d sysfs value : %x\n", __func__, i, tdata);
#endif
		if (tdata < 0) {
			pr_info("%s : error at simple_strtoul\n", __func__);
			break;
		} else if ((tdata == 0) && (i > 10)) {
			pr_info("%s : end of sysfs input(%d)\n", __func__, i);
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
		pr_info("[%s] OVERFLOW\n", __func__);
		goto err_overflow;
	}

	pr_info("[%s] string to array size : %d\n", __func__, count);

	err = ice40lm_set_data(data, arr_data, count);
	if (err != 0)
		pr_info("FAIL is possible?\n");

	err = ir_send_data_to_device(data);
	if (err != 0 && arr_data[0] > MIN_FREQ) {
		pr_info("%s: IR SEND might fail, retry!\n", __func__);

		err = ir_send_data_to_device(data);
		if (err != 0) {
			pr_info("%s: IR SEND might fail again\n", __func__);
			send_success = false;
		} else {
			send_success = true;
		}
	} else {
		send_success = true;
	}

err_overflow:
	data->ir_freq = 0;
	data->ir_sum = 0;

	kfree(arr_data);

	ir_led_power_control(false, data);

/* mclk off */
	clk_disable_unprepare(data->clock);

	return size;
}

static ssize_t ir_send_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
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

static DEVICE_ATTR(ir_send, 0664, ir_send_show, ir_send_store);

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
static DEVICE_ATTR(ir_send_result, 0664, ir_send_result_show, ir_send_result_store);

/* sysfs node irda_test */
static ssize_t irda_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret, i;
	struct ice4_fpga_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	struct {
			unsigned char addr;
			unsigned char data[IRDA_TEST_CODE_SIZE];
	} i2c_block_transfer;
	unsigned char BSR_data[IRDA_TEST_CODE_SIZE] = {
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

	temp = kzalloc(sizeof(u8)*(IRDA_TEST_CODE_SIZE+20), GFP_KERNEL);
	if (NULL == temp)
		pr_err("Failed to data allocate %s\n", __func__);
#endif


	if (gpio_get_value(g_pdata->cdone) != 1) {
		pr_err("%s: cdone fail !!\n", __func__);
		return 1;
	}
	pr_info("IRDA test code start\n");
	if (!fw_loaded) {
		pr_info("%s : firmware is not loaded\n", __func__);
		return 1;
	}

	ir_led_power_control(true, data);

	/* make data for sending */
	for (i = 0; i < IRDA_TEST_CODE_SIZE; i++)
		i2c_block_transfer.data[i] = BSR_data[i];

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
	ice40lm_read(client, IRDA_TEST_CODE_ADDR, IRDA_TEST_CODE_SIZE, temp);

	for (i = 0 ; i < IRDA_TEST_CODE_SIZE; i++)
		pr_info("[%s] %4d rd %5x, td %5x\n", __func__, i, i2c_block_transfer.data[i], temp[i]);

	ice40lm_print_gpio_status();
#endif
	mdelay(200);
	ir_led_power_control(false, data);

	return size;
}

static ssize_t irda_test_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return strlen(buf);
}

static DEVICE_ATTR(irda_test, 0664, irda_test_show, irda_test_store);

/* sysfs irda_version */
static ssize_t irda_ver_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ice4_fpga_data *data = dev_get_drvdata(dev);

	u8 fw_ver;
	ice40lm_read(data->client, 0x02, 1, &fw_ver);
	fw_ver = (fw_ver >> 2) & 0x3;

	return sprintf(buf, "%d\n", fw_ver);
}

static DEVICE_ATTR(irda_ver_check, 0664, irda_ver_check_show, NULL);

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
static int of_ice40lm_dt(struct device *dev, struct ice40lm_platform_data *pdata,
			struct ice4_fpga_data *data)
{
	struct device_node *np_ice40lm = dev->of_node;
	const char *temp_str;
	int ret;

	if(!np_ice40lm)
		return -EINVAL;

	pdata->creset_b = of_get_named_gpio(np_ice40lm, "ice40lm,creset_b", 0);
	pdata->cdone = of_get_named_gpio(np_ice40lm, "ice40lm,cdone", 0);
	pdata->irda_irq = of_get_named_gpio(np_ice40lm, "ice40lm,irda_irq", 0);
	pdata->spi_si_sda = of_get_named_gpio(np_ice40lm, "ice40lm,spi_si_sda", 0);
	pdata->spi_clk_scl = of_get_named_gpio(np_ice40lm, "ice40lm,spi_clk_scl", 0);
	pdata->spi_en_rstn = of_get_named_gpio(np_ice40lm, "ice40lm,spi_en_rstn", 0);

	ret = of_property_read_string(np_ice40lm, "clock-names", &temp_str);
	if (ret) {
		pr_err("%s: cannot get clock name(%d)", __func__, ret);
		return ret;
	}
	pr_info("%s: %s\n", __func__, temp_str);

	data->clock = devm_clk_get(dev, temp_str);
	if (IS_ERR(data->clock)) {
		pr_err("%s: cannot get clock(%d)", __func__, ret);
		return ret;
	}

	ret = of_property_read_string(np_ice40lm, "ice40lm,power", &temp_str);
	if (ret) {
		pr_err("%s: cannot get power type(%d)", __func__, ret);
		return ret;
	}

	if (!strncasecmp(temp_str, "LDO", 3)) {
		pr_info("%s: ice40lm will use %s\n", __func__, temp_str);
		power_type = PWR_LDO;
		pdata->ir_en = of_get_named_gpio(np_ice40lm, "ice40lm,ir_en", 0);
	} else if (!strncasecmp(temp_str, "REG", 3)) {
		pr_info("%s: ice40lm will use regulator\n", __func__);
		power_type = PWR_REG;
		ret = of_property_read_string(np_ice40lm, "ice40lm,regulator_name",
								&temp_str);
		if (ret) {
			pr_err("%s: cannot get regulator_name!(%d)\n",
				__func__, ret);
			return ret;
		}
		data->regulator_name = (char *)temp_str;
	} else if (!strncasecmp(temp_str, "ALW", 3)) {
		pr_info("%s: ice40lm power control(%s)\n", __func__, temp_str);
		power_type = PWR_ALW;
	} else {
		pr_err("%s: unused power type(%s)\n", __func__, temp_str);
		return -EINVAL;
	}

	return 0;
}
#else
static int of_ice40lm_dt(struct device *dev, struct ice40lm_platform_data *pdata)
{
	return -ENODEV;
}
#endif /* CONFIG_OF */

static int ice40lm_gpio_setting(void)
{
	int ret = 0;

	ret = gpio_request_one(g_pdata->creset_b, GPIOF_OUT_INIT_HIGH, "FPGA_CRESET_B");
	ret += gpio_request_one(g_pdata->cdone, GPIOF_IN, "FPGA_CDONE");
	ret += gpio_request_one(g_pdata->irda_irq, GPIOF_IN, "FPGA_IRDA_IRQ");
	ret += gpio_request_one(g_pdata->spi_en_rstn, GPIOF_OUT_INIT_LOW, "FPGA_SPI_EN");

	if (power_type == PWR_LDO) {
		if (g_pdata->spi_en_rstn != g_pdata->ir_en)
			ret += gpio_request_one(g_pdata->ir_en, GPIOF_OUT_INIT_LOW, "FPGA_IR_EN");
		else
			pr_info("%s: skip duplicated gpio request\n", __func__);
	}

/* i2c-gpio drv already request below pins */
	gpio_free(g_pdata->spi_si_sda);
	gpio_free(g_pdata->spi_clk_scl);
	ret += gpio_request_one(g_pdata->spi_si_sda, GPIOF_OUT_INIT_LOW, "FPGA_SPI_SI_SDA");
	ret += gpio_request_one(g_pdata->spi_clk_scl, GPIOF_OUT_INIT_LOW, "FPGA_SPI_CLK_SCL");

	return ret;
}

static int __devinit ice40lm_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct ice4_fpga_data *data;
	struct ice40lm_platform_data *pdata;
	struct device *ice40lm_dev;
	int error = 0;

	pr_info("%s probe!\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	data = kzalloc(sizeof(struct ice4_fpga_data), GFP_KERNEL);
	if (NULL == data) {
		pr_err("Failed to data allocate %s\n", __func__);
		error = -ENOMEM;
		goto alloc_fail;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct ice40lm_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			error =  -ENOMEM;
			goto platform_data_fail;
		}
		error = of_ice40lm_dt(&client->dev, pdata, data);
		if (error)
			goto platform_data_fail;
	}
	else
		pdata = client->dev.platform_data;

	g_pdata = pdata;
	error = ice40lm_gpio_setting();
	if (error)
		goto platform_data_fail;

	if (ice4_check_cdone()) {
		fw_loaded = 1;
		pr_info("FPGA FW is loaded!\n");
	} else {
		fw_loaded = 0;
		pr_info("FPGA FW is NOT loaded!\n");
	}

	data->client = client;
	mutex_init(&data->mutex);

	i2c_set_clientdata(client, data);

	/* slave address */
	client->addr = 0x50;

	if (power_type == PWR_REG) {
		data->regulator	= regulator_get(NULL, data->regulator_name);
		if (IS_ERR(data->regulator)) {
			pr_info("%s Failed to get regulator.\n", __func__);
			error = IS_ERR(data->regulator);
			goto platform_data_fail;
		}
	}

	ice40lm_dev = sec_device_create(data, "sec_ir");
	if (IS_ERR(ice40lm_dev))
		pr_err("Failed to create ice40lm_dev in sec_ir\n");

	if (sysfs_create_group(&ice40lm_dev->kobj, &sec_ir_attr_group) < 0) {
		sec_device_destroy(ice40lm_dev->devt);
		pr_err("Failed to create sysfs group for samsung ir!\n");
	}

	/* Create dedicated thread so that the delay of our work does not affect others */
	data->firmware_dl =
		create_singlethread_workqueue("ice40_firmware_dl");
	INIT_DELAYED_WORK(&data->fw_dl, fw_update);

	/* min 1ms is needed */
	queue_delayed_work(data->firmware_dl,
			&data->fw_dl, msecs_to_jiffies(20));


	pr_err("probe complete %s\n", __func__);

	return 0;

platform_data_fail:
	kfree(data);
alloc_fail:
	return error;
}

static int __devexit ice40lm_remove(struct i2c_client *client)
{
	struct ice4_fpga_data *data = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);
	kfree(data);
	return 0;
}

#ifdef CONFIG_PM
static int ice40lm_suspend(struct device *dev)
{
	gpio_set_value(g_pdata->spi_en_rstn, GPIO_LEVEL_LOW);
	return 0;
}

static int ice40lm_resume(struct device *dev)
{
	gpio_set_value(g_pdata->spi_en_rstn, GPIO_LEVEL_HIGH);
	return 0;
}

static const struct dev_pm_ops ice40lm_pm_ops = {
	.suspend	= ice40lm_suspend,
	.resume		= ice40lm_resume,
};
#endif

static const struct i2c_device_id ice4_id[] = {
	{"ice4", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ice4_id);


#if defined(CONFIG_OF)
static struct of_device_id ice40lm_match_table[] = {
	{ .compatible = "lattice,ice40lm",},
	{},
};
#else
#define ice40lm_match_table	NULL
#endif

static struct i2c_driver ice4_i2c_driver = {
	.driver = {
		.name	= "ice4",
		.of_match_table = ice40lm_match_table,
#ifdef CONFIG_PM
		.pm	= &ice40lm_pm_ops,
#endif
	},
	.probe = ice40lm_probe,
	.remove = __devexit_p(ice40lm_remove),
	.id_table = ice4_id,
};

static int __init ice40lm_init(void)
{
	return i2c_add_driver(&ice4_i2c_driver);
}
late_initcall(ice40lm_init);

static void __exit ice40lm_exit(void)
{
	i2c_del_driver(&ice4_i2c_driver);
}
module_exit(ice40lm_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ICE40LM FPGA FOR IRDA");
