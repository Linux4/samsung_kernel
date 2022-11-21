/****************************************************************

Siano Mobile Silicon, Inc.
MDTV receiver kernel modules.
Copyright (C) 2006-2008, Uri Shkolnik

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

 This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

****************************************************************/
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/dma-mapping.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/notifier.h>
#include <linux/gpio.h>

#include <linux/io.h>
#include <mach/dma.h>
#include <mach/addr-map.h>
#include <mach/hardware.h>

#include <linux/spi/cmmb.h>

#include "smsdbg_prn.h"
#include "smscharioctl.h"
#include "smscoreapi.h"
#include "smsspiphy.h"

extern void *smsspi_get_dev(void);
extern void smsspi_save_dev(void *dev);
extern void smsspidrv_sw_powerdown(void);

/*SPI interface */
#define DRIVER_NAME "cmmb_if"

/*
 * Here is a spi transfer limitation:
 * some platform can't support very large spi burst transfer
 * example:the pxa910 spi driver code support 8191B maxinum
 *
 * the transfer can split to muti transfer in chip_spi_xfer
 * if the xfer len is larger then the limit
 */
#define PXA_SPI_MAX_SIZE		4096

/*
 * If the platform dosen't provide the callback functions to
 * control the POWER or RESET, or the driver want to control
 * the pin directlly,
 * define "DRIVER_HANDLE_GPIO" Here
 */
#define DRIVER_HANDLE_GPIO

/*
 * If the platform dosen't pass the pin definition,or the
 * driver want to set the pin num directlly,
 * define the "PIN_DEFINED_BY_DRIVE" Here.
 *
 * You can define the power/reset/spi_cs pin num in this file
 * directlly. By this way,the driver is easy to debug or port
 */
/*#define PIN_DEFINED_BY_DRIVE*/
#define POWER_PIN_NUM			150
#define RESET_PIN_NUM			151
#define SPI_CS_PIN_NUM			34

static unsigned long u_irq_count = 1;

/*
 * A description of one of our devices.
 * Locking: controlled by s_mutex.  Certain fields, however, require
 *	    the dev_lock spinlock; they are marked as such by comments.
 *	    dev_lock is also required for access to device registers.
 */
struct smsspi_dev {
	unsigned long flags;	/* Buffer status, mainly (dev_lock) */
	int users;		/* How many open FDs */
	struct file *owner;	/* Who has data access (v4l2) */

	/* HW Subsystem structures */
	int gpio_power;		/* the pin to power on/off the module */
	int gpio_reset;		/* the pin to reset  the module */
	int gpio_cs;		/* spi cs pin num */
	int gpio_defined;	/* The platform has set the GPIO */

	/* Interrupt Register */
	int irq;
	void (*interruptHandler) (void *);
	void *intr_context;

	/* DMA buffers */
	int mapcount;
	unsigned int dma_buf_size;	/* allocated size */
	int order;		/* Internal buffer addresses */

	void *dma_rx_bufs;	/* Internal Rx buffer addresses */
	dma_addr_t dma_rx_handles;	/* Buffer Rx bus addresses */
	void *dma_tx_bufs;	/* Internal Tx buffer addresses */
	dma_addr_t dma_tx_handles;	/* Tx Buffer bus addresses */
	int is_tx_clean;	/* whether dma_tx_bufs has be cleaned */

	/* Locks */
	struct list_head dev_list;	/* link to other devices */
	struct mutex s_mutex;	/* Access to this structure */
	spinlock_t dev_lock;	/* Access to device */

	/* Misc */
	wait_queue_head_t iowait;	/* Waiting on frame data */
	struct spi_device *spi;
	unsigned char spi_irq_enable;

	/* the HW status, escepially relate to suspend and power*/
	unsigned int smschip_status;
};

