#include <linux/errno.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <linux/delay.h>
#include <linux/regulator/machine.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
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
#include <linux/timer.h>
#include <mach/mfp-pxa986-golden.h>
#include <mach/features.h>
#include "../common.h"
#include <linux/power_supply.h>
#include "lcd_s6e63m0_param.h"
#include "lcd_s6e63m0_smart.h"
#if defined(CONFIG_LCD_ESD_RECOVERY)
#include "esd_detect.h"
#endif	/* LCD_ESD_RECOVERY */


typedef struct S6e63m0_info {

	struct backlight_device		*bd;
	struct lcd_device		*lcd;
	struct device			*dev_bd;
	struct mutex			lock;
	unsigned int			current_gamma;
	unsigned int			bl_index;
	unsigned int			current_bl;
	unsigned int			acl_enable;
	unsigned int			cur_acl;
	const u8			    *elvss_offsets;
    u8				        elvss_pulse;
    bool                    panel_awake;
    enum elvss_brightness	elvss_brightness;
    u8                      **gamma_seq;
#ifdef SMART_DIMMING
    u8				        mtpData[MTP_DATA_LEN+1];
	struct str_smart_dim	smart;
#endif

};

struct pxa168fb_info *fbi_global = NULL;
struct S6e63m0_info *g_s6e63m0 = NULL;
static int get_gamma_table = 0;

extern int get_panel_id(void);
static void s6e63m0_set_acl(int update);
static void s6e63m0_set_elvss(void);
static void s6e63m0_smart_dimming(void);

#if defined(CONFIG_LCD_ESD_RECOVERY)
struct esd_det_info s6e63m0_esd_det;
static bool s6e63m0_is_active(void);
struct esd_det_info s6e63m0_esd_det = {
	.name = "s6e63m0",
	.mode = ESD_DET_INTERRUPT,
	.gpio = 19,
	.state = ESD_DET_NOT_INITIALIZED,
	.level = ESD_DET_HIGH,
	.is_active = s6e63m0_is_active,
	.is_normal = NULL,
	.recover = pxa168fb_reinit,
};
#endif	/* LCD_ESD_RECOVERY */

static int s6e63m0_init(struct pxa168fb_info *fbi)
{
    mutex_lock(&g_s6e63m0->lock);
    s6e63m0_smart_dimming();
	pxa168_dsi_cmd_array_tx(fbi, s6e63m0_l2_mtp_key_enable_seq,
		ARRAY_SIZE(s6e63m0_l2_mtp_key_enable_seq));
	pxa168_dsi_cmd_array_tx(fbi, s6e63m0_contention_error_table,
		ARRAY_SIZE(s6e63m0_contention_error_table));
	pxa168_dsi_cmd_array_tx(fbi, s6e63m0_panel_cond_set_seq,
		ARRAY_SIZE(s6e63m0_panel_cond_set_seq));
	pxa168_dsi_cmd_array_tx(fbi, s6e63m0_disp_cond_set_seq,
		ARRAY_SIZE(s6e63m0_disp_cond_set_seq));
	pxa168_dsi_cmd_array_tx(fbi, s6e63m0_etc_cond_set_seq,
		ARRAY_SIZE(s6e63m0_etc_cond_set_seq));
    s6e63m0_gamma_set();
    mutex_unlock(&g_s6e63m0->lock);

    printk("s6e63m0_init end\n");
	return 0;
}

static int s6e63m0_disable(struct pxa168fb_info *fbi)
{
	printk("%s\n", __func__);
#if defined(CONFIG_LCD_ESD_RECOVERY)
    esd_det_disable(&s6e63m0_esd_det);
#endif
    g_s6e63m0->panel_awake = false;
	/*
	 * Sperated lcd off routine multi tasking issue
	 * becasue of share of cpu0 after suspend.
	 */
	/*Display off*/
	pxa168_dsi_cmd_array_tx(fbi, s6e63m0_display_off_table,
			ARRAY_SIZE(s6e63m0_display_off_table));
#if 0
	msleep(HX8389B_DISP_OFF_DELAY);
	pxa168_dsi_cmd_array_tx(fbi, hx8389b_sleep_in_table,
			ARRAY_SIZE(hx8389b_sleep_in_table));
	msleep(HX8389B_SLEEP_IN_DELAY);
#endif
	return 0;
}

