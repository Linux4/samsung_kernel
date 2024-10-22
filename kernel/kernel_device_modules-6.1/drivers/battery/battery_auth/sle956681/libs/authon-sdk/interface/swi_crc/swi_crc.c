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
 * @file   swi_crc.c
 * @date   January, 2021
 * @brief  Implementation of SWI bus protocol
 */
#include "../../../authon-sdk/interface/interface.h"
#include "../../../authon-sdk/interface/register.h"
#include "../../../authon-sdk/platform/bitbang.h"
#include "../../../authon-sdk/platform/helper.h"
#include "../../authon_api.h"
#include "../../authon_config.h"
#include "../../authon_status.h"
#include "../swi_crc_bus_command.h"

extern uint16_t g_ulBaudPowerDownTime;
extern uint16_t g_PowerUpDelay;
extern uint8_t g_ResetDelay;
extern uint8_t g_WakeUpPulse;
extern uint16_t g_DevAddr;

#define CHECK_TRAINING_BITS_TIME                 (1)   /*Set '1' to check the data frame value against the Tau value calculated from Training bits */


uint8_t dev_found[CONFIG_SWI_MAX_COUNT][UID_BYTE_LEN];

/**
* @brief Send Output to SWI.
* @param ub_Code Absolute Register address.
* @param ub_Data byte data.
* @param b_WaitForInterrupt b_WaitForInterrupt .
* @param b_ImmediateInterrupt b_ImmediateInterrupt TRUE/FASE.
* @param bp_IrqDetected Pointer to IRQ status.
*/
void Swi_SendRawWord( uint32_t uw_SwiCMD, uint8_t b_WaitForInterrupt, uint8_t b_ImmediateInterrupt, uint8_t * bp_IrqDetected ){
  Int_Status bInterruptOccured = NO_INTERRUPT;
  uint32_t g_ulBaudLow = Get_SWILowBaud();
  uint32_t g_ulBaudHigh = Get_SWIHighBaud();
  uint32_t g_ulBaudStop = Get_SWIStopBaud();
  unsigned long flags;

//todo System porting required: implements platform interrupt disable for bit-banging operation.
   local_irq_save(flags);
   preempt_disable();

  GPIO_write(GPIO_LEVEL_HIGH); /*!< Send a STOP signal first to have time to receive either IRQ or data! */
  GPIO_set_dir(GPIO_DIRECTION_OUT, 1);
  udelay(g_ulBaudStop); /*!< send a STOP command first */

  GPIO_write(GPIO_LEVEL_LOW); /*!< send BCF */
  (uw_SwiCMD & 0x10000) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_HIGH); /*!< send _BCF */
  (uw_SwiCMD & 0x08000) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_LOW); /*!< send bit9 */
  (uw_SwiCMD & 0x04000) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_HIGH); /*!< send bit8 */
  (uw_SwiCMD & 0x02000) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_LOW); /*!< send bit7 */
  (uw_SwiCMD & 0x01000) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_HIGH); /*!< send bit6 */
  (uw_SwiCMD & 0x00800) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_LOW); /*!< send bit5 */
  (uw_SwiCMD & 0x00400) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_HIGH); /*!< send bit4 */
  (uw_SwiCMD & 0x00200) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_LOW); /*!< send Parity bit3 */
  (uw_SwiCMD & 0x00100) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_HIGH); /*!< send bit3 */
  (uw_SwiCMD & 0x00080) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_LOW); /*!< send bit2 */
  (uw_SwiCMD & 0x00040) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_HIGH); /*!< send bit1 */
  (uw_SwiCMD & 0x00020) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_LOW); /*!< send Parity bit2 */
  (uw_SwiCMD & 0x00010) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_HIGH); /*!< send bit0 */
  (uw_SwiCMD & 0x00008) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_LOW); /*!< send Parity bit1 */
  (uw_SwiCMD & 0x00004) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_HIGH); /*!< send Parity bit0 */
  (uw_SwiCMD & 0x00002) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  GPIO_write(GPIO_LEVEL_LOW); /*!< send inversion bit */
  (uw_SwiCMD & 0x00001) ? udelay(g_ulBaudHigh) : udelay(g_ulBaudLow);

  
  GPIO_write(GPIO_LEVEL_HIGH); /*!< send STOP */
  udelay(g_ulBaudStop);

  GPIO_set_dir(GPIO_DIRECTION_IN, 1); /*!< change GPIO as input */

  /* wait for interrupt */
  if (b_WaitForInterrupt == TRUE) {
      Swi_WaitForIrq(&bInterruptOccured, b_ImmediateInterrupt);
  }

  /* Update IRQ status */
  if ((b_WaitForInterrupt == TRUE) || (b_ImmediateInterrupt == TRUE)) {
      *bp_IrqDetected = bInterruptOccured;
  }


//todo System porting required: implements platform interrupt enable after the critical section of bit-banging operation.	
  local_irq_restore(flags);
  preempt_enable();

	// if it is read command, don't wait for stop
	//udelay(g_ulBaudStop);

}

