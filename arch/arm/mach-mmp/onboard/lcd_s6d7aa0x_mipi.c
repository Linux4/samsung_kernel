#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/regulator/machine.h>
#include <linux/lcd.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/mfp-mmp2.h>
#include <mach/mmp3.h>
#include <mach/pxa988.h>
#include <mach/pxa168fb.h>
#include <mach/regs-mcu.h>
#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#if defined(CONFIG_CPU_PXA1088)
#include <mach/mfp-pxa1088-delos.h>
#elif defined(CONFIG_CPU_PXA988)
#include <mach/mfp-pxa988-aruba.h>
#endif
#include "../common.h"
#include "s6d7aa0x_param.h"
static struct pxa168fb_info *fbi_global = NULL;
extern int get_panel_id(void);

#ifdef CONFIG_LDI_SUPPORT_MDNIE
static struct class *lcd_mDNIe_class;
static int isReadyTo_mDNIe = 1;
static struct mdnie_config mDNIe_cfg;
static void s6d78a0x_set_mdnie(struct pxa168fb_info *fbi, int scenario);
#ifdef ENABLE_MDNIE_TUNING
#define TUNING_FILE_PATH "/sdcard/mdnie/"
#define MAX_TUNING_FILE_NAME 100
#define NR_MDNIE_DATA 114
static int tuning_enable;
static char tuning_filename[MAX_TUNING_FILE_NAME];
static unsigned char mDNIe_data[NR_MDNIE_DATA] = {S6D78A0X_REG_MDNIE, 0x00};
static struct dsi_cmd_desc s6d78a0x_video_display_mDNIe_cmds[] = {
	{DSI_DI_DCS_LWRITE,0,0,sizeof(mDNIe_data),mDNIe_data},
};
static void s6d78a0x_set_mdnie_tuning(struct pxa168fb_info *fbi);
#endif	/* ENABLE_MDNIE_TUNING */
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

#define BOARD_ID_REV00 (0x0)
#define BOARD_ID_REV01 (0x1)

#ifdef  CONFIG_LDI_SUPPORT_MDNIE
static void s6d78a0x_set_mdnie(struct pxa168fb_info *fbi, int scenario)
{
	if (scenario > SCENARIO_MAX) {
		printk("%s, exceeded max scenario(val:%d)\n", __func__, scenario);
		return;
	}

	pxa168_dsi_cmd_array_tx(fbi, s6d78a0x_video_display_enable_access_register,
			ARRAY_SIZE(s6d78a0x_video_display_enable_access_register));

	pxa168_dsi_cmd_array_tx(fbi_global,
			&s6d78a0x_video_display_mDNIe_scenario_cmds[scenario],
			ARRAY_SIZE(s6d78a0x_video_display_mDNIe_size));

	pxa168_dsi_cmd_array_tx(fbi, s6d78a0x_video_display_disable_access_register,
			ARRAY_SIZE(s6d78a0x_video_display_disable_access_register));
}

#ifdef ENABLE_MDNIE_TUNING
static void s6d78a0x_set_mdnie_tuning(struct pxa168fb_info *fbi)
{
	printk("%s, set mDNIe tuning data\n", __func__);
	pxa168_dsi_cmd_array_tx(fbi, s6d78a0x_video_display_enable_access_register,
			ARRAY_SIZE(s6d78a0x_video_display_enable_access_register));

	pxa168_dsi_cmd_array_tx(fbi, s6d78a0x_video_display_mDNIe_cmds,
			ARRAY_SIZE(s6d78a0x_video_display_mDNIe_cmds));

	pxa168_dsi_cmd_array_tx(fbi, s6d78a0x_video_display_disable_access_register,
			ARRAY_SIZE(s6d78a0x_video_display_disable_access_register));
}
#endif	/* ENABLE_MDNIE_TUNING */
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

