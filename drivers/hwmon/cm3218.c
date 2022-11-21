/* drivers/hwmon/cm3218.c - cm3218 optical sensors driver
 *
 * Copyright (C) 2011 Capella Microsystems Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/lightsensor.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>
#include <linux/cm3218.h>
#include <asm/setup.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>

#define D(x...) pr_info(x)

#define I2C_RETRY_COUNT 10

#define NEAR_DELAY_TIME ((100 * HZ) / 1000)

#define CONTROL_INT_ISR_REPORT        0x00
#define CONTROL_ALS                   0x01

struct cm3218_info {
	struct class *cm3218_class;
	struct device *ls_dev;

	struct input_dev *ls_input_dev;

	struct mutex control_mutex;
	struct mutex als_enable_mutex;
	struct mutex als_disable_mutex;
	struct mutex als_get_adc_mutex;

	struct early_suspend early_suspend;
	struct i2c_client *i2c_client;
	struct workqueue_struct *lp_wq;
	struct work_struct sensor_irq_work;

	int intr_pin;
	int als_enable;
	int record_init_fail;

	uint16_t *adc_table;
	uint16_t cali_table[10];
	int irq;

	int ls_calibrate;
	int (*power) (int, uint8_t);	/* power to the chip */

	uint32_t als_kadc;
	uint32_t als_gadc;
	uint16_t golden_adc;

	struct wake_lock ps_wake_lock;
	int lightsensor_opened;
	uint8_t als_cmd_address;
	uint8_t check_interrupt_add;

	int current_level;
	uint16_t current_adc;

	unsigned long j_start;
	unsigned long j_end;

	uint16_t is_cmd;
	uint8_t record_clear_int_fail;
};

struct cm3218_info *lp_info;
int32_t als_kadc;

