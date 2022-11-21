/*
 * Driver for Power keys on s2mpw01 IC by PWRON rising, falling interrupts.
 *
 * drivers/input/keyboard/s2mpw01_keys.c
 * S2MPW01 Keyboard Driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/s2mpw01_keys.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#include <linux/of_platform.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/spinlock.h>
#include <linux/sec_sysfs.h>
#include <linux/mfd/samsung/s2mpw01.h>
#include <linux/mfd/samsung/s2mpw01-private.h>
#include <linux/wakelock.h>

#ifdef CONFIG_SEC_DEBUG
#include <linux/sec_debug.h>
#endif
#ifdef CONFIG_SLEEP_MONITOR
#include <linux/power/sleep_monitor.h>
#endif

#define WAKELOCK_TIME		HZ/10

int expander_power_keystate = 0;	/* key_pressed_show for 3x4 keypad */
EXPORT_SYMBOL(expander_power_keystate);

struct device *sec_power_key;
EXPORT_SYMBOL(sec_power_key);

struct power_button_data {
	struct power_keys_button *button;
	struct input_dev *input;
	struct timer_list timer;
	struct work_struct work;
	struct workqueue_struct *workqueue;
	unsigned int timer_debounce;	/* in msecs */
	unsigned int irq;
	spinlock_t lock;
	bool isr_status;
	bool disabled;
	bool key_pressed;
	bool key_state;
};

struct power_keys_drvdata {
	struct device *dev;
	struct s2mpw01_platform_data *s2mpw01_pdata;
	const struct power_keys_platform_data *pdata;
	struct input_dev *input;
	struct mutex	disable_lock;
	struct wake_lock         key_wake_lock;

	struct i2c_client *pmm_i2c;
	int		irq_pwronr;
	int		irq_pwronf;
#ifdef CONFIG_SLEEP_MONITOR
	u32 press_cnt;
	bool resume_state;
#endif
	struct power_button_data button_data[0];
};

/*
 * SYSFS interface for enabling/disabling keys and switches:
 *
 * There are 4 attributes under /sys/devices/platform/gpio-keys/
 *	keys [ro]              - bitmap of keys (EV_KEY) which can be
 *	                         disabled
 *	switches [ro]          - bitmap of switches (EV_SW) which can be
 *	                         disabled
 *	disabled_keys [rw]     - bitmap of keys currently disabled
 *	disabled_switches [rw] - bitmap of switches currently disabled
 *
 * Userland can change these values and hence disable event generation
 * for each key (or switch). Disabling a key means its interrupt line
 * is disabled.
 *
 * For example, if we have following switches set up as gpio-keys:
 *	SW_DOCK = 5
 *	SW_CAMERA_LENS_COVER = 9
 *	SW_KEYPAD_SLIDE = 10
 *	SW_FRONT_PROXIMITY = 11
 * This is read from switches:
 *	11-9,5
 * Next we want to disable proximity (11) and dock (5), we write:
 *	11,5
 * to file disabled_switches. Now proximity and dock IRQs are disabled.
 * This can be verified by reading the file disabled_switches:
 *	11,5
 * If we now want to enable proximity (11) switch we write:
 *	5
 * to disabled_switches.
 *
 * We can disable only those keys which don't allow sharing the irq.
 */

/**
 * get_n_events_by_type() - returns maximum number of events per @type
 * @type: type of button (%EV_KEY, %EV_SW)
 *
 * Return value of this function can be used to allocate bitmap
 * large enough to hold all bits for given type.
 */
static inline int get_n_events_by_type(int type)
{
	BUG_ON(type != EV_SW && type != EV_KEY);

	return (type == EV_KEY) ? KEY_CNT : SW_CNT;
}

/**
 * power_keys_disable_button() - disables given GPIO button
 * @bdata: button data for button to be disabled
 *
 * Disables button pointed by @bdata. This is done by masking
 * IRQ line. After this function is called, button won't generate
 * input events anymore. Note that one can only disable buttons
 * that don't share IRQs.
 *
 * Make sure that @bdata->disable_lock is locked when entering
 * this function to avoid races when concurrent threads are
 * disabling buttons at the same time.
 */
