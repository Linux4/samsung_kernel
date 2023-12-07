/*
 * driver/ice5lp_ledmatrix fpga driver
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
#include <linux/platform_data/ice5lp-ledmatrix.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_sysfs.h>
#include <linux/firmware.h>

#if defined (CONFIG_OF)
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#endif /* CONFIG_OF */

#include "ice5lp_ledmatrix_fw.h"

/* to enable mclk */
#include <asm/io.h>
#include <linux/clk.h>

//#define LEDMATRIX_TEST_CODE_SIZE	(52+48+4)
#define LEDMATRIX_TEST_CODE_SIZE	(56*10+8)
#define LEDMATRIX_TEST_CODE_ADDR	0x00
#define LEDMATRIX_VER_ADDR		0x07
#define LEDMATRIX_VER_ID		0xA2
#define LEDMATRIX_VER_ID_05T	0xA1
#define LEDMATRIX_VER_RETRY	5
#define READ_LENGTH		8

//#define US_TO_PATTERN		1000000
//#define MIN_FREQ		20000

/* K 3G FW has a limitation of data register : 1000 */
enum {
	REG_LIMIT = 990,
};

struct ice5_fpga_data {
	struct workqueue_struct		*firmware_dl;
	struct delayed_work		fw_dl;
	const struct firmware		*fw0;
	const struct firmware		*fw1;	
	struct i2c_client *client;
	struct regulator *regulator;
	char *regulator_name;
	struct mutex mutex;
	struct {
		unsigned char addr;
		unsigned char data[REG_LIMIT];
	} i2c_block_transfer;
	int i2c_len;

//	int ir_freq;
//	int ir_sum;
//	struct clk *clock;
};

enum {
	PWR_ALW,
	PWR_REG,
	PWR_LDO,
};

static struct ice5lp_ledmatrix_platform_data *g_pdata;
static int fw_loaded = 0;
static bool send_success = false;
static int power_type = PWR_LDO;

/*
 * Send ledmatrix firmware data through spi communication
 * Firmware Update Code
 */

static int ice5lp_fw_data_send(const u8 *data, int length)
{
	unsigned int i,j, k;
	unsigned char spibit;

	i=0;
	k=0;
	pr_info("ice5: %s : before spi_clk_scl : %d\n", __func__, gpio_get_value(g_pdata->spi_clk_scl));
	pr_info("ice5: %s : before spi_si_sda : %d\n", __func__, gpio_get_value(g_pdata->spi_si_sda));
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
		k=k+1;
#if 0 // for debug on download		
		if((k%4)==0 && k<=120) 
			pr_info("ice5: %02X%02X%02X%02X\n", data[k-4], data[k-3], data[k-2], data[k-1]);
#endif		
	}

	i = 0;
	while (i < 200) {
		gpio_set_value(g_pdata->spi_clk_scl, GPIO_LEVEL_LOW);
		i = i+1;
		gpio_set_value(g_pdata->spi_clk_scl, GPIO_LEVEL_HIGH);
	}

	pr_info("ice5: %s : after spi_clk_scl : %d\n", __func__, gpio_get_value(g_pdata->spi_clk_scl));
	pr_info("ice5: %s : after spi_si_sda : %d\n", __func__, gpio_get_value(g_pdata->spi_si_sda));	
	return 0;
}
 
static int ice5lp_fw_update_start(const u8 *data, int length, int creset)
{

	pr_info("ice5: %s\n", __func__);

	gpio_set_value(creset, GPIO_LEVEL_LOW);
	pr_info("ice5: %s : creset LOW : %d\n", __func__, gpio_get_value(creset));

	usleep_range(30, 40);

	gpio_set_value(creset, GPIO_LEVEL_HIGH);
	pr_info("ice5: %s : creset HIGH : %d\n", __func__, gpio_get_value(creset));
	usleep_range(1000, 1100);

	usleep_range(10, 20);
	ice5lp_fw_data_send(data, length);
	usleep_range(50, 60);

	return 0;
}

