/*
 * Copyright by ams AG
 * All rights are reserved.
 *
 * IMPORTANT - PLEASE READ CAREFULLY BEFORE COPYING, INSTALLING OR USING
 * THE SOFTWARE.
 *
 * THIS SOFTWARE IS PROVIDED FOR USE ONLY IN CONJUNCTION WITH AMS PRODUCTS.
 * USE OF THE SOFTWARE IN CONJUNCTION WITH NON-AMS-PRODUCTS IS EXPLICITLY
 * EXCLUDED.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/irq.h>
#include <linux/mutex.h>
#include <linux/unistd.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/pm.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/gpio/consumer.h>
#include <linux/sensor/sensors_core.h>
#include <linux/kfifo.h>

#include "ams_dax_reg.h"
#include "ams_dax.h"
#include "ams_i2c.h"

#ifdef CONFIG_OF
#include <linux/of_device.h>
#endif

#undef TEST
#undef DEBUG

#ifdef TEST
static u8 refcnt;
#endif

#define SMUX_SIZE (20)
#define STR1_MAX (16)
#define STR2_MAX (256)

#define VENDOR_NAME       "AMS"
#define CHIP_NAME         "TCS3701"
#define MODULE_NAME       "light_sensor"

#define REL_CH0 REL_DIAL
#define REL_CH1 REL_MISC

#define DEFAULT_DELAY_MS  200

enum als_channel_config {
	ALS_CH_CFG_1 = 1,
	ALS_CH_CFG_2,
	ALS_CH_CFG_3,
	ALS_CH_CFG_4,
	ALS_CH_CFG_5,
	ALS_CH_CFG_6,
	ALS_CH_CFG_MAX = ALS_CH_CFG_6,
	ALS_CH_CFG_DFLT = ALS_CH_CFG_4
};

static u8 smux_data[ALS_CH_CFG_MAX][SMUX_SIZE] = {
	{	/* 1 channel CRGBW */
		0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x11, 0x11,
		0x11, 0x11, 0x00, 0x00
	},
	{	/* 2 channel pos-neg */
		0x12, 0x10, 0x21, 0x21,
		0x11, 0x20, 0x12, 0x22,
		0x01, 0x21, 0x20, 0x12,
		0x12, 0x22, 0x21, 0x12,
		0x11, 0x02, 0x00, 0x00
	},
	{	/* 3 channel pos-neg + W */
		0x12, 0x13, 0x21, 0x21,
		0x11, 0x20, 0x12, 0x22,
		0x31, 0x21, 0x23, 0x12,
		0x12, 0x22, 0x21, 0x12,
		0x11, 0x32, 0x00, 0x00
	},
	{	/* 4 channel CRGB */
		0x14, 0x20, 0x23, 0x41,
		0x33, 0x12, 0x14, 0x24,
		0x03, 0x23, 0x10, 0x14,
		0x32, 0x44, 0x21, 0x23,
		0x13, 0x04, 0x00, 0x00
	},
	{	/* 5 channel CRGBW */
		0x14, 0x25, 0x23, 0x41,
		0x33, 0x12, 0x14, 0x24,
		0x53, 0x23, 0x15, 0x14,
		0x32, 0x44, 0x21, 0x23,
		0x13, 0x54, 0x00, 0x00
	},
	{
		0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0,
		0x0, 0x0, 0x0, 0x0
	}
};

static u8 data[32];

/*
 * ID, REVID, AUXID
 */
static u8 const dax_ids[] = {
	0x18,	0x00,	0x00,
	0x18,	0x31,	0x02,
	0x18,	0x11,	0x02,
	0x18,	0x51,	0x02,
	0x18,	0x01,	0xc3,
};

static char const *dax_names[] = {
	"tmd49073s",
	"tcs3701",
	"tcs3707",
	"tcs34077s",
	"tmd4910",
};

static struct i2c_device_id dax_idtable[] = {
	{CHIP_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, dax_idtable);

void dax_als_ch_cfg_work(struct dax_chip *chip)
{
	u8 als_ch_cfg = chip->als_ch_cfg;

	AMS_MUTEX_LOCK(&chip->lock);
	// enter config mode
	chip->cfg_mode = 1;

	// disable chip
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x00);

