#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <asm/uaccess.h>

#include <linux/proc_fs.h>

#include <linux/hardware_info.h>

#include <linux/platform_device.h>
#include <linux/uaccess.h>

//+S98901AA1-827, zhouweijie.wt, add, 2024/04/15, sia81xx bring up
static DEFINE_MUTEX(smartpa_mutex);
int smartpa_type = INVALD;
EXPORT_SYMBOL(smartpa_type);
module_param(smartpa_type, int, 0664);
MODULE_PARM_DESC(smartpa_type, "show smartpa type");
//-S98901AA1-827, zhouweijie.wt, add, 2024/04/15, sia81xx bring up

char Tp_name[HARDWARE_MAX_ITEM_LONGTH];// caoxin2.wt 2024.05.16 add for tp hardware info
char Lcm_name[HARDWARE_MAX_ITEM_LONGTH];//req  wuzhenzhen.wt 20140901 add for hardware info
char Sar_name[HARDWARE_MAX_ITEM_LONGTH];//bug 417945 , add sar info, chenrongli.wt, 20181218
//char board_id[HARDWARE_MAX_ITEM_LONGTH];//req  wuzhenzhen.wt 20140901 add for hardware info
//char hardware_id[HARDWARE_MAX_ITEM_LONGTH];
char nfc_version[HARDWARE_MAX_ITEM_LONGTH];//bug 604664 ,cuijiang.wt,2021.09.29,add nfc info
static char hardwareinfo_name[HARDWARE_MAX_ITEM][HARDWARE_MAX_ITEM_LONGTH];

static char *board_id;
EXPORT_SYMBOL_GPL(Tp_name);

char* hardwareinfo_items[HARDWARE_MAX_ITEM] =
{
	"LCD",
	"TP",
	"MEMORY",
	"CAM_FRONT",
	"CAM_BACK",
	"BT",
	"WIFI",
	"GSENSOR",
	"PLSENSOR",
	"GYROSENSOR",
	"MSENSOR",
	"SAR", //bug 417945 , add sar info, chenrongli.wt, 20181218
	"GPS",
	"FM",
        "NFC",//bug 604665,lulinliang.wt,2020.12.18,add nfc info
	"BATTERY",
	"CAM_M_BACK",
	"CAM_M_FRONT",
	"BOARD_ID",
	"HARDWARE_ID"
};


int hardwareinfo_set_prop(int cmd, const char *name)
{
	if(cmd < 0 || cmd >= HARDWARE_MAX_ITEM)
		return -1;

	strcpy(hardwareinfo_name[cmd], name);

	return 0;
}

/* +S98901AA1-8583, zhouweijie.wt, 20240713, add get hardwareinfo */
char* hardwareinfo_get_prop(int cmd)
{
    if(cmd < 0 || cmd >= HARDWARE_MAX_ITEM)
        return NULL;

    return hardwareinfo_name[cmd];
}
/* -S98901AA1-8583, zhouweijie.wt, 20240713, add get hardwareinfo */

int __weak tid_hardware_info_get(char *buf, int size)
{
	snprintf(buf, size, "touch info interface is not ready\n");

	return 0;
}

EXPORT_SYMBOL_GPL(hardwareinfo_set_prop);
EXPORT_SYMBOL_GPL(hardwareinfo_get_prop);
EXPORT_SYMBOL_GPL(Lcm_name);
//char lcm_serialnum[HARDWARE_MAX_ITEM_LONGTH];
//+S98901AA1-827, zhouweijie.wt, add, 2024/04/15, sia81xx bring up
int snd_soc_set_smartpa_type(const char * name, int pa_type)
{
	pr_info("%s driver set smartpa type is : %d",name,pa_type);
	mutex_lock(&smartpa_mutex);
	switch(pa_type) {
	case FS16XX:
		smartpa_type = FS16XX;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "FS16XX");
		break;
	case FS1962:
		smartpa_type = FS1962;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "FS1962");
		break;
	case AW88261:
		smartpa_type = AW88261;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "AW88261");
		break;
	case AW88266:
		smartpa_type = AW88266;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "AW88266");
		break;
	//+P86801AA1, zhangtao10.wt, ADD, 2023/05/04, aw smartpa compatibility
	case AW88257:
		smartpa_type = AW88257;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "AW88257");
		break;
	//-P86801AA1, zhangtao10.wt, ADD, 2023/05/04, aw smartpa compatibility
	case TAS2558:
		smartpa_type = TAS2558;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "TAS2558");
		break;
	case sia81xx:
		smartpa_type = sia81xx;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "sia81xx");
		break;
	case aw88394:
		smartpa_type = aw88394;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "aw88394");
		break;
	default:
		pr_info("this PA does not support\n\r");
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "unknown");
		break;
	}
	mutex_unlock(&smartpa_mutex);
	return smartpa_type;
 }
