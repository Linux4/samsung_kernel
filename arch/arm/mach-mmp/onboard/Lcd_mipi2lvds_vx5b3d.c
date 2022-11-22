/*
 * VX5B3D MIPI to RGB Bridge Chip
 *
 *
 * Copyright (C) 2013, Samsung Corporation.
 *
 * This software program is licensed subject to the GNU General Public License
 * (GPL).Version 2,June 1991, available at http://www.fsf.org/copyleft/gpl.html
 */
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/lcd.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/proc_fs.h>
#include <linux/suspend.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/regulator/machine.h>
#include <mach/cputype.h>
#include <mach/mfp-pxa986-goya.h>
#include <mach/regs-mpmu.h>
#include <mach/pxa988.h>
#include <mach/pxa168fb.h>
#include <mach/Quicklogic_Vxbridge_interface.h>

struct pxa168fb_info *fbi_global = NULL;
struct Quicklogic_bridge_info *g_vx5b3d = NULL;


void vx5b3d_backlightReg_on(int reg_on)
{
	struct Quicklogic_bridge_info *pvx5b3d = g_vx5b3d;

#if defined(CONFIG_QUICKLOGIC_BRIDGE)
	if (reg_on == true) {
		lcd_power(fbi_global, 1);
		mutex_lock(&pvx5b3d->pwr_lock);

		/*6.3Khz period for pwm */
		if(LVDS_CLK_48P05Mhz == pvx5b3d->lvds_clk)
			Quicklogic_i2c_write32(0x160, V5D3BX_6P3KHZ_REG_VALUE);
		else
			Quicklogic_i2c_write32(0x160, V5D3BX_6P3KHZ_REG_VALUE);

		/*Quicklogic_i2c_write32(0x164, 0x0);*/
		Quicklogic_i2c_write32(0x604, 0x3FFFFFE0/*lvds tx enable*/);
		mutex_unlock(&pvx5b3d->pwr_lock);

		msleep(110);

		mutex_lock(&pvx5b3d->pwr_lock);
		Quicklogic_i2c_write32(0x138, 0x3fff0000);
		Quicklogic_i2c_write32(0x15c, 0x5);
		mutex_unlock(&pvx5b3d->pwr_lock);

	} else {
		mutex_lock(&pvx5b3d->pwr_lock);	
		Quicklogic_i2c_write32(0x164, 0x0);
		Quicklogic_i2c_write32(0x15c, 0x0);
		msleep(100);
		Quicklogic_i2c_write32(0x604, 0x3FFFFFFF/*lvds tx disable*/);
		msleep(30);
		mutex_unlock(&pvx5b3d->pwr_lock);
	}
#endif	
}

