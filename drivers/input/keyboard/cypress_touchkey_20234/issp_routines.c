/* filename: ISSP_Routines.c
*/
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
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
* MA  02110-1301, USA.
*/


/*
#include <m8c.h>
#include "PSoCAPI.h"
*/

#include "issp_defs.h"
#include "issp_vectors.h"
#include "issp_extern.h"
#include "issp_errors.h"
#include "issp_directives.h"
#include "issp_delays.h"
#include <linux/module.h>

unsigned char  bTargetDataIN;
unsigned char  abTargetDataOUT[MAX_TARGET_DATABUFF_LEN];

unsigned char  bTargetAddress;
unsigned char  bTargetDataPtr;
unsigned char  bTargetID[10];

unsigned char  fIsError;

/* ((((((((((((((((((((( LOW-LEVEL ISSP SUBROUTINE SECTION ))))))))))))))))))))
   (( The subroutines in this section use functions from the C file          ))
   (( ISSP_Drive_Routines.c. The functions in that file interface to the     ))
   (( processor specific hardware. So, these functions should work as is, if ))
   (( the routines in ISSP_Drive_Routines.c are correctly converted.         ))
   (((((((((((((((((((((((((((((((((((())))))))))))))))))))))))))))))))))))))))
*/

 /*============================================================================
 RunClock()
 Description:
 Run Clock without sending/receiving bits. Use this when transitioning from
 write to read and read to write "num_cycles" is number of SCLK cycles, not
 number of counter cycles.

 SCLK cannot run faster than the specified maximum frequency of 8MHz. Some
 processors may need to have delays added after setting SCLK low and setting
 SCLK high in order to not exceed this specification. The maximum frequency
 of SCLK should be measured as part of validation of the final program

 ============================================================================
*/
void RunClock(unsigned int iNumCycles)
{
	int i;

	for (i = 0; i < iNumCycles; i++) {
		SCLKLow();
		SCLKHigh();
	}
}

 /*============================================================================
 bReceiveBit()
 Clocks the SCLK pin (high-low-high) and reads the status of the SDATA pin
 after the rising edge.

 SCLK cannot run faster than the specified maximum frequency of 8MHz. Some
 processors may need to have delays added after setting SCLK low and setting
 SCLK high in order to not exceed this specification. The maximum frequency
 of SCLK should be measured as part of validation of the final program

 Returns:
     0 if SDATA was low
     1 if SDATA was high
 ============================================================================
*/
unsigned char bReceiveBit(void)
{
	SCLKLow();
	SCLKHigh();
	if (fSDATACheck())
		return 1;
	else
		return 0;
}

 /*============================================================================
 bReceiveByte()
 Calls ReceiveBit 8 times to receive one byte.
 Returns:
     The 8-bit values recieved.
 ============================================================================
*/
unsigned char bReceiveByte(void)
{
	unsigned char b;
	unsigned char bCurrByte = 0x00;

	for (b = 0; b < 8; b++)
		bCurrByte = (bCurrByte<<1) + bReceiveBit();

	return bCurrByte;
}


 /*
 ============================================================================
 SendByte()
 This routine sends up to one byte of a vector, one bit at a time.
    bCurrByte   the byte that contains the bits to be sent.
    bSize       the number of bits to be sent. Valid values are 1 to 8.

 SCLK cannot run faster than the specified maximum frequency of 8MHz. Some
 processors may need to have delays added after setting SCLK low and setting
 SCLK high in order to not exceed this specification. The maximum frequency
 of SCLK should be measured as part of validation of the final program

 There is no returned value.
 ============================================================================
*/
void SendByte(unsigned char bCurrByte, unsigned char bSize)
{
	unsigned char b = 0;

	for (b = 0; b < bSize; b++) {
		if (bCurrByte & 0x80) {
			SetSDATAHigh();
			SCLKHigh();
			SCLKLow();
		} else {
			SetSDATALow();
			SCLKHigh();
			SCLKLow();
		}
		bCurrByte = bCurrByte << 1;
	}
}

 /*============================================================================
 SendVector()
 This routine sends the vector specifed. All vectors constant strings found
 in ISSP_Vectors.h.  The data line is returned to HiZ after the vector is
 sent.
    bVect      a pointer to the vector to be sent.
    nNumBits   the number of bits to be sent.
    bCurrByte  scratch var to keep the byte to be sent.

 There is no returned value.
 ============================================================================
*/
void SendVector(const unsigned char *bVect, unsigned int iNumBits)
{
	SetSDATAStrong();
	while (iNumBits > 0) {
		if (iNumBits >= 8) {
			SendByte(*(bVect), 8);
			iNumBits -= 8;
			bVect++;
		} else {
			SendByte(*(bVect), iNumBits);
			iNumBits = 0;
		}
	}
	SetSDATAHiZ();
}


