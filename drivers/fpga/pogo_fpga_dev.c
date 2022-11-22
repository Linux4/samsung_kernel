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

#include <linux/fpga/pogo_fpga.h>

#define POGO_FPGA_NAME "pogo_expander"

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

static struct pogo_fpga_platform_data pogo_fpga_pdata = 
{
	.fpga_cdone = 8,
	.fpga_pogo_ldo_en = 31,
	.fpga_gpio_reset = 85,
	.fpga_gpio_crst_b = GPIO_CRST_B,
	.fpga_gpio_spi_cs = GPIO_CS,
};

static struct class *pogo_fpga_class;
static uint8_t  tx_header_100_dummy_clocks[13] = { 0 };    // 100 clocks  ~= 13 dummy bytes transfered//msd cmtd

struct pogo_fpga *g_pogo_fpga;

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);


unsigned bufsiz = 9000;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

int pogo_fpga_rx_data(
	struct pogo_fpga *pogo_fpga,
	char *txbuf,
	char *rxbuf,
	int len)
{
	int err;
	u8 mode;
	struct spi_device *spi;
	struct spi_message msg;
	struct spi_transfer xfer = {
		.tx_buf = txbuf,
		.rx_buf = rxbuf,
		.len = len,
		.cs_change = 0,
		.speed_hz=SPI_CLK_FREQ,
	};

	mode = 0;	//spi mode3 is set in probe funcion but here we set for spi mode0//if we want spi_mode_3 we can commnet these 3 lines
	spi = spi_dev_get(pogo_fpga->spi_client);
	spi->mode=mode;

//	gpio_set_value (pogo_fpga->pdata->fpgaGpioSpiCs, 0);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	err = spi_sync(pogo_fpga->spi_client, &msg);

//	gpio_set_value (pogo_fpga->pdata->fpgaGpioSpiCs, 1);

	return err;
}

int pogo_fpga_tx_data(
	struct pogo_fpga *pogo_fpga,
	char *txbuf,
	int len)
{
	int err;
	u8 mode;
	struct spi_device *spi;
	struct spi_message msg;

	struct spi_transfer xfer = {
		.tx_buf = txbuf,
		.rx_buf = NULL,
		.len = len,
		.cs_change = 0,
		.speed_hz=SPI_CLK_FREQ,
	};

	mode = 0;//spi mode3 is set in probe funcion but here we set for spi mode0//if we want spi_mode_3 we can commnet these 3 lines
	spi = spi_dev_get(pogo_fpga->spi_client);
	spi->mode=mode;

//	gpio_set_value (pogo_fpga->pdata->fpgaGpioSpiCs, 0);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	err = spi_sync(pogo_fpga->spi_client, &msg);

//	gpio_set_value (pogo_fpga->pdata->fpgaGpioSpiCs, 1);

	return err;
}

static struct attribute *pogo_fpga_attributes[] = 
{
	NULL
};

static const struct attribute_group pogo_fpga_group_attr  = 
{
	.attrs = pogo_fpga_attributes,
};

