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
//Bug547957 gudi.wt,MODIFIY,20200422,P85943 add BMS_GAUGE ic info
#include <linux/hardware_info.h>
//Bug536193,gudi.wt 20200429,add node control queue_delayed_work_time
#include <linux/moduleparam.h>

/*+bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
#define USING_CW2017_BATT

/*+ EXTB04389,xujianbang.wt,20200711,limit value when reading error */
#define DEFAULT_VOLTAGE 4000
#define DEFAULT_CAP 50
#define DEFAULT_TEMP 329
/* -EXTB04389,xujianbang.wt,20200711,limit value when reading error */

#ifdef USING_CW2017_BATT
#define CW2017_INTERRUPT
#endif

#ifdef CW2017_INTERRUPT
#include <linux/pm_wakeup.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#endif

#ifdef USING_CW2017_BATT
struct cw_battery *wt_cw2017;

/*+bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/
#define CAPACITY(min, max, value)			\
		((min > value) ? min : ((value > max) ? max : value))
#define FULL_CAP 100
#define MIN_CAP 0
#define WT_CAP_CAL 98
#define DECIMAL_FULL 255
/*-bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/
#endif
/*-bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/

#define CWFG_ENABLE_LOG 1 //CHANGE   Customer need to change this for enable/disable log
#define CWFG_I2C_BUSNUM 5 //CHANGE   Customer need to change this number according to the principle of hardware
#define DOUBLE_SERIES_BATTERY 0

#define CW_PROPERTIES "cw2017-bat"

#define REG_VERSION             0x00
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
#define REG_VOLT_ID_H           0x0E
#define REG_VOLT_ID_L           0x0F
#define REG_BATINFO             0x10

#define MODE_SLEEP              0x30
#define MODE_NORMAL             0x00
#define MODE_DEFAULT            0xF0
#define CONFIG_UPDATE_FLG       0x80
#define NO_START_VERSION        160

/*bug556458,gudi.wt,2020528,FC:[CW2017]dead_error_condition*/
#define GPIO_CONFIG_SOC_CHANGE_STATUS	 (0x01 << 2)
#define GPIO_CONFIG_MIN_TEMP             (0x00 << 4)
#define GPIO_CONFIG_MAX_TEMP             (0x00 << 5)
#define GPIO_CONFIG_SOC_CHANGE           (0x00 << 6)
#define GPIO_CONFIG_MIN_TEMP_MARK        (0x01 << 4)
#define GPIO_CONFIG_MAX_TEMP_MARK        (0x01 << 5)
#define GPIO_CONFIG_SOC_CHANGE_MARK      (0x01 << 6)
/*+bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
#define ATHD                              0x7F		//0x0
/*-bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
#define DEFINED_MAX_TEMP                          450
#define DEFINED_MIN_TEMP                          0

#define DESIGN_CAPACITY                   4000
#define CWFG_NAME "cw2017"
#define SIZE_BATINFO    80
#define BATTERY_ID_COUNT 2
#define MAX_BATTERY_ID 7
/* +EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
#if defined(CONFIG_BATTERY_AGE_FORECAST)
#define WT_MAX_BATTERY_ID 10
#define WT_BATTERY_ID 5
#endif
/* -EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */

/*+bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/
#ifdef USING_CW2017_BATT
//+Bug536193,gudi.wt 20200429,add node control queue_delayed_work_time
static int queue_delayed_work_time = 30000; //8000
#else
//-Bug536193,gudi.wt 20200429,add node control queue_delayed_work_time
#define queue_delayed_work_time  30000 //8000
#endif
/*-bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/

#define cw_printk(fmt, arg...)        \
	({                                    \
		if(CWFG_ENABLE_LOG){              \
			printk("FG_CW2017 : %s-%d : " fmt, __FUNCTION__ ,__LINE__,##arg);  \
		}else{}                           \
	})     //need check by Chaman



/*+bug538305,xujianbang,20200312,Bringup:Add CW2017 driver.3/3*/
/*+bug545810,xujianbang,20200408,Update CW driver(battery ID/data).*/
/*+bug536193,gudi,20200429,update SCUD-LISHEN battery profile.*/
/*+bug536193,gudi,20200609,update SCUD-ATL battery profile.*/
/*+bug536193,gudi,20200706,update SCUD and ATL battery profile for 3.6V cut off in temp 50.*/
/*battery type enum
  * Type1:SUCD-ATL , ID: 12K
  * Type2:SUCD-LISHEN  , ID: 348K
  */