/*
 ============================================================================
 fDetectHiLoTransition()
 Waits for transition from SDATA = 1 to SDATA = 0.  Has a 100 msec timeout.
 TRANSITION_TIMEOUT is a loop counter for a 100msec timeout when waiting for
 a high-to-low transition. This is used in the polling loop of
 fDetectHiLoTransition(). The timing of the while(1) loops can be calculated
 and the number of loops is counted, using iTimer, to determine when 100
 msec has passed.

 SCLK cannot run faster than the specified maximum frequency of 8MHz. Some
 processors may need to have delays added after setting SCLK low and setting
 SCLK high in order to not exceed this specification. The maximum frequency
 of SCLK should be measured as part of validation of the final program

 Returns:
     0 if successful
    -1 if timed out.
 ============================================================================

*/
signed char fDetectHiLoTransition(void)
{
 /*  nTimer breaks out of the while loops if the wait in the two loops totals
     more than 100 msec.  Making this static makes the loop run a faster.
     This is really a processor/compiler dependency and it not needed.
*/
	unsigned long int iTimer = 0;

 /*    NOTE:
     These loops look unconventional, but it is necessary to check SDATA_PIN
     as shown because the transition can be missed otherwise, due to the
     length of the SDATA Low-High-Low after certain commands.

     Generate clocks for the target to pull SDATA High

 */
	iTimer = TRANSITION_TIMEOUT;

	SetSCLKStrong();
	while (1) {
		SCLKLow();
		/* Delay(CLK_DELAY); */
		if (fSDATACheck())       /* exit once SDATA goes HI */
			break;
		SCLKHigh();
		/*Delay(CLK_DELAY);*/
		 /*If the wait is too long then timeout*/
		if (iTimer-- == 0){			
			printk(KERN_ERR "%s: %d - error\n", __func__, __LINE__);
			return ERROR;
		}
	}
 /*
    Generate Clocks and wait for Target to pull SDATA Low again
 */
	iTimer = TRANSITION_TIMEOUT;
	while (1) {
		SCLKLow();
		/* Delay(CLK_DELAY); */
		if (!fSDATACheck()) /* exit once SDATA returns LOW */
			break;
		/*SCLKHigh();*/
		/* Delay(CLK_DELAY); */
		/* If the wait is too long then timeout */
		if (iTimer-- == 0){			
			printk(KERN_ERR "%s: %d - error\n", __func__, __LINE__);
			return ERROR;
		}
	}
	return PASS;
}


/* ((((((((((((((((((((( HIGH-LEVEL ISSP ROUTINE SECTION ))))))))))))))))))))))
   (( These functions are mostly made of calls to the low level routines     ))
   (( above.  This should isolate the processor-specific changes so that     ))
   (( these routines do not need to be modified.                             ))
   (((((((((((((((((((((((((((((((((((())))))))))))))))))))))))))))))))))))))))
*/

#ifdef RESET_MODE
 /*============================================================================
 fXRESInitializeTargetForISSP()
 Implements the intialization vectors for the device.
 Returns:
     0 if successful
     INIT_ERROR if timed out on handshake to the device.
 ============================================================================
*/
signed char fXRESInitializeTargetForISSP(void)
{
	 /*Configure the pins for initialization*/
	SetSDATAHiZ();
	SetSCLKStrong();
	SCLKLow();
	SetXRESStrong();

	/* Cycle reset and put the device
	  in programming mode when it exits reset */
	AssertXRES();
	Delay(XRES_CLK_DELAY);
	DeassertXRES();

	/*
	 !!! NOTE:
	  The timing spec that requires that the first Init-Vector happen within
	  1 msec after the reset/power up. For this reason, it is not advisable
	  to separate the above RESET_MODE or POWER_CYCLE_MODE code from the
	  Init-Vector instructions below. Doing so could introduce excess delay
	  and cause the target device to exit ISSP Mode.

	PTJ: Send id_setup_1 instead of init1_v
	PTJ: both send CA Test Key and do a Calibrate1 SROM function
	*/
	SendVector(id_setup_1, num_bits_id_setup_1);
	fIsError = fDetectHiLoTransition();
	if (fIsError != 0)
		return INIT_ERROR;
	SendVector(wait_and_poll_end, num_bits_wait_and_poll_end);

	/*
	 NOTE: DO NOT not wait for HiLo on SDATA after vector Init-3
		   it does not occur (per spec).
	*/
	return PASS;
}

