#include "jadard_platform.h"
#include "jadard_module.h"

static struct spi_device *spi = NULL;
extern struct jadard_module_fp g_module_fp;
extern struct jadard_ts_data *pjadard_ts_data;
extern struct jadard_ic_data *pjadard_ic_data;

/* HS03 code for SL6215DEV-4194 by duanyaoming at 20220411 start */
#ifdef JD_ZERO_FLASH
extern bool jadard_fw_ready;
#endif
/* HS03 code for SL6215DEV-4194 by duanyaoming at 20220411 end */

/*HS03 code for SL6215DEV-1404 by chenyihong at 20210922 start*/
/**
*Name：jadard_input_open
*Author：chenyihong
*Date：2021/09/18
*Param：struct input_dev *dev
*Return：0--success
*Purpose：Control input device enable
*/
int jadard_input_open(struct input_dev *dev)
{
    pjadard_ts_data->tp_on_off = 1;
    JD_I("jadard_input_open:input enable\n");

    return 0;
}

/**
*Name：jadard_input_close
*Author：chenyihong
*Date：2021/09/18
*Param：struct input_dev *dev
*Return：void
*Purpose：Control input device enable
*/
void jadard_input_close(struct input_dev *dev)
{
    pjadard_ts_data->tp_on_off = 0;
    JD_I("jadard_input_open:input disable\n");
}

int jadard_dev_set(struct jadard_ts_data *ts)
{
    int ret = 0;
    ts->input_dev = input_allocate_device();

    if (ts->input_dev == NULL) {
        ret = -ENOMEM;
        JD_E("%s: Failed to allocate input device\n", __func__);
        return ret;
    }

    ts->input_dev->name = "jadard-touchscreen";
    ts->tp_on_off = 1;
    ts->input_dev->open = jadard_input_open;
    ts->input_dev->close = jadard_input_close;
    return ret;
}
/*HS03 code for SL6215DEV-1404 by chenyihong at 20210922 end*/

int jadard_input_register_device(struct input_dev *input_dev)
{
    return input_register_device(input_dev);
}

int jadard_parse_dt(struct jadard_ts_data *ts,
                    struct jadard_i2c_platform_data *pdata)
{
    int coords_size;
    uint32_t coords[4] = {0};
    uint32_t ret, data;
    struct property *prop = NULL;
    struct device_node *dt = ts->dev->of_node;

       ret = of_property_read_u32(dt, "jadard,panel-max-points", &data);
    pjadard_ic_data->JD_MAX_PT = (!ret ? data : 10);
       ret = of_property_read_u32(dt, "jadard,int-is-edge", &data);
    pjadard_ic_data->JD_INT_EDGE = (!ret ? (data > 0 ? true : false) : true);

    JD_I("DT:MAX_PT = %d, INT_IS_EDGE = %d\n", pjadard_ic_data->JD_MAX_PT,
        pjadard_ic_data->JD_INT_EDGE);

    prop = of_find_property(dt, "jadard,panel-sense-nums", NULL);
    if (prop) {
        coords_size = prop->length / sizeof(uint32_t);

        if (coords_size != 2) {
            JD_E("%s:Invalid panel sense number size %d\n", __func__, coords_size);
            return -EINVAL;
        }
    }

    if (of_property_read_u32_array(dt, "jadard,panel-sense-nums", coords, coords_size) == 0) {
        pjadard_ic_data->JD_X_NUM = coords[0];
        pjadard_ic_data->JD_Y_NUM = coords[1];
        JD_I("DT:panel-sense-num = %d, %d\n",
            pjadard_ic_data->JD_X_NUM, pjadard_ic_data->JD_Y_NUM);
    }

    prop = of_find_property(dt, "jadard,panel-coords", NULL);
    if (prop) {
        coords_size = prop->length / sizeof(uint32_t);

        if (coords_size != 4) {
            JD_E("%s:Invalid panel coords size %d\n", __func__, coords_size);
            return -EINVAL;
        }
    }

    if (of_property_read_u32_array(dt, "jadard,panel-coords", coords, coords_size) == 0) {
        pdata->abs_x_min = coords[0];
        pdata->abs_x_max = coords[1];
        pdata->abs_y_min = coords[2];
        pdata->abs_y_max = coords[3];
        pjadard_ic_data->JD_X_RES = pdata->abs_x_max;
        pjadard_ic_data->JD_Y_RES = pdata->abs_y_max;

        JD_I("DT:panel-coords = %d, %d, %d, %d\n", pdata->abs_x_min,
            pdata->abs_x_max, pdata->abs_y_min, pdata->abs_y_max);
    }