/**
* @brief Send SWI command without interrupt response
* @param ub_Code command to be send
* @param ub_Data data to be send
*/
void Swi_SendRawWordNoIrq(uint8_t ub_Code, uint8_t ub_Data ){
	uint32_t swi_cmd;
	uint8_t  cmd_array[3];
	uint16_t crc_16;

	swi_cmd = (ub_Code << 8) + ub_Data;
	// send command
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// compute CRC
	cmd_array[0] = swi_cmd & 0xff;
	cmd_array[1] = (swi_cmd >> 8) & 0xff;
	cmd_array[2] = (swi_cmd >> 16) & 0x1;
	crc_16 = crc16_gen(cmd_array, 3);

	// send CRC0
	swi_cmd = (SWI_WD << 8) + (crc_16 & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );

	// send CRC1
	swi_cmd = (SWI_WD << 8) + ((crc_16 >> 8) & 0xff);
	swi_cmd = Swi_ComputeParity(swi_cmd);
	swi_cmd = Swi_AddInvertFlag(swi_cmd);
	Swi_SendRawWord( swi_cmd, FALSE, FALSE, NULL );
	
}


/**
* @brief count number of 1s from the input data
* @param uw_data SWI command code + data
* @param ub_CntBits total bits need to be counted
*/
static uint8_t CountOnes(uint16_t uw_data, uint8_t ub_CntBits){
	uint8_t counter;
	uint8_t i;

	counter = 0;
	for(i = 0;i < ub_CntBits;i ++){
		if(((uw_data >> i) & 0x0001) == 0x01)counter++;
	}
	return counter;
}

/**
* @brief Receive data from SWI
* @param ub_data SWI command code + data
* @param rd_len total bits need to be counted
*/
uint16_t Swi_ReceiveData( uint8_t * ub_data , uint16_t rd_len){
	uint16_t ret;
	uint16_t i;
	uint16_t cmd_parity;
	uint16_t P0_MASK = 0x5555;
	uint16_t P1_MASK = 0x6666;
	uint16_t P2_MASK = 0x7878;
	uint16_t P3_MASK = 0x7f80;
	uint32_t uw_recData;
	uint8_t ub_crcByte[256*3];
	uint16_t ptr;
	uint16_t crc_16;
	
	
	/* read out data */
	ptr = 0;
	for(i=0; i< (rd_len); i++){
		ret = Swi_ReceiveRawWord( &uw_recData);
		if(ret != INF_SWI_SUCCESS){
			return ret;
		}

		// record in crc array
		ub_crcByte[ptr++] = uw_recData & 0xff;
		ub_crcByte[ptr++] = (uw_recData >> 8) & 0xff;
		ub_crcByte[ptr++] = (uw_recData >>16) & 0xff;

		// check on inversion bit
		if((uw_recData & 0x01) == 0x01){
			uw_recData ^= 0x7fff;
		}
		uw_recData = uw_recData >> 1;

		// -----------------------
		// check Parity
		// -----------------------
		cmd_parity = 0;
		// P0
		cmd_parity = CountOnes(uw_recData & P0_MASK,16) & 0x01;
		// P1
		cmd_parity |= (CountOnes(uw_recData & P1_MASK,16) & 0x01)<<1;
		// P2
		cmd_parity |= (CountOnes(uw_recData & P2_MASK,16) & 0x01)<<2;
		// P3
		cmd_parity |= (CountOnes(uw_recData & P3_MASK,16) & 0x01)<<3;
		
		if(cmd_parity != 0)return INF_SWI_E_RD_PARITY;
		uw_recData = ((uw_recData >> 2)&0x01) |((uw_recData >> 3)&0x0e)|((uw_recData >> 4)&0xfff0);
		
		// check on ACK 
		if((uw_recData & 0x0200) != 0x0200){
			*ub_data = uw_recData & 0xff;
			return INF_SWI_E_RD_ACK;
		}

		*(ub_data+i) = uw_recData & 0xff;
	}


	// receive CRC
	crc_16 = 0;
	for(i=0; i< 2; i++){
		ret = Swi_ReceiveRawWord( &uw_recData);
		if(ret != INF_SWI_SUCCESS){
			return ret;
		}
		// check on inversion bit
		if((uw_recData & 0x01) == 0x01){
			uw_recData ^= 0x7fff;
		}
		uw_recData = uw_recData >> 1;

		// -----------------------
		// check Parity
		// -----------------------
		cmd_parity = 0;
		// P0
		cmd_parity = CountOnes(uw_recData & P0_MASK,16) & 0x01;
		// P1
		cmd_parity |= (CountOnes(uw_recData & P1_MASK,16) & 0x01)<<1;
		// P2
		cmd_parity |= (CountOnes(uw_recData & P2_MASK,16) & 0x01)<<2;
		// P3
		cmd_parity |= (CountOnes(uw_recData & P3_MASK,16) & 0x01)<<3;
		
		if(cmd_parity != 0)return INF_SWI_E_RD_PARITY;
		uw_recData = ((uw_recData >> 2)&0x01) |((uw_recData >> 3)&0x0e)|((uw_recData >> 4)&0xfff0);
		crc_16 |= (uw_recData & 0xff) << (i*8);
	}
	
	if(crc_16 != crc16_gen(ub_crcByte,ptr)){
		return INF_SWI_E_RD_CRC;
	}

	return INF_SWI_SUCCESS;

}

