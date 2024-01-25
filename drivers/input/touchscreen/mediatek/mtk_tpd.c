/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/
#include "tpd.h"
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/fb.h>
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
#include <mtk_6306_gpio.h>
#endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

//+bug782967,wanwen2.wt,add,20220827,tp modify resume and suspend
#if defined (CONFIG_WT_PROJECT_S96516SA1) || defined (CONFIG_WT_PROJECT_S96616AA1)
#include "panel_notifier.h"
#include "./focaltech_touch/focaltech_core.h"
#include "./ILI9882Q/ilitek_v3.h"
extern void tpd_resume(struct device *h);
extern void tpd_suspend(struct device *h);
extern void tpd_focal_resume(struct device *h);
extern void tpd_focal_suspend(struct device *h);
extern void tpd_ili_resume(void);
#endif
//-bug782967,wanwen2.wt,add,20220827,tp modify resume and suspend

#ifdef CONFIG_PANEL_NOTIFIER_W2 //CONFIG_WT_PROJECT_S96901
#include "panel_notifier.h"
#include <ILI7807S/ilitek_v3.h>
#include <FT8722/focaltech_core.h>
#endif

#if defined(CONFIG_MTK_S3320) || defined(CONFIG_MTK_S3320_50) \
	|| defined(CONFIG_MTK_S3320_47) || defined(CONFIG_MTK_MIT200) \
	|| defined(CONFIG_TOUCHSCREEN_SYNAPTICS_S3528) \
	|| defined(CONFIG_MTK_S7020) \
	|| defined(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_3320_50)
#include <linux/input/mt.h>
#endif /* CONFIG_MTK_S3320 */
/* for magnify velocity******************************************** */
#define TOUCH_IOC_MAGIC 'A'

#define TPD_GET_VELOCITY_CUSTOM_X _IO(TOUCH_IOC_MAGIC, 0)
#define TPD_GET_VELOCITY_CUSTOM_Y _IO(TOUCH_IOC_MAGIC, 1)
#define TPD_GET_FILTER_PARA _IOWR(TOUCH_IOC_MAGIC, 2, struct tpd_filter_t)
#ifdef CONFIG_COMPAT
#define COMPAT_TPD_GET_FILTER_PARA _IOWR(TOUCH_IOC_MAGIC, \
						2, struct tpd_filter_t)
#endif
struct tpd_filter_t tpd_filter;
struct tpd_dts_info tpd_dts_data;
struct pinctrl *pinctrl1;
struct pinctrl_state *pins_default;
struct pinctrl_state *eint_as_int, *eint_output0,
		*eint_output1, *rst_output0, *rst_output1;
const struct of_device_id touch_of_match[] = {
	{ .compatible = "mediatek,touch", },
	{},
};

enum lcm_name{
	TRULY_FT8722_MODULE = 0,
	TXD_FT8722_MODULE = 1,
	TM_ILI7807S_MODULE = 2,
	DSBJ_HX83112F_MODULE = 3,
	DJN_HX83112F_MODULE = 4,
	TXD_ILI9882Q_MODULE = 5,
	DJN_ICNL9911C_MODULE = 6,
	SKY_FT8006S_MODULE = 7,
	TXD_ILI9882Q10_MODULE = 8,
	CW_ILI7807S_MODULE = 9,
	TM_FT_MODULE = 10,
	TXD_ICNL9911C_MODULE = 11,
	LEAD_ICNL9911C_MODULE = 12,
	TRULY_ILI9882Q_MODULE = 13,
	TXD_GC7202H_MODULE = 14,
	LC_GC7202H_MODULE = 15,
	TM_ICNL9911C_MODULE = 16,
	BOE_HIMAX_MODULE = 17,
	TRULY_NT36528_MODULE =18,
	XINXIAN_TD4160_MODULE = 19,
	MDT_FT8057S_MODULE = 20,
	BOE_TD4160_MODULE = 21,
	TXD_NT36528_MODULE =22,
    HKC_GC7272_MODULE = 23,
	TXD_GC7272_MODULE = 24,
/* +S96818AA1-1936,daijun1.wt,add,2023/08/17,incl9916_hkc tp bringup */
	XINXIAN_ICNL9916_MODULE = 25,
	DSBJ_ICNL9916_MODULE = 26,
/* -S96818AA1-1936,daijun1.wt,add,2023/08/17,incl9916_hkc tp bringup */
};

enum touch_name{
	TRULY_FT8722 = 0,
};

int g_lcm_name;
//extern int g_touch_name;
extern char *saved_command_line;
char tp_name[20] = { 0 };

#ifdef CONFIG_PANEL_NOTIFIER_W2 //CONFIG_WT_PROJECT_S96901
extern int panel_register_client(struct notifier_block *nb);
extern void tpd_resume(struct device *h);
extern void tpd_suspend(struct device *h);
extern void tpd_fts_resume(struct device *dev);
extern void tpd_fts_suspend(struct device *dev);
#endif

