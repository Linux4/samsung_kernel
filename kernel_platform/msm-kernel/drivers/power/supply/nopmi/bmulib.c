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

/****************************************************************************/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/kmod.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/timex.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/syscalls.h>
#include <asm/uaccess.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include "parameter.h"
#include "table.h"
/*****************************************************************************
* Define section
* add all #define here
*****************************************************************************/

//#define FCC_UPDATA_CHARGE
#define VERSION		"2021.08.12/7.00.06 disable otp and expand rsense"
#define config_data		parameter->config

#define	RETRY_CNT	8
#define SHUTDOWN_HI          50
#define SHUTDOWN_TH1         100
#define SHUTDOWN_TH2         300

#define CHARGE_END_CURRENT2  (config_data->charge_end_current +2)
#define full_charge_data   (gas_gauge->fcc_data + gas_gauge->fcc_data / 100 - 1)

#define MAX_SUSPEND_CURRENT	(-45)		//H7 add wakup CAR range check, max suspend current
#define MAX_SUSPEND_CONSUME	(MAX_SUSPEND_CURRENT*10/36)	//H7 add wakup CAR range check, max suspend consumption(1000*mah)
#define MAX_SUSPEND_CHARGE	(1800*10/36)//H7 add wakup CAR range check, max suspend charge(1000*mah)

#define ABS(a, b) ((a>b)?(a-b):(b-a))

//ADC采样转换值
#define PARAM_RSENSE_AMPLIFIER  			1000
#define GDM_SLOPE_AMPLIF			 		10000

#define LSB_VADC_VOLT						(1160)
#define LSB_VADC_DIV						(32768)
#define LSB_CADC_VOLT						(144000)
#define LSB_CADC_DIV						(32768)
#define LSB_CC_VOLT							(625)
#define LSB_CC_DIV							(1000)

#define ADC_VOLTLSB							(LSB_VADC_VOLT*6)
#define ADC_VOLTLSB_FACTOR					LSB_VADC_DIV		//(1160mV/32768)*6 = 212.4uV
#define ADC_CADCLSB							LSB_CADC_VOLT
#define ADC_CADCLSB_FACTOR					LSB_CADC_DIV
#define ADC_CCLSB							LSB_CC_VOLT
#define ADC_CCLSB_FACTOR					LSB_CC_DIV
#define	ADC_INTMPLSB						LSB_VADC_VOLT
#define	ADC_INTMPLSB_FACTOR					LSB_VADC_DIV		//(1160mV/32768) = 35.4uV
#define ADC_EXTMPLSB						LSB_VADC_VOLT
#define ADC_EXTMPLSB_FACTOR					LSB_VADC_DIV	

#define CONSTANT_6UA						6
/*****************************************************************************
* static variables section
****************************************************************************/
static unsigned long bmu_kernel_memaddr;
bmu_data_t 	*batt_info;
gas_gauge_t	*gas_gauge;
parameter_data_t parameter_customer;
parameter_data_t *parameter;

int32_t *rc_table;
int32_t *xaxis_table;
int32_t *yaxis_table;
int32_t *zaxis_table;
int32_t	calculate_soc = 0;
uint8_t battery_ri = 100;
int32_t one_percent_rc = 0;
uint32_t accmulate_dsg_data = 0; 

static int32_t	car_error = 0;
static uint8_t	oz8806_cell_num  = 1;
static int32_t	res_divider_ratio = 2000;
static int32_t one_cell_divider_ratio = 2000;//(232 + 100) * 1000 / 100;
static uint8_t	charge_end_flag = 0;
static uint8_t wait_ocv_flag = 0;
static uint8_t power_on_flag = 0;
static uint8_t bmu_sleep_flag = 0;
static uint8_t discharge_end_flag = 0;
static uint8_t sleep_ocv_flag = 0;
static uint32_t previous_loop_timex = 0;
static uint32_t time_sec = 0;
/******************************************************************************
* Function prototype section
* add prototypes for all functions called by this file,execepting those
* declared in header file
*****************************************************************************/
static void 	oz8806_over_flow_prevent(void);
static int32_t 	afe_read_ocv_volt(int32_t *voltage);
static int32_t 	afe_read_car(int32_t *dat);
static int32_t 	afe_write_car(int32_t dat);
static int32_t 	afe_read_cell_temp(int32_t *date);
static int32_t 	afe_read_board_offset(int32_t *dat);
static int32_t 	afe_write_board_offset(int32_t date);

static int32_t 	oz8806_cell_voltage_read(int32_t *voltage);
static int32_t 	oz8806_ocv_voltage_read(int32_t *voltage);
static int32_t 	oz8806_current_read(int32_t *data);
static int32_t 	oz8806_car_read(int32_t *car);
static int32_t 	oz8806_car_write(int32_t data);
static void	 	bmu_wait_ready(void);
static void		check_shutdwon_voltage(void);
static int32_t		afe_read_cycleCount(uint32_t *dat);
static int32_t		afe_read_accmulate_dsg_data(uint32_t *dat);
static int32_t		oz8806_accDsgData_read(uint32_t *dat);
static int32_t		afe_read_batt_rc(uint32_t *dat);
static int32_t		afe_write_batt_rc(uint32_t dat);
/****************************************************************************
 * Description:
 *		read oz8806 batt_rc
 * Parameters:
 *
 * Return:
 *
 ****************************************************************************/
static int32_t oz8806_batt_rc_read(uint32_t *rc)
{
	int32_t 	ret = 0;
	uint32_t 	temp = 0;

	ret = sd77428_read_addr(the_oz8806,0x20000C92,&temp);
	if(ret)
		return -1;
	*rc = temp;

	return ret;
}

/****************************************************************************
 * Description:
 *		write oz8806 batt_rc
 * Parameters:
 *
 * Return:
 *
 ****************************************************************************/
