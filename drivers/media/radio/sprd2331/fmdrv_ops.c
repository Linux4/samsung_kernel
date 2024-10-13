/*
 * FM Radio  driver  with SPREADTRUM SC2331FM Radio chip
 *
 * Copyright (c) 2015 Spreadtrum
 * Author: Songhe Wei <songhe.wei@spreadtrum.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <linux/platform_device.h>
#include  <linux/module.h>
#include <linux/ioctl.h>
#include <linux/miscdevice.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include "fmdrv.h"
#include "fmdrv_main.h"
#include "fmdrv_ops.h"

#ifdef CONFIG_OF
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif


long fm_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	long ret = 0;
	u32 iarg = 0;
	/*fm_pr("FM_IOCTL cmd: 0x%x.", cmd);*/

	switch (cmd) {
	case FM_IOCTL_POWERUP:
		fm_pr("----power up----");
		fm_powerup(argp);
		ret = fm_tune(argp);
		break;

	case FM_IOCTL_POWERDOWN:
		fm_pr("----power down----");
		ret = fm_powerdown();
		break;

	case FM_IOCTL_TUNE:
		fm_pr("----TUNE----");
		ret = fm_tune(argp);
		break;

	case FM_IOCTL_SEEK:
		fm_pr("----SEEK----");
		ret = fm_seek(argp);
		break;

	case FM_IOCTL_SETVOL:
		fm_pr("----SETVOL----");
		ret = 0;
		break;

	case FM_IOCTL_GETVOL:
		fm_pr("----GETVOL----");
		ret = 0;
		break;

	case FM_IOCTL_MUTE:
		fm_pr("----MUTE----");
		ret = fm_mute(argp);
		break;

	case FM_IOCTL_GETRSSI:
		fm_pr("----GETRSSI----");
		ret = fm_getrssi(argp);
		break;

	case FM_IOCTL_SCAN:
		fm_pr("----SCAN----");
		ret = 0;
		break;

	case FM_IOCTL_STOP_SCAN:
		fm_pr("----STOP_SCAN----");
		ret = 0;
		break;

	case FM_IOCTL_GETCHIPID:
		fm_pr("----GETCHIPID----");
		iarg = 0x2341;
		if (copy_to_user(argp, &iarg, sizeof(iarg)))
			ret = -EFAULT;
		else
			ret = 0;
		break;

	case FM_IOCTL_EM_TEST:
		fm_pr("----EM_TEST----");
		ret = 0;
		break;

	case FM_IOCTL_RW_REG:
		fm_pr("----RW_REG----");
		ret = 0;
		break;

	case FM_IOCTL_GETMONOSTERO:
		fm_pr("----GETMONOSTERO----");
		ret = 0;
		break;
	case FM_IOCTL_GETCURPAMD:
		fm_pr("----GETCURPAMD----");
		ret = 0;
		break;

	case FM_IOCTL_GETGOODBCNT:
		fm_pr("----GETGOODBCNT----");
		ret = 0;
		break;

	case FM_IOCTL_GETBADBNT:
		fm_pr("----GETBADBNT----");
		ret = 0;
		break;

	case FM_IOCTL_GETBLERRATIO:
		fm_pr("----GETBLERRATIO----");
		ret = 0;
		break;

	case FM_IOCTL_RDS_ONOFF:
		fm_pr("----RDS_ONOFF----");
		ret = fm_rds_onoff(argp);
		break;

	case FM_IOCTL_RDS_SUPPORT:
		fm_pr("----RDS_SUPPORT----");
		ret = 0;
		if (copy_from_user(&iarg, arg, sizeof(iarg))) {
			fm_pr("fm RDS support 's ret value is -eFAULT\n");
			return -EFAULT;
		}
		iarg = 1;
		if (copy_to_user(arg, &iarg, sizeof(iarg)))
			ret = -EFAULT;
		break;

	case FM_IOCTL_RDS_SIM_DATA:
		fm_pr("----SIM_DATA----");
		ret = 0;
		break;

	case FM_IOCTL_IS_FM_POWERED_UP:
		fm_pr("----IS_FM_POWERED_UP----");
		ret = 0;
		break;

	case FM_IOCTL_OVER_BT_ENABLE:
		fm_pr("----OVER_BT_ENABLE----");
		ret = 0;
		break;

	case FM_IOCTL_ANA_SWITCH:
		fm_pr("----ANA_SWITCH----");
		ret = 0;
		break;

	case FM_IOCTL_GETCAPARRAY:
		fm_pr("----GETCAPARRAY----");
		ret = 0;
		break;

	case FM_IOCTL_I2S_SETTING:
		fm_pr("----I2S_SETTING----");
		ret = 0;
		break;

	case FM_IOCTL_RDS_GROUPCNT:
		fm_pr("----RDS_GROUPCNT----");
		ret = 0;
		break;

	case FM_IOCTL_RDS_GET_LOG:
		fm_pr("----GET_LOG----");
		ret = 0;
		break;

	case FM_IOCTL_SCAN_GETRSSI:
		fm_pr("----SCAN_GETRSSI----");
		ret = 0;
		break;

	case FM_IOCTL_SETMONOSTERO:
		fm_pr("----SETMONOSTERO----");
		ret = 0;
		break;

	case FM_IOCTL_RDS_BC_RST:
		fm_pr("----RDS_BC_RST----");
		ret = 0;
		break;

	case FM_IOCTL_CQI_GET:
		fm_pr("----CQI_GET----");
		ret = 0;
		break;

	case FM_IOCTL_GET_HW_INFO:
		fm_pr("----GET_HW_INFO----");
		ret = 0;
		break;

	case FM_IOCTL_GET_I2S_INFO:
		fm_pr("----GET_I2S_INFO----");
		ret = 0;
		break;

	case FM_IOCTL_IS_DESE_CHAN:
		fm_pr("----IS_DESE_CHAN----");
		ret = 0;
		break;

	case FM_IOCTL_TOP_RDWR:
		fm_pr("----TOP_RDWR----");
		ret = 0;
		break;

	case FM_IOCTL_HOST_RDWR:
		fm_pr("----HOST_RDWR----");
		ret = 0;
		break;

	case FM_IOCTL_PRE_SEARCH:
		fm_pr("----PRE_SEARCH----");
		ret = 0;
		break;

	case FM_IOCTL_RESTORE_SEARCH:
		fm_pr("----RESTORE_SEARCH----");
		ret = 0;
		break;
/*
	case FM_IOCTL_SET_SEARCH_THRESHOLD:
		;
		break;
*/
	case FM_IOCTL_GET_AUDIO_INFO:
		fm_pr("----GET_AUDIO_INFO----");
		ret = 0;
		break;

	case FM_IOCTL_SCAN_NEW:
		fm_pr("----SCAN_NEW----");
		ret = 0;
		break;

	case FM_IOCTL_SEEK_NEW:
		fm_pr("----SEEK_NEW----");
		ret = 0;
		break;

	case FM_IOCTL_TUNE_NEW:
		fm_pr("----TUNE_NEW----");
		ret = 0;
		break;

	case FM_IOCTL_SOFT_MUTE_TUNE:
		fm_pr("----SOFT_MUTE_TUNE----");
		ret = 0;
		break;

	case FM_IOCTL_DESENSE_CHECK:
		fm_pr("----DESENSE_CHECK----");
		ret = 0;
		break;

	case FM_IOCTL_FULL_CQI_LOG:
		fm_pr("----FULL_CQI_LOG----");
		ret = 0;
		break;

	case FM_IOCTL_DUMP_REG:
		fm_pr("----DUMP_REG----");
		ret = 0;
		break;

	default:
		fm_pr("Unknown FM IOCTL!");
		fm_pr("****************: 0x%x.", cmd);
		return -EINVAL;
	}

	return ret;
}

