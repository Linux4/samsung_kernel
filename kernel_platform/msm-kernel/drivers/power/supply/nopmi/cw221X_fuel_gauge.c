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

#define CWFG_ENABLE_LOG 1 /* CHANGE Customer need to change this for enable/disable log */

#define CW_PROPERTIES "bms"

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
#define USER_RSENSE             5000  /* mhom rsense * 1000  for convenience calculation */

#define queue_delayed_work_time  8000
#define queue_start_work_time    50

#define CW_SLEEP_20MS           20
#define CW_SLEEP_10MS           10
#define CW_UI_FULL              100
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

#define CW221X_NOT_ACTIVE          1
#define CW221X_PROFILE_NOT_READY   2
#define CW221X_PROFILE_NEED_UPDATE 3

#define CW2215_MARK             0x80
#define CW2217_MARK             0x40
#define CW2218_MARK             0x00

#define CW221X_WRITE_TEMP       0

#define TEMP_MAPPING_RAW     0
#define TEMP_MAPPING_REPORT  1
#define TEMP_MAPPING_BASE    270
#define TEMP_MAPPING_STEP    5
#define TEMP_MAPPING_MEMBER_MAX  122

#define cw_printk(fmt, arg...)                                                 \
	{                                                                          \
		if (CWFG_ENABLE_LOG)                                                   \
			pr_err("FG_CW221X : %s-%d : " fmt, __FUNCTION__ ,__LINE__,##arg);  \
		else {}                                                                \
	}
/*
static unsigned char config_profile_info[SIZE_OF_PROFILE] = {
	0x46,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xB0,0xBC,0xC2,0xC6,0xC5,0xC5,0x8C,0x4F,
	0x26,0xFF,0xFF,0xF3,0xC9,0x96,0x7A,0x5C,
	0x4D,0x45,0x38,0x7A,0xB4,0xDB,0xEF,0xCA,
	0xCA,0xCE,0xD1,0xD0,0xCD,0xCB,0xC6,0xCB,
	0xC6,0xC6,0xC8,0xA3,0x97,0x8C,0x85,0x7B,
	0x73,0x76,0x83,0x8E,0xA5,0x8E,0x52,0x4D,
	0x00,0x00,0x57,0x10,0x00,0x82,0xC6,0x00,
	0x00,0x00,0x64,0x10,0x91,0xAE,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0A,
};
*/
static unsigned char config_profile_info_1[SIZE_OF_PROFILE] = {
0x5A,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
0x90,	0x90,	0x90,	0x90,	0x90,	0x90,	0xE5,	0x37,
0x14,	0xB3,	0xB3,	0xB3,	0xB3,	0xB3,	0xB3,	0xB3,
0xA8,	0x6A,	0x51,	0xFF,	0xDA,	0xDC,	0x38,	0xDC,
0xCF,	0xD2,	0xD1,	0xD1,	0xCF,	0xCC,	0xC8,	0xC8,
0xC1,	0xC4,	0xC9,	0xA9,	0x95,	0x88,	0x81,	0x72,
0x68,	0x67,	0x76,	0x8E,	0xA8,	0x8B,	0x55,	0x58,
0x20,	0x00,	0xAB,	0x10,	0x00,	0xA0,	0x70,	0x00,
0x00,	0x00,	0x64,	0xFF,	0xC3,	0x73,	0x00,	0x00,
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0xB8,
};
/*
static unsigned char config_profile_info_2[SIZE_OF_PROFILE] = {
0x5A,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,
0x90,	0x90,	0x90,	0x90,	0x90,	0x90,	0xDF,	0x3C,
0x1A,	0x8D,	0x8D,	0x8D,	0x8D,	0x8D,	0x8D,	0x8D,
0x8A,	0x5C,	0x4E,	0xEB,	0xDC,	0xDC,	0x4B,	0xE1,
0xD7,	0xD5,	0xD4,	0xD2,	0xD0,	0xCC,	0xCA,	0xC8,
0xBF,	0xC3,	0xC8,	0xAB,	0x92,	0x87,	0x78,	0x6A,
0x5F,	0x5A,	0x6C,	0x8B,	0xA4,	0x8F,	0x5F,	0x54,
0x20,	0x00,	0xAB,	0x10,	0x00,	0xA0,	0x5D,	0x00,
0x00,	0x00,	0x64,	0x7C,	0xC3,	0x52,	0x00,	0x00,
0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x00,	0x87,
};
*/
static int temp_mapping_table[][TEMP_MAPPING_MEMBER_MAX] = {
{270,-200}, {275,-170}, {280,-145}, {285,-125}, {290,-105}, {295,-85}, {300,-65}, {305,-45}, {310,-30}, {315,-15},
{320,0}, {325,15}, {330,30}, {335,45}, {340,60}, {345,70}, {350,80}, {355,95}, {360,110}, {365,120},
{370,130}, {375,140}, {380,150}, {385,160}, {390,170}, {395,180}, {400,190}, {405,200}, {410,210}, {415,220},
{420,225}, {425,230}, {430,240}, {435,250}, {440,260}, {445,270}, {450,280}, {455,285}, {460,290}, {465,300},
{470,310}, {475,320}, {480,325}, {485,330}, {490,340}, {495,350}, {500,360}, {505,365}, {510,370}, {515,380},
{520,385}, {525,390}, {530,400}, {535,410}, {540,415}, {545,420}, {550,430}, {555,440}, {560,445}, {565,450},
{570,455}, {575,460}, {580,470}, {585,475}, {590,480}, {595,485}, {600,490}, {605,500}, {610,510}, {615,515},
{620,520}, {625,530}, {630,535}, {635,540}, {640,550}, {645,555}, {650,560}, {655,565}, {660,570}, {665,575},
{670,580}, {675,585}, {680,590}, {685,600}, {690,605}, {695,610}, {700,620}, {705,625}, {710,630}, {715,640},
{720,645}, {725,650}, {730,655}, {735,660}, {740,670}, {745,675}, {750,680}, {755,685}, {760,690}, {765,695},
{770,700}, {775,705}, {780,710}, {785,715}, {790,720}, {795,723}, {800,725}, {805,730}, {810,740}, {815,745}, 
{820,750}, {825,755}, {830,760}, {835,770}, {840,775}, {845,780}, {850,785}, {855,790}, {860,800}, {865,805}, 
{870,810}, {875,820}};

struct cw_battery {
	struct i2c_client *client;

	struct workqueue_struct *cwfg_workqueue;
	struct delayed_work battery_delay_work;

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
	int  temp;
	long cw_current;
	int  cycle;
	int  soh;
	int  fw_version;
};

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

#if CW221X_WRITE_TEMP
static int cw221x_write_temperature(struct i2c_client *client, int temperature)
{
	int ret;
	unsigned char A0_value;
	unsigned char A1_value;
	
	if(temperature < -40 || temperature > 87){
		return -1; //write error temperature
	}
	A0_value = (temperature + 40) * 2;
	ret = cw_read(cw_bat->client, REG_T_HOST_L, &A1_value);
	if (ret < 0)
		return ret;
	A1_value = ~A1_value;
	
	ret = cw_write(cw_bat->client, REG_T_HOST_H, &A0_value);
	if (ret < 0)
		return ret;
	ret = cw_write(cw_bat->client, REG_T_HOST_L, &A1_value);
	if (ret < 0)
		return ret;
	return 0;
}
#endif

/* CW221X iic write profile function */
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
	printk("updatettttttttttttttttttttttttttttt", ret);
	return ret;
}

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
	msleep(CW_SLEEP_20MS);  /* Here delay must >= 20 ms */

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
	msleep(CW_SLEEP_20MS);  /* Here delay must >= 20 ms */

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
 * The VCELL register(0x02 0x03) is an UNSIGNED 14bit read-only register that updates the battery voltage continuously.
 * Battery voltage is measured between the VCELL pin and VSS pin, which is the ground reference. A 14bit
 * sigma-delta A/D converter is used and the voltage resolution is 312.5uV. (0.3125mV is *5/16)
 */
