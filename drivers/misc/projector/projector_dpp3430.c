/*
 * projector.c  --  projector module driver
 *
 * Copyright (C) 2010 Samsung Electronics Co.Ltd
 * Author: Inbum Choi <inbum.choi@samsung.com>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/projector/projector.h>
#include <linux/projector/beam_gpio_i2c.h>

#include "prj_dpp3430_data.h"

#ifndef	GPIO_LEVEL_LOW
#define GPIO_LEVEL_LOW		0
#define GPIO_LEVEL_HIGH		1
#endif

/*
#define DPP_I2C_EMUL
#define LED_COMPENSATION
*/
#define PROJECTOR_DEBUG

// PRJ_WRITE, PRJ_READ
#define DPP3430_I2C			0x1B   
// PRJMSPD_WRITE, PRJMSPD_READ
#define M08980_I2C			0x36    

static DEFINE_MUTEX(beam_mutex);

#define DPPDATAFLASH(ARRAY)	(dpp_flash(ARRAY, sizeof(ARRAY)/sizeof(ARRAY[0])))
#define M08980CTRL(ARRAY) 		(dpp_flash(ARRAY, sizeof(ARRAY)/sizeof(ARRAY[0])))

#define ChangeDuty(array, data) (array[3] = (data))		// I2C_Write( 0x36, 0x22, 0xSeq Number); Cal data set 19th Sequence number

//static int motor_step;
/**
 * Keep the motor step between 0 and MAX_MOTOR_STEP
 * For upper layers
 */
#define MAX_MOTOR_STEP 60
/**
 * User for upper lears to get approximatly motor step
 */
static int motor_abs_step = MAX_MOTOR_STEP / 2;

// BEGIN CSLBSuP a.jakubiak
// static int once_motor_verified;
/**
 * Store number of motor steps since last out_of_range
 * If the number is small <30, so we can be quite sure that the kernel value is accurate
 * If the number is big, so we are not sure that the kernel value is accurate
 */
static int num_of_steps_since_out_of_range = 10000; 
/**
 * CAIC status 1-enabled, 0-disabled or unknown
 */
static int projector_caic = 0;

/**
 * Continue motor move 1-true, 0-false - for fast focus
 */
static int motor_conti_move = 0; 

/**
 * Duty to be set
 */
static int duty_to_set = 0;

/**
 * Current to be set
 */
static int red_to_set, green_to_set, blue_to_set;

/**
 * Labb to be set
 */
static int labb_to_set = 0;
/**
 * Keystone angle to be set
 */
static int keystone_angle_to_set = 0;
/**
 * Veryfy status to be set
 */
static int verify_status_to_set = 0;

/**
 * siop_flag_to_use
 */
static int siop_flag_to_use = SIOP_FLAG_FREEZE; // default

// END CSLBSuP


static int verify_value;



static int motor_direction=MOTOR_DIRECTION;

static int dpp343x_fwversion=0;
static int dpp343x_fwupdating=0;
unsigned char  *fw_buffer  = NULL;


static int brightness = BRIGHT_HIGH;
static int seq_array[3]={0,3,4};//140108_sks

struct workqueue_struct *projector_work_queue;

struct work_struct projector_work_power_on;
struct work_struct projector_work_power_off;
struct work_struct projector_work_motor_cw;
struct work_struct projector_work_motor_ccw;
struct work_struct projector_work_motor_cw_conti;
struct work_struct projector_work_motor_ccw_conti;
struct work_struct projector_work_testmode_on;
struct work_struct projector_work_rotate_screen;
struct work_struct projector_work_read_test;
struct work_struct projector_work_fw_update;
struct work_struct projector_work_set_brightness;
struct work_struct projector_work_projector_verify;
struct work_struct projector_work_keystone_control_on;
struct work_struct projector_work_keystone_control_off;
struct work_struct projector_work_keystone_angle_set;
struct work_struct projector_work_labb_control_on;
struct work_struct projector_work_labb_control_off;
struct work_struct projector_work_labb_strenght_set;
struct work_struct projector_work_duty_set;
struct work_struct projector_work_caic_set_on;
struct work_struct projector_work_caic_set_off;
struct work_struct projector_work_current_set;
struct work_struct projector_work_info;


struct device *sec_projector;
extern struct class *sec_class;

static int screen_direction=0;

static int status=PRJ_OFF;
static int saved_pattern = -1;

static unsigned int not_calibrated = 0xFF;
unsigned char RGB_BUF[MAX_LENGTH];
unsigned char cal_data[MAX_LENGTH];
static unsigned char seq_number;

volatile unsigned char flash_rgb_level_data[3][3][2] = {{{0,},},};
volatile unsigned char caldata_1p2W_to_0p6W[7][3][2] = {{{0,},},}; //sks_1217

unsigned char pre_dpp_Read_ShortStatus;
unsigned char pre_dpp_Read_SystemStatus;
unsigned char pre_dpp_Read_CommStatus;
unsigned char pre_M08980Read_LDO_STAT;
unsigned char pre_M08980Read_BOOST_STAT;
unsigned char pre_M08980Read_DMD_STAT_RB;
unsigned char pre_M08980Read_MOTOR_BUCK_STAT;
unsigned char pre_M08980Read_BUCKBOOST_STAT;
unsigned char pre_M08980Read_RED_VBAT_LSB;
unsigned char pre_M08980Read_GREEN_VBAT_LSB;
unsigned char pre_M08980Read_BLUE_VBAT_LSB;
unsigned char pre_M08980Read_RED_VLED_LSB;
unsigned char pre_M08980Read_GREEN_VLED_LSB;
unsigned char pre_M08980Read_BLUE_VLED_LSB;
unsigned char pre_M08980Read_VLED_MSB;
unsigned char pre_M08980Read_TEMP_LSB;
unsigned char pre_M08980Read_TEMP_MSB;
unsigned char pre_M08980Read_ADC_STATUS;
unsigned char pre_M08980Read_PWM_READBACK;
unsigned char pre_M08980Read_IOUT0_HDRM_DAC_READBACK;
unsigned char pre_M08980Read_IOUT1_HDRM_DAC_READBACK;
unsigned char pre_M08980Read_IOUT2_HDRM_DAC_READBACK;
unsigned char pre_M08980Read_DEVICE_ID;
unsigned char pre_M08980Read_ALARM0;
unsigned char pre_M08980Read_ALARM1;

struct projector_dpp3430_info {
	struct i2c_client			*client;
	struct projector_dpp3430_platform_data	*pdata;
	
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend			earlysuspend;
#endif

};

struct projector_M08980_info {
	struct i2c_client			*client;

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend			earlysuspend;
#endif

};

struct projector_dpp3430_info *info = NULL;
struct projector_M08980_info *M08980_info = NULL;



/**
 * Private declarations
 */


// Move motor one step CW 
void execute_motor_cw(void);

// Move motor one step ccw
void execute_motor_ccw(void);

// convert int to high and low byte
void int2bytes( unsigned int input, unsigned char *low, unsigned char *high );


int dpp_flash(unsigned char *DataSetArray, int iNumArray);
int get_proj_status(void);
int get_proj_motor_status(void);
int get_proj_rotation(void);
int get_proj_brightness(void);
void set_proj_status(int enProjectorStatus);
void rotate_proj_screen(int bLandscape);
void project_internal(int pattern);
void pwron_seq_gpio(void);
void pwron_seq_direction(void);
void pwron_seq_source_res(int value);
void pwron_seq_fdr(void);
void proj_pwron_seq(void);
void proj_pwroff_seq(void);
void proj_pwroff_gpioOnly(void);
void dpp343x_get_version_info(void);
void dpp343x_fwupdate(void);
void projector_caic_enable(void);
void projector_caic_disable(void);
void set_Gamma(int level);
void switch_to_lcd(void);
void switch_to_pattern(void);
void set_led_current(void);
void set_led_duty(void);
void set_custom_led_current(int red, int green, int blue);
void set_custom_led_duty(int duty);
void set_CAIC_CurrentDuty( int siop_flag );
int check_dpp_status(void);
int check_short_status(void);
void read_projector_status(void);
void flash_pattern(int pattern);
int projector_queue_work(struct work_struct *work);
void projector_flush_workqueue();
void freeze_on();
void freeze_off();
void ti_dmd_on();
void ti_dmd_off();
void ti_labb_on();
void ti_labb_off();


/**
 * Definition block
 */




int dpp_flash( unsigned char *DataSetArray, int iNumArray)
{
	int i = 0;
	int CurrentDataIndex = 0;
	int Bytes = 0;
	int R_Bytes = 0;
	int cnt=0;

	mutex_lock(&beam_mutex);

#ifdef PROJECTOR_DEBUG
	printk(KERN_INFO "[%s] \n", __func__);
#endif

	for (i = 0; i < iNumArray;) 
	{
        int command = DataSetArray[i + OFFSET_I2C_DIRECTION];
        switch(command) {
            case PRJ_READ:
            case PRJMSPD_READ:
                Bytes =  DataSetArray[i + OFFSET_I2C_NUM_BYTES];
                R_Bytes = DataSetArray[i + OFFSET_I2C_NUM_BYTES + 1];
                CurrentDataIndex = i + OFFSET_I2C_DATA_START + 1;
                break;
            default:
			    Bytes =  DataSetArray[i + OFFSET_I2C_NUM_BYTES];
        		R_Bytes = 0;
		    	CurrentDataIndex = i + OFFSET_I2C_DATA_START;
                break;
		}


        switch(command)
        {
            case PRJ_WRITE:
                beam_i2c_write(info->client, DPP3430_I2C, &DataSetArray[CurrentDataIndex], Bytes);
                //XXX msleep(2);
#ifdef PROJECTOR_DEBUG
                printk(KERN_INFO "[%s] PRJ_WRITE [%d]:", __func__, Bytes);
                for(cnt=0;cnt<Bytes;cnt++)
                    printk("0x%02X ", DataSetArray[CurrentDataIndex+cnt]);
                printk("\n");
#endif		
                break;

            case PRJMSPD_WRITE:
                beam_i2c_write(M08980_info->client, M08980_I2C, &DataSetArray[CurrentDataIndex], Bytes);
                //XXX msleep(1);
#ifdef PROJECTOR_DEBUG
                printk(KERN_INFO "[%s] PRJMSPD_WRITE [%d]:", __func__, Bytes);
                for(cnt=0;cnt<Bytes;cnt++)
                    printk("0x%02X ", DataSetArray[CurrentDataIndex+cnt]);
                printk("\n");
#endif		
                break;

            case PRJ_READ:
                memset(RGB_BUF, 0x0, sizeof(RGB_BUF));
                beam_i2c_read(info->client, DPP3430_I2C, &DataSetArray[CurrentDataIndex], RGB_BUF, Bytes, R_Bytes);
                //XXX msleep(2);
#ifdef PROJECTOR_DEBUG
                printk(KERN_INFO "[%s] PRJ_READ[%d] ", __func__, R_Bytes);
                for(cnt=0;cnt<R_Bytes;cnt++)
                    printk("0x%02X ",RGB_BUF[cnt]);
                printk("\n");
#endif
                break;

            case PRJMSPD_READ:
                memset(RGB_BUF, 0x0, sizeof(RGB_BUF));

                beam_i2c_read(M08980_info->client, M08980_I2C, &DataSetArray[CurrentDataIndex], RGB_BUF, Bytes, R_Bytes);

#ifdef PROJECTOR_DEBUG
                printk(KERN_INFO "[%s] PRJMSPD_READ[%d] ", __func__, R_Bytes);
                for(cnt=0;cnt<R_Bytes;cnt++)
                    printk("0x%02X ",RGB_BUF[cnt]);
                printk("\n");
#endif
                break;

            case PRJ_FWWRITE:
                beam_i2c_write(info->client, DPP3430_I2C, fw_buffer, 1025);
#ifdef PROJECTOR_DEBUG
                printk(KERN_INFO "[%s] PRJ_FWWRITE [%d]:", __func__, 1025);
                for(cnt=0;cnt<17;cnt++)
                    printk("0x%02X ", fw_buffer[cnt]);
                printk("\n");
#endif		
                break;

            default:
                printk(KERN_INFO "[%s] %d: data is invalid !!\n", __func__, command);
                return -EINVAL;

        }
        i = (CurrentDataIndex + Bytes + R_Bytes);
	}

	mutex_unlock(&beam_mutex);

	return 0;
}

