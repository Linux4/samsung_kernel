#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/sysctl.h>
#include <linux/workqueue.h>
#include <linux/hwmon-sysfs.h>
#include <linux/gpio.h>
#include <linux/time.h>
#include <linux/firmware.h>
#include <linux/of_gpio.h>
#include <linux/of_device.h>

#include <linux/init.h>
#include <linux/ioctl.h>
#include <linux/fs.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/errno.h>
#include <linux/compat.h>
#include <asm/uaccess.h>
#include <linux/debugfs.h>
#include <linux/regulator/consumer.h>

#include <linux/spi/fpga_i2c_expander.h>

#define FPGA_I2C_EXPANDER_NAME "i2c_expander"

/*
 * This supports access to SPI devices using normal userspace I/O calls.
 * Note that while traditional UNIX/POSIX I/O semantics are half duplex,
 * and often mask message boundaries, full SPI support requires full duplex
 * transfers.  There are several kinds of internal message boundaries to
 * handle chipselect management and other protocol options.
 *
 * SPI has a character major number assigned.  We allocate minor numbers
 * dynamically using a bitmask.  You must use hotplug tools, such as udev
 * (or mdev with busybox) to create and destroy the /dev/spidevB.C device
 * nodes, since there is no fixed association of minor numbers with any
 * particular SPI bus or device.
 */
#define SPIDEV_MAJOR		154	/* assigned */
#define N_SPI_MINORS		32	/* ... up to 256 */
static DECLARE_BITMAP(minors, N_SPI_MINORS);

/* Bit masks for spi_device.mode management.  Note that incorrect
 * settings for some settings can cause *lots* of trouble for other
 * devices on a shared bus:
 *
 *  - CS_HIGH ... this device will be active when it shouldn't be
 *  - 3WIRE ... when active, it won't behave as it should
 *  - NO_CS ... there will be no explicit message boundaries; this
 *	is completely incompatible with the shared bus model
 *  - READY ... transfers may proceed when they shouldn't.
 *
 * REVISIT should changing those flags be privileged?
 */
#define SPI_MODE_MASK	\
		(SPI_CPHA | SPI_CPOL | SPI_CS_HIGH \
		| SPI_LSB_FIRST | SPI_3WIRE | SPI_LOOP \
		| SPI_NO_CS | SPI_READY)

static struct fpga_i2c_expander_platform_data fpga_i2c_expander_pdata = 
{
	.fpga_gpio_creset = GPIO_CRESET,
	.fpga_gpio_spi_cs = GPIO_CS,
};

static struct class *fpga_i2c_expander_class;
static uint8_t  tx_header_100_dummy_clocks[13] = { 0 };    // 100 clocks  ~= 13 dummy bytes transfered//msd cmtd

struct fpga_i2c_expander *g_fpga_i2c_expander;

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);


unsigned bufsiz = 9000;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

ssize_t fpga_i2c_expander_write_func(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int retval;
	int temp_devnum, temp_slaveaddr, temp_reg, temp_len,temp_val, temp_reg_len;
	uint8_t temp_buf[10] = {0};

	retval = sscanf(buf, "%x %x %x %x %x %x", &temp_devnum, &temp_slaveaddr, &temp_reg, &temp_len, &temp_val, &temp_reg_len);
	if (retval == 0) {
		pr_info("[FPGA]: %s, fail to get value\n", __func__);
		return count;
	}

	temp_buf[0] = temp_val;

	if(temp_reg_len == 1)
		fpga_i2c_exp_write(temp_devnum, temp_slaveaddr, temp_reg, 1, temp_buf, temp_len);
	else if(temp_reg_len == 2)
		fpga_i2c_exp_write(temp_devnum, temp_slaveaddr, temp_reg, 2, temp_buf, temp_len);
	pr_info("[FPGA]: %s dev num %x, slave addr %x, reg %x, len %x  val %x\n", __func__,
			temp_devnum, temp_slaveaddr, temp_reg, temp_len, temp_buf[0]);

	return count;
}

