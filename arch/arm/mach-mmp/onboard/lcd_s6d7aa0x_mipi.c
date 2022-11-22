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
#include <linux/backlight.h>

#if defined(CONFIG_CPU_PXA1088)
#include <mach/mfp-pxa1088-delos.h>
#elif defined(CONFIG_CPU_PXA988)
#include <mach/mfp-pxa988-aruba.h>
#endif
#include "../common.h"
#include "s6d7aa0x_param.h"

enum CABC {
	CABC_OFF,
	CABC_ON,
	CABC_MAX,
};
struct s6d7aa0x_info {
	enum CABC cabc;

	struct class *mdnie;
	struct device *dev_mdnie;
	struct device *dev_bd;

	struct backlight_device *bd;
	struct lcd_device *lcd;
	struct mutex lock;
	struct mutex pwr_lock;
	struct mutex lvds_clk_switch_lock;

	unsigned int auto_brightness;
	unsigned int vee_strenght;
	unsigned int prevee_strenght;
	unsigned int first_count;
	unsigned int lcd_panel;
	unsigned int current_bl;
};

struct s6d7aa0x_info *global_s6d7info;
struct pxa168fb_info *fbi_global;
extern int get_panel_id(void);

#ifdef CONFIG_LDI_SUPPORT_MDNIE
static struct class *lcd_mDNIe_class;
static int isReadyTo_mDNIe = 1;
static struct mdnie_config mDNIe_cfg;
static void s6d7aa0x_set_mdnie(struct pxa168fb_info *fbi, int scenario);
#ifdef ENABLE_MDNIE_TUNING
#define TUNING_FILE_PATH "/sdcard/mdnie/"
#define MAX_TUNING_FILE_NAME 100
#define NR_MDNIE_DATA 119
#define NR_MDNIE_DATA_E7	8
#define NR_MDNIE_DATA_E8	17
#define NR_MDNIE_DATA_E9	25
#define NR_MDNIE_DATA_EA	25
#define NR_MDNIE_DATA_EB	25
#define NR_MDNIE_DATA_EC	19
static int tuning_enable;
static char tuning_filename[MAX_TUNING_FILE_NAME];

static unsigned char mDNIe_data[NR_MDNIE_DATA];
static unsigned char mDNIe_data_E7[NR_MDNIE_DATA_E7];
static unsigned char mDNIe_data_E8[NR_MDNIE_DATA_E8];
static unsigned char mDNIe_data_E9[NR_MDNIE_DATA_E9];
static unsigned char mDNIe_data_EA[NR_MDNIE_DATA_EA];
static unsigned char mDNIe_data_EB[NR_MDNIE_DATA_EB];
static unsigned char mDNIe_data_EC[NR_MDNIE_DATA_EC];

static struct dsi_cmd_desc s6d7aa0x_video_display_mDNIe_cmds[] = {
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_data_E7), mDNIe_data_E7},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_data_E8), mDNIe_data_E8},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_data_E9), mDNIe_data_E9},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_data_EA), mDNIe_data_EA},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_data_EB), mDNIe_data_EB},
	{DSI_DI_DCS_LWRITE, 0, 0, sizeof(mDNIe_data_EC), mDNIe_data_EC},
};

static int degas_parse_array(char *set_mdnie_data);
static void s6d7aa0x_set_mdnie_tuning(struct pxa168fb_info *fbi);
#endif /* ENABLE_MDNIE_TUNING */
#endif /* CONFIG_LDI_SUPPORT_MDNIE */

#define BOARD_ID_REV00 (0x0)
#define BOARD_ID_REV01 (0x1)

#ifdef CONFIG_LDI_SUPPORT_MDNIE

