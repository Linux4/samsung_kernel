

/**
 * Please remove the following macro definition(#define DEBUG) when releasing
 * the official software version.
 */
#define DEBUG
#define pr_fmt(fmt)    TOUCHSCREEN_CLASS_NAME " " DEFAULT_DEVICE_NAME ": " fmt

/**
 * default class name(/sys/class/TOUCHSCREEN_CLASS_NAME).
 * default device name(/sys/class/TOUCHSCREEN_CLASS_NAME/DEFAULT_DEVICE_NAME).
 */
#define TOUCHSCREEN_CLASS_NAME    "touchscreen"
#define DEFAULT_DEVICE_NAME       "touchpanel"

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/fb.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/bitops.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/irqdesc.h>
#include <linux/of.h>
#include <linux/mm.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinconf-generic.h>
#include "../../pinctrl/core.h"
#include "../../pinctrl/pinconf.h"
#include <linux/pinctrl/consumer.h>
#include "../../gpio/gpiolib.h"
#include "../../base/base.h"
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include "../../regulator/internal.h"
#include <linux/input/touch-info.h>

/**
 * struct tid_private - The private data of touch info device core
 * @mask:                  The mask of the gestures enable or disable.
 * @input_dev:             All gesture core will report keycode via this
 *  	input device.
 * @wakeup_code:           The key code of the last wakeup system.
 * @wakeup_code_name:      The key code name of the last wakeup
 *  	system(i.e. "double_tap" means double-tap).
 * @fb_notifier:           Guess what?
 * @poweron:               Is the screen on?
 * @is_upgrading_firmware: Is chip upgrading firmware?
 */
struct tid_private {
#ifdef CONFIG_TOUCHSCREEN_GESTURE
	atomic_t mask;
	struct input_dev *input_dev;
	atomic_t wakeup_code;
	const char *wakeup_code_name;
#endif
#ifdef CONFIG_FB
	struct notifier_block fb_notifier;
	bool poweron:1;
#endif
	bool is_upgrading_firmware:1;
	const char *ini_def_name;
};

/**
 * Do you think 'return dev_get_drvdata(dev)' is better?
 */
static inline struct touch_info_dev *dev_to_tid(struct device *dev)
{
	return container_of(dev, struct touch_info_dev, dev);
}

static inline struct device *tid_to_dev(struct touch_info_dev *tid)
{
	return &tid->dev;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 0, 0)
static inline struct device_node *dev_of_node(struct device *dev)
{
	if (!IS_ENABLED(CONFIG_OF))
		return NULL;
	return dev->of_node;
}
#endif

#ifdef CONFIG_FB
static int fb_notifier_call(struct notifier_block *nb, unsigned long event,
							void *data)
{
	int *blank;
	struct fb_event *evdata = data;
	struct tid_private *p;

	/* If we aren't interested in this event, skip it immediately */
	if (event != FB_EVENT_BLANK)
		return 0;

	if (unlikely(!evdata || !evdata->data))
		return 0;

	p = container_of(nb, struct tid_private, fb_notifier);
	blank = evdata->data;
	if (*blank == FB_BLANK_POWERDOWN)
		p->poweron = false;
	else if (*blank == FB_BLANK_UNBLANK)
		p->poweron = true;
	pr_debug("screen %s\n", p->poweron ? "on" : "off");

	return 0;
}

static inline bool is_poweron(struct device *dev)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return tid->p->poweron;
}

static void fb_notifier_init(struct device *dev)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct notifier_block *fb_notifier = &tid->p->fb_notifier;

	tid->p->poweron = true;
	fb_notifier->notifier_call = fb_notifier_call;
	fb_register_client(fb_notifier);
}

static void fb_notifier_remove(struct device *dev)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	fb_unregister_client(&tid->p->fb_notifier);
}
#else
static inline void fb_notifier_init(struct device *dev)
{
}

static inline void fb_notifier_remove(struct device *dev)
{
}

static inline bool is_poweron(struct device *dev)
{
	return true;
}
#endif /* CONFIG_FB */

static ssize_t poweron_show(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", is_poweron(dev));
}
static DEVICE_ATTR_RO(poweron);

static ssize_t reset(struct device *dev)
{
	int ret = -ENODEV;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;

	if (likely(tid_ops && tid_ops->reset))
		ret = tid_ops->reset(dev->parent);

	return ret;
}

static ssize_t reset_show(struct device *dev, struct device_attribute *attr,
						  char *buf)
{
	return reset(dev) ? : scnprintf(buf, PAGE_SIZE, "reset\n");
}

static ssize_t reset_store(struct device *dev, struct device_attribute *attr,
						   const char *buf, size_t count)
{
	if (unlikely(buf[0] != '1'))
		return -EINVAL;

	return reset(dev) ? : count;
}
static DEVICE_ATTR_RW(reset);

static ssize_t vendor_show(struct device *dev, struct device_attribute *attr,
						   char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", tid->vendor);
}
static DEVICE_ATTR_RO(vendor);

static ssize_t productinfo_show(struct device *dev,
								struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", tid->product);
}
static DEVICE_ATTR_RO(productinfo);

static ssize_t buildid_show(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;
	int major, minor;
	int ret;

	if (unlikely(!tid_ops || !tid_ops->get_version))
		return -ENODEV;

	ret = tid_ops->get_version(dev->parent, &major, &minor);
	if (unlikely(ret)) {
		dev_err(dev, "get version fail\n");
		return ret;
	}

	return scnprintf(buf, PAGE_SIZE, "%04x-%02x\n", major, minor);
}
static DEVICE_ATTR_RO(buildid);

static ssize_t path_show(struct device *dev, struct device_attribute *attr,
						 char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	ssize_t len;
	const char *path;
	struct kobject *kobj = tid->use_dev_path ? &dev->parent->kobj : &dev->kobj;

	path = kobject_get_path(kobj, GFP_KERNEL);
	len = scnprintf(buf, PAGE_SIZE, "%s\n", path ? : "na");
	kfree(path);

	return len;
}
static DEVICE_ATTR_RO(path);

static ssize_t flashprog_show(struct device *dev, struct device_attribute *attr,
							  char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", tid->p->is_upgrading_firmware);
}
static DEVICE_ATTR_RO(flashprog);

static int firmware_upgrade(struct device *dev, const char *buf, size_t count,
							bool force)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;
	const struct firmware *fw;
	char *c;
	char *name;
	int ret;

	if (!is_poweron(dev)) {
		dev_err(dev, "not allow upgrade firmware(power off)\n");
		return -EPERM;
	}

	if (unlikely(!tid_ops || !tid_ops->firmware_upgrade))
		return -ENODEV;

	name = kzalloc(count + 1, GFP_KERNEL);
	if (unlikely(!name))
		return -ENOMEM;
	memcpy(name, buf, count);

	if ((c = strnchr(name, count, '\n')))
		*c = '\0';

	get_device(dev);
	ret = request_firmware_direct(&fw, name, dev);
	if (unlikely(ret))
		goto err;

	if (unlikely(tid->p->is_upgrading_firmware)) {
		dev_info(dev, "is upgrading firmware, please wait\n");
		ret = -EBUSY;
		goto skip_upgrade;
	}
	tid->p->is_upgrading_firmware = true;
	ret = tid_ops->firmware_upgrade(dev->parent, fw, force);
	tid->p->is_upgrading_firmware = false;

skip_upgrade:
	release_firmware(fw);
err:
	kfree(name);
	put_device(dev);

	return ret ? : count;
}

static size_t get_firmware_name(struct device *dev, char *buf, size_t size);

static ssize_t doreflash_store(struct device *dev,
							   struct device_attribute *attr, const char *buf,
							   size_t count)
{
	return firmware_upgrade(dev, buf, count, false);
}

static ssize_t doreflash_show(struct device *dev,
							  struct device_attribute *attr, char *buf)
{
	char fw_name[64];
	size_t count = get_firmware_name(dev, fw_name, ARRAY_SIZE(fw_name));
	int ret;

	ret = firmware_upgrade(dev, fw_name, count, false);
	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "doreflash: %s\n", fw_name);
}
static DEVICE_ATTR_RW(doreflash);