static int i2c_recv_data(struct cm3218_info *lpi, uint8_t cmd, uint8_t *rxData,
		      int length)
{
	uint8_t loop_i;
	int val;
	uint16_t slaveAddr = lpi->als_cmd_address;
	uint8_t subaddr[1] = {cmd};

	struct i2c_msg msg[] = {
		{
		 .addr = slaveAddr,
		 .flags = 0,
		 .len = 1,
		 .buf = subaddr,
		 },
		{
		 .addr = slaveAddr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxData,
		 },
	};

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {

		if (i2c_transfer(lpi->i2c_client->adapter, msg, 2) > 0)
			break;

		val = gpio_get_value(lpi->intr_pin);
		/*check intr GPIO when i2c error */
		if (loop_i == 0 || loop_i == I2C_RETRY_COUNT - 1)
			pr_err("[CM3218 error] %s, i2c err, slaveAddr 0x%x"
			" ISR gpio %d  = %d,lpi->record_init_fail %d\n",
			__func__, slaveAddr, lpi->intr_pin, val,
			lpi->record_init_fail);

		msleep(10);
	}
	if (loop_i >= I2C_RETRY_COUNT) {
		pr_err("[CM3218 error] %s retry over %d\n",
		       __func__, I2C_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int i2c_recv_byte(struct cm3218_info *lpi, uint8_t *rxData, int length)
{
	uint8_t loop_i;
	int val;
	uint16_t slaveAddr = lpi->check_interrupt_add;

	struct i2c_msg msg[] = {
		{
		 .addr = slaveAddr,
		 .flags = I2C_M_RD,
		 .len = length,
		 .buf = rxData,
		 },
	};

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {

		if (i2c_transfer(lpi->i2c_client->adapter, msg, 1) > 0)
			break;

		val = gpio_get_value(lpi->intr_pin);
		/*check intr GPIO when i2c error */
		if (loop_i == 0 || loop_i == I2C_RETRY_COUNT - 1)
			D("[CM3218 error] %s, i2c err, slaveAddr 0x%x"
			" ISR gpio %d  = %d, lpi->record_init_fail %d\n",
			__func__, slaveAddr, lpi->intr_pin, val,
			lpi->record_init_fail);

		msleep(10);
	}
	if (loop_i >= I2C_RETRY_COUNT) {
		pr_err("[CM3218 error] %s retry over %d\n",
		       __func__, I2C_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int i2c_transfer_data(struct cm3218_info *lpi, uint8_t *txData,
								int length)
{
	uint8_t loop_i;
	int val;
	uint16_t slaveAddr = lpi->als_cmd_address;

	struct i2c_msg msg[] = {
		{
		 .addr = slaveAddr,
		 .flags = 0,
		 .len = length,
		 .buf = txData,
		 },
	};

	for (loop_i = 0; loop_i < I2C_RETRY_COUNT; loop_i++) {
		if (i2c_transfer(lpi->i2c_client->adapter, msg, 1) > 0)
			break;

		val = gpio_get_value(lpi->intr_pin);
		/*check intr GPIO when i2c error */
		if (loop_i == 0 || loop_i == I2C_RETRY_COUNT - 1)
			pr_err("[CM3218 error] %s, i2c err, slaveAddr 0x%x,"
			"value 0x%x,ISR gpio%d  = %d, lpi->record_init_fail %d\n",
			__func__, slaveAddr, txData[0],
			lpi->intr_pin, val, lpi->record_init_fail);

		msleep(10);
	}

	if (loop_i >= I2C_RETRY_COUNT) {
		pr_err("[CM3218 error] %s retry over %d\n",
		       __func__, I2C_RETRY_COUNT);
		return -EIO;
	}

	return 0;
}

static int cm3218_i2c_read_byte(struct cm3218_info *lpi, uint8_t *pdata)
{
	uint8_t buffer = 0;
	int ret = 0;

	if (pdata == NULL)
		return -EFAULT;

	ret = i2c_recv_byte(lpi, &buffer, 1);
	if (ret < 0) {
		pr_err("[CM3218 error]%s: i2c_recv_byte fail,"
		" slave addr: 0x%x\n", __func__, lpi->check_interrupt_add);
		return ret;
	}

	*pdata = buffer;
#if 0
	/* Debug use */
	printk(KERN_DEBUG "[CM3218] %s:i2c_recv_byte[0x%x] = 0x%x\n",
	       __func__, slaveAddr, *pdata);
#endif
	return ret;
}

static int cm3218_i2c_read_word(struct cm3218_info *lpi, uint8_t cmd,
				 uint16_t *pdata)
{
	uint8_t buffer[2];
	int ret = 0;

	if (pdata == NULL)
		return -EFAULT;

	ret = i2c_recv_data(lpi, cmd, buffer, 2);
	if (ret < 0) {
		pr_err("[CM3218 error]%s: i2c_recv_data fail"
			"[0x%x, 0x%x]\n", __func__, lpi->als_cmd_address, cmd);
		return ret;
	}

	*pdata = (buffer[1] << 8) | buffer[0];
#if 0
	/* Debug use */
	printk(KERN_DEBUG "[CM3218] %s: i2c_recv_data[0x%x, 0x%x] = 0x%x\n",
	       __func__, slaveAddr, cmd, *pdata);
#endif
	return ret;
}

static int cm3218_i2c_write_word(struct cm3218_info *lpi, uint8_t cmd,
				  uint16_t data)
{
	char buffer[3];
	int ret = 0;
#if 0
	/* Debug use */
	printk(KERN_DEBUG
	       "[CM3218] %s: cm3218_i2c_write_word[0x%x, 0x%x, 0x%x]\n",
	       __func__, SlaveAddress, cmd, data);
#endif
	buffer[0] = cmd;
	buffer[1] = (uint8_t) (data & 0xff);
	buffer[2] = (uint8_t) ((data & 0xff00) >> 8);

	ret = i2c_transfer_data(lpi, buffer, 3);
	if (ret < 0) {
		pr_err("[CM3218 error]%s: i2c_transfer_data fail\n",
		__func__);
		return -EIO;
	}

	return ret;
}

static int get_ls_adc_value(struct cm3218_info *lpi, uint16_t *als_step,
								bool resume)
{
	uint16_t tmpResult;
	int ret = 0;

	if (als_step == NULL)
		return -EFAULT;

	/* Read ALS data: */
	ret = cm3218_i2c_read_word(lpi, ALS_READ, als_step);
	if (ret < 0) {
		pr_err("[CM3218 error]%s: cm3218_i2c_read_word fail\n",
		       __func__);
		return -EIO;
	}

	if (!lpi->ls_calibrate) {
		tmpResult =
		    (uint32_t) (*als_step) * lpi->als_gadc / lpi->als_kadc;
		if (tmpResult > 0xFFFF)
			*als_step = 0xFFFF;
		else
			*als_step = tmpResult;
	}

	D("[CM3218] %s: adc = %d, ls_calibrate = %d\n",
	  __func__, *als_step, lpi->ls_calibrate);

	return ret;
}

static int set_lsensor_range(struct cm3218_info *lpi, uint16_t low_thd,
							uint16_t high_thd)
{
	int ret = 0;

	cm3218_i2c_write_word(lpi, ALS_HW, high_thd);
	cm3218_i2c_write_word(lpi, ALS_LW, low_thd);

	return ret;
}
static void report_ls_value(struct cm3218_info *lpi)
{
	uint16_t adc_value = 0;
	int i, level = 0;
	int ret;

	get_ls_adc_value(lpi, &adc_value, 0);

	lpi->is_cmd |= CM3218_ALS_INT_EN;

	if (lpi->ls_calibrate) {
		for (i = 0; i < 10; i++) {
				if (adc_value <= (*(lpi->cali_table + i))) {
					level = i;
					if (*(lpi->cali_table + i))
						break;
				}
				if (i == 9) {	/*avoid  i = 10, because
						'cali_table' of size is 10 */
					level = i;
					break;
				}
			}
		} else {
			for (i = 0; i < 10; i++) {
				if (adc_value <= (*(lpi->adc_table + i))) {
					level = i;
					if (*(lpi->adc_table + i))
						break;
				}
				if (i == 9) {	/*avoid  i = 10, because
						'cali_table' of size is 10 */
					level = i;
					break;
				}
			}
		}

	ret = set_lsensor_range(lpi, ((i == 0) || (adc_value == 0)) ? 0 :
				*(lpi->cali_table + (i - 1)) + 1,
				*(lpi->cali_table + i));

	if ((i == 0) || (adc_value == 0))
		D("[CM3218] %s: adc=%d, Level=%d, l_thd equal 0,"
		"h_thd = %d\n", __func__, adc_value, level,
		*(lpi->cali_table + i));
	else
		D("[CM3218] %s: adc=%d, Level=%d, l_thd = %d,"
		"h_thd = %d\n", __func__, adc_value, level,
	*(lpi->cali_table + (i - 1)) + 1,
	*(lpi->cali_table + i));

	lpi->current_level = level;
	lpi->current_adc = adc_value;
	input_report_abs(lpi->ls_input_dev, ABS_MISC, level);
	input_sync(lpi->ls_input_dev);
}

static int control_and_report(struct cm3218_info *lpi, uint8_t mode,
			      uint8_t cmd_enable)
{
	int ret = 0;
	int val;
	int fail_counter = 0;
	uint8_t add = 0;

	mutex_lock(&lpi->control_mutex);

	while (1) {
		val = gpio_get_value(lpi->intr_pin);
		D("[CM3218] %s, interrupt GPIO val = %d, fail_counter %d\n",
		  __func__, val, fail_counter);

		val = gpio_get_value(lpi->intr_pin);
		if (val == 0) {
			ret = cm3218_i2c_read_byte(lpi, &add);

			D("[CM3218] %s, interrupt GPIO val = %d,"
			" check_interrupt_add value = 0x%x, ret %d\n",
			__func__, val, add, ret);
		}
		val = gpio_get_value(lpi->intr_pin);
		if (val == 0) {
			ret =
			    cm3218_i2c_read_byte(lpi, &add);

			D("[CM3218] %s, interrupt GPIO val = %d,"
			" check_interrupt_add value = 0x%x, ret %d\n",
			__func__, val, add, ret);
		}

		lpi->is_cmd &= CM3218_ALS_INT_MASK;
		ret =
		    cm3218_i2c_write_word(lpi, ALS_CMD, lpi->is_cmd);
		if (ret == 0) {
			break;
		} else {
			fail_counter++;
			val = gpio_get_value(lpi->intr_pin);
			D("[CM3218] %s, interrupt GPIO val = %d,"
			" ,inital fail_counter %d\n",
			__func__, val, fail_counter);
		}
		if (fail_counter >= 10) {
			D("[CM3218] %s, clear INT fail_counter = %d\n",
			  __func__, fail_counter);
			if (lpi->record_clear_int_fail == 0)
				lpi->record_clear_int_fail = 1;
			ret = -ENOMEM;
			goto error_clear_interrupt;
		}
	}

	if (mode == CONTROL_ALS) {
		if (cmd_enable)
			lpi->is_cmd &= CM3218_ALS_SD_MASK;
		else
			lpi->is_cmd |= CM3218_ALS_SD;

		cm3218_i2c_write_word(lpi, ALS_CMD, lpi->is_cmd);

		lpi->als_enable = cmd_enable;
	}

	if ((mode == CONTROL_ALS) && (cmd_enable == 1)) {
		input_report_abs(lpi->ls_input_dev, ABS_MISC, -1);
		input_sync(lpi->ls_input_dev);
		msleep(100);
	}

	if (lpi->als_enable)
		report_ls_value(lpi);

	ret = cm3218_i2c_write_word(lpi, ALS_CMD, lpi->is_cmd);
	if (ret == 0)
		D("[CM3218] %s, re-enable INT OK\n", __func__);
	else
		D("[CM3218] %s, re-enable INT FAIL\n", __func__);

error_clear_interrupt:
	mutex_unlock(&lpi->control_mutex);
	return ret;
}

static void sensor_irq_do_work(struct work_struct *work)
{
	struct cm3218_info *lpi = \
		container_of(work, struct cm3218_info, sensor_irq_work);

	control_and_report(lpi, CONTROL_INT_ISR_REPORT, 0);

	enable_irq(lpi->irq);
}

static irqreturn_t cm3218_irq_handler(int irq, void *data)
{
	struct cm3218_info *lpi = data;

	disable_irq_nosync(lpi->irq);

	queue_work(lpi->lp_wq, &lpi->sensor_irq_work);

	return IRQ_HANDLED;
}

static int als_power(struct cm3218_info *lpi)
{
	if (lpi->power)
		lpi->power(LS_PWR_ON, 1);

	return 0;
}

static void ls_initial_cmd(struct cm3218_info *lpi)
{
	/*must disable l-sensor interrupt befrore IST create disable ALS func */
	lpi->is_cmd |= CM3218_ALS_SD;
	cm3218_i2c_write_word(lpi, ALS_CMD, lpi->is_cmd);
}

void lightsensor_set_kvalue(struct cm3218_info *lpi)
{
	if (!lpi) {
		pr_err("[CM3218 error]%s: ls_info is empty\n", __func__);
		return;
	}

	D("[CM3218] %s: ALS calibrated als_kadc=%d\n",
	  __func__, als_kadc);

	if (als_kadc >> 16 == ALS_CALIBRATED)
		lpi->als_kadc = als_kadc & 0xFFFF;
	else {
		lpi->als_kadc = 0;
		D("[CM3218] %s: no ALS calibrated\n", __func__);
	}

	if (lpi->als_kadc && lpi->golden_adc > 0) {
		lpi->als_kadc = (lpi->als_kadc > 0 && lpi->als_kadc < 0x1000) ?
		    lpi->als_kadc : lpi->golden_adc;
		lpi->als_gadc = lpi->golden_adc;
	} else {
		lpi->als_kadc = 1;
		lpi->als_gadc = 1;
	}
	D("[CM3218] %s: als_kadc=%d, als_gadc=%d\n",
	  __func__, lpi->als_kadc, lpi->als_gadc);
}

static int lightsensor_update_table(struct cm3218_info *lpi)
{
	uint32_t tmpData[10];
	int i;
	for (i = 0; i < 10; i++) {
		tmpData[i] = (uint32_t) (*(lpi->adc_table + i))
		    * lpi->als_kadc / lpi->als_gadc;
		if (tmpData[i] <= 0xFFFF)
			lpi->cali_table[i] = (uint16_t) tmpData[i];
		else
			lpi->cali_table[i] = 0xFFFF;

		D("[CM3218] %s: Calibrated adc_table: data[%d], %d\n",
		  __func__, i, lpi->cali_table[i]);
	}

	return 0;
}

static int lightsensor_enable(struct cm3218_info *lpi)
{
	int ret = -EIO;

	mutex_lock(&lpi->als_enable_mutex);
	D("[CM3218] %s\n", __func__);

	if (lpi->als_enable) {
		D("[CM3218] %s: already enabled\n", __func__);
		ret = 0;
	} else
		ret = control_and_report(lpi, CONTROL_ALS, 1);
	mutex_unlock(&lpi->als_enable_mutex);
	return ret;
}

static int lightsensor_disable(struct cm3218_info *lpi)
{
	int ret = -EIO;
	mutex_lock(&lpi->als_disable_mutex);
	D("[CM3218] %s\n", __func__);

	if (lpi->als_enable == 0) {
		D("[CM3218] %s: already disabled\n", __func__);
		ret = 0;
	} else
		ret = control_and_report(lpi, CONTROL_ALS, 0);
	mutex_unlock(&lpi->als_disable_mutex);
	return ret;
}

static int lightsensor_open(struct inode *inode, struct file *file)
{
	struct cm3218_info *lpi = lp_info;
	int rc = 0;

	D("[CM3218] %s\n", __func__);
	if (lpi->lightsensor_opened) {
		pr_err("[CM3218 error]%s: already opened\n", __func__);
		rc = -EBUSY;
	}
	lpi->lightsensor_opened = 1;
	return rc;
}

static int lightsensor_release(struct inode *inode, struct file *file)
{
	struct cm3218_info *lpi = lp_info;

	D("[CM3218] %s\n", __func__);
	lpi->lightsensor_opened = 0;
	return 0;
}

static long lightsensor_ioctl(struct file *file, unsigned int cmd,
			      unsigned long arg)
{
	int rc, val;
	struct cm3218_info *lpi = lp_info;

	/*D("[CM3218] %s cmd %d\n", __func__, _IOC_NR(cmd)); */

	switch (cmd) {
	case LIGHTSENSOR_IOCTL_ENABLE:
		if (get_user(val, (unsigned long __user *)arg)) {
			rc = -EFAULT;
			break;
		}
		D("[CM3218] %s LIGHTSENSOR_IOCTL_ENABLE, value = %d\n",
		  __func__, val);
		rc = val ? lightsensor_enable(lpi) : lightsensor_disable(lpi);
		break;
	case LIGHTSENSOR_IOCTL_GET_ENABLED:
		val = lpi->als_enable;
		D("[CM3218] %s LIGHTSENSOR_IOCTL_GET_ENABLED, enabled %d\n",
		  __func__, val);
		rc = put_user(val, (unsigned long __user *)arg);
		break;
	default:
		pr_err("[CM3218 error]%s: invalid cmd %d\n",
		       __func__, _IOC_NR(cmd));
		rc = -EINVAL;
	}

	return rc;
}

static const struct file_operations lightsensor_fops = {
	.owner = THIS_MODULE,
	.open = lightsensor_open,
	.release = lightsensor_release,
	.unlocked_ioctl = lightsensor_ioctl
};

static struct miscdevice lightsensor_misc = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "lightsensor",
	.fops = &lightsensor_fops
};

static ssize_t attr_adc_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	int ret;
	uint16_t adc_value;
	struct cm3218_info *lpi = dev_get_drvdata(dev);

	get_ls_adc_value(lpi, &adc_value, 0);
	lpi->current_adc = adc_value;

	ret = sprintf(buf, "%d\n", lpi->current_adc);

	return ret;
}

static ssize_t attr_enable_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct cm3218_info *lpi = dev_get_drvdata(dev);

	ret = sprintf(buf, "%d\n", lpi->als_enable);

	return ret;
}

static ssize_t attr_enable_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int ret = 0;
	int ls_auto;
	struct cm3218_info *lpi = dev_get_drvdata(dev);

	ls_auto = -1;
	sscanf(buf, "%d", &ls_auto);

	if (ls_auto != 0 && ls_auto != 1 && ls_auto != 147)
		return -EINVAL;

	if (ls_auto)
		ret = lightsensor_enable(lpi);
	else
		ret = lightsensor_disable(lpi);

	D("[CM3218] %s: lpi->als_enable = %d,"
	" lpi->ls_calibrate = %d, ls_auto=%d\n",
	__func__, lpi->als_enable, lpi->ls_calibrate,
	ls_auto);

	if (ret < 0)
		pr_err("[CM3218 error]%s: set auto light sensor fail\n",
		       __func__);

	return count;
}

static ssize_t attr_kadc_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret;
	struct cm3218_info *lpi = dev_get_drvdata(dev);

	ret = sprintf(buf, "%d\n", lpi->als_kadc);

	return ret;
}

static ssize_t attr_kadc_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int kadc_temp = 0;
	struct cm3218_info *lpi = dev_get_drvdata(dev);

	sscanf(buf, "%d", &kadc_temp);

	mutex_lock(&lpi->als_get_adc_mutex);
	if (kadc_temp != 0) {
		lpi->als_kadc = kadc_temp;
		if (lpi->als_gadc != 0) {
			if (lightsensor_update_table(lpi) < 0)
				pr_err("[CM3218 error] %s:"
				"update ls table fail\n", __func__);
		} else {
			pr_info("[CM3218]%s: als_gadc = %d wait to be set\n",
			       __func__, lpi->als_gadc);
		}
	} else {
		pr_info("[CM3218]%s: als_kadc can't be set to zero\n",
		       __func__);
	}

	mutex_unlock(&lpi->als_get_adc_mutex);
	return count;
}

