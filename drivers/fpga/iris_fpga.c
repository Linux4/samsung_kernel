/*
 * driver/ice40xx_iris IR Led driver
 *
 * Copyright (C) 2012 Samsung Electronics
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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <linux/device.h>
#include <linux/spinlock.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/clk.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/err.h>

#include <linux/iris_fpga.h>

#include <linux/pinctrl/consumer.h>

struct ice40_iris_data {
	struct i2c_client		*client;
	struct workqueue_struct		*firmware_dl;
	struct delayed_work		fw_dl;
	const struct firmware		*fw;
	struct mutex			mutex;
	struct pinctrl			*fw_pinctrl;
	struct ice40_iris_platform_data *pdata;
};

#ifdef CONFIG_CAMERA_IRIS
static struct ice40_iris_platform_data *g_pdata;
static struct ice40_iris_data *g_data;
extern struct class *camera_class;
struct device *tz_led_dev;
#endif
static int ice40_clock_en(struct ice40_iris_platform_data *pdata, int onoff)
{
	pr_info("%s - on : %d\n", __func__, onoff);

	if (onoff) {
		if (clk_prepare_enable(pdata->fpga_clk))
			pr_info("[ICE40_FPGA] %s: Couldn't prepare Clock\n", __func__);
	} else {
		clk_disable_unprepare(pdata->fpga_clk);
	}

	return 0;
}

static void fpga_enable(struct ice40_iris_platform_data *pdata, int enable_rst_n)
{
	gpio_set_value(pdata->rst_n,
			enable_rst_n ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);

	usleep_range(1000, 2000);
}


extern void fpga_rstn_control(void)
{
	pr_info("%s: high->low->high\n", __func__);

	gpio_set_value(g_pdata->rst_n, GPIO_LEVEL_HIGH);
	gpio_set_value(g_pdata->rst_n, GPIO_LEVEL_LOW);
	mdelay(1);
	gpio_set_value(g_pdata->rst_n, GPIO_LEVEL_HIGH);
}

extern void fpga_rstn_high(void)
{
	gpio_set_value(g_pdata->rst_n, GPIO_LEVEL_HIGH);
}

extern bool is_fw_dl_completed(void)
{
	int cdone = 0;
	cdone = gpio_get_value(g_pdata->cdone);

	return cdone ? true : false;
}

#ifdef CONFIG_OF
static int ice40_iris_parse_dt(struct device *dev,
			struct ice40_iris_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret;

	ret = of_property_read_u32(np, "ice40,fw_ver", &pdata->fw_ver);
	if (ret < 0) {
		pr_err("[%s]: failed to read fw_ver\n", __func__);
		return ret;
	}
	pdata->spi_si = of_get_named_gpio(np, "ice40,sda-gpio", 0);
	pdata->spi_clk = of_get_named_gpio(np, "ice40,scl-gpio", 0);
	pdata->cresetb = of_get_named_gpio(np, "ice40,cresetb", 0);
	pdata->cdone = of_get_named_gpio(np, "ice40,cdone", 0);
	pdata->rst_n = of_get_named_gpio(np, "ice40,reset_n", 0);
	pdata->pogo_ldo_en = of_get_named_gpio(np, "ice40,pogo_ldo_en", 0);
	pdata->spi_so = of_get_named_gpio(np, "ice40,spi-miso", 0);
/* TZ GPIO pin isn't enabled for FPGA driver, 
   TZ GPIO pin is enabled as part of sensor power sequence for TAB S4
   So, disabled below code
*/
   
#if 0
#ifdef CONFIG_CAMERA_IRIS	
	pdata->led_tz = of_get_named_gpio(np, "ice40,led_tz", 0);