void set_proj_status(int enProjectorStatus)
{
	status = enProjectorStatus;
	printk(KERN_INFO "[%s] projector status : %d\n", __func__, status);
}

int get_proj_status(void)
{
	printk(KERN_INFO "[%s] : %d\n", __func__,status);
	return status;
}

int get_proj_motor_status(void)
{
	int pi_int;

	pi_int = gpio_get_value(info->pdata->gpio_pi_int);

//
// #ifdef PROJECTOR_DEBUG
//	printk(KERN_ERR "[%s] (%d) \n", __func__,pi_int);
// #endif

	if(pi_int)
		return MOTOR_OUTOFRANGE;
	else
		return MOTOR_INRANGE;
}	


EXPORT_SYMBOL(get_proj_status);

static void projector_motor_cw_work(struct work_struct *work)
{
	int i;
	int count;
	
	M08980CTRL(M08980_PI_LDO_Enable);		
 
	/* H/W Bug Fix : When the direction is changed, motor control command have be sent twice*/
	if(motor_direction==MOTOR_CCW)
	{
		count = 3;
	}
	else
	{
		count = 1;
	}
 
#ifdef PROJECTOR_DEBUG
	printk(KERN_INFO "[%s] CW 1 step(%d)\n", __func__,count);
#endif	
	for(i=0;i<count;i++)
	{	
        execute_motor_cw();
        num_of_steps_since_out_of_range++;
	}
	motor_abs_step--;
	
	if(get_proj_motor_status() == MOTOR_OUTOFRANGE)
	{
		for(i=0;i<30;i++)
		{
            execute_motor_ccw();

			if(get_proj_motor_status() == MOTOR_INRANGE) break;
		}
        // BEGIN CSLBSuP a.jakubiak
        num_of_steps_since_out_of_range = 0;
        motor_abs_step = 0;
        // END CSLBSuP a.jakubiak

		printk(KERN_INFO "[%s] Motor out of ragne(%d) : %d \n", __func__,i, motor_abs_step);
	} else if (motor_abs_step < 0 ) {
        motor_abs_step = 0;
    }
	
	M08980CTRL(M08980_PI_LDO_Disable);
	
}

static void projector_motor_ccw_work(struct work_struct *work)
{
	int i;
	int count;


	M08980CTRL(M08980_PI_LDO_Enable);		

	/* H/W Bug Fix : When the direction is changed, motor control command have be sent twice*/
	if(motor_direction==MOTOR_CW)
	{
		count = 3;
	}
	else
	{
		count = 1;
	}

#ifdef PROJECTOR_DEBUG
	printk(KERN_INFO "[%s] CCW 1 step(%d)\n", __func__,count);
#endif
	for(i=0;i<count;i++)
	{	
        execute_motor_ccw();
        num_of_steps_since_out_of_range++;
	}
	motor_abs_step++;

	if(get_proj_motor_status() == MOTOR_OUTOFRANGE)
	{
		int i;
		for(i=0;i<30;i++)
		{
            execute_motor_cw();

			if(get_proj_motor_status() == MOTOR_INRANGE) break;
		}
        // BEGIN CSLBSuP a.jakubiak
        num_of_steps_since_out_of_range = 0;
        motor_abs_step = MAX_MOTOR_STEP;
        // END CSLBSuP a.jakubiak
        

		printk(KERN_INFO "[%s] Motor out of ragne(%d) : %d \n", __func__,i, motor_abs_step);
	} else if (motor_abs_step > MAX_MOTOR_STEP ) {
        motor_abs_step = MAX_MOTOR_STEP;
	}
	
	M08980CTRL(M08980_PI_LDO_Disable);
	
}


// BEGIN CSLBSuP m.wengierow
void set_custom_led_duty(int duty) {
	
	unsigned char d = (unsigned char)duty;
	printk(KERN_INFO "CSLBSuP [%s] duty=%d\n", __func__, duty);

	
    freeze_on();
    ti_dmd_on();

	ChangeDuty(Dmd_seq, d);
	DPPDATAFLASH(Dmd_seq);

    ti_dmd_off();
    freeze_off();

	printk(KERN_INFO "CSLBSuP [%s] done duty=%d\n", __func__, duty);
}

static void projector_duty_set_work(struct work_struct *work)
{
	set_custom_led_duty(duty_to_set);
}

static void projector_current_set_work(struct work_struct *work)
{
	set_custom_led_current(red_to_set, green_to_set, blue_to_set);
}

static void projector_labb_strenght_set_work(struct work_struct *work)
{
	if(labb_to_set >= 2 && labb_to_set <= 150)
	{
		LABB_Control[4] = labb_to_set;

        // freeze_on();
        ti_labb_on();
		DPPDATAFLASH(LABB_Control);
        ti_labb_off();
        // freeze_off();
	}
}

static void projector_labb_control_on_work(struct work_struct *work)
{
	LABB_Control[3] = LABB_ON;

    // freeze_on();
    ti_labb_on();
    DPPDATAFLASH(LABB_Control);
    ti_labb_off();
    // freeze_off();

}

static void projector_labb_control_off_work(struct work_struct *work)
{
	LABB_Control[3] = LABB_OFF;

    // freeze_on();
    ti_labb_on();
    DPPDATAFLASH(LABB_Control);
    ti_labb_off();
    // freeze_off();
}

static void projector_keystone_angle_set_work(struct work_struct *work)
{
	Keystone_SetAngle[4] = (keystone_angle_to_set&0xFF);
	DPPDATAFLASH(Keystone_SetAngle);
}

static void projector_keystone_control_on_work(struct work_struct *work)
{
	DPPDATAFLASH(Keystone_Enable);
}
static void projector_keystone_control_off_work(struct work_struct *work)
{
	DPPDATAFLASH(Keystone_Disable);
}


static void projector_verify_work(struct work_struct *work)
{

	
	if (verify_status_to_set == SWITCH_TO_LCD) {
		switch_to_lcd();
	} else if (verify_status_to_set == SWITCH_TO_PATTERN) {
		switch_to_pattern();
	} else if (verify_status_to_set == CURTAIN_ON) {
		if (status == PRJ_ON_INTERNAL)
		DPPDATAFLASH(Output_Curtain_Enable);
	} else if (verify_status_to_set == CURTAIN_OFF) {
		if (status == PRJ_ON_INTERNAL){
			DPPDATAFLASH(Output_Curtain_Disable);
		}
	} else {
		saved_pattern = verify_status_to_set;
		project_internal(verify_status_to_set);
	}
}

static void projector_set_brightness_work(struct work_struct *work)
{
	if( projector_caic == 1 ){
		set_CAIC_CurrentDuty(siop_flag_to_use);
	}else{
		set_led_current();
        set_led_duty();
	}
}


static void projector_motor_cw_conti_work(struct work_struct *work) {
    int i;
    int j;
    int count;

    printk(KERN_INFO "[%s] CW Conti start", __func__);

    M08980CTRL(M08980_PI_LDO_Enable);

    for (j = 1; j <= MAX_MOTOR_STEP && motor_conti_move == 1; j++) {
        /* H/W Bug Fix : When the direction is changed, motor control command have be sent twice*/
        count = motor_direction == MOTOR_CCW ? 3 : 1;

        for (i = 0; i < count; i++) {
            execute_motor_cw();
            num_of_steps_since_out_of_range++;
        }
        motor_abs_step--;

        if (get_proj_motor_status() == MOTOR_OUTOFRANGE) {
            for (i = 0; i < 30; i++) {
                execute_motor_ccw();

                if (get_proj_motor_status() == MOTOR_INRANGE) {
                    break; // for i
                }
            }
            num_of_steps_since_out_of_range = 0;
            motor_abs_step = 0;

            printk(KERN_INFO "[%s] Motor out of ragne(%d) : %d \n", __func__,i, motor_abs_step);
            break; // for j
        } else if (motor_abs_step < 0) {
            motor_abs_step = 0;
        }
    }
    M08980CTRL(M08980_PI_LDO_Disable);
    printk(KERN_INFO "[%s] CW Conti stop after %d steps", __func__, j);
}





static void projector_motor_ccw_conti_work(struct work_struct *work)
{
    int i;
    int j;
    int count;

    printk(KERN_INFO "[%s] CCW Conti start", __func__);

    M08980CTRL(M08980_PI_LDO_Enable);

    for (j = 1; j <= MAX_MOTOR_STEP && motor_conti_move == 1; j++) {
        /* H/W Bug Fix : When the direction is changed, motor control command have be sent twice*/
        count = motor_direction == MOTOR_CW ? 3 : 1;

        for (i = 0; i < count; i++) {
            execute_motor_ccw();
            num_of_steps_since_out_of_range++;
        }
        motor_abs_step++;

        if (get_proj_motor_status() == MOTOR_OUTOFRANGE) {
            int i;
            for (i = 0; i < 30; i++) {
                execute_motor_cw();

                if (get_proj_motor_status() == MOTOR_INRANGE)
                    break; // for i
            }
            num_of_steps_since_out_of_range = 0;
            motor_abs_step = MAX_MOTOR_STEP;

            printk(KERN_INFO "[%s] Motor out of ragne(%d) : %d \n", __func__,i, motor_abs_step);
            break; // for j
        } else if (motor_abs_step > MAX_MOTOR_STEP) {
            motor_abs_step = MAX_MOTOR_STEP;
        }
    }
    M08980CTRL(M08980_PI_LDO_Disable);
    printk(KERN_INFO "[%s] CCW Conti stop after %d steps", __func__, j);
}

static void projector_info_work(struct work_struct *work)
{
    read_projector_status();
}

// END CSLBSuP m.wengierow

/**
 * Turn on CAIC function
 */
void projector_caic_enable(void)
{
    printk(KERN_INFO "[%s] PRJ caic_enable ", __func__);
	
	DPPDATAFLASH(M08980_SPI_Enable);
	DPPDATAFLASH(CAIC_Enable);
    projector_caic = 1;
	msleep(10);
	
    //set_Gamma(1); //140108_sks
}
/**
 * Tunr off CAIC function
 */
void projector_caic_disable(void) {

    printk(KERN_INFO "[%s] PRJ caic_disable ", __func__);


    DPPDATAFLASH(CAIC_Disable);
    DPPDATAFLASH(M08980_SPI_Disable);
    projector_caic = 0;
    msleep(10);
	
    //set_Gamma(0); //140108_sks
    set_led_current();

    //sks_131223
    set_led_duty();
}

/**
 * Set led duty for brightness
 */
void set_led_duty() {
    if(!dpp343x_fwversion) dpp343x_get_version_info();
    if( ( dpp343x_fwversion & 0xffffff ) > 0x0d0c0a ) { //140108_sks
        int level = 0;
        switch(brightness) {
            case BRIGHT_HIGH_A:
            case BRIGHT_HIGH_B: 
            case BRIGHT_HIGH:
                level = BRIGHT_HIGH;
                break;
            case BRIGHT_MID_A:
            case BRIGHT_MID:
                level = BRIGHT_MID;
                break;
            case BRIGHT_LOW:
                level = BRIGHT_LOW;
                break;
            default:
                printk(KERN_ERR "[%s] wrong brightness %d", __func__, brightness);
                break;
        }

        freeze_on();
        ti_dmd_on();

        ChangeDuty(Dmd_seq, (seq_number+seq_array[level-1])); //140108_sks
        DPPDATAFLASH(Dmd_seq);

        ti_dmd_off();
        freeze_off();

    } else { 
        //Old Firmware
        ti_dmd_on();

        ChangeDuty(Dmd_seq, (seq_number+brightness-1));
        DPPDATAFLASH(Dmd_seq);

        ti_dmd_off();
        msleep(70);
    }
}




/**
 * Change projector CAIC
 */
static void projector_caic_set_on_work(struct work_struct *work)
{
    projector_caic_enable();
}

static void projector_caic_set_off_work(struct work_struct *work)
{
    projector_caic_disable();
}


static ssize_t store_projector_caic(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;
	sscanf(buf, "%d\n", &value);
	projector_flush_workqueue();
	
    switch(value) {
        case 1:
	        projector_queue_work(&projector_work_caic_set_on);
	        printk(KERN_INFO "CSLBSuP: [%s] Projector CAIC set Enable", __func__);
            break;
        case 0:
	        projector_queue_work(&projector_work_caic_set_off);
        	printk(KERN_INFO "CSLBSuP: [%s] Projector CAIC set Disable", __func__);
            break;
    }

	return count;
}