static ssize_t forcereflash_store(struct device *dev,
								  struct device_attribute *attr,
								  const char *buf, size_t count)
{
	return firmware_upgrade(dev, buf, count, true);
}

static ssize_t forcereflash_show(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	char fw_name[64];
	size_t count = get_firmware_name(dev, fw_name, ARRAY_SIZE(fw_name));
	int ret;

	ret = firmware_upgrade(dev, fw_name, count, true);
	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "forcereflash: %s\n", fw_name);
}
static DEVICE_ATTR_RW(forcereflash);

static inline int gpio_to_pin(struct pinctrl_gpio_range *range,
							  unsigned int gpio)
{
	unsigned int offset = gpio - range->base;

	return range->pins ? range->pins[offset] : range->pin_base + offset;
}

static int pinctrl_get_device_gpio_pin(unsigned int gpio,
									   struct pinctrl_dev **outdev, int *outpin)
{
	struct gpio_desc *gdesc = gpio_to_desc(gpio);
	struct gpio_pin_range *grange;
	struct list_head *head;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 5, 0)
	head = &gdesc->gdev->pin_ranges;
#else
	head = &gdesc->chip->pin_ranges;
#endif
	list_for_each_entry(grange, head, node) {
		struct pinctrl_gpio_range *range = &grange->range;

		if (gpio >= range->base && gpio < range->base + range->npins) {
			*outdev = grange->pctldev;
			*outpin = gpio_to_pin(range, gpio);
			return 0;
		}
	}
	return -ENODEV;
}

static int gpio_set_config(unsigned int gpio, unsigned long config)
{
	int ret, pin;
	unsigned int selector;
	char pin_group[16];
	const struct pinconf_ops *ops;
	struct pinctrl_dev *pctldev;

	ret = pinctrl_get_device_gpio_pin(gpio, &pctldev, &pin);
	if (ret)
		return ret;

	mutex_lock(&pctldev->mutex);
	ops = pctldev->desc->confops;
	if (!ops) {
		ret = -ENODEV;
		goto out;
	}

	ret = -ENODEV;
	if (ops->pin_config_set)
		ret = ops->pin_config_set(pctldev, pin, &config, 1);
	if (!ret)
		goto out;

	snprintf(pin_group, sizeof(pin_group), "gpio%d", pin);
	pr_debug("pin group name is '%s'", pin_group);
	selector = pinctrl_get_group_selector(pctldev, pin_group);
	if (ops->pin_config_group_set)
		ret = ops->pin_config_group_set(pctldev, selector, &config, 1);
out:
	mutex_unlock(&pctldev->mutex);

	return ret;
}

static bool gpio_get_config(unsigned int gpio, unsigned long config)
{
	int ret, pin;
	unsigned int selector;
	char pin_group[16];
	const struct pinconf_ops *ops;
	struct pinctrl_dev *pctldev;

	ret = pinctrl_get_device_gpio_pin(gpio, &pctldev, &pin);
	if (ret)
		return false;

	mutex_lock(&pctldev->mutex);
	ops = pctldev->desc->confops;
	if (!ops) {
		ret = -ENODEV;
		goto out;
	}
	ret = -ENODEV;
	if (ops->pin_config_get)
		ret = ops->pin_config_get(pctldev, pin, &config);
	if (ret == -EINVAL)
		goto out;

	if (!ret) {
		/* 1 means the config is set */
		ret = pinconf_to_config_argument(config);
		goto out;
	}

	snprintf(pin_group, sizeof(pin_group), "gpio%d", pin);
	pr_debug("pin group name is '%s'\n", pin_group);
	selector = pinctrl_get_group_selector(pctldev, pin_group);
	if (ops->pin_config_group_get)
		ret = ops->pin_config_group_get(pctldev, selector, &config);
	if (ret)
		goto out;
	/* 1 means the config is set */
	ret = pinconf_to_config_argument(config);
out:
	mutex_unlock(&pctldev->mutex);

	return ret > 0 ? true : false;
}

static int touch_gpio_set_config(unsigned int gpio,
								 const char *buf, size_t count)
{
	char *c;
	char arg[16] = { 0 };
	unsigned long config;

	if (!gpio_is_valid(gpio))
		return -EPERM;

	if (unlikely(count > sizeof(arg) - 1))
		return -EINVAL;
	memcpy(arg, buf, count);
	c = strnchr(arg, count, '\n');
	if (c)
		*c = '\0';
	arg[count] = '\0';

	if (!strcmp(arg, "no_pull")) {
		config = pinconf_to_config_packed(PIN_CONFIG_BIAS_DISABLE, 0);
	} else if (!strcmp(arg, "pull_up")) {
		config = pinconf_to_config_packed(PIN_CONFIG_BIAS_PULL_UP, 1);
	} else if (!strcmp(arg, "pull_down")) {
		config = pinconf_to_config_packed(PIN_CONFIG_BIAS_PULL_DOWN, 1);
	} else if (!strcmp(arg, "1") || !strcmp(arg, "high")) {
		gpio_set_value(gpio, 1);
		return 0;
	} else if (!strcmp(arg, "0") || !strcmp(arg, "low")) {
		gpio_set_value(gpio, 0);
		return 0;
	} else {
		return -EINVAL;
	}

	return gpio_set_config(gpio, config);
}

static const char *touch_gpio_get_config(unsigned int gpio)
{
	int i;
	static const char *const pulls[] = {
		"no_pull",
		"pull_up",
		"pull_down",
	};

	for (i = 0; i < ARRAY_SIZE(pulls); i++) {
		int pin_config_param[] = {
			PIN_CONFIG_BIAS_DISABLE,
			PIN_CONFIG_BIAS_PULL_UP,
			PIN_CONFIG_BIAS_PULL_DOWN,
		};
		unsigned long config;

		config = pinconf_to_config_packed(pin_config_param[i], 0);
		if (gpio_get_config(gpio, config))
			return pulls[i];
	}

	return NULL;
}

static struct irq_desc *gpio_to_irq_desc(struct device *dev, unsigned int gpio)
{
	int irq = -ENXIO;

	if (gpio_is_valid(gpio))
		irq = gpio_to_irq(gpio);

	if (unlikely(irq < 0)) {
		irq = of_irq_get(dev_of_node(dev), 0);
		if (irq <= 0)
			return NULL;
	}

	return irq_to_desc(irq);
}

static void gpio_seq_show(struct seq_file *s, struct device *dev,
						  unsigned int gpio)
{
	struct gpio_desc *gdesc = gpio_to_desc(gpio);
	int is_irq = test_bit(FLAG_USED_AS_IRQ, &gdesc->flags);

	gpiod_get_direction(gdesc);
	seq_printf(s, " %3d: gpio-%-3d (%-20.20s|%-20.20s) %-13s%-13s%-13s",
			  gpio, gpio_chip_hwgpio(gdesc),
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 3, 0)
			  gdesc->name ? :
#endif
			  "", gdesc->label,
			  touch_gpio_get_config(gpio) ? : "unknow",
			  test_bit(FLAG_IS_OUT, &gdesc->flags) ? "out" : "in",
			  gpio_get_value(gpio) ? "high" : "low");

	if (is_irq) {
		struct irq_desc *desc = gpio_to_irq_desc(dev, gpio);

		seq_printf(s, "%s", "IRQ");
		if (desc)
			seq_printf(s, "(irq: %3u hwirq: %3lu) %-8s", desc->irq_data.irq,
					   desc->irq_data.hwirq,
					   irqd_is_level_type(&desc->irq_data) ? "Level" : "Edge");
	}
	seq_putc(s, '\n');
}

static ssize_t touch_gpio_show(struct device *dev, unsigned int gpio, char *buf)
{
	size_t count;
	struct seq_file *s;

	if (!gpio_is_valid(gpio))
		return -EPERM;

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;
	s->buf = buf;
	s->size = PAGE_SIZE;
	gpio_seq_show(s, dev, gpio);
	count = s->count;
	kfree(s);

	return count;
}

static ssize_t irq_gpio_store(struct device *dev, struct device_attribute *attr,
							  const char *buf, size_t count)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return touch_gpio_set_config(tid->irq_gpio, buf, count) ? : count;
}