	// change smux
	/* smux write command */
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_CFG6, 0x10);

	/* send smux remap sequence */
	ams_i2c_reg_blk_write(chip->client,
			DAX_REG_RAM_START, smux_data[als_ch_cfg - 1],
			SMUX_SIZE);

	/* execute the smux command */
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x11);

	udelay(200);

	/* clear the smux command bit */
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_CFG6, 0x00);

	// disable unused channels
	if (als_ch_cfg == 1)
		chip->als_ch_disable = 0x3E;
	else if (als_ch_cfg == 2)
		chip->als_ch_disable = 0x3C;
	else if (als_ch_cfg == 3)
		chip->als_ch_disable = 0x38;
	else if (als_ch_cfg == 4)
		chip->als_ch_disable = 0x30;
	else if (als_ch_cfg == 5)
		chip->als_ch_disable = 0x20;
	else if (als_ch_cfg == 6)
		chip->als_ch_disable = 0x00;

	ams_i2c_write(chip->client, chip->shadow,
		DAX_REG_ALS_CHANNEL_CTRL, chip->als_ch_disable);

	// change fifo map
	if (als_ch_cfg == 1)
		chip->fifo_map = 0x02;
	else if (als_ch_cfg == 2)
		chip->fifo_map = 0x06;
	else if (als_ch_cfg == 3)
		chip->fifo_map = 0x0E;
	else if (als_ch_cfg == 4)
		chip->fifo_map = 0x1E;
	else if (als_ch_cfg == 5)
		chip->fifo_map = 0x3E;
	else if (als_ch_cfg == 6)
		chip->fifo_map = 0x7E;

	i2c_smbus_write_byte_data(chip->client, DAX_REG_FIFO_MAP, chip->fifo_map);

	// flush dax fifo
	ams_i2c_set_field(chip->client, chip->shadow, DAX_REG_CONTROL, 0x1, 0x1, 0x1);

	chip->fifo_overflowx = 0;
#ifdef DEBUG
	dev_info(&chip->client->dev, "fifo + overflowx reset\n");
#endif
	// enable als
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x03);

	// exit config mode
	chip->cfg_mode = 0;
	AMS_MUTEX_UNLOCK(&chip->lock);
}

static ssize_t wtime_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;

	AMS_MUTEX_LOCK(&chip->lock);
	chip->wtime = i2c_smbus_read_byte_data(chip->client, DAX_REG_WTIME);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", chip->wtime);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

static ssize_t wtime_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	int ret;
	struct dax_chip *chip = dev_get_drvdata(dev);

	AMS_MUTEX_LOCK(&chip->lock);
	ret = kstrtol(buf, 0, (long *)(&(chip->wtime)));
	i2c_smbus_write_byte_data(chip->client, DAX_REG_WTIME, chip->wtime);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return count;
}

static ssize_t astep_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;

	AMS_MUTEX_LOCK(&chip->lock);
	chip->astep = i2c_smbus_read_word_data(chip->client, DAX_REG_ASTEPL);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", chip->astep);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

// turn off power
// flush hardware fifo
// flush kfifo
// change astep
// turn on power
static ssize_t astep_store(struct device *dev, struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	struct dax_chip *chip = dev_get_drvdata(dev);

	AMS_MUTEX_LOCK(&chip->lock);
	ret = kstrtol(buf, 0, (long *)(&(chip->astep)));

	//disable als
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x01);
	i2c_smbus_write_word_data(chip->client, DAX_REG_ASTEPL, chip->astep);
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x03);
	AMS_MUTEX_UNLOCK(&chip->lock);
	
	SENSOR_INFO("astep=%d\n", chip->astep);

	return count;
}

static ssize_t again_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;

	AMS_MUTEX_LOCK(&chip->lock);
	ams_i2c_read(chip->client, DAX_REG_CFG1, &chip->again);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", chip->again);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

