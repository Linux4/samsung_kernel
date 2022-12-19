
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>

#include "sii9136.h"
#include "sii9136_driver.h"
#include "sii9136_reg.h"

#if 0
#define SII_LOG_FUNCTION_NAME_ENTRY             printk(KERN_INFO "[SiI9136]## %s() ++ ##\n",  __func__);
#define SII_LOG_FUNCTION_NAME_EXIT              printk(KERN_INFO "[SiI9136]## %s() -- ##\n",  __func__);
#else
#define SII_LOG_FUNCTION_NAME_ENTRY
#define SII_LOG_FUNCTION_NAME_EXIT
#endif

#ifndef LATTICE_ORIGINAL
struct sii9136_regulator_info regulator_info[REGULATOR_MAXCOUNT] = {
	{"sii9136,hdmi_vdd_1p2", 1200000},
	{"sii9136,hdmi_vdd_3p3", 3300000},
	{"sii9136,hdmi_vdd_2p5", 2500000},
	{"sii9136,hdmi_vdd_1p8", 1800000},
};
#endif
struct workqueue_struct *hdmi_wq = NULL;

// struct i2c_driver sii9136_i2c_driver;
struct i2c_client *sii9136_tpi_i2c_client = NULL;
struct i2c_client *sii9136_cpi_i2c_client = NULL;
struct i2c_client *sii9136_edid_i2c_client = NULL;
struct i2c_client *sii9136_edid_seg_i2c_client = NULL;


int HDMI_i2c_init = 0;


struct i2c_xfer_mem {
	uint8_t *block_tx_buffers;
} i2c_mem;

static struct i2c_adapter *i2c_bus_adapter;

struct i2c_dev_info {
	uint8_t dev_addr;
	struct i2c_client *client;
};

struct sii9136_data {
	struct sii9136_platform_data    *pdata;
	struct delayed_work    work;
	struct workqueue_struct    *workqueue;
	struct mutex    lock;
	struct wake_lock hdmi_lock;
	struct delayed_work dwc3_ref_clk_work;
	bool hdmi_connected;
};

struct i2c_client* get_simgI2C_client(u8 device_id);

#if 1
int hdmi_read_reg(uint8_t slave_addr, uint8_t offset)
{
	int ret = 0;

//	sii9136_client->addr = (slave_addr >> 1);

	struct i2c_client *client_ptr = get_simgI2C_client(slave_addr);

	ret = i2c_smbus_read_byte_data(client_ptr, offset);
	if (ret < 0) {
		pr_err("%s %s: failed to read i2c addr=0x%x, val=0x%x\n ", LOG_TAG,
			__func__, slave_addr, offset);
		return ret;
	}
	else {
		pr_err("%s %s: success to read i2c addr=0x%x, val=0x%x read_val=0x%x\n", LOG_TAG,
			__func__, slave_addr, offset, ret);
		return ret;
	}
//	*buf = (uint8_t) ret;
}

int hdmi_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value)
{
	int ret = 0;

//	sii9136_i2c_client->addr = (slave_addr >> 1);

	struct i2c_client *client_ptr = get_simgI2C_client(slave_addr);

	ret = i2c_smbus_write_byte_data(client_ptr, offset, value);
	if (ret < 0) {
		pr_err("%s %s(): failed to write i2c addr=%x, offset=0x%x, val=0x%x\n", LOG_TAG,
			__func__, slave_addr, offset, value);
	}
	else {

		printk("%s %s(): success to write i2c addr=%x, offset=0x%x, val=0x%x\n", LOG_TAG,
			__func__, slave_addr, offset, value);
	}
	
	return ret;
}

int hdmi_read_block_reg(uint8_t slave_addr, uint8_t offset,	u8 len, u8 *values)
{
	int ret=0;
	int i;

	#if 0
	struct i2c_client *client_ptr = get_simgI2C_client(slave_addr);

	if (!values)
		return -EINVAL;

	ret = i2c_smbus_read_i2c_block_data(client_ptr, offset, len, values);
	if (ret < 0) {
		pr_err("%s %s(): failed to read i2c block addr=%x, offset=0x%x, len=0x%x\n", LOG_TAG,__func__, slave_addr, offset,len);
		}
	else {
		printk("%s %s(): success to read i2c block addr=%x, offset=0x%x, len=0x%x\n", LOG_TAG,__func__, slave_addr, offset,len);
		}
	#endif

	if (!values)
		return -EINVAL;

	for(i=0;i<len;i++)
	{
		ret = hdmi_read_reg(slave_addr,offset+i);
		values[i]=ret;
		if (ret < 0) {
			pr_err("%s %s(): failed to read i2c block addr=%x, offset=0x%x, len=0x%x\n", LOG_TAG,__func__, slave_addr, offset+i, len);
			return ret;
		}
	}
	return ret;
}

