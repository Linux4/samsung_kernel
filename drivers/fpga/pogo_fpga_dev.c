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

#define TX_HEADER_100_DUMMY_CLOCK_SIZE	13

static struct pogo_fpga_platform_data pogo_fpga_pdata = 
{
	.fpga_cdone = 0,
	.fpga_pogo_ldo_en = 0,
	.fpga_gpio_reset = 0,
	.fpga_gpio_crst_b = 0,
	.fpga_gpio_spi_cs = 0,
};

typedef enum {
	FPGA_LPM_SUSPEND = 0,
	FPGA_LPM_RESUME,
} fpga_lpm_state_t;

static struct class *pogo_fpga_class;

struct pogo_fpga *g_pogo_fpga;

static LIST_HEAD(device_list);
static DEFINE_MUTEX(device_list_lock);


unsigned bufsiz = 9000;
module_param(bufsiz, uint, S_IRUGO);
MODULE_PARM_DESC(bufsiz, "data bytes in biggest supported SPI message");

int pogo_fpga_config(struct pogo_fpga *pogo_fpga);

bool is_fpga_wakeup(void)
{
	int miso, mosi, cs_n, spi_clk;

	if (!g_pogo_fpga)
		return false;

	miso = gpio_get_value(g_pogo_fpga->pdata->fpga_gpio_spi_miso);
	mosi = gpio_get_value(g_pogo_fpga->pdata->fpga_gpio_spi_mosi);
	spi_clk = gpio_get_value(g_pogo_fpga->pdata->fpga_gpio_spi_clk);
	cs_n = gpio_get_value(g_pogo_fpga->pdata->fpga_gpio_spi_cs);

	dev_info(&g_pogo_fpga->spi_client->dev, "%s: miso:%d, mosi:%d, clk:%d, cs:%d\n",
			__func__, miso, mosi, spi_clk, cs_n);

	if (miso == 0 && mosi == 1 && spi_clk == 1 && cs_n == 0)
		return true;

	return false;
}
EXPORT_SYMBOL(is_fpga_wakeup);

void fpga_fw_download(void)
{
	if (!g_pogo_fpga)
		return;

	dev_info(&g_pogo_fpga->spi_client->dev, "%s\n", __func__);
	pogo_fpga_config(g_pogo_fpga);
}
EXPORT_SYMBOL(fpga_fw_download);

void fpga_rstn_control(void)
{
	if (!g_pogo_fpga)
		return;

	dev_info(&g_pogo_fpga->spi_client->dev, "%s: high->low->high\n", __func__);

	gpio_set_value(g_pogo_fpga->pdata->fpga_gpio_reset, GPIO_LEVEL_HIGH);
	gpio_set_value(g_pogo_fpga->pdata->fpga_gpio_reset, GPIO_LEVEL_LOW);
	mdelay(1);
	gpio_set_value(g_pogo_fpga->pdata->fpga_gpio_reset, GPIO_LEVEL_HIGH);

}
EXPORT_SYMBOL(fpga_rstn_control);

void fpga_rstn_high(void)
{
	if (!g_pogo_fpga)
		return;

	gpio_set_value(g_pogo_fpga->pdata->fpga_gpio_reset, GPIO_LEVEL_HIGH);
}
EXPORT_SYMBOL(fpga_rstn_high);

void fpga_rstn_low(void)
{
	if (!g_pogo_fpga)
		return;

	gpio_set_value(g_pogo_fpga->pdata->fpga_gpio_reset, GPIO_LEVEL_LOW);
}
EXPORT_SYMBOL(fpga_rstn_low);

bool is_fw_dl_completed(void)
{
	int cdone = 0;

	if (!g_pogo_fpga)
		return false;

	cdone = gpio_get_value(g_pogo_fpga->pdata->fpga_cdone);

	return cdone ? true : false;
}
EXPORT_SYMBOL(is_fw_dl_completed);

int pogo_fpga_rx_data(struct pogo_fpga *pogo_fpga, char *txbuf, char *rxbuf, int len)
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
		.speed_hz = SPI_CLK_FREQ,
	};

	mode = 0; //spi mode3 is set in probe funcion but here we set for spi mode0//if we want spi_mode_3 we can commnet these 3 lines
	spi = spi_dev_get(pogo_fpga->spi_client);
	spi->mode = mode;

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_spi_cs, 0);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	err = spi_sync(pogo_fpga->spi_client, &msg);

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_spi_cs, 1);

	return err;
}