static int goya_update_brightness(struct Quicklogic_bridge_info *vx5b3d)
{
	struct Quicklogic_backlight_value *pwm = vx5b3d->vee_lightValue;

	int brightness = vx5b3d->bd->props.brightness;

	int vx5b3d_brightness = 0;
	u32 vee_strenght = 0;
	int ret = 0;


	/*
	register 0x160
	register 0x164
			  value of 0x164
	---> duty ratio = -------------
			  value of 0x160
	*/

	/* brightness tuning*/
	if (brightness > MAX_BRIGHTNESS_LEVEL)
		brightness = MAX_BRIGHTNESS_LEVEL;

//	if (brightness == LOW_BATTERY_LEVEL)/*Outgoing Quality Control Group issue*/
//		brightness = MINIMUM_VISIBILITY_LEVEL;

	if (brightness >= MID_BRIGHTNESS_LEVEL) {
		vx5b3d_brightness  = (brightness - MID_BRIGHTNESS_LEVEL) *
		(pwm->max - pwm->mid) / (MAX_BRIGHTNESS_LEVEL-MID_BRIGHTNESS_LEVEL) + pwm->mid;
	} else if (brightness >= LOW_BRIGHTNESS_LEVEL) {
		vx5b3d_brightness  = (brightness - LOW_BRIGHTNESS_LEVEL) *
		(pwm->mid - pwm->low) / (MID_BRIGHTNESS_LEVEL-LOW_BRIGHTNESS_LEVEL) + pwm->low;
	} else if (brightness >= DIM_BRIGHTNESS_LEVEL) {
		vx5b3d_brightness  = (brightness - DIM_BRIGHTNESS_LEVEL) *
		(pwm->low - pwm->dim) / (LOW_BRIGHTNESS_LEVEL-DIM_BRIGHTNESS_LEVEL) + pwm->dim;
	} else if (brightness > 0)
		vx5b3d_brightness  = pwm->dim;
	else {
		vx5b3d_brightness = 0;
		printk("brightness = [%d]: vx5b3d_brightness = [%d]\n",\
			brightness,vx5b3d_brightness);	
	}
	if (vx5b3d->cabc) {

		switch (vx5b3d->auto_brightness) {

		case	0 ... 3:
			vx5b3d->vee_strenght = V5D3BX_DEFAULT_STRENGHT;
			break;
		case	4 ... 5:
			vx5b3d->vee_strenght = V5D3BX_DEFAULT_LOW_STRENGHT;
			break;
		case 	6 ... 8:
			vx5b3d->vee_strenght = V5D3BX_DEFAULT_HIGH_STRENGHT;
			break;	
		default:
			vx5b3d->vee_strenght = V5D3BX_DEFAULT_STRENGHT;
		}
/*
	if (g_vx5d3b->auto_brightness >= 5)
		g_vx5d3b->vee_strenght = V5D3BX_MAX_STRENGHT;
*/
		vee_strenght = V5D3BX_VEESTRENGHT | ((vx5b3d->vee_strenght) << 27);

	if (!(vx5b3d->auto_brightness >= 5))
		vx5b3d_brightness = (vx5b3d_brightness * V5D3BX_CABCBRIGHTNESSRATIO) / 1000;

	} else {
		vee_strenght = V5D3BX_VEESTRENGHT | (V5D3BX_VEEDEFAULTVAL << 27);
	}

	/* brightness setting from platform is from 0 to 255 */
	mutex_lock(&vx5b3d->pwr_lock);

	if (LVDS_CLK_48P05Mhz == vx5b3d->lvds_clk)	
		vx5b3d->vx5b3d_backlight_frq = V5D3BX_6P3KHZ_DEFAULT_RATIO;
	else	/*Default for 51.6Mhz*/
		vx5b3d->vx5b3d_backlight_frq = V5D3BX_6P3KHZ_DEFAULT_RATIO;
/*

	if ((vx5b3d->prevee_strenght != vee_strenght) && (brightness != 0))
		ret |= Quicklogic_i2c_write32(0x400,vee_strenght);
	if (!vx5b3d->first_count)
		ret |= Quicklogic_i2c_write32(0x164,((vx5b3d_brightness * vx5b3d->vx5b3d_backlight_frq)/1000));
*/
#if defined(CONFIG_QUICKLOGIC_BRIDGE)
	ret |= Quicklogic_i2c_write32(0x164,((vx5b3d_brightness * vx5b3d->vx5b3d_backlight_frq)/1000));
	if ( vx5b3d_brightness == 0)
		Quicklogic_i2c_write32(0x15c, 0x0);
	else
		Quicklogic_i2c_write32(0x15c, 0x5);
#endif
#if 0
	/*backlight duty ration control when device is first backlight on.*/
	if (vx5b3d->first_count && brightness != 0) {
		printk("backlight control first...[%d] \n",brightness);
		vx5b3d_backlightReg_on();
		ret |= Quicklogic_i2c_write32(0x164,((vx5b3d_brightness * vx5b3d->vx5b3d_backlight_frq)/1000));

		vx5b3d->first_count = false;
	}
#endif
	vx5b3d->prevee_strenght = vee_strenght;

	mutex_unlock(&vx5b3d->pwr_lock);

	if (ret < 0)
		pr_info("vx5b3d_i2c_write fail [%d] ! \n",ret);

	return 0;
}

void goya_brightness_update(void)
{
	struct Quicklogic_bridge_info *vx5b3d = g_vx5b3d;
	printk("goya brightness update! \n");
	if (vx5b3d->vx5b3d_enable == false)
		vx5b3d_backlightReg_on(true);

	goya_update_brightness(vx5b3d);
}

static int goya_set_brightness(struct backlight_device *bd)
{
	struct Quicklogic_bridge_info *vx5b3d = g_vx5b3d;
	int ret = 0;
	
	vx5b3d->bd->props.brightness = bd->props.brightness;

	printk("backlight control =[%d] \n",bd->props.brightness);

	if (fbi_global->active)
		goya_update_brightness(vx5b3d);

	return ret;
}