static ssize_t again_store(struct device *dev, struct device_attribute *attr,
	const char *buf, size_t count)
{
	int ret;
	u8 again;
	struct dax_chip *chip = dev_get_drvdata(dev);

	AMS_MUTEX_LOCK(&chip->lock);

	ret = kstrtol(buf, 0, (long *)(&(again)));

	if (again >= 0 && again <= 12) {
		ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x01);
		ams_i2c_modify(chip->client, chip->shadow,
				DAX_REG_CFG1, DAX_MASK_AGAIN, again);
		ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x03);

		ams_i2c_set_field(chip->client, chip->shadow, DAX_REG_CONTROL,
				0x1, 0x1, 0x1);

		chip->fifo_overflowx = 0;
#ifdef DEBUG
		dev_info(&chip->client->dev, "fifo + overflowx reset\n");
#endif
	}

	AMS_MUTEX_UNLOCK(&chip->lock);

	return count;
}

static ssize_t fifo_reset_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dax_chip *chip = dev_get_drvdata(dev);

	AMS_MUTEX_LOCK(&chip->lock);
	ams_i2c_modify(chip->client, chip->shadow, DAX_REG_CONTROL,
		DAX_MASK_FIFO_CLR, 0x1);

	chip->fifo_overflowx = 0;
#ifdef DEBUG
	dev_info(&chip->client->dev, "fifo + overflowx reset\n");
#endif
	AMS_MUTEX_UNLOCK(&chip->lock);

	return count;
}

static ssize_t adata0_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	u16 adata;

	AMS_MUTEX_LOCK(&chip->lock);
	adata = i2c_smbus_read_word_data(chip->client, DAX_REG_ADATAL0);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", adata);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

static ssize_t adata1_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	u16 adata;

	AMS_MUTEX_LOCK(&chip->lock);
	adata = i2c_smbus_read_word_data(chip->client, DAX_REG_ADATAL1);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", adata);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

static ssize_t adata2_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	u16 adata;

	AMS_MUTEX_LOCK(&chip->lock);
	adata = i2c_smbus_read_word_data(chip->client, DAX_REG_ADATAL2);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", adata);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

static ssize_t adata3_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	u16 adata;

	AMS_MUTEX_LOCK(&chip->lock);
	adata = i2c_smbus_read_word_data(chip->client, DAX_REG_ADATAL3);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", adata);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

static ssize_t adata4_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	u16 adata;

	AMS_MUTEX_LOCK(&chip->lock);
	adata = i2c_smbus_read_word_data(chip->client, DAX_REG_ADATAL4);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", adata);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

static ssize_t adata5_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	u16 adata;

	AMS_MUTEX_LOCK(&chip->lock);
	adata = i2c_smbus_read_word_data(chip->client, DAX_REG_ADATAL5);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", adata);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

static ssize_t fifo_level_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;

	AMS_MUTEX_LOCK(&chip->lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", chip->fifo_level);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

static ssize_t fifo_overflowx_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;

	AMS_MUTEX_LOCK(&chip->lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", chip->fifo_overflowx);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

static ssize_t light_name_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static ssize_t als_ch_cfg_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;
	u8 als_ch_cfg;

	ret = kstrtol(buf, 0, (long *)(&(als_ch_cfg)));
	if (ret != 0)
		dev_err(&chip->client->dev, "kstrtol() error.\n");

	if ((als_ch_cfg != chip->als_ch_cfg) && (als_ch_cfg >= 1 && als_ch_cfg <= 6)) {

		dev_info(&chip->client->dev, "set als_ch_cfg to %d\n", als_ch_cfg);
		chip->als_ch_cfg = als_ch_cfg;

		dax_als_ch_cfg_work(chip);
	}


	return count;
}

static ssize_t als_ch_cfg_show(struct device *dev, struct device_attribute *attr,
		char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	ssize_t ret;

	AMS_MUTEX_LOCK(&chip->lock);
	ret = snprintf(buf, PAGE_SIZE, "%d\n", chip->als_ch_cfg);
	AMS_MUTEX_UNLOCK(&chip->lock);

	return ret;
}

static ssize_t light_reg_data_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	u8 reg = 0;
	int offset = 0;
	u8 val = 0;

	for (reg = 0x00; reg <= 0xFF; reg++) {
		val = i2c_smbus_read_byte_data(chip->client, reg);
		SENSOR_INFO("Read Reg: 0x%2x Value: 0x%2x\n", reg, val);
		offset += snprintf(buf + offset, PAGE_SIZE - offset,
			"Reg: 0x%2x Value: 0x%2x\n", reg, val);
	}

	return offset;
}