#endif
#endif

	pdata->fpga_clk = clk_get(dev, "fpga_clock");
	if (IS_ERR(pdata->fpga_clk)) {
		ret = PTR_ERR(pdata->fpga_clk);
		if (ret != -EPROBE_DEFER)
			pr_err("[%s] Couldn't get Clock [%d]\n", __func__, ret);
		pdata->fpga_clk = NULL;
		return ret;
	}

	return 0;
}
#else
static int ice40_iris_parse_dt(struct device *dev,
			struct ice40_iris_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int iris_pinctrl_configure(struct ice40_iris_data *data, int active)
{
	struct pinctrl_state *set_state;
        struct pinctrl *pinctrl;
	int retval;

	pinctrl = devm_pinctrl_get_select(&data->client->dev, "ice40_pogo_ldo_en");
        if (IS_ERR(pinctrl)) {
                pr_err("[FPGA]: pogo_ldo_en pin select error \n");
        }
	pr_info("[FPGA],%s : pogo_ldo_en %d\n", __func__,  gpio_get_value (data->pdata->pogo_ldo_en));

	pinctrl = devm_pinctrl_get_select(&data->client->dev, "ice40_disable_miso");
        if (IS_ERR(pinctrl)) {
                pr_err("[FPGA]: disable_miso pin select error \n");
        }
	pr_info("[FPGA],%s : disable_miso %d\n", __func__,  gpio_get_value (data->pdata->spi_so));

	if (data->fw_pinctrl != NULL) {
		if (active) {
			set_state = pinctrl_lookup_state(data->fw_pinctrl,
						"ice40_iris_fw_ready");
			if (IS_ERR(set_state)) {
				pr_info("cannot get ts pinctrl active state\n");
				return PTR_ERR(set_state);
			}
		} else {
			pr_info("not support inactive state\n");
			return 0;
		}
		retval = pinctrl_select_state(data->fw_pinctrl, set_state);
		if (retval) {
			pr_info("cannot get ts pinctrl active state\n");
			return retval;
		}
	} else {
		pr_info("[FPGA],%s error while configuring pinctrl, pinctrl is NULL\n", __func__);
		return -1;	
	}

	return 0;
}

static int ice40_iris_config(struct ice40_iris_data *data)
{
	struct ice40_iris_platform_data *pdata = data->pdata;
	int rc;

	pr_info("ice40_fw_ver[%d] spi_si[%d] spi_clk[%d] cresetb[%d] cdone[%d] rst_n[%d]\n",\
			pdata->fw_ver, pdata->spi_si, pdata->spi_clk,\
			pdata->cresetb, pdata->cdone, pdata->rst_n);

	iris_pinctrl_configure(data, 1);
	gpio_direction_output(pdata->spi_si, GPIO_LEVEL_LOW);
	gpio_direction_output(pdata->spi_clk, GPIO_LEVEL_LOW);

	rc = gpio_request(pdata->cresetb, "iris_creset");
	if (rc < 0) {
		pr_err("%s: cresetb error : %d\n", __func__, rc);
		return rc;
	}
	gpio_direction_output(pdata->cresetb, GPIO_LEVEL_HIGH);

	rc = gpio_request(pdata->rst_n, "iris_rst_n");
	if (rc < 0) {
		pr_err("%s: rst_n error : %d\n", __func__, rc);
		return rc;
	}
	gpio_direction_output(pdata->rst_n, GPIO_LEVEL_HIGH);

	rc = gpio_request(pdata->cdone, "iris_cdone");
	if (rc < 0) {
		pr_err("%s: cdone error : %d\n", __func__, rc);
		return rc;
	}
	gpio_direction_input(pdata->cdone);

	return 0;
}

/*
 * Send ice40 fpga firmware data thougth spi communication
 */
static int ice40_fpga_send_firmware_data(
		struct ice40_iris_platform_data *pdata, const u8 *fw_data, int len)
{
	unsigned int i, j;
	int cdone = 0;
	unsigned char spibit;

	cdone = gpio_get_value(pdata->cdone);
	pr_info("%s check f/w loading state[%d]\n", __func__, cdone);

	for (i = 0; i < len; i++){
		spibit = fw_data[i];
		for (j = 0; j < 8; j++){
			gpio_set_value_cansleep(pdata->spi_clk,
						GPIO_LEVEL_LOW);

			gpio_set_value_cansleep(pdata->spi_si,
					spibit & 0x80 ? GPIO_LEVEL_HIGH : GPIO_LEVEL_LOW);

			gpio_set_value_cansleep(pdata->spi_clk,
						GPIO_LEVEL_HIGH);
			spibit = spibit<<1;
		}
	}

	gpio_set_value_cansleep(pdata->spi_si, GPIO_LEVEL_HIGH);
	for (i = 0; i < 200; i++){
		gpio_set_value_cansleep(pdata->spi_clk, GPIO_LEVEL_LOW);
		gpio_set_value_cansleep(pdata->spi_clk, GPIO_LEVEL_HIGH);
	}
	usleep_range(1000, 1300);

	cdone = gpio_get_value(pdata->cdone);
	pr_info("%s check f/w loading state[%d]\n", __func__, cdone);

	return cdone;
}

static int ice40_fpga_fimrware_update_start(
		struct ice40_iris_data *data, const u8 *fw_data, int len)
{
	struct ice40_iris_platform_data *pdata = data->pdata;
	int retry = 0, ret_fw = 0;

	pr_info("%s\n", __func__);
	iris_pinctrl_configure(data, 1);

	fpga_enable(pdata, FPGA_DISABLE);
	gpio_direction_output(pdata->spi_si, GPIO_LEVEL_LOW);
	gpio_direction_output(pdata->spi_clk, GPIO_LEVEL_LOW);

	do {
		gpio_set_value_cansleep(pdata->cresetb, GPIO_LEVEL_LOW);
		usleep_range(30, 50);

		gpio_set_value_cansleep(pdata->cresetb, GPIO_LEVEL_HIGH);
		usleep_range(1000, 1300);

		ret_fw = ice40_fpga_send_firmware_data(pdata, fw_data, len);
		usleep_range(50, 70);
		if (ret_fw) {
			pr_info("FPGA firmware update success \n");
			break;
		}
	} while (retry++ < FIRMWARE_MAX_RETRY);

	return 0;
}

void ice40_fpga_firmware_update(struct ice40_iris_data *data)
{
	struct ice40_iris_platform_data *pdata = data->pdata;
	struct i2c_client *client = data->client;

	switch (pdata->fw_ver) {
	case 1:
		pr_info("%s[%d] fw_ver %d\n", __func__,
				__LINE__, pdata->fw_ver);
		if (request_firmware(&data->fw,
				"fpga/iris_fpga.fw", &client->dev)) {
			pr_err("%s: Can't open firmware file\n", __func__);
		} else {
			ice40_fpga_fimrware_update_start(data, data->fw->data,
					data->fw->size);
			release_firmware(data->fw);

			gpio_set_value(pdata->rst_n, GPIO_LEVEL_HIGH);
			pr_info("[FPGA],%s : rst %d\n", __func__,  gpio_get_value (pdata->rst_n));
			usleep_range(30, 50);

			gpio_set_value(pdata->rst_n, GPIO_LEVEL_LOW);
			pr_info("[FPGA],%s : rst %d\n", __func__,  gpio_get_value (pdata->rst_n));
			usleep_range(30, 50);

			gpio_set_value(pdata->rst_n, GPIO_LEVEL_HIGH);
			pr_info("[FPGA],%s : rst %d\n", __func__,  gpio_get_value (pdata->rst_n));
		}
		break;
	default:
		pr_err("[%s] Not supported [fw_ver = %d]\n",
				__func__, pdata->fw_ver);
		break;
	}
	usleep_range(10000, 12000);
}

static ssize_t ice40_fpga_fw_update_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct ice40_iris_data *data = dev_get_drvdata(dev);
	struct file *fp = NULL;
	long fsize = 0, nread = 0;
	const u8 *buff = 0;
	char fw_path[SEC_FPGA_MAX_FW_PATH];
	int locate, ret;
	mm_segment_t old_fs = get_fs();

	pr_info("%s\n", __func__);

	ret = sscanf(buf, "%1d", &locate);
	if (!ret) {
		pr_err("[%s] force select extSdCard\n", __func__);
		locate = 0;
	}

	old_fs = get_fs();
	set_fs(get_ds());

	if (locate) {
		snprintf(fw_path, SEC_FPGA_MAX_FW_PATH,
				"/storage/sdcard0/%s", SEC_FPGA_FW_FILENAME);
	} else {
		snprintf(fw_path, SEC_FPGA_MAX_FW_PATH,
				"/storage/extSdCard/%s", SEC_FPGA_FW_FILENAME);
	}

	fp = filp_open(fw_path, O_RDONLY, 0);
	if (IS_ERR(fp)) {
//		pr_err("file %s open error:%d\n", fw_path, (s32)fp);
		goto err_open;
	}

	fsize = fp->f_path.dentry->d_inode->i_size;
	pr_info("fpga firmware size: %ld\n", fsize);

	buff = kzalloc((size_t)fsize, GFP_KERNEL);
	if (!buff) {
		pr_err("fail to alloc buffer for fw\n");
		goto err_alloc;
	}

	nread = vfs_read(fp, (char __user *)buff, fsize, &fp->f_pos);
	if (nread != fsize) {
		pr_err("fail to read file %s (nread = %ld)\n",
				fw_path, nread);
		goto err_fw_size;
	}

	ice40_fpga_fimrware_update_start(data, (unsigned char *)buff, fsize);

err_fw_size:
	kfree(buff);
err_alloc:
	filp_close(fp, NULL);
err_open:
	set_fs(old_fs);

	return size;
}