int pogo_fpga_tx_data(struct pogo_fpga *pogo_fpga, char *txbuf, int len)
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
		.speed_hz = SPI_CLK_FREQ,
	};

	mode = 0;//spi mode3 is set in probe funcion but here we set for spi mode0//if we want spi_mode_3 we can commnet these 3 lines
	spi = spi_dev_get(pogo_fpga->spi_client);
	spi->mode = mode;

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_spi_cs, 0);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer, &msg);
	err = spi_sync(pogo_fpga->spi_client, &msg);

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_spi_cs, 1);

	return err;
}

static struct attribute *pogo_fpga_attributes[] = 
{
	NULL,
};

static const struct attribute_group pogo_fpga_group_attr = 
{
	.attrs = pogo_fpga_attributes,
};

int pogo_fpga_fw_load(struct pogo_fpga *pogo_fpga)
{
	int rc = 0;
	struct spi_device *spi_client = pogo_fpga->spi_client;
	const struct firmware *fw;

	if (pogo_fpga->pdata->fw_name == NULL) {
		dev_err(&spi_client->dev, "fw fime name is null\n");
		return -ENOENT;
	}

	dev_info(&spi_client->dev, "requesting Firmware %s\n", pogo_fpga->pdata->fw_name);
	rc = request_firmware(&fw, pogo_fpga->pdata->fw_name, &spi_client->dev);
	if (rc) {
		dev_err(&spi_client->dev, "request firmware failed %d\n", rc);
		return rc;
	}

	pogo_fpga->image_size = fw->size;
	dev_info(&spi_client->dev, "Firmware read size %d\n", (int)(pogo_fpga->image_size));

	pogo_fpga->image_data = (uint8_t *)kzalloc(pogo_fpga->image_size + TX_HEADER_100_DUMMY_CLOCK_SIZE, GFP_KERNEL);
	if (pogo_fpga->image_data == NULL) {
		dev_err(&spi_client->dev, "image_data memory alloc failed\n");
		release_firmware(fw);
		return -ENOMEM;
	}

	/* 
	* Extra bytes appended to the data to generate extra 100 clocks after transfering actual data
	*/
	memcpy(pogo_fpga->image_data, fw->data, pogo_fpga->image_size);
	memset(pogo_fpga->image_data + pogo_fpga->image_size,
			0, TX_HEADER_100_DUMMY_CLOCK_SIZE); /* 100 clocks ~= 13 dummy bytes transfered */

	release_firmware(fw);
	return 0;
}

