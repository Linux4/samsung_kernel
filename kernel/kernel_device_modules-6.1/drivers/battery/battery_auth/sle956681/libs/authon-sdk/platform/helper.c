/**
 * \copyright
 * MIT License
 *
 * Copyright (c) 2024 Infineon Technologies AG
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE
 *
 * \endcopyright
 */
/**
 * @file   helper.c
 * @date   January, 2021
 * @brief  Implementation of common related platform functions
 */
#include "../../authon-sdk/interface/register.h"
#include "../authon_api.h"
#include "../authon_config.h"
#include "../authon_status.h"
#include "../../authon-sdk/platform/helper.h"

//todo System porting required: implements platform dependency header for HW RNG, CRC, SHA, timer or GPIO, if required.
#include <linux/delay.h>
#include <linux/ktime.h>
#include <linux/time.h>
#include <linux/random.h>

extern AuthOn_Capability authon_capability;

/**
 * @brief Print project related info
  * */
void print_info(void){

	PRINT("===================\r\n");
	PRINT("OPTIGA(TM) Authenticate On Kernel Driver\r\n");
    PRINT("===================\r\n");
}

/**
* @brief System timer delay in ms
* @param uw_time_ms delay in ms
\todo System porting required: implements delay according to platform
*/
void delay_ms(uint32_t uw_time_ms) {
    for(;uw_time_ms;uw_time_ms--){
		udelay(1000);
	}
}

static uint8_t Reflect8(uint8_t in){
    uint8_t resByte = 0;
    int i=0;

    for (i = 0; i < 8; i++){
        if ((in & (1 << i)) != 0){
            resByte |= (uint8_t)(1 << (7 - i));
        }
    }

    return resByte;
}

static uint16_t Reflect16(uint16_t val){
    uint16_t resVal = 0;
    int i=0;

    for (i = 0; i < 16; i++){
        if ((val & (1 << i)) != 0){
            resVal |= (uint16_t)(1 << (15 - i));
        }
    }

    return resVal;
}

/**
* @brief Generates CRC required for SWI
* @param data_p data input to be calculated
* @param len number of bytes 
*/
uint16_t crc16_gen(uint8_t *data_p, uint16_t len){
     uint16_t i,j;
     uint16_t crc = 0xffff;
	 uint8_t data_reflet;

	if (len == 0)
	return (~crc);

	for(i=0; i<len; i++){
	    data_reflet = Reflect8(data_p[i]);
        crc ^= data_reflet << 8; /* move byte into MSB of 16bit CRC */

        for (j = 0; j < 8; j++){
            if ((crc & 0x8000) != 0){ /* test for MSB = bit 15 */            
                crc = ((crc << 1) ^ POLY);
            }
            else{
                crc <<= 1;
            }
        }
    }
	crc = Reflect16(crc);
    crc = crc ^ 0xffff;

	return (crc);
}


#if (ENABLE_DEBUG_PRINT == 1)
void PrintByteArrayHex(uint8_t *data, uint16_t ArrayLen){
	uint16_t i;
	for(i=0; i<ArrayLen; i++)
	{
		if((i != 0 )&&((i & 0x03) ==0) && (ArrayLen > 7 )){
      PRINT("\r\n");
    }
		if((i==0)||(i%4==0)){
			PRINT("[Page:%.2d] ", i/4);
		}

		PRINT_L("0x%.2X ", data[i]);
	}
	//PRINT("\r\n");
}
/**
* @brief Prints a selected row of the UID table structure
*/
void print_row_UID_table(uint8_t *UID) {
  PRINT("UID: %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x ", UID[0], UID[1], UID[2], UID[3],
                                                                               UID[4], UID[5], UID[6], UID[7],
                                                                               UID[8], UID[9], UID[10], UID[11]);    
}

/**
* @brief Prints HA parameters
*/
void print_HA_parameter(uint8_t *parameter) {
  PRINT("%.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x ", parameter[0], parameter[1], parameter[2], parameter[3],
                                                              parameter[4], parameter[5], parameter[6], parameter[7],
                                                              parameter[8], parameter[9]);    
}


/**
* @brief Prints ODC
*/
void print_odc(uint32_t *ODC){
int i=0;
PRINT("ODC:\r\n");
    for (i = 0; i < 12; i++) {
        PRINT("%.8x ", ODC[i]);
        if(i==0){
            continue;
        }
        if(((i+1)%4)==0){
            PRINT("\r\n");
        }
    }
    PRINT("\r\n");
}

/**
* @brief Prints Public key
*/
void print_puk(uint32_t *PUK){
  int i=0;
    PRINT("PUK:\r\n");
    for (i = 0; i < 5; i++) {
        PRINT("%.8x ", PUK[i]);
        if (i == 0) {
            continue;
        }
        if (((i + 1) % 4) == 0) {
            PRINT("\r\n");
        }
    }
    PRINT("\r\n");
}

void print_nvm_userpage(uint8_t *page, uint8_t count){
  int i=0;
	PRINT("idx: NVM Data\n");
	PRINT("   : B0 B1 B2 B3 \n");
	PRINT("   : ----------- \n");
    for (i = 0; i < count; ){
        PRINT("%.2d : %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x \r\n",i, page[0+i], page[1+i], page[2+i], page[3+i]);
        i += 4;
    }
}