/*
 * The porting layer, according to different host platform.
 *
 * The below SPI functions use the struct smsspi_dev,instead of
 * struct spi_device.
 * The purposes are
 *  1.make the interface simple
 *  2.easy to add other features in the further if need
 *
 * if the chip_xxx return 0, it is OK, other is not OK
 */

static int chip_power_reset(struct smsspi_dev *drv_info)
{
#ifdef DRIVER_HANDLE_GPIO
	int chip_rst;
	int rst_loop;
	sms_info("sms smschipreset\n");
	chip_rst = drv_info->gpio_reset;
	if (gpio_request(chip_rst, "cmmb rst")) {
		pr_warning("failed to request GPIO for CMMB RST\n");
		return -EIO;
	}

	/* reset chip twice,work around to fix reset fail sometimes */
	rst_loop = 2;
	while (rst_loop--) {
		/* reset cmmb, keep low for about 1ms */
		gpio_direction_output(chip_rst, 0);
		msleep(10);		/* 10ms enough */

		/* get cmmb go out of reset state */
		gpio_direction_output(chip_rst, 1);
		/* wait for at least 100ms after reset*/
		msleep(100);
	}

	gpio_free(chip_rst);
	return 0;
#else
	struct cmmb_platform_data *pdata;
	pdata = drv_info->spi->dev.platform_data;
	if (!pdata || !pdata->power_reset)
		return -EIO;
	pdata->power_reset();
	return 0;
#endif
}

static int chip_power_on(struct smsspi_dev *drv_info)
{
#ifdef DRIVER_HANDLE_GPIO
	int chip_en;

	struct cmmb_platform_data *pdataa;

	pdataa = drv_info->spi->dev.platform_data;
	if (pdataa && pdataa->cmmb_regulator)
		pdataa->cmmb_regulator(1);

	chip_en = drv_info->gpio_power;
	if (gpio_request(chip_en, "cmmb power")) {
		pr_warning("[ERROR] failed to request GPIO for CMMB POWER\n");
		return -EIO;
	}
	gpio_direction_output(chip_en, 0);
	msleep(100);	/* TODO:Check with HW Engineer */

	gpio_direction_output(chip_en, 1);
	gpio_free(chip_en);

	msleep(100);	/* TODO:Check with HW Engineer */

	return chip_power_reset(drv_info);
#else
	struct cmmb_platform_data *pdata;

	pdata = drv_info->spi->dev.platform_data;
	if (!pdata || !pdata->power_on)
		return -EIO;
	pdata->power_on();

	return 0;
#endif
}

static int chip_power_down(struct smsspi_dev *drv_info)
{
#ifdef DRIVER_HANDLE_GPIO
	int chip_en;

	struct cmmb_platform_data *pdataa;
	pdataa = drv_info->spi->dev.platform_data;

	chip_en = drv_info->gpio_power;
	if (gpio_request(chip_en, "cmmb power")) {
		pr_warning("failed to request GPIO for CMMB POWER\n");
		return -EIO;
	}

	gpio_direction_output(chip_en, 0);
	gpio_free(chip_en);
	msleep(100);	/* TODO:Check with HW Engineer */

	if (pdataa && pdataa->cmmb_regulator)
		pdataa->cmmb_regulator(0);

	return 0;
#else
	struct cmmb_platform_data *pdata;

	pdata = drv_info->spi->dev.platform_data;
	if (!pdata || !pdata->power_off)
		return -EIO;
	pdata->power_off();

	return 0;
#endif
}

static void chip_cs_assert(struct smsspi_dev *drv_info)
{
#ifdef DRIVER_HANDLE_GPIO
	int cs;
	cs = drv_info->gpio_cs;
	gpio_direction_output(cs, 0);
#else
	struct cmmb_platform_data *pdata;

	pdata = drv_info->spi->dev.platform_data;
	if (!pdata || !pdata->cs_assert)
		return;
	pdata->cs_assert();
#endif
}