/* +EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
#if defined(CONFIG_BATTERY_AGE_FORECAST)
static unsigned char config_info[WT_MAX_BATTERY_ID][SIZE_BATINFO] = {
	{
		0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xB7,0xCA,0xAF,0xB1,0xC4,0xB8,0xB4,0x90,
		0x77,0xFF,0xFF,0xDC,0x8E,0x7E,0x65,0x54,
		0x49,0x44,0x3B,0x8A,0xFF,0xDC,0x0C,0xD1,
		0xD0,0xD3,0xD2,0xD3,0xD0,0xCC,0xC9,0xC5,
		0xC2,0xC4,0xC8,0xA8,0x91,0x89,0x7E,0x71,
		0x65,0x66,0x7B,0x8E,0xA5,0x90,0x4D,0x42,
		0x00,0x00,0x70,0x06,0x00,0x00,0x00,0x00,
		0x00,0x00,0x64,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xA8,
	},//SCUD-LISHEN battery
	{
		0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xB7,0xCA,0xAF,0xB1,0xC4,0xB8,0xB4,0x90,
		0x77,0xFF,0xFF,0xDC,0x8E,0x7E,0x65,0x54,
		0x49,0x44,0x3B,0x8A,0xFF,0xDB,0x06,0xCF,
		0xD0,0xD1,0xD1,0xD1,0xCD,0xCA,0xC7,0xC3,
		0xC1,0xC4,0xC2,0xA2,0x8E,0x87,0x79,0x6E,
		0x64,0x67,0x7C,0x8E,0xA3,0x8E,0x4B,0x42,
		0x00,0x00,0x70,0x06,0x00,0x00,0x00,0x00,
		0x00,0x00,0x64,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xE1, //4380
	},
	{
		0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xB7,0xCA,0xAF,0xB1,0xC4,0xB8,0xB4,0x90,
		0x77,0xFF,0xFF,0xDC,0x8E,0x7E,0x65,0x54,
		0x49,0x44,0x3B,0x8A,0xFF,0xD9,0xFF,0xCD,
		0xD0,0xD0,0xD0,0xCE,0xCA,0xC8,0xC4,0xC1,
		0xC1,0xC4,0xB8,0x9C,0x8B,0x84,0x76,0x6B,
		0x62,0x68,0x7C,0x8E,0xA1,0x8C,0x4B,0x42,
		0x00,0x00,0x70,0x06,0x00,0x00,0x00,0x00,
		0x00,0x00,0x64,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x5A,//4360
	},
	{
		0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xB7,0xCA,0xAF,0xB1,0xC4,0xB8,0xB4,0x90,
		0x77,0xFF,0xFF,0xDC,0x8E,0x7E,0x65,0x54,
		0x49,0x44,0x3B,0x8A,0xFF,0xD8,0xFA,0xCD,
		0xCE,0xCE,0xCF,0xCC,0xC8,0xC5,0xC2,0xBE,
		0xC0,0xC4,0xAD,0x95,0x89,0x80,0x74,0x67,
		0x62,0x68,0x7D,0x8E,0x9E,0x8A,0x4B,0x42,
		0x00,0x00,0x70,0x06,0x00,0x00,0x00,0x00,
		0x00,0x00,0x64,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x8E,//4340
	},
	{
		0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xB7,0xCA,0xAF,0xB1,0xC4,0xB8,0xB4,0x90,
		0x77,0xFF,0xFF,0xDC,0x8E,0x7E,0x65,0x54,
		0x49,0x44,0x3B,0x8A,0xFF,0xD6,0x7B,0xCB,
		0xCB,0xCB,0xC8,0xC5,0xC3,0xBF,0xBA,0xBA,
		0xBF,0xB6,0x9A,0x8A,0x83,0x74,0x6A,0x60,
		0x5E,0x6A,0x7F,0x8F,0x98,0x85,0x49,0x42,
		0x00,0x00,0x70,0x06,0x00,0x00,0x00,0x00,
		0x00,0x00,0x64,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xA0,//4290
	},
	{
		0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xAD,0xC2,0xC7,0xC9,0xC6,0xC6,0x85,0x4C,
		0x15,0xFF,0xF5,0xBC,0x91,0x77,0x61,0x4F,
		0x48,0x44,0x3D,0x6D,0xE0,0xDC,0x45,0xE0,
		0xD4,0xD2,0xD2,0xD1,0xCE,0xCB,0xC8,0xC4,
		0xC0,0xC1,0xCB,0xA6,0x94,0x87,0x7F,0x71,
		0x66,0x6D,0x7B,0x8B,0xA3,0x94,0x4D,0x39,
		0x00,0x00,0x70,0x06,0x00,0x00,0x00,0x00,
		0x00,0x00,0x64,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xE1,
	}, //SCUD-ATL battery
	{
		0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xAD,0xC2,0xC7,0xC9,0xC6,0xC6,0x85,0x4C,
		0x15,0xFF,0xF5,0xBC,0x91,0x77,0x61,0x4F,
		0x48,0x44,0x3D,0x6D,0xE0,0xDB,0x42,0xDA,
		0xD3,0xD0,0xD1,0xCF,0xCC,0xC9,0xC6,0xC3,
		0xBE,0xC2,0xC6,0xA1,0x91,0x85,0x7C,0x6E,
		0x66,0x6D,0x7B,0x8B,0xA1,0x93,0x4C,0x39,
		0x00,0x00,0x70,0x06,0x00,0x00,0x00,0x00,
		0x00,0x00,0x64,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2A,//4380
	},
	{
		0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xAD,0xC2,0xC7,0xC9,0xC6,0xC6,0x85,0x4C,
		0x15,0xFF,0xF5,0xBC,0x91,0x77,0x61,0x4F,
		0x48,0x44,0x3D,0x6D,0xE0,0xDA,0x25,0xD4,
		0xD0,0xD0,0xCF,0xCD,0xCA,0xC7,0xC4,0xC0,
		0xBE,0xC3,0xBF,0x9D,0x8E,0x83,0x78,0x6C,
		0x65,0x6D,0x7B,0x8C,0x9F,0x90,0x4C,0x39,
		0x00,0x00,0x70,0x06,0x00,0x00,0x00,0x00,
		0x00,0x00,0x64,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x4E,//4360
	},
	{
		0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xAD,0xC2,0xC7,0xC9,0xC6,0xC6,0x85,0x4C,
		0x15,0xFF,0xF5,0xBC,0x91,0x77,0x61,0x4F,
		0x48,0x44,0x3D,0x6D,0xE0,0xD9,0x0B,0xD0,
		0xCE,0xCF,0xCD,0xCA,0xC8,0xC5,0xC2,0xBC,
		0xBB,0xC5,0xB2,0x98,0x8A,0x81,0x75,0x69,
		0x64,0x6D,0x7C,0x8C,0x9D,0x8E,0x4B,0x39,
		0x00,0x00,0x70,0x06,0x00,0x00,0x00,0x00,
		0x00,0x00,0x64,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x85,//4340
	},
	{
		0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xAD,0xC2,0xC7,0xC9,0xC6,0xC6,0x85,0x4C,
		0x15,0xFF,0xF5,0xBC,0x91,0x77,0x61,0x4F,
		0x48,0x44,0x3D,0x6D,0xE0,0xD6,0x79,0xCB,
		0xCB,0xCA,0xC7,0xC5,0xC2,0xC0,0xB8,0xB6,
		0xBE,0xBE,0x9B,0x8D,0x82,0x76,0x6B,0x62,
		0x63,0x6D,0x7C,0x8C,0x99,0x89,0x48,0x39,
		0x00,0x00,0x70,0x06,0x00,0x00,0x00,0x00,
		0x00,0x00,0x64,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xDB,//4290
	},
};
#else
static unsigned char config_info[BATTERY_ID_COUNT][SIZE_BATINFO] = {
	{
		0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xB7,0xCA,0xAF,0xB1,0xC4,0xB8,0xB4,0x90,
		0x77,0xFF,0xFF,0xDC,0x8E,0x7E,0x65,0x54,
		0x49,0x44,0x3B,0x8A,0xFF,0xDC,0x0C,0xD1,
		0xD0,0xD3,0xD2,0xD3,0xD0,0xCC,0xC9,0xC5,
		0xC2,0xC4,0xC8,0xA8,0x91,0x89,0x7E,0x71,
		0x65,0x66,0x7B,0x8E,0xA5,0x90,0x4D,0x42,
		0x00,0x00,0x70,0x06,0x00,0x00,0x00,0x00,
		0x00,0x00,0x64,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xA8,
	},//SCUD-LISHEN battery
	{
		0x5A,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
		0xAD,0xC2,0xC7,0xC9,0xC6,0xC6,0x85,0x4C,
		0x15,0xFF,0xF5,0xBC,0x91,0x77,0x61,0x4F,
		0x48,0x44,0x3D,0x6D,0xE0,0xDC,0x45,0xE0,
		0xD4,0xD2,0xD2,0xD1,0xCE,0xCB,0xC8,0xC4,
		0xC0,0xC1,0xCB,0xA6,0x94,0x87,0x7F,0x71,
		0x66,0x6D,0x7B,0x8B,0xA3,0x94,0x4D,0x39,
		0x00,0x00,0x70,0x06,0x00,0x00,0x00,0x00,
		0x00,0x00,0x64,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xE1,
	}, //SCUD-ATL battery
};
#endif
/* -EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
/*-bug573878,gudi,20200717,update SCUD and ATL battery profile for 3.3V cut off in temp -10.*/
/*-bug536193,gudi,20200706,update SCUD and ATL battery profile for 3.6V cut off in temp 50.*/
/*-bug536193,gudi,20200609,update SCUD-ATL battery profile.*/
/*-bug536193,gudi,20200429,update SCUD-LISHEN battery profile.*/
/*-bug545810,xujianbang,20200408,Update CW driver(battery ID/data).*/
/*-bug538305,xujianbang,20200312,Bringup:Add CW2017 driver.3/3*/