void ice5lp_fw_update(struct work_struct *work)
{
	struct ice5_fpga_data *data =
		container_of(work, struct ice5_fpga_data, fw_dl.work);
	struct i2c_client *client = data->client;
	int ret;
	int retry_count = 0;
	
	pr_info("ice5: %s\n", __func__);
	
	gpio_free(g_pdata->spi_clk_scl);
	gpio_free(g_pdata->spi_si_sda);

	gpio_request_one(g_pdata->creset_0, GPIOF_OUT_INIT_LOW, "creset_0");
	gpio_request_one(g_pdata->creset_1, GPIOF_OUT_INIT_LOW, "creset_1");
	gpio_request_one(g_pdata->fpga_reset, GPIOF_OUT_INIT_LOW, "fpga_reset");
	
retry_fw_dn:

	ret = gpio_request_one(g_pdata->spi_si_sda, GPIOF_OUT_INIT_HIGH, "spi_si_sda");
	if(ret){
		pr_err("%s: cannot request spi_si_sda(%d)", __func__, ret);
	}

	ret = gpio_request_one(g_pdata->spi_clk_scl, GPIOF_OUT_INIT_HIGH, "spi_clk_scl");
	if(ret){
		pr_err("%s: cannot request spi_clk_scl(%d)", __func__, ret);
	}

	if(retry_count%2==0)
	{
		if (request_firmware(&data->fw1, "ice50/EINK1.fw", &client->dev))
			pr_err("%s: Fail to open firmware 1 file\n", __func__);
		else
			ice5lp_fw_update_start(data->fw1->data, data->fw1->size, g_pdata->creset_1);


		usleep_range(1000, 1100);

		if (request_firmware(&data->fw0, "ice50/EINK0.fw", &client->dev))
			pr_err("%s: Fail to open firmware 0 file\n", __func__);
		else
			ice5lp_fw_update_start(data->fw0->data, data->fw0->size, g_pdata->creset_0);
	}
	else
	{
		if (request_firmware(&data->fw1, "ice50/EINK1_05T.fw", &client->dev))
			pr_err("%s: Fail to open firmware 05T 1 file\n", __func__);
		else
			ice5lp_fw_update_start(data->fw1->data, data->fw1->size, g_pdata->creset_1);


		usleep_range(1000, 1100);

		if (request_firmware(&data->fw0, "ice50/EINK0_05T.fw", &client->dev))
			pr_err("%s: Fail to open firmware 05T 0 file\n", __func__);
		else
			ice5lp_fw_update_start(data->fw0->data, data->fw0->size, g_pdata->creset_0);	
	}
	gpio_set_value(g_pdata->spi_clk_scl, GPIO_LEVEL_LOW);
	gpio_set_value(g_pdata->spi_si_sda,GPIO_LEVEL_LOW);
	usleep_range(1000, 1100);
	gpio_set_value(g_pdata->spi_clk_scl, GPIO_LEVEL_HIGH);
	gpio_set_value(g_pdata->spi_si_sda,GPIO_LEVEL_HIGH);
	usleep_range(30, 40);
	
	gpio_set_value(g_pdata->fpga_reset, GPIO_LEVEL_HIGH);
	usleep_range(30, 40);
	gpio_set_value(g_pdata->fpga_reset, GPIO_LEVEL_LOW);
	usleep_range(30, 40);
	gpio_set_value(g_pdata->fpga_reset, GPIO_LEVEL_HIGH);
	
	gpio_free(g_pdata->spi_clk_scl);
	gpio_free(g_pdata->spi_si_sda);

	gpio_request(g_pdata->spi_si_sda, "sda");
	gpio_request(g_pdata->spi_clk_scl, "scl");

	if (ice5lp_check_fwdone(client)) {
		pr_info("ice5lp_ledmatrix firmware update success\n");
		usleep_range(30, 40);
		gpio_set_value(g_pdata->fpga_reset, GPIO_LEVEL_LOW);
		fw_loaded = 1;
	} else {
		retry_count++;
		if(retry_count > LEDMATRIX_VER_RETRY){
			pr_info("Finally, fail to update ice5lp_ledmatrix firmware!\n");
			gpio_set_value(g_pdata->fpga_reset, GPIO_LEVEL_LOW);
			return;
		}
		pr_info("fail to update ice5lp_ledmatrix firmware! retry %d\n",retry_count);
		gpio_free(g_pdata->spi_clk_scl);
		gpio_free(g_pdata->spi_si_sda);
		
		gpio_set_value(g_pdata->creset_0, GPIO_LEVEL_LOW);
		gpio_set_value(g_pdata->creset_1, GPIO_LEVEL_LOW);
		gpio_set_value(g_pdata->fpga_reset, GPIO_LEVEL_LOW);
		usleep_range(2000, 2100);
		goto retry_fw_dn;
		
	}
	
}