static int goya_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static const struct backlight_ops goya_backlight_ops = {
	.get_brightness = goya_get_brightness,
	.update_status = goya_set_brightness,
};

int vx5b3d_power(struct pxa168fb_info *fbi, int on)
{
	struct Quicklogic_bridge_info *vx5b3d = g_vx5b3d;
	static struct regulator *lvds_1v8, *lvds_1v2;

	if (gpio_request(VX5B3D_RST, "lvds reset")) {
		pr_err("gpio %d request failed\n", VX5B3D_RST);
		return -EIO;
	}

	if (gpio_request(VX5B3D_PWR_EN_VXE_3P3, "lvds 3v3")) {
		pr_err("gpio %d request failed\n", VX5B3D_PWR_EN_VXE_3P3);
		return -EIO;
	}

	if (!lvds_1v2) {
		lvds_1v2 = regulator_get(NULL, "v_lvds_1v2");
		if (IS_ERR(lvds_1v2)) {
			pr_err("%s v_lvds_1v2 regulator get error!\n", __func__);
			goto regu_lcd_avdd;
		}
	}

	if (!lvds_1v8) {
		lvds_1v8 = regulator_get(NULL, "v_lvds_1v8");
		if (IS_ERR(lvds_1v8)) {
			pr_err("%s v_lvds_1v8 regulator get error!\n", __func__);
			goto regu_lcd_avdd;
		}
	}

	if (on) {

		/*LVDS_1P2 / 1P8 Enable*/
		regulator_set_voltage(lvds_1v2, 1200000, 1200000);
		regulator_set_voltage(lvds_1v8, 1800000, 1800000);
		
		regulator_enable(lvds_1v2);
		regulator_enable(lvds_1v8);

		if (fbi->skip_pw_on) {
			printk("vx5b3d_power skip pw on \n");
			gpio_free(VX5B3D_PWR_EN_VXE_3P3);
			gpio_free(VX5B3D_RST);			
			return 0;
		}
		/*LVDS_3P3 Enable*/
		gpio_direction_output(VX5B3D_PWR_EN_VXE_3P3, 1);
		mdelay(10);
		/*LVDS_RST*/
		gpio_direction_output(VX5B3D_RST,0);
		udelay(20);
		gpio_direction_output(VX5B3D_RST,1);
		udelay(20);

	} else {
		vx5b3d->prevee_strenght = 0;
		vx5b3d->vx5b3d_enable = false;

		vx5b3d_backlightReg_on(false);
		lcd_power(fbi, 0);		
#if defined(CONFIG_QUICKLOGIC_BRIDGE)
#if defined(CONFIG_SPA) || defined(CONFIG_SPA_LPM_MODE)
		if(!spa_lpm_charging_mode_get())
#else
		if (!lpcharge)
#endif
		i2c1_pin_changed(1);
#endif
		gpio_direction_output(VX5B3D_RST,0);
		udelay(20);
		gpio_direction_output(VX5B3D_PWR_EN_VXE_3P3, 0);
		regulator_disable(lvds_1v8);
		regulator_disable(lvds_1v2);
		msleep(350);

	}

	gpio_free(VX5B3D_PWR_EN_VXE_3P3);
	gpio_free(VX5B3D_RST);

	return 0;
 
regu_lcd_avdd:
	lvds_1v2 = NULL;
	lvds_1v8 = NULL;
	regulator_put(lvds_1v2);
	regulator_put(lvds_1v8);

	return -EIO;

}