int fm_release(struct inode *inode, struct file *filep)
{
	fm_pr("fm_misc_release.");

	fm_powerdown();

	wake_up_interruptible(&fmdev->rds_han.rx_queue);
	fmdev->rds_han.new_data_flag = 1;

	/*fm_deinit();*/

	return 0;
}

#ifdef CONFIG_COMPAT
static long fm_compat_ioctl(struct file *file,
			unsigned int cmd, unsigned long data)
{
/*

	if (_IOC_NR(cmd) <= 3 && _IOC_SIZE(cmd) == sizeof(compat_uptr_t)) {
		cmd &= ~(_IOC_SIZEMASK << _IOC_SIZESHIFT);
		cmd |= sizeof(void *) << _IOC_SIZESHIFT;
	}
*/
	fm_pr("start_fm_compat_ioctl FM_IOCTL cmd: 0x%x.", cmd);
	cmd = cmd & 0xFFF0FFFF;
	cmd = cmd | 0x00080000;
	fm_pr("fm_compat_ioctl FM_IOCTL cmd: 0x%x.", cmd);
	return fm_ioctl(file, cmd, (unsigned long)compat_ptr(data));
}
#endif

const struct file_operations fm_misc_fops = {
	.owner = THIS_MODULE,
	.open = fm_open,
	.read = fm_read_rds_data,
	.unlocked_ioctl = fm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = fm_compat_ioctl,
#endif
	.release = fm_release,
};