//static struct power_supply *chrg_usb_psy;
//static struct power_supply *chrg_ac_psy;

struct cw_battery {
    struct i2c_client *client;

    struct workqueue_struct *cwfg_workqueue;
	struct delayed_work battery_delay_work;
	/* +EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
	#if defined(CONFIG_BATTERY_AGE_FORECAST)
	struct delayed_work wt_delay_work;
	#endif
	/* -EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */

/*+bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
#ifdef USING_CW2017_BATT
	#ifdef CW2017_INTERRUPT
	struct delayed_work interrupt_work;
	struct wakeup_source	*cw2017_ws;
	int irq_gpio;
	#endif

	/*+bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/
	//int modify_capacity; /*WT CAL soc*/
	//int soc_decimal; /*decimal data*/
	/*-bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/
#else
	#ifdef CW2017_INTERRUPT
	struct delayed_work interrupt_work;
	#endif
#endif
/*-bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	struct power_supply cw_bat;
#else
	struct power_supply *cw_bat;
#endif
	/* +EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
	#if defined(CONFIG_BATTERY_AGE_FORECAST)
	struct power_supply *battery_psy;
	#endif
	/* -EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
	/*User set*/
	unsigned int design_capacity;
	/*IC value*/
	int version;
	int voltage;
	int capacity;
	int temp;

	/*IC config*/
	unsigned char int_config;
	unsigned char soc_alert;
	int temp_max;
	int temp_min;

	/*Get before profile write*/
	int volt_id;
	/* +EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
	#if defined(CONFIG_BATTERY_AGE_FORECAST)
	int cw_cycle;
	#endif
	/* -EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
	/*Get from charger power supply*/
	//unsigned int charger_mode;

	/*Mark for change cw_bat power_supply*/
	//int change;
};

struct adc_id {
	int id;
	int vold_id_min;
	int vold_id_max;
};

struct adc_id id_table[MAX_BATTERY_ID] =
{
	/*bug536193,gudi,20200429,update SCUD-LISHEN battery profile.*/
	{1, 380, 580},  //12k aTL formula value [425-535 ]
	{0, 1100,1300},  //348k  LISHEN  formula value [1155-1265 ]
};

/* +EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
#if defined(CONFIG_BATTERY_AGE_FORECAST)
struct adc_id cycle_table[WT_BATTERY_ID] =
{
	{0, 0,   299},
	{1, 300, 399},
	{2, 400, 699},
	{3, 700, 999},
	{4, 1000, 999999},
};
#endif
/* -EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */

static int battery_id = 0;

/*Define CW2017 iic read function*/
static int cw_read(struct i2c_client *client, unsigned char reg, unsigned char buf[])
{
	int ret = 0;
	ret = i2c_smbus_read_i2c_block_data( client, reg, 1, buf);
	if(ret < 0){
		printk("IIC error %d\n", ret);
	}
	return ret;
}
/*Define CW2017 iic write function*/
static int cw_write(struct i2c_client *client, unsigned char reg, unsigned char const buf[])
{
	int ret = 0;
	ret = i2c_smbus_write_i2c_block_data( client, reg, 1, &buf[0] );
	if(ret < 0){
		printk("IIC error %d\n", ret);
	}
	return ret;
}
/*Define CW2017 iic read word function*/
static int cw_read_word(struct i2c_client *client, unsigned char reg, unsigned char buf[])
{
	int ret = 0;
	ret = i2c_smbus_read_i2c_block_data( client, reg, 2, buf );
	if(ret < 0){
		printk("IIC error %d\n", ret);
	}
	return ret;
}

/*+bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
#ifdef USING_CW2017_BATT
static int reg_value;
static int wt_cw2017_reg_read(char *buf, const struct kernel_param *kp)
{
	int i, ret = 0;
	unsigned char reg_val = 0;
	char *out = buf;

	for(i = 0;i <= 0x10;i++) {
		ret = cw_read(wt_cw2017->client, (char)i, &reg_val);
		if(ret < 0)
			printk("WT:Could not read REG[0x%x],ret=%d\n", i, ret);
		else{
			reg_value = reg_val;
			printk("WT:REG[0x%x]= 0x%x\n", i, reg_val);
			out += sprintf(out, "0x%x: 0x%x\n", i, reg_val);
		}
	}

    return  (out - buf);
}

static struct kernel_param_ops cw2017_reg_ops = {
    //.set = wt_cw2017_reg_write,
    .get = wt_cw2017_reg_read,
};
module_param_cb(reg_value, &cw2017_reg_ops, &reg_value, 0644);
#endif
/*-bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/

static int cw2017_enable(struct cw_battery *cw_bat)
{
	int ret;
    unsigned char reg_val = MODE_SLEEP;
	printk("cw2017_enable!!!\n");

	ret = cw_write(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if(ret < 0)
		return ret;

	reg_val = MODE_NORMAL;
	ret = cw_write(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if(ret < 0)
		return ret;

	msleep(100);
	return 0;
}


static int cw_get_version(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_val = 0;
	int version = 0;
	ret = cw_read(cw_bat->client, REG_VERSION, &reg_val);
	if(ret < 0)
		return INT_MAX;

	version = reg_val;
	printk("version = %d\n", version);
	return version;
}

static int cw_get_voltage(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_val[2] = {0 , 0};
	unsigned int voltage = 0;

	ret = cw_read_word(cw_bat->client, REG_VCELL_H, reg_val);
	if(ret < 0) {
		/* EXTB04389,xujianbang.wt,20200711,limit value when reading error */
		printk("voltage error set 4000\n");
		return DEFAULT_VOLTAGE;
		//return INT_MAX;
	}

	voltage = (reg_val[0] << 8) + reg_val[1];
	voltage = voltage  * 5 / 16;

	return(voltage);
}