int vx5b3d_brige_init(struct pxa168fb_info *fbi)
{
	struct Quicklogic_bridge_info *vx5b3d = g_vx5b3d;
	int status;

	printk("VX5B3D ...START.....\n");
#if defined(CONFIG_QUICKLOGIC_BRIDGE)
	mutex_lock(&vx5b3d->pwr_lock);

	Quicklogic_mipi_write(fbi,0x700, 0x6C900040,4);

	if (LVDS_CLK_48P05Mhz == vx5b3d->lvds_clk)
		Quicklogic_mipi_write(fbi,0x704, 0x3040A,4);
	else
		Quicklogic_mipi_write(fbi,0x704, 0x30449,4); /* LVDS_CLK_50P98Mhz */

	Quicklogic_mipi_write(fbi,0x70C, 0x00004604,4);
	Quicklogic_mipi_write(fbi,0x710, /*0x54D004B vee off*/0x54D000B,4);
	Quicklogic_mipi_write(fbi,0x714, 0x20,4);
	Quicklogic_mipi_write(fbi,0x718, 0x00000102,4);
	Quicklogic_mipi_write(fbi,0x71C, 0xA8002F,4);
	Quicklogic_mipi_write(fbi,0x720, 0x0,4);
	
	Quicklogic_mipi_write(fbi,0x154, 0x00000000,4);
	Quicklogic_mipi_write(fbi,0x154, 0x80000000,4);
	mdelay(1); /* For pll locking */
	Quicklogic_mipi_write(fbi,0x700, 0x6C900840,4);
	Quicklogic_mipi_write(fbi,0x70C, 0x5E56/*0x5646*/,4);
	Quicklogic_mipi_write(fbi,0x718, 0x00000202,4);
	
	
	Quicklogic_mipi_write(fbi,0x154, 0x00000000,4);	
	Quicklogic_mipi_write(fbi,0x154, 0x80000000,4);
	mdelay(1); /* For pll locking */
	Quicklogic_mipi_write(fbi,0x37C, 0x00001063,4);
	Quicklogic_mipi_write(fbi,0x380, 0x82A86030,4);
	Quicklogic_mipi_write(fbi,0x384, 0x2861408B,4);
	Quicklogic_mipi_write(fbi,0x388, 0x00130285,4);
	Quicklogic_mipi_write(fbi,0x38C, 0x10630009,4);
	Quicklogic_mipi_write(fbi,0x394, 0x400B82A8,4);
	Quicklogic_mipi_write(fbi,0x600, 0x16CC78C,4);
	Quicklogic_mipi_write(fbi,0x604, 0x3FFFFFFF,4);
	Quicklogic_mipi_write(fbi,0x608, 0xD8C,4);


	Quicklogic_mipi_write(fbi,0x154, 0x00000000,4);
	Quicklogic_mipi_write(fbi,0x154, 0x80000000,4);

	/* ...move for system reset command (0x158)*/

	Quicklogic_mipi_write(fbi,0x120, 0x5,4);

	if (LVDS_CLK_48P05Mhz == vx5b3d->lvds_clk)
		Quicklogic_mipi_write(fbi,0x124, 0x1892C400,4);
	else
		Quicklogic_mipi_write(fbi,0x124, 0x1B12C400,4); /* LVDS_CLK_50P98Mhz */

	Quicklogic_mipi_write(fbi,0x128, 0x102806,4);
	
	if (LVDS_CLK_48P05Mhz == vx5b3d->lvds_clk)
		Quicklogic_mipi_write(fbi,0x12C, 0x62,4);
	else
		Quicklogic_mipi_write(fbi,0x12C, 0x6C,4); /*LVDS_CLK_50P98Mhz*/

	Quicklogic_mipi_write(fbi,0x130, 0x3C18,4);
	Quicklogic_mipi_write(fbi,0x134, 0x15,4);
	Quicklogic_mipi_write(fbi,0x138, 0xFF8000,4);
	Quicklogic_mipi_write(fbi,0x13C, 0x0,4);


	/*PWM  100 % duty ration*/

	Quicklogic_mipi_write(fbi,0x114, 0xc6302,4);
	/*backlight duty ratio control when device is first bring up.*/
#if 0
	mdelay(200);
	Quicklogic_mipi_write(fbi,0x160, 0x7F8/*0xff*/,4);
	Quicklogic_mipi_write(fbi,0x164, 0x2A8/*0x4c*/,4);
	Quicklogic_mipi_write(fbi,0x138, 0x3fff0000,4);
	Quicklogic_mipi_write(fbi,0x15c, 0x5,4);
	/* END...*/
#endif
	Quicklogic_mipi_write(fbi,0x140, 0x10000,4);
	/*Add for power consumtion*/
	Quicklogic_mipi_write(fbi,0x174, /*0xff vee off*/0x0,4);
	/*end*/

	/*
	slope = 2 / variance = 0x55550022
	slope register [15,10]
	*/
	Quicklogic_mipi_write(fbi, 0x404, 0x55550822,4);

	/*
	To minimize the text effect 
	this value from 0xa to 0xf
	*/
	Quicklogic_mipi_write(fbi, 0x418, 0x555502ff,4);

	/* 
	Disable brightnes issue Caused by IBC
	read 4 bytes from address 0x410 to 0x413
	0x15E50300 is read value for 0x410 register
	0x5E50300= 0x15E50300 & 0xefffffff
	 */
	Quicklogic_mipi_write(fbi,0x410, 0x5E50300,4);
	/*...end*/
	Quicklogic_mipi_write(fbi,0x20C, 0x124,4);
	Quicklogic_mipi_write(fbi,0x21C, 0x0,4);
	Quicklogic_mipi_write(fbi,0x224, 0x7,4);
	Quicklogic_mipi_write(fbi,0x228, 0x50001,4);
	Quicklogic_mipi_write(fbi,0x22C, 0xFF03,4);
	Quicklogic_mipi_write(fbi,0x230, 0x1,4);
	Quicklogic_mipi_write(fbi,0x234, 0xCA033E10,4);
	Quicklogic_mipi_write(fbi,0x238, 0x00000060,4);
	Quicklogic_mipi_write(fbi,0x23C, 0x82E86030,4);
	Quicklogic_mipi_write(fbi,0x244, 0x001E0285,4);
	
	if (LVDS_CLK_48P05Mhz == vx5b3d->lvds_clk)
		Quicklogic_mipi_write(fbi,0x258, 0x60062,4);
	else
		Quicklogic_mipi_write(fbi,0x258, 0xA006C,4); /*LVDS_CLK_50P98Mhz*/

	/*vee strenght initialization*/
	Quicklogic_mipi_write(fbi,0x400, 0x0,4);

	Quicklogic_mipi_write(fbi,0x158, 0x0,4);
	Quicklogic_mipi_write(fbi,0x158, 0x1,4);
	mutex_unlock(&vx5b3d->pwr_lock);
	
	mdelay(10); /* For pll locking */
	/*...end */
#endif	
	printk("VX5B3D ..END.....\n");

	goya_brightness_update();
	vx5b3d->vx5b3d_enable = true;

	return 0;
}

