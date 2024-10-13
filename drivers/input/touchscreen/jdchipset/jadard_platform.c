#include "jadard_platform.h"
#include "jadard_module.h"
#include "jadard_common.h"

static struct spi_device *spi;
extern struct jadard_module_fp g_module_fp;
extern struct jadard_ts_data *pjadard_ts_data;
extern struct jadard_ic_data *pjadard_ic_data;

#ifdef JD_HID_EN
extern struct jadard_spi_hid *shid;
#endif

int jadard_dev_set(struct jadard_ts_data *ts)
{
    ts->input_dev = input_allocate_device();

    if (ts->input_dev == NULL) {
        JD_E("%s: Failed to allocate input device\n", __func__);
        return -ENOMEM;
    }
    ts->input_dev->name = "jadard-touchscreen";

    if (pjadard_ic_data->JD_STYLUS_EN) {
        ts->stylus_dev = input_allocate_device();

        if (ts->stylus_dev == NULL) {
            JD_E("%s: Failed to allocate input stylus_dev\n", __func__);
            input_free_device(ts->input_dev);
            return -ENOMEM;
        }
        ts->stylus_dev->name = "jadard-stylus";
    }

    return 0;
}

int jadard_input_register_device(struct input_dev *input_dev)
{
    return input_register_device(input_dev);
}

int jadard_parse_dt(struct jadard_ts_data *ts,
                    struct jadard_platform_data *pdata)
{
    int coords_size = 0;
    uint32_t coords[4] = {0};
    uint32_t ret, data;
    struct property *prop = NULL;
    struct device_node *dt = ts->dev->of_node;

    ret = of_property_read_u32(dt, "jadard,panel-max-points", &data);
    pjadard_ic_data->JD_MAX_PT = (!ret ? data : 10);
    ret = of_property_read_u32(dt, "jadard,int-is-edge", &data);
    pjadard_ic_data->JD_INT_EDGE = (!ret ? (data > 0 ? true : false) : true);
    ret = of_property_read_u32(dt, "jadard,stylus", &data);
    pjadard_ic_data->JD_STYLUS_EN = (!ret ? (data > 0 ? true : false) : false);

    JD_I("DT:MAX_PT = %d, INT_IS_EDGE = %d, STYLUS_EN = %d\n", pjadard_ic_data->JD_MAX_PT,
        pjadard_ic_data->JD_INT_EDGE, pjadard_ic_data->JD_STYLUS_EN);

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
    struct spi_transfer t;
    uint8_t *buf = NULL;
    int retry;
    int error;

    mutex_lock(&(pjadard_ts_data->spi_lock));
    memset(&t, 0, sizeof(t));

    buf = kzalloc((cmd_len + data_len) * sizeof(uint8_t), GFP_KERNEL);
    if (buf == NULL) {
        JD_E("%s: Memory alloc fail\n", __func__);
        mutex_unlock(&(pjadard_ts_data->spi_lock));
        return -ENOMEM;
    }

	memcpy(buf, cmd, cmd_len);

    t.tx_buf = buf;
    t.rx_buf = buf;
    t.len = cmd_len + data_len;
    spi_message_init(&m);
    spi_message_add_tail(&t, &m);

