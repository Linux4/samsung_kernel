/* to Support headset detect function for factory 15 mode. KSND  */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/switch.h>
#include <linux/input.h>
#include <jack_factory.h>

//#define SEC_USE_JACK_ADC_OUTPUT
#undef  SEC_USE_JACK_SELECT

//define pointer of the dev_priv
static struct jack_device_priv *dev_priv;

static ssize_t jack_state_show(struct device *dev,
                                      struct device_attribute *attr, char *buf)
{
    struct jack_platform_det *jackdet = &dev_priv->jack;
    int report = 0;

    report = (jackdet->jack_det) ? 1 : 0;

    pr_warn("%s: [accdet] report %d, jack_det %d\n",
        __func__, report, jackdet->jack_det);

    return sprintf(buf, "%d\n", report);
}

static ssize_t jack_state_store(struct device *dev,
                    struct device_attribute *attr, const char *buf, size_t size)
{
    pr_warn("%s: [accdet] store value %c\n", __func__, buf[0]);

    return size;
}

static ssize_t jack_key_state_show(struct device *dev,
                                      struct device_attribute *attr, char *buf)
{
    struct jack_platform_det *jackdet = &dev_priv->jack;
    int report = 0;

    report = jackdet->button_det ? 1 : 0;

    return sprintf(buf, "%d\n", report);
}

static ssize_t jack_key_state_store(struct device *dev,
                  struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}

#ifdef SEC_USE_JACK_SELECT
static ssize_t jack_select_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    return 0;
}

static ssize_t jack_select_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t size)
{
    if ((!size) || (buf[0] != '1')) {
        switch_set_state(&dev_priv->sdev, 0);
    } else {
        switch_set_state(&dev_priv->sdev, 1);
    }

    return size;
}
#endif /* SEC_USE_JACK_SELECT */

static ssize_t jack_mic_adc_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    struct jack_platform_det *jackdet = &dev_priv->jack;

    return sprintf(buf, "%d\n", jackdet->adc_val);
}

static ssize_t jack_mic_adc_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t size)
{
    return size;
}

#if defined (SEC_USE_JACK_ADC_OUTPUT)
static ssize_t jack_adc_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    int val[2] = {0,};

    val[1] = dev_priv->mic_adc_range;

    return sprintf(buf, "%d %d\n",val[0],val[1]);
}

static ssize_t jack_key_adc_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    int val[2]  = {0,};

    if(dev_priv->jack_buttons_zones == NULL ){
        pr_err("No defined the headset zones array! \n");
        return sprintf(buf, "%d %d\n",0,0);
    }

    val[0] =  dev_priv->jack_buttons_zones[0].adc_low;
    val[1] =  dev_priv->jack_buttons_zones[0].adc_high;

    return sprintf(buf, "%d %d\n",val[0],val[1]);
}

static ssize_t jack_google_adc_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    int val[2]  = {0,};

    if(dev_priv->jack_buttons_zones == NULL ){
        pr_err("No defined the headset zones array! \n");
        return sprintf(buf, "%d %d\n",0,0);
    }

    val[0] =  dev_priv->jack_buttons_zones[1].adc_low;
    val[1] =  dev_priv->jack_buttons_zones[1].adc_high;

    return sprintf(buf, "%d %d\n",val[0],val[1]);
}

static ssize_t jack_vol_up_adc_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    int val[2]  = {0,};

    if(dev_priv->jack_buttons_zones == NULL ){
        pr_err("No defined the headset zones array! \n");
        return sprintf(buf, "%d %d\n",0,0);
    }

    val[0] =  dev_priv->jack_buttons_zones[2].adc_low;
    val[1] =  dev_priv->jack_buttons_zones[2].adc_high;

    return sprintf(buf, "%d %d\n",val[0],val[1]);
}

static ssize_t jack_vol_down_adc_show(struct device *dev,
                                    struct device_attribute *attr, char *buf)
{
    int val[2]  = {0,};

    if(dev_priv->jack_buttons_zones == NULL ){
        pr_err("No defined the headset zones array! \n");
        return sprintf(buf, "%d %d\n",0,0);
    }

    val[0] =  dev_priv->jack_buttons_zones[3].adc_low;
    val[1] =  dev_priv->jack_buttons_zones[3].adc_high;