static ssize_t ice40_fpga_fw_update_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return strlen(buf);
}

static void fw_work(struct work_struct *work)
{
	struct ice40_iris_data *data =
		container_of(work, struct ice40_iris_data, fw_dl.work);

	ice40_fpga_firmware_update(data);
}

static ssize_t fpga_i2c_write_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	struct ice40_iris_data *data = dev_get_drvdata(dev);
	int _addr, _data;
	int ret;

	mutex_lock(&data->mutex);

	ice40_clock_en(data->pdata, 1);
	fpga_enable(data->pdata, FPGA_ENABLE);

	sscanf(buf, "%1d %3d", &_addr, &_data);
	ret = i2c_smbus_write_byte_data(data->client, _addr, _data);
	if (ret < 0) {
		pr_info("%s iris i2c write failed %d\n", __func__, ret);
		fpga_enable(data->pdata, FPGA_DISABLE);
		ice40_clock_en(data->pdata, 0);

		mutex_unlock(&data->mutex);
		return ret;
	}

	ret = i2c_smbus_read_byte_data(data->client, _addr);
	pr_info("%s iris_write data %d\n", __func__, ret);

	fpga_enable(data->pdata, FPGA_DISABLE);
	ice40_clock_en(data->pdata, 0);

	mutex_unlock(&data->mutex);

	return count;
}