static void chip_cs_deassert(struct smsspi_dev *drv_info)
{
#ifdef DRIVER_HANDLE_GPIO
	int cs;
	cs = drv_info->gpio_cs;
	gpio_direction_output(cs, 1);
#else
	struct cmmb_platform_data *pdata;

	pdata = drv_info->spi->dev.platform_data;
	if (!pdata || !pdata->cs_deassert)
		return;
	pdata->cs_deassert();
#endif
}

/* -------------------------------------------------------------------- */
/*
 * DMA buffer management.  These functions need s_mutex held.
 */

/* FIXME: this is inefficient as hell, since dma_alloc_coherent just
 * does a get_free_pages() call, and we waste a good chunk of an orderN
 * allocation.  Should try to allocate the whole set in one chunk.
 */

static int chip_alloc_dma_bufs(struct smsspi_dev *drv_info)
{
	sms_info("enter\n");
	drv_info->dma_buf_size = PXA_SPI_MAX_SIZE;

	drv_info->order = get_order(drv_info->dma_buf_size);
	drv_info->dma_rx_bufs = (unsigned long *)
		__get_free_pages(GFP_KERNEL | GFP_DMA, drv_info->order);
	if (drv_info->dma_rx_bufs == NULL) {
		sms_err("Failed to allocate Rx DMA buffer\n");
		return -ENOMEM;
	}
	drv_info->dma_rx_handles = __pa(drv_info->dma_rx_bufs);

	drv_info->dma_tx_bufs = (unsigned long *)
		__get_free_pages(GFP_KERNEL | GFP_DMA, drv_info->order);
	if (drv_info->dma_tx_bufs == NULL) {
		sms_err("Failed to allocate Tx DMA buffer\n");
		free_pages((unsigned long)drv_info->dma_rx_bufs,
			   drv_info->order);
		drv_info->dma_rx_bufs = NULL;
		return -ENOMEM;
	}
	drv_info->dma_tx_handles = __pa(drv_info->dma_tx_bufs);

	/* For debug, remove eventually */
	memset(drv_info->dma_rx_bufs, 0xcc, drv_info->dma_buf_size);

	memset(drv_info->dma_tx_bufs, 0x00, drv_info->dma_buf_size);
	return 0;
}

static void chip_free_dma_bufs(struct smsspi_dev *drv_info)
{
	sms_info("enter\n");
	if (drv_info->dma_rx_bufs) {
		free_pages((unsigned long)drv_info->dma_rx_bufs,
			   drv_info->order);
		drv_info->dma_rx_bufs = NULL;
	}

	if (drv_info->dma_tx_bufs) {
		free_pages((unsigned long)drv_info->dma_tx_bufs,
			   drv_info->order);
		drv_info->dma_tx_bufs = NULL;
	}
}

static int spi_duplex_xfer(struct spi_device *spi,
			   const u8 *txbuffer, u8 *rxbuffer, int length)
{
	struct spi_transfer t = {
		.tx_buf = txbuffer,
		.rx_buf = rxbuffer,
		.len = length,
	};
	struct spi_message m;

	spi_message_init(&m);
	spi_message_add_tail(&t, &m);
	return spi_sync(spi, &m);
}

static int chip_spi_xfer(struct smsspi_dev *drv_info,
			 const u8 *txbuffer, u8 *rxbuffer, int length)
{
	struct spi_device *spi = drv_info->spi;
	int offset = 0;
	int temp_size = 0;
	int ret;

	if (!txbuffer) {
		if (!drv_info->is_tx_clean) {
			memset(drv_info->dma_tx_bufs, 0x00, PXA_SPI_MAX_SIZE);
			drv_info->is_tx_clean = 1;
		}
	}

	while (length) {
		temp_size =
		    (length >= PXA_SPI_MAX_SIZE) ? PXA_SPI_MAX_SIZE : length;

		if (txbuffer) {
			/* Spi Write has real data */
			drv_info->is_tx_clean = 0;
			memcpy(drv_info->dma_tx_bufs, txbuffer + offset,
			       temp_size);
		}
		ret = spi_duplex_xfer(spi,
				      (const u8 *)drv_info->dma_tx_bufs,
				      (u8 *) drv_info->dma_rx_bufs, temp_size);

		if (ret) {
			sms_err
			    ("%s: spi_write_read to cmmb device error! %d \r\n",
			     __func__, ret);
			break;
		}
		if (rxbuffer) {
			/* Spi Read has read data */
			memcpy(rxbuffer + offset, drv_info->dma_rx_bufs,
			       temp_size);
		}
		offset += temp_size;
		length -= temp_size;
	}
	return length;
}