static ssize_t light_reg_data_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	int reg, val, ret;

	if (sscanf(buf, "%2x,%4x", &reg, &val) != 2) {
		SENSOR_ERR("invalid value\n");
		return count;
	}

	ret = ams_i2c_write(chip->client, chip->shadow, reg, val);
	if(!ret)
		SENSOR_INFO("Register(0x%2x) data(0x%4x)\n", reg, val);
	else
		SENSOR_ERR("failed %d\n", ret);

	return count;
}

static void dax_report_events(struct dax_chip *chip)
{
	int i = 0;

	for (i = 0; i < chip->bytes; i = i + 4) {
		chip->ch0 = (data[i + 1] << 8) | data[i + 0];
		chip->ch1 = (data[i + 3] << 8) | data[i + 2];

#ifdef DEBUG
		SENSOR_INFO("data%d - ch0=%d(%d-%d), ch1=%d(%d-%d)\n", i, chip->ch0, data[i + 0], data[i + 1], chip->ch1, data[i + 2], data[i + 3]);
#endif
		input_report_rel(chip->input_dev, REL_CH0, chip->ch0 + 1);
		input_report_rel(chip->input_dev, REL_CH1, chip->ch1 + 1);
		input_sync(chip->input_dev);
	}
}

static void dax_work_func_light(struct work_struct *work)
{
	struct dax_chip *chip = container_of((struct delayed_work *)work, struct dax_chip, main_work);
	unsigned long delay = nsecs_to_jiffies(atomic_read(&chip->delay));
	int ret;

#ifdef DEBUG
	SENSOR_INFO("\n");
	SENSOR_INFO("chip->cfg_mode=%d\n", chip->cfg_mode);
	SENSOR_INFO("delay=%d\n", atomic_read(&chip->delay));

	dev_info(&chip->client->dev, "%d, "
			"[0x%x, 0x%x, 0x%x, 0x%x] "
			"[0x%x, 0x%x, 0x%x, 0x%x] "
			"[0x%x, 0x%x, 0x%x, 0x%x] "
			"[0x%x, 0x%x, 0x%x, 0x%x]\n",
		refcnt,
		_buf[0], _buf[1], _buf[2], _buf[3],
		_buf[4], _buf[5], _buf[6], _buf[7],
		_buf[8], _buf[9], _buf[10], _buf[11],
		_buf[12], _buf[13], _buf[14], _buf[15]);
	dev_info(&chip->client->dev, "%d, "
			"[0x%x, 0x%x, 0x%x, 0x%x] "
			"[0x%x, 0x%x, 0x%x, 0x%x] "
			"[0x%x, 0x%x, 0x%x, 0x%x] "
			"[0x%x, 0x%x, 0x%x, 0x%x]\n",
		refcnt,
		_buf[16], _buf[17], _buf[18], _buf[19],
		_buf[20], _buf[21], _buf[22], _buf[23],
		_buf[24], _buf[25], _buf[26], _buf[27],
		_buf[28], _buf[29], _buf[30], _buf[31]);
#endif

	if (chip->cfg_mode != 1) {
		u8 val;
		ams_i2c_read(chip->client, DAX_REG_STATUS6, &val);

		val &= DAX_MASK_FIFO_OV;
		val >>= DAX_FIFO_OV_SHIFT;
		chip->fifo_overflowx += val;
#ifdef DEBUG
		SENSOR_INFO("chip->fifo_overflowx=%d\n", val);
		if (val > 0)
			dev_err(&chip->client->dev, "dax fifo overflow [0x%x]\n", val);
#endif	

		ams_i2c_read(chip->client, DAX_REG_FIFO_LVL, &chip->fifo_level);

#ifdef DEBUG
		SENSOR_INFO("chip->fifo_level=%d\n", chip->fifo_level);
#endif

		/* give the chip's FIFO time to fill up */
		if (chip->fifo_level > 32) {
			if (chip->als_ch_cfg == 1 || chip->als_ch_cfg == 2 ||
				chip->als_ch_cfg == 4)
				chip->bytes = 32;
			else if (chip->als_ch_cfg == 3 || chip->als_ch_cfg == 5)
				chip->bytes = 30;
			else if (chip->als_ch_cfg == 6)
				chip->bytes = 24;
#ifdef DEBUG
			SENSOR_INFO("chip->bytes=%d\n", chip->bytes);
#endif

			memset(&data, 0x0, 32);

			ret = i2c_smbus_read_i2c_block_data(chip->client, DAX_REG_FDATAL, chip->bytes, (u8 *)&data);

			dax_report_events(chip);
		}
	}

	schedule_delayed_work(&chip->main_work, delay);
}