    pdata->gpio_irq = of_get_named_gpio(dt, "jadard,irq-gpio", 0);
    if (!gpio_is_valid(pdata->gpio_irq)) {
        JD_I("DT:gpio_irq value is not valid\n");
    }

    pdata->gpio_reset = of_get_named_gpio(dt, "jadard,rst-gpio", 0);
    if (!gpio_is_valid(pdata->gpio_reset)) {
        JD_I("DT:gpio_rst value is not valid\n");
    }

    JD_I("DT:gpio_irq = %d, gpio_rst = %d\n", pdata->gpio_irq, pdata->gpio_reset);

    return 0;
}

int jadard_bus_read(uint8_t *cmd, uint8_t cmd_len, uint8_t *data, uint32_t data_len, uint8_t toRetry)
{
    struct spi_message m;
    struct spi_transfer t[2];
    int retry;
    int error;

    mutex_lock(&(pjadard_ts_data->spi_lock));
    memset(&t, 0, sizeof(t));

    spi_message_init(&m);
    t[0].tx_buf = cmd;
    t[0].len = cmd_len;
    spi_message_add_tail(&t[0], &m);

    t[1].rx_buf = data;
    t[1].len = data_len;
    spi_message_add_tail(&t[1], &m);

    /*HS03 code for SL6215DEV-2515 by chenyihong at 20211020 start*/
    for (retry = 0; retry < toRetry; retry++) {
        error = spi_sync(pjadard_ts_data->spi, &m);
        if (unlikely(error)) {
            JD_E("SPI read error: %d\n", error);
            mdelay(2);
        } else {
            break;
        }
    }
    /*HS03 code for SL6215DEV-2515 by chenyihong at 20211020 end*/

    if (retry == toRetry) {
        JD_E("%s: SPI read error retry over %d\n",
            __func__, toRetry);
        mutex_unlock(&(pjadard_ts_data->spi_lock));
        return -EIO;
    }

    mutex_unlock(&(pjadard_ts_data->spi_lock));

    return 0;
}

static int jadard_spi_write(struct spi_device *spi, uint8_t *buf, uint32_t length)
{
    int status;
    struct spi_message m;
    struct spi_transfer	t = {
            .tx_buf		= buf,
            .len		= length,
    };

    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    status = spi_sync(spi, &m);

    if (status == 0) {
        status = m.status;
        if (status == 0)
            status = m.actual_length;
    }

    return status;
}

/*HS03 code for SL6215DEV-2515 by chenyihong at 20211020 start*/
int jadard_bus_write(uint8_t *cmd, uint8_t cmd_len, uint8_t *data, uint32_t data_len, uint8_t toRetry)
{
    uint8_t *buf = NULL;
    int status;
    int retry;

    mutex_lock(&(pjadard_ts_data->spi_lock));

    buf = kzalloc((cmd_len + data_len) * sizeof(uint8_t), GFP_KERNEL);

    /*HS03 code for SL6215DEV-3729 by chenyihong at 20211129 start*/
    if (buf == NULL) {
        JD_E("%s: Memory alloc fail\n", __func__);
        mutex_unlock(&(pjadard_ts_data->spi_lock));
        return -ENOMEM;
    }
    /*HS03 code for SL6215DEV-3729 by chenyihong at 20211129 end*/

    memcpy(buf, cmd, cmd_len);
    memcpy(buf + cmd_len, data, data_len);

    for (retry = 0; retry < toRetry; retry++) {
        status = jadard_spi_write(pjadard_ts_data->spi, buf, cmd_len + data_len);
        if (status < 0) {
            JD_E("SPI write error: %d\n", status);
            mdelay(2);
        } else {
            break;
        }
    }
    kfree(buf);

    if (retry == toRetry) {
        JD_E("%s: SPI write error retry over %d\n",
            __func__, toRetry);
        mutex_unlock(&(pjadard_ts_data->spi_lock));
        return -EIO;
    }

    mutex_unlock(&(pjadard_ts_data->spi_lock));

    return 0;
}
/*HS03 code for SL6215DEV-2515 by chenyihong at 20211020 end*/

void jadard_int_enable(bool enable)
{
    int irqnum = pjadard_ts_data->jd_irq;

    if (enable && (pjadard_ts_data->irq_enabled == 0)) {
        enable_irq(irqnum);
        pjadard_ts_data->irq_enabled = 1;
    } else if ((!enable) && (pjadard_ts_data->irq_enabled == 1)) {
        disable_irq_nosync(irqnum);
        pjadard_ts_data->irq_enabled = 0;
    }

    JD_I("irq_enable = %d\n", pjadard_ts_data->irq_enabled);
}