static void s6d7aa0x_set_mdnie(struct pxa168fb_info *fbi, int scenario)
{
	int file_num = 0;

	if (scenario > SCENARIO_MAX) {
		printk(KERN_INFO "%s, exceeded max scenario(val:%d)\n",
		       __func__, scenario);
		return;
	}

	pxa168_dsi_cmd_array_tx(fbi,
			s6d7aa0x_video_display_enable_access_register,
			ARRAY_SIZE(s6d7aa0x_video_display_enable_access_register));
	/*
		printk(KERN_INFO "scenario : %d \n", scenario);
		printk(KERN_INFO "%s, 1 transmit data[0]: 0x%02X, 0x%02X\n",__func__,
		s6d7aa0x_video_display_mDNIe_scenario_cmds[scenario].data[0],
		s6d7aa0x_video_display_mDNIe_scenario_cmds[scenario].data[1]);
	 */
	file_num = degas_parse_array(s6d7aa0x_video_display_mDNIe_scenario_cmds[scenario].data);
	/*
		printk(KERN_INFO "%s,  transmit data[0]: 0x%02X, 0x%02X\n",__func__,
		s6d7aa0x_video_display_mDNIe_scenario_cmds[scenario].data[0],
		s6d7aa0x_video_display_mDNIe_scenario_cmds[scenario].data[1]);
	 */
	if (file_num != NR_MDNIE_DATA)
		printk(KERN_INFO "[ERROR] : degas_parse_array() failed! : %d\n",
				file_num);

	pxa168_dsi_cmd_array_tx(fbi, s6d7aa0x_video_display_mDNIe_cmds,
			ARRAY_SIZE(s6d7aa0x_video_display_mDNIe_cmds));
}

#ifdef ENABLE_MDNIE_TUNING
static void s6d7aa0x_set_mdnie_tuning(struct pxa168fb_info *fbi)
{
	printk(KERN_INFO "%s, set mDNIe tuning data\n", __func__);
	pxa168_dsi_cmd_array_tx(fbi,
			s6d7aa0x_video_display_enable_access_register,
			ARRAY_SIZE
			(s6d7aa0x_video_display_enable_access_register));

	pxa168_dsi_cmd_array_tx(fbi, s6d7aa0x_video_display_mDNIe_cmds,
			ARRAY_SIZE(s6d7aa0x_video_display_mDNIe_cmds));

}
#endif /* ENABLE_MDNIE_TUNING */
#endif /* CONFIG_LDI_SUPPORT_MDNIE */

#ifdef CONFIG_LDI_SUPPORT_MDNIE
#ifdef ENABLE_MDNIE_TUNING
static int parse_text(char *src, int len)
{
	int i;
	int data = 0, value = 0, count = 0, comment = 0;
	char *cur_position;

	cur_position = src;
	for (i = 0; i < len; i++, cur_position++) {
		char a = *cur_position;
		switch (a) {
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
			if (comment > 1)
				break;
			if (data == 0 && a == '0')
				data = 1;
			else if (data == 2) {
				data = 3;
				value = (a - '0') * 16;
			} else if (data == 3) {
				value += (a - '0');
				mDNIe_data[count] = value;
				printk(KERN_INFO "Tuning value[%d] = 0x%02X\n",
				       count, value);
				count++;
				data = 0;
			};
			break;
		case 'a'...'f':
		case 'A'...'F':
			if (comment > 1)
				break;
			if (data == 2) {
				data = 3;
				if (a < 'a')
					value = (a - 'A' + 10) * 16;
				else
					value = (a - 'a' + 10) * 16;
			} else if (data == 3) {
				if (a < 'a')
					value += (a - 'A' + 10);
				else
					value += (a - 'a' + 10);
				mDNIe_data[count] = value;
				printk(KERN_INFO "Tuning value[%d]=0x%02X\n",
				       count, value);
				count++;
				data = 0;
			};
			break;
		case 'x':
		case 'X':
			if (data == 1)
				data = 2;
			break;
		default:
			if (comment == 1)
				comment = 0;
			data = 0;
			break;
		}
	}

	return count;
}