static ssize_t fpga_i2c_read_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	struct ice40_iris_data *data = dev_get_drvdata(dev);
	char *bufp = buf;
	u8 read_val;

	mutex_lock(&data->mutex);

	ice40_clock_en(data->pdata, 1);
	fpga_enable(data->pdata, FPGA_ENABLE);

	read_val = i2c_smbus_read_byte_data(data->client, 0x00);
	bufp += snprintf(bufp, SNPRINT_BUF_SIZE, "0x00 read val %d\n", read_val);
	read_val = i2c_smbus_read_byte_data(data->client, 0x01);
	bufp += snprintf(bufp, SNPRINT_BUF_SIZE, "0x01 read val %d\n", read_val);
	read_val = i2c_smbus_read_byte_data(data->client, 0x02);
	bufp += snprintf(bufp, SNPRINT_BUF_SIZE, "0x02 read val %d\n", read_val);
	read_val = i2c_smbus_read_byte_data(data->client, 0x03);
	bufp += snprintf(bufp, SNPRINT_BUF_SIZE, "0x03 read val %d\n", read_val);

	fpga_enable(data->pdata, FPGA_DISABLE);
	ice40_clock_en(data->pdata, 0);

	mutex_unlock(&data->mutex);

	return strlen(buf);
}

static struct device_attribute ice40_attrs[] = {
	__ATTR(ice40_fpga_fw_update, S_IRUGO|S_IWUSR|S_IWGRP,
			ice40_fpga_fw_update_show, ice40_fpga_fw_update_store),
	__ATTR(fpga_i2c_check, S_IRUGO|S_IWUSR|S_IWGRP,
			fpga_i2c_read_show, fpga_i2c_write_store),
};