static int ice5lp_ledmatrix_read(struct i2c_client *client, u16 addr, u8 length, u8 *value)
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

static int ice5lp_check_fwdone(struct i2c_client *client)
{
	u8 fw_ver;
	int ret = 0;
	
	ice5lp_ledmatrix_read(client, LEDMATRIX_VER_ADDR, 1, &fw_ver);
	pr_info("%s: fw 0x%x\n", __func__, fw_ver);
	if(fw_ver == LEDMATRIX_VER_ID)
		ret = 1;
	else if(fw_ver == LEDMATRIX_VER_ID_05T)
		ret = 1;
	else
		ret = 0;

	return ret;
}

#if 0
static int ice5lp_send_data_to_device(struct ice5_fpga_data *ir_data)
{
	struct ice5_fpga_data *data = ir_data;
	struct i2c_client *client = data->client;
	int buf_size = data->i2c_len;
//	int retry_count = 0;

	int ret;
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
//			ice5lp_ledmatrix_print_gpio_status();
		}
	}

#if defined(DEBUG)
	/* Registers can be read with special firmware */
//	ice5lp_ledmatrix_read(client, LEDMATRIX_TEST_CODE_ADDR, buf_size, temp);

	for (i = 0 ; i < buf_size; i++)
		pr_info("[%s] %4d data %5x\n", __func__, i, data->i2c_block_transfer.data[i]);

	kfree(temp);
#endif

	mdelay(10);

	mutex_unlock(&data->mutex);

	pr_info("%s done", __func__);
	return 0;

}

static int ice5lp_ledmatrix_set_data(struct ice5_fpga_data *data, int *arg, int count)
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

//	data->ir_sum = temp;
//	data->ir_freq = arg[0];

	return 0;
}

static void ledmatrix_led_power_control(bool led, struct ice5_fpga_data *data)
{
	int err;

	if (led) {
		if (power_type == PWR_REG) {
			err = regulator_enable(data->regulator);
			if (err)
				pr_info("%s: fail to turn on regulator\b", __func__);
		} else if (power_type == PWR_LDO) {
			gpio_set_value(g_pdata->ledmatrix_en, GPIO_LEVEL_HIGH);
		}
	} else {
		if (power_type == PWR_REG)
			regulator_disable(data->regulator);
		else if (power_type == PWR_LDO)
			gpio_set_value(g_pdata->ledmatrix_en, GPIO_LEVEL_LOW);
	}
}
#endif

/* start of sysfs code */
static ssize_t ledmatrix_send_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
#if 0
	struct ice5_fpga_data *data = dev_get_drvdata(dev);
	int tdata, i, count = 0;

	int *arr_data;
	int err;

/* mclk on */
//	clk_prepare_enable(data->clock);

	pr_info("%s : ir_send called with %d\n", __func__, (int)size);
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

	ledmatrix_led_power_control(true, data);

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

	err = ice5lp_ledmatrix_set_data(data, arr_data, count);
	if (err != 0)
		pr_info("FAIL is possible?\n");

	err = ice5lp_send_data_to_device(data);
	if (err != 0 && arr_data[0] > MIN_FREQ) {
		pr_info("%s: IR SEND might fail, retry!\n", __func__);

		err = ice5lp_send_data_to_device(data);
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
//	data->ir_freq = 0;
//	data->ir_sum = 0;

	kfree(arr_data);

	ledmatrix_led_power_control(false, data);

/* mclk off */
//	clk_disable_unprepare(data->clock);
#endif
	return size;
}

static ssize_t ledmatrix_send_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
#if 0
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
#endif	
	return strlen(buf);
}

static DEVICE_ATTR(ledmatrix_send, 0660, ledmatrix_send_show, ledmatrix_send_store);

/* sysfs node ir_send_result */
static ssize_t ledmatrix_send_result_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	if (send_success)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static ssize_t ledmatrix_send_result_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	return size;
}
static DEVICE_ATTR(ledmatrix_send_result, 0660, ledmatrix_send_result_show, ledmatrix_send_result_store);

/* sysfs node ledmatrix_test */
static ssize_t ledmatrix_test_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int len;
	struct ice5_fpga_data *data = dev_get_drvdata(dev);
	struct i2c_client *client = data->client;
	struct {
			unsigned char addr;
			unsigned char data[LEDMATRIX_TEST_CODE_SIZE];
	} i2c_block_transfer;


	int i;