int pogo_fpga_config(struct pogo_fpga *pogo_fpga)
{
	int rc = 0;
	struct spi_device *spi_client = pogo_fpga->spi_client;
	uint8_t config_fw[5];
	static bool first_download = true;

	dev_info(&spi_client->dev, "%s ++\n", __func__);

	if (pogo_fpga->image_data == NULL)
		return -ENOMEM;

	if (!mutex_trylock(&pogo_fpga->download_lock)) {
		dev_info(&spi_client->dev, "%s: fw download is already in progress\n", __func__);
		return 0;
	}

	if (!pogo_fpga->is_gpio_allocated) {
		rc = gpio_request(pogo_fpga->pdata->fpga_cdone, "FPGA_CDONE");
		if (rc) {
			dev_err(&spi_client->dev, "%s: unable to request gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_cdone, rc);
			goto out;
		}
		dev_info(&spi_client->dev, "CDONE %d\n", gpio_get_value(pogo_fpga->pdata->fpga_cdone));

		rc = gpio_request(pogo_fpga->pdata->fpga_pogo_ldo_en, "POGO_LDO_EN");
		if (rc) {
			dev_err(&spi_client->dev, "%s: unable to request gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_pogo_ldo_en, rc);
			goto out;
		}

		rc = gpio_direction_output(pogo_fpga->pdata->fpga_pogo_ldo_en, 1);
		if (rc) {
			dev_err(&spi_client->dev, "%s: unable to set direction output gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_pogo_ldo_en, rc);
			goto out;
		}
		dev_info(&spi_client->dev, "POGO_LDO_EN %d\n", gpio_get_value(pogo_fpga->pdata->fpga_pogo_ldo_en));

		rc = gpio_request(pogo_fpga->pdata->fpga_gpio_reset, "FPGA_RESET"); 
		if (rc) {
			dev_err(&spi_client->dev, "%s: unable to request gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_gpio_reset, rc);
			goto out;
		}

		rc = gpio_direction_output(pogo_fpga->pdata->fpga_gpio_reset, 1);
		if (rc) {
			dev_err(&spi_client->dev, "%s: unable to set direction output gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_gpio_reset, rc);
			goto out;
		}
		dev_info(&spi_client->dev, "reset %d\n", gpio_get_value(pogo_fpga->pdata->fpga_gpio_reset));

		rc = gpio_request(pogo_fpga->pdata->fpga_gpio_crst_b, "FPGA_CRST");
		if (rc) {
			dev_err(&spi_client->dev, "%s: unable to request gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_gpio_crst_b, rc);
			goto out;
		}

		rc = gpio_direction_output(pogo_fpga->pdata->fpga_gpio_crst_b, 1);
		if (rc) {
			dev_err(&spi_client->dev, "%s: unable to set direction output gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_gpio_crst_b, rc);
			goto out;
		}
		dev_info(&spi_client->dev, "crst_b %d\n", gpio_get_value(pogo_fpga->pdata->fpga_gpio_crst_b));

		rc = gpio_request(pogo_fpga->pdata->fpga_gpio_spi_cs, "FPGA_CS");
		if (rc) {
			dev_err(&spi_client->dev, "%s: unable to request gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_gpio_spi_cs, rc);
			goto out;
		}

		rc = gpio_direction_output(pogo_fpga->pdata->fpga_gpio_spi_cs, 1);
		if (rc) {
			dev_err(&spi_client->dev, "%s: unable to direction output gpio %d (%d)\n",
					__func__, pogo_fpga->pdata->fpga_gpio_spi_cs, rc);
			goto out;
		}
		dev_info(&spi_client->dev, "cs %d\n", gpio_get_value(pogo_fpga->pdata->fpga_gpio_spi_cs));
		pogo_fpga->is_gpio_allocated = 1;
		dev_info(&spi_client->dev, "%s : gpio allocate done\n", __func__);
	}

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_spi_cs, 0);		    
	dev_info(&spi_client->dev, "cs %d\n", gpio_get_value(pogo_fpga->pdata->fpga_gpio_spi_cs));

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_crst_b, 0);
	dev_info(&spi_client->dev, "crst_b %d\n", gpio_get_value(pogo_fpga->pdata->fpga_gpio_crst_b));
	udelay(1000);

	dev_info(&spi_client->dev, "CDONE %d\n", gpio_get_value(pogo_fpga->pdata->fpga_cdone));

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_crst_b, 1);
	dev_info(&spi_client->dev, "crst_b %d\n", gpio_get_value(pogo_fpga->pdata->fpga_gpio_crst_b));
	udelay(1000);	/* Wait for min 1000 microsec to clear internal configuration memory */

	dev_info(&spi_client->dev, "%s : fw write start\n", __func__);
	rc = pogo_fpga_tx_data(pogo_fpga, pogo_fpga->image_data, pogo_fpga->image_size + TX_HEADER_100_DUMMY_CLOCK_SIZE);
	if (rc < 0) {
		dev_err(&spi_client->dev, "%s: failed to write fw, %d\n",
				__func__, rc);
		goto out;
	}
	dev_info(&spi_client->dev, "%s : fw write done\n", __func__);

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_spi_cs, 1);  
	dev_info(&spi_client->dev, "cs %d\n", gpio_get_value(pogo_fpga->pdata->fpga_gpio_spi_cs));

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_reset, 0);
	dev_info(&spi_client->dev, "reset %d\n", gpio_get_value(pogo_fpga->pdata->fpga_gpio_reset));

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_reset, 1);
	dev_info(&spi_client->dev, "reset %d\n", gpio_get_value(pogo_fpga->pdata->fpga_gpio_reset));

	/*delay 1ms : to settle down FPGA*/
	mdelay(1);

	dev_info(&spi_client->dev, "%s : send reset cmds\n", __func__);
	/*Reset Sequence is added*/
	config_fw[0] = 0x00;
	config_fw[1] = 0x00;
	config_fw[2] = 0xFF;
	config_fw[3] = 0xFF;
	config_fw[4] = 0x80;
	rc = pogo_fpga_tx_data(pogo_fpga, config_fw, sizeof(config_fw));
	if (rc < 0) {
		dev_err(&spi_client->dev, "%s: failed to write cmd 0x%02X, %d\n",
				__func__, config_fw[0], rc);
		goto out;
	}
	memset(config_fw, 0, sizeof(config_fw));

	config_fw[0] = 0x08;
	config_fw[1] = 0x00;
	config_fw[2] = 0xFF;
	config_fw[3] = 0xFF;
	config_fw[4] = 0x80;
	rc = pogo_fpga_tx_data(pogo_fpga, config_fw, sizeof(config_fw));
	if (rc < 0) {
		dev_err(&spi_client->dev, "%s: failed to write cmd 0x%02X, %d\n",
				__func__, config_fw[0], rc);
		goto out;
	}
	memset(config_fw, 0, sizeof(config_fw));

	config_fw[0] = 0x10;
	config_fw[1] = 0x00;
	config_fw[2] = 0xFF;
	config_fw[3] = 0xFF;
	config_fw[4] = 0x80;
	rc = pogo_fpga_tx_data(pogo_fpga, config_fw, sizeof(config_fw));
	if (rc < 0) {
		dev_err(&spi_client->dev, "%s: failed to write cmd 0x%02X, %d\n",
				__func__, config_fw[0], rc);
		goto out;
	}
	memset(config_fw, 0, sizeof(config_fw));

	udelay(1000);	/* Wait for min 1000 microsec to clear internal configuration memory */

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_reset, GPIO_LEVEL_LOW);
	dev_info(&spi_client->dev, "reset %d\n", gpio_get_value(pogo_fpga->pdata->fpga_gpio_reset));

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_reset, GPIO_LEVEL_HIGH);
	dev_info(&spi_client->dev, "reset %d\n", gpio_get_value(pogo_fpga->pdata->fpga_gpio_reset));

	mdelay(10);

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_reset, GPIO_LEVEL_LOW);
	dev_info(&spi_client->dev, "reset %d\n", gpio_get_value(pogo_fpga->pdata->fpga_gpio_reset));
	dev_info(&spi_client->dev, "CDONE %d\n", gpio_get_value(pogo_fpga->pdata->fpga_cdone));

	if (!first_download)
		msleep(500);
	first_download = false;

	dev_info(&spi_client->dev, "%s --\n", __func__);