/**
* @brief Perform time distance measurement using ktime (Linux only).
* @param tau Tau derived from training bits
* @param distance measured timing distance.
* @param value decoded binary value on when the returned value is TRUE, else ignore.
*/
uint8_t time_distance_code(uint64_t tau, uint64_t distance, uint8_t *value)
{
	uint64_t half_tau = (tau>>1);
	//pr_info("tau=%llu distance=%llu\n", tau, distance);

	if((distance >= half_tau) && (distance <= (tau + half_tau))){
		*value=0;		
	} else if ( (distance >= ((2*tau)+half_tau)) && (distance <= ((3*tau) + half_tau))){
		*value=1;		
	}else{
		return FALSE;
	}

	return TRUE;
}

/**
* @brief Perform GPIO sampling to form SWI raw data from device.
* @param up_Byte Data sampled from GPIO.
*/
uint16_t Swi_ReceiveRawWord( uint32_t * up_wData ){
	uint16_t bPreviousSwiState;
	uint8_t ubIndex = SWI_FRAME_BIT_SIZE-1;
	uint8_t ubBitsToCapture;

	uint32_t ulTimeOut = Get_SWITimeoutBaud();
	uint32_t ulTimes[SWI_FRAME_BIT_SIZE];    
	uint32_t ulMaxTime = 0u;
	uint32_t ulMinTime = ~ulMaxTime;
	uint64_t ulCount;
	uint64_t ulThreshold;
	uint8_t i;
	unsigned long flags;

	uint32_t ktime_time[SWI_FRAME_BIT_SIZE];
	uint64_t ktime_firstcount;
	uint64_t ktime_count;
	uint32_t ktime_decode_data;
	uint64_t ktime_Tau;
	
	uint8_t logic_value[1];

#if (CHECK_TRAINING_BITS_TIME==1)
    uint32_t g_ulBaudLow = Get_SWILowBaud();
    uint64_t Training0=0;
    uint64_t Training1=0;

    uint64_t minTau1=(6 * g_ulBaudLow)/10;   //0.6*Tau
    uint64_t maxTau1=(14 * g_ulBaudLow)/10;  //1.4*Tau
    uint64_t minTau3= (26 * g_ulBaudLow)/10; //2.6*Tau
    uint64_t maxTau3= (34 * g_ulBaudLow)/10; //3.4*Tau

    uint32_t Training0us;
    uint32_t Training1us;
#endif

	// disable interrupt since the bit transmission timing is crucial.  
  local_irq_save(flags);
  preempt_disable();

	while (GPIO_read() && ulTimeOut) {
		ulTimeOut--;
	}

	if (ulTimeOut == 0u) {
		//Re-enable interrupt
		local_irq_restore(flags);
		preempt_enable();
		return INF_SWI_E_TIMEOUT;
	}

	//get port state
	bPreviousSwiState = GPIO_read(); /*!< get port state */
	ktime_firstcount = ktime_get_ns(); // First falling edge

	/*!< measure time using CPU tick and ktime*/
	for (ubBitsToCapture = SWI_FRAME_BIT_SIZE; ubBitsToCapture != 0u; ubBitsToCapture--) {
		ulCount = 0u;
		ulTimeOut = Get_SWITimeoutBaud();
		while ((GPIO_read() == bPreviousSwiState) && ulTimeOut) {
			ulCount++;
			ulTimeOut--;
		}
		ulTimes[ubIndex] = ulCount;
		ktime_time[ubIndex] = ktime_get_ns(); //next falling edge
		ubIndex--;

		bPreviousSwiState = GPIO_read();
	}

	//Re-enable interrupt
	local_irq_restore(flags);
	preempt_enable();

	// evaluate results of ktime
	for (ubIndex = 0; ubIndex <= 16; ubIndex++) {
		if (ktime_time[16 - ubIndex] > ktime_firstcount){
			ktime_firstcount = ktime_time[16 - ubIndex] - ktime_firstcount;
		}
		else{//this part is unlikely to happen since it take very long time to overflow 64-bit nanosec counter
			ktime_firstcount = ktime_time[16 - ubIndex] + (0xFFFFFFFFFFFFFFFF - ktime_firstcount);
		}

		ktime_count = ktime_firstcount;
		ktime_firstcount = ktime_time[16 - ubIndex];
		ktime_time[16 - ubIndex] = ktime_count;
	}
#if 0
    pr_info("%x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x", ktime_time[0],ktime_time[1],ktime_time[2],ktime_time[3],
                                                                ktime_time[4],ktime_time[5],ktime_time[6],ktime_time[7],
                                                                ktime_time[8],ktime_time[9],ktime_time[10],ktime_time[11],
                                                                ktime_time[12],ktime_time[13],ktime_time[14],ktime_time[15],
                                                                ktime_time[16] );
#endif

	/*!< evaluate detected results of CPU tick */
	for (ubIndex = (SWI_FRAME_BIT_SIZE-1); ubIndex != 0u; ubIndex--) {
		ulCount = ulTimes[ubIndex];
		if (ulCount < ulMinTime) {
			ulMinTime = ulCount;
		} else if (ulCount > ulMaxTime) {
			ulMaxTime = ulCount;
		} else {
			/*  no change required */
		}
	}

	/* calculate threshold using CPU tick */
	ulThreshold = ((ulMaxTime - ulMinTime) >> 1u);
	ulThreshold += ulMinTime;

	*up_wData = 0;
	for(i = 0;i < SWI_FRAME_BIT_SIZE;i++  ){
		*up_wData |=  ((ulTimes[i] > ulThreshold) ? 1u : 0u) << i;
	}
	//pr_info("CPU tick data = 0x%x\n", *up_wData);

	/* calculate Tau using ktime*/
	ktime_Tau = ((ktime_time[16] + ktime_time[15]) >> 2u);  //calculate Tau from training bits

#if (CHECK_TRAINING_BITS_TIME==1)
  Training0 = ktime_time[16];
  Training1 = ktime_time[15];
  //pr_info("ktime Training0 = %llu Training1 = %llu\n", Training0, Training1);
  
  //Check Training0 bit to fall between min and max of receiver Tau rate.
  //0.6*Tau(min 1 Tau)
  //1.4*Tau(max 1 Tau)
  //2.6*Tau(min 3 Tau)
  //3.4*Tau(max 3 Tau)
  
  if(Training0>Training1){
    Training0us = Training0>>10;
    if((Training0us < minTau3) || (Training0us > maxTau3)){      
      printk("Invalid training0= %d(%llu) minTau3=%llu maxTau3=%llu\n", Training0us,Training0, minTau3, maxTau3);      
      return INF_SWI_E_TRAINING; //Invalid training bit, let fail it
    }
  }else{
    Training0us = Training0>>10;
    if((Training0us < minTau1) || (Training0us > maxTau1)){
      printk("Invalid training0= %d(%llu) minTau1=%llu maxTau1=%llu\n", Training0us,Training0, minTau1, maxTau1);      
      return INF_SWI_E_TRAINING; //Invalid training bit, let fail it
    }
  }

  //Check Training1 bit to fall between min and max of receiver Tau rate.
  if(Training1>Training0){
    Training1us = Training1>>10;
    if((Training1us < minTau3) || (Training1us > maxTau3)){      
      printk("Invalid training1= %d(%llu) minTau3=%llu maxTau3=%llu\n", Training1us, Training1, minTau3, maxTau3);      
      return INF_SWI_E_TRAINING; //Invalid training bit, let fail it
    }
  }else{
    Training1us = Training1>>10;
    if((Training1us < minTau1) || (Training1us > maxTau1)){
      printk("Invalid training1= %d(%llu) minTau1=%llu maxTau1=%llu\n", Training1us, Training1, minTau1, maxTau1);      
      return INF_SWI_E_TRAINING; //Invalid training bit, let fail it
    }
  }
#endif
	
	ktime_decode_data=0x00;

	for(i=0; i<17; i++){		
		if(time_distance_code(ktime_Tau, ktime_time[i], logic_value) == TRUE){
			ktime_decode_data |= (logic_value[0] << (i));
		}else{			
			return INF_SWI_E_TRAINING; //Invalid training bit, let fail it
		}
	}	
	//pr_info("ktime_decode_data = 0x%x\n", ktime_decode_data);

	if(ktime_decode_data != (*up_wData)){
		pr_debug("mismatched CPU tick:0x%x Ktime:0x%x\n", *up_wData, ktime_decode_data );
		*up_wData = ktime_decode_data; //if the data is mismatched use the ktime derieved data 
	}

	return INF_SWI_SUCCESS;
}