#ifdef JD_RST_PIN_FUNC
void jadard_gpio_set_value(int pin_num, uint8_t value)
{
    gpio_set_value(pin_num, value);
}
#endif

#if defined(CONFIG_JD_DB)
static int jadard_regulator_configure(struct jadard_i2c_platform_data *pdata)
{
    int retval;

    pdata->vcc_dig = regulator_get(pjadard_ts_data->dev, "vdd");

    if (IS_ERR(pdata->vcc_dig)) {
        JD_E("%s: Failed to get regulator vdd\n",
          __func__);
        retval = PTR_ERR(pdata->vcc_dig);
        return retval;
    }

    pdata->vcc_ana = regulator_get(pjadard_ts_data->dev, "avdd");

    if (IS_ERR(pdata->vcc_ana)) {
        JD_E("%s: Failed to get regulator avdd\n",
          __func__);
        retval = PTR_ERR(pdata->vcc_ana);
        regulator_put(pdata->vcc_ana);
        return retval;
    }

    return 0;
};

static int jadard_power_on(struct jadard_i2c_platform_data *pdata, bool on)
{
    int retval;

    if (on) {
        retval = regulator_enable(pdata->vcc_dig);

        if (retval) {
            JD_E("%s: Failed to enable regulator vdd\n",
              __func__);
            return retval;
        }

        msleep(100);
        retval = regulator_enable(pdata->vcc_ana);

        if (retval) {
            JD_E("%s: Failed to enable regulator avdd\n",
              __func__);
            regulator_disable(pdata->vcc_dig);
            return retval;
        }
    } else {
        regulator_disable(pdata->vcc_dig);
        regulator_disable(pdata->vcc_ana);
    }

    return 0;
}

int jadard_gpio_power_config(struct jadard_i2c_platform_data *pdata)
{
    int error = jadard_regulator_configure(pdata);

    if (error) {
        JD_E("Failed to intialize hardware\n");
        goto err_regulator_not_on;
    }

#ifdef JD_RST_PIN_FUNC
    if (gpio_is_valid(pdata->gpio_reset)) {
        error = gpio_request(pdata->gpio_reset, "jadard_reset_gpio");

        if (error) {
            JD_E("unable to request gpio [%d]\n",
              pdata->gpio_reset);
            goto err_regulator_on;
        }

        error = gpio_direction_output(pdata->gpio_reset, 0);
        if (error) {
            JD_E("unable to set direction for gpio [%d]\n",
              pdata->gpio_reset);
            goto err_gpio_reset_req;
        }
    }
#endif

    error = jadard_power_on(pdata, true);
    if (error) {
        JD_E("Failed to power on hardware\n");
        goto err_gpio_reset_req;
    }

    if (gpio_is_valid(pdata->gpio_irq)) {
        error = gpio_request(pdata->gpio_irq, "jadard_gpio_irq");

        if (error) {
            JD_E("unable to request gpio [%d]\n",
              pdata->gpio_irq);
            goto err_power_on;
        }

        error = gpio_direction_input(pdata->gpio_irq);
        if (error) {
            JD_E("unable to set direction for gpio [%d]\n",
              pdata->gpio_irq);
            goto err_gpio_irq_req;
        }

        pjadard_ts_data->jd_irq = gpio_to_irq(pdata->gpio_irq);
    } else {
        JD_E("irq gpio not provided\n");
        goto err_power_on;
    }

    msleep(20);
#ifdef JD_RST_PIN_FUNC
    if (gpio_is_valid(pdata->gpio_reset)) {
        error = gpio_direction_output(pdata->gpio_reset, 1);

        if (error) {
            JD_E("unable to set direction for gpio [%d]\n",
              pdata->gpio_reset);
            goto err_gpio_irq_req;
        }
    }
#endif

    return 0;

err_gpio_irq_req:
    if (gpio_is_valid(pdata->gpio_irq))
        gpio_free(pdata->gpio_irq);

err_power_on:
    jadard_power_on(pdata, false);
err_gpio_reset_req:
#ifdef JD_RST_PIN_FUNC
    if (gpio_is_valid(pdata->gpio_reset))
        gpio_free(pdata->gpio_reset);

err_regulator_on:
#endif
err_regulator_not_on:
    return error;
}