out:
	mutex_unlock(&pogo_fpga->download_lock);
	return rc;
}

static int pogo_fpga_parse_dt(struct pogo_fpga *pogo_fpga)
{
	struct device_node *np = pogo_fpga->spi_client->dev.of_node;

	pogo_fpga->pdata->fpga_cdone = of_get_named_gpio(np, "fpga,gpio_cdone", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_cdone)) {
		dev_err(&pogo_fpga->spi_client->dev,
				"%s:%d, fpga_cdone not specified\n", __func__, __LINE__);
		return -1;
	}

	pogo_fpga->pdata->fpga_pogo_ldo_en = of_get_named_gpio(np, "fpga,pogo_ldo_en", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_pogo_ldo_en)) {
		dev_err(&pogo_fpga->spi_client->dev,
				"%s:%d, fpga_pogo_ldo_en not specified\n", __func__, __LINE__);
		return -1;
	}

	pogo_fpga->pdata->fpga_gpio_reset = of_get_named_gpio(np, "fpga,gpio_reset", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_gpio_reset)) {
		dev_err(&pogo_fpga->spi_client->dev,
				"%s:%d, reset gpio not specified\n", __func__, __LINE__);
		return -1;
	}

	pogo_fpga->pdata->fpga_gpio_crst_b = of_get_named_gpio(np, "fpga,gpio_crst_b", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_gpio_crst_b)) {
		dev_err(&pogo_fpga->spi_client->dev,
				"%s:%d, creset_b gpio not specified\n", __func__, __LINE__);
		return -1;
	}

	pogo_fpga->pdata->fpga_gpio_spi_cs = of_get_named_gpio(np, "fpga,gpio_cs", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_gpio_spi_cs)) {
		dev_err(&pogo_fpga->spi_client->dev,
				"%s:%d, cs gpio not specified\n", __func__, __LINE__);
		return -1;
	}

	pogo_fpga->pdata->fpga_gpio_spi_miso = of_get_named_gpio(np, "fpga,gpio_miso", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_gpio_spi_cs)) {
		dev_err(&pogo_fpga->spi_client->dev,
				"%s:%d, gpio_miso not specified\n", __func__, __LINE__);
		return -1;
	}

	pogo_fpga->pdata->fpga_gpio_spi_mosi = of_get_named_gpio(np, "fpga,gpio_mosi", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_gpio_spi_cs)) {
		dev_err(&pogo_fpga->spi_client->dev,
				"%s:%d, gpio_mosi not specified\n", __func__, __LINE__);
		return -1;
	}

	pogo_fpga->pdata->fpga_gpio_spi_clk = of_get_named_gpio(np, "fpga,gpio_spi_clk", 0);
	if (!gpio_is_valid(pogo_fpga->pdata->fpga_gpio_spi_cs)) {
		dev_err(&pogo_fpga->spi_client->dev,
				"%s:%d, gpio_spi_clk not specified\n", __func__, __LINE__);
		return -1;
	}

	of_property_read_string_index(np, "fpga,fw_name", 0, &pogo_fpga->pdata->fw_name);

	dev_info(&pogo_fpga->spi_client->dev,
			"%s: fw:%s, CDONE:%d, LDO_EN:%d, reset:%d, creset_b:%d, cs:%d\n",
			__func__, pogo_fpga->pdata->fw_name, pogo_fpga->pdata->fpga_cdone,
			pogo_fpga->pdata->fpga_pogo_ldo_en, pogo_fpga->pdata->fpga_gpio_reset,
			pogo_fpga->pdata->fpga_gpio_crst_b, pogo_fpga->pdata->fpga_gpio_spi_cs);

	return 0;
}

