/*
 *
 * FocalTech TouchScreen driver.
 *
 * Copyright (c) 2012-2020, FocalTech Systems, Ltd., all rights reserved.
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

/************************************************************************
*
* File Name: focaltech_spi.c
*
*    Author: FocalTech Driver Team
*
*   Created: 2019-03-21
*
*  Abstract: new spi protocol communication with TP
*
*   Version: v2.0
*
* Revision History:
*
************************************************************************/

/*****************************************************************************
* Included header files
*****************************************************************************/
#include "focaltech_core.h"

/*****************************************************************************
* Private constant and macro definitions using #define
*****************************************************************************/
#define SPI_RETRY_NUMBER            3
#define CS_HIGH_DELAY               150 /* unit: us */

#define DATA_CRC_EN                 0x20
#define WRITE_CMD                   0x00
#define READ_CMD                    (0x80 | DATA_CRC_EN)

#define SPI_DUMMY_BYTE              3
#define SPI_HEADER_LENGTH           6   /*CRC*/
/*A06 code for AL7160A-153 by wenghailong at 20240426 start*/
#include <linux/platform_data/spi-mt65xx.h>
#define FT_CS_SETUPTIME        50
/*A06 code for AL7160A-153 by wenghailong at 20240426 end*/
/*****************************************************************************
* Private enumerations, structures and unions using typedef
*****************************************************************************/

/*****************************************************************************
* Static variables
*****************************************************************************/

/*****************************************************************************
* Global variable or extern global variabls/functions
*****************************************************************************/

/*****************************************************************************
* Static function prototypes
*****************************************************************************/

/*****************************************************************************
* functions body
*****************************************************************************/
/* spi interface */
static int fts_spi_transfer(u8 *tx_buf, u8 *rx_buf, u32 len)
{
    int ret = 0;
    struct spi_device *spi = fts_data->spi;
    struct spi_message msg;
    struct spi_transfer xfer = {
        .tx_buf = tx_buf,
        .rx_buf = rx_buf,
        .len    = len,
    };

    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);

    ret = spi_sync(spi, &msg);
    if (ret) {
        FTS_ERROR("spi_sync fail,ret:%d", ret);
        return ret;
    }

    return ret;
}

static void fts_spi_buf_show(u8 *data, int datalen)
{
    int i = 0;
    int count = 0;
    int size = 0;
    char *tmpbuf = NULL;

    if (!data || (datalen <= 0)) {
        FTS_ERROR("data/datalen is invalid");
        return;
    }

    size = (datalen > 256) ? 256 : datalen;
    tmpbuf = kzalloc(1024, GFP_KERNEL);
    if (!tmpbuf) {
        FTS_ERROR("tmpbuf zalloc fail");
        return;
    }

    for (i = 0; i < size; i++)
        count += snprintf(tmpbuf + count, 1024 - count, "%02X ", data[i]);

    FTS_DEBUG("%s", tmpbuf);
    if (tmpbuf) {
        kfree(tmpbuf);
        tmpbuf = NULL;
    }
}

static void crckermit(u8 *data, u32 len, u16 *crc_out)
{
    u32 i = 0;
    u32 j = 0;
    u16 crc = 0xFFFF;

    for ( i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x01)
                crc = (crc >> 1) ^ 0x8408;
            else
                crc = (crc >> 1);
        }
    }

    *crc_out = crc;
}

static int rdata_check(u8 *rdata, u32 rlen)
{
    u16 crc_calc = 0;
    u16 crc_read = 0;

    crckermit(rdata, rlen - 2, &crc_calc);
    crc_read = (u16)(rdata[rlen - 1] << 8) + rdata[rlen - 2];
    if (crc_calc != crc_read) {
        fts_spi_buf_show(rdata, rlen);
        return -EIO;
    }

    return 0;
}