#define DECIMAL_MAX 178 //255 * 0.7
#define DECIMAL_MIN 76  //255 * 0.3
#define UI_FULL 100 //Bug536193,gudi.wt 20200428Modify full charge/recharge with CW2017 patch.
static int cw_get_capacity(struct cw_battery *cw_bat)
{
	//+Bug536193,gudi.wt 20200428Modify full charge/recharge with CW2017 patch.
	int ui_100 = UI_FULL;
	int remainder = 0;
	int UI_SOC = 0;
	//-Bug536193,gudi.wt 20200428Modify full charge/recharge with CW2017 patch.
	int ret = 0;
	unsigned char reg_val = 0;
	int soc = 0;
	int soc_decimal = 0;
	static int reset_loop = 0;
	ret = cw_read(cw_bat->client, REG_SOC_INT, &reg_val);
	if(ret < 0) {
		/* EXTB04389,xujianbang.wt,20200711,limit value when reading error */
		printk("REG_SOC_INT error set 50\n");
		return DEFAULT_CAP;
		//return INT_MAX;
	}
	soc = reg_val;

	ret = cw_read(cw_bat->client, REG_SOC_DECIMAL, &reg_val);
	if(ret < 0) {
		/* EXTB04389,xujianbang.wt,20200711,limit value when reading error */
		printk("REG_SOC_DECIMAL error set 50\n");
		return DEFAULT_CAP;
		//return INT_MAX;
	}
	soc_decimal = reg_val;

	/*+bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/
	#if 0//def USING_CW2017_BATT
	cw_bat->soc_decimal = soc_decimal;
	#endif
	/*-bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/

	if(soc > 100){
		reset_loop++;
		printk("IC error read soc error %d times\n", reset_loop);
		if(reset_loop > 5){
			printk("IC error. please reset IC");
			cw2017_enable(cw_bat); //here need modify
		}
		return cw_bat->capacity;
	}

	//+Bug536193,gudi.wt 20200428Modify full charge/recharge with CW2017 patch.
	UI_SOC = ((soc * 256 + soc_decimal) * 100)/ (ui_100*256);
	remainder = (((soc * 256 + soc_decimal) * 100 * 100) / (ui_100*256)) % 100;

	if(UI_SOC >= 100){
		UI_SOC = 100;
	}else{
		/* case 1 : aviod swing */	//EXTB P210227-01167,taohuayi.wt 20211228 [70,30] ->[90,10].
		if((UI_SOC >= cw_bat->capacity - 1) && (UI_SOC <= cw_bat->capacity + 1)
			&& (remainder > 90 || remainder < 10) ){
			UI_SOC = cw_bat->capacity;
		}
	}

	printk("[cw2017] UI_SOC =%d ,remainder =%d ,soc=%d,capacity=%d \n",UI_SOC,remainder,soc,cw_bat->capacity);

	return UI_SOC;
	//-Bug536193,gudi.wt 20200428Modify full charge/recharge with CW2017 patch.
}

/*+bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/
#if 0//def USING_CW2017_BATT
static int cw_get_capacity_modify(struct cw_battery *cw_bat)
{
	int soc_temp = 0;
	/* WT: soc*10 + decimal *10 */
	soc_temp = cw_bat->capacity * 10 + (cw_bat->soc_decimal * 10) / DECIMAL_FULL;

	/* WT: CAL soc*100/98 */
	soc_temp =  soc_temp * FULL_CAP / WT_CAP_CAL / 10;

	/* WT: Limit [0-100] */
	soc_temp = CAPACITY(MIN_CAP, FULL_CAP, soc_temp);

	/* case 1 : aviod swing */
	if((soc_temp >= cw_bat->modify_capacity - 1)
		&& (soc_temp <= cw_bat->modify_capacity + 1)
		&& (cw_bat->soc_decimal > DECIMAL_MAX
		|| cw_bat->soc_decimal < DECIMAL_MIN) && soc_temp != 100){
		soc_temp = cw_bat->modify_capacity;
	}

	if (soc_temp > cw_bat->modify_capacity) {
		/* SOC increased */
			soc_temp = cw_bat->modify_capacity + 1;
	} else if (soc_temp < cw_bat->modify_capacity) {
		/* SOC dropped */
		soc_temp = cw_bat->modify_capacity - 1;
	}

	return soc_temp;
}
#endif
/*-bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/

/* +bug 561005,gudi.wt,20200604,FC:temp over 55,temperature calibration */
/*+bug536193,gudi.wt,20200627,[FC:CW2017],over 40 temperature calibration*/
#define MIN_TEMP 400
#define MAX_TEMP 450
#define CAL_TEMP 15
/*-bug536193,gudi.wt,20200627,[FC:CW2017],over 40 temperature calibration */
static int cw_get_temp(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_val = 0;
	int temp = 0;
	ret = cw_read(cw_bat->client, REG_TEMP, &reg_val);
	if(ret < 0) {
		/* EXTB04389,xujianbang.wt,20200711,limit value when reading error */
		printk("cw_get_temp error set 329\n");
		return DEFAULT_TEMP;
		//return INT_MAX;
	}

	temp = reg_val * 10 / 2 - 400;
	if((temp >= MIN_TEMP) && (temp <= MAX_TEMP))
	{	/* bug 561005,gudi.wt,20200605,FC:temp over 50-56,temperature calibration */
		temp = temp + ((temp - MIN_TEMP) * CAL_TEMP / (MAX_TEMP - MIN_TEMP));
	}
	else if (temp > MAX_TEMP)
	{
		temp = temp + CAL_TEMP;
	}
	/* -bug 561005,gudi.wt,20200604,FC:temp over 55,temperature calibration */
	return temp;
}

static int cw_get_battery_ID(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_val[2] = {0 , 0};
	long ad_buff = 0;
	int i = 0;

	ret = cw_read_word(cw_bat->client, REG_VOLT_ID_H, reg_val);
	if(ret < 0)
		return INT_MAX;
	ad_buff = (reg_val[0] << 8) + reg_val[1];
	ad_buff = ad_buff  * 5 / 16;

	for(i = 0; i < MAX_BATTERY_ID; i++){
		if(ad_buff >= id_table[i].vold_id_min && ad_buff <= id_table[i].vold_id_max){
			cw_printk("vol=%d cw_get_battery_ID success ID = %d\n", ad_buff,id_table[i].id);
			if(i == 0)
				hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"P85943-QRD-SCUD-ATL-4V4-7040mah");
			else if(i ==  1)
				hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"P85943-QRD-SCUD-LISHEN-4V4-7040mah");
			else
				hardwareinfo_set_prop(HARDWARE_BATTERY_ID,"P85943-QRD-Default-Battery-4V4-7040mah");
			return id_table[i].id;
		}
	}

	printk("WT:error battery_id vol = %d\n", ad_buff);
	return INT_MAX;
}

