#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/workqueue.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/time.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/sizes.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/mutex.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include "cw2217_iio.h"

#include <linux/hardware_info.h>

#define CWFG_ENABLE_LOG 1 /* CHANGE Customer need to change this for enable/disable log */

#define CW_PROPERTIES "bms"
//#define BATTER_NAME "S88501_VEKEN_LI-ION_5000mah"
#define GAUGE_NAME "CW2217"

#define REG_CHIP_ID             0x00
#define REG_VCELL_H             0x02
#define REG_VCELL_L             0x03
#define REG_SOC_INT             0x04
#define REG_SOC_DECIMAL         0x05
#define REG_TEMP                0x06
#define REG_MODE_CONFIG         0x08
#define REG_GPIO_CONFIG         0x0A
#define REG_SOC_ALERT           0x0B
#define REG_TEMP_MAX            0x0C
#define REG_TEMP_MIN            0x0D
#define REG_CURRENT_H           0x0E
#define REG_CURRENT_L           0x0F
#define REG_T_HOST_H            0xA0
#define REG_T_HOST_L            0xA1
#define REG_USER_CONF           0xA2
#define REG_CYCLE_H             0xA4
#define REG_CYCLE_L             0xA5
#define REG_SOH                 0xA6
#define REG_IC_STATE            0xA7
#define REG_FW_VERSION          0xAB
#define REG_BAT_PROFILE         0x10

#define CONFIG_MODE_RESTART     0x30
#define CONFIG_MODE_ACTIVE      0x00
#define CONFIG_MODE_SLEEP       0xF0
#define CONFIG_UPDATE_FLG       0x80
#define IC_VCHIP_ID             0xA0
#define IC_READY_MARK           0x0C

#define GPIO_ENABLE_MIN_TEMP    0
#define GPIO_ENABLE_MAX_TEMP    0
#define GPIO_ENABLE_SOC_CHANGE  0
#define GPIO_SOC_IRQ_VALUE      0x0    /* 0x7F */
#define DEFINED_MAX_TEMP        45
#define DEFINED_MIN_TEMP        0

#define CWFG_NAME               "cw221X"
#define SIZE_OF_PROFILE         80
#define USER_RSENSE             10000  /* mhom rsense * 1000  for convenience calculation */

#define queue_delayed_work_time  3000
#define queue_start_work_time    50

#define CW_SLEEP_20MS           20
#define CW_SLEEP_30MS           30
#define CW_SLEEP_10MS           10
//bug 795378,taohuayi.wt,mod,2022/10/18,change charge full soc value
#define CW_UI_FULL              100
#define CW_UI_FULL_COLD         97
#define COMPLEMENT_CODE_U16     0x8000
#define CW_SLEEP_100MS          100
#define CW_SLEEP_200MS          200
#define CW_SLEEP_COUNTS         50
#define CW_TRUE                 1
#define CW_RETRY_COUNT          3
#define CW_VOL_UNIT             1000
#define CW_LOW_VOLTAGE_REF      2500
#define CW_LOW_VOLTAGE          3000
#define CW_LOW_VOLTAGE_STEP     10
//+P86801EA2-300 gudi.wt battery protect function
#define CW_UI_FULL_999_UPM6918          82
#define CW_UI_FULL_699_UPM6918          85
#define CW_UI_FULL_399_UPM6918          88
#define CW_UI_FULL_299_UPM6918          93
#define CW_UI_FULL_0_UPM6918            94

#define CW_UI_FULL_999_CX25890          86
#define CW_UI_FULL_699_CX25890          89
#define CW_UI_FULL_399_CX25890          91
#define CW_UI_FULL_299_CX25890          94
#define CW_UI_FULL_0_CX25890            95
//+P86801EA2-300 gudi.wt battery protect function

#define CW_ERROR_VOL			0
#define CW_ERROR_TEMP			-400
#define CW_ERROR_SOC			0


#define CW221X_NOT_ACTIVE          1
#define CW221X_PROFILE_NOT_READY   2
#define CW221X_PROFILE_NEED_UPDATE 3

#define CW2215_MARK             0x80
#define CW2217_MARK             0x40
#define CW2218_MARK             0x00