void tpd_get_dts_info(void)
{
	struct device_node *node1 = NULL;
	int key_dim_local[16] = {0}, i = 0;

	node1 = of_find_matching_node(node1, touch_of_match);
	if (node1) {
		of_property_read_u32(node1,
			"tpd-max-touch-num", &tpd_dts_data.touch_max_num);
		of_property_read_u32(node1,
			"use-tpd-button", &tpd_dts_data.use_tpd_button);
		pr_debug("[tpd]use-tpd-button = %d\n",
			tpd_dts_data.use_tpd_button);
		if (of_property_read_u32_array(node1, "tpd-resolution",
			tpd_dts_data.tpd_resolution,
			ARRAY_SIZE(tpd_dts_data.tpd_resolution))) {
			pr_debug("[tpd] resulution is %d %d",
				tpd_dts_data.tpd_resolution[0],
				tpd_dts_data.tpd_resolution[1]);
		}
		if (tpd_dts_data.use_tpd_button) {
			of_property_read_u32(node1,
				"tpd-key-num", &tpd_dts_data.tpd_key_num);
			if (of_property_read_u32_array(node1, "tpd-key-local",
				tpd_dts_data.tpd_key_local,
				ARRAY_SIZE(tpd_dts_data.tpd_key_local)))
				pr_debug("tpd-key-local: %d %d %d %d",
					tpd_dts_data.tpd_key_local[0],
					tpd_dts_data.tpd_key_local[1],
					tpd_dts_data.tpd_key_local[2],
					tpd_dts_data.tpd_key_local[3]);
			if (of_property_read_u32_array(node1,
				"tpd-key-dim-local",
				key_dim_local, ARRAY_SIZE(key_dim_local))) {
				memcpy(tpd_dts_data.tpd_key_dim_local,
					key_dim_local, sizeof(key_dim_local));
				for (i = 0; i < 4; i++) {
					pr_debug("[tpd]key[%d].key_x = %d\n", i,
						tpd_dts_data
							.tpd_key_dim_local[i]
							.key_x);
					pr_debug("[tpd]key[%d].key_y = %d\n", i,
						tpd_dts_data
							.tpd_key_dim_local[i]
							.key_y);
					pr_debug("[tpd]key[%d].key_W = %d\n", i,
						tpd_dts_data
							.tpd_key_dim_local[i]
							.key_width);
					pr_debug("[tpd]key[%d].key_H = %d\n", i,
						tpd_dts_data
							.tpd_key_dim_local[i]
							.key_height);
				}
			}
		}
		of_property_read_u32(node1, "tpd-filter-enable",
			&tpd_dts_data.touch_filter.enable);
		if (tpd_dts_data.touch_filter.enable) {
			of_property_read_u32(node1,
				"tpd-filter-pixel-density",
				&tpd_dts_data.touch_filter.pixel_density);
			if (of_property_read_u32_array(node1,
				"tpd-filter-custom-prameters",
				(u32 *)tpd_dts_data.touch_filter.W_W,
				ARRAY_SIZE(tpd_dts_data.touch_filter.W_W)))
				pr_debug("get tpd-filter-custom-parameters");
			if (of_property_read_u32_array(node1,
				"tpd-filter-custom-speed",
				tpd_dts_data.touch_filter.VECLOCITY_THRESHOLD,
				ARRAY_SIZE(tpd_dts_data
						.touch_filter
						.VECLOCITY_THRESHOLD)))
				pr_debug("get tpd-filter-custom-speed");
		}
		memcpy(&tpd_filter,
			&tpd_dts_data.touch_filter, sizeof(tpd_filter));
		pr_debug("[tpd]tpd-filter-enable = %d, pixel_density = %d\n",
				tpd_filter.enable, tpd_filter.pixel_density);
		tpd_dts_data.tpd_use_ext_gpio =
			of_property_read_bool(node1, "tpd-use-ext-gpio");
		of_property_read_u32(node1,
			"tpd-rst-ext-gpio-num",
			&tpd_dts_data.rst_ext_gpio_num);

	} else {
		TPD_DMESG("can't find touch compatible custom node\n");
	}
}

static DEFINE_MUTEX(tpd_set_gpio_mutex);
void tpd_gpio_as_int(int pin)
{
	mutex_lock(&tpd_set_gpio_mutex);
	TPD_DEBUG("[tpd] %s\n", __func__);
//+S96818AA1-1936,daijun1.wt,modify,2023/05/19,n28-tp prevent crashes caused by DTS
	if (pin == 1)
		pinctrl_select_state(pinctrl1, eint_as_int);
//-S96818AA1-1936,daijun1.wt,modify,2023/05/19,n28-tp prevent crashes caused by DTS
	mutex_unlock(&tpd_set_gpio_mutex);
}

void tpd_gpio_output(int pin, int level)
{
	mutex_lock(&tpd_set_gpio_mutex);
	TPD_DEBUG("%s pin = %d, level = %d\n", __func__, pin, level);
	if (pin == 1) {
//+S96818AA1-1936,daijun1.wt,modify,2023/05/19,n28-tp prevent crashes caused by DTS
		if (level)
			pinctrl_select_state(pinctrl1, eint_output1);
		else
			pinctrl_select_state(pinctrl1, eint_output0);
//-S96818AA1-1936,daijun1.wt,modify,2023/05/19,n28-tp prevent crashes caused by DTS
	} else {
		if (tpd_dts_data.tpd_use_ext_gpio) {
#ifdef CONFIG_MTK_MT6306_GPIO_SUPPORT
			mt6306_set_gpio_dir(
				tpd_dts_data.rst_ext_gpio_num, 1);
			mt6306_set_gpio_out(
				tpd_dts_data.rst_ext_gpio_num, level);
#endif
		} else {
			if (level)
				pinctrl_select_state(pinctrl1, rst_output1);
			else
				pinctrl_select_state(pinctrl1, rst_output0);
		}
	}
	mutex_unlock(&tpd_set_gpio_mutex);
}

void tddi_lcm_tp_reset_output(int level)
{
	mutex_lock(&tpd_set_gpio_mutex);
	TPD_DMESG("%s tp reset, level = %d\n", __func__,level);
    if (level)
		pinctrl_select_state(pinctrl1, rst_output1);
	else
		pinctrl_select_state(pinctrl1, rst_output0);
	mutex_unlock(&tpd_set_gpio_mutex);
}
EXPORT_SYMBOL(tddi_lcm_tp_reset_output);

