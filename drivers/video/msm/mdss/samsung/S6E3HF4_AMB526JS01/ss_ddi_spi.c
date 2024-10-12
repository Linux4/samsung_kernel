#include <linux/spi/spi.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>

#include "../ss_dsi_panel_common.h"

#define DRIVER_NAME "ddi_spi"

static struct spi_device *ss_spi;

static DEFINE_MUTEX(ss_spi_lock);

static u16 key_enable_tx_cmd[] = {0xF0, 0x2E, 0xAD};
static u16 op_read_rx_cmd[] = {0xB2};
static u16 rxbuf[10];
static char copr[10];
#if 0
static int ss_spi_write(char* data, int length)
{
	int i, res;
	u16 tx_buf[3];

	pr_info("%s ++ \n", __func__);

	if (!ss_spi) {
		pr_err("%s : spi is null..\n", __func__);
		return 1;
	}

	mutex_lock(&ss_spi_lock);

	tx_buf[0] = (0x0 << 8) | data[0];

	for (i=1; i<length; i++)
		tx_buf[i] = (0x1 << 8) | data[i];

	res = spi_write(ss_spi, tx_buf, 6);
	if (res)
		pr_err("%s : write fail..\n", __func__);

	mutex_unlock(&ss_spi_lock);

	pr_info("%s -- \n", __func__);

	return res;
}

static int ss_spi_read(char addr, char* rxbuf, int length)
{
	int res;
	u16 txbuf;

	struct spi_message spi_msg;
	struct spi_transfer spi_xfer;

	pr_info("%s ++ \n", __func__);

	if (!ss_spi) {
		pr_err("%s : spi is null..\n", __func__);
		return 1;
	}

	mutex_lock(&ss_spi_lock);

	txbuf = (0x0 << 8) | addr;

	spi_message_init(&spi_msg);
	spi_message_add_tail(&spi_xfer, &spi_msg);

	spi_xfer.tx_buf = &txbuf;
	spi_xfer.rx_buf = rxbuf;
	spi_xfer.len = length;

	res = spi_sync(ss_spi, &spi_msg);
	if (res)
		pr_info("%s : read fail..\n", __func__);

	memcpy(rxbuf, spi_xfer.rx_buf + 1, length);

	if (length)
	{
		pr_info("rx(0x%x) : %x %x %x %x %x %x %x %x %x %x\n", txbuf,
			rxbuf[0], rxbuf[1], rxbuf[2], rxbuf[3], rxbuf[4],
			rxbuf[5], rxbuf[6], rxbuf[7], rxbuf[8], rxbuf[9]);
	}

	mutex_unlock(&ss_spi_lock);

	pr_info("%s -- \n", __func__);

	return res;
}
#endif
static int ss_spi_write_read(struct spi_device *spi, u16 *txcmd, int rxaddr, u16 *rxbuf)
{
	u16 buf[1];
	u16 tbuf[3];

	struct spi_message msg;
	int ret;

	struct spi_transfer xfer_readkey = {
		.len		= 6,	/*byte count*/
		.tx_buf		= tbuf,
	};

	struct spi_transfer xfer_tx = {
		.len		= 2,
		.tx_buf		= buf,

	};
	struct spi_transfer xfer_rx = {
		.len		= 20,
		.rx_buf		= rxbuf,
	};


	pr_info("%s ++ \n", __func__);

	if (!ss_spi) {
		pr_err("%s : no ss_spi..\n", __func__);
		return 0;
	}
/*	mutex_lock(&ss_spi_lock);*/

	if (((0x00 << 8) | txcmd[0]) & 0x01)
		tbuf[0] = (((0x00 << 8) | txcmd[0]) >> 1) | 0x8000;
	else
		tbuf[0] = (((0x00 << 8) | txcmd[0]) >> 1);

	if (((0xFF << 8) | txcmd[1]) & 0x01)
		tbuf[1] = (((0xFF << 8) | txcmd[1]) >> 1) | 0x8000;
	else
		tbuf[1] = (((0xFF << 8) | txcmd[1]) >> 1);

	if (((0xFF << 8) | txcmd[2]) & 0x01)
		tbuf[2] = (((0xFF << 8) | txcmd[2]) >> 1 )| 0x8000;
	else
		tbuf[2] = (((0xFF << 8) | txcmd[2]) >> 1);

	pr_info("%s [TX buf] %2x %2x %2x\n", __func__, tbuf[0], tbuf[1], tbuf[2]);

	if (((0x00 << 8) | rxaddr) & 0x01)
		buf[0] = (((0x00 << 8) | rxaddr)>> 1) | 0x8000;
	else
		buf[0] = (((0x00 << 8) | rxaddr)>> 1);

	spi_message_init(&msg);
	spi_message_add_tail(&xfer_readkey, &msg);
	spi_message_add_tail(&xfer_tx, &msg);
	spi_message_add_tail(&xfer_rx, &msg);

	ret = spi_sync(spi, &msg);

	if (ret)
			pr_err("%s : spi_sync fail..\n", __func__);

	pr_info("rx(0x%x) : %x %x %x %x %x %x %x %x %x %x\n", rxaddr,
		rxbuf[0], rxbuf[1], rxbuf[2], rxbuf[3], rxbuf[4],
		rxbuf[5], rxbuf[6], rxbuf[7], rxbuf[8], rxbuf[9]);

/*	mutex_unlock(&ss_spi_lock);*/

	pr_info("%s -- \n", __func__);

	return ret;
}