static ssize_t attr_gadc_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int ret;
	struct cm3218_info *lpi = dev_get_drvdata(dev);

	ret = sprintf(buf, "%d\n", lpi->als_gadc);

	return ret;
}

static ssize_t attr_gadc_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int gadc_temp = 0;
	struct cm3218_info *lpi = dev_get_drvdata(dev);

	sscanf(buf, "%d", &gadc_temp);

	mutex_lock(&lpi->als_get_adc_mutex);
	if (gadc_temp != 0) {
		lpi->als_gadc = gadc_temp;
		if (lpi->als_kadc != 0) {
			if (lightsensor_update_table(lpi) < 0)
				pr_err("[CM3218 error] %s: update"
				"ls table fail\n", __func__);
		} else {
			pr_info("[CM3218]%s: als_kadc =%d wait to be set\n",
			       __func__, lpi->als_kadc);
		}
	} else {
		pr_info("[CM3218]%s: als_gadc can't be set to zero\n",
		       __func__);
	}
	mutex_unlock(&lpi->als_get_adc_mutex);
	return count;
}

static ssize_t attr_adc_table_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	unsigned length = 0;
	int i;
	struct cm3218_info *lpi = dev_get_drvdata(dev);

	for (i = 0; i < 10; i++) {
		length += sprintf(buf + length,
		"%d\n", *(lpi->adc_table + i));
	}
	return length;
}

