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
#include <asm/io.h>
#include "sr2351_fm_ctrl.h"

#ifdef CONFIG_OF
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#define SR2351_FM_VERSION	"v0.9"
#define  Google_FM_APP         1
static u32 g_volume = 0;
extern struct shark_fm_info_t shark_fm_info;
int sr2351_fm_set_volume(u32 iarg)
{
	SR2351_PRINT("FM set volume : %i.", iarg);

    g_volume = iarg;
	if(0 == iarg)
	{
	     sr2351_fm_mute();
	}
	else
	{
            sr2351_fm_unmute();
	}

	return 0;
}

u32 sr2351_fm_get_volume(void)
{
	SR2351_PRINT("FM get volume.");
	return g_volume;
}



int sr2351_fm_open(struct inode *inode, struct file *filep)
{
	int ret = -EINVAL;
	int status;

	SR2351_PRINT("start open shark fm module...");
/*
	if (get_suspend_state() != PM_SUSPEND_ON)
	{
		SR2351_PRINT("The system is suspending!");
		return -2;
	}
*/
/*
	ret = nonseekable_open(inode, filep);
	if (ret < 0) {
		SR2351_PRINT("open misc device failed.");
		return ret;
	}
*/

	ret = sr2351_fm_init();
	if (ret < 0) {
		SR2351_PRINT("sr2351_fm_init failed!");
		return ret;
	}

	ret = sr2351_fm_get_status(&status);
	if (ret < 0) {
		SR2351_PRINT("sr2351_read_fm_en failed!");
		return ret;
	}
	if (status) {
		SR2351_PRINT("sr2351 fm have been opened.");
	} else {
		ret = sr2351_fm_en();
		if (ret < 0) {
			SR2351_PRINT("sr2351_fm_en failed!");
			return ret;
		}
	}

	SR2351_PRINT("Open shark fm module success.");

	return 0;
}

/*
(shark_fm_info.rssi >> 16) & 0x1
=1: invalid freq
=0: valid freq
*/

static int fm_tune(void * p)
{
	struct fm_tune_parm parm;
	if (copy_from_user(&parm, p, sizeof(parm)))
		return -EFAULT;
	sr2351_fm_mute();
	sr2351_fm_set_tune(parm.freq);
	sr2351_fm_unmute();
	return 0;

}

static int fm_seek(void * p)
{
	struct fm_seek_parm parm;
	if (copy_from_user(&parm, p, sizeof(parm)))
		return -EFAULT;
	sr2351_fm_mute();
	sr2351_fm_seek(parm.freq, parm.seekdir, 3000, &parm.freq );
	if (copy_to_user(p, &parm, sizeof(parm)))
		return -EFAULT;
	return 0;

}

static int fm_mute(u32 iarg)
{
	if (iarg == 1) {
		sr2351_fm_mute();
		SR2351_PRINT("fm_mute: mute: %d", iarg);
		return 0;
	} else if (iarg == 0) {
		sr2351_fm_unmute();
		SR2351_PRINT("fm_unmute: mute: %d", iarg);
		return 0;
	} else {
		SR2351_PRINT("fm_mute error param, mute: %d", iarg);
		return -EFAULT;
	}
}

static int fm_softmute_tune(void * p)
{
	struct fm_softmute_tune_t parm;
	if (copy_from_user(&parm, p, sizeof(parm)))
		return -EFAULT;
	sr2351_fm_mute();
	sr2351_fm_set_tune(parm.freq);
	parm.rssi = shark_fm_info.inpwr_sts;
	parm.valid = !((shark_fm_info.rssi >> 16) & 0x1 );
	if (copy_to_user(p, &parm, sizeof(parm)))
		return -EFAULT;
	sr2351_fm_unmute();
	return 0;


}