static ssize_t irq_gpio_show(struct device *dev, struct device_attribute *attr,
							 char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return touch_gpio_show(dev, tid->irq_gpio, buf);
}
static DEVICE_ATTR_RW(irq_gpio);

static ssize_t rst_gpio_store(struct device *dev, struct device_attribute *attr,
							  const char *buf, size_t count)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return touch_gpio_set_config(tid->rst_gpio, buf, count) ? : count;
}

static ssize_t rst_gpio_show(struct device *dev, struct device_attribute *attr,
							 char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return touch_gpio_show(dev, tid->rst_gpio, buf);
}
static DEVICE_ATTR_RW(rst_gpio);

static ssize_t firmware_name_show(struct device *dev,
								  struct device_attribute *attr, char *buf)
{
	size_t count = get_firmware_name(dev, buf, PAGE_SIZE);

	return strlcat(buf, "\n", PAGE_SIZE - count);
}
static DEVICE_ATTR_RO(firmware_name);

static ssize_t disable_depth_show(struct device *dev,
								  struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct irq_desc *desc = gpio_to_irq_desc(dev, tid->irq_gpio);

	if (unlikely(!desc))
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%u\n", desc->depth);
}
static DEVICE_ATTR_RO(disable_depth);

static ssize_t wake_depth_show(struct device *dev,
							   struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct irq_desc *desc = gpio_to_irq_desc(dev, tid->irq_gpio);

	if (unlikely(!desc))
		return -ENODEV;

	return scnprintf(buf, PAGE_SIZE, "%u\n", desc->wake_depth);
}
static DEVICE_ATTR_RO(wake_depth);

static inline int __regulator_is_enabled(struct regulator_dev *rdev)
{
	/* A GPIO control always takes precedence */
	if (rdev->ena_pin)
		return rdev->ena_gpio_state;

	/* If we don't know then assume that the regulator is always on */
	if (!rdev->desc->ops->is_enabled)
		return 1;

	return rdev->desc->ops->is_enabled(rdev);
}

static void regulator_consumer_show(struct seq_file *s,
									struct regulator_dev *rdev)
{
	struct regulator *reg;

	mutex_lock(&rdev->mutex);
	/* Print a header if there are consumers. */
	if (rdev->open_count)
		seq_printf(s, "%-32s EN        Min_uV   Max_uV  load_uA   "
				   "%-32s use_count: %-8u enabled: %-8c\n",
				   "Device-Supply", rdev->desc->name, rdev->use_count,
				   __regulator_is_enabled(rdev) ? 'Y' : 'N');

	list_for_each_entry(reg, &rdev->consumer_list, list)
		seq_printf(s, "%-32s %c(%2d)   %8d %8d %8d\n",
				   reg->supply_name ? : "(null)-(null)",
				   reg->enabled ? 'Y' : 'N', reg->enabled,
				   reg->min_uV, reg->max_uV, reg->uA_load);

	mutex_unlock(&rdev->mutex);
}

static void regulator_consumers_show(struct seq_file *s, struct device *dev)
{
	struct device_node *np = dev_of_node(dev);
	struct	property *prop;

	if (!np)
		return;

	for_each_property_of_node(np, prop) {
		char *find;
		char *name;
		struct regulator *reg;
		struct regulator_dev *rdev;

		if (!(find = strstr(prop->name, "-supply")) ||
			strcmp(find, "-supply"))
			continue;
		name = kzalloc(find - prop->name + 1, GFP_KERNEL);
		if (unlikely(!name))
			return;
		memcpy(name, prop->name, find - prop->name);
		dev_dbg(dev, "regulator name is '%s'\n", prop->name);
		reg = regulator_get(dev->parent, name);
		kfree(name);
		if (!reg) {
			dev_err(dev, "get regulator(%s) fail\n", prop->name);
			continue;
		}
		rdev = reg->rdev;
		regulator_put(reg);
		regulator_consumer_show(s, rdev);
		seq_putc(s, '\n');
	}
}

static ssize_t regulator_show(struct device *dev,
							  struct device_attribute *attr, char *buf)
{
	size_t count;
	struct seq_file *s;

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (!s)
		return -ENOMEM;
	s->buf = buf;
	s->size = PAGE_SIZE;
	regulator_consumers_show(s, dev);
	count = s->count;
	kfree(s);

	return count;
}
static DEVICE_ATTR_RO(regulator);

#ifdef CONFIG_TOUCHSCREEN_OPEN_SHORT_TEST
static ssize_t ini_file_name_store(struct device *dev,
								   struct device_attribute *attr,
								   const char *buf, size_t count)
{
	char *c;
	char *name;
	struct touch_info_dev *tid = dev_to_tid(dev);

	name = kmalloc(count + 1, GFP_KERNEL);
	if (!name)
		return -ENOMEM;

	kfree(tid->p->ini_def_name);
	tid->p->ini_def_name = name;
	memcpy(name, buf, count);

	if ((c = strnchr(name, count, '\n')))
		*c = '\0';
	name[count] = 0;

	if (!strcmp(name, "off")) {
		kfree(name);
		tid->p->ini_def_name = NULL;
		name = "default setting";
	}
	dev_dbg(dev, "modify ini file name to '%s'\n", name);

	return count;
}

static size_t get_ini_name(struct device *dev, char *buf, size_t size);

static ssize_t ini_file_name_show(struct device *dev,
								  struct device_attribute *attr, char *buf)
{
	size_t count;
	struct touch_info_dev *tid = dev_to_tid(dev);

	if (tid->p->ini_def_name)
		return scnprintf(buf, PAGE_SIZE, "%s\n", tid->p->ini_def_name);

	count = get_ini_name(dev, buf, PAGE_SIZE);

	return strlcat(buf, "\n", PAGE_SIZE - count);
}
static DEVICE_ATTR_RW(ini_file_name);

static int open_short_test(struct device *dev);

static ssize_t open_short_test_show(struct device *dev,
									struct device_attribute *attr, char *buf)
{
	int ret = open_short_test(dev);

	if (ret < 0)
		return ret;

	return scnprintf(buf, PAGE_SIZE, "result=%d\n", !!ret);
}
static DEVICE_ATTR_RO(open_short_test);
#endif /* CONFIG_TOUCHSCREEN_OPEN_SHORT_TEST */

#ifdef CONFIG_TOUCHSCREEN_GESTURE
static ssize_t gesture_data_show(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", atomic_read(&tid->p->wakeup_code));
}
static DEVICE_ATTR_RO(gesture_data);

static ssize_t gesture_name_show(struct device *dev,
								 struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return scnprintf(buf, PAGE_SIZE, "%s\n", tid->p->wakeup_code_name);
}
static DEVICE_ATTR_RO(gesture_name);

static ssize_t gesture_control_show(struct device *dev,
									struct device_attribute *attr, char *buf)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return scnprintf(buf, PAGE_SIZE, "%x\n", atomic_read(&tid->p->mask));
}

static ssize_t gesture_control_store(struct device *dev,
									 struct device_attribute *attr,
									 const char *buf, size_t count)
{
	bool enable;
	int ret = 0;
	unsigned int mask;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;

	if (!is_poweron(dev)) {
		dev_err(dev, "not allow gesture control(power off)\n");
		return -EPERM;
	}

	if (count != sizeof(mask)) {
		if (sscanf(buf, "%x", &mask) != 1) {
			dev_dbg(dev, "input parameter is invalid: %s\n", buf);
			return -EINVAL;
		}
	} else {
		memcpy(&mask, buf, count);
	}

	enable = !!(mask & BIT(GS_KEY_ENABLE));
	if (enable) {
		mask &= BIT(GS_KEY_ENABLE) | (BIT(GS_KEY_END) - 1);
		dev_dbg(dev, "enable gesture, mask: 0x%08x\n", mask);
	} else {
		mask = 0;
		dev_dbg(dev, "disable all gesture\n");
	}
	atomic_set(&tid->p->mask, mask);

	if (likely(tid_ops && tid_ops->gesture_set_capability))
		ret = tid_ops->gesture_set_capability(dev->parent, enable);
	else
		dev_dbg(dev, "gesture_set_capability interface is not set\n");

	return ret ? : count;
}
static DEVICE_ATTR_RW(gesture_control);
#endif /* CONFIG_TOUCHSCREEN_GESTURE */