ssize_t fpga_i2c_expander_read_func(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int retval;
	int temp_devnum, temp_slaveaddr, temp_reg, temp_len, temp_reg_len;
	int index;
	uint8_t temp_buf[256] = {0};

	retval = sscanf(buf, "%x %x %x %x %x", &temp_devnum, &temp_slaveaddr, &temp_reg, &temp_len, &temp_reg_len);
	if (retval == 0) {
		pr_info("[FPGA]: %s, fail to get value\n", __func__);
		return count;
	}

	if(temp_reg_len == 1) {
		fpga_i2c_exp_read(temp_devnum, temp_slaveaddr, temp_reg, 1, temp_buf, temp_len);
	}
	else if(temp_reg_len == 2) {
		fpga_i2c_exp_read(temp_devnum, temp_slaveaddr, temp_reg, 2, temp_buf, temp_len);
	}
	pr_info("[FPGA]: %s devnum %x, slaveaddr %x, reg %x, len %x\n", __func__,
							    temp_devnum, temp_slaveaddr, temp_reg, temp_len);

	for(index = 1; index < temp_len + 1; index++)
		pr_info("\ndata received from fpga is %02x", temp_buf[index]);

	return count;
}

ssize_t fpga_i2c_expander_write_read_func(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int status = -EINVAL;

	printk(KERN_EMERG "Pbobing..i2c_write and read called using sysfs....\n");

	return status;
}

ssize_t fpga_i2c_expander_write_write_func(struct device *dev, struct device_attribute *attr,const char *buf, size_t count)
{
	int status = -EINVAL;

	printk(KERN_EMERG "Pbobing..i2c_write and write called using sysfs....\n");

	return status;
}

static DEVICE_ATTR(i2c_write, 0220, NULL, fpga_i2c_expander_write_func);
static DEVICE_ATTR(i2c_read, 0220, NULL, fpga_i2c_expander_read_func);
static DEVICE_ATTR(i2c_write_read, 0220, NULL, fpga_i2c_expander_write_read_func);
static DEVICE_ATTR(i2c_write_write, 0220, NULL, fpga_i2c_expander_write_write_func);

static struct attribute *fpga_i2c_expander_attributes[] = 
{
	&dev_attr_i2c_write.attr,
	&dev_attr_i2c_read.attr,
	&dev_attr_i2c_write_read.attr,
	&dev_attr_i2c_write_write.attr,
	NULL
};

static const struct attribute_group fpga_i2c_expander_group_attr  = 
{
	.attrs = fpga_i2c_expander_attributes,
};

static long fpga_i2c_expander_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long fpga_i2c_expander_compat_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	return fpga_i2c_expander_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
}
#else
#define spidev_compat_ioctl NULL
#endif /* CONFIG_COMPAT */

/*-------------------------------------------------------------------------*/

/*
 * We can't use the standard synchronous wrappers for file I/O; we
 * need to protect against async removal of the underlying spi_device.
 */

/*-------------------------------------------------------------------------*/

/* Read-only message with current device setup */
ssize_t fpga_i2c_expander_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	struct fpga_i2c_expander *fpga_i2c_expander;
	ssize_t			err = 0;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	fpga_i2c_expander = filp->private_data;

	mutex_lock(&fpga_i2c_expander->buf_lock);
	err = fpga_i2c_exp_rx_data(fpga_i2c_expander, NULL, fpga_i2c_expander->buffer, count);
	if (err > 0) {
		unsigned long	missing;

		missing = copy_to_user(buf, fpga_i2c_expander->buffer, err);
		if (missing == err)
			err = -EFAULT;
		else
			err = err - missing;
	}
	mutex_unlock(&fpga_i2c_expander->buf_lock);

	return err;
}

/* Write-only message with current device setup */
ssize_t fpga_i2c_expander_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	struct fpga_i2c_expander *fpga_i2c_expander;
	ssize_t			err = 0;
	unsigned long	missing;

	/* chipselect only toggles at start or end of operation */
	if (count > bufsiz)
		return -EMSGSIZE;

	fpga_i2c_expander = filp->private_data;

	mutex_lock(&fpga_i2c_expander->buf_lock);
	missing = copy_from_user(fpga_i2c_expander->buffer, buf, count);
	if (missing == 0) {
		fpga_i2c_exp_tx_data(fpga_i2c_expander,fpga_i2c_expander->buffer,count);
	} else
		err = -EFAULT;
	mutex_unlock(&fpga_i2c_expander->buf_lock);

	return err;
}