long sr2351_fm_ioctl(struct file *filep, unsigned int cmd, unsigned long arg)
{
	void __user *argp = (void __user *)arg;
	long ret = 0;
	u32 iarg = 0;
	u32 buf[4] = { 0 };

	SR2351_PRINT("FM IOCTL: 0x%x.", cmd);

	switch (cmd) {
#if Google_FM_APP
	case FM_IOCTL_POWERUP:
		sr2351_fm_dis();
		sr2351_fm_en();
		udelay(5);
		sr2351_fm_init();
		udelay(10);
		ret = fm_tune(argp);
		break;

	case FM_IOCTL_POWERDOWN:
		sr2351_fm_dis();
		udelay(10);
		ret = sr2351_fm_deinit();
		break;

	case FM_IOCTL_TUNE:
		ret = fm_tune(argp);
		break;

	case FM_IOCTL_SEEK:
		ret = fm_seek(argp);
		break;

	case FM_IOCTL_SETVOL:
		ret = 0;
		break;

	case FM_IOCTL_GETVOL:
		ret = 0;
		break;

	case FM_IOCTL_MUTE:
		if (copy_from_user(&iarg, argp, sizeof(iarg)))
			ret = -EFAULT;
		ret = fm_mute(iarg);
		break;

	case FM_IOCTL_GETRSSI:
		ret = sr2351_fm_get_rssi(&iarg);
		if (copy_to_user(argp, &iarg, sizeof(iarg)))
			ret = -EFAULT;
		break;

	case FM_IOCTL_SCAN:
		ret = 0;
		break;

	case FM_IOCTL_STOP_SCAN:
		ret = 0;
		break;

	case FM_IOCTL_GETCHIPID:
		iarg=0x6628;
		if (copy_to_user(argp, &iarg, sizeof(iarg)))
			ret=-EFAULT;
		else ret=0;
		break;

	case FM_IOCTL_EM_TEST:
		ret = 0;
		break;

	case FM_IOCTL_RW_REG:
		ret = 0;
		break;

	case FM_IOCTL_GETMONOSTERO:
		ret = 0;
		break;
	case FM_IOCTL_GETCURPAMD:
		ret = 0;
		break;

	case FM_IOCTL_GETGOODBCNT:
		ret = 0;
		break;

	case FM_IOCTL_GETBADBNT:
		ret = 0;
		break;

	case FM_IOCTL_GETBLERRATIO:
		ret = 0;
		break;

	case FM_IOCTL_RDS_ONOFF:
		ret = 0;
		break;

	case FM_IOCTL_RDS_SUPPORT:
		ret = 0;
		break;

	case FM_IOCTL_RDS_SIM_DATA:
		ret = 0;
		break;

	case FM_IOCTL_IS_FM_POWERED_UP:
		ret = 0;
		break;

	case FM_IOCTL_OVER_BT_ENABLE:
		ret = 0;
		break;

	case FM_IOCTL_ANA_SWITCH:
		ret = 0;
		break;

	case FM_IOCTL_GETCAPARRAY:
		ret = 0;
		break;

	case FM_IOCTL_I2S_SETTING:
		ret = 0;
		break;

	case FM_IOCTL_RDS_GROUPCNT:
		ret = 0;
		break;

	case FM_IOCTL_RDS_GET_LOG:
		ret = 0;
		break;

	case FM_IOCTL_SCAN_GETRSSI:
		ret = 0;
		break;

	case FM_IOCTL_SETMONOSTERO:
		ret = 0;
		break;

	case FM_IOCTL_RDS_BC_RST:
		ret = 0;
		break;

	case FM_IOCTL_CQI_GET:
		ret = 0;
		break;

	case FM_IOCTL_GET_HW_INFO:
		ret = 0;
		break;

	case FM_IOCTL_GET_I2S_INFO:
		ret = 0;
		break;

	case FM_IOCTL_IS_DESE_CHAN:
		ret = 0;
		break;

	case FM_IOCTL_TOP_RDWR:
		ret = 0;
		break;

	case FM_IOCTL_HOST_RDWR:
		ret = 0;
		break;

	case FM_IOCTL_PRE_SEARCH:
		ret = 0;
		break;

	case FM_IOCTL_RESTORE_SEARCH:
		ret = 0;
		break;

	//case FM_IOCTL_SET_SEARCH_THRESHOLD:
	//	;
	//	break;

	case FM_IOCTL_GET_AUDIO_INFO:
		;
		break;

	case FM_IOCTL_SCAN_NEW:
		;
		break;

	case FM_IOCTL_SEEK_NEW:
		;
		break;

	case FM_IOCTL_TUNE_NEW:
		;
		break;

	case FM_IOCTL_SOFT_MUTE_TUNE:
		ret = fm_softmute_tune(argp);
		break;

	case FM_IOCTL_DESENSE_CHECK:
		;
		break;

	case FM_IOCTL_FULL_CQI_LOG:
		;
		break;

	case FM_IOCTL_DUMP_REG:
		;
		break;

	case FM_IOCTL_CHECK_STATUS:
		ret = sr2351_fm_check_status(argp);
		break;

#else

	case FM_IOCTL_ENABLE:
		if (copy_from_user(&iarg, argp, sizeof(iarg)) || iarg > 1)
			ret = -EFAULT;

		if (iarg == 1)
			ret = sr2351_fm_en();
		else
			ret = sr2351_fm_dis();

		break;

	case FM_IOCTL_GET_ENABLE:
		ret = sr2351_fm_get_status(&iarg);
		if (copy_to_user(argp, &iarg, sizeof(iarg)))
			ret = -EFAULT;

		break;

	case FM_IOCTL_SET_TUNE:
		if (copy_from_user(&iarg, argp, sizeof(iarg)))
			return -EFAULT;
		sr2351_fm_mute();
		ret = sr2351_fm_set_tune(iarg);
		sr2351_fm_unmute();
		break;

	case FM_IOCTL_GET_FREQ:
		ret = sr2351_fm_get_frequency(&iarg);
		if (copy_to_user(argp, &iarg, sizeof(iarg)))
			ret = -EFAULT;
		break;

	case FM_IOCTL_SEARCH:
		if (copy_from_user(buf, argp, sizeof(buf)))
			ret = -EFAULT;
    sr2351_fm_mute();
		ret = sr2351_fm_seek(buf[0],	/* start frequency */
				    buf[1],	/* seek direction */
				    buf[2],	/* time out */
				    &buf[3]);	/* frequency found */

		if (copy_to_user(argp, buf, sizeof(buf)))
			ret = -EFAULT;

		break;

	case FM_IOCTL_STOP_SEARCH:
		ret = sr2351_fm_stop_seek();
		break;

	case FM_IOCTL_MUTE:
		break;

	case FM_IOCTL_SET_VOLUME:
		if (copy_from_user(&iarg, argp, sizeof(iarg)))
			ret = -EFAULT;

		ret = sr2351_fm_set_volume(iarg);
		break;

	case FM_IOCTL_GET_VOLUME:
		iarg = sr2351_fm_get_volume();
		if (copy_to_user(argp, &iarg, sizeof(iarg)))
			ret = -EFAULT;

		break;

	case FM_IOCTL_CONFIG:
		if (copy_from_user(&iarg, argp, sizeof(iarg)))
			ret = -EFAULT;

		SR2351_PRINT("\n\nFM FM_IOCTL_CONFIG set %d\n\n", iarg);

		break;

	case FM_IOCTL_GET_RSSI:
		ret = sr2351_fm_get_rssi(&iarg);
		if(copy_to_user(argp,&iarg,sizeof(iarg)))
		ret = -EFAULT;
		break;

#endif

	default:
		SR2351_PRINT("Unknown FM IOCTL!");
		//return -EINVAL;
		ret=0;
	}

	return ret;
}