static int degas_parse_array(char *set_mdnie_data)
{
	int i, k = 0;
	for (i = 0 ; i < NR_MDNIE_DATA_E7 ; i++)
		mDNIe_data_E7[i] = set_mdnie_data[k++];

	for (i = 0 ; i < NR_MDNIE_DATA_E8 ; i++)
		mDNIe_data_E8[i] = set_mdnie_data[k++];

	for (i = 0 ; i < NR_MDNIE_DATA_E9 ; i++)
		mDNIe_data_E9[i] = set_mdnie_data[k++];

	for (i = 0 ; i < NR_MDNIE_DATA_EA ; i++)
		mDNIe_data_EA[i] = set_mdnie_data[k++];

	for (i = 0 ; i < NR_MDNIE_DATA_EB ; i++)
		mDNIe_data_EB[i] = set_mdnie_data[k++];

	for (i = 0 ; i < NR_MDNIE_DATA_EC ; i++)
		mDNIe_data_EC[i] = set_mdnie_data[k++];

	printk(KERN_INFO "%s, is done! ,param count : %d", __func__, k);
#if 0
	/* print test */
	for (i = 0 ; i < NR_MDNIE_DATA_E7 ; i++)
		printk(KERN_INFO "mDNIe_data_E7[%d] = 0x%02X\n", i, mDNIe_data_E7[i]);

	for (i = 0 ; i < NR_MDNIE_DATA_E8 ; i++)
		printk(KERN_INFO "mDNIe_data_E8[%d] = 0x%02X\n", i, mDNIe_data_E8[i]);

	for (i = 0 ; i < NR_MDNIE_DATA_E9 ; i++)
		printk(KERN_INFO "mDNIe_data_E9[%d] = 0x%02X\n", i, mDNIe_data_E9[i]);

	for (i = 0 ; i < NR_MDNIE_DATA_EA ; i++)
		printk(KERN_INFO "mDNIe_data_EA[%d] = 0x%02X\n", i, mDNIe_data_EA[i]);

	for (i = 0 ; i < NR_MDNIE_DATA_EB ; i++)
		printk(KERN_INFO "mDNIe_data_EB[%d] = 0x%02X\n", i, mDNIe_data_EB[i]);

	for (i = 0 ; i < NR_MDNIE_DATA_EC ; i++)
		printk(KERN_INFO "mDNIe_data_EC[%d] = 0x%02X\n", i, mDNIe_data_EC[i]);
#endif
	return k;
}

static int load_tuning_data(char *filename)
{
	struct file *filp;
	char *dp;
	long l;
	loff_t pos;
	int ret, num, file_num;
	mm_segment_t fs;

	printk(KERN_INFO "[INFO]:%s called loading file name : [%s]\n",
	       __func__, filename);

	fs = get_fs();
	set_fs(get_ds());

	filp = filp_open(filename, O_RDONLY, 0);
	if (IS_ERR(filp)) {
		printk(KERN_ERR "[ERROR]:File open failed %d\n",
		       (int)IS_ERR(filp));
		return -1;
	}

	l = filp->f_path.dentry->d_inode->i_size;
	printk(KERN_INFO "[INFO]: Loading File Size : %ld(bytes)", l);

	dp = kmalloc(l + 10, GFP_KERNEL);
	if (dp == NULL) {
		printk(KERN_ERR
		       "Can't not alloc memory for tuning file load\n");
		filp_close(filp, current->files);
		return -1;
	}
	pos = 0;
	memset(dp, 0, l);
	printk(KERN_INFO "[INFO] : before vfs_read()\n");
	ret = vfs_read(filp, (char __user *)dp, l, &pos);
	printk(KERN_INFO "[INFO] : after vfs_read()\n");

	if (ret != l) {
		printk(KERN_INFO "[ERROR] : vfs_read() filed ret : %d\n", ret);
		kfree(dp);
		filp_close(filp, current->files);
		return -1;
	}

	filp_close(filp, current->files);

	set_fs(fs);
	num = parse_text(dp, l);

	file_num = degas_parse_array(mDNIe_data);
	if (file_num != NR_MDNIE_DATA)
		printk(KERN_INFO "[ERROR] : degas_parse_array() failed! : %d\n",
				file_num);

	if (!num) {
		printk(KERN_INFO "[ERROR]:Nothing to parse\n");
		kfree(dp);
		return -1;
	}

	printk(KERN_INFO "[INFO] : Loading Tuning Value's Count : %d", num);

	kfree(dp);
	return num;
}