static struct attribute *touch_info_dev_attrs[] = {
	&dev_attr_reset.attr,
	&dev_attr_buildid.attr,
	&dev_attr_doreflash.attr,
	&dev_attr_flashprog.attr,
	&dev_attr_forcereflash.attr,
	&dev_attr_productinfo.attr,
	&dev_attr_poweron.attr,
	&dev_attr_path.attr,
	&dev_attr_vendor.attr,
	&dev_attr_irq_gpio.attr,
	&dev_attr_rst_gpio.attr,
	&dev_attr_firmware_name.attr,
	&dev_attr_disable_depth.attr,
	&dev_attr_wake_depth.attr,
	&dev_attr_regulator.attr,
#ifdef CONFIG_TOUCHSCREEN_OPEN_SHORT_TEST
	&dev_attr_ini_file_name.attr,
	&dev_attr_open_short_test.attr,
#endif
#ifdef CONFIG_TOUCHSCREEN_GESTURE
	&dev_attr_gesture_data.attr,
	&dev_attr_gesture_name.attr,
	&dev_attr_gesture_control.attr,
#endif
	NULL,
};
ATTRIBUTE_GROUPS(touch_info_dev);

static struct class touchscreen_class = {
	.owner      = THIS_MODULE,
	.name       = TOUCHSCREEN_CLASS_NAME,
	.dev_groups = touch_info_dev_groups,
};

static int __match_name(struct device *dev, const void *data)
{
	return !strcmp(dev_name(dev), data);
}

static struct touch_info_dev *find_tid_by_name(const char *name)
{
	struct device *dev;

	dev = class_find_device(&touchscreen_class, NULL, name, __match_name);
	if (!dev)
		return NULL;
	put_device(dev);

	return dev_to_tid(dev);
}

static struct touch_info_dev *find_default_tid(void)
{
	static struct touch_info_dev *tid;

	if (unlikely(!tid)) {
		struct device *dev;

		/* only search once in order to optimize performance */
		tid = find_tid_by_name(DEFAULT_DEVICE_NAME);
		if (unlikely(!tid)) {
			pr_info("any devices is not found\n");
			return NULL;
		}
		dev = tid_to_dev(tid);
		dev_dbg(dev, "by default, it matches with the device: %s\n",
				dev_name(dev));
	}

	return tid;
}

#ifdef CONFIG_TOUCHSCREEN_PROC_INTERFACE
#define TOUCH_PROC_DIR      "touchscreen"
#define GESTURE_PROC_DIR    "gesture"

/**
 * open-short test implemention
 */
#ifdef CONFIG_TOUCHSCREEN_OPEN_SHORT_TEST
#define TOUCH_OS_TEST    "ctp_openshort_test"

#ifdef CONFIG_TOUCHSCREEN_OPEN_SHORT_TEST_STORE_RESULT
#include <linux/fs.h>

#define RESULT_PATH "/data/anr/open_short_result.txt"


static int open_short_write_to_file(const void *buf, size_t count)
{
	int ret;
	struct file *file;
	loff_t pos = 0;
	struct inode *inode;

	if (unlikely(!buf || !count))
		return -EINVAL;

	file = filp_open(RESULT_PATH, O_RDWR | O_APPEND | O_CREAT, 0600);
	if (IS_ERR(file))
		return PTR_ERR(file);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0)
	inode = file->f_inode;
#else
	inode = file->f_path.dentry->d_inode;
#endif
	/**
	 * If the file size exceeds 2MB, we delete the file.
	 */
	if (unlikely(i_size_read(inode) > SZ_2M)) {
		pr_debug("file size exceeds 2MB\n");
		filp_close(file, NULL);
		file = filp_open(RESULT_PATH, O_RDWR | O_APPEND | O_CREAT, 0600);
		if (IS_ERR(file))
			return PTR_ERR(file);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 13, 0)
	ret = kernel_write(file, buf, count, &pos);
#else
	ret = kernel_write(file, buf, count, pos);
#endif
	if (unlikely(ret < 0))
		pr_debug("error writing open-short result file: %s\n", RESULT_PATH);
	filp_close(file, NULL);

	return (ret < 0) ? -EIO : 0;
}

static int seq_file_buf_init(struct seq_file *s)
{
	s->count = 0;
	s->size = PAGE_SIZE;
	s->buf = (void *)__get_free_page(GFP_KERNEL);
	if (unlikely(!s->buf)) {
		s->size = 0;
		return -ENOMEM;
	}

	return 0;
}
#else
static int open_short_write_to_file(const void *buf, size_t count)
{
	return 0;
}

static int seq_file_buf_init(struct seq_file *s)
{
	s->buf = NULL;
	s->size = 0;
	s->count = 0;

	return 0;
}
#endif /* CONFIG_TOUCHSCREEN_OPEN_SHORT_TEST_STORE_RESULT */

static const char *get_panel_maker(struct device *dev);
static const char *get_panel_color(struct device *dev);

static size_t get_ini_name(struct device *dev, char *buf, size_t size)
{
	const char *color = NULL;
	struct touch_info_dev *tid = dev_to_tid(dev);

	get_panel_maker(dev);
	if (tid->ini_name_use_color)
		color = get_panel_color(dev);

	return snprintf(buf, size, color ? "%s-%s-%s-%s.ini" : "%s-%s-%s.ini",
					tid->vendor, tid->product, tid->panel_maker ? : "none",
					color);
}

static int open_short_test(struct device *dev)
{
	int ret;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;
	struct seq_file *s;
	char ini_name[64];
	const struct firmware *fw = NULL;
	const char *ini_def_name = tid->p->ini_def_name;

	if (!is_poweron(dev)) {
		dev_err(dev, "not allow open-short test(power off)\n");
		return -EPERM;
	}

	if (unlikely(!tid_ops || !tid_ops->open_short_test)) {
		dev_info(dev, "open-short test interface is invalid\n");
		return -ENODEV;
	}

	if (!ini_def_name) {
		get_ini_name(dev, ini_name, ARRAY_SIZE(ini_name));
		ini_def_name = ini_name;
	}
	dev_dbg(dev, "ini file name is '%s'\n", ini_def_name);

	get_device(dev);
	if (!tid->open_short_not_use_fw) {
		ret = request_firmware_direct(&fw, ini_def_name, dev);
		if (unlikely(ret))
			goto put_dev;
	}

	s = kzalloc(sizeof(*s), GFP_KERNEL);
	if (unlikely(!s)) {
		dev_err(dev, "%s: alloc memory fail\n", __func__);
		ret = -ENOMEM;
		goto release_fw;
	}
	ret = seq_file_buf_init(s);
	if (unlikely(ret)) {
		dev_err(dev, "%s: alloc memory fail, result will not write to file\n",
				__func__);
		/**
		 * Should we stop? If we continue, the result of this open-short test
		 * will not write to the file. But I don't care. Do you care?
		 *
		 * kfree(s);
		 * ret = -ENOMEM;
		 * goto release_fw;
		 */
	}

	dev_info(dev, "open-short test start\n");
	ret = tid_ops->open_short_test(dev->parent, s, fw);
	if (ret < 0) {
		dev_info(dev, "%s fail with errno: %d\n", "open-short test", ret);
		seq_printf(s, "%s fail with errno: %d\n", "open-short test", ret);
	} else {
		dev_info(dev, "%s %s\n", "open-short test", ret ? "pass" : "fail");
		seq_printf(s, "%s %s\n", "open-short test", ret ? "pass" : "fail");
	}
	seq_putc(s, '\n');

	if (s->count && open_short_write_to_file(s->buf, s->count))
		dev_err(dev, "write open short test result file fail\n");

	free_page((unsigned long)s->buf);
	kfree(s);
release_fw:
	release_firmware(fw);
put_dev:
	put_device(dev);

	return ret;
}

static int open_short_proc_show(struct seq_file *seq, void *offset)
{
	int ret = open_short_test(seq->private);

	if (ret < 0)
		return ret;
	seq_printf(seq, "result=%d\n", !!ret);

	return 0;
}

PROC_ENTRY_RO(open_short);

static inline void open_short_proc_create(struct device *dev)
{
	proc_create_data(TOUCH_PROC_DIR "/" TOUCH_OS_TEST, S_IRUGO,
					 NULL, &open_short_fops, dev);
}