#if defined(CONFIG_QUICKLOGIC_BRIDGE)
static ssize_t vx5b3dxreg_write_store(struct device *dev, struct
device_attribute *attr, const char *buf, size_t size)
{
	int value;
	static u32 vee_data = 0;
	static u16 vee_register = 0;
	static int cnt;
	int rc;

	rc = strict_strtoul(buf, (unsigned int) 0, (unsigned long *)&value);

	if (rc < 0)
		return rc;
	else {
		if ( cnt == 0 )
			vee_register = (u16)value;
		else
			vee_data = value;
		cnt++;
		printk("count value loop =[%d]\n",cnt);
		if (cnt == 2) {
			Quicklogic_i2c_write32(vee_register,vee_data);
			printk("vx5b3dx register[0x%x]..data[0x%x]\n", \
				vee_register, vee_data);
			cnt = 0;
		}

	}
	return size;	
}
static DEVICE_ATTR(veeregwrite, 0664,NULL, vx5b3dxreg_write_store);

static ssize_t vx5b3d_Regread_store(struct device *dev, struct
device_attribute *attr, const char *buf, size_t size)
{
	unsigned long value = 0;
	unsigned long ret_value = 0;	
	int rc = 0;

	if (fbi_global->active == 0)
		return size;

	rc = strict_strtoul(buf, (unsigned int) 0, (unsigned long *)&value);
	if (rc < 0)
		return rc;
	else {
		Quicklogic_i2c_read(value,&ret_value,4);
		pr_info("vx5b3d_Regread_register[0x%x]..return=[0x%x]\n",\
			value,ret_value);

		return size;
	}
}
static DEVICE_ATTR(veeregread, 0664,NULL, vx5b3d_Regread_store);
#endif

static ssize_t power_reduce_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct Quicklogic_bridge_info *pvx5b3d = g_vx5b3d;
	char *pos = buf;

	pos += sprintf(pos, "%d, %d, ", pvx5b3d->auto_brightness, pvx5b3d->cabc);

	return pos - buf;
}