static int s6e63m0_enable(struct pxa168fb_info *fbi)
{
	printk("%s\n", __func__);

	pxa168_dsi_cmd_array_tx(fbi, s6e63m0_display_on_table,
			ARRAY_SIZE(s6e63m0_display_on_table));
	printk("display on end\n");

    g_s6e63m0->panel_awake = true;
#if defined(CONFIG_LCD_ESD_RECOVERY)
    esd_det_enable(&s6e63m0_esd_det);
#endif
	return 0;
}

#if defined(CONFIG_LCD_ESD_RECOVERY)
static bool s6e63m0_is_active(void)
{
	pr_info("s6e63m0 is %s\n",
			fbi_global->active ? "active" : "inactive");
	return fbi_global->active;
}
#endif

static ssize_t lcd_panelName_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "S6E63M0");
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

static ssize_t power_reduce_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	char temp[3];
	sprintf(temp, "%d\n", g_s6e63m0->acl_enable);
	strcpy(buf, temp);
	return strlen(buf);
}

static ssize_t power_reduce_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	int rc;

	rc = strict_strtoul(buf, (unsigned int) 0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else{
		printk("[%s] acl_set_store - %d, %d, %d\n", __func__, g_s6e63m0->acl_enable, value, g_s6e63m0->panel_awake);
		if (g_s6e63m0->acl_enable != value) {
			mutex_lock(&g_s6e63m0->lock);
			g_s6e63m0->acl_enable = value;
			if (g_s6e63m0->panel_awake)
				s6e63m0_set_acl(1);
			mutex_unlock(&g_s6e63m0->lock);
		}
		return size;
	}
}
static DEVICE_ATTR(power_reduce, 0664, power_reduce_show, power_reduce_store);

#ifdef SMART_DIMMING
static void get_mtpData(void)
{
    struct dsi_buf *dbuf;

	dbuf = kzalloc(sizeof(struct dsi_buf), GFP_KERNEL);
	if (!dbuf) {
		printk("%s: can't alloc dsi rx buffer\n", __func__);
		return 0;
	}

    pxa168_dsi_cmd_array_rx(fbi_global, dbuf, hx8369b_video_read_mtp_cmds, ARRAY_SIZE(hx8369b_video_read_mtp_cmds));
}

