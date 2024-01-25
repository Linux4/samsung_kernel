/*****************************************************************************
* Copyright(c) BMT, 2021. All rights reserved.
*
* BMT [oz8806] Source Code Reference Design
* File:[bmulib.c]
*
* This Source Code Reference Design for BMT [oz8806] access
* ("Reference Design") is solely for the use of PRODUCT INTEGRATION REFERENCE ONLY,
* and contains confidential and privileged information of BMT International
* Limited. BMT shall have no liability to any PARTY FOR THE RELIABILITY,
* SERVICEABILITY FOR THE RESULT OF PRODUCT INTEGRATION, or results from: (i) any
* modification or attempted modification of the Reference Design by any party, or
* (ii) the combination, operation or use of the Reference Design with non-BMT
* Reference Design. Use of the Reference Design is at user's discretion to qualify
* the final work result.
*****************************************************************************/

#ifndef _PARAMETER_H_
#define _PARAMETER_H_

/****************************************************************************
* #include section
* add #include here if any
***************************************************************************/
#include "table.h"
#include <linux/power_supply.h>
#include <linux/suspend.h>
#include <linux/qti_power_supply.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/consumer.h>
#include <linux/module.h>
/****************************************************************************
* #define section
* add constant #define here if any
***************************************************************************/
#define MYDRIVER	"sd77428"

#define DISCH_CURRENT_TH    -10
#define O2_OCV_100_VOLTAGE  4350
#define TEMPERATURE_DATA_NUM 28

#define num_0      	0
#define num_1		1
#define num_32768  	32768
#define num_5		5
#define num_0x2c	0x2c
#define num_0x40	0x40
#define num_0x20	0x20
#define num_1000	1000
#define num_50		50
#define num_100	100
#define num_10		10
#define num_7		7
#define num_2		2
#define num_1000	1000
#define num_0x2f	0x2f
#define num_0x28	0x28
#define num_0x07	0x07
#define num_6		6
#define num_10		10
#define num_20		20
#define num_9		9
#define num_95		95
#define num_0x4c	0x4c
#define num_0x40	0x40
#define num_3		3
#define num_99		99
#define num_15		15
#define num_25		25
#define num_0x80	0x80

#define INIT_CAP (-2)
#define NO_FILE (-1)
#define PEC_ENABLE 1
/*[battery config]*/
//#define	EXT_THERMAL_READ
#define OZ8806_VOLTAGE  		4400
#define O2_CONFIG_CAPACITY  		6973
#define O2_CONFIG_RSENSE  		10000//Expanded 1000 times
#define O2_CONFIG_EOC  		350
#define OZ8806_EOD 			3450
/*
 * Rsense - Board_offset
 * 5mR    -  42
 * 10mR   -  22
 * 20mR   -  11
 * */
#if   O2_CONFIG_RSENSE == 5000
   #define O2_CONFIG_BOARD_OFFSET          -10 //42
#elif O2_CONFIG_RSENSE == 10000
   //#define O2_CONFIG_BOARD_OFFSET          22
   #define O2_CONFIG_BOARD_OFFSET          -2
#elif O2_CONFIG_RSENSE == 20000
   #define O2_CONFIG_BOARD_OFFSET          11
#endif

#define O2_SOC_START_THRESHOLD_VOL	3700
#define O2_CONFIG_RECHARGE		100
#define O2_TEMP_PULL_UP_R		47000 //电池ntc的上拉电阻
#define O2_TEMP_REF_VOLTAGE		1500 //mv
#define BATTERY_WORK_INTERVAL		10

#define RPULL (68000)
#define RDOWN (5100)

#define ENABLE_10MIN_END_CHARGE_FUN

#define BMT_PROPERTIES
#define BMT_PROPERTIES_NAME "bms"
struct oz8806_iio_channels {
	const char *datasheet_name;
	int channel_num;
	enum iio_chan_type type;
	long info_mask;
};

#define OZ8806_IIO_CHAN(_name, _num, _type, _mask)		\
	{						\
		.datasheet_name = _name,		\
		.channel_num = _num,			\
		.type = _type,				\
		.info_mask = _mask,			\
	},

#define OZ8806_CHAN_ENERGY(_name, _num)			\
	OZ8806_IIO_CHAN(_name, _num, IIO_ENERGY,		\
		BIT(IIO_CHAN_INFO_PROCESSED))