static ssize_t attr_adc_table_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	char *token[10];
	uint16_t tempdata[10];
	int i;
	struct cm3218_info *lpi = dev_get_drvdata(dev);

	pr_info("[CM3218]%s\n", buf);
	for (i = 0; i < 10; i++) {
		token[i] = strsep((char **)&buf, " ");
		tempdata[i] = simple_strtoul(token[i], NULL, 10);
		if (tempdata[i] < 1 || tempdata[i] > 0xffff) {
			pr_err("[CM3218 error] adc_table[%d] =  %d Err\n",
			       i, tempdata[i]);
			return count;
		}
	}
	mutex_lock(&lpi->als_get_adc_mutex);
	for (i = 0; i < 10; i++) {
		lpi->adc_table[i] = tempdata[i];
		pr_info("[CM3218]Set lpi->adc_table[%d] =  %d\n",
		       i, *(lpi->adc_table + i));
	}
	if (lightsensor_update_table(lpi) < 0)
		pr_err("[CM3218 error] %s: update ls table fail\n", __func__);
	mutex_unlock(&lpi->als_get_adc_mutex);
	D("[CM3218] %s\n", __func__);

	return count;
}

static ssize_t attr_cali_table_show(struct device *dev,
				 struct device_attribute *attr, char *buf)
{
	unsigned length = 0;
	int i;
	struct cm3218_info *lpi = dev_get_drvdata(dev);