int tpd_get_gpio_info(struct platform_device *pdev)
{
	int ret;

	TPD_DEBUG("[tpd %d] mt_tpd_pinctrl+++++++++++++++++\n", pdev->id);
	pinctrl1 = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR(pinctrl1)) {
		ret = PTR_ERR(pinctrl1);
		dev_info(&pdev->dev, "fwq Cannot find pinctrl1!\n");
		return ret;
	}
	pins_default = pinctrl_lookup_state(pinctrl1, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		TPD_DMESG("Cannot find pinctrl default %d!\n", ret);
	}
	eint_as_int = pinctrl_lookup_state(pinctrl1, "state_eint_as_int");
	if (IS_ERR(eint_as_int)) {
		ret = PTR_ERR(eint_as_int);
		TPD_DMESG("Cannot find pinctrl state_eint_as_int!\n");
		return ret;
	}
	eint_output0 = pinctrl_lookup_state(pinctrl1, "state_eint_output0");
	if (IS_ERR(eint_output0)) {
		ret = PTR_ERR(eint_output0);
		TPD_DMESG("Cannot find pinctrl state_eint_output0!\n");
		return ret;
	}
	eint_output1 = pinctrl_lookup_state(pinctrl1, "state_eint_output1");
	if (IS_ERR(eint_output1)) {
		ret = PTR_ERR(eint_output1);
		TPD_DMESG("Cannot find pinctrl state_eint_output1!\n");
		return ret;
	}
	if (tpd_dts_data.tpd_use_ext_gpio == false) {
		rst_output0 =
			pinctrl_lookup_state(pinctrl1, "state_rst_output0");
		if (IS_ERR(rst_output0)) {
			ret = PTR_ERR(rst_output0);
			TPD_DMESG("Cannot find pinctrl state_rst_output0!\n");
			return ret;
		}
		rst_output1 =
			pinctrl_lookup_state(pinctrl1, "state_rst_output1");
		if (IS_ERR(rst_output1)) {
			ret = PTR_ERR(rst_output1);
			TPD_DMESG("Cannot find pinctrl state_rst_output1!\n");
			return ret;
		}
	}
	TPD_DEBUG("[tpd%d] mt_tpd_pinctrl----------\n", pdev->id);
	return 0;
}

static int tpd_misc_open(struct inode *inode, struct file *file)
{
	return nonseekable_open(inode, file);
}

static int tpd_misc_release(struct inode *inode, struct file *file)
{
	return 0;
}

#ifdef CONFIG_COMPAT
static long tpd_compat_ioctl(
			struct file *file, unsigned int cmd,
			unsigned long arg)
{
	long ret;
	void __user *arg32 = compat_ptr(arg);

	if (!file->f_op || !file->f_op->unlocked_ioctl)
		return -ENOTTY;
	switch (cmd) {
	case COMPAT_TPD_GET_FILTER_PARA:
		if (arg32 == NULL) {
			pr_info("invalid argument.");
			return -EINVAL;
		}
		ret = file->f_op->unlocked_ioctl(file, TPD_GET_FILTER_PARA,
					   (unsigned long)arg32);
		if (ret) {
			pr_info("TPD_GET_FILTER_PARA unlocked_ioctl failed.");
			return ret;
		}
		break;
	default:
		pr_info("tpd: unknown IOCTL: 0x%08x\n", cmd);
		ret = -ENOIOCTLCMD;
		break;
	}
	return ret;
}
#endif
static long tpd_unlocked_ioctl(struct file *file,
			unsigned int cmd, unsigned long arg)
{
	/* char strbuf[256]; */
	void __user *data;

	long err = 0;

	if (_IOC_DIR(cmd) & _IOC_READ)
		err = !access_ok(VERIFY_WRITE,
			(void __user *)arg, _IOC_SIZE(cmd));
	else if (_IOC_DIR(cmd) & _IOC_WRITE)
		err = !access_ok(VERIFY_READ,
			(void __user *)arg, _IOC_SIZE(cmd));
	if (err) {
		pr_info("tpd: access error: %08X, (%2d, %2d)\n",
			cmd, _IOC_DIR(cmd), _IOC_SIZE(cmd));
		return -EFAULT;
	}

	switch (cmd) {
	case TPD_GET_VELOCITY_CUSTOM_X:
		data = (void __user *)arg;

		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data,
			&tpd_v_magnify_x, sizeof(tpd_v_magnify_x))) {
			err = -EFAULT;
			break;
		}

		break;

	case TPD_GET_VELOCITY_CUSTOM_Y:
		data = (void __user *)arg;

		if (data == NULL) {
			err = -EINVAL;
			break;
		}

		if (copy_to_user(data,
			&tpd_v_magnify_y, sizeof(tpd_v_magnify_y))) {
			err = -EFAULT;
			break;
		}

		break;
	case TPD_GET_FILTER_PARA:
			data = (void __user *) arg;

			if (data == NULL) {
				err = -EINVAL;
				TPD_DMESG("GET_FILTER_PARA: data is null\n");
				break;
			}

			if (copy_to_user(data, &tpd_filter,
					sizeof(struct tpd_filter_t))) {
				TPD_DMESG("GET_FILTER_PARA: copy data error\n");
				err = -EFAULT;
				break;
			}
			break;
	default:
		pr_info("tpd: unknown IOCTL: 0x%08x\n", cmd);
		err = -ENOIOCTLCMD;
		break;

	}

	return err;
}
static struct work_struct touch_resume_work;
static struct workqueue_struct *touch_resume_workqueue;
static const struct file_operations tpd_fops = {
/* .owner = THIS_MODULE, */
	.open = tpd_misc_open,
	.release = tpd_misc_release,
	.unlocked_ioctl = tpd_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = tpd_compat_ioctl,
#endif
};

/*---------------------------------------------------------------------------*/
static struct miscdevice tpd_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "touch",
	.fops = &tpd_fops,
};

/* ********************************************** */
/* #endif */