#ifdef CONFIG_CAMERA_IRIS
unsigned int fpga_reg[4]={0x00,0x00,0x00,0xF0};
extern void sm5703_fled_iris_on(void);
extern void sm5703_fled_iris_off(void);

void ir_led_on(int i)
{
    struct i2c_client *client = g_data->client;
    int ret;
    unsigned char _data[2];
	
    mutex_lock(&g_data->mutex);
    udelay(450);
    /* MIC2873 : E */

    ice40_clock_en(g_pdata,1);
    fpga_enable(g_pdata,1);


    pr_err("before i2c addr 0x%x\n", client->addr);
    client->addr = 0x6c;
    pr_err("after i2c addr 0x%x\n", client->addr);
    /* register set td */
    _data[0] = 0x00;
    _data[1] = fpga_reg[0];
	ret = i2c_smbus_write_byte_data(g_data->client, _data[0], _data[1]);
    if(ret < 0)
        pr_err("ir_led i2c error %d\n", __LINE__);
    _data[0] = 0x01;
    _data[1] = fpga_reg[1];
    ret = i2c_smbus_write_byte_data(g_data->client, _data[0], _data[1]);
    if(ret < 0)
        pr_err("ir_led i2c error %d\n", __LINE__);

    /* register set tp */
    _data[0] = 0x02;
    _data[1] = fpga_reg[2];
    ret = i2c_smbus_write_byte_data(g_data->client, _data[0], _data[1]);
    if(ret < 0)
        pr_err("ir_led i2c error %d\n", __LINE__);
	
    _data[0] = 0x03;
    _data[1] = fpga_reg[3];
    ret = i2c_smbus_write_byte_data(g_data->client, _data[0], _data[1]);
    if(ret < 0)
        pr_err("ir_led i2c error %d\n", __LINE__);

	ret = i2c_smbus_read_byte_data(g_data->client, 0x03);
	pr_err("%s iris_write data %d\n", __func__, ret);

    pr_err("ir_led_on : FPGA register value 0x00 : %x \n",fpga_reg[0]);
    pr_err("ir_led_on : FPGA register value 0x01 : %x \n",fpga_reg[1]);
    pr_err("ir_led_on : FPGA register value 0x02 : %x \n",fpga_reg[2]);
    pr_err("ir_led_on : FPGA register value 0x03 : %x \n",fpga_reg[3]);

    mutex_unlock(&g_data->mutex);
}

void ir_led_off(void)
{
    mutex_lock(&g_data->mutex);

#if 0
    /* MIC2873 */
    gpio_set_value_cansleep(g_pdata->led_tz, 0);
    udelay(450);
#endif

    fpga_enable(g_pdata,0);
    ice40_clock_en(g_pdata,0);

    mutex_unlock(&g_data->mutex);

    pr_err("ir_led_off");
}
/* Below code isn't required for TAB S4*/
#if 0
static ssize_t tz_led_gpio_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
	char temp_print[] = "tz led gpio\n";

    return snprintf(buf, sizeof(temp_print), "%s", temp_print);
}

static ssize_t set_tz_led_gpio(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
    char *after;
    unsigned long status = simple_strtoul(buf, &after, 10);

    if (status == 1) {
		pr_info("[%s] : tz led on", __func__);

	    gpio_set_value_cansleep(g_pdata->led_tz, 1);
		sm5703_fled_iris_on();
		//ir_led_on(1);	

    } else if (status == 0) {
		pr_info("[%s] : tz led off", __func__);	
		pr_info("set tz led gpio to low\n");
	    gpio_set_value_cansleep(g_pdata->led_tz, 0);
		sm5703_fled_iris_off();
		//ir_led_off();
    }
    return size;
}
static DEVICE_ATTR(tz_led_gpio, S_IWUSR|S_IWGRP|S_IROTH,
	tz_led_gpio_show, set_tz_led_gpio);