static int sms_spi_xfer(struct smsspi_dev *drv_info,
			const u8 *txbuffer, int txlength,
			u8 *rxbuffer, int rxlength)
{

	int xfer_len, un_xfer;
	xfer_len = (txlength >= rxlength) ? txlength : rxlength;

	chip_cs_assert(drv_info);
	un_xfer = chip_spi_xfer(drv_info, txbuffer, rxbuffer, xfer_len);
	chip_cs_deassert(drv_info);

	return un_xfer;
}

/* ---------------------------------------------------------------------*/
/*
 * We keep a simple list of known devices to search at open time.
 */
static LIST_HEAD(cmmb_dev_list);
static DEFINE_MUTEX(cmmb_dev_list_lock);

static void cmmb_add_dev(struct smsspi_dev *drv_info)
{
	sms_info("enter  %s +++++++++\n", __func__);
	mutex_lock(&cmmb_dev_list_lock);
	list_add_tail(&drv_info->dev_list, &cmmb_dev_list);
	mutex_unlock(&cmmb_dev_list_lock);
}

static void cmmb_remove_dev(struct smsspi_dev *drv_info)
{
	sms_info("enter  %s +++++++++\n", __func__);
	mutex_lock(&cmmb_dev_list_lock);
	list_del(&drv_info->dev_list);
	mutex_unlock(&cmmb_dev_list_lock);
}

static struct smsspi_dev *cmmb_find_by_spi(struct spi_device *spi)
{
	struct smsspi_dev *drv_info;
	/* sms_info("enter  %s +++++++++\n", __func__); */

	mutex_lock(&cmmb_dev_list_lock);
	list_for_each_entry(drv_info, &cmmb_dev_list, dev_list) {
		if (drv_info->spi == spi)
			goto done;
	}
	drv_info = NULL;
done:
	mutex_unlock(&cmmb_dev_list_lock);
	return drv_info;
}

/* ---------------------------------------------------------------------- */
/*
 * Interrupt handler stuff
 */
static irqreturn_t spibus_interrupt(int irq, void *context)
{
	struct smsspi_dev *spiphy_dev = (struct smsspi_dev *)context;
	/*sms_info("INT counter = %d\n", (int)u_irq_count++); */

	if (spiphy_dev->interruptHandler)
		spiphy_dev->interruptHandler(spiphy_dev->intr_context);

	return IRQ_HANDLED;

}