/* function definitions */
static int __init tpd_device_init(void);
static void __exit tpd_device_exit(void);
static int tpd_probe(struct platform_device *pdev);
static int tpd_remove(struct platform_device *pdev);
static struct work_struct tpd_init_work;
static struct workqueue_struct *tpd_init_workqueue;
static int tpd_suspend_flag;
int tpd_register_flag;
/* global variable definitions */
struct tpd_device *tpd;
static struct tpd_driver_t tpd_driver_list[TP_DRV_MAX_COUNT];	/* = {0}; */

struct platform_device tpd_device = {
	.name		= TPD_DEVICE,
	.id			= -1,
};
const struct dev_pm_ops tpd_pm_ops = {
	.suspend = NULL,
	.resume = NULL,
};
static struct platform_driver tpd_driver = {
	.remove = tpd_remove,
	.shutdown = NULL,
	.probe = tpd_probe,
	.driver = {
			.name = TPD_DEVICE,
			.pm = &tpd_pm_ops,
			.owner = THIS_MODULE,
			.of_match_table = touch_of_match,
	},
};
static struct tpd_driver_t *g_tpd_drv;
/* hh: use fb_notifier */
static struct notifier_block tpd_fb_notifier;
/* use fb_notifier */
static void touch_resume_workqueue_callback(struct work_struct *work)
{
	TPD_DEBUG("GTP %s\n", __func__);
	g_tpd_drv->resume(NULL);
	tpd_suspend_flag = 0;
}
#ifdef CONFIG_PANEL_NOTIFIER_W2 //CONFIG_WT_PROJECT_S96901
static int tpd_ilitek_notifier_callback(
			struct notifier_block *self,
			unsigned long event, void *data)
{
//struct ilitek_ts_data *ts = ilits;

	printk("[ILITEK]tpd_ilitek_notifier_callback in\n ");
	if (event == PANEL_UNPREPARE) {
		TPD_DMESG("event=%lu\n", event);
	//	tpd_suspend(&ts->spi->dev);
	} else if (event == PANEL_PREPARE) {
		TPD_DMESG("event=%lu\n", event);
	//	tpd_resume(&ts->spi->dev);
	}
	return 0;
}
static int tpd_fts_notifier_callback(
	struct notifier_block *self,
	unsigned long event,
	void *data)
{
	struct fts_ts_data *ts = fts_data;


	printk("[FTS]tpd_focaltech_notifier_callback in\n ");
	if (event == PANEL_UNPREPARE) {
		TPD_DMESG("event=%lu\n", event);
		tpd_fts_suspend(ts->dev);
	} else if (event == PANEL_PREPARE) {
		TPD_DMESG("event=%lu\n", event);
		tpd_fts_resume(ts->dev);
	}
	return 0;
}
#elif defined (CONFIG_WT_PROJECT_S96516SA1) || defined (CONFIG_WT_PROJECT_S96616AA1)
static int tpd_ilitek_notifier_callback(
	struct notifier_block *self,
	unsigned long event,
	void *data)
{
	struct ilitek_ts_data *ts = ilits;

	printk("[ILITEK]tpd_ilitek_notifier_callback in\n ");
	if (event == PANEL_UNPREPARE) {
		TPD_DMESG("event=%lu\n", event);
		tpd_suspend(&ts->spi->dev);
	} else if (event == PANEL_PREPARE) {
			TPD_DMESG("event=%lu\n", event);
			tpd_ili_resume();
			//tpd_resume(&ts->spi->dev);
	}
	return 0;
}

static int tpd_focal_notifier_callback(
	struct notifier_block *self,
	unsigned long event,
	void *data)
{
	struct fts_ts_data *ts = fts_data;

	printk("[FTS]tpd_focal_notifier_callback in\n ");
	if (event == PANEL_UNPREPARE) {
		TPD_DMESG("event=%lu\n", event);
		tpd_focal_suspend(&ts->spi->dev);
	} else if (event == PANEL_PREPARE) {
		TPD_DMESG("event=%lu\n", event);
		tpd_focal_resume(&ts->spi->dev);
	}
	return 0;
}
#endif

static int tpd_fb_notifier_callback(
			struct notifier_block *self,
			unsigned long event, void *data)
{
	struct fb_event *evdata = NULL;
	int blank;
	int err = 0;

	TPD_DEBUG("%s\n", __func__);

	evdata = data;
	/* If we aren't interested in this event, skip it immediately ... */
	if ((event == FB_EVENT_BLANK) || (event == FB_EARLY_EVENT_BLANK)) {

	    blank = *(int *)evdata->data;
	    TPD_DMESG("fb_notify(blank=%d, event=%d)\n", blank, event);

        if(event == FB_EVENT_BLANK) {
		    if(blank == FB_BLANK_UNBLANK) {
			    TPD_DMESG("LCD ON Notify\n");
			    if (g_tpd_drv && tpd_suspend_flag) {
			    err = queue_work(touch_resume_workqueue,
							     &touch_resume_work);
			        if (!err) {
				        TPD_DMESG("start resume_workqueue failed\n");
				        return err;
			        }
                }
		    }
	    } else {
            if(blank == FB_BLANK_POWERDOWN) {
			    TPD_DMESG("LCD OFF Notify, tpd_suspend_flag = %d\n", tpd_suspend_flag);
			    if (g_tpd_drv && !tpd_suspend_flag) {
				    err = cancel_work_sync(&touch_resume_work);
				    if (!err)
					    TPD_DMESG("cancel resume_workqueue failed\n");
				    g_tpd_drv->suspend(NULL);
			    }
			    tpd_suspend_flag = 1;
		    }
        }
    }
	return 0;
}
/* Add driver: if find TPD_TYPE_CAPACITIVE driver successfully, loading it */
int tpd_driver_add(struct tpd_driver_t *tpd_drv)
{
	int i;

	if (g_tpd_drv != NULL) {
		TPD_DMESG("touch driver exist\n");
		return -1;
	}
	/* check parameter */
	if (tpd_drv == NULL)
		return -1;
	tpd_drv->tpd_have_button = tpd_dts_data.use_tpd_button;
	/* R-touch */
	if (strcmp(tpd_drv->tpd_device_name, "generic") == 0) {
		tpd_driver_list[0].tpd_device_name = tpd_drv->tpd_device_name;
		tpd_driver_list[0].tpd_local_init = tpd_drv->tpd_local_init;
		tpd_driver_list[0].suspend = tpd_drv->suspend;
		tpd_driver_list[0].resume = tpd_drv->resume;
		tpd_driver_list[0].tpd_have_button = tpd_drv->tpd_have_button;
		return 0;
	}
	for (i = 1; i < TP_DRV_MAX_COUNT; i++) {
		/* add tpd driver into list */
		if (tpd_driver_list[i].tpd_device_name == NULL) {
			tpd_driver_list[i].tpd_device_name =
				tpd_drv->tpd_device_name;
			tpd_driver_list[i].tpd_local_init =
				tpd_drv->tpd_local_init;
			tpd_driver_list[i].suspend = tpd_drv->suspend;
			tpd_driver_list[i].resume = tpd_drv->resume;
			tpd_driver_list[i].tpd_have_button =
				tpd_drv->tpd_have_button;
			tpd_driver_list[i].attrs = tpd_drv->attrs;
			break;
		}
		if (strcmp(tpd_driver_list[i].tpd_device_name,
			tpd_drv->tpd_device_name) == 0)
			return 1;	/* driver exist */
	}

	return 0;
}