#endif
static ssize_t IR_LED_td_high_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
	char temp_print[] = "ir led high gpio\n";

    return snprintf(buf, sizeof(temp_print), "%s", temp_print);
}

static ssize_t IR_LED_td_high_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct i2c_client *client = g_data->client;

    int ret = 0;
    unsigned char _data[2];
	unsigned long temp = simple_strtoul(buf, NULL, 16);
    fpga_reg[0] = temp;
	pr_err("IR_LED_td_high_store : ENTER");

	mutex_lock(&g_data->mutex);

	//ice40_clock_en(1);
	//fpga_enable(1);

    client->addr = 0x6c;
    /* register set td high*/
    _data[0] = 0x00;
    _data[1] = temp;

    ret = i2c_smbus_write_byte_data(g_data->client, _data[0], _data[1]);

    if(ret < 0)
        pr_err("ir_led_td_high_store : i2c error hwh ");
	
	mutex_unlock(&g_data->mutex);

	return size;
}

/*static DEVICE_ATTR(ir_led_td_high, S_IWUSR|S_IWGRP|S_IROTH,
	ir_led_td_high_show, ir_led_td_high_store);*/

static ssize_t IR_LED_td_low_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
	char temp_print[] = "ir led  low show \n";

    return snprintf(buf, sizeof(temp_print), "%s", temp_print);
}

static ssize_t IR_LED_td_low_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
	struct i2c_client *client = g_data->client;

    int ret = 0;
    unsigned char _data[2];
    unsigned long temp = simple_strtoul(buf, NULL, 16);
    fpga_reg[1] = temp;
	pr_err("ir_led_td_low_store : ENTER");

	mutex_lock(&g_data->mutex);

	//ice40_clock_en(1);
	//fpga_enable(1);

    client->addr = 0x6c;
    /* register set td high*/
    _data[0] = 0x01;
    _data[1] = temp;


    ret = i2c_smbus_write_byte_data(g_data->client, _data[0], _data[1]);
	

    if(ret < 0)
        pr_err("ir_led_td_low_store : i2c error hwh ");

	mutex_unlock(&g_data->mutex);

	return size;
}

/*static DEVICE_ATTR(ir_led_td_low, S_IWUSR|S_IWGRP|S_IROTH,
	ir_led_td_low_show, ir_led_td_low_store);*/
static ssize_t IR_LED_tp_low_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
	char temp_print[] = "ir led  low show \n";

    return snprintf(buf, sizeof(temp_print), "%s", temp_print);
}

static ssize_t IR_LED_tp_high_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
	char temp_print[] = "ir led high gpio\n";

    return snprintf(buf, sizeof(temp_print), "%s", temp_print);
}

static ssize_t IR_LED_tp_high_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
    struct i2c_client *client = g_data->client;
    int ret = 0;
    unsigned char _data[2];
    unsigned long temp = simple_strtoul(buf, NULL, 16);
    fpga_reg[2] = temp;
	pr_err("IR_LED_tp_high_store : ENTER");

	mutex_lock(&g_data->mutex);

	
	//ice40_clock_en(1);
	//fpga_enable(1);

    client->addr = 0x6c;
    /* register set td high*/
    _data[0] = 0x02;
    _data[1] = temp;

	 ret = i2c_smbus_write_byte_data(g_data->client, _data[0], _data[1]);
    
    if(ret < 0)
        pr_err("ir_led_tp_high_store: i2c error hwh ");

	mutex_unlock(&g_data->mutex);

	return size;
}