static int __devinit sms_probe(struct spi_device *spi)
{
	struct smsspi_dev *drv_info;
	struct cmmb_platform_data *pdata;
	int ret;

	sms_info("sms  sms_probe +++++++++\n");
	u_irq_count = 0;

	/*
	 * bits_per_word cannot be configured in platform data
	 */
	spi->bits_per_word = 8;

	ret = spi_setup(spi);
	if (ret < 0)
		goto out;

	/*
	 * Start putting together one of our big cmmb device structures.
	 */
	ret = -ENOMEM;
	drv_info = kzalloc(sizeof(struct smsspi_dev), GFP_KERNEL);
	if (drv_info == NULL)
		goto out;

	mutex_init(&drv_info->s_mutex);
	mutex_lock(&drv_info->s_mutex);

	spin_lock_init(&drv_info->dev_lock);
	INIT_LIST_HEAD(&drv_info->dev_list);
	init_waitqueue_head(&drv_info->iowait);

	drv_info->spi = spi;
	drv_info->spi_irq_enable = 0;

	pdata = drv_info->spi->dev.platform_data;
	if (!pdata)
		goto out_free;

#ifndef PIN_DEFINED_BY_DRIVE
	if (pdata->gpio_defined) {

		drv_info->gpio_power = pdata->gpio_power;
		drv_info->gpio_reset = pdata->gpio_reset;
		drv_info->gpio_cs = pdata->gpio_cs;
	} else
#endif /* in case the pdata is not set outsize the dirver */
	{
		drv_info->gpio_power = POWER_PIN_NUM;
		drv_info->gpio_reset = RESET_PIN_NUM;
		drv_info->gpio_cs = SPI_CS_PIN_NUM;
	}

	chip_alloc_dma_bufs(drv_info);

#if 1
	irq_set_irq_type(spi->irq, IRQ_TYPE_EDGE_RISING);
	/*IRQ_TYPE_EDGE_FALLING */

	ret = request_irq(spi->irq, spibus_interrupt,
			  IRQF_DISABLED, "CMMB Demodulator", drv_info);
#else
	ret = request_irq(spi->irq, spibus_interrupt,
			  IRQF_TRIGGER_RISING, "CMMB Demodulator", drv_info);
#endif
	if (ret) {
		sms_err("CMMB request irq failed.\n");
		goto out_mutex;
	}

	drv_info->smschip_status |= SMSCHAR_STATUS_NEED_REPOWER;
	mutex_unlock(&drv_info->s_mutex);

	cmmb_add_dev(drv_info);
	smsspi_save_dev((void *)drv_info);

	/* Add here temp, if smschar call the function, delete it */
	/* chip_power_on(drv_info); */

	/* chip_powerdown(); */
	/* when probe no need power on, and power off */

	sms_info("Siano Demodulator detected\n");

	return ret;
out_mutex:
	mutex_unlock(&drv_info->s_mutex);
out_free:
	kzfree(drv_info);
out:
	sms_info("Siano error out\n");

	return ret;
}

static int sms_remove(struct spi_device *spi)
{
	struct smsspi_dev *drv_info = cmmb_find_by_spi(spi);

	sms_info("sms  sms_remove +++++++++\n");

	if (drv_info == NULL) {
		sms_err("cmmb_remove on unknown spi %p\n", spi);
		return -ENODEV;
	}

	mutex_lock(&drv_info->s_mutex);
	if (drv_info->users > 0)
		sms_err("Removing a device with users!\n");

	cmmb_remove_dev(drv_info);

	chip_free_dma_bufs(drv_info);

	free_irq(spi->irq, drv_info);

	mutex_unlock(&drv_info->s_mutex);

	if (drv_info) {
		/* free the drv_info */
		kzfree(drv_info);
	}

	return 0;
}

static int smsspi_suspend(struct spi_device *spi, pm_message_t mesg)
{
	struct smsspi_dev *drv_info = cmmb_find_by_spi(spi);

	/* As during the suspend, the CMMB module's power can be power off or
	* change to low volatage,
	* After resume, the chip can't work well,
	* So set a flag here to remide user space to re init the chip
	*/
	if (!(drv_info->smschip_status & SMSCHAR_STATUS_NEED_REPOWER)) {
		/* Please unmask SW_POWERDOWN_MODE
		 * if the HW power down is supported
		 */
		smsspidrv_sw_powerdown();

		chip_power_down(drv_info);
		drv_info->smschip_status |= SMSCHAR_STATUS_NEED_REPOWER;
	}

	drv_info->smschip_status |= SMSCHAR_STATUS_NEED_REINIT;
	free_irq(spi->irq, drv_info);

	return 0;
}