static void dax_light_enable(struct dax_chip *chip)
{
	SENSOR_INFO("\n");
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x03);
	schedule_delayed_work(&chip->main_work, nsecs_to_jiffies(atomic_read(&chip->delay)));
}

static void dax_light_disable(struct dax_chip *chip)
{
	SENSOR_INFO("\n");
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x01);
	cancel_delayed_work_sync(&chip->main_work);
}

static ssize_t light_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct dax_chip *chip = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SENSOR]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	pr_info("[SENSOR]: %s - new_value = %u\n", __func__, enable);

	if (enable && !chip->als_enabled) {
		dax_light_enable(chip);
	} else if (!enable && chip->als_enabled) {
		dax_light_disable(chip);
	}

	chip->als_enabled = enable;

	return size;
}

static ssize_t light_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", chip->als_enabled);
}

static ssize_t light_poll_delay_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	int ret = 0;

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&chip->delay));

	return ret;
}

static ssize_t light_poll_delay_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct dax_chip *chip = dev_get_drvdata(dev);
	int64_t new_delay;
	int ret;

	ret = kstrtoll(buf, 10, &new_delay);
	if (ret) {
		pr_err("[SENSOR]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	if (new_delay > DEFAULT_DELAY_MS * NSEC_PER_MSEC)
		new_delay = DEFAULT_DELAY_MS * NSEC_PER_MSEC;

	atomic_set(&chip->delay, new_delay);
	pr_info("[SENSOR]: %s - poll_delay = %lld\n", __func__, new_delay);

	return count;
}

static ssize_t light_vendor_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t light_lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", chip->ch0, chip->ch1);
}

static ssize_t light_data_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dax_chip *chip = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", chip->ch0, chip->ch1);
}

static DEVICE_ATTR_RO(adata0);
static DEVICE_ATTR_RO(adata1);
static DEVICE_ATTR_RO(adata2);
static DEVICE_ATTR_RO(adata3);
static DEVICE_ATTR_RO(adata4);
static DEVICE_ATTR_RO(adata5);
static DEVICE_ATTR_RW(astep);
static DEVICE_ATTR_RW(again);
static DEVICE_ATTR_RW(wtime);
static DEVICE_ATTR_RW(als_ch_cfg);
static DEVICE_ATTR_RO(fifo_level);
static DEVICE_ATTR_RO(fifo_overflowx);
static DEVICE_ATTR_WO(fifo_reset);
static DEVICE_ATTR(name, S_IRUGO, light_name_show, NULL);
static DEVICE_ATTR(reg_data, S_IRUGO, light_reg_data_show, light_reg_data_store);
static DEVICE_ATTR(vendor, S_IRUGO, light_vendor_show, NULL);
static DEVICE_ATTR(lux, S_IRUGO, light_lux_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, light_data_show, NULL);

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_adata0,
	&dev_attr_adata1,
	&dev_attr_adata2,
	&dev_attr_adata3,
	&dev_attr_adata4,
	&dev_attr_adata5,
	&dev_attr_astep,
	&dev_attr_again,
	&dev_attr_wtime,
	&dev_attr_als_ch_cfg,
	&dev_attr_fifo_level,
	&dev_attr_fifo_overflowx,
	&dev_attr_fifo_reset,
	&dev_attr_name,
	&dev_attr_reg_data,
	&dev_attr_vendor,
	&dev_attr_lux,
	&dev_attr_raw_data,
	NULL,
};

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		light_poll_delay_show, light_poll_delay_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		light_enable_show, light_enable_store);