static int32_t oz8806_batt_rc_write(uint32_t data)
{
	int32_t 	ret = 0;

	ret = sd77428_write_addr(the_oz8806,0x20000C92,data);
	if(ret)
		return -1;

	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to read batt_rc
 * Parameters:
 *
 * Return:
 *
 * Note:
 *
 *****************************************************************************/
int32_t afe_read_batt_rc(uint32_t *dat)
{
	int32_t ret;
	uint8_t i;
	uint32_t buf;

	for(i = num_0; i < RETRY_CNT; i++)
	{
		ret = oz8806_batt_rc_read(&buf);
		if(ret >= num_0)
			break;
		msleep(5);
	}
	if(i >= RETRY_CNT)
	{
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	}
	*dat = buf;
	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to write batt_rc
 * Parameters:
 *
 * Return:
 *
 * Note:
 *
 *****************************************************************************/
int32_t afe_write_batt_rc(uint32_t dat)
{
	int32_t ret;
	uint8_t i;

	for(i = num_0; i < RETRY_CNT; i++)
	{
		ret = oz8806_batt_rc_write(dat);
		if(ret >= num_0)
			break;
		msleep(5);
	}
	if(i >= RETRY_CNT)
	{
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	}
	return ret;
}
/*****************************************************************************
 * Description:
 *		bmu init chip
 * Parameters:
 *		used to init bmu
 * Return:
 *		None
 *****************************************************************************/
void bmu_init_chip(parameter_data_t *paramter_temp)
{
	int32_t ret = 0;
	int32_t data = 0;
	uint8_t * addr = NULL;
	config_data_t * config_test = NULL;
	uint32_t byte_num = 0;

	byte_num = sizeof(config_data_t) + sizeof(bmu_data_t) +  sizeof(gas_gauge_t);

	memset((uint8_t *)bmu_kernel_memaddr,num_0,byte_num);

	parameter = paramter_temp;
	addr = (uint8_t *)(parameter->config);
	for(byte_num = num_0;byte_num < sizeof(config_data_t);byte_num++)
	{
		 *((uint8_t *)bmu_kernel_memaddr + byte_num) = *addr++;
	}

	config_test = (config_data_t *)bmu_kernel_memaddr;

	batt_info = (bmu_data_t *)((uint8_t *)bmu_kernel_memaddr + byte_num);

	//--------------------------------------------------------------------------------------------------
	batt_info->Battery_ok = 1;
	batt_info->i2c_error_times = num_0;
	byte_num +=  sizeof(bmu_data_t);
	gas_gauge = (gas_gauge_t *)((uint8_t *)bmu_kernel_memaddr + byte_num);
	byte_num += sizeof(gas_gauge_t);
	//--------------------------------------------------------------------------------------------------
    gas_gauge->bmu_init_ok = 0;
	gas_gauge->charge_end = num_0;
	gas_gauge->charge_end_current_th2 = CHARGE_END_CURRENT2;
	gas_gauge->charge_strategy = 1;
	gas_gauge->charge_max_ratio = 3500;
	//gas_gauge->charge_max_ratio = 10000;  // for lianxiang

	gas_gauge->discharge_end = num_0;
	gas_gauge->discharge_current_th = DISCH_CURRENT_TH;
	gas_gauge->discharge_strategy = 1;
	gas_gauge->discharge_max_ratio = 2000;  // for lianxiang
	gas_gauge->dsg_end_voltage_hi = SHUTDOWN_HI;

	gas_gauge->batt_ri = 45; //120 // charging. It is related to voltage when full charging condition.
	gas_gauge->line_impedance = 0; //charging
	gas_gauge->max_chg_reserve_percentage = 1000;
	gas_gauge->fix_chg_reserve_percentage  = 0;
	gas_gauge->fast_charge_step = 2;
	gas_gauge->start_fast_charge_ratio = 1500;
	gas_gauge->charge_method_select = 0;
	gas_gauge->max_charge_current_fix = 4000;

	//add this for oinom, start
	gas_gauge->lower_capacity_reserve = 0;
	gas_gauge->lower_capacity_soc_start =30;

	gas_gauge->percent_10_reserve = 0;
    	//add this for oinom, end
	gas_gauge->poweron_batt_ri = 40;
	//this for test gas gaugue
	if(parameter->config->debug)
		gas_gauge->dsg_end_voltage_hi = 0;
	gas_gauge->dsg_end_voltage_th1 = SHUTDOWN_TH1;
	gas_gauge->dsg_end_voltage_th2 = SHUTDOWN_TH2;
	gas_gauge->dsg_count_2 = num_0;

	gas_gauge->overflow_data = num_32768 * num_5 * 1000 / config_data->fRsense;
	bmt_dbg("gas_gauge->overflow_data is %d\n",gas_gauge->overflow_data);

	bmu_init_gg(gas_gauge);

	bmu_init_table(&xaxis_table, &yaxis_table, &zaxis_table, &rc_table);

	bmt_dbg("byte_num is %d\n",byte_num);
	power_on_flag = num_0;
	bmu_sleep_flag = num_0;
	one_percent_rc = config_data->design_capacity  / num_100;

	bmt_dbg("DRIVER VERSION is %s\n",VERSION);
	bmt_dbg("Rsense:%d,RTempPullup:%d,VTempRef:%d,CarLSB:%d,CurLSB:%d,VolLSB:%d\n",
		config_test->fRsense,
		config_test->temp_pull_up,
		config_test->temp_ref_voltage,
		config_test->dbCARLSB,
		config_test->dbCurrLSB,
		config_test->fVoltLSB
	);
	bmt_dbg("FCC:%d,CV:%d,EOC:%d,EOD:%d,BoardOffset:%d,Debug:%d\n",
		config_test->design_capacity,
		config_test->charge_cv_voltage,
		config_test->charge_end_current,
		config_test->discharge_end_voltage,
		config_test->board_offset,
		config_test->debug
	);
	//check_pec_control();
	//move check_oz8806_status here to avoid iic-commu-issue
	// trim_bmu_VD23();
	/*
	 * board offset current is measured and reg0x18 value is updated only 
	 * after reset(power on reset or software reset), 
	 */
	if (oz8806_get_boot_up_time() < 625) 
	{
		bmt_dbg("wait for current offset detection\n");
		msleep(625 - oz8806_get_boot_up_time());
	#if 0
		afe_register_read_word(OZ8806_OP_BOARD_OFFSET,(uint16_t *)&data);
	#else
		afe_read_board_offset(&data);
	#endif
	}

	ret  = afe_read_board_offset(&data);
	bmt_dbg("board_offset is %d mA\n",data);

	if(ret >= num_0)
	{
		if(config_data->board_offset != num_0)
		{
			afe_write_board_offset(config_data->board_offset);
		}
		else if((data > num_10) || (data <= num_0))
			afe_write_board_offset(num_7);
	}

	bmt_dbg("parameter %d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
		config_data->fRsense,
		config_data->temp_pull_up,
		config_data->temp_ref_voltage,
		config_data->dbCARLSB,
		config_data->dbCurrLSB,
		config_data->fVoltLSB,
		config_data->design_capacity,
		config_data->charge_cv_voltage,
		config_data->charge_end_current,
		config_data->discharge_end_voltage,
		config_data->board_offset,
		config_data->debug,
		one_cell_divider_ratio
	);
}

/*****************************************************************************
 * Description:
 *		wait ocv flag
 * Parameters:
 *		None
 * Return:
 *		None
 *****************************************************************************/
static int32_t system_upgrade_flag = 0;
void wait_ocv_flag_fun(void)
{
	uint32_t date = 0;
        uint32_t bmulib_version = 0x20230720;

	afe_read_car(&batt_info->fRC);
	if((batt_info->fRC > 10000) || (batt_info->fRC < 0))
	{
		afe_write_car(50);
		bmt_dbg("lower battery!!!\n");
	}
	afe_read_cell_temp(&batt_info->fCellTemp);

	wait_ocv_flag = 1;
	afe_read_ocv_volt(&batt_info->fOCVVolt);
	batt_info->fVolt = batt_info->fOCVVolt;

	afe_read_car(&batt_info->fRC);
	bmt_dbg("wait_ocv_flag_fun:fRC %d poweron_flag: %d\n",batt_info->fRC,poweron_flag);

	sd77428_read_addr(the_oz8806, 0x20000D00, &date);//read version	date
	bmt_dbg("0x20000D00 reg is 0x%x\n", date);
	if(date != bmulib_version)
	{
		sd77428_write_addr(the_oz8806, 0x20000D00, bmulib_version);
		system_upgrade_flag = 1;
	}
//	if((batt_info->fRC < 5) || (batt_info->fRC > 2000))
	if((poweron_flag < 2) || (system_upgrade_flag == 1))
	{
		afe_read_ocv_volt(&batt_info->fOCVVolt);

		bmt_dbg("fOCVVolt %d,cell_num:%d,power_on_100_vol:%d\n",batt_info->fOCVVolt,oz8806_cell_num,gas_gauge->power_on_100_vol);

		if(batt_info->fOCVVolt < 2500 || batt_info->fOCVVolt  > (config_data->charge_cv_voltage + 150))
		{
			afe_read_cell_volt(&batt_info->fVolt);
			batt_info->fOCVVolt = batt_info->fVolt;
			bmt_dbg("ocv volt falut,use normal vbat %d as ocv\n",batt_info->fVolt);
		}
		batt_info->fVolt = batt_info->fOCVVolt;
		batt_info->fRSOC = one_latitude_table(parameter->ocv_data_num,parameter->ocv,batt_info->fOCVVolt);
		bmt_dbg("find ocv table batt_info.fRSOC is %d\n",batt_info->fRSOC); 
		//ocv 100%
		if((batt_info->fOCVVolt * oz8806_cell_num) >= gas_gauge->power_on_100_vol)
			batt_info->fRSOC = num_100;

		if((batt_info->fRSOC >num_100) || (batt_info->fRSOC < num_0))
			batt_info->fRSOC = num_50;

		batt_info->fRC = batt_info->fRSOC * config_data->design_capacity / num_100;
		if(batt_info->fRC  >= (gas_gauge->overflow_data - num_10))
		{
			batt_info->fRC = gas_gauge->overflow_data  - gas_gauge->overflow_data /num_100;
			batt_info->fRCPrev = batt_info->fRC;
		}
		afe_write_car(batt_info->fRC);
                afe_write_batt_rc(batt_info->fRC);
		batt_info->fRCPrev = batt_info->fRC;
		gas_gauge->fcc_data= config_data->design_capacity * 100/100;
		batt_info->sCaMAH = batt_info->fRSOC * config_data->design_capacity / num_100;
		gas_gauge->discharge_sCtMAH = config_data->design_capacity - batt_info->sCaMAH;
		power_on_flag = 1;
		bmt_dbg("fcc_data %d fRC %d sCaMAH %d\n",gas_gauge->fcc_data,batt_info->fRC,batt_info->sCaMAH);
//		afe_write_cycleCount(0);
		afe_write_accmulate_dsg_data(0);
		bmt_dbg("Power on mode is activated \n");
	}else
	{
		afe_read_cycleCount(&batt_info->cycle_count);
		afe_read_accmulate_dsg_data(&batt_info->dsg_data);
		accmulate_data_update();
		bmt_dbg("Normal mode is activated \n");
	}
}

/*****************************************************************************
* Description:
*		check shudown voltage control
* Parameters:
*		None
* Return:
*		None
*****************************************************************************/
void check_shutdwon_voltage(void)
{
	int32_t sleep_ocv_soc = 0,sleep_ocv_mah = 0;
	uint32_t diff = one_percent_rc * 10;
	uint32_t sleep_ocv_flag = 0;

	sd77428_read_addr(the_oz8806,0x40001010,&sleep_ocv_flag);
	sleep_ocv_flag = sleep_ocv_flag & 0x01;
	if(!sleep_ocv_flag)
		return;
	sleep_ocv_soc = one_latitude_table(parameter->ocv_data_num,parameter->ocv,batt_info->fOCVVolt);
	sleep_ocv_mah = sleep_ocv_soc * gas_gauge->fcc_data / 100;

	bmt_dbg("fRC %d,focvvolt %d,soc %d,mah %d,diff %d\r\n",batt_info->fRC,batt_info->fOCVVolt,sleep_ocv_soc,sleep_ocv_mah,diff);

	if((sleep_ocv_mah < batt_info->fRC) && ((batt_info->fRC - sleep_ocv_mah) >= diff))
	{
		batt_info->fRC = sleep_ocv_mah;
		batt_info->sCaMAH = batt_info->fRC;
	}
}

void check_poweron_voltage(void)
{
	int32_t infVolt = 0 ,sleep_ocv_soc = 0,sleep_ocv_mah = 0;
	uint32_t diff = one_percent_rc * 15;
	uint32_t sleep_ocv_flag = 0;

	sd77428_read_addr(the_oz8806,0x40001010,&sleep_ocv_flag);
	sleep_ocv_flag = sleep_ocv_flag & 0x01;
	bmt_dbg("sleep_ocv_flag:%d\n", sleep_ocv_flag);
	if(!sleep_ocv_flag)
		return;

	if(batt_info->fCellTemp < 10)
		gas_gauge->poweron_batt_ri = 50;
	else if(batt_info->fCellTemp > 40)
		gas_gauge->poweron_batt_ri = 30;
	else
		gas_gauge->poweron_batt_ri = 20;
	
	infVolt =  batt_info->fVolt - gas_gauge->poweron_batt_ri * batt_info->fCurr / 1000;
	sleep_ocv_soc = one_latitude_table(parameter->ocv_data_num,parameter->ocv,infVolt);

	sleep_ocv_mah = sleep_ocv_soc * gas_gauge->fcc_data / 100;
	afe_read_batt_rc(&batt_info->batt_rc);
	bmt_dbg("infVolt: %d batt_rc: %d sleep_ocv_mah: %d diff:%d\n",infVolt,batt_info->batt_rc,sleep_ocv_mah,diff);
	if((sleep_ocv_mah < batt_info->batt_rc) && ((batt_info->batt_rc - sleep_ocv_mah) >= diff))
	{
		batt_info->fRC = sleep_ocv_mah;
		batt_info->sCaMAH = batt_info->fRC;
		afe_write_batt_rc(batt_info->fRC);
	}
}

/*****************************************************************************
* Description:
*		bmu_wait_ready
* Parameters:
*		None
* Return:
*		None
*****************************************************************************/
static void bmu_wait_ready(void)
{
	bmt_dbg("power_on_flag %d, sleep_ocv_flag %d\n",power_on_flag,sleep_ocv_flag);
	//normal start,init soc with CAR
	if(0 == power_on_flag && 0 == sleep_ocv_flag)
	{
		afe_read_car(&batt_info->fRC);
		afe_read_cell_volt(&batt_info->fVolt);
		batt_info->sCaMAH = batt_info->fRC;
		afe_read_cell_temp(&batt_info->fCellTemp);
		afe_read_current(&batt_info->fCurr);
		check_poweron_voltage();
		bmt_dbg("fRC:%d sCaMAH:%d fVolt %d\n", batt_info->fRC,batt_info->sCaMAH,batt_info->fVolt);
	}
	//sleep ocv power on,init soc wit CAR,Sleep OCV,RC Table
	if(0 == power_on_flag && 1 == sleep_ocv_flag)
	{
		afe_read_car(&batt_info->fRC);
		batt_info->sCaMAH = batt_info->fRC;
		check_shutdwon_voltage();
	}
	//power on,init soc with Power on ocv
	if(1 == power_on_flag && 0 == sleep_ocv_flag)
		bmt_dbg("power on,use ocv soc\n");
	//power on and sleep ocv on, error
	if(1 == power_on_flag && 1 == sleep_ocv_flag)
		bmt_dbg("error\n");

	if(batt_info->sCaMAH <= num_0)
		batt_info->sCaMAH = one_percent_rc;
	if(batt_info->sCaMAH > full_charge_data)
		batt_info->sCaMAH = full_charge_data;

	if(batt_info->sCaMAH  >= (gas_gauge->overflow_data -num_10))
		batt_info->sCaMAH = gas_gauge->overflow_data  - gas_gauge->overflow_data/num_100;

	batt_info->fRSOC = batt_info->sCaMAH   * num_100;
	batt_info->fRSOC = batt_info->fRSOC / gas_gauge->fcc_data ;

	batt_info->fRC 		= batt_info->sCaMAH;
	batt_info->fRCPrev 	= batt_info->sCaMAH;
	gas_gauge->sCtMAH 	= batt_info->sCaMAH;

	bmt_dbg("batt_info->fVolt is %d\n",(batt_info->fVolt * oz8806_cell_num));
	bmt_dbg("batt_info->fRSOC is %d\n",batt_info->fRSOC);
	bmt_dbg("batt_info->sCaMAH is %d\n",batt_info->sCaMAH);
	bmt_dbg("gas_gauge->sCtMAH is %d\n",gas_gauge->sCtMAH);
	bmt_dbg("fcc is %d\n",gas_gauge->fcc_data);
	bmt_dbg("batt_info->fRC is %d\n",batt_info->fRC);
	bmt_dbg("batt_info->fCurr is %d\n",batt_info->fCurr);

	afe_write_car(batt_info->fRC);

	if(gas_gauge->fcc_data > batt_info->sCaMAH)                                  
		gas_gauge->discharge_sCtMAH = gas_gauge->fcc_data- batt_info->sCaMAH;    
	else                                                                         
		gas_gauge->discharge_sCtMAH = 0;                                         

	if(batt_info->fRSOC <= num_0)
	{
		gas_gauge->discharge_end = num_1;
		batt_info->fRSOC = num_0;
		gas_gauge->sCtMAH = num_0;
	}
	if(batt_info->fRSOC >= num_100)
	{
		gas_gauge->charge_end = num_1;
		charge_end_flag = num_1;
		batt_info->fRSOC = num_100;
		gas_gauge->discharge_sCtMAH = num_0;
		gas_gauge->discharge_fcc_update = num_1;
	}
	gas_gauge->bmu_init_ok = num_1;
}

void check_powerdown_voltage(void)
{	
	int32_t ret1 = 0,ret2 = 0,ret3 = 0,ret4 = 0,ret5 = 0;
	uint8_t i = 0;

	for(i = 0; i < 5; i++)
	{
		ret1 = sd77428_write_addr(the_oz8806,0x40001004,0xa923);//write time length
		ret2 = sd77428_write_addr(the_oz8806,0x40001008,0x0010);//write time mark
		ret3 = sd77428_write_addr(the_oz8806,0x4000100c,0x0000);//close irq
		ret4 = sd77428_write_addr(the_oz8806,0x40001010,0x0003);//reload time length & clear timer flag
		ret5 = sd77428_write_addr(the_oz8806,0x40002004,0x0000);//close wcdog
		if((ret1 < 0) || (ret2 < 0) || (ret3 < 0) || (ret4 < 0) || (ret5 < 0))
		{
			bmt_dbg("sd77428 write failed,i:%d!\n",i);
			msleep(5);
		}
		else
		{
			bmt_dbg("sd77428 write timer successed!\n");
			break;
		}
	}
	afe_write_batt_rc(batt_info->fRC);
	afe_read_batt_rc(&batt_info->batt_rc);
	bmt_dbg("write batt_rc: %d\n",batt_info->batt_rc);
}

void bmu_powerdown_mode(void)
{
	int32_t ret1 = 0,ret2 = 0,ret3 = 0,ret4 = 0,data = 0;
	uint8_t i = 0;

	for(i = 0; i < 5; i++)
	{
		ret1 = sd77428_write_addr(the_oz8806,0x400042A8,0x6318);//unlock
		ret2 = sd77428_write_addr(the_oz8806,0x400042AC,0x0040);//sleep mode
		ret3 = sd77428_write_addr(the_oz8806,0x40004160,0x0000);//close vadc
		sd77428_read_addr(the_oz8806,0x400041A0,&data);
		data = data & 0xFFF0;  //bit0 set 0
		ret4 = sd77428_write_addr(the_oz8806,0x400041A0,data);//close cadc
		if((ret1 < 0) || (ret2 < 0) || (ret3 < 0) || (ret4 < 0))
		{
			bmt_dbg("sd77428 write failed,ret1:%d,ret2:%d,ret3:%d,,ret4:%d,i:%d!\n",ret1,ret2,ret3,ret4,i);
			msleep(5);
		}
		else
		{
			bmt_dbg("sd77428 write sleep mode successed!\n");
			break;
		}
	}
	if(i >= 5)
		bmt_dbg("sd77428 powerdown set sleep mode failed!\n");

}

/*****************************************************************************
* Description:
*		power down  8806 chip
* Parameters:
*		None
* Return:
*		None
*****************************************************************************/
void bmu_power_down_chip(void)
{
	int32_t ret = 0;
	afe_read_cell_temp(&batt_info->fCellTemp);
	afe_read_cell_volt(&batt_info->fVolt);
	afe_read_current(&batt_info->fCurr);
	afe_read_car(&batt_info->fRC);

	if(parameter->config->debug){
		bmt_dbg("\n");
		bmt_dbg("batt_info->fVolt is %d\n",(batt_info->fVolt * oz8806_cell_num));
		bmt_dbg("batt_info->fRSOC is %d\n",batt_info->fRSOC);
		bmt_dbg("batt_info->sCaMAH is %d\n",batt_info->sCaMAH);
		bmt_dbg("batt_info->fRC is %d\n",batt_info->fRC);
		bmt_dbg("batt_info->fCurr is %d\n",batt_info->fCurr);
		bmt_dbg("batt_info->cycle_count is %d\n",batt_info->cycle_count);
	}
	check_powerdown_voltage();
	bmu_powerdown_mode();
	ret = sd77428_write_addr(the_oz8806,0x400042A8,0x6318);//unlock
	if(ret)
		bmt_dbg("sd77428 powerdown unlock failed!\n");	
	else
	{
		ret = sd77428_write_addr(the_oz8806,0x400042AC,0x0040);//sleep mode
		ret = sd77428_write_addr(the_oz8806,0x40004160,0x0000);
		if(ret)
			bmt_dbg("sd77428 powerdown set sleep mode failed!\n");	
	}

}

/*****************************************************************************
* Description:
*		power up  8806 chip
* Parameters:
*		None
* Return:
*		None
*****************************************************************************/
void bmu_wake_up_chip(void)
{
	int32_t ret;
	int32_t value;
	uint8_t discharge_flag = num_0;
	uint8_t i;
	int32_t sleep_time = 0;
	int32_t	car_wakeup = 0;
	int8_t adapter_status = 0;

	bmt_dbg("fCurr is %d\n",batt_info->fCurr);

	adapter_status = get_adapter_status();
	if ((batt_info->fCurr <= gas_gauge->discharge_current_th) ||
 		(adapter_status == O2_CHARGER_BATTERY))
	{
		discharge_flag = 1;
	}

	bmt_dbg("adapter_status: %d\n", adapter_status);

	bmu_sleep_flag = 1;

	time_sec = oz8806_get_system_boot_time();
	sleep_time = time_sec - previous_loop_timex;
	bmt_dbg("sleep time: %d s\n",sleep_time);

	if(batt_info->fRSOC >=num_100)
	{
		if(batt_info->fRSOC >= num_100)	batt_info->fRSOC = num_100;
		if(batt_info->sCaMAH > (full_charge_data))
		{
			bmt_dbg("BMT fgu wake up batt_info.sCaMAH big error is %d\n",batt_info->sCaMAH);
			batt_info->sCaMAH = full_charge_data;
		}
	}

	afe_read_current(&batt_info->fCurr);
	afe_read_cell_volt(&batt_info->fVolt);

	bmt_dbg("sCaMAH:%d,fRC:%d,fcurr:%d,volt:%d,sCtMAH:%d\n",
		batt_info->sCaMAH,batt_info->fRC,batt_info->fCurr,
		(batt_info->fVolt * oz8806_cell_num),
		gas_gauge->sCtMAH);

	for(i = num_0;i< num_3;i++)
	{
		ret = afe_read_car(&car_wakeup);
		if (ret >= 0)
		{
			if ((car_wakeup == 0) || ((!discharge_flag) && (car_wakeup < 0)))
			{
				car_error += 1;
				bmt_dbg("test fRC read: %d, %d\n",car_error, car_wakeup);	//H7 Test Version
			}
			else
				break;
		}
	}

	value = car_wakeup - batt_info->fRC;
	//*******CAR zero reading Workaround******************************
	if (sleep_time < 0)	sleep_time = 60;			//force 60s
	if (discharge_flag == 1)
		sleep_time *= MAX_SUSPEND_CONSUME;			//time * current / 3600 * 1000
	else
		sleep_time *= MAX_SUSPEND_CHARGE;			//time * current / 3600 * 1000

	// 10%
	if (ABS(car_wakeup, batt_info->fRC) > (10 * config_data->design_capacity / num_100))		//delta over 10%
	{
		if (((value * 1000) - sleep_time) < 0)				//over max car range
		{
			value = (sleep_time / 1000);
			bmt_dbg("Ab CAR:%d,mod:%d\n",car_wakeup,value);
			car_wakeup = value + batt_info->fRC;
			afe_write_car(car_wakeup);
		}
	}
	bmt_dbg("CAR Prev:%d,CAR Now:%d\n",batt_info->fRC,car_wakeup);
	//*******CAR zero reading Workaround******************************
	if(car_wakeup <= 0)  
	{
	    bmt_dbg("fRC is %d\n",car_wakeup);
		//oz8806_over_flow_prevent();
		gas_gauge->charge_fcc_update = num_0;
		gas_gauge->discharge_fcc_update = num_0;
		
		if (car_wakeup < 0)	//overflow check
		{
			if (batt_info->fVolt >= (config_data->charge_cv_voltage - 50))
			{
				car_wakeup = gas_gauge->fcc_data -num_1;
			}
			else
			{
				if (batt_info->fRSOC > 0)
					car_wakeup = batt_info->fRSOC * gas_gauge->fcc_data / num_100 - num_1;
				else
					car_wakeup = batt_info->sCaMAH;
			}
			afe_write_car(car_wakeup);
			batt_info->fRC = car_wakeup;
			batt_info->fRCPrev = batt_info->fRC;
		}

		if(discharge_flag)
		{
			check_shutdwon_voltage();
			value = calculate_soc_result();
			if(value < batt_info->fRSOC)
			{
				batt_info->fRSOC = value;
				batt_info->sCaMAH = batt_info->fRSOC * gas_gauge->fcc_data  / 100;
				batt_info->fRC = batt_info->sCaMAH;
				batt_info->fRCPrev = batt_info->fRC;
				afe_write_car(batt_info->fRC);
			}

			if(batt_info->fRSOC <= 0)
			{
				if((batt_info->fVolt <= config_data->discharge_end_voltage))
				{
					discharge_end_process();
				}
				//wait voltage
				else
				{
					batt_info->fRSOC  = 1;
					batt_info->sCaMAH = gas_gauge->fcc_data / num_100 ;
					gas_gauge->discharge_end = num_0;
				}
			}
			return;
		}
	}

	if ((discharge_flag) && (car_wakeup > batt_info->fRC))
	{
		bmt_dbg("it seems error 1\n");
		value = car_wakeup - batt_info->fRC;
		car_wakeup = batt_info->fRC - value;
		afe_write_car(car_wakeup);
	}
	if ((!discharge_flag) && (car_wakeup < batt_info->fRC))
	{
		bmt_dbg("it seems error 2\n");
		value = batt_info->fRC - car_wakeup;
		car_wakeup = batt_info->fRC + value;
		afe_write_car(car_wakeup);
	}

	bmt_dbg("tttt final wakeup car is %d\n",car_wakeup);
}
/*****************************************************************************
* Description:
*		bmu polling loop
* Parameters:
*		None
* Return:
*		None
*****************************************************************************/
void bmu_polling_loop(void)
{
	int32_t data = 0;
	int32_t ret = 0;
	static uint8_t error_times = 0;
	int8_t adapter_status = 0;

	bmt_dbg("-----------------------bmu_polling_loop start------------------\n");
	
	if(!(gas_gauge->bmu_init_ok))
	{
		if(!wait_ocv_flag)
			wait_ocv_flag_fun();
		else
			bmu_wait_ready();
		goto end;
	}

	if(oz8806_get_power_on_time() < 2250)
	{
		bmt_dbg("wait adc ready\n");
		msleep(2250 - oz8806_get_power_on_time());
		oz8806_get_power_on_time();
	}
	batt_info->fRCPrev = batt_info->fRC;

	afe_read_current(&batt_info->fCurr);
	afe_read_cell_volt(&batt_info->fVolt);
	afe_read_cell_temp(&batt_info->fCellTemp);

	bmt_dbg("fCurr %d, fVolt %d, fCellTemp %d\n",batt_info->fCurr,batt_info->fVolt,batt_info->fCellTemp);

	//be careful for large current
	//and wake up condition
	ret = afe_read_car(&data);
	if(parameter->config->debug) {
		if (data == 0)	car_error++;
		bmt_dbg("CAR is %d, %d\n",data, car_error);
	}

	if((ret >= 0) && (data > 0))
	{
		// for big current charge
		// 10%
		if (ABS(batt_info->fRCPrev, data) < (10 * config_data->design_capacity / num_100))
		{
			batt_info->fRC = data;
			error_times = 0;
		}
		else
		{
			error_times++;
			if(error_times > 3)
			{
				bmt_dbg("CAR error_times is %d\n",error_times);
				error_times = 0;
				afe_write_car(batt_info->sCaMAH);
				batt_info->fRCPrev = batt_info->sCaMAH;
				batt_info->fRC 	= batt_info->sCaMAH;
			}
		}
	}

	bmt_dbg("first sCaMAH:%d,fRC:%d,fRCPrev:%d,fcurr:%d,volt:%d\n",
			batt_info->sCaMAH,batt_info->fRC,batt_info->fRCPrev,
			batt_info->fCurr,(batt_info->fVolt * oz8806_cell_num));

	//back sCaMAH
	//H7====LOW VOLTAGE CHARGE PREVENT======
	if ((batt_info->fVolt < config_data->discharge_end_voltage) && (batt_info->fCurr > 0)) //charge and voltage < 3300
	{
		bmt_dbg("Low voltage %d charge current %d detected\n",batt_info->fVolt,batt_info->fCurr);
		if (batt_info->fRC > batt_info->fRCPrev)	//CAR increased
		{
			bmt_dbg("Low voltage fRC limit triggered %d\n",batt_info->fRCPrev);
			batt_info->fRC = batt_info->fRCPrev;	//Limit CAR as previous	
			afe_write_car(batt_info->fRC);			//write back CAR
		}
	}
	//H7====LOW VOLTAGE CHARGE PREVENT======

	gas_gauge->sCtMAH += batt_info->fRC - batt_info->fRCPrev;
	gas_gauge->discharge_sCtMAH += batt_info->fRCPrev - batt_info->fRC;
	age_update();

	if(gas_gauge->sCtMAH < num_0)  gas_gauge->sCtMAH = num_0;
	if(gas_gauge->discharge_sCtMAH < num_0)  gas_gauge->discharge_sCtMAH = num_0;

	//bmu_call();

	adapter_status = get_adapter_status();
	if ((batt_info->fCurr <= gas_gauge->discharge_current_th) ||
		(adapter_status == O2_CHARGER_BATTERY))
		discharge_process();
	else
		charge_process();

	bmt_dbg("second sCaMAH:%d,fRC:%d,fRCPrev:%d,fcurr:%d,volt:%d\n",
			batt_info->sCaMAH,batt_info->fRC,batt_info->fRCPrev,
			batt_info->fCurr,(batt_info->fVolt * oz8806_cell_num));

	if(gas_gauge->charge_end)
	{
		if(!charge_end_flag)
		{
			bmt_dbg("enter BMT fgu charge end\n");
			charge_end_flag = num_1;
			charge_end_process();
		}
	}
	else
	{
		charge_end_flag = num_0;
	}

	if(gas_gauge->discharge_end)
	{
		if(!discharge_end_flag)
		{
			discharge_end_flag = num_1;
			discharge_end_process();
		}
	}
	else
	{
		discharge_end_flag = num_0;
	}

	oz8806_over_flow_prevent();
	afe_read_cycleCount(&batt_info->cycle_count);
	afe_read_accmulate_dsg_data(&batt_info->dsg_data);
	/*
	//very dangerous
	if(batt_info->fVolt <= (config_data->discharge_end_voltage - num_100))
	{
		ret = afe_read_cell_volt(&batt_info->fVolt);
		if((ret >=0) && (batt_info->fVolt > 2500))
			discharge_end_process();
	}
	*/
	//gas_gauge->bmu_tick++;
	previous_loop_timex = time_sec;
	time_sec = oz8806_get_system_boot_time();
		bmt_dbg("----------------------------------------------------\n"
		"[bmt-fg] VERSION: %s-%x\n"
		"[bmt-fg] TABLEVERSION: %s\n" 
		"[bmt-fg] battery_ok: %d,time_x: %d\n"
		"[bmt-fg] chg_fcc_update: %d, disg_fcc_update: %d\n"
		"[bmt-fg] fVolt: %d   fCurr: %d   fCellTemp: %d   fRSOC: %d\n"
		"[bmt-fg] sCaMAH: %d, sCtMAH1: %d, sCtMAH2: %d\n"
		"[bmt-fg] fcc: %d, fRC: %d\n"
		"[bmt-fg] i2c_error_times: %d, acc_data:%d\n"
		"[bmt-fg] charge_end: %d, discharge_end: %d\n"
		"[bmt-fg] adapter_status: %d, soh: %d, cycle: %d, dsg_data:%d\n"
		"[bmt-fg] ----------------------------------------------------\n",
		VERSION,calculate_version,get_table_version(),batt_info->Battery_ok,time_sec,
		gas_gauge->charge_fcc_update,gas_gauge->discharge_fcc_update,
		(batt_info->fVolt * oz8806_cell_num),batt_info->fCurr,batt_info->fCellTemp,batt_info->fRSOC,
		batt_info->sCaMAH,gas_gauge->sCtMAH,gas_gauge->discharge_sCtMAH,
		gas_gauge->fcc_data,batt_info->fRC,
		batt_info->i2c_error_times, accmulate_dsg_data,
		gas_gauge->charge_end,gas_gauge->discharge_end,
		adapter_status,age_soh,batt_info->cycle_count,batt_info->dsg_data);

	if(batt_info->sCaMAH > (full_charge_data))
	{
		bmt_dbg("big error sCaMAH is %d\n",batt_info->sCaMAH);
		batt_info->sCaMAH = full_charge_data;	
	}

	if(batt_info->sCaMAH < (gas_gauge->fcc_data / 100 - 1))
	{
		//bmt_dbg("big error sCaMAH is %d\n",batt_info->sCaMAH);
		batt_info->sCaMAH = gas_gauge->fcc_data / 100 - 1;	
	}

	gas_gauge->fcc_data=  config_data->design_capacity;

end:
	batt_info->Battery_ok =1;
	bmt_dbg("Battery_ok %d\n",batt_info->Battery_ok);

	ret  = afe_read_board_offset(&data);
	bmt_dbg("config_data->board_offset %d, reg_board_offset %d\n",config_data->board_offset,data);
	if (ret >= 0)
	{
		if (config_data->board_offset != 0) 
		{
			//if (data != config_data->board_offset)
			if(ABS(data, config_data->board_offset) > 3)
			{
				afe_write_board_offset(config_data->board_offset);
				bmt_dbg("BMT fgu board offset error1 is %d \n",data);
			}
		}
		else
		{
			if((data > 10) || (data <= 0))
			{
				bmt_dbg("BMT fgu board offset error2 is %d \n",data);
				afe_write_board_offset(7);
			}
		}
	}
	bmt_dbg("batt_info->sCaMAH %d\n",batt_info->sCaMAH);
	bmt_dbg("-----------------------bmu_polling_loop end--------------------\n");
}

/*****************************************************************************
* Description:
*		 prevent oz8806 over flow 
* Parameters:
*		None
* Return:
*		None
*****************************************************************************/
static void oz8806_over_flow_prevent(void)
{
	int32_t ret;
	int32_t data;

	if((batt_info->fRSOC > num_0) && gas_gauge->discharge_end)
		gas_gauge->discharge_end = num_0;
   
	if((batt_info->fRSOC < num_100) && gas_gauge->charge_end)
		gas_gauge->charge_end = num_0;

	if(batt_info->fRC< num_0)
	{
		if(batt_info->fVolt >= (config_data->charge_cv_voltage - 200))
		{
			batt_info->fRC = gas_gauge->fcc_data -num_1;
		}
		else
		{
			if(batt_info->fRSOC > 0)
				batt_info->fRC = batt_info->fRSOC * gas_gauge->fcc_data / num_100 - num_1;
			else
				batt_info->fRC = batt_info->sCaMAH;
		}

		afe_write_car(batt_info->fRC);
		batt_info->fRCPrev = batt_info->fRC;
	}

	ret = afe_read_car(&data);
	if(ret<0)
		return;

	if(data < 5)
	{
		afe_write_car(batt_info->sCaMAH);
		batt_info->fRCPrev = batt_info->sCaMAH;
		batt_info->fRC 	= batt_info->sCaMAH;
		bmt_dbg("dddd car down is %d\n",data);
	}
	else if(data > (32768 * parameter->config->dbCARLSB * 1000 / config_data->fRsense -5))
	{
		afe_write_car(batt_info->sCaMAH);
		batt_info->fRCPrev = batt_info->sCaMAH;
		batt_info->fRC 	= batt_info->sCaMAH;
		bmt_dbg("dddd car up is %d\n",data);
	}

	if(ABS(batt_info->sCaMAH, data) > one_percent_rc)
	{
		if((batt_info->sCaMAH < gas_gauge->overflow_data ) && (batt_info->sCaMAH > 0))
		{

			afe_write_car(batt_info->sCaMAH);
			batt_info->fRCPrev = batt_info->sCaMAH;
			batt_info->fRC 	= batt_info->sCaMAH;
			if(parameter->config->debug){
				bmt_dbg("dddd write car batt_info->fRCPrev is %d\n",batt_info->fRCPrev);
				bmt_dbg("dddd write car batt_info->fRC is %d\n",batt_info->fRC);
				bmt_dbg("dddd write car batt_info->sCaMAH is %d\n",batt_info->sCaMAH);
			}
		}
	}

	if(batt_info->fRC > (full_charge_data))
	{
		bmt_dbg("more fRC %d\n",batt_info->fRC);
		batt_info->fRC = full_charge_data;
		batt_info->fRCPrev = batt_info->fRC;
		afe_write_car(batt_info->fRC);
	}
}

/*****************************************************************************
* Description:
*		charge fcc update process
* Parameters:
*		 None
* Return:
*		None
*****************************************************************************/
void charge_end_process(void)
{
	//FCC UPdate
	gas_gauge->fcc_data = config_data->design_capacity;
	batt_info->sCaMAH = full_charge_data;;

	afe_write_car(batt_info->sCaMAH);

	bmt_dbg("charge end\n");
	batt_info->fRSOC = num_100;
	gas_gauge->charge_end = num_1;
	charge_end_flag = num_1;
	//charger_finish = 0;
	gas_gauge->charge_fcc_update = 1;
	gas_gauge->discharge_fcc_update = 1;
	gas_gauge->discharge_sCtMAH = 0;
	power_on_flag = 0;
}

/*****************************************************************************
* Description:
*		discharge fcc update process
* Parameters:
*		 None
* Return:
*		None
*****************************************************************************/
void discharge_end_process(void)
{
	//FCC UPdate
	gas_gauge->fcc_data = config_data->design_capacity;

	batt_info->sCaMAH = gas_gauge->fcc_data / 100 - 1;

	afe_write_car(batt_info->sCaMAH);
	batt_info->fRCPrev = batt_info->sCaMAH;

	bmt_dbg("discharge end\n");
	batt_info->fRSOC = num_0;
	gas_gauge->discharge_end = num_1;
	gas_gauge->discharge_fcc_update = 1;
	gas_gauge->charge_fcc_update = 1;
	gas_gauge->sCtMAH = num_0;

}

static int32_t sbs_cal_lsb_factor(int32_t raw_val, int32_t calLSB, int32_t calFactor)
{
	return ((int64_t)raw_val * (int64_t)calLSB) / calFactor;
}

/****************************************************************************
 * Description:
 *		read oz8806 cell voltage
 * Parameters: 
 *		voltage: 	cell voltage in mV, range from -5120mv to +5120mv
 * Return:
 *		see I2C_STATUS_XXX define
 ****************************************************************************/
static int32_t oz8806_cell_voltage_read(int32_t *voltage)
{
	int32_t 	ret = 0,i = 0;
	int32_t 	reg = 0,reg1 = 0,reg2 = 0;
	uint32_t 	cell_voltage;

	for(i = 0;i < 2;i++)
	{
		ret = sd77428_read_addr(the_oz8806,0x40004114,&reg1);
		if(ret)
			return -1;
		ret = sd77428_read_addr(the_oz8806,0x40004114,&reg2);
		if(ret)
			return -1;
		reg = reg2;
		bmt_dbg("reg1:0x%04X,reg2:0x%04X,i:%d\n",reg1,reg2,i);
		if(reg1 == reg2)
			break;
	}

	cell_voltage = sbs_cal_lsb_factor((int32_t)(reg), ADC_VOLTLSB, ADC_VOLTLSB_FACTOR); 
	*voltage = cell_voltage;
	batt_info->i2c_error_times = 0;
	return ret;
}

/****************************************************************************
 * Description:
 *		read oz8806 cell OCV(open circuit voltage)
 * Parameters: 
 *		voltage: 	voltage in mV, range from -5120mv to +5120mv with 2.5mV LSB
 * Return:
 *		see I2C_STATUS_XXX define
 ****************************************************************************/
static int32_t oz8806_ocv_voltage_read(int32_t *voltage)
{
	int32_t 	ret = 0,i = 0;
	int32_t 	reg = 0,reg1 = 0,reg2 = 0;
	uint32_t 	cell_voltage;

	for(i = 0;i < 2;i++)
	{
		ret = sd77428_read_addr(the_oz8806,0x40004114,&reg1);
		if(ret)
			return -1;
		ret = sd77428_read_addr(the_oz8806,0x40004114,&reg2);
		if(ret)
			return -1;
		reg = reg2;
		bmt_dbg("reg1:0x%04X,reg2:0x%04X,i:%d\n",reg1,reg2,i);
		if(reg1 == reg2)
			break;
	}

	cell_voltage = sbs_cal_lsb_factor((int32_t)(reg), ADC_VOLTLSB, ADC_VOLTLSB_FACTOR); 
	*voltage = cell_voltage;
	return ret;
}

/****************************************************************************
 * Description:
 *		read oz8806 current
 * Parameters: 
 *		current: in mA, range is +/-64mv / Rsense with 7.8uv LSB / Rsense
 * Return:
 *		see I2C_STATUS_XXX define
 ****************************************************************************/
static int32_t oz8806_current_read(int32_t *data)
{

	int32_t 	ret = 0,i = 0;
	int32_t 	reg = 0,reg1 = 0,reg2 = 0;
	int32_t 	rsence;
	int32_t 	cell_current;
	rsence = parameter->config->fRsense;

	for(i = 0;i < 2;i++)
	{
		ret = sd77428_read_addr(the_oz8806,0x400041A8,&reg1);			//250ms
		if(ret)
			return -1;
		ret = sd77428_read_addr(the_oz8806,0x400041A8,&reg2);			//250ms
		if(ret)
			return -1;
		reg = reg2;
		bmt_dbg("reg1:0x%04X,reg2:0x%04X,i:%d\n",reg1,reg2,i);
		if(reg1 == reg2)
			break;
	}

	cell_current = (int16_t)reg;
	// pr_info("reg 0x%04X cell_current %d \n",reg, cell_current);

	cell_current = sbs_cal_lsb_factor((int32_t)(cell_current), ADC_CADCLSB, ADC_CADCLSB_FACTOR); 
	cell_current = cell_current * PARAM_RSENSE_AMPLIFIER / rsence;
	*data = cell_current;
	batt_info->i2c_error_times = 0;
	return ret;
}

/****************************************************************************
 * Description:
 *		read oz8806 temp
 * Parameters: 
 *		voltage: 	voltage value in mV, range is +/- 5120mV with 2.5mV LSB
 * Return:
 *		see I2C_STATUS_XXX define
 ****************************************************************************/
int32_t oz8806_temp_read(int32_t *voltage)
{
	int32_t 	ret = 0,i = 0;
	int32_t 	reg = 0,reg1 = 0,reg2 = 0;
	int32_t 	sbs_val ;

	for(i = 0;i < 2;i++)
	{
		ret = sd77428_read_addr(the_oz8806,0x40004118,&reg1);
		if(ret)
			return -1;
		ret = sd77428_read_addr(the_oz8806,0x40004118,&reg2);
		if(ret)
			return -1;
		reg = reg2;
		bmt_dbg("reg1:0x%04X,reg2:0x%04X,i:%d\n",reg1,reg2,i);
		if(reg1 == reg2)
			break;
	}
	sbs_val = sbs_cal_lsb_factor((int32_t)(reg), ADC_EXTMPLSB, ADC_EXTMPLSB_FACTOR);//mv
	*voltage = sbs_val;
	pr_info("bcy %d \n",sbs_val);
	return ret;
}

/****************************************************************************
 * Description:
 *		read oz8806 CAR
 * Parameters: 
 *		car: in mAHr, range is +/- 163840 uvHr/ Rsense with 5 uvHr LSB / Rsense
 * Return:
 *		see I2C_STATUS_XXX define
 ****************************************************************************/
static int32_t oz8806_car_read(int32_t *car)
{
	int32_t 	ret = 0,i = 0;
	int32_t 	reg = 0,reg1 = 0,reg2 = 0;
	int32_t 	rsence;
	uint32_t 	cc_lsb = 0;
	int32_t 	cc_data = 0;

	rsence = parameter->config->fRsense;

	for(i = 0;i < 2;i++)
	{
		ret = sd77428_read_addr(the_oz8806,0x400041B8,&reg1);
		if(ret)
			return -1;
		ret = sd77428_read_addr(the_oz8806,0x400041B8,&reg2);
		if(ret)
			return -1;
		reg = reg2;
		bmt_dbg("reg1:0x%04X,reg2:0x%04X,i:%d\n",reg1,reg2,i);
		if(reg1 == reg2)
			break;
	}

	ret = sd77428_read_addr(the_oz8806, 0x4000424C, &cc_lsb);
	if(ret)
		return -1;
	cc_lsb = cc_lsb & 0x03;

	cc_data = reg; 
	// 
	// cc_data = cc_data;
	cc_data *= LSB_CC_VOLT;
	cc_data <<= cc_lsb;	
	cc_data /= rsence;	
	*car = cc_data;
	return ret;
}

/****************************************************************************
 * Description:
 *		write oz8806 CAR
 * Parameters: 
 *		data: in mAHr, range is +/- 163840 uvHr/ Rsense with 5 uvHr LSB / Rsense
 * Return:
 *		see I2C_STATUS_XXX define
 ****************************************************************************/
static int32_t oz8806_car_write(int32_t data)
{
	int32_t 	temp;
	int32_t 	ret = 0;
	int32_t 	rsence;
	uint32_t 	cc_lsb = 0;

	ret = sd77428_read_addr(the_oz8806, 0x4000424C, &cc_lsb);
	if(ret)
		return -1;
	rsence = parameter->config->fRsense;
	temp = data;
	temp = temp * rsence;
	temp >>= cc_lsb;	//transfer to CAR
	temp = temp/LSB_CC_VOLT;

	ret = sd77428_write_addr(the_oz8806,0x400041B8,temp);
	return ret;
}

/****************************************************************************
 * Description:
 *		read oz8806 board offset
 * Parameters: 
 *		data: in mA, +/- 8mV / Rsense with 7.8uV / Rsense 
 * Return:
 *		see I2C_STATUS_XXX define
 ****************************************************************************/
static int32_t oz8806_board_offset_read(int32_t *data)
{
	int32_t 	ret = 0;
	int32_t 	reg = 0;
	int32_t 	rsence;
	int32_t 	cell_current;
	rsence = parameter->config->fRsense;

	ret = sd77428_read_addr(the_oz8806,0x400041A4,&reg);			//1s
	if(ret)
		return -1;

	cell_current = sbs_cal_lsb_factor((int32_t)(reg), ADC_CADCLSB, ADC_CADCLSB_FACTOR); 
	cell_current = cell_current * PARAM_RSENSE_AMPLIFIER / rsence;
	*data = cell_current;
	batt_info->i2c_error_times = 0;
	return ret;
}

/****************************************************************************
 * Description:
 *		write oz8806 board offset
 * Parameters: 
 *		data: the data will be wrriten
 * Return:
 *		see I2C_STATUS_XXX define
 ****************************************************************************/
static int32_t oz8806_board_offset_write(int32_t data)
{
	int32_t ret;
	int32_t over_data;
	int32_t rsence;
	rsence = parameter->config->fRsense;

	if(data > num_0){
		data = data * rsence / PARAM_RSENSE_AMPLIFIER;
		data = data * ADC_CADCLSB_FACTOR / ADC_CADCLSB;
		if(data > 0x7f)data = 0x7f;
		ret = sd77428_write_addr(the_oz8806,0x400041A4,(uint16_t)data);
	}
	else if(data < num_0){
		over_data = (-1024 * 78 * 1000) / (config_data->fRsense * num_10);
		if(data < over_data)
			data = over_data;
		data = data * config_data->fRsense * num_10 / 78;
		data /= 1000;
		data = (data & 0x007f) | 0x0080;
		ret = sd77428_write_addr(the_oz8806,0x400041A4,(uint16_t)data);
	}
	else
		ret = sd77428_write_addr(the_oz8806,0x400041A4,num_0);
	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to read cell voltage 
 * Parameters:
 *		vol:	buffer to store voltage read back
 * Return:
 *		see I2C_STATUS_XXX define
 * Note:
 *		it's acceptable if one or more times reading fail
 *****************************************************************************/
int32_t afe_read_cell_volt(int32_t *voltage)
{
	int32_t ret;
	uint8_t i;
	int32_t buf;

	for(i = num_0; i < RETRY_CNT; i++){
		ret = oz8806_cell_voltage_read(&buf);
		if(ret >= num_0)
				break;
		msleep(5);
	}
	if(i >= RETRY_CNT){
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	} 
	if(oz8806_cell_num > num_1)
	{
		//*voltage = buf * num_1000 / res_divider_ratio;
		bmt_dbg("read from register %d mv \n",buf);
		*voltage = buf * res_divider_ratio / num_1000; // res_divider_ratio = ((Rpull + Rdown) / Rdown) * 1000
		*voltage /= oz8806_cell_num;
	}
	else
	{
		*voltage = buf * one_cell_divider_ratio / 1000;
		*voltage /= 2;
	}
	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to read cell ocv  voltage 
 * Parameters:
 *		vol:	buffer to store voltage read back

 * Return:
 *		see I2C_STATUS_XXX define
 * Note:
 *		it's acceptable if one or more times reading fail
 *****************************************************************************/
static int32_t afe_read_ocv_volt(int32_t *voltage)
{
	int32_t ret;
	uint8_t i;
	int32_t buf;

	for(i = num_0; i < RETRY_CNT; i++){
		ret = oz8806_ocv_voltage_read(&buf);
		if(ret >= num_0)
				break;
	}
	if(i >= RETRY_CNT){
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	} 

	if(oz8806_cell_num > num_1)
	{
		*voltage = buf * res_divider_ratio / num_1000;
		*voltage /= oz8806_cell_num;
	}
	else
	{
		*voltage = buf * one_cell_divider_ratio / 1000;
		*voltage /= 2;
	}

	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to read current 
 * Parameters:
 *		dat:	buffer to store current value read back	
 * Return:
 *		see I2C_STATUS_XXX define
 * Note:
 *		it's acceptable if one or more times reading fail
 *****************************************************************************/
int32_t afe_read_current(int32_t *dat)
{
	int32_t ret;
	uint8_t i;
	int32_t buf;

	for(i = num_0; i < RETRY_CNT; i++){
		ret = oz8806_current_read(&buf);
		if(ret >= num_0)
				break;
		msleep(5);
	}
	if(i >= RETRY_CNT){
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	} 
	*dat = buf;
	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to read CAR(accummulated current calculation)
 * Parameters:
 *		dat:	buffer to store current value read back	
 * Return:
 *		see I2C_STATUS_XXX define
 * Note:
 *		it's acceptable if one or more times reading fail
 *****************************************************************************/
static int32_t afe_read_car(int32_t *dat)
{
	int32_t ret;
	uint8_t i;
	int32_t buf;

	for(i = num_0; i < RETRY_CNT; i++){
		ret = oz8806_car_read(&buf);
		if(ret >= num_0)
				break;
		msleep(5);
	}
	if(i >= RETRY_CNT){
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	} 
	*dat = buf;
	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to write car 
 * Parameters:
 *		dat:	buffer to store current value read back	
 * Return:
 *		see I2C_STATUS_XXX define
 * Note:
 *		it's acceptable if one or more times reading fail
 *****************************************************************************/
static int32_t afe_write_car(int32_t date)
{
	int32_t ret;
	uint8_t i;


	for(i = num_0; i < RETRY_CNT; i++){
		ret = oz8806_car_write(date);
		if(ret >= num_0)	break;
		msleep(5);
	}
	if(i >= RETRY_CNT){
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	} 
	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to write board offset 
 * Parameters:
 *		dat:	buffer to store current value read back	
 * Return:
 *		see I2C_STATUS_XXX define
 * Note:
 *		it's acceptable if one or more times reading fail
 *****************************************************************************/
static int32_t afe_write_board_offset(int32_t date)
{
	int32_t ret;
	uint8_t i;


	for(i = num_0; i < RETRY_CNT; i++){
		ret = oz8806_board_offset_write(date);
		if(ret >= num_0)	break;
	}
	if(i >= RETRY_CNT){
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	} 
	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to read board offset 
 * Parameters:
 *		dat:	buffer to store current value read back	
 * Return:
 *		see I2C_STATUS_XXX define
 * Note:
 *		it's acceptable if one or more times reading fail
 *****************************************************************************/
static int32_t afe_read_board_offset(int32_t *dat)
{
	int32_t ret;
	uint8_t i;
	int32_t buf;

	for(i = num_0; i < RETRY_CNT; i++){
		ret = oz8806_board_offset_read(&buf);
		if(ret >= num_0)
				break;
	}
	if(i >= RETRY_CNT){
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	} 
	*dat = buf;
	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to read temp
 * Parameters:
 *		dat:	buffer to store current value read back	
 * Return:
 *		see I2C_STATUS_XXX define
 * Note:
 *		it's acceptable if one or more times reading fail
 *****************************************************************************/
static int32_t afe_read_cell_temp(int32_t *data)
{
	int32_t ret = 0;
	uint8_t i = 0;
	uint32_t temp = 0;
	uint32_t tempbuf = 0;
	uint32_t temp1 = 0;
	int32_t buf;
	int32_t iRNtc = 0;

	if (gas_gauge->ext_temp_measure)
		return num_0;

	for(i = num_0; i < RETRY_CNT; i++){
		ret = oz8806_temp_read(&buf);
		if(ret >= num_0)	break;
		msleep(5);
	}
	if(i >= RETRY_CNT){
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	} 

	if((buf >= config_data->temp_ref_voltage)|| (buf <= num_0))
		*data = num_25;
	else
	{
		tempbuf = (uint32_t)buf;
		temp = tempbuf * config_data->temp_pull_up;
		temp1 = (config_data->temp_ref_voltage-tempbuf) + CONSTANT_6UA*config_data->temp_pull_up/1000;
		temp = 	(uint32_t)(temp/temp1);

		if(0XFFFFFFFF != config_data->temp_parrel_res)
			iRNtc = (int32_t)((config_data->temp_parrel_res*temp) / (config_data->temp_parrel_res-temp));
		else//parrel resistance not exist
			iRNtc = (int32_t)temp;
		pr_info("bmt iRNtc %d\n",iRNtc);
		*data =	one_latitude_table(parameter->cell_temp_num,parameter->temperature,iRNtc);
	}
	bmt_dbg("1111111111111 r is %d\n",temp);
	return ret;
}

/****************************************************************************
 * Description:
 *		read oz8806 cycleCount
 * Parameters: 
 *		
 * Return:
 *		
 ****************************************************************************/
static int32_t oz8806_cycleCount_read(uint32_t *cycleCount)
{
	int32_t 	ret = 0;
	uint32_t 	temp = 0;
	
	ret = sd77428_read_addr(the_oz8806,0x4000435c,&temp);
	if(ret)
		return -1;

	*cycleCount = temp;
	
	return ret;
}

/****************************************************************************
 * Description:
 *		write oz8806 cycleCount
 * Parameters: 
 *		
 * Return:
 *		
 ****************************************************************************/
static int32_t oz8806_cycleCount_write(uint32_t data)
{
	int32_t 	ret = 0;

	ret = sd77428_write_addr(the_oz8806,0x4000435c,data);
	if(ret)
		return -1;
	
	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to read cycle count
 * Parameters:
 *		
 * Return:
 *		
 * Note:
 *		
 *****************************************************************************/
int32_t afe_read_cycleCount(uint32_t *dat)
{
	int32_t ret;
	uint8_t i;
	uint32_t buf;

	for(i = num_0; i < RETRY_CNT; i++){
		ret = oz8806_cycleCount_read(&buf);
		if(ret >= num_0)
				break;
		msleep(5);
	}
	if(i >= RETRY_CNT){
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	} 
	*dat = buf;
	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to write cycle count 
 * Parameters:
 *		
 * Return:
 *		
 * Note:
 *		
 *****************************************************************************/
int32_t afe_write_cycleCount(uint32_t dat)
{
	int32_t ret;
	uint8_t i;

	for(i = num_0; i < RETRY_CNT; i++){
		ret = oz8806_cycleCount_write(dat);
		if(ret >= num_0)	break;
		msleep(5);
	}
	if(i >= RETRY_CNT){
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	} 
	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to read accmulate_dsg_data
 * Parameters:
 *		
 * Return:
 *		
 * Note:
 *		
 *****************************************************************************/
int32_t afe_read_accmulate_dsg_data(uint32_t *dat)
{
	int32_t ret;
	uint8_t i;
	uint32_t buf;

	for(i = num_0; i < RETRY_CNT; i++){
		ret = oz8806_accDsgData_read(&buf);
		if(ret >= num_0)
				break;
		msleep(5);
	}
	if(i >= RETRY_CNT){
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	} 
	*dat = buf;
	return ret;
}

/****************************************************************************
 * Description:
 *		read oz8806 accmulate_dsg_data
 * Parameters: 
 *		
 * Return:
 *		
 ****************************************************************************/
static int32_t oz8806_accDsgData_read(uint32_t *dat)
{
	int32_t 	ret = 0;
	uint32_t 	temp = 0;
	
	ret = sd77428_read_addr(the_oz8806,0x20000C88,&temp);//ram 20000C88-20000D00
	if(ret)
		return -1;

	*dat = temp;
	
	return ret;
}

/****************************************************************************
 * Description:
 *		write oz8806 accmulate_dsg_data
 * Parameters: 
 *		
 * Return:
 *		
 ****************************************************************************/
static int32_t oz8806_accDsgData_write(uint32_t data)
{
	int32_t 	ret = 0;

	ret = sd77428_write_addr(the_oz8806,0x20000C88,data);
	if(ret)
		return -1;
	
	return ret;
}

/*****************************************************************************
 * Description:
 *		wrapper function to write accmulate_dsg_data 
 * Parameters:
 *		
 * Return:
 *		
 * Note:
 *		
 *****************************************************************************/
int32_t afe_write_accmulate_dsg_data(uint32_t dat)
{
	int32_t ret;
	uint8_t i;

	for(i = num_0; i < RETRY_CNT; i++){
		ret = oz8806_accDsgData_write(dat);
		if(ret >= num_0)	break;
		msleep(5);
	}
	if(i >= RETRY_CNT){
		batt_info->i2c_error_times++;
		bmt_dbg("yyyy. %s failed, ret %d\n", __func__, ret);
		return ret;
	} 
	return ret;
}
/*****************************************************************************
 * Description:
 *		pec_calculate
 * Parameters:
 *		ucCrc:	source
 *		ucData: data
 * Return:
 *		crc data
 * Note:
 *
 *****************************************************************************/
void bmu_reinit(void)
{
	msleep(1500);//wait for poocv
	wait_ocv_flag = 0;
	power_on_flag = 0;
	gas_gauge->bmu_init_ok = 0;
	batt_info->i2c_error_times = 0;
	wait_ocv_flag_fun();
	bmu_wait_ready();
}
EXPORT_SYMBOL(bmu_reinit);

int32_t bmulib_init(void)
{
	int32_t ret = 0;
	uint32_t byte_num = 0;
	uint8_t * p = NULL;

	bmt_dbg("%s\n", __func__);

	//NOTICE: Don't change the sequence of calling below functions

	byte_num = sizeof(config_data_t) + sizeof(bmu_data_t) +  sizeof(gas_gauge_t);
	p = kzalloc(byte_num, GFP_KERNEL);
	bmu_kernel_memaddr = (unsigned long)p;
	
	if (!bmu_kernel_memaddr)
		goto fail_mem;

	parameter_customer.client = oz8806_get_client();
	if (!parameter_customer.client)
		goto fail_i2c;
	bmt_dbg("i2c address:%x\n", parameter_customer.client->addr);
	//struct oz8806_data *data = i2c_get_clientdata(parameter_customer.client);

	bmu_init_parameter(&parameter_customer);

	bmu_init_chip(&parameter_customer);

	oz8806_set_batt_info_ptr(batt_info);

	oz8806_set_gas_gauge(gas_gauge);

	bmu_polling_loop();
	return ret;
fail_mem:
	bmt_dbg("%s: bmu_kernel_memaddr error\n", __func__);
fail_i2c:
	bmt_dbg("%s: oz8806 i2c register error\n", __func__);
	return -1;
}

void bmulib_exit(void)
{
	bmt_dbg("%s\n", __func__);
	if (bmu_kernel_memaddr)
		kfree((int8_t *)bmu_kernel_memaddr);
	bmu_kernel_memaddr = 0;
}

MODULE_DESCRIPTION("FG Charger Driver");
MODULE_LICENSE("GPL v2");