static int smsspi_resume(struct spi_device *spi)
{
	int ret;
	struct smsspi_dev *drv_info = cmmb_find_by_spi(spi);

#if 1
	irq_set_irq_type(spi->irq, IRQ_TYPE_EDGE_RISING);
	/*IRQ_TYPE_EDGE_FALLING*/

	ret = request_irq(spi->irq, spibus_interrupt,
				IRQF_DISABLED,
				"CMMB Demodulator", drv_info);
#else
	ret = request_irq(spi->irq, spibus_interrupt,
				IRQF_TRIGGER_RISING,
				"CMMB Demodulator", drv_info);
#endif

	return ret;
}

static struct spi_driver sms_driver = {
	.driver = {
		   .name = DRIVER_NAME,
		   .owner = THIS_MODULE,
		   },
	.probe = sms_probe,
	.remove = __devexit_p(sms_remove),
	.suspend = smsspi_suspend,
	.resume = smsspi_resume,
};

int pxa_spi_register(void)
{
	return spi_register_driver(&sms_driver);
}

void pxa_spi_unregister(void)
{
	spi_unregister_driver(&sms_driver);
}

void smsspibus_xfer(void *context,
		    unsigned char *txbuf, unsigned long txbuf_phy_addr,
		    unsigned char *rxbuf, unsigned long rxbuf_phy_addr, int len)
{
	struct smsspi_dev *spiphy_dev = (struct smsspi_dev *)context;

	sms_spi_xfer(spiphy_dev, txbuf, len, rxbuf, len);
}

void smsspibus_ssp_suspend(void *context)
{
	sms_info("pxa_ssp_suspend.\n");
}

int smsspibus_ssp_resume(void *context)
{
	sms_info("pxa_ssp_resume.\n");
	return 0;
}

void smschipreset(void *context)
{
	struct smsspi_dev *drv_info = context;

	chip_power_reset(drv_info);
	drv_info->smschip_status &= ~SMSCHAR_STATUS_NEED_RESET;
}

void smschipon(void *context)
{
	struct smsspi_dev *drv_info = context;

	chip_power_on(drv_info);
	drv_info->smschip_status &= ~SMSCHAR_STATUS_NEED_REPOWER;
}

void smschipoff(void *context)
{
	struct smsspi_dev *drv_info = context;

	chip_power_down(drv_info);
	drv_info->smschip_status |= SMSCHAR_STATUS_NEED_REPOWER;
}

void smschip_getstatus(void *context, unsigned int *status)
{
	struct smsspi_dev *drv_info = context;

	*status = drv_info->smschip_status;
}

void smschip_clearstatus(void *context, unsigned int status)
{
	struct smsspi_dev *drv_info = context;

	drv_info->smschip_status &= ~status;
}

void *smsspiphy_init(void *context,
		     void (*smsspi_interruptHandler) (void *),
		     void *intr_context)
{
	struct smsspi_dev *drv_info;

	sms_info("enter smsspiphy_init\n");
	drv_info = smsspi_get_dev();
	if (!drv_info) {
		sms_info("spiphy_dev is null in smsspiphy_init\n");
		return NULL;
	}

	drv_info->interruptHandler = smsspi_interruptHandler;
	drv_info->intr_context = intr_context;
	drv_info->spi_irq_enable = 1;
	return drv_info;
}

int smsspiphy_deinit(void *context)
{
	struct smsspi_dev *drv_info = (struct smsspi_dev *)context;

	drv_info->interruptHandler = NULL;
	drv_info->intr_context = NULL;
	drv_info->spi_irq_enable = 0;

	smsspi_save_dev(NULL);

	sms_info("smsspiphy_deinit-\n");
	return 0;
}

void prepareForFWDnl(void *context)
{
	sms_info("NULL\n");
}

void fwDnlComplete(void *context, int App)
{
	sms_info("NULL\n");
}