	for (i = 0; i < 10; i++) {
		length += sprintf(buf + length,
		"%d\n", *(lpi->cali_table + i));
	}
	return length;
}

static uint8_t als_conf = -1;
static ssize_t attr_conf_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x\n", als_conf);
}

static ssize_t attr_conf_store(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t count)
{
	int value = 0;
	struct cm3218_info *lpi = dev_get_drvdata(dev);
	sscanf(buf, "0x%x", &value);

	als_conf = value;
	pr_info("[CM3218]set als_conf = %x\n", als_conf);
	cm3218_i2c_write_word(lpi, ALS_CMD, als_conf);
	return count;
}

static ssize_t attr_fLevel_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct cm3218_info *lpi = dev_get_drvdata(dev);
	return sprintf(buf, "%d\n", lpi->current_level);
}

static ssize_t attr_fLevel_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	int value = 0;
	int fLevel = -1;
	struct cm3218_info *lpi = dev_get_drvdata(dev);
	sscanf(buf, "%d", &value);
	(value >= 0) ? (value = min(value, 10)) : (value = max(value, -1));
	fLevel = value;
	input_report_abs(lpi->ls_input_dev, ABS_MISC, fLevel);
	input_sync(lpi->ls_input_dev);
	pr_info("[CM3218]set fLevel = %d\n", fLevel);

	msleep(1000);
	fLevel = -1;
	return count;
}