int fts_write(u8 *writebuf, u32 writelen)
{
    int ret = 0;
    int i = 0;
    struct fts_ts_data *ts_data = fts_data;
    u8 *txbuf = NULL;
    u8 *rxbuf = NULL;
    u32 txlen = 0;
    u32 txlen_need = writelen + SPI_HEADER_LENGTH + ts_data->dummy_byte;
    u32 datalen = writelen - 1;

    if (!writebuf || !writelen) {
        FTS_ERROR("writebuf/len is invalid");
        return -EINVAL;
    }

    mutex_lock(&ts_data->bus_lock);
    if (txlen_need > FTS_MAX_BUS_BUF) {
        txbuf = kzalloc(txlen_need, GFP_KERNEL);
        if (NULL == txbuf) {
            FTS_ERROR("txbuf malloc fail");
            ret = -ENOMEM;
            goto err_write;
        }

        rxbuf = kzalloc(txlen_need, GFP_KERNEL);
        if (NULL == rxbuf) {
            FTS_ERROR("rxbuf malloc fail");
            ret = -ENOMEM;
            goto err_write;
        }
    } else {
        txbuf = ts_data->bus_tx_buf;
        rxbuf = ts_data->bus_rx_buf;
        memset(txbuf, 0x0, FTS_MAX_BUS_BUF);
        memset(rxbuf, 0x0, FTS_MAX_BUS_BUF);
    }

    txbuf[txlen++] = writebuf[0];
    txbuf[txlen++] = WRITE_CMD;
    txbuf[txlen++] = (datalen >> 8) & 0xFF;
    txbuf[txlen++] = datalen & 0xFF;
    if (datalen > 0) {
        txlen = txlen + SPI_DUMMY_BYTE;
        memcpy(&txbuf[txlen], &writebuf[1], datalen);
        txlen = txlen + datalen;
    }

    for (i = 0; i < SPI_RETRY_NUMBER; i++) {
        ret = fts_spi_transfer(txbuf, rxbuf, txlen);
        if ((0 == ret) && ((rxbuf[3] & 0xA0) == 0)) {
            break;
        } else {
            FTS_DEBUG("data write(addr:%x),status:%x,retry:%d,ret:%d",
                      writebuf[0], rxbuf[3], i, ret);
            ret = -EIO;
            udelay(CS_HIGH_DELAY);
        }
    }
    if (ret < 0) {
        FTS_ERROR("data write(addr:%x) fail,status:%x,ret:%d",
                  writebuf[0], rxbuf[3], ret);
    }

err_write:
    if (txlen_need > FTS_MAX_BUS_BUF) {
        if (txbuf) {
            kfree(txbuf);
            txbuf = NULL;
        }

        if (rxbuf) {
            kfree(rxbuf);
            rxbuf = NULL;
        }
    }

    udelay(CS_HIGH_DELAY);
    mutex_unlock(&ts_data->bus_lock);
    return ret;
}

int fts_write_reg(u8 addr, u8 value)
{
    u8 writebuf[2] = { 0 };

    writebuf[0] = addr;
    writebuf[1] = value;
    return fts_write(writebuf, 2);
}