static int fpga_i2c_expander_open(struct inode *inode, struct file *filp)
{
	struct fpga_i2c_expander *fpga_i2c_expander;
	int			err = -ENXIO;

	mutex_lock(&device_list_lock);

	list_for_each_entry(fpga_i2c_expander, &device_list, device_entry) {
		if (fpga_i2c_expander->devt == inode->i_rdev) {
			err = 0;
			break;
		}
	}
	if (err == 0) {
		dev_err(&fpga_i2c_expander->spi_client->dev, "spidev_open called\n");
		if (!fpga_i2c_expander->buffer) {
			fpga_i2c_expander->buffer = kmalloc(bufsiz, GFP_KERNEL);
			if (!fpga_i2c_expander->buffer) {
				dev_dbg(&fpga_i2c_expander->spi_client->dev, "open/ENOMEM\n");
				err = -ENOMEM;
			}
		}
		if (!fpga_i2c_expander->bufferrx) {
			fpga_i2c_expander->bufferrx = kmalloc(bufsiz, GFP_KERNEL);
			if (!fpga_i2c_expander->bufferrx) {
				dev_dbg(&fpga_i2c_expander->spi_client->dev, "open/ENOMEM\n");
				kfree(fpga_i2c_expander->buffer);
				fpga_i2c_expander->buffer = NULL;
				err = -ENOMEM;
			}
		}
		if (err == 0) {
			fpga_i2c_expander->users++;
			filp->private_data = fpga_i2c_expander;
			nonseekable_open(inode, filp);
		}
	} else
		pr_debug("fpga_i2c_expander: nothing for minor %d\n", iminor(inode));

	mutex_unlock(&device_list_lock);

	return err;
}

static int fpga_i2c_expander_release(struct inode *inode, struct file *filp)
{
	struct fpga_i2c_expander *fpga_i2c_expander;
	int			err = 0;

	mutex_lock(&device_list_lock);
	fpga_i2c_expander = filp->private_data;
	filp->private_data = NULL;

	/* last close? */
	fpga_i2c_expander->users--;
	if (!fpga_i2c_expander->users) {
		kfree(fpga_i2c_expander->buffer);
		fpga_i2c_expander->buffer = NULL;
		kfree(fpga_i2c_expander->bufferrx);
		fpga_i2c_expander->bufferrx = NULL;
	}
	mutex_unlock(&device_list_lock);

	return err;
}

static const struct file_operations fpga_i2c_expander_fops = 
{
	.owner =	THIS_MODULE,
	/* REVISIT switch to aio primitives, so that userspace
	* gets more complete API coverage.  It'll simplify things
	* too, except for the locking.
	*/
	.write =	fpga_i2c_expander_write,
	.read =		fpga_i2c_expander_read,
	.unlocked_ioctl = fpga_i2c_expander_ioctl,
	.compat_ioctl = fpga_i2c_expander_compat_ioctl,
	.open =		fpga_i2c_expander_open,
	.release =	fpga_i2c_expander_release,
	.llseek =	no_llseek,
};