static ssize_t mDNIeTuning_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "Tunned File Name : %s\n", tuning_filename);

	return ret;
}

static ssize_t mDNIeTuning_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t size)
{
	char *pt;
	char a = *buf;

	if (a == '1') {
		tuning_enable = 1;
		printk(KERN_INFO "%s:Tuning_enable\n", __func__);
	} else if (a == '0') {
		tuning_enable = 0;
		printk(KERN_INFO "%s:Tuning_disable\n", __func__);
	} else {
		memset(tuning_filename, 0, sizeof(tuning_filename));
		sprintf(tuning_filename, "%s%s", TUNING_FILE_PATH, buf);

		pt = tuning_filename;
		while (*pt) {
			if (*pt == '\r' || *pt == '\n') {
				*pt = 0;
				break;
			}
			pt++;
		}
		printk(KERN_INFO "%s:%s\n", __func__, tuning_filename);
		if (load_tuning_data(tuning_filename) <= 0) {
			printk(KERN_ERR "[ERROR]:load_tunig_data() failed\n");
			return size;
		}

		if (tuning_enable && mDNIe_data[0]) {
			printk(KERN_INFO
			       "========================mDNIe!!!!!!!\n");
			s6d7aa0x_set_mdnie_tuning(fbi_global);
		}
	}
	return size;
}
#endif /* ENABLE_MDNIE_TUNING */

static void set_mDNIe_Mode(struct mdnie_config *mDNIeCfg)
{
	int value;

	if (!isReadyTo_mDNIe) {
		printk(KERN_INFO "%s, is not ready to mDNIE\n", __func__);
		return;
	}
#ifdef ENABLE_MDNIE_TUNING
	if (tuning_enable && mDNIe_data[0]) {
		s6d7aa0x_set_mdnie_tuning(fbi_global);
		return;
	}
#endif

	printk(KERN_INFO "%s:[mDNIe] negative=%d\n", __func__,
	       mDNIe_cfg.negative);
	msleep(100);
	if (mDNIe_cfg.negative) {
		printk(KERN_INFO "%s : apply negative color\n", __func__);
		s6d7aa0x_set_mdnie(fbi_global, SCENARIO_MAX);
		return;
	}

	switch (mDNIeCfg->scenario) {
		case UI_MODE:
			value = mDNIeCfg->scenario;
			break;

		case VIDEO_MODE:
			if (mDNIeCfg->outdoor == OUTDOOR_ON)
				value = SCENARIO_MAX + 1;
			else
				value = mDNIeCfg->scenario;
			break;

		case VIDEO_WARM_MODE:
			if (mDNIeCfg->outdoor == OUTDOOR_ON)
				value = SCENARIO_MAX + 2;
			else
				value = mDNIeCfg->scenario;

			break;

		case VIDEO_COLD_MODE:
			if (mDNIeCfg->outdoor == OUTDOOR_ON)
				value = SCENARIO_MAX + 3;
			else
				value = mDNIeCfg->scenario;

			break;

		case CAMERA_MODE:
			if (mDNIeCfg->outdoor == OUTDOOR_ON)
				value = SCENARIO_MAX + 4;
			else
				value = mDNIeCfg->scenario;

			break;

		case GALLERY_MODE:
			value = mDNIeCfg->scenario;
			break;

		case VT_MODE:
			value = mDNIeCfg->scenario;
			break;

		case BROWSER_MODE:
			value = mDNIeCfg->scenario;
			break;

		case EBOOK_MODE:
			value = mDNIeCfg->scenario;
			break;

		case EMAIL_MODE:
			value = mDNIeCfg->scenario;
			break;

		default:
			value = UI_MODE;
			break;
	};

	if (mDNIe_cfg.negative && value == UI_MODE)
		return;
	printk(KERN_INFO "%s:[mDNIe] value=%d ++ \n", __func__, value);
	s6d7aa0x_set_mdnie(fbi_global, value);
	return;
}