static struct device_attribute dev_attr_light_enable =
__ATTR(enable, S_IRUGO | S_IWUGO, attr_enable_show,
						attr_enable_store);

static struct device_attribute dev_attr_light_kadc =
__ATTR(kadc, S_IRUGO | S_IWUGO, attr_kadc_show, attr_kadc_store);

static struct device_attribute dev_attr_light_gadc =
__ATTR(gadc, S_IRUGO | S_IWUGO, attr_gadc_show, attr_gadc_store);

static struct device_attribute dev_attr_light_adc_table =
__ATTR(adc_table, S_IRUGO | S_IWUGO, attr_adc_table_show,
	attr_adc_table_store);

static struct device_attribute dev_attr_light_cali_table =
__ATTR(cali_table, S_IRUGO, attr_cali_table_show, NULL);

static struct device_attribute dev_attr_light_conf =
__ATTR(conf, S_IRUGO | S_IWUGO, attr_conf_show, attr_conf_store);

static struct device_attribute dev_attr_light_fLevel =
__ATTR(fLevel, S_IRUGO | S_IWUGO, attr_fLevel_show,
						attr_fLevel_store);

static struct device_attribute dev_attr_light_adc =
__ATTR(adc, S_IRUGO, attr_adc_show, NULL);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	&dev_attr_light_kadc.attr,
	&dev_attr_light_gadc.attr,
	&dev_attr_light_adc_table.attr,
	&dev_attr_light_cali_table.attr,
	&dev_attr_light_conf.attr,
	&dev_attr_light_fLevel.attr,
	&dev_attr_light_adc.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static int lightsensor_setup(struct cm3218_info *lpi)
{
	int ret;

	lpi->ls_input_dev = input_allocate_device();
	if (!lpi->ls_input_dev) {
		pr_err("[CM3218 error]%s: could not allocate"
		" ls input device\n", __func__);
		return -ENOMEM;
	}
	lpi->ls_input_dev->name = "cm3218-ls";

	input_set_drvdata(lpi->ls_input_dev, lpi);

	set_bit(EV_ABS, lpi->ls_input_dev->evbit);
	input_set_abs_params(lpi->ls_input_dev, ABS_MISC, 0, 9, 0, 0);

	ret = input_register_device(lpi->ls_input_dev);
	if (ret < 0) {
		pr_err("[CM3218 error]%s: can not register"
		" ls input device\n", __func__);
		goto err_free_ls_input_device;
	}

	ret = misc_register(&lightsensor_misc);
	if (ret < 0) {
		pr_err
		    ("[CM3218 error]%s: can not register ls misc device\n",
		     __func__);
		goto err_unregister_ls_input_device;
	}

	return ret;

err_unregister_ls_input_device:
	input_unregister_device(lpi->ls_input_dev);
err_free_ls_input_device:
	input_free_device(lpi->ls_input_dev);
	return ret;
}

