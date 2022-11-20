#ifndef __PRJ_DPP3430_DATA_H__
#define __PRJ_DPP3430_DATA_H__

#include <linux/projector/projector.h>
#include "dpp343x_img.h"

#define MAX_LENGTH (20)
#define OFFSET_I2C_DIRECTION (0)
#define OFFSET_I2C_NUM_BYTES (1)
#define OFFSET_I2C_DATA_START (2)


static unsigned char InitData_FlashDataTypeSelect[] = {
	PRJ_WRITE, 2, 0xDE, 0xA0
};

static unsigned char InitData_FlashUpdatePrecheck[] = {
	PRJ_READ, 5, 1, 0xDD, 0x14, 0x00, 0x00, 0x00,
};

  static unsigned char InitData_FlashDataLength[] = {
	PRJ_WRITE, 3, 0xDF, 0x14, 0x00
};

static unsigned char InitData_ReadFlashData[] = {
	PRJ_READ, 1, 20, 0xE3, 
};

/**/
static unsigned char fw_ReadStatus[] = {
	PRJ_READ, 1, 1, 0xD0, 
};
static unsigned char fw_WriteDataType[] = {
	PRJ_WRITE, 2, 0xDE, 0x00
};
static unsigned char fw_WriteDataLength[] = {
	PRJ_WRITE, 3, 0xDF, 0x00, 0x04
};
static unsigned char fw_ReadUpdatePreCheck[] = {
	PRJ_READ, 5, 1, 0xDD, 0x00, 0x00, 0x08, 0x00,
};
static unsigned char fw_WriteEraseFlash[] = {
	PRJ_WRITE, 5, 0xE0, 0xAA, 0xBB, 0xCC, 0xDD,
};
static unsigned char fw_WriteStartData[] = {
	PRJ_FWWRITE, 1, 0xE1,
};
static unsigned char fw_WriteContinueData[] = {
	PRJ_FWWRITE, 1, 0xE2,
};
static unsigned char fw_WriteSystemReset[] = {
	PRJ_WRITE, 5, 0x00, 0xAA, 0xBB, 0xCC, 0xDD,
};

/**/

static unsigned char fwversion_FlashDataTypeSelect[] = {
	PRJ_WRITE, 2, 0xDE, 0xB0,
	PRJ_READ, 5, 1, 0xDD, 0x04, 0x00, 0x00, 0x00,
};

static unsigned char fwversion_ReadFlashData[] = {
	PRJ_WRITE, 3, 0xDF, 0x04, 0x00,
	PRJ_READ, 1, 4, 0xE3, 
};

//static unsigned char WVGA_RGB888[] = {
//	PRJ_WRITE, 5, 0x2E, 0x20, 0x03, 0xE0, 0x01,
//	PRJ_WRITE, 9, 0x10, 0x00, 0x00, 0x00, 0x00, 0x20, 0x03, 0xE0, 0x01,
//	PRJ_WRITE, 2, 0x07, 0x43
//};		

//static unsigned char VWVGA_RGB888[] = {/*Vertical */
//	PRJ_WRITE, 5, 0x2E, 0x20, 0x03, 0xE0, 0x01,
//	PRJ_WRITE, 9, 0x10, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x01, 0x20, 0x03,
//	PRJ_WRITE, 2, 0x07, 0x43
//};		

static unsigned char VSWVGA_RGB888[] = {/*Vertical / swap*/
	PRJ_WRITE, 5, 0x2E, 0xE0, 0x01, 0x20, 0x03,
	PRJ_WRITE, 9, 0x10, 0x00, 0x00, 0x00, 0x00, 0xE0, 0x01, 0x20, 0x03,
	PRJ_WRITE, 2, 0x05, 0x00,
	PRJ_WRITE, 2, 0x07, 0x43,
	PRJ_WRITE, 2, 0xB2, 0x00,
};		

static unsigned char fWVGA_RGB888[] = {
	PRJ_WRITE, 5, 0x2E, 0x56, 0x03, 0xE0, 0x01,
	PRJ_WRITE, 9, 0x10, 0x00, 0x00, 0x00, 0x00, 0x56, 0x03, 0xE0, 0x01,
	PRJ_WRITE, 2, 0x07, 0x43
};		// Full-WVGA(854x480) for internal test pattern 