#else  /* else = the part is power cycle programmed */
/*
 ============================================================================
 fPowerCycleInitializeTargetForISSP()
 Implements the intialization vectors for the device.
 The first time fDetectHiLoTransition is called the Clk pin is highZ because
 the clock is not needed during acquire.
 Returns:
     0 if successful
     INIT_ERROR if timed out on handshake to the device.
 ============================================================================

*/
signed char fPowerCycleInitializeTargetForISSP(void)
{
	unsigned char n;

/*
     Set all pins to highZ to avoid back powering the PSoC through the GPIO
     protection diodes.
*/
	SetSCLKHiZ();
	SetSDATAHiZ();
	printk(KERN_ERR "%s: %d \n", __func__, __LINE__);

	 /* Turn on power to the target device before other signals */
	SetTargetVDDStrong();
	//jjlee	for (n = 0; n < 100; n++)
			Delay(10000);
	ApplyTargetVDD();
	 /* wait 1msec for the power to stabilize */

	for (n = 0; n < 10; n++)
		Delay(DELAY100us);

	/*
	 Set SCLK to high Z so there is no clock and wait for a high to low
	 transition on SDAT. SCLK is not needed this time.

	*/
	SetSCLKHiZ();
	fIsError = fDetectHiLoTransition();
	if (fIsError != 0)
		return INIT_ERROR;

	 /*Configure the pins for initialization */
	SetSDATAHiZ();
	SetSCLKStrong();
	SCLKLow();
/*PTJ: DO NOT SET A BREAKPOINT HERE AND EXPECT SILICON ID TO PASS!

	!!! NOTE:
	The timing spec that requires that the first Init-Vector happen within
	1 msec after the reset/power up. For this reason, it is not advisable
	to separate the above RESET_MODE or POWER_CYCLE_MODE code from the
	Init-Vector instructions below. Doing so could introduce excess delay
	and cause the target device to exit ISSP Mode.

	*/

	 SendVector(wait_and_poll_end, num_bits_wait_and_poll_end);
	/*20100114 KJHW(Jason)  : 0114 by KJHW */


	SendVector(id_setup_1, num_bits_id_setup_1);

	fIsError = fDetectHiLoTransition();
	
	printk(KERN_ERR "%s: %d \n", __func__, __LINE__);
	if (fIsError != 0)
		return INIT_ERROR;
	SendVector(wait_and_poll_end, num_bits_wait_and_poll_end);

	/*
	 NOTE: DO NOT not wait for HiLo on SDATA after vector Init-3
		   it does not occur (per spec).
	*/
	return PASS;
}
#endif


 /*============================================================================
 fVerifySiliconID()
 Returns:
     0 if successful
     Si_ID_ERROR if timed out on handshake to the device.
 ============================================================================
*/