#else
int jadard_gpio_power_config(struct jadard_i2c_platform_data *pdata)
{
    int error = 0;

#ifdef JD_RST_PIN_FUNC
    if (pdata->gpio_reset >= 0) {
        error = gpio_request(pdata->gpio_reset, "jadard_reset_gpio");

        if (error < 0) {
            JD_E("%s: request reset pin failed\n", __func__);
            return error;
        }

        error = gpio_direction_output(pdata->gpio_reset, 0);

        if (error) {
            JD_E("unable to set direction for gpio [%d]\n",
              pdata->gpio_reset);
            return error;
        }
    }
#endif

    if (gpio_is_valid(pdata->gpio_irq)) {
        /* configure touchscreen irq gpio */
        error = gpio_request(pdata->gpio_irq, "jadard_gpio_irq");

        if (error) {
            JD_E("unable to request gpio [%d]\n", pdata->gpio_irq);
            return error;
        }

        error = gpio_direction_input(pdata->gpio_irq);

        if (error) {
            JD_E("unable to set direction for gpio [%d]\n", pdata->gpio_irq);
            return error;
        }

        pjadard_ts_data->jd_irq = gpio_to_irq(pdata->gpio_irq);
    } else {
        JD_E("irq gpio not provided\n");
        return error;
    }

    msleep(20);
#ifdef JD_RST_PIN_FUNC
    if (pdata->gpio_reset >= 0) {
        error = gpio_direction_output(pdata->gpio_reset, 1);

        if (error) {
            JD_E("unable to set direction for gpio [%d]\n",
              pdata->gpio_reset);
            return error;
        }
    }
#endif

    return error;
}

#endif

irqreturn_t jadard_ts_isr_func(int irq, void *ptr)
{
    /* HS03 code for SL6215DEV-4194 by duanyaoming at 20220411 start */
#ifdef JD_ZERO_FLASH
    if (jadard_fw_ready == true) {
        jadard_ts_work((struct jadard_ts_data *)ptr);
    }
#else
    jadard_ts_work((struct jadard_ts_data *)ptr);
#endif
    /* HS03 code for SL6215DEV-4194 by duanyaoming at 20220411 end */
    return IRQ_HANDLED;
}

/*HS03 code for P211028-04422  by chenyihong at 20211028 start*/
int jadard_int_register_trigger(void)
{
    int ret = JD_NO_ERR;
    struct jadard_ts_data *ts = pjadard_ts_data;

    if (pjadard_ic_data->JD_INT_EDGE) {
        JD_I("%s edge triiger falling\n", __func__);
        ret = request_threaded_irq(ts->jd_irq, NULL, jadard_ts_isr_func,
                                    IRQF_TRIGGER_FALLING | IRQF_ONESHOT, JADARD_common_NAME, ts);
    } else {
        JD_I("%s level trigger low\n", __func__);
        ret = request_threaded_irq(ts->jd_irq, NULL, jadard_ts_isr_func,
                                    IRQF_TRIGGER_LOW | IRQF_ONESHOT, JADARD_common_NAME, ts);
    }

    return ret;
}
/*HS03 code for P211028-04422  by chenyihong at 20211028 end*/

void jadard_int_en_set(bool enable)
{
    struct jadard_ts_data *ts = pjadard_ts_data;

    if (enable) {
        if (jadard_int_register_trigger() == 0) {
            ts->irq_enabled = 1;
        }
    } else {
        jadard_int_enable(false);
        free_irq(ts->jd_irq, ts);
    }
}

int jadard_ts_register_interrupt(void)
{
    struct jadard_ts_data *ts = pjadard_ts_data;
    int ret = 0;
    ts->irq_enabled = 0;

    /* Work functon */
    if (ts->jd_irq) {
        ret = jadard_int_register_trigger();

        if (ret == 0) {
            ts->irq_enabled = 1;
            JD_I("%s: irq enabled at qpio: %d\n", __func__, ts->jd_irq);
        } else {
            JD_E("%s: request_irq failed\n", __func__);
        }
    } else {
        JD_I("%s: ts->jd_irq is empty.\n", __func__);
    }

    return ret;
}

static int jadard_common_suspend(struct device *dev)
{
    struct jadard_ts_data *ts = dev_get_drvdata(dev);

    JD_I("%s: enter \n", __func__);
    jadard_chip_common_suspend(ts);

    return 0;
}

static int jadard_common_resume(struct device *dev)
{
    struct jadard_ts_data *ts = dev_get_drvdata(dev);

    JD_I("%s: enter \n", __func__);
    jadard_chip_common_resume(ts);

    return 0;
}