EXPORT_SYMBOL_GPL(snd_soc_set_smartpa_type);
//-S98901AA1-827, zhouweijie.wt, add, 2024/04/15, sia81xx bring up

unsigned char a16_lcm_sn[30] = { 0, };
EXPORT_SYMBOL_GPL(a16_lcm_sn);
char lcm_serialnum[HARDWARE_MAX_ITEM_LONGTH];
static char* hw_lcm_serialnum_get(void)
{
	char* s1= "";

	s1 = a16_lcm_sn;
	strncpy(lcm_serialnum, a16_lcm_sn, 23);
	lcm_serialnum[23] = '\0';
	printk("lcm_serialnum found in tp : %s\n", lcm_serialnum);

	return s1;
}

static long hardwareinfo_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
	int ret = 0, hardwareinfo_num = HARDWARE_MAX_ITEM;
	void __user *data = (void __user *)arg;
	//char info[HARDWARE_MAX_ITEM_LONGTH];

	switch (cmd) {
	case HARDWARE_LCD_GET:
		hardwareinfo_set_prop(HARDWARE_LCD, Lcm_name);//req  wuzhenzhen.wt 20140901 add for hardware info
		hardwareinfo_num = HARDWARE_LCD;
		break;
	case HARDWARE_TP_GET:
		hardwareinfo_set_prop(HARDWARE_TP, Tp_name);//caoxin2.wt 2024.05.16 add for tp hardware info
		hardwareinfo_num = HARDWARE_TP;
		break;
	case HARDWARE_FLASH_GET:
		hardwareinfo_num = HARDWARE_FLASH;
		break;
	case HARDWARE_FRONT_CAM_GET:
		hardwareinfo_num = HARDWARE_FRONT_CAM;
		break;
	case HARDWARE_BACK_CAM_GET:
		hardwareinfo_num = HARDWARE_BACK_CAM;
		break;
	case HARDWARE_BACK_SUBCAM_GET:
		hardwareinfo_num = HARDWARE_BACK_SUB_CAM;
		break;
	//+bug604664,zhouyikuan.wt,ADD,2020/12/17,add wide angle info for mmigroup apk
	case HARDWARE_WIDE_ANGLE_CAM_GET:
		hardwareinfo_num = HARDWARE_WIDE_ANGLE_CAM;
		break;
	//-bug604664,zhouyikuan.wt,ADD,2020/12/17,add wide angle info for mmigroup apk
	//+S96818AA1-1936,lijiazhen2.wt,ADD,2024/05/06, add camera module info for factory apk
	case HARDWARE_MACRO_CAM_GET:
		hardwareinfo_num = HARDWARE_MACRO_CAM;
		break;
	//+S96818AA1-1936,lijiazhen2.wt,ADD,2024/05/06, add camera module info for factory apk
	case HARDWARE_BT_GET:
		hardwareinfo_set_prop(HARDWARE_BT, "MT6631");
		hardwareinfo_num = HARDWARE_BT;
		break;
	case HARDWARE_WIFI_GET:
		hardwareinfo_set_prop(HARDWARE_WIFI, "MT6631");
		hardwareinfo_num = HARDWARE_WIFI;
		break;
	case HARDWARE_ACCELEROMETER_GET:
		hardwareinfo_num = HARDWARE_ACCELEROMETER;
		break;
	case HARDWARE_ALSPS_GET:
		hardwareinfo_num = HARDWARE_ALSPS;
		break;
	case HARDWARE_GYROSCOPE_GET:
		hardwareinfo_num = HARDWARE_GYROSCOPE;
		break;
	case HARDWARE_MAGNETOMETER_GET:
		hardwareinfo_num = HARDWARE_MAGNETOMETER;
		break;
	/*bug 417945 , add sar info, chenrongli.wt, 20181218, begin*/
	case HARDWARE_SAR_GET:
		//hardwareinfo_set_prop(HARDWARE_SAR, Sar_name);
		hardwareinfo_num = HARDWARE_SAR;
		break;
	/*bug 417945 , add sar info, chenrongli.wt, 20181218, end*/
	case HARDWARE_GPS_GET:
		hardwareinfo_set_prop(HARDWARE_GPS, "MT6631");
	    hardwareinfo_num = HARDWARE_GPS;
		break;
	case HARDWARE_FM_GET:
		hardwareinfo_set_prop(HARDWARE_FM, "MT6631");
	    hardwareinfo_num = HARDWARE_FM;
		break;
       //+bug 604665,lulinliang.wt,2020.12.18,add nfc info
        case HARDWARE_NFC_GET:
               // hardwareinfo_set_prop(HARDWARE_NFC, nfc_version);//bug 604664,cuijiang.wt,2021.09.29,add nfc info
               // hardwareinfo_set_prop(HARDWARE_NFC, "S3NRN4VXC1-MX30");
                hardwareinfo_num = HARDWARE_NFC;
                break;
        //-bug 604665,lulinliang.wt,2020.12.18,add nfc info
	case HARDWARE_BATTERY_ID_GET:
		hardwareinfo_num = HARDWARE_BATTERY_ID;
		break;
	//+bug 436809  modify getian.wt 20190403 Add charger IC model information in factory mode
	case HARDWARE_CHARGER_IC_INFO_GET:
		hardwareinfo_num = HARDWARE_CHARGER_IC_INFO;
		break;
	//-bug 436809  modify getian.wt 20190403 Add charger IC model information in factory mode
	case HARDWARE_PD_CHARGER_GET:
		hardwareinfo_num = HARDWARE_PD_CHARGER;
		break;
	case HARDWARE_CHARGER_PUMP_GET:
		hardwareinfo_num = HARDWARE_CHARGER_PUMP;
		break;

	case HARDWARE_BACK_CAM_MOUDULE_ID_GET:
		hardwareinfo_num = HARDWARE_BACK_CAM_MOUDULE_ID;
		break;
	case HARDWARE_BACK_SUBCAM_MODULEID_GET:
		hardwareinfo_num = HARDWARE_BACK_SUBCAM_MOUDULE_ID;
		break;
	case HARDWARE_FRONT_CAM_MODULE_ID_GET:
		hardwareinfo_num = HARDWARE_FRONT_CAM_MOUDULE_ID;
		break;
		//+bug604664,zhouyikuan.wt,ADD,2020/12/17,add wide angle info for mmigroup apk
	case HARDWARE_WIDE_ANGLE_CAM_MOUDULE_ID_GET:
		hardwareinfo_num = HARDWARE_WIDE_ANGLE_CAM_MOUDULE_ID;
		break;
		//-bug604664,zhouyikuan.wt,ADD,2020/12/17,add wide angle info for mmigroup apk
	//+S96818AA1-1936,lijiazhen2.wt,ADD,2024/05/06, add camera module info for factory apk
	case HARDWARE_MACRO_CAM_MOUDULE_ID_GET:
		hardwareinfo_num = HARDWARE_MACRO_CAM_MOUDULE_ID;
		break;
	//-S96818AA1-1936,lijiazhen2.wt,ADD,2024/05/06, add camera module info for factory apk
/* 	case HARDWARE_BOARD_ID_GET:
		hardwareinfo_set_prop(HARDWARE_BOARD_ID, board_id);
		hardwareinfo_num = HARDWARE_BOARD_ID;
		break;
	case HARDWARE_HARDWARE_ID_GET:
		hardwareinfo_set_prop(HARDWARE_HARDWARE_ID, hardware_id);
		hardwareinfo_num = HARDWARE_HARDWARE_ID;
		break; */
	case HARDWARE_BACK_CAM_MOUDULE_ID_SET:
		if(copy_from_user(hardwareinfo_name[HARDWARE_BACK_CAM_MOUDULE_ID], data,sizeof(data)))
		{
			pr_err("wgz copy_from_user error");
			ret =  -EINVAL;
		}
		goto set_ok;
		break;
	case HARDWARE_FRONT_CAM_MODULE_ID_SET:
		if(copy_from_user(hardwareinfo_name[HARDWARE_FRONT_CAM_MOUDULE_ID], data,sizeof(data)))
		{
			pr_err("wgz copy_from_user error");
			ret =  -EINVAL;
		}
		goto set_ok;
		break;
  	//+S98901AA1-827, zhouweijie.wt, add, 2024/04/15, sia81xx bring up
	case HARDWARE_SMARTPA_IC_GET:
		pr_info("%s smartpa get ic",__func__);
		hardwareinfo_num = HARDWARE_SMARTPA;
		break;
  	//-S98901AA1-827, zhouweijie.wt, add, 2024/04/15, sia81xx bring up
	//zhaosidong.wt, BMS info
	case HARDWARE_BMS_GAUGE_GET:
		hardwareinfo_num = HARDWARE_BMS_GAUGE_ID;
		break;
	case HARDWARE_LCD_SERIALNUM_GET:
		hw_lcm_serialnum_get();
		hardwareinfo_set_prop(HARDWARE_LCD_SERIALNUM, lcm_serialnum);
		hardwareinfo_num = HARDWARE_LCD_SERIALNUM;
		break;
	default:
		ret = -EINVAL;
		goto err_out;
	}
	//memset(data, 0, HARDWARE_MAX_ITEM_LONGTH);//clear the buffer
	if (copy_to_user(data, hardwareinfo_name[hardwareinfo_num], strlen(hardwareinfo_name[hardwareinfo_num]))){
		//printk("%s, copy to usr error\n", __func__);
		ret =  -EINVAL;
	}