/**
* @brief Compute parity bits for SWI command, return command with parity
* @param ub_Code SWI command code
* @param ub_Data SWI command data
*/
uint16_t Swi_ComputeParity(uint16_t uw_SwiCmd){
	uint16_t cmd_parity;
	uint16_t P0_MASK = 0x055b;
	uint16_t P1_MASK = 0x066d;
	uint16_t P2_MASK = 0x078e;
	uint16_t P3_MASK = 0x07f0;

	
	cmd_parity = 0;
	// P0
	cmd_parity = CountOnes(uw_SwiCmd & P0_MASK, 16) & 0x01;
	// P1
	cmd_parity |= (CountOnes(uw_SwiCmd & P1_MASK, 16) & 0x01) << 1;
	// P2
	cmd_parity |= (CountOnes(uw_SwiCmd & P2_MASK, 16) & 0x01) << 3;
	// P3
	cmd_parity |= (CountOnes(uw_SwiCmd & P3_MASK, 16) & 0x01) << 7;


	// add in command
	cmd_parity |= ((uw_SwiCmd & 0xfff0) << 4) | ((uw_SwiCmd & 0x0e) << 3) | ((uw_SwiCmd & 0x01) << 2);

	return cmd_parity;
}

/**
* @brief Compute invert flag and add into SWI command
* @param uw_swi_cmdWParity SWI command with parity bits added
*/
uint32_t Swi_AddInvertFlag(uint16_t uw_swi_cmdWParity){
	uint32_t cmd_17bits;
	
	cmd_17bits = uw_swi_cmdWParity << 1;

	if(CountOnes(uw_swi_cmdWParity,14) >= 8){
		cmd_17bits |= 1;
		cmd_17bits ^= 0x7ffe;
	}

	return cmd_17bits;
}