/**
 * Set led current for brightness
 */
void set_led_current(void)
{
    int level = 0;
    switch(brightness) {
        case BRIGHT_HIGH_A:
        case BRIGHT_HIGH_B:
        case BRIGHT_HIGH:
            level = BRIGHT_HIGH;
            break;
        case BRIGHT_MID_A:
        case BRIGHT_MID:
            level = BRIGHT_MID;
            break;
        case BRIGHT_LOW:
        default:
            level = BRIGHT_LOW;
            break;
    }

	printk(KERN_INFO "[%s] brightness:%d level:%d\n", __func__, brightness, level);
		
	RED_MSB_DAC[3]   = flash_rgb_level_data[level - 1][0][0];
	RED_LSB_DAC[3]   = flash_rgb_level_data[level - 1][0][1];
	GREEN_MSB_DAC[3] = flash_rgb_level_data[level - 1][1][0];
	GREEN_LSB_DAC[3] = flash_rgb_level_data[level - 1][1][1];
	BLUE_MSB_DAC[3]  = flash_rgb_level_data[level - 1][2][0];
	BLUE_LSB_DAC[3]  = flash_rgb_level_data[level - 1][2][1];

    freeze_on();

	M08980CTRL(RED_MSB_DAC);
	M08980CTRL(RED_LSB_DAC);
	M08980CTRL(GREEN_MSB_DAC);
	M08980CTRL(GREEN_LSB_DAC);
	M08980CTRL(BLUE_MSB_DAC);
	M08980CTRL(BLUE_LSB_DAC);

    freeze_off();
}

void set_CAIC_CurrentDuty( int siop_flag ) //131202 //sks_131217
{
	
	printk(KERN_INFO "[%s] PRJ brightness:%d\n", __func__, brightness);

    // get firmware version
	if(!dpp343x_fwversion)	dpp343x_get_version_info();

    // requested by ks1211.sung 2013-12-16
    if( ( dpp343x_fwversion & 0xffffff ) > 0x0d0c0a ) { //140108_sks
		//(1) CAIC Current
        switch(brightness) {
            case BRIGHT_HIGH:
            case BRIGHT_HIGH_A:
            case BRIGHT_HIGH_B: 
                {
                    int level = 0;
                    switch(brightness) {
                        case BRIGHT_HIGH_A: //1.1W
                            level = 1; break;
                        case BRIGHT_HIGH_B: //1.0W
                            level = 2; break;
                        case BRIGHT_HIGH:  
                        default: 
                            level = 0; break;
                    }

                    CAIC_Current[3] = caldata_1p2W_to_0p6W[level][0][1]; //RED_LSB_DAC
                    CAIC_Current[4] = caldata_1p2W_to_0p6W[level][0][0]; //RED_MSB_DAC
                    CAIC_Current[5] = caldata_1p2W_to_0p6W[level][1][1]; //GREEN_LSB_DAC
                    CAIC_Current[6] = caldata_1p2W_to_0p6W[level][1][0]; //GREEN_MSB_DAC
                    CAIC_Current[7] = caldata_1p2W_to_0p6W[level][2][1]; //BLUE_LSB_DAC
                    CAIC_Current[8] = caldata_1p2W_to_0p6W[level][2][0]; //BLUE_MSB_DAC

                    //ChangeDuty(Dmd_seq, (seq_number+level)); //(0, 1, 2) 
                    ChangeDuty(Dmd_seq, (seq_number)); //(0)  //sks_140121

                    break;

                }
            case BRIGHT_MID:
            case BRIGHT_MID_A:
				{
                    int level = 0;
                    switch(brightness) {
                        case BRIGHT_MID_A: //0.8W
                            level = 4; break;
                        case BRIGHT_MID: //0.9W
                        default: 
                            level = 3; break;
                    }

                    CAIC_Current[3] = caldata_1p2W_to_0p6W[level][0][1]; //RED_LSB_DAC
                    CAIC_Current[4] = caldata_1p2W_to_0p6W[level][0][0]; //RED_MSB_DAC
                    CAIC_Current[5] = caldata_1p2W_to_0p6W[level][1][1]; //GREEN_LSB_DAC
                    CAIC_Current[6] = caldata_1p2W_to_0p6W[level][1][0]; //GREEN_MSB_DAC
                    CAIC_Current[7] = caldata_1p2W_to_0p6W[level][2][1]; //BLUE_LSB_DAC
                    CAIC_Current[8] = caldata_1p2W_to_0p6W[level][2][0]; //BLUE_MSB_DAC

                    ChangeDuty(Dmd_seq, (seq_number+3)); //(3)

                    break;
					
            	}
            case BRIGHT_LOW:
                {
                    CAIC_Current[3] = flash_rgb_level_data[brightness - 1][0][1]; //RED_LSB_DAC
                    CAIC_Current[4] = flash_rgb_level_data[brightness - 1][0][0]; //RED_MSB_DAC
                    CAIC_Current[5] = flash_rgb_level_data[brightness - 1][1][1]; //GREEN_LSB_DAC
                    CAIC_Current[6] = flash_rgb_level_data[brightness - 1][1][0]; //GREEN_MSB_DAC
                    CAIC_Current[7] = flash_rgb_level_data[brightness - 1][2][1]; //BLUE_LSB_DAC
                    CAIC_Current[8] = flash_rgb_level_data[brightness - 1][2][0]; //BLUE_MSB_DAC

                    ChangeDuty(Dmd_seq, (seq_number+4)); //(4)
                    break;
                }
            default:
	            printk(KERN_ERR "[%s] wrong brightness: %d\n", __func__, brightness);
                break;
        }

    } 
	else { //Old Firmware
        int level = 0;
        switch(brightness) {
            case BRIGHT_HIGH_A:
            case BRIGHT_HIGH_B:
            case BRIGHT_HIGH:
                level = BRIGHT_HIGH;
                break;
            case BRIGHT_MID_A:
            case BRIGHT_MID:
                level = BRIGHT_MID;
                break;
            case BRIGHT_LOW:
            default:
                level = BRIGHT_LOW;
                break;
        }
		CAIC_Current[3] = flash_rgb_level_data[level - 1][0][1]; //RED_LSB_DAC
		CAIC_Current[4] = flash_rgb_level_data[level - 1][0][0]; //RED_MSB_DAC
		CAIC_Current[5] = flash_rgb_level_data[level - 1][1][1]; //GREEN_LSB_DAC
		CAIC_Current[6] = flash_rgb_level_data[level - 1][1][0]; //GREEN_MSB_DAC
		CAIC_Current[7] = flash_rgb_level_data[level - 1][2][1]; //BLUE_LSB_DAC
		CAIC_Current[8] = flash_rgb_level_data[level - 1][2][0]; //BLUE_MSB_DAC

	    ChangeDuty(Dmd_seq, (seq_number+level-1));

    }

    switch(siop_flag) {
        case SIOP_FLAG_FREEZE:
            freeze_on();

            // check_short_status();

            ti_dmd_on();

            DPPDATAFLASH(CAIC_Current);
            DPPDATAFLASH(Dmd_seq);

            ti_dmd_off();

            freeze_off();
            break;

        case SIOP_FLAG_NO_FREEZE_NO_DUTY:
            // check_short_status();
            DPPDATAFLASH(CAIC_Current);
            break;

        case SIOP_FLAG_NO_FREEZE:
        default:
            // check_short_status();
            ti_dmd_on();

            DPPDATAFLASH(CAIC_Current);
            DPPDATAFLASH(Dmd_seq);

            ti_dmd_off();
            msleep(70);
            break;
    }


}

void set_Gamma(int level) //131205
{
	printk(KERN_INFO "[%s] level:%d\n", __func__, level);
		
	Gamma_Select[3] = level; //gamma_number

	DPPDATAFLASH(Gamma_Select);
	
	msleep(70);
}
    


void pwron_seq_gpio(void)
{
	printk(KERN_INFO "[%s] gpio_request \n", __func__);

	
	if(gpio_direction_output(info->pdata->gpio_prj_on, GPIO_LEVEL_HIGH))
	{
		printk(KERN_ERR "[%s] (gpio_request : MP_ON(%d) error \n", __func__,(info->pdata->gpio_prj_on));
	}
	msleep(10);

	if(gpio_direction_output(info->pdata->gpio_mp_on, GPIO_LEVEL_HIGH))
	{
		printk(KERN_ERR "[%s] (gpio_request : PRJ_EN(%d) error \n", __func__,(info->pdata->gpio_mp_on));
	}
	msleep(10);
}




void proj_pwron_seq(void)
{


	pwron_seq_gpio();

	// M08980 soft reset & init	
	M08980CTRL(M08980_SoftReset);
	msleep(10);			
	M08980CTRL(M08980_Init);
	msleep(10);
	
	//M08980 PRJ ON
	M08980CTRL(M08980_PRJON);
	
	msleep(500);

	// M08980 LED ON & Alarm Clear
	M08980CTRL(M08980_LEDON);
	msleep(10);
	M08980CTRL(M08980_AlarmClear);	

	if(not_calibrated) pwron_seq_fdr();
	if(!dpp343x_fwversion)	dpp343x_get_version_info();

	set_led_current();

}

void pwron_seq_direction(void)
{
	switch (get_proj_rotation()) {
	case PRJ_ROTATE_0:
		DPPDATAFLASH(Output_Rotate_0);
		break;
	case PRJ_ROTATE_90:
		DPPDATAFLASH(Output_Rotate_90);
		break;
	case PRJ_ROTATE_180:
		DPPDATAFLASH(Output_Rotate_180);
		break;
	case PRJ_ROTATE_270:
		DPPDATAFLASH(Output_Rotate_270);
		break;
    case PRJ_ROTATE_90_FULL:
		DPPDATAFLASH(Output_Rotate_90_full);
		break;
    case PRJ_ROTATE_270_FULL:
		DPPDATAFLASH(Output_Rotate_270_full);
		break;
	default:
		break;
	};
}

void pwron_seq_source_res(int value)
{
	if (value == LCD_VIEW) {
		DPPDATAFLASH(VSWVGA_RGB888/*WVGA_RGB888*/);
	} else if (value == INTERNAL_PATTERN) {
		DPPDATAFLASH(fWVGA_RGB888);
	}
}