int sr2351_fm_release(struct inode *inode, struct file *filep)
{
	SR2351_PRINT("sr2351_fm_misc_release");
	sr2351_fm_dis();
	udelay(5);
	sr2351_fm_deinit();

	return 0;
}

const struct file_operations sr2351_fm_misc_fops = {
	.owner = THIS_MODULE,
	.open = sr2351_fm_open,
	.unlocked_ioctl = sr2351_fm_ioctl,
	.release = sr2351_fm_release,
};

struct miscdevice sr2351_fm_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = SR2351_FM_DEV_NAME,
	.fops = &sr2351_fm_misc_fops,
};

#ifdef CONFIG_OF
static struct sr2351_fm_addr
{
   unsigned long fm_base;
   unsigned long apb_base;
   unsigned long pmu_base;
   unsigned long aonckg_base;
   unsigned long pin_base;
}sr2351_fm_base;

unsigned long rf2351_fm_get_fm_base(void)
{
   return sr2351_fm_base.fm_base;
}
unsigned long rf2351_fm_get_apb_base(void)
{
   return sr2351_fm_base.apb_base;
}

unsigned long rf2351_fm_get_pmu_base(void)
{
    return sr2351_fm_base.pmu_base;
}

unsigned long rf2351_fm_get_aonckg_base(void)
{
    return sr2351_fm_base.aonckg_base;
}