static void power_keys_disable_button(struct power_button_data *bdata)
{
	if (!bdata->disabled) {
		/*
		 * Disable IRQ and possible debouncing timer.
		 */
		disable_irq(bdata->irq);
		if (bdata->timer_debounce)
			del_timer_sync(&bdata->timer);

		bdata->disabled = true;
	}
}

/**
 * power_keys_enable_button() - enables given GPIO button
 * @bdata: button data for button to be disabled
 *
 * Enables given button pointed by @bdata.
 *
 * Make sure that @bdata->disable_lock is locked when entering
 * this function to avoid races with concurrent threads trying
 * to enable the same button at the same time.
 */
static void power_keys_enable_button(struct power_button_data *bdata)
{
	if (bdata->disabled) {
		enable_irq(bdata->irq);
		bdata->disabled = false;
	}
}

/**
 * power_keys_attr_show_helper() - fill in stringified bitmap of buttons
 * @ddata: pointer to drvdata
 * @buf: buffer where stringified bitmap is written
 * @type: button type (%EV_KEY, %EV_SW)
 * @only_disabled: does caller want only those buttons that are
 *                 currently disabled or all buttons that can be
 *                 disabled
 *
 * This function writes buttons that can be disabled to @buf. If
 * @only_disabled is true, then @buf contains only those buttons
 * that are currently disabled. Returns 0 on success or negative
 * errno on failure.
 */
static ssize_t power_keys_attr_show_helper(struct power_keys_drvdata *ddata,
					  char *buf, unsigned int type,
					  bool only_disabled)
{
	int n_events = get_n_events_by_type(type);
	unsigned long *bits;
	ssize_t ret;
	int i;

	bits = kcalloc(BITS_TO_LONGS(n_events), sizeof(*bits), GFP_KERNEL);
	if (!bits)
		return -ENOMEM;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct power_button_data *bdata = &ddata->button_data[i];

		if (bdata->button->type != type)
			continue;

		if (only_disabled && !bdata->disabled)
			continue;

		__set_bit(bdata->button->code, bits);
	}

	ret = bitmap_scnlistprintf(buf, PAGE_SIZE - 2, bits, n_events);
	buf[ret++] = '\n';
	buf[ret] = '\0';

	kfree(bits);

	return ret;
}

/**
 * power_keys_attr_store_helper() - enable/disable buttons based on given bitmap
 * @ddata: pointer to drvdata
 * @buf: buffer from userspace that contains stringified bitmap
 * @type: button type (%EV_KEY, %EV_SW)
 *
 * This function parses stringified bitmap from @buf and disables/enables
 * GPIO buttons accordingly. Returns 0 on success and negative error
 * on failure.
 */
static ssize_t power_keys_attr_store_helper(struct power_keys_drvdata *ddata,
					   const char *buf, unsigned int type)
{
	int n_events = get_n_events_by_type(type);
	unsigned long *bits;
	ssize_t error;
	int i;

	bits = kcalloc(BITS_TO_LONGS(n_events), sizeof(*bits), GFP_KERNEL);
	if (!bits)
		return -ENOMEM;

	error = bitmap_parselist(buf, bits, n_events);
	if (error)
		goto out;

	/* First validate */
	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct power_button_data *bdata = &ddata->button_data[i];

		if (bdata->button->type != type)
			continue;

		if (test_bit(bdata->button->code, bits) &&
		    !bdata->button->can_disable) {
			error = -EINVAL;
			goto out;
		}
	}

	mutex_lock(&ddata->disable_lock);

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct power_button_data *bdata = &ddata->button_data[i];

		if (bdata->button->type != type)
			continue;

		if (test_bit(bdata->button->code, bits))
			power_keys_disable_button(bdata);
		else
			power_keys_enable_button(bdata);
	}

	mutex_unlock(&ddata->disable_lock);

out:
	kfree(bits);
	return error;
}

#define ATTR_SHOW_FN(name, type, only_disabled)				\
static ssize_t power_keys_show_##name(struct device *dev,		\
				     struct device_attribute *attr,	\
				     char *buf)				\
{									\
	struct platform_device *pdev = to_platform_device(dev);		\
	struct power_keys_drvdata *ddata = platform_get_drvdata(pdev);	\
									\
	return power_keys_attr_show_helper(ddata, buf,			\
					  type, only_disabled);		\
}