static void s6e63m0_init_smart_dimming_table_22()
{
	unsigned int i, j;
	u8 gamma_22[MTP_DATA_LEN] = {0,};

	for (i = 0; i < MAX_GAMMA_VALUE; i++) {
		calc_gamma_table_22(&g_s6e63m0->smart, candela_table[i], gamma_22);
		for (j = 0; j < MTP_DATA_LEN; j++)
			gamma_table_sm2[i][j+2] = (gamma_22[j]); // j+2 : addr, 0x02, .. for first value
	}
#if 0
	printk("++++++++++++++++++ !SMART DIMMING RESULT! +++++++++++++++++++\n");

	for (i = 0; i < MAX_GAMMA_VALUE; i++) {
		printk("SmartDimming Gamma Result=[%3d] : ",candela_table[i]);
		for (j = 0; j < GAMMA_PARAM_LEN; j++)
			printk("[0x%02x], ", gamma_table_sm2[i][j+3]);
		printk("\n");
	}
	printk("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
#endif
}
#endif

static int get_gamma_value_from_bl(int brightness)
{
	int backlightlevel;

	/* brightness setting from platform is from 0 to 255
	 * But in this driver, brightness is
	  only supported from 0 to 24 */

	switch (brightness) {
	case 0 ... 29:
		backlightlevel = GAMMA_30CD;
		break;
	case 30 ... 39:
		backlightlevel = GAMMA_40CD;
		break;
	case 40 ... 49:
		backlightlevel = GAMMA_50CD;
		break;
	case 50 ... 59:
		backlightlevel = GAMMA_60CD;
		break;
	case 60 ... 69:
		backlightlevel = GAMMA_70CD;
		break;
	case 70 ... 79:
		backlightlevel = GAMMA_80CD;
		break;
	case 80 ... 89:
		backlightlevel = GAMMA_90CD;
		break;
	case 90 ... 99:
		backlightlevel = GAMMA_100CD;
		break;
	case 100 ... 109:
		backlightlevel = GAMMA_110CD;
		break;
	case 110 ... 119:
		backlightlevel = GAMMA_120CD;
		break;
	case 120 ... 129:
		backlightlevel = GAMMA_130CD;
		break;
	case 130 ... 139:
		backlightlevel = GAMMA_140CD;
		break;
	case 140 ... 149:
		backlightlevel = GAMMA_150CD;
		break;
	case 150 ... 159:
		backlightlevel = GAMMA_160CD;
		break;
	case 160 ... 169:
		backlightlevel = GAMMA_170CD;
		break;
	case 170 ... 179:
		backlightlevel = GAMMA_180CD;
		break;
	case 180 ... 189:
		backlightlevel = GAMMA_190CD;
		break;
	case 190 ... 199:
		backlightlevel = GAMMA_200CD;
		break;
	case 200 ... 209:
		backlightlevel = GAMMA_210CD;
		break;
	case 210 ... 219:
		backlightlevel = GAMMA_220CD;
		break;
	case 220 ... 229:
		backlightlevel = GAMMA_230CD;
		break;
	case 230 ... 245:
		backlightlevel = GAMMA_240CD;
		break;
	case 246 ... 254:
		backlightlevel = GAMMA_250CD;
		break;
	case 255:
		backlightlevel = GAMMA_300CD;
		break;

	default:
		backlightlevel = DEFAULT_GAMMA_LEVEL;
		break;
        }
	return backlightlevel;

}

static int s6e63m0_set_gamma(struct S6e63m0_info *lcd)
{
	int ret = 0;

	memcpy(&gamma_set_update[0], g_s6e63m0->gamma_seq[lcd->bl_index], 23/*GAMMA_SET_MAX*/);

#if 0
	for (i = 0; i < 22 + 1; i++) {
		printk("GAMMA[%d] : 0x%x\n", i, gamma_set_update[i]);
	}
#endif

	return ret;
}

void s6e63m0_gamma_set(void)
{
	struct S6e63m0_info *ps6e63m0 = g_s6e63m0;
	int gamma = 0;

	ps6e63m0->bl_index = get_gamma_value_from_bl(ps6e63m0->current_bl);
	gamma = candela_table[ps6e63m0->bl_index];
	/*printk("bl_index =[%d], gamma=[%d]\n",ps6e63m0->bl_index,gamma);*/
	/*if (gamma != ps6e63m0->current_gamma) */{

		ps6e63m0->current_gamma = gamma;

		s6e63m0_set_gamma(ps6e63m0);
		s6e63m0_set_acl(0);
		s6e63m0_set_elvss();

		if (g_s6e63m0->acl_enable)
		{
			pxa168_dsi_cmd_array_tx(fbi_global, ARRAY_AND_SIZE(s6e63m0_brightness_set_acl_on));
		} else
		{
			pxa168_dsi_cmd_array_tx(fbi_global, ARRAY_AND_SIZE(s6e63m0_brightness_set_acl_off));
		}

		printk("Update Brightness: gamma=[%d]\n", gamma);
	}

}

static void s6e63m0_set_acl(int update)
{
	if (g_s6e63m0->acl_enable) {
		printk("[%s] current lcd-> bl [%d] acl [%d]\n",__func__, g_s6e63m0->bl_index, g_s6e63m0->cur_acl);
		switch (g_s6e63m0->bl_index) {
		case 0 ... 2: /* 30cd ~ 60cd */
			if (g_s6e63m0->cur_acl != 0) {
                memcpy(&acl_set_update[1], acl_set_default[ACL_NULL_DSI], 27);
				g_s6e63m0->cur_acl = 0;
			}
			break;
		case 3 ... 24: /* 70cd ~ 250 */
			if (g_s6e63m0->cur_acl != 40) {
                memcpy(&acl_set_update[1], acl_set_default[ACL_40P_DSI], 27);
                g_s6e63m0->cur_acl = 40;
			}
			break;
			
		default:
			if (g_s6e63m0->cur_acl != 40) {
                memcpy(&acl_set_update[1], acl_set_default[ACL_40P_DSI], 27);
                g_s6e63m0->cur_acl = 40;
			}
				printk(" cur_acl=%d\n", g_s6e63m0->cur_acl);
			break;
		}
        if (update)
			pxa168_dsi_cmd_array_tx(fbi_global, s6e63m0_acl_on_set_seq, ARRAY_SIZE(s6e63m0_acl_on_set_seq));

	} else {
        memcpy(&acl_set_update[1], acl_set_default[ACL_NULL_DSI], 27);
		if (update)
			pxa168_dsi_cmd_array_tx(fbi_global, s6e63m0_acl_set_seq, ARRAY_SIZE(s6e63m0_acl_set_seq));
		g_s6e63m0->cur_acl = 0;
	}
}

static void s6e63m0_set_elvss(void)
{
	u8 elvss_val;

	enum elvss_brightness elvss_setting;

	if (g_s6e63m0->current_gamma < 110)
		elvss_setting = elvss_30cd_to_100cd;
	else if (g_s6e63m0->current_gamma < 170)
		elvss_setting = elvss_110cd_to_160cd;
	else if (g_s6e63m0->current_gamma < 210)
		elvss_setting = elvss_170cd_to_200cd;
	else
		elvss_setting = elvss_210cd_to_300cd;

	if (elvss_setting != g_s6e63m0->elvss_brightness) {
		int gamma_index;

		if (g_s6e63m0->elvss_offsets)
			elvss_val = g_s6e63m0->elvss_pulse +
					g_s6e63m0->elvss_offsets[elvss_setting];
		else
			elvss_val = 0x16;

		if (elvss_val > 0x1F)
			elvss_val = 0x1F;	/* max for STOD13CM */

		printk("[%s] ELVSS setting to %x\n", __func__, elvss_val);
		g_s6e63m0->elvss_brightness = elvss_setting;
		for (gamma_index = ELVSS_SET_START_IDX;
			gamma_index <= ELVSS_SET_END_IDX; gamma_index++)

			DCS_CMD_SEQ_ELVSS_SET[gamma_index] = elvss_val;

//		pxa168_dsi_cmd_array_tx(fbi_global, s6e63m0_elvss_set, ARRAY_SIZE(s6e63m0_elvss_set));
	}
}

static void s6e63m0_smart_dimming(void)
{
	int panel_id = get_panel_id();

	switch ((panel_id >> 8) & 0xFF) {
		case ID_VALUE_M2:
			printk( "[%s] Panel is AMS397GE MIPI M2\n", __func__);
			g_s6e63m0->elvss_pulse = panel_id  & 0xFF;
			g_s6e63m0->elvss_offsets = stod13cm_elvss_offsets;
			break;

		case ID_VALUE_SM2:
		case ID_VALUE_SM2_1:
			printk( "[%s] Panel is AMS397GE MIPI SM2\n", __func__);
			g_s6e63m0->elvss_pulse = panel_id  & 0xFF;
			g_s6e63m0->elvss_offsets = stod13cm_elvss_offsets;
			break;

		default:
			printk( "[%s] panel type not recognised (panel_id = %x, %x, %x)\n",
				(panel_id >> 16) & 0xFF, (panel_id >> 8) & 0xFF, panel_id  & 0xFF);
			g_s6e63m0->elvss_pulse = 0x16;
			g_s6e63m0->elvss_offsets = stod13cm_elvss_offsets;
			break;
	}
}

int s6e63m0_update_brightness(struct backlight_device *bl)
{
	struct S6e63m0_info *ps6e63m0 = g_s6e63m0;
	int brightness = bl->props.brightness;

	printk("brightness = [%d]\n",brightness);

	ps6e63m0->current_bl = brightness;

	if (!fbi_global->active)
		return 0;

#ifdef SMART_DIMMING
    if(get_gamma_table == 0)
    {
        printk("%s, SMART_DIMMING \n", __func__);
        get_mtpData();
        init_table_info_22(&ps6e63m0->smart);
        calc_voltage_table(&ps6e63m0->smart, ps6e63m0->mtpData);
        s6e63m0_init_smart_dimming_table_22();
        ps6e63m0->gamma_seq = (const u8 **)gamma_table_sm2;
        get_gamma_table = 1;
    }
#endif

	if (brightness > MAX_BRIGHTNESS_LEVEL)
		brightness = MAX_BRIGHTNESS_LEVEL;

    mutex_lock(&ps6e63m0->lock);
	s6e63m0_gamma_set();
    mutex_unlock(&ps6e63m0->lock);

	return 0;
}

int s6e63m0_get_brightness(struct backlight_device *bl)
{
	return bl->props.brightness;
}

static const struct backlight_ops s6e63m0_bd_ops = {
	.update_status	= s6e63m0_update_brightness,
	.get_brightness	= s6e63m0_get_brightness,
};

struct backlight_properties s6e63m0_backlight_props = {
	.brightness = DEFAULT_BRIGHTNESS,
	.max_brightness = MAX_BRIGHTNESS_LEVEL,
	.type = BACKLIGHT_RAW,
};

static int s6e63m0_probe(struct pxa168fb_info *fbi)
{
	struct S6e63m0_info *s6e63m0Info;
	int ret = 0;
#ifdef SMART_DIMMING
	int panel_id = get_panel_id();
#endif

	fbi_global = fbi;

	printk("%s, probe s6e63m0Info\n", __func__);


	/*For s6e63m0 structure alloc*/
	s6e63m0Info = kzalloc(sizeof(struct S6e63m0_info), GFP_KERNEL);

	if (!s6e63m0Info) {
		pr_err("failed to allocate s6e63m0Info\n");
		ret = -ENOMEM;
		goto error1;
	}

	/*For lcd class*/
	s6e63m0Info->lcd = lcd_device_register("panel", NULL, NULL, NULL);
	if (IS_ERR_OR_NULL(s6e63m0Info->lcd))
	{
		printk("Failed to create lcd class!\n");
		ret = -EINVAL;
		goto error1;
	}

	if (device_create_file(&s6e63m0Info->lcd->dev, &dev_attr_lcd_panelName) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_panelName.attr.name);
	if (device_create_file(&s6e63m0Info->lcd->dev, &dev_attr_lcd_MTP) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_MTP.attr.name);
	if (device_create_file(&s6e63m0Info->lcd->dev, &dev_attr_lcd_type) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_type.attr.name);
	if (device_create_file(&s6e63m0Info->lcd->dev, &dev_attr_power_reduce) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_power_reduce.attr.name);

	/*For backlight*/
	s6e63m0Info->bd = backlight_device_register("panel", s6e63m0Info->dev_bd, s6e63m0Info,
					&s6e63m0_bd_ops,&s6e63m0_backlight_props);

	s6e63m0Info->current_gamma = DEFAULT_BRIGHTNESS;
	s6e63m0Info->bl_index = DEFAULT_GAMMA_LEVEL;
	s6e63m0Info->current_bl= DEFAULT_BRIGHTNESS;
	s6e63m0Info->acl_enable = true;
	s6e63m0Info->panel_awake = true;
	s6e63m0Info->cur_acl = 0;
	s6e63m0Info->elvss_brightness = elvss_not_set;

	g_s6e63m0 = s6e63m0Info;

#ifdef SMART_DIMMING
	printk("%s, SMART_DIMMING : id : %x\n", __func__, panel_id);
    s6e63m0Info->smart.panelid[0] = (panel_id >> 16) & 0xFF;
    s6e63m0Info->smart.panelid[1] = (panel_id >> 8) & 0xFF;
    s6e63m0Info->smart.panelid[2] = (panel_id) & 0xFF;
#endif

    s6e63m0_smart_dimming();

	mutex_init(&s6e63m0Info->lock);

#if defined(CONFIG_LCD_ESD_RECOVERY)
	s6e63m0_esd_det.pdata = fbi;
	s6e63m0_esd_det.lock = &fbi->output_lock;
	if (get_panel_id())
		esd_det_init(&s6e63m0_esd_det);
#endif

	return ret;

error1:
	kfree(s6e63m0Info);	
	return ret;
}

struct pxa168fb_mipi_lcd_driver s6e63m0_lcd_driver = {
	.probe		= s6e63m0_probe,
	.init		= s6e63m0_init,
	.disable	= s6e63m0_disable,
	.enable		= s6e63m0_enable,
};