static const struct oz8806_iio_channels oz8806_iio_psy_channels[] = {
	OZ8806_CHAN_ENERGY("present", PSY_IIO_PRESENT)
	OZ8806_CHAN_ENERGY("status", PSY_IIO_STATUS)
	OZ8806_CHAN_ENERGY("capacity", PSY_IIO_CAPACITY)
	OZ8806_CHAN_ENERGY("current_now", PSY_IIO_CURRENT_NOW)
	OZ8806_CHAN_ENERGY("voltage_now", PSY_IIO_VOLTAGE_NOW)
	OZ8806_CHAN_ENERGY("voltage_max", PSY_IIO_VOLTAGE_MAX)
	OZ8806_CHAN_ENERGY("charge_full", PSY_IIO_CHARGE_FULL)
	OZ8806_CHAN_ENERGY("resistance_id", PSY_IIO_RESISTANCE_ID)
	OZ8806_CHAN_ENERGY("temp", PSY_IIO_TEMP)
	OZ8806_CHAN_ENERGY("cycle_count", PSY_IIO_CHARGE_COUNTER)
	OZ8806_CHAN_ENERGY("charge_full_design", PSY_IIO_CHARGE_FULL_DESIGN)
	OZ8806_CHAN_ENERGY("time_to_full_now", PSY_IIO_TIME_TO_FULL_NOW)
	OZ8806_CHAN_ENERGY("therm_curr", PSY_IIO_MAIN_FCC_MAX)
	OZ8806_CHAN_ENERGY("chip_ok", PSY_IIO_BMS_CHIP_OK)
	OZ8806_CHAN_ENERGY("battery_auth", PSY_IIO_BATTERY_AUTH)
	OZ8806_CHAN_ENERGY("soc_decimal", PSY_IIO_BMS_SOC_DECIMAL)
	OZ8806_CHAN_ENERGY("soc_decimal_rate", PSY_IIO_BMS_SOC_DECIMAL_RATE)
	OZ8806_CHAN_ENERGY("battery_id", PSY_IIO_BATTERY_ID)
};

enum sgm41542_iio_channels {
	CHG_STATUS,
	CHG_TYPE,
	ONLINE,
};

static const char * const sgm41542_iio_chan_name[] = {
	[CHG_STATUS] = "charger_status",
	[CHG_TYPE] = "charger_type",
	[ONLINE] = "main_charge_vbus_online",
};

/****************************************************************************
* Struct section
*  add struct #define here if any
***************************************************************************/
enum charger_type_t {
	O2_CHARGER_UNKNOWN = -1,
	O2_CHARGER_BATTERY = 0,
	O2_CHARGER_USB = 1,
	O2_CHARGER_AC = 4,
};

struct batt_data {
	int32_t	batt_soc;//Relative State Of Charged, present percentage of battery capacity
	int32_t	batt_voltage;//Voltage of battery, in mV
	int32_t	batt_current;//Current of battery, in mA; plus value means charging, minus value means discharging
	int32_t	batt_temp;//Temperature of battery
	int32_t	batt_capacity;//adjusted residual capacity
	int32_t	batt_fcc_data;
	int32_t	discharge_current_th;
 	uint8_t	charge_end;
	uint32_t cycle;
};

struct oz8806_data
{
	struct device *dev;
	struct iio_dev *indio_dev;
	struct iio_chan_spec *iio_chan;
	struct iio_channel	*int_iio_chans;
	struct iio_channel	**sgm41542_ext_iio_chans;
//	struct iio_channel	**pmic_ext_iio_chans;
	struct iio_channel	**wtchg_ext_iio_chans;
	struct iio_channel *batt_id_vol;
	struct power_supply *bat;
	struct power_supply *ac_psy;
	struct power_supply *usb_psy;
	struct delayed_work work;
//	struct delayed_work batt_id_detect_work;
	uint32_t interval;
	struct i2c_client	*myclient;
	struct mutex i2c_rw_lock;
	struct notifier_block pm_nb;		//alternative suspend/resume method
	struct batt_data batt_info;
	int32_t status;
	int batt_id;
};

typedef struct	 tag_config_data {
	int32_t	fRsense;		//= 20 * 1000;			//Rsense value of chip, in mini ohm expand 1000 times
	int32_t 	temp_pull_up;  //230000;
	int32_t	temp_ref_voltage; //1800;1.8v
	int32_t	dbCARLSB;		//= 5.0;		//LSB of CAR, comes from spec
	int32_t	dbCurrLSB;		//781 (7.8 *100 uV);	//LSB of Current, comes from spec
	int32_t	fVoltLSB;		//250 (2.5*100 mV);	//LSB of Voltage, comes from spec
	int32_t	design_capacity;	//= 7000;		//design capacity of the battery
 	int32_t	charge_cv_voltage;	//= 4200;		//CV Voltage at fully charged
	int32_t	charge_end_current;	//= 100;		//the current threshold of End of Charged
	int32_t	discharge_end_voltage;	//= 3550;		//mV
	int32_t 	board_offset;			//0; 				//mA, not more than caculate data
	uint8_t	debug;                                          // enable or disable O2MICRO debug information
}config_data_t;