ATTR_SHOW_FN(keys, EV_KEY, false);
ATTR_SHOW_FN(switches, EV_SW, false);
ATTR_SHOW_FN(disabled_keys, EV_KEY, true);
ATTR_SHOW_FN(disabled_switches, EV_SW, true);

/*
 * ATTRIBUTES:
 *
 * /sys/devices/platform/gpio-keys/keys [ro]
 * /sys/devices/platform/gpio-keys/switches [ro]
 */
static DEVICE_ATTR(keys, S_IRUGO, power_keys_show_keys, NULL);
static DEVICE_ATTR(switches, S_IRUGO, power_keys_show_switches, NULL);

#define ATTR_STORE_FN(name, type)					\
static ssize_t power_keys_store_##name(struct device *dev,		\
				      struct device_attribute *attr,	\
				      const char *buf,			\
				      size_t count)			\
{									\
	struct platform_device *pdev = to_platform_device(dev);		\
	struct power_keys_drvdata *ddata = platform_get_drvdata(pdev);	\
	ssize_t error;							\
									\
	error = power_keys_attr_store_helper(ddata, buf, type);		\
	if (error)							\
		return error;						\
									\
	return count;							\
}

ATTR_STORE_FN(disabled_keys, EV_KEY);
ATTR_STORE_FN(disabled_switches, EV_SW);

/*
 * ATTRIBUTES:
 *
 * /sys/devices/platform/gpio-keys/disabled_keys [rw]
 * /sys/devices/platform/gpio-keys/disables_switches [rw]
 */
static DEVICE_ATTR(disabled_keys, S_IWUSR | S_IRUGO,
		   power_keys_show_disabled_keys,
		   power_keys_store_disabled_keys);
static DEVICE_ATTR(disabled_switches, S_IWUSR | S_IRUGO,
		   power_keys_show_disabled_switches,
		   power_keys_store_disabled_switches);

static struct attribute *power_keys_attrs[] = {
	&dev_attr_keys.attr,
	&dev_attr_switches.attr,
	&dev_attr_disabled_keys.attr,
	&dev_attr_disabled_switches.attr,
	NULL,
};

static struct attribute_group power_keys_attr_group = {
	.attrs = power_keys_attrs,
};

#ifdef CONFIG_SEC_SYSFS
static ssize_t key_pressed_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_keys_drvdata *ddata = dev_get_drvdata(dev);
	int i;
	int keystate = 0;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct power_button_data *bdata = &ddata->button_data[i];
		keystate |= bdata->key_state;
		keystate |= expander_power_keystate;
	}

	if (keystate)
		sprintf(buf, "PRESS");
	else
		sprintf(buf, "RELEASE");

	return strlen(buf);
}

static ssize_t key_pressed_show_code(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct power_keys_drvdata *ddata = dev_get_drvdata(dev);
	int i;
	int volume_up = 0, volume_down = 0, power = 0;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct power_button_data *bdata = &ddata->button_data[i];
			if(bdata->button->code == KEY_VOLUMEUP)
				volume_up = bdata->key_state;
			else if(bdata->button->code == KEY_VOLUMEDOWN)
				volume_down = bdata->key_state;
			else if(bdata->button->code == KEY_POWER)
				power = bdata->key_state;
	}

	sprintf(buf, "%d %d %d", volume_up, volume_down, power);

	return strlen(buf);
}

/* the volume keys can be the wakeup keys in special case */
static ssize_t wakeup_enable(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct power_keys_drvdata *ddata = dev_get_drvdata(dev);
	int n_events = get_n_events_by_type(EV_KEY);
	unsigned long *bits;
	ssize_t error;
	int i;

	bits = kcalloc(BITS_TO_LONGS(n_events),
		sizeof(*bits), GFP_KERNEL);
	if (!bits)
		return -ENOMEM;

	error = bitmap_parselist(buf, bits, n_events);
	if (error)
		goto out;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct power_button_data *bdata = &ddata->button_data[i];
		if (test_bit(bdata->button->code, bits))
			bdata->button->wakeup = 1;
		else {
			if (!bdata->button->always_wakeup)
				bdata->button->wakeup = 0;
		}
	}

out:
	kfree(bits);
	return count;
}

static DEVICE_ATTR(sec_power_key_pressed, 0664, key_pressed_show, NULL);
static DEVICE_ATTR(sec_power_key_pressed_code, 0664, key_pressed_show_code, NULL);
static DEVICE_ATTR(wakeup_keys, 0664, NULL, wakeup_enable);

