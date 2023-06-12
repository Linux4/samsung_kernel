#include <linux/miscdevice.h>
#include <linux/sysfs.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/ioctl.h>
#include <linux/err.h>
#include <linux/errno.h>
#include  <linux/module.h>
#include <linux/platform_device.h>

#include "trout_fm_ctrl.h"
#include "trout_rf_common.h"
#include "trout_interface.h"

#define TROUT_FM_VERSION	"v0.9"



struct trout_interface *p_trout_interface;

static int g_volume = 0;

int trout_fm_set_volume(u8 iarg)
{
	TROUT_PRINT("FM set volume : %i.", iarg);
        
       g_volume = (int)iarg;
	if(0 == iarg)
	{
	     trout_fm_mute(); 
	}
	else
	{
            trout_fm_unmute();
	}
	
	return 0;
}

int trout_fm_get_volume(void)
{
	TROUT_PRINT("FM get volume.");
	return g_volume;
}



int trout_fm_open(struct inode *inode, struct file *filep)
{
	int ret = -EINVAL;
	int status;

	TROUT_PRINT("start open shark fm module...");
/*
	if (get_suspend_state() != PM_SUSPEND_ON)
	{
		TROUT_PRINT("The system is suspending!");
		return -2;
	}
*/
/*
	ret = nonseekable_open(inode, filep);
	if (ret < 0) {
		TROUT_PRINT("open misc device failed.");
		return ret;
	}
*/

	ret = trout_fm_init();
	if (ret < 0) {
		TROUT_PRINT("trout_fm_init failed!");
		return ret;
	}

	ret = trout_fm_get_status(&status);
	if (ret < 0) {
		TROUT_PRINT("trout_read_fm_en failed!");
		return ret;
	}
	if (status) {
		TROUT_PRINT("trout fm have been opened.");
	} else {
		ret = trout_fm_en();
		if (ret < 0) {
			TROUT_PRINT("trout_fm_en failed!");
			return ret;
		}
	}

	TROUT_PRINT("Open shark fm module success.");

	return 0;
}

int trout_fm_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	int ret = 0;
	int iarg = 0;
	int buf[4] = { 0 };

	TROUT_PRINT("FM IOCTL: 0x%x.", cmd);

	switch (cmd) {
	case Trout_FM_IOCTL_ENABLE:
		if (copy_from_user(&iarg, argp, sizeof(iarg)) || iarg > 1)
			ret = -EFAULT;

		if (iarg == 1)
		
			ret = trout_fm_en();	
		else
		    
			ret = trout_fm_dis();

		break;

	case Trout_FM_IOCTL_GET_ENABLE:
		ret = trout_fm_get_status(&iarg);
		if (copy_to_user(argp, &iarg, sizeof(iarg)))
			ret = -EFAULT;

		break;

	case Trout_FM_IOCTL_SET_TUNE:
		if (copy_from_user(&iarg, argp, sizeof(iarg)))
			ret = -EFAULT;

		ret = trout_fm_set_tune(iarg);

#ifdef TROUT_WIFI_POWER_SLEEP_ENABLE
		wifimac_sleep();
#endif
		break;

	case Trout_FM_IOCTL_GET_FREQ:
		ret = trout_fm_get_frequency((u16 *)&iarg);
		if (copy_to_user(argp, &iarg, sizeof(iarg)))
			ret = -EFAULT;

		break;

	case Trout_FM_IOCTL_SEARCH:
		if (copy_from_user(buf, argp, sizeof(buf)))
			ret = -EFAULT;

		ret = trout_fm_seek(buf[0],	/* start frequency */
				    buf[1],	/* seek direction */
				    buf[2],	/* time out */
				    (u16 *)&buf[3]);	/* frequency found */

		if (copy_to_user(argp, buf, sizeof(buf)))
			ret = -EFAULT;

		break;

	case Trout_FM_IOCTL_STOP_SEARCH:
		ret = trout_fm_stop_seek();
		break;

	case Trout_FM_IOCTL_MUTE:
		break;

	case Trout_FM_IOCTL_SET_VOLUME:
		if (copy_from_user(&iarg, argp, sizeof(iarg)))
			ret = -EFAULT;

		ret = trout_fm_set_volume((u8) iarg);
		break;

	case Trout_FM_IOCTL_GET_VOLUME:
		iarg = trout_fm_get_volume();
		if (copy_to_user(argp, &iarg, sizeof(iarg)))
			ret = -EFAULT;

		break;

	case Trout_FM_IOCTL_CONFIG:
		if (copy_from_user(&iarg, argp, sizeof(iarg)))
			ret = -EFAULT;

		TROUT_PRINT("\n\nFM FM_IOCTL_CONFIG set %d\n\n", iarg);
		/*
		if (iarg == 0)
			trout_fm_pcm_pin_cfg();
		else if (iarg == 1)
			trout_fm_iis_pin_cfg();
		*/

		break;

	default:
		TROUT_PRINT("Unknown FM IOCTL!");
		return -EINVAL;
	}

	return ret;
}

