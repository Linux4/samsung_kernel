#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include <linux/proc_fs.h>

#include <linux/hardware_info.h>

#include <linux/platform_device.h>
#include <linux/uaccess.h>

char Ctp_name[HARDWARE_MAX_ITEM_LONGTH];// wanglongfei.wt 20201216 add for tp hardware info
char Lcm_name[HARDWARE_MAX_ITEM_LONGTH];//req  wuzhenzhen.wt 20140901 add for hardware info
char Sar_name[HARDWARE_MAX_ITEM_LONGTH];//bug 417945 , add sar info, chenrongli.wt, 20181218
char board_id[HARDWARE_MAX_ITEM_LONGTH];//req  wuzhenzhen.wt 20140901 add for hardware info
// +ckl,zhangxingyuan.wt,add,20220722,add hardware info
char smart_pa_name[HARDWARE_MAX_ITEM_LONGTH];
// -ckl,zhangxingyuan.wt,add,20220722,add hardware info
char hardware_id[HARDWARE_MAX_ITEM_LONGTH];
char nfc_version[HARDWARE_MAX_ITEM_LONGTH];//bug 604664 ,cuijiang.wt,2021.09.29,add nfc info
static char hardwareinfo_name[HARDWARE_MAX_ITEM][HARDWARE_MAX_ITEM_LONGTH];

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

//+ bug 828710, liuling 2023.03.13 add get hardwareinfo, start
char* hardwareinfo_get_prop(int cmd)
{
    if(cmd < 0 || cmd >= HARDWARE_MAX_ITEM)
        return NULL;

    return hardwareinfo_name[cmd];
}
//- bug 828710, liuling 2023.03.13 add get hardwareinfo, end

int __weak tid_hardware_info_get(char *buf, int size)
{
	snprintf(buf, size, "touch info interface is not ready\n");

	return 0;
}

EXPORT_SYMBOL_GPL(hardwareinfo_set_prop);

//Bug 432505 njm@wt, 20180314 start
//for mtk platform
static char* boardid_get(void)
{
	char* s1= "";
	char* s2="not found";

	s1 = strstr(saved_command_line, "board_id=");
	if(!s1) {
		printk("board_id not found in cmdline\n");
		return s2;
	}
	s1 += strlen("board_id=");
	strncpy(board_id, s1, 9);
	board_id[10]='\0';
	s1 = board_id;
	//printk("board_id found in cmdline : %s\n", board_id);

	return s1;
}

/* +Req S96818AA1-1936,shenwenlei.wt,20230423, audio bringup */
#ifdef CONFIG_WT_PROJECT_AUDIO_PA
char* boardid_get_n28(void)
{
        char* s1= "";

        s1 = boardid_get();

        printk("board_id found in cmdline : %s\n", board_id);

        return s1;
}
EXPORT_SYMBOL_GPL(boardid_get_n28);
#endif
/* -Req S96818AA1-1936,shenwenlei.wt,20230423, audio bringup */

//Bug 438050 njm@wt, 20190415 start
static char* hwid_get(void)
{
	char* s1= "";
	char* s2="not found";
	char *ptr =NULL;

	s1 = strstr(saved_command_line, "hw_id=");
	if(!s1) {
		printk("hw_id not found in cmdline\n");
		return s2;
	}
	s1 += strlen("hw_id=");
	ptr=s1;
	while(*ptr != ' ' && *ptr != '\0') {
		ptr++;
	}
	strncpy(hardware_id, (const char *)s1,ptr-s1);
	hardware_id[ptr-s1]='\0';
	s1 = hardware_id;
	//printk("hw_id found in cmdline s1=: %s\n", s1);
	//Bug 438050 njm@wt, 20190415 end

	return s1;
}
//Bug 432505 njm@wt, 20180314 end

