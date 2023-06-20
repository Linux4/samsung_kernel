/* filename: ISSP_Directives.h */
#include "issp_revision.h"
#ifdef PROJECT_REV_304

/*
* Copyright 2006-2007, Cypress Semiconductor Corporation.

* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License
* as published by the Free Software Foundation; either version 2
* of the License, or (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
* MA  02110-1301, USA.
*/

/*--------------------- Compiler Directives ----------------------------------
*/
#ifndef INC_ISSP_DIRECTIVES
#define INC_ISSP_DIRECTIVES

 /*
 This directive will enable a Genral Purpose test-point on P1.7
 It can be toggled as needed to measure timing, execution, etc...
 A "Test Point" sets a GPIO pin of the host processor high or low. This GPIO
 pin can be observed with an oscilloscope to verify the timing of key
 programming steps. TPs have been added in main() that set Port 0, pin 1
 high during bulk erase, during each block write and during security write.
 The timing of these programming steps should be verified as correct as part
 of the validation process of the final program.

 ****************************************************************************
 ************* USER ATTENTION REQUIRED: TARGET SUPPLY VOLTAGE ***************
 ****************************************************************************
 This directive causes the proper Initialization vector #3 to be sent
 to the Target, based on what the Target Vdd programming voltage will
 be. Either 5V (if #define enabled) or 3.3V (if #define disabled).
 */

/*
#define TARGET_VOLTAGE_IS_5V
*/

/*
 ****************************************************************************
 **************** USER ATTENTION REQUIRED: PROGRAMMING MODE *****************
 ****************************************************************************
 This directive selects whether code that uses reset programming mode or code
 that uses power cycle programming is use. Reset programming mode uses the
 external reset pin (XRES) to enter programming mode. Power cycle programming
 mode uses the power-on reset to enter programming mode.
 Applying signals to various pins on the target device must be done in a
 deliberate order when using power cycle mode. Otherwise, high signals to GPIO
 pins on the target will power the PSoC through the protection diodes.
*/

/*
#define RESET_MODE

*/

/*
#define TX_ON

*/

/*
#define LCD_ON
*/

 /*****************************************************************************
 ****************** USER ATTENTION REQUIRED: TARGET PSOC ********************
 ****************************************************************************
 The directives below enable support for various PSoC devices. The root part
 number to be programmed should be un-commented so that its value becomes
 defined.  All other devices should be commented out.
 Select one device to be supported below:

 **** CY821x34 devices ****
*/


/*
#define CY8C21234
#define CY8C21334
#define CY8C21434
#define CY8C21534

*/
#define CY8C20234			/* Quark */


#endif  /*(INC_ISSP_DIRECTIVES)*/
#endif  /*(PROJECT_REV_) */
/* end of file ISSP_Directives.h */