#if 0	
	unsigned char BSR_data[LEDMATRIX_TEST_CODE_SIZE] = {
		0x01, // reg 0x00
		0x00, // reg 0x01
		0x00, // reg 0x02
		0x00, // reg 0x03
		0x00, // reg 0x04
		0x00, // reg 0x05
		0x00, // reg 0x06
		0x00, // reg 0x07
		0x01, 0x00, 
		0x03, 0x00, 
		0x06, 0x3F, 
		0x1C, 0x3F, 
		0x78, 0x21, 
		0x70, 0x21, 
		0x18, 0x21, 
		0x0C, 0x21, 
		0x06, 0x21, 
		0x03, 0x21, 
		0x01, 0x21, 
		0x00, 0x21, 
		0xFF, 0xBF, 
		0xFF, 0xBF, 
		0x18, 0x00, 
		0x00, 0x00, 
		0xFF, 0xFF, 
		0xFF, 0xFF, 
		0xFF, 0xFF, 
		0xFF, 0xFF, 
		0xFF, 0xFF, 
		0xFF, 0xFF, 
		0xFF, 0xFF, 
		0xFF, 0xFF,
		0x00,0x00,
		0x10,0x10,
		0x38,0x38,
		0x7c,0x7c,
		0x3e,0x3e,
		0xff,0xff,
		0x3e,0x3e,
		0x7c,0x7c,
		0x38,0x38,
		0x10,0x10,
		0x00,0x00,
		0x00,0x00,
		0x00,0x00,
		0x08,0x08,
		0x1c,0x1c,
		0x3e,0x3e,
		0x7c,0x7c,
		0xff,0xff,
		0x7c,0x7c,
		0x3e,0x3e,
		0x1c,0x1c,
		0x08,0x08,
		0x00,0x00,
		0x00,0x00,		
	};
#endif
	unsigned char BSR_data[LEDMATRIX_TEST_CODE_SIZE] = {
		0x01, // reg 0x00
		0x00, // reg 0x01
		0x00, // reg 0x02
		0x00, // reg 0x03
		0x00, // reg 0x04
		0x00, // reg 0x05
		0x00, // reg 0x06
		0x00, // reg 0x07
	0x00, 0x01, 	0x00, 0x01, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x01, 	0x00, 0x01, 
	0x00, 0x02, 	0x00, 0x02, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x02, 	0x00, 0x02, 
	0x00, 0x04, 	0x00, 0x04, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x04, 	0x00, 0x04, 
	0x00, 0x08, 	0x00, 0x08, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x08, 	0x00, 0x08, 
	0x00, 0x10, 	0x00, 0x10, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x10, 	0x00, 0x10, 
	0x00, 0x20, 	0x00, 0x20, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x20, 	0x00, 0x20, 
	0x00, 0x40, 	0x00, 0x40, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x40, 	0x00, 0x40, 
	0x00, 0x80, 	0x00, 0x80, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x80, 	0x00, 0x80, 
	0x01, 0x00, 	0x01, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x01, 0x00, 	0x01, 0x00, 
	0x02, 0x00, 	0x02, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x00, 0x00, 	0x02, 0x00, 	0x02, 0x00, 
};
	
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

#if defined(DEBUG)
	u8 *temp;

	temp = kzalloc(sizeof(u8)*(LEDMATRIX_TEST_CODE_SIZE+20), GFP_KERNEL);
	if (NULL == temp)
		pr_err("ice5 : Failed to data allocate %s\n", __func__);
#endif


	pr_info("ice5 : %s :  test code start\n",__func__);
	if (!fw_loaded) {
		pr_info("ice5: %s : firmware is not loaded\n", __func__);
		return 1;
	}