static struct attribute *sec_power_key_attrs[] = {
	&dev_attr_sec_power_key_pressed.attr,
	&dev_attr_sec_power_key_pressed_code.attr,
	&dev_attr_wakeup_keys.attr,
	NULL,
};

static struct attribute_group sec_power_key_attr_group = {
	.attrs = sec_power_key_attrs,
};
#endif

#ifdef CONFIG_SLEEP_MONITOR
#define	PRETTY_MAX	14
#define	STATE_BIT	24
#define	CNT_MARK	0xffff
#define	STATE_MARK	0xff
static int power_keys_get_sleep_monitor_cb(void* priv, unsigned int *raw_val, int check_level, int caller_type);

static struct sleep_monitor_ops  power_keys_sleep_monitor_ops = {
	 .read_cb_func =  power_keys_get_sleep_monitor_cb,
};

static int power_keys_get_sleep_monitor_cb(void* priv, unsigned int *raw_val, int check_level, int caller_type)
{
	struct power_keys_drvdata *ddata = priv;
	struct input_dev *input = ddata->input;
	int state = DEVICE_UNKNOWN;
	int pretty = 0;

	if ((check_level == SLEEP_MONITOR_CHECK_SOFT) ||\
	    (check_level == SLEEP_MONITOR_CHECK_HARD)) {
		if (ddata->resume_state)
			state = DEVICE_ON_ACTIVE1;
		else
			state = DEVICE_POWER_OFF;
	}

	*raw_val = ((state & STATE_MARK) << STATE_BIT) |\
			(ddata->press_cnt & CNT_MARK);

	if (ddata->press_cnt > PRETTY_MAX)
		pretty = PRETTY_MAX;
	else
		pretty = ddata->press_cnt;

	ddata->press_cnt = 0;

	dev_dbg(&input->dev, "%s: raw_val[0x%08x], check_level[%d], state[%d], pretty[%d]\n",
		__func__, *raw_val, check_level, state, pretty);

	return pretty;
}
#endif


static void power_keys_power_report_event(struct power_button_data *bdata)
{
	const struct power_keys_button *button = bdata->button;
	struct input_dev *input = bdata->input;
	struct power_keys_drvdata *ddata = input_get_drvdata(input);
	unsigned int type = button->type ?: EV_KEY;
	int state = bdata->key_pressed;

	wake_lock_timeout(&ddata->key_wake_lock, WAKELOCK_TIME);

	if (button->code == KEY_POWER) {
		printk(KERN_INFO "%s: [%s][%s]KEY_POWER\n",
			__func__, (!!state) ? "P" : "R", bdata->isr_status ? "I":"R");
		bdata->isr_status = false;
	}
	if ((button->code == KEY_HOMEPAGE) && !!state) {
		printk(KERN_INFO "HOME key is pressed\n");
	}

	if (type == EV_ABS) {
		if (state)
			input_event(input, type, button->code, button->value);
	} else {
		bdata->key_state = !!state;
		input_event(input, type, button->code, !!state);
	}
	input_sync(input);
#ifdef CONFIG_SLEEP_MONITOR
	if ((ddata->press_cnt < CNT_MARK) && (bdata->isr_status) && (!!state))
		ddata->press_cnt++;
#endif
}


static irqreturn_t power_keys_rising_irq_handler(int irq, void *dev_id)
{
	struct power_keys_drvdata *ddata = dev_id;
	/* TO DO: consider the flexibiliy */
	struct power_button_data *bdata = &ddata->button_data[0];

	bdata->key_pressed = true;
	bdata->isr_status = true;
	power_keys_power_report_event(bdata);

	return IRQ_HANDLED;
}

static irqreturn_t power_keys_falling_irq_handler(int irq, void *dev_id)
{
	struct power_keys_drvdata *ddata = dev_id;
	/* TO DO: consider the flexibiliy */
	struct power_button_data *bdata = &ddata->button_data[0];

	bdata->key_pressed = false;
	bdata->isr_status = true;
	power_keys_power_report_event(bdata);
	return IRQ_HANDLED;
}