/*get lcm serialnum */
/* +Req S96818AA1-1936,liuzhizun2.wt,modify,2023/06/07, td4160 read panel sn*/
unsigned char n28_lcm_sn[20];
char lcm_serialnum[HARDWARE_MAX_ITEM_LONGTH];
static char* hw_lcm_serialnum_get(void)
{
	char* s1= "";
	char* s2="not found";
	char *ptr =NULL;

	if ((strcmp(Lcm_name, "n28_td4160_dsi_vdo_hdp_xinxian_inx") == 0)
			|| (strcmp(Lcm_name, "n28_td4160_dsi_vdo_hdp_boe_boe") == 0)) {
		s1 = n28_lcm_sn;
		strncpy(lcm_serialnum, n28_lcm_sn, 20);
		lcm_serialnum[20] = '\0';
		printk("lcm_serialnum found in tp : %s\n", lcm_serialnum);
		return s1;
	} else {
		s1 = strstr(saved_command_line, "lcm_serialnum:");
		if(!s1) {
			printk("lcm_serialnum not found in cmdline\n");
			return s2;
		}
	}
	s1 += strlen("lcm_serialnum:");
	ptr=s1;
	while(*ptr != ' ' && *ptr != '\0') {
		ptr++;
	}
	strncpy(lcm_serialnum, (const char *)s1,ptr-s1);
	lcm_serialnum[ptr-s1]='\0';
	printk("lcm_serialnum found in cmdline : %s\n", lcm_serialnum);
	s1 = lcm_serialnum;
	return s1;
}
/* -Req S96818AA1-1936,liuzhizun2.wt,modify,2023/06/07, td4160 read panel sn*/

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
		hardwareinfo_set_prop(HARDWARE_TP, Ctp_name);//wanglongfei.wt 20201216 add for tp hardware info
		hardwareinfo_num = HARDWARE_TP;
		//tid_hardware_info_get(hardwareinfo_name[hardwareinfo_num],
		//				ARRAY_SIZE(hardwareinfo_name[0]));
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
    //+bug604664,liangyiyi.wt,ADD,2022/7/1,add micro info for mmigroup apk
	case HARDWARE_MACRO_CAM_GET:
		hardwareinfo_num = HARDWARE_MICRO_CAM;
		break;
    //+bug604664,liangyiyi.wt,ADD,2022/7/1,add micro info for mmigroup apk
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
		hardwareinfo_num = HARDWARE_NFC;
		break;
        //-bug 604665,lulinliang.wt,2020.12.18,add nfc info
	case HARDWARE_BATTERY_ID_GET:
		hardwareinfo_num = HARDWARE_BATTERY_ID;
		break;
	//+bug 436809  modify getian.wt 20190403 Add charger IC model information in factory mode
	case HARDWARE_CHARGER_IC_INFO_GET:
//bug770483,gudi.wt,add hardware charger ic info
#ifdef CONFIG_MT6360_PMIC
		hardwareinfo_set_prop(HARDWARE_CHARGER_IC_INFO, "MT6360_PMU_CHARGER");
#endif
		hardwareinfo_num = HARDWARE_CHARGER_IC_INFO;
		break;
	//-bug 436809  modify getian.wt 20190403 Add charger IC model information in factory mode
	//+bug 717431, liyiying.wt, add, 2021/2/21, n21s add gauge mmi message
	case HARDWARE_BMS_GAUGE_INFO_GET:
		hardwareinfo_num = HARDWARE_BMS_GAUGE_INFO;
		break;
	//-bug 717431, liyiying.wt, add, 2021/2/21, n21s add gauge mmi message
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
    //+bug604664,liangyiyi.wt,ADD,2022/7/1,add micro info for mmigroup apk
    case HARDWARE_MACRO_CAM_MOUDULE_ID_GET:
		hardwareinfo_num = HARDWARE_MICRO_CAM_MOUDULE_ID;
		break;
    //-bug604664,liangyiyi.wt,ADD,2022/7/1,add micro info for mmigroup apk
	// +ckl,zhangxingyuan.wt,add,20220722,add hardware info
  	case HARDWARE_SMARTPA_IC_GET:
  		hardwareinfo_set_prop(HARDWARE_SMART_PA_ID, smart_pa_name);
  		hardwareinfo_num = HARDWARE_SMART_PA_ID;
  		break;
  	// -ckl,zhangxingyuan.wt,add,20220722,add hardware info	

	case HARDWARE_BOARD_ID_GET:
		//Bug 432505 njm@wt, 20190314 start
		boardid_get();
		//Bug 432505 njm@wt, 20190314 end
		hardwareinfo_set_prop(HARDWARE_BOARD_ID, board_id);
		hardwareinfo_num = HARDWARE_BOARD_ID;
		break;
		//Bug 432505 njm@wt, 20190314 start
	case HARDWARE_HARDWARE_ID_GET:
		hwid_get();
		hardwareinfo_set_prop(HARDWARE_HARDWARE_ID, hardware_id);
		hardwareinfo_num = HARDWARE_HARDWARE_ID;
		break;
		//Bug 432505 njm@wt, 20190314 end
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
	/* get lcm serialnum */
	case HARDWARE_LCD_SERIALNUM_GET:
		hw_lcm_serialnum_get();
		hardwareinfo_set_prop(HARDWARE_LCD_SERIALNUM, lcm_serialnum);
		hardwareinfo_num = HARDWARE_LCD_SERIALNUM;
		break;
#ifdef CONFIG_N28_CHARGER_PRIVATE
		/* +Req S96818AA1-5169,zhouxiaopeng2.wt,20230519, mode information increased */
	case HARDWARE_PD_CHARGER_GET:
		hardwareinfo_num = HARDWARE_PD_CHARGER;
		break;
	case HARDWARE_CHARGER_PUMP_GET:
		hardwareinfo_num = HARDWARE_CHARGER_PUMP;
		break;
		/* -Req S96818AA1-5169,zhouxiaopeng2.wt,20230519, mode information increased */
#endif
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