static ssize_t mDNIeScenario_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret;
	ret = sprintf(buf, "mDNIeScenario_show : %d\n", mDNIe_cfg.scenario);
	return ret;
}

static ssize_t mDNIeScenario_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	unsigned int value;
	int ret;

	ret = kstrtoul(buf, 0, (unsigned long *)&value);
	printk(KERN_INFO "%s:(1)value=%d\n", __func__, value);

	switch (value) {
	case UI_MODE:
		value = UI_MODE;
		break;
	case VIDEO_MODE:
		value = VIDEO_MODE;
		break;
	case CAMERA_MODE:
		value = CAMERA_MODE;
		break;
	case GALLERY_MODE:
		value = GALLERY_MODE;
		break;
	case VT_MODE:
		value = VT_MODE;
		break;
	case BROWSER_MODE:
		value = BROWSER_MODE;
		break;
	case EBOOK_MODE:
		value = EBOOK_MODE;
		break;
	case EMAIL_MODE:
		value = EMAIL_MODE;
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
	ret = sprintf(buf, "mDNIeOutdoor_show : %d\n", mDNIe_cfg.outdoor);

	return ret;
}

static ssize_t mDNIeOutdoor_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	unsigned int value;
	int ret;

	ret = kstrtoul(buf, 0, (unsigned long *)&value);
	printk(KERN_INFO "%s:value=%d\n", __func__, value);
	mDNIe_cfg.outdoor = value;
	set_mDNIe_Mode(&mDNIe_cfg);
	return size;
}

static ssize_t mDNIeNegative_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	int ret = 0;
	ret = sprintf(buf, "mDNIeNegative_show : %d\n", mDNIe_cfg.negative);
	return ret;
}

static ssize_t mDNIeNegative_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t size)
{
	unsigned int value;
	int ret;

	ret = kstrtoul(buf, 0, (unsigned long *)&value);
	printk(KERN_INFO "%s:value=%d\n", __func__, value);

	if (value == 1)
		mDNIe_cfg.negative = 1;
	else
		mDNIe_cfg.negative = 0;

	set_mDNIe_Mode(&mDNIe_cfg);
	return size;
}
#endif /* CONFIG_LDI_SUPPORT_MDNIE */

#define PLATFORM_MAX_BRIGHTNESS	255
#define PLATFORM_MID_BRIGHTNESS	143
#define PLATFORM_LOW_BRIGHTNESS	5

static int adjust_brightness_range(int low, int mid, int max, int brightness)
{
	int ret = 0;

	if (brightness > PLATFORM_MAX_BRIGHTNESS)
		ret = PLATFORM_MAX_BRIGHTNESS;
	if (brightness >= PLATFORM_MID_BRIGHTNESS) {
		ret = (brightness - PLATFORM_MID_BRIGHTNESS) * (max - mid) /
			(PLATFORM_MAX_BRIGHTNESS - PLATFORM_MID_BRIGHTNESS) + mid;
	} else if (brightness >= PLATFORM_LOW_BRIGHTNESS) {
		ret = (brightness - PLATFORM_LOW_BRIGHTNESS) * (mid - low) /
			(PLATFORM_MID_BRIGHTNESS - PLATFORM_LOW_BRIGHTNESS) + low;
	} else
		ret = 0;

	return ret;
}
static int degas_set_brightness(struct s6d7aa0x_info *s6d7info)
{
	int brightness = adjust_brightness_range(5, 75, 160,
			s6d7info->bd->props.brightness);

/*	//read 52h
	struct dsi_buf *dbuf;
	static char pkt_size_mtp_cmd[] = {1};
	static char read_mtp[] = {0x52};
	static struct dsi_cmd_desc test_cmds[] = {
		{DSI_DI_SET_MAX_PKT_SIZE, HS_MODE, 0, sizeof(pkt_size_mtp_cmd), pkt_size_mtp_cmd},
		{DSI_DI_DCS_READ, HS_MODE, 0, sizeof(read_mtp), read_mtp},
	};
	dbuf = kzalloc(sizeof(struct dsi_buf), GFP_KERNEL);
	if (!dbuf) {
		printk("%s: can't alloc dsi rx buffer\n", __func__);
	}
*/

	s6d7aa0x_backlight_brightness_cmds[0].data[1] = (u8) brightness;
	printk(KERN_INFO "%s, brightness = %d\n", __func__, brightness);

	pxa168_dsi_cmd_array_tx(fbi_global, s6d7aa0x_backlight_brightness_cmds,
				ARRAY_SIZE(s6d7aa0x_backlight_brightness_cmds));

/*	//read 52h
	pxa168_dsi_cmd_array_rx(fbi_global, dbuf, test_cmds, ARRAY_SIZE(test_cmds));
	printk(KERN_INFO "%s, Read 52h : %d\n", __func__, dbuf->data[0]);
*/
	return 0;
}