int fts_read(u8 *cmd, u32 cmdlen, u8 *data, u32 datalen)
{
    int ret = 0;
    int i = 0;
    struct fts_ts_data *ts_data = fts_data;
    u8 *txbuf = NULL;
    u8 *rxbuf = NULL;
    u32 txlen = 0;
    u32 txlen_need = datalen + SPI_HEADER_LENGTH + ts_data->dummy_byte;
    u8 ctrl = READ_CMD;
    u32 dp = 0;

    if (!cmd || !cmdlen || !data || !datalen) {
        FTS_ERROR("cmd/cmdlen/data/datalen is invalid");
        return -EINVAL;
    }

    mutex_lock(&ts_data->bus_lock);
    if (txlen_need > FTS_MAX_BUS_BUF) {
        txbuf = kzalloc(txlen_need, GFP_KERNEL);
        if (NULL == txbuf) {
            FTS_ERROR("txbuf malloc fail");
            ret = -ENOMEM;
            goto err_read;
        }

        rxbuf = kzalloc(txlen_need, GFP_KERNEL);
        if (NULL == rxbuf) {
            FTS_ERROR("rxbuf malloc fail");
            ret = -ENOMEM;
            goto err_read;
        }
    } else {
        txbuf = ts_data->bus_tx_buf;
        rxbuf = ts_data->bus_rx_buf;
        memset(txbuf, 0x0, FTS_MAX_BUS_BUF);
        memset(rxbuf, 0x0, FTS_MAX_BUS_BUF);
    }

    txbuf[txlen++] = cmd[0];
    txbuf[txlen++] = ctrl;
    txbuf[txlen++] = (datalen >> 8) & 0xFF;
    txbuf[txlen++] = datalen & 0xFF;
    dp = txlen + SPI_DUMMY_BYTE;
    txlen = dp + datalen;
    if (ctrl & DATA_CRC_EN) {
        txlen = txlen + 2;
    }

    for (i = 0; i < SPI_RETRY_NUMBER; i++) {
        ret = fts_spi_transfer(txbuf, rxbuf, txlen);
        if ((0 == ret) && ((rxbuf[3] & 0xA0) == 0)) {
            memcpy(data, &rxbuf[dp], datalen);
            /* crc check */
            if (ctrl & DATA_CRC_EN) {
                ret = rdata_check(&rxbuf[dp], txlen - dp);
                if (ret < 0) {
                    FTS_DEBUG("data read(addr:%x) crc abnormal,retry:%d",
                              cmd[0], i);
                    udelay(CS_HIGH_DELAY);
                    continue;
                }
            }
            break;
        } else {
            FTS_DEBUG("data read(addr:%x) status:%x,retry:%d,ret:%d",
                      cmd[0], rxbuf[3], i, ret);
            ret = -EIO;
            udelay(CS_HIGH_DELAY);
        }
    }

    if (ret < 0) {
        FTS_ERROR("data read(addr:%x) %s,status:%x,ret:%d", cmd[0],
                  (i >= SPI_RETRY_NUMBER) ? "crc abnormal" : "fail",
                  rxbuf[3], ret);
    }

err_read:
    if (txlen_need > FTS_MAX_BUS_BUF) {
        if (txbuf) {
            kfree(txbuf);
            txbuf = NULL;
        }

        if (rxbuf) {
            kfree(rxbuf);
            rxbuf = NULL;
        }
    }

    udelay(CS_HIGH_DELAY);
    mutex_unlock(&ts_data->bus_lock);
    return ret;
}

int fts_read_reg(u8 addr, u8 *value)
{
    return fts_read(&addr, 1, value, 1);
}


int fts_bus_transfer_direct(u8 *writebuf, u32 writelen, u8 *readbuf, u32 readlen)
{
    int ret = 0;
    struct fts_ts_data *ts_data = fts_data;
    u8 *txbuf = NULL;
    u8 *rxbuf = NULL;
    bool read_cmd = (readbuf && readlen) ? 1 : 0;
    u32 txlen = (read_cmd) ? readlen : writelen;

    if (!writebuf || !writelen) {
        FTS_ERROR("writebuf/len is invalid");
        return -EINVAL;
    }

    mutex_lock(&ts_data->bus_lock);
    if (txlen > FTS_MAX_BUS_BUF) {
        txbuf = kzalloc(txlen, GFP_KERNEL);
        if (NULL == txbuf) {
            FTS_ERROR("txbuf malloc fail");
            ret = -ENOMEM;
            goto err_spi_dir;
        }

        rxbuf = kzalloc(txlen, GFP_KERNEL);
        if (NULL == rxbuf) {
            FTS_ERROR("rxbuf malloc fail");
            ret = -ENOMEM;
            goto err_spi_dir;
        }
    } else {
        txbuf = ts_data->bus_tx_buf;
        rxbuf = ts_data->bus_rx_buf;
        memset(txbuf, 0x0, FTS_MAX_BUS_BUF);
        memset(rxbuf, 0x0, FTS_MAX_BUS_BUF);
    }

    memcpy(txbuf, writebuf, writelen);
    ret = fts_spi_transfer(txbuf, rxbuf, txlen);
    if (ret < 0) {
        FTS_ERROR("data read(addr:%x) fail,status:%x,ret:%d", txbuf[0], rxbuf[3], ret);
        goto err_spi_dir;
    }

    if (read_cmd) {
        memcpy(readbuf, rxbuf, txlen);
    }

    ret = 0;
err_spi_dir:
    if (txlen > FTS_MAX_BUS_BUF) {
        if (txbuf) {
            kfree(txbuf);
            txbuf = NULL;
        }

        if (rxbuf) {
            kfree(rxbuf);
            rxbuf = NULL;
        }
    }

    udelay(CS_HIGH_DELAY);
    mutex_unlock(&ts_data->bus_lock);
    return ret;
}