#if defined(CONFIG_FB)
int jadard_fb_notifier_callback(struct notifier_block *self,
                            unsigned long event, void *data)
{
    struct fb_event *evdata = data;
    int *blank;
    struct jadard_ts_data *ts =
        container_of(self, struct jadard_ts_data, fb_notif);
    JD_I("%s\n", __func__);

    if (evdata && evdata->data && event == FB_EVENT_BLANK && ts != NULL &&
        ts->dev != NULL) {
        blank = evdata->data;

        switch (*blank) {
        case FB_BLANK_UNBLANK:
            jadard_common_resume(ts->dev);
            break;

        case FB_BLANK_POWERDOWN:
        case FB_BLANK_HSYNC_SUSPEND:
        case FB_BLANK_VSYNC_SUSPEND:
        case FB_BLANK_NORMAL:
            jadard_common_suspend(ts->dev);
            break;
        }
    }

    return 0;
}
#endif

int jadard_chip_common_probe(struct spi_device *spi)
{
    struct jadard_ts_data *ts = NULL;

    JD_I("Enter %s \n", __func__);
    if (spi->master->flags & SPI_MASTER_HALF_DUPLEX) {
        dev_err(&spi->dev,
                "%s: Full duplex not supported by host\n", __func__);
        return -EIO;
    }

    ts = kzalloc(sizeof(struct jadard_ts_data), GFP_KERNEL);
    if (ts == NULL) {
        JD_E("%s: allocate jadard_ts_data failed\n", __func__);
        return -ENOMEM;
    }

    pjadard_ts_data = ts;
    spi->bits_per_word = 8;
    spi->mode = SPI_MODE_0;

    ts->spi = spi;
    mutex_init(&(ts->spi_lock));
    ts->dev = &spi->dev;
    dev_set_drvdata(&spi->dev, ts);
    spi_set_drvdata(spi, ts);

    /* HS03 code for SL6215DEV-4194 by duanyaoming at 20220411 start */
#ifdef JD_ZERO_FLASH
    jadard_fw_ready = false;
#endif
    /* HS03 code for SL6215DEV-4194 by duanyaoming at 20220411 end */
    return jadard_chip_common_init();
}

int jadard_chip_common_remove(struct spi_device *spi)
{
    struct jadard_ts_data *ts = spi_get_drvdata(spi);

    ts->spi = NULL;
    /* spin_unlock_irq(&ts->spi_lock); */
    spi_set_drvdata(spi, NULL);

    jadard_chip_common_deinit();

    return 0;
}

static const struct dev_pm_ops jadard_common_pm_ops = {
#if (!defined(CONFIG_FB))
    .suspend = jadard_common_suspend,
    .resume  = jadard_common_resume,
#endif
};

#ifdef CONFIG_OF
static struct of_device_id jadard_match_table[] = {
    {.compatible = "jadard,jdcommon" },
    {},
};
#else
#define jadard_match_table NULL
#endif
static struct spi_driver jadard_common_driver = {
    .driver = {
        .name =		JADARD_common_NAME,
        .owner =	THIS_MODULE,
        .of_match_table = jadard_match_table,
    },
    .probe =	jadard_chip_common_probe,
    .remove =	jadard_chip_common_remove,
};

static int __init jadard_common_init(void)
{
    /* HS03 code for SR-SL6215-01-1169 by duanyaoming at 20220409 start */
    if (lcd_name) {
        if (NULL == strstr(lcd_name,"jd9365t")) {
            JD_I("it is not jadard tp\n");
            return -ENODEV;
        } else {
            JD_I("Jadard common touch panel driver init\n");
            spi_register_driver(&jadard_common_driver);
        }
    } else {
         JD_I("lcd_name is null\n");
         return -ENODEV;
    }

    /* HS03 code for SR-SL6215-01-1169 by duanyaoming at 20220409 end */
    return 0;
}

#if defined(CONFIG_JD_DB)
void jadard_workarround_init(void)
{
    JD_I("jadard_workarround_init by Driver\n");
    spi_register_driver(&jadard_common_driver);
}
#endif

static void __exit jadard_common_exit(void)
{
    if (spi) {
        spi_unregister_device(spi);
        spi = NULL;
    }
    spi_unregister_driver(&jadard_common_driver);
}
/*HS03 code for SL6215DEV-292 by chenyihong at 20210901 start*/
late_initcall(jadard_common_init);
//module_init(jadard_common_init);
/*HS03 code for SL6215DEV-292 by chenyihong at 20210901 end*/
module_exit(jadard_common_exit);

MODULE_DESCRIPTION("Jadard_common driver");
MODULE_LICENSE("GPL");