static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL,
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};

static int dax_init_input_device(struct dax_chip *chip)
{
	int ret = 0;
	struct input_dev *dev;

	/* allocate lightsensor input_device */
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name = MODULE_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, REL_CH0);
	input_set_capability(dev, EV_REL, REL_CH1);
	input_set_drvdata(dev, chip);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		return ret;
	}

	ret = sensors_create_symlink(&dev->dev.kobj, dev->name);
	if (ret < 0) {
		input_unregister_device(dev);
		return ret;
	}

	ret = sysfs_create_group(&dev->dev.kobj, &light_attribute_group);
	if (ret < 0) {
		sensors_remove_symlink(&dev->dev.kobj, dev->name);
		input_unregister_device(dev);
		return ret;
	}

	chip->input_dev = dev;
	return 0;
}

static int dax_get_id(struct dax_chip *chip, u8 *id, u8 *rev, u8 *aux)
{
	ams_i2c_read(chip->client, DAX_REG_AUXID, aux);
	ams_i2c_read(chip->client, DAX_REG_REVID, rev);
	ams_i2c_read(chip->client, DAX_REG_ID, id);

	return 0;
}

#ifdef CONFIG_OF
int dax_parse_dt(struct device *dev, struct dax_i2c_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	u32 val;

	if (!of_property_read_u32(np, "ams,persist", &val))
		pdata->parameters.persist = val;
	
	SENSOR_INFO("pdata->parameters.persist=%d\n", pdata->parameters.persist);

	if (!of_property_read_u32(np, "ams,als_gain", &val))
		pdata->parameters.als_gain = val;
	
	SENSOR_INFO("pdata->parameters.als_gain=%d\n", pdata->parameters.als_gain);

	if (!of_property_read_u32(np, "ams,als_deltap", &val))
		pdata->parameters.als_deltaP = val;

	if (!of_property_read_u32(np, "ams,als_time", &val))
		pdata->parameters.als_time = val;
	
	SENSOR_INFO("pdata->parameters.als_time=%d\n", pdata->parameters.als_time);

	if (!of_property_read_u32(np, "ams,dgf", &val))
		pdata->parameters.dgf = val;

	if (!of_property_read_u32(np, "ams,ct_coef", &val))
		pdata->parameters.ct_coef = val;

	if (!of_property_read_u32(np, "ams,ct_offset", &val))
		pdata->parameters.ct_offset = val;

	if (!of_property_read_u32(np, "ams,c_coef", &val))
		pdata->parameters.c_coef = val;

	if (!of_property_read_u32(np, "ams,r_coef", &val))
		pdata->parameters.r_coef = val;

	if (!of_property_read_u32(np, "ams,g_coef", &val))
		pdata->parameters.g_coef = val;

	if (!of_property_read_u32(np, "ams,b_coef", &val))
		pdata->parameters.b_coef = val;

	if (!of_property_read_u32(np, "ams,coef_scale", &val))
		pdata->parameters.coef_scale = val;

	SENSOR_INFO("pdata->parameters.coef_scale=%d\n", pdata->parameters.coef_scale);

	return 0;
}

static const struct of_device_id dax_of_match[] = {
	{ .compatible = "ams,tcs3701" },
	{ }
};
MODULE_DEVICE_TABLE(of, dax_of_match);
#endif