static unsigned char I_white[] = {
	PRJ_WRITE, 2, 0x05, 0x01,
	PRJ_WRITE, 3, 0x0B, 0x00, 0x77
};

static unsigned char I_black[] = {
	PRJ_WRITE, 2, 0x05, 0x01,
	PRJ_WRITE, 3, 0x0B, 0x00, 0x00
};

static unsigned char I_red[] = {
	PRJ_WRITE, 2, 0x05, 0x01,
	PRJ_WRITE, 3, 0x0B, 0x00, 0x11
};

static unsigned char I_green[] = {
	PRJ_WRITE, 2, 0x05, 0x01,
	PRJ_WRITE, 3, 0x0B, 0x00, 0x22
};

static unsigned char I_blue[] = {
	PRJ_WRITE, 2, 0x05, 0x01,
	PRJ_WRITE, 3, 0x0B, 0x00, 0x33
};

static unsigned char I_4x4checker[] = {
	PRJ_WRITE, 2, 0x05, 0x01,
	PRJ_WRITE, 7, 0x0B, 0x07, 0x07, 0x04, 0x00, 0x04, 0x00		
};

static unsigned char I_diagonal[] = {
	PRJ_WRITE, 2, 0x05, 0x01,
	PRJ_WRITE, 5, 0x0B, 0x04, 0x70, 0x7F, 0x7F		
};

// BEGIN PPNFAD a.jakubiak/m.wengierow
static unsigned char I_16x9checker[] = {

	PRJ_WRITE, 2, 0x05, 0x01,
	PRJ_WRITE, 7, 0x0B, 0x07, 0x70, 0x10, 0x00, 0x09, 0x00
};
// END PPNFAD a.jakubiak/m.wengierow

static unsigned char I_stripe[] = {
	PRJ_WRITE, 2, 0x05, 0x01,
	PRJ_WRITE, 5, 0x0B, 0x05, 0x70, 0x01, 0x09	
};

static unsigned char RGB_led_on[] = {
	PRJ_WRITE, 2, 0x52, 0x07
};

static unsigned char RGB_led_off[] = {
	PRJ_WRITE, 2, 0x52, 0x00
};

static unsigned char Output_Curtain_Enable[] = {
	PRJ_WRITE, 2, 0x16, 0x01
};

static unsigned char Output_Curtain_Disable[] = {
	PRJ_WRITE, 2, 0x16, 0x00
};

static unsigned char Freeze_On[] = {
	PRJ_WRITE, 2, 0x1A, 0x01
};

static unsigned char Freeze_Off[] = {
	PRJ_WRITE, 2, 0x1A, 0x00
};

static unsigned char Output_Rotate_0[] = {
	PRJ_WRITE, 2, 0x14, 0x02,
	PRJ_WRITE, 5, 0x12, 0x20, 0x01, 0xE0, 0x01,
};

static unsigned char Output_Rotate_270[] = {
	PRJ_WRITE, 2, 0x14, 0x03,
	PRJ_WRITE, 5, 0x12, 0xE0, 0x01, 0x20, 0x03, 
};

static unsigned char Output_Rotate_180[] = {
	PRJ_WRITE, 2, 0x14, 0x04,
	PRJ_WRITE, 5, 0x12, 0x20, 0x01, 0xE0, 0x01,
};

static unsigned char Output_Rotate_90[] = {
	PRJ_WRITE, 2, 0x14, 0x05,
	PRJ_WRITE, 5, 0x12, 0xE0, 0x01, 0x20, 0x03, 
};

static unsigned char Internal_pattern_direction[] = {
	PRJ_WRITE, 2, 0x14, 0x02,
	PRJ_WRITE, 5, 0x12, 0x56, 0x03, 0xE0, 0x01
};

static unsigned char External_source[] = {
	PRJ_WRITE, 2, 0x05, 0x00
};

static unsigned char Dmd_seq[] = {
	PRJ_WRITE, 2, 0x22, 0x00
};