static int degas_backlight_update_status(struct backlight_device *bl)
{
	struct s6d7aa0x_info *s6d7info = global_s6d7info;

	s6d7info->bd->props.brightness = bl->props.brightness;

	degas_set_brightness(s6d7info);
	return 0;
}

static int degas_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static const struct backlight_ops degas_backlight_ops = {
	.get_brightness = degas_get_brightness,
	.update_status = degas_backlight_update_status,
};

static int s6d7aa0x_init(struct pxa168fb_info *fbi)
{
	printk(KERN_INFO "%s\n", __func__);
	pxa168_dsi_cmd_array_tx(fbi, s6d7aa0x_power_on_init,
				ARRAY_SIZE(s6d7aa0x_power_on_init));

#ifdef CONFIG_LDI_SUPPORT_MDNIE
	isReadyTo_mDNIe = 1;
	set_mDNIe_Mode(&mDNIe_cfg);
#endif /* CONFIG_LDI_SUPPORT_MDNIE */

	return 0;
}

static int s6d7aa0x_enable(struct pxa168fb_info *fbi)
{
	printk(KERN_INFO "%s\n", __func__);
	pxa168_dsi_cmd_array_tx(fbi, s6d7aa0x_backlight_brightness_cmds,
				ARRAY_SIZE(s6d7aa0x_backlight_brightness_cmds));
	return 0;
}

static int s6d7aa0x_disable(struct pxa168fb_info *fbi)
{
	printk(KERN_INFO "%s\n", __func__);
#ifdef CONFIG_LDI_SUPPORT_MDNIE
	isReadyTo_mDNIe = 0;
#endif

	pxa168_dsi_cmd_array_tx(fbi, s6d7aa0x_power_off_cmds,
				ARRAY_SIZE(s6d7aa0x_power_off_cmds));
	return 0;
}

static ssize_t lcd_panelName_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "S6D7AA0X");
}

static ssize_t lcd_MTP_show(struct device *dev,
			    struct device_attribute *attr, char *buf)
{
	int panel_id = get_panel_id();

	((unsigned char *)buf)[0] = (panel_id >> 16) & 0xFF;
	((unsigned char *)buf)[1] = (panel_id >> 8) & 0xFF;
	((unsigned char *)buf)[2] = panel_id & 0xFF;

	printk(KERN_INFO "ldi mtpdata: %x %x %x\n", buf[0], buf[1], buf[2]);

	return 3;
}

static ssize_t lcd_type_show(struct device *dev,
			     struct device_attribute *attr, char *buf)
{
	int panel_id = get_panel_id();

	return sprintf(buf, "INH_%x%x%x",
		       (panel_id >> 16) & 0xFF,
		       (panel_id >> 8) & 0xFF, panel_id & 0xFF);
}

static DEVICE_ATTR(lcd_MTP, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH,
		   lcd_MTP_show, NULL);
static DEVICE_ATTR(lcd_panelName, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH,
		   lcd_panelName_show, NULL);