int tpd_driver_remove(struct tpd_driver_t *tpd_drv)
{
	int i = 0;
	/* check parameter */
	if (tpd_drv == NULL)
		return -1;
	for (i = 0; i < TP_DRV_MAX_COUNT; i++) {
		/* find it */
		if (strcmp(tpd_driver_list[i].tpd_device_name,
				tpd_drv->tpd_device_name) == 0) {
			memset(&tpd_driver_list[i], 0,
				sizeof(struct tpd_driver_t));
			break;
		}
	}
	return 0;
}

static void tpd_create_attributes(struct device *dev, struct tpd_attrs *attrs)
{
	int num = attrs->num;

	for (; num > 0;) {
		if (device_create_file(dev, attrs->attr[--num]))
			pr_info("mtk_tpd: tpd create attributes file failed\n");
	}
}

static int tp_match_lcm_name(void)
{
	TPD_DMESG("saved_command_line is %s \t%s, %d\n", saved_command_line,__func__, __LINE__);

	printk("[mtk-tpd] tp_match_lcm_name  saved_command_line is %s \n", saved_command_line);
	if (strstr(saved_command_line,"ft8722_fhdp_wt_dsi_vdo_cphy_90hz_txd_sharp")) {
		g_lcm_name = TXD_FT8722_MODULE;
		strcpy((char *)tp_name, "fts_ts");
	} else if (strstr(saved_command_line,"ft8722_fhdp_wt_dsi_vdo_cphy_90hz_txd")) {
		g_lcm_name = TXD_FT8722_MODULE;
		strcpy((char *)tp_name, "fts_ts");
	} else if (strstr(saved_command_line,"ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_tianma")){
		g_lcm_name = TM_ILI7807S_MODULE;
		strcpy((char *)tp_name, "ILITEK_TDDI_7807S");
	} else if (strstr(saved_command_line,"ft8722_fhdp_wt_dsi_vdo_cphy_90hz_truly")){
		g_lcm_name = TRULY_FT8722_MODULE;
		strcpy((char *)tp_name, "fts_ts");
	} else if (strstr(saved_command_line,"hx83112f_fhdp_wt_dsi_vdo_cphy_90hz_dsbj")) {
		g_lcm_name = DSBJ_HX83112F_MODULE;
		strcpy((char *)tp_name, "himax_generic");
	} else if (strstr(saved_command_line,"hx83112f_fhdp_wt_dsi_vdo_cphy_90hz_djn")) {
		g_lcm_name = DJN_HX83112F_MODULE;
		strcpy((char *)tp_name, "himax_generic");
	}  else if (strstr(saved_command_line,"ili7807s_fhdp_wt_dsi_vdo_cphy_90hz_chuangwei")){
		g_lcm_name = CW_ILI7807S_MODULE;
		strcpy((char *)tp_name, "ILITEK_TDDI_7807S");
	} else if (strstr(saved_command_line,"ili9882q_dsi_vdo_hdp_ctc_txd")) {
		g_lcm_name = TXD_ILI9882Q_MODULE;
		strcpy((char *)tp_name, "mediatek,ili9882q");
	} else if (strstr(saved_command_line,"incl9911c_dsi_vdo_hdp_huajiacai_dijing")) {
		g_lcm_name = DJN_ICNL9911C_MODULE;
		strcpy((char *)tp_name, "chipone-tddi");
	} else if (strstr(saved_command_line,"ft8006s_dsi_vdo_hdp_boe_skyworth")) {
		g_lcm_name = SKY_FT8006S_MODULE;
		strcpy((char *)tp_name, "focaltech,fts");
	} else if (strstr(saved_command_line,"ili9882q10_dsi_vdo_hdp_ctc_txd")) {
		g_lcm_name = TXD_ILI9882Q10_MODULE;
		strcpy((char *)tp_name, "mediatek,ili9882q");
	} else if (strstr(saved_command_line,"ft8006s_dsi_vdo_hdp_skyworth_shenchao")){
		g_lcm_name = TM_FT_MODULE;   //ili9881h_hd_plus_vdo_txd
		strcpy((char *)tp_name, "focaltech,fts");
	} else if (strstr(saved_command_line,"icnl9911c_dsi_vdo_hdp_txd_inx") || strstr(saved_command_line,"n28_icnl9911c_dsi_vdo_hdp_txd_inx")){
		g_lcm_name = TXD_ICNL9911C_MODULE;    //icnl9911c_hd_plus_vdo_txd
		strcpy((char *)tp_name, "chipone-tddi");
	} else if (strstr(saved_command_line,"icnl9911c_dsi_vdo_hdp_lead_hsd")){
		g_lcm_name = LEAD_ICNL9911C_MODULE;    //icnl9911c_hd_plus_vdo_txd
		strcpy((char *)tp_name, "chipone-tddi");
	} else if (strstr(saved_command_line,"ili9882q_dsi_vdo_hdp_truly_truly")) {
		g_lcm_name = TRULY_ILI9882Q_MODULE;
		strcpy((char *)tp_name, "mediatek,ili9882q");
	} else if (strstr(saved_command_line,"gc7202_dsi_vdo_hdp_txd_hkc")) {
		g_lcm_name = TXD_GC7202H_MODULE;
		strcpy((char *)tp_name, "gcore,touchscreen");
	} else if (strstr(saved_command_line,"gc7202_dsi_vdo_hdp_ice_panda")) {
		g_lcm_name = LC_GC7202H_MODULE;
		strcpy((char *)tp_name, "gcore,touchscreen");
	} else if (strstr(saved_command_line,"icnl9911c_dsi_vdo_hdp_tianma_hkc")) {
		g_lcm_name = TM_ICNL9911C_MODULE;
		strcpy((char *)tp_name, "chipone-tddi");
	} else if (strstr(saved_command_line,"hx83108_dsi_vdo_hdp_boe_boe")) {
		g_lcm_name = BOE_HIMAX_MODULE;
		strcpy((char *)tp_name, "himax");
	} else if (strstr(saved_command_line,"n28_nt36528_dsi_vdo_hdp_truly_truly")) {
		g_lcm_name = TRULY_NT36528_MODULE;
		strcpy((char *)tp_name, "NVT-ts");
//+S96818AA1-1936,daijun1.wt,modify,2023/06/26,td4160_boe tp bringup
	} else if (strstr(saved_command_line,"n28_td4160_dsi_vdo_hdp_xinxian_inx")) {
		g_lcm_name = XINXIAN_TD4160_MODULE;
		strcpy((char *)tp_name, "omnivision_tcm");
	} else if (strstr(saved_command_line,"n28_td4160_dsi_vdo_hdp_boe_boe")) {
		g_lcm_name = BOE_TD4160_MODULE;
		strcpy((char *)tp_name, "omnivision_tcm");
	} else if (strstr(saved_command_line,"n28_nt36528_dsi_vdo_hdp_txd_sharp")) {
		g_lcm_name = TXD_NT36528_MODULE;
		strcpy((char *)tp_name, "NVT-ts");
//-S96818AA1-1936,daijun1.wt,modify,2023/06/26,td4160_boe tp bringup
	} else if (strstr(saved_command_line,"n28_ft8057s_dsi_vdo_hdp_dsbj_mantix")) {
		g_lcm_name = MDT_FT8057S_MODULE;
		strcpy((char *)tp_name, "fts_ts");
	}else if (strstr(saved_command_line,"n28_gc7272_dsi_vdo_hdp_xinxian_hkc")) {
		g_lcm_name = HKC_GC7272_MODULE;
		strcpy((char *)tp_name, "gcore");
	} else if (strstr(saved_command_line,"n28_gc7272_dsi_vdo_hdp_txd_sharp")) {
		g_lcm_name = TXD_GC7272_MODULE;
		strcpy((char *)tp_name, "gcore");
/* +S96818AA1-1936,daijun1.wt,add,2023/08/17,incl9916_hkc tp bringup */
	} else if (strstr(saved_command_line,"n28_icnl9916c_dsi_vdo_hdp_xinxian_hkc")) {
		g_lcm_name = XINXIAN_ICNL9916_MODULE;
		strcpy((char *)tp_name, "chipone-tddi");
	} else if (strstr(saved_command_line,"n28_icnl9916c_dsi_vdo_hdp_dsbj_mdt")) {
		g_lcm_name = DSBJ_ICNL9916_MODULE;
		strcpy((char *)tp_name, "chipone-tddi");
/* -S96818AA1-1936,daijun1.wt,add,2023/08/17,incl9916_hkc tp bringup */
	}else {
		TPD_DMESG("lcm name not match!");
		return  -1;
	}

	printk("[mtk-tpd]   lcm_name=%d, tp name is %s\n",g_lcm_name,tp_name);

	return 0;
}

