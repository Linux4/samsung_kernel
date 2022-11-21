/*
 * =================================================================
 *
 *
 *	Description:  samsung display common file
 *
 *	Author: samsung display driver team
 *	Company:  Samsung Electronics
 *
 * ================================================================
 */

#include "ss_ddi_spi_common.h"

#define SPI_CTRL_RX 0x00

int ss_spi_read(struct spi_device *spi, u8 *buf,
				int tx_bpw, int rx_bpw, int tx_size, int rx_size, u8 rx_addr)
{
	int i;
	u8 *rx_buf = NULL;
	u8 *tx_buf = NULL;
#if defined(CONFIG_SEC_FACTORY)
	u8 *dummy_buf = NULL;
#endif
	struct samsung_display_driver_data *vdd = NULL;
	struct spi_message msg;
	int ret = 0;

	struct spi_transfer xfer[] = {
		{ .bits_per_word = tx_bpw,	.len = tx_size,	},
		{ .bits_per_word = rx_bpw,	.len = rx_size,	},
#if defined(CONFIG_SEC_FACTORY)
		{ .bits_per_word = rx_bpw,	.len = 1,		},
#endif
	};

	if (!spi) {
		LCD_ERR("no spi device..\n");
		ret = -EINVAL;
		goto err;
	}

	vdd = container_of(spi->dev.driver, struct samsung_display_driver_data,
				spi_driver.driver);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("no vdd");
		return -ENODEV;
	}

	mutex_lock(&vdd->ss_spi_lock);

	tx_buf = kmalloc(tx_size, GFP_KERNEL | GFP_DMA);
	if (tx_buf == NULL) {
		LCD_ERR("fail to alloc tx_buf..\n");
		goto err;
	}
	tx_buf[0] = rx_addr; // TX
	xfer[0].tx_buf = tx_buf;

	rx_buf = kmalloc(rx_size, GFP_KERNEL | GFP_DMA);
	if (rx_buf == NULL) {
		LCD_ERR("fail to alloc rx_buf..\n");
		goto err;
	}
	xfer[1].rx_buf = rx_buf;

#if defined(CONFIG_SEC_FACTORY)
	dummy_buf = kmalloc(xfer[2].len, GFP_KERNEL | GFP_DMA);
	if (dummy_buf == NULL) {
		LCD_ERR("fail to alloc dummy_buf..\n");
		goto err;
	}
	xfer[2].rx_buf = dummy_buf;
#endif

	if (vdd->ddi_spi_status == DDI_SPI_SUSPEND) {
		LCD_DEBUG("ddi spi is suspend..\n");
		ret = -EINVAL;
		goto err;
	}

	LCD_DEBUG("++\n");

	spi_message_init(&msg);

	for (i = 0; i < ARRAY_SIZE(xfer); i++)
		spi_message_add_tail(&xfer[i], &msg);

	ret = spi_sync(spi, &msg);
	if (ret) {
		pr_err("[mdss spi] %s : spi_sync fail..\n", __func__);
		goto err;
	}

	LCD_DEBUG("rx(0x%x) : ", tx_buf[1]);
	for (i = 0; i < rx_size; i++) {
		LCD_DEBUG("[%d] %02x ", i+1, rx_buf[i]);
	}
	LCD_DEBUG("\n");

	memcpy(buf, rx_buf, rx_size);
#if KERNEL_VER > 409
	if (vdd->ddi_spi_cs_high_gpio_for_gpara > 0) {
		LCD_INFO("%s wait \n", dev_name(spi->controller->dev.parent));

		/* max wait for 4 second */
		for (i = 0; i < 200; i++) {
			if (pm_runtime_status_suspended(spi->controller->dev.parent))
				break;

			msleep(20);
		}

		if (!pm_runtime_status_suspended(spi->controller->dev.parent)) {
			LCD_INFO("%s is not suspend for 4second\n", dev_name(spi->controller->dev.parent));

			pm_runtime_barrier(spi->controller->dev.parent);

			LCD_INFO("%s suspend status : %d \n", dev_name(spi->controller->dev.parent),
				pm_runtime_status_suspended(spi->controller->dev.parent));
		}

		LCD_INFO("%s end \n", dev_name(spi->controller->dev.parent));
	}
#endif
err:
	mutex_unlock(&vdd->ss_spi_lock);

	if (rx_buf)
		kfree(rx_buf);
	if (tx_buf)
		kfree(tx_buf);
#if defined(CONFIG_SEC_FACTORY)
	if (dummy_buf)
		kfree(dummy_buf);
#endif

	LCD_DEBUG("--\n");

	return ret;
}

static int ss_spi_parse_dt(struct spi_device *spi_dev)
{
	struct device_node *np;
	int val, ret;

	np = spi_dev->dev.of_node;
	if (!np) {
		LCD_ERR("of_node is null..\n");
		return -ENODEV;
	}

	LCD_ERR("np name : %s\n", np->full_name);

	ret = of_property_read_u32(np, "spi-max-frequency", &val);
	if (!ret)
		spi_dev->max_speed_hz = val;
	else
		LCD_ERR("No spi-max-frequency..\n");

	ret = of_property_read_u32(np, "spi-bpw", &val);
	if (!ret)
		spi_dev->bits_per_word = val;
	else
		LCD_ERR("No spi-bpw..\n");

	ret = of_property_read_u32(np, "spi-mode", &val);
	if (!ret)
		spi_dev->mode = (val > 3) ? 0 : val;
	else
		LCD_ERR("No spi-mode..\n");

	LCD_INFO("max speed (%d), bpw (%d), mode (%d) \n",
			spi_dev->max_speed_hz, spi_dev->bits_per_word, spi_dev->mode);

	return ret;
}