signed char fVerifySiliconID(void)
{
	SendVector(id_setup_2, num_bits_id_setup_2);
	fIsError = fDetectHiLoTransition();
	if (fIsError != 0) {
		#ifdef TX_ON
			TX8SW_PutCRLF();
			TX8SW_CPutString("fDetectHiLoTransition Error");
		#endif

		#ifdef LCD_ON
			LCD_Char_PrintString("fDetectHiLoTransition Error");
		#endif

		return SiID_ERROR;
	}
	SendVector(wait_and_poll_end, num_bits_wait_and_poll_end);

 /*   Send Read ID vector and get Target ID */
	SendVector(read_id_v, 11); /*Read-MSB Vector is the first 11-Bits */
	RunClock(2);  /*Two SCLK cycles between write & read */
	bTargetID[0] = bReceiveByte();
	RunClock(1);
	 /* 1+11 bits starting from the 3rd byte */
	SendVector(read_id_v + 2, 12);
	RunClock(2);                    /* Read-LSB Command */
	bTargetID[1] = bReceiveByte();

	RunClock(1);
	SendVector(read_id_v+4, 1); /*1 bit starting from the 5th byte */


    #ifdef TX_ON
		 /*Print READ-ID */
		TX8SW_PutCRLF();
		TX8SW_CPutString("Silicon-ID : ");
		TX8SW_PutChar(' ');
		TX8SW_PutSHexByte(bTargetID[0]);
		TX8SW_PutChar(' ');
		TX8SW_PutSHexByte(bTargetID[1]);
		TX8SW_PutChar(' ');

		/* See the latest spec. 40-95002, 40-95004, 001-15870, AN2026d*/
		switch (bTargetID[0]) {
		case 0x00:
			TX8SW_CPutString(
				"\r\nPSoC1 = 00xx (including Ovation-ONS)");
			switch (bTargetID[1]) {
			case 0x68:
				TX8SW_CPutString(
					"\r\nCY8C20234 8K, 512B(Quark)");
				TargetDatabufLen = 64;
				NumBanks = 1;
				BlocksPerBank = 128;
				SecurityBytesPerBank = 64;
				break;
			case 0xAD:
				TX8SW_CPutString(
					"\r\nCY8C20446A-24LQXI 16K, 2K(Krypton)");
				TargetDatabufLen = 128;
				NumBanks = 1;
				BlocksPerBank = 128;
				SecurityBytesPerBank = 64;
				break;
			case 0x37:
				TX8SW_CPutString(
					"\r\nCY8C21334 Automotive(Neutron) 8K,512B");
				TargetDatabufLen = 64;
				NumBanks = 1;
				BlocksPerBank = 128;
				SecurityBytesPerBank = 64;
				break;
			case 0x38:
				TX8SW_CPutString(
					"\r\nCY8C21434 Neutron");
				TargetDatabufLen = 64;
				NumBanks = 1;
				BlocksPerBank = 128;
				SecurityBytesPerBank = 64;
				break;
			default:
				break;
			}
			break;
		case 0x01:
			TX8SW_CPutString(
				"\r\nPSoC1 = 01xx(continued family and mask set growth");
			break;
		case 0x02:
			TX8SW_CPutString(
				"\r\nPSoC1 + SmartSense = 02xx");
			break;
		case 0x03:
			TX8SW_CPutString("\r\nUnallocated = 03xx");
			break;
		case 0x04:
			TX8SW_CPutString("\r\nPower PSoC = 04xx");
			break;
		case 0x05:
			TX8SW_CPutString(
				"\r\nTrueTouch Multi-Touch All Points(TMA) = 05xx");
			switch (bTargetID[1]) {
			case 0x9A:
				TX8SW_CPutString(
					"\r\nCY8CTMA340-LQI-01");
				TargetDatabufLen = 128;
				NumBanks = 1;
				BlocksPerBank = 256;
				SecurityBytesPerBank = 64;
				break;
			default:
				break;
			}

			break;
		case 0x06:
			TX8SW_CPutString(
				"\r\nTrueTouch Single Touch(TST) = 06xx");
			break;
		case 0x07:
			TX8SW_CPutString(
				"\r\nTrueTouch Multi-Touch Gesture(TMG) = 07xx");
			break;
		case 0x08:
			TX8SW_CPutString(
				"\r\nPSoC1 Value = 08xx");
			break;
		case 0x09:
			TX8SW_CPutString(
				"\r\nPSoC1 PLC = 09xx");
			break;
		case 0x0A:
			TX8SW_CPutString(
				"\r\nPSoC1 PLC + Ez Color = 0Axx");
			break;
		case 0x0B:
			TX8SW_CPutString(
				"\r\nPSoC1 + SmartSense_EMC = 0Bxx");
			break;
		case 0x0C:
			TX8SW_CPutString(
				"\r\nHaptics Only = 0Cxx");
			break;
		case 0x0D:
			TX8SW_CPutString(
				"\r\nHaptics + TrueTouch Multi-Touch All Points(TMA) = 0Dxx");
			break;
		case 0x0E:
			TX8SW_CPutString(
				"\r\nHaptics + TrueTouch Single Touch(TST = 0Exx");
			break;
		case 0x0F:
			TX8SW_CPutString(
				"\r\nHaptics + TrueTouch Multi-Touch Gesture(TMG) = 0Fxx");
			break;
		default:
			TX8SW_CPutString("\r\nUnknown Silicon ID !!");
			while (1)
				;
			break;
		}
	  #endif

		target_id_v[0] = bTargetID[0];
		target_id_v[1] = bTargetID[1];

	#ifdef LCD_ON
		LCD_Char_Position(1, 0);
		LCD_Char_PrintString("ID : ");
		LCD_Char_PrintInt8(bTargetID[0]);
		LCD_Char_PutChar(' ');
		LCD_Char_PrintInt8(bTargetID[1]);
		LCD_Char_PutChar(' ');
	#endif



	if (bTargetID[0] != target_id_v[0] || bTargetID[1] != target_id_v[1])
		return SiID_ERROR;
	else
		return PASS;
}