void pwron_seq_fdr(void)
{
	int rgb_log;
	int cnt;
	unsigned int dac;
	//sks_1217
	int red_1p2W; int green_1p2W; int blue_1p2W;
	int red_0p9W; int green_0p9W; int blue_0p9W;
	int red_0p6W; int green_0p6W; int blue_0p6W;
	int delta_red; int delta_green; int delta_blue;
	int red; int green; int blue;
	unsigned char rh, rl, gh, gl, bh, bl;
	
	DPPDATAFLASH(InitData_FlashDataTypeSelect);

	DPPDATAFLASH(InitData_FlashUpdatePrecheck);

	if ( RGB_BUF[0] != 0)
	{
		printk(KERN_ERR "[%s] Flash Data Type Error\n", __func__);
	}		

	DPPDATAFLASH(InitData_FlashDataLength);
	DPPDATAFLASH(InitData_ReadFlashData);
	
	for(rgb_log =0; rgb_log<20;rgb_log++)
	{
		printk(KERN_INFO "CSLBSuP RGB_BUF [%s] %d\n", __func__, RGB_BUF[rgb_log]);		
	}

	memcpy(cal_data,RGB_BUF,20);
	
	for(cnt = 0; cnt < 9; cnt++)
	{
		dac = 0;
		dac |= RGB_BUF[2*cnt] << 8 | RGB_BUF[2*cnt+1];
		
		not_calibrated = (dac < 2 || dac > 999) ? 1 : 0;
		
		flash_rgb_level_data[cnt/3][cnt%3][0] = RGB_BUF[2*cnt];
		flash_rgb_level_data[cnt/3][cnt%3][1] = RGB_BUF[2*cnt+1];
	}
	seq_number = RGB_BUF[18];
	printk(KERN_INFO "[%s] seq_number %x\n", __func__, seq_number);
	
	
	red_1p2W = flash_rgb_level_data[0][0][0] * 256 + flash_rgb_level_data[0][0][1];
	green_1p2W = flash_rgb_level_data[0][1][0] * 256 + flash_rgb_level_data[0][1][1];
	blue_1p2W = flash_rgb_level_data[0][2][0] * 256 + flash_rgb_level_data[0][2][1];
	
	red_0p9W = flash_rgb_level_data[1][0][0] * 256 + flash_rgb_level_data[1][0][1];
	green_0p9W = flash_rgb_level_data[1][1][0] * 256 + flash_rgb_level_data[1][1][1];
	blue_0p9W = flash_rgb_level_data[1][2][0] * 256 + flash_rgb_level_data[1][2][1];

	red_0p6W = flash_rgb_level_data[2][0][0] * 256 + flash_rgb_level_data[2][0][1];
	green_0p6W = flash_rgb_level_data[2][1][0] * 256 + flash_rgb_level_data[2][1][1];
	blue_0p6W = flash_rgb_level_data[2][2][0] * 256 + flash_rgb_level_data[2][2][1];

	//1.2W~0.9W
	delta_red = ( red_1p2W - red_0p9W ) / 3 ;
	delta_green = ( green_1p2W - green_0p9W ) / 3;
	delta_blue = ( blue_1p2W - blue_0p9W ) / 3;
	for(cnt = 0; cnt < 3; cnt++) 
	{
		red = red_1p2W - ( delta_red * cnt );
		green = green_1p2W - ( delta_green * cnt );
		blue = blue_1p2W - ( delta_blue * cnt );
		
		int2bytes( (unsigned int)red, &rl, &rh );
		int2bytes( (unsigned int)green, &gl, &gh );
		int2bytes( (unsigned int)blue, &bl, &bh );	
		
		caldata_1p2W_to_0p6W[cnt][0][0] = rh;
		caldata_1p2W_to_0p6W[cnt][0][1] = rl;
		caldata_1p2W_to_0p6W[cnt][1][0] = gh;
		caldata_1p2W_to_0p6W[cnt][1][1] = gl;
		caldata_1p2W_to_0p6W[cnt][2][0] = bh;
		caldata_1p2W_to_0p6W[cnt][2][1] = bl;
		
	    printk(KERN_INFO "[%s] PRJ 1.2W~0.9W : level=%d red=%d green=%d blue=%d\n", __func__, cnt, red, green, blue);	
	}

	//0.9W~0.6W
	delta_red = ( red_0p9W - red_0p6W ) / 3;
	delta_green = ( green_0p9W - green_0p6W ) / 3;
	delta_blue = ( blue_0p9W - blue_0p6W ) / 3;
	for(cnt = 0; cnt < 4; cnt++)
	{
		red = red_0p9W - ( delta_red * cnt );
		green = green_0p9W - ( delta_green * cnt );
		blue = blue_0p9W - ( delta_blue * cnt );
		
		int2bytes( (unsigned int)red, &rl, &rh );
		int2bytes( (unsigned int)green, &gl, &gh );
		int2bytes( (unsigned int)blue, &bl, &bh );	
		
		caldata_1p2W_to_0p6W[cnt+3][0][0] = rh;
		caldata_1p2W_to_0p6W[cnt+3][0][1] = rl;
		caldata_1p2W_to_0p6W[cnt+3][1][0] = gh;
		caldata_1p2W_to_0p6W[cnt+3][1][1] = gl;
		caldata_1p2W_to_0p6W[cnt+3][2][0] = bh;
		caldata_1p2W_to_0p6W[cnt+3][2][1] = bl;

	    printk(KERN_INFO "[%s] PRJ 0.9W~0.6W level=%d red=%d green=%d blue=%d\n", __func__, cnt, red, green, blue);	
	}
	
}


void dpp343x_fwupdate(void)
{

	int cnt,i;

	dpp343x_fwupdating = 1;

	fw_buffer = kzalloc(1030, GFP_KERNEL);
	
	if (!fw_buffer) {
		printk(KERN_ERR "[%s] fail to memory allocation.\n", __func__);
		return;
	}
	
	

	DPPDATAFLASH(fw_ReadStatus);
	if(RGB_BUF[0]!=0x81)
	{
		printk(KERN_ERR "fw_ReadStatus (%d):",RGB_BUF[0]);
		goto fw_error;
	}
	msleep(10);

	DPPDATAFLASH(fw_WriteDataType);
	msleep(100);
	
	DPPDATAFLASH(fw_ReadUpdatePreCheck);
	msleep(10);
	
	if(RGB_BUF[0])
	{
		printk(KERN_ERR "fw_ReadUpdatePreCheck (%d):",RGB_BUF[0]);
		goto fw_error;
	}
	

	DPPDATAFLASH(fw_WriteDataType);
	msleep(10);

	DPPDATAFLASH(fw_WriteEraseFlash);
	msleep(500);
	msleep(500);
	msleep(500);
	DPPDATAFLASH(fw_ReadStatus);
	if(RGB_BUF[0]&0x10)
	{
		printk(KERN_ERR "fw_ReadStatus (%d):",RGB_BUF[0]);
		goto fw_error;
	}
	msleep(10);
	DPPDATAFLASH(fw_WriteDataType);
	msleep(10);
	DPPDATAFLASH(fw_WriteDataLength);
	msleep(10);



	fw_buffer[0] = fw_WriteStartData[2];
	cal_data[18]=0;
	memcpy((u8 *)&dpp343x_img[DPP_FDROFFSET],(u8 *)cal_data,20);

	memcpy((u8 *)(fw_buffer+1),(u8 *)dpp343x_img,1024);
	DPPDATAFLASH(fw_WriteStartData);
	msleep(10);

	fw_buffer[0] = fw_WriteContinueData[2];

    for(cnt=1;cnt<512;cnt++)
    {
        memcpy((u8 *)(fw_buffer+1),(u8 *)(dpp343x_img+cnt*1024),1024);
        DPPDATAFLASH(fw_WriteContinueData);
        printk(KERN_ERR "%s (%d):", __func__,cnt);
        for(i=0;i<16;i++)
            printk( "0x%02X ", dpp343x_img[cnt*1024+i]);
        printk( "\n");
        msleep(10);
        dpp343x_fwupdating = cnt;
    }
	
	DPPDATAFLASH(fw_WriteSystemReset);
	msleep(1000);

fw_error:
	kzfree(fw_buffer);
	fw_buffer = NULL;
	dpp343x_fwupdating = 0;
	
	
}


int check_dpp_status(void)
{

	DPPDATAFLASH(dpp_ReadDMDID);
	printk(KERN_INFO "[%s] dpp_ReadDMDID : %d\n", __func__,RGB_BUF[0]);

	if(RGB_BUF[0]==0x00)
		return 1;
	else
		return 0;
}

int check_short_status(void)
{

    DPPDATAFLASH(READ_SHORT_STATUS);
    printk(KERN_INFO "[%s] READ_SHORT_STATUS : %d\n", __func__,RGB_BUF[0]);

    if(RGB_BUF[0]==0x00)
        return 1;
    else
        return 0;
}