#define cw_printk(fmt, arg...)                                                 \
	{                                                                          \
		if (CWFG_ENABLE_LOG)                                                   \
			printk("FG_CW221X : %s-%d : " fmt, __FUNCTION__ ,__LINE__,##arg);  \
		else {}                                                                \
	}

static unsigned char config_profile_info[3][SIZE_OF_PROFILE] = {
	{
	0x5A,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	0x93,	0xBC,	0xB6,	0xBC,	0xA5,	0x9B,	0xF0,	0xE9,
	0xE6,	0xFF,	0xFF,	0xC9,	0x95,	0x77,	0x63,	0x51,
	0x46,	0x3E,	0x2F,	0xC5,	0xC5,	0xDC,	0x39,	0xDD,
	0xD6,	0xD5,	0xD4,	0xD3,	0xD0,	0xCC,	0xCA,	0xC9,
	0xBD,	0xC3,	0xC8,	0xA8,	0x91,	0x87,	0x7B,	0x6B,
	0x5E,	0x5C,	0x71,	0x89,	0xA1,	0x97,	0x4F,	0x45,
	0x20,	0x00,	0xAB,	0x10,	0x00,	0xC2,	0x27,	0x00,
	0x00,	0x00,	0x64,	0x25,	0xD1,	0x2A,	0x00,	0x00,
	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x12,
	},
	{
	0x5A,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	0xA0,	0xB4,	0xA7,	0xB8,	0xA6,	0x9C,	0xEC,	0xE3,
	0xE2,	0xFF,	0xFF,	0xDF,	0xA4,	0x83,	0x6D,	0x57,
	0x4B,	0x45,	0x3C,	0xD2,	0xD2,	0xDC,	0x1F,	0xD6,
	0xD0,	0xD2,	0xD2,	0xD1,	0xCE,	0xCC,	0xC9,	0xC7,
	0xC2,	0xC4,	0xCA,	0xA6,	0x94,	0x89,	0x81,	0x74,
	0x69,	0x69,	0x7D,	0x8D,	0xA1,	0x95,	0x4C,	0x49,
	0x20,	0x00,	0xAB,	0x10,	0x00,	0xC2,	0x6B,	0x00,
	0x00,	0x00,	0x64,	0x2B,	0xD1,	0x62,	0x00,	0x00,
	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0xF4,
	},
	{
	0x5A,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
	0xB3,	0xBA,	0xB6,	0xA9,	0x9F,	0x95,	0xF3,	0xEE,
	0xED,	0xD8,	0xC0,	0x91,	0x73,	0x5E,	0x54,	0x45,
	0x3E,	0x35,	0x2D,	0xC3,	0xC4,	0xDC,	0x31,	0xDB,
	0xCF,	0xD0,	0xD0,	0xCF,	0xCC,	0xC9,	0xC6,	0xC1,
	0xC0,	0xC3,	0xCB,	0xA7,	0x94,	0x8D,	0x81,	0x78,
	0x6D,	0x75,	0x82,	0x8E,	0xA3,	0x93,	0x59,	0x49,
	0x20,	0x00,	0xAB,	0x10,	0x00,	0xC2,	0x5D,	0x00,
	0x00,	0x00,	0x64,	0x24,	0xD1,	0x57,	0x00,	0x00,
	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0xF5,
	},
};

struct cw_battery {
	struct i2c_client *client;

//+bug 761884, liyiying.wt, add, 2022/7/5, add cw2217 iio function
	struct device *dev;

	struct iio_dev *indio_dev;
	struct iio_chan_spec *iio_chan;
	struct iio_channel	*int_iio_chans;

	struct iio_channel	**wtchg_ext_iio_chans;
//-bug 761884, liyiying.wt, add, 2022/7/5, add cw2217 iio function

	struct iio_channel	*batt_id_vol;

	struct workqueue_struct *cwfg_workqueue;
	struct delayed_work battery_delay_work;

	struct workqueue_struct *cwfg_updateworkqueue;
	struct delayed_work battery_iio_update_work;

	struct delayed_work batt_id_detect_delay_work;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct power_supply cw_bat;
#else
	struct power_supply *cw_bat;
#endif
	int  chip_id;
	int  voltage;
	int  ic_soc_h;
	int  ic_soc_l;
	int  ui_soc;
	int  raw_soc;
	int  temp;
	long cw_current;
	int  cycle;
	int  cycle_user_control;
	int  soh;
	int  fw_version;

	int batt_id;

//+bug 809455, taohuayi.wt, add, 2022/11/03, change full charge current
	int pre_cur;
	int pre_cap;
//-bug 809455, taohuayi.wt, add, 2022/11/03, change full charge current
	int pre_vol;
	int pre_temp;
	int read_temp;
	int read_vol;
	int read_soc;

	unsigned long last_update;
	int  time_to_full;
};

//static int cw221x_write_iio_prop(struct cw_battery *cw_bat, enum cw221x_iio_type type, int channel, int val);

/* CW221X iic read function */
static int cw_read(struct i2c_client *client, unsigned char reg, unsigned char buf[])
{
	int ret;

	ret = i2c_smbus_read_i2c_block_data( client, reg, 1, buf);
	if (ret < 0)
		printk("IIC error %d\n", ret);

	return ret;
}

/* CW221X iic write function */
static int cw_write(struct i2c_client *client, unsigned char reg, unsigned char const buf[])
{
	int ret;

	ret = i2c_smbus_write_i2c_block_data( client, reg, 1, &buf[0] );
	if (ret < 0)
		printk("IIC error %d\n", ret);

	return ret;
}

/* CW221X iic read word function */
static int cw_read_word(struct i2c_client *client, unsigned char reg, unsigned char buf[])
{
	int ret;
	unsigned char reg_val[2] = { 0, 0 };
	unsigned int temp_val_buff;
	unsigned int temp_val_second;

	ret = i2c_smbus_read_i2c_block_data( client, reg, 2, reg_val );
	if (ret < 0)
		printk("IIC error %d\n", ret);
	temp_val_buff = (reg_val[0] << 8) + reg_val[1];

	msleep(4);
	ret = i2c_smbus_read_i2c_block_data( client, reg, 2, reg_val );
	if (ret < 0)
		printk("IIC error %d\n", ret);
	temp_val_second = (reg_val[0] << 8) + reg_val[1];

	if (temp_val_buff != temp_val_second) {
		msleep(4);
		ret = i2c_smbus_read_i2c_block_data( client, reg, 2, reg_val );
		if (ret < 0)
			printk("IIC error %d\n", ret);
		temp_val_buff = (reg_val[0] << 8) + reg_val[1];
	}

	buf[0] = reg_val[0];
	buf[1] = reg_val[1];

	return ret;
}

/* CW221X iic write profile function */
#if 0
static int cw_write_profile(struct i2c_client *client, unsigned char const buf[])
{
	int ret;
	int i;

	for (i = 0; i < SIZE_OF_PROFILE; i++) {
		ret = cw_write(client, REG_BAT_PROFILE + i, &buf[i]);
		if (ret < 0) {
			printk("IIC error %d\n", ret);
			return ret;
		}
	}

	return ret;
}
#endif

/* 
 * CW221X Active function 
 * The CONFIG register is used for the host MCU to configure the fuel gauge IC. The default value is 0xF0,
 * SLEEP and RESTART bits are set. To power up the IC, the host MCU needs to write 0x30 to exit shutdown
 * mode, and then write 0x00 to restart the gauge to enter active mode. To reset the IC, the host MCU needs
 * to write 0xF0, 0x30 and 0x00 in sequence to this register to complete the restart procedure. The CW221X
 * will reload relevant parameters and settings and restart SOC calculation. Note that the SOC may be a
 * different value after reset operation since it is a brand-new calculation based on the latest battery status.
 * CONFIG [3:0] is reserved. Don't do any operation with it.
 */
static int cw221X_active(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val = CONFIG_MODE_RESTART;

	cw_printk("\n");

	ret = cw_write(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	msleep(CW_SLEEP_30MS);  /* Here delay must >= 20 ms */

	reg_val = CONFIG_MODE_ACTIVE;
	ret = cw_write(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	msleep(CW_SLEEP_10MS);

	return 0;
}

/* 
 * CW221X Sleep function 
 * The CONFIG register is used for the host MCU to configure the fuel gauge IC. The default value is 0xF0,
 * SLEEP and RESTART bits are set. To power up the IC, the host MCU needs to write 0x30 to exit shutdown
 * mode, and then write 0x00 to restart the gauge to enter active mode. To reset the IC, the host MCU needs
 * to write 0xF0, 0x30 and 0x00 in sequence to this register to complete the restart procedure. The CW221X
 * will reload relevant parameters and settings and restart SOC calculation. Note that the SOC may be a
 * different value after reset operation since it is a brand-new calculation based on the latest battery status.
 * CONFIG [3:0] is reserved. Don't do any operation with it.
 */
static int cw221X_sleep(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val = CONFIG_MODE_RESTART;

	cw_printk("\n");

	ret = cw_write(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	msleep(CW_SLEEP_30MS);  /* Here delay must >= 20 ms */

	reg_val = CONFIG_MODE_SLEEP;
	ret = cw_write(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	msleep(CW_SLEEP_10MS);

	return 0;
}

/*
 * The 0x00 register is an UNSIGNED 8bit read-only register. Its value is fixed to 0xA0 in shutdown
 * mode and active mode.
 */
static int cw_get_chip_id(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val;
	int chip_id;

	ret = cw_read(cw_bat->client, REG_CHIP_ID, &reg_val);
	if (ret < 0)
		return ret;

	chip_id = reg_val;  /* This value must be 0xA0! */
	cw_printk("chip_id = %d\n", chip_id);
	cw_bat->chip_id = chip_id;

	return 0;
}

/*
 * wtchg_get_battery_id
 */
static void cw221x_get_battery_id (struct cw_battery *cw_bat)
{
	int	ret, batt_id_uv = 0;
	int batt_id_temp;

#ifdef CONFIG_QGKI_BUILD
	cw_bat->batt_id_vol = iio_channel_get(cw_bat->dev, "batt_id");
	if (!cw_bat->batt_id_vol) {
		pr_err("get cw_get_battery_id fail\n");
		return;
	}
#else	
	pr_err("cw221x_get_battery_id for google gki\n");
	cw_bat->batt_id = 0;
	return;
#endif
	ret = iio_read_channel_processed(cw_bat->batt_id_vol, &batt_id_uv);
	if (ret < 0) {
		dev_err(cw_bat->dev,"read batt_id_vol channel fail, ret=%d\n", ret);
		return;
	}
 /*+P86801AA1-1797, dingmingyuan.wt, add, 2023/7/19, Identify non-standard batteries*/
	batt_id_temp = batt_id_uv  / 1000;
	pr_err("wtchg_get_battery_id: batt_id_uv=%d \n", batt_id_uv);
	pr_err("wtchg_get_battery_id: batt_id_temp_=%d \n", batt_id_temp);

	if(batt_id_temp > 1350)
		cw_bat->batt_id = 1;//main
	else if (batt_id_temp < 850)
		cw_bat->batt_id = 2;//second
	else if ((batt_id_temp > 850) && (batt_id_temp < 1350))
		cw_bat->batt_id = 3;//third
	else
		cw_bat->batt_id = 0;//default
 /*+P86801AA1-1797, dingmingyuan.wt, add, 2023/7/19, Identify non-standard batteries*/
	pr_err("wtchg_get_battery_id: batt_id=%d \n", cw_bat->batt_id);
}

/*
 * The VCELL register(0x02 0x03) is an UNSIGNED 14bit read-only register that updates the battery voltage continuously.
 * Battery voltage is measured between the VCELL pin and VSS pin, which is the ground reference. A 14bit
 * sigma-delta A/D converter is used and the voltage resolution is 312.5uV. (0.3125mV is *5/16)
 */
static int cw_get_voltage(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val[2] = {0 , 0};
	unsigned int voltage;
	static int error_count = 0;

	ret = cw_read_word(cw_bat->client, REG_VCELL_H, reg_val);
	if (ret < 0)
		return ret;
	voltage = (reg_val[0] << 8) + reg_val[1];
	voltage = voltage  * 5 / 16;
	cw_bat->voltage = voltage;

	if (voltage ==  CW_ERROR_VOL && error_count++< CW_RETRY_COUNT) {
		cw_bat->voltage = cw_bat->pre_vol;
		cw_printk("CW2117[%d] vol:%d error count:%d setting:%d !!!!\n", __LINE__, voltage,  error_count, cw_bat->voltage);
	} else if (voltage != CW_ERROR_VOL && error_count > 0){
		error_count = 0;
	}
	cw_bat->read_vol = voltage;
	cw_bat->pre_vol = cw_bat->voltage;

	return 0;
}
static int cw_get_temp(struct cw_battery *cw_bat);
/*
 * The SOC register(0x04 0x05) is an UNSIGNED 16bit read-only register that indicates the SOC of the battery. The
 * SOC shows in % format, which means how much percent of the battery's total available capacity is
 * remaining in the battery now. The SOC can intrinsically adjust itself to cater to the change of battery status,
 * including load, temperature and aging etc.
 * The high byte(0x04) contains the SOC in 1% unit which can be directly used if this resolution is good
 * enough for the application. The low byte(0x05) provides more accurate fractional part of the SOC and its
 * LSB is (1/256) %.
 */

//+P86801EA2-300 gudi.wt battery protect function
#ifdef CONFIG_QGKI_BUILD
extern bool sgm41542_if_been_used(void);
extern bool cx25890h_if_been_used(void);
#endif
//-P86801EA2-300 gudi.wt battery protect function

static int cw_get_capacity(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val[2] = { 0, 0 };
	int ui_100 = CW_UI_FULL;
	int soc_h;
	int soc_l;
	int ui_soc;
	int remainder;
	static int count= 0;
	static int error_count = 0;

	ret = cw_read_word(cw_bat->client, REG_SOC_INT, reg_val);
	if (ret < 0)
		return ret;
	soc_h = reg_val[0];
	soc_l = reg_val[1];

//+P86801EA2-300 gudi.wt battery protect function
#ifdef CONFIG_QGKI_BUILD
	if(sgm41542_if_been_used()){
		if (cw_bat->cycle > 999)
		ui_100 = CW_UI_FULL_999_UPM6918;
	else if (cw_bat->cycle > 699)
		ui_100 = CW_UI_FULL_699_UPM6918;
	else if (cw_bat->cycle > 399)
		ui_100 = CW_UI_FULL_399_UPM6918;
	else if (cw_bat->cycle > 299)
		ui_100 = CW_UI_FULL_299_UPM6918;
	else
		ui_100 = CW_UI_FULL_0_UPM6918;
	}else if(cx25890h_if_been_used()){
		if (cw_bat->cycle > 999)
		ui_100 = CW_UI_FULL_999_CX25890;
	else if (cw_bat->cycle > 699)
		ui_100 = CW_UI_FULL_699_CX25890;
	else if (cw_bat->cycle > 399)
		ui_100 = CW_UI_FULL_399_CX25890;
	else if (cw_bat->cycle > 299)
		ui_100 = CW_UI_FULL_299_CX25890;
	else
		ui_100 = CW_UI_FULL_0_CX25890;
	}
	if (cw_bat->temp <= 120)
		ui_100 -= 2;
#endif
//-P86801EA2-300 gudi.wt battery protect function

	ui_soc = ((soc_h * 256 + soc_l) * 100)/ (ui_100 * 256);
	remainder = (((soc_h * 256 + soc_l) * 100 * 100) / (ui_100 * 256)) % 100;

	cw_bat->ic_soc_h = soc_h;
	cw_bat->ic_soc_l = soc_l;
	if(ui_soc >= 1){
		//+P231016-06147  gudi.wt,soc after limit still rising
		/* case 1 : aviod swing */	//EXTB P210227-01167,taohuayi.wt 20211228 [70,30] ->[90,10].
		if((ui_soc >= cw_bat->ui_soc - 1) && (ui_soc <= cw_bat->ui_soc + 1)
						&& (remainder > 70 || remainder < 30) ){
			ui_soc = cw_bat->ui_soc;
//+P231104-03175,gudi.wt，20231106,fix soc larger 100 can not show 100
		}else if(ui_soc >= 100){
			cw_printk("CW2015[%d]: UI_SOC = %d larger 100!!!!\n", __LINE__, ui_soc);
			ui_soc = 100;
			cw_bat->ui_soc = ui_soc;
		}else{
			cw_bat->ui_soc = ui_soc;
		}
//-P231104-03175,gudi.wt，20231106,fix soc larger 100 can not show 100
		//-P231016-06147  gudi.wt,soc after limit still rising
	}else if(ui_soc < 1){
            if(cw_bat->voltage < 3400){
                  count++;
                  if(count > 10)
                    cw_bat->ui_soc = ui_soc;
                  else 
                    cw_bat->ui_soc = ui_soc+1;
   	    }else{
                  cw_bat->ui_soc = ui_soc+1;
                  count = 0;
            }	
	//bug761884, liyiying.wt, add, 2022/9/6, change the report ui_100
	cw_bat->raw_soc = ((soc_h * 256 + soc_l) * 100) / (100 * 256);
	if(cw_bat->voltage < 3250)
		cw_bat->ui_soc = ui_soc;
	}

	if (ui_soc == CW_ERROR_SOC && error_count++ < CW_RETRY_COUNT) {
		ret = cw_get_temp(cw_bat);
		if(cw_bat->read_temp == CW_ERROR_TEMP){
			cw_bat->ui_soc = cw_bat->pre_cap;
			cw_printk("CW2117[%d] soc:%d error_count:%d setting to:%d\n", __LINE__, ui_soc, error_count, cw_bat->ui_soc);
		}
	} else if (ui_soc != CW_ERROR_SOC && error_count > 0 ) {
		error_count = 0;
	}

	cw_bat->read_soc = ui_soc;
	cw_bat->pre_cap = cw_bat->ui_soc;

	return 0;
}

/*
 * The TEMP register is an UNSIGNED 8bit read only register. 
 * It reports the real-time battery temperature
 * measured at TS pin. The scope is from -40 to 87.5 degrees Celsius, 
 * LSB is 0.5 degree Celsius. TEMP(C) = - 40 + Value(0x06 Reg) / 2 
 */
static int cw_get_temp(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val;
	int temp;
	static int error_count = 0;

	ret = cw_read(cw_bat->client, REG_TEMP, &reg_val);
	if (ret < 0)
		return ret;

	temp = (int)reg_val * 10 / 2 - 400;
	cw_bat->temp = temp;

	if (temp == CW_ERROR_TEMP && error_count++ < CW_RETRY_COUNT) {
		cw_bat->temp = cw_bat->pre_temp;
		cw_printk("CW2117[%d] temp:%d error_count:%d setting to:%d\n", __LINE__, temp, error_count, cw_bat->temp);
	} else if (temp != CW_ERROR_TEMP && error_count > 0 ) {
		error_count = 0;
	}

	cw_bat->read_temp = temp;
	cw_bat->pre_temp = cw_bat->temp;

	return 0;
}

static int cw_get_avg_temp(struct cw_battery *cw_bat)
{
	int i;
	static int sum = 0;
	int ret = 0;
	static int temp_buf[5] = {1000,1000,1000,1000,1000};
	static int start_count = 0;

	if(start_count == 0){
		for(i = 0;i < 5;i++){
			ret = cw_get_temp(cw_bat);
			temp_buf[i] = cw_bat->temp;
			sum += temp_buf[i];
		}
		start_count++;
	}

	if(temp_buf[4] != 1000){
		sum = sum - temp_buf[0];
		for(i = 0;i < 4;i++){
			temp_buf[i] = temp_buf[i+1];
		}
		ret = cw_get_temp(cw_bat);
		temp_buf[4] = cw_bat->temp;
		sum = sum + temp_buf[4];
	}
	cw_bat->temp = sum/5;
	return 0;
}

/* get complement code function, unsigned short must be U16 */
static long get_complement_code(unsigned short raw_code)
{
	long complement_code;
	int dir;

	if (0 != (raw_code & COMPLEMENT_CODE_U16)){
		dir = -1;
		raw_code =  (0xFFFF - raw_code) + 1;
	} else {
		dir = 1;
	}
	complement_code = (long)raw_code * dir;

	return complement_code;
}

/*
 * CURRENT is a SIGNED 16bit register(0x0E 0x0F) that reports current A/D converter result of the voltage across the
 * current sense resistor, 10mohm typical. The result is stored as a two's complement value to show positive
 * and negative current. Voltages outside the minimum and maximum register values are reported as the
 * minimum or maximum value.
 * The register value should be divided by the sense resistance to convert to amperes. The value of the
 * sense resistor determines the resolution and the full-scale range of the current readings. The LSB of 0x0F
 * is (52.4/32768)uV for CW2215 and CW2217. The LSB of 0x0F is (125/32768)uV for CW2218.
 * The default value is 0x0000, stands for 0mA. 0x7FFF stands for the maximum charging current and 0x8001 stands for
 * the maximum discharging current.
 */
static int cw_get_current(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val[2] = {0 , 0};
	long long cw_current; /* use long long type to guarantee 8 bytes space*/
	unsigned short current_reg;  /* unsigned short must u16 */

	ret = cw_read_word(cw_bat->client, REG_CURRENT_H, reg_val);
	if (ret < 0)
		return ret;

	current_reg = (reg_val[0] << 8) + reg_val[1];
	cw_current = get_complement_code(current_reg);
	if((cw_bat->fw_version & CW2215_MARK) != 0 || (cw_bat->fw_version & CW2217_MARK) != 0){
		cw_current = cw_current * 1600 / USER_RSENSE;
	}else if((cw_bat->fw_version != 0) && ((cw_bat->fw_version & 0xC0) == CW2218_MARK)){
		cw_current = cw_current * 3815 / USER_RSENSE;
	}else{
		cw_bat->cw_current = 0;
		printk("error! cw221x frimware read error!\n");
	}
	cw_bat->cw_current = cw_current;

	return 0;
}

/*
 * CYCLECNT is an UNSIGNED 16bit register(0xA4 0xA5) that counts cycle life of the battery. The LSB of 0xA5 stands
 * for 1/16 cycle. This register will be clear after enters shutdown mode
 */
static int cw_get_cycle_count(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val[2] = {0, 0};
	int cycle;

	if (cw_bat->cycle_user_control != 0xFFFF) {
		cw_bat->cycle = cw_bat->cycle_user_control;
		return 0;
	}

	ret = cw_read_word(cw_bat->client, REG_CYCLE_H, reg_val);
	if (ret < 0)
		return ret;

	cycle = (reg_val[0] << 8) + reg_val[1];
	cw_bat->cycle = cycle / 16;

	return 0;
}

static int cw_set_cycle_count(struct cw_battery *cw_bat, int val)
{
	cw_bat->cycle_user_control = val;
	return 0;
}

/*
 * SOH (State of Health) is an UNSIGNED 8bit register(0xA6) that represents the level of battery aging by tracking
 * battery internal impedance increment. When the device enters active mode, this register refresh to 0x64
 * by default. Its range is 0x00 to 0x64, indicating 0 to 100%. This register will be clear after enters shutdown
 * mode.
 */
static int cw_get_soh(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val;
	int soh;

	ret = cw_read(cw_bat->client, REG_SOH, &reg_val);
	if (ret < 0)
		return ret;

	soh = reg_val;
	cw_bat->soh = soh;

	return 0;
}

#define DESIGNED_CAPACITY 409200 //mAmin
#define CHARGE_FULL_SOC 100
#define DEADSOC_COEFFICIENT 100
#define CHARGE_STATE_CHANGE_SOC 92
//P231020-00869 gudi.wt,20231023,fix charge full time
#define CHARGE_CC_CURRENT_THRESHOLD 1700
#define MAGIC_CHARGE_CC_CURRENT 2000
#define MAGIC_CHARGE_END_CV_CURRENT 1300
#define DESIGNED_CAPACITY_85 347820 //mAmin
#define MAGIC_CHARGE_CC_CURRENT_85 2400
#define CHARGE_FULL_SOC_85 833

static int cw_get_time_to_charge_full(struct cw_battery *cw_bat)
{
#ifdef CONFIG_QGKI_BUILD
	static struct power_supply *bat_psy = NULL;
	union power_supply_propval cap_val;
	int ret;
#endif
	int ic_current = cw_bat->cw_current;
	int ic_soc_h = cw_bat->ic_soc_h;
	int ic_soc_l = cw_bat->ic_soc_l;
	int soh = cw_bat->soh;
	long int discharge_capacity;
	int magic_current;
	int time_to_charge_full;
	int deadsoc_coefficient;

#ifdef CONFIG_QGKI_BUILD
	if(!bat_psy){
		bat_psy= power_supply_get_by_name("battery");
		if(!bat_psy){
			printk("battery psy get failed\n");
			return 0;
		}
	}
	ret = power_supply_get_property(bat_psy,POWER_SUPPLY_PROP_BATT_FULL_CAPACITY,&cap_val);
#endif
	if(ic_current < 10){
		cw_bat->time_to_full = -1;
		return 0; //no chargering
	}

	deadsoc_coefficient = DEADSOC_COEFFICIENT;
	//for Paul change, You can add condition to change deadsoc_coefficient
#ifdef CONFIG_QGKI_BUILD
	if(cap_val.intval == 85){
		discharge_capacity = DESIGNED_CAPACITY_85 * soh / 100;
		discharge_capacity = discharge_capacity * (CHARGE_FULL_SOC_85/10*256 - (ic_soc_h *256 + ic_soc_l)) / 100 / 256;
}
	else{
		discharge_capacity = DESIGNED_CAPACITY * soh / 100;
		discharge_capacity = discharge_capacity * (CHARGE_FULL_SOC*256 - (ic_soc_h *256 + ic_soc_l)) / 100 / 256;
	}
#else
	discharge_capacity = DESIGNED_CAPACITY * soh / 100;
	discharge_capacity = discharge_capacity * (CHARGE_FULL_SOC*256 - (ic_soc_h *256 + ic_soc_l)) / 100 / 256;
#endif
	discharge_capacity = discharge_capacity * deadsoc_coefficient / 100;
	if(ic_current > CHARGE_CC_CURRENT_THRESHOLD && ic_soc_h < CHARGE_STATE_CHANGE_SOC)
#ifdef CONFIG_QGKI_BUILD
	if(cap_val.intval == 85)
		magic_current = MAGIC_CHARGE_CC_CURRENT_85;
	else
		magic_current = MAGIC_CHARGE_CC_CURRENT;
#else
		magic_current = MAGIC_CHARGE_CC_CURRENT;
#endif
//P231020-00869 gudi.wt modify time_to_charge_full for UI

	else if(ic_current <= CHARGE_CC_CURRENT_THRESHOLD && ic_current > 10)
		magic_current = MAGIC_CHARGE_END_CV_CURRENT;
	else
		magic_current = cw_bat->cw_current;
	time_to_charge_full = discharge_capacity * 60 / magic_current; //second

	cw_bat->time_to_full = time_to_charge_full;
	return 0;
}

/*
 * FW_VERSION register reports the firmware (FW) running in the chip. It is fixed to 0x00 when the chip is
 * in shutdown mode. When in active mode, Bit [7:6] = '01' stand for the CW2217, Bit [7:6] = '00' stand for 
 * the CW2218 and Bit [7:6] = '10' stand for CW2215.
 * Bit[5:0] stand for the FW version running in the chip. Note that the FW version is subject to update and 
 * contact sales office for confirmation when necessary.
*/
static int cw_get_fw_version(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val;
	int fw_version;

	ret = cw_read(cw_bat->client, REG_FW_VERSION, &reg_val);
	if (ret < 0)
		return ret;

	fw_version = reg_val; 
	cw_bat->fw_version = fw_version;

	return 0;
}

#if 0
static void cw221x_update_work(struct cw_battery *cw_bat)
{
	int ret;
	ret = cw221x_write_iio_prop(cw_bat, WT_CHG, WTCHG_UPDATE_WORK, MAIN_CHG);
	if (ret < 0)
		pr_info("%s: fail: %d\n", __func__, ret);
}
#endif

static int cw_init(struct cw_battery *cw_bat);

static int cw_recovery(struct cw_battery *cw_bat)
{
	int loop = 0;
	int ret = 0;

	ret = cw_init(cw_bat);
	while ((loop++ < CW_RETRY_COUNT ) && (ret != 0)) {
		msleep(CW_SLEEP_200MS);
		ret = cw_init(cw_bat);
	}

	if (ret) {
		pr_err("%s : cw221X recovery fail!\n", __func__);
		return -EPROBE_DEFER;
	}
	return ret;

}

static int cw_update_data(struct cw_battery *cw_bat)
{
	int ret = 0;

	ret += cw_get_voltage(cw_bat);
	ret += cw_get_cycle_count(cw_bat);
	ret += cw_get_capacity(cw_bat);
	ret += cw_get_avg_temp(cw_bat);
	ret += cw_get_current(cw_bat);
	ret += cw_get_soh(cw_bat);
	ret += cw_get_time_to_charge_full(cw_bat);

	pr_info("cw_update_data vol = %d  current = %ld cap = %d temp = %d ic_soc_h = %d ic_soc_l = %d raw_soc = %d,time_to_full = %d\n",
		cw_bat->voltage, cw_bat->cw_current, cw_bat->ui_soc, cw_bat->temp, cw_bat->ic_soc_h, cw_bat->ic_soc_l, cw_bat->raw_soc,cw_bat->time_to_full);

	if(cw_bat->read_temp == CW_ERROR_TEMP && cw_bat->read_vol == CW_ERROR_VOL && cw_bat->read_soc == CW_ERROR_SOC){
		pr_err("cw2217 disable, enter recovery\n");
		ret += cw_recovery(cw_bat);
		msleep(1500);
		ret += cw_get_voltage(cw_bat);
		ret += cw_get_cycle_count(cw_bat);
		ret += cw_get_capacity(cw_bat);
		ret += cw_get_avg_temp(cw_bat);
		ret += cw_get_current(cw_bat);
		ret += cw_get_soh(cw_bat);
		ret += cw_get_time_to_charge_full(cw_bat);
	}
	return ret;
}

static int cw_init_data(struct cw_battery *cw_bat)
{
	int ret = 0;

	ret = cw_get_fw_version(cw_bat);
	if(ret != 0){
		return ret;
	}
	cw_bat->cycle_user_control = 0xFFFF;
	ret += cw_get_chip_id(cw_bat);
	ret += cw_get_voltage(cw_bat);
	//+P86801AA2-3091  liwei.wt, modify, 2024/1/11, ui_soc decreases when soc read from fuelgague increased
	ret += cw_get_temp(cw_bat);
	//-P86801AA2-3091  liwei.wt, modify, 2024/1/11, ui_soc decreases when soc read from fuelgague increased
	ret += cw_get_cycle_count(cw_bat);
	ret += cw_get_capacity(cw_bat);
	ret += cw_get_current(cw_bat);
	ret += cw_get_soh(cw_bat);

	cw_printk("chip_id = %d vol = %d  cur = %ld cap = %d temp = %d  fw_version = %d\n",
		cw_bat->chip_id, cw_bat->voltage, cw_bat->cw_current, cw_bat->ui_soc, cw_bat->temp, cw_bat->fw_version);

	return ret;
}

/*CW221X update profile function, Often called during initialization*/
static int cw_config_start_ic(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val;
	int count = 0;
	int i;
	//+ P86801AA1-12448, xiejiaming.wt, add, 20230814, reboot and id error, temp was -20 
	int corr_batt_id;
	int profiles_num = ARRAY_SIZE(config_profile_info);
	//- P86801AA1-12448, xiejiaming.wt, add, 20230814, reboot and id error, temp was -20 

	ret = cw221X_sleep(cw_bat);
	if (ret < 0)
		return ret;	

	/* update new battery info */
	#if 0
	ret = cw_write_profile(cw_bat->client, config_profile_info[cw_bat->batt_id - 1]);
	if (ret < 0)
		return ret;
	#endif

	//+ P86801AA1-12448, xiejiaming.wt, add, 20230814, reboot and id error, temp was -20 
	corr_batt_id = cw_bat->batt_id - 1;
	if ( 0 > corr_batt_id || profiles_num <= corr_batt_id ) {
		cw_printk("batt_id:%d out of profile nums%d, correct to first profile\n", corr_batt_id, profiles_num);
		corr_batt_id = 0;
	}
	//- P86801AA1-12448, xiejiaming.wt, add, 20230814, reboot and id error, temp was -20 

	for (i = 0; i < SIZE_OF_PROFILE; i++) {
		ret = cw_write(cw_bat->client, REG_BAT_PROFILE + i, &config_profile_info[corr_batt_id][i]);
		cw_printk("0x%2x = 0x%2x\n",i, config_profile_info[corr_batt_id][i]);
		//cw_printk("0x%2x = 0x%2x\n", reg_profile, reg_val);
		if (ret < 0) {
			printk("IIC error %d\n", ret);
			return ret;
		}
	}

	//return ret;


	/* set UPDATE_FLAG AND SOC INTTERRUP VALUE*/
	reg_val = CONFIG_UPDATE_FLG | GPIO_SOC_IRQ_VALUE;   
	ret = cw_write(cw_bat->client, REG_SOC_ALERT, &reg_val);
	if (ret < 0)
		return ret;

	/*close all interruptes*/
	reg_val = 0; 
	ret = cw_write(cw_bat->client, REG_GPIO_CONFIG, &reg_val); 
	if (ret < 0)
		return ret;

	ret = cw221X_active(cw_bat);
	if (ret < 0) 
		return ret;

	while (CW_TRUE) {
		msleep(CW_SLEEP_100MS);
		cw_read(cw_bat->client, REG_IC_STATE, &reg_val);
		if (IC_READY_MARK == (reg_val & IC_READY_MARK))
			break;
		count++;
		if (count >= CW_SLEEP_COUNTS) {
			cw221X_sleep(cw_bat);
			return -1;
		}
	}

	return 0;
}

/*
 * Get the cw221X running state
 * Determine whether the profile needs to be updated 
*/
static int cw221X_get_state(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val;
	int i;
	int reg_profile;

	ret = cw_read(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if (ret < 0)
		return ret;
	if (reg_val != CONFIG_MODE_ACTIVE)
		return CW221X_NOT_ACTIVE;

	ret = cw_read(cw_bat->client, REG_SOC_ALERT, &reg_val);
	if (ret < 0)
		return ret;
	if (0x00 == (reg_val & CONFIG_UPDATE_FLG))
		return CW221X_PROFILE_NOT_READY;

	for (i = 0; i < SIZE_OF_PROFILE; i++) {
		ret = cw_read(cw_bat->client, (REG_BAT_PROFILE + i), &reg_val);
		if (ret < 0)
			return ret;
		reg_profile = REG_BAT_PROFILE + i;
		cw_printk("0x%2x = 0x%2x\n", reg_profile, reg_val);
		if (config_profile_info[cw_bat->batt_id - 1][i] != reg_val)
			break;
	}
	if (i != SIZE_OF_PROFILE)
		return CW221X_PROFILE_NEED_UPDATE;

	return 0;
}

/*CW221X init function, Often called during initialization*/
static int cw_init(struct cw_battery *cw_bat)
{
	int ret;

	cw_printk("\n");
	ret = cw_get_chip_id(cw_bat);
	if (ret < 0) {
		printk("iic read write error");
		return ret;
	}
	if (cw_bat->chip_id != IC_VCHIP_ID){
		printk("not cw221X\n");
		return -1;
	}

	ret = cw221X_get_state(cw_bat);
	if (ret < 0) {
		printk("iic read write error");
		return ret;
	}

	if (ret != 0) {
		ret = cw_config_start_ic(cw_bat);
		if (ret < 0)
			return ret;
	}
	cw_printk("cw221X init success!\n");

	return 0;
}

static void cw_bat_work(struct work_struct *work)
{
	struct delayed_work *delay_work;
	struct cw_battery *cw_bat;
	int ret;

	delay_work = container_of(work, struct delayed_work, work);
	cw_bat = container_of(delay_work, struct cw_battery, battery_delay_work);

	ret = cw_update_data(cw_bat);
	if (ret < 0)
		printk(KERN_ERR "iic read error when update data");

	
	#ifdef CW_PROPERTIES
	#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	power_supply_changed(&cw_bat->cw_bat); 
	#else
	power_supply_changed(cw_bat->cw_bat); 
	/*add iio noyify*/
	#endif
	#endif

	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(queue_delayed_work_time));
}

#if 0
static void cw_bat_iio_update_work(struct work_struct *work)
{
	struct delayed_work *delay_work;
	struct cw_battery *cw_bat;
	int ret = 0;
	static int pre_uisoc = 0;
	static int pre_temp = 0;

	delay_work = container_of(work, struct delayed_work, work);
	cw_bat = container_of(delay_work, struct cw_battery, battery_iio_update_work);

	//cap
	ret += cw_get_capacity(cw_bat);
	ret += cw_get_temp(cw_bat);
	if ((pre_uisoc != cw_bat->ui_soc)||(pre_temp != cw_bat->temp)) {
		cw221x_update_work(cw_bat);
		pre_uisoc = cw_bat->ui_soc;
		pre_temp = cw_bat->temp;
		pr_err("cw221x iio update!!!!!\n");
	}

	queue_delayed_work(cw_bat->cwfg_updateworkqueue, &cw_bat->battery_iio_update_work, msecs_to_jiffies(queue_delayed_work_time));
}
#endif

#ifdef CW_PROPERTIES
static int cw_battery_set_property(struct power_supply *psy,
				enum power_supply_property psp,
				const union power_supply_propval *val)
{
	int ret = 0;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct cw_battery *cw_bat;
	cw_bat = container_of(psy, struct cw_battery, cw_bat); 
#else
//	struct cw_battery *cw_bat = power_supply_get_drvdata(psy);   //temp-del;make error,the cw_bat not use
#endif

	switch(psp) {
	default:
		ret = -EINVAL; 
		break; 
	}

	return ret;
}

static int cw_battery_get_property(struct power_supply *psy,
				enum power_supply_property psp,
				union power_supply_propval *val)
{
	int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct cw_battery *cw_bat;
	cw_bat = container_of(psy, struct cw_battery, cw_bat); 
#else
	struct cw_battery *cw_bat = power_supply_get_drvdata(psy); 
#endif

	switch (psp) {
	case POWER_SUPPLY_PROP_CYCLE_COUNT:
		val->intval = cw_bat->cycle;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		val->intval = cw_bat->ui_soc;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		val->intval= POWER_SUPPLY_HEALTH_GOOD;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		val->intval = cw_bat->voltage <= 0 ? 0 : 1;
		break;  
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		val->intval = cw_bat->voltage * CW_VOL_UNIT;
		break;
	case POWER_SUPPLY_PROP_CURRENT_NOW:
		val->intval = cw_bat->cw_current;
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_TEMP: 
		val->intval = cw_bat->temp;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_NOW:
		val->intval = cw_bat->time_to_full;
		break;
	case POWER_SUPPLY_PROP_TIME_TO_FULL_AVG:
		val->intval = cw_bat->time_to_full;
		break;
	default:
		ret = -EINVAL; 
		break;
	}

	return ret;
}

static enum power_supply_property cw_battery_properties[] = {
	POWER_SUPPLY_PROP_CYCLE_COUNT,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_PRESENT,
	POWER_SUPPLY_PROP_VOLTAGE_NOW,
	POWER_SUPPLY_PROP_CURRENT_NOW,
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_TEMP,
	POWER_SUPPLY_PROP_TIME_TO_FULL_NOW,
	POWER_SUPPLY_PROP_TIME_TO_FULL_AVG,
};
#endif

struct iio_channel ** cw221x_get_ext_channels(struct device *dev,
		const char *const *channel_map, int size)
{
	int i, rc = 0;
	struct iio_channel **iio_ch_ext;

	iio_ch_ext = devm_kcalloc(dev, size, sizeof(*iio_ch_ext), GFP_KERNEL);
	if (!iio_ch_ext)
		return ERR_PTR(-ENOMEM);

	for (i = 0; i < size; i++) {
		iio_ch_ext[i] = devm_iio_channel_get(dev, channel_map[i]);
		if (IS_ERR(iio_ch_ext[i])) {
			rc = PTR_ERR(iio_ch_ext[i]);
			if (rc != -EPROBE_DEFER)
				dev_err(dev, "%s channel unavailable, %d\n",
						channel_map[i], rc);
			return ERR_PTR(rc);
		}
	}
	return iio_ch_ext;
}

bool cw_is_wtchg_chan_valid(struct cw_battery *cw_bat,
 enum wtcharge_iio_channels chan)
{
	int rc;
	struct iio_channel **iio_list;

	if (!cw_bat->wtchg_ext_iio_chans) {
		iio_list = cw221x_get_ext_channels(cw_bat->dev, wtchg_iio_chan_name,
		ARRAY_SIZE(wtchg_iio_chan_name));
		if (IS_ERR(iio_list)) {
			rc = PTR_ERR(iio_list);
			if (rc != -EPROBE_DEFER) {
				dev_err(cw_bat->dev, "Failed to get channels, %d\n", rc);
				cw_bat->wtchg_ext_iio_chans = NULL;
			}
			return false;
		}
		cw_bat->wtchg_ext_iio_chans = iio_list;
	}

	return true;
}

#if 0
static int cw221x_write_iio_prop(struct cw_battery *cw_bat,
	enum cw221x_iio_type type, int channel, int val)
{
	struct iio_channel *iio_chan_list;
	int ret;

	switch(type) {
	case WT_CHG:
		if (!cw_is_wtchg_chan_valid(cw_bat, channel)) {
			return -ENODEV;
		}
		iio_chan_list = cw_bat->wtchg_ext_iio_chans[channel];
		break;
	default:
		pr_err("iio_type %d is not supported\n", type);
		return -EINVAL;
	}
	ret = iio_write_channel_raw(iio_chan_list, val);

	return ret < 0 ? ret : 0;
}
#endif

//+bug 761884, liyiying.wt, add, 2022/7/5, add cw2217 iio function
static int cw2217_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct cw_battery *cw_bat = iio_priv(indio_dev);
	int ret = 0;
	*val1 = 0;

//+bug 782622, liyiying.wt, add, 2022/7/20, shorten the gauge date update time
#if 0//def WT_COMPILE_FACTORY_VERSION
	if (time_is_before_jiffies(cw_bat->last_update + 2 * HZ)) {
		cw_update_data(cw_bat);
		cw_bat->last_update = jiffies;
	}
#endif
//-bug 782622, liyiying.wt, add, 2022/7/20, shorten the gauge date update time

	switch (chan->channel) {
	case PSY_IIO_VOLTAGE_NOW:
		*val1 = cw_bat->voltage * CW_VOL_UNIT;;
		break;
	case PSY_IIO_CURRENT_NOW:
		*val1 = cw_bat->cw_current;
		break;
	case PSY_IIO_TEMP:
		*val1 = cw_bat->temp;
		break;
	case PSY_IIO_CAPACITY:
		*val1 = cw_bat->ui_soc;
		break;
	//+P86801AA1-3622,gudi.wt,20230705,battery protect func
	case PSY_IIO_CYCLE_COUNT:
		cw_get_cycle_count(cw_bat);
		*val1 = cw_bat->cycle;
		break;
	//-P86801AA1-3622,gudi.wt,20230705,battery protect func
	default:
		dev_err(cw_bat->dev, "Unsupported cw2217 IIO chan %d\n", chan->channel);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		dev_err(cw_bat->dev, "Couldn't read IIO channel %d, rc = %d\n",
			chan->channel, ret);
		return ret;
	}

	return IIO_VAL_INT;
}

static int cw2217_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct cw_battery *cw_bat = iio_priv(indio_dev);
	int ret = 0;

	switch (chan->channel) {
	case PSY_IIO_CYCLE_COUNT:
		ret = cw_set_cycle_count(cw_bat, val1);
		break;
	default:
		dev_err(cw_bat->dev, "Unsupported write cw2217 IIO chan %d, default return 2217\n", chan->channel);
		ret = 2217;
		break;
	}
	if (ret < 0)
		pr_err("Couldn't write cw2217_iio_write_raw IIO channel, ret = %d\n", ret);

	return ret;
}

static int cw2217_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct cw_battery *cw_bat = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = cw_bat->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(cw2217_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info cw2217_iio_info = {
	.read_raw	= cw2217_iio_read_raw,
	.write_raw	= cw2217_iio_write_raw,
	.of_xlate	= cw2217_iio_of_xlate,
};


static int cw2217_init_iio_psy(struct cw_battery *cw_bat)
{
	struct iio_dev *indio_dev = cw_bat->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(cw2217_iio_psy_channels);
	int ret, i;

	cw_bat->iio_chan = devm_kcalloc(cw_bat->dev, num_iio_channels,
				sizeof(*cw_bat->iio_chan), GFP_KERNEL);
	if (!cw_bat->iio_chan)
		return -ENOMEM;

	cw_bat->int_iio_chans = devm_kcalloc(cw_bat->dev,
				num_iio_channels,
				sizeof(*cw_bat->int_iio_chans),
				GFP_KERNEL);
	if (!cw_bat->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &cw2217_iio_info;
	indio_dev->dev.parent = cw_bat->dev;
	indio_dev->dev.of_node = cw_bat->dev->of_node;
	indio_dev->name = "cw2217_iio_dev";
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = cw_bat->iio_chan;
	indio_dev->num_channels = num_iio_channels;

	for (i = 0; i < num_iio_channels; i++) {
		cw_bat->int_iio_chans[i].indio_dev = indio_dev;
		chan = &cw_bat->iio_chan[i];
		cw_bat->int_iio_chans[i].channel = chan;
		chan->address = i;
		chan->channel = cw2217_iio_psy_channels[i].channel_num;
		chan->type = cw2217_iio_psy_channels[i].type;
		chan->datasheet_name =
			cw2217_iio_psy_channels[i].datasheet_name;
		chan->extend_name =
			cw2217_iio_psy_channels[i].datasheet_name;
		chan->info_mask_separate =
			cw2217_iio_psy_channels[i].info_mask;
	}

	ret = devm_iio_device_register(cw_bat->dev, indio_dev);
	if (ret)
		dev_err(cw_bat->dev, "Failed to register QG IIO device, rc=%d\n", ret);

	dev_err(cw_bat->dev, "cw2217 IIO device, rc=%d\n", ret);
	return ret;
}
//-bug 761884, liyiying.wt, add, 2022/7/5, add cw2217 iio function

static int cw221X_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	int loop = 0;
	struct cw_battery *cw_bat;
#ifdef CW_PROPERTIES
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	struct power_supply_desc *psy_desc;
	struct power_supply_config psy_cfg = {0};
#endif
#endif
	struct iio_dev *indio_dev;

	cw_printk("\n");

//+bug 761884, liyiying.wt, add, 2022/7/5, add cw2217 iio function
	/*iio init begin*/
	indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*cw_bat));
	//cw_bat = devm_kzalloc(&client->dev, sizeof(*cw_bat), GFP_KERNEL);
	cw_bat = iio_priv(indio_dev);
	if (!cw_bat) {
		printk("%s : cw_bat create fail!\n", __func__);
		return -ENOMEM;
	}
	cw_bat->indio_dev = indio_dev;
	cw_bat->dev = &client->dev;
	/*iio init end*/
//-bug 761884, liyiying.wt, add, 2022/7/5, add cw2217 iio function

	i2c_set_clientdata(client, cw_bat);
	cw_bat->client = client;

	printk("cw221x_get_battery_id!\n");
	cw221x_get_battery_id(cw_bat);
	printk("cw221x_get_battery_id end!\n");

	ret = cw_init(cw_bat);
	while ((loop++ < CW_RETRY_COUNT) && (ret != 0)) {
		msleep(CW_SLEEP_200MS);
		ret = cw_init(cw_bat);
	}
	if (ret) {
		printk("%s : cw221X init fail!\n", __func__);
		//bug 784499, liyiying@wt, add, 2022/8/5, plug usb not reaction
		return -EPROBE_DEFER;
	}

	ret = cw_init_data(cw_bat);
	if (ret) {
		printk("%s : cw221X init data fail!\n", __func__);
		//bug 784499, liyiying@wt, add, 2022/8/5, plug usb not reaction
		return -EPROBE_DEFER;
	}

//+bug 761884, liyiying.wt, add, 2022/7/5, add cw2217 iio function
	/*iio init begin*/
	ret = cw2217_init_iio_psy(cw_bat);
	if (ret < 0) {
		dev_err(cw_bat->dev, "Failed to initialize cw2217 iio psy: %d\n", ret);
//		goto err_1;
	}
	/*iio init end*/
//-bug 761884, liyiying.wt, add, 2022/7/5, add cw2217 iio function

#ifdef CW_PROPERTIES
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	cw_bat->cw_bat.name = CW_PROPERTIES;
	cw_bat->cw_bat.type = POWER_SUPPLY_TYPE_BATTERY;
	cw_bat->cw_bat.properties = cw_battery_properties;
	cw_bat->cw_bat.num_properties = ARRAY_SIZE(cw_battery_properties);
	cw_bat->cw_bat.get_property = cw_battery_get_property;
	cw_bat->cw_bat.set_property = cw_battery_set_property;
	ret = power_supply_register(&client->dev, &cw_bat->cw_bat);
	if (ret < 0) {
		power_supply_unregister(&cw_bat->cw_bat);
		return ret;
	}
#else
	psy_desc = devm_kzalloc(&client->dev, sizeof(*psy_desc), GFP_KERNEL);
	if (!psy_desc)
		return -ENOMEM;
	psy_cfg.drv_data = cw_bat;
	psy_desc->name = CW_PROPERTIES;
	psy_desc->type = POWER_SUPPLY_TYPE_BATTERY;
	psy_desc->properties = cw_battery_properties;
	psy_desc->num_properties = ARRAY_SIZE(cw_battery_properties);
	psy_desc->get_property = cw_battery_get_property;
	psy_desc->set_property = cw_battery_set_property;
	cw_bat->cw_bat = devm_power_supply_register(&client->dev,
					   psy_desc,
					   &psy_cfg);
	if (IS_ERR(cw_bat->cw_bat)) {
		ret = PTR_ERR(cw_bat->cw_bat);
		printk(KERN_ERR"failed to register battery: %d\n", ret);
		return ret;
	}
#endif
#endif

//+bug 782622, liyiying.wt, add, 2022/7/20, shorten the gauge date update time
#ifdef WT_COMPILE_FACTORY_VERSION
	cw_bat->last_update = jiffies;
#endif
//-bug 782622, liyiying.wt, add, 2022/7/20, shorten the gauge date update time

	cw_bat->cwfg_workqueue = create_singlethread_workqueue("cwfg_gauge");
	cw_bat->cwfg_updateworkqueue = create_singlethread_workqueue("cwfg_gaugeupdate");

	INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);
//	INIT_DELAYED_WORK(&cw_bat->battery_iio_update_work, cw_bat_iio_update_work);

	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work , msecs_to_jiffies(queue_start_work_time));
//	queue_delayed_work(cw_bat->cwfg_updateworkqueue, &cw_bat->battery_iio_update_work , msecs_to_jiffies(queue_start_work_time));


	hardwareinfo_set_prop(HARDWARE_BMS_GAUGE, GAUGE_NAME);
 /*+P86801AA1-1797, dingmingyuan.wt, add, 2023/7/19, Identify non-standard batteries*/
	if (cw_bat->batt_id == 1) {
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "P86801_SCUD_LI-ION_4v4_7040mah");
		printk("hardwareinfo_set_prop 1!\n");
	} else if (cw_bat->batt_id == 2) {
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "P86801_Ningde_LI-ION_4v4_7040mah");
		printk("hardwareinfo_set_prop 2!\n");
	} else if (cw_bat->batt_id == 3) {
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "P86801_TBD_LI-ION_4v4_7040mah");
		printk("hardwareinfo_set_prop 3!\n");
	} else {
		hardwareinfo_set_prop(HARDWARE_BATTERY_ID, "Unknow");
		printk("hardwareinfo_set_prop unknow battery!\n");
	}
 /*+P86801AA1-1797, dingmingyuan.wt, add, 2023/7/19, Identify non-standard batteries*/
	cw_printk("cw221X driver probe success!\n");
	return 0;
}