int trout_fm_release(struct inode *inode, struct file *filep)
{
	TROUT_PRINT("trout_fm_misc_release");

	trout_fm_deinit();

	return 0;
}

const struct file_operations trout_fm_misc_fops = {
	.owner = THIS_MODULE,
	.open = trout_fm_open,
	.unlocked_ioctl = trout_fm_ioctl,
	.release = trout_fm_release,
};

struct miscdevice trout_fm_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = TROUT_FM_DEV_NAME,
	.fops = &trout_fm_misc_fops,
};


static int trout_fm_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	char *ver_str = TROUT_FM_VERSION;

	p_trout_interface = NULL;
	TROUT_PRINT("**********************************************");
	TROUT_PRINT(" Trout FM driver ");
	TROUT_PRINT(" Version: %s", ver_str);
	TROUT_PRINT(" Build date: %s %s", __DATE__, __TIME__);
	TROUT_PRINT("**********************************************");

#ifdef CONFIG_INTERFACE_SHARED
	trout_shared_init(&p_trout_interface);
#endif

#ifdef CONFIG_INTERFACE_ONCHIP
	trout_onchip_init(&p_trout_interface);
#endif

	if (p_trout_interface == NULL) {
		TROUT_PRINT("none interface used!");
		return ret;
	}

	TROUT_PRINT("use %s interface.", p_trout_interface->name);

	ret = p_trout_interface->init();
	if (ret < 0) {
		TROUT_PRINT("interface init failed!");
		return ret;
	}

	ret = misc_register(&trout_fm_misc_device);
	if (ret < 0) {
		TROUT_PRINT("misc_register failed!");
		return ret;
	}

	TROUT_PRINT("trout_fm_init success.\n");

	return 0;
}

static int trout_fm_remove(struct platform_device *pdev)
{	

	TROUT_PRINT("exit_fm_driver!\n");
	misc_deregister(&trout_fm_misc_device);

	if (p_trout_interface) {
		p_trout_interface->exit();
		p_trout_interface = NULL;
	}

        return 0;
}

#ifdef CONFIG_PM
static int trout_fm_suspend(struct platform_device *dev, pm_message_t state)
{
    trout_fm_enter_sleep();
	
    return 0;
}

static int trout_fm_resume(struct platform_device *dev)
{
    trout_fm_exit_sleep();
	
    return 0;
}
#else
#define trout_fmt_suspend NULL
#define trout_fm_resume NULL
#endif


static struct platform_driver trout_fm_driver = {
	.driver = {
		.name = "trout_fm",
		.owner = THIS_MODULE,
	},
	.probe = trout_fm_probe,
	.remove = trout_fm_remove,
	.suspend = trout_fm_suspend,
    .resume = trout_fm_resume,
};

int __init init_fm_driver(void)
{
    int ret;

	ret = platform_driver_register(&trout_fm_driver);
	if (ret)
		TROUT_PRINT("trout_fm: probe failed: %d\n", ret);

	return ret;
}

void __exit exit_fm_driver(void)
{
    platform_driver_unregister(&trout_fm_driver);
}

module_init(init_fm_driver);
module_exit(exit_fm_driver);

MODULE_DESCRIPTION("TROUT FM radio driver");
MODULE_AUTHOR("Spreadtrum Inc.");
MODULE_LICENSE("GPL");
MODULE_VERSION(TROUT_FM_VERSION);