int fpga_i2c_expander_config(struct fpga_i2c_expander *fpga_i2c_expander,char *bitstream)
{
	int rc = 0;
	struct spi_device *spi_client = fpga_i2c_expander->spi_client;
	uint8_t config_fw[5];
	uint8_t *image_data = NULL;

	const struct firmware *fw = NULL;
	const uint8_t *sensorHubImage = NULL;
	size_t imageSize = 0;

	struct pinctrl *cs_pinctrl;

	pr_info("[FPGA] %s ++ \n", __func__);
	fpga_i2c_expander->fw_file_name = bitstream;//assigning bitstream which we got from userapp

	if (fpga_i2c_expander->fw_file_name != NULL) {
		dev_err(&spi_client->dev,"requesting Firmware %s",fpga_i2c_expander->fw_file_name);

		pr_info("[FPGA] %s request_firmware ++ \n", __func__);
		rc = request_firmware(&fw,fpga_i2c_expander->fw_file_name,&spi_client->dev);
		pr_info("[FPGA] %s request firmware -- \n", __func__);
		if (rc) {
			dev_err(&spi_client->dev,
					"qot load microcode data  from %s  error %d",
					fpga_i2c_expander->fw_file_name, rc);
			pr_info("[FPGA]%s : firmware load fail\n",__func__);
			return rc;
		}
		dev_err(&spi_client->dev, "Firmware read size %d",(int)(fw->size));
		sensorHubImage = fw->data;
		imageSize = fw->size;
	}

	if (!fpga_i2c_expander->is_gpio_allocated) {
		rc = gpio_request(fpga_i2c_expander->pdata->fpga_gpio_creset, "C_RST");
		if (rc) {
			pr_err("%s: unable to request gpio %d (%d)\n",
					__func__, fpga_i2c_expander->pdata->fpga_gpio_creset, rc);
			return rc;
		}

		rc = gpio_direction_output(fpga_i2c_expander->pdata->fpga_gpio_creset, 1);
		if (rc) {
			pr_err("%s: unable to set direction output gpio %d (%d)\n",
					__func__, fpga_i2c_expander->pdata->fpga_gpio_creset, rc);
			return rc;
		}
		pr_info("[FPGA],%s : reset %d\n", __func__,  gpio_get_value (fpga_i2c_expander->pdata->fpga_gpio_creset));

		rc = gpio_request(fpga_i2c_expander->pdata->fpga_gpio_spi_cs, "CS");
		if (rc) {
			pr_err("%s: unable to request gpio %d (%d)\n",
					__func__, fpga_i2c_expander->pdata->fpga_gpio_spi_cs, rc);
			return rc;
		}


		rc = gpio_direction_output(fpga_i2c_expander->pdata->fpga_gpio_spi_cs, 1);
		if (rc) {
			pr_err("%s: unable to direction output gpio %d (%d)\n",
					__func__, fpga_i2c_expander->pdata->fpga_gpio_spi_cs, rc);
			return rc;
		}
		pr_info("[FPGA],%s : cs %d\n", __func__,  gpio_get_value (fpga_i2c_expander->pdata->fpga_gpio_spi_cs));	

		fpga_i2c_expander->is_gpio_allocated = 1;
	}

	pr_info("[FPGA]%s : after gpio allocate\n",__func__);

	if (fpga_i2c_expander->fw_file_name != NULL) {
		/* 
		* Extra bytes appended to the data to generate extra 100 clocks after transfering actual data
		*/
		image_data =  (uint8_t *)kzalloc(imageSize + sizeof(tx_header_100_dummy_clocks), GFP_KERNEL);
		if (image_data == NULL) {
			return -ENOMEM;
		}

		memcpy(image_data, sensorHubImage, imageSize);
		memset(image_data + imageSize, 0, sizeof(tx_header_100_dummy_clocks));  /* 100 clocks  ~= 13 dummy bytes transfered */

		gpio_set_value(fpga_i2c_expander->pdata->fpga_gpio_spi_cs, 0);		        
		pr_info("[FPGA],%s : cs %d\n", __func__,  gpio_get_value (fpga_i2c_expander->pdata->fpga_gpio_spi_cs));

		gpio_set_value (fpga_i2c_expander->pdata->fpga_gpio_creset, 0);
		pr_info("[FPGA],%s : reset %d\n", __func__,  gpio_get_value (fpga_i2c_expander->pdata->fpga_gpio_creset));
		udelay (1000);

		gpio_set_value (fpga_i2c_expander->pdata->fpga_gpio_creset, 1);
		pr_info("[FPGA],%s : reset%d\n", __func__,  gpio_get_value (fpga_i2c_expander->pdata->fpga_gpio_creset));
		udelay (1000);	/* Wait for min 1000 microsec to clear internal configuration memory  */

		fpga_i2c_exp_tx_data(fpga_i2c_expander, image_data, imageSize + sizeof(tx_header_100_dummy_clocks));
		gpio_set_value (fpga_i2c_expander->pdata->fpga_gpio_spi_cs, 1);    
		pr_info("[FPGA],%s : cs %d\n", __func__,  gpio_get_value (fpga_i2c_expander->pdata->fpga_gpio_spi_cs));

		cs_pinctrl = devm_pinctrl_get_select(&spi_client->dev, "spi_5_cs_active");
		if (IS_ERR(cs_pinctrl)) {
			pr_err("[FPGA]: pin select error \n");
		}

		/*delay 1ms : to settle down FPGA*/
		mdelay(1);

		/*Reset Sequence is added*/
		config_fw[0] = 0x00;
		config_fw[1] = 0x00;
		config_fw[2] = 0xFF;
		config_fw[3] = 0xFF;
		config_fw[4] = 0x80;
		fpga_i2c_exp_tx_data(fpga_i2c_expander, config_fw, sizeof(config_fw));
		memset(config_fw, 0, sizeof(config_fw));

		config_fw[0] = 0x08;
		config_fw[1] = 0x00;
		config_fw[2] = 0xFF;
		config_fw[3] = 0xFF;
		config_fw[4] = 0x80;
		fpga_i2c_exp_tx_data(fpga_i2c_expander, config_fw, sizeof(config_fw));
		memset(config_fw, 0, sizeof(config_fw));

		config_fw[0] = 0x10;
		config_fw[1] = 0x00;
		config_fw[2] = 0xFF;
		config_fw[3] = 0xFF;
		config_fw[4] = 0x80;
		fpga_i2c_exp_tx_data(fpga_i2c_expander, config_fw, sizeof(config_fw));
		memset(config_fw, 0, sizeof(config_fw));

		kfree (image_data);
		release_firmware(fw);
		udelay (1000);	/* Wait for min 1000 microsec to clear internal configuration memory  */
	}

	fpga_i2c_expander->is_configured = 1;

	return rc;
}