static int initial_cm3218(struct cm3218_info *lpi)
{
	int val, ret, fail_counter = 0;
	uint8_t add = 0;

	val = gpio_get_value(lpi->intr_pin);
	D("[CM3218] %s, INTERRUPT GPIO val = %d\n", __func__, val);

check_interrupt_gpio:
	if (fail_counter >= 10) {
		D("[CM3218] %s, initial fail_counter = %d\n", __func__,
		  fail_counter);
		if (lpi->record_init_fail == 0)
			lpi->record_init_fail = 1;
	/*If devices without cm3218 chip and did not probe driver */
		return -ENOMEM;
	}
	lpi->is_cmd = lpi->is_cmd | CM3218_ALS_SD;
	ret =
	    cm3218_i2c_write_word(lpi, ALS_CMD, lpi->is_cmd);
	if ((ret < 0) && (fail_counter < 10)) {
		fail_counter++;
		val = gpio_get_value(lpi->intr_pin);
		if (val == 0) {
			D("[CM3218] %s, interrupt GPIO val = %d,"
			" , inital fail_counter %d\n",
			__func__, val, fail_counter);

			ret = cm3218_i2c_read_byte(lpi, &add);

			D("[CM3218] %s, check_interrupt_add value = 0x%x,"
			" ret %d\n",  __func__, add, ret);
		}
		val = gpio_get_value(lpi->intr_pin);
		if (val == 0) {
			D("[CM3218] %s, interrupt GPIO val = %d,"
			" ,inital fail_counter %d\n",
			__func__, val, fail_counter);

			ret = cm3218_i2c_read_byte(lpi, &add);

			D("[CM3218] %s, check_interrupt_add value = 0x%x,"
			" ret %d\n", __func__, add, ret);
		}
		goto check_interrupt_gpio;
	}

	return 0;
}

static int cm3218_setup(struct cm3218_info *lpi)
{
	int ret = 0;

	als_power(lpi);
	msleep(5);
	ret = gpio_request(lpi->intr_pin, "gpio_cm3218_intr");
	if (ret < 0) {
		pr_err("[CM3218 error]%s: gpio %d request failed (%d)\n",
		       __func__, lpi->intr_pin, ret);
		return ret;
	}

	ret = gpio_direction_input(lpi->intr_pin);
	if (ret < 0) {
		pr_err("[CM3218 error]%s: fail to set gpio %d as input"
		" (%d)\n", __func__, lpi->intr_pin, ret);
		goto fail_free_intr_pin;
	}

	ret = initial_cm3218(lpi);
	if (ret < 0) {
		pr_err
		    ("[CM3218 error]%s: fail to initial cm3218 (%d)\n",
		     __func__, ret);
		goto fail_free_intr_pin;
	}

	/*Default disable L sensor */
	ls_initial_cmd(lpi);

	ret = request_any_context_irq(lpi->irq,
				      cm3218_irq_handler,
				      IRQF_TRIGGER_FALLING, "cm3218", lpi);
	if (ret < 0) {
		pr_err("[CM3218 error]%s: req_irq(%d) fail for i gpio %d"
		" (%d)\n", __func__, lpi->irq, lpi->intr_pin, ret);
		goto fail_free_intr_pin;
	}

	return ret;

fail_free_intr_pin:
	gpio_free(lpi->intr_pin);
	return ret;
}

static void cm3218_early_suspend(struct early_suspend *h)
{
	struct cm3218_info *lpi = \
		container_of(h, struct cm3218_info, early_suspend);
	D("[CM3218] %s\n", __func__);

	if (lpi->als_enable)
		lightsensor_disable(lpi);
}

static void cm3218_late_resume(struct early_suspend *h)
{
	struct cm3218_info *lpi = \
		container_of(h, struct cm3218_info, early_suspend);

	D("[CM3218] %s\n", __func__);

	if (!lpi->als_enable)
		lightsensor_enable(lpi);
}