/* touch panel probe */
static int tpd_probe(struct platform_device *pdev)
{
	int touch_type = 1;	/* 0:R-touch, 1: Cap-touch */
	int i = 0;
#ifndef CONFIG_CUSTOM_LCM_X
#ifdef CONFIG_LCM_WIDTH
	unsigned long tpd_res_x = 0, tpd_res_y = 0;
	int ret = 0;
#endif
#endif
	tp_match_lcm_name();
	TPD_DMESG("enter %s, %d\n", __func__, __LINE__);
	pr_info("enter %s, %d\n", __func__, __LINE__);

	if (misc_register(&tpd_misc_device))
		pr_info("mtk_tpd: tpd_misc_device register failed\n");
	tpd_get_gpio_info(pdev);
	tpd = kmalloc(sizeof(struct tpd_device), GFP_KERNEL);
	if (tpd == NULL)
		return -ENOMEM;
	memset(tpd, 0, sizeof(struct tpd_device));

	/* allocate input device */
	tpd->dev = input_allocate_device();
	if (tpd->dev == NULL) {
		kfree(tpd);
		return -ENOMEM;
	}
	/* TPD_RES_X = simple_strtoul(LCM_WIDTH, NULL, 0); */
	/* TPD_RES_Y = simple_strtoul(LCM_HEIGHT, NULL, 0); */

	#ifdef CONFIG_MTK_LCM_PHYSICAL_ROTATION
	if (strncmp(CONFIG_MTK_LCM_PHYSICAL_ROTATION, "90", 2) == 0
		|| strncmp(CONFIG_MTK_LCM_PHYSICAL_ROTATION, "270", 3) == 0) {
#ifdef CONFIG_MTK_FB
/*Fix build errors,as some projects  cannot support these apis while bring up*/
		TPD_RES_Y = DISP_GetScreenWidth();
		TPD_RES_X = DISP_GetScreenHeight();
#endif
	} else
    #endif
	{
#ifdef CONFIG_CUSTOM_LCM_X
#ifndef CONFIG_FPGA_EARLY_PORTING
#if defined(CONFIG_MTK_FB) && defined(CONFIG_MTK_LCM)
/*Fix build errors,as some projects  cannot support these apis while bring up*/
		TPD_RES_X = DISP_GetScreenWidth();
		TPD_RES_Y = DISP_GetScreenHeight();
#else
/*for some projects, we do not use mtk framebuffer*/
	TPD_RES_X = tpd_dts_data.tpd_resolution[0];
	TPD_RES_Y = tpd_dts_data.tpd_resolution[1];
#endif
#endif
#else
#ifdef CONFIG_LCM_WIDTH
		ret = kstrtoul(CONFIG_LCM_WIDTH, 0, &tpd_res_x);
		if (ret < 0) {
			pr_info("Touch down get lcm_x failed");
			return ret;
		}
		TPD_RES_X = tpd_res_x;
		ret = kstrtoul(CONFIG_LCM_HEIGHT, 0, &tpd_res_x);
		if (ret < 0) {
			pr_info("Touch down get lcm_y failed");
			return ret;
		}
		TPD_RES_Y = tpd_res_y;
#endif
#endif
	}

//+bug 782967,wanwen.wt,add,20220728,lcd bringup
/*
	if (2560 == TPD_RES_X)
		TPD_RES_X = 2048;
	if (1600 == TPD_RES_Y)
		TPD_RES_Y = 1536;
	pr_debug("mtk_tpd: TPD_RES_X = %lu, TPD_RES_Y = %lu\n",
		TPD_RES_X, TPD_RES_Y);
*/
//+bug 782967,wanwen.wt,add,20220728,lcd bringup

	tpd_mode = TPD_MODE_NORMAL;
	tpd_mode_axis = 0;
	tpd_mode_min = TPD_RES_Y / 2;
	tpd_mode_max = TPD_RES_Y;
	tpd_mode_keypad_tolerance = TPD_RES_X * TPD_RES_X / 1600;
	/* struct input_dev dev initialization and registration */
	tpd->dev->name = TPD_DEVICE;
	set_bit(EV_ABS, tpd->dev->evbit);
	set_bit(EV_KEY, tpd->dev->evbit);
	set_bit(ABS_X, tpd->dev->absbit);
	set_bit(ABS_Y, tpd->dev->absbit);
	set_bit(ABS_PRESSURE, tpd->dev->absbit);
#if !defined(CONFIG_MTK_S3320) && !defined(CONFIG_MTK_S3320_47)\
	&& !defined(CONFIG_MTK_S3320_50) && !defined(CONFIG_MTK_MIT200) \
	&& !defined(CONFIG_TOUCHSCREEN_SYNAPTICS_S3528) \
	&& !defined(CONFIG_MTK_S7020) \
	&& !defined(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_3320_50)
	set_bit(BTN_TOUCH, tpd->dev->keybit);
#endif /* CONFIG_MTK_S3320 */
	set_bit(INPUT_PROP_DIRECT, tpd->dev->propbit);

	/* save dev for regulator_get() before tpd_local_init() */
	tpd->tpd_dev = &pdev->dev;
	for (i = 1; i < TP_DRV_MAX_COUNT; i++) {
		/* add tpd driver into list */
		if (tpd_driver_list[i].tpd_device_name != NULL) {
			printk("[mtk_tpd] tpd_driver_list[%d].tpd_device_name is %s, tp_name is %s\n", i, tpd_driver_list[i].tpd_device_name, tp_name);
			if(strcmp(tpd_driver_list[i].tpd_device_name, tp_name)) 
				continue;
			tpd_driver_list[i].tpd_local_init();
			/* msleep(1); */
			if (tpd_load_status == 1) {
				TPD_DMESG("%s, tpd_driver_name=%s\n", __func__,
					  tpd_driver_list[i].tpd_device_name);
				g_tpd_drv = &tpd_driver_list[i];
				break;
			}
		}
	}
	if (g_tpd_drv == NULL) {
		if (tpd_driver_list[0].tpd_device_name != NULL) {
			g_tpd_drv = &tpd_driver_list[0];
			/* touch_type:0: r-touch, 1: C-touch */
			touch_type = 0;
			g_tpd_drv->tpd_local_init();
			TPD_DMESG("Generic touch panel driver\n");
		} else {
			TPD_DMESG("no touch driver is loaded!!\n");
			return 0;
		}
	}
	touch_resume_workqueue = create_singlethread_workqueue("touch_resume");
	INIT_WORK(&touch_resume_work, touch_resume_workqueue_callback);
	/* use fb_notifier */

#ifdef CONFIG_PANEL_NOTIFIER_W2 //CONFIG_WT_PROJECT_S96901
	if((g_lcm_name == TM_ILI7807S_MODULE) || (g_lcm_name == CW_ILI7807S_MODULE)) {
  		tpd_fb_notifier.notifier_call = tpd_ilitek_notifier_callback;
		if (panel_register_client(&tpd_fb_notifier))
  			TPD_DMESG("[ILITEK] ilitek register fb_notifier fail!\n");
  	} else if (g_lcm_name == TXD_FT8722_MODULE){
		tpd_fb_notifier.notifier_call = tpd_fts_notifier_callback;
                if (panel_register_client(&tpd_fb_notifier))
                        TPD_DMESG("[FTS] focaltech_register fb_notifier fail!\n");
	} else {
		tpd_fb_notifier.notifier_call = tpd_fb_notifier_callback;
		if (fb_register_client(&tpd_fb_notifier))
			TPD_DMESG("register fb_notifier fail!\n");
	}
#elif defined (CONFIG_WT_PROJECT_S96516SA1) || defined (CONFIG_WT_PROJECT_S96616AA1)	
	if(g_lcm_name == TRULY_ILI9882Q_MODULE) {
		tpd_fb_notifier.notifier_call = tpd_ilitek_notifier_callback;
		if (panel_register_client(&tpd_fb_notifier))
			TPD_DMESG("[ILITEK] ilitek register fb_notifier fail!\n");
	} else if (g_lcm_name == TM_FT_MODULE){
		tpd_fb_notifier.notifier_call = tpd_focal_notifier_callback;
                if (panel_register_client(&tpd_fb_notifier))
                        TPD_DMESG("[FTS] focaltech_register fb_notifier fail!\n");
	} else {
		tpd_fb_notifier.notifier_call = tpd_fb_notifier_callback;
		if (fb_register_client(&tpd_fb_notifier))
			TPD_DMESG("register fb_notifier fail!\n");
	}
#else
	 tpd_fb_notifier.notifier_call = tpd_fb_notifier_callback;
                if (fb_register_client(&tpd_fb_notifier))
                        TPD_DMESG("register fb_notifier fail!\n");
#endif	
	/* TPD_TYPE_CAPACITIVE handle */
	if (touch_type == 1) {

		set_bit(ABS_MT_TRACKING_ID, tpd->dev->absbit);
		set_bit(ABS_MT_TOUCH_MAJOR, tpd->dev->absbit);
		set_bit(ABS_MT_TOUCH_MINOR, tpd->dev->absbit);
		set_bit(ABS_MT_POSITION_X, tpd->dev->absbit);
		set_bit(ABS_MT_POSITION_Y, tpd->dev->absbit);
		input_set_abs_params(tpd->dev,
			ABS_MT_POSITION_X, 0, TPD_RES_X, 0, 0);
		input_set_abs_params(tpd->dev,
			ABS_MT_POSITION_Y, 0, TPD_RES_Y, 0, 0);
#if defined(CONFIG_MTK_S3320) || defined(CONFIG_MTK_S3320_47) \
	|| defined(CONFIG_MTK_S3320_50) || defined(CONFIG_MTK_MIT200) \
	|| defined(CONFIG_TOUCHSCREEN_SYNAPTICS_S3528) \
	|| defined(CONFIG_MTK_S7020) \
	|| defined(CONFIG_TOUCHSCREEN_MTK_SYNAPTICS_3320_50)
		input_set_abs_params(tpd->dev,
		ABS_MT_PRESSURE, 0, 255, 0, 0);
		input_set_abs_params(tpd->dev,
			ABS_MT_WIDTH_MAJOR, 0, 15, 0, 0);
		input_set_abs_params(tpd->dev, ABS_MT_WIDTH_MINOR, 0, 15, 0, 0);
		input_mt_init_slots(tpd->dev, 10, 0);
#else
		input_set_abs_params(tpd->dev,
			ABS_MT_TOUCH_MAJOR, 0, 100, 0, 0);
		input_set_abs_params(tpd->dev,
			ABS_MT_TOUCH_MINOR, 0, 100, 0, 0);
#endif /* CONFIG_MTK_S3320 */
		TPD_DMESG("Cap touch panel driver\n");
	}
	input_set_abs_params(tpd->dev, ABS_X, 0, TPD_RES_X, 0, 0);
	input_set_abs_params(tpd->dev, ABS_Y, 0, TPD_RES_Y, 0, 0);
	input_abs_set_res(tpd->dev, ABS_X, TPD_RES_X);
	input_abs_set_res(tpd->dev, ABS_Y, TPD_RES_Y);
	input_set_abs_params(tpd->dev, ABS_PRESSURE, 0, 255, 0, 0);
	input_set_abs_params(tpd->dev, ABS_MT_TRACKING_ID, 0, 10, 0, 0);

	if (input_register_device(tpd->dev))
		TPD_DMESG("input_register_device failed.(tpd)\n");
	else
		tpd_register_flag = 1;
	if (g_tpd_drv->tpd_have_button)
		tpd_button_init();

	if (g_tpd_drv->attrs.num)
		tpd_create_attributes(&pdev->dev, &g_tpd_drv->attrs);

	return 0;
}
static int tpd_remove(struct platform_device *pdev)
{
	input_unregister_device(tpd->dev);
	return 0;
}

/* called when loaded into kernel */
static void tpd_init_work_callback(struct work_struct *work)
{
	TPD_DEBUG("MediaTek touch panel driver init\n");
	if (platform_driver_register(&tpd_driver) != 0)
		TPD_DMESG("unable to register touch panel driver.\n");
}
static int __init tpd_device_init(void)
{
	int res = 0;
	pr_info("[%s-%s-%d]\n", __FILE__, __func__, __LINE__);
	tpd_init_workqueue = create_singlethread_workqueue("mtk-tpd");
	INIT_WORK(&tpd_init_work, tpd_init_work_callback);

	res = queue_work(tpd_init_workqueue, &tpd_init_work);
	if (!res)
		pr_info("tpd : touch device init failed res:%d\n", res);
	return 0;
}
/* should never be called */
static void __exit tpd_device_exit(void)
{
	TPD_DMESG("MediaTek touch panel driver exit\n");
	/* input_unregister_device(tpd->dev); */
	platform_driver_unregister(&tpd_driver);
}

late_initcall(tpd_device_init);
module_exit(tpd_device_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek touch panel driver");
MODULE_AUTHOR("Kirby Wu<kirby.wu@mediatek.com>");