int fpga_i2c_expander_read_fw_version(struct fpga_i2c_expander *fpga_i2c_expander)
{
	int ret = 0;
	struct spi_list_s *i2c_cmd;

	i2c_cmd = fpga_i2c_expander->spi_cmd[0];

	mutex_lock(&i2c_cmd->device_lock);
	i2c_cmd->cmds->ret = 0;

	memset(&i2c_cmd->cmds->data_tx0, 0x00, 300);
	memset(&i2c_cmd->cmds->data_tx1, 0x00, 20);
	memset(&i2c_cmd->cmds->data_tx2, 0x00, 20);
	memset(&i2c_cmd->cmds->data_tx3, 0x00, 20);
	memset(&i2c_cmd->cmds->data_rx0, 0x00, 2040);
	memset(&i2c_cmd->cmds->data_rx1, 0x00, 10);

	i2c_cmd->cmds->data_tx0[0] = 0x87;
	i2c_cmd->cmds->tx_length0 = 1;
	i2c_cmd->cmds->rx_length0 = 1;
	i2c_cmd->cmds->op_code = DEVICE_I2C_RD;
	fpga_work(fpga_i2c_expander, i2c_cmd);

	if(i2c_cmd->cmds->ret !=0){
		ret = i2c_cmd->cmds->ret;   //SPI return value check..
		mutex_unlock(&i2c_cmd->device_lock);
		pr_err("%s(%d): SPI Error :%d\n", __func__, __LINE__, ret);
		return ret;
	}

	if(i2c_cmd->cmds->data_rx0[1] != FPGA_FW_VERSION ) {
		pr_err("%s(%d): FPGA_FW_VERSION is wrong.  original value : 0x%x , wrong value : 0x%x \n", __func__, __LINE__, FPGA_FW_VERSION, i2c_cmd->cmds->data_rx0[1]);
		ret = FW_VERSION_MISMATCH;
	} else
		pr_info("[FPGA] FPGA_FW_VERSION is 0x%x\n", i2c_cmd->cmds->data_rx0[1]);

	mutex_unlock(&i2c_cmd->device_lock);
	return ret;
}