typedef struct	 tag_parameter_data {
	int32_t            	 	ocv_data_num;
	int32_t              	cell_temp_num;
	one_latitude_data_t  	*ocv;
	one_latitude_data_t	 	*temperature;
	config_data_t 		 	*config;
	struct i2c_client 	 	*client;
	uint8_t	charge_pursue_step;
 	uint8_t	discharge_pursue_step;
	uint8_t	discharge_pursue_th;
}parameter_data_t;

typedef struct tag_bmu {
	int32_t	Battery_ok;
	int32_t	fRC;			//= 0;		//Remaining Capacity, indicates how many mAhr in battery
	int32_t	fRSOC;			//50 = 50%;	//Relative State Of Charged, present percentage of battery capacity
	int32_t	fVolt;			//= 0;						//Voltage of battery, in mV
	int32_t	fCurr;			//= 0;		//Current of battery, in mA; plus value means charging, minus value means discharging
	int32_t	fPrevCurr;		//= 0;						//last one current reading
	int32_t	fOCVVolt;		//= 0;						//Open Circuit Voltage
	int32_t	fCellTemp;		//= 0;						//Temperature of battery
	int32_t	fRCPrev;
	int32_t	sCaMAH;			//= 0;						//adjusted residual capacity
	int32_t	i2c_error_times;
	uint32_t	cycle_count;
	uint32_t	dsg_data;
}bmu_data_t;

typedef struct tag_gas_gauge {
	int32_t  overflow_data; //unit: mAh, maximum capacity that the IC can measure
	uint8_t  discharge_end;
 	uint8_t  charge_end;
	uint8_t  charge_fcc_update;
 	uint8_t  discharge_fcc_update;
	int32_t  sCtMAH ;    //be carefull, this must be int32_t
	int32_t  fcc_data;
	int32_t  discharge_sCtMAH ;//be carefull, this must be int32_t
	//uint8_t  charge_wait_times;
	//uint8_t  discharge_wait_times;
	//uint8_t  charge_count;
	//uint8_t  discharge_count;
	//uint32_t bmu_tick;
	//uint32_t charge_tick;
	//uint8_t charge_table_num;
	uint8_t rc_x_num;
	uint8_t rc_y_num;
	uint8_t rc_z_num;
	uint8_t  charge_strategy;
	int32_t  charge_sCaUAH;
	int32_t  charge_ratio;  //this must be static
 	uint8_t  charge_table_flag;
	int32_t  charge_end_current_th2;
	int32_t charge_max_ratio;
	uint8_t discharge_strategy;
	int32_t discharge_sCaUAH;
	int32_t discharge_ratio;  //this must be static
	uint8_t discharge_table_flag;
	int32_t	discharge_current_th;
	uint32_t discharge_max_ratio;
	int32_t dsg_end_voltage_hi;
	int32_t dsg_end_voltage_th1;
	int32_t dsg_end_voltage_th2;
	uint8_t dsg_count_2;
	uint8_t ocv_flag;
	uint8_t  vbus_ok;
	uint8_t charge_full;
	int32_t ri;
	int32_t batt_ri;
	int32_t line_impedance;
	int32_t max_chg_reserve_percentage;
	int32_t fix_chg_reserve_percentage;
	uint8_t fast_charge_step;
	int32_t start_fast_charge_ratio;
	uint8_t charge_method_select;
	int32_t max_charge_current_fix;
	uint8_t ext_temp_measure;
	uint8_t bmu_init_ok;
	int32_t stored_capacity;
	uint8_t lower_capacity_reserve;
	uint8_t lower_capacity_soc_start;
	uint8_t percent_10_reserve;
	uint32_t power_on_100_vol;
}gas_gauge_t;

/****************************************************************************
* debug func *
***************************************************************************/
#define bmt_dbg(fmt, args...) pr_err("[bmt-fg] %s: "fmt, __func__, ## args)

/****************************************************************************
* extern variable/function declaration of table.c
***************************************************************************/

extern one_latitude_data_t ocv_data[OCV_DATA_NUM];
extern int32_t	XAxisElement[XAxis];
extern int32_t  YAxisElement[YAxis];
extern int32_t	ZAxisElement[ZAxis];
extern int32_t	RCtable[YAxis*ZAxis][XAxis];
//extern one_latitude_data_t	charge_data[CHARGE_DATA_NUM];

extern uint8_t battery_ri;
extern int32_t one_percent_rc;

/****************************************************************************
* extern variable/function defined by parameter.c
***************************************************************************/
extern config_data_t config_data;
//******************************************************************************