void print_nvm_TMpage(uint8_t *page, uint16_t count){
  int i=0;
	PRINT("PG: idx: NVM TM Data\n");
	PRINT("   : B0 B1 B2 B3 B4 B5 B6 B7 \n");
	PRINT("   : ----------------------- \n");
    for (i = 0; i < count; ){
        PRINT("%.2d : %.3d : %.2x %.2x %.2x %.2x %.2x %.2x %.2x %.2x \r\n",(i/8),i, page[0+i], page[1+i], page[2+i], page[3+i],page[4+i],
        		page[5+i], page[6+i], page[7+i]);
        i += 8;
    }
}

void print_nvm_page(uint8_t *data, uint8_t pagenumber){
  
	if(pagenumber<100){
		PRINT(": Page %.2d\n", pagenumber);
	}else{
		PRINT(": Page %.3d\n", pagenumber);
	}
	//PRINT(": B0 B1 B2 B3 B4 B5 B6 B7 \n");
	//PRINT(": ----------------------- \n");

	PRINT(": %.2X %.2X %.2X %.2X \r\n",data[0], data[1], data[2], data[3]);
	PRINT("\n");
}

void print_nvm_page_with_offset(uint8_t *data, uint8_t pagenumber, uint8_t offset){
  

	PRINT("<< Page %.3d\n", pagenumber+offset);

	//PRINT(": B0 B1 B2 B3 B4 B5 B6 B7 \n");
	//PRINT(": ----------------------- \n");

	PRINT(": %.2X %.2X %.2X %.2X \r\n",data[0], data[1], data[2], data[3]);
	PRINT("\n");
}
void cls_screen(void){
    PRINT("\x1b[2J\x1b[;H");/* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
}

void PRINT(const char *string,...){

    char buffer[256];
    va_list args;
    va_start(args, string);
    vsprintf(buffer, string, args);
    pr_info("%s", buffer);
    va_end(args);
}

void PRINT_L(const char *string,...){
    char buffer[256];
    va_list args;
    va_start(args, string);
    vsprintf(buffer, string, args);
    pr_cont("%s", buffer);
    va_end(args);
}

void PRINT_C(PRINT_COLOR COLOR, char *string,...){
	switch(COLOR){
	case RED:
		PRINT("\033[1;31m");
		break;
	case GREEN:
		PRINT("\033[1;32m");
		break;
	case YELLOW:
		PRINT("\033[1;33m");
		break;
	case BLUE:
		PRINT("\033[1;34m");
		break;
	case MAGENTA:
		PRINT("\033[1;35m");
		break;
	case CYAN:
		PRINT("\033[1;36m");
		break;
	}

}
#else
void PRINT(const char *string,...){}
void PRINT_C(PRINT_COLOR COLOR, char *string,...){}
void cls_screen(void){}
void print_row_UID_table(uint8_t *UID){}
void print_odc(uint32_t *ODC){}
void print_puk(uint32_t *PUK){}
void print_nvm_userpage(uint8_t *page, uint8_t count){}
void print_nvm_TMpage(uint8_t *page, uint16_t count){}
void print_nvm_page(uint8_t *page, uint8_t pagenumber){}
#endif

/**
 * @brief This function implements displaying of byte array in hex format
 * @param  prgbBuf Pointer to the byte array to be displayed
 * @param wLen Length of byte array to be displayed
 */
void print_array(uint8_t* prgbBuf, uint16_t wLen)
{
	uint16_t wIndex;

	for(wIndex = 0; wIndex < wLen; wIndex++)
	{
        if ((wIndex % 10) == 0)
        {
            PRINT("\r\n");
        }
		if(*(prgbBuf+wIndex) < 16)
		{
			PRINT("0%X ", *(prgbBuf+wIndex));
		}
		else
		{
			PRINT("%X ", *(prgbBuf+wIndex));
		}
	}
	PRINT("\r\n");
}

void print_msg_digest(uint8_t* data, uint8_t len){
    //char print[10];
    uint32_t i=0;
    for (i=0; i < len; i++)
    {
        if ((i % 10) == 0)
        {
            PRINT("\r\n");
        }
        PRINT("0x%02X ", *(data+i));        
    }
    PRINT("\r\n");
}

#if (ENABLE_SWI_TAU_CALIBRATION==1)
//todo System porting required: implements platform SWI tuning.
/**
* @brief Pseudocode function for SWI tau calibration
\todo System porting required: Initialize the GPIO, monitor GPIO and calibrate timing, populate the bit-bang timing.
*/
void swi_tau_calibration(void){
#define CONFIG_OUTPUT    (0)   //todo: configure the SWI output
#define CONFIG_INPUT     (0)   //todo: configure the SWI input
#define CONFIG_GPIO      (x)   //todo: select an unused GPIO

#if (CONFIG_OUTPUT==1)
	//todo: Initilize GPIO as output
	//platform_config_gpio_output(CONFIG_GPIO);

	uint32_t period=100; //Configure Tau here. Need 1tau, 3tau, 5tau, 10tau, 10ms
	//Run a 50% duty cycle pulse on the GPIO
	while(1)
	{
		//platform_toggle_gpio(CONFIG_GPIO);
		udelay(period);
	}
#endif

#if (CONFIG_INPUT==1)
	//todo: Initialize GPIO as an input
	//platform_config_gpio_input(CONFIG_GPIO);

	//Connect GPIO to GND
	uint8_t low = //platform_get_gpio(CONFIG_GPIO);

	//Connect GPIO to VDD
	uint8_t high = //platform_get_gpio(CONFIG_GPIO);

#endif
#undef CONFIG_OUTPUT
#undef CONFIG_INPUT
#undef CONFIG_GPIO
}
#endif