static inline void open_short_proc_remove(void)
{
	remove_proc_entry(TOUCH_PROC_DIR "/" TOUCH_OS_TEST, NULL);
}
#else
static inline void open_short_proc_create(struct device *dev)
{
}

static inline void open_short_proc_remove(void)
{
}
#endif /* CONFIG_TOUCHSCREEN_OPEN_SHORT_TEST */

/**
 * lockdown infomation show implemention
 */
#ifdef CONFIG_TOUCHSCREEN_LOCKDOWN_INFO
#define TOUCH_LOCKDOWN_INFO         "lockdown_info"
#define LOCKDOWN_INFO_MAGIC_BASE    0x31

static bool is_lockdown_valid(const char *buf)
{
	char value[LOCKDOWN_INFO_SIZE] = { 0 };

	if (unlikely(!buf))
		return false;

	return !!memcmp(buf, value, ARRAY_SIZE(value));
}

static int get_lockdown_info(struct device *dev, char *buf)
{
	int ret;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;
	static char lockdown_buf[LOCKDOWN_INFO_SIZE] = { 0 };

retry:
	if (likely(is_lockdown_valid(lockdown_buf))) {
		memcpy(buf, lockdown_buf, LOCKDOWN_INFO_SIZE);
		return 0;
	}

	if (unlikely(!tid_ops || !tid_ops->get_lockdown_info))
		return -ENODEV;

	ret = tid_ops->get_lockdown_info(dev->parent, lockdown_buf);
	if (!ret)
		goto retry;

	return ret;
}

static const char *get_panel_maker(struct device *dev)
{
	int index;
	char buf[LOCKDOWN_INFO_SIZE] = { 0 };
	struct touch_info_dev *tid = dev_to_tid(dev);
	static const char *const panel_makers[] = {
		"biel-tpb",        /* 0x31 */
		"lens",
		"wintek",
		"ofilm",
		"biel-d1",         /* 0x35 */
		"tpk",
		"laibao",
		"sharp",
		"jdi",
		"eely",            /* 0x40 */
		"gis-ebbg",
		"lgd",
		"auo",
		"boe",
		"ds-mudong",       /* 0x45 */
		"tianma",
		"truly",
		"sdc",
		"primax",
		"cdot",            /* 0x50 */
		"djn",
	};

	if (likely(tid->panel_maker))
		return tid->panel_maker;

	if (unlikely(get_lockdown_info(dev, buf))) {
		dev_err(dev, "get panel maker fail\n");
		return NULL;
	}

	/**
	 * why is 6? 0x?a, 0x?b, 0x?c, 0x?d, 0x?e, and 0x?f is ignored.
	 */
	index = buf[LOCKDOWN_INFO_PANEL_MAKER_INDEX] - LOCKDOWN_INFO_MAGIC_BASE;
	index -= ((index + 1) >> 4) * 6;
	if (unlikely(index >= ARRAY_SIZE(panel_makers))) {
		dev_err(dev, "panel maker lockdown info is invalid\n");
		return NULL;
	}
	tid->panel_maker = panel_makers[index];

	return panel_makers[index];
}

static const char *get_panel_color(struct device *dev)
{
	int index;
	char buf[LOCKDOWN_INFO_SIZE] = { 0 };
	struct touch_info_dev *tid = dev_to_tid(dev);
	static const char *const panel_colors[] = {
		"white",        /* 0x31 */
		"black",
		"red",
		"yellow",
		"green",        /* 0x35 */
		"pink",
		"purple",
		"golden",
		"silver",
		"gray",         /* 0x40 */
		"blue",
		"pink-purple",
	};

	if (likely(tid->panel_color))
		return tid->panel_color;

	if (unlikely(get_lockdown_info(dev, buf))) {
		dev_err(dev, "get panel color fail\n");
		return NULL;
	}

	/**
	 * why is 6? 0x?a, 0x?b, 0x?c, 0x?d, 0x?e, and 0x?f is ignored.
	 */
	index = buf[LOCKDOWN_INFO_PANEL_COLOR_INDEX] - LOCKDOWN_INFO_MAGIC_BASE;
	index -= ((index + 1) >> 4) * 6;
	if (unlikely(index >= ARRAY_SIZE(panel_colors))) {
		dev_err(dev, "panel color lockdown info is invalid\n");
		return NULL;
	}
	tid->panel_color = panel_colors[index];

	return panel_colors[index];
}

static int get_hardware_id(struct device *dev)
{
	int hw_id;
	char buf[LOCKDOWN_INFO_SIZE] = { 0 };

	if (unlikely(get_lockdown_info(dev, buf))) {
		dev_err(dev, "get hardware id fail\n");
		return 0;
	}

	hw_id = buf[LOCKDOWN_INFO_HW_VERSION_INDEX];

	return hw_id;
}

static int lockdown_info_proc_show(struct seq_file *seq, void *offset)
{
	int i;
	int ret;
	char buf[LOCKDOWN_INFO_SIZE] = { 0 };

	ret = get_lockdown_info(seq->private, buf);
	if (unlikely(ret))
		return ret;

	/* lockdown info is only LOCKDOWN_INFO_SIZE bytes */
	for (i = 0; i < ARRAY_SIZE(buf); i++)
		seq_printf(seq, "%02x", buf[i]);
	seq_printf(seq, "\n");

	return ret;
}

PROC_ENTRY_RO(lockdown_info);

static inline void lockdown_info_proc_create(struct device *dev)
{
	proc_create_data(TOUCH_PROC_DIR "/" TOUCH_LOCKDOWN_INFO, S_IRUGO,
					 NULL, &lockdown_info_fops, dev);
}

static inline void lockdown_info_proc_remove(void)
{
	remove_proc_entry(TOUCH_PROC_DIR "/" TOUCH_LOCKDOWN_INFO, NULL);
}
#else
static const char *get_panel_maker(struct device *dev)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return tid->panel_maker;
}

static const char *get_panel_color(struct device *dev)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return tid->panel_color;
}

static inline int get_hardware_id(struct device *dev)
{
	return 0;
}

static inline void lockdown_info_proc_create(struct device *dev)
{
}

static inline void lockdown_info_proc_remove(void)
{
}
#endif /* CONFIG_TOUCHSCREEN_LOCKDOWN_INFO */

/**
 * gesture on/off and data implemention
 */
#ifdef CONFIG_TOUCHSCREEN_GESTURE
#define GESTURE_ON_OFF      "onoff"
#define GESTURE_DATA        "data"

/**
 * gesture on/off implemention
 */
static int gesture_control_proc_show(struct seq_file *seq, void *offset)
{
	struct touch_info_dev *tid = dev_to_tid(seq->private);
	unsigned int bit = BIT(GS_KEY_ENABLE) | BIT(GS_KEY_DOUBLE_TAP);

	seq_printf(seq, "%d\n", (atomic_read(&tid->p->mask) & bit) == bit);

	return 0;
}

static ssize_t gesture_control_proc_write(struct file *file,
										  const char __user * ubuf, size_t size,
										  loff_t * ppos)
{
	int ret = 0;
	bool enable;
	struct seq_file *seq = file->private_data;
	struct device *dev = seq->private;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;
	char buf[4];

	if (!is_poweron(dev)) {
		dev_err(dev, "not allow gesture control(power off)\n");
		return -EPERM;
	}

	if (unlikely(size > sizeof(buf)))
		return -EINVAL;

	if (unlikely(copy_from_user(buf, ubuf, size)))
		return -EFAULT;
	buf[sizeof(buf) - 1] = '\0';

	if (buf[0] == '1' || !strncmp(buf, "on", 2))
		enable = true;
	else if (buf[0] == '0' || !strncmp(buf, "off", 3))
		enable = false;
	else
		return -EINVAL;

	if (likely(tid_ops && tid_ops->gesture_set_capability))
		ret = tid_ops->gesture_set_capability(dev->parent, enable);
	else
		dev_dbg(dev, "gesture_set_capability interface is not set\n");
	atomic_set(&tid->p->mask,
			   enable ? (BIT(GS_KEY_ENABLE) | BIT(GS_KEY_DOUBLE_TAP)) : 0);
	*ppos += size;

	return ret ? : size;
}