unsigned long rf2351_fm_get_pin_base(void)
{
    return sr2351_fm_base.pin_base;
}

static int  sr2351_fm_parse_dts(struct device_node *np)
{
    int ret;
	struct resource res;

    //np = of_find_node_by_name(NULL, "sprd_fm");
    if (NULL == np) 
	{
        SR2351_PRINT("Can't get the sprd_fm node!\n");
        return -ENODEV;
    }
    SR2351_PRINT(" find the fm_sr2351 node!\n");

    ret = of_address_to_resource(np, 0, &res);
    if (ret < 0) 
	{
        SR2351_PRINT("Can't get the fm reg base!\n");
        return -EIO;
    }
    sr2351_fm_base.fm_base = (unsigned long)ioremap_nocache(res.start, resource_size(&res));
    if(!sr2351_fm_base.fm_base)
	{
		BUG();
	}
    SR2351_PRINT("fm reg base is 0x%x\n", sr2351_fm_base.fm_base);

    ret = of_address_to_resource(np, 1, &res);
    if (ret < 0) 
	{
        SR2351_PRINT("Can't get the apb reg base!\n");
        return -EIO;
    }
    sr2351_fm_base.apb_base = (unsigned long)ioremap_nocache(res.start, resource_size(&res));
    if(!sr2351_fm_base.apb_base)
    {
    	BUG();
    }	
    SR2351_PRINT("apb reg base is 0x%x\n", sr2351_fm_base.apb_base);

	ret = of_address_to_resource(np, 2, &res);
    if (ret < 0) 
	{
        SR2351_PRINT("Can't get the pmu reg base!\n");
        return -EIO;
    }
    sr2351_fm_base.pmu_base = (unsigned long)ioremap_nocache(res.start, resource_size(&res));
    if(!sr2351_fm_base.pmu_base)
	{
		BUG();
	}
    SR2351_PRINT("pmu reg base is 0x%x\n", sr2351_fm_base.pmu_base);

	ret = of_address_to_resource(np, 3, &res);
	if (ret < 0)
	{
		SR2351_PRINT("Can't get the aonckg reg base!\n");
		return -EIO;
	}
	sr2351_fm_base.aonckg_base = (unsigned long)ioremap_nocache(res.start, resource_size(&res));
	if(!sr2351_fm_base.aonckg_base)
    {
    	BUG();
    }	
    SR2351_PRINT("pmu reg base is 0x%x\n", sr2351_fm_base.aonckg_base);

	ret = of_address_to_resource(np, 4, &res);
	if (ret < 0)
	{
		SR2351_PRINT("Can't get the pin reg base!\n");
		return -EIO;
	}
	sr2351_fm_base.pin_base =  (unsigned long)ioremap_nocache(res.start, resource_size(&res));
	if(!sr2351_fm_base.pin_base)
    {
    	BUG();
    }	
    SR2351_PRINT("pin reg base is 0x%x\n", sr2351_fm_base.pin_base);


    return 0;
}