static int dax_probe(struct i2c_client *client,
		const struct i2c_device_id *idp)
{
	int i, ret;
	u8 id, rev, aux, test;
	char strtmp [STR1_MAX] = "";
	char smux_str[STR2_MAX] = "";

	static struct dax_chip *chip;
	struct dax_i2c_platform_data *pdata;
	struct device *dev = &client->dev;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "%s: i2c smbus byte data unsupported\n", __func__);
		return -EOPNOTSUPP;
	}

	chip = devm_kzalloc(dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	mutex_init(&chip->lock);

	chip->client = client;
	i2c_set_clientdata(client, chip);

	ret = dax_get_id(chip, &id, &rev, &aux);
	dev_info(dev, "%s: device id:%02x device revid:%02x device aux:%02x\n",  __func__, id, rev, aux);

	for (i = 0; i < ARRAY_SIZE(dax_ids)/3; i++) {
		if (id == (dax_ids[i*3+0]))
			if (rev == (dax_ids[i*3+1]))
				if (aux == (dax_ids[i*3+2]))
					break;
	}

	if (i < ARRAY_SIZE(dax_names)) {
		dev_info(dev, "%s: '%s revid_2: 0x%x' detected\n", __func__, dax_names[i], aux);
	} else {
		dev_err(dev, "%s: not supported chip id\n", __func__);
		ret = -EOPNOTSUPP;
		//goto id_failed;
	}

	pdata = devm_kzalloc(dev, sizeof(struct dax_i2c_platform_data), GFP_KERNEL);
	if (!pdata)
		return -ENOMEM;

	ret = dax_parse_dt(&client->dev, pdata);
	if (ret) {
		SENSOR_ERR("dax_parse_dt failed, ret=%d\n", ret);
		return ret;
	}

	chip->pdata = pdata;

	chip->als_ch_cfg = ALS_CH_CFG_DFLT;

	for (i = 0; i < SMUX_SIZE; i++) {
		snprintf(strtmp, STR1_MAX, "0x%02x ", smux_data[ALS_CH_CFG_DFLT - 1][i]);
		strncat(smux_str, strtmp, STR1_MAX);
	}

	dev_info(dev, "num-adc-channels: %d\n", ALS_CH_CFG_DFLT);
	dev_info(dev, "smux: %s\n", smux_str);

	AMS_MUTEX_LOCK(&chip->lock);

	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x00);
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x01);

	/* smux write command */
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_CFG6, 0x10);

	/* send smux remap sequence */
	ams_i2c_reg_blk_write(chip->client, DAX_REG_RAM_START, smux_data[ALS_CH_CFG_DFLT - 1], SMUX_SIZE);

	/* execute the smux command */
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x11);

	udelay(200);

	/* clear the smux command bit */
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_CFG6, 0x00);

	/* disable chip first */
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x00);

	/* disable unused channels */
	if (chip->als_ch_cfg == 1)
		chip->als_ch_disable = 0x3E;
	else if (chip->als_ch_cfg == 2)
		chip->als_ch_disable = 0x3C;
	else if (chip->als_ch_cfg == 3)
		chip->als_ch_disable = 0x38;
	else if (chip->als_ch_cfg == 4)
		chip->als_ch_disable = 0x30;
	else if (chip->als_ch_cfg == 5)
		chip->als_ch_disable = 0x20;
	else if (chip->als_ch_cfg == 6)
		chip->als_ch_disable = 0x00;
	else
		chip->als_ch_disable = 0x3F; /* invalid als channel config */

	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ALS_CHANNEL_CTRL, chip->als_ch_disable);

	ams_i2c_write(chip->client, chip->shadow, DAX_REG_AILTL, 0x00);
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_AILTH, 0x00);
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_AIHTL, 0xFF);
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_AIHTH, 0xFF);
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_PERS, 0x00);

	chip->astep = 180; /* 0.5 ms */
	//chip->astep = 359; /* 1 ms */
	//chip->astep = 1799; /* 50 ms */
	i2c_smbus_write_word_data(chip->client, DAX_REG_ASTEPL, chip->astep);
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ATIME, 0x00);

	chip->wtime = 0x00;
	i2c_smbus_write_byte_data(chip->client, DAX_REG_WTIME, chip->wtime);
	i2c_smbus_write_byte_data(chip->client, DAX_REG_INTENAB, 0x04);

	chip->again = 0x0C; /* 2048x */
	i2c_smbus_write_byte_data(chip->client, DAX_REG_CFG1, chip->again);

	/* configure fifo map for als */
	if (chip->als_ch_cfg == 1)
		chip->fifo_map = 0x02;
	else if (chip->als_ch_cfg == 2)
		chip->fifo_map = 0x06;
	else if (chip->als_ch_cfg == 3)
		chip->fifo_map = 0x0E;
	else if (chip->als_ch_cfg == 4)
		chip->fifo_map = 0x1E;
	else if (chip->als_ch_cfg == 5)
		chip->fifo_map = 0x3E;
	else if (chip->als_ch_cfg == 6)
		chip->fifo_map = 0x7E;
	else
		chip->fifo_map = 0x00; /* invalid als channel config */

	i2c_smbus_write_byte_data(chip->client, DAX_REG_FIFO_MAP, chip->fifo_map);

	/*
	 * disable HW AGC
	 * disable flicker
	 * set concurrent prox/als
	 * set fifo threshold to 16
	 */
	i2c_smbus_write_byte_data(chip->client, DAX_REG_CFG8, 0xD8);

	ams_i2c_read(chip->client, DAX_REG_PCFG1, &test);

	/* 163us "setup" time */
	ams_i2c_set_field(chip->client, chip->shadow, DAX_REG_PCFG1, 0x3, 0x1, 0x1);
	ams_i2c_read(chip->client, DAX_REG_PCFG1, &test);
	ams_i2c_read(chip->client, DAX_REG_CONTROL, &test);

	/* clear FIFO */
	ams_i2c_set_field(chip->client, chip->shadow, DAX_REG_CONTROL, 0x1, 0x1, 0x1);
	ams_i2c_read(chip->client, DAX_REG_CONTROL, &test);

	/* power on */
	ams_i2c_write(chip->client, chip->shadow, DAX_REG_ENABLE, 0x01);

	AMS_MUTEX_UNLOCK(&chip->lock);
	
	ret = dax_init_input_device(chip);
	if (ret) {
		SENSOR_ERR("Input device init failed, ret=%d\n", ret);
		goto err_init_input_device;
	}

	INIT_DELAYED_WORK(&chip->main_work, dax_work_func_light);
	atomic_set(&chip->delay, DEFAULT_DELAY_MS * NSEC_PER_MSEC);

	/* set sysfs for light sensor */
	ret = sensors_register(&chip->ls_dev, chip, sensor_attrs, MODULE_NAME);
	if (ret < 0) {
		SENSOR_ERR("Sensor registration failed, ret=%d\n", ret);
		goto err_sensors_register;
	}