int hdmi_ReadSegmentBlock(uint8_t slave_addr,uint8_t segment,uint8_t offset, u8 len, u8 *values)
{
	int ret=0;
#ifdef LATTICE_ORIGINAL
	int i;
#endif
	if (!values)
		return -EINVAL;

	hdmi_write_reg(EDID_SEG_ADDR,0x01, segment);

#if 0
	struct i2c_client *client_ptr = get_simgI2C_client(slave_addr);
	ret = i2c_smbus_read_i2c_block_data(client_ptr, offset, len, values);
	if (ret < 0) {
		pr_err("%s %s(): failed to read i2c seg block addr=%x, offset=0x%x, len=0x%x\n", LOG_TAG,
			__func__, slave_addr, len);
	}
	else {
		printk("%s %s(): success to read i2c seg block addr=%x, offset=0x%x, len=0x%x\n", LOG_TAG,
			__func__, slave_addr, len);
		}
#endif

	hdmi_read_block_reg(slave_addr, offset, len, values);

	return ret;	
}

int hdmi_write_block_reg(uint8_t slave_addr, uint8_t offset,u8 len, u8 *values)
{
	int ret=0;
	int i;

	if (!values)
		return -EINVAL;
#if 0

	struct i2c_client *client_ptr = get_simgI2C_client(slave_addr);

	ret = i2c_smbus_write_i2c_block_data(client_ptr, offset, len, values);
	if (ret < 0) {
		pr_err("%s %s(): failed to write i2c addr=%x\n", LOG_TAG,
			__func__, slave_addr);
	}
	else {

		printk("%s %s(): success to write i2c addr=%x\n", LOG_TAG,
				__func__, slave_addr);
	}
#endif

	for(i=0;i<len;i++)	
	{
		ret = hdmi_write_reg(slave_addr,offset+i, values[i]);
		if (ret < 0) {
			pr_err("%s %s(): failed to write block i2c addr=%x\n", LOG_TAG,__func__, slave_addr);
		}
	}

	return ret;
}


#endif

u8 sii9136_i2c_read(struct i2c_client *client, u8 reg)
{
	u8 ret;
	SII_LOG_FUNCTION_NAME_ENTRY;
/*	
	if(!MHL_i2c_init)
	{
		DEV_DBG_ERROR("I2C not ready");
		return 0;
	}
*/
	i2c_smbus_write_byte(client, reg);
	

	ret = i2c_smbus_read_byte(client);

	//printk(KERN_ERR "#######Read reg %x data %x\n", reg, ret);

	if (ret < 0)
	{
		DEV_DBG_ERROR("i2c read fail");
		return -EIO;
	}
    SII_LOG_FUNCTION_NAME_EXIT;
	return ret;

}

int sii9136_i2c_write(struct i2c_client *client, u8 reg, u8 data)
{
    int rc = 0;
    
    SII_LOG_FUNCTION_NAME_ENTRY;
/*
	if(!HDMI_i2c_init)
	{
		DEV_DBG_ERROR("I2C not ready");
		return 0;
	}
*/
	//printk(KERN_ERR "#######Write reg %x data %x\n", reg, data);
	rc = i2c_smbus_write_byte_data(client, reg, data);
	SII_LOG_FUNCTION_NAME_EXIT;
	return rc; 
}




struct i2c_client* get_simgI2C_client(u8 device_id)
{

	struct i2c_client* client_ptr;

	if(device_id == 0x72)
		client_ptr = sii9136_tpi_i2c_client;
	else if(device_id == 0xC0)
		client_ptr = sii9136_cpi_i2c_client;
	else if(device_id == 0xA0)
		client_ptr = sii9136_edid_i2c_client;
	else if(device_id == 0x60)
		client_ptr = sii9136_edid_seg_i2c_client;
	else
		client_ptr = NULL;
    
	return client_ptr;
}