PROC_ENTRY_RW(gesture_control);

/**
 * gesture data implemention
 */
static int gesture_data_proc_show(struct seq_file *seq, void *offset)
{
	seq_printf(seq, "K\n");

	return 0;
}

PROC_ENTRY_RO(gesture_data);

static inline void gesture_proc_create(struct device *dev)
{
	proc_create_data(GESTURE_PROC_DIR "/" GESTURE_ON_OFF, S_IRUGO | S_IWUGO,
					 NULL, &gesture_control_fops, dev);
	proc_create_data(GESTURE_PROC_DIR "/" GESTURE_DATA, S_IRUGO,
					 NULL, &gesture_data_fops, dev);
}

static inline void gesture_proc_remove(void)
{
	remove_proc_entry(GESTURE_PROC_DIR "/" GESTURE_ON_OFF, NULL);
	remove_proc_entry(GESTURE_PROC_DIR "/" GESTURE_DATA, NULL);
}
#else
static inline void gesture_proc_create(struct device *dev)
{
}

static inline void gesture_proc_remove(void)
{
}
#endif /* CONFIG_TOUCHSCREEN_GESTURE */

/**
 * The first device of create proc node. Beacause the same proc node should be
 * created only once.
 */
static inline struct device **get_proc_owner(void)
{
	static struct device *owner = NULL;

	return &owner;
}

static void touch_proc_add_device(struct device *dev)
{
	struct device **owner = get_proc_owner();

	BUG_ON(!dev);
	if (*owner) {
		dev_info(dev, "create proc again, just return\n");
		return;
	}
	*owner = dev;

	open_short_proc_create(dev);
	lockdown_info_proc_create(dev);
	gesture_proc_create(dev);
}

static void touch_proc_del_device(struct device *dev)
{
	struct device **owner = get_proc_owner();

	if (*owner != dev)
		return;
	*owner = NULL;

	open_short_proc_remove();
	lockdown_info_proc_remove();
	gesture_proc_remove();
}

static void create_proc(void)
{
	proc_mkdir(TOUCH_PROC_DIR, NULL);
	proc_mkdir(GESTURE_PROC_DIR, NULL);
}
#else
static const char *get_panel_maker(struct device *dev)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return tid->panel_maker;
}

static const char *get_panel_color(struct device *dev)
{
	struct touch_info_dev *tid = dev_to_tid(dev);

	return tid->panel_color;
}

static inline int get_hardware_id(struct device *dev)
{
	return 0;
}

static inline void touch_proc_add_device(struct device *dev)
{
}

static inline void touch_proc_del_device(struct device *dev)
{
}

static inline void create_proc(void)
{
}
#endif /* CONFIG_TOUCHSCREEN_PROC_INTERFACE */

#ifdef CONFIG_TOUCHSCREEN_GESTURE
enum {
	/* tap */
	DOUBLE_TAP = 0x270,
	ONECE_TAP,
	LONG_PRESS,

	/* swipe */
	SWIPE_X_LEFT = 0x280,
	SWIPE_X_RIGHT,
	SWIPE_Y_UP,
	SWIPE_Y_DOWN,

	/* unicode */
	UNICODE_E = 0x290,
	UNICODE_C,
	UNICODE_W,
	UNICODE_M,
	UNICODE_O,
	UNICODE_S,
	UNICODE_V,
	UNICODE_Z = UNICODE_V + 4,
};

/**
 * @support_codes, @code_names and @enum gesture_key must match one by one.
 *
 * Note: @enum gesture_key defined in touch-info.h.
 */
static const unsigned int support_codes[] = {
	SWIPE_X_LEFT,
	SWIPE_X_RIGHT,
	SWIPE_Y_UP,
	SWIPE_Y_DOWN,
	DOUBLE_TAP,
	ONECE_TAP,
	LONG_PRESS,
	UNICODE_E,
	UNICODE_C,
	UNICODE_W,
	UNICODE_M,
	UNICODE_O,
	UNICODE_S,
	UNICODE_V,
	UNICODE_Z,
};

static const char *const code_names[] = {
	"swipe_left",
	"swipe_right",
	"swipe_up",
	"swipe_down",
	"double_tap",
	"once_tap",
	"long_press",
	"e",
	"c",
	"w",
	"m",
	"o",
	"s",
	"v",
	"z",
};

static int gesture_report_input_dev_init(struct device *dev)
{
	int i;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct input_dev *input_dev = tid->p->input_dev;
	unsigned int codes[ARRAY_SIZE(support_codes)];

	/* Init and register input device */
	input_dev->name = "touchpanel-input";
	input_dev->id.bustype = BUS_HOST;
	input_set_drvdata(input_dev, tid);

	if (!of_property_read_u32_array(dev_of_node(dev), "touchpanel,codes", codes,
									ARRAY_SIZE(codes))) {
		unsigned int *code = (void *)support_codes;

		for (i = 0; i < ARRAY_SIZE(codes); i++, code++)
			if (codes[i]) {
				dev_dbg(dev, "modify code(support_codes[%d]) "
						"from 0x%x to 0x%x\n", i,
						support_codes[i], codes[i]);
				/**
				 * Do you think I am crazy?
				 * I am trying to modify a read-only variable.
				 */
				*code = codes[i];
			}
	}
	for (i = 0; i < ARRAY_SIZE(support_codes); i++)
		input_set_capability(input_dev, EV_KEY, support_codes[i]);

	return input_register_device(input_dev);
}

#define SYSFS_GESTURE_CLASS_NAME    "tp"

static void remove_link(void *data)
{
	sysfs_remove_link(data, SYSFS_GESTURE_CLASS_NAME);
}

struct class *__weak tid_creat_gesture_link_hook(void)
{
	return NULL;
}

static int gesture_report_init(struct device *dev)
{
	int ret;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct tid_private *p = tid->p;
	struct class *tp_class = tid_creat_gesture_link_hook();

	BUG_ON(GS_KEY_END != ARRAY_SIZE(support_codes));
	BUG_ON(GS_KEY_END != ARRAY_SIZE(code_names));

	/**
	 * Allocate and register input device.
	 */
	p->input_dev = devm_input_allocate_device(dev);
	if (unlikely(!p->input_dev)) {
		dev_err(dev, "failed to allocate input device\n");
		return -ENOMEM;
	}
	ret = gesture_report_input_dev_init(dev);
	if (unlikely(ret)) {
		dev_err(dev, "failed to register input device\n");
		return ret;
	}

	if (tp_class) {
		struct kobject *kobj = &tp_class->p->subsys.kobj;

		ret = sysfs_create_link(kobj, &dev->kobj, SYSFS_GESTURE_CLASS_NAME);
		if (unlikely(ret)) {
			dev_err(dev, "failed to create link\n");
			return ret;
		}
		ret = devm_add_action(dev, remove_link, kobj);
		if (unlikely(ret)) {
			sysfs_remove_link(kobj, SYSFS_GESTURE_CLASS_NAME);
			dev_err(dev, "failed to add action :%s\n", __func__);
			return ret;
		}
	}

	/* set default keycode name */
	p->wakeup_code_name = code_names[GS_KEY_DOUBLE_TAP];

	return 0;
}

/**
 * tid_report_key: - report new input event
 * @key: event code
 */
int tid_report_key(enum gesture_key key)
{
	struct device *dev;
	struct touch_info_dev *tid;
	unsigned int code;
	unsigned int mask;
	const char *gesture_name;

	if (unlikely(key >= GS_KEY_END))
		return -EINVAL;

	tid = find_default_tid();
	if (unlikely(!tid))
		return -ENODEV;
	dev = tid_to_dev(tid);
	get_device(dev);

	code = support_codes[key];
	gesture_name = code_names[key];
	mask = atomic_read(&tid->p->mask);
	if (unlikely(!(mask & BIT(GS_KEY_ENABLE)))) {
		dev_dbg(dev, "all gestures has disabled, ignore this code: 0x%x(%s)\n",
				code, gesture_name);
		goto out;
	}

	if (mask & BIT(key)) {
		struct input_dev *input_dev = tid->p->input_dev;

		tid->p->wakeup_code_name = gesture_name;
		atomic_set(&tid->p->wakeup_code, code);
		input_report_key(input_dev, code, 1);
		input_sync(input_dev);
		input_report_key(input_dev, code, 0);
		input_sync(input_dev);

		dev_dbg(dev, "input report keycode: 0x%x(%s)\n",
				code, gesture_name);
	} else {
		dev_dbg(dev, "ignore code: 0x%x(%s), according to mask: %08x\n", code,
				gesture_name, mask);
	}
out:
	put_device(dev);

	return 0;
}
EXPORT_SYMBOL(tid_report_key);
#else
static int gesture_report_init(struct device *dev)
{
	return 0;
}
#endif /* CONFIG_TOUCHSCREEN_GESTURE */