static void power_keys_report_state(struct power_keys_drvdata *ddata)
{
	struct input_dev *input = ddata->input;
	int i;

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct power_button_data *bdata = &ddata->button_data[i];
		/* TO DO: read status directly */
		bdata->key_pressed = 0;
		power_keys_power_report_event(bdata);
	}
	input_sync(input);
}

static int power_keys_open(struct input_dev *input)
{
	struct power_keys_drvdata *ddata = input_get_drvdata(input);

	dev_info(ddata->dev, "%s()\n", __func__);

	mutex_lock(&ddata->disable_lock);
	enable_irq(ddata->irq_pwronf);
	enable_irq(ddata->irq_pwronr);
	mutex_unlock(&ddata->disable_lock);

	power_keys_report_state(ddata);

	return 0;
}

static void power_keys_close(struct input_dev *input)
{
	struct power_keys_drvdata *ddata = input_get_drvdata(input);

	dev_info(ddata->dev, "%s()\n", __func__);

	mutex_lock(&ddata->disable_lock);
	disable_irq(ddata->irq_pwronf);
	disable_irq(ddata->irq_pwronr);
	mutex_unlock(&ddata->disable_lock);
}

/*
 * Handlers for alternative sources of platform_data
 */

#ifdef CONFIG_OF
/*
 * Translate OpenFirmware node properties into platform_data
 */
static struct power_keys_platform_data *power_keys_get_devtree_pdata(
		struct s2mpw01_dev *mfd_dev)
{
	/* to do */
	#define S2MPW01_SUPPORT_KEY_NUM	(1)
	struct device *dev = mfd_dev->dev;
	struct device_node *p_node, *key_node, *pp;
	struct power_keys_platform_data *pdata;
	struct power_keys_button *button;
	int error;
	int nbuttons;
	int i;

	p_node = mfd_dev->dev->of_node;
	if (!p_node) {
		error = -ENODEV;
		goto err_out;
	}

	key_node = of_find_node_by_name(p_node, "s2mpw01-keys");
	if (!key_node) {
		dev_err(dev, "could not find s2mpw01-keys sub-node\n");
		error = -ENOENT;
		goto err_out;
	}

	nbuttons = of_get_child_count(key_node);
	if (nbuttons > S2MPW01_SUPPORT_KEY_NUM)
		dev_warn(dev, "s2mpw01-keys support only one button.\n");

	pdata = kzalloc(sizeof(*pdata) + nbuttons * (sizeof *button),
			GFP_KERNEL);
	if (!pdata) {
		error = -ENOMEM;
		goto err_out;
	}

	pdata->buttons = (struct power_keys_button *)(pdata + 1);
	pdata->nbuttons = nbuttons;

	//pdata->rep = !!of_get_property(node, "autorepeat", NULL);

	i = 0;
	for_each_child_of_node(key_node, pp) {
		button = &pdata->buttons[i++];
		if (of_property_read_u32(pp, "linux,code", &button->code)) {
			dev_err(dev, "Button without keycode: 0x%x\n",
				button->gpio);
			error = -EINVAL;
			goto err_free_pdata;
		}

		button->desc = of_get_property(pp, "label", NULL);

		of_property_read_u32(pp, "wakeup", &button->wakeup);

		if (of_property_read_u32(pp, "linux,input-type", &button->type))
			button->type = EV_KEY;
	}

	if (pdata->nbuttons == 0) {
		error = -EINVAL;
		goto err_free_pdata;
	}

	return pdata;

err_free_pdata:
	kfree(pdata);
err_out:
	return ERR_PTR(error);
}



static struct of_device_id power_keys_of_match[] = {
	{ .compatible = "s2mpw01-power-keys", },
	{ },
};
MODULE_DEVICE_TABLE(of, power_keys_of_match);

#else

static inline struct power_keys_platform_data *
power_keys_get_devtree_pdata(struct s2mpw01_dev *mfd_dev)
{
	return ERR_PTR(-ENODEV);
}

#endif

static void power_remove_key(struct power_button_data *bdata)
{
	free_irq(bdata->irq, bdata);
	if (bdata->timer_debounce)
		del_timer_sync(&bdata->timer);
	cancel_work_sync(&bdata->work);
	if (gpio_is_valid(bdata->button->gpio))
		gpio_free(bdata->button->gpio);
}