//	ir_led_power_control(true, data);

	/* make data for sending */
	/*  0 : solid normal
	 *   1 : off
	 *   2 : ANI + scroll
	 *   3 : ANI + loop
	 *   4 : normal-> blinking -> scroll
	 */ 
	switch(value)
	{
		case 0 :
			len = LEDMATRIX_TEST_CODE_SIZE;
			i2c_block_transfer.data[0] = 0x01;
			i2c_block_transfer.data[1] = 0x00;
			i2c_block_transfer.data[2] = 0x00;
			i2c_block_transfer.data[3] = 0x00;
			i2c_block_transfer.data[4] = 0x00;
			for (i = 8; i < LEDMATRIX_TEST_CODE_SIZE; i++)
				i2c_block_transfer.data[i] = 0xff;
			break;
		case 1 :
			len = 4;
			i2c_block_transfer.data[0] = 0x00;
			i2c_block_transfer.data[1] = 0x00;
			i2c_block_transfer.data[2] = 0x00;
			i2c_block_transfer.data[3] = 0x00;
			i2c_block_transfer.data[4] = 0x00;
			break;
		case 2 :
			len = LEDMATRIX_TEST_CODE_SIZE;
			i2c_block_transfer.data[0] = 0x15;
			i2c_block_transfer.data[1] = 0x11;
			i2c_block_transfer.data[2] = 0x06;
			i2c_block_transfer.data[3] = 0x0B;
			i2c_block_transfer.data[4] = 0x05;
			for (i = 8; i < LEDMATRIX_TEST_CODE_SIZE; i++)
				i2c_block_transfer.data[i] = BSR_data[i];
			break;
		case 3 :
			len = LEDMATRIX_TEST_CODE_SIZE;
			i2c_block_transfer.data[0] = 0x15;
			i2c_block_transfer.data[1] = 0x11;
			i2c_block_transfer.data[2] = 0x07;
			i2c_block_transfer.data[3] = 0x0A;
			i2c_block_transfer.data[4] = 0x05;
			for (i = 8; i < LEDMATRIX_TEST_CODE_SIZE; i++)
				i2c_block_transfer.data[i] = BSR_data[i];
			break;
		case 4 :
			len = LEDMATRIX_TEST_CODE_SIZE;
			i2c_block_transfer.data[0] = 0x15;
			i2c_block_transfer.data[1] = 0x15;
			i2c_block_transfer.data[2] = 0x31;
			i2c_block_transfer.data[3] = 0x00;
			i2c_block_transfer.data[4] = 0x05;
			for (i = 8; i < LEDMATRIX_TEST_CODE_SIZE; i++)
				i2c_block_transfer.data[i] = BSR_data[i];
			break;
		default :
			len = 4;
			i2c_block_transfer.data[0] = 0x00;
			i2c_block_transfer.data[1] = 0x00;
			i2c_block_transfer.data[2] = 0x00;
			i2c_block_transfer.data[3] = 0x00;
			i2c_block_transfer.data[4] = 0x00;
			break;
	}
	

	/* sending data by I2C */
	i2c_block_transfer.addr = LEDMATRIX_TEST_CODE_ADDR;
	ret = i2c_master_send(client, (unsigned char *) &i2c_block_transfer,
			len+1);
	pr_info("ice5 : %s: ret %d\n", __func__, ret);
	if (ret < 0) {
		pr_err("ice5 : %s: err1 %d\n", __func__, ret);
		ret = i2c_master_send(client,(unsigned char *) &i2c_block_transfer,
			len+1);
		if (ret < 0)
			pr_err("ice5 : %s: err2 %d\n", __func__, ret);
	}

#if defined(DEBUG)
	ice5lp_ledmatrix_read(client, LEDMATRIX_TEST_CODE_ADDR, LEDMATRIX_TEST_CODE_SIZE, temp);

	for (i = 0 ; i < LEDMATRIX_TEST_CODE_SIZE; i++)
		pr_info("[%s] %4d rd %5x, td %5x\n", __func__, i, i2c_block_transfer.data[i], temp[i]);

	ice5lp_ledmatrix_print_gpio_status();
#endif
	mdelay(200);
//	ir_led_power_control(false, data);

	return size;
}

static ssize_t ledmatrix_test_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	return strlen(buf);
}

static DEVICE_ATTR(ledmatrix_test, 0660, ledmatrix_test_show, ledmatrix_test_store);

/* sysfs ledmatrix_version */
static ssize_t ledmatrix_ver_check_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct ice5_fpga_data *data = dev_get_drvdata(dev);

	u8 fw_ver;
	ice5lp_ledmatrix_read(data->client, LEDMATRIX_VER_ADDR, 1, &fw_ver);

	pr_info("ice5 : %s: fw 0x%x\n", __func__, fw_ver);

	return sprintf(buf, "%x\n", fw_ver);
}

static DEVICE_ATTR(ledmatrix_ver_check, 0660, ledmatrix_ver_check_show, NULL);