static int cm3218_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct cm3218_info *lpi;
	struct cm3218_platform_data *pdata;

	D("[CM3218] %s\n", __func__);

	lpi = kzalloc(sizeof(struct cm3218_info), GFP_KERNEL);
	if (!lpi)
		return -ENOMEM;

	/*D("[CM3218] %s: client->irq = %d\n", __func__, client->irq); */

	lpi->i2c_client = client;
	pdata = client->dev.platform_data;
	if (!pdata) {
		pr_err("[CM3218 error]%s: Assign platform_data error!!\n",
		       __func__);
		ret = -EBUSY;
		goto err_platform_data_null;
	}

	lpi->irq = client->irq;

	i2c_set_clientdata(client, lpi);

	lpi->intr_pin = pdata->intr;
	lpi->adc_table = pdata->levels;
	lpi->power = pdata->power;

	lpi->als_cmd_address = pdata->als_slave_address;
	lpi->check_interrupt_add = pdata->check_interrupt_add;

	lpi->is_cmd = pdata->is_cmd;

	lpi->j_start = 0;
	lpi->j_end = 0;
	lpi->record_clear_int_fail = 0;
	lpi->record_init_fail = 0;

	if (pdata->is_cmd == 0) {
		lpi->is_cmd =
		    CM3218_ALS_SM_2 | CM3218_ALS_IT_250ms | CM3218_ALS_PERS_1 |
		    CM3218_ALS_RES_1;
	}
	lp_info = lpi;

	mutex_init(&lpi->control_mutex);

	mutex_init(&lpi->als_enable_mutex);
	mutex_init(&lpi->als_disable_mutex);
	mutex_init(&lpi->als_get_adc_mutex);

	ret = lightsensor_setup(lpi);
	if (ret < 0) {
		pr_err("[CM3218 error]%s: lightsensor_setup error!!\n",
		       __func__);
		goto err_lightsensor_setup;
	}
	/* SET LUX STEP FACTOR HERE
	if adc raw value 70 eqauls to 1 lux
	the following will set the factor 1/70
	and lpi->golden_adc = 1;
	set als_kadc = (ALS_CALIBRATED <<16) | 70; */

	als_kadc = (ALS_CALIBRATED << 16) | 70;
	lpi->golden_adc = 1;

	/* ls calibrate always set to 1 */
	lpi->ls_calibrate = 1;

	lightsensor_set_kvalue(lpi);
	ret = lightsensor_update_table(lpi);
	if (ret < 0) {
		pr_err("[CM3218 error]%s: update ls table fail\n", __func__);
		goto err_lightsensor_update_table;
	}

	INIT_WORK(&lpi->sensor_irq_work, sensor_irq_do_work);
	lpi->lp_wq = create_singlethread_workqueue("cm3218_wq");
	if (!lpi->lp_wq) {
		pr_err("[CM3218 error]%s: can't create workqueue\n", __func__);
		ret = -ENOMEM;
		goto err_create_singlethread_workqueue;
	}
	wake_lock_init(&(lpi->ps_wake_lock), WAKE_LOCK_SUSPEND, "proximity");

	ret = cm3218_setup(lpi);
	if (ret < 0) {
		pr_err("[CM3218 error]%s: cm3218_setup error!\n", __func__);

	goto err_cm3218_setup;
	}

	lpi->cm3218_class = class_create(THIS_MODULE, "optical_sensors");
	if (IS_ERR(lpi->cm3218_class)) {
		ret = PTR_ERR(lpi->cm3218_class);
		lpi->cm3218_class = NULL;
		goto err_create_class;
	}

	lpi->ls_dev = device_create(lpi->cm3218_class,
				    NULL, 0, "%s", "lightsensor");
	if (unlikely(IS_ERR(lpi->ls_dev))) {
		ret = PTR_ERR(lpi->ls_dev);
		lpi->ls_dev = NULL;
		goto err_create_ls_device;
	}

	/* register the attributes */
	ret = sysfs_create_group(&lpi->ls_input_dev->dev.kobj,
				 &light_attribute_group);
	if (ret) {
		pr_err("[CM3232 error]%s: could not create sysfs group\n",
		       __func__);
		goto err_sysfs_create_group_light;
	}

	lpi->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	lpi->early_suspend.suspend = cm3218_early_suspend;
	lpi->early_suspend.resume = cm3218_late_resume;
	register_early_suspend(&lpi->early_suspend);

	D("[CM3218] %s: Probe success!\n", __func__);
	lpi->als_enable = 0;

	return ret;

err_sysfs_create_group_light:
	device_unregister(lpi->ls_dev);
err_create_ls_device:
	class_destroy(lpi->cm3218_class);
err_create_class:
err_cm3218_setup:
	destroy_workqueue(lpi->lp_wq);
	wake_lock_destroy(&(lpi->ps_wake_lock));

	input_unregister_device(lpi->ls_input_dev);
	input_free_device(lpi->ls_input_dev);
err_create_singlethread_workqueue:
err_lightsensor_update_table:
	mutex_destroy(&lpi->control_mutex);
	misc_deregister(&lightsensor_misc);
err_lightsensor_setup:
	mutex_destroy(&lpi->als_enable_mutex);
	mutex_destroy(&lpi->als_disable_mutex);
	mutex_destroy(&lpi->als_get_adc_mutex);
err_platform_data_null:
	kfree(lpi);
	return ret;
}

static const struct i2c_device_id cm3218_i2c_id[] = {
	{CM3218_I2C_NAME, 0},
	{}
};

static struct i2c_driver cm3218_driver = {
	.id_table = cm3218_i2c_id,
	.probe = cm3218_probe,
	.driver = {
		   .name = CM3218_I2C_NAME,
		   .owner = THIS_MODULE,
		   },
};

static int __init cm3218_init(void)
{
	return i2c_add_driver(&cm3218_driver);
}

static void __exit cm3218_exit(void)
{
	i2c_del_driver(&cm3218_driver);
}

module_init(cm3218_init);
module_exit(cm3218_exit);

MODULE_DESCRIPTION("CM3218 Driver");
MODULE_LICENSE("GPL");