void read_projector_status(void) //sks_140225
{
    printk(KERN_INFO "[%s] \n", __func__);

    //For DPP3430
    DPPDATAFLASH(dpp_Read_ShortStatus);
    printk(KERN_INFO "[%s] dpp_Read_ShortStatus[0x%02X] : 0x%02X\n", __func__, dpp_Read_ShortStatus[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_dpp_Read_ShortStatus ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_dpp_Read_ShortStatus);
        pre_dpp_Read_ShortStatus = RGB_BUF[0];
    }
    DPPDATAFLASH(dpp_Read_SystemStatus);
    printk(KERN_INFO "[%s] dpp_Read_SystemStatus[0x%02X] : 0x%02X\n", __func__, dpp_Read_SystemStatus[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_dpp_Read_SystemStatus ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue[ is not same to previous data[0x%02X]\n", __func__, pre_dpp_Read_ShortStatus);
        pre_dpp_Read_SystemStatus = RGB_BUF[0];
    }
    DPPDATAFLASH(dpp_Read_CommStatus);
    printk(KERN_INFO "[%s] dpp_Read_CommStatus[0x%02X] : 0x%02X\n", __func__, dpp_Read_CommStatus[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_dpp_Read_CommStatus ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue[ is not same to previous data[0x%02X]\n", __func__, pre_dpp_Read_ShortStatus);
        pre_dpp_Read_CommStatus = RGB_BUF[0];
    }

    //For M08980
    DPPDATAFLASH(M08980Read_LDO_STAT);
    printk(KERN_INFO "[%s] M08980Read_LDO_STAT[0x%02X] : 0x%02X\n", __func__, M08980Read_LDO_STAT[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_LDO_STAT ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_dpp_Read_ShortStatus);
        pre_M08980Read_LDO_STAT = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_BOOST_STAT);
    printk(KERN_INFO "[%s] M08980Read_BOOST_STAT[0x%02X] : 0x%02X\n", __func__, M08980Read_BOOST_STAT[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_BOOST_STAT ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_BOOST_STAT);
        pre_M08980Read_BOOST_STAT = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_DMD_STAT_RB);
    printk(KERN_INFO "[%s] M08980Read_DMD_STAT_RB[0x%02X] : 0x%02X\n", __func__, M08980Read_DMD_STAT_RB[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_DMD_STAT_RB ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_DMD_STAT_RB);
        pre_M08980Read_DMD_STAT_RB = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_MOTOR_BUCK_STAT);
    printk(KERN_INFO "[%s] M08980Read_MOTOR_BUCK_STAT[0x%02X] : 0x%02X\n", __func__, M08980Read_MOTOR_BUCK_STAT[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_MOTOR_BUCK_STAT ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_MOTOR_BUCK_STAT);
        pre_M08980Read_MOTOR_BUCK_STAT = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_BUCKBOOST_STAT);
    printk(KERN_INFO "[%s] M08980Read_BUCKBOOST_STAT[0x%02X] : 0x%02X\n", __func__, M08980Read_BUCKBOOST_STAT[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_BUCKBOOST_STAT ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_BUCKBOOST_STAT);
        pre_M08980Read_BUCKBOOST_STAT = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_RED_VBAT_LSB);
    printk(KERN_INFO "[%s] M08980Read_RED_VBAT_LSB[0x%02X] : 0x%02X\n", __func__, M08980Read_RED_VBAT_LSB[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_RED_VBAT_LSB ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_RED_VBAT_LSB);
        pre_M08980Read_RED_VBAT_LSB = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_GREEN_VBAT_LSB);
    printk(KERN_INFO "[%s] M08980Read_GREEN_VBAT_LSB[0x%02X] : 0x%02X\n", __func__, M08980Read_GREEN_VBAT_LSB[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_GREEN_VBAT_LSB ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_GREEN_VBAT_LSB);
        pre_M08980Read_GREEN_VBAT_LSB = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_BLUE_VBAT_LSB);
    printk(KERN_INFO "[%s] M08980Read_BLUE_VBAT_LSB[0x%02X] : 0x%02X\n", __func__, M08980Read_BLUE_VBAT_LSB[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_BLUE_VBAT_LSB ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_BLUE_VBAT_LSB);
        pre_M08980Read_BLUE_VBAT_LSB = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_RED_VLED_LSB);
    printk(KERN_INFO "[%s] M08980Read_RED_VLED_LSB[0x%02X] : 0x%02X\n", __func__, M08980Read_RED_VLED_LSB[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_RED_VLED_LSB ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_RED_VLED_LSB);
        pre_M08980Read_RED_VLED_LSB = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_GREEN_VLED_LSB);
    printk(KERN_INFO "[%s] M08980Read_GREEN_VLED_LSB[0x%02X] : 0x%02X\n", __func__, M08980Read_GREEN_VLED_LSB[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_GREEN_VLED_LSB ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_GREEN_VLED_LSB);
        pre_M08980Read_GREEN_VLED_LSB = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_BLUE_VLED_LSB);
    printk(KERN_INFO "[%s] M08980Read_BLUE_VLED_LSB[0x%02X] : 0x%02X\n", __func__, M08980Read_BLUE_VLED_LSB[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_BLUE_VLED_LSB ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_BLUE_VLED_LSB);
        pre_M08980Read_BLUE_VLED_LSB = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_VLED_MSB);
    printk(KERN_INFO "[%s] M08980Read_VLED_MSB[0x%02X] : 0x%02X\n", __func__, M08980Read_VLED_MSB[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_VLED_MSB ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_VLED_MSB);
        pre_M08980Read_VLED_MSB = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_TEMP_LSB);
    printk(KERN_INFO "[%s] M08980Read_TEMP_LSB[0x%02X] : 0x%02X\n", __func__, M08980Read_TEMP_LSB[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_TEMP_LSB ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_TEMP_LSB);
        pre_M08980Read_TEMP_LSB = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_TEMP_MSB);
    printk(KERN_INFO "[%s] M08980Read_TEMP_MSB[0x%02X] : 0x%02X\n", __func__, M08980Read_TEMP_MSB[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_TEMP_MSB ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_TEMP_MSB);
        pre_M08980Read_TEMP_MSB = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_ADC_STATUS);
    printk(KERN_INFO "[%s] M08980Read_ADC_STATUS[0x%02X] : 0x%02X\n", __func__, M08980Read_ADC_STATUS[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_ADC_STATUS ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_ADC_STATUS);
        pre_M08980Read_ADC_STATUS = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_PWM_READBACK);
    printk(KERN_INFO "[%s] M08980Read_PWM_READBACK[0x%02X] : 0x%02X\n", __func__, M08980Read_PWM_READBACK[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_PWM_READBACK ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_PWM_READBACK);
        pre_M08980Read_PWM_READBACK = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_IOUT0_HDRM_DAC_READBACK);
    printk(KERN_INFO "[%s] M08980Read_IOUT0_HDRM_DAC_READBACK[0x%02X] : 0x%02X\n", __func__, M08980Read_IOUT0_HDRM_DAC_READBACK[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_IOUT0_HDRM_DAC_READBACK ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_IOUT0_HDRM_DAC_READBACK);
        pre_M08980Read_IOUT0_HDRM_DAC_READBACK = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_IOUT1_HDRM_DAC_READBACK);
    printk(KERN_INFO "[%s] M08980Read_IOUT1_HDRM_DAC_READBACK[0x%02X] : 0x%02X\n", __func__, M08980Read_IOUT1_HDRM_DAC_READBACK[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_IOUT1_HDRM_DAC_READBACK ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_IOUT1_HDRM_DAC_READBACK);
        pre_M08980Read_IOUT1_HDRM_DAC_READBACK = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_IOUT2_HDRM_DAC_READBACK);
    printk(KERN_INFO "[%s] M08980Read_IOUT2_HDRM_DAC_READBACK[0x%02X] : 0x%02X\n", __func__, M08980Read_IOUT2_HDRM_DAC_READBACK[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_IOUT2_HDRM_DAC_READBACK ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_IOUT2_HDRM_DAC_READBACK);
        pre_M08980Read_IOUT2_HDRM_DAC_READBACK = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_DEVICE_ID);
    printk(KERN_INFO "[%s] M08980Read_DEVICE_ID[0x%02X] : 0x%02X\n", __func__, M08980Read_DEVICE_ID[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_DEVICE_ID ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_DEVICE_ID);
        pre_M08980Read_DEVICE_ID = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_ALARM0);
    printk(KERN_INFO "[%s] M08980Read_ALARM0[0x%02X] : 0x%02X\n", __func__, M08980Read_ALARM0[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_ALARM0 ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_ALARM0);
        pre_M08980Read_ALARM0 = RGB_BUF[0];
    }
    DPPDATAFLASH(M08980Read_ALARM1);
    printk(KERN_INFO "[%s] M08980Read_ALARM1[0x%02X] : 0x%02X\n", __func__, M08980Read_ALARM1[3], RGB_BUF[0]);
    if( RGB_BUF[0] != pre_M08980Read_ALARM1 ){
        printk(KERN_INFO "[%s] @@@ SKS_ReadValue is not same to previous data[0x%02X]\n", __func__, pre_M08980Read_ALARM1);
        pre_M08980Read_ALARM1 = RGB_BUF[0];
    }
}

void dpp343x_get_version_info(void)
{
	int cnt;
	
	DPPDATAFLASH(fwversion_FlashDataTypeSelect);

	if ( RGB_BUF[0] != 0)
	{
		printk(KERN_ERR "[%s] Flash Data Type Error\n", __func__);
	}

	DPPDATAFLASH(fwversion_ReadFlashData);


	for(cnt=0;cnt<4;cnt++)
	{
		dpp343x_fwversion = (dpp343x_fwversion<<8) | RGB_BUF[cnt];
		dpp343x_img_version = (dpp343x_img_version<<8) | dpp343x_img[DPP_FWOFFSET+cnt];
	}	
	
	if(dpp343x_fwversion != dpp343x_img_version)
	{
		printk(KERN_ERR "[%s] need to be updated fw from 0x%08X  to 0x%08X \n", __func__, dpp343x_fwversion, dpp343x_img_version);			
	}
	
	printk(KERN_INFO "[%s] fw version: 0x%08X \n", __func__, dpp343x_fwversion);			
	
}


static void proj_pwron_seq_work(struct work_struct *work)
{

	printk(KERN_INFO "[%s] \n", __func__);

	if (get_proj_status() == PRJ_ON_INTERNAL) 
	{
		DPPDATAFLASH(Output_Curtain_Enable);
		pwron_seq_direction();
		pwron_seq_source_res(LCD_VIEW);
		msleep(10);
		DPPDATAFLASH(External_source);

		// CAIC
		projector_caic_enable();
		
		DPPDATAFLASH(Output_Curtain_Disable);
	} 
	else 
	{

		proj_pwron_seq();

		if(check_dpp_status()) 
		{
			proj_pwroff_seq();
			return;
		}

		set_Gamma(1); //140219_sks

		//For error of set_gamma  //140219_sks
        if(check_dpp_status())
        {
            printk(KERN_INFO "[%s] set_gamma : Error 111111111111111\n", __func__);
            proj_pwroff_gpioOnly();

            proj_pwron_seq();
            set_Gamma(1);
            if(check_dpp_status()) 
            {
                printk(KERN_INFO "[%s] set_gamma : Error 22222222222222\n", __func__);
                proj_pwroff_gpioOnly(); 

                proj_pwron_seq();
                set_Gamma(1);
                if(check_dpp_status()) 
                {
                    printk(KERN_INFO "[%s] set_gamma : Error 33333333333333 ", __func__);
                    proj_pwroff_gpioOnly();
                    return;
                }
            }
        }

		pwron_seq_direction();
		pwron_seq_source_res(LCD_VIEW);

		DPPDATAFLASH(External_source);
		msleep(10);
		//CCA
		DPPDATAFLASH(CCA_Disable);
		//CAIC
		projector_caic_enable();
        set_CAIC_CurrentDuty(SIOP_FLAG_NO_FREEZE); //131205

		DPPDATAFLASH(RGB_led_on);

        // msleep(10);
        // read_projector_status();

	}
	set_proj_status(PRJ_ON_RGB_LCD);
	
}

static void proj_testmode_pwron_seq_work(struct work_struct *work)
{

	proj_pwron_seq();

	DPPDATAFLASH(Internal_pattern_direction);
	pwron_seq_source_res(INTERNAL_PATTERN);

    if( projector_caic != 0 ) {
        projector_caic_disable();
    }    

    flash_pattern( saved_pattern );

	DPPDATAFLASH(RGB_led_on);
	saved_pattern = -1;

	set_proj_status(PRJ_ON_INTERNAL);
}


void proj_pwroff_seq(void)
{
	// M08980 LED Off
	M08980CTRL(M08980_LEDOFF);
	msleep(10);
	
	gpio_direction_output(info->pdata->gpio_prj_on, GPIO_LEVEL_LOW);
	msleep(10);

	// M08980 Projector Off
	M08980CTRL(M08980_PRJOFF);
	msleep(400);		// HW
	
	gpio_direction_output(info->pdata->gpio_mp_on, GPIO_LEVEL_LOW);


}
void proj_pwroff_gpioOnly(void)
{
	// M08980 LED Off
	//M08980CTRL(M08980_LEDOFF);
	//msleep(10);

	gpio_direction_output(info->pdata->gpio_prj_on, GPIO_LEVEL_LOW);
	msleep(10);

	// M08980 Projector Off
	//M08980CTRL(M08980_PRJOFF);
	//msleep(400);		// HW

	gpio_direction_output(info->pdata->gpio_mp_on, GPIO_LEVEL_LOW);
	printk(KERN_INFO "[%s] prj_on & mp_on GPIO only off \n", __func__);

}

static void proj_pwroff_seq_work(struct work_struct *work)
{
	set_proj_status(PRJ_OFF);

	proj_pwroff_seq();
}

static void projector_rotate_screen_work(struct work_struct *work)
{
	if (status == PRJ_ON_RGB_LCD) {
		switch (screen_direction) {
		case PRJ_ROTATE_0:
			DPPDATAFLASH(Output_Curtain_Enable);
			DPPDATAFLASH(Output_Rotate_0);
			msleep(50);
			DPPDATAFLASH(Output_Curtain_Disable);
			break;
		case PRJ_ROTATE_90:
			DPPDATAFLASH(Output_Curtain_Enable);
			DPPDATAFLASH(Output_Rotate_90);
			msleep(50);
			DPPDATAFLASH(Output_Curtain_Disable);
			break;
		case PRJ_ROTATE_180:
			DPPDATAFLASH(Output_Curtain_Enable);
			DPPDATAFLASH(Output_Rotate_180);
			msleep(50);
			DPPDATAFLASH(Output_Curtain_Disable);
			break;
		case PRJ_ROTATE_270:
			DPPDATAFLASH(Output_Curtain_Enable);
			DPPDATAFLASH(Output_Rotate_270);
			msleep(50);
			DPPDATAFLASH(Output_Curtain_Disable);
			break;
		case PRJ_ROTATE_90_FULL:
			DPPDATAFLASH(Output_Curtain_Enable);
			DPPDATAFLASH(Output_Rotate_90_full);
			msleep(50);
			DPPDATAFLASH(Output_Curtain_Disable);
			break;
		case PRJ_ROTATE_270_FULL:
			DPPDATAFLASH(Output_Curtain_Enable);
			DPPDATAFLASH(Output_Rotate_270_full);
			msleep(50);
			DPPDATAFLASH(Output_Curtain_Disable);
			break;
		default:
			break;
		}
	}
}

static void projector_read_test_work(struct work_struct *work)
{
	if (status == PRJ_ON_RGB_LCD) {
		printk(KERN_INFO "%s : status is PRJ_ON_RGB_LCD", __func__);
		
	}
	else{
		printk(KERN_INFO "%s : status is not PRJ_ON_RGB_LCD", __func__);
	}
	

}
static void proj_fwupdate_seq_work(struct work_struct *work)
{

	u8 vl_fwversion = 0;

	if(!dpp343x_fwversion) 
	{
		printk(KERN_ERR "[%s] The fw version can't obtain  \n", __func__);			
		vl_fwversion=1;
	}

	if(get_proj_status() == PRJ_OFF) 
	{
		pwron_seq_gpio();

		// M08980 soft reset & init	
		M08980CTRL(M08980_SoftReset);
		msleep(10);			
		M08980CTRL(M08980_Init);
		msleep(10);
		
		//M08980 PRJ ON
		M08980CTRL(M08980_PRJON);
		
		msleep(500);
		msleep(500);
		msleep(500);
		msleep(500);

		// M08980 LED ON & Alarm Clear
		M08980CTRL(M08980_LEDON);
		msleep(10);
		M08980CTRL(M08980_AlarmClear);	
	}

	if(vl_fwversion)
		dpp343x_get_version_info();

	if(dpp343x_fwversion == dpp343x_img_version)
	{
		printk(KERN_ERR "[%s] The lastest fw version: 0x%08X \n", __func__, dpp343x_fwversion);			
		goto fw_error;
	}

	if(!dpp343x_fwversion) 
	{
		printk(KERN_ERR "[%s] The fw version can't obtain  \n", __func__);			
		goto fw_error;
	}

	dpp343x_fwupdate();
	
	dpp343x_get_version_info();

fw_error:
	if(get_proj_status() == PRJ_OFF) 
	{
		proj_pwroff_seq();
	}
}

int get_proj_rotation(void)
{
	return screen_direction;
}

int get_proj_brightness(void)
{
	return brightness;
}

int __devinit dpp3430_i2c_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
	int ret = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		printk(KERN_ERR "[%s] need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		return ret;
	}
	
	info = kzalloc(sizeof(struct projector_dpp3430_info), GFP_KERNEL);
	if (!info) {
		printk(KERN_ERR "[%s] fail to memory allocation.\n", __func__);
		return -1;
	}

	info->client = client;
	info->pdata = client->dev.platform_data;

	i2c_set_clientdata(client, info);

	
	if(gpio_request((info->pdata->gpio_mp_on), "MP_ON"))
	{
		printk(KERN_ERR "[%s] (gpio_request : MP_ON(%d) error\n", __func__,(info->pdata->gpio_mp_on));
	}
	
	if(gpio_request((info->pdata->gpio_prj_on), "PRJ_EN"))
	{
		printk(KERN_ERR "[%s] (gpio_request : PRJ_EN(%d) error \n", __func__,(info->pdata->gpio_prj_on));
	}

	// projector_work_queue = create_singlethread_workqueue("projector_work_queue");
    // projector_work_queue = alloc_workqueue("projector_work_queue", WQ_MEM_RECLAIM | WQ_NON_REENTRANT, 1);
    // projector_work_queue = system_wq;
    projector_work_queue = alloc_workqueue("projector_work_queue", 0, 0); // singe thread on cpu0

	if (!projector_work_queue) {
		printk(KERN_ERR "[%s] i2c_probe fail.\n", __func__);
		return -ENOMEM;
	}


#ifdef CONFIG_HAS_EARLYSUSPEND
	info->earlysuspend.level   = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	info->earlysuspend.suspend = projector_module_early_suspend;
	info->earlysuspend.resume  = projector_module_late_resume;
	register_early_suspend(&info->earlysuspend);
#endif
	INIT_WORK(&projector_work_power_on, proj_pwron_seq_work);
	INIT_WORK(&projector_work_power_off, proj_pwroff_seq_work);
	INIT_WORK(&projector_work_motor_cw, projector_motor_cw_work);
	INIT_WORK(&projector_work_motor_ccw, projector_motor_ccw_work);
	INIT_WORK(&projector_work_motor_cw_conti, projector_motor_cw_conti_work);
	INIT_WORK(&projector_work_motor_ccw_conti, projector_motor_ccw_conti_work);
	INIT_WORK(&projector_work_testmode_on, proj_testmode_pwron_seq_work);
	INIT_WORK(&projector_work_rotate_screen, projector_rotate_screen_work);
	INIT_WORK(&projector_work_read_test, projector_read_test_work);
	INIT_WORK(&projector_work_fw_update, proj_fwupdate_seq_work);
	// START NEW WORK
	INIT_WORK(&projector_work_set_brightness, projector_set_brightness_work);
	INIT_WORK(&projector_work_projector_verify, projector_verify_work);
	INIT_WORK(&projector_work_keystone_control_on, projector_keystone_control_on_work);
	INIT_WORK(&projector_work_keystone_control_off, projector_keystone_control_off_work);
	INIT_WORK(&projector_work_keystone_angle_set, projector_keystone_angle_set_work);
	INIT_WORK(&projector_work_labb_control_on, projector_labb_control_on_work);
	INIT_WORK(&projector_work_labb_control_off, projector_labb_control_off_work);
	INIT_WORK(&projector_work_labb_strenght_set, projector_labb_strenght_set_work);
	INIT_WORK(&projector_work_duty_set, projector_duty_set_work);
	INIT_WORK(&projector_work_caic_set_on, projector_caic_set_on_work);
	INIT_WORK(&projector_work_caic_set_off, projector_caic_set_off_work);
	INIT_WORK(&projector_work_current_set, projector_current_set_work);
	INIT_WORK(&projector_work_info, projector_info_work);

	//END NEW WORK
	
	printk(KERN_INFO "[%s] dpp3430_i2c_probe(MP%d,  PRJ.: %d\n", __func__,info->pdata->gpio_mp_on,info->pdata->gpio_prj_on);

	return 0;
}

__devexit int dpp3430_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id dpp3430_i2c_id[] = {
	{ "dpp3430", 0 },
	{ }
};
MODULE_DEVICE_TABLE(dpp3430_i2c, dpp3430_i2c_id);

static struct i2c_driver dpp3430_i2c_driver = {
	.driver = {
		.name = "dpp3430",
		.owner = THIS_MODULE,
	},
	.probe = dpp3430_i2c_probe,
	.remove = __devexit_p(dpp3430_i2c_remove),
	.id_table = dpp3430_i2c_id,
};


int __devinit M08980_i2c_probe(struct i2c_client *client,
                const struct i2c_device_id *id)
{
	int ret = 0;
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
	{
		printk(KERN_ERR "[%s] need I2C_FUNC_I2C\n", __func__);
		ret = -ENODEV;
		return ret;
	}
	M08980_info = kzalloc(sizeof(struct projector_M08980_info), GFP_KERNEL);
	if (!M08980_info) {
		printk(KERN_ERR "[%s] fail to memory allocation.\n", __func__);
		return -1;
	}

	M08980_info->client = client;
	//M08980_info->pdata = client->dev.platform_data;
	
	i2c_set_clientdata(client, M08980_info);


	printk(KERN_INFO "[%s] \n", __func__);

	return 0;
	
}

static const struct i2c_device_id M08980_i2c_id[] = {
	{ "M08980", 0 },
	{ }
};


__devexit int M08980_i2c_remove(struct i2c_client *client)
{
	return 0;
}

MODULE_DEVICE_TABLE(M08980_i2c, M08980_i2c_id);

static struct i2c_driver M08980_i2c_driver = {
	.driver = {
		.name = "M08980",
		.owner = THIS_MODULE,
	},
	.probe = M08980_i2c_probe,
	.remove = __devexit_p(M08980_i2c_remove),
	.id_table = M08980_i2c_id,
};


int projector_module_open(struct inode *inode, struct file *file)
{
		return 0;
}

int projector_module_release(struct inode *inode, struct file *file)
{
		return 0;
}


#ifdef CONFIG_HAS_EARLYSUSPEND
void projector_module_early_suspend(struct early_suspend *h)
{
	pr_info("%s\n", __func__);
	if (status == PRJ_OFF) {
		gpio_direction_output(info->pdata->gpio_prj_on, GPIO_LEVEL_LOW);
	}
}

void projector_module_late_resume(struct early_suspend *h)
{
}

#endif


static struct file_operations projector_module_fops = {
	.owner = THIS_MODULE,
	.open = projector_module_open,
	.release = projector_module_release,
};

static struct miscdevice projector_module_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "secProjector",
	.fops = &projector_module_fops,
};

void switch_to_lcd(){
	if (get_proj_status() == PRJ_ON_RGB_LCD) 
	{
	}else{
        freeze_on();

		pwron_seq_source_res(LCD_VIEW);
		msleep(10);
		DPPDATAFLASH(External_source);

        freeze_off();
	}
	set_proj_status(PRJ_ON_RGB_LCD);
	
}

void switch_to_pattern(){
	if (get_proj_status() == PRJ_ON_INTERNAL) 
	{
	}else{
        freeze_on();

		pwron_seq_source_res(INTERNAL_PATTERN);
		msleep(10);
		DPPDATAFLASH(I_white);

        freeze_off();
		
	}
	set_proj_status(PRJ_ON_INTERNAL);
}

void project_internal(int pattern)
{
	//projector_flush_workqueue();

	if (get_proj_status() == PRJ_OFF) 
	{
	    projector_queue_work(&projector_work_testmode_on);
	} 
	else 
	{
        freeze_on();

		if (get_proj_status() == PRJ_ON_RGB_LCD) 
		{
			
			// CAIC
			projector_caic_disable();
			

			DPPDATAFLASH(Internal_pattern_direction);
			pwron_seq_source_res(INTERNAL_PATTERN);
			set_proj_status(PRJ_ON_INTERNAL);

		}

		if (get_proj_status() == RGB_LED_OFF) 
		{
			DPPDATAFLASH(RGB_led_on);
			set_proj_status(PRJ_ON_INTERNAL);
		}

        flash_pattern(pattern);

        freeze_off();
	}

	printk(KERN_INFO "[%s] : pattern = %d\n", __func__,pattern);
}

void flash_pattern(int pattern) {
    switch (pattern) {
        case CHECKER:
            DPPDATAFLASH(I_4x4checker);
            verify_value = 20;
            break;
        case CHECKER_16x9:
            DPPDATAFLASH(I_16x9checker);
            verify_value = 40;
            break;
        case WHITE:
            DPPDATAFLASH(I_white);
            verify_value = 21;
            break;
        case BLACK:
            DPPDATAFLASH(I_black);
            verify_value = 22;
            break;
        case LEDOFF:
            DPPDATAFLASH(RGB_led_off);
            set_proj_status(RGB_LED_OFF);
            verify_value = 23;
            break;
        case RED:
            DPPDATAFLASH(I_red);
            verify_value = 24;
            break;
        case GREEN:
            DPPDATAFLASH(I_green);
            verify_value = 25;
            break;
        case BLUE:
            DPPDATAFLASH(I_blue);
            verify_value = 26;
            break;
        case BEAUTY:
            verify_value = 27;
            break;
        case STRIPE:
            DPPDATAFLASH(I_stripe);
            verify_value = 28;
            break;
        case DIAGONAL:
            DPPDATAFLASH(I_diagonal);
            verify_value = 29;
            break;			
        default:
            break;
    }
}


void move_motor_step(int value)
{
	int i, count;
	
	count =60;
	
	if(value ==98)
	{
		M08980CTRL(M08980_PI_LDO_Enable);		
		
		for(i=0;i<count;i++)
		{	
            execute_motor_cw();

			if(get_proj_motor_status() == MOTOR_OUTOFRANGE)
			{
				verify_value = 300+value;
				break;
			}
		}

		if(get_proj_motor_status() == MOTOR_OUTOFRANGE)
		{
			for(i=0;i<30;i++)
			{
                execute_motor_ccw();

				if(get_proj_motor_status() == MOTOR_INRANGE) break;
			}
		}
		
		M08980CTRL(M08980_PI_LDO_Disable);

	}
 	else if(value ==99)
	{
		M08980CTRL(M08980_PI_LDO_Enable);		
		
		for(i=0;i<count;i++)
		{	
            execute_motor_ccw();

			if(get_proj_motor_status() == MOTOR_OUTOFRANGE)
			{
				verify_value = 300+value;
				break;
			}
		}


		if(get_proj_motor_status() == MOTOR_OUTOFRANGE)
		{
			for(i=0;i<30;i++)
			{
                execute_motor_cw();

				if(get_proj_motor_status() == MOTOR_INRANGE) break;
			}
		}
		
		M08980CTRL(M08980_PI_LDO_Disable);

	}
	else
	{
		M08980CTRL(M08980_PI_LDO_Enable);		
		
		for(i=0;i<value;i++)
		{	
			execute_motor_cw();

			if(get_proj_motor_status() == MOTOR_OUTOFRANGE)
			{
				break;
			}
		}

		if(get_proj_motor_status() == MOTOR_OUTOFRANGE)
		{
			for(i=0;i<30;i++)
			{
				execute_motor_ccw();

				if(get_proj_motor_status() == MOTOR_INRANGE) break;
			}
		}
		M08980CTRL(M08980_PI_LDO_Disable);
		verify_value = 300+value;
	}

}

static ssize_t store_motor_action(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int direction;

	sscanf(buf, "%d\n", &direction);
      
    motor_conti_move = 0; // stop for conti
    projector_flush_workqueue(); // sync

	if (status != PRJ_OFF) {
		
		switch (direction) {
		case MOTOR_CW:
	        projector_queue_work(&projector_work_motor_cw);
			break;
		case MOTOR_CCW:
	        projector_queue_work(&projector_work_motor_ccw);
			break;
        case MOTOR_CONTINUE_CW:
            motor_conti_move = 1;
            projector_queue_work(&projector_work_motor_cw_conti);
            break;
        case MOTOR_CONTINUE_CCW:
            motor_conti_move = 1;
            projector_queue_work(&projector_work_motor_ccw_conti);
            break;
        case MOTOR_BREAK:
            // do nothing - we have already sent break and sync commands
            break;
		default:
			break;
		}
	}

	return count;
}


static ssize_t show_fwupdate(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result, vl_fw;

	if(dpp343x_fwupdating)
	{
		vl_fw = (dpp343x_fwupdating*100)/512;
	}
	else if(dpp343x_fwversion != dpp343x_img_version)
	{
		vl_fw = 1;
		printk(KERN_ERR "[%s] need to be updated fw from 0x%08X  to 0x%08X \n", __func__, dpp343x_fwversion, dpp343x_img_version);
	}
	else	vl_fw =100;

	result = sprintf(buf, "%d %d\n", vl_fw, dpp343x_fwversion);

	return result;
}


static ssize_t store_fwupdate(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;

	sscanf(buf, "%d\n", &value);

	if(value==1)
	{
		projector_flush_workqueue();
		projector_queue_work(&projector_work_fw_update);
	}
		
	return count;
}



static ssize_t show_brightness(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int result;

	result = sprintf(buf, "%d\n", brightness);

	return result;
}


static ssize_t show_proj_key(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;

	size = sprintf(buf, "%d\n", get_proj_status());

	return size;
}


static ssize_t store_proj_key(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;

	sscanf(buf, "%d\n", &value);

	projector_flush_workqueue();
	switch (value) {
	case 0:
	    projector_queue_work(&projector_work_power_off);
		verify_value = 0;
		break;
	case 1:
		if (get_proj_status() != PRJ_ON_RGB_LCD) {
	        projector_queue_work(&projector_work_power_on);
			verify_value = 10;
		}
		break;
	default:
		break;
	};

	printk(KERN_INFO "[%s] -->  %d\n", __func__, value);
	return count;
}


static ssize_t store_keystone_control(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;

	sscanf(buf, "%d\n", &value);
	projector_flush_workqueue();
	switch (value) {
	case 0:
	    projector_queue_work(&projector_work_keystone_control_off);
		break;
	case 1:
	    projector_queue_work(&projector_work_keystone_control_on);
		break;
	default:
		break;
	}

	printk(KERN_INFO "[%s] -->  %d\n", __func__, value);
	return count;
}


static ssize_t store_keystone_set_angle(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	sscanf(buf, "%d\n", &keystone_angle_to_set);
	

	if(keystone_angle_to_set >= -40 && keystone_angle_to_set <= 40)
	{
		projector_flush_workqueue();
	    projector_queue_work(&projector_work_keystone_angle_set);
	}
	
	printk(KERN_INFO "[%s] -->  0x%x\n", __func__, (keystone_angle_to_set&0xFF));
	return count;
}


static ssize_t store_labb_control(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;

	sscanf(buf, "%d\n", &value);
	projector_flush_workqueue();
	switch (value) {
	case 0:
	    projector_queue_work(&projector_work_labb_control_off);
		break;
	case 1:
	    projector_queue_work(&projector_work_labb_control_on);
		break;
	default:
		break;
	}

	printk(KERN_INFO "[%s] -->  %d\n", __func__, value);
	return count;
}




static ssize_t store_labb_set_strength(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{

	sscanf(buf, "%d\n", &labb_to_set);
	projector_flush_workqueue();
	projector_queue_work(&projector_work_labb_strenght_set);
	printk(KERN_INFO "[%s] -->  %d\n", __func__, labb_to_set);
	return count;
}
	
/* BEAM_HW : old_fw */
static ssize_t show_cal_history(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size, result = 10;
	int calibrated, latest_fw=20;
	int i=0;

#if 0
	if((not_calibrated==0)&&(dpp343x_fwversion!=0))
		result =0;
#else
	if(not_calibrated==0xFF)
		calibrated =10;
	else if(not_calibrated==0)
		calibrated =0;
	else
		calibrated =1;

	if(dpp343x_fwversion!=0)
	{	

		for(i=0;i<DPP_OLDFW;i++)
		{
			if(dpp343x_fwversion==dpp343x_img_oldversion[i])
				break;
		}

		if(i==DPP_OLDFW)
		{

			if(dpp343x_fwversion == dpp343x_img_version)
				latest_fw = 0;
			else
				latest_fw = 40;
		}
		else
			latest_fw = 2;
	}
	else
		latest_fw=20;

	result = calibrated + latest_fw;
#endif

	size = sprintf(buf, "%d\n", result);

	return size;
}

static ssize_t show_screen_direction(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;

	size = sprintf(buf, "%d\n", screen_direction);

	return size;
}

static ssize_t show_retval(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;

	size = sprintf(buf, "%d\n", verify_value);

	return size;
}

static ssize_t store_screen_direction(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int  value;

	sscanf(buf, "%d\n", &value);

	if (value >= PRJ_ROTATE_0 && value <= PRJ_ROTATE_270_FULL) {
		screen_direction = value;
	}

	return count;
}

static ssize_t store_rotate_screen(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;

	sscanf(buf, "%d\n", &value);

	if (value >= PRJ_ROTATE_0 && value <= PRJ_ROTATE_270_FULL) {
		projector_flush_workqueue();
		screen_direction = value;
		projector_queue_work(&projector_work_rotate_screen);

		printk(KERN_INFO "[%s] inputed rotate : %d\n", __func__, value);
	}

	return count;
}




static ssize_t store_projection_verify(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d\n", &verify_status_to_set);
	printk(KERN_INFO "[%s] selected internal pattern : %d\n",
			__func__, verify_status_to_set);
	projector_flush_workqueue();
	projector_queue_work(&projector_work_projector_verify);
	return count;
}

static ssize_t store_motor_verify(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value;

	sscanf(buf, "%d\n", &value);
	printk(KERN_INFO "[%s] %d\n", __func__, value);

	if (status != PRJ_OFF) 
	{
		/* This function is not needed anymore except DFMS */
		if(((value>=0)&&(value<=40))|| (value == 98 || value == 99) )
		{
			move_motor_step(value);
		}
	}
	return count;
}

// BEGIN CSLBSuP PPNFAD a.jakubiak/m.wengierow
void int2bytes( unsigned int input, unsigned char *low, unsigned char *high ) {
	// this code is taken from a presentation "PAD100 Control"
	*high = input / 256;
	*low  = input - ( 256 * (*high) );
}

void set_custom_led_current(int red, int green, int blue) {
	unsigned char rh, rl, gh, gl, bh, bl;
	
	printk(KERN_INFO "CSLBSuP [%s] red=%d green=%d blue=%d\n", __func__, red, green, blue);
	
	int2bytes( (unsigned int)red, &rl, &rh );
	int2bytes( (unsigned int)green, &gl, &gh );
	int2bytes( (unsigned int)blue, &bl, &bh );	
	
	RED_MSB_DAC[3] = rh;
	RED_LSB_DAC[3] = rl;
	GREEN_MSB_DAC[3] = gh;
	GREEN_LSB_DAC[3] = gl;
	BLUE_MSB_DAC[3] = bh;
	BLUE_LSB_DAC[3] = bl;

	M08980CTRL(RED_MSB_DAC);
	M08980CTRL(RED_LSB_DAC);
	M08980CTRL(GREEN_MSB_DAC);
	M08980CTRL(GREEN_LSB_DAC);
	M08980CTRL(BLUE_MSB_DAC);
	M08980CTRL(BLUE_LSB_DAC);
	

	printk(KERN_INFO "CSLBSuP [%s] done red=%d green=%d blue=%d\n", __func__, red, green, blue);
}


static ssize_t store_brightness(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
    int value;

    int new_brightness;

	sscanf(buf, "%d\n", &value);
    
    // sync projector work queue
	projector_flush_workqueue();

    // parse 
    switch (value) {
        case 1:  new_brightness = BRIGHT_HIGH;   break;
        case 2:  new_brightness = BRIGHT_MID;    break;
        case 3:  new_brightness = BRIGHT_LOW;    break;
        case 4:  new_brightness = BRIGHT_HIGH_A; break;
        case 5:  new_brightness = BRIGHT_HIGH_B; break;
        case 6:  new_brightness = BRIGHT_MID_A;  break;
        default: new_brightness = BRIGHT_HIGH;   break;
    }

    // set default method of changing the brightness
    siop_flag_to_use = SIOP_FLAG_FREEZE;

    // compare current brightness and new_brightness
    if(new_brightness == brightness) {
        // this is called, for example when we are deactivating the color compensation
        siop_flag_to_use = SIOP_FLAG_FREEZE; // I added this line explicit
    } else switch(brightness) {
        case BRIGHT_HIGH:
        case BRIGHT_HIGH_A:
        case BRIGHT_HIGH_B:
            switch(new_brightness) {
                case BRIGHT_HIGH_A:
                case BRIGHT_HIGH_B:
                case BRIGHT_HIGH:
                    // no need to change duty and freeze
                    siop_flag_to_use = SIOP_FLAG_NO_FREEZE_NO_DUTY;
                    break;
            }
            break;
        case BRIGHT_MID:
        case BRIGHT_MID_A:
            switch(new_brightness) {
                case BRIGHT_MID:
                case BRIGHT_MID_A:
                    // no need to change duty and freeze
                    siop_flag_to_use = SIOP_FLAG_NO_FREEZE_NO_DUTY;
                    break;
            }
            break;
    }

    brightness = new_brightness;


	if (get_proj_status() == PRJ_OFF) {
        // projector is off - do nothing
	} else {
		
		projector_flush_workqueue();
	    projector_queue_work(&projector_work_set_brightness);

		printk(KERN_INFO "[%s] Proj Brightness Changed : %d\n", __func__, value);
	}
	return count;
}

static ssize_t store_led_current(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d %d %d\n", &red_to_set, &green_to_set, &blue_to_set);

	if (get_proj_status() == PRJ_OFF) {
		printk(KERN_INFO "CSLBSuP: [%s] Projector led current NOT changed : %d %d %d\n", __func__, red_to_set, green_to_set, blue_to_set);
	} else {
	    projector_queue_work(&projector_work_current_set);
		printk(KERN_INFO "CSLBSuP: [%s] Projector led current changed : %d %d %d\n", __func__, red_to_set, green_to_set, blue_to_set);
	}
	return count;
}


void get_custom_led_current(int *red, int *green, int *blue) {
	
	
	unsigned char rh, rl, gh, gl, bh, bl;
	printk(KERN_INFO "CSLBSuP: [%s] ", __func__);
	
	rh = RED_MSB_DAC[3];
	rl = RED_LSB_DAC[3];
	gh = GREEN_MSB_DAC[3];
	gl = GREEN_LSB_DAC[3];
	bh = BLUE_MSB_DAC[3];
	bl = BLUE_LSB_DAC[3];
	

	*red   = rh * 256 + rl;
	*green = gh * 256 + gl;
	*blue  = bh * 256 + bl;

	printk(KERN_INFO "CSLBSuP [%s] done red=%d green=%d blue=%d\n", __func__, *red, *green, *blue);
}

static ssize_t show_led_current(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;
	int red, green, blue;

    projector_flush_workqueue();

	get_custom_led_current(&red, &green, &blue);

	size = sprintf(buf, "%d %d %d\n", red, green, blue);

	return size;
}


static ssize_t store_led_duty(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	sscanf(buf, "%d\n", &duty_to_set);
	projector_flush_workqueue();
	if (get_proj_status() == PRJ_OFF) {
		printk(KERN_INFO "CSLBSuP: [%s] Projector led duty NOT changed : %d\n", __func__, duty_to_set);
	} else {
	    projector_queue_work(&projector_work_duty_set);
		printk(KERN_INFO "CSLBSuP: [%s] Proj led duty changed : %d\n", __func__, duty_to_set);
	}
	return count;
}


void get_led_duty(int *duty) {

	unsigned char d;
	printk(KERN_INFO "CSLBSuP: [%s] ", __func__);

	d = RGB_BUF[18];

	*duty  = d;

	printk(KERN_INFO "CSLBSuP [%s] done duty=%c\n", __func__, (int) *duty);
}


static ssize_t show_led_duty(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int size;
	int duty;

	get_led_duty(&duty);

	size = sprintf(buf, "%d\n", duty);

	return size;
}
// END CSLBSuP PPNFAD a.jakubiak/m.wengierow

// BEGIN CSLBSuP PPNFAD a.jakubiak
static ssize_t show_motor_verify(struct device *dev, struct device_attribute *attr, char *buf) {
	int size;

    // ensure we are not reporting out of range values
    if(motor_abs_step<0) {
        motor_abs_step = 0;
    } else if(motor_abs_step>MAX_MOTOR_STEP) {
        motor_abs_step = MAX_MOTOR_STEP;
    }
    // but because motor is assync we can still get the out-of-range values

	size = sprintf(buf, "%d %d\n", motor_abs_step, num_of_steps_since_out_of_range );

	return size;
}

static ssize_t show_projector_caic(struct device *dev, struct device_attribute *attr, char *buf) {
	int size;

	size = sprintf(buf, "%d\n", projector_caic );

	return size;
}

// END CSLBSuP 

// BEGIN CSLBSuP

/**
 * Do one motor step CW
 */
void execute_motor_cw(void) {
    M08980CTRL(Motor_CW_out);
    motor_direction = MOTOR_CW;
    msleep(10); // temporary fix
}

/**
 * Do one motor step CCW
 */
void execute_motor_ccw(void) {
    M08980CTRL(Motor_CCW_out);
    motor_direction = MOTOR_CCW;
    msleep(10); // temporary fix
}

int cpu;

/**
 * Add job to projector work queue
 */
int projector_queue_work(struct work_struct *work) {
    return queue_work_on( cpu, projector_work_queue, work);
}

/**
 * Flash projector work queue, sync and wait all jobs are done
 */
void projector_flush_workqueue() {
    flush_workqueue(projector_work_queue);
}

/**
 * begin freeze projection and sleep
 */
void freeze_on() {
    msleep(10);
    // check_short_status();
	DPPDATAFLASH(Freeze_On);
	msleep(10);

}

/**
 * end freeze projection and sleep
 */
void freeze_off() {
	msleep(70);
    // check_short_status();
	DPPDATAFLASH(Freeze_Off);
    msleep(10);
}

/**
 * A special method to be executed before changing Dmd_seq
 */
void ti_dmd_on() {
    DPPDATAFLASH(TI_On_DMD);
    msleep(20);
}
/**
 * A special method to be executed after changing Dmd_seq
 */
void ti_dmd_off() {
	msleep(70);
    DPPDATAFLASH(TI_Off);
    msleep(20);
}
/**
 * A special method do be executed before changing LABB
 */
void ti_labb_on() {
    DPPDATAFLASH(TI_On_LABB);
    msleep(20);
}
/**
 * A special method to be executed after changing LABB
 */
void ti_labb_off() {
    DPPDATAFLASH(TI_Off);
    msleep(20);
}





/**
 * show projector info
 */
static ssize_t show_info(struct device *dev, struct device_attribute *attr, char *buf) {
    int size;

    // queue job and wait for results
    projector_queue_work(&projector_work_info);
    projector_flush_workqueue();
   
    // print output 
    size  = 0;
    size += sprintf(buf+size, "dpp_Read ShortStatus:               0x%02X\n", pre_dpp_Read_ShortStatus);
    size += sprintf(buf+size, "dpp_Read_SystemStatus:              0x%02X\n", pre_dpp_Read_SystemStatus);
    size += sprintf(buf+size, "dpp_Read_CommStatus:                0x%02X\n", pre_dpp_Read_CommStatus);
    size += sprintf(buf+size, "M08980Read_LDO_STAT:                0x%02X\n", pre_M08980Read_LDO_STAT);
    size += sprintf(buf+size, "M08980Read_BOOST_STAT:              0x%02X\n", pre_M08980Read_BOOST_STAT);
    size += sprintf(buf+size, "M08980Read_DMD_STAT_RB:             0x%02X\n", pre_M08980Read_DMD_STAT_RB);
    size += sprintf(buf+size, "M08980Read_MOTOR_BUCK_STAT:         0x%02X\n", pre_M08980Read_MOTOR_BUCK_STAT);
    size += sprintf(buf+size, "M08980Read_BUCKBOOST_STAT:          0x%02X\n", pre_M08980Read_BUCKBOOST_STAT);
    size += sprintf(buf+size, "M08980Read_RED_VBAT_LSB:            0x%02X\n", pre_M08980Read_RED_VBAT_LSB);
    size += sprintf(buf+size, "M08980Read_GREEN_VBAT_LSB:          0x%02X\n", pre_M08980Read_GREEN_VBAT_LSB);
    size += sprintf(buf+size, "M08980Read_BLUE_VBAT_LSB:           0x%02X\n", pre_M08980Read_BLUE_VBAT_LSB);
    size += sprintf(buf+size, "M08980Read_RED_VLED_LSB:            0x%02X\n", pre_M08980Read_RED_VLED_LSB);
    size += sprintf(buf+size, "M08980Read_GREEN_VLED_LSB:          0x%02X\n", pre_M08980Read_GREEN_VLED_LSB);
    size += sprintf(buf+size, "M08980Read_BLUE_VLED_LSB:           0x%02X\n", pre_M08980Read_BLUE_VLED_LSB);
    size += sprintf(buf+size, "M08980Read_VLED_MSB:                0x%02X\n", pre_M08980Read_VLED_MSB);
    size += sprintf(buf+size, "M08980Read_TEMP_LSB:                0x%02X\n", pre_M08980Read_TEMP_LSB);
    size += sprintf(buf+size, "M08980Read_TEMP_MSB:                0x%02X\n", pre_M08980Read_TEMP_MSB);
    size += sprintf(buf+size, "M08980Read_ADC_STATUS:              0x%02X\n", pre_M08980Read_ADC_STATUS);
    size += sprintf(buf+size, "M08980Read_PWM_READBACK:            0x%02X\n", pre_M08980Read_PWM_READBACK);
    size += sprintf(buf+size, "M08980Read_IOUT0_HDRM_DAC_READBACK: 0x%02X\n", pre_M08980Read_IOUT0_HDRM_DAC_READBACK);
    size += sprintf(buf+size, "M08980Read_IOUT1_HDRM_DAC_READBACK: 0x%02X\n", pre_M08980Read_IOUT1_HDRM_DAC_READBACK);
    size += sprintf(buf+size, "M08980Read_IOUT2_HDRM_DAC_READBACK: 0x%02X\n", pre_M08980Read_IOUT2_HDRM_DAC_READBACK);
    size += sprintf(buf+size, "M08980Read_DEVICE_ID:               0x%02X\n", pre_M08980Read_DEVICE_ID);
    size += sprintf(buf+size, "M08980Read_ALARM0:                  0x%02X\n", pre_M08980Read_ALARM0);
    size += sprintf(buf+size, "M08980Read_ALARM1:                  0x%02X\n", pre_M08980Read_ALARM1);

    return size;
}


// END CSLBSuP



//static ssize_t store_read_test(struct device *dev,
//		struct device_attribute *attr, const char *buf, size_t count)
//{
//	//int value;
//
//	projector_queue_work(&projector_work_read_test);
//
//	return count;
//}

static DEVICE_ATTR(proj_motor, S_IRUGO | S_IWUSR, NULL, store_motor_action);
static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR, show_brightness, store_brightness);
static DEVICE_ATTR(proj_key, S_IRUGO | S_IWUSR, show_proj_key, store_proj_key);
static DEVICE_ATTR(cal_history, S_IRUGO, show_cal_history, NULL);
static DEVICE_ATTR(proj_fw, S_IRUGO | S_IWUSR, show_fwupdate, store_fwupdate);
static DEVICE_ATTR(rotate_screen, S_IRUGO | S_IWUSR,NULL, store_rotate_screen);
static DEVICE_ATTR(screen_direction, S_IRUGO | S_IWUSR,	show_screen_direction, store_screen_direction);
static DEVICE_ATTR(projection_verify, S_IRUGO | S_IWUSR,	show_info, store_projection_verify);
//static DEVICE_ATTR(read_test, S_IRUGO | S_IWUGO,NULL, store_read_test);
static DEVICE_ATTR(retval, S_IRUGO, show_retval, NULL);
static DEVICE_ATTR(proj_keystone, S_IRUGO | S_IWUSR, NULL, store_keystone_control);
static DEVICE_ATTR(keystone_angle, S_IRUGO | S_IWUSR, NULL, store_keystone_set_angle);
static DEVICE_ATTR(proj_labb, S_IRUGO | S_IWUSR, NULL, store_labb_control);
static DEVICE_ATTR(labb_strength, S_IRUGO | S_IWUSR, NULL, store_labb_set_strength);
// BEGIN CSLBSuP PPNFAD a.jakubiak/m.wengierow
static DEVICE_ATTR(motor_verify, S_IRUGO | S_IWUSR, show_motor_verify  , store_motor_verify);
static DEVICE_ATTR(led_current,  S_IRUGO | S_IWUSR, show_led_current   , store_led_current);	
static DEVICE_ATTR(led_duty   ,  S_IRUGO | S_IWUSR, show_led_duty      , store_led_duty);
static DEVICE_ATTR(caic,         S_IRUGO | S_IWUSR, show_projector_caic, store_projector_caic); 
// static DEVICE_ATTR(info,         S_IRUGO          , show_info,           NULL);
// END CSLBSuP PPNFAD a.jakubiak/m.wengierow
//
//

int __init projector_module_init(void)
{
	int ret;

    printk(KERN_INFO "CSLBSuP: projector_module_init \n");

    cpu = get_cpu(); put_cpu();

	sec_projector = device_create(sec_class, NULL, 0, NULL, "sec_projector");
	if (IS_ERR(sec_projector)) {
		printk(KERN_ERR "Failed to create device(sec_projector)!\n");
	}

	if (device_create_file(sec_projector, &dev_attr_proj_motor) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_proj_motor.attr.name);

	if (device_create_file(sec_projector, &dev_attr_brightness) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_brightness.attr.name);

	if (device_create_file(sec_projector, &dev_attr_proj_key) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_proj_key.attr.name);

	if (device_create_file(sec_projector, &dev_attr_proj_keystone) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_proj_keystone.attr.name);
				
	if (device_create_file(sec_projector, &dev_attr_proj_labb) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_proj_labb.attr.name);
				
	if (device_create_file(sec_projector, &dev_attr_keystone_angle) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_keystone_angle.attr.name);
				
	if (device_create_file(sec_projector, &dev_attr_labb_strength) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_labb_strength.attr.name);
	
	if (device_create_file(sec_projector, &dev_attr_cal_history) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_cal_history.attr.name);

	if (device_create_file(sec_projector, &dev_attr_rotate_screen) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_rotate_screen.attr.name);

	if (device_create_file(sec_projector, &dev_attr_screen_direction) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_screen_direction.attr.name);

	if (device_create_file(sec_projector, &dev_attr_projection_verify) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_projection_verify.attr.name);

	if (device_create_file(sec_projector, &dev_attr_motor_verify) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_motor_verify.attr.name);

	if (device_create_file(sec_projector, &dev_attr_retval) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_retval.attr.name);

// BEGIN CSLBSuP PPNFAD a.jakubiak/m.wengierow
	if (device_create_file(sec_projector, &dev_attr_led_current) < 0)
		printk(KERN_ERR "CSLBSuP: Failed to create device file(%s)!\n",
				dev_attr_led_current.attr.name);

	if (device_create_file(sec_projector, &dev_attr_led_duty) < 0)
		printk(KERN_ERR "CSLBSuP: Failed to create device file(%s)!\n",
				dev_attr_led_duty.attr.name);

	if (device_create_file(sec_projector, &dev_attr_caic) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_caic.attr.name);
// END CSLBSuP PPNFAD a.jakubiak/m.wengierow

	if (device_create_file(sec_projector, &dev_attr_proj_fw) < 0)
		printk(KERN_ERR "Failed to create device file(%s)!\n",
				dev_attr_proj_fw.attr.name);


	ret = i2c_add_driver(&dpp3430_i2c_driver);
	ret = i2c_add_driver(&M08980_i2c_driver);
	ret |= misc_register(&projector_module_device);
	if (ret) {
		printk(KERN_ERR "Projector driver registration failed!\n");
	}


	printk(KERN_INFO "projector_module_init End \n");

	return ret;
}

void __exit projector_module_exit(void)
{
	i2c_del_driver(&dpp3430_i2c_driver);
	misc_deregister(&projector_module_device);
}

late_initcall(projector_module_init);
module_exit(projector_module_exit);

MODULE_DESCRIPTION("Samsung projector module driver");
MODULE_AUTHOR("Inbum Choi <inbum.choi@samsung.com>");
MODULE_LICENSE("GPL");
