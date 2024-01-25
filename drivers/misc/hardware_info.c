#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
//#include <asm/uaccess.h>
#include <linux/uaccess.h>

#include <linux/proc_fs.h>

 #include <linux/hardware_info.h>
 
 #include <linux/of_platform.h>
 
//+P86801AA1, daisiqing.wt, ADD, 2022/04/28, add smartpa info for mmitest
enum{
   INVALD = -1,
   FS16XX,
   AW88261,
   AW88266,
   TAS2558,
   FS1962,
   AW88257,
   MAX_NUM,
};
static DEFINE_MUTEX(smartpa_mutex);
int smartpa_type=INVALD;
EXPORT_SYMBOL(smartpa_type);
module_param(smartpa_type, int, 0664);
MODULE_PARM_DESC(smartpa_type, "show smartpa type");
//-P86801AA1, daisiqing.wt, ADD, 2022/04/28, add smartpa info for mmitest
 
 char Lcm_name[HARDWARE_MAX_ITEM_LONGTH];//req  wuzhenzhen.wt 20140901 add for hardware info
 char TP_name[HARDWARE_MAX_ITEM_LONGTH];//req  peiyuexiang.wt 20230426 add for hardware info
 char Sar_name[HARDWARE_MAX_ITEM_LONGTH];//bug 417945 , add sar info, chenrongli.wt, 20181218
 char board_id[HARDWARE_MAX_ITEM_LONGTH];//req  wuzhenzhen.wt 20140901 add for hardware info
 static char hardwareinfo_name[HARDWARE_MAX_ITEM][HARDWARE_MAX_ITEM_LONGTH];
 char hardware_id[HARDWARE_MAX_ITEM_LONGTH];//bug537051, zhaobeilong@wt, 20200311, ADD, add hardware board id info
 //+P86801AA1, caoxin2.wt 20230829, add, add mcu hardware info
 char Mcu_name[HARDWARE_MAX_ITEM_LONGTH];
 u8 pad_mcu_cur_ver[HARDWARE_MAX_ITEM_LONGTH];
 u8 kb_mcu_cur_ver[HARDWARE_MAX_ITEM_LONGTH];

 EXPORT_SYMBOL(Mcu_name);
 EXPORT_SYMBOL(pad_mcu_cur_ver);
 EXPORT_SYMBOL(kb_mcu_cur_ver);
 //-P86801AA1, caoxin2.wt 20230829, add, add mcu hardware info

 /*+P86801AA1-1797,wangkangmin.wt,ADD,20230626, add for gki build*/
 EXPORT_SYMBOL(Lcm_name);
 EXPORT_SYMBOL(TP_name);
 EXPORT_SYMBOL(Sar_name);
 EXPORT_SYMBOL(board_id);
 EXPORT_SYMBOL(hardware_id);
/*-P86801AA1-1797,wangkangmin.wt,ADD,20230626, add for gki build*/
 /*+P86801AA1, wangcong.wt, ADD, 2023/08/25, add lcm_serialnum*/
 #ifdef CONFIG_QGKI_BUILD
 char lcm_serialnum[HARDWARE_MAX_ITEM_LONGTH];
 EXPORT_SYMBOL(lcm_serialnum);
 extern void lcm_serialnum_get(void);
 #endif
 /*-P86801AA1, wangcong.wt, ADD, 2023/08/25, add lcm_serialnum*/
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
         "BATTERY",
         "CAM_M_BACK",
         "CAM_M_FRONT",
         "BOARD_ID",
         /* Bug 538123, zhangbin2.wt, 20200311, Add for battery info */
         "CHARGER_IC",
         "BMS_GAUGE",
 };
 
 
 int hardwareinfo_set_prop(int cmd, const char *name)
 {
         if(cmd < 0 || cmd >= HARDWARE_MAX_ITEM)
                 return -1;
 
         strcpy(hardwareinfo_name[cmd], name);
 
         return 0;
 }

 int __weak tid_hardware_info_get(char *buf, int size)
 {
         snprintf(buf, size, "touch info interface is not ready\n");
 
         return 0;
 }

EXPORT_SYMBOL(hardwareinfo_set_prop);

//+P86801AA1, zhangtao10.wt, ADD, 2022/04/28, add smartpa info for mmitest
int snd_soc_set_smartpa_type(const char * name, int pa_type)
{
	pr_info("%s driver set smartpa type is : %d",name,pa_type);
	mutex_lock(&smartpa_mutex);
	switch(pa_type) {
	case FS16XX:
		smartpa_type=FS16XX;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "FS16XX");
		break;
	case FS1962:
		smartpa_type=FS1962;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "FS1962");
		break;
	case AW88261:
		smartpa_type=AW88261;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "AW88261");
		break;
	case AW88266:
		smartpa_type=AW88266;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "AW88266");
		break;
//+P86801AA1, zhangtao10.wt, ADD, 2023/05/04, aw smartpa compatibility
	case AW88257:
		smartpa_type=AW88257;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "AW88257");
		break;