static ssize_t power_reduce_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct Quicklogic_bridge_info *pvx5b3d = g_vx5b3d;
	int value = 0, rc = 0;

	if (fbi_global->active == 0)
		return size;

	rc = strict_strtoul(buf, (unsigned int)0, (unsigned long *)&value);
	
	if (rc < 0)
		return rc;
	else {
		mutex_lock(&pvx5b3d->lock);

		printk("CABC[%d] -> [%d]\n",\
				pvx5b3d->cabc, value);

		if (pvx5b3d->cabc != value) {

			pvx5b3d->auto_brightness = value;
			pvx5b3d->cabc = (value) ? CABC_ON : CABC_OFF;

			if(pvx5b3d->cabc) {
				printk("cabc enable \n");
			} else {
				printk("cabc disable \n");
			}

			goya_brightness_update();
		}

		mutex_unlock(&pvx5b3d->lock);
	}
	return size;
}

static ssize_t lcd_panelName_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "HX8282A");
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
	int ret = 0;

	switch (panel_id) {

		case HX8282A_PANEL_BOE:
			ret = sprintf(buf, "BOE_BA070WS1-400");	
			break;	    	
		case HX8282A_PANEL_SDC:
			ret = sprintf(buf, "SDC_BA070WS1-400");	
			break;
		case HX8282A_PANEL_CPT:
			ret = sprintf(buf, "CPT_BA070WS1-400");	
			break;
		
		case HX8282A_PANEL_HIMAX:
			ret = sprintf(buf, "HIMAX_BA070WS1-400");	
			break;

		default:
			ret = sprintf(buf, "Panel disconnet");	
			break;		   
	}
	return ret;
}
static DEVICE_ATTR(power_reduce, 0644, power_reduce_show, power_reduce_store);
static DEVICE_ATTR(lcd_MTP, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_MTP_show, NULL);
static DEVICE_ATTR(lcd_panelName, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_panelName_show, NULL);
static DEVICE_ATTR(lcd_type, S_IRUGO | S_IWUSR | S_IWGRP | S_IXOTH, lcd_type_show, NULL);

static ssize_t lvds_clk_switch_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct Quicklogic_bridge_info *Vee_cabc = g_vx5b3d;

	int ret = 0;
	ret = sprintf(buf, "%d\n", Vee_cabc->lvds_clk);
	return ret;
}

extern int dsi_init(struct pxa168fb_info *fbi);

static ssize_t lvds_clk_switch_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)

{
	struct Quicklogic_bridge_info *Vee_cabc = g_vx5b3d;
	int ret = 0;
	unsigned int value = 0;

	ret = strict_strtoul(buf, 0, (unsigned long *)&value);

	dev_info(dev, "%s :: value=%d\n", __func__, value);

	if(value == Vee_cabc->lvds_clk)
	{
		printk(" lvds clk has been what you want,so not change\n");
		return -1;
	}
	else if( value > LVDS_CLK_50P98Mhz)
	{
		printk(" invalid lvds freq index!!!\n");
		return -1;
	}

	printk("%s:lvds clk swith to %\n",Vee_cabc->lvds_clk);

	/*just for user mode.
	for test mode,return -1 directly*/
	if (fbi_global->active == 0)
	{
		mutex_lock(&Vee_cabc->lvds_clk_switch_lock);
		/*save new lvds clk temprarily*/		
		Vee_cabc->lvds_clk = value;
		/*update video_modes for every lvds clk update request during lcd off,
		lvds setting will be updated during lcd resume */
	//	pxa168fb_update_modes(fbi_global,Vee_cabc->lvds_clk,Vee_cabc->lcd_panel);
		mutex_unlock(&Vee_cabc->lvds_clk_switch_lock);
		return -1;
	}

	mutex_lock(&Vee_cabc->lvds_clk_switch_lock);

	Vee_cabc->lvds_clk = value;

	Vee_cabc->lvds_clk_switching = true;

	g_vx5b3d->vx5b3d_enable = false;
	
	/*do dsi_init,then lvds_init is called to re-gerenate lvds pclk*/	
	dsi_init(fbi_global);
	
	Vee_cabc->orig_lvds_clk = Vee_cabc->lvds_clk;
	Vee_cabc->lvds_clk_switching = false;

	mutex_unlock(&Vee_cabc->lvds_clk_switch_lock);

	return count;
}

 DEVICE_ATTR(lvds_clk_switch, 0664, lvds_clk_switch_show, lvds_clk_switch_store);