u8 I2C_ReadByte(u8 deviceID, u8 offset)
{
    struct i2c_msg msg[2];
    u8 reg_buf[] = { offset };
    u8 data_buf[] = { 0 };
    int err;

    struct i2c_client* client_ptr = get_simgI2C_client(deviceID);

    if(!client_ptr)
    {
        DEV_DBG_ERROR("[MHL]I2C_ReadByte error %x\n",deviceID); 
        return 0;	
    }
/*
	if(is_MHL_connected() == false)
	{
        //DEV_DBG("[MHL] mhl disconnected\n");
      	  return 0;			
	}
*/
    msg[0].addr = client_ptr->addr;
    msg[0].flags = 0;
    msg[0].len = 1;
    msg[0].buf = reg_buf;

    msg[1].addr = client_ptr->addr;
    msg[1].flags = I2C_M_RD;
    msg[1].len = 1;
    msg[1].buf = data_buf;

    err = i2c_transfer(client_ptr->adapter, msg, 2);

    if (err < 0) {
        DEV_DBG_ERROR("I2C err: %d\n", err);
        return 0xFF; //err;
    }  

    //printk(KERN_INFO "$$$$ I2C 7A data :%x @@@ %x\n",*data_buf, data_buf[0] );

    return *data_buf;
}


int I2C_WriteByte(u8 deviceID, u8 offset, u8 value)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[2];
	struct i2c_client* client_ptr = get_simgI2C_client(deviceID);

	printk("%s\nI2C write!!!\nDeviceID: 0x%x, offset: 0x%x, value: 0x%x\n", __func__, deviceID, offset, value);

/*
	if(is_MHL_connected() == false)
	{
        //DEV_DBG("[MHL] mhl disconnected\n");
      	  return 0;			
	}
*/
	
    msg->addr = client_ptr->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = data;
	data[0] = offset;
	data[1] = value;

	err = i2c_transfer(client_ptr->adapter, msg, 1);
	
	if (err >= 0)
	{
	    return 0;
    }
  
	printk("I2C write err: %d\n", err);
	return err;
}


#if 0
int hdmi_write_reg(u8 deviceID, u8 offset, u8 value)
{
	int err;
	struct i2c_msg msg[1];
	unsigned char data[2];
//	struct i2c_client* client_ptr = get_simgI2C_client(deviceID);

	
    msg->addr = sii9136_client->addr;
	msg->flags = 0;
	msg->len = 2;
	msg->buf = data;
	data[0] = offset;
	data[1] = value;

	err = i2c_transfer(sii9136_client->adapter, msg, 1);
	
	if (err >= 0)
	{
	    return 0;
    }
  
  pr_err("%s %s: failed to write i2c addr=%x\n", LOG_TAG,
			  __func__, msg->addr);

	return err;
}
#endif

static inline int platform_write_i2c_block(struct i2c_adapter *i2c_bus, u8 page,
	u8 offset, u16 count, u8 *values)
{
	struct i2c_msg msg;
	u8 *buffer;
	int ret;

	buffer = kmalloc(count + 1, GFP_KERNEL);
	if (!buffer) {
		printk(KERN_ERR "%s:%d buffer allocation failed\n",
			__func__, __LINE__);
		return -ENOMEM;
	}

	buffer[0] = offset;
	memmove(&buffer[1], values, count);

	msg.flags = 0;
	msg.addr = page >> 1;
	msg.buf = buffer;
	msg.len = count + 1;

	ret = i2c_transfer(i2c_bus, &msg, 1);

	kfree(buffer);

	if (ret != 1) {
		printk(KERN_ERR "%s:%d I2c write failed 0x%02x:0x%02x\n",
			__func__, __LINE__, page, offset);
		ret = -EIO;
	} else {
		ret = 0;
	}

	return ret;
}

int hdmi_tx_write_reg_block(u8 page, u8 offset,
			       u16 count, u8 *values)
{
//	DUMP_I2C_TRANSFER(page, offset, count, values, true);

	return platform_write_i2c_block(i2c_bus_adapter, page, offset, count,
					values);
}

int hdmi_tx_write_reg(u8 page, u8 offset, u8 value)
{
	return hdmi_tx_write_reg_block(page, offset, 1, &value);
}

#ifdef LATTICE_ORIGINAL
static void sii9136_free_gpio(struct sii9136_data *sii9136)
{

}
#endif