//-P86801AA1, zhangtao10.wt, ADD, 2023/05/04, aw smartpa compatibility
	case TAS2558:
		smartpa_type=TAS2558;
		hardwareinfo_set_prop(HARDWARE_SMARTPA, "TAS2558");
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
//P86801AA1, zhangtao10.wt, ADD, 2022/04/28, add smartpa info for mmitest

 static long hardwareinfo_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
 {
         int ret = 0, hardwareinfo_num;
         void __user *data = (void __user *)arg;
         //char info[HARDWARE_MAX_ITEM_LONGTH];
 
         switch (cmd) {
         case HARDWARE_LCD_GET:
                 hardwareinfo_set_prop(HARDWARE_LCD, Lcm_name);
                 hardwareinfo_num = HARDWARE_LCD;
                 break;
         case HARDWARE_TP_GET:
                 hardwareinfo_set_prop(HARDWARE_TP,TP_name);
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
         case HARDWARE_BT_GET:
                 hardwareinfo_set_prop(HARDWARE_BT, "WCN3988");
                 hardwareinfo_num = HARDWARE_BT;
                 break;
         case HARDWARE_WIFI_GET:
                 hardwareinfo_set_prop(HARDWARE_WIFI, "WCN3988");
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
// + wt add Adaptive ss sensor hal, ludandan2 20230511, start
                 //hardwareinfo_set_prop(HARDWARE_SAR, Sar_name);
// - wt add Adaptive ss sensor hal, ludandan2 20230511, end
                  hardwareinfo_num = HARDWARE_SAR;
                  break;
          //+P86801AA1,caoxin2.wt,add,2023/08/29,KB add hardware_info
          case HARDWARE_MCU_GET:
	          hardwareinfo_set_prop(HARDWARE_MCU, Mcu_name);
	          hardwareinfo_num = HARDWARE_MCU;
		  break;
          case HARDWARE_PAD_MCU_GET:
	          hardwareinfo_set_prop(HARDWARE_PAD_MCU, pad_mcu_cur_ver);
	          hardwareinfo_num = HARDWARE_PAD_MCU;
		  break;
          case HARDWARE_KB_MCU_GET:
	          hardwareinfo_set_prop(HARDWARE_KB_MCU, kb_mcu_cur_ver);
	          hardwareinfo_num = HARDWARE_KB_MCU;
		  break;
          //-P86801AA1,caoxin2.wt,add,2023/08/29,KB add hardware_info
          case HARDWARE_GPS_GET:
                  hardwareinfo_set_prop(HARDWARE_GPS, "WCN3988");
              hardwareinfo_num = HARDWARE_GPS;
                  break;
          case HARDWARE_FM_GET:
                  hardwareinfo_set_prop(HARDWARE_FM, "WCN3988");
              hardwareinfo_num = HARDWARE_FM;
                  break;
          case HARDWARE_BATTERY_ID_GET:
                  hardwareinfo_num = HARDWARE_BATTERY_ID;
                  break;
          /* +Bug 538123, zhangbin2.wt, 20200311, Add for battery info, Begin +++ */
          case HARDWARE_CHARGER_IC_INFO_GET:
                  hardwareinfo_num = HARDWARE_CHARGER_IC;
                  break;
          case HARDWARE_BMS_GAUGE_GET:
                  hardwareinfo_num = HARDWARE_BMS_GAUGE;
                  break;
          /* -Bug 538123, zhangbin2.wt, 20200311, Add for battery info, End --- */
          case HARDWARE_BACK_CAM_MOUDULE_ID_GET:
                  hardwareinfo_num = HARDWARE_BACK_CAM_MOUDULE_ID;
                  break;
          case HARDWARE_FRONT_CAM_MODULE_ID_GET:
                  hardwareinfo_num = HARDWARE_FRONT_CAM_MOUDULE_ID;
                  break;
          case HARDWARE_BOARD_ID_GET:
                  hardwareinfo_set_prop(HARDWARE_BOARD_ID, board_id);
                  hardwareinfo_num = HARDWARE_BOARD_ID;
                  break;
  		//bug537051, zhaobeilong@wt, 20200311, ADD, add hardware board id info,start
          case HARDWARE_HARDWARE_ID_GET:
                  hardwareinfo_set_prop(HARDWARE_HARDWARE_ID, hardware_id);
                  hardwareinfo_num = HARDWARE_HARDWARE_ID;
  		break;
  		//bug537051, zhaobeilong@wt, 20200311, ADD, add hardware board id info,end
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
  //bug 538153, baoshulin@wingtech.com, 20200310, add mmitest and smartpa info
	//+P86801AA1, daisiqing.wt, ADD, 2022/04/28, add smartpa info for mmitest
          case HARDWARE_SMARTPA_IC_GET:
                  pr_info("%s smartpa get ic",__func__);
                  hardwareinfo_num = HARDWARE_SMARTPA;
                  break;
	//+P86801AA1, daisiqing.wt, ADD, 2022/04/28, add smartpa info for mmitest
          /*+P86801AA1, wangcong.wt, ADD, 2023/08/25, add lcm_serialnum*/
          #ifdef CONFIG_QGKI_BUILD
          case HARDWARE_LCD_SERIALNUM_GET:
                  lcm_serialnum_get();
                  hardwareinfo_set_prop(HARDWARE_LCD_SERIALNUM, lcm_serialnum);
                  hardwareinfo_num = HARDWARE_LCD_SERIALNUM;
                  break;
          #endif
          /*-P86801AA1, wangcong.wt, ADD, 2023/08/25, add lcm_serialnum*/
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
				  else if (i == HARDWARE_TP)
				  {
                          sprintf(temp_buffer, "%s : %s\n", hardwareinfo_items[i], TP_name);
				  }
                  else if (i == HARDWARE_BT || i == HARDWARE_WIFI || i == HARDWARE_GPS || i == HARDWARE_FM)
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
  
  static struct of_device_id boardinfo_of_match[] = {
          { .compatible = "wt:boardinfo", },
          {}
  };
  
  static struct platform_driver boardinfo_driver = {
          .driver = {
                  .name   = "boardinfo",
                  .owner  = THIS_MODULE,
                  .of_match_table = boardinfo_of_match,
          },
          .probe          = boardinfo_probe,
          .remove         = boardinfo_remove,
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
  
  