static void cw_update_data(struct cw_battery *cw_bat)
{
	cw_bat->voltage = cw_get_voltage(cw_bat);
	cw_bat->capacity = cw_get_capacity(cw_bat);
	cw_bat->temp = cw_get_temp(cw_bat);

/*+bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/
#if 0//def USING_CW2017_BATT
	if(cw_bat->modify_capacity == -EINVAL)
		cw_bat->modify_capacity = cw_bat->capacity;

	cw_bat->modify_capacity = cw_get_capacity_modify(cw_bat);
	printk("WT: vol = %d cap = %d m_soc = %d decimal = %d temp = %d\n",
		cw_bat->voltage, cw_bat->capacity, cw_bat->modify_capacity,
		cw_bat->soc_decimal, cw_bat->temp);
#else
	printk("vol = %d  cap = %d temp = %d\n",
		cw_bat->voltage, cw_bat->capacity, cw_bat->temp);
#endif
/*-bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/
}

static int cw_init_data(struct cw_battery *cw_bat)
{
/*+bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
#ifdef USING_CW2017_BATT
	int i, ret = 0;
	unsigned char reg_val = 0;
#endif
/*-bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/

	cw_bat->version = cw_get_version(cw_bat);
	cw_bat->voltage = cw_get_voltage(cw_bat);
	cw_bat->capacity = cw_get_capacity(cw_bat);
	cw_bat->temp = cw_get_temp(cw_bat);
	if(cw_bat->version == INT_MAX){
		return -1;
	}
	printk("ver = %d vol = %d  cap = %d temp = %d\n",
		cw_bat->version, cw_bat->voltage, cw_bat->capacity, cw_bat->temp);

/*+bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
#ifdef USING_CW2017_BATT
	for(i = 0; i <= 0x10; i++) {
		ret = cw_read(cw_bat->client, (char)i, &reg_val);
		if(ret < 0)
			printk("WT:Could not read REG[0x%x],ret=%d\n", i, ret);
		else
			printk("WT:init REG[0x%x]= 0x%x\n", i, reg_val);
	}
#endif
/*-bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/

	/* +EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	cw_bat->cw_cycle = -1;
#endif
	/* -EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */

	return 0;
}

static int cw_init_config(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char reg_gpio_config = 0;
	unsigned char athd = 0;
	unsigned char reg_val = 0;

	cw_bat->design_capacity = DESIGN_CAPACITY;
	/*IC config*/
/*+bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
#ifdef USING_CW2017_BATT
	/*bug556431,gudi.wt,20200528,FC:[CW2017]Wrong operator used CW2017.*/
	cw_bat->int_config = GPIO_CONFIG_SOC_CHANGE_MARK;
#else
	cw_bat->int_config = GPIO_CONFIG_MIN_TEMP | GPIO_CONFIG_MAX_TEMP | GPIO_CONFIG_SOC_CHANGE;
#endif
/*-bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/

	cw_bat->soc_alert = ATHD;
	cw_bat->temp_max = DEFINED_MAX_TEMP;
	cw_bat->temp_min = DEFINED_MIN_TEMP;

	reg_gpio_config = cw_bat->int_config;

	ret = cw_read(cw_bat->client, REG_SOC_ALERT, &reg_val);
	if(ret < 0)
		return ret;

	athd = reg_val & CONFIG_UPDATE_FLG; //clear athd
	athd = athd | cw_bat->soc_alert;
	/*bug556431,gudi.wt,20200528,FC:[CW2017]Wrong operator used CW2017.*/
	if(reg_gpio_config & GPIO_CONFIG_MAX_TEMP_MARK)
	{
		reg_val = (cw_bat->temp_max + 400) * 2 /10;
		ret = cw_write(cw_bat->client, REG_TEMP_MAX, &reg_val); 
		if(ret < 0)
			return ret;
	}
	/*bug556431,gudi.wt,20200528,FC:[CW2017]Wrong operator used CW2017.*/
	if(reg_gpio_config & GPIO_CONFIG_MIN_TEMP_MARK)
	{
		reg_val = (cw_bat->temp_min + 400) * 2 /10;
		ret = cw_write(cw_bat->client, REG_TEMP_MIN, &reg_val); 
		if(ret < 0)
			return ret;
	}

	ret = cw_write(cw_bat->client, REG_GPIO_CONFIG, &reg_gpio_config); 
	if(ret < 0)
		return ret;

	ret = cw_write(cw_bat->client, REG_SOC_ALERT, &athd);
	if(ret < 0)
		return ret;

	return 0;
}

/*CW2017 update profile function, Often called during initialization*/
static int cw_update_config_info(struct cw_battery *cw_bat)
{
	int ret = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;
	unsigned char reg_val1 = 0;
	int count = 0;

	/* update new battery info */
	for(i = 0; i < SIZE_BATINFO; i++)
	{
		reg_val = config_info[battery_id][i];
		ret = cw_write(cw_bat->client, REG_BATINFO + i, &reg_val);
        if(ret < 0)
			return ret;
		printk("w reg[%02X] = %02X\n", REG_BATINFO +i, reg_val);
	}

	ret = cw_read(cw_bat->client, REG_SOC_ALERT, &reg_val);
	if(ret < 0)
		return ret;

	reg_val |= CONFIG_UPDATE_FLG;   /* set UPDATE_FLAG */
	ret = cw_write(cw_bat->client, REG_SOC_ALERT, &reg_val);
	if(ret < 0)
		return ret;

	ret = cw2017_enable(cw_bat);
	if(ret < 0)
		return ret;

	while(cw_get_version(cw_bat) == NO_START_VERSION){
		msleep(100);
		count++;
		if(count > 30)
			break;
	}

	for (i = 0; i < 30; i++) {
        ret = cw_read(cw_bat->client, REG_SOC_INT, &reg_val);
        ret = cw_read(cw_bat->client, REG_SOC_INT + 1, &reg_val1);
		printk("i = %d soc = %d, .soc = %d\n", i, reg_val, reg_val1);
        if (ret < 0)
            return ret;
        else if (reg_val <= 100)
            break;

        msleep(200);
    }

	return 0;
}