static DEVICE_ATTR(lcd_type, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH,
		   lcd_type_show, NULL);
#ifdef CONFIG_LDI_SUPPORT_MDNIE
#ifdef ENABLE_MDNIE_TUNING
static DEVICE_ATTR(tuning, 0664, mDNIeTuning_show, mDNIeTuning_store);
#endif /* ENABLE_MDNIE_TUNING */
static DEVICE_ATTR(scenario, 0664, mDNIeScenario_show, mDNIeScenario_store);
static DEVICE_ATTR(outdoor, 0664, mDNIeOutdoor_show, mDNIeOutdoor_store);
static DEVICE_ATTR(negative, 0664, mDNIeNegative_show, mDNIeNegative_store);
#endif /* CONFIG_LDI_SUPPORT_MDNIE */

static int s6d7aa0x_probe(struct pxa168fb_info *fbi)
{
	struct device *dev_t;
	struct s6d7aa0x_info *s6d7info;
	int ret = 0;

	fbi_global = fbi;

	printk(KERN_INFO "%s, probe s6d7aa0x\n", __func__);

	/* for s6d7aa0x_info alloc */
	s6d7info = kzalloc(sizeof(struct s6d7aa0x_info), GFP_KERNEL);

	if (!s6d7info) {
		pr_err("failed to allocate s6d7info\n");
		ret = -ENOMEM;
		goto error1;
	}

	/* for lcd class */
	dev_t = device_create(lcd_class, NULL, 0, "%s", "panel");

	if (device_create_file(dev_t, &dev_attr_lcd_panelName) < 0)
		printk(KERN_INFO "Failed to create device file(%s)!\n",
		       dev_attr_lcd_panelName.attr.name);
	if (device_create_file(dev_t, &dev_attr_lcd_MTP) < 0)
		printk(KERN_INFO "Failed to create device file(%s)!\n",
		       dev_attr_lcd_MTP.attr.name);
	if (device_create_file(dev_t, &dev_attr_lcd_type) < 0)
		printk(KERN_INFO "Failed to create device file(%s)!\n",
		       dev_attr_lcd_type.attr.name);
#ifdef ENABLE_MDNIE_TUNING
	if (device_create_file(dev_t, &dev_attr_tuning) < 0)
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_tuning.attr.name);
#endif /* ENABLE_MDNIE_TUNING */

#ifdef CONFIG_LDI_SUPPORT_MDNIE
	lcd_mDNIe_class = class_create(THIS_MODULE, "mdnie");

	if (IS_ERR(lcd_mDNIe_class)) {
		printk(KERN_INFO "Failed to create mdnie!\n");
		return PTR_ERR(lcd_mDNIe_class);
	}

	dev_t = device_create(lcd_mDNIe_class, NULL, 0, "%s", "mdnie");

	if (device_create_file(dev_t, &dev_attr_scenario) < 0)
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_scenario.attr.name);
	if (device_create_file(dev_t, &dev_attr_outdoor) < 0)
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_outdoor.attr.name);
	if (device_create_file(dev_t, &dev_attr_negative) < 0)
		pr_err("Failed to create device file(%s)!\n",
		       dev_attr_negative.attr.name);
#endif /* CONFIG_LDI_SUPPORT_MDNIE */

	/* For Backlight */
	s6d7info->bd = backlight_device_register("panel", s6d7info->dev_bd,
						 s6d7info, &degas_backlight_ops,
						 NULL);

	s6d7info->bd->props.max_brightness = 255;
	s6d7info->bd->props.brightness = 150;
	s6d7info->bd->props.type = BACKLIGHT_RAW;

	global_s6d7info = s6d7info;
	return ret;

error1:
	kfree(s6d7info);
	return ret;
}

struct pxa168fb_mipi_lcd_driver s6d7aa0x_lcd_driver = {
	.probe = s6d7aa0x_probe,
	.init = s6d7aa0x_init,
	.disable = s6d7aa0x_disable,
	.enable = s6d7aa0x_enable,
};