set_ok:
err_out:
	return ret;
}

static ssize_t show_boardinfo(struct device *dev, struct device_attribute *attr, char *buf)
{
	int i = 0;
	char temp_buffer[HARDWARE_MAX_ITEM_LONGTH];
	int buf_size = 0;

	for(i = 0; i < HARDWARE_MAX_ITEM; i++)
	{
		memset(temp_buffer, 0, HARDWARE_MAX_ITEM_LONGTH);
		if(i == HARDWARE_LCD)
		{
			sprintf(temp_buffer, "%s : %s\n", hardwareinfo_items[i], Lcm_name);
		}
		else if(i == HARDWARE_BT || i == HARDWARE_WIFI || i == HARDWARE_GPS || i == HARDWARE_FM)
		{
			sprintf(temp_buffer, "%s : %s\n", hardwareinfo_items[i], "Qualcomm");
		}
		else
		{
			sprintf(temp_buffer, "%s : %s\n", hardwareinfo_items[i], hardwareinfo_name[i]);
		}
		strcat(buf, temp_buffer);
		buf_size +=strlen(temp_buffer);
	}

	return buf_size;
}
static DEVICE_ATTR(boardinfo, S_IRUGO, show_boardinfo, NULL);

module_param(board_id, charp, S_IRUGO);