static int ss_spi_probe(struct spi_device *client)
{
	struct samsung_display_driver_data *vdd;
	int ret = 0;

	if (client == NULL) {
		LCD_ERR("%s : ss_spi_probe spi is null\n", __func__);
		return -EINVAL;
	}

	vdd = container_of(client->dev.driver,
						struct samsung_display_driver_data,
						spi_driver.driver);

	LCD_INFO("chip select(%d), bus number(%d)\n",
			client->chip_select, client->master->bus_num);

	ret = ss_spi_parse_dt(client);
	if (ret) {
		LCD_ERR("can not parse from ddi spi dt..\n");
		return ret;
	}

	ret = spi_setup(client);
	if (ret < 0) {
		LCD_ERR("%s : spi_setup error (%d)\n", __func__, ret);
		return ret;
	}

	vdd->spi_dev = client;
	dev_set_drvdata(&client->dev, vdd);

	LCD_ERR("%s : --\n", __func__);
	return ret;
}

static int ss_spi_remove(struct spi_device *spi)
{
	pr_err("[mdss] %s : remove \n", __func__);
	return 0;
}

static int ddi_spi_suspend(struct device *dev)
{
	struct samsung_display_driver_data *vdd =
		(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("no vdd");
		return -ENODEV;
	}

	mutex_lock(&vdd->ss_spi_lock);
	vdd->ddi_spi_status = DDI_SPI_SUSPEND;
	LCD_DEBUG(" %d\n", vdd->ddi_spi_status);
	mutex_unlock(&vdd->ss_spi_lock);

	return 0;
}

static int ddi_spi_resume(struct device *dev)
{
	struct samsung_display_driver_data *vdd =
			(struct samsung_display_driver_data *)dev_get_drvdata(dev);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("no vdd");
		return -ENODEV;
	}

	mutex_lock(&vdd->ss_spi_lock);
	vdd->ddi_spi_status = DDI_SPI_RESUME;
	LCD_DEBUG(" %d\n", vdd->ddi_spi_status);
	mutex_unlock(&vdd->ss_spi_lock);

	return 0;
}

static int ddi_spi_reboot_cb(struct notifier_block *nb,
			unsigned long action, void *data)
{
	struct samsung_display_driver_data *vdd = container_of(nb,
		struct samsung_display_driver_data, spi_notif);

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("no vdd");
		return -ENODEV;
	}

	mutex_lock(&vdd->ss_spi_lock);
	vdd->ddi_spi_status = DDI_SPI_SUSPEND;
	LCD_ERR(" %d\n", vdd->ddi_spi_status);
	mutex_unlock(&vdd->ss_spi_lock);

	return NOTIFY_OK;
}

static const struct dev_pm_ops ddi_spi_pm_ops = {
	.suspend = ddi_spi_suspend,
	.resume = ddi_spi_resume,
};

static const struct of_device_id ddi_spi_match_table[] = {
	{   .compatible = "ss,ddi-spi",
	},
	{}
};

#if 0
static struct spi_driver ddi_spi_driver = {
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
		.of_match_table = ddi_spi_match_table,
		.pm	= &ddi_spi_pm_ops,
	},
	.probe		= ss_spi_probe,
	.remove		= ss_spi_remove,
};

static struct notifier_block ss_spi_reboot_notifier = {
	.notifier_call = ddi_spi_reboot_cb,
};
#endif

int ss_spi_init(struct samsung_display_driver_data *vdd)
{
	int ret;
	char drivername[10];

	if (IS_ERR_OR_NULL(vdd)) {
		LCD_ERR("no vdd");
		return -ENODEV;
	}

	if(!vdd->samsung_support_ddi_spi) {
		LCD_ERR("%s : No support for ddi spi\n", __func__);
		return 0;
	}

	if (vdd->ndx == PRIMARY_DISPLAY_NDX)
		sprintf(drivername, "ddi_spi");
	else
		sprintf(drivername, "ddi_spi%d", vdd->ndx);

	LCD_ERR("%s : ++\n", __func__);

	vdd->spi_driver.driver.name = drivername;
	vdd->spi_driver.driver.owner = THIS_MODULE;
	vdd->spi_driver.driver.of_match_table = ddi_spi_match_table;
	vdd->spi_driver.driver.pm = &ddi_spi_pm_ops;
	vdd->spi_driver.probe = ss_spi_probe;
	vdd->spi_driver.remove = ss_spi_remove;
	vdd->spi_notif.notifier_call = ddi_spi_reboot_cb;
	mutex_lock(&vdd->ss_spi_lock);
	vdd->ddi_spi_status = DDI_SPI_RESUME;
	mutex_unlock(&vdd->ss_spi_lock);

	ret = spi_register_driver(&vdd->spi_driver);
	if (ret)
		LCD_ERR("%s : ddi spi register fail : %d\n", __func__, ret);

	register_reboot_notifier(&vdd->spi_notif);

	LCD_ERR("%s : --\n", __func__);
	return ret;
}