int fts_bus_configure(struct fts_ts_data *ts_data, u8 *buf, u32 size)
{
    int ret = 0;
    FTS_FUNC_ENTER();
    if (ts_data->spi && buf && size) {
        ts_data->spi->mode = buf[0];
        ts_data->spi->bits_per_word = buf[1];
        ts_data->spi->max_speed_hz = *(u32 *)(buf + 3);
        FTS_INFO("spi,mode=%d,bits=%d,speed=%d", ts_data->spi->mode,
                 ts_data->spi->bits_per_word, ts_data->spi->max_speed_hz);
        ret = spi_setup(ts_data->spi);
        if (ret < 0) {
            FTS_ERROR("spi setup fail,ret:%d", ret);
        }
    }
    FTS_FUNC_EXIT();
    return ret;
}

/*A06 code for SR-AL7160A-01-91 by yuli at 20240411 start*/
static void fts_usb_notify_work(struct work_struct *work)
{
    if (NULL == work) {
        FTS_ERROR("parameter work is null!");
        return;
    }

    if (fts_data->charger_mode == true) {
        fts_ex_mode_switch(MODE_CHARGER, ENABLE);
        FTS_INFO("Charger Mode:%s", fts_data->charger_mode ? "On" : "Off");
    } else if (fts_data->charger_mode == false) {
        fts_ex_mode_switch(MODE_CHARGER, DISABLE);
        FTS_INFO("Charger Mode:%s", fts_data->charger_mode ? "On" : "Off");
    }
}

static int fts_usb_notifier_callback(struct notifier_block *this,unsigned long event, void *v)
{

    if (fts_data == NULL) {
        FTS_ERROR("fts_data is NULL");
        return -EFAULT;
    }

    if (event == TP_USB_PLUGIN_STATE) {
        FTS_INFO("USB plugin");
        if (fts_data->charger_mode) {
            FTS_INFO("Already in Charger Mode:%s, no need to update", fts_data->charger_mode ? "On" : "Off");
            return 0;
        } else {
            fts_data->charger_mode = true;
        }
    } else if (event == TP_USB_PLUGOUT_STATE) {
        FTS_INFO("USB plugout");
        if (!fts_data->charger_mode) {
            FTS_INFO("Already in Charger Mode:%s, no need to update", fts_data->charger_mode ? "On" : "Off");
            return 0;
        } else {
            fts_data->charger_mode = false;
        }
    }

    if ((!fts_data->suspended) && (fts_data->tp_usb_notify_wq != NULL)) {
        queue_work(fts_data->tp_usb_notify_wq, &fts_data->tp_usb_notify_work);
    }

    return 0;
}

static void fts_earphone_notify_work(struct work_struct *work)
{
    if (NULL == work) {
        FTS_ERROR("%s:  parameter work are null!", __func__);
        return;
    }

    if (fts_data->earphone_mode == true) {
        fts_ex_mode_switch(MODE_EARPHONE, ENABLE);
        FTS_INFO("Earphone Mode:%s", fts_data->earphone_mode ? "On" : "Off");
    } else if (fts_data->earphone_mode == false) {
        fts_ex_mode_switch(MODE_EARPHONE, DISABLE);
        FTS_INFO("Earphone Mode:%s", fts_data->earphone_mode ? "On" : "Off");
    }
}

