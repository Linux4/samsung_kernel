/*
 * Marvell 88PM80x ONKEY driver
 *
 * Copyright (C) 2012 Marvell International Ltd.
 * Haojian Zhuang <haojian.zhuang@marvell.com>
 * Qiao Zhou <zhouqiao@marvell.com>
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License. See the file "COPYING" in the main directory of this
 * archive for more details.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/mfd/88pm80x.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#ifdef CONFIG_FAKE_SYSTEMOFF
#include <linux/power/fake-sysoff.h>
#define LONGPRESS_INTERVAL (3 * HZ)
static void pm8xxx_longpress_report(struct work_struct *ignored);
static DECLARE_DELAYED_WORK(presscheck_work, pm8xxx_longpress_report);
static atomic_t longpress_work_state;
static struct wakeup_source suspend_longkey_lock;
static struct pm80x_onkey_info *pm8xxx_info;
#endif

#define PM800_LONG_ONKEY_EN		(1 << 0)
#define PM800_LONG_KEY_DELAY		(8)	/* 1 .. 16 seconds */
#define PM800_LONKEY_PRESS_TIME		((PM800_LONG_KEY_DELAY-1) << 4)
#define PM800_LONKEY_PRESS_TIME_MASK	(0xF0)
#define PM800_SW_PDOWN			(1 << 5)

#define PM800_LONGKEY_RESET		(1 << 3)

struct pm80x_onkey_info {
	struct input_dev *idev;
	struct pm80x_chip *pm80x;
	struct regmap *map;
	int irq;
};

#ifdef CONFIG_FAKE_SYSTEMOFF
void pm8xxx_longpress_report(struct work_struct *ignored)
{
	atomic_set(&longpress_work_state, 0);
	input_report_key(pm8xxx_info->idev, KEY_POWER, 1);
	input_report_key(pm8xxx_info->idev, KEY_POWER, 0);
	input_sync(pm8xxx_info->idev);
}
#endif

#if defined(CONFIG_KERNEL_DEBUG_SEC) && (defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7))
#define MY_NAME "88pm80x_onkey"
extern void gpio_keys_setstate(int keycode, bool bState);
extern bool gpio_keys_getstate(int keycode);
extern void gpio_keys_start_upload_modtimer(void);
extern int jack_is_detected;

#if defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
#define key_dbg(fmt, arg...)
#else
#define key_dbg(fmt, arg...) printk(KERN_NOTICE "%s: " fmt, MY_NAME, ## arg)
#endif
#endif

extern struct class *sec_class;
static int is_power_key_pressed;
/* 88PM80x gives us an interrupt when ONKEY is held */
static irqreturn_t pm80x_onkey_handler(int irq, void *data)
{
	struct pm80x_onkey_info *info = data;
	int ret = 0;
	unsigned int val;

	ret = regmap_read(info->map, PM800_STATUS_1, &val);
	if (ret < 0) {
		dev_err(info->idev->dev.parent, "failed to read status: %d\n",
			ret);
		return IRQ_NONE;
	}
	val &= PM800_ONKEY_STS1;
	/*
	 * HW workaround: There is bug of LONG_ONKEY_EVENT will be detected wrongly,
	 * then it will trigger  power down/power up cycle unexpectedly.
	 * Sw will reset the LONG_ONKEY timer to avoid this kind of issue.
	 */
	regmap_update_bits(info->map, PM800_WAKEUP1,
			PM800_LONGKEY_RESET, PM800_LONGKEY_RESET);
#ifdef CONFIG_FAKE_SYSTEMOFF
	if (fake_sysoff_block_onkey())
		goto out;
	if (fake_sysoff_status_query()) {
		if (val) { /*down key*/
			if (!atomic_cmpxchg(&longpress_work_state, 0, 1)) {
				schedule_delayed_work(&presscheck_work,
						LONGPRESS_INTERVAL);
				/* Think about following case: onkey down/up->
				 * kernel will awake 5s ->after 4s->
				 * user long down on key -> after 1s, suspend
				 * -> have no chance for 3s timeout*/
				__pm_wakeup_event(&suspend_longkey_lock,
				jiffies_to_msecs(LONGPRESS_INTERVAL + HZ));
			}
		} else { /*up key*/
			if (atomic_cmpxchg(&longpress_work_state, 1, 0)) {
				/* short press */
				cancel_delayed_work_sync(&presscheck_work);
			}
		}
	} else {
#endif
#if defined(CONFIG_KERNEL_DEBUG_SEC) && (defined(CONFIG_MACH_LT02) || defined(CONFIG_MACH_COCOA7))
		key_dbg("%s state = %d\n", __func__, val);
		gpio_keys_setstate(KEY_POWER, val ? true : false);
		if (val && gpio_keys_getstate(KEY_VOLUMEUP) && jack_is_detected)
			gpio_keys_start_upload_modtimer();
#endif
		input_report_key(info->idev, KEY_POWER, val);
		input_sync(info->idev);

#ifdef CONFIG_FAKE_SYSTEMOFF
	}
out:
#endif
	is_power_key_pressed = val;		// AT+KEYSHORT cmd
	return IRQ_HANDLED;
}