/*CW2015 init function, Often called during initialization*/
static int cw_init(struct cw_battery *cw_bat)
{
    int ret;
#ifndef CONFIG_BATTERY_AGE_FORECAST
    int i;
#endif
    unsigned char reg_val = MODE_NORMAL;
	unsigned char config_flg = 0;

	ret = cw_read(cw_bat->client, REG_MODE_CONFIG, &reg_val);
	if(ret < 0)
		return ret;

	ret = cw_read(cw_bat->client, REG_SOC_ALERT, &config_flg);
	if(ret < 0)
		return ret;

	/*+bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
	#ifdef USING_CW2017_BATT
	printk("WT:%s,mode=%d,config_flg=%d\n",__func__,reg_val,config_flg);
	#endif
	/*-bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/

	if(reg_val != MODE_NORMAL || ((config_flg & CONFIG_UPDATE_FLG) == 0x00)){
		ret = cw_update_config_info(cw_bat);
		if(ret < 0)
			return ret;
		/* +bug538123, gudi.wt, 20200707, Add for battery info */
		battery_id = cw_get_battery_ID(cw_bat);
		printk("CW-update cw_get_battery_ID =%d\n",battery_id);
		/* -bug538123, gudi.wt, 20200707, Add for battery info */
	} else {
		battery_id = cw_get_battery_ID(cw_bat);
		if(battery_id == INT_MAX){
			printk("get battery ID error,max!\n");
			battery_id = 0;
		}else if(battery_id >= BATTERY_ID_COUNT){
			printk("get battery ID error,count!\n");
			battery_id = 0;
		}
/* +EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
#ifndef CONFIG_BATTERY_AGE_FORECAST
		for(i = 0; i < SIZE_BATINFO; i++)
		{
			ret = cw_read(cw_bat->client, REG_BATINFO +i, &reg_val);
			if(ret < 0)
				return ret;

			if(config_info[battery_id][i] != reg_val)
			{
				break;
			}
		}
		if(i != SIZE_BATINFO)
		{
			//"update flag for new battery info need set"
			ret = cw_update_config_info(cw_bat);
			if(ret < 0)
				return ret;
		}
#endif
/* -EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
	}
	cw_printk("cw2017 init success!\n");
	return 0;
}

/* +EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
#if defined(CONFIG_BATTERY_AGE_FORECAST)
int cw_get_prop_from_battery(struct cw_battery *cw,
	enum power_supply_property psp, union power_supply_propval *val)
{
	int rc = 0;

	if (!cw->battery_psy){
		cw->battery_psy = power_supply_get_by_name("battery");
	}

	if(cw->battery_psy && psp == POWER_SUPPLY_PROP_BATTERY_CYCLE)
		rc = power_supply_get_property(cw->battery_psy, psp, val);

	return rc;
}

static void wt_batt_cycle_work(struct work_struct *work)
{
	struct delayed_work *delay_work = container_of(work, struct delayed_work, work);
	struct cw_battery *cw_bat = container_of(delay_work, struct cw_battery, wt_delay_work);
	int num = 0;
	unsigned char reg_val = 0;
	int i, ret;
	union power_supply_propval val;

	cw_get_prop_from_battery(cw_bat, POWER_SUPPLY_PROP_BATTERY_CYCLE, &val);
	cw_printk("wt wt_batt_cycle_work %d\n", val.intval);
	if(cw_bat->cw_cycle == val.intval){
		return ;
	}

	if(val.intval > 999999 || val.intval < 0){
		val.intval = 0;
	}
	cw_bat->cw_cycle = val.intval;
	for (i = 0; i < 5; i++){
		if(cw_bat->cw_cycle >= cycle_table[i].vold_id_min &&
			cw_bat->cw_cycle <= cycle_table[i].vold_id_max){
			num = cycle_table[i].id;
			break;
		}
	}
	battery_id = cw_get_battery_ID(cw_bat);
	if(battery_id == 0)
		battery_id = num;
	if(battery_id == 1)
		battery_id = num + 5;
	cw_printk("cw_bat->cw_cycle = %d cw_get_battery_ID num = %d,battery_id = %d\n",
				cw_bat->cw_cycle, num, battery_id);
	for(i = 0; i < SIZE_BATINFO; i++)
	{
		ret = cw_read(cw_bat->client, REG_BATINFO +i, &reg_val);
		if(ret < 0)
			return;

		if(config_info[battery_id][i] != reg_val)
			break;
	}

	if(i != SIZE_BATINFO)
	{
		cw_printk("update wt cw\n");
		//"update flag for new battery info need set"
		ret = cw_update_config_info(cw_bat);
		if(ret < 0)
			return;
	}

};
#endif
/* -EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */

static void cw_bat_work(struct work_struct *work)
{
    struct delayed_work *delay_work;
    struct cw_battery *cw_bat;

    delay_work = container_of(work, struct delayed_work, work);
    cw_bat = container_of(delay_work, struct cw_battery, battery_delay_work);

	cw_update_data(cw_bat);

/*+bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
#ifdef USING_CW2017_BATT
	/*Not update*/
#else
	#ifdef CW_PROPERTIES
	#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	power_supply_changed(&cw_bat->cw_bat);
	#else
	power_supply_changed(cw_bat->cw_bat);
	#endif
	#endif
#endif
/*-bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/

	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(queue_delayed_work_time));
}

//+Bug536193,gudi.wt 20200429,add node control queue_delayed_work_time
static int queue_delayed_work_time_set(const char *val, const struct kernel_param *kp)
{
	int ret;

	ret = param_set_int(val, kp);
	if (ret) {
		pr_err("error setting value %d\n", ret);
		return ret;
	}

	if (wt_cw2017) {
		printk("queue_delayed_work_time_set to %d\n", queue_delayed_work_time);
		cancel_delayed_work_sync(&wt_cw2017->battery_delay_work);
		schedule_delayed_work(&wt_cw2017->battery_delay_work,
					  round_jiffies_relative(msecs_to_jiffies
								 (queue_delayed_work_time)));
		return 0;
	}

	return 0;
}

static struct kernel_param_ops queue_delayed_work_time_ops = {
	.set = queue_delayed_work_time_set,
	.get = param_get_int,
};
/*new function in new andriod version*/
module_param_cb(queue_delayed_work_time, &queue_delayed_work_time_ops, &queue_delayed_work_time, 0644);
//-Bug536193,gudi.wt 20200429,add node control queue_delayed_work_time

#ifdef CW_PROPERTIES
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
    case POWER_SUPPLY_PROP_CAPACITY:
        /*+bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/
        #if  0//def USING_CW2017_BATT
            val->intval = cw_bat->modify_capacity;
        #else
            val->intval = cw_bat->capacity;
        #endif
        /*-bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/
            break;

	/*
    case POWER_SUPPLY_PROP_STATUS:   //Chaman charger ic will give a real value
            val->intval = cw_bat->status;
            break;
    */
    case POWER_SUPPLY_PROP_HEALTH:   //Chaman charger ic will give a real value
            val->intval= POWER_SUPPLY_HEALTH_GOOD;
            break;
    case POWER_SUPPLY_PROP_PRESENT:
            val->intval = cw_bat->voltage <= 0 ? 0 : 1;
            break;

    case POWER_SUPPLY_PROP_VOLTAGE_NOW:
            val->intval = cw_bat->voltage * 1000;
            break;

    case POWER_SUPPLY_PROP_TECHNOLOGY:  //Chaman this value no need
            val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
            break;

    case POWER_SUPPLY_PROP_TEMP:
            val->intval = cw_bat->temp;
            break;

/*    case POWER_SUPPLY_PROP_TEMP_ALERT_MIN:
            val->intval = cw_bat->temp_min;
            break;

    case POWER_SUPPLY_PROP_TEMP_ALERT_MAX:
            val->intval = cw_bat->temp_max;
            break;*/

    case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
            val->intval = cw_bat->design_capacity;
            break;

    default:
			ret = -EINVAL;
            break;
    }
    return ret;
}

/* +EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
#if defined(CONFIG_BATTERY_AGE_FORECAST)
static int cw_battery_set_property(struct power_supply *psy,
	enum power_supply_property prop, const union power_supply_propval *val)
{
	int ret = 0;
	struct cw_battery *cw_bat = power_supply_get_drvdata(psy);

	switch (prop) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		if(val->intval == 1) {
			pr_info("WT set battery protect\n");
			cancel_delayed_work(&cw_bat->wt_delay_work);
			schedule_delayed_work(&cw_bat->wt_delay_work,
				round_jiffies_relative(msecs_to_jiffies(500)));
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}
static int cw_battery_prop_is_writeable(struct power_supply *psy,
		enum power_supply_property psp)
{
	switch (psp) {
	case POWER_SUPPLY_PROP_VOLTAGE_NOW:
		return 1;
	default:
		break;
	}

	return 0;
}
#endif
/* -EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */

