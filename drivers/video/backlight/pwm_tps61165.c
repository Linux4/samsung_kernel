/*
 * linux/drivers/video/backlight/pwm_bl.c
 *
 * simple PWM based backlight control, board code has to setup
 * 1) pin configuration so PWM waveforms can output
 * 2) platform_data being correctly configured
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/lcd.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/pwm.h>
#include <linux/pwm_backlight.h>
#include <linux/slab.h>
#include <mach/tc35876x.h>
#ifdef CONFIG_MACH_LT02
#include <mach/mfp-pxa986-lt02.h>
#elif defined(CONFIG_MACH_COCOA7)
#include <mach/mfp-pxa986-cocoa7.h>
#else
#include <mach/mfp-mmp2.h>
#endif

struct mdnie_config mDNIe_cfg;

enum CABC {
	CABC_OFF,
	CABC_ON,
	CABC_MAX,
};

typedef struct mdnie_config {
	int scenario;
	int negative;
	int outdoor;
	int cabc;
	int mode;
	struct device	*dev;
	struct mutex	lock;
};

struct Vx5b3d_backlight_value {
	const unsigned int max;
	const unsigned int mid;
	const unsigned char low;
	const unsigned char dim;
};

static struct Vx5b3d_backlight_value backlight_table[1] = {
	{
		.max = 236,
		.mid = 140,
		.low = 10,
		.dim = 10,
	}
};

#define V5D3BX_VEESTRENGHT		0x00001f07
#define V5D3BX_VEETESTVAL		25

#define MIN_BRIGHTNESS			0
#define MAX_BRIGHTNESS_LEVEL		255
#define MID_BRIGHTNESS_LEVEL		195
#define LOW_BRIGHTNESS_LEVEL		20
#define DIM_BRIGHTNESS_LEVEL		20
#define DEFAULT_BRIGHTNESS		MID_BRIGHTNESS_LEVEL

struct pwm_bl_data {
	struct pwm_device	*pwm;
	struct device		*dev;
	struct lcd_device	*ld;
	struct Vx5b3d_backlight_value *backlight;

	unsigned int		period;
	unsigned int		lth_brightness;
	unsigned int		vee_strenght;
	u32			vee_ambient;

	int			(*notify)(struct device *,
					  int brightness);
	void			(*notify_after)(struct device *,
					int brightness);
	int			(*check_fb)(struct device *, struct fb_info *);
};

static int pwm_backlight_update_status(struct backlight_device *bl)
{
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);
	int brightness = bl->props.brightness;
	int max = bl->props.max_brightness;
	int ret = 0;
	struct Vx5b3d_backlight_value *pwm = pb->backlight;
	int vx5b3d_brightness = 0;
	u32 vee_strenght = 0;

	if (bl->props.power != FB_BLANK_UNBLANK)
		brightness = 0;

	if (bl->props.fb_blank != FB_BLANK_UNBLANK)
		brightness = 0;

	if (pb->notify)
		brightness = pb->notify(pb->dev, brightness);
/*
	register 0x160
	register 0x164
			  value of 0x164
	---> duty ration = -------------
			  value of 0x160
	
*/
#if defined(CONFIG_MACH_LT02)
	if (system_rev >= LT02_BRINGUP_02) {
#elif defined(CONFIG_MACH_COCOA7)
	if (system_rev >= COCOA7_R0_0) {
#endif
		/* brightness tuning*/
		if (brightness >= MID_BRIGHTNESS_LEVEL) {
			vx5b3d_brightness  = (brightness - MID_BRIGHTNESS_LEVEL) *
			(pwm->max - pwm->mid) / (MAX_BRIGHTNESS_LEVEL-MID_BRIGHTNESS_LEVEL) + pwm->mid;
		} else if (brightness >= LOW_BRIGHTNESS_LEVEL) {
			vx5b3d_brightness  = (brightness - LOW_BRIGHTNESS_LEVEL) *
			(pwm->mid - pwm->low) / (MID_BRIGHTNESS_LEVEL-LOW_BRIGHTNESS_LEVEL) + pwm->low;
		} else if (brightness >= DIM_BRIGHTNESS_LEVEL) {
			vx5b3d_brightness  = (brightness - DIM_BRIGHTNESS_LEVEL) *
			(pwm->low - pwm->dim) / (LOW_BRIGHTNESS_LEVEL-DIM_BRIGHTNESS_LEVEL) + pwm->dim;
		} else if (brightness > 0)
			vx5b3d_brightness  = pwm->dim;
		else
			vx5b3d_brightness = 0;

		printk("brightness = [%d]: vx5b3d_brightness = [%d]\n",brightness,vx5b3d_brightness);

		/* brightness setting from platform is from 0 to 255
		 * But in this driver, brightness is only supported from 0 to 24 */
		 
		/*FOR VX5B3D PWM CONTROL*/
		
		vee_strenght = V5D3BX_VEESTRENGHT | (V5D3BX_VEETESTVAL << 27);
		ret |= tc35876x_write32(0x164,vx5b3d_brightness);
		/*
		ret |= tc35876x_write32(0x400,vee_strenght);
		printk("vee_strenght value [0x%x]\n",vee_strenght);
		*/
		/* ret |= vx5d3b_i2c_release(); */

		if (ret < 0)
		dev_info(pb->dev, "ql_i2c_write fail [%d] ! \n",ret);

	} 
	
	/* rev == BringUp rev0.1 */
	else {	/*For tc35876x*/

		if (brightness == 0) {
			pwm_config(pb->pwm, 0, pb->period);
			pwm_disable(pb->pwm);
		} else {
			brightness = pb->lth_brightness +
				(brightness * (pb->period - pb->lth_brightness) / max);
			dev_info(pb->dev, "pwm backlight update =[%d]\n",brightness);
			pwm_config(pb->pwm, brightness, pb->period);
			pwm_enable(pb->pwm);
		}
	}

	if (pb->notify_after)
		pb->notify_after(pb->dev, brightness);

	return 0;
}