struct miscdevice fm_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = FM_DEV_NAME,
	.fops = &fm_misc_fops,
};

#ifdef CONFIG_OF

static const struct of_device_id  of_match_table_fm[] = {
	{ .compatible = "sprd,sprd_fm", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_match_table_fm);
#endif

static int fm_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	char *ver_str = FM_VERSION;

#ifdef CONFIG_OF
	struct device_node *np;
	np = pdev->dev.of_node;
#endif

	fm_pr("**********************************************");
	fm_pr(" marlin2 FM driver ");
	fm_pr(" Version: %s", ver_str);
	fm_pr(" Build date: %s %s", __DATE__, __TIME__);
	fm_pr("**********************************************");

	ret = misc_register(&fm_misc_device);
	if (ret < 0) {

		fm_pr("misc_register failed!");
		return ret;
	}

	fm_pr("fm_init success.\n");

	return 0;
}

static int fm_remove(struct platform_device *pdev)
{

	fm_pr("exit_fm_driver!\n");
	misc_deregister(&fm_misc_device);

	return 0;
}

#ifdef CONFIG_PM
static int fm_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int fm_resume(struct platform_device *dev)
{
	return 0;
}
#else
#define fm_suspend NULL
#define fm_resume NULL
#endif


static struct platform_driver fm_driver = {
	.driver = {
		.name = "sprd_fm",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		 .of_match_table = of_match_ptr(of_match_table_fm) ,
#endif
	},
	.probe = fm_probe,
	.remove = fm_remove,
	.suspend = fm_suspend,
	.resume = fm_resume,
};

#ifndef CONFIG_OF
struct platform_device fm_device = {
	.name = "sprd_fm",
	.id = -1,
};
#endif

int  fm_device_init_driver(void)
{
	int ret;
#ifndef CONFIG_OF
	ret = platform_device_register(&fm_device);
	if (ret) {
		fm_pr("fm: platform_device_register failed: %d\n", ret);
		return ret;
	}
#endif
	ret = platform_driver_register(&fm_driver);
	if (ret) {
#ifndef CONFIG_OF
		platform_device_unregister(&fm_device);
#endif
		fm_pr("fm: probe failed: %d\n", ret);
	}
	fm_pr("fm: probe sucess: %d\n", ret);
	return ret;

}

void fm_device_exit_driver(void)
{
	platform_driver_unregister(&fm_driver);

}