static int goya_bridge_dummy(struct pxa168fb_info *fbi)
{

	return 0;
}

static int goya_bridge_probe(struct pxa168fb_info *fbi)
{
	struct Quicklogic_bridge_info *vx5b3dInfo;
	int ret = 0;

	fbi_global = fbi;

	printk("%s, probe \n", __func__);

	/*For Bridge alloc*/
	vx5b3dInfo = kzalloc(sizeof(struct Quicklogic_bridge_info), GFP_KERNEL);

	if (!vx5b3dInfo) {
		pr_err("failed to allocate vx5b3dInfo\n");
		ret = -ENOMEM;
		goto error1;
	}

	/*For lcd class*/
	vx5b3dInfo->lcd = lcd_device_register("panel", NULL, NULL, NULL);
	if (IS_ERR_OR_NULL(vx5b3dInfo->lcd)) 
	{
		printk("Failed to create lcd class!\n");
		ret = -EINVAL;
		goto error1;
	}

	if (device_create_file(&vx5b3dInfo->lcd->dev, &dev_attr_lcd_panelName) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_panelName.attr.name);
	if (device_create_file(&vx5b3dInfo->lcd->dev, &dev_attr_lcd_MTP) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_MTP.attr.name);
	if (device_create_file(&vx5b3dInfo->lcd->dev, &dev_attr_lcd_type) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lcd_type.attr.name);

#if defined(CONFIG_QUICKLOGIC_BRIDGE)
	if (device_create_file(&vx5b3dInfo->lcd->dev, &dev_attr_veeregwrite) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_veeregwrite.attr.name);

	if (device_create_file(&vx5b3dInfo->lcd->dev, &dev_attr_veeregread) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_veeregwrite.attr.name);
#endif

	/*For backlight*/
	vx5b3dInfo->bd = backlight_device_register("panel", vx5b3dInfo->dev_bd, vx5b3dInfo,
					&goya_backlight_ops,NULL);

	if (device_create_file(&vx5b3dInfo->lcd->dev, &dev_attr_power_reduce) < 0)
		printk("Failed to create auto_brightness\n");

	vx5b3dInfo->bd->props.max_brightness = MAX_BRIGHTNESS_LEVEL;
	vx5b3dInfo->bd->props.brightness = DEFAULT_BRIGHTNESS;
	vx5b3dInfo->bd->props.type = BACKLIGHT_RAW;
	vx5b3dInfo->cabc = CABC_OFF;
	vx5b3dInfo->vee_strenght = V5D3BX_VEEDEFAULTVAL;
	vx5b3dInfo->current_bl = vx5b3dInfo->bd->props.brightness;
	vx5b3dInfo->auto_brightness = false;
	vx5b3dInfo->vx5b3d_enable = true;
	vx5b3dInfo->negative = false;
	vx5b3dInfo->prevee_strenght = 1;
	vx5b3dInfo->lcd_panel = get_panel_id();
	vx5b3dInfo->vee_lightValue = &backlight_table[vx5b3dInfo->lcd_panel];
	vx5b3dInfo->lvds_clk = LVDS_CLK_48P05Mhz;
	vx5b3dInfo->orig_lvds_clk = vx5b3dInfo->lvds_clk;
	vx5b3dInfo->vx5b3d_backlight_frq = V5D3BX_6P3KHZ_DEFAULT_RATIO;

	mutex_init(&vx5b3dInfo->lock);
	mutex_init(&vx5b3dInfo->pwr_lock);
	mutex_init(&vx5b3dInfo->lvds_clk_switch_lock);

	g_vx5b3d = vx5b3dInfo;

	if (device_create_file(&vx5b3dInfo->lcd->dev, &dev_attr_lvds_clk_switch) < 0)
		printk("Failed to create device file(%s)!\n", dev_attr_lvds_clk_switch.attr.name);

	return ret;

error1:
	kfree(vx5b3dInfo);	
	return ret;

}

struct pxa168fb_mipi_lcd_driver goya_lcd_driver = {
	.probe		= goya_bridge_probe,
	.init		= goya_bridge_dummy,
	.disable	= goya_bridge_dummy,
	.enable		= goya_bridge_dummy,
};