static int pwm_backlight_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static int pwm_backlight_check_fb(struct backlight_device *bl,
				  struct fb_info *info)
{
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	return !pb->check_fb || pb->check_fb(pb->dev, info);
}

static const struct backlight_ops pwm_backlight_ops = {
	.update_status	= pwm_backlight_update_status,
	.get_brightness	= pwm_backlight_get_brightness,
	.check_fb	= pwm_backlight_check_fb,
};

static ssize_t vee_ambient_store(struct device *dev, struct
device_attribute *attr, const char *buf, size_t size)
{
	struct pwm_bl_data *pb = dev_get_drvdata(dev);
	u32 value;
	int rc;
	
	/*Protection code for  power on /off test */
	if(pb->dev <= 0)
		return size;
	
	rc = strict_strtoul(buf, (unsigned int) 0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else{
		dev_info(dev, "vee_ambient_store - %d, %d\n", pb->vee_ambient, value);
		if (pb->vee_ambient!= value) {
			pb->vee_ambient = value;
			/*tc35876x_write32(0x164,value);*/
			/*vx5d3b_i2c_release();*/
		}
		return size;
	}
}
static DEVICE_ATTR(vee_ambient, 0664,NULL, vee_ambient_store);

static ssize_t vee_strenght_store(struct device *dev, struct
device_attribute *attr, const char *buf, size_t size)
{
	struct pwm_bl_data *pb = dev_get_drvdata(dev);
	int value;
	u32 vee_value = 0x00001f07;	
	int rc;
	
	/*Protection code for  power on /off test */
	if(pb->dev <= 0)
		return size;
	
	rc = strict_strtoul(buf, (unsigned int) 0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else{
		dev_info(dev, "vee_strenght_store[0x400] - %d, %d\n", pb->vee_strenght, value);
		if (pb->vee_strenght!= value) {
			pb->vee_strenght = value;			
			vee_value = vee_value | (value << 27);
			printk("vee_strenght value [0x%x]\n",vee_value);
			tc35876x_write32(0x400,vee_value);
			vx5d3b_i2c_release();			
		}
		return size;
	}
}
static DEVICE_ATTR(vee_strenght, 0664,NULL, vee_strenght_store);

static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[20];
	sprintf(temp, "BOE_HV070WSA\n");
	strcat(buf, temp);
	return strlen(buf);
}

static DEVICE_ATTR(lcd_type, 0664, lcd_type_show, NULL);


static ssize_t cabc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct mdnie_config *mdnie = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", mdnie->cabc);
}
 
static ssize_t cabc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct mdnie_config *mdnie = dev_get_drvdata(dev);
	unsigned int value;
	int ret;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if (value >= CABC_MAX)
		value = CABC_OFF;

	value = (value) ? CABC_ON : CABC_OFF;

	mutex_lock(&mdnie->lock);
	mdnie->cabc = value;
	mutex_unlock(&mdnie->lock);

	return count;
}

struct class *mdnie_class;
static struct device_attribute mdnie_attributes[] = {
	__ATTR(cabc, 0664, cabc_show, cabc_store),
	__ATTR_NULL,
};