static int power_keys_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct s2mpw01_dev *s2mpw01 = dev_get_drvdata(pdev->dev.parent);
	struct s2mpw01_platform_data *mpdata = s2mpw01->pdata;
	struct power_keys_platform_data *pdata = dev_get_platdata(dev);
	struct power_keys_drvdata *ddata = NULL;
	struct input_dev *input;
	int i, ret;
	int wakeup = 0;

	if (!pdata) {
		pdata = power_keys_get_devtree_pdata(s2mpw01);
		if (IS_ERR(pdata))
			return PTR_ERR(pdata);
	}

	input = input_allocate_device();
	if (!input) {
		dev_err(dev, "failed to allocate state\n");
		ret = -ENOMEM;
		goto fail1;
	}
	input->name	= pdata->name ? : pdev->name;
	input->phys	= "s2mpw01-keys/input0";
	input->dev.parent = dev;
	input->open	= power_keys_open;
	input->close	= power_keys_close;

	input->id.bustype	= BUS_I2C;
	input->id.vendor	= 0x0001;
	input->id.product	= 0x0001;
	input->id.version	= 0x0100;

	/* allocate driver data */
	ddata = kzalloc(sizeof(*ddata) +
			pdata->nbuttons * sizeof(struct power_button_data),
			GFP_KERNEL);
	if (!ddata) {
		dev_err(dev, "failed to allocate ddata.\n");
		return -ENOMEM;
	}
	ddata->dev	= dev;
	ddata->pdata	= pdata;
	ddata->input	= input;
	ddata->pmm_i2c	= s2mpw01->pmic;

	mutex_init(&ddata->disable_lock);

	wake_lock_init(&ddata->key_wake_lock,
			WAKE_LOCK_SUSPEND, "power keys wake lock");

	platform_set_drvdata(pdev, ddata);
	input_set_drvdata(input, ddata);

	/* Enable auto repeat feature of Linux input subsystem */
	if (ddata->pdata->rep)
		__set_bit(EV_REP, input->evbit);

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct power_keys_button *button = &ddata->pdata->buttons[i];
		struct power_button_data *bdata = &ddata->button_data[i];

		bdata->input = input;
		bdata->button = button;

		if (button->wakeup)
			wakeup = 1;

		input_set_capability(input, button->type ?: EV_KEY, button->code);
	}

	ddata->irq_pwronr = mpdata->irq_base + S2MPW01_PMIC_IRQ_PWRONR_INT1;
	ddata->irq_pwronf = mpdata->irq_base + S2MPW01_PMIC_IRQ_PWRONF_INT1;
	irq_set_status_flags(ddata->irq_pwronr, IRQ_NOAUTOEN);
	irq_set_status_flags(ddata->irq_pwronf, IRQ_NOAUTOEN);

	ret = request_any_context_irq(ddata->irq_pwronr,
			power_keys_rising_irq_handler, 0, "pwronr-irq", ddata);
	if(ret < 0) {
		dev_err(dev, "%s() fail to request pwron-r in IRQ: %d: %d\n",
				__func__, ddata->irq_pwronr, ret);
		goto fail1;
	}

	ret = request_any_context_irq(ddata->irq_pwronf,
			power_keys_falling_irq_handler, 0, "pwronf-irq", ddata);
	if(ret < 0) {
		dev_err(dev, "%s() fail to request pwron-f in IRQ: %d: %d\n",
				__func__, ddata->irq_pwronf, ret);
		goto fail1;
	}

	ret = sysfs_create_group(&dev->kobj, &power_keys_attr_group);
	if (ret) {
		dev_err(dev, "Unable to export keys/switches, ret: %d\n",
			ret);
		goto fail2;
	}

#ifdef CONFIG_SEC_SYSFS
	sec_power_key = sec_device_create(ddata, "sec_power_key");
	if (IS_ERR(sec_power_key))
		dev_err(dev, "%s failed to create sec_power_key\n", __func__);

	ret = sysfs_create_group(&sec_power_key->kobj, &sec_power_key_attr_group);
	if (ret) {
		dev_err(dev, "Unable to export keys/switches, error: %d\n",
			ret);
		goto fail2;
	}
#endif
	ret = input_register_device(input);
	if (ret) {
		dev_err(dev, "Unable to register input device, error: %d\n",
			ret);
		goto fail3;
	}

	device_init_wakeup(dev, wakeup);