#ifdef CONFIG_LDI_SUPPORT_MDNIE
#ifdef ENABLE_MDNIE_TUNING
static int parse_text(char *src, int len)
{
	int i;
	int data=0, value=0, count=1, comment=0;
	char *cur_position;

	count++;
	cur_position = src;
	for(i=0; i<len; i++, cur_position++) {
		char a = *cur_position;
		switch(a) {
		case '\r':
		case '\n':
			comment = 0;
			data = 0;
			break;
		case '/':
			comment++;
			data = 0;
			break;
		case '0'...'9':
			if(comment>1)
				break;
			if(data==0 && a=='0')
				data=1;
			else if(data==2){
				data=3;
				value = (a-'0')*16;
			}
			else if(data==3){
				value += (a-'0');
				mDNIe_data[count]=value;
				printk("Tuning value[%d]=0x%02X\n", count, value);
				count++;
				data=0;
			}
			break;
		case 'a'...'f':
		case 'A'...'F':
			if(comment>1)
				break;
			if(data==2){
				data=3;
				if(a<'a') value = (a-'A'+10)*16;
				else value = (a-'a'+10)*16;
			}
			else if(data==3){
				if(a<'a') value += (a-'A'+10);
				else value += (a-'a'+10);
				mDNIe_data[count]=value;
				printk("Tuning value[%d]=0x%02X\n", count, value);
				count++;
				data=0;
			}
			break;
		case 'x':
		case 'X':
			if(data==1)
				data=2;
			break;
		default:
			if(comment==1)
				comment = 0;
			data = 0;
			break;
		}
	}

	return count;
}

static int load_tuning_data(char *filename)
{
	struct file *filp;
	char	*dp;
	long	l ;
	loff_t  pos;
	int     ret, num;
	mm_segment_t fs;

	printk("[INFO]:%s called loading file name : [%s]\n",__func__,filename);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if(IS_ERR(filp))
	{
		printk(KERN_ERR "[ERROR]:File open failed %d\n", IS_ERR(filp));
		return -1;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	printk("[INFO]: Loading File Size : %ld(bytes)", l);

	dp = kmalloc(l+10, GFP_KERNEL);
	if(dp == NULL){
		printk("[ERROR]:Can't not alloc memory for tuning file load\n");
		filp_close(filp, current->files);
		return -1;
	}
	pos = 0;
	memset(dp, 0, l);
	printk("[INFO] : before vfs_read()\n");
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	printk("[INFO] : after vfs_read()\n");

	if(ret != l) {
		printk("[ERROR] : vfs_read() filed ret : %d\n",ret);
		kfree(dp);
		filp_close(filp, current->files);
		return -1;
	}

	filp_close(filp, current->files);

	set_fs(fs);
	num = parse_text(dp, l);

	if(!num) {
		printk("[ERROR]:Nothing to parse\n");
		kfree(dp);
		return -1;
	}

	printk("[INFO] : Loading Tuning Value's Count : %d", num);

	kfree(dp);
	return num;
}

static ssize_t mDNIeTuning_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	int ret = 0;
	ret = sprintf(buf,"Tunned File Name : %s\n",tuning_filename);

	return ret;
}

static ssize_t mDNIeTuning_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	char *pt;
	char a;
	unsigned long tunning_mode=0;

	a = *buf;

	if (a=='1') {
		tuning_enable = 1;
		printk("%s:Tuning_enable\n",__func__);
	} else if(a=='0') {
		tuning_enable = 0;
		printk("%s:Tuning_disable\n",__func__);
	} else {
		memset(tuning_filename,0,sizeof(tuning_filename));
		sprintf(tuning_filename,"%s%s",TUNING_FILE_PATH,buf);

		pt = tuning_filename;
		while(*pt) {
			if(*pt =='\r'|| *pt =='\n')
			{
				*pt = 0;
				break;
			}
			pt++;
		}
		printk("%s:%s\n",__func__,tuning_filename);
		if (load_tuning_data(tuning_filename) <= 0) {
			printk("[ERROR]:load_tunig_data() failed\n");
			return size;
		}

		if (tuning_enable && mDNIe_data[0]) {
			printk("========================mDNIe!!!!!!!\n");
			s6d78a0x_set_mdnie_tuning(fbi_global);
		}
	}
	return size;
}
#endif	/* ENABLE_MDNIE_TUNING */