static int pwm_backlight_probe(struct platform_device *pdev)
{
	struct backlight_properties props;
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl;
	struct pwm_bl_data *pb;
	int ret;
	const char *name = dev_name(&pdev->dev);

	if (!data) {
		dev_err(&pdev->dev, "failed to find platform data\n");
		return -EINVAL;
	}

	if (data->init) {
		ret = data->init(&pdev->dev);
		if (ret < 0)
			return ret;
	}

	pb = devm_kzalloc(&pdev->dev, sizeof(*pb), GFP_KERNEL);
	if (!pb) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	pb->period = data->pwm_period_ns;
	pb->notify = data->notify;
	pb->notify_after = data->notify_after;
	pb->check_fb = data->check_fb;
	pb->lth_brightness = data->lth_brightness *
		(data->pwm_period_ns / data->max_brightness);
	pb->dev = &pdev->dev;
	if (data->name)
               name = data->name;
	pb->backlight = &backlight_table[0];

	pb->pwm = pwm_request(data->pwm_id, "backlight");
	if (IS_ERR(pb->pwm)) {
		dev_err(&pdev->dev, "unable to request PWM for backlight\n");
		ret = PTR_ERR(pb->pwm);
		goto err_alloc;
	} else
		dev_dbg(&pdev->dev, "got pwm for backlight\n");

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = data->max_brightness;
	bl = backlight_device_register("panel", &pdev->dev, pb,
				       &pwm_backlight_ops, &props);
	if (IS_ERR(bl)) {
		dev_err(&pdev->dev, "failed to register backlight\n");
		ret = PTR_ERR(bl);
		goto err_bl;
	}

	bl->props.brightness = data->dft_brightness;
	/*backlight_update_status(bl);*/

	mdnie_class = class_create(THIS_MODULE, "mdnie");

	if (IS_ERR_OR_NULL(mdnie_class)) {
		pr_err("failed to create mdnie class\n");
	}

	mdnie_class->dev_attrs = mdnie_attributes;

	mDNIe_cfg.dev = device_create(mdnie_class, NULL, 0, &mDNIe_cfg, "mdnie");

	mutex_init(&mDNIe_cfg.lock);


	platform_set_drvdata(pdev, bl);

#ifdef CONFIG_LCD_CLASS_DEVICE
	pb->ld = lcd_device_register("panel", &pdev->dev,
					pb, NULL);

    	if (IS_ERR(pb->ld)) 
    	{
    		printk("Failed to create lcd / panel!\n");
    		goto err_bl;
    	}

    	ret = device_create_file(&pb->ld->dev, &dev_attr_lcd_type);

	if (ret < 0)
		dev_err(&pb->ld->dev, "failed to add sysfs entries, %d\n",
					__LINE__);
	
    	ret = device_create_file(&pb->ld->dev, &dev_attr_vee_strenght);

	if (ret < 0)
		dev_err(&pb->ld->dev, "failed to add sysfs entries, %d\n",
					__LINE__);

	ret = device_create_file(&pb->ld->dev, &dev_attr_vee_ambient);

	if (ret < 0)
		dev_err(&pb->ld->dev, "failed to add sysfs entries, %d\n",
					__LINE__);
	
#endif	
	return 0;

err_bl:
	pwm_free(pb->pwm);
err_alloc:
	if (data->exit)
		data->exit(&pdev->dev);
	return ret;
}

static int pwm_backlight_remove(struct platform_device *pdev)
{
	struct platform_pwm_backlight_data *data = pdev->dev.platform_data;
	struct backlight_device *bl = platform_get_drvdata(pdev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	backlight_device_unregister(bl);
	pwm_config(pb->pwm, 0, pb->period);
	pwm_disable(pb->pwm);
	pwm_free(pb->pwm);
	if (data->exit)
		data->exit(&pdev->dev);
	return 0;
}

#ifdef CONFIG_PM
static int pwm_backlight_suspend(struct device *dev)
{
	struct backlight_device *bl = dev_get_drvdata(dev);
	struct pwm_bl_data *pb = dev_get_drvdata(&bl->dev);

	if (pb->notify)
		pb->notify(pb->dev, 0);
	pwm_config(pb->pwm, 0, pb->period);
	pwm_disable(pb->pwm);
	if (pb->notify_after)
		pb->notify_after(pb->dev, 0);
	return 0;
}

static int pwm_backlight_resume(struct device *dev)
{
	struct backlight_device *bl = dev_get_drvdata(dev);

	backlight_update_status(bl);
	return 0;
}

static SIMPLE_DEV_PM_OPS(pwm_backlight_pm_ops, pwm_backlight_suspend,
			 pwm_backlight_resume);

#endif

static struct platform_driver pwm_backlight_driver = {
	.driver		= {
		.name	= "pwm-tps61165",
		.owner	= THIS_MODULE,
#ifdef CONFIG_PM
		.pm	= &pwm_backlight_pm_ops,
#endif
	},
	.probe		= pwm_backlight_probe,
	.remove		= pwm_backlight_remove,
};

module_platform_driver(pwm_backlight_driver);

MODULE_DESCRIPTION("PWM based Backlight Driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:pwm-backlight");