char *get_board_id(void)
{
	//printk("%s, board_id: %s\n", __func__, board_id);
	return board_id;
}
EXPORT_SYMBOL(get_board_id);

static int boardinfo_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int rc = 0;

	printk("%s: start\n", __func__);

	rc = device_create_file(dev, &dev_attr_boardinfo);
	if (rc < 0)
		return rc;

	dev_info(dev, "%s: ok\n", __func__);

	return 0;
}

static int boardinfo_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	device_remove_file(dev, &dev_attr_boardinfo);
	dev_info(&pdev->dev, "%s\n", __func__);
	return 0;
}


static struct file_operations hardwareinfo_fops = {
	.owner = THIS_MODULE,
	.open = simple_open,
	.unlocked_ioctl = hardwareinfo_ioctl,
	.compat_ioctl = hardwareinfo_ioctl,
};

static struct miscdevice hardwareinfo_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "hardwareinfo",
	.fops = &hardwareinfo_fops,
};

#if 0
static struct of_device_id boardinfo_of_match[] = {
	{ .compatible = "wt:boardinfo", },
	{}
};
#endif

static struct platform_driver boardinfo_driver = {
	.driver = {
		.name	= "boardinfo",
		.owner	= THIS_MODULE,
		//.of_match_table = boardinfo_of_match,
	},
	.probe		= boardinfo_probe,
	.remove		= boardinfo_remove,
};



static int __init hardwareinfo_init_module(void)
{
	int ret, i;

	for(i = 0; i < HARDWARE_MAX_ITEM; i++)
		strcpy(hardwareinfo_name[i], "NULL");

	ret = misc_register(&hardwareinfo_device);
	if(ret < 0){
		return -ENODEV;
	}

	ret = platform_driver_register(&boardinfo_driver);
	if(ret != 0)
	{
		return -ENODEV;
	}

	get_board_id();
	return 0;
}

static void __exit hardwareinfo_exit_module(void)
{
	misc_deregister(&hardwareinfo_device);
}

module_init(hardwareinfo_init_module);
module_exit(hardwareinfo_exit_module);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ming He <heming@wingtech.com>");