static enum power_supply_property cw_battery_properties[] = {
    POWER_SUPPLY_PROP_CAPACITY,
    //POWER_SUPPLY_PROP_STATUS,
    POWER_SUPPLY_PROP_HEALTH,
    POWER_SUPPLY_PROP_PRESENT,
    POWER_SUPPLY_PROP_VOLTAGE_NOW,
    POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_TEMP,
//	POWER_SUPPLY_PROP_TEMP_ALERT_MIN,
//	POWER_SUPPLY_PROP_TEMP_ALERT_MAX,
	POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN,
};
#endif

/*+bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
#ifdef USING_CW2017_BATT
#ifdef CW2017_INTERRUPT
static void interrupt_work_do_wakeup(struct work_struct *work)
{
	struct delayed_work *delay_work;
	struct cw_battery *cw_bat;
	int ret = 0;
	unsigned char reg_val = 0;

	delay_work = container_of(work, struct delayed_work, work);
	cw_bat = container_of(delay_work, struct cw_battery, interrupt_work);

	ret = cw_read(cw_bat->client, REG_GPIO_CONFIG, &reg_val);
	if(ret < 0) {
		printk("WT:Could not read REG_GPIO_CONFIG!ret=%d\n", reg_val);
	}
	else {
		printk("WT:wakeup read REG_GPIO_CONFIG  0x%x!\n", reg_val);
		/*bug556458,gudi.wt,2020528,FC:[CW2017]dead_error_condition*/
		if (reg_val & GPIO_CONFIG_SOC_CHANGE_STATUS)
			power_supply_changed(cw_bat->cw_bat);
	}
	/*bug556458,gudi.wt,20200528,FC:[CW2017]dead_error_condition:*/
	reg_val = GPIO_CONFIG_SOC_CHANGE_MARK;

	ret = cw_write(cw_bat->client, REG_GPIO_CONFIG, &reg_val);
	if(ret < 0) {
		printk("WT: clean REG_GPIO_CONFIG error ret=%d\n", ret);
	}

	/*WT:  relax wakelock when work finish*/
	__pm_relax(cw_bat->cw2017_ws);
}

static irqreturn_t ops_cw2017_int_handler_int_handler(int irq, void *dev_id)
{
	struct cw_battery *cw_bat = dev_id;

	__pm_stay_awake(cw_bat->cw2017_ws);
	printk("%s : cw2017 ops_cw2017_int_handler_int_handler\n", __func__);
	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->interrupt_work, msecs_to_jiffies(20));
	//__pm_relax(cw_bat->cw2017_ws);

	return IRQ_HANDLED;
}
#endif

#else
/*CW origin code. Error code*/
#ifdef CW2017_INTERRUPT

#define WAKE_LOCK_TIMEOUT       (10 * HZ)
static struct wake_lock cw2017_wakelock;

static void interrupt_work_do_wakeup(struct work_struct *work)
{
        struct delayed_work *delay_work;
        struct cw_battery *cw_bat;
		int ret = 0;
		unsigned char reg_val = 0;

        delay_work = container_of(work, struct delayed_work, work);
        cw_bat = container_of(delay_work, struct cw_battery, interrupt_work);

		ret = cw_read(cw_bat->client, REG_GPIO_CONFIG, &reg_val);
		if(ret < 0)
			return ret;
		/**/
}

static irqreturn_t ops_cw2017_int_handler_int_handler(int irq, void *dev_id)
{
        struct cw_battery *cw_bat = dev_id;
        wake_lock_timeout(&cw2017_wakelock, WAKE_LOCK_TIMEOUT);
        queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->interrupt_work, msecs_to_jiffies(20));
        return IRQ_HANDLED;
}
#endif
#endif
/*-bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/

static int cw2017_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret;
	int loop = 0;
	struct cw_battery *cw_bat;

/*+bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
#ifdef USING_CW2017_BATT
#ifdef CW2017_INTERRUPT
	struct device_node *np = client->dev.of_node;
	int irq_flags = 0;
#endif
#else
#ifdef CW2017_INTERRUPT
	int irq = 0;
#endif
#endif
/*-bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/

#ifdef CW_PROPERTIES
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 0)
	struct power_supply_desc *psy_desc;
	struct power_supply_config psy_cfg = {0};
#endif
#endif
    //struct device *dev;
	cw_printk("\n");
	cw_printk("cw2017_probe--wt\n");

    cw_bat = devm_kzalloc(&client->dev, sizeof(*cw_bat), GFP_KERNEL);
    if (!cw_bat) {
		cw_printk("cw_bat create fail!\n");
        return -ENOMEM;
    }

    i2c_set_clientdata(client, cw_bat);

    cw_bat->client = client;
    cw_bat->volt_id = 0;

/*+bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/
#if 0//def USING_CW2017_BATT
    cw_bat->modify_capacity = -EINVAL;
#endif
/*-bug548894,xujianbang.wt,20200421,FC:[CW2017]Modify full charge/recharge*/

    ret = cw_init(cw_bat);
    while ((loop++ < 3) && (ret != 0)) {
		msleep(200);
        ret = cw_init(cw_bat);
    }
    if (ret) {
		printk("%s : cw2017 init fail!\n", __func__);
        return ret;
    }

	ret = cw_init_config(cw_bat);
	if (ret) {
		printk("%s : cw2017 init config fail!\n", __func__);
		return ret;
	}

	ret = cw_init_data(cw_bat);
    if (ret) {
		printk("%s : cw2017 init data fail!\n", __func__);
        return ret;
    }

#ifdef CW_PROPERTIES
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 1, 0)
	cw_bat->cw_bat.name = CW_PROPERTIES;
	/*bug538305,xujianbang,20200310,Bringup:Add CW2017 driver.*/
	cw_bat->cw_bat.type = POWER_SUPPLY_TYPE_BMS;//POWER_SUPPLY_TYPE_BATTERY;
	cw_bat->cw_bat.properties = cw_battery_properties;
	cw_bat->cw_bat.num_properties = ARRAY_SIZE(cw_battery_properties);
	cw_bat->cw_bat.get_property = cw_battery_get_property;
	ret = power_supply_register(&client->dev, &cw_bat->cw_bat);
	if(ret < 0) {
	    power_supply_unregister(&cw_bat->cw_bat);
	    return ret;
	}