static int sii9136_system_init(void)
{
#if 1
	hdmi_write_reg(TPI, 0xC7, 0x00);		//Enable HW TPI
	hdmi_write_reg(TPI, 0x2A, 0x00);		//HDCP Off

	hdmi_write_reg(TPI, 0x1A, 0x11);		//TMDS off, HDMI out
	hdmi_write_reg(TPI, 0x1E, 0x00);		// D0 Full operation
	hdmi_write_reg(TPI, 0x00, 0x00);
	hdmi_write_reg(TPI, 0x01, 0x00);
	hdmi_write_reg(TPI, 0x02, 0x00);
	hdmi_write_reg(TPI, 0x03, 0x00);
	hdmi_write_reg(TPI, 0x04, 0x00);
	hdmi_write_reg(TPI, 0x05, 0x00);
	hdmi_write_reg(TPI, 0x06, 0x00);
	hdmi_write_reg(TPI, 0x07, 0x00);
	hdmi_write_reg(TPI, 0x08, 0x60);		//TMDS clk * 1, full wide
	hdmi_write_reg(TPI, 0x09, 0x00);		//RGB 8bpp
	hdmi_write_reg(TPI, 0x0A, 0x00);		//RGB 8bpp

	hdmi_write_reg(TPI, 0x0B, 0x00);		// YC normal input
	hdmi_write_reg(TPI, 0x1A, 0x00);			//TMDS on, HDMI out

	hdmi_write_reg(TPI, 0x0C, 0x5F);		// AVI Checksum (0x100 - (0x82+0x02+0x0D) + sum of (0x0D-0x19))
	hdmi_write_reg(TPI, 0x0D, 0x00);		// Byte 1 - RGB
	hdmi_write_reg(TPI, 0x0E, 0x00);		// Byte 2 - ITU601, 16:9 Aspect
	hdmi_write_reg(TPI, 0x0F, 0x00);
	hdmi_write_reg(TPI, 0x10, 0x10);		// Byte 4 - VIC Code: 16
	hdmi_write_reg(TPI, 0x11, 0x00);		// Byte 5 - No pixel replication
	hdmi_write_reg(TPI, 0x12, 0x00);
	hdmi_write_reg(TPI, 0x13, 0x00);
	hdmi_write_reg(TPI, 0x14, 0x00);
	hdmi_write_reg(TPI, 0x15, 0x00);
	hdmi_write_reg(TPI, 0x16, 0x00);
	hdmi_write_reg(TPI, 0x17, 0x00);
	hdmi_write_reg(TPI, 0x18, 0x00);
	hdmi_write_reg(TPI, 0x19, 0x00);
#else
	I2C_WriteByte(TPI, 0xC7, 0x00);		//Enable HW TPI
	I2C_WriteByte(TPI, 0x2A, 0x00);		//HDCP Off

	I2C_WriteByte(TPI, 0x1A, 0x11);		//TMDS off, HDMI out
	I2C_WriteByte(TPI, 0x1E, 0x00);		// D0 Full operation
	I2C_WriteByte(TPI, 0x00, 0x00);
	I2C_WriteByte(TPI, 0x01, 0x00);
	I2C_WriteByte(TPI, 0x02, 0x00);
	I2C_WriteByte(TPI, 0x03, 0x00);
	I2C_WriteByte(TPI, 0x04, 0x00);
	I2C_WriteByte(TPI, 0x05, 0x00);
	I2C_WriteByte(TPI, 0x06, 0x00);
	I2C_WriteByte(TPI, 0x07, 0x00);
	I2C_WriteByte(TPI, 0x08, 0x60);		//TMDS clk * 1, full wide
	I2C_WriteByte(TPI, 0x09, 0x00);		//RGB 8bpp
	I2C_WriteByte(TPI, 0x0A, 0x00);		//RGB 8bpp

	I2C_WriteByte(TPI, 0x0B, 0x00);		// YC normal input
//	I2C_WriteByte(TPI, 0x1A, 0x00);			//TMDS on, HDMI out

	I2C_WriteByte(TPI, 0x0C, 0x5F);		// AVI Checksum (0x100 - (0x82+0x02+0x0D) + sum of (0x0D-0x19))
	I2C_WriteByte(TPI, 0x0D, 0x00);		// Byte 1 - RGB
	I2C_WriteByte(TPI, 0x0E, 0x00);		// Byte 2 - ITU601, 16:9 Aspect
	I2C_WriteByte(TPI, 0x0F, 0x00);
	I2C_WriteByte(TPI, 0x10, 0x10);		// Byte 4 - VIC Code: 16
	I2C_WriteByte(TPI, 0x11, 0x00);		// Byte 5 - No pixel replication
	I2C_WriteByte(TPI, 0x12, 0x00);
	I2C_WriteByte(TPI, 0x13, 0x00);
	I2C_WriteByte(TPI, 0x14, 0x00);
	I2C_WriteByte(TPI, 0x15, 0x00);
	I2C_WriteByte(TPI, 0x16, 0x00);
	I2C_WriteByte(TPI, 0x17, 0x00);
	I2C_WriteByte(TPI, 0x18, 0x00);
	I2C_WriteByte(TPI, 0x19, 0x00);
#endif

	return 0;
}