/**
*earphone_notifier
*Param：struct notifier_block *nb, unsigned long event, void *ptr
*Return：earphone result
*Purpose：earphone notifier callback
*/
static int fts_earphone_notifier_callback(struct notifier_block *nb, unsigned long event, void *ptr)
{
    if (fts_data->earphone_mode != event) {
        if (event == HEADSET_PLUGIN_STATE) {
            fts_data->earphone_mode = true;
            FTS_INFO("earphone plugin");
        } else if (event == HEADSET_PLUGOUT_STATE) {
            fts_data->earphone_mode = false;
            FTS_INFO("earphone plugout");
        } else {
            FTS_ERROR("Illegal events, ignore");
            return 0;
        }

        if ((!fts_data->suspended) && (fts_data->tp_earphone_notify_wq != NULL)) {
            queue_work(fts_data->tp_earphone_notify_wq, &fts_data->tp_earphone_notify_work);
        }
    } else {
        FTS_INFO("event: %s. Already in Earphone Mode:%s, no need to update",
            event ? "earphone plugin" : "earphone plugout" , fts_data->earphone_mode ? "On" : "Off");
    }

    return 0;
}
/*A06 code for SR-AL7160A-01-91 by yuli at 20240411 end*/

/*A06 code for AL7160A-153 by wenghailong at 20240426 start*/
static struct mtk_chip_config spi_ctrdata = {
    .cs_setuptime = FT_CS_SETUPTIME,   //0.5us == 50*9.6 ns    //for FT8057P
};
/*A06 code for AL7160A-153 by wenghailong at 20240426 end*/

/*****************************************************************************
* TP Driver
*****************************************************************************/
static int fts_ts_probe(struct spi_device *spi)
{
    int ret = 0;
    struct fts_ts_data *ts_data = NULL;

    FTS_INFO("Touch Screen(SPI-2 BUS) driver prboe...");
    spi->mode = SPI_MODE_0;
    /*A06 code for AL7160A-153 by wenghailong at 20240426 start*/
    spi->controller_data = (void *)&spi_ctrdata;
    /*A06 code for AL7160A-153 by wenghailong at 20240426 end*/
    spi->bits_per_word = 8;
    ret = spi_setup(spi);
    if (ret < 0) {
        FTS_ERROR("spi setup fail");
        return ret;
    }

    /* malloc memory for global struct variable */
    ts_data = (struct fts_ts_data *)kzalloc(sizeof(*ts_data), GFP_KERNEL);
    if (!ts_data) {
        FTS_ERROR("allocate memory for fts_data fail");
        return -ENOMEM;
    }

    ts_data->spi = spi;
    ts_data->dev = &spi->dev;
    ts_data->log_level = 1;
    ts_data->bus_type = BUS_TYPE_SPI;
    ts_data->bus_ver = BUS_VER_V2;
    ts_data->dummy_byte = SPI_DUMMY_BYTE;
    spi_set_drvdata(spi, ts_data);

    ret = fts_ts_probe_entry(ts_data);
    if (ret) {
        FTS_ERROR("Touch Screen(SPI BUS) driver probe fail");
        spi_set_drvdata(spi, NULL);
        kfree_safe(ts_data);
        return ret;
    }

    /*A06 code for SR-AL7160A-01-91 by yuli at 20240411 start*/
    ts_data->tp_usb_notify_wq = create_singlethread_workqueue("fts_usb_wq");
    if (!ts_data->tp_usb_notify_wq) {
        FTS_INFO("allocate tp_usb_notify_wq failed\n");
    }
    INIT_WORK(&ts_data->tp_usb_notify_work, fts_usb_notify_work);
    ts_data->tp_usb_notify.notifier_call = fts_usb_notifier_callback;
    ret = register_usb_check_notifier(&ts_data->tp_usb_notify);
    if (ret) {
        FTS_ERROR("register_usb_check_notifier fail: %d\n", ret);
    } else {
        FTS_INFO("register_usb_check_notifier successfully");
    }

    ts_data->tp_earphone_notify_wq = create_singlethread_workqueue("fts_earphone_wq");
    if (!ts_data->tp_earphone_notify_wq) {
        FTS_INFO("allocate tp_earphone_notify_wq failed!");
    }
    INIT_WORK(&ts_data->tp_earphone_notify_work, fts_earphone_notify_work);
    ts_data->tp_earphone_notify.notifier_call = fts_earphone_notifier_callback;
    FTS_INFO("earphone blocking_notifier_chain_register!");
    ret = headset_notifier_register(&ts_data->tp_earphone_notify);
    if (ret) {
        FTS_ERROR("headset_notifier_register fail: %d\n", ret);
    } else {
        FTS_INFO("headset_notifier_register successfully");
    }
    /*A06 code for SR-AL7160A-01-91 by yuli at 20240411 end*/

    FTS_INFO("Touch Screen(SPI BUS) driver prboe successfully");
    return 0;
}