static int cw221X_remove(struct i2c_client *client)	 
{
	cw_printk("\n");
	return 0;
}

#ifdef CONFIG_PM
static int cw_bat_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);

	cw_printk("cw221X cw_bat_suspend\n");

	cancel_delayed_work(&cw_bat->battery_delay_work);
	return 0;
}

static int cw_bat_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);

	cw_printk("cw221X cw_bat_resume\n");

	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(20));
//	queue_delayed_work(cw_bat->cwfg_updateworkqueue, &cw_bat->battery_iio_update_work , msecs_to_jiffies(20));

	return 0;
}

static const struct dev_pm_ops cw_bat_pm_ops = {
	.suspend  = cw_bat_suspend,
	.resume   = cw_bat_resume,
};
#endif

static const struct i2c_device_id cw221X_id_table[] = {
	{ CWFG_NAME, 0 },
	{ }
};

static struct of_device_id cw221X_match_table[] = {
	{ .compatible = "cellwise,cw221X", },
	{ },
};

static struct i2c_driver cw221X_driver = {
	.driver   = {
		.name = CWFG_NAME,
#ifdef CONFIG_PM
		.pm = &cw_bat_pm_ops,
#endif
		.owner = THIS_MODULE,
		.of_match_table = cw221X_match_table,
	},
	.probe = cw221X_probe,
	.remove = cw221X_remove,
	.id_table = cw221X_id_table,
};

/*
	//Add to dsti file
	cw221X@64 { 
		compatible = "cellwise,cw221X";
		reg = <0x64>;
	} 
*/

static int __init cw221X_init(void)
{
	cw_printk("\n");
	i2c_add_driver(&cw221X_driver);
	return 0; 
}

static void __exit cw221X_exit(void)
{
	i2c_del_driver(&cw221X_driver);
}

module_init(cw221X_init);
module_exit(cw221X_exit);

MODULE_AUTHOR("Cellwise FAE");
MODULE_DESCRIPTION("CW221X FGADC Device Driver V0.1");
MODULE_LICENSE("GPL v2");