#ifdef TEST
	dev_info(dev, "Test mode.\n");
#endif
	dev_info(dev, "Probe ok.\n");
	return 0;

err_sensors_register:
	sensors_remove_symlink(&chip->input_dev->dev.kobj, chip->input_dev->name);
	input_unregister_device(chip->input_dev);
err_init_input_device:
//id_failed:
	kfree(chip);
	dev_err(dev, "Probe failed.\n");
	return ret;
}

static int dax_suspend(struct device *dev)
{
	struct dax_chip *chip = dev_get_drvdata(dev);

	SENSOR_INFO("\n");
	
	if (chip->als_enabled)
		dax_light_disable(chip);

	return 0;
}

static int dax_resume(struct device *dev)
{
	struct dax_chip *chip = dev_get_drvdata(dev);

	SENSOR_INFO("\n");
	
	if (chip->als_enabled)
		dax_light_enable(chip);

	return 0;
}

static int dax_remove(struct i2c_client *client)
{
	struct dax_chip *chip = i2c_get_clientdata(client);

	 /* clear hw fifo */
	ams_i2c_set_field(chip->client, chip->shadow, DAX_REG_CONTROL, 0x1, 0x1, 0x1);

	return 0;
}

static const struct dev_pm_ops dax_pm_ops = {
	.suspend = dax_suspend,
	.resume  = dax_resume,
};

static struct i2c_driver dax_driver = {
	.driver = {
		.name = CHIP_NAME,
		.pm = &dax_pm_ops,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(dax_of_match),
#endif
	},
	.id_table = dax_idtable,
	.probe = dax_probe,
	.remove = dax_remove,
};

module_i2c_driver(dax_driver);

MODULE_AUTHOR("AMS AOS Software<cs.americas@ams.com>");
MODULE_DESCRIPTION("AMS-TAOS sensor driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0");