static ssize_t IR_LED_tp_low_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t size)
{
    struct i2c_client *client = g_data->client;
    int ret = 0;
    unsigned char _data[2];
    unsigned long temp = simple_strtoul(buf, NULL, 16);
    fpga_reg[3] = temp;
	pr_err("IR_LED_tp_low_store : ENTER");

	mutex_lock(&g_data->mutex);

	//ice40_clock_en(1);
	///fpga_enable(1);


    client->addr = 0x6c;
    /* register set td high*/
    _data[0] = 0x03;
    _data[1] = temp;

    ret = i2c_smbus_write_byte_data(g_data->client, _data[0], _data[1]);

    if(ret < 0)
        pr_err("ir_led_tp_low_store: i2c error hwh ");


	mutex_unlock(&g_data->mutex);


	return size;
}

static struct device_attribute ice40_attrs_led[] = {
	__ATTR(IR_LED_td_high, S_IRUGO|S_IWUSR|S_IWGRP,
	IR_LED_td_high_show, IR_LED_td_high_store),
	__ATTR(IR_LED_td_low, S_IRUGO|S_IWUSR|S_IWGRP,
	IR_LED_td_low_show, IR_LED_td_low_store),
	__ATTR(IR_LED_tp_high, S_IRUGO|S_IWUSR|S_IWGRP,
	IR_LED_tp_high_show, IR_LED_tp_high_store),
	__ATTR(IR_LED_tp_low, S_IRUGO|S_IWUSR|S_IWGRP,
	IR_LED_tp_low_show, IR_LED_tp_low_store),
};
#endif

static int ice40_iris_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct ice40_iris_data *data;
	struct ice40_iris_platform_data *pdata;
	struct device *ice40_iris_dev;
#ifdef CONFIG_CAMERA_IRIS
	struct device *sec_ir_dev;
#endif
	int i, error;
	dev_t devt = (client->adapter->nr << 8) | client->addr;

	pr_info("%s probe!\n", __func__);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		error = -EIO;
		goto err_return;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		pr_err("%s: i2c functionality check error\n", __func__);
		dev_err(&client->dev, "need I2C_FUNC_SMBUS_BYTE_DATA.\n");
		error = -EIO;
		goto err_return;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				sizeof(*pdata), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			error = -ENOMEM;
			goto err_return;
		}
		error = ice40_iris_parse_dt(&client->dev, pdata);
		if (error) {
			goto err_data_mem;
		}
	} else {
		pdata = client->dev.platform_data;
	}
#ifdef CONFIG_CAMERA_IRIS
	g_pdata = pdata;
#endif
	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (NULL == data) {
		pr_err("Failed to data allocate %s\n", __func__);
		error = -ENOMEM;
		goto err_data_mem;
	}

	i2c_set_clientdata(client, data);
	data->client = client;
	data->pdata = pdata;
	mutex_init(&data->mutex);
#ifdef CONFIG_CAMERA_IRIS
		g_data = data;
#endif
	data->fw_pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(data->fw_pinctrl)) {
		if (PTR_ERR(data->fw_pinctrl) == -EPROBE_DEFER) {
			error = -EPROBE_DEFER;
			goto err_free_mem;
		}

		pr_info("Target does not use pinctrl\n");
		data->fw_pinctrl = NULL;
	}

	error = ice40_iris_config(data);
	if (error < 0) {
		pr_err("ice40_iris_config failed\n");
		goto err_pinctrl_put;
	}

	ice40_iris_dev = device_create(sec_class, NULL, devt, data, "sec_iris");
	if (IS_ERR(ice40_iris_dev)) {
		pr_err("Failed to create ice40_iris_dev device in sec_ir\n");
		error = -ENODEV;
		goto err_pinctrl_put;
	}
	
	/* sysfs entries */
	for (i = 0; i < ARRAY_SIZE(ice40_attrs); i++) {
		error = device_create_file(ice40_iris_dev, &ice40_attrs[i]);
		if (error < 0) {
			pr_err("Failed to create device file(%s)!\n", ice40_attrs[i].attr.name);
			goto err_dev_destroy;
		}
	}