static unsigned char Motor_CW_out[] = {
	PRJMSPD_WRITE, 2, 0x22, 0x13,		//	HW : 20ms / CW
	PRJMSPD_WRITE, 2, 0x22, 0x12
};

static unsigned char Motor_CCW_out[] = {
	PRJMSPD_WRITE, 2, 0x22, 0x11,		// HW : 20ms / CCW
	PRJMSPD_WRITE, 2, 0x22, 0x10
};
// BEGIN PPNFAD a.jakubiak/m.wengierow

// FIX ME - values
// XXX - can we change it to RED_MSB_DAC[] ect. ?
//static unsigned char red_msb_register[] = {
//	PRJ_WRITE, 5, 0x39, 0x00, 0x00, 0x00, 0x03,
//	PRJ_WRITE, 5, 0x38, 0x00, 0x00, 0x00, 0xD0,
//};
//
//static unsigned char red_lsb_register[] = {
//	PRJ_WRITE, 5, 0x39, 0x00, 0x00, 0x00, 0x04,
//	PRJ_WRITE, 5, 0x38, 0x00, 0x00, 0x00, 0xD0,
//};
//
//static unsigned char green_msb_register[] = {
//	PRJ_WRITE, 5, 0x39, 0x00, 0x00, 0x00, 0x05,
//	PRJ_WRITE, 5, 0x38, 0x00, 0x00, 0x00, 0xD0,
//};
//
//static unsigned char green_lsb_register[] = {
//	PRJ_WRITE, 5, 0x39, 0x00, 0x00, 0x00, 0x06,
//	PRJ_WRITE, 5, 0x38, 0x00, 0x00, 0x00, 0xD0,
//};
//
//static unsigned char blue_msb_register[] = {
//	PRJ_WRITE, 5, 0x39, 0x00, 0x00, 0x00, 0x07,
//	PRJ_WRITE, 5, 0x38, 0x00, 0x00, 0x00, 0xD0,
//};
//
//static unsigned char blue_lsb_register[] = {
//	PRJ_WRITE, 5, 0x39, 0x00, 0x00, 0x00, 0x08,
//	PRJ_WRITE, 5, 0x38, 0x00, 0x00, 0x00, 0xD0,
//};
// END PPNFAD a.jakubiak/m.wengierow