static void set_mDNIe_Mode(struct mdnie_config *mDNIeCfg)
{
	int value;

	if (!isReadyTo_mDNIe) {
		printk("%s, warnning : is not ready to mDNIE\n", __func__);
		return;
	}

#ifdef ENABLE_MDNIE_TUNING
	if (tuning_enable && mDNIe_data[0]) {
		s6d78a0x_set_mdnie_tuning(fbi_global);
		return;
	}
#endif

	printk("%s:[mDNIe] negative=%d\n", __func__, mDNIe_cfg.negative);
	msleep(100);
	if (mDNIe_cfg.negative) {
		printk("%s : apply negative color\n", __func__);
		s6d78a0x_set_mdnie(fbi_global, SCENARIO_MAX);
		return;
	}

	switch(mDNIeCfg->scenario) {
	case UI_MODE:
	case GALLERY_MODE:
		value = mDNIeCfg->scenario;
		break;

	case VIDEO_MODE:
		if(mDNIeCfg->outdoor == OUTDOOR_ON) {
			value = SCENARIO_MAX + 1;
		} else {
			value = mDNIeCfg->scenario;
		}
		break;

	case VIDEO_WARM_MODE:
		if(mDNIeCfg->outdoor == OUTDOOR_ON) {
			value = SCENARIO_MAX + 2;
		} else {
			value = mDNIeCfg->scenario;
		}
		break;

	case VIDEO_COLD_MODE:
		if(mDNIeCfg->outdoor == OUTDOOR_ON) {
			value = SCENARIO_MAX + 3;
		} else {
			value = mDNIeCfg->scenario;
		}
		break;

	case CAMERA_MODE:
		if(mDNIeCfg->outdoor == OUTDOOR_ON) {
			value = SCENARIO_MAX + 4;
		} else {
			value = mDNIeCfg->scenario;
		}
		break;

	default:
		value = UI_MODE;
		break;
	};

	printk("%s:[mDNIe] value=%d\n", __func__, value);

	if (mDNIe_cfg.negative && value == UI_MODE)
		return;
	printk("%s:[mDNIe] value=%d ++ \n", __func__, value);
	s6d78a0x_set_mdnie(fbi_global, value);
	return;
}

static ssize_t mDNIeScenario_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	int ret;
	ret = sprintf(buf,"mDNIeScenario_show : %d\n", mDNIe_cfg.scenario);
	return ret;
}

static ssize_t mDNIeScenario_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;
	int ret;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);
	printk("%s:value=%d\n", __func__, value);

	switch(value) {
	case UI_MODE:
	case VIDEO_MODE:
	case VIDEO_WARM_MODE:
	case VIDEO_COLD_MODE:
	case CAMERA_MODE:
	case GALLERY_MODE:
		break;
	default:
		value = UI_MODE;
		break;
	};

	mDNIe_cfg.scenario = value;
	set_mDNIe_Mode(&mDNIe_cfg);
	return size;
}

static ssize_t mDNIeOutdoor_show(struct device *dev,
		struct device_attribute *attr, char *buf)

{
	int ret;
	ret = sprintf(buf,"mDNIeOutdoor_show : %d\n", mDNIe_cfg.outdoor);

	return ret;
}

static ssize_t mDNIeOutdoor_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;
	int ret;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);
	printk("%s:value=%d\n", __func__, value);
	mDNIe_cfg.outdoor = value;
	set_mDNIe_Mode(&mDNIe_cfg);
	return size;
}

static ssize_t mDNIeNegative_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf,"mDNIeNegative_show : %d\n", mDNIe_cfg.negative);
	return ret;
}

static ssize_t mDNIeNegative_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned int value;
	int ret;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);
	printk("%s:value=%d\n", __func__, value);

	if(value == 1) {
		mDNIe_cfg.negative = 1;
	} else {
		mDNIe_cfg.negative = 0;
	}

	set_mDNIe_Mode(&mDNIe_cfg);
	return size;
}
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

static int s6d78a0x_init(struct pxa168fb_info *fbi)
{
	int board_id = get_board_id();
	pxa168_dsi_cmd_array_tx(fbi, s6d78a0x_video_display_enable_access_register,
			ARRAY_SIZE(s6d78a0x_video_display_enable_access_register));

	printk("%s, init s6d7aa0x\n", __func__);
	pxa168_dsi_cmd_array_tx(fbi, s6d78a0x_video_display_init_cmds,
			ARRAY_SIZE(s6d78a0x_video_display_init_cmds));
#if 0
	if (board_id == BOARD_ID_REV00) {
		printk("%s, init s6d78a0x\n", __func__);
		pxa168_dsi_cmd_array_tx(fbi, s6d78a0x_video_display_init_cmds,
				ARRAY_SIZE(s6d78a0x_video_display_init_cmds));
	} else {
		printk("%s, init s6d78a0x_es2\n", __func__);
		pxa168_dsi_cmd_array_tx(fbi, s6d78a0x_es2_video_display_init_cmds,
				ARRAY_SIZE(s6d78a0x_es2_video_display_init_cmds));
	}
#endif
	pxa168_dsi_cmd_array_tx(fbi, s6d78a0x_video_display_disable_access_register,
			ARRAY_SIZE(s6d78a0x_video_display_disable_access_register));

#ifdef  CONFIG_LDI_SUPPORT_MDNIE
	isReadyTo_mDNIe = 1;
	set_mDNIe_Mode(&mDNIe_cfg);
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

	return 0;
}