static void devm_tid_release(struct device *dev, void *res)
{
	struct touch_info_dev **tid = res;

	touch_info_dev_unregister(*tid);
}

static int devm_tid_match(struct device *dev, void *res, void *data)
{
	struct touch_info_dev **this = res, **tid = data;

	return *this == *tid;
}

/**
 * devm_touch_info_dev_allocate: - allocate memory for touch_info_dev
 * @dev:       pointer to the caller device
 * @alloc_ops: whether allocate memory for touch_info_dev_operations.
 *  	if @alloc_ops is %true, the function will allocate memory for
 *  	touch_info_dev_operations. if @alloc_ops is %false, it will not.
 **/
struct touch_info_dev *devm_touch_info_dev_allocate(struct device *dev,
													bool alloc_ops)
{
	struct touch_info_dev *tid;

	tid = devm_kzalloc(dev, sizeof(*tid), GFP_KERNEL);
	if (unlikely(!tid))
		return NULL;

	if (alloc_ops) {
		struct touch_info_dev_operations *tid_ops;

		tid_ops = devm_kzalloc(dev, sizeof(*tid_ops), GFP_KERNEL);
		if (unlikely(!tid_ops)) {
			devm_kfree(dev, tid);
			return NULL;
		}
		tid->tid_ops = tid_ops;
	}

	/**
	 * all other members have been cleared and do not need to be reinitialized
	 */
	tid->rst_gpio = -1;
	tid->irq_gpio = -1;

	return tid;
}
EXPORT_SYMBOL(devm_touch_info_dev_allocate);

/**
 * devm_touch_info_dev_register: - create a device for a managed device
 * @dev:    pointer to the caller device
 * @name:   name of new device to create
 * @tid:    the device infomation
 *
 * If an device allocated with this function needs to be freed
 * separately, devm_touch_info_dev_unregister() must be used.
 **/
int devm_touch_info_dev_register(struct device *dev, const char *name,
								 struct touch_info_dev *tid)
{
	struct touch_info_dev **dr;
	int ret;

	dr = devres_alloc(devm_tid_release, sizeof(tid), GFP_KERNEL);
	if (unlikely(!dr))
		return -ENOMEM;

	ret = touch_info_dev_register(dev, name, tid);
	if (unlikely(ret)) {
		devres_free(dr);
		return ret;
	}

	*dr = tid;
	devres_add(dev, dr);

	return 0;
}
EXPORT_SYMBOL(devm_touch_info_dev_register);

/**
 * devm_touch_info_dev_unregister: - destory the device
 * @dev:  device to destory
 * @tid:  the device infomation
 *
 * This function instead of touch_info_dev_unregister() should be used to
 * manually destory the device allocated with devm_touch_info_dev_register().
 **/
void devm_touch_info_dev_unregister(struct device *dev,
									struct touch_info_dev *tid)
{
	WARN_ON(devres_release(dev, devm_tid_release, devm_tid_match, &tid));
}
EXPORT_SYMBOL(devm_touch_info_dev_unregister);