/**
* @brief host Send SWI interrupt pulse to device if no interrupt response from device for the command that has interrupt response
*/
void Swi_AbortIrq(void) {
    uint32_t g_ulBaudLow = Get_SWILowBaud();

    GPIO_set_dir(GPIO_DIRECTION_OUT, 0);
    udelay(g_ulBaudLow); /*!< delay for 1 tau */
    GPIO_write(GPIO_LEVEL_HIGH);
    GPIO_set_dir(GPIO_DIRECTION_IN,1);
}

/**
* @brief Wait for SWI interrupt from device
* @param bp_IrqDetected Pointer to IRQ status, Expects IRQ immediated? :TRUE/FASE.
* @param b_Immediate IRQ Status.
*/
void Swi_WaitForIrq(Int_Status *bp_IrqDetected, uint8_t b_Immediate) {
    Int_Status bResult = NO_INTERRUPT;
    volatile uint32_t ulTimeOut;
    uint32_t g_ulBaudLow = Get_SWILowBaud();
    uint32_t g_ulBaudStop = Get_SWIStopBaud();
    uint32_t g_ulResponseTimeOut = Get_SWITimeoutBaud();
    uint32_t g_ulResponseLongRetry = Get_EccRetryCount();

    if (b_Immediate) {
        ulTimeOut = g_ulResponseTimeOut;
    } else {
        ulTimeOut = g_ulResponseLongRetry;
    }

    *bp_IrqDetected = NO_INTERRUPT;

    while (ulTimeOut) {
        if (!(GPIO_read())) {
            bResult = GOT_INTERRUPT;
            *bp_IrqDetected = GOT_INTERRUPT;
            break;
        }
        ulTimeOut--;
    }

    udelay(g_ulBaudLow);
    udelay(g_ulBaudStop);

    if (bResult==NO_INTERRUPT) {
        Swi_AbortIrq();
    }

    return;
}

/**
* @brief activate device by device address
* @param uw_DeviceAddress SWI device address
*/
void Swi_SelectByAddress(uint16_t uw_DeviceAddress){
	Send_EDA(HiByte(uw_DeviceAddress));
	Send_SDA(LoByte(uw_DeviceAddress));
}

/**
* @brief power up device by one SWI pulse after PDWN command
*/
void Swi_PowerUp(void) {
  uint32_t g_ulBaudPowerUpTime = Get_PowerupTime();
  GPIO_set_dir(GPIO_DIRECTION_OUT, 1);
  udelay(g_ulBaudPowerUpTime);
}

/**
* @brief power down device by pull SWI line low, after SWI pull low power down, 
* need to call Swi_PowerUp_HardRst() to power up device.
*/
void Swi_PowerDown_HardRst(void){

  uint32_t g_ulBaudPowerUpTime = Get_PowerupTime();
  uint32_t WakeupDelay = Get_WakeupTime();
  uint32_t WakeupLow = Get_WakeupLowTime();
  GPIO_set_dir(GPIO_DIRECTION_OUT, 0);
  udelay(WakeupDelay+WakeupLow);
  GPIO_write(GPIO_LEVEL_HIGH);
  udelay(g_ulBaudPowerUpTime);
}

/**
* @brief power down device by pull SWI line low
*/
void Swi_PowerDown(void){
  GPIO_set_dir(GPIO_DIRECTION_OUT, 0);
  GPIO_write(GPIO_LEVEL_LOW);
}

/**
* @brief power up device by pull SWI line high
*/
void Swi_PowerUp_HardRst(void){
  uint32_t PowerDownTime = Get_PowerdownTime();
  uint32_t PowerUpTime = Get_PowerupTime();
  GPIO_set_dir(GPIO_DIRECTION_OUT, 0);
  udelay(PowerDownTime);
  GPIO_write(GPIO_LEVEL_HIGH);
  udelay(PowerUpTime);
}

#define UID_BYTE_LENGTH  12
#define UID_BIT_SIZE  96