static ssize_t ledmatrix_pwr_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	if (send_success)
		return sprintf(buf, "1\n");
	else
		return sprintf(buf, "0\n");
}

static ssize_t ledmatrix_pwr_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	int rc;

	rc = kstrtoul(buf, (unsigned int)0, (unsigned long *)&value);
	if (rc < 0)
		return rc;

	if (!fw_loaded) {
		pr_info("ice5: %s : firmware is not loaded\n", __func__);
		return -EIO;
	}

	if(value==0)
	{
		gpio_set_value(g_pdata->fpga_reset, GPIO_LEVEL_LOW);
		pr_info("ice5: %s : FPGA_RST LOW\n", __func__);
	}
	else if(value==1)
	{
		gpio_set_value(g_pdata->fpga_reset, GPIO_LEVEL_HIGH);
		pr_info("ice5: %s : FPGA_RST HIGH\n", __func__);
	}
	
	return size;
}
static DEVICE_ATTR(ledmatrix_pwr, 0660, ledmatrix_pwr_show, ledmatrix_pwr_store);

static struct attribute *sec_ledmatrix_attributes[] = {
	&dev_attr_ledmatrix_send.attr,
	&dev_attr_ledmatrix_send_result.attr,
	&dev_attr_ledmatrix_test.attr,
	&dev_attr_ledmatrix_ver_check.attr,
	&dev_attr_ledmatrix_pwr.attr,
	NULL,
};

static struct attribute_group sec_ledmatrix_attr_group = {
	.attrs = sec_ledmatrix_attributes,
};
/* end of sysfs code */

#if defined(CONFIG_OF)
static int of_ice5lp_ledmatrix_dt(struct device *dev, struct ice5lp_ledmatrix_platform_data *pdata,
			struct ice5_fpga_data *data)
{
	struct device_node *np_ice5lp_ledmatrix = dev->of_node;

	if(!np_ice5lp_ledmatrix)
		return -EINVAL;

	pdata->creset_0 = of_get_named_gpio(np_ice5lp_ledmatrix, "ice5lp_ledmatrix,creset_0", 0);
	pdata->creset_1 = of_get_named_gpio(np_ice5lp_ledmatrix, "ice5lp_ledmatrix,creset_1", 0);
	pdata->fpga_reset = of_get_named_gpio(np_ice5lp_ledmatrix, "ice5lp_ledmatrix,fpga_reset", 0);
	pdata->spi_si_sda = of_get_named_gpio(np_ice5lp_ledmatrix, "ice5lp_ledmatrix,spi_si_sda", 0);
	pdata->spi_clk_scl = of_get_named_gpio(np_ice5lp_ledmatrix, "ice5lp_ledmatrix,spi_clk_scl", 0);

	pdata->ledmatrix_en = of_get_named_gpio(np_ice5lp_ledmatrix, "ice5lp_ledmatrix,ledmatrix_en", 0);	

	return 0;
}
#else
static int of_ice5lp_ledmatrix_dt(struct device *dev, struct ice5lp_ledmatrix_platform_data *pdata)
{
	return -ENODEV;
}
#endif /* CONFIG_OF */

static int __devinit ice5lp_ledmatrix_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct ice5_fpga_data *data;
	struct ice5lp_ledmatrix_platform_data *pdata;
	struct device *ice5lp_ledmatrix_dev;
	int error = 0;

	pr_info("%s probe!\n", __func__);

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	data = kzalloc(sizeof(struct ice5_fpga_data), GFP_KERNEL);
	if (NULL == data) {
		pr_err("Failed to data allocate %s\n", __func__);
		error = -ENOMEM;
		goto alloc_fail;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct ice5lp_ledmatrix_platform_data),
				GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			error =  -ENOMEM;
			goto platform_data_fail;
		}
		error = of_ice5lp_ledmatrix_dt(&client->dev, pdata, data);
		if (error)
			goto platform_data_fail;
	}
	else
		pdata = client->dev.platform_data;

	g_pdata = pdata;

	pr_info("%s : creset_0 : %d\n", __func__, g_pdata->creset_0);
	pr_info("%s : creset_1 : %d\n", __func__, g_pdata->creset_1);	
	pr_info("%s : fpga_reset : %d\n", __func__, g_pdata->fpga_reset);
	pr_info("%s : spi_si_sda : %d\n", __func__, g_pdata->spi_si_sda);
	pr_info("%s : spi_clk_scl : %d\n", __func__, g_pdata->spi_clk_scl);
	pr_info("%s : ledmatrix_en : %d\n", __func__, g_pdata->ledmatrix_en);

	if(gpio_request_one(pdata->ledmatrix_en, GPIOF_OUT_INIT_LOW, "ledmatrix_en")){
		pr_err("%s: cannot request ledmatrix_en", __func__);
	}else {
		pr_info("%s : power on before ledmatrix_en : %d\n", __func__, gpio_get_value(g_pdata->ledmatrix_en));
		gpio_set_value(g_pdata->ledmatrix_en, GPIO_LEVEL_HIGH);
		pr_info("%s : power on ledmatrix_en : %d\n", __func__, gpio_get_value(g_pdata->ledmatrix_en));
	}
	