#ifdef CONFIG_SLEEP_MONITOR
	ddata->resume_state = true;
	sleep_monitor_register_ops(ddata, &power_keys_sleep_monitor_ops,
			SLEEP_MONITOR_KEY);
#endif

	return 0;

 fail3:
	sysfs_remove_group(&dev->kobj, &power_keys_attr_group);
 fail2:
	while (--i >= 0) {
		struct power_button_data *bdata = &ddata->button_data[i];

		if (bdata->workqueue)
			destroy_workqueue(bdata->workqueue);

		power_remove_key(bdata);
	}

	platform_set_drvdata(pdev, NULL);
 fail1:
	wake_lock_destroy(&ddata->key_wake_lock);
	input_free_device(input);
	kfree(ddata);
	/* If we have no platform data, we allocated pdata dynamically. */
	if (!dev_get_platdata(dev))
		kfree(pdata);

	return ret;
}

static int power_keys_remove(struct platform_device *pdev)
{
	struct power_keys_drvdata *ddata = platform_get_drvdata(pdev);
	struct input_dev *input = ddata->input;
	int i;

#ifdef CONFIG_SLEEP_MONITOR
	sleep_monitor_unregister_ops(SLEEP_MONITOR_KEY);
#endif
	sysfs_remove_group(&pdev->dev.kobj, &power_keys_attr_group);

	device_init_wakeup(&pdev->dev, 0);
	wake_lock_destroy(&ddata->key_wake_lock);

	for (i = 0; i < ddata->pdata->nbuttons; i++) {
		struct power_button_data *bdata = &ddata->button_data[i];

		if (bdata->workqueue)
			destroy_workqueue(bdata->workqueue);

		power_remove_key(bdata);
	}

	input_unregister_device(input);

	/* If we have no platform data, we allocated pdata dynamically. */
	if (!dev_get_platdata(&pdev->dev))
		kfree(ddata->pdata);

	kfree(ddata);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int power_keys_suspend(struct device *dev)
{
	struct power_keys_drvdata *ddata = dev_get_drvdata(dev);
	struct input_dev *input = ddata->input;
	int i;

	if (device_may_wakeup(dev)) {
		for (i = 0; i < ddata->pdata->nbuttons; i++) {
			struct power_button_data *bdata = &ddata->button_data[i];
			if ((bdata->button->wakeup) && (bdata->irq))
				enable_irq_wake(bdata->irq);
		}
	} else {
		mutex_lock(&input->mutex);
		if (input->users)
			power_keys_close(input);
		mutex_unlock(&input->mutex);
	}

#ifdef CONFIG_SLEEP_MONITOR
	ddata->resume_state = false;
#endif

	return 0;
}

static int power_keys_resume(struct device *dev)
{
	struct power_keys_drvdata *ddata = dev_get_drvdata(dev);
	struct input_dev *input = ddata->input;
	int error = 0;
	int i;

#ifdef CONFIG_SLEEP_MONITOR
	ddata->resume_state = true;
#endif

	if (device_may_wakeup(dev)) {
		for (i = 0; i < ddata->pdata->nbuttons; i++) {
			struct power_button_data *bdata = &ddata->button_data[i];
			if ((bdata->button->wakeup) && (bdata->irq))
				disable_irq_wake(bdata->irq);
		}
	} else {
		mutex_lock(&input->mutex);
		if (input->users)
			error = power_keys_open(input);
		mutex_unlock(&input->mutex);
	}

	if (error)
		return error;

	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(power_keys_pm_ops, power_keys_suspend, power_keys_resume);

static struct platform_driver power_keys_device_driver = {
	.probe		= power_keys_probe,
	.remove		= power_keys_remove,
	.driver		= {
		.name	= "s2mpw01-power-keys",
		.owner	= THIS_MODULE,
		.pm	= &power_keys_pm_ops,
		.of_match_table = of_match_ptr(power_keys_of_match),
	}
};

static int __init power_keys_init(void)
{
	return platform_driver_register(&power_keys_device_driver);
}

static void __exit power_keys_exit(void)
{
	platform_driver_unregister(&power_keys_device_driver);
}

device_initcall(power_keys_init);
module_exit(power_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Keyboard driver for s2mpw01");
MODULE_ALIAS("platform:s2mpw01 Power key");