    for (retry = 0; retry < toRetry; retry++) {
        error = spi_sync(pjadard_ts_data->spi, &m);
        if (unlikely(error)) {
            JD_E("SPI read error: %d\n", error);
            mdelay(2);
        } else{
            memcpy(data, buf + cmd_len, data_len);
            break;
        }
    }
    kfree(buf);

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
    struct spi_transfer t = {
            .tx_buf     = buf,
            .len        = length,
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

int jadard_bus_write(uint8_t *cmd, uint8_t cmd_len, uint8_t *data, uint32_t data_len, uint8_t toRetry)
{
    uint8_t *buf = NULL;
    int status;
    int retry;

    mutex_lock(&(pjadard_ts_data->spi_lock));

    buf = kzalloc((cmd_len + data_len) * sizeof(uint8_t), GFP_KERNEL);

    if (buf == NULL) {
        JD_E("%s: Memory alloc fail\n", __func__);
        mutex_unlock(&(pjadard_ts_data->spi_lock));
        return -ENOMEM;
    }

    memcpy(buf, cmd, cmd_len);
    memcpy(buf + cmd_len, data, data_len);

    for (retry = 0; retry < toRetry; retry++) {
        status = jadard_spi_write(pjadard_ts_data->spi, buf, cmd_len + data_len);

        if (status < 0) {
            JD_E("SPI write error: %d\n", status);
            mdelay(2);
        } else{
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

void jadard_int_enable(bool enable)
{
    int irqnum = pjadard_ts_data->jd_irq;
    unsigned long irqflags = 0;

    spin_lock_irqsave(&pjadard_ts_data->irq_active, irqflags);

    if (enable && (pjadard_ts_data->irq_enabled == 0)) {
        enable_irq(irqnum);
        pjadard_ts_data->irq_enabled = 1;
    } else if ((!enable) && (pjadard_ts_data->irq_enabled == 1)) {
        disable_irq_nosync(irqnum);
        pjadard_ts_data->irq_enabled = 0;
    }

    JD_I("irq_enable = %d\n", pjadard_ts_data->irq_enabled);
    spin_unlock_irqrestore(&pjadard_ts_data->irq_active, irqflags);
}

#ifdef JD_RST_PIN_FUNC
void jadard_gpio_set_value(int pin_num, uint8_t value)
{
    gpio_set_value(pin_num, value);
}
#endif

#if defined(CONFIG_JD_DB)
static int jadard_regulator_configure(struct jadard_platform_data *pdata)
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

static int jadard_power_on(struct jadard_platform_data *pdata, bool on)
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

int jadard_gpio_power_config(struct jadard_platform_data *pdata)
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
            JD_E("unable to request rst-gpio [%d]\n", pdata->gpio_reset);
            goto err_regulator_on;
        }

        error = gpio_direction_output(pdata->gpio_reset, 0);
        if (error) {
            JD_E("unable to set direction for rst-gpio [%d]\n", pdata->gpio_reset);
            goto err_gpio_reset_req;
        }
    } else {
        JD_E("rst-gpio [%d] is not valid\n", pdata->gpio_reset);
        goto err_regulator_on;
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
            JD_E("unable to request irq-gpio [%d]\n", pdata->gpio_irq);
            goto err_power_on;
        }

        error = gpio_direction_input(pdata->gpio_irq);
        if (error) {
            JD_E("unable to set direction for irq-gpio [%d]\n", pdata->gpio_irq);
            goto err_gpio_irq_req;
        }

        pjadard_ts_data->jd_irq = gpio_to_irq(pdata->gpio_irq);
    } else {
        JD_E("irq-gpio [%d] is not valid\n", pdata->gpio_irq);
        goto err_power_on;
    }