static int s6d78a0x_enable(struct pxa168fb_info *fbi)
{
	printk("%s\n", __func__);
	pxa168_dsi_cmd_array_tx(fbi, s6d78a0x_video_display_on_cmds,
			ARRAY_SIZE(s6d78a0x_video_display_on_cmds));

	return 0;
}

static int s6d78a0x_disable(struct pxa168fb_info *fbi)
{
	printk("%s\n", __func__);
	isReadyTo_mDNIe = 0;
	pxa168_dsi_cmd_array_tx(fbi, s6d78a0x_video_display_off_cmds,
			ARRAY_SIZE(s6d78a0x_video_display_off_cmds));
	return 0;
}


static ssize_t lcd_panelName_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "S6D78A0X");
}

static ssize_t lcd_MTP_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int panel_id = get_panel_id();

	((unsigned char*)buf)[0] = (panel_id >> 16) & 0xFF;
	((unsigned char*)buf)[1] = (panel_id >> 8) & 0xFF;
	((unsigned char*)buf)[2] = panel_id & 0xFF;

	printk("ldi mtpdata: %x %x %x\n", buf[0], buf[1], buf[2]);

	return 3;
}

static ssize_t lcd_type_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	int panel_id = get_panel_id();

	return sprintf(buf, "INH_%x%x%x",
			(panel_id >> 16) & 0xFF,
			(panel_id >> 8) & 0xFF,
			panel_id  & 0xFF);
}

static DEVICE_ATTR(lcd_MTP, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_MTP_show, NULL);
static DEVICE_ATTR(lcd_panelName, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_panelName_show, NULL);
static DEVICE_ATTR(lcd_type, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_type_show, NULL);
#ifdef CONFIG_LDI_SUPPORT_MDNIE
#ifdef ENABLE_MDNIE_TUNING
static DEVICE_ATTR(tuning, 0664, mDNIeTuning_show, mDNIeTuning_store);
#endif	/* ENABLE_MDNIE_TUNING */
static DEVICE_ATTR(scenario, 0664, mDNIeScenario_show, mDNIeScenario_store);
static DEVICE_ATTR(outdoor, 0664, mDNIeOutdoor_show, mDNIeOutdoor_store);
static DEVICE_ATTR(negative, 0664, mDNIeNegative_show, mDNIeNegative_store);
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

static int s6d78a0x_probe(struct pxa168fb_info *fbi)
{
	struct device *dev_t;
	fbi_global = fbi;

	printk("%s, probe s6d78a0x\n", __func__);
	dev_t = device_create( lcd_class, NULL, 0, "%s", "panel");

	if (device_create_file(dev_t, &dev_attr_lcd_panelName) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_panelName.attr.name);
	if (device_create_file(dev_t, &dev_attr_lcd_MTP) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_MTP.attr.name);
	if (device_create_file(dev_t, &dev_attr_lcd_type) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_type.attr.name);
#ifdef  ENABLE_MDNIE_TUNING
	if (device_create_file(dev_t, &dev_attr_tuning) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_tuning.attr.name);
#endif	/* ENABLE_MDNIE_TUNING */

#ifdef CONFIG_LDI_SUPPORT_MDNIE
	lcd_mDNIe_class = class_create(THIS_MODULE, "mdnie");

	if (IS_ERR(lcd_mDNIe_class)) {
		printk("Failed to create mdnie!\n");
		return PTR_ERR( lcd_mDNIe_class );
	}

	dev_t = device_create( lcd_mDNIe_class, NULL, 0, "%s", "mdnie");

	if (device_create_file(dev_t, &dev_attr_scenario) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_scenario.attr.name);
	if (device_create_file(dev_t, &dev_attr_outdoor) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_outdoor.attr.name);
	if (device_create_file(dev_t, &dev_attr_negative) < 0)
		pr_err("Failed to create device file(%s)!\n", dev_attr_negative.attr.name);
#endif	/* CONFIG_LDI_SUPPORT_MDNIE */

	return 0;
}

struct pxa168fb_mipi_lcd_driver s6d78a0x_lcd_driver = {
	.probe		= s6d78a0x_probe,
	.init		= s6d78a0x_init,
	.disable	= s6d78a0x_disable,
	.enable		= s6d78a0x_enable,
};