#else
	psy_desc = devm_kzalloc(&client->dev, sizeof(*psy_desc), GFP_KERNEL);
	if (!psy_desc)
		return -ENOMEM;

	psy_cfg.drv_data = cw_bat;
	psy_desc->name = CW_PROPERTIES;
	/*bug538305,xujianbang,20200310,Bringup:Add CW2017 driver.*/
	psy_desc->type = POWER_SUPPLY_TYPE_BMS;//POWER_SUPPLY_TYPE_BATTERY;
	psy_desc->properties = cw_battery_properties;
	psy_desc->num_properties = ARRAY_SIZE(cw_battery_properties);
	psy_desc->get_property = cw_battery_get_property;
	/* +EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	psy_desc->set_property = cw_battery_set_property;
	psy_desc->property_is_writeable = cw_battery_prop_is_writeable,
#endif
	/* -EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
	cw_bat->cw_bat = power_supply_register(&client->dev, psy_desc, &psy_cfg);
	if(IS_ERR(cw_bat->cw_bat)) {
		ret = PTR_ERR(cw_bat->cw_bat);
	    printk(KERN_ERR"failed to register battery: %d\n", ret);
	    return ret;
	}
#endif
#endif

	cw_bat->cwfg_workqueue = create_singlethread_workqueue("cwfg_gauge");
	INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);
	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work , msecs_to_jiffies(50));
	/* +EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
#if defined(CONFIG_BATTERY_AGE_FORECAST)
	INIT_DELAYED_WORK(&cw_bat->wt_delay_work, wt_batt_cycle_work);
	schedule_delayed_work(&cw_bat->wt_delay_work, round_jiffies_relative(msecs_to_jiffies(120000)));
	cw_bat->battery_psy = power_supply_get_by_name("battery");
#endif
	/* -EXTB P200521-06425 caijiaqi.wt,20200814, Add, battery protect */
/*+bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/
#ifdef USING_CW2017_BATT
#ifdef CW2017_INTERRUPT
	INIT_DELAYED_WORK(&cw_bat->interrupt_work, interrupt_work_do_wakeup);

	cw_bat->cw2017_ws = wakeup_source_register(&client->dev,"qcom-cw2017_battery");
	if (!cw_bat->cw2017_ws) {
		cw_printk("cw2017_ws not success!\n");
		wakeup_source_unregister(cw_bat->cw2017_ws);
	}

	cw_bat->irq_gpio = of_get_named_gpio(np, "irq-gpio", 0);
	if (cw_bat->irq_gpio < 0) {
		pr_err("%s: no irq gpio provided.\n", __func__);
	} else {
		pr_err("%s: irq gpio provided ok.gpio=%d\n", __func__,cw_bat->irq_gpio);
	}

    if (gpio_is_valid(cw_bat->irq_gpio)) {
        ret = devm_gpio_request_one(&client->dev, cw_bat->irq_gpio,
            GPIOF_DIR_IN, "cw2017_irq");
        if (ret) {
            pr_err("%s: cw_irq request failed ret=%d\n", __func__,ret);
        }
		else
			pr_err("%s: cw_irq request OK ret=%d\n", __func__,ret);
    }
    else
        pr_err("%s: cw2017_irq invalid\n", __func__);

    /* CW2017 IRQ */
    if (gpio_is_valid(cw_bat->irq_gpio)) {
        /* Register irq handler */
        irq_flags = IRQF_TRIGGER_FALLING | IRQF_ONESHOT;
        ret = devm_request_threaded_irq(&client->dev,
                    gpio_to_irq(cw_bat->irq_gpio),
                    NULL, ops_cw2017_int_handler_int_handler, irq_flags,
                    "cw2017", cw_bat);
        if (ret != 0) {
            pr_err("%s: failed to request IRQ %d: %d\n",
                    __func__, gpio_to_irq(cw_bat->irq_gpio), ret);
        }
		else
			pr_err("%s: OK to request IRQ %d: %d\n",
                    __func__, gpio_to_irq(cw_bat->irq_gpio), ret);
    }
#endif
	/*WT: For filenote reading*/
	wt_cw2017 = cw_bat;

#else
#ifdef CW2017_INTERRUPT
	INIT_DELAYED_WORK(&cw_bat->interrupt_work, interrupt_work_do_wakeup);

	wake_lock_init(&cw2017_wakelock, WAKE_LOCK_SUSPEND, "cw2017_detect");
	if (client->irq > 0) {
			irq = client->irq;
			ret = request_irq(irq, ops_cw2017_int_handler_int_handler, IRQF_TRIGGER_FALLING, "cw2017_detect", cw_bat);
			if (ret < 0) {
					printk(KERN_ERR"fault interrupt registration failed err = %d\n", ret);
			}
			enable_irq_wake(irq);
	}
#endif
#endif
/*-bug546434,xujianbang.wt,20200410,FC:[CW2017]Enable irq of CW2017.*/

	//Bug547957 gudi.wt,MODIFIY,20200422,P85943 add BMS_GAUGE ic info
	hardwareinfo_set_prop(HARDWARE_BMS_GAUGE,"cw2017");

	cw_printk("cw2017 driver probe success!\n");
    return 0;
}

static int cw2017_remove(struct i2c_client *client)
{
	cw_printk("\n");
	return 0;
}

#ifdef CONFIG_PM
static int cw_bat_suspend(struct device *dev)
{
        struct i2c_client *client = to_i2c_client(dev);
        struct cw_battery *cw_bat = i2c_get_clientdata(client);
        cancel_delayed_work(&cw_bat->battery_delay_work);
        return 0;
}

static int cw_bat_resume(struct device *dev)
{
        struct i2c_client *client = to_i2c_client(dev);
        struct cw_battery *cw_bat = i2c_get_clientdata(client);
        queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(2));
        return 0;
}

static const struct dev_pm_ops cw_bat_pm_ops = {
        .suspend  = cw_bat_suspend,
        .resume   = cw_bat_resume,
};
#endif

static const struct i2c_device_id cw2017_id_table[] = {
	{CWFG_NAME, 0},
	{}
};

static struct of_device_id cw2017_match_table[] = {
	{ .compatible = "cellwise,cw2017", },
	{ },
};

static struct i2c_driver cw2017_driver = {
	.driver 	  = {
		.name = CWFG_NAME,
#ifdef CONFIG_PM
        .pm     = &cw_bat_pm_ops,
#endif
		.owner	= THIS_MODULE,
		.of_match_table = cw2017_match_table,
	},
	.probe		  = cw2017_probe,
	.remove 	  = cw2017_remove,
	.id_table = cw2017_id_table,
};

/*
static struct i2c_board_info __initdata fgadc_dev = {
	I2C_BOARD_INFO(CWFG_NAME, 0x63)
};
*/

static int __init cw2017_init(void)
{
	//struct i2c_client *client;
	//struct i2c_adapter *i2c_adp;
	cw_printk("\n");

    //i2c_register_board_info(CWFG_I2C_BUSNUM, &fgadc_dev, 1);
	//i2c_adp = i2c_get_adapter(CWFG_I2C_BUSNUM);
	//client = i2c_new_device(i2c_adp, &fgadc_dev);

    i2c_add_driver(&cw2017_driver);
    return 0;
}

/*
	//Add to dsti file
	cw2017@63 {
		compatible = "cellwise,cw2017";
		reg = <0x63>;
	}
*/

static void __exit cw2017_exit(void)
{
    i2c_del_driver(&cw2017_driver);
}

module_init(cw2017_init);
module_exit(cw2017_exit);

MODULE_AUTHOR("Chaman Qi");
MODULE_DESCRIPTION("CW2017 FGADC Device Driver V0.5");
MODULE_LICENSE("GPL");
