/* To support PBA function test */
static struct class *jack_class;
static struct device *jack_dev;

static ssize_t earjack_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);
	struct cod3022x_jack_det *jackdet = &cod3022x->jack_det;
	int status = jackdet->jack_det;
	int report = 0;

	pr_info("%s: status = %d\n", __func__, status);
	if (status)
		report = 1;

	return sprintf(buf, "%d\n", report);
}

static ssize_t earjack_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_key_state_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);
	struct cod3022x_jack_det *jackdet = &cod3022x->jack_det;
	int report = 0;

	pr_info("%s: Button Status: %d\n",__func__, report);
	
	report = jackdet->button_det ? true : false;

	return sprintf(buf, "%d\n", report);
}

static ssize_t earjack_key_state_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static ssize_t earjack_select_jack_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	pr_info("%s : operate nothing\n", __func__);

	return 0;
}

static ssize_t earjack_select_jack_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);

	if ((!size) || (buf[0] != '1')) {
		switch_set_state(&cod3022x->sdev, 0);
		pr_info("Forced remove microphone\n");
	} else {
		switch_set_state(&cod3022x->sdev, 1);
		pr_info("Forced detect microphone\n");
	}

	return size;
}

static ssize_t earjack_mic_adc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct cod3022x_priv *cod3022x = dev_get_drvdata(dev);
	struct cod3022x_jack_det *jackdet = &cod3022x->jack_det;

	return sprintf(buf, "%d\n", jackdet->adc_val);
}

static ssize_t earjack_mic_adc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{

	pr_info("%s : operate nothing\n", __func__);

	return size;
}

static DEVICE_ATTR(select_jack, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_select_jack_show, earjack_select_jack_store);

static DEVICE_ATTR(key_state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_key_state_show, earjack_key_state_store);

static DEVICE_ATTR(state, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_state_show, earjack_state_store);

static DEVICE_ATTR(mic_adc, S_IRUGO | S_IWUSR | S_IWGRP,
		   earjack_mic_adc_show, earjack_mic_adc_store);

static void create_jack_devices(struct cod3022x_priv *info)
{
	/* To support PBA function test */
	jack_class = class_create(THIS_MODULE, "audio");

	if (IS_ERR(jack_class))
		pr_err("Failed to create class\n");

	jack_dev = device_create(jack_class, NULL, 0, info, "earjack");

	if (device_create_file(jack_dev, &dev_attr_select_jack) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_select_jack.attr.name);

	if (device_create_file(jack_dev, &dev_attr_key_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_key_state.attr.name);

	if (device_create_file(jack_dev, &dev_attr_state) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_state.attr.name);
	
	if (device_create_file(jack_dev, &dev_attr_mic_adc) < 0)
		pr_err("Failed to create device file (%s)!\n",
			dev_attr_mic_adc.attr.name);
}