extern void bmu_init_parameter(parameter_data_t *parameter_customer);
extern void bmu_init_gg(gas_gauge_t *gas_gauge);
extern void bmu_reinit(void);
extern int32_t bmulib_init(void);
extern void bmulib_exit(void);

extern void	bmu_polling_loop(void);
extern void	bmu_wake_up_chip(void);
extern void	bmu_power_down_chip(void);
extern void	charge_end_process(void);
extern void	discharge_end_process(void);
extern int32_t	oz8806_temp_read(int32_t *voltage);
extern int32_t	afe_read_current(int32_t *dat);
extern int32_t	afe_read_cell_volt(int32_t *voltage);

extern void bmu_init_table(int32_t **x, int32_t **y, int32_t **z, int32_t **rc);

extern const char * get_table_version(void);
extern void bmt_print(const char *fmt, ...);
/****************************************************************************
* extern variable/function defined by oz8806_battery.c
***************************************************************************/
extern int32_t oz8806_vbus_voltage(void);
extern int32_t oz8806_get_simulated_temp(void);
extern int32_t oz8806_get_init_status(void); //if return 1, battery capacity is initiated successfully
extern void oz8806_battery_update_data(void);
extern int32_t oz8806_get_soc(void);
extern int32_t oz8806_get_cycle_count(void);
extern int32_t oz8806_get_remaincap(void);
extern int32_t oz8806_get_battry_current(void);
extern int32_t oz8806_get_battery_voltage(void);
extern int32_t oz8806_get_battery_temp(void);
//sd77428
extern int32_t sd77428_i2c_read_bytes(struct oz8806_data* chip, uint8_t cmd, uint8_t *val, uint8_t bytes);
extern int32_t sd77428_i2c_write_bytes(struct oz8806_data* chip,uint8_t cmd,uint8_t *val, uint8_t bytes);
extern uint8_t calculate_pec_byte(uint8_t data, uint8_t crcin);
extern uint8_t calculate_pec_bytes(uint8_t init_pec,uint8_t* pdata,uint8_t len);
extern int32_t sd77428_read_word(struct oz8806_data *chip,uint8_t index,uint16_t *data);
extern int32_t sd77428_write_word(struct oz8806_data *chip,uint8_t index,uint16_t data);
extern int32_t sd77428_read_block(struct oz8806_data *chip, uint8_t *data_buf,uint32_t len);
extern int32_t sd77428_write_block(struct oz8806_data *chip,uint8_t *data_buf,uint32_t len);
extern int32_t sd77428_read_addr(struct oz8806_data *chip, uint32_t addr, uint32_t* pdata);
extern int32_t sd77428_write_addr(struct oz8806_data *chip,uint32_t addr, uint32_t value);

//export symbol for bmulib
extern struct i2c_client * oz8806_get_client(void);
extern int8_t get_adapter_status(void);

extern void oz8806_set_batt_info_ptr(bmu_data_t  *batt_info);
extern void oz8806_set_gas_gauge(gas_gauge_t *gas_gauge);
extern unsigned long oz8806_get_system_boot_time(void);
extern unsigned long oz8806_get_power_on_time(void);
extern unsigned long oz8806_get_boot_up_time(void);
extern int32_t oz8806_wakeup_full_power(void);
/****************************************************************************
* extern variable/function defined by oz8806_api_dev.c.
* This file is deprecated in Linux system.
***************************************************************************/
extern int32_t calculate_soc_result(void);
extern int32_t one_latitude_table(int32_t number,one_latitude_data_t *data,int32_t value);
extern  uint8_t OZ8806_LookUpRCTable(int32_t infVolt,int32_t infCurr, int32_t infTemp, int32_t *infCal);
void charge_process(void);
void discharge_process(void);
void age_update(void);
void accmulate_data_update(void);

extern bmu_data_t	   *batt_info;
extern gas_gauge_t    *gas_gauge;
extern parameter_data_t parameter_customer;
extern parameter_data_t *parameter;
extern struct oz8806_data *the_oz8806;

extern int32_t *rc_table;
extern int32_t *xaxis_table;
extern int32_t *yaxis_table;
extern int32_t *zaxis_table;

extern int32_t calculate_mah;
extern int32_t  calculate_soc ;
extern uint32_t calculate_version;
extern int32_t age_soh;
extern int32_t cycle_soh;
extern uint32_t accmulate_dsg_data;
extern int32_t afe_write_cycleCount(uint32_t dat);
extern int32_t afe_write_accmulate_dsg_data(uint32_t dat);
extern uint32_t poweron_flag;
#define X_AXIS gas_gauge->rc_x_num
#define Y_AXIS gas_gauge->rc_y_num
#define Z_AXIS gas_gauge->rc_z_num
#endif //end _PARAMETER_H_

MODULE_LICENSE("GPL v2");