static SIMPLE_DEV_PM_OPS(pm80x_onkey_pm_ops, pm80x_dev_suspend,
			 pm80x_dev_resume);
// AT + KEYSHORT cmd
static ssize_t keys_read(struct device *dev, struct device_attribute *attr, char *buf)
{
	int count;

//	printk("is_key_pressed = %d . is_power_key_pressed = %d\n",is_key_pressed,is_power_key_pressed);
	if(is_power_key_pressed !=0 ) 
	{	
		count = sprintf(buf,"%s\n","PRESS");
	}
	else
	{
		count = sprintf(buf,"%s\n","RELEASE");
	}

       return count;
}
static DEVICE_ATTR(sec_power_key_pressed, S_IRUGO, keys_read, NULL);
static int __devinit pm80x_onkey_probe(struct platform_device *pdev)
{

	struct pm80x_chip *chip = dev_get_drvdata(pdev->dev.parent);
	struct pm80x_onkey_info *info;
	int irq, err;

	info = kzalloc(sizeof(struct pm80x_onkey_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->pm80x = chip;

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(&pdev->dev, "No IRQ resource!\n");
		err = -EINVAL;
		goto out;
	}

	info->irq = irq + chip->irq_base;
	info->map = info->pm80x->regmap;
	if (!info->map) {
		dev_err(&pdev->dev, "no regmap!\n");
		err = -EINVAL;
		goto out;
	}

	info->idev = input_allocate_device();
	if (!info->idev) {
		dev_err(&pdev->dev, "Failed to allocate input dev\n");
		err = -ENOMEM;
		goto out;
	}

	info->idev->name = "88pm80x_on";
	info->idev->phys = "88pm80x_on/input0";
	info->idev->id.bustype = BUS_I2C;
	info->idev->dev.parent = &pdev->dev;
	info->idev->evbit[0] = BIT_MASK(EV_KEY);
	__set_bit(KEY_POWER, info->idev->keybit);

	err = pm80x_request_irq(info->pm80x, info->irq, pm80x_onkey_handler,
				IRQF_ONESHOT, "onkey", info);
	if (err < 0) {
		dev_err(&pdev->dev, "Failed to request IRQ: #%d: %d\n",
			info->irq, err);
		goto out_reg;
	}

	err = input_register_device(info->idev);
	if (err) {
		dev_err(&pdev->dev, "Can't register input device: %d\n", err);
		goto out_irq;
	}

	platform_set_drvdata(pdev, info);

	/* Enable long onkey detection */
	regmap_update_bits(info->map, PM800_RTC_MISC4, PM800_LONG_ONKEY_EN,
			   PM800_LONG_ONKEY_EN);
	/* Set 8-second interval */
	regmap_update_bits(info->map, PM800_RTC_MISC3,
			   PM800_LONKEY_PRESS_TIME_MASK,
			   PM800_LONKEY_PRESS_TIME);

	device_init_wakeup(&pdev->dev, 1);
#ifdef CONFIG_FAKE_SYSTEMOFF
	pm8xxx_info = info;
	atomic_set(&longpress_work_state, 0);
	wakeup_source_init(&suspend_longkey_lock, "longkey_suspend");
#endif
#if 1
	{struct device *dev_t;
	
	dev_t = device_create(sec_class, NULL, 0, "%s", "sec_power_key");
	
	if(device_create_file(dev_t, &dev_attr_sec_power_key_pressed) < 0)
		 printk("Failed to create device file(%s)!\n", dev_attr_sec_power_key_pressed.attr.name);
	}
#endif
	return 0;

out_irq:
	pm80x_free_irq(info->pm80x, info->irq, info);
out_reg:
	input_free_device(info->idev);
out:
	kfree(info);
	return err;
}

static int __devexit pm80x_onkey_remove(struct platform_device *pdev)
{
	struct pm80x_onkey_info *info = platform_get_drvdata(pdev);

	device_init_wakeup(&pdev->dev, 0);
	pm80x_free_irq(info->pm80x, info->irq, info);
	input_unregister_device(info->idev);
	kfree(info);
#ifdef CONFIG_FAKE_SYSTEMOFF
	wakeup_source_trash(&suspend_longkey_lock);
#endif
	return 0;
}

static struct platform_driver pm80x_onkey_driver = {
	.driver = {
		   .name = "88pm80x-onkey",
		   .owner = THIS_MODULE,
		   .pm = &pm80x_onkey_pm_ops,
		   },
	.probe = pm80x_onkey_probe,
	.remove = __devexit_p(pm80x_onkey_remove),
};

module_platform_driver(pm80x_onkey_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Marvell 88PM80x ONKEY driver");
MODULE_AUTHOR("Qiao Zhou <zhouqiao@marvell.com>");
MODULE_ALIAS("platform:88pm80x-onkey");