int pogo_fpga_config(struct pogo_fpga *pogo_fpga,char *bitstream)
{
	int rc = 0;
	struct spi_device *spi_client = pogo_fpga->spi_client;
	uint8_t config_fw[5];
	uint8_t *image_data = NULL;

	const struct firmware *fw = NULL;
	const uint8_t *sensorHubImage = NULL;
	size_t imageSize = 0;

	struct pinctrl *cs_pinctrl;

	pr_info("[FPGA] %s ++ \n", __func__);
	pogo_fpga->fw_file_name = bitstream;//assigning bitstream which we got from userapp

	if (pogo_fpga->fw_file_name != NULL) {
		dev_err(&spi_client->dev,"requesting Firmware %s",pogo_fpga->fw_file_name);

		pr_info("[FPGA] %s request_firmware ++ \n", __func__);
		rc = request_firmware(&fw,pogo_fpga->fw_file_name,&spi_client->dev);
		pr_info("[FPGA] %s request firmware -- \n", __func__);
		if (rc) {
			dev_err(&spi_client->dev,
					"qot load microcode data  from %s  error %d",
					pogo_fpga->fw_file_name, rc);
			pr_info("[FPGA]%s : firmware load fail\n",__func__);
			return rc;
		}
		dev_err(&spi_client->dev, "Firmware read size %d",(int)(fw->size));
		sensorHubImage = fw->data;
		imageSize = fw->size;
	}

	cs_pinctrl = devm_pinctrl_get_select(&spi_client->dev, "fpga_cdone");
	if (IS_ERR(cs_pinctrl)) {
		pr_err("[FPGA]: cdone pin select error \n");
	}

	cs_pinctrl = devm_pinctrl_get_select(&spi_client->dev, "fpga_pogo_ldo_en");
	if (IS_ERR(cs_pinctrl)) {
		pr_err("[FPGA]: pogo_ldo_en pin select error \n");
	}

	cs_pinctrl = devm_pinctrl_get_select(&spi_client->dev, "spi_12_cs_gpio");
	if (IS_ERR(cs_pinctrl)) {
		pr_err("[FPGA]: cs_gpio pin select error \n");
	}

	if (!pogo_fpga->is_gpio_allocated) {
		rc = gpio_request(pogo_fpga->pdata->fpga_cdone, "FPGA_CDONE");
		if (rc) {
			pr_err("%s: unable to request gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_cdone, rc);
			return rc;
		}

		rc = gpio_direction_input(pogo_fpga->pdata->fpga_cdone);
		if (rc) {
			pr_err("%s: unable to set direction input gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_cdone, rc);
			return rc;
		}
		pr_info("[FPGA],%s : CDONE %d\n", __func__,  gpio_get_value (pogo_fpga->pdata->fpga_cdone));

		rc = gpio_request(pogo_fpga->pdata->fpga_pogo_ldo_en, "POGO_LDO_EN");
		if (rc) {
			pr_err("%s: unable to request gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_pogo_ldo_en, rc);
			return rc;
		}

		rc = gpio_direction_output(pogo_fpga->pdata->fpga_pogo_ldo_en, 1);
		if (rc) {
			pr_err("%s: unable to set direction output gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_pogo_ldo_en, rc);
			return rc;
		}
		pr_info("[FPGA],%s : POGO_LDO_EN %d\n", __func__,  gpio_get_value (pogo_fpga->pdata->fpga_pogo_ldo_en));

		rc = gpio_request(pogo_fpga->pdata->fpga_gpio_reset, "FPGA_RESET"); 
		if (rc) {
			pr_err("%s: unable to request gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_gpio_reset, rc);
			return rc;
		}

		rc = gpio_direction_output(pogo_fpga->pdata->fpga_gpio_reset, 1);
		if (rc) {
			pr_err("%s: unable to set direction output gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_gpio_reset, rc);
			return rc;
		}
		pr_info("[FPGA],%s : reset %d\n", __func__,  gpio_get_value (pogo_fpga->pdata->fpga_gpio_reset));

		rc = gpio_request(pogo_fpga->pdata->fpga_gpio_crst_b, "FPGA_CRST");
		if (rc) {
			pr_err("%s: unable to request gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_gpio_crst_b, rc);
			return rc;
		}

		rc = gpio_direction_output(pogo_fpga->pdata->fpga_gpio_crst_b, 1);
		if (rc) {
			pr_err("%s: unable to set direction output gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_gpio_crst_b, rc);
			return rc;
		}
		pr_info("[FPGA],%s : crst_b %d\n", __func__,  gpio_get_value (pogo_fpga->pdata->fpga_gpio_crst_b));

		rc = gpio_request(pogo_fpga->pdata->fpga_gpio_spi_cs, "FPGA_CS");
		if (rc) {
			pr_err("%s: unable to request gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_gpio_spi_cs, rc);
			return rc;
		}


		rc = gpio_direction_output(pogo_fpga->pdata->fpga_gpio_spi_cs, 1);
		if (rc) {
			pr_err("%s: unable to direction output gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_gpio_spi_cs, rc);
			return rc;
		}
		pr_info("[FPGA],%s : cs %d\n", __func__,  gpio_get_value (pogo_fpga->pdata->fpga_gpio_spi_cs));	

		pogo_fpga->is_gpio_allocated = 1;
	}

	pr_info("[FPGA]%s : after gpio allocate\n",__func__);

	if (pogo_fpga->fw_file_name != NULL) {
		/* 
		* Extra bytes appended to the data to generate extra 100 clocks after transfering actual data
		*/
		image_data =  (uint8_t *)kzalloc(imageSize + sizeof(tx_header_100_dummy_clocks), GFP_KERNEL);
		if (image_data == NULL) {
			return -ENOMEM;
		}

		memcpy(image_data, sensorHubImage, imageSize);
		memset(image_data + imageSize, 0, sizeof(tx_header_100_dummy_clocks));  /* 100 clocks  ~= 13 dummy bytes transfered */

		gpio_set_value(pogo_fpga->pdata->fpga_gpio_spi_cs, 0);		        
		pr_info("[FPGA],%s : cs %d\n", __func__,  gpio_get_value (pogo_fpga->pdata->fpga_gpio_spi_cs));

		gpio_set_value (pogo_fpga->pdata->fpga_gpio_crst_b, 0);
		pr_info("[FPGA],%s : crst_b %d\n", __func__,  gpio_get_value (pogo_fpga->pdata->fpga_gpio_crst_b));
		udelay (1000);

		gpio_set_value (pogo_fpga->pdata->fpga_gpio_crst_b, 1);
		pr_info("[FPGA],%s : crst_b %d\n", __func__,  gpio_get_value (pogo_fpga->pdata->fpga_gpio_crst_b));
		udelay (1000);	/* Wait for min 1000 microsec to clear internal configuration memory  */

		pogo_fpga_tx_data(pogo_fpga, image_data, imageSize + sizeof(tx_header_100_dummy_clocks));
		gpio_set_value (pogo_fpga->pdata->fpga_gpio_spi_cs, 1);    
		pr_info("[FPGA],%s : cs %d\n", __func__,  gpio_get_value (pogo_fpga->pdata->fpga_gpio_spi_cs));

		gpio_set_value (pogo_fpga->pdata->fpga_gpio_reset, 0);
		pr_info("[FPGA],%s : reset %d\n", __func__,  gpio_get_value (pogo_fpga->pdata->fpga_gpio_reset));

		gpio_set_value (pogo_fpga->pdata->fpga_gpio_reset, 1);
		pr_info("[FPGA],%s : reset %d\n", __func__,  gpio_get_value (pogo_fpga->pdata->fpga_gpio_reset));

		cs_pinctrl = devm_pinctrl_get_select(&spi_client->dev, "spi_12_cs_active");
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
		pogo_fpga_tx_data(pogo_fpga, config_fw, sizeof(config_fw));
		memset(config_fw, 0, sizeof(config_fw));

		config_fw[0] = 0x08;
		config_fw[1] = 0x00;
		config_fw[2] = 0xFF;
		config_fw[3] = 0xFF;
		config_fw[4] = 0x80;
		pogo_fpga_tx_data(pogo_fpga, config_fw, sizeof(config_fw));
		memset(config_fw, 0, sizeof(config_fw));

		config_fw[0] = 0x10;
		config_fw[1] = 0x00;
		config_fw[2] = 0xFF;
		config_fw[3] = 0xFF;
		config_fw[4] = 0x80;
		pogo_fpga_tx_data(pogo_fpga, config_fw, sizeof(config_fw));
		memset(config_fw, 0, sizeof(config_fw));

		kfree (image_data);
		release_firmware(fw);
		udelay (1000);	/* Wait for min 1000 microsec to clear internal configuration memory  */
	}

	pogo_fpga->is_configured = 1;

	return rc;
}

int pogo_fpga_read_fw_version(struct pogo_fpga *pogo_fpga)
{
	int ret = 0;
#if 0
	struct spi_list_s *i2c_cmd;

	i2c_cmd = pogo_fpga->spi_cmd[0];

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
//	fpga_work(pogo_fpga, i2c_cmd);

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
#endif
	return ret;
}

static int pogo_fpga_parse_dt(struct pogo_fpga *pogo_fpga)
{
	struct device_node *np = pogo_fpga->spi_client->dev.of_node;
	int ret;

	pogo_fpga->pdata->fpga_cdone = of_get_named_gpio(np, "fpga,gpio_cdone", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_cdone)) {
		pr_err("%s:%d, fpga_cdone not specified\n", __func__, __LINE__);
		return -1;
	}
	pr_info("[FPGA] %s: CDONE : %d \n", __func__, pogo_fpga->pdata->fpga_cdone);

	pogo_fpga->pdata->fpga_pogo_ldo_en = of_get_named_gpio(np, "fpga,pogo_ldo_en", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_pogo_ldo_en)) {
		pr_err("%s:%d, pogo_ldo_en not specified\n", __func__, __LINE__);
		return -1;
	}
	pr_info("[FPGA] %s: LDO_EN : %d \n", __func__, pogo_fpga->pdata->fpga_pogo_ldo_en);

	pogo_fpga->pdata->fpga_gpio_reset = of_get_named_gpio(np, "fpga,gpio_reset", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_gpio_reset)) {
		pr_err("%s:%d, reset gpio not specified\n", __func__, __LINE__);
		return -1;
	}
	pr_info("[FPGA] %s: reset : %d \n", __func__, pogo_fpga->pdata->fpga_gpio_reset);

	pogo_fpga->pdata->fpga_gpio_crst_b = of_get_named_gpio(np, "fpga,gpio_crst_b", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_gpio_crst_b)) {
		pr_err("%s:%d, reset gpio not specified\n", __func__, __LINE__);
		return -1;
	}
	pr_info("[FPGA] %s: reset : %d \n", __func__, pogo_fpga->pdata->fpga_gpio_crst_b);

	pogo_fpga->pdata->fpga_gpio_spi_cs = of_get_named_gpio(np, "fpga,gpio_cs", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_gpio_spi_cs)) {
		pr_err("%s:%d, cs gpio not specified\n", __func__, __LINE__);
		return -1;
	}
	pr_info("[FPGA] %s: cs : %d \n", __func__, pogo_fpga->pdata->fpga_gpio_spi_cs);

	pogo_fpga->vdd = devm_regulator_get(&pogo_fpga->spi_client->dev, "fpga,vdd");
		if (IS_ERR(pogo_fpga->vdd)) {
			pr_err("%s: cannot get fpga vdd\n", __func__);
			pogo_fpga->vdd = NULL;
		} else if (!regulator_get_voltage(pogo_fpga->vdd)) {
			ret = regulator_set_voltage(pogo_fpga->vdd, 1200000, 1200000);
			if (ret < 0) {
				pr_err("regulator set voltage failed, %d\n", ret);
			return ret;
		}
	}

	return 0;
}

static int pogo_fpga_probe(struct spi_device *spi_client) 
{
	unsigned long		minor;
	int err = 0;
//	int retry_count = 0;

	struct pogo_fpga *pogo_fpga = NULL;

	pr_info("[FPGA] %s ++ \n", __func__);

	dev_err(&spi_client->dev, "pogo_fpga_probe!!!!!!!!!!!\n");

	pogo_fpga = kzalloc(sizeof(struct pogo_fpga), GFP_KERNEL);
	if (!pogo_fpga) {
		dev_dbg(&spi_client->dev, "unable to allocate memory\n");
		err = -ENOMEM;
		goto exit;
	}

	pogo_fpga->spi_client = spi_client;

	spi_client->dev.platform_data = &pogo_fpga_pdata;	/* MDN Addition */
	pogo_fpga->pdata = spi_client->dev.platform_data;
	if (!pogo_fpga->pdata) {
		dev_dbg(&spi_client->dev, "No platform data - aborting\n");
		err = -EINVAL;
		goto exit;
	}

	err = pogo_fpga_parse_dt(pogo_fpga);
	if(err) {
		goto mem_cleanup;
		return err;
	}

	if(pogo_fpga->vdd != NULL) {
		err = regulator_enable(pogo_fpga->vdd);
		if (err) {
			printk(KERN_ERR"enable ldo failed, rc=%d\n", err);
			return err;
		}
	}

	pogo_fpga->spi_bus_num = spi_client->master->bus_num;

	/* Configure the SPI bus */
	/* not setting SPI_CS_HIGH SPI_NO_CS SPI_LSB_FIRST SPI_3WIRE SPI_READY */
	/* so it is MSB out first, CS active low, not 3 wire mode, no SPI ready support */ 

//	spi_client->mode = (SPI_MODE_3); //spi mode we can set here but now we set mode 0 on pogo_fpga_tx_data_msd() and pogo_fpga_rx_data_msd funcs..msd
	spi_client->mode = SPI_MODE_0; //spi mode we can set here but now we set mode 0 on 

	spi_client->bits_per_word = 8;
	spi_setup(spi_client);

	spi_set_drvdata(spi_client, pogo_fpga);

	g_pogo_fpga = pogo_fpga;

	mutex_init(&pogo_fpga->mutex);

	err = sysfs_create_group(&spi_client->dev.kobj, &pogo_fpga_group_attr);
	if (err)
		goto err_remove_files;

	spin_lock_init(&pogo_fpga->spi_lock);
	mutex_init(&pogo_fpga->buf_lock);

	INIT_LIST_HEAD(&pogo_fpga->device_entry);

	BUILD_BUG_ON(N_SPI_MINORS > 256);

	err = 0;
	pogo_fpga_class = class_create(THIS_MODULE, "fpga_class");
	if (IS_ERR(pogo_fpga_class)) {
		dev_err(&spi_client->dev, "class_create spidev failed!\n");
		err = PTR_ERR(pogo_fpga_class);
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

		pogo_fpga->devt = MKDEV(SPIDEV_MAJOR, minor);
		dev = device_create(pogo_fpga_class, &spi_client->dev, pogo_fpga->devt,
							pogo_fpga, "fpga_spi");

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
	list_add(&pogo_fpga->device_entry, &device_list);

	mutex_unlock(&device_list_lock);

//retry_config:

	err = pogo_fpga_config(pogo_fpga, "fpga/pogo_fpga.fw");

	if (err) {
		dev_err(&spi_client->dev, "unable to configure_iCE\n");
		goto err_remove_files;
	}
#if 0
	err = pogo_fpga_read_fw_version(pogo_fpga);

	if(err == FW_VERSION_MISMATCH) {
		if ( retry_count <= 3) {
			retry_count ++;
			goto retry_config;
		} else {
			dev_err(&spi_client->dev, "retry_count for fw_config was over 3times.\n");
			goto err_remove_files;
		}
	}
#endif
	if (err) {
		goto err_remove_files;
	}

	pr_info("[FPGA],%s : CDONE %d\n", __func__,  gpio_get_value (pogo_fpga->pdata->fpga_cdone));
	pr_info("[FPGA] %s -- \n", __func__);

	udelay(1000);
	goto exit;

err_remove_class:
	mutex_unlock(&device_list_lock);
	class_destroy(pogo_fpga_class);

err_remove_spidev_chrdev:
	unregister_chrdev(SPIDEV_MAJOR, POGO_FPGA_NAME);

err_remove_files:
	sysfs_remove_group(&spi_client->dev.kobj, &pogo_fpga_group_attr); 

mem_cleanup:
	if (pogo_fpga) {
		kfree(pogo_fpga);
		pogo_fpga = NULL;
	}
	
exit:
	return err;
}

static int pogo_fpga_remove(struct spi_device *spi_client) 
{
	struct pogo_fpga *pogo_fpga = spi_get_drvdata(spi_client);

	sysfs_remove_group(&spi_client->dev.kobj, &pogo_fpga_group_attr); 
	unregister_chrdev(SPIDEV_MAJOR, POGO_FPGA_NAME);
	list_del(&pogo_fpga->device_entry);
	device_destroy(pogo_fpga_class, pogo_fpga->devt);
	clear_bit(MINOR(pogo_fpga->devt), minors);
	class_destroy(pogo_fpga_class);
	unregister_chrdev(SPIDEV_MAJOR, POGO_FPGA_NAME);
	spi_set_drvdata(spi_client, NULL);

	/* prevent new opens */
	mutex_lock(&device_list_lock);
	if (pogo_fpga->users == 0)
		kfree(pogo_fpga);
	mutex_unlock(&device_list_lock);

	return 0;
}

static int pogo_fpga_resume(struct device *dev) 
{
	struct pogo_fpga *pogo_fpga = dev_get_drvdata(dev);
	uint8_t fpga_resume[5] = {0x38, 0x00, 0xff, 0xff, 0x01};

	pr_info("[FPGA],%s\n", __func__);

	pogo_fpga_tx_data(pogo_fpga, fpga_resume, sizeof(fpga_resume));
	
	return 0;
}

static int pogo_fpga_suspend(struct device *dev) 
{
	struct pogo_fpga *pogo_fpga = dev_get_drvdata(dev);
	uint8_t fpga_suspend[5] = {0x38, 0x00, 0xff, 0xff, 0x00};

	pr_info("[FPGA],%s\n", __func__);
	
	pogo_fpga_tx_data(pogo_fpga, fpga_suspend, sizeof(fpga_suspend));

	return 0;
}

static SIMPLE_DEV_PM_OPS(pogo_fpga_pm_ops, pogo_fpga_suspend,                                                    
                         pogo_fpga_resume);                                                                      
                                           
static struct of_device_id pogo_fpga_of_table[] = 
{
	{ .compatible = "pogo_fpga" },
	{ },
};
MODULE_DEVICE_TABLE(spi, pogo_fpga_of_table);

static struct spi_driver pogo_fpga_driver = 
{
	.driver = { 
		.name = "pogo_fpga",
		.of_match_table = pogo_fpga_of_table,
                .pm = &pogo_fpga_pm_ops,
	},
	.probe = pogo_fpga_probe,
	.remove = pogo_fpga_remove,
};

static int __init pogo_fpga_init(void)
{
	int ret = 0;

	pr_info("[FPGA] %s \n", __func__);
	ret = spi_register_driver(&pogo_fpga_driver);
	if(ret < 0) {
		pr_info("[FPGA] spi register failed: %d\n", ret);
		return ret;
	}
	pr_info("[FPGA] %s done\n", __func__);
	return ret;
}

static void __exit pogo_fpga_exit(void)
{
	spi_unregister_driver(&pogo_fpga_driver);
}

module_init(pogo_fpga_init);
module_exit(pogo_fpga_exit);

MODULE_DESCRIPTION("FPGA I2C Expander Driver");
MODULE_LICENSE("GPL");