#ifdef LATTICE_ORIGINAL
static int sii9136_parse_dt(
	struct device *dev, struct sii9136_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static ssize_t sysfs_i2c_test_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	char *after;
	unsigned long value = simple_strtoul(buf, &after, 10);

    SII_LOG_FUNCTION_NAME_ENTRY;
    
	printk("sysfs_i2c_test_store value: %ld\n", value);

	if (value == 1) {
		sii9136_system_init();
	}

	return size;
}


/*
 * Sysfs attribute files supported by this driver.
 */
struct device_attribute driver_attribs[] = {
	__ATTR(sysfs_i2c_test, 0644, NULL, sysfs_i2c_test_store),
	__ATTR_NULL
};

#ifdef LATTICE_ORIGINAL
static int sii9136_gpio_init(void)
#else
static int sii9136_gpio_init(struct sii9136_platform_data *pdata)
#endif
{
	int ret = 0;

	pr_info("sii9136 gpio init\n");

#ifdef LATTICE_ORIGINAL
	ret = gpio_request_one(GPIO_HDMI_INT, GPIOF_IN, "9136_int");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
				GPIO_HDMI_INT);
		return ret;
	}

	ret = gpio_request_one(GPIO_HDMI_RESET_N, GPIOF_OUT_INIT_HIGH, "9136_reset");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
				GPIO_HDMI_RESET_N);
		return ret;
	}
#else
	ret = gpio_request(pdata->gpio_hdmi_int,"9136_int");
	if (ret) {
		pr_err("%s : failed to request gpio_int %d\n", __func__,
				pdata->gpio_hdmi_int);
		return ret;
	}

	ret = gpio_request(pdata->gpio_hdmi_reset, "9136_reset");
	if (ret) {
		pr_err("%s : failed to request gpio_reset %d\n", __func__,
				pdata->gpio_hdmi_reset);
		return ret;
	}
#endif

	return ret;
}


static void hdmi_work_func(struct work_struct *work)
{
//	struct sii9136_platform_data *pdata = container_of(work, struct sii9136_platform_data, work);
//	printk("%s() - Work function!!!\n\n", __func__);
	TPI_Poll();

}

#ifdef LATTICE_ORIGINAL
static irqreturn_t sii9136_irq_handler(int irq, void *data)
{
	struct sii9136_platform_data *pdata = data;


//	disable_irq_nosync(pdata->client->irq);
	queue_work(hdmi_wq, &pdata->work);
	return IRQ_HANDLED;
}
#endif

static enum hrtimer_restart hdmi_timer_func(struct hrtimer *timer)
{
	struct sii9136_platform_data *pdata = container_of(timer, struct sii9136_platform_data, timer);

	queue_work(hdmi_wq, &pdata->work);
	hrtimer_start(&pdata->timer, ktime_set(0, 500000000), HRTIMER_MODE_REL); /* 500 msec */

	return HRTIMER_NORESTART;
}

#ifndef LATTICE_ORIGINAL
void sii9136_get_dt_pdata(struct device *dev, struct sii9136_platform_data *pdata)
{
	const char *sii9136_vdd_name;
	int rc = 0;
	int i;

	pdata->gpio_hdmi_int = of_get_named_gpio(dev->of_node, "sii9136,hdmi_int", 0);
	if (!gpio_is_valid(pdata->gpio_hdmi_int)) {
		pr_err("%s: Failed to get is valid hdmi_int\n", __func__);
	}

	pdata->gpio_hdmi_reset = of_get_named_gpio(dev->of_node, "sii9136,hdmi_reset", 0);
	if (!gpio_is_valid(pdata->gpio_hdmi_reset)) {
		pr_err("%s: Failed to get is valid hdmi_reset\n", __func__);
	}


	for (i=0; i< REGULATOR_MAXCOUNT; i++) {
		if(of_property_read_string(dev->of_node,
					regulator_info[i].vdd_name, &sii9136_vdd_name)) {
			pr_err("%s: Unable to find %s\n", __func__, regulator_info[i].vdd_name);
		} else {
			pr_info("%s: %s\n", __func__, sii9136_vdd_name);
			pdata->hdmi_regulators[i] = regulator_get(dev, sii9136_vdd_name);
			if (IS_ERR(pdata->hdmi_regulators[i])) {
				pr_err("%s: Failed to get %s\n", __func__, sii9136_vdd_name);
			} else {
				rc = regulator_set_voltage(pdata->hdmi_regulators[i], regulator_info[i].voltage, regulator_info[i].voltage);
				if (rc) {
					pr_err("%s: regulator_set(%s) failed rc=%d\n", __func__, sii9136_vdd_name, rc);
					regulator_put(pdata->hdmi_regulators[i]);
				} else {
					rc = regulator_enable(pdata->hdmi_regulators[i]);
					if (rc)
						pr_err("%s: regulator_enable(%s) failed rc=%d\n", __func__, sii9136_vdd_name ,rc);
				}
			}
		}
	}

}