    msleep(20);
#ifdef JD_RST_PIN_FUNC
    if (gpio_is_valid(pdata->gpio_reset)) {
        error = gpio_direction_output(pdata->gpio_reset, 1);

        if (error) {
            JD_E("unable to set direction for rst-gpio [%d]\n", pdata->gpio_reset);
            goto err_gpio_irq_req;
        }
        gpio_free(pdata->gpio_reset);
    } else {
        JD_E("rst-gpio [%d] is not valid\n", pdata->gpio_reset);
        goto err_gpio_irq_req;
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

void jadard_gpio_power_deconfig(struct jadard_platform_data *pdata)
{
    /* Only QCOM DB platform using */
}

#else
int jadard_gpio_power_config(struct jadard_platform_data *pdata)
{
    int error = 0;

#ifdef JD_RST_PIN_FUNC
    if (gpio_is_valid(pdata->gpio_reset)) {
        error = gpio_request(pdata->gpio_reset, "jadard_reset_gpio");

        if (error) {
            JD_E("unable to request rst-gpio [%d]\n", pdata->gpio_reset);
            return error;
        }

        error = gpio_direction_output(pdata->gpio_reset, 0);

        if (error) {
            JD_E("unable to set direction for rst-gpio [%d]\n", pdata->gpio_reset);
            gpio_free(pdata->gpio_reset);
            return error;
        }
    } else {
        JD_E("rst-gpio [%d] is not valid\n", pdata->gpio_reset);
        return error;
    }
#endif

    if (gpio_is_valid(pdata->gpio_irq)) {
        /* configure touchscreen irq gpio */
        error = gpio_request(pdata->gpio_irq, "jadard_gpio_irq");

        if (error) {
            JD_E("unable to request irq-gpio [%d]\n", pdata->gpio_irq);
#ifdef JD_RST_PIN_FUNC
            if (gpio_is_valid(pdata->gpio_reset)) {
                gpio_free(pdata->gpio_reset);
            }
#endif
            return error;
        }

        error = gpio_direction_input(pdata->gpio_irq);

        if (error) {
            JD_E("unable to set direction for irq-gpio [%d]\n", pdata->gpio_irq);
            gpio_free(pdata->gpio_irq);
#ifdef JD_RST_PIN_FUNC
            if (gpio_is_valid(pdata->gpio_reset)) {
                gpio_free(pdata->gpio_reset);
            }
#endif
            return error;
        }

        pjadard_ts_data->jd_irq = gpio_to_irq(pdata->gpio_irq);
    } else {
        JD_E("irq-gpio [%d] is not valid\n", pdata->gpio_irq);
#ifdef JD_RST_PIN_FUNC
        if (gpio_is_valid(pdata->gpio_reset)) {
            gpio_free(pdata->gpio_reset);
        }
#endif
        return error;
    }

    msleep(20);
#ifdef JD_RST_PIN_FUNC
    if (gpio_is_valid(pdata->gpio_reset)) {
        error = gpio_direction_output(pdata->gpio_reset, 1);

        if (error) {
            JD_E("unable to set direction for rst-gpio [%d]\n", pdata->gpio_reset);
            gpio_free(pdata->gpio_reset);
            if (gpio_is_valid(pdata->gpio_irq)) {
                gpio_free(pdata->gpio_irq);
            }
            return error;
        }
        gpio_free(pdata->gpio_reset);
    } else {
        JD_E("rst-gpio [%d] is not valid\n", pdata->gpio_reset);
        if (gpio_is_valid(pdata->gpio_irq)) {
            gpio_free(pdata->gpio_irq);
        }
        return error;
    }
#endif

    return error;
}

void jadard_gpio_power_deconfig(struct jadard_platform_data *pdata)
{
    /* Only MTK plateform using */
}

#endif

irqreturn_t jadard_ts_isr_func(int irq, void *ptr)
{
#ifndef JD_HID_EN
#ifdef JD_ZERO_FLASH
    if (pjadard_ts_data->fw_ready == true) {
        jadard_ts_work((struct jadard_ts_data *)ptr);
    }
#else
    jadard_ts_work((struct jadard_ts_data *)ptr);
#endif
#else
    if (shid->spi_hid_en) {
        jadard_spi_hid_work((struct jadard_spi_hid *)ptr);
    }
#endif
    return IRQ_HANDLED;
}

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

void jadard_ts_free_interrupt(void)
{
    struct jadard_ts_data *ts = pjadard_ts_data;

    free_irq(ts->jd_irq, ts);
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

#if defined(JD_CONFIG_FB)
#ifdef JD_CONFIG_DRM
int jadard_drm_check_dt(struct jadard_ts_data *ts)
{
    struct device_node *dt = ts->dev->of_node;
    struct device_node *node = NULL;
    struct drm_panel *panel = NULL;
    int i = 0;
    int count = 0;

    count = of_count_phandle_with_args(dt, "panel", NULL);
    if (count <= 0) {
        JD_I("%s: find drm_panel count(%d) fail\n", __func__, count);
        return 0;
    }
    JD_I("%s: find drm_panel count(%d)\n", __func__, count);
    for (i = 0; i < count; i++) {
        node = of_parse_phandle(dt, "panel", i);
        JD_I("DRM:node = %p\n", node);
        panel = of_drm_find_panel(node);
        JD_I("DRM:panel = %p\n", panel);

        of_node_put(node);
        if (!IS_ERR(panel)) {
            JD_I("%s: find drm_panel successfully\n", __func__);
            ts->active_panel = panel;
            return 0;
        }
    }

    JD_E("%s: no find drm_panel\n", __func__);

    return 0;
}

int jadard_fb_notifier_callback(struct notifier_block *self,
                            unsigned long event, void *data)
{
    struct drm_panel_notifier *evdata = data;
    int *blank;
    struct jadard_ts_data *ts =
        container_of(self, struct jadard_ts_data, fb_notif);
    JD_I("DRM: %s\n", __func__);

    if (evdata && evdata->data &&
        ((event == DRM_PANEL_EARLY_EVENT_BLANK) || (event == DRM_PANEL_EVENT_BLANK)) &&
        ts != NULL &&
        ts->dev != NULL)
    {
        blank = evdata->data;
        JD_I("DRM event:%lu, blank:%d\n", event, *blank);

        switch (*blank) {
        case DRM_PANEL_BLANK_UNBLANK:
            if (DRM_PANEL_EARLY_EVENT_BLANK == event) {
                JD_I("resume: event = %lu, Skipped\n", event);
            } else if (DRM_PANEL_EVENT_BLANK == event) {
                JD_I("resume: event = %lu, TP_RESUME\n", event);
                jadard_common_resume(ts->dev);
            }
            break;

        case DRM_PANEL_BLANK_POWERDOWN:
            if (DRM_PANEL_EARLY_EVENT_BLANK == event) {
                JD_I("suspend: event = %lu, TP_SUSPEND\n", event);
                jadard_common_suspend(ts->dev);
            } else if (DRM_PANEL_EVENT_BLANK == event) {
                JD_I("suspend: event = %lu, Skipped\n", event);
            }
            break;
        }
    }

    return 0;
}
#else
#ifdef JD_CONFIG_DRM_MSM
int jadard_fb_notifier_callback(struct notifier_block *self,
                            unsigned long event, void *data)
{
    struct fb_event *evdata = data;
    int *blank;
    struct jadard_ts_data *ts =
        container_of(self, struct jadard_ts_data, fb_notif);
    JD_I("MSM_DRM: %s\n", __func__);

    if (evdata && evdata->data &&
        ((event == MSM_DRM_EARLY_EVENT_BLANK) || (event == MSM_DRM_EVENT_BLANK)) &&
        ts != NULL &&
        ts->dev != NULL)
    {
        blank = evdata->data;
        JD_I("MSM_DRM event:%lu, blank:%d\n", event, *blank);

        switch (*blank) {
        case MSM_DRM_BLANK_UNBLANK:
            if (MSM_DRM_EARLY_EVENT_BLANK == event) {
                JD_I("resume: event = %lu, Skipped\n", event);
            } else if (MSM_DRM_EVENT_BLANK == event) {
                JD_I("resume: event = %lu, TP_RESUME\n", event);
                jadard_common_resume(ts->dev);
            }
            break;

        case MSM_DRM_BLANK_POWERDOWN:
            if (MSM_DRM_EARLY_EVENT_BLANK == event) {
                JD_I("suspend: event = %lu, TP_SUSPEND\n", event);
                jadard_common_suspend(ts->dev);
            } else if (MSM_DRM_EVENT_BLANK == event) {
                JD_I("suspend: event = %lu, Skipped\n", event);
            }
            break;
        }
    }

    return 0;
}
#else
int jadard_fb_notifier_callback(struct notifier_block *self,
                            unsigned long event, void *data)
{
    struct fb_event *evdata = data;
    int *blank;
    struct jadard_ts_data *ts =
        container_of(self, struct jadard_ts_data, fb_notif);
    JD_I("FB: %s\n", __func__);

    if (evdata && evdata->data &&
        ((event == FB_EARLY_EVENT_BLANK) || (event == FB_EVENT_BLANK)) &&
        ts != NULL &&
        ts->dev != NULL)
    {
        blank = evdata->data;
        JD_I("FB event:%lu, blank:%d\n", event, *blank);

        switch (*blank) {
        case FB_BLANK_UNBLANK:
            if (FB_EARLY_EVENT_BLANK == event) {
                JD_I("resume: event = %lu, Skipped\n", event);
            } else if (FB_EVENT_BLANK == event) {
                JD_I("resume: event = %lu, TP_RESUME\n", event);
                jadard_common_resume(ts->dev);
            }
            break;

        case FB_BLANK_POWERDOWN:
        case FB_BLANK_HSYNC_SUSPEND:
        case FB_BLANK_VSYNC_SUSPEND:
        case FB_BLANK_NORMAL:
            if (FB_EARLY_EVENT_BLANK == event) {
                JD_I("suspend: event = %lu, TP_SUSPEND\n", event);
                jadard_common_suspend(ts->dev);
            } else if (FB_EVENT_BLANK == event) {
                JD_I("suspend: event = %lu, Skipped\n", event);
            }
            break;
        }
    }

    return 0;
}
#endif /* JD_CONFIG_DRM_MSM */
#endif /* JD_CONFIG_DRM */
#endif /* defined(JD_CONFIG_FB) */

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
    /* Maybe config chip_select on Kasan system when read ICID 0000 */
     spi->chip_select = 0; 

    ts->spi = spi;
    mutex_init(&(ts->spi_lock));
    ts->dev = &spi->dev;
    dev_set_drvdata(&spi->dev, ts);
    spi_set_drvdata(spi, ts);
    spin_lock_init(&ts->irq_active);

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
#if (!defined(JD_CONFIG_FB)) && (!defined(JD_CONFIG_DRM))
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
        .name =     JADARD_common_NAME,
        .owner =    THIS_MODULE,
        .of_match_table = jadard_match_table,
    },
    .probe =    jadard_chip_common_probe,
    .remove =   jadard_chip_common_remove,
};

#if defined(__JADARD_KMODULE__)
int jadard_common_init(void)
{
	//jadard_jd9366tp_init();
    JD_I("SPI Jadard kmodule common touch panel driver init\n");
    spi_register_driver(&jadard_common_driver);
    return 0;
}

void jadard_common_exit(void)
{
	JD_I("%s\n", __func__);
	//jadard_jd9366tp_exit();
    if (spi) {
        spi_unregister_device(spi);
        spi = NULL;
    }
    spi_unregister_driver(&jadard_common_driver);
}

#else
static int __init jadard_common_init(void)
{
    JD_I("SPI Jadard common touch panel driver init\n");
	//jadard_jd9366tp_init(void);
#ifndef CONFIG_JD_DB
    spi_register_driver(&jadard_common_driver);
#endif

    return 0;
}

#if defined(CONFIG_JD_DB)
void jadard_workarround_init(void)
{
    JD_I("SPI jadard_workarround_init by Driver\n");

    spi_register_driver(&jadard_common_driver);
}
#endif

static void __exit jadard_common_exit(void)
{
	JD_I("%s\n", __func__);
	//jadard_jd9366tp_exit();
    if (spi) {
        spi_unregister_device(spi);
        spi = NULL;
    }
    spi_unregister_driver(&jadard_common_driver);
}

/* module_init(jadard_common_init);
module_exit(jadard_common_exit);

MODULE_DESCRIPTION("Jadard_common driver");
MODULE_LICENSE("GPL"); */

#endif