#ifdef CONFIG_CAMERA_IRIS
//Below code isn't required for TAB S4
#if 0
	tz_led_dev = device_create(camera_class, NULL, 0, NULL, "tz_led");
	if (tz_led_dev < 0)
		pr_err("Failed to create device(flash)!\n");
	if (device_create_file(tz_led_dev, &dev_attr_tz_led_gpio) < 0) {
		pr_err("failed to create device file, %s\n",
			   dev_attr_tz_led_gpio.attr.name);
	}
#endif
	sec_ir_dev = device_create(sec_class, NULL, 0, NULL, "sec_ir");
		if (sec_ir_dev < 0)
			pr_err("Failed to create device(flash)!\n");

	for (i = 0; i < ARRAY_SIZE(ice40_attrs_led); i++) {
		if (device_create_file(sec_ir_dev, &ice40_attrs_led[i]) < 0)
			pr_err("Failed to create device file(%s)!\n",
					ice40_attrs_led[i].attr.name);
	}
#endif

	/* Create dedicated thread so that
	 the delay of our work does not affect others */
	data->firmware_dl =
		create_singlethread_workqueue("ice40_firmware_dl");
	INIT_DELAYED_WORK(&data->fw_dl, fw_work);
	/* min 20ms is needed */
	queue_delayed_work(data->firmware_dl,
			&data->fw_dl, msecs_to_jiffies(20));

	pr_info("%s complete[%d]\n", __func__, __LINE__);

	return 0;

err_dev_destroy:
	device_destroy(sec_class, devt);
err_pinctrl_put:
	if (data->fw_pinctrl != NULL)
		devm_pinctrl_put(data->fw_pinctrl);	
err_free_mem:
	mutex_destroy(&data->mutex);
	kfree(data);
err_data_mem:
	devm_kfree(&client->dev, pdata);
err_return:
	return error;
}

static int ice40_iris_remove(struct i2c_client *client)
{
	struct ice40_iris_data *data = i2c_get_clientdata(client);

	i2c_set_clientdata(client, NULL);

	destroy_workqueue(data->firmware_dl);
	mutex_destroy(&data->mutex);
	kfree(data);
	return 0;
}

#if defined(CONFIG_PM)
static int ice40_fpga_suspend(struct device *dev)
{
	struct i2c_client *i2c = container_of(dev, struct i2c_client, dev);
	struct ice40_iris_data *data = i2c_get_clientdata(i2c);

	pr_info("%s: Set FPGA_RST low\n", __func__);

	gpio_set_value(data->pdata->rst_n, GPIO_LEVEL_LOW);

	return 0;
}

static int ice40_fpga_resume(struct device *dev)
{
	pr_info("%s: FPGA RESUME\n", __func__);

	return 0;
}
#endif

static const struct i2c_device_id ice40_iris_id[] = {
	{"ice40_iris", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, barcode_id);

#ifdef CONFIG_OF
static struct of_device_id ice40_iris_match_table[] = {
	{ .compatible = "ice40_iris",},
	{ },
};
#else
#define ice40_iris_match_table	NULL
#endif

#if defined(CONFIG_PM)
const struct dev_pm_ops ice40_fpga_pm = {
	.suspend = ice40_fpga_suspend,
	.resume = ice40_fpga_resume,
};
#endif

static struct i2c_driver ice40_i2c_driver = {
	.driver = {
		.name = "ice40_iris",
		.owner = THIS_MODULE,
		.of_match_table = ice40_iris_match_table,
#if defined(CONFIG_PM)
		.pm	= &ice40_fpga_pm,
#endif /* CONFIG_PM */
	},
	.probe = ice40_iris_probe,
	.remove = ice40_iris_remove,
	.id_table = ice40_iris_id,
};

static int __init ice40_iris_init(void)
{
	return i2c_add_driver(&ice40_i2c_driver);
}
late_initcall(ice40_iris_init);

static void __exit ice40_iris_exit(void)
{
	i2c_del_driver(&ice40_i2c_driver);
}
module_exit(ice40_iris_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SEC IRIS");