#endif

static int __devinit sii9136_tpi_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	struct sii9136_platform_data *pdata;

	struct class *hdmi_class;
	struct device *hdmi_dev;
	int ret = 0;

	pdata = kzalloc(sizeof(struct sii9136_platform_data), GFP_KERNEL);
	if (pdata == NULL) {		
		DEV_DBG_ERROR("failed to allocate memory for SiI9136\n");
		return -ENOMEM;
	}

	pdata->client = client;
	i2c_set_clientdata(client, pdata);

	
	DEV_DBG("SiI9136 attach success!!!\n");


	sii9136_tpi_i2c_client = client;

	HDMI_i2c_init = 1;

#ifdef LATTICE_ORIGINAL
	ret = sii9136_gpio_init();
#else
	sii9136_get_dt_pdata(&client->dev, pdata);
	ret = sii9136_gpio_init(pdata);
#endif
	if (ret) {
		pr_err("%s: failed to initialize gpio\n", __func__);
		goto err0;
	}

	hdmi_class = class_create(THIS_MODULE, "hdmi");
	if (IS_ERR(hdmi_class))
	{
		DEV_DBG_ERROR("Failed to create class(hdmi)!\n");
		printk("%s\nERROR!!!\n\n", __func__);

	}
	
 	hdmi_class->dev_attrs = driver_attribs;

	hdmi_dev = device_create(hdmi_class, NULL, 0, NULL, "hdmi_dev");
	if (IS_ERR(hdmi_dev))
	{
		DEV_DBG_ERROR("Failed to create device(hdmi_dev)!\n");
		
		printk("%s\nERROR!!!\n\n", __func__);
	}

 
	INIT_WORK(&pdata->work, hdmi_work_func);

	printk("%s\nEND!!!\n\n", __func__);

//	sii9136_system_init();

	mdelay(100);


	TPI_Init();

#ifdef SiI9136_DRV_POLLING

	hrtimer_init(&pdata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	pdata->timer.function = hdmi_timer_func;
	hrtimer_start(&pdata->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
#else

#ifdef LATTICE_ORIGINAL
	ret = request_threaded_irq(gpio_to_irq(GPIO_HDMI_INT), NULL, sii9136_irq_handler,
			IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
			"sii9136", pdata);
#else
	ret = request_threaded_irq(gpio_to_irq(pdata->gpio_hdmi_int), NULL, sii9136_irq_handler,
			IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
			"sii9136", pdata);
#endif
	if (ret < 0) {
		pr_err("%s : failed to request irq\n", __func__);
		goto err2;

	}

#endif

    SII_LOG_FUNCTION_NAME_EXIT;

	return 0;

err0:
	sii9136_tpi_i2c_client = NULL;
	kfree(pdata);
#ifndef LATTICE_ORIGINAL
	return ret;
#endif


/////////
#if 0
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct sii9136_data *sii9136;
	struct sii9136_platform_data *pdata = NULL;
	int client_id = -1;

	int ret = 0;



	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	/* going to use block read/write, so check for this too */
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;


	pdata = kzalloc(sizeof(struct sii9136_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "failed to allocate driver data\n");
		return -ENOMEM;
	}

	sii9136->pdata = client->dev.platform_data;
	if (!sii9136->pdata) {
		dev_err(&client->dev, "failed to find platform data\n");
		ret = -EINVAL;
		goto err_exit0;
	}

	return 0;

err_exit0:
	kfree(sii9136);
	return ret;

#endif
}

static int __devinit sii9136_cpi_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct sii9136_platform_data *pdata;

	pdata = kzalloc(sizeof(struct sii9136_platform_data), GFP_KERNEL);
	if (pdata == NULL) {		
		DEV_DBG_ERROR("failed to allocate memory for SiI9136\n");
		return -ENOMEM;
	}

	pdata->client = client;
	i2c_set_clientdata(client, pdata);

	sii9136_cpi_i2c_client = client;

	printk("%s\nEND!!!\n\n", __func__);

	return 0;
}