static char* spi_copr_read(void)
{
	int ret;
	int i;

	ret = ss_spi_write_read(ss_spi, key_enable_tx_cmd, op_read_rx_cmd[0], rxbuf);

	if (ret){
		pr_err("%s : spi_sync fail..\n", __func__);
		return NULL;
	}

	for(i = 0; i<10; i++)
		copr[i] = (rxbuf[i] >> 8 & 0xff);

	pr_debug("rx: %x %x %x %x %x %x %x %x %x %x\n",
			copr[0], copr[1], copr[2], copr[3], copr[4],
			copr[5], copr[6], copr[7], copr[8], copr[9]);

	return copr;
}

static int ss_spi_probe(struct spi_device *client)
{
	int ret;
	struct samsung_display_driver_data *vdd = samsung_get_vdd();

	pr_info("%s : ++ \n", __func__);

	if (client == NULL) {
		pr_err("%s : ss_spi_probe spi is null\n", __func__);
		return -1;
	}

	client->max_speed_hz = 8000000;
	client->bits_per_word = 9;
	client->mode =  SPI_MODE_3;

	ret = spi_setup(client);

	if (ret < 0) {
		pr_err("%s : spi_setup error (%d)\n", __func__, ret);
		return ret;
	}

	ss_spi = client;
	vdd->panel_func.samsung_spi_read_reg = spi_copr_read;

	pr_info("%s : -- \n", __func__);

	return ret;
}

static int ss_spi_remove(struct spi_device *spi)
{
	pr_err("%s : remove \n", __func__);
	return 0;
}

static const struct of_device_id ddi_spi_match_table[] = {
	{   .compatible = "ss,ddi-spi",
	},
	{}
};

static struct spi_driver ddi_spi_driver = {
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = ddi_spi_match_table,
	},
	.probe		= ss_spi_probe,
	.remove		= ss_spi_remove,
};

static int __init ss_spi_init(void)
{
	int ret;

	pr_info("%s : ++ \n", __func__);

	ret = spi_register_driver(&ddi_spi_driver);
	if (ret)
		pr_err("%s : ddi spi register fail : %d\n", __func__, ret);

	pr_info("%s : -- \n", __func__);

	return ret;
}
module_init(ss_spi_init);

static void __exit ss_spi_exit(void)
{
	spi_unregister_driver(&ddi_spi_driver);
}
module_exit(ss_spi_exit);

MODULE_DESCRIPTION("spi interface driver for ddi");
MODULE_LICENSE("GPL");