static int fts_ts_remove(struct spi_device *spi)
{
    struct fts_ts_data *ts_data = spi_get_drvdata(spi);
    FTS_FUNC_ENTER();
    if (ts_data) {
        fts_ts_remove_entry(ts_data);
        spi_set_drvdata(spi, NULL);
        kfree_safe(ts_data);
    }
    FTS_FUNC_EXIT();
    return 0;
}

#if IS_ENABLED(CONFIG_PM) && FTS_PATCH_COMERR_PM
static int fts_pm_suspend(struct device *dev)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    ts_data->pm_suspend = true;
    reinit_completion(&ts_data->pm_completion);
    return 0;
}

static int fts_pm_resume(struct device *dev)
{
    struct fts_ts_data *ts_data = dev_get_drvdata(dev);

    ts_data->pm_suspend = false;
    complete(&ts_data->pm_completion);
    return 0;
}

static const struct dev_pm_ops fts_dev_pm_ops = {
    .suspend = fts_pm_suspend,
    .resume = fts_pm_resume,
};
#endif

static const struct spi_device_id fts_ts_id[] = {
    {FTS_DRIVER_NAME, 0},
    {},
};
static const struct of_device_id fts_dt_match[] = {
    {.compatible = "focaltech,fts", },
    {},
};
MODULE_DEVICE_TABLE(of, fts_dt_match);

static struct spi_driver fts_ts_spi_driver = {
    .probe = fts_ts_probe,
    .remove = fts_ts_remove,
    .driver = {
        .name = FTS_DRIVER_NAME,
        .owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_PM) && FTS_PATCH_COMERR_PM
        .pm = &fts_dev_pm_ops,
#endif
        .of_match_table = of_match_ptr(fts_dt_match),
    },
    .id_table = fts_ts_id,
};

static int __init fts_ts_spi_init(void)
{
    int ret = 0;

    FTS_FUNC_ENTER();
    /*A06 code for SR-AL7160A-01-102  by yuli at 20240409 start*/
    if (tp_detect_panel("ft8057p")) {
        FTS_INFO("No need to load ft8057p touch driver");
        return 0;
    } else {
        ret = spi_register_driver(&fts_ts_spi_driver);
        if ( ret != 0 ) {
            FTS_ERROR("Focaltech touch screen driver init failed!");
        }
    }
    /*A06 code for SR-AL7160A-01-102  by yuli at 20240409 end*/
    FTS_FUNC_EXIT();
    return ret;
}

static void __exit fts_ts_spi_exit(void)
{
    spi_unregister_driver(&fts_ts_spi_driver);
}

module_init(fts_ts_spi_init);
module_exit(fts_ts_spi_exit);

MODULE_AUTHOR("FocalTech Driver Team");
MODULE_DESCRIPTION("FocalTech Touchscreen Driver(SPI)");
MODULE_LICENSE("GPL v2");