    return sprintf(buf, "%d %d\n",val[0],val[1]);
}
#endif

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR | S_IWGRP,
                        jack_state_show, jack_state_store);

static DEVICE_ATTR(key_state, S_IRUGO | S_IWUSR | S_IWGRP,
                        jack_key_state_show, jack_key_state_store);

#ifdef SEC_USE_JACK_SELECT
static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWGRP,
                        jack_select_show, jack_select_store);
#endif /* SEC_USE_JACK_SELECT */

static DEVICE_ATTR(mic_adc, S_IRUGO | S_IWUSR | S_IWGRP,
                        jack_mic_adc_show, jack_mic_adc_store);

#if defined (SEC_USE_JACK_ADC_OUTPUT)
static DEVICE_ATTR(jacks_adc,  S_IRUGO | S_IWUSR | S_IWGRP,
                        jack_adc_show, NULL);
static DEVICE_ATTR(send_end_btn_adc,  S_IRUGO | S_IWUSR | S_IWGRP,
                        jack_key_adc_show, NULL);
static DEVICE_ATTR(voc_assist_btn_adc,  S_IRUGO | S_IWUSR | S_IWGRP,
                        jack_google_adc_show, NULL);
static DEVICE_ATTR(vol_up_btn_adc,  S_IRUGO | S_IWUSR | S_IWGRP,
                        jack_vol_up_adc_show, NULL);
static DEVICE_ATTR(vol_down_btn_adc,  S_IRUGO | S_IWUSR | S_IWGRP,
                        jack_vol_down_adc_show, NULL);
#endif

void create_jack_factory_devices(struct jack_device_priv *pPriv_dat)
{
    static atomic_t device_count;
    static struct class *audio_class;
    static struct device *earjack;
    int index;

    if( pPriv_dat != NULL ){
        dev_priv = pPriv_dat;
    } else {
        pr_err("Failed to get the priv pointor!\n");
        return;
    }

    audio_class = class_create(THIS_MODULE, "audio");

    if (IS_ERR(audio_class)) {
        pr_err("Failed to create class\n");
        return;
    }
    atomic_set(&device_count, 0);
    index=atomic_inc_return(&device_count);
    earjack = device_create(audio_class, NULL,MKDEV(0, index), NULL, "earjack");

#ifdef SEC_USE_JACK_SELECT
    if (device_create_file(earjack, &dev_attr_select_jack) < 0) {
        pr_err("Failed to create (%s)\n", dev_attr_select_jack.attr.name);
    }
#endif /* SEC_USE_JACK_SELECT */

    if (device_create_file(earjack, &dev_attr_key_state) < 0){
        pr_err("Failed to create (%s)\n", dev_attr_key_state.attr.name);
    }

    if (device_create_file(earjack, &dev_attr_state) < 0){
        pr_err("Failed to create (%s)\n", dev_attr_state.attr.name);
    }

    if (device_create_file(earjack, &dev_attr_mic_adc) < 0){
        pr_err("Failed to create (%s)\n", dev_attr_mic_adc.attr.name);
    }

#if defined (SEC_USE_JACK_ADC_OUTPUT)
    if (device_create_file(earjack, &dev_attr_jacks_adc) < 0){
        pr_err("Failed to create (%s)\n", dev_attr_jacks_adc.attr.name);
    }

    if (device_create_file(earjack, &dev_attr_send_end_btn_adc) < 0){
        pr_err("Failed to create (%s)\n", dev_attr_send_end_btn_adc.attr.name);
    }

    if (device_create_file(earjack, &dev_attr_voc_assist_btn_adc) < 0){
        pr_err("Failed to create (%s)\n", dev_attr_voc_assist_btn_adc.attr.name);
    }

    if (device_create_file(earjack, &dev_attr_vol_up_btn_adc) < 0){
        pr_err("Failed to create (%s)\n", dev_attr_vol_up_btn_adc.attr.name);
    }

    if (device_create_file(earjack, &dev_attr_vol_down_btn_adc) < 0){
        pr_err("Failed to create (%s)\n", dev_attr_vol_down_btn_adc.attr.name);
    }
#endif
}