#if 0	
	error = ice5lp_ledmatrix_gpio_setting();
	if (error)
		goto platform_data_fail;

	if (ice5_check_cdone()) {
		fw_loaded = 1;
		pr_info("FPGA FW is loaded!\n");
	} else {
		fw_loaded = 0;
		pr_info("FPGA FW is NOT loaded!\n");
	}
#endif
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

	ice5lp_ledmatrix_dev = sec_device_create(data, "sec_ledmatrix");
	if (IS_ERR(ice5lp_ledmatrix_dev))
		pr_err("Failed to create ice5lp_ledmatrix_dev in sec_ir\n");

	if (sysfs_create_group(&ice5lp_ledmatrix_dev->kobj, &sec_ledmatrix_attr_group) < 0) {
		sec_device_destroy(ice5lp_ledmatrix_dev->devt);
		pr_err("Failed to create sysfs group for samsung ir!\n");
	}

	/* Create dedicated thread so that the delay of our work does not affect others */
	data->firmware_dl =
		create_singlethread_workqueue("ice5lp_ledmatrix_firmware_dl");
	INIT_DELAYED_WORK(&data->fw_dl, ice5lp_fw_update);

	/* min 1ms is needed */
	queue_delayed_work(data->firmware_dl,
			&data->fw_dl, msecs_to_jiffies(20));

	pr_info("%s : %s %s:\n", __func__, 
		dev_driver_string(&client->dev), dev_name(&client->dev));

	return 0;

platform_data_fail:
	kfree(data);
alloc_fail:
	return error;
}

static int __devexit ice5lp_ledmatrix_remove(struct i2c_client *client)
{
	struct ice5_fpga_data *data = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);
	kfree(data);
	return 0;
}

#ifdef CONFIG_PM
static int ice5lp_ledmatrix_suspend(struct device *dev)
{
	//gpio_set_value(g_pdata->spi_en_rstn, GPIO_LEVEL_LOW);
	return 0;
}

static int ice5lp_ledmatrix_resume(struct device *dev)
{
	//gpio_set_value(g_pdata->spi_en_rstn, GPIO_LEVEL_HIGH);
	return 0;
}

static const struct dev_pm_ops ice5lp_ledmatrix_pm_ops = {
	.suspend	= ice5lp_ledmatrix_suspend,
	.resume		= ice5lp_ledmatrix_resume,
};
#endif

static const struct i2c_device_id ice5_id[] = {
	{"ice5", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, ice5_id);


#if defined(CONFIG_OF)
static struct of_device_id ice5lp_ledmatrix_match_table[] = {
	{ .compatible = "lattice,ice5lp_ledmatrix",},
	{},
};
#else
#define ice5lp_ledmatrix_match_table	NULL
#endif

static struct i2c_driver ice5_i2c_driver = {
	.driver = {
		.name	= "ice5",
		.of_match_table = ice5lp_ledmatrix_match_table,
#ifdef CONFIG_PM
		.pm	= &ice5lp_ledmatrix_pm_ops,
#endif
	},
	.probe = ice5lp_ledmatrix_probe,
	.remove = __devexit_p(ice5lp_ledmatrix_remove),
	.id_table = ice5_id,
};

static int __init ice5lp_ledmatrix_init(void)
{
	return i2c_add_driver(&ice5_i2c_driver);
}
late_initcall(ice5lp_ledmatrix_init);

static void __exit ice5lp_ledmatrix_exit(void)
{
	i2c_del_driver(&ice5_i2c_driver);
}
module_exit(ice5lp_ledmatrix_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ICE5LP LEDMATRIX FPGA FOR IRDA");