static int cw_get_voltage(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val[2] = {0 , 0};
	unsigned int voltage;

	ret = cw_read_word(cw_bat->client, REG_VCELL_H, reg_val);
	if (ret < 0)
		return ret;
	voltage = (reg_val[0] << 8) + reg_val[1];
	voltage = voltage  * 5 / 16;
	cw_bat->voltage = voltage;

	return 0;
}

/*
 * The SOC register(0x04 0x05) is an UNSIGNED 16bit read-only register that indicates the SOC of the battery. The
 * SOC shows in % format, which means how much percent of the battery's total available capacity is
 * remaining in the battery now. The SOC can intrinsically adjust itself to cater to the change of battery status,
 * including load, temperature and aging etc.
 * The high byte(0x04) contains the SOC in 1% unit which can be directly used if this resolution is good
 * enough for the application. The low byte(0x05) provides more accurate fractional part of the SOC and its
 * LSB is (1/256) %.
 */
static int cw_get_capacity(struct cw_battery *cw_bat)
{
	int ret;
	unsigned char reg_val[2] = { 0, 0 };
	int ui_100 = CW_UI_FULL;
	int soc_h;
	int soc_l;
	int ui_soc;
	int remainder;

	ret = cw_read_word(cw_bat->client, REG_SOC_INT, reg_val);
	if (ret < 0)
		return ret;
	soc_h = reg_val[0];
	soc_l = reg_val[1];
	ui_soc = ((soc_h * 256 + soc_l) * 100)/ (ui_100 * 256);
	remainder = (((soc_h * 256 + soc_l) * 100 * 100) / (ui_100 * 256)) % 100;
	if (ui_soc >= 100){
		cw_printk("CW2015[%d]: UI_SOC = %d larger 100!!!!\n", __LINE__, ui_soc);
		ui_soc = 100;
	}
	cw_bat->ic_soc_h = soc_h;
	cw_bat->ic_soc_l = soc_l;
	cw_bat->ui_soc = ui_soc;

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
	int temp = 0;
	int index = 0;

	ret = cw_read(cw_bat->client, REG_TEMP, &reg_val);
	if (ret < 0)
		return ret;

	temp = (int)reg_val * 10 / 2 - 400;
	
	if (temp < TEMP_MAPPING_BASE) 
	{
		cw_bat->temp = -250;
	}
	else
	{
		index = (temp - TEMP_MAPPING_BASE)/TEMP_MAPPING_STEP;
		cw_bat->temp =  temp_mapping_table[index][TEMP_MAPPING_REPORT];
	}
	
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
	if(((cw_bat->fw_version & CW2215_MARK) != 0) || ((cw_bat->fw_version & CW2217_MARK) != 0)){
		cw_current = cw_current * 1600 / USER_RSENSE;
	}else if(((cw_bat->fw_version) != 0) && ((cw_bat->fw_version & 0xC0) == CW2218_MARK)){
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

	ret = cw_read_word(cw_bat->client, REG_CYCLE_H, reg_val);
	if (ret < 0)
		return ret;

	cycle = (reg_val[0] << 8) + reg_val[1];
	cw_bat->cycle = cycle / 16;

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

static int cw_update_data(struct cw_battery *cw_bat)
{
	int ret = 0;

	ret += cw_get_voltage(cw_bat);
	ret += cw_get_capacity(cw_bat);
	ret += cw_get_temp(cw_bat);
	ret += cw_get_current(cw_bat);
	ret += cw_get_cycle_count(cw_bat);
	ret += cw_get_soh(cw_bat);
	cw_printk("vol = %d  current = %ld cap = %d temp = %d\n", 
		cw_bat->voltage, cw_bat->cw_current, cw_bat->ui_soc, cw_bat->temp);

	return ret;
}

static int cw_init_data(struct cw_battery *cw_bat)
{
	int ret = 0;
	
	ret = cw_get_fw_version(cw_bat);
	if(ret != 0){
		return ret;
	}
	ret += cw_get_chip_id(cw_bat);
	ret += cw_get_voltage(cw_bat);
	ret += cw_get_capacity(cw_bat);
	ret += cw_get_temp(cw_bat);
	ret += cw_get_current(cw_bat);
	ret += cw_get_cycle_count(cw_bat);
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

	ret = cw221X_sleep(cw_bat);
	if (ret < 0)
		return ret;	

	/* update new battery info */
	ret = cw_write_profile(cw_bat->client, config_profile_info_1);
	if (ret < 0)
		return ret;

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
		if (config_profile_info_1[i] != reg_val)
			break;
	}
	if ( i != SIZE_OF_PROFILE)
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
	#endif
	#endif

	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(queue_delayed_work_time));
}

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
	//struct cw_battery *cw_bat = power_supply_get_drvdata(psy); 
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
		ret = cw_get_current(cw_bat);
		if(ret < 0) {
			val->intval = 0;
		} else {
			val->intval = cw_bat->cw_current * 1000;
		}
		break;
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_TEMP:
		val->intval = cw_bat->temp;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL:
		val->intval = 5000000 * cw_bat->soh / 100;
		break;
	case POWER_SUPPLY_PROP_CHARGE_COUNTER:
		val->intval = 5000 *  cw_bat->ui_soc / 100;
		break;
	case POWER_SUPPLY_PROP_CHARGE_FULL_DESIGN:
		val->intval = 5000000;
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
	POWER_SUPPLY_PROP_CHARGE_FULL,
	POWER_SUPPLY_PROP_CHARGE_COUNTER,
};
#endif 

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
	cw_printk("\n");

	cw_bat = devm_kzalloc(&client->dev, sizeof(*cw_bat), GFP_KERNEL);
	if (!cw_bat) {
		printk("%s : cw_bat create fail!\n", __func__);
		return -ENOMEM;
	}
	i2c_set_clientdata(client, cw_bat);
	cw_bat->client = client;
	ret = cw_init(cw_bat);
	while ((loop++ < CW_RETRY_COUNT) && (ret != 0)) {
		msleep(CW_SLEEP_200MS);
		ret = cw_init(cw_bat);
	}
	if (ret) {
		printk("%s : cw221X init fail!\n", __func__);
		return ret;
	}
	ret = cw_init_data(cw_bat);
	if (ret) {
		printk("%s : cw221X init data fail!\n", __func__);
		return ret;
	}
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
	cw_bat->cw_bat = power_supply_register(&client->dev, psy_desc, &psy_cfg);
	if (IS_ERR(cw_bat->cw_bat)) {
		ret = PTR_ERR(cw_bat->cw_bat);
		printk(KERN_ERR"failed to register battery: %d\n", ret);
		return ret;
	}
#endif
#endif
	cw_bat->cwfg_workqueue = create_singlethread_workqueue("cwfg_gauge");
	INIT_DELAYED_WORK(&cw_bat->battery_delay_work, cw_bat_work);
	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work , msecs_to_jiffies(queue_start_work_time));

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

	cancel_delayed_work(&cw_bat->battery_delay_work);
	return 0;
}

static int cw_bat_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cw_battery *cw_bat = i2c_get_clientdata(client);

	queue_delayed_work(cw_bat->cwfg_workqueue, &cw_bat->battery_delay_work, msecs_to_jiffies(20));
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
MODULE_DEVICE_TABLE(i2c, cw221X_id_table);
static struct of_device_id cw221X_match_table[] = {
	{ .compatible = "cellwise,cw221X", },
	{ },
};
MODULE_DEVICE_TABLE(of, cw221X_match_table);
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