#if (ENABLE_SWI_SEARCH ==1)
uint8_t ub_Stack[UID_BIT_SIZE];
uint8_t ub_StackPointer = 0;
/**
* @brief Get the probe status of the given UID bit position.
* @param ub_BitInfo 
*             bit 0: 0: DIP needed	; 1: DIP not needed
*             bit 1: 0: DIE1 not needed;  1: DIE1 needed
*             bit 2: 0: DIE0 not needed;  1: DIE0 needed
*/
static uint8_t b_UidSearch_GetDipDoneBit(uint8_t ub_BitInfo) {
  if (ub_BitInfo & 0x01) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/**
* @brief Update the probe status(Probe done or not) of the given UID bit position.
* @param ub_BitInfo 
*             bit 0: 0: DIP needed	; 1: DIP not needed
*             bit 1: 0: DIE1 not needed;  1: DIE1 needed
*             bit 2: 0: DIE0 not needed;  1: DIE0 needed
*/
static void b_UidSearch_SetDipDoneBit(uint8_t *ub_BitInfo, uint16_t b_Bit) {
  if (b_Bit) {
    *ub_BitInfo = (*ub_BitInfo) | 0x01; /*!< set */
  } else {
    *ub_BitInfo = (*ub_BitInfo) & 0xfe; /*!< clear */
  }
}

/**
* @brief Get the probe status(DIE0 or not) of the given UID bit position. 
*        <BR>Return TRUE if DIE0(bit2) is set
* @param ub_BitInfo 
*             bit 0: 0: DIP needed	; 1: DIP not needed
*             bit 1: 0: DIE1 not needed;  1: DIE1 needed
*             bit 2: 0: DIE0 not needed;  1: DIE0 needed
*/
static uint8_t b_UidSearch_GetDIE0Info(uint8_t ub_BitInfo) {
  if (ub_BitInfo & 0x04) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/**
* @brief Update the probe status(DIE0 or not) of the given UID bit position.
* @param ub_BitInfo 
*             bit 0: 0: DIP needed	; 1: DIP not needed
*             bit 1: 0: DIE1 not needed;  1: DIE1 needed
*             bit 2: 0: DIE0 not needed;  1: DIE0 needed
*/
static void b_UidSearch_SetDIE0Info(uint8_t *ub_BitInfo, uint16_t b_Data) {
  if (b_Data) {
    *ub_BitInfo = (*ub_BitInfo) | 0x04; /*!< set */
  } else {
    *ub_BitInfo = (*ub_BitInfo) & 0xfb; /*!< clear */
  }
}

/**
* @brief Get the probe status(DIE1 or not) of the given UID bit position. 
*        <BR>Return TRUE if DIE1(bit1) is set
* @param ub_BitInfo 
*             bit 0: 0: DIP needed	; 1: DIP not needed
*             bit 1: 0: DIE1 not needed;  1: DIE1 needed
*             bit 2: 0: DIE0 not needed;  1: DIE0 needed
*/
static uint8_t b_UidSearch_GetDIE1Info(uint8_t ub_BitInfo) {
  if (ub_BitInfo & 0x02) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/**
* @brief Update the probe status(DIE1 or not) of the given UID bit position.
* @param ub_BitInfo 
*             bit 0: 0: DIP needed	; 1: DIP not needed
*             bit 1: 0: DIE1 not needed;  1: DIE1 needed
*             bit 2: 0: DIE0 not needed;  1: DIE0 needed
*/
static void b_UidSearch_SetDIE1Info(uint8_t *ub_BitInfo) {
  *ub_BitInfo = (*ub_BitInfo) | 0x02;
}

/**
* @brief Pops the stack to retrieve the last branch location.
* @param ub_Data: read out last data in ub_Stack.
*/
static uint16_t b_pop(uint8_t *ub_Data) {
  if (ub_StackPointer == 0) {
    return APP_SID_E_STACK_EMPTY;
  }

  *ub_Data = ub_Stack[--ub_StackPointer];
  return APP_SID_SUCCESS;
}

/**
* @brief Push the branch location data to the stack.
* @param ub_Data: stores the branch location to the stack.
*/
static uint16_t b_push(uint8_t ub_Data) {
  if (ub_StackPointer == UID_BIT_SIZE) {
    return APP_SID_E_STACK_FULL;
  }

  ub_Stack[ub_StackPointer++] = ub_Data;
  return APP_SID_SUCCESS;
}

/**
* @brief Returns the current stack pointer.
*/
static uint8_t ub_SizeOfStack() {
  return ub_StackPointer;
}

/**
* @brief Find UID of the device connected on the SWI. At the end of the search, the device is automatically selected.
*        <BR>NOTE: this function only supports one attached device.
* @param DeviceUID Return device UID.
*/
static uint16_t Swi_BitSearchUID(uint8_t *DeviceUID) {
	uint8_t bFound_0 = 0u;
	uint8_t bFound_1 = 0u;
	uint8_t ubBitIndex = 0u;
	uint8_t UID_BYTE_INDEX = 0u;
	uint8_t ub_BitsToSearch = UID_BIT_SIZE;
	uint8_t *UID = (uint8_t *)DeviceUID;

	Send_DISS();

	for (; ub_BitsToSearch != 0u; ub_BitsToSearch--) {
		UID_BYTE_INDEX = ubBitIndex >> 3u;
		UID[UID_BYTE_INDEX] = (uint8_t)(UID[UID_BYTE_INDEX] << 1u);

		Send_DIP0(&bFound_0);
		Send_DIP1(&bFound_1);

		if ((bFound_1 == FALSE) && (bFound_0 == TRUE)) { /*!< current bit is 0 */
			UID[UID_BYTE_INDEX] &= 0xFEu;
			Send_DIE0();
		} else if ((bFound_1 == TRUE) && (bFound_0 == FALSE)) { /*!< current bit is 1 */
			UID[UID_BYTE_INDEX] |= 0x01u;
			Send_DIE1();
		} else { /*!< current bit is neither 0 nor 1 */
			Swi_AbortIrq();
			return APP_SID_E_NO_DEV;
		}
    ubBitIndex++;
  }

  return APP_SID_SUCCESS;
}
/**
* @brief Search for UIDs on the SWI where the UID will be returned in increasing order.
*    <BR>The device will be assigned with a temporary device address sequentially from 1 to N. 1 is the smallest UID found.
*    <BR>Note: The device address is loaded into the register and not written into the NVM. 
*    <BR>Therefore, upon power cycle, the device will get the value from the NVM device address again.
* @param pst_DetectedPuid Pointer to the UID table structure.
* @param ubp_DevCnt Pointer to the number of devices found. If NULL, expects only 1 device on the SWI bus.
*/
uint16_t Swi_SearchUID96(uint8_t *pst_DetectedPuid, uint16_t *ubp_DevCnt) {
  uint8_t UID[UID_BYTE_LENGTH];
  uint8_t ubBitInfo[UID_BIT_SIZE];
  uint8_t ubCurrentIdPtr, ubLastIdPtr = 0;
  uint16_t ubSlaveCnt = 0;
  uint8_t bFound_0;
  uint8_t bFound_1;
  uint8_t ubBitCnt = UID_BIT_SIZE;
  uint8_t ubByteIndex;
  uint8_t bSearchDone = FALSE;
  uint8_t i;
  uint8_t ubpBytes[UID_BYTE_LENGTH];
  uint8_t *ptr = (uint8_t *)pst_DetectedPuid;

  //uint32_t SwiTimeOut = Get_SWITimeoutBaud();
  //uint32_t EccTimeOut = Get_EccTimeout();

  if (ubp_DevCnt == NULL) { /*!< Expects only 1 device on SWI bus */
    return Swi_BitSearchUID(pst_DetectedPuid);
  }

  ub_StackPointer = 0; /*!< clears the stack pointer */
  memset(ubBitInfo, 0, ubBitCnt);

  do {
    if (ubSlaveCnt > 0) {
      //Swi_SendRawWordNoIrq(SWI_BC, SWI_DIE0);
	  Send_DIE0();
    }

    //Swi_SendRawWordNoIrq(SWI_BC, SWI_DISS); /*!< start ID search */
	Send_DISS();
    ubCurrentIdPtr = UID_BIT_SIZE;
    ubBitCnt = UID_BIT_SIZE;

    memset(ubpBytes, 0, sizeof(ubpBytes));

    for (; ubBitCnt > 0; ubBitCnt--) {
      ubCurrentIdPtr--;
      ubByteIndex = (uint8_t)(11 - (ubCurrentIdPtr >> 3u));
      ubpBytes[ubByteIndex] = (uint8_t)(ubpBytes[ubByteIndex] << 1u); /*!< init LSB to 1 */

      /*!< Check UID bit position probe status, if false -> proceed to probe */
      if (b_UidSearch_GetDipDoneBit(ubBitInfo[ubCurrentIdPtr]) == FALSE) /*!< need to do DIP */
      {
        //Swi_SendRawWordNoIrq(SWI_BC, SWI_DIP0); /*!< is current bit 0? */
		Send_DIP0(&bFound_0);
        //Swi_WaitForIrq(&bFound_0, TRUE, SwiTimeOut, EccTimeOut );
        //udelay(SwiTimeOut);

		Send_DIP1(&bFound_1);

        //Swi_SendRawWordNoIrq(SWI_BC, SWI_DIP1); /*!< is current bit 1? */
        //Swi_WaitForIrq(&bFound_1, TRUE, SwiTimeOut, EccTimeOut);
        //udelay(SwiTimeOut);

        /*!< current bit = 1 and current bit = 0 */
        if ((bFound_0 == TRUE) && (bFound_1 == TRUE)) {
          ubLastIdPtr = ubCurrentIdPtr;

          /*!< Stack stores the bit location of both DIP0_responded and DIP1_Responded */
          if (b_push(ubLastIdPtr) == APP_SID_E_STACK_FULL) {
            return APP_SID_E_STACK_FULL;
          }

          /*!< Transverse '0' Branch using DIE0 */
          ubpBytes[ubByteIndex] &= 0xFEu; /*!< Enter '0' branch first. */
          //Swi_SendRawWordNoIrq(SWI_BC, SWI_DIE0);
		  Send_DIE0();
          b_UidSearch_SetDIE0Info(&ubBitInfo[ubLastIdPtr], TRUE); /*!< Note that DIE0 is needed  */
          b_UidSearch_SetDIE1Info(&ubBitInfo[ubCurrentIdPtr]);    /*!< Note that DIE1 is needed */
        }                                                         /*!< current bit = 1 only*/
        else if ((bFound_0 == TRUE) && (bFound_1 == FALSE)) {
          /*!< Transverse '0' Branch using DIE0 */
          ubpBytes[ubByteIndex] &= 0xFEu; /*!< '0' is found, update UID value. Enter '0' branch. */
          //Swi_SendRawWordNoIrq(SWI_BC, SWI_DIE0);
		  Send_DIE0();
          b_UidSearch_SetDIE0Info(&ubBitInfo[ubCurrentIdPtr], TRUE); /*!< Note that DIE0 is needed */
        }                                                            /*!< current bit = 0 only */
        else if ((bFound_0 == FALSE) && (bFound_1 == TRUE))          /*!< DIE1 */
        {
          /*!< Transverse '1' Branch using DIE1 */
          ubpBytes[ubByteIndex] |= 0x01u; /*!< '1' is found, update UID value. Enter '1' branch. */
          //Swi_SendRawWordNoIrq(SWI_BC, SWI_DIE1);
		  Send_DIE1();
          b_UidSearch_SetDIE0Info(&ubBitInfo[ubCurrentIdPtr], FALSE); /*!< Note that DIE0 is not needed. */
          b_UidSearch_SetDIE1Info(&ubBitInfo[ubCurrentIdPtr]);        /*!< Note that DIE1 is needed. */
        } else {                                                      /*!< No Response  */
          return APP_SID_E_NO_DEV;
        }

        b_UidSearch_SetDipDoneBit(&ubBitInfo[ubCurrentIdPtr], TRUE);  //Note that current bit position is done.
                                                                      /*!< // DIPDone == TRUE //Case of DIP is done but branch occurred. */
      } else {
        if (ubCurrentIdPtr == ubLastIdPtr) {
          b_pop(&ubCurrentIdPtr); /*!< Get pointer position from stack */
        }

        /*!< If DIE0 is required? -> DIE0 */
        if (b_UidSearch_GetDIE0Info(ubBitInfo[ubCurrentIdPtr]) == TRUE) {
          ubpBytes[ubByteIndex] &= 0xFEu;
          //Swi_SendRawWordNoIrq(SWI_BC, SWI_DIE0);
		  Send_DIE0();
          /*!< If DIE1 is required? -> DIE1 */
        } else if (b_UidSearch_GetDIE1Info(ubBitInfo[ubCurrentIdPtr]) == TRUE) {
          ubpBytes[ubByteIndex] |= 0x01u;
          //Swi_SendRawWordNoIrq(SWI_BC, SWI_DIE1);		  
		  Send_DIE1();
        }
      }

      /*!< clear dip done from last bit index that has two positive response */
      if (ubCurrentIdPtr == 0) {
        /*!< No collision, so that's the end */
        if (ub_SizeOfStack() == 0) {
          bSearchDone = TRUE;
        } else { /*!< retrieve the location of last collision bit */

          b_pop(&ubLastIdPtr); /*!< refresh ubLastIdPtr  */
          b_push(ubLastIdPtr);

          /*!< clears all the done bits from LSB to collision location  */
          /*!< clears DIP done bit since last id pointer  */
          for (i = 0; i < ubLastIdPtr; i++) {
            b_UidSearch_SetDipDoneBit(&ubBitInfo[i], FALSE);  //Note all values unti last pointer position is NOT done
          }

          b_UidSearch_SetDIE0Info(&ubBitInfo[ubLastIdPtr], FALSE); /*!< clear  */
        }
      }  // if (ubCurrentIdPtr == 0)
    }    //for ( ; ubBitCnt > 0; ubBitCnt --)

    /*!< Copy UID to buffer  */
    memcpy(ptr, ubpBytes, UID_BYTE_LENGTH);

    ubSlaveCnt++;
    ptr += sizeof(UID);

  } while (bSearchDone == FALSE);

  *ubp_DevCnt = ubSlaveCnt;
  return APP_SID_SUCCESS;
}
#endif

/**
* @brief Select device by applying a given device UID. 
*        <BR>After UID selection a device address can be set for easier
* @param stp_DeviceToSelect Pointer to UID structure to hold the UID to use.
*/
uint16_t Swi_SelectByPuid(uint8_t *stp_DeviceToSelect) {
  uint8_t ubByteIndex;
  uint8_t ubBitIndex = 0;
  uint8_t ub_BitsToExecute = UID_BIT_SIZE;

  uint8_t bRefBit = 0;
  uint8_t *ubpRefBytes = (uint8_t *)stp_DeviceToSelect;

  if (ub_BitsToExecute > UID_BIT_SIZE){
    return APP_SID_E_SELECT;
  }

  Send_DIE0();
  Send_DISS();

  for (; ub_BitsToExecute != 0u; ub_BitsToExecute--) {
    ubByteIndex = ubBitIndex >> 3u;
    bRefBit = (uint8_t)((ubpRefBytes[ubByteIndex] & (1u << (7u - (ubBitIndex & 0x07u)))));

    if (bRefBit == 0u) {
	    Send_DIE0();
    } else {
	    Send_DIE1();
    }

    ubBitIndex++;
  }

  return APP_SID_SUCCESS;
}