static int __devinit sii9136_edid_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct sii9136_platform_data *pdata;

	pdata = kzalloc(sizeof(struct sii9136_platform_data), GFP_KERNEL);
	if (pdata == NULL) {		
		DEV_DBG_ERROR("failed to allocate memory for SiI9136\n");
		return -ENOMEM;
	}

	pdata->client = client;
	i2c_set_clientdata(client, pdata);

	sii9136_edid_i2c_client = client;

	printk("%s\nEND!!!\n\n", __func__);

	return 0;
}

static int __devinit sii9136_edid_seg_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct sii9136_platform_data *pdata;

	pdata = kzalloc(sizeof(struct sii9136_platform_data), GFP_KERNEL);
	if (pdata == NULL) {		
		DEV_DBG_ERROR("failed to allocate memory for SiI9136\n");
		return -ENOMEM;
	}

	pdata->client = client;
	i2c_set_clientdata(client, pdata);

	sii9136_edid_seg_i2c_client = client;

	printk("%s\nEND!!!\n\n", __func__);

	return 0;
}


#ifdef LATTICE_ORIGINAL
static int sii9136_tpi_i2c_remove(struct i2c_client *client)
{

	return 0;
}

static int sii9136_cpi_i2c_remove(struct i2c_client *client)
{

	return 0;
}

static int sii9136_edid_i2c_remove(struct i2c_client *client)
{

	return 0;
}

static int sii9136_edid_seg_i2c_remove(struct i2c_client *client)
{

	return 0;
}


static int sii9136_tpi_i2c_suspend(struct i2c_client *client, pm_message_t state)
{
	return 0;
}


static int sii9136_tpi_i2c_resume(struct i2c_client *client)
{
	return 0;
}
#endif

#ifndef LATTICE_ORIGINAL
static int __devinit of_sii9136_probe_dt(struct i2c_client *client,
                        const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	const struct i2c_device_id *id_table = client->driver->id_table;
	static struct sii9136_platform_data *pdata = NULL;
	u32 client_id = -1;

	if (!client->dev.of_node) {
		dev_err(&client->dev, "sii9136: Client node not-found\n");
		return -1;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -EIO;

	/* going to use block read/write, so check for this too */
	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_I2C_BLOCK))
		return -EIO;

	if (of_property_read_u32(client->dev.of_node, "sii9136,client_id", &client_id))
		dev_err(&client->dev, "Wrong Client_id# %d", client_id);

	if (0 == client_id) {
		//pdata = platform_init_data(client);
		sii9136_tpi_i2c_probe(client, id_table);
		pdata = client->dev.platform_data;
	} else {
		client->dev.platform_data = pdata;
		if (1 == client_id)
			sii9136_cpi_i2c_probe(client, id_table);
		else if (2 == client_id)
			sii9136_edid_i2c_probe(client, id_table);
		else if (3 == client_id)
			sii9136_edid_seg_i2c_probe(client, id_table);
	}

        return 0;
}
#endif

#ifdef LATTICE_ORIGINAL
static const struct i2c_device_id sii9136_tpi_id[] = {
	{ "sii9136_tpi", 0 },
	{ }
};

static const struct i2c_device_id sii9136_cpi_id[] = {
	{ "sii9136_cpi", 0 },
	{ }
};

static const struct i2c_device_id sii9136_edid_id[] = {
	{ "sii9136_edid", 0 },
	{ }
};

static const struct i2c_device_id sii9136_edid_seg_id[] = {
	{ "sii9136_edid_seg", 0 },
	{ }
};



MODULE_DEVICE_TABLE(i2c, sii9136_tpi_id);
MODULE_DEVICE_TABLE(i2c, sii9136_cpi_id);
MODULE_DEVICE_TABLE(i2c, sii9136_edid_id);
MODULE_DEVICE_TABLE(i2c, sii9136_edid_seg_id);


static struct i2c_driver sii9136_tpi_i2c_driver = {
	.driver  = {
		.name  = "sii9136_tpi",
		.owner  = THIS_MODULE,
	},
	.probe  = sii9136_tpi_i2c_probe,
	.remove  = sii9136_tpi_i2c_remove,
	.suspend = sii9136_tpi_i2c_suspend,
	.resume = sii9136_tpi_i2c_resume,
	.id_table  = sii9136_tpi_id,
};