static const struct of_device_id  of_match_table_fm[] = {
	{ .compatible = "sprd,sprd_fm", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_match_table_fm);
#endif

static int sr2351_fm_probe(struct platform_device *pdev)
{
	int ret = -EINVAL;
	char *ver_str = SR2351_FM_VERSION;

#ifdef CONFIG_OF
    struct device_node *np;
    np = pdev->dev.of_node;

    ret = sr2351_fm_parse_dts(np);
	if (ret < 0)
	{
	    SR2351_PRINT("sr2351_fm_parse_dts failed!");
		return ret;
	}
#endif

	SR2351_PRINT("**********************************************");
	SR2351_PRINT(" SR2351 FM driver ");
	SR2351_PRINT(" Version: %s", ver_str);
	SR2351_PRINT(" Build date: %s %s", __DATE__, __TIME__);
	SR2351_PRINT("**********************************************");

	ret = misc_register(&sr2351_fm_misc_device);
	if (ret < 0) 
	{
		SR2351_PRINT("misc_register failed!");
		return ret;
	}

	SR2351_PRINT("sr2351_fm_init success.\n");

	return 0;
}

static int sr2351_fm_remove(struct platform_device *pdev)
{

	SR2351_PRINT("exit_fm_driver!\n");
	misc_deregister(&sr2351_fm_misc_device);

	return 0;
}

#ifdef CONFIG_PM
static int sr2351_fm_suspend(struct platform_device *dev, pm_message_t state)
{
	sr2351_fm_enter_sleep();
    return 0;
}

static int sr2351_fm_resume(struct platform_device *dev)
{
	sr2351_fm_exit_sleep();
    return 0;
}
#else
#define sr2351_fmt_suspend NULL
#define sr2351_fm_resume NULL
#endif


static struct platform_driver sr2351_fm_driver = {
	.driver = {
		.name = "sprd_fm",
		.owner = THIS_MODULE,
    #ifdef CONFIG_OF
		 .of_match_table = of_match_ptr(of_match_table_fm) ,
	#endif	
	},
	.probe = sr2351_fm_probe,
	.remove = sr2351_fm_remove,
	.suspend = sr2351_fm_suspend,
    .resume = sr2351_fm_resume,
};

#ifndef CONFIG_OF
struct platform_device sr2351_fm_device = {
	.name = "sprd_fm",
	.id = -1,
};
#endif

int __init init_fm_driver(void)
{
    int ret;
#ifndef CONFIG_OF
	ret = platform_device_register(&sr2351_fm_device);
	if (ret)
	{
		SR2351_PRINT("sr2351_fm: platform_device_register failed: %d\n", ret);
		return ret;
	}
#endif
	ret = platform_driver_register(&sr2351_fm_driver);
	if (ret)
	{
	#ifndef CONFIG_OF
		platform_device_unregister(&sr2351_fm_device);
	#endif
		SR2351_PRINT("sr2351_fm: probe failed: %d\n", ret);
	}
	SR2351_PRINT("sr2351_fm: probe sucess: %d\n", ret);

	return ret;
}

void __exit exit_fm_driver(void)
{
    platform_driver_unregister(&sr2351_fm_driver);
}

module_init(init_fm_driver);
module_exit(exit_fm_driver);

MODULE_DESCRIPTION("SR2351 FM radio driver");
MODULE_AUTHOR("Spreadtrum Inc.");
MODULE_LICENSE("GPL");
MODULE_VERSION(SR2351_FM_VERSION);