static int fpga_i2c_expander_parse_dt(struct fpga_i2c_expander *fpga_i2c_expander)
{
	struct device_node *np = fpga_i2c_expander->spi_client->dev.of_node;
	int ret;

	fpga_i2c_expander->pdata->fpga_gpio_creset = of_get_named_gpio(np, "fpga,gpio_reset", 0);
	if (!gpio_is_valid(fpga_i2c_expander->pdata->fpga_gpio_creset)) {
		pr_err("%s:%d, reset gpio not specified\n", __func__, __LINE__);
		return -1;
	}
	pr_info("[FPGA] %s: reset : %d \n", __func__, fpga_i2c_expander->pdata->fpga_gpio_creset);

	fpga_i2c_expander->pdata->fpga_gpio_spi_cs = of_get_named_gpio(np, "fpga,gpio_cs", 0);
	if (!gpio_is_valid(fpga_i2c_expander->pdata->fpga_gpio_spi_cs)) {
		pr_err("%s:%d, cs gpio not specified\n", __func__, __LINE__);
		return -1;
	}
	pr_info("[FPGA] %s: cs : %d \n", __func__, fpga_i2c_expander->pdata->fpga_gpio_spi_cs);

	fpga_i2c_expander->vdd = devm_regulator_get(&fpga_i2c_expander->spi_client->dev, "fpga,vdd");
		if (IS_ERR(fpga_i2c_expander->vdd)) {
			pr_err("%s: cannot get fpga vdd\n", __func__);
			fpga_i2c_expander->vdd = NULL;
		} else if (!regulator_get_voltage(fpga_i2c_expander->vdd)) {
			ret = regulator_set_voltage(fpga_i2c_expander->vdd, 1200000, 1200000);
			if (ret < 0) {
				pr_err("regulator set voltage failed, %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int fpga_i2c_expander_probe(struct spi_device *spi_client) 
{
	unsigned long		minor;
	int err = 0;
	int i;
	int retry_count = 0;

	struct fpga_i2c_expander *fpga_i2c_expander = NULL;

	pr_info("[FPGA] %s ++ \n", __func__);

	dev_err(&spi_client->dev, "fpga_i2c_expander_probe!!!!!!!!!!!\n");

	fpga_i2c_expander = kzalloc(sizeof(struct fpga_i2c_expander), GFP_KERNEL);
	if (!fpga_i2c_expander) {
		dev_dbg(&spi_client->dev, "unable to allocate memory\n");
		err = -ENOMEM;
		goto exit;
	}

	fpga_i2c_expander->spi_client = spi_client;

	spi_client->dev.platform_data = &fpga_i2c_expander_pdata;	/* MDN Addition */
	fpga_i2c_expander->pdata = spi_client->dev.platform_data;
	if (!fpga_i2c_expander->pdata) {
		dev_dbg(&spi_client->dev, "No platform data - aborting\n");
		err = -EINVAL;
		goto exit;
	}

	err = fpga_i2c_expander_parse_dt(fpga_i2c_expander);
	if(err) {
		goto mem_cleanup;
		return err;
	}

	if(fpga_i2c_expander->vdd != NULL) {
		err = regulator_enable(fpga_i2c_expander->vdd);
		if (err) {
			printk(KERN_ERR"enable ldo failed, rc=%d\n", err);
			return err;
		}
	}

	fpga_i2c_expander->spi_bus_num = spi_client->master->bus_num;

	/* Configure the SPI bus */
	/* not setting SPI_CS_HIGH SPI_NO_CS SPI_LSB_FIRST SPI_3WIRE SPI_READY */
	/* so it is MSB out first, CS active low, not 3 wire mode, no SPI ready support */ 

//	spi_client->mode = (SPI_MODE_3); //spi mode we can set here but now we set mode 0 on fpga_i2c_expander_tx_data_msd() and fpga_i2c_expander_rx_data_msd funcs..msd
	spi_client->mode = SPI_MODE_0; //spi mode we can set here but now we set mode 0 on 

	spi_client->bits_per_word = 8;
	spi_setup(spi_client);

	fpga_i2c_expander->spi_mem = kzalloc(sizeof(struct spi_xfer_mem), GFP_KERNEL);
	if (!fpga_i2c_expander->spi_mem){
		 err = -ENOMEM;
		goto mem_cleanup;
	}

	for (i = 0; i < I2C_MAX_CH_NUM; i++) {
		fpga_i2c_expander->spi_cmd[i] = kzalloc(sizeof(struct spi_list_s), GFP_KERNEL);
		fpga_i2c_expander->spi_cmd[i]->cmds = kzalloc(sizeof(struct spi_cmd_s), GFP_KERNEL);
		if(!fpga_i2c_expander->spi_cmd[i] || !fpga_i2c_expander->spi_cmd[i]->cmds) {
			err = -ENOMEM;
			goto mem_cleanup;
		}
		mutex_init(&fpga_i2c_expander->spi_cmd[i]->device_lock);
	}

	INIT_LIST_HEAD(&fpga_i2c_expander->spi_cmd_queue);
	INIT_WORK(&fpga_i2c_expander->spi_cmd_func, fpga_spi_queue_handle);
	mutex_init(&fpga_i2c_expander->spi_cmd_lock);

	spi_set_drvdata(spi_client, fpga_i2c_expander);

	g_fpga_i2c_expander = fpga_i2c_expander;

	mutex_init(&fpga_i2c_expander->mutex);

	err = sysfs_create_group(&spi_client->dev.kobj, &fpga_i2c_expander_group_attr);
	if (err)
		goto err_remove_files;

	spin_lock_init(&fpga_i2c_expander->spi_lock);
	mutex_init(&fpga_i2c_expander->buf_lock);

	INIT_LIST_HEAD(&fpga_i2c_expander->device_entry);

	BUILD_BUG_ON(N_SPI_MINORS > 256);

	err = register_chrdev(SPIDEV_MAJOR, "fpga_i2c", &fpga_i2c_expander_fops);
	if (err < 0) {
		dev_err(&spi_client->dev, "register_chrdev spi failed!\n");
		goto err_remove_files;
	}

	err = 0;
	fpga_i2c_expander_class = class_create(THIS_MODULE, "fpga_class");
	if (IS_ERR(fpga_i2c_expander_class)) {
		dev_err(&spi_client->dev, "class_create spidev failed!\n");
		err = PTR_ERR(fpga_i2c_expander_class);
		goto err_remove_spidev_chrdev;
	}

	err = 0;
	/* If we can allocate a minor number, hook up this device.
	* Reusing minors is fine so long as udev or mdev is working.
	*/
	mutex_lock(&device_list_lock);
	minor = find_first_zero_bit(minors, N_SPI_MINORS);
	if (minor < N_SPI_MINORS) {
		struct device *dev;

		fpga_i2c_expander->devt = MKDEV(SPIDEV_MAJOR, minor);
		dev = device_create(fpga_i2c_expander_class, &spi_client->dev, fpga_i2c_expander->devt,
							fpga_i2c_expander, "fpga_spi");

		if (IS_ERR(dev)) {
			err = PTR_ERR(dev);
			dev_err(&spi_client->dev, "device_create failed!\n");
			goto err_remove_class;
		}
	} else {
		dev_err(&spi_client->dev, "no minor number available!\n");
		err = -ENODEV;
		goto err_remove_class;
	}

	set_bit(minor, minors);
	list_add(&fpga_i2c_expander->device_entry, &device_list);

	mutex_unlock(&device_list_lock);

retry_config:

	err = fpga_i2c_expander_config(fpga_i2c_expander, "fpga/spi_to_i2c_bitmap.fw");

	if (err) {
		dev_err(&spi_client->dev, "unable to configure_iCE\n");
		goto err_remove_files;
	}

	err = fpga_i2c_expander_read_fw_version(fpga_i2c_expander);

	if(err == FW_VERSION_MISMATCH) {
		if ( retry_count <= 3) {
			retry_count ++;
			goto retry_config;
		} else {
			dev_err(&spi_client->dev, "retry_count for fw_config was over 3times.\n");
			goto err_remove_files;
		}
	}

	if (err) {
		goto err_remove_files;
	}

	pr_info("[FPGA] %s -- \n", __func__);

	udelay(1000);
	goto exit;

err_remove_class:
	mutex_unlock(&device_list_lock);
	class_destroy(fpga_i2c_expander_class);

err_remove_spidev_chrdev:
	unregister_chrdev(SPIDEV_MAJOR, FPGA_I2C_EXPANDER_NAME);

err_remove_files:
	sysfs_remove_group(&spi_client->dev.kobj, &fpga_i2c_expander_group_attr); 

mem_cleanup:
	if (fpga_i2c_expander) {
		kfree(fpga_i2c_expander);
		fpga_i2c_expander = NULL;
	}
	
exit:
	return err;
}

static int fpga_i2c_expander_remove(struct spi_device *spi_client) 
{
	struct fpga_i2c_expander *fpga_i2c_expander = spi_get_drvdata(spi_client);

	sysfs_remove_group(&spi_client->dev.kobj, &fpga_i2c_expander_group_attr); 
	unregister_chrdev(SPIDEV_MAJOR, FPGA_I2C_EXPANDER_NAME);
	list_del(&fpga_i2c_expander->device_entry);
	device_destroy(fpga_i2c_expander_class, fpga_i2c_expander->devt);
	clear_bit(MINOR(fpga_i2c_expander->devt), minors);
	class_destroy(fpga_i2c_expander_class);
	unregister_chrdev(SPIDEV_MAJOR, FPGA_I2C_EXPANDER_NAME);
	spi_set_drvdata(spi_client, NULL);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	if (fpga_i2c_expander->users == 0)
		kfree(fpga_i2c_expander);
	mutex_unlock(&device_list_lock);

	return 0;
}

static int fpga_i2c_expander_resume(struct device *dev) 
{
	struct fpga_i2c_expander *fpga_i2c_expander = dev_get_drvdata(dev);
	uint8_t fpga_resume[5] = {0x38, 0x00, 0xff, 0xff, 0x01};

	pr_info("[FPGA],%s\n", __func__);

	fpga_i2c_exp_tx_data(fpga_i2c_expander, fpga_resume, sizeof(fpga_resume));
	
	return 0;
}

static int fpga_i2c_expander_suspend(struct device *dev) 
{
	struct fpga_i2c_expander *fpga_i2c_expander = dev_get_drvdata(dev);
	uint8_t fpga_suspend[5] = {0x38, 0x00, 0xff, 0xff, 0x00};

	pr_info("[FPGA],%s\n", __func__);
	
	fpga_i2c_exp_tx_data(fpga_i2c_expander, fpga_suspend, sizeof(fpga_suspend));

	return 0;
}

static SIMPLE_DEV_PM_OPS(fpga_i2c_expander_pm_ops, fpga_i2c_expander_suspend,                                                    
                         fpga_i2c_expander_resume);                                                                      
                                           
static struct of_device_id fpga_i2c_expander_of_table[] = 
{
	{ .compatible = "fpga_i2c_expander" },
	{ },
};
MODULE_DEVICE_TABLE(spi, fpga_i2c_expander_of_table);

static struct spi_driver fpga_i2c_expander_driver = 
{
	.driver = { 
		.name = "i2c_exp",
		.of_match_table = fpga_i2c_expander_of_table,
                .pm = &fpga_i2c_expander_pm_ops,
	},
	.probe = fpga_i2c_expander_probe,
	.remove = fpga_i2c_expander_remove,
};

static int __init fpga_i2c_expander_init(void)
{
	int ret = 0;

	pr_info("[FPGA] %s \n", __func__);
	ret = spi_register_driver(&fpga_i2c_expander_driver);
	if(ret < 0) {
		pr_info("[FPGA] spi register failed: %d\n", ret);
		return ret;
	}
	pr_info("[FPGA] %s done\n", __func__);
	return ret;
}

static void __exit fpga_i2c_expander_exit(void)
{
	spi_unregister_driver(&fpga_i2c_expander_driver);
}

module_init(fpga_i2c_expander_init);
module_exit(fpga_i2c_expander_exit);

MODULE_DESCRIPTION("FPGA I2C Expander Driver");
MODULE_LICENSE("GPL");