#define of_property_read_string_and_check(np, prop)                           \
	do {                                                                      \
		if (!tid->prop) {                                                     \
			if (of_property_read_string(np, "touchpanel,"#prop, &tid->prop))  \
				pr_debug("%s is not set\n", #prop);                           \
		}                                                                     \
	} while (0)

#define of_property_read_bool_and_set(np, prop)                               \
	do {                                                                      \
		if (of_property_read_bool(np, "touchpanel,"#prop))                    \
			tid->prop = true;                                                 \
	} while (0)

/**
 * From now on, we support passing parameters via device tree(dts).
 *
 * The valid properties are as follows(i.e.):
 *     touchpanel,vendor = "focal";
 *     touchpanel,product = "ft5336";
 *     touchpanel,panel_maker = "boe";
 *     touchpanel,use_dev_path;
 *     touchpanel,fw_name_use_color;
 *     touchpanel,ini_name_use_color;
 *     touchpanel,open_short_not_use_fw;
 *     touchpanel,rst-gpio = <&tlmm 64 0x0>;
 *     touchpanel,irq-gpio = <&tlmm 65 0x0>;
 *
 * Note: Do not use 'touchpanel,use_dev_path', unless you know what this means.
 */
static int of_touch_info_dev_parse(struct device *dev)
{
	struct device_node *np = dev_of_node(dev);
	struct touch_info_dev *tid = dev_to_tid(dev);

	if (!np) {
		dev_dbg(dev, "%s: device node is not exist\n", __func__);
		return -ENODEV;
	}

	of_property_read_string_and_check(np, vendor);
	of_property_read_string_and_check(np, product);
	of_property_read_string_and_check(np, panel_maker);
	of_property_read_bool_and_set(np, use_dev_path);
	of_property_read_bool_and_set(np, fw_name_use_color);
	of_property_read_bool_and_set(np, ini_name_use_color);
	of_property_read_bool_and_set(np, open_short_not_use_fw);

	if (!gpio_is_valid(tid->rst_gpio))
		tid->rst_gpio = of_get_named_gpio(np, "touchpanel,rst-gpio", 0);
	if (!gpio_is_valid(tid->irq_gpio))
		tid->irq_gpio = of_get_named_gpio(np, "touchpanel,irq-gpio", 0);

	return 0;
}

int devres_release_all(struct device *dev);

static void remove_fb_notifier(void *data)
{
	fb_notifier_remove(data);
}

static void tid_release(struct device *dev)
{
	dev_dbg(dev, "device: '%s': remove\n", dev_name(dev));
}

/**
 * touch_info_dev_register: - create a device with some special file of sysfs
 * @dev:    pointer to the caller device
 * @name:   name of new device to create
 * @tid:    the device infomation
 *
 * If the @name is NULL, the name of created device will be "touchpanel".
 * You should call the touch_info_dev_unregister() to destory the device which
 * is created by touch_info_dev_register().
 **/
int touch_info_dev_register(struct device *dev, const char *name,
							struct touch_info_dev *tid)
{
	int ret;
	struct device *device;
	const char *dev_name = name ? : DEFAULT_DEVICE_NAME;
	struct tid_private *p;

	BUG_ON(!tid || !dev);
	if (find_tid_by_name(dev_name)) {
		pr_err("'%s' is already registered\n", dev_name);
		return -EEXIST;
	}

	device = tid_to_dev(tid);
	device_initialize(device);
	device->devt = MKDEV(0, 0);
	device->class = &touchscreen_class;
	device->parent = dev;
	device->release = tid_release;
	device->of_node = dev_of_node(dev);
	dev_set_drvdata(device, tid);
	ret = dev_set_name(device, "%s", dev_name);
	if (ret)
		goto error;
	ret = device_add(device);
	if (ret)
		goto error;

	p = devm_kzalloc(device, sizeof(*p), GFP_KERNEL);
	if (unlikely(!p)) {
		dev_err(device, "alloc memory fail\n");
		ret = -ENOMEM;
		goto unregister_dev;
	}
	tid->p = p;

	ret = gesture_report_init(device);
	if (unlikely(ret))
		goto unregister_dev;

	fb_notifier_init(device);
	ret = devm_add_action(device, remove_fb_notifier, device);
	if (unlikely(ret)) {
		fb_notifier_remove(dev);
		dev_err(device, "failed to add action: %s\n", __func__);
		goto unregister_dev;
	}
	touch_proc_add_device(device);
	of_touch_info_dev_parse(device);

	return 0;
unregister_dev:
	devres_release_all(device);
	device_del(device);
error:
	put_device(device);
	return ret;
}
EXPORT_SYMBOL(touch_info_dev_register);

/**
 * touch_info_dev_unregister: - destory the device which is created
 * via touch_info_dev_register()
 * @tid:  the device infomation
 *
 * You should call the touch_info_dev_unregister() to destory the device
 * which is created via touch_info_dev_register().
 **/
void touch_info_dev_unregister(struct touch_info_dev *tid)
{
	struct device *dev = tid_to_dev(tid);

	touch_proc_del_device(dev);
	devres_release_all(dev);
	device_unregister(dev);
}
EXPORT_SYMBOL(touch_info_dev_unregister);

/**
 * tid_hardware_info_get: - get hardware info and print it to the buf
 * @buf:  the buffer to store hardware info
 * @size: the buffer size
 *
 * The return value is the number of characters written into @buf not including
 * the trailing '\0'. If @size is == 0 the function returns 0. If something
 * error, it return errno.
 **/
int tid_hardware_info_get(char *buf, size_t size)
{
	int ret;
	const char *color = NULL;
	int minor = 0;
	struct device *dev;
	struct touch_info_dev *tid;
	struct touch_info_dev_operations *tid_ops;

	tid = find_default_tid();
	if (unlikely(!tid))
		return -ENODEV;
	dev = tid_to_dev(tid);
	get_device(dev);

	tid_ops = tid->tid_ops;
	if (likely(tid_ops && tid_ops->get_version)) {
		ret = tid_ops->get_version(dev->parent, NULL, &minor);
	} else {
		dev_dbg(dev, "get_version interface is not set\n");
		ret = 0;
	}
	if (unlikely(ret)) {
		dev_err(dev, "get version fail and set version 0\n");
		minor = 0;
	}

	get_panel_maker(dev);
	color = get_panel_color(dev);

	ret = scnprintf(buf, size, color ? "%s,%s,fw:0x%02X,%s" : "%s,%s,fw:0x%02X",
					tid->panel_maker ? : "none", tid->product, minor, color);
	dev_dbg(dev, "hardware info is '%s'\n", buf);
	put_device(dev);

	return ret;
}
EXPORT_SYMBOL(tid_hardware_info_get);

/**
 * In tid_upgrade_firmware_nowait(), I want to use a local variable to store
 * firmware name. But it does not work well when the kernel version below 4.4.
 * If you use the linux-4.15, you can just do this. Because the
 * request_firmware_nowait() will request memory to store firmware name. Details
 * can compare the request_firmware_nowait() function between linux-4.4 and
 * linux-4.15.
 */
struct firmware_context {
	struct device *dev;
	char firmware_name[64];
};

static void firmware_callback(const struct firmware *fw, void *context)
{
	int ret;
	int minor_new = 0, minor_old = 0;
	struct firmware_context *fw_context = context;
	struct device *dev = fw_context->dev;
	struct touch_info_dev *tid = dev_to_tid(dev);
	struct touch_info_dev_operations *tid_ops = tid->tid_ops;
	bool use_color = tid->fw_name_use_color;

	/**
	 * If we request firmware fail, we can retry once.
	 */
	if (unlikely(!fw || !fw->data)) {
		int len;
		char name[64] = { 0 };

		dev_err(dev, "load firmware '%s' fail and retry\n",
				fw_context->firmware_name);
		tid->fw_name_use_color = !use_color;
		len = get_firmware_name(dev, name, ARRAY_SIZE(name));
		if (unlikely(len > ARRAY_SIZE(name) - 1)) {
			dev_err(dev, "get firmware name fail, the buf size is too small\n");
			goto out;
		}

		if (!strcmp(name, fw_context->firmware_name))
			goto out;
		dev_dbg(dev, "retry firmware name is '%s'\n", name);

		memcpy(fw_context->firmware_name, name, ARRAY_SIZE(name));
		if (request_firmware(&fw, fw_context->firmware_name, dev))
			goto out;
	}

	if (unlikely(!tid_ops || !tid_ops->firmware_upgrade))
		goto out;

	if (likely(tid_ops->get_version)) {
		ret = tid_ops->get_version(dev, NULL, &minor_old);
		if (unlikely(ret))
			dev_err(dev, "%s: get firmware version fail", __func__);
		else
			dev_dbg(dev, "before upgrade firmware, the version is: 0x%02X\n",
					minor_old);
	}

	ret = tid_ops->firmware_upgrade(dev->parent, fw, false);
	if (unlikely(ret)) {
		dev_err(dev, "upgrade firmware fail with errno: %d\n", ret);
		goto out;
	}

	if (likely(tid_ops->get_version)) {
		ret = tid_ops->get_version(dev, NULL, &minor_new);
		if (unlikely(ret)) {
			dev_err(dev, "%s: get firmware version fail", __func__);
			goto out;
		}
		if (minor_new > minor_old)
			dev_info(dev, "upgrade firmware success, the version is: 0x%02X\n",
					 minor_new);
		else
			dev_dbg(dev, "no need to upgrade firmware\n");
	}
out:
	tid->fw_name_use_color = use_color;
	release_firmware(fw);
	kfree(fw_context);
	/* matches tid_upgrade_firmware_nowait() */
	put_device(dev);
}

static size_t get_firmware_name(struct device *dev, char *buf, size_t size)
{
	const char *color = NULL;
	struct touch_info_dev *tid = dev_to_tid(dev);
	int hw_id;

	get_panel_maker(dev);
	if (tid->fw_name_use_color)
		color = get_panel_color(dev);
	hw_id = get_hardware_id(dev);

	return snprintf(buf, size, color ? "%s-%s-%s-h%d-%s.img" : "%s-%s-%s-h%d.img",
					tid->vendor, tid->product, tid->panel_maker ? : "none", hw_id,
					color);
}

/**
 * tid_upgrade_firmware_nowait: - asynchronous version of request_firmware
 * @tid:  struct touch_info_dev
 *
 * Notice: If disable lockdown info interface, the firmware name is
 * 'vendor-product-panelmaker.img'. Otherwise, the firmware name is
 * 'vendor-product-panelmaker-panelcolor.img'. if @tid->fw_name_use_color
 * is set.
 **/
int tid_upgrade_firmware_nowait(struct touch_info_dev *tid)
{
	int ret;
	struct device *dev = tid_to_dev(tid);
	struct firmware_context *fw_context;

	fw_context = kzalloc(sizeof(*fw_context), GFP_KERNEL);
	if (unlikely(!fw_context))
		return -ENOMEM;

	get_device(dev);
	fw_context->dev = dev;

	ret = get_firmware_name(dev, fw_context->firmware_name,
							ARRAY_SIZE(fw_context->firmware_name));
	if (unlikely(ret > ARRAY_SIZE(fw_context->firmware_name) - 1)) {
		dev_err(dev, "get firmware name fail, the buf size is too small\n");
		ret = -ENOMEM;
		goto err;
	}
	dev_dbg(dev, "firmware name is '%s'\n", fw_context->firmware_name);

	ret = request_firmware_nowait(THIS_MODULE, true, fw_context->firmware_name,
								  dev, GFP_KERNEL, fw_context,
								  firmware_callback);
	if (unlikely(ret))
		goto err;

	return 0;
err:
	kfree(fw_context);
	put_device(dev);
	return ret;
}
EXPORT_SYMBOL(tid_upgrade_firmware_nowait);

/**
 * tid_panel_maker: - get panel maker of touchscreen
 */
const char *tid_panel_maker(void)
{
	struct touch_info_dev *tid;

	tid = find_default_tid();
	if (unlikely(!tid))
		return NULL;

	return tid->panel_maker;
}
EXPORT_SYMBOL(tid_panel_maker);

/**
 * tid_panel_color: - get panel color of touchscreen
 */
const char *tid_panel_color(void)
{
	struct touch_info_dev *tid;

	tid = find_default_tid();
	if (unlikely(!tid))
		return NULL;

	return tid->panel_color;
}
EXPORT_SYMBOL(tid_panel_color);

static int touch_info_dev_init(void)
{
	class_register(&touchscreen_class);
	create_proc();
	pr_debug("touch info interface ready\n");

	return 0;
}
subsys_initcall(touch_info_dev_init);

MODULE_AUTHOR("smcdef <songmuchun@wingtech.com>");
MODULE_LICENSE("GPL v2");