static int pogo_fpga_probe(struct spi_device *spi_client) 
{
	unsigned long minor;
	int err = 0;
	struct pogo_fpga *pogo_fpga = NULL;

	dev_info(&spi_client->dev, "%s ++\n", __func__);

	pogo_fpga = kzalloc(sizeof(struct pogo_fpga), GFP_KERNEL);
	if (!pogo_fpga) {
		dev_err(&spi_client->dev, "unable to allocate memory\n");
		err = -ENOMEM;
		goto exit;
	}

	pogo_fpga->spi_client = spi_client;

	spi_client->dev.platform_data = &pogo_fpga_pdata;	/* MDN Addition */
	pogo_fpga->pdata = spi_client->dev.platform_data;
	if (!pogo_fpga->pdata) {
		dev_err(&spi_client->dev, "No platform data - aborting\n");
		err = -EINVAL;
		goto exit;
	}

	err = pogo_fpga_parse_dt(pogo_fpga);
	if (err) {
		goto mem_cleanup;
	}

	if (pogo_fpga->vdd != NULL) {
		err = regulator_enable(pogo_fpga->vdd);
		if (err) {
			dev_err(&spi_client->dev, "enable ldo failed, rc=%d\n", err);
			goto mem_cleanup;
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
	mutex_init(&pogo_fpga->download_lock);

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

	err = pogo_fpga_fw_load(pogo_fpga);
	if (err) {
		dev_err(&spi_client->dev, "unable to load fw\n");
		goto err_remove_files;
	}

	err = pogo_fpga_config(pogo_fpga);
	if (err) {
		dev_err(&spi_client->dev, "unable to configure_iCE\n");
		goto err_remove_files;
	}
	dev_info(&spi_client->dev, "%s --\n", __func__);

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
	if (pogo_fpga->image_data)
		kfree(pogo_fpga->image_data);
	if (pogo_fpga) {
		g_pogo_fpga = NULL;
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

	kfree(pogo_fpga->image_data);
	/* prevent new opens */
	mutex_lock(&device_list_lock);
	if (pogo_fpga->users == 0)
		kfree(pogo_fpga);
	mutex_unlock(&device_list_lock);
	g_pogo_fpga = NULL;

	return 0;
}

static int pogo_fpga_resume(struct device *dev) 
{
	struct pogo_fpga *pogo_fpga = dev_get_drvdata(dev);

	dev_dbg(&pogo_fpga->spi_client->dev, "%s\n", __func__);

	return 0;
}

static int pogo_fpga_suspend(struct device *dev) 
{
	struct pogo_fpga *pogo_fpga = dev_get_drvdata(dev);

	dev_dbg(&pogo_fpga->spi_client->dev, "%s\n", __func__);

	gpio_set_value(pogo_fpga->pdata->fpga_gpio_reset, GPIO_LEVEL_LOW);

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

	pr_info("%s\n", __func__);
	ret = spi_register_driver(&pogo_fpga_driver);
	if (ret < 0) {
		pr_info("spi register failed: %d\n", ret);
		return ret;
	}
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