static unsigned char CAIC_Current[] = {//131202
	PRJ_WRITE, 7, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

static unsigned char CCA_Disable[] = {//131202
	PRJ_WRITE, 2, 0x86, 0x00
};

static unsigned char Gamma_Select[] = {//131205
	PRJ_WRITE, 2, 0x27, 0x00
};

static unsigned char RED_MSB_DAC[] = {
	PRJMSPD_WRITE, 2, 0x03, 0x00
};

static unsigned char RED_LSB_DAC[] = {
	PRJMSPD_WRITE, 2, 0x04, 0x00
};

static unsigned char GREEN_MSB_DAC[] = {
	PRJMSPD_WRITE, 2, 0x05, 0x00
};

static unsigned char GREEN_LSB_DAC[] = {
	PRJMSPD_WRITE, 2, 0x06, 0x00
};

static unsigned char BLUE_MSB_DAC[] = {
	PRJMSPD_WRITE, 2, 0x07, 0x00
};

static unsigned char BLUE_LSB_DAC[] = {
	PRJMSPD_WRITE, 2, 0x08, 0x00
};

static unsigned char Keystone_Enable[] = {						// THR : 1.79, DMD Offset : 1
	PRJ_WRITE, 6, 0x88, 0x01, 0xCA, 0x01, 0x00, 0x01
};

static unsigned char Keystone_Disable[] = {						// THR : 1.79, DMD Offset : 1
	PRJ_WRITE, 6, 0x88, 0x00, 0xCA, 0x01, 0x00, 0x01
};

static char Keystone_SetAngle[] = {					// Pitch Angle : -40 ~ 40 deg
	PRJ_WRITE, 3, 0xBB, 0x00, 0x00
};

static unsigned char LABB_Control[] = {
	PRJ_WRITE, 3, 0x80, 0x00, 0x00
};

static unsigned char M08980_SoftReset[] = {	   
	PRJMSPD_WRITE, 2, 0x30, 0xAA,   
	PRJMSPD_WRITE, 2, 0x30, 0x00
};

static unsigned char M08980_Init[] = {	
	PRJMSPD_WRITE, 2, 0x02, 0x09,   
	PRJMSPD_WRITE, 2, 0x0A, 0x7E,   
	PRJMSPD_WRITE, 2, 0x03, 0x01,   
	PRJMSPD_WRITE, 2, 0x04, 0x35,   
	PRJMSPD_WRITE, 2, 0x05, 0x01,   
	PRJMSPD_WRITE, 2, 0x06, 0x35,   
	PRJMSPD_WRITE, 2, 0x07, 0x01,   
	PRJMSPD_WRITE, 2, 0x08, 0x35,   
	PRJMSPD_WRITE, 2, 0x09, 0x77,   
	PRJMSPD_WRITE, 2, 0x0C, 0x00,   
	PRJMSPD_WRITE, 2, 0x16, 0x08,   
	PRJMSPD_WRITE, 2, 0x19, 0x77,   
	PRJMSPD_WRITE, 2, 0x1A, 0x07,   
	PRJMSPD_WRITE, 2, 0x13, 0x12,   
	PRJMSPD_WRITE, 2, 0x14, 0x53,   
	PRJMSPD_WRITE, 2, 0x1D, 0x3F,   
	PRJMSPD_WRITE, 2, 0x1E, 0x10,   
	PRJMSPD_WRITE, 2, 0x27, 0x10,   
	PRJMSPD_WRITE, 2, 0x28, 0x17,   
	PRJMSPD_WRITE, 2, 0x29, 0x70,   
	PRJMSPD_WRITE, 2, 0x2B, 0x80,   
	PRJMSPD_WRITE, 2, 0x2C, 0x14,   
	PRJMSPD_WRITE, 2, 0x2D, 0x65,   
	PRJMSPD_WRITE, 2, 0x21, 0x88,   
	PRJMSPD_WRITE, 2, 0x20, 0x00,   
	PRJMSPD_WRITE, 2, 0x3A, 0xFF,   
	PRJMSPD_WRITE, 2, 0x3B, 0xFE,   
	PRJMSPD_WRITE, 2, 0x17, 0xF0, 
	PRJMSPD_WRITE, 2, 0x15, 0x04	
};

static unsigned char M08980_PRJON[] = {
	PRJMSPD_WRITE, 2, 0x0B, 0x09  
};

static unsigned char M08980_PRJOFF[] = {
	PRJMSPD_WRITE, 2, 0x0B, 0x08
};

static unsigned char M08980_LEDON[] = {
	PRJMSPD_WRITE, 2, 0x1C, 0x90,
	PRJMSPD_WRITE, 2, 0x1B, 0x21
};

static unsigned char M08980_LEDOFF[] = {
	PRJMSPD_WRITE, 2, 0x1B, 0x20
};

static unsigned char M08980_AlarmClear[] = {
	PRJMSPD_WRITE, 2, 0x21, 0x88    
};

static unsigned char M08980_PI_LDO_Enable[] = {
	PRJMSPD_WRITE, 2, 0x02, 0x08    
};

static unsigned char M08980_PI_LDO_Disable[] = {
	PRJMSPD_WRITE, 2, 0x02, 0x09    
};

static unsigned char M08980_SPI_Enable[] = {
	PRJMSPD_WRITE, 2, 0x16, 0x00    
};

static unsigned char M08980_SPI_Disable[] = {
	PRJMSPD_WRITE, 2, 0x16, 0x08    
};

/* CAIC */
static unsigned char CAIC_Enable[] = {
	PRJ_WRITE, 2, 0x50, 0x01    
};

static unsigned char CAIC_Disable[] = {
	PRJ_WRITE, 2, 0x50, 0x00    
};

//static unsigned char FW_VER_CHECK[] = {
//	ʿϴٸ ߰
//};

#endif