static struct i2c_driver sii9136_cpi_i2c_driver = {
	.driver  = {
		.name  = "sii9136_cpi",
		.owner  = THIS_MODULE,
	},
	.probe  = sii9136_cpi_i2c_probe,
	.remove  = sii9136_cpi_i2c_remove,
	.id_table  = sii9136_cpi_id,
};

static struct i2c_driver sii9136_edid_i2c_driver = {
	.driver  = {
		.name  = "sii9136_edid",
		.owner  = THIS_MODULE,
	},
	.probe  = sii9136_edid_i2c_probe,
	.remove  = sii9136_edid_i2c_remove,
	.id_table  = sii9136_edid_id,
};

static struct i2c_driver sii9136_edid_seg_i2c_driver = {
	.driver  = {
		.name  = "sii9136_edid_seg",
		.owner  = THIS_MODULE,
	},
	.probe  = sii9136_edid_seg_i2c_probe,
	.remove  = sii9136_edid_seg_i2c_remove,
	.id_table  = sii9136_edid_seg_id,
};
#else
static struct of_device_id sii9136_dt_ids[] = {
        { .compatible = "sii9136_tpi",},
        { .compatible = "sii9136_cpi",},
        { .compatible = "sii9136_edid",},
        { .compatible = "sii9136_edid_seg",},
        {}
};
MODULE_DEVICE_TABLE(of, sii8240_dt_ids);

static const struct i2c_device_id sii9136_id[] = {
        {"sii9136_hdmi", 0},
        {}
};

MODULE_DEVICE_TABLE(i2c, sii9136_id);

static struct i2c_driver sii9136_i2c_driver = {
        .driver = {
                .owner = THIS_MODULE,
                .name = "sii9136_hdmi_dt",
                .of_match_table = of_match_ptr(sii9136_dt_ids),
//                .pm = &sii8240_pm_ops,
        },
        .id_table = sii9136_id,
        .probe = of_sii9136_probe_dt,
};
#endif

static int __init sii9136_init(void)
{
	int ret;
	pr_info("sii9136 : %s:%d: Starting SiI9136 Driver\n", __func__, __LINE__);

	hdmi_wq = create_singlethread_workqueue("hdmi_wq");
	if (!hdmi_wq)
		return -ENOMEM;

#ifdef LATTICE_ORIGINAL
	ret = i2c_add_driver(&sii9136_edid_i2c_driver);
	if (ret < 0) {
		pr_err("sii9136:%s:%d: [ERROR] hdmi edid i2c driver init failed", __func__, __LINE__);
		return ret;
	}

	ret = i2c_add_driver(&sii9136_edid_seg_i2c_driver);
	if (ret < 0) {
		pr_err("sii9136:%s:%d: [ERROR] hdmi edid_seg i2c driver init failed", __func__, __LINE__);
		return ret;
	}


	ret = i2c_add_driver(&sii9136_cpi_i2c_driver);
	if (ret < 0) {
		pr_err("sii9136:%s:%d: [ERROR] hdmi tx cpi i2c driver init failed", __func__, __LINE__);
		return ret;
	}

	ret = i2c_add_driver(&sii9136_tpi_i2c_driver);
	if (ret < 0) {
		pr_err("sii9136:%s:%d: [ERROR] hdmi tx tpi i2c driver init failed", __func__, __LINE__);
		return ret;
	}
#else
        ret = i2c_add_driver(&sii9136_i2c_driver);
        if (ret < 0) {
                pr_err("[ERROR] sii9136: sii9136_hdmi i2c driver init failed");
                return ret;
        }
#endif


	return 0;
}

static void __exit sii9136_exit(void)
{
	if (hdmi_wq)
		destroy_workqueue(hdmi_wq);

#ifdef LATTICE_ORIGINAL
	i2c_del_driver(&sii9136_tpi_i2c_driver);
	i2c_del_driver(&sii9136_cpi_i2c_driver);
	i2c_del_driver(&sii9136_edid_i2c_driver);
	i2c_del_driver(&sii9136_edid_seg_i2c_driver);
#else
	i2c_del_driver(&sii9136_i2c_driver);
#endif
}

module_init(sii9136_init);
module_exit(sii9136_exit);

MODULE_DESCRIPTION("HDMI Transmitter SiI9136 driver");
MODULE_AUTHOR("LATTICE Semiconductor <http://www.latticesemi.com>");
MODULE_LICENSE("GPL");