/*
 ============================================================================
 fEraseTarget()
 Perform a bulk erase of the target device.
 Returns:
     0 if successful
     ERASE_ERROR if timed out on handshake to the device.
 ============================================================================

*/

signed char fEraseTarget(void)
{
	SendVector(erase, num_bits_erase);
	fIsError = fDetectHiLoTransition();
	if (fIsError != 0)
		return ERASE_ERROR;
	SendVector(wait_and_poll_end, num_bits_wait_and_poll_end);
	return PASS;
}


 /*============================================================================
 LoadTarget()
 Transfers data from array in Host to RAM buffer in the target.
 Returns the checksum of the data.
 ============================================================================*/
unsigned int iLoadTarget(void)
{
	unsigned char bTemp;
	unsigned int  iChecksumData = 0;

	 /*Set SDATA to Strong Drive here because SendByte() does not */
	SetSDATAStrong();

	bTargetAddress = 0x00;
	bTargetDataPtr = 0x00;

	while (bTargetDataPtr < TargetDatabufLen) {
		bTemp = abTargetDataOUT[bTargetDataPtr];
		iChecksumData += bTemp;

		SendByte(write_byte_start, 5);
		SendByte(bTargetAddress, 6);
		SendByte(bTemp, 8);
		SendByte(write_byte_end, 3);

		bTargetAddress += 4;
		bTargetDataPtr++;
	}

	return iChecksumData;
	}

 /*============================================================================
 fProgramTargetBlock()
 Program one block with data that has been loaded into a RAM buffer in the
 target device.
 Returns:
     0 if successful
     BLOCK_ERROR if timed out on handshake to the device.
 ============================================================================
*/

signed char fProgramTargetBlock(unsigned char bBankNumber,
						unsigned char bBlockNumber)
{
	SendVector(set_block_num, num_bits_set_block_num);

	/* Set the drive here because SendByte() does not.*/
	SetSDATAStrong();
	SendByte(bBlockNumber, 8);
	SendByte(set_block_num_end, 3);

	 /*Send the program-block vector. */
	SendVector(program, num_bits_program);
	 /*wait for acknowledge from target. */
	 fIsError = fDetectHiLoTransition();
	 if (fIsError != 0)
		return BLOCK_ERROR;
	 /*Send the Wait-For-Poll-End vector*/

	 SendVector(wait_and_poll_end, num_bits_wait_and_poll_end);
	return PASS;

/*
    PTJ: Don't do READ-STATUS here because that will
    PTJ: require that we return multiple error values, if error occurs
*/
}


 /*============================================================================
 fAddTargetBankChecksum()
 Reads and adds the target bank checksum to the referenced accumulator.
 Returns:
     0 if successful
     VERIFY_ERROR if timed out on handshake to the device.
 ============================================================================
*/

signed char fAccTargetBankChecksum(unsigned int *pAcc)
{
	unsigned int wCheckSumData = 0;

	SendVector(checksum_v, num_bits_checksum);

	fIsError = fDetectHiLoTransition();
	if (fIsError != 0)
		return CHECKSUM_ERROR;
	SendVector(wait_and_poll_end, num_bits_wait_and_poll_end);

	 /*Send Read Checksum vector and get Target Checksum*/

	 SendVector(read_checksum_v, 11); /*first 11-bits is ReadCKSum-MSB*/

	 RunClock(2); /*Two SCLKs between write & read*/

	 bTargetDataIN = bReceiveByte();
	RunClock(1);
	wCheckSumData = ((unsigned int) bTargetDataIN)<<8;
	/*12 bits starting from 3rd character*/
	SendVector(read_checksum_v + 2, 12);

	 RunClock(2);                         /* Read-LSB Command*/

	 bTargetDataIN = bReceiveByte();
	RunClock(1);
	 /*Send the final bit of the command */
	SendVector(read_checksum_v + 4, 1);

	 wCheckSumData |= (unsigned int) bTargetDataIN;

	*pAcc = wCheckSumData;

	return PASS;
}

/*

 ============================================================================
 ReStartTarget()
 After programming, the target PSoC must be reset to take it out of
 programming mode. This routine performs a reset.
 ============================================================================
*/

void ReStartTarget(void)
{
	//int i;
	#ifdef RESET_MODE
	/* Assert XRES, then release, then disable XRES-Enable */
	AssertXRES();
	Delay(XRES_CLK_DELAY);
	DeassertXRES();
	#else
	/* Set all pins to highZ to avoid back powering
	   the PSoC through the GPIO protection diodes.
	*/
	SetSCLKHiZ();
	SetSDATAHiZ();
	/* Cycle power on the target to cause a reset */
	RemoveTargetVDD();
	// jjlee	for (i = 0; i < 100; i++)
			Delay(50000);
	ApplyTargetVDD();
	#endif
}

 /*============================================================================
 fVerifySetup()
 Verify the block just written to. This can be done byte-by-byte before the
 protection bits are set.
 Returns:
     0 if successful
     BLOCK_ERROR if timed out on handshake to the device.
 ============================================================================
*/

signed char fVerifySetup(unsigned char bBankNumber, unsigned char bBlockNumber)
{
	SendVector(set_block_num, num_bits_set_block_num);

	  /* Set the drive here because SendByte() does not */
	SetSDATAStrong();
	SendByte(bBlockNumber, 8);
	SendByte(set_block_num_end, 3);

	SendVector(verify_setup, num_bits_my_verify_setup);
	fIsError = fDetectHiLoTransition();
	if (fIsError != 0)
		return VERIFY_ERROR;
	SendVector(wait_and_poll_end, num_bits_wait_and_poll_end);

	return PASS;
}

 /*============================================================================
 fReadByteLoop()
 Reads the data back from Target SRAM and compares it to expected data in
 Host SRAM
 Returns:
     0 if successful
     BLOCK_ERROR if timed out on handshake to the device.
 ============================================================================
*/

signed char fReadByteLoop(void)
{
	bTargetAddress = 0;
	bTargetDataPtr = 0;

	while (bTargetDataPtr < TargetDatabufLen) {
		/* Send Read Byte vector and then get a byte from Target */
		SendVector(read_byte_v, 5);
		/* Set the drive here because SendByte() does not */
		SetSDATAStrong();
		SendByte(bTargetAddress, 6);

		/* Run two SCLK cycles between writing and reading */
		RunClock(2);
		/* Set to HiZ so Target can drive SDATA */
		SetSDATAHiZ();
		bTargetDataIN = bReceiveByte();

		RunClock(1);
		/* Send the ReadByte Vector End */
		SendVector(read_byte_v + 1, 1);

		if (bTargetDataIN != abTargetDataOUT[bTargetDataPtr]) {
		#ifdef TX_ON
			TX8SW_PutCRLF();
			TX8SW_CPutString("bTargetDataIN : ");
			TX8SW_PutSHexByte(bTargetDataIN);
			TX8SW_CPutString(" abTargetDataOUT : ");
			TX8SW_PutSHexByte(abTargetDataOUT[bTargetDataPtr]);
		#endif
			return BLOCK_ERROR;
		}

		bTargetDataPtr++;
		bTargetAddress += 4;

	}

	return PASS;
}

 /*============================================================================
 fSecureTargetFlash()
 Before calling, load the array, abTargetDataOUT, with the desired security
 settings using LoadArrayWithSecurityData(StartAddress,Length,SecurityType).
 The can be called multiple times with different SecurityTypes as needed for
 particular Flash Blocks. Or set them all the same using the call below:
 LoadArrayWithSecurityData(0,SECURITY_BYTES_PER_BANK, 0);
 Returns:
     0 if successful
     SECURITY_ERROR if timed out on handshake to the device.
 ============================================================================
*/

signed char fSecureTargetFlash(void)
{
	unsigned char bTemp;

	bTargetAddress = 0x00;
	bTargetDataPtr = 0x00;

	SetSDATAStrong();
	while (bTargetDataPtr < SecurityBytesPerBank) {
		bTemp = abTargetDataOUT[bTargetDataPtr];
		SendByte(write_byte_start, 5);
		SendByte(bTargetAddress, 6);
		SendByte(bTemp, 8);
		SendByte(write_byte_end, 3);

		bTargetAddress += 4;
		bTargetDataPtr++;
	}

	SendVector(secure, num_bits_secure);
	fIsError = fDetectHiLoTransition();
	if (fIsError != 0)
		return SECURITY_ERROR;
	SendVector(wait_and_poll_end, num_bits_wait_and_poll_end);
	return PASS;
}

#endif  /*(PROJECT_REV_) */
/* end of file ISSP_Routines.c */
